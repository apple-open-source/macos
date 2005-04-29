/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nv/nv_video.c,v 1.21 2003/11/10 18:22:24 tsi Exp $ */

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

#include "nv_include.h"
#include "nv_dma.h"

#define OFF_DELAY 	500  /* milliseconds */
#define FREE_DELAY 	5000

#define OFF_TIMER 	0x01
#define FREE_TIMER	0x02
#define CLIENT_VIDEO_ON	0x04

#define TIMER_MASK      (OFF_TIMER | FREE_TIMER)

#define NUM_BLIT_PORTS 32

typedef struct _NVPortPrivRec {
   short        brightness;
   short        contrast;
   short        saturation;
   short        hue;
   RegionRec    clip;
   CARD32       colorKey;
   Bool         autopaintColorKey;
   Bool		doubleBuffer;
   CARD32       videoStatus;
   int		currentBuffer;
   Time         videoTime;
   Bool		grabbedByV4L;
   Bool         iturbt_709;
   Bool         blitter;
   FBLinearPtr  linear;
   int pitch;
   int offset;
} NVPortPrivRec, *NVPortPrivPtr;


static XF86VideoAdaptorPtr NVSetupOverlayVideo(ScreenPtr);
static XF86VideoAdaptorPtr NVSetupBlitVideo(ScreenPtr);

static void NVStopOverlay (ScrnInfoPtr);
static void NVPutOverlayImage(ScrnInfoPtr pScrnInfo,
                              int offset,
			      int id,
			      int dstPitch,
                              BoxPtr dstBox,
			      int x1, int y1, int x2, int y2,
                              short width, short height,
                              short src_w, short src_h,
                              short dst_w, short dst_h,
                              RegionPtr cliplist);

static int  NVSetOverlayPortAttribute(ScrnInfoPtr, Atom, INT32, pointer);
static int  NVGetOverlayPortAttribute(ScrnInfoPtr, Atom ,INT32 *, pointer);
static int  NVSetBlitPortAttribute(ScrnInfoPtr, Atom, INT32, pointer);
static int  NVGetBlitPortAttribute(ScrnInfoPtr, Atom ,INT32 *, pointer);


static void NVStopOverlayVideo(ScrnInfoPtr, pointer, Bool);
static void NVStopBlitVideo(ScrnInfoPtr, pointer, Bool);

static int  NVPutImage( ScrnInfoPtr, short, short, short, short, short, short, short, short, int, unsigned char*, short, short, Bool, RegionPtr, pointer);
static void NVQueryBestSize(ScrnInfoPtr, Bool, short, short, short, short, unsigned int *, unsigned int *, pointer);
static int  NVQueryImageAttributes(ScrnInfoPtr, int, unsigned short *, unsigned short *,  int *, int *);

static void NVVideoTimerCallback(ScrnInfoPtr, Time);

static void NVInitOffscreenImages (ScreenPtr pScreen);


#define GET_OVERLAY_PRIVATE(pNv) \
   (NVPortPrivPtr)((pNv)->overlayAdaptor->pPortPrivates[0].ptr)

#define GET_BLIT_PRIVATE(pNv) \
   (NVPortPrivPtr)((pNv)->blitAdaptor->pPortPrivates[0].ptr)

#define MAKE_ATOM(a) MakeAtom(a, sizeof(a) - 1, TRUE)

static Atom xvBrightness, xvContrast, xvColorKey, xvSaturation, 
            xvHue, xvAutopaintColorKey, xvSetDefaults, xvDoubleBuffer,
            xvITURBT709;

/* client libraries expect an encoding */
static XF86VideoEncodingRec DummyEncoding =
{ 
   0,
   "XV_IMAGE",
   2046, 2046,
   {1, 1}
};

#define NUM_FORMATS_ALL 6

XF86VideoFormatRec NVFormats[NUM_FORMATS_ALL] = 
{
   {15, TrueColor}, {16, TrueColor}, {24, TrueColor},
   {15, DirectColor}, {16, DirectColor}, {24, DirectColor}
};

#define NUM_ATTRIBUTES 9

XF86AttributeRec NVAttributes[NUM_ATTRIBUTES] =
{
   {XvSettable | XvGettable, 0, 1, "XV_DOUBLE_BUFFER"},
   {XvSettable | XvGettable, 0, (1 << 24) - 1, "XV_COLORKEY"},
   {XvSettable | XvGettable, 0, 1, "XV_AUTOPAINT_COLORKEY"},
   {XvSettable             , 0, 0, "XV_SET_DEFAULTS"},
   {XvSettable | XvGettable, -512, 511, "XV_BRIGHTNESS"},
   {XvSettable | XvGettable, 0, 8191, "XV_CONTRAST"},
   {XvSettable | XvGettable, 0, 8191, "XV_SATURATION"},
   {XvSettable | XvGettable, 0, 360, "XV_HUE"},
   {XvSettable | XvGettable, 0, 1, "XV_ITURBT_709"}
};

#define NUM_IMAGES_YUV 4
#define NUM_IMAGES_ALL 5

