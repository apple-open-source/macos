/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sis/sis_video.c,v 1.10 2003/02/04 02:44:29 dawes Exp $ */
/*
 * Xv driver for SiS 300 and 310/325 series.
 *
 * (Based on the mga Xv driver by Mark Vojkovich and i810 Xv
 * driver by Jonathan Bian <jonathan.bian@intel.com>.)
 *
 * Copyright 2000 Silicon Integrated Systems Corp, Inc., HsinChu, Taiwan.
 * Copyright 2002,2003 by Thomas Winischhofer, Vienna, Austria.
 * All Rights Reserved.
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
 * Authors:
 *      Sung-Ching Lin <sclin@sis.com.tw>
 *
 *	Thomas Winischhofer <thomas@winischhofer.net>:
 *              - 310/325 series (315/550/650/651/740/M650) support
 *		- (possibly incomplete) Xabre (SiS330) support
 *              - new mode switching code for 300, 310/325 and 330 series
 *              - many fixes for 300/540/630/730 chipsets,
 *              - many fixes for 5597/5598, 6326 and 530/620 chipsets,
 *              - VESA mode switching (deprecated),
 *              - extended CRT2/video bridge handling support,
 *              - dual head support on 300, 310/325 and 330 series
 *              - 650/LVDS (up to 1400x1050), 650/Chrontel 701x support
 *              - 30xB/30xLV/30xLVX video bridge support (300, 310/325, 330 series)
 *              - Xv support for 5597/5598, 6326, 530/620 and 310/325 series
 *              - video overlay enhancements for 300 series
 *              - TV and hi-res support for the 6326
 *              - etc.
 *
 * TW: This supports the following chipsets:
 *  SiS300: No registers >0x65, offers one overlay
 *  SiS630/730: No registers >0x6b, offers two overlays (one used for CRT1, one for CRT2)
 *  SiS550: Full register range, offers two overlays (one used for CRT1, one for CRT2)
 *  SiS315: Full register range, offers one overlay (used for both CRT1 and CRT2 alt.)
 *  SiS650/740: Full register range, offers one overlay (used for both CRT1 and CRT2 alt.)
 *  SiSM650/651: Full register range, two overlays (one used for CRT1, one for CRT2)
 *
 * Help for reading the code:
 * 315/550/650/740/M650/651 = SIS_315_VGA
 * 300/630/730              = SIS_300_VGA
 * For chipsets with 2 overlays, hasTwoOverlays will be true
 */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86Resources.h"
#include "xf86_ansic.h"
#include "compiler.h"
#include "xf86PciInfo.h"
#include "xf86Pci.h"
#include "xf86fbman.h"
#include "regionstr.h"

#include "sis.h"
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

static 		XF86VideoAdaptorPtr SISSetupImageVideo(ScreenPtr);
static void 	SISStopVideo(ScrnInfoPtr, pointer, Bool);
static int 	SISSetPortAttribute(ScrnInfoPtr, Atom, INT32, pointer);
static int 	SISGetPortAttribute(ScrnInfoPtr, Atom ,INT32 *, pointer);
static void 	SISQueryBestSize(ScrnInfoPtr, Bool, short, short, short,
			short, unsigned int *,unsigned int *, pointer);
static int 	SISPutImage( ScrnInfoPtr,
    			short, short, short, short, short, short, short, short,
    			int, unsigned char*, short, short, Bool, RegionPtr, pointer);
static int 	SISQueryImageAttributes(ScrnInfoPtr,
    			int, unsigned short *, unsigned short *, int *, int *);
static void 	SISVideoTimerCallback(ScrnInfoPtr pScrn, Time now);
static void     SISInitOffscreenImages(ScreenPtr pScrn);

#define MAKE_ATOM(a) MakeAtom(a, sizeof(a) - 1, TRUE)

extern BOOLEAN  SiSBridgeIsInSlaveMode(ScrnInfoPtr pScrn);

#define IMAGE_MIN_WIDTH         32  /* Minimum and maximum source image sizes */
#define IMAGE_MIN_HEIGHT        24
#define IMAGE_MAX_WIDTH        720
#define IMAGE_MAX_HEIGHT       576
#define IMAGE_MAX_WIDTH_M650  1920
#define IMAGE_MAX_HEIGHT_M650 1080

#define OVERLAY_MIN_WIDTH       32  /* Minimum overlay sizes */
#define OVERLAY_MIN_HEIGHT      24

#define DISPMODE_SINGLE1 0x1  /* TW: CRT1 only */
#define DISPMODE_SINGLE2 0x2  /* TW: CRT2 only */
#define DISPMODE_MIRROR  0x4  /* TW: CRT1 + CRT2 MIRROR (see note below) */

#ifdef SISDUALHEAD
#define HEADOFFSET (pSiS->dhmOffset)
#endif

/* TW: Note on "MIRROR":
 *     When using VESA on machines with an enabled video bridge, this means
 *     a real mirror. CRT1 and CRT2 have the exact same resolution and
 *     refresh rate. The same applies to modes which require the bridge to
 *     operate in slave mode.
 *     When not using VESA and the bridge is not in slave mode otherwise,
 *     CRT1 and CRT2 have the same resolution but possibly a different
 *     refresh rate.
 */

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
    inSISIDXREG(SISVID, reg, ret);
    return(ret);
}

static void setvideoreg(SISPtr pSiS, CARD8 reg, CARD8 data)
{
    outSISIDXREG(SISVID, reg, data);
}

static void setvideoregmask(SISPtr pSiS, CARD8 reg, CARD8 data, CARD8 mask)
{
    CARD8   old;

    inSISIDXREG(SISVID, reg, old);
    data = (data & mask) | (old & (~mask));
    outSISIDXREG(SISVID, reg, data);
}

static void setsrregmask(SISPtr pSiS, CARD8 reg, CARD8 data, CARD8 mask)
{
    CARD8   old;

    inSISIDXREG(SISSR, reg, old);
    data = (data & mask) | (old & (~mask));
    outSISIDXREG(SISSR, reg, data);
}

#if 0
static CARD8 getsisreg(SISPtr pSiS, CARD8 index_offset, CARD8 reg)
{
    CARD8 ret;
    inSISIDXREG(index_offset, reg, ret);
    return(ret);
}
#endif

/* VBlank */
static CARD8 vblank_active_CRT1(SISPtr pSiS)
{
    return (inSISREG(SISINPSTAT) & 0x08);
}

static CARD8 vblank_active_CRT2(SISPtr pSiS)
{
    CARD8 ret;
    if(pSiS->VGAEngine == SIS_315_VGA) {
       inSISIDXREG(SISPART1, Index_310_CRT2_FC_VR, ret);
    } else {
       inSISIDXREG(SISPART1, Index_CRT2_FC_VR, ret);
    }
    return((ret & 0x02) ^ 0x02);
}

/* Scanline - unused */
#if 0
static CARD32 get_scanline_CRT1(SISPtr pSiS)
{
    CARD32 line;

    _siswrite (pSiS, REG_PRIM_CRT_COUNTER, 0x00000001);
    line = _sisread (pSiS, REG_PRIM_CRT_COUNTER);

    return ((line >> 16) & 0x07FF);
}

static CARD32 get_scanline_CRT2(SISPtr pSiS)
{
    CARD32 line;

    line = (CARD32)(getsisreg(pSiS, SISPART1, Index_CRT2_FC_VCount1) & 0x70) * 16
                + getsisreg(pSiS, SISPART1, Index_CRT2_FC_VCount);

    return line;
}
#endif

void SISInitVideo(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    XF86VideoAdaptorPtr *adaptors, *newAdaptors = NULL;
    XF86VideoAdaptorPtr newAdaptor = NULL;
    int num_adaptors;

    newAdaptor = SISSetupImageVideo(pScreen);
    if(newAdaptor)
	SISInitOffscreenImages(pScreen);

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
}

/* client libraries expect an encoding */
static XF86VideoEncodingRec DummyEncoding =
{
   0,
   "XV_IMAGE",
   IMAGE_MAX_WIDTH, IMAGE_MAX_HEIGHT,
   {1, 1}
};

static XF86VideoEncodingRec DummyEncoding_M650 =
{
   0,
   "XV_IMAGE",
   IMAGE_MAX_WIDTH_M650, IMAGE_MAX_HEIGHT_M650,
   {1, 1}
};

#define NUM_FORMATS 3

static XF86VideoFormatRec SISFormats[NUM_FORMATS] =
{
   { 8, PseudoColor},
   {16, TrueColor},
   {24, TrueColor}
};

#define NUM_ATTRIBUTES_300 5
#define NUM_ATTRIBUTES_325 7

static XF86AttributeRec SISAttributes_300[NUM_ATTRIBUTES_300] =
{
   {XvSettable | XvGettable, 0, (1 << 24) - 1, "XV_COLORKEY"},
   {XvSettable | XvGettable, -128, 127,        "XV_BRIGHTNESS"},
   {XvSettable | XvGettable, 0, 7,             "XV_CONTRAST"},
   {XvSettable | XvGettable, 0, 1,             "XV_AUTOPAINT_COLORKEY"},
   {XvSettable             , 0, 0,             "XV_SET_DEFAULTS"}
};

static XF86AttributeRec SISAttributes_325[NUM_ATTRIBUTES_325] =
{
   {XvSettable | XvGettable, 0, (1 << 24) - 1, "XV_COLORKEY"},
   {XvSettable | XvGettable, -128, 127,        "XV_BRIGHTNESS"},
   {XvSettable | XvGettable, 0, 7,             "XV_CONTRAST"},
   {XvSettable | XvGettable, -7, 7,            "XV_SATURATION"},
   {XvSettable | XvGettable, -8, 7,            "XV_HUE"},	
   {XvSettable | XvGettable, 0, 1,             "XV_AUTOPAINT_COLORKEY"},
   {XvSettable             , 0, 0,             "XV_SET_DEFAULTS"}
};

#define NUM_IMAGES 6
#define PIXEL_FMT_YV12 FOURCC_YV12  /* 0x32315659 */
#define PIXEL_FMT_UYVY FOURCC_UYVY  /* 0x59565955 */
#define PIXEL_FMT_YUY2 FOURCC_YUY2  /* 0x32595559 */
#define PIXEL_FMT_I420 FOURCC_I420  /* 0x30323449 */
#define PIXEL_FMT_RGB5 0x35315652
#define PIXEL_FMT_RGB6 0x36315652

