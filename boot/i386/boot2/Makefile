
#	Makefile for i386 boot program
#	define FLOPPY and SMALL using DEFINES macro as necessary

DIR = boot2
include ../MakePaths.dir

OPTIM = -Os
CFLAGS	= $(RC_CFLAGS) $(OPTIM) $(MORECPP) -arch i386 -g -Wmost -Werror \
          -fno-builtin -DSAIO_INTERNAL_USER -static
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
# LIBS= -lc_static
LIBS= -L$(SYMDIR) -lsaio -lsa -lrcz 

OTHER_FILES =

INSTALLDIR = $(DSTROOT)/usr/standalone/i386
VPATH = $(OBJROOT):$(SYMROOT)

# The ordering is important;
# boot2.o must be first.
OBJS = boot2.o boot.o graphics.o drivers.o prompt.o options.o
# button.o browser.o scrollbar.o == NOTYET	

UTILDIR = ../util
SFILES = boot2.s
CFILES = boot.c graphics.c drivers.c prompt.c options.c
HFILES = boot.h
OTHERFILES = Makefile
ALLSRC = $(FOREIGNSRC) $(FOREIGNBIN) $(SFILES) $(CFILES) \
	$(HFILES) $(OTHERFILES)
DIRS_NEEDED = $(OBJROOT) $(SYMROOT)
BOOT2ADDR = 20200
MAXBOOTSIZE = 65024

all: $(DIRS_NEEDED) boot

boot: machOconv $(OBJS)
	$(LD) -static -preload -segaddr __TEXT $(BOOT2ADDR) -segalign 20 \
		-o $(SYMROOT)/boot.sys $(OBJS) $(LIBS) -lcc_kext
	machOconv $(SYMROOT)/boot.sys $(SYMROOT)/boot
	size $(SYMROOT)/boot.sys
	ls -l $(SYMROOT)/boot
	@( size=`ls -l $(SYMROOT)/boot | awk '{ print $$5}'` ; \
	  if expr "$$size" ">" "$(MAXBOOTSIZE)" > /dev/null ;\
	  then \
	    echo "Booter executable larger than $(MAXBOOTSIZE) bytes" ;\
	    rm $(SYMROOT)/boot ;\
	    exit 1;\
	  fi)

prompt.o: vers.h
vers.h:
	echo "#define I386BOOT_VERSION \"`vers_string -f 5.0`\"" | \
	  tr - . > $(SYMROOT)/vers.h

install_i386:: all $(INSTALLDIR)
	cp $(SYMROOT)/boot $(OTHER_FILES) $(INSTALLDIR)
	cd $(INSTALLDIR); chmod u+w boot $(OTHER_FILES)

clean::
	rm -f $(SYMROOT)/boot.sys $(SYMROOT)/boot

include ../MakeInc.dir

#dependencies
-include $(OBJROOT)/Makedep
