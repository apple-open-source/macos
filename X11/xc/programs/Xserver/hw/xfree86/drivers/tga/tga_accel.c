/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/tga/tga_accel.c,v 1.15 2001/11/21 22:32:59 alanh Exp $ */

/*
 * Copyright 1996,1997 by Alan Hourihane, Wigan, England.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Alan Hourihane not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Alan Hourihane makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * ALAN HOURIHANE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL ALAN HOURIHANE BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Alan Hourihane, alanh@fairlite.demon.co.uk
 * 
 * DEC TGA accelerated options.
 */

#include "fb.h"
#include "micmap.h"
#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86PciInfo.h"
#include "xf86Pci.h"
#include "xf86cmap.h"
#include "mipointer.h"

#include "mibstore.h"
#include "miline.h"

#include "tga_regs.h"
#include "BT.h"
#include "tga.h"

/* defines */

#define BLIT_FORWARDS 0
#define BLIT_BACKWARDS 1
#define USE_BLOCK_FILL 2
#define USE_OPAQUE_FILL 3
#define MIX_SRC 0x03

#define CE_BUFSIZE 256

#define FB_OFFSET(x, y) (((long)(y) * pScrn->displayWidth * (pTga->Bpp)) + (long)(x) * pTga->Bpp)

/* prototypes */

static void TGACopyLineForwards(ScrnInfoPtr pScrn, int x1, int y1, int x2,
			 int y2, int w);
static void TGACopyLineBackwards(ScrnInfoPtr pScrn, int x1, int y1, int x2,
			  int y2, int w);
extern void TGASync(ScrnInfoPtr pScrn);
static void TGASetupForSolidFill(ScrnInfoPtr pScrn, int color, int rop,
			  unsigned int planemask);
static void TGASubsequentSolidFillRect(ScrnInfoPtr pScrn, int x, int y, int w, int h);
static void TGASetupForScreenToScreenCopy(ScrnInfoPtr pScrn, int xdir, int ydir,
				   int rop, unsigned int planemask,
				   int transparency_color);
static void TGASubsequentScreenToScreenCopy(ScrnInfoPtr pScrn, int x1, int y1,
				     int x2, int y2, int w, int h);
static void TGASetupForMono8x8PatternFill(ScrnInfoPtr pScrn, int patx, int paty,
				   int fg, int bg, int rop,
				   unsigned int planemask);
static void TGASubsequentMono8x8PatternFillRect(ScrnInfoPtr pScrn, int patx,
					 int paty, int x, int y, int w,
					 int h);
static void TGASetupForScanlineCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
							  int fg, int bg,
							  int rop,
							  unsigned int planemask);

static void
TGASubsequentScanlineCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
						int x, int y, int w,
						int h, int skipleft);
static void
TGASubsequentColorExpandScanline(ScrnInfoPtr pScrn, int bufno);


/*
 * The following function sets up the supported acceleration. Call it
 * from the FbInit() function in the SVGA driver.
 */
Bool
DEC21030AccelInit(ScreenPtr pScreen)
{
  XAAInfoRecPtr TGA_AccelInfoRec;
  BoxRec AvailFBArea;
  ScrnInfoPtr pScrn;
  TGAPtr pTga;

  pScrn = xf86Screens[pScreen->myNum];
  pTga = TGAPTR(pScrn);

  /*  ErrorF("DEC21030AccelInit called!"); */
  
  /* first, create the XAAInfoRec */
  TGA_AccelInfoRec = XAACreateInfoRec();

  /*  ErrorF("XAACreateInfoRec called"); */

  if(pScrn->depth == 8) {
    pTga->depthflag = BPP8PACKED;
    pTga->Bpp = 1;
  } else {
    pTga->depthflag = BPP24;
    pTga->Bpp = 4;
  }
 
  TGA_AccelInfoRec->Flags =  PIXMAP_CACHE | LINEAR_FRAMEBUFFER |
    OFFSCREEN_PIXMAPS;
  
  TGA_AccelInfoRec->Sync = TGASync;

  /* solid fill */
  
  TGA_AccelInfoRec->SolidFillFlags = 0;
  TGA_AccelInfoRec->SetupForSolidFill = TGASetupForSolidFill;
  TGA_AccelInfoRec->SubsequentSolidFillRect = TGASubsequentSolidFillRect;

  /* screen to screen copy */
  TGA_AccelInfoRec->ScreenToScreenCopyFlags = NO_TRANSPARENCY;
  TGA_AccelInfoRec->SetupForScreenToScreenCopy =
    TGASetupForScreenToScreenCopy;
  TGA_AccelInfoRec->SubsequentScreenToScreenCopy =
    TGASubsequentScreenToScreenCopy;

  /* mono 8x8 pattern fill */
  
  TGA_AccelInfoRec->Mono8x8PatternFillFlags =
    HARDWARE_PATTERN_PROGRAMMED_BITS | BIT_ORDER_IN_BYTE_LSBFIRST;
  TGA_AccelInfoRec->SetupForMono8x8PatternFill =
    TGASetupForMono8x8PatternFill;
  TGA_AccelInfoRec->SubsequentMono8x8PatternFillRect =
    TGASubsequentMono8x8PatternFillRect;

  /* color expand */
  /* does not work for 32bpp (yet) */
  TGA_AccelInfoRec->ScanlineCPUToScreenColorExpandFillFlags =
    BIT_ORDER_IN_BYTE_LSBFIRST;

  TGA_AccelInfoRec->NumScanlineColorExpandBuffers = 1;
  pTga->buffers[0] = (CARD32 *)xnfalloc(CE_BUFSIZE);
  TGA_AccelInfoRec->ScanlineColorExpandBuffers =
    (unsigned char **)pTga->buffers;
  TGA_AccelInfoRec->SetupForScanlineCPUToScreenColorExpandFill =
    TGASetupForScanlineCPUToScreenColorExpandFill;
  TGA_AccelInfoRec->SubsequentScanlineCPUToScreenColorExpandFill = 
    TGASubsequentScanlineCPUToScreenColorExpandFill;
  TGA_AccelInfoRec->SubsequentColorExpandScanline = 
    TGASubsequentColorExpandScanline;

  /* lines */
  
  TGA_AccelInfoRec->PolylinesThinSolid = TGAPolyLines;
  if(pTga->NoXaaPolySegment == FALSE)
    TGA_AccelInfoRec->PolySegmentThinSolid = TGAPolySegment;
  TGA_AccelInfoRec->PolylinesThinSolidFlags = 0x0;
  TGA_AccelInfoRec->PolySegmentThinSolidFlags = 0x0;

  TGA_AccelInfoRec->PolylinesThinDashed = TGAPolyLinesDashed;
  if(pTga->NoXaaPolySegment == FALSE)
    TGA_AccelInfoRec->PolySegmentThinDashed = TGAPolySegmentDashed;
  TGA_AccelInfoRec->PolylinesThinDashedFlags = 0x0;
  TGA_AccelInfoRec->PolySegmentThinDashedFlags = 0x0;
  TGA_AccelInfoRec->DashedLineFlags = LINE_PATTERN_LSBFIRST_LSBJUSTIFIED;
  TGA_AccelInfoRec->DashPatternMaxLength = 16;

  /* initialize the pixmap cache */
    
  AvailFBArea.x1 = 0;
  AvailFBArea.y1 = 0; /* these gotta be 0 */
  AvailFBArea.x2 = pScrn->displayWidth;
  AvailFBArea.y2 = (pScrn->videoRam * 1024) / (pScrn->displayWidth *
					       pTga->Bpp);
  xf86InitFBManager(pScreen, &AvailFBArea);

  TGA_AccelInfoRec->PixmapCacheFlags = 0;

  /* initialize XAA */
  return(XAAInit(pScreen, TGA_AccelInfoRec));
}

