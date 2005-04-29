
DIR = boot1
include ../MakePaths.dir

INSTALLDIR = $(DSTROOT)/usr/standalone/i386
DIRS_NEEDED = $(OBJROOT) $(SYMROOT)

NASM = $(SYMROOT)/nasm

VERSIONED_FILES = boot1h

VERS = `vers_string -f 5.0 | tr - .`
NEW_VERS = Darwin boot1h v$(VERS)
ifneq "" "$(wildcard /bin/mkdirs)"
  MKDIRS = /bin/mkdirs
else
  MKDIRS = /bin/mkdir -p
endif

all: $(DIRS_NEEDED) $(VERSIONED_FILES)

boot1h: boot1.s Makefile
	$(NASM) -dBOOTDEV=HDISK -dVERS="'$(NEW_VERS)'" boot1.s -o $(SYMROOT)/$@

install_i386:: all $(INSTALLDIR)
	cp $(SYMROOT)/boot1h $(INSTALLDIR)/
	cd $(INSTALLDIR); chmod u+w $(VERSIONED_FILES) 

clean::
	rm -f $(SYMROOT)/boot1h

include ../MakeInc.dir
