/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/apm/apm_accel.c,v 1.21 2002/10/08 22:14:03 tsi Exp $ */

#include "apm.h"
#include "miline.h"

/* Defines */
#define MAXLOOP 1000000

/* Translation from X ROP's to APM ROP's. */
static unsigned char apmROP[] = {
  0,
  0x88,
  0x44,
  0xCC,
  0x22,
  0xAA,
  0x66,
  0xEE,
  0x11,
  0x99,
  0x55,
  0xDD,
  0x33,
  0xBB,
  0x77,
  0xFF
};


#include "apm_funcs.c"

#define IOP_ACCESS
#include "apm_funcs.c"

#define PSZ	24
#include "apm_funcs.c"

#define PSZ	24
#define IOP_ACCESS
#include "apm_funcs.c"

static void
ApmRemoveStipple(FBAreaPtr area)
{
    ((struct ApmStippleCacheRec *)area->devPrivate.ptr)->apmStippleCached = FALSE;
}

static void
ApmMoveStipple(FBAreaPtr from, FBAreaPtr to)
{
    struct ApmStippleCacheRec *pApm = (struct ApmStippleCacheRec *)to->devPrivate.ptr;

    pApm->apmStippleCache.x = to->box.x1;
    pApm->apmStippleCache.y += to->box.y1 - from->box.y1;
    /* TODO : move data */
}

/*
 * ApmCacheMonoStipple
 * because my poor AT3D needs stipples stored linearly in memory.
 */
static XAACacheInfoPtr
ApmCacheMonoStipple(ScrnInfoPtr pScrn, PixmapPtr pPix)
{
    APMDECL(pScrn);
    int		w = pPix->drawable.width, W = (w + 31) & ~31;
    int		h = pPix->drawable.height;
    int		i, j, dwords, mem, width, funcNo;
    FBAreaPtr	draw;
    struct ApmStippleCacheRec	*pCache;
    unsigned char	*srcPtr;
    CARD32		*dstPtr;

    for (i = 0; i < APM_CACHE_NUMBER; i++)
	if ((pApm->apmCache[i].apmStippleCache.serialNumber == pPix->drawable.serialNumber)
		&& pApm->apmCache[i].apmStippleCached &&
		(pApm->apmCache[i].apmStippleCache.fg == -1) &&
		(pApm->apmCache[i].apmStippleCache.bg == -1)) {
	    pApm->apmCache[i].apmStippleCache.trans_color = -1;
	    return &pApm->apmCache[i].apmStippleCache;
	}
    if ((i = ++pApm->apmCachePtr) >= APM_CACHE_NUMBER)
	i = pApm->apmCachePtr = 0;
    pCache = &pApm->apmCache[i];
    if (pCache->apmStippleCached) {
	pCache->apmStippleCached = FALSE;
	xf86FreeOffscreenArea(pCache->area);
    }

    draw = xf86AllocateLinearOffscreenArea(pApm->pScreen, (W * h + 7) / 8,
				    (pApm->CurrentLayout.mask32 + 1) << 1,
				    ApmMoveStipple, ApmRemoveStipple, pCache);
    if (!draw)
	return NULL;	/* Let's hope this will never happen... */

    pCache->area = draw;
    pCache->apmStippleCache.serialNumber = pPix->drawable.serialNumber;
    pCache->apmStippleCache.trans_color =
	pCache->apmStippleCache.bg =
	pCache->apmStippleCache.fg = -1;
    pCache->apmStippleCache.orig_w = w;
    pCache->apmStippleCache.orig_h = h;
    pCache->apmStippleCache.x = draw->box.x1;
    pCache->apmStippleCache.y = draw->box.y1 + ((pCache - pApm->apmCache) + 1) * pApm->CurrentLayout.Scanlines;
    mem = ((draw->box.x2 - draw->box.x1) * (draw->box.y2 - draw->box.y1) *
			pScrn->bitsPerPixel) / (W * h);
    width = 2;
    while (width * width <= mem)
	width++;
    width--;
    pCache->apmStippleCache.w = (width * W + pScrn->bitsPerPixel - 1) /
			pScrn->bitsPerPixel;
    pCache->apmStippleCache.h = ((draw->box.x2 - draw->box.x1) *
			(draw->box.y2 - draw->box.y1)) /
			pCache->apmStippleCache.w;
    pCache->apmStippleCached = TRUE;

    if (w < 32) {
	if (w & (w - 1))	funcNo = 1;
	else			funcNo = 0;
    } else			funcNo = 2;

    dstPtr = ((CARD32 *)pApm->FbBase) + (draw->box.x1 +
			draw->box.y1*pApm->CurrentLayout.bytesPerScanline) / 4;
    j = 0;
    dwords = (pCache->apmStippleCache.w * pScrn->bitsPerPixel) / 32;
    while (j + h <= pCache->apmStippleCache.h) {
	srcPtr = (unsigned char *)pPix->devPrivate.ptr;
	for (i = h; --i >= 0; ) {
	    (*XAAStippleScanlineFuncMSBFirst[funcNo])(dstPtr, (CARD32 *)srcPtr, 0, w, dwords);
	    srcPtr += pPix->devKind;
	    dstPtr += dwords;
	}
	j += h;
    }
    srcPtr = (unsigned char *)pPix->devPrivate.ptr;
    for (i = pCache->apmStippleCache.h - j ; --i >= 0; ) {
	(*XAAStippleScanlineFuncMSBFirst[funcNo])(dstPtr, (CARD32 *)srcPtr, 0, w, dwords);
	srcPtr += pPix->devKind;
	dstPtr += dwords;
    }

    return &pCache->apmStippleCache;
}

