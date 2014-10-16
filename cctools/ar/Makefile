ifneq "" "$(SDKROOT)"
  SDK = -isysroot $(SDKROOT)
  CC = $(shell xcrun -find -sdk $(SDKROOT) cc)
  DSYMUTIL = $(shell xcrun -find -sdk ${SDKROOT} dsymutil)
else
  DSYMUTIL = dsymutil
endif

OFLAG = -Os
CFLAGS = -g $(OFLAG) -Wall -I$(SRCROOT)/../include $(SDK)
LIBSTUFF = -L$(SYMROOT)/../libstuff -lstuff
MKDIRS = /bin/mkdir -p
SRCROOT = .
OBJROOT = .
SYMROOT = .
OFILE_DIR = $(OBJROOT)
VPATH = $(OFILE_DIR)

BINDIR = /usr/bin
MANDIR = /usr/share/man
DSTDIRS = $(DSTROOT)$(BINDIR) $(DSTROOT)$(MANDIR)/man1 $(DSTROOT)$(MANDIR)/man5

HFILES = archive.h extern.h pathnames.h
CFILES = append.c ar.c archive.c contents.c delete.c extract.c misc.c move.c \
	 print.c replace.c 
MAN1 = ar.1
MAN5 = ar.5
OBJS = $(CFILES:.c=.o)
INSTALL_FILES = $(CFILES) $(HFILES) $(MAN1) $(MAN5) Makefile notes

PRODUCT = ar

all: $(OFILE_DIR) $(SYMROOT) $(PRODUCT).NEW

$(PRODUCT).NEW:	$(OFILE_DIR) $(SYMROOT) $(OBJS)
	$(CC) $(RC_CFLAGS) $(SDK) -o $(SYMROOT)/$@ $(OBJS) $(LIBSTUFF)
	$(DSYMUTIL) $(SYMROOT)/$@

.c.o:
	$(CC) $(CFLAGS) $(RC_CFLAGS) $(COPTS) -c -o $(OFILE_DIR)/$*.o $<

clean: shlib_clean
	-rm -f $(SYMROOT)/$(PRODUCT).NEW
	-rm -r -f *dSYM

shlib_clean:
	-cd $(OFILE_DIR); rm -f $(OBJS)
	-rm -f make.out

install: all $(DSTDIRS)
	install -c -s -m 555 $(SYMROOT)/$(PRODUCT).NEW \
		$(DSTROOT)$(BINDIR)/$(PRODUCT)
	install -c -m 444 $(MAN1) $(DSTROOT)$(MANDIR)/man1
	install -c -m 444 $(MAN5) $(DSTROOT)$(MANDIR)/man5

installsrc:
	$(MKDIRS) $(SRCROOT)
	chmod 755 $(SRCROOT)
	gnutar cf - $(INSTALL_FILES) | (cd $(SRCROOT); gnutar xf -)
	chmod 444 $(SRCROOT)/*

$(OFILE_DIR) $(SYMROOT) $(DSTDIRS):
	$(MKDIRS) $@
