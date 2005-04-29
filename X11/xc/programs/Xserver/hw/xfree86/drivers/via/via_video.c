/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/via/via_video.c,v 1.17 2004/02/04 04:15:09 dawes Exp $ */
/*
 * Copyright 1998-2003 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2003 S3 Graphics, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * VIA, S3 GRAPHICS, AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/*
 * I N C L U D E S
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
#include "via_driver.h"
#include "via_video.h"

#include "ddmpeg.h"
#include "via_capture.h"
#include "via.h"

#include "xf86xv.h"
#include "Xv.h"
#include "xaa.h"
#include "xaalocal.h"
#include "dixstruct.h"
#include "via_xvpriv.h"
#include "via_swov.h"


/*
 * D E F I N E
 */
#define OFF_DELAY       200  /* milliseconds */
#define FREE_DELAY      60000
#define PARAMSIZE       1024
#define SLICESIZE       65536       
#define OFF_TIMER       0x01
#define FREE_TIMER      0x02
#define TIMER_MASK      (OFF_TIMER | FREE_TIMER)

#define LOW_BAND 0x0CB0
#define MID_BAND 0x1f10

#define  XV_IMAGE          0
#define  NTSC_COMPOSITE    1
#define  NTSC_TUNER        2
#define  NTSC_SVIDEO       3
#define  PAL_SVIDEO        4
#define  PAL_60_COMPOSITE  5
#define  PAL_60_TUNER      6
#define  PAL_60_SVIDEO     7
#define MAKE_ATOM(a) MakeAtom(a, sizeof(a) - 1, TRUE)

#define  IN_FLIP     ( viaVidEng->ramtab & 0x00000003)
#define  IN_DISPLAY  ( viaVidEng->interruptflag & 0x00000200)
#define  IN_VBLANK   ( !IN_DISPLAY )

#ifndef XvExtension
void viaInitVideo(ScreenPtr pScreen) {}
#else

/*
 *  F U N C T I O N   D E C L A R A T I O N
 */
XF86VideoAdaptorPtr viaSetupImageVideoG(ScreenPtr);
static void viaStopVideoG(ScrnInfoPtr, pointer, Bool);
static int viaSetPortAttributeG(ScrnInfoPtr, Atom, INT32, pointer);
static int viaGetPortAttributeG(ScrnInfoPtr, Atom ,INT32 *, pointer);
static void viaQueryBestSizeG(ScrnInfoPtr, Bool,
        short, short, short, short, unsigned int *, unsigned int *, pointer);
static int viaPutImageG( ScrnInfoPtr, 
        short, short, short, short, short, short, short, short,
        int, unsigned char*, short, short, Bool, RegionPtr, pointer);
static int viaPutVideo(ScrnInfoPtr ,
    short , short , short , short ,short , short , short , short ,
    RegionPtr , pointer );

static int viaQueryImageAttributesG(ScrnInfoPtr, 
        int, unsigned short *, unsigned short *,  int *, int *);

static Atom xvBrightness, xvContrast, xvColorKey, xvHue, xvSaturation, xvPort,
	    xvCompose, xvEncoding, xvMute, xvVolume, xvFreq, xvAudioCtrl,
	    xvHQV, xvBOB, xvExitTV;

/*
 *  S T R U C T S
 */
/* client libraries expect an encoding */
static XF86VideoEncodingRec DummyEncoding[8] =
{
  { XV_IMAGE        , "XV_IMAGE",-1, -1,{1, 1}},
  { NTSC_COMPOSITE  , "ntsc-composite",720, 480, { 1001, 60000 }},
  { NTSC_TUNER      , "ntsc-tuner",720, 480, { 1001, 60000 }},
  { NTSC_SVIDEO     , "ntsc-svideo",720, 480, { 1001, 60000 }},
  { PAL_SVIDEO      , "pal-svideo",720, 576, { 1, 50 }},
  { PAL_60_COMPOSITE, "pal_60-composite", 704, 576, { 1, 50 }},
  { PAL_60_TUNER    , "pal_60-tuner", 720, 576, { 1, 50 }},
  { PAL_60_SVIDEO   , "pal_60-svideo",720, 576, { 1, 50 }}
};

#define NUM_FORMATS_G 9

static XF86VideoFormatRec FormatsG[NUM_FORMATS_G] = 
{
  { 8, TrueColor }, /* Dithered */
  { 8, PseudoColor }, /* Using .. */
  { 8, StaticColor },
  { 8, GrayScale },
  { 8, StaticGray }, /* .. TexelLUT */
  {16, TrueColor},
  {24, TrueColor},
  {16, DirectColor},
  {24, DirectColor}
};

#define NUM_ATTRIBUTES_G 17

static XF86AttributeRec AttributesG[NUM_ATTRIBUTES_G] =
{
   {XvSettable | XvGettable, 0, (1 << 24) - 1, "XV_COLORKEY"},
   {XvSettable | XvGettable, -1000, 1000, "XV_BRIGHTNESS"},
   {XvSettable | XvGettable, -1000, 1000, "XV_CONTRAST"},
   {XvSettable | XvGettable,-1000,1000,"XV_SATURATION"},
   {XvSettable | XvGettable,-1000,1000,"XV_HUE"},
   {XvSettable | XvGettable,0,255,"XV_MUTE"},
   {XvSettable | XvGettable,0,255,"XV_VOLUME"},
   {XvSettable,0,2,"XV_PORT"},
   {XvSettable,0,2,"XV_COMPOSE"},
   {XvSettable,0,2,"XV_SVIDEO"},
   {XvSettable | XvGettable,0, 255,"XV_ENCODING"},
   {XvSettable | XvGettable,0, 255, "XV_CHANNEL"},
   {XvSettable,0,-1,"XV_FREQ"},
   {XvSettable,0,2,"XV_AUDIOCTRL"},
   {XvSettable,0,2,"XV_HIGHQVDO"},
   {XvSettable,0,2,"XV_BOB"},
   {XvSettable,0,2,"XV_EXITTV"},
};

