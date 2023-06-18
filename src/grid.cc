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

#include <fstream.h>
#include <stdio.h>
#include <strstream>

#include <set>
#include "grid.hh"

//////////////////////////////////////////////////////////////////////
// wordblock

wordblock::wordblock() {
  cls_size = 0;
}

void wordblock::getword(symbol *s) {
  int i, len = cls.size();
  for (i = 0; i < len; i++)
    s[i] = cls[i]->getsymbol();
}

//////////////////////////////////////////////////////////////////////
// class cell

cell cell::outside_cell;

cell::cell(symbol s) : 
  wbl_size(0), attempts(0), symb(s), preferred(symbol::none), locked(false) {
}

void cell::addword(wordblock *w, int pos) {
  struct wordref wr = {pos, w};
  wbl.push_back(wr);
  wbl_size++;
}

void cell::setsymbol(const symbol &s) {
  if (locked)
    throw error("Attempt to set symbol in locked cell");
  if (s != symbol::empty && s != symbol::outside)
    attempts++;
  symb = s;
}

void cell::remove() {
  symb = symbol::outside;
}

void cell::clear(bool setpreferred) {
  if (setpreferred) 
    preferred = symb;
  else
    preferred = symbol::none;
  symb = symbol::empty;
}

ostream &operator << (ostream &os, cell &c) {
  return os << c.symb;
}

bool cell::isoutside() {
  return symb == symbol::outside;
}

bool cell::isfilled() {
  return (symb!=symbol::empty)&&(symb!=symbol::outside);
}

//////////////////////////////////////////////////////////////////////
// class coord

coord::coord(int _x, int _y) : x(_x), y(_y) {
}

ostream &operator << (ostream &os, coord &c) {
  return os << c.x << ',' << c.y;
}

//////////////////////////////////////////////////////////////////////
// class grid

grid::grid(int width, int height) 
  : cls(0), cls_size(0), verbose(false) {
  init_grid(width, height);
  cellno(-1).setsymbol(symbol::outside);
  buildwords();
}

void grid::init_grid(int w, int h) {
  this->w = w;
  this->h = h;
  cls.clear();
  for (int i = 0; i < w*h; i++)
    cls.push_back(cell());
  cls_size = cls.size();
}

/**
void grid::get_template_h(const coord &c, symbol *s, int &len, int &pos) {
  coord i = c;
  // find first
  while (!cellat(i.x-1, i.y).isoutside())
    i.x--;
  pos = c.x - i.x;
  len = 0;
  while (!cellat(i).isoutside()) {
    s[len] = cellat(i).getsymbol();
    i.x++; len++;
  }
}

void grid::get_template_v(const coord &c, symbol *s, int &len, int &pos) {
  coord i = c;
  // find first
  while (!cellat(i.x, i.y-1).isoutside())
    i.y--;
  pos = c.y - i.y;
  len = 0;
  while (!cellat(i).isoutside()) {
    s[len] = cellat(i).getsymbol();
    i.y++; len++;
  }
}
**/

symbolset cell::findpossible(dict &d) {
  int nwords = numwords();
  if (nwords == 0) throw error("Bugger");
  
  symbolset ss = ~0;

  for (int i = 0; i < nwords; i++) {

    int pos = getpos(i);
    wordblock &wb = getwordblock(i);
    int len = wb.length();
   
    symbol word[len+1]; word[len] = symbol::outside;

    wb.getword(word);

    ss &= d.findpossible(word, len, pos); // intersect solutions
    if (setup.verbose) {
      cout << "vertical: "; dumpsymbollist(word, len);
      dumpset(ss);
    }
  }
  
  return ss;
}


void grid::load_template(const string &filename) {
  ifstream tf(filename.c_str());
  if (!tf.is_open()) throw error("Failed to open pattern file");
  string istr;
  getline(tf, istr);
  w = h = 1;
  sscanf(istr.c_str(), "%d %d", &w, &h);
  if ((w==0)||(h==0))
    throw error("Grid dimensions must be at least 1");
  init_grid(w, h);
  
  for (int y=0;y<h;y++) {
    getline(tf,istr);
    if (tf.eof()) throw error("Not enough lines in input file");
    // if (istr.length() < unsigned(w+1)) throw error("Line to short in input file");
    for (int x=0;x<w;x++) {
      if (unsigned(x) >= istr.length()) {
	cellat(x, y).remove();
	continue;
      }

      char ch = istr[x];
      if (ch == '+')
	cellat(x, y).clear();
      else if (ch == ' ' )
	cellat(x, y).remove();
      else if (isalpha(ch))
	cellat(x, y).setsymbol(tolower(ch));
      else
	throw error("Invalid character in input file");
    }
  }
  buildwords();
  lock();
}

void grid::load(const string &fn) {
  cls.clear();
  wbl.clear();
  w = h = 0;

  ifstream f(fn.c_str());
  if (!f.is_open()) throw error("Failed to open file");
  string ln;

  while (!f.eof()) {
    getline(f, ln);
    const char *st = ln.c_str();
    
    wordblock *wb = new wordblock();
    int pos = 0;
    while (*st != '\0') {
      while (*st&&(!isdigit(*st))) st++;
      if (*st == '\0') break;
      int a = atoi(st);
      while (cls.size() <= unsigned(a))
	cls.push_back(cell(symbol::outside));
      cls_size = cls.size();
      cls[a].setsymbol(symbol::empty);
      wb->addcell(a, *this);
      cls[a].addword(wb, pos); pos++;
      while (*st && (isdigit(*st))) st++;
    }
    if (wb->length())
      wbl.push_back(wb);
    else
      delete wb;
  }
  lock();
}

