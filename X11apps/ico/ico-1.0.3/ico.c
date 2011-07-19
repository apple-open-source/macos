/***********************************************************

Copyright (c) 1987  X Consortium

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the X Consortium.


Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/

/******************************************************************************
 * Description
 *	Display a wire-frame rotating icosahedron, with hidden lines removed
 *
 * Arguments:
 *	-r		display on root window instead of creating a new one
 * (plus a host of others, try -help)
 *****************************************************************************/
/* Additions by jimmc@sci:
 *  faces and colors
 *  double buffering on the display
 *  additional polyhedra
 *  sleep switch
 */

/*
 * multi-thread version by Stephen Gildea, January 1992
 */

/* Additions by Carlos A M dos Santos, XFree86 project, September 1999:
 *  use of "q" to quit threads
 *  support for ICCCM delete window message
 *  better thread support - mutex and condition to control termination
 */

#ifdef HAVE_CONFIG_H
#include "config.h"

#include <X11/XlibConf.h>
#ifdef XTHREADS
# define MULTITHREAD
#endif
#endif /* HAVE_CONFIG_H / autoconf */

#include <math.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/Xfuncs.h>
#include <X11/keysym.h>
#include <stdio.h>
#ifdef MULTIBUFFER
#include <X11/extensions/multibuf.h>
#endif /* MULTIBUFFER */
#ifdef MULTITHREAD
#include <X11/Xthreads.h>
#endif
#include <X11/Xos.h>

#define MIN_ICO_WIDTH 5
#define MIN_ICO_HEIGHT 5
#define DEFAULT_ICO_WIDTH 150
#define DEFAULT_ICO_HEIGHT 150
#define DEFAULT_DELTAX 13
#define DEFAULT_DELTAY 9

#include "polyinfo.h"	/* define format of one polyhedron */

/* Now include all the files which define the actual polyhedra */
static Polyinfo polygons[] = {
#include "allobjs.h"
};
#define NumberPolygons sizeof(polygons)/sizeof(polygons[0])

#include <stdlib.h>
#include <time.h>	/* for time_t */
#include <sys/time.h>	/* for struct timeval */

typedef double Transform3D[4][4];

typedef struct {
	int prevX, prevY;
	unsigned long *plane_masks;	/* points into dbpair.plane_masks */
	unsigned long enplanemask;	/* what we enable for drawing */
	XColor *colors;		/* size = 2 ** totalplanes */
	unsigned long *pixels;	/* size = 2 ** planesperbuf */
} DBufInfo;

/* variables that need to be per-thread */

struct closure {
    /* these elements were originally in DBufPair, a struct type */
    int planesperbuf;
    int pixelsperbuf;	/* = 1<<planesperbuf */
    int totalplanes;	/* = 2*planesperbuf */
    int totalpixels;	/* = 1<<totalplanes */
    unsigned long *plane_masks;	/* size = totalplanes */
    unsigned long pixels[1];
    int dbufnum;
    DBufInfo bufs[2];
    DBufInfo *drawbuf, *dpybuf;
    /* end of old DBufPair dbpair */
    /* these elements were originally global variables */
    Window win, draw_window;
    int winW, winH;
    Colormap cmap;
    GC gcontext;
#ifdef MULTIBUFFER
    Multibuffer multibuffers[2];
#endif /* MULTIBUFFER */
    int nplanesets;
    /* items needed by drawPoly */
    char drawn[MAXNV][MAXNV];
    Transform3D xform;
    Point3D xv[2][MAXNV];
    int xv_buffer;
    double wo2, ho2;
#ifdef MULTITHREAD           
    int thread_num;
#endif
};


/* The display is shared and writable, but Xlib locks it as necessary */

static Display *dpy;

/* This atom will be used to catch the ICCCM "delete window" message. It will
 * be allocated once and used in read-only mode by threads, so it can be a
 * global variable */

static Atom wm_delete_window;

/*
 * variables that are not set except maybe in initialization before
 * any additional threads are created
 */

static char *Primaries[] = {
    "red", "green", "blue", "yellow", "cyan", "magenta"
};
#define NumberPrimaries 6

static const char *help_message[] = {
"where options include:",
"-display host:dpy           X server to use",
"    -geometry geom          geometry of window to use",
"    -size WxH               size of object to rotate",
"    -delta +X+Y             amount by which to move object",
"    -r                      draw in the root window",
"    -d number               dashed line pattern for wire frames",
"    -bg color               background color",
"    -colors color ...       codes to use on sides",
"    -p#                     use # (1 through 8) primary colors",
#ifdef MULTIBUFFER           
"    -dbl                    use double buffering (extension if present)",
#else                        
"    -dbl                    use double buffering (software only)",
#endif                       
"    -softdbl                use software double buffering",
"    -noedges                don't draw wire frame edges",
"    -faces                  draw faces",
"    -copy                   use multibuffer update action Copied",
"    -untouched              use multibuffer update action Untouched",
"    -undefined              use multibuffer update action Undefined",
"    -lw number              line width to use",
"    -i                      invert",
"    -sleep number           seconds to sleep in between draws",
"    -obj objname            type of polyhedral object to draw",
"    -objhelp                list polyhedral objects available",
#ifdef MULTITHREAD           
"    -threads number         number of windows, each its own thread",
#endif
NULL};

static const char *ProgramName;	/* argv[0] */

