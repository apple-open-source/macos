/*
 *  Permedia 2 Xv Driver
 *
 *  Copyright (C) 1998-2000 Michael H. Schimek <m.schimek@netway.at>
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 */
 
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/glint/pm2_video.c,v 1.23 2002/12/02 22:52:30 tsi Exp $ */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86Pci.h"
#include "xf86PciInfo.h"
#include "xf86fbman.h"
#include "xf86i2c.h"
#include "xf86xv.h"
#include "Xv.h"

#include "glint_regs.h"
#include "glint.h"

#ifndef XvExtension

void Permedia2VideoInit(ScreenPtr pScreen) {}
void Permedia2VideoUninit(ScrnInfoPtr pScrn) {}
void Permedia2VideoEnterVT(ScrnInfoPtr pScrn) {}
void Permedia2VideoLeaveVT(ScrnInfoPtr pScrn) {}

#else

#undef MIN
#undef ABS
#undef CLAMP
#undef ENTRIES

#define MIN(a, b) (((a) < (b)) ? (a) : (b)) 
#define ABS(n) (((n) < 0) ? -(n) : (n))
#define CLAMP(v, min, max) (((v) < (min)) ? (min) : MIN(v, max))
#define ENTRIES(array) (sizeof(array) / sizeof((array)[0]))

#define ADAPTORS 3
#define PORTS 6

#define PCI_SUBSYSTEM_ID_WINNER_2000_P2C	0x0a311048
#define PCI_SUBSYSTEM_ID_GLORIA_SYNERGY_P2C	0x0a321048
#define PCI_SUBSYSTEM_ID_GLORIA_SYNERGY_P2A	0x0a351048
#define PCI_SUBSYSTEM_ID_WINNER_2000_P2A	0x0a441048

/*
 *  Proprietary kernel backbone interface
 */

#define XVIPC_MAGIC		0x6A5D70E6
#define XVIPC_VERSION		1
#define VIDIOC_PM2_XVIPC	0x00007F7F

typedef enum {
    OP_ATTR = 0,
    OP_RESET = 8,	/* unused */
    OP_START,
    OP_STOP,
    OP_PLUG,
    OP_VIDEOSTD,
    OP_WINDOW,		/* unused */
    OP_CONNECT,
    OP_EVENT,
    OP_ALLOC,
    OP_FREE,
    OP_UPDATE,
    OP_NOP,		/* ignored */
    OP_ENTER,
    OP_LEAVE,
    OP_DISCONNECT
} xvipc_op;

typedef struct _pm2_xvipc {
    int			magic;
    void 		*pm2p, *pAPriv;
    int			port, op, time, block;
    int			a, b, c, d, e, f;
} pm2_xvipc;

static pm2_xvipc xvipc;
static int xvipc_fd = -1;


#define MAX_BUFFERS 2

typedef struct {
    CARD32			xy, wh;				/* 16.0 16.0 */
    INT32			s, t;				/* 12.20 fp */
    short			y1, y2;
} CookieRec, *CookiePtr;

typedef struct _PortPrivRec {
    struct _AdaptorPrivRec *    pAdaptor;
    I2CDevRec                   I2CDev;

    INT32			Attribute[8];			/* Brig, Con, Sat, Hue, Int, Filt, BkgCol, Alpha */

    int				BuffersRequested;
    int				BuffersAllocated;
    FBAreaPtr			pFBArea[MAX_BUFFERS];
    CARD32			BufferBase[MAX_BUFFERS];	/* FB byte offset */
    CARD32			BufferStride;			/* bytes */
    CARD32			BufferPProd;			/* PProd(BufferStride in buffer pixels) */

    INT32			vx, vy, vw, vh;			/* 12.10 fp */
    int				dx, dy, dw, dh;
    int				fw, fh;

    CookiePtr			pCookies;
    int				nCookies;
    INT32			dS, dT;				/* 12.20 fp */

    int				Id, Bpp;			/* Scaler */

    int                         Plug;
    int				BkgCol;				/* RGB 5:6:5; 5:6:5 */
    Bool			StreamOn;			/* buffer <-> hardware */
    int				VideoOn;			/* buffer <-> screen */
    int				VideoStdReq;

    int				StopDelay;
    
    int				FramesPerSec, FrameAcc;

} PortPrivRec, *PortPrivPtr;

enum { VIDEO_OFF, VIDEO_ONE_SHOT, VIDEO_ON };

typedef struct _LFBAreaRec {
    struct _LFBAreaRec *	Next;
    int				Linear;
    FBAreaPtr			pFBArea;
} LFBAreaRec, *LFBAreaPtr;

typedef struct _AdaptorPrivRec {
    struct _AdaptorPrivRec *	Next;
    ScrnInfoPtr			pScrn;

    void *			pm2p;
    LFBAreaPtr			LFBList;

    CARD32			dFifoControl;
    CARD32			dDitherMode;
    CARD32			dAlphaBlendMode;
    CARD32			dTextureDataFormat;

    OsTimerPtr			Timer;
    int				TimerUsers;
    int				Delay, Instant;

    int				FramesPerSec;
    int				FrameLines;
    int				IntLine;			/* Frame, not field */
    int				LinePer;			/* nsec */

    Bool			VideoIO;
    int                         VideoStd;

    PortPrivRec                 Port[PORTS];

} AdaptorPrivRec, *AdaptorPrivPtr;

static AdaptorPrivPtr AdaptorPrivList = NULL;

#define FreeCookies(pPPriv)		\
do {					\
    if ((pPPriv)->pCookies) {		\
        xfree((pPPriv)->pCookies);	\
	(pPPriv)->pCookies = NULL;	\
    }					\
} while (0)

#define PORTNUM(p) ((int)((p) - &pAPriv->Port[0]))
#define BPPSHIFT(g) (2 - (g)->BppShift)	/* Bytes per pixel = 1 << BPPSHIFT(pGlint) */

#define DEBUG(x)

static const Bool ColorBars = FALSE;


/*
 *  XF86Config VideoAdaptor options
 */

typedef enum {
    OPTION_DEVICE,
    OPTION_FPS,
    OPTION_BUFFERS,
    OPTION_ENCODING,
    OPTION_EXPOSE	/* obsolete, ignored */
} OptToken;

/* XXX These should be made const, and per-screen/adaptor copies processed. */
static OptionInfoRec AdaptorOptions[] = {
    { OPTION_DEVICE,		"Device",	OPTV_STRING,	{0}, FALSE },
    { -1,			NULL,		OPTV_NONE,	{0}, FALSE }
};
static OptionInfoRec InputOptions[] = {
    { OPTION_BUFFERS,		"Buffers",	OPTV_INTEGER,	{0}, FALSE },
    { OPTION_FPS,		"FramesPerSec", OPTV_INTEGER,	{0}, FALSE },
    { OPTION_ENCODING,		"Encoding",	OPTV_STRING,	{0}, FALSE },
    { -1,			NULL,		OPTV_NONE,	{0}, FALSE }
};
static OptionInfoRec OutputOptions[] = {
    { OPTION_BUFFERS,		"Buffers",	OPTV_INTEGER,	{0}, FALSE },
    { OPTION_EXPOSE,		"Expose",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_FPS,		"FramesPerSec", OPTV_INTEGER,	{0}, FALSE },
    { OPTION_ENCODING,		"Encoding",	OPTV_STRING,	{0}, FALSE },
    { -1,			NULL,		OPTV_NONE,	{0}, FALSE }
};


/*
 *  Attributes
 */
 
#define XV_ENCODING	"XV_ENCODING"
#define XV_BRIGHTNESS	"XV_BRIGHTNESS"
#define XV_CONTRAST 	"XV_CONTRAST"
#define XV_SATURATION	"XV_SATURATION"
#define XV_HUE		"XV_HUE"

/* Proprietary */

#define XV_INTERLACE	"XV_INTERLACE"	/* Interlaced (bool) */
#define XV_FILTER	"XV_FILTER"	/* Bilinear filter (bool) */
#define XV_BKGCOLOR	"XV_BKGCOLOR"	/* Output background (0x00RRGGBB) */
#define XV_ALPHA	"XV_ALPHA"	/* Scaler alpha channel (bool) */

#define MAKE_ATOM(a) MakeAtom(a, sizeof(a) - 1, TRUE)

static Atom xvEncoding, xvBrightness, xvContrast, xvSaturation, xvHue;
static Atom xvInterlace, xvFilter, xvBkgColor, xvAlpha;

/* Input */

static XF86VideoEncodingRec
InputVideoEncodings[] =
{
    { 0, "pal-composite",		704, 576, { 1, 50 }},
    { 1, "pal-composite_adaptor",	704, 576, { 1, 50 }},
    { 2, "pal-svideo",			704, 576, { 1, 50 }},
    { 3, "ntsc-composite",		704, 480, { 1001, 60000 }},
    { 4, "ntsc-composite_adaptor",	704, 480, { 1001, 60000 }},
    { 5, "ntsc-svideo",			704, 480, { 1001, 60000 }},
    { 6, "secam-composite",		704, 576, { 1, 50 }},
    { 7, "secam-composite_adaptor",	704, 576, { 1, 50 }},
    { 8, "secam-svideo",		704, 576, { 1, 50 }},
};

static XF86AttributeRec
InputVideoAttributes[] =
{
    { XvSettable | XvGettable, -1000, +1000, XV_BRIGHTNESS },
    { XvSettable | XvGettable, -3000, +1000, XV_CONTRAST },
    { XvSettable | XvGettable, -3000, +1000, XV_SATURATION },
    { XvSettable | XvGettable, -1000, +1000, XV_HUE },
    { XvSettable | XvGettable, 0, 2, XV_INTERLACE },
    { XvSettable | XvGettable, 0, 1, XV_FILTER },
};

static XF86VideoFormatRec
InputVideoFormats[] =
{
    { 8,  TrueColor }, /* Dithered */
    { 15, TrueColor },
    { 16, TrueColor },
    { 24, TrueColor },
};

/* Output */

static XF86VideoEncodingRec
OutputVideoEncodings[] =
{
    { 0, "pal-composite_adaptor",	704, 576, { 1, 50 }},
    { 1, "pal-svideo",			704, 576, { 1, 50 }},
    { 2, "ntsc-composite_adaptor",	704, 480, { 1001, 60000 }},
    { 3, "ntsc-svideo",			704, 480, { 1001, 60000 }},
};

static XF86AttributeRec
OutputVideoAttributes[] =
{
    { XvSettable | XvGettable, 0, 2, XV_INTERLACE },
    { XvSettable | XvGettable, 0, 1, XV_FILTER },
    { XvSettable | XvGettable, 0x000000, 0xFFFFFF, XV_BKGCOLOR },
};

static XF86VideoFormatRec
OutputVideoFormats[] =
{
    { 8,  TrueColor },
    { 8,  PseudoColor }, /* Using .. */
    { 8,  StaticColor },
    { 8,  GrayScale },
    { 8,  StaticGray }, /* .. TexelLUT */
    { 15, TrueColor },
    { 16, TrueColor },
    { 24, TrueColor },
};

/* Scaler */

static XF86VideoEncodingRec
ScalerEncodings[] =
{
    { 0, "XV_IMAGE", 2047, 2047, { 1, 1 }},
};

static XF86AttributeRec
ScalerAttributes[] =
{
    { XvSettable | XvGettable, 0, 1, XV_FILTER },
    { XvSettable | XvGettable, 0, 1, XV_ALPHA },
};

#define ScalerVideoFormats InputVideoFormats

/*
 *  FOURCC from http://www.webartz.com/fourcc
 *  Generic GUID for legacy FOURCC XXXXXXXX-0000-0010-8000-00AA00389B71
 */
#define LE4CC(a,b,c,d) (((CARD32)(a)&0xFF)|(((CARD32)(b)&0xFF)<<8)|(((CARD32)(c)&0xFF)<<16)|(((CARD32)(d)&0xFF)<<24))
#define GUID4CC(a,b,c,d) { a,b,c,d,0,0,0,0x10,0x80,0,0,0xAA,0,0x38,0x9B,0x71 }

#define NoOrder LSBFirst

static XF86ImageRec
ScalerImages[] =
{
    /* Planar YVU 4:2:0 (emulated) */
    { LE4CC('Y','V','1','2'), XvYUV, NoOrder, GUID4CC('Y','V','1','2'),
      12, XvPlanar, 3, 0, 0, 0, 0,
      8, 8, 8,  1, 2, 2,  1, 2, 2, "YVU", XvTopToBottom },

    /* Packed YUYV 4:2:2 */
    { LE4CC('Y','U','Y','2'), XvYUV, NoOrder, GUID4CC('Y','U','Y','2'),
      16, XvPacked, 1, 0, 0, 0, 0,
      8, 8, 8,  1, 2, 2,  1, 1, 1, "YUYV", XvTopToBottom },

    /* Packed UYVY 4:2:2 */
    { LE4CC('U','Y','V','Y'), XvYUV, NoOrder, GUID4CC('U','Y','V','Y'),
      16, XvPacked, 1, 0, 0, 0, 0,
      8, 8, 8,  1, 2, 2,  1, 1, 1, "UYVY", XvTopToBottom },

    /* Packed YUVA 4:4:4 */
    { LE4CC('Y','U','V','A') /* XXX not registered */, XvYUV, LSBFirst, { 0 },
      32, XvPacked, 1, 0, 0, 0, 0,
      8, 8, 8,  1, 1, 1,  1, 1, 1, "YUVA", XvTopToBottom },

    /* Packed VUYA 4:4:4 */
    { LE4CC('V','U','Y','A') /* XXX not registered */, XvYUV, LSBFirst, { 0 },
      32, XvPacked, 1, 0, 0, 0, 0,
      8, 8, 8,  1, 1, 1,  1, 1, 1, "VUYA", XvTopToBottom },

    /* RGBA 8:8:8:8 */
    { 0x41, XvRGB, LSBFirst, { 0 },
      32, XvPacked, 1, 24, 0x0000FF, 0x00FF00, 0xFF0000, 
      0, 0, 0,  0, 0, 0,  0, 0, 0, "RGBA", XvTopToBottom },

    /* RGB 5:6:5 */
    { 0x42, XvRGB, LSBFirst, { 0 },
      16, XvPacked, 1, 16, 0x001F, 0x07E0, 0xF800, 
      0, 0, 0,  0, 0, 0,  0, 0, 0, "RGB", XvTopToBottom },

    /* RGBA 5:5:5:1 */
    { 0x43, XvRGB, LSBFirst, { 0 },
      16, XvPacked, 1, 15, 0x001F, 0x03E0, 0x7C00, 
      0, 0, 0,  0, 0, 0,  0, 0, 0, "RGBA", XvTopToBottom },

    /* RGBA 4:4:4:4 */
    { 0x44, XvRGB, LSBFirst, { 0 },
      16, XvPacked, 1, 12, 0x000F, 0x00F0, 0x0F00, 
      0, 0, 0,  0, 0, 0,  0, 0, 0, "RGBA", XvTopToBottom },

    /* RGBA 2:3:2:1 */
    { 0x45, XvRGB, NoOrder, { 0 },
      8, XvPacked, 1, 7, 0x03, 0x1C, 0x60, 
      0, 0, 0,  0, 0, 0,  0, 0, 0, "RGBA", XvTopToBottom },

    /* RGB 3:3:2 */
    { 0x46, XvRGB, NoOrder, { 0 },
      8, XvPacked, 1, 8, 0x07, 0x38, 0xC0, 
      0, 0, 0,  0, 0, 0,  0, 0, 0, "RGB", XvTopToBottom },

    /* BGRA 8:8:8:8 */
    { 0x47, XvRGB, LSBFirst, { 0 },
      32, XvPacked, 1, 24, 0xFF0000, 0x00FF00, 0x0000FF,
      0, 0, 0,  0, 0, 0,  0, 0, 0, "BGRA", XvTopToBottom },

    /* BGR 5:6:5 */
    { 0x48, XvRGB, LSBFirst, { 0 },
      16, XvPacked, 1, 16, 0xF800, 0x07E0, 0x001F,
      0, 0, 0,  0, 0, 0,  0, 0, 0, "BGR", XvTopToBottom },

    /* BGRA 5:5:5:1 */
    { 0x49, XvRGB, LSBFirst, { 0 },
      16, XvPacked, 1, 15, 0x7C00, 0x03E0, 0x001F,
      0, 0, 0,  0, 0, 0,  0, 0, 0, "BGRA", XvTopToBottom },

    /* BGRA 4:4:4:4 */
    { 0x4A, XvRGB, LSBFirst, { 0 },
      16, XvPacked, 1, 12, 0x0F00, 0x00F0, 0x000F,
      0, 0, 0,  0, 0, 0,  0, 0, 0, "BGRA", XvTopToBottom },

    /* BGRA 2:3:2:1 */
    { 0x4B, XvRGB, NoOrder, { 0 },
      8, XvPacked, 1, 7, 0x60, 0x1C, 0x03,
      0, 0, 0,  0, 0, 0,  0, 0, 0, "BGRA", XvTopToBottom },

    /* BGR 2:3:3 */
    { 0x4C, XvRGB, NoOrder, { 0 },
      8, XvPacked, 1, 8, 0xC0, 0x38, 0x07,
      0, 0, 0,  0, 0, 0,  0, 0, 0, "BGR", XvTopToBottom },
};


