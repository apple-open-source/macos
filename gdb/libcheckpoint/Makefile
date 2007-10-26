# Makefile for libcheckpoint library of helper functions for the inferior process.

SRCROOT = .
OBJROOT = $(SRCROOT)
SYMROOT = $(OBJROOT)
DSTROOT =

GDBDIR = /usr/libexec/gdb
DSTDIR = $(DSTROOT)$(GDBDIR)

LIB = libcheckpoint.dylib

CFILES = cpfork.c cpcg.c

OBJS = $(CFILES:%.c=$(OBJROOT)/%.o)

INSTALL = install

CC = cc

CFLAGS = -gdwarf-2 $(RC_CFLAGS)

.SUFFIXES : .c .h .o

.PHONY : all install clean

# Standard targets.

all : $(SYMROOT)/$(LIB)

# (Note that installsrc and installhdrs not needed)

install : all
	mkdir -p $(DSTDIR)
	dsymutil $(SYMROOT)/$(LIB)
	strip -S -o $(DSTDIR)/$(LIB) $(SYMROOT)/$(LIB)

clean :
	rm -f $(OBJS) $(SYMROOT)/$(LIB)

$(OBJROOT)/%.o : $(SRCROOT)/%.c
	mkdir -p $(OBJROOT)
	$(CC) -c $(CFLAGS) $< -o $@

$(SYMROOT)/$(LIB) : $(OBJS)
	$(CC) $(CFLAGS) -o $@ -dynamiclib \
		-install_name $(GDBDIR)/$(LIB) \
		-single_module -compatibility_version 1 \
		-current_version 1 $(OBJS) -framework ApplicationServices
