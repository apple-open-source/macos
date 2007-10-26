# Note: For Darwin developers only building for current MacOS X release is 
# supported.  The Openstep target will NOT build outside of Apple as it requires
# 4.3bsd licenced code.
#
# Building for three target OS's are currently supported:
#
# MacOS X (the default)
#	RC_OS is set to macos (the top level makefile does this)
#	RC_CFLAGS needs -D__KODIAK__ when RC_RELEASE is Kodiak (Public Beta),
#		to get the Public Beta directory layout.
#	RC_CFLAGS needs -D__GONZO_BUNSEN_BEAKER__ when RC_RELEASE is Gonzo,
#		Bunsen or Beaker to get the old directory layout.
#	The code is #ifdef'ed with __Mach30__ is picked up from <mach/mach.h>
# Rhapsody
#	RC_OS is set to teflon
#	RC_CFLAGS needs the additional flag -D__HERA__
# Openstep
#	RC_OS is set to nextstep
#	RC_CFLAGS needs the additional flag -D__OPENSTEP__
#
export USE_APPLE_PB_SUPPORT = all

DSTROOT = /
RC_OS = macos
RC_CFLAGS =

INSTALLSRC_SUBDIRS = $(COMMON_SUBDIRS) ar file include dyld
COMMON_SUBDIRS = libstuff as gprof misc libmacho ld libdyld \
		 mkshlib otool profileServer RelNotes man cbtlibs
ifeq "macos" "$(RC_OS)"
  APPLE_SUBDIRS := $(shell if [ "$(RC_RELEASE)" = "Beaker"  ] || \
			      [ "$(RC_RELEASE)" = "Bunsen"  ] || \
			      [ "$(RC_RELEASE)" = "Gonzo"   ] || \
			      [ "$(RC_RELEASE)" = "Kodiak"  ] || \
			      [ "$(RC_RELEASE)" = "Cheetah" ] || \
			      [ "$(RC_RELEASE)" = "Puma"    ]; then \
				echo "ar file" ; \
			    else \
				echo "ar" ; fi; )
else
  APPLE_SUBDIRS = ar file
endif

ifeq "macos" "$(RC_OS)"
  OLD_DYLD_STUFF := $(shell if [ "$(RC_RELEASE)" = "Beaker"    ] || \
			       [ "$(RC_RELEASE)" = "Bunsen"    ] || \
			       [ "$(RC_RELEASE)" = "Gonzo"     ] || \
			       [ "$(RC_RELEASE)" = "Kodiak"    ] || \
			       [ "$(RC_RELEASE)" = "Cheetah"   ] || \
			       [ "$(RC_RELEASE)" = "Puma"      ] || \
			       [ "$(RC_RELEASE)" = "Jaguar"    ] || \
			       [ "$(RC_RELEASE)" = "Panther"   ] || \
			       [ "$(RC_RELEASE)" = "MuonPrime" ] || \
			       [ "$(RC_RELEASE)" = "MuonSeed"  ] || \
			       [ "$(RC_RELEASE)" = "SUPanWheat" ]; then \
				echo "dyld" ; \
			    else \
				echo "" ; fi; )
else
  OLD_DYLD_STUFF = dyld
endif

ifeq "nextstep" "$(RC_OS)"
  SUBDIRS = $(COMMON_SUBDIRS) $(OLD_DYLD_STUFF)
else
  SUBDIRS = $(COMMON_SUBDIRS) $(OLD_DYLD_STUFF) $(APPLE_SUBDIRS)
endif

ifneq "" "$(wildcard /bin/mkdirs)"
  MKDIRS = /bin/mkdirs
else
  MKDIRS = /bin/mkdir -p
endif

