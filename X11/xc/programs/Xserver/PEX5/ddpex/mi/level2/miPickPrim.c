/* $Xorg: miPickPrim.c,v 1.4 2001/02/09 02:04:10 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/level2/miPickPrim.c,v 3.8 2001/12/14 19:57:29 dawes Exp $ */

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
extern int atx_el_to_path();
extern void text2_xform();
extern void text3_xform();

/*
 * Function Name: compute_pick_volume
 *
 * Purpose:	Compute the intersection of the clip limits and the
 *		pick aperture. PEX-SI's pick aperture can be a DC_HitBox
 *              or an NPC_HitVolume. However, for pick correllation purposes,
 *              the pick aperture will always be a NPC subvolume. i.e., the
 *              DC_HitBox has been mapped into NPC. 
 * Return:	
 *	pick volume, if any.
 */
ddpex2rtn
compute_pick_volume(aperture, view, pDDC, pick_volume)
/* in */
register	ddNpcSubvolume	    *aperture;	  /* NPC Pick Aperture */
 		ddViewEntry	    *view;	  /* View clipping info.  
						   * view->clipLimits contains
						   * the desired clip limits */
                miDDContext         *pDDC;        /* Pointer to DDContext */
/* out */
register	ddNpcSubvolume	    *pick_volume; /* Intersection to use */
{
    register	ddNpcSubvolume	    *clip_limit;
                ddUSHORT             all_clipped;
                ddCoord4D            NPC_Min, NPC_Max;
/* calls */

    all_clipped = 0;
    clip_limit = &(view->clipLimits);

    NPC_Max.x = clip_limit->maxval.x;
    NPC_Max.y = clip_limit->maxval.y;
    NPC_Max.z = clip_limit->maxval.z;
    NPC_Max.w = 1.0;
    NPC_Min.x = clip_limit->minval.x;
    NPC_Min.y = clip_limit->minval.y;
    NPC_Min.z = clip_limit->minval.z;
    NPC_Min.w = 1.0;

    if (view->clipFlags != 0) { 
	/*
	 * only test intersection of volumes if any view clipping flags are on
	 */
	all_clipped =
	    ( (aperture->minval.x > NPC_Max.x) ||
	      (aperture->minval.y > NPC_Max.y) ||
	      (aperture->minval.z > NPC_Max.z) ||
	      (aperture->maxval.x < NPC_Min.x) ||
	      (aperture->maxval.y < NPC_Min.y) ||
	      (aperture->maxval.z < NPC_Min.z) );
	
	if (all_clipped) {
	    /*
	     * the intersection of the volumes is null;
	     * everything is always clipped.
	     */
	    return (all_clipped);  /* 
				    * I.E., trivial reject situation, nothing
				    * will be picked 
				    */
	}
    } 

    /* look at X-Y */
    if (view->clipFlags >= PEXClipXY) {
	/* not clipping to the clip limit so use all of aperture */
	pick_volume->minval.x = aperture->minval.x;
	pick_volume->minval.y = aperture->minval.y;
	pick_volume->maxval.x = aperture->maxval.x;
	pick_volume->maxval.y = aperture->maxval.y;
    }
    else {
	pick_volume->minval.x = MAX(aperture->minval.x, NPC_Min.x);
	pick_volume->minval.y = MAX(aperture->minval.y, NPC_Min.y);
	pick_volume->maxval.x = MIN(aperture->maxval.x, NPC_Max.x);
	pick_volume->maxval.y = MIN(aperture->maxval.y, NPC_Max.y);
    }

    /* look at Z */
    if (view->clipFlags >= PEXClipBack)
	pick_volume->maxval.z = aperture->maxval.z;
    else
	pick_volume->maxval.z = MIN(aperture->maxval.z, NPC_Max.z);

    if (view->clipFlags >= PEXClipFront)
	pick_volume->minval.z = aperture->minval.z;
    else
	pick_volume->minval.z = MAX(aperture->minval.z, NPC_Min.z);

	return (Success);
}


/*
 * Function Name: compute_pick_volume_xform
 *
 * Purpose:	Compute the transformation that transform the primitive
 *		to be picked from pick_volume to CC. Remember that we
 *              will use the standard primitive clipping functions to figure
 *              out whether a given primitive lies within the pick aperture.
 * Return:	
 *	 pv_to_cc_xform to be used to figure out pick hits.
 */
void
compute_pick_volume_xform(pick_volume, pv_to_cc_xform)
/* in */
                ddNpcSubvolume       *pick_volume;
/* out */
                ddFLOAT              pv_to_cc_xform[4][4];
