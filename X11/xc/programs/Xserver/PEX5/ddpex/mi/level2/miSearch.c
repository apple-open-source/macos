/* $Xorg: miSearch.c,v 1.4 2001/02/09 02:04:10 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/level2/miSearch.c,v 3.7 2001/12/14 19:57:30 dawes Exp $ */

#include "miWks.h"
#include "misc.h"
#include "miscstruct.h"
#include "PEXErr.h"
#include "miRender.h"
#include "miStruct.h"
#include "ddpex2.h"
#include "miFont.h"
#include "miText.h"
#include "miClip.h"
#include "pexos.h"

#define MAX(a,b) (((a)>(b))?(a):(b))
#define MIN(a,b) (((a)<(b))?(a):(b))

extern ocTableType InitExecuteOCTable[];

/*
 * Function Name: compute_search_volume
 *
 * Purpose:	Compute the intersection of the clip limits and the
 *		search aperture. The search aperture is defined as
 *              a cube centered around search position and search
 *              distance being the distance between the center and the
 *              faces of the cube.
 * Return:	
 *	search volume.
 */
ddpex2rtn
compute_search_volume(pDDC, search_volume)
/* in */
    miDDContext      *pDDC;

/* out */
register	ddNpcSubvolume	    *search_volume; /* Intersection to use */
{
/* calls */

/* Local variable definitions */
    ddCoord3D         pos;
    ddFLOAT           dis;

    /* Get the search position and distance first */

    pos = pDDC->Static.search.position;
    dis = pDDC->Static.search.distance;

    if (dis <= 0.0) dis = 0.0001; /* Make sure we have the search volume  */
                                  /* collapsing almost to reference point */
                                  /* when the distance is negative or zero*/

    /* Compute a search volume centered around the search position and */
    /* of half width, half height, and half length equal to the search */
    /* distance.                                                       */

    search_volume->minval.x = pos.x - dis;
    search_volume->maxval.x = pos.x + dis;

    search_volume->minval.y = pos.y - dis;
    search_volume->maxval.y = pos.y + dis;

    search_volume->minval.z = pos.z - dis;
    search_volume->maxval.z = pos.z + dis;

    return (Success);
}


/*
 * Function Name: compute_search_volume_xform
 *
 * Purpose:	Compute the transformation that transform the primitive
 *		to be searched from search_volume to CC. Remember that we
 *              will use the standard primitive clipping functions to figure
 *              out whether a given primitive lies within the search aperture.
 * Return:	
 *	 sv_to_cc_xform to be used to figure out search hits.
 */
void
compute_search_volume_xform(search_volume, sv_to_cc_xform)
/* in */
                ddNpcSubvolume       *search_volume;
/* out */
                ddFLOAT              sv_to_cc_xform[4][4];
/* calls */
{
    /* The transformation needed here is to go from search_volume to clip_
     * volume as shown in 2D below. We extend the transform to handle the 3D
     * trivially.
     *
     *            search_volume                          clip_volume
     *            -------------                          -----------
     *
     *               +-----+(c,d)                        +---------+(1,1)
     *               |     |                             |         |
     *               |     |         =======>            |         |
     *               |     |                             |         |
     *          (a,b)+-----+                      (-1,-1)+---------+
     *
     *    sv_to_cc_xform (2D):       2/(c-a)    0     (c+a)/(a-c)
     *                                  0    2/(d-b)  (d+b)/(b-d)
     *                                  0       0          0
     */

    memcpy( (char *)sv_to_cc_xform, (char *)ident4x4, 16 * sizeof(ddFLOAT));

    /* Check for trivial search volume, I.E., a point or a cube with */
    /* zero dimensions. If so, initialize the transform to ? ? ?     */

    if (search_volume->maxval.x == search_volume->minval.x) {

	/* Plug in the special ? ? ? */

	return;
    }

    /* The search volume is NOT trivially a point; it is a finite */
    /* volume for which a transform can be computed to go to CC.  */

    sv_to_cc_xform[0][0] = 
	2.0 / (search_volume->maxval.x - search_volume->minval.x);
    sv_to_cc_xform[1][1] = 
	2.0 / (search_volume->maxval.y - search_volume->minval.y);
    sv_to_cc_xform[2][2] = 
	2.0 / (search_volume->maxval.z - search_volume->minval.z);
    sv_to_cc_xform[0][3] =
	(search_volume->maxval.x + search_volume->minval.x) /
	(search_volume->minval.x - search_volume->maxval.x);
    sv_to_cc_xform[1][3] =
	(search_volume->maxval.y + search_volume->minval.y) /
	(search_volume->minval.y - search_volume->maxval.y);
    sv_to_cc_xform[2][3] =
	(search_volume->maxval.z + search_volume->minval.z) /
	(search_volume->minval.z - search_volume->maxval.z);
}


