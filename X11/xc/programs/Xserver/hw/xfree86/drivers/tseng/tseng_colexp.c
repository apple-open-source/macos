/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/tseng/tseng_colexp.c,v 1.14 2001/02/15 17:54:55 eich Exp $ */





/*
 * ET4/6K acceleration interface -- color expansion primitives.
 *
 * Uses Harm Hanemaayer's generic acceleration interface (XAA).
 *
 * Author: Koen Gadeyne
 *
 * Much of the acceleration code is based on the XF86_W32 server code from
 * Glenn Lai.
 *
 *
 *     Color expansion capabilities of the Tseng chip families:
 *
 *     Chip     screen-to-screen   CPU-to-screen   Supported depths
 *
 *   ET4000W32/W32i   No               Yes             8bpp only
 *   ET4000W32p       Yes              Yes             8bpp only
 *   ET6000           Yes              No              8/16/24/32 bpp
 */

#include "tseng.h"
#include "tseng_acl.h"
#include "tseng_inline.h"

void TsengSetupForScreenToScreenColorExpandFill(ScrnInfoPtr pScrn,
    int fg, int bg, int rop, unsigned int planemask);

void TsengSubsequentScreenToScreenColorExpandFill(ScrnInfoPtr pScrn,
    int x, int y, int w, int h, int srcx, int srcy, int skipleft);

void TsengSubsequentScanlineCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
    int x, int y, int w, int h, int skipleft);

void TsengSubsequentColorExpandScanline(ScrnInfoPtr pScrn,
    int bufno);

void TsengSetupForCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
    int fg, int bg, int rop, unsigned int planemask);

void TsengSubsequentCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
    int x, int y, int w, int h, int skipleft);

void TsengSubsequentColorExpandScanline_8bpp(ScrnInfoPtr pScrn, int bufno);
void TsengSubsequentColorExpandScanline_16bpp(ScrnInfoPtr pScrn, int bufno);
void TsengSubsequentColorExpandScanline_24bpp(ScrnInfoPtr pScrn, int bufno);
void TsengSubsequentColorExpandScanline_32bpp(ScrnInfoPtr pScrn, int bufno);