static void
TGASetupForScanlineCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
					      int fg, int bg, int rop,
					      unsigned int planemask)
{
#ifdef PROFILE
    unsigned int start, stop;
#endif
    TGAPtr pTga = NULL;
    unsigned int fgcolor = 0, bgcolor = 0, pmask = 0;
    TGA_DECL();

    pTga = TGAPTR(pScrn);
    TGA_GET_IOBASE();
    TGA_GET_OFFSET();

/*      ErrorF("TGASetupForScanlineCPUToScreenColorExpandFill called\n"); */
    if(pTga->depthflag == BPP8PACKED) {
      fgcolor = (fg | (fg << 8) | (fg << 16) | (fg << 24));
      bgcolor = bg | (bg << 8) | (bg << 16) | (bg << 24);
      pmask = planemask | (planemask << 8) | (planemask << 16)
	| (planemask << 24);
    }
    else {
      bgcolor = bg;
      fgcolor = fg;
      pmask = planemask;
    }
    pTga->current_rop = rop | pTga->depthflag;
    
    if(bg == -1) {
	pTga->transparent_pattern_p = 1;
	if(rop == MIX_SRC) {
	  pTga->block_or_opaque_p = USE_BLOCK_FILL;
	  TGA_FAST_WRITE_REG(fgcolor, TGA_BLOCK_COLOR0_REG);
	  TGA_FAST_WRITE_REG(fgcolor, TGA_BLOCK_COLOR1_REG);
	  if(pTga->depthflag == BPP24) {
	    TGA_FAST_WRITE_REG(fgcolor, TGA_BLOCK_COLOR2_REG);
	    TGA_FAST_WRITE_REG(fgcolor, TGA_BLOCK_COLOR3_REG);
	    TGA_FAST_WRITE_REG(fgcolor, TGA_BLOCK_COLOR4_REG);
	    TGA_FAST_WRITE_REG(fgcolor, TGA_BLOCK_COLOR5_REG);
	    TGA_FAST_WRITE_REG(fgcolor, TGA_BLOCK_COLOR6_REG);
	    TGA_FAST_WRITE_REG(fgcolor, TGA_BLOCK_COLOR7_REG);
	  }
	}
	else {
	  pTga->block_or_opaque_p = USE_OPAQUE_FILL;
	  TGA_FAST_WRITE_REG(fgcolor, TGA_FOREGROUND_REG);
	}
    }
    else {
	pTga->transparent_pattern_p = 0;
	TGA_FAST_WRITE_REG(bgcolor, TGA_BACKGROUND_REG);
	TGA_FAST_WRITE_REG(fgcolor, TGA_FOREGROUND_REG);
    }
    TGA_FAST_WRITE_REG(pmask, TGA_PLANEMASK_REG); /* we know when to
						     reset this */
    TGA_FAST_WRITE_REG(pTga->current_rop, TGA_RASTEROP_REG);

    TGA_SAVE_OFFSET();
    
    return;
}

static void
TGASubsequentScanlineCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
						int x, int y, int w,
						int h, int skipleft)
{
#ifdef PROFILE
    unsigned int start, stop;
#endif
    TGAPtr pTga;
    TGA_DECL();

    pTga = TGAPTR(pScrn);
    TGA_GET_IOBASE();
    TGA_GET_OFFSET();

/*      ErrorF("TGASubsequentScanlineCPUToScreenColorExpandFill called\n"); */
/*      ErrorF("w = %d, h = %d\n", w, h); */
    
    pTga->ce_height = h;
    pTga->ce_width = w;
    pTga->ce_x = x;
    pTga->ce_y = y;
    pTga->ce_skipleft = skipleft;
/*      ErrorF("skipleft is %d\n", skipleft); */

    if(pTga->transparent_pattern_p) {
	if(pTga->block_or_opaque_p == USE_BLOCK_FILL)
	    TGA_FAST_WRITE_REG(BLOCKSTIPPLE | X11 | pTga->depthflag,
			       TGA_MODE_REG);
	else
	    TGA_FAST_WRITE_REG(TRANSPARENTSTIPPLE | X11 | pTga->depthflag,
			       TGA_MODE_REG);
/*  	ErrorF("transparent stipple with x = %d, y = %d, w = %d, h = %d\n", */
/*  	       x, y, w, h); */
    }
    else
	TGA_FAST_WRITE_REG(OPAQUESTIPPLE | X11 | pTga->depthflag,
			   TGA_MODE_REG);
    
    TGA_SAVE_OFFSET();
    return;
}

static void
TGASubsequentColorExpandScanline(ScrnInfoPtr pScrn, int bufno)
{
#ifdef PROFILE
    unsigned int start, stop;
#endif
    TGAPtr pTga;
    unsigned char *p = NULL;
    int width = 0;
    unsigned int addr;
    unsigned int pixelmask = 0;
    unsigned int stipple;
    unsigned int align_mask;
    int align = 0;
    int skipleft;

    CARD32 c = 0, d = 0;
    CARD32 *e = NULL;
    int i = 0, num_dwords = 0;
    TGA_DECL();
    
    pTga = TGAPTR(pScrn);
    TGA_GET_IOBASE();
    TGA_GET_OFFSET();

    align_mask = (pTga->depthflag == BPP24) ? 0x0f : 0x03;

#if 0
    ErrorF("TGASubsequentColorExpandScanline called\n");
    if(pTga->transparent_pattern_p)
      ErrorF("transparent color expand\n");
#endif

    p = (unsigned char *)pTga->buffers[0];
    addr = FB_OFFSET(pTga->ce_x, pTga->ce_y);
    width = pTga->ce_width;
    skipleft = pTga->ce_skipleft;

    while(width > 0) {
	if(!pTga->transparent_pattern_p)
	    pixelmask = 0xFFFFFFFF;
	
	align = (addr & align_mask) / pTga->Bpp; /* no. pixels out of align */
	if (align) {
	    if (!pTga->transparent_pattern_p)
		pixelmask <<= align;
/*  	    ErrorF("alignment is %d\n", align); */
	    addr -= align * pTga->Bpp;
	    width += align;

	    e = (CARD32 *)p;
	    num_dwords = (width / 32) + 1;
	    if(num_dwords > (CE_BUFSIZE / 4)) { /* shouldn't happen */
		ErrorF("TGASubsequentColorExpandScanline passed scanline %d bytes long, truncating\n", num_dwords * 4);
		num_dwords = CE_BUFSIZE / 4;
	    }
	    for(i = 0; i < num_dwords; i++) {
		c = e[i];
		if(i == 0)
		    e[i] = c << align;
		else
		    e[i] = (d >> (32 - align)) | (c << align);
		d = c;
	    }
	}
	
	if (!pTga->transparent_pattern_p) {
	    if (skipleft) {
		pixelmask <<= skipleft;
		skipleft = 0;
	    }
	    if (width < 32) {
		pixelmask &= (0xFFFFFFFF >> (32 - width));
	    }
	    TGA_FAST_WRITE_REG(pixelmask, TGA_PIXELMASK_REG);
	}
	else {
	    unsigned int *i = NULL;
	
/*  	    ErrorF("transparent scanline with x = %d, y = %d, w = %d, h = %d\n",  pTga->ce_x, pTga->ce_y, pTga->ce_width, pTga->ce_height); */
	    if (skipleft) {
		i = (unsigned int *)p;
		*i &= (0xFFFFFFFF << skipleft);
		skipleft = 0;
	    }
	    if (width < 32) {
		i = (unsigned int *)p;
		*i &= (0xFFFFFFFF >> (32 - width));
	    }
	}
    
	stipple = *((unsigned int *)p);
	switch (pTga->Chipset) {
	case PCI_CHIP_TGA2:
	    *(unsigned int *)(pTga->FbBase + addr) = stipple; WMB;
	    break;
	case PCI_CHIP_DEC21030:
	    TGA_FAST_WRITE_REG(addr, TGA_ADDRESS_REG);
	    TGA_FAST_WRITE_REG(stipple, TGA_CONTINUE_REG);
	}
	addr += 32 * pTga->Bpp;
	p += 4;
	width -= 32;
    }
    pTga->ce_height--;
    if(pTga->ce_height == 0) {
	TGA_FAST_WRITE_REG(SIMPLE | X11 | pTga->depthflag,
			   TGA_MODE_REG);
	TGA_FAST_WRITE_REG(MIX_SRC | pTga->depthflag, TGA_RASTEROP_REG);
	TGA_FAST_WRITE_REG(0xFFFFFFFF, TGA_PLANEMASK_REG);
    }
    else
	pTga->ce_y += 1;
    
    TGA_SAVE_OFFSET();
    return;
}

