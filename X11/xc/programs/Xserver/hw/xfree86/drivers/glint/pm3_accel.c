/*
 * Copyright 2000-2001 by Sven Luther <luther@dpt-info.u-strasbg.fr>.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Sven Luther not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission. Sven Luther makes no representations
 * about the suitability of this software for any purpose. It is provided
 * "as is" without express or implied warranty.
 *
 * SVEN LUTHER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL SVEN LUTHER BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors:  Sven Luther, <luther@dpt-info.u-strasbg.fr>
 *           Alan Hourihane, <alanh@fairlite.demon.co.uk>
 *
 * this work is sponsored by Appian Graphics.
 * 
 * Permedia 3 accelerated options.
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/glint/pm3_accel.c,v 1.31 2003/01/06 00:04:54 alanh Exp $ */

#include "Xarch.h"
#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"

#include "xf86PciInfo.h"
#include "xf86Pci.h"

#include "miline.h"

#include "fb.h"

#include "glint_regs.h"
#include "pm3_regs.h"
#include "glint.h"

#include "xaalocal.h"		/* For replacements */

#define DEBUG 0

#if DEBUG
# define TRACE_ENTER(str)       ErrorF("pm3_accel: " str " %d\n",pScrn->scrnIndex)
# define TRACE_EXIT(str)        ErrorF("pm3_accel: " str " done\n")
# define TRACE(str)             ErrorF("pm3_accel trace: " str "\n")
#else
# define TRACE_ENTER(str)
# define TRACE_EXIT(str)
# define TRACE(str)
#endif

#define PM3_WRITEMASK \
  (pGlint->PM3_UsingSGRAM ? PM3FBHardwareWriteMask : PM3FBSoftwareWriteMask )
#define PM3_OTHERWRITEMASK \
  (pGlint->PM3_UsingSGRAM ? PM3FBSoftwareWriteMask : PM3FBHardwareWriteMask )

#ifndef XF86DRI
#define PM3_PLANEMASK(planemask)				\
{ 								\
	if (planemask != pGlint->planemask) {			\
		pGlint->planemask = planemask;			\
		REPLICATE(planemask); 				\
		GLINT_WRITE_REG(planemask, PM3_WRITEMASK);	\
	}							\
} 
#else
#define PM3_PLANEMASK(planemask)				\
	{							\
		pGlint->planemask = planemask;			\
		REPLICATE(planemask); 				\
		GLINT_WRITE_REG(planemask, PM3_WRITEMASK);	\
	}
#endif

/* Clipping */
static void Permedia3SetClippingRectangle(ScrnInfoPtr pScrn, int x, int y,
				int w, int h);
static void Permedia3DisableClipping(ScrnInfoPtr pScrn);
/* ScreenToScreenCopy */
static void Permedia3SubsequentScreenToScreenCopy(ScrnInfoPtr pScrn,
				int x1, int y1, int x2,
				int y2, int w, int h);
static void Permedia3SetupForScreenToScreenCopy(ScrnInfoPtr pScrn,
				int xdir, int ydir, int rop, 
                                unsigned int planemask,
				int transparency_color);
/* SolidFill */
static void Permedia3SetupForFillRectSolid(ScrnInfoPtr pScrn, int color,
				int rop, unsigned int planemask);
static void Permedia3SubsequentFillRectSolid(ScrnInfoPtr pScrn, int x,
				int y, int w, int h);
static void Permedia3SubsequentFillRectSolid32bpp(ScrnInfoPtr pScrn, int x,
				int y, int w, int h);
/* 8x8 Mono Pattern Fills */
static void Permedia3SetupForMono8x8PatternFill(ScrnInfoPtr pScrn, 
				int patternx, int patterny, int fg, int bg,
				int rop, unsigned int planemask);
static void Permedia3SubsequentMono8x8PatternFillRect(ScrnInfoPtr pScrn, 
				int x_offset, int y_offset, int x, int y, 
				int w, int h);
static void Permedia3SetupForScanlineCPUToScreenColorExpandFill(
				ScrnInfoPtr pScrn,
				int fg, int bg, int rop, 
				unsigned int planemask);
static void Permedia3SubsequentScanlineCPUToScreenColorExpandFill(
				ScrnInfoPtr pScrn, int x,
				int y, int w, int h, int skipleft);
static void Permedia3SubsequentColorExpandScanline(ScrnInfoPtr pScrn, int bufno);
static void Permedia3SetupForScanlineImageWrite(ScrnInfoPtr pScrn, int rop,
				unsigned int planemask, int trans_color,
				int bpp, int depth);
static void Permedia3SubsequentScanlineImageWriteRect(ScrnInfoPtr pScrn, 
				int x, int y, int w, int h, int skipleft);
static void Permedia3SubsequentImageWriteScanline(ScrnInfoPtr pScrn, int bufno);
static void Permedia3RestoreAccelState(ScrnInfoPtr pScrn);
static void Permedia3WritePixmap(ScrnInfoPtr pScrn, int x, int y, int w, int h,
				unsigned char *src, int srcwidth, int rop,
				unsigned int planemask, int transparency_color,
				int bpp, int depth);
static void Permedia3WriteBitmap(ScrnInfoPtr pScrn, int x, int y, int w, int h, 
				unsigned char *src, int srcwidth, int skipleft, 
				int fg, int bg, int rop,unsigned int planemask);