/*
 * variables set by command-line options
 */
static const char *geom = NULL;	/* -geometry: window geometry */
static int useRoot = 0;		/* -r */
static int dash = 0;		/* -d: dashed line pattern */
static char **colornames;	/* -colors (points into argv) */
#ifdef MULTIBUFFER
static int update_action = MultibufferUpdateActionBackground;
#endif
static int linewidth = 0;	/* -lw */
static int multibufext = 0;	/* -dbl: use Multi-Buffering extension */
static int dblbuf = 0;		/* -dbl or -softdbl: double buffering */
static int numcolors = 0;	/* -p: number of primary colors to use */
static const char *background_colorname = NULL; /* -bg */
static int doedges = 1;		/* -noedges turns this off */
static int dofaces = 0;		/* -faces */
static int invert = 0;		/* -i */
static const char *ico_geom = NULL;	/* -size: size of object in window */
static const char *delta_geom = NULL;	/* -delta: amount by which to move object */
static Polyinfo *poly;		/* -obj: the poly to draw */
static int dsync = 0;		/* -dsync */
static int xsync = 0;		/* -sync */
static int msleepcount = 0;	/* -sleep value in milliseconds*/
#ifdef MULTITHREAD
static int thread_count;
#ifdef XMUTEX_INITIALIZER
static xmutex_rec count_mutex = XMUTEX_INITIALIZER;
#else
static xmutex_rec count_mutex;
#endif
static xcondition_rec count_cond;/* Xthreads doesn't define an equivalent to
				 * PTHREAD_COND_INITIALIZER, so we must call
				 * xcondition_init later */
#endif

/******************************************************************************
 * Description
 *	Error handling
 *****************************************************************************/

#if defined(__GNUC__) && \
    ((__GNUC__ > 2) || ((__GNUC__ == 2) && (__GNUC_MINOR__ >= 7)))
void icoFatal (const char *fmt, const char *a0) __attribute__((__noreturn__));
#endif
void
icoFatal(const char *fmt, const char *a0)
{
	fprintf(stderr, "%s: ", ProgramName);
	fprintf(stderr, fmt, a0);
	fprintf(stderr, "\n");
	exit(1);
}


/******************************************************************************
 * Description
 *	Memory allocation
 *****************************************************************************/

static char *
xalloc(unsigned int nbytes)
{
        char *p;

	p = malloc(nbytes);
	if (p)
		return p;

	fprintf(stderr, "%s: no more memory\n", ProgramName);
	exit(1);
}


/******************************************************************************
 * Description
 *	Sleep a certain number of milliseconds
 *****************************************************************************/

static void
msleep(unsigned int msecs)
{
	struct timeval timeout;

	timeout.tv_sec = msecs / 1000; timeout.tv_usec = (msecs % 1000) * 1000;
	select(1,NULL,NULL,NULL,&timeout);
}


/******************************************************************************
 * Description
 *	Format a 4x4 identity matrix.
 *
 * Output
 *	*m		Formatted identity matrix
 *****************************************************************************/

static void
IdentMat(Transform3D m)
{
	int i;
	int j;

	for (i = 3; i >= 0; --i) {
		for (j = 3; j >= 0; --j)
			m[i][j] = 0.0;
		m[i][i] = 1.0;
	}
}


/******************************************************************************
 * Description
 *	Concatenate two 4-by-4 transformation matrices.
 *
 * Input
 *	l		multiplicand (left operand)
 *	r		multiplier (right operand)
 *
 * Output
 *	*m		Result matrix
 *****************************************************************************/

static void
ConcatMat(Transform3D l, Transform3D r, Transform3D m)
{
	int i;
	int j;

	for (i = 0; i < 4; ++i)
		for (j = 0; j < 4; ++j)
			m[i][j] = l[i][0] * r[0][j]
			    + l[i][1] * r[1][j]
			    + l[i][2] * r[2][j]
			    + l[i][3] * r[3][j];
}


/******************************************************************************
 * Description
 *	Format a matrix that will perform a rotation transformation
 *	about the specified axis.  The rotation angle is measured
 *	counterclockwise about the specified axis when looking
 *	at the origin from the positive axis.
 *
 * Input
 *	axis		Axis ('x', 'y', 'z') about which to perform rotation
 *	angle		Angle (in radians) of rotation
 *	A		Pointer to rotation matrix
 *
 * Output
 *	*m		Formatted rotation matrix
 *****************************************************************************/

static void
FormatRotateMat(char axis, double angle, Transform3D m)
{
	double s, c;

	IdentMat(m);

	s = sin(angle);
	c = cos(angle);

	switch (axis)
	{
		case 'x':
			m[1][1] = m[2][2] = c;
			m[1][2] = s;
			m[2][1] = -s;
			break;
		case 'y':
			m[0][0] = m[2][2] = c;
			m[2][0] = s;
			m[0][2] = -s;
			break;
		case 'z':
			m[0][0] = m[1][1] = c;
			m[0][1] = s;
			m[1][0] = -s;
			break;
	}
}


/******************************************************************************
 * Description
 *	Perform a partial transform on non-homogeneous points.
 *	Given an array of non-homogeneous (3-coordinate) input points,
 *	this routine multiplies them by the 3-by-3 upper left submatrix
 *	of a standard 4-by-4 transform matrix.  The resulting non-homogeneous
 *	points are returned.
 *
 * Input
 *	n		number of points to transform
 *	m		4-by-4 transform matrix
 *	in		array of non-homogeneous input points
 *
 * Output
 *	*out		array of transformed non-homogeneous output points
 *****************************************************************************/

