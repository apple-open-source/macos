/* $Xorg: miPolyLine.c,v 1.4 2001/02/09 02:04:10 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/level2/miPolyLine.c,v 3.7 2001/12/14 19:57:29 dawes Exp $ */

#include "mipex.h"
#include "misc.h"
#include "miscstruct.h"
#include "ddpex3.h"
#include "PEXErr.h"
#include "miStruct.h"
#include "PEXprotost.h"
#include "miRender.h"
#include "gcstruct.h"
#include "ddpex2.h"
#include "miClip.h"
#include "pexos.h"


/*++
 |
 |  Function Name:	miPolyLines
 |
 |  Function Description:
 |	 Handles the Polyline 3D,  Polyline 2D, Polyline set 3D with data ocs.
 |
 |  Note(s):
 |
 --*/

ddpex3rtn
miPolyLines(pRend, pExecuteOC)
/* in */
    ddRendererPtr       pRend;	  /* renderer handle */
    miGenericStr       *pExecuteOC;
{
/* calls */
      ddpex3rtn		miTransform();
      ddpex3rtn		miConvertVertexColors();
      ddpex3rtn		miClipPolyLines();
      ddpex3rtn		miRenderPolyLine();

/* Local variable definitions */
    miDDContext		*pddc = (miDDContext *)(pRend->pDDContext);
    miPolylineStruct	*ddPoly = (miPolylineStruct *)(pExecuteOC+1);
    miListHeader	*input_list = (miListHeader *)ddPoly;
    miListHeader	*color_list,
    			*mc_list,
			*mc_clist,
			*cc_list, 
			*clip_list,
                        *dcue_list,
			*dc_list;
    ddpex3rtn		status;
    ddPointType		out_type;
    ddUSHORT        	clip_mode;      /* view or model clipping */

    /*
     * Convert per-vertex colors to rendering color model.
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

    /* Check for Model clipping */

    if (pddc->Dynamic->pPCAttr->modelClip == PEXClip) {

      ComputeMCVolume(pRend, pddc);	/*  Compute  modelling coord version
                                   	    of clipping volume */
      clip_mode = MI_MCLIP;


      /* Tranform points to 4D for clipping */
      out_type = color_list->type;
      if (status = miTransform(pddc, color_list, &mc_clist,
			      	 ident4x4,
				 ident4x4,
				 DD_SetVert4D(out_type)))
          return (status);


      if (status = miClipPolyLines(pddc, mc_clist, &mc_list, clip_mode)) 
                return(status);

      /* if nothing left, return early */
      if (mc_list->numLists <= 0) return(Success);


    } else mc_list = color_list;

    clip_mode = MI_VCLIP;


    /* Transform and clip the paths created */
    out_type = mc_list->type;
    if (status = miTransform(pddc, mc_list, &cc_list, 
			       pddc->Dynamic->mc_to_cc_xform,
			       NULL4x4,
			       DD_SetVert4D(out_type)))
	  return (status);

    if (status = miClipPolyLines(pddc, cc_list, &clip_list, clip_mode)) 
		return(status);

    /* if nothing left, return early */
    if (clip_list->numLists <= 0) return(Success);

    /* DEPTH CUEING */
    if (pddc->Dynamic->pPCAttr->depthCueIndex) {
      miDepthCuePLine(pRend, clip_list, &dcue_list);
      clip_list = dcue_list;
    }

    /* Transform to DC coordinates */
    out_type = clip_list->type;
    DD_SetVert2D(out_type);
    DD_SetVertShort(out_type);
    if (status = miTransform(pddc, clip_list, &dc_list, 
			      	 pddc->Dynamic->cc_to_dc_xform, 
				 NULL4x4, 
				 out_type))
	  return (status);


    return (pddc->Static.RenderProcs[POLYLINE_RENDER_TABLE_INDEX](pRend, 
								  pddc, 
								  dc_list));
}

/*++
 |
 |  Function Name:	miClipPolyLines
 |
 |  Function Description:
 |	 Handles the Polyline 3D,  Polyline 2D, Polyline set 3D with data ocs.
 |
 |  Note(s):
 |	 This routine modifies both the input and output
 |	 data structures - the input had better not
 |	 be pointing at the CSS data store!
 |
 --*/