static XF86ImageRec SISImages[NUM_IMAGES] =
{
    XVIMAGE_YUY2, /* TW: If order is changed, SISOffscreenImages must be adapted */
    XVIMAGE_YV12,
    XVIMAGE_UYVY,
    XVIMAGE_I420
    ,
    { /* RGB 555 */
      0x35315652,
      XvRGB,
      LSBFirst,
      {'R','V','1','5',
       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
      16,
      XvPacked,
      1,
/*    15, 0x001F, 0x03E0, 0x7C00, - incorrect! */
      15, 0x7C00, 0x03E0, 0x001F,
      0, 0, 0,
      0, 0, 0,
      0, 0, 0,
      {'R', 'V', 'B',0,
       0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
      XvTopToBottom
    },
    { /* RGB 565 */
      0x36315652,
      XvRGB,
      LSBFirst,
      {'R','V','1','6',
       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
      16,
      XvPacked,
      1,
/*    16, 0x001F, 0x07E0, 0xF800, - incorrect!  */
      16, 0xF800, 0x07E0, 0x001F,
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
    CARD16  origPitch;

    CARD8   keyOP;
    CARD16  HUSF;
    CARD16  VUSF;
    CARD8   IntBit;
    CARD8   wHPre;

    CARD16  srcW;
    CARD16  srcH;

    BoxRec  dstBox;

    CARD32  PSY;
    CARD32  PSV;
    CARD32  PSU;
    CARD8   bobEnable;

    CARD8   contrastCtrl;
    CARD8   contrastFactor;

    CARD8   lineBufSize;

    CARD8   (*VBlankActiveFunc)(SISPtr);
#if 0
    CARD32  (*GetScanLineFunc)(SISPtr pSiS);
#endif

    CARD16  SCREENheight;

#if 0
    /* TW: The following are not used yet */
    CARD16  SubPictHUSF;        /* Subpicture scaling */
    CARD16  SubpictVUSF;
    CARD8   SubpictIntBit;
    CARD8   SubPictwHPre;
    CARD16  SubPictsrcW;       /* Subpicture source width */
    CARD16  SubPictsrcH;       /* Subpicture source height */
    BoxRec  SubPictdstBox;     /* SubPicture destination box */
    CARD32  SubPictAddr;       /* SubPicture address */
    CARD32  SubPictPitch;      /* SubPicture pitch */
    CARD32  SubPictOrigPitch;  /* SubPicture real pitch (needed for scaling twice) */
    CARD32  SubPictPreset;     /* Subpicture Preset */

    CARD32  MPEG_Y;	       /* MPEG Y Buffer Addr */
    CARD32  MPEG_UV;	       /* MPEG UV Buffer Addr */
#endif
    
} SISOverlayRec, *SISOverlayPtr;

typedef struct {
    FBLinearPtr  linear;	/* TW: We now use Linear, not Area */
    CARD32       bufAddr[2];

    unsigned char currentBuf;

    short drw_x, drw_y, drw_w, drw_h;
    short src_x, src_y, src_w, src_h;   
    int id;
    short srcPitch, height;
    
    char          brightness;
    unsigned char contrast;
    char 	  hue;
    char          saturation;

    RegionRec    clip;
    CARD32       colorKey;
    Bool 	 autopaintColorKey;

    CARD32       videoStatus;
    Time         offTime;
    Time         freeTime;

    CARD32       displayMode;
    Bool	 bridgeIsSlave;

    Bool         hasTwoOverlays;   /* TW: Chipset has two overlays */
    Bool         dualHeadMode;     /* TW: We're running in DHM */

    Bool         needToScale;      /* TW: Need to scale video */

    int          shiftValue;       /* 550/650 need word addr/pitch, 630 double word */

    short        oldx1, oldx2, oldy1, oldy2;
    int          mustwait;

    Bool         grabbedByV4L;	   /* V4L stuff */
    int          pitch;
    int          offset;

} SISPortPrivRec, *SISPortPrivPtr;        

#define GET_PORT_PRIVATE(pScrn) \
   (SISPortPrivPtr)((SISPTR(pScrn))->adaptor->pPortPrivates[0].ptr)

static void
SISSetPortDefaults (ScrnInfoPtr pScrn, SISPortPrivPtr pPriv)
{
    pPriv->colorKey    = 0x000101fe;
    pPriv->videoStatus = 0;
    pPriv->brightness  = 0;
    pPriv->contrast    = 4;
    pPriv->hue         = 0;
    pPriv->saturation  = 0;
    pPriv->autopaintColorKey = TRUE;
}

static void 
SISResetVideo(ScrnInfoPtr pScrn)
{
    SISPtr pSiS = SISPTR(pScrn);
    SISPortPrivPtr pPriv = GET_PORT_PRIVATE(pScrn);

    /* Unlock registers */
#ifdef UNLOCK_ALWAYS
    sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif
    if (getvideoreg (pSiS, Index_VI_Passwd) != 0xa1) {
        setvideoreg (pSiS, Index_VI_Passwd, 0x86);
        if (getvideoreg (pSiS, Index_VI_Passwd) != 0xa1)
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                       "Xv: Video password could not unlock registers\n");
    }

    /* Initialize first overlay (CRT1) ------------------------------- */

    /* Write-enable video registers */
    setvideoregmask(pSiS, Index_VI_Control_Misc2,         0x80, 0x81);

    /* Disable overlay */
    setvideoregmask(pSiS, Index_VI_Control_Misc0,         0x00, 0x02);

    /* Disable bobEnable */
    setvideoregmask(pSiS, Index_VI_Control_Misc1,         0x02, 0x02);

    /* Reset scale control and contrast */
    setvideoregmask(pSiS, Index_VI_Scale_Control,         0x60, 0x60);
    setvideoregmask(pSiS, Index_VI_Contrast_Enh_Ctrl,     0x04, 0x1F);
  
    setvideoreg(pSiS, Index_VI_Disp_Y_Buf_Preset_Low,     0x00);
    setvideoreg(pSiS, Index_VI_Disp_Y_Buf_Preset_Middle,  0x00);
    setvideoreg(pSiS, Index_VI_UV_Buf_Preset_Low,         0x00);
    setvideoreg(pSiS, Index_VI_UV_Buf_Preset_Middle,      0x00);
    setvideoreg(pSiS, Index_VI_Disp_Y_UV_Buf_Preset_High, 0x00);
    setvideoreg(pSiS, Index_VI_Play_Threshold_Low,        0x00);
    setvideoreg(pSiS, Index_VI_Play_Threshold_High,       0x00);

    /* Initialize second overlay (CRT2) ---- only for 630/730, 550, M650/651 */
    if (pPriv->hasTwoOverlays) {
    	/* Write-enable video registers */
    	setvideoregmask(pSiS, Index_VI_Control_Misc2,         0x81, 0x81);

    	/* Disable overlay */
    	setvideoregmask(pSiS, Index_VI_Control_Misc0,         0x00, 0x02);

    	/* Disable bobEnable */
    	setvideoregmask(pSiS, Index_VI_Control_Misc1,         0x02, 0x02);

    	/* Reset scale control and contrast */
    	setvideoregmask(pSiS, Index_VI_Scale_Control,         0x60, 0x60);
    	setvideoregmask(pSiS, Index_VI_Contrast_Enh_Ctrl,     0x04, 0x1F);

    	setvideoreg(pSiS, Index_VI_Disp_Y_Buf_Preset_Low,     0x00);
    	setvideoreg(pSiS, Index_VI_Disp_Y_Buf_Preset_Middle,  0x00);
    	setvideoreg(pSiS, Index_VI_UV_Buf_Preset_Low,         0x00);
    	setvideoreg(pSiS, Index_VI_UV_Buf_Preset_Middle,      0x00);
    	setvideoreg(pSiS, Index_VI_Disp_Y_UV_Buf_Preset_High, 0x00);
    	setvideoreg(pSiS, Index_VI_Play_Threshold_Low,        0x00);
    	setvideoreg(pSiS, Index_VI_Play_Threshold_High,       0x00);
    }

    /* set default properties for overlay 1 (CRT1) -------------------------- */
    setvideoregmask (pSiS, Index_VI_Control_Misc2,        0x00, 0x01);
    setvideoregmask (pSiS, Index_VI_Contrast_Enh_Ctrl,    0x04, 0x07);
    setvideoreg (pSiS, Index_VI_Brightness,               0x20);
    if (pSiS->VGAEngine == SIS_315_VGA) {
      	setvideoreg (pSiS, Index_VI_Hue,          	   0x00);
       	setvideoreg (pSiS, Index_VI_Saturation,            0x00);
    }

    /* set default properties for overlay 2(CRT2) only 630/730 and 550 ------ */
    if (pPriv->hasTwoOverlays) {
    	setvideoregmask (pSiS, Index_VI_Control_Misc2,        0x01, 0x01);
    	setvideoregmask (pSiS, Index_VI_Contrast_Enh_Ctrl,    0x04, 0x07);
    	setvideoreg (pSiS, Index_VI_Brightness,               0x20);
    	if (pSiS->VGAEngine == SIS_315_VGA) {
       		setvideoreg (pSiS, Index_VI_Hue,              0x00);
       		setvideoreg (pSiS, Index_VI_Saturation,       0x00);
    	}
    }
}

/* TW: Set display mode (single CRT1/CRT2, mirror).
 *     MIRROR mode is only available on chipsets with two overlays.
 *     On the other chipsets, if only CRT1 or only CRT2 are used,
 *     the correct display CRT is chosen automatically. If both
 *     CRT1 and CRT2 are connected, the user can choose between CRT1 and
 *     CRT2 by using the option XvOnCRT2.
 */
static void
set_dispmode(ScrnInfoPtr pScrn, SISPortPrivPtr pPriv)
{
    SISPtr pSiS = SISPTR(pScrn);

    pPriv->dualHeadMode = pPriv->bridgeIsSlave = FALSE;

    if(SiSBridgeIsInSlaveMode(pScrn)) pPriv->bridgeIsSlave = TRUE;

    if( (pSiS->VBFlags & VB_DISPMODE_MIRROR) ||
        ((pPriv->bridgeIsSlave) && (pSiS->VBFlags & DISPTYPE_DISP2)) )  {
	if(pPriv->hasTwoOverlays)
           pPriv->displayMode = DISPMODE_MIRROR;     /* TW: CRT1 + CRT2 (2 overlays) */
	else if(pSiS->XvOnCRT2)
	   pPriv->displayMode = DISPMODE_SINGLE2;
	else
	   pPriv->displayMode = DISPMODE_SINGLE1;
    } else {
#ifdef SISDUALHEAD
      if(pSiS->DualHeadMode) {
         pPriv->dualHeadMode = TRUE;
      	 if(pSiS->SecondHead)
	     /* TW: Slave is always CRT1 */
	     pPriv->displayMode = DISPMODE_SINGLE1;
	 else
	     /* TW: Master is always CRT2 */
	     pPriv->displayMode = DISPMODE_SINGLE2;
      } else
#endif
      if(pSiS->VBFlags & DISPTYPE_DISP1) {
      	pPriv->displayMode = DISPMODE_SINGLE1;  /* TW: CRT1 only */
      } else {
        pPriv->displayMode = DISPMODE_SINGLE2;  /* TW: CRT2 only */
      }
    }
}

static void
set_disptype_regs(ScrnInfoPtr pScrn, SISPortPrivPtr pPriv)
{
    SISPtr pSiS = SISPTR(pScrn);

    /* TW:
     *     SR06[7:6]
     *	      Bit 7: Enable overlay 2 on CRT2
     *	      Bit 6: Enable overlay 1 on CRT2
     *     SR32[7:6]
     *        Bit 7: DCLK/TCLK overlay 2
     *               0=DCLK (overlay on CRT1)
     *               1=TCLK (overlay on CRT2)
     *        Bit 6: DCLK/TCLK overlay 1
     *               0=DCLK (overlay on CRT1)
     *               1=TCLK (overlay on CRT2)
     *
     * On chipsets with two overlays, we can freely select and also
     * have a mirror mode. However, we use overlay 1 for CRT1 and
     * overlay 2 for CRT2.
     * For chipsets with only one overlay, user must choose whether
     * to display the overlay on CRT1 or CRT2 by setting XvOnCRT2
     * to TRUE (CRT2) or FALSE (CRT1). The hardware does not
     * support any kind of "Mirror" mode on these chipsets.
     */
#ifdef UNLOCK_ALWAYS
    sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif
    switch (pPriv->displayMode)
    {
        case DISPMODE_SINGLE1:				/* TW: CRT1 only */
	  if (pPriv->hasTwoOverlays) {
	      if (pPriv->dualHeadMode) {
	         setsrregmask (pSiS, 0x06, 0x00, 0x40);
      	         setsrregmask (pSiS, 0x32, 0x00, 0x40);
	      } else {
      	         setsrregmask (pSiS, 0x06, 0x00, 0xc0);
      	         setsrregmask (pSiS, 0x32, 0x00, 0xc0);
              }
	  } else {
	      setsrregmask (pSiS, 0x06, 0x00, 0xc0);
	      setsrregmask (pSiS, 0x32, 0x00, 0xc0);
	  }
	  break;
       	case DISPMODE_SINGLE2:  			/* TW: CRT2 only */
	  if (pPriv->hasTwoOverlays) {
	      if (pPriv->dualHeadMode) {
	         setsrregmask (pSiS, 0x06, 0x80, 0x80);
      	         setsrregmask (pSiS, 0x32, 0x80, 0x80);
	      } else {
   	         setsrregmask (pSiS, 0x06, 0x80, 0xc0);
      	         setsrregmask (pSiS, 0x32, 0x80, 0xc0);
	      }
	  } else {
              setsrregmask (pSiS, 0x06, 0x40, 0xc0);
	      setsrregmask (pSiS, 0x32, 0x40, 0xc0);
	  }
	  break;
    	case DISPMODE_MIRROR:				/* TW: CRT1 + CRT2 */
	default:					
          setsrregmask (pSiS, 0x06, 0x80, 0xc0);
      	  setsrregmask (pSiS, 0x32, 0x80, 0xc0);
	  break;
    }
}

static XF86VideoAdaptorPtr
SISSetupImageVideo(ScreenPtr pScreen)
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
    adapt->name = "SIS 300/310/325 series Video Overlay";
    adapt->nEncodings = 1;
    if(pSiS->Flags650 & SiS650_LARGEOVERLAY) {
       adapt->pEncodings = &DummyEncoding_M650;
    } else {
       adapt->pEncodings = &DummyEncoding;
    }
    adapt->nFormats = NUM_FORMATS;
    adapt->pFormats = SISFormats;
    adapt->nPorts = 1;
    adapt->pPortPrivates = (DevUnion*)(&adapt[1]);

    pPriv = (SISPortPrivPtr)(&adapt->pPortPrivates[1]);

    adapt->pPortPrivates[0].ptr = (pointer)(pPriv);
    adapt->nImages = NUM_IMAGES;
    if(pSiS->VGAEngine == SIS_300_VGA) {
       adapt->pAttributes = SISAttributes_300;
       adapt->nAttributes = NUM_ATTRIBUTES_300;
    } else {
       adapt->pAttributes = SISAttributes_325;
       adapt->nAttributes = NUM_ATTRIBUTES_325;
    }
    adapt->pImages = SISImages;
    adapt->PutVideo = NULL;
    adapt->PutStill = NULL;
    adapt->GetVideo = NULL;
    adapt->GetStill = NULL;
    adapt->StopVideo = SISStopVideo;
    adapt->SetPortAttribute = SISSetPortAttribute;
    adapt->GetPortAttribute = SISGetPortAttribute;
    adapt->QueryBestSize = SISQueryBestSize;
    adapt->PutImage = SISPutImage;
    adapt->QueryImageAttributes = SISQueryImageAttributes;

    pPriv->videoStatus = 0;
    pPriv->currentBuf  = 0;
    pPriv->linear      = NULL;
    pPriv->grabbedByV4L= FALSE;

    SISSetPortDefaults(pScrn, pPriv);

    /* gotta uninit this someplace */
    REGION_INIT(pScreen, &pPriv->clip, NullBox, 0); 

    pSiS->adaptor = adapt;

    pSiS->xvBrightness = MAKE_ATOM("XV_BRIGHTNESS");
    pSiS->xvContrast   = MAKE_ATOM("XV_CONTRAST");
    pSiS->xvColorKey   = MAKE_ATOM("XV_COLORKEY");
    if(pSiS->VGAEngine == SIS_315_VGA) {
       pSiS->xvSaturation = MAKE_ATOM("XV_SATURATION");
       pSiS->xvHue        = MAKE_ATOM("XV_HUE");
    }
    pSiS->xvAutopaintColorKey = MAKE_ATOM("XV_AUTOPAINT_COLORKEY");
    pSiS->xvSetDefaults       = MAKE_ATOM("XV_SET_DEFAULTS");

    /* TW: Setup chipset type helpers */
    if (pSiS->hasTwoOverlays)
       pPriv->hasTwoOverlays = TRUE;
    else
       pPriv->hasTwoOverlays = FALSE;

    /* TW: 300 series require double words for addresses and pitches,
     *     310/325 series accept word.
     */
    switch (pSiS->VGAEngine) {
    case SIS_315_VGA:
    	pPriv->shiftValue = 1;
	break;
    case SIS_300_VGA:
    default:
    	pPriv->shiftValue = 2;
	break;
    }

    /* Set displayMode according to VBFlags */
    set_dispmode(pScrn, pPriv);

    /* Set SR(06, 32) registers according to DISPMODE */
    set_disptype_regs(pScrn, pPriv);

    SISResetVideo(pScrn);

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
SISSetPortAttribute(ScrnInfoPtr pScrn, Atom attribute,
  		    INT32 value, pointer data)
{
  SISPortPrivPtr pPriv = (SISPortPrivPtr)data;
  SISPtr pSiS = SISPTR(pScrn);

  if(attribute == pSiS->xvBrightness) {
    if((value < -128) || (value > 127))
       return BadValue;
    pPriv->brightness = value;
  } else if(attribute == pSiS->xvContrast) {
    if((value < 0) || (value > 7))
       return BadValue;
    pPriv->contrast = value;
  } else if(attribute == pSiS->xvColorKey) {
    pPriv->colorKey = value;
    REGION_EMPTY(pScrn->pScreen, &pPriv->clip);
  } else if(attribute == pSiS->xvAutopaintColorKey) {
     if ((value < 0) || (value > 1))
       return BadValue;
     pPriv->autopaintColorKey = value;
  } else if(attribute == pSiS->xvSetDefaults) {
        SISSetPortDefaults(pScrn, pPriv);
  } else if(pSiS->VGAEngine == SIS_315_VGA) {
     if(attribute == pSiS->xvHue) {
       if((value < -8) || (value > 7))
         return BadValue;
       pPriv->hue = value;
     } else if(attribute == pSiS->xvSaturation) {
       if((value < -7) || (value > 7))
         return BadValue;
       pPriv->saturation = value;
     } else return BadMatch;
  } else return BadMatch;
  return Success;
}

static int 
SISGetPortAttribute(
  ScrnInfoPtr pScrn, 
  Atom attribute,
  INT32 *value, 
  pointer data
){
  SISPortPrivPtr pPriv = (SISPortPrivPtr)data;
  SISPtr pSiS = SISPTR(pScrn);

  if(attribute == pSiS->xvBrightness) {
    *value = pPriv->brightness;
  } else if(attribute == pSiS->xvContrast) {
    *value = pPriv->contrast;
  } else if(attribute == pSiS->xvColorKey) {
    *value = pPriv->colorKey;
  } else if (attribute == pSiS->xvAutopaintColorKey) {
    *value = (pPriv->autopaintColorKey) ? 1 : 0;
  } else if(pSiS->VGAEngine == SIS_315_VGA) {
    if(attribute == pSiS->xvHue) {
       *value = pPriv->hue;
    } else if(attribute == pSiS->xvSaturation) {
       *value = pPriv->saturation;
    } else return BadMatch;
  } else return BadMatch;
  return Success;
}

static void 
SISQueryBestSize(
  ScrnInfoPtr pScrn, 
  Bool motion,
  short vid_w, short vid_h, 
  short drw_w, short drw_h, 
  unsigned int *p_w, unsigned int *p_h, 
  pointer data
){
  *p_w = drw_w;
  *p_h = drw_h; 
}

static void
calc_scale_factor(SISOverlayPtr pOverlay, ScrnInfoPtr pScrn,
                 SISPortPrivPtr pPriv, int index, int iscrt2)
{
  SISPtr pSiS = SISPTR(pScrn);
  CARD32 I=0,mult=0;
  int flag=0;

  int dstW = pOverlay->dstBox.x2 - pOverlay->dstBox.x1;
  int dstH = pOverlay->dstBox.y2 - pOverlay->dstBox.y1;
  int srcW = pOverlay->srcW;
  int srcH = pOverlay->srcH;
  CARD16 LCDheight = pSiS->LCDheight;
  int srcPitch = pOverlay->origPitch;
  int origdstH = dstH;

  /* TW: Stretch image due to idiotic LCD "auto"-scaling on LVDS (and 630+301B) */
  if(pSiS->VBFlags & CRT2_LCD) {
     if(pPriv->bridgeIsSlave) {
  	if(pSiS->VBFlags & VB_LVDS) {
  	   dstH = (dstH * LCDheight) / pOverlay->SCREENheight;
        } else if( (pSiS->VGAEngine == SIS_300_VGA) &&
		   (pSiS->VBFlags & (VB_301B|VB_302B|VB_30xLV|VB_30xLVX)) ) {
  	   dstH = (dstH * LCDheight) / pOverlay->SCREENheight;
        }
     } else if(iscrt2) {
  	if (pSiS->VBFlags & VB_LVDS) {
   		dstH = (dstH * LCDheight) / pOverlay->SCREENheight;
		if (pPriv->displayMode == DISPMODE_MIRROR) flag = 1;
        } else if ( (pSiS->VGAEngine == SIS_300_VGA) &&
                    (pSiS->VBFlags & (VB_301B|VB_302B|VB_30xLV|VB_30xLVX)) ) {
    		dstH = (dstH * LCDheight) / pOverlay->SCREENheight;
		if (pPriv->displayMode == DISPMODE_MIRROR) flag = 1;
        }
     }
  }
  /* TW: For double scan modes, we need to double the height
   *     (Perhaps we also need to scale LVDS, but I'm not sure.)
   *     On 310/325 series, we need to double the width as well.
   *     Interlace mode vice versa.
   */
  if(pSiS->CurrentLayout.mode->Flags & V_DBLSCAN) {
	   	dstH = origdstH << 1;
		flag = 0;
		if(pSiS->VGAEngine == SIS_315_VGA) {
			dstW <<= 1;
		}
  }
  if(pSiS->CurrentLayout.mode->Flags & V_INTERLACE) {
  		dstH = origdstH >> 1;
		flag = 0;
  }

  if(dstW < OVERLAY_MIN_WIDTH) dstW = OVERLAY_MIN_WIDTH;
  if (dstW == srcW) {
        pOverlay->HUSF   = 0x00;
        pOverlay->IntBit = 0x05;
	pOverlay->wHPre  = 0;
  } else if (dstW > srcW) {
        dstW += 2;
        pOverlay->HUSF   = (srcW << 16) / dstW;
        pOverlay->IntBit = 0x04;
	pOverlay->wHPre  = 0;
  } else {
        int tmpW = dstW;

	/* TW: It seems, the hardware can't scale below factor .125 (=1/8) if the
	       pitch isn't a multiple of 256.
	       TODO: Test this on the 310/325 series!
	 */
	if((srcPitch % 256) || (srcPitch < 256)) {
	   if(((dstW * 1000) / srcW) < 125) dstW = tmpW = ((srcW * 125) / 1000) + 1;
	}

        I = 0;
        pOverlay->IntBit = 0x01;
        while (srcW >= tmpW) {
            tmpW <<= 1;
            I++;
        }
        pOverlay->wHPre = (CARD8)(I - 1);
        dstW <<= (I - 1);
        if ((srcW % dstW))
            pOverlay->HUSF = ((srcW - dstW) << 16) / dstW;
        else
            pOverlay->HUSF = 0x00;
  }

  if(dstH < OVERLAY_MIN_HEIGHT) dstH = OVERLAY_MIN_HEIGHT;
  if (dstH == srcH) {
        pOverlay->VUSF   = 0x00;
        pOverlay->IntBit |= 0x0A;
  } else if (dstH > srcH) {
        dstH += 0x02;
        pOverlay->VUSF = (srcH << 16) / dstH;
        pOverlay->IntBit |= 0x08;
  } else {
        CARD32 realI;

        I = realI = srcH / dstH;
        pOverlay->IntBit |= 0x02;

        if (I < 2) {
            pOverlay->VUSF = ((srcH - dstH) << 16) / dstH;
	    /* TW: Needed for LCD-scaling modes */
	    if ((flag) && (mult = (srcH / origdstH)) >= 2)
	    		pOverlay->pitch /= mult;
        } else {
#if 0
            if (((pOverlay->bobEnable & 0x08) == 0x00) &&
                (((srcPitch * I)>>2) > 0xFFF)){
                pOverlay->bobEnable |= 0x08;
                srcPitch >>= 1;
            }
#endif
            if (((srcPitch * I)>>2) > 0xFFF) {
                I = (0xFFF*2/srcPitch);
                pOverlay->VUSF = 0xFFFF;
            } else {
                dstH = I * dstH;
                if (srcH % dstH)
                    pOverlay->VUSF = ((srcH - dstH) << 16) / dstH;
                else
                    pOverlay->VUSF = 0x00;
            }
            /* set video frame buffer offset */
            pOverlay->pitch = (CARD16)(srcPitch*I);
        }
   }
}

static void
set_line_buf_size(SISOverlayPtr pOverlay)
{
    CARD8  preHIDF;
    CARD32 I;
    CARD32 line = pOverlay->srcW;

    if ( (pOverlay->pixelFormat == PIXEL_FMT_YV12) ||
         (pOverlay->pixelFormat == PIXEL_FMT_I420) )
    {
        preHIDF = pOverlay->wHPre & 0x07;
        switch (preHIDF)
        {
            case 3 :
                if ((line & 0xffffff00) == line)
                   I = (line >> 8);
                else
                   I = (line >> 8) + 1;
                pOverlay->lineBufSize = (CARD8)(I * 32 - 1);
                break;
            case 4 :
                if ((line & 0xfffffe00) == line)
                   I = (line >> 9);
                else
                   I = (line >> 9) + 1;
                pOverlay->lineBufSize = (CARD8)(I * 64 - 1);
                break;
            case 5 :
                if ((line & 0xfffffc00) == line)
                   I = (line >> 10);
                else
                   I = (line >> 10) + 1;
                pOverlay->lineBufSize = (CARD8)(I * 128 - 1);
                break;
            case 6 :
                if ((line & 0xfffff800) == line)
                   I = (line >> 11);
                else
                   I = (line >> 11) + 1;
                pOverlay->lineBufSize = (CARD8)(I * 256 - 1);
                break;
            default :
                if ((line & 0xffffff80) == line)
                   I = (line >> 7);
                else
                   I = (line >> 7) + 1;
                pOverlay->lineBufSize = (CARD8)(I * 16 - 1);
                break;
        }
    } else { /* YUV2, UYVY */
        if ((line & 0xffffff8) == line)
            I = (line >> 3);
        else
            I = (line >> 3) + 1;
        pOverlay->lineBufSize = (CARD8)(I - 1);
    }
}

static void
merge_line_buf(SISPtr pSiS, SISPortPrivPtr pPriv, Bool enable)
{
  if(enable) {
    switch (pPriv->displayMode){
    case DISPMODE_SINGLE1:
        if (pPriv->hasTwoOverlays) {
           if (pPriv->dualHeadMode) {
	       /* line merge */
	       setvideoregmask(pSiS, Index_VI_Control_Misc2, 0x00, 0x11);
      	       setvideoregmask(pSiS, Index_VI_Control_Misc1, 0x04, 0x04);
	   } else {
	       /* dual line merge */
	       setvideoregmask(pSiS, Index_VI_Control_Misc2, 0x10, 0x11);
      	       setvideoregmask(pSiS, Index_VI_Control_Misc1, 0x00, 0x04);
	   }
        } else {
      	   setvideoregmask(pSiS, Index_VI_Control_Misc2, 0x10, 0x11);
      	   setvideoregmask(pSiS, Index_VI_Control_Misc1, 0x00, 0x04);
	}
      	break;
    case DISPMODE_SINGLE2:
    	if (pPriv->hasTwoOverlays) {
	   if (pPriv->dualHeadMode) {
	      /* line merge */
	      setvideoregmask(pSiS, Index_VI_Control_Misc2, 0x01, 0x11);
     	      setvideoregmask(pSiS, Index_VI_Control_Misc1, 0x04, 0x04);
	   } else {
	      /* line merge */
      	      setvideoregmask(pSiS, Index_VI_Control_Misc2, 0x01, 0x11); 
     	      setvideoregmask(pSiS, Index_VI_Control_Misc1, 0x04, 0x04);
	   }
	} else {
	   setvideoregmask(pSiS, Index_VI_Control_Misc2, 0x10, 0x11);
      	   setvideoregmask(pSiS, Index_VI_Control_Misc1, 0x00, 0x04);
	}
     	break;
    case DISPMODE_MIRROR:
    default:
        /* line merge */
      	setvideoregmask(pSiS, Index_VI_Control_Misc2, 0x00, 0x11);
      	setvideoregmask(pSiS, Index_VI_Control_Misc1, 0x04, 0x04);
	if (pPriv->hasTwoOverlays) {
	   /* line merge */
      	   setvideoregmask(pSiS, Index_VI_Control_Misc2, 0x01, 0x11);
      	   setvideoregmask(pSiS, Index_VI_Control_Misc1, 0x04, 0x04);
	}
      	break;
    }
  } else {
    switch (pPriv->displayMode) {
    case DISPMODE_SINGLE1:
    	setvideoregmask(pSiS, Index_VI_Control_Misc2, 0x00, 0x11);
    	setvideoregmask(pSiS, Index_VI_Control_Misc1, 0x00, 0x04);
    	break;
    case DISPMODE_SINGLE2:
    	if (pPriv->hasTwoOverlays) {
    	   setvideoregmask(pSiS, Index_VI_Control_Misc2, 0x01, 0x11);
    	   setvideoregmask(pSiS, Index_VI_Control_Misc1, 0x00, 0x04);
	} else {
	   setvideoregmask(pSiS, Index_VI_Control_Misc2, 0x00, 0x11);
    	   setvideoregmask(pSiS, Index_VI_Control_Misc1, 0x00, 0x04);
	}
	break;
    case DISPMODE_MIRROR:
    default:
    	setvideoregmask(pSiS, Index_VI_Control_Misc2, 0x00, 0x11);
    	setvideoregmask(pSiS, Index_VI_Control_Misc1, 0x00, 0x04);
	if (pPriv->hasTwoOverlays) {
	   setvideoregmask(pSiS, Index_VI_Control_Misc2, 0x01, 0x11);
    	   setvideoregmask(pSiS, Index_VI_Control_Misc1, 0x00, 0x04);
	}
        break;
    }
  }
}

static void
set_format(SISPtr pSiS, SISOverlayPtr pOverlay)
{
    CARD8 fmt;

    switch (pOverlay->pixelFormat){
    case PIXEL_FMT_YV12:
    case PIXEL_FMT_I420:
        fmt = 0x0c;
        break;
    case PIXEL_FMT_YUY2:
        fmt = 0x28; 
        break;
    case PIXEL_FMT_UYVY:
        fmt = 0x08;
        break;
    case PIXEL_FMT_RGB5:   /* D[5:4] : 00 RGB555, 01 RGB 565 */
        fmt = 0x00;
	break;
    case PIXEL_FMT_RGB6:
        fmt = 0x10;
	break;
    default:
        fmt = 0x00;
        break;
    }
    setvideoregmask(pSiS, Index_VI_Control_Misc0, fmt, 0x7c);
}

static void
set_colorkey(SISPtr pSiS, CARD32 colorkey)
{
    CARD8 r, g, b;

    b = (CARD8)(colorkey & 0xFF);
    g = (CARD8)((colorkey>>8) & 0xFF);
    r = (CARD8)((colorkey>>16) & 0xFF);

    /* Activate the colorkey mode */
    setvideoreg(pSiS, Index_VI_Overlay_ColorKey_Blue_Min  ,(CARD8)b);
    setvideoreg(pSiS, Index_VI_Overlay_ColorKey_Green_Min ,(CARD8)g);
    setvideoreg(pSiS, Index_VI_Overlay_ColorKey_Red_Min   ,(CARD8)r);

    setvideoreg(pSiS, Index_VI_Overlay_ColorKey_Blue_Max  ,(CARD8)b);
    setvideoreg(pSiS, Index_VI_Overlay_ColorKey_Green_Max ,(CARD8)g);
    setvideoreg(pSiS, Index_VI_Overlay_ColorKey_Red_Max   ,(CARD8)r);
}

static void
set_brightness(SISPtr pSiS, CARD8 brightness)
{
    setvideoreg(pSiS, Index_VI_Brightness, brightness);
}

static void
set_contrast(SISPtr pSiS, CARD8 contrast)
{
    setvideoregmask(pSiS, Index_VI_Contrast_Enh_Ctrl, contrast, 0x07);
}

/* 310/325 series only */
static void
set_saturation(SISPtr pSiS, char saturation)
{
    CARD8 temp = 0;
    
    if(saturation < 0) {
    	temp |= 0x88;
	saturation = -saturation;
    }
    temp |= (saturation & 0x07);
    temp |= ((saturation & 0x07) << 4);
    
    setvideoreg(pSiS, Index_VI_Saturation, temp);
}

/* 310/325 series only */
static void
set_hue(SISPtr pSiS, CARD8 hue)
{
    setvideoreg(pSiS, Index_VI_Hue, (hue & 0x08) ? (hue ^ 0x07) : hue);
}

#ifdef NOT_YET_IMPLEMENTED /* ----------- TW: FOR FUTURE USE -------------------- */

/* TW: Set Alpha */
static void
set_alpha(SISPtr pSiS, CARD8 alpha)
{
    CARD8 data;

    data = getvideoreg(pSiS, Index_VI_Key_Overlay_OP);
    data &= 0x0F;
    setvideoreg(pSiS,Index_VI_Key_Overlay_OP, data | (alpha << 4));
}

/* TW: Set SubPicture Start Address (yet unused) */
static void
set_subpict_start_offset(SISPtr pSiS, SISOverlayPtr pOverlay, int index)
{
    CARD32 temp;
    CARD8  data;

    temp = pOverlay->SubPictAddr >> 4; /* TW: 630 <-> 315 shiftValue? */

    setvideoreg(pSiS,Index_VI_SubPict_Buf_Start_Low, temp & 0xFF);
    setvideoreg(pSiS,Index_VI_SubPict_Buf_Start_Middle, (temp>>8) & 0xFF);
    setvideoreg(pSiS,Index_VI_SubPict_Buf_Start_High, (temp>>16) & 0x3F);
    if (pSiS->VGAEngine == SIS_315_VGA) {
       setvideoreg(pSiS,Index_VI_SubPict_Start_Over, (temp>>22) & 0x01);
       /* Submit SubPict offset ? */
       /* data=getvideoreg(pSiS,Index_VI_Control_Misc3); */
       setvideoreg(pSiS,Index_VI_Control_Misc3, (1 << index) | 0x04);
    }
}

/* TW: Set SubPicture Pitch (yet unused) */
static void
set_subpict_pitch(SISPtr pSiS, SISOverlayPtr pOverlay, int index)
{
    CARD32 temp;
    CARD8  data;

    temp = pOverlay->SubPictPitch >> 4; /* TW: 630 <-> 315 shiftValue? */

    setvideoreg(pSiS,Index_VI_SubPict_Buf_Pitch, temp & 0xFF);
    if (pSiS->VGAEngine == SIS_315_VGA) {
       setvideoreg(pSiS,Index_VI_SubPict_Buf_Pitch_High, (temp>>8) & 0xFF);
       /* Submit SubPict pitch ? */
       /* data=getvideoreg(pSiS,Index_VI_Control_Misc3); */
       setvideoreg(pSiS,Index_VI_Control_Misc3, (1 << index) | 0x04);
    }
}

/* TW: Calculate and set SubPicture scaling (untested, unused yet) */
static void
set_subpict_scale_factor(SISOverlayPtr pOverlay, ScrnInfoPtr pScrn,
                         SISPortPrivPtr pPriv, int index, int iscrt2)
{
  SISPtr pSiS = SISPTR(pScrn);
  CARD32 I=0,mult=0;
  int flag=0;

  int dstW = pOverlay->SubPictdstBox.x2 - pOverlay->SubPictdstBox.x1;
  int dstH = pOverlay->SubPictdstBox.y2 - pOverlay->SubPictdstBox.y1;
  int srcW = pOverlay->SubPictsrcW;
  int srcH = pOverlay->SubPictsrcH;
  CARD16 LCDheight = pSiS->LCDheight;
  int srcPitch = pOverlay->SubPictOrigPitch;
  int origdstH = dstH;

  /* TW: Stretch image due to idiotic LCD "auto"-scaling */
  /* INCOMPLETE - See set_scale_factor() */
  if ( (pPriv->bridgeIsSlave) && (pSiS->VBFlags & CRT2_LCD) ) {
  	dstH = (dstH * LCDheight) / pOverlay->SCREENheight;
  } else if ((index) && (pSiS->VBFlags & CRT2_LCD)) {
   	dstH = (dstH * LCDheight) / pOverlay->SCREENheight;
	if (pPriv->displayMode == DISPMODE_MIRROR) flag = 1;
  }

  if (dstW == srcW) {
        pOverlay->SubPictHUSF   = 0x00;
        pOverlay->SubPictIntBit = 0x01;
  } else if (dstW > srcW) {
        pOverlay->SubPictHUSF   = (srcW << 16) / dstW;
        pOverlay->SubPictIntBit = 0x00;
  } else {
        int tmpW = dstW;

        I = 0x00;
        while (srcW >= tmpW) {
            tmpW <<= 1;
            I++;
        }
        pOverlay->SubPictwHPre = (CARD8)(I - 1);
        dstW <<= (I - 1);
        if ((srcW % dstW))
            pOverlay->SubPictHUSF = ((srcW - dstW) << 16) / dstW;
        else
            pOverlay->SubPictHUSF = 0x00;

	pOverlay->SubPictIntBit = 0x01;
  }

  if (dstH == srcH) {
        pOverlay->SubPictVUSF   = 0x00;
        pOverlay->SubPictIntBit |= 0x02;
  } else if (dstH > srcH) {
        dstH += 0x02;
        pOverlay->SubPictVUSF = (srcH << 16) / dstH;
     /* pOverlay->SubPictIntBit |= 0x00; */
  } else {
        CARD32 realI;

        I = realI = srcH / dstH;
        pOverlay->SubPictIntBit |= 0x02;

        if (I < 2) {
            pOverlay->SubPictVUSF = ((srcH - dstH) << 16) / dstH;
	    /* TW: Needed for LCD-scaling modes */
	    if ((flag) && (mult = (srcH / origdstH)) >= 2)
	    		pOverlay->SubPictPitch /= mult;
        } else {
            if (((srcPitch * I)>>2) > 0xFFF) {
                I = (0xFFF*2/srcPitch);
                pOverlay->SubPictVUSF = 0xFFFF;
            } else {
                dstH = I * dstH;
                if (srcH % dstH)
                    pOverlay->SubPictVUSF = ((srcH - dstH) << 16) / dstH;
                else
                    pOverlay->SubPictVUSF = 0x00;
            }
            /* set video frame buffer offset */
            pOverlay->SubPictPitch = (CARD16)(srcPitch*I);
        }
   }
   /* set SubPicture scale factor */
   setvideoreg (pSiS, Index_VI_SubPict_Hor_Scale_Low,  (CARD8)(pOverlay->SubPictHUSF));
   setvideoreg (pSiS, Index_VI_SubPict_Hor_Scale_High, (CARD8)((pOverlay->SubPictHUSF)>>8));
   setvideoreg (pSiS, Index_VI_SubPict_Vert_Scale_Low, (CARD8)(pOverlay->SubPictVUSF));
   setvideoreg (pSiS, Index_VI_SubPict_Vert_Scale_High,(CARD8)((pOverlay->SubPictVUSF)>>8));

   setvideoregmask (pSiS, Index_VI_SubPict_Scale_Control,
   				(pOverlay->SubPictIntBit << 3) |
				(pOverlay->SubPictwHPre), 0x7f);
}

/* TW: Set SubPicture Preset (yet unused) */
static void
set_subpict_preset(SISPtr pSiS, SISOverlayPtr pOverlay)
{
    CARD32 temp;
    CARD8  data;

    temp = pOverlay->SubPictPreset >> 4; /* TW: 630 <-> 315 ? */

    setvideoreg(pSiS,Index_VI_SubPict_Buf_Preset_Low, temp & 0xFF);
    setvideoreg(pSiS,Index_VI_SubPict_Buf_Preset_Middle, (temp>>8) & 0xFF);
    data = getvideoreg(pSiS,Index_VI_SubPict_Buf_Start_High);
    if (temp > 0xFFFF)
    	data |= 0x40;
    else
    	data &= ~0x40;
    setvideoreg(pSiS,Index_VI_SubPict_Buf_Start_High, data);
}

static void
enable_subpict_overlay(SISPtr pSiS, Bool enable)
{
   setvideoregmask(pSiS, Index_VI_SubPict_Scale_Control,
   		enable ? 0x40 : 0x00,
		0x40);
}

/* TW: Set overlay for subpicture */
static void
set_subpict_overlay(SISPtr pSiS, SISOverlayPtr pOverlay, SISPortPrivPtr pPriv, int index)
{
    ScrnInfoPtr pScrn = pSiS->pScrn;

    set_subpict_pitch(pSiS, &overlay, index);
    set_subpict_start_offset(pSiS, &overlay, index);
    set_subpict_scale_factor(&overlay, pScrn, pPriv, index);
    /* set_subpict_preset(pSiS, &overlay); */
    /* enable_subpict_overlay(pSiS, 1); */
}


/* TW: Set MPEG Field Preset (yet unused) */
static void
set_mpegfield_preset(SISPtr pSiS, SISOverlayPtr pOverlay)
{
    setvideoreg(pSiS,Index_MPEG_Y_Buf_Preset_Low, pOverlay->MPEG_Y & 0xFF);
    setvideoreg(pSiS,Index_MPEG_Y_Buf_Preset_Middle, (pOverlay->MPEG_Y>>8) & 0xFF);

    setvideoreg(pSiS,Index_MPEG_UV_Buf_Preset_Low, pOverlay->MPEG_UV & 0xFF);
    setvideoreg(pSiS,Index_MPEG_UV_Buf_Preset_Middle, (pOverlay->MPEG_UV>>8) & 0xFF);

    setvideoreg(pSiS,Index_MPEG_Y_UV_Buf_Preset_High,
    		((pOverlay->MPEG_Y>>16) & 0x0F) | ((pOverlay->MPEG_UV>>12) & 0xF0));
}

static void
set_mpegfield_scale(SISPtr pSiS, SISOverlayPtr pOverlay)
{
	/* Empty for now */
}

#endif /* ------------------------------------------------------------------- */

static void
set_overlay(SISPtr pSiS, SISOverlayPtr pOverlay, SISPortPrivPtr pPriv, int index)
{
    ScrnInfoPtr pScrn = pSiS->pScrn;

    CARD16 pitch=0;
    CARD8  h_over=0, v_over=0;
    CARD16 top, bottom, left, right;
    CARD16 screenX = pSiS->CurrentLayout.mode->HDisplay;
    CARD16 screenY = pSiS->CurrentLayout.mode->VDisplay;
    CARD8  data;
    CARD32 watchdog;

    top = pOverlay->dstBox.y1;
    bottom = pOverlay->dstBox.y2;
    if (bottom > screenY) {
        bottom = screenY;
    }

    left = pOverlay->dstBox.x1;
    right = pOverlay->dstBox.x2;
    if (right > screenX) {
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

    h_over = (((left>>8) & 0x0f) | ((right>>4) & 0xf0));
    v_over = (((top>>8) & 0x0f) | ((bottom>>4) & 0xf0));

    pitch = pOverlay->pitch >> pPriv->shiftValue;

    /* set line buffer size */
    setvideoreg(pSiS, Index_VI_Line_Buffer_Size, pOverlay->lineBufSize);

    /* set color key mode */
    setvideoregmask (pSiS, Index_VI_Key_Overlay_OP, pOverlay->keyOP, 0x0f);

    /* TW: We don't have to wait for vertical retrace in all cases */
    if(pPriv->mustwait) {
	watchdog = WATCHDOG_DELAY;
    	while (pOverlay->VBlankActiveFunc(pSiS) && --watchdog);
	watchdog = WATCHDOG_DELAY;
	while ((!pOverlay->VBlankActiveFunc(pSiS)) && --watchdog);
	if (!watchdog) xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
			"Xv: Waiting for vertical retrace timed-out\n");
    }

    /* Unlock address registers */
    data = getvideoreg(pSiS, Index_VI_Control_Misc1);
    setvideoreg (pSiS, Index_VI_Control_Misc1, data | 0x20);
    /* TEST: Is this required? */
    setvideoreg (pSiS, Index_VI_Control_Misc1, data | 0x20);
    /* TEST end */

    /* TEST: Is this required? */
    if (pSiS->Chipset == SIS_315_VGA)
    	setvideoreg (pSiS, Index_VI_Control_Misc3, 0x00);
    /* TEST end */

    /* Set Y buf pitch */
    setvideoreg (pSiS, Index_VI_Disp_Y_Buf_Pitch_Low, (CARD8)(pitch));
    setvideoregmask (pSiS, Index_VI_Disp_Y_UV_Buf_Pitch_Middle, (CARD8)(pitch>>8), 0x0f);

    /* Set Y start address */
    setvideoreg (pSiS, Index_VI_Disp_Y_Buf_Start_Low,    (CARD8)(pOverlay->PSY));
    setvideoreg (pSiS, Index_VI_Disp_Y_Buf_Start_Middle, (CARD8)((pOverlay->PSY)>>8));
    setvideoreg (pSiS, Index_VI_Disp_Y_Buf_Start_High,   (CARD8)((pOverlay->PSY)>>16));

    /* set 310/325 series overflow bits for Y plane */
    if (pSiS->VGAEngine == SIS_315_VGA) {
        setvideoreg (pSiS, Index_VI_Disp_Y_Buf_Pitch_High, (CARD8)(pitch>>12));
    	setvideoreg (pSiS, Index_VI_Y_Buf_Start_Over, ((CARD8)((pOverlay->PSY)>>24) & 0x01));
    }

    /* Set U/V data if using plane formats */
    if ( (pOverlay->pixelFormat == PIXEL_FMT_YV12) ||
    	 (pOverlay->pixelFormat == PIXEL_FMT_I420) )  {

        CARD32  PSU=0, PSV=0;

        PSU = pOverlay->PSU;
        PSV = pOverlay->PSV;

	/* Set U/V pitch */
	setvideoreg (pSiS, Index_VI_Disp_UV_Buf_Pitch_Low, (CARD8)(pitch >> 1));
        setvideoregmask (pSiS, Index_VI_Disp_Y_UV_Buf_Pitch_Middle, (CARD8)(pitch >> 5), 0xf0);

        /* set U/V start address */
        setvideoreg (pSiS, Index_VI_U_Buf_Start_Low,   (CARD8)PSU);
        setvideoreg (pSiS, Index_VI_U_Buf_Start_Middle,(CARD8)(PSU>>8));
        setvideoreg (pSiS, Index_VI_U_Buf_Start_High,  (CARD8)(PSU>>16));

        setvideoreg (pSiS, Index_VI_V_Buf_Start_Low,   (CARD8)PSV);
        setvideoreg (pSiS, Index_VI_V_Buf_Start_Middle,(CARD8)(PSV>>8));
        setvideoreg (pSiS, Index_VI_V_Buf_Start_High,  (CARD8)(PSV>>16));

	/* 310/325 series overflow bits */
	if (pSiS->VGAEngine == SIS_315_VGA) {
	   setvideoreg (pSiS, Index_VI_Disp_UV_Buf_Pitch_High, (CARD8)(pitch>>13));
	   setvideoreg (pSiS, Index_VI_U_Buf_Start_Over, ((CARD8)(PSU>>24) & 0x01));
	   setvideoreg (pSiS, Index_VI_V_Buf_Start_Over, ((CARD8)(PSV>>24) & 0x01));
	}
    }

    if (pSiS->VGAEngine == SIS_315_VGA) {
	/* Trigger register copy for 310 series */
	setvideoreg(pSiS, Index_VI_Control_Misc3, 1 << index);
    }

    /* set scale factor */
    setvideoreg (pSiS, Index_VI_Hor_Post_Up_Scale_Low, (CARD8)(pOverlay->HUSF));
    setvideoreg (pSiS, Index_VI_Hor_Post_Up_Scale_High,(CARD8)((pOverlay->HUSF)>>8));
    setvideoreg (pSiS, Index_VI_Ver_Up_Scale_Low,      (CARD8)(pOverlay->VUSF));
    setvideoreg (pSiS, Index_VI_Ver_Up_Scale_High,     (CARD8)((pOverlay->VUSF)>>8));

    setvideoregmask (pSiS, Index_VI_Scale_Control,     (pOverlay->IntBit << 3)
                                                      |(pOverlay->wHPre), 0x7f);

    /* set destination window position */
    setvideoreg(pSiS, Index_VI_Win_Hor_Disp_Start_Low, (CARD8)left);
    setvideoreg(pSiS, Index_VI_Win_Hor_Disp_End_Low,   (CARD8)right);
    setvideoreg(pSiS, Index_VI_Win_Hor_Over,           (CARD8)h_over);

    setvideoreg(pSiS, Index_VI_Win_Ver_Disp_Start_Low, (CARD8)top);
    setvideoreg(pSiS, Index_VI_Win_Ver_Disp_End_Low,   (CARD8)bottom);
    setvideoreg(pSiS, Index_VI_Win_Ver_Over,           (CARD8)v_over);

    setvideoregmask(pSiS, Index_VI_Control_Misc1, pOverlay->bobEnable, 0x1a);

    /* Lock the address registers */
    setvideoregmask(pSiS, Index_VI_Control_Misc1, 0x00, 0x20);
}

/* TW: Overlay MUST NOT be switched off while beam is over it */
static void
close_overlay(SISPtr pSiS, SISPortPrivPtr pPriv)
{
  CARD32 watchdog;

  if ((pPriv->displayMode == DISPMODE_SINGLE2) ||
      (pPriv->displayMode == DISPMODE_MIRROR)) {
     if (pPriv->hasTwoOverlays) {
     	setvideoregmask (pSiS, Index_VI_Control_Misc2, 0x01, 0x01);
     	watchdog = WATCHDOG_DELAY;
     	while(vblank_active_CRT2(pSiS) && --watchdog);
     	watchdog = WATCHDOG_DELAY;
     	while((!vblank_active_CRT2(pSiS)) && --watchdog);
     	setvideoregmask(pSiS, Index_VI_Control_Misc0, 0x00, 0x02);
     	watchdog = WATCHDOG_DELAY;
     	while(vblank_active_CRT2(pSiS) && --watchdog);
     	watchdog = WATCHDOG_DELAY;
     	while((!vblank_active_CRT2(pSiS)) && --watchdog);
     } else if (pPriv->displayMode == DISPMODE_SINGLE2) {
      	setvideoregmask (pSiS, Index_VI_Control_Misc2, 0x00, 0x01);
     	watchdog = WATCHDOG_DELAY;
     	while(vblank_active_CRT1(pSiS) && --watchdog);
     	watchdog = WATCHDOG_DELAY;
     	while((!vblank_active_CRT1(pSiS)) && --watchdog);
     	setvideoregmask(pSiS, Index_VI_Control_Misc0, 0x00, 0x02);
     	watchdog = WATCHDOG_DELAY;
     	while(vblank_active_CRT1(pSiS) && --watchdog);
     	watchdog = WATCHDOG_DELAY;
     	while((!vblank_active_CRT1(pSiS)) && --watchdog);
     }
  }
  if ((pPriv->displayMode == DISPMODE_SINGLE1) ||
      (pPriv->displayMode == DISPMODE_MIRROR)) {
     setvideoregmask (pSiS, Index_VI_Control_Misc2, 0x00, 0x01);
     watchdog = WATCHDOG_DELAY;
     while(vblank_active_CRT1(pSiS) && --watchdog);
     watchdog = WATCHDOG_DELAY;
     while((!vblank_active_CRT1(pSiS)) && --watchdog);
     setvideoregmask(pSiS, Index_VI_Control_Misc0, 0x00, 0x02);
     watchdog = WATCHDOG_DELAY;
     while(vblank_active_CRT1(pSiS) && --watchdog);
     watchdog = WATCHDOG_DELAY;
     while((!vblank_active_CRT1(pSiS)) && --watchdog);
  }
}

static void
SISDisplayVideo(ScrnInfoPtr pScrn, SISPortPrivPtr pPriv)
{
   SISPtr pSiS = SISPTR(pScrn);
   
   short srcPitch = pPriv->srcPitch;
   short height = pPriv->height;
   SISOverlayRec overlay; 
   int srcOffsetX=0, srcOffsetY=0;
   int sx, sy;
   int index = 0, iscrt2 = 0;

   memset(&overlay, 0, sizeof(overlay));
   overlay.pixelFormat = pPriv->id;
   overlay.pitch = overlay.origPitch = srcPitch;
   overlay.keyOP = 0x03;	/* DestKey mode */
   /* overlay.bobEnable = 0x02; */
   overlay.bobEnable = 0x00;    /* Disable BOB (whatever that is) */

   overlay.SCREENheight = pSiS->CurrentLayout.mode->VDisplay;
   
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
       overlay.PSY = pPriv->bufAddr[pPriv->currentBuf] + sx + sy*srcPitch;
       overlay.PSV = pPriv->bufAddr[pPriv->currentBuf] + height*srcPitch + ((sx + sy*srcPitch/2) >> 1);
       overlay.PSU = pPriv->bufAddr[pPriv->currentBuf] + height*srcPitch*5/4 + ((sx + sy*srcPitch/2) >> 1);
#ifdef SISDUALHEAD
       overlay.PSY += HEADOFFSET;
       overlay.PSV += HEADOFFSET;
       overlay.PSU += HEADOFFSET;
#endif
       overlay.PSY >>= pPriv->shiftValue;
       overlay.PSV >>= pPriv->shiftValue;
       overlay.PSU >>= pPriv->shiftValue;
       break;
     case PIXEL_FMT_I420:
       sx = (pPriv->src_x + srcOffsetX) & ~7;
       sy = (pPriv->src_y + srcOffsetY) & ~1;
       overlay.PSY = pPriv->bufAddr[pPriv->currentBuf] + sx + sy*srcPitch;
       overlay.PSV = pPriv->bufAddr[pPriv->currentBuf] + height*srcPitch*5/4 + ((sx + sy*srcPitch/2) >> 1);
       overlay.PSU = pPriv->bufAddr[pPriv->currentBuf] + height*srcPitch + ((sx + sy*srcPitch/2) >> 1);
#ifdef SISDUALHEAD
       overlay.PSY += HEADOFFSET;
       overlay.PSV += HEADOFFSET;
       overlay.PSU += HEADOFFSET;
#endif
       overlay.PSY >>= pPriv->shiftValue;
       overlay.PSV >>= pPriv->shiftValue;
       overlay.PSU >>= pPriv->shiftValue;
       break;
     case PIXEL_FMT_YUY2:
     case PIXEL_FMT_UYVY:
     case PIXEL_FMT_RGB6:
     case PIXEL_FMT_RGB5:
     default:
       sx = (pPriv->src_x + srcOffsetX) & ~1;
       sy = (pPriv->src_y + srcOffsetY);
       overlay.PSY = (pPriv->bufAddr[pPriv->currentBuf] + sx*2 + sy*srcPitch);
#ifdef SISDUALHEAD
       overlay.PSY += HEADOFFSET;
#endif
       overlay.PSY >>= pPriv->shiftValue;
       break;      
   }

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

   /* TW: setup dispmode (MIRROR, SINGLEx) */
   set_dispmode(pScrn, pPriv);

   /* TW: set display mode SR06,32 (CRT1, CRT2 or mirror) */
   set_disptype_regs(pScrn, pPriv);

   /* set (not only calc) merge line buffer */
   merge_line_buf(pSiS, pPriv, (overlay.srcW > 384));

   /* calculate (not set!) line buffer length */
   set_line_buf_size(&overlay);

   if (pPriv->displayMode == DISPMODE_SINGLE2) {
     if (pPriv->hasTwoOverlays) {
	  /* TW: On chips with two overlays we use
	   * overlay 2 for CRT2 */
      	  index = 1; iscrt2 = 1;
     } else {
     	  /* TW: On chips with only one overlay we
	   * use that only overlay for CRT2 */
          index = 0; iscrt2 = 1;
     }
     overlay.VBlankActiveFunc = vblank_active_CRT2;
     /* overlay.GetScanLineFunc = get_scanline_CRT2; */
   } else {
     index = 0; iscrt2 = 0;
     overlay.VBlankActiveFunc = vblank_active_CRT1;
     /* overlay.GetScanLineFunc = get_scanline_CRT1; */
   }

   /* TW: Do the following in a loop for CRT1 and CRT2 ----------------- */
MIRROR:

   /* calculate (not set!) scale factor */
   calc_scale_factor(&overlay, pScrn, pPriv, index, iscrt2);

   /* Select video1 (used for CRT1) or video2 (used for CRT2) */
   setvideoregmask(pSiS, Index_VI_Control_Misc2, index, 0x01);

   /* set format */
   set_format(pSiS, &overlay);

   /* set color key */
   set_colorkey(pSiS, pPriv->colorKey);

   /* set brightness, contrast, hue and saturation */
   set_brightness(pSiS, pPriv->brightness);
   set_contrast(pSiS, pPriv->contrast);
   if (pSiS->VGAEngine == SIS_315_VGA) {
   	set_hue(pSiS, pPriv->hue);
   	set_saturation(pSiS, pPriv->saturation);
   }

   /* set overlay */
   set_overlay(pSiS, &overlay, pPriv, index);

   /* enable overlay */
   setvideoregmask (pSiS, Index_VI_Control_Misc0, 0x02, 0x02);

   if(index == 0 &&
      pPriv->displayMode == DISPMODE_MIRROR &&
      pPriv->hasTwoOverlays) {
     index = 1; iscrt2 = 1;
     overlay.VBlankActiveFunc = vblank_active_CRT2;
     /* overlay.GetScanLineFunc = get_scanline_CRT2; */
     goto MIRROR;
   }
   pPriv->mustwait = 0;
}

static FBLinearPtr
SISAllocateOverlayMemory(
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

   new_linear = xf86AllocateOffscreenLinear(pScreen, size, 8,
                                            NULL, NULL, NULL);

   if(!new_linear) {
        int max_size;

        xf86QueryLargestOffscreenLinear(pScreen, &max_size, 8,
				       PRIORITY_EXTREME);

        if(max_size < size) return NULL;

        xf86PurgeUnlockedOffscreenAreas(pScreen);
        new_linear = xf86AllocateOffscreenLinear(pScreen, size, 8,
                                                 NULL, NULL, NULL);
   }
   if (!new_linear)
        xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	           "Xv: Failed to allocate %dK of video memory\n", size/1024);
#ifdef TWDEBUG
   else
        xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	           "Xv: Allocated %dK of video memory\n", size/1024);
#endif

   return new_linear;
}

