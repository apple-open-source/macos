/* $Xorg: xwud.c,v 1.5 2001/02/09 02:06:04 xorgcvs Exp $ */
/*

Copyright 1985, 1986, 1988, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/
/* $XFree86: xc/programs/xwud/xwud.c,v 3.10tsi Exp $ */

/* xwud - marginally useful raster image undumper */


#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <X11/XWDFile.h>
#define  XK_LATIN1
#include <X11/keysymdef.h>
#include <errno.h>
#include <stdlib.h>

Atom wm_protocols;
Atom wm_delete_window;
int split;

char *progname;

static void usage(void);
static Bool Read(char *ptr, int size, int nitems, FILE *stream);
static void putImage(Display *dpy, Window image_win, GC gc, 
		     XImage *out_image, int x, int y, int w, int h);
static void putScaledImage(Display *display, Drawable d, GC gc, 
			   XImage *src_image, int exp_x, int exp_y, 
			   unsigned int exp_width, unsigned int exp_height, 
			   unsigned int dest_width, unsigned dest_height);
static void Latin1Upper(char *s);
static void Extract_Plane(XImage *in_image, XImage *out_image, int plane);
static int EffectiveSize(XVisualInfo *vinfo);
static int VisualRank(int class);
static int IsGray(Display *dpy, XStandardColormap *stdmap);
static void Do_StdGray(Display *dpy, XStandardColormap *stdmap, int ncolors, 
		       XColor *colors, XImage *in_image, XImage *out_image);
static void Do_StdCol(Display *dpy, XStandardColormap *stdmap, int ncolors, 
		      XColor *colors, XImage *in_image, XImage *out_image);
static Colormap CopyColormapAndFree(Display *dpy, Colormap colormap);
static void Do_Pseudo(Display *dpy, Colormap *colormap, int ncolors, 
		      XColor *colors, XImage *in_image, XImage *out_image);
static void Do_Direct(Display *dpy, XWDFileHeader *header, Colormap *colormap, 
		      int ncolors, XColor *colors, 
		      XImage *in_image, XImage *out_image, XVisualInfo *vinfo);
static unsigned int Image_Size(XImage *image);
static void Error(char *string);
static void _swapshort(char *bp, unsigned int n);
static void _swaplong(char *bp, unsigned int n);
static void DumpHeader(const XWDFileHeader *header, const char *win_name);

static void
usage(void)
{
    fprintf(stderr, "usage: %s [-in <file>] [-noclick] [-geometry <geom>] [-display <display>]\n", progname);
    fprintf(stderr, "            [-new] [-std <maptype>] [-raw] [-vis <vis-type-or-id>]\n");
    fprintf(stderr, "            [-help] [-rv] [-plane <number>] [-fg <color>] [-bg <color>]\n");
    fprintf(stderr, "            [-scale]\n");
    exit(1);
}

static Bool
Read(char *ptr, int size, int nitems, FILE *stream)
{
    size *= nitems;
    while (size) {
	nitems = fread(ptr, 1, size, stream);
	if (nitems <= 0)
	    return False;
	size -= nitems;
	ptr += nitems;
    }
    return True;
}