void
Permedia3InitializeEngine(ScrnInfoPtr pScrn)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    int colorformat = 0;

    /* Initialize the Accelerator Engine to defaults */
    TRACE_ENTER("Permedia3InitializeEngine");

    if ((IS_J2000) && (pGlint->Chipset == PCI_VENDOR_3DLABS_CHIP_GAMMA)) {
	GLINT_SLOW_WRITE_REG(pGlint->MultiIndex, BroadcastMask);
    }
    if (pGlint->MultiAperture) {
	ErrorF("pm3_accel: SVEN : multiAperture set\n");
    	/* Only write the following register to the first PM3 */
    	GLINT_SLOW_WRITE_REG(1, BroadcastMask);
    	GLINT_SLOW_WRITE_REG(0x00000001,    ScanLineOwnership);

    	/* Only write the following register to the second PM3 */
    	GLINT_SLOW_WRITE_REG(2, BroadcastMask);
    	GLINT_SLOW_WRITE_REG(0x00000005,    ScanLineOwnership);

    	/* Make sure the rest of the register writes go to both PM3's */
    	GLINT_SLOW_WRITE_REG(3, BroadcastMask);
    }

    /* Disable LocalBuffer. Fixes stripes problems when 
     * doing screen-to-screen copies */
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE, PM3LBDestReadMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE, PM3LBDestReadEnables);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE, PM3LBSourceReadMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE, PM3LBWriteMode);

    /* Host out PreInit */
    /* Set filter mode to enable sync tag & data output */
    GLINT_SLOW_WRITE_REG(0x400,		FilterMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE, StatisticMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE, PM3DeltaMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE, RasterizerMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE, ScissorMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE, LineStippleMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE, AreaStippleMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE, PM3GIDMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE, DepthMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE, StencilMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE, ColorDDAMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE, PM3TextureCoordMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE, PM3TextureIndexMode0);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE, PM3TextureIndexMode1);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE, TextureReadMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE, PM3LUTMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE, TextureFilterMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE, PM3TextureCompositeMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE, PM3TextureApplicationMode);
    GLINT_SLOW_WRITE_REG(0, PM3TextureCompositeColorMode1);
    GLINT_SLOW_WRITE_REG(0, PM3TextureCompositeAlphaMode1);
    GLINT_SLOW_WRITE_REG(0, PM3TextureCompositeColorMode0);
    GLINT_SLOW_WRITE_REG(0, PM3TextureCompositeAlphaMode0);

    GLINT_SLOW_WRITE_REG(UNIT_DISABLE, FogMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE, ChromaTestMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE, AlphaTestMode);
    /* Not done in P3Lib ??? */
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE, AntialiasMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE, PM3AlphaTestMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE, YUVMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE, PM3AlphaBlendColorMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE, PM3AlphaBlendAlphaMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE, DitherMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE, LogicalOpMode);
    
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE, StatisticMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE, RouterMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE, PM3Window);

    GLINT_SLOW_WRITE_REG(0, PM3Config2D);
    
    GLINT_SLOW_WRITE_REG(0xffffffff, PM3SpanColorMask);

    GLINT_SLOW_WRITE_REG(0, PM3XBias);
    GLINT_SLOW_WRITE_REG(0, PM3YBias);

    GLINT_SLOW_WRITE_REG(0, PM3DeltaControl);

    GLINT_SLOW_WRITE_REG(0xffffffff, BitMaskPattern);

    /* ScissorStippleUnit Initialization (is it needed ?) */
    pGlint->ClippingOn = FALSE;
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE, ScissorMode);
    /* We never use Screen Scissor ...
    GLINT_SLOW_WRITE_REG(
	(pScrn->virtualX&0xffff)|((pScrn->virtualY&0xffff)<<16),
	ScreenSize);
    GLINT_SLOW_WRITE_REG(
	(0&0xffff)|((0&0xffff)<<16),
	WindowOrigin);
    */

    /* StencilDepthUnit Initialization */
    GLINT_SLOW_WRITE_REG(0, PM3Window);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE, DepthMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE, StencilMode);
    GLINT_SLOW_WRITE_REG(0, StencilData);

    /* FBReadUnit Initialization */
    TRACE("Permedia3InitializeEngine : only syncs upto now");
    GLINT_SLOW_WRITE_REG(
	PM3FBDestReadEnables_E(0xff) |
	PM3FBDestReadEnables_R(0xff) |
	PM3FBDestReadEnables_ReferenceAlpha(0xff),
	PM3FBDestReadEnables);
    GLINT_SLOW_WRITE_REG(0, PM3FBDestReadBufferAddr0);
    GLINT_SLOW_WRITE_REG(0, PM3FBDestReadBufferOffset0);
    GLINT_SLOW_WRITE_REG(
	PM3FBDestReadBufferWidth_Width(pScrn->displayWidth),
	PM3FBDestReadBufferWidth0);
    /*
    GLINT_SLOW_WRITE_REG(0, PM3FBDestReadBufferAddr1);
    GLINT_SLOW_WRITE_REG(0, PM3FBDestReadBufferOffset1);
    GLINT_SLOW_WRITE_REG(
	PM3FBDestReadBufferWidth_Width(pScrn->displayWidth),
	PM3FBDestReadBufferWidth1);
    GLINT_SLOW_WRITE_REG(0, PM3FBDestReadBufferAddr2);
    GLINT_SLOW_WRITE_REG(0, PM3FBDestReadBufferOffset2);
    GLINT_SLOW_WRITE_REG(
	PM3FBDestReadBufferWidth_Width(pScrn->displayWidth),
	PM3FBDestReadBufferWidth2);
    GLINT_SLOW_WRITE_REG(0, PM3FBDestReadBufferAddr3);
    GLINT_SLOW_WRITE_REG(0, PM3FBDestReadBufferOffset3);
    GLINT_SLOW_WRITE_REG(
	PM3FBDestReadBufferWidth_Width(pScrn->displayWidth),
	PM3FBDestReadBufferWidth3);
    */
    GLINT_SLOW_WRITE_REG(
	PM3FBDestReadMode_ReadEnable |
	/* Not needed, since FBDestRead is the same as FBWrite.
	PM3FBDestReadMode_Blocking |
	*/ 
	PM3FBDestReadMode_Enable0,
	PM3FBDestReadMode);
    TRACE("Permedia3InitializeEngine : DestRead");
    GLINT_SLOW_WRITE_REG(0, PM3FBSourceReadBufferAddr);
    GLINT_SLOW_WRITE_REG(0, PM3FBSourceReadBufferOffset);
    GLINT_SLOW_WRITE_REG(
	PM3FBSourceReadBufferWidth_Width(pScrn->displayWidth),
	PM3FBSourceReadBufferWidth);
    GLINT_SLOW_WRITE_REG(
	PM3FBSourceReadMode_Blocking |
	PM3FBSourceReadMode_ReadEnable,
	PM3FBSourceReadMode);
    TRACE("Permedia3InitializeEngine : SourceRead");
    switch (pScrn->bitsPerPixel) {
	case 8:
	    pGlint->PM3_PixelSize = 2;
#if X_BYTE_ORDER == X_BIG_ENDIAN
	    pGlint->RasterizerSwap = 3<<15;	/* Swap host data */
#endif
	    break;
	case 16:
	    pGlint->PM3_PixelSize = 1;
#if X_BYTE_ORDER == X_BIG_ENDIAN
	    pGlint->RasterizerSwap = 2<<15;	/* Swap host data */
#endif
	    break;
	case 32:
	    pGlint->PM3_PixelSize = 0;
	    break;
    }
    GLINT_SLOW_WRITE_REG(pGlint->PM3_PixelSize, PixelSize);
#if X_BYTE_ORDER == X_BIG_ENDIAN
    GLINT_SLOW_WRITE_REG(1 | pGlint->RasterizerSwap, RasterizerMode);
