/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/s3virge/s3v_xv.c,v 1.11 2003/11/10 18:22:25 tsi Exp $ */
/*
Copyright (C) 2000 The XFree86 Project, Inc.  All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FIT-
NESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
XFREE86 PROJECT BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the XFree86 Project shall not
be used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the XFree86 Project.
*/

/*
 * s3v_xv.c
 * X Video Extension support
 *
 * S3 ViRGE driver
 *
 * 7/2000 Kevin Brosius
 *
 * Useful references:
 * X Video extension support -> xc/programs/hw/xfree86/common/xf86xv.c
 *
 */


	/* Most xf86 commons are already in s3v.h */
#include	"s3v.h"

#if 0
#define OFF_DELAY 	250  /* milliseconds */
#define FREE_DELAY 	15000

#define OFF_TIMER 	0x01
#define FREE_TIMER	0x02
#endif
#define CLIENT_VIDEO_ON	0x04

#define S3V_MAX_PORTS 1

#if 0
static void S3VInitOffscreenImages(ScreenPtr);
#endif

static XF86VideoAdaptorPtr S3VAllocAdaptor(ScrnInfoPtr pScrn);
static XF86VideoAdaptorPtr S3VSetupImageVideoOverlay(ScreenPtr);
static int  S3VSetPortAttributeOverlay(ScrnInfoPtr, Atom, INT32, pointer);
static int  S3VGetPortAttributeOverlay(ScrnInfoPtr, Atom ,INT32 *, pointer);

#if 0
static XF86VideoAdaptorPtr MGASetupImageVideoTexture(ScreenPtr);
static int  MGASetPortAttributeTexture(ScrnInfoPtr, Atom, INT32, pointer);
static int  MGAGetPortAttributeTexture(ScrnInfoPtr, Atom ,INT32 *, pointer);
#endif
static void S3VStopVideo(ScrnInfoPtr, pointer, Bool);
static void S3VQueryBestSize(ScrnInfoPtr, Bool, short, short, short, short, 
			unsigned int *, unsigned int *, pointer);
static int  S3VPutImage(ScrnInfoPtr, short, short, short, short, short, 
			short, short, short, int, unsigned char*, short, 
			short, Bool, RegionPtr, pointer);
static int  S3VQueryImageAttributes(ScrnInfoPtr, int, unsigned short *, 
			unsigned short *,  int *, int *);

#if 0
static void MGABlockHandler(int, pointer, pointer, pointer);
#endif

static void S3VResetVideoOverlay(ScrnInfoPtr);

#if 0
#define MAKE_ATOM(a) MakeAtom(a, sizeof(a) - 1, TRUE)

static Atom xvBrightness, xvContrast, xvColorKey;

#endif /* 0 */

int S3VQueryXvCapable(ScrnInfoPtr pScrn)
{
  S3VPtr ps3v = S3VPTR(pScrn);

  if(
     ((pScrn->bitsPerPixel == 24) || 
      (pScrn->bitsPerPixel == 16)
      ) 
     &&
     ((ps3v->Chipset == S3_ViRGE_DXGX)  || 
      S3_ViRGE_MX_SERIES(ps3v->Chipset) || 
      S3_ViRGE_GX2_SERIES(ps3v->Chipset)
      ))
    return TRUE;
  else
    return FALSE;
}


