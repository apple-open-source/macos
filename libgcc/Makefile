
SRCROOT = .
SYMROOT = .
OBJROOT = .

PAX = /bin/pax -rw
MKDIR = /bin/mkdir -p
CHMOD = /bin/chmod

ifeq (,$(RC_ARCHS))
	# build for the local arch only
	ARCH_CFLAGS =
else
	# assume the toolchain supports static compilation for all request archs
	ARCH_CFLAGS = $(patsubst %,-arch %,$(RC_ARCHS))
endif


USRLIBDIR = /usr/lib
DSTDIRS = $(DSTROOT)$(USRLIBDIR) 

INSTALLSRC_FILES = Makefile stub.c

# default target for development builds
all: $(OBJROOT)/libgcc_s.dylib

$(OBJROOT)/libgcc_s.dylib : $(SRCROOT)/stub.c
	$(CC) $(ARCH_CFLAGS) $^ -dynamiclib -install_name /usr/lib/libgcc_s.1.dylib \
		-compatibility_version 1 -current_version ${RC_ProjectBuildVersion} \
		-nostdlib -o $(OBJROOT)/libgcc_s.dylib.full
	strip -c -x $(OBJROOT)/libgcc_s.dylib.full -o $@ 
	

clean:
	rm -f $(OBJROOT)/libgcc_s.dylib.full  $(OBJROOT)/libgcc_s.dylib


install: $(OBJROOT)/libgcc_s.dylib
	mkdir -p $(DSTROOT)/usr/lib
	cp $(OBJROOT)/libgcc_s.dylib $(DSTROOT)/usr/lib/libgcc_s.10.5.dylib 
	cd $(DSTROOT)/usr/lib; \
	ln -s libgcc_s.10.5.dylib libgcc_s.10.4.dylib; \
	ln -s libSystem.B.dylib libgcc_s.1.dylib; \
	

installhdrs:

installsrc:
	$(MKDIR) $(SRCROOT)
	$(CHMOD) 755 $(SRCROOT)
	$(PAX) $(INSTALLSRC_FILES) $(SRCROOT)
	$(CHMOD) 444 $(SRCROOT)/*

