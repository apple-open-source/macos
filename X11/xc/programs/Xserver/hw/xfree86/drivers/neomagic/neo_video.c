/**********************************************************************
Copyright 2002 by Shigehiro Nomura.

                        All Rights Reserved

Permission to use, copy, modify, distribute, and sell this software and
its documentation for any purpose is hereby granted without fee,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of Shigehiro Nomura not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  Shigehiro Nomura
and its suppliers make no representations about the suitability of this
software for any purpose.  It is provided "as is" without express or 
implied warranty.

SHIGEHIRO NOMURA DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
EVENT SHALL SHIGEHIRO NOMURA AND/OR ITS SUPPLIERS BE LIABLE FOR ANY
SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
**********************************************************************/

/*
 * Copyright 2002 SuSE Linux AG, Author: Egbert Eich
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/neomagic/neo_video.c,v 1.4 2002/11/25 14:05:00 eich Exp $ */

#include "neo.h"
#include "neo_video.h"

#define nElems(x)		(sizeof(x) / sizeof(x[0]))
#define MAKE_ATOM(a)	MakeAtom(a, sizeof(a) - 1, TRUE)

#if defined(XvExtension)

#include "dixstruct.h"
#include "xaa.h"
#include "xaalocal.h"

static XF86VideoAdaptorPtr NEOSetupVideo(ScreenPtr);

static int NEOPutVideo(ScrnInfoPtr, short, short, short, short, 
		       short, short, short, short, RegionPtr, pointer);

static void NEOStopVideo(ScrnInfoPtr, pointer, Bool);
static int NEOSetPortAttribute(ScrnInfoPtr, Atom, INT32, pointer);
static int NEOGetPortAttribute(ScrnInfoPtr, Atom, INT32 *, pointer);
static void NEOQueryBestSize(ScrnInfoPtr, Bool, short, short, short, 
			     short, unsigned int *, unsigned int *, pointer);
static int NEOPutImage(ScrnInfoPtr, short, short, short, short, short, short, 
		       short, short, int, unsigned char *, short, short, Bool,
		       RegionPtr, pointer);
static int NEOQueryImageAttributes(ScrnInfoPtr, int, unsigned short *, 
				   unsigned short *, int *, int *);

static Bool RegionsEqual(RegionPtr, RegionPtr);
static void NEODisplayVideo(ScrnInfoPtr, int, int, short, short, int, int, 
			    int, int, int, BoxPtr, short, short, short, short);

static void NEOInitOffscreenImages(ScreenPtr);
static FBLinearPtr NEOAllocateMemory(ScrnInfoPtr, FBLinearPtr, int);
static void NEOCopyData(unsigned char *, unsigned char *, int, int, int, int);
static void NEOCopyYV12Data(unsigned char *, unsigned char *, unsigned char *,
			    unsigned char *, int, int, int, int, int);

static int NEOAllocSurface(ScrnInfoPtr, int, unsigned short, unsigned short, 
			   XF86SurfacePtr);
static int NEOFreeSurface(XF86SurfacePtr);
static int NEODisplaySurface(XF86SurfacePtr, short, short, short, short, 
			     short, short, short, short, RegionPtr clipBoxes);
static int NEOStopSurface(XF86SurfacePtr);
static int NEOGetSurfaceAttribute(ScrnInfoPtr, Atom, INT32 *);
static int NEOSetSurfaceAttribute(ScrnInfoPtr, Atom, INT32);

static Atom xvColorKey, xvBrightness, xvInterlace;

void
NEOInitVideo(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    NEOPtr nPtr = NEOPTR(pScrn);
    XF86VideoAdaptorPtr *overlayAdaptors, *newAdaptors = NULL;
    XF86VideoAdaptorPtr newAdaptor = NULL;
    int numAdaptors;

    numAdaptors = xf86XVListGenericAdaptors(pScrn, &overlayAdaptors);

    if (nPtr->NeoChipset >= NM2160 
	&& !nPtr->noLinear 
	&& nPtr->NeoMMIOBase2 != NULL){
	nPtr->video = TRUE;
	newAdaptor = NEOSetupVideo(pScreen);
	NEOInitOffscreenImages(pScreen);
    } else
	nPtr->video = FALSE;

    if (newAdaptor){
	if (!numAdaptors){
	    numAdaptors = 1;
	    overlayAdaptors = &newAdaptor;
	} else {
	    newAdaptors = xalloc((numAdaptors + 1) 
				 * sizeof(XF86VideoAdaptorPtr*));
	    if (newAdaptors){
		memcpy(newAdaptors, overlayAdaptors, 
		       numAdaptors * sizeof(XF86VideoAdaptorPtr));
		newAdaptors[numAdaptors++] = newAdaptor;
		overlayAdaptors = newAdaptors;
	    }
	}
    }

    if (numAdaptors)
	xf86XVScreenInit(pScreen, overlayAdaptors, numAdaptors);

    if (newAdaptors)
	xfree(newAdaptors);
}

static XF86VideoEncodingRec NEOVideoEncodings[] =
{
    {
	NEO_VIDEO_VIDEO,
	"XV_VIDEO",
	1024, 1024,
	{1, 1}
    },
    {
	NEO_VIDEO_IMAGE,
	"XV_IMAGE",
	1024, 1024,
	{1, 1}
    }
};

static XF86VideoFormatRec NEOVideoFormats[] =
{
    {  8, PseudoColor },
    { 15, TrueColor },
    { 16, TrueColor },
    { 24, TrueColor },
};

