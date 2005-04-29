#
# This file is a Makefile for IMG, usable for CygWin B20.1

EXTENSION	= Img
VERSION		= 1.2
TCL_VER 	= 82

IMG_LIB_FILE	= libimg12s.a
IMG_DLL_FILE	= img12.dll

# Directory where libz, libpng, libjpeg, libttf and libgif are at runtime.
IMG_RUNTIME_DIR	=	$(exec_prefix)/lib

# How to reference the libz library.
ZREF		= -L${IMG_RUNTIME_DIR} -L. -lz

#
# names of additional libraries to be used/produced.
#

Z_DLL_FILE	=	zlib.dll
PNG_DLL_FILE	=	png.dll
JPEG_DLL_FILE	=	jpeg62.dll
TIFF_DLL_FILE	=	tiff.dll

Z_LIB_FILE	=	libz.a
PNG_LIB_FILE	=	libpng.a
JPEG_LIB_FILE	=	libjpeg.a
TIFF_LIB_FILE	=	libtiff.a

Z_STATIC_LIB_FILE	=	libzs.a
PNG_STATIC_LIB_FILE	=	libpngs.a
JPEG_STATIC_LIB_FILE	=	libjpegs.a
TIFF_STATIC_LIB_FILE	=	libtiffs.a

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
TCL_LIB_SPEC	= /progra~1/tcl/lib/libtclstub$(TCL_VER).a

# Directories in which the Tk core can be found
TK_INC_DIR	= /progra~1/tcl/include
TK_LIB_DIR	= /progra~1/tcl/lib
TK_BIN_DIR	= /progra~1/tcl/bin
TK_LIB_SPEC	= /progra~1/tcl/lib/libtkstub$(TCL_VER).a
X11_LIB_DIR	= /progra~1/tcl/include/Xlib

# Directories in which the X11 includes and libraries can be found
TK_XINCLUDES		=	-I/progra~1/tcl/include/Xlib
TK_XLIBSW		=

# Libraries to be included with libimg$(VERSION).so
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


# Directory where libimg.a is at run-time:
LIB_RUNTIME_DIR =	$(exec_prefix)/lib/$(EXTENSION)$(VERSION)


# Directory in which to install the archive libimg.a:
LIB_INSTALL_DIR =	$(INSTALL_ROOT)$(LIB_RUNTIME_DIR)


# Directory in which to install the extended shell tclsh:
BIN_INSTALL_DIR =	$(INSTALL_ROOT)$(exec_prefix)/bin


# Directory in which to install the include file transform.h:
INCLUDE_INSTALL_DIR =	$(INSTALL_ROOT)$(prefix)/include


# Top-level directory in which to install manual entries:
MAN_INSTALL_DIR =	$(INSTALL_ROOT)$(prefix)/man

# Directory where libz, libpng and libjpeg are (or will be) installed.
IMG_INSTALL_DIR	=	$(INSTALL_ROOT)$(IMG_RUNTIME_DIR)

# To change the compiler switches, for example to change from -O
# to -g, change the following line:
CFLAGS          =       -O2 -mno-cygwin -fnative-struct -fomit-frame-pointer -DNDEBUG -D__WIN32__ -DWIN32 -D_WINDOWS -DZLIB_DLL -DPNG_USE_PNGGCCRD

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
# these definitions by hand.

IMG_SHLIB_CFLAGS =


# The symbol below provipng support for dynamic loading and shared
# libraries.  See configure.in for a pngcription of what it means.
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

zlibdir		=	$(srcdir)/libz
pngdir		=	$(srcdir)/libpng
jpegdir		=	$(srcdir)/libjpeg
tiffdir 	=	$(srcdir)/libtiff

#----------------------------------------------------------------
# The information below should be usable as is.  The configure
# script won't modify it and you shouldn't need to modify it
# either.
#----------------------------------------------------------------

CC		=	gcc
AS = as
LD = ld
DLLTOOL = dlltool
DLLWRAP = dllwrap 
WINDRES = windres

DLL_LDFLAGS = -mwindows -Wl,-e,_DllMain@12
DLL_LDLIBS = -ltkstub$(TCL_VER) -ltclstub$(TCL_VER)

baselibs   = -lkernel32 $(optlibs) -ladvapi32
winlibs    = $(baselibs) -luser32 -lgdi32 -lcomdlg32 -lwinspool
guilibs	   = $(libc) $(winlibs)

guilibsdll = $(libcdll) $(winlibs)

