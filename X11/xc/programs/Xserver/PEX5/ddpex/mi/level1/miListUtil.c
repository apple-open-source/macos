/* $Xorg: miListUtil.c,v 1.4 2001/02/09 02:04:09 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/level1/miListUtil.c,v 3.7 2001/12/14 19:57:16 dawes Exp $ */

#include "miRender.h"
#include "misc.h"
#include "miscstruct.h"
#include "PEXErr.h"
#include "gcstruct.h"
#include "pexos.h"


/*++
 |
 |  Function Name:      miCopyPath
 |
 |  Function Description:
 |       Copies the data in a path to a second path.
 |
 |  Note(s):
 |
 --*/

ddpex3rtn
miCopyPath(pddc, vinput, voutput, flags)
	miDDContext	*pddc;		/* dd context handle */
	miListHeader	*vinput;
	miListHeader	**voutput;
	int		flags;
{
    miListHeader	*output;
    int			j;
    listofddPoint	*pddilist;
    listofddPoint	*pddolist;
    int			point_size;
    
    /* Use the pre-defined 4D list for output */
    *voutput = output = MI_NEXTTEMPDATALIST(pddc);

    /* Insure sufficient room for each header */
    MI_ALLOCLISTHEADER(output, vinput->numLists)
    if (!output->ddList) return(BadAlloc);

    output->type = vinput->type;
    output->numLists = vinput->numLists;
    output->flags =  vinput->flags;

    pddilist = vinput->ddList;
    pddolist = output->ddList;

    DD_VertPointSize(vinput->type, point_size);

    /* Now, transform each list */
    for(j=0; j < vinput->numLists; j++) {

      if ((pddolist->numPoints = pddilist->numPoints) <= 0) continue;

      /*
       * Insure sufficient room for each vertex 
       * Add one to leave room for possible polygon close.
       */
      MI_ALLOCLISTOFDDPOINT(pddolist,(pddilist->numPoints+1),point_size);
      if (!pddolist->pts.p4Dpt) return(BadAlloc);

      memcpy( (char *)pddolist->pts.p4Dpt, 
	    (char *)pddilist->pts.p4Dpt, 
	    ((int)(pddilist->numPoints))*point_size);

      pddilist++;
      pddolist++;
    }

    return(Success);
}

#define	VERTEX_FLAG (1 << 0)
#define	COLOR_FLAG  (1 << 1)
#define	NORMAL_FLAG (1 << 2)
#define EDGE_FLAG   (1 << 3)
/*++
 |
 |  Function Name:      miFilterPath
 |
 |  Function Description:
 |       Copies the supplied input path to the output path,
 |       but only copying the indicated fields.
 |
 |  Note(s):
 |
 |       fields:  (1 << 0) - point data
 |                (1 << 1) - color data
 |                (1 << 2) - normal data
 |                (1 << 3) - edge data
 |
 |       any combination of the above flags are valid.
 |       if the indicated field doesn not exist, it is ignored.
 |
 --*/

ddpex3rtn
miFilterPath(pddc, vinput, voutput, fields)
	miDDContext	*pddc;		/* dd context handle */
	miListHeader	*vinput;
	miListHeader	**voutput;
	int		fields;
{
    char		*in_pt, *out_pt;
    listofddPoint	*pddilist;
    listofddPoint	*pddolist;
    miListHeader	*output;
    int			in_point_size, out_point_size;
    int			color_offset, normal_offset, edge_offset;
    int			vertex_size, color_size;
    int			i, j;
    
    /* Use the pre-defined 4D list for output */
    *voutput = output = MI_NEXTTEMPDATALIST(pddc);

    /* Insure sufficient room for each header */
    MI_ALLOCLISTHEADER(output, vinput->numLists)
    if (!output->ddList) return(BadAlloc);

    output->type = vinput->type;
    output->numLists = vinput->numLists;
    output->flags =  vinput->flags;

    pddilist = vinput->ddList;
    pddolist = output->ddList;

    if (fields & VERTEX_FLAG) {
	if (DD_IsVertFloat(vinput->type)) {
          if (DD_IsVert2D(vinput->type)) vertex_size = sizeof(ddCoord2D);
          else if (DD_IsVert3D(vinput->type)) vertex_size = sizeof(ddCoord3D);
          else vertex_size = sizeof(ddCoord4D);
        } else {
          if (DD_IsVert2D(vinput->type)) vertex_size = sizeof(ddCoord2DS);
          else vertex_size = sizeof(ddCoord3DS);
        }
    } else DD_UnSetVertCoord(output->type);

    if (fields & COLOR_FLAG) {
	DD_VertOffsetColor(vinput->type, color_offset);
	if (color_offset < 0) fields &= ~COLOR_FLAG;
	else {
	  if (DD_IsVertIndexed(vinput->type)) 
	    color_size = sizeof(ddIndexedColour);
	  else if (DD_IsVertRGB8(vinput->type)) 
	    color_size = sizeof(ddRgb8Colour);
	  else if (DD_IsVertRGB16(vinput->type)) 
	    color_size = sizeof(ddRgb16Colour);
	  else color_size = sizeof(ddRgbFloatColour);
        }
    } else DD_UnSetColour(output->type);

    if (fields & NORMAL_FLAG) {
	DD_VertOffsetNormal(vinput->type, normal_offset);
	if (normal_offset < 0) fields &= ~NORMAL_FLAG;
    } else DD_UnSetVertNormal(output->type);

    if (fields & EDGE_FLAG) {
	DD_VertOffsetEdge(vinput->type, edge_offset);
	if (edge_offset < 0) fields &= ~EDGE_FLAG;
    } else DD_UnSetVertEdge(output->type);

    DD_VertPointSize(vinput->type, in_point_size);
    DD_VertPointSize(output->type, out_point_size);

    /* Now, transform each list */
    for(j=0; j < vinput->numLists; j++) {

      if ((pddolist->numPoints = pddilist->numPoints) <= 0) continue;

      /*
       * Insure sufficient room for each vertex 
       * Add one to leave room for possible polygon close.
       */
      MI_ALLOCLISTOFDDPOINT(pddolist,(pddilist->numPoints+1),out_point_size);
      if (!pddolist->pts.p4Dpt) return(BadAlloc);
      out_pt = (char *)pddolist->pts.p4Dpt;
      in_pt = (char *)pddilist->pts.p4Dpt;

      for (i = 0; i < pddilist->numPoints; i++) {
	 if (fields & VERTEX_FLAG) memcpy( out_pt, in_pt, vertex_size);
	 if (fields & COLOR_FLAG) memcpy( (out_pt+ color_offset), 
					(in_pt + color_offset), 
					color_size);
	 if (fields & NORMAL_FLAG) memcpy( (out_pt+ color_offset), 
					(in_pt + normal_offset), 
					sizeof(ddVector3D));
	 if (fields & EDGE_FLAG) memcpy( (out_pt+ color_offset), 
					(in_pt + edge_offset), 
					sizeof(ddULONG));
	 in_pt += in_point_size;
	 out_pt += out_point_size;
      }

      pddilist++;
      pddolist++;
    }

    return(Success);
}

