/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/tseng/tseng_accel.c,v 1.33 2001/02/15 17:54:55 eich Exp $ */

/*
 * ET4/6K acceleration interface.
 *
 * Uses Harm Hanemaayer's generic acceleration interface (XAA).
 *
 * Author: Koen Gadeyne
 *
 * Much of the acceleration code is based on the XF86_W32 server code from
 * Glenn Lai.
 *
 */

/*
 * if NO_OPTIMIZE is set, some optimizations are disabled.
 *
 * What it basically tries to do is minimize the amounts of writes to
 * accelerator registers, since these are the ones that slow down small
 * operations a lot.
 */

#define NO_OPTIMIZE

/*
 * if ET6K_TRANSPARENCY is set, ScreentoScreenCopy operations (and pattern
 * fills) will support transparency. But then the planemask support has to
 * be dropped. The default here is to support planemasks, because all Tseng
 * chips can do this. Only the ET6000 supports a transparency compare. The
 * code could be easily changed to support transparency on the ET6000 and
 * planemasks on the others, but that's only useful when transparency is
 * more important than planemasks.
 */

#undef ET6K_TRANSPARENCY

#include "tseng.h"
#include "tseng_acl.h"
#include "tseng_inline.h"

#include "miline.h"

void TsengSync(ScrnInfoPtr pScrn);

void TsengSetupForSolidFill(ScrnInfoPtr pScrn,
    int color, int rop, unsigned int planemask);
void TsengW32iSubsequentSolidFillRect(ScrnInfoPtr pScrn,
    int x, int y, int w, int h);
void TsengW32pSubsequentSolidFillRect(ScrnInfoPtr pScrn,
    int x, int y, int w, int h);
void Tseng6KSubsequentSolidFillRect(ScrnInfoPtr pScrn,
    int x, int y, int w, int h);

/* void TsengSubsequentFillTrapezoidSolid(); */

void TsengSetupForScreenToScreenCopy(ScrnInfoPtr pScrn,
    int xdir, int ydir, int rop,
    unsigned int planemask, int trans_color);
void TsengSubsequentScreenToScreenCopy(ScrnInfoPtr pScrn,
    int x1, int y1, int x2, int y2, int w, int h);

void TsengSetupForColor8x8PatternFill(ScrnInfoPtr pScrn,
    int patx, int paty, int rop, unsigned int planemask, int trans_color);

void TsengSubsequentColor8x8PatternFillRect(ScrnInfoPtr pScrn,
    int patx, int paty, int x, int y, int w, int h);

void TsengSetupForScanlineImageWrite(ScrnInfoPtr pScrn,
    int rop, unsigned int planemask, int trans_color, int bpp, int depth);

void TsengSubsequentScanlineImageWriteRect(ScrnInfoPtr pScrn,
    int x, int y, int w, int h, int skipleft);

void TsengSubsequentImageWriteScanline(ScrnInfoPtr pScrn,
    int bufno);

#if 0
void TsengSetupForSolidLine(ScrnInfoPtr pScrn,
    int color, int rop, unsigned int planemask);
#endif

void TsengSubsequentSolidBresenhamLine(ScrnInfoPtr pScrn,
        int x, int y, int major, int minor, int err, int len, int octant);


/*
 * The following function sets up the supported acceleration. Call it from
 * the FbInit() function in the SVGA driver. Do NOT initialize any hardware
 * in here. That belongs in tseng_init_acl().
 */