#define NUM_IMAGES_G 2

static XF86ImageRec ImagesG[NUM_IMAGES_G] =
{
   {
        0x32595559,
        XvYUV,
        LSBFirst,
        {'Y','U','Y','2',
          0x00,0x00,0x00,0x10,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71},
        16,
        XvPacked,
        1,
        0, 0, 0, 0 ,
        8, 8, 8, 
        1, 2, 2,
        1, 2, 2,
        {'Y','U','Y','V',
          0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        XvTopToBottom
    } ,
    {
        0x32315659,
        XvYUV,
        LSBFirst,
        {'Y','V','1','2',
          0x00,0x00,0x00,0x10,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71},
        12,
        XvPlanar,
        3,
        0, 0, 0, 0 ,
        8, 8, 8,
        1, 2, 2,
        1, 2, 2,
        {'Y','V','U',
          0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        XvTopToBottom
   }/*,
   {
        0x59565955,
        XvYUV,
        LSBFirst,
        {'U','Y','V','Y',
          0x00,0x00,0x00,0x10,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71},
        16,
        XvPacked,
        1,
        0, 0, 0, 0 ,
        8, 8, 8,
        1, 2, 2,
        1, 1, 1,
        {'U','Y','V','Y',
          0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        XvTopToBottom
   }
*/
};

static char * XVPORTNAME[5] =
{
   "XV_SWOV",
   "XV_TV0" ,
   "XV_TV1" ,
   "XV_UTCTRL",
   "XV_DUMMY"
};

/*
 *  F U N C T I O N
 */
static __inline void waitVBLANK(vmmtr viaVidEng)
{
   while (IN_DISPLAY);
}

static __inline void waitIfFlip(vmmtr viaVidEng) 
{
  while( IN_FLIP );
}


static __inline void waitDISPLAYBEGIN(vmmtr viaVidEng)
{
    while (IN_VBLANK);
}

/*
 *   Decide if the mode support video overlay. This depends on the bandwidth
 *   of the mode and the type of RAM available.
 */

static Bool DecideOverlaySupport(VIAPtr pVia)
{
    /* Small trick here. We keep the height in 16's of lines and width in 32's 
       to avoid numeric overflow */
    unsigned long bandwidth = (pVia->graphicInfo.dwHeight >> 4) * (pVia->graphicInfo.dwWidth >> 5) *
    				pVia->graphicInfo.dwBPP * pVia->graphicInfo.dwRefreshRate;

    VGAOUT8(0x3D4, 0x3D);
    switch ((VGAIN8(0x3D5) & 0x70) >> 4)
    {
        case 0:			/* No overlay without DDR */
        case SDR100:
        case SDR133:
            return FALSE;
        case DDR100:
            /* Basic limit for DDR100 is about this */
            if(bandwidth > 1800000)
            	return FALSE;
            /* But we have constraints at higher than 800x600 */
            if (pVia->graphicInfo.dwWidth > 800)
            {
            	if(pVia->graphicInfo.dwBPP != 8)
            		return FALSE;
            	if(pVia->graphicInfo.dwHeight > 768)
            		return FALSE;
            	if(pVia->graphicInfo.dwRefreshRate > 60)
            		return FALSE;
            }
            return TRUE;

        case DDR133:
            if(bandwidth > 7901250)
            	return FALSE;
            return TRUE;
    }
    return FALSE;
}


void viaResetVideo(ScrnInfoPtr pScrn) 
{
    VIAPtr  pVia = VIAPTR(pScrn);
    vmmtr   viaVidEng = (vmmtr) pVia->VidMapBase;    

    DBG_DD(ErrorF(" via_video.c : viaResetVideo: \n"));

    waitVBLANK(viaVidEng);

    viaVidEng->compose    = 0;
    viaVidEng->video1_ctl = 0;
    viaVidEng->video3_ctl = 0;

}

void viaSaveVideo(ScrnInfoPtr pScrn)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    vmmtr   viaVidEng = (vmmtr) pVia->VidMapBase;    

    pVia->dwV1 = ((vmmtr)viaVidEng)->video1_ctl;
    pVia->dwV3 = ((vmmtr)viaVidEng)->video3_ctl;
    waitVBLANK(viaVidEng);
    viaVidEng->video1_ctl = 0;
    viaVidEng->video3_ctl = 0;
}

void viaRestoreVideo(ScrnInfoPtr pScrn)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    vmmtr   viaVidEng = (vmmtr) pVia->VidMapBase;    

    waitVBLANK(viaVidEng);
    viaVidEng->video1_ctl = pVia->dwV1 ;
    viaVidEng->video3_ctl = pVia->dwV3 ;
}

void viaExitVideo(ScrnInfoPtr pScrn)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    vmmtr   viaVidEng = (vmmtr) pVia->VidMapBase;    

    DBG_DD(ErrorF(" via_video.c : viaExitVideo : \n"));

    waitVBLANK(viaVidEng);
    viaVidEng->video1_ctl = 0;
    viaVidEng->video3_ctl = 0;
}   

XF86VideoAdaptorPtr adaptPtr[XV_PORT_NUM];

