##---------------------------------------------------------------------
# GNUmakefile for perl
# Call Makefile to do the work, but for the install case, unpack the
# tarball to create the project source directory
##---------------------------------------------------------------------
PROJECT = perl
VERSION = 5.8.1-RC3
FIX = $(SRCROOT)/fix
PROJVERS = $(PROJECT)-$(VERSION)
TARBALL = $(PROJVERS).tar.bz

no_target:
	@$(MAKE) -f Makefile

##---------------------------------------------------------------------
# We patch hints/darwin.sh to install in $(DSTROOT), and to force putting
# things in the right place.  We also patch lib/ExtUtils/MM_Unix.pm to
# fix a problem where it sometimes loses a slash (this bug has been reported
# so hopefully it will be fix in final 5.8.1).
#
# For pre-release versions of perl, patch perl.c and Config.pm.  For RC3,
# we need to replace CPAN.pm with a regressed version.
##---------------------------------------------------------------------
install:
	@if [ ! -d $(OBJROOT)/$(TARBALL) ]; then \
	    echo ditto $(SRCROOT) $(OBJROOT); \
	    ditto $(SRCROOT) $(OBJROOT); \
	    echo cd $(OBJROOT); \
	    cd $(OBJROOT); \
	    echo bzcat $(TARBALL) \| gnutar xf -; \
	    bzcat $(TARBALL) | gnutar xf -; \
	    echo rm -rf $(PROJECT); \
	    rm -rf $(PROJECT); \
	    echo mv $(PROJVERS) $(PROJECT); \
	    mv $(PROJVERS) $(PROJECT); \
	    echo Patching lib/ExtUtils/MM_Unix.pm; \
	    ed - $(PROJECT)/lib/ExtUtils/MM_Unix.pm < MM_Unix.pm.ed; \
	    echo Patching hints/darwin.sh; \
	    cat hints.append >> $(PROJECT)/hints/darwin.sh; \
	    echo Patching perl.c; \
	    ed - $(PROJECT)/perl.c < pre-perl.c.ed; \
	    echo cp CPAN.pm $(PROJECT)/lib/CPAN.pm; \
	    cp CPAN.pm $(PROJECT)/lib/CPAN.pm; \
	    for i in `find $(PROJECT) -type f | xargs fgrep -l DARWIN`; do \
		echo Patching $$i; \
		ed - $$i < $(OBJROOT)/fix-DARWIN.ed; \
	    done; \
	fi
	$(MAKE) -C $(OBJROOT) -f Makefile install SRCROOT=$(OBJROOT) \
		OBJROOT="$(OBJROOT)/$(PROJECT)"
	ed - $(DSTROOT)/System/Library/Perl/5.8.1/darwin-thread-multi-2level/Config.pm < $(OBJROOT)/pre-Config.pm.ed

.DEFAULT:
	@$(MAKE) -f Makefile $@
