/* Header:   //Mercury/Projects/archives/XFree86/4.0/smi_video.c.-arc   1.14   30 Nov 2000 16:51:40   Frido  $ */
/*
Copyright (C) 1994-1999 The XFree86 Project, Inc.  All Rights Reserved.
Copyright (C) 2000 Silicon Motion, Inc.  All Rights Reserved.
Copyright (C) 2001 Corvin Zahn.  All Rights Reserved.

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

Except as contained in this notice, the names of the XFree86 Project and
Silicon Motion shall not be used in advertising or otherwise to promote the
sale, use or other dealings in this Software without prior written
authorization from the XFree86 Project and silicon Motion.
*/

/*
this is a heavy modified version of the V1.2.2 original siliconmotion driver.
- SAA7111 support
- supports attributes: XV_ENCODING, XV_BRIGHTNESS, XV_CONTRAST,
  XV_SATURATION, XV_HUE, XV_COLORKEY, XV_INTERLACED
  XV_CAPTURE_BRIGHTNESS can be used to set brightness in the capture device
- bug fixes
- tries not to use acceleration functions (if USE_XAA = 0)
- interlaced video for double vertical resolution

Author of changes: Corvin Zahn <zahn@zac.de>
Date:   2.11.2001
*/

/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/siliconmotion/smi_video.c,v 1.9 2003/01/12 03:55:49 tsi Exp $ */

#include "smi.h"
#include "smi_video.h"


/*

new attribute:

XV_INTERLACED = 0: only one field of an interlaced video signal is displayed:
               -> half vertical resolution, but no comb like artifacts from
	       moving vertical edges
XV_INTERLACED = 1: both fields of an interlaced video signal are displayed:
               -> full vertical resolution, but comb like artifacts from
	       moving vertical edges

The default value can be set with the driver option Interlaced

*/




#undef MIN
#undef ABS
#undef CLAMP
#undef ENTRIES

#define MIN(a, b) (((a) < (b)) ? (a) : (b)) 
#define ABS(n) (((n) < 0) ? -(n) : (n))
#define CLAMP(v, min, max) (((v) < (min)) ? (min) : MIN(v, max))

#define ENTRIES(array) (sizeof(array) / sizeof((array)[0]))
#define nElems(x)		(sizeof(x) / sizeof(x[0]))

#define MAKE_ATOM(a)	MakeAtom(a, sizeof(a) - 1, TRUE)

#if defined(XvExtension) && SMI_USE_VIDEO

/* USE_XAA = 1: use XAA functions for color key rectangle fill,
   USE_XAA = 0: use xf86XVFillKeyHelper for color key rectangle fill,
                needs common/xf86xv.c >= 1.30,
		      common/xf86xv.h >= 1.22,
                      loader/xf86sym.c >= 1.194 */
#define USE_XAA 1

#include "dixstruct.h"
#if USE_XAA
#include "xaa.h"
#include "xaalocal.h"
#endif


static int SMI_AddEncoding(XF86VideoEncodingPtr enc, int i,
			   int norm, int input, int channel);
static void SMI_BuildEncodings(SMI_PortPtr p);

static XF86VideoAdaptorPtr SMI_SetupVideo(ScreenPtr pScreen);
static void SMI_ResetVideo(ScrnInfoPtr pScrn);

#if SMI_USE_CAPTURE
static int SMI_PutVideo(ScrnInfoPtr pScrn,
		short vid_x, short vid_y, short drw_x, short drw_y,
		short vid_w, short vid_h, short drw_w, short drw_h,
		RegionPtr clipBoxes, pointer data);
#endif
static void SMI_StopVideo(ScrnInfoPtr pScrn, pointer data, Bool shutdown);
static int SMI_SetPortAttribute(ScrnInfoPtr pScrn, Atom attribute,
		INT32 value, pointer data);
static int SMI_GetPortAttribute(ScrnInfoPtr pScrn, Atom attribute,
		INT32 *value, pointer data);
static void SMI_QueryBestSize(ScrnInfoPtr pScrn, Bool motion,
		short vid_w, short vid_h, short drw_w, short drw_h,
		unsigned int *p_w, unsigned int *p_h, pointer data);
static int SMI_PutImage(ScrnInfoPtr pScrn,
		short src_x, short src_y, short drw_x, short drw_y,
		short src_w, short src_h, short drw_w, short drw_h,
		int id, unsigned char *buf, short width, short height, Bool sync,
		RegionPtr clipBoxes, pointer data);
static int SMI_QueryImageAttributes(ScrnInfoPtr pScrn,
		int id, unsigned short *width, unsigned short *height,
		int *picthes, int *offsets);

static Bool RegionsEqual(RegionPtr A, RegionPtr B);
static Bool SMI_ClipVideo(ScrnInfoPtr pScrn, BoxPtr dst,
		INT32 *x1, INT32 *y1, INT32 *x2, INT32 *y2,
		RegionPtr reg, INT32 width, INT32 height);
static void SMI_DisplayVideo(ScrnInfoPtr pScrn, int id, int offset,
		short width, short height, int pitch, int x1, int y1, int x2, int y2,
		BoxPtr dstBox, short vid_w, short vid_h, short drw_w, short drw_h);
static void SMI_BlockHandler(int i, pointer blockData, pointer pTimeout,
		pointer pReadMask);
#if 0
static void SMI_WaitForSync(ScrnInfoPtr pScrn);
#endif
/*static int SMI_SendI2C(ScrnInfoPtr pScrn, CARD8 device, char *devName,
        SMI_I2CDataPtr i2cData);*/

static void SMI_InitOffscreenImages(ScreenPtr pScreen);
static FBAreaPtr SMI_AllocateMemory(ScrnInfoPtr pScrn, FBAreaPtr area,
		int numLines);
static void SMI_CopyData(unsigned char *src, unsigned char *dst, int srcPitch,
		int dstPitch, int height, int width);
static void SMI_CopyYV12Data(unsigned char *src1, unsigned char *src2,
		unsigned char *src3, unsigned char *dst, int srcPitch1, int srcPitch2,
		int dstPitch, int height, int width);

static int SMI_AllocSurface(ScrnInfoPtr pScrn,
		int id, unsigned short width, unsigned short height,
		XF86SurfacePtr surface);
static int SMI_FreeSurface(XF86SurfacePtr surface);
static int SMI_DisplaySurface(XF86SurfacePtr surface,
		short vid_x, short vid_y, short drw_x, short drw_y,
		short vid_w, short vid_h, short drw_w, short drw_h,
		RegionPtr clipBoxes);
static int SMI_StopSurface(XF86SurfacePtr surface);
static int SMI_GetSurfaceAttribute(ScrnInfoPtr pScrn, Atom attr, INT32 *value);
static int SMI_SetSurfaceAttribute(ScrnInfoPtr pScrn, Atom attr, INT32 value);

static int SetAttr(ScrnInfoPtr pScrn, int i, int value);
static int SetAttrSAA7110(ScrnInfoPtr pScrn, int i, int value);
static int SetAttrSAA7111(ScrnInfoPtr pScrn, int i, int value);

/**
 * Atoms
 */

static Atom xvColorKey;
static Atom xvEncoding;
static Atom xvBrightness,xvCapBrightness, xvContrast, xvSaturation, xvHue;
static Atom xvInterlaced;


/******************************************************************************\
**																			  **
**                           C A P A B I L I T I E S                          **
**																			  **
\******************************************************************************/


/**************************************************************************/
/* input channels */

#define N_COMPOSITE_CHANNELS 4
#define N_SVIDEO_CHANNELS 2

#define N_VIDEO_INPUTS 2
typedef enum _VideoInput { COMPOSITE, SVIDEO } VideoInput;


/**************************************************************************/
/* video input formats */

typedef struct _VideoInputDataRec {
    char* name;
} VideoInputDataRec;

static VideoInputDataRec VideoInputs[] = {
    { "composite" },
    { "svideo" }
};


/**************************************************************************/
/* video norms */

#define N_VIDEO_NORMS 3
typedef enum _VideoNorm { PAL, NTSC, SECAM } VideoNorm;

typedef struct _VideoNormDataRec {
    char* name;
    unsigned long Wt;
    unsigned long Wa;
    unsigned long Ht;
    unsigned long Ha;
    unsigned long HStart;
    unsigned long VStart;
    XvRationalRec rate;
} VideoNormDataRec;


static VideoNormDataRec VideoNorms[] =
{
    /* PAL-BDGHI */
    {"pal", 864, 704, 625, 576, 16, 16, { 1, 50 }},
    /* NTSC */
    {"ntsc", 858, 704, 525, 480, 21, 8, { 1001, 60000 }},
    /* SECAM (not tested) */
    {"secam", 864, 7040, 625, 576, 31, 16, { 1, 50 }},
};


/**************************************************************************/
/* number of (generated) XV_ENCODING vaulues */
#define N_ENCODINGS ((N_VIDEO_NORMS) * (N_COMPOSITE_CHANNELS + N_SVIDEO_CHANNELS))


/**************************************************************************/

static XF86VideoFormatRec SMI_VideoFormats[] =
{
	{ 15, TrueColor },					/* depth, class				*/
	{ 16, TrueColor },					/* depth, class				*/
	{ 24, TrueColor },					/* depth, class				*/
};


/**************************************************************************/

/**
 * Attributes
 */

#define XV_ENCODING_NAME        "XV_ENCODING"
#define XV_BRIGHTNESS_NAME      "XV_BRIGHTNESS"
#define XV_CAPTURE_BRIGHTNESS_NAME      "XV_CAPTURE_BRIGHTNESS"
#define XV_CONTRAST_NAME        "XV_CONTRAST"
#define XV_SATURATION_NAME      "XV_SATURATION"
#define XV_HUE_NAME             "XV_HUE"
#define XV_COLORKEY_NAME        "XV_COLORKEY"
#define XV_INTERLACED_NAME      "XV_INTERLACED"


/* fixed order! */
static XF86AttributeRec SMI_VideoAttributesSAA711x[N_ATTRS] = {
    {XvSettable | XvGettable,        0, N_ENCODINGS-1, XV_ENCODING_NAME},
    {XvSettable | XvGettable,        0,           255, XV_BRIGHTNESS_NAME},
    {XvSettable | XvGettable,        0,           255, XV_CAPTURE_BRIGHTNESS_NAME},
    {XvSettable | XvGettable,        0,           127, XV_CONTRAST_NAME},
    {XvSettable | XvGettable,        0,           127, XV_SATURATION_NAME},
    {XvSettable | XvGettable,     -128,           127, XV_HUE_NAME},
    {XvSettable | XvGettable, 0x000000,      0xFFFFFF, XV_COLORKEY_NAME},
    {XvSettable | XvGettable,        0,             1, XV_INTERLACED_NAME},
};

static XF86AttributeRec SMI_VideoAttributes[N_ATTRS] = {
    {XvSettable | XvGettable,        0,           255, XV_BRIGHTNESS_NAME},
    {XvSettable | XvGettable, 0x000000,      0xFFFFFF, XV_COLORKEY_NAME},
};


