/**
 * cwc - a crossword compiler. Copyright 1999 Lars Christensen
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA. 
 **/

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <locale.h>
#include <iostream.h>
#include <sys/time.h>

#include <fstream.h>
#include <string>
#include <strstream>
#include <math.h>

#include <set>
#include <vector>
#include <list>

#include "timer.hh"
#include "symbol.hh"
#include "dict.hh"
#include "letterdict.hh"
#include "grid.hh"

#include "cwc.hh"

//////////////////////////////////////////////////////////////////////
// class walker

walker::walker(grid &thegrid) : current(0), g(thegrid) {
  limit = thegrid.getempty();
  inited = false;
}

cell &walker::currentcell() {
  if (!inited) throw error("walker not inited");
  return g.cellno(current);
}

void walker::backto(int c) {
  while (current != c)
    backward();
}

bool walker::current_oneof(int no[], int n) {
  for (int i=0;i<n;i++) {
    if (no[i] == current)
      return true;
  }
  return false;
}

void walker::backto_oneof(int no[], int n) {
  while (!current_oneof(no, n))
    backward();
}

void walker::backto_oneof(backtracker &bt) {
  backward(false); // dont save current
  while (!bt.stophere(current))
    backward(true); // save all we skip
}

void walker::forward() {
  if (inited) {
    cellno.push_back(current);
    do step_forward(); while (!g.cellno(current).isempty());
  } else {
    init();
    inited = true;
  }
}

void walker::backward(bool savepreferred) {
  if (!g.cellno(current).isoutside())
    g.cellno(current).clear(savepreferred);
  current = cellno.back();
  cellno.pop_back();
}

bool walker::moresteps() {
  return (cellno.size() + 1) < unsigned(limit);
}

void walker::findnext() {
  int ncells = g.numcells();
  for (int i = 0; i < ncells; i++)
    if (g.cellno(i).isempty()) {
      current = i;
      return;
    }
  throw error("No empty cells");
}

void walker::init() {
  findnext();
}

//////////////////////////////////////////////////////////////////////
// class prefix_walker

void prefix_walker::step_forward() {
  current++; // not correct now
}

//////////////////////////////////////////////////////////////////////
// class flood_walker

flood_walker::flood_walker(grid &g) : walker(g) {
}

void flood_walker::step_forward() {
  vector<int>::iterator i;
  for (i = cellno.begin(); i != cellno.end(); i++) {
    int cno = *i;

    int nwords = g.cellno(cno).numwords();
    for (int w = 0; w < nwords; w++) {
      cell &thecell = g.cellno(cno);
      wordblock &wb = thecell.getwordblock(w);
      int pos = thecell.getpos(w);
      int len = wb.length();

      if (pos > 0) {
	int cellbefore = wb.getcellno(pos-1);
	if (g.cellno(cellbefore).isempty()) {
	  current = cellbefore;
	  return;
	}
      }
      if (pos < len-1) {
	int cellafter = wb.getcellno(pos + 1);
	if (g.cellno(cellafter).isempty()) {
	  current = cellafter;
	  return;
	}
      }
    }
  }
  // at this point: no adjacent cells found
  findnext();
}

//////////////////////////////////////////////////////////////////////
// class backtracker

backtracker::backtracker(grid &thegrid) : g(thegrid) {
}

//////////////////////////////////////////////////////////////////////
// class naive_backtracker

// The naive backtracker simply backs up to the previously
// filled cell.

void naive_backtracker::backtrack(walker &w) {
  w.backward(false);
}

//////////////////////////////////////////////////////////////////////
// class smart_backtracker 
//
// the smart backtracker steps back to the previously filled cell
// that is within reach from the current cell

void smart_backtracker::backtrack(walker &w) {
  // search up
  int cpos = w.stepno();

  // initially, forget all bt points that are dependend on cells behind
  // current point.
  list<cpair>::iterator next;
  for (list<cpair>::iterator i = bt_points.begin();
       i != bt_points.end();
       i = next) {
    next = i; next++;
    if ((*i).first <= cpos) {
      if (setup.debuginfo) cout << "removing " << (*i).second << "(from " << (*i).first << ")" << endl;
      bt_points.erase(i);
    }
  }
  
  int cno = w.getcurrent();

  cell &c = g.cellno(cno);
  int nwords = c.numwords();
  for (int wno = 0; wno < nwords; wno++) {
    wordblock &wb = c.getwordblock(wno);
    int len = wb.length();

    int pos = c.getpos(wno);
    for (int p = 0; p < len; p++) {
      if ((p!=pos)&&(wb.getcell(p).isfilled()))
	bt_points.push_back(cpair(cpos, wb.getcellno(p)));
    }

  }

  if (setup.debuginfo) {
    cout << "BTSET:" << endl;
    for (list<cpair>::iterator i = bt_points.begin(); i != bt_points.end(); i++)
      cout << " (" << (*i).first << ',' << (*i).second << ")";
    cout << endl;
  }
  
  w.backto_oneof(*this);
}