int 
main(int argc, char *argv[])
{
    Display *dpy;
    int screen;
    register int i;
    XImage in_image_struct;
    XImage *in_image, *out_image;
    XSetWindowAttributes attributes;
    XVisualInfo vinfo, *vinfos;
    long mask;
    register char *buffer;
    unsigned long swaptest = 1;
    int count, stdcnt;
    unsigned buffer_size;
    int win_name_size;
    int ncolors;
    char *file_name = NULL;
    char *win_name;
    Bool inverse = False, rawbits = False, newmap = False;
    Bool onclick = True;
    Bool scale = False;
    int plane = -1;
    char *std = NULL;
    char *vis = NULL;
    char *display_name = NULL;
    char *fgname = NULL;
    char *bgname = NULL;
    char *geom = NULL;
    int gbits = 0;
    XSizeHints hints;
    XTextProperty textprop;
    XClassHint class_hint;
    XColor *colors = NULL, color, igncolor;
    Window image_win;
    Colormap colormap;
    XEvent event;
    register XExposeEvent *expose = (XExposeEvent *)&event;
    GC gc;
    XGCValues gc_val;
    XWDFileHeader header;
    XWDColor xwdcolor;
    FILE *in_file = stdin;
    char *map_name;
    Atom map_prop;
    XStandardColormap *stdmaps, *stdmap = NULL;
    char c;
    int win_width, win_height;
    Bool dump_header = False;

    progname = argv[0];

    for (i = 1; i < argc; i++) {
	if (strcmp(argv[i], "-bg") == 0) {
	    if (++i >= argc) usage();
	    bgname = argv[i];
	    continue;
	}
	if (strcmp(argv[i], "-display") == 0) {
	    if (++i >= argc) usage();
	    display_name = argv[i];
	    continue;
	}
	if (strcmp(argv[i], "-dumpheader") == 0) {
	    dump_header = True;
	    continue;
	}
	if (strcmp(argv[i], "-fg") == 0) {
	    if (++i >= argc) usage();
	    fgname = argv[i];
	    continue;
	}
	if (strcmp(argv[i], "-geometry") == 0) {
	    if (++i >= argc) usage();
	    geom = argv[i];
	    continue;
	}
	if (strcmp(argv[i], "-help") == 0) {
	    usage();
	}
	if (strcmp(argv[i], "-in") == 0) {
	    if (++i >= argc) usage();
	    file_name = argv[i];
	    continue;
	}
	if (strcmp(argv[i], "-inverse") == 0) { /* for compatibility */
	    inverse = True;
	    continue;
	}
	if (strcmp(argv[i], "-new") == 0) {
	    newmap = True;
	    if (std) usage();
	    continue;
	}
	if (strcmp(argv[i], "-noclick") == 0) {
	    onclick = False;
	    continue;
	}
	if (strcmp(argv[i], "-plane") == 0) {
	    if (++i >= argc) usage();
	    plane = atoi(argv[i]);
	    continue;
	}
	if (strcmp(argv[i], "-raw") == 0) {
	    rawbits = True;
	    if (std) usage();
	    continue;
	}
	if (strcmp(argv[i], "-rv") == 0) {
	    inverse = True;
	    continue;
	}
	if (strcmp(argv[i], "-scale") == 0) {
	    scale = True;
	    continue;
	}
	if (strcmp(argv[i], "-split") == 0) {
	    split = True;
	    continue;
	}
	if (strcmp(argv[i], "-std") == 0) {
	    if (++i >= argc) usage();
	    std = argv[i];
	    if (newmap || rawbits) usage();
	    continue;
	}
	if (strcmp(argv[i], "-vis") == 0) {
	    if (++i >= argc) usage();
	    vis = argv[i];
	    continue;
	}
	usage();
    }
    
    if (file_name) {
	in_file = fopen(file_name, "rb");
	if (in_file == NULL)
	    Error("Can't open input file as specified.");
    }
#ifdef WIN32
    else
	_setmode(fileno(in_file), _O_BINARY);
#endif
    
    dpy = XOpenDisplay(display_name);
    if (dpy == NULL) {
	fprintf(stderr, "%s:  unable to open display \"%s\"\n",
		progname, XDisplayName(display_name));
	exit(1);
    }
    screen = DefaultScreen(dpy);

    /*
     * Read in header information.
     */
    if(!Read((char *)&header, SIZEOF(XWDheader), 1, in_file))
      Error("Unable to read dump file header.");

    if (*(char *) &swaptest)
	_swaplong((char *) &header, SIZEOF(XWDheader));

    /* check to see if the dump file is in the proper format */
    if (header.file_version != XWD_FILE_VERSION) {
	fprintf(stderr,"xwud: XWD file format version mismatch.");
	Error("exiting.");
    }
    if (header.header_size < SIZEOF(XWDheader)) {
	fprintf(stderr,"xwud: XWD header size is too small.");
	Error("exiting.");
    }

    /* alloc window name */
    win_name_size = (header.header_size - SIZEOF(XWDheader));
    if((win_name = malloc((unsigned) win_name_size + 6)) == NULL)
      Error("Can't malloc window name storage.");
    strcpy(win_name, "xwud: ");

     /* read in window name */
    if(!Read(win_name + 6, sizeof(char), win_name_size, in_file))
      Error("Unable to read window name from dump file.");

    if (dump_header) {
	DumpHeader(&header, win_name);
	exit(0);
    }
   
    /* initialize the input image */

    in_image = &in_image_struct;
    in_image->depth = header.pixmap_depth;
    in_image->format = header.pixmap_format;
    in_image->xoffset = header.xoffset;
    in_image->data = NULL;
    in_image->width = header.pixmap_width;
    in_image->height = header.pixmap_height;
    in_image->bitmap_pad = header.bitmap_pad;
    in_image->bytes_per_line = header.bytes_per_line;
    in_image->byte_order = header.byte_order;
    in_image->bitmap_unit = header.bitmap_unit;
    in_image->bitmap_bit_order = header.bitmap_bit_order;
    in_image->bits_per_pixel = header.bits_per_pixel;
    in_image->red_mask = header.red_mask;
    in_image->green_mask = header.green_mask;
    in_image->blue_mask = header.blue_mask;
    if (!XInitImage(in_image))
	Error("Invalid input image header data.");

    /* read in the color map buffer */
    if((ncolors = header.ncolors)) {
	colors = (XColor *)malloc((unsigned) ncolors * sizeof(XColor));
	if (!colors)
	    Error("Can't malloc color table");
	for (i = 0; i < ncolors; i++) {
	    if(!Read((char *) &xwdcolor, SIZEOF(XWDColor), 1, in_file))
		Error("Unable to read color map from dump file.");
	    colors[i].pixel = xwdcolor.pixel;
	    colors[i].red = xwdcolor.red;
	    colors[i].green = xwdcolor.green;
	    colors[i].blue = xwdcolor.blue;
	    colors[i].flags = xwdcolor.flags;
	}
	if (*(char *) &swaptest) {
	    for (i = 0; i < ncolors; i++) {
		_swaplong((char *) &colors[i].pixel, sizeof(long));
		_swapshort((char *) &colors[i].red, 3 * sizeof(short));
	    }
	}
    }
    else
	/* no color map exists, turn on the raw option */
	rawbits = True;

    /* alloc the pixel buffer */
    buffer_size = Image_Size(in_image);
    if((buffer = malloc(buffer_size)) == NULL)
      Error("Can't malloc data buffer.");

    /* read in the image data */
    if (!Read(buffer, sizeof(char), (int)buffer_size, in_file))
        Error("Unable to read pixmap from dump file.");

     /* close the input file */
    (void) fclose(in_file);

    if (plane >= in_image->depth)
	Error("plane number exceeds image depth");
    if ((in_image->format == XYPixmap) && (plane >= 0)) {
	buffer += in_image->bytes_per_line * in_image->height *
		  (in_image->depth - (plane + 1));
	in_image->depth = 1;
	ncolors = 0;
    }
    if (in_image->bits_per_pixel == 1 && in_image->depth == 1) {
	in_image->format = XYBitmap;
	newmap = False;
	rawbits = True;
    }
    in_image->data = buffer;

    if (std) {
	map_name = malloc(strlen(std) + 9);
	strcpy(map_name, "RGB_");
	strcat(map_name, std);
	strcat(map_name, "_MAP");
	Latin1Upper(map_name);
	map_prop = XInternAtom(dpy, map_name, True);
	if (!map_prop || !XGetRGBColormaps(dpy, RootWindow(dpy, screen),
					   &stdmaps, &stdcnt, map_prop))
	    Error("specified standard colormap does not exist");
    }
    vinfo.screen = screen;
    mask = VisualScreenMask;
    if (vis)
    {
	char *vt;
	vt = malloc(strlen(vis) + 1);
	strcpy(vt, vis);
	Latin1Upper(vt);
	if (strcmp(vt, "STATICGRAY") == 0) {
	    vinfo.class = StaticGray;
	    mask |= VisualClassMask;
	} else if (strcmp(vt, "GRAYSCALE") == 0) {
	    vinfo.class = GrayScale;
	    mask |= VisualClassMask;
	} else if (strcmp(vt, "STATICCOLOR") == 0) {
	    vinfo.class = StaticColor;
	    mask |= VisualClassMask;
	} else if (strcmp(vt, "PSEUDOCOLOR") == 0) {
	    vinfo.class = PseudoColor;
	    mask |= VisualClassMask;
	} else if (strcmp(vt, "DIRECTCOLOR") == 0) {
	    vinfo.class = DirectColor;
	    mask |= VisualClassMask;
	} else if (strcmp(vt, "TRUECOLOR") == 0) {
	    vinfo.class = TrueColor;
	    mask |= VisualClassMask;
	} else if (strcmp(vt, "MATCH") == 0) {
	    vinfo.class = header.visual_class;
	    mask |= VisualClassMask;
	} else if (strcmp(vt, "DEFAULT") == 0) {
	    vinfo.visualid= XVisualIDFromVisual(DefaultVisual(dpy, screen));
	    mask |= VisualIDMask;
	} else {
	    vinfo.visualid = 0;
	    mask |= VisualIDMask;
	    sscanf(vis, "0x%lx", &vinfo.visualid);
	    if (!vinfo.visualid)
	      sscanf(vis, "%lu", &vinfo.visualid);
	    if (!vinfo.visualid)
	      Error("invalid visual specifier");
	}
    }
    if (rawbits && (in_image->depth > 1) && (plane < 0)) {
	vinfo.depth = in_image->depth;
	mask |= VisualDepthMask;
    }
    vinfos = XGetVisualInfo(dpy, mask, &vinfo, &count);
    if (count == 0)
	Error("no matching visual found");

    /* find a workable visual */
    if (std) {
	stdmap = &stdmaps[0];
	if (mask & VisualIDMask) {
	    for (i = 0; i < stdcnt; i++) {
		if (stdmaps[i].visualid == vinfo.visualid) {
		    stdmap = &stdmaps[i];
		    break;
		}
	    }
	    if (stdmap->visualid != vinfo.visualid)
		Error("no standard colormap matching specified visual");
	}
	for (i = 0; i < count; i++) {
	    if (stdmap->visualid == vinfos[i].visualid) {
		vinfo = vinfos[i];
		break;
	    }
	}
    } else if ((in_image->depth == 1) ||
	       ((in_image->format == ZPixmap) && (plane >= 0)) ||
	       rawbits) {
	vinfo = vinfos[0];
	if (!(mask & VisualIDMask)) {
	    for (i = 0; i < count; i++) {
		if ((vinfos[i].visualid ==
		     XVisualIDFromVisual(DefaultVisual(dpy, screen))) &&
		    (vinfos[i].depth == DefaultDepth(dpy, screen))) {
		    vinfo = vinfos[i];
		    break;
		}
	    }
	}
    } else {
	/* get best visual */
	vinfo = vinfos[0];
	for (i = 0; i < count; i++) {
	    int z1, z2;
	    z2 = EffectiveSize(&vinfos[i]);
	    if ((z2 >= ncolors) &&
		(vinfos[i].depth == in_image->depth) &&
		(vinfos[i].class == header.visual_class))
	    {
		vinfo = vinfos[i];
		break;
	    }
	    z1 = EffectiveSize(&vinfo);
	    if ((z2 > z1) ||
		((z2 == z1) &&
		 (VisualRank(vinfos[i].class) >= VisualRank(vinfo.class))))
		vinfo = vinfos[i];
	}
	if ((newmap || (vinfo.visual != DefaultVisual(dpy, screen))) &&
	    (vinfo.class != StaticGray) &&
	    (vinfo.class != StaticColor) &&
	    (vinfo.class == header.visual_class) &&
	    (vinfo.depth == in_image->depth) &&
	    ((vinfo.class == PseudoColor) ||
	     (vinfo.class == GrayScale) ||
	     ((vinfo.red_mask == header.red_mask) &&
	      (vinfo.green_mask == header.green_mask) &&
	      (vinfo.blue_mask == header.blue_mask)))) {
	    rawbits = True;
	    newmap = True;
	}
    }

    /* get the appropriate colormap */
    if (newmap && (vinfo.class & 1) &&
	(vinfo.depth == in_image->depth) &&
	(vinfo.class == header.visual_class) &&
	(vinfo.colormap_size >= ncolors) &&
	(vinfo.red_mask == header.red_mask) &&
	(vinfo.green_mask == header.green_mask) &&
	(vinfo.blue_mask == header.blue_mask)) {
	colormap = XCreateColormap(dpy, RootWindow(dpy, screen), vinfo.visual,
				   AllocAll);
	if (ncolors) {
	    for (i = 0; i < ncolors; i++)
		colors[i].flags = DoRed|DoGreen|DoBlue;
	    XStoreColors(dpy, colormap, colors, ncolors);
	}
    } else if (std) {
	colormap = stdmap->colormap;
    } else {
	if (!newmap && (vinfo.visual == DefaultVisual(dpy, screen)))
	    colormap = DefaultColormap(dpy, screen);
	else
	    colormap = XCreateColormap(dpy, RootWindow(dpy, screen),
				       vinfo.visual, AllocNone);
	newmap = False;
    }

    /* create the output image */
    if ((in_image->format == ZPixmap) && (plane >= 0)) {
	out_image = XCreateImage(dpy, vinfo.visual, 1,
				 XYBitmap, 0, NULL,
				 in_image->width, in_image->height,
				 XBitmapPad(dpy), 0);
	out_image->data = malloc(Image_Size(out_image));
	Extract_Plane(in_image, out_image, plane);
	ncolors = 0;
    } else if (rawbits || newmap) {
	out_image = in_image;
    } else {
	out_image = XCreateImage(dpy, vinfo.visual, vinfo.depth,
				 (vinfo.depth == 1) ? XYBitmap :
						      in_image->format,
				 in_image->xoffset, NULL,
				 in_image->width, in_image->height,
				 XBitmapPad(dpy), 0);
	out_image->data = malloc(Image_Size(out_image));
	if (std) {
	    if (!stdmap->green_max && !stdmap->blue_max && IsGray(dpy, stdmap))
		Do_StdGray(dpy, stdmap, ncolors, colors, in_image, out_image);
	    else
		Do_StdCol(dpy, stdmap, ncolors, colors, in_image, out_image);
	} else if ((header.visual_class == TrueColor) ||
		   (header.visual_class == DirectColor))
	    Do_Direct(dpy, &header, &colormap, ncolors, colors,
		      in_image, out_image, &vinfo);
	else
	    Do_Pseudo(dpy, &colormap, ncolors, colors, in_image, out_image);
    }

    if (out_image->depth == 1) {
	if (fgname &&
	    XAllocNamedColor(dpy, colormap, fgname, &color, &igncolor))
	    gc_val.foreground = color.pixel;
	else if ((ncolors == 2) && XAllocColor(dpy, colormap, &colors[1]))
	    gc_val.foreground = colors[1].pixel;
	else
	    gc_val.foreground = BlackPixel (dpy, screen);
	if (bgname &&
	    XAllocNamedColor(dpy, colormap, bgname, &color, &igncolor))
	    gc_val.background = color.pixel;
	else if ((ncolors == 2) && XAllocColor(dpy, colormap, &colors[0]))
	    gc_val.background = colors[0].pixel;
	else
	    gc_val.background = WhitePixel (dpy, screen);
	if (inverse) {
	    unsigned long tmp;
	    tmp = gc_val.foreground;
	    gc_val.foreground = gc_val.background;
	    gc_val.background = tmp;
	}
    } else {
	gc_val.background = XGetPixel(out_image, 0, 0);
	gc_val.foreground = 0;
    }

    attributes.background_pixel = gc_val.background;
    attributes.border_pixel = gc_val.background;
    if (scale)
	attributes.bit_gravity = ForgetGravity;
    else
	attributes.bit_gravity = NorthWestGravity;
    attributes.event_mask = ButtonPressMask|ButtonReleaseMask|KeyPressMask|
			    ExposureMask;
    if (scale)
	attributes.event_mask |= StructureNotifyMask;
    attributes.colormap = colormap;

    hints.x = header.window_x;
    hints.y = header.window_y;
    hints.width = out_image->width;
    hints.height = out_image->height;
    if (geom)
	gbits = XParseGeometry(geom, &hints.x, &hints.y,
			       (unsigned int *)&hints.width,
			       (unsigned int *)&hints.height);
    hints.flags = ((gbits & (XValue|YValue)) ? USPosition : 0) |
		  ((gbits & (HeightValue|WidthValue)) ? USSize : PSize);
    if (!scale) {
	hints.flags |= PMaxSize;
	hints.max_width = (hints.width > out_image->width) ?
	    hints.width : out_image->width;
	hints.max_height = (hints.height > out_image->height) ?
	    hints.height : out_image->height;
    }
    if ((gbits & XValue) && (gbits & XNegative))
	hints.x += DisplayWidth(dpy, screen) - hints.width; 
    if ((gbits & YValue) && (gbits & YNegative))
	hints.y += DisplayHeight(dpy, screen) - hints.height;

    /* create the image window */
    image_win = XCreateWindow(dpy, RootWindow(dpy, screen),
			      hints.x, hints.y, hints.width, hints.height,
			      0, vinfo.depth, InputOutput, vinfo.visual,
			      CWBorderPixel|CWBackPixel|CWColormap|CWEventMask|CWBitGravity,
			      &attributes);
    win_width = hints.width;
    win_height = hints.height;

    /* Setup for ICCCM delete window. */
    wm_protocols = XInternAtom(dpy, "WM_PROTOCOLS", False);
    wm_delete_window = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    (void) XSetWMProtocols (dpy, image_win, &wm_delete_window, 1);
     
    textprop.value = (unsigned char *) win_name;
    textprop.encoding = XA_STRING;
    textprop.format = 8;
    textprop.nitems = strlen(win_name);
    class_hint.res_name = (char *)NULL;
    class_hint.res_class = "Xwud";
    /* set standard properties */
    XSetWMProperties(dpy, image_win, &textprop, (XTextProperty *)NULL,
		     argv, argc, &hints, (XWMHints *)NULL, &class_hint);

    /* map the image window */
    XMapWindow(dpy, image_win);

    gc = XCreateGC (dpy, image_win, GCForeground|GCBackground, &gc_val);

    while (1) {
	/* wait on mouse input event to terminate */
	XNextEvent(dpy, &event);
	switch(event.type) {
	case ClientMessage:
	  if (event.xclient.message_type == wm_protocols && 
	      event.xclient.data.l[0] == wm_delete_window)  {
	    XCloseDisplay(dpy);
	    exit(0);		/* ICCCM delete window */
	  }
	    break;
	case ButtonPress:
	    break;
	case ButtonRelease:
	    if (onclick) {
		XCloseDisplay(dpy);
		exit(0);
	    }
	    break;
	case KeyPress:
	    i = XLookupString(&event.xkey, &c, 1, NULL, NULL);
	    if ((i == 1) && ((c == 'q') || (c == 'Q') || (c == '\03'))) {
		XCloseDisplay(dpy);
		exit(0);
	    }
	    break;
	case ConfigureNotify:
	    win_width = event.xconfigure.width;
	    win_height = event.xconfigure.height;
	    break;
	case Expose:
	    if (scale)
		putScaledImage(dpy, image_win, gc, out_image,
			       expose->x, expose->y,
			       expose->width, expose->height,
			       win_width, win_height);
	    else if ((expose->x < out_image->width) &&
		     (expose->y < out_image->height)) {
		if ((out_image->width - expose->x) < expose->width)
		    expose->width = out_image->width - expose->x;
		if ((out_image->height - expose->y) < expose->height)
		    expose->height = out_image->height - expose->y;
		putImage(dpy, image_win, gc, out_image,
			  expose->x, expose->y,
			  expose->width, expose->height);
	    }
	    break;
	}
    }
    exit(0);
}