Bool
TsengXAAInit_Colexp(ScrnInfoPtr pScrn)
{
    int i, j, r;
    TsengPtr pTseng = TsengPTR(pScrn);
    XAAInfoRecPtr pXAAInfo = pTseng->AccelInfoRec;

    PDEBUG("	TsengXAAInit_Colexp\n");

#ifdef TODO
    if (OFLG_ISSET(OPTION_XAA_NO_COL_EXP, &vga256InfoRec.options))
	return;
#endif

    /* FIXME! disable accelerated color expansion for W32/W32i until it's fixed */
/*  if (Is_W32 || Is_W32i) return; */

    /*
     * Screen-to-screen color expansion.
     *
     * Scanline-screen-to-screen color expansion is slower than
     * CPU-to-screen color expansion.
     */

    pXAAInfo->ScreenToScreenColorExpandFillFlags =
	BIT_ORDER_IN_BYTE_LSBFIRST |
	SCANLINE_PAD_DWORD |
	LEFT_EDGE_CLIPPING |
	NO_PLANEMASK;

#if 1
    if (Is_ET6K || (Is_W32p && (pScrn->bitsPerPixel == 8))) {
	pXAAInfo->SetupForScreenToScreenColorExpandFill =
	    TsengSetupForScreenToScreenColorExpandFill;
	pXAAInfo->SubsequentScreenToScreenColorExpandFill =
	    TsengSubsequentScreenToScreenColorExpandFill;
    }
#endif

    /*
     * Scanline CPU to screen color expansion for all W32 engines.
     *
     * real CPU-to-screen color expansion is extremely tricky, and only
     * works for 8bpp anyway.
     *
     * This also allows us to do 16, 24 and 32 bpp color expansion by first
     * doubling the bitmap pattern before color-expanding it, because W32s
     * can only do 8bpp color expansion.
     */

    pXAAInfo->ScanlineCPUToScreenColorExpandFillFlags =
	BIT_ORDER_IN_BYTE_LSBFIRST |
	SCANLINE_PAD_DWORD |
	NO_PLANEMASK;

#if 1
    if (!Is_ET6K) {
	pTseng->XAAScanlineColorExpandBuffers[0] =
	    xnfalloc(((pScrn->virtualX + 31)/32) * 4 * pTseng->Bytesperpixel);
	if (pTseng->XAAScanlineColorExpandBuffers[0] == NULL) {
	    xf86Msg(X_ERROR, "Could not malloc color expansion scanline buffer.\n");
	    return FALSE;
	}
	pXAAInfo->NumScanlineColorExpandBuffers = 1;
	pXAAInfo->ScanlineColorExpandBuffers = pTseng->XAAScanlineColorExpandBuffers;

	pXAAInfo->SetupForScanlineCPUToScreenColorExpandFill =
	    TsengSetupForCPUToScreenColorExpandFill;

	pXAAInfo->SubsequentScanlineCPUToScreenColorExpandFill =
	    TsengSubsequentScanlineCPUToScreenColorExpandFill;

	switch (pScrn->bitsPerPixel) {
	case 8:
	    pXAAInfo->SubsequentColorExpandScanline =
		TsengSubsequentColorExpandScanline_8bpp;
	    break;
	case 15:
	case 16:
	    pXAAInfo->SubsequentColorExpandScanline =
		TsengSubsequentColorExpandScanline_16bpp;
	    break;
	case 24:
	    pXAAInfo->SubsequentColorExpandScanline =
		TsengSubsequentColorExpandScanline_24bpp;
	    break;
	case 32:
	    pXAAInfo->SubsequentColorExpandScanline =
		TsengSubsequentColorExpandScanline_32bpp;
	    break;
	}
	/* create color expansion LUT (used for >8bpp only) */
	pTseng->ColExpLUT = xnfalloc(sizeof(CARD32)*256);
	if (pTseng->ColExpLUT == NULL) {
	    xf86Msg(X_ERROR, "Could not malloc color expansion tables.\n");
	    return FALSE;
	}
	for (i = 0; i < 256; i++) {
	    r = 0;
	    for (j = 7; j >= 0; j--) {
		r <<= pTseng->Bytesperpixel;
		if ((i >> j) & 1)
		    r |= (1 << pTseng->Bytesperpixel) - 1;
	    }
	    pTseng->ColExpLUT[i] = r;
	    /* ErrorF("0x%08X, ",r ); if ((i%8)==7) ErrorF("\n"); */
	}
    }
#endif
#if 1
    if (Is_ET6K) {
	/*
	 * Triple-buffering is needed to account for double-buffering of Tseng
	 * acceleration registers.
	 */
	pXAAInfo->NumScanlineColorExpandBuffers = 3;
	pXAAInfo->ScanlineColorExpandBuffers =
	    pTseng->XAAColorExpandBuffers;
	pXAAInfo->SetupForScanlineCPUToScreenColorExpandFill =
	    TsengSetupForScreenToScreenColorExpandFill;
	pXAAInfo->SubsequentScanlineCPUToScreenColorExpandFill =
	    TsengSubsequentScanlineCPUToScreenColorExpandFill;
	pXAAInfo->SubsequentColorExpandScanline =
	    TsengSubsequentColorExpandScanline;

	/* calculate memory addresses from video memory offsets */
	for (i = 0; i < pXAAInfo->NumScanlineColorExpandBuffers; i++) {
	    pTseng->XAAColorExpandBuffers[i] =
		pTseng->FbBase + pTseng->AccelColorExpandBufferOffsets[i];
	}

	/*
	 * for banked memory, translate those addresses to fall in the
	 * correct aperture. Color expansion uses aperture #0, which sits at
	 * pTseng->FbBase + 0x18000 + 48.
	 */
	if (!pTseng->UseLinMem) {
	    for (i = 0; i < pXAAInfo->NumScanlineColorExpandBuffers; i++) {
		pTseng->XAAColorExpandBuffers[i] =
		    pTseng->XAAColorExpandBuffers[i]
		    - pTseng->AccelColorExpandBufferOffsets[0]
		    + 0x18000 + 48;
	    }
	}
	pXAAInfo->ScanlineColorExpandBuffers = pTseng->XAAColorExpandBuffers;
    }
#endif

#ifdef TSENG_CPU_TO_SCREEN_COLOREXPAND
    /*
     * CPU-to-screen color expansion doesn't seem to be reliable yet. The
     * W32 needs the correct amount of data sent to it in this mode, or it
     * hangs the machine until is does (?). Currently, the init code in this
     * file or the XAA code that uses this does something wrong, so that
     * occasionally we get accelerator timeouts, and after a few, complete
     * system hangs.
     *
     * The W32 engine requires SCANLINE_NO_PAD, but that doesn't seem to
     * work very well (accelerator hangs).
     *
     * What works is this: tell XAA that we have SCANLINE_PAD_DWORD, and then
     * add the following code in TsengSubsequentCPUToScreenColorExpand():
     *     w = (w + 31) & ~31; this code rounds the width up to the nearest
     * multiple of 32, and together with SCANLINE_PAD_DWORD, this makes
     * CPU-to-screen color expansion work. Of course, the display isn't
     * correct (4 chars are "blanked out" when only one is written, for
     * example). But this shows that the principle works. But the code
     * doesn't...
     *
     * The same thing goes for PAD_BYTE: this also works (with the same
     * problems as SCANLINE_PAD_DWORD, although less prominent)
     */

    pXAAInfo->CPUToScreenColorExpandFillFlags =
	BIT_ORDER_IN_BYTE_LSBFIRST |
	SCANLINE_PAD_DWORD |   /* no other choice */
	CPU_TRANSFER_PAD_DWORD |
	NO_PLANEMASK;

    if (Is_W32_any && (pScrn->bitsPerPixel == 8)) {
	pXAAInfo->SetupForCPUToScreenColorExpandFill =
	    TsengSetupForCPUToScreenColorExpandFill;
	pXAAInfo->SubsequentCPUToScreenColorExpandFill =
	    TsengSubsequentCPUToScreenColorExpandFill;

	/* we'll be using MMU aperture 2 */
	pXAAInfo->ColorExpandBase = (CARD8 *)pTseng->tsengCPU2ACLBase;
	/* ErrorF("tsengCPU2ACLBase = 0x%x\n", pTseng->tsengCPU2ACLBase); */
	/* aperture size is 8kb in banked mode. Larger in linear mode, but 8kb is enough */
	pXAAInfo->ColorExpandRange = 8192;
    }
#endif
    return TRUE;
}

