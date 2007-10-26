# This is the auto-Makefile for the lib subdirectory of Taylor UUCP.

noinst_LIBRARIES = libuucp.a

libuucp_a_SOURCES = buffer.c crc.c debug.c escape.c getopt.c getop1.c \
	parse.c quote.c quotes.c spool.c status.c xfree.c xmall.c xreall.c
libuucp_a_LIBADD = $(LIBOBJS)

AM_CFLAGS = -I.. -I$(srcdir)/.. $(WARN_CFLAGS)
