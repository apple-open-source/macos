/* $Xorg: miLight.c,v 1.4 2001/02/09 02:04:09 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/level2/miLight.c,v 1.9 2001/12/14 19:57:28 dawes Exp $ */

#include "miLUT.h"
#include "misc.h"
#include "miscstruct.h"
#include "PEXErr.h"
#include "miRender.h"
#include "miLight.h"
#include "pexos.h"

 
extern ddpex3rtn	InquireLUTEntryAddress();
ddpex3rtn ComputeWCEyePosition();

/*++
 |
 |  Apply_Lighting(pRend, pddc, input_vert, output_fct)
 |
 |	Create a facet list for the fill area defined by the
 |	specified vertex list.
 |
 |      NOTE: This routine does not use back-facing lighting parameters.
 |	      Furthermore, it assumes that the input lights are
 |	      specified in rgb.
 |
 --*/
ddpex3rtn
miApply_Lighting(pRend, pddc, point, mat_color, normal, out_color)
    ddRendererPtr	pRend;		/* renderer handle */
    miDDContext		*pddc;
    ddCoord4D		*point;
    ddRgbFloatColour	*mat_color;
    ddVector3D		*normal;
    ddRgbFloatColour	*out_color;
{
/* calls */
#ifndef XFree86LOADER
    double		pow();
#endif

/* uses */
    listofObj		*light_sources = pddc->Dynamic->pPCAttr->lightState;
    ddUSHORT		*index = (ddUSHORT *)light_sources->pList;
    miLightEntry	*pLUT;
    ddLightEntry	*lightentry; 
    ddUSHORT		status;
    ddFLOAT		n_dot_l, v_dot_r, d_dot_l;
    ddFLOAT		distance;
    ddFLOAT		atten;
    ddVector3D		view_vec, refl_vec, light_vec;
    int			j;


    /* some quick bounds checking */
    if (light_sources->numObj <= 0) {
      *out_color = *mat_color;
      return(Success);
    }

    out_color->red = out_color->green = out_color->blue = 0.0;

    for (j = 0; j < light_sources->numObj; j++) {
      /* Fetch light source data from LUT */
      if ((InquireLUTEntryAddress (PEXLightLUT,
				   pRend->lut[PEXLightLUT], *index,
				   &status, (ddPointer *)(&pLUT)))
	   == PEXLookupTableError)
	return (PEXLookupTableError);

	lightentry = &pLUT->entry;

      /* 
       * Insure that light color is same model as the current
       * rendering model.
       */
      if (lightentry->lightColour.colourType) {}

      /* The shading equations are different for each light type */
      switch(lightentry->lightType) {

	case PEXLightAmbient:
	  /*
	  * An ambient light simply adds a constant background
	  * color term to all the facets in a scene.
	  */
	  out_color->red += 
			pddc->Static.attrs->reflAttr.ambient * 
			(lightentry->lightColour.colour.rgbFloat.red *
			 mat_color->red);
	  out_color->green +=
			pddc->Static.attrs->reflAttr.ambient *
			(lightentry->lightColour.colour.rgbFloat.green*
			 mat_color->green);
	  out_color->blue += 
			pddc->Static.attrs->reflAttr.ambient *
			(lightentry->lightColour.colour.rgbFloat.blue *
			 mat_color->blue);
	  break;

	case PEXLightWcsVector:
	  /*
	   * A directional light source is located at an 
	   * infinite distance from the object along
	   * the specified direction.
	   */
	  n_dot_l = 0.0;

	  /* compute reflect view vector */
	  DOT_PRODUCT(&lightentry->direction, normal, n_dot_l);
	  /* negate because light vector should point towards light */
	  n_dot_l = -n_dot_l;
	  if (n_dot_l <= 0.0) break; 

	  switch(pddc->Static.attrs->reflModel) {
	    /* 
	     * Note that the total light source contribution
	     * is the sum of the specular, diffuse and ambient
	     * contributions.
	     */
	    case PEXReflectionSpecular:

		CALCULATE_REFLECTION_VECTOR(&refl_vec, -n_dot_l,
					    normal,
					    &lightentry->direction);
    		refl_vec.x *= -1.0;		
    		refl_vec.y *= -1.0;		
    		refl_vec.z *= -1.0;		
		NORMALIZE_VECTOR (&refl_vec, v_dot_r);

		/*
		 * Insure eye point is correct for view vector
		 * calculation. As eye_point is computed from
		 * the inverse view matrix, this matrix must
		 * be valid for eye point to be correct.
		 */
		if (pddc->Static.misc.flags & INVVIEWXFRMFLAG)
		  ComputeWCEyePosition(pRend, pddc);

		if (NEAR_ZERO(pddc->Static.misc.eye_pt.w)) {
		  /* 
		   * if the homogeneous component of the eye
		   * position is near zero after the inverse
		   * transform from NPC space, it indicates that
		   * the view transform is an orthographic, as oppposed
		   * to perspective transformation and that the eye
		   * point thus represents a vector rather than a point
		   */
		  view_vec.x = pddc->Static.misc.eye_pt.x;
		  view_vec.y = pddc->Static.misc.eye_pt.y;
		  view_vec.z = pddc->Static.misc.eye_pt.z;
		} else {
		  /*
		   * Compute the view vector.
		   */
		  CALCULATE_DIRECTION_VECTOR(&pddc->Static.misc.eye_pt,
					     point,
					     &view_vec);
		  NORMALIZE_VECTOR (&view_vec, v_dot_r);
		}

		DOT_PRODUCT(&refl_vec, &view_vec, v_dot_r);
		if (v_dot_r > 0.0)
		 {
		  v_dot_r = pow(v_dot_r, 
			     pddc->Static.attrs->reflAttr.specularConc);
		  out_color->red += 
		    v_dot_r *
		    pddc->Static.attrs->reflAttr.specular *
		    (lightentry->lightColour.colour.rgbFloat.red * 
		     pddc->Static.attrs->reflAttr.specularColour.colour.rgbFloat.red);
		  out_color->green += 
		    v_dot_r *
		    pddc->Static.attrs->reflAttr.specular *
		    (lightentry->lightColour.colour.rgbFloat.green * 
		     pddc->Static.attrs->reflAttr.specularColour.colour.rgbFloat.green);
		  out_color->blue += 
		    v_dot_r *
		    pddc->Static.attrs->reflAttr.specular *
		    (lightentry->lightColour.colour.rgbFloat.blue * 
		     pddc->Static.attrs->reflAttr.specularColour.colour.rgbFloat.blue);
		 }

	    case PEXReflectionDiffuse:

		out_color->red += n_dot_l *
			pddc->Static.attrs->reflAttr.diffuse *
			(lightentry->lightColour.colour.rgbFloat.red * 
			 mat_color->red);
		out_color->green += n_dot_l *
			pddc->Static.attrs->reflAttr.diffuse *
			(lightentry->lightColour.colour.rgbFloat.green * 
			 mat_color->green);
		out_color->blue += n_dot_l *
			pddc->Static.attrs->reflAttr.diffuse *
			(lightentry->lightColour.colour.rgbFloat.blue * 
			 mat_color->blue);

	    case PEXReflectionAmbient:
		/* No ambient contribution except from ambient lights */

	   break;
	  }
	  break;

	case PEXLightWcsPoint:
	  /*
	   * A point light source is a radiating point source
	   * located at the specified position.
	   */
	  n_dot_l = 0.0;
	  /* 
	   * Note that the total light source contribution
	   * is the sum if the specular, diffuse and ambient
	   * contributions.
	   */

	  CALCULATE_DIRECTION_VECTOR(&lightentry->point,
				     point,
				     &light_vec);
	  NORMALIZE_VECTOR (&light_vec, distance);

	  /* compute reflect view vector */
	  DOT_PRODUCT(&light_vec, normal, n_dot_l);
	  if (n_dot_l <= 0.0) break; 

	  /* Compute light attenuation */
	  atten = 1.0/(lightentry->attenuation1 + 
			   (lightentry->attenuation2 * distance));

	  switch(pddc->Static.attrs->reflModel) {

	      case PEXReflectionSpecular:

		CALCULATE_REFLECTION_VECTOR(&refl_vec, n_dot_l,
					    normal,
					    &light_vec);
		NORMALIZE_VECTOR (&refl_vec, v_dot_r);

		/*
		 * Insure eye point is correct for view vector
		 * calculation. As eye_point is computed from
		 * the inverse view matrix, this matrix must
		 * be valid for eye point to be correct.
		 */
		if (pddc->Static.misc.flags & INVVIEWXFRMFLAG)
		  ComputeWCEyePosition(pRend, pddc);

		if (NEAR_ZERO(pddc->Static.misc.eye_pt.w)) {
		  /* 
		   * if the homogeneous component of the eye
		   * position is near zero after the inverse
		   * transform from NPC space, it indicates that
		   * the view transform is an orthographic, as oppposed
		   * to perspective transformation and that the eye
		   * point thus represents a vector rather than a point
		   */
		  view_vec.x = pddc->Static.misc.eye_pt.x;
		  view_vec.y = pddc->Static.misc.eye_pt.y;
		  view_vec.z = pddc->Static.misc.eye_pt.z;
		} else {
		  /*
		   * Compute the view vector.
		   */
		  CALCULATE_DIRECTION_VECTOR(&pddc->Static.misc.eye_pt,
					     point,
					     &view_vec);
		  NORMALIZE_VECTOR (&view_vec, v_dot_r);
		}

		DOT_PRODUCT(&refl_vec, &view_vec, v_dot_r);
		if (v_dot_r > 0.0)
		 {
		  v_dot_r = pow(v_dot_r, 
			     pddc->Static.attrs->reflAttr.specularConc);
		  out_color->red += 
		    atten * v_dot_r *
		    pddc->Static.attrs->reflAttr.specular *
		    (lightentry->lightColour.colour.rgbFloat.red * 
		     pddc->Static.attrs->reflAttr.specularColour.colour.rgbFloat.red);
		  out_color->green += 
		    atten * v_dot_r *
		    pddc->Static.attrs->reflAttr.specular *
		    (lightentry->lightColour.colour.rgbFloat.green * 
		     pddc->Static.attrs->reflAttr.specularColour.colour.rgbFloat.green);
		  out_color->blue += 
		    atten * v_dot_r *
		    pddc->Static.attrs->reflAttr.specular *
		    (lightentry->lightColour.colour.rgbFloat.blue * 
		     pddc->Static.attrs->reflAttr.specularColour.colour.rgbFloat.blue);
		 }

	      case PEXReflectionDiffuse:

		out_color->red += atten * n_dot_l *
			pddc->Static.attrs->reflAttr.diffuse *
			(lightentry->lightColour.colour.rgbFloat.red * 
			 mat_color->red);
		out_color->green += atten * n_dot_l *
			pddc->Static.attrs->reflAttr.diffuse *
			(lightentry->lightColour.colour.rgbFloat.green * 
			 mat_color->green);
		out_color->blue += atten * n_dot_l *
			pddc->Static.attrs->reflAttr.diffuse *
			(lightentry->lightColour.colour.rgbFloat.blue * 
			 mat_color->blue);

	      case PEXReflectionAmbient:
		/* No ambient contribution except from ambient lights */
		break;
	  }
	  break;

	case PEXLightWcsSpot:
	  /*
	   * A spot light source is a radiating point source
	   * output whose output is limited by a cone of specified
	   * direction and angle.
	   */
	  n_dot_l = 0.0;
	  /* 
	   * Note that the total light source contribution
	   * is the sum if the specular, diffuse and ambient
	   * contributions.
	   */

	  CALCULATE_DIRECTION_VECTOR(&lightentry->point,
				     point,
				     &light_vec);
	  NORMALIZE_VECTOR (&light_vec, distance);

	  /* Check that object is within spot angle. */
	  DOT_PRODUCT(&lightentry->direction, &light_vec, d_dot_l);
	  /* Negate because light vec should point other way for this test */
	  d_dot_l = -d_dot_l;
	  if (d_dot_l <= pLUT->cosSpreadAngle) break;
	  d_dot_l = pow(d_dot_l, lightentry->concentration);

	  /* compute reflect view vector */
	  DOT_PRODUCT(&light_vec, normal, n_dot_l);
	  if (n_dot_l <= 0.0) break; 

	  /* Compute light attenuation */
	  atten = 1.0/(lightentry->attenuation1 + 
			(lightentry->attenuation2 * distance));

	  switch(pddc->Static.attrs->reflModel) {

	      case PEXReflectionSpecular:

		CALCULATE_REFLECTION_VECTOR(&refl_vec, n_dot_l,
					    normal,
					    &light_vec);
		NORMALIZE_VECTOR (&refl_vec, v_dot_r);

		/*
		 * Insure eye point is correct for view vector
		 * calculation. As eye_point is computed from
		 * the inverse view matrix, this matrix must
		 * be valid for eye point to be correct.
		 */
		if (pddc->Static.misc.flags & INVVIEWXFRMFLAG)
		  ComputeWCEyePosition(pRend, pddc);

		if (NEAR_ZERO(pddc->Static.misc.eye_pt.w)) {
		  /* 
		   * if the homogeneous component of the eye
		   * position is near zero after the inverse
		   * transform from NPC space, it indicates that
		   * the view transform is an orthographic, as oppposed
		   * to perspective transformation and that the eye
		   * point thus represents a vector rather than a point
		   */
		  view_vec.x = pddc->Static.misc.eye_pt.x;
		  view_vec.y = pddc->Static.misc.eye_pt.y;
		  view_vec.z = pddc->Static.misc.eye_pt.z;
		} else {
		  /*
		   * Compute the view vector.
		   */
		  CALCULATE_DIRECTION_VECTOR(&pddc->Static.misc.eye_pt,
					     point,
					     &view_vec);
		  NORMALIZE_VECTOR (&view_vec, v_dot_r);
		}

		DOT_PRODUCT(&refl_vec, &view_vec, v_dot_r);
		if (v_dot_r > 0.0)
		 {
		  v_dot_r = pow(v_dot_r, 
			     pddc->Static.attrs->reflAttr.specularConc);
		  out_color->red += 
		    atten * v_dot_r * d_dot_l *
		    pddc->Static.attrs->reflAttr.specular *
		    (lightentry->lightColour.colour.rgbFloat.red * 
		     pddc->Static.attrs->reflAttr.specularColour.colour.rgbFloat.red);
		  out_color->green += 
		    atten * v_dot_r * d_dot_l *
		    pddc->Static.attrs->reflAttr.specular *
		    (lightentry->lightColour.colour.rgbFloat.green * 
		     pddc->Static.attrs->reflAttr.specularColour.colour.rgbFloat.green);
		  out_color->blue += 
		    atten * v_dot_r * d_dot_l *
		    pddc->Static.attrs->reflAttr.specular *
		    (lightentry->lightColour.colour.rgbFloat.blue * 
		     pddc->Static.attrs->reflAttr.specularColour.colour.rgbFloat.blue);
		 }

	      case PEXReflectionDiffuse:

		out_color->red += atten * n_dot_l * d_dot_l *
			pddc->Static.attrs->reflAttr.diffuse *
			(lightentry->lightColour.colour.rgbFloat.red * 
			 mat_color->red);
		out_color->green += atten * n_dot_l * d_dot_l *
			pddc->Static.attrs->reflAttr.diffuse *
			(lightentry->lightColour.colour.rgbFloat.green * 
			 mat_color->green);
		out_color->blue += atten * n_dot_l * d_dot_l *
			pddc->Static.attrs->reflAttr.diffuse *
			(lightentry->lightColour.colour.rgbFloat.blue * 
			 mat_color->blue);

	      case PEXReflectionAmbient:
		/* No ambient contribution except from ambient lights */
		break;
	  }
      }
    index++; /* skip to next light source index in list */

    }

    return(Success);
}

