##---------------------------------------------------------------------
# GNUmakefile for python
# Call Makefile to do the work, but for the install case, unpack the
# tarball to create the project source directory
##---------------------------------------------------------------------
PROJECT = python
NAME = Python
VERSION = 2.3
FIX = $(SRCROOT)/fix
NAMEVERS = $(NAME)-$(VERSION)
TARBALL = $(NAMEVERS).tgz
FIX = $(OBJROOT)/fix

no_target:
	@$(MAKE) -f Makefile

##---------------------------------------------------------------------
# We patch configure to remove the "-arch_only ppc" option, since we
# build fat.  We also set DYLD_NEW_LOCAL_SHARED_REGIONS or else python.exe
# will crash.
# PR-3373087: in site.py, catch exception for HOME is not set
# (for received faxes)
##---------------------------------------------------------------------
install:
	@if [ ! -d $(OBJROOT)/$(TARBALL) ]; then \
	    echo ditto $(SRCROOT) $(OBJROOT); \
	    ditto $(SRCROOT) $(OBJROOT); \
	    echo cd $(OBJROOT); \
	    cd $(OBJROOT); \
	    echo gnutar xzf $(TARBALL); \
	    gnutar xzf $(TARBALL); \
	    echo rm -rf $(PROJECT); \
	    rm -rf $(PROJECT); \
	    echo mv $(NAMEVERS) $(PROJECT); \
	    mv $(NAMEVERS) $(PROJECT); \
	    echo Patching configure; \
	    ed - $(PROJECT)/configure < $(FIX)/configure.ed; \
	    echo patch $(PROJECT)/Lib/site.py $(FIX)/site.py.patch; \
	    patch $(PROJECT)/Lib/site.py $(FIX)/site.py.patch; \
	fi
	DYLD_NEW_LOCAL_SHARED_REGIONS=1 $(MAKE) -C $(OBJROOT) -f Makefile \
		install SRCROOT=$(OBJROOT) OBJROOT="$(OBJROOT)/$(PROJECT)"

.DEFAULT:
	@$(MAKE) -f Makefile $@
