
#----------------------------------------------------------------
# This file is a Makefile for Trf 2.1.4 (as of MAY-06-2009), usable for CygWin B20.1
# Donated by Jan Nijtmans <jan.nijtmans@cmg.nl> <nijtmans@wxs.nl>
#----------------------------------------------------------------

EXTENSION	= Trf
VERSION		= 2.1.4
#TCL_VERSION	= 81
TCL_VERSION	= 82

TRF_DLL_FILE	    = ${EXTENSION}21.dll
TRF_LIB_FILE	    = lib${EXTENSION}21.a
TRF_STATIC_LIB_FILE = lib${EXTENSION}21s.a
TRF_STUB_LIB_FILE   = lib${EXTENSION}stub21.a

SSL_LIBRARY	= -DSSL_LIB_NAME=\"libeay32.dll\"
BZ2_LIBRARY	= -DBZ2_LIB_NAME=\"bz2.dll\"
#ZLIB_STATIC	= -DZLIB_STATIC_BUILD
#BZLIB_STATIC	= -DBZLIB_STATIC_BUILD

#
#----------------------------------------------------------------
# Things you can change to personalize the Makefile for your own
# site (you can make these changes in either Makefile.in or
# Makefile, but changes to Makefile will get lost if you re-run
# the configuration script).
#----------------------------------------------------------------

# Directory in which the source of this extension can be found
srcdir		=	.
TMPDIR		=	.

# Directories in which the Tcl core can be found
TCL_INC_DIR	= /progra~1/tcl/include
TCL_LIB_DIR	= /progra~1/tcl/lib
TCL_LIB_SPEC	= /progra~1/tcl/lib/libtclstub$(TCL_VERSION).a

# Libraries to be included with trf.dll
TCL_SHARED_LIBS		=

# Default top-level directories in which to install architecture-
# specific files (exec_prefix) and machine-independent files such
# as scripts (prefix).  The values specified here may be overridden
# at configure-time with the --exec-prefix and --prefix options
# to the "configure" script.

prefix		=	/progra~1/Tcl
exec_prefix	=	$(prefix)

# Directory containing scripts supporting the work of this makefile
tool		=	$(srcdir)/tools


# The following definition can be set to non-null for special systems
# like AFS with replication.  It allows the pathnames used for installation
# to be different than those used for actually reference files at
# run-time.  INSTALL_ROOT is prepended to $prefix and $exec_prefix
# when installing files.
INSTALL_ROOT =


# Directory where trf.dll is at run-time:
LIB_RUNTIME_DIR =	$(exec_prefix)/lib/$(EXTENSION)$(VERSION)


# Directory in which to install the archive trf.dll:
LIB_INSTALL_DIR =	$(INSTALL_ROOT)$(LIB_RUNTIME_DIR)


# Directory in which to install the extended shell tclsh:
BIN_INSTALL_DIR =	$(INSTALL_ROOT)$(exec_prefix)/bin


# Directory in which to install the include file transform.h:
INCLUDE_INSTALL_DIR =	$(INSTALL_ROOT)$(prefix)/include


# Top-level directory in which to install manual entries:
MAN_INSTALL_DIR =	$(INSTALL_ROOT)$(prefix)/man

# To change the compiler switches, for example to change from -O
# to -g, change the following line:
CFLAGS		=	-O2 -fnative-struct -mno-cygwin -DNDEBUG -DUSE_TCL_STUBS -D__WIN32__ -DWIN32 -D_WINDOWS -DZLIB_DLL -DTCL_THREADS -DHAVE_STDLIB_H

# To disable ANSI-C procedure prototypes reverse the comment characters
# on the following lines:
PROTO_FLAGS =
#PROTO_FLAGS = -DNO_PROTOTYPE


# To enable memory debugging reverse the comment characters on the following
# lines.  Warning:  if you enable memory debugging, you must do it
# *everywhere*, including all the code that calls Tcl, and you must use
# ckalloc and ckfree everywhere instead of malloc and free.
MEM_DEBUG_FLAGS =
#MEM_DEBUG_FLAGS = -DTCL_MEM_DEBUG


# Some versions of make, like SGI's, use the following variable to
# determine which shell to use for executing commands:
SHELL =		/bin/sh