void S3VInitVideo(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    XF86VideoAdaptorPtr *adaptors, *newAdaptors = NULL;
    XF86VideoAdaptorPtr newAdaptor = NULL;
    S3VPtr ps3v = S3VPTR(pScrn);
    int num_adaptors;

    if(
       ((pScrn->bitsPerPixel == 24) || 
	(pScrn->bitsPerPixel == 16)
	) 
       &&
       ((ps3v->Chipset == S3_ViRGE_DXGX)  || 
	S3_ViRGE_MX_SERIES(ps3v->Chipset) || 
	S3_ViRGE_GX2_SERIES(ps3v->Chipset) /* || */
	/* (ps3v->Chipset == S3_ViRGE) */
	)
       && !ps3v->NoAccel
       && ps3v->XVideo
       )
    {
#if 0
	if((pMga->Overlay8Plus24 /* || dualhead */ || pMga->TexturedVideo) &&
	   (pScrn->bitsPerPixel != 24))
        {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Using texture video\n");
	    newAdaptor = MGASetupImageVideoTexture(pScreen);
	    pMga->TexturedVideo = TRUE;
	} else {
#endif

	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Using overlay video\n");
	    newAdaptor = S3VSetupImageVideoOverlay(pScreen);

#if 0
	    pMga->TexturedVideo = FALSE;
	}*/

	if(!pMga->Overlay8Plus24 /* && !dualhead */)	  
	  S3VInitOffscreenImages(pScreen);
	pMga->BlockHandler = pScreen->BlockHandler;
	pScreen->BlockHandler = MGABlockHandler;
#endif
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

#define NUM_FORMATS_OVERLAY 4
#define NUM_FORMATS_TEXTURE 4

static XF86VideoFormatRec Formats[NUM_FORMATS_TEXTURE] = 
{
  /*{15, TrueColor},*/ {16, TrueColor}, {24, TrueColor} /* ,
    {15, DirectColor}*/, {16, DirectColor}, {24, DirectColor}
};

#if 0
#define NUM_ATTRIBUTES_OVERLAY 3

static XF86AttributeRec Attributes[NUM_ATTRIBUTES_OVERLAY] =
{
   {XvSettable | XvGettable, 0, (1 << 24) - 1, "XV_COLORKEY"},
   {XvSettable | XvGettable, -128, 127, "XV_BRIGHTNESS"},
   {XvSettable | XvGettable, 0, 255, "XV_CONTRAST"}
};
#endif

#define NUM_IMAGES 3

static XF86ImageRec Images[NUM_IMAGES] =
{
  XVIMAGE_YUY2,
  /* As in mga, YV12 & I420 are converted to YUY2 on the fly by */
  /* copy over conversion. */
  XVIMAGE_YV12,
  XVIMAGE_I420
	/* XVIMAGE_UYVY */
};



static int 
S3VSetPortAttributeOverlay(
  ScrnInfoPtr pScrn, 
  Atom attribute,
  INT32 value, 
  pointer data
){
#if 0
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
#endif

return BadMatch;

#if 0
  return Success;
#endif
}

static int 
S3VGetPortAttributeOverlay(
  ScrnInfoPtr pScrn, 
  Atom attribute,
  INT32 *value, 
  pointer data
){
#if 0
  MGAPtr pMga = MGAPTR(pScrn);
  MGAPortPrivPtr pPriv = pMga->portPrivate;

  if(attribute == xvBrightness) {
	*value = pPriv->brightness;
  } else
  if(attribute == xvContrast) {
	*value = pPriv->contrast;
  } else
  if(attribute == xvColorKey) {
	*value = pPriv->colorKey;
  } else 
#endif

return BadMatch;

#if 0
  return Success;
#endif
}



static void 
S3VQueryBestSize(
  ScrnInfoPtr pScrn, 
  Bool motion,
  short vid_w, short vid_h, 
  short drw_w, short drw_h, 
  unsigned int *p_w, unsigned int *p_h, 
  pointer data
){
  *p_w = drw_w;
  *p_h = drw_h;

#if 0
  /* Only support scaling up, no down scaling. */
  /* This doesn't seem to work (at least for XMovie) */
  /* and the DESIGN doc says this is illegal anyway... */
  if( drw_w < vid_w ) *p_w = vid_w;
  if( drw_h < vid_h ) *p_h = vid_h;
#endif
}



static void 
S3VResetVideoOverlay(ScrnInfoPtr pScrn) 
{
  /* empty for ViRGE at the moment... */
#if 0
  S3VPtr ps3v = S3VPTR(pScrn);
  S3VPortPrivPtr pPriv = ps3v->portPrivate;

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
#endif

#if 0
    OUTREG(MGAREG_BESLUMACTL, ((pPriv->brightness & 0xff) << 16) |
			       (pPriv->contrast & 0xff));
#endif /*0*/
}



static XF86VideoAdaptorPtr
S3VAllocAdaptor(ScrnInfoPtr pScrn)
{
    XF86VideoAdaptorPtr adapt;
    S3VPtr ps3v = S3VPTR(pScrn);
    S3VPortPrivPtr pPriv;
    int i;

    if(!(adapt = xf86XVAllocateVideoAdaptorRec(pScrn)))
	return NULL;

    if(!(pPriv = xcalloc(1, sizeof(S3VPortPrivRec)  + 
			(sizeof(DevUnion) * S3V_MAX_PORTS)))) 
    {
	xfree(adapt);
	return NULL;
    }

    adapt->pPortPrivates = (DevUnion*)(&pPriv[1]);

    for(i = 0; i < S3V_MAX_PORTS; i++)
	adapt->pPortPrivates[i].val = i;

#if 0
    xvBrightness = MAKE_ATOM("XV_BRIGHTNESS");
    xvContrast   = MAKE_ATOM("XV_CONTRAST");
    xvColorKey   = MAKE_ATOM("XV_COLORKEY");
#endif

    pPriv->colorKey = 
      (1 << pScrn->offset.red) | 
      (1 << pScrn->offset.green) |
      (((pScrn->mask.blue >> pScrn->offset.blue) - 1) << pScrn->offset.blue); 

#if 0
    pPriv->brightness = 0;
    pPriv->contrast = 128;
#endif

    pPriv->videoStatus = 0;
    pPriv->lastPort = -1;

    ps3v->adaptor = adapt;
    ps3v->portPrivate = pPriv;

    return adapt;
}





static XF86VideoAdaptorPtr 
S3VSetupImageVideoOverlay(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    S3VPtr ps3v = S3VPTR(pScrn);
    XF86VideoAdaptorPtr adapt;

    adapt = S3VAllocAdaptor(pScrn);

    adapt->type = XvWindowMask | XvInputMask | XvImageMask;
    adapt->flags = VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT;
    adapt->name = "S3 ViRGE Backend Scaler";
    adapt->nEncodings = 1;
    adapt->pEncodings = &DummyEncoding[0];
    adapt->nFormats = NUM_FORMATS_OVERLAY;
    adapt->pFormats = Formats;
    adapt->nPorts = 1;
    adapt->pAttributes = NULL /*Attributes*/;
#if 0
    if (pMga->Chipset == PCI_CHIP_MGAG400) {
	adapt->nImages = 4;
	adapt->nAttributes = 3;
    } else {
#endif
	adapt->nImages = 3;
	adapt->nAttributes = 0;
	/* }*/
    adapt->pImages = Images;
    adapt->PutVideo = NULL;
    adapt->PutStill = NULL;
    adapt->GetVideo = NULL;
    adapt->GetStill = NULL;
    adapt->StopVideo = S3VStopVideo;
    /* Empty Attrib functions - required anyway */
    adapt->SetPortAttribute = S3VSetPortAttributeOverlay;
    adapt->GetPortAttribute = S3VGetPortAttributeOverlay;
    adapt->QueryBestSize = S3VQueryBestSize;
    adapt->PutImage = S3VPutImage;
    adapt->QueryImageAttributes = S3VQueryImageAttributes;

    /* gotta uninit this someplace */
    REGION_NULL(pScreen, &(ps3v->portPrivate->clip));

    S3VResetVideoOverlay(pScrn);

    return adapt;
}


static void 
S3VStopVideo(ScrnInfoPtr pScrn, pointer data, Bool shutdown)
{
  S3VPtr ps3v = S3VPTR(pScrn);
  S3VPortPrivPtr pPriv = ps3v->portPrivate;

#if 0
  MGAPtr pMga = MGAPTR(pScrn);
  MGAPortPrivPtr pPriv = pMga->portPrivate;

  if(pMga->TexturedVideo) return;
#endif

  REGION_EMPTY(pScrn->pScreen, &pPriv->clip);   

  if(shutdown) {
     if(pPriv->videoStatus & CLIENT_VIDEO_ON)
       {
	 if ( S3_ViRGE_GX2_SERIES(ps3v->Chipset) ||
	      S3_ViRGE_MX_SERIES(ps3v->Chipset)
	      )
	   {
	     /*  Aaarg... It .. won't.. go .. away!  */
	     /* So let's be creative, make the overlay really */
	     /* small and near an edge. */
	     /* Size of 0 leaves a window sized vertical stripe */
	     /* Size of 1 leaves a single pixel.. */
	     OUTREG(SSTREAM_WINDOW_SIZE_REG, 1);
	     /* And hide it at 0,0 */
	     OUTREG(SSTREAM_START_REG, 0 );
	   }
	 else
	   {
	     /* Primary over secondary */
	     OUTREG(BLEND_CONTROL_REG, 0x01000000);
	   }
       }

     if(pPriv->area) {
	xf86FreeOffscreenArea(pPriv->area);
	pPriv->area = NULL;
     }
     pPriv->videoStatus = 0;
#if 0
  } else {
     if(pPriv->videoStatus & CLIENT_VIDEO_ON) {
	pPriv->videoStatus |= OFF_TIMER;
	pPriv->offTime = currentTime.milliseconds + OFF_DELAY; 
     }
#endif
  }
}



static FBAreaPtr
S3VAllocateMemory(
   ScrnInfoPtr pScrn,
   FBAreaPtr area,
   int numlines
){
   ScreenPtr pScreen;
   FBAreaPtr new_area;

   if(area) {
	if((area->box.y2 - area->box.y1) >= numlines) 
	   return area;
        
        if(xf86ResizeOffscreenArea(area, pScrn->displayWidth, numlines))
	   return area;

	xf86FreeOffscreenArea(area);
   }

   pScreen = screenInfo.screens[pScrn->scrnIndex];

   new_area = xf86AllocateOffscreenArea(pScreen, pScrn->displayWidth, 
				numlines, 0, NULL, NULL, NULL);

   if(!new_area) {
	int max_w, max_h;

	xf86QueryLargestOffscreenArea(pScreen, &max_w, &max_h, 0,
			FAVOR_WIDTH_THEN_AREA, PRIORITY_EXTREME);
	
	if((max_w < pScrn->displayWidth) || (max_h < numlines))
	   return NULL;

	xf86PurgeUnlockedOffscreenAreas(pScreen);
	new_area = xf86AllocateOffscreenArea(pScreen, pScrn->displayWidth, 
				numlines, 0, NULL, NULL, NULL);
   }

   return new_area;
}



static void
S3VDisplayVideoOverlay(
    ScrnInfoPtr pScrn,
    int id,
    int offset,
    short width, short height,
    int pitch, 
    /* x,y src co-ordinates */
    int x1, int y1, int x2, int y2,
    /* dst in BoxPtr format */
    BoxPtr dstBox,
    /* src width and height */
    short src_w, short src_h,
    /* dst width and height */
    short drw_w, short drw_h
){
    int tmp;

#if 0
    CHECK_DMA_QUIESCENT(pMga, pScrn);
#endif
    S3VPtr ps3v = S3VPTR(pScrn);
    S3VPortPrivPtr pPriv = ps3v->portPrivate;

  vgaHWPtr hwp = VGAHWPTR(pScrn);
  /*  S3VPtr ps3v = S3VPTR(pScrn);*/
  int vgaCRIndex, vgaCRReg, vgaIOBase;
  vgaIOBase = hwp->IOBase;
  vgaCRIndex = vgaIOBase + 4;
  vgaCRReg = vgaIOBase + 5;

   /* If streams aren't enabled, do nothing */
   if(!ps3v->NeedSTREAMS)
     return;

#if 0
    /* got 64 scanlines to do it in */
    tmp = INREG(MGAREG_VCOUNT) + 64;
    if(tmp > pScrn->currentMode->VDisplay)
	tmp -= pScrn->currentMode->VDisplay;
#endif
    
    /* Reference at http://www.webartz.com/fourcc/ */
      /* Looks like ViRGE only supports YUY2 and Y211?, */
      /* listed as YUV-16 (4.2.2) and YUV (2.1.1) in manual. */

#if 0 
      /* Only supporting modes we listed for the time being, */   
      /* No, switching required... #if 0'd this out */  

    switch(id) {
    case FOURCC_UYVY:
      /*
	FOURCC=0x59565955
	bpp=16
	YUV 4:2:2 (Y sample at every
	pixel, U and V sampled at
	every second pixel
	horizontally on each line). A
	macropixel contains 2 pixels
	in 1 u_int32.
      */

      /* OUTREG(MGAREG_BESGLOBCTL, 0x000000c3 | (tmp << 16));*/
	 break;
    case FOURCC_YUY2:
      /*
	FOURCC=0x32595559
	bpp=16
	YUV 4:2:2 as for UYVY but
	with different component
	ordering within the u_int32
	macropixel.

	Supports YV12 & I420 by copy over conversion of formats to YUY2,
	copied from mga driver.  Thanks Mark!
       */
    default:
      /*OUTREG(MGAREG_BESGLOBCTL, 0x00000083 | (tmp << 16));*/
      /* YUV-16 (4.2.2) Secondary stream */
      /* temp ... add DDA Horiz Accum. */
      /*OUTREG(SSTREAM_CONTROL_REG, 0x02000000); / YUV-16 */
      /* works for xvtest and suzi */
      /* OUTREG(SSTREAM_CONTROL_REG, 0x01000000);  * YCbCr-16 * no scaling */

      /* calc horizontal scale factor */
      tmp = drw_w / src_w;
      if (drw_w == src_w) tmp = 0; 
      else if (tmp>=4) tmp =3;
      else if (tmp>=2) tmp =2;
      else tmp =1;

      /* YCbCr-16 */
      OUTREG(SSTREAM_CONTROL_REG, 
	     tmp << 28 | 0x01000000 | 
	     ((((src_w-1)<<1)-(drw_w-1)) & 0xfff)
	     );
      break;
    }
#endif

      /* calc horizontal scale factor */
      if (drw_w == src_w) 
	tmp = 0; 
      else 
	tmp =2;
      /* YCbCr-16 */
    OUTREG(SSTREAM_CONTROL_REG, 
	   tmp << 28 | 0x01000000 |
	   ((((src_w-1)<<1)-(drw_w-1)) & 0xfff)
	   );

    OUTREG(SSTREAM_STRETCH_REG, 
	   ((src_w - 1) & 0x7ff) | (((src_w-drw_w-1) & 0x7ff) << 16)	   
	   );

    /* Color key on primary */
    if ( S3_ViRGE_GX2_SERIES(ps3v->Chipset) ||
	 S3_ViRGE_MX_SERIES(ps3v->Chipset)
	 )
      {
	/* 100% of secondary, no primary */
	/* gx2/mx can both blend while keying, need to */
	/* select secondary here, otherwise all you'll get */
	/* from the primary is the color key.  (And setting */
	/* 0 here gives you black... no primary or secondary. */
	/* Discovered that the hard way!) */
	OUTREG(BLEND_CONTROL_REG, 0x20 );
      }
    else
      {
	OUTREG(BLEND_CONTROL_REG, 0x05000000);
      }

    OUTREG(SSTREAM_FBADDR0_REG, offset & 0x3fffff );
    OUTREG(SSTREAM_STRIDE_REG, pitch & 0xfff );

    OUTREG(K1_VSCALE_REG, src_h-1 );
    OUTREG(K2_VSCALE_REG, (src_h - drw_h) & 0x7ff );

    if ( S3_ViRGE_GX2_SERIES(ps3v->Chipset) || 
	 S3_ViRGE_MX_SERIES(ps3v->Chipset) )
      {
	/* enable vert interp. & bandwidth saving - gx2 */
	OUTREG(DDA_VERT_REG, (((~drw_h)-1) & 0xfff ) |
	       /* bw & vert interp */ 
	       0xc000 
	       /* no bw save 0x8000*/
	       );
      }
    else
      {
	OUTREG(DDA_VERT_REG, (((~drw_h)-1)) & 0xfff );
      }

    OUTREG(SSTREAM_START_REG, ((dstBox->x1 +1) << 16) | (dstBox->y1 +1));
    OUTREG(SSTREAM_WINDOW_SIZE_REG, 
	   ( ((drw_w-1) << 16) | (drw_h ) ) & 0x7ff07ff
	   );

    if ( S3_ViRGE_GX2_SERIES(ps3v->Chipset) ||
	 S3_ViRGE_MX_SERIES(ps3v->Chipset)
	 )
      {
	OUTREG(COL_CHROMA_KEY_CONTROL_REG, 
	       /* color key ON - keying on primary */
	       0x40000000  | 
	       /* # bits to compare */
	       ((pScrn->weight.red-1) << 24) |

	       ((pPriv->colorKey & pScrn->mask.red) >> pScrn->offset.red) << 
	       (16 + 8-pScrn->weight.red) |
	   
	       ((pPriv->colorKey & pScrn->mask.green) >> pScrn->offset.green) <<
	       (8 + 8-pScrn->weight.green) |
	   
	       ((pPriv->colorKey & pScrn->mask.blue) >> pScrn->offset.blue) <<
	       (8-pScrn->weight.blue)
	       );
      } 
    else 
      {
	OUTREG(COL_CHROMA_KEY_CONTROL_REG, 
	       /* color key ON */
	       0x10000000 |
	       /* # bits to compare */
	       ((pScrn->weight.red-1) << 24) |

	       ((pPriv->colorKey & pScrn->mask.red) >> pScrn->offset.red) << 
	       (16 + 8-pScrn->weight.red) |
	   
	       ((pPriv->colorKey & pScrn->mask.green) >> pScrn->offset.green) <<
	       (8 + 8-pScrn->weight.green) |
	   
	       ((pPriv->colorKey & pScrn->mask.blue) >> pScrn->offset.blue) <<
	       (8-pScrn->weight.blue)
	       );
      }

    if ( S3_ViRGE_GX2_SERIES(ps3v->Chipset) ||
	 S3_ViRGE_MX_SERIES(ps3v->Chipset) )
      {
	VGAOUT8(vgaCRIndex, 0x92);
	VGAOUT8(vgaCRReg, (((pitch + 7) / 8) >> 8) | 0x80);
	VGAOUT8(vgaCRIndex, 0x93);
	VGAOUT8(vgaCRReg, (pitch + 7) / 8);
      }

}


static int 
S3VPutImage( 
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
   S3VPtr ps3v = S3VPTR(pScrn);
   S3VPortPrivPtr pPriv = ps3v->portPrivate;
   INT32 x1, x2, y1, y2;
   unsigned char *dst_start;
   int pitch, new_h, offset, offset2=0, offset3=0;
   int srcPitch, srcPitch2=0, dstPitch;
   int top, left, npixels, nlines;
   BoxRec dstBox;
   CARD32 tmp;

   /* If streams aren't enabled, do nothing */
   if(!ps3v->NeedSTREAMS)
     return Success;

   /* Clip */
   x1 = src_x;
   x2 = src_x + src_w;
   y1 = src_y;
   y2 = src_y + src_h;

   dstBox.x1 = drw_x;
   dstBox.x2 = drw_x + drw_w;
   dstBox.y1 = drw_y;
   dstBox.y2 = drw_y + drw_h;

   if(!xf86XVClipVideoHelper(&dstBox, &x1, &x2, &y1, &y2,
			     clipBoxes, width, height))
	return Success;

   /*if(!pMga->TexturedVideo) {*/
	dstBox.x1 -= pScrn->frameX0;
	dstBox.x2 -= pScrn->frameX0;
	dstBox.y1 -= pScrn->frameY0;
	dstBox.y2 -= pScrn->frameY0;
	/*}*/

   pitch = pScrn->bitsPerPixel * pScrn->displayWidth >> 3;

   dstPitch = ((width << 1) + 15) & ~15;
   new_h = ((dstPitch * height) + pitch - 1) / pitch;

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

   if(!(pPriv->area = S3VAllocateMemory(pScrn, pPriv->area, new_h)))
	return BadAlloc;

    /* copy data */
    top = y1 >> 16;
    left = (x1 >> 16) & ~1;
    npixels = ((((x2 + 0xffff) >> 16) + 1) & ~1) - left;
    left <<= 1;

    offset = pPriv->area->box.y1 * pitch;
    dst_start = ps3v->FBStart + offset + left + (top * dstPitch);
    /*dst_start = pMga->FbStart + offset + left + (top * dstPitch);*/

#if 0
    if(pMga->TexturedVideo && pMga->AccelInfoRec->NeedToSync &&
	((long)data != pPriv->lastPort)) 
    {
	MGAStormSync(pScrn);
	pMga->AccelInfoRec->NeedToSync = FALSE;
    }
#endif

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
	xf86XVCopyYUV12ToPacked(buf + (top * srcPitch) + (left >> 1), 
				buf + offset2, buf + offset3, dst_start,
				srcPitch, srcPitch2, dstPitch, nlines, npixels);
	break;
    case FOURCC_UYVY:
    case FOURCC_YUY2:
    default:
	buf += (top * srcPitch) + left;
	nlines = ((y2 + 0xffff) >> 16) - top;
	xf86XVCopyPacked(buf, dst_start, srcPitch, dstPitch, nlines, npixels);
        break;
    }

#if 0
    if(pMga->TexturedVideo) {
	pPriv->lastPort = (long)data;
	MGADisplayVideoTexture(pScrn, id, offset, 
		REGION_NUM_RECTS(clipBoxes), REGION_RECTS(clipBoxes),
		width, height, dstPitch, src_x, src_y, src_w, src_h,
		drw_x, drw_y, drw_w, drw_h);
	pPriv->videoStatus = FREE_TIMER;
	pPriv->freeTime = currentTime.milliseconds + FREE_DELAY;
    } else {
#endif
    /* update cliplist */
	if(!REGION_EQUAL(pScrn->pScreen, &pPriv->clip, clipBoxes)) {
	    REGION_COPY(pScrn->pScreen, &pPriv->clip, clipBoxes);
	    /* draw these */
	    xf86XVFillKeyHelper(pScrn->pScreen, pPriv->colorKey, clipBoxes);
	}

	offset += left + (top * dstPitch);
	S3VDisplayVideoOverlay(pScrn, id, offset, width, height, dstPitch,
	     x1, y1, x2, y2, &dstBox, src_w, src_h, drw_w, drw_h);

	pPriv->videoStatus = CLIENT_VIDEO_ON;
#if 0
    }
    pMga->VideoTimerCallback = MGAVideoTimerCallback;
#endif

    return Success;
}


static int 
S3VQueryImageAttributes(
    ScrnInfoPtr pScrn, 
    int id, 
    unsigned short *w, unsigned short *h, 
    int *pitches, int *offsets
){
#if 0
    MGAPtr pMga = MGAPTR(pScrn);
#endif
    int size, tmp;

#if 0
    if(pMga->TexturedVideo) {
	if(*w > 2046) *w = 2046;
	if(*h > 2046) *h = 2046;
    } else {
#endif
	if(*w > 1024) *w = 1024;
	if(*h > 1024) *h = 1024;
#if 0
    }
#endif

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
