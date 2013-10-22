export USE_APPLE_PB_SUPPORT = all

ifeq "$(EFITOOLS)" "efitools"
	EFI_SUPPORT = -DEFI_SUPPORT
endif

ifneq "" "$(SDKROOT)"
  SDK = -isysroot $(SDKROOT)
  CC = $(shell xcrun -find -sdk $(SDKROOT) cc)
  CXX = $(shell xcrun -find -sdk $(SDKROOT) c++)
  DSYMUTIL = $(shell xcrun -find -sdk ${SDKROOT} dsymutil)
else
  DSYMUTIL = dsymutil
endif

OFLAG = -Os
LTO = -DLTO_SUPPORT
LEGACY = -D_MACH_I386_THREAD_STATUS_FPSTATE_LEGACY_FIELD_NAMES_ \
	 -D_ARCHITECTURE_I386_FPU_FPSTATE_LEGACY_FIELD_NAMES_
CFLAGS = $(OFLAG) $(LTO) -g -Wall -I$(SRCROOT)/../include \
	 -I$(SRCROOT) $(LEGACY) $(EFI_SUPPORT) $(SDK)

LIBSTUFF = -L$(SYMROOT)/../libstuff -lstuff
CXXLIB = -stdlib=libc++

MKDIRS = /bin/mkdir -p
SRCROOT = .
SYMROOT = .
OBJROOT = .
OFILE_DIR = $(OBJROOT)
VPATH = $(OFILE_DIR)

BINDIR = /bin
USRBINDIR = /usr/bin

CFILES = main.c ofile_print.c m68k_disasm.c i860_disasm.c \
	 m88k_disasm.c i386_disasm.c ppc_disasm.c hppa_disasm.c \
	 sparc_disasm.c print_objc.c notify.c arm_disasm.c \
	 print_objc2_64bit.c print_objc2_32bit.c
ifeq "$(EFITOOLS)" "efitools"
	CFILES += coff_print.c
endif
HFILES = otool.h ofile_print.h m68k_disasm.h i860_disasm.h \
	 sparc_disasm.h m88k_disasm.h i386_disasm.h ppc_disasm.h hppa_disasm.h \
	 notify.h arm_disasm.h
OBJS = $(CFILES:.c=.o) ofile.o
INSTALL_FILES = $(CFILES) $(HFILES) Makefile notes

PRODUCT = otool
all: $(OFILE_DIR) $(SYMROOT) $(PRODUCT).NEW

$(PRODUCT).NEW:	$(OFILE_DIR) $(SYMROOT) $(OBJS)
	$(CC) $(RC_CFLAGS) -nostdlib -r -o $(OBJROOT)/private.o \
		$(OBJS) $(LIBSTUFF)
	$(CXX) $(RC_CFLAGS) $(SDK) -o $(SYMROOT)/$@ $(OBJROOT)/private.o \
		$(LIBSTUFF) $(CXXLIB)
	$(DSYMUTIL) $(SYMROOT)/$@

.c.o:
	$(CC) $(CFLAGS) $(RC_CFLAGS) -c -o $(OFILE_DIR)/$*.o $<

ofile.o: $(SRCROOT)/../libstuff/ofile.c
	$(CC) $(CFLAGS) $(RC_CFLAGS) -c -o $(OFILE_DIR)/$*.o \
		$(SRCROOT)/../libstuff/ofile.c -DOTOOL

clean: shlib_clean
	-rm -f $(SYMROOT)/$(PRODUCT).NEW
	-rm -r -f *dSYM

shlib_clean:
	-cd $(OFILE_DIR); rm -f $(OBJS) private.o
	-rm -f make.out

install: all
	$(MKDIRS) $(DSTROOT)$(USRBINDIR)
	install -c -s -m 555 $(SYMROOT)/$(PRODUCT).NEW \
		$(DSTROOT)$(USRBINDIR)/$(PRODUCT)

installsrc:
	$(MKDIRS) $(SRCROOT)
	chmod 755 $(SRCROOT)
	gnutar cf - $(INSTALL_FILES) | (cd $(SRCROOT); gnutar xf -)
	chmod 444 $(SRCROOT)/*

$(OFILE_DIR) $(SYMROOT):
	$(MKDIRS) $@
