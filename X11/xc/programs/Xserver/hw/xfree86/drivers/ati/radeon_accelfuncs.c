/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/radeon_accelfuncs.c,v 1.8 2003/11/03 05:11:05 tsi Exp $ */
/*
 * Copyright 2000 ATI Technologies Inc., Markham, Ontario, and
 *                VA Linux Systems Inc., Fremont, California.
 *
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation on the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT.  IN NO EVENT SHALL ATI, VA LINUX SYSTEMS AND/OR
 * THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/*
 * Authors:
 *   Kevin E. Martin <martin@xfree86.org>
 *   Rickard E. Faith <faith@valinux.com>
 *   Alan Hourihane <alanh@fairlite.demon.co.uk>
 *   Michel Dänzer <michel@daenzer.net>
 *
 * Credits:
 *
 *   Thanks to Ani Joshi <ajoshi@shell.unixbox.com> for providing source
 *   code to his Radeon driver.  Portions of this file are based on the
 *   initialization code for that driver.
 *
 * References:
 *
 * !!!! FIXME !!!!
 *   RAGE 128 VR/ RAGE 128 GL Register Reference Manual (Technical
 *   Reference Manual P/N RRG-G04100-C Rev. 0.04), ATI Technologies: April
 *   1999.
 *
 *   RAGE 128 Software Development Manual (Technical Reference Manual P/N
 *   SDK-G04000 Rev. 0.01), ATI Technologies: June 1999.
 *
 * Notes on unimplemented XAA optimizations:
 *
 *   SetClipping:   This has been removed as XAA expects 16bit registers
 *                  for full clipping.
 *   TwoPointLine:  The Radeon supports this. Not Bresenham.
 *   DashedLine with non-power-of-two pattern length: Apparently, there is
 *                  no way to set the length of the pattern -- it is always
 *                  assumed to be 8 or 32 (or 1024?).
 *   ScreenToScreenColorExpandFill: See p. 4-17 of the Technical Reference
 *                  Manual where it states that monochrome expansion of frame
 *                  buffer data is not supported.
 *   CPUToScreenColorExpandFill, direct: The implementation here uses a hybrid
 *                  direct/indirect method.  If we had more data registers,
 *                  then we could do better.  If XAA supported a trigger write
 *                  address, the code would be simpler.
 *   Color8x8PatternFill: Apparently, an 8x8 color brush cannot take an 8x8
 *                  pattern from frame buffer memory.
 *   ImageWrites:   Same as CPUToScreenColorExpandFill
 *
 */

#if defined(ACCEL_MMIO) && defined(ACCEL_CP)
#error Cannot define both MMIO and CP acceleration!
#endif

#if !defined(UNIXCPP) || defined(ANSICPP)
#define FUNC_NAME_CAT(prefix,suffix) prefix##suffix
#else
#define FUNC_NAME_CAT(prefix,suffix) prefix/**/suffix
#endif

#ifdef ACCEL_MMIO
#define FUNC_NAME(prefix) FUNC_NAME_CAT(prefix,MMIO)
#else
#ifdef ACCEL_CP
#define FUNC_NAME(prefix) FUNC_NAME_CAT(prefix,CP)
#else
#error No accel type defined!
#endif
#endif

/* MMIO:
 *
 * Wait for the graphics engine to be completely idle: the FIFO has
 * drained, the Pixel Cache is flushed, and the engine is idle.  This is
 * a standard "sync" function that will make the hardware "quiescent".
 *
 * CP:
 *
 * Wait until the CP is completely idle: the FIFO has drained and the CP
 * is idle.
 */
void
FUNC_NAME(RADEONWaitForIdle)(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    int            i    = 0;

#ifdef ACCEL_CP
    /* Make sure the CP is idle first */
    if (info->CPStarted) {
	int  ret;
	FLUSH_RING();

	for (;;) {
	    do {
		ret = drmCommandNone(info->drmFD, DRM_RADEON_CP_IDLE);
		if (ret && ret != -EBUSY) {
		    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			       "%s: CP idle %d\n", __FUNCTION__, ret);
		}
	    } while ((ret == -EBUSY) && (i++ < RADEON_TIMEOUT));

	    if (ret == 0) return;

	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Idle timed out, resetting engine...\n");
	    RADEONEngineReset(pScrn);
	    RADEONEngineRestore(pScrn);

	    /* Always restart the engine when doing CP 2D acceleration */
	    RADEONCP_RESET(pScrn, info);
	    RADEONCP_START(pScrn, info);
	}
    }
#endif

    RADEONTRACE(("WaitForIdle (entering): %d entries, stat=0x%08x\n",
		     INREG(RADEON_RBBM_STATUS) & RADEON_RBBM_FIFOCNT_MASK,
		     INREG(RADEON_RBBM_STATUS)));

    /* Wait for the engine to go idle */
    RADEONWaitForFifoFunction(pScrn, 64);

    for (;;) {
	for (i = 0; i < RADEON_TIMEOUT; i++) {
	    if (!(INREG(RADEON_RBBM_STATUS) & RADEON_RBBM_ACTIVE)) {
		RADEONEngineFlush(pScrn);
		return;
	    }
	}
	RADEONTRACE(("Idle timed out: %d entries, stat=0x%08x\n",
		     INREG(RADEON_RBBM_STATUS) & RADEON_RBBM_FIFOCNT_MASK,
		     INREG(RADEON_RBBM_STATUS)));
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Idle timed out, resetting engine...\n");
	RADEONEngineReset(pScrn);
	RADEONEngineRestore(pScrn);
#ifdef XF86DRI
	if (info->directRenderingEnabled) {
	    RADEONCP_RESET(pScrn, info);
	    RADEONCP_START(pScrn, info);
	}
#endif
    }
}

/* This callback is required for multiheader cards using XAA */
static void
FUNC_NAME(RADEONRestoreAccelState)(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

#ifdef ACCEL_MMIO

    CARD32         pitch64;

    pitch64 = ((pScrn->displayWidth * (pScrn->bitsPerPixel / 8) + 0x3f)) >> 6;

    OUTREG(RADEON_DEFAULT_OFFSET, (((INREG(RADEON_DISPLAY_BASE_ADDR) + pScrn->fbOffset) >> 10) |
				   (pitch64 << 22)));

    /* FIXME: May need to restore other things, like BKGD_CLK FG_CLK... */

    RADEONWaitForIdleMMIO(pScrn);

#else /* ACCEL_CP */

    RADEONWaitForFifo(pScrn, 1);
    OUTREG(RADEON_DEFAULT_OFFSET, info->frontPitchOffset);

    RADEONWaitForIdleMMIO(pScrn);

#if 0
    /* Not working yet */
    RADEONMMIO_TO_CP(pScrn, info);
#endif

    /* FIXME: May need to restore other things, like BKGD_CLK FG_CLK... */
#endif
}