#define SET_FUNCTION_COLOREXPAND \
    if (Is_ET6K) \
      ACL_MIX_CONTROL(0x32); \
    else \
      ACL_ROUTING_CONTROL(0x08);

#define SET_FUNCTION_COLOREXPAND_CPU \
    ACL_ROUTING_CONTROL(0x02);


void
TsengSubsequentScanlineCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
    int x, int y, int w, int h, int skipleft)
{
    TsengPtr pTseng = TsengPTR(pScrn);

    if (!Is_ET6K) {
	/* the accelerator needs DWORD padding, and "w" is in PIXELS... */
	pTseng->acl_colexp_width_dwords = (MULBPP(pTseng, w) + 31) >> 5;
	pTseng->acl_colexp_width_bytes = (MULBPP(pTseng, w) + 7) >> 3;
    }

    pTseng->acl_ColorExpandDst = FBADDR(pTseng, x, y);
    pTseng->acl_skipleft = skipleft;

    wait_acl_queue(pTseng);

#if 0
    ACL_MIX_Y_OFFSET(w - 1);

    ErrorF(" W=%d", w);
#endif
    SET_XY(pTseng, w, 1);
}

void
TsengSubsequentColorExpandScanline(ScrnInfoPtr pScrn,
    int bufno)
{
    TsengPtr pTseng = TsengPTR(pScrn);

    wait_acl_queue(pTseng);

    ACL_MIX_ADDRESS((pTseng->AccelColorExpandBufferOffsets[bufno] << 3) + pTseng->acl_skipleft);
    START_ACL(pTseng, pTseng->acl_ColorExpandDst);

    /* move to next scanline */
    pTseng->acl_ColorExpandDst += pTseng->line_width;

    /*
     * If not using triple-buffering, we need to wait for the queued
     * register set to be transferred to the working register set here,
     * because otherwise an e.g. double-buffering mechanism could overwrite
     * the buffer that's currently being worked with with new data too soon.
     *
     * WAIT_QUEUE; // not needed with triple-buffering
     */
}



