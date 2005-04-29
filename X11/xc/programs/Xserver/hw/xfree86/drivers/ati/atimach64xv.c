/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/atimach64xv.c,v 1.8 2004/01/05 16:42:03 tsi Exp $ */
/*
 * Copyright 2003 through 2004 by Marc Aurele La France (TSI @ UQV), tsi@xfree86.org
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of Marc Aurele La France not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Marc Aurele La France makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as-is" without express or implied warranty.
 *
 * MARC AURELE LA FRANCE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO
 * EVENT SHALL MARC AURELE LA FRANCE BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "ati.h"
#include "atiaccel.h"
#include "atichip.h"
#include "atimach64accel.h"
#include "atimach64io.h"
#include "atimach64xv.h"

#include "Xv.h"
#include "fourcc.h"

#define MAKE_ATOM(string) MakeAtom(string, strlen(string), TRUE)
#define MaxScale          (CARD32)(CARD16)(-1)

static unsigned long ATIMach64XVAtomGeneration = (unsigned long)(-1);

static XF86VideoEncodingRec ATIMach64VideoEncoding_A[] =
{
    { 0, "XV_IMAGE", 384, 2048, {1, 1} }
};
#define nATIMach64VideoEncoding_A NumberOf(ATIMach64VideoEncoding_A)

static XF86VideoEncodingRec ATIMach64VideoEncoding_B[] =
{
    { 0, "XV_IMAGE", 720, 2048, {1, 1} }
};
#define nATIMach64VideoEncoding_B NumberOf(ATIMach64VideoEncoding_B)

/* nATIMach64VideoEncoding_[AB] should be equal */
#define nATIMach64VideoEncoding nATIMach64VideoEncoding_A

static XF86VideoFormatRec ATIMach64VideoFormat[] =
{
    { 8, TrueColor},
    { 8, DirectColor},
    { 8, PseudoColor},
    { 8, GrayScale},
    { 8, StaticGray},
    { 8, StaticColor},
    {15, TrueColor},
    {16, TrueColor},
    {24, TrueColor},
    {15, DirectColor},
    {16, DirectColor},
    {24, DirectColor}
};
#define nATIMach64VideoFormat NumberOf(ATIMach64VideoFormat)

static XF86AttributeRec ATIMach64Attribute[] =
{
    /* These are only supported on the Rage Pro and later ... */
    {
        XvSettable | XvGettable,
        -1000, 1000,
        "XV_SATURATION"
    },
    {
        XvSettable | XvGettable,
        -1000, 1000,
        "XV_BRIGHTNESS"
    },
    {
        XvSettable | XvGettable,
        -1000, 1000,
        "XV_COLOUR"
    },
    {
        XvSettable | XvGettable,
        -1000, 1000,
        "XV_COLOR"
    },

    /* Local attributes, odds and ends for compatibility, etc... */
    {
        XvSettable | XvGettable,
        0, 1,
        "XV_AUTOPAINT_COLOURKEY"
    },
    {
        XvSettable | XvGettable,
        0, 1,
        "XV_AUTOPAINT_COLORKEY"
    },
    {
        XvSettable | XvGettable,
        0, (1 << 24) - 1,
        "XV_COLOURKEY"
    },
    {
        XvSettable | XvGettable,
        0, (1 << 24) - 1,
        "XV_COLORKEY"
    },
    {
        XvSettable | XvGettable,
        0, (1 << 24) - 1,
        "XV_COLOURKEY_MASK"
    },
    {
        XvSettable | XvGettable,
        0, (1 << 24) - 1,
        "XV_COLORKEY_MASK"
    },
    {
        XvSettable,
        0, 0,
        "XV_SET_DEFAULTS"
    },
    {   /* Keep last */
        XvSettable | XvGettable,
        0, 1,
        "XV_DOUBLE_BUFFER"
    }
};
#define nATIMach64Attribute NumberOf(ATIMach64Attribute)

static XF86ImageRec ATIMach64Image[] =
{
    XVIMAGE_YUY2,
    XVIMAGE_UYVY,
    XVIMAGE_YV12,
    XVIMAGE_I420
};
#define nATIMach64Image NumberOf(ATIMach64Image)

/* A local XVideo adaptor attribute record */
typedef struct _ATIMach64Attribute
{
    Atom  AttributeID;
    INT32 MaxValue;             /* ... for the hardware */
    void  (*SetAttribute) NestedPrototype((ATIPtr, INT32));
    INT32 (*GetAttribute) NestedPrototype((ATIPtr));
} ATIMach64AttributeRec, *ATIMach64AttributePtr;

/* Functions to get/set XVideo adaptor attributes */

static void
ATIMach64SetSaturationAttribute
(
    ATIPtr pATI,
    INT32  Value
)
{
    /* Set the register */
    pATI->NewHW.scaler_colour_cntl &=
        ~(SCALE_SATURATION_U | SCALE_SATURATION_V);
    pATI->NewHW.scaler_colour_cntl |= SetBits(Value, SCALE_SATURATION_U) |
        SetBits(Value, SCALE_SATURATION_V);
    outf(SCALER_COLOUR_CNTL, pATI->NewHW.scaler_colour_cntl);
}

static INT32
ATIMach64GetSaturationAttribute
(
    ATIPtr pATI
)
{
    return (INT32)GetBits(pATI->NewHW.scaler_colour_cntl, SCALE_SATURATION_U);
}

static void
ATIMach64SetBrightnessAttribute
(
    ATIPtr pATI,
    INT32  Value
)
{
    /* Set the register */
    pATI->NewHW.scaler_colour_cntl &= ~SCALE_BRIGHTNESS;
    pATI->NewHW.scaler_colour_cntl |= SetBits(Value, SCALE_BRIGHTNESS);
    outf(SCALER_COLOUR_CNTL, pATI->NewHW.scaler_colour_cntl);
}

