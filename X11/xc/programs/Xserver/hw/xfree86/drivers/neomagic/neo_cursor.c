/**********************************************************************
Copyright 1998, 1999 by Precision Insight, Inc., Cedar Park, Texas.

                        All Rights Reserved

Permission to use, copy, modify, distribute, and sell this software and
its documentation for any purpose is hereby granted without fee,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of Precision Insight not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  Precision Insight
and its suppliers make no representations about the suitability of this
software for any purpose.  It is provided "as is" without express or 
implied warranty.

PRECISION INSIGHT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY
SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
**********************************************************************/
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/neomagic/neo_cursor.c,v 1.9 2002/10/30 12:52:21 alanh Exp $ */

/*
 * The original Precision Insight driver for
 * XFree86 v.3.3 has been sponsored by Red Hat.
 *
 * Authors:
 *   Jens Owen (jens@tungstengraphics.com)
 *   Kevin E. Martin (kevin@precisioninsight.com)
 *
 * Port to Xfree86 v.4.0
 *   1998, 1999 by Egbert Eich (Egbert.Eich@Physik.TU-Darmstadt.DE)
 */


/* All drivers should typically include these */
#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h" 
#include "vgaHW.h"

#include "xf86Cursor.h"
#include "cursorstr.h"
/* Driver specific headers */
#include "neo.h"

static void _neoLoadCursorImage(ScrnInfoPtr pScrn, unsigned char *src,
				int xoff, int yoff);

void
NeoShowCursor(ScrnInfoPtr pScrn)
{
    NEOPtr nPtr = NEOPTR(pScrn);

    /* turn cursor on */
    OUTREG(NEOREG_CURSCNTL, NEO_CURS_ENABLE);
    nPtr->NeoHWCursorShown = TRUE;    
}

void
NeoHideCursor(ScrnInfoPtr pScrn)
{
    NEOPtr nPtr = NEOPTR(pScrn);
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    
    /*
     * turn cursor off 
     *
     * Sometimes we loose the I/O map, so directly use I/O here
     */

    VGAwGR(0x82,0x00);

    nPtr->NeoHWCursorShown = FALSE;
}

#define MAX_CURS 64

#define REVBITS_32(__b) { \
  ((unsigned char *)&__b)[0] = byte_reversed[((unsigned char *)&__b)[0]]; \
  ((unsigned char *)&__b)[1] = byte_reversed[((unsigned char *)&__b)[1]]; \
  ((unsigned char *)&__b)[2] = byte_reversed[((unsigned char *)&__b)[2]]; \
  ((unsigned char *)&__b)[3] = byte_reversed[((unsigned char *)&__b)[3]]; \
}

