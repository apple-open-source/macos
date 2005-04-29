/************************************************************

Copyright 1989, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

********************************************************/
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/mga/mga_arc.c,v 1.13 2004/02/20 16:59:49 tsi Exp $ */


#include "X.h"
#include "gcstruct.h"
#include "windowstr.h"
#include "pixmapstr.h"
#include "regionstr.h"
#include "Xprotostr.h"
#include "regionstr.h"
#include "mizerarc.h"
#include "mi.h"
#include "scrnintstr.h"
#include "xaa.h"
#include "xaalocal.h"
#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"

#include "xf86Pci.h"

#include "mga_bios.h"
#include "mga.h"
#include "mga_reg.h"
#include "mga_macros.h"


/* 
   This is only faster than cfb for stuff other than GXcopy.  
   And even then, only when pci_retries are on.  
*/


#define DRAW_POINT(x, y) { \
	tmp = x; \
	OUTREG(MGAREG_FXBNDRY, (tmp) | (((tmp) + 1) << 16)); \
	OUTREG(MGAREG_YDSTLEN + MGAREG_EXEC, ((y) << 16) | 1); \
}

static void
MGAZeroArc(
    DrawablePtr pDraw,
    GCPtr pGC,
    xArc *arc
){
    int yoffset, dyoffset, x, y, a, b, d, mask, k1, k3, dx, dy, tmp;
    XAAInfoRecPtr infoRec = GET_XAAINFORECPTR_FROM_GC(pGC);
    ScrnInfoPtr pScrn = infoRec->pScrn;
    MGAPtr pMga = MGAPTR(pScrn);
    miZeroArcRec info;
    Bool do360;
    DDXPointRec org, orgo;

    CHECK_DMA_QUIESCENT( pMga, infoRec->pScrn );

    (*infoRec->SetupForSolidFill)(infoRec->pScrn, pGC->fgPixel, 
		pGC->alu, pGC->planemask);

    do360 = miZeroArcSetup(arc, &info, TRUE);
    org.y = info.yorg + pDraw->y;
    org.x = 0;
    orgo.y = info.yorgo + pDraw->y;
    orgo.x = 0;
    info.xorg += pDraw->x;
    info.xorgo += pDraw->x;

    MIARCSETUP();
    yoffset = y ? 1 : 0;
    dyoffset = 0;
    mask = info.initialMask;
    if (!(arc->width & 1)) {
	WAITFIFO(4);
	if (mask & 2)
	    DRAW_POINT(info.xorgo, org.y);
	if (mask & 8)
	    DRAW_POINT(info.xorgo, orgo.y);
    }
    if (!info.end.x || !info.end.y) {
	mask = info.end.mask;
	info.end = info.altend;
    }
    if (do360 && (arc->width == arc->height) && !(arc->width & 1)) {
        int xoffset = 1;
	DDXPointRec orghb, orgohb;

	orghb.y = org.y + info.h;
	orghb.x = org.x + info.xorg;
	orgohb.y = orghb.y;
	orgohb.x = orghb.x - info.h;

	org.x += info.xorg;
	orgo.x += info.xorg;
	orghb.x += info.h;
	while (1) {
	    WAITFIFO(16);
	    DRAW_POINT(org.x + x, org.y + yoffset);
	    DRAW_POINT(org.x - x, org.y + yoffset);
	    DRAW_POINT(orgo.x - x, orgo.y - yoffset);
	    DRAW_POINT(orgo.x + x, orgo.y - yoffset);
	    if (a < 0) break;
	    DRAW_POINT(orghb.x - y, orghb.y - xoffset);
	    DRAW_POINT(orgohb.x + y, orgohb.y - xoffset);
	    DRAW_POINT(orgohb.x + y, orgohb.y + xoffset);
	    DRAW_POINT(orghb.x - y, orghb.y + xoffset);
	    xoffset ++;
	    MIARCCIRCLESTEP(yoffset ++;);
	}
	org.x -= info.xorg;
	orgo.x -= info.xorg;
	x = info.w;
	yoffset = info.h;
    }
    else if (do360) {
	while (y < info.h || x < info.w) {
	    MIARCOCTANTSHIFT(dyoffset = 1;);
	    WAITFIFO(8);
	    DRAW_POINT(org.x + info.xorg + x, org.y + yoffset);
	    DRAW_POINT(org.x + info.xorgo - x, org.y + yoffset);
	    DRAW_POINT(orgo.x + info.xorgo - x, orgo.y - yoffset);
	    DRAW_POINT(orgo.x + info.xorg + x, orgo.y - yoffset);
	    MIARCSTEP(yoffset += dyoffset;, yoffset++;);
	}
    }
    else {
	while (y < info.h || x < info.w) {
	    MIARCOCTANTSHIFT(dyoffset = 1;);
	    if ((x == info.start.x) || (y == info.start.y)) {
		mask = info.start.mask;
		info.start = info.altstart;
	    }
	    WAITFIFO(8);
	    if (mask & 1)
		DRAW_POINT(org.x + info.xorg + x, org.y + yoffset);
	    if (mask & 2)
		DRAW_POINT(org.x + info.xorgo - x, org.y + yoffset);
	    if (mask & 4)
		DRAW_POINT(orgo.x + info.xorgo - x, orgo.y - yoffset);
	    if (mask & 8)
		DRAW_POINT(orgo.x + info.xorg + x, orgo.y - yoffset);
	    if ((x == info.end.x) || (y == info.end.y)) {
		mask = info.end.mask;
		info.end = info.altend;
	    }
	    MIARCSTEP(yoffset += dyoffset;, yoffset++;);
	}
    }
    if ((x == info.start.x) || (y == info.start.y))
	mask = info.start.mask;
     
    WAITFIFO(4);
    if (mask & 1)
	DRAW_POINT(org.x + info.xorg + x, org.y + yoffset);
    if (mask & 4)
	DRAW_POINT(orgo.x + info.xorgo - x, orgo.y - yoffset);
    if (arc->height & 1) {
	WAITFIFO(4);
	if (mask & 2)
	    DRAW_POINT(org.x + info.xorgo - x, org.y + yoffset);
	if (mask & 8)
	    DRAW_POINT(orgo.x + info.xorg + x, orgo.y - yoffset);
    }

    SET_SYNC_FLAG(infoRec);
}

void
MGAPolyArcThinSolid (
    DrawablePtr	pDraw,
    GCPtr	pGC,
    int		narcs,
    xArc	*parcs
){
    xArc *arc;
    BoxRec box;
    int i, x2, y2;
    RegionPtr cclip;

    cclip = pGC->pCompositeClip;

    if(!REGION_NUM_RECTS(cclip))
	return;

    for (arc = parcs, i = narcs; --i >= 0; arc++) {
	if (miCanZeroArc(arc)) {
	    box.x1 = arc->x + pDraw->x;
	    box.y1 = arc->y + pDraw->y;
 	    x2 = box.x1 + (int)arc->width + 1;
 	    box.x2 = x2;
 	    y2 = box.y1 + (int)arc->height + 1;
 	    box.y2 = y2;
 	    if ( (x2 <= MAXSHORT) && (y2 <= MAXSHORT) &&
 		    (RECT_IN_REGION(pDraw->pScreen, cclip, &box) == rgnIN) )
		MGAZeroArc (pDraw, pGC, arc);
	    else
		miZeroPolyArc(pDraw, pGC, 1, arc);
	}
	else
	    miPolyArc(pDraw, pGC, 1, arc);
    }
}