/* Setup for XAA SolidFill */
static void
FUNC_NAME(RADEONSetupForSolidFill)(ScrnInfoPtr pScrn,
				   int color,
				   int rop,
				   unsigned int planemask)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);
    ACCEL_PREAMBLE();

    /* Save for later clipping */
    info->dp_gui_master_cntl_clip = (info->dp_gui_master_cntl
				     | RADEON_GMC_BRUSH_SOLID_COLOR
				     | RADEON_GMC_SRC_DATATYPE_COLOR
				     | RADEON_ROP[rop].pattern);

    BEGIN_ACCEL(4);

    OUT_ACCEL_REG(RADEON_DP_GUI_MASTER_CNTL, info->dp_gui_master_cntl_clip);
    OUT_ACCEL_REG(RADEON_DP_BRUSH_FRGD_CLR,  color);
    OUT_ACCEL_REG(RADEON_DP_WRITE_MASK,      planemask);
    OUT_ACCEL_REG(RADEON_DP_CNTL,            (RADEON_DST_X_LEFT_TO_RIGHT
					      | RADEON_DST_Y_TOP_TO_BOTTOM));

    FINISH_ACCEL();
}

/* Subsequent XAA SolidFillRect
 *
 * Tests: xtest CH06/fllrctngl, xterm
 */
static void
FUNC_NAME(RADEONSubsequentSolidFillRect)(ScrnInfoPtr pScrn,
					 int x, int y,
					 int w, int h)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);
    ACCEL_PREAMBLE();

    BEGIN_ACCEL(2);

    OUT_ACCEL_REG(RADEON_DST_Y_X,          (y << 16) | x);
    OUT_ACCEL_REG(RADEON_DST_WIDTH_HEIGHT, (w << 16) | h);

    FINISH_ACCEL();
}

/* Setup for XAA solid lines */
static void
FUNC_NAME(RADEONSetupForSolidLine)(ScrnInfoPtr pScrn,
				   int color,
				   int rop,
				   unsigned int planemask)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);
    ACCEL_PREAMBLE();

    /* Save for later clipping */
    info->dp_gui_master_cntl_clip = (info->dp_gui_master_cntl
				     | RADEON_GMC_BRUSH_SOLID_COLOR
				     | RADEON_GMC_SRC_DATATYPE_COLOR
				     | RADEON_ROP[rop].pattern);

    if (info->ChipFamily >= CHIP_FAMILY_RV200) {
	BEGIN_ACCEL(1);
	OUT_ACCEL_REG(RADEON_DST_LINE_PATCOUNT,
		      0x55 << RADEON_BRES_CNTL_SHIFT);
    }

    BEGIN_ACCEL(3);

    OUT_ACCEL_REG(RADEON_DP_GUI_MASTER_CNTL, info->dp_gui_master_cntl_clip);
    OUT_ACCEL_REG(RADEON_DP_BRUSH_FRGD_CLR,  color);
    OUT_ACCEL_REG(RADEON_DP_WRITE_MASK,      planemask);

    FINISH_ACCEL();
}

/* Subsequent XAA solid horizontal and vertical lines */
static void
FUNC_NAME(RADEONSubsequentSolidHorVertLine)(ScrnInfoPtr pScrn,
					    int x, int y,
					    int len,
					    int dir)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);
    int            w    = 1;
    int            h    = 1;
    ACCEL_PREAMBLE();

    if (dir == DEGREES_0) w = len;
    else                  h = len;

    BEGIN_ACCEL(3);

    OUT_ACCEL_REG(RADEON_DP_CNTL,          (RADEON_DST_X_LEFT_TO_RIGHT
					    | RADEON_DST_Y_TOP_TO_BOTTOM));
    OUT_ACCEL_REG(RADEON_DST_Y_X,          (y << 16) | x);
    OUT_ACCEL_REG(RADEON_DST_WIDTH_HEIGHT, (w << 16) | h);

    FINISH_ACCEL();
}

/* Subsequent XAA solid TwoPointLine line
 *
 * Tests: xtest CH06/drwln, ico, Mark Vojkovich's linetest program
 *
 * [See http://www.xfree86.org/devel/archives/devel/1999-Jun/0102.shtml for
 * Mark Vojkovich's linetest program, posted 2Jun99 to devel@xfree86.org.]
 */
static void
FUNC_NAME(RADEONSubsequentSolidTwoPointLine)(ScrnInfoPtr pScrn,
					     int xa, int ya,
					     int xb, int yb,
					     int flags)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);
    ACCEL_PREAMBLE();

    /* TODO: Check bounds -- RADEON only has 14 bits */

    if (!(flags & OMIT_LAST))
	FUNC_NAME(RADEONSubsequentSolidHorVertLine)(pScrn,
						    xb, yb, 1,
						    DEGREES_0);

    BEGIN_ACCEL(2);

    OUT_ACCEL_REG(RADEON_DST_LINE_START, (ya << 16) | xa);
    OUT_ACCEL_REG(RADEON_DST_LINE_END,   (yb << 16) | xb);

    FINISH_ACCEL();
}

/* Setup for XAA dashed lines
 *
 * Tests: xtest CH05/stdshs, XFree86/drwln
 *
 * NOTE: Since we can only accelerate lines with power-of-2 patterns of
 * length <= 32
 */
static void
FUNC_NAME(RADEONSetupForDashedLine)(ScrnInfoPtr pScrn,
				    int fg,
				    int bg,
				    int rop,
				    unsigned int planemask,
				    int length,
				    unsigned char *pattern)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);
    CARD32         pat  = *(CARD32 *)(pointer)pattern;
    ACCEL_PREAMBLE();

    /* Save for determining whether or not to draw last pixel */
    info->dashLen = length;
    info->dashPattern = pat;

