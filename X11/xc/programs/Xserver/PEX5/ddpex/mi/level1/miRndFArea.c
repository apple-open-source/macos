/* $Xorg: miRndFArea.c,v 1.4 2001/02/09 02:04:09 xorgcvs Exp $ */
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

#define NEED_EVENTS
#include "miRender.h"
#include "misc.h"
#include "miscstruct.h"
#include "PEXErr.h"
#include "extnsionst.h"
#include "gcstruct.h"
#include "ddpex2.h"


/*++
 |
 |  Function Name:	miRenderFillArea
 |
 |  Function Description:
 |	 Renders fill areas to the screen
 |
 |  Note(s):
 |
 --*/

ddpex3rtn
miRenderFillArea(pRend, pddc, input_list, input_facet, shape, noedges)
/* in */
    ddRendererPtr	pRend;		/* renderer handle */
    miDDContext		*pddc;		/* dd context handle */
    miListHeader	*input_list;	/* fill area data */
    listofddFacet	*input_facet;	/* per facet data */
    ddBitmaskShort	shape;		/* shape hint */
    ddUCHAR		noedges;	/*ignore edges flag*/
/* out */
{

/* calls */
      ddpex3rtn		miDDC_to_GC_fill_area();
      ddpex3rtn		miCopyPath();
      ddpex3rtn		miFilterPath();
      ddpex3rtn		miRemoveInvisibleEdges();


/* Local variable definitions */
      ddPointUnion	in_pt;
      ddFacetUnion	out_fct;
      listofddPoint	*pddlist;
      listofddFacet	*fct_list;
      miListHeader	*copy_list;
      miListHeader	*temp_list;
      GCPtr		pGC;
      int		point_size, facet_size;
      int		num_points;
      ddULONG		colourindex;
      ddColourSpecifier	intcolour;
      miColourEntry	*pintcolour;
      int		j, k;
      ddpex3rtn		err = Success;
      ddUSHORT		status = 0;


      /*
       * ddx trashes the input data by adding window offsets.
       * Thus, if the data is going to be re-used for rendering
       * of edges, it must be saved :-(.
       */
      if ((pddc->Static.attrs->edges != PEXOff) && 
	  (!noedges) &&
	  (pddc->Static.attrs->intStyle != PEXInteriorStyleEmpty))
	miCopyPath(pddc, input_list, &copy_list, 0);
      else copy_list = input_list;

      /*
       * Update the fill area GC to reflect the current 
       * fill area attributes 
       */
      pGC = pddc->Static.misc.pFillAreaGC;
      if (pddc->Static.misc.flags & FILLAREAGCFLAG)
	miDDC_to_GC_fill_area(pRend, pddc, pGC);

      /* 
       * Render that path.
       */
      switch (pddc->Static.attrs->intStyle) {

	case PEXInteriorStyleHollow:
	 {
	  /*
	   * The final fill area color is determined by a hierarchy
	   * of sources. The first source is the by vertex colors.
	   * If the data has vertex colors, then the final color
	   * is an average of the vertex colors. If there are no
	   * vertex colors, then the fill area is set to the facet
	   * color for the current facet. IF there are no facet colors,
	   * then the color is determined by the surface color attribute.
	   */

	  if ( (DD_IsVertColour(input_list->type)) && 
		(!MI_DDC_IS_HIGHLIGHT(pddc)) ) {
	    /*
	     * If vertex colors, simply create a facet list.
	     */
	    DDFacetSIZE(DD_FACET_RGBFLOAT, facet_size);
	    fct_list = MI_NEXTTEMPFACETLIST(pddc);
	    fct_list->type = DD_FACET_RGBFLOAT;
	    MI_ALLOCLISTOFDDFACET(fct_list, 1, facet_size);
	    if (!fct_list->facets.pNoFacet) return(BadAlloc);
	    out_fct = fct_list->facets;
	    DD_VertPointSize(input_list->type, point_size);
	    point_size -= sizeof(ddCoord2DS);

	    out_fct.pFacetRgbFloat->red = 0.0;
	    out_fct.pFacetRgbFloat->green = 0.0;
	    out_fct.pFacetRgbFloat->blue = 0.0;
	    num_points = 0;

            for (j = 0, pddlist = input_list->ddList; 
	         j < input_list->numLists; j++, pddlist++) {

		in_pt = pddlist->pts;

		/* Compute average facet color. Note that we exclude */
		/* the last point from the average, since it is the  */
		/* same as the first point */
		for (k = 1; k < pddlist->numPoints; k++) {
		  in_pt.p2DSpt++;	/* skip coord data */
		  out_fct.pFacetRgbFloat->red 
					+= in_pt.pRgbFloatClr->red;
		  out_fct.pFacetRgbFloat->green 
					+= in_pt.pRgbFloatClr->green;
		  out_fct.pFacetRgbFloat->blue 
					+= in_pt.pRgbFloatClr->blue;
		  in_pt.ptr += point_size;	/* skip color and normal */
		  num_points++;
		}
            }

	    /* complete average */
	    if (num_points > 0) {
	      out_fct.pFacetRgbFloat->red /= num_points;
	      out_fct.pFacetRgbFloat->green /= num_points;
	      out_fct.pFacetRgbFloat->blue /= num_points;
	    }
	    /* clamp on saturation */
	    if (out_fct.pFacetRgbFloat->red > 1.0)
		    out_fct.pFacetRgbFloat->red = 1.0;
	    if (out_fct.pFacetRgbFloat->green > 1.0)
		    out_fct.pFacetRgbFloat->green = 1.0;
	    if (out_fct.pFacetRgbFloat->blue > 1.0)
		    out_fct.pFacetRgbFloat->blue = 1.0;

	    /* new facet colors override input ones */
	    fct_list->numFacets = 1;
	    input_facet = fct_list;
	  }

	  /* Remove all data from vertex data but vertex coordinates */
	  if ((DD_IsVertNormal(input_list->type)) ||
	      (DD_IsVertEdge(input_list->type)) ||
	      (DD_IsVertColour(input_list->type)) ) {
	    if (err = miFilterPath(pddc, input_list, &temp_list, 1))
	      return(err);
	    input_list = temp_list;
	  }

	  if ( (input_facet) &&
	       (input_facet->numFacets > 0) &&
	       (DD_IsFacetColour(input_facet->type)) &&
		(!MI_DDC_IS_HIGHLIGHT(pddc)) ) {

	    /* Compute index value for ddx */
	    if (pddc->Static.attrs->echoMode == PEXEcho)
		intcolour = pddc->Static.attrs->echoColour;
	    else {
		intcolour.colourType = PEXRgbFloatColour;
		intcolour.colour.rgbFloat = *input_facet->facets.pFacetRgbFloat;
	    }
	    miColourtoIndex(pRend, pddc->Dynamic->pPCAttr->colourApproxIndex,
			    &intcolour, &colourindex);

	    /* Only set GC value if necessary */
	    if (colourindex != pGC->fgPixel) {
	      pGC->fgPixel = colourindex;
	      /* Register changes with ddx */
	      pGC->serialNumber |= GC_CHANGE_SERIAL_BIT;
	      pGC->stateChanges |= GCForeground;
	      (*pGC->funcs->ChangeGC)(pGC, GCForeground);
	      /* Insure that the GC is reset to proper color next time */
	      pddc->Static.misc.flags |= FILLAREAGCFLAG;
	    }

	    /* validate GC prior to start of rendering */
	    if (pGC->serialNumber != pRend->pDrawable->serialNumber)
	      ValidateGC(pRend->pDrawable, pGC);

	    /* Render each bound as a polyline */
            for (j = 0, pddlist = input_list->ddList; 
	         j < input_list->numLists; j++, pddlist++) {

              if (pddlist->numPoints > 0) {

	        /* Call ddx to render the polygon */
	        (*GetGCValue(pGC, ops->Polylines)) 
			     (pRend->pDrawable,
			     pGC,
			     CoordModeOrigin, 
			     pddlist->numPoints, 
			     pddlist->pts.p2DSpt);
		}
            }
	  }

	  else {

	    /*
	     * If no vertex or facet colors, use surface attributes.
	     * Surface attributes are set to highlight colour 
	     * if highlighting
	     */
	    if (pddc->Static.attrs->echoMode == PEXEcho) 
		intcolour = pddc->Static.attrs->echoColour;
	    else 
		intcolour = pddc->Static.attrs->surfaceColour;

	    miColourtoIndex( pRend,
			     pddc->Dynamic->pPCAttr->colourApproxIndex,
			     &intcolour, &colourindex);

	    /* Only set GC value if necessary */
	    if (colourindex != pGC->fgPixel) {
	       pGC->fgPixel = colourindex;
	       /* Register changes with ddx */
	       pGC->serialNumber |= GC_CHANGE_SERIAL_BIT;
	       pGC->stateChanges |= GCForeground;
	       (*pGC->funcs->ChangeGC)(pGC, GCForeground);
	    }

	    /* validate GC prior to start of rendering */
	    if (pGC->serialNumber != pRend->pDrawable->serialNumber)
	      ValidateGC(pRend->pDrawable, pGC);

	    /* Render each bound as a polyline */
            for (j = 0, pddlist = input_list->ddList; 
	         j < input_list->numLists; j++, pddlist++)
              if (pddlist->numPoints > 0)
	        /* Call ddx to render the polygon */
	        (*GetGCValue(pGC, ops->Polylines)) 
			     (pRend->pDrawable,
			     pGC, 
			     CoordModeOrigin, 
			     pddlist->numPoints, 
			     pddlist->pts.p2DSpt);
	 }
	 break;
	}

	/* Note that patterns and hatching are currently not implemented */
	case PEXInteriorStylePattern:
	case PEXInteriorStyleHatch:
	case PEXInteriorStyleSolid:
	 {
	  /*
	   * The final fill area color is determined by a hierarchy
	   * of sources. The first source is the by vertex colors.
	   * If the data has vertex colors, then the final color
	   * is an average of the vertex colors. If there are no
	   * vertex colors, then the fill area is set to the facet
	   * color for the current facet. IF there are no facet colors,
	   * then the color is determined by the surface color attribute.
	   */

	  if ( (DD_IsVertColour(input_list->type)) && 
		(!MI_DDC_IS_HIGHLIGHT(pddc)) ) {
	    /*
	     * If vertex colors, simply create a facet list.
	     */
	    DDFacetSIZE(DD_FACET_RGBFLOAT, facet_size);
	    fct_list = MI_NEXTTEMPFACETLIST(pddc);
	    fct_list->type = DD_FACET_RGBFLOAT;
	    MI_ALLOCLISTOFDDFACET(fct_list, 1, facet_size);
	    if (!fct_list->facets.pNoFacet) return(BadAlloc);
	    out_fct = fct_list->facets;
	    DD_VertPointSize(input_list->type, point_size);
	    point_size -= sizeof(ddCoord2DS);

	    out_fct.pFacetRgbFloat->red = 0.0;
	    out_fct.pFacetRgbFloat->green = 0.0;
	    out_fct.pFacetRgbFloat->blue = 0.0;
	    num_points = 0;

            for (j = 0, pddlist = input_list->ddList; 
	         j < input_list->numLists; j++, pddlist++) {

		in_pt = pddlist->pts;

		/* Compute average facet color. Note that we exclude */
		/* the last point from the average, since it is the  */
		/* same as the first point */
		for (k = 1; k < pddlist->numPoints; k++) {
		  in_pt.p2DSpt++;	/* skip coord data */
		  out_fct.pFacetRgbFloat->red 
					+= in_pt.pRgbFloatClr->red;
		  out_fct.pFacetRgbFloat->green 
					+= in_pt.pRgbFloatClr->green;
		  out_fct.pFacetRgbFloat->blue 
					+= in_pt.pRgbFloatClr->blue;
		  in_pt.ptr += point_size;	/* skip color and normal */
		  num_points++;
		}
            }

	    /* complete average */
	    if (num_points > 0) {
	      out_fct.pFacetRgbFloat->red /= num_points;
	      out_fct.pFacetRgbFloat->green /= num_points;
	      out_fct.pFacetRgbFloat->blue /= num_points;
	    }
	    /* clamp on saturation */
	    if (out_fct.pFacetRgbFloat->red > 1.0)
		    out_fct.pFacetRgbFloat->red = 1.0;
	    if (out_fct.pFacetRgbFloat->green > 1.0)
		    out_fct.pFacetRgbFloat->green = 1.0;
	    if (out_fct.pFacetRgbFloat->blue > 1.0)
		    out_fct.pFacetRgbFloat->blue = 1.0;

	    /* new facet colors override input ones */
	    fct_list->numFacets = 1;
	    input_facet = fct_list;
	  }

	  /* Remove all data from vertex data but vertex coordinates */
	  if ((DD_IsVertNormal(input_list->type)) ||
	      (DD_IsVertEdge(input_list->type)) ||
	      (DD_IsVertColour(input_list->type)) ) {
	    if (err = miFilterPath(pddc, input_list, &temp_list, 1))
	      return(err);
	    input_list = temp_list;
	  }

	  if ( (input_facet) &&
	       (input_facet->numFacets > 0) &&
	       (DD_IsFacetColour(input_facet->type)) &&
		(!MI_DDC_IS_HIGHLIGHT(pddc)) ) {

	    /* Compute index value for ddx */
	    if (pddc->Static.attrs->echoMode == PEXEcho) 
		intcolour = pddc->Static.attrs->echoColour;
	    else {
		intcolour.colourType = PEXRgbFloatColour;
		intcolour.colour.rgbFloat = *input_facet->facets.pFacetRgbFloat;
	    }
	    miColourtoIndex(pRend, pddc->Dynamic->pPCAttr->colourApproxIndex, 
			    &intcolour, &colourindex);

	    /* Only set GC value if necessary */
	    if (colourindex != pGC->fgPixel) {
	      pGC->fgPixel = colourindex;
	      /* Register changes with ddx */
	      pGC->serialNumber |= GC_CHANGE_SERIAL_BIT;
	      pGC->stateChanges |= GCForeground;
	      (*pGC->funcs->ChangeGC)(pGC, GCForeground);
	      /* Insure that the GC is reset to proper color next time */
	      pddc->Static.misc.flags |= FILLAREAGCFLAG;
	    }

	    /* validate GC prior to start of rendering */
	    if (pGC->serialNumber != pRend->pDrawable->serialNumber)
	      ValidateGC(pRend->pDrawable, pGC);

	    /* Render the polygon.  */
            for (j = 0, pddlist = input_list->ddList; 
	         j < input_list->numLists; j++, pddlist++) {

              if (pddlist->numPoints > 0) {

	        /* Call ddx to render the polygon */
	        (*GetGCValue(pGC, ops->FillPolygon)) 
			     (pRend->pDrawable,
			     pGC, 
			     shape != PEXUnknownShape ? shape : PEXComplex,
			     CoordModeOrigin, 
			     pddlist->numPoints, 
			     pddlist->pts.p2DSpt);
	      }
            }
	  }

	  else {

	    /*
	     * If no vertex or facet colors, use surface attributes.
	     * Surface attributes are set to highlight colour 
	     * if highlighting
	     */
	    if (pddc->Static.attrs->echoMode == PEXEcho)
		intcolour = pddc->Static.attrs->echoColour;
	    else 
		intcolour = pddc->Static.attrs->surfaceColour;

	    miColourtoIndex( pRend,
			     pddc->Dynamic->pPCAttr->colourApproxIndex,
			     &intcolour, &colourindex);

	    /* Only set GC value if necessary */
	    if (colourindex != pGC->fgPixel) {
	       pGC->fgPixel = colourindex;
	       /* Register changes with ddx */
	       pGC->serialNumber |= GC_CHANGE_SERIAL_BIT;
	       pGC->stateChanges |= GCForeground;
	       (*pGC->funcs->ChangeGC)(pGC, GCForeground);
	    }

	    /* validate GC prior to start of rendering */
	    if (pGC->serialNumber != 
	        pRend->pDrawable->serialNumber)
	      ValidateGC(pRend->pDrawable, pGC);

	    /* Render each bound as a polyline */
            for (j = 0, pddlist = input_list->ddList; 
	         j < input_list->numLists; j++, pddlist++)
              if (pddlist->numPoints > 0)
	        /* Call ddx to render the polygon */
	        (*GetGCValue(pGC, ops->FillPolygon)) 
			     (pRend->pDrawable,
			     pGC, 
			     shape != PEXUnknownShape ? shape : PEXComplex,
			     CoordModeOrigin, 
			     pddlist->numPoints, 
			     pddlist->pts.p2DSpt);
	 }
	 break;
	 }

	case PEXInteriorStyleEmpty:
	 break;
      }

      /*
       * Now check to see if fill area edges are to be drawn
       */
      if ((pddc->Static.attrs->edges != PEXOff) && (!noedges)) {

	/* If edge flags, remove invisible edges */
	if (DD_IsVertEdge(copy_list->type))
	  miRemoveInvisibleEdges(pddc, copy_list, &input_list);
	else input_list = copy_list;

	/* Remove all data from vertex data but vertex coordinates */
	if ((DD_IsVertNormal(input_list->type)) ||
	    (DD_IsVertEdge(input_list->type)) ||
	    (DD_IsVertColour(input_list->type)) ) {
	  if (err = miFilterPath(pddc, input_list, &copy_list, 1))
	    return(err);
	  input_list = copy_list;
	}

	/*
	 * Update the fill area GC to reflect the current 
	 * fill area attributes 
	 */
	if (pddc->Static.misc.flags & EDGEGCFLAG)
	  miDDC_to_GC_edges(pRend, pddc, pddc->Static.misc.pEdgeGC);

	/* validate GC prior to start of rendering */
	if (pddc->Static.misc.pEdgeGC->serialNumber != 
	    pRend->pDrawable->serialNumber)
	  ValidateGC(pRend->pDrawable, pddc->Static.misc.pEdgeGC);

	/* Render each bound as a polyline */
        for (j = 0, pddlist = input_list->ddList; 
	     j < input_list->numLists; j++, pddlist++)
           if (pddlist->numPoints > 0)
	     /* Call ddx to render the polygon */
	     (*GetGCValue(pddc->Static.misc.pEdgeGC, ops->Polylines)) 
			  (pRend->pDrawable,
			  pddc->Static.misc.pEdgeGC, 
			  CoordModeOrigin, 
			  pddlist->numPoints, 
			  pddlist->pts.p2DSpt);
    }

    return(Success);
}