/**************************************************************************/
static XF86ImageRec SMI_VideoImages[] =
{
	XVIMAGE_YUY2,
	XVIMAGE_YV12,
	XVIMAGE_I420,
	{
		FOURCC_RV15,					/* id						*/
		XvRGB,							/* type						*/
		LSBFirst,						/* byte_order				*/
		{ 'R', 'V' ,'1', '5',
		  0x00, '5',  0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00 },		/* guid						*/
		16,								/* bits_per_pixel			*/
		XvPacked,						/* format					*/
		1,								/* num_planes				*/
		15,								/* depth					*/
		0x001F, 0x03E0, 0x7C00,			/* red_mask, green, blue	*/
		0, 0, 0,						/* y_sample_bits, u, v		*/
		0, 0, 0,						/* horz_y_period, u, v		*/
		0, 0, 0,						/* vert_y_period, u, v		*/
		{ 'R', 'V', 'B' },				/* component_order			*/
		XvTopToBottom					/* scaline_order			*/
	},
	{
		FOURCC_RV16,					/* id						*/
		XvRGB,							/* type						*/
		LSBFirst,						/* byte_order				*/
		{ 'R', 'V' ,'1', '6',
		  0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00 },		/* guid						*/
		16,								/* bits_per_pixel			*/
		XvPacked,						/* format					*/
		1,								/* num_planes				*/
		16,								/* depth					*/
		0x001F, 0x07E0, 0xF800,			/* red_mask, green, blue	*/
		0, 0, 0,						/* y_sample_bits, u, v		*/
		0, 0, 0,						/* horz_y_period, u, v		*/
		0, 0, 0,						/* vert_y_period, u, v		*/
		{ 'R', 'V', 'B' },				/* component_order			*/
		XvTopToBottom					/* scaline_order			*/
	},
	{
		FOURCC_RV24,					/* id						*/
		XvRGB,							/* type						*/
		LSBFirst,						/* byte_order				*/
		{ 'R', 'V' ,'2', '4',
		  0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00 },		/* guid						*/
		24,								/* bits_per_pixel			*/
		XvPacked,						/* format					*/
		1,								/* num_planes				*/
		24,								/* depth					*/
		0x0000FF, 0x00FF00, 0xFF0000,	/* red_mask, green, blue	*/
		0, 0, 0,						/* y_sample_bits, u, v		*/
		0, 0, 0,						/* horz_y_period, u, v		*/
		0, 0, 0,						/* vert_y_period, u, v		*/
		{ 'R', 'V', 'B' },				/* component_order			*/
		XvTopToBottom					/* scaline_order			*/
	},
	{
		FOURCC_RV32,					/* id						*/
		XvRGB,							/* type						*/
		LSBFirst,						/* byte_order				*/
		{ 'R', 'V' ,'3', '2',
		  0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00 },		/* guid						*/
		32,								/* bits_per_pixel			*/
		XvPacked,						/* format					*/
		1,								/* num_planes				*/
		24,								/* depth					*/
		0x0000FF, 0x00FF00, 0xFF0000,	/* red_mask, green, blue	*/
		0, 0, 0,						/* y_sample_bits, u, v		*/
		0, 0, 0,						/* horz_y_period, u, v		*/
		0, 0, 0,						/* vert_y_period, u, v		*/
		{ 'R', 'V', 'B' },				/* component_order			*/
		XvTopToBottom					/* scaline_order			*/
	},
};


/**************************************************************************/

/**
 * SAA7111 video decoder register values
 */


/** SAA7111 control sequences for selecting one out of four
    composite input channels */
static I2CByte SAA7111CompositeChannelSelect[N_COMPOSITE_CHANNELS][4] = {
    { 0x02, 0xC0, 0x09, 0x4A}, /* CVBS AI11 */
    { 0x02, 0xC1, 0x09, 0x4A}, /* CVBS AI12 */
    { 0x02, 0xC2, 0x09, 0x4A}, /* CVBS AI21 */
    { 0x02, 0xC3, 0x09, 0x4A}, /* CVBS AI22 */
};


/** SAA7111 control sequences for selecting one out of two
    s-video input channels */
static I2CByte SAA7111SVideoChannelSelect[N_SVIDEO_CHANNELS][4] = {
    { 0x02, 0xC6, 0x09, 0xCA}, /* Y/C AI11/AI21 */
    { 0x02, 0xC7, 0x09, 0xCA}, /* Y/C AI12/AI22 */
};


/** SAA7111 control sequences for selecting one out of three
    video norms */
static I2CByte SAA7111VideoStd[3][8] = {
    {0x06, 108, 0x07, 108, 0x08, 0x09, 0x0E, 0x01}, /* PAL */
    {0x06, 107, 0x07, 107, 0x08, 0x49, 0x0E, 0x01}, /* NTSC */
    {0x06, 108, 0x07, 108, 0x08, 0x01, 0x0E, 0x51}  /* SECAM */
};


#if 0
static I2CByte SAA7110InitData[] =
{
	/* Configuration */
    0x00, 0x4C, 0x01, 0x3C, 0x02, 0x00, 0x03, 0xEF,
    0x04, 0xBD, 0x05, 0xE2, 0x06, 0x00, 0x07, 0x00,
    0x08, 0xF8, 0x09, 0xF8, 0x0A, 0x60, 0x0B, 0x60,
    0x0C, 0x00, 0x0D, 0x80, 0x0E, 0x18, 0x0F, 0xD9,
    0x10, 0x00, 0x11, 0x2B, 0x12, 0x40, 0x13, 0x40,
    0x14, 0x42, 0x15, 0x1A, 0x16, 0xFF, 0x17, 0xDA,
    0x18, 0xE6, 0x19, 0x90, 0x20, 0xD9, 0x21, 0x16,
    0x22, 0x40, 0x23, 0x40, 0x24, 0x80, 0x25, 0x40,
    0x26, 0x80, 0x27, 0x4F, 0x28, 0xFE, 0x29, 0x01,
    0x2A, 0xCF, 0x2B, 0x0F, 0x2C, 0x03, 0x2D, 0x01,
    0x2E, 0x83, 0x2F, 0x03, 0x30, 0x40, 0x31, 0x35,
    0x32, 0x02, 0x33, 0x8C, 0x34, 0x03,

	/* NTSC */
    0x11, 0x2B, 0x0F, 0xD9,

	/* RCA input connector */
    0x06, 0x00, 0x0E, 0x18, 0x20, 0xD9, 0x21, 0x16,
    0x22, 0x40, 0x2C, 0x03,

};
#endif

static I2CByte SAA7111InitData[] =
{
    0x11, 0x1D, /* 0D D0=1: automatic colour killer off
		   D1=0: DMSD data to YUV output
		   D2=1: output enable H/V sync on
		   D3=1: output enable YUV data on */
    0x02, 0xC0, /* Mode 0 */
    0x03, 0x23, /* automatic gain */
    0x04, 0x00, /*  */
    0x05, 0x00, /*  */
    0x06, 108,  /* hor sync begin */
    0x07, 108,  /* hor sync stop */
    0x08, 0x88, /* sync control:
		   D1-0=00: VNOI = normal mode
		   D2=0: PLL closed
		   D3=1: VTR mode
		   D7=1: automatic field detection */
    0x09, 0x41, /* 4A luminance control */
    0x0A, 0x80, /* brightness = 128 (CCIR level) */
    0x0B, 0x40, /* contrast = 1.0 */
    0x0C, 0x40, /* crominance = 1.0 (CCIR level) */
    0x0D, 0x00, /* hue = 0 */
    0x0E, 0x01, /* chroma bandwidth = nominal
		   fast colour time constant = nominal
		   chrom comp filter on
		   colour standard PAL BGHI, NTSC M */
    0x10, 0x48, /* luminance delay compensation = 0
		   VRLN = 1
		   fine pos of hs = 0
		   output format = YUV 422 */
    0x12, 0x00, /* 20 D5=1: VPO in tristate */
    0x13, 0x00,
    0x15, 0x00,
    0x16, 0x00,
    0x17, 0x00,

};


/**************************************************************************/

/**
 * generates XF86VideoEncoding[i] with video norm norm, video input format
 * input and video input channel channel
 */
static int
SMI_AddEncoding(XF86VideoEncodingPtr enc, int i,
		int norm, int input, int channel)
{
    char* norm_string;
    char* input_string;
    char channel_string[20];

    ENTER_PROC("SMI_AddEncoding");

    norm_string = VideoNorms[norm].name;
    input_string = VideoInputs[input].name;
    sprintf(channel_string, "%d", channel);
    enc[i].id     = i;
    enc[i].name   = xalloc(strlen(norm_string) + 
			   strlen(input_string) + 
			   strlen(channel_string)+3);
    if (NULL == enc[i].name) {
	LEAVE_PROC("SMI_AddEncoding");
	return -1;
    }
    enc[i].width  = VideoNorms[norm].Wa;
    enc[i].height = VideoNorms[norm].Ha;
    enc[i].rate   = VideoNorms[norm].rate;
    sprintf(enc[i].name,"%s-%s-%s", norm_string, input_string, channel_string);

    LEAVE_PROC("SMI_AddEncoding");
    return 0;
}


/**
 * builds XF86VideoEncodings with all legal combinations of video norm,
 * video input format and video input channel
 */
static void
SMI_BuildEncodings(SMI_PortPtr p)
{
    int ch, n;

    ENTER_PROC("SMI_BuildEncodings");

    /* allocate memory for encoding array */
    p->enc = xalloc(sizeof(XF86VideoEncodingRec) * N_ENCODINGS);
    if (NULL == p->enc)
	goto fail;
    memset(p->enc,0,sizeof(XF86VideoEncodingRec) * N_ENCODINGS);
    /* allocate memory for video norm array */
    p->norm = xalloc(sizeof(int) * N_ENCODINGS);
    if (NULL == p->norm)
	goto fail;
    memset(p->norm,0,sizeof(int) * N_ENCODINGS);
    /* allocate memory for video input format array */
    p->input = xalloc(sizeof(int) * N_ENCODINGS);
    if (NULL == p->input)
	goto fail;
    memset(p->input,0,sizeof(int) * N_ENCODINGS);
    /* allocate memory for video channel number array */
    p->channel = xalloc(sizeof(int) * N_ENCODINGS);
    if (NULL == p->channel)
	goto fail;
    memset(p->channel,0,sizeof(int) * N_ENCODINGS);

    /* fill arrays */
    p->nenc = 0;
    for (ch = 0; ch < N_COMPOSITE_CHANNELS; ch++) {
	for (n = 0; n < N_VIDEO_NORMS; n++) {
	    SMI_AddEncoding(p->enc, p->nenc, n, COMPOSITE, ch);
	    p->norm[p->nenc]  = n;
	    p->input[p->nenc] = COMPOSITE;
	    p->channel[p->nenc] = ch;
	    p->nenc++;
	}
    }
    for (ch = 0; ch < N_SVIDEO_CHANNELS; ch++) {
	for (n = 0; n < N_VIDEO_NORMS; n++) {
	    SMI_AddEncoding(p->enc, p->nenc, n, SVIDEO, ch);
	    p->norm[p->nenc]  = n;
	    p->input[p->nenc] = SVIDEO;
	    p->channel[p->nenc] = ch;
	    p->nenc++;
	}
    }
    LEAVE_PROC("SMI_BuildEncodings");
    return;
    
 fail:
    if (p->input) xfree(p->input);
    p->input = NULL;
    if (p->norm) xfree(p->norm);
    p->norm = NULL;
    if (p->channel) xfree(p->channel);
    p->channel = NULL;
    if (p->enc) xfree(p->enc);
    p->enc = NULL;
    p->nenc = 0;
    LEAVE_PROC("SMI_BuildEncodings");
}