/* Block Fill mode is faster, but only works for certain rops.  So we will
   have to implement Opaque Fill anyway, so we will do that first, then
   do Block Fill for the special cases
*/
void
TGASetupForSolidFill(ScrnInfoPtr pScrn, int color, int rop,
		     unsigned int planemask)
{
#ifdef PROFILE
  unsigned int start, stop;
#endif
  TGAPtr pTga;
  unsigned int fgcolor = 0, pmask = 0;
  TGA_DECL();
  
  pTga = TGAPTR(pScrn);
  TGA_GET_IOBASE();
  TGA_GET_OFFSET();
  /*  ErrorF("TGASetupForSolidFill called"); */

  if(pTga->depthflag == BPP8PACKED) {
    fgcolor = color | (color << 8) | (color << 16) | (color << 24);
    pmask = planemask | (planemask << 8) | (planemask << 16) |
		     (planemask << 24);
  }
  else {
    fgcolor = color;
    pmask = planemask;
  }
    
  
  if(rop == MIX_SRC) { /* we can just do a block copy */
    pTga->block_or_opaque_p = USE_BLOCK_FILL;
    TGA_FAST_WRITE_REG(fgcolor, TGA_BLOCK_COLOR0_REG);
    TGA_FAST_WRITE_REG(fgcolor, TGA_BLOCK_COLOR1_REG);
    if(pTga->depthflag == BPP24) {
      TGA_FAST_WRITE_REG(fgcolor, TGA_BLOCK_COLOR2_REG);
      TGA_FAST_WRITE_REG(fgcolor, TGA_BLOCK_COLOR3_REG);
      TGA_FAST_WRITE_REG(fgcolor, TGA_BLOCK_COLOR4_REG);
      TGA_FAST_WRITE_REG(fgcolor, TGA_BLOCK_COLOR5_REG);
      TGA_FAST_WRITE_REG(fgcolor, TGA_BLOCK_COLOR6_REG);
      TGA_FAST_WRITE_REG(fgcolor, TGA_BLOCK_COLOR7_REG);
    }
  }
  else {
    pTga->block_or_opaque_p = USE_OPAQUE_FILL;
    pTga->current_rop = rop | pTga->depthflag;
    TGA_FAST_WRITE_REG(fgcolor, TGA_FOREGROUND_REG);
/*      ErrorF("opaque fill called\n"); */
  }

  pTga->current_planemask = pmask;
  TGA_FAST_WRITE_REG(0xFFFFFFFF, TGA_DATA_REG);
  TGA_SAVE_OFFSET();
  return;
}

void
TGASubsequentSolidFillRect(ScrnInfoPtr pScrn, int x, int y, int w, int h)
{
  unsigned int mode_reg = 0;
  int i = 0;
  unsigned int pixel_count = 0; /* the actual # of pixels to be written */
  unsigned int write_data = 0; /* the actual data written */
  int a1 = 0;
#ifdef PROFILE
  unsigned int stop, start;
#endif
  TGAPtr pTga;
  TGA_DECL();

  pTga = TGAPTR(pScrn);
  TGA_GET_IOBASE();
  TGA_GET_OFFSET();
  
  /*  ErrorF("TGASubsequentFillRectSolid called\n"); */

  if(pTga->block_or_opaque_p == USE_OPAQUE_FILL) {
    mode_reg = OPAQUEFILL | X11 | pTga->depthflag;
    TGA_FAST_WRITE_REG(pTga->current_rop, TGA_RASTEROP_REG);
    /* we have to set this to GXCOPY every time before we exit */
  }
  else
    mode_reg = BLOCKFILL | X11 | pTga->depthflag;
  
  TGA_FAST_WRITE_REG(mode_reg, TGA_MODE_REG);
  TGA_FAST_WRITE_REG(pTga->current_planemask, TGA_PLANEMASK_REG);

  if(w > 2048) {
    ErrorF("TGASubsequentSolidFillRect called with w = %d, truncating.\n");
    w = 2048;
  }
  pixel_count = w - 1;
  
  for(i = 0; i < h; i++) {
    a1 = FB_OFFSET(x, y + i);
    if(pTga->block_or_opaque_p == USE_OPAQUE_FILL)
      TGA_FAST_WRITE_REG(0xFFFFFFFF, TGA_PIXELMASK_REG);
    write_data = pixel_count;
    TGA_FAST_WRITE_REG(a1, TGA_ADDRESS_REG);
    TGA_FAST_WRITE_REG(write_data, TGA_CONTINUE_REG);
  }
    
  mode_reg = SIMPLE | X11 | pTga->depthflag;
  TGA_FAST_WRITE_REG(mode_reg, TGA_MODE_REG);
  if(pTga->block_or_opaque_p == USE_OPAQUE_FILL)
    TGA_FAST_WRITE_REG(MIX_SRC | pTga->depthflag,
		       TGA_RASTEROP_REG); /* GXCOPY */
  TGA_FAST_WRITE_REG(0xFFFFFFFF, TGA_PLANEMASK_REG);

  TGA_SAVE_OFFSET();  
  return;
}

/* we only need to calculate the direction of a move once per move,
   so we do it in the setup function and leave it */

void
TGASetupForScreenToScreenCopy(ScrnInfoPtr pScrn, int xdir, int ydir,
			      int rop, unsigned int planemask,
			      int transparency_color)
    /* xdir 1 = left-to-right, -1 = right to left
       ydir 1 = top-to-bottom, -1 = bottom to top
    */
{
#ifdef PROFILE
  unsigned int start, stop;
#endif
  TGAPtr pTga;
  unsigned int pmask = 0;
  TGA_DECL();

  pTga = TGAPTR(pScrn);
  TGA_GET_IOBASE();
  TGA_GET_OFFSET();
  
  /* see section 6.2.9 */

  if (pTga->depthflag == BPP8PACKED) {
    pmask = planemask | (planemask << 8) | (planemask << 16) |
		 (planemask << 24);
  }
  else
    pmask = planemask;

  pTga->current_planemask = pmask;
  TGA_FAST_WRITE_REG(pmask, TGA_PLANEMASK_REG);

  pTga->current_rop = rop | pTga->depthflag;

  /* do we copy a rectangle from top to bottom or bottom to top? */
  if (ydir == -1) {
    pTga->blitdir = BLIT_FORWARDS;
  }
  else {
    pTga->blitdir = BLIT_BACKWARDS;
  }
  TGA_SAVE_OFFSET();
  return;
}

/*
 * This is the implementation of the SubsequentForScreenToScreenCopy
 * that sends commands to the coprocessor to perform a screen-to-screen
 * copy of the specified areas, with the parameters from the SetUp call.
 */
