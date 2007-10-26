/*
 * $Xorg: xsetroot.c,v 1.4 2001/02/09 02:05:59 xorgcvs Exp $
 *
Copyright 1987, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.
 */

/*
 * xsetroot.c 	MIT Project Athena, X Window System root window 
 *		parameter setting utility.  This program will set 
 *		various parameters of the X root window.
 *
 *  Author:	Mark Lillibridge, MIT Project Athena
 *		11-Jun-87
 */
/* $XFree86: xc/programs/xsetroot/xsetroot.c,v 1.7 2001/04/01 14:00:24 tsi Exp $ */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xmu/CurUtil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "X11/bitmaps/gray"

#define Dynamic 1

char *program_name;
Display *dpy;
int screen;
Window root;
char *fore_color = NULL;
char *back_color = NULL;
int reverse = 0;
int save_colors = 0;
int unsave_past = 0;
Pixmap save_pixmap = (Pixmap)None;

static void usage(void);
static void FixupState(void);
static void SetBackgroundToBitmap(Pixmap bitmap, 
				  unsigned int width, unsigned int height);
static Cursor CreateCursorFromFiles(char *cursor_file, char *mask_file);
static Cursor CreateCursorFromName(char *name);
static Pixmap MakeModulaBitmap(int mod_x, int mod_y);
static XColor NameToXColor(char *name, unsigned long pixel);
static unsigned long NameToPixel(char *name, unsigned long pixel);
static Pixmap ReadBitmapFile(char *filename, unsigned int *width, unsigned int *height, int *x_hot, int *y_hot);

static void
usage(void)
{
    fprintf(stderr, "usage: %s [options]\n", program_name);
    fprintf(stderr, "  where options are:\n");
    fprintf(stderr, "  -display <display>   or   -d <display>\n");
    fprintf(stderr, "  -fg <color>   or   -foreground <color>\n");
    fprintf(stderr, "  -bg <color>   or   -background <color>\n");
    fprintf(stderr, "  -rv   or   -reverse\n");
    fprintf(stderr, "  -help\n");
    fprintf(stderr, "  -def   or   -default\n");
    fprintf(stderr, "  -name <string>\n");
    fprintf(stderr, "  -cursor <cursor file> <mask file>\n");
    fprintf(stderr, "  -cursor_name <cursor-font name>\n");
    fprintf(stderr, "  -solid <color>\n");
    fprintf(stderr, "  -gray   or   -grey\n");
    fprintf(stderr, "  -bitmap <filename>\n");
    fprintf(stderr, "  -mod <x> <y>\n");
    exit(1);
    /*NOTREACHED*/
}