/******************************************************************************\
**                                                                            **
**                  X V E X T E N S I O N   I N T E R F A C E                 **
**                                                                            **
\******************************************************************************/

void
SMI_InitVideo(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    SMIPtr psmi = SMIPTR(pScrn);
    XF86VideoAdaptorPtr *ptrAdaptors, *newAdaptors = NULL;
    XF86VideoAdaptorPtr newAdaptor = NULL;
    int numAdaptors;

    ENTER_PROC("SMI_InitVideo");

    numAdaptors = xf86XVListGenericAdaptors(pScrn, &ptrAdaptors);

    DEBUG((VERBLEV, "numAdaptors=%d\n", numAdaptors));

    if (psmi->rotate == 0)
    {
        newAdaptor = SMI_SetupVideo(pScreen);
        DEBUG((VERBLEV, "newAdaptor=%p\n", newAdaptor));
        SMI_InitOffscreenImages(pScreen);
    }

    if (newAdaptor != NULL)
    {
        if (numAdaptors == 0)
        {
            numAdaptors = 1;
            ptrAdaptors = &newAdaptor;
        }
        else
        {
            newAdaptors = xalloc((numAdaptors + 1) *
                    sizeof(XF86VideoAdaptorPtr*));
            if (newAdaptors != NULL)
            {
                memcpy(newAdaptors, ptrAdaptors,
                        numAdaptors * sizeof(XF86VideoAdaptorPtr));
                newAdaptors[numAdaptors++] = newAdaptor;
                ptrAdaptors = newAdaptors;
            }
        }
    }

    if (numAdaptors != 0)
    {
      DEBUG((VERBLEV, "ScreenInit %i\n",numAdaptors));
        xf86XVScreenInit(pScreen, ptrAdaptors, numAdaptors);
    }

    if (newAdaptors != NULL)
    {
        xfree(newAdaptors);
    }

    LEAVE_PROC("SMI_InitVideo");
}


/*************************************************************************/

/*
 *  Video codec controls
 */

#if 0
/**
 * scales value value of attribute i to range min, max
 */
static int
Scale(int i, int value, int min, int max)
{
    return min + (value - SMI_VideoAttributes[i].min_value) * (max - min) /
	(SMI_VideoAttributes[i].max_value - SMI_VideoAttributes[i].min_value);
}
#endif
/**
 * sets video decoder attributes channel, encoding, brightness, contrast, saturation, hue
 */
static int
SetAttr(ScrnInfoPtr pScrn, int i, int value)
{
    SMIPtr pSmi = SMIPTR(pScrn);
    SMI_PortPtr pPort = (SMI_PortPtr) pSmi->ptrAdaptor->pPortPrivates[0].ptr;

    if (i < XV_ENCODING || i > XV_HUE)
	return BadMatch;
    
    /* clamps value to attribute range */
    value = CLAMP(value, SMI_VideoAttributes[i].min_value,
		  SMI_VideoAttributes[i].max_value);

    if (i == XV_BRIGHTNESS) {
	int my_value = (value <= 128? value + 128 : value - 128);
	WRITE_VPR(pSmi, 0x5C, 0xEDEDED | (my_value << 24));
    } else if (pPort->I2CDev.SlaveAddr == SAA7110) {
	return SetAttrSAA7110(pScrn, i, value);
    }
    else if (pPort->I2CDev.SlaveAddr == SAA7111) {
	return SetAttrSAA7111(pScrn, i, value);
    }
#if 0
    else {
	return XvBadAlloc;
    }
#endif

    return Success;
}


/**
 * sets SAA7110 video decoder attributes channel, encoding, brightness, contrast, saturation, hue
 */
static int
SetAttrSAA7110(ScrnInfoPtr pScrn, int i, int value)
{
    /* not supported */
    return XvBadAlloc;
}


/**
 * sets SAA7111 video decoder attributes channel, encoding,
 * brightness, contrast, saturation, hue
 */
static int
SetAttrSAA7111(ScrnInfoPtr pScrn, int i, int value)
{
    SMIPtr pSmi = SMIPTR(pScrn);
    SMI_PortPtr pPort = (SMI_PortPtr) pSmi->ptrAdaptor->pPortPrivates[0].ptr;

    if (i == XV_ENCODING) {
	int norm;
	int input;
	int channel;
	norm = pPort->norm[value];
	input = pPort->input[value];
	channel = pPort->channel[value];

	DEBUG((VERBLEV, "SetAttribute XV_ENCODING: %d. norm=%d input=%d channel=%d\n",
	       value, norm, input, channel));

	/* set video norm */
	if (!xf86I2CWriteVec(&(pPort->I2CDev), SAA7111VideoStd[norm],
			     ENTRIES(SAA7111VideoStd[norm]) / 2)) {
	    return XvBadAlloc;
	}
	/* set video input format and channel */
	if (input == COMPOSITE) {
	    if (!xf86I2CWriteVec(&(pPort->I2CDev),
				 SAA7111CompositeChannelSelect[channel],
				 ENTRIES(SAA7111CompositeChannelSelect[channel]) / 2)) {
		return XvBadAlloc;
	    }
	}
	else {
	    if (!xf86I2CWriteVec(&(pPort->I2CDev),
				 SAA7111SVideoChannelSelect[channel],
				 ENTRIES(SAA7111SVideoChannelSelect[channel]) / 2)) {
		return XvBadAlloc;
	    }
	}
    }
    else if (i >= XV_CAPTURE_BRIGHTNESS && i <= XV_HUE) {
	int slave_adr = 0;

	switch (i) {

	case XV_CAPTURE_BRIGHTNESS:
	    DEBUG((VERBLEV, "SetAttribute XV_BRIGHTNESS: %d\n", value));
	    slave_adr = 0x0a;
	    break;
		
	case XV_CONTRAST:
	    DEBUG((VERBLEV, "SetAttribute XV_CONTRAST: %d\n", value));
	    slave_adr = 0x0b;
	    break;

	case XV_SATURATION:
	    DEBUG((VERBLEV, "SetAttribute XV_SATURATION: %d\n", value));
	    slave_adr = 0x0c;
	    break;

	case XV_HUE:
	    DEBUG((VERBLEV, "SetAttribute XV_HUE: %d\n", value));
	    slave_adr = 0x0d;
	    break;

	default:
	    return XvBadAlloc;
	}
	if (!xf86I2CWriteByte(&(pPort->I2CDev), slave_adr, (value & 0xff)))
	    return XvBadAlloc;
    }
    else {
	return BadMatch;
    }

    /* debug: show registers */
    {
	I2CByte i2c_bytes[32];
	int i;
	xf86I2CReadBytes(&(pPort->I2CDev), 0, i2c_bytes, 32);
	DEBUG((VERBLEV, "SAA7111 Registers\n"));
	for (i=0; i<32; i++) {
	    DEBUG((VERBLEV, "%02X=%02X ", i, i2c_bytes[i]));
	    if ((i&7) == 7) DEBUG((VERBLEV, "\n"));
	}
    }

    return Success;
}


/******************************************************************************\
**																			  **
**						 V I D E O   M A N A G E M E N T					  **
**																			  **
\******************************************************************************/

static XF86VideoAdaptorPtr
SMI_SetupVideo(
	ScreenPtr	pScreen
)
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	SMIPtr pSmi = SMIPTR(pScrn);
	SMI_PortPtr smiPortPtr;
	XF86VideoAdaptorPtr ptrAdaptor;

	ENTER_PROC("SMI_SetupVideo");

	ptrAdaptor = xcalloc(1, sizeof(XF86VideoAdaptorRec) +
			sizeof(DevUnion) + sizeof(SMI_PortRec));
	if (ptrAdaptor == NULL)
	{
		LEAVE_PROC("SMI_SetupVideo");
		return(NULL);
	}

	ptrAdaptor->type = XvInputMask
#if SMI_USE_CAPTURE
					 | XvOutputMask
					 | XvVideoMask
#endif
					 | XvImageMask
					 | XvWindowMask
					 ;

	ptrAdaptor->flags = VIDEO_OVERLAID_IMAGES
					  | VIDEO_CLIP_TO_VIEWPORT
					  ;

	ptrAdaptor->name = "Silicon Motion Lynx Series Video Engine";

    ptrAdaptor->nPorts = 1;
    ptrAdaptor->pPortPrivates = (DevUnion*) &ptrAdaptor[1];
    ptrAdaptor->pPortPrivates[0].ptr = (pointer) &ptrAdaptor->pPortPrivates[1];

    smiPortPtr = (SMI_PortPtr) ptrAdaptor->pPortPrivates[0].ptr;

    SMI_BuildEncodings(smiPortPtr);
    ptrAdaptor->nEncodings = smiPortPtr->nenc;
    ptrAdaptor->pEncodings = smiPortPtr->enc;
#if 0
    /* aaa whats this? */
	for (i = 0; i < nElems(SMI_VideoEncodings); i++)
	{
		SMI_VideoEncodings[i].width = pSmi->lcdWidth;
		SMI_VideoEncodings[i].height = pSmi->lcdHeight;
	}
#endif

	ptrAdaptor->nFormats = nElems(SMI_VideoFormats);
	ptrAdaptor->pFormats = SMI_VideoFormats;

	ptrAdaptor->nAttributes = nElems(SMI_VideoAttributes);
	ptrAdaptor->pAttributes = SMI_VideoAttributes;

	ptrAdaptor->nImages = nElems(SMI_VideoImages);
	ptrAdaptor->pImages = SMI_VideoImages;

#if SMI_USE_CAPTURE
    ptrAdaptor->PutVideo = SMI_PutVideo;
	ptrAdaptor->PutStill = NULL;
    ptrAdaptor->GetVideo = NULL;
	ptrAdaptor->GetStill = NULL;
#else
	ptrAdaptor->PutVideo = NULL;
	ptrAdaptor->PutStill = NULL;
	ptrAdaptor->GetVideo = NULL;
	ptrAdaptor->GetStill = NULL;
#endif
	ptrAdaptor->StopVideo = SMI_StopVideo;
	ptrAdaptor->SetPortAttribute = SMI_SetPortAttribute;
	ptrAdaptor->GetPortAttribute = SMI_GetPortAttribute;
	ptrAdaptor->QueryBestSize = SMI_QueryBestSize;
	ptrAdaptor->PutImage = SMI_PutImage;
	ptrAdaptor->QueryImageAttributes = SMI_QueryImageAttributes;

    smiPortPtr->Attribute[XV_COLORKEY] = pSmi->videoKey;
    smiPortPtr->Attribute[XV_INTERLACED] = pSmi->interlaced;
	smiPortPtr->videoStatus = 0;