#endif
    TRACE("Permedia3InitializeEngine : PixelSize");

    /* LogicalOpUnit Initialization */
    GLINT_SLOW_WRITE_REG(0xffffffff,	PM3_OTHERWRITEMASK);

    /* FBWriteUnit Initialization */
    GLINT_SLOW_WRITE_REG(
	PM3FBWriteMode_WriteEnable|
	PM3FBWriteMode_OpaqueSpan|
	PM3FBWriteMode_Enable0,
	PM3FBWriteMode);
    GLINT_SLOW_WRITE_REG(0, PM3FBWriteBufferAddr0);
    GLINT_SLOW_WRITE_REG(0, PM3FBWriteBufferOffset0);
    GLINT_SLOW_WRITE_REG(
	PM3FBWriteBufferWidth_Width(pScrn->displayWidth),
	PM3FBWriteBufferWidth0);
    /*
    GLINT_SLOW_WRITE_REG(0, PM3FBWriteBufferAddr1);
    GLINT_SLOW_WRITE_REG(0, PM3FBWriteBufferOffset1);
    GLINT_SLOW_WRITE_REG(
	PM3FBWriteBufferWidth_Width(pScrn->displayWidth),
	PM3FBWriteBufferWidth1);
    GLINT_SLOW_WRITE_REG(0, PM3FBWriteBufferAddr2);
    GLINT_SLOW_WRITE_REG(0, PM3FBWriteBufferOffset2);
    GLINT_SLOW_WRITE_REG(
	PM3FBWriteBufferWidth_Width(pScrn->displayWidth),
	PM3FBWriteBufferWidth2);
    GLINT_SLOW_WRITE_REG(0, PM3FBWriteBufferAddr3);
    GLINT_SLOW_WRITE_REG(0, PM3FBWriteBufferOffset3);
    GLINT_SLOW_WRITE_REG(
	PM3FBWriteBufferWidth_Width(pScrn->displayWidth),
	PM3FBWriteBufferWidth3);
    */
    TRACE("Permedia3InitializeEngine : FBWrite");
    /* SizeOfframebuffer */
    GLINT_SLOW_WRITE_REG(
	pScrn->displayWidth *
	(8 * pGlint->FbMapSize / (pScrn->bitsPerPixel * pScrn->displayWidth)
	    >4095?4095: 8 * pGlint->FbMapSize /
	    (pScrn->bitsPerPixel * pScrn->displayWidth)),
	PM3SizeOfFramebuffer);
    GLINT_SLOW_WRITE_REG(0xffffffff,	PM3_WRITEMASK);
    TRACE("Permedia3InitializeEngine : FBHardwareWriteMask & SizeOfFramebuffer");
    /* Color Format */
    switch (pScrn->depth) {
	case 8:
	    colorformat = 4;
	    break;
	case 15:
	    colorformat = 2;
	    break;
	case 16:
	    colorformat = 3;
	    break;
	case 24:
	case 32:
	    colorformat = 0;
	    break;
    }
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE|
	((colorformat&0xf)<<2)|(1<<10),
	DitherMode);

    /* Other stuff */
    pGlint->startxdom = 0;
    pGlint->startxsub = 0;
    pGlint->starty = 0;
    pGlint->count = 0;
    pGlint->dy = 1<<16;
    pGlint->dxdom = 0;
    pGlint->x = 0;
    pGlint->y = 0;
    pGlint->h = 0;
    pGlint->w = 0;
    pGlint->ROP = 0xFF;
    GLINT_SLOW_WRITE_REG(0, dXDom);
    GLINT_SLOW_WRITE_REG(0, dXSub);
    GLINT_SLOW_WRITE_REG(1<<16, dY);
    GLINT_SLOW_WRITE_REG(0, StartXDom);
    GLINT_SLOW_WRITE_REG(0, StartXSub);
    GLINT_SLOW_WRITE_REG(0, StartY);
    GLINT_SLOW_WRITE_REG(0, GLINTCount);
    if (*pGlint->AccelInfoRec->Sync)
    	(*pGlint->AccelInfoRec->Sync)(pScrn);
    TRACE_EXIT("Permedia3InitializeEngine");
}

Bool
Permedia3AccelInit(ScreenPtr pScreen)
{
    XAAInfoRecPtr infoPtr;
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    GLINTPtr pGlint = GLINTPTR(pScrn);

    pGlint->AccelInfoRec = infoPtr = XAACreateInfoRec();
    if (!infoPtr) return FALSE;

    /* Generic accel engine flags */
    infoPtr->Flags = PIXMAP_CACHE |
		     OFFSCREEN_PIXMAPS |
		     LINEAR_FRAMEBUFFER;

    /* Synchronization of the accel engine */
    if (pGlint->MultiAperture)
    	infoPtr->Sync = DualPermedia3Sync;
    else
    	infoPtr->Sync = Permedia3Sync;

    Permedia3InitializeEngine(pScrn);

    /* Clipping Setup */
    infoPtr->ClippingFlags = 0;
    infoPtr->SetClippingRectangle = Permedia3SetClippingRectangle;
    infoPtr->DisableClipping = Permedia3DisableClipping;

    /* ScreenToScreenCopy */
    infoPtr->ScreenToScreenCopyFlags = NO_TRANSPARENCY;
    infoPtr->SetupForScreenToScreenCopy =
					Permedia3SetupForScreenToScreenCopy;
    infoPtr->SubsequentScreenToScreenCopy =
					Permedia3SubsequentScreenToScreenCopy;

    /* SolidFill */
    infoPtr->SolidFillFlags = 0;
    infoPtr->SetupForSolidFill = Permedia3SetupForFillRectSolid;
    infoPtr->SubsequentSolidFillRect = Permedia3SubsequentFillRectSolid;

    /* 8x8 Mono Pattern Fills */
    infoPtr->Mono8x8PatternFillFlags =  HARDWARE_PATTERN_PROGRAMMED_BITS |
    					HARDWARE_PATTERN_PROGRAMMED_ORIGIN |
    					HARDWARE_PATTERN_SCREEN_ORIGIN |
					BIT_ORDER_IN_BYTE_LSBFIRST;
    infoPtr->SetupForMono8x8PatternFill =
				Permedia3SetupForMono8x8PatternFill;
    infoPtr->SubsequentMono8x8PatternFillRect = 
				Permedia3SubsequentMono8x8PatternFillRect;

    infoPtr->ScanlineCPUToScreenColorExpandFillFlags = 
						LEFT_EDGE_CLIPPING |
						LEFT_EDGE_CLIPPING_NEGATIVE_X |
					       	BIT_ORDER_IN_BYTE_LSBFIRST |
						CPU_TRANSFER_PAD_DWORD;

    infoPtr->NumScanlineColorExpandBuffers = 1;
    pGlint->ScratchBuffer                 = xalloc(((pScrn->virtualX+62)/32*4)
					    + (pScrn->virtualX
					    * pScrn->bitsPerPixel / 8));
    infoPtr->ScanlineColorExpandBuffers = 
					pGlint->XAAScanlineColorExpandBuffers;
    pGlint->XAAScanlineColorExpandBuffers[0] = 
					pGlint->IOBase + OutputFIFO + 4;

    infoPtr->SetupForScanlineCPUToScreenColorExpandFill =
			Permedia3SetupForScanlineCPUToScreenColorExpandFill;
    infoPtr->SubsequentScanlineCPUToScreenColorExpandFill = 
			Permedia3SubsequentScanlineCPUToScreenColorExpandFill;
    infoPtr->SubsequentColorExpandScanline = 
			Permedia3SubsequentColorExpandScanline;

    infoPtr->ScanlineImageWriteFlags = 	NO_GXCOPY |
					LEFT_EDGE_CLIPPING |
					LEFT_EDGE_CLIPPING_NEGATIVE_X |
					BIT_ORDER_IN_BYTE_LSBFIRST |
					CPU_TRANSFER_PAD_DWORD;
    infoPtr->NumScanlineImageWriteBuffers = 1;
    infoPtr->ScanlineImageWriteBuffers = 
					pGlint->XAAScanlineColorExpandBuffers;
    infoPtr->SetupForScanlineImageWrite =
	    Permedia3SetupForScanlineImageWrite;
    infoPtr->SubsequentScanlineImageWriteRect =
	    Permedia3SubsequentScanlineImageWriteRect;
    infoPtr->SubsequentImageWriteScanline = 
			Permedia3SubsequentImageWriteScanline;

    infoPtr->WriteBitmap = Permedia3WriteBitmap;
    infoPtr->WriteBitmapFlags = 0;

    infoPtr->WritePixmap = Permedia3WritePixmap;
    infoPtr->WritePixmapFlags = 0;

    {
	Bool shared_accel = FALSE;
	int i;

	for(i = 0; i < pScrn->numEntities; i++) {
	    if(xf86IsEntityShared(pScrn->entityList[i]))
		shared_accel = TRUE;
	}
	if(shared_accel == TRUE)
	    infoPtr->RestoreAccelState = Permedia3RestoreAccelState;
    }

    Permedia3EnableOffscreen(pScreen);

    return(XAAInit(pScreen, infoPtr));
}