/*
 *  Video codec tables
 */

#define SAA7111_SLAVE_ADDRESS 0x48
#define SAA7125_SLAVE_ADDRESS 0x88

static I2CByte
DecInitVec[] =
{
    0x11, 0x00,
    0x02, 0xC1, 0x03, 0x00, 0x04, 0x00, 0x05, 0x00,
    0x06, 0x00, 0x07, 0x00, 0x08, 0x00, 0x09, 0x4A,
    0x0A, 0x80, 0x0B, 0x40, 0x0C, 0x40, 0x0D, 0x00,
    0x0E, 0x01, 0x10, 0xC8, 0x12, 0x20,
    0x13, 0x00, 0x15, 0x00, 0x16, 0x00, 0x17, 0x00,
};

static I2CByte
EncInitVec[] =
{
    0x3A, 0x83, 0x61, 0xC2,
    0x5A, 119,  0x5B, 0x7D,
    0x5C, 0xAF, 0x5D, 0x3C, 0x5E, 0x3F, 0x5F, 0x3F,
    0x60, 0x70, 0x62, 0x4B, 0x67, 0x00,
    0x68, 0x00, 0x69, 0x00, 0x6A, 0x00, 0x6B, 0x20,
    0x6C, 0x03, 0x6D, 0x30, 0x6E, 0xA0, 0x6F, 0x00,
    0x70, 0x80, 0x71, 0xE8, 0x72, 0x10,
    0x7A, 0x13, 0x7B, 0xFB, 0x7C, 0x00, 0x7D, 0x00,
};

static I2CByte Dec02[3] = { 0xC1, 0xC0, 0xC4 };
static I2CByte Dec09[3] = { 0x4A, 0x4A, 0xCA };
static I2CByte Enc3A[3] = { 0x03, 0x03, 0x23 };
static I2CByte Enc61[3] = { 0x06, 0x01, 0xC2 };

static I2CByte
DecVS[3][8] =
{
    { 0x06, 108, 0x07, 108, 0x08, 0x09, 0x0E, 0x01 },
    { 0x06, 107, 0x07, 107, 0x08, 0x49, 0x0E, 0x01 },
    { 0x06, 108, 0x07, 108, 0x08, 0x01, 0x0E, 0x51 }
};

#define FSC(n) ((CARD32)((n) / 27e6 * 4294967296.0 + .5))
#define SUBCARRIER_FREQ_PAL  (4.433619e6)
#define SUBCARRIER_FREQ_NTSC (3.579545e6)

static I2CByte
EncVS[2][14] =
{
    { 0x62, 0x4B, 0x6B, 0x28, 0x6E, 0xA0,
      0x63, (I2CByte)(FSC(SUBCARRIER_FREQ_PAL) >> 0),
      0x64, (I2CByte)(FSC(SUBCARRIER_FREQ_PAL) >> 8),
      0x65, (I2CByte)(FSC(SUBCARRIER_FREQ_PAL) >> 16),
      0x66, (I2CByte)(FSC(SUBCARRIER_FREQ_PAL) >> 24) },
    { 0x62, 0x6A, 0x6B, 0x20, 0x6E, 0x20,
      0x63, (I2CByte)(FSC(SUBCARRIER_FREQ_NTSC) >> 0),
      0x64, (I2CByte)(FSC(SUBCARRIER_FREQ_NTSC) >> 8),
      0x65, (I2CByte)(FSC(SUBCARRIER_FREQ_NTSC) >> 16),
      0x66, (I2CByte)(FSC(SUBCARRIER_FREQ_NTSC) >> 24) }
};

/* Forward */
static void StopVideoStream(PortPrivPtr pPPriv, Bool shutdown);
static void RestoreVideoStd(AdaptorPrivPtr pAPriv);
static Bool xvipcHandshake(PortPrivPtr pPPriv, int op, Bool block);


/*
 *  Video codec controls
 */

static int
SetAttr(PortPrivPtr pPPriv, int i, int value)
{
    AdaptorPrivPtr pAPriv = pPPriv->pAdaptor;
    int v;

    if (value < InputVideoAttributes[i].min_value)
	value = InputVideoAttributes[i].min_value;
    else
    if (value > InputVideoAttributes[i].max_value)
	value = InputVideoAttributes[i].max_value;

    switch (i) {
    case 0:
        v = 128 + (MIN(value, 999) * 128) / 1000;
        break;

    case 1:
    case 2:
        v = 64 + (MIN(value, 999) * 64) / 1000;
        break;

    default:
        v = (MIN(value, 999) * 128) / 1000;
        break;
    }

    if (pAPriv->pm2p) {
	xvipc.a = v << 8;

	if (!xvipcHandshake(pPPriv, OP_ATTR + i, TRUE))
	    return XvBadAlloc;
    } else
	if (!xf86I2CWriteByte(&pPPriv->I2CDev, 0x0A + i, v))
	    return XvBadAlloc;

    pPPriv->Attribute[i] = value;
    
    return Success;
}

static int
SetPlug(PortPrivPtr pPPriv, int Plug)
{
    AdaptorPrivPtr pAPriv = pPPriv->pAdaptor;

    if (pAPriv->pm2p) {
	xvipc.a = Plug - (pPPriv == &pAPriv->Port[1]);

	if (!xvipcHandshake(pPPriv, OP_PLUG, TRUE))
	    return XvBadAlloc;
    } else {
	if (pPPriv == &pAPriv->Port[0]) {
	    if (!xf86I2CWriteByte(&pPPriv->I2CDev, 0x02, Dec02[Plug]) ||
		!xf86I2CWriteByte(&pPPriv->I2CDev, 0x09, Dec09[Plug]))
		return XvBadAlloc;
	} else {
	    if (pPPriv->StreamOn) {
		if (!xf86I2CWriteByte(&pPPriv->I2CDev, 0x3A, Enc3A[Plug]))
		    return XvBadAlloc;
	    } else
		if (ColorBars)
		    xf86I2CWriteByte(&pPPriv->I2CDev, 0x3A, 0x83);
	}
    }

    pPPriv->Plug = Plug;

    return Success;
}

enum { PAL, NTSC, SECAM };

static int
SetVideoStd(PortPrivPtr pPPriv, int VideoStd)
{
    AdaptorPrivPtr pAPriv = pPPriv->pAdaptor;
    int r = Success;

    if (pAPriv->pm2p) {
	xvipc.a = VideoStd;
	
	if (!xvipcHandshake(&pAPriv->Port[0], OP_VIDEOSTD, TRUE))
	    return XvBadAlloc;
	
	VideoStd = xvipc.a; /* Actual */
    } else {
	if (VideoStd == SECAM)
	    xf86I2CWriteByte(&pAPriv->Port[1].I2CDev, 0x61, 0xC2);
	    /* Disable output, SECAM not supported */

	if (!xf86I2CWriteVec(&pAPriv->Port[0].I2CDev, &DecVS[VideoStd][0], 4)) {
	    pAPriv->VideoStd = -1;
	    return XvBadAlloc;
	}

	if (VideoStd != SECAM)
	    if (!xf86I2CWriteVec(&pAPriv->Port[1].I2CDev, &EncVS[VideoStd][0], 7)) {
		pAPriv->VideoStd = -1;
		return XvBadAlloc;
	    }
    }

    pAPriv->VideoStd = VideoStd;
    pPPriv->VideoStdReq = VideoStd;

    if (VideoStd == NTSC) {
	pAPriv->FramesPerSec = 30;
	pAPriv->FrameLines = 525;
	pAPriv->IntLine = 513;
	pAPriv->LinePer = 63555;
    } else {
	pAPriv->FramesPerSec = 25;
	pAPriv->FrameLines = 625;
	pAPriv->IntLine = 613;
	pAPriv->LinePer = 64000;
    }

#if 0 /* XF86Config option */

    pAPriv->Port[0].FramesPerSec = pAPriv->FramesPerSec;
    pAPriv->Port[1].FramesPerSec = pAPriv->FramesPerSec;

#endif

    return r;
}


/*
 *  Buffer management
 */

static void
RemoveAreaCallback(FBAreaPtr pFBArea)
{
    PortPrivPtr pPPriv = (PortPrivPtr) pFBArea->devPrivate.ptr;
    AdaptorPrivPtr pAPriv = pPPriv->pAdaptor;
    DEBUG(ScrnInfoPtr pScrn = pAPriv->pScrn;)
    int i;

    for (i = 0; i < MAX_BUFFERS && pPPriv->pFBArea[i] != pFBArea; i++);

    if (i >= MAX_BUFFERS)
	return;

    DEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 4,
	"RemoveAreaCallback port #%d, buffer #%d, pFB=%p, off=0x%08x\n",
	PORTNUM(pPPriv), i, pPPriv->pFBArea[i], pPPriv->BufferBase[i]));

    if (pAPriv->VideoIO && PORTNUM(pPPriv) < 2) {
        StopVideoStream(pPPriv, FALSE);
    }

    for (; i < MAX_BUFFERS - 1; i++)
	pPPriv->pFBArea[i] = pPPriv->pFBArea[i + 1];

    pPPriv->pFBArea[MAX_BUFFERS - 1] = NULL;

    pPPriv->BuffersAllocated--;
}

static void
RemoveableBuffers(PortPrivPtr pPPriv, Bool remove)
{
    int i;

    for (i = 0; i < MAX_BUFFERS; i++)
	if (pPPriv->pFBArea[i])
	    pPPriv->pFBArea[i]->RemoveAreaCallback =
		remove ? RemoveAreaCallback : NULL;
}

static void
FreeBuffers(PortPrivPtr pPPriv)
{
    DEBUG(AdaptorPrivPtr pAPriv = pPPriv->pAdaptor;)
    DEBUG(ScrnInfoPtr pScrn = pAPriv->pScrn;)
    int i;

    RemoveableBuffers(pPPriv, FALSE);

    for (i = MAX_BUFFERS - 1; i >= 0; i--)
	if (pPPriv->pFBArea[i]) {
	    DEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 4,
		"FreeBuffers port #%d, buffer #%d, pFB=%p, off=0x%08x\n",
		PORTNUM(pPPriv), i, pPPriv->pFBArea[i], pPPriv->BufferBase[i]));

	    xf86FreeOffscreenArea(pPPriv->pFBArea[i]);

	    pPPriv->pFBArea[i] = NULL;
	}

    pPPriv->BuffersAllocated = 0;
}

enum { FORCE_LINEAR = 1, FORCE_RECT };

static int
AllocateBuffers(PortPrivPtr pPPriv,
    int w, int h, int bytespp,
    int num, int force)
{
    AdaptorPrivPtr pAPriv = pPPriv->pAdaptor;
    ScrnInfoPtr pScrn = pAPriv->pScrn;
    GLINTPtr pGlint = GLINTPTR(pScrn);
    Bool linear = (force != FORCE_RECT);
    int i, j, retry = 0;

    FreeBuffers(pPPriv);

    for (i = 0; i < num; i++) {
	if (linear) {
	    for (j = (w + 31) >> 5; partprodPermedia[j] < 0; j++);

	    pPPriv->BufferStride = j * bytespp * 32;
	    pPPriv->BufferPProd = partprodPermedia[j];

	    pPPriv->pFBArea[i] = xf86AllocateLinearOffscreenArea(pScrn->pScreen,
    		(pPPriv->BufferStride * h + (1 << BPPSHIFT(pGlint)) - 1) >> BPPSHIFT(pGlint),
		8 >> BPPSHIFT(pGlint), NULL, NULL, (pointer) pPPriv);

	    if (pPPriv->pFBArea[i])
		/* pPPriv->BufferBase[i] = pPPriv->pFBArea[i].linear; */
		pPPriv->BufferBase[i] =
		    ((pPPriv->pFBArea[i]->box.y1 * pScrn->displayWidth) +
		     pPPriv->pFBArea[i]->box.x1) << BPPSHIFT(pGlint);

	    DEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 4,
		"New linear buffer %dx%d, rec %dx%d -> pFB=%p, off=0x%08x\n",
		w, h, pPPriv->BufferStride, h, pPPriv->pFBArea[i], pPPriv->BufferBase[i]));
	} else {
	    pPPriv->BufferStride = pScrn->displayWidth << BPPSHIFT(pGlint);

	    j = pPPriv->BufferStride / bytespp;

	    if (j <= w && j <= 2048 && (j & 31) == 0 &&
		(pPPriv->BufferPProd = partprodPermedia[j >> 5]) >= 0)
	    {
		pPPriv->pFBArea[i] = xf86AllocateOffscreenArea(pScrn->pScreen,
    		    w, h, 8 >> BPPSHIFT(pGlint), NULL, NULL, (pointer) pPPriv);

		if (pPPriv->pFBArea[i])
		    pPPriv->BufferBase[i] =
			((pPPriv->pFBArea[i]->box.y1 * pScrn->displayWidth) +
		         pPPriv->pFBArea[i]->box.x1) << BPPSHIFT(pGlint);

		DEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 4,
		    "New rect buffer %dx%d, stride %d, %d -> pFB=%p, off=0x%08x\n",
		    w, h, pPPriv->BufferStride, j, pPPriv->pFBArea[i], pPPriv->BufferBase[i]));
	    }
	}

	if (pPPriv->pFBArea[i])
	    continue;

	if (!force && i == 0 && retry++ < 1) {
	    linear ^= TRUE;
	    i = -1;
	} else
	    break;
    }

    return pPPriv->BuffersAllocated = i;
}


/*
 *  Blitter
 */

static Bool
RemakePutCookies(PortPrivPtr pPPriv, RegionPtr pRegion)
{
    BoxPtr pBox;
    CookiePtr pCookie;
    int nBox;

    if (!pRegion) {
	pBox = (BoxPtr) NULL;
	nBox = pPPriv->nCookies;
    } else {
	pBox = REGION_RECTS(pRegion);
	nBox = REGION_NUM_RECTS(pRegion);

	if (!pPPriv->pCookies || pPPriv->nCookies < nBox) {
	    if (!(pCookie = (CookiePtr) xrealloc(pPPriv->pCookies, nBox * sizeof(CookieRec))))
    		return FALSE;

	    pPPriv->pCookies = pCookie;
	}
    }

    pPPriv->dS = (pPPriv->vw << 10) / pPPriv->dw;
    pPPriv->dT = (pPPriv->vh << 10) / pPPriv->dh;

    for (pCookie = pPPriv->pCookies; nBox--; pCookie++, pBox++) {
	if (pRegion) {
	    pCookie->y1 = pBox->y1;
	    pCookie->y2 = pBox->x1;
	    pCookie->xy = (pBox->y1 << 16) | pBox->x1;
	    pCookie->wh = ((pBox->y2 - pBox->y1) << 16) |
			   (pBox->x2 - pBox->x1);
	}

	pCookie->s = (pPPriv->vx << 10) + (pCookie->y2 - pPPriv->dx) * pPPriv->dS;
	pCookie->t = (pPPriv->vy << 10) + (pCookie->y1 - pPPriv->dy) * pPPriv->dT;
    }

    pPPriv->nCookies = pCookie - pPPriv->pCookies;

    return TRUE;
}