static void
neoSetCursorPosition(ScrnInfoPtr pScrn, int x, int y)
{
    NEOPtr nPtr = NEOPTR(pScrn);
    NEOACLPtr nAcl = NEOACLPTR(pScrn);
    int i;
    CARD32 bits, bits2;
    unsigned char *_dest = ((unsigned char *)nPtr->NeoFbBase +
			    nAcl->CursorAddress);
    unsigned char *src = nPtr->NeoCursorImage;
    int xoff = 0, yoff = 0;
    
    if ((y < 0) && (y > (-MAX_CURS))) {
	yoff = -y;
	y = 0;
    }
    if ((x < 0) && (x > (-MAX_CURS))) {
	xoff = -x;
	x = 0;
    }
    if (yoff != nPtr->NeoCursorPrevY || xoff !=nPtr->NeoCursorPrevX) {
	nPtr->NeoCursorPrevY = yoff;
	nPtr->NeoCursorPrevX = xoff;
       
        /* This is for sprites that move off the top of the display.
	 * this code simply updates the pointer used for loading the sprite.
	 * Note, in our driver's RealizeCursor, the allocated buffer size
	 * is twice as large as needed, and we initialize the upper half to all
	 * zeros, so we can use this pointer trick here.
	 */
       
         if (yoff) {
	    src += (yoff * 16);       
	 }

	 /* This is for sprites that move off the left edge of the display.
	  * this code has to do some ugly bit swizzling to generate new cursor
	  * masks that give the impression the cursor is moving off the screen.
	  * WARNING: PLATFORM SPECIFIC!  This is 32-bit little endian code!
	  */
          if (xoff)
	    {
	       if (xoff < 32) { /* offset 1-31 */
		  for (i=0; i<256; i+=2) {
		     bits = ((CARD32 *)src)[i];
		     bits2 = ((CARD32 *)src)[i+1];
		     
		     REVBITS_32(bits);
		     REVBITS_32(bits2);
		     
		     bits = ((bits >> xoff) | (bits2 << (32-xoff)));
		     bits2 >>= xoff;
		     
		     REVBITS_32(bits);
		     REVBITS_32(bits2);
		     
		     ((CARD32 *) nAcl->CursTemp)[i] = bits;
		     ((CARD32 *) nAcl->CursTemp)[i+1] = bits2;
		  }
	       }
	       else { /* offset 32-63 */
		  for (i=0; i<256; i+=2) {
		     bits = ((CARD32 *)src)[i];
		     bits2 = ((CARD32 *)src)[i+1];

		     REVBITS_32(bits2);
		     
		     bits = (bits2 >> (xoff-32));
		     bits2 = 0;
		     
		     REVBITS_32(bits);
		     
		     ((CARD32 *)nAcl->CursTemp)[i] = bits;
		     ((CARD32 *)nAcl->CursTemp)[i+1] = bits2;
		  }
	       }
	       src = nAcl->CursTemp;
	    }
       memcpy(_dest, src, 1024);
       OUTREG(NEOREG_CURSMEMPOS, ((0x000f & (nAcl->CursorAddress >> 10)) << 8) |
	      ((0x0ff0 & (nAcl->CursorAddress >> 10)) >> 4));
       
       
    }
   
    /* Move the cursor */
    OUTREG(NEOREG_CURSX, x);
    OUTREG(NEOREG_CURSY, y);
}

static void
neoSetCursorColors(ScrnInfoPtr pScrn, int bg, int fg)
{
    NEOPtr nPtr = NEOPTR(pScrn);

    /* swap blue and red */
    fg = ((fg & 0xff0000) >> 16) | ((fg & 0xff) << 16) | (fg & 0xff00);
    bg = ((bg & 0xff0000) >> 16) | ((bg & 0xff) << 16) | (bg & 0xff00);

    /* load colors */
    OUTREG(NEOREG_CURSFGCOLOR, fg);
    OUTREG(NEOREG_CURSBGCOLOR, bg);
}

static void
_neoLoadCursorImage(ScrnInfoPtr pScrn, unsigned char *src, int xoff, int yoff)
{
    NEOPtr nPtr = NEOPTR(pScrn);
    NEOACLPtr nAcl = NEOACLPTR(pScrn);
    int i;
    unsigned char *_dest, *_src;
    int _width, _fill;
    
    for (i = 0; i< nPtr->CursorInfo->MaxHeight - yoff; i++) {
      _dest = ((unsigned char *)nPtr->NeoFbBase
	       + nAcl->CursorAddress
	       + ((nPtr->CursorInfo->MaxWidth >> 2) * i));
      _width = (nPtr->CursorInfo->MaxWidth
		- (xoff & 0x38)) >> 3;
      _src = (src + ((nPtr->CursorInfo->MaxWidth >> 2) * i));
      _fill = (xoff & 0x38) >> 3;
      
      memcpy(_dest,_src,_width);
      memset(_dest + _width, 0, _fill);
      
      _dest += (nPtr->CursorInfo->MaxWidth >> 3);
      _src += (nPtr->CursorInfo->MaxWidth >> 3);
      memcpy(_dest,_src,_width);
      memset(_dest + _width, 0, _fill);
    }
    memset(nPtr->NeoFbBase + nAcl->CursorAddress 
	   + ((nPtr->CursorInfo->MaxWidth >> 2) * i),
	   0, (nPtr->CursorInfo->MaxHeight - i)
	   * (nPtr->CursorInfo->MaxWidth >> 2));
    /* set cursor address here or we loose the cursor on video mode change */
    /* Load storage location.  */
    OUTREG(NEOREG_CURSMEMPOS, ((0x000f & (nAcl->CursorAddress >> 10)) << 8)  | 
	   ((0x0ff0 & (nAcl->CursorAddress >> 10)) >> 4));
}