ddpex3rtn
miClipPolyLines(pddc, vinput, voutput, clip_mode)
/* in */
	miDDContext	*pddc;
        miListHeader    *vinput;
        miListHeader    **voutput;
    	ddUSHORT        clip_mode;      /* view or model clipping */
{

/* uses */
    ddPointUnion        in_ptP, in_ptQ;
    ddPointUnion        out_pt;
    float               t_ptP, t_ptQ;
    miListHeader	*input, *output, *list1, *list2;
    listofddPoint	*pddilist;
    listofddPoint	*pddolist;
    int			num_lists;
    int			vert_count;
    int			point_size, num_planes;
    int			current_plane,j,k,
			clip_code, pts_in_list;
    ddHalfSpace         *MC_HSpace;
    ddUSHORT            current_clip;
    char                new_list;

	
    /* Vertex data must be homogeneous for view clipping */
    if ((clip_mode == MI_VCLIP) && !(DD_IsVert4D(vinput->type)))
                return(1);


    /* Allocate an initial number of headers */
    list1 = MI_NEXTTEMPDATALIST(pddc);
    MI_ALLOCLISTHEADER(list1, MI_ROUND_LISTHEADERCOUNT(vinput->numLists));      
    if (!list1->ddList) return(BadAlloc);
    list1->type = vinput->type;
    list1->flags =  vinput->flags;
    input = vinput;

    list2 = MI_NEXTTEMPDATALIST(pddc);
    MI_ALLOCLISTHEADER(list2, MI_ROUND_LISTHEADERCOUNT(vinput->numLists));
    if (!list2->ddList) return(BadAlloc);
    list2->type = vinput->type;
    list2->flags =  vinput->flags;

    *voutput = output = list2;

    DD_VertPointSize(vinput->type, point_size);

    num_lists = 0;

    /* Now, clip each list */
 
    /* Get point size so that this works for all point types */
    DD_VertPointSize(vinput->type, point_size);
 
    /* 
     * Each list is now clipped in turn against each (enabled) boundary.
     */
 
 
    if (clip_mode == MI_MCLIP) {
      num_planes = pddc->Static.misc.ms_MCV->numObj;
      MC_HSpace = (ddHalfSpace *)(pddc->Static.misc.ms_MCV->pList);
    }

    else num_planes = 6; /* view clipping to a cube */
 
    for (current_plane = 0; current_plane < num_planes; current_plane++) {
      current_clip = 1 << current_plane;  
 
      num_lists = 0;          /* Restart list counter each pass */
 
      for (j = 0, pddilist = input->ddList, pddolist = output->ddList,
             output->numLists = 0, num_lists = 0, new_list = 1;
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
	pts_in_list = 0;
	clip_code = 0;

                                        /* Aquire two points */
                                        /* and generate clip code */
        in_ptP.ptr = pddilist->pts.ptr;
        COMPUTE_CLIP_PARAMS(in_ptP,t_ptP,0,clip_mode,
                        current_clip,MC_HSpace,clip_code);
        /*
         * Initialize the output array. If the first point is
	 * inside the bounds, load it.
         */

        if(!(clip_code)) {
	  COPY_POINT(in_ptP, out_pt, point_size);
	  ++pts_in_list;
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
              ++pts_in_list;
              break;

            case 1:                   /* P is out, Q is in */
              CLIP_AND_COPY(input->type, in_ptP, t_ptP,
                              in_ptQ, t_ptQ, out_pt);
              out_pt.ptr += point_size;
              COPY_POINT(in_ptQ, out_pt, point_size);
	      out_pt.ptr += point_size;
              pts_in_list = 2;
              break;

            case 2:                   /* P is in, Q is out */
              CLIP_AND_COPY(input->type, in_ptQ, t_ptQ,
                              in_ptP, t_ptP, out_pt);
              out_pt.ptr += point_size;
              ++pts_in_list;
   	      pddolist->numPoints = pts_in_list;
              pddolist++;
              output->numLists++;
              num_lists++;
              MI_ALLOCLISTHEADER(output,
                        MI_ROUND_LISTHEADERCOUNT(output->numLists));
              /* skip to next output list */
              pddolist = output->ddList + num_lists;

              /* Insure sufficient room for remaining verts
                        and degenerate points */
              MI_ALLOCLISTOFDDPOINT(pddolist,
                                ((pddilist->numPoints - k)+1),
                                point_size);

              out_pt.ptr = pddolist->pts.ptr;
              if (!out_pt.ptr) return(BadAlloc);
              pts_in_list = 0; /*start a new list*/ 
              break;

            case 3:                  /* both are out; do nothing */ 
              break;

          }
	  in_ptP.ptr = in_ptQ.ptr;
	  t_ptP = t_ptQ;
	  in_ptQ.ptr += point_size;
          clip_code >>= 1;
	}
        if (pts_in_list > 1) {
          pddolist->numPoints = pts_in_list;
          pddolist++;
          num_lists++;
        }   /* else use the same output list */

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
    *voutput = input;

    return (Success);

}


/*++
 |
 |  DepthCuePLine(pRend, input_vert, output_vert)
 |
 |  Applies depth cueing to the vertex colors in a data list
 |
 --*/
ddpex3rtn
miDepthCuePLine(pRend, input_vert, output_vert)
    ddRendererPtr       pRend;          /* renderer handle */
    miListHeader        *input_vert;    /* input vertex data */
    miListHeader        **output_vert;  /* output vertex data */
{
    miDDContext         *pddc = (miDDContext *)(pRend->pDDContext);
    miListHeader                *out_vert;
    listofddPoint               *pddilist;
    listofddPoint               *pddolist;
    ddPointUnion                in_pt, out_pt;
    ddRgbFloatColour            *in_color;
    int                         point_size, facet_size;
    int                         numPoints;
    int                         i,j,outpoint_size;
    miColourEntry               *pintcolour;
    ddFLOAT                     pt_depth;
    ddULONG                     colourindex;
    ddColourSpecifier           intcolour;
    ddUSHORT                    status;
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
 
    /* Get current line color if appropriate */
    if (!(DD_IsVertColour(input_vert->type)) &&
                (pddc->Static.attrs->lineColour.colourType
                        == PEXIndexedColour)) {
      if ((InquireLUTEntryAddress (PEXColourLUT, pRend->lut[PEXColourLUT],
             pddc->Static.attrs->lineColour.colour.indexed.index,
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
 
      for (j = 0; j < pddilist->numPoints; j++)
        {
        /* First copy over coordinate data */
        pt_depth = in_pt.p4Dpt->z;
        *out_pt.p4Dpt = *in_pt.p4Dpt;
        in_pt.p4Dpt++;
        out_pt.p4Dpt++;
 
        /*
         * Next color
         * Colour is derived first from the vertex, second from the
         * from the current PC attributes.
         */

        if (DD_IsVertColour(input_vert->type)){
          in_color = in_pt.pRgbFloatClr;
          in_pt.pRgbFloatClr++;
        }
        else {
          if (pddc->Static.attrs->lineColour.colourType
                        == PEXIndexedColour)
            in_color = &pintcolour->entry.colour.rgbFloat;
          else in_color =
            &(pddc->Static.attrs->lineColour.colour.rgbFloat);
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