static void
PartialNonHomTransform(int n, Transform3D m, const Point3D *in, Point3D *out)
{
	for (; n > 0; --n, ++in, ++out) {
		out->x = in->x * m[0][0] + in->y * m[1][0] + in->z * m[2][0];
		out->y = in->x * m[0][1] + in->y * m[1][1] + in->z * m[2][1];
		out->z = in->x * m[0][2] + in->y * m[1][2] + in->z * m[2][2];
	}
}


/*
 * Unfortunately we can not use XWindowEvent and XCheckWindowEvent to get
 * ClientMessage events, because there is no corresponding event mask. We must
 * use XIfEvent and XCheckIfEvent and this function as a predicate. Better if
 * Xlib had some kind of XWindowAnyEvent and XCheckWindowEvent. -- Casantos.
 */

static Bool
predicate(Display *display, XEvent *event, XPointer args)
{
    Window w = (Window) args;
    return event->xany.window == w;
}

/******************************************************************************
 * Description
 *	Icosahedron animator.
 *****************************************************************************/

static void
icoClearArea(struct closure *closure, int x, int y, int w, int h)
{
	if (multibufext && dblbuf)
		return;

	if (dblbuf || dofaces) {
		XSetForeground(dpy,
			closure->gcontext,
			closure->drawbuf->pixels[0]);

		/* use background as foreground color for fill */
		XFillRectangle(dpy,closure->win,closure->gcontext,x,y,w,h);
	} else {
		XClearArea(dpy,closure->win,x,y,w,h,0);
	}
}

/* Set up points, transforms, etc.  */

static void
initPoly(struct closure *closure, Polyinfo *poly, int icoW, int icoH)
{
    Point3D *vertices = poly->v;
    int NV = poly->numverts;
    Transform3D r1;
    Transform3D r2;

    FormatRotateMat('x', 5 * 3.1416 / 180.0, r1);
    FormatRotateMat('y', 5 * 3.1416 / 180.0, r2);
    ConcatMat(r1, r2, closure->xform);

    memcpy((char *)closure->xv[0], (char *)vertices, NV * sizeof(Point3D));
    closure->xv_buffer = 0;

    closure->wo2 = icoW / 2.0;
    closure->ho2 = icoH / 2.0;
}

static void
setDrawBuf (struct closure *closure, int n)
{
    XGCValues xgcv;
    unsigned long mask;

#ifdef MULTIBUFFER
    if (multibufext && dblbuf) {
	closure->win = closure->multibuffers[n];
	n = 0;
    }
#endif /* MULTIBUFFER */

    closure->drawbuf = closure->bufs+n;
    xgcv.foreground = closure->drawbuf->pixels[closure->pixelsperbuf-1];
    xgcv.background = closure->drawbuf->pixels[0];
    mask = GCForeground | GCBackground;
    if (dblbuf && !multibufext) {
	xgcv.plane_mask = closure->drawbuf->enplanemask;
	mask |= GCPlaneMask;
    }
    XChangeGC(dpy, closure->gcontext, mask, &xgcv);
}

static void
setDisplayBuf(struct closure *closure, int n, int firsttime)
{
#ifdef MULTIBUFFER
	if (multibufext && dblbuf) {
		XmbufDisplayBuffers (dpy, 1, &closure->multibuffers[n], msleepcount, 0);
		if (!firsttime)
			return;
		n = 0;
	}
#endif
	closure->dpybuf = closure->bufs+n;
	if (closure->totalpixels > 2)
	    XStoreColors(dpy,closure->cmap,closure->dpybuf->colors,closure->totalpixels);
}

static void
setBufColor(struct closure *closure, int n, XColor *color)
{
	int i,j,cx;
	DBufInfo *b;
	unsigned long pix;

	for (i=0; i<closure->nplanesets; i++) {
		b = closure->bufs+i;
		for (j=0; j<(dblbuf&&!multibufext?closure->pixelsperbuf:1); j++) {
			cx = n + j*closure->pixelsperbuf;
			pix = b->colors[cx].pixel;
			b->colors[cx] = *color;
			b->colors[cx].pixel = pix;
			b->colors[cx].flags = DoRed | DoGreen | DoBlue;
		}
	}
}

/******************************************************************************
 * Description
 *	Undraw previous polyhedron (by erasing its bounding box).
 *	Rotate and draw the new polyhedron.
 *
 * Input
 *	poly		the polyhedron to draw
 *	gc		X11 graphics context to be used for drawing
 *	icoX, icoY	position of upper left of bounding-box
 *	icoW, icoH	size of bounding-box
 *	prevX, prevY	position of previous bounding-box
 *****************************************************************************/

