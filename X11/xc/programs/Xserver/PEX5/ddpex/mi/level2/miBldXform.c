/* $Xorg: miBldXform.c,v 1.4 2001/02/09 02:04:09 xorgcvs Exp $ */
/*

Copyright 1989, 1990, 1991, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.


Copyright 1989, 1990, 1991 by Sun Microsystems, Inc

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name Sun Microsystems not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

SUN MICROSYSTEMS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT
SHALL SUN MICROSYSTEMS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

*/
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/level2/miBldXform.c,v 3.7 2001/12/14 19:57:20 dawes Exp $ */

#include "X.h"
#include "Xproto.h"
#include "misc.h"
#include "miLUT.h"
#include "PEXErr.h"
#include "pixmap.h"
#include "windowstr.h"
#include "gcstruct.h"
#include "scrnintstr.h"
#include "regionstr.h"
#include "miscstruct.h"
#include "miRender.h"
#include "pexos.h"

/* External variables and functions */
extern	void		miMatMult();
extern  void		miMatCopy();
extern  ddpex3rtn	InquireLUTEntryAddress();
extern  void		SetViewportClip();


/*++
 |
 |  Function Name:	miBldViewport_xform
 |
 |  Function Description:
 |	 Computes the viewport_xform with special handling for the ddc.
 |       This is used for setting up for drawing, picking, searching,
 |       and MapDCtoWC/MapWCtoDC
 |
 |  Note(s):
 |
 --*/

