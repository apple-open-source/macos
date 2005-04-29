/*
 * Copyright 2001 by Alan Hourihane, Sychdyn, North Wales.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Alan Hourihane not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Alan Hourihane makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * ALAN HOURIHANE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL ALAN HOURIHANE BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors: Alan Hourihane, alanh@fairlite.demon.co.uk
 *          Sven Luther <luther@dpt-info.u-strasbg.fr>
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/glint/pm3_video.c,v 1.15 2003/11/10 18:22:21 tsi Exp $ */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86Resources.h"
#include "xf86_ansic.h"
#include "compiler.h"
#include "xf86PciInfo.h"
#include "xf86Pci.h"
#include "xf86fbman.h"
#include "regionstr.h"

#include "glint.h"
#include "glint_regs.h"
#include "pm3_regs.h"
#include "Xv.h"
#include "xaa.h"
#include "xaalocal.h"
#include "dixstruct.h"
#include "fourcc.h"

#define OFF_DELAY 	200  /* milliseconds */
#define FREE_DELAY 	60000

#define OFF_TIMER 	0x01
#define FREE_TIMER	0x02
#define CLIENT_VIDEO_ON	0x04

#define TIMER_MASK      (OFF_TIMER | FREE_TIMER)

static XF86VideoAdaptorPtr Permedia3SetupImageVideo(ScreenPtr);
static void Permedia3InitOffscreenImages(ScreenPtr);
static void Permedia3StopVideo(ScrnInfoPtr, pointer, Bool);
static int Permedia3SetPortAttribute(ScrnInfoPtr, Atom, INT32, pointer);
static int Permedia3GetPortAttribute(ScrnInfoPtr, Atom ,INT32 *, pointer);
static void Permedia3QueryBestSize(ScrnInfoPtr, Bool,
	short, short, short, short, unsigned int *, unsigned int *, pointer);
static int Permedia3PutImage( ScrnInfoPtr, 
	short, short, short, short, short, short, short, short,
	int, unsigned char*, short, short, Bool, RegionPtr, pointer);
static int Permedia3QueryImageAttributes(ScrnInfoPtr, 
	int, unsigned short *, unsigned short *,  int *, int *);
static void Permedia3VideoTimerCallback(ScrnInfoPtr pScrn, Time time);

#define MAKE_ATOM(a) MakeAtom(a, sizeof(a) - 1, TRUE)

static Atom xvColorKey, xvDoubleBuffer, xvAutopaintColorKey, xvFilter;

void Permedia3InitVideo(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    XF86VideoAdaptorPtr *adaptors, *newAdaptors = NULL;
    XF86VideoAdaptorPtr newAdaptor = NULL;
    GLINTPtr pGlint = GLINTPTR(pScrn);
    int num_adaptors;
	
    /* Because of bugs in the PM3 when uploading images via the
     * bypass to the framebuffer, we always have to use the accelerator.
     */
    if (pGlint->NoAccel)
	return;

    newAdaptor = Permedia3SetupImageVideo(pScreen);
    Permedia3InitOffscreenImages(pScreen);

    num_adaptors = xf86XVListGenericAdaptors(pScrn, &adaptors);

    if(newAdaptor) {
	if(!num_adaptors) {
	    num_adaptors = 1;
	    adaptors = &newAdaptor;
	} else {
	    newAdaptors =  /* need to free this someplace */
		xalloc((num_adaptors + 1) * sizeof(XF86VideoAdaptorPtr*));
	    if(newAdaptors) {
		memcpy(newAdaptors, adaptors, num_adaptors * 
					sizeof(XF86VideoAdaptorPtr));
		newAdaptors[num_adaptors] = newAdaptor;
		adaptors = newAdaptors;
		num_adaptors++;
	    }
	}
    }

    if(num_adaptors)
        xf86XVScreenInit(pScreen, adaptors, num_adaptors);

    if(newAdaptors)
	xfree(newAdaptors);
}

/* client libraries expect an encoding */
static XF86VideoEncodingRec DummyEncoding[1] =
{
 {
   0,
   "XV_IMAGE",
   2047, 2047,
   {1, 1}
 }
};

#define NUM_FORMATS 4

static XF86VideoFormatRec Formats[NUM_FORMATS] = 
{
  {8, PseudoColor},  {15, TrueColor}, {16, TrueColor}, {24, TrueColor}
};

#define NUM_ATTRIBUTES 4 

static XF86AttributeRec Attributes[NUM_ATTRIBUTES] =
{
   {XvSettable | XvGettable, 0, 1, "XV_DOUBLE_BUFFER"},
   {XvSettable | XvGettable, 0, (1 << 24) - 1, "XV_COLORKEY"},
   {XvSettable | XvGettable, 0, 1, "XV_AUTOPAINT_COLORKEY"},
   {XvSettable | XvGettable, 0, 2, "XV_FILTER"},
};

/*
 *  FOURCC from http://www.webartz.com/fourcc
 *  Generic GUID for legacy FOURCC XXXXXXXX-0000-0010-8000-00AA00389B71
 */
#define LE4CC(a,b,c,d) (((CARD32)(a)&0xFF)|(((CARD32)(b)&0xFF)<<8)|(((CARD32)(c)&0xFF)<<16)|(((CARD32)(d)&0xFF)<<24))
#define GUID4CC(a,b,c,d) { a,b,c,d,0,0,0,0x10,0x80,0,0,0xAA,0,0x38,0x9B,0x71 }

#define NoOrder LSBFirst

#define NUM_IMAGES 15