void
Permedia3EnableOffscreen (ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    GLINTPtr pGlint = GLINTPTR(pScrn);
    BoxRec AvailFBArea;

    /* Available Framebuffer Area for XAA. */
    AvailFBArea.x1 = 0;
    AvailFBArea.y1 = 0;
    AvailFBArea.x2 = pScrn->displayWidth;
    /* X coords are short's so we have to do this to make sure we dont wrap*/
    AvailFBArea.y2 = ((pGlint->FbMapSize > 16384*1024) ? 16384*1024 :
	pGlint->FbMapSize)  / (pScrn->displayWidth *
	pScrn->bitsPerPixel / 8);

    /* Permedia3 has a maximum 4096x4096 framebuffer */
    if (AvailFBArea.y2 > 4095) AvailFBArea.y2 = 4095;

    xf86InitFBManager(pScreen, &AvailFBArea);
}
#define CHECKCLIPPING				\
{						\
    if (pGlint->ClippingOn) {			\
	pGlint->ClippingOn = FALSE;		\
	GLINT_WAIT(1);				\
	GLINT_WRITE_REG(0, ScissorMode);	\
    }						\
}

void
Permedia3Sync(ScrnInfoPtr pScrn)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);

    CHECKCLIPPING;

    while (GLINT_READ_REG(DMACount) != 0);
    GLINT_WAIT(2);
    GLINT_WRITE_REG(0x400, FilterMode);
    GLINT_WRITE_REG(0, GlintSync);
    do {
   	while(GLINT_READ_REG(OutFIFOWords) == 0);
    } while (GLINT_READ_REG(OutputFIFO) != Sync_tag);
}

void
DualPermedia3Sync(
	ScrnInfoPtr pScrn
){
    GLINTPtr pGlint = GLINTPTR(pScrn);

    CHECKCLIPPING;

    while (GLINT_READ_REG(DMACount) != 0);
    GLINT_WAIT(3);
    GLINT_WRITE_REG(3, BroadcastMask); /* hack! this shouldn't need to be reloaded */
    GLINT_WRITE_REG(0x400, FilterMode);
    GLINT_WRITE_REG(0, GlintSync);

    /* Read 1st PM3 until Sync Tag shows */
    ACCESSCHIP1();
    do {
   	while(GLINT_READ_REG(OutFIFOWords) == 0);
    } while (GLINT_READ_REG(OutputFIFO) != Sync_tag);

    ACCESSCHIP2();
    /* Read 2nd PM3 until Sync Tag shows */
    do {
   	while(GLINT_READ_REG(OutFIFOWords) == 0);
    } while (GLINT_READ_REG(OutputFIFO) != Sync_tag);

    ACCESSCHIP1();
}

static void
Permedia3SetClippingRectangle(ScrnInfoPtr pScrn, int x1, int y1, int x2, int y2)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    GLINT_WAIT(3);
    GLINT_WRITE_REG(((y1&0x0fff)<<16)|(x1&0x0fff), ScissorMinXY);
    GLINT_WRITE_REG(((y2&0x0fff)<<16)|(x2&0x0fff), ScissorMaxXY);
    GLINT_WRITE_REG(1, ScissorMode);
    pGlint->ClippingOn = TRUE;
}
static void
Permedia3DisableClipping(ScrnInfoPtr pScrn)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    CHECKCLIPPING;
}