/* calls */
{
    /* The transformation needed here is to go from pick_volume to clip_volume
     * as shown in 2D below. We extend the transform to handle the 3D case
     * trivially.
     *
     *            pick_volume                            clip_volume
     *            -----------                            -----------
     *
     *               +-----+(c,d)                        +---------+(1,1)
     *               |     |                             |         |
     *               |     |         =======>            |         |
     *               |     |                             |         |
     *          (a,b)+-----+                      (-1,-1)+---------+
     *
     *    pv_to_cc_xform (2D):       2/(c-a)    0     (c+a)/(a-c)
     *                                  0    2/(d-b)  (d+b)/(b-d)
     *                                  0       0          0
     */

    memcpy( (char *)pv_to_cc_xform, (char *)ident4x4, 16 * sizeof(ddFLOAT));
    pv_to_cc_xform[0][0] = 
	2.0 / (pick_volume->maxval.x - pick_volume->minval.x);
    pv_to_cc_xform[1][1] = 
	2.0 / (pick_volume->maxval.y - pick_volume->minval.y);
    pv_to_cc_xform[2][2] = 
	2.0 / (pick_volume->maxval.z - pick_volume->minval.z);
    pv_to_cc_xform[0][3] =
	(pick_volume->maxval.x + pick_volume->minval.x) /
	(pick_volume->minval.x - pick_volume->maxval.x);
    pv_to_cc_xform[1][3] =
	(pick_volume->maxval.y + pick_volume->minval.y) /
	(pick_volume->minval.y - pick_volume->maxval.y);
    pv_to_cc_xform[2][3] =
	(pick_volume->maxval.z + pick_volume->minval.z) /
	(pick_volume->minval.z - pick_volume->maxval.z);
}


/*
 * Function Name: convert_dcHitBox_to_npc
 *
 * Purpose:	Convert a dcHitBox into an equivalent NPCHitVolume. This
 *		is to facilitate picking to be done always in NPC.
 * Return:	
 *	 NPCHitVolume to be used as the aperture.
 */
void
convert_dcHitBox_to_npc (pRend, aperture)
/* in */
    ddRendererPtr       pRend;	  /* renderer handle */
/* out */
    ddPC_NPC_HitVolume   *aperture;
{
/* calls */
      void		miTransformPoint();

/* Local variable definitions */
    miDDContext       *pDDC = (miDDContext *)(pRend->pDDContext);
    ddCoord4D          DC_Min, DC_Max, NPC_Min, NPC_Max;
    ddFLOAT            inv_xform[4][4];
    ddPC_DC_HitBox     dcHitBox;

    /* Get the DC pick aperture first */

    dcHitBox = pDDC->Static.pick.input_rec.dc_hit_box;

    /* Figure out the square aperture centered around the DC_HitBox */
    /* position and sides along X and Y equal to twice the distance */
    /* specified in the DC_HitBox.                                  */

    DC_Min.x = (ddFLOAT)(dcHitBox.position.x - dcHitBox.distance);
    DC_Min.y = (ddFLOAT)(dcHitBox.position.y - dcHitBox.distance);
    DC_Min.z = 0.0;       /* This is a DONT CARE value */
    DC_Min.w = 1.0;
    DC_Max.x = (ddFLOAT)(dcHitBox.position.x + dcHitBox.distance);
    DC_Max.y = (ddFLOAT)(dcHitBox.position.y + dcHitBox.distance);
    DC_Max.z = 0.0;       /* This is a DONT CARE value */
    DC_Max.w = 1.0;

    /* Now get the inverse viewport transform to transform the DC_HitBox */
    /* into NPC_HitVolume.                                               */

    memcpy( (char *)inv_xform, (char *)pDDC->Static.misc.inv_vpt_xform, 
		16*sizeof(ddFLOAT));

    /* Compute the corners of the DC_HitBox in NPC */

    miTransformPoint (&DC_Min, inv_xform, &NPC_Min);
    miTransformPoint (&DC_Max, inv_xform, &NPC_Max);

    /* Use the z coordinates of the current NPC subvolume in use to */
    /* set up the DC aperture as an equivalent NPC sub-volume.      */

    aperture->minval.x = NPC_Min.x;
    aperture->minval.y = NPC_Min.y;
    aperture->minval.z = pRend->viewport.minval.z;
    aperture->maxval.x = NPC_Max.x;
    aperture->maxval.y = NPC_Max.y;
    aperture->maxval.z = pRend->viewport.maxval.z;
}


/*
 * Function Name: ClipNPCPoint4D
 *
 * Purpose:	Dtermine if the 4D NPC point is within the current NPC
 *		sub-volume and set the outcode accordingly.
 * Return:	
 *	outcode to indicate whether the 4D point is in or out.
 */
static ddpex2rtn
ClipNPCPoint4D (pRend, in_pt, oc)
/* in */
    ddRendererPtr       pRend;	  /* renderer handle */
    ddCoord4D          *in_pt;
/* out */
    ddUSHORT           *oc;
{
/* Local variables */
    ddCoord3D          npc_pt;
    ddNpcSubvolume    *cliplimits;
    int                j;
    ddUSHORT 	       status;
    miViewEntry       *view_entry;
    ddUSHORT           cur_index;

    /* Get the actual 3D point by dividing by w first */

    npc_pt.x = (in_pt->x)/(in_pt->w);
    npc_pt.y = (in_pt->y)/(in_pt->w);
    npc_pt.z = (in_pt->z)/(in_pt->w);

    /* Get the next defined view entry from the View LUT */

    cur_index = ((miDDContext *)pRend->pDDContext)->Dynamic
	          ->pPCAttr->viewIndex;

    if ((InquireLUTEntryAddress (PEXViewLUT, pRend->lut[PEXViewLUT],
				 cur_index, &status, (ddPointer *)&view_entry))
	== PEXLookupTableError)
	return (PEXLookupTableError);

    /* Get the pointer to current NPC subvolume clip limits */

    cliplimits = &(view_entry->entry.clipLimits);

    *oc = 0;              /* Initialize oc to NOT CLIPPED state */

    /* Compare the npc_pt against these cliplimits and set the oc */

    if (npc_pt.x < cliplimits->minval.x) 
	*oc |= MI_CLIP_LEFT;
    else if (npc_pt.x > cliplimits->maxval.x)
	*oc |= MI_CLIP_RIGHT;
    if (npc_pt.y < cliplimits->minval.y) 
	*oc |= MI_CLIP_BOTTOM;
    else if (npc_pt.y > cliplimits->maxval.y)
	*oc |= MI_CLIP_TOP;
    if (npc_pt.z < cliplimits->minval.z) 
	*oc |= MI_CLIP_FRONT;
    else if (npc_pt.z > cliplimits->maxval.z)
	*oc |= MI_CLIP_BACK;
}


