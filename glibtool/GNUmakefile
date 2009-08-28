##---------------------------------------------------------------------
# GNUmakefile for glibtool
# Call Makefile to do the work, but for the install case, unpack the
# tarball, patch files and then call the Makefile
##---------------------------------------------------------------------
PROJECT = glibtool
ORIGNAME = libtool
VERSION = 2.2.4
SRCDIR = $(OBJROOT)/SRCDIR
SOURCES = $(SRCDIR)/$(PROJECT)
FIX = $(SRCDIR)/fix
NAMEVERS = $(ORIGNAME)-$(VERSION)
TARBALL = $(NAMEVERS).tar.bz2

no_target:
	@$(MAKE) -f Makefile

##---------------------------------------------------------------------
# Create copy and build there. Touch various .m4 and configure files
# (pausing a bit in-between), to keep the Makefiles happy.
#
# After installing, we need to strip the libltdl libraries and install the
# man page
#
# 3383468 - Remove the 'g' prefix for config.guess and config.sub.
##---------------------------------------------------------------------
install:
	@set -x && if [ ! -d $(SRCDIR) ]; then \
	    ditto $(SRCROOT) $(SRCDIR) && \
	    cd $(SRCDIR) && \
	    gnutar xojf $(TARBALL) && \
	    rm -rf $(PROJECT) && \
	    mv $(NAMEVERS) $(PROJECT) && \
	    cd $(SOURCES) && \
	    for i in `find . -name aclocal.m4`; do \
		ed - $$i < $(FIX)/aclocal.m4.ed || exit 1; \
	    done && \
	    sync && \
	    sleep 2 && \
	    for i in `find . -name configure -o -name libtool.m4`; do \
		ed - $$i < $(FIX)/configure.ed || exit 1; \
	    done; \
	fi
	$(MAKE) -f Makefile install SRCROOT=$(SRCDIR)
	find $(DSTROOT)/usr/lib -type f | xargs strip -S
	bzcat $(SRCROOT)/libltdl.3.tar.bz2 | (cd $(DSTROOT); tar xf -)
	rm -f $(DSTROOT)/usr/share/info/dir

.DEFAULT:
	@$(MAKE) -f Makefile $@