static void
drawPoly(struct closure *closure, Polyinfo *poly, GC gc,
	 int icoX, int icoY, int icoW, int icoH, int prevX, int prevY)
{
	int *f = poly->f;
	int NV = poly->numverts;
	int NF = poly->numfaces;

	int p0;
	int p1;
	XPoint *pv2;
	XSegment *pe;
	Point3D *pxv;
	XPoint v2[MAXNV];
	XSegment edges[MAXEDGES];
	int i;
	int j,k;
	int *pf;
	int facecolor;

	int pcount;
	double pxvz;
	XPoint ppts[MAXEDGESPERPOLY];

	/* Switch double-buffer and rotate vertices */

	closure->xv_buffer = !closure->xv_buffer;
	PartialNonHomTransform(NV, closure->xform,
		closure->xv[!closure->xv_buffer],
		closure->xv[closure->xv_buffer]);


	/* Convert 3D coordinates to 2D window coordinates: */

	pxv = closure->xv[closure->xv_buffer];
	pv2 = v2;
	for (i = NV - 1; i >= 0; --i) {
		pv2->x = (int) ((pxv->x + 1.0) * closure->wo2) + icoX;
		pv2->y = (int) ((pxv->y + 1.0) * closure->ho2) + icoY;
		++pxv;
		++pv2;
	}


	/* Accumulate edges to be drawn, eliminating duplicates for speed: */

	pxv = closure->xv[closure->xv_buffer];
	pv2 = v2;
	pf = f;
	pe = edges;
	bzero(closure->drawn, sizeof(closure->drawn));

	if (dblbuf)
		setDrawBuf(closure, closure->dbufnum);
			/* switch drawing buffers if double buffered */
	/* for faces, need to clear before FillPoly */
	if (dofaces && !(multibufext && dblbuf)) {
		/* multibuf uses update background */
		if (dblbuf)
			icoClearArea(closure,
				closure->drawbuf->prevX - linewidth/2,
				closure->drawbuf->prevY - linewidth/2,
				icoW + linewidth + 1, icoH + linewidth + 1);
		icoClearArea(closure,
			prevX - linewidth/2, prevY - linewidth/2,
			icoW + linewidth + 1, icoH + linewidth + 1);
	}

	if (dsync)
		XSync(dpy, 0);

	for (i = NF - 1; i >= 0; --i, pf += pcount) {

		pcount = *pf++;	/* number of edges for this face */
		pxvz = 0.0;
		for (j=0; j<pcount; j++) {
			p0 = pf[j];
			pxvz += pxv[p0].z;
		}

		/* If facet faces away from viewer, don't consider it: */
		if (pxvz<0.0)
			continue;

		if (dofaces) {
			if (numcolors)
				facecolor = i%numcolors + 1;
			else
				facecolor = 1;
			XSetForeground(dpy, gc,
				closure->drawbuf->pixels[facecolor]);
			for (j=0; j<pcount; j++) {
				p0 = pf[j];
				ppts[j].x = pv2[p0].x;
				ppts[j].y = pv2[p0].y;
			}
			XFillPolygon(dpy, closure->win, gc, ppts, pcount,
				Convex, CoordModeOrigin);
		}

		if (doedges) {
			for (j=0; j<pcount; j++) {
				if (j<pcount-1) k=j+1;
				else k=0;
				p0 = pf[j];
				p1 = pf[k];
				if (!closure->drawn[p0][p1]) {
					closure->drawn[p0][p1] = 1;
					closure->drawn[p1][p0] = 1;
					pe->x1 = pv2[p0].x;
					pe->y1 = pv2[p0].y;
					pe->x2 = pv2[p1].x;
					pe->y2 = pv2[p1].y;
					++pe;
				}
			}
		}
	}

	/* Erase previous, draw current icosahedrons; sync for smoothness. */

	if (doedges) {
		if (dofaces) {
			XSetForeground(dpy, gc, closure->drawbuf->pixels[0]);
				/* use background as foreground color */
		} else {
			if (dblbuf && !multibufext)
				icoClearArea(closure,
					closure->drawbuf->prevX - linewidth/2,
					closure->drawbuf->prevY - linewidth/2,
					icoW + linewidth + 1,
					icoH + linewidth + 1);
			if (!(multibufext && dblbuf))
				icoClearArea(closure,
					prevX - linewidth/2,
					prevY - linewidth/2,
					icoW + linewidth + 1,
					icoH + linewidth + 1);
			if (dblbuf || dofaces) {
				XSetForeground(dpy, gc, closure->drawbuf->pixels[
					closure->pixelsperbuf-1]);
			}
		}
		XDrawSegments(dpy, closure->win, gc, edges, pe - edges);
	}

	if (dsync)
		XSync(dpy, 0);

	if (dblbuf) {
		closure->drawbuf->prevX = icoX;
		closure->drawbuf->prevY = icoY;
		setDisplayBuf(closure, closure->dbufnum, 0);
	}
	if (dblbuf)
		closure->dbufnum = 1 - closure->dbufnum;
	if (!(multibufext && dblbuf) && msleepcount > 0)
		msleep(msleepcount);
}

