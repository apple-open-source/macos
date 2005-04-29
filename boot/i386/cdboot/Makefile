
DIR = cdboot
include ../MakePaths.dir

NASM = $(SYMROOT)/nasm
INSTALLDIR = $(DSTROOT)/usr/standalone/i386
DIRS_NEEDED = $(SYMROOT)

all: $(DIRS_NEEDED) cdboot

cdboot: cdboot.s $(SYMROOT)/boot Makefile $(NASM)
	$(NASM) cdboot.s -o $(SYMROOT)/cdboot
	dd if=$(SYMROOT)/boot of=$(SYMROOT)/cdboot conv=sync bs=2k seek=1

biostest: biostest.asm Makefile $(NASM)
	$(NASM) biostest.asm -o $(SYMROOT)/$@

install_i386:: all $(INSTALLDIR)
	cp $(SYMROOT)/cdboot $(INSTALLDIR)
	cd $(INSTALLDIR); chmod u+w cdboot

clean::
	rm -f $(SYMROOT)/cdboot

include ../MakeInc.dir

#dependencies