static void
putImage(Display *dpy, Window image_win, GC gc, XImage *out_image, 
	 int x, int y, int w, int h)
{
#define SPLIT_SIZE  100
    int	t_x, t_y, t_w, t_h;
    if (split) {
    	for (t_y = y; t_y < y + h; t_y += t_h) {
    	    t_h = SPLIT_SIZE;
    	    if (t_y + t_h > y + h)
	    	t_h = y + h - t_y;
    	    for (t_x = x; t_x < x + w; t_x += t_w) {
	    	t_w = SPLIT_SIZE;
	    	if (t_x + t_w > x + w)
	    	    t_w = x + w - t_x;
	    	XPutImage(dpy, image_win, gc, out_image,
		      	  t_x, t_y, t_x, t_y, t_w, t_h);
    	    }
    	}
    } else {
	XPutImage (dpy, image_win, gc, out_image, x, y, x, y, w, h);
    }
}

typedef short Position;
typedef unsigned short Dimension;
typedef unsigned long Pixel;

#define roundint(x)                   (int)((x) + 0.5)

typedef struct {
  Position *x, *y;
  Dimension *width, *height;
} Table;

static void
putScaledImage(Display *display, Drawable d, GC gc, XImage *src_image, 
	       int exp_x, int exp_y,
	       unsigned int exp_width, unsigned int exp_height, 
	       unsigned int dest_width, unsigned dest_height)
{
    XImage *dest_image;
    Position x, y, min_y, max_y, exp_max_y, src_x, src_max_x, src_y;
    Dimension w, h, strip_height;
    Table table;    
    Pixel pixel;
    double ratio_x, ratio_y;
    Bool fast8;

    if (dest_width == src_image->width && dest_height == src_image->height) {
	/* same for x and y, just send it out */
	XPutImage(display, d, gc, src_image, exp_x, exp_y, 
		  exp_x, exp_y, exp_width, exp_height); 
	return;
    }

    ratio_x = (double)dest_width / (double)src_image->width;
    ratio_y = (double)dest_height / (double)src_image->height;
 
    src_x = exp_x / ratio_x;
    if (src_x >= src_image->width)
	src_x = src_image->width - 1;
    src_y = exp_y / ratio_y;
    if (src_y >= src_image->height)
	src_y = src_image->height - 1;
    exp_max_y = exp_y + exp_height;
    src_max_x = roundint((exp_x + exp_width) / ratio_x) + 1;
    if (src_max_x > src_image->width)
	src_max_x = src_image->width;

    strip_height = 65536 / roundint(ratio_x * src_image->bytes_per_line);
    if (strip_height == 0)
	strip_height = 1;
    if (strip_height > exp_height)
	strip_height = exp_height;

    h = strip_height + roundint(ratio_y);
    dest_image = XCreateImage(display,
			      DefaultVisualOfScreen(
					     DefaultScreenOfDisplay(display)),
			      src_image->depth, src_image->format, 
			      0, NULL,
			      dest_width, h,
			      src_image->bitmap_pad, 0);
    dest_image->data = malloc(dest_image->bytes_per_line * h);
    fast8 = (src_image->depth == 8 && src_image->bits_per_pixel == 8 &&
	     dest_image->bits_per_pixel == 8 && src_image->format == ZPixmap);

    table.x = (Position *) malloc(sizeof(Position) * (src_image->width + 1));
    table.y = (Position *) malloc(sizeof(Position) * (src_image->height + 1));
    table.width = (Dimension *) malloc(sizeof(Dimension) * src_image->width);
    table.height = (Dimension *) malloc(sizeof(Dimension)*src_image->height);
    
    table.x[0] = 0;
    for (x = 1; x <= src_image->width; x++) {
	table.x[x] = roundint(ratio_x * x);
	table.width[x - 1] = table.x[x] - table.x[x - 1];
    }

    table.y[0] = 0;
    for (y = 1; y <= src_image->height; y++) {
	table.y[y] = roundint(ratio_y * y);
	table.height[y - 1] = table.y[y] - table.y[y - 1];
    }

    for (min_y = table.y[src_y]; min_y < exp_max_y; min_y = table.y[y]) {
	max_y = min_y + strip_height;
	if (max_y > exp_max_y) {
	    strip_height = exp_max_y - min_y;
	    max_y = exp_max_y;
	}
	for (y = src_y; table.y[y] < max_y; y++) {
	    if (table.y[y] < min_y)
		continue;
	    if (fast8) {
		for (x = src_x; x < src_max_x; x++) {
		    pixel = ((unsigned char *)src_image->data)
			[y * src_image->bytes_per_line + x];
		    for (h = 0; h < table.height[y]; h++) {
			memset(dest_image->data +
			       (table.y[y] + h - min_y) *
			       dest_image->bytes_per_line + table.x[x],
			       pixel, table.width[x]);
		    }
		}
	    } else {
		for (x = src_x; x < src_max_x; x++) {
		    pixel = XGetPixel(src_image, x, y);
		    for (h = 0; h < table.height[y]; h++) {
			for (w = 0; w < table.width[x]; w++)
			    XPutPixel(dest_image,
				      table.x[x] + w,
				      table.y[y] + h - min_y,
				      pixel);
		    }
		}
	    }
	}
	XPutImage(display, d, gc, dest_image, exp_x, 0,
		  exp_x, min_y, exp_width, table.y[y] - min_y);
	if (y >= src_image->height)
	    break;
    }
    
    XFree((char *)table.x);
    XFree((char *)table.y);
    XFree((char *)table.width);
    XFree((char *)table.height);

    XDestroyImage(dest_image);
}

