/* $Xorg: miQuadMesh.c,v 1.4 2001/02/09 02:04:10 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/level2/miQuadMesh.c,v 3.8 2001/12/14 19:57:29 dawes Exp $ */

#include "miLUT.h"
#include "ddpex2.h"
#include "ddpex3.h"
#include "PEXErr.h"
#include "miStruct.h"
#include "PEXprotost.h"
#include "misc.h"
#include "miscstruct.h"
#include "miRender.h"
#include "gcstruct.h"
#include "miClip.h"
#include "pexos.h"


/*++
 |
 |  Function Name:	miQuadMesh
 |
 |  Function Description:
 |	 Handles the Quad Mesh OC
 |
 |  Note(s):This routine decomposes Quad Meshes to a series of triangle
 |  	     meshes for rendering
 |
 --*/

ddpex3rtn
miQuadMesh(pRend, pExecuteOC)
/* in */
    ddRendererPtr       pRend;	  		
    miGenericStr       *pExecuteOC;


{

/* Calls */
      extern ocTableType InitExecuteOCTable[];

/* Local variable definitions */
      miQuadMeshStruct  *ddQuad = (miQuadMeshStruct *)(pExecuteOC+1);
      miDDContext	*pddc = (miDDContext *)(pRend->pDDContext);
      
      miGenericStr      *pGStr;
      miTriangleStripStruct 
			*ddTri;			/*pointer to temporary
						 |triangle strip structure
						 |for decomposed QuadMesh
						 |row of cells 
						 */
      miListHeader	*tri_list;

      ddPointUnion	in_pt, out_pt;

      listofddFacet	*tri_facets;

      char 		*quad_fptr,		/* pointers to */
			*tri_fptr;		/* facet data  */

      ddpex3rtn		status;

      int		inpoint_size,
			outpoint_size,
			facet_size,
			row_size,
			edge_offset,
			i,j,
			num_cols,
			num_rows;

/***********************************************************************/

      
  /* Initialize status */

  status = Success;

  /* Allocate space for a Generic Structure Element */

  if (!(pGStr = (miGenericStr *) (xalloc(sizeof(miGenericStr) + 
					 sizeof(miTriangleStripStruct))))) {
      status = BadAlloc;
      goto exit;
  }

  /* Initialize the header data into allocated generic struct */

  pGStr->elementType = PEXOCTriangleStrip;

  /* The length data is ignored by the rendering routine and hence is */
  /* left as whatever GARBAGE that will be present at the alloc time. */

  ddTri = (miTriangleStripStruct *) (pGStr + 1);

  num_cols = (ddQuad->mPts - 1);
  num_rows = (ddQuad->nPts - 1);

  /* At this point, we need to check if the edges are visible or not. */
  /* If they are we need to explicitly add them. This is part of the  */
  /* because they are in the FillArea primitive, and we didn't wish   */
  /* to have special cases all over the code. (particularly structure */
  /* storage and readback on both sides of the wire)  		*/

  DD_VertPointSize(ddQuad->points.type, inpoint_size);

  in_pt.ptr = ddQuad->points.ddList->pts.ptr;
  row_size = (inpoint_size * (num_cols+ 1));
  quad_fptr = (char *)ddQuad->pFacets->facets.pNoFacet;

  for (i = 1; i <= num_rows ; i++) {

    /* All temporary storage must be reallocated
     * each time, since the allocation routines rotate through
     * the headers, and will overwrite any particular header
     * after four calls.  This coould perhaps be avoided by
     * directly using xalloc, but we do it this way for
     * consistency.
     */


    tri_list = MI_NEXTTEMPDATALIST(pddc);	/* get new miListHeader */
    MI_ALLOCLISTHEADER(tri_list, 1);		/* get new ddList attached */

    tri_list->ddList->numPoints = 2 * (num_cols + 1); 
    tri_list->type = ddQuad->points.type;
    if ((pddc->Static.attrs->edges != PEXOff) &&
        (!(DD_IsVertEdge(ddQuad->points.type)))) {
      DD_SetVertEdge(tri_list->type);
      DD_VertOffsetEdge(tri_list->type, edge_offset);
    }
    DD_VertPointSize(tri_list->type, outpoint_size);
    tri_list->flags = ddQuad->points.flags;
    tri_list->numLists = 1;
    MI_ALLOCLISTOFDDPOINT(			/* get vertex storage */
		tri_list->ddList, (2 * (num_cols + 1)), (outpoint_size));
 
    /* Allocate triangle strip facet storage if necessary*/
    if (ddQuad->pFacets) {
      if (ddQuad->pFacets->numFacets > 0) {
        DDFacetSIZE(ddQuad->pFacets->type, facet_size);
        tri_facets = MI_NEXTTEMPFACETLIST(pddc);
        tri_facets->type = ddQuad->pFacets->type;
        MI_ALLOCLISTOFDDFACET(tri_facets, 2 * num_cols, facet_size);
        if (!tri_facets->facets.pFacetRgbFloatN) {
	    status = BadAlloc;
	    goto exit;
	}
        tri_fptr = (char *)tri_facets->facets.pNoFacet;
        tri_facets->numFacets = 2 * num_cols;
        ddTri->pFacets = tri_facets;
      }
      else {
	quad_fptr = 0;
	tri_fptr = 0;
	tri_facets = 0;
	ddTri->pFacets = 0;
      }
 
    } else {
      quad_fptr = 0;
      tri_fptr = 0;
      tri_facets = 0;
      ddTri->pFacets = 0;
    }

    out_pt.ptr =  tri_list->ddList->pts.ptr;

    memcpy( out_pt.ptr, in_pt.ptr, inpoint_size); 
    if (pddc->Static.attrs->edges != PEXOff) {
      *(out_pt.ptr + edge_offset) = ~0; 
    }
    out_pt.ptr += outpoint_size;
 
    memcpy( out_pt.ptr, (in_pt.ptr + row_size), inpoint_size); 
    if (pddc->Static.attrs->edges != PEXOff) {
      *(out_pt.ptr + edge_offset) = ~0; 
      CLEAR_FWD_EDGE(out_pt.ptr, edge_offset);                
    }
    in_pt.ptr += inpoint_size;                                              
    out_pt.ptr += outpoint_size;      

    /* Build up a triangle strip */
    for (j = 1;  j <= num_cols; j++) {

      memcpy( out_pt.ptr, in_pt.ptr, inpoint_size);                             
      if (pddc->Static.attrs->edges != PEXOff) {
        *(out_pt.ptr + edge_offset) = ~0; 
      }
      out_pt.ptr += outpoint_size;                                             
      memcpy( out_pt.ptr, (in_pt.ptr + row_size), inpoint_size); 
      if (pddc->Static.attrs->edges != PEXOff){
        *(out_pt.ptr + edge_offset) = ~0; 
        CLEAR_FWD_EDGE(out_pt.ptr, edge_offset);
      }
      in_pt.ptr += inpoint_size;                                              
      out_pt.ptr += outpoint_size;                                             

      if (ddQuad->pFacets->numFacets > 0) {
        memcpy( tri_fptr, quad_fptr, facet_size);
    	tri_fptr += facet_size;
        memcpy( tri_fptr, quad_fptr, facet_size);
    	tri_fptr += facet_size; 
    	quad_fptr += facet_size;
      } 
    }

    ddTri->points = *tri_list;

    /*fire off the triangle strip routine*/
    if (status = InitExecuteOCTable[(int)(pGStr->elementType)](pRend, pGStr))
      goto exit;
  }
  
  /* Free up allocated space for generic struct and return */

  exit:

  xfree((char *)pGStr);
  return (status);
}