void
TGASubsequentScreenToScreenCopy(ScrnInfoPtr pScrn, int x1, int y1, int x2,
				int y2, int w, int h)
{
  /* x1, y1 = source coords
     x2, y2 = destination coords
     w = width
     h = height
  */

  int i = 0;
  void (*copy_func)(ScrnInfoPtr pScrn, int x1, int y1, int x2, int y2, int w);
#ifdef PROFILE
  unsigned int stop, start;
#endif
  TGAPtr pTga;
  TGA_DECL();

  pTga = TGAPTR(pScrn);
  TGA_GET_IOBASE();
  TGA_GET_OFFSET();
#if 0
  ErrorF("TGASubsequentScreenToScreenCopy(,%d,%d,%d,%d,%d,%d):"
	 " COPY %s BLIT %s\n",
	 x1, y1, x2, y2, w, h, (x2 > x1 && (x1 + w) > x2)?"BWD":"FWD",
	 (pTga->blitdir == BLIT_FORWARDS)?"FWD":"BWD");
#endif
  TGASync(pScrn); /* ?? */

  TGA_FAST_WRITE_REG(COPY | X11 | pTga->depthflag, TGA_MODE_REG);
  TGA_FAST_WRITE_REG(pTga->current_rop, TGA_RASTEROP_REG);
  TGA_FAST_WRITE_REG(pTga->current_planemask, TGA_PLANEMASK_REG);

  if(x2 > x1 && (x1 + w) > x2)
    copy_func = TGACopyLineBackwards;
  else 
    copy_func = TGACopyLineForwards; 

  TGA_SAVE_OFFSET();  
  if(pTga->blitdir == BLIT_FORWARDS) {
    for(i = h - 1; i >= 0; i--) { /* copy from bottom to top */
      (*copy_func)(pScrn, x1, y1 + i, x2, y2 + i, w);
    }
  }
  else {
    for(i = 0; i < h; i++) {
      (*copy_func)(pScrn, x1, y1 + i, x2, y2 + i, w);
    }
  }

  TGASync(pScrn); /* ?? */

  TGA_GET_OFFSET();
  TGA_FAST_WRITE_REG(SIMPLE | X11 | pTga->depthflag, TGA_MODE_REG);
  TGA_FAST_WRITE_REG(MIX_SRC | pTga->depthflag, TGA_RASTEROP_REG);
  TGA_FAST_WRITE_REG(0xFFFFFFFF, TGA_PLANEMASK_REG);
  TGA_SAVE_OFFSET();
  
  return;
}