int
main(int argc, char *argv[]) 
{
    int excl = 0;
    int nonexcl = 0;
    int restore_defaults = 0;
    char *display_name = NULL;
    char *name = NULL;
    char *cursor_file = NULL;
    char *cursor_mask = NULL;
    char *cursor_name = NULL;
    char *solid_color = NULL;
    Cursor cursor;
    int gray = 0;
    char *bitmap_file = NULL;
    int mod_x = 0;
    int mod_y = 0;
    register int i;
    unsigned int ww, hh;
    Pixmap bitmap;

    program_name=argv[0];

    for (i = 1; i < argc; i++) {
	if (!strcmp ("-display", argv[i]) || !strcmp ("-d", argv[i])) {
	    if (++i>=argc) usage ();
	    display_name = argv[i];
	    continue;
	}
	if (!strcmp("-help", argv[i])) {
	    usage();
	}
	if (!strcmp("-def", argv[i]) || !strcmp("-default", argv[i])) {
	    restore_defaults = 1;
	    continue;
	}
	if (!strcmp("-name", argv[i])) {
	    if (++i>=argc) usage();
	    name = argv[i];
	    nonexcl++;
	    continue;
	}
	if (!strcmp("-cursor", argv[i])) {
	    if (++i>=argc) usage();
	    cursor_file = argv[i];
	    if (++i>=argc) usage();
	    cursor_mask = argv[i];
	    nonexcl++;
	    continue;
	}
	if (!strcmp("-cursor_name", argv[i])) {
	    if (++i>=argc) usage();
	    cursor_name = argv[i];
	    nonexcl++;
	    continue;
	}
	if (!strcmp("-fg",argv[i]) || !strcmp("-foreground",argv[i])) {
	    if (++i>=argc) usage();
	    fore_color = argv[i];
	    continue;
	}
	if (!strcmp("-bg",argv[i]) || !strcmp("-background",argv[i])) {
	    if (++i>=argc) usage();
	    back_color = argv[i];
	    continue;
	}
	if (!strcmp("-solid", argv[i])) {
	    if (++i>=argc) usage();
	    solid_color = argv[i];
	    excl++;
	    continue;
	}
	if (!strcmp("-gray", argv[i]) || !strcmp("-grey", argv[i])) {
	    gray = 1;
	    excl++;
	    continue;
	}
	if (!strcmp("-bitmap", argv[i])) {
	    if (++i>=argc) usage();
	    bitmap_file = argv[i];
	    excl++;
	    continue;
	}
	if (!strcmp("-mod", argv[i])) {
	    if (++i>=argc) usage();
	    mod_x = atoi(argv[i]);
	    if (mod_x <= 0) mod_x = 1;
	    if (++i>=argc) usage();
	    mod_y = atoi(argv[i]);
	    if (mod_y <= 0) mod_y = 1;
	    excl++;
	    continue;
	}
	if (!strcmp("-rv",argv[i]) || !strcmp("-reverse",argv[i])) {
	    reverse = 1;
	    continue;
	}
	usage();
    } 

    /* Check for multiple use of exclusive options */
    if (excl > 1) {
	fprintf(stderr, "%s: choose only one of {solid, gray, bitmap, mod}\n",
		program_name);
	usage();
    }

    dpy = XOpenDisplay(display_name);
    if (!dpy) {
	fprintf(stderr, "%s:  unable to open display '%s'\n",
		program_name, XDisplayName (display_name));
	exit (2);
    }
    screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);
  
    /* If there are no arguments then restore defaults. */
    if (!excl && !nonexcl)
	restore_defaults = 1;
  
    /* Handle a cursor file */
    if (cursor_file) {
	cursor = CreateCursorFromFiles(cursor_file, cursor_mask);
	XDefineCursor(dpy, root, cursor);
	XFreeCursor(dpy, cursor);
    }
  
    if (cursor_name) {
	cursor = CreateCursorFromName (cursor_name);
	if (cursor)
	{
	    XDefineCursor (dpy, root, cursor);
	    XFreeCursor (dpy, cursor);
	}
    }
    /* Handle -gray and -grey options */
    if (gray) {
	bitmap = XCreateBitmapFromData(dpy, root, gray_bits,
				       gray_width, gray_height);
	SetBackgroundToBitmap(bitmap, gray_width, gray_height);
    }
  
    /* Handle -solid option */
    if (solid_color) {
	XSetWindowBackground(dpy, root, NameToPixel(solid_color,
						    BlackPixel(dpy, screen)));
	XClearWindow(dpy, root);
	unsave_past = 1;
    }
  
    /* Handle -bitmap option */
    if (bitmap_file) {
	bitmap = ReadBitmapFile(bitmap_file, &ww, &hh, (int *)NULL, (int *)NULL);
	SetBackgroundToBitmap(bitmap, ww, hh);
    }
  
    /* Handle set background to a modula pattern */
    if (mod_x) {
	bitmap = MakeModulaBitmap(mod_x, mod_y);
	SetBackgroundToBitmap(bitmap, 16, 16);
    }
  
    /* Handle set name */
    if (name)
	XStoreName(dpy, root, name);
  
    /* Handle restore defaults */
    if (restore_defaults) {
	if (!cursor_file)
	    XUndefineCursor(dpy, root);
	if (!excl) {
	    XSetWindowBackgroundPixmap(dpy, root, (Pixmap) None);
	    XClearWindow(dpy, root);
	    unsave_past = 1;
	}
    }
  
    FixupState();
    XCloseDisplay(dpy);
    exit (0);
}


