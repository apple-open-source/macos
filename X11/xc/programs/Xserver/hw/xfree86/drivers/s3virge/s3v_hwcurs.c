/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/s3virge/s3v_hwcurs.c,v 1.8 2004/02/13 23:58:43 dawes Exp $ */

/*
 * Copyright (C) 1994-1999 The XFree86 Project, Inc.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 *   1.  Redistributions of source code must retain the above copyright
 *       notice, this list of conditions, and the following disclaimer.
 *
 *   2.  Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer
 *       in the documentation and/or other materials provided with the
 *       distribution, and in the same place and form as other copyright,
 *       license and disclaimer information.
 *
 *   3.  The end-user documentation included with the redistribution,
 *       if any, must include the following acknowledgment: "This product
 *       includes software developed by The XFree86 Project, Inc
 *       (http://www.xfree86.org/) and its contributors", in the same
 *       place and form as other third-party acknowledgments.  Alternately,
 *       this acknowledgment may appear in the software itself, in the
 *       same form and location as other such third-party acknowledgments.
 *
 *   4.  Except as contained in this notice, the name of The XFree86
 *       Project, Inc shall not be used in advertising or otherwise to
 *       promote the sale, use or other dealings in this Software without
 *       prior written authorization from The XFree86 Project, Inc.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE XFREE86 PROJECT, INC OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/*
 * s3v_hwcurs.c
 * HW Cursor support for 4.0 design level
 *
 * S3 ViRGE driver
 *
 *
 */


#include "s3v.h"

/* protos */

static void S3VLoadCursorImage(ScrnInfoPtr pScrn, unsigned char *src);
static void S3VShowCursor(ScrnInfoPtr pScrn);
static void S3VHideCursor(ScrnInfoPtr pScrn);
static void S3VSetCursorPosition(ScrnInfoPtr pScrn, int x, int y);
static void S3VSetCursorColors(ScrnInfoPtr pScrn, int bg, int fg);


/*
 * Read/write to the DAC via MMIO 
 */

#define inCRReg(reg) (VGAHWPTR(pScrn))->readCrtc( VGAHWPTR(pScrn), reg )
#define outCRReg(reg, val) (VGAHWPTR(pScrn))->writeCrtc( VGAHWPTR(pScrn), reg, val )



/****
 ***  HW Cursor
 */
static void
S3VLoadCursorImage(ScrnInfoPtr pScrn, unsigned char *src)
{
  S3VPtr ps3v = S3VPTR(pScrn);
 
    /*PVERB5("	S3VLoadCursorImage\n");*/

    /* Load storage location.  */
    outCRReg( HWCURSOR_ADDR_LOW_CR4D, 0xff & (ps3v->FBCursorOffset/1024));
    outCRReg( HWCURSOR_ADDR_HIGH_CR4C, (0x0f00 & (ps3v->FBCursorOffset/1024)) >> 8);

	/* Copy cursor image to framebuffer storage */
	memcpy( (ps3v->FBBase + ps3v->FBCursorOffset), src, 1024);

}


static void 
S3VShowCursor(ScrnInfoPtr pScrn)
{
  char tmp;

  tmp = inCRReg(HWCURSOR_MODE_CR45);
    /* Enable cursor */
  outCRReg(HWCURSOR_MODE_CR45, tmp | 1 );
}


static void
S3VHideCursor(ScrnInfoPtr pScrn)
{
  char tmp;
  
  tmp = inCRReg(HWCURSOR_MODE_CR45);
   /* Disable cursor */
  outCRReg(HWCURSOR_MODE_CR45, tmp & ~1 );
}


static void
S3VSetCursorPosition(ScrnInfoPtr pScrn, int x, int y)
{
   unsigned char xoff = 0, yoff = 0;

   /*
   if (!xf86VTSema)
      return;
    */

   /*
   x -= s3vHotX;
   y -= s3vHotY;
    */

   /*
    * Make these even when used.  There is a bug/feature on at least
    * some chipsets that causes a "shadow" of the cursor in interlaced
    * mode.  Making this even seems to have no visible effect, so just
    * do it for the generic case.
    * note - xoff & yoff are used for displaying partial cursors on screen
    * edges.
    */

   if (x < 0) {
     xoff = ((-x) & 0xFE);
     x = 0;
   }

   if (y < 0) {
      yoff = ((-y) & 0xFE);
      y = 0;
   }

   /* Double y position for a doublescan mode */
   if(pScrn->currentMode->Flags & V_DBLSCAN) y <<= 1;

   /* This is the recommended order to move the cursor */

   outCRReg( 0x46, (x & 0xff00)>>8 );
   outCRReg( 0x47, (x & 0xff) );
   outCRReg( 0x49, (y & 0xff) );
   outCRReg( 0x4e, xoff );
   outCRReg( 0x4f, yoff );
   outCRReg( 0x48, (y & 0xff00)>>8 );
}


