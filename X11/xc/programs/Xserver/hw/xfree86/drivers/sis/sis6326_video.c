/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sis/sis6326_video.c,v 1.2 2003/01/29 15:42:16 eich Exp $ */
/*
 * Xv driver for SiS 5597/5598, 6236 and 530/620.
 *
 * Copyright 2002 by Thomas Winischhofer, Vienna, Austria.
 *
 * Based on sis_video.c which is
 *    Copyright 2000 Silicon Integrated Systems Corp, Inc., HsinChu, Taiwan.
 *    Parts Copyright 2002 by Thomas Winischhofer, Vienna, Austria.
 *    All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL INTEL, AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Author:
 *      Thomas Winischhofer <thomas@winischhofer.net>
 */

#include "sis.h"
#ifdef USE6326VIDEO

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86Resources.h"
#include "xf86_ansic.h"
#include "compiler.h"
#include "xf86PciInfo.h"
#include "xf86Pci.h"
#include "xf86fbman.h"
#include "regionstr.h"

#include "xf86xv.h"
#include "Xv.h"
#include "xaa.h"
#include "xaalocal.h"
#include "dixstruct.h"
#include "fourcc.h"

#include "sis_regs.h"

#define OFF_DELAY   	200  /* milliseconds */
#define FREE_DELAY  	60000

#define OFF_TIMER   	0x01
#define FREE_TIMER  	0x02
#define CLIENT_VIDEO_ON 0x04

#define TIMER_MASK      (OFF_TIMER | FREE_TIMER)

#define WATCHDOG_DELAY  500000 /* Watchdog counter for Vertical Restrace waiting */

static 		XF86VideoAdaptorPtr SIS6326SetupImageVideo(ScreenPtr);
static void 	SIS6326StopVideo(ScrnInfoPtr, pointer, Bool);
static int 	SIS6326SetPortAttribute(ScrnInfoPtr, Atom, INT32, pointer);
static int 	SIS6326GetPortAttribute(ScrnInfoPtr, Atom ,INT32 *, pointer);
static void 	SIS6326QueryBestSize(ScrnInfoPtr, Bool, short, short, short,
			short, unsigned int *,unsigned int *, pointer);
static int 	SIS6326PutImage( ScrnInfoPtr,
    			short, short, short, short, short, short, short, short,
    			int, unsigned char*, short, short, Bool, RegionPtr, pointer);
static int 	SIS6326QueryImageAttributes(ScrnInfoPtr,
    			int, unsigned short *, unsigned short *, int *, int *);
static void 	SIS6326VideoTimerCallback(ScrnInfoPtr pScrn, Time now);
static void     SIS6326InitOffscreenImages(ScreenPtr pScrn);

#define MAKE_ATOM(a) MakeAtom(a, sizeof(a) - 1, TRUE)

static Atom xvBrightness, xvContrast, xvColorKey;
static Atom xvAutopaintColorKey, xvSetDefaults;

#define IMAGE_MIN_WIDTH        32  /* Minimum and maximum image sizes */
#define IMAGE_MIN_HEIGHT       24
#define IMAGE_MAX_WIDTH       720  /* Are these correct for the chips ? */
#define IMAGE_MAX_HEIGHT      576
#define IMAGE_MAX_WIDTH_5597  384  
#define IMAGE_MAX_HEIGHT_5597 288

#if 0
static int oldH, oldW;
#endif

/****************************************************************************
 * Raw register access : These routines directly interact with the sis's
 *                       control aperature.  Must not be called until after
 *                       the board's pci memory has been mapped.
 ****************************************************************************/

#if 0
static CARD32 _sisread(SISPtr pSiS, CARD32 reg)
{
    return *(pSiS->IOBase + reg);
}

static void _siswrite(SISPtr pSiS, CARD32 reg, CARD32 data)
{
    *(pSiS->IOBase + reg) = data;
}
#endif

static CARD8 getvideoreg(SISPtr pSiS, CARD8 reg)
{
    CARD8 ret;
    inSISIDXREG(SISCR, reg, ret);
    return(ret);
}

static void setvideoreg(SISPtr pSiS, CARD8 reg, CARD8 data)
{
    outSISIDXREG(SISCR, reg, data);
}

static void setvideoregmask(SISPtr pSiS, CARD8 reg, CARD8 data, CARD8 mask)
{
    CARD8   old;

    inSISIDXREG(SISCR, reg, old);
    data = (data & mask) | (old & (~mask));
    outSISIDXREG(SISCR, reg, data);
}

/* VBlank */
static CARD8 vblank_active_CRT1(SISPtr pSiS)
{
    return (inSISREG(SISINPSTAT) & 0x08);
}

/* Scanline - unused */
#if 0
static CARD32 get_scanline_CRT1(SISPtr pSiS)
{
    CARD8 temp;

    temp = getvideoreg(pSiS, 0x20);
    temp = getvideoreg(pSiS, 0x1b);
    return((getvideoreg(pSiS, 0x1d) << 8) | getvideoreg(pSiS, 0x1c));
}
#endif

