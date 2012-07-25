export USE_APPLE_PB_SUPPORT = all
OFLAG = -Os
RC_OS = macos
X_CFLAGS =
ifneq "" "$(SDKROOT)"
  SDK = -isysroot $(SDKROOT)
  CC = $(shell xcrun -find -sdk $(SDKROOT) cc)
endif

ifeq "clang" "$(notdir $(CC))"
  CFLAGS = $(OFLAG) -g -Wall \
	 -I$(SRCROOT) -I$(SRCROOT)/../include -I$(OFILE_DIR) $(SDK)
else
  CFLAGS = $(OFLAG) -g -Wall $(X_CFLAGS) \
	 -I$(SRCROOT) -I$(SRCROOT)/../include -I$(OFILE_DIR) $(SDK)
endif

USE_DEPENDENCY_FILE := $(shell if [ "$(notdir $(CC))" != "mwccppc" ] && \
				  [ "$(RC_XBS)" != "YES" ]; then \
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

EFIBINDIR = /usr/local/efi/bin

CFILES = makerelocs.c mtoc.c

INSTALL_FILES = $(CFILES) Makefile notes

PROGS = makerelocs.NEW mtoc.NEW

all:	$(OFILE_DIR) $(SYMROOT) $(PROGS)

ifeq "NO" "$(USE_DEPENDENCY_FILE)"
.c.o:
	$(CC) $(CFLAGS) $(RC_CFLAGS) -c -o $(OFILE_DIR)/$*.o $<
else
.c.o:
	$(CC) $(CFLAGS) $(RC_CFLAGS) -c -MD -dependency-file \
		$(OFILE_DIR)/$*.d -o $(OFILE_DIR)/$*.o $<
	md -d -u $(OFILE_DIR)/Makedep $(OFILE_DIR)/$*.d
endif

makerelocs.NEW: makerelocs.o
	$(CC) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/makerelocs.private.o \
		$(OFILE_DIR)/makerelocs.o $(LIBSTUFF)
	$(CC) $(RC_CFLAGS) $(SDK) -o $(SYMROOT)/makerelocs.NEW \
		$(OFILE_DIR)/makerelocs.private.o

mtoc.NEW: mtoc.o
	$(CC) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/mtoc.private.o \
		$(OFILE_DIR)/mtoc.o $(LIBSTUFF)
	$(CC) $(RC_CFLAGS) $(SDK) -o $(SYMROOT)/mtoc.NEW \
		$(OFILE_DIR)/mtoc.private.o

clean: shlib_clean
	-rm -f \
	$(SYMROOT)/makerelocs.NEW \
	$(SYMROOT)/mtoc.NEW

shlib_clean:
	-rm -f \
	$(OFILE_DIR)/makerelocs.o \
	$(OFILE_DIR)/makerelocs.private.o \
	$(OFILE_DIR)/mtoc.o \
	$(OFILE_DIR)/mtoc.private.o
	-rm -f $(OFILE_DIR)/Makedep make.out

install: all install

install:
	$(MKDIRS) $(DSTROOT)$(EFIBINDIR)
# For now don't overwrite the existing makerelocs program the efitools project
# builds.
#	install -c -s -m 555 $(SYMROOT)/makerelocs.NEW \
#		$(DSTROOT)$(EFIBINDIR)/makerelocs
	install -c -s -m 555 $(SYMROOT)/mtoc.NEW \
		$(DSTROOT)$(EFIBINDIR)/mtoc

installsrc:
	$(MKDIRS) $(SRCROOT)
	chmod 755 $(SRCROOT)
	gnutar cf - $(INSTALL_FILES) | (cd $(SRCROOT); gnutar xf -)
	chmod 444 $(SRCROOT)/*

$(OFILE_DIR) $(SYMROOT):
	$(MKDIRS) $@

-include $(OFILE_DIR)/Makedep