void
TGACopyLineForwards(ScrnInfoPtr pScrn, int x1, int y1, int x2, int y2, int w)
{
  /* copy a line of width w from x1,y1 to x2,y2 using copy mode */
  int read;
  unsigned long source_address, destination_address;
  unsigned int mask_source, mask_destination;
  int cando;
  unsigned int cando_mask;
  int source_align, destination_align;
  int pixel_shift;
#ifdef PROFILE
  unsigned int start, stop;
#endif
  TGAPtr pTga;
  TGA_DECL();

  pTga = TGAPTR(pScrn);
  TGA_GET_IOBASE();
  TGA_GET_OFFSET();

  cando = 32;
  cando_mask = 0xFFFFFFFFU;
  if (pTga->Chipset == PCI_CHIP_DEC21030 && pTga->depthflag == BPP24) {
    cando = 16;
    cando_mask = 0x0000FFFFU;
  }

  source_address = FB_OFFSET(x1, y1);
  destination_address = FB_OFFSET(x2, y2);
#if 0 
  ErrorF("CPY-FWD(,%d,%d,%d,%d,%d): sadr = 0x%lx, dadr = 0x%lx\n",
	 x1, y1, x2, y2, w, source_address, destination_address);
#endif
  read = 0;
  while (read < w) {

    mask_source = cando_mask;
    if ((w - read) >= cando)
      mask_destination = cando_mask;
    else
      mask_destination = cando_mask >> (cando - (w - read));

    source_align = source_address & 0x07;
    source_address -= source_align;
    mask_source <<= source_align / pTga->Bpp;
    /*    mask_source &= cando_mask; */

    destination_align = destination_address & 0x07;
    destination_address -= destination_align;
    mask_destination <<= destination_align / pTga->Bpp;
    /*    mask_destination &= cando_mask; */

    if (destination_align >= source_align)
      pixel_shift = destination_align - source_align;
    else {
      pixel_shift = 8 - (source_align - destination_align);
      /* we need to prime the residue register in this case */
      destination_address -= 8;
      mask_destination <<= 8 / pTga->Bpp;
      mask_destination &= cando_mask;/* ?? */
    }

    TGA_FAST_WRITE_REG(pixel_shift, TGA_PIXELSHIFT_REG);
    switch (pTga->Chipset) {
    case PCI_CHIP_TGA2:
        *(unsigned int *)(pTga->FbBase + source_address) = mask_source; WMB;
	*(unsigned int *)(pTga->FbBase + destination_address) = mask_destination; WMB;
	break;
    case PCI_CHIP_DEC21030:
        /* use GADR and GCTR */
        TGA_FAST_WRITE_REG(source_address, TGA_ADDRESS_REG);
	TGA_FAST_WRITE_REG(mask_source, TGA_CONTINUE_REG);
	TGA_FAST_WRITE_REG(destination_address, TGA_ADDRESS_REG);
	TGA_FAST_WRITE_REG(mask_destination, TGA_CONTINUE_REG);
	break;
    }
    
    source_address += (cando - (pixel_shift / pTga->Bpp)) * pTga->Bpp;
    destination_address += cando * pTga->Bpp;

    read += cando;
    read -= destination_align / pTga->Bpp; /* "read" is perhaps better
				    called "written"... */
    if (destination_align < source_align) {
      read -= 8 / pTga->Bpp;
    }
  }

  TGA_SAVE_OFFSET();
  return;
}

  
void
TGACopyLineBackwards(ScrnInfoPtr pScrn, int x1, int y1, int x2,
			  int y2, int w)
     /* x1, y1 = source
	x2, y2 = destination
	w = width
     */
{
  unsigned long a1, a2;
  unsigned long source_address, destination_address;
  unsigned int mask_source, mask_destination;
  int cando;
  unsigned int cando_mask;
  int source_align, destination_align;
  int pixel_shift;
  int read;
#ifdef PROFILE
  unsigned int start, stop;
#endif
  TGAPtr pTga;
  TGA_DECL();

  pTga = TGAPTR(pScrn);
  TGA_GET_IOBASE();
  TGA_GET_OFFSET();
  
  cando = 32;
  cando_mask = 0xFFFFFFFFU;
  if (pTga->Chipset == PCI_CHIP_DEC21030 && pTga->depthflag == BPP24) {
    cando = 16;
    cando_mask = 0x0000FFFFU;
  }

  a1 = FB_OFFSET(x1, y1);
  a2 = FB_OFFSET(x2, y2);
      
  source_address = FB_OFFSET((x1 + w) - cando, y1);
  destination_address = FB_OFFSET((x2 + w) - cando, y2);

#if 0
  ErrorF("CPY-BWD(,%d,%d,%d,%d,%d): sadr = 0x%lx, dadr = 0x%lx"
	 " a1 0x%lx a2 0x%lx\n",
	 x1, y1, x2, y2, w, source_address, destination_address, a1, a2);
#endif

  read = 0;
  while (read < w) {
    mask_source = cando_mask;
    if ((w - read) >= cando)
      mask_destination = cando_mask;
    else {
      mask_destination = ((unsigned int)cando_mask) << (cando - (w - read));
      mask_destination &= cando_mask; /* esp. for cando==16 */
    }

    source_align = source_address & 0x07;
    destination_align = destination_address & 0x07;

    if (read == 0 && destination_align &&
       (source_align > destination_align)) {
      /* we want to take out all the destination_align pixels in one
	 little copy first, then move on to the main stuff */
      unsigned long tmp_src, tmp_dest;
      unsigned int tmp_src_mask, tmp_dest_mask;
      
      tmp_src = a1 + (w - (source_align / pTga->Bpp)) * pTga->Bpp;
      tmp_dest = a2 + (w - (destination_align / pTga->Bpp) - (8 / pTga->Bpp)) * pTga->Bpp;
      tmp_src_mask = cando_mask;
      tmp_dest_mask = ((unsigned int)0x000000FF) >> (8 - destination_align) / pTga->Bpp;
      tmp_dest_mask <<= 8 / pTga->Bpp;
      pixel_shift = (8 - source_align) + destination_align;
#if 0
      ErrorF("CPY-BWD - preliminary copy: sa = %d, da = %d, ps =%d\n",
	     source_align, destination_align, pixel_shift);
#endif
      TGA_FAST_WRITE_REG(pixel_shift, TGA_PIXELSHIFT_REG);
      switch (pTga->Chipset)
      {
      case PCI_CHIP_TGA2:
	  *(unsigned int *)(pTga->FbBase + tmp_src) = tmp_src_mask; WMB;
	  *(unsigned int *)(pTga->FbBase + tmp_dest) = tmp_dest_mask; WMB;
	  break;
      case PCI_CHIP_DEC21030:
	  /* use GADR and GCTR */
	  TGA_FAST_WRITE_REG(tmp_src, TGA_ADDRESS_REG);
	  TGA_FAST_WRITE_REG(tmp_src_mask, TGA_CONTINUE_REG);
	  TGA_FAST_WRITE_REG(tmp_dest, TGA_ADDRESS_REG);
	  TGA_FAST_WRITE_REG(tmp_dest_mask, TGA_CONTINUE_REG);
	  break;
      }

      source_address += (8 - source_align);
      mask_source >>= (8 - source_align) / pTga->Bpp;
      mask_source >>= destination_align / pTga->Bpp;
      mask_destination >>= destination_align / pTga->Bpp;
    }
    else if (read == 0 && (source_align != destination_align)) {
      source_address += (8 - source_align);
      /*    	mask_source >>= (8 - source_align); */
      /* if we uncomment this, it breaks...TGA tries to
	 optimize away a read of our last pixels... */
    }
    else if (source_align) {
      source_address += (8 - source_align);
      mask_source >>= (8 - source_align) / pTga->Bpp;
    }

    if (destination_align) {
      destination_address += (8 - destination_align);
      mask_destination >>= (8 - destination_align) / pTga->Bpp;
    }

    if (destination_align >= source_align)
      pixel_shift = destination_align - source_align;
    else {
      pixel_shift = (8 - source_align) + destination_align;
      if (destination_align) {
	source_address += 8;
	mask_source >>= 8 / pTga->Bpp;
      }
    }

#if 0
    ErrorF("CPY-BWD - normal: sadr 0x%lx sm 0x%x dadr 0x%lx dm 0x%x"
	   " sa %d da %d ps %d read %d\n",
	   source_address, mask_source,
	   destination_address, mask_destination,
	   source_align, destination_align, pixel_shift, read);
#endif
    TGA_FAST_WRITE_REG(pixel_shift, TGA_PIXELSHIFT_REG);
    switch (pTga->Chipset) {
    case PCI_CHIP_TGA2:
        *(unsigned int *)(pTga->FbBase + source_address) = mask_source; WMB;
	*(unsigned int *)(pTga->FbBase + destination_address) = mask_destination; WMB;
	break;
    case PCI_CHIP_DEC21030:
        /* use GADR and GCTR */
        TGA_FAST_WRITE_REG(source_address, TGA_ADDRESS_REG);
	TGA_FAST_WRITE_REG(mask_source, TGA_CONTINUE_REG);
	TGA_FAST_WRITE_REG(destination_address, TGA_ADDRESS_REG);
	TGA_FAST_WRITE_REG(mask_destination, TGA_CONTINUE_REG);
	break;
    }

/*      if(read == 0) */
/*        ErrorF("sa = %d, da = %d, ps = %d\n", source_align, destination_align, */
/*      	     pixel_shift); */
    
    if (destination_align > source_align) {
      source_address -= cando * pTga->Bpp - 8;
      destination_address -= (cando - (pixel_shift / pTga->Bpp)) * pTga->Bpp;
      if (read == 0)
	read += (cando - 8 / pTga->Bpp) + source_align / pTga->Bpp;
      else 
	read += cando - 8 / pTga->Bpp;      
    }
    else if (destination_align == source_align) {
      source_address -= cando * pTga->Bpp;
      destination_address -= cando * pTga->Bpp;
      if (read == 0 && destination_align)
	read += (cando - (8 - destination_align) / pTga->Bpp);
      else
	read += cando;
    }
    else if (source_align > destination_align) {
      source_address -= cando * pTga->Bpp - 8;
      destination_address -= (cando - (pixel_shift / pTga->Bpp)) * pTga->Bpp;
      /* only happens when read == 0 */
      if (destination_align)
	read += (cando - 16 / pTga->Bpp) + source_align / pTga->Bpp;
      else
	read += cando - pixel_shift / pTga->Bpp;
    }
  }

  TGA_SAVE_OFFSET();
  return;
}

void
TGASetupForMono8x8PatternFill(ScrnInfoPtr pScrn, int patx, int paty,
			      int fg, int bg, int rop, unsigned int planemask)
{
#ifdef PROFILE
  unsigned int start, stop;
#endif
  TGAPtr pTga;
  unsigned int fgcolor = 0, bgcolor = 0, pmask = 0;
  TGA_DECL();

  pTga = TGAPTR(pScrn);
  TGA_GET_IOBASE();
  TGA_GET_OFFSET();

/*    ErrorF("TGASetupForMono8x8PatternFill called with patx = %d, paty = %d, fg = %d, bg = %d, rop = %d, planemask = %d\n", */
/*  	 patx, paty, fg, bg, rop, planemask);  */
  
  if(bg == -1)  /* we are transparent */
    pTga->transparent_pattern_p = 1;
  else
    pTga->transparent_pattern_p = 0;

  if(rop == MIX_SRC)
    pTga->block_or_opaque_p = USE_BLOCK_FILL;
  else
    pTga->block_or_opaque_p = USE_OPAQUE_FILL;

  if(pTga->depthflag == BPP8PACKED) {
    fgcolor = fg | (fg << 8) | (fg << 16) | (fg << 24);
    bgcolor = bg | (bg << 8) | (bg << 16) | (bg << 24);
    pmask = planemask | (planemask << 8) | (planemask << 16) |
      (planemask << 24);
  }
  else {
    fgcolor = fg;
    bgcolor = bg;
    pmask = planemask;
  }
   

  if(pTga->transparent_pattern_p &&
     pTga->block_or_opaque_p == USE_BLOCK_FILL) {
    /* we can use block fill mode to draw a transparent stipple */
    TGA_FAST_WRITE_REG(fgcolor, TGA_BLOCK_COLOR0_REG);
    TGA_FAST_WRITE_REG(fgcolor, TGA_BLOCK_COLOR1_REG);
    if(pTga->depthflag == BPP24) {
      TGA_FAST_WRITE_REG(fgcolor, TGA_BLOCK_COLOR2_REG);
      TGA_FAST_WRITE_REG(fgcolor, TGA_BLOCK_COLOR3_REG);
      TGA_FAST_WRITE_REG(fgcolor, TGA_BLOCK_COLOR4_REG);
      TGA_FAST_WRITE_REG(fgcolor, TGA_BLOCK_COLOR5_REG);
      TGA_FAST_WRITE_REG(fgcolor, TGA_BLOCK_COLOR6_REG);
      TGA_FAST_WRITE_REG(fgcolor, TGA_BLOCK_COLOR7_REG);
    }
  }
  else if(pTga->transparent_pattern_p) {
    TGA_FAST_WRITE_REG(fgcolor, TGA_FOREGROUND_REG);
  }
  else {
    TGA_FAST_WRITE_REG(bgcolor, TGA_BACKGROUND_REG);
    TGA_FAST_WRITE_REG(fgcolor, TGA_FOREGROUND_REG);
    TGA_FAST_WRITE_REG(0xFFFFFFFF, TGA_PIXELMASK_REG);
  }
  pTga->current_rop = rop;
  pTga->current_planemask = pmask;
  TGA_SAVE_OFFSET();
  return;
}

