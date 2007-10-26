OFLAG = -Os
CFLAGS = $(OFLAG) -Wall $(RC_NONARCH_CFLAGS)
CFLAGS_DYNANMIC_NO_PIC_ppc    = -mdynamic-no-pic
CFLAGS_DYNANMIC_NO_PIC_ppc64  = 
CFLAGS_DYNANMIC_NO_PIC_i386   = -mdynamic-no-pic
CFLAGS_DYNANMIC_NO_PIC_x86_64 = 

SRCROOT = .
SYMROOT = .
OBJROOT = .

PAX = /bin/pax -rw
MKDIR = /bin/mkdir -p
CHMOD = /bin/chmod
LIPO = /usr/bin/lipo

ifeq (,$(RC_ARCHS))
# build for the local arch only
STATIC_ARCH_CFLAGS =
DYNAMIC_ARCH_CFLAGS =
else
# assume the toolchain supports static compilation for all request archs
STATIC_ARCH_CFLAGS = $(patsubst %,-arch %,$(RC_ARCHS))
DYNAMIC_ARCH_CFLAGS = $(patsubst %,-arch %,$(RC_ARCHS))
endif

USRLIBDIR = /usr/lib
LOCLIBDIR = /usr/local/lib
DSTDIRS = $(DSTROOT)$(USRLIBDIR) $(DSTROOT)$(LOCLIBDIR)

CFILES = crt.c icplusplus.c 
SFILES = start.s dyld_glue.s
INSTALLSRC_FILES = $(CFILES) $(SFILES) Makefile

USRLIB_INSTALL_FILES = $(SYMROOT)/crt1.o $(SYMROOT)/crt1.10.5.o $(SYMROOT)/gcrt1.o $(SYMROOT)/dylib1.o $(SYMROOT)/dylib1.10.5.o $(SYMROOT)/bundle1.o
LOCLIB_INSTALL_FILES = $(SYMROOT)/crt0.o 

# default target for development builds
all: $(USRLIB_INSTALL_FILES) $(LOCLIB_INSTALL_FILES)

# rules
$(OBJROOT)/%.static.o : %.c
	$(CC) -static -c $(CFLAGS) $(STATIC_ARCH_CFLAGS) $^ -o $@
	
$(OBJROOT)/%.static.o : %.s
	$(CC) -static -c $(CFLAGS) $(STATIC_ARCH_CFLAGS) $^ -o $@

$(OBJROOT)/%.dynamic_no_pic.o : %.c
ifeq (,$(RC_ARCHS))
	$(CC) -c $(CFLAGS) $(DYNAMIC_ARCH_CFLAGS) -mmacosx-version-min=10.3 $^ -o $@
else
	$(foreach arch,$(RC_ARCHS), $(CC) -arch $(arch) $(CFLAGS_DYNANMIC_NO_PIC_$(arch)) -mmacosx-version-min=10.3 -c $(CFLAGS) $^ -o $@.$(arch); )
	$(LIPO) -create  $(patsubst %, $@.%,$(RC_ARCHS)) -o $@
endif
	
$(OBJROOT)/%.10.5.pic.o : %.c
	$(CC) -c $(CFLAGS) -mmacosx-version-min=10.5 $(DYNAMIC_ARCH_CFLAGS) $^ -o $@

$(OBJROOT)/%.dynamic_no_pic.o : %.s
	$(CC)  -DMACH_HEADER_SYMBOL_NAME=__mh_execute_header -mdynamic-no-pic -mmacosx-version-min=10.3 -c $(CFLAGS) $(DYNAMIC_ARCH_CFLAGS) $^ -o $@

$(OBJROOT)/%.profile.dynamic_no_pic.o : %.c
	$(CC) -mdynamic-no-pic -DGCRT -mmacosx-version-min=10.3 -c $(CFLAGS) $(DYNAMIC_ARCH_CFLAGS) $^ -o $@

$(OBJROOT)/%.pic.o : %.c
	$(CC) -c $(CFLAGS) $(DYNAMIC_ARCH_CFLAGS) -mmacosx-version-min=10.3 $^ -o $@

$(OBJROOT)/%.pic.o : %.s
	$(CC) -c $(CFLAGS) $(DYNAMIC_ARCH_CFLAGS) $^ -o $@