static XF86ImageRec Images[NUM_IMAGES] =
{
    /* Planar YVU 4:2:0 (emulated) */
    { LE4CC('Y','V','1','2'), XvYUV, NoOrder, GUID4CC('Y','V','1','2'),
      12, XvPlanar, 3, 0, 0, 0, 0,
      8, 8, 8,  1, 2, 2,  1, 2, 2, "YVU", XvTopToBottom },

    /* Packed YUYV 4:2:2 */
    { LE4CC('Y','U','Y','2'), XvYUV, NoOrder, GUID4CC('Y','U','Y','2'),
      16, XvPacked, 1, 0, 0, 0, 0,
      8, 8, 8,  1, 2, 2,  1, 1, 1, "YUYV", XvTopToBottom },

    /* Packed UYVY 4:2:2 */
    { LE4CC('U','Y','V','Y'), XvYUV, NoOrder, GUID4CC('U','Y','V','Y'),
      16, XvPacked, 1, 0, 0, 0, 0,
      8, 8, 8,  1, 2, 2,  1, 1, 1, "UYVY", XvTopToBottom },

    /* Packed YUVA 4:4:4 */
    { LE4CC('Y','U','V','A') /* XXX not registered */, XvYUV, LSBFirst, { 0 },
      32, XvPacked, 1, 0, 0, 0, 0,
      8, 8, 8,  1, 1, 1,  1, 1, 1, "YUVA", XvTopToBottom },

    /* Packed VUYA 4:4:4 */
    { LE4CC('V','U','Y','A') /* XXX not registered */, XvYUV, LSBFirst, { 0 },
      32, XvPacked, 1, 0, 0, 0, 0,
      8, 8, 8,  1, 1, 1,  1, 1, 1, "VUYA", XvTopToBottom },

    /* RGBA 8:8:8:8 */
    { 0x41, XvRGB, LSBFirst, { 0 },
      32, XvPacked, 1, 24, 0x0000FF, 0x00FF00, 0xFF0000, 
      0, 0, 0,  0, 0, 0,  0, 0, 0, "RGBA", XvTopToBottom },

    /* RGB 5:6:5 */
    { 0x42, XvRGB, LSBFirst, { 0 },
      16, XvPacked, 1, 16, 0x001F, 0x07E0, 0xF800, 
      0, 0, 0,  0, 0, 0,  0, 0, 0, "RGB", XvTopToBottom },

    /* RGBA 5:5:5:1 */
    { 0x43, XvRGB, LSBFirst, { 0 },
      16, XvPacked, 1, 15, 0x001F, 0x03E0, 0x7C00, 
      0, 0, 0,  0, 0, 0,  0, 0, 0, "RGBA", XvTopToBottom },

    /* RGBA 4:4:4:4 */
    { 0x44, XvRGB, LSBFirst, { 0 },
      16, XvPacked, 1, 12, 0x000F, 0x00F0, 0x0F00, 
      0, 0, 0,  0, 0, 0,  0, 0, 0, "RGBA", XvTopToBottom },

    /* RGB 3:3:2 */
    { 0x46, XvRGB, NoOrder, { 0 },
      8, XvPacked, 1, 8, 0x07, 0x38, 0xC0, 
      0, 0, 0,  0, 0, 0,  0, 0, 0, "RGB", XvTopToBottom },

    /* BGRA 8:8:8:8 */
    { 0x47, XvRGB, LSBFirst, { 0 },
      32, XvPacked, 1, 24, 0xFF0000, 0x00FF00, 0x0000FF,
      0, 0, 0,  0, 0, 0,  0, 0, 0, "BGRA", XvTopToBottom },

    /* BGR 5:6:5 */
    { 0x48, XvRGB, LSBFirst, { 0 },
      16, XvPacked, 1, 16, 0xF800, 0x07E0, 0x001F,
      0, 0, 0,  0, 0, 0,  0, 0, 0, "BGR", XvTopToBottom },

    /* BGRA 5:5:5:1 */
    { 0x49, XvRGB, LSBFirst, { 0 },
      16, XvPacked, 1, 15, 0x7C00, 0x03E0, 0x001F,
      0, 0, 0,  0, 0, 0,  0, 0, 0, "BGRA", XvTopToBottom },

    /* BGRA 4:4:4:4 */
    { 0x4A, XvRGB, LSBFirst, { 0 },
      16, XvPacked, 1, 12, 0x0F00, 0x00F0, 0x000F,
      0, 0, 0,  0, 0, 0,  0, 0, 0, "BGRA", XvTopToBottom },

    /* BGR 2:3:3 */
    { 0x4C, XvRGB, NoOrder, { 0 },
      8, XvPacked, 1, 8, 0xC0, 0x38, 0x07,
      0, 0, 0,  0, 0, 0,  0, 0, 0, "BGR", XvTopToBottom },
};

#define MAX_BUFFERS 2

typedef struct {
   FBAreaPtr	area[MAX_BUFFERS];
   RegionRec	clip;
   CARD32	colorKey;
   CARD32	videoStatus;
   Time		offTime;
   Time		freeTime;
   int		Video_Shift;
   int		Format;
   Bool		ramdacOn;
   Bool		doubleBuffer;
   Bool		autopaintColorKey;
   int		Filter;
   int		sx, sy;
   int		offset[MAX_BUFFERS];
   int		buffer;
} GLINTPortPrivRec, *GLINTPortPrivPtr;

#define GET_PORT_PRIVATE(pScrn) \
   (GLINTPortPrivPtr)((GLINTPTR(pScrn))->adaptor->pPortPrivates[0].ptr)

#define	RAMDAC_WRITE(data,index)				\
do{                                                             \
	GLINT_WRITE_REG(((index)>>8)&0xff, PM3RD_IndexHigh);	\
 	GLINT_WRITE_REG((index)&0xff, PM3RD_IndexLow);		\
	GLINT_WRITE_REG(data, PM3RD_IndexedData);		\
}while(0)

void Permedia3ResetVideo(ScrnInfoPtr pScrn) 
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    GLINTPortPrivPtr pPriv = pGlint->adaptor->pPortPrivates[0].ptr;

    GLINT_WAIT(15);
    GLINT_WRITE_REG(0xfff0|(0xffff<<16), PM3VideoOverlayFifoControl);
    GLINT_WRITE_REG(PM3VideoOverlayMode_DISABLE, PM3VideoOverlayMode);
    pPriv->ramdacOn = FALSE;
    RAMDAC_WRITE(PM3RD_VideoOverlayControl_DISABLE, PM3RD_VideoOverlayControl);
    RAMDAC_WRITE((pPriv->colorKey&0xff0000)>>16, PM3RD_VideoOverlayKeyR);
    RAMDAC_WRITE((pPriv->colorKey&0x00ff00)>>8, PM3RD_VideoOverlayKeyG);
    RAMDAC_WRITE(pPriv->colorKey&0x0000ff, PM3RD_VideoOverlayKeyB);
    GLINT_WRITE_REG(PM3VideoOverlayUpdate_ENABLE, PM3VideoOverlayUpdate);
}


