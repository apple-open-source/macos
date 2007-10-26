##
# Makefile for TimeZoneData
#
# See http://www.gnu.org/manual/make/html_chapter/make_toc.html#SEC_Contents
# for documentation on makefiles. Most of this was culled from the ncurses makefile.
#
##

#################################
#################################
# MAKE VARS
#################################
#################################

# ':=' denotes a "simply expanded" variable. Its value is
# set at the time of definition and it never recursively expands
# when used. This is in contrast to using '=' which denotes a
# recursively expanded variable.

SHELL := /bin/sh

# Sane defaults, which are typically overridden on the command line.
SRCROOT=$(shell pwd)
OBJROOT=$(SRCROOT)/build
DSTROOT=$(OBJROOT)
SYMROOT=$(OBJROOT)
RC_ARCHS=

#################################
# Install
#################################

INSTALL = /usr/bin/install
INSTALLDIR = /usr/local/share/tz

#################################
# B&I TARGETS
#################################

installsrc :
	if test ! -d $(SRCROOT); then mkdir $(SRCROOT); fi;
	tar cf - ./makefile ./tz*.tar.gz | (cd $(SRCROOT) ; tar xfp -);
# 	for i in `find $(SRCROOT)/icuSources/ | grep "CVS$$"` ; do \
# 		if test -d $$i ; then \
# 			rm -rf $$i; \
# 		fi; \
# 	done
# 	for j in `find $(SRCROOT)/icuSources/ | grep ".cvsignore"` ; do \
# 		if test -f $$j ; then \
# 			rm -f $$j; \
# 		fi; \
# 	done

installhdrs : 

install : installhdrs
	if test ! -d $(DSTROOT)$(INSTALLDIR)/; then \
		$(INSTALL) -d -m 0775 $(DSTROOT)$(INSTALLDIR)/; \
	fi;
	$(INSTALL) -b -m 0644 $(SRCROOT)/tz*.tar.gz $(DSTROOT)$(INSTALLDIR)

clean :