IMG_DEFINES	= -Ddlopen=imgDlopen -Ddlclose=imgDlclose -Ddlsym=imgDlsym \
		  -Ddlerror=imgDlerror -DSTATIC_BUILD -DUSE_TCL_STUBS -DUSE_TK_STUBS

# $(TCL_CC_SWITCHES)
INCLUDES	=	-I. -I$(srcdir) -I$(zlibdir) -I$(pngdir) \
			-I$(jpegdir) -I$(tiffdir) \
			-I$(TCL_INC_DIR) -I$(TK_INC_DIR) $(TK_XINCLUDES)
DEFINES		=	$(PROTO_FLAGS) $(MEM_DEBUG_FLAGS) $(IMG_SHLIB_CFLAGS) \
			$(IMG_DEFINES)

CC_SWITCHES	=	$(CFLAGS) $(DEFINES) $(INCLUDES)

#		fundamentals of this library
SOURCES	=	imgInit.c imgObj.c imgUtil.c imgPmap.c imgWinPmap.c \
		imgBMP.c imgGIF.c imgJPEG.c imgPNG.c imgPS.c \
		imgTIFF.c imgTIFFjpeg.c imgTIFFpixar.c imgTIFFzip.c \
		imgWindow.c imgXBM.c imgXPM.c xcolors.c
OBJECTS	=	imgInit.o imgObj.o imgUtil.o imgPmap.o imgWinPmap.o \
		imgBMP.o imgGIF.o imgJPEG.o imgPNG.o imgPS.o \
		imgTIFF.o imgTIFFjpeg.o imgTIFFpixar.o imgTIFFzip.o \
		imgWindow.o imgXBM.o imgXPM.o tclLoadWin.o

#-------------------------------------------------------#

default:	$(IMG_LIB_FILE) $(IMG_DLL_FILE)

all:	$(Z_LIB_FILE) $(PNG_LIB_FILE) $(JPEG_LIB_FILE) $(TIFF_LIB_FILE)\
		$(TTF_LIB_FILE) $(GIF_LIB_FILE) default

test:	$(IMG_DLL_FILE)
	wish${TCL_VERSION} demo.tcl

demo.c:    demo.tcl
	tcl2c -o demo.c demo.tcl -a -img

demo.exe:    $(IMG_LIB_FILE) demo.c
	$(CC) -o demo.exe -DSHARED $(CC_SWITCHES) demo.c -L. $(IMG_LIB_FILE) -L$(IMG_RUNTIME_DIR) -ltk${TCL_VERSION} -ltcl${TCL_VERSION} $(winlibs)

# -ljpeg -ltiff -lpng -lz -lm -ldl

install-all:	install install-z install-png install-jpeg install-tiff

install: $(IMG_LIB_FILE)
	@$(tool)/mkinstalldirs $(LIB_INSTALL_DIR)
	@echo "Installing $(IMG_LIB_FILE)"
	@$(INSTALL_SHLIB) $(IMG_LIB_FILE) $(LIB_INSTALL_DIR)
	@$(RANLIB) $(LIB_INSTALL_DIR)/$(IMG_LIB_FILE)
	@echo "Installing pkgIndex.tcl"
	@$(INSTALL_DATA) $(srcdir)/pkgIndex.tcl $(LIB_INSTALL_DIR)

install-z:	$(Z_LIB_FILE)
	@$(tool)/mkinstalldirs $(IMG_INSTALL_DIR) $(INCLUDE_INSTALL_DIR)
	@echo "Installing $(Z_LIB_FILE)"
	@$(INSTALL_SHLIB) $(Z_LIB_FILE) $(IMG_INSTALL_DIR)
	@$(RANLIB) $(IMG_INSTALL_DIR)/$(Z_LIB_FILE)
	@for i in zlib.h zconf.h; do \
	    echo "Installing $$i"; \
	    $(INSTALL_DATA) $(zlibdir)/$$i $(INCLUDE_INSTALL_DIR); \
	done;

install-png:	$(PNG_LIB_FILE)
	@$(tool)/mkinstalldirs $(IMG_INSTALL_DIR) $(INCLUDE_INSTALL_DIR)
	@echo "Installing $(PNG_LIB_FILE)"
	@$(INSTALL_SHLIB) $(PNG_LIB_FILE) $(IMG_INSTALL_DIR)
	@for i in png.h pngconf.h; do \
	    echo "Installing $$i"; \
	    $(INSTALL_DATA) $(pngdir)/$$i $(INCLUDE_INSTALL_DIR); \
	done;

