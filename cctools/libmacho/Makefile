RC_OS = macos
OFLAG = -O
OPEN = -D__OPEN_SOURCE__
LEGACY = -D_MACH_I386_THREAD_STATUS_FPSTATE_LEGACY_FIELD_NAMES_ \
	 -D_ARCHITECTURE_I386_FPU_FPSTATE_LEGACY_FIELD_NAMES_

ifeq "" "$(NEXT_ROOT)"
KERN = -fno-builtin -finline -fno-keep-inline-functions -fsigned-bitfields \
       -msoft-float -nostdinc \
       -iwithsysroot /System/Library/Frameworks/Kernel.framework/PrivateHeaders/libsa \
       -iwithsysroot /System/Library/Frameworks/Kernel.framework/PrivateHeaders \
       -iwithsysroot /System/Library/Frameworks/Kernel.framework/Headers/libsa \
       -iwithsysroot /System/Library/Frameworks/Kernel.framework/Headers/bsd \
       -iwithsysroot /System/Library/Frameworks/Kernel.framework/Headers \
       -DKERNEL -DKERNEL_PRIVATE -DAPPLE -DNeXT -DLIBSA_PRIVATE
else
KERN = -fno-builtin -finline -fno-keep-inline-functions -fsigned-bitfields \
       -msoft-float -nostdinc \
-I$(NEXT_ROOT)/System/Library/Frameworks/Kernel.framework/PrivateHeaders/libsa \
-I$(NEXT_ROOT)/System/Library/Frameworks/Kernel.framework/PrivateHeaders \
       -I$(NEXT_ROOT)/System/Library/Frameworks/Kernel.framework/Headers/libsa \
       -I$(NEXT_ROOT)/System/Library/Frameworks/Kernel.framework/Headers/bsd \
       -I$(NEXT_ROOT)/System/Library/Frameworks/Kernel.framework/Headers \
       -DKERNEL -DKERNEL_PRIVATE -DAPPLE -DNeXT -DLIBSA_PRIVATE
endif

ifeq "macos" "$(RC_OS)"
  CC := $(shell if [ "$(RC_RELEASE)" == "Tiger" ] || \
		   [ "$(RC_RELEASE)" == "Gordian" ] || \
		   [ "$(RC_RELEASE)" == "Saffron" ] || \
		   [ "$(RC_USE_GCC35_FOR_PPC64)" == "YES" ]; then \
		     echo gcc-3.5 ; \
		else \
		     echo "$(CC)" ; \
	     fi; )
endif
ifeq "mwccppc" "$(notdir $(CC))"
  CFLAGS = $(OFLAG) $(OPEN) -g $(LEGACY) \
     -I$(SRCROOT) -I$(SRCROOT)/../include -I$(OFILE_DIR)
else
  CFLAGS = $(OFLAG) $(OPEN) -g -Wall $(X_CFLAGS) $(LEGACY) \
     -I$(SRCROOT) -I$(SRCROOT)/../include -I$(OFILE_DIR)
endif

USE_DEPENDENCY_FILE := $(shell if [ "$(notdir $(CC))" != "mwccppc" ] && \
				  [ "$(RC_XBS)" != "YES" ]; then \
	     echo YES ; else echo NO ; \
	     fi; )

ifneq "" "$(wildcard /bin/mkdirs)"
  MKDIRS = /bin/mkdirs
else
  MKDIRS = /bin/mkdir -p
endif

SRCROOT = .
SYMROOT = .
OBJROOT = .
OFILE_DIRS = $(OBJROOT)/shlib_obj $(OBJROOT)/profile_obj \
	     $(OBJROOT)/optimized_obj $(OBJROOT)/dynamic_obj \
	     $(OBJROOT)/ptmp_obj $(OBJROOT)/otmp_obj $(OBJROOT)/dtmp_obj \
	     $(OBJROOT)/kld_obj $(OBJROOT)/ktmp_obj
OFILE_DIR = $(OBJROOT)
VPATH = $(OFILE_DIR)

