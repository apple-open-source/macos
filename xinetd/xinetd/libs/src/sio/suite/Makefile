# (c) Copyright 1992, 1993 by Panagiotis Tsirigotis
# All rights reserved.  The file named COPYRIGHT specifies the terms 
# and conditions for redistribution.

#
# $Id: Makefile,v 1.1.1.1 2002/01/31 07:09:04 zarzycki Exp $
#

CC					= cc -I..

#
# NOTE: When using the test scripts, CLFAGS is provided as an
#		  argument to make, the setting of it here has no effect.
#
CFLAGS			= -g
LIBOBJS			= ../libsio.a

DISTRIBUTION_FILES=copytest.c example.c print.c tietest.c buftest.c fdtest.c tester sprint_test testlib README

ALL=Sread Swrite Sputchar Sgetchar Srdline \
	 Sgetc Sputc Sfetch Sflush Sundo switch \
	 Sprint buftest tietest switch2 example fdtest evtest

evtest: evtest.c $(LIBOBJS)
	$(CC) $(CFLAGS) -o $@ evtest.c $(LIBOBJS) $(LDFLAGS) || rm -f $@

fdtest: fdtest.c $(LIBOBJS)
	$(CC) $(CFLAGS) -o $@ fdtest.c $(LIBOBJS) $(LDFLAGS) || rm -f $@

buftest: buftest.c $(LIBOBJS)
	$(CC) $(CFLAGS) -o $@ buftest.c $(LIBOBJS) $(LDFLAGS) || rm -f $@

Sprint: print.c $(LIBOBJS)
	$(CC) $(CFLAGS) -o $@ print.c $(LIBOBJS) $(LDFLAGS) || rm -f $@

Sputchar: copytest.c $(LIBOBJS)
	$(CC) $(CFLAGS) -o $@ copytest.c $(LIBOBJS) $(LDFLAGS) || rm -f $@

Sgetchar: copytest.c $(LIBOBJS)
	$(CC) $(CFLAGS) -o $@ copytest.c $(LIBOBJS) $(LDFLAGS) || rm -f $@

Srdline: copytest.c $(LIBOBJS)
	$(CC) $(CFLAGS) -o $@ copytest.c $(LIBOBJS) || rm -f $@

Sread: copytest.c $(LIBOBJS)
	$(CC) $(CFLAGS) -o $@ copytest.c $(LIBOBJS) $(LDFLAGS) || rm -f $@

Swrite: copytest.c $(LIBOBJS)
	$(CC) $(CFLAGS) -o $@ copytest.c $(LIBOBJS) $(LDFLAGS) || rm -f $@

Sgetc: copytest.c $(LIBOBJS)
	$(CC) $(CFLAGS) -o $@ copytest.c $(LIBOBJS) $(LDFLAGS) || rm -f $@

Sputc: copytest.c $(LIBOBJS)
	$(CC) $(CFLAGS) -o $@ copytest.c $(LIBOBJS) $(LDFLAGS) || rm -f $@

Sfetch: copytest.c $(LIBOBJS)
	$(CC) $(CFLAGS) -o $@ copytest.c $(LIBOBJS) $(LDFLAGS) || rm -f $@

Sflush: copytest.c $(LIBOBJS)
	$(CC) $(CFLAGS) -o $@ copytest.c $(LIBOBJS) $(LDFLAGS) || rm -f $@

Sundo: copytest.c $(LIBOBJS)
	$(CC) $(CFLAGS) -o $@ copytest.c $(LIBOBJS) $(LDFLAGS) || rm -f $@

switch: copytest.c $(LIBOBJS)
	$(CC) $(CFLAGS) -o $@ copytest.c $(LIBOBJS) $(LDFLAGS) || rm -f $@

switch2: copytest.c $(LIBOBJS)
	$(CC) $(CFLAGS) -o $@ copytest.c $(LIBOBJS) $(LDFLAGS) || rm -f $@

tietest: tietest.c $(LIBOBJS)
	$(CC) $(CFLAGS) -o $@ tietest.c $(LIBOBJS) $(LDFLAGS) || rm -f $@

example: example.c $(LIBOBJS)
	$(CC) $(CFLAGS) -o $@ example.c $(LIBOBJS) $(LDFLAGS) || rm -f $@

clean:
	rm -f $(ALL) core

checkout: $(DISTRIBUTION_FILES)

$(DISTRIBUTION_FILES):
	co $@

dist:
	-co -q $(DISTRIBUTION_FILES)
