/*
 * Copyright 1996-2000 by Robin Cutshaw <robin@XFree86.Org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Robin Cutshaw not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Robin Cutshaw makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * ROBIN CUTSHAW DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL ROBIN CUTSHAW BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 */

/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/i128/i128IBMDAC.c,v 1.2 2000/10/25 22:53:33 robin Exp $ */

#include "xf86.h"
#include "xf86_ansic.h"
#include "xf86PciInfo.h"
#include "xf86Pci.h"
#include "cursorstr.h"
#include "servermd.h"

#include "i128.h"
#include "i128reg.h"
#include "IBMRGB.h"


void I128IBMShowCursor(ScrnInfoPtr pScrn);
void I128IBMHideCursor(ScrnInfoPtr pScrn);
void I128IBMSetCursorPosition(ScrnInfoPtr pScrn, int x, int y);
void I128IBMSetCursorColors(ScrnInfoPtr pScrn, int bg, int fg);
void I128IBMLoadCursorImage(ScrnInfoPtr pScrn, unsigned char *src);
unsigned char *I128IBMRealizeCursor(xf86CursorInfoPtr infoPtr, CursorPtr pCurs);
static Bool I128IBMUseHWCursor(ScreenPtr pScrn, CursorPtr pCurs);


Bool
I128IBMHWCursorInit(ScrnInfoPtr pScrn)
{
   xf86CursorInfoPtr infoPtr;
   ScreenPtr pScreen = screenInfo.screens[pScrn->scrnIndex];
   I128Ptr pI128 = I128PTR(pScrn);

   if (!pI128->HWCursor)
      return FALSE;

   infoPtr = xf86CreateCursorInfoRec();
   if (!infoPtr) return FALSE;

   pI128->CursorInfoRec = infoPtr;
   infoPtr->MaxWidth = 64;
   infoPtr->MaxHeight = 64;
   infoPtr->SetCursorColors = I128IBMSetCursorColors;
   infoPtr->SetCursorPosition = I128IBMSetCursorPosition;
   infoPtr->LoadCursorImage = I128IBMLoadCursorImage;
   infoPtr->HideCursor = I128IBMHideCursor;
   infoPtr->ShowCursor = I128IBMShowCursor;
   infoPtr->UseHWCursor = I128IBMUseHWCursor;
   infoPtr->Flags = HARDWARE_CURSOR_TRUECOLOR_AT_8BPP |
                    HARDWARE_CURSOR_AND_SOURCE_WITH_MASK |
                    HARDWARE_CURSOR_SOURCE_MASK_INTERLEAVE_1;
   infoPtr->RealizeCursor = I128IBMRealizeCursor;

   return(xf86InitCursor(pScreen, infoPtr));
}


/*
 * Convert the cursor from server-format to hardware-format.  The IBMRGB
 * has two planes, plane 0 selects cursor color 0 or 1 and plane 1
 * selects transparent or display cursor.  The bits of these planes
 * are packed together so that one byte has 4 pixels. The organization
 * looks like:
 *             Byte 0x000 - 0x00F    top scan line, left to right
 *                  0x010 - 0x01F
 *                    .       .
 *                  0x3F0 - 0x3FF    bottom scan line
 *
 *             Byte/bit map - D7D6,D5D4,D3D2,D1D0  four pixels, two planes each
 *             Pixel/bit map - P1P0  (plane 1) == 1 maps to cursor color
 *                                   (plane 1) == 0 maps to transparent
 *                                   (plane 0) maps to cursor colors 0 and 1
 */