bool smart_backtracker::stophere(int p) {
  for (list<cpair>::iterator i = bt_points.begin();
       i != bt_points.end();
       i++) {
    if ((*i).second == p) {
      bt_points.erase(i); // dont stop here again
      return true;
    }
  }
  return false;
}

//////////////////////////////////////////////////////////////////////
// compiler
//
// Cross word compiler logic

compiler::compiler(grid &thegrid, walker &thewalker,
		   backtracker &thebacktracker, dict &thedict)
  : g(thegrid), w(thewalker), bt(thebacktracker), d(thedict) {
  g.verbose = verbose = false;
  findall = false;
}

#define success true
#define failure false

// upon failure, walker is backed up to some cell
// the reclevel trying to compute this cell catches it
// and others will return.

timer dtimer;

bool compiler::compile_rest(double rejected) {
  int c = w.getcurrent();
  if (verbose)
    cout << "attempting to find solution for " << c << endl;
  symbolset ss = g(c).findpossible(d);
  int npossible = numones(ss);
  rejected += (numalpha-double(npossible)) * pow(numalpha, numcells - w.stepno());
  if (verbose)
    dumpset(ss);

  symbolset bit;
  // use preferred if any
  if (g(c).haspreferred()) {
    symbol s = g(c).getpreferred();
    symbolset ss2 = s.getsymbolset();
    if (ss2 & ss) {
      bit = ss2;
      ss &= ~bit; // remove bit from set
    }
    else
      bit = pickbit(ss);
  } else
    bit = pickbit(ss);
  for (; bit; bit=pickbit(ss)) {
    symbol s = symbol::symbolbit(bit);
    g(c).setsymbol(s);
    if (setup.showallsteps) {
      g.dump_simple(cout);
    } else if ((showsteps && dtimer.getmsecs() > 500)) {
      g.dump_simple(cout);
      dtimer.reset();
    }
    if (w.moresteps()) {
      w.forward();
      if (compile_rest(rejected) == success) return success;
      if (w.getcurrent() != c) return failure; // catch if ==
      // cout << "continue at " << c << endl;
      rejected += pow(numalpha, numcells - w.stepno());
    } else {
      this->rejected = rejected;
      return success;
    }
    g(c).setsymbol(symbol::empty);
  }
  if (w.stepno() > 1) {
    bt.backtrack(w);
    int cur = w.getcurrent();
    if (verbose) 
      cout << "return to " << cur << " from " << c << endl;
  }
  return failure;
}

void compiler::compile() {
  dtimer.reset(); dtimer.start();
  w.forward();
  numcells = g.numopen();
  numalpha = symbol::numalpha();
  compile_rest();
}

//////////////////////////////////////////////////////////////////////
// main

void dumpset(symbolset ss) {
  int i,n;
  cout << '{';
  for (i=1, n = 0; i; i<<=1, n++) {
    if (ss&i)
      cout << symbol::alphabet[n];
  }
  cout << '}' << endl;
}

void dumpsymbollist(symbol *s, int n) {
  for (int i=0;i<n;i++) {
    if (s[i] == symbol::empty)
      cout << '-';
    else
      cout << s[i];
  }
  cout << endl;
}

void trivial_random_init() {
  struct timeval tv;
  gettimeofday(&tv, 0);
  srand(tv.tv_usec);
}

void random_init(setup_s &s) {
  unsigned int q;
  if (!s.setseed) {
    int fd = open("/dev/random", O_RDONLY);
    if (fd == -1) {
      trivial_random_init();
      return;
    }
    if (read(fd, &q, 4) == -1)
      perror("read");
    close(fd);
  } else {
    q = s.seed;
  }
  cout << "random seed: " << q << endl;
  srand(q);
}

setup_s setup = {
  setup.ascii_format,
  setup.floodwalker,
  setup.letterdict,
  false,
  false,
  false,
  "/usr/dict/words",
  "",
  setup.noformat,
  false,
  false,
  0,
  false,
};

char usage[] =
"Usage: cwc [options]\n"
"\n"
"options:\n"
"   -d <filename>     use another dictionary file (default /usr/dict/words)\n"
"   -p <filename>     read grid pattern from file\n"
"   -w <walkertype>   Walking heuristics: prefix or flood\n"
"   -f <format>       output format, one of `simple' or `ascii'\n"
"   -i <indextype>    Choose dictionary index style. `btree' or `letter'\n"
"   -r seed           Set the random seed\n"
"   -v                Be verbose - prints algorithmic info\n"
"   -s                Print the grid filling regularly during compilation\n"
"   -S                Print the grid filling each step\n"
"   -b                Benchmark dictionaries\n"
"   -? -h             Display this help screen\n"
;