Bool
TsengXAAInit(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    TsengPtr pTseng = TsengPTR(pScrn);
    XAAInfoRecPtr pXAAinfo;
    BoxRec AvailFBArea;

    PDEBUG("	TsengXAAInit\n");
    pTseng->AccelInfoRec = pXAAinfo = XAACreateInfoRec();
    if (!pXAAinfo)
	return FALSE;

    /*
     * Set up the main acceleration flags.
     */
    pXAAinfo->Flags = PIXMAP_CACHE;

#ifdef TODO
    if (Tseng_bus != T_BUS_PCI)
	pXAAinfo->Flags |= COP_FRAMEBUFFER_CONCURRENCY;
#endif

    /*
     * The following line installs a "Sync" function, that waits for
     * all coprocessor operations to complete.
     */
    pXAAinfo->Sync = TsengSync;

    /* W32 and W32i must wait for ACL before changing registers */
    pTseng->need_wait_acl = (Is_W32_W32i || Is_W32p);

    pTseng->line_width = pScrn->displayWidth * pTseng->Bytesperpixel;

#if 1
    /*
     * SolidFillRect.
     *
     * The W32 and W32i chips don't have a register to set the amount of
     * bytes per pixel, and hence they don't skip 1 byte in each 4-byte word
     * at 24bpp. Therefor, the FG or BG colors would have to be concatenated
     * in video memory (R-G-B-R-G-B-... instead of R-G-B-X-R-G-B-X-..., with
     * X = dont' care), plus a wrap value that is a multiple of 3 would have
     * to be set. There is no such wrap combination available.
     */
#ifdef OBSOLETE
    pXAAinfo->SolidFillFlags |= NO_PLANEMASK;
#endif

    if (!(Is_W32_W32i && (pScrn->bitsPerPixel == 24))) {
	pXAAinfo->SetupForSolidFill = TsengSetupForSolidFill;
	if (Is_ET6K) {
	    pXAAinfo->SubsequentSolidFillRect = Tseng6KSubsequentSolidFillRect;
	} else if (Is_W32p)
	    pXAAinfo->SubsequentSolidFillRect = TsengW32pSubsequentSolidFillRect;
	else			       /* W32, W32i */
	    pXAAinfo->SubsequentSolidFillRect = TsengW32iSubsequentSolidFillRect;
    }
#ifdef TSENG_TRAPEZOIDS
    if (Is_ET6K) {
	/* disabled for now: not fully compliant yet */
	pXAAinfo->SubsequentFillTrapezoidSolid = TsengSubsequentFillTrapezoidSolid;
    }
#endif
#endif

#if 1
    /*
     * SceenToScreenCopy (BitBLT).
     * 
     * Restrictions: On ET6000, we support EITHER a planemask OR
     * TRANSPARENCY, but not both (they use the same Pattern map).
     * All other chips can't do TRANSPARENCY at all.
     */
#ifdef ET6K_TRANSPARENCY
    pXAAinfo->CopyAreaFlags = NO_PLANEMASK;
    if (!Is_ET6K) {
	pXAAinfo->CopyAreaFlags |= NO_TRANSPARENCY;
    }
#else
    pXAAinfo->CopyAreaFlags = NO_TRANSPARENCY;
#endif

    pXAAinfo->SetupForScreenToScreenCopy =
	TsengSetupForScreenToScreenCopy;
    pXAAinfo->SubsequentScreenToScreenCopy =
	TsengSubsequentScreenToScreenCopy;
#endif

#if 0
    /*
     * ImageWrite.
     *
     * SInce this uses off-screen scanline buffers, it is only of use when
     * complex ROPs are used. But since the current XAA pixmap cache code
     * only works when an ImageWrite is provided, the NO_GXCOPY flag is
     * temporarily disabled.
     */

    if (pTseng->AccelImageWriteBufferOffsets[0]) {
	pXAAinfo->ScanlineImageWriteFlags =
	    pXAAinfo->CopyAreaFlags | LEFT_EDGE_CLIPPING /* | NO_GXCOPY */ ;
	pXAAinfo->NumScanlineImageWriteBuffers = 2;
	pXAAinfo->SetupForScanlineImageWrite =
	    TsengSetupForScanlineImageWrite;
	pXAAinfo->SubsequentScanlineImageWriteRect =
	    TsengSubsequentScanlineImageWriteRect;
	pXAAinfo->SubsequentImageWriteScanline =
	    TsengSubsequentImageWriteScanline;

	/* calculate memory addresses from video memory offsets */
	for (i = 0; i < pXAAinfo->NumScanlineImageWriteBuffers; i++) {
	    pTseng->XAAScanlineImageWriteBuffers[i] =
		pTseng->FbBase + pTseng->AccelImageWriteBufferOffsets[i];
	}

	/*
	 * for banked memory, translate those addresses to fall in the
	 * correct aperture. Imagewrite uses aperture #1, which sits at
	 * pTseng->FbBase + 0x1A000.
	 */
	if (!pTseng->UseLinMem) {
	    for (i = 0; i < pXAAinfo->NumScanlineImageWriteBuffers; i++) {
		pTseng->XAAScanlineImageWriteBuffers[i] =
		    pTseng->XAAScanlineImageWriteBuffers[i]
		    - pTseng->AccelImageWriteBufferOffsets[0]
		    + 0x1A000;
	    }
	}
	pXAAinfo->ScanlineImageWriteBuffers = pTseng->XAAScanlineImageWriteBuffers;
    }
#endif
    /*
     * 8x8 pattern tiling not possible on W32/i/p chips in 24bpp mode.
     * Currently, 24bpp pattern tiling doesn't work at all on those.
     *
     * FIXME: On W32 cards, pattern tiling doesn't work as expected.
     */
    pXAAinfo->Color8x8PatternFillFlags = HARDWARE_PATTERN_PROGRAMMED_ORIGIN;

    pXAAinfo->CachePixelGranularity = 8 * 8;

#ifdef ET6K_TRANSPARENCY
    pXAAinfo->PatternFlags |= HARDWARE_PATTERN_NO_PLANEMASK;
    if (Is_ET6K) {
	pXAAinfo->PatternFlags |= HARDWARE_PATTERN_TRANSPARENCY;
    }
#endif

#if 0
    /* FIXME! This needs to be fixed for W32 and W32i (it "should work") */
    if ((pScrn->bitsPerPixel != 24) && (Is_W32p || Is_ET6K)) {
	pXAAinfo->SetupForColor8x8PatternFill =
	    TsengSetupForColor8x8PatternFill;
	pXAAinfo->SubsequentColor8x8PatternFillRect =
	    TsengSubsequentColor8x8PatternFillRect;
    }
#endif

#if 0 /*1*/
    /*
     * SolidLine.
     *
     * We use Bresenham by preference, because it supports hardware clipping
     * (using the error term). TwoPointLines() is implemented, but not used,
     * because clipped lines are not accelerated (hardware clipping support
     * is lacking)...
     */

    if (Is_W32p || Is_ET6K) {
	/*
	 * Fill in the hardware linedraw ACL_XY_DIRECTION table
	 *
	 * W32BresTable[] converts XAA interface Bresenham octants to direct
	 * ACL direction register contents. This includes the correct bias
	 * setting etc.
	 *
	 * According to miline.h (but with base 0 instead of base 1 as in
	 * miline.h), the octants are numbered as follows:
	 *
	 *   \    |    /
	 *    \ 2 | 1 /
	 *     \  |  /
	 *    3 \ | / 0
	 *       \|/
	 *   -----------
	 *       /|\
	 *    4 / | \ 7
	 *     /  |  \
	 *    / 5 | 6 \
	 *   /    |    \
	 *
	 * In ACL_XY_DIRECTION, bits 2:0 are defined as follows:
	 *	0: '1' if XDECREASING
	 *	1: '1' if YDECREASING
	 *	2: '1' if XMAJOR (== not YMAJOR)
	 *
	 * Bit 4 defines the bias.  It should be set to '1' for all octants
	 * NOT passed to miSetZeroLineBias(). i.e. the inverse of the X bias.
	 *
	 * (For MS compatible bias, the data book says to set to the same as
	 * YDIR, i.e. bit 1 of the same register, = '1' if YDECREASING. MS
	 * bias is towards octants 0..3 (i.e. Y decreasing), hence this
	 * definition of bit 4)
	 *
	 */
	pTseng->BresenhamTable = xnfalloc(8);
	if (pTseng->BresenhamTable == NULL) {
	    xf86Msg(X_ERROR, "Could not malloc Bresenham Table.\n");
	    return FALSE;
	}
	for (i=0; i<8; i++) {
	    unsigned char zerolinebias = miGetZeroLineBias(pScreen);
	    pTseng->BresenhamTable[i] = 0xA0; /* command=linedraw, use error term */
	    if (i & XDECREASING) pTseng->BresenhamTable[i] |= 0x01;
	    if (i & YDECREASING) pTseng->BresenhamTable[i] |= 0x02;
	    if (!(i & YMAJOR))   pTseng->BresenhamTable[i] |= 0x04;
	    if ((1 << i) & zerolinebias) pTseng->BresenhamTable[i] |= 0x10;
	    /* ErrorF("BresenhamTable[%d]=0x%x\n", i, pTseng->BresenhamTable[i]); */
	} 

	pXAAinfo->SolidLineFlags = 0;
	pXAAinfo->SetupForSolidLine = TsengSetupForSolidFill;
	pXAAinfo->SubsequentSolidBresenhamLine =
	    TsengSubsequentSolidBresenhamLine;
	/*
	 * ErrorTermBits is used to limit minor, major and error term, so it
	 * must be min(errorterm_size, delta_major_size, delta_minor_size)
	 * But the calculation for major and minor is done on the DOUBLED
	 * values (as per the Bresenham algorithm), so they can also have 13
	 * bits (inside XAA). They are divided by 2 in this driver, so they
	 * are then again limited to 12 bits.
	 */
	pXAAinfo->SolidBresenhamLineErrorTermBits = 13;
    }
#endif

#if 1
    /* set up color expansion acceleration */
    if (!TsengXAAInit_Colexp(pScrn))
	return FALSE;
#endif


    /*
     * For Tseng, we set up some often-used values
     */

    switch (pTseng->Bytesperpixel) {   /* for MULBPP optimization */
    case 1:
	pTseng->powerPerPixel = 0;
	pTseng->planemask_mask = 0x000000FF;
	pTseng->neg_x_pixel_offset = 0;
	break;
    case 2:
	pTseng->powerPerPixel = 1;
	pTseng->planemask_mask = 0x0000FFFF;
	pTseng->neg_x_pixel_offset = 1;
	break;
    case 3:
	pTseng->powerPerPixel = 1;
	pTseng->planemask_mask = 0x00FFFFFF;
	pTseng->neg_x_pixel_offset = 2;		/* is this correct ??? */
	break;
    case 4:
	pTseng->powerPerPixel = 2;
	pTseng->planemask_mask = 0xFFFFFFFF;
	pTseng->neg_x_pixel_offset = 3;
	break;
    }

    /*
     * Init ping-pong registers.
     * This might be obsoleted by the BACKGROUND_OPERATIONS flag.
     */
    pTseng->tsengFg = 0;
    pTseng->tsengBg = 16;
    pTseng->tsengPat = 32;

    /* for register write optimisation */
    pTseng->tseng_old_dir = -1;
    pTseng->old_x = 0;
    pTseng->old_y = 0;

    /*
     * Finally, we set up the video memory space available to the pixmap
     * cache. In this case, all memory from the end of the virtual screen to
     * the end of video memory minus 1K (which we already reserved), can be
     * used.
     */

    AvailFBArea.x1 = 0;
    AvailFBArea.y1 = 0;
    AvailFBArea.x2 = pScrn->displayWidth;
    AvailFBArea.y2 = (pScrn->videoRam * 1024) /
	(pScrn->displayWidth * pTseng->Bytesperpixel);

    xf86InitFBManager(pScreen, &AvailFBArea);

    return (XAAInit(pScreen, pXAAinfo));

}