static INT32
ATIMach64GetBrightnessAttribute
(
    ATIPtr pATI
)
{
    return (INT32)GetBits(pATI->NewHW.scaler_colour_cntl, SCALE_BRIGHTNESS);
}

static void
ATIMach64SetDoubleBufferAttribute
(
    ATIPtr pATI,
    INT32  Value
)
{
    pATI->DoubleBuffer = Value;
}

static INT32
ATIMach64GetDoubleBufferAttribute
(
    ATIPtr pATI
)
{
    return (int)pATI->DoubleBuffer;
}

static void
ATIMach64SetAutoPaintAttribute
(
    ATIPtr pATI,
    INT32  Value
)
{
    pATI->AutoPaint = Value;
}

static INT32
ATIMach64GetAutoPaintAttribute
(
    ATIPtr pATI
)
{
    return (int)pATI->AutoPaint;
}

static void
ATIMach64SetColourKeyAttribute
(
    ATIPtr pATI,
    INT32  Value
)
{
    pATI->NewHW.overlay_graphics_key_clr =
        (CARD32)(Value & ((1 << pATI->depth) - 1));
    outf(OVERLAY_GRAPHICS_KEY_CLR, pATI->NewHW.overlay_graphics_key_clr);
}

static INT32
ATIMach64GetColourKeyAttribute
(
    ATIPtr pATI
)
{
    return (INT32)pATI->NewHW.overlay_graphics_key_clr;
}

static void
ATIMach64SetColourKeyMaskAttribute
(
    ATIPtr pATI,
    INT32  Value
)
{
    pATI->NewHW.overlay_graphics_key_msk =
        (CARD32)(Value & ((1 << pATI->depth) - 1));
    outf(OVERLAY_GRAPHICS_KEY_MSK, pATI->NewHW.overlay_graphics_key_msk);
}

static INT32
ATIMach64GetColourKeyMaskAttribute
(
    ATIPtr pATI
)
{
    return (INT32)pATI->NewHW.overlay_graphics_key_msk;
}

/*
 * ATIMach64SetDefaultAttributes --
 *
 * This function calls other functions to set default values for the various
 * attributes of an XVideo port.
 */
static void
ATIMach64SetDefaultAttributes
(
    ATIPtr pATI,
    INT32  Value
)
{
    ATIMach64SetAutoPaintAttribute(pATI, TRUE);
    ATIMach64SetDoubleBufferAttribute(pATI, FALSE);
    ATIMach64SetColourKeyMaskAttribute(pATI, (1 << pATI->depth) - 1);
    ATIMach64SetColourKeyAttribute(pATI, (3 << ((2 * pATI->depth) / 3)) |
                                         (2 << ((1 * pATI->depth) / 3)) |
                                         (1 << ((0 * pATI->depth) / 3)));

    if (pATI->Chip < ATI_CHIP_264GTPRO)
        return;

    ATIMach64SetBrightnessAttribute(pATI, 32);
    ATIMach64SetSaturationAttribute(pATI, 16);
}

/*
 * There is a one-to-one correspondance between elements of the following array
 * and those of ATIMach64Attribute.
 */
static ATIMach64AttributeRec ATIMach64AttributeInfo[nATIMach64Attribute] =
{
    {   /* SATURATION */
        0, 23,
        ATIMach64SetSaturationAttribute,
        ATIMach64GetSaturationAttribute
    },
    {   /* BRIGHTNESS */
        0, 63,
        ATIMach64SetBrightnessAttribute,
        ATIMach64GetBrightnessAttribute
    },
    {   /* COLOUR */
        0, 23,
        ATIMach64SetSaturationAttribute,
        ATIMach64GetSaturationAttribute
    },
    {   /* COLOR */
        0, 23,
        ATIMach64SetSaturationAttribute,
        ATIMach64GetSaturationAttribute
    },
    {   /* AUTOPAINT_COLOURKEY */
        0, 1,
        ATIMach64SetAutoPaintAttribute,
        ATIMach64GetAutoPaintAttribute
    },
    {   /* AUTOPAINT_COLORKEY */
        0, 1,
        ATIMach64SetAutoPaintAttribute,
        ATIMach64GetAutoPaintAttribute
    },
    {   /* COLOURKEY */
        0, (1 << 24) - 1,
        ATIMach64SetColourKeyAttribute,
        ATIMach64GetColourKeyAttribute
    },
    {   /* COLORKEY */
        0, (1 << 24) - 1,
        ATIMach64SetColourKeyAttribute,
        ATIMach64GetColourKeyAttribute
    },
    {   /* COLOURKEY_MASK */
        0, (1 << 24) - 1,
        ATIMach64SetColourKeyMaskAttribute,
        ATIMach64GetColourKeyMaskAttribute
    },
    {   /* COLORKEY_MASK */
        0, (1 << 24) - 1,
        ATIMach64SetColourKeyMaskAttribute,
        ATIMach64GetColourKeyMaskAttribute
    },
    {   /* SET_DEFAULTS */
        0, 0,
        ATIMach64SetDefaultAttributes,
        NULL
    },
    {   /* DOUBLE_BUFFER */
        0, 1,
        ATIMach64SetDoubleBufferAttribute,
        ATIMach64GetDoubleBufferAttribute
    }
};

/*
 * ATIMach64FindAttribute --
 *
 * This function is called to locate an Xv attribute's table entry.
 */
static int
ATIMach64FindPortAttribute
(
    ATIPtr pATI,
    Atom   AttributeID
)
{
    int iAttribute;

    if (pATI->Chip < ATI_CHIP_264GTPRO)
        iAttribute = 4;
    else
        iAttribute = 0;

    for (;  iAttribute < nATIMach64Attribute;  iAttribute++)
        if (AttributeID == ATIMach64AttributeInfo[iAttribute].AttributeID)
            return iAttribute;

    return -1;
}