ifeq "macos" "$(RC_OS)"
  SYSTEMDIR := $(shell if [ "$(RC_RELEASE)" = "Beaker" ] || \
			  [ "$(RC_RELEASE)" = "Bunsen" ] || \
			  [ "$(RC_RELEASE)" = "Gonzo" ]; then \
	 echo /Local/Developer/System; else echo /usr/local/lib/system; fi; )
else
  SYSTEMDIR = /Local/Developer/System
endif

CFILES = arch.c getsecbyname.c getsegbyname.c get_end.c \
	 swap.c hppa_swap.c i386_swap.c m68k_swap.c sparc_swap.c \
	 ppc_swap.c i860_swap.c m88k_swap.c

INSTALL_FILES = $(CFILES) $(HFILES) shlib.ofileList dylib.ofileList Makefile \
		notes

OBJS = arch.o getsecbyname.o getsegbyname.o get_end.o swap.o \
       hppa_swap.o i386_swap.o m68k_swap.o sparc_swap.o ppc_swap.o \
       i860_swap.o m88k_swap.o vers.o

ifeq "$(OLD_LIBKLD)" "YES"
	KLD_OFILES = kld_ofiles
else
	KLD_OFILES = 
endif

all: lib_ofiles

macos: optimized_ofiles $(KLD_OFILES)

teflon: optimized_ofiles $(KLD_OFILES)

nextstep: optimized_ofiles $(KLD_OFILES) shlib_ofiles

lib_ofiles: $(OFILE_DIRS) $(SYMROOT) $(RC_OS) dynamic_ofiles profile_ofiles

shlib_ofiles: $(OBJROOT)/shlib_obj
	@if [ $(SRCROOT) = . ];						\
	then								\
	    (cd shlib_obj;						\
		$(MAKE) -f ../Makefile $(OBJS)				\
		VPATH=.. 						\
		OFILE_DIR=.						\
		SRCROOT=..						\
		COPTS="-static -DSHLIB -I/LocalDeveloper/Headers/libsys" \
		LIBSTUFF="-L../../libstuff -lstuff_s"			\
		OFLAG="$(OFLAG)"					\
		RC_CFLAGS="$(RC_CFLAGS)"				\
		RC_ARCHS="$(RC_ARCHS)");				\
	    rm -f shlib_obj/libmacho.ofileList;				\
	    cp shlib.ofileList shlib_obj/libmacho.ofileList;		\
	else								\
	    (cd $(OBJROOT)/shlib_obj;					\
		$(MAKE) -f $(SRCROOT)/Makefile $(OBJS)			\
		VPATH=$(SRCROOT)					\
		OFILE_DIR=$(OBJROOT)/shlib_obj				\
		SRCROOT=$(SRCROOT)					\
		SYMROOT=$(SYMROOT)					\
		COPTS="-static -DSHLIB -I/LocalDeveloper/Headers/libsys" \
		LIBSTUFF="-L$(SYMROOT)/../libstuff -lstuff_s"		\
		OFLAG="$(OFLAG)"					\
		RC_CFLAGS="$(RC_CFLAGS)"				\
		RC_ARCHS="$(RC_ARCHS)");				\
	    rm -f $(OBJROOT)/shlib_obj/libmacho.ofileList;		\
	    cp shlib.ofileList $(OBJROOT)/shlib_obj/libmacho.ofileList;	\
	fi