void viaInitVideo(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    VIAPtr  pVia = VIAPTR(pScrn);
    XF86VideoAdaptorPtr *adaptors, *newAdaptors = NULL;
    XF86VideoAdaptorPtr newAdaptor = NULL;
    int num_adaptors;

    DBG_DD(ErrorF(" via_video.c : viaInitVideo : \n"));

    /*
     *	We have no KM400 information..
     */
    if(pVia->Chipset == VIA_CLE266)
    {
        newAdaptor = viaSetupImageVideoG(pScreen);
    }

    num_adaptors = xf86XVListGenericAdaptors(pScrn, &adaptors);

    DBG_DD(ErrorF(" via_video.c : num_adaptors : %d\n",num_adaptors));
    if(newAdaptor) {
        if(!num_adaptors) {
            num_adaptors = 1;
            adaptors = &newAdaptor; /* Now ,useless */
        } else {
            DBG_DD(ErrorF(" via_video.c : viaInitVideo : Warning !!! MDS not supported yet !\n"));
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
        xf86XVScreenInit(pScreen, adaptPtr, XV_PORT_NUM);

    if(newAdaptors)
        xfree(newAdaptors);
}


XF86VideoAdaptorPtr 
viaSetupImageVideoG(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    /* XF86VideoAdaptorPtr adaptPtr[XV_PORT_NUM]; */
    viaPortPrivRec *gviaPortPriv[XV_PORT_NUM];
    DevUnion  *         pdevUnion[XV_PORT_NUM];
    int  i;
    
    DBG_DD(ErrorF(" via_video.c : viaSetupImageVideoG: \n"));

    /* Look for available devices */
    ViaTunerProbe(pScrn);

    xvBrightness      = MAKE_ATOM("XV_BRIGHTNESS");
    xvContrast        = MAKE_ATOM("XV_CONTRAST");
    xvColorKey        = MAKE_ATOM("XV_COLORKEY");
    xvHue             = MAKE_ATOM("XV_HUE");
    xvSaturation      = MAKE_ATOM("XV_SATURATION");
    xvMute            = MAKE_ATOM("XV_MUTE");
    xvVolume          = MAKE_ATOM("XV_VOLUME");
    xvPort            = MAKE_ATOM("XV_PORT");
    xvCompose         = MAKE_ATOM("XV_COMPOSE");
    xvEncoding        = MAKE_ATOM("XV_ENCODING");
    xvFreq            = MAKE_ATOM("XV_FREQ");
    xvAudioCtrl       = MAKE_ATOM("XV_AUDIOCTRL");
    xvHQV             = MAKE_ATOM("XV_HIGHQVDO");
    xvBOB             = MAKE_ATOM("XV_BOB");    
    xvExitTV          = MAKE_ATOM("XV_EXITTV");

    /* AllocatePortPriv();*/
    for ( i = 0; i< XV_PORT_NUM; i ++ ) {
        if(!(adaptPtr[i] = xf86XVAllocateVideoAdaptorRec(pScrn)))
            return NULL;

        gviaPortPriv[i] =  (viaPortPrivPtr)xnfcalloc(1, sizeof(viaPortPrivRec) );
        pdevUnion[i] = (DevUnion  *)xnfcalloc(1, sizeof(DevUnion));

        if(i == 0)		/* Overlay engine */
        {
            adaptPtr[i]->type = XvInputMask | XvWindowMask | XvImageMask | XvVideoMask | XvStillMask;
            adaptPtr[i]->flags = VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT;
        }
        else
        {
            adaptPtr[i]->type = XvInputMask | XvWindowMask | XvVideoMask;
            adaptPtr[i]->flags = VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT;
        }
        adaptPtr[i]->name = XVPORTNAME[i];
        adaptPtr[i]->nEncodings = 8;
        adaptPtr[i]->pEncodings = DummyEncoding;
        adaptPtr[i]->nFormats = sizeof(FormatsG) / sizeof(FormatsG[0]);
        adaptPtr[i]->pFormats = FormatsG;

        /* The adapter can handle 1 port simultaneously */
        adaptPtr[i]->nPorts = 1; 
        adaptPtr[i]->pPortPrivates = pdevUnion[i];
        adaptPtr[i]->pPortPrivates->ptr = (pointer) gviaPortPriv[i];
        if (i == 3) /* Utility port doesn't need attribute */
        {
            adaptPtr[i]->nAttributes = 0;
            adaptPtr[i]->pAttributes = NULL;
        }
        else
        {
            adaptPtr[i]->nAttributes = NUM_ATTRIBUTES_G;
            adaptPtr[i]->pAttributes = AttributesG;
        }
        adaptPtr[i]->nImages = NUM_IMAGES_G;
        adaptPtr[i]->pImages = ImagesG;
        adaptPtr[i]->PutVideo = viaPutVideo;
        adaptPtr[i]->StopVideo = viaStopVideoG;
        adaptPtr[i]->SetPortAttribute = viaSetPortAttributeG;
        adaptPtr[i]->GetPortAttribute = viaGetPortAttributeG;
        adaptPtr[i]->QueryBestSize = viaQueryBestSizeG;
        adaptPtr[i]->PutImage = viaPutImageG;
        adaptPtr[i]->QueryImageAttributes = viaQueryImageAttributesG;

#ifdef COLOR_KEY
        gviaPortPriv[i]->colorKey = 0x0821;
#endif
        gviaPortPriv[i]->brightness = 0;
        gviaPortPriv[i]->saturation = 0;
        gviaPortPriv[i]->contrast = 0;
        gviaPortPriv[i]->hue = 0;
        gviaPortPriv[i]->xv_portnum = i;

        /* gotta uninit this someplace */
#ifdef XFREE86_44
        REGION_NULL(pScreen, &gviaPortPriv[i]->clip);
#else
	REGION_INIT(pScreen, &gviaPortPriv[i]->clip, NullBox, 1);
#endif
    } /* End of for */
    viaResetVideo(pScrn);
    return adaptPtr[0];
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


static int CreateSWOVSurface(ScrnInfoPtr pScrn, viaPortPrivPtr pPriv, int fourcc, short width, short height)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    LPDDSURFACEDESC lpSurfaceDesc = &pPriv->SurfaceDesc;
    unsigned long retCode;

    if (pVia->Video.VideoStatus & SWOV_SURFACE_CREATED)
        return Success;

    lpSurfaceDesc->dwWidth  = (unsigned long)width;
    lpSurfaceDesc->dwHeight = (unsigned long)height;
    lpSurfaceDesc->dwBackBufferCount =1;

    lpSurfaceDesc->dwFourCC = (unsigned long)fourcc;

    if (Success != (retCode = VIAVidCreateSurface(pScrn, lpSurfaceDesc)))
	return retCode;

    pPriv->ddLock.dwFourCC = (unsigned long)fourcc;

    VIAVidLockSurface(pScrn, &pPriv->ddLock);

    pPriv->ddLock.SWDevice.lpSWOverlaySurface[0] = pVia->FBBase + pPriv->ddLock.SWDevice.dwSWPhysicalAddr[0];
    pPriv->ddLock.SWDevice.lpSWOverlaySurface[1] = pVia->FBBase + pPriv->ddLock.SWDevice.dwSWPhysicalAddr[1];

    DBG_DD(ErrorF(" lpSWOverlaySurface[0]: %p\n", pPriv->ddLock.SWDevice.lpSWOverlaySurface[0]));
    DBG_DD(ErrorF(" lpSWOverlaySurface[1]: %p\n", pPriv->ddLock.SWDevice.lpSWOverlaySurface[1]));
    
    pVia->Video.VideoStatus |= SWOV_SURFACE_CREATED|SW_VIDEO_ON;
    pVia->Video.dwAction = ACTION_SET_VIDEOSTATUS;
    return Success;
}


static void DestroySWOVSurface(ScrnInfoPtr pScrn,  viaPortPrivPtr pPriv)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    LPDDSURFACEDESC lpSurfaceDesc = &pPriv->SurfaceDesc;
    DBG_DD(ErrorF(" via_video.c : Destroy SW Overlay Surface, fourcc =0x%08x : \n",
              lpSurfaceDesc->dwFourCC));

    if (pVia->Video.VideoStatus & SWOV_SURFACE_CREATED)
    {
        DBG_DD(ErrorF(" via_video.c : Destroy SW Overlay Surface, VideoStatus =0x%08x : \n",
              pVia->Video.VideoStatus));
    }
    else
    {
        DBG_DD(ErrorF(" via_video.c : No SW Overlay Surface Destroyed, VideoStatus =0x%08x : \n",
              pVia->Video.VideoStatus));
        return;
    }

    VIAVidDestroySurface(pScrn, lpSurfaceDesc);        

    pVia->Video.VideoStatus &= ~SWOV_SURFACE_CREATED;
    pVia->Video.dwAction = ACTION_SET_VIDEOSTATUS;
}


static void  StopSWOVerlay(ScrnInfoPtr pScrn)
{
    DDUPDATEOVERLAY      UpdateOverlay_Video;
    LPDDUPDATEOVERLAY    lpUpdateOverlay = &UpdateOverlay_Video;
    VIAPtr  pVia = VIAPTR(pScrn);

    pVia->Video.VideoStatus &= ~SW_VIDEO_ON;
    pVia->Video.dwAction = ACTION_SET_VIDEOSTATUS;

    lpUpdateOverlay->dwFlags  = DDOVER_HIDE;
    VIAVidUpdateOverlay(pScrn, lpUpdateOverlay);    
}


static void 
viaStopVideoG(ScrnInfoPtr pScrn, pointer data, Bool exit)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    viaPortPrivPtr pPriv = (viaPortPrivPtr)data;

    DBG_DD(ErrorF(" via_video.c : viaStopVideoG: exit=%d\n", exit));

    REGION_EMPTY(pScrn->pScreen, &pPriv->clip);
    if(exit) {
       StopSWOVerlay(pScrn);
       DestroySWOVSurface(pScrn, pPriv);
       pVia->dwFrameNum = 0;    
       pPriv->old_drw_x= 0;
       pPriv->old_drw_y= 0;
       pPriv->old_drw_w= 0;
       pPriv->old_drw_h= 0;
   } else {
       StopSWOVerlay(pScrn);
   }
}