/*
 * ATIMach64SetPortAttribute --
 *
 * This function sets the value of a particular port's attribute.
 */
static int
ATIMach64SetPortAttribute
(
    ScrnInfoPtr pScreenInfo,
    Atom        AttributeID,
    INT32       Value,
    pointer     pATI
)
{
    INT32 Range;
    int   iAttribute;

    if (((iAttribute = ATIMach64FindPortAttribute(pATI, AttributeID)) < 0) ||
        !ATIMach64AttributeInfo[iAttribute].SetAttribute)
        return BadMatch;

    Range = ATIMach64Attribute[iAttribute].max_value -
        ATIMach64Attribute[iAttribute].min_value;

    if (Range >= 0)
    {
        /* Limit and scale the value */
        Value -= ATIMach64Attribute[iAttribute].min_value;

        if (Value < 0)
            Value = 0;
        else if (Value > Range)
            Value = Range;

        if (Range != ATIMach64AttributeInfo[iAttribute].MaxValue)
        {
            if (ATIMach64AttributeInfo[iAttribute].MaxValue > 0)
                Value *= ATIMach64AttributeInfo[iAttribute].MaxValue;
            if (Range > 0)
                Value /= Range;
        }
    }

    (*ATIMach64AttributeInfo[iAttribute].SetAttribute)(pATI, Value);

    return Success;
}

/*
 * ATIMach64SetPortAttribute --
 *
 * This function retrieves the value of a particular port's attribute.
 */
static int
ATIMach64GetPortAttribute
(
    ScrnInfoPtr pScreenInfo,
    Atom        AttributeID,
    INT32       *Value,
    pointer     pATI
)
{
    INT32 Range;
    int   iAttribute;

    if (!Value ||
        ((iAttribute = ATIMach64FindPortAttribute(pATI, AttributeID)) < 0) ||
        !ATIMach64AttributeInfo[iAttribute].GetAttribute)
        return BadMatch;

    *Value = (*ATIMach64AttributeInfo[iAttribute].GetAttribute)(pATI);

    Range = ATIMach64Attribute[iAttribute].max_value -
        ATIMach64Attribute[iAttribute].min_value;

    if (Range >= 0)
    {
        if (Range != ATIMach64AttributeInfo[iAttribute].MaxValue)
        {
            /* (Un-)scale the value */
            if (Range > 0)
                *Value *= Range;
            if (ATIMach64AttributeInfo[iAttribute].MaxValue > 0)
                *Value /= ATIMach64AttributeInfo[iAttribute].MaxValue;
        }

        *Value += ATIMach64Attribute[iAttribute].min_value;
    }

    return Success;
}

/*
 * ATIMach64RemoveLinearCallback --
 *
 * This is called by the framebuffer manager to release the offscreen XVideo
 * buffer after the video has been temporarily disabled due to its window being
 * iconified or completely occluded.
 */
static void
ATIMach64RemoveLinearCallback
(
    FBLinearPtr pLinear
)
{
    ATIPtr pATI = ATIPTR(xf86Screens[pLinear->pScreen->myNum]);

    pATI->pXVBuffer = NULL;
    outf(OVERLAY_SCALE_CNTL, SCALE_EN);
}

/*
 * ATIMach64StopVideo --
 *
 * This is called to stop displaying a video.  Note that, to prevent jittering
 * this doesn't actually turn off the overlay unless 'Cleanup' is TRUE, i.e.
 * when the video is to be actually stopped rather than temporarily disabled.
 */
static void
ATIMach64StopVideo
(
    ScrnInfoPtr pScreenInfo,
    pointer     Data,
    Bool        Cleanup
)
{
    ScreenPtr pScreen = pScreenInfo->pScreen;
    ATIPtr    pATI    = Data;

    if (pATI->ActiveSurface)
        return;

    REGION_EMPTY(pScreen, &pATI->VideoClip);

    if (!Cleanup)
    {
        /*
         * Free offscreen buffer if/when its allocation is needed by XAA's
         * pixmap cache.
         */
        if (pATI->pXVBuffer)
            pATI->pXVBuffer->RemoveLinearCallback =
                ATIMach64RemoveLinearCallback;
        return;
    }

    pATI->pXVBuffer = ATIResizeOffscreenLinear(pScreen, pATI->pXVBuffer, 0);
    outf(OVERLAY_SCALE_CNTL, SCALE_EN);
}

/*
 * ATIMach64QueryBestSize --
 *
 * Quoting XVideo docs:
 *
 * This function provides the client with a way to query what the destination
 * dimensions would end up being if they were to request that an area
 * VideoWidth by VideoHeight from the video stream be scaled to rectangle of
 * DrawableWidth by DrawableHeight on the screen.  Since it is not expected
 * that all hardware will be able to get the target dimensions exactly, it is
 * important that the driver provide this function.
 */
static void
ATIMach64QueryBestSize
(
    ScrnInfoPtr  pScreenInfo,
    Bool         Motion,
    short        VideoWidth,
    short        VideoHeight,
    short        DrawableWidth,
    short        DrawableHeight,
    unsigned int *Width,
    unsigned int *Height,
    pointer      pATI
)
{
    *Width  = DrawableWidth;
    *Height = DrawableHeight;
}

/*
 * ATIMach64QueryImageAttributes --
 *
 * Quoting XVideo docs:
 *
 * This function is called to let the driver specify how data for a particular
 * image of size Width by Height should be stored.  Sometimes only the size and
 * corrected width and height are needed.  In that case pitches and offsets are
 * NULL.  The size of the memory required for the image is returned by this
 * function.  The width and height of the requested image can be altered by the
 * driver to reflect format limitations (such as component sampling periods
 * that are larger than one).  If pPitch and pOffset are not NULL, these will
 * be arrays with as many elements in them as there are planes in the image
 * format.  The driver should specify the pitch (in bytes) of each scanline in
 * the particular plane as well as the offset to that plane (in bytes) from the
 * beginning of the image.
 */