static XF86VideoAdaptorPtr 
Permedia3SetupImageVideo(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    GLINTPtr pGlint = GLINTPTR(pScrn);
    XF86VideoAdaptorPtr adapt;
    GLINTPortPrivPtr pPriv;

    if(!(adapt = xcalloc(1, sizeof(XF86VideoAdaptorRec) +
			    sizeof(GLINTPortPrivRec) +
			    sizeof(DevUnion))))
	return NULL;

    adapt->type = XvWindowMask | XvInputMask | XvImageMask;
    adapt->flags = VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT;
    adapt->name = "Permedia3 Backend Scaler";
    adapt->nEncodings = 1;
    adapt->pEncodings = DummyEncoding;
    adapt->nFormats = NUM_FORMATS;
    adapt->pFormats = Formats;
    adapt->nPorts = 1;
    adapt->pPortPrivates = (DevUnion*)(&adapt[1]);
    pPriv = (GLINTPortPrivPtr)(&adapt->pPortPrivates[1]);
    adapt->pPortPrivates[0].ptr = (pointer)(pPriv);
    adapt->pAttributes = Attributes;
    adapt->nImages = NUM_IMAGES;
    adapt->nAttributes = NUM_ATTRIBUTES;
    adapt->pImages = Images;
    adapt->PutVideo = NULL;
    adapt->PutStill = NULL;
    adapt->GetVideo = NULL;
    adapt->GetStill = NULL;
    adapt->StopVideo = Permedia3StopVideo;
    adapt->SetPortAttribute = Permedia3SetPortAttribute;
    adapt->GetPortAttribute = Permedia3GetPortAttribute;
    adapt->QueryBestSize = Permedia3QueryBestSize;
    adapt->PutImage = Permedia3PutImage;
    adapt->QueryImageAttributes = Permedia3QueryImageAttributes;

    /* FIXME : depth 15 and 16 doesn't work here */
    pPriv->colorKey = pGlint->videoKey;
    pPriv->videoStatus = 0;
    pPriv->buffer = 0; /* double buffer (or maybe triple later) */
    pPriv->doubleBuffer = TRUE;
    pPriv->autopaintColorKey = TRUE;
    pPriv->Filter = PM3VideoOverlayMode_FILTER_FULL;
    
    /* gotta uninit this someplace */
    REGION_NULL(pScreen, &pPriv->clip);

    pGlint->adaptor = adapt;

    xvDoubleBuffer      = MAKE_ATOM("XV_DOUBLE_BUFFER");
    xvColorKey          = MAKE_ATOM("XV_COLORKEY");
    xvAutopaintColorKey = MAKE_ATOM("XV_AUTOPAINT_COLORKEY");
    xvFilter		= MAKE_ATOM("XV_FILTER");

    Permedia3ResetVideo(pScrn);

    return adapt;
}


static void 
Permedia3StopVideo(ScrnInfoPtr pScrn, pointer data, Bool shutdown)
{
  GLINTPtr pGlint = GLINTPTR(pScrn);
  GLINTPortPrivPtr pPriv = (GLINTPortPrivPtr)data;
  int i;

  REGION_EMPTY(pScrn->pScreen, &pPriv->clip);   

  if(shutdown) {
     if(pPriv->videoStatus & CLIENT_VIDEO_ON) {
    	pPriv->ramdacOn = FALSE;
	GLINT_WAIT(4);
        RAMDAC_WRITE(PM3RD_VideoOverlayControl_DISABLE,
		PM3RD_VideoOverlayControl);
	GLINT_WRITE_REG(PM3VideoOverlayMode_DISABLE,
		PM3VideoOverlayMode);
     }
     for (i = 0; i < (pPriv->doubleBuffer ? 2 : 1); i++) {
        if(pPriv->area[i]) {
	   xf86FreeOffscreenArea(pPriv->area[i]);
	   pPriv->area[i] = NULL;
        }
     }
     pPriv->videoStatus = 0;
  } else {
     if(pPriv->videoStatus & CLIENT_VIDEO_ON) {
	pPriv->videoStatus |= OFF_TIMER;
	pPriv->offTime = currentTime.milliseconds + OFF_DELAY; 
     }
  }
}

static int 
Permedia3SetPortAttribute(
  ScrnInfoPtr pScrn, 
  Atom attribute,
  INT32 value, 
  pointer data
){
    GLINTPortPrivPtr pPriv = (GLINTPortPrivPtr)data;
    GLINTPtr pGlint = GLINTPTR(pScrn);

    if (attribute == xvDoubleBuffer)
    {
        if ((value < 0) || (value > 1))
            return BadValue;
        pPriv->doubleBuffer = value;
    }
    else if (attribute == xvColorKey)
    {
        pPriv->colorKey = value;
	GLINT_WAIT(9);
    	RAMDAC_WRITE((value & 0xff0000)>>16, PM3RD_VideoOverlayKeyR);
    	RAMDAC_WRITE((value & 0x00ff00)>>8, PM3RD_VideoOverlayKeyG);
    	RAMDAC_WRITE((value & 0x0000ff), PM3RD_VideoOverlayKeyB);
	REGION_EMPTY(pScrn->pScreen, &pPriv->clip);   
    }
    else if (attribute == xvAutopaintColorKey)
    {
        if ((value < 0) || (value > 1))
            return BadValue;
        pPriv->autopaintColorKey = value;
    }
    else if (attribute == xvFilter)
    {
        if ((value < 0) || (value > 2))
            return BadValue;
	switch (value) {
	    case 0:
    		pPriv->Filter = PM3VideoOverlayMode_FILTER_OFF;
		break;
	    case 1:
    		pPriv->Filter = PM3VideoOverlayMode_FILTER_FULL;
		break;
	    case 2:
    		pPriv->Filter = PM3VideoOverlayMode_FILTER_PARTIAL;
		break;
	}
    }
    else
        return BadMatch;

  return Success;
}

static int 
Permedia3GetPortAttribute(
  ScrnInfoPtr pScrn, 
  Atom attribute,
  INT32 *value, 
  pointer data
){
    GLINTPortPrivPtr pPriv = (GLINTPortPrivPtr)data;

    if (attribute == xvDoubleBuffer)
        *value = (pPriv->doubleBuffer) ? 1 : 0;
    else if (attribute == xvColorKey)
        *value = pPriv->colorKey;
    else if (attribute == xvAutopaintColorKey)
        *value = (pPriv->autopaintColorKey) ? 1 : 0;
    else if (attribute == xvFilter)
        *value = pPriv->Filter >> 14;
    else
        return BadMatch;

    return Success;
}

static void 
Permedia3QueryBestSize(
  ScrnInfoPtr pScrn, 
  Bool motion,
  short vid_w, short vid_h, 
  short drw_w, short drw_h, 
  unsigned int *p_w, unsigned int *p_h, 
  pointer data
){
    if(vid_w > (drw_w << 3))
	drw_w = vid_w >> 3;
    if(vid_h > (drw_h << 3))
	drw_h = vid_h >> 3;

    *p_w = drw_w;
    *p_h = drw_h; 
}