/* App "xawtv" attribute from -1000 to 1000 */
/* But SAA7113H needs 0 to 255              */
#define Attr_Mapping(x)   x = ( (x + 1000) >> 3 )


/****************************************************************************
 *  SetTunerChannel                                                       *
 *  Function: Sets the tuner to a requested channel                         *
 *  Inputs: CARD16 channel - the tuner channel to be set.                   *
 *  Outputs: NONE                                                          *
 ****************************************************************************/
static void SetTunerChannel (ViaTunerPtr pTuner, viaPortPrivPtr pChanPriv, INT32 frequency)
{
    int      control;
    short    divider = 0;

    switch(pChanPriv->dwEncoding)
    {
      case PAL_60_COMPOSITE :
      case PAL_60_TUNER     :
      case PAL_60_SVIDEO    :
           divider = 633 + (short)frequency;
           break;
      case NTSC_COMPOSITE   :
      case NTSC_TUNER       :
      case NTSC_SVIDEO      :
           divider = 733 + (short)frequency;
           break;
      default:
           divider = frequency;
    }

    control = 0x8E00;

    if ( divider <= LOW_BAND )
        control |= 0xA0;
    else if ( divider <= MID_BAND )
        control |= 0x90;
    else
        control |= 0x30;

    DBG_DD(ErrorF(" via_video.c : SetTunerChannel : Divider = 0x%08x, Control= 0x%08x, \n", divider,control));

    if(pTuner)
    	ViaTunerChannel(pTuner, divider, control);

} /* SetTunerChannel ()... */

