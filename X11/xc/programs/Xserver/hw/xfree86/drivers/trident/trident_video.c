/*
 * Copyright 1992-2000 by Alan Hourihane, Wigan, England.
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
 * Author:  Alan Hourihane, alanh@fairlite.demon.co.uk
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/trident/trident_video.c,v 1.31 2002/12/22 18:54:43 alanh Exp $ */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86Resources.h"
#include "xf86_ansic.h"
#include "compiler.h"
#include "xf86PciInfo.h"
#include "xf86Pci.h"
#include "xf86fbman.h"
#include "regionstr.h"

#include "trident.h"
#include "trident_regs.h"
#include "Xv.h"
#include "xaa.h"
#include "xaalocal.h"
#include "dixstruct.h"
#include "fourcc.h"

#define OFF_DELAY 	800  /* milliseconds */
#define FREE_DELAY 	60000

#define OFF_TIMER 	0x01
#define FREE_TIMER	0x02
#define CLIENT_VIDEO_ON	0x04

#define TIMER_MASK      (OFF_TIMER | FREE_TIMER)

static XF86VideoAdaptorPtr TRIDENTSetupImageVideo(ScreenPtr);
static void TRIDENTInitOffscreenImages(ScreenPtr);
static void TRIDENTStopVideo(ScrnInfoPtr, pointer, Bool);
static int TRIDENTSetPortAttribute(ScrnInfoPtr, Atom, INT32, pointer);
static int TRIDENTGetPortAttribute(ScrnInfoPtr, Atom ,INT32 *, pointer);
static void TRIDENTQueryBestSize(ScrnInfoPtr, Bool,
	short, short, short, short, unsigned int *, unsigned int *, pointer);
static int TRIDENTPutImage( ScrnInfoPtr, 
	short, short, short, short, short, short, short, short,
	int, unsigned char*, short, short, Bool, RegionPtr, pointer);
static int TRIDENTQueryImageAttributes(ScrnInfoPtr, 
	int, unsigned short *, unsigned short *,  int *, int *);
static void TRIDENTVideoTimerCallback(ScrnInfoPtr pScrn, Time time);
static void tridentSetVideoContrast(TRIDENTPtr pTrident,int value);
static void tridentSetVideoParameters(TRIDENTPtr pTrident, int brightness, 
				      int saturation, int hue);
void tridentFixFrame(ScrnInfoPtr pScrn, int *fixFrame);
static void WaitForSync(ScrnInfoPtr pScrn);

#define MAKE_ATOM(a) MakeAtom(a, sizeof(a) - 1, TRUE)

static Atom xvColorKey, xvSaturation, xvBrightness, xvHUE,  xvContrast;

void TRIDENTInitVideo(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    XF86VideoAdaptorPtr *adaptors, *newAdaptors = NULL;
    XF86VideoAdaptorPtr newAdaptor = NULL;
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    int num_adaptors;

    /* 
     * The following has been tested on:
     *
     * 9525         : flags: None
     * Cyber9397(DVD) : flags: VID_ZOOM_NOMINI
     * CyberBlade/i7: flags: VID_ZOOM_INV | VID_ZOOM_MINI
     * CyberBlade/i1: flags: VID_ZOOM_INV | VID_ZOOM_MINI
     * CyberBlade/Ai1: flags: VID_ZOOM_INV 
     * Cyber 9540   : flags: VID_ZOOM_INV | VID_SHIFT_4
     * CyberXPm8    : flags: VID_ZOOM_INV | VID_SHIFT_4
     *
     * When you make changes make sure not to break these
     * Add new chipsets to this list.
     */
    if (pTrident->Chipset >= BLADE3D) {
       pTrident->videoFlags = VID_ZOOM_INV ;
       if (pTrident->Chipset <= CYBERBLADEI1D)
	 pTrident->videoFlags |= VID_ZOOM_MINI;
       else if (pTrident->Chipset < CYBERBLADEAI1 /* verified EE */
		|| pTrident->Chipset > CYBERBLADEAI1D)
	 pTrident->videoFlags |= VID_OFF_SHIFT_4;
    }
    if (pTrident->Chipset == CYBER9397 || pTrident->Chipset == CYBER9397DVD)
	pTrident->videoFlags = VID_ZOOM_NOMINI;

    if (pTrident->Chipset == CYBER9397DVD || pTrident->Chipset >= CYBER9525DVD)
	pTrident->videoFlags |= VID_DOUBLE_LINEBUFFER_FOR_WIDE_SRC;

    newAdaptor = TRIDENTSetupImageVideo(pScreen);
    TRIDENTInitOffscreenImages(pScreen);

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

    if (pTrident->videoFlags)
	xf86DrvMsgVerb(pScrn->scrnIndex,X_INFO,3,
		       "Trident Video Flags: %s %s %s\n",
		   pTrident->videoFlags & VID_ZOOM_INV ? "VID_ZOOM_INV" : "",
		   pTrident->videoFlags & VID_ZOOM_MINI ? "VID_ZOOM_MINI" : "",                   pTrident->videoFlags & VID_OFF_SHIFT_4 ? "VID_OFF_SHIFT_4"
		   : "",
		   pTrident->videoFlags & VID_ZOOM_NOMINI ? "VID_ZOOM_NOMINI"
		   : "");
		   
}

/* client libraries expect an encoding */
static XF86VideoEncodingRec DummyEncoding[1] =
{
 {
   0,
   "XV_IMAGE",
   1024, 1024,
   {1, 1}
 }
};

#define NUM_FORMATS 4

static XF86VideoFormatRec Formats[NUM_FORMATS] = 
{
  {8, PseudoColor},  {15, TrueColor}, {16, TrueColor}, {24, TrueColor}
};

#ifdef TRIDENT_XV_GAMMA
#define NUM_ATTRIBUTES 6
#else
#define NUM_ATTRIBUTES 5
#endif

static XF86AttributeRec Attributes[NUM_ATTRIBUTES] =
{
    {XvSettable | XvGettable, 0, (1 << 24) - 1, "XV_COLORKEY"},
    {XvSettable | XvGettable, 0, 187,           "XV_SATURATION"},
    {XvSettable | XvGettable, 0, 0x3F,          "XV_BRIGHTNESS"},
    {XvSettable | XvGettable, 0, 360 ,          "XV_HUE"},
    {XvSettable | XvGettable, 0, 7,           "XV_CONTRAST"}
};

