
DIR = sarld
include ../MakePaths.dir

DIRS_NEEDED = $(OBJROOT) $(SYMROOT)
VPATH = $(OBJROOT):$(SYMROOT)

INSTALLDIR = $(DSTROOT)/usr/standalone/i386

LIBSA=$(SYMROOT)/libsa.a
LIBSADIR = ../libsa
LIBSARLD = -lsarld
SARLD = $(SYMROOT)/sarld

CFLAGS = -static
ARCHFLAGS = -arch i386

RLD_ADDR = `awk '/RLD_ADDR/ { print $$3 }' < $(LIBSADIR)/memory.h`

all: $(SARLD)

$(SARLD): $(SARLD).sys
	strip -o $(SARLD) $(SARLD).sys
	
$(SARLD).sys: $(LIBSA)
	$(CC) $(COPTS) $(CFLAGS) $(ARCHFLAGS) -o $(SARLD).sys \
		-e _sa_rld -u _sa_rld \
                -seg1addr $(RLD_ADDR) -preload -nostdlib $(LIBSARLD) $(LIBSA)

clean::
	rm -rf $(SARLD) $(SARLD).sys
	
install_i386:: $(SARLD) $(INSTALLDIR)
	cp $(SARLD) $(INSTALLDIR)

include ../MakeInc.dir

#dependencies
