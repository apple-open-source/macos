/* $Xorg: miPck1Prim.c,v 1.4 2001/02/09 02:04:09 xorgcvs Exp $ */
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
not be used in advertising or publicity pertaining to distribution of
the software without specific, written prior permission.

SUN MICROSYSTEMS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT
SHALL SUN MICROSYSTEMS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

*/

#include "miRender.h"
#include "misc.h"
#include "miscstruct.h"
#include "PEXErr.h"
#include "miStruct.h"
#include "miFont.h"
#include "miWks.h"
#include "miText.h"
#include "miClip.h"


/*++
 |
 |  Function Name:	miPick1PolyLine
 |
 |  Function Description:
 |	 Handles the level 1picking of Polyline 3D,  Polyline 2D, 
 |       Polyline 3D with data OCs.
 |
 |  Note(s):
 |
 ++*/

ddpex2rtn
miPick1PolyLine(pRend, pddc, input_list)
/* in */
    ddRendererPtr       pRend;	  /* renderer handle */
    miDDContext        *pddc;
    miListHeader       *input_list;
{
/* calls */

      /* Check if anything is remaining. If so, the pick volume  */
      /* intersects the polyline(s). If not, everything has been */
      /* clipped out. Accordingly, update the global Pick_Flag.  */

      if (input_list->numLists > 0) {
	  pddc->Static.pick.status = PEXOk;
      }
      return (Success);
}


/*++
 |
 |  Function Name:	miPick1Text
 |
 |  Function Description:
 |	 Handles the level 1 picking of Text OCs.
 |
 |  Note(s):
 |
 ++*/

ddpex2rtn
miPick1Text(pRend, pddc, input_list)
/* in */
    ddRendererPtr       pRend;	  /* renderer handle */
    miDDContext        *pddc;
    miListHeader       *input_list;
/* out */
{
/* calls */

      /* Check if anything is remaining. If so, the pick volume  */
      /* intersects the polyline(s). If not, everything has been */
      /* clipped out. Accordingly, update the global Pick_Flag.  */

      if (input_list->numLists > 0) {
	  pddc->Static.pick.status = PEXOk;
      }
      return (Success);
}


/*++
 |
 |  Function Name:	miPick1Marker
 |
 |  Function Description:
 |	 Handles the level 1 picking of Marker OCs.
 |
 |  Note(s):
 |
 ++*/

ddpex2rtn
miPick1Marker(pRend, pddc, input_list)
/* in */
    ddRendererPtr       pRend;	  /* renderer handle */
    miDDContext        *pddc;
    miListHeader       *input_list;
/* out */
{
/* calls */

      /* Check if anything is remaining. If so, the pick volume   */
      /* intersects the polymarker(s). If not, everything has been*/
      /* clipped out. Accordingly, update the global Pick_Flag.   */

      if (input_list->numLists > 0) {
	  pddc->Static.pick.status = PEXOk;
      }
      return (Success);
}


/*
 * Function Name: CheckFillAreaPick
 *
 * Purpose:	Check if the input set of points represents a polygon that
 *              is INSIDE or OUTSIDE of the pick aperture.
 *		
 * Return:	Success if the points represent a polygon that is inside.
 *              Otherwise, return a negative number indicating NO_PICK.
 *	 
 */
static
ddpex2rtn
CheckFAreaPick1 (in_list)
/* in */
    miListHeader        *in_list;