static void
neoLoadCursorImage(ScrnInfoPtr pScrn, unsigned char *src)
{
    NEOPtr nPtr = NEOPTR(pScrn);
    nPtr->NeoCursorImage = src;  /* store src address for later use */
   
    /* Reset these because we have a new cursor image */
    nPtr->NeoCursorPrevY = nPtr->NeoCursorPrevX = 0;
   
    _neoLoadCursorImage(pScrn,src,0,0);
}

static Bool
neoUseHWCursor(ScreenPtr pScr, CursorPtr pCurs)
{
    NEOACLPtr nAcl = NEOACLPTR(xf86Screens[pScr->myNum]);

    return(nAcl->UseHWCursor && !nAcl->NoCursorMode);
}

static unsigned char*
neoRealizeCursor(xf86CursorInfoPtr infoPtr, CursorPtr pCurs)
{
    CARD32 *SrcS, *SrcM, *DstS, *DstM;
    CARD32 *pSrc, *pMsk;
    unsigned char *mem;
    int SrcPitch, DstPitch, y, x, z;

    mem = (unsigned char*)xnfcalloc(4096,1);
    SrcPitch = (pCurs->bits->width + 31) >> 5;
    DstPitch = infoPtr->MaxWidth >> 4;
    SrcS = (CARD32*)pCurs->bits->source;
    SrcM = (CARD32*)pCurs->bits->mask;
    DstS = (CARD32*)mem;
    DstM = DstS + (DstPitch >> 1);
    
    for(y = pCurs->bits->height, pSrc = DstS, pMsk = DstM; 
	y--; 
	pSrc+=DstPitch, pMsk+=DstPitch, SrcS+=SrcPitch, SrcM+=SrcPitch) {
	for(x = 0; x < SrcPitch; x++) {
	    pSrc[x] = ~SrcS[x] & SrcM[x];
	    pMsk[x] = SrcM[x];
	    for (z = 0; z < 4; z++) { 
		((char *)pSrc)[x*4+z] =
		    byte_reversed[((char *)pSrc)[x*4+z] & 0xFF];
		((char *)pMsk)[x*4+z] =
		    byte_reversed[((char *)pMsk)[x*4+z] & 0xFF];
	    }
	}
#if 0
	for (;x < DstPitch; x++) {
	    pSrc[x] = 0;
	    pMsk[x] = 0;
	}
#endif
    }

    return (unsigned char *)mem;
}

Bool
NeoCursorInit(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    NEOPtr nPtr = NEOPTR(pScrn);
    xf86CursorInfoPtr infoPtr;
    infoPtr = xf86CreateCursorInfoRec();
    if(!infoPtr) return FALSE;

    nPtr->CursorInfo = infoPtr;

    infoPtr->MaxHeight = 64;
    infoPtr->MaxWidth = 64;
    infoPtr->Flags = HARDWARE_CURSOR_TRUECOLOR_AT_8BPP;

    infoPtr->SetCursorColors = neoSetCursorColors;
    infoPtr->SetCursorPosition = neoSetCursorPosition;
    infoPtr->LoadCursorImage = neoLoadCursorImage;
    infoPtr->HideCursor = NeoHideCursor;
    infoPtr->ShowCursor = NeoShowCursor;
    infoPtr->UseHWCursor = neoUseHWCursor;
    infoPtr->RealizeCursor = neoRealizeCursor;
    
    return(xf86InitCursor(pScreen, infoPtr));
}