static void
HWCopySetup(ScrnInfoPtr pScrn, int x, int y, int w, int h)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);

    GLINT_WAIT(4);
    GLINT_WRITE_REG(0xffffffff, FBHardwareWriteMask);
    GLINT_WRITE_REG(
	PM3Config2D_ForegroundROPEnable |
	PM3Config2D_ForegroundROP(GXcopy) |
	PM3Config2D_FBWriteEnable,
	PM3Config2D);
    GLINT_WRITE_REG(
	PM3RectanglePosition_XOffset(x) |
	PM3RectanglePosition_YOffset(y),
	PM3RectanglePosition);
    GLINT_WRITE_REG(
	PM3Render2D_SpanOperation |
	PM3Render2D_XPositive |
	PM3Render2D_YPositive |
	PM3Render2D_Operation_SyncOnHostData |
	PM3Render2D_Width(w) | PM3Render2D_Height(h),
	PM3Render2D);
}

static void
HWCopyYV12(ScrnInfoPtr pScrn, CARD8 *Y, int w, int h)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    int size = w * h;
    CARD8 *V = Y + size;
    CARD8 *U = V + (size >> 2);
    CARD32 *dst;
    int pass2 = 0;
    int dwords, i, x = 0;

    dwords = size >> 1;

    w >>= 1;

    while (dwords >= pGlint->FIFOSize) {
	dst = (CARD32*)((char*)pGlint->IOBase + OutputFIFO + 4);
	GLINT_WAIT(pGlint->FIFOSize);
	/* (0x15 << 4) | 0x05 is the TAG for FBSourceData */
	GLINT_WRITE_REG(((pGlint->FIFOSize - 2) << 16) | (0x15 << 4) |
	    0x05, OutputFIFO);
	for (i = pGlint->FIFOSize - 1; i; i--, Y += 2, x++) {
	    if (x == w) {
		x = 0;
		if (pass2 == 0)
		    pass2 = 1;
		else
		if (pass2 == 1) {
		    pass2 = 0;
		    U += w;
		    V += w;
		}
	    }
	    *dst++ = Y[0] + (U[x] << 8) + (Y[1] << 16) + (V[x] << 24);
	}
	dwords -= pGlint->FIFOSize - 1;
    }
    if (dwords) {
	dst = (CARD32*)((char*)pGlint->IOBase + OutputFIFO + 4);
	GLINT_WAIT(dwords + 1);
	/* (0x15 << 4) | 0x05 is the TAG for FBSourceData */
	GLINT_WRITE_REG(((dwords - 1) << 16) | (0x15 << 4) |
	    0x05, OutputFIFO);
	for (i = dwords; i; i--, Y += 2, x++) {
	    if (x == w) {
		x = 0;
		if (pass2 == 0)
		    pass2 = 1;
		else
		if (pass2 == 1) {
		    pass2 = 0;
		    U += w;
		    V += w;
		}
	    }
	    *dst++ = Y[0] + (U[x] << 8) + (Y[1] << 16) + (V[x] << 24);
	}
    }
}

static void
HWCopyFlat(ScrnInfoPtr pScrn, CARD8 *src, int w, int h)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    GLINTPortPrivPtr pPriv = pGlint->adaptor->pPortPrivates[0].ptr;
    int pitch = pScrn->displayWidth;
    CARD8 *tmp_src;
    int dwords;

    if (w == pitch) {
    	dwords = (w * h) >> (2 - pPriv->Video_Shift);
    	while(dwords >= pGlint->FIFOSize) {
	    GLINT_WAIT(pGlint->FIFOSize);
            GLINT_WRITE_REG(((pGlint->FIFOSize - 2) << 16) | (0x15 << 4) |
			0x05, OutputFIFO);
	    GLINT_MoveDWORDS(
		(CARD32*)((char*)pGlint->IOBase + OutputFIFO + 4),
	 	(CARD32*)src, pGlint->FIFOSize - 1);
	    dwords -= (pGlint->FIFOSize - 1);
	    src += (pGlint->FIFOSize << 2) - 4;
    	}
    	if(dwords) {
	    GLINT_WAIT(dwords + 1);
            GLINT_WRITE_REG(((dwords - 1) << 16)|(0x15 << 4) |0x05, OutputFIFO);
	    GLINT_MoveDWORDS(
		(CARD32*)((char*)pGlint->IOBase + OutputFIFO + 4),
	 	(CARD32*)src, dwords);
    	}
    } else {
  	while (h--) {
    	    tmp_src = src;
    	    dwords = w >> (2 - pPriv->Video_Shift);
    	    while(dwords >= pGlint->FIFOSize) {
		GLINT_WAIT(pGlint->FIFOSize);
        	GLINT_WRITE_REG(((pGlint->FIFOSize - 2) << 16) | (0x15 << 4) |
			0x05, OutputFIFO);
		GLINT_MoveDWORDS(
			(CARD32*)((char*)pGlint->IOBase + OutputFIFO + 4),
	 		(CARD32*)src, pGlint->FIFOSize - 1);
		dwords -= (pGlint->FIFOSize - 1);
		src += (pGlint->FIFOSize << 2) - 4;
    	    }
    	    if(dwords) {
		GLINT_WAIT(dwords + 1);
        	GLINT_WRITE_REG(((dwords-1)<<16)|(0x15<<4) | 0x05, OutputFIFO);
		GLINT_MoveDWORDS(
			(CARD32*)((char*)pGlint->IOBase + OutputFIFO + 4),
	 		(CARD32*)src, dwords);
    	    }
    	    src = tmp_src + (w << pPriv->Video_Shift);
  	}
    }
}

static FBAreaPtr
Permedia3AllocateMemory(ScrnInfoPtr pScrn, FBAreaPtr area, int width, int height)
{
  ScreenPtr pScreen;
  FBAreaPtr new_area;

  if (area) {
    if ((area->box.x2 - area->box.x1 >= width) &&
        (area->box.y2 - area->box.y1 >= height))
      return area;

    if (xf86ResizeOffscreenArea(area, width, height))
      return area;

    xf86FreeOffscreenArea(area);
  }

  pScreen = screenInfo.screens[pScrn->scrnIndex];

  new_area = xf86AllocateOffscreenArea(pScreen, width, height, pScrn->bitsPerPixel / 8, NULL, NULL, NULL);

  if (!new_area) {
    int max_width, max_height;

    xf86QueryLargestOffscreenArea(pScreen, &max_width, &max_height, pScrn->bitsPerPixel / 8, 0, PRIORITY_EXTREME);

    if (max_width < width || max_height < height)
      return NULL;

    xf86PurgeUnlockedOffscreenAreas(pScreen);
    new_area = xf86AllocateOffscreenArea(pScreen, width, height, pScrn->bitsPerPixel / 8, NULL, NULL, NULL);
  }

  return new_area;
}