profile_ofiles:	$(OBJROOT)/profile_obj $(OBJROOT)/ptmp_obj
	@if [ $(SRCROOT) = . ];						\
	then								\
	    (cd ptmp_obj;						\
		$(MAKE) -f ../Makefile libmacho_pg.a			\
		VPATH=.. 						\
		OFILE_DIR=.						\
		SRCROOT=..						\
		COPTS="-dynamic -pg"					\
		LIBSTUFF="-L../../libstuff -lstuff_p"			\
		OFLAG="$(OFLAG)"					\
		RC_CFLAGS="$(RC_CFLAGS)"				\
		RC_ARCHS="$(RC_ARCHS)");				\
	    (cd profile_obj; rm -f $(OBJS));				\
	    (cd ptmp_obj; cp $(OBJS) ../profile_obj); \
	    rm -f dynamic_profile_obj;					\
	    ln -s ./profile_obj dynamic_profile_obj;			\
	    rm -f profile_obj/libmacho.ofileList;			\
	    cp dylib.ofileList profile_obj/libmacho.ofileList;		\
	else								\
	    (cd $(OBJROOT)/ptmp_obj;					\
		$(MAKE) -f $(SRCROOT)/Makefile libmacho_pg.a		\
		VPATH=$(SRCROOT)					\
		OFILE_DIR=$(OBJROOT)/ptmp_obj				\
		SRCROOT=$(SRCROOT)					\
		SYMROOT=$(SYMROOT)					\
		LIBSTUFF="-L$(SYMROOT)/../libstuff -lstuff_p"		\
		COPTS="-dynamic -pg"					\
		OFLAG="$(OFLAG)"					\
		RC_CFLAGS="$(RC_CFLAGS)"				\
		RC_ARCHS="$(RC_ARCHS)");				\
	    (cd $(OBJROOT)/profile_obj; rm -f $(OBJS));			\
	    (cd $(OBJROOT)/ptmp_obj; 					\
		cp $(OBJS) $(OBJROOT)/profile_obj); \
	    rm -f $(OBJROOT)/dynamic_profile_obj;			\
	    ln -s ./profile_obj $(OBJROOT)/dynamic_profile_obj;		\
	    rm -f $(OBJROOT)/profile_obj/libmacho.ofileList;		\
	    cp dylib.ofileList $(OBJROOT)/profile_obj/libmacho.ofileList; \
	fi

optimized_ofiles: $(OBJROOT)/optimized_obj $(OBJROOT)/otmp_obj
	@if [ $(SRCROOT) = . ];						\
	then								\
	    (cd otmp_obj;						\
		$(MAKE) -f ../Makefile libmacho_static.a		\
		VPATH=.. 						\
		OFILE_DIR=.						\
		SRCROOT=..						\
		COPTS="-static"						\
		LIBSTUFF="-L../../libstuff -lstuff"			\
		OFLAG="$(OFLAG)"					\
		RC_CFLAGS="$(RC_CFLAGS)"				\
		RC_ARCHS="$(RC_ARCHS)");				\
	    (cd optimized_obj; rm -f $(OBJS));				\
	    (cd otmp_obj; cp $(OBJS) ../optimized_obj); 		\
	    rm -f static_obj;						\
	    ln -s ./optimized_obj static_obj;				\
	    rm -f optimized_obj/libmacho.ofileList;			\
	    cp dylib.ofileList optimized_obj/libmacho.ofileList;	\
	else								\
	    (cd $(OBJROOT)/otmp_obj;					\
		$(MAKE) -f $(SRCROOT)/Makefile libmacho_static.a	\
		VPATH=$(SRCROOT)					\
		OFILE_DIR=$(OBJROOT)/otmp_obj				\
		SRCROOT=$(SRCROOT)					\
		SYMROOT=$(SYMROOT)					\
		COPTS="-static"						\
		LIBSTUFF="-L$(SYMROOT)/../libstuff -lstuff"		\
		OFLAG="$(OFLAG)"					\
		RC_CFLAGS="$(RC_CFLAGS)"				\
		RC_ARCHS="$(RC_ARCHS)");				\
	    (cd $(OBJROOT)/optimized_obj; rm -f $(OBJS));		\
	    (cd $(OBJROOT)/otmp_obj; 					\
		cp $(OBJS) $(OBJROOT)/optimized_obj); 			\
	    rm -f $(OBJROOT)/static_obj;				\
	    ln -s ./optimized_obj $(OBJROOT)/static_obj;		\
	    rm -f $(OBJROOT)/optimized_obj/libmacho.ofileList;		\
	    cp dylib.ofileList $(OBJROOT)/optimized_obj/libmacho.ofileList; \
	fi

