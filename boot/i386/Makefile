
#	Makefile for i386 boot program
#	define FLOPPY and SMALL using DEFINES macro as necessary

CFLAGS	= -O $(MORECPP) -arch i386 -g -mi386:unaligned-text -static
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
# LIBS= -lc_static

#
# these paths are only valid in subdirectories of this directory
#
OBJROOT=`pwd`/../../obj/i386
SYMROOT=`pwd`/../../sym/i386
DSTROOT=`pwd`/../../dst/i386
SRCROOT=/tmp

VPATH = $(OBJROOT):$(SYMROOT)

# SUBDIRS = rcz util libsa libsaio sarld nasm boot2 boot1 boot0 strings
SUBDIRS = rcz util libsa libsaio nasm boot2 boot1 boot0 strings

all tags clean debug install installhdrs:
	@for i in ${SUBDIRS}; \
	do \
		echo ================= make $@ for $$i =================; \
		( cd $$i; ${MAKE}					  \
			"OBJROOT=$(OBJROOT)/$$i"			  \
		  	"SYMROOT=$(SYMROOT)"				  \
			"DSTROOT=$(DSTROOT)"				  \
			"SRCROOT=$(SRCROOT)"				  \
			"RC_ARCHS=$(RC_ARCHS)"				  \
			"RC_KANJI=$(RC_KANJI)"				  \
			"JAPANESE=$(JAPANESE)"				  \
			"RC_CFLAGS=$(RC_CFLAGS)" $@ 			  \
		) || exit $?; 						  \
	done

installsrc:
	tar cf - . | (cd ${SRCROOT}; tar xfBp -)
