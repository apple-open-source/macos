/***************************************************************************
 
Copyright 2000 Intel Corporation.  All Rights Reserved. 

Permission is hereby granted, free of charge, to any person obtaining a 
copy of this software and associated documentation files (the 
"Software"), to deal in the Software without restriction, including 
without limitation the rights to use, copy, modify, merge, publish, 
distribute, sub license, and/or sell copies of the Software, and to 
permit persons to whom the Software is furnished to do so, subject to 
the following conditions: 

The above copyright notice and this permission notice (including the 
next paragraph) shall be included in all copies or substantial portions 
of the Software. 

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF 
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. 
IN NO EVENT SHALL INTEL, AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, 
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR 
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR 
THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/i810/i810_video.c,v 1.27 2003/11/11 00:58:18 dawes Exp $ */

/*
 * i810_video.c: i810 Xv driver. Based on the mga Xv driver by Mark Vojkovich.
 *
 * Authors: 
 * 	Jonathan Bian <jonathan.bian@intel.com>
 *      Offscreen Images:
 *        Matt Sottek <matthew.j.sottek@intel.com>
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

#include "i810.h"
#include "xf86xv.h"
#include "Xv.h"
#include "xaa.h"
#include "xaalocal.h"
#include "dixstruct.h"
#include "fourcc.h"

#define OFF_DELAY 	250  /* milliseconds */
#define FREE_DELAY 	15000

#define OFF_TIMER 	0x01
#define FREE_TIMER	0x02
#define CLIENT_VIDEO_ON	0x04

#define TIMER_MASK      (OFF_TIMER | FREE_TIMER)

static void I810InitOffscreenImages(ScreenPtr);

static XF86VideoAdaptorPtr I810SetupImageVideo(ScreenPtr);
static void I810StopVideo(ScrnInfoPtr, pointer, Bool);
static int I810SetPortAttribute(ScrnInfoPtr, Atom, INT32, pointer);
static int I810GetPortAttribute(ScrnInfoPtr, Atom ,INT32 *, pointer);
static void I810QueryBestSize(ScrnInfoPtr, Bool,
	short, short, short, short, unsigned int *, unsigned int *, pointer);
static int I810PutImage( ScrnInfoPtr, 
	short, short, short, short, short, short, short, short,
	int, unsigned char*, short, short, Bool, RegionPtr, pointer);
static int I810QueryImageAttributes(ScrnInfoPtr, 
	int, unsigned short *, unsigned short *,  int *, int *);

static void I810BlockHandler(int, pointer, pointer, pointer);

#define MAKE_ATOM(a) MakeAtom(a, sizeof(a) - 1, TRUE)

static Atom xvBrightness, xvContrast, xvColorKey;

#define IMAGE_MAX_WIDTH		1440
#define IMAGE_FAST_WIDTH	720
#define IMAGE_MAX_HEIGHT	1080
#define Y_BUF_SIZE		(IMAGE_MAX_WIDTH * IMAGE_MAX_HEIGHT)

#define OVERLAY_UPDATE(p)	OUTREG(0x30000, p | 0x80000000);

/*
 * OV0CMD - Overlay Command Register
 */
#define	VERTICAL_CHROMINANCE_FILTER 	0x70000000
#define VC_SCALING_OFF		0x00000000
#define VC_LINE_REPLICATION	0x10000000
#define VC_UP_INTERPOLATION	0x20000000
#define VC_PIXEL_DROPPING	0x50000000
#define VC_DOWN_INTERPOLATION	0x60000000
#define VERTICAL_LUMINANCE_FILTER	0x0E000000
#define VL_SCALING_OFF		0x00000000
#define VL_LINE_REPLICATION	0x02000000
#define VL_UP_INTERPOLATION	0x04000000
#define VL_PIXEL_DROPPING	0x0A000000
#define VL_DOWN_INTERPOLATION	0x0C000000
#define	HORIZONTAL_CHROMINANCE_FILTER 	0x01C00000
#define HC_SCALING_OFF		0x00000000
#define HC_LINE_REPLICATION	0x00400000
#define HC_UP_INTERPOLATION	0x00800000
#define HC_PIXEL_DROPPING	0x01400000
#define HC_DOWN_INTERPOLATION	0x01800000
#define HORIZONTAL_LUMINANCE_FILTER	0x00380000
#define HL_SCALING_OFF		0x00000000
#define HL_LINE_REPLICATION	0x00080000
#define HL_UP_INTERPOLATION	0x00100000
#define HL_PIXEL_DROPPING	0x00280000
#define HL_DOWN_INTERPOLATION	0x00300000

#define Y_ADJUST		0x00010000	
#define OV_BYTE_ORDER		0x0000C000
#define UV_SWAP			0x00004000
#define Y_SWAP			0x00008000
#define Y_AND_UV_SWAP		0x0000C000
#define SOURCE_FORMAT		0x00003C00
#define	RGB_555			0x00000800
#define	RGB_565			0x00000C00
#define	YUV_422			0x00002000
#define	YUV_411			0x00002400
#define	YUV_420			0x00003000
#define	YUV_410			0x00003800
#define BUFFER_AND_FIELD	0x00000006
#define	BUFFER0_FIELD0		0x00000000
#define	BUFFER1_FIELD0		0x00000004
#define OVERLAY_ENABLE		0x00000001

#define UV_VERT_BUF1 		0x02
#define UV_VERT_BUF0 		0x04

/*
 * DOV0STA - Display/Overlay 0 Status Register
 */
#define	DOV0STA 	0x30008

#define MINUV_SCALE	0x1

#define RGB16ToColorKey(c) \
	(((c & 0xF800) << 8) | ((c & 0x07E0) << 5) | ((c & 0x001F) << 3))

#define RGB15ToColorKey(c) \
        (((c & 0x7c00) << 9) | ((c & 0x03E0) << 6) | ((c & 0x001F) << 3))

