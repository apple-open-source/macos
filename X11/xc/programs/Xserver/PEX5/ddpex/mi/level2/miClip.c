/* $Xorg: miClip.c,v 1.4 2001/02/09 02:04:09 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/level2/miClip.c,v 3.7 2001/12/14 19:57:20 dawes Exp $ */

#include "mipex.h"
#include "misc.h"
#include "miscstruct.h"
#include "PEXErr.h"
#include "miRender.h"
#include "miClip.h"
#include "gcstruct.h"
#include "pexos.h"


/*++
 |
 |  Function Name:	miClipPointList
 |
 |  Function Description:
 |	 Clips each point is a listofddPoint. Clipping here means
 |	 that the point is either copied or not copied to the
 |	 output list.
 |
 |  Note(s):
 |
 --*/

ddpex3rtn
miClipPointList(pddc, vinput, voutput, clip_mode)
/* in */
	miDDContext	*pddc;
        miListHeader    *vinput;
        miListHeader    **voutput;
	ddUSHORT	clip_mode;
{

/* uses */
    char		*in_pt;
    char		*out_pt;
    miListHeader	*output;
    listofddPoint	*pddilist;
    listofddPoint	*pddolist;
    int			num_lists;
    int			vert_count;
    int			num_points;
    int			point_size, clip_code;
    int			j,k, num_passes;
    float		t;
    ddUSHORT		oc, clipflags;
    ddHalfSpace         *MC_HSpace;
	
    /* Vertex data must be homogeneous for clipper */
    if (!(DD_IsVert4D(vinput->type))) return(1);

    /* Reformat clipflags to internal format */
    if (pddc->Dynamic->clipFlags & PEXClipXY)
      clipflags=(MI_CLIP_LEFT | MI_CLIP_RIGHT | MI_CLIP_TOP | MI_CLIP_BOTTOM);
    else clipflags = 0;
    if (pddc->Dynamic->clipFlags & PEXClipFront) clipflags |= MI_CLIP_FRONT;
    if (pddc->Dynamic->clipFlags & PEXClipBack) clipflags |= MI_CLIP_BACK;

    /* Use the pre-defined clip list for output */
    *voutput = output = MI_NEXTTEMPDATALIST(pddc);

    /* Allocate an initial number of headers */
    MI_ALLOCLISTHEADER(output, MI_ROUND_LISTHEADERCOUNT(vinput->numLists))
    if (!output->ddList) return(BadAlloc);

    output->type = vinput->type;
    output->flags =  vinput->flags;

    pddilist = vinput->ddList;
    pddolist = output->ddList;
    DD_VertPointSize(vinput->type, point_size);

    num_lists = 0;

    /* Now, clip each list */
    for (j = 0; j < vinput->numLists; j++) {

      /* Skip list if no points */
      if ((vert_count = pddilist->numPoints) <= 0) {
	pddilist++;
	continue;
      }

      /* Insure sufficient room for each vertex */
      MI_ALLOCLISTOFDDPOINT(pddolist, vert_count, point_size);
      if (!(out_pt = pddolist->pts.ptr)) return(BadAlloc);

      in_pt = pddilist->pts.ptr;

      num_points = 0;


      /* For each vertex, clip a polyline segment */
      while (vert_count--) {

        /* this is really only used for annotation text, so going */
        /* through all the half-spaces for each point is not */
        /* a big deal */

	CLIP_POINT4D(in_pt, oc, clip_mode);

	if (!(oc)) {
	  /* Copy the next point into the clip array */
	  memcpy( out_pt, in_pt, point_size);
	  num_points++;
	  out_pt += point_size;
	}

	/* skip to next point */
	in_pt += point_size;
       }

       /* skip to next list */
       pddilist++;

       /* Don't increment list count unless points were added to last list */
       if (num_points > 0) {
	  pddolist->numPoints = num_points;
	  num_lists++;
	  pddolist++;
       }
    }


    output->numLists = num_lists;

    return (Success);

}


 
/*++
 |
 |  ComputeMCVolume(pRend, pddc)
 |
 |      Compute a modelling coordinate version of the model clipping
 |	volume; 
 |
 --*/
ddpex3rtn
ComputeMCVolume(pRend, pddc)
  ddRendererPtr   	pRend;          /* renderer handle */
  miDDContext       	*pddc; 
{
  extern void     	miMatCopy();
  extern void     	miMatMatInverse();

  ddHalfSpace		*wcHS,		/* world coord half spaces */
			mcHS;		/* model coord half spaces */ 

  int			i, count;
  float			pxform[4][4];	/* point transform */
  float			vxform[4][4];	/* vector transform */
  float			length;
 
 
  /* check to see if already computed */
  if (pddc->Static.misc.flags & MCVOLUMEFLAG) {

    /* Verify inverse transform */
    VALIDATEINVTRMCTOWCXFRM(pddc);
 
    /* Don't want transpose of inverse for point xform! */
    miMatCopy(pddc->Static.misc.inv_tr_mc_to_wc_xform,
		pxform);
    miMatTranspose(pxform);

    /* Want transpose of forward point xform for inverse vector xform */
    miMatCopy(pddc->Dynamic->mc_to_wc_xform, vxform);
    miMatTranspose(vxform);

    count = pddc->Dynamic->pPCAttr->modelClipVolume->numObj;
    wcHS = (ddHalfSpace *)(pddc->Dynamic->pPCAttr->modelClipVolume->pList);

    pddc->Static.misc.ms_MCV->numObj = 0; 

    for(i = 0; i < count; i++) {

        miTransformPoint(&wcHS->point, pxform,
                         &mcHS.point);

        miTransformVector(&wcHS->vector, vxform, &mcHS.vector);

	NORMALIZE_VECTOR(&mcHS.vector, length);
	
	DOT_PRODUCT(&mcHS.vector, &mcHS.point, mcHS.dist);
	
	puAddToList(&mcHS, 1, pddc->Static.misc.ms_MCV);

	++wcHS;

    }

    /* Clear ddc status flag */
    pddc->Static.misc.flags &= ~MCVOLUMEFLAG;
  }
  return (Success);
}