#if 0
extern GCOps XAAPixmapOps;
static RegionPtr (*SaveCopyAreaPixmap)(DrawablePtr, DrawablePtr, GC *, int, int, int, int, int, int);

static RegionPtr
ApmCopyAreaPixmap(DrawablePtr pSrcDrawable, DrawablePtr pDstDrawable, GC *pGC,
		int srcx, int srcy,
		int width, int height,
		int dstx, int dsty )
{
    register int Scanlines;
    int is;
    int id;
    int sx = 0, sy = 0, dx = 0, dy = 0, pitch;
    RegionPtr pReg;

    if (APMPTR(xf86Screens[(pGC)->pScreen->myNum])->Chipset == AT3D) {
	Scanlines = APMPTR(xf86Screens[(pGC)->pScreen->myNum])->CurrentLayout.Scanlines;
	is = (pSrcDrawable->type == DRAWABLE_PIXMAP) ? APM_GET_PIXMAP_PRIVATE(pSrcDrawable)->num : 0;
	id = (pDstDrawable->type == DRAWABLE_PIXMAP) ? APM_GET_PIXMAP_PRIVATE(pDstDrawable)->num : 0;
	if (is) {
	    sx = pSrcDrawable->x;
	    sy = pSrcDrawable->y % Scanlines;
	    pitch = 2 * pSrcDrawable->width;
	    pSrcDrawable->x = (sx + ((PixmapPtr)pSrcDrawable)->devKind * sy) % pitch;
	    pSrcDrawable->y = (sx + ((PixmapPtr)pSrcDrawable)->devKind * sy) / pitch;
	    ((PixmapPtr)pSrcDrawable)->devKind = pitch;
	    pSrcDrawable->depth = 16;
	}
	if (id) {
	    dx = pDstDrawable->x;
	    dy = pDstDrawable->y % Scanlines;
	    pitch = 2 * pDstDrawable->width;
	    pDstDrawable->x = (dx + ((PixmapPtr)pDstDrawable)->devKind * dy) % pitch;
	    pDstDrawable->y = (dx + ((PixmapPtr)pDstDrawable)->devKind * dy) / pitch;
	    ((PixmapPtr)pDstDrawable)->devKind = pitch;
	    pDstDrawable->depth = 16;
	}
	pReg = (*SaveCopyAreaPixmap)(pSrcDrawable, pDstDrawable, pGC,
					    srcx, srcy % Scanlines,
					    width, height,
					    dstx, dsty % Scanlines);
	if (is) {
	    pSrcDrawable->x = sx;
	    pSrcDrawable->y = sy + is * Scanlines;
	}
	if (id) {
	    pDstDrawable->x = dx;
	    pDstDrawable->y = dy + id * Scanlines;
	}
	return pReg;
    }
    return (*SaveCopyAreaPixmap)(pSrcDrawable, pDstDrawable, pGC,
					srcx, srcy,
					width, height,
					dstx, dsty);
}
#endif

void ApmAccelReserveSpace(ApmPtr pApm)
{
    memType	mem, ScratchMemOffset;

    mem			= xf86Screens[pApm->pScreen->myNum]->videoRam << 10;
    /*
     * Reserve at least four lines for mono to color expansion
     */
    ScratchMemOffset	= ((mem - pApm->OffscreenReserved) /
			pApm->CurrentLayout.bytesPerScanline - 4) *
			pApm->CurrentLayout.bytesPerScanline;
    pApm->ScratchMemSize= mem - ScratchMemOffset - pApm->OffscreenReserved;
    pApm->ScratchMemPtr	= pApm->ScratchMemOffset
			= (memType)pApm->FbBase + ScratchMemOffset;
    pApm->ScratchMemEnd	= (memType)pApm->FbBase + mem - pApm->OffscreenReserved;
}

