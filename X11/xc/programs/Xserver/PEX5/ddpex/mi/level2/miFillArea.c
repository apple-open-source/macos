/* $Xorg: miFillArea.c,v 1.4 2001/02/09 02:04:09 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/level2/miFillArea.c,v 3.7 2001/12/14 19:57:23 dawes Exp $ */

#include "miLUT.h"
#include "misc.h"
#include "miscstruct.h"
#include "ddpex3.h"
#include "PEXErr.h"
#include "miStruct.h"
#include "PEXprotost.h"
#include "miRender.h"
#include "gcstruct.h"
#include "ddpex2.h"
#include "miLight.h"
#include "miClip.h"
#include "pexos.h"


static ddpex3rtn	Complete_FillArea_Facetlist();
static ddpex3rtn	Calculate_FillArea_Facet_Normal();
static ddpex3rtn	Calculate_FillArea_Vertex_Color_and_Normal();

/*++
 |
 |  Function Name:	miFillArea
 |
 |  Function Description:
 |	 Handles the Fill area 3D, Fill area 2D,  Fill area 3D with data,
 |	Fill are set 2D,  Fill are set 3D, Fill are set 3D with data ocs.
 |
 |  Note(s):
 |
 --*/

ddpex3rtn
miFillArea(pRend, pExecuteOC)
/* in */
    ddRendererPtr       pRend;	  /* renderer handle */
    miGenericStr       *pExecuteOC;
/* out */
{
/* calls */
      ddpex3rtn		miTransform();
      ddpex3rtn		miConvertVertexColors();
      ddpex3rtn		miConvertFacetColors();
      ddpex3rtn		miLightFillArea();
      ddpex3rtn		miClipFillArea();
      ddpex3rtn		miCullFillArea();
      ddpex3rtn		miRenderFillArea();

/* Local variable definitions */
      miFillAreaStruct	*ddFill = (miFillAreaStruct *)(pExecuteOC+1);
      miListHeader	*input_list = &ddFill->points;	    /* Input points */
      ddBitmaskShort	shape = ddFill->shape;		    /* shape hint */
      ddUCHAR		noEdges = ddFill->ignoreEdges;	    /* edge flag*/
      listofddFacet	*input_facet = ddFill->pFacets;	    /* facets */
      miDDContext	*pddc = (miDDContext *)(pRend->pDDContext);

      miListHeader	*color_list,
      			*mc_list,
			*mc_clist,
			*wc_list, 
      			*light_list, 
      			*cc_list, 
			*clip_list,
			*dcue_list,
			*cull_list,
			*dc_list;

      listofddFacet	*color_facet,
      			*mc_facet,
			*wc_facet,
      			*light_facet,
      			*cc_facet,
			*clip_facet,
			*cull_facet,
			*dc_facet;

      listofddPoint	*sp;
      int		i, j;
      ddUSHORT 	      	clip_mode;
      ddpex3rtn		status;
      ddPointType	out_type;

      /*
       * Convert per-vertex and per-facet colors to rendering color model.
       * Note that this implementation only supports rgb float.
       */

      if (DD_IsVertColour(input_list->type)) {
	if (status = miConvertVertexColors(pRend, 
					   input_list, PEXRdrColourModelRGB, 
					   &color_list))
	  return (status);
      } else {
	color_list = input_list;
      } 

      if ((input_facet) && (DD_IsFacetColour(input_facet->type))) {
	if (status = miConvertFacetColors(pRend, 
					  input_facet, PEXRdrColourModelRGB, 
					  &color_facet))
	  return (status);
      } else {
	color_facet = input_facet;
      } 

      /* Check for Model clipping */
 
      if (pddc->Dynamic->pPCAttr->modelClip == PEXClip) {
	/* Compute  modelling coord version of clipping volume */ 
        ComputeMCVolume(pRend, pddc);      
        clip_mode = MI_MCLIP;

	/* Tranform points to 4D for clipping */
        out_type = color_list->type;
        if (status = miTransform(pddc,
                                 color_list, &mc_clist,
                                 ident4x4, 
                                 ident4x4, 
                                 DD_SetVert4D(out_type)))
          return (status);
 
 
        if (status = miClipFillArea(pddc, mc_clist, color_facet,
                                &mc_list, &mc_facet, clip_mode))
            return (status);

	/* if nothing left after modeling clip, return early */
	if (mc_list->numLists <= 0) return(Success);
 
      } else {
        mc_list = color_list;
        mc_facet = color_facet;
      }

      clip_mode = MI_VCLIP;

      /* 
       * First, check lighting requirements 
       */
      if (pddc->Static.attrs->reflModel != PEXReflectionNoShading) {
	
	/* Transform to WC prior to applying lighting */
	out_type = mc_list->type;
	if (DD_IsVertNormal(out_type)) VALIDATEINVTRMCTOWCXFRM(pddc);
	if (status = miTransform(pddc, mc_list, &wc_list, 
				 pddc->Dynamic->mc_to_wc_xform,
				 pddc->Static.misc.inv_tr_mc_to_wc_xform,
				 DD_SetVert4D(out_type)))
          return (status);

	/* Transform facet normals if necessary */
	if ((mc_facet) && 
	    (mc_facet->numFacets > 0) &&
	    (DD_IsFacetNormal(mc_facet->type))) {
	  VALIDATEINVTRMCTOWCXFRM(pddc);
	  if (status = miFacetTransform(pddc, 
				mc_facet, &wc_facet, 
				pddc->Static.misc.inv_tr_mc_to_wc_xform))
            return (status);
	} else wc_facet = mc_facet;

	/* Apply lighting */
	if (status = miLightFillArea(pRend, pddc, 
				     wc_list, wc_facet, 
				     &light_list, &light_facet))
          return (status);

	
	/* Transform to CC for clipping */
	if (DD_IsVertNormal(light_list->type)) VALIDATEINVTRWCTOCCXFRM(pddc);
	if (status = miTransform(pddc, light_list, &cc_list, 
				 pddc->Dynamic->wc_to_cc_xform,
				 pddc->Static.misc.inv_tr_wc_to_cc_xform,
				 light_list->type))
          return (status);

	/* Transform facet normals if necessary */
	if ( (light_facet) && 
	     (light_facet->numFacets > 0) &&
	     (DD_IsFacetNormal(light_facet->type)) ) {
	  VALIDATEINVTRWCTOCCXFRM(pddc);
	  if (status = miFacetTransform(pddc, 
				light_facet, &cc_facet, 
				pddc->Static.misc.inv_tr_wc_to_cc_xform))
            return (status);
	} else cc_facet = light_facet;

      } 
      else {
         
        out_type = mc_list->type;
	if (DD_IsVertNormal(out_type)) VALIDATEINVTRMCTOCCXFRM(pddc);
        if (status = miTransform(pddc, mc_list, &cc_list,
                                 pddc->Dynamic->mc_to_cc_xform,
                                 pddc->Static.misc.inv_tr_mc_to_cc_xform,
                                 DD_SetVert4D(out_type)))
          return (status);
 

	if ((mc_facet) && 
	    (mc_facet->numFacets > 0) &&
	    (DD_IsFacetNormal(mc_facet->type))) {
	    VALIDATEINVTRMCTOCCXFRM(pddc);
	    if (status = miFacetTransform(pddc, 
				mc_facet, &cc_facet, 
				pddc->Static.misc.inv_tr_mc_to_cc_xform))
            return (status);
	} else cc_facet = mc_facet;
      }

      /* View clip primitive */
      if (status = miClipFillArea(pddc, cc_list, cc_facet,
			      &clip_list, &clip_facet, clip_mode))
	    return (status);

      /* if nothing left after view clip, return early */
      if (clip_list->numLists <= 0) return(Success);

      /* Now cull according to current culling mode */
      if (pddc->Dynamic->pPCAttr->cullMode) {
        if (status = miCullFillArea(pddc, clip_list, clip_facet,
				    &cull_list, &cull_facet))
	    return (status);

	/* if nothing left after culling, return early */
	if (cull_list->numLists <= 0) return(Success);
	clip_list = cull_list;
	clip_facet = cull_facet;
      } else {
	cull_list = clip_list;
	cull_facet = clip_facet;
      }

      /* DEPTH CUEING */
      if (pddc->Dynamic->pPCAttr->depthCueIndex) {
	miDepthCueFillArea(pRend, cull_list, cull_facet, &dcue_list);
	cull_list = dcue_list;
      }

      /* Lastly, transform to DC coordinates */
      out_type = cull_list->type;
      DD_SetVert2D(out_type);
      DD_SetVertShort(out_type);
      if (DD_IsVertNormal(out_type)) VALIDATEINVTRCCTODCXFRM(pddc);
      if (status = miTransform(pddc, cull_list, &dc_list, 
				 pddc->Dynamic->cc_to_dc_xform,
				 pddc->Static.misc.inv_tr_cc_to_dc_xform,
				 out_type) )
	  return (status);

      /* Transform facet normals if necessary */
      if ( (clip_facet) && 
	     (clip_facet->numFacets > 0) &&
	     (DD_IsFacetNormal(clip_facet->type)) ) {
        VALIDATEINVTRCCTODCXFRM(pddc);
        if (status = miFacetTransform(pddc, 
				clip_facet, &dc_facet, 
				pddc->Static.misc.inv_tr_cc_to_dc_xform))
	    return (status);
      } else dc_facet = clip_facet;


      return (pddc->Static.RenderProcs[FILLAREA_RENDER_TABLE_INDEX](pRend, 
								    pddc, 
								    dc_list, 
								    dc_facet, 
								    shape, 
								    noEdges));
}