#if X_BYTE_ORDER == X_BIG_ENDIAN
# define PAT_SHIFT(pat, shift) (pat >> shift)
#else
# define PAT_SHIFT(pat, shift) (pat << shift)
#endif

    switch (length) {
    case  2: pat |= PAT_SHIFT(pat,  2);  /* fall through */
    case  4: pat |= PAT_SHIFT(pat,  4);  /* fall through */
    case  8: pat |= PAT_SHIFT(pat,  8);  /* fall through */
    case 16: pat |= PAT_SHIFT(pat, 16);
    }

    /* Save for later clipping */
    info->dp_gui_master_cntl_clip = (info->dp_gui_master_cntl
				     | (bg == -1
					? RADEON_GMC_BRUSH_32x1_MONO_FG_LA
					: RADEON_GMC_BRUSH_32x1_MONO_FG_BG)
				     | RADEON_ROP[rop].pattern
				     | RADEON_GMC_BYTE_LSB_TO_MSB);
    info->dash_fg = fg;
    info->dash_bg = bg;

    BEGIN_ACCEL((bg == -1) ? 4 : 5);

    OUT_ACCEL_REG(RADEON_DP_GUI_MASTER_CNTL, info->dp_gui_master_cntl_clip);
    OUT_ACCEL_REG(RADEON_DP_WRITE_MASK,      planemask);
    OUT_ACCEL_REG(RADEON_DP_BRUSH_FRGD_CLR,  fg);
    if (bg != -1)
	OUT_ACCEL_REG(RADEON_DP_BRUSH_BKGD_CLR, bg);
    OUT_ACCEL_REG(RADEON_BRUSH_DATA0,        pat);

    FINISH_ACCEL();
}

/* Helper function to draw last point for dashed lines */
static void
FUNC_NAME(RADEONDashedLastPel)(ScrnInfoPtr pScrn,
			       int x, int y,
			       int fg)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);
    CARD32         dp_gui_master_cntl = info->dp_gui_master_cntl_clip;
    ACCEL_PREAMBLE();

    dp_gui_master_cntl &= ~RADEON_GMC_BRUSH_DATATYPE_MASK;
    dp_gui_master_cntl |=  RADEON_GMC_BRUSH_SOLID_COLOR;

    dp_gui_master_cntl &= ~RADEON_GMC_SRC_DATATYPE_MASK;
    dp_gui_master_cntl |=  RADEON_GMC_SRC_DATATYPE_COLOR;

    BEGIN_ACCEL(7);

    OUT_ACCEL_REG(RADEON_DP_GUI_MASTER_CNTL, dp_gui_master_cntl);
    OUT_ACCEL_REG(RADEON_DP_BRUSH_FRGD_CLR,  fg);
    OUT_ACCEL_REG(RADEON_DP_CNTL,            (RADEON_DST_X_LEFT_TO_RIGHT
					      | RADEON_DST_Y_TOP_TO_BOTTOM));
    OUT_ACCEL_REG(RADEON_DST_Y_X,            (y << 16) | x);
    OUT_ACCEL_REG(RADEON_DST_WIDTH_HEIGHT,   (1 << 16) | 1);

    /* Restore old values */
    OUT_ACCEL_REG(RADEON_DP_GUI_MASTER_CNTL, info->dp_gui_master_cntl_clip);
    OUT_ACCEL_REG(RADEON_DP_BRUSH_FRGD_CLR,  info->dash_fg);

    FINISH_ACCEL();
}

/* Subsequent XAA dashed line */
static void
FUNC_NAME(RADEONSubsequentDashedTwoPointLine)(ScrnInfoPtr pScrn,
					      int xa, int ya,
					      int xb, int yb,
					      int flags,
					      int phase)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);
    ACCEL_PREAMBLE();

    /* TODO: Check bounds -- RADEON only has 14 bits */

    if (!(flags & OMIT_LAST)) {
	int deltax = abs(xa - xb);
	int deltay = abs(ya - yb);
	int shift;

	if (deltax > deltay) shift = deltax;
	else                 shift = deltay;

	shift += phase;
	shift %= info->dashLen;

	if ((info->dashPattern >> shift) & 1)
	    FUNC_NAME(RADEONDashedLastPel)(pScrn, xb, yb, info->dash_fg);
	else if (info->dash_bg != -1)
	    FUNC_NAME(RADEONDashedLastPel)(pScrn, xb, yb, info->dash_bg);
    }

    BEGIN_ACCEL(3);

    OUT_ACCEL_REG(RADEON_DST_LINE_START,   (ya << 16) | xa);
    OUT_ACCEL_REG(RADEON_DST_LINE_PATCOUNT, phase);
    OUT_ACCEL_REG(RADEON_DST_LINE_END,     (yb << 16) | xb);

    FINISH_ACCEL();
}

/* Set up for transparency
 *
 * Mmmm, Seems as though the transparency compare is opposite to r128.
 * It should only draw when source != trans_color, this is the opposite
 * of that.
 */
static void
FUNC_NAME(RADEONSetTransparency)(ScrnInfoPtr pScrn,
				 int trans_color)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);

    if ((trans_color != -1) || (info->XAAForceTransBlit == TRUE)) {
	ACCEL_PREAMBLE();

	BEGIN_ACCEL(3);

	OUT_ACCEL_REG(RADEON_CLR_CMP_CLR_SRC, trans_color);
	OUT_ACCEL_REG(RADEON_CLR_CMP_MASK,    RADEON_CLR_CMP_MSK);
	OUT_ACCEL_REG(RADEON_CLR_CMP_CNTL,    (RADEON_SRC_CMP_EQ_COLOR
					       | RADEON_CLR_CMP_SRC_SOURCE));

	FINISH_ACCEL();
    }
}

/* Setup for XAA screen-to-screen copy
 *
 * Tests: xtest CH06/fllrctngl (also tests transparency)
 */
static void
FUNC_NAME(RADEONSetupForScreenToScreenCopy)(ScrnInfoPtr pScrn,
					    int xdir, int ydir,
					    int rop,
					    unsigned int planemask,
					    int trans_color)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);
    ACCEL_PREAMBLE();

    info->xdir = xdir;
    info->ydir = ydir;

    /* Save for later clipping */
    info->dp_gui_master_cntl_clip = (info->dp_gui_master_cntl
				     | RADEON_GMC_BRUSH_NONE
				     | RADEON_GMC_SRC_DATATYPE_COLOR
				     | RADEON_ROP[rop].rop
				     | RADEON_DP_SRC_SOURCE_MEMORY);

    BEGIN_ACCEL(3);

    OUT_ACCEL_REG(RADEON_DP_GUI_MASTER_CNTL, info->dp_gui_master_cntl_clip);
    OUT_ACCEL_REG(RADEON_DP_WRITE_MASK,      planemask);
    OUT_ACCEL_REG(RADEON_DP_CNTL,
		  ((xdir >= 0 ? RADEON_DST_X_LEFT_TO_RIGHT : 0) |
		   (ydir >= 0 ? RADEON_DST_Y_TOP_TO_BOTTOM : 0)));

    FINISH_ACCEL();

    info->trans_color = trans_color;
    FUNC_NAME(RADEONSetTransparency)(pScrn, trans_color);
}