#define FORMAT_YUYV ((0 << 5) + (1 << 4) + ((19 & 0x10) << 2) + ((19 & 0x0F) << 0))
#define FORMAT_UYVY ((1 << 5) + (1 << 4) + ((19 & 0x10) << 2) + ((19 & 0x0F) << 0))
#define FORMAT_YUVA ((0 << 5) + ((18 & 0x10) << 2) + ((18 & 0x0F) << 0))
#define FORMAT_VUYA ((1 << 5) + ((18 & 0x10) << 2) + ((18 & 0x0F) << 0))

static void
PutYUV(PortPrivPtr pPPriv, int BufferBase,
    int format, int bptshift, int alpha)
{
    AdaptorPrivPtr pAPriv = pPPriv->pAdaptor;
    ScrnInfoPtr pScrn = pAPriv->pScrn;
    GLINTPtr pGlint = GLINTPTR(pScrn);
    CookiePtr pCookie = pPPriv->pCookies;
    int nCookies = pPPriv->nCookies;

    DEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 5, "PutYUV %08x %08x\n",
	BufferBase, format));

    if (!nCookies || (GLINT_READ_REG(InFIFOSpace) < 200))
	return; /* Denial of service fix, N/A for scaler */

    CHECKCLIPPING;

    GLINT_WRITE_REG(1 << 16, dY);
    GLINT_WRITE_REG(0, RasterizerMode);
    GLINT_WRITE_REG(UNIT_DISABLE, AreaStippleMode);
    GLINT_WRITE_REG(UNIT_ENABLE, TextureAddressMode);
    GLINT_WRITE_REG(pPPriv->dS, dSdx);
    GLINT_WRITE_REG(0, dSdyDom);
    GLINT_WRITE_REG(0, dTdx);
    GLINT_WRITE_REG(pPPriv->dT, dTdyDom);
    GLINT_WRITE_REG(BufferBase >> bptshift, PMTextureBaseAddress);
    GLINT_WRITE_REG((bptshift << 19) | pPPriv->BufferPProd, PMTextureMapFormat);
    GLINT_WRITE_REG(format, PMTextureDataFormat);
    GLINT_WRITE_REG((pPPriv->Attribute[5] << 17) | /* FilterMode */
		    (11 << 13) | (11 << 9) | /* TextureSize log2 */ 
		    UNIT_ENABLE, PMTextureReadMode);
    GLINT_WRITE_REG((0 << 4) /* RGB */ |
		    (3 << 1) /* Copy */ |
		    UNIT_ENABLE, TextureColorMode);
    if (alpha)
	GLINT_WRITE_REG(pAPriv->dAlphaBlendMode, AlphaBlendMode);
    GLINT_WRITE_REG(pAPriv->dDitherMode, DitherMode);
    GLINT_WRITE_REG(UNIT_DISABLE, LogicalOpMode);
    GLINT_WRITE_REG((alpha << 10) | /* ReadDestination */
		    pGlint->pprod, FBReadMode);
    GLINT_WRITE_REG(0xFFFFFFFF, FBHardwareWriteMask);
    GLINT_WRITE_REG(UNIT_ENABLE, YUVMode);

    for (; nCookies--; pCookie++) {
	GLINT_WAIT(5);
	GLINT_WRITE_REG(pCookie->xy, RectangleOrigin);
	GLINT_WRITE_REG(pCookie->wh, RectangleSize);
	GLINT_WRITE_REG(pCookie->s, SStart);
	GLINT_WRITE_REG(pCookie->t, TStart);
        GLINT_WRITE_REG(PrimitiveRectangle |
			XPositive |
			YPositive |
			TextureEnable, Render);
    }

    pGlint->x = pGlint->y = -1; /* Force reload */
    pGlint->w = pGlint->h = -1;
    pGlint->ROP = 0xFF;
    pGlint->planemask = 0xFFFFFFFF;

    GLINT_WAIT(8);
    GLINT_WRITE_REG(UNIT_DISABLE, TextureAddressMode);
    GLINT_WRITE_REG(pGlint->TexMapFormat, PMTextureMapFormat);
    GLINT_WRITE_REG(UNIT_DISABLE, PMTextureReadMode);
    GLINT_WRITE_REG(UNIT_DISABLE, TextureColorMode);
    GLINT_WRITE_REG(UNIT_DISABLE, DitherMode);
    if (alpha) {
	GLINT_WRITE_REG(UNIT_DISABLE, AlphaBlendMode);
	GLINT_WRITE_REG(pGlint->pprod, FBReadMode);
    }
    GLINT_WRITE_REG(UNIT_DISABLE, YUVMode);
}

#define FORMAT_RGB8888 ((0 << 5) + (0 << 4) + ((0 & 0x10) << 2) + ((0 & 0x0F) << 0))
#define FORMAT_RGB565  ((0 << 5) + (1 << 4) + ((16 & 0x10) << 2) + ((16 & 0x0F) << 0))
#define FORMAT_RGB5551 ((0 << 5) + (0 << 4) + ((1 & 0x10) << 2) + ((1 & 0x0F) << 0))
#define FORMAT_RGB4444 ((0 << 5) + (0 << 4) + ((2 & 0x10) << 2) + ((2 & 0x0F) << 0))
#define FORMAT_RGB332  ((0 << 5) + (1 << 4) + ((5 & 0x10) << 2) + ((5 & 0x0F) << 0))
#define FORMAT_RGB2321 ((0 << 5) + (0 << 4) + ((9 & 0x10) << 2) + ((9 & 0x0F) << 0))
#define FORMAT_BGR8888 ((1 << 5) + (0 << 4) + ((0 & 0x10) << 2) + ((0 & 0x0F) << 0))
#define FORMAT_BGR565  ((1 << 5) + (1 << 4) + ((16 & 0x10) << 2) + ((16 & 0x0F) << 0))
#define FORMAT_BGR5551 ((1 << 5) + (0 << 4) + ((1 & 0x10) << 2) + ((1 & 0x0F) << 0))
#define FORMAT_BGR4444 ((1 << 5) + (0 << 4) + ((2 & 0x10) << 2) + ((2 & 0x0F) << 0))
#define FORMAT_BGR332  ((1 << 5) + (1 << 4) + ((5 & 0x10) << 2) + ((5 & 0x0F) << 0))
#define FORMAT_BGR2321 ((1 << 5) + (0 << 4) + ((9 & 0x10) << 2) + ((9 & 0x0F) << 0))

static void
PutRGB(PortPrivPtr pPPriv, int BufferBase, int format, int bptshift, int alpha)
{
    AdaptorPrivPtr pAPriv = pPPriv->pAdaptor;
    ScrnInfoPtr pScrn = pAPriv->pScrn;
    GLINTPtr pGlint = GLINTPTR(pScrn);
    CookiePtr pCookie = pPPriv->pCookies;
    int nCookies = pPPriv->nCookies;

    DEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 5, "PutRGB %08x %08x\n",
	BufferBase, format));

    if (!nCookies)
	return;

    CHECKCLIPPING;

    GLINT_WRITE_REG(1 << 16, dY);
    GLINT_WRITE_REG(0, RasterizerMode);
    GLINT_WRITE_REG(UNIT_DISABLE, AreaStippleMode);
    GLINT_WRITE_REG(UNIT_ENABLE, TextureAddressMode);
    GLINT_WRITE_REG(pPPriv->dS, dSdx);
    GLINT_WRITE_REG(0, dSdyDom);
    GLINT_WRITE_REG(0, dTdx);
    GLINT_WRITE_REG(pPPriv->dT, dTdyDom);
    GLINT_WRITE_REG(BufferBase >> bptshift, PMTextureBaseAddress);    
    GLINT_WRITE_REG((bptshift << 19) | pPPriv->BufferPProd, PMTextureMapFormat);
    GLINT_WRITE_REG(format, PMTextureDataFormat);
    GLINT_WRITE_REG((pPPriv->Attribute[5] << 17) | /* FilterMode */
		    (11 << 13) | (11 << 9) | /* TextureSize log2 */ 
		    UNIT_ENABLE, PMTextureReadMode);
    GLINT_WRITE_REG((0 << 4) /* RGB */ |
		    (3 << 1) /* Copy */ |
		    UNIT_ENABLE, TextureColorMode);
    if (alpha)
	GLINT_WRITE_REG(pAPriv->dAlphaBlendMode, AlphaBlendMode);
    GLINT_WRITE_REG(pAPriv->dDitherMode, DitherMode);
    GLINT_WRITE_REG(UNIT_DISABLE, LogicalOpMode);
    GLINT_WRITE_REG((alpha << 10) | /* ReadDestination */
		    pGlint->pprod, FBReadMode);
    GLINT_WRITE_REG(0xFFFFFFFF, FBHardwareWriteMask);

    for (; nCookies--; pCookie++) {
	GLINT_WAIT(5);
	GLINT_WRITE_REG(pCookie->xy, RectangleOrigin);
	GLINT_WRITE_REG(pCookie->wh, RectangleSize);
	GLINT_WRITE_REG(pCookie->s, SStart);
	GLINT_WRITE_REG(pCookie->t, TStart);
        GLINT_WRITE_REG(PrimitiveRectangle |
			XPositive |
			YPositive |
			TextureEnable, Render);
    }

    pGlint->x = pGlint->y = -1; /* Force reload */
    pGlint->w = pGlint->h = -1;
    pGlint->ROP = 0xFF;
    pGlint->planemask = 0xFFFFFFFF;

    GLINT_WAIT(7);
    GLINT_WRITE_REG(UNIT_DISABLE, TextureAddressMode);
    GLINT_WRITE_REG(pGlint->TexMapFormat, PMTextureMapFormat);
    GLINT_WRITE_REG(UNIT_DISABLE, PMTextureReadMode);
    GLINT_WRITE_REG(UNIT_DISABLE, TextureColorMode);
    GLINT_WRITE_REG(UNIT_DISABLE, DitherMode);
    if (alpha) {
	GLINT_WRITE_REG(UNIT_DISABLE, AlphaBlendMode);
	GLINT_WRITE_REG(pGlint->pprod, FBReadMode);
    }
}

static void
BlackOut(PortPrivPtr pPPriv, RegionPtr pRegion)
{
    ScrnInfoPtr pScrn = pPPriv->pAdaptor->pScrn;
    GLINTPtr pGlint = GLINTPTR(pScrn);
    RegionRec DRegion;
    BoxRec DBox;
    BoxPtr pBox;
    int nBox;

    DEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 5,
	"BlackOut %d,%d,%d,%d -- %d,%d,%d,%d\n",
	pPPriv->vx, pPPriv->vy, pPPriv->vw, pPPriv->vh,
	pPPriv->dx, pPPriv->dy, pPPriv->dw, pPPriv->dh));

    DBox.x1 = pPPriv->dx - (pPPriv->vx * pPPriv->dw) / pPPriv->vw;
    DBox.y1 = pPPriv->dy - (pPPriv->vy * pPPriv->dh) / pPPriv->vh;
    DBox.x2 = DBox.x1 + (pPPriv->fw * pPPriv->dw) / pPPriv->vw;
    DBox.y2 = DBox.y1 + (pPPriv->fh * pPPriv->dh) / pPPriv->vh;

    REGION_INIT(pScreen, &DRegion, &DBox, 1);

    if (pRegion)
	REGION_SUBTRACT(pScreen, &DRegion, &DRegion, pRegion);

    nBox = REGION_NUM_RECTS(&DRegion);
    pBox = REGION_RECTS(&DRegion);

    GLINT_WAIT(15);
    CHECKCLIPPING;

    GLINT_WRITE_REG(UNIT_DISABLE, ColorDDAMode);
    GLINT_WRITE_REG(pPPriv->BufferPProd, FBReadMode);
    GLINT_WRITE_REG(0x1, FBReadPixel); /* 16 */
    GLINT_WRITE_REG(pPPriv->BkgCol, FBBlockColor);
    GLINT_WRITE_REG(pPPriv->BufferBase[0] >> 1 /* 16 */, FBWindowBase);
    GLINT_WRITE_REG(UNIT_DISABLE, LogicalOpMode);

    for (; nBox--; pBox++) {
        int w = ((pBox->x2 - pBox->x1) * pPPriv->vw + pPPriv->dw) / pPPriv->dw + 1;
	int h = ((pBox->y2 - pBox->y1) * pPPriv->vh + pPPriv->dh) / pPPriv->dh + 1;
	int x = ((pBox->x1 - DBox.x1) * pPPriv->vw + (pPPriv->dw >> 1)) / pPPriv->dw;
	int y = ((pBox->y1 - DBox.y1) * pPPriv->vh + (pPPriv->dh >> 1)) / pPPriv->dh;

	if ((x + w) > pPPriv->fw)
	    w = pPPriv->fw - x;
	if ((y + h) > pPPriv->fh)
	    h = pPPriv->fh - y;

	GLINT_WAIT(3);
	GLINT_WRITE_REG((y << 16) | x, RectangleOrigin);
	GLINT_WRITE_REG((h << 16) | w, RectangleSize);
	GLINT_WRITE_REG(PrimitiveRectangle |
	    XPositive | YPositive | FastFillEnable, Render);
    }

    REGION_UNINIT(pScreen, &DRegion);

    pGlint->x = pGlint->y = -1; /* Force reload */
    pGlint->w = pGlint->h = -1;
    pGlint->ROP = 0xFF;
    GLINT_WAIT(3);
    GLINT_WRITE_REG(0, FBWindowBase);
    GLINT_WRITE_REG(pGlint->pprod, FBReadMode);
    GLINT_WRITE_REG(pGlint->PixelWidth, FBReadPixel);
}

static Bool
RemakeGetCookies(PortPrivPtr pPPriv, RegionPtr pRegion)
{
    BoxPtr pBox;
    CookiePtr pCookie;
    int nBox;
    int dw1 = pPPriv->dw - 1;
    int dh1 = pPPriv->dh - 1;

    if (!pRegion) {
	pBox = (BoxPtr) NULL;
	nBox = pPPriv->nCookies;
    } else {
	pBox = REGION_RECTS(pRegion);
	nBox = REGION_NUM_RECTS(pRegion);

	if (!pPPriv->pCookies || pPPriv->nCookies < nBox) {
	    if (!(pCookie = (CookiePtr) xrealloc(pPPriv->pCookies, nBox * sizeof(CookieRec))))
    		return FALSE;

	    pPPriv->pCookies = pCookie;
	}
    }

    pPPriv->dS = (pPPriv->dw << 20) / pPPriv->vw;
    pPPriv->dT = (pPPriv->dh << 20) / pPPriv->vh;

    for (pCookie = pPPriv->pCookies; nBox--; pBox++) {
	int n1, n2;

	if (pRegion) {
	    n1 = ((pBox->x1 - pPPriv->dx) * pPPriv->vw + dw1) / pPPriv->dw;
            n2 = ((pBox->x2 - pPPriv->dx) * pPPriv->vw - 1) / pPPriv->dw;
	    
	    if (n1 > n2)
		continue; /* Clip is subpixel */

	    pCookie->xy = n1 + pPPriv->vx;
	    pCookie->wh = n2 - n1 + 1;
	    pCookie->s = n1 * pPPriv->dS + (pPPriv->dx << 20);
	    pCookie->y1 = pBox->y1;
	    pCookie->y2 = pBox->y2;
	}

	n1 = ((pCookie->y1 - pPPriv->dy) * pPPriv->vh + dh1) / pPPriv->dh;
	n2 = ((pCookie->y2 - pPPriv->dy) * pPPriv->vh - 1) / pPPriv->dh;
	pCookie->xy = (pCookie->xy & 0xFFFF) | ((n1 + pPPriv->vy) << 16);
	pCookie->wh = (pCookie->wh & 0xFFFF) | ((n2 - n1 + 1) << 16);
	pCookie->t = n1 * pPPriv->dT + (pPPriv->dy << 20);
	if (n1 > n2) pCookie->t = -1;

	pCookie++;
    }

    pPPriv->nCookies = pCookie - pPPriv->pCookies;
    return TRUE;
}

