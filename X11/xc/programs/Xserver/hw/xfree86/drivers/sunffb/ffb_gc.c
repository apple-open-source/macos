/*
 * Acceleration for the Creator and Creator3D framebuffer - GC implementation.
 *
 * Copyright (C) 1998,1999,2000 Jakub Jelinek (jakub@redhat.com)
 * Copyright (C) 1998 Michal Rehacek (majkl@iname.com)
 * Copyright (C) 1999 David S. Miller (davem@redhat.com)
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
 * JAKUB JELINEK, MICHAL REHACEK, OR DAVID MILLER BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunffb/ffb_gc.c,v 1.3 2000/12/01 00:24:34 dawes Exp $ */

#include "ffb.h"
#include "ffb_regs.h"
#include "ffb_rcache.h"
#include "ffb_fifo.h"
#include "ffb_loops.h"
#include "ffb_gc.h"

#include "scrnintstr.h"
#include "pixmapstr.h"
#include "fontstruct.h"
#include "dixfontstr.h"

#define PSZ 8
#include "cfb.h"
#undef PSZ
#include "cfb32.h"

#include "migc.h"
#include "mi.h"
#include "mispans.h"

GCOps CreatorTEOps1Rect8 = {
	CreatorFillSpans,
	CreatorSetSpans,
	cfbPutImage,
	CreatorCopyArea,
	CreatorCopyPlane,
	CreatorPolyPoint,
	CreatorPolylines,
	CreatorPolySegment,
	miPolyRectangle,
	CreatorZeroPolyArc,
	CreatorFillPolygon,
	CreatorPolyFillRect,
	CreatorPolyFillArcSolid,
	miPolyText8,
	miPolyText16,
	miImageText8,
	miImageText16,
	CreatorTEGlyphBlt,
	CreatorPolyTEGlyphBlt,
	miPushPixels
#ifdef NEED_LINEHELPER
	,NULL
#endif
};

GCOps CreatorTEOps1Rect32 = {
	CreatorFillSpans,
	CreatorSetSpans,
	cfb32PutImage,
	CreatorCopyArea,
	CreatorCopyPlane,
	CreatorPolyPoint,
	CreatorPolylines,
	CreatorPolySegment,
	miPolyRectangle,
	CreatorZeroPolyArc,
	CreatorFillPolygon,
	CreatorPolyFillRect,
	CreatorPolyFillArcSolid,
	miPolyText8,
	miPolyText16,
	miImageText8,
	miImageText16,
	CreatorTEGlyphBlt,
	CreatorPolyTEGlyphBlt,
	miPushPixels
#ifdef NEED_LINEHELPER
	,NULL
#endif
};

GCOps CreatorTEOps8 = {
	CreatorFillSpans,
	CreatorSetSpans,
	cfbPutImage,
	CreatorCopyArea,
	CreatorCopyPlane,
	CreatorPolyPoint,
	CreatorLineSSStub,
	CreatorSegmentSSStub,
	miPolyRectangle,
	CreatorZeroPolyArc,
	CreatorFillPolygon,
	CreatorPolyFillRect,
	CreatorPolyFillArcSolid,
	miPolyText8,
	miPolyText16,
	miImageText8,
	miImageText16,
	CreatorTEGlyphBlt,
	CreatorPolyTEGlyphBlt,
	miPushPixels
#ifdef NEED_LINEHELPER
	,NULL
#endif
};

GCOps CreatorTEOps32 = {
	CreatorFillSpans,
	CreatorSetSpans,
	cfb32PutImage,
	CreatorCopyArea,
	CreatorCopyPlane,
	CreatorPolyPoint,
	CreatorLineSSStub,
	CreatorSegmentSSStub,
	miPolyRectangle,
	CreatorZeroPolyArc,
	CreatorFillPolygon,
	CreatorPolyFillRect,
	CreatorPolyFillArcSolid,
	miPolyText8,
	miPolyText16,
	miImageText8,
	miImageText16,
	CreatorTEGlyphBlt,
	CreatorPolyTEGlyphBlt,
	miPushPixels
#ifdef NEED_LINEHELPER
	,NULL
#endif
};

