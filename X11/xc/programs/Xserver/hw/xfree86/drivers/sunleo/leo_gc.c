/*
 * Acceleration for the Leo (ZX) framebuffer - GC implementation.
 *
 * Copyright (C) 1999, 2000 Jakub Jelinek (jakub@redhat.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * JAKUB JELINEK BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunleo/leo_gc.c,v 1.2 2000/12/01 00:24:35 dawes Exp $ */

#define PSZ 32

#include "leo.h"
#include "leo_regs.h"
#include "leo_gc.h"

#include "X.h"
#include "Xmd.h"
#include "Xproto.h"
#include "cfb.h"
#include "fontstruct.h"
#include "dixfontstr.h"
#include "gcstruct.h"
#include "windowstr.h"
#include "pixmapstr.h"
#include "scrnintstr.h"
#include "region.h"

#include "mistruct.h"
#include "mibstore.h"
#include "migc.h"

#include "cfbmskbits.h"
#include "cfb8bit.h"

void LeoValidateGC(GCPtr, unsigned long, DrawablePtr);
static void LeoDestroyGC(GCPtr);

GCFuncs LeoGCFuncs = {
	LeoValidateGC,
	miChangeGC,
	miCopyGC,
	LeoDestroyGC,
	miChangeClip,
	miDestroyClip,
	miCopyClip,
};

GCOps	LeoTEOps1Rect = {
	cfbSolidSpansCopy,
	cfbSetSpans,
	cfbPutImage,
	LeoCopyArea,
	cfbCopyPlane,
	cfbPolyPoint,
	cfb8LineSS1Rect,
	cfb8SegmentSS1Rect,
	miPolyRectangle,
	cfbZeroPolyArcSS8Copy,
	cfbFillPoly1RectCopy,
	LeoPolyFillRect1Rect,
	cfbPolyFillArcSolidCopy,
	miPolyText8,
	miPolyText16,
	miImageText8,
	miImageText16,
	LeoTEGlyphBlt,
	LeoPolyTEGlyphBlt,
	mfbPushPixels
#ifdef NEED_LINEHELPER
	,NULL
#endif
};

GCOps	LeoNonTEOps1Rect = {
	cfbSolidSpansCopy,
	cfbSetSpans,
	cfbPutImage,
	LeoCopyArea,
	cfbCopyPlane,
	cfbPolyPoint,
	cfb8LineSS1Rect,
	cfb8SegmentSS1Rect,
	miPolyRectangle,
	cfbZeroPolyArcSS8Copy,
	cfbFillPoly1RectCopy,
	LeoPolyFillRect1Rect,
	cfbPolyFillArcSolidCopy,
	miPolyText8,
	miPolyText16,
	miImageText8,
	miImageText16,
	cfbImageGlyphBlt8,
	LeoPolyGlyphBlt,
	mfbPushPixels
#ifdef NEED_LINEHELPER
	,NULL
#endif
};

GCOps	LeoTEOps = {
	cfbSolidSpansCopy,
	cfbSetSpans,
	cfbPutImage,
	LeoCopyArea,
	cfbCopyPlane,
	cfbPolyPoint,
	cfbLineSS,
	cfbSegmentSS,
	miPolyRectangle,
	cfbZeroPolyArcSS8Copy,
	miFillPolygon,
	LeoPolyFillRect,
	cfbPolyFillArcSolidCopy,
	miPolyText8,
	miPolyText16,
	miImageText8,
	miImageText16,
	LeoTEGlyphBlt,
	LeoPolyTEGlyphBlt,
	mfbPushPixels
#ifdef NEED_LINEHELPER
	,NULL
#endif
};

GCOps	LeoNonTEOps = {
	cfbSolidSpansCopy,
	cfbSetSpans,
	cfbPutImage,
	LeoCopyArea,
	cfbCopyPlane,
	cfbPolyPoint,
	cfbLineSS,
	cfbSegmentSS,
	miPolyRectangle,
	cfbZeroPolyArcSS8Copy,
	miFillPolygon,
	LeoPolyFillRect,
	cfbPolyFillArcSolidCopy,
	miPolyText8,
	miPolyText16,
	miImageText8,
	miImageText16,
	cfbImageGlyphBlt8,
	LeoPolyGlyphBlt,
	mfbPushPixels
#ifdef NEED_LINEHELPER
	,NULL
#endif
};

