
#	Makefile for i386 boot program
#	define FLOPPY and SMALL using DEFINES macro as necessary

DIR = install
include ../MakePaths.dir

DEBUG = -g
CFLAGS	= $(DEBUG) $(MORECPP) -arch i386 -Wmost -DINSTALL_INTERNAL_USER
DEFINES=
CONFIG = hd
SYMDIR = $(SYMROOT)
LIBSADIR = ../libsa
LIBSAIODIR = ../libsaio
UTILDIR = ../util
INC = -I. -I$(SYMDIR) -I$(LIBSADIR) -I$(LIBSAIODIR) -I$(UTILDIR)
ifneq "" "$(wildcard /bin/mkdirs)"
  MKDIRS = /bin/mkdirs
else
  MKDIRS = /bin/mkdir -p
endif
AS = as
LD = ld

LIBSA = $(SYMDIR)/libsa.a
LIBSAIO = $(SYMDIR)/libsaio.a
LIBS = $(LIBSAIO) $(LIBSA)

OTHER_FILES =

INSTALLDIR = $(DSTROOT)/usr/standalone/i386
VPATH = $(OBJROOT):$(SYMROOT)

OBJS = choose.o mode.o install_table.o

UTILDIR = ../util
SFILES = boot2.s
CFILES = boot.c load.c stringTable.c graphics.c browser.c \
	 button.c scrollbar.c prompt.c
HFILES = kernBootStruct.h
OTHERFILES = Makefile machOconv.c README
ALLSRC = $(FOREIGNSRC) $(FOREIGNBIN) $(SFILES) $(CFILES) \
	$(HFILES) $(OTHERFILES)
DIRS_NEEDED = $(OBJROOT) $(SYMROOT)
BOOT2ADDR = 3000

SIG = $(SYMROOT)/sig
GENFILES = $(SYMROOT)/install_internal.h \
	$(SYMROOT)/install_external.h \
	$(SYMROOT)/install_table.c \
	$(SYMROOT)/install_defs.h

all: $(DIRS_NEEDED) install_external.h install.module

install.module: $(SYMROOT) $(OBJS)
	$(CC) $(COPTS) $(CFLAGS) $(RC_CFLAGS) -o $@ \
		$(OBJS) \
		-e _install_functions -u _install_functions \
                -seg1addr 30000 -preload -nostdlib $(LIBS)

install_internal.h: install_external.h
install_table.c: install_external.h
install_defs.h: install_external.h
install_external.h: install.def
	$(SIG) -d $(SYMROOT) -n install install.def

install_i386:: all $(INSTALLDIR)
	cp $(SYMROOT)/boot $(OTHER_FILES) $(INSTALLDIR)
	cd $(INSTALLDIR); chmod u+w boot $(OTHER_FILES)

clean::
	rm -f $(SYMROOT)/boot.sys $(GENFILES)

include ../MakeInc.dir

#dependencies
-include $(OBJROOT)/Makedep