/* Subsequent XAA screen-to-screen copy */
static void
FUNC_NAME(RADEONSubsequentScreenToScreenCopy)(ScrnInfoPtr pScrn,
					      int xa, int ya,
					      int xb, int yb,
					      int w, int h)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);
    ACCEL_PREAMBLE();

    if (info->xdir < 0) xa += w - 1, xb += w - 1;
    if (info->ydir < 0) ya += h - 1, yb += h - 1;

    BEGIN_ACCEL(3);

    OUT_ACCEL_REG(RADEON_SRC_Y_X,          (ya << 16) | xa);
    OUT_ACCEL_REG(RADEON_DST_Y_X,          (yb << 16) | xb);
    OUT_ACCEL_REG(RADEON_DST_HEIGHT_WIDTH, (h  << 16) | w);

    FINISH_ACCEL();
}

/* Setup for XAA mono 8x8 pattern color expansion.  Patterns with
 * transparency use `bg == -1'.  This routine is only used if the XAA
 * pixmap cache is turned on.
 *
 * Tests: xtest XFree86/fllrctngl (no other test will test this routine with
 *                                 both transparency and non-transparency)
 */
static void
FUNC_NAME(RADEONSetupForMono8x8PatternFill)(ScrnInfoPtr pScrn,
					    int patternx,
					    int patterny,
					    int fg,
					    int bg,
					    int rop,
					    unsigned int planemask)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);
#if X_BYTE_ORDER == X_BIG_ENDIAN
    unsigned char  pattern[8];
#endif
    ACCEL_PREAMBLE();

#if X_BYTE_ORDER == X_BIG_ENDIAN
    /* Take care of endianness */
    pattern[0] = (patternx & 0x000000ff);
    pattern[1] = (patternx & 0x0000ff00) >> 8;
    pattern[2] = (patternx & 0x00ff0000) >> 16;
    pattern[3] = (patternx & 0xff000000) >> 24;
    pattern[4] = (patterny & 0x000000ff);
    pattern[5] = (patterny & 0x0000ff00) >> 8;
    pattern[6] = (patterny & 0x00ff0000) >> 16;
    pattern[7] = (patterny & 0xff000000) >> 24;
#endif

    /* Save for later clipping */
    info->dp_gui_master_cntl_clip = (info->dp_gui_master_cntl
				     | (bg == -1
					? RADEON_GMC_BRUSH_8X8_MONO_FG_LA
					: RADEON_GMC_BRUSH_8X8_MONO_FG_BG)
				     | RADEON_ROP[rop].pattern
#if X_BYTE_ORDER == X_LITTLE_ENDIAN
				     | RADEON_GMC_BYTE_MSB_TO_LSB
#endif
				    );

    BEGIN_ACCEL((bg == -1) ? 5 : 6);

    OUT_ACCEL_REG(RADEON_DP_GUI_MASTER_CNTL, info->dp_gui_master_cntl_clip);
    OUT_ACCEL_REG(RADEON_DP_WRITE_MASK,      planemask);
    OUT_ACCEL_REG(RADEON_DP_BRUSH_FRGD_CLR,  fg);
    if (bg != -1)
	OUT_ACCEL_REG(RADEON_DP_BRUSH_BKGD_CLR, bg);
#if X_BYTE_ORDER == X_LITTLE_ENDIAN
    OUT_ACCEL_REG(RADEON_BRUSH_DATA0,        patternx);
    OUT_ACCEL_REG(RADEON_BRUSH_DATA1,        patterny);
#else
    OUT_ACCEL_REG(RADEON_BRUSH_DATA0,        *(CARD32 *)(pointer)&pattern[0]);
    OUT_ACCEL_REG(RADEON_BRUSH_DATA1,        *(CARD32 *)(pointer)&pattern[4]);
#endif

    FINISH_ACCEL();
}

/* Subsequent XAA 8x8 pattern color expansion.  Because they are used in
 * the setup function, `patternx' and `patterny' are not used here.
 */
static void
FUNC_NAME(RADEONSubsequentMono8x8PatternFillRect)(ScrnInfoPtr pScrn,
						  int patternx,
						  int patterny,
						  int x, int y,
						  int w, int h)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);
    ACCEL_PREAMBLE();

    BEGIN_ACCEL(3);

    OUT_ACCEL_REG(RADEON_BRUSH_Y_X,        (patterny << 8) | patternx);
    OUT_ACCEL_REG(RADEON_DST_Y_X,          (y << 16) | x);
    OUT_ACCEL_REG(RADEON_DST_HEIGHT_WIDTH, (h << 16) | w);

    FINISH_ACCEL();
}

#if 0
/* Setup for XAA color 8x8 pattern fill
 *
 * Tests: xtest XFree86/fllrctngl (with Mono8x8PatternFill off)
 */
static void
FUNC_NAME(RADEONSetupForColor8x8PatternFill)(ScrnInfoPtr pScrn,
					     int patx, int paty,
					     int rop,
					     unsigned int planemask,
					     int trans_color)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);
    ACCEL_PREAMBLE();

    /* Save for later clipping */
    info->dp_gui_master_cntl_clip = (info->dp_gui_master_cntl
				     | RADEON_GMC_BRUSH_8x8_COLOR
				     | RADEON_GMC_SRC_DATATYPE_COLOR
				     | RADEON_ROP[rop].pattern
				     | RADEON_DP_SRC_SOURCE_MEMORY);

    BEGIN_ACCEL(3);

    OUT_ACCEL_REG(RADEON_DP_GUI_MASTER_CNTL, info->dp_gui_master_cntl_clip);
    OUT_ACCEL_REG(RADEON_DP_WRITE_MASK,      planemask);
    OUT_ACCEL_REG(RADEON_SRC_Y_X,            (paty << 16) | patx);

    FINISH_ACCEL();

    info->trans_color = trans_color;
    FUNC_NAME(RADEONSetTransparency)(pScrn, trans_color);
}

/* Subsequent XAA 8x8 pattern color expansion */
static void
FUNC_NAME(RADEONSubsequentColor8x8PatternFillRect)(ScrnInfoPtr pScrn,
						   int patx, int paty,
						   int x, int y,
						   int w, int h)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);
    ACCEL_PREAMBLE();

    BEGIN_ACCEL(3);

    OUT_ACCEL_REG(RADEON_BRUSH_Y_X,        (paty << 16) | patx);
    OUT_ACCEL_REG(RADEON_DST_Y_X,          (y << 16) | x);
    OUT_ACCEL_REG(RADEON_DST_HEIGHT_WIDTH, (h << 16) | w);

    FINISH_ACCEL();
}
#endif