static int
ATIMach64QueryImageAttributes
(
    ScrnInfoPtr    pScreenInfo,
    int            ImageID,
    unsigned short *Width,
    unsigned short *Height,
    int            *pPitch,
    int            *pOffset
)
{
    int Size, tmp;

    if (!Width || !Height)
        return 0;

    if (*Width > 2048)
        *Width = 2048;
    else
        *Width = (*Width + 1) & ~1;

    if (*Height > 2048)
        *Height = 2048;

    if (pOffset)
        pOffset[0] = 0;

    switch (ImageID)
    {
        case FOURCC_YV12:
        case FOURCC_I420:
            *Height = (*Height + 1) & ~1;
            Size = (*Width + 3) & ~3;
            if (pPitch)
                pPitch[0] = Size;
            Size *= *Height;
            if (pOffset)
                pOffset[1] = Size;
            tmp = ((*Width >> 1) + 3) & ~3;
            if (pPitch)
                pPitch[1] = pPitch[2] = tmp;
            tmp *= (*Height >> 1);
            Size += tmp;
            if (pOffset)
                pOffset[2] = Size;
            Size += tmp;
            break;

        case FOURCC_UYVY:
        case FOURCC_YUY2:
            Size = *Width << 1;
            if (pPitch)
                pPitch[0] = Size;
            Size *= *Height;
            break;

        default:
            Size = 0;
            break;
    }

    return Size;
}

/*
 * ATIMach64ScaleVideo --
 *
 * This function is called to calculate overlay scaling factors.
 */
static void
ATIMach64ScaleVideo
(
    ATIPtr         pATI,
    DisplayModePtr pMode,
    int            SrcW,
    int            SrcH,
    int            DstW,
    int            DstH,
    CARD32         *pHScale,
    CARD32         *pVScale
)
{
    int Shift;

    *pHScale = ATIDivide(SrcW, DstW,
        GetBits(pATI->NewHW.pll_vclk_cntl, PLL_ECP_DIV) + 12, 0);

    Shift = 12;
    if (pMode->Flags & V_INTERLACE)
        Shift++;

    if (pATI->OptionPanelDisplay && (pATI->LCDPanelID >= 0))
    {
        if (pMode->VDisplay < pATI->LCDVertical)
        {
            SrcH *= pMode->VDisplay;
            DstH *= pATI->LCDVertical;
        }
    }
    else
    {
        if (pMode->Flags & V_DBLSCAN)
            Shift--;
        if (pMode->VScan > 1)
            DstH *= pMode->VScan;
    }

    *pVScale = ATIDivide(SrcH, DstH, Shift, 0);
}

/*
 * ATIMach64ClipVideo --
 *
 * Clip the video (both source and destination) and make various other
 * adjustments.
 */
static Bool
ATIMach64ClipVideo
(
    ScrnInfoPtr pScreenInfo,
    ATIPtr      pATI,
    int         ImageID,
    short       SrcX,
    short       SrcY,
    short       SrcW,
    short       SrcH,
    short       DstX,
    short       DstY,
    short       *DstW,
    short       *DstH,
    short       Width,
    short       Height,
    RegionPtr   pClip,
    BoxPtr      pDstBox,
    INT32       *SrcX1,
    INT32       *SrcX2,
    INT32       *SrcY1,
    INT32       *SrcY2,
    int         *SrcLeft,
    int         *SrcTop
)
{
    CARD32 HScale, VScale;

    /* Check hardware limits */
    if ((Height <= 0) || (Height > 2048) || (Width <= 0) || (Width > 720) ||
        ((Width > 384) && (pATI->Chip < ATI_CHIP_264VTB)))
        return FALSE;

    ATIMach64ScaleVideo(pATI, pScreenInfo->currentMode,
        SrcW, SrcH, *DstW, *DstH, &HScale, &VScale);
    if (!HScale || !VScale)
        return FALSE;
    if (HScale > MaxScale)
        *DstW = (*DstW * HScale) / MaxScale;
    if (VScale > MaxScale)
        *DstH = (*DstH * HScale) / MaxScale;

    /* Clip both the source and the destination */
    *SrcX1 = SrcX;
    *SrcX2 = SrcX + SrcW;
    *SrcY1 = SrcY;
    *SrcY2 = SrcY + SrcH;

    pDstBox->x1 = DstX;
    pDstBox->x2 = DstX + *DstW;
    pDstBox->y1 = DstY;
    pDstBox->y2 = DstY + *DstH;

    if (!xf86XVClipVideoHelper(pDstBox, SrcX1, SrcX2, SrcY1, SrcY2,
                               pClip, Width, Height))
        return FALSE;

    /*
     * Reset overlay scaler origin.  This prevents jittering during
     * viewport panning or while the video is being moved or gradually
     * obscured/unobscured.
     */
    pDstBox->x1 = DstX;
    pDstBox->y1 = DstY;

    /* Translate to the current viewport */
    pDstBox->x1 -= pScreenInfo->frameX0;
    pDstBox->x2 -= pScreenInfo->frameX0;
    pDstBox->y1 -= pScreenInfo->frameY0;
    pDstBox->y2 -= pScreenInfo->frameY0;

    *SrcLeft = *SrcTop = 0;

    /*
     * If the overlay scaler origin ends up outside the current viewport, move
     * it to the viewport's top left corner.  This unavoidably causes a slight
     * jittering in the image (even with double-buffering).
     */
    if (pDstBox->x1 < 0)
    {
        *SrcLeft = ((-pDstBox->x1 * SrcW) / *DstW) & ~1;
        pDstBox->x1 = 0;
    }

    if (pDstBox->y1 < 0)
    {
        *SrcTop = (-pDstBox->y1 * SrcH) / *DstH;
        pDstBox->y1 = 0;

        switch (ImageID)
        {
            case FOURCC_YV12:
            case FOURCC_I420:
                *SrcTop = (*SrcTop + 1) & ~1;
                break;

            default:
                break;
        }
    }

    return TRUE;
}

