##---------------------------------------------------------------------
# GNUmakefile for glibtool
# Call Makefile to do the work, but for the install case, unpack the
# tarball, patch files and then call the Makefile
##---------------------------------------------------------------------
PROJECT = glibtool
ORIGNAME = libtool
VERSION = 1.5
SRCDIR = $(OBJROOT)/SRCDIR
SOURCES = $(SRCDIR)/$(PROJECT)
FIX = $(SRCDIR)/fix
NAMEVERS = $(ORIGNAME)-$(VERSION)
TARBALL = $(NAMEVERS).tar.gz

no_target:
	@$(MAKE) -f Makefile

##---------------------------------------------------------------------
# Create copy and build there.  To fix the problem with tildes, we
# replace the tildes with CTRL-B, using two "ed" scripts.  Then we
# apply a patch file that disables the use of "strip -x" which even
# fails on libltdl.a.  Finally, touch various .m4, Makefile.in and
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
	    for i in `find . -name \*.m4 -o -name configure -o -name ltmain\*`; do \
		echo Patching $$i; \
		ed - $$i < $(FIX)/fixtilde.ed; \
	    done; \
	    for i in `find . -type f | xargs grep -l -e -dynamiclib`; do \
		echo Patching $$i; \
		ed - $$i < $(FIX)/dynamiclib.ed; \
	    done; \
	    for i in `find . -type f | xargs grep -l -e -bundle`; do \
		echo Patching $$i; \
		ed - $$i < $(FIX)/bundle.ed; \
	    done; \
	    for i in `find . -type f | xargs grep -l '$$LD$$reload_flag'`; do \
		echo Patching $$i; \
		ed - $$i < $(FIX)/fixld.ed; \
	    done; \
	    for i in `find . -type f | xargs grep -l '^# Libtool was configured on host'`; do \
		echo Patching $$i; \
		ed - $$i < $(FIX)/addsep.ed; \
	    done; \
	    for i in `find . -type f ! -name ChangeLog\* | xargs grep -l 'delay_variable_subst'`; do \
		echo Patching $$i; \
		ed - $$i < $(FIX)/unescape.ed; \
	    done; \
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
	install -d -m 0755 $(DSTROOT)/usr/share/man/man1
	install -m 0644 $(SRCDIR)/glibtool.1 $(DSTROOT)/usr/share/man/man1
	mv -f $(DSTROOT)/usr/share/libtool/gconfig.guess \
	    $(DSTROOT)/usr/share/libtool/config.guess
	mv -f $(DSTROOT)/usr/share/libtool/gconfig.sub \
	    $(DSTROOT)/usr/share/libtool/config.sub

.DEFAULT:
	@$(MAKE) -f Makefile $@