# Tcl used to let the configure script choose which program to use
# for installing, but there are just too many different versions of
# "install" around;  better to use the install-sh script that comes
# with the distribution, which is slower but guaranteed to work.

INSTALL = $(tool)/install-sh -c


# The symbols below provide support for dynamic loading and shared
# libraries.  The values of the symbols are normally set by the
# configure script.  You shouldn't normally need to modify any of
# these definitions by hand. The second definition should be used
# in conjunction with Tcl 8.1.

TRF_SHLIB_CFLAGS =
#TRF_SHLIB_CFLAGS = -DTCL_USE_STUBS


# The symbol below provides support for dynamic loading and shared
# libraries.  See configure.in for a description of what it means.
# The values of the symbolis normally set by the configure script.

SHLIB_LD =


# Libraries to use when linking:  must include at least the dynamic
# loading library and the math library (in that order).  This
# definition is determined by the configure script.
ALL_LIBS =  $(TCL_LIB)

LIBS =

#----------------------------------------------------------------
# The information below is modified by the configure script when
# Makefile is generated from Makefile.in.  You shouldn't normally
# modify any of this stuff by hand.
#----------------------------------------------------------------

INSTALL_PROGRAM =	$(INSTALL) -m 744
INSTALL_DATA =		$(INSTALL) -m 644
INSTALL_SHLIB =		$(INSTALL) -m 555
RANLIB =		ranlib
SHLIB_SUFFIX =		.dll

#----------------------------------------------------------------
# The information below should be usable as is.  The configure
# script won't modify it and you shouldn't need to modify it
# either.
#----------------------------------------------------------------

CC		=	gcc
AS = as
LD = ld
DLLTOOL = dlltool
DLLWRAP = dllwrap -mnocygwin 
WINDRES = windres

DLL_LDFLAGS = -mwindows -Wl,-e,_DllMain@12
DLL_LDLIBS = -L/progra~1/tcl/lib -ltclstub$(TCL_VERSION)

baselibs   = -lkernel32 $(optlibs) -ladvapi32
winlibs    = $(baselibs) -luser32 -lgdi32 -lcomdlg32 -lwinspool
guilibs	   = $(libc) $(winlibs)

guilibsdll = $(libcdll) $(winlibs)

TRF_DEFINES	= -D__WIN32__ -DSTATIC_BUILD  ${TRF_SHLIB_CFLAGS} -DTRF_VERSION="\"${VERSION}\"" ${SSL_LIBRARY} ${ZLIB_STATIC} ${BZLIB_STATIC} -DBUGS_ON_EXIT

# $(TCL_CC_SWITCHES)
INCLUDES	=	-I. -I$(srcdir) -I../generic -I$(TCL_INC_DIR)
DEFINES		=	$(PROTO_FLAGS) $(MEM_DEBUG_FLAGS) $(TRF_SHLIB_CFLAGS) \
			$(TRF_DEFINES)

CC_SWITCHES	=	$(CFLAGS) $(DEFINES) $(INCLUDES)

#		fundamentals of this library
SOURCES	=	../generic/adler.c \
	../generic/asc85code.c \
	../generic/b64code.c \
	../generic/bincode.c \
	../generic/binio.c \
	../generic/convert.c \
	../generic/crc.c \
	../generic/crc_zlib.c \
	../generic/dig_opt.c \
	../generic/digest.c \
	../generic/haval.c \
	../generic/hexcode.c \
	../generic/init.c \
	../generic/load.c \
	../generic/crypt.c \
	../generic/loadman.c \
	../generic/md2.c \
	../generic/md5dig.c \
	../generic/octcode.c \
	../generic/otpcode.c \
	../generic/otpmd5.c \
	../generic/otpsha1.c \
	../generic/registry.c \
	../generic/rs_ecc.c \
	../generic/sha.c \
	../generic/sha1.c \
	../generic/rmd160.c \
	../generic/rmd128.c \
	../generic/unstack.c \
	../generic/util.c \
	../generic/uucode.c \
	../generic/zip.c \
	../generic/zip_opt.c \
	../generic/zlib.c \
	../generic/bz2.c \
	../generic/bz2_opt.c \
	../generic/bz2lib.c \
	../generic/qpcode.c \
	../generic/reflect.c \
	../generic/ref_opt.c \
	../generic/trfStubInit.c \
	../compat/tclLoadWin.c