install-jpeg:	$(JPEG_LIB_FILE)
	@$(tool)/mkinstalldirs $(IMG_INSTALL_DIR) $(INCLUDE_INSTALL_DIR)
	@echo "Installing $(JPEG_LIB_FILE)"
	@$(INSTALL_SHLIB) $(JPEG_LIB_FILE) $(IMG_INSTALL_DIR)
	@for i in jpeglib.h jconfig.h jmorecfg.h jerror.h; do \
	    echo "Installing $$i"; \
	    $(INSTALL_DATA) $(jpegdir)/$$i $(INCLUDE_INSTALL_DIR); \
	done;

install-tiff:	$(TIFF_LIB_FILE)
	@$(tool)/mkinstalldirs $(IMG_INSTALL_DIR) $(INCLUDE_INSTALL_DIR)
	@echo "Installing $(TIFF_LIB_FILE)"
	@$(INSTALL_SHLIB) $(TIFF_LIB_FILE) $(IMG_INSTALL_DIR)
	@for i in tiff.h tiffio.h tiffconf.h; do \
	    echo "Installing $$i"; \
	    $(INSTALL_DATA) $(tiffdir)/$$i $(INCLUDE_INSTALL_DIR); \
	done;


#-------------------------------------------------------#

.c.o:
	$(CC) -c $(CC_SWITCHES) $< -o $@

#-------------------------------------------------------#

$(IMG_DLL_FILE):	$(OBJECTS) imgres.o $(TMPDIR)/compat/dllEntry.o
	$(DLLWRAP) -s $(DLL_LDFLAGS) -mno-cygwin -o $@ $(OBJECTS) \
		imgres.o $(TMPDIR)/compat/dllEntry.o --def img.def \
		-L$(IMG_RUNTIME_DIR) $(DLL_LDLIBS) \
		-ltkstub$(TCL_VER) -ltclstub$(TCL_VER) 

$(IMG_LIB_FILE):	$(OBJECTS)
	$(AR) cr $(IMG_LIB_FILE) $(OBJECTS)

tclLoadWin.o:	compat/tclLoadWin.c
	$(CC) -c $(CC_SWITCHES) -o $@ compat/tclLoadWin.c


#img.def:	$(OBJECTS)
#	$(DLLTOOL) --export-all --exclude-symbols DllMain@12 --output-def $@ $(OBJECTS)

imgres.o:	img.rc
	$(WINDRES) --include . --include $(TCL_INC_DIR) --define VS_VERSION_INFO=1 img.rc -o imgres.o


#-------------------------------------------------------#
# support for external libraries (libz, libpng, libjpeg, libtiff, libttf, libungif)

$(tiffdir)/tif_fax3sm.c: $(tiffdir)/mkg3states.c $(tiffdir)/tif_fax3.h
	${CC} -o $(tiffdir)/mkg3states ${CFLAGS} $(tiffdir)/mkg3states.c
	rm -f $(tiffdir)/tif_fax3sm.c
	$(tiffdir)/mkg3states -c const $(tiffdir)/tif_fax3sm.c

ZLIB_SRC	=	\
	$(zlibdir)/adler32.c	$(zlibdir)/compress.c	$(zlibdir)/crc32.c	\
	$(zlibdir)/deflate.c	$(zlibdir)/gzio.c	$(zlibdir)/infblock.c	\
	$(zlibdir)/infcodes.c	$(zlibdir)/inffast.c	$(zlibdir)/inflate.c	\
	$(zlibdir)/inftrees.c	$(zlibdir)/infutil.c	$(zlibdir)/trees.c	\
	$(zlibdir)/uncompr.c	$(zlibdir)/zutil.c

ZLIB_OBJ	=	\
	$(zlibdir)/adler32.o	$(zlibdir)/compress.o	$(zlibdir)/crc32.o	\
	$(zlibdir)/deflate.o	$(zlibdir)/gzio.o	$(zlibdir)/infblock.o	\
	$(zlibdir)/infcodes.o	$(zlibdir)/inffast.o	$(zlibdir)/inflate.o	\
	$(zlibdir)/inftrees.o	$(zlibdir)/infutil.o	$(zlibdir)/trees.o	\
	$(zlibdir)/uncompr.o	$(zlibdir)/zutil.o


