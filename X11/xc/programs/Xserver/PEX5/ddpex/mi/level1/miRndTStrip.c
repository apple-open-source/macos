/* $Xorg: miRndTStrip.c,v 1.4 2001/02/09 02:04:09 xorgcvs Exp $ */
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
#include "miClip.h"
#include "misc.h"
#include "miscstruct.h"
#include "PEXErr.h"
#include "extnsionst.h"
#include "gcstruct.h"
#include "ddpex2.h"


/*++
 |
 |  Function Name:	miRenderTriStrip
 |
 |  Function Description:
 |	 Renders a triangle strip to a drawable
 |
 |  Note(s):
 |
 --*/

ddpex3rtn
miRenderTriStrip(pRend, pddc, input_list, input_facet)
/* in */
    ddRendererPtr       pRend;          /* renderer handle */
    miDDContext         *pddc;          /* dd context handle */
    miListHeader        *input_list;    /* triangle strip vertex data */
    listofddFacet	*input_facet;	/* per facet data */
{


/* calls */
      ddpex3rtn         miDDC_to_GC_fill_area();
      ddpex3rtn         miCopyPath();
      ddpex3rtn         miFilterPath();
      ddpex3rtn         miCloseFillArea();
      ddpex3rtn         miRemoveInvisibleEdges();


/* Local variable definitions */
      ddPointUnion      in_pt, tmp_pt;
      ddFacetUnion      out_fct;
      listofddPoint     *pddlist;
      listofddFacet     *fct_list;
      miListHeader      *edge_copy;
      miListHeader      *temp_list;
      GCPtr             pGC;
      int               point_size, facet_size, num_facets,
			color_offset, edge_offset;
      ddULONG           colourindex;
      ddColourSpecifier intcolour;
      miColourEntry     *pintcolour;
      ddCoord2DS        output_array[4];
      int               i, j, k;
      ddpex3rtn         err = Success;
      ddUSHORT		status = 0;

    /* remember that ALL vertex types are of the form:
     *
     *   |---------------------------|---------|----------|---------|
     *             coords               color     normal      edge
     *                                  (opt)     (opt)      (opt)
     */
 


    if (input_list->numLists == 0) return(1);

    else {
      /*
       * ddx trashes the input data by adding window offsets.
       * Thus, if the data is going to be re-used for rendering
       * of edges, it must be saved :-(.
       */

      if (pddc->Static.attrs->edges != PEXOff) 
	miCopyPath(pddc, input_list, &edge_copy, 0);

      /*
       * Update the fill area GC to reflect the current
       * fill area attributes
       */
      pGC = pddc->Static.misc.pFillAreaGC;
      if (pddc->Static.misc.flags & FILLAREAGCFLAG)
	miDDC_to_GC_fill_area(pRend, pddc, pddc->Static.misc.pFillAreaGC);

      /*
       * Render appropriately. One can assume that facets have been
       * pre-computed if we need facet data.
       */

      switch (pddc->Static.attrs->intStyle) {

	case PEXInteriorStyleHollow:

	/* At this point, for a fully implemented level1, there would
	 * be another "switch" statement here that would select between
	 * interpolation methods. This switch would exist also exist
	 * for the "solid" fill styles as well. We have, for now,
	 * only implemented InterpNone (flat shading) 
	 */
	
	 {
          /*
           * The final fill area color is determined by a hierarchy
           * of sources. The first source is the vertex colors.
           * If the data has vertex colors, then the final color
           * is an average of the vertex colors. If there are no
           * vertex colors, then the facet is set to the
           * color for the current facet. IF there are no facet colors,
           * then the color is determined by the surface color attribute.
	   * If highlighting is on, then the color is determined by
	   * the surface color attribute which has been set to the 
	   * highlight color
           */

          if ( (DD_IsVertColour(input_list->type)) &&
		(!MI_DDC_IS_HIGHLIGHT(pddc)) ) {
            /*
             * If vertex colors, simply create a facet list.
             */
            DDFacetSIZE(DD_FACET_RGBFLOAT, facet_size);
            fct_list = MI_NEXTTEMPFACETLIST(pddc);
            fct_list->type = DD_FACET_RGBFLOAT;


            MI_ALLOCLISTOFDDFACET(fct_list, input_facet->numFacets, facet_size);
            if (!fct_list->facets.pNoFacet) return(BadAlloc);
            out_fct = fct_list->facets;
            DD_VertPointSize(input_list->type, point_size);
	    DD_VertOffsetColor(input_list->type, color_offset);

            for(i = 0, pddlist = input_list->ddList;
			 i < input_list->numLists; i++) {
              in_pt = pddlist->pts;
              in_pt.ptr += color_offset;       /* skip coord data */

              /* Compute average facet color */
              for (k = 2; k < (pddlist->numPoints); k++) {
                out_fct.pFacetRgbFloat->red = 0.0;
                out_fct.pFacetRgbFloat->green = 0.0;
                out_fct.pFacetRgbFloat->blue = 0.0;

	        tmp_pt.ptr = in_pt.ptr + point_size;

                out_fct.pFacetRgbFloat->red
                                    += in_pt.pRgbFloatClr->red;
                out_fct.pFacetRgbFloat->green
                                    += in_pt.pRgbFloatClr->green;
                out_fct.pFacetRgbFloat->blue
                                    += in_pt.pRgbFloatClr->blue;

                out_fct.pFacetRgbFloat->red
                                    += tmp_pt.pRgbFloatClr->red;
                out_fct.pFacetRgbFloat->green
                                    += tmp_pt.pRgbFloatClr->green;
                out_fct.pFacetRgbFloat->blue
                                    += tmp_pt.pRgbFloatClr->blue;
	        tmp_pt.ptr += point_size;
                out_fct.pFacetRgbFloat->red
                                    += tmp_pt.pRgbFloatClr->red;
                out_fct.pFacetRgbFloat->green
                                    += tmp_pt.pRgbFloatClr->green;
                out_fct.pFacetRgbFloat->blue
                                    += tmp_pt.pRgbFloatClr->blue;

                if (pddlist->numPoints > 2) {
                  out_fct.pFacetRgbFloat->red /= 3;
                  out_fct.pFacetRgbFloat->green /= 3;
                  out_fct.pFacetRgbFloat->blue /= 3;
                }
 		/* clamp on saturation */
                if (out_fct.pFacetRgbFloat->red > 1.0)
                        out_fct.pFacetRgbFloat->red = 1.0;
                if (out_fct.pFacetRgbFloat->green > 1.0)
                        out_fct.pFacetRgbFloat->green = 1.0;
                if (out_fct.pFacetRgbFloat->blue > 1.0)
                        out_fct.pFacetRgbFloat->blue = 1.0;

 
                in_pt.ptr += point_size;      /* skip to next point */
                out_fct.pFacetRgbFloat++;
	        fct_list->numFacets++;
	      } 
	      ++pddlist;
	    }
            /* new facet colors override input ones */
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
 
            pGC = pddc->Static.misc.pFillAreaGC;
            out_fct = input_facet->facets;
            DDFacetSIZE(input_facet->type, facet_size);
	    if (pddc->Static.attrs->echoMode == PEXEcho)
		intcolour = pddc->Static.attrs->echoColour;
	    else {
		intcolour.colourType = PEXRgbFloatColour;
		intcolour.colour.rgbFloat = *out_fct.pFacetRgbFloat;
	    }
            miColourtoIndex(	pRend, pddc->Dynamic->pPCAttr->colourApproxIndex,
				&intcolour, &colourindex);

          }
             
          else {
 
            /*
             * If no vertex or facet colors, use surface attributes.
	     * If highlighting is on, then the color is determined by
	     * the surface color attribute which has been set to the 
	     * highlight color
             */
	    if (pddc->Static.attrs->echoMode == PEXEcho)
		intcolour = pddc->Static.attrs->echoColour;
	    else {
		intcolour = pddc->Static.attrs->surfaceColour;
	    }
               
	    miColourtoIndex( pRend, 
			     pddc->Dynamic->pPCAttr->colourApproxIndex,
			     &intcolour, &colourindex);
           }
 
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
            if (pGC->serialNumber !=
                pRend->pDrawable->serialNumber)
              ValidateGC(pRend->pDrawable, pGC);
 
            /* Render each bound as a polyline.
	     * Note - Additional functionality should be able
	     * to distinguesh between "real" and degenerate facets
	     * (produced by clipping) by some internal mechanism, 
	     * possibly more bits in the edge flags, and NOT
	     * draw the boundaries for degenerate facets here.
	     */

           DD_VertPointSize(input_list->type, point_size);

           for(i = 0, pddlist = input_list->ddList;
		 i < input_list->numLists; i++) {

             for (j = 2, in_pt.ptr = pddlist->pts.ptr;
                        j < pddlist->numPoints; j++) {

               output_array[0] = *in_pt.p2DSpt;

               tmp_pt.ptr = in_pt.ptr + point_size;

               output_array[1] = *tmp_pt.p2DSpt;

               tmp_pt.ptr += point_size;

               output_array[2] = *tmp_pt.p2DSpt;

               output_array[3] = *in_pt.p2DSpt;

               /* Call ddx to fill polygon */
               (*GetGCValue
	       (pddc->Static.misc.pFillAreaGC, ops->Polylines))
                             (pRend->pDrawable,
                             pddc->Static.misc.pFillAreaGC,
                             CoordModeOrigin,
                             4,
                             output_array);

               in_pt.ptr += point_size;

               if ( (input_facet) &&
                 (input_facet->numFacets > 0) &&
                 (DD_IsFacetColour(input_facet->type)) ) {
		 out_fct.pNoFacet += facet_size;
	
		  if (pddc->Static.attrs->echoMode == PEXEcho)
		      intcolour = pddc->Static.attrs->echoColour;
		  else {
		      intcolour.colourType = PEXRgbFloatColour;
		      intcolour.colour.rgbFloat = *out_fct.pFacetRgbFloat;
		  }
            	 miColourtoIndex(   pRend, 
				    pddc->Dynamic->pPCAttr->colourApproxIndex,
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

	       }
             }
	     ++pddlist;
	   }
  
	 }
         break;

	case PEXInteriorStylePattern:
	case PEXInteriorStyleHatch:
	case PEXInteriorStyleSolid:
	 {
          /*
           * The final facet color is determined by a hierarchy
           * of sources. The first source is the by vertex colors.
           * If the data has vertex colors, then the final color
           * is an average of the vertex colors. If there are no
           * vertex colors, then the fill area is set to the facet
           * color for the current facet. IF there are no facet colors,
           * then the color is determined by the surface color attribute.
	   * If highlighting is on, then the color is determined by
	   * the surface color attribute which has been set to the 
	   * highlight color
           */

          if ( (DD_IsVertColour(input_list->type)) &&
		(!MI_DDC_IS_HIGHLIGHT(pddc)) ) {
            /*
             * If vertex colors, simply create a facet list.
             */
            DDFacetSIZE(DD_FACET_RGBFLOAT, facet_size);
            fct_list = MI_NEXTTEMPFACETLIST(pddc);
            fct_list->type = DD_FACET_RGBFLOAT;

	    /* Determine number of facets */
	    for (i = 0, num_facets = 0, pddlist = input_list->ddList;
		 i < input_list->numLists; i++) {
	      if (pddlist->numPoints > 2)
		num_facets += (pddlist->numPoints - 2);
	      pddlist++;
	    }

            MI_ALLOCLISTOFDDFACET(fct_list, num_facets, facet_size);
            if (!fct_list->facets.pNoFacet) return(BadAlloc);
            out_fct = fct_list->facets;

            DD_VertPointSize(input_list->type, point_size);
            DD_VertOffsetColor(input_list->type, color_offset);

            for(i = 0, pddlist = input_list->ddList;
			 i < input_list->numLists; i++) {
              in_pt = pddlist->pts;
              in_pt.ptr += color_offset;       /* skip coord data */
 
              /* Compute average facet color */
              for (k = 2; k < (pddlist->numPoints); k++) {
                out_fct.pFacetRgbFloat->red = 0.0;
                out_fct.pFacetRgbFloat->green = 0.0;
                out_fct.pFacetRgbFloat->blue = 0.0;

                tmp_pt.ptr = in_pt.ptr + point_size;

                out_fct.pFacetRgbFloat->red
                                    += in_pt.pRgbFloatClr->red;
                out_fct.pFacetRgbFloat->green
                                    += in_pt.pRgbFloatClr->green;
                out_fct.pFacetRgbFloat->blue
                                    += in_pt.pRgbFloatClr->blue;

                out_fct.pFacetRgbFloat->red
                                    += tmp_pt.pRgbFloatClr->red;
                out_fct.pFacetRgbFloat->green
                                    += tmp_pt.pRgbFloatClr->green;
                out_fct.pFacetRgbFloat->blue
                                    += tmp_pt.pRgbFloatClr->blue;
                tmp_pt.ptr += point_size;
                out_fct.pFacetRgbFloat->red
                                    += tmp_pt.pRgbFloatClr->red;
                out_fct.pFacetRgbFloat->green
                                    += tmp_pt.pRgbFloatClr->green;
                out_fct.pFacetRgbFloat->blue
                                    += tmp_pt.pRgbFloatClr->blue;

                if (pddlist->numPoints > 2) {
                  out_fct.pFacetRgbFloat->red /= 3.0;
                  out_fct.pFacetRgbFloat->green /= 3.0;
                  out_fct.pFacetRgbFloat->blue /= 3.0;
                }
		 /* clamp on saturation */
                if (out_fct.pFacetRgbFloat->red > 1.0)
                        out_fct.pFacetRgbFloat->red = 1.0;
                if (out_fct.pFacetRgbFloat->green > 1.0)
                        out_fct.pFacetRgbFloat->green = 1.0;
                if (out_fct.pFacetRgbFloat->blue > 1.0)
                        out_fct.pFacetRgbFloat->blue = 1.0;


                in_pt.ptr += point_size;      /* skip to next point */
                out_fct.pFacetRgbFloat++;
                fct_list->numFacets++;
              }
	      ++pddlist;
	    }
            /* new facet colors override input ones */
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
 
            pGC = pddc->Static.misc.pFillAreaGC;
            out_fct = input_facet->facets;
            DDFacetSIZE(input_facet->type, facet_size);
 
 
            /* Render each bound as a polygon */

           DD_VertPointSize(input_list->type, point_size);

           for(i = 0, pddlist = input_list->ddList; 
				i < input_list->numLists; i++) {

             for (j = 2, in_pt.ptr = pddlist->pts.ptr;
                        j < pddlist->numPoints; j++) {

                  output_array[0] = *in_pt.p2DSpt;

                  tmp_pt.ptr = in_pt.ptr + point_size;

                  output_array[1] = *tmp_pt.p2DSpt;

                  tmp_pt.ptr += point_size;

                  output_array[2] = *tmp_pt.p2DSpt;

                  output_array[3] = *in_pt.p2DSpt;


                /* Compute index value for ddx */
		if (pddc->Static.attrs->echoMode == PEXEcho)
		    intcolour = pddc->Static.attrs->echoColour;
		else {
		    intcolour.colourType = PEXRgbFloatColour;
		    intcolour.colour.rgbFloat = *out_fct.pFacetRgbFloat;
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
 
                /* Call ddx to fill polygon */
                (*GetGCValue(pGC, ops->FillPolygon))
                             (pRend->pDrawable,
                             pGC,
                             Convex,
                             CoordModeOrigin,
                             4,
                             output_array);


 
                in_pt.ptr += point_size;
		out_fct.pNoFacet += facet_size;
              }
	      pddlist++;
	    }
          }
             
          else {
 
            /*
             * If no vertex or facet colors, use surface attributes.
	     * If highlighting is on, then the color is determined by
	     * the surface color attribute which has been set to the 
	     * highlight color
             */
	    if (pddc->Static.attrs->echoMode == PEXEcho)
		intcolour = pddc->Static.attrs->echoColour;
	    else {
		intcolour = pddc->Static.attrs->surfaceColour;
	    }
               
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
	       /* Insure that the GC is reset to proper color next time */
	       pddc->Static.misc.flags |= FILLAREAGCFLAG;
            }
 
            /* validate GC prior to start of rendering */
            if (pGC->serialNumber !=
                pRend->pDrawable->serialNumber)
              ValidateGC(pRend->pDrawable, pGC);
 
            /* Render each bound as a polyline */
            DD_VertPointSize(input_list->type, point_size);

            for(i = 0, pddlist = input_list->ddList;
                                i < input_list->numLists; i++) {
 
 	      for (j = 2, in_pt.ptr = pddlist->pts.ptr;
                        j < pddlist->numPoints; j++) {

                output_array[0] = *in_pt.p2DSpt;

                tmp_pt.ptr = in_pt.ptr + point_size;

                output_array[1] = *tmp_pt.p2DSpt;

                tmp_pt.ptr += point_size;

                output_array[2] = *tmp_pt.p2DSpt;

                output_array[3] = *in_pt.p2DSpt;
 
                /* Call ddx to fill polygon */ 
                (*GetGCValue(pGC, ops->FillPolygon))
                             (pRend->pDrawable,
                             pGC, 
                             Convex,
                             CoordModeOrigin, 
                             4, 
                             output_array);
    
                in_pt.ptr += point_size;
              }
	      pddlist++;
	    }

 
         break;
	 }
      }

	case PEXInteriorStyleEmpty:
	 break;

      } /*End of area rendering */

    /*
     * Now check to see if fill area edges are to be drawn
     */
    if (pddc->Static.attrs->edges != PEXOff) {

        input_list = edge_copy;

	DD_VertPointSize(input_list->type, point_size);
	DD_VertOffsetEdge(input_list->type, edge_offset);

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

 
          for (i = 0, pddlist = input_list->ddList; 
	     i < input_list->numLists; i++) {


  	    /* Render each bound as a polyline */

	    /*
             * Within an edge flag, there are two bits determining edge
             * visibility. Bit zero determines whether an edge is drawn
             * between the current vertex N and vertex N-2, while bit one
             * determines if an edge is drawn between edge N and N+1.
             * If N-2 < 0 or N+1 > numfacets the edge is not drawn.
             *
             *         2_________ _4
             *         /\         /|\
             *        /  \       /   \
             *       /    \     /     \
             *      /      \   /       \
             *    1<--------+3+---------5
             *
             * So, for the third element in the edge list,
             * bit 0 indicates presence of an edge from 3 -> 4
             *    (forward edge)
             * bit 1 indicates presence of an edge from 3 -> 1
             *    (backward edge)
             * 
             */
  
 
	     for (j = 0, in_pt.ptr = pddlist->pts.ptr;
		 j < pddlist->numPoints; j++) {

	     if((j+1) < pddlist->numPoints){
	       if(FWD_EDGE_FLAG & *(in_pt.ptr + edge_offset)) {

		  output_array[0] = *in_pt.p2DSpt;

		  tmp_pt.ptr = in_pt.ptr + point_size;

		  output_array[1] = *tmp_pt.p2DSpt; 

	     	  (*GetGCValue(pddc->Static.misc.pEdgeGC, ops->Polylines)) 
	 		 (pRend->pDrawable,
			  pddc->Static.misc.pEdgeGC, 
			  CoordModeOrigin, 
			  2, 
			  output_array);
	       }
	     }
	     if (j > 1){
	       if(BKWD_EDGE_FLAG & *(in_pt.ptr + edge_offset)) {

       		  output_array[0] = *in_pt.p2DSpt;
 
                  tmp_pt.ptr = in_pt.ptr - (2 *  point_size);
 
                  output_array[1] = *tmp_pt.p2DSpt; 

                  (*GetGCValue(pddc->Static.misc.pEdgeGC, ops->Polylines))
                         (pRend->pDrawable,
                          pddc->Static.misc.pEdgeGC,
                          CoordModeOrigin,
                          2,
                          output_array);
                
	       }
	     }
	     in_pt.ptr += point_size;
	   }
	   ++pddlist;
         }
    }

    return (Success);
  }
}