/*
 * We use this intermediate CPU-to-Screen color expansion because the one
 * provided by XAA seems to lock up the accelerator engine.
 *
 * One of the main differences between the XAA approach and this one is that
 * transfers are done per byte. I'm not sure if that is needed though.
 */

void TsengSubsequentColorExpandScanline_8bpp(ScrnInfoPtr pScrn, int bufno)
{
    TsengPtr pTseng = TsengPTR(pScrn);
    pointer dest = pTseng->tsengCPU2ACLBase;
    int i,j;
    CARD8 *bufptr;

    i = pTseng->acl_colexp_width_bytes;
    bufptr = (CARD8 *) (pTseng->XAAScanlineColorExpandBuffers[bufno]);

    wait_acl_queue(pTseng);
    START_ACL (pTseng, pTseng->acl_ColorExpandDst);

/*  *((LongP) (MMioBase + 0x08)) = (CARD32) pTseng->acl_ColorExpandDst;*/
/*  MMIO_OUT32(tsengCPU2ACLBase,0, (CARD32)pTseng->acl_ColorExpandDst); */
    j = 0;
    /* Copy scanline data to accelerator MMU aperture byte by byte */
    while (i--) {		       /* FIXME: we need to take care of PCI bursting and MMU overflow here! */
	MMIO_OUT8(dest,j++, *bufptr++);
    }

    /* move to next scanline */
    pTseng->acl_ColorExpandDst += pTseng->line_width;
}

/*
 * This function does direct memory-to-CPU bit doubling for color-expansion
 * at 16bpp on W32 chips. They can only do 8bpp color expansion, so we have
 * to expand the incoming data to 2bpp first.
 */

void TsengSubsequentColorExpandScanline_16bpp(ScrnInfoPtr pScrn, int bufno)
{
    TsengPtr pTseng = TsengPTR(pScrn);
    pointer dest = pTseng->tsengCPU2ACLBase;
    int i,j;
    CARD8 *bufptr;
    register CARD32 bits16;
    
    i = pTseng->acl_colexp_width_dwords * 2;
    bufptr = (CARD8 *) (pTseng->XAAScanlineColorExpandBuffers[bufno]);
    
    wait_acl_queue(pTseng);
    START_ACL(pTseng, pTseng->acl_ColorExpandDst);

    j = 0;
    while (i--) {
	bits16 = pTseng->ColExpLUT[*bufptr++];
	MMIO_OUT8(dest,j++,bits16 & 0xFF);
	MMIO_OUT8(dest,j++,(bits16 >> 8) & 0xFF);
    }

    /* move to next scanline */
    pTseng->acl_ColorExpandDst += pTseng->line_width;
}

/*
 * This function does direct memory-to-CPU bit doubling for color-expansion
 * at 24bpp on W32 chips. They can only do 8bpp color expansion, so we have
 * to expand the incoming data to 3bpp first.
 */

void TsengSubsequentColorExpandScanline_24bpp(ScrnInfoPtr pScrn, int bufno)
{
    TsengPtr pTseng = TsengPTR(pScrn);
    pointer dest = pTseng->tsengCPU2ACLBase;
    int i, k, j = -1;
    CARD8 *bufptr;
    register CARD32 bits24;

    i = pTseng->acl_colexp_width_dwords * 4;
    bufptr = (CARD8 *) (pTseng->XAAScanlineColorExpandBuffers[bufno]);

    wait_acl_queue(pTseng);
    START_ACL(pTseng, pTseng->acl_ColorExpandDst);

    /* take 8 input bits, expand to 3 output bytes */
    bits24 = pTseng->ColExpLUT[*bufptr++];
    k = 0;
    while (i--) {
	if ((j++) == 2) {	       /* "i % 3" operation is much to expensive */
	    j = 0;
	    bits24 = pTseng->ColExpLUT[*bufptr++];
	}
	MMIO_OUT8(dest,k++,bits24 & 0xFF);
	bits24 >>= 8;
    }

    /* move to next scanline */
    pTseng->acl_ColorExpandDst += pTseng->line_width;
}

/*
 * This function does direct memory-to-CPU bit doubling for color-expansion
 * at 32bpp on W32 chips. They can only do 8bpp color expansion, so we have
 * to expand the incoming data to 4bpp first.
 */