static void
initDBufs(struct closure *closure, int fg, int bg, int planesperbuf)
{
	int i,j,jj,j0,j1,k,m,t;
	DBufInfo *b;
	XColor bgcolor, fgcolor;

	closure->nplanesets = (dblbuf && !multibufext ? 2 : 1);

	closure->planesperbuf = planesperbuf;
	closure->pixelsperbuf = 1<<planesperbuf;
	closure->totalplanes = closure->nplanesets * planesperbuf;
	closure->totalpixels = 1<<closure->totalplanes;
	closure->plane_masks = (unsigned long *)
		xalloc(closure->totalplanes * sizeof(unsigned long));
	closure->dbufnum = 0;
	for (i=0; i < closure->nplanesets; i++) {
		b = closure->bufs+i;
		b->plane_masks = closure->plane_masks + (i*planesperbuf);
		b->colors = (XColor *)
			xalloc(closure->totalpixels * sizeof(XColor));
		b->pixels = (unsigned long *)
			xalloc(closure->pixelsperbuf * sizeof(unsigned long));
	}

	if (closure->totalplanes == 1) {
	    closure->pixels[0] = bg;
	    closure->plane_masks[0] = fg ^ bg;
	} else {
	    t = XAllocColorCells(dpy,closure->cmap,0,
		    closure->plane_masks,closure->totalplanes, closure->pixels,1);
			    /* allocate color planes */
	    if (t==0) {
		    icoFatal("can't allocate enough color planes", NULL);
	    }
	}

	fgcolor.pixel = fg;
	bgcolor.pixel = bg;
	XQueryColor(dpy,closure->cmap,&fgcolor);
	XQueryColor(dpy,closure->cmap,&bgcolor);

	setBufColor(closure, 0,&bgcolor);
	setBufColor(closure, 1,&fgcolor);
	for (i=0; i<closure->nplanesets; i++) {
		b = closure->bufs+i;
		for (j0=0; j0<(dblbuf&&!multibufext?closure->pixelsperbuf:1); j0++) {
		    for (j1=0; j1<closure->pixelsperbuf; j1++) {
			j = (j0<<closure->planesperbuf)|j1;
			if (i==0) jj=j;
			else jj= (j1<<closure->planesperbuf)|j0;
			b->colors[jj].pixel = closure->pixels[0];
			for (k=0, m=j; m; k++, m=m>>1) {
				if (m&1)
				   b->colors[jj].pixel ^= closure->plane_masks[k];
			}
			b->colors[jj].flags = DoRed | DoGreen | DoBlue;
		    }
		}
		b->prevX = b->prevY = 0;
		b->enplanemask = 0;
		for (j=0; j<planesperbuf; j++) {
			b->enplanemask |= b->plane_masks[j];
		}
		for (j=0; j<closure->pixelsperbuf; j++) {
			b->pixels[j] = closure->pixels[0];
			for (k=0, m=j; m; k++, m=m>>1) {
				if (m&1)
				   b->pixels[j] ^= b->plane_masks[k];
			}
		}
	}

	if (!(multibufext && dblbuf)) {
	    setDrawBuf(closure, 0);
	    XSetBackground(dpy, closure->gcontext, closure->bufs[0].pixels[0]);
	    XSetWindowBackground(dpy, closure->draw_window, closure->bufs[0].pixels[0]);
	    XSetPlaneMask(dpy, closure->gcontext, AllPlanes);
	    icoClearArea(closure, 0, 0, closure->winW, closure->winH); /* clear entire window */
	}
}

static void
setBufColname(struct closure *closure, int n, char *colname)
{
	int t;
	XColor dcolor, color;

	t = XLookupColor(dpy,closure->cmap,colname,&dcolor,&color);
	if (t==0) {	/* no such color */
		icoFatal("no such color %s",colname);
	}
	setBufColor(closure, n,&color);
}


