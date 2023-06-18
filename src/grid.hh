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

#ifndef GRID_HH
#define GRID_HH

#include <vector>
#include "symbol.hh"
#include "dict.hh"

class cell;
class wordblock;
class grid;

struct wordref {
  int pos;
  wordblock *wbl;
};

class cell {
  vector<wordref> wbl; int wbl_size;
  int attempts;
  symbol symb;
  symbol preferred;
  bool locked;
public:
  static cell outside_cell;

  cell(symbol s = symbol::empty); // ctor
  void setsymbol(const symbol &s);

  // structural methods
  void addword(wordblock *w, int pos);
  int numwords() { return wbl_size; }
  wordblock &getwordblock(int wordno) { return *wbl[wordno].wbl; }
  int getpos(int wordno) { return wbl[wordno].pos; }
  void clearwords() { wbl.clear(); }

  symbol getsymbol() { return symb; }
  symbol getpreferred() { return preferred; }

  bool haspreferred() { return preferred != symbol::none; }
  void usepreferred();
  void remove(); // remove from grid (make outsider)
  void clear(bool setpreferred = true); // make empty

  bool isoutside();
  bool isinside() { return !isoutside(); }

  bool isempty() {
    return symb == symbol::empty;
  }

  bool isfilled();

  int getattempts() { return attempts; }

  bool islocked() { return locked; }
  void lock() { locked = true; }

  symbolset findpossible(dict &d);

  friend ostream&operator<<(ostream &os, cell &c);

  void dumpwords() {
    cout << "got " << numwords() << " words." << endl;
  }

  // statistics

  int depencydegree(int level);
};

struct coord {
  int x, y;
public:
  coord(int _x = 0, int _y = 0);
  bool operator==(const coord &c) const { return ((x==c.x)&&(y==c.y)); }
};

class grid {
protected:
  vector<cell> cls; int cls_size;
  vector<wordblock*> wbl;
  void init_grid(int w, int h);

public:
  bool verbose;
  int w, h;
  grid(int width = 4, int height = 4);

  inline cell &cellno(int n) {
    if ((n < 0)||(n >= cls_size))
      return cell::outside_cell;
    return cls[n];
  }
  
  inline int cellnofromxy(int x, int y) {
    return y*w + x;
  }

  inline cell &cellat(int x, int y) { 
    if ((x < 0)||(x >= w)||(y < 0)||(y>=h))
      return cell::outside_cell;
    return cls[y*w + x];
  }

  cell &cellat(coord &c) {
    return cellat(c.x, c.y);
  }

  cell &operator()(int x, int y) { return cellat(x, y); }
  cell &operator()(coord &c) { return cellat(c); }
  cell &operator()(int p) { return cellno(p); }

  void load_template(const string &filename);
  void load(const string &fn);
  void buildwords();

  void dump(ostream &os, setup_s::output_format_t);
  void dump_ascii(ostream &os);
  void dump_simple(ostream &os);
  void dump_ggrid(ostream &os);

  void lock();

  int getempty();

  // statistics

  float interlockdegree();
  float density();
  float attemptaverage();
  int numopen();
  int numcells() { return cls.size(); }
  double depencydegree(int level);
  int celldepencies(int cellno, int level);
};


struct cellref {
  int cellno;
  grid *g;
  cellref(int no, grid &gr) : cellno(no), g(&gr) {}
  cell *operator ->() { return &g->cellno(cellno); }
  cell *ptr() { return &g->cellno(cellno); }
};


class wordblock {
  vector<cellref> cls; int cls_size;
public:
  wordblock();
  void addcell(int n, grid &gr) {
    cls.push_back(cellref(n, gr));
    cls_size++;
  }
  int length() { return cls_size; }
  void getword(symbol *);
  int getcellno(int pos) { 
    if ((pos < 0)||(pos >= cls_size)) throw error("Bug");
    return cls[pos].cellno; 
  }
  cell &getcell(int pos) { 
    if ((pos < 0)||(pos >= cls_size)) return cell::outside_cell;
    return *cls[pos].ptr(); 
  }
};

ostream &operator << (ostream &os, coord &c);


#endif
