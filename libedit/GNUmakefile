##---------------------------------------------------------------------
# GNUmakefile for libedit
# Call Makefile to do the work, but for the install and installhdrs cases,
# unpack the tarball, patch files, run autoreconf and then call the Makefile
##---------------------------------------------------------------------
Project = libedit
SRCDIR = $(OBJROOT)/SRCDIR

no_target:
	@$(MAKE) -f Makefile

# Automatic Extract & Patch
AEP_Project    = $(Project)
AEP_Version    = 2.11
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
#AEP_Filename   = $(AEP_ProjVers).tar.gz
#AEP_ExtractDir = $(AEP_ProjVers)
AEP_Filename   = $(AEP_ExtractDir).tar.gz
AEP_ExtractDir = libedit-20080712-2.11
AEP_Patches    = patch-configure.ac \
                 patch-el_push \
		 patch-histedit.h \
		 patch-history.c \
		 patch-readline.c \
		 patch-readline.h

ifeq ($(suffix $(AEP_Filename)),.bz2)
AEP_ExtractOption = j
else
AEP_ExtractOption = z
endif

$(SRCDIR):
	mkdir -p $(SRCDIR)
	tar -C $(SRCDIR) -$(AEP_ExtractOption)xof $(SRCROOT)/$(AEP_Filename)
	rm -rf $(SRCDIR)/$(Project)
	mv $(SRCDIR)/$(AEP_ExtractDir) $(SRCDIR)/$(Project)
	@set -x && \
	cd $(SRCDIR)/$(Project) && \
	for patchfile in $(AEP_Patches); do \
		patch -p0 -i $(SRCROOT)/patches/$$patchfile || exit 1; \
	done && \
	autoreconf -fvi && \
	rm -rf autom4te.cache

install: $(SRCDIR)
	$(MAKE) -f Makefile install SRCROOT=$(SRCDIR) \
	AEP_Version=$(AEP_Version) Project=$(Project)

installhdrs: $(SRCDIR)
	$(MAKE) -f Makefile installhdrs SRCROOT=$(SRCDIR) \
	AEP_Version=$(AEP_Version) Project=$(Project)

.DEFAULT:
	@$(MAKE) -f Makefile $@
