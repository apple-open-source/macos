SHELL		:= /bin/sh

VERSION		= 1.0
CC		= cc
CPPFLAGS	= -I$(SRCROOT)
CFLAGS		= -Os -g3 -no-cpp-precomp -Wall $(RC_CFLAGS)
LDFLAGS		= $(RC_CFLAGS) -install_name /usr/lib/libutil.dylib -compatibility_version $(VERSION) \
		  -current_version $(VERSION)
INSTALL		= install -c
LN		= ln
MKDIR		= mkdir
STRIP		= strip
AR		= ar
RANLIB		= ranlib

SRCROOT		= .
OBJROOT		= $(SRCROOT)
SYMROOT		= $(OBJROOT)
DSTROOT		=

LIB		:= libutil1.0.dylib
SRCS		:= _secure_path.c getmntopts.c humanize_number.c \
	           pidfile.c property.c realhostname.c trimdomain.c uucplock.c
HDRS		:= libutil.h mntopts.h
MAN3		:= _secure_path.3 getmntopts.3 humanize_number.3 pidfile.3 \
		   property.3 realhostname.3 realhostname_sa.3 trimdomain.3 uucplock.3

.SUFFIXES :
.SUFFIXES : .c .h .o

.PHONY :
.PHONY : all installsrc installhdrs install clean installlib installman

all : $(SYMROOT)/$(LIB) 

#
# xbs targets.
#
installsrc :
	@if test ! -d $(SRCROOT); then \
		echo "$(INSTALL) -d $(SRCROOT)"; \
		$(INSTALL) -d $(SRCROOT); \
	fi
	tar cf - . | (cd $(SRCROOT); tar xpf -)
	@for i in `find $(SRCROOT) | grep "/\.svn$$"`; do \
		if test -d $$i ; then \
			echo "rm -rf $$i"; \
			rm -rf $$i; \
		fi; \
	done

installhdrs :
	$(INSTALL) -d $(DSTROOT)/usr/local/include
	$(INSTALL) -m 0644 $(HDRS) $(DSTROOT)/usr/local/include


install : installhdrs installlib strip installman install-plist

clean :
	rm -f $(patsubst %.c,$(OBJROOT)/%.o,$(SRCS))
	rm -f $(SYMROOT)/*~
	rm -f $(SRCROOT)/.\#*
	rm -f $(SYMROOT)/$(LIB)

strip:
	$(STRIP) -x -S $(DSTROOT)/usr/lib/$(LIB)

#
# Internal targets and rules.
#
installlib : $(SYMROOT)/$(LIB)
	$(INSTALL) -d $(DSTROOT)/usr/lib
	$(INSTALL) -m 0755 $< $(DSTROOT)/usr/lib
	$(LN) -fs libutil1.0.dylib $(DSTROOT)/usr/lib/libutil.dylib

installman :
	$(INSTALL) -d $(DSTROOT)/usr/local/share/man/man3
	@for i in $(MAN3); do\
		echo "$(INSTALL) -m 0444 $(SRCROOT)/$$i $(DSTROOT)/usr/local/share/man/man3/"; \
		$(INSTALL) -m 0444 $(SRCROOT)/$$i $(DSTROOT)/usr/local/share/man/man3; \
	done

$(OBJROOT)/%.o : $(SRCROOT)/%.c \
	     $(patsubst %.h,$(SRCROOT)/%.h,$(HDRS))
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@

$(SYMROOT)/$(LIB) : $(patsubst %.c,$(OBJROOT)/%.o,$(SRCS))
	$(CC) -dynamiclib $(LDFLAGS) -o $@ $?

OSV	= $(DSTROOT)/usr/local/OpenSourceVersions
OSL	= $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL) $(SRCROOT)/libutil.plist $(OSV)/
	$(MKDIR) $(OSL)
	$(INSTALL) $(SRCROOT)/libutil.txt $(OSL)/