static void
GetYUV(PortPrivPtr pPPriv)
{
    AdaptorPrivPtr pAPriv = pPPriv->pAdaptor;
    ScrnInfoPtr pScrn = pAPriv->pScrn;
    GLINTPtr pGlint = GLINTPTR(pScrn);
    CookiePtr pCookie = pPPriv->pCookies;
    int nCookies = pPPriv->nCookies;

    DEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 5, "GetYUV\n"));

    if (!nCookies || (GLINT_READ_REG(InFIFOSpace) < 200))
	return;
    
    GLINT_WAIT(25);
    CHECKCLIPPING;

    GLINT_WRITE_REG(1 << 16, dY);
    GLINT_WRITE_REG(0, RasterizerMode);
    GLINT_WRITE_REG(UNIT_DISABLE, AreaStippleMode);
    GLINT_WRITE_REG(UNIT_ENABLE, TextureAddressMode);
    GLINT_WRITE_REG(pPPriv->dS, dSdx);
    GLINT_WRITE_REG(0, dSdyDom);
    GLINT_WRITE_REG(0, dTdx);
    GLINT_WRITE_REG(pPPriv->dT, dTdyDom);
    GLINT_WRITE_REG(0, PMTextureBaseAddress);
    GLINT_WRITE_REG(pAPriv->dTextureDataFormat, PMTextureDataFormat);
    GLINT_WRITE_REG((pPPriv->Attribute[5] << 17) | /* FilterMode */
		    (11 << 13) | (11 << 9) | /* TextureSize log2 */
		    UNIT_ENABLE, PMTextureReadMode);
    if (pScrn->depth == 8)
	GLINT_WRITE_REG(UNIT_ENABLE, TexelLUTMode);
    GLINT_WRITE_REG((0 << 4) /* RGB */ |
		    (3 << 1) /* Copy */ |
		    UNIT_ENABLE, TextureColorMode);
    GLINT_WRITE_REG((1 << 10) |			/* RGB */
		    ((16 & 0x10) << 12) |
		    ((16 & 0x0F) << 2) |	/* 5:6:5f */
		    UNIT_ENABLE, DitherMode);
    GLINT_WRITE_REG(UNIT_DISABLE, LogicalOpMode);
    GLINT_WRITE_REG(pPPriv->BufferPProd, FBReadMode);
    GLINT_WRITE_REG(pPPriv->BufferBase[0] >> 1 /* 16 */, FBWindowBase);
    GLINT_WRITE_REG(0x1, FBReadPixel); /* 16 */
    GLINT_WRITE_REG(UNIT_DISABLE, YUVMode);

    for (; nCookies--; pCookie++)
	if (pCookie->t >= 0) {
	    GLINT_WAIT(5);
	    GLINT_WRITE_REG(pCookie->xy, RectangleOrigin);
	    GLINT_WRITE_REG(pCookie->wh, RectangleSize);
	    GLINT_WRITE_REG(pCookie->s, SStart);
	    GLINT_WRITE_REG(pCookie->t, TStart);
    	    GLINT_WRITE_REG(PrimitiveRectangle |
			    XPositive |
			    YPositive |
			    TextureEnable, Render);
	}

    pGlint->x = pGlint->y = -1; /* Force reload */
    pGlint->w = pGlint->h = -1;
    pGlint->ROP = 0xFF;

    GLINT_WAIT(9);
    GLINT_WRITE_REG(UNIT_DISABLE, TextureAddressMode);
    GLINT_WRITE_REG(UNIT_DISABLE, TextureColorMode);
    GLINT_WRITE_REG(UNIT_DISABLE, DitherMode);
    if (pScrn->depth == 8)
	GLINT_WRITE_REG(UNIT_DISABLE, TexelLUTMode);
    GLINT_WRITE_REG(UNIT_DISABLE, PMTextureReadMode);
    GLINT_WRITE_REG(pGlint->pprod, FBReadMode);
    GLINT_WRITE_REG(0, FBWindowBase);
    GLINT_WRITE_REG(pGlint->PixelWidth, FBReadPixel);
    GLINT_WRITE_REG(UNIT_DISABLE, YUVMode);
}

static int
SetBkgCol(PortPrivPtr pPPriv, int value)
{
    pPPriv->Attribute[6] = value;

    pPPriv->BkgCol = ((value & 0xF80000) >> 8) |
		     ((value & 0x00FC00) >> 5) |
		     ((value & 0x0000F8) >> 3);

    pPPriv->BkgCol += pPPriv->BkgCol << 16;

    if (pPPriv->VideoOn) {
	BlackOut(pPPriv, NULL);
	GetYUV(pPPriv);
    }

    return Success;
}

/* os/WaitFor.c */

static CARD32
TimerCallback(OsTimerPtr pTim, CARD32 now, pointer p)
{
    AdaptorPrivPtr pAPriv = (AdaptorPrivPtr) p;
    GLINTPtr pGlint = GLINTPTR(pAPriv->pScrn);
    PortPrivPtr pPPriv;
    int i, delay;

    if (!pAPriv->pm2p) {
	pPPriv = &pAPriv->Port[0];

	if (pPPriv->VideoOn > VIDEO_OFF) {
	    pPPriv->FrameAcc += pPPriv->FramesPerSec;

	    if (pPPriv->FrameAcc >= pAPriv->FramesPerSec) {
		pPPriv->FrameAcc -= pAPriv->FramesPerSec;

		PutYUV(pPPriv, (!pPPriv->pFBArea[1]) ?
		    pPPriv->BufferBase[0] : pPPriv->BufferBase[1 -
			GLINT_READ_REG(VSABase + VSVideoAddressIndex)], FORMAT_YUYV, 1, 0);
	    }
	} else
	    if (pPPriv->StopDelay >= 0 && !(pPPriv->StopDelay--)) {
		StopVideoStream(pPPriv, TRUE);
		RestoreVideoStd(pAPriv);
	    }

	pPPriv = &pAPriv->Port[1];

	if (pPPriv->VideoOn > VIDEO_OFF) {
	    pPPriv->FrameAcc += pPPriv->FramesPerSec;

	    if (pPPriv->FrameAcc >= pAPriv->FramesPerSec) {
		pPPriv->FrameAcc -= pAPriv->FramesPerSec;

		GetYUV(pPPriv);
	    }
	} else
	    if (pPPriv->StopDelay >= 0 && !(pPPriv->StopDelay--)) {
		StopVideoStream(pPPriv, TRUE);
		RestoreVideoStd(pAPriv);
	    }
    }
    
    for (i = 2; i <= 5; i++) {
	if (pAPriv->Port[i].StopDelay >= 0) {
	    if (!(pAPriv->Port[i].StopDelay--)) {
		FreeBuffers(&pAPriv->Port[i]);
		FreeCookies(&pAPriv->Port[i]);
		pAPriv->TimerUsers &= ~(1 << i);
	    }
	}
    }

    if (!pAPriv->pm2p) {
	if (pAPriv->Port[0].StreamOn) {
	    delay = GLINT_READ_REG(VSABase + VSCurrentLine);
	    
	    if (!(GLINT_READ_REG(VSStatus) & VS_FieldOne0A))
		delay += pAPriv->FrameLines >> 1;
	    
	    if (delay > (pAPriv->IntLine - 16))
		delay -= pAPriv->FrameLines;
	    
	    return (((pAPriv->IntLine - delay) * pAPriv->LinePer) + 999999) / 1000000;
	} else if (pAPriv->Port[1].StreamOn) {
	    delay = GLINT_READ_REG(VSBBase + VSCurrentLine);
	    
	    if (!(GLINT_READ_REG(VSStatus) & VS_FieldOne0B))
		delay += pAPriv->FrameLines >> 1;
	    
	    if (delay > (pAPriv->IntLine - 16))
		delay -= pAPriv->FrameLines;
	    
	    return (((pAPriv->IntLine - delay) * pAPriv->LinePer) + 999999) / 1000000;
	}
    }

    if (pAPriv->TimerUsers)
	return pAPriv->Instant;

    return 0; /* Cancel */
}


/*
 *  Video stream (bounce buffer <-> hardware)
 */

static void
StopVideoStream(PortPrivPtr pPPriv, Bool shutdown)
{
    AdaptorPrivPtr pAPriv = pPPriv->pAdaptor;
    GLINTPtr pGlint = GLINTPTR(pAPriv->pScrn);
    int VideoOn;

    pPPriv->StopDelay = -1;

    VideoOn = pPPriv->VideoOn;
    pPPriv->VideoOn = VIDEO_OFF;

    if (!pPPriv->StreamOn)
	return;

    if (pAPriv->pm2p) {
	xvipcHandshake(pPPriv, OP_STOP, TRUE);

	pPPriv->StreamOn = FALSE;

	if (shutdown)
	    FreeCookies(pPPriv);

	if (VideoOn > VIDEO_OFF && pGlint->NoAccel)
	    Permedia2Sync(pAPriv->pScrn);

	return;
    }

    if (pPPriv == &pAPriv->Port[0]) {
	int line, eeek = 0;

	do {
	    if (eeek++ > 1000000) break;
	    line = GLINT_READ_REG(VSABase + VSCurrentLine);
	} while (line > 15);
	
	GLINT_WRITE_REG(0, VSABase + VSControl);
	
	pAPriv->Port[0].StreamOn = FALSE;
	
	usleep(80000);
    } else {
    	xf86I2CWriteByte(&pAPriv->Port[1].I2CDev, 0x3A, 0x83);
	if (!ColorBars)
	    xf86I2CWriteByte(&pAPriv->Port[1].I2CDev, 0x61, 0xC2);
	
	GLINT_WRITE_REG(0, VSBBase + VSControl);
	
	pAPriv->Port[1].StreamOn = FALSE;
    }

    if (!pAPriv->Port[0].StreamOn && !pAPriv->Port[1].StreamOn) {
	if (shutdown)
	    xf86I2CWriteByte(&pAPriv->Port[1].I2CDev, 0x61, 0xC2);
	xf86I2CWriteByte(&pAPriv->Port[0].I2CDev, 0x11, 0x00);
    }

    if (shutdown) {
	FreeBuffers(pPPriv);
	FreeCookies(pPPriv);
	
	if (pAPriv->TimerUsers) {
	    pAPriv->TimerUsers &= ~PORTNUM(pPPriv);
	    if (!pAPriv->TimerUsers)
		TimerCancel(pAPriv->Timer);
	}
	
	if (VideoOn > VIDEO_OFF && pGlint->NoAccel)
	    Permedia2Sync(pAPriv->pScrn);
    }
}

static Bool
StartVideoStream(PortPrivPtr pPPriv, RegionPtr pRegion)
{
    AdaptorPrivPtr pAPriv = pPPriv->pAdaptor;
    GLINTPtr pGlint = GLINTPTR(pAPriv->pScrn);

    if (pAPriv->VideoStd < 0)
	return FALSE;

    pPPriv->StopDelay = -1;

    if (pAPriv->pm2p) {
	if (pPPriv == &pAPriv->Port[0]) {
	    if (!RemakePutCookies(pPPriv, pRegion))
		return FALSE;
	    if (pPPriv->StreamOn)
		return TRUE;
	} else {
	    if (!RemakeGetCookies(pPPriv, pRegion))
		return FALSE;
	    if (pPPriv->StreamOn) {
		BlackOut(pPPriv, pRegion);
		return TRUE;
	    }
	}

	xvipc.a = pPPriv->BuffersRequested;
	xvipc.b = !pPPriv->Attribute[4];
	xvipc.c = 1 + (pPPriv->Attribute[4] & 2);

	if (!xvipcHandshake(pPPriv, OP_START, TRUE))
		return FALSE;

	if (pPPriv == &pAPriv->Port[1]) {
	    pPPriv->BufferBase[0] = xvipc.d;
	    BlackOut(pPPriv, pRegion);
	}

	return pPPriv->StreamOn = TRUE;
    } else {
	CARD32 Base = (pPPriv == &pAPriv->Port[0]) ? VSABase : VSBBase;

        if (pPPriv->BuffersAllocated < pPPriv->BuffersRequested) {
	    int height = ((pAPriv->VideoStd == NTSC) ? 512 : 608) >> (!pPPriv->Attribute[4]);

    	    if (!AllocateBuffers(pPPriv, 704, height, 2, pPPriv->BuffersRequested, 0))
		return FALSE;

	    pPPriv->fw = 704;
	    pPPriv->fh = InputVideoEncodings[pAPriv->VideoStd * 3].height >>
			(!pPPriv->Attribute[4]);
	}

	if (pPPriv == &pAPriv->Port[0]) {
	    if (!RemakePutCookies(pPPriv, pRegion))
		return FALSE;
	} else {
	    if (!RemakeGetCookies(pPPriv, pRegion))
		return FALSE;

	    BlackOut(pPPriv, pRegion);
	}

	if (pPPriv->StreamOn)
	    return TRUE;

	GLINT_WRITE_REG(pPPriv->BufferBase[0] / 8, Base + VSVideoAddress0);
	if (pPPriv->pFBArea[1])
	    GLINT_WRITE_REG(pPPriv->BufferBase[1] / 8, Base + VSVideoAddress1);
	else
	    GLINT_WRITE_REG(pPPriv->BufferBase[0] / 8, Base + VSVideoAddress1);
	GLINT_WRITE_REG(pPPriv->BufferStride / 8, Base + VSVideoStride);

	GLINT_WRITE_REG(0, Base + VSCurrentLine);

	if (pAPriv->VideoStd == NTSC) {
	    GLINT_WRITE_REG(16, Base + VSVideoStartLine);
	    GLINT_WRITE_REG(16 + 240, Base + VSVideoEndLine);
	    GLINT_WRITE_REG(288 + (8 & ~3) * 2, Base + VSVideoStartData);
	    GLINT_WRITE_REG(288 + ((8 & ~3) + 704) * 2, Base + VSVideoEndData);
	} else {
	    GLINT_WRITE_REG(16, Base + VSVideoStartLine);
	    GLINT_WRITE_REG(16 + 288, Base + VSVideoEndLine);
	    GLINT_WRITE_REG(288 + (8 & ~3) * 2, Base + VSVideoStartData);
	    GLINT_WRITE_REG(288 + ((8 & ~3) + 704) * 2, Base + VSVideoEndData);
	}

	GLINT_WRITE_REG(2, Base + VSVideoAddressHost);
	GLINT_WRITE_REG(0, Base + VSVideoAddressIndex);

	if (pPPriv == &pAPriv->Port[0]) {
	    int line, eeek = 0;

	    xf86I2CWriteByte(&pAPriv->Port[0].I2CDev, 0x11, 0x0D);

	    do {
		if (eeek++ > 1000000) break;
		line = GLINT_READ_REG(VSABase + VSCurrentLine);
	    } while (line > 15);

	    GLINT_WRITE_REG(VSA_Video |
			    (pPPriv->Attribute[4] ? 
				VSA_CombineFields : VSA_Discard_FieldTwo),
			    VSABase + VSControl);
	    if (ColorBars)
		if (!pAPriv->Port[1].StreamOn) {
		    xf86I2CWriteByte(&pAPriv->Port[1].I2CDev, 0x3A, 0x83);
		    xf86I2CWriteByte(&pAPriv->Port[1].I2CDev, 0x61, Enc61[pAPriv->VideoStd]);
		}
	} else {
	    GLINT_WRITE_REG(VSB_Video |
			    (pPPriv->Attribute[4] ? VSB_CombineFields : 0) |
			  /* VSB_GammaCorrect | */
			    (16 << 4) | /* 5:6:5 */
			    (1 << 9) | /* 16 */
			    VSB_RGBOrder, VSBBase + VSControl);
	    xf86I2CWriteByte(&pAPriv->Port[0].I2CDev, 0x11, 0x0D);
	    xf86I2CWriteByte(&pAPriv->Port[1].I2CDev, 0x3A, Enc3A[pPPriv->Plug]);
	    xf86I2CWriteByte(&pAPriv->Port[1].I2CDev, 0x61, Enc61[pAPriv->VideoStd]);
	}

	pAPriv->TimerUsers |= 1 << PORTNUM(pPPriv);
	TimerSet(pAPriv->Timer, 0, 80, TimerCallback, pAPriv);

	return pPPriv->StreamOn = TRUE;
    }

    return FALSE;
}


/*
 *  Xv interface
 */