all clean:
	@if [ "$(SRCROOT)" != "" ] && \
	    [ "$(OBJROOT)" != "" ] && \
	    [ "$(SYMROOT)" != "" ];			\
	then								\
	    CWD=`pwd`; cd "$(DSTROOT)"; DSTROOT=`pwd`; cd "$$CWD";	\
	    for i in `echo $(SUBDIRS)`;					\
	      do							\
		    echo =========== $(MAKE) $@ for $$i =============;	\
		    (cd $$i; $(MAKE) RC_CFLAGS="$(RC_CFLAGS)"		\
			RC_ARCHS="$(RC_ARCHS)" RC_OS="$(RC_OS)"		\
			VERS_STRING_FLAGS="$(VERS_STRING_FLAGS)"	\
			OLD_DYLD_STUFF="$(OLD_DYLD_STUFF)"		\
			DSTROOT=$$DSTROOT				\
			SRCROOT=$(SRCROOT)/$$i				\
			OBJROOT=$(OBJROOT)/$$i				\
			SYMROOT=$(SYMROOT)/$$i $@) || exit 1 ;		\
	      done							\
	else								\
	    CWD=`pwd`; cd "$(DSTROOT)"; DSTROOT=`pwd`; cd "$$CWD";	\
	    for i in `echo $(SUBDIRS)`;					\
	      do							\
		    echo =========== $(MAKE) $@ for $$i =============;	\
		    (cd $$i; $(MAKE) RC_CFLAGS="$(RC_CFLAGS)"		\
			RC_ARCHS="$(RC_ARCHS)" RC_OS="$(RC_OS)"		\
			OLD_DYLD_STUFF="$(OLD_DYLD_STUFF)"		\
			DSTROOT=$$DSTROOT $@) || exit 1 ;		\
	      done							\
	fi

install:
	@if [ $(SRCROOT) ];						\
	then								\
	    projName=`basename $(SRCROOT) | 				\
		sed 's/-[-0-9.]*//' | sed 's/\.cvs//'`;			\
	    if [ "$$projName" = cctools ];				\
	    then							\
		target=install_tools;					\
	    elif [ "$$projName" = cctoolslib ];				\
	    then							\
	    	target=lib_ofiles_install;				\
	    else							\
	        echo "Unknown project name $$projName";			\
		exit 1;							\
	    fi;								\
	    CWD=`pwd`; cd "$(DSTROOT)"; DSTROOT=`pwd`; cd "$$CWD";	\
	    echo =========== $(MAKE) $$target =============;		\
	    $(MAKE) RC_CFLAGS="$(RC_CFLAGS)"				\
		RC_ARCHS="$(RC_ARCHS)" RC_OS="$(RC_OS)"			\
		VERS_STRING_FLAGS="$(VERS_STRING_FLAGS)"		\
		OLD_DYLD_STUFF="$(OLD_DYLD_STUFF)"			\
		DSTROOT=$$DSTROOT					\
		SRCROOT=$(SRCROOT)					\
		OBJROOT=$(OBJROOT)					\
		SYMROOT=$(SYMROOT) $$target;				\
	else								\
	    CWD=`pwd`; cd "$(DSTROOT)"; DSTROOT=`pwd`; cd "$$CWD";	\
	    echo =========== $(MAKE) install_tools =============;	\
	    $(MAKE) RC_CFLAGS="$(RC_CFLAGS)" RC_ARCHS="$(RC_ARCHS)" 	\
		RC_OS="$(RC_OS)" SUBDIRS="$(SUBDIRS)"			\
		VERS_STRING_FLAGS="$(VERS_STRING_FLAGS)"		\
		OLD_DYLD_STUFF="$(OLD_DYLD_STUFF)"			\
		DSTROOT=$$DSTROOT install_tools lib_ofiles_install;	\
	fi