GCOps *
LeoMatchCommon (GCPtr pGC, cfbPrivGCPtr devPriv)
{
	if (pGC->lineWidth != 0)
		return 0;
	if (pGC->lineStyle != LineSolid)
		return 0;
	if (pGC->fillStyle != FillSolid)
		return 0;
	if (devPriv->rop != GXcopy)
		return 0;
	if (pGC->font &&
		FONTMAXBOUNDS(pGC->font,rightSideBearing) -
		FONTMINBOUNDS(pGC->font,leftSideBearing) <= 32 &&
		FONTMINBOUNDS(pGC->font,characterWidth) >= 0) {
		if (TERMINALFONT(pGC->font))
			if (devPriv->oneRect)
				return &LeoTEOps1Rect;
			else
				return &LeoTEOps;
		else
			if (devPriv->oneRect)
				return &LeoNonTEOps1Rect;
			else
				return &LeoNonTEOps;
	}
	return 0;
}

Bool
LeoCreateGC(GCPtr pGC)
{
	LeoPrivGCPtr gcPriv;

	if (pGC->depth == 1)
		return mfbCreateGC (pGC);
	if (!cfbCreateGC (pGC))
		return FALSE;

	pGC->ops = & LeoNonTEOps;
	pGC->funcs = & LeoGCFuncs;
	gcPriv = LeoGetGCPrivate (pGC);
	gcPriv->type = DRAWABLE_WINDOW;
	gcPriv->stipple = 0;
	return TRUE;
}

static void
LeoDestroyGC (GCPtr pGC)
{
	LeoPrivGCPtr gcPriv = LeoGetGCPrivate (pGC);
        
	if (gcPriv->stipple)
		xfree (gcPriv->stipple);
	miDestroyGC (pGC);
}