/*++
 |
 |  Function Name:	miPickAnnoText2D
 |
 |  Function Description:
 |	 Handles the picking of Annotation text 2D ocs.
 |
 |  Note(s):
 |
 --*/

ddpex2rtn
miPickAnnoText2D(pRend, pExecuteOC)
/* in */
    ddRendererPtr       pRend;	  /* renderer handle */
    miGenericStr       *pExecuteOC;
/* out */
{
    /* local */
    miAnnoText2DStruct	*ddText = (miAnnoText2DStruct *)(pExecuteOC+1);
    miTextElement       text_el;            /* text element */
    ddUSHORT            numEncodings = ddText->numEncodings;
    ddCoord2D           *pOrigin = ddText->pOrigin;  /* string origin */
    ddCoord2D           *pOffset = ddText->pOffset;
    pexMonoEncoding     *pText = ddText->pText;	  /* text string */

/* calls */
    extern ddpex3rtn    miTransform();
    extern ddpex3rtn    miClipPolyLines();
    extern void		miTransformPoint();

/* Define required temporary variables */

    ddPC_NPC_HitVolume aperture;
    ddNpcSubvolume     pv;
    miViewEntry       *view_entry;
    ddViewEntry       *view;
    ddULONG            numChars; /* Needed for xalloc */
    pexMonoEncoding   *pMono;
    ddCoord2D          align;    /* alignment info */
    ddFLOAT            tc_to_npc_xform[4][4];
    ddFLOAT            buf_xform[4][4], buf1_xform[4][4];
    ddFLOAT            buf2_xform[4][4];
    miDDContext       *pDDC, *pddc;
    ddFLOAT            exp, tx, ty;
    ddFLOAT            ptx, pty, ptx_first, pty_first;
    int                i, j, k;
    int                count;  /* Count of characters to be picked */
    ddFLOAT            ei0npc, ei1npc, ei3npc;
    miCharPath        *save_ptr;
    miListHeader      *cc_path, *clip_path;
    listofddPoint     *sp;
    XID		       temp;
    int		       status;
    ddUSHORT           aflag, LUTstatus;
    ddCoord4D          MC_Origin, CC_Origin, NPC_Origin;
    ddUSHORT           oc;         /* Outcode for 4D point clipping */
    ddUSHORT           Pick_Flag, cur_index;

    /* Get the DDContext handle for local use */

    pddc = pDDC = (miDDContext *)pRend->pDDContext;

    /* Transform and clip the text origin first to see if any picking */
    /* needs to be done at all. If the NPC subvolume does not contain */
    /* the origin, the annotation text is not picked.                 */

    MC_Origin.x = pOrigin->x;
    MC_Origin.y = pOrigin->y;
    MC_Origin.z = 0.0;
    MC_Origin.w = 1.0;

    if (pDDC->Dynamic->pPCAttr->modelClip == PEXClip) {

      ComputeMCVolume(pRend, pddc);	/*  Compute  modelling coord version
					    of clipping volume */
      CLIP_POINT4D(&MC_Origin, oc, MI_MCLIP);

      if (oc) {
	  pDDC->Static.pick.status = PEXNoPick;
	  return (Success); /* origin model clipped out */
      }
    }

    /* Get the current view index and the corresponding transforms */

    cur_index = pDDC->Dynamic->pPCAttr->viewIndex;

    if ((InquireLUTEntryAddress (PEXViewLUT, pRend->lut[PEXViewLUT],
				 cur_index, &LUTstatus, (ddPointer *)&view_entry))
	== PEXLookupTableError)
	return (PEXLookupTableError);

    /* Compute the mc_to_npc for the current view */

    miMatMult (pDDC->Dynamic->mc_to_npc_xform,
	       pDDC->Dynamic->mc_to_wc_xform,
	       view_entry->vom);

    miTransformPoint (&MC_Origin, pDDC->Dynamic->mc_to_npc_xform,
		      &NPC_Origin);

    if ((ClipNPCPoint4D (pRend, &NPC_Origin, &oc)) == PEXLookupTableError)
	return (PEXLookupTableError);
    if (oc) {
	pDDC->Static.pick.status = PEXNoPick;
	return (Success);  /* Don't pick anything; origin clipped out */
    }


    /* Keep the NPC_Origin computed above for later use */

    /* Get the pick aperture and convert into NPC, if required. */

    if (pDDC->Static.pick.type == PEXPickDeviceDC_HitBox) {

	/* Convert the dcHitBox into a NPCHitVolume */

	convert_dcHitBox_to_npc (pRend, &aperture);
    }
    else {

	/* Copy data straight from the NPC pick aperture input record */

	aperture = pDDC->Static.pick.input_rec.npc_hit_volume;
    }

    /* Set the annotation text flag to one */

    aflag = 1;

    /* Determine the total number of characters in the ISTRING */

    numChars = 0;
    pMono = pText;
    for (i=0; i<numEncodings; i++) {
      int bytes = pMono->numChars * ((pMono->characterSetWidth == PEXCSByte) ?
	  sizeof(CARD8) : ((pMono->characterSetWidth == PEXCSShort) ?
	  sizeof(CARD16) : sizeof(CARD32)));
      numChars += (ddULONG)pMono->numChars;
      pMono = (pexMonoEncoding *) ((char *) (pMono + 1) +
	  bytes + PADDING (bytes));
    }

    if (numChars == 0)
    {
	pDDC->Static.pick.status = PEXNoPick;
	return (Success);
    }


    /* Convert text string into required paths */

    if ((status = atx_el_to_path (pRend, pDDC, numEncodings, pText,
	numChars, &text_el, &align, &count)) != Success) {
      return (status);
    }

    /* Compute the required Character Space to Modelling Space Transform */

    text2_xform (pOrigin, pDDC->Static.attrs, &align, text_el.xform, aflag);

    /* Set up the new composite transform first. Note that in the case */
    /* of annotation text, only the text origin is transformed by the  */
    /* complete pipeline transform. The text itself is affected only by*/
    /* the transformed origin in NPC, the NPC offset , npc_to_cc, and  */
    /* the workstation transform.                                      */

    /* Now compute the initial composite transform for the first char.  */
    /* The required transforms for characters are - text space to model */
    /* space transform, transformation of the annotation text origin, if*/
    /* any. Note the ABSENCE of npc to cc transform here because of the */
    /* PICKING as opposed to rendering.                                 */

    /* Get the translation due to the transformation of the annotation  */
    /* text origin by mc_to_npc_xform into buf1_xform.                  */

    memcpy( (char *)buf1_xform, (char *) ident4x4, 16 * sizeof(ddFLOAT));
    buf1_xform[0][3] += NPC_Origin.x - MC_Origin.x;
    buf1_xform[1][3] += NPC_Origin.y - MC_Origin.y;

    miMatMult (buf2_xform, text_el.xform, buf1_xform);

    /* Add the offset in NPC */

    buf2_xform[0][3] += pOffset->x;
    buf2_xform[1][3] += pOffset->y;

    /* Pick the paths in text_el as polylines */

    /* Get the current character expansion factor */

    exp = ABS((ddFLOAT)pDDC->Static.attrs->charExpansion);

    /* Save the pointer to the beginning of the character data */

    save_ptr = text_el.paths;

    Pick_Flag = 0;   /* Initialize flag to indicate NO_PICK */

    /* Get the current defined view entry from the View LUT */

    view = &(view_entry->entry);

    /* Compute the intersection of the pick aperture with the NPC */
    /* sub-volume defined for the current view.                   */

    if (compute_pick_volume (&aperture, view, pDDC, &pv)) {

	/* We have NO intersection between the pick aperture  */
	/* and the NPC subvolume defined for the current view */

	goto TextHit;      /* NoPick, Skip everything else */
    }

    /* Get the transform to go from pick volume to CC - buf1_xform */
     
    compute_pick_volume_xform (&pv, buf1_xform);

    /* Do for all characters (paths) in the text_el */

    /* Initialize the previous translation components */

    ptx = pty = 0.0;

    for (k=0; k<count; k++) {  /* Pick characters one by one */

	/* Check if the character is not renderable, for e.g., space */
	/* char. If so just skip to next character in the ISTRING and*/
	/* continue.                                                 */

	if (!(text_el.paths->path->ddList)) {
	    ptx = (ddFLOAT)(((text_el.paths)->trans).x);
	    pty = (ddFLOAT)(((text_el.paths)->trans).y);
	    text_el.paths++;
	    continue;
	}

	/* Modify the composite transform by the previous translation */
	/* and the current scaling in x realizing the char expansion  */

	tx = ptx;
	ty = pty;

	ptx = (ddFLOAT)(((text_el.paths)->trans).x);
	pty = (ddFLOAT)(((text_el.paths)->trans).y);

	/* Check to see if this is the very first character and the */
	/* text path is Up or Down. If so, we need to modify tx by  */
	/* first character translation to align with the rest of the*/
	/* characters in the string.                                */

	if ((pDDC->Static.attrs->atextPath == PEXPathUp ||
	     pDDC->Static.attrs->atextPath == PEXPathDown) && k == 0)
	    tx += ptx;

	/* NOTE THAT THE ABOVE COMPUTATION WILL ONLY WORK FOR THE */
	/* FIRST CHARACTER IN THE STRING. ptx FOR ALL OTHERS WILL */
	/* BE RELATIVE TO THE TEXT ORIGIN AND SO WILL NOT GIVE THE*/
	/* REQUIRED EFFECTIVE CHARACTER WIDTH. HOWEVER, THIS IS   */
	/* NOT A PROBLEM HERE SINCE WE NEED THIS SPECIAL CASE ONLY*/
	/* FOR THE FIRST CHARACTER.                               */
	/*                                                        */
	/* FURTHER, NOTE THAT ptx WILL BE NEGATIVE AND HENCE USE  */
	/* OF +=                                                  */

	if (k == 0) {
	    ptx_first = ptx; /* Get the first character translation */

	    /* Adjust the translation by character spacing factor to*/
	    /* get just the character width.                        */

	    ptx_first += (pDDC->Static.attrs->charSpacing) * 
		          FONT_COORD_HEIGHT;

	    pty_first = pty; /* Save first character height */

	    /* Adjust the translation by character spacing factor to*/
	    /* get just the character height.                       */

	    pty_first += (pDDC->Static.attrs->charSpacing) * 
		          FONT_COORD_HEIGHT;
	}

	/* Check to see if the text path is Left. If so, we need   */
	/* to modify tx by the first character width so as to start*/
	/* the string to the left of the text origin.              */

	if (pDDC->Static.attrs->atextPath == PEXPathLeft)
	    tx += ptx_first;

	/* Buffer the tc_to_npc_xform first */

	memcpy( (char *)tc_to_npc_xform, (char *)buf2_xform,16*sizeof(ddFLOAT));

	/* Apply the per character translation and scaling by */
	/* directly modifying the concerned matrix elements.  */

	for (i=0; i<4; ++i) {
	    /* Buffer the element values */
	    ei0npc = tc_to_npc_xform[i][0];
	    ei1npc = tc_to_npc_xform[i][1];
	    ei3npc = tc_to_npc_xform[i][3];
	    /* Modify the transform */
	    tc_to_npc_xform[i][0] = exp * ei0npc;
	    tc_to_npc_xform[i][3] = tx * ei0npc + ty * ei1npc + ei3npc;
	}

	/* Get buf_xform = (tc_to_npc_xform * buf1_xform) */

	miMatMult (buf_xform, tc_to_npc_xform, buf1_xform);

	/* Transform and clip the paths corresponding to current */
	/* character.                                            */

	if (status = miTransform(pDDC, text_el.paths->path, &cc_path, 
				 buf_xform,
				 NULL4x4,
				 DD_HOMOGENOUS_POINT))
	    return (status);

	/* Now pass the paths through the line clipper to see if */
	/* it lies within the pick aperture. If anything remains,*/
	/* then return a PICK. Otherwise, return a NO_PICK.      */

	if (status = miClipPolyLines (pDDC, cc_path, &clip_path,
				      MI_VCLIP)) {

	    /* BadAlloc, or NOT 4D points */
	 
	    return (status);
	}
	else { 
	    /* Check if anything is remaining. If so, the pick volume */
	    /* intersects the text string. If not, this char has been */
	    /* clipped out. Accordingly, update the Pick_Flag.        */

	    if (clip_path->numLists > 0) {
		Pick_Flag = 1;
		goto TextHit;
	    }
	}

	/* Update the pointer to next character */

	text_el.paths++;

    }   /* Loop for all characters in the text string */

    TextHit:

    if (Pick_Flag)

	/* Update the ddContext pick status to PICK */

	pDDC->Static.pick.status = PEXOk;
    else
	/* Update the ddContext pick status to NO_PICK */

	pDDC->Static.pick.status = PEXNoPick;

    /* Free up space allocated for text stroke data */

    xfree ((char *)save_ptr);

    return (Success);
}


