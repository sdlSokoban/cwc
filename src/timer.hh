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

#include <sys/times.h>

/**
 * The timer module implement a simple stop-watch time
 * measuring process time (not real time).
 */

class timer {
protected:
  clock_t elapsed, starttime;
  bool running;
  clock_t getptime();
public:
  clock_t getticks();
  timer();
  void start();
  void stop();
  void reset();
  int getmsecs();
};
