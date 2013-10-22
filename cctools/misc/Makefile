export USE_APPLE_PB_SUPPORT = all

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
TRIE = -DTRIE_SUPPORT
CFLAGS = $(OFLAG) $(LTO) $(TRIE) -g -Wall \
	 -I$(SRCROOT) -I$(SRCROOT)/../include -I$(OFILE_DIR) $(SDK) \
	 -I$(DT_TOOLCHAIN_DIR)/usr/local/include

LIBSTUFF = -L$(SYMROOT)/../libstuff -lstuff
CXXLIB = -stdlib=libc++

ifeq "" "$(TRIE)"
  LIB_PRUNETRIE =
  else
  LIB_PRUNETRIE = -L$(DT_TOOLCHAIN_DIR)/usr/local/lib -lprunetrie
endif

MKDIRS = /bin/mkdir -p
SRCROOT = .
OBJROOT = .
OFILE_DIR = $(OBJROOT)
VPATH = $(OFILE_DIR)
SYMROOT = .

BINDIR = /bin
USRBINDIR = /usr/bin
LOCBINDIR = /usr/local/bin
LOCLIBDIR = /usr/local/lib

CFILES1 = libtool.c
CFILES2 = lipo.c size.c strings.c nm.c checksyms.c inout.c \
	 indr.c strip.c segedit.c cmpdylib.c \
	 pagestuff.c redo_prebinding.c seg_addr_table.c \
	 check_dylib.c seg_hack.c install_name_tool.c \
	 codesign_allocate.c ctf_insert.c
INSTALL_FILES = $(CFILES1) $(CFILES2) Makefile notes Info.plist

OS_PROGS = lipo.NEW strings.NEW

PROGS = $(OS_PROGS) size.NEW nm.NEW libtool.NEW checksyms.NEW \
	indr.NEW strip.NEW nmedit.NEW segedit.NEW cmpdylib.NEW \
	pagestuff.NEW redo_prebinding.NEW seg_addr_table.NEW check_dylib.NEW \
	seg_hack.NEW install_name_tool.NEW codesign_allocate.NEW ctf_insert.NEW

all:	$(OFILE_DIR) $(SYMROOT) $(PROGS) lib_ofiles

os_tools: $(OFILE_DIR) $(SYMROOT) $(OS_PROGS)

lib_ofiles: $(OFILE_DIR) $(SYMROOT) libredo_prebinding.a

.c.o:
	$(CC) $(CFLAGS) $(RC_CFLAGS) -c -o $(OFILE_DIR)/$*.o $<

libredo_prebinding.o: redo_prebinding.c
	$(CC) $(CFLAGS) $(RC_CFLAGS) -c \
		-o $(OFILE_DIR)/$*.o $(SRCROOT)/redo_prebinding.c -DLIBRARY_API

checksyms.NEW: checksyms.o
	$(CC) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/checksyms.private.o \
		$(OFILE_DIR)/checksyms.o $(LIBSTUFF)
	$(CC) $(RC_CFLAGS) $(SDK) -o $(SYMROOT)/checksyms.NEW \
		$(OFILE_DIR)/checksyms.private.o
	$(DSYMUTIL) $(SYMROOT)/checksyms.NEW

cmpdylib.NEW: cmpdylib.o
	$(CC) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/cmpdylib.private.o \
		$(OFILE_DIR)/cmpdylib.o $(LIBSTUFF)
	$(CC) $(RC_CFLAGS) $(SDK) -o $(SYMROOT)/cmpdylib.NEW \
		$(OFILE_DIR)/cmpdylib.private.o
	$(DSYMUTIL) $(SYMROOT)/cmpdylib.NEW

lipo.NEW: lipo.o
	$(CC) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/lipo.private.o \
		$(OFILE_DIR)/lipo.o $(LIBSTUFF)
	$(CC) $(RC_CFLAGS) $(SDK) -o $(SYMROOT)/lipo.NEW \
		$(OFILE_DIR)/lipo.private.o
	$(DSYMUTIL) $(SYMROOT)/lipo.NEW

size.NEW: size.o
	$(CC) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/size.private.o \
		$(OFILE_DIR)/size.o $(LIBSTUFF)
	$(CC) $(RC_CFLAGS) $(SDK) -o $(SYMROOT)/size.NEW \
		$(OFILE_DIR)/size.private.o
	$(DSYMUTIL) $(SYMROOT)/size.NEW

strings.NEW: strings.o
	$(CC) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/strings.private.o \
		$(OFILE_DIR)/strings.o $(LIBSTUFF)
	$(CC) $(RC_CFLAGS) $(SDK) -o $(SYMROOT)/strings.NEW \
		$(OFILE_DIR)/strings.private.o
	$(DSYMUTIL) $(SYMROOT)/strings.NEW