static void
Latin1Upper(char *s)
{
    unsigned char *str = (unsigned char *)s;
    unsigned char c;

    for (; (c = *str); str++)
    {
	if ((c >= XK_a) && (c <= XK_z))
	    *str = c - (XK_a - XK_A);
	else if ((c >= XK_agrave) && (c <= XK_odiaeresis))
	    *str = c - (XK_agrave - XK_Agrave);
	else if ((c >= XK_oslash) && (c <= XK_thorn))
	    *str = c - (XK_oslash - XK_Ooblique);
    }
}

static void
Extract_Plane(XImage *in_image, XImage *out_image, int plane)
{
    register int x, y;

    for (y = 0; y < in_image->height; y++)
	for (x = 0; x < in_image->width; x++)
	    XPutPixel(out_image, x, y,
		      (XGetPixel(in_image, x, y) >> plane) & 1);
}

static int
EffectiveSize(XVisualInfo *vinfo)
{
    if ((vinfo->class == DirectColor) || (vinfo->class == TrueColor))
	return (vinfo->red_mask | vinfo->green_mask | vinfo->blue_mask) + 1;
    else
	return vinfo->colormap_size;
}

static int
VisualRank(int class)
{
    switch (class) {
    case PseudoColor:
	return 5;
    case TrueColor:
	return 4;
    case DirectColor:
	return 3;
    case StaticColor:
	return 2;
    case GrayScale:
	return 1;
    case StaticGray:
	return 0;
    }
    /* NOTREACHED */
    return -1;
}

