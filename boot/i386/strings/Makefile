
DIR = strings
include ../MakePaths.dir

INSTALLDIR = $(DSTROOT)/usr/standalone/i386
VPATH = $(OBJROOT)

CHOWN = chown

FILES = BootHelp.txt

all:

install_i386:: $(INSTALLDIR)
	cp $(FILES) $(INSTALLDIR)
	chown -fR root.wheel $(INSTALLDIR)
	chmod -R ugo-w  $(INSTALLDIR)

include ../MakeInc.dir