/* function to create and run an ico window */
static void *
do_ico_window(void *ptr)
{
	int fg, bg;
	XSetWindowAttributes xswa;
	XWindowAttributes xwa;
	XEvent xev;
	int icoX, icoY;
	unsigned long vmask;
	XGCValues xgcv;
	int initcolors = 0;
	int icoDeltaX = DEFAULT_DELTAX, icoDeltaY = DEFAULT_DELTAY;
	int icodeltax2, icodeltay2;
	Bool blocking = False;
	int winX, winY;
	int icoW = 0, icoH = 0;
	KeySym ksym;
	Bool do_it = True;
	char buf[20];
	struct closure *closure = ptr;
#ifdef MULTITHREAD           
	int len;
#endif

#ifdef DEBUG
	printf("thread %x starting\n", xthread_self());
#endif
	closure->cmap = XDefaultColormap(dpy,DefaultScreen(dpy));
	if (!closure->cmap) {
		icoFatal("no default colormap!", NULL);
	}

	fg = WhitePixel(dpy, DefaultScreen(dpy));
	bg = BlackPixel(dpy, DefaultScreen(dpy));
	if (background_colorname) {
	    XColor cdef, igndef;

	    if (XAllocNamedColor (dpy, closure->cmap, background_colorname,
				  &cdef, &igndef))
	      bg = cdef.pixel;
	    else 
	      icoFatal("background: no such color \"%s\"",background_colorname);
	}
	if (numcolors && (!dofaces || numcolors == 1)) {
	    XColor cdef, igndef;

	    if (XAllocNamedColor (dpy, closure->cmap, colornames[0], &cdef, &igndef))
	      fg = cdef.pixel;
	    else 
	      icoFatal("face: no such color \"%s\"", colornames[0]);
	}

	if (invert) {
	    unsigned long tmp = fg;
	    fg = bg;
	    bg = tmp;
	}

	/* Set up window parameters, create and map window if necessary */

	if (useRoot) {
		closure->draw_window = DefaultRootWindow(dpy);
		winX = 0;
		winY = 0;
		closure->winW = DisplayWidth(dpy, DefaultScreen(dpy));
		closure->winH = DisplayHeight(dpy, DefaultScreen(dpy));
	} else {
		closure->winW = closure->winH = (multibufext&&dblbuf ? 300 : 600);
		winX = (DisplayWidth(dpy, DefaultScreen(dpy)) - closure->winW) >> 1;
		winY = (DisplayHeight(dpy, DefaultScreen(dpy)) - closure->winH) >> 1;
		if (geom) 
			XParseGeometry(geom, &winX, &winY,
				       (unsigned int *)&closure->winW,
				       (unsigned int *)&closure->winH);

		xswa.event_mask = ExposureMask |
				  StructureNotifyMask |
				  KeyPressMask;
		xswa.background_pixel = bg;
		xswa.border_pixel = fg;

		closure->draw_window = XCreateWindow(dpy,
		    DefaultRootWindow(dpy), 
		    winX, winY, closure->winW, closure->winH, 0, 
		    DefaultDepth(dpy, DefaultScreen(dpy)), 
		    InputOutput, DefaultVisual(dpy, DefaultScreen(dpy)),
		    CWEventMask | CWBackPixel | CWBorderPixel, &xswa);
#ifdef MULTITHREAD           
		len = sprintf(buf, "Ico: thread %d", closure->thread_num);
		XChangeProperty(dpy, closure->draw_window,
				XA_WM_NAME, XA_STRING, 8, 
				PropModeReplace, (unsigned char *)buf, len);
#else
		XChangeProperty(dpy, closure->draw_window,
				XA_WM_NAME, XA_STRING, 8, 
				PropModeReplace, (unsigned char *)"Ico", 3);
#endif
		(void) XSetWMProtocols (dpy, closure->draw_window,
					&wm_delete_window, 1);
		XMapWindow(dpy, closure->draw_window);
#ifdef DEBUG
		printf("thread %x waiting for Expose\n", xthread_self());
#endif
		for (;;) {
		    XIfEvent(dpy, &xev, predicate, (XPointer) closure->draw_window);
		    if (xev.type == Expose)
			break;
		}
#ifdef DEBUG
		printf("thread %x got Expose\n", xthread_self());
#endif
		if (XGetWindowAttributes(dpy,closure->draw_window,&xwa)==0) {
			icoFatal("cannot get window attributes (size)", NULL);
		}
		closure->winW = xwa.width;
		closure->winH = xwa.height;
	}

	if (ico_geom) 
	  XParseGeometry (ico_geom, &icoX, &icoY,
			  (unsigned int *)&icoW,
			  (unsigned int *)&icoH);
	if (icoW <= 0) icoW = DEFAULT_ICO_WIDTH;
	if (icoH <= 0) icoH = DEFAULT_ICO_HEIGHT;
	if (icoW < MIN_ICO_WIDTH) icoW = MIN_ICO_WIDTH;
	if (icoH < MIN_ICO_HEIGHT) icoH = MIN_ICO_HEIGHT;

	if (delta_geom) {
	    unsigned int junk;

	    XParseGeometry (delta_geom, &icoDeltaX, &icoDeltaY, &junk, &junk);
	    if (icoDeltaX == 0 && icoDeltaY == 0) {
		icoDeltaX = DEFAULT_DELTAX;
		icoDeltaY = DEFAULT_DELTAY;
	    }
	}

	closure->win = None;

#ifdef MULTIBUFFER
	if (multibufext && dblbuf) {
	    if (XmbufCreateBuffers (dpy, closure->draw_window, 2, update_action,
				    MultibufferUpdateHintFrequent,
				    closure->multibuffers) == 2) {
		XCopyArea (dpy, closure->draw_window, closure->multibuffers[1],
			   DefaultGC(dpy, DefaultScreen(dpy)),
			   0, 0, closure->winW, closure->winH, 0, 0);
		closure->win = closure->multibuffers[1];
	    } else 
	      icoFatal ("unable to obtain 2 buffers", NULL);
	}
#endif /* MULTIBUFFER */
	if (closure->win == None) closure->win = closure->draw_window;

	/* Set up a graphics context */

	vmask = (GCBackground | GCForeground | GCLineWidth);
	xgcv.background = bg;
	xgcv.foreground = fg;
	xgcv.line_width = linewidth;
	if (dash) {
	    xgcv.line_style = LineDoubleDash;
	    xgcv.dashes = dash;
	    vmask |= (GCLineStyle | GCDashList);
	}
	closure->gcontext = XCreateGC (dpy, closure->draw_window, vmask, &xgcv);

	if (dofaces && numcolors>1) {
	    int i,t,bits;
		bits = 0;
		for (t=numcolors; t; t=t>>1) bits++;
		initDBufs(closure, fg,bg,bits);
		/* don't set the background color */
		for (i=0; i<numcolors; i++) {
			setBufColname(closure, i+1,colornames[i]);
		}
		initcolors = 1;
	}
	else if (dblbuf || dofaces) {
		initDBufs(closure, fg,bg,1);
		initcolors = 1;
	}
	if (initcolors) {
	    setDisplayBuf(closure, dblbuf?1:0, 1); /* insert new colors */
	}

	if (dsync)
		XSync(dpy, 0);

	/* Get the initial position, size, and speed of the bounding-box */

	srand((int) time((time_t *)0) % 231);
	icoX = ((closure->winW - icoW) * (rand() & 0xFF)) >> 8;
	icoY = ((closure->winH - icoH) * (rand() & 0xFF)) >> 8;


	/* Bounce the box in the window */

	icodeltax2 = icoDeltaX * 2;
	icodeltay2 = icoDeltaY * 2;
	initPoly(closure, poly, icoW, icoH);

	while (do_it) {
		int prevX;
		int prevY;
		Bool do_event;

		/*
		 * This is not a good example of how to do event reading
		 * in multi-threaded programs.  More commonly there would
		 * be one thread reading all events and dispatching them
		 * to the appropriate thread.  However, the threaded version
		 * of ico was developed to test the MT Xlib implementation,
		 * so it is useful to have it behave a little oddly.
		 * For a discussion of how to write multi-threaded X programs,
		 * see Gildea, S., "Multi-Threaded Xlib", The X Resource,
		 * Issue 5, January 1993, pp. 159-166.
		 */
		if (blocking) {
		    XIfEvent(dpy, &xev, predicate, (XPointer) closure->win);
		    do_event = True;
		} else
		    do_event = XCheckIfEvent(dpy, &xev, predicate,
			    (XPointer) closure->win);
		if (do_event) {
		    switch (xev.type) {
		      case ConfigureNotify:
#ifdef DEBUG
			printf("thread %x configure\n", xthread_self());
#endif
			if (xev.xconfigure.width != closure->winW ||
			    xev.xconfigure.height != closure->winH)
			  icoX = icoY = 1;
			closure->winW = xev.xconfigure.width;
			closure->winH = xev.xconfigure.height;
			break;
		      case KeyPress:
#ifdef DEBUG
			printf("thread %x keypress\n", xthread_self());
#endif
			XLookupString(&xev.xkey, buf, 10, &ksym, NULL);
			do_it = ((ksym != XK_Q) && ksym != XK_q);
			break;
		      case MapNotify:
			blocking = False;
#ifdef DEBUG
			printf("thread %x unblocking\n", xthread_self());
#endif
			break;
		      case UnmapNotify:
			blocking = True;
#ifdef DEBUG
			printf("thread %x blocking\n", xthread_self());
#endif
			break;
		      case ClientMessage:
#ifdef DEBUG
			printf("thread %x message\n", xthread_self());
#endif
			if (xev.xclient.data.l[0] == wm_delete_window)
			    do_it = False;
			else
			    XBell (dpy, 0);
			continue;
		    }
		}

		prevX = icoX;
		prevY = icoY;

		icoX += icoDeltaX;
		if (icoX < 0 || icoX + icoW > closure->winW) {
			icoX -= icodeltax2;
			icoDeltaX = - icoDeltaX;
			icodeltax2 = icoDeltaX * 2;
		}
		icoY += icoDeltaY;
		if (icoY < 0 || icoY + icoH > closure->winH) {
			icoY -= icodeltay2;
			icoDeltaY = - icoDeltaY;
			icodeltay2 = icoDeltaY * 2;
		}

		drawPoly(closure, poly, closure->gcontext,
			 icoX, icoY, icoW, icoH, prevX, prevY);
	}
	XDestroyWindow(dpy, closure->win);
#ifdef MULTITHREAD
	xmutex_lock(&count_mutex);
	thread_count--;
	if (thread_count == 0) {
	    xcondition_broadcast(&count_cond);
	}
	xmutex_unlock(&count_mutex);
#endif
	return NULL;
}

