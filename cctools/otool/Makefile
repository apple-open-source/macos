RC_OS = macos
export USE_APPLE_PB_SUPPORT = all
OFLAG = -O
ifeq "macos" "$(RC_OS)"
  X_CFLAGS := $(shell if [ "$(RC_RELEASE)" != "Beaker" ] &&    \
			 [ "$(RC_RELEASE)" != "Bunsen" ] &&    \
			 [ "$(RC_RELEASE)" != "Gonzo"  ] &&    \
			 [ "$(RC_RELEASE)" != "Kodiak" ]; then \
	     echo -Wno-long-double -no-cpp-precomp ; \
	     fi; )
endif

STATIC = -static
ifeq "mwccppc" "$(notdir $(CC))"
  CFLAGS = $(OFLAG) -g -I../include -I$(SRCROOT)
else
  CFLAGS = $(STATIC) $(OFLAG) -g -Wall -Wno-precomp -I../include -I$(SRCROOT) \
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
  LIBS = -L$(NEXT_ROOT)$(SYSTEMDIR) -lc_static
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
OFILE_DIR = $(OBJROOT)
VPATH = $(OFILE_DIR)

BINDIR = /bin
USRBINDIR = /usr/bin

CFILES = main.c ofile_print.c m68k_disasm.c i860_disasm.c \
	 m88k_disasm.c i386_disasm.c ppc_disasm.c hppa_disasm.c \
	 sparc_disasm.c print_objc.c
HFILES = otool.h ofile_print.h m68k_disasm.h i860_disasm.h \
	 sparc_disasm.h m88k_disasm.h i386_disasm.h ppc_disasm.h hppa_disasm.h
OBJS = $(CFILES:.c=.o) ofile.o vers.o
INSTALL_FILES = $(CFILES) $(HFILES) Makefile notes

PRODUCT = otool
all: $(OFILE_DIR) $(SYMROOT) $(PRODUCT).NEW

$(PRODUCT).NEW:	$(OFILE_DIR) $(SYMROOT) $(OBJS)
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

clean: shlib_clean
	-rm -f $(SYMROOT)/$(PRODUCT).NEW $(OFILE_DIR)/vers.c

shlib_clean:
	-cd $(OFILE_DIR); rm -f $(OBJS)
	-rm -f $(OFILE_DIR)/Makedep make.out

install: all $(RC_OS)_install

teflon_install macos_install:
	$(MKDIRS) $(DSTROOT)$(USRBINDIR)
	install -c -s -m 555 $(SYMROOT)/$(PRODUCT).NEW \
		$(DSTROOT)$(USRBINDIR)/$(PRODUCT)

nextstep_install:
	$(MKDIRS) $(DSTROOT)$(BINDIR)
	install -c -s -m 555 $(SYMROOT)/$(PRODUCT).NEW \
		$(DSTROOT)$(BINDIR)/$(PRODUCT)

installsrc:
	$(MKDIRS) $(SRCROOT)
	chmod 755 $(SRCROOT)
	gnutar cf - $(INSTALL_FILES) | (cd $(SRCROOT); gnutar xf -)
	chmod 444 $(SRCROOT)/*

$(OFILE_DIR) $(SYMROOT):
	$(MKDIRS) $@

-include $(OFILE_DIR)/Makedep
