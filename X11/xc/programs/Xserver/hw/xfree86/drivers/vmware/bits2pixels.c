/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/vmware/bits2pixels.c,v 1.2 2002/12/11 17:07:58 dawes Exp $ */
/* **********************************************************
 * Copyright (C) 1999-2001 VMware, Inc.
 * All Rights Reserved
 * **********************************************************/
#ifdef VMX86_DEVEL
char rcsId_bits2pixels[] = "Id: bits2pixels.c,v 1.6 2001/01/26 23:32:15 yoel Exp $";
#else
#define FILECODE "F(814)"
#endif

/*
 * bits2pixels.c --
 *
 *      Emulation routines to convert bitmaps to pixmaps
 */

#include "vm_basic_types.h"
#include "bits2pixels.h"


/*
 *  Local functions
 */

static void RasterBitsToPixels8(uint8 *bits, uint32 bits_increment,
			 uint8 *pix, uint32 pix_increment,
			 uint32 width, uint32 height, uint32 fg, uint32 bg);

static void RasterBitsToPixels16(uint8 *bits, uint32 bits_increment,
			  uint8 *pix, uint32 pix_increment,
			  uint32 width, uint32 height, uint32 fg, uint32 bg);

static void RasterBitsToPixels24(uint8 *bits, uint32 bits_increment,
			  uint8 *pix, uint32 pix_increment,
			  uint32 width, uint32 height, uint32 fg, uint32 bg);

static void RasterBitsToPixels32(uint8 *bits, uint32 bits_increment,
			  uint8 *pix, uint32 pix_increment,
			  uint32 width, uint32 height, uint32 fg, uint32 bg);


/*
 *----------------------------------------------------------------------
 *
 * vmwareRaster_BitsToPixels --
 *
 *	Convert a bitmap to a pixmap, converting 1 bits to the foreground
 *      color (fg) and 0 bits to the background color (bg).
 *
 * Results:
 *      Pixmap filled with pixels
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

void
vmwareRaster_BitsToPixels(uint8 *bits, uint32 bits_increment,
		    uint8 *pix, uint32 pix_increment, int bytes_per_pixel,
		    uint32 width, uint32 height, uint32 fg, uint32 bg)
{
   switch (bytes_per_pixel) {
      case 1:
	 RasterBitsToPixels8(bits, bits_increment, pix, pix_increment,
			     width, height, fg, bg);
	 break;

      case 2:
	 RasterBitsToPixels16(bits, bits_increment, pix, pix_increment,
			      width, height, fg, bg);
	 break;

      case 3:
	 RasterBitsToPixels24(bits, bits_increment, pix, pix_increment,
			      width, height, fg, bg);
	 break;

      case 4:
	 RasterBitsToPixels32(bits, bits_increment, pix, pix_increment,
			      width, height, fg, bg);
	 break;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * RasterBitsToPixels8 --
 *
 *	Convert a bitmap to a pixmap, converting 1 bits to the foreground
 *      color (fg) and 0 bits to the background color (bg), for an 8-bit
 *	pixmap
 *
 * Results:
 *      Pixmap filled with pixels
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

void
RasterBitsToPixels8(uint8 *bits, uint32 bits_increment,
		    uint8 *pix, uint32 pix_increment,
		    uint32 width, uint32 height, uint32 fg, uint32 bg)
{
   uint8 *lpix, *lbits;
   int i, j;
   uint32 expbits = 0;		 /* Bits to be expanded */

   for (i=0; i<height; i++) {
      lpix = pix;
      lbits = bits;
      for (j = width ; j > 0; j -= 4) {
	 expbits = (*lbits >> 4) & 0x0f;
	 
	 if (j < 4)
	    break;
	 
	 switch (expbits) {
	 case 0:
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    break;
	 case 1:
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    break;
	 case 2:
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    break;
	 case 3:
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    break;
	 case 4:
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    break;
	 case 5:
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    break;
	 case 6:
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    break;
	 case 7:
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    break;
	 case 8:
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    break;
	 case 9:
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    break;
	 case 10:
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    break;
	 case 11:
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    break;
	 case 12:
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    break;
	 case 13:
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    break;
	 case 14:
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    break;
	 case 15:
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    break;
	 }

	 expbits = *lbits & 0x0f;

	 j -= 4;
	 if (j < 4) {
	    break;
	 }

	 switch (expbits) {
	 case 0:
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    break;
	 case 1:
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    break;
	 case 2:
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    break;
	 case 3:
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    break;
	 case 4:
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    break;
	 case 5:
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    break;
	 case 6:
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    break;
	 case 7:
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    break;
	 case 8:
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    break;
	 case 9:
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    break;
	 case 10:
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    break;
	 case 11:
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    break;
	 case 12:
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    break;
	 case 13:
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    break;
	 case 14:
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    break;
	 case 15:
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    break;
	 }
	 lbits++;
      }

      if (j > 0) {
	 *lpix++ = (expbits & 0x08) ? fg : bg;
	 j--;
	 if (j > 0) {
	    *lpix++ = (expbits & 0x04) ? fg : bg;
	    j--;
	    if (j > 0) {
	       *lpix++ = (expbits & 0x02) ? fg : bg;
	       j--;
	    }
	 }
      }

      pix += pix_increment;
      bits += bits_increment;
   }
   return;
}