kld_ofiles: $(OBJROOT)/kld_obj $(OBJROOT)/ktmp_obj
	@if [ $(SRCROOT) = . ];						\
	then								\
	    (cd ktmp_obj;						\
		$(MAKE) -f ../Makefile libmacho_kld.a			\
		VPATH=.. 						\
		OFILE_DIR=.						\
		SRCROOT=..						\
		COPTS="-static -DRLD -DSA_RLD $(KERN)"			\
		LIBSTUFF="-L../../libstuff -lstuff_kld"			\
		OFLAG="$(OFLAG)"					\
		RC_CFLAGS="$(RC_CFLAGS)"				\
		RC_ARCHS="$(RC_ARCHS)");				\
	    (cd kld_obj; rm -f $(OBJS));				\
	    (cd ktmp_obj; cp $(OBJS) ../kld_obj); 			\
	else								\
	    (cd $(OBJROOT)/ktmp_obj;					\
		$(MAKE) -f $(SRCROOT)/Makefile libmacho_kld.a		\
		VPATH=$(SRCROOT)					\
		OFILE_DIR=$(OBJROOT)/ktmp_obj				\
		SRCROOT=$(SRCROOT)					\
		SYMROOT=$(SYMROOT)					\
		COPTS="-static -DRLD -DSA_RLD $(KERN)"			\
		LIBSTUFF="-L$(SYMROOT)/../libstuff -lstuff_kld"		\
		OFLAG="$(OFLAG)"					\
		RC_CFLAGS="$(RC_CFLAGS)"				\
		RC_ARCHS="$(RC_ARCHS)");				\
	    (cd $(OBJROOT)/kld_obj; rm -f $(OBJS));		\
	    (cd $(OBJROOT)/ktmp_obj; 					\
		cp $(OBJS) $(OBJROOT)/kld_obj); 			\
	fi

dynamic_ofiles: $(OBJROOT)/dynamic_obj $(OBJROOT)/dtmp_obj
	@if [ $(SRCROOT) = . ];						\
	then								\
	    (cd dtmp_obj;						\
		$(MAKE) -f ../Makefile libmacho.a			\
		VPATH=.. 						\
		OFILE_DIR=.						\
		SRCROOT=..						\
		COPTS="-dynamic"					\
		LIBSTUFF="-L../../libstuff -lstuff"			\
		OFLAG="$(OFLAG)"					\
		RC_CFLAGS="$(RC_CFLAGS)"				\
		RC_ARCHS="$(RC_ARCHS)");				\
	    (cd dynamic_obj; rm -f $(OBJS));				\
	    (cd dtmp_obj; cp $(OBJS) ../dynamic_obj); 			\
	    rm -f dynamic_obj/libmacho.ofileList;			\
	    cp dylib.ofileList dynamic_obj/libmacho.ofileList;		\
	else								\
	    (cd $(OBJROOT)/dtmp_obj;					\
		$(MAKE) -f $(SRCROOT)/Makefile libmacho.a		\
		VPATH=$(SRCROOT)					\
		OFILE_DIR=$(OBJROOT)/dtmp_obj				\
		SRCROOT=$(SRCROOT)					\
		SYMROOT=$(SYMROOT)					\
		LIBSTUFF="-L$(SYMROOT)/../libstuff -lstuff"		\
		COPTS="-dynamic"					\
		OFLAG="$(OFLAG)"					\
		RC_CFLAGS="$(RC_CFLAGS)"				\
		RC_ARCHS="$(RC_ARCHS)");				\
	    (cd $(OBJROOT)/dynamic_obj; rm -f $(OBJS));			\
	    (cd $(OBJROOT)/dtmp_obj;					\
		cp $(OBJS) $(OBJROOT)/dynamic_obj); 			\
	    rm -f $(OBJROOT)/dynamic_obj/libmacho.ofileList;		\
	    cp dylib.ofileList $(OBJROOT)/dynamic_obj/libmacho.ofileList; \
	fi