void TsengSubsequentColorExpandScanline_32bpp(ScrnInfoPtr pScrn, int bufno)
{
    TsengPtr pTseng = TsengPTR(pScrn);
    pointer dest = pTseng->tsengCPU2ACLBase;
    int i,j;
    CARD8 *bufptr;
    register CARD32 bits32;

    i = pTseng->acl_colexp_width_dwords;
   /* amount of blocks of 8 bits to expand to 32 bits (=1 DWORD) */
    bufptr = (CARD8 *) (pTseng->XAAScanlineColorExpandBuffers[bufno]);

    wait_acl_queue(pTseng);
    START_ACL(pTseng, pTseng->acl_ColorExpandDst);

    j = 0;
    while (i--) {
	bits32 = pTseng->ColExpLUT[*bufptr++];
	MMIO_OUT8(dest,j++,bits32 & 0xFF);
	MMIO_OUT8(dest,j++,(bits32 >> 8) & 0xFF);
	MMIO_OUT8(dest,j++,(bits32 >> 16) & 0xFF);
	MMIO_OUT8(dest,j++,(bits32 >> 24) & 0xFF);
    }

    /* move to next scanline */
    pTseng->acl_ColorExpandDst += pTseng->line_width;
}

/*
 * CPU-to-Screen color expansion.
 *   This is for ET4000 only (The ET6000 cannot do this)
 */

void TsengSetupForCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
    int fg, int bg, int rop, unsigned int planemask)
{
    TsengPtr pTseng = TsengPTR(pScrn);

/*  ErrorF("X"); */

    PINGPONG(pTseng);

    wait_acl_queue(pTseng);

    SET_FG_ROP(rop);
    SET_BG_ROP_TR(rop, bg);

    SET_XYDIR(0);

    SET_FG_BG_COLOR(pTseng, fg, bg);

    SET_FUNCTION_COLOREXPAND_CPU;

    /* assure correct alignment of MIX address (ACL needs same alignment here as in MMU aperture) */
    ACL_MIX_ADDRESS(0);
}

/*
 * TsengSubsequentCPUToScreenColorExpand() is potentially dangerous:
 *   Not writing enough data to the MMU aperture for CPU-to-screen color
 *   expansion will eventually cause a system deadlock!
 *
 * Note that CPUToScreenColorExpand operations _always_ require a
 * WAIT_INTERFACE before starting a new operation (this is empyrical,
 * though)
 */

void TsengSubsequentCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
    int x, int y, int w, int h, int skipleft)
{
    TsengPtr pTseng = TsengPTR(pScrn);
    int destaddr = FBADDR(pTseng, x, y);

    /* ErrorF(" %dx%d|%d ",w,h,skipleft); */
    if (skipleft)
	ErrorF("Can't do: Skipleft = %d\n", skipleft);

/*  wait_acl_queue(); */
    ErrorF("=========WAIT     FIXME!\n");
    WAIT_INTERFACE;

    ACL_MIX_Y_OFFSET(w - 1);
    SET_XY(pTseng, w, h);
    START_ACL(pTseng, destaddr);
}


void
TsengSetupForScreenToScreenColorExpandFill(ScrnInfoPtr pScrn,
    int fg, int bg, int rop, unsigned int planemask)
{
    TsengPtr pTseng = TsengPTR(pScrn);

/*  ErrorF("SSC "); */

    PINGPONG(pTseng);

    wait_acl_queue(pTseng);

    SET_FG_ROP(rop);
    SET_BG_ROP_TR(rop, bg);

    SET_FG_BG_COLOR(pTseng, fg, bg);

    SET_FUNCTION_COLOREXPAND;

    SET_XYDIR(0);
}

void
TsengSubsequentScreenToScreenColorExpandFill(ScrnInfoPtr pScrn,
    int x, int y, int w, int h, int srcx, int srcy, int skipleft)
{
    TsengPtr pTseng = TsengPTR(pScrn);
    int destaddr = FBADDR(pTseng, x, y);

/*    int srcaddr = FBADDR(pTseng, srcx, srcy); */

    wait_acl_queue(pTseng);

    SET_XY(pTseng, w, h);
    ACL_MIX_ADDRESS(		       /* MIX address is in BITS */
	(((srcy * pScrn->displayWidth) + srcx) * pScrn->bitsPerPixel) + skipleft);

    ACL_MIX_Y_OFFSET(pTseng->line_width << 3);

    START_ACL(pTseng, destaddr);
}
