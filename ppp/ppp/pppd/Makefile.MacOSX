#
# pppd makefile for Mac OS X
#

ARCHFLAGS = 

BINDIR = $(DSTROOT)/usr/sbin
MANDIR = $(DSTROOT)/usr/share/man

OBJS = main.o magic.o fsm.o lcp.o ipcp.o upap.o chap.o chap_ms.o md5.o md4.o ccp.o  \
	auth.o options.o demand.o utils.o sys-MacOSX.o tdb.o tty.o 
        # multilink.o 


COMPILE_FLAGS = -DNO_DRAND48 -DDEBUGUPAP -DDEBUGCHAP -DDEBUGLCP -DDEBUGIPCP -DHAVE_PATHS_H -DPLUGIN -DCHAPMS -DUSE_CRYPT
 
COPTS = $(ARCHFLAGS) -O -D__APPLE__ -D__FreeBSD__

CFLAGS = -g $(COPTS) $(COMPILE_FLAGS) -I../include -F/System/Library/Frameworks -F/System/Library/PrivateFrameworks

all: pppd

pppd: $(OBJS) 
	$(CC) $(CFLAGS) -nostdlib -r $(OBJS) -o private_obj.o
	$(CC) $(CFLAGS) -o $(SYMROOT)/pppd private_obj.o $(LIBS) -framework CoreFoundation -framework SystemConfiguration
    
install:
	/bin/mkdir -p $(MANDIR)/man8 $(BINDIR) 
	install -c -m 4511 -o root -g daemon $(SYMROOT)/pppd $(BINDIR)/pppd
	strip -x $(BINDIR)/pppd
	install -c -m  444 -o root -g wheel pppd.8 $(MANDIR)/man8


clean:
	rm -f $(OBJS) $(SYMROOT)/pppd *.o *~ 