unsigned char *
I128IBMRealizeCursor(xf86CursorInfoPtr infoPtr, CursorPtr pCurs)
{
   register int i, j;
   unsigned char *pServMsk;
   unsigned char *pServSrc;
   int   wsrc, h;
   unsigned char *mem, *dst;

   mem = (unsigned char *)xcalloc(1,1024);  /* 64x64x2 bits */
   dst = mem;

   if (!mem)
      return NULL;

   pServSrc = (unsigned char *)pCurs->bits->source;
   pServMsk = (unsigned char *)pCurs->bits->mask;

   h = pCurs->bits->height;
   if (h > infoPtr->MaxHeight)
      h = infoPtr->MaxHeight;

   wsrc = PixmapBytePad(pCurs->bits->width, 1);	/* bytes per line */

   for (i = 0; i < infoPtr->MaxHeight; i++,mem+=16) {
      for (j = 0; j < infoPtr->MaxWidth / 8; j++) {
	 register unsigned char mask, source;

	 if (i < h && j < wsrc) {
	    /*
	     * mask byte ABCDEFGH and source byte 12345678 map to two byte
	     * cursor data H8G7F6E5 D4C3B2A1
	     */
	    mask = *pServMsk++;
	    source = *pServSrc++ & mask;

	    /* map 1 byte source and mask into two byte cursor data */
	    mem[j*2] =     ((mask&0x01) << 7) | ((source&0x01) << 6) |
		           ((mask&0x02) << 4) | ((source&0x02) << 3) |
		           ((mask&0x04) << 1) | (source&0x04)        |
		           ((mask&0x08) >> 2) | ((source&0x08) >> 3) ;
	    mem[(j*2)+1] = ((mask&0x10) << 3) | ((source&0x10) << 2) |
		           (mask&0x20)        | ((source&0x20) >> 1) |
		           ((mask&0x40) >> 3) | ((source&0x40) >> 4) |
		           ((mask&0x80) >> 6) | ((source&0x80) >> 7) ;
	 } else {
	    mem[j*2]     = 0x00;
	    mem[(j*2)+1] = 0x00;
	 }
      }
      /*
       * if we still have more bytes on this line (j < wsrc),
       * we have to ignore the rest of the line.
       */
       while (j++ < wsrc) pServMsk++,pServSrc++;
   }
   return dst;
}


void 
I128IBMShowCursor(ScrnInfoPtr pScrn)
{
   CARD32 tmpl, tmph;
   I128Ptr pI128 = I128PTR(pScrn);

   /* Enable cursor - X11 mode */
   tmpl = pI128->mem.rbase_g[IDXL_I] & 0xFF;
   tmph = pI128->mem.rbase_g[IDXH_I] & 0xFF;
   pI128->mem.rbase_g[IDXCTL_I] = 0;					MB;
   pI128->mem.rbase_g[IDXH_I] = 0;					MB;
   pI128->mem.rbase_g[IDXL_I] = IBMRGB_curs;				MB;
   pI128->mem.rbase_g[DATA_I] = 0x27;					MB;

   pI128->mem.rbase_g[IDXH_I] = tmph;					MB;
   pI128->mem.rbase_g[IDXL_I] = tmpl;					MB;

   return;
}

void
I128IBMHideCursor(ScrnInfoPtr pScrn)
{
   CARD32 tmpl, tmph, tmp1;
   I128Ptr pI128 = I128PTR(pScrn);

   tmpl = pI128->mem.rbase_g[IDXL_I] & 0xFF;
   tmph = pI128->mem.rbase_g[IDXH_I] & 0xFF;
   pI128->mem.rbase_g[IDXCTL_I] = 0;					MB;
   pI128->mem.rbase_g[IDXH_I] = 0;					MB;
   pI128->mem.rbase_g[IDXL_I] = IBMRGB_curs;				MB;
   tmp1 = pI128->mem.rbase_g[DATA_I] & 0xFC;
   pI128->mem.rbase_g[DATA_I] = tmp1;					MB;

   pI128->mem.rbase_g[IDXH_I] = tmph;					MB;
   pI128->mem.rbase_g[IDXL_I] = tmpl;					MB;

   return;
}

void
I128IBMSetCursorPosition(ScrnInfoPtr pScrn, int x, int y)
{
   CARD32 tmpl, tmph;
   I128Ptr pI128 = I128PTR(pScrn);

   x += 64;
   y += 64;

   tmpl = pI128->mem.rbase_g[IDXL_I] & 0xFF;
   tmph = pI128->mem.rbase_g[IDXH_I] & 0xFF;

   pI128->mem.rbase_g[IDXH_I] = 0;					MB;
   pI128->mem.rbase_g[IDXL_I] = IBMRGB_curs_hot_x;			MB;
   pI128->mem.rbase_g[DATA_I] = 0x3F;					MB;
   pI128->mem.rbase_g[IDXL_I] = IBMRGB_curs_hot_y;			MB;
   pI128->mem.rbase_g[DATA_I] = 0x3F;					MB;
   pI128->mem.rbase_g[IDXL_I] = IBMRGB_curs_xl;				MB;
   pI128->mem.rbase_g[DATA_I] = x & 0xFF;				MB;
   pI128->mem.rbase_g[IDXL_I] = IBMRGB_curs_xh;				MB;
   pI128->mem.rbase_g[DATA_I] = (x >> 8) & 0x0F;			MB;
   pI128->mem.rbase_g[IDXL_I] = IBMRGB_curs_yl;				MB;
   pI128->mem.rbase_g[DATA_I] = y & 0xFF;				MB;
   pI128->mem.rbase_g[IDXL_I] = IBMRGB_curs_yh;				MB;
   pI128->mem.rbase_g[DATA_I] = (y >> 8) & 0x0F;			MB;

   pI128->mem.rbase_g[IDXH_I] = tmph;					MB;
   pI128->mem.rbase_g[IDXL_I] = tmpl;					MB;

   return;
}