ddpex3rtn
miBldViewport_xform(pRend, pDrawable, xform, pddc )
    ddRendererPtr       pRend;	   /* renderer handle */
    DrawablePtr		pDrawable; /* pointer to X drawable */
    ddFLOAT		xform[4][4]; /* returned transform */
    miDDContext		*pddc;	   /* dd context handle */
{

    ddFLOAT	Sx, Sy, Sz;
    ddFLOAT	Sxy;
    ddFLOAT	Tx, Ty, Tz;

    /*
     * Determine the npc -> dc viewport transform. Let's represent
     * this transform as follows :
     *
     *            Sx     0      0      0
     *            0      Sy     0      0
     *            0      0      Sz     0
     *            Tx     Ty     Tz     1
     *
     * There are two components to this transformation.
     *
     * NOTE THAT THE TRANSLATION COMPONENTS DETERMINED IN THE BeginRenderi
ng
     * CODE ARE DIFFERENT FOR THE PICKING CASE. WE USE THE PHIGS DC SPACE
     * TO REPRESENT THE PICK APERTURE AS OPPOSED TO THE X'S SCREEN SPACE.
     *
     *
     * The elements Sx, Sy, Tx, and Ty are computed in two stages as
     * follows :
     *
     * First, the npc subvolume in the renderer determines
     * the subvolume that is projected onto the viewport.
     * This tranformation is implemented by "magnifying" this
     * subvolume to the range (0, 0, 0)-(1,1,1) 
     *
     *         NPC space                "magnified" space
     *
     *          +-----+(c,d)             +--------+(1,1)
     *          |     |                  |        |
     *          |     |      <====>      |        |
     *          |     |                  |        |
     *     (a,b)+-----+             (0,0)+--------+
     *
     *                    1/(c-a)       0      0
     *                      0        1/(d-b)   0
     *                   -a/(c-a)   -b/(d-b)   0
     *
     */
    /*
     * Initialize the scaling components Sx, Sy, and Sz.
     *
     */

    Sx = 1/(pRend->npcSubvolume.maxval.x - pRend->npcSubvolume.minval.x);
    Sy = 1/(pRend->npcSubvolume.maxval.y - pRend->npcSubvolume.minval.y);
    Sz = 1/(pRend->npcSubvolume.maxval.z - pRend->npcSubvolume.minval.z);

    /*
     * Secondly, if the useDrawable flag in the viewport is set, then 
     * get the min and max of the drawable from the pDrawable passed 
     * and store it in the viewport as the default values. Otherwise, 
     * the values are as available in the viewport slot of renderer
     * context already set up. Note that the same aspect ratio limitation
     * used above also applies to the viewport transform.
     */
    memcpy( (char *) xform, (char *) ident4x4, 16 * sizeof(ddFLOAT));

    if (pRend->viewport.useDrawable) { /* Use the drawable width and height */
      /* FOR RENDERING:
       * Note that The origin is always 0,0 since ddx points
       * are specified relative to the window origin
       * Lastly, note that the screen origin is at the upper left
       * corner of the window, the viewing space origin is at the
       * lower left corner. To "flip" the coordinates to match
       * the screen space, apply the following transform:
       *
       *            1	   0	    0	0
       *            0	  -1 	    0	0
       *            0	   0        1	0
       *            0 window_y_max  0	0
       *
       *
       *    ie y' = (window_y_max - y)
       *
       */
      /* FOR PICKING:
       * Note that The origin is always 0,0 since DC points
       * are specified relative to the window origin. The
       * required transform is :
       *
       *            Sx    0     0      0
       *            0    Sy     0      0
       *            0     0     1      0
       *            0     0     0      0
       *
       */
      /*
       * Modify the scaling components Sx and Sy to include the second 
       * transform as follows :
       */

      Sx *= (ddFLOAT)pDrawable->width;
      Sy *= (ddFLOAT)pDrawable->height;

      /* 
       * Initialize the translation components Tx, Ty and Tz
       * Note the difference for picking and searching from
       * drawing. drawing flips the y value. the others don't
       */

      Tx = 0.0;
      if ((pRend->render_mode == MI_REND_PICKING) || (!pddc))
          Ty = 0.0;
      else if ((pRend->render_mode == MI_REND_DRAWING) && (pddc))
          Ty = pDrawable->height;
      Tz = 0.0;

    } else {

      xRectangle viewport;

      /*  FOR RENDERING:
       * Limit the rendering for each of the GC's to the viewport 
       * Note that The origin is always 0,0 since ddx points
       * are specified relative to the window origin
       * Lastly, note that the screen origin is at the upper left
       * corner of the window, the viewing space origin is at the
       * lower left corner. Furthermore, primitives must also be
       * translated from this lower left origin to the viewport origin. 
       * To "flip" the coordinates to match the screen space, apply 
       * the following transform:
       *
       *            1		 	   0			  0    0
       *            0			  -1			  0    0
       *            0		 	   0			  1    0
       *   viewport_origin_x  (window_y_max - viewport_origin_y)  0    0
       *
       *
       *    ie y' = (window_y_max - viewport_origin_y - y)
       *
       * Lastly, note that the origin of the clipping rectangle for
       * ddx is relative to the upper left corner of the rectangle,
       * thus this corner must still be further offset by the viewport
       * height.
       *
       */
      /* FOR PICKING:
       * Limit the picking for each of the primitives to the viewport
       * Note that The origin is always 0,0 since DC points
       * are specified relative to the window origin. The required
       * transform is :
       *
       *            Sx    0     0      0
       *            0    Sy     0      0
       *            0     0     1      0
       *            Tx   Ty     0      0
       *
       */
      /* FOR SEARCHING: (this is called when initializing the renderer)
       * there is no drawable, so work around that.  None of what this
       * procedure does is needed for searching, so there may be a better
       * way to deal with this. 
       */
      viewport.width =  pRend->viewport.maxval.x - pRend->viewport.minval.x;
      viewport.height = pRend->viewport.maxval.y - pRend->viewport.minval.y;

      viewport.x = pRend->viewport.minval.x; 
      if (pDrawable)
          viewport.y = pDrawable->height - 
                   pRend->viewport.minval.y - 
                   viewport.height;
      else
	  viewport.y = 0;

      /*
       * Modify the scaling components Sx and Sy to include the second 
       * transform as follows :
       */

      Sx *= (ddFLOAT)viewport.width;
      Sy *= (ddFLOAT)viewport.height;

      /* 
       * Initialize the translation components Tx, Ty and Tz.
       */

      Tx = (ddFLOAT)pRend->viewport.minval.x;
       
      if ((pRend->render_mode == MI_REND_PICKING) || (!pddc))
          Ty = (ddFLOAT)pRend->viewport.minval.y;
      else if ((pRend->render_mode == MI_REND_DRAWING) && (pddc))
	  if (pDrawable)
              Ty = (ddFLOAT)(pDrawable->height - pRend->viewport.minval.y);
	  else Ty = 0.0;
      Tz = 0.0;

      if ((pRend->render_mode == MI_REND_DRAWING) && pDrawable && pddc) {
	  /* Set the GC clip mask to clip to the viewport */

	  ddLONG numrects = pRend->clipList->numObj;

	  if (numrects > 0) {
	      xRectangle      *xrects, *p;
	      ddDeviceRect    *ddrects;
	      RegionPtr	      clipRegion;
	      RegionRec	      tempRegion;
	      GCPtr	      pGC;
	      BoxRec	      box;
	      int             i;

	      ddrects = (ddDeviceRect *) pRend->clipList->pList;
	      xrects = (xRectangle*) xalloc (numrects * sizeof(xRectangle));
	      if (!xrects) return BadAlloc;
	      /* Need to convert to XRectangle format */
	      for (i = 0, p = xrects; i < numrects; i++, p++, ddrects++) {
		  p->x = ddrects->xmin;
		  p->y = pRend->pDrawable->height - ddrects->ymax;
		  p->width = ddrects->xmax - ddrects->xmin + 1;
		  p->height = ddrects->ymax - ddrects->ymin + 1;
	      }

	      /*
	       * Compute the intersection of the viewport and the GC's
               * clip region.  Note that there is a GC for each of the
               * primitive types, and we only have to compute the
	       * intersected region for one of them.  This computed region
	       * will be good for all of the GC's.
	       */

	      pGC = pddc->Static.misc.pPolylineGC;

	      clipRegion = RECTS_TO_REGION(pGC->pScreen, numrects,
	          xrects, Unsorted);

	      xfree((char *) xrects);

	      box.x1 = viewport.x;
	      box.y1 = viewport.y;
	      box.x2 = viewport.x + viewport.width;
	      box.y2 = viewport.y + viewport.height;

	      REGION_INIT(pGC->pScreen, &tempRegion, &box, 1);
	      REGION_INTERSECT(pGC->pScreen, clipRegion, clipRegion, &tempRegion);
	      REGION_UNINIT(pGC->pScreen, &tempRegion);


	      /*
	       * Now set the GC clip regions.
	       */

	      SetViewportClip (pddc->Static.misc.pPolylineGC, clipRegion);
	      SetViewportClip (pddc->Static.misc.pFillAreaGC, clipRegion);
	      SetViewportClip (pddc->Static.misc.pEdgeGC, clipRegion);
	      SetViewportClip (pddc->Static.misc.pPolyMarkerGC, clipRegion);
	      SetViewportClip (pddc->Static.misc.pTextGC, clipRegion);

	      REGION_DESTROY(pGC->pScreen, clipRegion);
	  }
	  else {
	      SetClipRects(pddc->Static.misc.pPolylineGC,
		0, 0, 1, &viewport,YXBanded);
	      SetClipRects(pddc->Static.misc.pFillAreaGC,
		0, 0, 1, &viewport,YXBanded);
	      SetClipRects(pddc->Static.misc.pEdgeGC,
		0, 0, 1, &viewport,YXBanded);
	      SetClipRects(pddc->Static.misc.pPolyMarkerGC,
		0, 0, 1, &viewport,YXBanded);
	      SetClipRects(pddc->Static.misc.pTextGC,
		0, 0, 1, &viewport,YXBanded);
	  }
      }
    }

    /* 
     * Note that Phigs requires that aspect ratio be maintained.
     * Therefore, choose the minimum X or y ratio to represent the
     * x and y view port scaling factor. Note that Z is exempt
     * from this aspect ratio restriction.
     */
    Sxy = (Sx < Sy) ? Sx : Sy;

    /*
     * Finally, set the viewport transform components using Sxy, Sz,
     * and the initial Tx, Ty, Tz modified suitably.
     */

    xform[0][0]= Sxy;
    /* Negate the value to "flip" the screen (see above), IF WE ARE */
    /* RENDERING. On the other hand, since we are PICKING, we need  */
    /* the PHIGS DC, i.e., the lower left is the origin and NO FLIP */
    /* of y is required.                                            */
    /* In case pddc is NULL, this routine is being called from Map- */
    /* DcWc and the situation is similar to PICKING. i.e., No y flip*/
    /* is required.                                                 */
    if ((pRend->render_mode == MI_REND_PICKING) || (!pddc))
        xform[1][1]= Sxy;
    else if ((pRend->render_mode == MI_REND_DRAWING) && (pddc))
        xform[1][1]= -Sxy;
    xform[2][2]= Sz;

    xform[0][3] = Tx - (pRend->npcSubvolume.minval.x * Sxy);

    /* Note, the "+" compensates for negating the [1][1] term above */
    if ((pRend->render_mode == MI_REND_PICKING) || (!pddc))
        xform[1][3]= Ty - (pRend->npcSubvolume.minval.y * Sxy);
    else if ((pRend->render_mode == MI_REND_DRAWING) && (pddc))
        xform[1][3]= Ty + (pRend->npcSubvolume.minval.y * Sxy);

    xform[2][3]= Tz - (pRend->npcSubvolume.minval.z * Sz);

    /* Mark as invalid appropriate inverse transforms in dd context */
    if (pddc)
        pddc->Static.misc.flags |= ( INVTRCCTODCXFRMFLAG );

}