#ifdef ATIMove32

/* A faster intercept */
#undef  xf86XVCopyPacked
#define xf86XVCopyPacked ATIMach64XVCopyPacked

static void
ATIMach64XVCopyPacked
(
    const CARD8 *pSrc,
    CARD8       *pDst,
    int         SrcPitch,
    int         DstPitch,
    int         Height,
    int         Width
)
{
    Width >>= 1;
    while (--Height >= 0)
    {
        ATIMove32(pDst, pSrc, Width);
        pSrc += SrcPitch;
        pDst += DstPitch;
    }
}

#endif

/*
 * ATIMach64DisplayVideo --
 *
 * This function programmes Mach64 registers needed to display a video.
 */
static void
ATIMach64DisplayVideo
(
    ScrnInfoPtr pScreenInfo,
    ATIPtr      pATI,
    BoxPtr      pDstBox,
    int         ImageID,
    int         Offset,
    int         Pitch,
    short       SrcW,
    short       SrcH,
    short       DstW,
    short       DstH,
    short       Width,
    short       Height
)
{
    DisplayModePtr pMode = pScreenInfo->currentMode;
    CARD32         HScale, VScale;

    if (pMode->VScan > 1)
    {
        pDstBox->y1 *= pMode->VScan;
        pDstBox->y2 *= pMode->VScan;
    }
    if (pMode->Flags & V_DBLSCAN)
    {
        pDstBox->y1 <<= 1;
        pDstBox->y2 <<= 1;
    }

    /* Recalculate overlay scale factors */
    ATIMach64ScaleVideo(pATI, pMode, SrcW, SrcH, DstW, DstH, &HScale, &VScale);

    pATI->NewHW.video_format &= ~SCALER_IN;
    if (ImageID == FOURCC_UYVY)
        pATI->NewHW.video_format |= SCALER_IN_YVYU422;
    else
        pATI->NewHW.video_format |= SCALER_IN_VYUY422;

    ATIMach64WaitForFIFO(pATI, 8);
    outq(OVERLAY_Y_X_START, OVERLAY_Y_X_END, OVERLAY_LOCK_START |
        SetWord(pDstBox->x1, 1) | SetWord(pDstBox->y1, 0),
        SetWord(pDstBox->x2 - 1, 1) | SetWord(pDstBox->y2 - 1, 0));
    outf(OVERLAY_SCALE_INC, SetWord(HScale, 1) | SetWord(VScale, 0));
    outf(SCALER_HEIGHT_WIDTH, SetWord(Width, 1) | SetWord(Height, 0));
    outf(VIDEO_FORMAT, pATI->NewHW.video_format);

    if (pATI->Chip < ATI_CHIP_264VTB)
    {
        outf(BUF0_OFFSET, Offset);
        outf(BUF0_PITCH, Pitch);
    }
    else
    {
        outf(SCALER_BUF0_OFFSET, Offset);
        outf(SCALER_BUF_PITCH, Pitch);
    }

    outf(OVERLAY_SCALE_CNTL, SCALE_PIX_EXPAND | OVERLAY_EN | SCALE_EN);
}

/*
 * ATIMach64PutImage --
 *
 * This function is called to put a video image on the screen.
 */
