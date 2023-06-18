# Makefile for cwc
#
# Beware of the importancy of optimization. The responsibility in cwc
# is heavily distributed and not many functions are declared 'inline',
# thus it is important that the compiler inlines as many functions as
# possible

CPP=g++
OPTIMIZE=-O3
#PROFILE=-pg
#DEBUG=-ggdb

CPPFLAGS=-Wall $(OPTIMIZE) $(PROFILE) $(DEBUG)

OBJS=timer.o letterdict.o symbol.o dict.o grid.o cwc.o wordlist.o

cwc: $(OBJS)
	g++ -ocwc $(OBJS) $(CPPFLAGS)

remake: clean cwc

include depend

depend:
	g++ -MM *.cc > depend

$(OBJS): %.o: %.cc %.hh
	$(CPP) $(CPPFLAGS) -c $< -o $@

clean:
	rm -f *~ *.o cwc depend

