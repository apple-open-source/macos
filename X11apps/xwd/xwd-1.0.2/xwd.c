/* $Xorg: xwd.c,v 1.5 2001/02/09 02:06:03 xorgcvs Exp $ */

/*

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
/* $XFree86: xc/programs/xwd/xwd.c,v 3.11 2002/09/16 18:06:21 eich Exp $ */

/*
 * xwd.c MIT Project Athena, X Window system window raster image dumper.
 *
 * This program will dump a raster image of the contents of a window into a 
 * file for output on graphics printers or for other uses.
 *
 *  Author:	Tony Della Fera, DEC
 *		17-Jun-85
 * 
 *  Modification history:
 *
 *  11/14/86 Bill Wyatt, Smithsonian Astrophysical Observatory
 *    - Removed Z format option, changing it to an XY option. Monochrome 
 *      windows will always dump in XY format. Color windows will dump
 *      in Z format by default, but can be dumped in XY format with the
 *      -xy option.
 *
 *  11/18/86 Bill Wyatt
 *    - VERSION 6 is same as version 5 for monchrome. For colors, the 
 *      appropriate number of Color structs are dumped after the header,
 *      which has the number of colors (=0 for monochrome) in place of the
 *      V5 padding at the end. Up to 16-bit displays are supported. I
 *      don't yet know how 24- to 32-bit displays will be handled under
 *      the Version 11 protocol.
 *
 *  6/15/87 David Krikorian, MIT Project Athena
 *    - VERSION 7 runs under the X Version 11 servers, while the previous
 *      versions of xwd were are for X Version 10.  This version is based
 *      on xwd version 6, and should eventually have the same color
 *      abilities. (Xwd V7 has yet to be tested on a color machine, so
 *      all color-related code is commented out until color support
 *      becomes practical.)
 */

/*%
 *%    This is the format for commenting out color-related code until
 *%  color can be supported.
%*/

#include <stdio.h>
#include <errno.h>
#include <X11/Xos.h>
#include <stdlib.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

typedef unsigned long Pixel;
#include "X11/XWDFile.h"

#define FEEP_VOLUME 0

/* Include routines to do parsing */
#include "dsimple.h"
#include "list.h"
#include "wsutils.h"
#include "multiVis.h"

#ifdef XKB
#include <X11/extensions/XKBbells.h>
#endif

/* Setable Options */

int format = ZPixmap;
Bool nobdrs = False;
Bool on_root = False;
Bool standard_out = True;
Bool debug = False;
Bool silent = False;
Bool use_installed = False;
long add_pixel_value = 0;


extern int main(int, char **);
extern void Window_Dump(Window, FILE *);
extern int Image_Size(XImage *);
extern int Get_XColors(XWindowAttributes *, XColor **);
extern void _swapshort(register char *, register unsigned);
extern void _swaplong(register char *, register unsigned);
static long parse_long(char *);
static int Get24bitDirectColors(XColor **);
static int ReadColors(Visual *, Colormap, XColor **);


static long parse_long (s)
    char *s;
{
    char *fmt = "%lu";
    long retval = 0L;
    int thesign = 1;

    if (s && s[0]) {
	if (s[0] == '-') s++, thesign = -1;
	if (s[0] == '0') s++, fmt = "%lo";
	if (s[0] == 'x' || s[0] == 'X') s++, fmt = "%lx";
	(void) sscanf (s, fmt, &retval);
    }
    return (thesign * retval);
}

int
main(argc, argv)
    int argc;
    char **argv;
{
    register int i;
    Window target_win;
    FILE *out_file = stdout;
    Bool frame_only = False;

    INIT_NAME;

    Setup_Display_And_Screen(&argc, argv);

    /* Get window select on command line, if any */
    target_win = Select_Window_Args(&argc, argv);

    for (i = 1; i < argc; i++) {
	if (!strcmp(argv[i], "-nobdrs")) {
	    nobdrs = True;
	    continue;
	}
	if (!strcmp(argv[i], "-debug")) {
	    debug = True;
	    continue;
	}
	if (!strcmp(argv[i], "-help"))
	  usage();
	if (!strcmp(argv[i], "-out")) {
	    if (++i >= argc) usage();
	    if (!(out_file = fopen(argv[i], "wb")))
	      Fatal_Error("Can't open output file as specified.");
	    standard_out = False;
	    continue;
	}
	if (!strcmp(argv[i], "-xy")) {
	    format = XYPixmap;
	    continue;
	}
	if (!strcmp(argv[i], "-screen")) {
	    on_root = True;
	    continue;
	}
	if (!strcmp(argv[i], "-icmap")) {
	    use_installed = True;
	    continue;
	}
	if (!strcmp(argv[i], "-add")) {
	    if (++i >= argc) usage();
	    add_pixel_value = parse_long (argv[i]);
	    continue;
	}
	if (!strcmp(argv[i], "-frame")) {
	    frame_only = True;
	    continue;
	}
	if (!strcmp(argv[i], "-silent")) {
	    silent = True;
	    continue;
	}
	usage();
    }
#ifdef WIN32
    if (standard_out)
	_setmode(fileno(out_file), _O_BINARY);
#endif
    
    /*
     * Let the user select the target window.
     */
    if (target_win == None)
	target_win = Select_Window(dpy, !frame_only);

    /*
     * Dump it!
     */
    Window_Dump(target_win, out_file);

    XCloseDisplay(dpy);
    if (fclose(out_file)) {
	perror("xwd");
	exit(1);
    }
    exit(0);
}