static int
ATIMach64PutImage
(
    ScrnInfoPtr   pScreenInfo,
    short         SrcX,
    short         SrcY,
    short         DstX,
    short         DstY,
    short         SrcW,
    short         SrcH,
    short         DstW,
    short         DstH,
    int           ImageID,
    unsigned char *Buffer,
    short         Width,
    short         Height,
    Bool          Synchronise,
    RegionPtr     pClip,
    pointer       Data
)
{
    ATIPtr    pATI    = Data;
    ScreenPtr pScreen;
    INT32     SrcX1, SrcX2, SrcY1, SrcY2;
    BoxRec    DstBox;
    int       SrcPitch, SrcPitchUV, DstPitch, DstSize;
    int       SrcTop, SrcLeft, DstWidth, DstHeight;
    int       Top, Bottom, Left, Right, nLine, nPixel, Offset;
    int       OffsetV, OffsetU;
    int       tmp;
    CARD8     *pDst;

    if (pATI->ActiveSurface)
        return Success;

    if (!ATIMach64ClipVideo(pScreenInfo, pATI, ImageID,
                            SrcX, SrcY, SrcW, SrcH,
                            DstX, DstY, &DstW, &DstH,
                            Width, Height, pClip, &DstBox,
                            &SrcX1, &SrcX2, &SrcY1, &SrcY2,
                            &SrcLeft, &SrcTop))
        return Success;

    pScreen = pScreenInfo->pScreen;

    DstWidth = Width - SrcLeft;
    DstHeight = Height - SrcTop;

    /*
     * Allocate an offscreen buffer for the entire source, even though only a
     * subset of the source will be copied into it.
     */
    DstPitch = /* bytes */
        (DstWidth + DstWidth + 15) & ~15;
    DstSize =  /* pixels */
        ((DstPitch * DstHeight) + pATI->AdjustDepth - 1) / pATI->AdjustDepth;

    pATI->pXVBuffer = ATIResizeOffscreenLinear(pScreen, pATI->pXVBuffer,
        (pATI->DoubleBuffer + 1) * DstSize);

    if (!pATI->pXVBuffer)
    {
        if (!pATI->DoubleBuffer)
            return BadAlloc;

        pATI->pXVBuffer =
            ATIResizeOffscreenLinear(pScreen, pATI->pXVBuffer, DstSize);

        if (!pATI->pXVBuffer)
            return BadAlloc;

        xf86DrvMsg(pScreenInfo->scrnIndex, X_WARNING,
            "Video image double-buffering downgraded to single-buffering\n due"
            " to insufficient video memory.\n");
        pATI->DoubleBuffer = pATI->CurrentBuffer = 0;
    }
    else
    {
        /* Possibly switch buffers */
        pATI->CurrentBuffer = pATI->DoubleBuffer - pATI->CurrentBuffer;
    }

    /* Synchronise video memory accesses */
    ATIMach64Sync(pScreenInfo);

    Offset = (pATI->pXVBuffer->offset * pATI->AdjustDepth) +
             (pATI->CurrentBuffer * DstSize * pATI->AdjustDepth);
    pDst = pATI->pMemoryLE;
    pDst += Offset;

    switch (ImageID)
    {
        case FOURCC_YV12:
        case FOURCC_I420:
            Left = (SrcX1 >> 16) & ~1;
            Right = ((SrcX2 + 0x1FFFF) >> 16) & ~1;
            Top = (SrcY1 >> 16) & ~1;
            Bottom = ((SrcY2 + 0x1FFFF) >> 16) & ~1;

            if ((Right < Width) && ((SrcX1 & 0x1FFFF) <= (SrcX2 & 0x1FFFF)))
                Right += 2;
            if ((Bottom < Height) && ((SrcY1 & 0x1FFFF) <= (SrcY2 & 0x1FFFF)))
                Bottom += 2;

            nPixel = Right - Left;
            nLine = Bottom - Top;

            SrcPitch = (Width + 3) & ~3;
            OffsetV = SrcPitch * Height;
            SrcPitchUV = ((Width >> 1) + 3) & ~3;
            OffsetU = ((Height >> 1) * SrcPitchUV) + OffsetV;

            tmp = ((Top >> 1) * SrcPitchUV) + (Left >> 1);
            OffsetV += tmp;
            OffsetU += tmp;

            if (ImageID == FOURCC_I420)
            {
                tmp = OffsetV;
                OffsetV = OffsetU;
                OffsetU = tmp;
            }

            pDst += ((Top - SrcTop) * DstPitch) + ((Left - SrcLeft) << 1);

            xf86XVCopyYUV12ToPacked(Buffer + (Top * SrcPitch) + Left,
                Buffer + OffsetV, Buffer + OffsetU, pDst, SrcPitch, SrcPitchUV,
                DstPitch, nLine, nPixel);
            break;

        case FOURCC_UYVY:
        case FOURCC_YUY2:
        default:
            Left = (SrcX1 >> 16) & ~1;
            Right = ((SrcX2 + 0x1FFFF) >> 16) & ~1;
            Top = SrcY1 >> 16;
            Bottom = (SrcY2 + 0x0FFFF) >> 16;

            if ((Right < Width) && ((SrcX1 & 0x1FFFF) <= (SrcX2 & 0x1FFFF)))
                Right += 2;
            if ((Bottom < Height) && ((SrcY1 & 0x0FFFF) <= (SrcY2 & 0x0FFFF)))
                Bottom++;

            nPixel = Right - Left;
            nLine = Bottom - Top;

            SrcPitch = Width << 1;
            Buffer += (Top * SrcPitch) + (Left << 1);
            pDst += ((Top - SrcTop) * DstPitch) + ((Left - SrcLeft) << 1);

            xf86XVCopyPacked(Buffer, pDst, SrcPitch, DstPitch, nLine, nPixel);
            break;
    }

    if (!REGION_EQUAL(pScreen, &pATI->VideoClip, pClip))
    {
        REGION_COPY(pScreen, &pATI->VideoClip, pClip);
        if (pATI->AutoPaint)
            xf86XVFillKeyHelper(pScreen, pATI->NewHW.overlay_graphics_key_clr,
                pClip);
    }

    ATIMach64DisplayVideo(pScreenInfo, pATI, &DstBox, ImageID,
        Offset, DstPitch / 2, SrcW, SrcH, DstW, DstH, DstWidth, DstHeight);

    return Success;
}

/*
 * ATIMach64AllocateSurface --
 *
 * This function allocates an offscreen buffer (called a "surface") for use by
 * an external driver such as 'v4l'.
 */
static int
ATIMach64AllocateSurface
(
    ScrnInfoPtr    pScreenInfo,
    int            ImageID,
    unsigned short Width,
    unsigned short Height,
    XF86SurfacePtr pSurface
)
{
    ScreenPtr pScreen;
    ATIPtr    pATI = ATIPTR(pScreenInfo);

    if (pATI->ActiveSurface)
        return BadAlloc;

    if ((Height <= 0) || (Height > 2048) || (Width <= 0) || (Width > 720) ||
        ((Width > 384) && (pATI->Chip < ATI_CHIP_264VTB)))
        return BadValue;

    Width = (Width + 1) & ~1;
    pATI->SurfacePitch = ((Width << 1) + 15) & ~15;

    pScreen = pScreenInfo->pScreen;

    pATI->pXVBuffer = ATIResizeOffscreenLinear(pScreen, pATI->pXVBuffer,
        ((Height * pATI->SurfacePitch) + pATI->AdjustDepth - 1) /
        pATI->AdjustDepth);
    if (!pATI->pXVBuffer)
        return BadAlloc;

    pATI->SurfaceOffset = pATI->pXVBuffer->offset * pATI->AdjustDepth;

    pSurface->pScrn = pScreenInfo;
    pSurface->id = ImageID;
    pSurface->width = Width;
    pSurface->height = Height;
    pSurface->pitches = &pATI->SurfacePitch;
    pSurface->offsets = &pATI->SurfaceOffset;
    pSurface->devPrivate.ptr = pATI;

    /* Stop the video */
    outf(OVERLAY_SCALE_CNTL, SCALE_EN);
    REGION_EMPTY(pScreen, &pATI->VideoClip);
    pATI->ActiveSurface = TRUE;

    return Success;
}