static void
SISFreeOverlayMemory(ScrnInfoPtr pScrn)
{
    SISPortPrivPtr pPriv = GET_PORT_PRIVATE(pScrn);

    if(pPriv->linear) {
        xf86FreeOffscreenLinear(pPriv->linear);
	pPriv->linear = NULL;
    }
}

static void
SISStopVideo(ScrnInfoPtr pScrn, pointer data, Bool shutdown)
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
     SISFreeOverlayMemory(pScrn);
     pPriv->videoStatus = 0;
     pSiS->VideoTimerCallback = NULL;
  } else {
     if(pPriv->videoStatus & CLIENT_VIDEO_ON) {
       pPriv->videoStatus = OFF_TIMER | CLIENT_VIDEO_ON;
       pPriv->offTime = currentTime.milliseconds + OFF_DELAY;
       pSiS->VideoTimerCallback = SISVideoTimerCallback;
     }
  }
}

static int
SISPutImage(
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
       totalSize = (pPriv->srcPitch * height * 3) >> 1; /* Verified */
       break;
     case PIXEL_FMT_YUY2:
     case PIXEL_FMT_UYVY:
     case PIXEL_FMT_RGB6:
     case PIXEL_FMT_RGB5:
     default:
       pPriv->srcPitch = ((width << 1) + 3) & ~3;	/* Verified */
       /* Size = width * 2 * height */
       totalSize = pPriv->srcPitch * height;
   }

   /* allocate memory (we do doublebuffering) */
   if(!(pPriv->linear = SISAllocateOverlayMemory(pScrn, pPriv->linear,
						 totalSize<<1)))
	return BadAlloc;

   /* fixup pointers */
   pPriv->bufAddr[0] = (pPriv->linear->offset * depth);
   pPriv->bufAddr[1] = pPriv->bufAddr[0] + totalSize;

   /* copy data */
   /* TODO: subimage */
   memcpy(pSiS->FbBase + pPriv->bufAddr[pPriv->currentBuf], buf, totalSize);

   SISDisplayVideo(pScrn, pPriv);

   /* update cliplist */
   if(pPriv->autopaintColorKey &&
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

   pSiS->VideoTimerCallback = SISVideoTimerCallback;

   return Success;
}

