LEGACY = -D_MACH_I386_THREAD_STATUS_FPSTATE_LEGACY_FIELD_NAMES_ \
	 -D_ARCHITECTURE_I386_FPU_FPSTATE_LEGACY_FIELD_NAMES_

KERN = -fno-builtin -finline -fno-keep-inline-functions -fsigned-bitfields \
       -msoft-float -nostdinc \
-I$(NEXT_ROOT)/System/Library/Frameworks/Kernel.framework/PrivateHeaders/libsa \
-I$(NEXT_ROOT)/System/Library/Frameworks/Kernel.framework/PrivateHeaders \
       -I$(NEXT_ROOT)/System/Library/Frameworks/Kernel.framework/Headers/libsa \
       -I$(NEXT_ROOT)/System/Library/Frameworks/Kernel.framework/Headers/bsd \
       -I$(NEXT_ROOT)/System/Library/Frameworks/Kernel.framework/Headers \
       -DKERNEL -DKERNEL_PRIVATE -DAPPLE -DNeXT -DLIBSA_PRIVATE

ifeq "cctools_ofiles_Sim" "$(RC_ProjectName)"
  S = _sim
endif

DEPENDENT_LIBS = -nodefaultlibs -umbrella System -L/usr/lib/system \
		 -ldyld -lsystem_malloc -lsystem_c -lsystem_kernel -lcompiler_rt

ifneq "" "$(SDKROOT)"
  SDK = -isysroot $(SDKROOT)
  CC = $(shell xcrun -find -sdk $(SDKROOT) cc)
  STRIP = $(shell xcrun -find -sdk ${SDKROOT} strip)
  DSYMUTIL = $(shell xcrun -find -sdk ${SDKROOT} dsymutil)
else
  STRIP = strip
  DSYMUTIL = dsymutil
endif

OFLAG = -Os
CFLAGS = $(OFLAG) -g -Wall $(LEGACY) \
	 -I$(SRCROOT) -I$(SRCROOT)/../include -I$(OFILE_DIR) $(SDK)
PG = -pg

MKDIRS = /bin/mkdir -p
SRCROOT = .
SYMROOT = .
OBJROOT = .
OFILE_DIRS = $(OBJROOT)/shlib_obj $(OBJROOT)/profile_obj \
	     $(OBJROOT)/optimized_obj $(OBJROOT)/dynamic_obj \
	     $(OBJROOT)/ptmp_obj $(OBJROOT)/otmp_obj $(OBJROOT)/dtmp_obj \
	     $(OBJROOT)/kld_obj $(OBJROOT)/ktmp_obj
OFILE_DIR = $(OBJROOT)
VPATH = $(OFILE_DIR)

ARCHIVEDIR = /usr/local/lib/system
DYLIBDIR = /usr/lib/system

CFILES = arch.c getsecbyname.c getsegbyname.c get_end.c \
	 swap.c hppa_swap.c i386_swap.c m68k_swap.c sparc_swap.c \
	 ppc_swap.c i860_swap.c m88k_swap.c slot_name.c

INSTALL_FILES = $(CFILES) $(HFILES) shlib.ofileList dylib.ofileList Makefile \
		notes

OBJS = arch.o getsecbyname.o getsegbyname.o get_end.o swap.o \
       hppa_swap.o i386_swap.o m68k_swap.o sparc_swap.o ppc_swap.o \
       i860_swap.o m88k_swap.o slot_name.o

ifeq "$(OLD_LIBKLD)" "YES"
	KLD_OFILES = kld_ofiles
else
	KLD_OFILES =
endif

all: lib_ofiles