/*
 * ATIMach64FreeSurface --
 *
 * This function called to free a surface's offscreen buffer.
 */
static int
ATIMach64FreeSurface
(
    XF86SurfacePtr pSurface
)
{
    ATIPtr pATI = pSurface->devPrivate.ptr;

    if (!pATI->ActiveSurface)
        return Success;

    outf(OVERLAY_SCALE_CNTL, SCALE_EN);
    pATI->pXVBuffer = ATIResizeOffscreenLinear(pSurface->pScrn->pScreen,
        pATI->pXVBuffer, 0);
    pATI->ActiveSurface = FALSE;

    return Success;
}

/*
 * ATIMach64DisplaySurface --
 *
 * This function is called to display a video surface.
 */
static int
ATIMach64DisplaySurface
(
    XF86SurfacePtr pSurface,
    short          SrcX,
    short          SrcY,
    short          DstX,
    short          DstY,
    short          SrcW,
    short          SrcH,
    short          DstW,
    short          DstH,
    RegionPtr      pClip
)
{
    ATIPtr      pATI = pSurface->devPrivate.ptr;
    ScrnInfoPtr pScreenInfo;
    int         ImageID;
    short       Width, Height;
    BoxRec      DstBox;
    INT32       SrcX1, SrcX2, SrcY1, SrcY2;
    int         SrcLeft, SrcTop, SrcPitch, Offset;

    if (!pATI->ActiveSurface)
        return Success;

    pScreenInfo = pSurface->pScrn;
    ImageID = pSurface->id;
    Width = pSurface->width;
    Height = pSurface->height;

    if (!ATIMach64ClipVideo(pScreenInfo, pATI, ImageID,
                            SrcX, SrcY, SrcW, SrcH,
                            DstX, DstY, &DstW, &DstH,
                            Width, Height, pClip, &DstBox,
                            &SrcX1, &SrcX2, &SrcY1, &SrcY2,
                            &SrcLeft, &SrcTop))
        return Success;

    xf86XVFillKeyHelper(pScreenInfo->pScreen,
        pATI->NewHW.overlay_graphics_key_clr, pClip);

    SrcPitch = pSurface->pitches[0];
    Offset = pSurface->offsets[0] + (SrcTop * SrcPitch) + (SrcLeft << 1);
    ATIMach64DisplayVideo(pScreenInfo, pATI, &DstBox, ImageID,
        Offset, SrcPitch, SrcW, SrcH, DstW, DstH, Width, Height);

    return Success;
}

/*
 * ATIMach64StopSurface --
 *
 * This function is called to stop the overlaid display of a video surface.
 */
static int
ATIMach64StopSurface
(
    XF86SurfacePtr pSurface
)
{
    ATIPtr pATI = pSurface->devPrivate.ptr;

    if (pATI->ActiveSurface)
        outf(OVERLAY_SCALE_CNTL, SCALE_EN);

    return Success;
}

/*
 * ATIMach64GetSurfaceAttribute --
 *
 * Retrieve the value of an XVideo attribute.
 */
static int
ATIMach64GetSurfaceAttribute
(
    ScrnInfoPtr pScreenInfo,
    Atom        AttributeID,
    INT32       *Value
)
{
    return ATIMach64GetPortAttribute(pScreenInfo, AttributeID, Value,
                                     ATIPTR(pScreenInfo));
}

/*
 * ATIMach64SetSurfaceAttribute
 *
 * Set the value of an XVideo attribute.
 */
static int
ATIMach64SetSurfaceAttribute
(
    ScrnInfoPtr pScreenInfo,
    Atom        AttributeID,
    INT32       Value
)
{
    return ATIMach64SetPortAttribute(pScreenInfo, AttributeID, Value,
                                     ATIPTR(pScreenInfo));
}

/* XVideo surface registration data */
static XF86OffscreenImageRec ATIMach64Surface_A[] =
{
    {
        &ATIMach64Image[0],             /* YUY2 */
        VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT,
        ATIMach64AllocateSurface,
        ATIMach64FreeSurface,
        ATIMach64DisplaySurface,
        ATIMach64StopSurface,
        ATIMach64GetSurfaceAttribute,
        ATIMach64SetSurfaceAttribute,
        384, 2048,
        nATIMach64Attribute - 5,        /* No double-buffering */
        ATIMach64Attribute + 4          /* No saturation nor brightness */
    },
    {
        &ATIMach64Image[1],             /* UYVY */
        VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT,
        ATIMach64AllocateSurface,
        ATIMach64FreeSurface,
        ATIMach64DisplaySurface,
        ATIMach64StopSurface,
        ATIMach64GetSurfaceAttribute,
        ATIMach64SetSurfaceAttribute,
        384, 2048,
        nATIMach64Attribute - 5,        /* No double-buffering */
        ATIMach64Attribute + 4          /* No saturation nor brightness */
    }
};
#define nATIMach64Surface_A NumberOf(ATIMach64Surface_A)

static XF86OffscreenImageRec ATIMach64Surface_B[] =
{
    {
        &ATIMach64Image[0],             /* YUY2 */
        VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT,
        ATIMach64AllocateSurface,
        ATIMach64FreeSurface,
        ATIMach64DisplaySurface,
        ATIMach64StopSurface,
        ATIMach64GetSurfaceAttribute,
        ATIMach64SetSurfaceAttribute,
        720, 2048,
        nATIMach64Attribute - 5,        /* No double-buffering */
        ATIMach64Attribute + 4          /* No saturation nor brightness */
    },
    {
        &ATIMach64Image[1],             /* UYVY */
        VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT,
        ATIMach64AllocateSurface,
        ATIMach64FreeSurface,
        ATIMach64DisplaySurface,
        ATIMach64StopSurface,
        ATIMach64GetSurfaceAttribute,
        ATIMach64SetSurfaceAttribute,
        720, 2048,
        nATIMach64Attribute - 5,        /* No double-buffering */
        ATIMach64Attribute + 4          /* No saturation nor brightness */
    }
};
#define nATIMach64Surface_B NumberOf(ATIMach64Surface_B)