static int
IsGray(Display *dpy, XStandardColormap *stdmap)
{
    XColor color;

    color.pixel = stdmap->base_pixel + (stdmap->red_max * stdmap->red_mult);
    XQueryColor(dpy, stdmap->colormap, &color);
    return (color.green || color.blue);
}

static void 
Do_StdGray(Display *dpy, XStandardColormap *stdmap, 
	   int ncolors, XColor *colors, XImage *in_image, XImage *out_image)
{
    register int i, x, y;
    register XColor *color;
    unsigned lim;

    lim = stdmap->red_max + 1;
    for (i = 0, color = colors; i < ncolors; i++, color++)
	color->pixel = stdmap->base_pixel +
		       (((((int)(30L * color->red +
			         59L * color->green +
			         11L * color->blue) / 100)
			  * lim) >> 16) * stdmap->red_mult);
    for (y = 0; y < in_image->height; y++) {
	for (x = 0; x < in_image->width; x++) {
	    XPutPixel(out_image, x, y,
		      colors[XGetPixel(in_image, x, y)].pixel);
	}
    }
}

#define MapVal(val,lim,mult) ((((val * lim) + 32768) / 65535) * mult)

static void
Do_StdCol(Display *dpy, XStandardColormap *stdmap, 
	  int ncolors, XColor *colors, XImage *in_image, XImage *out_image)
{
    register int i, x, y;
    register XColor *color;
    unsigned limr, limg, limb;

    limr = stdmap->red_max;
    limg = stdmap->green_max;
    limb = stdmap->blue_max;
    for (i = 0, color = colors; i < ncolors; i++, color++)
	color->pixel = stdmap->base_pixel +
		       MapVal(color->red, limr, stdmap->red_mult) +
		       MapVal(color->green, limg, stdmap->green_mult) +
		       MapVal(color->blue, limb, stdmap->blue_mult);
    for (y = 0; y < in_image->height; y++) {
	for (x = 0; x < in_image->width; x++) {
	    XPutPixel(out_image, x, y,
		      colors[XGetPixel(in_image, x, y)].pixel);
	}
    }
}

