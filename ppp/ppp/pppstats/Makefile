#
# pppstats makefile
#

ARCHFLAGS = 

BINDIR = $(DSTROOT)/usr/sbin
MANDIR = $(DSTROOT)/usr/share/man

CFLAGS = -O  -D__FreeBSD__ $(ARCHFLAGS) -I../include

SRCS = pppstats.c
OBJS = pppstats.o
LIBS = 

all: pppstats

pppstats: $(OBJS)
	$(CC) -o pppstats $(CFLAGS) $(OBJS) $(LIBS)


install: 
	/bin/mkdir -p $(MANDIR)/man8 $(BINDIR) 
	install -c -s -m 555 -o root -g daemon pppstats $(BINDIR)/pppstats
	install -c -m 444 -o root -g wheel pppstats.8 $(MANDIR)/man8

clean:
	rm -f pppstats *.o *~ core

