/* $Xorg: miRndMarkr.c,v 1.4 2001/02/09 02:04:09 xorgcvs Exp $ */
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


Copyright 1989, 1990, 1991 by Sun Microsystems, Inc.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of Sun Microsystems,
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

SUN MICROSYSTEMS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT
SHALL SUN MICROSYSTEMS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

*/
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/level1/miRndMarkr.c,v 3.8 2001/12/14 19:57:17 dawes Exp $ */

#define NEED_EVENTS
#include "miRender.h"
#include "Xprotostr.h"
#include "misc.h"
#include "miscstruct.h"
#include "PEXErr.h"
#include "extnsionst.h"
#include "gcstruct.h"
#include "ddpex2.h"
#include "miMarkers.h"
#include "pexos.h"

#define FULL_CIRCLE 360*64


/*++
 |
 |  Function Name:	miRenderMarker
 |
 |  Function Description:
 |	 Renders Polylines to the screen.
 |
 |  Note(s):
 |
 --*/

ddpex3rtn
miRenderMarker(pRend, pddc, input_list)
/* in */
    ddRendererPtr       pRend;          /* renderer handle */
    miDDContext         *pddc;          /* dd context handle */
    miListHeader        *input_list;    /* fill area data */
{
/* calls */
	ddpex3rtn	miTransform();
	ddpex3rtn	miClipPointList();

/* Local variable definitions */
      miListHeader	*temp_list;
      miListHeader	save_list;
      int		i, j, k;
      miListHeader	*input_marker_list, *xformed_marker;
      listofddPoint	*pddlist1, *pddlist2;
      float		marker_trans[4][4];
      ddCoord2DS	*in_pt;
      ddpex3rtn		status;


      /* Remove all data  but vertex coordinates */
      if ((DD_IsVertNormal(input_list->type)) ||
	  (DD_IsVertEdge(input_list->type)) ||
	  (DD_IsVertColour(input_list->type)) ) {
	status = miFilterPath(pddc, input_list, &temp_list, 0);
	if (status) return(status);
	input_list = temp_list;
      }

    /*
     * Update the marker GC to reflect the current 
     * marker attributes 
     */
    if (pddc->Static.misc.flags & MARKERGCFLAG)
      miDDC_to_GC_marker(pRend, pddc, pddc->Static.misc.pPolyMarkerGC);

    /*
     * Now render the appropriate marker
     */
    switch (pddc->Static.attrs->markerType) {

	case PEXMarkerDot:
	   /* validate GC prior to start of rendering */
	   if (pddc->Static.misc.pPolyMarkerGC->serialNumber != 
	       pRend->pDrawable->serialNumber)
      	     ValidateGC(pRend->pDrawable, pddc->Static.misc.pPolyMarkerGC);

	   /* 
	    * Render a pixel at each marker position 
	    */
	   for (j = 0, pddlist1 = input_list->ddList; 
		j < input_list->numLists; j++, pddlist1++)
	      if (pddlist1->numPoints > 0)
		/* Call ddx to render the polylines */
		(*GetGCValue(pddc->Static.misc.pPolyMarkerGC, ops->PolyPoint)) 
			     (pRend->pDrawable,
			      pddc->Static.misc.pPolyMarkerGC, 
			      CoordModeOrigin, 
			      pddlist1->numPoints, 
			      pddlist1->pts.p2DSpt);
	   break;

	case PEXMarkerCross:
	   /* new path contains the default marker description to render */
	   input_marker_list = &cross_header;
	   goto render_marker;

	case PEXMarkerAsterisk:
	   /* new path contains the default marker description to render */
	   input_marker_list = &asterisk_header;
	   goto render_marker;

	case PEXMarkerCircle:
	   {
	    xArc        *parcs=0,	/* ddx polyarc input structure */
			*pcurarcs;

	    /* 
	     * Draw a circle for each position in the PolyMarker list.
	     */
	    for (i = 0, pddlist1 = input_list->ddList; 
		 i < input_list->numLists; i++, pddlist1++) {
	      /*
	       * Ensure enough arc structures
	       */
	      if (parcs)
	        pcurarcs = parcs = 
			(xArc *)xrealloc(parcs,sizeof(xArc)*pddlist1->numPoints);
	      else pcurarcs = parcs = 
			(xArc *)xalloc(sizeof(xArc)*pddlist1->numPoints);

	      in_pt = pddlist1->pts.p2DSpt;

	      /* Create an arc structure for every PolyMarker point */
	      for (j = 0; j < pddlist1->numPoints; j++) {
		pcurarcs->x = in_pt->x 
				- (ddUSHORT)pddc->Static.attrs->markerScale;
		pcurarcs->y = (in_pt++)->y
				- (ddUSHORT)pddc->Static.attrs->markerScale;
		pcurarcs->width = (ddUSHORT)(pddc->Static.attrs->markerScale*2);
		pcurarcs->height = pcurarcs->width;
		pcurarcs->angle1 = 0;
		(pcurarcs++)->angle2 = FULL_CIRCLE;
	      }

	      /* validate GC prior to start of rendering */
	      if (pddc->Static.misc.pPolyMarkerGC->serialNumber != 
		  pRend->pDrawable->serialNumber)
		ValidateGC(pRend->pDrawable, pddc->Static.misc.pPolyMarkerGC);

	      /* Call ddx to render a circle */
	      (*GetGCValue(pddc->Static.misc.pPolyMarkerGC, ops->PolyArc)) 
			   (pRend->pDrawable,
			    pddc->Static.misc.pPolyMarkerGC, 
			    pddlist1->numPoints, 
			    parcs);
	    }

	    /* free temporary resources */
	    if (parcs) xfree(parcs);

	   }
	   break;

	case PEXMarkerX:
	   /* new path contains the default marker description to render */
	   input_marker_list = &X_header;

render_marker:
	   /*
	    * marker_trans contains the transformation to transform
	    * the unit marker default specification to the final
	    * screen size/position. The scale factor used in x and y 
	    * is the PC makerScale, while the translation is provided
	    * by the (now DC) marker position specified in the input
	    * vertex list.
	    */
	   memcpy( (char *) marker_trans, 
		 (char *) ident4x4, 16 * sizeof(ddFLOAT));
	   marker_trans[0][0] = pddc->Static.attrs->markerScale;
	   marker_trans[1][1] = pddc->Static.attrs->markerScale;

	   /*
	    * the transform routine automatically" selects the
	    * output data area. In order to not overwrite the
	    * polymarker data, the list header is copied to
	    * a temporary area. Note that the maxLists field
	    * associated with the old list is zeroed so that 
	    * new data will be alloc'ed on the next transform.
	    */
	   save_list = *input_list;
	   input_list->maxLists = 0;

	   /* 
	    * Draw a marker for each position in the PolyMarker list.
	    */
	   for (i = 0, pddlist1 = save_list.ddList; 
		i < save_list.numLists; i++, pddlist1++) {

	      in_pt = pddlist1->pts.p2DSpt;

	      /* for every PolyMarker point */
	      for (j = 0; j < pddlist1->numPoints; j++) {

		/* Transform marker description into screen coords */
		marker_trans[0][3] = (float)in_pt->x;
		marker_trans[1][3] = (float)(in_pt++)->y;
		if (status = miTransform(pddc, 
				       input_marker_list, &xformed_marker, 
				       marker_trans,
				       NULL4x4,
				       DD_2DS_POINT))
		  return (status);

		/* validate GC prior to start of rendering */
	        if (pddc->Static.misc.pPolyMarkerGC->serialNumber != 
		    pRend->pDrawable->serialNumber)
		  ValidateGC(pRend->pDrawable, pddc->Static.misc.pPolyMarkerGC);

		/* We should have DC paths here; Render them */
		for (k = 0, pddlist2 = xformed_marker->ddList; 
		     k < xformed_marker->numLists; k++, pddlist2++)
		  if (pddlist2->numPoints > 0)
		   /* Call ddx to render the polylines */
		   (*GetGCValue(pddc->Static.misc.pPolyMarkerGC,ops->Polylines))
				(pRend->pDrawable,
				 pddc->Static.misc.pPolyMarkerGC, 
				 CoordModeOrigin, 
				 pddlist2->numPoints, 
				 pddlist2->pts.p2DSpt);
	      }
	   }
	   MI_FREELISTHEADER(&save_list);
	   break;

	default:
	   break;
    }

    return (Success);
}