static int
SISQueryImageAttributes(
  ScrnInfoPtr pScrn,
  int id,
  unsigned short *w, unsigned short *h,
  int *pitches, int *offsets
){
    int    pitchY, pitchUV;
    int    size, sizeY, sizeUV;
    SISPtr pSiS = SISPTR(pScrn);

    if(*w < IMAGE_MIN_WIDTH) *w = IMAGE_MIN_WIDTH;
    if(*h < IMAGE_MIN_HEIGHT) *h = IMAGE_MIN_HEIGHT;

    if(pSiS->Flags650 & SiS650_LARGEOVERLAY) {
       if(*w > IMAGE_MAX_WIDTH_M650) *w = IMAGE_MAX_WIDTH_M650;
       if(*h > IMAGE_MAX_HEIGHT_M650) *h = IMAGE_MAX_HEIGHT_M650;
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
    case PIXEL_FMT_RGB6:
    case PIXEL_FMT_RGB5:
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
SISVideoTimerCallback (ScrnInfoPtr pScrn, Time now)
{
    SISPtr         pSiS = SISPTR(pScrn);
    SISPortPrivPtr pPriv = NULL;
    unsigned char  sridx, cridx;

    pSiS->VideoTimerCallback = NULL;

    if(!pScrn->vtSema) return;

    if (pSiS->adaptor) {
    	pPriv = GET_PORT_PRIVATE(pScrn);
	if(!pPriv->videoStatus)
	   pPriv = NULL;
    }

    if (pPriv) {
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
	      pSiS->VideoTimerCallback = SISVideoTimerCallback;
          } else
	  if(pPriv->videoStatus & FREE_TIMER) {  
              SISFreeOverlayMemory(pScrn);
	      pPriv->mustwait = 1;
              pPriv->videoStatus = 0;
          }
        } else
	  pSiS->VideoTimerCallback = SISVideoTimerCallback;
      }
   }
}