$(OBJROOT)/%.crt.pic.o : %.s
	$(CC)  -DMACH_HEADER_SYMBOL_NAME=__mh_execute_header -DCRT -c $(CFLAGS) $(DYNAMIC_ARCH_CFLAGS) $^ -o $@

$(OBJROOT)/%.bundle.pic.o : %.s
	$(CC)  -DMACH_HEADER_SYMBOL_NAME=__mh_bundle_header -c $(CFLAGS) $(DYNAMIC_ARCH_CFLAGS) $^ -o $@

$(OBJROOT)/%.dylib.pic.o : %.s
	$(CC)  -DMACH_HEADER_SYMBOL_NAME=__mh_dylib_header -DCFM_GLUE -c $(CFLAGS) $(DYNAMIC_ARCH_CFLAGS) $^ -o $@

vpath %.s $(SRCROOT)


# targets
$(SYMROOT)/crt1.o: $(OBJROOT)/start.dynamic_no_pic.o $(OBJROOT)/crt.dynamic_no_pic.o $(OBJROOT)/dyld_glue.dynamic_no_pic.o 
	$(CC) -r $(DYNAMIC_ARCH_CFLAGS) -mmacosx-version-min=10.3 -nostdlib -keep_private_externs $^ /usr/lib/dyld -o $@ 

$(SYMROOT)/crt1.10.5.o: $(OBJROOT)/start.pic.o $(OBJROOT)/crt.10.5.pic.o $(OBJROOT)/dyld_glue.crt.pic.o 
	$(CC) -r $(DYNAMIC_ARCH_CFLAGS) -nostdlib -keep_private_externs $^ -o $@ 

$(SYMROOT)/gcrt1.o: $(OBJROOT)/start.dynamic_no_pic.o $(OBJROOT)/crt.profile.dynamic_no_pic.o $(OBJROOT)/dyld_glue.dynamic_no_pic.o
	$(CC) -r $(DYNAMIC_ARCH_CFLAGS) -mmacosx-version-min=10.3 -nostdlib -keep_private_externs $^ /usr/lib/dyld -o $@ 

$(SYMROOT)/bundle1.o: $(OBJROOT)/dyld_glue.bundle.pic.o
	cp	$^ $@
	
$(SYMROOT)/dylib1.o: $(OBJROOT)/dyld_glue.dylib.pic.o $(OBJROOT)/icplusplus.pic.o
	$(CC) -r $(DYNAMIC_ARCH_CFLAGS) -mmacosx-version-min=10.3  -nostdlib -keep_private_externs $^ -o $@ 

$(SYMROOT)/dylib1.10.5.o: $(OBJROOT)/dyld_glue.dylib.pic.o 
	cp	$^ $@

$(SYMROOT)/crt0.o:   $(OBJROOT)/start.static.o $(OBJROOT)/crt.static.o 
	$(CC) -r $(DYNAMIC_ARCH_CFLAGS) -nostdlib -keep_private_externs $^ -o $@ 



clean:
	rm -f $(OBJROOT)/*.o $(SYMROOT)/*.o


install: all $(DSTDIRS)
	cp $(SYMROOT)/crt1.o		$(DSTROOT)$(USRLIBDIR)/crt1.o
	cp $(SYMROOT)/crt1.10.5.o	$(DSTROOT)$(USRLIBDIR)/crt1.10.5.o
	cp $(SYMROOT)/gcrt1.o		$(DSTROOT)$(USRLIBDIR)/gcrt1.o
	cp $(SYMROOT)/dylib1.o		$(DSTROOT)$(USRLIBDIR)/dylib1.o
	cp $(SYMROOT)/dylib1.10.5.o 	$(DSTROOT)$(USRLIBDIR)/dylib1.10.5.o
	cp $(SYMROOT)/bundle1.o		$(DSTROOT)$(USRLIBDIR)/bundle1.o
	cp $(SYMROOT)/crt0.o		$(DSTROOT)$(LOCLIBDIR)/crt0.o

installhdrs:

installsrc:
	$(MKDIR) $(SRCROOT)
	$(CHMOD) 755 $(SRCROOT)
	$(PAX) $(INSTALLSRC_FILES) $(SRCROOT)
	$(CHMOD) 444 $(SRCROOT)/*

$(OJBROOT) $(SYMROOT) $(DSTDIRS):
	$(MKDIR) $@