void
TGASubsequentMono8x8PatternFillRect(ScrnInfoPtr pScrn, int patx, int paty,
				    int x, int y, int w, int h)
/* patx and paty = first & second dwords of pattern to be rendered, packed */
{
  TGAPtr pTga;
  int i, j;
  unsigned int stipple_mask[8], align, tmp;
#ifdef PROFILE
  register unsigned int stop, start;
#endif
  TGA_DECL();


/*    ErrorF("TGASubsequentMono8x8PatternFillRect called with x = %d, y = %d, w = %d, h = %d\n", x, y, w, h); */
  
  pTga = TGAPTR(pScrn);
  TGA_GET_IOBASE();
  TGA_GET_OFFSET();

  if(w > 2048)
    ErrorF("TGASubsequentMono8x8PatternFillRect called with w > 2048, truncating\n");
  if(pTga->block_or_opaque_p == USE_OPAQUE_FILL)
    TGA_FAST_WRITE_REG(pTga->current_rop, TGA_RASTEROP_REG);

  TGA_FAST_WRITE_REG(pTga->current_planemask, TGA_PLANEMASK_REG);
  if(pTga->depthflag == BPP8PACKED)
    align = FB_OFFSET(x, y) % 4;
  else
    align = x % 4;

  for(i = 0; i < 4; i++) {
    tmp = (patx >> (i * 8)) & 0xFF;
    stipple_mask[i] = (tmp | (tmp << 8) | (tmp << 16) | (tmp << 24));
  }
  for(i = 4; i < 8; i++) {
    tmp = (paty >> ((i - 4) * 8)) & 0xFF;
    stipple_mask[i] = (tmp | (tmp << 8) | (tmp << 16) | (tmp << 24));
  }
  if(align) { /* stipples must be aligned to four bytes */
    for(i = 0; i < 8; i++) {
      stipple_mask[i] = (stipple_mask[i] << align) |
	((stipple_mask[i] & 0xFF000000) >> (32 - align));
    }
  }

  if((pTga->block_or_opaque_p == USE_BLOCK_FILL) && pTga->transparent_pattern_p) {
    /* use block fill */
    TGA_FAST_WRITE_REG(BLOCKFILL | X11 | pTga->depthflag, TGA_MODE_REG);

    for(i = 0, j = 0; i < h; i++, (j == 7) ? (j = 0) : (j++)) { 
      TGA_FAST_WRITE_REG(stipple_mask[j], TGA_DATA_REG);
      TGA_FAST_WRITE_REG(FB_OFFSET(x, y + i), TGA_ADDRESS_REG);
      TGA_FAST_WRITE_REG(w - 1, TGA_CONTINUE_REG);
    }
  }
  else if(pTga->transparent_pattern_p) {
    /* if we can't use block fill, we'll use transparent fill */
    TGA_FAST_WRITE_REG(TRANSPARENTFILL | X11 | pTga->depthflag, TGA_MODE_REG);
    for(i = 0, j = 0; i < h; i++, (j == 7) ? (j = 0) : (j++)) { 
      TGA_FAST_WRITE_REG(stipple_mask[j], TGA_DATA_REG);
      TGA_FAST_WRITE_REG(FB_OFFSET(x, y + i), TGA_ADDRESS_REG);
      TGA_FAST_WRITE_REG(w - 1, TGA_CONTINUE_REG);
    }
  }
  else { /* use opaque fill mode */
/*      ErrorF("Using opaque fill mode\n"); */
    TGA_FAST_WRITE_REG(OPAQUEFILL | X11 | pTga->depthflag, TGA_MODE_REG);
    for(i = 0, j = 0; i < h; i++, (j == 7) ? (j = 0) : (j++)) { 
      TGA_FAST_WRITE_REG(stipple_mask[j], TGA_DATA_REG);
      TGA_FAST_WRITE_REG(FB_OFFSET(x, y + i), TGA_ADDRESS_REG);
      TGA_FAST_WRITE_REG(w - 1, TGA_CONTINUE_REG); 
    }
  }
  
  TGA_FAST_WRITE_REG(SIMPLE | X11 | pTga->depthflag, TGA_MODE_REG);
  TGA_FAST_WRITE_REG(MIX_SRC | pTga->depthflag, TGA_RASTEROP_REG);
  TGA_FAST_WRITE_REG(0xFFFFFFFF, TGA_PLANEMASK_REG);

  TGA_SAVE_OFFSET();

  return;
}

void
TGASetupForSolidLine(ScrnInfoPtr pScrn, int color, int rop,
		     unsigned int planemask)
{
#ifdef PROFILE
  unsigned int start, stop;
#endif
  TGAPtr pTga = NULL;
  unsigned int fgcolor = 0, pmask = 0;
  TGA_DECL();
  
  pTga = TGAPTR(pScrn);
  TGA_GET_IOBASE();
  TGA_GET_OFFSET();
/*    ErrorF("TGASetupForSolidLine called\n"); */

  if(pTga->depthflag == BPP8PACKED) {
    fgcolor = color | (color << 8) | (color << 16) | (color << 24);
    pmask = planemask | (planemask << 8) | (planemask << 16) |
      (planemask << 24);
  }
  else {
    fgcolor = color;
    pmask = planemask;
  }
    
  
  pTga->current_rop = rop | pTga->depthflag;
  TGA_FAST_WRITE_REG(fgcolor, TGA_FOREGROUND_REG);
  pTga->current_planemask = pmask;
  TGA_FAST_WRITE_REG(0xFFFF, TGA_DATA_REG);
  TGA_FAST_WRITE_REG(pScrn->displayWidth, TGA_WIDTH_REG);
  TGA_SAVE_OFFSET();

  return;
}

void
TGASubsequentSolidHorVertLine(ScrnInfoPtr pScrn, int x, int y, int len,
			      int dir)
{
  if(dir == DEGREES_0)  /* line is to the right */
      TGASubsequentSolidLine(pScrn, x, y, x + len, y, 0x0, OMIT_LAST);
  else if(dir == DEGREES_270) /* line is down */
      TGASubsequentSolidLine(pScrn, x, y, x, y + len, YMAJOR, OMIT_LAST);
  else
      ErrorF("TGASubsequentSolidHorVertLine passed dir %d!\n", dir);

  return;
}