install_tools: installhdrs
	@if [ $(SRCROOT) ];						\
	then								\
	    CWD=`pwd`; cd "$(DSTROOT)"; DSTROOT=`pwd`; cd "$$CWD";	\
	    for i in `echo $(SUBDIRS)`;					\
	      do							\
		    echo ======== $(MAKE) install for $$i ============;	\
		    (cd $$i; $(MAKE) RC_CFLAGS="$(RC_CFLAGS)"		\
			RC_ARCHS="$(RC_ARCHS)" RC_OS="$(RC_OS)"		\
			VERS_STRING_FLAGS="$(VERS_STRING_FLAGS)"	\
			OLD_DYLD_STUFF="$(OLD_DYLD_STUFF)"		\
			DSTROOT=$$DSTROOT				\
			SRCROOT=$(SRCROOT)/$$i				\
			OBJROOT=$(OBJROOT)/$$i				\
			SYMROOT=$(SYMROOT)/$$i install) || exit 1;	\
	      done;							\
	    if [ $(RC_RELEASE) ];					\
	    then							\
	      CWD=`pwd`; cd "$(DSTROOT)"; DSTROOT=`pwd`; cd "$$CWD";	\
	      for i in `echo $(SUBDIRS)`;				\
	        do							\
		    echo ===== $(MAKE) shlib_clean for $$i ==========;	\
		    (cd $$i; $(MAKE) RC_CFLAGS="$(RC_CFLAGS)"		\
			RC_ARCHS="$(RC_ARCHS)" RC_OS="$(RC_OS)"		\
			VERS_STRING_FLAGS="$(VERS_STRING_FLAGS)"	\
			OLD_DYLD_STUFF="$(OLD_DYLD_STUFF)"		\
			DSTROOT=$$DSTROOT				\
			SRCROOT=$(SRCROOT)/$$i				\
			OBJROOT=$(OBJROOT)/$$i				\
			SYMROOT=$(SYMROOT)/$$i shlib_clean) || exit 1;	\
	      done							\
	    fi								\
	else								\
	    CWD=`pwd`; cd "$(DSTROOT)"; DSTROOT=`pwd`; cd "$$CWD";	\
	    for i in `echo $(SUBDIRS)`;					\
	      do							\
		    echo ========= $(MAKE) install for $$i ===========;	\
		    (cd $$i; $(MAKE) RC_CFLAGS="$(RC_CFLAGS)"		\
			RC_ARCHS="$(RC_ARCHS)" RC_OS="$(RC_OS)"		\
			VERS_STRING_FLAGS="$(VERS_STRING_FLAGS)"	\
			OLD_DYLD_STUFF="$(OLD_DYLD_STUFF)"		\
			DSTROOT=$$DSTROOT install) || exit 1;		\
	      done							\
	fi

ofiles_install:
	@ export RC_FORCEHDRS=YES;					\
	$(MAKE) RC_CFLAGS="$(RC_CFLAGS)"				\
		RC_ARCHS="$(RC_ARCHS)"					\
		RC_OS="$(RC_OS)"					\
		DSTROOT=$$DSTROOT					\
		SRCROOT=$(SRCROOT)					\
		OBJROOT=$(OBJROOT)					\
		SYMROOT=$(SYMROOT)					\
		OLD_DYLD_STUFF="$(OLD_DYLD_STUFF)"			\
		lib_ofiles_install