/* ScreenToScreenCopy definition */
static void
Permedia3SetupForScreenToScreenCopy(ScrnInfoPtr pScrn, 
				int xdir, int ydir, int rop,
				unsigned int planemask, int transparency_color)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    TRACE_ENTER("Permedia3SetupForScreenToScreenCopy");

    pGlint->PM3_Render2D =
	PM3Render2D_SpanOperation |
	PM3Render2D_Operation_Normal;

    pGlint->ClippingOn = TRUE;

    pGlint->PM3_Config2D =
	PM3Config2D_UserScissorEnable |
	PM3Config2D_ForegroundROPEnable |
	PM3Config2D_ForegroundROP(rop) |
	PM3Config2D_FBWriteEnable;

    if (xdir == 1) pGlint->PM3_Render2D |= PM3Render2D_XPositive;
    if (ydir == 1) pGlint->PM3_Render2D |= PM3Render2D_YPositive;

    if ((rop!=GXclear)&&(rop!=GXset)&&(rop!=GXnoop)&&(rop!=GXinvert)) {
	pGlint->PM3_Render2D |= PM3Render2D_FBSourceReadEnable;
	pGlint->PM3_Config2D |= PM3Config2D_Blocking;
    }

    if ((rop!=GXclear)&&(rop!=GXset)&&(rop!=GXcopy)&&(rop!=GXcopyInverted))
	pGlint->PM3_Config2D |= PM3Config2D_FBDestReadEnable;

    GLINT_WAIT(2);
    PM3_PLANEMASK(planemask);
    GLINT_WRITE_REG(pGlint->PM3_Config2D, PM3Config2D);

    TRACE_EXIT("Permedia3SetupForScreenToScreenCopy");
}
static void
Permedia3SubsequentScreenToScreenCopy(ScrnInfoPtr pScrn, int x1, int y1,
					int x2, int y2, int w, int h)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    /* Spans needs to be 32 bit aligned. */
    int x_align = x1 & 0x1f;
    TRACE_ENTER("Permedia3SubsequentScreenToScreenCopy");
    GLINT_WAIT(5);
    GLINT_WRITE_REG(((y2&0x0fff)<<16)|(x2&0x0fff), ScissorMinXY);
    GLINT_WRITE_REG((((y2+h)&0x0fff)<<16)|((x2+w)&0x0fff), ScissorMaxXY);
    GLINT_WRITE_REG(
	PM3RectanglePosition_XOffset(x2-x_align) |
	PM3RectanglePosition_YOffset(y2),
	PM3RectanglePosition);
    GLINT_WRITE_REG(
	PM3FBSourceReadBufferOffset_XOffset(x1-x2)|
	PM3FBSourceReadBufferOffset_YOffset(y1-y2),
	PM3FBSourceReadBufferOffset);
    GLINT_WRITE_REG(pGlint->PM3_Render2D |
	PM3Render2D_Width(w+x_align)|
	PM3Render2D_Height(h),
	PM3Render2D);
    TRACE_EXIT("Permedia3SubsequentScreenToScreenCopy");
}

/* Solid Fills */
static void
Permedia3SetupForFillRectSolid(ScrnInfoPtr pScrn, int color, 
				    int rop, unsigned int planemask)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    TRACE_ENTER("Permedia3SetupForFillRectSolid");
    /* Prepare Common Render2D & Config2D data */
    pGlint->PM3_Render2D =
	PM3Render2D_XPositive |
	PM3Render2D_YPositive |
	PM3Render2D_Operation_Normal;
    pGlint->PM3_Config2D =
	PM3Config2D_UseConstantSource |
	PM3Config2D_ForegroundROPEnable |
	PM3Config2D_ForegroundROP(rop) |
	PM3Config2D_FBWriteEnable;
    GLINT_WAIT(3);
    REPLICATE(color);
    /* We can't do block fills properly at 32bpp, so we can stick the chip
     * into 16bpp and double the width and xcoord, but it seems that at
     * extremely high resolutions (above 1600) it doesn't fill.
     * so, we fall back to the slower span filling method.
     */
    if ((rop == GXcopy) && (pScrn->bitsPerPixel == 32) && 
	(pScrn->displayWidth <= 1600)) {
    	pGlint->AccelInfoRec->SubsequentSolidFillRect = 
		Permedia3SubsequentFillRectSolid32bpp;
	if (pGlint->PM3_UsingSGRAM) {
	    GLINT_WRITE_REG(color, PM3FBBlockColor);
	} else {
	    pGlint->PM3_Render2D |= PM3Render2D_SpanOperation;
	    GLINT_WRITE_REG(color, PM3ForegroundColor);
	}
    } else {
    	pGlint->AccelInfoRec->SubsequentSolidFillRect = 
		Permedia3SubsequentFillRectSolid;
    	/* Can't do block fills at 8bpp either */
    	if ((rop == GXcopy) && (pScrn->bitsPerPixel == 16)) {
	    if (pGlint->PM3_UsingSGRAM) {
	        GLINT_WRITE_REG(color, PM3FBBlockColor);
	    } else {
	        pGlint->PM3_Render2D |= PM3Render2D_SpanOperation;
		GLINT_WRITE_REG(color, PM3ForegroundColor);
	    }
        } else {
	    pGlint->PM3_Render2D |= PM3Render2D_SpanOperation;
	    GLINT_WRITE_REG(color, PM3ForegroundColor);
    	}
    }
    PM3_PLANEMASK(planemask);
    if (((rop!=GXclear)&&(rop!=GXset)&&(rop!=GXcopy)&&(rop!=GXcopyInverted))
      || ((planemask != 0xffffffff) && !(pGlint->PM3_UsingSGRAM)))
	pGlint->PM3_Config2D |= PM3Config2D_FBDestReadEnable;
    GLINT_WRITE_REG(pGlint->PM3_Config2D, PM3Config2D);
    TRACE_EXIT("Permedia3SetupForFillRectSolid");
}

static void
Permedia3SubsequentFillRectSolid(ScrnInfoPtr pScrn, int x, int y, int w, int h)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    TRACE_ENTER("Permedia3SubsequentFillRectSolid");

    GLINT_WAIT(2);
    GLINT_WRITE_REG(
	PM3RectanglePosition_XOffset(x) |
	PM3RectanglePosition_YOffset(y),
	PM3RectanglePosition);
    GLINT_WRITE_REG(pGlint->PM3_Render2D |
	PM3Render2D_Width(w) | PM3Render2D_Height(h),
	PM3Render2D);

    TRACE_EXIT("Permedia3SubsequentFillRectSolid");
}

static void
Permedia3SubsequentFillRectSolid32bpp(ScrnInfoPtr pScrn, int x, int y, int w, int h)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    TRACE_ENTER("Permedia3SubsequentFillRectSolid32bpp");

    GLINT_WAIT(6);

    /* Put the chip into 16bpp mode */
    GLINT_WRITE_REG(1, PixelSize);
    /* Now double the displayWidth */
    GLINT_WRITE_REG(
	PM3FBWriteBufferWidth_Width(pScrn->displayWidth<<1),
	PM3FBWriteBufferWidth0);

    /* and double the x,w coords */
    GLINT_WRITE_REG(
	PM3RectanglePosition_XOffset(x<<1) |
	PM3RectanglePosition_YOffset(y),
	PM3RectanglePosition);
    GLINT_WRITE_REG(pGlint->PM3_Render2D |
	PM3Render2D_Width(w<<1) | PM3Render2D_Height(h),
	PM3Render2D);
    
    /* Now fixup */
    GLINT_WRITE_REG(
	PM3FBWriteBufferWidth_Width(pScrn->displayWidth),
	PM3FBWriteBufferWidth0);
    GLINT_WRITE_REG(0, PixelSize);
    TRACE_EXIT("Permedia3SubsequentFillRectSolid32bpp");
}

