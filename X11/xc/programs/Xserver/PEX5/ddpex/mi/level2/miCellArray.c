/* $Xorg: miCellArray.c,v 1.4 2001/02/09 02:04:09 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/level2/miCellArray.c,v 3.8 2001/12/14 19:57:20 dawes Exp $ */

#include "mipex.h"
#include "misc.h"
#include "miscstruct.h"
#include "ddpex3.h"
#include "PEXErr.h"
#include "miStruct.h"
#include "PEXprotost.h"
#include "miRender.h"
#include "ddpex2.h"
#include "pexos.h"


/*++
 |
 |  Function Name:	miCellArray
 |
 |  Function Description:
 |	
 |	Draws the outline of the cell array parallelogram using
 |	the current polyline attributes.	
 |
 |  	Note(s): A whole lot-o work needs to be done in order to
 |		 support full color indexing (which is really an
 |		 attempt at texture mapping)
 |
 |      The Parse routine (in pexOCParse) should be enhanced to detect
 |	whether |dR|, |dQ|, dx or dy are of zero length 
 |
 |	
 |
 --*/


ddpex3rtn
miCellArray(pRend, pExecuteOC)
/* in */
  ddRendererPtr		pRend;	  
  miGenericStr	       *pExecuteOC;

  {

/***************************************************************************/

  /* calls */
    extern ocTableType	InitExecuteOCTable[];

  /* Local variable definitions */
    miGenericStr        *pGStr;
    miCellArrayStruct	*ddCell = (miCellArrayStruct *)(pExecuteOC+1);
    miDDContext		*pddc = (miDDContext *)(pRend->pDDContext);
    miListHeader	*input = &ddCell->point;	    
    miPolylineStruct	*output;
    listofddPoint       *pddolist;
    int			point_size, nGridLines, i;
    ddPointUnion	in_pt, out_pt, Ppt, Qpt, Rpt, Spt;
    ddpex3rtn		status;
    ddCoord3D		dRpt, dQpt, x_step, y_step, 
			basept, endpt;
    ddCoord2D		tempQ, tempR;



/*
 *	A Cell Array is defined by three points, P, Q, and R->  This
 *	defines a parallelogram, closed by an implied S
 *
 *		P-----(dR)---->R
 *		 \		\
 *		  \		 \
 *		  (dQ)		  \
 *		    \		   \
 *		     \		    \
 *		      Q--------------S (implied)
 *
 *	S = P + dR + dQ
 */

    if (!(pGStr = (miGenericStr *) (xalloc(sizeof(miGenericStr) +
                                         sizeof(miPolylineStruct))))) 
        return(BadAlloc);

    pGStr->elementType = PEXOCPolylineSet;
 
    output = (miPolylineStruct *) (pGStr + 1);

    /* We need to initialize the maxLists to zero since the MI_ALLOCLISTHEADER
       macro does reallocs only when necessary, based on this field. The
       intent is to use the temp lists of the renderer over & over, but
       in this case we are not getting a header from MI_NEXTTEMPDATALIST */

    output->maxLists = 0;
  
    nGridLines = (ddCell->dx) + (ddCell->dy) + 2;
    MI_ALLOCLISTHEADER(output,MI_ROUND_LISTHEADERCOUNT(nGridLines));

    point_size = sizeof(ddCoord3DL);
    for(i = 0, pddolist = output->ddList; i < nGridLines; i++){
      MI_ALLOCLISTOFDDPOINT(pddolist, 2, point_size);
      pddolist++;
    }

    /* Initialize output listHeader */
    output->type =  DD_3D_POINT;
    output->flags = input->flags;
    output->numLists = nGridLines;

    /* Get input points */
    if (DD_IsVert3D(input->type))
    {
	Ppt.ptr = ddCell->point.ddList->pts.ptr;
	Rpt.ptr = Ppt.ptr + sizeof(ddCoord3D); 
	Qpt.ptr = Rpt.ptr + sizeof(ddCoord3D); 
    }
    else
    {
	Ppt.ptr = ddCell->point.ddList->pts.ptr;
	Spt.ptr = Ppt.ptr + sizeof(ddCoord2D);

	tempR.x = Spt.p2Dpt->x;
	tempR.y = Ppt.p2Dpt->y;
	tempQ.x = Ppt.p2Dpt->x;
	tempQ.y = Spt.p2Dpt->y;

	Rpt.p2Dpt = &tempR;
	Qpt.p2Dpt = &tempQ;
    }

    /* Calculate DQ and DR */
    dQpt.x = Qpt.p2Dpt->x - Ppt.p2Dpt->x;
    dQpt.y = Qpt.p2Dpt->y - Ppt.p2Dpt->y;
    dRpt.x = Rpt.p2Dpt->x - Ppt.p2Dpt->x;
    dRpt.y = Rpt.p2Dpt->y - Ppt.p2Dpt->y;

    /* Calculate step sizes for grid lines */
    x_step.x = (dRpt.x / ddCell->dx);
    x_step.y = (dRpt.y / ddCell->dx);
    y_step.x = (dQpt.x / ddCell->dy);
    y_step.y = (dQpt.y / ddCell->dy);

    /* Add Z component if necessary */
    if DD_IsVert3D(input->type) {
      dQpt.z = Rpt.p3Dpt->z - Ppt.p3Dpt->z;
      dRpt.z = Qpt.p3Dpt->z - Ppt.p3Dpt->z;
      x_step.z = (dRpt.z / ddCell->dx); 
      y_step.z = (dRpt.z / ddCell->dy); 
    } else { 
      x_step.z = 0.0; 
      y_step.z = 0.0; 
    }

    basept.x = Ppt.p2Dpt->x;
    basept.y = Ppt.p2Dpt->y;
    if DD_IsVert3D(input->type) basept.z = Ppt.p3Dpt->z;
      else basept.z = 0.0; 

    pddolist = output->ddList;
    /* "x" lines */
    for(i = 0, out_pt.ptr = pddolist->pts.ptr;
	 i <= ddCell->dy; i++){

      out_pt.ptr = pddolist->pts.ptr;
      endpt.x = basept.x + dRpt.x;
      endpt.y = basept.y + dRpt.y;
      endpt.z = basept.z + dRpt.z;

      memcpy( out_pt.ptr, (char *)&basept, point_size); 
      out_pt.ptr += point_size;
      memcpy( out_pt.ptr, (char *)&endpt, point_size); 
      out_pt.ptr += point_size;

      basept.x += y_step.x;
      basept.y += y_step.y;
      basept.z += y_step.z;

      pddolist->numPoints = 2;
      pddolist++;
    } 

    basept.x = Ppt.p2Dpt->x;
    basept.y = Ppt.p2Dpt->y;
    if DD_IsVert3D(input->type) basept.z = Ppt.p3Dpt->z;
      else basept.z = 0.0;
 
    /* "y" lines */
    for(i = 0; i <= ddCell->dx; i++){
 
      out_pt.ptr = pddolist->pts.ptr;
      endpt.x = basept.x + dQpt.x;
      endpt.y = basept.y + dQpt.y;
      endpt.z = basept.z + dQpt.z; 
      
      memcpy( out_pt.ptr, (char *)&basept, point_size); 
      out_pt.ptr += point_size; 
      memcpy( out_pt.ptr, (char *)&endpt, point_size); 
      out_pt.ptr += point_size;
 
      basept.x += x_step.x;
      basept.y += x_step.y;
      basept.z += x_step.z; 
 
      pddolist->numPoints = 2;
      pddolist++; 
    } 
    output->numLists = nGridLines;
 
    /* Call Polyline routine with output array */
    status = InitExecuteOCTable[(int)(pGStr->elementType)](pRend, pGStr);

    /* clean up memory allocation */

    for(i = 0, pddolist = output->ddList; i < nGridLines; i++){
      xfree((char *)pddolist->pts.ptr);
      pddolist++;
    }
    xfree((char *)output->ddList);
    xfree((char *)pGStr);

    return(status);
} 