/*
 *----------------------------------------------------------------------
 *
 * RasterBitsToPixels16 --
 *
 *	Convert a bitmap to a pixmap, converting 1 bits to the foreground
 *      color (fg) and 0 bits to the background color (bg), for a 16-bit
 *	pixmap
 *
 * Results:
 *      Pixmap filled with pixels
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

void
RasterBitsToPixels16(uint8 *bits, uint32 bits_increment,
		     uint8 *pix, uint32 pix_increment,
		     uint32 width, uint32 height, uint32 fg, uint32 bg)
{
   uint16 *lpix;
   uint8 *lbits;
   int i, j;
   uint32 expbits = 0;		 /* Bits to be expanded */

   for (i=0; i<height; i++) {
      lpix = (uint16 *)pix;
      lbits = bits;
      for (j = width; j > 0; j -= 4) {
	 expbits = (*lbits >> 4) & 0x0f;

	 if (j < 4)
	    break;

	 switch (expbits) {
	 case 0:
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    break;
	 case 1:
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    break;
	 case 2:
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    break;
	 case 3:
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    break;
	 case 4:
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    break;
	 case 5:
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    break;
	 case 6:
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    break;
	 case 7:
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    break;
	 case 8:
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    break;
	 case 9:
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    break;
	 case 10:
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    break;
	 case 11:
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    break;
	 case 12:
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    break;
	 case 13:
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    break;
	 case 14:
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    break;
	 case 15:
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    break;
	 }

	 expbits = *lbits & 0x0f;

	 j -= 4;
	 if (j < 4) {
	    break;
	 }

	 switch (expbits) {
	 case 0:
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    break;
	 case 1:
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    break;
	 case 2:
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    break;
	 case 3:
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    break;
	 case 4:
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    break;
	 case 5:
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    break;
	 case 6:
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    break;
	 case 7:
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    break;
	 case 8:
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    break;
	 case 9:
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    break;
	 case 10:
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    break;
	 case 11:
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    break;
	 case 12:
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    break;
	 case 13:
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    break;
	 case 14:
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    break;
	 case 15:
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    break;
	 }
	 lbits++;
      }

      if (j > 0) {
	 *lpix++ = (expbits & 0x08) ? fg : bg;
	 j--;
	 if (j > 0) {
	    *lpix++ = (expbits & 0x04) ? fg : bg;
	    j--;
	    if (j > 0) {
	       *lpix++ = (expbits & 0x02) ? fg : bg;
	       j--;
	    }
	 }
      }

      pix += pix_increment;
      bits += bits_increment;
   }
   return;
}