void I810InitVideo(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    XF86VideoAdaptorPtr *adaptors, *newAdaptors = NULL;
    XF86VideoAdaptorPtr newAdaptor = NULL;
    int num_adaptors;
	
    if (pScrn->bitsPerPixel != 8) 
    {
	newAdaptor = I810SetupImageVideo(pScreen);
	I810InitOffscreenImages(pScreen);
    }

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

/* *INDENT-OFF* */
/* client libraries expect an encoding */
static XF86VideoEncodingRec DummyEncoding[1] =
{
 {
   0,
   "XV_IMAGE",
   IMAGE_MAX_WIDTH, IMAGE_MAX_HEIGHT,
   {1, 1}
 }
};

#define NUM_FORMATS 3

static XF86VideoFormatRec Formats[NUM_FORMATS] = 
{
  {15, TrueColor}, {16, TrueColor}, {24, TrueColor}
};

#define NUM_ATTRIBUTES 3

static XF86AttributeRec Attributes[NUM_ATTRIBUTES] =
{
   {XvSettable | XvGettable, 0, (1 << 24) - 1, "XV_COLORKEY"},
   {XvSettable | XvGettable, -128, 127, "XV_BRIGHTNESS"},
   {XvSettable | XvGettable, 0, 255, "XV_CONTRAST"}
};

#define NUM_IMAGES 6

#define I810_RV15 0x35315652
#define I810_RV16 0x36315652

static XF86ImageRec Images[NUM_IMAGES] =
{
   {
	I810_RV15,
        XvRGB,
	LSBFirst,
	{'R','V','1','5',
	  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
	16,
	XvPacked,
	1,
	15, 0x7C00, 0x03E0, 0x001F,
	0, 0, 0,
	0, 0, 0,
	0, 0, 0,
	{'R','V','B',0,
	  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
	XvTopToBottom
   },
   {
	I810_RV16,
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
	XVIMAGE_YUY2,
	XVIMAGE_YV12,
	XVIMAGE_I420,
	XVIMAGE_UYVY
};
/* *INDENT-ON* */

typedef struct {
    CARD32 OBUF_0Y;
    CARD32 OBUF_1Y;
    CARD32 OBUF_0U;
    CARD32 OBUF_0V;
    CARD32 OBUF_1U;
    CARD32 OBUF_1V;
    CARD32 OV0STRIDE;
    CARD32 YRGB_VPH;
    CARD32 UV_VPH;
    CARD32 HORZ_PH;
    CARD32 INIT_PH;
    CARD32 DWINPOS;
    CARD32 DWINSZ;
    CARD32 SWID;
    CARD32 SWIDQW;
    CARD32 SHEIGHT;
    CARD32 YRGBSCALE;
    CARD32 UVSCALE;
    CARD32 OV0CLRC0;
    CARD32 OV0CLRC1;
    CARD32 DCLRKV;
    CARD32 DCLRKM;
    CARD32 SCLRKVH;
    CARD32 SCLRKVL;
    CARD32 SCLRKM;
    CARD32 OV0CONF;
    CARD32 OV0CMD;
} I810OverlayRegRec, *I810OverlayRegPtr;

typedef struct {
	CARD32       YBuf0offset;
	CARD32       UBuf0offset;
	CARD32       VBuf0offset;

	CARD32       YBuf1offset;
	CARD32       UBuf1offset;
	CARD32       VBuf1offset;

	unsigned char currentBuf;

	int          brightness;
	int          contrast;

	RegionRec    clip;
	CARD32       colorKey;

	CARD32       videoStatus;
	Time         offTime;
	Time         freeTime;
	FBLinearPtr  linear;
} I810PortPrivRec, *I810PortPrivPtr;        

#define GET_PORT_PRIVATE(pScrn) \
   (I810PortPrivPtr)((I810PTR(pScrn))->adaptor->pPortPrivates[0].ptr)

static void I810ResetVideo(ScrnInfoPtr pScrn) 
{
    I810Ptr pI810 = I810PTR(pScrn);
    I810PortPrivPtr pPriv = pI810->adaptor->pPortPrivates[0].ptr;
    I810OverlayRegPtr overlay = (I810OverlayRegPtr) (pI810->FbBase + pI810->OverlayStart); 

    /*
     * Default to maximum image size in YV12
     */

    overlay->YRGB_VPH = 0;
    overlay->UV_VPH = 0;
    overlay->HORZ_PH = 0;
    overlay->INIT_PH = 0;
    overlay->DWINPOS = 0;
    overlay->DWINSZ = (IMAGE_MAX_HEIGHT << 16) | IMAGE_MAX_WIDTH; 
    overlay->SWID =  IMAGE_MAX_WIDTH | (IMAGE_MAX_WIDTH << 15);       
    overlay->SWIDQW = (IMAGE_MAX_WIDTH >> 3) | (IMAGE_MAX_WIDTH << 12);
    overlay->SHEIGHT = IMAGE_MAX_HEIGHT | (IMAGE_MAX_HEIGHT << 15);
    overlay->YRGBSCALE = 0x80004000; /* scale factor 1 */
    overlay->UVSCALE = 0x80004000; /* scale factor 1 */
    overlay->OV0CLRC0 = 0x4000; /* brightness: 0 contrast: 1.0 */
    overlay->OV0CLRC1 = 0x80; /* saturation: bypass */

    /*
     * Enable destination color keying
     */
    switch(pScrn->depth) {
    case 16: overlay->DCLRKV = RGB16ToColorKey(pPriv->colorKey);
             overlay->DCLRKM = 0x80070307;
             break;
    case 15: overlay->DCLRKV = RGB15ToColorKey(pPriv->colorKey);
             overlay->DCLRKM = 0x80070707;
             break;
    default: overlay->DCLRKV = pPriv->colorKey;
             overlay->DCLRKM = 0x80000000;
             break;
    }

    overlay->SCLRKVH = 0;
    overlay->SCLRKVL = 0;
    overlay->SCLRKM = 0; /* source color key disable */
    overlay->OV0CONF = 0; /* two 720 pixel line buffers */ 

    overlay->OV0CMD = VC_UP_INTERPOLATION | HC_UP_INTERPOLATION | Y_ADJUST |
		      YUV_420;

    OVERLAY_UPDATE(pI810->OverlayPhysical);
}


static XF86VideoAdaptorPtr 
I810SetupImageVideo(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    I810Ptr pI810 = I810PTR(pScrn);
    XF86VideoAdaptorPtr adapt;
    I810PortPrivPtr pPriv;

    if(!(adapt = xcalloc(1, sizeof(XF86VideoAdaptorRec) +
			    sizeof(I810PortPrivRec) +
			    sizeof(DevUnion))))
	return NULL;

    adapt->type = XvWindowMask | XvInputMask | XvImageMask;
    adapt->flags = VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT;
    adapt->name = "I810 Video Overlay";
    adapt->nEncodings = 1;
    adapt->pEncodings = DummyEncoding;
    adapt->nFormats = NUM_FORMATS;
    adapt->pFormats = Formats;
    adapt->nPorts = 1;
    adapt->pPortPrivates = (DevUnion*)(&adapt[1]);

    pPriv = (I810PortPrivPtr)(&adapt->pPortPrivates[1]);

    adapt->pPortPrivates[0].ptr = (pointer)(pPriv);
    adapt->pAttributes = Attributes;
    adapt->nImages = NUM_IMAGES;
    adapt->nAttributes = NUM_ATTRIBUTES;
    adapt->pImages = Images;
    adapt->PutVideo = NULL;
    adapt->PutStill = NULL;
    adapt->GetVideo = NULL;
    adapt->GetStill = NULL;
    adapt->StopVideo = I810StopVideo;
    adapt->SetPortAttribute = I810SetPortAttribute;
    adapt->GetPortAttribute = I810GetPortAttribute;
    adapt->QueryBestSize = I810QueryBestSize;
    adapt->PutImage = I810PutImage;
    adapt->QueryImageAttributes = I810QueryImageAttributes;

    pPriv->colorKey = pI810->colorKey & ((1 << pScrn->depth) - 1);
    pPriv->videoStatus = 0;
    pPriv->brightness = 0;
    pPriv->contrast = 64;
    pPriv->linear = NULL;
    pPriv->currentBuf = 0;

    /* gotta uninit this someplace */
    REGION_NULL(pScreen, &pPriv->clip);

    pI810->adaptor = adapt;

    pI810->BlockHandler = pScreen->BlockHandler;
    pScreen->BlockHandler = I810BlockHandler;

    xvBrightness = MAKE_ATOM("XV_BRIGHTNESS");
    xvContrast   = MAKE_ATOM("XV_CONTRAST");
    xvColorKey   = MAKE_ATOM("XV_COLORKEY");

    I810ResetVideo(pScrn);

    return adapt;
}


/* I810ClipVideo -  

   Takes the dst box in standard X BoxRec form (top and left
   edges inclusive, bottom and right exclusive).  The new dst
   box is returned.  The source boundaries are given (x1, y1 
   inclusive, x2, y2 exclusive) and returned are the new source 
   boundaries in 16.16 fixed point. 
*/

static void
I810ClipVideo(
  BoxPtr dst, 
  INT32 *x1, 
  INT32 *x2, 
  INT32 *y1, 
  INT32 *y2,
  BoxPtr extents,            /* extents of the clip region */
  INT32 width, 
  INT32 height
){
    INT32 vscale, hscale, delta;
    int diff;

    hscale = ((*x2 - *x1) << 16) / (dst->x2 - dst->x1);
    vscale = ((*y2 - *y1) << 16) / (dst->y2 - dst->y1);

    *x1 <<= 16; *x2 <<= 16;
    *y1 <<= 16; *y2 <<= 16;

    diff = extents->x1 - dst->x1;
    if(diff > 0) {
	dst->x1 = extents->x1;
	*x1 += diff * hscale;     
    }
    diff = dst->x2 - extents->x2;
    if(diff > 0) {
	dst->x2 = extents->x2;
	*x2 -= diff * hscale;     
    }
    diff = extents->y1 - dst->y1;
    if(diff > 0) {
	dst->y1 = extents->y1;
	*y1 += diff * vscale;     
    }
    diff = dst->y2 - extents->y2;
    if(diff > 0) {
	dst->y2 = extents->y2;
	*y2 -= diff * vscale;     
    }

    if(*x1 < 0) {
	diff =  (- *x1 + hscale - 1)/ hscale;
	dst->x1 += diff;
	*x1 += diff * hscale;
    }
    delta = *x2 - (width << 16);
    if(delta > 0) {
	diff = (delta + hscale - 1)/ hscale;
	dst->x2 -= diff;
	*x2 -= diff * hscale;
    }
    if(*y1 < 0) {
	diff =  (- *y1 + vscale - 1)/ vscale;
	dst->y1 += diff;
	*y1 += diff * vscale;
    }
    delta = *y2 - (height << 16);
    if(delta > 0) {
	diff = (delta + vscale - 1)/ vscale;
	dst->y2 -= diff;
	*y2 -= diff * vscale;
    }
} 

static void 
I810StopVideo(ScrnInfoPtr pScrn, pointer data, Bool shutdown)
{
  I810PortPrivPtr pPriv = (I810PortPrivPtr)data;
  I810Ptr pI810 = I810PTR(pScrn);

  I810OverlayRegPtr overlay = (I810OverlayRegPtr) (pI810->FbBase + pI810->OverlayStart); 

  REGION_EMPTY(pScrn->pScreen, &pPriv->clip);   

  if(shutdown) {
     if(pPriv->videoStatus & CLIENT_VIDEO_ON) {
	overlay->OV0CMD &= 0xFFFFFFFE;
	OVERLAY_UPDATE(pI810->OverlayPhysical);
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
     }
  }

}

static int 
I810SetPortAttribute(
  ScrnInfoPtr pScrn, 
  Atom attribute,
  INT32 value, 
  pointer data
){
  I810PortPrivPtr pPriv = (I810PortPrivPtr)data;
  I810Ptr pI810 = I810PTR(pScrn);
  I810OverlayRegPtr overlay = (I810OverlayRegPtr) (pI810->FbBase + pI810->OverlayStart); 

  if(attribute == xvBrightness) {
	if((value < -128) || (value > 127))
	   return BadValue;
	pPriv->brightness = value;
	overlay->OV0CLRC0 = (pPriv->contrast << 8) | (pPriv->brightness & 0xff);
	OVERLAY_UPDATE(pI810->OverlayPhysical);
  } else
  if(attribute == xvContrast) {
	if((value < 0) || (value > 255))
	   return BadValue;
	pPriv->contrast = value;
	overlay->OV0CLRC0 = (pPriv->contrast << 8) | (pPriv->brightness & 0xff);
	OVERLAY_UPDATE(pI810->OverlayPhysical);
  } else
  if(attribute == xvColorKey) {
	pPriv->colorKey = value;
	switch(pScrn->depth) {
	case 16: overlay->DCLRKV = RGB16ToColorKey(pPriv->colorKey);
	         break;
	case 15: overlay->DCLRKV = RGB15ToColorKey(pPriv->colorKey);
                 break;
	default: overlay->DCLRKV = pPriv->colorKey;
                 break;
	}
	OVERLAY_UPDATE(pI810->OverlayPhysical);
	REGION_EMPTY(pScrn->pScreen, &pPriv->clip);   
  } else return BadMatch;

  return Success;
}

static int 
I810GetPortAttribute(
  ScrnInfoPtr pScrn, 
  Atom attribute,
  INT32 *value, 
  pointer data
){
  I810PortPrivPtr pPriv = (I810PortPrivPtr)data;

  if(attribute == xvBrightness) {
	*value = pPriv->brightness;
  } else
  if(attribute == xvContrast) {
	*value = pPriv->contrast;
  } else
  if(attribute == xvColorKey) {
	*value = pPriv->colorKey;
  } else return BadMatch;

  return Success;
}

static void 
I810QueryBestSize(
  ScrnInfoPtr pScrn, 
  Bool motion,
  short vid_w, short vid_h, 
  short drw_w, short drw_h, 
  unsigned int *p_w, unsigned int *p_h, 
  pointer data
){
   if(vid_w > (drw_w << 1)) drw_w = vid_w >> 1;
   if(vid_h > (drw_h << 1)) drw_h = vid_h >> 1;

  *p_w = drw_w;
  *p_h = drw_h; 
}


static void
I810CopyPackedData(
   ScrnInfoPtr pScrn, 
   unsigned char *buf,
   int srcPitch,
   int dstPitch,
   int top,
   int left,
   int h,
   int w
   )
{
    I810Ptr pI810 = I810PTR(pScrn);
    I810PortPrivPtr pPriv = pI810->adaptor->pPortPrivates[0].ptr;
    unsigned char *src, *dst;
    
    src = buf + (top*srcPitch) + (left<<1);

    if (pPriv->currentBuf == 0)
	dst = pI810->FbBase + pPriv->YBuf0offset;
    else
	dst = pI810->FbBase + pPriv->YBuf1offset;

    w <<= 1;
    while(h--) {
	memcpy(dst, src, w);
	src += srcPitch;
	dst += dstPitch;
    }
}

static void
I810CopyPlanarData(
   ScrnInfoPtr pScrn, 
   unsigned char *buf,
   int srcPitch,
   int dstPitch,  /* of chroma */
   int srcH,
   int top,
   int left,
   int h,
   int w,
   int id
   )
{
    I810Ptr pI810 = I810PTR(pScrn);
    I810PortPrivPtr pPriv = pI810->adaptor->pPortPrivates[0].ptr;
    int i;
    unsigned char *src1, *src2, *src3, *dst1, *dst2, *dst3;

    /* Copy Y data */
    src1 = buf + (top*srcPitch) + left;
    if (pPriv->currentBuf == 0)
	dst1 = pI810->FbBase + pPriv->YBuf0offset;
    else
	dst1 = pI810->FbBase + pPriv->YBuf1offset;

    for (i = 0; i < h; i++) {
	memcpy(dst1, src1, w);
	src1 += srcPitch;
	dst1 += dstPitch << 1;
    }

    /* Copy V data for YV12, or U data for I420 */
    src2 = buf + (srcH*srcPitch) + ((top*srcPitch)>>2) + (left>>1);
    if (pPriv->currentBuf == 0) {
	if (id == FOURCC_I420)
	    dst2 = pI810->FbBase + pPriv->UBuf0offset;
	else
	    dst2 = pI810->FbBase + pPriv->VBuf0offset;
    } else {
	if (id == FOURCC_I420)
	    dst2 = pI810->FbBase + pPriv->UBuf1offset;
	else
	    dst2 = pI810->FbBase + pPriv->VBuf1offset;
    }

    for (i = 0; i < h/2; i++) {
	memcpy(dst2, src2, w/2);
	src2 += srcPitch>>1;
	dst2 += dstPitch;
    }

    /* Copy U data for YV12, or V data for I420 */
    src3 = buf + (srcH*srcPitch) + ((srcH*srcPitch)>>2) + ((top*srcPitch)>>2) + (left>>1);
    if (pPriv->currentBuf == 0) {
	if (id == FOURCC_I420) 
	    dst3 = pI810->FbBase + pPriv->VBuf0offset;
	else
	    dst3 = pI810->FbBase + pPriv->UBuf0offset;
    } else {
	if (id == FOURCC_I420) 
	    dst3 = pI810->FbBase + pPriv->VBuf1offset;
	else
	    dst3 = pI810->FbBase + pPriv->UBuf1offset;
    }
    
    for (i = 0; i < h/2; i++) {
	memcpy(dst3, src3, w/2);
	src3 += srcPitch>>1;
	dst3 += dstPitch;
    }
}

static void
I810DisplayVideo(
    ScrnInfoPtr pScrn,
    int id,
    short width, short height,
    int dstPitch,  /* of chroma for 4:2:0 */
    int x1, int y1, int x2, int y2,
    BoxPtr dstBox,
    short src_w, short src_h,
    short drw_w, short drw_h
){
    I810Ptr pI810 = I810PTR(pScrn);
    I810PortPrivPtr pPriv = pI810->adaptor->pPortPrivates[0].ptr;
    I810OverlayRegPtr overlay = (I810OverlayRegPtr) (pI810->FbBase + pI810->OverlayStart); 
    int xscaleInt, xscaleFract, yscaleInt, yscaleFract;
    int xscaleIntUV = 0, xscaleFractUV = 0, yscaleIntUV = 0, yscaleFractUV = 0;
    unsigned int swidth;

    switch(id) {
    case FOURCC_YV12:
    case FOURCC_I420:
	swidth = (width + 7) & ~7;
	overlay->SWID = (swidth << 15) | swidth;
	overlay->SWIDQW = (swidth << 12) | (swidth >> 3);
	break;
    case FOURCC_UYVY:
    case FOURCC_YUY2:
    default:
	swidth = ((width + 3) & ~3) << 1;
	overlay->SWID = swidth;
	overlay->SWIDQW = swidth >> 3;
	break;
    }

    /* wide video formats (>720 pixels) are special */
    if( swidth > IMAGE_FAST_WIDTH ) {
	overlay->OV0CONF = 1; /* one 1440 pixel line buffer */ 
    } else {
	overlay->OV0CONF = 0; /* two 720 pixel line buffers */
    }
	    
    overlay->SHEIGHT = height | (height << 15);
    overlay->DWINPOS = (dstBox->y1 << 16) | (dstBox->x1);
    overlay->DWINSZ = ((dstBox->y2 - dstBox->y1) << 16) | 
	              (dstBox->x2 - dstBox->x1);

    /* buffer locations */
    overlay->OBUF_0Y = pPriv->YBuf0offset;
    overlay->OBUF_1Y = pPriv->YBuf1offset;
    overlay->OBUF_0U = pPriv->UBuf0offset; 
    overlay->OBUF_0V = pPriv->VBuf0offset;
    overlay->OBUF_1U = pPriv->UBuf1offset;
    overlay->OBUF_1V = pPriv->VBuf1offset;

    /* 
     * Calculate horizontal and vertical scaling factors, default to 1:1
     */
    overlay->YRGBSCALE = 0x80004000;
    overlay->UVSCALE = 0x80004000;

    /* 
     * Initially, YCbCr and Overlay Enable and
     * vertical chrominance up interpolation and horozontal chrominance
     * up interpolation
     */
    overlay->OV0CMD = VC_UP_INTERPOLATION | HC_UP_INTERPOLATION | Y_ADJUST | 
	              OVERLAY_ENABLE; 

    if ((drw_w != src_w) || (drw_h != src_h)) 
    {
	xscaleInt = (src_w / drw_w) & 0x3;
	xscaleFract = (src_w << 12) / drw_w;
	yscaleInt = (src_h / drw_h) & 0x3;
	yscaleFract = (src_h << 12) / drw_h;

	overlay->YRGBSCALE = (xscaleInt << 15) | 
	                     ((xscaleFract & 0xFFF) << 3) |
	                     (yscaleInt) |
			     ((yscaleFract & 0xFFF) << 20);

	if (drw_w > src_w) 
	{
	    /* horizontal up-scaling */
	    overlay->OV0CMD &= ~HORIZONTAL_CHROMINANCE_FILTER;
	    overlay->OV0CMD &= ~HORIZONTAL_LUMINANCE_FILTER;
	    overlay->OV0CMD |= (HC_UP_INTERPOLATION | HL_UP_INTERPOLATION);
	}

	if (drw_h > src_h) 
	{ 
	    /* vertical up-scaling */
	    overlay->OV0CMD &= ~VERTICAL_CHROMINANCE_FILTER;
	    overlay->OV0CMD &= ~VERTICAL_LUMINANCE_FILTER;
	    overlay->OV0CMD |= (VC_UP_INTERPOLATION | VL_UP_INTERPOLATION);
	}

	if (drw_w < src_w) 
	{ 
	    /* horizontal down-scaling */
	    overlay->OV0CMD &= ~HORIZONTAL_CHROMINANCE_FILTER;
	    overlay->OV0CMD &= ~HORIZONTAL_LUMINANCE_FILTER;
	    overlay->OV0CMD |= (HC_DOWN_INTERPOLATION | HL_DOWN_INTERPOLATION);
	}

	if (drw_h < src_h) 
	{ 
	    /* vertical down-scaling */
	    overlay->OV0CMD &= ~VERTICAL_CHROMINANCE_FILTER;
	    overlay->OV0CMD &= ~VERTICAL_LUMINANCE_FILTER;
	    overlay->OV0CMD |= (VC_DOWN_INTERPOLATION | VL_DOWN_INTERPOLATION);
	}

	/* now calculate the UV scaling factor */

	if (xscaleFract)
	{
	    xscaleFractUV = xscaleFract >> MINUV_SCALE;
	    overlay->OV0CMD &= ~HC_DOWN_INTERPOLATION;
	    overlay->OV0CMD |= HC_UP_INTERPOLATION;
	}

	if (xscaleInt)
	{
	    xscaleIntUV = xscaleInt >> MINUV_SCALE;
	    if (xscaleIntUV)
	    {
		overlay->OV0CMD &= ~HC_UP_INTERPOLATION;
	    }
	}

	if (yscaleFract)
	{
	    yscaleFractUV = yscaleFract >> MINUV_SCALE;
	    overlay->OV0CMD &= ~VC_DOWN_INTERPOLATION;
	    overlay->OV0CMD |= VC_UP_INTERPOLATION;
	}

	if (yscaleInt)
	{
	    yscaleIntUV = yscaleInt >> MINUV_SCALE;
	    if (yscaleIntUV)
	    {
		overlay->OV0CMD &= ~VC_UP_INTERPOLATION;
		overlay->OV0CMD |= VC_DOWN_INTERPOLATION;
	    }
	}

	overlay->UVSCALE = yscaleIntUV | ((xscaleFractUV & 0xFFF) << 3) |
	                   ((yscaleFractUV & 0xFFF) << 20);
    }

    switch(id) {
    case FOURCC_YV12:
    case FOURCC_I420:
	/* set UV vertical phase to -0.25 */
	overlay->UV_VPH = 0x30003000;
	overlay->INIT_PH = UV_VERT_BUF0 | UV_VERT_BUF1;
	overlay->OV0STRIDE = (dstPitch << 1) | (dstPitch << 16);
	overlay->OV0CMD &= ~SOURCE_FORMAT;
	overlay->OV0CMD |= YUV_420;
	break;
    case I810_RV15:
    case I810_RV16:
	overlay->UV_VPH = 0;
	overlay->INIT_PH = 0;
	overlay->OV0STRIDE = dstPitch;
	overlay->OV0CMD &= ~SOURCE_FORMAT;
	overlay->OV0CMD |= (id==I810_RV15 ? RGB_555 : RGB_565);
	overlay->OV0CMD &= ~OV_BYTE_ORDER;
	break;
    case FOURCC_UYVY:
    case FOURCC_YUY2:
    default:
	overlay->UV_VPH = 0;
	overlay->INIT_PH = 0;
	overlay->OV0STRIDE = dstPitch;
	overlay->OV0CMD &= ~SOURCE_FORMAT;
	overlay->OV0CMD |= YUV_422;
	overlay->OV0CMD &= ~OV_BYTE_ORDER;
	if (id == FOURCC_UYVY)
	    overlay->OV0CMD |= Y_SWAP;
	break;
    }

    overlay->OV0CMD &= ~BUFFER_AND_FIELD;
    if (pPriv->currentBuf == 0)
	overlay->OV0CMD |= BUFFER0_FIELD0;
    else
	overlay->OV0CMD |= BUFFER1_FIELD0;

    OVERLAY_UPDATE(pI810->OverlayPhysical);

}

static FBLinearPtr
I810AllocateMemory(
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

   new_linear = xf86AllocateOffscreenLinear(pScreen, size, 4,
                                            NULL, NULL, NULL);

   if(!new_linear) {
        int max_size;

        xf86QueryLargestOffscreenLinear(pScreen, &max_size, 4, 
				       PRIORITY_EXTREME);

        if(max_size < size) return NULL;

        xf86PurgeUnlockedOffscreenAreas(pScreen);
        new_linear = xf86AllocateOffscreenLinear(pScreen, size, 4, 
                                                 NULL, NULL, NULL);
   } 

   return new_linear;
}

static int 
I810PutImage( 
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
    I810Ptr pI810 = I810PTR(pScrn);
    I810PortPrivPtr pPriv = (I810PortPrivPtr)data;
    INT32 x1, x2, y1, y2;
    int srcPitch, dstPitch;
    int top, left, npixels, nlines, size, loops;
    BoxRec dstBox;


    /* Clip */
    x1 = src_x;
    x2 = src_x + src_w;
    y1 = src_y;
    y2 = src_y + src_h;

    dstBox.x1 = drw_x;
    dstBox.x2 = drw_x + drw_w;
    dstBox.y1 = drw_y;
    dstBox.y2 = drw_y + drw_h;

    I810ClipVideo(&dstBox, &x1, &x2, &y1, &y2, 
		  REGION_EXTENTS(pScrn->pScreen, clipBoxes), width, height);
    
    if((x1 >= x2) || (y1 >= y2))
       return Success;
    /* 
     * Fix for 4 pixel granularity of AdjustFrame  
     * unless boarder is clipped  by frame 
     */
    dstBox.x1 -= (pScrn->frameX0 & 
		  ((dstBox.x1 == pScrn->frameX0) ? ~0x0UL : ~0x3UL));
    dstBox.x2 -= (pScrn->frameX0 & ~0x3);
    dstBox.y1 -= pScrn->frameY0; 
    dstBox.y2 -= pScrn->frameY0;

    switch(id) {
    case FOURCC_YV12:
    case FOURCC_I420:
	 srcPitch = (width + 3) & ~3;
	 dstPitch = ((width >> 1) + 7) & ~7;  /* of chroma */
	 size =  dstPitch * height * 3;	
	 break;
    case FOURCC_UYVY:
    case FOURCC_YUY2:
    default:
	 srcPitch = (width << 1);
	 dstPitch = (srcPitch + 7) & ~7;
	 size = dstPitch * height;
	 break;
    }  

    if(!(pPriv->linear = I810AllocateMemory(pScrn, pPriv->linear, 
		(pScrn->bitsPerPixel == 16) ? size : (size >> 1))))
	return BadAlloc;

    /* fixup pointers */
    pPriv->YBuf0offset = pPriv->linear->offset * pI810->cpp;
    pPriv->UBuf0offset = pPriv->YBuf0offset + (dstPitch * 2 * height);
    pPriv->VBuf0offset = pPriv->UBuf0offset + (dstPitch * height >> 1);
    
    pPriv->YBuf1offset = (pPriv->linear->offset * pI810->cpp) + size;
    pPriv->UBuf1offset = pPriv->YBuf1offset + (dstPitch * 2 * height);
    pPriv->VBuf1offset = pPriv->UBuf1offset + (dstPitch * height >> 1);


    /* Make sure this buffer isn't in use */
    loops = 0;
    while (loops < 1000000) {
          if(((INREG(DOV0STA)&0x00100000)>>20) == pPriv->currentBuf) {
            break;
          }
          loops++;
    }
    if(loops >= 1000000) {
      pPriv->currentBuf = !pPriv->currentBuf;
    }


    /* buffer swap */
    if (pPriv->currentBuf == 0)
        pPriv->currentBuf = 1;
    else
        pPriv->currentBuf = 0;

    /* copy data */
    top = y1 >> 16;
    left = (x1 >> 16) & ~1;
    npixels = ((((x2 + 0xffff) >> 16) + 1) & ~1) - left;

    switch(id) {
    case FOURCC_YV12:
    case FOURCC_I420:
	top &= ~1;
	nlines = ((((y2 + 0xffff) >> 16) + 1) & ~1) - top;
	I810CopyPlanarData(pScrn, buf, srcPitch, dstPitch,  height, top, left, 
                           nlines, npixels, id);
	break;
    case FOURCC_UYVY:
    case FOURCC_YUY2:
    default:
	nlines = ((y2 + 0xffff) >> 16) - top;
	I810CopyPackedData(pScrn, buf, srcPitch, dstPitch, top, left, nlines, 
                                                              npixels);
        break;
    }

    /* update cliplist */
    if(!REGION_EQUAL(pScrn->pScreen, &pPriv->clip, clipBoxes)) {
	REGION_COPY(pScrn->pScreen, &pPriv->clip, clipBoxes);
	/* draw these */
	xf86XVFillKeyHelper(pScrn->pScreen, pPriv->colorKey, clipBoxes);
    }

    I810DisplayVideo(pScrn, id, width, height, dstPitch, 
	     x1, y1, x2, y2, &dstBox, src_w, src_h, drw_w, drw_h);

    pPriv->videoStatus = CLIENT_VIDEO_ON;

    return Success;
}


static int 
I810QueryImageAttributes(
  ScrnInfoPtr pScrn, 
  int id, 
  unsigned short *w, unsigned short *h, 
  int *pitches, int *offsets
){
    int size, tmp;

    if(*w > IMAGE_MAX_WIDTH) *w = IMAGE_MAX_WIDTH;
    if(*h > IMAGE_MAX_HEIGHT) *h = IMAGE_MAX_HEIGHT;

    *w = (*w + 1) & ~1;
    if(offsets) offsets[0] = 0;

    switch(id) {
      /* IA44 is for XvMC only */
    case FOURCC_IA44:
    case FOURCC_AI44:
	if(pitches) pitches[0] = *w;
	size = *w * *h;
	break;
    case FOURCC_YV12:
    case FOURCC_I420:
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
    case FOURCC_UYVY:
    case FOURCC_YUY2:
    default:
	size = *w << 1;
	if(pitches) pitches[0] = size;
	size *= *h;
	break;
    }

    return size;
}

static void
I810BlockHandler (
    int i,
    pointer     blockData,
    pointer     pTimeout,
    pointer     pReadmask
){
    ScreenPtr   pScreen = screenInfo.screens[i];
    ScrnInfoPtr pScrn = xf86Screens[i];
    I810Ptr      pI810 = I810PTR(pScrn);
    I810PortPrivPtr pPriv = GET_PORT_PRIVATE(pScrn);
    I810OverlayRegPtr overlay = (I810OverlayRegPtr) (pI810->FbBase + pI810->OverlayStart); 

    pScreen->BlockHandler = pI810->BlockHandler;
    
    (*pScreen->BlockHandler) (i, blockData, pTimeout, pReadmask);

    pScreen->BlockHandler = I810BlockHandler;

    if(pPriv->videoStatus & TIMER_MASK) {
	UpdateCurrentTime();
	if(pPriv->videoStatus & OFF_TIMER) {
	    if(pPriv->offTime < currentTime.milliseconds) {
		/* Turn off the overlay */
		overlay->OV0CMD &= 0xFFFFFFFE;
		OVERLAY_UPDATE(pI810->OverlayPhysical);

		pPriv->videoStatus = FREE_TIMER;
		pPriv->freeTime = currentTime.milliseconds + FREE_DELAY;
	    }
	} else {  /* FREE_TIMER */
	    if(pPriv->freeTime < currentTime.milliseconds) {
		if(pPriv->linear) {
		   xf86FreeOffscreenLinear(pPriv->linear);
		   pPriv->linear = NULL;
		}
		pPriv->videoStatus = 0;
	    }
        }
    }
}


/***************************************************************************
 * Offscreen Images
 ***************************************************************************/

typedef struct {
  FBLinearPtr linear;
  Bool isOn;
} OffscreenPrivRec, * OffscreenPrivPtr;

static int 
I810AllocateSurface(
    ScrnInfoPtr pScrn,
    int id,
    unsigned short w, 	
    unsigned short h,
    XF86SurfacePtr surface
){
    FBLinearPtr linear;
    int pitch, fbpitch, size, bpp;
    OffscreenPrivPtr pPriv;
    I810Ptr pI810 = I810PTR(pScrn);

    if((w > 1024) || (h > 1024))
	return BadAlloc;

    w = (w + 1) & ~1;
    pitch = ((w << 1) + 15) & ~15;
    bpp = pScrn->bitsPerPixel >> 3;
    fbpitch = bpp * pScrn->displayWidth;
    size = ((pitch * h) + bpp - 1) / bpp;

    if(!(linear = I810AllocateMemory(pScrn, NULL, size)))
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

    memset(pI810->FbBase + surface->offsets[0],0,size);

    return Success;
}

static int 
I810StopSurface(
    XF86SurfacePtr surface
){
    OffscreenPrivPtr pPriv = (OffscreenPrivPtr)surface->devPrivate.ptr;

    if(pPriv->isOn) {
      I810Ptr pI810 = I810PTR(surface->pScrn);

      I810OverlayRegPtr overlay = (I810OverlayRegPtr) (pI810->FbBase + pI810->OverlayStart); 

      overlay->OV0CMD &= 0xFFFFFFFE;
      OVERLAY_UPDATE(pI810->OverlayPhysical);

      pPriv->isOn = FALSE;
    }

    return Success;
}


static int 
I810FreeSurface(
    XF86SurfacePtr surface
){
    OffscreenPrivPtr pPriv = (OffscreenPrivPtr)surface->devPrivate.ptr;

    if(pPriv->isOn) {
	I810StopSurface(surface);
    }
    xf86FreeOffscreenLinear(pPriv->linear);
    xfree(surface->pitches);
    xfree(surface->offsets);
    xfree(surface->devPrivate.ptr);

    return Success;
}

static int
I810GetSurfaceAttribute(
    ScrnInfoPtr pScrn,
    Atom attribute,
    INT32 *value
){
    return I810GetPortAttribute(pScrn, attribute, value, 0);
}

static int
I810SetSurfaceAttribute(
    ScrnInfoPtr pScrn,
    Atom attribute,
    INT32 value
){
    return I810SetPortAttribute(pScrn, attribute, value, 0);
}


static int 
I810DisplaySurface(
    XF86SurfacePtr surface,
    short src_x, short src_y, 
    short drw_x, short drw_y,
    short src_w, short src_h, 
    short drw_w, short drw_h,
    RegionPtr clipBoxes
){
    OffscreenPrivPtr pPriv = (OffscreenPrivPtr)surface->devPrivate.ptr;
    ScrnInfoPtr pScrn = surface->pScrn;
    I810Ptr      pI810 = I810PTR(pScrn);
    I810PortPrivPtr pI810Priv =  GET_PORT_PRIVATE(pScrn);

    INT32 x1, y1, x2, y2;
    INT32 loops = 0;
    BoxRec dstBox;

    x1 = src_x;
    x2 = src_x + src_w;
    y1 = src_y;
    y2 = src_y + src_h;

    dstBox.x1 = drw_x;
    dstBox.x2 = drw_x + drw_w;
    dstBox.y1 = drw_y;
    dstBox.y2 = drw_y + drw_h;

    I810ClipVideo(&dstBox, &x1, &x2, &y1, &y2,
		  REGION_EXTENTS(screenInfo.screens[0], clipBoxes),
		  surface->width, surface->height);

    /* 
     * Fix for 4 pixel granularity of AdjustFrame  
     * unless boarder is clipped  by frame 
     */
    dstBox.x1 -= (pScrn->frameX0 & 
		  ((dstBox.x1 == pScrn->frameX0) ? ~0x0UL : ~0x3UL));
    dstBox.x2 -= (pScrn->frameX0 & ~0x3);
    dstBox.y1 -= pScrn->frameY0;
    dstBox.y2 -= pScrn->frameY0;

    /* fixup pointers */
    pI810Priv->YBuf0offset = surface->offsets[0];
    pI810Priv->YBuf1offset = pI810Priv->YBuf0offset;

   /* wait for the last rendered buffer to be flipped in */
    while (((INREG(DOV0STA)&0x00100000)>>20) != pI810Priv->currentBuf) {
      if(loops == 200000) {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Overlay Lockup\n");
	break;
      }
      loops++;
    }

    /* buffer swap */
    if (pI810Priv->currentBuf == 0)
      pI810Priv->currentBuf = 1;
    else
      pI810Priv->currentBuf = 0;

    I810ResetVideo(pScrn);

    I810DisplayVideo(pScrn, surface->id, surface->width, surface->height,
		     surface->pitches[0], x1, y1, x2, y2, &dstBox,
		     src_w, src_h, drw_w, drw_h);

    xf86XVFillKeyHelper(pScrn->pScreen, pI810Priv->colorKey, clipBoxes);

    pPriv->isOn = TRUE;
    /* we've prempted the XvImage stream so set its free timer */
    if(pI810Priv->videoStatus & CLIENT_VIDEO_ON) {
      REGION_EMPTY(pScrn->pScreen, & pI810Priv->clip);   
      UpdateCurrentTime();
      pI810Priv->videoStatus = FREE_TIMER;
      pI810Priv->freeTime = currentTime.milliseconds + FREE_DELAY;
      pScrn->pScreen->BlockHandler = I810BlockHandler;
    }

    return Success;
}


static void 
I810InitOffscreenImages(ScreenPtr pScreen)
{
    XF86OffscreenImagePtr offscreenImages;

    /* need to free this someplace */
    if(!(offscreenImages = xalloc(sizeof(XF86OffscreenImageRec)))) {
      return;
    }

    offscreenImages[0].image = &Images[0];
    offscreenImages[0].flags = VIDEO_OVERLAID_IMAGES | 
			       VIDEO_CLIP_TO_VIEWPORT;
    offscreenImages[0].alloc_surface = I810AllocateSurface;
    offscreenImages[0].free_surface = I810FreeSurface;
    offscreenImages[0].display = I810DisplaySurface;
    offscreenImages[0].stop = I810StopSurface;
    offscreenImages[0].setAttribute = I810SetSurfaceAttribute;
    offscreenImages[0].getAttribute = I810GetSurfaceAttribute;
    offscreenImages[0].max_width = 1024;
    offscreenImages[0].max_height = 1024;
    offscreenImages[0].num_attributes = 1;
    offscreenImages[0].attributes = Attributes;

    xf86XVRegisterOffscreenImages(pScreen, offscreenImages, 1);
}