/* v4l uses range 0 - 65535; Xv uses -1000 - 1000 */
static int
v4l_to_xv(int val) {
    val = val * 2000 / 65536 - 1000;
    if (val < -1000) val = -1000;
    if (val >  1000) val =  1000;
    return val;
}
static int
xv_to_v4l(int val) {
    val = val * 65536 / 2000 + 32768;
    if (val <    -0) val =     0;
    if (val > 65535) val = 65535;
    return val;
}


static int 
viaSetPortAttributeG(
    ScrnInfoPtr pScrn,
    Atom attribute,
    INT32 value,
    pointer data
){
    VIAPtr  pVia = VIAPTR(pScrn);
    vmmtr   viaVidEng = (vmmtr) pVia->VidMapBase;    
    viaPortPrivPtr pPriv = (viaPortPrivPtr)data;
    int attr, avalue;
    ViaTunerPtr pTuner = NULL;
    
    if(pPriv->xv_portnum == COMMAND_FOR_TV0)
    	pTuner = pVia->Tuner[0];
    else if(pPriv->xv_portnum == COMMAND_FOR_TV1)
        pTuner = pVia->Tuner[1];
        ;
    DBG_DD(ErrorF(" via_video.c : viaSetPortAttributeG : \n"));

    pVia->OverlaySupported = DecideOverlaySupport(pVia);


    /* Color Key */
    if(attribute == xvColorKey) {
            DBG_DD(ErrorF("  V4L Disable  xvColorKey = %08x\n",value));

            pPriv->colorKey = value;
            /* All assume color depth is 16 */
            value &= 0x00FFFFFF;
            viaVidEng->color_key = value;
            viaVidEng->snd_color_key = value;
            REGION_EMPTY(pScrn->pScreen, &pPriv->clip);
            DBG_DD(ErrorF("  V4L Disable done  xvColorKey = %08x\n",value));

    /* Color Control */
    } else if (attribute == xvBrightness ||
               attribute == xvContrast   ||
               attribute == xvSaturation ||
               attribute == xvHue) {
        if (attribute == xvBrightness)
        {
            DBG_DD(ErrorF("     xvBrightness = %08d\n",value));
            pPriv->pict.brightness = xv_to_v4l(value);
            if(pTuner)
            	ViaTunerBrightness(pTuner, value);
        }
        if (attribute == xvContrast)
        {
            DBG_DD(ErrorF("     xvContrast = %08d\n",value));
            pPriv->pict.contrast   = xv_to_v4l(value);
            if(pTuner)
            	ViaTunerContrast(pTuner, value);
        }
        if (attribute == xvSaturation)
        {
            DBG_DD(ErrorF("     xvSaturation = %08d\n",value));
            pPriv->pict.colour     = xv_to_v4l(value);
            if(pTuner)
            	ViaTunerSaturation(pTuner, value);
        }
        if (attribute == xvHue)
        {
            DBG_DD(ErrorF("     xvHue = %08d\n",value));
            pPriv->pict.hue        = xv_to_v4l(value);
            if(pTuner)
            	ViaTunerHue(pTuner, value);
        }

    /* Audio control */
    } else if (attribute == xvMute){
            DBG_DD(ErrorF(" via_video.c : viaSetPortAttributeG : xvMute = %08d\n",value));
            if(value)
            	value = 1;
           ViaAudioMute(pVia, value);
    } else if (attribute == xvVolume){
            DBG_DD(ErrorF(" via_video.c : viaSetPortAttributeG : xvVolume = %08d\n",value));
            pPriv->Volume = value;
            attr = ATTR_VOLUME;
            avalue = value;
            /* FIXME */
    /* Tuner control. Channel switch */
    } else if (attribute == xvFreq){
            DBG_DD(ErrorF(" via_video.c : viaSetPortAttributeG : xvFreq = %08x\n",value));
            SetTunerChannel(pTuner, pPriv, value );
    /* Video decoder control. NTSC/PAL, SVIDEO/COMPOSITIVE/TV */
    } else if (attribute == xvEncoding){
            DBG_DD(ErrorF("     xvEncoding = %d. \n",value));
            pPriv->dwEncoding = value;
            if(pTuner)
            	ViaTunerStandard(pTuner, value);
    /* VIA Proprietary Attribute for Video control */
    } else if (attribute == xvPort ){
            DBG_DD(ErrorF(" via_video.c : viaSetPortAttributeG : xvPort=%d\n", value));

    } else if (attribute == xvCompose){
            DBG_DD(ErrorF(" via_video.c : viaSetPortAttributeG : xvCompose=%08x\n",value));

    } else if (attribute == xvHQV){
            DBG_DD(ErrorF(" via_video.c : viaSetPortAttributeG : xvHQV=%08x\n",value));

    } else if (attribute == xvBOB){
            DBG_DD(ErrorF(" via_video.c : viaSetPortAttributeG : xvBOB=%08x\n",value));

    } else if (attribute == xvExitTV){
            DBG_DD(ErrorF(" via_video.c : viaSetPortAttributeG : xvExitTV=%08x\n",value));

    /* VIA Proprietary Attribute for AUDIO control */
    } else if (attribute == xvAudioCtrl ){
            DBG_DD(ErrorF(" via_video.c : viaSetPortAttributeG : xvAudioSwitch=%d\n", value));
            if(pTuner)
            {
            	ViaAudioMode(pVia, value);
            	if(pPriv->xv_portnum == COMMAND_FOR_TV0)
            		ViaAudioSelect(pVia,0);
            	else
            		ViaAudioSelect(pVia, 1);
           }
    }else{
           DBG_DD(ErrorF(" via_video.c : viaSetPortAttributeG : is not supported the attribute"));
           return BadMatch;
    }
    
    /* attr,avalue hardware processing goes here */
    (void)attr;
    (void)avalue;
    
    return Success;
}