static int
Get24bitDirectColors(colors)
XColor **colors ;
{
    int i , ncolors = 256 ;
    XColor *tcol ;

    *colors = tcol = (XColor *)malloc(sizeof(XColor) * ncolors) ;

    for(i=0 ; i < ncolors ; i++)
    {
	tcol[i].pixel = i << 16 | i << 8 | i ;
	tcol[i].red = tcol[i].green = tcol[i].blue = i << 8   | i ;
    }

    return ncolors ;
}


/*
 * Window_Dump: dump a window to a file which must already be open for
 *              writting.
 */

void
Window_Dump(window, out)
     Window window;
     FILE *out;
{
    unsigned long swaptest = 1;
    XColor *colors;
    unsigned buffer_size;
    int win_name_size;
    int header_size;
    int ncolors, i;
    char *win_name;
    Bool got_win_name;
    XWindowAttributes win_info;
    XImage *image;
    int absx, absy, x, y;
    unsigned width, height;
    int dwidth, dheight;
    int bw;
    Window dummywin;
    XWDFileHeader header;
    XWDColor xwdcolor;
    
    int                 transparentOverlays , multiVis;
    int                 numVisuals;
    XVisualInfo         *pVisuals;
    int                 numOverlayVisuals;
    OverlayInfo         *pOverlayVisuals;
    int                 numImageVisuals;
    XVisualInfo         **pImageVisuals;
    list_ptr            vis_regions;    /* list of regions to read from */
    list_ptr            vis_image_regions ;
    Visual		vis_h,*vis ;
    int			allImage = 0 ;

    /*
     * Inform the user not to alter the screen.
     */
    if (!silent) {
#ifdef XKB
	XkbStdBell(dpy,None,50,XkbBI_Wait);
#else
	XBell(dpy,FEEP_VOLUME);
#endif
	XFlush(dpy);
    }

    /*
     * Get the parameters of the window being dumped.
     */
    if (debug) outl("xwd: Getting target window information.\n");
    if(!XGetWindowAttributes(dpy, window, &win_info)) 
      Fatal_Error("Can't get target window attributes.");

    /* handle any frame window */
    if (!XTranslateCoordinates (dpy, window, RootWindow (dpy, screen), 0, 0,
				&absx, &absy, &dummywin)) {
	fprintf (stderr, 
		 "%s:  unable to translate window coordinates (%d,%d)\n",
		 program_name, absx, absy);
	exit (1);
    }
    win_info.x = absx;
    win_info.y = absy;
    width = win_info.width;
    height = win_info.height;
    bw = 0;

    if (!nobdrs) {
	absx -= win_info.border_width;
	absy -= win_info.border_width;
	bw = win_info.border_width;
	width += (2 * bw);
	height += (2 * bw);
    }
    dwidth = DisplayWidth (dpy, screen);
    dheight = DisplayHeight (dpy, screen);


    /* clip to window */
    if (absx < 0) width += absx, absx = 0;
    if (absy < 0) height += absy, absy = 0;
    if (absx + width > dwidth) width = dwidth - absx;
    if (absy + height > dheight) height = dheight - absy;

    XFetchName(dpy, window, &win_name);
    if (!win_name || !win_name[0]) {
	win_name = "xwdump";
	got_win_name = False;
    } else {
	got_win_name = True;
    }

    /* sizeof(char) is included for the null string terminator. */
    win_name_size = strlen(win_name) + sizeof(char);

    /*
     * Snarf the pixmap with XGetImage.
     */

    x = absx - win_info.x;
    y = absy - win_info.y;

    multiVis = GetMultiVisualRegions(dpy,RootWindow(dpy, screen), 
               absx, absy, 
	       width, height,&transparentOverlays,&numVisuals, &pVisuals,
               &numOverlayVisuals,&pOverlayVisuals,&numImageVisuals,
               &pImageVisuals,&vis_regions,&vis_image_regions,&allImage) ;
    if (on_root || multiVis)
    {
	if(!multiVis)
	    image = XGetImage (dpy, RootWindow(dpy, screen), absx, absy, 
                    width, height, AllPlanes, format);
	else
	    image = ReadAreaToImage(dpy, RootWindow(dpy, screen), absx, absy, 
                width, height,
    		numVisuals,pVisuals,numOverlayVisuals,pOverlayVisuals,
                numImageVisuals, pImageVisuals,vis_regions,
                vis_image_regions,format,allImage);
    }
    else
	image = XGetImage (dpy, window, x, y, width, height, AllPlanes, format);
    if (!image) {
	fprintf (stderr, "%s:  unable to get image at %dx%d+%d+%d\n",
		 program_name, width, height, x, y);
	exit (1);
    }

    if (add_pixel_value != 0) XAddPixel (image, add_pixel_value);

    /*
     * Determine the pixmap size.
     */
    buffer_size = Image_Size(image);

    if (debug) outl("xwd: Getting Colors.\n");

    if( !multiVis)
    {
       ncolors = Get_XColors(&win_info, &colors);
       vis = win_info.visual ;
    }
    else
    {
       ncolors = Get24bitDirectColors(&colors) ;
       initFakeVisual(&vis_h) ;
       vis = &vis_h ;
    }
    /*
     * Inform the user that the image has been retrieved.
     */
    if (!silent) {
#ifdef XKB
	XkbStdBell(dpy,window,FEEP_VOLUME,XkbBI_Proceed);
	XkbStdBell(dpy,window,FEEP_VOLUME,XkbBI_RepeatingLastBell);
#else
	XBell(dpy, FEEP_VOLUME);
	XBell(dpy, FEEP_VOLUME);
#endif
	XFlush(dpy);
    }

    /*
     * Calculate header size.
     */
    if (debug) outl("xwd: Calculating header size.\n");
    header_size = SIZEOF(XWDheader) + win_name_size;

    /*
     * Write out header information.
     */
    if (debug) outl("xwd: Constructing and dumping file header.\n");
    header.header_size = (CARD32) header_size;
    header.file_version = (CARD32) XWD_FILE_VERSION;
    header.pixmap_format = (CARD32) format;
    header.pixmap_depth = (CARD32) image->depth;
    header.pixmap_width = (CARD32) image->width;
    header.pixmap_height = (CARD32) image->height;
    header.xoffset = (CARD32) image->xoffset;
    header.byte_order = (CARD32) image->byte_order;
    header.bitmap_unit = (CARD32) image->bitmap_unit;
    header.bitmap_bit_order = (CARD32) image->bitmap_bit_order;
    header.bitmap_pad = (CARD32) image->bitmap_pad;
    header.bits_per_pixel = (CARD32) image->bits_per_pixel;
    header.bytes_per_line = (CARD32) image->bytes_per_line;
    /****
    header.visual_class = (CARD32) win_info.visual->class;
    header.red_mask = (CARD32) win_info.visual->red_mask;
    header.green_mask = (CARD32) win_info.visual->green_mask;
    header.blue_mask = (CARD32) win_info.visual->blue_mask;
    header.bits_per_rgb = (CARD32) win_info.visual->bits_per_rgb;
    header.colormap_entries = (CARD32) win_info.visual->map_entries;
    *****/
    header.visual_class = (CARD32) vis->class;
    header.red_mask = (CARD32) vis->red_mask;
    header.green_mask = (CARD32) vis->green_mask;
    header.blue_mask = (CARD32) vis->blue_mask;
    header.bits_per_rgb = (CARD32) vis->bits_per_rgb;
    header.colormap_entries = (CARD32) vis->map_entries;

    header.ncolors = ncolors;
    header.window_width = (CARD32) win_info.width;
    header.window_height = (CARD32) win_info.height;
    header.window_x = absx;
    header.window_y = absy;
    header.window_bdrwidth = (CARD32) win_info.border_width;

    if (*(char *) &swaptest) {
	_swaplong((char *) &header, sizeof(header));
	for (i = 0; i < ncolors; i++) {
	    _swaplong((char *) &colors[i].pixel, sizeof(CARD32));
	    _swapshort((char *) &colors[i].red, 3 * sizeof(short));
	}
    }

    if (fwrite((char *)&header, SIZEOF(XWDheader), 1, out) != 1 ||
	fwrite(win_name, win_name_size, 1, out) != 1) {
	perror("xwd");
	exit(1);
    }

    /*
     * Write out the color maps, if any
     */

    if (debug) outl("xwd: Dumping %d colors.\n", ncolors);
    for (i = 0; i < ncolors; i++) {
	xwdcolor.pixel = colors[i].pixel;
	xwdcolor.red = colors[i].red;
	xwdcolor.green = colors[i].green;
	xwdcolor.blue = colors[i].blue;
	xwdcolor.flags = colors[i].flags;
	if (fwrite((char *) &xwdcolor, SIZEOF(XWDColor), 1, out) != 1) {
	    perror("xwd");
	    exit(1);
	}
    }

    /*
     * Write out the buffer.
     */
    if (debug) outl("xwd: Dumping pixmap.  bufsize=%d\n",buffer_size);

    /*
     *    This copying of the bit stream (data) to a file is to be replaced
     *  by an Xlib call which hasn't been written yet.  It is not clear
     *  what other functions of xwd will be taken over by this (as yet)
     *  non-existant X function.
     */
    if (fwrite(image->data, (int) buffer_size, 1, out) != 1) {
	perror("xwd");
	exit(1);
    }

    /*
     * free the color buffer.
     */

    if(debug && ncolors > 0) outl("xwd: Freeing colors.\n");
    if(ncolors > 0) free(colors);

    /*
     * Free window name string.
     */
    if (debug) outl("xwd: Freeing window name string.\n");
    if (got_win_name) XFree(win_name);

    /*
     * Free image
     */
    XDestroyImage(image);
}