#define FORMAT_RGB8888	PM3VideoOverlayMode_COLORFORMAT_RGB8888 
#define FORMAT_RGB4444	PM3VideoOverlayMode_COLORFORMAT_RGB4444
#define FORMAT_RGB5551	PM3VideoOverlayMode_COLORFORMAT_RGB5551
#define FORMAT_RGB565	PM3VideoOverlayMode_COLORFORMAT_RGB565
#define FORMAT_RGB332	PM3VideoOverlayMode_COLORFORMAT_RGB332
#define FORMAT_BGR8888	PM3VideoOverlayMode_COLORFORMAT_BGR8888
#define FORMAT_BGR4444	PM3VideoOverlayMode_COLORFORMAT_BGR4444
#define FORMAT_BGR5551	PM3VideoOverlayMode_COLORFORMAT_BGR5551
#define FORMAT_BGR565	PM3VideoOverlayMode_COLORFORMAT_BGR565
#define FORMAT_BGR332	PM3VideoOverlayMode_COLORFORMAT_BGR332
#define FORMAT_CI8	PM3VideoOverlayMode_COLORFORMAT_CI8
#define FORMAT_VUY444	PM3VideoOverlayMode_COLORFORMAT_VUY444
#define FORMAT_YUV444	PM3VideoOverlayMode_COLORFORMAT_YUV444
#define FORMAT_VUY422	PM3VideoOverlayMode_COLORFORMAT_VUY422
#define FORMAT_YUV422	PM3VideoOverlayMode_COLORFORMAT_YUV422

/* Notice, have to check that we dont overflow the deltas here ... */
static void
compute_scale_factor(
    short* src_w, short* dst_w,
    unsigned int* shrink_delta, unsigned int* zoom_delta)
{
    /* NOTE: If we don't return reasonable values here then the video
     * unit can potential shut off and won't display an image until re-enabled.
     * Seems as though the zoom_delta is o.k, and I've not had the problem.
     * The 'shrink_delta' is prone to this the most - FIXME ! */

    if (*src_w >= *dst_w) {
	*src_w &= ~0x3;
	*dst_w &= ~0x3;
	*shrink_delta = (((*src_w << 16) / *dst_w) + 0x0f) & 0x0ffffff0;
	*zoom_delta = 1<<16;
	if ( ((*shrink_delta * *dst_w) >> 16) & 0x03 )
	    *shrink_delta += 0x10;
    } else {
	*src_w &= ~0x3;
	*dst_w &= ~0x3;
	*zoom_delta = (((*src_w << 16) / *dst_w) + 0x0f) & 0x0001fff0;
	*shrink_delta = 1<<16;
	if ( ((*zoom_delta * *dst_w) >> 16) & 0x03 )
	    *zoom_delta += 0x10;
    }
}

static void
Permedia3DisplayVideo(
    ScrnInfoPtr pScrn,
    int id,
    int offset,
    short width, short height,
    int x1, int y1, int x2, int y2,
    BoxPtr dstBox,
    short src_w, short src_h,
    short drw_w, short drw_h
){
    GLINTPtr pGlint = GLINTPTR(pScrn);
    GLINTPortPrivPtr portPriv = pGlint->adaptor->pPortPrivates[0].ptr;
    unsigned int shrink, zoom;
    unsigned int newx2;

    /* Let's overlay only to visible parts of the screen */
    if (dstBox->x1 == 0) {
	x1 = drw_w - dstBox->x2;
	drw_w = dstBox->x2;
    }
    if (dstBox->x2 == pScrn->frameX1) {
	x2 = drw_w - (dstBox->x2 - dstBox->x1);
	drw_w = (dstBox->x2 - dstBox->x1);
    }

    /* Avoid divide by zero in compute_scale_factor. */
    if (drw_w < 8)
	return;

    /* Let's adjust the width of source and dest to be compliant with 
     * the Permedia3 overlay unit requirement, and compute the X deltas. */
    newx2 = drw_w;
    compute_scale_factor(&src_w, &drw_w, &shrink, &zoom);
    dstBox->x2 -= (newx2 - drw_w);

    /* We do a long wait here - for everything that needs to be written */
    GLINT_WAIT(39);
    GLINT_WRITE_REG(offset>>portPriv->Video_Shift,
	portPriv->buffer ? PM3VideoOverlayBase1 : PM3VideoOverlayBase0);
    /* Let's set the source pitch. */
    GLINT_WRITE_REG(PM3VideoOverlayStride_STRIDE(pScrn->displayWidth<< 
	(pScrn->bitsPerPixel>>4) >>portPriv->Video_Shift), 
	PM3VideoOverlayStride);
    /* Let's set the position and size of the visible part of the source. */
    GLINT_WRITE_REG(PM3VideoOverlayWidth_WIDTH(src_w),
	PM3VideoOverlayWidth);
    GLINT_WRITE_REG(PM3VideoOverlayHeight_HEIGHT(src_h),
	PM3VideoOverlayHeight);
    GLINT_WRITE_REG(
	PM3VideoOverlayOrigin_XORIGIN(x1) |
	PM3VideoOverlayOrigin_YORIGIN(y1),
	PM3VideoOverlayOrigin);
    /* Scale the source to the destinationsize */
    if (src_h == drw_h) {
	GLINT_WRITE_REG(
	    PM3VideoOverlayYDelta_NONE,
	    PM3VideoOverlayYDelta);
    } else {
	GLINT_WRITE_REG(
	    PM3VideoOverlayYDelta_DELTA(src_h,drw_h),
	    PM3VideoOverlayYDelta);
    }
    if (src_w == drw_w) {
    	GLINT_WRITE_REG(1<<16, PM3VideoOverlayShrinkXDelta);
    	GLINT_WRITE_REG(1<<16, PM3VideoOverlayZoomXDelta);
    } else {
    	GLINT_WRITE_REG(shrink, PM3VideoOverlayShrinkXDelta);
    	GLINT_WRITE_REG(zoom, PM3VideoOverlayZoomXDelta);
    }
    GLINT_WRITE_REG(portPriv->buffer, PM3VideoOverlayIndex);

    /* Now set the ramdac video overlay region and mode */
    RAMDAC_WRITE((dstBox->x1&0xff), PM3RD_VideoOverlayXStartLow);
    RAMDAC_WRITE((dstBox->x1&0xf00)>>8, PM3RD_VideoOverlayXStartHigh);
    RAMDAC_WRITE((dstBox->x2&0xff), PM3RD_VideoOverlayXEndLow);
    RAMDAC_WRITE((dstBox->x2&0xf00)>>8,PM3RD_VideoOverlayXEndHigh);
    RAMDAC_WRITE((dstBox->y1&0xff), PM3RD_VideoOverlayYStartLow); 
    RAMDAC_WRITE((dstBox->y1&0xf00)>>8, PM3RD_VideoOverlayYStartHigh);
    RAMDAC_WRITE((dstBox->y2&0xff), PM3RD_VideoOverlayYEndLow); 
    RAMDAC_WRITE((dstBox->y2&0xf00)>>8,PM3RD_VideoOverlayYEndHigh);

    GLINT_WRITE_REG(portPriv->Video_Shift << 5 |
	portPriv->Format |
	portPriv->Filter |
	PM3VideoOverlayMode_BUFFERSYNC_MANUAL |
	PM3VideoOverlayMode_FLIP_VIDEO |
	PM3VideoOverlayMode_ENABLE,
	PM3VideoOverlayMode);

    if (!portPriv->ramdacOn) {
    	RAMDAC_WRITE(PM3RD_VideoOverlayControl_ENABLE |
		PM3RD_VideoOverlayControl_KEY_COLOR |
		PM3RD_VideoOverlayControl_MODE_MAINKEY |
		PM3RD_VideoOverlayControl_DIRECTCOLOR_ENABLED,
		PM3RD_VideoOverlayControl);
	portPriv->ramdacOn = TRUE;
    }
    GLINT_WRITE_REG(PM3VideoOverlayUpdate_ENABLE,
	PM3VideoOverlayUpdate);
}