void
I128IBMSetCursorColors(ScrnInfoPtr pScrn, int bg, int fg)
{
   CARD32 tmp;
   I128Ptr pI128 = I128PTR(pScrn);

   tmp = pI128->mem.rbase_g[IDXL_I] & 0xFF;

   /* Background color */
   pI128->mem.rbase_g[IDXL_I] = IBMRGB_curs_col1_r;			MB;
   pI128->mem.rbase_g[DATA_I] = (bg & 0x00FF0000) >> 16;		MB;
   pI128->mem.rbase_g[IDXL_I] = IBMRGB_curs_col1_g;			MB;
   pI128->mem.rbase_g[DATA_I] = (bg & 0x0000FF00) >> 8;			MB;
   pI128->mem.rbase_g[IDXL_I] = IBMRGB_curs_col1_b;			MB;
   pI128->mem.rbase_g[DATA_I] = (bg & 0x000000FF);			MB;

   /* Foreground color */
   pI128->mem.rbase_g[IDXL_I] = IBMRGB_curs_col2_r;			MB;
   pI128->mem.rbase_g[DATA_I] = (fg & 0x00FF0000) >> 16;		MB;
   pI128->mem.rbase_g[IDXL_I] = IBMRGB_curs_col2_g;			MB;
   pI128->mem.rbase_g[DATA_I] = (fg & 0x0000FF00) >> 8;			MB;
   pI128->mem.rbase_g[IDXL_I] = IBMRGB_curs_col2_b;			MB;
   pI128->mem.rbase_g[DATA_I] = (fg & 0x000000FF);			MB;

   pI128->mem.rbase_g[IDXL_I] = tmp;					MB;

   return;
}

void 
I128IBMLoadCursorImage(ScrnInfoPtr pScrn, unsigned char *src)
{
   I128Ptr pI128 = I128PTR(pScrn);
   register int   i;
   CARD32 tmph, tmpl, tmpc;

   tmpc = pI128->mem.rbase_g[IDXCTL_I] & 0xFF;
   tmph = pI128->mem.rbase_g[IDXH_I] & 0xFF;
   tmpl = pI128->mem.rbase_g[IDXL_I] & 0xFF;

   pI128->BlockCursor = TRUE;

   pI128->mem.rbase_g[IDXCTL_I] = 0;					MB;

   pI128->mem.rbase_g[IDXL_I] = IBMRGB_curs_hot_x;			MB;
   pI128->mem.rbase_g[DATA_I] = 0x00;					MB;
   pI128->mem.rbase_g[IDXL_I] = IBMRGB_curs_hot_y;			MB;
   pI128->mem.rbase_g[DATA_I] = 0x00;					MB;
   pI128->mem.rbase_g[IDXL_I] = IBMRGB_curs_xl;				MB;
   pI128->mem.rbase_g[DATA_I] = 0xFF;					MB;
   pI128->mem.rbase_g[IDXL_I] = IBMRGB_curs_xh;				MB;
   pI128->mem.rbase_g[DATA_I] = 0x7F;					MB;
   pI128->mem.rbase_g[IDXL_I] = IBMRGB_curs_yl;				MB;
   pI128->mem.rbase_g[DATA_I] = 0xFF;					MB;
   pI128->mem.rbase_g[IDXL_I] = IBMRGB_curs_yh;				MB;
   pI128->mem.rbase_g[DATA_I] = 0x7F;					MB;

   pI128->mem.rbase_g[IDXH_I] = (IBMRGB_curs_array >> 8) & 0xFF;	MB;
   pI128->mem.rbase_g[IDXL_I] = IBMRGB_curs_array & 0xFF;		MB;

   pI128->mem.rbase_g[IDXCTL_I] = 1; /* enable auto-inc */		MB;

   /* 
    * Output the cursor data.  The realize function has put the planes into
    * their correct order, so we can just blast this out.
    */
   for (i = 0; i < 1024; i++,src++) {
      pI128->mem.rbase_g[DATA_I] = (CARD32 )*src;			MB;
   }

   pI128->mem.rbase_g[IDXCTL_I] = tmpc;                                 MB;
   pI128->mem.rbase_g[IDXH_I] = tmph;                                   MB;
   pI128->mem.rbase_g[IDXL_I] = tmpl;                                   MB;

   pI128->BlockCursor = FALSE;

   return;
}


