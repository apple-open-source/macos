DIR = boot1u
include ../MakePaths.dir

OPTIM = -Os
CFLAGS	= $(RC_CFLAGS) $(OPTIM) $(MORECPP) -arch i386 -g -Wmost -Werror \
          -fno-builtin -DSAIO_INTERNAL_USER -DBOOT1 -static \
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
LIBS= -L$(SYMDIR) -lsa
#LIBS= -L$(SYMDIR)

OTHER_FILES =

INSTALLDIR = $(DSTROOT)/usr/standalone/i386
VPATH = $(SYMROOT):$(OBJROOT)
vpath %.c ../libsaio
vpath %   $(SYMROOT)
vpath %.h ../libsaio
vpath %   $(SYMROOT)

# The ordering is important;
# boot1u.o must be first.
OBJS = boot1u.o asm.o bios.o boot.o string.o malloc.o disk.o put.o \
     ufs.o cache.o misc.o ufs_byteorder.o table.o

# OBJS += $(OBJROOT)/../libsa/prf.o $(OBJROOT)/../libsa/printf.o

# We build the following source files from the libsaio directory:
# ufs.o cache.o ufs_byteorder.o table.o
# and get these object files from libsa:
# prf.o printf.o
# If they increase in size, or if other accidental dependencies
# are created with other .o files in libsa.a, then boot1u can get too large.
# Use care in changing libsaio and libsa.

UTILDIR = ../util
SFILES = boot1u.s bios.s asm.s
CFILES = boot.c disk.c malloc.c put.c string.c \
     ufs.c cache.c misc.c ufs_byteorder.c table.c
HFILES = 
OTHERFILES = Makefile
ALLSRC = $(FOREIGNSRC) $(FOREIGNBIN) $(SFILES) $(CFILES) \
	$(HFILES) $(OTHERFILES)
DIRS_NEEDED = $(OBJROOT) $(SYMROOT)
BOOT1UADDR = 10200

# Max boot1u code size is 7k (to leave room for the disk label)
# minus 512 bytes for the partition booter,
# for a total of 6.5k.
MAXBOOTSIZE = 6656

NASM = $(SYMROOT)/nasm

all: $(DIRS_NEEDED) boot1u0 boot1u

boot1u0: boot1u0.s Makefile $(NASM)
	$(NASM) boot1u0.s -o $(SYMROOT)/$@

boot1u: $(SYMROOT)/machOconv boot1u0 $(OBJS)
	$(LD) -static -preload -segaddr __TEXT $(BOOT1UADDR) -segalign 20 \
		-o $(SYMROOT)/$(@F).sys $(filter %.o,$^) $(LIBS) -lcc_kext
	size $(SYMROOT)/$(@F).sys
	$(SYMROOT)/machOconv $(SYMROOT)/$(@F).sys $(SYMROOT)/$(@F).post
	ls -l $(SYMROOT)/$(@F).post
	@( size=`ls -l $(SYMROOT)/$(@F).post | awk '{ print $$5}'` ; \
	  if expr "$$size" ">" "$(MAXBOOTSIZE)" > /dev/null ;\
	  then \
	    echo "Booter executable larger than $(MAXBOOTSIZE) bytes" ;\
	    exit 1;\
	  fi)
	cat $(SYMROOT)/boot1u0 $(SYMROOT)/$(@F).post | dd obs=7k conv=osync of=$(SYMROOT)/$(@F)
	rm $(SYMROOT)/$(@F).post


install_i386:: all $(INSTALLDIR)
	cp $(SYMROOT)/boot1u $(OTHER_FILES) $(INSTALLDIR)
	cd $(INSTALLDIR); chmod u+w boot1u $(OTHER_FILES)

clean::
	rm -f $(SYMROOT)/boot1u.sys $(SYMROOT)/boot1u $(SYMROOT)/boot1u0

include ../MakeInc.dir

#dependencies
-include $(OBJROOT)/Makedep