void
TGASubsequentSolidLine(ScrnInfoPtr pScrn, int x1, int y1, int x2, int y2,
		       int octant, int flags)
{
#ifdef PROFILE
  unsigned int start, stop;
#endif
  TGAPtr pTga = NULL;
  CARD32 abs_dx = 0, abs_dy = 0, address = 0, octant_reg = 0;
  int length = 0;
  TGA_DECL();

  pTga = TGAPTR(pScrn);
  TGA_GET_IOBASE();
  TGA_GET_OFFSET();
/*    ErrorF("TGASubsequentSolidLine called\n"); */

  TGA_FAST_WRITE_REG(pTga->current_rop, TGA_RASTEROP_REG);
  TGA_FAST_WRITE_REG(OPAQUELINE | X11 | pTga->depthflag |
		     ((flags & OMIT_LAST) ? 0x0 : CAP_ENDS), TGA_MODE_REG);
  TGA_FAST_WRITE_REG(pTga->current_planemask, TGA_PLANEMASK_REG);

  address = FB_OFFSET(x1, y1);
  TGA_FAST_WRITE_REG(address, TGA_ADDRESS_REG);
  abs_dx = abs(x2 - x1);
  abs_dy = abs(y2 - y1);
  if(octant & YMAJOR)
    length = abs_dy;
  else
    length = abs_dx;

  if(octant & YMAJOR) {
      if(octant & YDECREASING) {
	  if(octant & XDECREASING)
	      octant_reg = TGA_SLOPE0_REG;
	  else
	      octant_reg = TGA_SLOPE2_REG;
      }
      else {
	  if(octant & XDECREASING)
	      octant_reg = TGA_SLOPE1_REG;
	  else
	      octant_reg = TGA_SLOPE3_REG;
      }
  }
  else {
      if(octant & YDECREASING) {
	  if(octant & XDECREASING)
	      octant_reg = TGA_SLOPE4_REG;
	  else
	      octant_reg = TGA_SLOPE6_REG;
      }
      else {
	  if(octant & XDECREASING)
	      octant_reg = TGA_SLOPE5_REG;
	  else
	      octant_reg = TGA_SLOPE7_REG;
      }
  }

  TGA_FAST_WRITE_REG(abs_dx | (abs_dy << 16), octant_reg);
  if(length > 16 && length % 16)
    length -= length % 16;
  else
    length -= 16;
  
  while(length > 0) {
    TGA_FAST_WRITE_REG(0xFFFFFFFF, TGA_CONTINUE_REG);      
    length -= 16;
  }

  TGA_FAST_WRITE_REG(SIMPLE | X11 | pTga->depthflag, TGA_MODE_REG);
  TGA_FAST_WRITE_REG(MIX_SRC | pTga->depthflag, TGA_RASTEROP_REG);
  TGA_FAST_WRITE_REG(0xFFFFFFFF, TGA_PLANEMASK_REG);  

  TGA_SAVE_OFFSET();

  return;
}

void
TGASetupForClippedLine(ScrnInfoPtr pScrn, int x1, int y1, int x2, int y2,
		       int octant)
{
#ifdef PROFILE
  unsigned int start, stop;
#endif
  TGAPtr pTga = NULL;
  CARD32 abs_dx = 0, abs_dy = 0, octant_reg = 0;
  TGA_DECL();

  pTga = TGAPTR(pScrn);
  TGA_GET_IOBASE();
  TGA_GET_OFFSET();
/*    ErrorF("TGASetupForClippedLine called\n"); */

/*    TGA_FAST_WRITE_REG(pTga->current_rop | BPP8PACKED, TGA_RASTEROP_REG); */
/*    TGA_FAST_WRITE_REG(OPAQUELINE | X11 | BPP8PACKED, TGA_MODE_REG); */
  
  abs_dx = abs(x2 - x1);
  abs_dy = abs(y2 - y1);

  if(octant & YMAJOR) {
      if(octant & YDECREASING) {
	  if(octant & XDECREASING)
	      octant_reg = TGA_NOSLOPE0_REG;
	  else
	      octant_reg = TGA_NOSLOPE2_REG;
      }
      else {
	  if(octant & XDECREASING)
	      octant_reg = TGA_NOSLOPE1_REG;
	  else
	      octant_reg = TGA_NOSLOPE3_REG;
      }
  }
  else {
      if(octant & YDECREASING) {
	  if(octant & XDECREASING)
	      octant_reg = TGA_NOSLOPE4_REG;
	  else
	      octant_reg = TGA_NOSLOPE6_REG;
      }
      else {
	  if(octant & XDECREASING)
	      octant_reg = TGA_NOSLOPE5_REG;
	  else
	      octant_reg = TGA_NOSLOPE7_REG;
      }
  }

  TGA_FAST_WRITE_REG(abs_dx | (abs_dy << 16), octant_reg);

  TGA_SAVE_OFFSET();
  return;
}

void
TGASubsequentClippedSolidLine(ScrnInfoPtr pScrn, int x1, int y1, int len,
			      int err)
    /* we are already in line mode, now we need to
       1) write starting pixel address to gaddr
       2) write initial error and line length to bresenham 3
       3) write the gctr to draw the line, and repeat for lines > 16 pixels
       4) set mode back to simple mode
    */
{
#ifdef PROFILE
  unsigned int start, stop;
#endif
  TGAPtr pTga = NULL;
  CARD32 address = 0;
  int length = 0;
  TGA_DECL();

  pTga = TGAPTR(pScrn);
  TGA_GET_IOBASE();
  TGA_GET_OFFSET();
/*    ErrorF("TGASubsequentClippedSolidLine called\n"); */

  address = FB_OFFSET(x1, y1);
  TGA_FAST_WRITE_REG(address, TGA_ADDRESS_REG);

  TGA_FAST_WRITE_REG(pTga->current_rop | pTga->depthflag, TGA_RASTEROP_REG);
  TGA_FAST_WRITE_REG(OPAQUELINE | X11 | pTga->depthflag, TGA_MODE_REG);
  TGA_FAST_WRITE_REG(pTga->current_planemask, TGA_PLANEMASK_REG);
  
  length = len;  
  TGA_FAST_WRITE_REG((err << 15) | (length & 0xF), TGA_BRES3_REG);
  
  while(length > 0) {
    TGA_FAST_WRITE_REG(0xFFFFFFFF, TGA_CONTINUE_REG);      
    if(length > 16 && length % 16)
	length -= length % 16;
    else
	length -= 16;
  }

  TGA_FAST_WRITE_REG(SIMPLE | X11 | pTga->depthflag, TGA_MODE_REG);
  TGA_FAST_WRITE_REG(MIX_SRC | pTga->depthflag, TGA_RASTEROP_REG);
  TGA_FAST_WRITE_REG(pTga->current_planemask, TGA_PLANEMASK_REG);

  TGA_SAVE_OFFSET();

  return;
  
}
      
void
TGASetupForDashedLine(ScrnInfoPtr pScrn, int fg, int bg, int rop,
		      unsigned int planemask, int length,
		      unsigned char *pattern)
{
#ifdef PROFILE
  unsigned int start = 0, stop = 0;
#endif
  TGAPtr pTga = NULL;
  unsigned int color1 = 0, color2 = 0, pmask = 0;
  TGA_DECL();
  
  pTga = TGAPTR(pScrn);
  TGA_GET_IOBASE();
  TGA_GET_OFFSET();
/*    ErrorF("TGASetupForDashedLine called\n"); */

  if(pTga->depthflag == BPP8PACKED) {
    color1 = fg | (fg << 8) | (fg << 16) | (fg << 24);
    color2 = bg | (bg << 8) | (bg << 16) | (bg << 24);
    pmask = planemask | (planemask << 8) | (planemask << 16)
      | (planemask << 24);
  }
  else {
    color1 = fg;
    color2 = fg;
    pmask = planemask;
  }
  
  pTga->current_rop = rop | pTga->depthflag;
  TGA_FAST_WRITE_REG(color1, TGA_FOREGROUND_REG);
  pTga->current_planemask = pmask;
  if(bg == -1) /* transparent line */
    pTga->transparent_pattern_p = 1;
  else {
    pTga->transparent_pattern_p = 0;
    TGA_FAST_WRITE_REG(color2, TGA_BACKGROUND_REG);
  }
     
  pTga->line_pattern = pattern[0] | (pattern[1] << 8);
  pTga->line_pattern_length = length;

  TGA_FAST_WRITE_REG(pScrn->displayWidth, TGA_WIDTH_REG);
  TGA_SAVE_OFFSET();

  return;
}