/*++
 |
 |  Function Name:	miPickAnnoText3D
 |
 |  Function Description:
 |	 Handles the picking of Annotation text 3D ocs.
 |
 |  Note(s):
 |
 --*/

ddpex2rtn
miPickAnnoText3D(pRend, pExecuteOC)
/* in */
    ddRendererPtr       pRend;	  /* renderer handle */
    miGenericStr       *pExecuteOC;
/* out */
{
    /* local */
    miAnnoTextStruct	*ddText = (miAnnoTextStruct *)(pExecuteOC+1);
    miTextElement       text_el;            /* text element */
    ddUSHORT            numEncodings = ddText->numEncodings;
    ddCoord3D           *pOrigin = ddText->pOrigin;  /* string origin */
    ddCoord3D           *pOffset = ddText->pOffset;
    pexMonoEncoding     *pText = ddText->pText;	  /* text string */

/* calls */
    extern ddpex3rtn    miTransform();
    extern ddpex3rtn    miClipPolyLines();
    extern void		miTransformPoint();

/* Define required temporary variables */

    ddPC_NPC_HitVolume aperture;
    ddNpcSubvolume     pv;
    miViewEntry       *view_entry;
    ddViewEntry       *view;
    ddULONG            numChars; /* Needed for xalloc */
    pexMonoEncoding   *pMono;
    ddCoord2D          align;    /* alignment info */
    ddFLOAT            tc_to_npc_xform[4][4];
    ddFLOAT            buf_xform[4][4], buf1_xform[4][4];
    ddFLOAT            buf2_xform[4][4];
    miDDContext       *pDDC, *pddc;
    ddFLOAT            exp, tx, ty;
    ddFLOAT            ptx, pty, ptx_first, pty_first;
    int                i, j, k;
    int                count;  /* Count of characters to be picked */
    ddFLOAT            ei0npc, ei1npc, ei3npc;
    miCharPath        *save_ptr;
    miListHeader      *cc_path, *clip_path;
    listofddPoint     *sp;
    XID		       temp;
    int		       status;
    ddUSHORT           aflag, LUTstatus;
    static ddVector3D   Directions[2] = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0};
    ddCoord3D           *pDirections = (ddCoord3D *)Directions;
    ddCoord4D          MC_Origin, CC_Origin, NPC_Origin;
    ddUSHORT           oc;         /* Outcode for 4D point clipping */
    ddUSHORT           Pick_Flag, cur_index;

    /* Get the DDContext handle for local use */

    pddc = pDDC = (miDDContext *)pRend->pDDContext;

    /* Transform and clip the text origin first to see if any picking */
    /* needs to be done at all. If the NPC subvolume does not contain */
    /* the origin, the annotation text is not picked.                 */

    MC_Origin.x = pOrigin->x;
    MC_Origin.y = pOrigin->y;
    MC_Origin.z = pOrigin->z;
    MC_Origin.w = 1.0;

    if (pDDC->Dynamic->pPCAttr->modelClip == PEXClip) {

      ComputeMCVolume(pRend, pddc);	/*  Compute  modelling coord version
					    of clipping volume */
      CLIP_POINT4D(&MC_Origin, oc, MI_MCLIP);

      if (oc) {
	  pDDC->Static.pick.status = PEXNoPick;
	  return (Success); /* origin model clipped out */
      }
    }

    /* Get the current view index and the corresponding transforms */

    cur_index = pDDC->Dynamic->pPCAttr->viewIndex;

    if ((InquireLUTEntryAddress (PEXViewLUT, pRend->lut[PEXViewLUT],
				 cur_index, &LUTstatus, (ddPointer *)&view_entry))
	== PEXLookupTableError)
	return (PEXLookupTableError);

    /* Compute the mc_to_npc for the current view */

    miMatMult (pDDC->Dynamic->mc_to_npc_xform,
	       pDDC->Dynamic->mc_to_wc_xform,
	       view_entry->vom);

    miTransformPoint (&MC_Origin, pDDC->Dynamic->mc_to_npc_xform,
		      &NPC_Origin);

    if ((ClipNPCPoint4D (pRend, &NPC_Origin, &oc)) == PEXLookupTableError)
	return (PEXLookupTableError);
    if (oc) {
	pDDC->Static.pick.status = PEXNoPick;
	return (Success);  /* Don't pick anything; origin clipped out */
    }

    /* Keep the NPC_Origin computed above for later use */

    /* Get the pick aperture and convert into NPC, if required. */

    if (pDDC->Static.pick.type == PEXPickDeviceDC_HitBox) {

	/* Convert the dcHitBox into a NPCHitVolume */

	convert_dcHitBox_to_npc (pRend, &aperture);
    }
    else {

	/* Copy data straight from the NPC pick aperture input record */

	aperture = pDDC->Static.pick.input_rec.npc_hit_volume;
    }

    /* Set the annotation text flag to one */

    aflag = 1;

    /* Determine the total number of characters in the ISTRING */

    numChars = 0;
    pMono = pText;
    for (i=0; i<numEncodings; i++) {
      int bytes = pMono->numChars * ((pMono->characterSetWidth == PEXCSByte) ?
	  sizeof(CARD8) : ((pMono->characterSetWidth == PEXCSShort) ?
	  sizeof(CARD16) : sizeof(CARD32)));
      numChars += (ddULONG)pMono->numChars;
      pMono = (pexMonoEncoding *) ((char *) (pMono + 1) +
	  bytes + PADDING (bytes));
    }

    if (numChars == 0)
    {
	pDDC->Static.pick.status = PEXNoPick;
	return (Success);
    }


    /* Convert text string into required paths */

    if ((status = atx_el_to_path (pRend, pDDC, numEncodings, pText,
	numChars, &text_el, &align, &count)) != Success) {
      return (status);
    }

    /* Compute the required Character Space to Modelling Space Transform */

    text3_xform (pOrigin,pDirections, (pDirections+1),
		 pDDC->Static.attrs, &align, text_el.xform, aflag);

    /* Set up the new composite transform first. Note that in the case */
    /* of annotation text, only the text origin is transformed by the  */
    /* complete pipeline transform. The text itself is affected only by*/
    /* the transformed origin in NPC, the NPC offset , npc_to_cc, and  */
    /* the workstation transform.                                      */

    /* Now compute the initial composite transform for the first char.  */
    /* The required transforms for characters are - text space to model */
    /* space transform, transformation of the annotation text origin, if*/
    /* any. Note the ABSENCE of npc to cc transform here because of the */
    /* PICKING as opposed to rendering.                                 */

    /* Get the translation due to the transformation of the annotation  */
    /* text origin by mc_to_npc_xform into buf1_xform.                  */

    memcpy( (char *)buf1_xform, (char *) ident4x4, 16 * sizeof(ddFLOAT));
    buf1_xform[0][3] += NPC_Origin.x - MC_Origin.x;
    buf1_xform[1][3] += NPC_Origin.y - MC_Origin.y;
    buf1_xform[2][3] += NPC_Origin.z - MC_Origin.z;

    miMatMult (buf2_xform, text_el.xform, buf1_xform);

    /* Add the offset in NPC */

    buf2_xform[0][3] += pOffset->x;
    buf2_xform[1][3] += pOffset->y;
    buf2_xform[2][3] += pOffset->z;

    /* Pick the paths in text_el as polylines */

    /* Get the current character expansion factor */

    exp = ABS((ddFLOAT)pDDC->Static.attrs->charExpansion);

    /* Save the pointer to the beginning of the character data */

    save_ptr = text_el.paths;

    Pick_Flag = 0;   /* Initialize flag to indicate NO_PICK */

    /* Get the current defined view entry from the View LUT */

    view = &(view_entry->entry);

    /* Compute the intersection of the pick aperture with the NPC */
    /* sub-volume defined for the current view.                   */

    if (compute_pick_volume (&aperture, view, pDDC, &pv)) {

	/* We have NO intersection between the pick aperture  */
	/* and the NPC subvolume defined for the current view */

	goto TextHit;      /* NoPick, Skip everything else */
    }

    /* Get the transform to go from pick volume to CC - buf1_xform */
     
    compute_pick_volume_xform (&pv, buf1_xform);

    /* Do for all characters (paths) in the text_el */

    /* Initialize the previous translation components */

    ptx = pty = 0.0;

    for (k=0; k<count; k++) {  /* Pick characters one by one */
	    
	/* Check if the character is not renderable, for e.g., space */
	/* char. If so just skip to next character in the ISTRING and*/
	/* continue.                                                 */

	if (!(text_el.paths->path->ddList)) {
	    ptx = (ddFLOAT)(((text_el.paths)->trans).x);
	    pty = (ddFLOAT)(((text_el.paths)->trans).y);
	    text_el.paths++;
	    continue;
	}

	/* Modify the composite transform by the previous translation */
	/* and the current scaling in x realizing the char expansion  */

	tx = ptx;
	ty = pty;

	ptx = (ddFLOAT)(((text_el.paths)->trans).x);
	pty = (ddFLOAT)(((text_el.paths)->trans).y);

	/* Check to see if this is the very first character and the */
	/* text path is Up or Down. If so, we need to modify tx by  */
	/* first character translation to align with the rest of the*/
	/* characters in the string.                                */

	if ((pDDC->Static.attrs->atextPath == PEXPathUp ||
	     pDDC->Static.attrs->atextPath == PEXPathDown) && k == 0)
	    tx += ptx;

	/* NOTE THAT THE ABOVE COMPUTATION WILL ONLY WORK FOR THE */
	/* FIRST CHARACTER IN THE STRING. ptx FOR ALL OTHERS WILL */
	/* BE RELATIVE TO THE TEXT ORIGIN AND SO WILL NOT GIVE THE*/
	/* REQUIRED EFFECTIVE CHARACTER WIDTH. HOWEVER, THIS IS   */
	/* NOT A PROBLEM HERE SINCE WE NEED THIS SPECIAL CASE ONLY*/
	/* FOR THE FIRST CHARACTER.                               */
	/*                                                        */
	/* FURTHER, NOTE THAT ptx WILL BE NEGATIVE AND HENCE USE  */
	/* OF +=                                                  */

	if (k == 0) {
	    ptx_first = ptx; /* Get the first character translation */

	    /* Adjust the translation by character spacing factor to*/
	    /* get just the character width.                        */

	    ptx_first += (pDDC->Static.attrs->charSpacing) * 
		          FONT_COORD_HEIGHT;

	    pty_first = pty; /* Save first character height */

	    /* Adjust the translation by character spacing factor to*/
	    /* get just the character height.                       */

	    pty_first += (pDDC->Static.attrs->charSpacing) * 
		          FONT_COORD_HEIGHT;
	}

	/* Check to see if the text path is Left. If so, we need   */
	/* to modify tx by the first character width so as to start*/
	/* the string to the left of the text origin.              */

	if (pDDC->Static.attrs->atextPath == PEXPathLeft)
	    tx += ptx_first;

	/* Buffer the tc_to_npc_xform first */

	memcpy( (char *)tc_to_npc_xform, (char *)buf2_xform,16*sizeof(ddFLOAT));

	/* Apply the per character translation and scaling by */
	/* directly modifying the concerned matrix elements.  */

	for (i=0; i<4; ++i) {
	    /* Buffer the element values */
	    ei0npc = tc_to_npc_xform[i][0];
	    ei1npc = tc_to_npc_xform[i][1];
	    ei3npc = tc_to_npc_xform[i][3];
	    /* Modify the transform */
	    tc_to_npc_xform[i][0] = exp * ei0npc;
	    tc_to_npc_xform[i][3] = tx * ei0npc + ty * ei1npc + ei3npc;
	}

	/* Get buf_xform = (tc_to_npc_xform * buf1_xform) */

	miMatMult (buf_xform, tc_to_npc_xform, buf1_xform);

	/* Transform and clip the paths corresponding to current */
	/* character.                                            */

	if (status = miTransform(pDDC, text_el.paths->path, &cc_path, 
				 buf_xform,
				 NULL4x4,
				 DD_HOMOGENOUS_POINT))
	    return (status);

	/* Now pass the paths through the line clipper to see if */
	/* it lies within the pick aperture. If anything remains,*/
	/* then return a PICK. Otherwise, return a NO_PICK.      */

	if (status = miClipPolyLines (pDDC, cc_path, &clip_path,
				      MI_VCLIP)) {

	    /* BadAlloc, or NOT 4D points */
	 
	    return (status);
	}
	else { 
	    /* Check if anything is remaining. If so, the pick volume */
	    /* intersects the text string. If not, this char has been */
	    /* clipped out. Accordingly, update the Pick_Flag.        */

	    if (clip_path->numLists > 0) {
		Pick_Flag = 1;
		goto TextHit;
	    }
	}

	/* Update the pointer to next character */

	text_el.paths++;

    }   /* Loop for all characters in the text string */

    TextHit:

    if (Pick_Flag)

	/* Update the ddContext pick status to PICK */

	pDDC->Static.pick.status = PEXOk;
    else
	/* Update the ddContext pick status to NO_PICK */

	pDDC->Static.pick.status = PEXNoPick;

    /* Free up space allocated for text stroke data */

    xfree ((char *)save_ptr);

    return (Success);
}


