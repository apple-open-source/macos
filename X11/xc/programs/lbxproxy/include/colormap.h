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
/* $XFree86: xc/programs/lbxproxy/include/colormap.h,v 1.6 2001/12/14 20:00:55 dawes Exp $ */

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
#if NeedNestedPrototypes
    LbxVisualPtr /* pVisual */,
    CARD16* /* red */,
    CARD16* /* green */,
    CARD16* /* blue */
#endif
);

extern void ResolveColor(
#if NeedFunctionPrototypes
    LbxVisualPtr /* pVisual */,
    CARD16* /* red */,
    CARD16* /* green */,
    CARD16* /* blue */
#endif
);

extern Pixel (* LbxFindFreePixel)(
#if NeedFunctionPrototypes
    ColormapPtr /* pmap */,
    CARD32 	/* red */,
    CARD32	/* green */,
    CARD32	/* blue */
#endif
);

extern Pixel FindFreePixel(
#if NeedFunctionPrototypes
    ColormapPtr /* pmap */,
    CARD32 	/* red */,
    CARD32	/* green */,
    CARD32	/* blue */
#endif
);

extern Entry * (* LbxFindBestPixel)(
#if NeedNestedPrototypes
    ColormapPtr /* pmap */,
    CARD32	/* red */,
    CARD32	/* green */,
    CARD32	/* blue */,
    int		/* channels */
#endif
);

extern Entry * FindBestPixel(
#if NeedNestedPrototypes
    ColormapPtr	/* pmap */,
    CARD32	/* red */,
    CARD32	/* green */,
    CARD32	/* blue */,
    int		/* channels */
#endif
);

extern void ReleaseCmap(
#if NeedFunctionPrototypes
    ClientPtr	/* client */,
    ColormapPtr	/* pmap */
#endif
);

extern int CreateColormap(
#if NeedFunctionPrototypes
    ClientPtr /*client*/,
    Colormap /*cmap*/,
    VisualID /*visual*/
#endif
);

extern int FreeColormap(
#if NeedFunctionPrototypes
    ClientPtr /*client*/,
    Colormap /*cmap*/
#endif
);

extern int CreateVisual(
#if NeedFunctionPrototypes
    int /*depth*/,
    xVisualType * /*vis*/
#endif
);

extern LbxVisualPtr GetVisual(
#if NeedFunctionPrototypes
     VisualID /*vid*/
#endif
);

extern Bool InitColors(
#if NeedFunctionPrototypes
    void
#endif
);

extern RGBEntryPtr FindColorName(
#if NeedFunctionPrototypes
    XServerPtr /*server*/,
    char * /*name*/,
    int /*len*/,
    LbxVisualPtr /*pVisual*/
#endif
);

extern Bool AddColorName(
#if NeedFunctionPrototypes
    XServerPtr /*server*/,
    char * /*name*/,
    int /*len*/,
    RGBEntryRec * /*rgbe*/
#endif
);

extern void FreeColors(
#if NeedFunctionPrototypes
    void
#endif
);

extern int DestroyColormap(
#if NeedFunctionPrototypes
    ClientPtr /*client*/,
    pointer /*pmap*/,
    XID /*id*/
#endif
);

extern int FindPixel(
#if NeedFunctionPrototypes
    ClientPtr /*client*/,
    ColormapPtr /*pmap*/,
    CARD32 /*red*/,
    CARD32 /*green*/,
    CARD32 /*blue*/,
    Entry ** /*pent*/
#endif
);

extern int IncrementPixel(
#if NeedFunctionPrototypes
    ClientPtr /*pclient*/,
    ColormapPtr /*pmap*/,
    Entry * /*pent*/,
    Bool /*from_server*/
#endif
);

extern int AllocCell(
#if NeedFunctionPrototypes
    ClientPtr /*pclient*/,
    ColormapPtr /*pmap*/,
    Pixel /*pixel*/
#endif
);

extern int StorePixel(
#if NeedFunctionPrototypes
    ClientPtr /*client*/,
    ColormapPtr /*pmap*/,
    CARD32 /*red*/,
    CARD32 /*green*/,
    CARD32 /*blue*/,
    Pixel /*pixel*/,
    Bool /*from_server*/
#endif
);

extern void GotServerFreeCellsEvent(
#if NeedFunctionPrototypes
    ColormapPtr	/* pmap */,
    Pixel	/* pixel_start */,
    Pixel	/* pixel_end */
#endif
);

extern void FreeAllClientPixels(
#if NeedFunctionPrototypes
    ColormapPtr /* pmap */,
    int         /* client */
#endif
);

extern int FreeClientPixels(
#if NeedFunctionPrototypes
    ClientPtr /*client*/,
    pointer /*pcr*/,
    XID /*id*/
#endif
);

extern int FreePixels(
#if NeedFunctionPrototypes
    ClientPtr /*client*/,
    ColormapPtr /*pmap*/,
    int /*num*/,
    Pixel * /*pixels*/,
    Pixel /*mask*/
#endif
);

#endif				/* COLORMAP_H_ */