/*++
 |
 |  Function Name:	miClipFillArea
 |
 |  Function Description:
 |	 Handles the fill area 3D,  fill area 2D, 
 |	 and fill area set 3D with data ocs.
 |
 |  Note(s):
 |
 |	 This routine uses a Sutherland-Hodgman approach for
 |	 polygon clipping. (See, for example, Rodger's "Procedural
 |	 Elements for Computer Graphics", pp 169-179)).
 |	 Each list is clipped successively against each (enabled) 
 |	 clipping boundary. 
 |
 --*/
ddpex3rtn
miClipFillArea(pddc, input_vert, input_fct, output_vert, output_fct, clip_mode)
/* in */
	miDDContext	*pddc;
        miListHeader    *input_vert;	/* input vertex data */
	listofddFacet	*input_fct;     /* input facet data */
        miListHeader    **output_vert;	/* output vertex data */
	listofddFacet	**output_fct;   /* output facet data */
	ddUSHORT	clip_mode;	/* view or model clipping */
{
/* calls */
    ddpex3rtn	miCloseFillArea();

/* uses */
    ddPointUnion        in_ptP, in_ptQ;
    ddPointUnion        out_pt;
    float               t_ptP, t_ptQ;
    char		*in_fct, *out_fct;
    int			point_size;
    int			facet_size;
    int			num_points;
    int			extra_point;
    int			vert_count;
    miListHeader	*input, *output, *list1, *list2;
    listofddPoint	*pddilist;
    listofddPoint	*pddolist;
    listofddFacet	*finput, *foutput, *fct_list1, *fct_list2;
    int			num_lists;
    int			i, j, k, num_planes;
    ddUSHORT		clipflags;
    ddUSHORT		current_clip;
    int			edge_offset, clip_code, pts_inlist;
    ddULONG		*edge_ptr;
    ddHalfSpace         *MC_HSpace;
    ddpex3rtn		status;
    char		do_edges, 
			outside;   	/* flag to indicate current */
                                        /* point of interest is outside */



    /* Vertex data must be homogeneous for view clipping */
    if ((clip_mode == MI_VCLIP) && !(DD_IsVert4D(input_vert->type))) 
		return(1);

    /* Insure that the polygon bounds form closed contours */
    if (status = miCloseFillArea(input_vert)) return(status);

    /* PHIGS specifies a single facet for a set of fill areas */
    *output_fct = input_fct;

    /* 
     * Use the pre-defined clip lists for output 
     * Note that two buffers are used in "ping-pong" fashion:
     * first the first buffer is used for output, then the second.
     */
    list1 = MI_NEXTTEMPDATALIST(pddc);
    MI_ALLOCLISTHEADER(list1, MI_ROUND_LISTHEADERCOUNT(input_vert->numLists));
    if (!list1->ddList) return(BadAlloc);

    list2 = MI_NEXTTEMPDATALIST(pddc);
    MI_ALLOCLISTHEADER(list2, MI_ROUND_LISTHEADERCOUNT(input_vert->numLists));
    if (!list2->ddList) return(BadAlloc);

    /* 
     * Must have edge flags in vertex if edges are to be drawn - 
     * otherwise, cannot determine wich edges are "original" and
     * which edges where added by the clipper. 
     */

    if (pddc->Static.attrs->edges != PEXOff) {
      do_edges = PEXOn;
      if (!(DD_IsVertEdge(input_vert->type))) {
        if (status = miAddEdgeFlag(pddc, input_vert, &list1)) 
		return(status);
        input = list1;
      } else input = input_vert;
    } else {
      do_edges = PEXOff;
      /* Allocate an initial number of headers */
      input = input_vert;
    }
    /* Note that adding edges will change the input type */
    list1->type = input->type;
    list2->type = input->type;
    list1->flags =  input->flags;
    list2->flags =  input->flags;
    output = list2;


    /* Get point size so that this works for all point types */
    DD_VertPointSize(input->type, point_size);
    DD_VertOffsetEdge(input->type, edge_offset);

    /* 
     * Each list is now clipped in turn against each (enabled) boundary.
     */

 
    if (clip_mode == MI_MCLIP) {
        num_planes = pddc->Static.misc.ms_MCV->numObj;
        MC_HSpace = (ddHalfSpace *)(pddc->Static.misc.ms_MCV->pList);
    }
    else num_planes = 6;  /* view clipping to a cube */
 
    for (i = 0; i < num_planes; i++) {
        current_clip = 1 << i;	/* current_clip is new clip bound */

        num_lists = 0;		/* Restart list counter each pass */

        for (j = 0, pddilist = input->ddList, pddolist = output->ddList,
             output->numLists = 0;
             j < input->numLists; j++) {
 

	  /* Don't process if no points */
       	  if ((vert_count = pddilist->numPoints) <= 0) {
	    pddilist++;
	    continue;
          }

          /* 
	   * Insure sufficient room for each vertex.
	   * Note that twice the vertex count is an upper-bound for
	   * a clipped polygon (although there is probably a "tighter fit").
	   */
          MI_ALLOCLISTOFDDPOINT(pddolist, 2*vert_count, point_size);
          if (!(out_pt.ptr = (char *)pddolist->pts.ptr)) return(BadAlloc);
	  pts_inlist = 0;
	  clip_code = 0;
                                        /* Aquire two points */
                                        /* and generate clip code */
          in_ptP.ptr = pddilist->pts.ptr;
          COMPUTE_CLIP_PARAMS(in_ptP,t_ptP,0,clip_mode,
                        current_clip,MC_HSpace,clip_code);

          if(!(clip_code)) {
            COPY_POINT(in_ptP, out_pt, point_size);
            ++pts_inlist;
            out_pt.ptr += point_size;
          }
 
          in_ptQ.ptr = in_ptP.ptr + point_size;

          for (k = 1; k < pddilist->numPoints; k++){

            COMPUTE_CLIP_PARAMS(in_ptQ, t_ptQ, 1, clip_mode,
                        current_clip,MC_HSpace,clip_code);

            switch(clip_code) {
              case 0:                   /* both P and Q are in bounds */
                COPY_POINT(in_ptQ, out_pt, point_size);
                out_pt.ptr += point_size;
                ++pts_inlist;
		outside = PEXOff;
                break;
              case 1:                   /* P is out, Q is in */
                CLIP_AND_COPY(input->type, in_ptP, t_ptP,
                              in_ptQ, t_ptQ, out_pt);
                out_pt.ptr += point_size;
                COPY_POINT(in_ptQ, out_pt, point_size);
                out_pt.ptr += point_size;
                pts_inlist += 2;
		outside = PEXOff;
                break;
              case 2:                   /* P is in, Q is out */
                CLIP_AND_COPY(input->type, in_ptQ, t_ptQ,
                                in_ptP, t_ptP, out_pt);
                if (do_edges == PEXOn) {
                  edge_ptr = (ddULONG *)(out_pt.ptr + edge_offset);
                  *edge_ptr = 0;
                }
                out_pt.ptr += point_size;
                ++pts_inlist;
		outside = PEXOn;
                break;
              case 3:                   /* both are out */
		outside = PEXOn;
                break;
            }
 
            in_ptP.ptr = in_ptQ.ptr;
	    t_ptP = t_ptQ;
            in_ptQ.ptr += point_size;
            clip_code >>= 1;
          }

          if (pts_inlist > 1) {
	    if (outside == PEXOn) {
	      /* special case where last segment clipped out. Need to
		 close the fill area */
	      COPY_POINT(pddolist->pts, out_pt, point_size);
	      pts_inlist++;
	    }
            pddolist->numPoints = pts_inlist;
            pddolist++;
            num_lists++;

	  }

	  /* Now, skip to next input list */
          pddilist++;
        }

        /* Complete initialization of output list header */
        output->numLists = num_lists;

        /* Use result of previous clip for input to next clip */
        input = output;
        if (output == list2) 
	  output = list1;
        else output = list2;

        if (clip_mode == MI_MCLIP) MC_HSpace++;
      } /* end of processing for all planes */

    /* Current input list is last processed (clipped) list */
    *output_vert = input;
    return (Success);

}

