
DIR = libsa
include ../MakePaths.dir

UTILDIR = ../util
INSTALL_SA_DIR = $(DSTROOT)/System/Library/Frameworks/System.framework/Versions/B/PrivateHeaders/standalone
INSTALL_MD_DIR = $(DSTROOT)/System/Library/Frameworks/System.framework/Versions/B/PrivateHeaders/machdep/i386

OPTIM = -Os
CFLAGS	= $(RC_CFLAGS) $(OPTIM) $(MORECPP) -arch i386 -g -Wmost -Wno-precomp \
	-munaligned-text -static -traditional-cpp

INC = -I. -I$(SYMROOT) -I$(UTILDIR)
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

SA_OBJS = prf.o printf.o zalloc.o \
	string.o strtol.o error.o \
	setjmp.o qsort.o bswap.o

SFILES = setjmp.s
CFILES = prf.c printf.c zalloc.c \
	string.c strtol.c error.c \
	qsort.c bswap.c
HFILES = memory.h
EXPORTED_HFILES = libsa.h kernBootStruct.h memory.h
INSTALLED_SA_HFILES = libsa.h
INSTALLED_MD_HFILES = kernBootStruct.h
OTHERFILES = Makefile
ALLSRC =  $(SFILES) $(CFILES) $(HFILES) $(OTHERFILES)
LIBS = libsa.a
DIRS_NEEDED = $(OBJROOT) $(SYMROOT)

all: $(DIRS_NEEDED) $(LIBS)

libsa.a: $(SA_OBJS)
	rm -f $(SYMROOT)/libsa.a
	ar q $(SYMROOT)/libsa.a $(SA_OBJS)
	ranlib $(SYMROOT)/libsa.a

clean::
	rm -rf $(SYMROOT)/libsa.a $(SYMROOT)/libsaio.a

$(INSTALL_SA_DIR) $(INSTALL_MD_DIR):
	$(MKDIRS) $@

installhdrs:: $(INSTALL_SA_DIR) $(INSTALL_MD_DIR)
	cp $(INSTALLED_SA_HFILES) $(INSTALL_SA_DIR)
	cp $(INSTALLED_MD_HFILES) $(INSTALL_MD_DIR)

include ../MakeInc.dir

# dependencies
-include $(OBJROOT)/Makedep
