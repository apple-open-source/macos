RC_OS = macos
OFLAG = -Os
ifneq "" "$(SDKROOT)"
  SDK = -isysroot $(SDKROOT)
  CC = $(shell xcrun -find -sdk $(SDKROOT) cc)
endif
	
CFLAGS = $(OFLAG) -g -I$(SRCROOT)/../include -Wall $(SDK)
MKDIRS = /bin/mkdir -p
SRCROOT = .
SYMROOT = .
OBJROOT = .
OFILE_DIR = $(OBJROOT)
VPATH = $(OFILE_DIR)

CFILES = libsyminfo.c
OBJS = $(CFILES:.c=.o)
INSTALL_FILES = $(CFILES) Makefile notes
LOCLIBDIR = /usr/local/lib
LIBSTUFF = -L$(SYMROOT)/../libstuff -lstuff

all: $(OFILE_DIR) $(SYMROOT) lib_ofiles

lib_ofiles: $(OFILE_DIR) $(SYMROOT) libsyminfo.a 

.c.o:
	$(CC) $(CFLAGS) $(RC_CFLAGS) $(COPTS) -c -o $(OFILE_DIR)/$*.o $<

clean shlib_clean:
	-rm -r -f $(OFILE_DIRS)
	-rm -f libsyminfo.a
	-rm -r -f *dSYM

install: all

installsrc:
	$(MKDIRS) $(SRCROOT)
	chmod 755 $(SRCROOT)
	gnutar cf - $(INSTALL_FILES) | (cd $(SRCROOT); gnutar xf -)
	chmod 444 $(SRCROOT)/*

$(SYMROOT):
	$(MKDIRS) $@

libsyminfo.o: libsyminfo.c
	$(CC) $(CFLAGS) $(RC_CFLAGS) -c \
		-o $(OFILE_DIR)/$*.o $(SRCROOT)/libsyminfo.c -DLIBRARY_API

libsyminfo.a: libsyminfo.o
	$(CC) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/libsyminfo.o.tmp \
		libsyminfo.o $(LIBSTUFF)
	libtool -static -o $(SYMROOT)/libsyminfo.a \
		$(OBJROOT)/libsyminfo.o.tmp


lib_ofiles_install: lib_ofiles
	$(MKDIRS) $(DSTROOT)$(LOCLIBDIR)
	install -p -c -m 444 $(SYMROOT)/libsyminfo.a \
		$(DSTROOT)$(LOCLIBDIR)/libsyminfo.a
	ranlib $(DSTROOT)$(LOCLIBDIR)/libsyminfo.a

$(OFILE_DIR) $(SYMROOT):
	$(MKDIRS) $@