/*********************************************************************************************/

int
ApmAccelInit(ScreenPtr pScreen)
{
    ScrnInfoPtr		pScrn = xf86Screens[pScreen->myNum];
    APMDECL(pScrn);
    XAAInfoRecPtr	pXAAinfo;
    BoxRec		AvailFBArea;
    memType		mem, ScratchMemOffset;
    int			i, stat;

    pApm->AccelInfoRec	= pXAAinfo = XAACreateInfoRec();
    if (!pXAAinfo)
	return FALSE;

    mem			= pScrn->videoRam << 10;
    ScratchMemOffset	= pApm->ScratchMemOffset - (memType)pApm->FbBase;
    switch (pApm->CurrentLayout.bitsPerPixel) {
    case 8:
    case 24:
	pApm->ScratchMemWidth =
		(mem - ScratchMemOffset - pApm->OffscreenReserved) / 1;
	pApm->ScratchMem =
		((ScratchMemOffset & 0xFFF000) << 4) |
		    (ScratchMemOffset & 0xFFF);
	break;

    case 16:
	pApm->ScratchMemWidth =
		(mem - ScratchMemOffset - pApm->OffscreenReserved) / 2;
	pApm->ScratchMem =
		((ScratchMemOffset & 0xFFE000) << 3) |
		    ((ScratchMemOffset & 0x1FFE) >> 1);
	break;

    case 32:
	pApm->ScratchMemWidth =
		(mem - ScratchMemOffset - pApm->OffscreenReserved) / 4;
	pApm->ScratchMem =
		((ScratchMemOffset & 0xFFC000) << 2) |
		    ((ScratchMemOffset & 0x3FFC) >> 2);
	break;
    }
    pApm->OffscreenReserved = mem - ScratchMemOffset;

    /*
     * Abort
     */
    if (pApm->Chipset == AP6422)
	i = 4;
    else
	i = 8;
    if (pApm->noLinear) {
	stat = RDXL_IOP(0x1FC);
	while ((stat & (STATUS_HOSTBLTBUSY | STATUS_ENGINEBUSY)) ||
		((stat & STATUS_FIFO) < i)) {
	    WRXB_IOP(0x1FC, 0);
	    stat = RDXL_IOP(0x1FC);
	}
    }
    else {
	stat = RDXL_M(0x1FC);
	while ((stat & (STATUS_HOSTBLTBUSY | STATUS_ENGINEBUSY)) ||
		((stat & STATUS_FIFO) < i)) {
	    WRXB_M(0x1FC, 0);
	    stat = RDXL_M(0x1FC);
	}
    }

    /* Setup current register values */
    for (i = 0; i < sizeof(pApm->regcurr) / 4; i++)
	((CARD32 *)curr)[i] = RDXL(0x30 + 4*i);

    SETCLIP_CTRL(1);
    SETCLIP_CTRL(0);
    SETBYTEMASK(0x00);
    SETBYTEMASK(0xFF);
    SETROP(ROP_S_xor_D);
    SETROP(ROP_S);

#if 0
    if (XAAPixmapOps.CopyArea != ApmCopyAreaPixmap) {
	SaveCopyAreaPixmap = XAAPixmapOps.CopyArea;
	XAAPixmapOps.CopyArea = ApmCopyAreaPixmap;
    }
#endif

    ApmSetupXAAInfo(pApm, pXAAinfo);

    if (!pApm->noLinear) {
	pApm->SetupForSolidFill			= ApmSetupForSolidFill;
	pApm->SubsequentSolidFillRect		= ApmSubsequentSolidFillRect;
	pApm->SetupForSolidFill24		= ApmSetupForSolidFill24;
	pApm->SubsequentSolidFillRect24		= ApmSubsequentSolidFillRect24;
	pApm->SetupForScreenToScreenCopy	= ApmSetupForScreenToScreenCopy;
	pApm->SubsequentScreenToScreenCopy	= ApmSubsequentScreenToScreenCopy;
	pApm->SetupForScreenToScreenCopy24	= ApmSetupForScreenToScreenCopy24;
	pApm->SubsequentScreenToScreenCopy24	= ApmSubsequentScreenToScreenCopy24;
    }
    else {
	pApm->SetupForSolidFill			= ApmSetupForSolidFill_IOP;
	pApm->SubsequentSolidFillRect		= ApmSubsequentSolidFillRect_IOP;
	pApm->SetupForSolidFill24		= ApmSetupForSolidFill24_IOP;
	pApm->SubsequentSolidFillRect24		= ApmSubsequentSolidFillRect24_IOP;
	pApm->SetupForScreenToScreenCopy	= ApmSetupForScreenToScreenCopy_IOP;
	pApm->SubsequentScreenToScreenCopy	= ApmSubsequentScreenToScreenCopy_IOP;
	pApm->SetupForScreenToScreenCopy24	= ApmSetupForScreenToScreenCopy24_IOP;
	pApm->SubsequentScreenToScreenCopy24	= ApmSubsequentScreenToScreenCopy24_IOP;
    }

    /*
     * Init Rush extension.
     * Must be initialized once per generation.
     */
#ifdef XF86RUSH_EXT
    if (!pApm->CreatePixmap) {
	pApm->CreatePixmap	= pScreen->CreatePixmap;
	pApm->DestroyPixmap	= pScreen->DestroyPixmap;
    }
    XFree86RushExtensionInit(pScreen);
#endif

    /* Pixmap cache setup */
    pXAAinfo->CachePixelGranularity = (pApm->CurrentLayout.mask32 + 1) << 1;
    AvailFBArea.x1 = 0;
    AvailFBArea.y1 = 0;
    AvailFBArea.x2 = pScrn->displayWidth;
    AvailFBArea.y2 = (pScrn->videoRam * 1024 - pApm->OffscreenReserved) /
	(pScrn->displayWidth * ((pScrn->bitsPerPixel + 7) >> 3));

    xf86InitFBManager(pScreen, &AvailFBArea);

    bzero(pApm->apmCache, sizeof pApm->apmCache);

    return XAAInit(pScreen, pXAAinfo);
}

