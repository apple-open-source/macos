
DIR = boot1
include ../MakePaths.dir

INSTALLDIR = $(DSTROOT)/usr/standalone/i386
DIRS_NEEDED = $(OBJROOT) $(SYMROOT)

FOREIGNNEXT = boot1f

NASM = $(SYMROOT)/nasm

VERSIONED_FILES = boot1f

VERS = `vers_string -f 5.0 | tr - .`
NEW_VERS = Rhapsody boot1 v$(VERS)
ifneq "" "$(wildcard /bin/mkdirs)"
  MKDIRS = /bin/mkdirs
else
  MKDIRS = /bin/mkdir -p
endif

all: $(DIRS_NEEDED) $(VERSIONED_FILES)

boot1f: boot1.s Makefile
	$(NASM) -dBOOTDEV=FLOPPY -dVERS="'$(NEW_VERS)'" boot1.s -o $(SYMROOT)/$@

install_i386:: all $(INSTALLDIR)
	cp $(SYMROOT)/boot1f $(INSTALLDIR)/
	cd $(INSTALLDIR); chmod u+w $(FOREIGNNEXT)

include ../MakeInc.dir
