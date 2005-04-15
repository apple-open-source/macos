RC_OS = macos
export USE_APPLE_PB_SUPPORT = all
OFLAG = -O
ifeq "macos" "$(RC_OS)"
  X_CFLAGS := $(shell if [ "$(RC_RELEASE)" != "Beaker" ] &&    \
			 [ "$(RC_RELEASE)" != "Bunsen" ] &&    \
			 [ "$(RC_RELEASE)" != "Gonzo"  ] &&    \
			 [ "$(RC_RELEASE)" != "Kodiak" ]; then \
	     echo -Wno-long-double -no-cpp-precomp; \
	     fi; )
endif

STATIC = -static
ifeq "mwccppc" "$(notdir $(CC))"
  CFLAGS = $(OFLAG) -g -I$(SRCROOT)/../include -I$(SRCROOT)
else
  CFLAGS = $(STATIC) $(OFLAG) -g -Wall -I$(SRCROOT)/../include -I$(SRCROOT) \
	   $(X_CFLAGS)
endif

USE_DEPENDENCY_FILE := $(shell if [ "$(notdir $(CC))" != "mwccppc" ]; then \
	     echo YES ; else echo NO ; \
	     fi; )

LIBSTUFF = -L$(SYMROOT)/../libstuff -lstuff

ifeq "macos" "$(RC_OS)"
  SYSTEMDIR := $(shell if [ "$(RC_RELEASE)" = "Beaker" ] || \
			  [ "$(RC_RELEASE)" = "Bunsen" ] || \
			  [ "$(RC_RELEASE)" = "Gonzo" ]; then \
	 echo /Local/Developer/System; else echo /usr/local/lib/system; fi; )
else
  SYSTEMDIR = /Local/Developer/System
endif
ifeq "macos" "$(RC_OS)"
  LIBS := $(shell if [ "$(RC_RELEASE)" = "Beaker"    ] || \
		     [ "$(RC_RELEASE)" = "Bunsen"    ] || \
		     [ "$(RC_RELEASE)" = "Gonzo"     ] || \
		     [ "$(RC_RELEASE)" = "Kodiak"    ] || \
		     [ "$(RC_RELEASE)" = "Cheetah"   ] || \
		     [ "$(RC_RELEASE)" = "Puma"      ] || \
		     [ "$(RC_RELEASE)" = "Jaguar"    ] || \
		     [ "$(RC_RELEASE)" = "Panther"   ] || \
		     [ "$(RC_RELEASE)" = "MuonPrime" ] || \
		     [ "$(RC_RELEASE)" = "MuonSeed"  ] || \
		     [ "$(RC_RELEASE)" = "SUPanWheat" ]; then \
		    echo "-L$(NEXT_ROOT)$(SYSTEMDIR) -lc_static" ; \
	    else \
		    echo "-L$(NEXT_ROOT)$(SYSTEMDIR) -lc_static -lm_static" \
	  ; fi; )
else
  LIBS = -lc_static
endif

ifneq "" "$(wildcard /bin/mkdirs)"
  MKDIRS = /bin/mkdirs
else
  MKDIRS = /bin/mkdir -p
endif

SRCROOT = .
SYMROOT = .
OBJROOT = .
OFILE_DIRS = $(OBJROOT)/otool_dir $(OBJROOT)/otool64_dir

BINDIR = /bin
USRBINDIR = /usr/bin

CFILES = main.c ofile_print.c m68k_disasm.c i860_disasm.c \
	 m88k_disasm.c i386_disasm.c ppc_disasm.c hppa_disasm.c \
	 sparc_disasm.c print_objc.c notify.c
HFILES = otool.h ofile_print.h m68k_disasm.h i860_disasm.h \
	 sparc_disasm.h m88k_disasm.h i386_disasm.h ppc_disasm.h hppa_disasm.h \
	 notify.h
OBJS = $(CFILES:.c=.o) ofile.o vers.o
INSTALL_FILES = $(CFILES) $(HFILES) Makefile notes

all:	$(OFILE_DIRS) $(SYMROOT) otool_build otool64_build

otool_build:	$(OBJROOT)/otool_dir $(SYMROOT)/otool_dir  
	@if [ $(SRCROOT) = . ];						\
	then								\
	    cd otool_dir; $(MAKE) -f ../Makefile otool.NEW PRODUCT="otool" \
		VPATH=.. OFILE_DIR=. SRCROOT=.. RC_CFLAGS="$(RC_CFLAGS)" \
		LIBSTUFF="-L../../libstuff -lstuff"			\
		VERS_STRING_FLAGS="$(VERS_STRING_FLAGS)"		\
		MAKEINCDIR="`pwd`" CC="$(CC)";				\
	else								\
	    cd $(OBJROOT)/otool_dir; $(MAKE) -f $(SRCROOT)/Makefile	\
		otool.NEW PRODUCT="otool" VPATH=$(SRCROOT) 		\
		OFILE_DIR=$(OBJROOT)/otool_dir				\
		SYMROOT=$(SYMROOT)/otool_dir SRCROOT=$(SRCROOT)		\
		MAKEINCDIR="`pwd`" 					\
		LIBSTUFF="-L$(SYMROOT)/../libstuff -lstuff"		\
		VERS_STRING_FLAGS="$(VERS_STRING_FLAGS)"		\
		RC_CFLAGS="$(RC_CFLAGS)" CC="$(CC)";			\
	fi

