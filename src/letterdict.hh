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

#include <set>
#include <vector>
#include "symbol.hh"
#include "dict.hh"
#include "wordlist.hh"

class letterdict : public dict {
  typedef vector<int> intvec;
  intvec ****p;
  symbolset **all;
  wordlist *wl;
  static intvec emptyvec;
public:
  letterdict();
  void addword(symbol *i, int wordi);
  intvec *getintvec(int len, int pos, symbol s);
  symbolset findpossible(symbol *, int len, int pos);
  void load(const string &fn);
};
