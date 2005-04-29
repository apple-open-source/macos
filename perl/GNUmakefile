##---------------------------------------------------------------------
# GNUmakefile for perl
# Call Makefile to do the work, but for the install case, unpack the
# tarball to create the project source directory
##---------------------------------------------------------------------
PROJECT = perl
VERSION = 5.8.6
BASE = $(basename $(VERSION))
ARCHLIB = darwin-thread-multi-2level
FIX = $(SRCROOT)/fix
PROJVERS = $(PROJECT)-$(VERSION)
TARBALL = $(PROJVERS).tar.bz2
EXTRAS = $(DSTROOT)/System/Library/Perl/Extras
EXTRASPERL = $(EXTRAS)/$(VERSION)
LIBRARYPERL = $(DSTROOT)/Library/Perl/$(VERSION)
APPENDFILE = AppendToPath
PREPENDFILE = PrependToPath
PERL = $(DSTROOT)/System/Library/Perl
CORE = $(ARCHLIB)/CORE
LIBPERL = libperl.dylib
LIBBASE = lib/$(BASE)
LIBPERLLINK = ../../$(VERSION)/$(CORE)/$(LIBPERL)
SLP = $(PERL)/$(VERSION)
EXTRAOBJROOT=$(OBJROOT)/Extras-objroot
COMPATVERSIONS = 5.8.1

no_target: $(OBJROOT)/$(PROJECT) build

build:
	$(MAKE) -C $(OBJROOT) -f Makefile SRCROOT=$(OBJROOT) \
		OBJROOT="$(OBJROOT)/$(PROJECT)" _VERSION=$(VERSION) \
		PREPENDFILE=$(PREPENDFILE) APPENDFILE=$(APPENDFILE)

##---------------------------------------------------------------------
# buildextras expects perl to be installed in root and that the
# Carbon frameworks are available.  This won't work on pure darwin.
##---------------------------------------------------------------------
buildextras:
	$(MAKE) -C Extras EXTRAS=$(EXTRAS) EXTRASPERL=$(EXTRASPERL) \
	    SLP=$(SLP) ARCHLIB=$(ARCHLIB) OBJROOT=$(EXTRAOBJROOT)

install: $(OBJROOT)/$(PROJECT) installperl

installperl:
	$(MAKE) -C $(OBJROOT) -f Makefile install SRCROOT=$(OBJROOT) \
		OBJROOT="$(OBJROOT)/$(PROJECT)" _VERSION=$(VERSION) \
		PREPENDFILE=$(PREPENDFILE) APPENDFILE=$(APPENDFILE)
	install -d $(LIBRARYPERL)
	install -d $(EXTRASPERL)/$(ARCHLIB)
	echo '$(subst $(DSTROOT),,$(EXTRASPERL))' > $(LIBRARYPERL)/$(APPENDFILE)
	@for i in $(COMPATVERSIONS); do \
		echo echo /Library/Perl/$$i \>\> $(LIBRARYPERL)/$(APPENDFILE); \
		echo /Library/Perl/$$i >> $(LIBRARYPERL)/$(APPENDFILE); \
	done
	install -d $(PERL)/$(LIBBASE)
	ln -s $(LIBPERLLINK) $(PERL)/$(LIBBASE)/$(LIBPERL)
	install_name_tool -id /System/Library/Perl/$(LIBBASE)/$(LIBPERL) $(SLP)/$(CORE)/$(LIBPERL)

##---------------------------------------------------------------------
# installextras expects perl to be installed in root and that the
# Carbon frameworks are available.  This won't work on pure darwin.
##---------------------------------------------------------------------
installextras:
	$(MAKE) -C Extras install EXTRAS=$(EXTRAS) EXTRASPERL=$(EXTRASPERL) \
	    SLP=$(SLP) ARCHLIB=$(ARCHLIB) OBJROOT=$(EXTRAOBJROOT)

##---------------------------------------------------------------------
# We patch hints/darwin.sh to install in $(DSTROOT), and to force putting
# things in the right place.
##---------------------------------------------------------------------
$(OBJROOT)/$(PROJECT):
	cd $(SRCROOT) && pax -r -w . $(OBJROOT)
	echo cd $(OBJROOT)
	@cd $(OBJROOT); \
	echo bzcat $(TARBALL) \| gnutar xf -; \
	bzcat $(TARBALL) | gnutar xf -; \
	echo rm -rf $(PROJECT); \
	rm -rf $(PROJECT); \
	echo mv $(PROJVERS) $(PROJECT); \
	mv $(PROJVERS) $(PROJECT); \
	echo Patching hints/darwin.sh; \
	cat hints.append >> $(PROJECT)/hints/darwin.sh; \
	echo Patching perl.c; \
	chmod u+w $(PROJECT)/perl.c; \
	ed - $(PROJECT)/perl.c < perl.c.ed; \
	echo Patching Path.pm '(#23953)'; \
	chmod u+w $(PROJECT)/lib/File/Path.pm; \
	patch $(PROJECT)/lib/File/Path.pm perl-23953.patch; \
	chmod u-w $(PROJECT)/lib/File/Path.pm; \
	echo Patching perlio.c '(#33990)'; \
	chmod u+w $(PROJECT)/perlio.c; \
	patch $(PROJECT)/perlio.c perl-33990.patch; \
	echo Patching patchlevel.h; \
	chmod u+w $(PROJECT)/patchlevel.h; \
	ed - $(PROJECT)/patchlevel.h < patchlevel.h.ed; \
	chmod u-w $(PROJECT)/patchlevel.h

.DEFAULT:
	@$(MAKE) -f Makefile $@