int parseparameters(int argc, char *argv[]) {
  int c;
  while (c=getopt(argc, argv, "d:p:vf:hsSw:i:br:g:?"), c != -1) {
    switch (c) {
    case 'g':
      setup.gridfile = optarg; 
      setup.gridformat = setup.generalgrid;
      break;
    case 'b': setup.benchdict = true; break;
    case 'd': setup.dictfile = optarg; break;
    case 'p':
      setup.gridfile = optarg;
      setup.gridformat = setup.squaregrid;
      break;
    case 'v': setup.verbose = true; break;
    case 'r': setup.setseed = true; setup.seed = atoi(optarg); break;
    case 'f': {
      string s(optarg);
      if (s=="simple")
	setup.output_format = setup.simple_format;
      else if (s == "ascii")
	setup.output_format = setup.ascii_format;
      else {
	puts("Invalid format specifier");
	return -1;
      }
    }
    break;
    case 'w': {
      string s(optarg);
      if (s=="prefix")
	setup.walkertype = setup.prefixwalker;
      else if (s=="flood")
	setup.walkertype = setup.floodwalker;
      else {
	puts("Invalid walker specifier");
	return -1;
      }
    }
    break;
    case 'i': {
      string s(optarg);
      if (s=="btree")
	setup.dictstyle = setup.btreedict;
      else if (s=="letter")
	setup.dictstyle = setup.letterdict;
      else {
	puts("Invalid dictionary index style");
	return -1;
      }
    }
    break;
    case 's': setup.showsteps = true; break;
    case 'S': setup.showallsteps = true; break;
    case '?':
    case 'h': printf(usage); return -1;
    }
  }
  return 0;
}

int main(int argc, char *argv[]) {
  if (parseparameters(argc, argv) == -1) exit(EXIT_FAILURE);
  random_init(setup);
  symbol::buildindex();

  if (setlocale(LC_CTYPE, "da_DK") == 0)
    cout << "Failed to set locale" << endl;

  try {

    if (setup.benchdict) {
      dodictbench();
      exit(EXIT_SUCCESS);
    }

    dict *d = 0;
    switch(setup.dictstyle) {
    case setup.btreedict:
      cout << "Using binary tree index" << endl;
      d = new btree_dict();
      break;
    case setup.letterdict:
      cout << "Using letter index" << endl;
      d = new letterdict();
      break;
    }
    d->load(setup.dictfile);

    grid g;
    if (setup.gridformat == setup.generalgrid)
      g.load(setup.gridfile);
    else if (setup.gridformat == setup.squaregrid)
      g.load_template(setup.gridfile);
    // g.dump_ggrid(cout);
    int nopen = g.numopen();
    double space = pow(symbol::numalpha(), nopen);
    cout << nopen << " cells to be filled. " << space << " possible fillings." << endl;
    walker *w;
    switch (setup.walkertype) {
    case setup.prefixwalker:
      w = new prefix_walker(g);
      puts("Using prefix walking heuristics");
      break;
    case setup.floodwalker:
      w = new flood_walker(g);
      puts("Using flood walking heuristics");
      break;
    default:
      puts("Internal error");
      exit(EXIT_FAILURE);
    }

    cout << "Degree of interlock: " << g.interlockdegree()*100 << "%" << endl;
    double depdeg1 = g.depencydegree(1);
    double depdeg2 = g.depencydegree(2);
    cout << "Degree of depency: " << depdeg1 << '(' << (depdeg1*100.0/nopen) << "%)" << endl;
    cout << "Degree of 2nd level depency: " << depdeg2 << '(' << (depdeg2*100.0/nopen) << "%)" << endl;

    smart_backtracker bt(g);
    
    compiler c(g, *w, bt, *d);
    c.verbose = setup.verbose;
    c.showsteps = setup.showsteps;
    timer t; t.start();
    c.compile();
    t.stop();
    
    g.dump(cout, setup.output_format);
    cout << "Attempt average: " << g.attemptaverage() << endl;
    cout << "Compilation time: " << t.getmsecs() << " msecs" << endl;
    double searched = c.getrejected();
    cout << searched << " solutions searched. " << (searched*100/space) << "% of search space." << endl;
  } catch (error e) {
    cout << e.what() << endl;
    exit(EXIT_FAILURE);
  }
}

void dodictbench() {
  int t1, t2;

  btree_dict bd;
  bd.load("/usr/dict/words");
  t1 = dictbench(bd);

  letterdict d2; 
  d2.load("/usr/dict/words");
  t2 = dictbench(d2);
  
  cout << "btree=" << t1 << ", letter=" << t2 << endl;
}

int dictbench(dict &d) {
  symbol s[MAXWORDLEN];
  int o[MAXWORDLEN];

  timer t; t.start();

  for (int r=0; r<100; r++) {
    for (int i=0;i<MAXWORDLEN;i++)
      o[i] = i;

    for (int len = 1; len < MAXWORDLEN; len++) {
      for (int i=0;i<len;i++) {
	s[i] = symbol::empty;
	// shuffle
	int q = rand()%len;
	int t = o[q]; o[q] = o[i]; o[i] = t;
      }
      int n = 0;
      int pos = o[n++];
      while (n <= len) {
	symbolset ss = d.findpossible(s, len, pos);
	if (!ss) break;
	s[pos] = symbol::symbolbit(pickbit(ss));
	pos = o[n++];
      }
    }
  }

  t.stop();

  return t.getticks();
}