/*++
 |
 |  miLightFillArea(pRend, pddc, input_vert, input_fct, output_vert, output_fct)
 |
 |	Perform lighting calculations for the vertex or facet
 |	data provided according to the current attributes.
 |
 --*/
ddpex3rtn
miLightFillArea(pRend, pddc, input_vert, input_fct, output_vert, output_fct)
    ddRendererPtr	pRend;		/* renderer handle */
    miDDContext		*pddc;		/* dd Context pointer */
    miListHeader	*input_vert;    /* input vertex data */
    listofddFacet	*input_fct;     /* input facet data */
    miListHeader	**output_vert;  /* output vertex data */
    listofddFacet	**output_fct;   /* output facet data */
{
/* calls */
    ddpex3rtn		miApply_Lighting();
	   ddpex3rtn	miFilterPath();

/* uses */
    listofddFacet		*fct_list;
    miListHeader		*out_vert;
    ddRgbFloatNormal		*in_fct;
    ddRgbFloatColour		*out_fct;
    listofddPoint		*pddilist;
    listofddPoint		*pddolist;
    ddRgbFloatNormalPoint4D	*in_pt;
    ddRgbFloatPoint4D		*out_pt;
    int				i, j;
    ddpex3rtn			status;

    /*
     * First, Insure that the vertex and/or facet data
     * is sufficient for the current Surface Interpolation method.
     * Note that this implementation does not support
     * PEXSurfaceInterpDotProduct and PEXSurfaceInterpNormal and
     * that these surface interpolation types are approximated by
     * PEXSurfaceInterpColour. The cases dealt with here, therefore,
     * are constant surface color (thus a single color is computed
     * per facet) and interpolated surface color (thus a color
     * per verted is required).
     */
    switch(pddc->Static.attrs->surfInterp) {

      case PEXSurfaceInterpNone:

	  /*
	   * Insure that input facet data is in proper format
	   * for flat shading.
	   */
          if ((!input_fct) || 
	      (input_fct->numFacets == 0) ||
	      (!( (DD_IsFacetColour(input_fct->type)) && 
	        (DD_IsFacetNormal(input_fct->type))))) {
		Complete_FillArea_Facetlist(pddc, input_vert, input_fct, 
						output_fct);
		input_fct = *output_fct;
	  }

	  /*
	   * Should have facets with normals and surface colors now.
	   * Since we are flat shading, there is no further use
	   * for per-vertex color or normal data (however, leave
	   * any edge information). Remove it to prevent confusion
	   * further down the pipeline.
	   */
	  if ( (DD_IsVertNormal(input_vert->type)) ||
		(DD_IsVertNormal(input_vert->type)) ) {
	    if (status = miFilterPath(pddc, input_vert, output_vert,
				      ((1 << 3) | 1) ))
	      return(status);
	  } else {
	    *output_vert = input_vert;
	  }

	  /*
	   * allocate storage for the facet list
	   * Note that the output facet list only contains colors.
	   */
	  *output_fct = fct_list = MI_NEXTTEMPFACETLIST(pddc);
	  fct_list->numFacets = input_fct->numFacets;
	  fct_list->type = DD_FACET_RGBFLOAT;
	  MI_ALLOCLISTOFDDFACET(fct_list, input_fct->numFacets,
				sizeof(ddRgbFloatColour));
	  if (!(out_fct = fct_list->facets.pFacetRgbFloat)) return(BadAlloc);
	  in_fct = input_fct->facets.pFacetRgbFloatN;
	  pddilist = input_vert->ddList;

	  /*
	   * Compute lighted facet color for each facet.
	   * Facet color is simply the sum of the lighting contributions 
	   * from each light source.
	   */
	  for (i= 0; i < input_fct->numFacets; i++) {
	    if (status = miApply_Lighting(pRend, pddc, 
					  pddilist->pts.p4Dpt, 
					  &(in_fct->colour), 
					  &(in_fct->normal), 
					  out_fct ))
		return(status);

	      in_fct++;
	      out_fct++;
	      pddilist++;
	  }
	break;

      case PEXSurfaceInterpColour:
      case PEXSurfaceInterpDotProduct:
      case PEXSurfaceInterpNormal:

	if ( (!DD_IsVertColour(input_vert->type)) || 
	     (!DD_IsVertNormal(input_vert->type)) ) {
	  Calculate_FillArea_Vertex_Color_and_Normal(pddc, input_vert,
						     input_fct,
						     output_vert);
	  input_vert = *output_vert;
	}

	/* No further need for per facet data */
	*output_fct = 0;

	/* Perform shading on a per-vertex basis */
	if (pddc->Static.attrs->reflModel != PEXReflectionNoShading) {

	  /* Use one of the pre-defined 4D list for output */
	  *output_vert = out_vert = MI_NEXTTEMPDATALIST(pddc);
 
	  /* Insure sufficient room for each header */
	  MI_ALLOCLISTHEADER(out_vert, input_vert->numLists)
	  if (!out_vert->ddList) return(BadAlloc);

	  out_vert->type = DD_RGBFLOAT_POINT4D;
	  out_vert->numLists = input_vert->numLists;
	  out_vert->flags =  input_vert->flags;

	  pddilist = input_vert->ddList;
	  pddolist = out_vert->ddList;

	  for (i = 0; i < input_vert->numLists; i++) {

	    pddolist->numPoints = pddilist->numPoints;

	    MI_ALLOCLISTOFDDPOINT(pddolist,(pddilist->numPoints+1),
				  sizeof(ddRgbFloatPoint4D));
	    if (!(out_pt = pddolist->pts.pRgbFloatpt4D)) return(BadAlloc);
	    in_pt = pddilist->pts.pRgbFloatNpt4D;

	    for (j = 0; j < pddilist->numPoints; j++)
	     {
	      out_pt->pt = in_pt->pt;
	      if (status = miApply_Lighting(pRend, pddc, 
					    &(in_pt->pt),
					    &(in_pt->colour),
				            &(in_pt->normal),
				            &(out_pt->colour)))
	        return(status);

	      in_pt++;
	      out_pt++;
	     }
	    pddilist++;
	    pddolist++;
	  }

	}

	break;

      default:
	*output_vert = input_vert;
	*output_fct = input_fct;

    }

    return(Success);
}