#define FOURCC_RGB 0x0000003
#define XVIMAGE_RGB \
   { \
        FOURCC_RGB, \
        XvRGB, \
        LSBFirst, \
        { 0x03, 0x00, 0x00, 0x00, \
          0x00,0x00,0x00,0x10,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71}, \
        32, \
        XvPacked, \
        1, \
        24, 0x00ff0000, 0x0000ff00, 0x000000ff, \
        0, 0, 0, \
        0, 0, 0, \
        0, 0, 0, \
        {'B','G','R','X',\
          0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, \
        XvTopToBottom \
   }

static XF86ImageRec NVImages[NUM_IMAGES_ALL] =
{
    XVIMAGE_YUY2,
    XVIMAGE_YV12,
    XVIMAGE_UYVY,
    XVIMAGE_I420,
    XVIMAGE_RGB
};

static void 
NVSetPortDefaults (ScrnInfoPtr pScrnInfo, NVPortPrivPtr pPriv)
{
    NVPtr pNv = NVPTR(pScrnInfo);

    pPriv->brightness           = 0;
    pPriv->contrast             = 4096;
    pPriv->saturation           = 4096;
    pPriv->hue                  = 0;
    pPriv->colorKey             = pNv->videoKey;
    pPriv->autopaintColorKey    = TRUE;
    pPriv->doubleBuffer		= TRUE;
    pPriv->iturbt_709           = FALSE;
}


void 
NVResetVideo (ScrnInfoPtr pScrnInfo)
{
    NVPtr          pNv     = NVPTR(pScrnInfo);
    NVPortPrivPtr  pPriv   = GET_OVERLAY_PRIVATE(pNv);
    int            satSine, satCosine;
    double         angle;
    
    angle = (double)pPriv->hue * 3.1415927 / 180.0;
    
    satSine = pPriv->saturation * sin(angle);
    if (satSine < -1024)
        satSine = -1024;
    satCosine = pPriv->saturation * cos(angle);
    if (satCosine < -1024)
        satCosine = -1024;
    
    pNv->PMC[0x8910/4] = (pPriv->brightness << 16) | pPriv->contrast;
    pNv->PMC[0x8914/4] = (pPriv->brightness << 16) | pPriv->contrast;
    pNv->PMC[0x8918/4] = (satSine << 16) | (satCosine & 0xffff);
    pNv->PMC[0x891C/4] = (satSine << 16) | (satCosine & 0xffff);
    pNv->PMC[0x8b00/4] = pPriv->colorKey;
}



static void 
NVStopOverlay (ScrnInfoPtr pScrnInfo)
{
    NVPtr          pNv     = NVPTR(pScrnInfo);

    pNv->PMC[0x00008704/4] = 1;
}

static FBLinearPtr
NVAllocateOverlayMemory(
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
        
        if(max_size < size)
           return NULL;

        xf86PurgeUnlockedOffscreenAreas(pScreen);
        new_linear = xf86AllocateOffscreenLinear(pScreen, size, 32, 
                                                NULL, NULL, NULL);
   }

   return new_linear;
}

static void NVFreeOverlayMemory(ScrnInfoPtr pScrnInfo)
{
    NVPtr               pNv   = NVPTR(pScrnInfo);
    NVPortPrivPtr  pPriv   = GET_OVERLAY_PRIVATE(pNv);

    if(pPriv->linear) {
        xf86FreeOffscreenLinear(pPriv->linear);
	pPriv->linear = NULL;
    }
}


static void NVFreeBlitMemory(ScrnInfoPtr pScrnInfo)
{
    NVPtr               pNv   = NVPTR(pScrnInfo);
    NVPortPrivPtr  pPriv   = GET_BLIT_PRIVATE(pNv);

    if(pPriv->linear) {
        xf86FreeOffscreenLinear(pPriv->linear);
        pPriv->linear = NULL;
    }
}


void NVInitVideo (ScreenPtr pScreen)
{
    ScrnInfoPtr 	pScrn = xf86Screens[pScreen->myNum];
    XF86VideoAdaptorPtr *adaptors, *newAdaptors = NULL;
    XF86VideoAdaptorPtr overlayAdaptor = NULL;
    XF86VideoAdaptorPtr blitAdaptor = NULL;
    NVPtr         	pNv   = NVPTR(pScrn);
    int 		num_adaptors;

    if((pScrn->bitsPerPixel != 8) && (pNv->Architecture >= NV_ARCH_10)) {
	overlayAdaptor = NVSetupOverlayVideo(pScreen);
  
	if(overlayAdaptor)
	    NVInitOffscreenImages(pScreen);
    }

    if((pScrn->bitsPerPixel != 8) && !pNv->NoAccel)
        blitAdaptor = NVSetupBlitVideo(pScreen);

    num_adaptors = xf86XVListGenericAdaptors(pScrn, &adaptors);
    
    if(blitAdaptor || overlayAdaptor) {
        int size = num_adaptors;

        if(overlayAdaptor) size++;
        if(blitAdaptor)    size++;

        if((newAdaptors = xalloc(size * sizeof(XF86VideoAdaptorPtr*)))) {
            if(num_adaptors) {
                 memcpy(newAdaptors, adaptors,
                        num_adaptors * sizeof(XF86VideoAdaptorPtr));
            }
            if(overlayAdaptor) {
                newAdaptors[num_adaptors] = overlayAdaptor;
                num_adaptors++;
	    }
            if(blitAdaptor) {
                newAdaptors[num_adaptors] = blitAdaptor;
                num_adaptors++;
            }
	    adaptors = newAdaptors;
        }
    }

    if (num_adaptors)
        xf86XVScreenInit(pScreen, adaptors, num_adaptors);

    if (newAdaptors)
	xfree(newAdaptors);    
}