/* Free past incarnation if needed, and retain state if needed. */
static void
FixupState(void)
{
    Atom prop, type;
    int format;
    unsigned long length, after;
    unsigned char *data;

    if (!(DefaultVisual(dpy, screen)->class & Dynamic))
	unsave_past = 0;
    if (!unsave_past && !save_colors)
	return;
    prop = XInternAtom(dpy, "_XSETROOT_ID", False);
    if (unsave_past) {    
	(void)XGetWindowProperty(dpy, root, prop, 0L, 1L, True, AnyPropertyType,
				 &type, &format, &length, &after, &data);
	if ((type == XA_PIXMAP) && (format == 32) &&
	    (length == 1) && (after == 0))
	    XKillClient(dpy, *((Pixmap *)data));
	else if (type != None)
	    fprintf(stderr, "%s: warning: _XSETROOT_ID property is garbage\n",
		    program_name);
    }
    if (save_colors) {
	if (!save_pixmap)
	    save_pixmap = XCreatePixmap(dpy, root, 1, 1, 1);
	XChangeProperty(dpy, root, prop, XA_PIXMAP, 32, PropModeReplace,
			(unsigned char *) &save_pixmap, 1);
	XSetCloseDownMode(dpy, RetainPermanent);
    }
}

/*
 * SetBackgroundToBitmap: Set the root window background to a caller supplied 
 *                        bitmap.
 */
static void
SetBackgroundToBitmap(Pixmap bitmap, unsigned int width, unsigned int height)
{
    Pixmap pix;
    GC gc;
    XGCValues gc_init;

    gc_init.foreground = NameToPixel(fore_color, BlackPixel(dpy, screen));
    gc_init.background = NameToPixel(back_color, WhitePixel(dpy, screen));
    if (reverse) {
	unsigned long temp=gc_init.foreground;
	gc_init.foreground=gc_init.background;
	gc_init.background=temp;
    }
    gc = XCreateGC(dpy, root, GCForeground|GCBackground, &gc_init);
    pix = XCreatePixmap(dpy, root, width, height,
			(unsigned int)DefaultDepth(dpy, screen));
    XCopyPlane(dpy, bitmap, pix, gc, 0, 0, width, height, 0, 0, (unsigned long)1);
    XSetWindowBackgroundPixmap(dpy, root, pix);
    XFreeGC(dpy, gc);
    XFreePixmap(dpy, bitmap);
    if (save_colors)
	save_pixmap = pix;
    else
	XFreePixmap(dpy, pix);
    XClearWindow(dpy, root);
    unsave_past = 1;
}


/*
 * CreateCursorFromFiles: make a cursor of the right colors from two bitmap
 *                        files.
 */
#define BITMAP_HOT_DEFAULT 8

static Cursor
CreateCursorFromFiles(char *cursor_file, char *mask_file)
{
    Pixmap cursor_bitmap, mask_bitmap;
    unsigned int width, height, ww, hh;
    int x_hot, y_hot;
    Cursor cursor;
    XColor fg, bg, temp;

    fg = NameToXColor(fore_color, BlackPixel(dpy, screen));
    bg = NameToXColor(back_color, WhitePixel(dpy, screen));
    if (reverse) {
	temp = fg; fg = bg; bg = temp;
    }

    cursor_bitmap = ReadBitmapFile(cursor_file, &width, &height, &x_hot, &y_hot);
    mask_bitmap = ReadBitmapFile(mask_file, &ww, &hh, (int *)NULL, (int *)NULL);

    if (width != ww || height != hh) {
	fprintf(stderr, 
"%s: dimensions of cursor bitmap and cursor mask bitmap are different\n", 
		program_name);
	exit(1);
	/*NOTREACHED*/
    }

    if ((x_hot == -1) && (y_hot == -1)) {
	x_hot = BITMAP_HOT_DEFAULT;
	y_hot = BITMAP_HOT_DEFAULT;
    }
    if ((x_hot < 0) || (x_hot >= width) ||
	(y_hot < 0) || (y_hot >= height)) {
	fprintf(stderr, "%s: hotspot is outside cursor bounds\n", program_name);
	exit(1);
	/*NOTREACHED*/
    }

    cursor = XCreatePixmapCursor(dpy, cursor_bitmap, mask_bitmap, &fg, &bg,
				 (unsigned int)x_hot, (unsigned int)y_hot);
    XFreePixmap(dpy, cursor_bitmap);
    XFreePixmap(dpy, mask_bitmap);

    return(cursor);
}