static int
Permedia2PutVideo(ScrnInfoPtr pScrn,
    short vid_x, short vid_y, short drw_x, short drw_y,
    short vid_w, short vid_h, short drw_w, short drw_h,
    RegionPtr clipBoxes, pointer data)
{
    PortPrivPtr pPPriv = (PortPrivPtr) data;
    AdaptorPrivPtr pAPriv = pPPriv->pAdaptor;
    int sw, sh;

    DEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3,
	"PutVideo %d,%d,%d,%d -> %d,%d,%d,%d\n",
	vid_x, vid_y, vid_w, vid_h, drw_x, drw_y, drw_w, drw_h));

    sw = InputVideoEncodings[pAPriv->VideoStd * 3].width;
    sh = InputVideoEncodings[pAPriv->VideoStd * 3].height;

    if ((vid_x + vid_w) > sw ||
        (vid_y + vid_h) > sh)
        return BadValue;

    pPPriv->VideoOn = VIDEO_OFF;

    pPPriv->vx = ((vid_x << 10) * pPPriv->fw) / sw;
    pPPriv->vy = ((vid_y << 10) * pPPriv->fh) / sh;
    pPPriv->vw = ((vid_w << 10) * pPPriv->fw) / sw;
    pPPriv->vh = ((vid_h << 10) * pPPriv->fh) / sh;

    pPPriv->dx = drw_x;
    pPPriv->dy = drw_y;
    pPPriv->dw = drw_w;
    pPPriv->dh = drw_h;

    pPPriv->FrameAcc = pAPriv->FramesPerSec;

    if (!StartVideoStream(pPPriv, clipBoxes))
        return XvBadAlloc;

    pPPriv->VideoOn = VIDEO_ON;

    return Success;
}

static int
Permedia2PutStill(ScrnInfoPtr pScrn,
    short vid_x, short vid_y, short drw_x, short drw_y,
    short vid_w, short vid_h, short drw_w, short drw_h,
    RegionPtr clipBoxes, pointer data)
{
    PortPrivPtr pPPriv = (PortPrivPtr) data;  
    AdaptorPrivPtr pAPriv = pPPriv->pAdaptor;
    GLINTPtr pGlint = GLINTPTR(pScrn);
    int sw, sh, r = Success;

    DEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3,
	"PutStill %d,%d,%d,%d -> %d,%d,%d,%d\n",
	vid_x, vid_y, vid_w, vid_h, drw_x, drw_y, drw_w, drw_h));

    sw = InputVideoEncodings[pAPriv->VideoStd * 3].width;
    sh = InputVideoEncodings[pAPriv->VideoStd * 3].height;

    if ((vid_x + vid_w) > sw ||
        (vid_y + vid_h) > sh)
        return BadValue;

    pPPriv->VideoOn = VIDEO_OFF;

    pPPriv->vx = ((vid_x << 10) * pPPriv->fw) / sw;
    pPPriv->vy = ((vid_y << 10) * pPPriv->fh) / sh;
    pPPriv->vw = ((vid_w << 10) * pPPriv->fw) / sw;
    pPPriv->vh = ((vid_h << 10) * pPPriv->fh) / sh;

    pPPriv->dx = drw_x;
    pPPriv->dy = drw_y;
    pPPriv->dw = drw_w;
    pPPriv->dh = drw_h;

    pPPriv->FrameAcc = pAPriv->FramesPerSec;

    if (!StartVideoStream(pPPriv, clipBoxes))
        return XvBadAlloc;

    if (pAPriv->pm2p) {
	/* Sleep, not busy wait, until the very next frame is ready.
	   Accept memory requests and other window's update events
	   in the meantime. */
	for (pPPriv->VideoOn = VIDEO_ONE_SHOT; pPPriv->VideoOn;)
	    if (!xvipcHandshake(pPPriv, OP_UPDATE, TRUE)) {
		r = FALSE;
		break;
	    }
    } else {
        usleep(80000);

        PutYUV(pPPriv, (!pPPriv->pFBArea[1]) ?
            pPPriv->BufferBase[0] : pPPriv->BufferBase[1 -
		GLINT_READ_REG(VSABase + VSVideoAddressIndex)], FORMAT_YUYV, 1, 0);
    }

    pPPriv->StopDelay = 125;

    return r;
}

static int
Permedia2GetVideo(ScrnInfoPtr pScrn,
    short vid_x, short vid_y, short drw_x, short drw_y,
    short vid_w, short vid_h, short drw_w, short drw_h,
    RegionPtr clipBoxes, pointer data)
{
    PortPrivPtr pPPriv = (PortPrivPtr) data;
    AdaptorPrivPtr pAPriv = pPPriv->pAdaptor;
    int sw, sh;

    DEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3,
	"GetVideo %d,%d,%d,%d <- %d,%d,%d,%d\n",
	vid_x, vid_y, vid_w, vid_h, drw_x, drw_y, drw_w, drw_h));

    sw = InputVideoEncodings[pAPriv->VideoStd * 3].width;
    sh = InputVideoEncodings[pAPriv->VideoStd * 3].height;

    if ((vid_x + vid_w) > sw ||
        (vid_y + vid_h) > sh) {
        return BadValue;
    }

    pPPriv->VideoOn = VIDEO_OFF;

    pPPriv->vx = (vid_x * pPPriv->fw) / sw;
    pPPriv->vy = (vid_y * pPPriv->fh) / sh;
    pPPriv->vw = (vid_w * pPPriv->fw) / sw;
    pPPriv->vh = (vid_h * pPPriv->fh) / sh;

    pPPriv->dx = drw_x;
    pPPriv->dy = drw_y;
    pPPriv->dw = drw_w;
    pPPriv->dh = drw_h;

    pPPriv->FrameAcc = pAPriv->FramesPerSec;

    if (!StartVideoStream(pPPriv, clipBoxes)) {
        return XvBadAlloc;
    }

    GetYUV(pPPriv);

    pPPriv->VideoOn = VIDEO_ON;

    return Success;
}

static int
Permedia2GetStill(ScrnInfoPtr pScrn,
    short vid_x, short vid_y, short drw_x, short drw_y,
    short vid_w, short vid_h, short drw_w, short drw_h,
    RegionPtr clipBoxes, pointer data)
{
    PortPrivPtr pPPriv = (PortPrivPtr) data;  
    AdaptorPrivPtr pAPriv = pPPriv->pAdaptor;
    int sw, sh;

    DEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3,
	"GetStill %d,%d,%d,%d <- %d,%d,%d,%d\n",
	vid_x, vid_y, vid_w, vid_h, drw_x, drw_y, drw_w, drw_h));

    sw = InputVideoEncodings[pAPriv->VideoStd * 3].width;
    sh = InputVideoEncodings[pAPriv->VideoStd * 3].height;

    if ((vid_x + vid_w) > sw ||
        (vid_y + vid_h) > sh)
        return BadValue;

    pPPriv->VideoOn = VIDEO_OFF;

    pPPriv->vx = (vid_x * pPPriv->fw) / sw;
    pPPriv->vy = (vid_y * pPPriv->fh) / sh;
    pPPriv->vw = (vid_w * pPPriv->fw) / sw;
    pPPriv->vh = (vid_h * pPPriv->fh) / sh;

    pPPriv->dx = drw_x;
    pPPriv->dy = drw_y;
    pPPriv->dw = drw_w;
    pPPriv->dh = drw_h;

    pPPriv->FrameAcc = pAPriv->FramesPerSec;

    if (!StartVideoStream(pPPriv, clipBoxes))
        return XvBadAlloc;

    GetYUV(pPPriv);

    return Success;
}

static void
CopyYV12LE(CARD8 *Y, CARD32 *dst, int width, int height, int pitch)
{
    int Y_size = width * height;
    CARD8 *V = Y + Y_size;
    CARD8 *U = V + (Y_size >> 2);
    int pad = (pitch >> 2) - (width >> 1);
    int x;

    width >>= 1;

    for (height >>= 1; height > 0; height--) {
	for (x = 0; x < width; Y += 2, x++)
	    *dst++ = Y[0] + (U[x] << 8) + (Y[1] << 16) + (V[x] << 24);
	dst += pad;
	for (x = 0; x < width; Y += 2, x++)
	    *dst++ = Y[0] + (U[x] << 8) + (Y[1] << 16) + (V[x] << 24);
	dst += pad;
	U += width;
	V += width;
    }
}

#if X_BYTE_ORDER == X_BIG_ENDIAN

static void
CopyYV12BE(CARD8 *Y, CARD32 *dst, int width, int height, int pitch)
{
    int Y_size = width * height;
    CARD8 *V = Y + Y_size;
    CARD8 *U = V + (Y_size >> 2);
    int pad = (pitch >> 2) - (width >> 1);
    int x;

    width >>= 1;

    for (height >>= 1; height > 0; height--) {
	for (x = 0; x < width; Y += 2, x++)
	    *dst++ = V[x] + (Y[1] << 8) + (U[x] << 16) + (Y[0] << 24);
	dst += pad;
	for (x = 0; x < width; Y += 2, x++)
	    *dst++ = V[x] + (Y[1] << 8) + (U[x] << 16) + (Y[0] << 24);
	dst += pad;
	U += width;
	V += width;
    }
}

#endif /* X_BYTE_ORDER == X_BIG_ENDIAN */

static void
CopyFlat(CARD8 *src, CARD8 *dst, int width, int height, int pitch)
{
    if (width == pitch) {
	memcpy(dst, src, width * height);
	return;
    }

    while (height > 0) {
	memcpy(dst, src, width);
	dst += pitch;
	src += width;
	height--;
    }
}

static int
Permedia2PutImage(ScrnInfoPtr pScrn,
    short src_x, short src_y, short drw_x, short drw_y,
    short src_w, short src_h, short drw_w, short drw_h,
    int id, unsigned char *buf, short width, short height,
    Bool sync, RegionPtr clipBoxes, pointer data)
{
    PortPrivPtr pPPriv = (PortPrivPtr) data;  
    AdaptorPrivPtr pAPriv = pPPriv->pAdaptor;
    GLINTPtr pGlint = GLINTPTR(pScrn);
    int i;

    DEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3,
	"PutImage %d,%d,%d,%d -> %d,%d,%d,%d id=0x%08x buf=%p w=%d h=%d sync=%d\n",
	src_x, src_y, src_w, src_h, drw_x, drw_y, drw_w, drw_h,
	id, buf, width, height, sync));

    if ((src_x + src_w) > width ||
        (src_y + src_h) > height)
        return BadValue;

    pPPriv->vx = src_x << 10;
    pPPriv->vy = src_y << 10;
    pPPriv->vw = src_w << 10;
    pPPriv->vh = src_h << 10;

    pPPriv->dx = drw_x;
    pPPriv->dy = drw_y;
    pPPriv->dw = drw_w;
    pPPriv->dh = drw_h;

    if (!RemakePutCookies(pPPriv, clipBoxes))
	return XvBadAlloc;

    if (pPPriv->BuffersAllocated <= 0 ||
	id != pPPriv->Id || /* same bpp */
	width != pPPriv->fw ||
	height != pPPriv->fh)
    {
	for (i = 0; i < ENTRIES(ScalerImages); i++)
	    if (id == ScalerImages[i].id)
		break;

	if (i >= ENTRIES(ScalerImages))
	    return XvBadAlloc;
#if 0
	if (pPPriv->BuffersAllocated <= 0 ||
	    pPPriv->Bpp != ScalerImages[i].bits_per_pixel ||
	    width > pPPriv->fw || height > pPPriv->fw ||
	    pPPriv->fw * pPPriv->fh > width * height * 2)
#else
	if (1)
#endif
	{
	    Permedia2Sync(pScrn);

	    if (!AllocateBuffers(pPPriv, width, height,
		(ScalerImages[i].bits_per_pixel + 7) >> 3, 1, 0)) {
		pPPriv->Id = 0;
		pPPriv->Bpp = 0;
		pPPriv->fw = 0;
		pPPriv->fh = 0;

		return XvBadAlloc;
	    }

	    pPPriv->Id = id;
	    pPPriv->Bpp = ScalerImages[i].bits_per_pixel;
	    pPPriv->fw = width;
	    pPPriv->fh = height;

	    RemoveableBuffers(pPPriv, TRUE);
	} else
	    Permedia2Sync(pScrn);
    } else
	Permedia2Sync(pScrn);

    switch (id) {
    case LE4CC('Y','V','1','2'):
#if X_BYTE_ORDER == X_LITTLE_ENDIAN
	CopyYV12LE(buf, (CARD32 *)((CARD8 *) pGlint->FbBase + pPPriv->BufferBase[0]),
	    width, height, pPPriv->BufferStride);
#else
	if (pGlint->FBDev)
	    CopyYV12LE(buf, (CARD32 *)((CARD8 *) pGlint->FbBase + pPPriv->BufferBase[0]),
		width, height, pPPriv->BufferStride);
	else
	    CopyYV12BE(buf, (CARD32 *)((CARD8 *) pGlint->FbBase + pPPriv->BufferBase[0]),
		width, height, pPPriv->BufferStride);
#endif
	PutYUV(pPPriv, pPPriv->BufferBase[0], FORMAT_YUYV, 1, 0);
	break;

    case LE4CC('Y','U','Y','2'):
	CopyFlat(buf, (CARD8 *) pGlint->FbBase + pPPriv->BufferBase[0],
	    width << 1, height, pPPriv->BufferStride);
	PutYUV(pPPriv, pPPriv->BufferBase[0], FORMAT_YUYV, 1, 0);
	break;

    case LE4CC('U','Y','V','Y'):
	CopyFlat(buf, (CARD8 *) pGlint->FbBase + pPPriv->BufferBase[0],
	    width << 1, height, pPPriv->BufferStride);
	PutYUV(pPPriv, pPPriv->BufferBase[0], FORMAT_UYVY, 1, 0);
	break;

    case LE4CC('Y','U','V','A'):
	CopyFlat(buf, (CARD8 *) pGlint->FbBase + pPPriv->BufferBase[0],
	    width << 2, height, pPPriv->BufferStride);
	PutYUV(pPPriv, pPPriv->BufferBase[0], FORMAT_YUVA, 2, pPPriv->Attribute[7]);
	break;

    case LE4CC('V','U','Y','A'):
	CopyFlat(buf, (CARD8 *) pGlint->FbBase + pPPriv->BufferBase[0],
	    width << 2, height, pPPriv->BufferStride);
	PutYUV(pPPriv, pPPriv->BufferBase[0], FORMAT_VUYA, 2, pPPriv->Attribute[7]);
	break;

    case 0x41: /* RGBA 8:8:8:8 */
	CopyFlat(buf, (CARD8 *) pGlint->FbBase + pPPriv->BufferBase[0],
	    width << 2, height, pPPriv->BufferStride);
	PutRGB(pPPriv, pPPriv->BufferBase[0], FORMAT_RGB8888, 2, pPPriv->Attribute[7]);
	break;

    case 0x42: /* RGB 5:6:5 */
	CopyFlat(buf, (CARD8 *) pGlint->FbBase + pPPriv->BufferBase[0],
	    width << 1, height, pPPriv->BufferStride);
	PutRGB(pPPriv, pPPriv->BufferBase[0], FORMAT_RGB565, 1, 0);
	break;

    case 0x43: /* RGB 1:5:5:5 */
	CopyFlat(buf, (CARD8 *) pGlint->FbBase + pPPriv->BufferBase[0],
	    width << 1, height, pPPriv->BufferStride);
	PutRGB(pPPriv, pPPriv->BufferBase[0], FORMAT_RGB5551, 1, pPPriv->Attribute[7]);
	break;

    case 0x44: /* RGB 4:4:4:4 */
	CopyFlat(buf, (CARD8 *) pGlint->FbBase + pPPriv->BufferBase[0],
	    width << 1, height, pPPriv->BufferStride);
	PutRGB(pPPriv, pPPriv->BufferBase[0], FORMAT_RGB4444, 1, pPPriv->Attribute[7]);
	break;

    case 0x45: /* RGB 1:2:3:2 */
	CopyFlat(buf, (CARD8 *) pGlint->FbBase + pPPriv->BufferBase[0],
	    width, height, pPPriv->BufferStride);
	PutRGB(pPPriv, pPPriv->BufferBase[0], FORMAT_RGB2321, 0, pPPriv->Attribute[7]);
	break;

    case 0x46: /* RGB 2:3:3 */
	CopyFlat(buf, (CARD8 *) pGlint->FbBase + pPPriv->BufferBase[0],
	    width, height, pPPriv->BufferStride);
	PutRGB(pPPriv, pPPriv->BufferBase[0], FORMAT_RGB332, 0, 0);
	break;

    case 0x47: /* BGRA 8:8:8:8 */
	CopyFlat(buf, (CARD8 *) pGlint->FbBase + pPPriv->BufferBase[0],
	    width << 2, height, pPPriv->BufferStride);
	PutRGB(pPPriv, pPPriv->BufferBase[0], FORMAT_BGR8888, 2, pPPriv->Attribute[7]);
	break;

    case 0x48: /* BGR 5:6:5 */
	CopyFlat(buf, (CARD8 *) pGlint->FbBase + pPPriv->BufferBase[0],
	    width << 1, height, pPPriv->BufferStride);
	PutRGB(pPPriv, pPPriv->BufferBase[0], FORMAT_BGR565, 1, 0);
	break;

    case 0x49: /* BGR 1:5:5:5 */
	CopyFlat(buf, (CARD8 *) pGlint->FbBase + pPPriv->BufferBase[0],
	    width << 1, height, pPPriv->BufferStride);
	PutRGB(pPPriv, pPPriv->BufferBase[0], FORMAT_BGR5551, 1, pPPriv->Attribute[7]);
	break;

    case 0x4A: /* BGR 4:4:4:4 */
	CopyFlat(buf, (CARD8 *) pGlint->FbBase + pPPriv->BufferBase[0],
	    width << 1, height, pPPriv->BufferStride);
	PutRGB(pPPriv, pPPriv->BufferBase[0], FORMAT_BGR4444, 1, pPPriv->Attribute[7]);
	break;

    case 0x4B: /* BGR 1:2:3:2 */
	CopyFlat(buf, (CARD8 *) pGlint->FbBase + pPPriv->BufferBase[0],
	    width << 0, height, pPPriv->BufferStride);
	PutRGB(pPPriv, pPPriv->BufferBase[0], FORMAT_BGR2321, 0, pPPriv->Attribute[7]);
	break;

    case 0x4C: /* BGR 2:3:3 */
	CopyFlat(buf, (CARD8 *) pGlint->FbBase + pPPriv->BufferBase[0],
	    width << 0, height, pPPriv->BufferStride);
	PutRGB(pPPriv, pPPriv->BufferBase[0], FORMAT_BGR332, 0, 0);
	break;

    default:
	return XvBadAlloc;
    }

    pPPriv->StopDelay = pAPriv->Delay;

    if (!pAPriv->TimerUsers) {
	pAPriv->TimerUsers |= 1 << PORTNUM(pPPriv);
	TimerSet(pAPriv->Timer, 0, 80, TimerCallback, pAPriv);
    }

    if (sync) /* sched_yield? */
	Permedia2Sync(pScrn);

    return Success;
}