#ifdef ACCEL_CP
#define CP_BUFSIZE (info->indirectBuffer->total/4-9)

/* Helper function to write out a HOSTDATA_BLT packet into the indirect
 * buffer and set the XAA scratch buffer address appropriately.
 */
static void
RADEONCPScanlinePacket(ScrnInfoPtr pScrn, int bufno)
{
    RADEONInfoPtr info = RADEONPTR(pScrn);
    int           chunk_words = info->scanline_hpass * info->scanline_words;
    ACCEL_PREAMBLE();

    if (RADEON_VERBOSE) {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "CPScanline Packet h=%d hpass=%d chunkwords=%d\n",
		   info->scanline_h, info->scanline_hpass, chunk_words);
    }
    BEGIN_RING(chunk_words+9);

    OUT_RING(CP_PACKET3(RADEON_CP_PACKET3_CNTL_HOSTDATA_BLT,chunk_words+9-2));
    OUT_RING(info->dp_gui_master_cntl_clip);
    OUT_RING((info->scanline_y << 16) |
	     (info->scanline_x1clip & 0xffff));
    OUT_RING(((info->scanline_y+info->scanline_hpass) << 16) |
	     (info->scanline_x2clip & 0xffff));
    OUT_RING(info->scanline_fg);
    OUT_RING(info->scanline_bg);
    OUT_RING((info->scanline_y << 16) |
	     (info->scanline_x & 0xffff));
    OUT_RING((info->scanline_hpass << 16) |
	     (info->scanline_w & 0xffff));
    OUT_RING(chunk_words);

    info->scratch_buffer[bufno] = (unsigned char *)&__head[__count];
    __count += chunk_words;

    /* The ring can only be advanced after the __head and __count have
       been adjusted above */
    FINISH_ACCEL();

    info->scanline_y += info->scanline_hpass;
    info->scanline_h -= info->scanline_hpass;
}
#endif

/* Setup for XAA indirect CPU-to-screen color expansion (indirect).
 * Because of how the scratch buffer is initialized, this is really a
 * mainstore-to-screen color expansion.  Transparency is supported when
 * `bg == -1'.
 */
static void
FUNC_NAME(RADEONSetupForScanlineCPUToScreenColorExpandFill)(ScrnInfoPtr pScrn,
							    int fg,
							    int bg,
							    int rop,
							    unsigned int
							    planemask)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);
    ACCEL_PREAMBLE();

    /* Save for later clipping */
    info->dp_gui_master_cntl_clip = (info->dp_gui_master_cntl
				     | RADEON_GMC_DST_CLIPPING
				     | RADEON_GMC_BRUSH_NONE
				     | (bg == -1
					? RADEON_GMC_SRC_DATATYPE_MONO_FG_LA
					: RADEON_GMC_SRC_DATATYPE_MONO_FG_BG)
				     | RADEON_ROP[rop].rop
#if X_BYTE_ORDER == X_LITTLE_ENDIAN
				     | RADEON_GMC_BYTE_LSB_TO_MSB
#else
				     | RADEON_GMC_BYTE_MSB_TO_LSB
#endif
				     | RADEON_DP_SRC_SOURCE_HOST_DATA);

#ifdef ACCEL_MMIO

#if X_BYTE_ORDER == X_LITTLE_ENDIAN
    BEGIN_ACCEL(4);
#else
    BEGIN_ACCEL(5);

    OUT_ACCEL_REG(RADEON_RBBM_GUICNTL,       RADEON_HOST_DATA_SWAP_NONE);
#endif
    OUT_ACCEL_REG(RADEON_DP_GUI_MASTER_CNTL, info->dp_gui_master_cntl_clip);
    OUT_ACCEL_REG(RADEON_DP_WRITE_MASK,      planemask);
    OUT_ACCEL_REG(RADEON_DP_SRC_FRGD_CLR,    fg);
    OUT_ACCEL_REG(RADEON_DP_SRC_BKGD_CLR,    bg);

#else /* ACCEL_CP */

    info->scanline_fg = fg;
    info->scanline_bg = bg;

#if X_BYTE_ORDER == X_LITTLE_ENDIAN
    BEGIN_ACCEL(1);
#else
    BEGIN_ACCEL(2);

    OUT_ACCEL_REG(RADEON_RBBM_GUICNTL,       RADEON_HOST_DATA_SWAP_32BIT);
#endif
    OUT_ACCEL_REG(RADEON_DP_WRITE_MASK,      planemask);

#endif

    FINISH_ACCEL();
}

/* Subsequent XAA indirect CPU-to-screen color expansion.  This is only
 * called once for each rectangle.
 */
static void
FUNC_NAME(RADEONSubsequentScanlineCPUToScreenColorExpandFill)(ScrnInfoPtr
							      pScrn,
							      int x, int y,
							      int w, int h,
							      int skipleft)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);
#ifdef ACCEL_MMIO
    ACCEL_PREAMBLE();

    info->scanline_h      = h;
    info->scanline_words  = (w + 31) >> 5;

#ifdef __alpha__
    /* Always use indirect for Alpha */
    if (0)
#else
    if ((info->scanline_words * h) <= 9)
#endif
    {
	/* Turn on direct for less than 9 dword colour expansion */
	info->scratch_buffer[0] =
	    (unsigned char *)(ADDRREG(RADEON_HOST_DATA_LAST)
			      - (info->scanline_words - 1));
	info->scanline_direct   = 1;
    } else {
	/* Use indirect for anything else */
	info->scratch_buffer[0] = info->scratch_save;
	info->scanline_direct   = 0;
    }

    BEGIN_ACCEL(4 + (info->scanline_direct ?
		     (info->scanline_words * h) : 0));

    OUT_ACCEL_REG(RADEON_SC_TOP_LEFT,      (y << 16)     | ((x+skipleft)
							    & 0xffff));
    OUT_ACCEL_REG(RADEON_SC_BOTTOM_RIGHT,  ((y+h) << 16) | ((x+w) & 0xffff));
    OUT_ACCEL_REG(RADEON_DST_Y_X,          (y << 16)     | (x & 0xffff));
    /* Have to pad the width here and use clipping engine */
    OUT_ACCEL_REG(RADEON_DST_HEIGHT_WIDTH, (h << 16)     | ((w + 31) & ~31));

    FINISH_ACCEL();

