# Makefile for poll(2) emulator
#
# $Id: Makefile,v 1.3 2003/08/31 18:31:28 emoy Exp $
# ---------------------------------------------------------------------------

#############################################################################
# Definitions that you need to edit
#############################################################################

# ---------------------------------------------------------------------------
# Installation prefix

PREFIX		= /usr/local
LIBDIR		= $(PREFIX)/lib
INCDIR		= $(PREFIX)/include

# ---------------------------------------------------------------------------
# Platform-specific bits
#
# For GNU CC on *BSD. Should work for FreeBSD, NetBSD, OpenBSD and BSD/OS
LINK_SHARED	= $(CC) -dynamiclib
SHLIB_EXT	= so
SHLIB_NOVER	= $(LIB_NAME).$(SHLIB_EXT)
SHLIB		= $(LIB_NAME).$(SHLIB_EXT).$(VERSION)
SHLIB_INSTALLED = $(LIBDIR)/$(LIB_NAME).$(SHLIB_EXT).$(MAJOR)

# Benjamin Reed <ranger@befunk.com>:
# On Mac OS X, comment out the above lines, and uncomment these instead.
#LINK_SHARED	= $(CC) -install_name $(PREFIX)/lib/$(SHLIB) \
#			-compatibility_version $(COMPAT_VERSION) \
#			-current_version $(VERSION) -dynamiclib
#SHLIB_EXT	= dylib
#SHLIB_NOVER	= $(LIB_NAME).$(SHLIB_EXT)
#SHLIB		= $(LIB_NAME).$(VERSION).$(SHLIB_EXT)
#SHLIB_INSTALLED= $(LIBDIR)/$(LIB_NAME).$(MAJOR).$(SHLIB_EXT)

# If you have a BSD-compatible install(1), use:
INSTALL		= install -c

# If you do not have a BSD-compatible install(1), use:
#INSTALL	= ./install.sh -c

# ---------------------------------------------------------------------------
# Compilation and Linkage

MAJOR		= 1
MINOR		= 3
VERSION		= $(MAJOR).$(MINOR)
COMPAT_VERSION	= $(MAJOR)
CC		= cc
LIB_NAME	= libpoll
LIB		= $(LIB_NAME).a
COMPILE_STATIC	= $(CC) $(CFLAGS) -c 
COMPILE_SHARED	= $(CC) $(CFLAGS) -c
RANLIB		= ranlib

#############################################################################
# There should be no need to edit past this point
#############################################################################

.SUFFIXES: .po

.c.po:
	$(COMPILE_SHARED) $< -o $*.po
.c.o:
	$(COMPILE_STATIC) $<

all:		libs
libs:		$(SHLIB) $(LIB)
test:		polltest

dirs:
		@echo "creating directories..."
		$(INSTALL) -m 755 -d $(LIBDIR)
		$(INSTALL) -m 755 -d $(INCDIR)

install:	all dirs
		$(INSTALL) -m 755 $(SHLIB) $(LIB) $(LIBDIR)
		ln -sf $(SHLIB) $(SHLIB_INSTALLED)
		ln -sf $(SHLIB) $(LIBDIR)/$(SHLIB_NOVER)
		$(INSTALL) -m 644 poll.h $(INCDIR)

clean:
		rm -f poll.po *.o $(LIB) $(SHLIB) $(SHLIB_NOVER) polltest

$(SHLIB):	poll.po
		$(LINK_SHARED) -o $(SHLIB) poll.po
$(LIB):		poll.o
		ar rv $(LIB) poll.o
		ranlib $(LIB)
polltest:	polltest.o
		$(CC) -o polltest polltest.o $(LIB)