lib_ofiles lib_ofiles_install: installhdrs
	@if [ $(SRCROOT) ];						\
	then								\
	    CWD=`pwd`; cd "$(DSTROOT)"; DSTROOT=`pwd`; cd "$$CWD";	\
	    SED_RC_CFLAGS=`echo "$(RC_CFLAGS)" | sed 's/-arch ppc64//'  \
 		| sed 's/-arch x86_64//'`;				\
	    echo =========== $(MAKE) $@ for libstuff =============;	\
	    (cd libstuff; $(MAKE) "RC_CFLAGS=$(RC_CFLAGS)"		\
		RC_ARCHS="$(RC_ARCHS)" RC_OS="$(RC_OS)"			\
		DSTROOT=$$DSTROOT					\
		SRCROOT=$(SRCROOT)/libstuff				\
		OBJROOT=$(OBJROOT)/libstuff				\
		SYMROOT=$(SYMROOT)/libstuff $@) || exit 1;		\
	    echo =========== $(MAKE) all for libstuff =============;	\
	    (cd libstuff; $(MAKE) "RC_CFLAGS=$$SED_RC_CFLAGS"		\
		RC_ARCHS="$(RC_ARCHS)" RC_OS="$(RC_OS)"			\
		DSTROOT=$$DSTROOT					\
		SRCROOT=$(SRCROOT)/libstuff				\
		OBJROOT=$(OBJROOT)/libstuff				\
		SYMROOT=$(SYMROOT)/libstuff all) || exit 1;		\
	    echo =========== $(MAKE) $@ for libmacho =============;	\
	    (cd libmacho; $(MAKE) RC_CFLAGS="$(RC_CFLAGS)"		\
		RC_ARCHS="$(RC_ARCHS)" RC_OS="$(RC_OS)"			\
		DSTROOT=$$DSTROOT					\
		SRCROOT=$(SRCROOT)/libmacho				\
		OBJROOT=$(OBJROOT)/libmacho				\
		SYMROOT=$(SYMROOT)/libmacho $@) || exit 1;		\
	    echo =========== $(MAKE) $@ for ld =============;		\
	    (cd ld; $(MAKE) "RC_CFLAGS=$$SED_RC_CFLAGS"			\
		RC_ARCHS="$(RC_ARCHS)" RC_OS="$(RC_OS)"			\
		DSTROOT=$$DSTROOT					\
		SRCROOT=$(SRCROOT)/ld					\
		OBJROOT=$(OBJROOT)/ld					\
		SYMROOT=$(SYMROOT)/ld $@) || exit 1;			\
	    echo =========== $(MAKE) $@ for libdyld =============;	\
	    (cd libdyld; $(MAKE) "RC_CFLAGS=$$SED_RC_CFLAGS"		\
		RC_ARCHS="$(RC_ARCHS)" RC_OS="$(RC_OS)"			\
		DSTROOT=$$DSTROOT					\
		SRCROOT=$(SRCROOT)/libdyld				\
		OBJROOT=$(OBJROOT)/libdyld				\
		SYMROOT=$(SYMROOT)/libdyld $@) || exit 1;		\
	    echo =========== $(MAKE) $@ for misc =============;	\
	    (cd misc; $(MAKE) "RC_CFLAGS=$(RC_CFLAGS)"			\
		RC_ARCHS="$(RC_ARCHS)" RC_OS="$(RC_OS)"			\
		DSTROOT=$$DSTROOT					\
		SRCROOT=$(SRCROOT)/misc					\
		OBJROOT=$(OBJROOT)/misc					\
		SYMROOT=$(SYMROOT)/misc $@) || exit 1;			\
	    echo =========== $(MAKE) $@ for cbtlibs =============;	\
	    (cd cbtlibs; $(MAKE) "RC_CFLAGS=$(RC_CFLAGS)"		\
		RC_ARCHS="$(RC_ARCHS)" RC_OS="$(RC_OS)"			\
		DSTROOT=$$DSTROOT					\
		SRCROOT=$(SRCROOT)/cbtlibs				\
		OBJROOT=$(OBJROOT)/cbtlibs				\
		SYMROOT=$(SYMROOT)/cbtlibs $@) || exit 1;		\
	else								\
	    CWD=`pwd`; cd "$(DSTROOT)"; DSTROOT=`pwd`; cd "$$CWD";	\
	    SED_RC_CFLAGS=`echo "$(RC_CFLAGS)" | sed 's/-arch ppc64//'  \
 		| sed 's/-arch x86_64//'`;				\
	    echo =========== $(MAKE) $@ for libstuff =============;	\
	    (cd libstuff; $(MAKE) "RC_CFLAGS=$(RC_CFLAGS)"		\
		RC_ARCHS="$(RC_ARCHS)" RC_OS="$(RC_OS)"			\
		DSTROOT=$$DSTROOT $@) || exit 1;			\
	    echo =========== $(MAKE) all for libstuff =============;	\
	    (cd libstuff; $(MAKE) "RC_CFLAGS=$$SED_RC_CFLAGS"		\
		RC_ARCHS="$(RC_ARCHS)" RC_OS="$(RC_OS)"			\
		DSTROOT=$$DSTROOT all) || exit 1;			\
	    echo =========== $(MAKE) $@ for libmacho =============;	\
	    (cd libmacho; $(MAKE) RC_CFLAGS="$(RC_CFLAGS)"		\
		RC_ARCHS="$(RC_ARCHS)" RC_OS="$(RC_OS)"			\
		DSTROOT=$$DSTROOT $@) || exit 1;			\
	    echo =========== $(MAKE) $@ for ld =============;		\
	    (cd ld; $(MAKE) "RC_CFLAGS=$$SED_RC_CFLAGS"			\
		RC_ARCHS="$(RC_ARCHS)" RC_OS="$(RC_OS)"			\
		DSTROOT=$$DSTROOT $@) || exit 1;			\
	    echo =========== $(MAKE) $@ for libdyld =============;	\
	    (cd libdyld; $(MAKE) "RC_CFLAGS=$$SED_RC_CFLAGS"		\
		RC_ARCHS="$(RC_ARCHS)" RC_OS="$(RC_OS)"			\
		DSTROOT=$$DSTROOT $@) || exit 1;			\
	    echo =========== $(MAKE) $@ for misc =============;		\
	    (cd misc; $(MAKE) "RC_CFLAGS=$(RC_CFLAGS)"			\
		RC_ARCHS="$(RC_ARCHS)" RC_OS="$(RC_OS)"			\
		DSTROOT=$$DSTROOT $@) || exit 1;			\
	    (cd cbtlibs; $(MAKE) "RC_CFLAGS=$(RC_CFLAGS)"		\
		RC_ARCHS="$(RC_ARCHS)" RC_OS="$(RC_OS)"			\
		DSTROOT=$$DSTROOT $@) || exit 1;			\
	fi