/*
 * This is the implementation of the Sync() function.
 *
 * To avoid pipeline/cache/buffer flushing in the PCI subsystem and the VGA
 * controller, we might replace this read-intensive code with a dummy
 * accelerator operation that causes a hardware-blocking (wait-states) until
 * the running operation is done.
 */
void
TsengSync(ScrnInfoPtr pScrn)
{
    TsengPtr pTseng = TsengPTR(pScrn);

    WAIT_ACL;
}

/*
 * This is the implementation of the SetupForSolidFill function
 * that sets up the coprocessor for a subsequent batch for solid
 * rectangle fills.
 */
void
TsengSetupForSolidFill(ScrnInfoPtr pScrn,
    int color, int rop, unsigned int planemask)
{
    TsengPtr pTseng = TsengPTR(pScrn);

    /*
     * all registers are queued in the Tseng chips, except of course for the
     * stuff we want to store in off-screen memory. So we have to use a
     * ping-pong method for those if we want to avoid having to wait for the
     * accelerator when we want to write to these.
     */

/*    ErrorF("S"); */

    PINGPONG(pTseng);

    wait_acl_queue(pTseng);

    /*
     * planemask emulation uses a modified "standard" FG ROP (see ET6000
     * data book p 66 or W32p databook p 37: "Bit masking"). We only enable
     * the planemask emulation when the planemask is not a no-op, because
     * blitting speed would suffer.
     */

    if ((planemask & pTseng->planemask_mask) != pTseng->planemask_mask) {
	SET_FG_ROP_PLANEMASK(rop);
	SET_BG_COLOR(pTseng, planemask);
    } else {
	SET_FG_ROP(rop);
    }
    SET_FG_COLOR(pTseng, color);

    SET_FUNCTION_BLT;
}