nm.NEW: nm.o
	$(CC) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/nm.private.o \
		$(OFILE_DIR)/nm.o $(LIBSTUFF)
	$(CC) $(RC_CFLAGS) $(SDK) -o $(SYMROOT)/nm.NEW \
		$(OFILE_DIR)/nm.private.o
	$(DSYMUTIL) $(SYMROOT)/nm.NEW

libtool.NEW: libtool.o
	$(CC) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/libtool.private.o \
		$(OFILE_DIR)/libtool.o $(LIBSTUFF)
	$(CC) $(RC_CFLAGS) $(SDK) -o $(SYMROOT)/libtool.NEW \
		$(OFILE_DIR)/libtool.private.o
	$(DSYMUTIL) $(SYMROOT)/libtool.NEW

redo_prebinding.NEW: redo_prebinding.o
	$(CC) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/redo_prebinding.private.o \
		$(OFILE_DIR)/redo_prebinding.o $(LIBSTUFF)
	$(CC) $(RC_CFLAGS) $(SDK) -o $(SYMROOT)/redo_prebinding.NEW \
		$(OFILE_DIR)/redo_prebinding.private.o
	$(DSYMUTIL) $(SYMROOT)/redo_prebinding.NEW

libredo_prebinding.a: libredo_prebinding.o
	$(CC) $(RC_CFLAGS) -nostdlib -r -o $(OBJROOT)/libredo.o \
		libredo_prebinding.o $(LIBSTUFF)
	libtool -static -o $(SYMROOT)/libredo_prebinding.a $(OBJROOT)/libredo.o

seg_addr_table.NEW: seg_addr_table.o
	$(CC) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/seg_addr_table.private.o \
		$(OFILE_DIR)/seg_addr_table.o $(LIBSTUFF)
	$(CC) $(RC_CFLAGS) $(SDK) -o $(SYMROOT)/seg_addr_table.NEW \
		$(OFILE_DIR)/seg_addr_table.private.o
	$(DSYMUTIL) $(SYMROOT)/seg_addr_table.NEW

check_dylib.NEW: check_dylib.o
	$(CC) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/check_dylib.private.o \
		$(OFILE_DIR)/check_dylib.o $(LIBSTUFF)
	$(CC) $(RC_CFLAGS) $(SDK) -o $(SYMROOT)/check_dylib.NEW \
		$(OFILE_DIR)/check_dylib.private.o
	$(DSYMUTIL) $(SYMROOT)/check_dylib.NEW

inout.NEW: inout.o
	$(CC) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/inout.private.o \
		$(OFILE_DIR)/inout.o $(LIBSTUFF)
	$(CC) $(RC_CFLAGS) $(SDK) -o $(SYMROOT)/inout.NEW \
		$(OFILE_DIR)/inout.private.o
	$(DSYMUTIL) $(SYMROOT)/inout.NEW

codesign_allocate.NEW: codesign_allocate.o
	$(CC) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/codesign_allocate.private.o \
		$(OFILE_DIR)/codesign_allocate.o $(LIBSTUFF)
	$(CC) $(RC_CFLAGS) $(SDK) -o $(SYMROOT)/codesign_allocate.NEW \
		-sectcreate __TEXT __info_plist Info.plist \
		$(OFILE_DIR)/codesign_allocate.private.o
	$(DSYMUTIL) $(SYMROOT)/codesign_allocate.NEW

ctf_insert.NEW: ctf_insert.o
	$(CC) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/ctf_insert.private.o \
		$(OFILE_DIR)/ctf_insert.o $(LIBSTUFF)
	$(CC) $(RC_CFLAGS) $(SDK) -o $(SYMROOT)/ctf_insert.NEW \
		$(OFILE_DIR)/ctf_insert.private.o
	$(DSYMUTIL) $(SYMROOT)/ctf_insert.NEW

seg_hack.NEW: seg_hack.o
	$(CC) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/seg_hack.private.o \
		$(OFILE_DIR)/seg_hack.o $(LIBSTUFF)
	$(CC) $(RC_CFLAGS) $(SDK) -o $(SYMROOT)/seg_hack.NEW \
		$(OFILE_DIR)/seg_hack.private.o
	$(DSYMUTIL) $(SYMROOT)/seg_hack.NEW

install_name_tool.NEW: install_name_tool.o
	$(CC) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/install_name_tool.private.o \
		$(OFILE_DIR)/install_name_tool.o $(LIBSTUFF)
	$(CC) $(RC_CFLAGS) $(SDK) -o $(SYMROOT)/install_name_tool.NEW \
		$(OFILE_DIR)/install_name_tool.private.o
	$(DSYMUTIL) $(SYMROOT)/install_name_tool.NEW