static int 
Permedia3PutImage( 
  ScrnInfoPtr pScrn, 
  short src_x, short src_y, 
  short drw_x, short drw_y,
  short src_w, short src_h, 
  short drw_w, short drw_h,
  int id, unsigned char* buf, 
  short width, short height, 
  Bool sync,
  RegionPtr clipBoxes, pointer data
){
#if 0
   GLINTPtr pGlint = GLINTPTR(pScrn);
#endif
   GLINTPortPrivPtr pPriv = (GLINTPortPrivPtr)data;
   INT32 x1, x2, y1, y2;
   int pitch;
   int i;
   int w_bpp, bpp;
   Bool copy_flat = TRUE;
   BoxRec dstBox;

   /* Let's find the image format and Video_Shift values */
   switch (id) {
	case LE4CC('Y','V','1','2'):
	    pPriv->Format = FORMAT_YUV422;
	    pPriv->Video_Shift = 1;
	    copy_flat = FALSE;
	    break;
	case LE4CC('Y','U','Y','2'):
	    pPriv->Format = FORMAT_YUV422;
	    pPriv->Video_Shift = 1;
	    break;
	case LE4CC('U','Y','V','Y'):
	    pPriv->Format = FORMAT_VUY422;
	    pPriv->Video_Shift = 1;
	    break;
	case LE4CC('Y','U','V','A'):
	    pPriv->Format = FORMAT_YUV444;
	    pPriv->Video_Shift = 2;
	    break;
	case LE4CC('V','U','Y','A'):
	    pPriv->Format = FORMAT_VUY444;
	    pPriv->Video_Shift = 2;
	    break;
	case 0x41: /* RGBA 8:8:8:8 */
	    pPriv->Format = FORMAT_RGB8888;
	    pPriv->Video_Shift = 2;
	    break;
	case 0x42: /* RGB 5:6:5 */
	    pPriv->Format = FORMAT_RGB565;
	    pPriv->Video_Shift = 1;
	    break;
	case 0x43: /* RGB 1:5:5:5 */
	    pPriv->Format = FORMAT_RGB5551;
	    pPriv->Video_Shift = 1;
	    break;
	case 0x44: /* RGB 4:4:4:4 */
	    pPriv->Format = FORMAT_RGB4444;
	    pPriv->Video_Shift = 1;
	    break;
	case 0x46: /* RGB 2:3:3 */
	    pPriv->Format = FORMAT_RGB332;
	    pPriv->Video_Shift = 0;
	    break;
	case 0x47: /* BGRA 8:8:8:8 */
	    pPriv->Format = FORMAT_BGR8888;
	    pPriv->Video_Shift = 2;
	    break;
	case 0x48: /* BGR 5:6:5 */
	    pPriv->Format = FORMAT_BGR565;
	    pPriv->Video_Shift = 1;
	    break;
	case 0x49: /* BGR 1:5:5:5 */
	    pPriv->Format = FORMAT_BGR5551;
	    pPriv->Video_Shift = 1;
	    break;
	case 0x4A: /* BGR 4:4:4:4 */
	    pPriv->Format = FORMAT_BGR4444;
	    pPriv->Video_Shift = 1;
	    break;
	case 0x4C: /* BGR 2:3:3 */
	    pPriv->Format = FORMAT_BGR332;
	    pPriv->Video_Shift = 0;
	    break;
	default:
	    return XvBadAlloc;
    }

    /* Clip */
    x1 = src_x;
    x2 = src_x + src_w;
    y1 = src_y;
    y2 = src_y + src_h;

    dstBox.x1 = drw_x;
    dstBox.x2 = drw_x + drw_w;
    dstBox.y1 = drw_y;
    dstBox.y2 = drw_y + drw_h;

    if(!xf86XVClipVideoHelper(&dstBox, &x1, &x2, &y1, &y2, clipBoxes, 
                              width, height))
    	return Success;

    dstBox.x1 -= pScrn->frameX0;
    dstBox.x2 -= pScrn->frameX0;
    dstBox.y1 -= pScrn->frameY0;
    dstBox.y2 -= pScrn->frameY0;

    bpp = pScrn->bitsPerPixel >> 3;
    pitch = bpp * pScrn->displayWidth;

    w_bpp = (width << pPriv->Video_Shift) >> (pScrn->bitsPerPixel >> 4);

    for (i = 0; i < (pPriv->doubleBuffer ? 2 : 1); i++) {
      if (!(pPriv->area[i] = 
		Permedia3AllocateMemory(pScrn,pPriv->area[i],w_bpp,src_h)))
        return BadAlloc;

      pPriv->offset[i] = (pPriv->area[i]->box.x1 * bpp) + 
					(pPriv->area[i]->box.y1 * pitch);
    }

    HWCopySetup(pScrn, pPriv->area[pPriv->buffer]->box.x1, 
		       pPriv->area[pPriv->buffer]->box.y1, w_bpp, height);

    if (copy_flat) 
	HWCopyFlat(pScrn, buf, width, height);
    else 
	HWCopyYV12(pScrn, buf, width, height);

    /* paint the color key */
    if(pPriv->autopaintColorKey &&
       !REGION_EQUAL(pScrn->pScreen, &pPriv->clip, clipBoxes)) {
    	/* update cliplist */
        REGION_COPY(pScrn->pScreen, &pPriv->clip, clipBoxes);
#if 0
	GLINT_WAIT(1);
    	GLINT_WRITE_REG(PM3VideoOverlayMode_DISABLE,
						PM3VideoOverlayMode);
	pPriv->ramdacOn = FALSE;
#endif
        xf86XVFillKeyHelper(pScrn->pScreen, pPriv->colorKey, clipBoxes);
    }

    Permedia3Sync(pScrn);

    Permedia3DisplayVideo(pScrn, id, pPriv->offset[pPriv->buffer], width,height,
	     x1, y1, x2, y2, &dstBox, src_w, src_h, drw_w, drw_h);

    /* Switch buffer on next run - double buffer */
    if (pPriv->doubleBuffer) {
    	if (!pPriv->buffer)
	    pPriv->buffer = 1;
    	else
	    pPriv->buffer = 0;
    }

    pPriv->videoStatus = CLIENT_VIDEO_ON;

    return Success;
}

