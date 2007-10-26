#
# xbs-compatible Makefile for top.
#

SHELL		:= /bin/sh

CC		= cc
CPPFLAGS	= -I$(SRCROOT) -DTOP_DEPRECATED #-DTOP_JAGUAR
CFLAGS		= -Os -g3 -no-cpp-precomp -Wall $(RC_CFLAGS)
LIB_LDFLAGS	= -framework CoreFoundation -framework IOKit
BIN_LDFLAGS	= -lpanel -lncurses -lutil
LDFLAGS		= $(LIB_LDFLAGS) $(BIN_LDFLAGS)
INSTALL		= install -c
LN		= ln
STRIP		= strip
AR		= ar
RANLIB		= ranlib

SRCROOT		= .
OBJROOT		= $(SRCROOT)
SYMROOT		= $(OBJROOT)
DSTROOT		=

# File lists related to the library.
LIB		:= libtop.a
LIB_HDRS	:= libtop.h
LIB_PHDRS	:= ch.h dch.h ql.h qr.h rb.h
LIB_SRCS	:= ch.c dch.c libtop.c

# File lists related to the binary.
BIN		:= top
BIN_SRCS	:= disp.c log.c samp.c top.c
BIN_HDRS	:= disp.h log.h samp.h top.h
BIN_MAN1	:= top.1

.SUFFIXES :
.SUFFIXES : .c .h .o

.PHONY :
.PHONY : all installsrc installhdrs install clean installlib installbin \
	 installman

all : $(SYMROOT)/$(LIB) $(SYMROOT)/$(BIN)

#
# xbs targets.
#
installsrc :
	@if test ! -d $(SRCROOT); then \
		echo "$(INSTALL) -d $(SRCROOT)"; \
		$(INSTALL) -d $(SRCROOT); \
	fi
	tar cf - . | (cd $(SRCROOT); tar xpf -)
	@for i in `find $(SRCROOT) | grep "CVS$$"`; do \
		if test -d $$i ; then \
			echo "rm -rf $$i"; \
			rm -rf $$i; \
		fi; \
	done

installhdrs :

install : installlib installbin installman

clean :
	rm -f $(patsubst %.c,$(OBJROOT)/%.o,$(LIB_SRCS) $(BIN_SRCS))
	rm -f $(SYMROOT)/$(LIB) $(SYMROOT)/$(BIN)

#
# Internal targets and rules.
#
installlib : $(SYMROOT)/$(LIB)
	$(INSTALL) -d $(DSTROOT)/usr/local/lib
	$(INSTALL) -m 0755 $< $(DSTROOT)/usr/local/lib
	$(INSTALL) -d $(DSTROOT)/usr/local/include
	$(INSTALL) -m 0644 $(LIB_HDRS) $(DSTROOT)/usr/local/include

installbin : $(SYMROOT)/$(BIN)
	$(INSTALL) -d $(DSTROOT)/usr/bin
	$(INSTALL) -s -m 4755 $< $(DSTROOT)/usr/bin

installman :
	$(INSTALL) -d $(DSTROOT)/usr/share/man/man1
	@for i in $(BIN_MAN1); do\
		echo "$(INSTALL) -m 0444 $(SRCROOT)/$$i $(DSTROOT)/usr/share/man/man1/"; \
		$(INSTALL) -m 0444 $(SRCROOT)/$$i $(DSTROOT)/usr/share/man/man1; \
	done

$(OBJROOT)/%.o : $(SRCROOT)/%.c \
	     $(patsubst %.h,$(SRCROOT)/%.h,$(LIB_HDRS) $(LIB_PHDRS) $(BIN_HDRS))
	$(CC) -c $(CPPFLAGS) $(CFLAGS) -mdynamic-no-pic $< -o $@

$(SYMROOT)/$(LIB) : $(patsubst %.c,$(SRCROOT)/%.c,$(LIB_SRCS)) \
	     $(patsubst %.h,$(SRCROOT)/%.h,$(LIB_HDRS) $(LIB_PHDRS))
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $(LIB_SRCS)
	mv *.o $(OBJROOT)
	$(AR) cru $@ $(patsubst %.c,$(OBJROOT)/%.o,$(LIB_SRCS))
	$(RANLIB) $@

$(SYMROOT)/$(BIN) : $(patsubst %.c,$(OBJROOT)/%.o,$(BIN_SRCS)) $(SYMROOT)/$(LIB)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	chown root $@
	chmod 4755 $@