/*
 * This is the implementation of the SubsequentForSolidFillRect function
 * that sends commands to the coprocessor to fill a solid rectangle of
 * the specified location and size, with the parameters from the SetUp
 * call.
 *
 * Splitting it up between ET4000 and ET6000 avoids lots of chipset type
 * comparisons.
 */
void
TsengW32pSubsequentSolidFillRect(ScrnInfoPtr pScrn,
    int x, int y, int w, int h)
{
    TsengPtr pTseng = TsengPTR(pScrn);
    int destaddr = FBADDR(pTseng, x, y);

    wait_acl_queue(pTseng);

    /* 
     * Restoring the ACL_SOURCE_ADDRESS here is needed as long as Bresenham
     * lines are enabled for >8bpp. Or until XAA allows us to render
     * horizontal lines using the same Bresenham code instead of re-routing
     * them to FillRectSolid. For XDECREASING lines, the SubsequentBresenham
     * code adjusts the ACL_SOURCE_ADDRESS to make sure XDECREASING lines
     * are drawn with the correct colors. But if a batch of subsequent
     * operations also holds a few horizontal lines, they will be routed to
     * here without calling the SetupFor... code again, and the
     * ACL_SOURCE_ADDRESS will be wrong.
     */
    ACL_SOURCE_ADDRESS(pTseng->AccelColorBufferOffset + pTseng->tsengFg);

    SET_XYDIR(0);   /* FIXME: not needed with separate setupforsolidline */

    SET_XY_4(pTseng, w, h);
    START_ACL(pTseng, destaddr);
}

