#
# Makefile for chat on Mac OS X
# 
#

ARCHFLAGS = 
BINDIR = $(DSTROOT)/usr/sbin
MANDIR = $(DSTROOT)/usr/share/man

CFLAGS=	-O $(ARCHFLAGS) -DAPPLE

SRCS =	 chat.c
OBJS =	 chat.o
LIBS =   

MANPAGES = chat.8


#
# Load dependencies
#
all:	chat

chat:	$(OBJS)
	$(CC) -o chat $(CFLAGS) $(OBJS) $(LIBS)

install:
	echo $(BINDIR)
	/bin/mkdir -p $(MANDIR)/man8 $(BINDIR) 
	install -c -s -m 555 -o root -g wheel chat $(BINDIR)/chat
	install -c -m 444 -o root -g wheel chat.8 $(MANDIR)/man8

clean:
	rm -f *~ *.o chat