void SIS6326InitVideo(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    XF86VideoAdaptorPtr *adaptors, *newAdaptors = NULL;
    XF86VideoAdaptorPtr newAdaptor = NULL;
    int num_adaptors;

    newAdaptor = SIS6326SetupImageVideo(pScreen);
    if(newAdaptor)
        SIS6326InitOffscreenImages(pScreen);

    num_adaptors = xf86XVListGenericAdaptors(pScrn, &adaptors);

    if(newAdaptor) {
    	if(!num_adaptors) {
        	num_adaptors = 1;
        	adaptors = &newAdaptor;
    	} else {
        	/* need to free this someplace */
        	newAdaptors = xalloc((num_adaptors + 1) * sizeof(XF86VideoAdaptorPtr*));
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
#if 0
    oldW = 0; oldH = 0;  /* DEBUG */
#endif
}

/* client libraries expect an encoding */
static XF86VideoEncodingRec DummyEncoding =
{
   0,
   "XV_IMAGE",
   IMAGE_MAX_WIDTH, IMAGE_MAX_HEIGHT,
   {1, 1}
};

static XF86VideoEncodingRec DummyEncoding5597 =
{
   0,
   "XV_IMAGE",
   IMAGE_MAX_WIDTH_5597, IMAGE_MAX_HEIGHT_5597,
   {1, 1}
};

#define NUM_FORMATS 4

static XF86VideoFormatRec SIS6326Formats[NUM_FORMATS] =
{
   { 8, PseudoColor},
   {15, TrueColor},
   {16, TrueColor},
   {24, TrueColor}
};

#define NUM_ATTRIBUTES 5

static XF86AttributeRec SIS6326Attributes[NUM_ATTRIBUTES] =
{
   {XvSettable | XvGettable, 0, (1 << 24) - 1, "XV_COLORKEY"},
   {XvSettable | XvGettable, -128, 127,        "XV_BRIGHTNESS"},
   {XvSettable | XvGettable, 0, 7,             "XV_CONTRAST"},
   {XvSettable | XvGettable, 0, 1,             "XV_AUTOPAINT_COLORKEY"},
   {XvSettable             , 0, 0,             "XV_SET_DEFAULTS"}
};

#define NUM_IMAGES 6
#define NUM_IMAGES_NOYV12 4
#define PIXEL_FMT_YV12 FOURCC_YV12  /* 0x32315659 */
#define PIXEL_FMT_UYVY FOURCC_UYVY  /* 0x59565955 */
#define PIXEL_FMT_YUY2 FOURCC_YUY2  /* 0x32595559 */
#define PIXEL_FMT_I420 FOURCC_I420  /* 0x30323449 */
#define PIXEL_FMT_RGB5 0x35315652
#define PIXEL_FMT_RGB6 0x36315652

static XF86ImageRec SIS6326Images[NUM_IMAGES] =
{
    XVIMAGE_YUY2, /* TW: If order is changed, SIS6326OffscreenImages must be adapted */
    XVIMAGE_UYVY,
    XVIMAGE_YV12,
    XVIMAGE_I420,
    {
      0x35315652,
      XvRGB,
      LSBFirst,
      {'R','V','1','5',
       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
      16,
      XvPacked,
      1,
/*    15, 0x001F, 0x03E0, 0x7C00,  - incorrect! */
      15, 0x7C00, 0x03E0, 0x001F,  
      0, 0, 0,
      0, 0, 0,
      0, 0, 0,
      {'R', 'V', 'B',0,
       0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
      XvTopToBottom
    },
    {
      0x36315652,
      XvRGB,
      LSBFirst,
      {'R','V','1','6',
       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
      16,
      XvPacked,
      1,
/*    16, 0x001F, 0x07E0, 0xF800,  - incorrect!  */
      16, 0xF800, 0x07E0, 0x001F, 
      0, 0, 0,
      0, 0, 0,
      0, 0, 0,
      {'R', 'V', 'B',0,
       0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
      XvTopToBottom
    }
};

static XF86ImageRec SIS6326ImagesNoYV12[NUM_IMAGES_NOYV12] =
{
    XVIMAGE_YUY2, /* TW: If order is changed, SIS6326OffscreenImages must be adapted */
    XVIMAGE_UYVY,
    {
      0x35315652,
      XvRGB,
      LSBFirst,
      {'R','V','1','5',
       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
      16,
      XvPacked,
      1,
/*    15, 0x001F, 0x03E0, 0x7C00,  */
      15, 0x7C00, 0x03E0, 0x001F,  /* TW: Should be more correct than the other... */
      0, 0, 0,
      0, 0, 0,
      0, 0, 0,
      {'R', 'V', 'B',0,
       0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
      XvTopToBottom
    },
    {
      0x36315652,
      XvRGB,
      LSBFirst,
      {'R','V','1','6',
       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
      16,
      XvPacked,
      1,
/*    16, 0x001F, 0x07E0, 0xF800,   */
      16, 0xF800, 0x07E0, 0x001F,   /* TW: Should be more correct than the other... */
      0, 0, 0,
      0, 0, 0,
      0, 0, 0,
      {'R', 'V', 'B',0,
       0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
      XvTopToBottom
    }
};

typedef struct {
    int pixelFormat;

    CARD16  pitch;

    CARD8   keyOP;

    CARD8   HUSF;
    CARD8   VUSF;
    CARD8   HIntBit;
    CARD8   wHPre;
    CARD8   PitchMult;

    CARD16  srcW;
    CARD16  srcH;

    BoxRec  dstBox;

    CARD32  PSY;
    CARD32  PSV;
    CARD32  PSU;
    CARD8   YUVEnd;

    CARD8   lineBufSize;

    CARD8   (*VBlankActiveFunc)(SISPtr);
/*  CARD32  (*GetScanLineFunc)(SISPtr pSiS); */

} SISOverlayRec, *SISOverlayPtr;

typedef struct {
    FBLinearPtr  linear;	/* TW: We now use Linear, not Area */
    CARD32       bufAddr[2];

    unsigned char currentBuf;

    short  drw_x, drw_y, drw_w, drw_h;
    short  src_x, src_y, src_w, src_h;
    int    id;
    short  srcPitch, height, width;
    CARD32 totalSize;
    
    char          brightness;
    unsigned char contrast;

    RegionRec    clip;
    CARD32       colorKey;
    Bool 	 autopaintColorKey;

    CARD32       videoStatus;
    Time         offTime;
    Time         freeTime;

    short        oldx1, oldx2, oldy1, oldy2;
    int          mustwait;

    Bool         grabbedByV4L;	   /* V4L stuff */
    int          pitch;
    int          offset;

} SISPortPrivRec, *SISPortPrivPtr;        

#define GET_PORT_PRIVATE(pScrn) \
   (SISPortPrivPtr)((SISPTR(pScrn))->adaptor->pPortPrivates[0].ptr)

static void
SIS6326SetPortDefaults (ScrnInfoPtr pScrn, SISPortPrivPtr pPriv)
{
    pPriv->colorKey    = 0x000101fe;
    pPriv->videoStatus = 0;
    pPriv->brightness  = 0;
    pPriv->contrast    = 4;
    pPriv->autopaintColorKey = TRUE;
}

static void
SIS6326ResetVideo(ScrnInfoPtr pScrn)
{
    SISPtr pSiS = SISPTR(pScrn);

    /* Unlock registers */
#ifdef UNLOCK_ALWAYS
    sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif
    if(getvideoreg (pSiS, Index_VI6326_Passwd) != 0xa1) {
        setvideoreg (pSiS, Index_VI6326_Passwd, 0x86);
        if(getvideoreg (pSiS, Index_VI6326_Passwd) != 0xa1)
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                       "Xv: Video password could not unlock video registers\n");
    }

    /* Initialize the overlay ----------------------------------- */

    switch(pSiS->Chipset) {
    case PCI_CHIP_SIS6326:
       /* Disable overlay (D[1]) & capture (D[0]) */
       setvideoregmask(pSiS, Index_VI6326_Control_Misc0,      0x00, 0x03);

       /* What do these do? (Datasheet names these bits "reserved") */
       setvideoregmask(pSiS, Index_VI6326_Control_Misc0,      0x00, 0x18);
       setvideoregmask(pSiS, Index_VI6326_Control_Misc0,      0x00, 0x0c);

       /* Select YUV format (D[6]) and "gfx + video" mode (D[4]), odd polarity? (D[7]) */
       setvideoregmask(pSiS, Index_VI6326_Control_Misc0,      0x40, 0xD0);
       /* No interrupt, no filter, disable dithering */
       setvideoregmask(pSiS, Index_VI6326_Control_Misc1,      0x00, 0x7A);
       /* Disable VMI (D[4:3]), Brooktree support (D[6]) and system memory framebuffer (D[7]) */
       setvideoregmask(pSiS, Index_VI6326_Control_Misc3,      0x00, 0xF8);
       /* Disable video decimation */
       setvideoregmask(pSiS, Index_VI6326_Control_Misc6,      0x00, 0x80);
       break;
    case PCI_CHIP_SIS5597:
       /* Disable overlay (D[1]) & capture (D[0]) */
       setvideoregmask(pSiS, Index_VI6326_Control_Misc0,      0x00, 0x03);

       /* What do these do? (Datasheet names these bits "reserved") */
       setvideoregmask(pSiS, Index_VI6326_Control_Misc0,      0x00, 0x18);
       setvideoregmask(pSiS, Index_VI6326_Control_Misc0,      0x00, 0x0c);

       /* Select YUV format (D[6]) and "gfx + video" mode (D[4]), odd polarity? (D[7]) */
       setvideoregmask(pSiS, Index_VI6326_Control_Misc0,      0x40, 0xD0);
       /* No interrupt, no filter, disable dithering */
       setvideoregmask(pSiS, Index_VI6326_Control_Misc1,      0x00, 0x7A);
       /* Disable Brooktree support (D[6]) and system memory framebuffer (D[7]) */
       setvideoregmask(pSiS, Index_VI6326_Control_Misc3,      0x00, 0xC0);
       /* Disable video decimation (has a really strange effect if enabled) */
       setvideoregmask(pSiS, Index_VI6326_Control_Misc6,      0x00, 0x80);
       break;
    case PCI_CHIP_SIS530:
       /* What is this? (Bit is "reserved") */
       setvideoregmask (pSiS, Index_VI6326_Control_Misc4,     0x40, 0x40);
       /* Disable overlay (D[1]) */
       setvideoregmask(pSiS, Index_VI6326_Control_Misc0,      0x00, 0x02);

       /* What do these do? (Datasheet names these bits "reserved") */
       setvideoregmask(pSiS, Index_VI6326_Control_Misc0,      0x00, 0x18);
       setvideoregmask(pSiS, Index_VI6326_Control_Misc0,      0x00, 0x0c);

       /* Select YUV format (D[6]) and "gfx + video" mode (D[4]) */
       setvideoregmask(pSiS, Index_VI6326_Control_Misc0,      0x40, 0x50);
       break;
    default:
    	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		"Internal error: SiS6326ResetVideo() called with invalid chipset (%x)\n",
		pSiS->Chipset);
        return;
    }

    /* Clear format selection */
    setvideoregmask(pSiS, Index_VI6326_Control_Misc1,         0x00, 0x04);
    setvideoregmask(pSiS, Index_VI6326_Control_Misc4,         0x00, 0x07);

    /* Select RGB Chromakey format (D[2]=0), CCIR 601 UV data format (D[1]=0) */
    /* D[1]: 1 = 2's complement, 0 = CCIR 601 format */
    setvideoregmask(pSiS, Index_VI6326_Control_Misc3,         0x00, 0x06);

    /* Reset contrast control */
    setvideoregmask(pSiS, Index_VI6326_Contrast_Enh_Ctrl,     0x04, 0x1F);

    /* Set treshold */
    if(pSiS->oldChipset < OC_SIS6326) {
       CARD8 temp;
       inSISIDXREG(SISSR, 0x33, temp);  /* Synchronous DRAM Timing? */
       if(temp & 0x01) temp = 0x50;
       else            temp = 0;
       setvideoreg(pSiS, Index_VI6326_Play_Threshold_Low,     temp);
       setvideoreg(pSiS, Index_VI6326_Play_Threshold_High,    temp);
    } else {
       CARD8 temp;
       setvideoreg(pSiS, Index_VI6326_Play_Threshold_Low,     0x00);
       setvideoreg(pSiS, Index_VI6326_Play_Threshold_High,    0x00);
       inSISIDXREG(SISSR, 0x33, temp);  /* Are we using SGRAM Timing? */
       if(temp & 0x01) temp = 0x10;
       else            temp = 0;
       setvideoregmask(pSiS, Index_VI6326_Control_Misc4,      temp, 0x10);
    }

    /* set default properties for overlay     ------------------------------- */

    setvideoregmask (pSiS, Index_VI6326_Contrast_Enh_Ctrl,    0x04, 0x07);
    setvideoreg (pSiS, Index_VI6326_Brightness,               0x20);

    if(pSiS->oldChipset < OC_SIS6205A || pSiS->oldChipset > OC_SIS82204) {
       setvideoregmask(pSiS, Index_VI6326_AlphaGraph,         0x00, 0xF8);
       setvideoregmask(pSiS, Index_VI6326_AlphaVideo,         0xF8, 0xF8);
    } else {
       setvideoregmask(pSiS, Index_VI6326_AlphaGraph,         0x00, 0xE1);
       setvideoregmask(pSiS, Index_VI6326_AlphaVideo,         0xE1, 0xE1);
    }
}

static XF86VideoAdaptorPtr
SIS6326SetupImageVideo(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    SISPtr pSiS = SISPTR(pScrn);
    XF86VideoAdaptorPtr adapt;
    SISPortPrivPtr pPriv;

    if(!(adapt = xcalloc(1, sizeof(XF86VideoAdaptorRec) +
                            sizeof(SISPortPrivRec) +
                            sizeof(DevUnion))))
    	return NULL;

    adapt->type = XvWindowMask | XvInputMask | XvImageMask;
    adapt->flags = VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT;
    adapt->name = "SIS 5597/5598/6326/530/620 Video Overlay";
    adapt->nEncodings = 1;
    if(pSiS->oldChipset < OC_SIS6326) {
      adapt->pEncodings = &DummyEncoding5597;
    } else {
      adapt->pEncodings = &DummyEncoding;
    }
    adapt->nFormats = NUM_FORMATS;
    adapt->pFormats = SIS6326Formats;
    adapt->nPorts = 1;
    adapt->pPortPrivates = (DevUnion*)(&adapt[1]);

    pPriv = (SISPortPrivPtr)(&adapt->pPortPrivates[1]);

    adapt->pPortPrivates[0].ptr = (pointer)(pPriv);
    adapt->pAttributes = SIS6326Attributes;
    adapt->nAttributes = NUM_ATTRIBUTES;
    if(pSiS->NoYV12 == 1) {
       adapt->nImages = NUM_IMAGES_NOYV12;
       adapt->pImages = SIS6326ImagesNoYV12;
    } else {
       adapt->nImages = NUM_IMAGES;
       adapt->pImages = SIS6326Images;
    }
    adapt->PutVideo = NULL;
    adapt->PutStill = NULL;
    adapt->GetVideo = NULL;
    adapt->GetStill = NULL;
    adapt->StopVideo = SIS6326StopVideo;
    adapt->SetPortAttribute = SIS6326SetPortAttribute;
    adapt->GetPortAttribute = SIS6326GetPortAttribute;
    adapt->QueryBestSize = SIS6326QueryBestSize;
    adapt->PutImage = SIS6326PutImage;
    adapt->QueryImageAttributes = SIS6326QueryImageAttributes;

    pPriv->videoStatus = 0;
    pPriv->currentBuf  = 0;
    pPriv->linear      = NULL;
    pPriv->grabbedByV4L= FALSE;

    SIS6326SetPortDefaults(pScrn, pPriv);

    /* gotta uninit this someplace */
    REGION_INIT(pScreen, &pPriv->clip, NullBox, 0); 

    pSiS->adaptor = adapt;

    xvBrightness = MAKE_ATOM("XV_BRIGHTNESS");
    xvContrast   = MAKE_ATOM("XV_CONTRAST");
    xvColorKey   = MAKE_ATOM("XV_COLORKEY");
    xvAutopaintColorKey = MAKE_ATOM("XV_AUTOPAINT_COLORKEY");
    xvSetDefaults       = MAKE_ATOM("XV_SET_DEFAULTS");

    SIS6326ResetVideo(pScrn);

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

static int
SIS6326SetPortAttribute(ScrnInfoPtr pScrn, Atom attribute,
  		    INT32 value, pointer data)
{
  SISPortPrivPtr pPriv = (SISPortPrivPtr)data;

  if(attribute == xvBrightness) {
    if((value < -128) || (value > 127))
       return BadValue;
    pPriv->brightness = value;
  } else if(attribute == xvContrast) {
    if((value < 0) || (value > 7))
       return BadValue;
    pPriv->contrast = value;
  } else if(attribute == xvColorKey) {
    pPriv->colorKey = value;
    REGION_EMPTY(pScrn->pScreen, &pPriv->clip);
  } else if (attribute == xvAutopaintColorKey) {
     if ((value < 0) || (value > 1))
       return BadValue;
     pPriv->autopaintColorKey = value;
  } else if (attribute == xvSetDefaults) {
        SIS6326SetPortDefaults(pScrn, pPriv);
  } else return BadMatch;
  return Success;
}

static int 
SIS6326GetPortAttribute(
  ScrnInfoPtr pScrn, 
  Atom attribute,
  INT32 *value, 
  pointer data
){
  SISPortPrivPtr pPriv = (SISPortPrivPtr)data;

  if(attribute == xvBrightness) {
    *value = pPriv->brightness;
  } else if(attribute == xvContrast) {
    *value = pPriv->contrast;
  } else if(attribute == xvColorKey) {
    *value = pPriv->colorKey;
  } else if (attribute == xvAutopaintColorKey)
    *value = (pPriv->autopaintColorKey) ? 1 : 0;
  else return BadMatch;
  return Success;
}

static void 
SIS6326QueryBestSize(
  ScrnInfoPtr pScrn, 
  Bool motion,
  short vid_w, short vid_h, 
  short drw_w, short drw_h, 
  unsigned int *p_w, unsigned int *p_h, 
  pointer data
){
  *p_w = drw_w;
  *p_h = drw_h; 

  /* TODO: report the HW limitation */
}

static void  /* V 530/6326 */
calc_scale_factor(SISPtr pSiS, SISOverlayPtr pOverlay, ScrnInfoPtr pScrn,
                 SISPortPrivPtr pPriv)
{
  CARD32 temp=0;

  int dstW = pOverlay->dstBox.x2 - pOverlay->dstBox.x1;
  int dstH = pOverlay->dstBox.y2 - pOverlay->dstBox.y1;
  int srcW = pOverlay->srcW;
  int srcH = pOverlay->srcH;

#if 0
  /* DEBUG */
  if((oldH != dstH) || (oldW != dstW)){
  	xf86DrvMsg(0, X_INFO, "Video size %dx%d\n", dstW, dstH);
	oldH = dstH; oldW = dstW;
  }
  /* /DEBUG */
#endif

  /* TW: For double scan modes, we need to double the height */
  if(pSiS->CurrentLayout.mode->Flags & V_DBLSCAN) {
	dstH <<= 1;
  }
  /* TW: For interlace modes, we need to half the height */
  if(pSiS->CurrentLayout.mode->Flags & V_INTERLACE) {
	dstH >>= 1;
  }

  /* Horizontal */
  if(dstW < IMAGE_MIN_WIDTH) dstW = IMAGE_MIN_WIDTH;
  if(dstW == srcW) {
        pOverlay->HUSF    = 0x00;
        pOverlay->HIntBit = 0x01;
  } else if(dstW > srcW) {
	pOverlay->HIntBit = 0x00;
	temp = srcW * 64 / (dstW + 1);
	if(temp > 63) temp = 63;
	pOverlay->HUSF = temp;
  } else {
  	/* TW: 6326 can't scale below factor .440 - to check with 530/620 */
        if(((dstW * 1000) / srcW) < 440) dstW = ((srcW * 440) / 1000) + 1;
	temp = srcW / dstW;
	if(temp > 15) temp = 15;
	pOverlay->HIntBit = temp;
	temp = srcW * 64 / dstW;
	pOverlay->HUSF = temp - (pOverlay->HIntBit * 64);
  }

  /* Vertical */
  if(dstH < IMAGE_MIN_HEIGHT) dstH = IMAGE_MIN_HEIGHT;
  if(dstH == srcH) {
        pOverlay->VUSF = 0x00;
	pOverlay->PitchMult = 1;
  } else if(dstH > srcH) {
  	temp = srcH * 64 / (dstH + 1);
	if (temp > 63) temp = 63;
	pOverlay->VUSF = temp;
	pOverlay->PitchMult = 1;
  } else {
        /* TW: 6326 can't scale below factor .440 - to check with 530/620 */
	if(((dstH * 1000) / srcH) < 440) dstH = ((srcH * 440) / 1000) + 1;
	temp = srcH / dstH;
	if(srcH % dstH) {
	   temp++;
	   pOverlay->VUSF = (srcH * 64) / (temp * dstH);
	} else {
	   pOverlay->VUSF = 0x00;
	}
	pOverlay->PitchMult = temp;
  }
}

static void  /* V 530/6326 */
calc_line_buf_size(SISOverlayPtr pOverlay)
{
    CARD32 I;
    CARD32 line = pOverlay->srcW;

    if( (pOverlay->pixelFormat == PIXEL_FMT_YV12) ||
        (pOverlay->pixelFormat == PIXEL_FMT_I420) )
    {
        I = (line >> 5) + (((line >> 6) * 2)) + 3;
	I <<= 5;
    } else { /* YUV2, UYVY, RGB */
        I = line << 1;
        if(I & 7)  I += 8;
    }
    I += 8;
    I >>= 3;
    pOverlay->lineBufSize = (CARD8)I;
}

static void  /* V 530/6326 */
merge_line_buf(SISPtr pSiS, SISPortPrivPtr pPriv, Bool enable)
{
  if(enable) {
     setvideoregmask(pSiS, Index_VI6326_Control_Misc5, 0x10, 0x10);
  } else {
     setvideoregmask(pSiS, Index_VI6326_Control_Misc5, 0x00, 0x10);
  }
}

static void  /* V 530/6326 */
set_format(SISPtr pSiS, SISOverlayPtr pOverlay)
{
    CARD8 fmt, misc0, misc1, misc4;

    switch (pOverlay->pixelFormat){
    case PIXEL_FMT_YV12:
    case PIXEL_FMT_I420: /* V/530 V/6326 */
        fmt   = 0x80;  /* D[7:6]  10 YUV2(=YUYV), 01 VYUY, 00 UYVY, 11 YVYU / 00 RGB 555, 01 RGB 565 */
	misc0 = 0x40;  /* D[6]: 1 = YUV, 0 = RGB */
	misc4 = 0x05;  /* D[1:0] 00 RGB 555, 01 YUV 422, 10 RGB 565; D[2] 1 = YUV420 mode */
	misc1 = 0xff;
        break;
    case PIXEL_FMT_UYVY:
        fmt   = 0x00;  /* D[7:6]  10 YUV2(=YUYV), 01 VYUY, 00 UYVY, 11 YVYU / 00 RGB 555, 01 RGB 565 */
	misc0 = 0x40;  /* D[6]: 1 = YUV, 0 = RGB */
	misc4 = 0x00;  /* D[1:0] 00 RGB 555, 01 YUV 422, 10 RGB 565; D[2] 1 = YUV420 mode */
	misc1 = 0xff;
        break;
    case PIXEL_FMT_YUY2: /* V/530 V/6326 */
        fmt   = 0x80;  /* D[7:6]  10 YUV2(=YUYV), 01 VYUY, 00 UYVY, 11 YVYU / 00 RGB 555, 01 RGB 565 */
	misc0 = 0x40;  /* D[6]: 1 = YUV, 0 = RGB */
	misc4 = 0x00;  /* D[1:0]  00 RGB 555, 01 YUV 422, 10 RGB 565; D[2] 1 = YUV420 mode */
	misc1 = 0xff;
        break;
    case PIXEL_FMT_RGB6: /* V/530 V/6326 */
        fmt   = 0x40;  /* D[7:6]  10 YUV2(=YUYV), 01 VYUY, 00 UYVY, 11 YVYU / 00 RGB 555, 01 RGB 565 */
	misc0 = 0x00;  /* D[6]: 1 = YUV, 0 = RGB */
	misc4 = 0xff;
	misc1 = 0x00;  /* D[2] = Capture format selection (DS5597) - WDR sets this */
	break;
    case PIXEL_FMT_RGB5: /* V/530 V/6326 */
    default:
        fmt   = 0x00;  /* D[7:6]  10 YUV2(=YUYV), 01 VYUY, 00 UYVY, 11 YVYU / 00 RGB 555, 01 RGB 565 */
	misc0 = 0x00;  /* D[6]: 1 = YUV, 0 = RGB */
	misc4 = 0xff;
	misc1 = 0x04;  /* D[2] = Capture format selection (DS5597) - WDR sets this */
	break;
    }

    setvideoregmask(pSiS, Index_VI6326_VideoFormatSelect, fmt,   0xC0);
    setvideoregmask(pSiS, Index_VI6326_Control_Misc0,     misc0, 0x40);
    if(misc4 == 0xff) {
	  setvideoregmask(pSiS, Index_VI6326_Control_Misc1,  misc1, 0x04);
	  if(pSiS->oldChipset >= OC_SIS5597) {
               setvideoregmask(pSiS, Index_VI6326_Control_Misc4, 0x00, 0x05);
	  }
    } else {
          if(pSiS->oldChipset >= OC_SIS5597) {
               setvideoregmask(pSiS, Index_VI6326_Control_Misc4,  misc4, 0x05);
	  }
	  setvideoregmask(pSiS, Index_VI6326_Control_Misc1,  0x00, 0x04);
    }
}

static void  /* V 6326/530 */
set_colorkey(SISPtr pSiS, CARD32 colorkey)
{
    CARD8 r, g, b, s;

    b = (CARD8)(colorkey & 0xFF);
    g = (CARD8)((colorkey>>8) & 0xFF);
    r = (CARD8)((colorkey>>16) & 0xFF);

    if(pSiS->CurrentLayout.bitsPerPixel >= 24) {
         s = b;
	 b = r;
	 r = s;
    }

    setvideoreg(pSiS, Index_VI6326_Overlay_ColorKey_Blue_Min  ,(CARD8)b);
    setvideoreg(pSiS, Index_VI6326_Overlay_ColorKey_Green_Min ,(CARD8)g);
    setvideoreg(pSiS, Index_VI6326_Overlay_ColorKey_Red_Min   ,(CARD8)r);

    setvideoreg(pSiS, Index_VI6326_Overlay_ColorKey_Blue_Max  ,(CARD8)b);
    setvideoreg(pSiS, Index_VI6326_Overlay_ColorKey_Green_Max ,(CARD8)g);
    setvideoreg(pSiS, Index_VI6326_Overlay_ColorKey_Red_Max   ,(CARD8)r);
}

static void
set_brightness(SISPtr pSiS, CARD8 brightness)
{
    setvideoreg(pSiS, Index_VI6326_Brightness, brightness);
}

static void
set_contrast(SISPtr pSiS, CARD8 contrast)
{
    setvideoregmask(pSiS, Index_VI6326_Contrast_Enh_Ctrl, contrast, 0x07);
}

static void
set_contrast_data(SISPtr pSiS, int value)
{
  unsigned long temp;

  if(value < 10000) temp = 0;
  else temp = (value - 10000) / 20000;
  if(temp > 3) temp = 3;
  setvideoregmask(pSiS, Index_VI6326_Contrast_Enh_Ctrl, (temp << 6), 0xC0);
  switch(temp) {
    case 0: temp =  2048; break;
    case 1: temp =  4096; break;
    case 2: temp =  8192; break;
    case 3: temp = 16384; break;
  }
  temp <<= 10;
  temp /= value;
  setvideoreg(pSiS, Index_VI6326_Contrast_Factor, temp);
}

static void
set_overlay(SISPtr pSiS, SISOverlayPtr pOverlay, SISPortPrivPtr pPriv, int index)
{
    ScrnInfoPtr pScrn = pSiS->pScrn;

    CARD16 pitch=0;
    CARD8  h_over=0, v_over=0;
    CARD16 top, bottom, left, right;
    CARD16 screenX = pSiS->CurrentLayout.mode->HDisplay;
    CARD16 screenY = pSiS->CurrentLayout.mode->VDisplay;
    CARD32 watchdog;

    top = pOverlay->dstBox.y1;
    bottom = pOverlay->dstBox.y2;
    if(bottom > screenY) {
        bottom = screenY;
    }

    left = pOverlay->dstBox.x1;
    right = pOverlay->dstBox.x2;
    if(right > screenX) {
        right = screenX;
    }

    /* TW: DoubleScan modes require Y coordinates * 2 */
    if(pSiS->CurrentLayout.mode->Flags & V_DBLSCAN) {
    	 top <<= 1;
	 bottom <<= 1;
    }
    /* TW: Interlace modes require Y coordinates / 2 */
    if(pSiS->CurrentLayout.mode->Flags & V_INTERLACE) {
    	 top >>= 1;
	 bottom >>= 1;
    }

    h_over = (((left>>8) & 0x07) | ((right>>4) & 0x70));
    v_over = (((top>>8) & 0x07) | ((bottom>>4) & 0x70));

    pitch = pOverlay->pitch * pOverlay->PitchMult;
    pitch >>= 2;   /* Datasheet: Unit = double word - verified */
    if(pitch > 0xfff) {
    	pitch = pOverlay->pitch * (0xFFF * 2 / pOverlay->pitch);
	pOverlay->VUSF = 0x3F;
    }

    /* set color key */
    set_colorkey(pSiS, pPriv->colorKey);

    /* set color key mode */
    setvideoregmask(pSiS, Index_VI6326_Key_Overlay_OP, pOverlay->keyOP, 0x0f);

    setvideoregmask(pSiS, Index_VI6326_Control_Misc0, 0x00, 0x0c);
    setvideoregmask(pSiS, Index_VI6326_Control_Misc0, 0x00, 0x18);

    /* Set Y buf pitch */   /* Datasheet: Unit = double word - verified */
    setvideoreg(pSiS, Index_VI6326_Disp_Y_Buf_Pitch_Low, (CARD8)(pitch));
    setvideoregmask(pSiS, Index_VI6326_Disp_Y_Buf_Pitch_High, (CARD8)(pitch>>8), 0x0f);
    /* Set U/V pitch if using planar formats */
    if( (pOverlay->pixelFormat == PIXEL_FMT_YV12) ||
    	(pOverlay->pixelFormat == PIXEL_FMT_I420) )  {
	/* Set U/V pitch */  /* Datasheet: Unit = double word - verified */
	setvideoreg(pSiS, Index_VI6326_Disp_UV_Buf_Pitch_Low, (CARD8)pitch >> 1);
        setvideoregmask(pSiS, Index_VI6326_Disp_UV_Buf_Pitch_High, (CARD8)(pitch >> 9), 0x0f);
    }

    /* set line buffer size */
    setvideoreg(pSiS, Index_VI6326_Line_Buffer_Size, pOverlay->lineBufSize);

    /* set scale factor */
    setvideoreg(pSiS, Index_VI6326_Hor_Scale,             (CARD8)((pOverlay->HUSF) | 0xC0));
    setvideoregmask(pSiS, Index_VI6326_Hor_Scale_Integer, (CARD8)(pOverlay->HIntBit), 0x0F);
    setvideoregmask(pSiS, Index_VI6326_Ver_Scale,         (CARD8)(pOverlay->VUSF), 0x3F);

    /* TW: We don't have to wait for vertical retrace in all cases */
    if(pPriv->mustwait) {
	watchdog = WATCHDOG_DELAY;
	while ((!pOverlay->VBlankActiveFunc(pSiS)) && --watchdog);
	if(!watchdog) xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
			"Xv: Waiting for vertical retrace timed-out\n");
    }

    /* set destination window position */
    setvideoreg(pSiS, Index_VI6326_Win_Hor_Disp_Start_Low, (CARD8)left);
    setvideoreg(pSiS, Index_VI6326_Win_Hor_Disp_End_Low,   (CARD8)right);
    setvideoreg(pSiS, Index_VI6326_Win_Hor_Over,           (CARD8)h_over);

    setvideoreg(pSiS, Index_VI6326_Win_Ver_Disp_Start_Low, (CARD8)top);
    setvideoreg(pSiS, Index_VI6326_Win_Ver_Disp_End_Low,   (CARD8)bottom);
    setvideoreg(pSiS, Index_VI6326_Win_Ver_Over,           (CARD8)v_over);

    /* Set Y start address */
    setvideoreg (pSiS, Index_VI6326_Disp_Y_Buf_Start_Low,    (CARD8)(pOverlay->PSY));
    setvideoreg (pSiS, Index_VI6326_Disp_Y_Buf_Start_Middle, (CARD8)((pOverlay->PSY)>>8));
    if(pSiS->oldChipset <= OC_SIS6326) {  	/* all old chipsets incl 6326 */
       /* Set overflow bits */
       setvideoregmask (pSiS, Index_VI6326_Disp_Capt_Y_Buf_Start_High,
                                             (CARD8)(((pOverlay->PSY)>>12) & 0xF0), 0xF0);
       /* Set framebuffer end address */
       setvideoreg (pSiS, Index_VI6326_Disp_Y_End,    (CARD8)(pOverlay->YUVEnd));
    } else {  				/* 530/620 */
       /* Set overflow bits */
       setvideoregmask (pSiS, Index_VI6326_Disp_Capt_Y_Buf_Start_High,
                                             (CARD8)(((pOverlay->PSY)>>13) & 0xF8), 0xF8);
    }

    /* Set U/V start addresses if using plane formats */
    if( (pOverlay->pixelFormat == PIXEL_FMT_YV12) ||
    	(pOverlay->pixelFormat == PIXEL_FMT_I420) )  {

        CARD32 PSU = pOverlay->PSU;
        CARD32 PSV = pOverlay->PSV;

        /* set U/V start address */
        setvideoreg (pSiS, Index_VI6326_U_Buf_Start_Low,   (CARD8)PSU);
        setvideoreg (pSiS, Index_VI6326_U_Buf_Start_Middle,(CARD8)(PSU >> 8));

        setvideoreg (pSiS, Index_VI6326_V_Buf_Start_Low,   (CARD8)PSV);
        setvideoreg (pSiS, Index_VI6326_V_Buf_Start_Middle,(CARD8)(PSV >> 8));

	setvideoreg (pSiS, Index_VI6326_UV_Buf_Start_High,
					(CARD8)(((PSU >> 16) & 0x0F) | ((PSV >> 12) & 0xF0)) );

	if(pSiS->oldChipset > OC_SIS6326) {
	   /* Set bit 20 of the addresses in Misc5 (530/620 only) */
	   setvideoreg (pSiS, Index_VI6326_Control_Misc5,
				(CARD8)(((PSU >> (20-1)) & 0x02) | ((PSV >> (20-2)) & 0x04)) );
	}
    }

    /* set brightness and contrast */
    set_brightness(pSiS, pPriv->brightness);
    if(pSiS->oldChipset > OC_SIS6205C) {
       set_contrast_data(pSiS, (pOverlay->dstBox.x2 - pOverlay->dstBox.x1) *
                               (pOverlay->dstBox.y2 - pOverlay->dstBox.y1));
       set_contrast(pSiS, pPriv->contrast);
    }

    /* set format */
    set_format(pSiS, pOverlay);
}

/* TW: Overlay MUST NOT be switched off while beam is over it */
static void
close_overlay(SISPtr pSiS, SISPortPrivPtr pPriv)
{
  CARD32 watchdog;

  watchdog = WATCHDOG_DELAY;
  while((!vblank_active_CRT1(pSiS)) && --watchdog);
  if(pSiS->oldChipset > OC_SIS6326) {
     /* what is this? */
     setvideoregmask(pSiS, Index_VI6326_Control_Misc4, 0x40, 0x40);
  }
  /* disable overlay */
  setvideoregmask(pSiS, Index_VI6326_Control_Misc0, 0x00, 0x02);
}

static void
SIS6326DisplayVideo(ScrnInfoPtr pScrn, SISPortPrivPtr pPriv)
{
   SISPtr pSiS = SISPTR(pScrn);

   short srcPitch = pPriv->srcPitch;
   short height = pPriv->height;
   short width = pPriv->width;
   SISOverlayRec overlay;
   int srcOffsetX=0, srcOffsetY=0;
   int sx, sy;
   int index = 0;
   int pitch;

   memset(&overlay, 0, sizeof(overlay));
   overlay.pixelFormat = pPriv->id;
   overlay.pitch = srcPitch;
   overlay.keyOP = VI6326_ROP_DestKey;	/* DestKey mode */

   overlay.dstBox.x1 = pPriv->drw_x - pScrn->frameX0;
   overlay.dstBox.x2 = pPriv->drw_x + pPriv->drw_w - pScrn->frameX0;
   overlay.dstBox.y1 = pPriv->drw_y - pScrn->frameY0;
   overlay.dstBox.y2 = pPriv->drw_y + pPriv->drw_h - pScrn->frameY0;

   if((overlay.dstBox.x1 > overlay.dstBox.x2) ||
   		(overlay.dstBox.y1 > overlay.dstBox.y2))
     return;

   if((overlay.dstBox.x2 < 0) || (overlay.dstBox.y2 < 0))
     return;

   if(overlay.dstBox.x1 < 0) {
     srcOffsetX = pPriv->src_w * (-overlay.dstBox.x1) / pPriv->drw_w;
     overlay.dstBox.x1 = 0;
   }
   if(overlay.dstBox.y1 < 0) {
     srcOffsetY = pPriv->src_h * (-overlay.dstBox.y1) / pPriv->drw_h;
     overlay.dstBox.y1 = 0;
   }

   switch(pPriv->id){
     case PIXEL_FMT_YV12:
       sx = (pPriv->src_x + srcOffsetX) & ~7;
       sy = (pPriv->src_y + srcOffsetY) & ~1;
       pitch = (width + 3) & ~3;
       overlay.PSY = pPriv->bufAddr[pPriv->currentBuf] + sx + sy * pitch;
       overlay.PSV = overlay.PSY + pitch * height;
       overlay.PSU = overlay.PSV + ((((width >> 1) + 3) & ~3) * (height >> 1));
       overlay.PSY >>= 2;
       overlay.PSV >>= 2;
       overlay.PSU >>= 2;
       break;
     case PIXEL_FMT_I420:
       sx = (pPriv->src_x + srcOffsetX) & ~7;
       sy = (pPriv->src_y + srcOffsetY) & ~1;
       pitch = (width + 3) & ~3;
       overlay.PSY = pPriv->bufAddr[pPriv->currentBuf] + sx + sy * pitch;
       overlay.PSU = overlay.PSY + pitch * height;
       overlay.PSV = overlay.PSU + ((((width >> 1) + 3) & ~3) * (height >> 1));
       overlay.PSY >>= 2;
       overlay.PSV >>= 2;
       overlay.PSU >>= 2;
       break;
     case PIXEL_FMT_YUY2:
     case PIXEL_FMT_UYVY:
     case PIXEL_FMT_RGB6:
     case PIXEL_FMT_RGB5:
     default:
       sx = (pPriv->src_x + srcOffsetX) & ~1;
       sy = (pPriv->src_y + srcOffsetY);
       overlay.PSY = (pPriv->bufAddr[pPriv->currentBuf] + sx*2 + sy*srcPitch);
       overlay.PSY >>= 2;
       break;
   }

   /* FIXME: Is this correct? (Is it required to set the end address?
    *        Datasheet is not clear) - (reg does not exist on 530/620)
    */
   overlay.YUVEnd = (pPriv->bufAddr[pPriv->currentBuf] + pPriv->totalSize) >> 14;

   /* FIXME: is it possible that srcW < 0 */
   overlay.srcW = pPriv->src_w - (sx - pPriv->src_x);
   overlay.srcH = pPriv->src_h - (sy - pPriv->src_y);

   if ( (pPriv->oldx1 != overlay.dstBox.x1) ||
   	(pPriv->oldx2 != overlay.dstBox.x2) ||
	(pPriv->oldy1 != overlay.dstBox.y1) ||
	(pPriv->oldy2 != overlay.dstBox.y2) ) {
	pPriv->mustwait = 1;
	pPriv->oldx1 = overlay.dstBox.x1; pPriv->oldx2 = overlay.dstBox.x2;
	pPriv->oldy1 = overlay.dstBox.y1; pPriv->oldy2 = overlay.dstBox.y2;
   }

   /* calculate line buffer length */
   calc_line_buf_size(&overlay);

   overlay.VBlankActiveFunc = vblank_active_CRT1;
/* overlay.GetScanLineFunc = get_scanline_CRT1;  */

   /* calculate scale factor */
   calc_scale_factor(pSiS, &overlay, pScrn, pPriv);

   /* set (not only determine) if line buffer is to be merged */
   if(pSiS->oldChipset > OC_SIS5597) {
      int temp;
      if(pSiS->oldChipset <= OC_SIS6326) temp = 352;
      else                               temp = 384;
      merge_line_buf(pSiS, pPriv, (overlay.srcW > temp));
   }

   /* set overlay */
   set_overlay(pSiS, &overlay, pPriv, index);

   /* enable overlay */
   if(pSiS->oldChipset > OC_SIS6326) {
       setvideoregmask (pSiS, Index_VI6326_Control_Misc4, 0x40, 0x40);
   }
   setvideoregmask (pSiS, Index_VI6326_Control_Misc0, 0x02, 0x02);

   pPriv->mustwait = 0;
}

static FBLinearPtr
SIS6326AllocateOverlayMemory(
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

   new_linear = xf86AllocateOffscreenLinear(pScreen, size, 32,
                                            NULL, NULL, NULL);

   if(!new_linear) {
        int max_size;

        xf86QueryLargestOffscreenLinear(pScreen, &max_size, 32,
				       PRIORITY_EXTREME);

        if(max_size < size) return NULL;

        xf86PurgeUnlockedOffscreenAreas(pScreen);
        new_linear = xf86AllocateOffscreenLinear(pScreen, size, 32,
                                                 NULL, NULL, NULL);
   }
   if (!new_linear)
        xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	           "Xv: Failed to allocate %dK of video memory\n", size/1024);

   return new_linear;
}

static void
SIS6326FreeOverlayMemory(ScrnInfoPtr pScrn)
{
    SISPortPrivPtr pPriv = GET_PORT_PRIVATE(pScrn);

    if(pPriv->linear) {
        xf86FreeOffscreenLinear(pPriv->linear);
	pPriv->linear = NULL;
    }
}

static void
SIS6326StopVideo(ScrnInfoPtr pScrn, pointer data, Bool shutdown)
{
  SISPortPrivPtr pPriv = (SISPortPrivPtr)data;
  SISPtr pSiS = SISPTR(pScrn);

  if(pPriv->grabbedByV4L)
  	return;

  REGION_EMPTY(pScrn->pScreen, &pPriv->clip);

  if(shutdown) {
     if(pPriv->videoStatus & CLIENT_VIDEO_ON) {
       close_overlay(pSiS, pPriv);
       pPriv->mustwait = 1;
     }
     SIS6326FreeOverlayMemory(pScrn);
     pPriv->videoStatus = 0;
     pSiS->VideoTimerCallback = NULL;
  } else {
     if(pPriv->videoStatus & CLIENT_VIDEO_ON) {
       pPriv->videoStatus = OFF_TIMER | CLIENT_VIDEO_ON;
       pPriv->offTime = currentTime.milliseconds + OFF_DELAY;
       pSiS->VideoTimerCallback = SIS6326VideoTimerCallback;
     }
  }
}

static int
SIS6326PutImage(
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
   SISPtr pSiS = SISPTR(pScrn);
   SISPortPrivPtr pPriv = (SISPortPrivPtr)data;

   int totalSize=0;
   int depth = pSiS->CurrentLayout.bitsPerPixel >> 3;

   if(pPriv->grabbedByV4L)
   	return Success;

   pPriv->drw_x = drw_x;
   pPriv->drw_y = drw_y;
   pPriv->drw_w = drw_w;
   pPriv->drw_h = drw_h;
   pPriv->src_x = src_x;
   pPriv->src_y = src_y;
   pPriv->src_w = src_w;
   pPriv->src_h = src_h;
   pPriv->id = id;
   pPriv->height = height;
   pPriv->width = width;

   /* TW: Pixel formats:
      1. YU12:  3 planes:       H    V
               Y sample period  1    1   (8 bit per pixel)
	       V sample period  2    2	 (8 bit per pixel, subsampled)
	       U sample period  2    2   (8 bit per pixel, subsampled)

 	 Y plane is fully sampled (width*height), U and V planes
	 are sampled in 2x2 blocks, hence a group of 4 pixels requires
	 4 + 1 + 1 = 6 bytes. The data is planar, ie in single planes
	 for Y, U and V.
      2. UYVY: 3 planes:        H    V
               Y sample period  1    1   (8 bit per pixel)
	       V sample period  2    1	 (8 bit per pixel, subsampled)
	       U sample period  2    1   (8 bit per pixel, subsampled)
	 Y plane is fully sampled (width*height), U and V planes
	 are sampled in 2x1 blocks, hence a group of 4 pixels requires
	 4 + 2 + 2 = 8 bytes. The data is bit packed, there are no separate
	 Y, U or V planes.
	 Bit order:  U0 Y0 V0 Y1  U2 Y2 V2 Y3 ...
      3. I420: Like YU12, but planes U and V are in reverse order.
      4. YUY2: Like UYVY, but order is
                     Y0 U0 Y1 V0  Y2 U2 Y3 V2 ...
   */

   switch(id){
     case PIXEL_FMT_YV12:
     case PIXEL_FMT_I420:
       pPriv->srcPitch = (width + 7) & ~7;
       /* Size = width * height * 3 / 2 */
       totalSize = (pPriv->srcPitch * height * 3) >> 1;
       break;
     case PIXEL_FMT_YUY2:
     case PIXEL_FMT_UYVY:
     case PIXEL_FMT_RGB5:
     case PIXEL_FMT_RGB6:
     default:
       pPriv->srcPitch = ((width << 1) + 3) & ~3;
       /* Size = width * 2 * height */
       totalSize = pPriv->srcPitch * height;
   }

   pPriv->totalSize = totalSize;

   /* allocate memory (we do doublebuffering) */
   if(!(pPriv->linear = SIS6326AllocateOverlayMemory(pScrn, pPriv->linear,
						 totalSize<<1)))
	return BadAlloc;

   /* fixup pointers */
   pPriv->bufAddr[0] = (pPriv->linear->offset * depth);
   pPriv->bufAddr[1] = pPriv->bufAddr[0] + totalSize;

   /* copy data */
   memcpy(pSiS->FbBase + pPriv->bufAddr[pPriv->currentBuf], buf, totalSize);

   SIS6326DisplayVideo(pScrn, pPriv);

   /* update cliplist */
   if(  pPriv->autopaintColorKey &&
        (pPriv->grabbedByV4L || !RegionsEqual(&pPriv->clip, clipBoxes))) {
     /* We always paint colorkey for V4L */
     if (!pPriv->grabbedByV4L)
     	REGION_COPY(pScreen, &pPriv->clip, clipBoxes);
     /* draw these */
     /* xf86XVFillKeyHelper(pScrn->pScreen, pPriv->colorKey, clipBoxes); - for X4.2 */
     XAAFillSolidRects(pScrn, pPriv->colorKey, GXcopy, ~0,
                    REGION_NUM_RECTS(clipBoxes),
                    REGION_RECTS(clipBoxes));
   }

   pPriv->currentBuf ^= 1;

   pPriv->videoStatus = CLIENT_VIDEO_ON;

   pSiS->VideoTimerCallback = SIS6326VideoTimerCallback;

   return Success;
}

static int
SIS6326QueryImageAttributes(
  ScrnInfoPtr pScrn,
  int id,
  unsigned short *w, unsigned short *h,
  int *pitches, int *offsets
){
    SISPtr pSiS = SISPTR(pScrn);
    int pitchY, pitchUV;
    int size, sizeY, sizeUV;

    if(*w < IMAGE_MIN_WIDTH) *w = IMAGE_MIN_WIDTH;
    if(*h < IMAGE_MIN_HEIGHT) *h = IMAGE_MIN_HEIGHT;

    if(pSiS->oldChipset < OC_SIS6326) {
       if(*w > IMAGE_MAX_WIDTH_5597) *w = IMAGE_MAX_WIDTH_5597;
       if(*h > IMAGE_MAX_HEIGHT_5597) *h = IMAGE_MAX_HEIGHT_5597;
    } else {
       if(*w > IMAGE_MAX_WIDTH) *w = IMAGE_MAX_WIDTH;
       if(*h > IMAGE_MAX_HEIGHT) *h = IMAGE_MAX_HEIGHT;
    }

    switch(id) {
    case PIXEL_FMT_YV12:
    case PIXEL_FMT_I420:
        *w = (*w + 7) & ~7;
        *h = (*h + 1) & ~1;
        pitchY = *w;
    	pitchUV = *w >> 1;
    	if(pitches) {
      	    pitches[0] = pitchY;
            pitches[1] = pitches[2] = pitchUV;
        }
    	sizeY = pitchY * (*h);
    	sizeUV = pitchUV * ((*h) >> 1);
    	if(offsets) {
          offsets[0] = 0;
          offsets[1] = sizeY;
          offsets[2] = sizeY + sizeUV;
        }
        size = sizeY + (sizeUV << 1);
    	break;
    case PIXEL_FMT_YUY2:
    case PIXEL_FMT_UYVY:
    case PIXEL_FMT_RGB5:
    case PIXEL_FMT_RGB6:
    default:
        *w = (*w + 1) & ~1;
        pitchY = *w << 1;
    	if(pitches) pitches[0] = pitchY;
    	if(offsets) offsets[0] = 0;
    	size = pitchY * (*h);
    	break;
    }

    return size;
}

static void
SIS6326VideoTimerCallback (ScrnInfoPtr pScrn, Time now)
{
    SISPtr         pSiS = SISPTR(pScrn);
    SISPortPrivPtr pPriv = NULL;
    unsigned char  sridx, cridx;

    pSiS->VideoTimerCallback = NULL;

    if(!pScrn->vtSema) return;

    if(pSiS->adaptor) {
    	pPriv = GET_PORT_PRIVATE(pScrn);
	if(!pPriv->videoStatus)
	   pPriv = NULL;
    }

    if(pPriv) {
      if(pPriv->videoStatus & TIMER_MASK) {
        UpdateCurrentTime();
	if(pPriv->offTime < currentTime.milliseconds) {
          if(pPriv->videoStatus & OFF_TIMER) {
              /* Turn off the overlay */
	      sridx = inSISREG(SISSR); cridx = inSISREG(SISCR);
              close_overlay(pSiS, pPriv);
	      outSISREG(SISSR, sridx); outSISREG(SISCR, cridx);
	      pPriv->mustwait = 1;
              pPriv->videoStatus = FREE_TIMER;
              pPriv->freeTime = currentTime.milliseconds + FREE_DELAY;
	      pSiS->VideoTimerCallback = SIS6326VideoTimerCallback;
          } else
	  if(pPriv->videoStatus & FREE_TIMER) {  
              SIS6326FreeOverlayMemory(pScrn);
	      pPriv->mustwait = 1;
              pPriv->videoStatus = 0;
          }
        } else
	  pSiS->VideoTimerCallback = SIS6326VideoTimerCallback;
      }
   }
}

/* TW: Offscreen surface stuff for v4l */

static int
SIS6326AllocSurface (
    ScrnInfoPtr pScrn,
    int id,
    unsigned short w,
    unsigned short h,
    XF86SurfacePtr surface
)
{
    SISPtr pSiS = SISPTR(pScrn);
    SISPortPrivPtr pPriv = GET_PORT_PRIVATE(pScrn);
    int size, depth;

    if((w < IMAGE_MIN_WIDTH) || (h < IMAGE_MIN_HEIGHT))
    	return BadValue;

    if(pSiS->oldChipset < OC_SIS6326) {
      if((w > IMAGE_MAX_WIDTH_5597) || (h > IMAGE_MAX_HEIGHT_5597))
    	  return BadValue;
    } else {
      if((w > IMAGE_MAX_WIDTH) || (h > IMAGE_MAX_HEIGHT))
    	  return BadValue;
    }

    if(pPriv->grabbedByV4L)
    	return BadAlloc;

    depth = pSiS->CurrentLayout.bitsPerPixel >> 3;

    w = (w + 1) & ~1;
    pPriv->pitch = ((w << 1) + 63) & ~63; /* Only packed pixel modes supported */
    size = h * pPriv->pitch;
    pPriv->linear = SIS6326AllocateOverlayMemory(pScrn, pPriv->linear, size);
    if(!pPriv->linear)
    	return BadAlloc;

    pPriv->totalSize = size;

    pPriv->offset    = pPriv->linear->offset * depth;

    surface->width   = w;
    surface->height  = h;
    surface->pScrn   = pScrn;
    surface->id      = id;
    surface->pitches = &pPriv->pitch;
    surface->offsets = &pPriv->offset;
    surface->devPrivate.ptr = (pointer)pPriv;

    close_overlay(pSiS, pPriv);
    pPriv->videoStatus = 0;
    REGION_EMPTY(pScrn->pScreen, &pPriv->clip);
    pSiS->VideoTimerCallback = NULL;
    pPriv->grabbedByV4L = TRUE;
    return Success;
}

static int
SIS6326StopSurface (XF86SurfacePtr surface)
{
    SISPortPrivPtr pPriv = (SISPortPrivPtr)(surface->devPrivate.ptr);
    SISPtr pSiS = SISPTR(surface->pScrn);

    if(pPriv->grabbedByV4L && pPriv->videoStatus) {
        close_overlay(pSiS, pPriv);
	pPriv->mustwait = 1;
	pPriv->videoStatus = 0;
    }
    return Success;
}

static int
SIS6326FreeSurface (XF86SurfacePtr surface)
{
    SISPortPrivPtr pPriv = (SISPortPrivPtr)(surface->devPrivate.ptr);

    if(pPriv->grabbedByV4L) {
	SIS6326StopSurface(surface);
	SIS6326FreeOverlayMemory(surface->pScrn);
	pPriv->grabbedByV4L = FALSE;
    }
    return Success;
}

static int
SIS6326GetSurfaceAttribute (
    ScrnInfoPtr pScrn,
    Atom attribute,
    INT32 *value
)
{
    SISPortPrivPtr pPriv = GET_PORT_PRIVATE(pScrn);

    return SIS6326GetPortAttribute(pScrn, attribute, value, (pointer)pPriv);
}

static int
SIS6326SetSurfaceAttribute(
    ScrnInfoPtr pScrn,
    Atom attribute,
    INT32 value
)
{
    SISPortPrivPtr pPriv = GET_PORT_PRIVATE(pScrn);;

    return SIS6326SetPortAttribute(pScrn, attribute, value, (pointer)pPriv);
}

static int
SIS6326DisplaySurface (
    XF86SurfacePtr surface,
    short src_x, short src_y,
    short drw_x, short drw_y,
    short src_w, short src_h,
    short drw_w, short drw_h,
    RegionPtr clipBoxes
)
{
   ScrnInfoPtr pScrn = surface->pScrn;
   SISPortPrivPtr pPriv = (SISPortPrivPtr)(surface->devPrivate.ptr);

   if(!pPriv->grabbedByV4L)
    	return Success;

   pPriv->drw_x = drw_x;
   pPriv->drw_y = drw_y;
   pPriv->drw_w = drw_w;
   pPriv->drw_h = drw_h;
   pPriv->src_x = src_x;
   pPriv->src_y = src_y;
   pPriv->src_w = src_w;
   pPriv->src_h = src_h;
   pPriv->id = surface->id;
   pPriv->height = surface->height;
   pPriv->bufAddr[0] = surface->offsets[0];
   pPriv->currentBuf = 0;
   pPriv->srcPitch = surface->pitches[0];

   SIS6326DisplayVideo(pScrn, pPriv);

   if(pPriv->autopaintColorKey) {
   	XAAFillSolidRects(pScrn, pPriv->colorKey, GXcopy, ~0,
                    REGION_NUM_RECTS(clipBoxes),
                    REGION_RECTS(clipBoxes));
   }

   pPriv->videoStatus = CLIENT_VIDEO_ON;

   return Success;
}

XF86OffscreenImageRec SIS6326OffscreenImages[2] =
{
 {
   &SIS6326Images[0],  	/* YUV2 */
   VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT,
   SIS6326AllocSurface,
   SIS6326FreeSurface,
   SIS6326DisplaySurface,
   SIS6326StopSurface,
   SIS6326GetSurfaceAttribute,
   SIS6326SetSurfaceAttribute,
   IMAGE_MAX_WIDTH, IMAGE_MAX_HEIGHT,
   NUM_ATTRIBUTES,
   &SIS6326Attributes[0]  /* Support all attributes */
  },
  {
   &SIS6326Images[1],	/* UYVY */
   VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT,
   SIS6326AllocSurface,
   SIS6326FreeSurface,
   SIS6326DisplaySurface,
   SIS6326StopSurface,
   SIS6326GetSurfaceAttribute,
   SIS6326SetSurfaceAttribute,
   IMAGE_MAX_WIDTH, IMAGE_MAX_HEIGHT,
   NUM_ATTRIBUTES,
   &SIS6326Attributes[0]  /* Support all attributes */
  },
};

static void
SIS6326InitOffscreenImages(ScreenPtr pScrn)
{
    xf86XVRegisterOffscreenImages(pScrn, SIS6326OffscreenImages, 2);
}
#else
int sis_foo;
#endif