void
TsengW32iSubsequentSolidFillRect(ScrnInfoPtr pScrn,
    int x, int y, int w, int h)
{
    TsengPtr pTseng = TsengPTR(pScrn);
    int destaddr = FBADDR(pTseng, x, y);

    wait_acl_queue(pTseng);

    SET_XYDIR(0);

    SET_XY_6(pTseng, w, h);
    START_ACL(pTseng, destaddr);
}

void
Tseng6KSubsequentSolidFillRect(ScrnInfoPtr pScrn,
    int x, int y, int w, int h)
{
    TsengPtr pTseng = TsengPTR(pScrn);
    int destaddr = FBADDR(pTseng, x, y);

    wait_acl_queue(pTseng);

    /* see comment in TsengW32pSubsequentFillRectSolid */
    ACL_SOURCE_ADDRESS(pTseng->AccelColorBufferOffset + pTseng->tsengFg);

    /* if XYDIR is not reset here, drawing a hardware line in between
     * blitting, with the same ROP, color, etc will not cause a call to
     * SetupFor... (because linedrawing uses SetupForSolidFill() as its
     * Setup() function), and thus the direction register will have been
     * changed by the last LineDraw operation.
     */
    SET_XYDIR(0);

    SET_XY_6(pTseng, w, h);
    START_ACL_6(destaddr);
}

/*
 * This is the implementation of the SetupForScreenToScreenCopy function
 * that sets up the coprocessor for a subsequent batch of
 * screen-to-screen copies.
 */

static __inline__ void
Tseng_setup_screencopy(TsengPtr pTseng,
    int rop, unsigned int planemask,
    int trans_color, int blit_dir)
{
    wait_acl_queue(pTseng);

#ifdef ET6K_TRANSPARENCY
    if (Is_ET6K && (trans_color != -1)) {
	SET_BG_COLOR(trans_color);
	SET_FUNCTION_BLT_TR;
    } else
	SET_FUNCTION_BLT;

    SET_FG_ROP(rop);
#else
    if ((planemask & pTseng->planemask_mask) != pTseng->planemask_mask) {
	SET_FG_ROP_PLANEMASK(rop);
	SET_BG_COLOR(pTseng, planemask);
    } else {
	SET_FG_ROP(rop);
    }
    SET_FUNCTION_BLT;
#endif
    SET_XYDIR(blit_dir);
}

