
DIR = boot0
include ../MakePaths.dir

NASM = $(SYMROOT)/nasm
INSTALLDIR = $(DSTROOT)/usr/standalone/i386
DIRS_NEEDED = $(SYMROOT)

all: $(DIRS_NEEDED) boot0

boot0: boot0.s Makefile $(NASM)
	$(NASM) boot0.s -o $(SYMROOT)/$@

install_i386:: all $(INSTALLDIR)
	cp $(SYMROOT)/boot0 $(INSTALLDIR)
	cd $(INSTALLDIR); chmod u+w boot0

include ../MakeInc.dir

#dependencies