lib_ofiles: $(OFILE_DIRS) $(SYMROOT) optimized_ofiles $(KLD_OFILES) \
	    dynamic_ofiles profile_ofiles

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
		$(MAKE) -f ../Makefile libmacho_pg.a libmacho$S_profile.dylib \
		INSTALL_NAME=/usr/lib/system/libmacho$S_profile.dylib	\
		VPATH=.. 						\
		OFILE_DIR=.						\
		SRCROOT=..						\
		COPTS="-dynamic $(PG)"					\
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
		libmacho$S_profile.dylib				\
		INSTALL_NAME=/usr/lib/system/libmacho$S_profile.dylib	\
		VPATH=$(SRCROOT)					\
		OFILE_DIR=$(OBJROOT)/ptmp_obj				\
		SRCROOT=$(SRCROOT)					\
		SYMROOT=$(SYMROOT)					\
		LIBSTUFF="-L$(SYMROOT)/../libstuff -lstuff_p"		\
		COPTS="-dynamic $(PG)"					\
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
	    (cd $(OBJROOT)/kld_obj; rm -f $(OBJS));			\
	    (cd $(OBJROOT)/ktmp_obj; 					\
		cp $(OBJS) $(OBJROOT)/kld_obj); 			\
	fi

dynamic_ofiles: $(OBJROOT)/dynamic_obj $(OBJROOT)/dtmp_obj
	@if [ $(SRCROOT) = . ];						\
	then								\
	    (cd dtmp_obj;						\
		$(MAKE) -f ../Makefile libmacho.a libmacho$S.dylib	\
		INSTALL_NAME=/usr/lib/system/libmacho$S.dylib		\
		VPATH=.. 						\
		OFILE_DIR=.						\
		SRCROOT=..						\
		COPTS="-dynamic"					\
		LIBSTUFF="-L../../libstuff -lstuff"			\
		OFLAG="$(OFLAG)"					\
		RC_CFLAGS="$(RC_CFLAGS)"				\
		RC_ARCHS="$(RC_ARCHS)");				\
	    (cd dtmp_obj;						\
		$(MAKE) -f ../Makefile libmacho$S_debug.dylib		\
		INSTALL_NAME=/usr/lib/system/libmacho$S_debug.dylib	\
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
		$(MAKE) -f $(SRCROOT)/Makefile libmacho.a libmacho$S.dylib \
		INSTALL_NAME=/usr/lib/system/libmacho$S.dylib		\
		VPATH=$(SRCROOT)					\
		OFILE_DIR=$(OBJROOT)/dtmp_obj				\
		SRCROOT=$(SRCROOT)					\
		SYMROOT=$(SYMROOT)					\
		LIBSTUFF="-L$(SYMROOT)/../libstuff -lstuff"		\
		COPTS="-dynamic"					\
		OFLAG="$(OFLAG)"					\
		RC_CFLAGS="$(RC_CFLAGS)"				\
		RC_ARCHS="$(RC_ARCHS)");				\
	    (cd $(OBJROOT)/dtmp_obj;					\
		$(MAKE) -f $(SRCROOT)/Makefile libmacho$S_debug.dylib	\
		INSTALL_NAME=/usr/lib/system/libmacho$S_debug.dylib	\
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

.c.o:
	$(CC) $(CFLAGS) $(RC_CFLAGS) $(COPTS) -c -o $(OFILE_DIR)/$*.o $<

clean:
	-rm -r -f $(OFILE_DIRS) $(OBJROOT)/static_obj

shlib_clean:
	-rm -r -f $(OBJROOT)/ptmp_obj $(OBJROOT)/otmp_obj $(OBJROOT)/dtmp_obj

libmacho.a libmacho_pg.a libmacho_static.a libmacho_kld.a: $(OBJS)
	libtool -static -o $(SYMROOT)/$@ $(OBJS)

libmacho$S.dylib libmacho$S_profile.dylib libmacho$S_debug.dylib: $(OBJS)
	$(CC) $(CFLAGS) $(RC_CFLAGS) -dynamiclib  \
		-o $(SYMROOT)/$@ $(OBJS) \
		-install_name $(INSTALL_NAME) \
		-compatibility_version 1 \
		-current_version $(RC_ProjectSourceVersion) \
		$(DEPENDENT_LIBS)
	$(DSYMUTIL) $(SYMROOT)/$@

install:

lib_ofiles_install: all
	$(MKDIRS) $(DSTROOT)$(ARCHIVEDIR) $(DSTROOT)$(DYLIBDIR)
	@if [ $(SRCROOT) = . ];						\
	then								\
	    install -c -m 444 dtmp_obj/libmacho.a			\
		     $(DSTROOT)$(ARCHIVEDIR)/libmacho.a;		\
	    ranlib $(DSTROOT)$(ARCHIVEDIR)/libmacho.a;			\
	    (cd $(DSTROOT)$(ARCHIVEDIR) ;				\
	    rm -f libmacho_debug.a ; ln -s libmacho.a libmacho_debug.a); \
	    install -c -m 444 ptmp_obj/libmacho_pg.a			 \
		     $(DSTROOT)$(ARCHIVEDIR)/libmacho_profile.a;	\
	    ranlib $(DSTROOT)$(ARCHIVEDIR)/libmacho_profile.a;		\
	    install -c -m 444 otmp_obj/libmacho_static.a		\
		     $(DSTROOT)$(ARCHIVEDIR)/libmacho_static.a;		\
	    ranlib $(DSTROOT)$(ARCHIVEDIR)/libmacho_static.a;		\
	    install -c -m 555 dtmp_obj/libmacho$S.dylib			\
		     $(DSTROOT)$(DYLIBDIR)/libmacho$S.dylib;		\
	    $(STRIP) -S $(DSTROOT)$(DYLIBDIR)/libmacho$S.dylib;		\
	    install -c -m 555 dtmp_obj/libmacho$S_profile.dylib		\
		     $(DSTROOT)$(DYLIBDIR)/libmacho$S_profile.dylib;	\
	    $(STRIP) -S $(DSTROOT)$(DYLIBDIR)/libmacho$S_profile.dylib;	\
	    install -c -m 555 dtmp_obj/libmacho$S_debug.dylib		\
		     $(DSTROOT)$(DYLIBDIR)/libmacho$S_debug.dylib;	\
	    $(STRIP) -S $(DSTROOT)$(DYLIBDIR)/libmacho$S_debug.dylib;	\
	else								\
	    install -c -m 444 $(SYMROOT)/libmacho.a			\
		     $(DSTROOT)$(ARCHIVEDIR)/libmacho.a;		\
	    ranlib $(DSTROOT)$(ARCHIVEDIR)/libmacho.a;			\
	    (cd $(DSTROOT)$(ARCHIVEDIR) ;				\
	    rm -f libmacho_debug.a ; ln -s libmacho.a libmacho_debug.a); \
	    install -c -m 444 $(SYMROOT)/libmacho_pg.a			 \
		     $(DSTROOT)$(ARCHIVEDIR)/libmacho_profile.a;	\
	    ranlib $(DSTROOT)$(ARCHIVEDIR)/libmacho_profile.a;		\
	    install -c -m 444 $(SYMROOT)/libmacho_static.a		\
		     $(DSTROOT)$(ARCHIVEDIR)/libmacho_static.a;		\
	    ranlib $(DSTROOT)$(ARCHIVEDIR)/libmacho_static.a;		\
	    install -c -m 555 $(SYMROOT)/libmacho$S.dylib		\
		     $(DSTROOT)$(DYLIBDIR)/libmacho$S.dylib;		\
	    $(STRIP) -S $(DSTROOT)$(DYLIBDIR)/libmacho$S.dylib;		\
	    install -c -m 555 $(SYMROOT)/libmacho$S_profile.dylib	\
		     $(DSTROOT)$(DYLIBDIR)/libmacho$S_profile.dylib;	\
	    $(STRIP) -S $(DSTROOT)$(DYLIBDIR)/libmacho$S_profile.dylib;	\
	    install -c -m 555 $(SYMROOT)/libmacho$S_debug.dylib	\
		     $(DSTROOT)$(DYLIBDIR)/libmacho$S_debug.dylib;	\
	    $(STRIP) -S $(DSTROOT)$(DYLIBDIR)/libmacho$S_debug.dylib;	\
	fi

installsrc:
	$(MKDIRS) $(SRCROOT)
	chmod 755 $(SRCROOT)
	gnutar cf - $(INSTALL_FILES) | (cd $(SRCROOT); gnutar xf -)
	chmod 444 $(SRCROOT)/*

$(OFILE_DIRS) $(SYMROOT):
	$(MKDIRS) $@