void
TsengSetupForScreenToScreenCopy(ScrnInfoPtr pScrn,
    int xdir, int ydir, int rop,
    unsigned int planemask, int trans_color)
{
    /*
     * xdir can be either 1 (left-to-right) or -1 (right-to-left).
     * ydir can be either 1 (top-to-bottom) or -1 (bottom-to-top).
     */

    TsengPtr pTseng = TsengPTR(pScrn);
    int blit_dir = 0;

/*    ErrorF("C%d ", trans_color); */

    pTseng->acl_blitxdir = xdir;
    pTseng->acl_blitydir = ydir;

    if (xdir == -1)
	blit_dir |= 0x1;
    if (ydir == -1)
	blit_dir |= 0x2;

    Tseng_setup_screencopy(pTseng, rop, planemask, trans_color, blit_dir);

    ACL_SOURCE_WRAP(0x77);	       /* no wrap */
    ACL_SOURCE_Y_OFFSET(pTseng->line_width - 1);
}

/*
 * This is the implementation of the SubsequentForScreenToScreenCopy
 * that sends commands to the coprocessor to perform a screen-to-screen
 * copy of the specified areas, with the parameters from the SetUp call.
 * In this sample implementation, the direction must be taken into
 * account when calculating the addresses (with coordinates, it might be
 * a little easier).
 *
 * Splitting up the SubsequentScreenToScreenCopy between ET4000 and ET6000
 * doesn't seem to improve speed for small blits (as it did with
 * SolidFillRect).
 */

void
TsengSubsequentScreenToScreenCopy(ScrnInfoPtr pScrn,
    int x1, int y1, int x2, int y2,
    int w, int h)
{
    TsengPtr pTseng = TsengPTR(pScrn);
    int srcaddr, destaddr;

    /*
     * Optimizing note: the pre-calc code below (i.e. until the first
     * register write) doesn't significantly affect performance. Removing it
     * all boosts small blits from 24.22 to 25.47 MB/sec. Don't waste time
     * on that. One less PCI bus write would boost us to 30.00 MB/sec, up
     * from 24.22. Waste time on _that_...
     */

    /* tseng chips want x-sizes in bytes, not pixels */
    x1 = MULBPP(pTseng, x1);
    x2 = MULBPP(pTseng, x2);

    /*
     * If the direction is "decreasing", the chip wants the addresses
     * to be at the other end, so we must be aware of that in our
     * calculations.
     */
    if (pTseng->acl_blitydir == -1) {
	srcaddr = (y1 + h - 1) * pTseng->line_width;
	destaddr = (y2 + h - 1) * pTseng->line_width;
    } else {
	srcaddr = y1 * pTseng->line_width;
	destaddr = y2 * pTseng->line_width;
    }
    if (pTseng->acl_blitxdir == -1) {
	/* Accelerator start address must point to first byte to be processed.
	 * Depending on the direction, this is the first or the last byte
	 * in the multi-byte pixel.
	 */
	int eol = MULBPP(pTseng, w);

	srcaddr += x1 + eol - 1;
	destaddr += x2 + eol - 1;
    } else {
	srcaddr += x1;
	destaddr += x2;
    }

    wait_acl_queue(pTseng);

    SET_XY(pTseng, w, h);
    ACL_SOURCE_ADDRESS(srcaddr);
    START_ACL(pTseng, destaddr);
}

static int pat_src_addr;

void
TsengSetupForColor8x8PatternFill(ScrnInfoPtr pScrn,
    int patx, int paty, int rop, unsigned int planemask, int trans_color)
{
    TsengPtr pTseng = TsengPTR(pScrn);

    pat_src_addr = FBADDR(pTseng, patx, paty);

    ErrorF("P");

    Tseng_setup_screencopy(pTseng, rop, planemask, trans_color, 0);

    switch (pTseng->Bytesperpixel) {
    case 1:
	ACL_SOURCE_WRAP(0x33);       /* 8x8 wrap */
	ACL_SOURCE_Y_OFFSET(8 - 1);
	break;
    case 2:
	ACL_SOURCE_WRAP(0x34);       /* 16x8 wrap */
	ACL_SOURCE_Y_OFFSET(16 - 1);
	break;
    case 3:
	ACL_SOURCE_WRAP(0x3D);       /* 24x8 wrap --- only for ET6000 !!! */
	ACL_SOURCE_Y_OFFSET(32 - 1); /* this is no error -- see databook */
	break;
    case 4:
	ACL_SOURCE_WRAP(0x35);       /* 32x8 wrap */
	ACL_SOURCE_Y_OFFSET(32 - 1);
    }
}

