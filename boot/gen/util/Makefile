#
# Until I can remove the dependency on the appkit,
# we'll just keep the generated files in this directory
# and install them directly, rather than generating them again.
#

DIR = util
include ../MakePaths.dir

VPATH = $(OBJROOT):$(SYMROOT)

INSTALLDIR = $(DSTROOT)/usr/standalone/i386
LANGDIR = $(INSTALLDIR)/English.lproj

CFLAGS = -Wmost -Wno-precomp -g

CFILES = machOconv.c mkfont.c tif_packbits.c
MFILES = dumptiff.m
HFILES = cursor.h
EXPORT_HFILES = bitmap.h font.h
ALLSRC = $(CFILES) $(MFILES) $(HFILES) $(EXPORT_HFILES)

TIFFILES = return.tiff ns_box.tiff ns_text.tiff ns_logo.tiff dot.tiff
TIFF_HFILES = $(TIFFILES:.tiff=.h)
TIFF_BFILES = $(TIFFILES:.tiff=_bitmap.h)

CURSOR_HFILES = ns_wait1.h ns_wait1_bitmap.h \
		ns_wait2.h ns_wait2_bitmap.h \
		ns_wait3.h ns_wait3_bitmap.h
OTHER_HFILES = hdot.h hdot_bitmap.h

FONTFILES = 14.TimesIta
FONT_HFILES = FontBitmap.h

#PROGRAMS = machOconv mkfont dumptiff sig
PROGRAMS = machOconv

OUTFILES = $(PROGRAMS) $(TIFF_HFILES) $(TIFF_BFILES) $(CURSOR_HFILES) \
		$(FONT_HFILES) $(OTHER_HFILES)
DUMPTIFF = $(SYMROOT)/dumptiff
DUMPTIFF_OBJS = tif_packbits.o dumptiff.o BooterBitmap.o
SIG = $(SYMROOT)/sig
SIG_OBJS = sig.o

DIRS_NEEDED = $(OBJROOT) $(SYMROOT) $(LANGDIR)

#BITMAPS = Panel.image Wait1.image Wait2.image Wait3.image
BITMAPS = Panel.image
FONTS = Default.font

.SUFFIXES: .tiff
.tiff.h:
	$(DUMPTIFF) -o $(SYMROOT)/$* $<

#all: $(DIRS_NEEDED) $(PROGRAMS) $(OUTFILES)
all: $(DIRS_NEEDED) $(PROGRAMS) $(BITMAPS)

#clean::
#	cd $(SYMROOT); rm -f $(OUTFILES)
clean::
	cd $(SYMROOT); rm -f $(PROGRAMS)

install_i386:: $(INSTALLDIR) $(LANGDIR)
	cp $(BITMAPS) $(INSTALLDIR)
	cp $(FONTS) $(INSTALLDIR)/English.lproj

$(TIFF_HFILES): $(DUMPTIFF)
$(TIFF_BFILES): $(TIFF_HFILES)
$(CURSOR_HFILES): CURSOR_HFILES
CURSOR_HFILES: $(DUMPTIFF)
	$(DUMPTIFF) -c -o $(SYMROOT)/ns_wait
$(FONT_HFILES): mkfont $(FONTFILES)
	mkfont $(FONTFILES) -c $(SYMROOT)/$@
	
hdot.h hdot_bitmap.h: dot.tiff $(DUMPTIFF)
	$(DUMPTIFF) -b 3 -o $(SYMROOT)/hdot dot.tiff

sig: $(SIG_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(DEFINES) -o $(SYMROOT)/$@ $(SIG_OBJS)

dumptiff: $(DUMPTIFF_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(DEFINES) -o $(SYMROOT)/$@ $(DUMPTIFF_OBJS) -lNeXT_s
	
mkfont: mkfont.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(DEFINES) -o $(SYMROOT)/$@ mkfont.o
	
machOconv: machOconv.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(DEFINES) -o $(SYMROOT)/$@ machOconv.o

include ../MakeInc.dir

#dependencies
-include $(OBJROOT)/Makedep

