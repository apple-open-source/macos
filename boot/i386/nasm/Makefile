
DIR = nasm
include ../MakePaths.dir

# Don't install in the usual place.  Currently it's only 
# needed for building boot0, boot1, boot1f, and nullboot1.
#MANINSTALLDIR = $(DSTROOT)/usr/share/man/man1
#INSTALLDIR = $(DSTROOT)/usr/local/bin
MANINSTALLDIR = $(SYMROOT)
INSTALLDIR = $(SYMROOT)

OPTIM = -O2
CFLAGS	= $(OPTIM) -g  -Wall -traditional-cpp
DEFINES=
INC = -I.
ifneq "" "$(wildcard /bin/mkdirs)"
  MKDIRS = /bin/mkdirs
else
  MKDIRS = /bin/mkdir -p
endif

VPATH = $(OBJROOT):$(SYMROOT)

NASM = nasm.o nasmlib.o float.o insnsa.o assemble.o labels.o \
       parser.o outform.o outbin.o outaout.o outcoff.o outelf.o \
       outobj.o outas86.o outrdf.o outdbg.o preproc.o listing.o \
       eval.o

NDISASM = ndisasm.o disasm.o sync.o nasmlib.o insnsd.o

CFILES = nasm.c nasmlib.c float.c insnsa.c assemble.c labels.c \
	parser.c outform.c outbin.c outaout.c outcoff.c outelf.c \
	outobj.c outas86.c outrdf.c outdbg.c preproc.c listing.c \
	eval.c ndisasm.c disasm.c sync.c insnsd.c

HFILES = nasm.h nasmlib.h preproc.h parser.h assemble.h labels.h \
	outform.h listing.h eval.h insns.h sync.h disasm.h float.h

OTHERFILES = Makefile

ALLSRC =  $(CFILES) $(HFILES) $(OTHERFILES)

DIRS_NEEDED = $(OBJROOT) $(SYMROOT)

all: $(DIRS_NEEDED) nasm ndisasm installman

nasm: $(NASM)
	$(CC) -o $(INSTALLDIR)/nasm $(NASM)

ndisasm: $(NDISASM)
	$(CC) -o $(INSTALLDIR)/ndisasm $(NDISASM)

installman: nasm.1 ndisasm.1
	cp nasm.1 $(MANINSTALLDIR)
	cp ndisasm.1 $(MANINSTALLDIR)

# These two source files are automagically generated from a single
# instruction-table file by a Perl script. They're distributed,
# though, so it isn't necessary to have Perl just to recompile NASM
# from the distribution.

insnsa.c insnsd.c: insns.dat insns.pl
	perl insns.pl insns.dat

# This source file is generated from the standard macros file
# `standard.mac' by another Perl script. Again, it's part of the
# standard distribution.

macros.c: standard.mac macros.pl
	perl macros.pl standard.mac

clean::
	rm -rf $(SYMROOT)/nasm $(SYMROOT)/ndisasm

$(INSTALLDIR) $(MANINSTALLDIR):
	$(MKDIRS) $@

installhdrs:: $(INSTALLDIR) $(MANINSTALLDIR)

include ../MakeInc.dir

# dependencies
-include $(OBJROOT)/Makedep