installsrc: SRCROOT
	$(MKDIRS) $(SRCROOT)
	cp Makefile APPLE_LICENSE PB.project $(SRCROOT)
	for i in `echo $(INSTALLSRC_SUBDIRS)`; \
	  do \
		echo =========== $(MAKE) $@ for $$i =============;	\
		(cd $$i; $(MAKE) SRCROOT=$$SRCROOT/$$i $@) || exit 1;	\
	  done

installGASsrc: SRCROOT
	$(MKDIRS) $(SRCROOT)
	cp Makefile $(SRCROOT)
	@for i in as libstuff include ; \
	  do \
		echo =========== $(MAKE) $@ for $$i =============;	\
		(cd $$i; $(MAKE) SRCROOT=$$SRCROOT/$$i $@) || exit 1;	\
	  done

fromGASsrc:
	@CWD=`pwd`; cd "$(DSTROOT)"; DSTROOT=`pwd`; cd "$$CWD";		\
	echo =========== $(MAKE) fromGASsrc for libstuff =============;	\
	(cd libstuff; $(MAKE) RC_CFLAGS="$(RC_CFLAGS)"			\
	    RC_ARCHS="$(RC_ARCHS)" RC_OS="$(RC_OS)"			\
	    DSTROOT=$$DSTROOT fromGASsrc) || exit 1;			\
	echo =========== $(MAKE) appc_build for as =============;	\
	(cd as; $(MAKE) RC_CFLAGS="$(RC_CFLAGS)"			\
	    RC_ARCHS="$(RC_ARCHS)" RC_OS="$(RC_OS)"			\
	    DSTROOT=$$DSTROOT appc_build) || exit 1;			\

installhdrs: $(DSTROOT)
	@if [ $(SRCROOT) ];						\
	then								\
	    projName=`basename $(SRCROOT) | sed 's/-[0-9.]*//'`;	\
	    if [ "$$projName" = cctools -a $(RC_OS) = macos ] &&	\
	       [ "$(RC_FORCEHDRS)" != "YES" ];				\
	    then							\
	    	echo === cctools does not install headers for macos ===;\
	    else							\
		(cd include; $(MAKE) DSTROOT=$(DSTROOT)			\
			RC_OS="$(RC_OS)" install) || exit 1;		\
		if [ $(OLD_DYLD_STUFF) ];				\
		then							\
		    (cd dyld; $(MAKE) DSTROOT=$(DSTROOT)		\
			 RC_OS=$(RC_OS)	installhdrs) || exit 1;		\
		fi;							\
	    fi;								\
	else								\
	    (cd include; $(MAKE) DSTROOT=$(DSTROOT) RC_OS=$(RC_OS) 	\
		install) || exit 1;					\
	    if [ $(OLD_DYLD_STUFF) ];					\
	    then							\
		(cd dyld; $(MAKE) DSTROOT=$(DSTROOT) RC_OS=$(RC_OS) 	\
		    installhdrs) || exit 1;				\
	    fi;								\
	fi

$(DSTROOT):
	$(MKDIRS) $@

SRCROOT:
	@if [ -n "${$@}" ]; \
	then \
		exit 0; \
	else \
		echo Must define $@; \
		exit 1; \
	fi