/*++
 |
 |  Function Name:	miComputeListBounds
 |
 |  Function Description:
 |	 Computes the extents of the supplied list.
 |
 |  Note(s):
 |
 --*/

ddpex3rtn
miComputeListBounds(pddc, vinput, bounds)
/* in */
	miDDContext	*pddc;
        miListHeader    *vinput;
        ddListBounds    *bounds;
{
/* uses */
    char		*in_pt;
    listofddPoint	*pddilist;
    int			vert_count;
    int			point_size;
    int			j;
    char		first = 1;
	
    if (DD_IsVert2D(vinput->type)) {
	bounds->zmin = bounds->zmax = 0.0;
	bounds->wmin = bounds->wmax = 1.0;
    } else if (DD_IsVert3D(vinput->type)) {
	bounds->wmin = bounds->wmax = 1.0;
    } 

    pddilist = vinput->ddList;
    DD_VertPointSize(vinput->type, point_size);

    /* Now, clip each list */

    for (j = 0; j < vinput->numLists; j++) {

	/* Skip list if no points */
	if ((vert_count = pddilist->numPoints) <= 0) {
	  pddilist++;
	  continue;
	}

	in_pt = pddilist->pts.ptr;

	if (first) {
	  bounds->xmin = bounds->xmax = ((ddCoord4D *)in_pt)->x;
	  bounds->ymin = bounds->ymax = ((ddCoord4D *)in_pt)->y;
	  if (DD_IsVert3D(vinput->type)) 
	    bounds->zmin = bounds->zmax = ((ddCoord4D *)in_pt)->z;
	  else if (DD_IsVert4D(vinput->type)) 
	    bounds->wmin = bounds->wmax = ((ddCoord4D *)in_pt)->w;
	  first = 0;
	}

	/* For each vertex, clip a polyline segment */
	while (vert_count--) {

	  if (((ddCoord4D *)in_pt)->x < bounds->xmin) 
	    bounds->xmin = ((ddCoord4D *)in_pt)->x;
	  if (((ddCoord4D *)in_pt)->x > bounds->xmax) 
	    bounds->xmax = ((ddCoord4D *)in_pt)->x;
	  if (((ddCoord4D *)in_pt)->y < bounds->ymin) 
	    bounds->ymin = ((ddCoord4D *)in_pt)->y;
	  if (((ddCoord4D *)in_pt)->y > bounds->ymax) 
	    bounds->ymax = ((ddCoord4D *)in_pt)->y;

	  if (DD_IsVert3D(vinput->type)) {
	    if (((ddCoord4D *)in_pt)->z < bounds->zmin) 
		bounds->zmin = ((ddCoord4D *)in_pt)->z;
	    if (((ddCoord4D *)in_pt)->z > bounds->zmax) 
		bounds->zmax = ((ddCoord4D *)in_pt)->z;

	  } else if (DD_IsVert4D(vinput->type)) {
	    if (((ddCoord4D *)in_pt)->z < bounds->zmin) 
		bounds->zmin = ((ddCoord4D *)in_pt)->z;
	    if (((ddCoord4D *)in_pt)->z > bounds->zmax) 
		bounds->zmax = ((ddCoord4D *)in_pt)->z;
	    if (((ddCoord4D *)in_pt)->w < bounds->wmin) 
		bounds->wmin = ((ddCoord4D *)in_pt)->w;
	    if (((ddCoord4D *)in_pt)->w > bounds->wmax) 
		bounds->wmax = ((ddCoord4D *)in_pt)->w;
	  }

	  in_pt += point_size;
	}

       /* skip to next list */
       pddilist++;

    }

    return (Success);
}