PNG_SRC	=	\
	$(pngdir)/png.c      $(pngdir)/pngerror.c $(pngdir)/pngmem.c	\
	$(pngdir)/pngpread.c $(pngdir)/pngread.c  $(pngdir)/pngrio.c   	\
	$(pngdir)/pngrtran.c $(pngdir)/pngrutil.c $(pngdir)/pngset.c   \
	$(pngdir)/pngtrans.c $(pngdir)/pngwio.c   $(pngdir)/pngwrite.c \
	$(pngdir)/pngwtran.c $(pngdir)/pngwutil.c $(pngdir)/pngget.c \
	$(pngdir)/pnggccrd.c

PNG_OBJ	=	\
	$(pngdir)/png.o      $(pngdir)/pngerror.o $(pngdir)/pngmem.o	\
        $(pngdir)/pngpread.o $(pngdir)/pngread.o  $(pngdir)/pngrio.o    \
	$(pngdir)/pngrtran.o $(pngdir)/pngrutil.o $(pngdir)/pngset.o   \
	$(pngdir)/pngtrans.o $(pngdir)/pngwio.o   $(pngdir)/pngwrite.o \
	$(pngdir)/pngwtran.o $(pngdir)/pngwutil.o $(pngdir)/pngget.o \
	$(pngdir)/pnggccrd.o


JPEG_SRC	=	\
	$(jpegdir)/jcapimin.c $(jpegdir)/jcapistd.c $(jpegdir)/jccoefct.c \
	$(jpegdir)/jccolor.c  $(jpegdir)/jcdctmgr.c $(jpegdir)/jchuff.c \
	$(jpegdir)/jcinit.c   $(jpegdir)/jcmainct.c $(jpegdir)/jcmarker.c \
	$(jpegdir)/jcmaster.c $(jpegdir)/jcomapi.c  $(jpegdir)/jcparam.c \
	$(jpegdir)/jcphuff.c  $(jpegdir)/jcprepct.c $(jpegdir)/jcsample.c \
	$(jpegdir)/jctrans.c  $(jpegdir)/jdapimin.c $(jpegdir)/jdapistd.c \
	$(jpegdir)/jdatadst.c $(jpegdir)/jdatasrc.c $(jpegdir)/jdcoefct.c \
	$(jpegdir)/jdcolor.c  $(jpegdir)/jddctmgr.c $(jpegdir)/jdhuff.c \
	$(jpegdir)/jdinput.c  $(jpegdir)/jdmainct.c $(jpegdir)/jdmarker.c \
	$(jpegdir)/jdmaster.c $(jpegdir)/jdmerge.c  $(jpegdir)/jdphuff.c \
	$(jpegdir)/jdpostct.c $(jpegdir)/jdsample.c $(jpegdir)/jdtrans.c \
	$(jpegdir)/jerror.c   $(jpegdir)/jfdctflt.c $(jpegdir)/jfdctfst.c \
	$(jpegdir)/jfdctint.c $(jpegdir)/jidctflt.c $(jpegdir)/jidctfst.c \
	$(jpegdir)/jidctint.c $(jpegdir)/jidctred.c $(jpegdir)/jquant1.c \
	$(jpegdir)/jquant2.c  $(jpegdir)/jutils.c   $(jpegdir)/jmemmgr.c \
	$(jpegdir)/jmemansi.c

JPEG_OBJ	=	\
	$(jpegdir)/jcapimin.o $(jpegdir)/jcapistd.o $(jpegdir)/jccoefct.o \
	$(jpegdir)/jccolor.o  $(jpegdir)/jcdctmgr.o $(jpegdir)/jchuff.o \
	$(jpegdir)/jcinit.o   $(jpegdir)/jcmainct.o $(jpegdir)/jcmarker.o \
	$(jpegdir)/jcmaster.o $(jpegdir)/jcomapi.o  $(jpegdir)/jcparam.o \
	$(jpegdir)/jcphuff.o  $(jpegdir)/jcprepct.o $(jpegdir)/jcsample.o \
	$(jpegdir)/jctrans.o  $(jpegdir)/jdapimin.o $(jpegdir)/jdapistd.o \
	$(jpegdir)/jdatadst.o $(jpegdir)/jdatasrc.o $(jpegdir)/jdcoefct.o \
	$(jpegdir)/jdcolor.o  $(jpegdir)/jddctmgr.o $(jpegdir)/jdhuff.o \
	$(jpegdir)/jdinput.o  $(jpegdir)/jdmainct.o $(jpegdir)/jdmarker.o \
	$(jpegdir)/jdmaster.o $(jpegdir)/jdmerge.o  $(jpegdir)/jdphuff.o \
	$(jpegdir)/jdpostct.o $(jpegdir)/jdsample.o $(jpegdir)/jdtrans.o \
	$(jpegdir)/jerror.o   $(jpegdir)/jfdctflt.o $(jpegdir)/jfdctfst.o \
	$(jpegdir)/jfdctint.o $(jpegdir)/jidctflt.o $(jpegdir)/jidctfst.o \
	$(jpegdir)/jidctint.o $(jpegdir)/jidctred.o $(jpegdir)/jquant1.o \
	$(jpegdir)/jquant2.o  $(jpegdir)/jutils.o   $(jpegdir)/jmemmgr.o \
	$(jpegdir)/jmemansi.o

