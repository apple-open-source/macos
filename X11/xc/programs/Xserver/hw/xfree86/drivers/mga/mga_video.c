/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/mga/mga_video.c,v 1.29 2001/12/26 14:54:04 dawes Exp $ */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86Resources.h"
#include "xf86_ansic.h"
#include "compiler.h"
#include "xf86PciInfo.h"
#include "xf86Pci.h"
#include "xf86fbman.h"
#include "regionstr.h"

#include "mga_bios.h"
#include "mga_reg.h"
#include "mga.h"
#include "mga_macros.h"
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

#define MGA_MAX_PORTS	32

#ifndef XvExtension
void MGAInitVideo(ScreenPtr pScreen) {}
#else

static void MGAInitOffscreenImages(ScreenPtr);

static XF86VideoAdaptorPtr MGASetupImageVideoOverlay(ScreenPtr);
static int  MGASetPortAttributeOverlay(ScrnInfoPtr, Atom, INT32, pointer);
static int  MGAGetPortAttributeOverlay(ScrnInfoPtr, Atom ,INT32 *, pointer);

static XF86VideoAdaptorPtr MGASetupImageVideoTexture(ScreenPtr);
static int  MGASetPortAttributeTexture(ScrnInfoPtr, Atom, INT32, pointer);
static int  MGAGetPortAttributeTexture(ScrnInfoPtr, Atom ,INT32 *, pointer);

static void MGAStopVideo(ScrnInfoPtr, pointer, Bool);
static void MGAQueryBestSize(ScrnInfoPtr, Bool, short, short, short, short, 
			unsigned int *, unsigned int *, pointer);
static int  MGAPutImage(ScrnInfoPtr, short, short, short, short, short, 
			short, short, short, int, unsigned char*, short, 
			short, Bool, RegionPtr, pointer);
static int  MGAQueryImageAttributes(ScrnInfoPtr, int, unsigned short *, 
			unsigned short *,  int *, int *);


static void MGAResetVideoOverlay(ScrnInfoPtr);

static void MGAVideoTimerCallback(ScrnInfoPtr pScrn, Time time);


#define MAKE_ATOM(a) MakeAtom(a, sizeof(a) - 1, TRUE)

static Atom xvBrightness, xvContrast, xvColorKey, xvDoubleBuffer;