static void
Permedia2StopVideo(ScrnInfoPtr pScrn, pointer data, Bool shutdown)
{
    PortPrivPtr pPPriv = (PortPrivPtr) data;  
    AdaptorPrivPtr pAPriv = pPPriv->pAdaptor;
    GLINTPtr pGlint = GLINTPTR(pScrn);

    DEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3,
	"StopVideo port=%d, shutdown=%d\n", PORTNUM(pPPriv), shutdown));

    if (shutdown) {
	if (PORTNUM(pPPriv) < 2) {
	    StopVideoStream(pPPriv, TRUE);
	    RestoreVideoStd(pAPriv);
	} else {
	    FreeBuffers(pPPriv);
	    FreeCookies(pPPriv);
	    if (pAPriv->TimerUsers) {
		pAPriv->TimerUsers &= ~PORTNUM(pPPriv);
		if (!pAPriv->TimerUsers)
		    TimerCancel(pAPriv->Timer);
	    }
	}
    } else {
	pPPriv->VideoOn = VIDEO_OFF;
	pPPriv->StopDelay = 750; /* appx. 30 sec */

	if (pGlint->NoAccel)
	    Permedia2Sync(pScrn);
    }
}

static void
RestartVideo(PortPrivPtr pPPriv, int old_VideoOn)
{
    AdaptorPrivPtr pAPriv = pPPriv->pAdaptor;
    int new_fh;

    if (pPPriv->VideoOn > VIDEO_OFF ||
	pAPriv->VideoStd < 0 /* invalid */)
	return;

    new_fh = InputVideoEncodings[pAPriv->VideoStd * 3].height >>
	(1 - (pPPriv->Attribute[4] & 1));

    if (new_fh != pPPriv->fh) {
	pPPriv->vy = (pPPriv->vy * new_fh) / pPPriv->fh;
	pPPriv->vh = (pPPriv->vh * new_fh) / pPPriv->fh;

	pPPriv->fh = new_fh;
    }

    if (old_VideoOn) {
	if (StartVideoStream(pPPriv, NULL)) {
	    pPPriv->VideoOn = old_VideoOn;

	    if (pPPriv == &pAPriv->Port[1])
		GetYUV(pPPriv);
	} else {
	    DEBUG(xf86DrvMsgVerb(pAPriv->pScrn->scrnIndex, X_INFO, 4,
		"RestartVideo port=%d suspend\n", PORTNUM(pPPriv)));
	    pPPriv->VideoOn = -old_VideoOn; /* suspend (not off) */
	}
    }
}

static int
Permedia2SetPortAttribute(ScrnInfoPtr pScrn,
    Atom attribute, INT32 value, pointer data)
{
    PortPrivPtr pPPriv = (PortPrivPtr) data;
    AdaptorPrivPtr pAPriv = pPPriv->pAdaptor;
    int old_VideoStd, old_Plug;
    int VideoStd = -1, Plug = 0;
    int r;

    DEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3,
	"SPA attr=%d val=%d port=%d\n",
	attribute, value, PORTNUM(pPPriv)));

    if (attribute == xvFilter) {
	pPPriv->Attribute[5] = !!value;
	return Success;
    } else if (attribute == xvAlpha) {
	pPPriv->Attribute[7] = !!value;
	return Success;
    }

    if (PORTNUM(pPPriv) >= 2)
	return BadMatch;

    if (attribute == xvInterlace) {
	int old_value = pPPriv->Attribute[4];

	value %= 3;

	if (value != old_value) {
	    int old_VideoOn = ABS(pPPriv->VideoOn);
#if 0
	    if (old_VideoOn)
		return XvBadAlloc;
#endif
	    StopVideoStream(pPPriv, FALSE);
	    FreeBuffers(pPPriv);
	    pPPriv->Attribute[4] = value;
	    RestartVideo(pPPriv, old_VideoOn);

	    if (pPPriv->VideoOn < 0 /* suspended */) {
		pPPriv->Attribute[4] = old_value;
		RestartVideo(pPPriv, old_VideoOn);
		return XvBadAlloc;
	    }
	}

	return Success;
    }

    if (pPPriv == &pAPriv->Port[0]) {
	/*
	 *  Input
	 */
	if (attribute == xvEncoding) {
	    if (value < 0 || value > ENTRIES(InputVideoEncodings))
		return XvBadEncoding;

	    VideoStd = value / 3;
	    Plug = value % 3;

	    /* Fall through */

	} else if (attribute == xvBrightness)
	    return SetAttr(&pAPriv->Port[0], 0, value);
	else if (attribute == xvContrast)
	    return SetAttr(&pAPriv->Port[0], 1, value);
   	else if (attribute == xvSaturation)
	    return SetAttr(&pAPriv->Port[0], 2, value);
	else if (attribute == xvHue)
	    return SetAttr(&pAPriv->Port[0], 3, value);
    } else {
	/*
	 *  Output
	 */
	if (attribute == xvEncoding) {
	    if (value < 0 || value > ENTRIES(OutputVideoEncodings))
		return XvBadEncoding;

	    VideoStd = value / 2;
	    Plug = (value % 2) + 1;

	    /* Fall through */

	} else if (attribute == xvBkgColor)
	    return SetBkgCol(pPPriv, value);
#if 1
	else if (attribute == xvBrightness ||
		 attribute == xvContrast ||
		 attribute == xvSaturation ||
		 attribute == xvHue)
	    return Success;
#endif
    }

    if (attribute != xvEncoding)
	return BadMatch;

    old_VideoStd = pAPriv->VideoStd;
    old_Plug = pPPriv->Plug;

#if 0
    if (pAPriv->Port[0].VideoOn ||
	pAPriv->Port[1].VideoOn)
	return XvBadAlloc;
#endif

    if (Plug != old_Plug)
	if ((r = SetPlug(pPPriv, Plug)) != Success)
	    return r;

    if (VideoStd != old_VideoStd) {
	int old_VideoOn0 = ABS(pAPriv->Port[0].VideoOn);
	int old_VideoOn1 = ABS(pAPriv->Port[1].VideoOn);

	StopVideoStream(&pAPriv->Port[0], FALSE);
	StopVideoStream(&pAPriv->Port[1], FALSE);

	if (VideoStd == NTSC || pAPriv->VideoStd == NTSC) {
	    FreeBuffers(&pAPriv->Port[0]);
	    FreeBuffers(&pAPriv->Port[1]);
	}

	if (SetVideoStd(pPPriv, VideoStd) == Success) {
	    RestartVideo(&pAPriv->Port[0], old_VideoOn0);
	    RestartVideo(&pAPriv->Port[1], old_VideoOn1);
	}

	if (pAPriv->Port[0].VideoOn < 0 ||
	    pAPriv->Port[1].VideoOn < 0 ||
	    VideoStd != pAPriv->VideoStd) {
	    if (SetVideoStd(pPPriv, old_VideoStd) == Success) {
		RestartVideo(&pAPriv->Port[0], old_VideoOn0);
		RestartVideo(&pAPriv->Port[1], old_VideoOn1);
	    }

	    if (Plug != old_Plug)
		SetPlug(pPPriv, old_Plug);

	    return XvBadAlloc;
	}
    }

    return Success;
}

static void
RestoreVideoStd(AdaptorPrivPtr pAPriv)
{
    if (pAPriv->Port[0].VideoOn && !pAPriv->Port[1].VideoOn &&
	pAPriv->Port[0].VideoStdReq != pAPriv->VideoStd)
	Permedia2SetPortAttribute(pAPriv->pScrn, xvEncoding,
	    pAPriv->Port[0].VideoStdReq * 3 + pAPriv->Port[0].Plug, 
	    (pointer) &pAPriv->Port[0]);
    else
    if (pAPriv->Port[1].VideoOn && !pAPriv->Port[0].VideoOn &&
	pAPriv->Port[1].VideoStdReq != pAPriv->VideoStd)
	Permedia2SetPortAttribute(pAPriv->pScrn, xvEncoding,
	    pAPriv->Port[2].VideoStdReq * 2 + pAPriv->Port[1].Plug - 1, 
	    (pointer) &pAPriv->Port[1]);
}

static int
Permedia2GetPortAttribute(ScrnInfoPtr pScrn, 
    Atom attribute, INT32 *value, pointer data)
{
    PortPrivPtr pPPriv = (PortPrivPtr) data;
    AdaptorPrivPtr pAPriv = pPPriv->pAdaptor;

    if (PORTNUM(pPPriv) >= 2 &&
	attribute != xvFilter &&
	attribute != xvAlpha)
	return BadMatch;

    if (attribute == xvEncoding) {
	if (pAPriv->VideoStd < 0)
	    return XvBadAlloc;
	else
	    if (pPPriv == &pAPriv->Port[0])
		*value = pAPriv->VideoStd * 3 + pPPriv->Plug;
	    else
		*value = pAPriv->VideoStd * 2 + pPPriv->Plug - 1;
    } else if (attribute == xvBrightness)
	*value = pPPriv->Attribute[0];
    else if (attribute == xvContrast)
	*value = pPPriv->Attribute[1];
    else if (attribute == xvSaturation)
	*value = pPPriv->Attribute[2];
    else if (attribute == xvHue)
	*value = pPPriv->Attribute[3];
    else if (attribute == xvInterlace)
	*value = pPPriv->Attribute[4];
    else if (attribute == xvFilter)
	*value = pPPriv->Attribute[5];
    else if (attribute == xvBkgColor)
	*value = pPPriv->Attribute[6];
    else if (attribute == xvAlpha)
	*value = pPPriv->Attribute[7];
    else
	return BadMatch;

    DEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3,
	"GPA attr=%d val=%d port=%d\n",
	attribute, *value, PORTNUM(pPPriv)));

    return Success;
}

static void
Permedia2QueryBestSize(ScrnInfoPtr pScrn, Bool motion,
    short vid_w, short vid_h, short drw_w, short drw_h,
    unsigned int *p_w, unsigned int *p_h, pointer data)
{
    *p_w = drw_w;
    *p_h = drw_h;
}

static int
Permedia2QueryImageAttributes(ScrnInfoPtr pScrn,
    int id, unsigned short *width, unsigned short *height,
    int *pitches, int *offsets)
{
    int i, pitch;

    *width = CLAMP(*width, 1, 2047);
    *height = CLAMP(*height, 1, 2047);

    if (offsets)
	offsets[0] = 0;

    switch (id) {
    case LE4CC('Y','V','1','2'): /* Planar YVU 4:2:0 (emulated) */
	*width = CLAMP((*width + 1) & ~1, 2, 2046);
	*height = CLAMP((*height + 1) & ~1, 2, 2046);

	pitch = *width; /* luma component */

	if (offsets) {
	    offsets[1] = pitch * *height;
	    offsets[2] = offsets[1] + (offsets[1] >> 2);
	}

	if (pitches) {
	    pitches[0] = pitch;
	    pitches[1] = pitches[2] = pitch >> 1;
	}

	return pitch * *height * 3 / 2;

    case LE4CC('Y','U','Y','2'): /* Packed YUYV 4:2:2 */
    case LE4CC('U','Y','V','Y'): /* Packed UYVY 4:2:2 */
	*width = CLAMP((*width + 1) & ~1, 2, 2046);

	pitch = *width * 2;

	if (pitches)
	    pitches[0] = pitch;

	return pitch * *height;

    default:
	for (i = 0; i < ENTRIES(ScalerImages); i++)
	    if (ScalerImages[i].id == id)
		break;

	if (i >= ENTRIES(ScalerImages))
	    break;

	pitch = *width * (ScalerImages[i].bits_per_pixel >> 3);

	if (pitches)
	    pitches[0] = pitch;

	return pitch * *height;
    }

    return 0;
}

static void
RestoreVideo(AdaptorPrivPtr pAPriv)
{
    GLINTPtr pGlint = GLINTPTR(pAPriv->pScrn);

    GLINT_WRITE_REG(pAPriv->dFifoControl, PMFifoControl);
    GLINT_WRITE_REG(0, VSABase + VSControl);
    GLINT_WRITE_REG(0, VSBBase + VSControl);
    usleep(160000);
    GLINT_MASK_WRITE_REG(VS_UnitMode_ROM, ~VS_UnitMode_Mask, VSConfiguration);
}

static void
InitializeVideo(AdaptorPrivPtr pAPriv)
{
    GLINTPtr pGlint = GLINTPTR(pAPriv->pScrn);
    int i;

    GLINT_WRITE_REG(0, VSABase + VSControl);
    GLINT_WRITE_REG(0, VSBBase + VSControl);

#if 0
    GLINT_MASK_WRITE_REG(0, ~(VSAIntFlag | VSBIntFlag), IntEnable);
    GLINT_WRITE_REG(VSAIntFlag | VSBIntFlag, IntFlags); /* Reset */
#endif

    for (i = 0x0018; i <= 0x00B0; i += 8) {
        GLINT_WRITE_REG(0, VSABase + i);
        GLINT_WRITE_REG(0, VSBBase + i);
    }

    GLINT_WRITE_REG((0 << 8) | (132 << 0), VSABase + VSFifoControl);
    GLINT_WRITE_REG((0 << 8) | (132 << 0), VSBBase + VSFifoControl);

    GLINT_MASK_WRITE_REG(
        VS_UnitMode_AB8 |
        VS_GPBusMode_A |
     /* VS_HRefPolarityA | */
        VS_VRefPolarityA |
        VS_VActivePolarityA |
     /* VS_UseFieldA | */
        VS_FieldPolarityA |
     /* VS_FieldEdgeA | */
     /* VS_VActiveVBIA | */
	VS_InterlaceA |
	VS_ReverseDataA |

     /* VS_HRefPolarityB | */
        VS_VRefPolarityB |
        VS_VActivePolarityB |
     /* VS_UseFieldB | */
        VS_FieldPolarityB |
     /* VS_FieldEdgeB | */
     /* VS_VActiveVBIB | */
        VS_InterlaceB |
     /* VS_ColorSpaceB_RGB | */
     /* VS_ReverseDataB | */
     /* VS_DoubleEdgeB | */
        0, ~0x1FFFFE0F, VSConfiguration);

    pAPriv->dFifoControl = GLINT_READ_REG(PMFifoControl);
    GLINT_WRITE_REG((12 << 8) | 8, PMFifoControl);
}