/* 8x8 Mono Pattern Fills */
static void 
Permedia3SetupForMono8x8PatternFill(ScrnInfoPtr pScrn, 
			   int patternx, int patterny, 
			   int fg, int bg, int rop,
			   unsigned int planemask)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    TRACE_ENTER("Permedia3SetupForMono8x8PatternFill");
    REPLICATE(fg);
    pGlint->PM3_Render2D =
	PM3Render2D_AreaStippleEnable |
	PM3Render2D_SpanOperation |
	PM3Render2D_XPositive |
	PM3Render2D_YPositive |
	PM3Render2D_Operation_Normal;
    pGlint->PM3_Config2D =
	PM3Config2D_UseConstantSource |
	PM3Config2D_ForegroundROPEnable |
	PM3Config2D_ForegroundROP(rop) |
	PM3Config2D_FBWriteEnable;
    if ((rop!=GXclear)&&(rop!=GXset)&&(rop!=GXcopy)&&(rop!=GXcopyInverted))
	pGlint->PM3_Config2D |= PM3Config2D_FBDestReadEnable;
    pGlint->PM3_AreaStippleMode = 1;
/* Mirror stipple pattern horizontally */
#if X_BYTE_ORDER == X_BIG_ENDIAN
    pGlint->PM3_AreaStippleMode |= (1<<18);
#endif
    pGlint->PM3_AreaStippleMode |= (2<<1);
    pGlint->PM3_AreaStippleMode |= (2<<4);
    if (bg != -1) {
	REPLICATE(bg);
	pGlint->PM3_Config2D |= PM3Config2D_OpaqueSpan;
	pGlint->PM3_AreaStippleMode |= 1<<20;
	GLINT_WAIT(12);
    	GLINT_WRITE_REG(bg, BackgroundColor);
    }
    else GLINT_WAIT(11);
    GLINT_WRITE_REG((patternx & 0xFF), AreaStipplePattern0);
    GLINT_WRITE_REG((patternx & 0xFF00) >> 8, AreaStipplePattern1);
    GLINT_WRITE_REG((patternx & 0xFF0000) >> 16, AreaStipplePattern2);
    GLINT_WRITE_REG((patternx & 0xFF000000) >> 24, AreaStipplePattern3);
    GLINT_WRITE_REG((patterny & 0xFF), AreaStipplePattern4);
    GLINT_WRITE_REG((patterny & 0xFF00) >> 8, AreaStipplePattern5);
    GLINT_WRITE_REG((patterny & 0xFF0000) >> 16, AreaStipplePattern6);
    GLINT_WRITE_REG((patterny & 0xFF000000) >> 24, AreaStipplePattern7);
    GLINT_WRITE_REG(fg, PM3ForegroundColor);
    PM3_PLANEMASK(planemask);
    GLINT_WRITE_REG(pGlint->PM3_Config2D, PM3Config2D);
    TRACE_EXIT("Permedia3SetupForMono8x8PatternFill");
}
static void 
Permedia3SubsequentMono8x8PatternFillRect(ScrnInfoPtr pScrn, 
			   int x_offset, int y_offset,
			   int x, int y, int w, int h)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    TRACE_ENTER("Permedia3SubsequentMono8x8PatternFillRect");
    GLINT_WAIT(3);
    GLINT_WRITE_REG(
	PM3RectanglePosition_XOffset(x) |
	PM3RectanglePosition_YOffset(y),
	PM3RectanglePosition);
    GLINT_WRITE_REG(
	(x_offset&0x7)<<7 | (y_offset&0x7)<<12 |
	pGlint->PM3_AreaStippleMode,
	AreaStippleMode);
    GLINT_WRITE_REG(pGlint->PM3_Render2D |
	PM3Render2D_Width(w) | PM3Render2D_Height(h),
	PM3Render2D);
    TRACE_EXIT("Permedia3SubsequentMono8x8PatternFillRect");
}

static void
Permedia3SetupForScanlineCPUToScreenColorExpandFill(
	ScrnInfoPtr pScrn,
	int fg, int bg, 
	int rop, 
	unsigned int planemask
){
    GLINTPtr pGlint = GLINTPTR(pScrn);

    REPLICATE(fg);
    pGlint->PM3_Render2D =
	PM3Render2D_SpanOperation |
	PM3Render2D_XPositive |
	PM3Render2D_YPositive |
	PM3Render2D_Operation_SyncOnBitMask;
    pGlint->PM3_Config2D =
	PM3Config2D_UserScissorEnable |
	PM3Config2D_UseConstantSource |
	PM3Config2D_ForegroundROPEnable |
	PM3Config2D_ForegroundROP(rop) |
	PM3Config2D_FBWriteEnable;
    if ((rop!=GXclear)&&(rop!=GXset)&&(rop!=GXcopy)&&(rop!=GXcopyInverted))
	pGlint->PM3_Config2D |= PM3Config2D_FBDestReadEnable;
    if (bg != -1) {
	REPLICATE(bg);
	pGlint->PM3_Config2D |= PM3Config2D_OpaqueSpan;
	GLINT_WAIT(4);
    	GLINT_WRITE_REG(bg, BackgroundColor);
    }
    else GLINT_WAIT(3);
    GLINT_WRITE_REG(fg, PM3ForegroundColor);
    PM3_PLANEMASK(planemask);
    GLINT_WRITE_REG(pGlint->PM3_Config2D, PM3Config2D);
}

static void
Permedia3SubsequentScanlineCPUToScreenColorExpandFill(
	ScrnInfoPtr pScrn,
	int x, int y, int w, int h,
	int skipleft
){
    GLINTPtr pGlint = GLINTPTR(pScrn);

    TRACE_ENTER("Permedia2SubsequentScanlineCPUToScreenColorExpandFill");
	
    pGlint->dwords = ((w + 31) >> 5); /* dwords per scanline */
 
    pGlint->cpucount = h;

    GLINT_WAIT(5);
    GLINT_WRITE_REG(((y&0x0fff)<<16)|((x+skipleft)&0x0fff), ScissorMinXY);
    GLINT_WRITE_REG((((y+h)&0x0fff)<<16)|((x+w)&0x0fff), ScissorMaxXY);
    GLINT_WRITE_REG(
	PM3RectanglePosition_XOffset(x) |
	PM3RectanglePosition_YOffset(y),
	PM3RectanglePosition);
    GLINT_WRITE_REG(pGlint->PM3_Render2D |
	PM3Render2D_Width(w) | PM3Render2D_Height(h),
	PM3Render2D);

#if defined(__alpha__)
    if (0) /* force Alpha to use indirect always */
#else
    if ((pGlint->dwords*h) < pGlint->FIFOSize)
#endif
    {
	/* Turn on direct for less than 120 dword colour expansion */
    	pGlint->XAAScanlineColorExpandBuffers[0] = pGlint->IOBase+OutputFIFO+4;
	pGlint->ScanlineDirect = 1;
    	GLINT_WRITE_REG(((pGlint->dwords*h)-1)<<16 | 0x0D, OutputFIFO);
    	GLINT_WAIT(pGlint->dwords*h);
    } else {
	/* Use indirect for anything else */
    	pGlint->XAAScanlineColorExpandBuffers[0] = pGlint->ScratchBuffer;
	pGlint->ScanlineDirect   = 0;
    }

    pGlint->cpucount--;
}

