
#	Makefile for i386 boot program
#	define FLOPPY and SMALL using DEFINES macro as necessary

DIR = testmodule
include ../MakePaths.dir

DEBUG = -g
OPTIM = -Os
CFLAGS	= $(DEBUG) $(OPTIM) $(MORECPP) -arch i386 -Wmost -DTEST_INTERNAL_USER
DEFINES=
CONFIG = hd
SYMDIR = $(SYMROOT)
LIBSADIR = ../libsa
LIBSAIODIR = ../libsaio
UTILDIR = ../util
INC = -I. -I.. -I$(SYMDIR) -I$(LIBSADIR) -I$(LIBSAIODIR) -I$(UTILDIR)
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

OBJS = test.o test_table.o

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

all: $(DIRS_NEEDED) test_external.h test.module

test.module: $(SYMROOT) $(OBJS)
	$(CC) $(COPTS) $(CFLAGS) $(RC_CFLAGS) -o $@ \
		$(OBJS) \
		-e _test_entry -u _test_entry \
                -seg1addr 50000 -preload -nostdlib $(LIBS)

test_internal.h: test_external.h
test_table.c: test_external.h
test_defs.h: test_external.h
test_external.h: test.def
	$(SIG) -d $(SYMROOT) test.def

install_i386:: all $(INSTALLDIR)
	cp $(SYMROOT)/boot $(OTHER_FILES) $(INSTALLDIR)
	cd $(INSTALLDIR); chmod u+w boot $(OTHER_FILES)

clean::
	rm -f $(SYMROOT)/boot.sys $(GENFILES)

include ../MakeInc.dir

#dependencies
-include $(OBJROOT)/Makedep