/*++
 |
 |  ComputeWCEyePosition(pRend, pddc)
 |
 |	Compute a WC eye position from the inverse view transform.
 |
 --*/
ddpex3rtn ComputeWCEyePosition(pRend, pddc)
	ddRendererPtr	pRend;		/* renderer handle */
	miDDContext	*pddc;
{
	extern void	miMatCopy();

	/* First, update inverse view matrix */
	miViewEntry	*pLUT;
	ddUSHORT	status;
	ddCoord4D	NPCEye;
	ddFLOAT		mag;

	/* Get the view table entry at current view index first */
	if ((InquireLUTEntryAddress (PEXViewLUT, pRend->lut[PEXViewLUT],
				     pddc->Dynamic->pPCAttr->viewIndex,
				     &status, (ddPointer *)&pLUT))
	    == PEXLookupTableError)
	  return (PEXLookupTableError);


	/* Compute the composite [VOM] next */
/*
	ddViewEntry	*viewbundle;
	viewbundle = &pLUT->entry;
	miMatMult (pddc->Static.misc.inv_view_xform,
		   viewbundle->orientation,
		   viewbundle->mapping );
*/

	if (!pLUT->inv_flag)
	{
		/* compute the inverse */
		miMatCopy(pLUT->vom, pddc->Static.misc.inv_view_xform);
		miMatInverse(pddc->Static.misc.inv_view_xform);
		miMatCopy(pddc->Static.misc.inv_view_xform, pLUT->vom_inv);
		pLUT->inv_flag = MI_TRUE;
	}
	else
		miMatCopy(pLUT->vom_inv, pddc->Static.misc.inv_view_xform);

	/* Clear ddc status flag */
	pddc->Static.misc.flags &= ~INVVIEWXFRMFLAG;
	
	/* 
	 * Multiply NPC eye point by inverse view matrix 
	 * NPC eye point is defined as (0.0, 0.0, 1.0, 0.0) in NPC space
	 */
	NPCEye.x = 0.0;
	NPCEye.y = 0.0;
	NPCEye.z = 1.0;
	NPCEye.w = 0.0;
	miTransformPoint(&NPCEye, pddc->Static.misc.inv_view_xform,
			 &pddc->Static.misc.eye_pt);

	/* normalize the result if a eye pint is a vector in WC */
	if (NEAR_ZERO(pddc->Static.misc.eye_pt.w)) {
	  NORMALIZE_VECTOR (&(pddc->Static.misc.eye_pt), mag);
	}
}