TIFF_SRC	=	\
	$(tiffdir)/tif_aux.c      $(tiffdir)/tif_close.c    $(tiffdir)/tif_codec.c \
	$(tiffdir)/tif_compress.c $(tiffdir)/tif_dir.c      $(tiffdir)/tif_dirinfo.c \
	$(tiffdir)/tif_dirread.c  $(tiffdir)/tif_dirwrite.c $(tiffdir)/tif_dumpmode.c \
	$(tiffdir)/tif_error.c    $(tiffdir)/tif_fax3.c     $(tiffdir)/tif_fax3sm.c \
	$(tiffdir)/tif_getimage.c $(tiffdir)/tif_flush.c    $(tiffdir)/tif_luv.c \
	$(tiffdir)/tif_lzw.c      $(tiffdir)/tif_next.c     $(tiffdir)/tif_open.c \
	$(tiffdir)/tif_packbits.c $(tiffdir)/tif_predict.c  $(tiffdir)/tif_print.c \
	$(tiffdir)/tif_read.c     $(tiffdir)/tif_swab.c     $(tiffdir)/tif_strip.c \
	$(tiffdir)/tif_thunder.c  $(tiffdir)/tif_tile.c     $(tiffdir)/tif_unix.c \
	$(tiffdir)/tif_version.c  $(tiffdir)/tif_warning.c  $(tiffdir)/tif_write.c

TIFF_OBJ	=	\
	$(tiffdir)/tif_aux.o      $(tiffdir)/tif_close.o    $(tiffdir)/tif_codec.o \
	$(tiffdir)/tif_compress.o $(tiffdir)/tif_dir.o      $(tiffdir)/tif_dirinfo.o \
	$(tiffdir)/tif_dirread.o  $(tiffdir)/tif_dirwrite.o $(tiffdir)/tif_dumpmode.o \
	$(tiffdir)/tif_error.o    $(tiffdir)/tif_fax3.o     $(tiffdir)/tif_fax3sm.o \
	$(tiffdir)/tif_getimage.o $(tiffdir)/tif_flush.o    $(tiffdir)/tif_luv.o \
	$(tiffdir)/tif_lzw.o      $(tiffdir)/tif_next.o     $(tiffdir)/tif_open.o \
	$(tiffdir)/tif_packbits.o $(tiffdir)/tif_predict.o  $(tiffdir)/tif_print.o \
	$(tiffdir)/tif_read.o     $(tiffdir)/tif_swab.o     $(tiffdir)/tif_strip.o \
	$(tiffdir)/tif_thunder.o  $(tiffdir)/tif_tile.o     $(tiffdir)/tif_unix.o \
	$(tiffdir)/tif_version.o  $(tiffdir)/tif_warning.o  $(tiffdir)/tif_write.o

z:	$(Z_LIB_FILE) $(Z_STATIC_LIB_FILE) $(Z_DLL_FILE)
	@echo ... done

$(Z_DLL_FILE):	$(ZLIB_OBJ) $(TMPDIR)/compat/dllEntry.o
	$(DLLWRAP) -s $(DLL_LDFLAGS) -mno-cygwin -o $@ $(ZLIB_OBJ) $(TMPDIR)/compat/dllEntry.o --def z.def -L$(IMG_RUNTIME_DIR) $(DLL_LDLIBS)

$(Z_STATIC_LIB_FILE):	$(ZLIB_OBJ)
	rm -f		$(Z_STATIC_LIB_FILE)
	$(AR) cr $(Z_STATIC_LIB_FILE) $(ZLIB_OBJ)

$(Z_LIB_FILE): zexp.def
	$(DLLTOOL) -k --as=$(AS) --dllname ZLIB.DLL \
	--def $(TMPDIR)/zexp.def --output-lib $(Z_LIB_FILE)

