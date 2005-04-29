CC=gcc

all: dot demo

dot.lo: dot.c
	libtool --mode=compile ${CC} `dotneato-config --cflags` -c dot.c -o dot.lo

dot: dot.lo
	libtool --mode=link ${CC} dot.lo `dotneato-config --libtool` `dotneato-config --libs` -o dot


demo.lo: demo.c
	libtool --mode=compile ${CC} `dotneato-config --cflags` -c demo.c -o demo.lo

demo: demo.lo
	libtool --mode=link ${CC} demo.lo `dotneato-config --libtool` `dotneato-config --libs` -o demo


clean:
	rm -f dot demo *.lo
