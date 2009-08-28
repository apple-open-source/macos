#
#----------------------------------------------------------------
# This file is a Makefile for Memchan 2.2a4 (as of AUG-20-2002), usable for CygWin B20.1
# Donated by Jan Nijtmans <jan.nijtmans@cmg.nl> <nijtmans@wxs.nl>
#----------------------------------------------------------------

EXTENSION	= Memchan
VERSION		= 2.2a4
TCL_VERSION	= 81

MEMCHAN_DLL_FILE= memchan22.dll

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
#TCL_LIB_SPEC	= /progra~1/tcl/lib/libtclstub81.a
TCL_LIB_SPEC	= /progra~1/tcl/lib/libtclstub82.a

# Libraries to be included with memchan.dll
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


# Directory where memchan.dll is at run-time:
LIB_RUNTIME_DIR =	$(exec_prefix)/lib/$(EXTENSION)$(VERSION)


# Directory in which to install the archive memchan.dll:
LIB_INSTALL_DIR =	$(INSTALL_ROOT)$(LIB_RUNTIME_DIR)


# Directory in which to install the extended shell tclsh:
BIN_INSTALL_DIR =	$(INSTALL_ROOT)$(exec_prefix)/bin


# Directory in which to install the include file transform.h:
INCLUDE_INSTALL_DIR =	$(INSTALL_ROOT)$(prefix)/include


# Top-level directory in which to install manual entries:
MAN_INSTALL_DIR =	$(INSTALL_ROOT)$(prefix)/man

# To change the compiler switches, for example to change from -O
# to -g, change the following line:
#CFLAGS		=	-O2 -mno-cygwin -DNDEBUG -D__WIN32__
CFLAGS		=	-O2 -mno-cygwin -DNDEBUG -D__WIN32__ -DTCL_THREADS

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

MEMCHAN_SHLIB_CFLAGS =


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
#DLL_LDLIBS = -L/progra~1/tcl/lib -ltclstub81
DLL_LDLIBS = -L/progra~1/tcl/lib -ltclstub82

baselibs   = -lkernel32 $(optlibs) -ladvapi32
winlibs    = $(baselibs) -luser32 -lgdi32 -lcomdlg32 -lwinspool
guilibs	   = $(libc) $(winlibs)

guilibsdll = $(libcdll) $(winlibs)

MEMCHAN_DEFINES	= -D__WIN32__ -DSTATIC_BUILD -DUSE_TCL_STUBS -DMEMCHAN_VERSION="\"${VERSION}\"" -DHAVE_LTOA

# $(TCL_CC_SWITCHES)
INCLUDES	=	-I. -I$(srcdir) -I$(TCL_INC_DIR)
DEFINES		=	$(PROTO_FLAGS) $(MEM_DEBUG_FLAGS) $(MEMCHAN_SHLIB_CFLAGS) \
			$(MEMCHAN_DEFINES)

CC_SWITCHES	=	$(CFLAGS) $(DEFINES) $(INCLUDES)

#		fundamentals of this library
SOURCES	=	../generic/counter.c \
		../generic/fifo.c \
		../generic/init.c \
		../generic/memchan.c

OBJECTS	=	counter.o \
		fifo.o \
		init.o \
		memchan.o

#-------------------------------------------------------#

default:	$(MEMCHAN_DLL_FILE)

all:	default

test:	$(MEMCHAN_DLL_FILE)
	wish${TK_VERSION} demo.tcl


#-------------------------------------------------------#

counter.o:	../generic/counter.c
	$(CC) -c $(CC_SWITCHES) ../generic/counter.c -o $@

fifo.o:	../generic/fifo.c
	$(CC) -c $(CC_SWITCHES) ../generic/fifo.c -o $@

init.o:	../generic/init.c
	$(CC) -c $(CC_SWITCHES) ../generic/init.c -o $@

memchan.o:	../generic/memchan.c
	$(CC) -c $(CC_SWITCHES) ../generic/memchan.c -o $@

dllEntry.o:	dllEntry.c
	$(CC) -c $(CC_SWITCHES) dllEntry.c -o $@

#-------------------------------------------------------#

$(MEMCHAN_DLL_FILE):	$(OBJECTS) mcres.o dllEntry.o memchan.def
	$(DLLWRAP) -s $(DLL_LDFLAGS) -mno-cygwin -o $@ $(OBJECTS) \
		mcres.o dllEntry.o --def memchan.def \
		$(DLL_LDLIBS) 

memchan.def: $(OBJECTS)
	$(DLLTOOL) --export-all --exclude-symbols DllMain@12 --output-def $@ $(OBJECTS)

mcres.o: mc.rc
	$(WINDRES) --include . --define VS_VERSION_INFO=1 mc.rc mcres.o


#-------------------------------------------------------#

clean:
	del $(OBJECTS) $(MEMCHAN_DLL_FILE)
	del TAGS depend *~ */*~ core* tests/core* so_locations lib*.so*

distclean:	clean
	del config.* $(jpegdir)/config.log $(jpegdir)/config.status
	del Makefile
	del pkgIndex.tcl

#-------------------------------------------------------#
# DO NOT DELETE THIS LINE -- make depend depends on it.
