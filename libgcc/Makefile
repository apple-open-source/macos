
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

SDK_DIR = $(shell xcodebuild -version -sdk "$(SDKROOT)" Path)
STRIP   = $(shell xcodebuild -sdk "$(SDKROOT)" -find strip)
CC      = $(shell xcodebuild -sdk "$(SDKROOT)" -find cc)

ifeq (,$(RC_PURPLE))
	INSTALL_TARGET = install-MacOSX
else
	INSTALL_TARGET = install-iPhoneOS
endif

USRLIBDIR = /usr/lib
DSTDIRS = $(DSTROOT)$(USRLIBDIR) 

INSTALLSRC_FILES = Makefile stub.c reexports.exp

# default target for development builds
all: $(OBJROOT)/libgcc_s.dylib

$(OBJROOT)/libgcc_s.dylib : $(SRCROOT)/stub.c
	$(CC) $(ARCH_CFLAGS) $^ -dynamiclib -install_name /usr/lib/libgcc_s.1.dylib \
		-compatibility_version 1 -current_version ${RC_ProjectBuildVersion} \
		-nostdlib -o $(OBJROOT)/libgcc_s.dylib.full
	$(STRIP) -c -x $(OBJROOT)/libgcc_s.dylib.full -o $@ 
	

clean:
	rm -f $(OBJROOT)/libgcc_s.dylib.full  $(OBJROOT)/libgcc_s.dylib

install:  $(INSTALL_TARGET)


installhdrs:

installsrc:
	$(MKDIR) $(SRCROOT)
	$(CHMOD) 755 $(SRCROOT)
	$(PAX) $(INSTALLSRC_FILES) $(SRCROOT)
	$(CHMOD) 444 $(SRCROOT)/*


install-MacOSX: $(OBJROOT)/libgcc_s.dylib
	mkdir -p $(DSTROOT)/usr/lib
	cp $(OBJROOT)/libgcc_s.dylib $(DSTROOT)/usr/lib/libgcc_s.10.5.dylib 
	cd $(DSTROOT)/usr/lib; \
	ln -s libgcc_s.10.5.dylib libgcc_s.10.4.dylib; \
	ln -s libSystem.B.dylib libgcc_s.1.dylib; \
	

install-iPhoneOS : $(SYMROOT)/libgcc_s.1.dylib 
	mkdir -p $(DSTROOT)/usr/lib
	$(STRIP) -S $(SYMROOT)/libgcc_s.1.dylib -o $(DSTROOT)/usr/lib/libgcc_s.1.dylib

$(SYMROOT)/libgcc_s.1.dylib :  $(SRCROOT)/reexports.exp
	$(CC) $(ARCH_CFLAGS) -dynamiclib -install_name /usr/lib/libgcc_s.1.dylib \
		-compatibility_version 1 -current_version ${RC_ProjectSourceVersion} \
        -Wl,-reexported_symbols_list,$(SRCROOT)/reexports.exp \
		-nostdlib -o $@ -Wl,-upward-lSystem -isysroot $(SDK_DIR)