static Bool
I128IBMUseHWCursor(ScreenPtr pScrn, CursorPtr pCurs)
{           
   if( XF86SCRNINFO(pScrn)->currentMode->Flags & V_DBLSCAN )
      return FALSE;
   return TRUE;
}           


Bool I128TIHWCursorInit(ScrnInfoPtr pScrn) { return FALSE; }
Bool I128ProgramTi3025(ScrnInfoPtr pScrn, DisplayModePtr mode) { return FALSE; }

Bool
I128ProgramIBMRGB(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
   I128Ptr pI128 = I128PTR(pScrn);
   unsigned char tmp2, m, n, df, best_m, best_n, best_df, max_n;
   CARD32 tmpl, tmph, tmpc;
   long f, vrf, outf, best_vrf, best_diff, best_outf, diff;
   long requested_freq;
   int   freq = mode->SynthClock;
   int   flags = mode->Flags;

#define REF_FREQ	 25175000
#define MAX_VREF	  3380000
/* Actually, MIN_VREF can be as low as 1000000;
 * this allows clock speeds down to 17 MHz      */
#define MIN_VREF	  1500000
#define MAX_VCO		220000000
#define MIN_VCO		 65000000

   if (freq < 25000) {
       xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
              "Specified dot clock (%.3f) too low for IBM RGB52x",
	      freq / 1000.0);
       return(FALSE);
   } else if (freq > MAX_VCO) {
       xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
              "Specified dot clock (%.3f) too high for IBM RGB52x",
	      freq / 1000.0);
       return(FALSE);
   }

   requested_freq = freq * 1000;

   best_m = best_n = best_df = 0;
   best_vrf = best_outf = 0;
   best_diff = requested_freq;  /* worst case */

   for (df=0; df<4; df++) {
   	max_n = REF_FREQ / MIN_VREF;
   	if (df < 3)
   		max_n >>= 1;
	for (n=2; n<max_n; n++)
		for (m=65; m<=128; m++) {
			vrf = REF_FREQ / n;
			if (df < 3)
				vrf >>= 1;
			if ((vrf > MAX_VREF) || (vrf < MIN_VREF))
				continue;

			f = vrf * m;
			outf = f;
			if (df < 2)
				outf >>= 2 - df;
			if ((f > MAX_VCO) || (f < MIN_VCO))
				continue;

			/* outf is a valid freq, pick the closest now */

			if ((diff = (requested_freq - outf)) < 0)
				diff = -diff;;
			if (diff < best_diff) {
				best_diff = diff;
				best_m = m;
				best_n = n;
				best_df = df;
				best_outf = outf;
			}
		}
   }

   /* do we have an acceptably close frequency? (less than 1% diff) */

   if (best_diff > (requested_freq/100)) {
       xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
              "Specified dot clock (%.3f) too far (best %.3f) IBM RGB52x",
	      requested_freq / 1000.0, best_outf / 1000.0);
       return(FALSE);
   }

   pI128->mem.rbase_g[PEL_MASK] = 0xFF;					MB;

   tmpc = pI128->mem.rbase_g[IDXCTL_I] & 0xFF;
   tmph = pI128->mem.rbase_g[IDXH_I] & 0xFF;
   tmpl = pI128->mem.rbase_g[IDXL_I] & 0xFF;

   pI128->mem.rbase_g[IDXH_I] = 0;					MB;
   pI128->mem.rbase_g[IDXCTL_I] = 0;					MB;

   pI128->mem.rbase_g[IDXL_I] = IBMRGB_misc_clock;			MB;
   tmp2 = pI128->mem.rbase_g[DATA_I] & 0xFF;
   pI128->mem.rbase_g[DATA_I] = tmp2 | 0x81;				MB;

   pI128->mem.rbase_g[IDXL_I] = IBMRGB_m0+4;				MB;
   pI128->mem.rbase_g[DATA_I] = (best_df<<6) | (best_m&0x3f);		MB;
   pI128->mem.rbase_g[IDXL_I] = IBMRGB_n0+4;				MB;
   pI128->mem.rbase_g[DATA_I] = best_n;					MB;

   pI128->mem.rbase_g[IDXL_I] = IBMRGB_pll_ctrl1;			MB;
   tmp2 = pI128->mem.rbase_g[DATA_I] & 0xFF;
   pI128->mem.rbase_g[DATA_I] = (tmp2&0xf8) | 3;  /* 8 M/N pairs in PLL */ MB;

   pI128->mem.rbase_g[IDXL_I] = IBMRGB_pll_ctrl2;			MB;
   tmp2 = pI128->mem.rbase_g[DATA_I] & 0xFF;
   pI128->mem.rbase_g[DATA_I] = (tmp2&0xf0) | 2;  /* clock number 2 */	MB;

   pI128->mem.rbase_g[IDXL_I] = IBMRGB_misc_clock;			MB;
   tmp2 = pI128->mem.rbase_g[DATA_I] & 0xf0;
   pI128->mem.rbase_g[DATA_I] = tmp2 | ((flags & V_DBLCLK) ? 0x03 : 0x01); MB;

   pI128->mem.rbase_g[IDXL_I] = IBMRGB_sync;				MB;
   pI128->mem.rbase_g[DATA_I] = ((flags & V_PHSYNC) ? 0x10 : 0x00)
                           | ((flags & V_PVSYNC) ? 0x20 : 0x00);	MB;
   pI128->mem.rbase_g[IDXL_I] = IBMRGB_hsync_pos;			MB;
   pI128->mem.rbase_g[DATA_I] = 0x01;  /* Delay syncs by 1 pclock */	MB;
   pI128->mem.rbase_g[IDXL_I] = IBMRGB_pwr_mgmt;			MB;
   pI128->mem.rbase_g[DATA_I] = 0x00;					MB;
   pI128->mem.rbase_g[IDXL_I] = IBMRGB_dac_op;				MB;
   tmp2 = (pI128->RamdacType == IBM528_DAC) ? 0x02 : 0x00;  /* fast slew */
   if (pI128->DACSyncOnGreen) tmp2 |= 0x08;
   pI128->mem.rbase_g[DATA_I] = tmp2;					MB;
   pI128->mem.rbase_g[IDXL_I] = IBMRGB_pal_ctrl;			MB;
   pI128->mem.rbase_g[DATA_I] = 0x00;					MB;
   pI128->mem.rbase_g[IDXL_I] = IBMRGB_sysclk;				MB;
   pI128->mem.rbase_g[DATA_I] = 0x01;					MB;
   pI128->mem.rbase_g[IDXL_I] = IBMRGB_misc1;				MB;
   tmp2 = pI128->mem.rbase_g[DATA_I] & 0xbc;
   tmp2 |= 0x20;
   if ((pI128->MemoryType != I128_MEMORY_DRAM) &&
       (pI128->MemoryType != I128_MEMORY_SGRAM))
   	tmp2 |= (pI128->RamdacType == IBM528_DAC) ? 3 : 1;
   pI128->mem.rbase_g[DATA_I] = tmp2;					MB;
   pI128->mem.rbase_g[IDXL_I] = IBMRGB_misc2;				MB;
   tmp2 = 0x03;
   if (pI128->DAC8Bit)
	tmp2 |= 0x04;
   if (!((pI128->MemoryType == I128_MEMORY_DRAM) &&
	 (pI128->bitsPerPixel > 16)))
	tmp2 |= 0x40;
   if ((pI128->MemoryType == I128_MEMORY_SGRAM) &&
	 (pI128->bitsPerPixel > 16) &&
         (pI128->RamdacType != SILVER_HAMMER_DAC) )
	tmp2 &= 0x3F;
   pI128->mem.rbase_g[DATA_I] = tmp2;					MB;
   pI128->mem.rbase_g[IDXL_I] = IBMRGB_misc3;				MB;
   pI128->mem.rbase_g[DATA_I] = 0x00;					MB;
   pI128->mem.rbase_g[IDXL_I] = IBMRGB_misc4;				MB;
   pI128->mem.rbase_g[DATA_I] = 0x00;					MB;

   /* ?? There is no write to cursor control register */

   if (pI128->RamdacType == IBM526_DAC) {
	if (pI128->MemoryType == I128_MEMORY_SGRAM) {
	    pI128->mem.rbase_g[IDXL_I] = IBMRGB_sysclk_ref_div;		MB;
	    pI128->mem.rbase_g[DATA_I] = 0x09;				MB;
	    pI128->mem.rbase_g[IDXL_I] = IBMRGB_sysclk_vco_div;		MB;
	    pI128->mem.rbase_g[DATA_I] = 0x83;				MB;
	} else {
	/* program mclock to 52MHz */
	    pI128->mem.rbase_g[IDXL_I] = IBMRGB_sysclk_ref_div;		MB;
	    pI128->mem.rbase_g[DATA_I] = 0x08;				MB;
	    pI128->mem.rbase_g[IDXL_I] = IBMRGB_sysclk_vco_div;		MB;
	    pI128->mem.rbase_g[DATA_I] = 0x41;				MB;
	}
	/* should delay at least a millisec so we'll wait 50 */
   	usleep(50000);
   }

   switch (pI128->depth) {
   	case 24: /* 32 bit */
   		pI128->mem.rbase_g[IDXL_I] = IBMRGB_pix_fmt;		MB;
   		tmp2 = pI128->mem.rbase_g[DATA_I] & 0xf8;
   		pI128->mem.rbase_g[DATA_I] = tmp2 | 0x06;		MB;
   		pI128->mem.rbase_g[IDXL_I] = IBMRGB_32bpp;		MB;
   		pI128->mem.rbase_g[DATA_I] = 0x03;			MB;
   		break;
	case 16:
   		pI128->mem.rbase_g[IDXL_I] = IBMRGB_pix_fmt;		MB;
   		tmp2 = pI128->mem.rbase_g[DATA_I] & 0xf8;
   		pI128->mem.rbase_g[DATA_I] = tmp2 | 0x04;		MB;
   		pI128->mem.rbase_g[IDXL_I] = IBMRGB_16bpp;		MB;
   		pI128->mem.rbase_g[DATA_I] = 0xC7;			MB;
   		break;
	case 15:
   		pI128->mem.rbase_g[IDXL_I] = IBMRGB_pix_fmt;		MB;
   		tmp2 = pI128->mem.rbase_g[DATA_I] & 0xf8;
   		pI128->mem.rbase_g[DATA_I] = tmp2 | 0x04;		MB;
   		pI128->mem.rbase_g[IDXL_I] = IBMRGB_16bpp;		MB;
   		pI128->mem.rbase_g[DATA_I] = 0xC5;			MB;
   		break;
	default: /* 8 bit */
   		pI128->mem.rbase_g[IDXL_I] = IBMRGB_pix_fmt;		MB;
   		tmp2 = pI128->mem.rbase_g[DATA_I] & 0xf8;
   		pI128->mem.rbase_g[DATA_I] = tmp2 | 0x03;		MB;
   		pI128->mem.rbase_g[IDXL_I] = IBMRGB_8bpp;		MB;
   		pI128->mem.rbase_g[DATA_I] = 0x00;			MB;
   		break;
   }

   pI128->mem.rbase_g[IDXCTL_I] = tmpc;					MB;
   pI128->mem.rbase_g[IDXH_I] = tmph;					MB;
   pI128->mem.rbase_g[IDXL_I] = tmpl;					MB;

   return(TRUE);
}