indr.NEW: indr.o
	$(CC) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/indr.private.o \
		$(OFILE_DIR)/indr.o $(LIBSTUFF)
	$(CC) $(RC_CFLAGS) $(SDK) -o $(SYMROOT)/indr.NEW \
		$(OFILE_DIR)/indr.private.o
	$(DSYMUTIL) $(SYMROOT)/indr.NEW

strip.NEW: strip.o
	$(CC) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/strip.private.o \
		$(OFILE_DIR)/strip.o $(LIBSTUFF)
	$(CXX) $(RC_CFLAGS) $(SDK) -o $(SYMROOT)/strip.NEW \
		$(OFILE_DIR)/strip.private.o $(LIB_PRUNETRIE) $(CXXLIB)
	$(DSYMUTIL) $(SYMROOT)/strip.NEW

nmedit.NEW: nmedit.o
	$(CC) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/nmedit.private.o \
		$(OFILE_DIR)/nmedit.o $(LIBSTUFF)
	$(CC) $(RC_CFLAGS) $(SDK) -o $(SYMROOT)/nmedit.NEW \
		$(OFILE_DIR)/nmedit.private.o
	$(DSYMUTIL) $(SYMROOT)/nmedit.NEW

nmedit.o: strip.c
	$(CC) $(CFLAGS) $(RC_CFLAGS) -c \
		-o $(OFILE_DIR)/nmedit.o $(SRCROOT)/strip.c -DNMEDIT

segedit.NEW: segedit.o
	$(CC) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/segedit.private.o \
		$(OFILE_DIR)/segedit.o $(LIBSTUFF)
	$(CC) $(RC_CFLAGS) $(SDK) -o $(SYMROOT)/segedit.NEW \
		$(OFILE_DIR)/segedit.private.o
	$(DSYMUTIL) $(SYMROOT)/segedit.NEW

pagestuff.NEW: pagestuff.o
	$(CC) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/pagestuff.private.o \
		$(OFILE_DIR)/pagestuff.o $(LIBSTUFF)
	$(CC) $(RC_CFLAGS) $(SDK) -o $(SYMROOT)/pagestuff.NEW \
		$(OFILE_DIR)/pagestuff.private.o
	$(DSYMUTIL) $(SYMROOT)/pagestuff.NEW


clean: shlib_clean
	-rm -f \
	$(SYMROOT)/lipo.NEW \
	$(SYMROOT)/size.NEW \
	$(SYMROOT)/strings.NEW \
	$(SYMROOT)/nm.NEW \
	$(SYMROOT)/libtool.NEW \
	$(SYMROOT)/checksyms.NEW \
	$(SYMROOT)/seg_addr_table.NEW \
	$(SYMROOT)/check_dylib.NEW \
	$(SYMROOT)/inout.NEW \
	$(SYMROOT)/codesign_allocate.NEW \
	$(SYMROOT)/ctf_insert.NEW \
	$(SYMROOT)/seg_hack.NEW \
	$(SYMROOT)/install_name_tool.NEW \
	$(SYMROOT)/codesign_allocate.NEW \
	$(SYMROOT)/indr.NEW \
	$(SYMROOT)/strip.NEW \
	$(SYMROOT)/nmedit.NEW \
	$(SYMROOT)/segedit.NEW \
	$(SYMROOT)/pagestuff.NEW \
	$(SYMROOT)/redo_prebinding.NEW \
	$(SYMROOT)/libredo_prebinding.a \
	$(SYMROOT)/cmpdylib.NEW

shlib_clean:
	-rm -f \
	$(OFILE_DIR)/lipo.o \
	$(OFILE_DIR)/size.o \
	$(OFILE_DIR)/strings.o \
	$(OFILE_DIR)/nm.o \
	$(OFILE_DIR)/libtool.o \
	$(OFILE_DIR)/make.h \
	$(OFILE_DIR)/checksyms.o \
	$(OFILE_DIR)/inout.o \
	$(OFILE_DIR)/codesign_allocate.o \
	$(OFILE_DIR)/ctf_insert.o \
	$(OFILE_DIR)/seg_hack.o \
	$(OFILE_DIR)/install_name_tool.o \
	$(OFILE_DIR)/indr.o \
	$(OFILE_DIR)/segedit.o \
	$(OFILE_DIR)/pagestuff.o \
	$(OFILE_DIR)/redo_prebinding.o \
	$(OFILE_DIR)/libredo_prebinding.o \
	$(OFILE_DIR)/libredo.o \
	$(OFILE_DIR)/seg_addr_table.o \
	$(OFILE_DIR)/check_dylib.o \
	$(OFILE_DIR)/cmpdylib.o \
	$(OFILE_DIR)/nmedit.o \
	$(OFILE_DIR)/strip.o \
	$(OFILE_DIR)/lipo.private.o \
	$(OFILE_DIR)/size.private.o \
	$(OFILE_DIR)/strings.private.o \
	$(OFILE_DIR)/nm.private.o \
	$(OFILE_DIR)/libtool.private.o \
	$(OFILE_DIR)/checksyms.private.o \
	$(OFILE_DIR)/inout.private.o \
	$(OFILE_DIR)/codesign_allocate.private.o \
	$(OFILE_DIR)/ctf_insert.private.o \
	$(OFILE_DIR)/seg_hack.private.o \
	$(OFILE_DIR)/install_name_tool.private.o \
	$(OFILE_DIR)/indr.private.o \
	$(OFILE_DIR)/segedit.private.o \
	$(OFILE_DIR)/pagestuff.private.o \
	$(OFILE_DIR)/redo_prebinding.private.o \
	$(OFILE_DIR)/seg_addr_table.private.o \
	$(OFILE_DIR)/check_dylib.private.o \
	$(OFILE_DIR)/cmpdylib.private.o \
	$(OFILE_DIR)/nmedit.private.o \
	$(OFILE_DIR)/strip.private.o \
	-rm -f make.out
	-rm -r -f *dSYM