#else /* ACCEL_CP */

    info->scanline_x      = x;
    info->scanline_y      = y;
    /* Have to pad the width here and use clipping engine */
    info->scanline_w      = (w + 31) & ~31;
    info->scanline_h      = h;

    info->scanline_x1clip = x + skipleft;
    info->scanline_x2clip = x + w;

    info->scanline_words  = info->scanline_w / 32;
    info->scanline_hpass  = min(h,(CP_BUFSIZE/info->scanline_words));

    RADEONCPScanlinePacket(pScrn, 0);

#endif
}

/* Subsequent XAA indirect CPU-to-screen color expansion and indirect
 * image write.  This is called once for each scanline.
 */
static void
FUNC_NAME(RADEONSubsequentScanline)(ScrnInfoPtr pScrn,
				    int bufno)
{
    RADEONInfoPtr    info = RADEONPTR(pScrn);
#ifdef ACCEL_MMIO
    CARD32          *p    = (pointer)info->scratch_buffer[bufno];
    int              i;
    int              left = info->scanline_words;
    volatile CARD32 *d;
    ACCEL_PREAMBLE();

    if (info->scanline_direct) return;

    --info->scanline_h;

    while (left) {
	write_mem_barrier();
	if (left <= 8) {
	  /* Last scanline - finish write to DATA_LAST */
	  if (info->scanline_h == 0) {
	    BEGIN_ACCEL(left);
				/* Unrolling doesn't improve performance */
	    for (d = ADDRREG(RADEON_HOST_DATA_LAST) - (left - 1); left; --left)
		*d++ = *p++;
	    return;
	  } else {
	    BEGIN_ACCEL(left);
				/* Unrolling doesn't improve performance */
	    for (d = ADDRREG(RADEON_HOST_DATA7) - (left - 1); left; --left)
		*d++ = *p++;
	  }
	} else {
	    BEGIN_ACCEL(8);
				/* Unrolling doesn't improve performance */
	    for (d = ADDRREG(RADEON_HOST_DATA0), i = 0; i < 8; i++)
		*d++ = *p++;
	    left -= 8;
	}
    }

    FINISH_ACCEL();

#else /* ACCEL_CP */

    if (--info->scanline_hpass) {
	info->scratch_buffer[bufno] += 4 * info->scanline_words;
    } else if (info->scanline_h) {
	info->scanline_hpass =
	    min(info->scanline_h,(CP_BUFSIZE/info->scanline_words));
	RADEONCPScanlinePacket(pScrn, bufno);
    }

#endif
}

/* Setup for XAA indirect image write */
static void
FUNC_NAME(RADEONSetupForScanlineImageWrite)(ScrnInfoPtr pScrn,
					    int rop,
					    unsigned int planemask,
					    int trans_color,
					    int bpp,
					    int depth)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);
    ACCEL_PREAMBLE();

    info->scanline_bpp = bpp;

    /* Save for later clipping */
    info->dp_gui_master_cntl_clip = (info->dp_gui_master_cntl
				     | RADEON_GMC_DST_CLIPPING
				     | RADEON_GMC_BRUSH_NONE
				     | RADEON_GMC_SRC_DATATYPE_COLOR
				     | RADEON_ROP[rop].rop
				     | RADEON_GMC_BYTE_MSB_TO_LSB
				     | RADEON_DP_SRC_SOURCE_HOST_DATA);

#ifdef ACCEL_MMIO

#if X_BYTE_ORDER == X_LITTLE_ENDIAN
    BEGIN_ACCEL(2);
#else
    BEGIN_ACCEL(3);

    if (bpp == 16)
	OUT_ACCEL_REG(RADEON_RBBM_GUICNTL,   RADEON_HOST_DATA_SWAP_16BIT);
    else if (bpp == 32)
	OUT_ACCEL_REG(RADEON_RBBM_GUICNTL,   RADEON_HOST_DATA_SWAP_32BIT);
    else
	OUT_ACCEL_REG(RADEON_RBBM_GUICNTL,   RADEON_HOST_DATA_SWAP_NONE);
#endif
    OUT_ACCEL_REG(RADEON_DP_GUI_MASTER_CNTL, info->dp_gui_master_cntl_clip);

#else /* ACCEL_CP */

#if X_BYTE_ORDER == X_LITTLE_ENDIAN
    BEGIN_ACCEL(1);
#else
    BEGIN_ACCEL(2);

    if (bpp == 16)
	OUT_ACCEL_REG(RADEON_RBBM_GUICNTL,   RADEON_HOST_DATA_SWAP_HDW);
    else
	OUT_ACCEL_REG(RADEON_RBBM_GUICNTL,   RADEON_HOST_DATA_SWAP_NONE);
#endif
#endif
    OUT_ACCEL_REG(RADEON_DP_WRITE_MASK,      planemask);

    FINISH_ACCEL();

    info->trans_color = trans_color;
    FUNC_NAME(RADEONSetTransparency)(pScrn, trans_color);
}

/* Subsequent XAA indirect image write. This is only called once for
 * each rectangle.
 */
static void
FUNC_NAME(RADEONSubsequentScanlineImageWriteRect)(ScrnInfoPtr pScrn,
						  int x, int y,
						  int w, int h,
						  int skipleft)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);

#ifdef ACCEL_MMIO

    int            shift = 0; /* 32bpp */
    ACCEL_PREAMBLE();

    if (pScrn->bitsPerPixel == 8) shift = 3;
    else if (pScrn->bitsPerPixel == 16) shift = 1;

    info->scanline_h      = h;
    info->scanline_words  = (w * info->scanline_bpp + 31) >> 5;

#ifdef __alpha__
    /* Always use indirect for Alpha */
    if (0)
#else
    if ((info->scanline_words * h) <= 9)
#endif
    {
	/* Turn on direct for less than 9 dword colour expansion */
	info->scratch_buffer[0]
	    = (unsigned char *)(ADDRREG(RADEON_HOST_DATA_LAST)
				- (info->scanline_words - 1));
	info->scanline_direct = 1;
    } else {
	/* Use indirect for anything else */
	info->scratch_buffer[0] = info->scratch_save;
	info->scanline_direct = 0;
    }

    BEGIN_ACCEL(4 + (info->scanline_direct ?
		     (info->scanline_words * h) : 0));

    OUT_ACCEL_REG(RADEON_SC_TOP_LEFT,      (y << 16)     | ((x+skipleft)
							    & 0xffff));
    OUT_ACCEL_REG(RADEON_SC_BOTTOM_RIGHT,  ((y+h) << 16) | ((x+w) & 0xffff));
    OUT_ACCEL_REG(RADEON_DST_Y_X,          (y << 16)     | (x & 0xffff));
    /* Have to pad the width here and use clipping engine */
    OUT_ACCEL_REG(RADEON_DST_HEIGHT_WIDTH, (h << 16)     | ((w + shift) &
							    ~shift));

    FINISH_ACCEL();

