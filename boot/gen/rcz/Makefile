
DIR = rcz
include ../MakePaths.dir

UTILDIR = ../util
LIBSADIR = ../libsa
USRBIN = $(DSTROOT)/usr/bin
INSTALLDIR = $(DSTROOT)/System/Library/Frameworks/System.framework/Versions/B/PrivateHeaders/standalone

DEBUG = -O
CFLAGS	= $(DEBUG) $(MORECPP) $(RC_CFLAGS) -g -Wmost -Wno-precomp -traditional-cpp
DEFINES=
CONFIG = hd
INC = -I. -I$(SYMROOT) -I$(UTILDIR) -I$(LIBSADIR)
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

SFILES =
CFILES = 
HFILES = 
EXPORTED_HFILES =
INSTALLED_HFILES =
OTHERFILES = Makefile
ALLSRC =  $(SFILES) $(CFILES) \
	$(HFILES) $(OTHERFILES)
DIRS_NEEDED = $(OBJROOT) $(SYMROOT) $(USRBIN)

RCZ_OBJS = rcz.o rcz_compress_mem.o rcz_decompress_mem.o

all: $(DIRS_NEEDED) rcz

rcz: $(USRBIN) $(RCZ_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(DEFINES) -o $(USRBIN)/$@ $(RCZ_OBJS)
	strip -x -u $(USRBIN)/$@

clean::
	rm -rf $(USRBIN)/rcz

include ../MakeInc.dir

# dependencies
-include $(OBJROOT)/Makedep
