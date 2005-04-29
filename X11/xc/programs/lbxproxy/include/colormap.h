/* $Xorg: colormap.h,v 1.4 2001/02/09 02:05:32 xorgcvs Exp $ */
/*

Copyright 1998  The Open Group

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
 * Copyright 1994 Network Computing Devices, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name Network Computing Devices, Inc. not be
 * used in advertising or publicity pertaining to distribution of this
 * software without specific, written prior permission.
 *
 * THIS SOFTWARE IS PROVIDED `AS-IS'.  NETWORK COMPUTING DEVICES, INC.,
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING WITHOUT
 * LIMITATION ALL IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE, OR NONINFRINGEMENT.  IN NO EVENT SHALL NETWORK
 * COMPUTING DEVICES, INC., BE LIABLE FOR ANY DAMAGES WHATSOEVER, INCLUDING
 * SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES, INCLUDING LOSS OF USE, DATA,
 * OR PROFITS, EVEN IF ADVISED OF THE POSSIBILITY THEREOF, AND REGARDLESS OF
 * WHETHER IN AN ACTION IN CONTRACT, TORT OR NEGLIGENCE, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 * 
 */
/* $XFree86: xc/programs/lbxproxy/include/colormap.h,v 1.7 2003/11/17 22:20:48 dawes Exp $ */

#ifndef COLORMAP_H_
#define COLORMAP_H_

typedef struct _rgbentry {
    char       *name;
    int         namelen;
    VisualID	visual;
    CARD16      xred,
                xblue,
                xgreen;		/* exact */
    CARD16      vred,
                vblue,
                vgreen;		/* visual */
} RGBEntryRec, *RGBEntryPtr;

typedef struct _RGBEntry {
    struct _RGBEntry *next;
    RGBEntryRec color;
} RGBCacheEntryRec, *RGBCacheEntryPtr;

#define NBUCKETS        16

typedef CARD32 Pixel;

#define PIXEL_FREE		0
#define PIXEL_PRIVATE		1
#define PIXEL_SHARED		2

typedef struct _entry {
    CARD16      red,
                green,
                blue;
    char	status;
    char	server_ref;
    int		refcnt;
    Pixel	pixel;
} Entry;


#define CMAP_NOT_GRABBED	0
#define CMAP_GRAB_REQUESTED	1
#define CMAP_GRABBED		2

#define DynamicClass  1

typedef struct _visual {
    int         class;
    VisualID    id;
    int         depth;
    int		bitsPerRGB;
    int		colormapEntries;
    CARD32	redMask, greenMask, blueMask;
    int		offsetRed, offsetGreen, offsetBlue;
} LbxVisualRec, *LbxVisualPtr;

#define NUMRED(pv) (((pv)->redMask >> (pv)->offsetRed) + 1)
#define NUMGREEN(pv) (((pv)->greenMask >> (pv)->offsetGreen) + 1)
#define NUMBLUE(pv) (((pv)->blueMask >> (pv)->offsetBlue) + 1)
#define REDPART(pv,pix) (((pix) & (pv)->redMask) >> (pv)->offsetRed)
#define GREENPART(pv,pix) (((pix) & (pv)->greenMask) >> (pv)->offsetGreen)
#define BLUEPART(pv,pix) (((pix) & (pv)->blueMask) >> (pv)->offsetBlue)

typedef struct _cmap {
    Colormap    id;
    LbxVisualPtr pVisual;
    Bool	grab_status;
    Entry      *red;
    Entry      *green;
    Entry      *blue;
    int        *numPixelsRed;
    int        *numPixelsGreen;
    int        *numPixelsBlue;
    Pixel     **clientPixelsRed;
    Pixel     **clientPixelsGreen;
    Pixel     **clientPixelsBlue;
}           ColormapRec, *ColormapPtr;


extern void (* LbxResolveColor)(
    LbxVisualPtr /* pVisual */,
    CARD16* /* red */,
    CARD16* /* green */,
    CARD16* /* blue */
);

extern void ResolveColor(
    LbxVisualPtr /* pVisual */,
    CARD16* /* red */,
    CARD16* /* green */,
    CARD16* /* blue */
);

extern Pixel (* LbxFindFreePixel)(
    ColormapPtr /* pmap */,
    CARD32 	/* red */,
    CARD32	/* green */,
    CARD32	/* blue */
);

extern Pixel FindFreePixel(
    ColormapPtr /* pmap */,
    CARD32 	/* red */,
    CARD32	/* green */,
    CARD32	/* blue */
);

extern Entry * (* LbxFindBestPixel)(
    ColormapPtr /* pmap */,
    CARD32	/* red */,
    CARD32	/* green */,
    CARD32	/* blue */,
    int		/* channels */
);

extern Entry * FindBestPixel(
    ColormapPtr	/* pmap */,
    CARD32	/* red */,
    CARD32	/* green */,
    CARD32	/* blue */,
    int		/* channels */
);

extern void ReleaseCmap(
    ClientPtr	/* client */,
    ColormapPtr	/* pmap */
);

extern int CreateColormap(
    ClientPtr /*client*/,
    Colormap /*cmap*/,
    VisualID /*visual*/
);

extern int FreeColormap(
    ClientPtr /*client*/,
    Colormap /*cmap*/
);

extern int CreateVisual(
    int /*depth*/,
    xVisualType * /*vis*/
);

extern LbxVisualPtr GetVisual(
     VisualID /*vid*/
);

extern Bool InitColors(
    void
);

extern RGBEntryPtr FindColorName(
    XServerPtr /*server*/,
    char * /*name*/,
    int /*len*/,
    LbxVisualPtr /*pVisual*/
);

extern Bool AddColorName(
    XServerPtr /*server*/,
    char * /*name*/,
    int /*len*/,
    RGBEntryRec * /*rgbe*/
);

extern void FreeColors(
    void
);

extern int DestroyColormap(
    ClientPtr /*client*/,
    pointer /*pmap*/,
    XID /*id*/
);

extern int FindPixel(
    ClientPtr /*client*/,
    ColormapPtr /*pmap*/,
    CARD32 /*red*/,
    CARD32 /*green*/,
    CARD32 /*blue*/,
    Entry ** /*pent*/
);

extern int IncrementPixel(
    ClientPtr /*pclient*/,
    ColormapPtr /*pmap*/,
    Entry * /*pent*/,
    Bool /*from_server*/
);

extern int AllocCell(
    ClientPtr /*pclient*/,
    ColormapPtr /*pmap*/,
    Pixel /*pixel*/
);

extern int StorePixel(
    ClientPtr /*client*/,
    ColormapPtr /*pmap*/,
    CARD32 /*red*/,
    CARD32 /*green*/,
    CARD32 /*blue*/,
    Pixel /*pixel*/,
    Bool /*from_server*/
);

extern void GotServerFreeCellsEvent(
    ColormapPtr	/* pmap */,
    Pixel	/* pixel_start */,
    Pixel	/* pixel_end */
);

extern void FreeAllClientPixels(
    ColormapPtr /* pmap */,
    int         /* client */
);

extern int FreeClientPixels(
    ClientPtr /*client*/,
    pointer /*pcr*/,
    XID /*id*/
);

extern int FreePixels(
    ClientPtr /*client*/,
    ColormapPtr /*pmap*/,
    int /*num*/,
    Pixel * /*pixels*/,
    Pixel /*mask*/
);

#endif				/* COLORMAP_H_ */