static Bool
xvipcHandshake(PortPrivPtr pPPriv, int op, Bool block)
{
    int r;
    int brake = 150;

    xvipc.magic = XVIPC_MAGIC;
    xvipc.op = op;
    xvipc.block = block;

    if (pPPriv) {
	AdaptorPrivPtr pAPriv = pPPriv->pAdaptor;

	xvipc.pm2p = pAPriv->pm2p;
	xvipc.pAPriv = pAPriv;
	xvipc.port = PORTNUM(pPPriv);
    } else {
	xvipc.pm2p = (void *) -1;
	xvipc.pAPriv = NULL;
	xvipc.port = -1;
    }

    for (;;) {
	if (brake-- <= 0)
	    return FALSE; /* I brake for bugs. */

	DEBUG(xf86MsgVerb(X_INFO, 4,
	    "PM2 XVIPC send op=%d bl=%d po=%d a=%d b=%d c=%d\n",
	    xvipc.op, xvipc.block, xvipc.port, xvipc.a, xvipc.b, xvipc.c));

	r = ioctl(xvipc_fd, VIDIOC_PM2_XVIPC, (void *) &xvipc);

	DEBUG(xf86MsgVerb(X_INFO, 4,
	    "PM2 XVIPC recv op=%d bl=%d po=%d a=%d b=%d c=%d err=%d/%d\n",
	    xvipc.op, xvipc.block, xvipc.port, xvipc.a, xvipc.b, xvipc.c, r, errno));

	switch (xvipc.op) {
	case OP_ALLOC:
	{
	    AdaptorPrivPtr pAPriv = xvipc.pAPriv;
	    ScrnInfoPtr pScrn = pAPriv->pScrn;
	    GLINTPtr pGlint = GLINTPTR(pScrn);
	    FBAreaPtr pFBArea = NULL;
	    LFBAreaPtr pLFBArea;

	    xvipc.a = -1;

	    pLFBArea = xalloc(sizeof(LFBAreaRec));

	    if (pLFBArea) {
		pLFBArea->pFBArea = pFBArea =
		    xf86AllocateLinearOffscreenArea(pScrn->pScreen,
		        xvipc.b >> BPPSHIFT(pGlint), 2, NULL, NULL, NULL);

		if (pFBArea) {
		    /* xvipc.a = pFBArea->linear; */
		    pLFBArea->Linear = xvipc.a =
			((pFBArea->box.y1 * pScrn->displayWidth) +
			    pFBArea->box.x1) << BPPSHIFT(pGlint);
		} else
		    xfree(pLFBArea);
	    }

	    /* Report results */

	    if (ioctl(xvipc_fd, VIDIOC_PM2_XVIPC, (void *) &xvipc) != 0)
		if (pFBArea) {
		    xf86FreeOffscreenArea(pFBArea);
		    xfree(pLFBArea);
		    pFBArea = NULL;
		}

	    if (pFBArea) {
		pLFBArea->Next = pAPriv->LFBList;
		pAPriv->LFBList = pLFBArea;
	    }

	    DEBUG(xf86MsgVerb(X_INFO, 3, "PM2 XVIPC alloc addr=%d=0x%08x pFB=%p\n",
		xvipc.a, xvipc.a, pFBArea));

	    goto event;
	}

	case OP_FREE:
	{
	    AdaptorPrivPtr pAPriv = xvipc.pAPriv;
	    LFBAreaPtr pLFBArea, *ppLFBArea;

	    for (ppLFBArea = &pAPriv->LFBList; (pLFBArea = *ppLFBArea);
		ppLFBArea = &pLFBArea->Next)
		if (pLFBArea->Linear == xvipc.a)
		    break;

	    if (!pLFBArea)
		xvipc.a = -1;

	    DEBUG(xf86MsgVerb(X_INFO, 3, "PM2 XVIPC free addr=%d=0x%08x pFB=%p\n",
		xvipc.a, xvipc.a, pLFBArea ? pLFBArea->pFBArea : NULL));

	    if (ioctl(xvipc_fd, VIDIOC_PM2_XVIPC, (void *) &xvipc) == 0 && pLFBArea) {
		xf86FreeOffscreenArea(pLFBArea->pFBArea);
		*ppLFBArea = pLFBArea->Next;
		xfree(pLFBArea);
	    }

	    goto event;
	}

	case OP_UPDATE:
	{
	    AdaptorPrivPtr pAPriv = xvipc.pAPriv;
	    PortPrivPtr pPPriv;

	    pPPriv = &pAPriv->Port[0];

	    if (pPPriv->VideoOn > VIDEO_OFF && xvipc.a > 0) {
	        pPPriv->FrameAcc += pPPriv->FramesPerSec;
	        if (pPPriv->FrameAcc >= pAPriv->FramesPerSec) {
		    pPPriv->FrameAcc -= pAPriv->FramesPerSec;

		    /* Asynchronous resizing caused by kernel app */

		    if (xvipc.c != pPPriv->fw ||
		        xvipc.d != pPPriv->fh) {
			pPPriv->vx = (pPPriv->vx * xvipc.c) / pPPriv->fw;
			pPPriv->vw = (pPPriv->vw * xvipc.c) / pPPriv->fw;
			pPPriv->vy = (pPPriv->vy * xvipc.d) / pPPriv->fh;
			pPPriv->vh = (pPPriv->vh * xvipc.d) / pPPriv->fh;

			pPPriv->fw = xvipc.c;
			pPPriv->fh = xvipc.d;
			pPPriv->BufferPProd = xvipc.e;

			RemakePutCookies(pPPriv, NULL);
		    }

		    PutYUV(pPPriv, xvipc.a, FORMAT_YUYV, 1, 0);

		    if (pPPriv->VideoOn == VIDEO_ONE_SHOT)
			pPPriv->VideoOn = VIDEO_OFF;
		}
	    } else
		if (pPPriv->StopDelay >= 0 && !(pPPriv->StopDelay--)) {
		    StopVideoStream(pPPriv, TRUE);
		    RestoreVideoStd(pAPriv);
		}

	    pPPriv = &pAPriv->Port[1];

	    if (pPPriv->VideoOn > VIDEO_OFF && xvipc.b > 0) {
	        pPPriv->FrameAcc += pPPriv->FramesPerSec;
		if (pPPriv->FrameAcc >= pAPriv->FramesPerSec) {
		    pPPriv->FrameAcc -= pAPriv->FramesPerSec;

		    pPPriv->BufferBase[0] = xvipc.b;

		    /* Output is always exclusive, no async resizing */

		    GetYUV(pPPriv);

		    if (pPPriv->VideoOn == VIDEO_ONE_SHOT)
			pPPriv->VideoOn = VIDEO_OFF;
		}
	    } else
		if (pPPriv->StopDelay >= 0 && !(pPPriv->StopDelay--)) {
		    StopVideoStream(pPPriv, TRUE);
		    RestoreVideoStd(pAPriv);
		}

	    /* Fall through */
	}

	default:
	event:
	    if (xvipc.op == op)
		return r == 0;

	    xvipc.op = OP_EVENT;
	    xvipc.block = block;
	}
    }

    return TRUE;
}

static void
Permedia2ReadInput(int fd, pointer unused)
{
    xvipcHandshake(NULL, OP_EVENT, FALSE);
}

static Bool
xvipcOpen(char *name, ScrnInfoPtr pScrn)
{
    const char *osname;

    if (xvipc_fd >= 0)
	return TRUE;

    xf86GetOS(&osname, NULL, NULL, NULL);

    if (!osname || strcmp(osname, "linux"))
	return FALSE;

    for (;;) {
	DEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 2,
	    "XVIPC probing device %s\n", name));

    	if ((xvipc_fd = open(name, O_RDWR /* | O_TRUNC */, 0)) < 0)
	    break;

	xvipc.magic = XVIPC_MAGIC;
	xvipc.pm2p = (void *) -1;
	xvipc.pAPriv = NULL;
	xvipc.op = OP_CONNECT;
	xvipc.a = 0;
	xvipc.b = 0;
	xvipc.c = 0;
	xvipc.d = 0;

	if (ioctl(xvipc_fd, VIDIOC_PM2_XVIPC, (void *) &xvipc) < 0 || xvipc.pm2p)
	    break;

	if (xvipc.c != XVIPC_VERSION) {
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		       "Your Permedia 2 kernel driver %d.%d uses XVIPC protocol "
		       "V.%d while this Xv driver expects V.%d. Please update.\n",
		       xvipc.a, xvipc.b, xvipc.c, XVIPC_VERSION);
	    break;
	}

	xf86AddInputHandler(xvipc_fd, Permedia2ReadInput, NULL);

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Xv driver opened %s\n", name);

	return TRUE;
    }

    if (xvipc_fd >= 0)
	close(xvipc_fd);

    xvipc_fd = -1;

    xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "Cannot find Permedia 2 kernel driver.\n");

    return FALSE;
}

static void
DeleteAdaptorPriv(AdaptorPrivPtr pAPriv)
{
    int i;

    if (pAPriv->VideoIO) {
	StopVideoStream(&pAPriv->Port[0], TRUE);
	StopVideoStream(&pAPriv->Port[1], TRUE);
    }

    for (i = 0; i < 6; i++) {
        FreeBuffers(&pAPriv->Port[i]);
	FreeCookies(&pAPriv->Port[i]);
    }

    TimerFree(pAPriv->Timer);

    if (pAPriv->VideoIO) {
	if (pAPriv->pm2p)
	    xvipcHandshake(&pAPriv->Port[0], OP_DISCONNECT, TRUE);
	else {
	    xf86DestroyI2CDevRec(&pAPriv->Port[0].I2CDev, FALSE);
	    xf86DestroyI2CDevRec(&pAPriv->Port[1].I2CDev, FALSE);

	    RestoreVideo(pAPriv);
	}
    }

    xfree(pAPriv);
}

static AdaptorPrivPtr
NewAdaptorPriv(ScrnInfoPtr pScrn, Bool VideoIO)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    AdaptorPrivPtr pAPriv = (AdaptorPrivPtr) xcalloc(1, sizeof(AdaptorPrivRec));
    int i;

    if (!pAPriv)
	return NULL;

    pAPriv->pScrn = pScrn;
    
    for (i = 0; i < PORTS; i++)
	pAPriv->Port[i].pAdaptor = pAPriv;

    switch (pScrn->depth) {
    case 8:
        pAPriv->dDitherMode =
	    (0 << 10) |			/* BGR */
	    (1 << 1) |			/* Dither */
	    ((5 & 0x10) << 12) |
	    ((5 & 0x0F) << 2) |		/* 3:3:2f */
	    UNIT_ENABLE;
        pAPriv->dAlphaBlendMode =
	    (0 << 13) |
	    ((5 & 0x10) << 12) |
	    ((5 & 0x0F) << 8) |
	    (84 << 1) |			/* Blend (decal) RGB */
	    UNIT_ENABLE;
	pAPriv->dTextureDataFormat =
	    (1 << 4) |			/* No alpha */
	    ((14 & 0x10) << 2) |
	    ((14 & 0x0F) << 0);		/* CI8 */
	break;

    case 15:
        pAPriv->dDitherMode =
	    (1 << 10) |			/* RGB */
	    ((1 & 0x10) << 12) |
	    ((1 & 0x0F) << 2) |		/* 5:5:5:1f */
	    UNIT_ENABLE;
        pAPriv->dAlphaBlendMode =
	    (1 << 13) |
	    ((1 & 0x10) << 12) |
	    ((1 & 0x0F) << 8) |
	    (84 << 1) |
	    UNIT_ENABLE;
	pAPriv->dTextureDataFormat =
    	    (1 << 5) |			/* RGB */
	    (1 << 4) |
	    ((1 & 0x10) << 2) |
	    ((1 & 0x0F) << 0);
	break;

    case 16:
        pAPriv->dDitherMode =
	    (1 << 10) |			/* RGB */
	    ((16 & 0x10) << 12) |
	    ((16 & 0x0F) << 2) |	/* 5:6:5f */
	    UNIT_ENABLE;
        pAPriv->dAlphaBlendMode =
	    (1 << 13) |
	    ((16 & 0x10) << 12) |
	    ((16 & 0x0F) << 8) |
	    (84 << 1) |
	    UNIT_ENABLE;
	pAPriv->dTextureDataFormat =
	    (1 << 5) |
	    (1 << 4) |
	    ((16 & 0x10) << 2) |
	    ((16 & 0x0F) << 0);
	break;

    case 24:
        pAPriv->dDitherMode =
	    (1 << 10) |			/* RGB */
	    ((0 & 0x10) << 12) |
	    ((0 & 0x0F) << 2) |		/* 8:8:8:8 */
	    UNIT_ENABLE;
        pAPriv->dAlphaBlendMode =
	    (1 << 13) |
	    ((0 & 0x10) << 12) |
	    ((0 & 0x0F) << 8) |
	    (84 << 1) |
	    UNIT_ENABLE;
	pAPriv->dTextureDataFormat =
	    (1 << 5) |
	    (1 << 4) |
	    ((0 & 0x10) << 2) |
	    ((0 & 0x0F) << 0);
	break;

    default:
	xfree(pAPriv);
	return NULL;
    }

    pAPriv->VideoIO = VideoIO;

    if (VideoIO) {
	if (xvipc_fd >= 0) {
	    /* Initial handshake, take over control of this head */

	    xvipc.magic = XVIPC_MAGIC;
	    xvipc.pm2p = (void *) -1;		/* Kernel head ID */
	    xvipc.pAPriv = pAPriv;		/* Server head ID */
	    xvipc.op = OP_CONNECT;

	    xvipc.a = pGlint->PciInfo->bus;
	    xvipc.b = pGlint->PciInfo->device;
	    xvipc.c = pGlint->PciInfo->func;

	    xvipc.d = pScrn->videoRam << 10;	/* XF86Config overrides probing */

	    if (ioctl(xvipc_fd, VIDIOC_PM2_XVIPC, (void *) &xvipc) < 0) {
		if (errno == EBUSY)
		    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
			       "Another application already opened the Permedia 2 "
			       "kernel driver for this board. To enable "
			       "shared access please start the server first.\n");
		else
		    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
			       "Failed to initialize kernel backbone "
			       "due to error %d: %s.\n", errno, strerror(errno));
		goto failed;
	    }

	    pAPriv->pm2p = xvipc.pm2p;
	} else {
	    InitializeVideo(pAPriv);

	    if (!xf86I2CProbeAddress(pGlint->VSBus, SAA7111_SLAVE_ADDRESS))
    		goto failed;

	    pAPriv->Port[0].I2CDev.DevName = "Decoder SAA 7111A";
    	    pAPriv->Port[0].I2CDev.SlaveAddr = SAA7111_SLAVE_ADDRESS;
	    pAPriv->Port[0].I2CDev.pI2CBus = pGlint->VSBus;

	    if (!xf86I2CDevInit(&pAPriv->Port[0].I2CDev))
		goto failed;

	    if (!xf86I2CWriteVec(&pAPriv->Port[0].I2CDev, DecInitVec, ENTRIES(DecInitVec) / 2))
		goto failed;

	    if (!xf86I2CProbeAddress(pGlint->VSBus, SAA7125_SLAVE_ADDRESS))
		goto failed;

	    pAPriv->Port[1].I2CDev.DevName = "Encoder SAA 7125";
	    pAPriv->Port[1].I2CDev.SlaveAddr = SAA7125_SLAVE_ADDRESS;
    	    pAPriv->Port[1].I2CDev.pI2CBus = pGlint->VSBus;

	    if (!xf86I2CDevInit(&pAPriv->Port[1].I2CDev))
		goto failed;

	    if (!xf86I2CWriteVec(&pAPriv->Port[1].I2CDev, EncInitVec, ENTRIES(EncInitVec) / 2))
		goto failed;
	}

	if (SetVideoStd(&pAPriv->Port[0], PAL) != Success ||
	    SetPlug(&pAPriv->Port[0], 0) != Success ||  /* composite */
	    SetPlug(&pAPriv->Port[1], 1) != Success)    /* composite-adaptor */
	    goto failed;

	pAPriv->Port[1].VideoStdReq = pAPriv->Port[0].VideoStdReq;

	pAPriv->Port[0].BuffersRequested = 2;
	pAPriv->Port[1].BuffersRequested = 1;

	for (i = 0; i < 2; i++) {
    	    pAPriv->Port[i].fw = 704;
    	    pAPriv->Port[i].fh = 576;
    	    pAPriv->Port[i].FramesPerSec = 30;
    	    pAPriv->Port[i].BufferPProd = partprodPermedia[704 >> 5];
	}

	SetAttr(&pAPriv->Port[0], 0, 0);	/* Brightness (-1000..+1000) */
	SetAttr(&pAPriv->Port[0], 1, 0);	/* Contrast (-3000..+1000) */
	SetAttr(&pAPriv->Port[0], 2, 0);	/* Color saturation (-3000..+1000) */
	SetAttr(&pAPriv->Port[0], 3, 0);	/* Hue (-1000..+1000) */

	pAPriv->Port[0].Attribute[4] = 1;	/* Interlaced (0 = not, 1 = yes,
						    2 = double scan 50/60 Hz) */
	pAPriv->Port[0].Attribute[5] = 0;	/* Bilinear Filter (Bool) */

	pAPriv->Port[1].Attribute[4] = 1;	/* Interlaced (Bool) */
	pAPriv->Port[1].Attribute[5] = 0;	/* Bilinear Filter (Bool) */

	SetBkgCol(&pAPriv->Port[1], 0x000000);	/* BkgColor 0x00RRGGBB */
    } /* VideoIO */

    if (!(pAPriv->Timer = TimerSet(NULL, 0, 0, TimerCallback, pAPriv)))
        goto failed;

    for (i = 0; i < PORTS; i++)
    	pAPriv->Port[i].StopDelay = -1;

    /* Frontend scaler */    

    for (i = 2; i < 6; i++) {
	pAPriv->Port[i].fw = 0;
	pAPriv->Port[i].fh = 0;
	pAPriv->Port[i].BuffersRequested = 1;
	pAPriv->Delay = 125;
	pAPriv->Instant = 1000 / 25;

	if (!VideoIO || pAPriv->pm2p) {
	    pAPriv->Delay = 5;
	    pAPriv->Instant = 1000;
	}

	pAPriv->Port[i].Attribute[5] = 0;	/* Bilinear Filter (Bool) */
	pAPriv->Port[i].Attribute[7] = 0;	/* Alpha Enable (Bool) */
    }

    return pAPriv;