void
TGASubsequentDashedLine(ScrnInfoPtr pScrn, int x1, int y1, int x2,
				int y2, int octant, int flags, int phase)
{
#ifdef PROFILE
  unsigned int start, stop;
#endif
  TGAPtr pTga = NULL;
  CARD32 abs_dx = 0, abs_dy = 0, address = 0, octant_reg = 0;
  int length = 0;
  CARD16 line_mask = 0;
  int pattern_overflow = 0;
  int l = 0;
  TGA_DECL();
  
  pTga = TGAPTR(pScrn);
  TGA_GET_IOBASE();
  TGA_GET_OFFSET();
/*    ErrorF("TGASubsequentDashedLine called\n"); */

  TGA_FAST_WRITE_REG(pTga->current_rop, TGA_RASTEROP_REG);
  TGA_FAST_WRITE_REG(pTga->current_planemask, TGA_PLANEMASK_REG);
  if(pTga->transparent_pattern_p)
    TGA_FAST_WRITE_REG(TRANSPARENTLINE | X11 | pTga->depthflag |
		       ((flags & OMIT_LAST) ? 0x0 : CAP_ENDS),
		       TGA_MODE_REG);
  else
    TGA_FAST_WRITE_REG(OPAQUELINE | X11 | pTga->depthflag |
		       ((flags & OMIT_LAST) ? 0x0 : CAP_ENDS),
		       TGA_MODE_REG);
  
  address = FB_OFFSET(x1, y1);
  TGA_FAST_WRITE_REG(address, TGA_ADDRESS_REG);  
  abs_dx = abs(x2 - x1);
  abs_dy = abs(y2 - y1);
  if(abs_dx > abs_dy)
    length = abs_dx;
  else
    length = abs_dy;

  if(octant & YMAJOR) {
      if(octant & YDECREASING) {
	  if(octant & XDECREASING)
	      octant_reg = TGA_SLOPE0_REG;
	  else
	      octant_reg = TGA_SLOPE2_REG;
      }
      else {
	  if(octant & XDECREASING)
	      octant_reg = TGA_SLOPE1_REG;
	  else
	      octant_reg = TGA_SLOPE3_REG;
      }
  }
  else {
      if(octant & YDECREASING) {
	  if(octant & XDECREASING)
	      octant_reg = TGA_SLOPE4_REG;
	  else
	      octant_reg = TGA_SLOPE6_REG;
      }
      else {
	  if(octant & XDECREASING)
	      octant_reg = TGA_SLOPE5_REG;
	  else
	      octant_reg = TGA_SLOPE7_REG;
      }
  }
  /* set up our first pattern with phase.  Keep track of if we overflow the
     pattern by pattern_overflow, and correct on the next write */
		   
  if(phase) {
    line_mask = pTga->line_pattern >> phase;
    l = (pTga->line_pattern_length - phase);
  }
  else {
    line_mask = pTga->line_pattern;
    l = pTga->line_pattern_length;
  }
  
  while(l < 16) {
    line_mask |= pTga->line_pattern << l;
    l += pTga->line_pattern_length;
  }
  pattern_overflow = l - 16;
    
  TGA_FAST_WRITE_REG(line_mask, TGA_DATA_REG);
  
  TGA_FAST_WRITE_REG(abs_dx | (abs_dy << 16), octant_reg);

  if(length > 16 && length % 16)
    length -= length % 16;
  else
    length -= 16;
  
  while(length > 0) {

    if(pattern_overflow) {
      line_mask = pTga->line_pattern >> (pTga->line_pattern_length -
					 pattern_overflow);
      l = pattern_overflow;
    }
    else {
      line_mask = pTga->line_pattern;
      l = pTga->line_pattern_length;
    }
    while(l < 16) {
      line_mask |= (pTga->line_pattern << l);
      l += pTga->line_pattern_length;
    }
    pattern_overflow = l - 16;
      
    TGA_FAST_WRITE_REG(line_mask, TGA_CONTINUE_REG);
    length -= 16;
  }

  TGA_FAST_WRITE_REG(SIMPLE | X11 | pTga->depthflag, TGA_MODE_REG);
  TGA_FAST_WRITE_REG(MIX_SRC | pTga->depthflag, TGA_RASTEROP_REG);
  TGA_FAST_WRITE_REG(0xFFFFFFFF, TGA_PLANEMASK_REG);
  
  TGA_SAVE_OFFSET();

  return;
}

void
TGASubsequentClippedDashedLine(ScrnInfoPtr pScrn, int x1, int y1, int len,
			      int err, int phase)
{
#ifdef PROFILE
  unsigned int start, stop;
#endif
  TGAPtr pTga = NULL;
  CARD32 address = 0;
  int length = 0;
  CARD16 line_mask = 0;
  int pattern_overflow = 0;
  int l = 0;
  TGA_DECL();
  
  pTga = TGAPTR(pScrn);
  TGA_GET_IOBASE();
  TGA_GET_OFFSET();
  /*    ErrorF("TGASubsequentClippedDashedLine called\n"); */

  address = FB_OFFSET(x1, y1);
  TGA_FAST_WRITE_REG(address, TGA_ADDRESS_REG);

  TGA_FAST_WRITE_REG(pTga->current_rop, TGA_RASTEROP_REG);
  TGA_FAST_WRITE_REG(pTga->current_planemask, TGA_PLANEMASK_REG);

  if(pTga->transparent_pattern_p)
    TGA_FAST_WRITE_REG(TRANSPARENTLINE | X11 | pTga->depthflag, TGA_MODE_REG);
  else
    TGA_FAST_WRITE_REG(OPAQUELINE | X11 | pTga->depthflag, TGA_MODE_REG);
  
  length = len;  
  TGA_FAST_WRITE_REG((err << 15) | (length & 0xF), TGA_BRES3_REG);

		   
  if(phase) {
    line_mask = pTga->line_pattern >> phase;
    l = (pTga->line_pattern_length - phase);
  }
  else {
    line_mask = pTga->line_pattern;
    l = pTga->line_pattern_length;
  }
  
  while(l < 16) {
    line_mask |= pTga->line_pattern << l;
    l += pTga->line_pattern_length;
  }
  pattern_overflow = l - 16;

  
  while(length > 0) {
    TGA_FAST_WRITE_REG(line_mask, TGA_CONTINUE_REG);
    
    if(pattern_overflow) {
      line_mask = pTga->line_pattern >> (pTga->line_pattern_length -
					 pattern_overflow);
      l = pattern_overflow;
    }
    else {
      line_mask = pTga->line_pattern;
      l = pTga->line_pattern_length;
    }
    while(l < 16) {
      line_mask |= (pTga->line_pattern << l);
      l += pTga->line_pattern_length;
    }
    pattern_overflow = l - 16;
      
    if(length > 16 && length % 16)
      length -= length % 16;
    else
      length -= 16;
  }

  TGA_FAST_WRITE_REG(SIMPLE | X11 | pTga->depthflag, TGA_MODE_REG);
  TGA_FAST_WRITE_REG(MIX_SRC | pTga->depthflag, TGA_RASTEROP_REG);
  TGA_FAST_WRITE_REG(0xFFFFFFFF, TGA_PLANEMASK_REG);
  
  TGA_SAVE_OFFSET();

  return;
}
