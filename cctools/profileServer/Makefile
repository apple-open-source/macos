OFLAG = -O

ifeq "mwccppc" "$(notdir $(CC))"
  CFLAGS = $(OFLAG) -g \
	 -I$(SRCROOT) -I$(SRCROOT)/../include -I$(OFILE_DIR)
else
  CFLAGS = $(OFLAG) -g -Wall -Wno-precomp \
	 -I$(SRCROOT) -I$(SRCROOT)/../include -I$(OFILE_DIR)
endif

USE_DEPENDENCY_FILE := $(shell if [ "$(notdir $(CC))" != "mwccppc" ]; then \
	     echo YES ; else echo NO ; \
	     fi; )

LIBSTUFF = -L$(SYMROOT)/../libstuff -lstuff

ifneq "" "$(wildcard /bin/mkdirs)"
  MKDIRS = /bin/mkdirs
else
  MKDIRS = /bin/mkdir -p
endif

SRCROOT = .
OBJROOT = .
OFILE_DIR = $(OBJROOT)
VPATH = $(OFILE_DIR)
SYMROOT = .

BINDIR = /usr/bin
ETCDIR = /usr/etc
USRLIBDIR = /usr/libexec

CFILES = dylibprof.c profileServer.c profile_client.c
HFILES = profileServer.h
INSTALL_FILES = $(CFILES) $(HFILES) Makefile notes

all: $(RC_OS)_all

teflon_all nextstep_all: $(OFILE_DIR) $(SYMROOT) dylibprof.NEW profileServer.NEW

macos_all:
	@echo "Warning: profileServer not ported yet to Mach 3.0 (not built)"

vers.c:
	vers_string -c $(VERS_STRING_FLAGS) cctools_profileServer \
		> $(OFILE_DIR)/$@

ifeq "NO" "$(USE_DEPENDENCY_FILE)"
.c.o:
	$(CC) $(CFLAGS) $(RC_CFLAGS) -c -o $(OFILE_DIR)/$*.o $<
else
.c.o:
	$(CC) $(CFLAGS) $(RC_CFLAGS) -c -MD -dependency-file \
		$(OFILE_DIR)/$*.d -o $(OFILE_DIR)/$*.o $<
	md -d -u $(OFILE_DIR)/Makedep $(OFILE_DIR)/$*.d
endif

dylibprof.NEW: dylibprof.o vers.o
	$(CC) $(CFLAGS) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/dylibprof.private.o \
		$(OFILE_DIR)/dylibprof.o $(OFILE_DIR)/vers.o $(LIBSTUFF)
	$(CC) $(CFLAGS) $(RC_CFLAGS) -o $(SYMROOT)/dylibprof.NEW \
		$(OFILE_DIR)/dylibprof.private.o

profileServer.NEW: profileServer.o vers.o
	$(CC) $(CFLAGS) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/profileServer.private.o \
		$(OFILE_DIR)/profileServer.o $(OFILE_DIR)/vers.o $(LIBSTUFF)
	$(CC) $(CFLAGS) $(RC_CFLAGS) -o $(SYMROOT)/profileServer.NEW \
		$(OFILE_DIR)/profileServer.private.o

clean: shlib_clean
	-rm -f \
	$(SYMROOT)/dylibprof.NEW \
	$(SYMROOT)/profileServer.NEW

shlib_clean:
	-rm -f \
	$(OFILE_DIR)/vers.c \
	$(OFILE_DIR)/vers.o \
	$(OFILE_DIR)/dylibprof.o \
	$(OFILE_DIR)/dylibprof.private.o \
	$(OFILE_DIR)/profileServer.o \
	$(OFILE_DIR)/profileServer.private.o
	-rm -f $(OFILE_DIR)/Makedep make.out

install: $(RC_OS)

teflon nextstep: all $(RC_OS)_install
	$(MKDIRS) $(DSTROOT)$(BINDIR)
	install -c -s -m 555 $(SYMROOT)/dylibprof.NEW \
		$(DSTROOT)$(BINDIR)/dylibprof

macos:

teflon_install macos_install:
	$(MKDIRS) $(DSTROOT)$(USRLIBDIR)
	install -c -s -m 555 $(SYMROOT)/profileServer.NEW \
			  $(DSTROOT)$(USRLIBDIR)/profileServer

nextstep_install:
	$(MKDIRS) $(DSTROOT)$(ETCDIR)
	install -c -s -m 555 $(SYMROOT)/profileServer.NEW \
			  $(DSTROOT)$(ETCDIR)/profileServer

installsrc:
	$(MKDIRS) $(SRCROOT)
	chmod 755 $(SRCROOT)
	gnutar cf - $(INSTALL_FILES) | (cd $(SRCROOT); gnutar xf -)
	chmod 444 $(SRCROOT)/*

$(OFILE_DIR) $(SYMROOT):
	$(MKDIRS) $@

-include $(OFILE_DIR)/Makedep