static int 
viaGetPortAttributeG(
    ScrnInfoPtr pScrn,
    Atom attribute,
    INT32 *value,
    pointer data
){
    viaPortPrivPtr pPriv = (viaPortPrivPtr)data;

    DBG_DD(ErrorF(" via_video.c : viaGetPortAttributeG : port %d\n",pPriv->xv_portnum));

    *value = 0;


    if (attribute == xvColorKey ) {
           *value =(INT32) pPriv->colorKey;
           DBG_DD(ErrorF(" via_video.c :    ColorKey 0x%x\n",pPriv->colorKey));

    /* Color Control */
    } else if (attribute == xvBrightness ||
               attribute == xvContrast   ||
               attribute == xvSaturation ||
               attribute == xvHue) {
        if (attribute == xvBrightness)
        {
            *value = v4l_to_xv(pPriv->pict.brightness);
            DBG_DD(ErrorF("    xvBrightness = %08d\n", *value));
        }
        if (attribute == xvContrast)
        {
            *value = v4l_to_xv(pPriv->pict.contrast);
            DBG_DD(ErrorF("    xvContrast = %08d\n", *value));
        }
        if (attribute == xvSaturation)
        {
            *value = v4l_to_xv(pPriv->pict.colour);
            DBG_DD(ErrorF("    xvSaturation = %08d\n", *value));
        }
        if (attribute == xvHue)
        {
            *value = v4l_to_xv(pPriv->pict.hue);
            DBG_DD(ErrorF("    xvHue = %08d\n", *value));
        }

     }else {
           /*return BadMatch*/ ;
     }
    return Success;
}

static void 
viaQueryBestSizeG(
    ScrnInfoPtr pScrn,
    Bool motion,
    short vid_w, short vid_h,
    short drw_w, short drw_h,
    unsigned int *p_w, unsigned int *p_h,
    pointer data
){
    DBG_DD(ErrorF(" via_video.c : viaQueryBestSizeG :\n"));
    *p_w = drw_w;
    *p_h = drw_h;

    if(*p_w > 2048 )
           *p_w = 2048;
}

/*
 *  To do SW Flip
 */
static void Flip(VIAPtr pVia, viaPortPrivPtr pPriv, int fourcc, unsigned long DisplayBufferIndex)
{
    switch(fourcc)
    {
        case FOURCC_UYVY:
        case FOURCC_YUY2:
            while ((VIDInD(HQV_CONTROL) & HQV_SW_FLIP) );
            VIDOutD(HQV_SRC_STARTADDR_Y, pPriv->ddLock.SWDevice.dwSWPhysicalAddr[DisplayBufferIndex]);
            VIDOutD(HQV_CONTROL,( VIDInD(HQV_CONTROL)&~HQV_FLIP_ODD) |HQV_SW_FLIP|HQV_FLIP_STATUS);
            break;

        case FOURCC_YV12:
        default:
            while ((VIDInD(HQV_CONTROL) & HQV_SW_FLIP) );
            VIDOutD(HQV_SRC_STARTADDR_Y, pPriv->ddLock.SWDevice.dwSWPhysicalAddr[DisplayBufferIndex]);
            VIDOutD(HQV_SRC_STARTADDR_U, pPriv->ddLock.SWDevice.dwSWCbPhysicalAddr[DisplayBufferIndex]);
            VIDOutD(HQV_SRC_STARTADDR_V, pPriv->ddLock.SWDevice.dwSWCrPhysicalAddr[DisplayBufferIndex]);
            VIDOutD(HQV_CONTROL,( VIDInD(HQV_CONTROL)&~HQV_FLIP_ODD) |HQV_SW_FLIP|HQV_FLIP_STATUS);
            break;
    }
}

static void CopyDataYUV422(
    ScrnInfoPtr pScrn,
    VIAPtr  pVia,
    unsigned char * src,
    unsigned char * dst,
    int srcPitch,
    int dstPitch,
    int h,
    int w )
{
    int count;

    /*  copy YUY2 data to video memory,
     *  do 32 bits alignment.
     */
     
    w <<= 1;		/* Each pixel is YU or YV - 16 bits */
    srcPitch <<= 1;
    
    count = h;
    while(count--) {
        memcpy(dst, src, w);
        src += srcPitch;
        dst += dstPitch;
    }
}


static void
CopyDataYUV420(
   ScrnInfoPtr pScrn,
   VIAPtr  pVia,
   unsigned char *src1,
   unsigned char *src2,
   unsigned char *src3,
   unsigned char *dst1,
   unsigned char *dst2,
   unsigned char *dst3,
   int srcPitch,
   int dstPitch,
   int h,
   int w
){
    int count;

    /* copy Y component to video memory */
    count = h;
    while(count--) {
        memcpy(dst1, src1, w);
        src1 += srcPitch;
        dst1 += dstPitch;
    }

    /* UV component is 1/4 of Y */
    w >>= 1;
    h >>= 1;
    srcPitch >>= 1;
    dstPitch >>= 1;

    /* copy V(Cr) component to video memory */
    count = h;
    while(count--) {
        memcpy(dst2, src2, w);
        src2 += srcPitch;
        dst2 += dstPitch;
    }

    /* copy U(Cb) component to video memory */
    count = h;
    while(count--) {
        memcpy(dst3, src3, w);
        src3 += srcPitch;
        dst3 += dstPitch;
    }
}