static XF86VideoAdaptorPtr
NVSetupBlitVideo (ScreenPtr pScreen)
{
    ScrnInfoPtr pScrnInfo = xf86Screens[pScreen->myNum];
    NVPtr       pNv       = NVPTR(pScrnInfo);
    XF86VideoAdaptorPtr adapt;
    NVPortPrivPtr       pPriv;
    int         i;

    if (!(adapt = xcalloc(1, sizeof(XF86VideoAdaptorRec) +
                             sizeof(NVPortPrivRec) +
                             (sizeof(DevUnion) * NUM_BLIT_PORTS))))
    {
        return NULL;
    }

    adapt->type                 = XvWindowMask | XvInputMask | XvImageMask;
    adapt->flags                = 0;
    adapt->name                 = "NV Video Blitter";
    adapt->nEncodings           = 1;
    adapt->pEncodings           = &DummyEncoding;
    adapt->nFormats             = NUM_FORMATS_ALL;
    adapt->pFormats             = NVFormats;
    adapt->nPorts               = NUM_BLIT_PORTS;
    adapt->pPortPrivates        = (DevUnion*)(&adapt[1]);

    pPriv = (NVPortPrivPtr)(&adapt->pPortPrivates[NUM_BLIT_PORTS]);
    for(i = 0; i < NUM_BLIT_PORTS; i++)
       adapt->pPortPrivates[i].ptr = (pointer)(pPriv);

    adapt->pAttributes          = NULL;
    adapt->nAttributes          = 0;
    adapt->pImages              = NVImages;
    adapt->nImages              = NUM_IMAGES_ALL;
    adapt->PutVideo             = NULL;
    adapt->PutStill             = NULL;
    adapt->GetVideo             = NULL;
    adapt->GetStill             = NULL;
    adapt->StopVideo            = NVStopBlitVideo;
    adapt->SetPortAttribute     = NVSetBlitPortAttribute;
    adapt->GetPortAttribute     = NVGetBlitPortAttribute;
    adapt->QueryBestSize        = NVQueryBestSize;
    adapt->PutImage             = NVPutImage;
    adapt->QueryImageAttributes = NVQueryImageAttributes;

    pPriv->videoStatus          = 0;
    pPriv->grabbedByV4L         = FALSE;
    pPriv->blitter              = TRUE;
    pPriv->doubleBuffer         = FALSE;

    pNv->blitAdaptor            = adapt;

    return adapt;
}

static XF86VideoAdaptorPtr 
NVSetupOverlayVideo (ScreenPtr pScreen)
{
    ScrnInfoPtr pScrnInfo = xf86Screens[pScreen->myNum];
    NVPtr       pNv       = NVPTR(pScrnInfo);
    XF86VideoAdaptorPtr adapt;
    NVPortPrivPtr       pPriv;
    
    if (!(adapt = xcalloc(1, sizeof(XF86VideoAdaptorRec) + 
                             sizeof(NVPortPrivRec) + 
                             sizeof(DevUnion))))
    {
        return NULL;
    } 

    adapt->type                 = XvWindowMask | XvInputMask | XvImageMask;
    adapt->flags                = VIDEO_OVERLAID_IMAGES|VIDEO_CLIP_TO_VIEWPORT;
    adapt->name                 = "NV Video Overlay";
    adapt->nEncodings           = 1;
    adapt->pEncodings           = &DummyEncoding;
    adapt->nFormats             = NUM_FORMATS_ALL;
    adapt->pFormats             = NVFormats;
    adapt->nPorts               = 1;
    adapt->pPortPrivates        = (DevUnion*)(&adapt[1]);
    pPriv                       = (NVPortPrivPtr)(&adapt->pPortPrivates[1]);
    adapt->pPortPrivates[0].ptr = (pointer)(pPriv);
    adapt->pAttributes          = NVAttributes;
    adapt->nAttributes          = NUM_ATTRIBUTES;
    adapt->pImages              = NVImages;
    adapt->nImages              = NUM_IMAGES_YUV;
    adapt->PutVideo             = NULL;
    adapt->PutStill             = NULL;
    adapt->GetVideo             = NULL;
    adapt->GetStill             = NULL;
    adapt->StopVideo            = NVStopOverlayVideo;
    adapt->SetPortAttribute     = NVSetOverlayPortAttribute;
    adapt->GetPortAttribute     = NVGetOverlayPortAttribute;
    adapt->QueryBestSize        = NVQueryBestSize;
    adapt->PutImage             = NVPutImage;
    adapt->QueryImageAttributes = NVQueryImageAttributes;
    
    pPriv->videoStatus		= 0;
    pPriv->currentBuffer	= 0;
    pPriv->grabbedByV4L		= FALSE;
    pPriv->blitter              = FALSE;

    NVSetPortDefaults (pScrnInfo, pPriv);
    
    /* gotta uninit this someplace */
    REGION_NULL(pScreen, &pPriv->clip);
    
    pNv->overlayAdaptor		= adapt;
    
    xvBrightness        = MAKE_ATOM("XV_BRIGHTNESS");
    xvDoubleBuffer      = MAKE_ATOM("XV_DOUBLE_BUFFER");
    xvContrast          = MAKE_ATOM("XV_CONTRAST");
    xvColorKey          = MAKE_ATOM("XV_COLORKEY");
    xvSaturation        = MAKE_ATOM("XV_SATURATION");
    xvHue               = MAKE_ATOM("XV_HUE");
    xvAutopaintColorKey = MAKE_ATOM("XV_AUTOPAINT_COLORKEY");
    xvSetDefaults       = MAKE_ATOM("XV_SET_DEFAULTS");
    xvITURBT709         = MAKE_ATOM("XV_ITURBT_709");

    NVResetVideo(pScrnInfo);

    return adapt;
}

