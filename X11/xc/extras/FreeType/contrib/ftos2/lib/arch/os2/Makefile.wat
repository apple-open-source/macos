# This file is part of the FreeType project.
# Modified for FTIFI - Mike
#
# It builds the library and test programs using Watcom C/C++ under OS/2.
#
# You will need nmake!!
# If you can change this makefile to work with wmake, please
# inform me.
#
# Use this file while in the lib directory with the following statement:
#
#   nmake -f arch\os2\Makefile.icc

CC = wcc386
CFLAGS = -4r -Otexan -zp1 -bd -zc -Iarch\os2 -I. -Iextend

SRC = ttapi.c    ttcache.c ttcalc.c   ttcmap.c   tterror.c  \
      ttfile.c   ttgload.c ttinterp.c ttlists.c  ttload.c   \
      ttmemory.c ttmutex.c ttobjs.c   ttraster.c ttextend.c \
      \
      extend\ftxgasp.c extend\ftxkern.c

OBJ = $(SRC:.c=.obj)

.c.obj:
   $(CC) $(CFLAGS) $*.c

all: libttf.lib

libttf.lib: $(OBJ)
        -move ft*.obj extend
        !wlib -c $@ -+$?

clean:
        -del *.obj
        -del extend\*.obj

distclean: clean
        -del libttf.lib

# end of Makefile.wcc