static void
Permedia3SubsequentColorExpandScanline(ScrnInfoPtr pScrn, int bufno)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    CARD32 *srcp = (CARD32*)pGlint->XAAScanlineColorExpandBuffers[bufno];
    int dwords = pGlint->dwords;

    if (!pGlint->ScanlineDirect) {
	while(dwords >= pGlint->FIFOSize) {
	    GLINT_WAIT(pGlint->FIFOSize);
            GLINT_WRITE_REG(((pGlint->FIFOSize - 2) << 16) | 0x0D, OutputFIFO);
	    GLINT_MoveDWORDS(
			(CARD32*)((char*)pGlint->IOBase + OutputFIFO + 4),
	 		(CARD32*)srcp, pGlint->FIFOSize - 1);
	    dwords -= pGlint->FIFOSize - 1;
	    srcp += pGlint->FIFOSize - 1;
	}
	if(dwords) {
	    GLINT_WAIT(dwords + 1);
            GLINT_WRITE_REG(((dwords - 1) << 16) | 0x0D, OutputFIFO);
	    GLINT_MoveDWORDS(
			(CARD32*)((char*)pGlint->IOBase + OutputFIFO + 4),
	 		(CARD32*)srcp, dwords);
	}
    }
}

/* Images Writes */
static void Permedia3SetupForScanlineImageWrite(ScrnInfoPtr pScrn, int rop,
	unsigned int planemask, int trans_color, int bpp, int depth)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    TRACE_ENTER("Permedia3SetupForScanlineImageWrite");
    pGlint->PM3_Render2D =
	PM3Render2D_SpanOperation |
	PM3Render2D_XPositive |
	PM3Render2D_YPositive |
	PM3Render2D_Operation_SyncOnHostData;
    pGlint->PM3_Config2D =
	PM3Config2D_UserScissorEnable |
	PM3Config2D_ForegroundROPEnable |
	PM3Config2D_ForegroundROP(rop) |
	PM3Config2D_FBWriteEnable;
    if ((rop!=GXclear)&&(rop!=GXset)&&(rop!=GXcopy)&&(rop!=GXcopyInverted))
	pGlint->PM3_Config2D |= PM3Config2D_FBDestReadEnable;
    GLINT_WAIT(2);
    PM3_PLANEMASK(planemask);
    GLINT_WRITE_REG(pGlint->PM3_Config2D, PM3Config2D);
    TRACE_EXIT("Permedia3SetupForScanlineImageWrite");
}

static void Permedia3SubsequentScanlineImageWriteRect(ScrnInfoPtr pScrn, 
	int x, int y, int w, int h, int skipleft)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    TRACE_ENTER("Permedia3SubsequentScanlineImageWrite");
    pGlint->dwords = (((w * pScrn->bitsPerPixel) + 3) >> 2); /* per scanline */

    pGlint->cpucount = h;
    GLINT_WAIT(5);
    GLINT_WRITE_REG(((y&0x0fff)<<16)|((x+skipleft)&0x0fff), ScissorMinXY);
    GLINT_WRITE_REG((((y+h)&0x0fff)<<16)|((x+w)&0x0fff), ScissorMaxXY);
    GLINT_WRITE_REG(
	PM3RectanglePosition_XOffset(x) |
	PM3RectanglePosition_YOffset(y),
	PM3RectanglePosition);
    GLINT_WRITE_REG(pGlint->PM3_Render2D |
	PM3Render2D_Width(w) | PM3Render2D_Height(h),
	PM3Render2D);

#if defined(__alpha__)
    if (0) /* force Alpha to use indirect always */
#else
    if (pGlint->dwords < pGlint->FIFOSize)
#endif
    {
	/* Turn on direct for less than 120 dword colour expansion */
    	pGlint->XAAScanlineColorExpandBuffers[0] = pGlint->IOBase+OutputFIFO+4;
	pGlint->ScanlineDirect = 1;
    	GLINT_WRITE_REG(((pGlint->dwords*h)-1)<<16 | (0x15<<4) | 0x05, 
								OutputFIFO);
    	GLINT_WAIT(pGlint->dwords);
    } else {
	/* Use indirect for anything else */
    	pGlint->XAAScanlineColorExpandBuffers[0] = pGlint->ScratchBuffer;
	pGlint->ScanlineDirect   = 0;
    }

    pGlint->cpucount--;
    TRACE_EXIT("Permedia3SubsequentScanlineImageWrite");
}

static void
Permedia3SubsequentImageWriteScanline(ScrnInfoPtr pScrn, int bufno)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    int dwords = pGlint->dwords;

    if (pGlint->ScanlineDirect) {
    	if (pGlint->cpucount--)
    	    GLINT_WAIT(dwords);
	return;
    } else {
	while(dwords >= pGlint->FIFOSize) {
	    GLINT_WAIT(pGlint->FIFOSize);
            GLINT_WRITE_REG(((pGlint->FIFOSize - 2) << 16) | (0x15 << 4) |
							0x05, OutputFIFO);
	    GLINT_MoveDWORDS(
			(CARD32*)((char*)pGlint->IOBase + OutputFIFO + 4),
	 		(CARD32*)pGlint->XAAScanlineColorExpandBuffers[bufno],
			pGlint->FIFOSize - 1);
	    dwords -= pGlint->FIFOSize - 1;
	}
	if(dwords) {
	    GLINT_WAIT(dwords + 1);
            GLINT_WRITE_REG(((dwords - 1) << 16) | (0x15 << 4) | 
							0x05, OutputFIFO);
	    GLINT_MoveDWORDS(
			(CARD32*)((char*)pGlint->IOBase + OutputFIFO + 4),
	 		(CARD32*)pGlint->XAAScanlineColorExpandBuffers[bufno],
			dwords);
	}
    }
}

static void
Permedia3RestoreAccelState(ScrnInfoPtr pScrn)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    if ((IS_J2000) && (pGlint->Chipset == PCI_VENDOR_3DLABS_CHIP_GAMMA)) {
	GLINT_SLOW_WRITE_REG(pGlint->MultiIndex, BroadcastMask);
    }
    Permedia3Sync(pScrn);
}