static void
NVPutOverlayImage (
    ScrnInfoPtr pScrnInfo,
    int         offset,
    int         id,
    int         dstPitch,
    BoxPtr      dstBox,
    int         x1,
    int         y1,
    int		x2,
    int		y2,
    short       width,
    short       height,
    short       src_w,
    short       src_h,
    short       drw_w,
    short       drw_h,
    RegionPtr   clipBoxes
)
{
    NVPtr          pNv     = NVPTR(pScrnInfo);
    NVPortPrivPtr  pPriv   = GET_OVERLAY_PRIVATE(pNv);
    int buffer = pPriv->currentBuffer;

    /* paint the color key */
    if(pPriv->autopaintColorKey && 
       (pPriv->grabbedByV4L ||
	!REGION_EQUAL(pScrnInfo->pScreen, &pPriv->clip, clipBoxes)))
    {
	/* we always paint V4L's color key */
	if(!pPriv->grabbedByV4L)
           REGION_COPY(pScrnInfo->pScreen, &pPriv->clip, clipBoxes);
        xf86XVFillKeyHelper(pScrnInfo->pScreen, pPriv->colorKey, clipBoxes);
    }

    if(pNv->CurrentLayout.mode->Flags & V_DBLSCAN) {
        dstBox->y1 <<= 1;
        dstBox->y2 <<= 1;
        drw_h <<= 1;
    }

    pNv->PMC[(0x8900/4) + buffer] = offset;
    pNv->PMC[(0x8928/4) + buffer] = (height << 16) | width;
    pNv->PMC[(0x8930/4) + buffer] = ((y1 << 4) & 0xffff0000) | (x1 >> 12);
    pNv->PMC[(0x8938/4) + buffer] = (src_w << 20) / drw_w;
    pNv->PMC[(0x8940/4) + buffer] = (src_h << 20) / drw_h;
    pNv->PMC[(0x8948/4) + buffer] = (dstBox->y1 << 16) | dstBox->x1;
    pNv->PMC[(0x8950/4) + buffer] = ((dstBox->y2 - dstBox->y1) << 16) |
                               	       (dstBox->x2 - dstBox->x1);

    dstPitch |= 1 << 20;   /* use color key */

    if(id != FOURCC_UYVY)
	dstPitch |= 1 << 16;
    if(pPriv->iturbt_709)
        dstPitch |= 1 << 24;

    pNv->PMC[(0x8958/4) + buffer] = dstPitch;
    pNv->PMC[0x00008704/4] = 0;
    pNv->PMC[0x8700/4] = 1 << (buffer << 2);

    pPriv->videoStatus = CLIENT_VIDEO_ON;
}



static void
NVPutBlitImage (
    ScrnInfoPtr pScrnInfo,
    int         offset,
    int         id,
    int         dstPitch,
    BoxPtr      dstBox,
    int         x1,
    int         y1,
    int         x2,
    int         y2,
    short       width,
    short       height,
    short       src_w,
    short       src_h,
    short       drw_w,
    short       drw_h,
    RegionPtr   clipBoxes
)
{
    NVPtr          pNv     = NVPTR(pScrnInfo);
    NVPortPrivPtr  pPriv   = GET_BLIT_PRIVATE(pNv);
    BoxPtr         pbox    = REGION_RECTS(clipBoxes);
    int            nbox    = REGION_NUM_RECTS(clipBoxes);
    CARD32         dsdx, dtdy, size, point, srcpoint, format;

    dsdx = (src_w << 20) / drw_w;
    dtdy = (src_h << 20) / drw_h;

    size = ((dstBox->y2 - dstBox->y1) << 16) | (dstBox->x2 - dstBox->x1);
    point = (dstBox->y1 << 16) | dstBox->x1;

    dstPitch |= (STRETCH_BLIT_SRC_FORMAT_ORIGIN_CENTER << 16) |
                (STRETCH_BLIT_SRC_FORMAT_FILTER_BILINEAR << 24);

    srcpoint = ((y1 << 4) & 0xffff0000) | (x1 >> 12);

    switch(id) {
    case FOURCC_RGB:
        format = STRETCH_BLIT_FORMAT_X8R8G8B8;
        break;
    case FOURCC_UYVY:
        format = STRETCH_BLIT_FORMAT_UYVY;
        break;
    default:
        format = STRETCH_BLIT_FORMAT_YUYV;
        break;
    }

    if(pNv->CurrentLayout.depth == 15) {
        NVDmaStart(pNv, SURFACE_FORMAT, 1);
        NVDmaNext (pNv, SURFACE_FORMAT_DEPTH15);
    }

    NVDmaStart(pNv, STRETCH_BLIT_FORMAT, 1);
    NVDmaNext (pNv, format);

    while(nbox--) {
       NVDmaStart(pNv, RECT_SOLID_COLOR, 1);
       NVDmaNext (pNv, 0);

       NVDmaStart(pNv, STRETCH_BLIT_CLIP_POINT, 6);
       NVDmaNext (pNv, (pbox->y1 << 16) | pbox->x1); 
       NVDmaNext (pNv, ((pbox->y2 - pbox->y1) << 16) | (pbox->x2 - pbox->x1));
       NVDmaNext (pNv, point);
       NVDmaNext (pNv, size);
       NVDmaNext (pNv, dsdx);
       NVDmaNext (pNv, dtdy);

       NVDmaStart(pNv, STRETCH_BLIT_SRC_SIZE, 4);
       NVDmaNext (pNv, (height << 16) | width);
       NVDmaNext (pNv, dstPitch);
       NVDmaNext (pNv, offset);
       NVDmaNext (pNv, srcpoint);
       pbox++;
    }

    if(pNv->CurrentLayout.depth == 15) {
        NVDmaStart(pNv, SURFACE_FORMAT, 1);
        NVDmaNext (pNv, SURFACE_FORMAT_DEPTH16);
    }

    NVDmaKickoff(pNv);
    SET_SYNC_FLAG(pNv->AccelInfoRec);

    pPriv->videoStatus = FREE_TIMER;
    pPriv->videoTime = currentTime.milliseconds + FREE_DELAY;
    pNv->VideoTimerCallback = NVVideoTimerCallback;
}

/*
 * StopVideo
 */
static void NVStopOverlayVideo
(
    ScrnInfoPtr pScrnInfo,
    pointer     data,
    Bool        Exit
)
{
    NVPtr pNv = NVPTR(pScrnInfo);
    NVPortPrivPtr pPriv = (NVPortPrivPtr)data;

    if(pPriv->grabbedByV4L) return;
    
    REGION_EMPTY(pScrnInfo->pScreen, &pPriv->clip);   

    if(Exit) {
	if(pPriv->videoStatus & CLIENT_VIDEO_ON) 
            NVStopOverlay(pScrnInfo);
	NVFreeOverlayMemory(pScrnInfo);
	pPriv->videoStatus = 0;
    } else { 
	if(pPriv->videoStatus & CLIENT_VIDEO_ON) {
	    pPriv->videoStatus = OFF_TIMER | CLIENT_VIDEO_ON;
	    pPriv->videoTime = currentTime.milliseconds + OFF_DELAY; 
	    pNv->VideoTimerCallback = NVVideoTimerCallback;
	}
    }
}