otool64_build:	$(OBJROOT)/otool64_dir $(SYMROOT)/otool64_dir
	@if [ $(SRCROOT) = . ];						\
	then								\
	    cd otool64_dir; $(MAKE) -f ../Makefile otool64.NEW 		\
		PRODUCT="otool64" VPATH=.. OFILE_DIR=. SRCROOT=.. 	\
		RC_CFLAGS="$(RC_CFLAGS) -DARCH64" \
		LIBSTUFF="-L../../libstuff -lstuff64"			\
		VERS_STRING_FLAGS="$(VERS_STRING_FLAGS)"		\
		MAKEINCDIR="`pwd`" CC="$(CC)" PRODUCT="otool64";	\
	else								\
	    cd $(OBJROOT)/otool64_dir; $(MAKE) -f $(SRCROOT)/Makefile	\
		otool64.NEW PRODUCT="otool64" VPATH=$(SRCROOT) 		\
		OFILE_DIR=$(OBJROOT)/otool64_dir			\
		SYMROOT=$(SYMROOT)/otool64_dir SRCROOT=$(SRCROOT)	\
		MAKEINCDIR="`pwd`" 					\
		LIBSTUFF="-L$(SYMROOT)/../libstuff -lstuff64"		\
		VERS_STRING_FLAGS="$(VERS_STRING_FLAGS)"		\
		RC_CFLAGS="$(RC_CFLAGS) -DARCH64" CC="$(CC)";		\
	fi

$(PRODUCT).NEW:	$(OBJS)
ifeq "mwccppc" "$(notdir $(CC))"
	$(CC) $(CFLAGS) $(RC_CFLAGS) -o $(SYMROOT)/$@ $(OBJS) $(LIBSTUFF)
else
	$(CC) $(CFLAGS) $(RC_CFLAGS) -o $(SYMROOT)/$@ $(OBJS) \
		$(LIBSTUFF) $(LIBS)
endif

vers.c:
	vers_string -c $(VERS_STRING_FLAGS) $(PRODUCT) > $(OFILE_DIR)/$@

ifeq "NO" "$(USE_DEPENDENCY_FILE)"
.c.o:
	$(CC) $(CFLAGS) $(RC_CFLAGS) -c -o $(OFILE_DIR)/$*.o $<
else
.c.o:
	$(CC) $(CFLAGS) $(RC_CFLAGS) -c -MD -dependency-file $(OFILE_DIR)/$*.d \
		-o $(OFILE_DIR)/$*.o $<
	md -d -u $(OFILE_DIR)/Makedep $(OFILE_DIR)/$*.d
endif

print_objc.o: print_objc.c
ifeq "NO" "$(USE_DEPENDENCY_FILE)"
	$(CC) $(CFLAGS) $(RC_CFLAGS) -c -o $(OFILE_DIR)/$*.o $< -ObjC
else
	$(CC) $(CFLAGS) $(RC_CFLAGS) -c -MD -dependency-file $(OFILE_DIR)/$*.d \
		-o $(OFILE_DIR)/$*.o $< -ObjC
	md -d -u $(OFILE_DIR)/Makedep $(OFILE_DIR)/$*.d
endif

ofile.o: $(SRCROOT)/../libstuff/ofile.c
ifeq "NO" "$(USE_DEPENDENCY_FILE)"
	$(CC) $(CFLAGS) $(RC_CFLAGS) -c -o $(OFILE_DIR)/$*.o \
		$(SRCROOT)/../libstuff/ofile.c -DOTOOL
else
	$(CC) $(CFLAGS) $(RC_CFLAGS) -c -MD -dependency-file $(OFILE_DIR)/$*.d \
		-o $(OFILE_DIR)/$*.o $(SRCROOT)/../libstuff/ofile.c -DOTOOL
	md -d -u $(OFILE_DIR)/Makedep $(OFILE_DIR)/$*.d
endif

clean shlib_clean:
	-rm -r -f $(OFILE_DIRS)

install: all $(RC_OS)_install

teflon_install macos_install:
	$(MKDIRS) $(DSTROOT)$(USRBINDIR)
	install -c -s -m 555 $(SYMROOT)/otool_dir/otool.NEW \
		$(DSTROOT)$(USRBINDIR)/otool
	install -c -s -m 555 $(SYMROOT)/otool64_dir/otool64.NEW \
		$(DSTROOT)$(USRBINDIR)/otool64

nextstep_install:
	$(MKDIRS) $(DSTROOT)$(BINDIR)
	install -c -s -m 555 $(SYMROOT)/otool_dir/otool.NEW \
		$(DSTROOT)$(BINDIR)/otool
	install -c -s -m 555 $(SYMROOT)/otool64_dir/otool64.NEW \
		$(DSTROOT)$(BINDIR)/otool64

installsrc:
	$(MKDIRS) $(SRCROOT)
	chmod 755 $(SRCROOT)
	gnutar cf - $(INSTALL_FILES) | (cd $(SRCROOT); gnutar xf -)
	chmod 444 $(SRCROOT)/*

$(OFILE_DIRS) $(SYMROOT) $(SYMROOT)/otool_dir $(SYMROOT)/otool64_dir: 
	$(MKDIRS) $@

-include $(OFILE_DIR)/Makedep