static void
Permedia3WritePixmap(
    ScrnInfoPtr pScrn,
    int x, int y, int w, int h,
    unsigned char *src,
    int srcwidth,
    int rop,
    unsigned int planemask,
    int trans,
    int bpp, int depth
)
{
    int dwords;
    int count;
    int skipleft = (long)src & 0x03L;
    int Bpp = bpp >> 3;
    CARD32 *srcp;
    GLINTPtr pGlint = GLINTPTR(pScrn);
    TRACE_ENTER("Permedia3WritePixmap");

    if (skipleft) {
	/* Skipleft is either
	 *   - 0, 1, 2 or 3 in 8 bpp
	 *   - 0 or 1 in 16 bpp
	 *   - 0 in 32 bpp
	 */
	skipleft /= Bpp;

	x -= skipleft;	     
	w += skipleft;

	src = (unsigned char*)((long)src & ~0x03L);     
    }

    pGlint->PM3_Render2D =
	PM3Render2D_SpanOperation |
	PM3Render2D_XPositive |
	PM3Render2D_YPositive |
	PM3Render2D_Operation_SyncOnHostData;
    pGlint->PM3_Config2D =
	PM3Config2D_UserScissorEnable |
	PM3Config2D_ForegroundROPEnable |
	PM3Config2D_ForegroundROP(rop) |
	PM3Config2D_FBWriteEnable;
    if ((rop!=GXclear)&&(rop!=GXset)&&(rop!=GXcopy)&&(rop!=GXcopyInverted))
	pGlint->PM3_Config2D |= PM3Config2D_FBDestReadEnable;
    GLINT_WAIT(6);
    PM3_PLANEMASK(planemask);
    GLINT_WRITE_REG(pGlint->PM3_Config2D, PM3Config2D);
    GLINT_WRITE_REG(((y&0x0fff)<<16)|((x+skipleft)&0x0fff), ScissorMinXY);
    GLINT_WRITE_REG((((y+h)&0x0fff)<<16)|((x+w)&0x0fff), ScissorMaxXY);
    GLINT_WRITE_REG(
	PM3RectanglePosition_XOffset(x) |
	PM3RectanglePosition_YOffset(y),
	PM3RectanglePosition);
    GLINT_WRITE_REG(pGlint->PM3_Render2D |
	PM3Render2D_Width(w) | PM3Render2D_Height(h),
	PM3Render2D);
    /* width of the stuff to copy in 32 bit words */
    dwords = ((w * Bpp) + 3) >> 2;

    while(h--) {
	count = dwords;
	srcp = (CARD32*)src;
	while(count >= pGlint->FIFOSize) {
	    GLINT_WAIT(pGlint->FIFOSize);
	    /* (0x15 << 4) | 0x05 is the TAG for FBSourceData */
            GLINT_WRITE_REG(((pGlint->FIFOSize - 2) << 16) | (0x15 << 4) | 
					0x05, OutputFIFO);
	    GLINT_MoveDWORDS(
			(CARD32*)((char*)pGlint->IOBase + OutputFIFO + 4),
	 		(CARD32*)srcp, pGlint->FIFOSize - 1);
	    count -= pGlint->FIFOSize - 1;
	    srcp += pGlint->FIFOSize - 1;
	}
	if(count) {
	    GLINT_WAIT(count + 1);
	    /* (0x15 << 4) | 0x05 is the TAG for FBSourceData */
            GLINT_WRITE_REG(((count - 1) << 16) | (0x15 << 4) | 
					0x05, OutputFIFO);
	    GLINT_MoveDWORDS(
			(CARD32*)((char*)pGlint->IOBase + OutputFIFO + 4),
	 		(CARD32*)srcp, count);
	}
	src += srcwidth;
    }  

    Permedia3DisableClipping(pScrn);
    Permedia3Sync(pScrn);
}

static void
Permedia3WriteBitmap(ScrnInfoPtr pScrn,
    int x, int y, int w, int h,
    unsigned char *src,
    int srcwidth, int skipleft,
    int fg, int bg, int rop,
    unsigned int planemask
)
{
    int dwords;
    GLINTPtr pGlint = GLINTPTR(pScrn);
    int count;
    CARD32 *srcp;
    TRACE_ENTER("Permedia3WriteBitmap");

    w += skipleft;
    x -= skipleft;
    dwords = (w + 31) >>5;

    REPLICATE(fg);
    pGlint->PM3_Render2D =
	PM3Render2D_SpanOperation |
	PM3Render2D_XPositive |
	PM3Render2D_YPositive |
	PM3Render2D_Operation_SyncOnBitMask;
    pGlint->PM3_Config2D =
	PM3Config2D_UserScissorEnable |
	PM3Config2D_UseConstantSource |
	PM3Config2D_ForegroundROPEnable |
	PM3Config2D_ForegroundROP(rop) |
	PM3Config2D_FBWriteEnable;
    if ((rop!=GXclear)&&(rop!=GXset)&&(rop!=GXcopy)&&(rop!=GXcopyInverted))
	pGlint->PM3_Config2D |= PM3Config2D_FBDestReadEnable;
    if (bg != -1) {
	REPLICATE(bg);
	pGlint->PM3_Config2D |= PM3Config2D_OpaqueSpan;
	GLINT_WAIT(8);
    	GLINT_WRITE_REG(bg, BackgroundColor);
    }
    else GLINT_WAIT(7);
    GLINT_WRITE_REG(fg, PM3ForegroundColor);
    PM3_PLANEMASK(planemask);
    GLINT_WRITE_REG(pGlint->PM3_Config2D, PM3Config2D);
    GLINT_WRITE_REG(((y&0x0fff)<<16)|((x+skipleft)&0x0fff), ScissorMinXY);
    GLINT_WRITE_REG((((y+h)&0x0fff)<<16)|((x+w)&0x0fff), ScissorMaxXY);
    GLINT_WRITE_REG(
	PM3RectanglePosition_XOffset(x) |
	PM3RectanglePosition_YOffset(y),
	PM3RectanglePosition);
    GLINT_WRITE_REG(pGlint->PM3_Render2D |
	PM3Render2D_Width(w) | PM3Render2D_Height(h),
	PM3Render2D);

    while(h--) {
	count = dwords;
	srcp = (CARD32*)src;
	while(count >= pGlint->FIFOSize) {
	    GLINT_WAIT(pGlint->FIFOSize);
            GLINT_WRITE_REG(((pGlint->FIFOSize - 2) << 16) |
					0x0D, OutputFIFO);
	    GLINT_MoveDWORDS(
			(CARD32*)((char*)pGlint->IOBase + OutputFIFO + 4),
	 		(CARD32*)srcp, pGlint->FIFOSize - 1);
	    count -= pGlint->FIFOSize - 1;
	    srcp += pGlint->FIFOSize - 1;
	}
	if(count) {
	    GLINT_WAIT(count + 1);
            GLINT_WRITE_REG(((count - 1) << 16) | 0x0D, OutputFIFO);
	    GLINT_MoveDWORDS(
			(CARD32*)((char*)pGlint->IOBase + OutputFIFO + 4),
	 		(CARD32*)srcp, count);
	}
	src += srcwidth;
    }  

    Permedia3DisableClipping(pScrn);
    Permedia3Sync(pScrn);
}