static XF86OffscreenImageRec ATIMach64Surface_C[] =
{
    {
        &ATIMach64Image[0],             /* YUY2 */
        VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT,
        ATIMach64AllocateSurface,
        ATIMach64FreeSurface,
        ATIMach64DisplaySurface,
        ATIMach64StopSurface,
        ATIMach64GetSurfaceAttribute,
        ATIMach64SetSurfaceAttribute,
        720, 2048,
        nATIMach64Attribute - 1,        /* No double-buffering */
        ATIMach64Attribute
    },
    {
        &ATIMach64Image[1],             /* UYVY */
        VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT,
        ATIMach64AllocateSurface,
        ATIMach64FreeSurface,
        ATIMach64DisplaySurface,
        ATIMach64StopSurface,
        ATIMach64GetSurfaceAttribute,
        ATIMach64SetSurfaceAttribute,
        720, 2048,
        nATIMach64Attribute - 1,        /* No double-buffering */
        ATIMach64Attribute
    }
};
#define nATIMach64Surface_C NumberOf(ATIMach64Surface_C)

/*
 * ATIMach64XVInitialiseAdaptor --
 *
 * This function is called to make a Mach64's hardware overlay support
 * available as an XVideo adaptor.
 */
int
ATIMach64XVInitialiseAdaptor
(
    ScreenPtr           pScreen,
    ScrnInfoPtr         pScreenInfo,
    ATIPtr              pATI,
    XF86VideoAdaptorPtr **pppAdaptor
)
{
    XF86VideoAdaptorPtr pAdaptor;
    int                 Index;

    if (!pATI->Block1Base)
        return 0;

    if (!(pAdaptor = xf86XVAllocateVideoAdaptorRec(pScreenInfo)))
        return 0;

    *pppAdaptor = xnfalloc(sizeof(pAdaptor));
    **pppAdaptor = pAdaptor;

    pAdaptor->nPorts = 1;
    pAdaptor->pPortPrivates = pATI->XVPortPrivate;
    pATI->XVPortPrivate[0].ptr = pATI;

    pAdaptor->type = XvInputMask | XvImageMask | XvWindowMask;
    pAdaptor->flags = VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT;
    pAdaptor->name = "ATI Mach64 Back-end Overlay Scaler";

    if (pATI->Chip < ATI_CHIP_264VTB)
    {
        pAdaptor->nEncodings = nATIMach64VideoEncoding_A;
        pAdaptor->pEncodings = ATIMach64VideoEncoding_A;
    }
    else
    {
        pAdaptor->nEncodings = nATIMach64VideoEncoding_B;
        pAdaptor->pEncodings = ATIMach64VideoEncoding_B;
    }

    pAdaptor->nFormats = nATIMach64VideoFormat;
    pAdaptor->pFormats = ATIMach64VideoFormat;

    pAdaptor->nAttributes = nATIMach64Attribute;
    pAdaptor->pAttributes = ATIMach64Attribute;

    if (pATI->Chip < ATI_CHIP_264GTPRO)
    {
        /* Older controllers don't have brightness or saturation controls */
        pAdaptor->nAttributes -= 4;
        pAdaptor->pAttributes += 4;
    }

    pAdaptor->nImages = nATIMach64Image;
    pAdaptor->pImages = ATIMach64Image;

    pAdaptor->StopVideo            = ATIMach64StopVideo;
    pAdaptor->SetPortAttribute     = ATIMach64SetPortAttribute;
    pAdaptor->GetPortAttribute     = ATIMach64GetPortAttribute;
    pAdaptor->QueryBestSize        = ATIMach64QueryBestSize;
    pAdaptor->PutImage             = ATIMach64PutImage;
    pAdaptor->QueryImageAttributes = ATIMach64QueryImageAttributes;

    REGION_NULL(pScreen, &pATI->VideoClip);
    pATI->ActiveSurface = FALSE;

    if (ATIMach64XVAtomGeneration != serverGeneration)
    {
        /* Refresh static data */
        ATIMach64XVAtomGeneration = serverGeneration;

        Index = nATIMach64Attribute - pAdaptor->nAttributes;
        for (;  Index < nATIMach64Attribute;  Index++)
            ATIMach64AttributeInfo[Index].AttributeID =
                MAKE_ATOM(ATIMach64Attribute[Index].name);
    }

    ATIMach64SetDefaultAttributes(pATI, 0);

    if (pATI->Chip < ATI_CHIP_264VTB)
    {
        xf86XVRegisterOffscreenImages(pScreen,
            ATIMach64Surface_A, nATIMach64Surface_A);
    }
    else if (pATI->Chip < ATI_CHIP_264GTPRO)
    {
        xf86XVRegisterOffscreenImages(pScreen,
            ATIMach64Surface_B, nATIMach64Surface_B);
    }
    else
    {
        xf86XVRegisterOffscreenImages(pScreen,
            ATIMach64Surface_C, nATIMach64Surface_C);
    }

    return 1;
}

/*
 * ATIMach64CloseXVideo --
 *
 * This function is called during screen termination to clean up after
 * initialisation of Mach64 XVideo support.
 */
void
ATIMach64CloseXVideo
(
    ScreenPtr   pScreen,
    ScrnInfoPtr pScreenInfo,
    ATIPtr      pATI
)
{
    ATIMach64StopVideo(pScreenInfo, pATI, TRUE);

    REGION_UNINIT(pScreen, &pATI->VideoClip);
}