/*++
 |
 |  miCullFillArea(pddc, input_vert, input_fct, output_vert, output_fct)
 |
 |	Perform lighting calculations for the vertex or facet
 |	data provided according to the current attributes.
 |
 --*/
ddpex3rtn
miCullFillArea(pddc, input_vert, input_fct, output_vert, output_fct)
    miDDContext		*pddc;		/* dd Context pointer */
    miListHeader	*input_vert;    /* input vertex data */
    listofddFacet	*input_fct;     /* input facet data */
    miListHeader	**output_vert;  /* output vertex data */
    listofddFacet	**output_fct;   /* output facet data */
{
/* uses */
    miListHeader		*out_vert;
    listofddPoint		*pddilist;
    listofddPoint		*pddolist;
    listofddFacet		*fct_list;
    ddFacetUnion		in_fct;
    ddFacetUnion		out_fct;
    listofddPoint		temp;
    int				i, j;
    char			accept;
    char			return_facet_list;
    int				numLists=0;
    int				point_size, facet_size;

    /*
     * Create facet normals if necessary. These are used to determine
     * if the facet is to be culled. Note: only return a facet list
     * if a valid facet list is input.
     */
    if ( (!input_fct) || (input_fct->numFacets <= 0) ) {
	Calculate_FillArea_Facet_Normal(pddc, input_vert, 
					(listofddFacet *)0, &input_fct);
	return_facet_list = 0;
	*output_fct = 0;

    } else {
	if (!(DD_IsFacetNormal(input_fct->type))) {
	  Calculate_FillArea_Facet_Normal(pddc, input_vert, 
					input_fct, output_fct);
	  input_fct = *output_fct;
	}
	return_facet_list = 1;
    }

    /*
     * allocate storage for the output vertex and facet list
     */
    *output_vert = out_vert = MI_NEXTTEMPDATALIST(pddc);
    out_vert->type = input_vert->type;
    out_vert->flags = input_vert->flags;
    MI_ALLOCLISTHEADER(out_vert, input_vert->numLists)
    if (!out_vert->ddList) return(BadAlloc);
    pddilist = input_vert->ddList;
    pddolist = out_vert->ddList;
    DD_VertPointSize(input_vert->type, point_size);

    fct_list = MI_NEXTTEMPFACETLIST(pddc);
    fct_list->type = input_fct->type;
    DDFacetSIZE(input_fct->type, facet_size);
    MI_ALLOCLISTOFDDFACET(fct_list, input_fct->numFacets, facet_size);
    out_fct = fct_list->facets;
    if (!out_fct.pNoFacet) return(BadAlloc);
    in_fct = input_fct->facets;


    /*
     * This test is performed in NPC space. As a result,
     * the sign of the z component of the facet normal
     * indicates the direction in which the facet is pointing.
     * Therefore if the cullmode is PEXBackFaces and the
     * z component is negative, reject the facet. Similarily,
     * if the z component of the normal is positive, and 
     * the cullmode is PEXFrontFaces, also reject the face.
     * Lastly, note that it is not necessary to copy the vertex
     * data - it is sufficient to copy the listofddPoint header.
     * To avoid alloc problems, however, the headers
     * are interchanged. (As opposed to over-writing the destination
     * header)
     */
    for (i= 0; i < input_fct->numFacets; i++) {

	accept = 0;

        if (pddc->Dynamic->pPCAttr->cullMode == PEXBackFaces) {
	  if (DD_IsFacetColour(input_fct->type)) {
	    if (in_fct.pFacetRgbFloatN->normal.z >= 0) accept = 1;
	  } else if (in_fct.pFacetN->z >= 0) accept = 1;
	} else /* pddc->Dynamic->pPCAttr->cullMode == PEXFrontFaces */ {
	  if (DD_IsFacetColour(input_fct->type)) {
	    if (in_fct.pFacetRgbFloatN->normal.z < 0) accept = 1;
	  } else if (in_fct.pFacetN->z < 0) accept = 1;
	}


	if (accept) {
	  /* First, swap the listofddPoint headers */
	  temp = *pddilist;
	  *pddilist = *pddolist;
	  *(pddolist++) = temp;
	  
	  /* Now, copy the facet info */
	  if (DD_IsFacetColour(input_fct->type))
	    *(out_fct.pFacetRgbFloatN++) = *in_fct.pFacetRgbFloatN;
	  else *(out_fct.pFacetN++) = *in_fct.pFacetN;

	  numLists++;
	}

	pddilist++;
	if (DD_IsFacetColour(input_fct->type)) in_fct.pFacetRgbFloatN++;
	else in_fct.pFacetN++;
    }

    out_vert->numLists = numLists;
    fct_list->numFacets = numLists;

    /* 
     * Only return facet list if one was passed in. Reduces the
     * information that must be processed by the rest of the pipeline.
     */
    if (return_facet_list) *output_fct = fct_list; 

    return(Success);
}