static XF86AttributeRec NEOVideoAttributes[] =
{
    {
	XvSettable | XvGettable,
	0x000000, 0xFFFFFF,
	"XV_COLORKEY"
    },
    {
	XvSettable | XvGettable,
	-128, 127,
	"XV_BRIGHTNESS"
    },
    {
	XvSettable | XvGettable,
	0,2,
	"XV_INTERLACE"
    },
};

static XF86ImageRec NEOVideoImages[] =
{
    XVIMAGE_YUY2,
    XVIMAGE_YV12,
    XVIMAGE_I420,
    {
	FOURCC_RV15,
	XvRGB,
	LSBFirst,
	{ 'R', 'V' ,'1', '5',
	  0x00,'5',0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
	16,
	XvPacked,
	1,
	15, 0x001F, 0x03E0, 0x7C00,
	0, 0, 0,
	0, 0, 0,
	0, 0, 0,
	{ 'R', 'V', 'B' },
	XvTopToBottom
    },
    {
	FOURCC_RV16,
	XvRGB,
	LSBFirst,
	{ 'R', 'V' ,'1', '6',
	  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 },
	16,
	XvPacked,
	1,
	16, 0xF800, 0x07E0, 0x001F,
	0, 0, 0,
	0, 0, 0,
	0, 0, 0,
	{ 'R', 'V', 'B' },
	XvTopToBottom
    }
};

static XF86VideoAdaptorPtr
NEOSetupVideo(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    NEOPtr nPtr = NEOPTR(pScrn);
    NEOPortPtr pPriv;
    XF86VideoAdaptorPtr overlayAdaptor;
    int i;

#ifdef DEBUG
    xf86DrvMsg(pScrn->scrnIndex,X_INFO,"NEOSetupVideo\n");
#endif
    if ((overlayAdaptor = xcalloc(1, sizeof(XF86VideoAdaptorRec) +
			      sizeof(DevUnion) + 
			      sizeof(NEOPortRec))) == NULL){
	return (NULL);
    }

    overlayAdaptor->type = XvInputMask | XvImageMask | XvWindowMask 
	| XvOutputMask | XvVideoMask;
    overlayAdaptor->flags = VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT;
    overlayAdaptor->name = "NeoMagic Video Engine";
    overlayAdaptor->nEncodings = nElems(NEOVideoEncodings);
    overlayAdaptor->pEncodings = NEOVideoEncodings;
    for (i = 0; i < nElems(NEOVideoEncodings); i++){
	NEOVideoEncodings[i].width = 1024;
	NEOVideoEncodings[i].height = 1024;
    }
    overlayAdaptor->nFormats = nElems(NEOVideoFormats);
    overlayAdaptor->pFormats = NEOVideoFormats;
    overlayAdaptor->nPorts = 1;
    overlayAdaptor->pPortPrivates = (DevUnion*) &overlayAdaptor[1];
    overlayAdaptor->pPortPrivates[0].ptr = 
	(pointer) &overlayAdaptor->pPortPrivates[1];
    overlayAdaptor->nAttributes = nElems(NEOVideoAttributes);
    overlayAdaptor->pAttributes = NEOVideoAttributes;
    overlayAdaptor->nImages = nElems(NEOVideoImages);
    overlayAdaptor->pImages = NEOVideoImages;

    overlayAdaptor->PutVideo = NEOPutVideo;
    overlayAdaptor->PutStill = NULL;
    overlayAdaptor->GetVideo = NULL;
    overlayAdaptor->GetStill = NULL;

    overlayAdaptor->StopVideo = NEOStopVideo;
    overlayAdaptor->SetPortAttribute = NEOSetPortAttribute;
    overlayAdaptor->GetPortAttribute = NEOGetPortAttribute;
    overlayAdaptor->QueryBestSize = NEOQueryBestSize;
    overlayAdaptor->PutImage = NEOPutImage;
    overlayAdaptor->QueryImageAttributes = NEOQueryImageAttributes;

    pPriv = (NEOPortPtr)overlayAdaptor->pPortPrivates[0].ptr;
    pPriv->colorKey = nPtr->videoKey;
    pPriv->interlace = nPtr->interlace;
    pPriv->videoStatus = 0;
    pPriv->brightness = 0;
    REGION_INIT(pScreen, &pPriv->clip, NullBox, 0);
    nPtr->overlayAdaptor = overlayAdaptor;

    xvBrightness = MAKE_ATOM("XV_BRIGHTNESS");
    xvColorKey = MAKE_ATOM("XV_COLORKEY");
    xvInterlace = MAKE_ATOM("XV_INTERLACE");
    
    NEOResetVideo(pScrn);

    return (overlayAdaptor);
}

void
NEOResetVideo(ScrnInfoPtr pScrn)
{
    NEOPtr nPtr = NEOPTR(pScrn);
    NEOPortPtr pPriv = (NEOPortPtr)nPtr->overlayAdaptor->pPortPrivates[0].ptr;
    int r, g, b;
    VGA_HWP(pScrn);

#ifdef DEBUG
    xf86DrvMsg(pScrn->scrnIndex,X_INFO,"NEOResetVideo\n");
#endif
    switch (pScrn->depth){
    case 8:
	OUTGR(0xc6, pPriv->colorKey & 0);
	OUTGR(0xc5, pPriv->colorKey & 0xff);
	OUTGR(0xc7, pPriv->colorKey & 0);
	break;
    default:
	r = (pPriv->colorKey & pScrn->mask.red) >> pScrn->offset.red;
	g = (pPriv->colorKey & pScrn->mask.green) >> pScrn->offset.green;
	b = (pPriv->colorKey & pScrn->mask.blue) >> pScrn->offset.blue;
	OUTGR(0xc5, r);
	OUTGR(0xc6, g);
	OUTGR(0xc7, b);
	break;
    }
    OUTGR(0xc4, pPriv->brightness);
}

static int
NEOPutVideo(ScrnInfoPtr pScrn, 
	     short src_x, short src_y, short drw_x, short drw_y,
	     short src_w, short src_h, short drw_w, short drw_h,
	     RegionPtr clipBoxes, pointer data)
{
    NEOPortPtr pPriv = (NEOPortPtr)data;
    NEOPtr nPtr = NEOPTR(pScrn);
    CARD32 src_pitch, offset;
    int xscale, yscale;
    BoxRec dstBox;
    INT32 x1, y1, x2, y2;
    int size, bpp;
    unsigned char capctrl;
    VGA_HWP(pScrn);

#ifdef DEBUG
    xf86DrvMsg(pScrn->scrnIndex,X_INFO,"NEOPutVideo: src: %d %d %d %d\n", 
	       src_x, src_y, src_w, src_h);
    xf86DrvMsg(pScrn->scrnIndex,X_INFO,"NEOPutVideo: drw: %d %d %d %d\n", 
	       drw_x, drw_y, drw_w, drw_h);
#endif
    if (src_w > 720)
	src_w = 720;
    if (src_h > 576)
        src_h = 576;
    if (pPriv->interlace != 2)
	src_h /= 2;
    x1 = src_x;
    y1 = src_y;
    x2 = src_x + src_w;
    y2 = src_y + src_h;

    dstBox.x1 = drw_x;
    dstBox.y1 = drw_y;
    dstBox.x2 = drw_x + drw_w;
    dstBox.y2 = drw_y + drw_h;

    if (!xf86XVClipVideoHelper(&dstBox, &x1, &x2, &y1, &y2,
			       clipBoxes, src_w, src_h)){
	return(Success);
    }
#ifdef DEBUG
    xf86DrvMsg(pScrn->scrnIndex,X_INFO,"NEOPutVideo: %d %d %d %d\n", 
	       x1, y1, x2, y2);
#endif

    dstBox.x1 -= pScrn->frameX0;
    dstBox.y1 -= pScrn->frameY0;
    dstBox.x2 -= pScrn->frameX0;
    dstBox.y2 -= pScrn->frameY0;
#ifdef DEBUG
    xf86DrvMsg(pScrn->scrnIndex,X_INFO,"NEOPutVideo: dstBox %d %d %d %d\n", 
	       dstBox.x1, dstBox.y1, dstBox.x2, dstBox.y2);
#endif

    bpp = (pScrn->bitsPerPixel + 1) >> 3;
    src_pitch = (src_w + 7) & ~7;

    xscale = 0x1000;
    if (src_w <= drw_w){
	xscale = (src_w * 0x1000 / drw_w) & 0xffff;
    }

    yscale = 0x1000;
    if (src_h <= drw_h){
 	yscale = (src_h * 0x1000 / drw_h) & 0xffff;
    }

    size = src_h * src_pitch * 2;

    if (size > nPtr->overlay){
	if ((pPriv->linear = NEOAllocateMemory(pScrn, pPriv->linear, size)) 
	    == NULL){
	    return (BadAlloc);
	}
    } else {
	pPriv->linear = NULL;
    }
    
    if (pPriv->linear == NULL){
	offset = nPtr->overlay_offset;
    } else {
	offset = pPriv->linear->offset * bpp;
    }
    
#ifdef DEBUG
    xf86DrvMsg(pScrn->scrnIndex,X_INFO,"NEOPutVideo: offset=0x%x\n", offset);
#endif
     WAIT_ENGINE_IDLE();
     memset(nPtr->NeoFbBase + offset, 0, size);

    if (!RegionsEqual(&pPriv->clip, clipBoxes)){
	REGION_COPY(pScreen, &pPriv->clip, clipBoxes);
	xf86XVFillKeyHelper(pScrn->pScreen, pPriv->colorKey,
			    clipBoxes);
    }

    x1 >>= 16;
    y1 >>= 16;
    x2 >>= 16;
    y2 >>= 16;

    switch (nPtr->NeoChipset) {
    default:
    case NM2160: 
	offset/=2;
	OUTGR(0xbc, 0x4f);
 	break;
    case NM2200:
    case NM2230:
    case NM2360:
    case NM2380:
	OUTGR(0xbc, 0x2e);
 	break;
    }
 

    OUTGR(0xb1, (((dstBox.x2-1) >> 4) & 0xf0) | ((dstBox.x1 >> 8) & 0x0f));
    OUTGR(0xb2, dstBox.x1);
    OUTGR(0xb3, dstBox.x2 - 1);
    OUTGR(0xb4, (((dstBox.y2 - 1) >> 4) & 0xf0) | ((dstBox.y1 >> 8) & 0x0f));
    OUTGR(0xb5, dstBox.y1);
    OUTGR(0xb6, dstBox.y2 - 1);
    OUTGR(0xb7, offset >> 16);
    OUTGR(0xb8, offset >> 8);
    OUTGR(0xb9, offset );
    OUTGR(0xba, src_pitch >> 8);
    OUTGR(0xbb, src_pitch);

    OUTGR(0xc0, xscale >> 8);
    OUTGR(0xc1, xscale);
    OUTGR(0xc2, yscale >> 8);
    OUTGR(0xc3, yscale);
    OUTGR(0xbf, 0x02);

    OUTGR(0x0a, 0x21);

    OUTSR(0x0c, offset );
    OUTSR(0x0d, offset >> 8);
    OUTSR(0x0e, offset >> 16);
    OUTSR(0x1a, src_pitch);
    OUTSR(0x1b, src_pitch>>8);

    OUTSR(0x17, 0 + x1);
    OUTSR(0x18, 0 + x2 -1);
    OUTSR(0x19, (((0 + x2 - 1) >> 4) & 0xf0) | (((0 + x1) >> 8) & 0x0f));

    OUTSR(0x14, 14 + y1);
    OUTSR(0x15, 14 + y2 - 2);
    OUTSR(0x16, (((14 + y2 - 1) >> 4) & 0xf0) | (((14 + y1) >> 8) & 0x0f));

    OUTSR(0x1c, 0xfb);
    OUTSR(0x1d, 0x00);
    OUTSR(0x1e, 0xe2);
    OUTSR(0x1f, 0x02);

    OUTSR(0x09, 0x11);
    OUTSR(0x0a, 0x00);

    capctrl = 0x21;
    switch (pPriv->interlace){
    case 0: /* Combine 2 fields */
	break;
    case 1: /* one field only */
	capctrl |= 0x80;
	break;
    case 2: /* Interlaced fields */
	capctrl |= 0x40;
	break;
    }
    OUTSR(0x08, capctrl);

#if 0
    OUTGR(0x0a, 0x01);
#endif
    OUTGR(0xb0, 0x03);

    pPriv->videoStatus = CLIENT_VIDEO_ON;
    return (Success);
}

static void
NEOStopVideo(ScrnInfoPtr pScrn, pointer data, Bool exit)
{
    NEOPortPtr pPriv = (NEOPortPtr)data;
    NEOPtr nPtr = NEOPTR(pScrn);
    VGA_HWP(pScrn);

#ifdef DEBUG
    xf86DrvMsg(pScrn->scrnIndex,X_INFO,"NEOStopVideo\n");
#endif
    REGION_EMPTY(pScrn->pScreen, &pPriv->clip);

    if (exit){
	if (pPriv->videoStatus & CLIENT_VIDEO_ON){
#ifdef DEBUG
            xf86DrvMsg(pScrn->scrnIndex,X_INFO,"NEOStopVideo: stop capture\n");
#endif
	    OUTGR(0xb0, 0x02);
	    OUTGR(0x0a, 0x21);
	    OUTSR(0x08, 0xa0);
#if 0
	    OUTGR(0x0a, 0x01);
#endif
	}
	if (pPriv->linear != NULL){
	    xf86FreeOffscreenLinear(pPriv->linear);
	    pPriv->linear = NULL;
	}
	pPriv->videoStatus = 0;
    } else {
	if (pPriv->videoStatus & CLIENT_VIDEO_ON){
	    OUTGR(0xb0, 0x02);
	    OUTGR(0x0a, 0x21);
	    OUTSR(0x08, 0xa0);
#if 0
	    OUTGR(0x0a, 0x01);
#endif
	    pPriv->videoStatus |= OFF_TIMER;
	    pPriv->offTime = currentTime.milliseconds + OFF_DELAY;
	}
    }
}

static int
NEOSetPortAttribute(ScrnInfoPtr pScrn, Atom attribute, INT32 value, 
		    pointer data)
{
    NEOPortPtr pPriv = (NEOPortPtr)data;
    NEOPtr nPtr = NEOPTR(pScrn);
    VGA_HWP(pScrn);

#ifdef DEBUG
    xf86DrvMsg(pScrn->scrnIndex,X_INFO,"NEOSetPortAttribute\n");
#endif
    if (attribute == xvColorKey){
	int r, g, b;

	pPriv->colorKey = value;
	switch (pScrn->depth){
	case 8:
	    OUTGR(0xc6, pPriv->colorKey & 0xff);
	    OUTGR(0xc5, 0x00);
	    OUTGR(0xc7, 0x00);
	    break;
	default:
	    r = (pPriv->colorKey & pScrn->mask.red) >> pScrn->offset.red;
	    g = (pPriv->colorKey & pScrn->mask.green) >> pScrn->offset.green;
	    b = (pPriv->colorKey & pScrn->mask.blue) >> pScrn->offset.blue;
	    OUTGR(0xc5, r);
	    OUTGR(0xc6, g);
	    OUTGR(0xc7, b);
	}
    } else if (attribute == xvBrightness){
	if ((value < -128) || (value > 127)){
	    return (BadValue);
	}
	pPriv->brightness = value;
	OUTGR(0xc4, value);
    } else if (attribute == xvInterlace){
	if (value < 0  ||  value > 2){
	    return (BadValue);
	}
	pPriv->interlace = value;
    } else {
	return (BadMatch);
    }
    return (Success);
}

static int
NEOGetPortAttribute(ScrnInfoPtr pScrn, Atom attribute, INT32 *value, 
		    pointer data)
{
    NEOPortPtr pPriv = (NEOPortPtr)data;

#ifdef DEBUG
    xf86DrvMsg(pScrn->scrnIndex,X_INFO,"NEOGetPortAttribute\n");
#endif
    if (attribute == xvColorKey){
	*value = pPriv->colorKey;
    } else if (attribute == xvBrightness){
	*value = pPriv->brightness;
    } else if (attribute == xvInterlace){
	*value = pPriv->interlace;
    } else {
	return (BadMatch);
    }
    return (Success);
}

static void
NEOQueryBestSize(ScrnInfoPtr pScrn, Bool motion, 
		 short vid_w, short vid_h, short drw_w, short drw_h,
		 unsigned int *p_w, unsigned int *p_h,
		 pointer data)
{
#ifdef DEBUG
    xf86DrvMsg(pScrn->scrnIndex,X_INFO,"NEOQueryBestSize\n");
#endif
    *p_w = min(drw_w, 1024);
    *p_h = min(drw_h, 1024);
}

static int
NEOPutImage(ScrnInfoPtr pScrn, 
	    short src_x, short src_y, short drw_x, short drw_y,
	    short src_w, short src_h, short drw_w, short drw_h,
	    int id, unsigned char *buf, short width, short height,
	    Bool sync, RegionPtr clipBoxes, pointer data)
{
    NEOPtr nPtr = NEOPTR(pScrn);
    NEOPortPtr pPriv = (NEOPortPtr)nPtr->overlayAdaptor->pPortPrivates[0].ptr;
    INT32 x1, y1, x2, y2;
    int bpp;
    int srcPitch, srcPitch2 = 0, dstPitch, size;
    BoxRec dstBox;
    CARD32 offset, offset2 = 0, offset3 = 0, tmp;
    int left, top, nPixels, nLines;
    unsigned char *dstStart;

#ifdef DEBUG
    xf86DrvMsg(pScrn->scrnIndex,X_INFO,"NEOPutImage\n");
#endif

    x1 = src_x;
    y1 = src_y;
    x2 = src_x + src_w;
    y2 = src_y + src_h;
    
    dstBox.x1 = drw_x;
    dstBox.y1 = drw_y;
    dstBox.x2 = drw_x + drw_w;
    dstBox.y2 = drw_y + drw_h;

    if (!xf86XVClipVideoHelper(&dstBox, &x1, &x2, &y1, &y2,
			       clipBoxes, width, height)){
	return (Success);
    }

    dstBox.x1 -= pScrn->frameX0;
    dstBox.y1 -= pScrn->frameY0;
    dstBox.x2 -= pScrn->frameX0;
    dstBox.y2 -= pScrn->frameY0;    
    
    bpp = ((pScrn->bitsPerPixel + 1) >> 3);
    
    switch (id){
    case FOURCC_YV12:
	srcPitch  = (width + 3) & ~3;
	offset2   = srcPitch * height;
	srcPitch2 = ((width >> 1) + 3) & ~3;
	offset3   = offset2 + (srcPitch2 * (height >> 1));
  	dstPitch  = ((width << 1) + 15) & ~15; 
	break;
    case FOURCC_I420:
	srcPitch  = (width + 3) & ~3;
	offset3   = srcPitch * height;
	srcPitch2 = ((width >> 1) + 3) & ~3;
	offset2   = offset3 + (srcPitch2 * (height >> 1));
  	dstPitch  = ((width << 1) + 15) & ~15;
	break;
    case FOURCC_YUY2:
    case FOURCC_RV15:
    case FOURCC_RV16:
    default:
	srcPitch = width << 1;
  	dstPitch = (srcPitch + 15) & ~15;
	break;
    }
    
    size = dstPitch * height;
    if (size > nPtr->overlay){
	if ((pPriv->linear = NEOAllocateMemory(pScrn, pPriv->linear, size)) 
	    == NULL){
	    return (BadAlloc);
	}
    } else {
	pPriv->linear = NULL;
    }

    top = y1 >> 16;
    left = (x1 >> 16) & ~1;
    nPixels = ((((x2 + 0xFFFF) >> 16) + 1) & ~1) - left;
    left <<= 1;

    if (pPriv->linear == NULL){
	offset = nPtr->overlay_offset;
    } else {
	offset =  pPriv->linear->offset * bpp;
    }
    
#ifdef DEBUG
    xf86DrvMsg(pScrn->scrnIndex,X_INFO,"offset=%x\n", offset);
#endif

    dstStart = (unsigned char *)(nPtr->NeoFbBase + offset + left);
    
    switch (id){
    case FOURCC_YV12:
    case FOURCC_I420:
	top &= ~1;
	tmp = ((top >> 1) * srcPitch2) + (left >> 2);
	offset2 += tmp;
	offset3 += tmp;
	nLines = ((((y2 + 0xFFFF) >> 16) + 1) & ~1) - top;
	NEOCopyYV12Data(buf + (top * srcPitch) + (left >> 1), buf + offset2, 
			buf + offset3, dstStart, srcPitch, srcPitch2, 
			dstPitch, nLines, nPixels);
	break;
    default:
	buf += (top * srcPitch) + left;
	nLines = ((y2 + 0xFFFF) >> 16) - top;
	NEOCopyData(buf, dstStart, srcPitch, dstPitch, nLines, nPixels << 1);
    }

    if (!RegionsEqual(&pPriv->clip, clipBoxes)){
	REGION_COPY(pScreen, &pPriv->clip, clipBoxes);
        xf86XVFillKeyHelper(pScrn->pScreen, pPriv->colorKey, clipBoxes);
    }
	NEODisplayVideo(pScrn, id, offset, width, height, dstPitch, x1, y1,
			x2, y2,	&dstBox, src_w, src_h, drw_w, drw_h);
    
    pPriv->videoStatus = CLIENT_VIDEO_ON;
    return (Success);
	
}

static int
NEOQueryImageAttributes(ScrnInfoPtr pScrn, int id, 
			unsigned short *width, unsigned short *height,
			int *pitches, int *offsets)
{
    int size, tmp;

#ifdef DEBUG
    xf86DrvMsg(pScrn->scrnIndex,X_INFO,"NEOQueryImageAttributes\n");
#endif
    if (*width > 1024){
	*width = 1024;
    }
    if (*height > 1024){
	*height = 1024;
    }

    *width = (*width + 1) & ~1;
    if (offsets != NULL){
	offsets[0] = 0;
    }

    switch (id){
    case FOURCC_YV12:
    case FOURCC_I420:
	*height = (*height + 1) & ~1;
	size = (*width + 3) & ~3;
	if (pitches != NULL){
	    pitches[0] = size;
	}
	size *= *height;
	if (offsets != NULL){
	    offsets[1] = size;
	}
	tmp = ((*width >> 1) + 3) & ~3;
	if (pitches != NULL){
	    pitches[1] = pitches[2] = tmp;
	}
	tmp *= (*height >> 1);
	size += tmp;
	if (offsets != NULL){
	    offsets[2] = size;
	}
	size += tmp;
	break;
    case FOURCC_YUY2:
    case FOURCC_RV15:
    case FOURCC_RV16:
    default:
	size = *width * 2;
	if (pitches != NULL){
	    pitches[0] = size;
	}
	size *= *height;
	break;
    }
    return (size);
}

static Bool
RegionsEqual(RegionPtr A, RegionPtr B)
{
    int *dataA, *dataB;
    int num;

    num = REGION_NUM_RECTS(A);
    if (num != REGION_NUM_RECTS(B)){
	return (FALSE);
    }

    if ((A->extents.x1 != B->extents.x1)
	|| (A->extents.y1 != B->extents.y1)
	|| (A->extents.x2 != B->extents.x2)
	|| (A->extents.y2 != B->extents.y2)){
	return (FALSE);
    }

    dataA = (int*) REGION_RECTS(A);
    dataB = (int*) REGION_RECTS(B);

    while (num--){
	if ((dataA[0] != dataB[0]) || (dataA[1] != dataB[1])){
	    return (FALSE);
	}
	dataA += 2;
	dataB += 2;
    }
    return (TRUE);
}

static void
NEODisplayVideo(ScrnInfoPtr pScrn, int id, int offset, 
		 short width, short height, int pitch,
		 int x1, int y1, int x2, int y2, BoxPtr dstBox,
		 short src_w, short src_h, short drw_w, short drw_h)
{
    NEOPtr nPtr = NEOPTR(pScrn);
    int hstretch, vstretch, fmt;
    VGA_HWP(pScrn);
#ifdef DEBUG
    xf86DrvMsg(pScrn->scrnIndex,X_INFO,"NEODisplayVideo\n");
    xf86DrvMsg(pScrn->scrnIndex,X_INFO,"NEODisplayVideo src_w=%d, src_h=%d, pitch=%d, drw_w=%d, drw_h=%d\n", src_w, src_h, pitch, drw_w, drw_h);
#endif
#define WIDTH_THRESHOLD 160
    if (dstBox->x2 >= pScrn->virtualX) {
	/*
	 * This is a hack to work around a problem when video is moved
	 * accross the right border.
	 */
	int diff_s = (width - ((x2 - x1) >> 16)) & ~1;
	int diff_d = (drw_w - dstBox->x2 + dstBox->x1) & ~1;

	offset -= 2 * ((diff_s > diff_d) ? diff_d : diff_s);
	dstBox->x1 -= diff_d;
    } else if (dstBox->x2 - dstBox->x1 < WIDTH_THRESHOLD) {
	/*
	 * When the video window is less than about 160 pixel wide
	 * it will be distoreted. We attempt to fix it by actually
	 * making it wider and relying on the color key to prevent
	 * it from appearing outside of the video.
	 */
	int pre, post;
	int scale = 1;
	
	if (dstBox->x1 < WIDTH_THRESHOLD) {
	    pre = dstBox->x1;
	    post = 160 - pre;
	} else {
	    pre = 160;
	    post = 0;
	}
	offset -= 2 * scale * pre;
	dstBox->x1 -= pre;
	dstBox->x2 += post;
    }
    if (nPtr->videoHZoom != 1.0) {
	if ((dstBox->x2 += 5) > pScrn->virtualX)
	    dstBox->x2 = pScrn->virtualX;
	if (dstBox->x1 > 0) dstBox->x1 += 2;
    }
    
    fmt = 0x00;
    switch (id){
    case FOURCC_YV12:
    case FOURCC_I420:
    case FOURCC_YUY2:
	fmt = 0x00;
	break;
    case FOURCC_RV15:
    case FOURCC_RV16:
	fmt = 0x20;
	break;
    }

    offset += (x1 >> 15) & ~0x03;
    
    switch (nPtr->NeoChipset) {
    default:
    case NM2160: 
        offset/=2;
	pitch/=2;
        OUTGR(0xbc, 0x4f);
	break;
    case NM2200:
    case NM2230:
    case NM2360:
    case NM2380:
        OUTGR(0xbc, 0x2e);
	break;
    }

    /* factor 4 for granularity */
    hstretch = (double)0x1000 * 4 / (int)(nPtr->videoHZoom * 4);
    if (drw_w > src_w)
	hstretch = (((int)src_w) * hstretch) / (int) drw_w;
    
    vstretch = (double)0x1000 / nPtr->videoVZoom;
    if (drw_h > src_h)
	vstretch = (((int)src_h) * vstretch )/ (int) drw_h;

    OUTGR(0xb1, (((dstBox->x2 - 1) >> 4) & 0xf0) | ((dstBox->x1 >> 8) & 0x0f));
    OUTGR(0xb2, dstBox->x1);
    OUTGR(0xb3, dstBox->x2 - 1);
    OUTGR(0xb4, (((dstBox->y2 - 1) >> 4) & 0xf0) | ((dstBox->y1 >> 8) & 0x0f));
    OUTGR(0xb5, dstBox->y1);
    OUTGR(0xb6, dstBox->y2 - 1);
    OUTGR(0xb7, offset >> 16);
    OUTGR(0xb8, offset >> 8);
    OUTGR(0xb9, offset );
    OUTGR(0xba, pitch >> 8);
    OUTGR(0xbb, pitch);
     
    OUTGR(0xbd, 0x02);
    OUTGR(0xbe, 0x00);
    OUTGR(0xbf, 0x02);

    OUTGR(0xc0, hstretch >> 8);
    OUTGR(0xc1, hstretch);
    OUTGR(0xc2, vstretch >> 8);
    OUTGR(0xc3, vstretch);

    OUTGR(0xb0, fmt | 0x03);

    OUTGR(0x0a, 0x21);
    OUTSR(0x08, 0xa0);
    OUTGR(0x0a, 0x01);
}

static void
NEOInitOffscreenImages(ScreenPtr pScreen)
{
    XF86OffscreenImagePtr offscreenImages;

#ifdef DEBUG
    xf86DrvMsg(xf86Screens[pScreen->myNum]->scrnIndex,X_INFO,"NEOInitOffscreenImages\n");
#endif
    if ((offscreenImages = xalloc(sizeof(XF86OffscreenImageRec))) == NULL){
	return;
    }

    offscreenImages->image = NEOVideoImages;
    offscreenImages->flags = VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT;
    offscreenImages->alloc_surface = NEOAllocSurface;
    offscreenImages->free_surface = NEOFreeSurface;
    offscreenImages->display = NEODisplaySurface;
    offscreenImages->stop = NEOStopSurface;
    offscreenImages->getAttribute = NEOGetSurfaceAttribute;
    offscreenImages->setAttribute = NEOSetSurfaceAttribute;
    offscreenImages->max_width = 1024;
    offscreenImages->max_height = 1024;
    offscreenImages->num_attributes = nElems(NEOVideoAttributes);
    offscreenImages->attributes = NEOVideoAttributes;

    xf86XVRegisterOffscreenImages(pScreen, offscreenImages, 1);
}

static FBLinearPtr
NEOAllocateMemory(ScrnInfoPtr pScrn, FBLinearPtr linear, int size)
{
    ScreenPtr pScreen;
    FBLinearPtr new_linear;
    int bytespp = pScrn->bitsPerPixel >> 3;

    /* convert size in bytes into number of pixels */
    size = (size + bytespp - 1) / bytespp;
#ifdef DEBUG
    xf86DrvMsg(pScrn->scrnIndex,X_INFO,
	       "NEOAllocateMemory: linear=%x, size=%d\n", linear, size);
#endif
    if (linear){
#ifdef DEBUG
        xf86DrvMsg(pScrn->scrnIndex,X_INFO,
		   "NEOAllocateMemory: linear->size=%d\n", linear->size);
#endif
	if (linear->size >= size){
	    return (linear);
	}

	if (xf86ResizeOffscreenLinear(linear, size)){
	    return (linear);
	}

	xf86FreeOffscreenLinear(linear);
    }


    pScreen = screenInfo.screens[pScrn->scrnIndex];
    if ((new_linear = xf86AllocateOffscreenLinear(pScreen, size, 16, NULL,
						  NULL, NULL)) == NULL){
	int max_size;

	xf86QueryLargestOffscreenLinear(pScreen, &max_size, 16, 
					PRIORITY_EXTREME);
#ifdef DEBUG
        xf86DrvMsg(pScrn->scrnIndex,X_INFO,
		   "NEOAllocateMemory: max_size=%d\n", max_size);
#endif
	if (max_size < size){
	    return (NULL);
	}

	xf86PurgeUnlockedOffscreenAreas(pScreen);
	new_linear = xf86AllocateOffscreenLinear(pScreen, 
						 size, 16, NULL, NULL, NULL);
    }
    
    return (new_linear);
}

static void
NEOCopyData(unsigned char *src, unsigned char *dst, 
	    int srcPitch, int dstPitch,
	    int height, int width)
{
    while (height-- > 0){
	memcpy(dst, src, width);
	src += srcPitch;
	dst += dstPitch;
    }
}

static void
NEOCopyYV12Data(unsigned char *src1, unsigned char *src2,
		unsigned char *src3, unsigned char *dst,
		int srcPitch1, int srcPitch2, int dstPitch,
		int height, int width)
{
    CARD32 *pDst = (CARD32 *) dst;
    int i;

    width >>= 1;
    height >>= 1;
    dstPitch >>= 2;
    while (--height >= 0){
	for (i =0; i < width; i++){
	    pDst[i] = src1[i << 1] | (src1[(i << 1) + 1] << 16) |
		(src3[i] << 8) | (src2[i] << 24);
	}
	pDst += dstPitch;
	src1 += srcPitch1;

	for (i =0; i < width; i++){
	    pDst[i] = src1[i << 1] | (src1[(i << 1) + 1] << 16) |
		(src3[i] << 8) | (src2[i] << 24);
	}
	pDst += dstPitch;
	src1 += srcPitch1;
	    src2 += srcPitch2;
	    src3 += srcPitch2;
    }
}

static int
NEOAllocSurface(ScrnInfoPtr pScrn, int id, 
		unsigned short width, unsigned short height,
		XF86SurfacePtr surface)
{
    int pitch, bpp, size;
    NEOOffscreenPtr pPriv;
    FBLinearPtr linear;

#ifdef DEBUG
    xf86DrvMsg(pScrn->scrnIndex,X_INFO,"NEOAllocSurface\n");
#endif
    if (width > 1024  || height > 1024){
	return (BadAlloc);
    }

    width = (width + 1) & ~1;
    bpp = ((pScrn->bitsPerPixel + 1) >> 3);
    pitch = ((width << 1) + 15) & ~15;
    size = pitch * height;

    if ((linear = NEOAllocateMemory(pScrn, NULL, size)) == NULL){
	return (BadAlloc);
    }

    surface->width = width;
    surface->height = height;
    if ((surface->pitches = xalloc(sizeof(int))) == NULL){
	xf86FreeOffscreenLinear(linear);
	return (BadAlloc);
    }
    if ((surface->offsets = xalloc(sizeof(int))) == NULL){
	xfree(surface->pitches);
	xf86FreeOffscreenLinear(linear);
	return (BadAlloc);
    }

    if ((pPriv = xalloc(sizeof(NEOOffscreenRec))) == NULL){
	xfree(surface->pitches);
	xfree(surface->offsets);
	xf86FreeOffscreenLinear(linear);
	return (BadAlloc);
    }

    pPriv->linear = linear;
    pPriv->isOn = FALSE;

    surface->pScrn = pScrn;
    surface->id = id;
    surface->pitches[0] = pitch;
    surface->offsets[0] = linear->offset << 1;
    surface->devPrivate.ptr = (pointer)pPriv;
    return (Success);
}

static int
NEOFreeSurface(XF86SurfacePtr surface)
{
    NEOOffscreenPtr pPriv = (NEOOffscreenPtr)surface->devPrivate.ptr;

#ifdef DEBUG
    xf86DrvMsg(0,X_INFO,"NEOFreeSurface\n");
#endif
    if (pPriv->isOn)
	NEOStopSurface(surface);

    xf86FreeOffscreenLinear(pPriv->linear);
    xfree(surface->pitches);
    xfree(surface->offsets);
    xfree(surface->devPrivate.ptr);
    return (Success);
}

static int
NEODisplaySurface(XF86SurfacePtr surface,
		  short src_x, short src_y, short drw_x, short drw_y,
		  short src_w, short src_h, short drw_w, short drw_h,
		  RegionPtr clipBoxes)
{
    NEOOffscreenPtr pPriv = (NEOOffscreenPtr)surface->devPrivate.ptr;
    NEOPtr nPtr = NEOPTR(surface->pScrn);
    NEOPortPtr portPriv = nPtr->overlayAdaptor->pPortPrivates[0].ptr;
    INT32 x1, y1, x2, y2;
    BoxRec dstBox;

#ifdef DEBUG
    xf86DrvMsg(surface->pScrn->scrnIndex,X_INFO,"NEODisplaySurface\n");
#endif
    x1 = src_x;
    x2 = src_x + src_w;
    y1 = src_y;
    y2 = src_y + src_h;

    dstBox.x1 = drw_x;
    dstBox.x2 = drw_x + drw_w;
    dstBox.y1 = drw_y;
    dstBox.y2 = drw_y + drw_h;
    if (!xf86XVClipVideoHelper( &dstBox, &x1, &x2, &y1, &y2,
			       clipBoxes, surface->width, surface->height)){
	return (Success);
    }

    dstBox.x1 -= surface->pScrn->frameX0;
    dstBox.y1 -= surface->pScrn->frameY0;
    dstBox.x2 -= surface->pScrn->frameX0;
    dstBox.y2 -= surface->pScrn->frameY0;

    xf86XVFillKeyHelper(surface->pScrn->pScreen, portPriv->colorKey,
			clipBoxes);
    NEOResetVideo(surface->pScrn);
    NEODisplayVideo(surface->pScrn, surface->id, surface->offsets[0],
		    surface->width, surface->height, surface->pitches[0], 
		    x1, y1, x2, y2, &dstBox, src_w, src_h, drw_w, drw_h);
    
    pPriv->isOn = TRUE;
    if (portPriv->videoStatus & CLIENT_VIDEO_ON){
	REGION_EMPTY(pScrn->pScreen, &portPriv->clip);
	UpdateCurrentTime();
	portPriv->videoStatus = FREE_TIMER;
	portPriv->freeTime = currentTime.milliseconds + FREE_DELAY;
    }
    return (Success);
}

static int
NEOStopSurface(XF86SurfacePtr surface)
{
    NEOOffscreenPtr pPriv = (NEOOffscreenPtr)surface->devPrivate.ptr;

#ifdef DEBUG
    xf86DrvMsg(surface->pScrn->scrnIndex,X_INFO,"NEOStopSurface\n");
#endif
    if (pPriv->isOn){
	NEOPtr nPtr = NEOPTR(surface->pScrn);
	VGA_HWP(surface->pScrn);
	OUTGR(0xb0, 0x02);
	pPriv->isOn = FALSE;
    }
    return (Success);
}

static int
NEOGetSurfaceAttribute(ScrnInfoPtr pScrn, Atom attr, INT32 *value)
{
    NEOPtr nPtr = NEOPTR(pScrn);

#ifdef DEBUG
    xf86DrvMsg(pScrn->scrnIndex,X_INFO,"NEOGetSurfaceAttribute\n");
#endif
    return (NEOGetPortAttribute(pScrn, 
            attr, value, (pointer)nPtr->overlayAdaptor->pPortPrivates[0].ptr));
}

static int
NEOSetSurfaceAttribute(ScrnInfoPtr pScrn, Atom attr, INT32 value)
{
    NEOPtr nPtr = NEOPTR(pScrn);

#ifdef DEBUG
    xf86DrvMsg(pScrn->scrnIndex,X_INFO,"NEOSetSurfaceAttribute\n");
#endif
    return (NEOSetPortAttribute(pScrn, 
            attr, value, (pointer)nPtr->overlayAdaptor->pPortPrivates[0].ptr));
}

#else /* XvExtension */

void NEOInitVideo(ScreenPtr pScreen) {}
void NEOResetVideo(ScreenPtr pScreen) {}

#endif