static int 
Permedia3QueryImageAttributes(
  ScrnInfoPtr pScrn, 
  int id, 
  unsigned short *w, unsigned short *h, 
  int *pitches, int *offsets
){
    int size, tmp;

    if(*w > 2047) *w = 2047;
    if(*h > 2047) *h = 2047;

    *w = (*w + 1) & ~1;
    if(offsets) offsets[0] = 0;

    switch(id) {
    case FOURCC_YV12:		/* YV12 */
	*h = (*h + 1) & ~1;
	size = (*w + 3) & ~3;
	if(pitches) pitches[0] = size;
	size *= *h;
	if(offsets) offsets[1] = size;
	tmp = ((*w >> 1) + 3) & ~3;
	if(pitches) pitches[1] = pitches[2] = tmp;
	tmp *= (*h >> 1);
	size += tmp;
	if(offsets) offsets[2] = size;
	size += tmp;
	break;
    default:			/* RGB15, RGB16, YUY2 */
	size = *w << 1;
	if(pitches) pitches[0] = size;
	size *= *h;
	break;
    }

    return size;
}

/****************** Offscreen stuff ***************/

typedef struct {
  FBAreaPtr area;
  Bool isOn;
  int Video_Shift;
  int Format;
  Bool ramdacOn;
} OffscreenPrivRec, * OffscreenPrivPtr;

static int 
Permedia3AllocateSurface(
    ScrnInfoPtr pScrn,
    int id,
    unsigned short w, 	
    unsigned short h,
    XF86SurfacePtr surface
){
    FBAreaPtr area;
    int fbpitch, bpp;
    OffscreenPrivPtr pPriv;

    if((w > 2047) || (h > 2047))
	return BadAlloc;

    w = (w + 1) & ~1;
    bpp = pScrn->bitsPerPixel >> 3;
    fbpitch = bpp * pScrn->displayWidth;

    if(!(area = Permedia3AllocateMemory(pScrn, NULL, w, h)))
	return BadAlloc;

    surface->width = w;
    surface->height = h;

    if(!(surface->offsets = xalloc(sizeof(int)))) {
	xf86FreeOffscreenArea(area);
	return BadAlloc;
    }
    if(!(pPriv = xalloc(sizeof(OffscreenPrivRec)))) {
	xfree(surface->offsets);
	xf86FreeOffscreenArea(area);
	return BadAlloc;
    }

    pPriv->area = area;
    pPriv->isOn = FALSE;

    surface->pScrn = pScrn;
    surface->id = id;   
    surface->offsets[0] = (area->box.x1 * bpp) + (area->box.y1 * fbpitch);
    surface->devPrivate.ptr = (pointer)pPriv;

    return Success;
}

static int 
Permedia3StopSurface(
    XF86SurfacePtr surface
){
    OffscreenPrivPtr pPriv = (OffscreenPrivPtr)surface->devPrivate.ptr;

    if(pPriv->isOn) {
	GLINTPtr pGlint = GLINTPTR(surface->pScrn);
    	pPriv->ramdacOn = FALSE;
	GLINT_WAIT(4);
        RAMDAC_WRITE(PM3RD_VideoOverlayControl_DISABLE,
		PM3RD_VideoOverlayControl);
	GLINT_WRITE_REG(PM3VideoOverlayMode_DISABLE,
		PM3VideoOverlayMode);
	pPriv->isOn = FALSE;
    }

    return Success;
}

static int 
Permedia3FreeSurface(
    XF86SurfacePtr surface
){
    OffscreenPrivPtr pPriv = (OffscreenPrivPtr)surface->devPrivate.ptr;

    if(pPriv->isOn)
	Permedia3StopSurface(surface);
    xf86FreeOffscreenArea(pPriv->area);
    xfree(surface->pitches);
    xfree(surface->offsets);
    xfree(surface->devPrivate.ptr);

    return Success;
}

static int
Permedia3GetSurfaceAttribute(
    ScrnInfoPtr pScrn,
    Atom attribute,
    INT32 *value
){
    return Permedia3GetPortAttribute(pScrn, attribute, value, 
			(pointer)(GET_PORT_PRIVATE(pScrn)));
}

static int
Permedia3SetSurfaceAttribute(
    ScrnInfoPtr pScrn,
    Atom attribute,
    INT32 value
){
    return Permedia3SetPortAttribute(pScrn, attribute, value, 
			(pointer)(GET_PORT_PRIVATE(pScrn)));
}