/*++
 |
 |  Function Name:	miCloseFillArea
 |
 |  Function Description:
 |	 Close each bound in a list of vertex lists. "Closing"
 |	 a bound means insuring that the last point in the 
 |	 bound is the same as the first point. This routine is
 |	 used, for example, to close each bound in a polygon
 |	 prior to "hollow" style rendering.
 |
 |  Note(s):
 |	 Note this routine does its work in place - the input
 |	 list of vertices is (potentially) modified.
 |
 --*/

ddpex3rtn
miCloseFillArea(vinput)
/* in */
        miListHeader    *vinput;
{
/* uses */
    char		*first_pt, *last_pt;
    listofddPoint	*pddlist;
    int			vert_count;
    int			point_size;
    int			i, j;
    int			close;

    pddlist = vinput->ddList;
    DD_VertPointSize(vinput->type, point_size);

    /* 
     * Close each list.
     */
    for (i = 0; i < vinput->numLists; i++) {

       	  if ((vert_count = pddlist->numPoints) <= 1) {
	    pddlist++;
	    continue;
          }

	  close = 1;

	  /* 
	   * Insure that the list forms a closed bound. 
	   * Note that comparison is point type dependant.
	   */
          first_pt = (char *)pddlist->pts.p4Dpt;
          last_pt =  ((char *)pddlist->pts.p4Dpt) + (vert_count-1)*point_size;

	  if (DD_IsVertFloat(vinput->type)) {

	    if ((DD_IsVert2D(vinput->type)) &&
		(((ddCoord4D *)first_pt)->x == ((ddCoord4D *)last_pt)->x) &&
		(((ddCoord4D *)first_pt)->y == ((ddCoord4D *)last_pt)->y))
	      close = 0;

	    else 
	      if ((DD_IsVert3D(vinput->type)) &&
		  (((ddCoord4D *)first_pt)->x == ((ddCoord4D *)last_pt)->x) &&
		  (((ddCoord4D *)first_pt)->y == ((ddCoord4D *)last_pt)->y) &&
		  (((ddCoord4D *)first_pt)->z == ((ddCoord4D *)last_pt)->z))
	      close = 0;

	    else 
	      if ((((ddCoord4D *)first_pt)->x == ((ddCoord4D *)last_pt)->x) &&
		  (((ddCoord4D *)first_pt)->y == ((ddCoord4D *)last_pt)->y) &&
		  (((ddCoord4D *)first_pt)->z == ((ddCoord4D *)last_pt)->z) &&
		  (((ddCoord4D *)first_pt)->w == ((ddCoord4D *)last_pt)->w))
	      close = 0;

	  } else {

	    if ((DD_IsVert2D(vinput->type)) &&
		(((ddCoord3DS *)first_pt)->x == ((ddCoord3DS *)last_pt)->x) &&
		(((ddCoord3DS *)first_pt)->y == ((ddCoord3DS *)last_pt)->y))
	      close = 0;

	    else 
	    if ((((ddCoord3DS *)first_pt)->x == ((ddCoord3DS *)last_pt)->x) &&
		(((ddCoord3DS *)first_pt)->y == ((ddCoord3DS *)last_pt)->y) &&
		(((ddCoord3DS *)first_pt)->z == ((ddCoord3DS *)last_pt)->z))
	      close = 0;

	  }


	  /*
	   * if close is set then need to close the bound.
	   * Copy the first point to one poast the last point
	   * in the bound.
	   */
	  if (close) {
	       
             /* Insure sufficient room for each vertex */
             MI_ALLOCLISTOFDDPOINT(pddlist, vert_count+1, point_size);
	     if (!pddlist->pts.p2DSpt) return(BadAlloc);

	     /* Insure realloc didn't move array */
	     first_pt = (char *)pddlist->pts.p4Dpt;
	     last_pt =  ((char *)pddlist->pts.p4Dpt) 
				+ (vert_count * point_size);

	     /* copy first point to end of list */
	     /* JSH - assuming copy may overlap */
	     memmove( last_pt, first_pt, point_size);

	     /* Increment point count */
	     pddlist->numPoints += 1;
	  }

	  /* Now, ski to next input list */
          pddlist++;
    }
    return (Success);
}