/******************************************************************************
 * Description
 *	Main routine.  Process command-line arguments, then bounce a bounding
 *	box inside the window.  Call DrawIco() to redraw the icosahedron.
 *****************************************************************************/

static void
giveObjHelp(void)
{
	int i;
	Polyinfo *poly;

	printf("%-16s%-12s  #Vert.  #Edges  #Faces  %-16s\n",
		"Name", "ShortName", "Dual");
	for (i=0; i<NumberPolygons; i++) {
		poly = polygons+i;
		printf("%-16s%-12s%6d%8d%8d    %-16s\n",
			poly->longname, poly->shortname,
			poly->numverts, poly->numedges, poly->numfaces,
			poly->dual);
	}
}

static Polyinfo *
findpoly(const char *name)
{
	int i;
        Polyinfo *poly;

	for (i=0; i<NumberPolygons; i++) {
		poly = polygons+i;
		if (strcmp(name,poly->longname)==0 || strcmp(name,poly->shortname)==0)
			return poly;
	}
	icoFatal("can't find object %s", name);
}

int main(int argc, char **argv)
{
	const char *display = NULL;
#ifdef MULTIBUFFER
	int mbevbase, mberrbase;
#endif
#ifdef MULTITHREAD
	int nthreads = 1;	/* -threads: number of windows */
	int i;
#endif
	struct closure *closure;

	ProgramName = argv[0];

	/* Process arguments: */

	poly = findpoly("icosahedron");	/* default */

	for (argv++, argc--; argc > 0; argv++, argc--) {
		if (!strcmp (*argv, "-display")) {
			if (argc < 2)
				icoFatal("missing argument for %s", *argv);
			display = *++argv; argc--;
		} else if (!strncmp (*argv, "-g", 2)) {
			if (argc < 2)
				icoFatal("missing argument for %s", *argv);
			geom = *++argv; argc--;
		} else if (!strcmp(*argv, "-r"))
			useRoot = 1;
		else if (!strcmp (*argv, "-d")) {
			if (argc < 2)
				icoFatal("missing argument for %s", *argv);
			dash = atoi(*++argv); argc--;
		}
#ifdef MULTITHREAD
		else if (!strcmp(*argv, "-threads")) {
			if (argc < 2)
				icoFatal("missing argument for %s", *argv);
		        nthreads = atoi(*++argv); argc--;
		}
#endif
		else if (!strcmp(*argv, "-colors")) {
			if (argc < 2)
				icoFatal("missing argument for %s", *argv);
			colornames = ++argv; argc--; numcolors = 0;
			for ( ; argc > 0 && argv[0][0]!='-'; argv++, argc--, numcolors++) ;
			argv--; argc++;
		}
		else if (!strcmp (*argv, "-copy")) {
#ifdef MULTIBUFFER
			update_action = MultibufferUpdateActionCopied;
#endif
		}
		else if (!strcmp (*argv, "-untouched")) {
#ifdef MULTIBUFFER
			update_action = MultibufferUpdateActionUntouched;
#endif
		}
		else if (!strcmp (*argv, "-undefined")) {
#ifdef MULTIBUFFER
			update_action = MultibufferUpdateActionUndefined;
#endif
		} else if (!strcmp (*argv, "-lw")) {
			if (argc < 2)
				icoFatal("missing argument for %s", *argv);
			linewidth = atoi(*++argv); argc--;
		} else if (!strcmp (*argv, "-dbl")) {
			dblbuf = 1;
#ifdef MULTIBUFFER
			multibufext = 1;
#endif
		}
		else if (!strcmp(*argv, "-softdbl")) {
		        dblbuf = 1;
			multibufext = 0;
		}
		else if (!strncmp(*argv, "-p", 2)) {
			numcolors = atoi(argv[0]+2);
			if (numcolors < 1 || numcolors > NumberPrimaries)
				numcolors = NumberPrimaries;
			colornames = Primaries;
			dofaces = 1;
		}
		else if (!strcmp(*argv, "-bg")) {
			if (argc < 2)
				icoFatal("missing argument for %s", *argv);
			background_colorname = *++argv; argc--;
		} else if (!strcmp(*argv, "-noedges"))
			doedges = 0;
		else if (!strcmp(*argv, "-faces"))
			dofaces = 1;
		else if (!strcmp(*argv, "-i"))
			invert = 1;
		else if (!strcmp(*argv, "-size")) {
			if (argc < 2)
				icoFatal("missing argument for %s", *argv);
			ico_geom = *++argv; argc--;
		} else if (!strcmp(*argv, "-delta")) {
			if (argc < 2)
				icoFatal("missing argument for %s", *argv);
			delta_geom = *++argv; argc--;
		} else if (!strcmp (*argv, "-sleep")) {
			float f;
			if (argc < 2)
				icoFatal("missing argument for %s", *argv);
			if (sscanf (*++argv, "%f", &f) < 1)
				icoFatal("invalid argument for %s", argv[-1]);
			msleepcount = (int) (f * 1000.0);
			argc--;
		} else if (!strcmp (*argv, "-obj")) {
			if (argc < 2)
				icoFatal("missing argument for %s", *argv);
			poly = findpoly(*++argv); argc--;
		} else if (!strcmp(*argv, "-dsync"))
			dsync = 1;
		else if (!strncmp(*argv, "-sync",  5)) 
			xsync = 1;
		else if (!strcmp(*argv, "-objhelp")) {
			giveObjHelp();
			exit(1);
		}
		else {	/* unknown arg */
			const char **cpp;

			fprintf (stderr, "usage:  %s [options]\n\n",
			         ProgramName);
			for (cpp = help_message; *cpp; cpp++)
				fprintf (stderr, "%s\n", *cpp);
			exit (1);
		}
	}

	if (!dofaces && !doedges)
		icoFatal("nothing to draw", NULL);

#ifdef MULTITHREAD
	XInitThreads();
#endif
	if (!(dpy = XOpenDisplay(display)))
	    icoFatal("cannot open display \"%s\"", XDisplayName(display));
    	wm_delete_window = XInternAtom (dpy, "WM_DELETE_WINDOW", False);
	if (xsync)
	    XSynchronize(dpy, True);

#ifdef MULTIBUFFER
	if (multibufext && !XmbufQueryExtension (dpy, &mbevbase, &mberrbase)) {
	    multibufext = 0;
	}
#endif

#ifdef MULTITHREAD
#ifndef XMUTEX_INITIALIZER
	xmutex_init(&count_mutex);
#endif
	xcondition_init(&count_cond);

	/* start all threads here */
	thread_count = nthreads;
	for (i=1; i <= nthreads; i++) {
	    closure = (struct closure *) xalloc(sizeof(struct closure));
	    closure->thread_num = i;
	    xthread_fork(do_ico_window, closure);
	}
	/* wait until all theads terminate */
	xmutex_lock(&count_mutex);
	xcondition_wait(&count_cond, &count_mutex);
	xmutex_unlock(&count_mutex);
#else
	/* start the animation */
	closure = (struct closure *) xalloc(sizeof(struct closure));
	do_ico_window(closure);
#endif
	XCloseDisplay(dpy);
	return 0;
}
