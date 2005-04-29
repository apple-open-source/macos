#
# config.mk.in -- autoconf template for Vim on Unix		vim:ts=8:sw=8:
#
# DO NOT EDIT config.mk!!  It will be overwritten by configure.
# Edit Makefile and run "make" or run ./configure with other arguments.
#
# Configure does not edit the makefile directly. This method is not the
# standard use of GNU autoconf, but it has two advantages:
#   a) The user can override every choice made by configure.
#   b) Modifications to the makefile are not lost when configure is run.
#
# I hope this is worth being nonstandard. jw.



VIMNAME		= vim
EXNAME		= ex
VIEWNAME	= view

CC		= gcc
DEFS		= -DHAVE_CONFIG_H
CFLAGS		= -O2 -fno-strength-reduce -Wall
CPPFLAGS	= 
srcdir		= .

LDFLAGS		= -L/usr/local/lib
LIBS		= -lncurses  -liconv
TAGPRG		= ctags -t

CPP		= gcc -E
CPP_MM		= M
X_CFLAGS	= -I/usr/X11R6/include 
X_LIBS_DIR	= -L/usr/X11R6/lib 
X_PRE_LIBS	=  -lSM -lICE -lXpm
X_EXTRA_LIBS	=  -lSM -lICE
X_LIBS		= -lXt -lX11

PERL		= 
PERLLIB		= 
PERL_LIBS	= 
SHRPENV		= 
PERL_SRC	= 
PERL_OBJ	= 
PERL_PRO	= 
PERL_CFLAGS	= 

PYTHON_SRC	= 
PYTHON_OBJ	= 
PYTHON_CFLAGS	= 
PYTHON_LIBS	= 
PYTHON_CONFDIR	= 
PYTHON_GETPATH_CFLAGS = 

TCL		= 
TCL_SRC		= 
TCL_OBJ		= 
TCL_PRO		= 
TCL_CFLAGS	= 
TCL_LIBS	= 

HANGULIN_SRC	= 
HANGULIN_OBJ	= 

WORKSHOP_SRC	= 
WORKSHOP_OBJ	= 

NETBEANS_SRC	= netbeans.c
NETBEANS_OBJ	= objects/netbeans.o

RUBY		= 
RUBY_SRC	= 
RUBY_OBJ	= 
RUBY_PRO	= 
RUBY_CFLAGS	= 
RUBY_LIBS	= 

SNIFF_SRC	= 
SNIFF_OBJ	= 

AWK		= awk

STRIP		= strip

EXEEXT		= 

COMPILEDBY	= 

INSTALLVIMDIFF	= installvimdiff
INSTALLGVIMDIFF	= installgvimdiff

### Line break character as octal number for "tr"
NL		= "\\012"

### Top directory for everything
prefix		= /usr/local

### Top directory for the binary
exec_prefix	= ${prefix}

### Prefix for location of data files
BINDIR		= ${exec_prefix}/bin

### Prefix for location of data files
DATADIR		= ${prefix}/share

### Prefix for location of man pages
MANDIR		= ${prefix}/man

### Do we have a GUI
GUI_INC_LOC	= 
GUI_LIB_LOC	= 
GUI_SRC		= $(ATHENA_SRC)
GUI_OBJ		= $(ATHENA_OBJ)
GUI_DEFS	= $(ATHENA_DEFS)
GUI_IPATH	= $(ATHENA_IPATH)
GUI_LIBS_DIR	= $(ATHENA_LIBS_DIR)
GUI_LIBS1	= $(ATHENA_LIBS1)
GUI_LIBS2	= $(ATHENA_LIBS2)
GUI_TARGETS	= $(ATHENA_TARGETS)
GUI_MAN_TARGETS	= $(ATHENA_MAN_TARGETS)
GUI_TESTTARGET	= $(ATHENA_TESTTARGET)
NARROW_PROTO	= 
GUI_X_LIBS	= -lXmu -lXext
MOTIF_LIBNAME	= 
GTK_LIBNAME	= 

### Any OS dependent extra source and object file
OS_EXTRA_SRC	= 
OS_EXTRA_OBJ	= 

### If the *.po files are to be translated to *.mo files.
MAKEMO		= 

# Make sure that "make first" will run "make all" once configure has done its
# work.  This is needed when using the Makefile in the top directory.
first: all
