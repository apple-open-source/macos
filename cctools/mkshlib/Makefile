OFLAG = -O
ifeq "macos" "$(RC_OS)"
  X_CFLAGS := $(shell if [ "$(RC_RELEASE)" != "Beaker" ] &&    \
			 [ "$(RC_RELEASE)" != "Bunsen" ] &&    \
			 [ "$(RC_RELEASE)" != "Gonzo"  ] &&    \
			 [ "$(RC_RELEASE)" != "Kodiak" ]; then \
	     echo -Wno-long-double -no-cpp-precomp ; \
	     fi; )
endif

ifeq "mwccppc" "$(notdir $(CC))"
  CFLAGS = $(OFLAG) -g -I../include
else
  CFLAGS = $(OFLAG) -g -Wall -I../include -Wno-precomp $(X_CFLAGS)
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

OBJROOT = .
OFILE_DIR = $(OBJROOT)
VPATH = $(OFILE_DIR)
SYMROOT = .

BINDIR = /usr/local/bin
DSTDIRS = $(DSTROOT)$(BINDIR)

HFILES = mkshlib.h branch.h hack_libgcc.h libgcc.h
COMMON_CFILES = parse_spec.c
MKCFILES = mkshlib.c target.c host.c errors.c
CMPCFILES = cmpshlib.c
HACKFILES = hack_libgcc.c hack_spec.c
INSTALL_FILES = $(MKCFILES) $(CMPCFILES) $(HACKFILES) $(COMMON_CFILES) \
		$(HFILES) Makefile notes

MKOBJS = $(MKCFILES:.c=.o) $(COMMON_CFILES:.c=.o)
CMPOBJS = $(CMPCFILES:.c=.o) $(COMMON_CFILES:.c=.o)
HACKOBJS = $(HACKFILES:.c=.o) $(COMMON_CFILES:.c=.o)


all:	$(OFILE_DIR) $(SYMROOT) mkshlib.NEW cmpshlib.NEW hack_libgcc.NEW

vers.c:
	vers_string -c $(VERS_STRING_FLAGS) mkshlib > $(OFILE_DIR)/$@

ifeq "NO" "$(USE_DEPENDENCY_FILE)"
.c.o:
	$(CC) $(CFLAGS) $(RC_CFLAGS) -c -o $(OFILE_DIR)/$*.o $<
else
.c.o:
	$(CC) $(CFLAGS) $(RC_CFLAGS) -c -MD -dependency-file $(OFILE_DIR)/$*.d \
		-o $(OFILE_DIR)/$*.o $<
	md -d -u $(OFILE_DIR)/Makedep $(OFILE_DIR)/$*.d
endif

mkshlib.NEW: $(MKOBJS) vers.o
	$(CC) $(CFLAGS) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/mkshlib.private.o \
		$(MKOBJS) $(OFILE_DIR)/vers.o $(LIBSTUFF)
	$(CC) $(CFLAGS) $(RC_CFLAGS) -o $(SYMROOT)/mkshlib.NEW \
		$(OFILE_DIR)/mkshlib.private.o

cmpshlib.NEW:  $(CMPOBJS) vers.o
	$(CC) $(CFLAGS) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/cmpshlib.private.o \
		$(CMPOBJS) $(OFILE_DIR)/vers.o $(LIBSTUFF)
	$(CC) $(CFLAGS) $(RC_CFLAGS) -o $(SYMROOT)/cmpshlib.NEW \
		$(OFILE_DIR)/cmpshlib.private.o

hack_libgcc.NEW:  $(HACKOBJS) vers.o
	$(CC) $(CFLAGS) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/hack_libgcc.private.o \
		$(HACKOBJS) $(OFILE_DIR)/vers.o $(LIBSTUFF)
	$(CC) $(CFLAGS) $(RC_CFLAGS) -o $(SYMROOT)/hack_libgcc.NEW \
		$(OFILE_DIR)/hack_libgcc.private.o

clean: shlib_clean
	-rm -f \
	$(SYMROOT)/mkshlib.NEW \
	$(SYMROOT)/cmpshlib.NEW \
	$(SYMROOT)/hack_libgcc.NEW

shlib_clean:
	-cd $(OFILE_DIR); rm -f $(MKOBJS) $(CMPOBJS) $(HACKOBJS)
	-rm -f \
	$(OFILE_DIR)/mkshlib.private.o \
	$(OFILE_DIR)/cmpshlib.private.o \
	$(OFILE_DIR)/hack_libgcc.private.o \
	$(OFILE_DIR)/vers.c \
	$(OFILE_DIR)/vers.o
	-rm -f $(OFILE_DIR)/Makedep make.out

install: all $(DSTDIRS)
	install -c -s -m 555 $(SYMROOT)/mkshlib.NEW \
		$(DSTROOT)$(BINDIR)/mkshlib
	install -c -s -m 555 $(SYMROOT)/cmpshlib.NEW \
		$(DSTROOT)$(BINDIR)/cmpshlib
	install -c -s -m 555 $(SYMROOT)/hack_libgcc.NEW \
		$(DSTROOT)$(BINDIR)/hack_libgcc

installsrc:
	$(MKDIRS) $(SRCROOT)
	chmod 755 $(SRCROOT)
	gnutar cf - $(INSTALL_FILES) | (cd $(SRCROOT); gnutar xf -)
	chmod 444 $(SRCROOT)/*

$(OFILE_DIR) $(SYMROOT) $(DSTDIRS):
	$(MKDIRS) $@

-include $(OFILE_DIR)/Makedep