static Colormap
CopyColormapAndFree(Display *dpy, Colormap colormap)
{
    if (colormap == DefaultColormap(dpy, DefaultScreen(dpy)))
	return XCopyColormapAndFree(dpy, colormap);
    Error("Visual type is not large enough to hold all colors of the image.");
    /*NOTREACHED*/
    return (Colormap)0;
}

static void
Do_Pseudo(Display *dpy, Colormap *colormap, 
	  int ncolors, XColor *colors, XImage *in_image, XImage *out_image)
{
    register int i, x, y;
    register XColor *color;

    for (i = 0; i < ncolors; i++)
	colors[i].flags = 0;
    for (y = 0; y < in_image->height; y++) {
	for (x = 0; x < in_image->width; x++) {
	    color = &colors[XGetPixel(in_image, x, y)];
	    if (!color->flags) {
		color->flags = DoRed | DoGreen | DoBlue;
		if (!XAllocColor(dpy, *colormap, color)) {
		    *colormap = CopyColormapAndFree(dpy, *colormap);
		    XAllocColor(dpy, *colormap, color);
		}
	    }
	    XPutPixel(out_image, x, y, color->pixel);
	}
    }
}

static void
Do_Direct(Display *dpy, XWDFileHeader *header, Colormap *colormap, 
	  int ncolors, XColor *colors, XImage *in_image, XImage *out_image,
          XVisualInfo *vinfo)
{
    register int x, y;
    XColor color;
    unsigned long rmask, gmask, bmask;
    unsigned long ormask, ogmask, obmask;
    unsigned long  rshift = 0, gshift = 0, bshift = 0;
    unsigned long  orshift = 0, ogshift = 0, obshift = 0;
    int i;
    unsigned long pix, xpix;
    unsigned long *pixels, *rpixels;

    rmask = header->red_mask;
    while (!(rmask & 1)) {
	rmask >>= 1;
	rshift++;
    }
    gmask = header->green_mask;
    while (!(gmask & 1)) {
	gmask >>= 1;
	gshift++;
    }
    bmask = header->blue_mask;
    while (!(bmask & 1)) {
	bmask >>= 1;
	bshift++;
    }
    if (in_image->depth <= 12) {
	pix = 1 << in_image->depth;
	pixels = (unsigned long *)malloc(sizeof(unsigned long) * pix);
	for (i = 0; i < pix; i++)
	    pixels[i] = ~0L;
	color.flags = DoRed | DoGreen | DoBlue;
	for (y = 0; y < in_image->height; y++) {
	    for (x = 0; x < in_image->width; x++) {
		pix = XGetPixel(in_image, x, y);
		if ((color.pixel = pixels[pix]) == ~0L) {
		    color.red = (pix >> rshift) & rmask;
		    color.green = (pix >> gshift) & gmask;
		    color.blue = (pix >> bshift) & bmask;
		    if (ncolors) {
			color.red = colors[color.red].red;
			color.green = colors[color.green].green;
			color.blue = colors[color.blue].blue;
		    } else {
			color.red = (((unsigned long)color.red * 65535) /
				     rmask);
			color.green = (((unsigned long)color.green * 65535) /
				       gmask);
			color.blue = (((unsigned long)color.blue * 65535) /
				      bmask);
		    }
		    if (!XAllocColor(dpy, *colormap, &color)) {
			*colormap = CopyColormapAndFree(dpy, *colormap);
			XAllocColor(dpy, *colormap, &color);
		    }
		    pixels[pix] = color.pixel;
		}
		XPutPixel(out_image, x, y, color.pixel);
	    }
	}
    } else if (header->visual_class == TrueColor &&
	       vinfo->class == TrueColor) {
	ormask = vinfo->red_mask;
	while (!(ormask & 1)) {
	    ormask >>= 1;
	    orshift++;
	}
	ogmask = vinfo->green_mask;
	while (!(ogmask & 1)) {
	    ogmask >>= 1;
	    ogshift++;
	}
	obmask = vinfo->blue_mask;
	while (!(obmask & 1)) {
	    obmask >>= 1;
	    obshift++;
	}
	for (y = 0; y < in_image->height; y++) {
	    for (x = 0; x < in_image->width; x++) {
		pix = XGetPixel(in_image, x, y);
		xpix = (((((pix >> rshift) & rmask) * 65535 / rmask)
			 * ormask / 65535) << orshift) |
		       (((((pix >> gshift) & gmask) * 65535 / gmask)
			 * ogmask / 65535) << ogshift) |
		       (((((pix >> bshift) & bmask) * 65535 / bmask)
			 * obmask / 65535) << obshift);
		XPutPixel(out_image, x, y, xpix);
	    }
	}
    } else {
	if (header->visual_class == TrueColor)
	    ncolors = 0;
	pix = 1 << 12;
	pixels = (unsigned long *)malloc(sizeof(unsigned long) * pix);
	rpixels = (unsigned long *)malloc(sizeof(unsigned long) * pix);
	for (i = 0; i < pix; i++) {
	    pixels[i] = ~0L;
	    rpixels[i] = ~0L;
	}
	color.flags = DoRed | DoGreen | DoBlue;
	for (y = 0; y < in_image->height; y++) {
	    for (x = 0; x < in_image->width; x++) {
		pix = XGetPixel(in_image, x, y);
		xpix = ((pix >> 12) ^ pix) & ((1 << 12) - 1);
		if (((color.pixel = pixels[xpix]) == ~0L) ||
		    (rpixels[xpix] != pix)) {
		    color.red = (pix >> rshift) & rmask;
		    color.green = (pix >> gshift) & gmask;
		    color.blue = (pix >> bshift) & bmask;
		    if (ncolors) {
			color.red = colors[color.red].red;
			color.green = colors[color.green].green;
			color.blue = colors[color.blue].blue;
		    } else {
			color.red = (((unsigned long)color.red * 65535) /
				     rmask);
			color.green = (((unsigned long)color.green * 65535) /
				       gmask);
			color.blue = (((unsigned long)color.blue * 65535) /
				      bmask);
		    }
		    if (!XAllocColor(dpy, *colormap, &color)) {
			*colormap = CopyColormapAndFree(dpy, *colormap);
			XAllocColor(dpy, *colormap, &color);
		    }
		    pixels[xpix] = color.pixel;
		    rpixels[xpix] = pix;
		}
		XPutPixel(out_image, x, y, color.pixel);
	    }
	}
    }
}