static int 
viaPutImageG( 
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
    VIAPtr  pVia = VIAPTR(pScrn);
    viaPortPrivPtr pPriv = (viaPortPrivPtr)data;
    vmmtr   viaVidEng = (vmmtr) pVia->VidMapBase;    
    unsigned long retCode;
/*    int i;
    BoxPtr pbox; */

# ifdef XV_DEBUG
    ErrorF(" via_video.c : viaPutImageG : called\n");
    ErrorF(" via_video.c : FourCC=0x%x width=%d height=%d sync=%d\n",id,width,height,sync);
    ErrorF(" via_video.c : src_x=%d src_y=%d src_w=%d src_h=%d colorkey=0x%x\n",src_x, src_y, src_w, src_h, pPriv->colorKey);
    ErrorF(" via_video.c : drw_x=%d drw_y=%d drw_w=%d drw_h=%d\n",drw_x,drw_y,drw_w,drw_h);
# endif

    switch ( pPriv->xv_portnum )
    {
        case COMMAND_FOR_TV0 :
        case COMMAND_FOR_TV1 :
            DBG_DD(ErrorF(" via_video.c :              : Shall not happen! \n"));
            break;

        case COMMAND_FOR_SWOV    :
        case COMMAND_FOR_DUMMY   :
            {
                DDUPDATEOVERLAY      UpdateOverlay_Video;
                LPDDUPDATEOVERLAY    lpUpdateOverlay = &UpdateOverlay_Video;

                int srcPitch, dstPitch;
                int srcYSize, srcUVSize;
                int dstYSize, dstUVSize;
                unsigned long dwUseExtendedFIFO=0;

                DBG_DD(ErrorF(" via_video.c :              : S/W Overlay! \n"));

                /*  Allocate video memory(CreateSurface),
                 *  add codes to judge if need to re-create surface
                 */
                if ( (pPriv->old_src_w != src_w) || (pPriv->old_src_h != src_h) )
                    DestroySWOVSurface(pScrn, pPriv);

                if (Success != ( retCode = CreateSWOVSurface(pScrn, pPriv, id, width, height) ))
                {
                   DBG_DD(ErrorF("             : Fail to Create SW Video Surface\n"));
		   return retCode;
                }


                /*  Copy image data from system memory to video memory
                 *  TODO: use DRM's DMA feature to accelerate data copy
                 */
                srcPitch = width;
                srcYSize  = width * height;
                srcUVSize = srcYSize >>2;
                dstPitch = pPriv->ddLock.SWDevice.dwPitch;
                dstYSize  = dstPitch * height;
                dstUVSize = dstYSize >>2;

                switch(id)
                {
                    case FOURCC_YV12:
                       CopyDataYUV420(pScrn, pVia, buf , buf + srcYSize, buf + srcYSize + srcUVSize,
                              pPriv->ddLock.SWDevice.lpSWOverlaySurface[pVia->dwFrameNum&1],
                              pPriv->ddLock.SWDevice.lpSWOverlaySurface[pVia->dwFrameNum&1] + dstYSize,
                              pPriv->ddLock.SWDevice.lpSWOverlaySurface[pVia->dwFrameNum&1] + dstYSize + dstUVSize,
                              srcPitch, dstPitch, height, width);
                        break;

                    case FOURCC_UYVY:
                    case FOURCC_YUY2:
                    default:
                        CopyDataYUV422(pScrn, pVia, buf,
                               pPriv->ddLock.SWDevice.lpSWOverlaySurface[pVia->dwFrameNum&1],
                               srcPitch, dstPitch, height, width);
                        break;
                }

                /* If there is bandwidth issue, block the H/W overlay */
                if ((viaVidEng->video3_ctl & 0x00000001) && !pVia->OverlaySupported)
                     return BadAlloc;

                /* 
                 *  fill video overlay parameter
                 */
                lpUpdateOverlay->rSrc.left = src_x;
                lpUpdateOverlay->rSrc.top = src_y;
                lpUpdateOverlay->rSrc.right = src_x + width;
                lpUpdateOverlay->rSrc.bottom = src_y + height;

                lpUpdateOverlay->rDest.left = drw_x;
                lpUpdateOverlay->rDest.top = drw_y;
                lpUpdateOverlay->rDest.right = lpUpdateOverlay->rDest.left + drw_w;
                lpUpdateOverlay->rDest.bottom = drw_y + drw_h;

                lpUpdateOverlay->dwFlags = DDOVER_SHOW | DDOVER_KEYDEST;
                if (pScrn->bitsPerPixel == 8)
                {
                    lpUpdateOverlay->dwColorSpaceLowValue = pPriv->colorKey & 0xff;
                }
                else
                {
                    lpUpdateOverlay->dwColorSpaceLowValue = pPriv->colorKey;
                }
                lpUpdateOverlay->dwFourcc = id;

                /* If use extend FIFO mode */
                if ((pVia->graphicInfo.dwWidth > 1024))
                {
                    dwUseExtendedFIFO = 1;
                }


                DBG_DD(ErrorF("             : Flip\n"));
                Flip(pVia, pPriv, id, pVia->dwFrameNum&1);

                pVia->dwFrameNum ++;
    
                /* If the dest rec. & extendFIFO doesn't change, don't do UpdateOverlay 
                   unless the surface clipping has changed */
                if ( (pPriv->old_drw_x == drw_x) && (pPriv->old_drw_y == drw_y)
                     && (pPriv->old_drw_w == drw_w) && (pPriv->old_drw_h == drw_h)
                     && (pPriv->old_src_w == src_w) && (pPriv->old_src_h == src_h)
                     && (pVia->old_dwUseExtendedFIFO == dwUseExtendedFIFO)
                     && (pVia->Video.VideoStatus & SW_VIDEO_ON) &&
                     RegionsEqual(&pPriv->clip, clipBoxes))
                {
                    return Success;
                }

                pPriv->old_drw_x = drw_x;
                pPriv->old_drw_y = drw_y;
                pPriv->old_drw_w = drw_w;
                pPriv->old_drw_h = drw_h;
                pVia->old_dwUseExtendedFIFO = dwUseExtendedFIFO;
                pVia->Video.VideoStatus |= SW_VIDEO_ON;

                /* add to judge if need to re-create surface */
                pPriv->old_src_w = src_w;
                pPriv->old_src_h = src_h;

                /*  BitBlt: Draw the colorkey rectangle */
                if(!RegionsEqual(&pPriv->clip, clipBoxes)) {
                    REGION_COPY(pScrn->pScreen, &pPriv->clip, clipBoxes);
                    
                    xf86XVFillKeyHelper(pScrn->pScreen, pPriv->colorKey, clipBoxes);
                }

                /*
                 *  Call v4l to do update video overlay
                 */
                if ( -1 == VIAVidUpdateOverlay(pScrn, lpUpdateOverlay))
                {
                    DBG_DD(ErrorF(" via_video.c :              : call v4l updateoverlay fail. \n"));
                }
                else
                {
		    DBG_DD(ErrorF(" via_video.c : PutImageG done OK\n"));
                    return Success;
                }
            }
            break;

        case COMMAND_FOR_UTCTRL :{
            VIAXVUtilityProc(pScrn, buf);
            break;
        }
        default:
            DBG_DD(ErrorF(" via_video.c : XVPort not supported\n"));
            break;
    }
    DBG_DD(ErrorF(" via_video.c : PutImageG done OK\n"));
    return Success;
}