/*++
 |
 |  Function Name:	miSearchPrimitives
 |
 |  Function Description:
 |       Handles the searching of most primitives in a generic fashion.
 |
 |  Note(s):
 |
 ++*/

ddpex2rtn
miSearchPrimitives(pRend, pExecuteOC)
/* in */
    ddRendererPtr       pRend;	  /* renderer handle */
    miGenericStr       *pExecuteOC;
{
/* calls */

/* Local variable definitions */
      miDDContext	*pDDC = (miDDContext *)(pRend->pDDContext);
      ddNpcSubvolume     sv;
      ddFLOAT            buf1_xform[4][4];

      /* Compute the search volume to do the ISS */

      compute_search_volume (pDDC, &sv);

      /* Get the transform to go from search volume to CC - buf1_xform */
     
      compute_search_volume_xform (&sv, buf1_xform);

      /* Get wc_to_cc_xform = (wc_to_npc_xform * buf1_xform) */

      miMatMult (pDDC->Dynamic->wc_to_cc_xform, 
		 pDDC->Dynamic->wc_to_npc_xform, buf1_xform);

      /* Get mc_to_cc_xform = (mc_to_wc_xform * wc_to_cc_xform) */

      miMatMult (pDDC->Dynamic->mc_to_cc_xform, 
		 pDDC->Dynamic->mc_to_wc_xform,
		 pDDC->Dynamic->wc_to_cc_xform);

      /* Now, call the level 2 rendering function to transform and */
      /* clip the primitive. Note that the level 1 function vector */
      /* now has PICKING routines instead of rendering routines. If*/
      /* a search is detected the GLOBAL FLAG will have been up-   */
      /* dated. Check this flag to determine if anything was indeed*/
      /* searched by the level 1 searching routines.               */

      InitExecuteOCTable[(int)(pExecuteOC->elementType)]
	  (pRend, pExecuteOC);

      /* If successful PICK, set the search status flag */

      if (pDDC->Static.pick.status == PEXOk)
	  pDDC->Static.search.status = PEXFound;  

      return (Success);
}


/*++
 |
 |  Function Name:	miTestSearchGdp3d
 |
 |  Function Description:
 |	 Provides the dummy test routine for searching 3d Gdps.
 |       with data OCs.
 |
 |  Note(s):
 |
 ++*/

ddpex2rtn
miTestSearchGdp3d(pRend, pExecuteOC)
/* in */
    ddRendererPtr       pRend;	  /* renderer handle */
    miGenericStr       *pExecuteOC;
/* out */
{
    ErrorF ("miTestSearchGdp3d\n");
    return (Success);
}


/*++
 |
 |  Function Name:	miTestSearchGdp2d
 |
 |  Function Description:
 |	 Provides the dummy test routine for searching 2d Gdps.
 |       with data OCs.
 |
 |  Note(s):
 |
 ++*/

ddpex2rtn
miTestSearchGdp2d(pRend, pExecuteOC)
/* in */
    ddRendererPtr       pRend;	  /* renderer handle */
    miGenericStr       *pExecuteOC;
/* out */
{
    ErrorF ("miSearchPickGdp2d\n");
    return (Success);
}