/*++
 |
 |  Function Name:	miPickPrimitives
 |
 |  Function Description:
 |       Handles the picking of most primitives in a generic fashion.
 |
 |  Note(s):
 |
 ++*/

ddpex2rtn
miPickPrimitives(pRend, pExecuteOC)
/* in */
    ddRendererPtr       pRend;	  /* renderer handle */
    miGenericStr       *pExecuteOC;
{
/* calls */

/* Local variable definitions */
      miDDContext	*pDDC = (miDDContext *)(pRend->pDDContext);
      ddPC_NPC_HitVolume aperture;
      ddNpcSubvolume     pv;
      miViewEntry       *view_entry;
      ddViewEntry       *view;
      ddFLOAT            buf1_xform[4][4];
      ddUSHORT           cur_index;
      ddUSHORT           status;

      /* Get the pick aperture and convert into NPC, if required. */

      if (pDDC->Static.pick.type == PEXPickDeviceDC_HitBox) {

	  /* Convert the dcHitBox into a NPCHitVolume */

	  convert_dcHitBox_to_npc (pRend, &aperture);

      }
      else {

	  /* Copy data straight from the NPC pick aperture input record */

	  aperture = pDDC->Static.pick.input_rec.npc_hit_volume;
      }

      /* Clear out the cc_to_dc_xform, since we will not be going to DC */

      memcpy( (char *) pDDC->Dynamic->cc_to_dc_xform, 
		(char *) ident4x4, 16 * sizeof(ddFLOAT));

      /* Get the current defined view entry from the View LUT */

      cur_index = pDDC->Dynamic->pPCAttr->viewIndex;

      if ((InquireLUTEntryAddress (PEXViewLUT, pRend->lut[PEXViewLUT],
				   cur_index, &status, (ddPointer *)&view_entry))
	  == PEXLookupTableError)
	  return (PEXLookupTableError);

      view = &(view_entry->entry);

      /* Compute the intersection of the pick aperture with the NPC */
      /* sub-volume defined for the current view.                   */

      if (compute_pick_volume (&aperture, view, pDDC, &pv)) {

	  /* We have NO intersection between the pick aperture  */
	  /* and the NPC subvolume defined for the current view */

	  return (Success);      /* NoPick, Just return */
      }

      /* Get the transform to go from pick volume to CC - buf1_xform */
     
      compute_pick_volume_xform (&pv, buf1_xform);

      /* Compute the mc_to_npc for the current view */

      miMatMult (pDDC->Dynamic->mc_to_npc_xform,
		 pDDC->Dynamic->mc_to_wc_xform,
		 view_entry->vom);

      /* Get wc_to_cc_xform = (wc_to_npc_xform * buf1_xform) */
      /* Note that wc_to_npc_xform == view_entry->vom.       */

      miMatMult (pDDC->Dynamic->wc_to_cc_xform, 
		 view_entry->vom, buf1_xform);

      /* Get mc_to_cc_xform = (mc_to_npc_xform * buf1_xform) */

      miMatMult (pDDC->Dynamic->mc_to_cc_xform, 
		 pDDC->Dynamic->mc_to_npc_xform, buf1_xform);

      /* Now, call the level 2 rendering function to transform and */
      /* clip the primitive. Note that the level 1 function vector */
      /* now has PICKING routines instead of rendering routines. If*/
      /* a pick is detected the GLOBAL PICK FLAG will have been up-*/
      /* dated. Check this flag to determine if anything was indeed*/
      /* picked by the level 1 picking routines.                   */

      InitExecuteOCTable[(int)(pExecuteOC->elementType)]
	  (pRend, pExecuteOC);

      /* If successful PICK, just return */

      if (pDDC->Static.pick.status == PEXOk) ;;  /* For debug */

      return (Success);
}


/*++
 |
 |  Function Name:	miTestPickGdp3d
 |
 |  Function Description:
 |	 Provides the dummy test routine for picking 3d Gdps.
 |       with data OCs.
 |
 |  Note(s):
 |
 ++*/

ddpex2rtn
miTestPickGdp3d(pRend, pExecuteOC)
/* in */
    ddRendererPtr       pRend;	  /* renderer handle */
    miGenericStr       *pExecuteOC;
/* out */
{
    ErrorF ("miTestPickGdp3d\n");
    return (Success);
}


/*++
 |
 |  Function Name:	miTestPickGdp2d
 |
 |  Function Description:
 |	 Provides the dummy test routine for picking 2d Gdps.
 |       with data OCs.
 |
 |  Note(s):
 |
 ++*/

ddpex2rtn
miTestPickGdp2d(pRend, pExecuteOC)
/* in */
    ddRendererPtr       pRend;	  /* renderer handle */
    miGenericStr       *pExecuteOC;
/* out */
{
    ErrorF ("miTestPickGdp2d\n");
    return (Success);
}


