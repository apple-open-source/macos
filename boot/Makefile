export USE_APPLE_PB_SUPPORT = all

#	Makefile for kernel booter

# CFLAGS	= -O $(MORECPP) -arch i386 -g 
DEFINES=
CONFIG = hd
LIBDIR = libsa
INC = -I. -I$(LIBDIR)
ifneq "" "$(wildcard /bin/mkdirs)"
  MKDIRS = /bin/mkdirs
else
  MKDIRS = /bin/mkdir -p
endif
AS = as
LD = ld
PAX = /bin/pax

OBJROOT = `pwd`/obj
SYMROOT = `pwd`/sym
DSTROOT = `pwd`/dst
SRCROOT = /tmp
ARCHLESS_RC_CFLAGS=`echo $(RC_CFLAGS) | sed 's/-arch [a-z0-9]*//g'`

VPATH = $(OBJROOT):$(SYMROOT)

GENERIC_SUBDIRS = gen

#
# Currently builds for i386
#

all tags clean debug install installhdrs: $(SYMROOT) $(OBJROOT)
	@if [ -z "$(RC_ARCHS)" ]; then					  \
		RC_ARCHS="i386";					  \
	fi;								  \
	SUBDIRS="$(GENERIC_SUBDIRS) $$RC_ARCHS";			  \
	for i in $$SUBDIRS; 						  \
	do \
	    if [ -d $$i ]; then						  \
		echo ================= make $@ for $$i =================; \
		( OBJROOT=$(OBJROOT)/$${i};				  \
		  SYMROOT=$(SYMROOT)/$${i};				  \
		  DSTROOT=$(DSTROOT);					  \
	          XCFLAGS=$(ARCHLESS_RC_CFLAGS);			  \
	          GENSUBDIRS="$(GENERIC_SUBDIRS)";			  \
	          for x in $$GENSUBDIRS;				  \
	          do							  \
	              if [ "$$x" == "$$i" ]; then			  \
	                  XCFLAGS="$(RC_CFLAGS)";			  \
	                  break;					  \
	              fi						  \
	          done;							  \
		  echo "$$OBJROOT $$SYMROOT $$DSTROOT"; \
		    cd $$i; ${MAKE}					  \
			"OBJROOT=$$OBJROOT"		 	  	  \
		  	"SYMROOT=$$SYMROOT"				  \
			"DSTROOT=$$DSTROOT"				  \
			"SRCROOT=$$SRCROOT"				  \
			"RC_ARCHS=$$RC_ARCHS"				  \
			"TARGET=$$i"					  \
			"RC_KANJI=$(RC_KANJI)"				  \
			"JAPANESE=$(JAPANESE)"				  \
			"RC_CFLAGS=$$XCFLAGS" $@			  \
		) || exit $?; 						  \
	    else							  \
	    	echo "========= nothing to build for $$i =========";	  \
	    fi;								  \
	done

installsrc:
	gnutar cf - . | (cd ${SRCROOT}; gnutar xpf -)

$(SYMROOT) $(OBJROOT) $(DSTROOT):
	@$(MKDIRS) $@