/*++
 |
 |  Compute_CC_Dcue(pRend, pddc)
 |
 |      Compute a CC version of the current depth cue bundle entry
 |
 --*/
ddpex3rtn Compute_CC_Dcue(pRend, pddc)
        ddRendererPtr   pRend;          /* renderer handle */
        miDDContext     *pddc;
{
	ddpex3rtn	miConvertColor();

        ddUSHORT        status;
        miDepthCueEntry	*dcue_entry;
	ddFLOAT		*matrix, cc_frontplane, cc_backplane; 

	/* Check valid flag */
	if (!(pddc->Static.misc.flags & CC_DCUEVERSION)) return(Success);

        /* Get current depth cueing information */
        if ((InquireLUTEntryAddress (PEXDepthCueLUT, pRend->lut[PEXDepthCueLUT],
             pddc->Dynamic->pPCAttr->depthCueIndex,
             &status, (ddPointer *)&dcue_entry)) == PEXLookupTableError)
          return (PEXLookupTableError);

	/* Compute cc versions of front and back planes
	   We assume that the transformation matrix from npc to cc is
	   diagonal, and only tranform the "z" portion */

	matrix = (ddFLOAT *)(pddc->Dynamic->npc_to_cc_xform);

	/* skip to Z translate and scale terms */
	matrix += 10;

	cc_frontplane = (*matrix * dcue_entry->entry.frontPlane) + *(matrix + 1); 
	cc_backplane = (*matrix * dcue_entry->entry.backPlane) + *(matrix + 1); 
 
	/* copy over relevant info */
	pddc->Static.misc.cc_dcue_entry.mode = dcue_entry->entry.mode;
	pddc->Static.misc.cc_dcue_entry.frontScaling = 
		dcue_entry->entry.frontScaling;
	pddc->Static.misc.cc_dcue_entry.backScaling = 
		dcue_entry->entry.backScaling;

        pddc->Static.misc.cc_dcue_entry.frontPlane = cc_frontplane;
        pddc->Static.misc.cc_dcue_entry.backPlane = cc_backplane;

	/* Convert depth cue color to proper rendering model during copy */
	miConvertColor(pRend,
		       &dcue_entry->entry.depthCueColour,
		       pddc->Dynamic->pPCAttr->rdrColourModel,
		       &pddc->Static.misc.cc_dcue_entry.depthCueColour);

	/* validate flag */
	pddc->Static.misc.flags &= ~CC_DCUEVERSION;
 
}