failed:

    DeleteAdaptorPriv(pAPriv);

    return NULL;
}


/*
 *  Glint interface
 */

void
Permedia2VideoEnterVT(ScrnInfoPtr pScrn)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    AdaptorPrivPtr pAPriv;

    DEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 2, "Xv enter VT\n"));

    for (pAPriv = AdaptorPrivList; pAPriv != NULL; pAPriv = pAPriv->Next)
	if (pAPriv->pScrn == pScrn) {
	    if (pAPriv->VideoIO) {
		if (pAPriv->pm2p)
		    xvipcHandshake(&pAPriv->Port[0], OP_ENTER, TRUE);
		else {
		    InitializeVideo(pAPriv);

		    xf86I2CWriteVec(&pAPriv->Port[1].I2CDev, EncInitVec, ENTRIES(EncInitVec) / 2);
		}

		SetVideoStd(&pAPriv->Port[0], pAPriv->VideoStd);
		SetPlug(&pAPriv->Port[0], pAPriv->Port[0].Plug);
		SetPlug(&pAPriv->Port[1], pAPriv->Port[1].Plug);
	    }

	    if (pGlint->NoAccel)
		Permedia2InitializeEngine(pScrn);

	    break;
	}
}

void
Permedia2VideoLeaveVT(ScrnInfoPtr pScrn)
{
    AdaptorPrivPtr pAPriv;

    for (pAPriv = AdaptorPrivList; pAPriv != NULL; pAPriv = pAPriv->Next)
	if (pAPriv->pScrn == pScrn) {
	    if (pAPriv->VideoIO) {
		StopVideoStream(&pAPriv->Port[0], TRUE);
		StopVideoStream(&pAPriv->Port[1], TRUE);

		if (pAPriv->pm2p)
		    xvipcHandshake(&pAPriv->Port[0], OP_LEAVE, TRUE);
		else
		    RestoreVideo(pAPriv);
	    }
	    break;
	}

    DEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 2, "Elvis left the building\n"));
}

void
Permedia2VideoUninit(ScrnInfoPtr pScrn)
{
    AdaptorPrivPtr pAPriv, *ppAPriv;

    for (ppAPriv = &AdaptorPrivList; (pAPriv = *ppAPriv); ppAPriv = &(pAPriv->Next))
	if (pAPriv->pScrn == pScrn) {
	    *ppAPriv = pAPriv->Next;
	    DeleteAdaptorPriv(pAPriv);
	    break;
	}

    if (xvipc_fd >= 0) {
	close(xvipc_fd);
	xvipc_fd = -1;
    }

    DEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 2, "Xv cleanup\n"));
}

void
Permedia2VideoInit(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    GLINTPtr pGlint = GLINTPTR(pScrn);
    AdaptorPrivPtr pAPriv;
    pointer options[3];
    DevUnion Private[PORTS];
    XF86VideoAdaptorRec VAR[ADAPTORS];
    XF86VideoAdaptorPtr VARPtrs[ADAPTORS];
    Bool VideoIO = TRUE;
    int i;

    switch (pGlint->Chipset) {
    case PCI_VENDOR_TI_CHIP_PERMEDIA2:
    case PCI_VENDOR_3DLABS_CHIP_PERMEDIA2:
    case PCI_VENDOR_3DLABS_CHIP_PERMEDIA2V:
        break;

    default:
        return;
    }

    options[0] = NULL;	/* VideoAdaptor "input" subsection options */
    options[1] = NULL;	/* VideoAdaptor "output" subsection options */
    options[2] = NULL;	/* VideoAdaptor options */

    for (i = 0;; i++) {
	char *adaptor; /* receives VideoAdaptor section identifier */

	if (!options[0])
	    options[0] = xf86FindXvOptions(pScreen->myNum, i, "input", &adaptor, options[2] ? NULL : &options[2]);

	if (!options[1])
	    options[1] = xf86FindXvOptions(pScreen->myNum, i, "output", &adaptor, options[2] ? NULL : &options[2]);

	if (!adaptor) {
	    if (!i) /* VideoAdaptor reference enables Xv vio driver */
		VideoIO = FALSE;
	    break;
	} else if (options[0] && options[1])
	    break;
    }

    if (VideoIO)
	switch (pciReadLong(pGlint->PciTag, PCI_SUBSYSTEM_ID_REG)) {
	case PCI_SUBSYSTEM_ID_WINNER_2000_P2A:
	case PCI_SUBSYSTEM_ID_WINNER_2000_P2C:
	case PCI_SUBSYSTEM_ID_GLORIA_SYNERGY_P2A:
	case PCI_SUBSYSTEM_ID_GLORIA_SYNERGY_P2C:
	    break;

	default:
	    xf86DrvMsgVerb(pScrn->scrnIndex, X_PROBED, 1, "No Xv vio support for this board\n");
	    VideoIO = FALSE;
	}

    if (pGlint->NoAccel && !VideoIO)
	return;

    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 1, "Initializing Xv driver rev. 4\n");

    if (VideoIO) {
	for (i = 0; i <= 2; i++) {
	    xf86ProcessOptions(pScrn->scrnIndex, options[i],
		(i == 0) ? InputOptions :
		(i == 1) ? OutputOptions :
			   AdaptorOptions);

	    xf86ShowUnusedOptions(pScrn->scrnIndex, options[i]);
	}

	if (xf86IsOptionSet(AdaptorOptions, OPTION_DEVICE)) {
    	    if (!xvipcOpen(xf86GetOptValString(AdaptorOptions, OPTION_DEVICE), pScrn))
		VideoIO = FALSE;
	}
    }

    if (!(pAPriv = NewAdaptorPriv(pScrn, VideoIO))) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Xv driver initialization failed\n");
	return;
    }

    if (VideoIO) {
	int n;

	if (xf86GetOptValInteger(InputOptions, OPTION_BUFFERS, &n))
	    pAPriv->Port[0].BuffersRequested = CLAMP(n, 1, 2);
	if (xf86GetOptValInteger(InputOptions, OPTION_FPS, &n))
	    pAPriv->Port[0].FramesPerSec = CLAMP(n, 1, 30);

	if (xf86GetOptValInteger(OutputOptions, OPTION_BUFFERS, &n))
	    pAPriv->Port[1].BuffersRequested = 1;
	if (xf86GetOptValInteger(OutputOptions, OPTION_FPS, &n))
	    pAPriv->Port[1].FramesPerSec = CLAMP(n, 1, 30);	
    }

    if (pGlint->NoAccel) {
	BoxRec AvailFBArea;

	xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "Xv driver overrides NoAccel option\n");

	Permedia2InitializeEngine(pScrn);

	AvailFBArea.x1 = 0;
	AvailFBArea.y1 = 0;
	AvailFBArea.x2 = pScrn->displayWidth;
	AvailFBArea.y2 = pGlint->FbMapSize /
	    (pScrn->displayWidth * pScrn->bitsPerPixel / 8);

	xf86InitFBManager(pScreen, &AvailFBArea);
    }

#if defined(XFree86LOADER) && 0
    if (xf86LoaderCheckSymbol("xf86InitLinearFBManagerRegion")) {
	int last = pGlint->FbMapSize / (pScrn->bitsPerPixel / 8) - 1;
	BoxRec AvailFBArea;
	RegionPtr Region;

	AvailFBArea.x1 = 0;
	AvailFBArea.y1 = pScrn->virtualY;
	AvailFBArea.x2 = last % pScrn->displayWidth + 1;
	AvailFBArea.y2 = last / pScrn->displayWidth + 1;

	DEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 2,
	    "Using linear FB %d,%d-%d,%d pitch %d (%dk)\n",
	    AvailFBArea.x1, AvailFBArea.y1, AvailFBArea.x2, AvailFBArea.y2,
	    pScrn->displayWidth, (((AvailFBArea.y2 - AvailFBArea.y1)
	    * pScrn->displayWidth) << BPPSHIFT(pGlint)) / 1024));

	Region = xf86LinearFBRegion(pScreen, &AvailFBArea, pScrn->displayWidth);
	xf86InitLinearFBManagerRegion(pScreen, Region);
	REGION_DESTROY(pScreen, Region);
    }
#endif

    memset(VAR, 0, sizeof(VAR));

    for (i = 0; i < PORTS; i++)
	Private[i].ptr = (pointer) &pAPriv->Port[i];

    for (i = 0; i < ADAPTORS; i++) {
	VARPtrs[i] = &VAR[i];
	switch (i) {
	case 0:
	    VAR[i].name = "Permedia 2 Video Input";
	    VAR[i].type = XvInputMask | XvWindowMask | XvVideoMask | XvStillMask;
	    VAR[i].nPorts = 1;
	    VAR[i].pPortPrivates = &Private[0];
	    VAR[i].nAttributes	= ENTRIES(InputVideoAttributes);
	    VAR[i].pAttributes	= InputVideoAttributes;
	    VAR[i].nEncodings	= ENTRIES(InputVideoEncodings);
	    VAR[i].pEncodings	= InputVideoEncodings;
	    VAR[i].nFormats	= ENTRIES(InputVideoFormats);
	    VAR[i].pFormats	= InputVideoFormats;
	    break;

	case 1:
	    VAR[i].name = "Permedia 2 Video Output";
	    VAR[i].type = XvOutputMask | XvWindowMask | XvVideoMask | XvStillMask;
	    VAR[i].nPorts = 1;
	    VAR[i].pPortPrivates = &Private[1];
	    VAR[i].nAttributes	= ENTRIES(OutputVideoAttributes);
	    VAR[i].pAttributes	= OutputVideoAttributes;
	    VAR[i].nEncodings	= ENTRIES(OutputVideoEncodings);
	    VAR[i].pEncodings	= OutputVideoEncodings;
	    VAR[i].nFormats	= ENTRIES(OutputVideoFormats);
	    VAR[i].pFormats	= OutputVideoFormats;
	    break;

	case 2:
	    VAR[i].name = "Permedia 2 Frontend Scaler";
	    VAR[i].type = XvInputMask | XvWindowMask | XvImageMask;
	    VAR[i].nPorts = 3;
	    VAR[i].pPortPrivates = &Private[2];
	    VAR[i].nAttributes	= ENTRIES(ScalerAttributes);
	    VAR[i].pAttributes	= ScalerAttributes;
	    VAR[i].nEncodings	= ENTRIES(ScalerEncodings);
	    VAR[i].pEncodings	= ScalerEncodings;
	    VAR[i].nFormats	= ENTRIES(ScalerVideoFormats);
	    VAR[i].pFormats	= ScalerVideoFormats;
	    VAR[i].nImages	= ENTRIES(ScalerImages);
	    VAR[i].pImages	= ScalerImages;
	    break;
	}

	VAR[i].PutVideo = Permedia2PutVideo;
	VAR[i].PutStill = Permedia2PutStill;
	VAR[i].GetVideo = Permedia2GetVideo;
	VAR[i].GetStill = Permedia2GetStill;
	VAR[i].StopVideo = Permedia2StopVideo;
	VAR[i].SetPortAttribute = Permedia2SetPortAttribute;
	VAR[i].GetPortAttribute = Permedia2GetPortAttribute;
	VAR[i].QueryBestSize = Permedia2QueryBestSize;
	VAR[i].PutImage = Permedia2PutImage;
	VAR[i].QueryImageAttributes = Permedia2QueryImageAttributes;
    }

    if (VideoIO ? xf86XVScreenInit(pScreen, &VARPtrs[0], 3) :
		  xf86XVScreenInit(pScreen, &VARPtrs[2], 1)) {
	char *s;

	xvEncoding	= MAKE_ATOM(XV_ENCODING);
	xvHue		= MAKE_ATOM(XV_HUE);
	xvSaturation	= MAKE_ATOM(XV_SATURATION);
	xvBrightness	= MAKE_ATOM(XV_BRIGHTNESS);
	xvContrast	= MAKE_ATOM(XV_CONTRAST);
	xvInterlace	= MAKE_ATOM(XV_INTERLACE);
	xvFilter	= MAKE_ATOM(XV_FILTER);
	xvBkgColor	= MAKE_ATOM(XV_BKGCOLOR);
	xvAlpha		= MAKE_ATOM(XV_ALPHA);

	pAPriv->Next = AdaptorPrivList;
	AdaptorPrivList = pAPriv;

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Xv frontend scaler enabled\n");

	if (VideoIO) {
	    if ((s = xf86GetOptValString(InputOptions, OPTION_ENCODING)))
	        for (i = 0; i < ENTRIES(InputVideoEncodings); i++)
		    if (!strncmp(s, InputVideoEncodings[i].name, strlen(s))) {
			Permedia2SetPortAttribute(pScrn, xvEncoding, i, (pointer) &pAPriv->Port[0]);
			break;
		    }

	    if ((s = xf86GetOptValString(OutputOptions, OPTION_ENCODING)))
		for (i = 0; i < ENTRIES(OutputVideoEncodings); i++)
		    if (!strncmp(s, OutputVideoEncodings[i].name, strlen(s))) {
			Permedia2SetPortAttribute(pScrn, xvEncoding, i, (pointer) &pAPriv->Port[1]);
			break;
		    }

	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Xv vio driver %senabled\n",
		pAPriv->pm2p ? "with kernel backbone " : "");
	}
    } else {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Xv initialization failed\n");
	DeleteAdaptorPriv(pAPriv);
    }
}

#endif /* XvExtension */
