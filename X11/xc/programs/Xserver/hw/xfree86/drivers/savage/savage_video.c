/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/savage/savage_video.c,v 1.11 2003/01/12 03:55:49 tsi Exp $ */

#include "Xv.h"
#include "dix.h"
#include "dixstruct.h"
#include "fourcc.h"
#include "xaalocal.h"

#include "savage_driver.h"

#define OFF_DELAY 	200  /* milliseconds */
#define FREE_DELAY 	60000

#define OFF_TIMER 	0x01
#define FREE_TIMER	0x02
#define CLIENT_VIDEO_ON	0x04

#define TIMER_MASK      (OFF_TIMER | FREE_TIMER)

#ifndef XvExtension
void SavageInitVideo(ScreenPtr pScreen) {}
void SavageResetVideo(ScrnInfoPtr pScrn) {}
#else

void myOUTREG( SavagePtr psav, unsigned long offset, unsigned long value );

static XF86VideoAdaptorPtr SavageSetupImageVideo(ScreenPtr);
static void SavageInitOffscreenImages(ScreenPtr);
static void SavageStopVideo(ScrnInfoPtr, pointer, Bool);
static int SavageSetPortAttribute(ScrnInfoPtr, Atom, INT32, pointer);
static int SavageGetPortAttribute(ScrnInfoPtr, Atom ,INT32 *, pointer);
static void SavageQueryBestSize(ScrnInfoPtr, Bool,
	short, short, short, short, unsigned int *, unsigned int *, pointer);
static int SavagePutImage( ScrnInfoPtr, 
	short, short, short, short, short, short, short, short,
	int, unsigned char*, short, short, Bool, RegionPtr, pointer);
static int SavageQueryImageAttributes(ScrnInfoPtr, 
	int, unsigned short *, unsigned short *,  int *, int *);

void SavageStreamsOn(ScrnInfoPtr pScrn, int id);
void SavageStreamsOff(ScrnInfoPtr pScrn);
void SavageResetVideo(ScrnInfoPtr pScrn); 

static void SavageInitStreamsOld(ScrnInfoPtr pScrn);
static void SavageInitStreamsNew(ScrnInfoPtr pScrn);
static void (*SavageInitStreams)(ScrnInfoPtr pScrn) = NULL;

static void SavageSetColorKeyOld(ScrnInfoPtr pScrn);
static void SavageSetColorKeyNew(ScrnInfoPtr pScrn);
static void (*SavageSetColorKey)(ScrnInfoPtr pScrn) = NULL;

static void SavageSetColorOld(ScrnInfoPtr pScrn );
static void SavageSetColorNew(ScrnInfoPtr pScrn );
static void (*SavageSetColor)(ScrnInfoPtr pScrn ) = NULL;

static void SavageDisplayVideoOld(
    ScrnInfoPtr pScrn, int id, int offset,
    short width, short height, int pitch, 
    int x1, int y1, int x2, int y2,
    BoxPtr dstBox,
    short src_w, short src_h,
    short drw_w, short drw_h
);
static void SavageDisplayVideoNew(
    ScrnInfoPtr pScrn, int id, int offset,
    short width, short height, int pitch, 
    int x1, int y1, int x2, int y2,
    BoxPtr dstBox,
    short src_w, short src_h,
    short drw_w, short drw_h
);
static void (*SavageDisplayVideo)(
    ScrnInfoPtr pScrn, int id, int offset,
    short width, short height, int pitch, 
    int x1, int y1, int x2, int y2,
    BoxPtr dstBox,
    short src_w, short src_h,
    short drw_w, short drw_h
) = NULL;

static void OverlayParamInit(ScrnInfoPtr pScrn);
static void InitStreamsForExpansion(SavagePtr psav);

/*static void SavageBlockHandler(int, pointer, pointer, pointer);*/

#define XVTRACE	4

#define MAKE_ATOM(a) MakeAtom(a, sizeof(a) - 1, TRUE)

static Atom xvColorKey, xvBrightness, xvContrast, xvSaturation, xvHue;

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

#define NUM_ATTRIBUTES 5

static XF86AttributeRec Attributes[NUM_ATTRIBUTES] =
{
   {XvSettable | XvGettable, 0, (1 << 24) - 1, "XV_COLORKEY"},
   {XvSettable | XvGettable, -128, 127, "XV_BRIGHTNESS"},
   {XvSettable | XvGettable, 0, 255, "XV_CONTRAST"},
   {XvSettable | XvGettable, 0, 255, "XV_SATURATION"},
   {XvSettable | XvGettable, -180, 180, "XV_HUE"}
};

#define FOURCC_RV16	0x36315652
#define FOURCC_RV15	0x35315652
#define FOURCC_Y211	0x31313259

/*
 * For completeness sake, here is a cracking of the fourcc's I support.
 *
 * YUY2, packed 4:2:2, byte order: Y0 U0 Y1 V0  Y2 U2 Y3 V2
 * Y211, packed 2:1:1, byte order: Y0 U0 Y2 V0  Y4 U2 Y6 V2
 * YV12, planar 4:1:1, Y plane HxW, V plane H/2xW/2, U plane H/2xW/2
 * I420, planar 4:1:1, Y plane HxW, U plane H/2xW/2, V plane H/2xW/2
 * (I420 is also known as IYUV)
 */
  