static void NVStopBlitVideo
(
    ScrnInfoPtr pScrnInfo,
    pointer     data,
    Bool        Exit
)
{
}

static int NVSetOverlayPortAttribute
(
    ScrnInfoPtr pScrnInfo, 
    Atom        attribute,
    INT32       value, 
    pointer     data
)
{
    NVPortPrivPtr pPriv = (NVPortPrivPtr)data;
    
    if (attribute == xvBrightness)
    {
        if ((value < -512) || (value > 512))
            return BadValue;
        pPriv->brightness = value;
    }
    else if (attribute == xvDoubleBuffer)
    {
        if ((value < 0) || (value > 1))
            return BadValue;
        pPriv->doubleBuffer = value;
    }
    else if (attribute == xvContrast)
    {
        if ((value < 0) || (value > 8191))
            return BadValue;
        pPriv->contrast = value;
    }
    else if (attribute == xvHue)
    {
        value %= 360;
        if (value < 0)
            value += 360;
        pPriv->hue = value;
    }
    else if (attribute == xvSaturation)
    {
        if ((value < 0) || (value > 8191))
            return BadValue;
        pPriv->saturation = value;
    }
    else if (attribute == xvColorKey)
    {
        pPriv->colorKey = value;
        REGION_EMPTY(pScrnInfo->pScreen, &pPriv->clip);   
    }
    else if (attribute == xvAutopaintColorKey)
    {
        if ((value < 0) || (value > 1))
            return BadValue;
        pPriv->autopaintColorKey = value;
    }
    else if (attribute == xvITURBT709)
    {
        if ((value < 0) || (value > 1))
            return BadValue;
        pPriv->iturbt_709 = value;
    }
    else if (attribute == xvSetDefaults)
    {
        NVSetPortDefaults(pScrnInfo, pPriv);
    }
    else
        return BadMatch;
    
    NVResetVideo(pScrnInfo);
    return Success;
}


static int NVGetOverlayPortAttribute
(
    ScrnInfoPtr  pScrnInfo, 
    Atom         attribute,
    INT32       *value, 
    pointer      data
)
{
    NVPortPrivPtr pPriv = (NVPortPrivPtr)data;
    
    if (attribute == xvBrightness)
        *value = pPriv->brightness;
    else if (attribute == xvDoubleBuffer)
        *value = (pPriv->doubleBuffer) ? 1 : 0;
    else if (attribute == xvContrast)
        *value = pPriv->contrast;
    else if (attribute == xvSaturation)
        *value = pPriv->saturation;
    else if (attribute == xvHue)
        *value = pPriv->hue;
    else if (attribute == xvColorKey)
        *value = pPriv->colorKey;
    else if (attribute == xvAutopaintColorKey)
        *value = (pPriv->autopaintColorKey) ? 1 : 0;
    else if (attribute == xvITURBT709)
        *value = (pPriv->iturbt_709) ? 1 : 0;
    else
        return BadMatch;
    
    return Success;
}

static int NVSetBlitPortAttribute
(
    ScrnInfoPtr pScrnInfo,
    Atom        attribute,
    INT32       value,
    pointer     data
)
{
    return BadMatch;
}

static int NVGetBlitPortAttribute
(
    ScrnInfoPtr  pScrnInfo,
    Atom         attribute,
    INT32       *value,
    pointer      data
)
{
    return BadMatch;
}


/*
 * QueryBestSize
 */
static void NVQueryBestSize
(
    ScrnInfoPtr   pScrnInfo, 
    Bool          motion,
    short         vid_w,
    short         vid_h, 
    short         drw_w,
    short         drw_h, 
    unsigned int *p_w,
    unsigned int *p_h, 
    pointer       data
)
{
    if(vid_w > (drw_w << 3))
	drw_w = vid_w >> 3;
    if(vid_h > (drw_h << 3))
	drw_h = vid_h >> 3;

    *p_w = drw_w;
    *p_h = drw_h; 
}