/**
 * builds the words/cell structures when we use a square grid formation
 */

void grid::buildwords() {
  wbl.clear();
  for (int n = 0; n < numcells(); n++)
    cls[n].clearwords();

  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      int cno = cellnofromxy(x, y);
      wordblock *w = 0;
      int pos = 0;
      while (cellat(x, y).isinside()) {
	if (w==0) { w = new wordblock(); wbl.push_back(w); }
	w->addcell(cno, *this);
	cellno(cno).addword(w, pos);
	pos++;
	x++;
	cno = cellnofromxy(x, y);
      }
    }
  }
  for (int x = 0; x < w; x++) {
    for (int y = 0; y < h; y++) {
      wordblock *w = 0;
      int pos = 0;
      while (cellat(x, y).isinside()) {
	if (w == 0) { w = new wordblock(); wbl.push_back(w); }
	w->addcell(cellnofromxy(x, y), *this);
	cellat(x, y).addword(w, pos);
	pos++; y++;
      }
    }
  }
}

void grid::dump_ggrid(ostream &os) {
  bool first = true;
  for (vector<wordblock*>::iterator i = wbl.begin(); i != wbl.end(); i++) {
    int wlen = (*i)->length();
    for (int p = 0; p < wlen; p++) {
      if (!first) cout << ' ';
      os << (*i)->getcellno(p);
      first = false;
    }
    os << endl; first = true;
  }
}

char bold[] = "\x1b[1m";
char normal[] = "\x1b[0m";

void grid::dump_ascii(ostream &os) {
  strstream vertbarstream;
  vertbarstream << '+';
  for (int i=0;i<w;i++) vertbarstream << "---+";
  vertbarstream << ends;
  string vertbar = vertbarstream.str();
  
  os << vertbar << endl;
  for (int y=0;y<h;y++) {
    os << "|";
    for (int x=0; x<w; x++) {
      cell &c = cellat(x,y);
      if (c.isoutside())
	os << "XXX|";
      else
	os << ' ' <<  c << " |";
    }
    os << endl;
    os << vertbar << endl;
  }
}

void grid::dump_simple(ostream &os) {
  for (int y=0;y<h;y++) {
    for (int x=0;x<w;x++) {
      os << cellat(x, y);
    }
    os << endl;
  }
}

void grid::dump(ostream &os, setup_s::output_format_t fmt) {
  if (w == 0) {
    for (int i = 0; unsigned(i) < wbl.size(); i++) {
      int len = wbl[i]->length();
      symbol *s = new symbol[len + 1];
      s[len] = symbol::outside;
      wbl[i]->getword(s);
      cout << s << ' ';
      delete s;

      cout << '(';
      for (int p = 0; p < len; p++) {
	if (p) cout << ',';
	cout << wbl[i]->getcellno(p);
      }
      cout << ')' << endl;
    }
    return;
  }
    
  switch (fmt) {
  case setup.ascii_format:
    dump_ascii(os); break;
  case setup.simple_format:
    dump_simple(os); break;
  }
}

float grid::interlockdegree() {
  int interlocked = 0, total = 0;
  for (int y=0; y<h; y++) {
    for (int x=0;x<w;x++) {
      if (cellat(x, y).isinside()) {
	bool vertuse = cellat(x, y-1).isinside() || cellat(x, y+1).isinside();
	bool horizuse = cellat(x-1, y).isinside() || cellat(x+1, y).isinside();
	if (vertuse && horizuse)
	  interlocked++;
	total++;
      }
    }
  }
  return float(interlocked) / float(total);
}

float grid::attemptaverage() {
  int sum = 0, n = 0;
  for (int y=0; y<h; y++) {
    for (int x=0; x<w; x++) {
      if (cellat(x, y).isinside()) {
	sum += cellat(x, y).getattempts();
	n++;
      }
    }
  }
  return sum / float(n);
}

double grid::depencydegree(int level) {
  int n = numcells(), ncell = 0;
  int d = 0;
  for (int i = 0; i < n; i++) {
    if (cellno(i).isinside()) {
      d += celldepencies(i, level);
      ncell++;
    }
  }
  return double(d) / double(ncell);
}

int grid::celldepencies(int cno, int level) {
  set<int> cnums;
  cnums.insert(cno);

  while (level--) {
    set<int> newnums;
    for (set<int>::iterator i = cnums.begin(); i != cnums.end(); i++) {
      int clno = *i;
      int words = cellno(clno).numwords();
      for (int w = 0; w < words; w++) {
	wordblock &wb = cellno(clno).getwordblock(w);
	int wlen = wb.length();
	for (int j = 0; j < wlen; j++)
	  newnums.insert(wb.getcellno(j));
      }
    }
    for (set<int>::iterator i = newnums.begin(); i != newnums.end(); i++)
      cnums.insert(*i);
  }
  return cnums.size();
}

void grid::lock() {
  int n = numcells();
  for (int i = 0; i < n; i++)
    if (!cellno(i).isempty())
      cellno(i).lock();
}

int grid::getempty() { 
  int n = 0;
  int ncells = numcells();
  for (int i = 0 ; i< ncells; i++)
    if (cellno(i).isempty())
      n++;
  return n;
}

int grid::numopen() {
  int n=0;
  int ncells = numcells();
  for (int i = 0; i < ncells; i++)
    if (!cls[i].islocked())
      n++;
  return n;
}