Bool
I128ProgramSilverHammer(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
   /* The SilverHammer DAC is essentially the same as the IBMRGBxxx DACs,
    * but with fewer options and a different reference frequency.
    */

   I128Ptr pI128 = I128PTR(pScrn);
   unsigned char tmp2, m, n, df, best_m, best_n, best_df, max_n;
   CARD32 tmpl, tmph, tmpc;
   long f, vrf, outf, best_vrf, best_diff, best_outf, diff;
   long requested_freq;
   int   freq = mode->SynthClock;
   int   flags = mode->Flags;
   int   skew = mode->HSkew;

#undef  REF_FREQ
#define REF_FREQ	 37500000
#undef  MAX_VREF
#define MAX_VREF	  9000000
#define MIN_VREF	  1500000
#undef  MAX_VCO
#define MAX_VCO		270000000
#define MIN_VCO		 65000000

   if (freq < 25000) {
       xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
              "Specified dot clock (%.3f) too low for SilverHammer",
	      freq / 1000.0);
       return(FALSE);
   } else if (freq > MAX_VCO) {
       xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
              "Specified dot clock (%.3f) too high for SilverHammer",
	      freq / 1000.0);
       return(FALSE);
   }

   requested_freq = freq * 1000;

   best_m = best_n = best_df = 0;
   best_vrf = best_outf = 0;
   best_diff = requested_freq;  /* worst case */

   for (df=0; df<4; df++) {
   	max_n = REF_FREQ / MIN_VREF;
   	if (df < 3)
   		max_n >>= 1;
	for (n=2; n<max_n; n++)
		for (m=65; m<=128; m++) {
			vrf = REF_FREQ / n;
			if (df < 3)
				vrf >>= 1;
			if ((vrf > MAX_VREF) || (vrf < MIN_VREF))
				continue;

			f = vrf * m;
			outf = f;
			if (df < 2)
				outf >>= 2 - df;
			if ((f > MAX_VCO) || (f < MIN_VCO))
				continue;

			/* outf is a valid freq, pick the closest now */

			if ((diff = (requested_freq - outf)) < 0)
				diff = -diff;;
			if (diff < best_diff) {
				best_diff = diff;
				best_m = m;
				best_n = n;
				best_df = df;
				best_outf = outf;
			}
		}
   }

   /* do we have an acceptably close frequency? (less than 1% diff) */

   if (best_diff > (requested_freq/100)) {
       xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
              "Specified dot clock (%.3f) too far (best %.3f) SilverHammer",
	      requested_freq / 1000.0, best_outf / 1000.0);
       return(FALSE);
   }

   pI128->mem.rbase_g[PEL_MASK] = 0xFF;					MB;

   tmpc = pI128->mem.rbase_g[IDXCTL_I] & 0xFF;
   tmph = pI128->mem.rbase_g[IDXH_I] & 0xFF;
   tmpl = pI128->mem.rbase_g[IDXL_I] & 0xFF;

   pI128->mem.rbase_g[IDXH_I] = 0;					MB;
   pI128->mem.rbase_g[IDXCTL_I] = 0;					MB;

   pI128->mem.rbase_g[IDXL_I] = IBMRGB_misc_clock;			MB;
   tmp2 = pI128->mem.rbase_g[DATA_I] & 0xFF;
   pI128->mem.rbase_g[DATA_I] = tmp2 | 0x81;				MB;

   if (!pI128->Primary) {
       pI128->mem.rbase_g[IDXL_I] = IBMRGB_m0;				MB;
       pI128->mem.rbase_g[DATA_I] = 0x15;				MB;
       pI128->mem.rbase_g[IDXL_I] = IBMRGB_m0+1;			MB;
       pI128->mem.rbase_g[DATA_I] = 0x10;				MB;
       pI128->mem.rbase_g[IDXL_I] = IBMRGB_m0+2;			MB;
       pI128->mem.rbase_g[DATA_I] = 0x2c;				MB;
       pI128->mem.rbase_g[IDXL_I] = IBMRGB_m0+3;			MB;
       pI128->mem.rbase_g[DATA_I] = 0x12;				MB;
   }
   pI128->mem.rbase_g[IDXL_I] = IBMRGB_m0+4;				MB;
   pI128->mem.rbase_g[DATA_I] = (best_df<<6) | (best_m&0x3f);		MB;
   pI128->mem.rbase_g[IDXL_I] = IBMRGB_n0+4;				MB;
   pI128->mem.rbase_g[DATA_I] = best_n;					MB;

   pI128->mem.rbase_g[IDXL_I] = IBMRGB_pll_ctrl1;			MB;
   tmp2 = pI128->mem.rbase_g[DATA_I] & 0xFF;
   pI128->mem.rbase_g[DATA_I] = (tmp2&0xf8) | 3;  /* 8 M/N pairs in PLL */ MB;

   pI128->mem.rbase_g[IDXL_I] = IBMRGB_pll_ctrl2;			MB;
   tmp2 = pI128->mem.rbase_g[DATA_I] & 0xFF;
   pI128->mem.rbase_g[DATA_I] = (tmp2&0xf0) | 2;  /* clock number 2 */	MB;

   pI128->mem.rbase_g[IDXL_I] = IBMRGB_misc_clock;			MB;
   tmp2 = pI128->mem.rbase_g[DATA_I] & 0xf0;
   pI128->mem.rbase_g[DATA_I] = tmp2 | ((flags & V_DBLCLK) ? 0x03 : 0x01); MB;

   pI128->mem.rbase_g[IDXL_I] = IBMRGB_sync;				MB;
   pI128->mem.rbase_g[DATA_I] = ((flags & V_PHSYNC) ? 0x10 : 0x00)
                           | ((flags & V_PVSYNC) ? 0x20 : 0x00);	MB;
   pI128->mem.rbase_g[IDXL_I] = IBMRGB_hsync_pos;			MB;
   pI128->mem.rbase_g[DATA_I] = ((flags & V_HSKEW)  ? skew : 0x01);	MB;
   pI128->mem.rbase_g[IDXL_I] = IBMRGB_pwr_mgmt;			MB;