OBJECTS	=	adler.o \
	asc85code.o \
	b64code.o \
	bincode.o \
	binio.o \
	convert.o \
	crc.o \
	crc_zlib.o \
	dig_opt.o \
	digest.o \
	haval.o \
	hexcode.o \
	init.o \
	load.o \
	crypt.o \
	loadman.o \
	md2.o \
	md5dig.o \
	octcode.o \
	otpcode.o \
	otpmd5.o \
	otpsha1.o \
	registry.o \
	rs_ecc.o \
	sha.o \
	sha1.o \
	rmd160.o \
	rmd128.o \
	unstack.o \
	util.o \
	uucode.o \
	zip.o \
	zip_opt.o \
	zlib.o \
	bz2.o \
	bz2_opt.o \
	bz2lib.o \
	qpcode.o \
	reflect.o \
	ref_opt.o \
	trfStubInit.o \
	tclLoadWin.o

#-------------------------------------------------------#

default:	$(TRF_DLL_FILE) $(TRF_LIB_FILE) $(TRF_STUB_LIB_FILE)

all:	default

test:	$(TRF_DLL_FILE)
	wish${TK_VERSION} demo.tcl


#-------------------------------------------------------#

trfStubLib.o:	../generic/trfStubLib.c
	$(CC) -c $(CC_SWITCHES) ../generic/trfStubLib.c -o $@

trfStubInit.o:	../generic/trfStubInit.c
	$(CC) -c $(CC_SWITCHES) ../generic/trfStubInit.c -o $@

adler.o:	../generic/adler.c
	$(CC) -c $(CC_SWITCHES) ../generic/adler.c -o $@

asc85code.o:	../generic/asc85code.c
	$(CC) -c $(CC_SWITCHES) ../generic/asc85code.c -o $@

b64code.o:	../generic/b64code.c
	$(CC) -c $(CC_SWITCHES) ../generic/b64code.c -o $@

bincode.o:	../generic/bincode.c
	$(CC) -c $(CC_SWITCHES) ../generic/bincode.c -o $@

binio.o:	../generic/binio.c
	$(CC) -c $(CC_SWITCHES) ../generic/binio.c -o $@

convert.o:	../generic/convert.c
	$(CC) -c $(CC_SWITCHES) ../generic/convert.c -o $@

crc.o:	../generic/crc.c
	$(CC) -c $(CC_SWITCHES) ../generic/crc.c -o $@

crc_zlib.o:	../generic/crc_zlib.c
	$(CC) -c $(CC_SWITCHES) ../generic/crc_zlib.c -o $@

dig_opt.o:	../generic/dig_opt.c
	$(CC) -c $(CC_SWITCHES) ../generic/dig_opt.c -o $@

digest.o:	../generic/digest.c
	$(CC) -c $(CC_SWITCHES) ../generic/digest.c -o $@

haval.o:	../generic/haval.c
	$(CC) -c $(CC_SWITCHES) ../generic/haval.c -o $@

hexcode.o:	../generic/hexcode.c
	$(CC) -c $(CC_SWITCHES) ../generic/hexcode.c -o $@

init.o:	../generic/init.c
	$(CC) -c $(CC_SWITCHES) ../generic/init.c -o $@

load.o:	../generic/load.c
	$(CC) -c $(CC_SWITCHES) ../generic/load.c -o $@

crypt.o:	../generic/crypt.c
	$(CC) -c $(CC_SWITCHES) ../generic/crypt.c -o $@

loadman.o:	../generic/loadman.c
	$(CC) -c $(CC_SWITCHES) ../generic/loadman.c -o $@

md5dig.o:	../generic/md5dig.c
	$(CC) -c $(CC_SWITCHES) ../generic/md5dig.c -o $@

md2.o:	../generic/md2.c
	$(CC) -c $(CC_SWITCHES) ../generic/md2.c -o $@

octcode.o:	../generic/octcode.c
	$(CC) -c $(CC_SWITCHES) ../generic/octcode.c -o $@

otpcode.o:	../generic/otpcode.c
	$(CC) -c $(CC_SWITCHES) ../generic/otpcode.c -o $@

otpmd5.o:	../generic/otpmd5.c
	$(CC) -c $(CC_SWITCHES) ../generic/otpmd5.c -o $@