/*++
 |
 |  Complete_FillArea_Facetlist(pddc, input_vert, input_fct, output_fct)
 |
 |	Create an output facet list with facet color and normal using
 |	proper precedence rules.
 |
 --*/
static
ddpex3rtn
Complete_FillArea_Facetlist(pddc, input_vert, input_fct, output_fct)
    miDDContext		*pddc;
    miListHeader	*input_vert;    /* input vertex data */
    listofddFacet	*input_fct;     /* input facet data */
    listofddFacet	**output_fct;   /* output facet data */
{

    listofddFacet	*fct_list;
    ddRgbFloatNormal	*out_fct;
    listofddPoint	*pddlist;
    char		*in_pt;
    ddFacetUnion	in_fct;
    int			point_size, color_offset, normal_offset;
    int			numPoints;
    ddCoord3D		*vert1, *vert2, *vert3;
    int			i,j;
    int			point_count;
    float		length;
    char		have_colors, have_normals, done;

    have_colors = have_normals = 0;

    /* What data must be added to output facet list ? */
    if ((input_fct) && (input_fct->type != DD_FACET_NONE)) {
      in_fct.pNoFacet = input_fct->facets.pNoFacet;
      if DD_IsFacetNormal(input_fct->type) have_normals = ~0;
      if DD_IsFacetColour(input_fct->type) have_colors = ~0;
    }

    if ((have_colors) && (have_normals)) { /* Why are we here? */
     *output_fct = input_fct;
     return(Success);
    }

    /*
     * Allocate storage for the facet list
     */
    fct_list = MI_NEXTTEMPFACETLIST(pddc);
    MI_ALLOCLISTOFDDFACET(fct_list, 1, sizeof(ddRgbFloatNormal));
    if (!fct_list->facets.pFacetRgbFloatN) return(BadAlloc);
    out_fct = (ddRgbFloatNormal *)fct_list->facets.pFacetRgbFloatN;

    DD_VertPointSize(input_vert->type, point_size);

    /* 
     * Compute "intrinsic" color of facet.
     * There is a "hierarchy" of sources for a facet's intrinsic
     * color:
     *		vertex_color present?  facet_color = vertex_color
     * 		else facet_color present? facet_color = facet_color
     *		else facet_color = PC_surface_color
     * Obviously there is no pre-defined facet color in this routine,
     * so either use vertex color or surface color from PC.
     */

    if (DD_IsVertColour(input_vert->type)) {
      /* Compute average facet color */
      point_count = 0;
      pddlist = input_vert->ddList;
      out_fct->colour.red=out_fct->colour.green=out_fct->colour.blue=0.0;
      for (i = 0; i < input_vert->numLists; i++, pddlist++) {
	in_pt = pddlist->pts.ptr;
	for (j = 0; j < input_vert->numLists; 
	     j++, in_pt += point_size, point_count++) {
	  out_fct->colour.red += ((ddRgbFloatPoint4D *)in_pt)->colour.red;
	  out_fct->colour.green += ((ddRgbFloatPoint4D *)in_pt)->colour.green;
	  out_fct->colour.blue += ((ddRgbFloatPoint4D *)in_pt)->colour.blue;
	}
      }
      out_fct->colour.red /= point_count;
      out_fct->colour.green /= point_count;
      out_fct->colour.blue /= point_count;

    } else if (have_colors) {
      out_fct->colour = *in_fct.pFacetRgbFloat;

    } else {
      /* use front face colors. This needs to get generalized
         to deal with back-facing attributes*/
      out_fct->colour = pddc->Static.attrs->surfaceColour.colour.rgbFloat;
    }

    if (!(have_normals)) {

      done = PEXOff;
      pddlist = input_vert->ddList;

      for (j = 0; ((j < input_vert->numLists) && !(done)); j++) {

	/* Don't process if insufficient number of points */
	if ((numPoints = pddlist->numPoints) > 2) {

	  in_pt = pddlist->pts.ptr;

	  /* 
	   * Compute surface normal.
	   * The Surface normal is the cross product of the first
	   * three non-colinear points.
	   */

	  vert1 = ((ddCoord3D *)in_pt);
	  in_pt += point_size;
	  vert2 = ((ddCoord3D *)in_pt);
	  in_pt += point_size;
	  vert3 = ((ddCoord3D *)in_pt);
	  in_pt += point_size;

	  numPoints -= 3;
	
	  CROSS_PRODUCT(vert1, vert2, vert3, &(out_fct->normal));
	  NORMALIZE_VECTOR(&(out_fct->normal), length);

	  while (NEAR_ZERO(length) && (numPoints > 0)) {
	    vert1 = vert2;
	    vert2 = vert3;
	    vert3 = ((ddCoord3D *)in_pt);
	    in_pt += point_size;
	    CROSS_PRODUCT(vert1, vert2, vert3, &(out_fct->normal));
	    NORMALIZE_VECTOR(&(out_fct->normal), length);
	    numPoints--;
	  }
	  if (!(NEAR_ZERO(length))) done = PEXOn;	/* :-) */

	}
	pddlist++;
      }

    } else {
      /* use input facet normals */
      out_fct->normal = *in_fct.pFacetN;
    }

    fct_list->type = DD_FACET_RGBFLOAT_NORM;
    fct_list->numFacets = 1;

    *output_fct = fct_list;

    return(Success);
}

/*++
 |
 |  Calculate_FillArea_Facet_Normal
 |
 |	Add facet normals to a facet list.
 |
 --*/
