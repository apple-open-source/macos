DIR = boot1u
include ../MakePaths.dir

#
# these paths are only valid in subdirectories of this directory
#
OBJROOT=../../obj/i386/boot1u
SYMROOT=../../sym/i386
DSTROOT=../../dst/i386
SRCROOT=/tmp

OPTIM = -Os
CFLAGS	= $(RC_CFLAGS) $(OPTIM) $(MORECPP) -arch i386 -g -Wmost -Werror \
          -fno-builtin -DSAIO_INTERNAL_USER -static \
	  -fomit-frame-pointer -mpreferred-stack-boundary=2 \
	  -fno-align-functions
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
LIBS= -L$(SYMDIR) -lsaio -lsa
#LIBS= -L$(SYMDIR)

OTHER_FILES =

INSTALLDIR = $(DSTROOT)/usr/standalone/i386
VPATH = $(OBJROOT):$(SYMROOT)

# The ordering is important;
# boot1u.o must be first.
#OBJS = $(OBJROOT)/boot1u.o $(OBJROOT)/asm.o $(OBJROOT)/bios.o $(OBJROOT)/boot.o $(OBJROOT)/string.o $(OBJROOT)/malloc.o $(OBJROOT)/disk.o $(OBJROOT)/put.o
OBJS = boot1u.o asm.o bios.o boot.o string.o malloc.o disk.o put.o

# OBJS += $(OBJROOT)/../libsaio/ufs.o \
#   $(OBJROOT)/../libsaio/cache.o \
#   $(OBJROOT)/../libsaio/ufs_byteorder.o \
#   $(OBJROOT)/../libsaio/table.o \
#   $(OBJROOT)/../libsaio/misc.o

# OBJS += $(OBJROOT)/../libsa/prf.o $(OBJROOT)/../libsa/printf.o

# We get the following object files out of libsaio:
# ufs.o cache.o ufs_byteorder.o table.o
# and these from libsa:
# prf.o printf.o
# If they increase in size, or if other accidental dependencies
# are created with other .o files in libsaio.a, then boot1u can get too large.
# Use care in changing the library.

UTILDIR = ../util
SFILES = boot1u.s bios.s asm.s
CFILES = boot.c disk.c string.c put.c malloc.c
HFILES = 
OTHERFILES = Makefile
ALLSRC = $(FOREIGNSRC) $(FOREIGNBIN) $(SFILES) $(CFILES) \
	$(HFILES) $(OTHERFILES)
DIRS_NEEDED = $(OBJROOT) $(SYMROOT)
BOOT1UADDR = 10200
MAXBOOTSIZE = 7680
NASM = $(SYMROOT)/nasm

all: $(DIRS_NEEDED) boot1u0 boot1u

boot1u0: boot1u0.s Makefile $(NASM)
	$(NASM) boot1u0.s -o $(SYMROOT)/$@

boot1u: $(SYMROOT)/machOconv $(OBJS) boot1u0
	$(LD) -static -preload -segaddr __TEXT $(BOOT1UADDR) -segalign 20 \
		-o $(SYMROOT)/boot1u.sys $(OBJS) $(LIBS) -lcc_kext
	size $(SYMROOT)/boot1u.sys
	$(SYMROOT)/machOconv $(SYMROOT)/boot1u.sys $(SYMROOT)/boot1u.post
	ls -l $(SYMROOT)/boot1u.post
	@( size=`ls -l $(SYMROOT)/boot1u.post | awk '{ print $$5}'` ; \
	  if expr "$$size" ">" "$(MAXBOOTSIZE)" > /dev/null ;\
	  then \
	    echo "Booter executable larger than $(MAXBOOTSIZE) bytes" ;\
	    exit 1;\
	  fi)
	cat $(SYMROOT)/boot1u0 $(SYMROOT)/boot1u.post | dd obs=8k conv=osync of=$(SYMROOT)/boot1u
	rm $(SYMROOT)/boot1u.post


install_i386:: all $(INSTALLDIR)
	cp $(SYMROOT)/boot1u $(OTHER_FILES) $(INSTALLDIR)
	cd $(INSTALLDIR); chmod u+w boot1u $(OTHER_FILES)

clean::
	rm -f $(SYMROOT)/boot1u.sys $(SYMROOT)/boot1u

include ../MakeInc.dir

#dependencies
-include $(OBJROOT)/Makedep