void ApmSetupXAAInfo(ApmPtr pApm, XAAInfoRecPtr pXAAinfo)
{
    pApm->CurrentLayout.Setup_DEC = 0;
    switch(pApm->CurrentLayout.bitsPerPixel)
    {
      case 8:
           pApm->CurrentLayout.Setup_DEC |= DEC_BITDEPTH_8;
           break;
      case 16:
           pApm->CurrentLayout.Setup_DEC |= DEC_BITDEPTH_16;
           break;
      case 24:
	   /* Note : in 24 bpp, the accelerator wants linear coordinates */
           pApm->CurrentLayout.Setup_DEC |= DEC_BITDEPTH_24 | DEC_SOURCE_LINEAR |
				   DEC_DEST_LINEAR;
           break;
      case 32:
           pApm->CurrentLayout.Setup_DEC |= DEC_BITDEPTH_32;
           break;
      default:
           xf86DrvMsg(xf86Screens[pApm->pScreen->myNum]->scrnIndex, X_WARNING,
		    "Cannot set up drawing engine control for bpp = %d\n",
		    pApm->CurrentLayout.bitsPerPixel);
           break;
    }

    switch(pApm->CurrentLayout.displayWidth)
    {
      case 640:
           pApm->CurrentLayout.Setup_DEC |= DEC_WIDTH_640;
           break;
      case 800:
           pApm->CurrentLayout.Setup_DEC |= DEC_WIDTH_800;
           break;
      case 1024:
           pApm->CurrentLayout.Setup_DEC |= DEC_WIDTH_1024;
           break;
      case 1152:
           pApm->CurrentLayout.Setup_DEC |= DEC_WIDTH_1152;
           break;
      case 1280:
           pApm->CurrentLayout.Setup_DEC |= DEC_WIDTH_1280;
           break;
      case 1600:
           pApm->CurrentLayout.Setup_DEC |= DEC_WIDTH_1600;
           break;
      default:
           xf86DrvMsg(xf86Screens[pApm->pScreen->myNum]->scrnIndex, X_WARNING,
		       "Cannot set up drawing engine control "
		       "for screen width = %d\n", pApm->CurrentLayout.displayWidth);
           break;
    }

    if (!pXAAinfo)
	return;

    /*
     * Set up the main acceleration flags.
     */
    pXAAinfo->Flags = PIXMAP_CACHE | LINEAR_FRAMEBUFFER | OFFSCREEN_PIXMAPS;
    pXAAinfo->CacheMonoStipple = ApmCacheMonoStipple;

    if (pApm->CurrentLayout.bitsPerPixel != 24) {
	if (!pApm->noLinear) {
#define	XAA(s)		pXAAinfo->s = Apm##s
	    if (pApm->Chipset < AT24)
		pXAAinfo->Sync = ApmSync6422;
	    else
		XAA(Sync);

	    /* Accelerated filled rectangles */
	    pXAAinfo->SolidFillFlags = NO_PLANEMASK;
	    XAA(SetupForSolidFill);
	    XAA(SubsequentSolidFillRect);

	    /* Accelerated screen to screen color expansion */
	    pXAAinfo->ScreenToScreenColorExpandFillFlags = NO_PLANEMASK;
	    XAA(SetupForScreenToScreenColorExpandFill);
	    XAA(SubsequentScreenToScreenColorExpandFill);

#if 0
	    The constraints of the transfer range are incompatible with the
	    XAA architecture. I rewrote the XAA functions using ImageWrite
	    /* Accelerated CPU to screen color expansion */
	    if ((pApm->Chipset == AT24 && pApm->ChipRev >= 4) ||
		    pApm->Chipset == AT3D) {
		pXAAinfo->CPUToScreenColorExpandFillFlags =
		  NO_PLANEMASK | SCANLINE_PAD_DWORD | CPU_TRANSFER_PAD_QWORD
		  | BIT_ORDER_IN_BYTE_MSBFIRST | LEFT_EDGE_CLIPPING |
		  LEFT_EDGE_CLIPPING_NEGATIVE_X | SYNC_AFTER_COLOR_EXPAND;
		XAA(SetupForCPUToScreenColorExpandFill);
		XAA(SubsequentCPUToScreenColorExpandFill);
		pXAAinfo->ColorExpandBase	= pApm->BltMap;
		pXAAinfo->ColorExpandRange	= (pApm->Chipset >= AT3D) ? 32*1024 : 30*1024;
	    }

	    /* Accelerated image transfers */
	    pXAAinfo->ImageWriteFlags	=
			    LEFT_EDGE_CLIPPING | NO_PLANEMASK |
			    SCANLINE_PAD_DWORD | CPU_TRANSFER_PAD_QWORD |
			    LEFT_EDGE_CLIPPING_NEGATIVE_X |
			    SYNC_AFTER_IMAGE_WRITE;
	    pXAAinfo->ImageWriteBase	= pApm->BltMap;
	    pXAAinfo->ImageWriteRange	= (pApm->Chipset >= AT3D) ? 32*1024 : 30*1024;
	    XAA(SetupForImageWrite);
	    XAA(SubsequentImageWriteRect);
#endif
	    pXAAinfo->WritePixmapFlags = LEFT_EDGE_CLIPPING | NO_PLANEMASK |
			    LEFT_EDGE_CLIPPING_NEGATIVE_X;
	    XAA(WritePixmap);
	    pXAAinfo->FillImageWriteRectsFlags	=
			    LEFT_EDGE_CLIPPING | NO_PLANEMASK |
			    LEFT_EDGE_CLIPPING_NEGATIVE_X;
	    XAA(FillImageWriteRects);
	    pXAAinfo->WriteBitmapFlags = LEFT_EDGE_CLIPPING | NO_PLANEMASK |
			    LEFT_EDGE_CLIPPING_NEGATIVE_X |
			    BIT_ORDER_IN_BYTE_LSBFIRST;
	    XAA(WriteBitmap);
	    pXAAinfo->TEGlyphRendererFlags = LEFT_EDGE_CLIPPING | NO_PLANEMASK |
			    LEFT_EDGE_CLIPPING_NEGATIVE_X;
	    XAA(TEGlyphRenderer);

	    /* Accelerated screen-screen bitblts */
	    pXAAinfo->ScreenToScreenCopyFlags = NO_PLANEMASK;
	    XAA(SetupForScreenToScreenCopy);
	    XAA(SubsequentScreenToScreenCopy);

	    /* Accelerated Line drawing */
	    pXAAinfo->SolidLineFlags = NO_PLANEMASK | HARDWARE_CLIP_LINE;
	    XAA(SetClippingRectangle);
	    pXAAinfo->SolidBresenhamLineErrorTermBits = 15;

	    if (pApm->Chipset >= AT24) {
		XAA(SubsequentSolidBresenhamLine);

		/* Pattern fill */
		pXAAinfo->Mono8x8PatternFillFlags = NO_PLANEMASK |
				HARDWARE_PATTERN_PROGRAMMED_BITS |
				HARDWARE_PATTERN_SCREEN_ORIGIN;
		XAA(SetupForMono8x8PatternFill);
		XAA(SubsequentMono8x8PatternFillRect);
		if (pApm->CurrentLayout.bitsPerPixel == 8) {
		    pXAAinfo->Color8x8PatternFillFlags = NO_PLANEMASK |
				    HARDWARE_PATTERN_SCREEN_ORIGIN;
		    XAA(SetupForColor8x8PatternFill);
		    XAA(SubsequentColor8x8PatternFillRect);
		}
	    }
	    else
		pXAAinfo->SubsequentSolidBresenhamLine =
		    ApmSubsequentSolidBresenhamLine6422;
#undef XAA
	}
	else {
#define	XAA(s)		pXAAinfo->s = Apm##s##_IOP
	    if (pApm->Chipset < AT24)
		pXAAinfo->Sync = ApmSync6422_IOP;
	    else
		XAA(Sync);

	    /* Accelerated filled rectangles */
	    pXAAinfo->SolidFillFlags = NO_PLANEMASK;
	    XAA(SetupForSolidFill);
	    XAA(SubsequentSolidFillRect);

	    /* Accelerated screen to screen color expansion */
	    pXAAinfo->ScreenToScreenColorExpandFillFlags = NO_PLANEMASK;
	    XAA(SetupForScreenToScreenColorExpandFill);
	    XAA(SubsequentScreenToScreenColorExpandFill);

#if 0
	    The constraints of the transfer range are incompatible with the
	    XAA architecture. I rewrote the XAA functions using ImageWrite
	    /* Accelerated CPU to screen color expansion */
	    if ((pApm->Chipset == AT24 && pApm->ChipRev >= 4) ||
		    pApm->Chipset == AT3D) {
		pXAAinfo->CPUToScreenColorExpandFillFlags =
		  NO_PLANEMASK | SCANLINE_PAD_DWORD | CPU_TRANSFER_PAD_QWORD
		  | BIT_ORDER_IN_BYTE_MSBFIRST | LEFT_EDGE_CLIPPING |
		  LEFT_EDGE_CLIPPING_NEGATIVE_X | SYNC_AFTER_COLOR_EXPAND;
		XAA(SetupForCPUToScreenColorExpandFill);
		XAA(SubsequentCPUToScreenColorExpandFill);
		pXAAinfo->ColorExpandBase	= pApm->BltMap;
		pXAAinfo->ColorExpandRange	= (pApm->Chipset >= AT3D) ? 32*1024 : 30*1024;
	    }

	    /* Accelerated image transfers */
	    pXAAinfo->ImageWriteFlags	=
			    LEFT_EDGE_CLIPPING | NO_PLANEMASK |
			    SCANLINE_PAD_DWORD | CPU_TRANSFER_PAD_QWORD;
	    pXAAinfo->ImageWriteBase	= pApm->BltMap;
	    pXAAinfo->ImageWriteRange	= (pApm->Chipset >= AT3D) ? 32*1024 : 30*1024;
	    XAA(SetupForImageWrite);
	    XAA(SubsequentImageWriteRect);
#endif
	    pXAAinfo->WritePixmapFlags = LEFT_EDGE_CLIPPING | NO_PLANEMASK |
			    LEFT_EDGE_CLIPPING_NEGATIVE_X;
	    XAA(WritePixmap);
	    pXAAinfo->FillImageWriteRectsFlags	=
			    LEFT_EDGE_CLIPPING | NO_PLANEMASK |
			    LEFT_EDGE_CLIPPING_NEGATIVE_X;
	    XAA(FillImageWriteRects);
	    pXAAinfo->WriteBitmapFlags = LEFT_EDGE_CLIPPING | NO_PLANEMASK |
			    LEFT_EDGE_CLIPPING_NEGATIVE_X |
			    BIT_ORDER_IN_BYTE_LSBFIRST;
	    XAA(WriteBitmap);
	    pXAAinfo->TEGlyphRendererFlags = LEFT_EDGE_CLIPPING | NO_PLANEMASK |
			    LEFT_EDGE_CLIPPING_NEGATIVE_X;
	    XAA(TEGlyphRenderer);

	    /* Accelerated screen-screen bitblts */
	    pXAAinfo->ScreenToScreenCopyFlags = NO_PLANEMASK;
	    XAA(SetupForScreenToScreenCopy);
	    XAA(SubsequentScreenToScreenCopy);

	    /* Accelerated Line drawing */
	    pXAAinfo->SolidLineFlags = NO_PLANEMASK | HARDWARE_CLIP_LINE;
	    XAA(SetClippingRectangle);
	    pXAAinfo->SolidBresenhamLineErrorTermBits = 15;

	    if (pApm->Chipset >= AT24) {
		XAA(SubsequentSolidBresenhamLine);

		/* Pattern fill */
		pXAAinfo->Mono8x8PatternFillFlags = NO_PLANEMASK |
				HARDWARE_PATTERN_PROGRAMMED_BITS |
				HARDWARE_PATTERN_SCREEN_ORIGIN;
		XAA(SetupForMono8x8PatternFill);
		XAA(SubsequentMono8x8PatternFillRect);
		if (pApm->CurrentLayout.bitsPerPixel == 8) {
		    pXAAinfo->Color8x8PatternFillFlags = NO_PLANEMASK |
				    HARDWARE_PATTERN_SCREEN_ORIGIN;
		    XAA(SetupForColor8x8PatternFill);
		    XAA(SubsequentColor8x8PatternFillRect);
		}
	    }
	    else
		pXAAinfo->SubsequentSolidBresenhamLine =
		    ApmSubsequentSolidBresenhamLine6422_IOP;
#undef XAA
	}
    }
    else {
	if (!pApm->noLinear) {
#define	XAA(s)		pXAAinfo->s = Apm##s##24
	    XAA(Sync);

	    /* Accelerated filled rectangles */
	    pXAAinfo->SolidFillFlags = NO_PLANEMASK;
	    XAA(SetupForSolidFill);
	    XAA(SubsequentSolidFillRect);

#if 0
	    /* Accelerated screen to screen color expansion */
	    pXAAinfo->ScreenToScreenColorExpandFillFlags = NO_PLANEMASK;
	    XAA(SetupForScreenToScreenColorExpandFill);
	    XAA(SubsequentScreenToScreenColorExpandFill);

#if 0
	    The constraints of the transfer range are incompatible with the
	    XAA architecture. I rewrote the XAA functions using ImageWrite
	    /* Accelerated CPU to screen color expansion */
	    if (pApm->Chipset == AT3D && pApm->ChipRev >= 4) {
		pXAAinfo->CPUToScreenColorExpandFillFlags =
		  NO_PLANEMASK | SCANLINE_PAD_DWORD | CPU_TRANSFER_PAD_QWORD
		  | BIT_ORDER_IN_BYTE_MSBFIRST | LEFT_EDGE_CLIPPING |
		  LEFT_EDGE_CLIPPING_NEGATIVE_X | SYNC_AFTER_COLOR_EXPAND;
		XAA(SetupForCPUToScreenColorExpandFill);
		XAA(SubsequentCPUToScreenColorExpandFill);
		pXAAinfo->ColorExpandBase	= pApm->BltMap;
		pXAAinfo->ColorExpandRange	= 32*1024;
	    }

	    /* Accelerated image transfers */
	    pXAAinfo->ImageWriteFlags	=
			    LEFT_EDGE_CLIPPING | NO_PLANEMASK |
			    SCANLINE_PAD_DWORD | CPU_TRANSFER_PAD_QWORD |
			    SYNC_AFTER_IMAGE_WRITE;
	    pXAAinfo->ImageWriteBase	= pApm->BltMap;
	    pXAAinfo->ImageWriteRange	= 32*1024;
	    XAA(SetupForImageWrite);
	    XAA(SubsequentImageWriteRect);
#endif
	    pXAAinfo->WritePixmapFlags = LEFT_EDGE_CLIPPING | NO_PLANEMASK |
			    LEFT_EDGE_CLIPPING_NEGATIVE_X;
	    XAA(WritePixmap);
	    pXAAinfo->FillImageWriteRectsFlags	=
			    LEFT_EDGE_CLIPPING | NO_PLANEMASK |
			    LEFT_EDGE_CLIPPING_NEGATIVE_X;
	    XAA(FillImageWriteRects);
	    pXAAinfo->WriteBitmapFlags = LEFT_EDGE_CLIPPING | NO_PLANEMASK |
			    LEFT_EDGE_CLIPPING_NEGATIVE_X |
			    BIT_ORDER_IN_BYTE_LSBFIRST;
	    XAA(WriteBitmap);
	    pXAAinfo->TEGlyphRendererFlags = LEFT_EDGE_CLIPPING | NO_PLANEMASK |
			    LEFT_EDGE_CLIPPING_NEGATIVE_X;
	    XAA(TEGlyphRenderer);

	    /* Accelerated Line drawing */
	    pXAAinfo->SolidLineFlags = NO_PLANEMASK | HARDWARE_CLIP_LINE;
	    XAA(SubsequentSolidBresenhamLine);
	    XAA(SetClippingRectangle);
	    pXAAinfo->SolidBresenhamLineErrorTermBits = 15;

	    /* Pattern fill */
	    pXAAinfo->Mono8x8PatternFillFlags = NO_PLANEMASK | NO_TRANSPARENCY |
			    HARDWARE_PATTERN_PROGRAMMED_BITS |
			    HARDWARE_PATTERN_SCREEN_ORIGIN;
	    XAA(SetupForMono8x8PatternFill);
	    XAA(SubsequentMono8x8PatternFillRect);
#endif

	    /* Accelerated screen-screen bitblts */
	    pXAAinfo->ScreenToScreenCopyFlags = NO_PLANEMASK | NO_TRANSPARENCY;
	    XAA(SetupForScreenToScreenCopy);
	    XAA(SubsequentScreenToScreenCopy);
#undef XAA
	}
	else {
#define	XAA(s)		pXAAinfo->s = Apm##s##24##_IOP
	    XAA(Sync);

	    /* Accelerated filled rectangles */
	    pXAAinfo->SolidFillFlags = NO_PLANEMASK;
	    XAA(SetupForSolidFill);
	    XAA(SubsequentSolidFillRect);

#if 0
	    /* Accelerated screen to screen color expansion */
	    pXAAinfo->ScreenToScreenColorExpandFillFlags = NO_PLANEMASK;
	    XAA(SetupForScreenToScreenColorExpandFill);
	    XAA(SubsequentScreenToScreenColorExpandFill);

#if 0
	    The constraints of the transfer range are incompatible with the
	    XAA architecture. I rewrote the XAA functions using ImageWrite
	    /* Accelerated CPU to screen color expansion */
	    if (pApm->Chipset == AT3D && pApm->ChipRev >= 4) {
		pXAAinfo->CPUToScreenColorExpandFillFlags =
		  NO_PLANEMASK | SCANLINE_PAD_DWORD | CPU_TRANSFER_PAD_QWORD
		  | BIT_ORDER_IN_BYTE_MSBFIRST | LEFT_EDGE_CLIPPING |
		  LEFT_EDGE_CLIPPING_NEGATIVE_X | SYNC_AFTER_COLOR_EXPAND;
		XAA(SetupForCPUToScreenColorExpandFill);
		XAA(SubsequentCPUToScreenColorExpandFill);
		pXAAinfo->ColorExpandBase	= pApm->BltMap;
		pXAAinfo->ColorExpandRange	= 32*1024;
	    }

	    /* Accelerated image transfers */
	    pXAAinfo->ImageWriteFlags	=
			    LEFT_EDGE_CLIPPING | NO_PLANEMASK |
			    SCANLINE_PAD_DWORD | CPU_TRANSFER_PAD_QWORD;
	    pXAAinfo->ImageWriteBase	= pApm->BltMap;
	    pXAAinfo->ImageWriteRange	= 32*1024;
	    XAA(SetupForImageWrite);
	    XAA(SubsequentImageWriteRect);
#endif
	    pXAAinfo->WritePixmapFlags = LEFT_EDGE_CLIPPING | NO_PLANEMASK |
			    LEFT_EDGE_CLIPPING_NEGATIVE_X;
	    XAA(WritePixmap);
	    pXAAinfo->FillImageWriteRectsFlags	=
			    LEFT_EDGE_CLIPPING | NO_PLANEMASK |
			    LEFT_EDGE_CLIPPING_NEGATIVE_X;
	    XAA(FillImageWriteRects);
	    pXAAinfo->WriteBitmapFlags = LEFT_EDGE_CLIPPING | NO_PLANEMASK |
			    LEFT_EDGE_CLIPPING_NEGATIVE_X |
			    BIT_ORDER_IN_BYTE_LSBFIRST;
	    XAA(WriteBitmap);
	    pXAAinfo->TEGlyphRendererFlags = LEFT_EDGE_CLIPPING | NO_PLANEMASK |
			    LEFT_EDGE_CLIPPING_NEGATIVE_X;
	    XAA(TEGlyphRenderer);

	    /* Accelerated Line drawing */
	    pXAAinfo->SolidLineFlags = NO_PLANEMASK | HARDWARE_CLIP_LINE;
	    XAA(SubsequentSolidBresenhamLine);
	    XAA(SetClippingRectangle);
	    pXAAinfo->SolidBresenhamLineErrorTermBits = 15;

	    /* Pattern fill */
	    pXAAinfo->Mono8x8PatternFillFlags = NO_PLANEMASK | NO_TRANSPARENCY |
			    HARDWARE_PATTERN_PROGRAMMED_BITS |
			    HARDWARE_PATTERN_SCREEN_ORIGIN;
	    XAA(SetupForMono8x8PatternFill);
	    XAA(SubsequentMono8x8PatternFillRect);
#endif

	    /* Accelerated screen-screen bitblts */
	    pXAAinfo->ScreenToScreenCopyFlags = NO_PLANEMASK | NO_TRANSPARENCY;
	    XAA(SetupForScreenToScreenCopy);
	    XAA(SubsequentScreenToScreenCopy);
#undef XAA
	}
    }
}