static void
S3VSetCursorColors(ScrnInfoPtr pScrn, int bg, int fg)
{
    S3VPtr ps3v = S3VPTR(pScrn);

    /*PVERB5("	S3VSetCursorColors\n");*/

	switch( pScrn->bitsPerPixel) {
	case 8:
	  if (!(S3_ViRGE_GX2_SERIES(ps3v->Chipset) || S3_ViRGE_MX_SERIES(ps3v->Chipset))) {
	    /* Reset the cursor color stack pointer */
	    inCRReg(0x45);
	    /* Write foreground */
	    outCRReg(0x4a, fg);
	    outCRReg(0x4a, fg);
	    /* Reset the cursor color stack pointer */
	    inCRReg(0x45);
	    /* Write background */
	    outCRReg(0x4b, bg);
	    outCRReg(0x4b, bg);
	    break;
	  }  /* else fall through for ViRGE/MX... */
	case 16:
	  if (!(S3_ViRGE_GX2_SERIES(ps3v->Chipset) || S3_ViRGE_MX_SERIES(ps3v->Chipset))) {
	    /* adjust colors to 16 bits */
	    if (pScrn->weight.green == 5 && ps3v->Chipset != S3_ViRGE_VX) {
	      fg = ((fg & 0xf80000) >> 9) |
		((fg & 0xf800) >> 6) |
		((fg & 0xf8) >> 3);
	      bg = ((bg & 0xf80000) >> 9) |
		((bg & 0xf800) >> 6) |
		((bg & 0xf8) >> 3);
	    } else {
	      fg = ((fg & 0xf80000) >> 8) |
		((fg & 0xfc00) >> 5) |
		((fg & 0xf8) >> 3);
	      bg = ((bg & 0xf80000) >> 8) |
		((bg & 0xfc00) >> 5) |
		((bg & 0xf8) >> 3);
	    }

	    inCRReg(0x45);
	    /* Write foreground */
	    outCRReg(0x4a, fg);
	    outCRReg(0x4a, fg >> 8);
	    /* needed for 2nd pixel in double-clock modes */
	    outCRReg(0x4a, fg);
	    outCRReg(0x4a, fg >> 8);
	    /* Reset the cursor color stack pointer */
	    inCRReg(0x45);
	    /* Write background */
	    outCRReg(0x4b, bg);
	    outCRReg(0x4b, bg >> 8);
	    /* needed for 2nd pixel in double-clock modes */
	    outCRReg(0x4b, bg);
	    outCRReg(0x4b, bg >> 8);
	    break;
	  }  /* else fall through for ViRGE/MX... */

	case 24:
	case 32:
	  /* Do it straight, full 24 bit color. */
      
	  /* Reset the cursor color stack pointer */
	  inCRReg(0x45);
	  /* Write low, mid, high bytes - foreground */
	  outCRReg(0x4a, fg);
	  outCRReg(0x4a, fg >> 8);
	  outCRReg(0x4a, fg >> 16);
	  /* Reset the cursor color stack pointer */
	  inCRReg(0x45);
	  /* Write low, mid, high bytes - background */
	  outCRReg(0x4b, bg);
	  outCRReg(0x4b, bg >> 8);
	  outCRReg(0x4b, bg >> 16);
	  break;
	}
}


Bool 
S3VHWCursorInit(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    S3VPtr ps3v = S3VPTR(pScrn);
    xf86CursorInfoPtr infoPtr;

    PVERB5("	S3VHWCursorInit\n");

    infoPtr = xf86CreateCursorInfoRec();
    if(!infoPtr) return FALSE;
    
    ps3v->CursorInfoRec = infoPtr;

    infoPtr->MaxWidth = 64;
    infoPtr->MaxHeight = 64;
    infoPtr->Flags = HARDWARE_CURSOR_SOURCE_MASK_INTERLEAVE_16 |
    				 HARDWARE_CURSOR_SWAP_SOURCE_AND_MASK |
				 HARDWARE_CURSOR_AND_SOURCE_WITH_MASK |
        			 HARDWARE_CURSOR_BIT_ORDER_MSBFIRST;
    if (S3_ViRGE_GX2_SERIES(ps3v->Chipset) || S3_ViRGE_MX_SERIES(ps3v->Chipset))
       infoPtr->Flags |= HARDWARE_CURSOR_TRUECOLOR_AT_8BPP |
	                 HARDWARE_CURSOR_INVERT_MASK;

    infoPtr->SetCursorColors = S3VSetCursorColors;
    infoPtr->SetCursorPosition = S3VSetCursorPosition;
    infoPtr->LoadCursorImage = S3VLoadCursorImage;
    infoPtr->HideCursor = S3VHideCursor;
    infoPtr->ShowCursor = S3VShowCursor;
    infoPtr->UseHWCursor = NULL;

    return(xf86InitCursor(pScreen, infoPtr));
}

/*EOF*/