GCOps CreatorNonTEOps1Rect8 = {
	CreatorFillSpans,
	CreatorSetSpans,
	cfbPutImage,
	CreatorCopyArea,
	CreatorCopyPlane,
	CreatorPolyPoint,
	CreatorPolylines,
	CreatorPolySegment,
	miPolyRectangle,
	CreatorZeroPolyArc,
	CreatorFillPolygon,
	CreatorPolyFillRect,
	CreatorPolyFillArcSolid,
	miPolyText8,
	miPolyText16,
	miImageText8,
	miImageText16,
	miImageGlyphBlt,
	CreatorPolyGlyphBlt,
	miPushPixels
#ifdef NEED_LINEHELPER
	,NULL
#endif
};

GCOps CreatorNonTEOps1Rect32 = {
	CreatorFillSpans,
	CreatorSetSpans,
	cfb32PutImage,
	CreatorCopyArea,
	CreatorCopyPlane,
	CreatorPolyPoint,
	CreatorPolylines,
	CreatorPolySegment,
	miPolyRectangle,
	CreatorZeroPolyArc,
	CreatorFillPolygon,
	CreatorPolyFillRect,
	CreatorPolyFillArcSolid,
	miPolyText8,
	miPolyText16,
	miImageText8,
	miImageText16,
	miImageGlyphBlt,
	CreatorPolyGlyphBlt,
	miPushPixels
#ifdef NEED_LINEHELPER
	,NULL
#endif
};

GCOps CreatorNonTEOps8 = {
	CreatorFillSpans,
	CreatorSetSpans,
	cfbPutImage,
	CreatorCopyArea,
	CreatorCopyPlane,
	CreatorPolyPoint,
	CreatorLineSSStub,
	CreatorSegmentSSStub,
	miPolyRectangle,
	CreatorZeroPolyArc,
	CreatorFillPolygon,
	CreatorPolyFillRect,
	CreatorPolyFillArcSolid,
	miPolyText8,
	miPolyText16,
	miImageText8,
	miImageText16,
	miImageGlyphBlt,
	CreatorPolyGlyphBlt,
	miPushPixels
#ifdef NEED_LINEHELPER
	,NULL
#endif
};

GCOps CreatorNonTEOps32 = {
	CreatorFillSpans,
	CreatorSetSpans,
	cfb32PutImage,
	CreatorCopyArea,
	CreatorCopyPlane,
	CreatorPolyPoint,
	CreatorLineSSStub,
	CreatorSegmentSSStub,
	miPolyRectangle,
	CreatorZeroPolyArc,
	CreatorFillPolygon,
	CreatorPolyFillRect,
	CreatorPolyFillArcSolid,
	miPolyText8,
	miPolyText16,
	miImageText8,
	miImageText16,
	miImageGlyphBlt,
	CreatorPolyGlyphBlt,
	miPushPixels
#ifdef NEED_LINEHELPER
	,NULL
#endif
};

#define FONTWIDTH(font)	(FONTMAXBOUNDS(font,rightSideBearing) - \
			 FONTMINBOUNDS(font,leftSideBearing))
#define FONTHEIGHT(font) (FONTMAXBOUNDS(font,ascent) + \
			  FONTMINBOUNDS(font,descent))

static GCOps *
CreatorMatchCommon (GCPtr pGC, cfbPrivGCPtr devPriv)
{
	int depth = pGC->depth;

	if (pGC->lineWidth != 0) return 0;
	if (pGC->lineStyle != LineSolid) return 0;
	if (pGC->fillStyle != FillSolid) return 0;
	if (devPriv->rop != GXcopy) return 0;
	if (pGC->font &&
	    FONTWIDTH (pGC->font) <= 32 &&
	    FONTHEIGHT (pGC->font) <= 100 &&
	    FONTMINBOUNDS(pGC->font,characterWidth) >= 0) {
		if (TERMINALFONT(pGC->font)) {
			if (devPriv->oneRect) {
				return (depth == 8 ?
					&CreatorTEOps1Rect8 :
					&CreatorTEOps1Rect32);
			} else {
				return (depth == 8 ?
					&CreatorTEOps8 :
					&CreatorTEOps32);
			}
		} else {
			if (devPriv->oneRect) {
				return (depth == 8 ?
					&CreatorNonTEOps1Rect8 :
					&CreatorNonTEOps1Rect32);
			} else {
				return (depth == 8 ?
					&CreatorNonTEOps8 :
					&CreatorNonTEOps32);
			}
		}
	}
	return 0;
}