void
TsengSubsequentColor8x8PatternFillRect(ScrnInfoPtr pScrn,
    int patx, int paty, int x, int y, int w, int h)
{
    TsengPtr pTseng = TsengPTR(pScrn);
    int destaddr = FBADDR(pTseng, x, y);
    int srcaddr = pat_src_addr + MULBPP(pTseng, paty * 8 + patx);

    wait_acl_queue(pTseng);

    ACL_SOURCE_ADDRESS(srcaddr);

    SET_XY(pTseng, w, h);
    START_ACL(pTseng, destaddr);
}

/*
 * ImageWrite is nothing more than a per-scanline screencopy.
 */

void 
TsengSetupForScanlineImageWrite(ScrnInfoPtr pScrn,
    int rop, unsigned int planemask, int trans_color, int bpp, int depth)
{
    TsengPtr pTseng = TsengPTR(pScrn);

/*    ErrorF("IW"); */

    Tseng_setup_screencopy(pTseng, rop, planemask, trans_color, 0);

    ACL_SOURCE_WRAP(0x77);	       /* no wrap */
    ACL_SOURCE_Y_OFFSET(pTseng->line_width - 1);
}

void 
TsengSubsequentScanlineImageWriteRect(ScrnInfoPtr pScrn,
    int x, int y, int w, int h, int skipleft)
{
    TsengPtr pTseng = TsengPTR(pScrn);

/*    ErrorF("r%d",h); */

    pTseng->acl_iw_dest = y * pTseng->line_width + MULBPP(pTseng, x);
    pTseng->acl_skipleft = MULBPP(pTseng, skipleft);

    wait_acl_queue(pTseng);
    SET_XY(pTseng, w, 1);
}

void 
TsengSubsequentImageWriteScanline(ScrnInfoPtr pScrn,
    int bufno)
{
    TsengPtr pTseng = TsengPTR(pScrn);

/*    ErrorF("%d", bufno); */

    wait_acl_queue(pTseng);

    ACL_SOURCE_ADDRESS(pTseng->AccelImageWriteBufferOffsets[bufno] 
		       + pTseng->acl_skipleft);
    START_ACL(pTseng, pTseng->acl_iw_dest);
    pTseng->acl_iw_dest += pTseng->line_width;
}

/*
 * W32p/ET6000 hardware linedraw code. 
 *
 * TsengSetupForSolidFill() is used as a setup function.
 *
 * Three major problems that needed to be solved here:
 *
 * 1. The "bias" value must be translated into the "line draw algorithm"
 *    parameter in the Tseng accelerators. This parameter, although not
 *    documented as such, needs to be set to the _inverse_ of the
 *    appropriate bias bit (i.e. for the appropriate octant).
 *
 * 2. In >8bpp modes, the accelerator will render BYTES in the same order as
 *    it is drawing the line. This means it will render the colors in the
 *    same order as well, reversing the byte-order in pixels that are drawn
 *    right-to-left. This causes wrong colors to be rendered.
 *
 * 3. The Tseng data book says that the ACL Y count register needs to be
 *    programmed with "dy-1". A similar thing is said about ACL X count. But
 *    this assumes (x2,y2) is NOT drawn (although that is not mentionned in
 *    the data book). X assumes the endpoint _is_ drawn. If "dy-1" is used,
 *    this sometimes results in a negative value (if dx==dy==0),
 *    causing a complete accelerator hang.
 */

void
TsengSubsequentSolidBresenhamLine(ScrnInfoPtr pScrn,
    int x, int y, int major, int minor, int err, int len, int octant)
{
    TsengPtr pTseng = TsengPTR(pScrn);
    int destaddr = FBADDR(pTseng, x, y);
    int xydir = pTseng->BresenhamTable[octant];
    
    /* Tseng wants the real dx/dy in major/minor. Bresenham uses 2*dx and 2*dy */
    minor >>= 1;
    major >>= 1;

    wait_acl_queue(pTseng);

    if (!(octant & YMAJOR)) {
	SET_X_YRAW(pTseng, len, 0xFFF);
    } else {
	SET_XY_RAW(pTseng,0xFFF, len - 1);
    }

    SET_DELTA(minor, major);
    ACL_ERROR_TERM(-err);  /* error term from XAA is NEGATIVE */

    /* make sure colors are rendered correctly if >8bpp */
    if (octant & XDECREASING) {
	destaddr += pTseng->Bytesperpixel - 1;
	ACL_SOURCE_ADDRESS(pTseng->AccelColorBufferOffset 
			   + pTseng->tsengFg + pTseng->neg_x_pixel_offset);
    } else
	ACL_SOURCE_ADDRESS(pTseng->AccelColorBufferOffset + pTseng->tsengFg);

    SET_XYDIR(xydir);

    START_ACL(pTseng, destaddr);
}