#else /* ACCEL_CP */

    int  pad = 0; /* 32bpp */

    if (pScrn->bitsPerPixel == 8)       pad = 3;
    else if (pScrn->bitsPerPixel == 16) pad = 1;

    info->scanline_x      = x;
    info->scanline_y      = y;
    /* Have to pad the width here and use clipping engine */
    info->scanline_w      = (w + pad) & ~pad;
    info->scanline_h      = h;

    info->scanline_x1clip = x + skipleft;
    info->scanline_x2clip = x + w;

    info->scanline_words  = (w * info->scanline_bpp + 31) / 32;
    info->scanline_hpass  = min(h,(CP_BUFSIZE/info->scanline_words));

    RADEONCPScanlinePacket(pScrn, 0);

#endif
}

/* Set up the clipping rectangle */
static void
FUNC_NAME(RADEONSetClippingRectangle)(ScrnInfoPtr pScrn,
				      int xa, int ya,
				      int xb, int yb)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);
    unsigned long  tmp1 = 0;
    unsigned long  tmp2 = 0;
    ACCEL_PREAMBLE();

    if (xa < 0) {
	tmp1 = (-xa) & 0x3fff;
	tmp1 |= RADEON_SC_SIGN_MASK_LO;
    } else {
	tmp1 = xa;
    }

    if (ya < 0) {
	tmp1 |= (((-ya) & 0x3fff) << 16);
	tmp1 |= RADEON_SC_SIGN_MASK_HI;
    } else {
	tmp1 |= (ya << 16);
    }

    xb++; yb++;

    if (xb < 0) {
	tmp2 = (-xb) & 0x3fff;
	tmp2 |= RADEON_SC_SIGN_MASK_LO;
    } else {
	tmp2 = xb;
    }

    if (yb < 0) {
	tmp2 |= (((-yb) & 0x3fff) << 16);
	tmp2 |= RADEON_SC_SIGN_MASK_HI;
    } else {
	tmp2 |= (yb << 16);
    }

    BEGIN_ACCEL(3);

    OUT_ACCEL_REG(RADEON_DP_GUI_MASTER_CNTL, (info->dp_gui_master_cntl_clip
					      | RADEON_GMC_DST_CLIPPING));
    OUT_ACCEL_REG(RADEON_SC_TOP_LEFT,        tmp1);
    OUT_ACCEL_REG(RADEON_SC_BOTTOM_RIGHT,    tmp2);

    FINISH_ACCEL();

    FUNC_NAME(RADEONSetTransparency)(pScrn, info->trans_color);
}

/* Disable the clipping rectangle */
static void
FUNC_NAME(RADEONDisableClipping)(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr info  = RADEONPTR(pScrn);
    ACCEL_PREAMBLE();

    BEGIN_ACCEL(3);

    OUT_ACCEL_REG(RADEON_DP_GUI_MASTER_CNTL, info->dp_gui_master_cntl_clip);
    OUT_ACCEL_REG(RADEON_SC_TOP_LEFT,        0);
    OUT_ACCEL_REG(RADEON_SC_BOTTOM_RIGHT,    (RADEON_DEFAULT_SC_RIGHT_MAX |
					      RADEON_DEFAULT_SC_BOTTOM_MAX));

    FINISH_ACCEL();

    FUNC_NAME(RADEONSetTransparency)(pScrn, info->trans_color);
}

#ifdef ACCEL_CP
/* Point the DST_PITCH_OFFSET register at the current buffer.  This
 * allows us to interact with the back and depth buffers.  All CP 2D
 * acceleration commands use the DST_PITCH_OFFSET register.
 */
void
RADEONSelectBuffer(ScrnInfoPtr pScrn, int buffer)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);
    ACCEL_PREAMBLE();

    switch (buffer) {
    case RADEON_BACK:
	info->dst_pitch_offset = info->backPitchOffset;
	break;
    case RADEON_DEPTH:
	info->dst_pitch_offset = info->depthPitchOffset;
	break;
    default:
    case RADEON_FRONT:
	info->dst_pitch_offset = info->frontPitchOffset;
	break;
    }

    BEGIN_ACCEL(1);

    OUT_ACCEL_REG(RADEON_DEFAULT_OFFSET, info->dst_pitch_offset);

    FINISH_ACCEL();
}
#endif