/*
 * Report the syntax for calling xwd.
 */
void
usage()
{
    fprintf (stderr,
"usage: %s [-display host:dpy] [-debug] [-help] %s [-nobdrs] [-out <file>]",
	   program_name, "[{-root|-id <id>|-name <name>}]");
    fprintf (stderr, " [-xy] [-add value] [-frame]\n");
    exit(1);
}


/*
 * Determine the pixmap size.
 */

int Image_Size(image)
     XImage *image;
{
    if (image->format != ZPixmap)
      return(image->bytes_per_line * image->height * image->depth);

    return(image->bytes_per_line * image->height);
}

#define lowbit(x) ((x) & (~(x) + 1))

static int
ReadColors(vis,cmap,colors)
Visual *vis ;
Colormap cmap ;
XColor **colors ;
{
    int i,ncolors ;

    ncolors = vis->map_entries;

    if (!(*colors = (XColor *) malloc (sizeof(XColor) * ncolors)))
      Fatal_Error("Out of memory!");

    if (vis->class == DirectColor ||
	vis->class == TrueColor) {
	Pixel red, green, blue, red1, green1, blue1;

	red = green = blue = 0;
	red1 = lowbit(vis->red_mask);
	green1 = lowbit(vis->green_mask);
	blue1 = lowbit(vis->blue_mask);
	for (i=0; i<ncolors; i++) {
	  (*colors)[i].pixel = red|green|blue;
	  (*colors)[i].pad = 0;
	  red += red1;
	  if (red > vis->red_mask)
	    red = 0;
	  green += green1;
	  if (green > vis->green_mask)
	    green = 0;
	  blue += blue1;
	  if (blue > vis->blue_mask)
	    blue = 0;
	}
    } else {
	for (i=0; i<ncolors; i++) {
	  (*colors)[i].pixel = i;
	  (*colors)[i].pad = 0;
	}
    }

    XQueryColors(dpy, cmap, *colors, ncolors);
    
    return(ncolors);
}

/*
 * Get the XColors of all pixels in image - returns # of colors
 */
int Get_XColors(win_info, colors)
     XWindowAttributes *win_info;
     XColor **colors;
{
    int i, ncolors;
    Colormap cmap = win_info->colormap;

    if (use_installed)
	/* assume the visual will be OK ... */
	cmap = XListInstalledColormaps(dpy, win_info->root, &i)[0];
    if (!cmap)
	return(0);
    ncolors = ReadColors(win_info->visual,cmap,colors) ;
    return ncolors ;
}

void
_swapshort (bp, n)
    register char *bp;
    register unsigned n;
{
    register char c;
    register char *ep = bp + n;

    while (bp < ep) {
	c = *bp;
	*bp = *(bp + 1);
	bp++;
	*bp++ = c;
    }
}

void
_swaplong (bp, n)
    register char *bp;
    register unsigned n;
{
    register char c;
    register char *ep = bp + n;

    while (bp < ep) {
        c = bp[3];
        bp[3] = bp[0];
        bp[0] = c;
        c = bp[2];
        bp[2] = bp[1];
        bp[1] = c;
        bp += 4;
    }
}