static ddpex3rtn
Calculate_FillArea_Facet_Normal(pddc, input_vert, input_fct, output_fct)
    miDDContext		*pddc;
    miListHeader	*input_vert;    /* input vertex data */
    listofddFacet	*input_fct;	/* input facet data */
    listofddFacet	**output_fct;   /* output facet data */
{

    listofddFacet	*fct_list;
    ddRgbFloatColour	*in_fct;
    ddFacetUnion	out_fct;
    listofddPoint	*pddlist;
    char		*in_pt;
    ddVector3D		normal;
    int			point_size;
    int			numPoints;
    ddCoord3D		*vert1, *vert2, *vert3;
    int			numfacets;
    int			i, j;
    float		length;
    char		done;

    /* Some quick error checking */
    if ((input_fct) && (DD_IsFacetNormal(input_fct->type))) return(Success);

    /*
     * Allocate storage for the output facet list
     */
    fct_list = MI_NEXTTEMPFACETLIST(pddc);
    if ((input_fct) && DD_IsFacetColour(input_fct->type)) {
      in_fct = input_fct->facets.pFacetRgbFloat;
      fct_list->type = DD_FACET_RGBFLOAT_NORM;
      numfacets = 1;
      MI_ALLOCLISTOFDDFACET(fct_list, 1, sizeof(ddRgbFloatNormal));
    } else {
      in_fct = 0;
      fct_list->type = DD_FACET_NORM;
      numfacets = 1;
      MI_ALLOCLISTOFDDFACET(fct_list, 1, sizeof(ddVector3D));
    }

    fct_list->numFacets = numfacets;

    if (!fct_list->facets.pNoFacet) return(BadAlloc);
    out_fct = fct_list->facets;

    DD_VertPointSize(input_vert->type, point_size);

    pddlist = input_vert->ddList;


    /* Copy the input facet color */
    if (in_fct) {
      *out_fct.pFacetRgbFloat = *in_fct;
      in_fct++;
    }

    done = PEXOff;

    for(i = 0; ((i < input_vert->numLists) && (!(done))); i++) {


      if ((numPoints = pddlist->numPoints) > 2) {

        in_pt = pddlist->pts.ptr;

	/* 
	 * Compute surface normal.
	 * The Surface normal is the cross product of the first
	 * three non-colinear points.
	 */
	numPoints -= 3;
	vert1 = ((ddCoord3D *)in_pt);
	in_pt += point_size;
	vert2 = ((ddCoord3D *)in_pt);
	in_pt += point_size;
	vert3 = ((ddCoord3D *)in_pt);
	in_pt += point_size;
	CROSS_PRODUCT(vert1, vert2, vert3, &normal);
	NORMALIZE_VECTOR(&normal, length);

	while (NEAR_ZERO(length) && (--numPoints >= 0)) {
	  vert1 = vert2;
	  vert2 = vert3;
	  vert3 = ((ddCoord3D *)in_pt);
	  in_pt += point_size;
	  CROSS_PRODUCT(vert1, vert2, vert3, &normal);
	  NORMALIZE_VECTOR(&normal, length);
	}

	/* Initialize to some arbitrary value if degenerate */
	if (!NEAR_ZERO(length))  {
	  done = PEXOn;

          if (in_fct) (out_fct.pFacetRgbFloatN++)->normal = normal;
          else *(out_fct.pFacetN++) = normal;
	}
      }

      pddlist++;
    }

    *output_fct = fct_list;

    return(Success);
}

/*++
 |
 |  Calculate_FillArea_Vertex_Color_and_Normal
 |
 |	Add vertex normals and colors to a vertex list.
 |
 --*/
static ddpex3rtn
Calculate_FillArea_Vertex_Color_and_Normal(pddc, input_vert, input_fct,
					   output_vert)
    miDDContext		*pddc;
    miListHeader	*input_vert;    /* input vertex data */
    listofddFacet	*input_fct;	/* input facet data */
    miListHeader	**output_vert;  /* output vertex data */
{
    miListHeader		*out_vert;
    listofddFacet		*fct_list;
    ddRgbFloatNormal		*out_fct;
    listofddPoint		*pddilist;
    listofddPoint		*pddolist;
    ddRgbFloatNormalPoint4D	*out_pt;
    ddPointUnion		in_pt;
    ddFacetUnion		in_fct;
    int				point_size, facet_size;
    int				numFacets=0;
    int				numPoints;
    int				i,j;
    char			done;

    /* Some quick error checking */
    if ((DD_IsVertNormal(input_vert->type)) && 
	(DD_IsVertColour(input_vert->type)))
      return(Success);

    /* Use one of the pre-defined 4D list for output */
    *output_vert = out_vert = MI_NEXTTEMPDATALIST(pddc);

    /* Insure sufficient room for each header */
    MI_ALLOCLISTHEADER(out_vert, input_vert->numLists)
    if (!out_vert->ddList) return(BadAlloc);

    out_vert->type = DD_RGBFLOAT_NORM_POINT4D;
    out_vert->numLists = input_vert->numLists;
    out_vert->flags =  input_vert->flags;

    pddilist = input_vert->ddList;
    pddolist = out_vert->ddList;

    DD_VertPointSize(input_vert->type, point_size);

    /* Compute facet normals if no per-vertex normals with data */
    if (!(DD_IsVertNormal(input_vert->type)))  {

      if (!(input_fct)) {
        if (i = Calculate_FillArea_Facet_Normal(pddc, input_vert,
                                              input_fct, &fct_list))
          return(i);
        input_fct = fct_list;

      }  
 
      else if (!(DD_IsFacetNormal(input_fct->type))) {
        if (i = Calculate_FillArea_Facet_Normal(pddc, input_vert, 
					      input_fct, &fct_list))
	  return(i);
        input_fct = fct_list;
      }
    }

    if ((input_fct) && (input_fct->numFacets > 0))
      in_fct = input_fct->facets;
    else in_fct.pNoFacet = 0;

    DDFacetSIZE(input_fct->type, facet_size);

    done = PEXOff;

    for (i = 0; i < input_vert->numLists; i++) {

      pddolist->numPoints = pddilist->numPoints;

      MI_ALLOCLISTOFDDPOINT(pddolist,(pddilist->numPoints+1),
			    sizeof(ddRgbFloatNormalPoint4D));
      if (!(out_pt = pddolist->pts.pRgbFloatNpt4D)) return(BadAlloc);
      in_pt = pddilist->pts;

      for (j = 0; j < pddilist->numPoints; j++)
        {
	 /* First copy over coordinate data */
	 out_pt->pt = *in_pt.p4Dpt;

	 /* 
	  * Next color 
	  * Colour is derived first from the vertex, second from the
	  * facet, and third from the current PC attributes.
	  */
	 if (DD_IsVertColour(input_vert->type))
	   out_pt->colour = in_pt.pRgbFloatpt4D->colour;
	 else if ((in_fct.pNoFacet) && (DD_IsFacetColour(input_fct->type)))
	   out_pt->colour = *in_fct.pFacetRgbFloat;
	 else 
	   out_pt->colour = pddc->Static.attrs->surfaceColour.colour.rgbFloat;

	 /* 
	  * Next normals 
	  * Colour is derived first from the vertex, second from the
	  * facet (note that we insured above that there were facet normals).
	  */
	 if (DD_IsVertNormal(input_vert->type))
	   out_pt->normal = in_pt.pNpt4D->normal;
	 else if (DD_IsFacetColour(input_fct->type))
	     out_pt->normal = in_fct.pFacetRgbFloatN->normal;
	 else out_pt->normal = *in_fct.pFacetN;

	 out_pt++;
	 in_pt.ptr += point_size;
        }

        pddilist++;
        pddolist++;
      }

    return(Success);
}


 
/*++
 |
 |  DepthCueFillArea(pRend, input_vert, input_fct, output_vert)
 |
 |  Applies depth cueing to the vertex colors in a FillArea data list
 |
 --*/