/* Use 0x01 below with digital flat panel to conserve energy and reduce noise */
   pI128->mem.rbase_g[DATA_I] = (pI128->FlatPanel ? 0x01 : 0x00);	MB;
   pI128->mem.rbase_g[IDXL_I] = IBMRGB_dac_op;				MB;
   pI128->mem.rbase_g[DATA_I] = (pI128->DACSyncOnGreen ? 0x08 : 0x00);	MB;
   pI128->mem.rbase_g[IDXL_I] = IBMRGB_pal_ctrl;			MB;
   pI128->mem.rbase_g[DATA_I] = 0x00;					MB;
   pI128->mem.rbase_g[IDXL_I] = IBMRGB_sysclk;				MB;
   pI128->mem.rbase_g[DATA_I] = 0x01;					MB;
   pI128->mem.rbase_g[IDXL_I] = IBMRGB_misc1;				MB;
   tmp2 = pI128->mem.rbase_g[DATA_I] & 0xbc;
   if ((pI128->MemoryType != I128_MEMORY_DRAM) &&
       (pI128->MemoryType != I128_MEMORY_SGRAM))
   	tmp2 |= (pI128->RamdacType == IBM528_DAC) ? 3 : 1;
   pI128->mem.rbase_g[DATA_I] = tmp2;					MB;
   pI128->mem.rbase_g[IDXL_I] = IBMRGB_misc2;				MB;
   tmp2 = 0x03;
   if (pI128->DAC8Bit)
	tmp2 |= 0x04;
   if (!((pI128->MemoryType == I128_MEMORY_DRAM) &&
	 (pI128->bitsPerPixel > 16)))
	tmp2 |= 0x40;
   if ((pI128->MemoryType == I128_MEMORY_SGRAM) &&
	 (pI128->bitsPerPixel > 16) &&
         (pI128->RamdacType != SILVER_HAMMER_DAC) )
	tmp2 &= 0x3F;
   pI128->mem.rbase_g[DATA_I] = tmp2;					MB;
   pI128->mem.rbase_g[IDXL_I] = IBMRGB_misc3;				MB;
   pI128->mem.rbase_g[DATA_I] = 0x00;					MB;
   pI128->mem.rbase_g[IDXL_I] = IBMRGB_misc4;				MB;
   pI128->mem.rbase_g[DATA_I] = 0x00;					MB;

   /* ?? There is no write to cursor control register */

   /* Set the memory clock speed to 95 MHz */
   pI128->mem.rbase_g[IDXL_I] = IBMRGB_sysclk_ref_div;		MB;
   pI128->mem.rbase_g[DATA_I] = 0x08;				MB;
   pI128->mem.rbase_g[IDXL_I] = IBMRGB_sysclk_vco_div;		MB;
   pI128->mem.rbase_g[DATA_I] = 0x50;				MB;

   /* should delay at least a millisec so we'll wait 50 */
   usleep(50000);

   switch (pI128->depth) {
   	case 24: /* 32 bit */
   		pI128->mem.rbase_g[IDXL_I] = IBMRGB_pix_fmt;		MB;
   		tmp2 = pI128->mem.rbase_g[DATA_I] & 0xf8;
   		pI128->mem.rbase_g[DATA_I] = tmp2 | 0x06;		MB;
   		pI128->mem.rbase_g[IDXL_I] = IBMRGB_32bpp;		MB;
   		pI128->mem.rbase_g[DATA_I] = 0x03;			MB;
   		break;
	case 16:
   		pI128->mem.rbase_g[IDXL_I] = IBMRGB_pix_fmt;		MB;
   		tmp2 = pI128->mem.rbase_g[DATA_I] & 0xf8;
   		pI128->mem.rbase_g[DATA_I] = tmp2 | 0x04;		MB;
   		pI128->mem.rbase_g[IDXL_I] = IBMRGB_16bpp;		MB;
   		pI128->mem.rbase_g[DATA_I] = 0xC7;			MB;
   		break;
	case 15:
   		pI128->mem.rbase_g[IDXL_I] = IBMRGB_pix_fmt;		MB;
   		tmp2 = pI128->mem.rbase_g[DATA_I] & 0xf8;
   		pI128->mem.rbase_g[DATA_I] = tmp2 | 0x04;		MB;
   		pI128->mem.rbase_g[IDXL_I] = IBMRGB_16bpp;		MB;
   		pI128->mem.rbase_g[DATA_I] = 0xC5;			MB;
   		break;
	default: /* 8 bit */
   		pI128->mem.rbase_g[IDXL_I] = IBMRGB_pix_fmt;		MB;
   		tmp2 = pI128->mem.rbase_g[DATA_I] & 0xf8;
   		pI128->mem.rbase_g[DATA_I] = tmp2 | 0x03;		MB;
   		pI128->mem.rbase_g[IDXL_I] = IBMRGB_8bpp;		MB;
   		pI128->mem.rbase_g[DATA_I] = 0x00;			MB;
   		break;
   }

   pI128->mem.rbase_g[IDXCTL_I] = tmpc;					MB;
   pI128->mem.rbase_g[IDXH_I] = tmph;					MB;
   pI128->mem.rbase_g[IDXL_I] = tmpl;					MB;

   return(TRUE);
}