static void NVCopyData420
(
    unsigned char *src1,
    unsigned char *src2,
    unsigned char *src3,
    unsigned char *dst1,
    int            srcPitch,
    int            srcPitch2,
    int            dstPitch,
    int            h,
    int            w
)
{
   CARD32 *dst;
   CARD8 *s1, *s2, *s3;
   int i, j;

   w >>= 1;

   for(j = 0; j < h; j++) {
        dst = (CARD32*)dst1;
        s1 = src1;  s2 = src2;  s3 = src3;
        i = w;
        while(i > 4) {
#if X_BYTE_ORDER == X_BIG_ENDIAN
           dst[0] = (s1[0] << 24) | (s1[1] << 8) | (s3[0] << 16) | s2[0];
           dst[1] = (s1[2] << 24) | (s1[3] << 8) | (s3[1] << 16) | s2[1];
           dst[2] = (s1[4] << 24) | (s1[5] << 8) | (s3[2] << 16) | s2[2];
           dst[3] = (s1[6] << 24) | (s1[7] << 8) | (s3[3] << 16) | s2[3];
#else
           dst[0] = s1[0] | (s1[1] << 16) | (s3[0] << 8) | (s2[0] << 24);
           dst[1] = s1[2] | (s1[3] << 16) | (s3[1] << 8) | (s2[1] << 24);
           dst[2] = s1[4] | (s1[5] << 16) | (s3[2] << 8) | (s2[2] << 24);
           dst[3] = s1[6] | (s1[7] << 16) | (s3[3] << 8) | (s2[3] << 24);
#endif
           dst += 4; s2 += 4; s3 += 4; s1 += 8;
           i -= 4;
        }

        while(i--) {
#if X_BYTE_ORDER == X_BIG_ENDIAN
           dst[0] = (s1[0] << 24) | (s1[1] << 8) | (s3[0] << 16) | s2[0];
#else
           dst[0] = s1[0] | (s1[1] << 16) | (s3[0] << 8) | (s2[0] << 24);
#endif
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


static void NVMoveDWORDS(
   CARD32* dest,
   CARD32* src,
   int dwords )
{
     while(dwords & ~0x03) {
        *dest = *src;
        *(dest + 1) = *(src + 1);
        *(dest + 2) = *(src + 2);
        *(dest + 3) = *(src + 3);
        src += 4;
        dest += 4;
        dwords -= 4;
     }
     if(!dwords) return;
     *dest = *src;
     if(dwords == 1) return;
     *(dest + 1) = *(src + 1);
     if(dwords == 2) return;
     *(dest + 2) = *(src + 2);
}

#if X_BYTE_ORDER == X_BIG_ENDIAN
static void NVMoveDWORDSSwapped(
   CARD32* dest,
   CARD8* src,
   int dwords )
{
     while(dwords--) {
        *dest++ = (src[3] << 24) | (src[2] << 16) | (src[1] << 8) | src[0];
        src += 4;
     }
}
#endif

static void NVCopyData422
(
  unsigned char *src,
  unsigned char *dst,
  int            srcPitch,
  int            dstPitch,
  int            h,
  int            w
)
{
    w >>= 1;  /* pixels to DWORDS */
    while(h--) {
        NVMoveDWORDS((CARD32*)dst, (CARD32*)src, w);
        src += srcPitch;
        dst += dstPitch;
    }
}

static void NVCopyDataRGB
(
  unsigned char *src,
  unsigned char *dst,
  int            srcPitch,
  int            dstPitch,
  int            h,
  int            w
)
{
    while(h--) {
#if X_BYTE_ORDER == X_BIG_ENDIAN
        NVMoveDWORDSSwapped((CARD32*)dst, (CARD8*)src, w);
#else
        NVMoveDWORDS((CARD32*)dst, (CARD32*)src, w);
#endif
        src += srcPitch;
        dst += dstPitch;
    }
}


/*
 * PutImage
 */
static int NVPutImage
( 
    ScrnInfoPtr  pScrnInfo, 
    short        src_x,
    short        src_y, 
    short        drw_x,
    short        drw_y,
    short        src_w,
    short        src_h, 
    short        drw_w,
    short        drw_h,
    int          id,
    unsigned char *buf, 
    short        width,
    short        height, 
    Bool         Sync,
    RegionPtr    clipBoxes,
    pointer      data
)
{
    NVPortPrivPtr pPriv = (NVPortPrivPtr)data;
    NVPtr pNv = NVPTR(pScrnInfo);
    INT32 xa, xb, ya, yb;
    unsigned char *dst_start;
    int newSize, offset, s2offset, s3offset;
    int srcPitch, srcPitch2, dstPitch;
    int top, left, right, bottom, npixels, nlines, bpp;
    Bool skip = FALSE;
    BoxRec dstBox;
    CARD32 tmp;

   /*
    * s2offset, s3offset - byte offsets into U and V plane of the
    *                      source where copying starts.  Y plane is
    *                      done by editing "buf".
    *
    * offset - byte offset to the first line of the destination.
    *
    * dst_start - byte address to the first displayed pel.
    *
    */

    if(pPriv->grabbedByV4L) return Success;

    /* make the compiler happy */
    s2offset = s3offset = srcPitch2 = 0;
    
    if(!pPriv->blitter) {
       if(src_w > (drw_w << 3))
          drw_w = src_w >> 3;
       if(src_h > (drw_h << 3))
          drw_h = src_h >> 3;
    }

    /* Clip */
    xa = src_x;
    xb = src_x + src_w;
    ya = src_y;
    yb = src_y + src_h;
    
    dstBox.x1 = drw_x;
    dstBox.x2 = drw_x + drw_w;
    dstBox.y1 = drw_y;
    dstBox.y2 = drw_y + drw_h;
    
    if(!xf86XVClipVideoHelper(&dstBox, &xa, &xb, &ya, &yb, clipBoxes, 
                              width, height))
    	return Success;
    
    if(!pPriv->blitter) {
        dstBox.x1 -= pScrnInfo->frameX0;
        dstBox.x2 -= pScrnInfo->frameX0;
        dstBox.y1 -= pScrnInfo->frameY0;
        dstBox.y2 -= pScrnInfo->frameY0;
    }

    bpp = pScrnInfo->bitsPerPixel >> 3;

    switch(id) {
    case FOURCC_YV12:
    case FOURCC_I420:
        srcPitch = (width + 3) & ~3;	/* of luma */
        s2offset = srcPitch * height;
        srcPitch2 = ((width >> 1) + 3) & ~3;
        s3offset = (srcPitch2 * (height >> 1)) + s2offset;
        dstPitch = ((width << 1) + 63) & ~63;
        break;
    case FOURCC_UYVY:
    case FOURCC_YUY2:
        srcPitch = width << 1;
        dstPitch = ((width << 1) + 63) & ~63;
        break;
    case FOURCC_RGB:
        srcPitch = width << 2;
        dstPitch = ((width << 2) + 63) & ~63;
        break;
    default:
        return BadImplementation;
    }

    newSize = height * dstPitch / bpp;

    if(pPriv->doubleBuffer)
	newSize <<= 1;

    pPriv->linear = NVAllocateOverlayMemory(pScrnInfo, 
					    pPriv->linear, 
					    newSize);

    if(!pPriv->linear) return BadAlloc;

    offset = pPriv->linear->offset * bpp;

    if(pPriv->doubleBuffer) {
        int mask = 1 << (pPriv->currentBuffer << 2);

#if 0
        /* burn the CPU until the next buffer is available */
        while(pNv->PMC[0x00008700/4] & mask);
#else
        /* overwrite the newest buffer if there's not one free */
        if(pNv->PMC[0x00008700/4] & mask) {
           if(!pPriv->currentBuffer)
              offset += (newSize * bpp) >> 1;
           skip = TRUE;
        } else 
#endif
        if(pPriv->currentBuffer)
            offset += (newSize * bpp) >> 1;
    }

    dst_start = pNv->FbStart + offset;
        
    /* We need to enlarge the copied rectangle by a pixel so the HW 
       filtering doesn't pick up junk laying outside of the source */

    left = (xa - 0x00010000) >> 16;
    if(left < 0) left = 0;
    top = (ya - 0x00010000) >> 16;
    if(top < 0) top = 0;
    right = (xb + 0x0001ffff) >> 16;
    if(right > width) right = width;
    bottom = (yb + 0x0001ffff) >> 16;
    if(bottom > height) bottom = height;

    if(pPriv->blitter) NVSync(pScrnInfo);

    switch(id) {
    case FOURCC_YV12:
    case FOURCC_I420:
        left &= ~1;
        npixels = ((right + 1) & ~1) - left;
        top &= ~1;
        nlines = ((bottom + 1) & ~1) - top;

        dst_start += (left << 1) + (top * dstPitch);
        tmp = ((top >> 1) * srcPitch2) + (left >> 1);
        s2offset += tmp;
        s3offset += tmp;
        if(id == FOURCC_I420) {
           tmp = s2offset;
           s2offset = s3offset;
           s3offset = tmp;
        }
        NVCopyData420(buf + (top * srcPitch) + left,
				buf + s2offset, buf + s3offset,
				dst_start, srcPitch, srcPitch2,
				dstPitch, nlines, npixels);
        break;
    case FOURCC_UYVY:
    case FOURCC_YUY2:
        left &= ~1;
        npixels = ((right + 1) & ~1) - left;
        nlines = bottom - top;
        
        left <<= 1;
        buf += (top * srcPitch) + left;
        dst_start += left + (top * dstPitch);

        NVCopyData422(buf, dst_start, srcPitch, dstPitch, nlines, npixels);
        break;
    case FOURCC_RGB:
        npixels = right - left;
        nlines = bottom - top;

        left <<= 2;
        buf += (top * srcPitch) + left;
        dst_start += left + (top * dstPitch);

        NVCopyDataRGB(buf, dst_start, srcPitch, dstPitch, nlines, npixels);
        break;
    default:
        return BadImplementation;
    }

    if(!skip) {
       if(pPriv->blitter) {
            NVPutBlitImage(pScrnInfo, offset, id, dstPitch, &dstBox,
                           xa, ya, xb, yb,
                           width, height, src_w, src_h, drw_w, drw_h,
                           clipBoxes);
       } else {
            NVPutOverlayImage(pScrnInfo, offset, id, dstPitch, &dstBox, 
                             xa, ya, xb, yb,
                             width, height, src_w, src_h, drw_w, drw_h, 
                             clipBoxes);
            pPriv->currentBuffer ^= 1;
       }
    } 

    return Success;
}
/*
 * QueryImageAttributes
 */
static int NVQueryImageAttributes
(
    ScrnInfoPtr pScrnInfo, 
    int         id, 
    unsigned short *w,
    unsigned short *h, 
    int         *pitches,
    int         *offsets
)
{
    int size, tmp;
    
    if(*w > 2046)
        *w = 2046;
    if(*h > 2046)
        *h = 2046;
    
    *w = (*w + 1) & ~1;
    if (offsets)
        offsets[0] = 0;
    
    switch (id)
    {
        case FOURCC_YV12:
        case FOURCC_I420:
            *h = (*h + 1) & ~1;
            size = (*w + 3) & ~3;
            if (pitches)
                pitches[0] = size;
            size *= *h;
            if (offsets)
                offsets[1] = size;
            tmp = ((*w >> 1) + 3) & ~3;
            if (pitches)
                pitches[1] = pitches[2] = tmp;
            tmp *= (*h >> 1);
            size += tmp;
            if (offsets)
                offsets[2] = size;
            size += tmp;
            break;
        case FOURCC_UYVY:
        case FOURCC_YUY2:
            size = *w << 1;
            if (pitches)
                pitches[0] = size;
            size *= *h;
            break;
        case FOURCC_RGB:
            size = *w << 2;
            if(pitches)
               pitches[0] = size;
            size *= *h;
            break;
        default:
            *w = *h = size = 0;
            break;
    }
    return size;
}

static void NVVideoTimerCallback 
(
    ScrnInfoPtr pScrnInfo,
    Time currentTime
)
{
    NVPtr         pNv = NVPTR(pScrnInfo);
    NVPortPrivPtr pOverPriv = NULL;
    NVPortPrivPtr pBlitPriv = NULL;
    Bool needCallback = FALSE;

    if(!pScrnInfo->vtSema) return; 

    if(pNv->overlayAdaptor) {
	pOverPriv = GET_OVERLAY_PRIVATE(pNv);
	if(!pOverPriv->videoStatus)
	   pOverPriv = NULL;
    }

    if(pNv->blitAdaptor) {
        pBlitPriv = GET_BLIT_PRIVATE(pNv);
        if(!pBlitPriv->videoStatus)
           pBlitPriv = NULL;
    }
   
    if(pOverPriv) {
         if(pOverPriv->videoTime < currentTime) {
	    if(pOverPriv->videoStatus & OFF_TIMER) {
		NVStopOverlay(pScrnInfo);
		pOverPriv->videoStatus = FREE_TIMER;
                pOverPriv->videoTime = currentTime + FREE_DELAY;
                needCallback = TRUE;
	    } else
            if(pOverPriv->videoStatus & FREE_TIMER) {
		NVFreeOverlayMemory(pScrnInfo);
		pOverPriv->videoStatus = 0;
	    }	
	 } else {
            needCallback = TRUE;
         }
    }

    if(pBlitPriv) {
        if(pBlitPriv->videoTime < currentTime) {
            NVFreeBlitMemory(pScrnInfo);
            pBlitPriv->videoStatus = 0;              
        } else {
            needCallback = TRUE;
        }
    }

    pNv->VideoTimerCallback = needCallback ? NVVideoTimerCallback : NULL;
}


/***** Exported offscreen surface stuff ****/


static int
NVAllocSurface (
    ScrnInfoPtr pScrnInfo,
    int id,
    unsigned short w,   
    unsigned short h,
    XF86SurfacePtr surface
)
{
    NVPtr pNv = NVPTR(pScrnInfo);
    NVPortPrivPtr pPriv = GET_OVERLAY_PRIVATE(pNv); 
    int size, bpp;

    bpp = pScrnInfo->bitsPerPixel >> 3;

    if(pPriv->grabbedByV4L) return BadAlloc;

    if((w > 2046) || (h > 2046)) return BadValue;

    w = (w + 1) & ~1;
    pPriv->pitch = ((w << 1) + 63) & ~63;
    size = h * pPriv->pitch / bpp;

    pPriv->linear = NVAllocateOverlayMemory(pScrnInfo, pPriv->linear,
					    size);

    if(!pPriv->linear) return BadAlloc;

    pPriv->offset = pPriv->linear->offset * bpp;

    surface->width = w;
    surface->height = h;
    surface->pScrn = pScrnInfo;
    surface->pitches = &pPriv->pitch; 
    surface->offsets = &pPriv->offset;
    surface->devPrivate.ptr = (pointer)pPriv;
    surface->id = id;

    /* grab the video */
    NVStopOverlay(pScrnInfo);
    pPriv->videoStatus = 0;
    REGION_EMPTY(pScrnInfo->pScreen, &pPriv->clip);
    pPriv->grabbedByV4L = TRUE;

    return Success;
}

static int
NVStopSurface (XF86SurfacePtr surface)
{
    NVPortPrivPtr pPriv = (NVPortPrivPtr)(surface->devPrivate.ptr);

    if(pPriv->grabbedByV4L && pPriv->videoStatus) {
	NVStopOverlay(surface->pScrn);
	pPriv->videoStatus = 0;
    }

    return Success;
}

static int 
NVFreeSurface (XF86SurfacePtr surface)
{
    NVPortPrivPtr pPriv = (NVPortPrivPtr)(surface->devPrivate.ptr);

    if(pPriv->grabbedByV4L) {
	NVStopSurface(surface);
	NVFreeOverlayMemory(surface->pScrn);
	pPriv->grabbedByV4L = FALSE;
    }

    return Success;
}

static int
NVGetSurfaceAttribute (
    ScrnInfoPtr pScrnInfo,
    Atom attribute,
    INT32 *value
)
{
    NVPtr pNv = NVPTR(pScrnInfo);
    NVPortPrivPtr pPriv = GET_OVERLAY_PRIVATE(pNv);
    
    return NVGetOverlayPortAttribute(pScrnInfo, attribute, value, (pointer)pPriv);
}

static int
NVSetSurfaceAttribute(
    ScrnInfoPtr pScrnInfo,
    Atom attribute,
    INT32 value
)
{
    NVPtr pNv = NVPTR(pScrnInfo);
    NVPortPrivPtr pPriv = GET_OVERLAY_PRIVATE(pNv);
   
    return NVSetOverlayPortAttribute(pScrnInfo, attribute, value, (pointer)pPriv);
}

static int
NVDisplaySurface (
    XF86SurfacePtr surface,
    short src_x, short src_y, 
    short drw_x, short drw_y,
    short src_w, short src_h, 
    short drw_w, short drw_h,
    RegionPtr clipBoxes
)
{
    ScrnInfoPtr pScrnInfo = surface->pScrn;
    NVPortPrivPtr pPriv = (NVPortPrivPtr)(surface->devPrivate.ptr);
    INT32 xa, xb, ya, yb;
    BoxRec dstBox;

    if(!pPriv->grabbedByV4L) return Success;

    if(src_w > (drw_w << 3))
	drw_w = src_w >> 3;
    if(src_h > (drw_h << 3))
	drw_h = src_h >> 3;

    /* Clip */
    xa = src_x;
    xb = src_x + src_w;
    ya = src_y;
    yb = src_y + src_h;
    
    dstBox.x1 = drw_x;
    dstBox.x2 = drw_x + drw_w;
    dstBox.y1 = drw_y;
    dstBox.y2 = drw_y + drw_h;
    
    if(!xf86XVClipVideoHelper(&dstBox, &xa, &xb, &ya, &yb, clipBoxes, 
		    surface->width, surface->height))
    {
        return Success;
    }
    
    dstBox.x1 -= pScrnInfo->frameX0;
    dstBox.x2 -= pScrnInfo->frameX0;
    dstBox.y1 -= pScrnInfo->frameY0;
    dstBox.y2 -= pScrnInfo->frameY0;

    pPriv->currentBuffer = 0;

    NVPutOverlayImage (pScrnInfo, surface->offsets[0], surface->id,
			surface->pitches[0], &dstBox, xa, ya, xb, yb,
			surface->width, surface->height, src_w, src_h,
			drw_w, drw_h, clipBoxes);

    return Success;
}

XF86OffscreenImageRec NVOffscreenImages[2] =
{
 {
   &NVImages[0],
   VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT,
   NVAllocSurface,
   NVFreeSurface,
   NVDisplaySurface,
   NVStopSurface,
   NVGetSurfaceAttribute,
   NVSetSurfaceAttribute,
   2046, 2046,
   NUM_ATTRIBUTES - 1,
   &NVAttributes[1]
  },
 {
   &NVImages[2],
   VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT,
   NVAllocSurface,
   NVFreeSurface,
   NVDisplaySurface,
   NVStopSurface,
   NVGetSurfaceAttribute,
   NVSetSurfaceAttribute,
   2046, 2046,
   NUM_ATTRIBUTES - 1,
   &NVAttributes[1]
  },
};

static void
NVInitOffscreenImages (ScreenPtr pScreen)
{
    xf86XVRegisterOffscreenImages(pScreen, NVOffscreenImages, 2);
}