static int 
Permedia3DisplaySurface(
    XF86SurfacePtr surface,
    short src_x, short src_y, 
    short drw_x, short drw_y,
    short src_w, short src_h, 
    short drw_w, short drw_h,
    RegionPtr clipBoxes
){
    OffscreenPrivPtr pPriv = (OffscreenPrivPtr)surface->devPrivate.ptr;
    ScrnInfoPtr pScrn = surface->pScrn;
    GLINTPtr pGlint = GLINTPTR(pScrn);
    GLINTPortPrivPtr portPriv = pGlint->adaptor->pPortPrivates[0].ptr;
    INT32 x1, y1, x2, y2;
    BoxRec dstBox;

    x1 = src_x;
    x2 = src_x + src_w;
    y1 = src_y;
    y2 = src_y + src_h;

    dstBox.x1 = drw_x;
    dstBox.x2 = drw_x + drw_w;
    dstBox.y1 = drw_y;
    dstBox.y2 = drw_y + drw_h;

    if(!xf86XVClipVideoHelper(&dstBox, &x1, &x2, &y1, &y2, clipBoxes, 
		    surface->width, surface->height))
    {
        return Success;
    }

    dstBox.x1 -= pScrn->frameX0;
    dstBox.x2 -= pScrn->frameX0;
    dstBox.y1 -= pScrn->frameY0;
    dstBox.y2 -= pScrn->frameY0;

   /* Let's find the image format and Video_Shift values */
   switch (surface->id) {
	case LE4CC('Y','V','1','2'):
	    pPriv->Format = FORMAT_YUV422;
	    pPriv->Video_Shift = 1;
	    break;
	case LE4CC('Y','U','Y','2'):
	    pPriv->Format = FORMAT_YUV422;
	    pPriv->Video_Shift = 1;
	    break;
	case LE4CC('U','Y','V','Y'):
	    pPriv->Format = FORMAT_VUY422;
	    pPriv->Video_Shift = 1;
	    break;
	case LE4CC('Y','U','V','A'):
	    pPriv->Format = FORMAT_YUV444;
	    pPriv->Video_Shift = 2;
	    break;
	case LE4CC('V','U','Y','A'):
	    pPriv->Format = FORMAT_VUY444;
	    pPriv->Video_Shift = 2;
	    break;
	case 0x41: /* RGBA 8:8:8:8 */
	    pPriv->Format = FORMAT_RGB8888;
	    pPriv->Video_Shift = 2;
	    break;
	case 0x42: /* RGB 5:6:5 */
	    pPriv->Format = FORMAT_RGB565;
	    pPriv->Video_Shift = 1;
	    break;
	case 0x43: /* RGB 1:5:5:5 */
	    pPriv->Format = FORMAT_RGB5551;
	    pPriv->Video_Shift = 1;
	    break;
	case 0x44: /* RGB 4:4:4:4 */
	    pPriv->Format = FORMAT_RGB4444;
	    pPriv->Video_Shift = 1;
	    break;
	case 0x46: /* RGB 2:3:3 */
	    pPriv->Format = FORMAT_RGB332;
	    pPriv->Video_Shift = 0;
	    break;
	case 0x47: /* BGRA 8:8:8:8 */
	    pPriv->Format = FORMAT_BGR8888;
	    pPriv->Video_Shift = 2;
	    break;
	case 0x48: /* BGR 5:6:5 */
	    pPriv->Format = FORMAT_BGR565;
	    pPriv->Video_Shift = 1;
	    break;
	case 0x49: /* BGR 1:5:5:5 */
	    pPriv->Format = FORMAT_BGR5551;
	    pPriv->Video_Shift = 1;
	    break;
	case 0x4A: /* BGR 4:4:4:4 */
	    pPriv->Format = FORMAT_BGR4444;
	    pPriv->Video_Shift = 1;
	    break;
	case 0x4C: /* BGR 2:3:3 */
	    pPriv->Format = FORMAT_BGR332;
	    pPriv->Video_Shift = 0;
	    break;
	default:
	    return XvBadAlloc;
    }

    Permedia3DisplayVideo(pScrn, surface->id, surface->offsets[0], 
	     surface->width, surface->height, 
	     x1, y1, x2, y2, &dstBox, src_w, src_h, drw_w, drw_h);

    xf86XVFillKeyHelper(pScrn->pScreen, portPriv->colorKey, clipBoxes);

    pPriv->isOn = TRUE;
    /* we've prempted the XvImage stream so set its free timer */
    if(portPriv->videoStatus & CLIENT_VIDEO_ON) {
	REGION_EMPTY(pScrn->pScreen, &portPriv->clip);   
	UpdateCurrentTime();
	portPriv->videoStatus = FREE_TIMER;
	portPriv->freeTime = currentTime.milliseconds + FREE_DELAY;
	pGlint->VideoTimerCallback = Permedia3VideoTimerCallback;
    }

    return Success;
}

static void 
Permedia3InitOffscreenImages(ScreenPtr pScreen)
{
    XF86OffscreenImagePtr offscreenImages;

    /* need to free this someplace */
    if(!(offscreenImages = xalloc(sizeof(XF86OffscreenImageRec))))
	return;

    offscreenImages[0].image = &Images[0];
    offscreenImages[0].flags = VIDEO_OVERLAID_IMAGES | 
			       VIDEO_CLIP_TO_VIEWPORT;
    offscreenImages[0].alloc_surface = Permedia3AllocateSurface;
    offscreenImages[0].free_surface = Permedia3FreeSurface;
    offscreenImages[0].display = Permedia3DisplaySurface;
    offscreenImages[0].stop = Permedia3StopSurface;
    offscreenImages[0].setAttribute = Permedia3SetSurfaceAttribute;
    offscreenImages[0].getAttribute = Permedia3GetSurfaceAttribute;
    offscreenImages[0].max_width = 2047;
    offscreenImages[0].max_height = 2047;
    offscreenImages[0].num_attributes = NUM_ATTRIBUTES;
    offscreenImages[0].attributes = Attributes;
    
    xf86XVRegisterOffscreenImages(pScreen, offscreenImages, 1);
}

static void
Permedia3VideoTimerCallback(ScrnInfoPtr pScrn, Time time)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    GLINTPortPrivPtr pPriv = pGlint->adaptor->pPortPrivates[0].ptr;
    int i;

    if(pPriv->videoStatus & TIMER_MASK) {
	if(pPriv->videoStatus & OFF_TIMER) {
	    if(pPriv->offTime < time) {
		pPriv->ramdacOn = FALSE;
		GLINT_WAIT(4);
        	RAMDAC_WRITE(PM3RD_VideoOverlayControl_DISABLE,
					PM3RD_VideoOverlayControl);
		GLINT_WRITE_REG(PM3VideoOverlayMode_DISABLE,
					PM3VideoOverlayMode);
		pPriv->videoStatus = FREE_TIMER;
		pPriv->freeTime = time + FREE_DELAY;
	    }
	} else {  /* FREE_TIMER */
	    if(pPriv->freeTime < time) {
		for (i = 0; i < (pPriv->doubleBuffer ? 2 : 1); i++) {
		   if(pPriv->area[i]) {
		      xf86FreeOffscreenArea(pPriv->area[i]);
		      pPriv->area[i] = NULL;
		   }
		}
		pPriv->videoStatus = 0;
	        pGlint->VideoTimerCallback = NULL;
	    }
        }
    } else  /* shouldn't get here */
	pGlint->VideoTimerCallback = NULL;
}