static void
CreatorDestroyGC (GCPtr pGC)
{
	CreatorPrivGCPtr gcPriv = CreatorGetGCPrivate (pGC);
        
	if (gcPriv->stipple)
		xfree (gcPriv->stipple);
	miDestroyGC (pGC);
}

static __inline__ void
CreatorNewLine(GCPtr pGC, cfbPrivGCPtr devPriv, CreatorPrivGCPtr gcPriv, int accel)
{
	pGC->ops->FillPolygon = miFillPolygon;
	pGC->ops->PolyRectangle = miPolyRectangle;
	if (pGC->lineWidth == 0)
		pGC->ops->PolyArc = miZeroPolyArc;
	else
		pGC->ops->PolyArc = miPolyArc;
	if (accel) {
		pGC->ops->FillPolygon = CreatorFillPolygon;
		if (pGC->lineWidth == 0 && pGC->capStyle != CapNotLast)
			pGC->ops->PolyArc = CreatorZeroPolyArc;
	}
	pGC->ops->PolySegment = miPolySegment;
	gcPriv->linepat = 0;

	/* Segment and Line ops are only accelerated if there is
	 * one clipping region.
	 */
	if (accel && !devPriv->oneRect)
		accel = 0;

	if (pGC->lineStyle == LineSolid) {
		if(pGC->lineWidth == 0) {
			if (pGC->fillStyle == FillSolid) {
				pGC->ops->Polylines = CreatorLineSSStub;
				pGC->ops->PolySegment = CreatorSegmentSSStub;
			} else
				pGC->ops->Polylines = miZeroLine;
			if (accel) {
				gcPriv->PolySegment = pGC->ops->PolySegment;
				gcPriv->Polylines = pGC->ops->Polylines;
				pGC->ops->PolySegment = CreatorPolySegment;
				pGC->ops->Polylines = CreatorPolylines;
			}
		} else {
			pGC->ops->Polylines = miWideLine;
		}
	} else if(pGC->lineStyle == LineOnOffDash) {
		if (pGC->lineWidth == 0 && pGC->fillStyle == FillSolid) {
			pGC->ops->Polylines = CreatorLineSDStub;
			pGC->ops->PolySegment = CreatorSegmentSDStub;
			if(accel &&
			   CreatorCheckLinePattern(pGC, gcPriv)) {
				gcPriv->PolySegment = pGC->ops->PolySegment;
				gcPriv->Polylines = pGC->ops->Polylines;
				pGC->ops->PolySegment = CreatorPolySegment;
				pGC->ops->Polylines = CreatorPolylines;
			}
		} else {
			pGC->ops->Polylines = miWideDash;
		}
	} else if(pGC->lineStyle == LineDoubleDash) {
		if (pGC->lineWidth == 0 && pGC->fillStyle == FillSolid) {
			pGC->ops->Polylines = CreatorLineSDStub;
			pGC->ops->PolySegment = CreatorSegmentSDStub;
		} else {
			pGC->ops->Polylines = miWideDash;
		}
	}
}

static __inline__ void
CreatorNewGlyph(GCPtr pGC, CreatorPrivGCPtr gcPriv)
{
	if (FONTWIDTH(pGC->font) > 32 ||
	    FONTHEIGHT(pGC->font) > 100 ||
	    FONTMINBOUNDS(pGC->font,characterWidth) < 0) {
		pGC->ops->PolyGlyphBlt = miPolyGlyphBlt;
		pGC->ops->ImageGlyphBlt = miImageGlyphBlt;
	} else {
		if (pGC->fillStyle == FillSolid) {
			if (TERMINALFONT (pGC->font)) {
				pGC->ops->PolyGlyphBlt = CreatorPolyTEGlyphBlt;
			} else {
				pGC->ops->PolyGlyphBlt = CreatorPolyGlyphBlt;
			}
		} else {
			pGC->ops->PolyGlyphBlt = miPolyGlyphBlt;
		}			

		/* special case ImageGlyphBlt for terminal emulator fonts */
		if (TERMINALFONT(pGC->font))
			pGC->ops->ImageGlyphBlt = CreatorTEGlyphBlt;
		else
			pGC->ops->ImageGlyphBlt = miImageGlyphBlt;
	}
}

