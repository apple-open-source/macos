#
# Until I can remove the dependency on the appkit,
# we'll just keep the generated files in this directory
# and install them directly, rather than generating them again.
#

DIR = util
include ../MakePaths.dir

VPATH = $(OBJROOT):$(SYMROOT)

INSTALLDIR = $(DSTROOT)/usr/standalone/i386
LOCALBIN = $(DSTROOT)/usr/local/bin

OPTIM = -Os
CFLAGS = $(RC_CFLAGS) $(OPTIM) -Wmost -Werror -g
LDFLAGS =
CFILES = machOconv.c
ALLSRC = $(CFILES) $(MFILES) $(HFILES) $(EXPORT_HFILES)

PROGRAMS = machOconv

OUTFILES = $(PROGRAMS)

DIRS_NEEDED = $(OBJROOT) $(SYMROOT) $(LANGDIR)

all: $(DIRS_NEEDED) $(PROGRAMS)

clean::
	-(cd $(SYMROOT); rm -f $(PROGRAMS))

machOconv: machOconv.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(DEFINES) -o $(SYMROOT)/$(@F) machOconv.o

include ../MakeInc.dir

#dependencies
-include $(OBJROOT)/Makedep