static Cursor
CreateCursorFromName(char *name)
{
    XColor fg, bg, temp;
    int	    i;
    Font    fid;

    fg = NameToXColor(fore_color, BlackPixel(dpy, screen));
    bg = NameToXColor(back_color, WhitePixel(dpy, screen));
    if (reverse) {
	temp = fg; fg = bg; bg = temp;
    }
    i = XmuCursorNameToIndex (name);
    if (i == -1)
	return (Cursor) 0;
    fid = XLoadFont (dpy, "cursor");
    if (!fid)
	return (Cursor) 0;
    return XCreateGlyphCursor (dpy, fid, fid,
			       i, i+1, &fg, &bg);
}

/*
 * MakeModulaBitmap: Returns a modula bitmap based on an x & y mod.
 */
static Pixmap 
MakeModulaBitmap(int mod_x, int mod_y)
{
    int i;
    long pattern_line = 0;
    char modula_data[16*16/8];

    for (i=16; i--; ) {
	pattern_line <<=1;
	if ((i % mod_x) == 0) pattern_line |= 0x0001;
    }
    for (i=0; i<16; i++) {
	if ((i % mod_y) == 0) {
	    modula_data[i*2] = (char)0xff;
	    modula_data[i*2+1] = (char)0xff;
	} else {
	    modula_data[i*2] = pattern_line & 0xff;
	    modula_data[i*2+1] = (pattern_line>>8) & 0xff;
	}
    }

    return(XCreateBitmapFromData(dpy, root, modula_data, 16, 16));
}


/*
 * NameToXColor: Convert the name of a color to its Xcolor value.
 */
static XColor 
NameToXColor(char *name, unsigned long pixel)
{
    XColor c;
    
    if (!name || !*name) {
	c.pixel = pixel;
	XQueryColor(dpy, DefaultColormap(dpy, screen), &c);
    } else if (!XParseColor(dpy, DefaultColormap(dpy, screen), name, &c)) {
	fprintf(stderr, "%s: unknown color or bad color format: %s\n",
			program_name, name);
	exit(1);
	/*NOTREACHED*/
    }
    return(c);
}

static unsigned long 
NameToPixel(char *name, unsigned long pixel)
{
    XColor ecolor;

    if (!name || !*name)
	return pixel;
    if (!XParseColor(dpy,DefaultColormap(dpy,screen),name,&ecolor)) {
	fprintf(stderr,"%s:  unknown color \"%s\"\n",program_name,name);
	exit(1);
	/*NOTREACHED*/
    }
    if (!XAllocColor(dpy, DefaultColormap(dpy, screen),&ecolor)) {
	fprintf(stderr, "%s:  unable to allocate color for \"%s\"\n",
		program_name, name);
	exit(1);
	/*NOTREACHED*/
    }
    if ((ecolor.pixel != BlackPixel(dpy, screen)) &&
	(ecolor.pixel != WhitePixel(dpy, screen)) &&
	(DefaultVisual(dpy, screen)->class & Dynamic))
	save_colors = 1;
    return(ecolor.pixel);
}

static Pixmap 
ReadBitmapFile(char *filename, unsigned int *width, unsigned int *height, 
	       int *x_hot, int *y_hot)
{
    Pixmap bitmap;
    int status;

    status = XReadBitmapFile(dpy, root, filename, width,
			     height, &bitmap, x_hot, y_hot);
    if (status == BitmapSuccess)
      return(bitmap);
    else if (status == BitmapOpenFailed)
	fprintf(stderr, "%s: can't open file: %s\n", program_name, filename);
    else if (status == BitmapFileInvalid)
	fprintf(stderr, "%s: bad bitmap format file: %s\n",
			program_name, filename);
    else
	fprintf(stderr, "%s: insufficient memory for bitmap: %s",
			program_name, filename);
    exit(1);
    /*NOTREACHED*/
}