static void
FUNC_NAME(RADEONAccelInit)(ScreenPtr pScreen, XAAInfoRecPtr a)
{
    ScrnInfoPtr    pScrn = xf86Screens[pScreen->myNum];
    RADEONInfoPtr  info  = RADEONPTR(pScrn);

    a->Flags                            = (PIXMAP_CACHE
					   | OFFSCREEN_PIXMAPS
					   | LINEAR_FRAMEBUFFER);

				/* Sync */
    a->Sync                             = FUNC_NAME(RADEONWaitForIdle);

				/* Solid Filled Rectangle */
    a->PolyFillRectSolidFlags           = 0;
    a->SetupForSolidFill
	= FUNC_NAME(RADEONSetupForSolidFill);
    a->SubsequentSolidFillRect
	= FUNC_NAME(RADEONSubsequentSolidFillRect);

				/* Screen-to-screen Copy */
    a->ScreenToScreenCopyFlags          = 0;
    a->SetupForScreenToScreenCopy
	= FUNC_NAME(RADEONSetupForScreenToScreenCopy);
    a->SubsequentScreenToScreenCopy
	= FUNC_NAME(RADEONSubsequentScreenToScreenCopy);

				/* Mono 8x8 Pattern Fill (Color Expand) */
    a->SetupForMono8x8PatternFill
	= FUNC_NAME(RADEONSetupForMono8x8PatternFill);
    a->SubsequentMono8x8PatternFillRect
	= FUNC_NAME(RADEONSubsequentMono8x8PatternFillRect);
    a->Mono8x8PatternFillFlags          = (HARDWARE_PATTERN_PROGRAMMED_BITS
					   | HARDWARE_PATTERN_PROGRAMMED_ORIGIN
					   | HARDWARE_PATTERN_SCREEN_ORIGIN);

#if X_BYTE_ORDER == X_LITTLE_ENDIAN
    if (info->ChipFamily >= CHIP_FAMILY_RV200)
	a->Mono8x8PatternFillFlags |= BIT_ORDER_IN_BYTE_MSBFIRST;
    else
	a->Mono8x8PatternFillFlags |= BIT_ORDER_IN_BYTE_LSBFIRST;
#else
    a->Mono8x8PatternFillFlags |= BIT_ORDER_IN_BYTE_LSBFIRST;
#endif

				/* Indirect CPU-To-Screen Color Expand */

    /* RADEON gets upset, when using HOST provided data without a source
       rop.  To show run 'xtest's drwarc. */
    a->ScanlineCPUToScreenColorExpandFillFlags
	= (LEFT_EDGE_CLIPPING
	   | ROP_NEEDS_SOURCE
	   | LEFT_EDGE_CLIPPING_NEGATIVE_X);
    a->NumScanlineColorExpandBuffers    = 1;
    a->ScanlineColorExpandBuffers       = info->scratch_buffer;
    info->scratch_save
	= xalloc(((pScrn->virtualX+31)/32*4)
		 + (pScrn->virtualX * info->CurrentLayout.pixel_bytes));
    info->scratch_buffer[0]             = info->scratch_save;
    a->SetupForScanlineCPUToScreenColorExpandFill
	= FUNC_NAME(RADEONSetupForScanlineCPUToScreenColorExpandFill);
    a->SubsequentScanlineCPUToScreenColorExpandFill
	= FUNC_NAME(RADEONSubsequentScanlineCPUToScreenColorExpandFill);
    a->SubsequentColorExpandScanline    = FUNC_NAME(RADEONSubsequentScanline);

				/* Solid Lines */
    a->SetupForSolidLine
	= FUNC_NAME(RADEONSetupForSolidLine);
    a->SubsequentSolidHorVertLine
	= FUNC_NAME(RADEONSubsequentSolidHorVertLine);

#ifdef XFree86LOADER
    if (info->xaaReq.minorversion >= 1) {
#endif

    /* RADEON only supports 14 bits for lines and clipping and only
     * draws lines that are completely on-screen correctly.  This will
     * cause display corruption problem in the cases when out-of-range
     * commands are issued, like when dimming screen during GNOME logout
     * in dual-head setup.  Solid and dashed lines are therefore limited
     * to the virtual screen.
     */

    a->SolidLineFlags = LINE_LIMIT_COORDS;
    a->SolidLineLimits.x1 = 0;
    a->SolidLineLimits.y1 = 0;
    a->SolidLineLimits.x2 = pScrn->virtualX-1;
    a->SolidLineLimits.y2 = pScrn->virtualY-1;

    /* Call miSetZeroLineBias() to have mi/mfb/cfb/fb routines match
       hardware accel two point lines */
    miSetZeroLineBias(pScreen, (OCTANT5 | OCTANT6 | OCTANT7 | OCTANT8));

    a->SubsequentSolidTwoPointLine
	= FUNC_NAME(RADEONSubsequentSolidTwoPointLine);

    /* Disabled on RV200 and newer because it does not pass XTest */
    if (info->ChipFamily < CHIP_FAMILY_RV200) {
	a->SetupForDashedLine
	    = FUNC_NAME(RADEONSetupForDashedLine);
	a->SubsequentDashedTwoPointLine
	    = FUNC_NAME(RADEONSubsequentDashedTwoPointLine);
	a->DashPatternMaxLength         = 32;
	/* ROP3 doesn't seem to work properly for dashedline with GXinvert */
	a->DashedLineFlags              = (LINE_PATTERN_LSBFIRST_LSBJUSTIFIED
					   | LINE_PATTERN_POWER_OF_2_ONLY
					   | LINE_LIMIT_COORDS
					   | ROP_NEEDS_SOURCE);
	a->DashedLineLimits.x1 = 0;
	a->DashedLineLimits.y1 = 0;
	a->DashedLineLimits.x2 = pScrn->virtualX-1;
	a->DashedLineLimits.y2 = pScrn->virtualY-1;
    }

#ifdef XFree86LOADER
    } else {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "libxaa too old, can't accelerate TwoPoint lines\n");
    }
#endif

    /* Clipping, note that without this, all line accelerations will
     * not be called
     */
    a->SetClippingRectangle
	= FUNC_NAME(RADEONSetClippingRectangle);
    a->DisableClipping
	= FUNC_NAME(RADEONDisableClipping);
    a->ClippingFlags
	= (HARDWARE_CLIP_SOLID_LINE
	   | HARDWARE_CLIP_DASHED_LINE
	/* | HARDWARE_CLIP_SOLID_FILL -- seems very slow with this on */
	   | HARDWARE_CLIP_MONO_8x8_FILL
	   | HARDWARE_CLIP_SCREEN_TO_SCREEN_COPY);

    if (xf86IsEntityShared(info->pEnt->index)) {
	/* If there are more than one devices sharing this entity, we
	 * have to assign this call back, otherwise the XAA will be
	 * disabled
	 */
	if (xf86GetNumEntityInstances(info->pEnt->index) > 1)
	    a->RestoreAccelState        = FUNC_NAME(RADEONRestoreAccelState);
    }

				/* ImageWrite */
    a->NumScanlineImageWriteBuffers     = 1;
    a->ScanlineImageWriteBuffers        = info->scratch_buffer;
    a->SetupForScanlineImageWrite
	= FUNC_NAME(RADEONSetupForScanlineImageWrite);
    a->SubsequentScanlineImageWriteRect
	= FUNC_NAME(RADEONSubsequentScanlineImageWriteRect);
    a->SubsequentImageWriteScanline     = FUNC_NAME(RADEONSubsequentScanline);
    a->ScanlineImageWriteFlags          = (CPU_TRANSFER_PAD_DWORD
#ifdef ACCEL_MMIO
		/* Performance tests show that we shouldn't use GXcopy
		 * for uploads as a memcpy is faster
		 */
					  | NO_GXCOPY
#endif
		/* RADEON gets upset, when using HOST provided data
		 * without a source rop. To show run 'xtest's ptimg
		 */
					  | ROP_NEEDS_SOURCE
					  | SCANLINE_PAD_DWORD
					  | LEFT_EDGE_CLIPPING
					  | LEFT_EDGE_CLIPPING_NEGATIVE_X);

#if 0
				/* Color 8x8 Pattern Fill */
    a->SetupForColor8x8PatternFill
	= FUNC_NAME(RADEONSetupForColor8x8PatternFill);
    a->SubsequentColor8x8PatternFillRect
	= FUNC_NAME(RADEONSubsequentColor8x8PatternFillRect);
    a->Color8x8PatternFillFlags         = (HARDWARE_PATTERN_PROGRAMMED_ORIGIN
					   | HARDWARE_PATTERN_SCREEN_ORIGIN
					   | BIT_ORDER_IN_BYTE_LSBFIRST);
#endif
}

#undef FUNC_NAME
