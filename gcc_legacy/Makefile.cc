#
# Copyright (C) 1994-1996 NeXT Software Inc.
# All Rights Reserved
#

#
# These ROOTs will get overridden by RC
#
OBJROOT = `pwd`
DSTROOT =
SRCROOT = `pwd`

PREFIX  = /Developer
OBJDIR  = $(OBJROOT)/$(ARCH)_obj/cc
SRCDIR  = $(SRCROOT)/cc
DSTDIR  = $(DSTROOT)$(PREFIX)

VERSION := $(shell sed -e 's/.*\"\([^ \"]*\)[ \"].*/\1/' <$(SRCDIR)/version.c)
# It is no longer necessary to know what the installed version is.
# INSTALLED_VERSION := $(shell gcc -v 2>&1 | fgrep version | sed -e 's/[^0-9]*\([^ ]*\) .*/\1/')

LOCALDIR        = $(DSTROOT)/Local
LOCALBINDIR     = $(LOCALDIR)/Executables
GCCBINDIR       = $(DSTROOT)$(PREFIX)/Executables
GCCLIBDIR       = $(DSTROOT)$(PREFIX)/Libraries
GCCEXECPREFIX   = $(GCCLIBDIR)/gcc-lib/$(ARCH)
GCCEXECDIR      = $(GCCEXECPREFIX)/$(VERSION)
GCCINCLDIR      = $(GCCEXECPREFIX)/ginclude

ARCH = `$NEXT_ROOT/Developer/Executables/arch`

all install:: $(OBJDIR) $(EXECDIR) $(EXECDIR)/bin
	@echo ==== Building GCC for $(ARCH) ====
	cd $(OBJDIR) ;\
	$(SRCDIR)/configure --exec-prefix=$(PREFIX) \
			    --prefix=$(LOCALDIR) \
			    --gas $(ARCH) ;\
	make -w CC=gcc \
		CFLAGS=-O2 \
		libdir=$(PREFIX)/Libraries \
		BISON=$(LOCALBINDIR)/bison \
		BISON_SIMPLE=$(LOCALDIR)/Libraries/bison.simple \
		LANGUAGES="objective-c c++" ;

install:: $(GCCBINDIR) $(GCCLIBDIR) $(GCCEXECDIR) $(GCCINCLDIR) $(LOCALBINDIR)
	echo ==== Installing GCC ==== ;\
	cd $(OBJDIR) ;\
	echo ==== Installing Binaries ==== ;\
	make -w CC=gcc \
		GCC_INSTALL_NAME=gcc \
		bindir=$(GCCBINDIR) \
		libdir=$(GCCLIBDIR) \
		tooldir=$(GCCEXECDIR) \
		CFLAGS=-O \
		BISON=$(LOCALBINDIR)/bison \
		BISON_SIMPLE=$(LOCALDIR)/Libraries/bison.simple \
		LANGUAGES="objective-c objective-c++ c++" \
		install
	-rm -f  $(GCCBINDIR)/$(ARCH)-gcc
	-rm -f  $(GCCEXECDIR)/cc1
	-mkdirs $(GCCINCLDIR)
	-cp -p  $(OBJDIR)/include/float.h     $(GCCINCLDIR)
	-cp -p  $(OBJDIR)/include/limits.h    $(GCCINCLDIR)
	-cp -p  $(OBJDIR)/include/syslimits.h $(GCCINCLDIR)
	rm -rf  $(GCCEXECDIR)/include/*
	cp -rp  $(SRCDIR)/ginclude/*     $(GCCINCLDIR)
	cp -p   $(SRCDIR)/fixincludes    $(GCCINCLDIR)
	cp -p   $(SRCDIR)/fixinc.svr4    $(GCCINCLDIR)
	-cp -p  $(SRCDIR)/README-fixinc  $(GCCINCLDIR)
	-if [ ! -f $(LOCALBINDIR)/gcc ]; then \
		(cd $(LOCALBINDIR); ln -s ../../Developer/Executables/gcc .); \
	fi

	
$(OBJDIR) $(DSTDIR) $(GCCBINDIR) $(GCCLIBDIR) $(GCCEXECDIR) $(GCCINCLDIR) $(LOCALBINDIR):
	if [ ! -d $@ ]; then \
		mkdirs $@; \
	fi