#if 0
    /* aaa does not work ? */
    if (xf86I2CProbeAddress(pSmi->I2C, SAA7111))
    {
        LEAVE_PROC("SMI_SetupVideo");
        return(NULL);
    }
    DEBUG((VERBLEV, "SAA7111 detected\n"));
#endif

    smiPortPtr->I2CDev.DevName = "SAA 7111A";
    smiPortPtr->I2CDev.SlaveAddr = SAA7111;
    smiPortPtr->I2CDev.pI2CBus = pSmi->I2C;

    
    if (xf86I2CDevInit(&(smiPortPtr->I2CDev)))
    {
	
	if (xf86I2CWriteVec(&(smiPortPtr->I2CDev), 	SAA7111InitData, 
			    ENTRIES(SAA7111InitData) / 2)) {
	    xvEncoding   = MAKE_ATOM(XV_ENCODING_NAME);
	    xvHue        = MAKE_ATOM(XV_HUE_NAME);
	    xvSaturation = MAKE_ATOM(XV_SATURATION_NAME);
	    xvContrast   = MAKE_ATOM(XV_CONTRAST_NAME);
	    
	    xvInterlaced = MAKE_ATOM(XV_INTERLACED_NAME);
	    DEBUG((VERBLEV, "SAA7111 intialized\n"));
    
	} else { 
	    xf86DestroyI2CDevRec(&(smiPortPtr->I2CDev),FALSE);
	    smiPortPtr->I2CDev.SlaveAddr = 0;
	}
    } else
	smiPortPtr->I2CDev.SlaveAddr = 0;
	
    REGION_INIT(pScreen, &smiPortPtr->clip, NullBox, 0);
    
    pSmi->ptrAdaptor = ptrAdaptor;
    pSmi->BlockHandler = pScreen->BlockHandler;
    pScreen->BlockHandler = SMI_BlockHandler;
    
    xvColorKey   = MAKE_ATOM(XV_COLORKEY_NAME);
    xvBrightness = MAKE_ATOM(XV_BRIGHTNESS_NAME);
    xvCapBrightness = MAKE_ATOM(XV_CAPTURE_BRIGHTNESS_NAME);
    
    SMI_ResetVideo(pScrn);
    LEAVE_PROC("SMI_SetupVideo");
    return(ptrAdaptor);
}