/*++
 |
 |  Function Name:	miBldCC_xform
 |
 |  Function Description:
 |	 Computes the transformation to go from NPC to the PEX-SI's 
 |       clipping space.
 |
 |  Note(s):
 |
 --*/

ddpex3rtn
miBldCC_xform(pRend, pddc)
    ddRendererPtr	pRend;	  /* renderer handle */
    miDDContext		*pddc;	  /* dd context handle */

{

    ddNpcSubvolume	*cliplimits;
    miViewEntry		*viewbundle;
    ddFLOAT		cc_to_npc[4][4];
    ddUSHORT    	status;

    /* 
     * Check if a view LUT is available and accordingly, get the view 
     * matrices either from the view LUT or from the defaults.
     */

    if (!(pRend->lut[PEXViewLUT])) {   /* Use defaults */

	 memcpy( (char *) pddc->Dynamic->npc_to_cc_xform, 
		(char *) ident4x4, 
		16 * sizeof(ddFLOAT));
	 pddc->Dynamic->npc_to_cc_xform[0][0] =  2.0;
	 pddc->Dynamic->npc_to_cc_xform[1][1] =  2.0;
	 pddc->Dynamic->npc_to_cc_xform[2][2] =  2.0;
	 pddc->Dynamic->npc_to_cc_xform[0][3] = -1.0;
	 pddc->Dynamic->npc_to_cc_xform[1][3] = -1.0;
	 pddc->Dynamic->npc_to_cc_xform[2][3] = -1.0;

	 memcpy( (char *) pddc->Dynamic->wc_to_npc_xform,
		(char *) ident4x4,
		16*(sizeof(ddFLOAT)));

	 memcpy( (char *) cc_to_npc,
		(char *) ident4x4, 
		16 * sizeof(ddFLOAT));
	 cc_to_npc[0][0] = 0.5;
	 cc_to_npc[1][1] = 0.5;
	 cc_to_npc[2][2] = 0.5;
	 cc_to_npc[0][3] = 0.5;
	 cc_to_npc[1][3] = 0.5;
	 cc_to_npc[2][3] = 0.5;

/* THIS SHOULD GO AWAY EVENTUALLY... */
pddc->Dynamic->clipFlags = 0;

    } else {

      /* Get the view table entry at current view index first */

      if ((InquireLUTEntryAddress (PEXViewLUT, pRend->lut[PEXViewLUT], 
				   pddc->Dynamic->pPCAttr->viewIndex, 
				   &status, (ddPointer *)(&viewbundle)))
	  == PEXLookupTableError)
	return (PEXLookupTableError);

      miMatCopy(viewbundle->vom, pddc->Dynamic->wc_to_npc_xform);

/* THIS SHOULD GO AWAY EVENTUALLY... */
pddc->Dynamic->clipFlags = viewbundle->entry.clipFlags;

      if (viewbundle->entry.clipFlags)
        {
	/* Use the clip limits as set up in the current view */
 
	 cliplimits  = &viewbundle->entry.clipLimits;
	 /* 
	  * Compute the npc_to_cc & cc_to_npc 
	  * transformations.
	  * The Pex view clipper can only clip against a cube with
	  * corners (-1,-1,-1) (1,1,1).
	  * It is therefore necessary to append an additional 
	  * transformation to the wc_to_cc_xform transform such 
	  * that the view clip volume is transformed to this cube, 
	  * rather than leave the space untouched and clip against actual
	  * clip planes specified in the view lut.
	  * Note that this transformation must be undone by the
	  * subsequent cc_to_dc_xform, so the inverse of this
	  * matrix is pre-pended to this transform.
	  *
	  * two matrices then (note these extend trivially into 3D):
	  *
	  *       NPC subvolume              SI clipping space
	  *
	  *          +-----+(c,d)               +--------+(1,1)
	  *          |     |                    |        |
	  *          |     |      <====>        |        |
	  *          |     |                    |        |
	  *     (a,b)+-----+             (-1,-1)+--------+
	  *
	  *   npc_to_clip:     2/(c-a)       0      0
	  *                      0        2/(d-b)   0
	  *                 (c+a)/(a-c) (d+b)/b-d)  0
	  *
	  *   clip_to_npc:   (c-a)/2         0      0
	  *                     0         (d-b)/2   0
	  *                  (c+a)/2      (d+b)/2   0
	  *
	  * Last, note that the cliplimits from the view table entry are only
	  * used if enabled by the clip flags.
	  */
	 memcpy( (char *) pddc->Dynamic->npc_to_cc_xform,
		(char *) ident4x4, 16 * sizeof(ddFLOAT));

	 memcpy( (char *) cc_to_npc, 
		(char *) ident4x4, 16 * sizeof(ddFLOAT));

	 /* set up X & Y clip limits */

	 if (viewbundle->entry.clipFlags & PEXClipXY) {
	   pddc->Dynamic->npc_to_cc_xform[0][0] = 
			2.0/(cliplimits->maxval.x - cliplimits->minval.x);
	   pddc->Dynamic->npc_to_cc_xform[1][1] = 
			2.0/(cliplimits->maxval.y - cliplimits->minval.y);
	   pddc->Dynamic->npc_to_cc_xform[0][3] = 
		(cliplimits->maxval.x + cliplimits->minval.x) / 
		(cliplimits->minval.x - cliplimits->maxval.x);
	   pddc->Dynamic->npc_to_cc_xform[1][3] = 
		(cliplimits->maxval.y + cliplimits->minval.y) / 
		(cliplimits->minval.y - cliplimits->maxval.y);

	   cc_to_npc[0][0] = 
			(cliplimits->maxval.x - cliplimits->minval.x)/2.0;
	   cc_to_npc[1][1] = 
			(cliplimits->maxval.y - cliplimits->minval.y)/2.0;
	   cc_to_npc[0][3] = 
			(cliplimits->maxval.x + cliplimits->minval.x)/2.0;
	   cc_to_npc[1][3] =
			(cliplimits->maxval.y + cliplimits->minval.y)/2.0;
	 } else {

	   /* Use default NPC clip limits (0.0 <-> 1.0) */

	   pddc->Dynamic->npc_to_cc_xform[0][0] = 2.0/(1.0 - 0.0);
	   pddc->Dynamic->npc_to_cc_xform[1][1] = 2.0/(1.0 - 0.0);
	   pddc->Dynamic->npc_to_cc_xform[0][3] = (1.0 + 0.0)/(0.0 - 1.0);
	   pddc->Dynamic->npc_to_cc_xform[1][3] = (1.0 + 0.0)/(0.0 - 1.0);

	   cc_to_npc[0][0] = (1.0 - 0.0)/2.0;
	   cc_to_npc[1][1] = (1.0 - 0.0)/2.0;
	   cc_to_npc[0][3] = (1.0 + 0.0)/2.0;
	   cc_to_npc[1][3] = (1.0 + 0.0)/2.0;
	 }

	 /* Now, front & back clip limits */

	 if ( (viewbundle->entry.clipFlags & PEXClipFront) &&
	      (viewbundle->entry.clipFlags & PEXClipBack) ) {

	    /* Both front and back clipping on */

	   pddc->Dynamic->npc_to_cc_xform[2][2] = 
			2.0/(cliplimits->maxval.z - cliplimits->minval.z);
	   pddc->Dynamic->npc_to_cc_xform[2][3] = 
		(cliplimits->maxval.z + cliplimits->minval.z) / 
		(cliplimits->minval.z - cliplimits->maxval.z);

	   cc_to_npc[2][2] = 
			(cliplimits->maxval.z - cliplimits->minval.z)/2.0;
	   cc_to_npc[2][3] =
			(cliplimits->maxval.z + cliplimits->minval.z)/2.0;

	 } else if (viewbundle->entry.clipFlags & PEXClipFront) {

	   /* Only front clipping ON; Use default NPC Back value */

	   pddc->Dynamic->npc_to_cc_xform[2][2] = 
			2.0/(1.0 - cliplimits->minval.z);
	   pddc->Dynamic->npc_to_cc_xform[2][3] = 
		(1.0 + cliplimits->minval.z) / 
		(cliplimits->minval.z - 1.0);

	   cc_to_npc[2][2] = 
			(1.0 - cliplimits->minval.z)/2.0;
	   cc_to_npc[2][3] =
			(1.0 + cliplimits->minval.z)/2.0;

	 } else if (viewbundle->entry.clipFlags & PEXClipBack) {

	   /* Only back clipping ON; Use default NPC Front value */

	   pddc->Dynamic->npc_to_cc_xform[2][2] = 2.0/(cliplimits->maxval.z);
	   pddc->Dynamic->npc_to_cc_xform[2][3] = -1.0;

	   cc_to_npc[2][2] = (cliplimits->maxval.z)/2.0;
	   cc_to_npc[2][3] = (cliplimits->maxval.z)/2.0;

	 } else {

	   /* Both front and back clipping OFF; Use default NPC space */

	   pddc->Dynamic->npc_to_cc_xform[2][2] = 2.0/(1.0 - 0.0);
	   pddc->Dynamic->npc_to_cc_xform[2][3] = (1.0 + 0.0) / (0.0 - 1.0);

	   cc_to_npc[2][2] = (1.0 - 0.0)/2.0;
	   cc_to_npc[2][3] = (1.0 + 0.0)/2.0;
	 }

        }
      else  /* All clipping OFF; Use the entire NPC space, i.e., (0,0,0) */
	    /* to (1,1,1)                                                */
        {

	 memcpy( (char *) pddc->Dynamic->npc_to_cc_xform, 
		(char *) ident4x4, 16 * sizeof(ddFLOAT));
	 pddc->Dynamic->npc_to_cc_xform[0][0] =  2.0;
	 pddc->Dynamic->npc_to_cc_xform[1][1] =  2.0;
	 pddc->Dynamic->npc_to_cc_xform[2][2] =  2.0;
	 pddc->Dynamic->npc_to_cc_xform[0][3] = -1.0;
	 pddc->Dynamic->npc_to_cc_xform[1][3] = -1.0;
	 pddc->Dynamic->npc_to_cc_xform[2][3] = -1.0;

	 memcpy( (char *) cc_to_npc,
		(char *) ident4x4, 16 * sizeof(ddFLOAT));
	 cc_to_npc[0][0] = 0.5;
	 cc_to_npc[1][1] = 0.5;
	 cc_to_npc[2][2] = 0.5;
	 cc_to_npc[0][3] = 0.5;
	 cc_to_npc[1][3] = 0.5;
	 cc_to_npc[2][3] = 0.5;

        }
    }

    /*
     * Compute the various composites stored in the
     * dd context.
     */
    /*
     * Compute the composite cc -> dc, i.e., Clip to Device.
     */
    miMatMult ( pddc->Dynamic->cc_to_dc_xform,
		cc_to_npc, 
		pddc->Static.misc.viewport_xform);

    /*
     * Compute the composite wc -> cc, i.e., World to Clip.
     */
    miMatMult ( pddc->Dynamic->wc_to_cc_xform,
		pddc->Dynamic->wc_to_npc_xform, 
		pddc->Dynamic->npc_to_cc_xform);
    /*
     * Compute the composite mc -> wc, i.e. Modelling to World
     */
    miMatMult(  pddc->Dynamic->mc_to_wc_xform,
                pddc->Dynamic->pPCAttr->localMat,
                pddc->Dynamic->pPCAttr->globalMat);

    /*
     * Compute the composite mc -> npc, i.em Modelling to NPC
     */
    miMatMult ( pddc->Dynamic->mc_to_npc_xform,
		pddc->Dynamic->mc_to_wc_xform, 
		pddc->Dynamic->wc_to_npc_xform);

    /* 
     * Compute the composite [VCM], i.e, Modelling to Clip.
     */
    miMatMult ( pddc->Dynamic->mc_to_cc_xform,
		pddc->Dynamic->mc_to_wc_xform, 
		pddc->Dynamic->wc_to_cc_xform);
    /* 
     * Compute the composite mc -> dc transform, i.e., Modelling to Device.
     */
    miMatMult ( pddc->Dynamic->mc_to_dc_xform,
		pddc->Dynamic->mc_to_cc_xform, 
		pddc->Dynamic->cc_to_dc_xform);

    /* Mark as invalid appropriate inverse transforms in dd context */
    pddc->Static.misc.flags |= (INVTRWCTOCCXFRMFLAG | INVTRMCTOCCXFRMFLAG | 
				INVVIEWXFRMFLAG | CC_DCUEVERSION);

}


void SetViewportClip(pGC, clipRegion)
    GCPtr pGC;
    RegionPtr clipRegion;
{
    /*
     * When we call the GC's ChangeClip function, it destroys
     * the region we pass to it.  Since this function is called
     * for each of the GC's, we copy clipRegion into tempRegion
     * and use tempRegion when calling ChangeClip.
     */

    RegionPtr tempRegion = REGION_CREATE(pGC->pScreen, NullBox, 0);

    REGION_COPY(pGC->pScreen, tempRegion, clipRegion);

    pGC->serialNumber |= GC_CHANGE_SERIAL_BIT;
    pGC->clipOrg.x = 0;
    pGC->clipOrg.y = 0;
    pGC->stateChanges |= GCClipXOrigin|GCClipYOrigin;
    (*pGC->funcs->ChangeClip)(pGC, CT_REGION, tempRegion, 0);
    if (pGC->funcs->ChangeGC)
	(*pGC->funcs->ChangeGC) (pGC, GCClipXOrigin|GCClipYOrigin|GCClipMask);
}