ddpex3rtn
miDepthCueFillArea(pRend, input_vert, input_fct, output_vert)
    ddRendererPtr       pRend;          /* renderer handle */
    miListHeader        *input_vert;    /* input vertex data */
    listofddFacet       *input_fct;     /* input facet data */
    miListHeader        **output_vert;  /* output vertex data */
{
    miDDContext         *pddc = (miDDContext *)(pRend->pDDContext);
    miListHeader                *out_vert;
    listofddFacet               *fct_list;
    ddRgbFloatNormal            *out_fct;
    listofddPoint               *pddilist;
    listofddPoint               *pddolist;
    ddPointUnion                in_pt, out_pt;
    ddRgbFloatColour            *in_color;
    ddFacetUnion                in_fct;
    int                         point_size, facet_size;
    int                         numFacets=0;
    int                         numPoints;
    int                         i,j,outpoint_size;
    miColourEntry               *pintcolour;
    ddFLOAT                     pt_depth;
    ddULONG                     colourindex;
    ddColourSpecifier           intcolour;
    ddUSHORT                   status;
    ddDepthCueEntry             *dcue_entry;

    /* look for empty list header */
    if (input_vert->numLists == 0) return(Success);

    /* validate CC version of Depth Cue information */
    if (pddc->Static.misc.flags & CC_DCUEVERSION)
	Compute_CC_Dcue(pRend, pddc);

    /* check to make sure depth cuing is enabled */
    if (pddc->Static.misc.cc_dcue_entry.mode == PEXOff) {
	*output_vert = input_vert;
	return(Success);
    }

    /* Else, depth cue! Use one of the pre-defined 4D list for output */
    *output_vert = out_vert = MI_NEXTTEMPDATALIST(pddc);

    /* Insure sufficient room for each header */
    MI_ALLOCLISTHEADER(out_vert, input_vert->numLists)
    if (!out_vert->ddList) return(BadAlloc);

    out_vert->type = input_vert->type;
    DD_SetVertRGBFLOAT(out_vert->type);
    out_vert->numLists = input_vert->numLists;
    out_vert->flags =  input_vert->flags;

    pddilist = input_vert->ddList;
    pddolist = out_vert->ddList;
    DD_VertPointSize(input_vert->type, point_size);
 
    if ((input_fct) && (input_fct->numFacets > 0)){
      in_fct = input_fct->facets;
      DDFacetSIZE(input_fct->type, facet_size);
    }
    else in_fct.pNoFacet = 0;
 
    /* Get current surface color if appropriate */
    if (!(DD_IsVertColour(input_vert->type)) &&
                (pddc->Static.attrs->surfaceColour.colourType
                        == PEXIndexedColour)) {
      if ((InquireLUTEntryAddress (PEXColourLUT, pRend->lut[PEXColourLUT],
             pddc->Static.attrs->surfaceColour.colour.indexed.index,
             &status, (ddPointer *)&pintcolour)) == PEXLookupTableError)
          return (PEXLookupTableError);
    }

    DD_VertPointSize(out_vert->type, outpoint_size);

    for (i = 0; i < input_vert->numLists; i++) {

      pddolist->numPoints = pddilist->numPoints;

      MI_ALLOCLISTOFDDPOINT(pddolist,(pddilist->numPoints+1),
                            outpoint_size);
      if (!(out_pt.ptr = pddolist->pts.ptr)) return(BadAlloc);
      in_pt = pddilist->pts;

      for (j = 0; j < pddilist->numPoints; j++) {
        /* First copy over coordinate data */
        pt_depth = in_pt.p4Dpt->z;
        *out_pt.p4Dpt = *in_pt.p4Dpt;
        in_pt.p4Dpt++;
        out_pt.p4Dpt++;

        /*
         * Next color
         * Colour is derived first from the vertex, second from the
         * facet, and third from the current PC attributes.
         */
 
        if (DD_IsVertColour(input_vert->type)){
          in_color = in_pt.pRgbFloatClr;
          in_pt.pRgbFloatClr++;
        }
        else {
          if ((in_fct.pNoFacet) && (DD_IsFacetColour(input_fct->type)))
            in_color = in_fct.pFacetRgbFloat;
          else {
            if (pddc->Static.attrs->surfaceColour.colourType
                        == PEXIndexedColour)
              in_color = &pintcolour->entry.colour.rgbFloat;
            else in_color =
                &(pddc->Static.attrs->surfaceColour.colour.rgbFloat);
          }
        }

        APPLY_DEPTH_CUEING(pddc->Static.misc.cc_dcue_entry, 
			pt_depth, in_color, out_pt.pRgbFloatClr)

        out_pt.pRgbFloatClr++;

         /*
          * Next normals
          * Colour is derived first from the vertex, second from the
          * facet (note that we insured above that there were facet normals).
          */

        if DD_IsVertNormal(input_vert->type) {
          *out_pt.pNormal = *in_pt.pNormal;
          in_pt.pNormal++;
          out_pt.pNormal++;
        }

        /* Next pass along edge info if there is any */
        if (DD_IsVertEdge(out_vert->type)) {
          *out_pt.pEdge = *in_pt.pEdge;
          in_pt.pEdge++;
          out_pt.pEdge++;
        }


      }

      pddilist++;
      pddolist++;
    }

    return(Success);
}