otpsha1.o:	../generic/otpsha1.c
	$(CC) -c $(CC_SWITCHES) ../generic/otpsha1.c -o $@

registry.o:	../generic/registry.c
	$(CC) -c $(CC_SWITCHES) ../generic/registry.c -o $@

rs_ecc.o:	../generic/rs_ecc.c
	$(CC) -c $(CC_SWITCHES) ../generic/rs_ecc.c -o $@

sha.o:	../generic/sha.c
	$(CC) -c $(CC_SWITCHES) ../generic/sha.c -o $@

sha1.o:	../generic/sha1.c
	$(CC) -c $(CC_SWITCHES) ../generic/sha1.c -o $@

rmd160.o:	../generic/rmd160.c
	$(CC) -c $(CC_SWITCHES) ../generic/rmd160.c -o $@

rmd128.o:	../generic/rmd128.c
	$(CC) -c $(CC_SWITCHES) ../generic/rmd128.c -o $@

unstack.o:	../generic/unstack.c
	$(CC) -c $(CC_SWITCHES) ../generic/unstack.c -o $@

util.o:	../generic/util.c
	$(CC) -c $(CC_SWITCHES) ../generic/util.c -o $@

uucode.o:	../generic/uucode.c
	$(CC) -c $(CC_SWITCHES) ../generic/uucode.c -o $@

zip.o:	../generic/zip.c
	$(CC) -c $(CC_SWITCHES) ../generic/zip.c -o $@

zip_opt.o:	../generic/zip_opt.c
	$(CC) -c $(CC_SWITCHES) ../generic/zip_opt.c -o $@

zlib.o:	../generic/zlib.c
	$(CC) -c $(CC_SWITCHES) ../generic/zlib.c -o $@

bz2.o:	../generic/zip.c
	$(CC) -c $(CC_SWITCHES) ../generic/bz2.c -o $@

bz2_opt.o:	../generic/zip_opt.c
	$(CC) -c $(CC_SWITCHES) ../generic/bz2_opt.c -o $@

bz2lib.o:	../generic/zlib.c
	$(CC) -c $(CC_SWITCHES) ../generic/bz2lib.c -o $@

qpcode.o:	../generic/qpcode.c
	$(CC) -c $(CC_SWITCHES) ../generic/qpcode.c -o $@

reflect.o:	../generic/reflect.c
	$(CC) -c $(CC_SWITCHES) ../generic/reflect.c -o $@

ref_opt.o:	../generic/ref_opt.c
	$(CC) -c $(CC_SWITCHES) ../generic/ref_opt.c -o $@

tclLoadWin.o:	../compat/tclLoadWin.c
	$(CC) -c $(CC_SWITCHES) ../compat/tclLoadWin.c -o $@

dllEntry.o:	dllEntry.c
	$(CC) -c $(CC_SWITCHES) dllEntry.c -o $@

#-------------------------------------------------------#

$(TRF_DLL_FILE):	$(OBJECTS) trfres.o dllEntry.o trf.def
	$(DLLWRAP) -s $(DLL_LDFLAGS) -mno-cygwin -o $@ $(OBJECTS) \
		trfres.o dllEntry.o --def trf.def \
		$(DLL_LDLIBS) 

$(TRF_LIB_FILE): trf.def $(TRF_DLL_FILE)
	$(DLLTOOL) --as=$(AS) --dllname $(TRF_DLL_FILE) \
		--def trf.def --output-lib $@

$(TRF_STATIC_LIB_FILE): $(OBJECTS)
	$(AR) cr $@ $(OBJECTS)

$(TRF_STUB_LIB_FILE): trfStubLib.o
	$(AR) cr $@ trfStubLib.o

trf.def: $(OBJECTS)
	$(DLLTOOL) --export-all --exclude-symbols DllMain@12  --output-def $@ $(OBJECTS)

trfres.o: trf.rc
	$(WINDRES) --include . --define VS_VERSION_INFO=1 trf.rc trfres.o


#-------------------------------------------------------#

clean:
	del $(OBJECTS) $(TRF_DLL_FILE)
	del TAGS depend *~ */*~ core* tests/core* so_locations lib*.so*

distclean:	clean
	del config.*
	del Makefile

#-------------------------------------------------------#
# DO NOT DELETE THIS LINE -- make depend depends on it.
