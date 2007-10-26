##---------------------------------------------------------------------
# GNUmakefile for glibtool
# Call Makefile to do the work, but for the install case, unpack the
# tarball, patch files and then call the Makefile
##---------------------------------------------------------------------
PROJECT = glibtool
ORIGNAME = libtool
VERSION = 1.5.22
SRCDIR = $(OBJROOT)/SRCDIR
SOURCES = $(SRCDIR)/$(PROJECT)
FIX = $(SRCDIR)/fix
NAMEVERS = $(ORIGNAME)-$(VERSION)
TARBALL = $(NAMEVERS).tar.gz

no_target:
	@$(MAKE) -f Makefile

##---------------------------------------------------------------------
# Create copy and build there. Ttouch various .m4, Makefile.in and
# configure files (pausing a second in-between), to keep the Makefiles
# happy.
#
# After installing, we need to strip the libltdl libraries and install the
# man page
#
# 3383468 - Remove the 'g' prefix for config.guess and config.sub.
##---------------------------------------------------------------------
install:
	@if [ ! -d $(SRCDIR) ]; then \
	    echo ditto $(SRCROOT) $(SRCDIR); \
	    ditto $(SRCROOT) $(SRCDIR); \
	    echo cd $(SRCDIR); \
	    cd $(SRCDIR); \
	    echo gnutar xzf $(TARBALL); \
	    gnutar xzf $(TARBALL); \
	    echo rm -rf $(PROJECT); \
	    rm -rf $(PROJECT); \
	    echo mv $(NAMEVERS) $(PROJECT); \
	    mv $(NAMEVERS) $(PROJECT); \
	    echo cd $(SOURCES); \
	    cd $(SOURCES); \
	    echo patch -p0 < $(FIX)/Patches; \
	    patch -p0 < $(FIX)/Patches; \
	    sync; \
	    sleep 2; \
	    echo Touching acinclude.m4 files; \
	    find . -name acinclude.m4 | xargs touch; \
	    sleep 2; \
	    echo Touching aclocal.m4 files; \
	    find . -name aclocal.m4 | xargs touch; \
	    sleep 2; \
	    echo Touching config-h.in files; \
	    find . -name config-h.in | xargs touch; \
	    sleep 2; \
	    echo Touching Makefile.in files; \
	    find . -name Makefile.in | xargs touch; \
	    sleep 2; \
	    echo Touching configure files; \
	    find . -name configure | xargs touch; \
	fi
	LIBTOOL_CMD_SEP= $(MAKE) -f Makefile install SRCROOT=$(SRCDIR)
	find $(DSTROOT)/usr/lib -type f | xargs strip -S

.DEFAULT:
	@$(MAKE) -f Makefile $@