static XF86ImageRec Images[] =
{
   XVIMAGE_YUY2,
   XVIMAGE_YV12,
   XVIMAGE_I420,
   {
	FOURCC_RV15,
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
   {
	FOURCC_RV16,
        XvRGB,
	LSBFirst,
	{'R','V','1','6',
	  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
	16,
	XvPacked,
	1,
	16, 0x001F, 0x07E0, 0xF800,
	0, 0, 0,
	0, 0, 0,
	0, 0, 0,
	{'R','V','B',0,
	  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
	XvTopToBottom
   },
   {
	FOURCC_Y211,
	XvYUV,
	LSBFirst,
	{'Y','2','1','1',
	  0x00,0x00,0x00,0x10,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71},
	6,
	XvPacked,
	3,
	0, 0, 0, 0 ,
	8, 8, 8, 
	2, 4, 4,
	1, 1, 1,
	{'Y','U','Y','V',
	  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
	XvTopToBottom
   }
};

#define NUM_IMAGES (sizeof(Images)/sizeof(Images[0]))

typedef struct {
   int		brightness;	/* -128 .. 127 */
   CARD32	contrast;	/* 0 .. 255 */
   CARD32	saturation;	/* 0 .. 255 */
   int		hue;		/* -128 .. 127 */

   FBAreaPtr	area;
   RegionRec	clip;
   CARD32	colorKey;
   CARD32	videoStatus;
   Time		offTime;
   Time		freeTime;
   int		lastKnownPitch;
} SavagePortPrivRec, *SavagePortPrivPtr;


#define GET_PORT_PRIVATE(pScrn) \
   (SavagePortPrivPtr)((SAVPTR(pScrn))->adaptor->pPortPrivates[0].ptr)

/**************************************
   S3 streams processor
**************************************/

#define EXT_MISC_CTRL2              0x67

/* New streams */

/* CR67[2] = 1 : enable stream 1 */
#define ENABLE_STREAM1              0x04
/* CR67[1] = 1 : enable stream 2 */
#define ENABLE_STREAM2              0x02
/* mask to clear CR67[2,1] */
#define NO_STREAMS                  0xF9
/* CR67[3] = 1 : Mem-mapped regs */
#define USE_MM_FOR_PRI_STREAM       0x08

#define HDM_SHIFT	16
#define HDSCALE_4	(2 << HDM_SHIFT)
#define HDSCALE_8	(3 << HDM_SHIFT)
#define HDSCALE_16	(4 << HDM_SHIFT)
#define HDSCALE_32	(5 << HDM_SHIFT)
#define HDSCALE_64	(6 << HDM_SHIFT)

/* Old Streams */

#define ENABLE_STREAMS_OLD	    0x0c
#define NO_STREAMS_OLD		    0xf3
/* CR69[0] = 1 : Mem-mapped regs */
#define USE_MM_FOR_PRI_STREAM_OLD   0x01


/*
 * There are two different streams engines used in the Savage line.
 * The old engine is in the 3D, 4, Pro, and Twister.
 * The new engine is in the 2000, MX, IX, and Super.
 */


/* streams registers for old engine */
#define PSTREAM_CONTROL_REG		0x8180
#define COL_CHROMA_KEY_CONTROL_REG	0x8184
#define SSTREAM_CONTROL_REG		0x8190
#define CHROMA_KEY_UPPER_BOUND_REG	0x8194
#define SSTREAM_STRETCH_REG		0x8198
#define COLOR_ADJUSTMENT_REG		0x819C
#define BLEND_CONTROL_REG		0x81A0
#define PSTREAM_FBADDR0_REG		0x81C0
#define PSTREAM_FBADDR1_REG		0x81C4
#define PSTREAM_STRIDE_REG		0x81C8
#define DOUBLE_BUFFER_REG		0x81CC
#define SSTREAM_FBADDR0_REG		0x81D0
#define SSTREAM_FBADDR1_REG		0x81D4
#define SSTREAM_STRIDE_REG		0x81D8
#define SSTREAM_VSCALE_REG		0x81E0
#define SSTREAM_VINITIAL_REG		0x81E4
#define SSTREAM_LINES_REG		0x81E8
#define STREAMS_FIFO_REG		0x81EC
#define PSTREAM_WINDOW_START_REG	0x81F0
#define PSTREAM_WINDOW_SIZE_REG		0x81F4
#define SSTREAM_WINDOW_START_REG	0x81F8
#define SSTREAM_WINDOW_SIZE_REG		0x81FC
#define FIFO_CONTROL			0x8200
#define PSTREAM_FBSIZE_REG		0x8300
#define SSTREAM_FBSIZE_REG		0x8304
#define SSTREAM_FBADDR2_REG		0x8308

#define OS_XY(x,y)	(((x+1)<<16)|(y+1))
#define OS_WH(x,y)	(((x-1)<<16)|(y))

static
unsigned int GetBlendForFourCC( int id )
{
    switch( id ) {
	case FOURCC_YUY2:
	case FOURCC_YV12:
	case FOURCC_I420:
	    return 1;
	case FOURCC_Y211:
	    return 4;
	case FOURCC_RV15:
	    return 3;
	case FOURCC_RV16:
	    return 5;
        default:
	    return 0;
    }
}

void myOUTREG( SavagePtr psav, unsigned long offset, unsigned long value )
{
    ErrorF( "MMIO %04x, was %08x, want %08x,", 
	offset, MMIO_IN32( psav->MapBase, offset ), value );
    MMIO_OUT32( psav->MapBase, offset, value );
    ErrorF( " now %08x\n", MMIO_IN32( psav->MapBase, offset ) );
}

void SavageInitStreamsOld(ScrnInfoPtr pScrn)
{
    SavagePtr psav = SAVPTR(pScrn);
    unsigned long jDelta;
    unsigned long format = 0;

    /*
     * For the OLD streams engine, several of these registers
     * cannot be touched unless streams are on.  Seems backwards to me;
     * I'd want to set 'em up, then cut 'em loose.
     */

    xf86ErrorFVerb(XVTRACE, "SavageInitStreams\n" );

    /* Primary stream reflects the frame buffer. */

    switch( pScrn->depth ) {
    case  8: format = 0 << 24; break;
    case 15: format = 3 << 24; break;
    case 16: format = 5 << 24; break;
    case 24: format = 7 << 24; break;
    }

    jDelta = pScrn->displayWidth * pScrn->bitsPerPixel / 8;
    OUTREG( PSTREAM_WINDOW_START_REG, OS_XY(0,0) );
    OUTREG( PSTREAM_WINDOW_SIZE_REG, OS_WH(pScrn->displayWidth, pScrn->virtualY) );
    OUTREG( PSTREAM_FBADDR0_REG, pScrn->fbOffset );
    OUTREG( PSTREAM_FBADDR1_REG, 0 );
    OUTREG( PSTREAM_STRIDE_REG, jDelta );
    OUTREG( PSTREAM_CONTROL_REG, format );
    OUTREG( PSTREAM_FBSIZE_REG, jDelta * pScrn->virtualY >> 3 );

    OUTREG( COL_CHROMA_KEY_CONTROL_REG, 0 );
    OUTREG( SSTREAM_CONTROL_REG, 0 );
    OUTREG( CHROMA_KEY_UPPER_BOUND_REG, 0 );
    OUTREG( SSTREAM_STRETCH_REG, 0 );
    OUTREG( COLOR_ADJUSTMENT_REG, 0 );
    OUTREG( BLEND_CONTROL_REG, 1 << 24 );
    OUTREG( DOUBLE_BUFFER_REG, 0 );
    OUTREG( SSTREAM_FBADDR0_REG, 0 );
    OUTREG( SSTREAM_FBADDR1_REG, 0 );
    OUTREG( SSTREAM_FBADDR2_REG, 0 );
/*    OUTREG( SSTREAM_FBSIZE_REG, 0 ); */
    OUTREG( SSTREAM_STRIDE_REG, 0 );
    OUTREG( SSTREAM_VSCALE_REG, 0 );
    OUTREG( SSTREAM_LINES_REG, 0 );
    OUTREG( SSTREAM_VINITIAL_REG, 0 );
    OUTREG( SSTREAM_WINDOW_START_REG, OS_XY(0xfffe, 0xfffe) );
    OUTREG( SSTREAM_WINDOW_SIZE_REG, OS_WH(10,2) );
}

#undef OUTREG
#if 0
#define OUTREG(a,v)	myOUTREG(psav,a,v)
#else
#define OUTREG(addr,val) MMIO_OUT32(psav->MapBase, addr, val)
#endif

void SavageInitStreamsNew(ScrnInfoPtr pScrn)
{
    SavagePtr psav = SAVPTR(pScrn);
    unsigned long jDelta;

    xf86ErrorFVerb(XVTRACE, "SavageInitStreams\n" );

    if( 
	S3_SAVAGE_MOBILE_SERIES(psav->Chipset) && 
	!psav->CrtOnly && 
	!psav->TvOn 
    ) {
	OverlayParamInit( pScrn );
    }

    /* Primary stream reflects the frame buffer. */

    jDelta = pScrn->displayWidth * pScrn->bitsPerPixel / 8;
    OUTREG( PRI_STREAM_BUFFERSIZE, jDelta * pScrn->virtualY >> 3 );
    OUTREG( PRI_STREAM_FBUF_ADDR0, pScrn->fbOffset );
    OUTREG( PRI_STREAM_STRIDE, jDelta );

    OUTREG( SEC_STREAM_CKEY_LOW, 0 );
    OUTREG( SEC_STREAM_CKEY_UPPER, 0 );
    OUTREG( SEC_STREAM_HSCALING, 0 );
    OUTREG( SEC_STREAM_VSCALING, 0 );
    OUTREG( BLEND_CONTROL, 0 );
    OUTREG( SEC_STREAM_FBUF_ADDR0, 0 );
    OUTREG( SEC_STREAM_FBUF_ADDR1, 0 );
    OUTREG( SEC_STREAM_FBUF_ADDR2, 0 );
    OUTREG( SEC_STREAM_WINDOW_START, 0 );
    OUTREG( SEC_STREAM_WINDOW_SZ, 0 );
/*    OUTREG( SEC_STREAM_BUFFERSIZE, 0 ); */
    OUTREG( SEC_STREAM_TILE_OFF, 0 );
    OUTREG( SEC_STREAM_OPAQUE_OVERLAY, 0 );
    OUTREG( SEC_STREAM_STRIDE, 0 );

    /* These values specify brightness, contrast, saturation and hue. */
    OUTREG( SEC_STREAM_COLOR_CONVERT1, 0x0000C892 );
    OUTREG( SEC_STREAM_COLOR_CONVERT2, 0x00039F9A );
    OUTREG( SEC_STREAM_COLOR_CONVERT3, 0x01F1547E );
}

void SavageStreamsOn(ScrnInfoPtr pScrn, int id)
{
    SavagePtr psav = SAVPTR(pScrn);
    unsigned char jStreamsControl;
    unsigned short vgaCRIndex = psav->vgaIOBase + 4;
    unsigned short vgaCRReg = psav->vgaIOBase + 5;

    xf86ErrorFVerb(XVTRACE, "SavageStreamsOn\n" );

    /* Sequence stolen from streams.c in M7 NT driver */


    xf86EnableIO();

    /* Unlock extended registers. */

    VGAOUT16(vgaCRIndex, 0x4838);
    VGAOUT16(vgaCRIndex, 0xa039);
    VGAOUT16(0x3c4, 0x0608);

    VGAOUT8( vgaCRIndex, EXT_MISC_CTRL2 );

    if( S3_SAVAGE_MOBILE_SERIES(psav->Chipset) ||
        (psav->Chipset == S3_SUPERSAVAGE) ||
        (psav->Chipset == S3_SAVAGE2000) )
    {
	jStreamsControl = VGAIN8( vgaCRReg ) | ENABLE_STREAM1;

	/* Wait for VBLANK. */
	
	VerticalRetraceWait(psav);

	/* Fire up streams! */

	VGAOUT16( vgaCRIndex, (jStreamsControl << 8) | EXT_MISC_CTRL2 );

	psav->blendBase = GetBlendForFourCC( id ) << 9;
	xf86ErrorFVerb(XVTRACE+1,"Format %4.4s, blend is %08x\n", &id, psav->blendBase );
	OUTREG( BLEND_CONTROL, psav->blendBase | 0x08 );

	/* These values specify brightness, contrast, saturation and hue. */
	OUTREG( SEC_STREAM_COLOR_CONVERT1, 0x0000C892 );
	OUTREG( SEC_STREAM_COLOR_CONVERT2, 0x00039F9A );
	OUTREG( SEC_STREAM_COLOR_CONVERT3, 0x01F1547E );
    }
    else
    {
	jStreamsControl = VGAIN8( vgaCRReg ) | ENABLE_STREAMS_OLD;

	/* Wait for VBLANK. */
	
	VerticalRetraceWait(psav);

	/* Fire up streams! */

	VGAOUT16( vgaCRIndex, (jStreamsControl << 8) | EXT_MISC_CTRL2 );

	SavageInitStreamsOld( pScrn );
    }

    /* Wait for VBLANK. */
    
    VerticalRetraceWait(psav);

    /* Turn on secondary stream TV flicker filter, once we support TV. */

    /* SR70 |= 0x10 */

    psav->videoFlags |= VF_STREAMS_ON;
    psav->videoFourCC = id;
}


void SavageStreamsOff(ScrnInfoPtr pScrn)
{
    SavagePtr psav = SAVPTR(pScrn);
    unsigned char jStreamsControl;
    unsigned short vgaCRIndex = psav->vgaIOBase + 4;
    unsigned short vgaCRReg = psav->vgaIOBase + 5;

    xf86ErrorFVerb(XVTRACE, "SavageStreamsOff\n" );

    xf86EnableIO();

    /* Unlock extended registers. */

    VGAOUT16(vgaCRIndex, 0x4838);
    VGAOUT16(vgaCRIndex, 0xa039);
    VGAOUT16(0x3c4, 0x0608);

    VGAOUT8( vgaCRIndex, EXT_MISC_CTRL2 );
    if( S3_SAVAGE_MOBILE_SERIES(psav->Chipset)  ||
        (psav->Chipset == S3_SUPERSAVAGE) ||
        (psav->Chipset == S3_SAVAGE2000) )
	jStreamsControl = VGAIN8( vgaCRReg ) & NO_STREAMS;
    else
	jStreamsControl = VGAIN8( vgaCRReg ) & NO_STREAMS_OLD;

    /* Wait for VBLANK. */

    VerticalRetraceWait(psav);

    /* Kill streams. */

    VGAOUT16( vgaCRIndex, (jStreamsControl << 8) | EXT_MISC_CTRL2 );

    VGAOUT16( vgaCRIndex, 0x0093 );
    VGAOUT8( vgaCRIndex, 0x92 );
    VGAOUT8( vgaCRReg, VGAIN8(vgaCRReg) & 0x40 );

    psav->videoFlags &= ~VF_STREAMS_ON;
}


void SavageInitVideo(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    XF86VideoAdaptorPtr *adaptors, *newAdaptors = NULL;
    XF86VideoAdaptorPtr newAdaptor = NULL;
    SavagePtr psav = SAVPTR(pScrn);
    int num_adaptors;

    xf86ErrorFVerb(XVTRACE,"SavageInitVideo\n");
    if(
	S3_SAVAGE_MOBILE_SERIES(psav->Chipset) ||
        (psav->Chipset == S3_SUPERSAVAGE) ||
	(psav->Chipset == S3_SAVAGE2000)
    )
    {
	newAdaptor = SavageSetupImageVideo(pScreen);
	SavageInitOffscreenImages(pScreen);

	SavageInitStreams = SavageInitStreamsNew;
	SavageSetColor = SavageSetColorNew;
	SavageSetColorKey = SavageSetColorKeyNew;
	SavageDisplayVideo = SavageDisplayVideoNew;
    }
    else
    {
	newAdaptor = SavageSetupImageVideo(pScreen);
	SavageInitOffscreenImages(pScreen);
    /*DELETENEXTLINE*/
	/* Since newAdaptor is still NULL, these are still disabled for now. */
	SavageInitStreams = SavageInitStreamsOld;
	SavageSetColor = SavageSetColorOld;
	SavageSetColorKey = SavageSetColorKeyOld;
	SavageDisplayVideo = SavageDisplayVideoOld;
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

    if( newAdaptor )
    {
	if( SavageInitStreams == SavageInitStreamsNew )
	    SavageInitStreams(pScrn);
	psav->videoFlags = 0;
	psav->videoFourCC = 0;
    }
}


void SavageSetColorKeyOld(ScrnInfoPtr pScrn)
{
    SavagePtr psav = SAVPTR(pScrn);
    SavagePortPrivPtr pPriv = psav->adaptor->pPortPrivates[0].ptr;
    int red, green, blue;

    /* Here, we reset the colorkey and all the controls. */

    red = (pPriv->colorKey & pScrn->mask.red) >> pScrn->offset.red;
    green = (pPriv->colorKey & pScrn->mask.green) >> pScrn->offset.green;
    blue = (pPriv->colorKey & pScrn->mask.blue) >> pScrn->offset.blue;

    if( !pPriv->colorKey ) {
	OUTREG( COL_CHROMA_KEY_CONTROL_REG, 0 );
	OUTREG( CHROMA_KEY_UPPER_BOUND_REG, 0 );
	OUTREG( BLEND_CONTROL_REG, 0 );
    }
    else {
	switch (pScrn->depth) {
	case 8:
	    OUTREG( COL_CHROMA_KEY_CONTROL_REG,
		0x37000000 | (pPriv->colorKey & 0xFF) );
	    OUTREG( CHROMA_KEY_UPPER_BOUND_REG,
		0x00000000 | (pPriv->colorKey & 0xFF) );
	    break;
	case 15:
	    OUTREG( COL_CHROMA_KEY_CONTROL_REG, 
		0x05000000 | (red<<19) | (green<<11) | (blue<<3) );
	    OUTREG( CHROMA_KEY_UPPER_BOUND_REG, 
		0x00000000 | (red<<19) | (green<<11) | (blue<<3) );
	    break;
	case 16:
	    OUTREG( COL_CHROMA_KEY_CONTROL_REG, 
		0x16000000 | (red<<19) | (green<<10) | (blue<<3) );
	    OUTREG( CHROMA_KEY_UPPER_BOUND_REG, 
		0x00020002 | (red<<19) | (green<<10) | (blue<<3) );
	    break;
	case 24:
	    OUTREG( COL_CHROMA_KEY_CONTROL_REG, 
		0x17000000 | (red<<16) | (green<<8) | (blue) );
	    OUTREG( CHROMA_KEY_UPPER_BOUND_REG, 
		0x00000000 | (red<<16) | (green<<8) | (blue) );
	    break;
	}    

	/* We use destination colorkey */
	OUTREG( BLEND_CONTROL_REG, 0x05000000 );
    }
}

void SavageSetColorKeyNew(ScrnInfoPtr pScrn) 
{
    SavagePtr psav = SAVPTR(pScrn);
    SavagePortPrivPtr pPriv = psav->adaptor->pPortPrivates[0].ptr;
    int red, green, blue;

    /* Here, we reset the colorkey and all the controls. */

    red = (pPriv->colorKey & pScrn->mask.red) >> pScrn->offset.red;
    green = (pPriv->colorKey & pScrn->mask.green) >> pScrn->offset.green;
    blue = (pPriv->colorKey & pScrn->mask.blue) >> pScrn->offset.blue;

    if( !pPriv->colorKey ) {
	OUTREG( SEC_STREAM_CKEY_LOW, 0 );
	OUTREG( SEC_STREAM_CKEY_UPPER, 0 );
	OUTREG( BLEND_CONTROL, psav->blendBase | 0x08 );
    }
    else {
	switch (pScrn->depth) {
	case 8:
	    OUTREG( SEC_STREAM_CKEY_LOW, 
		0x47000000 | (pPriv->colorKey & 0xFF) );
	    OUTREG( SEC_STREAM_CKEY_UPPER,
		0x47000000 | (pPriv->colorKey & 0xFF) );
	    break;
	case 15:
	    OUTREG( SEC_STREAM_CKEY_LOW, 
		0x45000000 | (red<<19) | (green<<11) | (blue<<3) );
	    OUTREG( SEC_STREAM_CKEY_UPPER, 
		0x45000000 | (red<<19) | (green<<11) | (blue<<3) );
	    break;
	case 16:
	    OUTREG( SEC_STREAM_CKEY_LOW, 
		0x46000000 | (red<<19) | (green<<10) | (blue<<3) );
	    OUTREG( SEC_STREAM_CKEY_UPPER, 
		0x46020002 | (red<<19) | (green<<10) | (blue<<3) );
	    break;
	case 24:
	    OUTREG( SEC_STREAM_CKEY_LOW, 
		0x47000000 | (red<<16) | (green<<8) | (blue) );
	    OUTREG( SEC_STREAM_CKEY_UPPER, 
		0x47000000 | (red<<16) | (green<<8) | (blue) );
	    break;
	}    

	/* We assume destination colorkey */
	OUTREG( BLEND_CONTROL, psav->blendBase | 0x08 );
    }
}


void SavageSetColorOld( ScrnInfoPtr pScrn )
{
    SavagePtr psav = SAVPTR(pScrn);
    SavagePortPrivPtr pPriv = psav->adaptor->pPortPrivates[0].ptr;

    xf86ErrorFVerb(XVTRACE, "bright %d, contrast %d, saturation %d, hue %d\n",
	pPriv->brightness, pPriv->contrast, pPriv->saturation, pPriv->hue );

    if( 
	(psav->videoFourCC == FOURCC_RV15) ||
	(psav->videoFourCC == FOURCC_RV16)
    )
    {
	OUTREG( COLOR_ADJUSTMENT_REG, 0 );
    }
    else
    {
        /* Change 0..255 into 0..15 */
	long sat = pPriv->saturation * 16 / 256;
	double hue = pPriv->hue * 0.017453292;
	unsigned long hs1 = ((long)(sat * cos(hue))) & 0x1f;
	unsigned long hs2 = ((long)(sat * sin(hue))) & 0x1f;

	OUTREG( COLOR_ADJUSTMENT_REG, 
	    0x80008000 |
	    (pPriv->brightness + 128) |
	    ((pPriv->contrast & 0xf8) << (12-7)) | 
	    (hs1 << 16) |
	    (hs2 << 24)
	);

    }
}

void SavageSetColorNew( ScrnInfoPtr pScrn )
{
    SavagePtr psav = SAVPTR(pScrn);
    SavagePortPrivPtr pPriv = psav->adaptor->pPortPrivates[0].ptr;

    /* Brightness/contrast/saturation/hue computations. */

    double k, dk1, dk2, dk3, dk4, dk5, dk6, dk7, dkb;
    int k1, k2, k3, k4, k5, k6, k7, kb;
    double s = pPriv->saturation / 128.0;
    double h = pPriv->hue * 0.017453292;
    unsigned long assembly;

    xf86ErrorFVerb(XVTRACE, "bright %d, contrast %d, saturation %d, hue %d\n",
	pPriv->brightness, pPriv->contrast, pPriv->saturation, pPriv->hue );

    if( psav->videoFourCC == FOURCC_Y211 )
	k = 1.0;	/* YUV */
    else
	k = 1.14;	/* YCrCb */

    /*
     * The S3 documentation must be wrong for k4 and k5.  Their default
     * values, which they hardcode in their Windows driver, have the
     * opposite sign from the results in the register spec.
     */

    dk1 = k * pPriv->contrast;
    dk2 = 64.0 * 1.371 * k * s * cos(h);
    dk3 = -64.0 * 1.371 * k * s * sin(h);
    dk4 = -128.0 * k * s * (0.698 * cos(h) - 0.336 * sin(h));
    dk5 = -128.0 * k * s * (0.698 * sin(h) + 0.336 * cos(h));
    dk6 = 64.0 * 1.732 * k * s * sin(h);	/* == k3 / 1.26331, right? */
    dk7 = 64.0 * 1.732 * k * s * cos(h);	/* == k2 / -1.26331, right? */
    dkb = 128.0 * pPriv->brightness + 64.0;
    if( psav->videoFourCC != FOURCC_Y211 )
	dkb -= dk1 * 14.0;

    k1 = (int)(dk1+0.5) & 0x1ff;
    k2 = (int)(dk2+0.5) & 0x1ff;
    k3 = (int)(dk3+0.5) & 0x1ff;
    assembly = (k3<<18) | (k2<<9) | k1;
    xf86ErrorFVerb(XVTRACE+1, "CC1 = %08x  ", assembly );
    OUTREG( SEC_STREAM_COLOR_CONVERT1, assembly );

    k4 = (int)(dk4+0.5) & 0x1ff;
    k5 = (int)(dk5+0.5) & 0x1ff;
    k6 = (int)(dk6+0.5) & 0x1ff;
    assembly = (k6<<18) | (k5<<9) | k4;
    xf86ErrorFVerb(XVTRACE+1, "CC2 = %08x  ", assembly );
    OUTREG( SEC_STREAM_COLOR_CONVERT2, assembly );

    k7 = (int)(dk7+0.5) & 0x1ff;
    kb = (int)(dkb+0.5) & 0xffff;
    assembly = (kb<<9) | k7;
    xf86ErrorFVerb(XVTRACE+1, "CC3 = %08x\n", assembly );
    OUTREG( SEC_STREAM_COLOR_CONVERT3, assembly );
}


void SavageResetVideo(ScrnInfoPtr pScrn) 
{
    xf86ErrorFVerb(XVTRACE,"SavageResetVideo\n");
    SavageSetColor( pScrn );
    SavageSetColorKey( pScrn );
}


static XF86VideoAdaptorPtr 
SavageSetupImageVideo(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    SavagePtr psav = SAVPTR(pScrn);
    XF86VideoAdaptorPtr adapt;
    SavagePortPrivPtr pPriv;

    xf86ErrorFVerb(XVTRACE,"SavageSetupImageVideo\n");

    if(!(adapt = xcalloc(1, sizeof(XF86VideoAdaptorRec) +
			    sizeof(SavagePortPrivRec) +
			    sizeof(DevUnion))))
	return NULL;

    adapt->type		= XvWindowMask | XvInputMask | XvImageMask;
    adapt->flags	= VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT;
    adapt->name			= "Savage Streams Engine";
    adapt->nEncodings 		= 1;
    adapt->pEncodings 		= DummyEncoding;
    adapt->nFormats 		= NUM_FORMATS;
    adapt->pFormats 		= Formats;
    adapt->nPorts 		= 1;
    adapt->pPortPrivates = (DevUnion*)(&adapt[1]);
    pPriv = (SavagePortPrivPtr)(&adapt->pPortPrivates[1]);
    adapt->pPortPrivates[0].ptr	= (pointer)(pPriv);
    adapt->pAttributes		= Attributes;
    adapt->nImages		= NUM_IMAGES;
    adapt->nAttributes		= NUM_ATTRIBUTES;
    adapt->pImages		= Images;
    adapt->PutVideo		= NULL;
    adapt->PutStill		= NULL;
    adapt->GetVideo		= NULL;
    adapt->GetStill		= NULL;
    adapt->StopVideo		= SavageStopVideo;
    adapt->SetPortAttribute	= SavageSetPortAttribute;
    adapt->GetPortAttribute	= SavageGetPortAttribute;
    adapt->QueryBestSize	= SavageQueryBestSize;
    adapt->PutImage		= SavagePutImage;
    adapt->QueryImageAttributes	= SavageQueryImageAttributes;

    xvBrightness = MAKE_ATOM("XV_BRIGHTNESS");
    xvContrast   = MAKE_ATOM("XV_CONTRAST");
    xvColorKey   = MAKE_ATOM("XV_COLORKEY");
    xvHue        = MAKE_ATOM("XV_HUE");
    xvSaturation = MAKE_ATOM("XV_SATURATION");

    pPriv->colorKey = 
      (1 << pScrn->offset.red) | 
      (1 << pScrn->offset.green) |
      (((pScrn->mask.blue >> pScrn->offset.blue) - 1) << pScrn->offset.blue); 
    pPriv->videoStatus = 0;
    pPriv->brightness = 0;
    pPriv->contrast = 128;
    pPriv->saturation = 128;
#if 0
    /* 
     * The S3 driver has these values for some of the chips.  I have yet
     * to find any Savage where these make sense.
     */
    pPriv->brightness = 64;
    pPriv->contrast = 16;
    pPriv->saturation = 128;
#endif
    pPriv->hue = 0;
    pPriv->lastKnownPitch = 0;

    /* gotta uninit this someplace */
    REGION_INIT(pScreen, &pPriv->clip, NullBox, 0); 

    psav->adaptor = adapt;

#if 0
    psav->BlockHandler = pScreen->BlockHandler;
    pScreen->BlockHandler = SavageBlockHandler;
#endif

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


/* SavageClipVideo -  

   Takes the dst box in standard X BoxRec form (top and left
   edges inclusive, bottom and right exclusive).  The new dst
   box is returned.  The source boundaries are given (x1, y1 
   inclusive, x2, y2 exclusive) and returned are the new source 
   boundaries in 16.16 fixed point.
*/

static void
SavageClipVideo(
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
SavageStopVideo(ScrnInfoPtr pScrn, pointer data, Bool shutdown)
{
    SavagePortPrivPtr pPriv = (SavagePortPrivPtr)data;
    /*SavagePtr psav = SAVPTR(pScrn); */

    xf86ErrorFVerb(XVTRACE,"SavageStopVideo\n");

    REGION_EMPTY(pScrn->pScreen, &pPriv->clip);   

    SavageStreamsOff( pScrn );

    if(shutdown) {
	if(pPriv->area) {
	    xf86FreeOffscreenArea(pPriv->area);
	    pPriv->area = NULL;
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
SavageSetPortAttribute(
    ScrnInfoPtr pScrn, 
    Atom attribute,
    INT32 value, 
    pointer data
){
    SavagePortPrivPtr pPriv = (SavagePortPrivPtr)data;
    SavagePtr psav = SAVPTR(pScrn);

    if(attribute == xvColorKey) {
	pPriv->colorKey = value;
	if( psav->videoFlags & VF_STREAMS_ON)
	    SavageSetColorKey( pScrn );
	REGION_EMPTY(pScrn->pScreen, &pPriv->clip);   
    } 
    else if( attribute == xvBrightness) {
	if((value < -128) || (value > 127))
	    return BadValue;
	pPriv->brightness = value;
	if( psav->videoFlags & VF_STREAMS_ON)
	    SavageSetColor( pScrn );
    }
    else if( attribute == xvContrast) {
	if((value < 0) || (value > 255))
	    return BadValue;
	pPriv->contrast = value;
	if( psav->videoFlags & VF_STREAMS_ON)
	    SavageSetColor( pScrn );
    }
    else if( attribute == xvSaturation) {
	if((value < 0) || (value > 255))
	    return BadValue;
	pPriv->saturation = value;
	if( psav->videoFlags & VF_STREAMS_ON)
	    SavageSetColor( pScrn );
    }
    else if( attribute == xvHue) {
	if((value < -180) || (value > 180))
	    return BadValue;
	pPriv->hue = value;
	if( psav->videoFlags & VF_STREAMS_ON)
	    SavageSetColor( pScrn );
    }
    else
	return BadMatch;

    return Success;
}


static int 
SavageGetPortAttribute(
  ScrnInfoPtr pScrn, 
  Atom attribute,
  INT32 *value, 
  pointer data
){
    SavagePortPrivPtr pPriv = (SavagePortPrivPtr)data;

    if(attribute == xvColorKey) {
	*value = pPriv->colorKey;
    }
    else if( attribute == xvBrightness ) {
	*value = pPriv->brightness;
    }
    else if( attribute == xvContrast ) {
	*value = pPriv->contrast;
    }
    else if( attribute == xvHue ) {
	*value = pPriv->hue;
    }
    else if( attribute == xvSaturation ) {
	*value = pPriv->saturation;
    }
    else return BadMatch;

    return Success;
}

static void 
SavageQueryBestSize(
  ScrnInfoPtr pScrn, 
  Bool motion,
  short vid_w, short vid_h, 
  short drw_w, short drw_h, 
  unsigned int *p_w, unsigned int *p_h, 
  pointer data
){
    /* What are the real limits for the Savage? */

    *p_w = drw_w;
    *p_h = drw_h; 

    if(*p_w > 16384) *p_w = 16384;
}


static void
SavageCopyData(
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
SavageCopyPlanarData(
   unsigned char *src1, /* Y */
   unsigned char *src2, /* V */
   unsigned char *src3, /* U */
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
/* Shouldn't this be 'if LITTLEENDIAN'? */
#if 1
	    dst[i] = src1[i << 1] | (src1[(i << 1) + 1] << 16) |
		     (src3[i] << 8) | (src2[i] << 24);
#else
	    dst[i] = (src1[i << 1] << 24) | (src1[(i << 1) + 1] << 8) |
		     (src3[i] << 0) | (src2[i] << 16);
#endif
	}
	dst += dstPitch;
	src1 += srcPitch;
	if(j & 1) {
	    src2 += srcPitch2;
	    src3 += srcPitch2;
	}
   }
}

static FBAreaPtr
SavageAllocateMemory(
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

    xf86PurgeUnlockedOffscreenAreas(pScreen);
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
SavageDisplayVideoOld(
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
    SavagePtr psav = SAVPTR(pScrn);
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    SavagePortPrivPtr pPriv = psav->adaptor->pPortPrivates[0].ptr;
    /*DisplayModePtr mode = pScrn->currentMode;*/
    int vgaCRIndex, vgaCRReg, vgaIOBase;
    unsigned int ssControl;


    vgaIOBase = hwp->IOBase;
    vgaCRIndex = vgaIOBase + 4;
    vgaCRReg = vgaIOBase + 5;

    if( psav->videoFourCC != id )
	SavageStreamsOff(pScrn);

    if( !psav->videoFlags & VF_STREAMS_ON )
    {
	SavageStreamsOn(pScrn, id);
	SavageResetVideo(pScrn);
    }

    /* Set surface format. */

    OUTREG(SSTREAM_CONTROL_REG, 
	(GetBlendForFourCC(psav->videoFourCC) << 24) + src_w );

    /* Calculate horizontal scale factor. */

    OUTREG(SSTREAM_STRETCH_REG, (src_w << 15) / drw_w );

    /* Calculate vertical scale factor. */

    OUTREG(SSTREAM_LINES_REG, src_h );
    OUTREG(SSTREAM_VINITIAL_REG, 0 );
    OUTREG(SSTREAM_VSCALE_REG, (src_h << 15) / drw_h );

    /* Set surface location and stride. */

    OUTREG(SSTREAM_FBADDR0_REG, (offset + (x1>>15)) & 0x3ffff0 );
    OUTREG(SSTREAM_FBADDR1_REG, 0 );
    
    OUTREG(SSTREAM_STRIDE_REG, pitch & 0xfff );

    OUTREG(SSTREAM_WINDOW_START_REG, OS_XY(dstBox->x1, dstBox->y1) );
    OUTREG(SSTREAM_WINDOW_SIZE_REG, OS_WH(drw_w, drw_h) );

    ssControl = 0;

    if( src_w > (drw_w << 1) )
    {
	/* BUGBUG shouldn't this be >=?  */
	if( src_w <= (drw_w << 2) )
	    ssControl |= HDSCALE_4;
	else if( src_w > (drw_w << 3) )
	    ssControl |= HDSCALE_8;
	else if( src_w > (drw_w << 4) )
	    ssControl |= HDSCALE_16;
	else if( src_w > (drw_w << 5) )
	    ssControl |= HDSCALE_32;
	else if( src_w > (drw_w << 6) )
	    ssControl |= HDSCALE_64;
    }

    ssControl |= src_w;
    ssControl |= (1 << 24);
    OUTREG(SSTREAM_CONTROL_REG, ssControl);

    /* Set color key on primary. */

    SavageSetColorKey( pScrn );

    /* Set FIFO L2 on second stream. */

    if( pPriv->lastKnownPitch != pitch )
    {
	unsigned char cr92;

	pPriv->lastKnownPitch = pitch;

	pitch = (pitch + 7) / 8;
	VGAOUT8(vgaCRIndex, 0x92);
	cr92 = VGAIN8(vgaCRReg);
	VGAOUT8(vgaCRReg, (cr92 & 0x40) | (pitch >> 8) | 0x80);
	VGAOUT8(vgaCRIndex, 0x93);
	VGAOUT8(vgaCRReg, pitch);
    }

}

static void
SavageDisplayVideoNew(
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
    SavagePtr psav = SAVPTR(pScrn);
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    /*DisplayModePtr mode = pScrn->currentMode;*/
    SavagePortPrivPtr pPriv = psav->adaptor->pPortPrivates[0].ptr;
    int vgaCRIndex, vgaCRReg, vgaIOBase;


    vgaIOBase = hwp->IOBase;
    vgaCRIndex = vgaIOBase + 4;
    vgaCRReg = vgaIOBase + 5;

    if( psav->videoFourCC != id )
	SavageStreamsOff(pScrn);

    if( !psav->videoFlags & VF_STREAMS_ON )
    {
	SavageStreamsOn(pScrn, id);
	SavageResetVideo(pScrn);
    }

    /* Calculate horizontal and vertical scale factors. */

    if( psav->Chipset == S3_SAVAGE2000 )
    {
	OUTREG(SEC_STREAM_HSCALING, 
	    (65536 * src_w / drw_w) & 0x1FFFFF );
	if( src_w < drw_w )
	    OUTREG(SEC_STREAM_HSCALE_NORMALIZE,
		((2048 * src_w / drw_w) & 0x7ff) << 16 );
	else
	    OUTREG(SEC_STREAM_HSCALE_NORMALIZE, 2048 << 16 );
	OUTREG(SEC_STREAM_VSCALING, 
	    (65536 * src_h / drw_h) & 0x1FFFFF );
    }
    else
    {
	if( 
	    S3_SAVAGE_MOBILE_SERIES(psav->Chipset) &&
	    !psav->CrtOnly &&
	    !psav->TvOn
	) {
	    drw_w = (float)(drw_w * psav->XExp1)/(float)psav->XExp2 + 1;
	    drw_h = (float)(drw_h * psav->YExp1)/(float)psav->YExp2 + 1;
	    dstBox->x1 = (float)(dstBox->x1 * psav->XExp1)/(float)psav->XExp2;
	    dstBox->y1 = (float)(dstBox->y1 * psav->YExp1)/(float)psav->YExp2;
	    dstBox->x1 += psav->displayXoffset;
	    dstBox->y1 += psav->displayYoffset;
	}

	OUTREG(SEC_STREAM_HSCALING, 
	    ((src_w&0xfff)<<20) | ((65536 * src_w / drw_w) & 0x1FFFF ));
	/* BUGBUG need to add 00040000 if src stride > 2048 */
	OUTREG(SEC_STREAM_VSCALING, 
	    ((src_h&0xfff)<<20) | ((65536 * src_h / drw_h) & 0x1FFFF ));
    }

    /*
     * Set surface location and stride.  We use x1>>15 because all surfaces
     * are 2 bytes/pixel.
     */

    OUTREG(SEC_STREAM_FBUF_ADDR0, (offset + (x1>>15)) & 0x3ffff0 );
    OUTREG(SEC_STREAM_STRIDE, pitch & 0xfff );
    OUTREG(SEC_STREAM_WINDOW_START, ((dstBox->x1+1) << 16) | (dstBox->y1+1) );
    OUTREG(SEC_STREAM_WINDOW_SZ, ((drw_w) << 16) | drw_h );

    /* Set color key on primary. */

    SavageSetColorKey( pScrn );

    /* Set FIFO L2 on second stream. */

    if( pPriv->lastKnownPitch != pitch )
    {
	unsigned char cr92;

	pPriv->lastKnownPitch = pitch;
	pitch = (pitch + 7) / 8 - 4;
	VGAOUT8(vgaCRIndex, 0x92);
	cr92 = VGAIN8(vgaCRReg);
	VGAOUT8(vgaCRReg, (cr92 & 0x40) | (pitch >> 8) | 0x80);
	VGAOUT8(vgaCRIndex, 0x93);
	VGAOUT8(vgaCRReg, pitch);
    }
}

static int 
SavagePutImage( 
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
    SavagePortPrivPtr pPriv = (SavagePortPrivPtr)data;
    SavagePtr psav = SAVPTR(pScrn);
    INT32 x1, x2, y1, y2;
    unsigned char *dst_start;
    int pitch, new_h, offset, offsetV=0, offsetU=0;
    int srcPitch, srcPitch2=0, dstPitch;
    int top, left, npixels, nlines;
    BoxRec dstBox;
    CARD32 tmp;

    if(drw_w > 16384) drw_w = 16384;

    /* Clip */
    x1 = src_x;
    x2 = src_x + src_w;
    y1 = src_y;
    y2 = src_y + src_h;

    dstBox.x1 = drw_x;
    dstBox.x2 = drw_x + drw_w;
    dstBox.y1 = drw_y;
    dstBox.y2 = drw_y + drw_h;

    SavageClipVideo(&dstBox, &x1, &x2, &y1, &y2, 
		REGION_EXTENTS(pScreen, clipBoxes), width, height);

    drw_w = dstBox.x2 - dstBox.x1;
    drw_h = dstBox.y2 - dstBox.y1;
    src_w = ( x2 - x1 ) >> 16;
    src_h = ( y2 - y1 ) >> 16;

    if((x1 >= x2) || (y1 >= y2))
	return Success;

    dstBox.x1 -= pScrn->frameX0;
    dstBox.x2 -= pScrn->frameX0;
    dstBox.y1 -= pScrn->frameY0;
    dstBox.y2 -= pScrn->frameY0;

    pitch = pScrn->bitsPerPixel * pScrn->displayWidth >> 3;

    dstPitch = ((width << 1) + 15) & ~15;
    new_h = ((dstPitch * height) + pitch - 1) / pitch;

    switch(id) {
    case FOURCC_Y211:		/* Y211 */
        srcPitch = width;
	break;
    case FOURCC_YV12:		/* YV12 */
	srcPitch = (width + 3) & ~3;
	offsetV = srcPitch * height;
	srcPitch2 = ((width >> 1) + 3) & ~3;
	offsetU = (srcPitch2 * (height >> 1)) + offsetV;
	break;
    case FOURCC_I420:
	srcPitch = (width + 3) & ~3;
	offsetU = srcPitch * height;
	srcPitch2 = ((width >> 1) + 3) & ~3;
	offsetV = (srcPitch2 * (height >> 1)) + offsetU;
	break;
    case FOURCC_RV15:		/* RGB15 */
    case FOURCC_RV16:		/* RGB16 */
    case FOURCC_YUY2:		/* YUY2 */
    default:
	srcPitch = (width << 1);
	break;
    }  

    if(!(pPriv->area = SavageAllocateMemory(pScrn, pPriv->area, new_h)))
	return BadAlloc;

    /* copy data */
    top = y1 >> 16;
    left = (x1 >> 16) & ~1;
    npixels = ((((x2 + 0xffff) >> 16) + 1) & ~1) - left;
    left <<= 1;

    offset = (pPriv->area->box.y1 * pitch) + (top * dstPitch);
    dst_start = psav->FBBase + offset + left;

    switch(id) {
    case FOURCC_YV12:		/* YV12 */
    case FOURCC_I420:
	top &= ~1;
	tmp = ((top >> 1) * srcPitch2) + (left >> 2);
	offsetU += tmp;
	offsetV += tmp; 
	nlines = ((((y2 + 0xffff) >> 16) + 1) & ~1) - top;
	SavageCopyPlanarData(
	    buf + (top * srcPitch) + (left >> 1), 
	    buf + offsetV, 
	    buf + offsetU, 
	    dst_start, srcPitch, srcPitch2, dstPitch, nlines, npixels);
	break;
    case FOURCC_Y211:		/* Y211 */
    case FOURCC_RV15:		/* RGB15 */
    case FOURCC_RV16:		/* RGB16 */
    case FOURCC_YUY2:		/* YUY2 */
    default:
	buf += (top * srcPitch) + left;
	nlines = ((y2 + 0xffff) >> 16) - top;
	SavageCopyData(buf, dst_start, srcPitch, dstPitch, nlines, npixels);
	break;
    }  

    /* update cliplist */
    if(!RegionsEqual(&pPriv->clip, clipBoxes)) {
	REGION_COPY(pScreen, &pPriv->clip, clipBoxes);
	/* draw these */
	XAAFillSolidRects(pScrn, pPriv->colorKey, GXcopy, ~0, 
					REGION_NUM_RECTS(clipBoxes),
					REGION_RECTS(clipBoxes));
    }
   
    SavageDisplayVideo(pScrn, id, offset, width, height, dstPitch,
	     x1, y1, x2, y2, &dstBox, src_w, src_h, drw_w, drw_h);

    pPriv->videoStatus = CLIENT_VIDEO_ON;

    return Success;
}

static int 
SavageQueryImageAttributes(
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
    case FOURCC_Y211:
	size = *w << 2;
	if(pitches) pitches[0] = size;
	size *= *h;
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
    case FOURCC_RV15:		/* RGB15 */
    case FOURCC_RV16:		/* RGB16 */
    case FOURCC_YUY2:
    default:
	size = *w << 1;
	if(pitches) pitches[0] = size;
	size *= *h;
	break;
    }

    return size;
}

#if 0

static void
CHIPSBlockHandler (
    int i,
    pointer     blockData,
    pointer     pTimeout,
    pointer     pReadmask
){
    ScreenPtr   pScreen = screenInfo.screens[i];
    ScrnInfoPtr pScrn = xf86Screens[i];
    CHIPSPtr    cPtr = CHIPSPTR(pScrn);
    CHIPSPortPrivPtr pPriv = GET_PORT_PRIVATE(pScrn);
    unsigned char mr3c;
    
    pScreen->BlockHandler = cPtr->BlockHandler;
    
    (*pScreen->BlockHandler) (i, blockData, pTimeout, pReadmask);

    pScreen->BlockHandler = CHIPSBlockHandler;

    CHIPSHiQVSync(pScrn);
    if(pPriv->videoStatus & TIMER_MASK) {
	UpdateCurrentTime();
	if(pPriv->videoStatus & OFF_TIMER) {
	    if(pPriv->offTime < currentTime.milliseconds) {
		mr3c = cPtr->readMR(cPtr, 0x3C);
		cPtr->writeMR(cPtr, 0x3C, (mr3c & 0xFE));
		pPriv->videoStatus = FREE_TIMER;
		pPriv->freeTime = currentTime.milliseconds + FREE_DELAY;
	    }
	} else {  /* FREE_TIMER */
	    if(pPriv->freeTime < currentTime.milliseconds) {
		if(pPriv->area) {
		   xf86FreeOffscreenArea(pPriv->area);
		   pPriv->area = NULL;
		}
		pPriv->videoStatus = 0;
	    }
        }
    }
}

#endif

/****************** Offscreen stuff ***************/

typedef struct {
  FBAreaPtr area;
  Bool isOn;
} OffscreenPrivRec, * OffscreenPrivPtr;

static int 
SavageAllocateSurface(
    ScrnInfoPtr pScrn,
    int id,
    unsigned short w, 	
    unsigned short h,
    XF86SurfacePtr surface
){
    FBAreaPtr area;
    int pitch, fbpitch, numlines;
    OffscreenPrivPtr pPriv;

    if((w > 1024) || (h > 1024))
	return BadAlloc;

    w = (w + 1) & ~1;
    pitch = ((w << 1) + 15) & ~15;
    fbpitch = pScrn->bitsPerPixel * pScrn->displayWidth >> 3;
    numlines = ((pitch * h) + fbpitch - 1) / fbpitch;

    if(!(area = SavageAllocateMemory(pScrn, NULL, numlines)))
	return BadAlloc;

    surface->width = w;
    surface->height = h;

    if(!(surface->pitches = xalloc(sizeof(int))))
	return BadAlloc;
    if(!(surface->offsets = xalloc(sizeof(int)))) {
	xfree(surface->pitches);
	return BadAlloc;
    }
    if(!(pPriv = xalloc(sizeof(OffscreenPrivRec)))) {
	xfree(surface->pitches);
	xfree(surface->offsets);
	return BadAlloc;
    }

    pPriv->area = area;
    pPriv->isOn = FALSE;

    surface->pScrn = pScrn;
    surface->id = id;   
    surface->pitches[0] = pitch;
    surface->offsets[0] = area->box.y1 * fbpitch;
    surface->devPrivate.ptr = (pointer)pPriv;

    return Success;
}

static int 
SavageStopSurface(
    XF86SurfacePtr surface
){
    OffscreenPrivPtr pPriv = (OffscreenPrivPtr)surface->devPrivate.ptr;

    if(pPriv->isOn) {
	/*SavagePtr psav = SAVPTR(surface->pScrn);*/
	SavageStreamsOff( surface->pScrn );
	pPriv->isOn = FALSE;
    }

    return Success;
}


static int 
SavageFreeSurface(
    XF86SurfacePtr surface
){
    OffscreenPrivPtr pPriv = (OffscreenPrivPtr)surface->devPrivate.ptr;

    if(pPriv->isOn)
	SavageStopSurface(surface);
    xf86FreeOffscreenArea(pPriv->area);
    xfree(surface->pitches);
    xfree(surface->offsets);
    xfree(surface->devPrivate.ptr);

    return Success;
}

static int
SavageGetSurfaceAttribute(
    ScrnInfoPtr pScrn,
    Atom attribute,
    INT32 *value
){
    return SavageGetPortAttribute(pScrn, attribute, value, 
			(pointer)(GET_PORT_PRIVATE(pScrn)));
}

static int
SavageSetSurfaceAttribute(
    ScrnInfoPtr pScrn,
    Atom attribute,
    INT32 value
){
    return SavageSetPortAttribute(pScrn, attribute, value, 
			(pointer)(GET_PORT_PRIVATE(pScrn)));
}


static int 
SavageDisplaySurface(
    XF86SurfacePtr surface,
    short src_x, short src_y, 
    short drw_x, short drw_y,
    short src_w, short src_h, 
    short drw_w, short drw_h,
    RegionPtr clipBoxes
){
    OffscreenPrivPtr pPriv = (OffscreenPrivPtr)surface->devPrivate.ptr;
    ScrnInfoPtr pScrn = surface->pScrn;
    SavagePortPrivPtr portPriv = GET_PORT_PRIVATE(pScrn);
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

    SavageClipVideo(&dstBox, &x1, &x2, &y1, &y2, 
                	REGION_EXTENTS(pScreen, clipBoxes), 
			surface->width, surface->height);

    if((x1 >= x2) || (y1 >= y2))
	return Success;

    dstBox.x1 -= pScrn->frameX0;
    dstBox.x2 -= pScrn->frameX0;
    dstBox.y1 -= pScrn->frameY0;
    dstBox.y2 -= pScrn->frameY0;

    XAAFillSolidRects(pScrn, portPriv->colorKey, GXcopy, ~0, 
                                        REGION_NUM_RECTS(clipBoxes),
                                        REGION_RECTS(clipBoxes));

    SavageDisplayVideo(pScrn, surface->id, surface->offsets[0], 
	     surface->width, surface->height, surface->pitches[0],
	     x1, y1, x2, y2, &dstBox, src_w, src_h, drw_w, drw_h);

    pPriv->isOn = TRUE;
#if 0
    if(portPriv->videoStatus & CLIENT_VIDEO_ON) {
	REGION_EMPTY(pScrn->pScreen, &portPriv->clip);   
	UpdateCurrentTime();
	portPriv->videoStatus = FREE_TIMER;
	portPriv->freeTime = currentTime.milliseconds + FREE_DELAY;
    }
#endif

    return Success;
}


static void 
SavageInitOffscreenImages(ScreenPtr pScreen)
{
    XF86OffscreenImagePtr offscreenImages;
    SavagePtr psav = SAVPTR(xf86Screens[pScreen->myNum]);

    /* need to free this someplace */
    if (!psav->offscreenImages) {
	if(!(offscreenImages = xalloc(sizeof(XF86OffscreenImageRec))))
	    return;
	psav->offscreenImages = offscreenImages;
    } else {
	offscreenImages = psav->offscreenImages;
    }

    offscreenImages[0].image = &Images[0];
    offscreenImages[0].flags = VIDEO_OVERLAID_IMAGES | 
			       VIDEO_CLIP_TO_VIEWPORT;
    offscreenImages[0].alloc_surface = SavageAllocateSurface;
    offscreenImages[0].free_surface = SavageFreeSurface;
    offscreenImages[0].display = SavageDisplaySurface;
    offscreenImages[0].stop = SavageStopSurface;
    offscreenImages[0].setAttribute = SavageSetSurfaceAttribute;
    offscreenImages[0].getAttribute = SavageGetSurfaceAttribute;
    offscreenImages[0].max_width = 1024;
    offscreenImages[0].max_height = 1024;
    offscreenImages[0].num_attributes = NUM_ATTRIBUTES;
    offscreenImages[0].attributes = Attributes;
    
    xf86XVRegisterOffscreenImages(pScreen, offscreenImages, 1);
}

/* Function to get lcd factor, display offset for overlay use
 * Input: pScrn; Output: x,yfactor, displayoffset in pScrn
 */
static void OverlayParamInit(ScrnInfoPtr pScrn)
{
    SavagePtr psav = SAVPTR(pScrn);

    psav = SAVPTR(pScrn);
    psav->cxScreen = psav->iResX;
    InitStreamsForExpansion(psav);
}

/* Function to calculate lcd expansion x,yfactor and offset for overlay
 */
static void InitStreamsForExpansion(SavagePtr psav)
{
    int		PanelSizeX,PanelSizeY;
    int		ViewPortWidth,ViewPortHeight;
    int		XFactor, YFactor;

    PanelSizeX = psav->PanelX;
    PanelSizeY = psav->PanelY;
    ViewPortWidth = psav->iResX;
    ViewPortHeight = psav->iResY;
    if( PanelSizeX == 1408 )
	PanelSizeX = 1400;
    psav->XExpansion = 0x00010001;
    psav->YExpansion = 0x00010001;
    psav->displayXoffset = 0;
    psav->displayYoffset = 0;

    VGAOUT8(0x3C4, HZEXP_FACTOR_IGA1);
    XFactor = VGAIN8(0x3C5) >> 4;
    VGAOUT8(0x3C4, VTEXP_FACTOR_IGA1);
    YFactor = VGAIN8(0x3C5) >> 4;

    switch( XFactor )
    {
	case 1:
	    psav->XExpansion = 0x00010001;
	    psav->displayXoffset = 
		(((PanelSizeX - ViewPortWidth) / 2) + 0x7) & 0xFFF8;
	    break;

	case 3:
	    psav->XExpansion = 0x00090008;
	    psav->displayXoffset = 
		(((PanelSizeX - ((9 * ViewPortWidth)/8)) / 2) + 0x7) & 0xFFF8;
	    break;

	case 4:
	    psav->XExpansion = 0x00050004;

	    if ((psav->cxScreen == 800) && (PanelSizeX !=1400))
	    {
		psav->displayXoffset = 
		    (((PanelSizeX - ((5 * ViewPortWidth)/4)) / 2) ) & 0xFFF8; 
	    }
	    else
	    {
		psav->displayXoffset = 
		    (((PanelSizeX - ((5 * ViewPortWidth)/4)) / 2) +0x7) & 0xFFF8;
	    }
	    break;

	case 6:
	    psav->XExpansion = 0x00030002;
	    psav->displayXoffset = 
		(((PanelSizeX - ((3 * ViewPortWidth)/2)) / 2) + 0x7) & 0xFFF8;
	    break;

	case 7:
	    psav->XExpansion = 0x00020001;
	    psav->displayXoffset = 
		(((PanelSizeX - (2 * ViewPortWidth)) / 2) + 0x7) & 0xFFF8;
	    break;
    }
	
    switch( YFactor )
    {
	case 0:
	    psav->YExpansion = 0x00010001;
	    psav->displayYoffset = (PanelSizeY - ViewPortHeight) / 2;
	    break;
	case 1:
	    psav->YExpansion = 0x00010001;
	    psav->displayYoffset = (PanelSizeY - ViewPortHeight) / 2;
	    break;
	case 2:
	    psav->YExpansion = 0x00040003;
	    psav->displayYoffset = (PanelSizeY - ((4 * ViewPortHeight)/3)) / 2;
	    break;
	case 4:
	    psav->YExpansion = 0x00050004;
	    psav->displayYoffset = (PanelSizeY - ((5 * ViewPortHeight)/4)) / 2;
	    break;
	case 5:
	    psav->YExpansion = 0x00040003;

	    if((psav->cxScreen == 1024)&&(PanelSizeX ==1400))
	    {
		psav->displayYoffset = 
		    ((PanelSizeY - ((4 * ViewPortHeight)/3)) / 2) - 0x1 ;
	    }
	    else
	    {
		psav->displayYoffset = (PanelSizeY - ((4 * ViewPortHeight)/3)) / 2;
	    }
	    break;
	case 6:
	    psav->YExpansion = 0x00050004;
	    psav->displayYoffset = (PanelSizeY - ((5 * ViewPortHeight)/4)) / 2;
	    break;
	case 7:
	    psav->YExpansion = 0x00030002;
	    psav->displayYoffset = (PanelSizeY - ((3 * ViewPortHeight)/2)) / 2;
	    break;
	case 8:
	    psav->YExpansion = 0x00020001;
	    psav->displayYoffset = (PanelSizeY - (2 * ViewPortHeight)) /2;
	    break;
	case 9:
	    psav->YExpansion = 0x00090004;
	    psav->displayYoffset = (PanelSizeY - ((9 * ViewPortHeight)/4)) /2;
	    break;
	case 11:
	    psav->YExpansion = 0x00110005;
	    psav->displayYoffset = (PanelSizeY - ((11 * ViewPortHeight)/5)) /2;
	    break;
	case 12:
	    psav->YExpansion = 0x00070003;
	    psav->displayYoffset = (PanelSizeY - ((7 * ViewPortHeight)/3)) /2;
	    break;
	case 14:
	    psav->YExpansion = 0x00050002;
	    psav->displayYoffset = (PanelSizeY - ((5 * ViewPortHeight)/2)) /2;
	    break;
	case 15:
	    psav->YExpansion = 0x00040001;
	    psav->displayYoffset = (PanelSizeY - (4 * ViewPortHeight)) /2;
	    break;
    }
    psav->XExp1 = psav->XExpansion >> 16;
    psav->XExp2 = psav->XExpansion & 0xFFFF;
    psav->YExp1 = psav->YExpansion >> 16;
    psav->YExp2 = psav->YExpansion & 0xFFFF;
}  /* InitStreamsForExpansionPM */

#endif /* XvExtension */