void MGAInitVideo(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    XF86VideoAdaptorPtr *adaptors, *newAdaptors = NULL;
    XF86VideoAdaptorPtr newAdaptor = NULL;
    MGAPtr pMga = MGAPTR(pScrn);
    int num_adaptors;

    if((pScrn->bitsPerPixel != 8) && !pMga->NoAccel &&
       (pMga->SecondCrtc == FALSE) &&
       ((pMga->Chipset == PCI_CHIP_MGAG200) ||
        (pMga->Chipset == PCI_CHIP_MGAG200_PCI) ||
        (pMga->Chipset == PCI_CHIP_MGAG400) ||
	(pMga->Chipset == PCI_CHIP_MGAG550))) 
    {
	if((pMga->Overlay8Plus24 || pMga->TexturedVideo) &&
	   (pScrn->bitsPerPixel != 24))
        {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Using texture video\n");
	    newAdaptor = MGASetupImageVideoTexture(pScreen);
	    pMga->TexturedVideo = TRUE;
	} else {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Using overlay video\n");
	    newAdaptor = MGASetupImageVideoOverlay(pScreen);
	    pMga->TexturedVideo = FALSE;
	}
	if(!pMga->Overlay8Plus24)
	    MGAInitOffscreenImages(pScreen);
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

/* client libraries expect an encoding */
static XF86VideoEncodingRec DummyEncoding[2] =
{
 {   /* overlay limit */
   0,
   "XV_IMAGE",
   1024, 1024,
   {1, 1}
 },
 {  /* texture limit */
   0,
   "XV_IMAGE",
   2046, 2046,
   {1, 1}
 }
};

#define NUM_FORMATS 6

static XF86VideoFormatRec Formats[NUM_FORMATS] = 
{
   {15, TrueColor}, {16, TrueColor}, {24, TrueColor},
   {15, DirectColor}, {16, DirectColor}, {24, DirectColor}
};

#define NUM_ATTRIBUTES_OVERLAY 4

static XF86AttributeRec Attributes[NUM_ATTRIBUTES_OVERLAY] =
{
   {XvSettable | XvGettable, 0, (1 << 24) - 1, "XV_COLORKEY"},
   {XvSettable | XvGettable, -128, 127, "XV_BRIGHTNESS"},
   {XvSettable | XvGettable, 0, 255, "XV_CONTRAST"},
   {XvSettable | XvGettable, 0, 1, "XV_DOUBLE_BUFFER"}
};

#define NUM_IMAGES 4

static XF86ImageRec Images[NUM_IMAGES] =
{
	XVIMAGE_YUY2,
	XVIMAGE_YV12,
	XVIMAGE_I420,
	XVIMAGE_UYVY
};

static void 
MGAResetVideoOverlay(ScrnInfoPtr pScrn) 
{
    MGAPtr pMga = MGAPTR(pScrn);
    MGAPortPrivPtr pPriv = pMga->portPrivate;

    CHECK_DMA_QUIESCENT(pMga, pScrn);
   
    outMGAdac(0x51, 0x01); /* keying on */
    outMGAdac(0x52, 0xff); /* full mask */
    outMGAdac(0x53, 0xff);
    outMGAdac(0x54, 0xff);

    outMGAdac(0x55, (pPriv->colorKey & pScrn->mask.red) >> 
		    pScrn->offset.red);
    outMGAdac(0x56, (pPriv->colorKey & pScrn->mask.green) >> 
		    pScrn->offset.green);
    outMGAdac(0x57, (pPriv->colorKey & pScrn->mask.blue) >> 
		    pScrn->offset.blue);

    OUTREG(MGAREG_BESLUMACTL, ((pPriv->brightness & 0xff) << 16) |
			       (pPriv->contrast & 0xff));
}


static XF86VideoAdaptorPtr
MGAAllocAdaptor(ScrnInfoPtr pScrn, Bool doublebuffer)
{
    XF86VideoAdaptorPtr adapt;
    MGAPtr pMga = MGAPTR(pScrn);
    MGAPortPrivPtr pPriv;
    int i;

    if(!(adapt = xf86XVAllocateVideoAdaptorRec(pScrn)))
	return NULL;

    if(!(pPriv = xcalloc(1, sizeof(MGAPortPrivRec) + 
			(sizeof(DevUnion) * MGA_MAX_PORTS)))) 
    {
	xfree(adapt);
	return NULL;
    }

    adapt->pPortPrivates = (DevUnion*)(&pPriv[1]);

    for(i = 0; i < MGA_MAX_PORTS; i++)
	adapt->pPortPrivates[i].val = i;

    xvBrightness = MAKE_ATOM("XV_BRIGHTNESS");
    xvContrast   = MAKE_ATOM("XV_CONTRAST");
    xvColorKey   = MAKE_ATOM("XV_COLORKEY");
    xvDoubleBuffer = MAKE_ATOM("XV_DOUBLE_BUFFER");   

    pPriv->colorKey = pMga->videoKey;
    pPriv->videoStatus = 0;
    pPriv->brightness = 0;
    pPriv->contrast = 128;
    pPriv->lastPort = -1;
    pPriv->doubleBuffer = doublebuffer;       
    pPriv->currentBuffer = 0;         

    pMga->adaptor = adapt;
    pMga->portPrivate = pPriv;

    return adapt;
}

static XF86VideoAdaptorPtr 
MGASetupImageVideoOverlay(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    MGAPtr pMga = MGAPTR(pScrn);
    XF86VideoAdaptorPtr adapt;

    adapt = MGAAllocAdaptor(pScrn, TRUE);

    adapt->type = XvWindowMask | XvInputMask | XvImageMask;
    adapt->flags = VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT;
    adapt->name = "Matrox G-Series Backend Scaler";
    adapt->nEncodings = 1;
    adapt->pEncodings = &DummyEncoding[0];
    adapt->nFormats = NUM_FORMATS;
    adapt->pFormats = Formats;
    adapt->nPorts = 1;
    adapt->pAttributes = Attributes;
    if (pMga->Chipset == PCI_CHIP_MGAG400 || 
	pMga->Chipset == PCI_CHIP_MGAG550) {
	adapt->nImages = 4;
	adapt->nAttributes = 4;
    } else {
	adapt->nImages = 3;
	adapt->nAttributes = 1;
    }
    adapt->pImages = Images;
    adapt->PutVideo = NULL;
    adapt->PutStill = NULL;
    adapt->GetVideo = NULL;
    adapt->GetStill = NULL;
    adapt->StopVideo = MGAStopVideo;
    adapt->SetPortAttribute = MGASetPortAttributeOverlay;
    adapt->GetPortAttribute = MGAGetPortAttributeOverlay;
    adapt->QueryBestSize = MGAQueryBestSize;
    adapt->PutImage = MGAPutImage;
    adapt->QueryImageAttributes = MGAQueryImageAttributes;

    /* gotta uninit this someplace */
    REGION_INIT(pScreen, &(pMga->portPrivate->clip), NullBox, 0); 

    MGAResetVideoOverlay(pScrn);

    return adapt;
}


static XF86VideoAdaptorPtr 
MGASetupImageVideoTexture(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    XF86VideoAdaptorPtr adapt;
    MGAPtr pMga = MGAPTR(pScrn);

    adapt = MGAAllocAdaptor(pScrn, FALSE);

    adapt->type = XvWindowMask | XvInputMask | XvImageMask;
    adapt->flags = 0;
    adapt->name = "Matrox G-Series Texture Engine";
    adapt->nEncodings = 1;
    adapt->pEncodings = &DummyEncoding[1];
    adapt->nFormats = NUM_FORMATS;
    adapt->pFormats = Formats;
    adapt->nPorts = MGA_MAX_PORTS;
    adapt->pAttributes = NULL;
    adapt->nAttributes = 0;
    adapt->pImages = Images;
    if (pMga->Chipset == PCI_CHIP_MGAG400 ||
	pMga->Chipset == PCI_CHIP_MGAG550)
	adapt->nImages = 4;
    else
	adapt->nImages = 3;
    adapt->PutVideo = NULL;
    adapt->PutStill = NULL;
    adapt->GetVideo = NULL;
    adapt->GetStill = NULL;
    adapt->StopVideo = MGAStopVideo;
    adapt->SetPortAttribute = MGASetPortAttributeTexture;
    adapt->GetPortAttribute = MGAGetPortAttributeTexture;
    adapt->QueryBestSize = MGAQueryBestSize;
    adapt->PutImage = MGAPutImage;
    adapt->QueryImageAttributes = MGAQueryImageAttributes;

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


/* MGAClipVideo -  

   Takes the dst box in standard X BoxRec form (top and left
   edges inclusive, bottom and right exclusive).  The new dst
   box is returned.  The source boundaries are given (x1, y1 
   inclusive, x2, y2 exclusive) and returned are the new source 
   boundaries in 16.16 fixed point. 
*/

#define DummyScreen screenInfo.screens[0]

static Bool
MGAClipVideo(
  BoxPtr dst, 
  INT32 *x1, 
  INT32 *x2, 
  INT32 *y1, 
  INT32 *y2,
  RegionPtr reg,
  INT32 width, 
  INT32 height
){
    INT32 vscale, hscale, delta;
    BoxPtr extents = REGION_EXTENTS(DummyScreen, reg);
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
    if(*x1 >= *x2) return FALSE;

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
    if(*y1 >= *y2) return FALSE;

    if((dst->x1 != extents->x1) || (dst->x2 != extents->x2) ||
       (dst->y1 != extents->y1) || (dst->y2 != extents->y2))
    {
	RegionRec clipReg;
	REGION_INIT(DummyScreen, &clipReg, dst, 1);
	REGION_INTERSECT(DummyScreen, reg, reg, &clipReg);
	REGION_UNINIT(DummyScreen, &clipReg);
    }
    return TRUE;
} 

static void 
MGAStopVideo(ScrnInfoPtr pScrn, pointer data, Bool shutdown)
{
  MGAPtr pMga = MGAPTR(pScrn);
  MGAPortPrivPtr pPriv = pMga->portPrivate;

  if(pMga->TexturedVideo) return;

  REGION_EMPTY(pScrn->pScreen, &pPriv->clip);   

  if(shutdown) {
     if(pPriv->videoStatus & CLIENT_VIDEO_ON)
	OUTREG(MGAREG_BESCTL, 0);
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
MGASetPortAttributeOverlay(
  ScrnInfoPtr pScrn, 
  Atom attribute,
  INT32 value, 
  pointer data
){
  MGAPtr pMga = MGAPTR(pScrn);
  MGAPortPrivPtr pPriv = pMga->portPrivate;

  CHECK_DMA_QUIESCENT(pMga, pScrn);

  if(attribute == xvBrightness) {
	if((value < -128) || (value > 127))
	   return BadValue;
	pPriv->brightness = value;
	OUTREG(MGAREG_BESLUMACTL, ((pPriv->brightness & 0xff) << 16) |
			           (pPriv->contrast & 0xff));
  } else
  if(attribute == xvContrast) {
	if((value < 0) || (value > 255))
	   return BadValue;
	pPriv->contrast = value;
	OUTREG(MGAREG_BESLUMACTL, ((pPriv->brightness & 0xff) << 16) |
			           (pPriv->contrast & 0xff));
  } else
  if(attribute == xvColorKey) {
	pPriv->colorKey = value;
	outMGAdac(0x55, (pPriv->colorKey & pScrn->mask.red) >> 
		    pScrn->offset.red);
	outMGAdac(0x56, (pPriv->colorKey & pScrn->mask.green) >> 
		    pScrn->offset.green);
	outMGAdac(0x57, (pPriv->colorKey & pScrn->mask.blue) >> 
		    pScrn->offset.blue);
	REGION_EMPTY(pScrn->pScreen, &pPriv->clip);   
  } else
  if(attribute == xvDoubleBuffer) {
	if((value < 0) || (value > 1))
          return BadValue;
	pPriv->doubleBuffer = value;  
  } else return BadMatch;

  return Success;
}

static int 
MGAGetPortAttributeOverlay(
  ScrnInfoPtr pScrn, 
  Atom attribute,
  INT32 *value, 
  pointer data
){
  MGAPtr pMga = MGAPTR(pScrn);
  MGAPortPrivPtr pPriv = pMga->portPrivate;

  if(attribute == xvBrightness) {
	*value = pPriv->brightness;
  } else
  if(attribute == xvContrast) {
	*value = pPriv->contrast;
  } else
  if(attribute == xvDoubleBuffer) {
        *value = pPriv->doubleBuffer ? 1 : 0;
  } else
  if(attribute == xvColorKey) {
	*value = pPriv->colorKey;
  } else return BadMatch;

  return Success;
}


static int 
MGASetPortAttributeTexture(
  ScrnInfoPtr pScrn, 
  Atom attribute,
  INT32 value, 
  pointer data
) {
  return BadMatch;
}


static int 
MGAGetPortAttributeTexture(
  ScrnInfoPtr pScrn, 
  Atom attribute,
  INT32 *value, 
  pointer data
){
  return BadMatch;
}

static void 
MGAQueryBestSize(
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
MGACopyData(
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
MGACopyMungedData(
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
   CARD32 *dst;
   CARD8 *s1, *s2, *s3;
   int i, j;

   w >>= 1;

   for(j = 0; j < h; j++) {
        dst = (CARD32*)dst1;
        s1 = src1;  s2 = src2;  s3 = src3;
        i = w;
        while(i > 4) {
           dst[0] = s1[0] | (s1[1] << 16) | (s3[0] << 8) | (s2[0] << 24);
           dst[1] = s1[2] | (s1[3] << 16) | (s3[1] << 8) | (s2[1] << 24);
           dst[2] = s1[4] | (s1[5] << 16) | (s3[2] << 8) | (s2[2] << 24);
           dst[3] = s1[6] | (s1[7] << 16) | (s3[3] << 8) | (s2[3] << 24);
           dst += 4; s2 += 4; s3 += 4; s1 += 8;
           i -= 4;
        }

        while(i--) {
           dst[0] = s1[0] | (s1[1] << 16) | (s3[0] << 8) | (s2[0] << 24);
           dst++; s2++; s3++;
           s1 += 2;
        }

        dst1 += dstPitch;
        src1 += srcPitch;
        if(j & 1) {
            src2 += srcPitch2;
            src3 += srcPitch2;
        }
   }
}


static FBLinearPtr
MGAAllocateMemory(
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
MGADisplayVideoOverlay(
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
    MGAPtr pMga = MGAPTR(pScrn);
    int tmp, hzoom, intrep;

    CHECK_DMA_QUIESCENT(pMga, pScrn);

    /* got 48 scanlines to do it in */
    tmp = INREG(MGAREG_VCOUNT) + 48;
    /* FIXME always change it in vertical retrace use CrtcV ?*/
    if(tmp > pScrn->currentMode->CrtcVTotal)
	tmp -= 49; /* too bad */
    else
        tmp = pScrn->currentMode->CrtcVTotal -1;

    tmp = pScrn->currentMode->VDisplay +1;
    /* enable accelerated 2x horizontal zoom when pixelclock >135MHz */
    hzoom = (pScrn->currentMode->Clock > 135000) ? 1 : 0;

    switch(id) {
    case FOURCC_UYVY:
	OUTREG(MGAREG_BESGLOBCTL, 0x000000c0 | (3 * hzoom) | (tmp << 16));
	break;
    case FOURCC_YUY2:
    default:
	OUTREG(MGAREG_BESGLOBCTL, 0x00000080 | (3 * hzoom) | (tmp << 16));
	break;
    }

    OUTREG(MGAREG_BESA1ORG, offset);

    if(y1 & 0x00010000)
	OUTREG(MGAREG_BESCTL, 0x00040c41);
    else 
	OUTREG(MGAREG_BESCTL, 0x00040c01);
 
    OUTREG(MGAREG_BESHCOORD, (dstBox->x1 << 16) | (dstBox->x2 - 1));
    OUTREG(MGAREG_BESVCOORD, (dstBox->y1 << 16) | (dstBox->y2 - 1));

    OUTREG(MGAREG_BESHSRCST, x1 & 0x03fffffc);
    OUTREG(MGAREG_BESHSRCEND, (x2 - 0x00010000) & 0x03fffffc);
    OUTREG(MGAREG_BESHSRCLST, (width - 1) << 16);
   
    OUTREG(MGAREG_BESPITCH, pitch >> 1);

    OUTREG(MGAREG_BESV1WGHT, y1 & 0x0000fffc);
    OUTREG(MGAREG_BESV1SRCLST, height - 1 - (y1 >> 16));

    intrep = ((drw_h == src_h) || (drw_h < 2)) ? 0 : 1;
    tmp = ((src_h - intrep) << 16)/(drw_h - intrep);
    if(tmp >= (32 << 16))
	tmp = (32 << 16) - 1;
    OUTREG(MGAREG_BESVISCAL, tmp & 0x001ffffc);

    intrep = ((drw_w == src_w) || (drw_w < 2)) ? 0 : 1;
    tmp = (((src_w - intrep) << 16)/(drw_w - intrep)) << hzoom;
    if(tmp >= (32 << 16))
	tmp = (32 << 16) - 1;
    OUTREG(MGAREG_BESHISCAL, tmp & 0x001ffffc);

}

static void
MGADisplayVideoTexture(
    ScrnInfoPtr pScrn,
    int id, int offset,
    int nbox, BoxPtr pbox,
    int width, int height, int pitch,
    short src_x, short src_y,
    short src_w, short src_h,
    short drw_x, short drw_y,
    short drw_w, short drw_h
){
    MGAPtr pMga = MGAPTR(pScrn);
    int log2w = 0, log2h = 0, i, incx, incy, padw, padh;
    
    pitch >>= 1;

    i = 12;
    while(--i) {
	if(width & (1 << i)) {
	    log2w = i;
	    if(width & ((1 << i) - 1)) 
		log2w++;
	    break;		
	}
    }

    i = 12;
    while(--i) {
	if(height & (1 << i)) {
	    log2h = i;
	    if(height & ((1 << i) - 1)) 
		log2h++;		
	    break;		
	}
    }

    padw = 1 << log2w;
    padh = 1 << log2h;
    incx = (src_w << 20)/(drw_w * padw);
    incy = (src_h << 20)/(drw_h * padh);
   
    CHECK_DMA_QUIESCENT(pMga, pScrn);

    if(pMga->Overlay8Plus24) {
	i = 0x00ffffff;
	WAITFIFO(1);
	SET_PLANEMASK(i);
    }

    WAITFIFO(15);
    OUTREG(MGAREG_TMR0, incx);  /* sx inc */
    OUTREG(MGAREG_TMR1, 0);  /* sy inc */
    OUTREG(MGAREG_TMR2, 0);  /* tx inc */
    OUTREG(MGAREG_TMR3, incy);  /* ty inc */
    OUTREG(MGAREG_TMR4, 0x00000000); 
    OUTREG(MGAREG_TMR5, 0x00000000);
    OUTREG(MGAREG_TMR8, 0x00010000);
    OUTREG(MGAREG_TEXORG, offset);
    OUTREG(MGAREG_TEXWIDTH,  log2w | (((8 - log2w) & 63) << 9) | 
				((width - 1) << 18));
    OUTREG(MGAREG_TEXHEIGHT, log2h | (((8 - log2h) & 63) << 9) | 
				((height - 1) << 18));
    if(id == FOURCC_UYVY)
	OUTREG(MGAREG_TEXCTL, 0x1A00010b | ((pitch & 0x07FF) << 9));
    else
	OUTREG(MGAREG_TEXCTL, 0x1A00010a | ((pitch & 0x07FF) << 9));
    OUTREG(MGAREG_TEXCTL2, 0x00000014);
    OUTREG(MGAREG_DWGCTL, 0x000c7076);   
    OUTREG(MGAREG_TEXFILTER, 0x01e00020);
    OUTREG(MGAREG_ALPHACTRL, 0x00000001);

    padw = (src_x << 20)/padw;
    padh = (src_y << 20)/padh;

    while(nbox--) {
	WAITFIFO(4);
	OUTREG(MGAREG_TMR6, (incx * (pbox->x1 - drw_x)) + padw);
	OUTREG(MGAREG_TMR7, (incy * (pbox->y1 - drw_y)) + padh);
	OUTREG(MGAREG_FXBNDRY, (pbox->x2 << 16) | (pbox->x1 & 0xffff));
	OUTREG(MGAREG_YDSTLEN + MGAREG_EXEC, 
				(pbox->y1 << 16) | (pbox->y2 - pbox->y1));
	pbox++;
    }

    pMga->AccelInfoRec->NeedToSync = TRUE;
}

static int 
MGAPutImage( 
  ScrnInfoPtr pScrn, 
  short src_x, short src_y, 
  short drw_x, short drw_y,
  short src_w, short src_h, 
  short drw_w, short drw_h,
  int id, unsigned char* buf, 
  short width, short height, 
  Bool Sync,
  RegionPtr clipBoxes, pointer data
){
   MGAPtr pMga = MGAPTR(pScrn);
   MGAPortPrivPtr pPriv = pMga->portPrivate;
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

   if(!MGAClipVideo(&dstBox, &x1, &x2, &y1, &y2, clipBoxes, width, height))
	return Success;

   if(!pMga->TexturedVideo) {
	dstBox.x1 -= pScrn->frameX0;
	dstBox.x2 -= pScrn->frameX0;
	dstBox.y1 -= pScrn->frameY0;
	dstBox.y2 -= pScrn->frameY0;
   }

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

   if(!(pPriv->linear = MGAAllocateMemory(pScrn, pPriv->linear, 
						pPriv->doubleBuffer ? (new_size << 1) : new_size)))
   {
	return BadAlloc;
   }
   pPriv->currentBuffer ^= 1;

    /* copy data */
   top = y1 >> 16;
   left = (x1 >> 16) & ~1;
   npixels = ((((x2 + 0xffff) >> 16) + 1) & ~1) - left;
   left <<= 1;

   offset = pPriv->linear->offset * bpp;
   if(pPriv->doubleBuffer)
        offset += pPriv->currentBuffer * new_size * bpp;
   dst_start = pMga->FbStart + offset + left + (top * dstPitch);

   if(pMga->TexturedVideo && pMga->AccelInfoRec->NeedToSync &&
	((long)data != pPriv->lastPort)) 
   {
	MGAStormSync(pScrn);
   }

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
	MGACopyMungedData(buf + (top * srcPitch) + (left >> 1), 
			  buf + offset2, buf + offset3, dst_start,
			  srcPitch, srcPitch2, dstPitch, nlines, npixels);
	break;
    case FOURCC_UYVY:
    case FOURCC_YUY2:
    default:
	buf += (top * srcPitch) + left;
	nlines = ((y2 + 0xffff) >> 16) - top;
	MGACopyData(buf, dst_start, srcPitch, dstPitch, nlines, npixels);
        break;
    }

    if(pMga->TexturedVideo) {
	pPriv->lastPort = (long)data;
	MGADisplayVideoTexture(pScrn, id, offset, 
		REGION_NUM_RECTS(clipBoxes), REGION_RECTS(clipBoxes),
		width, height, dstPitch, src_x, src_y, src_w, src_h,
		drw_x, drw_y, drw_w, drw_h);
	pPriv->videoStatus = FREE_TIMER;
	pPriv->freeTime = currentTime.milliseconds + FREE_DELAY;
    } else {
    /* update cliplist */
	if(!RegionsEqual(&pPriv->clip, clipBoxes)) {
	    REGION_COPY(pScreen, &pPriv->clip, clipBoxes);
	    /* draw these */
	    XAAFillSolidRects(pScrn, pPriv->colorKey, GXcopy, ~0, 
					REGION_NUM_RECTS(clipBoxes),
					REGION_RECTS(clipBoxes));
	}

	offset += top * dstPitch;
	MGADisplayVideoOverlay(pScrn, id, offset, width, height, dstPitch,
	     x1, y1, x2, y2, &dstBox, src_w, src_h, drw_w, drw_h);

	pPriv->videoStatus = CLIENT_VIDEO_ON;
    }
    pMga->VideoTimerCallback = MGAVideoTimerCallback;

    return Success;
}


static int 
MGAQueryImageAttributes(
    ScrnInfoPtr pScrn, 
    int id, 
    unsigned short *w, unsigned short *h, 
    int *pitches, int *offsets
){
    MGAPtr pMga = MGAPTR(pScrn);
    int size, tmp;

    if(pMga->TexturedVideo) {
	if(*w > 2046) *w = 2046;
	if(*h > 2046) *h = 2046;
    } else {
	if(*w > 1024) *w = 1024;
	if(*h > 1024) *h = 1024;
    }

    *w = (*w + 1) & ~1;
    if(offsets) offsets[0] = 0;

    switch(id) {
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
MGAVideoTimerCallback(ScrnInfoPtr pScrn, Time time)
{
    MGAPtr pMga = MGAPTR(pScrn);
    MGAPortPrivPtr pPriv = pMga->portPrivate;

    if(pPriv->videoStatus & TIMER_MASK) {
	if(pPriv->videoStatus & OFF_TIMER) {
	    if(pPriv->offTime < time) {
		OUTREG(MGAREG_BESCTL, 0);
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
	        pMga->VideoTimerCallback = NULL;
	    }
        }
    } else  /* shouldn't get here */
	pMga->VideoTimerCallback = NULL;
}


/****************** Offscreen stuff ***************/

typedef struct {
  FBLinearPtr linear;
  Bool isOn;
} OffscreenPrivRec, * OffscreenPrivPtr;

static int 
MGAAllocateSurface(
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

    if(!(linear = MGAAllocateMemory(pScrn, NULL, size)))
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
MGAStopSurface(
    XF86SurfacePtr surface
){
    OffscreenPrivPtr pPriv = (OffscreenPrivPtr)surface->devPrivate.ptr;

    if(pPriv->isOn) {
	MGAPtr pMga = MGAPTR(surface->pScrn);
	OUTREG(MGAREG_BESCTL, 0);
	pPriv->isOn = FALSE;
    }

    return Success;
}


static int 
MGAFreeSurface(
    XF86SurfacePtr surface
){
    OffscreenPrivPtr pPriv = (OffscreenPrivPtr)surface->devPrivate.ptr;

    if(pPriv->isOn)
	MGAStopSurface(surface);
    xf86FreeOffscreenLinear(pPriv->linear);
    xfree(surface->pitches);
    xfree(surface->offsets);
    xfree(surface->devPrivate.ptr);

    return Success;
}

static int
MGAGetSurfaceAttribute(
    ScrnInfoPtr pScrn,
    Atom attribute,
    INT32 *value
){
    return MGAGetPortAttributeOverlay(pScrn, attribute, value, 0);
}

static int
MGASetSurfaceAttribute(
    ScrnInfoPtr pScrn,
    Atom attribute,
    INT32 value
){
    return MGASetPortAttributeOverlay(pScrn, attribute, value, 0);
}


static int 
MGADisplaySurface(
    XF86SurfacePtr surface,
    short src_x, short src_y, 
    short drw_x, short drw_y,
    short src_w, short src_h, 
    short drw_w, short drw_h,
    RegionPtr clipBoxes
){
    OffscreenPrivPtr pPriv = (OffscreenPrivPtr)surface->devPrivate.ptr;
    ScrnInfoPtr pScrn = surface->pScrn;
    MGAPtr pMga = MGAPTR(pScrn);
    MGAPortPrivPtr portPriv = pMga->portPrivate;
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

    if(!MGAClipVideo(&dstBox, &x1, &x2, &y1, &y2, clipBoxes, 
			surface->width, surface->height))
    {
	return Success;
    }

    dstBox.x1 -= pScrn->frameX0;
    dstBox.x2 -= pScrn->frameX0;
    dstBox.y1 -= pScrn->frameY0;
    dstBox.y2 -= pScrn->frameY0;

    MGAResetVideoOverlay(pScrn);

    MGADisplayVideoOverlay(pScrn, surface->id, surface->offsets[0], 
	     surface->width, surface->height, surface->pitches[0],
	     x1, y1, x2, y2, &dstBox, src_w, src_h, drw_w, drw_h);

    XAAFillSolidRects(pScrn, portPriv->colorKey, GXcopy, ~0, 
                                        REGION_NUM_RECTS(clipBoxes),
                                        REGION_RECTS(clipBoxes));

    pPriv->isOn = TRUE;
    /* we've prempted the XvImage stream so set its free timer */
    if(portPriv->videoStatus & CLIENT_VIDEO_ON) {
	REGION_EMPTY(pScrn->pScreen, &portPriv->clip);   
	UpdateCurrentTime();
	portPriv->videoStatus = FREE_TIMER;
	portPriv->freeTime = currentTime.milliseconds + FREE_DELAY;
	pMga->VideoTimerCallback = MGAVideoTimerCallback;
    }

    return Success;
}


static void 
MGAInitOffscreenImages(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    MGAPtr pMga = MGAPTR(pScrn);
    int num = (pMga->Chipset == PCI_CHIP_MGAG400 || pMga->Chipset == PCI_CHIP_MGAG550) ? 2 : 1;
    XF86OffscreenImagePtr offscreenImages;

    /* need to free this someplace */
    if(!(offscreenImages = xalloc(num * sizeof(XF86OffscreenImageRec))))
	return;

    offscreenImages[0].image = &Images[0];
    offscreenImages[0].flags = VIDEO_OVERLAID_IMAGES | 
			       VIDEO_CLIP_TO_VIEWPORT;
    offscreenImages[0].alloc_surface = MGAAllocateSurface;
    offscreenImages[0].free_surface = MGAFreeSurface;
    offscreenImages[0].display = MGADisplaySurface;
    offscreenImages[0].stop = MGAStopSurface;
    offscreenImages[0].setAttribute = MGASetSurfaceAttribute;
    offscreenImages[0].getAttribute = MGAGetSurfaceAttribute;
    offscreenImages[0].max_width = 1024;
    offscreenImages[0].max_height = 1024;
    offscreenImages[0].num_attributes = (num == 1) ? 1 : 4;
    offscreenImages[0].attributes = Attributes;

    if(num == 2) {
	offscreenImages[1].image = &Images[3];
	offscreenImages[1].flags = VIDEO_OVERLAID_IMAGES | 
				   VIDEO_CLIP_TO_VIEWPORT;
	offscreenImages[1].alloc_surface = MGAAllocateSurface;
	offscreenImages[1].free_surface = MGAFreeSurface;
	offscreenImages[1].display = MGADisplaySurface;
	offscreenImages[1].stop = MGAStopSurface;
	offscreenImages[1].setAttribute = MGASetSurfaceAttribute;
	offscreenImages[1].getAttribute = MGAGetSurfaceAttribute;
	offscreenImages[1].max_width = 1024;
	offscreenImages[1].max_height = 1024;
	offscreenImages[1].num_attributes = 4;
	offscreenImages[1].attributes = Attributes;
    }

    xf86XVRegisterOffscreenImages(pScreen, offscreenImages, num);
}

#endif  /* !XvExtension */
