RC_OS = macos
OFLAG = -Os
LIBSTUFF = -L$(SYMROOT)/../libstuff -lstuff

ifneq "clang" "$(notdir $(CC))"
  GCC_FLAGS = $(X_CFLAGS)
endif

USE_DEPENDENCY_FILE := $(shell if [ "$(notdir $(CC))" != "clang" ] && \
				  [ "$(RC_XBS)" != "YES" ]; then \
	     echo YES ; else echo NO ; \
	     fi; )

ifneq "" "$(SDKROOT)"
  SDK = -isysroot $(SDKROOT)
  CC = $(shell xcrun -find -sdk $(SDKROOT) cc)
endif
CFLAGS = $(OFLAG) -g -I../include -Wall $(GCC_FLAGS) -DNeXT_MOD -DDEBUG $(SDK)

ifneq "" "$(wildcard /bin/mkdirs)"
  MKDIRS = /bin/mkdirs
else
  MKDIRS = /bin/mkdir -p
endif

OBJROOT = .
OFILE_DIR = $(OBJROOT)
VPATH = $(OFILE_DIR)
SYMROOT = .

BINDIR = /usr/ucb
USRBINDIR = /usr/bin
nextstep_LIBDIR = /usr/lib
teflon_LIBDIR   = /usr/share
macos_LIBDIR    = /usr/share

HFILES = gprof.h m68k.h vax.h
CFILES = gprof.c arcs.c dfn.c lookup.c calls.c hertz.c printgprof.c \
	 printlist.c getnfile.c scatter.c
OBJS = $(CFILES:.c=.o) vers.o
INSTALL_FILES = $(CFILES) $(HFILES) gprof.flat gprof.callg Makefile notes

PRODUCT = gprof
all: $(OFILE_DIR) $(SYMROOT) $(PRODUCT).NEW

$(PRODUCT).NEW:	$(OFILE_DIR) $(SYMROOT) $(OBJS)
	$(CC) $(RC_CFLAGS) -nostdlib -r \
		-o $(SYMROOT)/$@.private.o $(OBJS) $(LIBSTUFF)
	$(CC) $(RC_CFLAGS) $(SDK) -o $(SYMROOT)/$@ $(SYMROOT)/$@.private.o

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

clean: shlib_clean
	-rm -f $(SYMROOT)/$(PRODUCT).NEW $(OFILE_DIR)/vers.c
	-rm -f $(SYMROOT)/$(PRODUCT).NEW.private.o

shlib_clean:
	-cd $(OFILE_DIR); rm -f $(OBJS)
	-rm -f $(OFILE_DIR)/Makedep make.out

install: all common_install $(RC_OS)_install

teflon_install macos_install:
	$(MKDIRS) $(DSTROOT)$(USRBINDIR)
	install -c -s -m 555 $(SYMROOT)/$(PRODUCT).NEW \
		$(DSTROOT)$(USRBINDIR)/$(PRODUCT)

nextstep_install:
	$(MKDIRS) $(DSTROOT)$(BINDIR)
	install -c -s -m 555 $(SYMROOT)/$(PRODUCT).NEW \
		$(DSTROOT)$(BINDIR)/$(PRODUCT)

common_install:
	$(MKDIRS) $(DSTROOT)$($(RC_OS)_LIBDIR)
	install -c -m 444 gprof.flat $(DSTROOT)$($(RC_OS)_LIBDIR)
	install -c -m 444 gprof.callg $(DSTROOT)$($(RC_OS)_LIBDIR)

installsrc:
	$(MKDIRS) $(SRCROOT)
	chmod 755 $(SRCROOT)
	gnutar cf - $(INSTALL_FILES) | (cd $(SRCROOT); gnutar xf -)
	chmod 444 $(SRCROOT)/*

$(OFILE_DIR) $(SYMROOT):
	$(MKDIRS) $@

-include $(OFILE_DIR)/Makedep
