
DIR = libsaio
include ../MakePaths.dir

UTILDIR = ../util
LIBSADIR = ../libsa
INSTALLDIR = $(DSTROOT)/System/Library/Frameworks/System.framework/Versions/B/PrivateHeaders/standalone
SYMROOT=

OPTIM = -Os
CFLAGS	= $(RC_CFLAGS) $(OPTIM) $(MORECPP) -arch i386 -g -Wmost -Werror \
    -D__ARCHITECTURE__=\"i386\" -DSAIO_INTERNAL_USER \
    -DRCZ_COMPRESSED_FILE_SUPPORT \
    -fno-builtin -static -fomit-frame-pointer \
    -mpreferred-stack-boundary=2 -fno-align-functions
DEFINES=
CONFIG = hd
INC = -I../rcz -I. -I$(SYMROOT) -I$(UTILDIR) -I$(LIBSADIR)
ifneq "" "$(wildcard /bin/mkdirs)"
  MKDIRS = /bin/mkdirs
else
  MKDIRS = /bin/mkdir -p
endif
AS = as
LD = ld
# LIBS= -lc_static
LIBS=

VPATH = $(OBJROOT):$(SYMROOT)

SAIO_OBJS = table.o asm.o bios.o biosfn.o \
	disk.o sys.o cache.o bootstruct.o \
	ufs.o ufs_byteorder.o \
	stringTable.o load.o memory.o misc.o \
	vbe.o nbp.o hfs.o hfs_compare.o \
	xml.o

SAIO_EXTERN_OBJS = console.o

SFILES =
CFILES = 
HFILES = 
EXPORTED_HFILES =
INSTALLED_HFILES =
OTHERFILES = Makefile
ALLSRC =  $(SFILES) $(CFILES) \
	$(HFILES) $(OTHERFILES)
LIBS = libsaio.a
DIRS_NEEDED = $(OBJROOT) $(SYMROOT)
#GENFILES = $(SYMROOT)/saio_internal.h \
#	$(SYMROOT)/saio_external.h \
#	$(SYMROOT)/saio_defs.h \
#	$(SYMROOT)/saio_table.c

#SIG = $(SYMROOT)/sig

all: $(DIRS_NEEDED) libsaio.h $(LIBS)

#libsaio_static.a: $(SAIO_OBJS)
#	rm -f $(SYMROOT)/$@
#	ar q $(SYMROOT)/$@ $(SAIO_OBJS)
#	ranlib $(SYMROOT)/$@

libsaio.a: $(SAIO_EXTERN_OBJS) $(SAIO_OBJS)
	rm -f $(SYMROOT)/$(@F)
	ar q $(SYMROOT)/$(@F) $(SAIO_EXTERN_OBJS) $(SAIO_OBJS)
	ranlib $(SYMROOT)/$(@F)

#saio_internal.h: saio_external.h
#saio_table.c: saio_external.h
#saio_defs.h: saio_external.h
#saio_external.h: saio.def
#	$(SIG) -d $(SYMROOT) -n saio saio.def

clean::
	rm -rf $(SYMROOT)/libsaio.a

#installhdrs:: $(INSTALLDIR)
#	cp $(INSTALLED_HFILES) $(INSTALLDIR)

include ../MakeInc.dir

# dependencies
-include $(OBJROOT)/Makedep