static __inline__ void
CreatorNewFillSpans(GCPtr pGC, cfbPrivGCPtr devPriv, CreatorPrivGCPtr gcPriv, int accel)
{
	if (pGC->fillStyle == FillSolid) {
		pGC->ops->FillSpans = CreatorSolidSpansGeneralStub;
	} else if(pGC->fillStyle == FillTiled) {
		if (pGC->pRotatedPixmap) {
			int pmsk = (pGC->depth == 8 ? 0xff : 0xffffff);
			if (pGC->alu == GXcopy && (pGC->planemask & pmsk) == pmsk)
				pGC->ops->FillSpans = CreatorTile32FSCopyStub;
			else
				pGC->ops->FillSpans = CreatorTile32FSGeneralStub;
		} else
			pGC->ops->FillSpans = CreatorUnnaturalTileFSStub;
	} else if(pGC->fillStyle == FillStippled) {
		if (pGC->pRotatedPixmap)
			pGC->ops->FillSpans = Creator8Stipple32FSStub;
		else
			pGC->ops->FillSpans = CreatorUnnaturalStippleFSStub;
	} else if(pGC->fillStyle == FillOpaqueStippled) {
		if (pGC->pRotatedPixmap)
			pGC->ops->FillSpans = Creator8OpaqueStipple32FSStub;
		else
			pGC->ops->FillSpans = CreatorUnnaturalStippleFSStub;
	} else
		FatalError("CreatorValidateGC: illegal fillStyle\n");
	if (accel)
		pGC->ops->FillSpans = CreatorFillSpans;
}

static __inline__ void
CreatorNewFillArea(GCPtr pGC, cfbPrivGCPtr devPriv, CreatorPrivGCPtr gcPriv, int accel)
{
  	if (accel) {
		pGC->ops->PolyFillRect = CreatorPolyFillRect;
		pGC->ops->PolyFillArc = CreatorPolyFillArcSolid;
	} else {
		pGC->ops->PolyFillRect = miPolyFillRect;
		if(pGC->fillStyle == FillSolid || pGC->fillStyle == FillTiled)
			pGC->ops->PolyFillRect = CreatorPolyFillRectStub;
		pGC->ops->PolyFillArc = miPolyFillArc;
	}
	pGC->ops->PushPixels = mfbPushPixels;
}