#ifdef TODO
/*
 * Trapezoid filling code.
 *
 * TsengSetupForSolidFill() is used as a setup function
 */

#undef DEBUG_TRAP

#ifdef TSENG_TRAPEZOIDS
void
TsengSubsequentFillTrapezoidSolid(ytop, height, left, dxL, dyL, eL, right, dxR, dyR, eR)
    int ytop;
    int height;
    int left;
    int dxL, dyL;
    int eL;
    int right;
    int dxR, dyR;
    int eR;
{
    unsigned int tseng_bias_compensate = 0xd8;
    int destaddr, algrthm;
    int xcount = right - left + 1;     /* both edges included */
    int dir_reg = 0x60;		       /* trapezoid drawing; use error term for primary edge */
    int sec_dir_reg = 0x20;	       /* use error term for secondary edge */
    int octant = 0;

    /*    ErrorF("#"); */

    int destaddr, algrthm;
    int xcount = right - left + 1;

#ifdef USE_ERROR_TERM
    int dir_reg = 0x60;
    int sec_dir_reg = 0x20;

#else
    int dir_reg = 0x40;
    int sec_dir_reg = 0x00;

#endif
    int octant = 0;
    int bias = 0x00;		       /* FIXME !!! */

/*    ErrorF("#"); */

#ifdef DEBUG_TRAP
    ErrorF("ytop=%d, height=%d, left=%d, dxL=%d, dyL=%d, eL=%d, right=%d, dxR=%d, dyR=%d, eR=%d ",
	ytop, height, left, dxL, dyL, eL, right, dxR, dyR, eR);
#endif

    if ((dyL < 0) || (dyR < 0))
	ErrorF("Tseng Trapezoids: Wrong assumption: dyL/R < 0\n");

    destaddr = FBADDR(pTseng, left, ytop);

    /* left edge */
    if (dxL < 0) {
	dir_reg |= 1;
	octant |= XDECREASING;
	dxL = -dxL;
    }
    /* Y direction is always positive (top-to-bottom drawing) */

    wait_acl_queue(pTseng);

    /* left edge */
    /* compute axial direction and load registers */
    if (dxL >= dyL) {		       /* X is major axis */
	dir_reg |= 4;
	SET_DELTA(dyL, dxL);
	if (dir_reg & 1) {	       /* edge coherency: draw left edge */
	    destaddr += pTseng->Bytesperpixel;
	    sec_dir_reg |= 0x80;
	    xcount--;
	}
    } else {			       /* Y is major axis */
	SetYMajorOctant(octant);
	SET_DELTA(dxL, dyL);
    }
    ACL_ERROR_TERM(eL);

    /* select "linedraw algorithm" (=bias) and load direction register */
    /* ErrorF(" o=%d ", octant); */
    algrthm = ((tseng_bias_compensate >> octant) & 1) ^ 1;
    dir_reg |= algrthm << 4;
    SET_XYDIR(dir_reg);

    /* right edge */
    if (dxR < 0) {
	sec_dir_reg |= 1;
	dxR = -dxR;
    }
    /* compute axial direction and load registers */
    if (dxR >= dyR) {		       /* X is major axis */
	sec_dir_reg |= 4;
	SET_SECONDARY_DELTA(dyR, dxR);
	if (dir_reg & 1) {	       /* edge coherency: do not draw right edge */
	    sec_dir_reg |= 0x40;
	    xcount++;
	}
    } else {			       /* Y is major axis */
	SET_SECONDARY_DELTA(dxR, dyR);
    }
    ACL_SECONDARY_ERROR_TERM(eR);

    /* ErrorF("%02x", sec_dir_reg); */
    SET_SECONDARY_XYDIR(sec_dir_reg);

    SET_XY_6(pTseng, xcount, height);

#ifdef DEBUG_TRAP
    ErrorF("-> %d,%d\n", xcount, height);
#endif

    START_ACL_6(destaddr);
}
#endif

#endif