static int 
viaQueryImageAttributesG(
    ScrnInfoPtr pScrn,
    int id,
    unsigned short *w, unsigned short *h,
    int *pitches, int *offsets
){
    int size, tmp;

    DBG_DD(ErrorF(" via_video.c : viaQueryImageAttributesG : FourCC=0x%x, ", id));

    if ( (!w) || (!h) )
       return 0;

    if(*w > 1024) *w = 1024;
    if(*h > 1024) *h = 1024;

    *w = (*w + 1) & ~1;
    if(offsets) 
           offsets[0] = 0;

    switch(id) {
    case 0x32315659:  /*Planar format : YV12 -4:2:0*/
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
 
    case 0x59565955:  /*Packed format : UYVY -4:2:2*/
    case 0x32595559:  /*Packed format : YUY2 -4:2:2*/
    default:
        size = *w << 1;
        if(pitches) 
             pitches[0] = size;
        size *= *h;
        break;
    }

    if ( pitches )
       DBG_DD(ErrorF(" pitches[0]=%d, pitches[1]=%d, pitches[2]=%d, ", pitches[0], pitches[1], pitches[2]));
    if ( offsets )
       DBG_DD(ErrorF(" offsets[0]=%d, offsets[1]=%d, offsets[2]=%d, ", offsets[0], offsets[1], offsets[2]));
    DBG_DD(ErrorF(" width=%d, height=%d \n", *w, *h));

    return size;
}


static int
viaPutVideo(ScrnInfoPtr pScrn,
    short src_x, short src_y, short drw_x, short drw_y,
    short src_w, short src_h, short drw_w, short drw_h,
    RegionPtr clipBoxes, pointer data)
{

    viaPortPrivPtr pPriv=(viaPortPrivPtr)data;

#ifdef XV_DEBUG
    ErrorF(" via_video.c : viaPutVideo : Src %dx%d, %d, %d, %p\n",src_w,src_h,src_x,src_y,clipBoxes);
    ErrorF(" via_video.c : Dst %dx%d, %d, %d \n",drw_w,drw_h,drw_x,drw_y);
    ErrorF(" via_video.c : colorkey : 0x%x \n",pPriv->colorKey);
#endif


    /*  BitBlt: Color fill */
    if(!RegionsEqual(&pPriv->clip, clipBoxes)) {
        REGION_COPY(pScrn->pScreen, &pPriv->clip, clipBoxes);
        XAAFillSolidRects(pScrn,pPriv->colorKey,GXcopy, ~0,
                          REGION_NUM_RECTS(clipBoxes),
                          REGION_RECTS(clipBoxes));
    }

    switch ( pPriv->xv_portnum )
    {
        case COMMAND_FOR_TV0 :
            pPriv->yuv_win.x = drw_x;
            pPriv->yuv_win.y = drw_y;
            pPriv->yuv_win.width = drw_w;
            pPriv->yuv_win.height = drw_h;
            pPriv->yuv_win.chromakey = pPriv->colorKey;
            break;
            
        case COMMAND_FOR_TV1 :
            pPriv->yuv_win.x = drw_x;
            pPriv->yuv_win.y = drw_y;
            pPriv->yuv_win.width = drw_w;
            pPriv->yuv_win.height = drw_h;
            pPriv->yuv_win.chromakey = pPriv->colorKey;
            break;

        case COMMAND_FOR_SWOV    :
        case COMMAND_FOR_DUMMY   :
        case COMMAND_FOR_UTCTRL :
            DBG_DD(ErrorF(" via_video.c : This port doesn't support PutVideo.\n"));
            return XvBadAlloc;
        default:
            DBG_DD(ErrorF(" via_video.c : Error port access.\n"));
            return XvBadAlloc;
    }

    return Success;
}

#endif  /* !XvExtension */