/* TW: Offscreen surface stuff */

static int
SISAllocSurface (
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

#ifdef TWDEBUG
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Xv: SISAllocSurface called\n");
#endif

    if((w < IMAGE_MIN_WIDTH) || (h < IMAGE_MIN_HEIGHT))
          return BadValue;
    if(pSiS->Flags650 & SiS650_LARGEOVERLAY) {
       if((w > IMAGE_MAX_WIDTH_M650) || (h > IMAGE_MAX_HEIGHT_M650))
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
    size = h * pPriv->pitch; /*  / depth;   - Why? */
    pPriv->linear = SISAllocateOverlayMemory(pScrn, pPriv->linear, size);
    if(!pPriv->linear)
    	return BadAlloc;

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
SISStopSurface (XF86SurfacePtr surface)
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
SISFreeSurface (XF86SurfacePtr surface)
{
    SISPortPrivPtr pPriv = (SISPortPrivPtr)(surface->devPrivate.ptr);

    if(pPriv->grabbedByV4L) {
	SISStopSurface(surface);
	SISFreeOverlayMemory(surface->pScrn);
	pPriv->grabbedByV4L = FALSE;
    }
    return Success;
}

static int
SISGetSurfaceAttribute (
    ScrnInfoPtr pScrn,
    Atom attribute,
    INT32 *value
)
{
    SISPortPrivPtr pPriv = GET_PORT_PRIVATE(pScrn);

    return SISGetPortAttribute(pScrn, attribute, value, (pointer)pPriv);
}

static int
SISSetSurfaceAttribute(
    ScrnInfoPtr pScrn,
    Atom attribute,
    INT32 value
)
{
    SISPortPrivPtr pPriv = GET_PORT_PRIVATE(pScrn);;

    return SISSetPortAttribute(pScrn, attribute, value, (pointer)pPriv);
}

static int
SISDisplaySurface (
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

#ifdef TWDEBUG
   xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Xv: DisplaySurface called\n");
#endif

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

   SISDisplayVideo(pScrn, pPriv);

   if(pPriv->autopaintColorKey) {
   	XAAFillSolidRects(pScrn, pPriv->colorKey, GXcopy, ~0,
                    REGION_NUM_RECTS(clipBoxes),
                    REGION_RECTS(clipBoxes));
   }

   pPriv->videoStatus = CLIENT_VIDEO_ON;

   return Success;
}

#define NUMOFFSCRIMAGES 4

static XF86OffscreenImageRec SISOffscreenImages_300[NUMOFFSCRIMAGES] =
{
 {
   &SISImages[0],  	/* YUV2 */
   VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT,
   SISAllocSurface,
   SISFreeSurface,
   SISDisplaySurface,
   SISStopSurface,
   SISGetSurfaceAttribute,
   SISSetSurfaceAttribute,
   IMAGE_MAX_WIDTH, IMAGE_MAX_HEIGHT,
   NUM_ATTRIBUTES_300,
   &SISAttributes_300[0]  /* Support all attributes */
 },
 {
   &SISImages[2],	/* UYVY */
   VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT,
   SISAllocSurface,
   SISFreeSurface,
   SISDisplaySurface,
   SISStopSurface,
   SISGetSurfaceAttribute,
   SISSetSurfaceAttribute,
   IMAGE_MAX_WIDTH, IMAGE_MAX_HEIGHT,
   NUM_ATTRIBUTES_300,
   &SISAttributes_300[0]  /* Support all attributes */
 }
 ,
 {
   &SISImages[4],	/* RV15 */
   VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT,
   SISAllocSurface,
   SISFreeSurface,
   SISDisplaySurface,
   SISStopSurface,
   SISGetSurfaceAttribute,
   SISSetSurfaceAttribute,
   IMAGE_MAX_WIDTH, IMAGE_MAX_HEIGHT,
   NUM_ATTRIBUTES_300,
   &SISAttributes_300[0]  /* Support all attributes */
 },
 {
   &SISImages[5],	/* RV16 */
   VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT,
   SISAllocSurface,
   SISFreeSurface,
   SISDisplaySurface,
   SISStopSurface,
   SISGetSurfaceAttribute,
   SISSetSurfaceAttribute,
   IMAGE_MAX_WIDTH, IMAGE_MAX_HEIGHT,
   NUM_ATTRIBUTES_300,
   &SISAttributes_300[0]  /* Support all attributes */
 }
};

static XF86OffscreenImageRec SISOffscreenImages_325[NUMOFFSCRIMAGES] =
{
 {
   &SISImages[0],  	/* YUV2 */
   VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT,
   SISAllocSurface,
   SISFreeSurface,
   SISDisplaySurface,
   SISStopSurface,
   SISGetSurfaceAttribute,
   SISSetSurfaceAttribute,
   IMAGE_MAX_WIDTH, IMAGE_MAX_HEIGHT,
   NUM_ATTRIBUTES_325,
   &SISAttributes_325[0]  /* Support all attributes */
 },
 {
   &SISImages[2],	/* UYVY */
   VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT,
   SISAllocSurface,
   SISFreeSurface,
   SISDisplaySurface,
   SISStopSurface,
   SISGetSurfaceAttribute,
   SISSetSurfaceAttribute,
   IMAGE_MAX_WIDTH, IMAGE_MAX_HEIGHT,
   NUM_ATTRIBUTES_325,
   &SISAttributes_325[0]  /* Support all attributes */
 }
 ,
 {
   &SISImages[4],	/* RV15 */
   VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT,
   SISAllocSurface,
   SISFreeSurface,
   SISDisplaySurface,
   SISStopSurface,
   SISGetSurfaceAttribute,
   SISSetSurfaceAttribute,
   IMAGE_MAX_WIDTH, IMAGE_MAX_HEIGHT,
   NUM_ATTRIBUTES_325,
   &SISAttributes_325[0]  /* Support all attributes */
 },
 {
   &SISImages[5],	/* RV16 */
   VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT,
   SISAllocSurface,
   SISFreeSurface,
   SISDisplaySurface,
   SISStopSurface,
   SISGetSurfaceAttribute,
   SISSetSurfaceAttribute,
   IMAGE_MAX_WIDTH, IMAGE_MAX_HEIGHT,
   NUM_ATTRIBUTES_325,
   &SISAttributes_325[0]  /* Support all attributes */
 }
};

static XF86OffscreenImageRec SISOffscreenImages_M650[NUMOFFSCRIMAGES] =
{
 {
   &SISImages[0],  	/* YUV2 */
   VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT,
   SISAllocSurface,
   SISFreeSurface,
   SISDisplaySurface,
   SISStopSurface,
   SISGetSurfaceAttribute,
   SISSetSurfaceAttribute,
   IMAGE_MAX_WIDTH_M650, IMAGE_MAX_HEIGHT_M650,
   NUM_ATTRIBUTES_325,
   &SISAttributes_325[0]  /* Support all attributes */
 },
 {
   &SISImages[2],	/* UYVY */
   VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT,
   SISAllocSurface,
   SISFreeSurface,
   SISDisplaySurface,
   SISStopSurface,
   SISGetSurfaceAttribute,
   SISSetSurfaceAttribute,
   IMAGE_MAX_WIDTH_M650, IMAGE_MAX_HEIGHT_M650,
   NUM_ATTRIBUTES_325,
   &SISAttributes_325[0]  /* Support all attributes */
 }
 ,
 {
   &SISImages[4],	/* RV15 */
   VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT,
   SISAllocSurface,
   SISFreeSurface,
   SISDisplaySurface,
   SISStopSurface,
   SISGetSurfaceAttribute,
   SISSetSurfaceAttribute,
   IMAGE_MAX_WIDTH_M650, IMAGE_MAX_HEIGHT_M650,
   NUM_ATTRIBUTES_325,
   &SISAttributes_325[0]  /* Support all attributes */
 },
 {
   &SISImages[5],	/* RV16 */
   VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT,
   SISAllocSurface,
   SISFreeSurface,
   SISDisplaySurface,
   SISStopSurface,
   SISGetSurfaceAttribute,
   SISSetSurfaceAttribute,
   IMAGE_MAX_WIDTH_M650, IMAGE_MAX_HEIGHT_M650,
   NUM_ATTRIBUTES_325,
   &SISAttributes_325[0]  /* Support all attributes */
 }
};

static void
SISInitOffscreenImages(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    SISPtr pSiS = SISPTR(pScrn);
    
    if(pSiS->VGAEngine == SIS_300_VGA) {
       xf86XVRegisterOffscreenImages(pScreen, SISOffscreenImages_300, NUMOFFSCRIMAGES);
    } else {
       if(pSiS->Flags650 & SiS650_LARGEOVERLAY) {
          xf86XVRegisterOffscreenImages(pScreen, SISOffscreenImages_M650, NUMOFFSCRIMAGES);
       } else {
          xf86XVRegisterOffscreenImages(pScreen, SISOffscreenImages_325, NUMOFFSCRIMAGES);
       }
    }
}