ifeq "NO" "$(USE_DEPENDENCY_FILE)"
.c.o:
	$(CC) $(CFLAGS) $(RC_CFLAGS) $(COPTS) -c -o $(OFILE_DIR)/$*.o $<
else
.c.o:
	$(CC) $(CFLAGS) $(RC_CFLAGS) $(COPTS) -c \
		-MD -dependency-file $(OFILE_DIR)/$*.d \
		-o $(OFILE_DIR)/$*.o $<
	md -d -u $(OFILE_DIR)/Makedep $(OFILE_DIR)/$*.d
endif

vers.c:
	vers_string -l $(VERS_STRING_FLAGS) libmacho > $(OFILE_DIR)/$@
	echo "const char *libmacho_version= SGS_VERS;" >> $(OFILE_DIR)/$@

clean:
	-rm -r -f $(OFILE_DIRS) $(OBJROOT)/static_obj

shlib_clean:
	-rm -r -f $(OBJROOT)/ptmp_obj $(OBJROOT)/otmp_obj $(OBJROOT)/dtmp_obj

libmacho.a libmacho_pg.a libmacho_static.a libmacho_kld.a: $(OBJS)
	libtool -static -o $(SYMROOT)/$@ $(OBJS)


install: $(RC_OS)_install

macos_install:

teflon_install nextstep_install: all


lib_ofiles_install: $(RC_OS)_lib_ofiles_install

macos_lib_ofiles_install: all
	$(MKDIRS) $(DSTROOT)$(SYSTEMDIR)
	if [ $(SRCROOT) = . ];						\
	then								\
	    install -c -m 444 dtmp_obj/libmacho.a			\
		     $(DSTROOT)$(SYSTEMDIR)/libmacho.a;			\
	    ranlib $(DSTROOT)$(SYSTEMDIR)/libmacho.a;			\
	    (cd $(DSTROOT)$(SYSTEMDIR) ;				\
	    rm -f libmacho_debug.a ; ln -s libmacho.a libmacho_debug.a); \
	    install -c -m 444 ptmp_obj/libmacho_pg.a			 \
		     $(DSTROOT)$(SYSTEMDIR)/libmacho_profile.a;		\
	    ranlib $(DSTROOT)$(SYSTEMDIR)/libmacho_profile.a;		\
	    install -c -m 444 otmp_obj/libmacho_static.a		\
		     $(DSTROOT)$(SYSTEMDIR)/libmacho_static.a;		\
	    ranlib $(DSTROOT)$(SYSTEMDIR)/libmacho_static.a;		\
	else								\
	    install -c -m 444 $(SYMROOT)/libmacho.a			\
		     $(DSTROOT)$(SYSTEMDIR)/libmacho.a;			\
	    ranlib $(DSTROOT)$(SYSTEMDIR)/libmacho.a;			\
	    (cd $(DSTROOT)$(SYSTEMDIR) ;				\
	    rm -f libmacho_debug.a ; ln -s libmacho.a libmacho_debug.a); \
	    install -c -m 444 $(SYMROOT)/libmacho_pg.a			 \
		     $(DSTROOT)$(SYSTEMDIR)/libmacho_profile.a;		\
	    ranlib $(DSTROOT)$(SYSTEMDIR)/libmacho_profile.a;		\
	    install -c -m 444 $(SYMROOT)/libmacho_static.a		\
		     $(DSTROOT)$(SYSTEMDIR)/libmacho_static.a;		\
	    ranlib $(DSTROOT)$(SYSTEMDIR)/libmacho_static.a;		\
	fi

teflon_lib_ofiles_install nextstep_lib_ofiles_install:


installsrc:
	$(MKDIRS) $(SRCROOT)
	chmod 755 $(SRCROOT)
	gnutar cf - $(INSTALL_FILES) | (cd $(SRCROOT); gnutar xf -)
	chmod 444 $(SRCROOT)/*

$(OFILE_DIRS) $(SYMROOT):
	$(MKDIRS) $@

-include $(OFILE_DIR)/Makedep