static unsigned int 
Image_Size(XImage *image)
{
    if (image->format != ZPixmap)
      return(image->bytes_per_line * image->height * image->depth);

    return((unsigned)image->bytes_per_line * image->height);
}

static void
Error(char *string)
{
	fprintf(stderr, "xwud: Error => %s\n", string);
	if (errno != 0) {
		perror("xwud");
		fprintf(stderr, "\n");
	}
	exit(1);
}

static void
_swapshort(char *bp, unsigned int n)
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

static void
_swaplong(char *bp, unsigned int n)
{
    register char c;
    register char *ep = bp + n;
    register char *sp;

    while (bp < ep) {
	sp = bp + 3;
	c = *sp;
	*sp = *bp;
	*bp++ = c;
	sp = bp + 1;
	c = *sp;
	*sp = *bp;
	*bp++ = c;
	bp += 2;
    }
}

static void
DumpHeader(const XWDFileHeader *header, const char *win_name)
{
	printf("window name:        %s\n", win_name);
	printf("sizeof(XWDheader):  %d\n", (int)sizeof(*header));
	printf("header size:        %d\n", (int)header->header_size);
	printf("file version:       %d\n", (int)header->file_version);
	printf("pixmap format:      %d\n", (int)header->pixmap_format);
	printf("pixmap depth:       %d\n", (int)header->pixmap_depth);
	printf("pixmap width:       %d\n", (int)header->pixmap_width);
	printf("pixmap height:      %d\n", (int)header->pixmap_height);
	printf("x offset:           %d\n", (int)header->xoffset);
	printf("byte order:         %d\n", (int)header->byte_order);
	printf("bitmap unit:        %d\n", (int)header->bitmap_unit);
	printf("bitmap bit order:   %d\n", (int)header->bitmap_bit_order);
	printf("bitmap pad:         %d\n", (int)header->bitmap_pad);
	printf("bits per pixel:     %d\n", (int)header->bits_per_pixel);
	printf("bytes per line:     %d\n", (int)header->bytes_per_line);
	printf("visual class:       %d\n", (int)header->visual_class);
	printf("red mask:           %d\n", (int)header->red_mask);
	printf("green mask:         %d\n", (int)header->green_mask);
	printf("blue mask:          %d\n", (int)header->blue_mask);
	printf("bits per rgb:       %d\n", (int)header->bits_per_rgb);
	printf("colormap entries:   %d\n", (int)header->colormap_entries);
	printf("num colors:         %d\n", (int)header->ncolors);
	printf("window width:       %d\n", (int)header->window_width);
	printf("window height:      %d\n", (int)header->window_height);
	printf("window x:           %d\n", (int)header->window_x);
	printf("window y:           %d\n", (int)header->window_y);
	printf("border width:       %d\n", (int)header->window_bdrwidth);
}

