export USE_APPLE_PB_SUPPORT = all
RC_OS = macos
ifneq "" "$(SDKROOT)"
  SDK = -isysroot $(SDKROOT)
  CC = $(shell xcrun -find -sdk $(SDKROOT) cc)
  DSYMUTIL = $(shell xcrun -find -sdk ${SDKROOT} dsymutil)
else
  DSYMUTIL = dsymutil
endif

OFLAG = -Os
CFLAGS = $(OFLAG) -g -Wall \
	 -I$(SRCROOT) -I$(SRCROOT)/../include -I$(OFILE_DIR) $(SDK)
LIBSTUFF = -L$(SYMROOT)/../libstuff -lstuff
MKDIRS = /bin/mkdir -p

SRCROOT = .
OBJROOT = .
OFILE_DIR = $(OBJROOT)
VPATH = $(OFILE_DIR)
SYMROOT = .

EFIBINDIR = /usr/local/efi/bin
LOCBINDIR = /usr/local/bin

CFILES = makerelocs.c mtoc.c

INSTALL_FILES = $(CFILES) Makefile notes

PROGS = makerelocs.NEW mtoc.NEW

all:	$(OFILE_DIR) $(SYMROOT) $(PROGS)

.c.o:
	$(CC) $(CFLAGS) $(RC_CFLAGS) -c -o $(OFILE_DIR)/$*.o $<

makerelocs.NEW: makerelocs.o
	$(CC) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/makerelocs.private.o \
		$(OFILE_DIR)/makerelocs.o $(LIBSTUFF)
	$(CC) $(RC_CFLAGS) $(SDK) -o $(SYMROOT)/makerelocs.NEW \
		$(OFILE_DIR)/makerelocs.private.o
	$(DSYMUTIL) $(SYMROOT)/makerelocs.NEW

mtoc.NEW: mtoc.o
	$(CC) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/mtoc.private.o \
		$(OFILE_DIR)/mtoc.o $(LIBSTUFF)
	$(CC) $(RC_CFLAGS) $(SDK) -o $(SYMROOT)/mtoc.NEW \
		$(OFILE_DIR)/mtoc.private.o
	$(DSYMUTIL) $(SYMROOT)/mtoc.NEW

clean: shlib_clean
	-rm -f \
	$(SYMROOT)/makerelocs.NEW \
	$(SYMROOT)/mtoc.NEW
	-rm -r -f *dSYM

shlib_clean:
	-rm -f \
	$(OFILE_DIR)/makerelocs.o \
	$(OFILE_DIR)/makerelocs.private.o \
	$(OFILE_DIR)/mtoc.o \
	$(OFILE_DIR)/mtoc.private.o
	-rm -f make.out

install: all install

install:
	$(MKDIRS) $(DSTROOT)$(EFIBINDIR) $(DSTROOT)$(LOCBINDIR)
# For now don't overwrite the existing makerelocs program the efitools project
# builds.
#	install -c -s -m 555 $(SYMROOT)/makerelocs.NEW \
#		$(DSTROOT)$(EFIBINDIR)/makerelocs
	install -c -s -m 555 $(SYMROOT)/mtoc.NEW \
		$(DSTROOT)$(EFIBINDIR)/mtoc
	install -c -s -m 555 $(SYMROOT)/mtoc.NEW \
		$(DSTROOT)$(LOCBINDIR)/mtoc

installsrc:
	$(MKDIRS) $(SRCROOT)
	chmod 755 $(SRCROOT)
	gnutar cf - $(INSTALL_FILES) | (cd $(SRCROOT); gnutar xf -)
	chmod 444 $(SRCROOT)/*

$(OFILE_DIR) $(SYMROOT):
	$(MKDIRS) $@
