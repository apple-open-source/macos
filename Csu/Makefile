OFLAG = -Os
CFLAGS = $(OFLAG) -Wall $(RC_NONARCH_CFLAGS)

SRCROOT = .
SYMROOT = .
OBJROOT = .

PAX = /bin/pax -rw
MKDIR = /bin/mkdir -p
CHMOD = /bin/chmod
LIPO = /usr/bin/lipo

ifeq (,$(RC_ARCHS))
	# build for the local arch only
	ARCH_CFLAGS =
else
	# assume the toolchain supports static compilation for all request archs
	ARCH_CFLAGS = $(patsubst %,-arch %,$(RC_ARCHS))
endif

# v1 = MacOSX 10.4 and earlier, iPhone n/a
# v2 = MacOSX 10.5 and later, iPhone 2.0 or later
# v3 = MacOSX 10.6 and later, iPhone 2.0 or later

ifeq (,$(RC_PURPLE))
	OS_MIN_V1	= -mmacosx-version-min=10.4
	OS_MIN_V2	= -mmacosx-version-min=10.5
	OS_MIN_V3	= -mmacosx-version-min=10.6
	INSTALL_TARGET  = install_macosx
else
	OS_MIN_V2	= -miphoneos-version-min=2.0
	OS_MIN_V3	= -miphoneos-version-min=2.0
	INSTALL_TARGET  = install_iphone
endif

USRLIBDIR = /usr/lib
LOCLIBDIR = /usr/local/lib
DSTDIRS = $(DSTROOT)$(USRLIBDIR) $(DSTROOT)$(LOCLIBDIR)

INSTALLSRC_FILES = Makefile crt.c icplusplus.c lazy_dylib_loader.c start.s dyld_glue.s lazy_dylib_helper.s

INTERMEDIATE_FILES =	$(SYMROOT)/crt1.v1.o  $(SYMROOT)/crt1.v2.o $(SYMROOT)/crt1.v3.o \
			$(SYMROOT)/gcrt1.o $(SYMROOT)/crt0.o \
			$(SYMROOT)/dylib1.v1.o $(SYMROOT)/dylib1.v2.o \
			$(SYMROOT)/bundle1.v1.o \
			$(SYMROOT)/lazydylib1.o

# default target for development builds
all: $(INTERMEDIATE_FILES) 


$(SYMROOT)/crt1.v1.o: start.s crt.c dyld_glue.s 
	$(CC) -r $(ARCH_CFLAGS) -Os $(OS_MIN_V1) -mdynamic-no-pic -nostdlib -keep_private_externs $^ -o $@  -DCRT -DOLD_LIBSYSTEM_SUPPORT

$(SYMROOT)/crt1.v2.o: start.s crt.c dyld_glue.s 
	$(CC) -r $(ARCH_CFLAGS) -Os $(OS_MIN_V2) -nostdlib -keep_private_externs $^ -o $@  -DCRT

$(SYMROOT)/crt1.v3.o: start.s crt.c
	$(CC) -r $(ARCH_CFLAGS) -Os $(OS_MIN_V3) -nostdlib -keep_private_externs $^ -o $@  -DADD_PROGRAM_VARS 


$(SYMROOT)/gcrt1.o: start.s crt.c dyld_glue.s 
	$(CC) -r $(ARCH_CFLAGS) -Os $(OS_MIN_V1) -nostdlib -keep_private_externs $^ -o $@  -DGCRT  -DOLD_LIBSYSTEM_SUPPORT


$(SYMROOT)/crt0.o: start.s crt.c
	$(CC) -r $(ARCH_CFLAGS) -Os -static -nostdlib -keep_private_externs $^ -o $@ 


$(SYMROOT)/dylib1.v1.o: dyld_glue.s icplusplus.c
	$(CC) -r $(ARCH_CFLAGS) -Os $(OS_MIN_V1)  -nostdlib -keep_private_externs $^ -o $@  -DCFM_GLUE

$(SYMROOT)/dylib1.v2.o: dyld_glue.s
	$(CC) -r $(ARCH_CFLAGS) -Os $(OS_MIN_V2)  -nostdlib -keep_private_externs $^ -o $@  -DCFM_GLUE
		

$(SYMROOT)/bundle1.v1.o: dyld_glue.s
	$(CC) -r $(ARCH_CFLAGS) -Os $(OS_MIN_V1)  -nostdlib -keep_private_externs $^ -o $@ 



$(SYMROOT)/lazydylib1.o: lazy_dylib_helper.s lazy_dylib_loader.c 
	$(CC) -r $(ARCH_CFLAGS) -Os -nostdlib -keep_private_externs $^ -o $@ 

clean:
	rm -f $(OBJROOT)/*.o $(SYMROOT)/*.o


install: all $(DSTDIRS) $(INSTALL_TARGET)


install_iphone:
	cp $(SYMROOT)/crt1.v2.o		$(DSTROOT)$(USRLIBDIR)/crt1.o
	cp $(SYMROOT)/dylib1.v2.o	$(DSTROOT)$(USRLIBDIR)/dylib1.o
	cp $(SYMROOT)/bundle1.v2o	$(DSTROOT)$(USRLIBDIR)/bundle1.o
	cp $(SYMROOT)/lazydylib1.o	$(DSTROOT)$(USRLIBDIR)/lazydylib1.o
	cp $(SYMROOT)/gcrt1.o		$(DSTROOT)$(USRLIBDIR)/gcrt1.o
	cp $(SYMROOT)/crt0.o		$(DSTROOT)$(LOCLIBDIR)/crt0.o


install_macosx:
	cp $(SYMROOT)/crt1.v3.o		$(DSTROOT)$(USRLIBDIR)/crt1.10.6.o
	cp $(SYMROOT)/crt1.v2.o		$(DSTROOT)$(USRLIBDIR)/crt1.10.5.o
	cp $(SYMROOT)/crt1.v1.o		$(DSTROOT)$(USRLIBDIR)/crt1.o
	cp $(SYMROOT)/gcrt1.o		$(DSTROOT)$(USRLIBDIR)/gcrt1.o
	cp $(SYMROOT)/dylib1.v2.o	$(DSTROOT)$(USRLIBDIR)/dylib1.10.5.o
	cp $(SYMROOT)/dylib1.v1.o 	$(DSTROOT)$(USRLIBDIR)/dylib1.o
	cp $(SYMROOT)/bundle1.v1.o	$(DSTROOT)$(USRLIBDIR)/bundle1.o
	cp $(SYMROOT)/lazydylib1.o	$(DSTROOT)$(USRLIBDIR)/lazydylib1.o
	cp $(SYMROOT)/crt0.o		$(DSTROOT)$(LOCLIBDIR)/crt0.o

installhdrs:

installsrc:
	$(MKDIR) $(SRCROOT)
	$(CHMOD) 755 $(SRCROOT)
	$(PAX) $(INSTALLSRC_FILES) $(SRCROOT)
	$(CHMOD) 444 $(SRCROOT)/*

$(OJBROOT) $(SYMROOT) $(DSTDIRS):
	$(MKDIR) $@