void
CreatorValidateGC (GCPtr pGC, Mask changes, DrawablePtr pDrawable)
{
	int	mask;		/* stateChanges */
	int	new_rrop;
	int	new_line, new_text, new_fillspans, new_fillarea;
	int	new_rotate;
	int	xrot, yrot;
	/* flags for changing the proc vector */
	cfbPrivGCPtr devPriv;
	CreatorPrivGCPtr gcPriv;
	int	oneRect, type;
	int	accel, drawableChanged;
	FFBPtr  pFfb = GET_FFB_FROM_SCREEN(pDrawable->pScreen);

	gcPriv = CreatorGetGCPrivate (pGC);
	type = pFfb->vtSema ? -1 : pDrawable->type;
	if (type != DRAWABLE_WINDOW) {
		if (gcPriv->type == DRAWABLE_WINDOW) {
			extern GCOps cfbNonTEOps;
			extern GCOps cfb32NonTEOps;

			miDestroyGCOps (pGC->ops);

			if (pGC->depth == 8)
				pGC->ops = &cfbNonTEOps;
			else
				pGC->ops = &cfb32NonTEOps;

			changes = (1 << (GCLastBit+1)) - 1;
			pGC->stateChanges = changes;
			gcPriv->type = type;
		}
		if (pGC->depth == 8)
			cfbValidateGC (pGC, changes, pDrawable);
		else
			cfb32ValidateGC (pGC, changes, pDrawable);

		/* Our high speed VIS copyarea can
		 * be used on pixmaps too.
		 * But don't clobber someones ops prototype!!
		 */
		if (!pGC->ops->devPrivate.val) {
			pGC->ops = miCreateGCOps(pGC->ops);
			pGC->ops->devPrivate.val = 1;
		}
		pGC->ops->CopyArea = CreatorCopyArea;
		return;
	}

	if (gcPriv->type != DRAWABLE_WINDOW) {
		changes = (1 << (GCLastBit+1)) - 1;
		gcPriv->type = DRAWABLE_WINDOW;
	}

	new_rotate = pGC->lastWinOrg.x != pDrawable->x || 
		     pGC->lastWinOrg.y != pDrawable->y;
	if(new_rotate != 0) {
		pGC->lastWinOrg.x = pDrawable->x;
		pGC->lastWinOrg.y = pDrawable->y;
	}

	devPriv = cfbGetGCPrivate(pGC);
	new_rrop = FALSE;
	new_line = FALSE;
	new_text = FALSE; 
	new_fillspans = FALSE;
	new_fillarea = FALSE;

	drawableChanged = (pDrawable->serialNumber !=
			   (pGC->serialNumber & (DRAWABLE_SERIAL_BITS)));
#define CLIP_BITS	(GCClipXOrigin|GCClipYOrigin|GCClipMask|GCSubwindowMode)
	/* If the client clip is different or moved OR the subwindowMode has
	 * changed OR the window's clip has changed since the last validation,
	 * we need to recompute the composite clip .
	 */
	if ((changes & CLIP_BITS) != 0 || drawableChanged) {
		miComputeCompositeClip(pGC, pDrawable);
		oneRect = REGION_NUM_RECTS(cfbGetCompositeClip(pGC)) == 1;
		if (oneRect != devPriv->oneRect) {
			new_line = TRUE;
			devPriv->oneRect = oneRect;
		}
	}

	/* A while loop with a switch statement inside?  No thanks.  -DaveM */
	mask = changes;
	if((mask & (GCFunction | GCForeground | GCBackground | GCPlaneMask)) != 0)
		new_rrop = TRUE;
	if((mask & (GCPlaneMask | GCFillStyle | GCFont)) != 0)
		new_text = TRUE;
	if((mask & (GCLineStyle | GCLineWidth | GCFillStyle | GCCapStyle)) != 0)
		new_line = TRUE;
	if((mask & (GCFillStyle | GCTile | GCStipple)) != 0)
		new_fillspans = new_fillarea = TRUE;
	if(new_rotate == FALSE &&
	   (mask & (GCTileStipXOrigin | GCTileStipYOrigin)) != 0)
		new_rotate = TRUE;
	if((mask & GCStipple) != 0) {
		if(pGC->stipple) {
			int width = pGC->stipple->drawable.width;
			PixmapPtr nstipple;

			if ((width <= 32) && !(width & (width - 1))) {
				int depth = pGC->depth;
				nstipple = (depth == 8 ?
					    cfbCopyPixmap(pGC->stipple) :
					    cfb32CopyPixmap(pGC->stipple));
				if (nstipple) {
					if (depth == 8)
						cfbPadPixmap(nstipple);
					else
						cfb32PadPixmap(nstipple);
					(*pGC->pScreen->DestroyPixmap)(pGC->stipple);
					pGC->stipple = nstipple;
				}
			}
		}
	}

	/* If the drawable has changed, check its depth and ensure suitable
	 * entries are in the proc vector.
	 */
	if (drawableChanged)
		new_fillspans = TRUE;	/* deal with FillSpans later */

	if (new_rotate || new_fillspans) {
		Bool new_pix = FALSE;

		xrot = pGC->patOrg.x + pDrawable->x;
		yrot = pGC->patOrg.y + pDrawable->y;
 		if (!CreatorCheckFill (pGC, pDrawable)) {
			switch (pGC->fillStyle) {
			case FillTiled:
				if (!pGC->tileIsPixel)
				{
					int width = pGC->tile.pixmap->drawable.width;

					if (pGC->depth == 8)
						width *= 8;
					else
						width *= 32;

					if ((width <= 32) && !(width & (width - 1))) {
						if (pGC->depth == 8)
							cfbCopyRotatePixmap(pGC->tile.pixmap,
									    &pGC->pRotatedPixmap,
									    xrot, yrot);
						else
							cfb32CopyRotatePixmap(pGC->tile.pixmap,
									      &pGC->pRotatedPixmap,
									      xrot, yrot);
						new_pix = TRUE;
					}
				}
				break;
			case FillStippled:
			case FillOpaqueStippled:
				{
					int width = pGC->stipple->drawable.width;

					if ((width <= 32) && !(width & (width - 1)))
					{
						mfbCopyRotatePixmap(pGC->stipple,
								    &pGC->pRotatedPixmap, xrot, yrot);
						new_pix = TRUE;
					}
				}
				break;
			}
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
				gcPriv->stipple->alu = pGC->alu | FFB_ROP_EDIT_BIT;
			else
				gcPriv->stipple->alu = pGC->alu;
			if (pGC->fillStyle != FillTiled) {
				gcPriv->stipple->fg = pGC->fgPixel;
				gcPriv->stipple->bg = pGC->bgPixel;
			}
		}

		old_rrop = devPriv->rop;
		if (pGC->depth == 8)
			devPriv->rop = cfbReduceRasterOp (pGC->alu, pGC->fgPixel,
							  pGC->planemask,
							  &devPriv->and, &devPriv->xor);
		else
			devPriv->rop = cfb32ReduceRasterOp (pGC->alu, pGC->fgPixel,
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
		GCOps *newops;
		int using_creator_ops = 0;

		if ((newops = CreatorMatchCommon (pGC, devPriv))) {
			if (pGC->ops->devPrivate.val)
				miDestroyGCOps (pGC->ops);
			pGC->ops = newops;
			new_rrop = new_line = new_fillspans = new_text = new_fillarea = 0;
			using_creator_ops = 1;
		} else {
			if (!pGC->ops->devPrivate.val) {
				pGC->ops = miCreateGCOps (pGC->ops);
				pGC->ops->devPrivate.val = 1;
			}

			/* We have to make sure the copyarea op always
			 * points to our special routine as it maintains the
			 * synchronization between the raster processor and direct
			 * access to the frame buffer.
			 */
			pGC->ops->CopyArea = CreatorCopyArea;
		}
		if (pGC->depth == 8)
			newops = cfbMatchCommon(pGC, devPriv);
		else
			newops = cfb32MatchCommon(pGC, devPriv);

		if (newops) {
			gcPriv->PolySegment = newops->PolySegment;
			gcPriv->Polylines = newops->Polylines;

			if (using_creator_ops) {
				/* Fixup line/segment backup ops. */
				if (pGC->ops->PolySegment == CreatorPolySegment)
					gcPriv->PolySegment = CreatorSegmentSSStub;
				if (pGC->ops->Polylines == CreatorPolylines)
					gcPriv->Polylines = CreatorLineSSStub;
			}
		}
	}

	accel = pGC->fillStyle == FillSolid || gcPriv->stipple;

	/* deal with the changes we've collected */
	if (new_line)
		CreatorNewLine(pGC, devPriv, gcPriv, accel);

	if (new_text && pGC->font)
		CreatorNewGlyph(pGC, gcPriv);

	if (new_fillspans)
		CreatorNewFillSpans(pGC, devPriv, gcPriv, accel);

	if (new_fillarea)
		CreatorNewFillArea(pGC, devPriv, gcPriv, accel);
}

GCFuncs	CreatorGCFuncs = {
	CreatorValidateGC,
	miChangeGC,
	miCopyGC,
	CreatorDestroyGC,
	miChangeClip,
	miDestroyClip,
	miCopyClip
};

Bool
CreatorCreateGC (GCPtr pGC)
{
	CreatorPrivGCPtr gcPriv;
	
	if (pGC->depth == 1)
		return mfbCreateGC(pGC);

	if (pGC->depth == 8) {
		if (!cfbCreateGC(pGC))
			return FALSE;
	} else {
		if (!cfb32CreateGC(pGC))
			return FALSE;
	}

	if (pGC->depth == 8)
		pGC->ops = &CreatorNonTEOps8;
	else
		pGC->ops = &CreatorNonTEOps32;

	pGC->funcs = &CreatorGCFuncs;
	gcPriv = CreatorGetGCPrivate(pGC);
	gcPriv->type = DRAWABLE_WINDOW;
	gcPriv->linepat = 0;
	gcPriv->stipple = 0;
	gcPriv->PolySegment = CreatorSegmentSSStub;
	gcPriv->Polylines = CreatorLineSSStub;

	return TRUE;
}