void
LeoValidateGC(GCPtr pGC, unsigned long changes, DrawablePtr pDrawable)
{
	int mask;
	int index;
	int new_rrop;
	int new_line, new_text, new_fillspans, new_fillarea;
	int new_rotate;
	int xrot, yrot;
	/* flags for changing the proc vector */
	LeoPrivGCPtr gcPriv;
        cfbPrivGCPtr devPriv;
	int oneRect, type;
	LeoPtr pLeo = LeoGetScreenPrivate (pDrawable->pScreen);
	
	gcPriv = LeoGetGCPrivate (pGC);
	type = pLeo->vtSema ? -1 : pDrawable->type;
	if (type != DRAWABLE_WINDOW) {
		if (gcPriv->type == DRAWABLE_WINDOW) {
			extern GCOps cfbNonTEOps;

			miDestroyGCOps (pGC->ops);
			pGC->ops = &cfbNonTEOps;
			changes = (1 << (GCLastBit+1)) - 1;
			pGC->stateChanges = changes;
			gcPriv->type = type;
		}
		cfbValidateGC (pGC, changes, pDrawable);
		return;
	}

	if (gcPriv->type != DRAWABLE_WINDOW) {
		changes = (1 << (GCLastBit+1)) - 1;
		gcPriv->type = DRAWABLE_WINDOW;
	}

	new_rotate = pGC->lastWinOrg.x != pDrawable->x ||
		 pGC->lastWinOrg.y != pDrawable->y;

	pGC->lastWinOrg.x = pDrawable->x;
	pGC->lastWinOrg.y = pDrawable->y;
	devPriv = cfbGetGCPrivate(pGC);

	new_rrop = FALSE;
	new_line = FALSE;
	new_text = FALSE;
	new_fillspans = FALSE;
	new_fillarea = FALSE;

	/*
	 * if the client clip is different or moved OR the subwindowMode has
	 * changed OR the window's clip has changed since the last validation
	 * we need to recompute the composite clip 
	 */

	if ((changes & (GCClipXOrigin|GCClipYOrigin|GCClipMask|GCSubwindowMode)) ||
	    (pDrawable->serialNumber != (pGC->serialNumber & DRAWABLE_SERIAL_BITS))) {
		miComputeCompositeClip (pGC, pDrawable);
		oneRect = REGION_NUM_RECTS(cfbGetCompositeClip(pGC)) == 1;
		if (oneRect != devPriv->oneRect)
			new_line = TRUE;
		devPriv->oneRect = oneRect;
	}

	mask = changes;
	while (mask) {
		index = lowbit (mask);
		mask &= ~index;

		/*
		 * this switch acculmulates a list of which procedures might have
		 * to change due to changes in the GC.  in some cases (e.g.
		 * changing one 16 bit tile for another) we might not really need
		 * a change, but the code is being paranoid. this sort of batching
		 * wins if, for example, the alu and the font have been changed,
		 * or any other pair of items that both change the same thing. 
		 */
		switch (index) {
		case GCFunction:
		case GCForeground:
			new_rrop = TRUE;
			break;
		case GCPlaneMask:
			new_rrop = TRUE;
			new_text = TRUE;
			break;
		case GCBackground:
			break;
		case GCLineStyle:
		case GCLineWidth:
			new_line = TRUE;
			break;
		case GCJoinStyle:
		case GCCapStyle:
			break;
		case GCFillStyle:
			new_text = TRUE;
			new_fillspans = TRUE;
			new_line = TRUE;
			new_fillarea = TRUE;
			break;
		case GCFillRule:
			break;
		case GCTile:
			new_fillspans = TRUE;
			new_fillarea = TRUE;
			break;

		case GCStipple:
			if (pGC->stipple) {
			int width = pGC->stipple->drawable.width;
				PixmapPtr nstipple;

				if ((width <= PGSZ) && !(width & (width - 1)) &&
				    (nstipple = cfbCopyPixmap(pGC->stipple))) {
					cfbPadPixmap(nstipple);
					(*pGC->pScreen->DestroyPixmap)(pGC->stipple);
					pGC->stipple = nstipple;
				}
			}
			new_fillspans = TRUE;
			new_fillarea = TRUE;
			break;

		case GCTileStipXOrigin:
			new_rotate = TRUE;
			break;

		case GCTileStipYOrigin:
			new_rotate = TRUE;
			break;

		case GCFont:
			new_text = TRUE;
			break;
		case GCSubwindowMode:
			break;
		case GCGraphicsExposures:
			break;
		case GCClipXOrigin:
			break;
		case GCClipYOrigin:
			break;
		case GCClipMask:
			break;
		case GCDashOffset:
			break;
		case GCDashList:
			break;
		case GCArcMode:
			break;
		default:
			break;
		}
	}

	/*
	 * If the drawable has changed,  ensure suitable
	 * entries are in the proc vector. 
	 */
	if (pDrawable->serialNumber != (pGC->serialNumber & (DRAWABLE_SERIAL_BITS))) {
		new_fillspans = TRUE;	/* deal with FillSpans later */
	}

	if (new_rotate || new_fillspans) {
		Bool new_pix = FALSE;

		xrot = pGC->patOrg.x + pDrawable->x;
		yrot = pGC->patOrg.y + pDrawable->y;

		LeoCheckFill (pGC, pDrawable);
		
		switch (pGC->fillStyle) {
		case FillTiled:
			if (!pGC->tileIsPixel) {
				int width = pGC->tile.pixmap->drawable.width * PSZ;

				if ((width <= 32) && !(width & (width - 1))) {
					cfbCopyRotatePixmap(pGC->tile.pixmap,
							    &pGC->pRotatedPixmap,
							    xrot, yrot);
					new_pix = TRUE;
				}
			}
			break;
		}

		if (!new_pix && pGC->pRotatedPixmap) {
			(*pGC->pScreen->DestroyPixmap)(pGC->pRotatedPixmap);
			pGC->pRotatedPixmap = (PixmapPtr) NULL;
		}
	}

	if (new_rrop) {
		int old_rrop;
		
		if (gcPriv->stipple) {
			if (pGC->fillStyle == FillStippled)
				gcPriv->stipple->alu = pGC->alu | 0x80;
			else
				gcPriv->stipple->alu = pGC->alu;
			if (pGC->fillStyle != FillTiled) {
				gcPriv->stipple->fg = pGC->fgPixel;
				gcPriv->stipple->bg = pGC->bgPixel;
			}
                }

		old_rrop = devPriv->rop;
		devPriv->rop = cfbReduceRasterOp (pGC->alu, pGC->fgPixel,
						  pGC->planemask,
						  &devPriv->and, &devPriv->xor);
		if (old_rrop == devPriv->rop)
			new_rrop = FALSE;
		else {
			new_line = TRUE;
			new_text = TRUE;
			new_fillspans = TRUE;
			new_fillarea = TRUE;
		}
	}

	if (new_rrop || new_fillspans || new_text || new_fillarea || new_line) {
		GCOps	*newops;

		if ((newops = LeoMatchCommon (pGC, devPriv)) != NULL) {
			if (pGC->ops->devPrivate.val)
			miDestroyGCOps (pGC->ops);
			pGC->ops = newops;
			new_rrop = new_line = new_fillspans = new_text = new_fillarea = 0;
		} else {
			if (!pGC->ops->devPrivate.val) {
				pGC->ops = miCreateGCOps (pGC->ops);
				pGC->ops->devPrivate.val = 1;
			}
			pGC->ops->CopyArea = LeoCopyArea;
		}
	}

	/* deal with the changes we've collected */
	if (new_line) {
		pGC->ops->FillPolygon = miFillPolygon;
		if (devPriv->oneRect && pGC->fillStyle == FillSolid) {
			switch (devPriv->rop) {
			case GXcopy:
				pGC->ops->FillPolygon = cfbFillPoly1RectCopy;
				break;
			default:
				pGC->ops->FillPolygon = LeoFillPoly1RectGeneral;
				break;
			}
		}
		if (pGC->lineWidth == 0) {
			if ((pGC->lineStyle == LineSolid) && (pGC->fillStyle == FillSolid)) {
				switch (devPriv->rop) {
				case GXcopy:
					pGC->ops->PolyArc = cfbZeroPolyArcSS8Copy;
					break;
				default:
					pGC->ops->PolyArc = LeoZeroPolyArcSS8General;
					break;
				}
			} else
				pGC->ops->PolyArc = miZeroPolyArc;
		} else
			pGC->ops->PolyArc = miPolyArc;
		pGC->ops->PolySegment = miPolySegment;
		switch (pGC->lineStyle) {
		case LineSolid:
			if(pGC->lineWidth == 0) {
				if (pGC->fillStyle == FillSolid) {
					if (devPriv->oneRect &&
					    ((pDrawable->x >= pGC->pScreen->width - 32768) &&
					    (pDrawable->y >= pGC->pScreen->height - 32768))) {
						pGC->ops->Polylines = cfb8LineSS1Rect;
						pGC->ops->PolySegment = cfb8SegmentSS1Rect;
					} else {
						pGC->ops->Polylines = cfbLineSS;
						pGC->ops->PolySegment = cfbSegmentSS;
					}
				} else
					pGC->ops->Polylines = miZeroLine;
			} else
				pGC->ops->Polylines = miWideLine;
			break;
		case LineOnOffDash:
		case LineDoubleDash:
			if (pGC->lineWidth == 0 && pGC->fillStyle == FillSolid) {
				pGC->ops->Polylines = cfbLineSD;
				pGC->ops->PolySegment = cfbSegmentSD;
			} else
				pGC->ops->Polylines = miWideDash;
			break;
		}
	}

	if (new_text && pGC->font) {
		if (FONTMAXBOUNDS(pGC->font,rightSideBearing) -
		    FONTMINBOUNDS(pGC->font,leftSideBearing) > 32 ||
		    FONTMINBOUNDS(pGC->font,characterWidth) < 0) {
			pGC->ops->PolyGlyphBlt = miPolyGlyphBlt;
			pGC->ops->ImageGlyphBlt = miImageGlyphBlt;
		} else {
			if (pGC->fillStyle == FillSolid) {
				if (TERMINALFONT (pGC->font))
					pGC->ops->PolyGlyphBlt = LeoPolyTEGlyphBlt;
				else
					pGC->ops->PolyGlyphBlt = LeoPolyGlyphBlt;
			} else
				pGC->ops->PolyGlyphBlt = miPolyGlyphBlt;
				
			/* special case ImageGlyphBlt for terminal emulator fonts */
			if (TERMINALFONT (pGC->font))
				pGC->ops->ImageGlyphBlt = LeoTEGlyphBlt;
			else
				pGC->ops->ImageGlyphBlt = miImageGlyphBlt;
		}
	}	

	if (new_fillspans) {
		switch (pGC->fillStyle) {
		case FillSolid:
			pGC->ops->FillSpans = LeoFillSpansSolid;
			break;
		case FillTiled:
			if (pGC->pRotatedPixmap) {
				if (pGC->alu == GXcopy && (pGC->planemask & PMSK) == PMSK)
					pGC->ops->FillSpans = cfbTile32FSCopy;
				else
					pGC->ops->FillSpans = LeoTile32FSGeneral;
			} else
				pGC->ops->FillSpans = cfbUnnaturalTileFS;
			break;
		case FillStippled:
			pGC->ops->FillSpans = cfbUnnaturalStippleFS;
			break;
		case FillOpaqueStippled:
			pGC->ops->FillSpans = cfbUnnaturalStippleFS;
			break;
		default:
			FatalError("LeoValidateGC: illegal fillStyle\n");
		}
		if (gcPriv->stipple)
			pGC->ops->FillSpans = LeoFillSpansStippled;
	} /* end of new_fillspans */

	if (new_fillarea) {
		pGC->ops->PolyFillRect = miPolyFillRect;
		if (pGC->fillStyle == FillSolid) {
			if (devPriv->oneRect)
				pGC->ops->PolyFillRect = LeoPolyFillRect1Rect;
			else
				pGC->ops->PolyFillRect = LeoPolyFillRect;
		} else if (gcPriv->stipple)
			pGC->ops->PolyFillRect = LeoPolyFillStippledRect;
		else if (pGC->fillStyle == FillTiled)
			pGC->ops->PolyFillRect = cfbPolyFillRect;
		pGC->ops->PolyFillArc = miPolyFillArc;
		if (pGC->fillStyle == FillSolid) {
			switch (devPriv->rop) {
			case GXcopy:
				pGC->ops->PolyFillArc = cfbPolyFillArcSolidCopy;
				break;
			default:
				pGC->ops->PolyFillArc = LeoPolyFillArcSolidGeneral;
				break;
			}
		}
	}
}