install: all dt_install os_tools_install

install_os_tools: os_tools os_tools_install

os_tools_install:
	$(MKDIRS) $(DSTROOT)$(USRBINDIR)
	install -c -s -m 555 $(SYMROOT)/strings.NEW \
		$(DSTROOT)$(USRBINDIR)/strings
	install -c -s -m 555 $(SYMROOT)/lipo.NEW $(DSTROOT)$(USRBINDIR)/lipo

dt_install:
	$(MKDIRS) $(DSTROOT)$(USRBINDIR)
	install -c -s -m 555 $(SYMROOT)/strip.NEW $(DSTROOT)$(USRBINDIR)/strip
	install -c -s -m 555 $(SYMROOT)/size.NEW $(DSTROOT)$(USRBINDIR)/size
	install -c -s -m 555 $(SYMROOT)/nm.NEW $(DSTROOT)$(USRBINDIR)/nm
	install -c -s -m 555 $(SYMROOT)/libtool.NEW \
		$(DSTROOT)$(USRBINDIR)/libtool
	(cd $(DSTROOT)$(USRBINDIR); rm -f ranlib; ln -s libtool ranlib)
	install -c -s -m 555 $(SYMROOT)/segedit.NEW \
		$(DSTROOT)$(USRBINDIR)/segedit
	install -c -s -m 555 $(SYMROOT)/cmpdylib.NEW \
			  $(DSTROOT)$(USRBINDIR)/cmpdylib
	install -c -s -m 555 $(SYMROOT)/pagestuff.NEW \
			  $(DSTROOT)$(USRBINDIR)/pagestuff
	install -c -s -m 555 $(SYMROOT)/redo_prebinding.NEW \
			  $(DSTROOT)$(USRBINDIR)/redo_prebinding
	install -c -s -m 555 $(SYMROOT)/nmedit.NEW $(DSTROOT)$(USRBINDIR)/nmedit
	install -c -s -m 555 $(SYMROOT)/install_name_tool.NEW \
			  $(DSTROOT)$(USRBINDIR)/install_name_tool
	install -c -s -m 555 $(SYMROOT)/codesign_allocate.NEW \
			  $(DSTROOT)$(USRBINDIR)/codesign_allocate
	install -c -s -m 555 $(SYMROOT)/ctf_insert.NEW \
			  $(DSTROOT)$(USRBINDIR)/ctf_insert
	$(MKDIRS) $(DSTROOT)$(LOCBINDIR)
	install -c -s -m 555 $(SYMROOT)/checksyms.NEW \
			  $(DSTROOT)$(LOCBINDIR)/checksyms
	install -c -s -m 555 $(SYMROOT)/seg_addr_table.NEW \
			  $(DSTROOT)$(LOCBINDIR)/seg_addr_table
	install -c -s -m 555 $(SYMROOT)/check_dylib.NEW \
			  $(DSTROOT)$(LOCBINDIR)/check_dylib
	install -c -s -m 555 $(SYMROOT)/indr.NEW $(DSTROOT)$(LOCBINDIR)/indr
	install -c -s -m 555 $(SYMROOT)/seg_hack.NEW \
			  $(DSTROOT)$(LOCBINDIR)/seg_hack

lib_ofiles_install: lib_ofiles
	$(MKDIRS) $(DSTROOT)$(LOCLIBDIR)
	install -p -c -m 444 $(SYMROOT)/libredo_prebinding.a \
		 $(DSTROOT)$(LOCLIBDIR)/libredo_prebinding.a

installsrc:
	$(MKDIRS) $(SRCROOT)
	chmod 755 $(SRCROOT)
	gnutar cf - $(INSTALL_FILES) | (cd $(SRCROOT); gnutar xf -)
	chmod 444 $(SRCROOT)/*

$(OFILE_DIR) $(SYMROOT):
	$(MKDIRS) $@