static void
SMI_ResetVideo(
	ScrnInfoPtr	pScrn
)
{
	SMIPtr pSmi = SMIPTR(pScrn);
    SMI_PortPtr pPort = (SMI_PortPtr) pSmi->ptrAdaptor->pPortPrivates[0].ptr;
	int r, g, b;

	ENTER_PROC("SMI_ResetVideo");

    SetAttr(pScrn, XV_ENCODING, 0);     /* Encoding = pal-composite-0 */
    SetAttr(pScrn, XV_BRIGHTNESS, 128); /* Brightness = 128 (CCIR level) */
    SetAttr(pScrn, XV_CAPTURE_BRIGHTNESS, 128); /* Brightness = 128 (CCIR level) */
    SetAttr(pScrn, XV_CONTRAST, 71);    /* Contrast = 71 (CCIR level) */
    SetAttr(pScrn, XV_SATURATION, 64);  /* Color saturation = 64 (CCIR level) */
    SetAttr(pScrn, XV_HUE, 0);          /* Hue = 0 */

	switch (pScrn->depth)
	{
		case 8:
            WRITE_VPR(pSmi, 0x04, pPort->Attribute[XV_COLORKEY] & 0x00FF);
			WRITE_VPR(pSmi, 0x08, 0);
			break;

		case 15:
		case 16:
            WRITE_VPR(pSmi, 0x04, pPort->Attribute[XV_COLORKEY] & 0xFFFF);
			WRITE_VPR(pSmi, 0x08, 0);
			break;

		default:
            r = (pPort->Attribute[XV_COLORKEY] & pScrn->mask.red) >> pScrn->offset.red;
            g = (pPort->Attribute[XV_COLORKEY] & pScrn->mask.green) >> pScrn->offset.green;
            b = (pPort->Attribute[XV_COLORKEY] & pScrn->mask.blue) >> pScrn->offset.blue;
			WRITE_VPR(pSmi, 0x04, ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
			WRITE_VPR(pSmi, 0x08, 0);
			break;
	}

    WRITE_VPR(pSmi, 0x5C, 0xEDEDED | (pPort->Attribute[XV_BRIGHTNESS] << 24));

	LEAVE_PROC("SMI_ResetVideo");
}


#if SMI_USE_CAPTURE
static int
SMI_PutVideo(
	ScrnInfoPtr	pScrn,
	short		vid_x,
	short		vid_y,
	short		drw_x,
	short		drw_y,
	short		vid_w,
	short		vid_h,
	short		drw_w,
	short		drw_h,
	RegionPtr	clipBoxes,
	pointer		data
)
{
    SMI_PortPtr pPort = (SMI_PortPtr) data;
	SMIPtr pSmi = SMIPTR(pScrn);
	CARD32 vid_pitch, vid_address;
	CARD32 vpr00, cpr00;
	int xscale, yscale;
	BoxRec dstBox;
	INT32 x1, y1, x2, y2;
    int norm;
	int areaHeight, width, height, fbPitch;
	int top, left;

    ENTER_PROC("SMI_PutVideo");

    DEBUG((VERBLEV, "Interlaced Video %d\n", pPort->Attribute[XV_INTERLACED]));

    if (!pPort->Attribute[XV_INTERLACED]) {
	/* no interlace: lines will be doubled */
	vid_h /= 2;
    }

    /* field start aaa*/
    norm = pPort->norm[pPort->Attribute[XV_ENCODING]];
    vid_x += VideoNorms[norm].HStart;
    vid_y += VideoNorms[norm].VStart;
    /* only even values allowed (UV-phase) */
    vid_x &= ~1;

    DEBUG((VERBLEV, "vid_x=%d vid_y=%d drw_x=%d drw_y=%d  "
	   "vid_w=%d vid_h=%d drw_w=%d drw_h=%d\n",
	   vid_x, vid_y, drw_x, drw_y, vid_w, vid_h, drw_w, drw_h));

	x1 = vid_x;
	y1 = vid_y;
	x2 = vid_x + vid_w;
	y2 = vid_y + vid_h;

	width = vid_w;
	height = vid_h;

	dstBox.x1 = drw_x;
	dstBox.y1 = drw_y;
	dstBox.x2 = drw_x + drw_w;
	dstBox.y2 = drw_y + drw_h;

#if 1
	if (!SMI_ClipVideo(pScrn, &dstBox, &x1, &y1, &x2, &y2, clipBoxes, width,
			height))
#else
    if (!xf86XVClipVideoHelper(&dstBox, &x1, &y1, &x2, &y2, clipBoxes, width,
            height))
#endif
	{
        LEAVE_PROC("SMI_PutVideo");
		return(Success);
	}

    DEBUG((VERBLEV, "Clip: x1=%d y1=%d x2=%d y2=%d\n",  x1 >> 16, y1 >> 16, x2 >> 16, y2 >> 16));

	dstBox.x1 -= pScrn->frameX0;
	dstBox.y1 -= pScrn->frameY0;
	dstBox.x2 -= pScrn->frameX0;
	dstBox.y2 -= pScrn->frameY0;

	vid_pitch = (vid_w * 2 + 7) & ~7;

	vpr00 = READ_VPR(pSmi, 0x00) & ~0x0FF000FF;
	cpr00 = READ_CPR(pSmi, 0x00) & ~0x000FFF00;

    /* vpr00:
       Bit 2..0   = 6: Video Window I Format                    = YUV4:2:2
       Bit 3      = 1: Video Window I Enable                    = enabled
       Bit 4      = 0: Video Window I YUV Averaging             = disabled
       Bit 5      = 0: Video Window I Hor. Replication          = disabled
       Bit 6      = 0: Video Window I data doubling             = disabled
       Bit 14..8  = 0: Video Window II                          = disabled
       Bit 18..16 = 0: Graphics Data Format                     = 8-bit index
       Bit 19     = 0: Top Video Window Select                  = window I
       Bit 20     = 1: Color Key for Window I                   = enabled
       Bit 21     = 0: Vertical Interpolation                   = s. below
       Bit 22     = 0: Flicker Reduction for TV Modes           = disabled
       Bit 23     = 0: Fixed Vertical Interpolation             = disabled
       Bit 24     = 1: Select Video Window I Source Addr        = 
       Bit 25     = 0: Enable V0FIFO to fetch 8-Bit color data  = disabled
       Bit 26     = 0:
       Bit 27     = 1: Color Key for Window II                  = disabled
       Bit 31..28 = reserved
    */
    if (pPort->Attribute[XV_INTERLACED]) {
	/*
	  Bit 21     = 0: Vertical Interpolation                   = disabled
	  Bit 24     = 0: Select Video Window I Source Addr        = 0
	*/
	vpr00 |= 0x0010000E;
    }
    else {
	/*
	  Bit 21     = 10: Vertical Interpolation                   = enabled
	  Bit 24     = 1: Select Video Window I Source Addr        = 1
	  1= Video window I source addr = capture port buffer ?
	*/
	vpr00 |= 0x0130000E;
    }

    /* cpr00:
       Bit 0      = 1: Video Capture Enable                     = enabled
       Bit 8      = 0: Capture Control                          = continous
       Bit 9      = 0: Double Buffer Enable                     = s. below
       Bit 10     = 0: Interlace Data Capture                   = s. below
       Bit 13..11 = 0: Frame Skip Enable                        = s. below
       Bit 15..14 = 0: Video Capture Input Format               = YUV4:2:2
       Bit 17..16 = 0: Enable Hor. Reduction                    = s. below
       Bit 19..18 = 0: Enable Vert. Reduction                   = s. below
       Bit 21..20 = 0: Enable Hor. Filtering                    = s. below
       Bit 22     = 0: HREF Polarity                            = high active
       Bit 23     = 0: VREF Polarity                            = high active
       Bit 24     = 1: Field Detection Method VSYNC edge        = rising
    */
    if (pPort->Attribute[XV_INTERLACED]) {
	/*
	  Bit 9      = 1: Double Buffer Enable                  = enabled
	  Bit 10     = 1: Interlace Data Capture                = enabled
	  Bit 13..11 = 0: Frame Skip Enable                     = no skip
	*/
	cpr00 |= 0x01000601;
    }
    else {
	/*
	  Bit 9      = 0: Double Buffer Enable                  = disabled
	  Bit 10     = 0: Interlace Data Capture                = disabled
	  Bit 13..11 = 010: Frame Skip Enable                   = skip every other frame
	*/
	cpr00 |= 0x01000801;
    }

	if (pSmi->ByteSwap)
		cpr00 |= 0x00004000;

	fbPitch = pSmi->Stride;
	if (pSmi->Bpp != 3)
	{
		fbPitch *= pSmi->Bpp;
	}

	if (vid_w <= drw_w)
	{
		xscale = (256 * vid_w / drw_w) & 0xFF;
	}
	else if (vid_w / 2 <= drw_w)
	{
		xscale = (128 * vid_w / drw_w) & 0xFF;
		width /= 2;
		vid_pitch /= 2;
		cpr00 |= 0x00010000;
	}
	else if (vid_w / 4 <= drw_w)
	{
		xscale = (64 * vid_w / drw_w) & 0xFF;
		width /= 4;
		vid_pitch /= 4;
		cpr00 |= 0x00020000;
	}
	else
	{
		xscale = 0;
		width /= 4;
		vid_pitch /= 4;
		cpr00 |= 0x00020000;
	}

	if (vid_h <= drw_h)
	{
		yscale = (256 * vid_h / drw_h) & 0xFF;
	}
	else if (vid_h / 2 <= drw_h)
	{
		yscale = (128 * vid_h / drw_h) & 0xFF;
		height /= 2;
		cpr00 |= 0x00040000;
	}
	else if (vid_h / 4 <= drw_h)
	{
		yscale = (64 * vid_h / drw_h) & 0xFF;
		height /= 4;
		cpr00 |= 0x00080000;
	}
	else
	{
		yscale = 0;
		height /= 4;
		cpr00 |= 0x00080000;
	}

	do
	{
		areaHeight = (vid_pitch * height + fbPitch - 1) / fbPitch;
	DEBUG((VERBLEV, "SMI_AllocateMemory: vid_pitch=%d height=%d fbPitch=%d areaHeight=%d\n",
	       vid_pitch, height, fbPitch, areaHeight));
        pPort->area = SMI_AllocateMemory(pScrn, pPort->area, areaHeight);
        if (pPort->area == NULL)
		{
			if ((cpr00 & 0x000C0000) == 0)
			{
				/* height -> 1/2 height */
				yscale = (128 * vid_h / drw_h) & 0xFF;
				height = vid_h / 2;
				cpr00 |= 0x00040000;
			}
			else if (cpr00 & 0x00040000)
			{
				/* 1/2 height -> 1/4 height */
				yscale = (64 * vid_h / drw_h) & 0xFF;
				height = vid_h / 4;
				cpr00 ^= 0x000C0000;
			}
			else
			{
				/* 1/4 height */
				if ((cpr00 & 0x00030000) == 0)
				{
					/* width -> 1/2 width */
					xscale = (128 * vid_w / drw_w) & 0xFF;
					width = vid_w / 2;
					cpr00 |= 0x00010000;
				}
				else if (cpr00 & 0x00010000)
				{
					/* 1/2 width -> 1/4 width */
					xscale = (64 * vid_w / drw_w) & 0xFF;
					width = vid_w / 4;
					cpr00 ^= 0x00030000;
				}
				else
				{
		    DEBUG((VERBLEV, "allocate error\n"));
                    LEAVE_PROC("SMI_PutVideo");
					return(BadAlloc);
				}
			}
		}
	}
    while (pPort->area == NULL);

    DEBUG((VERBLEV, "xscale==%d yscale=%d width=%d height=%d\n",
	   xscale, yscale, width, height));

    /* aaa whats this                     ----------------------v ?
    vid_address = (pPort->area->box.y1 * fbPitch) + ((y1 >> 16) * vid_pitch);*/
    vid_address = (pPort->area->box.y1 * fbPitch);

    DEBUG((VERBLEV, "test RegionsEqual\n"));
    if (!RegionsEqual(&pPort->clip, clipBoxes))
    {
	DEBUG((VERBLEV, "RegionCopy\n"));
        REGION_COPY(pScreen, &pPort->clip, clipBoxes);
#if USE_XAA
        XAAFillSolidRects(pScrn, pPort->Attribute[XV_COLORKEY], GXcopy, ~0,
				REGION_NUM_RECTS(clipBoxes), REGION_RECTS(clipBoxes));
#else
	DEBUG((VERBLEV, "FillKey\n"));
	xf86XVFillKeyHelper(pScrn->pScreen, pPort->Attribute[XV_COLORKEY], clipBoxes);
#endif

	}

	left = x1 >> 16;
	top = y1 >> 16;
	width = (x2 - x1) >> 16;
	height = (y2 - y1) >> 16;

	OUT_SEQ(pSmi, 0x21, IN_SEQ(pSmi, 0x21) & ~0x04);
	WRITE_VPR(pSmi, 0x54, READ_VPR(pSmi, 0x54) | 0x00200000);
#if 0
	SMI_WaitForSync(pScrn);
#endif
    /* Video Window I Left and Top Boundaries */
	WRITE_VPR(pSmi, 0x14, dstBox.x1 + (dstBox.y1 << 16));
    /* Video Window I Right and Bottom Boundaries */
	WRITE_VPR(pSmi, 0x18, dstBox.x2 + (dstBox.y2 << 16));
    /* Video Window I Source Width and Offset */
	WRITE_VPR(pSmi, 0x20, (vid_pitch / 8) + ((vid_pitch / 8) << 16));
    /* Video Window I Stretch Factor */
	WRITE_VPR(pSmi, 0x24, (xscale << 8) + yscale);

    if (pPort->Attribute[XV_INTERLACED]) {
	/* Video Window II Left and Top Boundaries */
	WRITE_VPR(pSmi, 0x28, dstBox.x1 + (dstBox.y1 << 16));
	/* Video Window II Right and Bottom Boundaries */
	WRITE_VPR(pSmi, 0x2C, dstBox.x2 + (dstBox.y2 << 16));
	/* Video Window II Source Width and Offset */
	WRITE_VPR(pSmi, 0x34, (vid_pitch / 8) + ((vid_pitch / 8) << 16));
	/* Video Window II Stretch Factor */
	WRITE_VPR(pSmi, 0x38, (xscale << 8) + yscale);

	/* Video Window I Source Start Address */
	WRITE_VPR(pSmi, 0x1C, vid_address / 8);
	/* Video Window II Source Start Address */
	WRITE_VPR(pSmi, 0x30, vid_address / 8);

	/* Video Window I Source Start Address */
	WRITE_VPR(pSmi, 0x48, vid_address / 8);
	/* Video Window II Source Start Address */
	WRITE_VPR(pSmi, 0x4C, vid_address / 8 + vid_pitch / 8);
	
	/* Video Source Clipping Control */
	WRITE_CPR(pSmi, 0x04, left + ((top/2) << 16));
	/* Video Source Capture Size Control */
	WRITE_CPR(pSmi, 0x08, width + ((height/2) << 16));
	/* Capture Port Buffer I Source Start Address */
	WRITE_CPR(pSmi, 0x0C, vid_address / 8);
	/* Capture Port Buffer II Source Start Address */
	WRITE_CPR(pSmi, 0x10, vid_address / 8 + vid_pitch / 8);
	/* Capture Port Source Offset Address */
	WRITE_CPR(pSmi, 0x14, 2*(vid_pitch / 8) + ((2*(vid_pitch / 8)) << 16));
    }
    else {
	/* Video Source Clipping Control */
	WRITE_CPR(pSmi, 0x04, left + (top << 16));
	/* Video Source Capture Size Control */
	WRITE_CPR(pSmi, 0x08, width + (height << 16));
	/* Capture Port Buffer I Source Start Address */
	WRITE_CPR(pSmi, 0x0C, vid_address / 8);
	/* Capture Port Buffer II Source Start Address */
	WRITE_CPR(pSmi, 0x10, vid_address / 8);
	/* Capture Port Source Offset Address */
	WRITE_CPR(pSmi, 0x14, (vid_pitch / 8) + ((vid_pitch / 8) << 16));
    }

	WRITE_CPR(pSmi, 0x00, cpr00);
	WRITE_VPR(pSmi, 0x00, vpr00);

    pPort->videoStatus = CLIENT_VIDEO_ON;
    DEBUG((VERBLEV, "SMI_PutVideo success\n"));
    LEAVE_PROC("SMI_PutVideo");
	return(Success);
}
#endif


static void
SMI_StopVideo(
	ScrnInfoPtr	pScrn,
	pointer		data,
	Bool		shutdown
)
{
	SMI_PortPtr pPort = (SMI_PortPtr) data;
	SMIPtr pSmi = SMIPTR(pScrn);

	ENTER_PROC("SMI_StopVideo");

	REGION_EMPTY(pScrn->pScreen, &pPort->clip);

	if (shutdown)
	{
		if (pPort->videoStatus & CLIENT_VIDEO_ON)
		{
			WRITE_VPR(pSmi, 0x00, READ_VPR(pSmi, 0x00) & ~0x01000008);
#if SMI_USE_CAPTURE
			WRITE_CPR(pSmi, 0x00, READ_CPR(pSmi, 0x00) & ~0x00000001);
			WRITE_VPR(pSmi, 0x54, READ_VPR(pSmi, 0x54) & ~0x00F00000);
/* #864		OUT_SEQ(pSmi, 0x21, IN_SEQ(pSmi, 0x21) | 0x04); */
#endif
		}
        if (pPort->area != NULL)
		{
            xf86FreeOffscreenArea(pPort->area);
            pPort->area = NULL;
		}
        pPort->videoStatus = 0;
        /* pPort->i2cDevice = 0;aaa*/
	}
	else
	{
        if (pPort->videoStatus & CLIENT_VIDEO_ON)
		{
            pPort->videoStatus |= OFF_TIMER;
            pPort->offTime = currentTime.milliseconds + OFF_DELAY;
		}
	}

	LEAVE_PROC("SMI_StopVideo");
}


static int
SMI_SetPortAttribute(
	ScrnInfoPtr	pScrn,
	Atom		attribute,
	INT32		value,
	pointer		data
)
{
    int res;
    SMI_PortPtr pPort = (SMI_PortPtr) data;
	SMIPtr pSmi = SMIPTR(pScrn);

	ENTER_PROC("SMI_SetPortAttribute");

    if (attribute == xvColorKey) {
		int r, g, b;

        pPort->Attribute[XV_COLORKEY] = value;
		switch (pScrn->depth)
		{
			case 8:
				WRITE_VPR(pSmi, 0x04, value & 0x00FF);
				break;

			case 15:
			case 16:
				WRITE_VPR(pSmi, 0x04, value & 0xFFFF);
				break;

			default:
				r = (value & pScrn->mask.red) >> pScrn->offset.red;
				g = (value & pScrn->mask.green) >> pScrn->offset.green;
				b = (value & pScrn->mask.blue) >> pScrn->offset.blue;
				WRITE_VPR(pSmi, 0x04,
						((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
				break;
		}
	res = Success;
	}
    else if (attribute == xvInterlaced) {
        pPort->Attribute[XV_INTERLACED] = (value != 0);
	res = Success;
		}
    else if (attribute == xvEncoding) {
        res = SetAttr(pScrn, XV_ENCODING, value);
	}
    else if (attribute == xvBrightness) {
        res = SetAttr(pScrn, XV_BRIGHTNESS, value);
    }
    else if (attribute == xvCapBrightness) {
        res = SetAttr(pScrn, XV_CAPTURE_BRIGHTNESS, value);
    }
    else if (attribute == xvContrast) {
        res = SetAttr(pScrn, XV_CONTRAST, value);
    }
    else if (attribute == xvSaturation) {
        res = SetAttr(pScrn, XV_SATURATION, value);
    }
    else if (attribute == xvHue) {
        res = SetAttr(pScrn, XV_HUE, value);
    }
    else {
        res = BadMatch;
	}

	LEAVE_PROC("SMI_SetPortAttribute");
    return(res);
}


static int
SMI_GetPortAttribute(
	ScrnInfoPtr	pScrn,
	Atom		attribute,
	INT32		*value,
	pointer		data
)
{
    SMI_PortPtr pPort = (SMI_PortPtr) data;

	ENTER_PROC("SMI_GetPortAttribute");
    if (attribute == xvEncoding)
        *value = pPort->Attribute[XV_ENCODING];
    else if (attribute == xvBrightness)
        *value = pPort->Attribute[XV_BRIGHTNESS];
    else if (attribute == xvCapBrightness)
        *value = pPort->Attribute[XV_CAPTURE_BRIGHTNESS];
    else if (attribute == xvContrast)
        *value = pPort->Attribute[XV_CONTRAST];
    else if (attribute == xvSaturation)
        *value = pPort->Attribute[XV_SATURATION];
    else if (attribute == xvHue)
        *value = pPort->Attribute[XV_HUE];
    else if (attribute == xvColorKey)
        *value = pPort->Attribute[XV_COLORKEY];

	else
	{
		LEAVE_PROC("SMI_GetPortAttribute");
		return(BadMatch);
	}

	LEAVE_PROC("SMI_GetPortAttribute");
	return(Success);
}


static void
SMI_QueryBestSize(
	ScrnInfoPtr		pScrn,
	Bool			motion,
	short			vid_w,
	short			vid_h,
	short			drw_w,
	short			drw_h,
	unsigned int	*p_w,
	unsigned int	*p_h,
	pointer			data
)
{
	SMIPtr pSmi = SMIPTR(pScrn);

	ENTER_PROC("SMI_QueryBestSize");

	*p_w = min(drw_w, pSmi->lcdWidth);
	*p_h = min(drw_h, pSmi->lcdHeight);

	LEAVE_PROC("SMI_QueryBestSize");
}


static int
SMI_PutImage(
	ScrnInfoPtr		pScrn,
	short			src_x,
	short			src_y,
	short			drw_x,
	short			drw_y,
	short			src_w,
	short			src_h,
	short			drw_w,
	short			drw_h,
	int				id,
	unsigned char	*buf,
	short			width,
	short			height,
	Bool			sync,
	RegionPtr		clipBoxes,
	pointer			data
)
{
	SMIPtr pSmi = SMIPTR(pScrn);
    SMI_PortPtr pPort = (SMI_PortPtr) pSmi->ptrAdaptor->pPortPrivates[0].ptr;
	INT32 x1, y1, x2, y2;
	int bpp = 0;
	int fbPitch, srcPitch, srcPitch2 = 0, dstPitch, areaHeight;
	BoxRec dstBox;
	CARD32 offset, offset2 = 0, offset3 = 0, tmp;
	int left, top, nPixels, nLines;
	unsigned char *dstStart;

	ENTER_PROC("SMI_PutImage");

	x1 = src_x;
	y1 = src_y;
	x2 = src_x + src_w;
	y2 = src_y + src_h;

	dstBox.x1 = drw_x;
	dstBox.y1 = drw_y;
	dstBox.x2 = drw_x + drw_w;
	dstBox.y2 = drw_y + drw_h;

	if (!SMI_ClipVideo(pScrn, &dstBox, &x1, &y1, &x2, &y2, clipBoxes, width,
			height))
	{
		LEAVE_PROC("SMI_PutImage");
		return(Success);
	}

	dstBox.x1 -= pScrn->frameX0;
	dstBox.y1 -= pScrn->frameY0;
	dstBox.x2 -= pScrn->frameX0;
	dstBox.y2 -= pScrn->frameY0;

	if (pSmi->Bpp == 3)
	{
		fbPitch = pSmi->Stride;
	}
	else
	{
		fbPitch = pSmi->Stride * pSmi->Bpp;
	}

	switch (id)
	{
		case FOURCC_YV12:
			srcPitch  = (width + 3) & ~3;
			offset2   = srcPitch * height;
			srcPitch2 = ((width >> 1) + 3) & ~3;
			offset3   = offset2 + (srcPitch2 * (height >> 1));
			dstPitch  = ((width << 1) + 15) & ~15;
			break;

		case FOURCC_I420:
			srcPitch  = (width + 3) & ~3;
			offset3   = srcPitch * height;
			srcPitch2 = ((width >> 1) + 3) & ~3;
			offset2   = offset3 + (srcPitch2 * (height >> 1));
			dstPitch  = ((width << 1) + 15) & ~15;
			break;

		case FOURCC_RV24:
			bpp = 3;
			srcPitch = width * bpp;
			dstPitch = (srcPitch + 15) & ~15;
			break;

		case FOURCC_RV32:
			bpp = 4;
			srcPitch = width * bpp;
			dstPitch = (srcPitch + 15) & ~15;
			break;

		case FOURCC_YUY2:
		case FOURCC_RV15:
		case FOURCC_RV16:
		default:
			bpp = 2;
			srcPitch = width * bpp;
			dstPitch = (srcPitch + 15) & ~15;
			break;
	}

	areaHeight = ((dstPitch * height) + fbPitch - 1) / fbPitch;
    pPort->area = SMI_AllocateMemory(pScrn, pPort->area, areaHeight);
    if (pPort->area == NULL)
	{
		LEAVE_PROC("SMI_PutImage");
		return(BadAlloc);
	}

	top = y1 >> 16;
	left = (x1 >> 16) & ~1;
	nPixels = ((((x2 + 0xFFFF) >> 16) + 1) & ~1) - left;
	left *= bpp;

    offset = (pPort->area->box.y1 * fbPitch) + (top * dstPitch);
	dstStart = pSmi->FBBase + offset + left;

	switch (id)
	{
		case FOURCC_YV12:
		case FOURCC_I420:
			top &= ~1;
			tmp = ((top >> 1) * srcPitch2) + (left >> 2);
			offset2 += tmp;
			offset3 += tmp;
			nLines = ((((y2 + 0xFFFF) >> 16) + 1) & ~1) - top;
			SMI_CopyYV12Data(buf + (top * srcPitch) + (left >> 1),
					buf + offset2, buf + offset3, dstStart, srcPitch, srcPitch2,
					dstPitch, nLines, nPixels);
			break;

		default:
			buf += (top * srcPitch) + left;
			nLines = ((y2 + 0xFFFF) >> 16) - top;
			SMI_CopyData(buf, dstStart, srcPitch, dstPitch, nLines,
					nPixels * bpp);
			break;
	}

    if (!RegionsEqual(&pPort->clip, clipBoxes))
	{
        REGION_COPY(pScreen, &pPort->clip, clipBoxes);
#if USE_XAA
        XAAFillSolidRects(pScrn, pPort->Attribute[XV_COLORKEY], GXcopy, ~0,
				REGION_NUM_RECTS(clipBoxes), REGION_RECTS(clipBoxes));
#else
	/*aaa*/
	return(BadAlloc);
#endif
	}

	SMI_DisplayVideo(pScrn, id, offset, width, height, dstPitch, x1, y1, x2, y2,
			&dstBox, src_w, src_h, drw_w, drw_h);

    pPort->videoStatus = CLIENT_VIDEO_ON;
	LEAVE_PROC("SMI_PutImage");
	return(Success);
	
}


static int
SMI_QueryImageAttributes(
	ScrnInfoPtr		pScrn,
	int				id,
	unsigned short	*width,
	unsigned short	*height,
	int				*pitches,
	int				*offsets
)
{
	SMIPtr pSmi = SMIPTR(pScrn);
	int size, tmp;

	ENTER_PROC("SMI_QueryImageAttributes");

	if (*width > pSmi->lcdWidth)
	{
		*width = pSmi->lcdWidth;
	}
	if (*height > pSmi->lcdHeight)
	{
		*height = pSmi->lcdHeight;
	}

	*width = (*width + 1) & ~1;
	if (offsets != NULL)
	{
		offsets[0] = 0;
	}

	switch (id)
	{
		case FOURCC_YV12:
		case FOURCC_I420:
			*height = (*height + 1) & ~1;
			size = (*width + 3) & ~3;
			if (pitches != NULL)
			{
				pitches[0] = size;
			}
			size *= *height;
			if (offsets != NULL)
			{
				offsets[1] = size;
			}
			tmp = ((*width >> 1) + 3) & ~3;
			if (pitches != NULL)
			{
				pitches[1] = pitches[2] = tmp;
			}
			tmp *= (*height >> 1);
			size += tmp;
			if (offsets != NULL)
			{
				offsets[2] = size;
			}
			size += tmp;
			break;

		case FOURCC_YUY2:
		case FOURCC_RV15:
		case FOURCC_RV16:
		default:
			size = *width * 2;
			if (pitches != NULL)
			{
				pitches[0] = size;
			}
			size *= *height;
			break;

		case FOURCC_RV24:
			size = *width * 3;
			if (pitches != NULL)
			{
				pitches[0] = size;
			}
			size *= *height;
			break;

		case FOURCC_RV32:
			size = *width * 4;
			if (pitches != NULL)
			{
				pitches[0] = size;
			}
			size *= *height;
			break;
	}

	LEAVE_PROC("SMI_QueryImageAttributes");
	return(size);
}


/******************************************************************************\
**																			  **
**						S U P P O R T   F U N C T I O N S					  **
**																			  **
\******************************************************************************/
#if 0
static void
SMI_WaitForSync(
	ScrnInfoPtr	pScrn
)
{
	SMIPtr pSmi = SMIPTR(pScrn);
	vgaHWPtr hwp = VGAHWPTR(pScrn);
	int vgaIOBase  = hwp->IOBase;
	int vgaCRIndex = vgaIOBase + VGA_CRTC_INDEX_OFFSET;
	int vgaCRData  = vgaIOBase + VGA_CRTC_DATA_OFFSET;

	VerticalRetraceWait();
}
#endif

static Bool
RegionsEqual(
	RegionPtr	A,
	RegionPtr	B
)
{
	int *dataA, *dataB;
	int num;

	ENTER_PROC("RegionsEqual");

	num = REGION_NUM_RECTS(A);
	if (num != REGION_NUM_RECTS(B))
	{
		LEAVE_PROC("RegionsEqual");
		return(FALSE);
	}

	if (   (A->extents.x1 != B->extents.x1)
		|| (A->extents.y1 != B->extents.y1)
		|| (A->extents.x2 != B->extents.x2)
		|| (A->extents.y2 != B->extents.y2)
	)
	{
		LEAVE_PROC("RegionsEqual");
		return(FALSE);
	}

	dataA = (int*) REGION_RECTS(A);
	dataB = (int*) REGION_RECTS(B);

	while (num--)
	{
		if ((dataA[0] != dataB[0]) || (dataA[1] != dataB[1]))
		{
			return(FALSE);
		}
		dataA += 2;
		dataB += 2;
	}

	LEAVE_PROC("RegionsEqual");
	return(TRUE);
}

static Bool
SMI_ClipVideo(
	ScrnInfoPtr	pScrn,
	BoxPtr		dst,
	INT32		*x1,
	INT32		*y1,
	INT32		*x2,
	INT32		*y2,
	RegionPtr	reg,
	INT32		width,
	INT32		height
)
{
    INT32 vscale, hscale;
	BoxPtr extents = REGION_EXTENTS(pScrn, reg);
	int diff;

	ENTER_PROC("SMI_ClipVideo");

    DEBUG((VERBLEV, "ClipVideo(%d): x1=%d y1=%d x2=%d y2=%d\n",  __LINE__, *x1 >> 16, *y1 >> 16, *x2 >> 16, *y2 >> 16));
	/* PDR#941 */
	extents->x1 = max(extents->x1, pScrn->frameX0);
	extents->y1 = max(extents->y1, pScrn->frameY0);

	hscale = ((*x2 - *x1) << 16) / (dst->x2 - dst->x1);
	vscale = ((*y2 - *y1) << 16) / (dst->y2 - dst->y1);

	*x1 <<= 16; *y1 <<= 16;
	*x2 <<= 16; *y2 <<= 16;

    DEBUG((VERBLEV, "ClipVideo(%d): x1=%d y1=%d x2=%d y2=%d\n",  __LINE__, *x1 >> 16, *y1 >> 16, *x2 >> 16, *y2 >> 16));

	diff = extents->x1 - dst->x1;
	if (diff > 0)
	{
		dst->x1 = extents->x1;
		*x1 += diff * hscale;
	}

	diff = extents->y1 - dst->y1;
	if (diff > 0)
	{
		dst->y1 = extents->y1;
		*y1 += diff * vscale;
	}

	diff = dst->x2 - extents->x2;
	if (diff > 0)
	{
		dst->x2 = extents->x2; /* PDR#687 */
		*x2 -= diff * hscale;
	}

	diff = dst->y2 - extents->y2;
	if (diff > 0)
	{
		dst->y2 = extents->y2;
		*y2 -= diff * vscale;
	}

    DEBUG((VERBLEV, "ClipVideo(%d): x1=%d y1=%d x2=%d y2=%d\n",  __LINE__, *x1 >> 16, *y1 >> 16, *x2 >> 16, *y2 >> 16));

	if (*x1 < 0)
	{
		diff = (-*x1 + hscale - 1) / hscale;
		dst->x1 += diff;
		*x1 += diff * hscale;
	}

	if (*y1 < 0)
	{
		diff = (-*y1 + vscale - 1) / vscale;
		dst->y1 += diff;
		*y1 += diff * vscale;
	}

    DEBUG((VERBLEV, "ClipVideo(%d): x1=%d y1=%d x2=%d y2=%d\n",  __LINE__, *x1 >> 16, *y1 >> 16, *x2 >> 16, *y2 >> 16));

#if 0 /* aaa was macht dieser code? */
	delta = *x2 - (width << 16);
	if (delta > 0)
	{
		diff = (delta + hscale - 1) / hscale;
		dst->x2 -= diff;
		*x2 -= diff * hscale;
	}

	delta = *y2 - (height << 16);
	if (delta > 0)
	{
		diff = (delta + vscale - 1) / vscale;
		dst->y2 -= diff;
		*y2 -= diff * vscale;
	}
#endif

    DEBUG((VERBLEV, "ClipVideo(%d): x1=%d y1=%d x2=%d y2=%d\n",  __LINE__, *x1 >> 16, *y1 >> 16, *x2 >> 16, *y2 >> 16));

	if ((*x1 >= *x2) || (*y1 >= *y2))
	{
		LEAVE_PROC("SMI_ClipVideo");
		return(FALSE);
	}

	if (   (dst->x1 != extents->x1) || (dst->y1 != extents->y1)
		|| (dst->x2 != extents->x2) || (dst->y2 != extents->y2)
	)
	{
		RegionRec clipReg;
		REGION_INIT(pScrn, &clipReg, dst, 1);
		REGION_INTERSECT(pScrn, reg, reg, &clipReg);
		REGION_UNINIT(pScrn, &clipReg);
	}

    DEBUG((VERBLEV, "ClipVideo(%d): x1=%d y1=%d x2=%d y2=%d\n",  __LINE__, *x1 >> 16, *y1 >> 16, *x2 >> 16, *y2 >> 16));

	LEAVE_PROC("SMI_ClipVideo");
	return(TRUE);
}

static void
SMI_DisplayVideo(
	ScrnInfoPtr	pScrn,
	int			id,
	int			offset,
	short		width,
	short		height,
	int			pitch,
	int			x1,
	int			y1,
	int			x2,
	int			y2,
	BoxPtr		dstBox,
	short		vid_w,
	short		vid_h,
	short		drw_w,
	short		drw_h
)
{
	SMIPtr pSmi = SMIPTR(pScrn);
	CARD32 vpr00;
	int hstretch, vstretch;

	ENTER_PROC("SMI_DisplayVideo");

	vpr00 = READ_VPR(pSmi, 0x00) & ~0x0CB800FF;

	switch (id)
	{
		case FOURCC_YV12:
		case FOURCC_I420:
		case FOURCC_YUY2:
			vpr00 |= 0x6;
			break;

		case FOURCC_RV15:
			vpr00 |= 0x1;
			break;

		case FOURCC_RV16:
			vpr00 |= 0x2;
			break;

		case FOURCC_RV24:
			vpr00 |= 0x4;
			break;

		case FOURCC_RV32:
			vpr00 |= 0x3;
			break;
	}

	
	if (drw_w > vid_w)
	{
		hstretch = (2560 * vid_w / drw_w + 5) / 10;
	}
	else
	{
		hstretch = 0;
	}

	if (drw_h > vid_h)
	{
		vstretch = (2560 * vid_h / drw_h + 5) / 10;
		vpr00 |= 1 << 21;
	}
	else
	{
		vstretch = 0;
	}
#if 0
	SMI_WaitForSync(pScrn);
#endif
	WRITE_VPR(pSmi, 0x00, vpr00 | (1 << 3) | (1 << 20));
	WRITE_VPR(pSmi, 0x14, (dstBox->x1) | (dstBox->y1 << 16));
	WRITE_VPR(pSmi, 0x18, (dstBox->x2) | (dstBox->y2 << 16));
	WRITE_VPR(pSmi, 0x1C, offset >> 3);
	WRITE_VPR(pSmi, 0x20, (pitch >> 3) | ((pitch >> 3) << 16));
	WRITE_VPR(pSmi, 0x24, (hstretch << 8) | vstretch);

	LEAVE_PROC("SMI_DisplayVideo");
}

static void
SMI_BlockHandler(
	int		i,
	pointer	blockData,
	pointer	pTimeout,
	pointer	pReadMask
)
{
	ScreenPtr	pScreen = screenInfo.screens[i];
	ScrnInfoPtr	pScrn	= xf86Screens[i];
	SMIPtr		pSmi    = SMIPTR(pScrn);
    SMI_PortPtr pPort = (SMI_PortPtr) pSmi->ptrAdaptor->pPortPrivates[0].ptr;

	pScreen->BlockHandler = pSmi->BlockHandler;
	(*pScreen->BlockHandler)(i, blockData, pTimeout, pReadMask);
	pScreen->BlockHandler = SMI_BlockHandler;

    if (pPort->videoStatus & TIMER_MASK)
	{
		UpdateCurrentTime();
        if (pPort->videoStatus & OFF_TIMER)
		{
            if (pPort->offTime < currentTime.milliseconds)
			{
				WRITE_VPR(pSmi, 0x00, READ_VPR(pSmi, 0x00) & ~0x00000008);
                pPort->videoStatus = FREE_TIMER;
                pPort->freeTime = currentTime.milliseconds + FREE_DELAY;
			}
		}
		else
		{
            if (pPort->freeTime < currentTime.milliseconds)
			{
                xf86FreeOffscreenArea(pPort->area);
                pPort->area = NULL;
			}
            pPort->videoStatus = 0;
		}
	}
}

#if 0
static int
SMI_SendI2C(
	ScrnInfoPtr		pScrn,
	CARD8			device,
	char			*devName,
	SMI_I2CDataPtr	i2cData
)
{
	SMIPtr pSmi = SMIPTR(pScrn);
	I2CDevPtr dev;
	int status = Success;

	ENTER_PROC("SMI_SendI2C");

	if (pSmi->I2C == NULL)
	{
		LEAVE_PROC("SMI_SendI2C");
		return(BadAlloc);
	}

	dev = xf86CreateI2CDevRec();
	if (dev == NULL)
	{
		LEAVE_PROC("SMI_SendI2C");
		return(BadAlloc);
	}
	dev->DevName = devName;
	dev->SlaveAddr = device;
	dev->pI2CBus = pSmi->I2C;

	if (!xf86I2CDevInit(dev))
	{
		status = BadAlloc;
	}
	else
	{
		while (i2cData->address != 0xFF || i2cData->data != 0xFF) /* PDR#676 */
		{
			if (!xf86I2CWriteByte(dev, i2cData->address, i2cData->data))
			{
				status = BadAlloc;
				break;
			}
			i2cData++;
		}
	}

	xf86DestroyI2CDevRec(dev, TRUE);
	LEAVE_PROC("SMI_SendI2C");
	return(status);
}
#endif

/******************************************************************************\
**																			  **
**				 O F F S C R E E N   M E M O R Y   M A N A G E R			  **
**																			  **
\******************************************************************************/

static void
SMI_InitOffscreenImages(
	ScreenPtr	pScreen
)
{
	XF86OffscreenImagePtr offscreenImages;
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	SMIPtr pSmi = SMIPTR(pScrn);
	SMI_PortPtr pPort = (SMI_PortPtr) pSmi->ptrAdaptor->pPortPrivates[0].ptr;

	ENTER_PROC("SMI_InitOffscreenImages");

	offscreenImages = xalloc(sizeof(XF86OffscreenImageRec));
	if (offscreenImages == NULL)
	{
		LEAVE_PROC("SMI_InitOffscreenImages");
		return;
	}

	offscreenImages->image = SMI_VideoImages;
	offscreenImages->flags = VIDEO_OVERLAID_IMAGES
						   | VIDEO_CLIP_TO_VIEWPORT;
	offscreenImages->alloc_surface = SMI_AllocSurface;
	offscreenImages->free_surface = SMI_FreeSurface;
	offscreenImages->display = SMI_DisplaySurface;
	offscreenImages->stop = SMI_StopSurface;
	offscreenImages->getAttribute = SMI_GetSurfaceAttribute;
	offscreenImages->setAttribute = SMI_SetSurfaceAttribute;
	offscreenImages->max_width = pSmi->lcdWidth;
	offscreenImages->max_height = pSmi->lcdHeight;
	if (!pPort->I2CDev.SlaveAddr) {
	    offscreenImages->num_attributes = nElems(SMI_VideoAttributes);
	    offscreenImages->attributes = SMI_VideoAttributes; 
	} else {
	    offscreenImages->num_attributes = 
		nElems(SMI_VideoAttributesSAA711x);
	    offscreenImages->attributes = SMI_VideoAttributesSAA711x; 
	}
	xf86XVRegisterOffscreenImages(pScreen, offscreenImages, 1);

	LEAVE_PROC("SMI_InitOffscreenImages");
}

static FBAreaPtr
SMI_AllocateMemory(
	ScrnInfoPtr	pScrn,
	FBAreaPtr	area,
	int			numLines
)
{
	ScreenPtr pScreen = screenInfo.screens[pScrn->scrnIndex];

	ENTER_PROC("SMI_AllocateMemory");

	if (area != NULL)
	{
		if ((area->box.y2 - area->box.y1) >= numLines)
		{
            LEAVE_PROC("SMI_AllocateMemory (area->box.y2 - area->box.y1) >= numLines ok");
			return(area);
		}

		if (xf86ResizeOffscreenArea(area, pScrn->displayWidth, numLines))
		{
            LEAVE_PROC("SMI_AllocateMemory xf86ResizeOffscreenArea ok");
			return(area);
		}

		xf86FreeOffscreenArea(area);
	}

	area = xf86AllocateOffscreenArea(pScreen, pScrn->displayWidth, numLines, 0,
			NULL, NULL, NULL);

	if (area == NULL)
	{
		int maxW, maxH;

		xf86QueryLargestOffscreenArea(pScreen, &maxW, &maxH, 0,
				FAVOR_WIDTH_THEN_AREA, PRIORITY_EXTREME);

	DEBUG((VERBLEV, "QueryLargestOffscreenArea maxW=%d maxH=%d displayWidth=%d numlines=%d\n",
	       maxW, maxH, pScrn->displayWidth, numLines));
		if ((maxW < pScrn->displayWidth) || (maxH < numLines))
		{
            LEAVE_PROC("SMI_AllocateMemory (maxW < pScrn->displayWidth) || (maxH < numLines)");
			return(NULL);
		}

		xf86PurgeUnlockedOffscreenAreas(pScreen);
		area = xf86AllocateOffscreenArea(pScreen, pScrn->displayWidth, numLines,
				0, NULL, NULL, NULL);
	}

    DEBUG((VERBLEV, "area = %p\n", area));
	LEAVE_PROC("SMI_AllocateMemory");
	return(area);
}

static void
SMI_CopyData(
	unsigned char	*src,
	unsigned char	*dst,
	int				srcPitch,
	int				dstPitch,
	int				height,
	int				width
)
{
	ENTER_PROC("SMI_CopyData");

	while (height-- > 0)
	{
		memcpy(dst, src, width);
		src += srcPitch;
		dst += dstPitch;
	}

	LEAVE_PROC("SMI_CopyData");
}

static void
SMI_CopyYV12Data(
	unsigned char	*src1,
	unsigned char	*src2,
	unsigned char	*src3,
	unsigned char	*dst,
	int				srcPitch1,
	int				srcPitch2,
	int				dstPitch,
	int				height,
	int				width
)
{
	CARD32 *pDst = (CARD32 *) dst;
	int i, j;

	ENTER_PROC("SMI_CopyYV12Data");

	for (j = 0; j < height; j++)
	{
		for (i =0; i < width; i++)
		{
			pDst[i] = src1[i << 1] | (src1[(i << 1) + 1] << 16) |
					(src3[i] << 8) | (src2[i] << 24);
		}
		pDst += dstPitch >> 2;
		src1 += srcPitch1;
		if (j & 1)
		{
			src2 += srcPitch2;
			src3 += srcPitch2;
		}
	}

	LEAVE_PROC("SMI_CopyYV12Data");
}

static int
SMI_AllocSurface(
	ScrnInfoPtr		pScrn,
	int				id,
	unsigned short	width,
	unsigned short	height,
	XF86SurfacePtr	surface
)
{
	SMIPtr pSmi = SMIPTR(pScrn);
	int numLines, pitch, fbPitch, bpp;
	SMI_OffscreenPtr ptrOffscreen;
	FBAreaPtr area;

	ENTER_PROC("SMI_AllocSurface");

	if ((width > pSmi->lcdWidth) || (height > pSmi->lcdHeight))
	{
		LEAVE_PROC("SMI_AllocSurface");
		return(BadAlloc);
	}

	if (pSmi->Bpp == 3)
	{
		fbPitch = pSmi->Stride;
	}
	else
	{
		fbPitch = pSmi->Stride * pSmi->Bpp;
	}

	width = (width + 1) & ~1;
	switch (id)
	{
		case FOURCC_YV12:
		case FOURCC_I420:
		case FOURCC_YUY2:
		case FOURCC_RV15:
		case FOURCC_RV16:
			bpp = 2;
			break;

		case FOURCC_RV24:
			bpp = 3;
			break;

		case FOURCC_RV32:
			bpp = 4;
			break;

		default:
			LEAVE_PROC("SMI_AllocSurface");
			return(BadAlloc);
	}
	pitch = (width * bpp + 15) & ~15;

	numLines = ((height * pitch) + fbPitch - 1) / fbPitch;

	area = SMI_AllocateMemory(pScrn, NULL, numLines);
	if (area == NULL)
	{
		LEAVE_PROC("SMI_AllocSurface");
		return(BadAlloc);
	}

	surface->pitches = xalloc(sizeof(int));
	if (surface->pitches == NULL)
	{
		xf86FreeOffscreenArea(area);
		LEAVE_PROC("SMI_AllocSurface");
		return(BadAlloc);
	}
	surface->offsets = xalloc(sizeof(int));
	if (surface->offsets == NULL)
	{
		xfree(surface->pitches);
		xf86FreeOffscreenArea(area);
		LEAVE_PROC("SMI_AllocSurface");
		return(BadAlloc);
	}

	ptrOffscreen = xalloc(sizeof(SMI_OffscreenRec));
	if (ptrOffscreen == NULL)
	{
		xfree(surface->offsets);
		xfree(surface->pitches);
		xf86FreeOffscreenArea(area);
		LEAVE_PROC("SMI_AllocSurface");
		return(BadAlloc);
	}

	surface->pScrn = pScrn;
	surface->id = id;
	surface->width = width;
	surface->height = height;
	surface->pitches[0] = pitch;
	surface->offsets[0] = area->box.y1 * fbPitch;
	surface->devPrivate.ptr = (pointer) ptrOffscreen;

	ptrOffscreen->area = area;
	ptrOffscreen->isOn = FALSE;

	LEAVE_PROC("SMI_AllocSurface");
	return(Success);
}

static int
SMI_FreeSurface(
	XF86SurfacePtr	surface
)
{
	SMI_OffscreenPtr ptrOffscreen = (SMI_OffscreenPtr) surface->devPrivate.ptr;

	ENTER_PROC("SMI_FreeSurface");

	if (ptrOffscreen->isOn)
	{
		SMI_StopSurface(surface);
	}

	xf86FreeOffscreenArea(ptrOffscreen->area);
	xfree(surface->pitches);
	xfree(surface->offsets);
	xfree(surface->devPrivate.ptr);

	LEAVE_PROC("SMI_FreeSurface");
	return(Success);
}

static int
SMI_DisplaySurface(
	XF86SurfacePtr	surface,
	short			vid_x,
	short			vid_y,
	short			drw_x,
	short			drw_y,
	short			vid_w,
	short			vid_h,
	short			drw_w,
	short			drw_h,
	RegionPtr		clipBoxes
)
{
	SMI_OffscreenPtr ptrOffscreen = (SMI_OffscreenPtr) surface->devPrivate.ptr;
	SMIPtr pSmi = SMIPTR(surface->pScrn);
    SMI_PortPtr pPort = pSmi->ptrAdaptor->pPortPrivates[0].ptr;
	INT32 x1, y1, x2, y2;
	BoxRec dstBox;

	ENTER_PROC("SMI_DisplaySurface");

	x1 = vid_x;
	x2 = vid_x + vid_w;
	y1 = vid_y;
	y2 = vid_y + vid_h;

	dstBox.x1 = drw_x;
	dstBox.x2 = drw_x + drw_w;
	dstBox.y1 = drw_y;
	dstBox.y2 = drw_y + drw_h;

	if (!SMI_ClipVideo(surface->pScrn, &dstBox, &x1, &y1, &x2, &y2, clipBoxes,
			surface->width, surface->height))
	{
		LEAVE_PROC("SMI_DisplaySurface");
		return(Success);
	}

	dstBox.x1 -= surface->pScrn->frameX0;
	dstBox.y1 -= surface->pScrn->frameY0;
	dstBox.x2 -= surface->pScrn->frameX0;
	dstBox.y2 -= surface->pScrn->frameY0;

#if USE_XAA
    XAAFillSolidRects(surface->pScrn, pPort->Attribute[XV_COLORKEY], GXcopy, ~0,
			REGION_NUM_RECTS(clipBoxes), REGION_RECTS(clipBoxes));
#else
	/*aaa*/
	return(BadAlloc);
#endif

	SMI_ResetVideo(surface->pScrn);
	SMI_DisplayVideo(surface->pScrn, surface->id, surface->offsets[0],
			surface->width, surface->height, surface->pitches[0], x1, y1, x2,
			y2, &dstBox, vid_w, vid_h, drw_w, drw_h);

	ptrOffscreen->isOn = TRUE;
    if (pPort->videoStatus & CLIENT_VIDEO_ON)
	{
        REGION_EMPTY(pScrn->pScreen, &pPort->clip);
		UpdateCurrentTime();
        pPort->videoStatus = FREE_TIMER;
        pPort->freeTime = currentTime.milliseconds + FREE_DELAY;
	}

	LEAVE_PROC("SMI_DisplaySurface");
	return(Success);
}

static int
SMI_StopSurface(
	XF86SurfacePtr	surface
)
{
	SMI_OffscreenPtr ptrOffscreen = (SMI_OffscreenPtr) surface->devPrivate.ptr;

	ENTER_PROC("SMI_StopSurface");

	if (ptrOffscreen->isOn)
	{
		SMIPtr pSmi = SMIPTR(surface->pScrn);
		WRITE_VPR(pSmi, 0x00, READ_VPR(pSmi, 0x00) & ~0x00000008);
		ptrOffscreen->isOn = FALSE;
	}

	LEAVE_PROC("SMI_StopSurface");
	return(Success);
}

static int
SMI_GetSurfaceAttribute(
	ScrnInfoPtr	pScrn,
	Atom		attr,
	INT32		*value
)
{
	SMIPtr pSmi = SMIPTR(pScrn);

	return(SMI_GetPortAttribute(pScrn, attr, value,
			(pointer) pSmi->ptrAdaptor->pPortPrivates[0].ptr));
}

static int
SMI_SetSurfaceAttribute(
	ScrnInfoPtr	pScrn,
	Atom		attr,
	INT32		value
)
{
	SMIPtr pSmi = SMIPTR(pScrn);

	return(SMI_SetPortAttribute(pScrn, attr, value,
			(pointer) pSmi->ptrAdaptor->pPortPrivates[0].ptr));
}
#else /* XvExtension */
void SMI_InitVideo(ScreenPtr pScreen) {}
#endif