/*
 *----------------------------------------------------------------------
 *
 * RasterBitsToPixels24 --
 *
 *	Convert a bitmap to a pixmap, converting 1 bits to the foreground
 *      color (fg) and 0 bits to the background color (bg), for a 24-bit
 *	pixmap
 *
 * Results:
 *      Pixmap filled with pixels
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

void
RasterBitsToPixels24(uint8 *bits, uint32 bits_increment,
		     uint8 *pix, uint32 pix_increment,
		     uint32 width, uint32 height, uint32 fg, uint32 bg)
{
   uint8 *lpix, *lbits;
   uint32 fgColor1, fgColor2, fgColor3;
   uint32 bgColor1, bgColor2, bgColor3;

   int i, j;
   uint32 expbits = 0;		 /* Bits to be expanded */

   fgColor1 = fg & 0x000000ff;
   fgColor2 = (fg >> 8) & 0x000000ff;
   fgColor3 = (fg >> 16) & 0x000000ff;

   bgColor1 = bg & 0x000000ff;
   bgColor2 = (bg >> 8) & 0x000000ff;
   bgColor3 = (bg >> 16) & 0x000000ff;

   for (i=0; i<height; i++) {
      lpix = pix;
      lbits = bits;
      for (j = width; j > 0; j -= 4) {
	 expbits = (*lbits >> 4) & 0x0f;

	 if (j < 4)
	    break;

	 switch (expbits) {
	 case 0:
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    break;
	 case 1:
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    break;
	 case 2:
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    break;
	 case 3:
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    break;
	 case 4:
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    break;
	 case 5:
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    break;
	 case 6:
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    break;
	 case 7:
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    break;
	 case 8:
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    break;
	 case 9:
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    break;
	 case 10:
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    break;
	 case 11:
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    break;
	 case 12:
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    break;
	 case 13:
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    break;
	 case 14:
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    break;
	 case 15:
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    break;
	 }

	 expbits = *lbits & 0x0f;

	 j -= 4;
	 if (j < 4) {
	    break;
	 }

	 switch (expbits) {
	 case 0:
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    break;
	 case 1:
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    break;
	 case 2:
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    break;
	 case 3:
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    break;
	 case 4:
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    break;
	 case 5:
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    break;
	 case 6:
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    break;
	 case 7:
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    break;
	 case 8:
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    break;
	 case 9:
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    break;
	 case 10:
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    break;
	 case 11:
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    break;
	 case 12:
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    break;
	 case 13:
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    break;
	 case 14:
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = bgColor1;
	    *lpix++ = bgColor2;
	    *lpix++ = bgColor3;
	    break;
	 case 15:
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    *lpix++ = fgColor1;
	    *lpix++ = fgColor2;
	    *lpix++ = fgColor3;
	    break;
	 }
	 lbits++;
      }

      if (j > 0) {
	 *lpix++ = (expbits & 0x08) ? fgColor1 : bgColor1;
	 *lpix++ = (expbits & 0x08) ? fgColor2 : bgColor2;
	 *lpix++ = (expbits & 0x08) ? fgColor3 : bgColor3;
	 j--;
	 if (j > 0) {
	    *lpix++ = (expbits & 0x04) ? fgColor1 : bgColor1;
	    *lpix++ = (expbits & 0x04) ? fgColor2 : bgColor2;
	    *lpix++ = (expbits & 0x04) ? fgColor3 : bgColor3;
	    j--;
	    if (j > 0) {
	       *lpix++ = (expbits & 0x02) ? fgColor1 : bgColor1;
	       *lpix++ = (expbits & 0x02) ? fgColor2 : bgColor2;
	       *lpix++ = (expbits & 0x02) ? fgColor3 : bgColor3;
	       j--;
	    }
	 }
      }

      pix += pix_increment;
      bits += bits_increment;
   }
   return;
}



/*
 *----------------------------------------------------------------------
 *
 * RasterBitsToPixels32 --
 *
 *	Convert a bitmap to a pixmap, converting 1 bits to the foreground
 *      color (fg) and 0 bits to the background color (bg), for a 32-bit
 *	pixmap
 *
 * Results:
 *      Pixmap filled with pixels
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

void
RasterBitsToPixels32(uint8 *bits, uint32 bits_increment,
		     uint8 *pix, uint32 pix_increment,
		     uint32 width, uint32 height, uint32 fg, uint32 bg)
{
   uint32 *lpix;
   uint8 *lbits;
   int i, j;
   uint32 expbits = 0;		 /* Bits to be expanded */

   for (i=0; i<height; i++) {
      lpix = (uint32 *)pix;
      lbits = bits;
      for (j = width; j > 0; j -= 4) {
	 expbits = (*lbits >> 4) & 0x0f;
	 
	 if (j < 4)
	    break;

	 switch (expbits) {
	 case 0:
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    break;
	 case 1:
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    break;
	 case 2:
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    break;
	 case 3:
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    break;
	 case 4:
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    break;
	 case 5:
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    break;
	 case 6:
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    break;
	 case 7:
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    break;
	 case 8:
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    break;
	 case 9:
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    break;
	 case 10:
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    break;
	 case 11:
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    break;
	 case 12:
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    break;
	 case 13:
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    break;
	 case 14:
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    break;
	 case 15:
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    break;
	 }

	 expbits = *lbits & 0x0f;

	 j -= 4;
	 if (j < 4) {
	    break;
	 }

	 switch (expbits) {
	 case 0:
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    break;
	 case 1:
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    break;
	 case 2:
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    break;
	 case 3:
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    break;
	 case 4:
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    break;
	 case 5:
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    break;
	 case 6:
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    break;
	 case 7:
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    break;
	 case 8:
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    break;
	 case 9:
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    break;
	 case 10:
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    break;
	 case 11:
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    break;
	 case 12:
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = bg;
	    break;
	 case 13:
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    *lpix++ = fg;
	    break;
	 case 14:
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = bg;
	    break;
	 case 15:
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    *lpix++ = fg;
	    break;
	 }
	 lbits++;
      }

      if (j > 0) {
	 *lpix++ = (expbits & 0x08) ? fg : bg;
	 j--;
	 if (j > 0) {
	    *lpix++ = (expbits & 0x04) ? fg : bg;
	    j--;
	    if (j > 0) {
	       *lpix++ = (expbits & 0x02) ? fg : bg;
	       j--;
	    }
	 }
      }

      pix += pix_increment;
      bits += bits_increment;
   }
   return;
}