/* out */
{
/* calls */

/* Local variables */

    listofddPoint       *poly_list;
    ddPointUnion         pt_list;
    int                  pt_size;
    ddCoord2D            v1, v2;
    ddSHORT              x_same, y_same;
    int                  i, j;
    int                  vcount, hcount;

    /*----------------------Picking Algorithm-------------------------*/
    /*                                                                */
    /* The first step is to look for trivial acceptance. I.E., the re-*/
    /* maining polygon is partially enclosed by the pick aperture and */
    /* at least one point lies completely within the aperture.        */
    /* If not, we have a possible degenerate case wherein, the edges  */
    /* of the remaining polygon is coinciding with the faces of the   */
    /* pick aperture. This also means that all the edges are either   */
    /* horizontal or vertical. There can be no 3D diagonal edges.     */
    /*                                                                */
    /* The algorithm implemented here uses the odd-even rule. The idea*/
    /* is to treat the input points, a pair at a time as an edge of   */
    /* the polygon remaining. The edge is tested to see if it is hor- */
    /* -izontal, or vertical or diagonal. Note that we use the 3D def-*/
    /* inition of horizontal, vertical, and diagonal here. A horizon- */
    /* edge is an edge parallel to X or Z axis, a vertical edge is one*/
    /* being parallel to only Y axis, and a diagonal edge is one which*/
    /* is NOT parallel to any of the three axes of the 3D aperture.   */
    /*                                                                */
    /* Thus, clearly, any diagonal edge present means that the polygon*/
    /* is intersecting the pick aperture and hence a PICK condition is*/
    /* detected. However, if there are no diagonal edges, then we have*/
    /* a degenerate situation with the vertical or horizontal edges at*/
    /* or on the boundaries of the pick aperture.                     */
    /* Using the odd-even rule it is clear that if there are an odd   */
    /* number of vertical edges either to the left or in front of the */
    /* center of the pick aperture, or to the right or in back of the */
    /* center of the pick aperture, then the pick is sorrounded by the*/
    /* polygon and hence is picked. On the other hand, if the number  */
    /* of vertical edges is even, then  we have the pick aperture OUT-*/
    /* SIDE of the polygon, and thus is not picked. The horizontal    */
    /* edges testing come into picture when there are no vertical ones*/
    /* present. The test once again is to check whether the count of  */
    /* horizontal edges either to the left or front, or right or back */
    /* of the center of the aperture, is even or odd. If odd, detect  */
    /* a PICK, else, detect a NO_PICK.                                */
    /*                                                                */
    /*----------------------------------------------------------------*/

    /* Test for trivial pick case. i.e., at least one point is fully */
    /* contained within the converted pick aperture in CC.           */

    poly_list = in_list->ddList;     /* Initialize poly list pointer */

    DD_VertPointSize (in_list->type, pt_size);   /* Get point size */

    for (i=0; i<in_list->numLists; i++, poly_list++) {

	/* Get the pointer to the next points list */

	pt_list.ptr = poly_list->pts.ptr;

	for (j=0; j<in_list->ddList->numPoints; j++) {

	    /* Update pt_list to point to next point */

	    pt_list.ptr += pt_size;

	    /* Test for containment within the pick aperture */

	    if ((pt_list.p2DSpt->x > -1) &&
		(pt_list.p2DSpt->x < 1) &&
		(pt_list.p2DSpt->y > -1) &&
		(pt_list.p2DSpt->y < 1))

		/* This point is fully within the pick aperture. */
		/* No need to test further. Just detect PICK.    */

		return (Success);
	}
    }

    /* We have a degenerate polygon. Test further to determine if the */
    /* pick aperture is fully or partially enclosed by the polygon.   */

    /* Initialize the vertical and horizontal edge counters */

    vcount = hcount = 0;

    /* Set up a loop for testing edges and counting vertical edges */
    /* and horizontal edges.                                       */

    poly_list = in_list->ddList;     /* Initialize poly list pointer */

    for (i=0; i<in_list->numLists; i++, poly_list++) { /* Do for all Polys */

	/* Get the pointer to the next points list */

	pt_list.ptr = poly_list->pts.ptr;

	for (j=0; j<in_list->ddList->numPoints-1; j++) { /* Do for all edges */

	    /* Get the first vertex of current edge */

	    v1.x = pt_list.p2DSpt->x;
	    v1.y = pt_list.p2DSpt->y;

	    /* Update pt_list to point to next point */

	    pt_list.ptr += pt_size;

	    /* Get the next vertex of current edge */

	    v2.x = pt_list.p2DSpt->x;
	    v2.y = pt_list.p2DSpt->y;

	    /* Test the edge type and update the vcount and the hcount */

	    if (MI_NEAR_ZERO(v1.x-v2.x)) 
		x_same = 1;
	    else
		x_same = 0;

	    if (MI_NEAR_ZERO(v1.y-v2.y)) 
		y_same = 1;
	    else
		y_same = 0;

	    if ((x_same)&&(v1.x > 0))
		/* Edge is parallel to Y axis AND is to the right  */
		/* of the center of pick aperture; Increment vcount*/

		vcount++;

	    if ((y_same)&&(v1.y > 0))
		/* Edge is parallel to X axis AND is to the top of  */
		/* the center of pick aperture; Increment the hcount*/

		hcount++;

	}   /* Loop for all edges of current polygon */

	/* Test if vcount = odd; if so, detect PICK */

	if (vcount%2) {
	    return (Success);
	}
	else {

	    /* Else, if vcount == 0, test if hcount = odd; */
	    /* if so, detect PICK.                         */

	    if ((vcount == 0)&&(hcount%2)) return (Success);
	}

	continue;   /* Try the next polygon for containment test */

    }   /* Loop for all polygons */

    return (-1);   /* Return negative to indicate NO_PICK */
}


/*++
 |
 |  Function Name:	miPick1FillArea
 |
 |  Function Description:
 |	 Handles the level 1 picking of FillArea OCs.
 |
 |  Note(s):
 |
 ++*/

ddpex2rtn
miPick1FillArea(pRend, pddc, input_list, input_facet, shape, noedges)
/* in */
    ddRendererPtr       pRend;	  /* renderer handle */
    miDDContext        *pddc;
    miListHeader       *input_list;
    listofddFacet      *input_facet;
    ddBitmaskShort      shape;
    ddUCHAR             noedges;
/* out */
{
/* calls */

    /* Check for successful polygon pick */

    if (CheckFAreaPick1 (input_list) == Success) {
	pddc->Static.pick.status = PEXOk;
    }
    return (Success);
}


/*++
 |
 |  Function Name:	miPick1TriStrip
 |
 |  Function Description:
 |	 Handles the level 1 picking of Triangle Strip OCs.
 |
 |  Note(s):
 |
 ++*/

ddpex2rtn
miPick1TriStrip(pRend, pddc, input_list, input_facet)
/* in */
    ddRendererPtr       pRend;	  /* renderer handle */
    miDDContext        *pddc;
    miListHeader       *input_list;
    listofddFacet      *input_facet;
/* out */
{
/* calls */

    /* Check for successful triangle strip pick. */
    /* Note that clipper for triangle strips will*/
    /* NOT generate dengenerate cases and hence  */
    /* any remaining vertices will constitute a  */
    /* Pick situation.                           */

    if ((input_list->numLists) > 0) {
	pddc->Static.pick.status = PEXOk;
    }
    return (Success);
}


