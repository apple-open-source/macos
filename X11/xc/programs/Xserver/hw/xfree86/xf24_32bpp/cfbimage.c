/* $XFree86: xc/programs/Xserver/hw/xfree86/xf24_32bpp/cfbimage.c,v 1.3 2000/02/25 00:21:00 mvojkovi Exp $ */

#include "X.h"
#include "windowstr.h"
#include "pixmapstr.h"
#include "scrnintstr.h"
#include "gcstruct.h"
#define PSZ 8
#include "cfb.h"
#undef PSZ
#include "cfb24.h"
#include "cfb32.h"
#include "cfb24_32.h"
#include "servermd.h"
#include "mi.h"


void
cfb24_32GetImage (
    DrawablePtr pDraw,
    int sx, int sy, int w, int h,
    unsigned int format,
    unsigned long planemask,
    char *pdstLine
){
    if(!w || !h) return;

    if (!cfbDrawableEnabled (pDraw))
	return;

    if(pDraw->bitsPerPixel != 24) {
	cfb32GetImage(pDraw, sx, sy, w, h, format, planemask, pdstLine);
	return;
    } else if (format == ZPixmap) {
	BoxRec box;
	DDXPointRec ptSrc;
	RegionRec rgnDst;
	ScreenPtr pScreen;
	PixmapPtr pPixmap;

	pScreen = pDraw->pScreen;
        pPixmap = GetScratchPixmapHeader(pScreen, w, h, 24, 32,
                        PixmapBytePad(w,24), (pointer)pdstLine);
        if (!pPixmap)
            return;
        if ((planemask & 0x00ffffff) != 0x00ffffff)
            bzero((char *)pdstLine, pPixmap->devKind * h);
        ptSrc.x = sx + pDraw->x;
        ptSrc.y = sy + pDraw->y;
        box.x1 = 0;
        box.y1 = 0;
        box.x2 = w;
        box.y2 = h;
        REGION_INIT(pScreen, &rgnDst, &box, 1);
        cfbDoBitblt24To32(pDraw, (DrawablePtr)pPixmap, GXcopy, &rgnDst,
                    &ptSrc, planemask, 0);
        REGION_UNINIT(pScreen, &rgnDst);
        FreeScratchPixmapHeader(pPixmap);
    } else
	miGetImage(pDraw, sx, sy, w, h, format, planemask, pdstLine);
}


void
cfb24_32GetSpans(
   DrawablePtr pDraw,
   int wMax,
   DDXPointPtr ppt,
   int *pwidth,
   int nspans,
   char *pDst
){
   int pitch, i;
   CARD8 *ptr, *ptrBase;

   if (!cfbDrawableEnabled (pDraw))
	return;

   if(pDraw->bitsPerPixel != 24){
	cfb32GetSpans(pDraw, wMax, ppt, pwidth, nspans, pDst);
	return;
   }

   /* gotta get spans from a 24 bpp window */
   cfbGetByteWidthAndPointer(pDraw, pitch, ptrBase);

   while(nspans--) {
	ptr = ptrBase + (ppt->y * pitch) + (ppt->x * 3);
	
	for(i = *pwidth; i--; ptr += 3, pDst += 4) {
	/* this is used so infrequently that it's not worth optimizing */
	   pDst[0] = ptr[0];
	   pDst[1] = ptr[1];
	   pDst[2] = ptr[2];
	}
	
	ppt++; pwidth++;
   }
}


