/* $Xorg: miTriStrip.c,v 1.4 2001/02/09 02:04:11 xorgcvs Exp $ */
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
supporting documentation, and that the name of Sun Microsystems
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
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/level2/miTriStrip.c,v 3.7 2001/12/14 19:57:30 dawes Exp $ */

#include "miClip.h"
#include "misc.h"
#include "miscstruct.h"
#include "ddpex3.h"
#include "PEXErr.h"
#include "miStruct.h"
#include "PEXprotost.h"
#include "miRender.h"
#include "gcstruct.h"
#include "miLight.h"
#include "ddpex2.h"
#include "pexos.h"


static ddpex3rtn	miClipTriStrip();
static ddpex3rtn	miCullTriStrip();
static ddpex3rtn	miDepthCueTriStrip();
static ddpex3rtn	Complete_TriFacetList();
static ddpex3rtn	Calculate_TriStrip_Facet_Normal();
static ddpex3rtn	Calculate_TriStrip_Vertex_Color_and_Normal();
static ddpex3rtn	Breakup_TriStrip();

/*++
 |
 |  Function Name:	miTriangleStrip
 |
 |  Function Description:
 |	 Handles the triangle strip OC.
 |
 |  Note(s):
 |
 --*/

ddpex3rtn
miTriangleStrip(pRend, pExecuteOC)
/* in */
    ddRendererPtr       pRend;	  /* renderer handle */
    miGenericStr       *pExecuteOC;
{

/* calls */
      ddpex3rtn         miTransform();
      ddpex3rtn		miConvertVertexColors();
      ddpex3rtn		miConvertFacetColors();
      ddpex3rtn         miLightTriStrip();
      ddpex3rtn         miRenderTriStrip();
      ddpex3rtn         ComputeMCVolume();

/* Local variable definitions */
      miTriangleStripStruct	*ddTri 
	                           = (miTriangleStripStruct *)(pExecuteOC+1);
      miListHeader      *input_list = &(ddTri->points);	   /* Input points */
      listofddFacet     *input_facet = ddTri->pFacets;     /* facets */
      miDDContext       *pddc = (miDDContext *)(pRend->pDDContext);
      miListHeader      *color_list,
			*mc_list,
			*wc_list,
                        *mc_clist,
                        *light_list,
                        *cc_list,
                        *clip_list,
                        *cull_list,
			*dcue_list,
                        *dc_list;

      listofddFacet     *color_facet,
			*mc_facet,
			*wc_facet,
                        *light_facet,
                        *cc_facet,
                        *clip_facet,
                        *cull_facet,
                        *dc_facet;
      listofddPoint     *sp;
      int               i, j;
      ddpex3rtn         status;
      ddPointType       out_type;
      ddUSHORT		clip_mode;	
 
   
      /* 
       * First, check data input
       * At this point, the data is all in one list
       */

      if (input_list->numLists == 0) return(Success);

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


        if (status = miClipTriStrip(pddc, mc_clist, color_facet, 
				    &mc_list, &mc_facet, clip_mode))
            return (status);

      /* if nothing left after model clip, return early */
      if (mc_list->numLists <= 0) return(Success);

      } else {
	mc_list = color_list;
	mc_facet = color_facet;
      }
 
      /* Note - After clipping the triangle strip may have
       * decomposed into a number of separate triangle strips
       */
 	

      /* 
       * Next, check lighting requirements
       */

      if (pddc->Static.attrs->reflModel != PEXReflectionNoShading) {
        
        /* Transform to WC prior to applying lighting */
        out_type = mc_list->type;
	if (DD_IsVertNormal(out_type)) VALIDATEINVTRMCTOWCXFRM(pddc);
        if (status = miTransform(pddc,
                                 mc_list, &wc_list,
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
        if (status = miLightTriStrip(pRend, pddc,
                                     wc_list, wc_facet,
                                     &light_list, &light_facet))
          return (status);


        /* Transform to CC for clipping */
	if (DD_IsVertNormal(light_list->type)) VALIDATEINVTRWCTOCCXFRM(pddc);
        if (status = miTransform(pddc,
                                 light_list, &cc_list,
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

      } else {
 
        /* PEXReflectionNoShading case */

        /* Now transform points and facets normals to CC */
        out_type = mc_list->type;
	if (DD_IsVertNormal(out_type)) VALIDATEINVTRMCTOCCXFRM(pddc);
        if (status = miTransform(pddc,
                                 mc_list, &cc_list,
                                 pddc->Dynamic->mc_to_cc_xform,
                                 pddc->Static.misc.inv_tr_mc_to_cc_xform,
                                 DD_SetVert4D(out_type)))
          return (status);

        if ( (mc_facet) &&
             (mc_facet->numFacets > 0) &&
             (DD_IsFacetNormal(mc_facet->type)) ) {
	  VALIDATEINVTRMCTOCCXFRM(pddc);
          if (status = miFacetTransform(pddc,
                                      mc_facet, &cc_facet,
                                      pddc->Static.misc.inv_tr_mc_to_cc_xform))
          return (status);
	} else cc_facet = mc_facet;

      }

      /* View clip primitive */
      clip_mode = MI_VCLIP;
      if (status = miClipTriStrip(pddc, cc_list, cc_facet,
              &clip_list, &clip_facet, clip_mode))
	return (status);

      /* if nothing left after view clip, return early */
      if (clip_list->numLists <= 0) return(Success);

      /* Note - After View clipping, the triangle strip may have
       * decomposed into a number of separate triangle strips
       */

      if (pddc->Dynamic->pPCAttr->cullMode) {
      /* Now cull according to current culling mode */
      if (status = miCullTriStrip(pddc, clip_list, clip_facet,
                                    &cull_list, &cull_facet))
          return (status);

      /* if nothing left after culling, return early */
      if (cull_list->numLists <= 0) return(Success);
      } else {
	cull_list = clip_list;
	cull_facet = clip_facet;
      }

      /* DEPTH CUEING */
      if (pddc->Dynamic->pPCAttr->depthCueIndex) {
        miDepthCueTriStrip(pRend, cull_list, cull_facet, &dcue_list);
        cull_list = dcue_list;
      }

      /* Lastly, transform to DC coordinates */
      out_type = cull_list->type;
      DD_SetVert2D(out_type);
      DD_SetVertShort(out_type);
      if (DD_IsVertNormal(out_type)) VALIDATEINVTRCCTODCXFRM(pddc);
      if (status = miTransform(pddc,
			       cull_list, &dc_list,
			       pddc->Dynamic->cc_to_dc_xform,
			       pddc->Static.misc.inv_tr_cc_to_dc_xform,
			       out_type) )
          return (status);
 
      /* Transform facet normals if necessary */
      if ( (cull_facet) &&
           (cull_facet->numFacets > 0) &&
           (DD_IsFacetNormal(cull_facet->type)) ) {
	VALIDATEINVTRCCTODCXFRM(pddc);
        if (status = miFacetTransform(pddc,
                               cull_facet, &dc_facet,
                               pddc->Static.misc.inv_tr_cc_to_dc_xform))
            return (status);
      } else dc_facet = cull_facet;

    return (pddc->Static.RenderProcs[TRISTRIP_RENDER_TABLE_INDEX](pRend,
                                                                  pddc,
                                                                  dc_list,
                                                                  dc_facet));
}
/*************************************************************************/

/*++
 |
 |  Function Name:	miClipTriStrip
 |
 |  Function Description:
 |	 Clips a triangle strip. Note that this routine
 |	 will return a triangle strip, though it sometimes creates
 |	 "degenerate" (ie colinear vertices) triangles to achieve
 |	 this result. At this point "hollow" filled triangles will
 |	 exhibit clipping artifacts, (degenerate triangles) whereas 
 |	 "empty" fill with edge visibility enabled will not.
 |
 |  Note(s):
 |
 |	 This routine uses a Sutherland-Hodgman approach for
 |	 polygon clipping. (See, for example, Rodger's "Procedural
 |	 Elements for Computer Graphics", pp 169-179).
 |	 Each triangle strip is clipped successively against each (enabled) 
 |	 clipping boundary. 
 |
 --*/

static
ddpex3rtn
miClipTriStrip(pddc, input_vert, input_fct, output_vert, output_fct,clip_mode)
/* in */
	miDDContext	*pddc;
	miListHeader    *input_vert;	/* input vertex data */
	listofddFacet	*input_fct;	/* input facet data */
	miListHeader    **output_vert;	/* output vertex data */
	listofddFacet	**output_fct;	/* output facet data */
	ddUSHORT	clip_mode;
{
/* uses */
    ddPointUnion	in_ptP, in_ptQ, in_ptR;
    ddPointUnion	out_pt;
    float		t_ptP, t_ptQ, t_ptR;
    char		*in_fct, *out_fct;
    miListHeader	*vinput, *voutput, *list1, *list2;
    listofddFacet	*finput, *foutput, *fct_list1, *fct_list2;
    listofddPoint	*pddilist;
    listofddPoint	*pddolist;
    int			point_size;
    int			facet_size;
    int			pts_in_list;
    int			vert_count;
    int			new_facets;
    int			clip_plane, j, k, out_listnum;
    ddUSHORT		clipflags;
    ddUSHORT		current_clip;
    int			edge_offset;
    ddHalfSpace		*MC_HSpace;
    ddVector3D		PdotN, QdotN;
    int			clip_code;
    ddULONG		*edge_ptr;
    char		new_list;
    ddpex3rtn		status;
    int			num_planes;

    /* Vertex data must be homogeneous and contain more than two points
     *	for clipper 
     */
    if (!(DD_IsVert4D(input_vert->type)) 
	|| (input_vert->numLists == 0)) return(1);

    /* 
     * Use the pre-defined clip lists for output 
     */

    /* 
     * Must have edge flags in vertex if edges are to be drawn - 
     * otherwise, cannot determine wich edges are "original" and
     * which edges where added by the clipper. 
     */
    if ((pddc->Static.attrs->edges != PEXOff) &&
	(!(DD_IsVertEdge(input_vert->type))))
    {
      if (status = miAddEdgeFlag(pddc, input_vert, &list1)) return(status);
      vinput = list1;

    } else {
      /* Allocate an initial number of headers */
      list1 = MI_NEXTTEMPDATALIST(pddc);
      MI_ALLOCLISTHEADER(list1, 
			MI_ROUND_LISTHEADERCOUNT(input_vert->numLists));
      if (!list1->ddList) return(BadAlloc);
      list1->type = input_vert->type;
      list1->flags =  input_vert->flags;
      vinput = input_vert; /* Initially set to input list */
    }

    list2 = MI_NEXTTEMPDATALIST(pddc);
    MI_ALLOCLISTHEADER(list2, 
			MI_ROUND_LISTHEADERCOUNT(vinput->numLists));
    if (!list2->ddList) return(BadAlloc);
    list2->type = vinput->type;
    list2->flags =  vinput->flags;
    voutput = list2;

    /*
     * Initialize an output facet list. Note that facet lists are only
     * maintained if an initial one is supplied.
     * Note that twice the number of input facets is an upper bound,
     * although there is probably a tighter "fit".
     */
    if ((input_fct) && (input_fct->numFacets)) {
      finput = input_fct;
      DDFacetSIZE(input_fct->type, facet_size);
      fct_list1 = MI_NEXTTEMPFACETLIST(pddc);
      fct_list1->type = input_fct->type;
      MI_ALLOCLISTOFDDFACET(fct_list1, 3*input_fct->numFacets, facet_size);
      if (!fct_list1->facets.pFacetRgbFloatN) 
	return(BadAlloc);
      foutput = fct_list1;

      fct_list2 = MI_NEXTTEMPFACETLIST(pddc);
      fct_list2->type = input_fct->type;
      MI_ALLOCLISTOFDDFACET(fct_list2, 3*input_fct->numFacets, facet_size);
      if (!fct_list2->facets.pFacetRgbFloatN) 
	return(BadAlloc);
    } else 
      finput = foutput = fct_list1 = fct_list2 = 0;

    /* 
     * Get point size so that this works for all point types 
     */
    DD_VertPointSize(vinput->type, point_size);
    if DD_IsVertEdge(vinput->type)
    		DD_VertOffsetEdge(vinput->type, edge_offset);

    /* 
     * Each list is now clipped in turn against each (enabled) boundary.
     */

    if (clip_mode == MI_MCLIP) 
		num_planes = pddc->Static.misc.ms_MCV->numObj;
    else num_planes = 6;

    if (clip_mode == MI_MCLIP) 
	MC_HSpace = (ddHalfSpace *)(pddc->Static.misc.ms_MCV->pList);

    for (clip_plane = 0; clip_plane < num_planes; clip_plane++) {

      /* do entire list against one clipping plane at a time */
      current_clip = 1 << clip_plane;  /* current_clip is new clip bound */

	/*
	 * Triangle strips begin with only one input and output 
	 * vertex list. If a facet is found to be entirely out 
	 * of bounds, A new list is started at the new included 
	 * point.  This new series of lists is clipped in turn
	 * against the next clipping planei, and may be decomposed into
	 * yet more lists. However, only a single facet list is 
	 * maintained, at it is assumed that a one<->one correspondence 
	 * exists between vertex triads and facets (within a single list)
	 * even if degenerate facets are added.
	 */

        if (finput) {
	  in_fct = (char *)(finput->facets.pNoFacet);
          MI_ALLOCLISTOFDDFACET(foutput, 
			(3*finput->numFacets), facet_size);
	  out_fct = (char *)(foutput->facets.pNoFacet); 
        }

        for (j = 0, out_listnum = 0, new_list = 1, new_facets = 0,
	     pddilist = vinput->ddList, pddolist = voutput->ddList,
	     voutput->numLists = 0; 
	     j < vinput->numLists; j++) {


          /* Don't process if not enough points */
          if ((vert_count = pddilist->numPoints) < 3) {
            pddilist++;
            continue;
          }


          /* Insure sufficient room for vertecies and degenerate points 
           * Note that twice the vertex count is an upper-bound for
           * a clipped triangle(although there is probably a "tighter fit").
           */
	  
          MI_ALLOCLISTOFDDPOINT(pddolist, 2*vert_count, point_size);
          out_pt = pddolist->pts;
          if (!out_pt.ptr) return(BadAlloc);

	  clip_code = 0;		
					/* Aquire first three points */
					/* and generate clip code */
          in_ptP.ptr = pddilist->pts.ptr;	
	  COMPUTE_CLIP_PARAMS(in_ptP,t_ptP,0,clip_mode,
			current_clip,MC_HSpace,clip_code);
          in_ptQ.ptr = in_ptP.ptr + point_size;
	  COMPUTE_CLIP_PARAMS(in_ptQ, t_ptQ, 1, clip_mode,
			current_clip,MC_HSpace,clip_code);
          in_ptR.ptr = in_ptQ.ptr + point_size;
	  /* 
	   * Initialize the output array. The output array is expected
	   * to always contain the first two points, so load them - 
	   * clipped if necessary.
	   */
	  switch(clip_code)
	   {
	      case 0:			/* both P and Q are in bounds */
		COPY_POINT(in_ptP, out_pt, point_size);
		out_pt.ptr += point_size;
		COPY_POINT(in_ptQ, out_pt, point_size);
		out_pt.ptr += point_size;
		pts_in_list = 2;
		break;
	      case 1:			/* P is out, Q is in */ 
		CLIP_AND_COPY(vinput->type, in_ptP, t_ptP, 
				in_ptQ, t_ptQ, out_pt);
		out_pt.ptr += point_size;
		COPY_POINT(in_ptQ, out_pt, point_size);
		out_pt.ptr += point_size;
		pts_in_list = 2;
		break;
	      case 2:			/* P is in, Q is out */
		COPY_POINT(in_ptP, out_pt, point_size);
		out_pt.ptr += point_size;
		CLIP_AND_COPY(vinput->type, in_ptQ, t_ptQ, 
				in_ptP, t_ptP, out_pt);
		out_pt.ptr += point_size;
		pts_in_list = 2;
		break;
	      case 3:			/* both are out */
		pts_in_list = 0;
		break;
	   }
	  
          /* 
	   * For each vertex, clip a triangle. 
	   * Each case assumes that points P & Q (the first and
	   * second points in the triangle, pointed to in the input
	   * list by in_ptR & in_ptQ) are already copied
	   * into the output array (clipped, if necessary and except for
	   * case 3), while R will be copied into the output array (pointed 
	   * to by out_pt) if necessary during execution of the case 
	   * statement. 
	   *
	   * It is important to note that if a facet is completely
	   * rejected, a new list is formed.  Also, to maintain
	   * the correct "sense" of the normals, if the new
	   * list is about to begin on an even facet #, a
	   * degenerate facet is placed at the beginning of the
	   * new list.
           *
	   * Also, note that if additional facets are generated
	   * (cases 1,2,3,4,& 6) the artifact edge flag is set to zero.
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
	   *	(forward edge)
	   * bit 1 indicates presence of an edge from 3 -> 1  
	   *	(backward edge)
	   *
	   * Also note that clipping in PHIGS does NOT
	   * display clipped edges along the boundaries, whereas fill
	   * area attribute HOLLOW does.  This implementation does
	   * not handle the HOLLOW fill style properly in that respect. 
	   * One possible solution is to use additional bits in the
	   * edge flag field.
	   */

          for (k = 2; k < vert_count; k++) {
	    /* 
	     * Clip the new triangle 
	     * There are 8 possible ways that the triangle (P, Q, R) can
	     * be intersected by a clipping plane:
	     *
	     *	P, Q, R in  	P, Q, R out
	     *	P in Q, R out  	P, Q in R out	P, R in, Q out
	     *	Q in P, R out  	Q, R in P out
	     *	R in P, Q out
	     *
	     * Each of these cases are handled separately.  
	     */

	    new_list = 0;
	    COMPUTE_CLIP_PARAMS(in_ptR, t_ptR, 2, clip_mode,
			current_clip,MC_HSpace,clip_code);

	    switch(clip_code)
	     {
	      /* 
	       * Case 0 - trivial accept P, Q, and R
	       * Proceed to next triangle.
	       */
	      case 0:
		COPY_POINT(in_ptR, out_pt, point_size);
		out_pt.ptr += point_size;
		pts_in_list++;
		if (finput) {
		  COPY_FACET(in_fct, out_fct, facet_size);
		  out_fct += facet_size;
		  ++new_facets;
		}
		break;

	      /* 
	       * Case 1 - P is outside, Q & R are inside clip zone.
	       * This is a complex case which results in the creation
	       * of several new triangles including a "degenerate" triangle 
	       * (two identical endpoints). Point "A" and point "Q" are
	       * already in the output list. Output vertex list is A-Q-B-Q-R 
	       *
               *       |Q+                    |Q+             
               *       | /\                   | /\            
               *       |/  \                  |/  \           
               *       |    \     ===>       A+    \     
               *      /|     \                |     \         
	       *    P+-|------+R             B+------+R       
	       *       |                      |
	       *      triangle PQR        triangles AQB, QBQ(degenerate)
	       *						& BQR
	       *
	       */
	      case 1:
					/* If edges are visible,
					 * we wish to disable the edge from
					 * Q to B. This edge is specified
					 * by the forward edge of the first Q,
					 * the forward edge of B, and the 
					 * backward edge of the second Q 
					 */
		if (pddc->Static.attrs->edges != PEXOff) {
		  out_pt.ptr -= point_size;
                  CLEAR_FWD_EDGE(out_pt.ptr, edge_offset); 
					/* edge from Q -> B */
		  out_pt.ptr += point_size;
		}

		CLIP_AND_COPY(vinput->type, in_ptP, t_ptP, 
					in_ptR, t_ptR, out_pt);
					/* Places point "B" into output list */
		if (pddc->Static.attrs->edges != PEXOff) {
                  CLEAR_FWD_EDGE(out_pt.ptr,edge_offset); 
					/* edge from B -> Q */
		  CLEAR_BKWD_EDGE(out_pt.ptr,edge_offset);
					/* edge from B -> A */
		}
		out_pt.ptr += point_size;

		COPY_POINT(in_ptQ, out_pt, point_size); /* degenerate point */
		if (pddc->Static.attrs->edges != PEXOff) 
                  	CLEAR_BKWD_EDGE(out_pt.ptr,edge_offset); 
		out_pt.ptr += point_size;

		COPY_POINT(in_ptR, out_pt, point_size);
		out_pt.ptr += point_size;
		pts_in_list += 3;

		if (finput) {
		  /* 
		   * Add three identical facets 
		   */ 
		  COPY_FACET(in_fct, out_fct, facet_size);
		  out_fct += facet_size;
		  COPY_FACET(in_fct, out_fct, facet_size);
		  out_fct += facet_size;
		  COPY_FACET(in_fct, out_fct, facet_size);
		  out_fct += facet_size;
		  new_facets += 3;
		}
		break;

	      /* 
	       * Case 2 - P is inside, Q is outside, R is inside.
	       * Points P and A are already in the output list.
	       * Output list is P-A-R-B-R : disable edges from 
	       * A to R
	       *             
               *        Q+                 
               *         /\                    A  B
               *     ----------             ---+--+---        
               *       /    \     ===>        /    \
               *      /      \               /      \         
	       *    P+--------+R           P+--------+R       
	       *             
	       *      triangle PQR        triangles PAR, ARB, RBR
	       *             
	       */
	      case 2:
		if (pddc->Static.attrs->edges != PEXOff) {
		  out_pt.ptr -= point_size;
                  CLEAR_FWD_EDGE(out_pt.ptr,edge_offset); 
					/* Edge from A -> R */
		  out_pt.ptr += point_size;
		}

		COPY_POINT(in_ptR, out_pt, point_size);
					/* Place point "R" into output list */
		if (pddc->Static.attrs->edges != PEXOff) 
                	CLEAR_FWD_EDGE(out_pt.ptr,edge_offset); 
					/* Edge from R -> B */
		out_pt.ptr += point_size;

		CLIP_AND_COPY(vinput->type, in_ptQ, t_ptQ, 
				in_ptR, t_ptR, out_pt);
					/*place point "B" into output list */
		if (pddc->Static.attrs->edges != PEXOff) 
                	CLEAR_BKWD_EDGE(out_pt.ptr,edge_offset); 
					/* Edge from B -> A */
		out_pt.ptr += point_size;
	
		COPY_POINT(in_ptR, out_pt, point_size); /* degenerate point */
		out_pt.ptr += point_size;
		pts_in_list += 3;

		if (finput) {
		  /* 
		   * Add three identical facets
		   */ 
		  COPY_FACET(in_fct, out_fct, facet_size);
		  out_fct += facet_size;
		  COPY_FACET(in_fct, out_fct, facet_size);
		  out_fct += facet_size;
		  COPY_FACET(in_fct, out_fct, facet_size);
		  out_fct += facet_size;
		  new_facets += 3;
		}
		break;

	      /* 
	       * Case 3 - P and Q outside, R inside.
	       * Note - this is the first triangle in a new list
	       * 
	       * Output list is A-B-R. If beginning on odd facet
	       * output list is B-A-B-R
	       *
	       * 
               *        Q+  |                      |          
               *         /\ |                      |          
               *        /  \|                      |          
               *       /    |     ===>            B+     
               *      /     |\                     |\         
	       *    P+------|-+R                  A+-+R       
	       *            |                      |         
	       *      triangle PQR        triangle ABR or BABR
	       *
	       *
	       */
	      case 3:
		/*
		 * Note - this is the first triangle in a 
		 * list, so copy the first two points in
		 * addition to copying the last point. Also,
		 * in model clipping we must be careful to
		 * preserve the "sense" of the normals, so
		 * add a degenerate facet if necessary. 
		 */

		if(IS_ODD(k)) {
                  CLIP_AND_COPY(vinput->type, in_ptQ, t_ptQ,
                                in_ptR, t_ptR, out_pt);
                                        /*Places point "B" into output list */
                  out_pt.ptr += point_size;
		  pts_in_list++;
                  if (finput) {
                    COPY_FACET(in_fct, out_fct, facet_size);
                    out_fct += facet_size;
                    ++new_facets;
		  }
		}

		CLIP_AND_COPY(vinput->type, in_ptP, t_ptP, 
				in_ptR, t_ptR, out_pt);
					/*Places point "A" into output list */

                if (pddc->Static.attrs->edges != PEXOff) 
                        CLEAR_FWD_EDGE(out_pt.ptr,edge_offset);
					/*Edge from A -> B */
		out_pt.ptr += point_size;

		CLIP_AND_COPY(vinput->type, in_ptQ, t_ptQ, 
				in_ptR, t_ptR, out_pt);
					/*Places point "B" into output list */
		out_pt.ptr += point_size;


		COPY_POINT(in_ptR, out_pt, point_size);
					/*Places point "R" into output list */
		out_pt.ptr += point_size;

		if (finput) {
		  COPY_FACET(in_fct, out_fct, facet_size);
		  out_fct += facet_size;
		  ++new_facets;
		}
		pts_in_list += 3;
		break;

	      /* 
	       * Case 4 - P and Q inside, R outside
	       * Output list is P-Q-B-Q-A
	       *
               *        Q+  |                  Q+  |          
               *         /\ |                   /\ |          
               *        /  \|                  /  \|          
               *       /    |     ===>        /    +A     
               *      /     |\               /     |
	       *    P+------|-+R           P+------+B
	       *            |                      |
	       *             
	       *    triangle PQR          triangles PQB, BQB, BQA
	       *             
	       */
	      case 4:

                if (pddc->Static.attrs->edges != PEXOff) {
                  out_pt.ptr -= point_size;
                  CLEAR_FWD_EDGE(out_pt.ptr, edge_offset);
                                        /* edge from Q -> B */
                  out_pt.ptr += point_size;
                }

		CLIP_AND_COPY(vinput->type, in_ptR, t_ptR, 
				in_ptP, t_ptP, out_pt);
					/* Places "B" into output */
		if (pddc->Static.attrs->edges != PEXOff) 
                	CLEAR_FWD_EDGE(out_pt.ptr,edge_offset); 
					/* Edge from B -> Q */
		out_pt.ptr += point_size;

		COPY_POINT(in_ptQ, out_pt, point_size);
                                        /* Places "Q" into output */
		if (pddc->Static.attrs->edges != PEXOff) 
                	CLEAR_FWD_EDGE(out_pt.ptr,edge_offset); 
					/* Edge from Q -> B */
		out_pt.ptr += point_size;

		CLIP_AND_COPY(vinput->type, in_ptR, t_ptR, 
				in_ptQ, t_ptQ, out_pt);
					/* Places "A" into output */
		if (pddc->Static.attrs->edges != PEXOff) 
                	CLEAR_BKWD_EDGE(out_pt.ptr,edge_offset); 
					/* Edge from A -> B */
		out_pt.ptr += point_size;
		pts_in_list += 3;

		if (finput) {
		  /* 
		   * Add three identical facets
		   */ 
		  COPY_FACET(in_fct, out_fct, facet_size);
		  out_fct += facet_size;
		  COPY_FACET(in_fct, out_fct, facet_size);
		  out_fct += facet_size;
		  COPY_FACET(in_fct, out_fct, facet_size);
		  out_fct += facet_size;
		  new_facets += 3;
		}
		break;

	      /* 
	       * Case 5 - P and R are outside, Q in inside.
	       * Output list is A-Q-B. A and Q already in list
	       *
               *        Q+                      Q+
               *         /\                     /\ 
               *     ----------             ---+--+---        
               *       /    \     ===>        A    B
               *      /      \         
	       *    P+--------+R      
	       *             
	       *      triangle PQR        triangles AQB
	       *             
	       */
	      case 5:
		CLIP_AND_COPY(vinput->type, in_ptR, t_ptR, 
				in_ptQ, t_ptQ, out_pt);
					/*Places point "B" into output list */
                if (pddc->Static.attrs->edges != PEXOff) 
                        CLEAR_BKWD_EDGE(out_pt.ptr,edge_offset);
					/* Edge from B -> A */
		out_pt.ptr += point_size;
		pts_in_list++;

		if (finput) {
		  COPY_FACET(in_fct, out_fct, facet_size);
		  out_fct += facet_size;
		  ++new_facets;
		}
		break;

	      /* 
	       * Case 6 - P is inside, R and Q are outside
	       * Output list is: P-A-B
	       *             
               *       |Q+                    |
               *       | /\                   |
               *       |/  \                  |
               *       |    \     ===>        +A
               *      /|     \               /|
	       *    P+-|------+R           P+-+B
	       *       |                      |
	       *      triangle PQR        triangles 
	       *
	       */
	      case 6:
                if (pddc->Static.attrs->edges != PEXOff) {
		  out_pt.ptr -= point_size;
                  CLEAR_FWD_EDGE(out_pt.ptr,edge_offset);
		  out_pt.ptr += point_size;
		}

		CLIP_AND_COPY(vinput->type, in_ptR, t_ptR, 
				in_ptP, t_ptP, out_pt);
					/* Places "B" into output list */
		out_pt.ptr += point_size;
		pts_in_list += 1;

		if (finput) {
		  COPY_FACET(in_fct, out_fct, facet_size);
		  out_fct += facet_size;
		  ++new_facets;
		}
		break;

	      /* 
	       * Case 7 - P, Q, and R are outside
	       * trivial rejection; begin new list 
	       * at first "included" point if
	       * sufficient number of points 
	       *
               *        Q+             
               *         /\            
               *        /  \           
               *       /    \     ===> 
               *      /      \        
	       *    P+--------+R      
	       *                      
	       */
	      case 7:
		/* Complete initialization of last list */
          	if (pts_in_list > 2) {
            	  /* Generate  a new output list, increment list count. */
            	  pddolist->numPoints = pts_in_list;
		  voutput->numLists++;
		  out_listnum++;
		  MI_ALLOCLISTHEADER(voutput, 
			MI_ROUND_LISTHEADERCOUNT(voutput->numLists));
		  /* skip to next output list */
		  pddolist = voutput->ddList + out_listnum;

          	  /* Insure sufficient room for remaining verts 
			and degenerate points */
          	  MI_ALLOCLISTOFDDPOINT(pddolist, 
				2*(vert_count - k), point_size);

		  out_pt.ptr = pddolist->pts.ptr;
          	  if (!out_pt.ptr) return(BadAlloc);
		}

          	else {
		  /* Not enough points for output list */
            	  pddolist->numPoints = 0;
		}
 
		/* Look for next point inside bounds */
		do {
		  clip_code = 0;
		  in_ptR.ptr += point_size;
		  if (finput) in_fct += facet_size;
		  k++;
  		  if (k == vert_count)
  		    break;
            	  COMPUTE_CLIP_PARAMS(in_ptR, t_ptR, 2, clip_mode,
			current_clip,MC_HSpace,clip_code);
		} while(clip_code);
		if (k < vert_count) {
  		  /*
  		   * k is incremented by the for loop, but because we have
  		   * a new_list, the pointers won't get bumped.  Therefore,
  		   * k must be decremented to keep it consistent with the
  		   * pointers for reentering the for loop.
  		   *
  		   * However, k must not be adjusted when k == vert_count,
  		   * because in this case P, Q and R are all clipped and it
  		   * is necessary to leave k == vert_count to terminate the
  		   * for loop.
  		   */
  		  k--;

		  /* Get P & Q; re-enter loop. 
	 	   * Next case encountered will be 3, which will
		   * handle the odd-even rule for normals.
		   */
		  in_ptQ.ptr = in_ptR.ptr - point_size;
		  in_ptP.ptr = in_ptQ.ptr - point_size;
		  clip_code = 0; /* Q's ORd in at top of loop */
          	  COMPUTE_CLIP_PARAMS(in_ptP, t_ptP, 0, clip_mode,
			current_clip,MC_HSpace,clip_code);
          	  COMPUTE_CLIP_PARAMS(in_ptQ, t_ptQ, 1, clip_mode,
			current_clip,MC_HSpace,clip_code);
 		} 
		new_list = 1;
		pts_in_list = 0; /*start a new list*/
	        break;

	     } /* end of cases */

	      if (!new_list) {
		/* Prepare for next point */
		in_ptP = in_ptQ; t_ptP = t_ptQ;
		in_ptQ = in_ptR; t_ptQ = t_ptR;
		in_ptR.ptr += point_size;
		if (finput) in_fct += facet_size;
		clip_code >>= 1;
	      }

          } /* end of single list processing */

          if (pts_in_list > 2) {	/* Got some points! */
            /* skip to next list, increment list count. */
            pddolist->numPoints = pts_in_list;
	    ++pddolist;
	    ++out_listnum;
	    ++voutput->numLists;
          }
          else pddolist->numPoints = 0;		/* use same list */

          pddilist++;
        } /* end of list of lists processing */

	if (foutput) foutput->numFacets = new_facets;
 
      if (out_listnum > 0) {
        /* Use result of previous clip for input to next clip */
        vinput = voutput;
        if (voutput == list2) voutput = list1;	/* ping-pong */
        else voutput = list2;

        if (finput) {
          /* Use result of previous clip for input to next clip */
          finput = foutput;
          if (foutput == fct_list2) foutput = fct_list1;
          else foutput = fct_list2;
        }
      } else {
	/* If no lists, exit loop */
	vinput = voutput;
	finput = foutput;
      }
      if (clip_mode == MI_MCLIP) MC_HSpace++;

    } /* end of processing for all clip planes */


    /* Current input list is last processed (clipped) list */
    *output_vert = vinput;
    if (finput) *output_fct = finput;
    else *output_fct = input_fct;


    return (Success);

}


 
/*++
 |
 |  miLightTriStrip
 |
 |      Perform lighting calculations for the vertex or facet
 |      data provided according to the current attributes.
 |
 --*/

ddpex3rtn
miLightTriStrip(pRend, pddc, input_vert, input_fct, output_vert, output_fct)
    ddRendererPtr       pRend;          /* renderer handle */
    miDDContext         *pddc;          /* dd Context pointer */
    miListHeader        *input_vert;    /* input vertex data */
    listofddFacet       *input_fct;     /* input facet data */
    miListHeader        **output_vert;  /* output vertex data */
    listofddFacet       **output_fct;   /* output facet data */
{
/* calls */
    ddpex3rtn           miApply_Lighting();
    ddpex3rtn           miFilterPath();
 
/* uses */
    listofddFacet               *fct_list, *broken_fct;
    miListHeader                *out_vert, *broken_list;
    ddVector3D                  in_vertnorm;
    ddRgbFloatColour            in_vertcolour;
    ddRgbFloatNormal            *in_fct;
    ddRgbFloatColour            *out_fct;
    listofddPoint               *pddilist;
    listofddPoint               *pddolist;
    ddPointUnion                in_pt, out_pt;
    int                         i, j, k, num_facets,
				inpoint_size, outpoint_size;
    ddpex3rtn                   status;


    /* Look for empty fill style.  Still might have to render edges. */
    if (pddc->Static.attrs->intStyle == PEXInteriorStyleEmpty)
	return(Success);
 
    /*
     * First, Insure that the vertex and/or facet data
     * is sufficient for the current Surface Interpolation method.
     * Note that this implementation does not support
     * PEXSurfaceInterpDotProduct and PEXSurfaceInterpNormal and
     * that these surface interpolation types are approximated by
     * PEXSurfaceInterpColour. The cases dealt with here, therefore,
     * are constant surface color (thus a single color is computed
     * per facet) and interpolated surface color (thus a color
     * per vertex is required).
     */

    DD_VertPointSize(input_vert->type, inpoint_size);
    switch(pddc->Static.attrs->surfInterp) {
 
      case PEXSurfaceInterpNone: /* Flat shading */
 
          if ((!input_fct) || 
	      (input_fct->numFacets == 0) ||
	      (!( (DD_IsFacetColour(input_fct->type)) && 
	        (DD_IsFacetNormal(input_fct->type))))) {

            Complete_TriFacetList(pRend, input_vert, input_fct,
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
	       (DD_IsVertColour(input_vert->type)) ) {
	    if (status = miFilterPath(pddc, input_vert, output_vert, 
				      ((1 << 3) | 1) ))
	      return(status);
	  } else {
	    *output_vert = input_vert;
	  }

	  /*
           * Now allocate storage for the output facet list
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
	  for(i = 0, in_pt.ptr = pddilist->pts.ptr; 
		i < input_vert->numLists; i++) {
 
            /*
             * Compute lighted facet color for each facet.
             * Facet color is simply the sum of the lighting contributions
             * from each light source.
             */
            for (k = 2; k < pddilist->numPoints; k++) {

	      /* One point at a time. Again, it is assumed there is a */
	      /* one<->one correspondence between the points in the lists */
	      /* and individual facets. Also note that we are associating */
	      /* arbitrarily the first point with the "position" of the */
	      /* facet. Perhaps a more correct method would use that average */

              if (status = miApply_Lighting(pRend, pddc,
                                          in_pt.p4Dpt,
                                          &(in_fct->colour),
                                          &(in_fct->normal),
                                          out_fct ))
                return(status);

		in_pt.ptr += inpoint_size;
                in_fct++;
                out_fct++;
            }
	    /* skip to next input vertex list */
	    pddilist++;
	  }
        break;
 
      case PEXSurfaceInterpColour:
      case PEXSurfaceInterpDotProduct:
      case PEXSurfaceInterpNormal:

        if (!DD_IsVertNormal(input_vert->type))  {
	  /* Here, and only here, we must create a separate list for
	   * facet of the triangle strip.  The reason for this is that
	   * we can only associate with each vertex a single normal,
	   * when in reality a vertex has different normals depending
	   * upon which facet is being rendered.  The lighting effects
	   * only appear in positional lights, because the dot
	   * product with the vertex normals is indeed dependent upon
	   * position */
	
          if (i = Breakup_TriStrip(pddc, input_vert, input_fct, 
			&broken_list, &broken_fct)) return (i);
	  input_vert = broken_list;
	  input_fct = broken_fct;
        }

	/* Insure sufficient info for calculation */ 
        if ( (!DD_IsVertColour(input_vert->type)) ||
             (!DD_IsVertNormal(input_vert->type)) ) {
          Calculate_TriStrip_Vertex_Color_and_Normal(pRend, input_vert,
                                                     input_fct,
                                                     output_vert);
          input_vert = *output_vert;
        }
 
        /* From here facet data only used in culling operation */
        *output_fct = input_fct;
 
        /* Use one of the pre-defined 4D list for output */
        *output_vert = out_vert = MI_NEXTTEMPDATALIST(pddc);

        /* Insure sufficient room for each header */         
        MI_ALLOCLISTHEADER(out_vert, 
			MI_ROUND_LISTHEADERCOUNT(input_vert->numLists))
        if (!out_vert->ddList) return(BadAlloc);
 
        out_vert->type = DD_RGBFLOAT_POINT4D;
        if (pddc->Static.attrs->edges != PEXOff &&
	    DD_IsVertEdge(input_vert->type)) {
            DD_SetVertEdge(out_vert->type);
        }
        out_vert->numLists = input_vert->numLists;
        out_vert->flags =  input_vert->flags;
 
        DD_VertPointSize(out_vert->type, outpoint_size);
        pddilist = input_vert->ddList;
        pddolist = out_vert->ddList;
 
        for(i = 0; i < input_vert->numLists; i++) {

          pddolist->numPoints = pddilist->numPoints;
          MI_ALLOCLISTOFDDPOINT(pddolist, pddolist->numPoints,
                                  outpoint_size);
          if (!(out_pt.ptr = pddolist->pts.ptr)) return(BadAlloc);

          for (j = 0, in_pt.ptr = pddilist->pts.ptr;
		 j < pddilist->numPoints; j++) {

            /* Copy over the coordinate info */
            *out_pt.p4Dpt = *in_pt.p4Dpt;
 
	    /* move output pointer to colour field */
	    out_pt.p4Dpt++;

	    /* miApplyLighting works on a single point at a time */
            if (status = miApply_Lighting(pRend, pddc,
                                            in_pt.p4Dpt,
                                            &(in_pt.pRgbFloatNpt4D->colour),
                                            &(in_pt.pRgbFloatNpt4D->normal),
                                            out_pt.pRgbFloatClr))
			return(status);

	    /* increment pointers */ 
            in_pt.pRgbFloatNpt4D++;
            out_pt.pRgbFloatClr++;

            if (DD_IsVertEdge(out_vert->type)) {
              *out_pt.pEdge = *in_pt.pEdge;
              out_pt.pEdge++;
              in_pt.pEdge++;
            }


          }
	  /* skip to next input and output vertex list */
	  pddilist++;
	  pddolist++;
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
 |  Complete_TriFacetList(pRend, input_vert, output_fct)
 |
 |      Generates a facet list with colors and normals 
 |	This routine should get folded into one with 
 |	Create_Normals, with some form of flag to signal
 |	the type of output required. It would also be the
 |	place to put back-face attribute distinction.
 |
 --*/
/* calls */

static
ddpex3rtn
Complete_TriFacetList(pRend, input_vert, input_fct, output_fct)
    ddRendererPtr       pRend;    /* renderer handle */
    miListHeader        *input_vert;    /* input vertex data */
    listofddFacet       *input_fct;     /* input facet data */
    listofddFacet       **output_fct;   /* output facet data */
{
/* local */
    miDDContext         *pddc = (miDDContext *)(pRend->pDDContext);
    listofddFacet       *fct_list;
    ddRgbFloatNormal    *out_fct;
    listofddPoint       *pddlist;
    ddPointUnion	in_pt, tmp_pt;
    ddFacetUnion        in_fct;
    int                 point_size;
    ddRgbFloatPoint4D   *vert1, *vert2, *vert3;
    int                 total_facets;
    int			i,j;
    float               length;
    ddpex3rtn           status;
    char		have_colors, have_normals;

    have_colors = have_normals = 0;

    /* What data must be added to output facet list ? */
    if (!(input_fct) || (input_fct->type == DD_FACET_NONE)) {
      /*
       * Since we are creating the facet list for the first
       * we need to learn how many verticies are in all the
       * lists.
       */
      pddlist = input_vert->ddList;
      for (i = 0, total_facets = 0; i < input_vert->numLists; pddlist++, i++)
	total_facets  += (pddlist->numPoints - 2); 
    } else {
      /* use input facet information */
      total_facets = input_fct->numFacets;
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
    fct_list->type = DD_FACET_RGBFLOAT_NORM;
    MI_ALLOCLISTOFDDFACET(fct_list, total_facets, sizeof(ddRgbFloatNormal));
    if (!fct_list->facets.pNoFacet) return(BadAlloc);
    out_fct = (ddRgbFloatNormal *)fct_list->facets.pFacetRgbFloatN;

    DD_VertPointSize(input_vert->type, point_size);

    /* Don't process if insufficient number of points */

    if (input_vert->numLists > 0) {

     pddlist = input_vert->ddList;
	
     for (i = 0; i < input_vert->numLists; i++) {

      in_pt.ptr = pddlist->pts.ptr;

      for (j = 2; j < pddlist->numPoints; j++) {

	/*
         * Compute "intrinsic" color of facet.
         * There is a "hierarchy" of sources for a facet's intrinsic
         * color:
         *              vertex_colors present?  facet_color = 
	 *		  vertex_color of 1st point of facet
         *              else facet_color present? facet_color = facet_color
         *              else facet_color = PC_surface_color
	 *	Note - this needs to be enhanced, using a dot product
	 *	with the eye position to determine which surface color
	 *	should be assigned in the case of front/back face
	 *	distinguishing.  this entire routine might get rolled into
	 *	miLightTriStrip, and the correct color and reflectance
	 *	model get passed into miApplyLighting.
         */
 
	if (DD_IsVertColour(input_vert->type)) {

	  out_fct->colour.red=out_fct->colour.green=out_fct->colour.blue=0.0;
          vert1 = in_pt.pRgbFloatpt4D;
          tmp_pt.ptr = in_pt.ptr + point_size;
          vert2 = tmp_pt.pRgbFloatpt4D;
          tmp_pt.ptr += point_size;
          vert3 = tmp_pt.pRgbFloatpt4D;
	  
	  out_fct->colour.red = ( vert1->colour.red + 
				  vert2->colour.red +
				  vert3->colour.red )/3.0;
	  out_fct->colour.green = ( vert1->colour.green + 
				    vert2->colour.green +
				    vert3->colour.green )/3.0;
	  out_fct->colour.blue = ( vert1->colour.blue + 
				   vert2->colour.blue +
				   vert3->colour.blue )/3.0;

        } else if (have_colors) {
	  out_fct->colour = *in_fct.pFacetRgbFloat;
	  in_fct.pFacetRgbFloat++;
	} else {
	  /* use front face colors. This needs to get generalized
	     to deal with back-facing attributes*/
          out_fct->colour =
                (pddc->Static.attrs->surfaceColour.colour.rgbFloat);
	}

	if (!(have_normals)) {
         /*
          * Compute surface normal. Normals are required in
	  * Apply_Lighting for directional and positional lights,
	  * as well as culling (if enabled). One COULD check
	  * for ambient only, no culling, to optimize.
	  *
          * The Surface normal is the cross product 
          * of three non-colinear points, in correct order.
          */        

         vert1 = in_pt.pRgbFloatpt4D;
         tmp_pt.ptr = in_pt.ptr + point_size;
         vert2 = tmp_pt.pRgbFloatpt4D;
         tmp_pt.ptr += point_size;
         vert3 = tmp_pt.pRgbFloatpt4D;
	  
         if IS_ODD(j) {
	      CROSS_PRODUCT((&(vert3->pt)), (&(vert2->pt)), (&(vert1->pt)), 
			    &(out_fct->normal));
	      }
	 else {
	      CROSS_PRODUCT((&(vert1->pt)), (&(vert2->pt)), (&(vert3->pt)), 
			    &(out_fct->normal));
	    }

            NORMALIZE_VECTOR(&(out_fct->normal), length);
 
            /* Initialize to zero if triangle is
	     * degenerate and points are co-linear
	     */

            if NEAR_ZERO(length) {
	      /* degenerate facet */ 
	      (out_fct->normal.x = 0.0); 
              (out_fct->normal.y = 0.0);
              (out_fct->normal.z = 0.0);
            }
	  } else { 
	    /* use input facet normals */
	    out_fct->normal = *in_fct.pFacetN;
	    in_fct.pFacetN++;
	  }

          /* Process next facet */
          out_fct++;
	  in_pt.ptr += point_size;
        }
 
	pddlist++;
      }

      fct_list->numFacets = total_facets;
      fct_list->type = DD_FACET_RGBFLOAT_NORM;

    } else {
      fct_list->type = DD_FACET_NONE;
      fct_list->numFacets = 0;
    }
 
  *output_fct = fct_list;
  return(Success);

}
 
/*++
 |
 |  Calculate_TriStrip_Facet_Normal
 |
 |      Add facet normals to a facet list.
 |	Here we are counting on the fact that there is a one to one
 |	correspondance between input facets and vertex ordering.
 |
 --*/
static
ddpex3rtn
Calculate_TriStrip_Facet_Normal(pddc, input_vert, input_fct, output_fct)
    miDDContext         *pddc;
    miListHeader        *input_vert;    /* input vertex data */
    listofddFacet       *input_fct;     /* input facet data */
    listofddFacet       **output_fct;   /* output facet data */
{

    listofddFacet       *fct_list;
    ddRgbFloatColour    *in_fct;
    ddFacetUnion        out_fct;
    listofddPoint       *pddlist;
    ddPointUnion	in_pt, nxt_pt;
    ddVector3D          normal;
    int                 point_size;
    ddCoord3D           *vert1, *vert2, *vert3;
    int                 numfacets;
    int                 i,j;
    float               length;


    /* Some quick error checking */
    if ((input_fct) && (DD_IsFacetNormal(input_fct->type))) return(Success);


    /*
     * First, allocate storage for the facet list
     */
    fct_list = MI_NEXTTEMPFACETLIST(pddc);
    if ((input_fct) && DD_IsFacetColour(input_fct->type)) {
      in_fct = input_fct->facets.pFacetRgbFloat;
      fct_list->type = DD_FACET_RGBFLOAT_NORM;
      numfacets = input_fct->numFacets;
      MI_ALLOCLISTOFDDFACET(fct_list, numfacets, sizeof(ddRgbFloatNormal));
    } else {
      in_fct = 0;
      fct_list->type = DD_FACET_NORM;


      /* Determine the total number of facets in all the lists */ 
      for (i = 0, numfacets = 0, pddlist = input_vert->ddList;
	   i < input_vert->numLists; pddlist++, i++)
        numfacets  += (pddlist->numPoints - 2);

      MI_ALLOCLISTOFDDFACET(fct_list, numfacets, sizeof(ddVector3D));
    }
 
    fct_list->numFacets = numfacets;
    if (!fct_list->facets.pNoFacet) return(BadAlloc);

    /* Otherwise... */ 
    out_fct = fct_list->facets;
    DD_VertPointSize(input_vert->type, point_size);
 
    /* Don't process if no facets (!) */
    if (numfacets == 0) return(1);
 
    else {
      for(i = 0, pddlist = input_vert->ddList; 
		i < input_vert->numLists; i++) {
        for (j = 2, in_pt.ptr = pddlist->pts.ptr; 
			j < pddlist->numPoints; j++) {
 
          /* Copy the input facet color */
          if (in_fct) {
            *out_fct.pFacetRgbFloat = *in_fct;
            in_fct++;
          }

	  /* Calculate and copy normal */ 
 
          vert1 = in_pt.p3Dpt;
          nxt_pt.ptr = in_pt.ptr + point_size;
          vert2 = nxt_pt.p3Dpt;
          nxt_pt.ptr += point_size;
          vert3 = nxt_pt.p3Dpt;

          if (IS_ODD(j)) {
		CROSS_PRODUCT(vert3, vert2, vert1, &normal);
	  } else {
		CROSS_PRODUCT(vert1, vert2, vert3, &normal);
	  }

          NORMALIZE_VECTOR(&normal, length)
	  
          /* Initialize to zero if triangle is
           * degenerate and points are co-linear
           */

          if (length == 0.0) {
            (normal.x = 0.0);
            (normal.y = 0.0);
            (normal.z = 0.0);
           }

 
          if (in_fct) (out_fct.pFacetRgbFloatN++)->normal = normal;
          else *(out_fct.pFacetN++) = normal;

          in_pt.ptr += point_size;
        }
	pddlist++;
      }
    }
 
    *output_fct = fct_list;
 
    return(Success);
}

 
/*++
 |
 |  Breakup_TriStrip 
 |
 |   Breaks up a triangle strip into as many lists as there are facets.
 |   This is necessary for shading interpolation methods as we need to
 |   facilitate a set of normals per facet.   
 |     
 |    
 |
 --*/
static
ddpex3rtn
Breakup_TriStrip(pddc, input_vert, input_fct, output_vert, output_fct)
    miDDContext         *pddc;
    miListHeader        *input_vert;
    miListHeader	**output_vert;    
    listofddFacet       *input_fct;     /* input facet data */
    listofddFacet       **output_fct;   /* output facet data */
{
    miListHeader	*list1;
    listofddFacet       *fct_list;
    ddRgbFloatColour    *in_fct;
    ddFacetUnion        out_fct;
    listofddPoint       *pddilist, *pddolist;
    ddPointUnion        pt_in, nxt_pt;
    char		*in_vert, *out_vert;
    int                 point_size;
    int                 numfacets, facetofpoints;
    int                 i,j;
 


    /* make sure that we have a facet list, and that it has normals */
    if (!((input_fct) && (DD_IsFacetNormal(input_fct->type)))) {
      if (i = Calculate_TriStrip_Facet_Normal(pddc, input_vert,
                                              input_fct, &fct_list))
        return(i);
      input_fct = fct_list;
    }
  
    list1 = MI_NEXTTEMPDATALIST(pddc);
    MI_ALLOCLISTHEADER(list1,
                        MI_ROUND_LISTHEADERCOUNT(input_fct->numFacets));
    if (!list1->ddList) return(BadAlloc);
    list1->type = input_vert->type;
    list1->flags =  input_vert->flags;

    list1->numLists = input_fct->numFacets; 
    

    DD_VertPointSize(input_vert->type, point_size);
    facetofpoints = 3 * point_size;

    for(i = 0,  pddilist = input_vert->ddList,
		pddolist = list1->ddList;
                i < input_vert->numLists; i++) {

      for (j = 2, pt_in.ptr = pddilist->pts.ptr;
                j < pddilist->numPoints; j++) {

        MI_ALLOCLISTOFDDPOINT(pddolist, 1, facetofpoints);

	/*
	 * Note that to preserver correct normals, must
	 * flip the order of the vertices every other facet.
	 */
        if IS_ODD(j) {

	  in_vert = pt_in.ptr + 2*point_size;
	  out_vert = pddolist->pts.ptr;
	  memcpy( out_vert, in_vert, point_size);
	  in_vert -= point_size; out_vert += point_size;
	  memcpy( out_vert, in_vert, point_size);
				 out_vert += point_size;
	  memcpy( out_vert, pt_in.ptr, point_size);
  
	} else {

	  memcpy( pddolist->pts.ptr, pt_in.ptr, facetofpoints);

	}

	pddolist->numPoints = 3;
	pddolist++;		/* one list per facet */
	pt_in.ptr += point_size;
      }
      pddilist++;
    }

    *output_fct = input_fct;
    *output_vert = list1; 
    return(Success);        
}
 



 
/*++
 |
 |  Calculate_TriStrip_Vertex_Color_and_Normal
 |
 |      Add vertex normals and colors to a vertex list.
 |
 --*/
static
ddpex3rtn
Calculate_TriStrip_Vertex_Color_and_Normal(pRend, input_vert, input_fct,
                                           output_vert)

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
    ddFacetUnion                in_fct;
    int                         inpoint_size, outpoint_size;
    int                         facet_size;
    int                         numFacets=0;
    int                         i,j;
    ddpex3rtn           	status;

 
    /* Some quick error checking */
    if ((DD_IsVertNormal(input_vert->type)) &&
        (DD_IsVertColour(input_vert->type)))
      return(Success);
 
    /* Use one of the pre-defined 4D list for output */
    *output_vert = out_vert = MI_NEXTTEMPDATALIST(pddc);
 
    /* Insure sufficient room for each header */
    MI_ALLOCLISTHEADER(out_vert, 
			MI_ROUND_LISTHEADERCOUNT(input_vert->numLists))
    if (!out_vert->ddList) return(BadAlloc);
 
    out_vert->type = DD_RGBFLOAT_NORM_POINT4D;
    if (pddc->Static.attrs->edges != PEXOff &&
	DD_IsVertEdge(input_vert->type)) {
	DD_SetVertEdge(out_vert->type);
    }
    out_vert->numLists = input_vert->numLists;
    out_vert->flags =  input_vert->flags;
 
    pddilist = input_vert->ddList;
    pddolist = out_vert->ddList;
 
    DD_VertPointSize(input_vert->type, inpoint_size);
    DD_VertPointSize(out_vert->type, outpoint_size);
 
    /* Compute facet normals if no per-vertex normals with data */
    if ( (!(DD_IsVertNormal(input_vert->type))) &&
         ((!(input_fct)) || (!(DD_IsFacetNormal(input_fct->type)))) ) {
 
      if (i = Calculate_TriStrip_Facet_Normal(pddc, input_vert,
                                              input_fct, &fct_list))
        return(i);
 
      input_fct = fct_list;
    }
 
    if ((input_fct) && (input_fct->numFacets > 0)) {
	in_fct = input_fct->facets;
	DDFacetSIZE(input_fct->type, facet_size);
    }
    else in_fct.pNoFacet = 0;
 

    for(i = 0; i < input_vert->numLists; i++) { 
      pddolist->numPoints = pddilist->numPoints;
      in_pt.ptr = pddilist->pts.ptr;
      MI_ALLOCLISTOFDDPOINT(pddolist,(pddilist->numPoints+1),
                            outpoint_size);
      if (!(out_pt.ptr = pddolist->pts.ptr)) return(BadAlloc);

      for (j = 0, in_pt.ptr = pddilist->pts.ptr,
        out_pt.ptr = pddolist->pts.ptr;
        j < pddilist->numPoints; j++) {
 
        /* First copy over coordinate data */
        *out_pt.p4Dpt = *in_pt.p4Dpt;
        in_pt.p4Dpt++;
        out_pt.p4Dpt++;
 
        /*
         * Next color
         * Colour is derived first from the vertex, second from the
         * facet, and third from the current PC attributes.
         */
        if (DD_IsVertColour(input_vert->type)){
          *out_pt.pRgbFloatClr = *in_pt.pRgbFloatClr;
          in_pt.pRgbFloatClr++;
        } 
	else {
	  if ((in_fct.pNoFacet) && (DD_IsFacetColour(input_fct->type)))
            *out_pt.pRgbFloatClr = *in_fct.pFacetRgbFloat;
          else {
            *out_pt.pRgbFloatClr =
                (pddc->Static.attrs->surfaceColour.colour.rgbFloat);
          }
        }
 
        out_pt.pRgbFloatClr++;
             
        /*
         * Next normals
         * Normals are derived first from the vertex, second from the
         * facet (note that we insured above that there were facet normals).
         */
        if DD_IsVertNormal(input_vert->type) {
          *out_pt.pNormal = *in_pt.pNormal;
          in_pt.pNormal++;
        }
        else if (DD_IsFacetColour(input_fct->type))
          *out_pt.pNormal = in_fct.pFacetRgbFloatN->normal;
        else *out_pt.pNormal = *in_fct.pFacetN;

	out_pt.pNormal++; 
 
        /* Next pass along edge info if there is any */
        if (DD_IsVertEdge(out_vert->type)) {
          *out_pt.pEdge = *in_pt.pEdge;
          in_pt.pEdge++;
          out_pt.pEdge++;
        }
 
      }
      if ((in_fct.pNoFacet)) in_fct.pNoFacet += facet_size;
      pddilist++;
      pddolist++;
    }
    return(Success);
}
 
/*++
 |
 |  miCullTriStrip(pddc, input_vert, input_fct, output_vert, output_fct)
 |
 |      Perform culling of facets, and their associated data points,
 |      according to the current culling mode.
 |
 --*/

static
ddpex3rtn
miCullTriStrip(pddc, input_vert, input_fct, output_vert, output_fct)
    miDDContext         *pddc;          /* dd Context pointer */
    miListHeader        *input_vert;    /* input vertex data */
    listofddFacet       *input_fct;     /* input facet data */
    miListHeader        **output_vert;  /* output vertex data */
    listofddFacet       **output_fct;   /* output facet data */
{
/* uses */
    miListHeader                *out_vert;
    listofddPoint               *pddilist;
    listofddPoint               *pddolist;
    listofddFacet               *fct_list;
    ddFacetUnion                in_fct;
    ddFacetUnion                out_fct;
    ddPointUnion                in_pt,out_pt;
    listofddPoint               temp;
    int                         i, j;
    char                        accept, new_list;
    char                        return_facet_list;
    int                         point_size, facet_size;
    int				verts_in_list, out_listnum;

    /*
     * Create facet normals if necessary. These are used to determine
     * if the facet is to be culled. Note: only return a facet list
     * if a valid facet list is input.
     */
    if ( (!input_fct) || (input_fct->numFacets <= 0) ) {
        Calculate_TriStrip_Facet_Normal(pddc, input_vert,
                                        (listofddFacet *)0, &input_fct);
        return_facet_list = 0;
        *output_fct = 0;
    } else {
        if (!(DD_IsFacetNormal(input_fct->type))) {
          Calculate_TriStrip_Facet_Normal(pddc, input_vert,
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
    out_vert->numLists = 0;
    MI_ALLOCLISTHEADER(out_vert, 
			MI_ROUND_LISTHEADERCOUNT(input_vert->numLists))
    if (!out_vert->ddList) return(BadAlloc);


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
     */

    for(i = 0, out_listnum = 1, fct_list->numFacets = 0, 
	out_vert->numLists = 0,
	pddilist = input_vert->ddList, pddolist = out_vert->ddList; 
	i < input_vert->numLists; i++) {

      MI_ALLOCLISTOFDDPOINT(pddolist,(pddilist->numPoints+1),point_size);
      if (!pddolist->pts.ptr) return(BadAlloc);
 
      for (j= 2, verts_in_list = 0, new_list = 1,
	   in_pt.ptr = pddilist->pts.ptr + (2 * point_size),
	   out_pt.ptr = pddolist->pts.ptr,
	   pddolist->numPoints = 0; 
		j < pddilist->numPoints; j++) {

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

	  if (new_list) { /* starting new list after facet(s) culled */
	    /*initialize first points for the first facet */
	    memcpy( out_pt.ptr, in_pt.ptr - (2 * point_size), (2*point_size));
	    out_pt.ptr += 2 * point_size;
	    new_list = 0;
	    verts_in_list += 2;
	  }

	  /* copy the vertex info associated with this facet */
	  memcpy( out_pt.ptr, in_pt.ptr, point_size);
	  out_pt.ptr += point_size;
	  ++verts_in_list;

          /* copy the facet info */
          if (DD_IsFacetColour(input_fct->type))
            *out_fct.pFacetRgbFloatN = *in_fct.pFacetRgbFloatN;
          else *out_fct.pFacetN = *in_fct.pFacetN;

	  /* increment the output pointer */
	  out_fct.pNoFacet += facet_size;
    	  fct_list->numFacets++;

        } else {
	  /* Facet culled; terminate output vertex list */
	  if (verts_in_list > 2) {   /* facets in this list */
	    pddolist->numPoints = verts_in_list;
	    ++out_vert->numLists;
	    out_listnum++;
	    /* create a new output vertex list; load first points */
	    MI_ALLOCLISTHEADER(out_vert,
                        MI_ROUND_LISTHEADERCOUNT(out_listnum))
	    if (!out_vert->ddList) return(BadAlloc);
  
	    pddolist = out_vert->ddList + (out_listnum - 1);
	    pddolist->numPoints = 0;
      	    MI_ALLOCLISTOFDDPOINT(pddolist,
			(pddilist->numPoints - j + 2), point_size);
	    if (!pddolist->pts.ptr) return(BadAlloc);
	    out_pt.ptr =  pddolist->pts.ptr;
	    verts_in_list = 0;
	  } 
	  new_list = 1;
        }
	in_pt.ptr += point_size;
	in_fct.pNoFacet += facet_size; 
      }
      ++pddilist;
      if (verts_in_list > 2) {
	pddolist->numPoints = verts_in_list;
	++out_listnum;
	++out_vert->numLists;
	++pddolist;
      }
    }
 
    /*
     * Only return facet list if one was passed in. Reduces the
     * information that must be processed by the rest of the pipeline.
     */
    if (return_facet_list) *output_fct = fct_list;
       
    return(Success);
}

/*++
 |
 |  miDepthCueTriStrip(pddc, input_vert, input_fct, output_vert, output_fct)
 |
 |     Performs Depth cueing of triangle strips data lists..
 |	Assigns per-vertex colors to the data list according to the 
 |      "Annex E - Informative" discussion of the ISO PHIGS PLUS spec. 
 |     
 |
 --*/
static
ddpex3rtn
miDepthCueTriStrip(pRend, input_vert, input_fct, output_vert)

    ddRendererPtr       pRend;          /* renderer handle */
    miListHeader        *input_vert;    /* input vertex data */
    listofddFacet       *input_fct;     /* input facet data */
    miListHeader        **output_vert;  /* output vertex data */
{
    miDDContext         *pddc = (miDDContext *)(pRend->pDDContext);
    miListHeader                *out_vert;
    listofddFacet               *fct_list;
    ddFLOAT			pt_depth;
    listofddPoint               *pddilist;
    listofddPoint               *pddolist;
    ddPointUnion                in_pt, out_pt;
    ddRgbFloatColour		*in_color;
    ddFacetUnion                in_fct;
    int                         inpoint_size, outpoint_size;
    int                         facet_size;
    int                         numFacets=0;
    int                         i,j;
    ddpex3rtn                   status;
    ddDepthCueEntry		*dcue_entry;

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
    MI_ALLOCLISTHEADER(out_vert,
                        MI_ROUND_LISTHEADERCOUNT(input_vert->numLists))
    if (!out_vert->ddList) return(BadAlloc);

    out_vert->type = input_vert->type;
    if (!(DD_IsVertColour(input_vert->type))) 
		DD_SetVertRGBFLOAT(out_vert->type); 
    out_vert->numLists = input_vert->numLists;
    out_vert->flags =  input_vert->flags;

    pddilist = input_vert->ddList;
    pddolist = out_vert->ddList;

    DD_VertPointSize(input_vert->type, inpoint_size);
    DD_VertPointSize(out_vert->type, outpoint_size);

    if ((input_fct) && (input_fct->numFacets > 0)) {
      in_fct = input_fct->facets;
      DDFacetSIZE(input_fct->type, facet_size);
    } else in_fct.pNoFacet = 0;

    for(i = 0; i < input_vert->numLists; i++) {
      pddolist->numPoints = pddilist->numPoints;
      in_pt.ptr = pddilist->pts.ptr;
      MI_ALLOCLISTOFDDPOINT(pddolist,(pddilist->numPoints+1),
                            outpoint_size);
      if (!(out_pt.ptr = pddolist->pts.ptr)) return(BadAlloc);
 
      for (j = 0, in_pt.ptr = pddilist->pts.ptr,
        out_pt.ptr = pddolist->pts.ptr;
        j < pddilist->numPoints; j++) {

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
            in_color =
                &(pddc->Static.attrs->surfaceColour.colour.rgbFloat);
          }
        }

	APPLY_DEPTH_CUEING(pddc->Static.misc.cc_dcue_entry,
		 pt_depth, in_color, out_pt.pRgbFloatClr)

        out_pt.pRgbFloatClr++;

        /*
         * Next normals
         * Normals are derived first from the vertex, second from the
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
      if ((in_fct.pNoFacet)) in_fct.pNoFacet += facet_size;
      pddilist++;
      pddolist++;
    }
    return(Success);
}