png:	$(PNG_LIB_FILE) $(PNG_STATIC_LIB_FILE) $(PNG_DLL_FILE)
	@echo ... done

$(PNG_DLL_FILE):	$(PNG_OBJ) $(TMPDIR)/compat/dllEntry.o
	$(DLLWRAP) -s $(DLL_LDFLAGS) -mno-cygwin -o $@ $(PNG_OBJ) $(TMPDIR)/compat/dllEntry.o --def libpng/msvc/png32ms.def -L. -L$(IMG_RUNTIME_DIR) -lz $(DLL_LDLIBS)

$(PNG_STATIC_LIB_FILE):	$(PNG_OBJ)
	rm -f		$(PNG_STATIC_LIB_FILE)
	$(AR) cr $(PNG_STATIC_LIB_FILE) $(PNG_OBJ)

$(PNG_LIB_FILE):	libpng/msvc/png32ms.def
	$(DLLTOOL) --as=$(AS) --dllname PNG.DLL \
	  --def $(TMPDIR)/libpng/msvc/png32ms.def --output-lib $(PNG_LIB_FILE)

jpeg:	$(JPEG_LIB_FILE) $(JPEG_STATIC_LIB_FILE) $(JPEG_DLL_FILE)
	@echo ... done

$(JPEG_DLL_FILE):	$(JPEG_OBJ) $(TMPDIR)/compat/dllEntry.o
	$(DLLWRAP) -s $(DLL_LDFLAGS) -mno-cygwin -o $@ $(JPEG_OBJ) $(TMPDIR)/compat/dllEntry.o --def jpeg.def -L$(IMG_RUNTIME_DIR) $(DLL_LDLIBS)

$(JPEG_STATIC_LIB_FILE):	$(JPEG_OBJ)
	rm -f		$(JPEG_STATIC_LIB_FILE)
	$(AR) cr $(JPEG_STATIC_LIB_FILE) $(JPEG_OBJ)

$(JPEG_LIB_FILE): jpeg.def
	$(DLLTOOL) --as=$(AS) --dllname JPEG62.DLL \
	  --def $(TMPDIR)/jpeg.def --output-lib $(JPEG_LIB_FILE)

jpeg.def: $(JPEG_OBJ)
	$(DLLTOOL) --export-all --exclude-symbols DllMain@12 --output-def $@ $(JPEG_OBJ)

tiff:	$(TIFF_LIB_FILE) $(TIFF_STATIC_LIB_FILE) $(TIFF_DLL_FILE)
	@echo ... done

$(TIFF_DLL_FILE):	$(TIFF_OBJ) $(TMPDIR)/compat/dllEntry.o
	$(DLLWRAP) -s $(DLL_LDFLAGS) -mno-cygwin -o $@ $(TIFF_OBJ) $(TMPDIR)/compat/dllEntry.o --def tiff.def -L$(IMG_RUNTIME_DIR) $(DLL_LDLIBS)

$(TIFF_STATIC_LIB_FILE):	$(TIFF_OBJ)
	rm -f		$(TIFF_STATIC_LIB_FILE)
	$(AR) cr $(TIFF_STATIC_LIB_FILE) $(TIFF_OBJ)

$(TIFF_LIB_FILE): tiff.def
	$(DLLTOOL) --as=$(AS) --dllname TIFF.DLL \
	  --def $(TMPDIR)/tiff.def --output-lib $(TIFF_LIB_FILE)

tiff.def: $(TIFF_OBJ)
	$(DLLTOOL) --export-all --exclude-symbols DllMain@12 --output-def $@ $(TIFF_OBJ)


#-------------------------------------------------------#

clean:
	del $(OBJECTS) $(IMG_LIB_FILE)
	del $(ZLIB_OBJ) $(PNG_OBJ) $(JPEG_OBJ) $(TIFF_OBJ)
	del $(Z_LIB_FILE) $(PNG_LIB_FILE) $(JPEG_LIB_FILE)
	del $(TIFF_LIB_FILE)
	del TAGS depend *~ */*~ core* tests/core* so_locations lib*.so*
	del img$(VERSION)

distclean:	clean
	del config.* $(jpegdir)/config.log $(jpegdir)/config.status
	del Makefile $(jpegdir)/Makefile $(jpegdir)/jconfig.h
	del $(tiffdir)/port.h $(tiffdir)/Makefile
	del pkgIndex.tcl

#-------------------------------------------------------#
# DO NOT DELETE THIS LINE -- make depend depends on it.