#if 0
# define NUM_IMAGES 4
#else
# define NUM_IMAGES 4
#endif

static XF86ImageRec Images[NUM_IMAGES] =
{
#if 0
    {
	0x35315652,
        XvRGB,
	LSBFirst,
	{'R','V','1','5',
	  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
	16,
	XvPacked,
	1,
	15, 0x001F, 0x03E0, 0x7C00,
	0, 0, 0,
	0, 0, 0,
	0, 0, 0,
	{'R','V','B',0,
	  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
	XvTopToBottom
   },
#endif
   {
	0x36315652,
        XvRGB,
	LSBFirst,
	{'R','V','1','6',
	  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
	16,
	XvPacked,
	1,
	16, 0xF800, 0x07E0, 0x001F,
	0, 0, 0,
	0, 0, 0,
	0, 0, 0,
	{'R','V','B',0,
	  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
	XvTopToBottom
   },
   XVIMAGE_YV12,
   XVIMAGE_YUY2
};

typedef struct {
   FBLinearPtr	linear;
   RegionRec	clip;
   CARD32	colorKey;
   CARD8        Saturation;
   CARD8        Brightness;
   CARD16       HUE;
   INT8         Contrast;
   CARD32	videoStatus;
   Time		offTime;
   Time		freeTime;
   int          fixFrame;
} TRIDENTPortPrivRec, *TRIDENTPortPrivPtr;


#define GET_PORT_PRIVATE(pScrn) \
   (TRIDENTPortPrivPtr)((TRIDENTPTR(pScrn))->adaptor->pPortPrivates[0].ptr)

void TRIDENTResetVideo(ScrnInfoPtr pScrn) 
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    TRIDENTPortPrivPtr pPriv = pTrident->adaptor->pPortPrivates[0].ptr;
    int vgaIOBase = VGAHWPTR(pScrn)->IOBase;
    int red, green, blue;
    int tmp;

    if (pTrident->Chipset >= CYBER9388) {
    	OUTW(vgaIOBase + 4, 0x80B9); 
    	OUTW(vgaIOBase + 4, 0x00BE); 
    	OUTW(0x3C4, 0xC057);
    	OUTW(0x3C4, 0x3420);
    	OUTW(0x3C4, 0x3037);
    } else {
	if (pTrident->Chipset >= PROVIDIA9682) {
    	    OUTB(0x83C8, 0x57);
    	    OUTB(0x83C6, 0xC0);
    	    OUTW(vgaIOBase + 4, 0x26BE); 
	} else {
    	    OUTB(0x83C8, 0x37);
    	    OUTB(0x83C6, 0x01);
    	    OUTB(0x83C8, 0x00);
    	    OUTB(0x83C6, 0x00);
	}
    }

    if (pTrident->Chipset >= BLADEXP) {
	OUTW(0x3C4, 0x007A);
	OUTW(0x3C4, 0x007D);
    }
    switch (pScrn->depth) {
    case 8:
	VIDEOOUT(pPriv->colorKey, pTrident->keyOffset);
	VIDEOOUT(0x00, (pTrident->keyOffset + 1));
	VIDEOOUT(0x00, (pTrident->keyOffset + 2));
	VIDEOOUT(0xFF, (pTrident->keyOffset + 4));
	VIDEOOUT(0x00, (pTrident->keyOffset + 5));
	VIDEOOUT(0x00, (pTrident->keyOffset + 6));
	break;
    default:
	red = (pPriv->colorKey & pScrn->mask.red) >> pScrn->offset.red;
	green = (pPriv->colorKey & pScrn->mask.green) >> pScrn->offset.green;
	blue = (pPriv->colorKey & pScrn->mask.blue) >> pScrn->offset.blue;
	switch (pScrn->depth) {
	case 15:
	    tmp = (red << 10) | (green << 5) | (blue);
	    VIDEOOUT((tmp & 0xff), pTrident->keyOffset);
	    VIDEOOUT((tmp & 0xff00)>>8, (pTrident->keyOffset + 1));
	    VIDEOOUT(0x00, (pTrident->keyOffset + 2));
	    VIDEOOUT(0xFF, (pTrident->keyOffset + 4));
	    VIDEOOUT(0xFF, (pTrident->keyOffset + 5));
	    VIDEOOUT(0x00, (pTrident->keyOffset + 6));
	    break;
	case 16:
	    tmp = (red << 11) | (green << 5) | (blue);
	    VIDEOOUT((tmp & 0xff), pTrident->keyOffset);
	    VIDEOOUT((tmp & 0xff00)>>8, (pTrident->keyOffset + 1));
	    VIDEOOUT(0x00, (pTrident->keyOffset + 2));
	    VIDEOOUT(0xFF, (pTrident->keyOffset + 4));
	    VIDEOOUT(0xFF, (pTrident->keyOffset + 5));
	    VIDEOOUT(0x00, (pTrident->keyOffset + 6));
	    break;
	case 24:
	    VIDEOOUT(blue, pTrident->keyOffset);
	    VIDEOOUT(green, (pTrident->keyOffset + 1));
	    VIDEOOUT(red, (pTrident->keyOffset + 2));
	    VIDEOOUT(0xFF, (pTrident->keyOffset + 4));
	    VIDEOOUT(0xFF, (pTrident->keyOffset + 5));
	    VIDEOOUT(0xFF, (pTrident->keyOffset + 6));
	    break;
	}    
    }

    if (pTrident->Chipset >= CYBER9388) {
    	tridentSetVideoContrast(pTrident,pPriv->Contrast);
    	tridentSetVideoParameters(pTrident,pPriv->Brightness,pPriv->Saturation,
                            pPriv->HUE);
    }

    OUTW(vgaIOBase + 4, 0x848E);
}


static XF86VideoAdaptorPtr 
TRIDENTSetupImageVideo(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    XF86VideoAdaptorPtr adapt;
    TRIDENTPortPrivPtr pPriv;

    if(!(adapt = xcalloc(1, sizeof(XF86VideoAdaptorRec) +
			    sizeof(TRIDENTPortPrivRec) +
			    sizeof(DevUnion))))
	return NULL;

    adapt->type = XvWindowMask | XvInputMask | XvImageMask;
    adapt->flags = VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT;
    adapt->name = "Trident Backend Scaler";
    adapt->nEncodings = 1;
    adapt->pEncodings = DummyEncoding;
    adapt->nFormats = NUM_FORMATS;
    adapt->pFormats = Formats;
    adapt->nPorts = 1;
    adapt->pPortPrivates = (DevUnion*)(&adapt[1]);
    pPriv = (TRIDENTPortPrivPtr)(&adapt->pPortPrivates[1]);
    adapt->pPortPrivates[0].ptr = (pointer)(pPriv);
    adapt->pAttributes = Attributes;
    adapt->nImages = NUM_IMAGES;
    if (pTrident->Chipset >= CYBER9388) {
    	adapt->nAttributes = NUM_ATTRIBUTES;
    } else {
    	adapt->nAttributes = 1; /* Just colorkey */
    }
    adapt->pImages = Images;
    adapt->PutVideo = NULL;
    adapt->PutStill = NULL;
    adapt->GetVideo = NULL;
    adapt->GetStill = NULL;
    adapt->StopVideo = TRIDENTStopVideo;
    adapt->SetPortAttribute = TRIDENTSetPortAttribute;
    adapt->GetPortAttribute = TRIDENTGetPortAttribute;
    adapt->QueryBestSize = TRIDENTQueryBestSize;
    adapt->PutImage = TRIDENTPutImage;
    adapt->QueryImageAttributes = TRIDENTQueryImageAttributes;

    pPriv->colorKey = pTrident->videoKey & ((1 << pScrn->depth) - 1);
    pPriv->Brightness = 45;
    pPriv->Saturation = 80;
    pPriv->Contrast = 4;
    pPriv->HUE = 0;
    pPriv->videoStatus = 0;
    pPriv->fixFrame = 100;

    /* gotta uninit this someplace */
    REGION_INIT(pScreen, &pPriv->clip, NullBox, 0); 

    pTrident->adaptor = adapt;

    xvColorKey   = MAKE_ATOM("XV_COLORKEY");
   
    if (pTrident->Chipset >= CYBER9388) {
    	xvBrightness = MAKE_ATOM("XV_BRIGHTNESS");
    	xvSaturation = MAKE_ATOM("XV_SATURATION");
    	xvHUE        = MAKE_ATOM("XV_HUE");
    	xvContrast   = MAKE_ATOM("XV_CONTRAST");
    }

    if (pTrident->Chipset >= PROVIDIA9682) 
	pTrident->keyOffset = 0x50;
    else
	pTrident->keyOffset = 0x30;

    TRIDENTResetVideo(pScrn);

    return adapt;
}


static Bool
RegionsEqual(RegionPtr A, RegionPtr B)
{
    int *dataA, *dataB;
    int num;

    num = REGION_NUM_RECTS(A);
    if(num != REGION_NUM_RECTS(B))
	return FALSE;

    if((A->extents.x1 != B->extents.x1) ||
       (A->extents.x2 != B->extents.x2) ||
       (A->extents.y1 != B->extents.y1) ||
       (A->extents.y2 != B->extents.y2))
	return FALSE;

    dataA = (int*)REGION_RECTS(A);
    dataB = (int*)REGION_RECTS(B);

    while(num--) {
	if((dataA[0] != dataB[0]) || (dataA[1] != dataB[1]))
	   return FALSE;
	dataA += 2; 
	dataB += 2;
    }

    return TRUE;
}

#define DummyScreen screenInfo.screens[0]

static void 
TRIDENTStopVideo(ScrnInfoPtr pScrn, pointer data, Bool shutdown)
{
  TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
  TRIDENTPortPrivPtr pPriv = (TRIDENTPortPrivPtr)data;
  int vgaIOBase = VGAHWPTR(pScrn)->IOBase;

  REGION_EMPTY(pScrn->pScreen, &pPriv->clip);   

  if(shutdown) {
     if(pPriv->videoStatus & CLIENT_VIDEO_ON) {
	 OUTW(vgaIOBase + 4, 0x0091);
	 WaitForSync(pScrn);
	 OUTW(vgaIOBase + 4, 0x848E);
     }
     if(pPriv->linear) {
	xf86FreeOffscreenLinear(pPriv->linear);
	pPriv->linear = NULL;
     }
     pPriv->videoStatus = 0;
  } else {
     if(pPriv->videoStatus & CLIENT_VIDEO_ON) {
	pPriv->videoStatus |= OFF_TIMER;
	pPriv->offTime = currentTime.milliseconds + OFF_DELAY; 
	pTrident->VideoTimerCallback = TRIDENTVideoTimerCallback;
     }
  }
}

#undef PI
#define PI 3.14159265

static void
tridentSetVideoContrast(TRIDENTPtr pTrident,int value)
{
  OUTW(0x3C4, (((value & 0x7)|((value & 0x7) << 4)) << 8) | 0xBC);
}

static void
tridentSetVideoParameters(TRIDENTPtr pTrident, int brightness, 
			  int saturation, int hue)
{
    double dtmp;
    CARD8 sign, tmp, tmp1;

    if (brightness >= 0x20) 
      brightness -= 0x20;
    else 
      brightness += 0x20;
    dtmp = sin((double)hue / 180.0 * PI) * saturation / 12.5;
    sign = (dtmp < 0) ? 1 << 1 : 0;
    tmp1 = ((int)fabs(dtmp) >> 4) & 0x1;
    tmp = brightness << 2 | sign | tmp1;
    OUTW(0x3C4, tmp << 8 | 0xB1);

    tmp1 = ((int)fabs(dtmp) & 0x7 ) << 5;
    dtmp = cos((double)hue / 180.0 * PI) * saturation / 12.5;
    sign = (dtmp < 0) ? 1 << 4 : 0;
    tmp1 |= (int)fabs(dtmp)  & 0xf;
    tmp = sign | tmp1;
    OUTW(0x3C4, tmp << 8 | 0xB0);
}

static int 
TRIDENTSetPortAttribute(
  ScrnInfoPtr pScrn, 
  Atom attribute,
  INT32 value, 
  pointer data
){
  TRIDENTPortPrivPtr pPriv = (TRIDENTPortPrivPtr)data;
  TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

  if(attribute == xvColorKey) {
	int red, green, blue;
	int tmp;
	pPriv->colorKey = value;
	switch (pScrn->depth) {
	case 8:
	    VIDEOOUT(pPriv->colorKey, pTrident->keyOffset);
	    VIDEOOUT(0x00, (pTrident->keyOffset + 1));
	    VIDEOOUT(0x00, (pTrident->keyOffset + 2));
	    break;
	default:
	    red = (pPriv->colorKey & pScrn->mask.red) >> pScrn->offset.red;
	    green = (pPriv->colorKey & pScrn->mask.green) >> pScrn->offset.green;
	    blue = (pPriv->colorKey & pScrn->mask.blue) >> pScrn->offset.blue;
	    switch (pScrn->depth) {
	    case 15:
	    	tmp = (red << 10) | (green << 5) | (blue);
	    	VIDEOOUT((tmp&0xff), pTrident->keyOffset);
	    	VIDEOOUT((tmp&0xff00)>>8, (pTrident->keyOffset + 1));
	    	VIDEOOUT(0x00, (pTrident->keyOffset + 2));
		break;
	    case 16:
	    	tmp = (red << 11) | (green << 5) | (blue);
	    	VIDEOOUT((tmp&0xff), pTrident->keyOffset);
	    	VIDEOOUT((tmp&0xff00)>>8, (pTrident->keyOffset + 1));
	    	VIDEOOUT(0x00, (pTrident->keyOffset + 2));
		break;
	    case 24:
	    	VIDEOOUT(blue, pTrident->keyOffset);
	    	VIDEOOUT(green, (pTrident->keyOffset + 1));
	    	VIDEOOUT(red, (pTrident->keyOffset + 2));
		break;
	    }    
	}    
	REGION_EMPTY(pScrn->pScreen, &pPriv->clip);   
  } else if (attribute == xvBrightness) {
    if ((value < 0) || (value > 0x3f))
      return BadValue;
    pPriv->Brightness = value;
    tridentSetVideoParameters(pTrident, pPriv->Brightness, pPriv->Saturation,
			      pPriv->HUE);
  } else if (attribute == xvSaturation) {
    if ((value < 0) || (value > 187))
      return BadValue;
    pPriv->Saturation = value;
    tridentSetVideoParameters(pTrident, pPriv->Brightness, pPriv->Saturation,
			      pPriv->HUE);
  } else if (attribute == xvHUE) {
    if ((value < 0) || (value > 360))
      return BadValue;
    pPriv->HUE = value;
    tridentSetVideoParameters(pTrident, pPriv->Brightness, pPriv->Saturation,
			      pPriv->HUE);
  } else if (attribute == xvContrast) {
    if ((value < 0) || (value > 7))
      return BadValue;
    pPriv->Contrast = value;
    tridentSetVideoContrast(pTrident,value);
  } else
    return BadMatch;

  return Success;
}

static int 
TRIDENTGetPortAttribute(
  ScrnInfoPtr pScrn, 
  Atom attribute,
  INT32 *value, 
  pointer data
){
  TRIDENTPortPrivPtr pPriv = (TRIDENTPortPrivPtr)data;

  if(attribute == xvColorKey) {
	*value = pPriv->colorKey;
  } else if(attribute == xvBrightness) {
	*value = pPriv->Brightness;
  } else if(attribute == xvSaturation) {
	*value = pPriv->Saturation;
  } else if (attribute == xvHUE) {
	*value = pPriv->HUE;
  } else if (attribute == xvContrast) {
	*value = pPriv->Contrast;
  } else
    return BadMatch;

  return Success;
}

static void 
TRIDENTQueryBestSize(
  ScrnInfoPtr pScrn, 
  Bool motion,
  short vid_w, short vid_h, 
  short drw_w, short drw_h, 
  unsigned int *p_w, unsigned int *p_h, 
  pointer data
){
  *p_w = drw_w;
  *p_h = drw_h; 

  if(*p_w > 16384) *p_w = 16384;
}


static void
TRIDENTCopyData(
  unsigned char *src,
  unsigned char *dst,
  int srcPitch,
  int dstPitch,
  int h,
  int w
){
    w <<= 1;
    while(h--) {
	memcpy(dst, src, w);
	src += srcPitch;
	dst += dstPitch;
    }
}

static void
TRIDENTCopyMungedData(
   unsigned char *src1,
   unsigned char *src2,
   unsigned char *src3,
   unsigned char *dst1,
   int srcPitch,
   int srcPitch2,
   int dstPitch,
   int h,
   int w
){
   CARD32 *dst = (CARD32*)dst1;
   int i, j;

   dstPitch >>= 2;
   w >>= 1;

   for(j = 0; j < h; j++) {
	for(i = 0; i < w; i++) {
	    dst[i] = src1[i << 1] | (src1[(i << 1) + 1] << 16) |
		     (src3[i] << 8) | (src2[i] << 24);
	}
	dst += dstPitch;
	src1 += srcPitch;
	if(j & 1) {
	    src2 += srcPitch2;
	    src3 += srcPitch2;
	}
   }
}

static FBLinearPtr
TRIDENTAllocateMemory(
   ScrnInfoPtr pScrn,
   FBLinearPtr linear,
   int size
){
   ScreenPtr pScreen;
   FBLinearPtr new_linear;

   if(linear) {
	if(linear->size >= size) 
	   return linear;
        
        if(xf86ResizeOffscreenLinear(linear, size))
	   return linear;

	xf86FreeOffscreenLinear(linear);
   }

   pScreen = screenInfo.screens[pScrn->scrnIndex];

   new_linear = xf86AllocateOffscreenLinear(pScreen, size, 16, 
   						NULL, NULL, NULL);

   if(!new_linear) {
	int max_size;

	xf86QueryLargestOffscreenLinear(pScreen, &max_size, 16, 
						PRIORITY_EXTREME);
	
	if(max_size < size)
	   return NULL;

	xf86PurgeUnlockedOffscreenAreas(pScreen);
	new_linear = xf86AllocateOffscreenLinear(pScreen, size, 16, 
						NULL, NULL, NULL);
   }

   return new_linear;
}

static void
TRIDENTDisplayVideo(
    ScrnInfoPtr pScrn,
    int id,
    int offset,
    short width, short height,
    int pitch, 
    int x1, int y1, int x2, int y2,
    BoxPtr dstBox,
    short src_w, short src_h,
    short drw_w, short drw_h
){
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    int vgaIOBase = VGAHWPTR(pScrn)->IOBase;
    int zoomx1, zoomx2, zoomy1, zoomy2;
    int tx1,tx2;
    int ty1,ty2;
    
    switch(id) {
    case 0x35315652:		/* RGB15 */
    case 0x36315652:		/* RGB16 */
	if (pTrident->Chipset >= CYBER9388) {
    	    OUTW(vgaIOBase + 4, 0x22BF);
	    OUTW(vgaIOBase + 4, 0x248F);
	} else {
    	    OUTW(vgaIOBase + 4, 0x118F);
	}
	break;
    case FOURCC_YV12:		/* YV12 */
    case FOURCC_YUY2:		/* YUY2 */
    default:
	if (pTrident->Chipset >= CYBER9388) {
    	    OUTW(vgaIOBase + 4, 0x00BF);
	    OUTW(vgaIOBase + 4, 0x208F);
	} else {
    	    OUTW(vgaIOBase + 4, 0x108F);
	}
	break;
    }  
    tx1 = dstBox->x1 + pTrident->hsync;
    tx2 = dstBox->x2 + pTrident->hsync + pTrident->hsync_rskew; 
    ty1 = dstBox->y1 + pTrident->vsync - 2;
    ty2 = dstBox->y2 + pTrident->vsync + 2 + pTrident->vsync_bskew;

    OUTW(vgaIOBase + 4, (tx1 & 0xff) <<8 | 0x86);
    OUTW(vgaIOBase + 4, (tx1 & 0xff00)   | 0x87);
    OUTW(vgaIOBase + 4, (ty1 & 0xff) <<8 | 0x88);
    OUTW(vgaIOBase + 4, (ty1 & 0xff00)   | 0x89);
    OUTW(vgaIOBase + 4, (tx2 & 0xff) <<8 | 0x8A);
    OUTW(vgaIOBase + 4, (tx2 & 0xff00)   | 0x8B);
    OUTW(vgaIOBase + 4, (ty2 & 0xff) <<8 | 0x8C);
    OUTW(vgaIOBase + 4, (ty2 & 0xff00)   | 0x8D);

    offset += (x1 >> 15) & ~0x01;

    if (pTrident->videoFlags & VID_OFF_SHIFT_4)
        offset = offset >> 4;
    else
        offset = offset >> 3;

    OUTW(vgaIOBase + 4, (((width<<1) & 0xff)<<8)   | 0x90);
    OUTW(vgaIOBase + 4, ((width<<1) & 0xff00)      | 0x91);
    OUTW(vgaIOBase + 4, ((offset) & 0xff) << 8     | 0x92);
    OUTW(vgaIOBase + 4, ((offset) & 0xff00)        | 0x93);
    OUTW(vgaIOBase + 4, ((offset) & 0x070000) >> 8 | 0x94);
    
    /* Horizontal Zoom */
    if (pTrident->videoFlags & VID_ZOOM_INV) {
	if ((pTrident->videoFlags & VID_ZOOM_MINI) && src_w > drw_w)
	    zoomx2 = (int)((float)drw_w/(float)src_w * 1024) 
		| (((int)((float)src_w/(float)drw_w) - 1)&7)<<10 | 0x8000;
	else
	    zoomx2 = (int)(float)src_w/(float)drw_w * 1024;
	
	OUTW(vgaIOBase + 4, (zoomx2&0xff)<<8 | 0x80);
	OUTW(vgaIOBase + 4, (zoomx2&0x9f00) | 0x81);
    } else {
	if (drw_w == src_w
	    || ((pTrident->videoFlags & VID_ZOOM_NOMINI) && (src_w > drw_w))) {
	    OUTW(vgaIOBase + 4, 0x0080);
	    OUTW(vgaIOBase + 4, 0x0081);
	} else
	    if (drw_w > src_w) {
		float z;

		z = (float)((drw_w)/(float)src_w) - 1.0;
		
		zoomx1 =  z;
		zoomx2 = (z - (int)zoomx1 ) * 1024;
		
		OUTW(vgaIOBase + 4, (zoomx2&0xff)<<8 | 0x80);
		OUTW(vgaIOBase + 4, (zoomx1&0x0f)<<10 | (zoomx2&0x0300) |0x81);
	    } else {
		zoomx1 =   ((float)drw_w/(float)src_w);
		zoomx2 = ( ((float)drw_w/(float)src_w) - (int)zoomx1 ) * 1024;
		OUTW(vgaIOBase + 4, (zoomx2&0xff)<<8 |   0x80);
		OUTW(vgaIOBase + 4, (zoomx2&0x0300)|
		     (((int)((float)src_w/(float)drw_w)-1)&7)<<10 | 0x8081);
	    }
    }
    
    /* Vertical Zoom */
    if (pTrident->videoFlags & VID_ZOOM_INV) {
	if ((pTrident->videoFlags & VID_ZOOM_MINI) && src_h > drw_h)
	    zoomy2 = (int)(( ((float)drw_h/(float)src_h)) * 1024) 
		| (((int)((float)src_h/(float)drw_h)-1)&7)<<10 
		| 0x8000;
	else 
	    zoomy2 = ( ((float)src_h/(float)drw_h)) * 1024;
	OUTW(vgaIOBase + 4, (zoomy2&0xff)<<8 | 0x82);
	OUTW(vgaIOBase + 4, (zoomy2&0x9f00) | 0x0083);
    } else {
	if (drw_h == src_h
	    || ((pTrident->videoFlags & VID_ZOOM_NOMINI) && (src_h > drw_h))) {
	    OUTW(vgaIOBase + 4, 0x0082);
	    OUTW(vgaIOBase + 4, 0x0083);
	} else
	    if (drw_h > src_h) {
		float z;
		
		z = (float)drw_h/(float)src_h - 1;
		zoomy1 =  z;
		zoomy2 = (z - (int)zoomy1 ) * 1024;
		
		OUTW(vgaIOBase + 4, (zoomy2&0xff)<<8 | 0x82);
		OUTW(vgaIOBase + 4, (zoomy1&0x0f)<<10 | (zoomy2&0x0300) |0x83);
	    } else {
		zoomy1 =   ((float)drw_h/(float)src_h);
		zoomy2 = ( ((float)drw_h/(float)src_h) - (int)zoomy1 ) * 1024;
		OUTW(vgaIOBase + 4, (zoomy2&0xff)<<8 | 0x82);
		OUTW(vgaIOBase + 4, (zoomy2&0x0300)|
		     (((int)((float)src_h/(float)drw_h)-1)&7)<<10 | 0x8083);
	    }
    } 

    if (pTrident->Chipset >= CYBER9388) {
	int lb = (width+2) >> 2;

    	OUTW(vgaIOBase + 4, ((lb & 0x100)>>1) | 0x0895);
    	OUTW(vgaIOBase + 4,  (lb & 0xFF)<<8   | 0x0096);
    	if ((pTrident->videoFlags & VID_DOUBLE_LINEBUFFER_FOR_WIDE_SRC)
	      && (src_w > 384)) { 
    	    OUTW(0x3C4, 0x0497); /* 2x line buffers */ 
    	} else {
    	    OUTW(0x3C4, 0x0097); /* 1x line buffers */
    	}
    	OUTW(vgaIOBase + 4, 0x8097); 
    	OUTW(vgaIOBase + 4, 0x00BA);
    	OUTW(vgaIOBase + 4, 0x00BB);
    	OUTW(vgaIOBase + 4, 0xFFBC);
    	OUTW(vgaIOBase + 4, 0xFFBD);
    	OUTW(vgaIOBase + 4, 0x04BE); 
    	OUTW(vgaIOBase + 4, 0x948E);
    } else {
	
    	OUTW(vgaIOBase + 4, ((((id == FOURCC_YV12) || (id == FOURCC_YUY2)) 
					? (width >> 2) : (width >> 6)) << 8) | 0x95);
    	OUTW(vgaIOBase + 4, ((((id == FOURCC_YV12) || (id == FOURCC_YUY2)) 
				? ((width+2) >> 2) : ((width+2) >> 6)) << 8) |0x96);

    	OUTW(vgaIOBase + 4, 0x948E);
	OUTB(0x83C8, 0x00);
	OUTB(0x83C6, 0x95);
    }
}

static int 
TRIDENTPutImage( 
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
   TRIDENTPortPrivPtr pPriv = (TRIDENTPortPrivPtr)data;
   TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
   INT32 x1, x2, y1, y2;
   unsigned char *dst_start;
   int pitch, new_size, offset, offset2 = 0, offset3 = 0;
   int srcPitch, srcPitch2 = 0, dstPitch;
   int top, left, npixels, nlines, bpp;
   BoxRec dstBox;
   CARD32 tmp;
   
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

   dstPitch = ((width << 1) + 15) & ~15;
   new_size = ((dstPitch * height) + bpp - 1) / bpp;
   switch(id) {
   case FOURCC_YV12:
   case FOURCC_I420:
	srcPitch = (width + 3) & ~3;
	offset2 = srcPitch * height;
	srcPitch2 = ((width >> 1) + 3) & ~3;
	offset3 = (srcPitch2 * (height >> 1)) + offset2;
	break;
   case FOURCC_UYVY:
   case FOURCC_YUY2:
   default:
	srcPitch = (width << 1);
	break;
   }  

   if(!(pPriv->linear = TRIDENTAllocateMemory(pScrn, pPriv->linear, new_size)))
	return BadAlloc;

    /* copy data */
   top = y1 >> 16;
   left = (x1 >> 16) & ~1;
   npixels = ((((x2 + 0xffff) >> 16) + 1) & ~1) - left;
   left <<= 1;

   offset = pPriv->linear->offset * bpp;
   
   dst_start = pTrident->FbBase + offset + left + (top * dstPitch);

   switch(id) {
    case FOURCC_YV12:
    case FOURCC_I420:
	top &= ~1;
	tmp = ((top >> 1) * srcPitch2) + (left >> 2);
	offset2 += tmp;
	offset3 += tmp;
	if(id == FOURCC_I420) {
	   tmp = offset2;
	   offset2 = offset3;
	   offset3 = tmp;
	}
	nlines = ((((y2 + 0xffff) >> 16) + 1) & ~1) - top;
	TRIDENTCopyMungedData(buf + (top * srcPitch) + (left >> 1), 
			  buf + offset2, buf + offset3, dst_start,
			  srcPitch, srcPitch2, dstPitch, nlines, npixels);
	break;
    case FOURCC_UYVY:
    case FOURCC_YUY2:
    default:
	buf += (top * srcPitch) + left;
	nlines = ((y2 + 0xffff) >> 16) - top;
	TRIDENTCopyData(buf, dst_start, srcPitch, dstPitch, nlines, npixels);
        break;
    }

    /* update cliplist */
    if(!RegionsEqual(&pPriv->clip, clipBoxes)) {
    	/* update cliplist */
        REGION_COPY(pScreen, &pPriv->clip, clipBoxes);
        xf86XVFillKeyHelper(pScrn->pScreen, pPriv->colorKey, clipBoxes);
    }

    offset += top * dstPitch;

    tridentFixFrame(pScrn,&pPriv->fixFrame);
    TRIDENTDisplayVideo(pScrn, id, offset, width, height, dstPitch,
	     x1, y1, x2, y2, &dstBox, src_w, src_h, drw_w, drw_h);

    pPriv->videoStatus = CLIENT_VIDEO_ON;

    return Success;
}

static int 
TRIDENTQueryImageAttributes(
  ScrnInfoPtr pScrn, 
  int id, 
  unsigned short *w, unsigned short *h, 
  int *pitches, int *offsets
){
    int size, tmp;

    if(*w > 1024) *w = 1024;
    if(*h > 1024) *h = 1024;

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
  FBLinearPtr linear;
  Bool isOn;
} OffscreenPrivRec, * OffscreenPrivPtr;

static int 
TRIDENTAllocateSurface(
    ScrnInfoPtr pScrn,
    int id,
    unsigned short w, 	
    unsigned short h,
    XF86SurfacePtr surface
){
    FBLinearPtr linear;
    int pitch, fbpitch, size, bpp;
    OffscreenPrivPtr pPriv;

    if((w > 1024) || (h > 1024))
	return BadAlloc;

    w = (w + 1) & ~1;
    pitch = ((w << 1) + 15) & ~15;
    bpp = pScrn->bitsPerPixel >> 3;
    fbpitch = bpp * pScrn->displayWidth;
    size = ((pitch * h) + bpp - 1) / bpp;

    if(!(linear = TRIDENTAllocateMemory(pScrn, NULL, size)))
	return BadAlloc;

    surface->width = w;
    surface->height = h;

    if(!(surface->pitches = xalloc(sizeof(int)))) {
	xf86FreeOffscreenLinear(linear);
	return BadAlloc;
    }
    if(!(surface->offsets = xalloc(sizeof(int)))) {
	xfree(surface->pitches);
	xf86FreeOffscreenLinear(linear);
	return BadAlloc;
    }
    if(!(pPriv = xalloc(sizeof(OffscreenPrivRec)))) {
	xfree(surface->pitches);
	xfree(surface->offsets);
	xf86FreeOffscreenLinear(linear);
	return BadAlloc;
    }

    pPriv->linear = linear;
    pPriv->isOn = FALSE;

    surface->pScrn = pScrn;
    surface->id = id;   
    surface->pitches[0] = pitch;
    surface->offsets[0] = linear->offset * bpp;
    surface->devPrivate.ptr = (pointer)pPriv;

    return Success;
}

static int 
TRIDENTStopSurface(
    XF86SurfacePtr surface
){
    OffscreenPrivPtr pPriv = (OffscreenPrivPtr)surface->devPrivate.ptr;

    if(pPriv->isOn) {
	TRIDENTPtr pTrident = TRIDENTPTR(surface->pScrn);
    	int vgaIOBase = VGAHWPTR(surface->pScrn)->IOBase;

	OUTW(vgaIOBase + 4, 0x0091);
	WaitForSync(surface->pScrn);
 	OUTW(vgaIOBase + 4, 0x848E);
	pPriv->isOn = FALSE;
    }

    return Success;
}


static int 
TRIDENTFreeSurface(
    XF86SurfacePtr surface
){
    OffscreenPrivPtr pPriv = (OffscreenPrivPtr)surface->devPrivate.ptr;

    if(pPriv->isOn)
	TRIDENTStopSurface(surface);
    xf86FreeOffscreenLinear(pPriv->linear);
    xfree(surface->pitches);
    xfree(surface->offsets);
    xfree(surface->devPrivate.ptr);

    return Success;
}

static int
TRIDENTGetSurfaceAttribute(
    ScrnInfoPtr pScrn,
    Atom attribute,
    INT32 *value
){
    return TRIDENTGetPortAttribute(pScrn, attribute, value, 
			(pointer)(GET_PORT_PRIVATE(pScrn)));
}

static int
TRIDENTSetSurfaceAttribute(
    ScrnInfoPtr pScrn,
    Atom attribute,
    INT32 value
){
    return TRIDENTSetPortAttribute(pScrn, attribute, value, 
			(pointer)(GET_PORT_PRIVATE(pScrn)));
}

static int 
TRIDENTDisplaySurface(
    XF86SurfacePtr surface,
    short src_x, short src_y, 
    short drw_x, short drw_y,
    short src_w, short src_h, 
    short drw_w, short drw_h,
    RegionPtr clipBoxes
){
    OffscreenPrivPtr pPriv = (OffscreenPrivPtr)surface->devPrivate.ptr;
    ScrnInfoPtr pScrn = surface->pScrn;
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    TRIDENTPortPrivPtr portPriv = pTrident->adaptor->pPortPrivates[0].ptr;
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

    TRIDENTResetVideo(pScrn);

    tridentFixFrame(pScrn,&portPriv->fixFrame);
    TRIDENTDisplayVideo(pScrn, surface->id, surface->offsets[0], 
	     surface->width, surface->height, surface->pitches[0],
	     x1, y1, x2, y2, &dstBox, src_w, src_h, drw_w, drw_h);

    xf86XVFillKeyHelper(pScrn->pScreen, portPriv->colorKey, clipBoxes);

    pPriv->isOn = TRUE;
    /* we've prempted the XvImage stream so set its free timer */
    if(portPriv->videoStatus & CLIENT_VIDEO_ON) {
	REGION_EMPTY(pScrn->pScreen, &portPriv->clip);   
	UpdateCurrentTime();
	portPriv->videoStatus = FREE_TIMER;
	portPriv->freeTime = currentTime.milliseconds + FREE_DELAY;
	pTrident->VideoTimerCallback = TRIDENTVideoTimerCallback;
    }

    return Success;
}

static void 
TRIDENTInitOffscreenImages(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    XF86OffscreenImagePtr offscreenImages;

    /* need to free this someplace */
    if(!(offscreenImages = xalloc(sizeof(XF86OffscreenImageRec))))
	return;

    offscreenImages[0].image = &Images[0];
    offscreenImages[0].flags = VIDEO_OVERLAID_IMAGES | 
			       VIDEO_CLIP_TO_VIEWPORT;
    offscreenImages[0].alloc_surface = TRIDENTAllocateSurface;
    offscreenImages[0].free_surface = TRIDENTFreeSurface;
    offscreenImages[0].display = TRIDENTDisplaySurface;
    offscreenImages[0].stop = TRIDENTStopSurface;
    offscreenImages[0].setAttribute = TRIDENTSetSurfaceAttribute;
    offscreenImages[0].getAttribute = TRIDENTGetSurfaceAttribute;
    offscreenImages[0].max_width = 1024;
    offscreenImages[0].max_height = 1024;
    if (pTrident->Chipset >= CYBER9388) {
    	offscreenImages[0].num_attributes = NUM_ATTRIBUTES;
    } else {
    	offscreenImages[0].num_attributes = 1; /* just colorkey */
    }
    offscreenImages[0].attributes = Attributes;
    
    xf86XVRegisterOffscreenImages(pScreen, offscreenImages, 1);
}

static void
TRIDENTVideoTimerCallback(ScrnInfoPtr pScrn, Time time)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    TRIDENTPortPrivPtr pPriv = pTrident->adaptor->pPortPrivates[0].ptr;
    int vgaIOBase = VGAHWPTR(pScrn)->IOBase;

    if(pPriv->videoStatus & TIMER_MASK) {
	if(pPriv->videoStatus & OFF_TIMER) {
	    if(pPriv->offTime < time) {
		OUTW(vgaIOBase + 4, 0x0091);
		WaitForSync(pScrn);
  		OUTW(vgaIOBase + 4, 0x848E);
		pPriv->videoStatus = FREE_TIMER;
		pPriv->freeTime = time + FREE_DELAY;
	    }
	} else {  /* FREE_TIMER */
	    if(pPriv->freeTime < time) {
		if(pPriv->linear) {
		   xf86FreeOffscreenLinear(pPriv->linear);
		   pPriv->linear = NULL;
		}
		pPriv->videoStatus = 0;
	        pTrident->VideoTimerCallback = NULL;
	    }
        }
    } else  /* shouldn't get here */
	pTrident->VideoTimerCallback = NULL;
}

    /* Calculate skew offsets for video overlay */


void
tridentFixFrame(ScrnInfoPtr pScrn, int *fixFrame)
{

    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    int vgaIOBase = VGAHWPTR(pScrn)->IOBase;
    int HTotal, HSyncStart;
    int VTotal, VSyncStart;
    int h_off = 0;
    int v_off = 0;
    unsigned char CRTC[0x11];
    Bool isShadow;
    unsigned char shadow = 0;

    if ((*fixFrame)++ < 100) 
	return;
  
  *fixFrame = 0;

  OUTB(0x3CE, CyberControl);
  isShadow = ((INB(0x3CF) & 0x81) == 0x81);
  
  if (isShadow)
      SHADOW_ENABLE(shadow);

    OUTB(vgaIOBase + 4, 0x0);
    CRTC[0x0] = INB(vgaIOBase + 5);
    OUTB(vgaIOBase + 4, 0x4);
    CRTC[0x4] = INB(vgaIOBase + 5);
    OUTB(vgaIOBase + 4, 0x5);
    CRTC[0x5] = INB(vgaIOBase + 5);
    OUTB(vgaIOBase + 4, 0x6);
    CRTC[0x6] = INB(vgaIOBase + 5);
    OUTB(vgaIOBase + 4, 0x7);
    CRTC[0x7] = INB(vgaIOBase + 5);
    OUTB(vgaIOBase + 4, 0x10);
    CRTC[0x10] = INB(vgaIOBase + 5);

    HTotal = CRTC[0] << 3;
    VTotal = CRTC[6] 
	| ((CRTC[7] & (1<<0)) << 8)
	| ((CRTC[7] & (1<<5)) << 4);
    HSyncStart = (CRTC[4] 
		  + ((CRTC[5] >> 5) & 0x3)) << 3;
    VSyncStart = CRTC[0x10] 
	| ((CRTC[7] & (1<<2)) << 6)
	| ((CRTC[7] & (1<<7)) << 2);

    if (isShadow) {
	SHADOW_RESTORE(shadow);
	if (pTrident->lcdMode != 0xff) {
	    h_off = (LCD[pTrident->lcdMode].display_x 
		     - pScrn->currentMode->HDisplay) >> 1;
	    v_off = (LCD[pTrident->lcdMode].display_y 
		     - pScrn->currentMode->VDisplay) >> 1;
	}
    } 

    pTrident->hsync = (HTotal - HSyncStart) + 23 + h_off;
    pTrident->vsync = (VTotal - VSyncStart) - 2 + v_off;
    pTrident->hsync_rskew = 0;
    pTrident->vsync_bskew = 0;
  
    /* 
     * HACK !! As awful as this is, it appears to be the only way....Sigh!
     * We have XvHsync and XvVsync as options now, which adjust 
     * at the very end of this function. It'll be helpful for now
     * and we can get more data on some of these skew values.
     */
    switch (pTrident->Chipset) {
	case TGUI9680:
	    /* Furthur tweaking needed */
    	    pTrident->hsync -= 84;
	    pTrident->vsync += 2;
	    break;
	case PROVIDIA9682:
	    /* Furthur tweaking needed */
	    pTrident->hsync += 7;
	    break;
	case PROVIDIA9685:
	    /* Spot on */
	    break;
        case BLADEXP:
        case CYBERBLADEXPAI1:
	    pTrident->hsync -= 15;
	    pTrident->hsync_rskew = 3;
	    break;
	case BLADE3D:
	    if (pScrn->depth == 24)
		pTrident->hsync -= 8;
	    else
		pTrident->hsync -= 6;
	    break;
	case CYBERBLADEI7:
	case CYBERBLADEI7D:
	case CYBERBLADEI1:
	case CYBERBLADEI1D:
	    pTrident->hsync -= 8;
	    break;
	case CYBERBLADEAI1:
	    pTrident->hsync -= 7;
	    break;
	case CYBERBLADEAI1D:
	    pTrident->vsync += 2;
	    pTrident->vsync_bskew = -4;
	    pTrident->hsync -= 5;
	    break;
	case CYBERBLADEE4:
	    pTrident->hsync -= 8;
	    break;
	case CYBER9397:
	    pTrident->hsync -= 1;
  	    pTrident->vsync -= 0;	     
	    pTrident->vsync_bskew = 0;
	    break;
    case CYBER9397DVD:
	    pTrident->hsync_rskew = -1;
	    pTrident->vsync_bskew = -1;
	    break;
    }
    pTrident->hsync+=pTrident->OverrideHsync;
    pTrident->vsync+=pTrident->OverrideVsync;
    pTrident->hsync_rskew += pTrident->OverrideRskew;
    pTrident->vsync_bskew += pTrident->OverrideBskew;
}
    
static void
WaitForSync(ScrnInfoPtr	pScrn)
{
    register vgaHWPtr hwp = VGAHWPTR(pScrn);

    while (!(hwp->readST01(hwp)&0x8)) {};
    while (hwp->readST01(hwp)&0x8) {};
}
