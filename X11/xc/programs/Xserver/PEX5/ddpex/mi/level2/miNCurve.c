/* $Xorg: miNCurve.c,v 1.4 2001/02/09 02:04:10 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/level2/miNCurve.c,v 3.8 2001/12/14 19:57:28 dawes Exp $ */

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
#include "miNurbs.h"
#include "pexos.h"


/*++
 |
 |  Function Name:	miNurbsCurve
 |
 |  Function Description:
 |	 Handles the Nurbs Curve Pex OC.
 |
 |  Note(s):
 |
 --*/

ddpex3rtn
miNurbsCurve(pRend, pExecuteOC)
/* in */
    ddRendererPtr       pRend;	  /* renderer handle */
    miGenericStr       *pExecuteOC;
{
/* calls */
    ddpex3rtn		tessellate_curve();
    extern ocTableType	InitExecuteOCTable[];

/* Local variable definitions */
    miDDContext		*pddc = (miDDContext *)(pRend->pDDContext);
    miNurbStruct	*ddCurve = (miNurbStruct *)(pExecuteOC+1);
    miGenericStr	*pGStr;
    miListHeader	*tesselated_list;
    miListHeader	*polyline_list;
    ddpex3rtn		status;

    switch(pddc->Static.attrs->curveApprox.approxMethod) {

	case PEXApproxImpDep:
	case PEXApproxConstantBetweenKnots:
	case PEXApproxDcChordalSize:
	case PEXCurveApproxDcChordalDev:
	case PEXApproxDcRelative:
	default:

	case PEXApproxWcsChordalSize:
	case PEXCurveApproxWcsChordalDev:
	case PEXApproxWcsRelative:

	   /* apply curve approximation criteria in wc */
	   if (status = tessellate_curve( pddc, ddCurve, 
					  &tesselated_list,
					  pddc->Dynamic->mc_to_wc_xform ))
	     return (status);

	   break;


	case PEXApproxNpcChordalSize:
	case PEXCurveApproxNpcChordalDev:
	case PEXApproxNpcRelative:

	   /* apply curve approximation criteria in wc */
	   if (status = tessellate_curve( pddc, ddCurve, 
					  &tesselated_list,
					  pddc->Dynamic->mc_to_npc_xform ))
	     return (status);

	   break;
    }

    /* allocate polyline command block */
    if (!(pGStr = (miGenericStr *) (xalloc(sizeof(miGenericStr) +
					   sizeof(miListHeader)))))
      return(BadAlloc);

    pGStr->elementType = PEXOCPolylineSet;
    /* The length data is ignored by the rendering routine and hence is */
    /* left as whatever GARBAGE that will be present at the alloc time. */

    polyline_list = (miListHeader *) (pGStr + 1);
    *polyline_list = *tesselated_list;

    /* render tesselated curve */
    status = InitExecuteOCTable[(int)(pGStr->elementType)](pRend, pGStr);

    xfree(pGStr);

    return (status);

}

/*++
 |
 |  Function Name:	multiply_basis_func_control_pts
 |
 |  Function Description:
 |
 | 	Multiply nurb basis func and control points to get polynomial coeffs
 |
 |     |poly[0]|        |C0,0 C0,1 C0,2 C0,3| |Pxi+3|
 |     |poly[1]|   =    |C1,0 C1,1 C1,2 C1,3| |Pxi+2|
 |     |poly[2]|        |C2,0 C2,1 C2,2 C2,3| |Pxi+1|
 |     |poly[3]|        |C3,0 C3,1 C3,2 C3,3| |Pxi  |
 |
 |
 |  Note(s):
 |
 --*/
static 
void
multiply_basis_func_control_pts( pt_type, order, span, pts, C, poly )
    ddPointType    pt_type;
    ddUSHORT	order; 
    int         span;
    char	*pts;			/* control points */
    double	C[MAXORD][MAXORD];	/* span basis matrix */
    double	poly[4][MAXORD];	/* computed polynomial basis mtx */
{
    int		i, k;
    double	x, y, z, w;
    char	*pt;
    int		point_size;

    DD_VertPointSize(pt_type, point_size);

    for ( k = 0; k < order; k++ ) {
	x = y = z = w = 0.0;
	pt = pts + point_size * (span - order);
	for ( i = 0; i < order; i++ ) {	/* for all coeffs */
	    pt += point_size;
	    x += C[k][i] * ((ddCoord4D *)pt)->x;
	    y += C[k][i] * ((ddCoord4D *)pt)->y;
	    if ( !DD_IsVert2D(pt_type) ) {
		z += C[k][i] * ((ddCoord4D *)pt)->z;
		if ( DD_IsVert4D(pt_type) )
		  w += C[k][i] * ((ddCoord4D *)pt)->w;
	   }
	}
	poly[XX][k] = x;
	poly[YY][k] = y;
	poly[ZZ][k] = z;
	poly[WW][k] = w;
    }
}



/*++
 |
 |  Function Name:	compute_fwd_matrix2D
 |
 |  Function Description:
 |	 
 |	initial scale and convert coefficients to forward difference basis
 |
 |                |1  0  0  0 | |1  0  0  0 | |polyx[0]|
 |     |fdx[0]|   |0  1  1  1 | |0 dt  0  0 | |polyx[1]|
 |     |fdx[1]| = |           | |       2   | |        |
 |     |fdx[2]|   |0  0  2  6 | |0  0 dt  0 | |polyx[2]|
 |     |fdx[3]|   |           | |          3| |        |
 |                |0  0  0  6 | |0  0  0 dt | |polyx[3]|
 |     
 |
 |  Note(s):
 |
 --*/
static 
void
compute_fwd_matrix2D( pt_type, order, dt, poly )
    ddPointType    pt_type;
    ddUSHORT	order; 
    float	dt;
    double	poly[MAXORD][MAXORD]; 
{
    int		i, j, k;
    double	fd[MAXORD];
    double	sptofd[MAXORD][MAXORD];
    double	dtpow[MAXORD];

    register	double	a0;

    dtpow[0] = 1.0; 			/* make scale matrix */
    for ( i = 1; i < order; i++ )
	dtpow[i] = dtpow[i-1] * dt;

    for ( i = 0; i < order; i++ )	/* scale matrix x fwd matrix */
	for (j = i; j < order; j++ )
	    sptofd[i][j] = mi_nu_ptofd[i][j] * dtpow[j];

    for ( i = 0; i < 3; i++ ) {		/* x, y, z */
	for ( j = 0; j < order; j++ ) {
	    a0 = 0.0;
	    for ( k = j; k < order; k++ )
		a0 += sptofd[j][k] * poly[i][k];
	    fd[j] = a0;
	}
	for ( j = 0; j < order; j++ )
	    poly[i][j] = fd[j];
    }

    if ( DD_IsVert4D(pt_type) ) {
	for ( j = 0; j < order; j++ ) {
	    a0 = 0.0;
	    for ( k = j; k < order; k++ )
		a0 += sptofd[j][k] * poly[WW][k];
	    fd[j] = a0;
	}
	for ( j = 0; j < order; j++ )
	    poly[WW][j] = fd[j];
    }
}



/*++
 |
 |  Function Name:	ofd_curve
 |
 |  Function Description:
 |
 |  Note(s):
 |
 --*/
static 
void
ofd_curve( pt_type, order, knot, num_segs, dt, A, pts )
    ddPointType    pt_type;
    ddUSHORT	order; 
    ddFLOAT		*knot;
    int			num_segs;
    float		dt;
    register double	A[4][MAXORD];
    char		*pts;
{
    register int	i, j;
    register int	point_size;

    DD_VertPointSize(pt_type, point_size);

    /* Move to the first point of the span. */
    ((ddCoord4D *)pts)->x = A[XX][0]; 
    ((ddCoord4D *)pts)->y = A[YY][0]; 
    ((ddCoord4D *)pts)->z = A[ZZ][0]; 
    ((ddCoord4D *)pts)->w = A[WW][0];
    pts += point_size;

    for ( i = 1; i <= num_segs; i++ ) {		/* number of steps */
	for ( j = 0; j < order - 1; j++ ) {	/* forward difference */
	    A[XX][j] += A[XX][j+1];
	    A[YY][j] += A[YY][j+1];
	}

	((ddCoord4D *)pts)->x = A[XX][0];
	((ddCoord4D *)pts)->y = A[YY][0];

	if ( !DD_IsVert2D(pt_type) ) {

	  for ( j = 0; j < order - 1; j++ )
		A[ZZ][j] += A[ZZ][j+1];

	  ((ddCoord4D *)pts)->z = A[ZZ][0];

	  if ( DD_IsVert4D(pt_type) ) {

	    for ( j = 0; j < order - 1; j++ )
		A[WW][j] += A[WW][j+1];

	    ((ddCoord4D *)pts)->w = A[WW][0];
	  }
	}

	pts += point_size;
    }

    return;
}



/*++
 |
 |  Function Name:	nu_compute_nurb_curve
 |
 |  Function Description:
 |
 |  Note(s):
 |
 --*/
static
ddpex3rtn
nu_compute_nurb_curve( pddc, curve, aptype, apval, curve_list )
    miDDContext		*pddc;
    miNurbStruct	*curve;
    int			aptype;
    ddFLOAT		apval;
    miListHeader	**curve_list;
{
    ddUSHORT		 order = curve->order;
    int		 i;

    miListHeader *control_points = &curve->points;
    ddFLOAT	 (*rknots)[MAXORD]=0;		/* reciprocal of knot diff */
    double	 C[MAXORD][MAXORD];	/* bspline to poly matrix */
    double	 A[4][MAXORD];		/* xyzw curve coefficients */
    int		 numKnots;		/* number of knots aftetr insertion */
    int		 numnewKnots;
    ddFLOAT	 *knots = 0;		/* new knots after insertion */
    ddCoord4D	 *cpts = 0;		/* new ctrl pts after insertion */
    ddCoord4D	 *out_pts;		/* output data pointer */
    listofddPoint   *pddlist;		/* data list pointer */
    int		 num_cpts, num_subsegs, num_additional_knots;
    float	 additional_knots[2];
    float	 dt;

    if (curve->numKnots != control_points->ddList->numPoints + order)
	return 0;

    if ( !(curve->numKnots > 0 && control_points->ddList->numPoints > 0) )
	return 0;

    /* Trimming by knot insertion (if needed). */
    num_additional_knots = 0;
    if ( curve->uMin > curve->pKnots[order-1] )
	additional_knots[num_additional_knots++] = curve->uMin;

    if ( curve->uMax < curve->pKnots[curve->numKnots - order] )
	additional_knots[num_additional_knots++] = curve->uMax; 
    
    if ( num_additional_knots > 0 ) {

	if ( !( knots = (ddFLOAT *)
	    xalloc((num_additional_knots + curve->numKnots)*sizeof(float))))
	    goto no_mem;

	if ( !( cpts = (ddCoord4D *)
	      xalloc( (num_additional_knots + control_points->ddList->numPoints)
			* sizeof(ddCoord4D))) )
	    goto no_mem;

	numnewKnots = num_additional_knots;
	for ( i = 0; i < num_additional_knots; i++ )
	    knots[i] = additional_knots[i];

	if ( !mi_nu_insert_knots( order, control_points->type,
				  curve->numKnots, curve->pKnots, 
				 (ddFLOAT *)(control_points->ddList->pts.ptr), 
				  &numnewKnots, knots, 
				  (ddFLOAT *)cpts ) )
	    goto no_mem;

	numKnots = numnewKnots;
	num_cpts = numnewKnots - order;

    } else {

	numKnots = curve->numKnots;
	knots = curve->pKnots;
	num_cpts = numKnots - order;
	cpts = control_points->ddList->pts.p4Dpt;
    }



    /* Build the tessellation. */
    if ( order > MAXORD || order == 1 ) {

	/* 
	 * if order > MAXORD, Draw only the control polygon. 
	 * if order == 1, draw point at each control point.
	 */
	*curve_list = MI_NEXTTEMPDATALIST(pddc);
	MI_ALLOCLISTHEADER((*curve_list), 1);

	if ( DD_IsVert4D(control_points->type) ) {

	  ddCoord4D	*in_pt4D, *out_pt4D;

	  (*curve_list)->type = DDPT_4D;
	  MI_ALLOCLISTOFDDPOINT( (*curve_list)->ddList, num_cpts,
				 sizeof(ddCoord4D) );
	  out_pt4D = (*curve_list)->ddList->pts.p4Dpt;
          if ( !out_pt4D ) return(BadAlloc);
	  in_pt4D = cpts;

	  for ( i = 0; i < num_cpts; i++ ) *(out_pt4D++) = *(in_pt4D++);
	    
	} else if ( DD_IsVert3D(control_points->type) ) {

	  ddCoord3D	*in_pt3D, *out_pt3D;

	  (*curve_list)->type = DDPT_3D;
	  MI_ALLOCLISTOFDDPOINT( (*curve_list)->ddList, num_cpts,
				 sizeof(ddCoord3D) );
	  out_pt3D = (*curve_list)->ddList->pts.p3Dpt;
          if ( !out_pt3D ) return(BadAlloc);
	  in_pt3D = (ddCoord3D *)cpts;

	  for ( i = 0; i < num_cpts; i++ ) *(out_pt3D++) = *(in_pt3D++);

	} else {

	  ddCoord2D	*in_pt2D, *out_pt2D;

	  (*curve_list)->type = DDPT_2D;
	  MI_ALLOCLISTOFDDPOINT( (*curve_list)->ddList, num_cpts,
				 sizeof(ddCoord2D) );
	  out_pt2D = (*curve_list)->ddList->pts.p2Dpt;
          if ( !out_pt2D ) return(BadAlloc);
	  in_pt2D = (ddCoord2D *)cpts;

	  for ( i = 0; i < num_cpts; i++ ) *(out_pt2D++) = *(in_pt2D++);

	}

    } else {	/* ( order > 1 and <= MAXORD ) */

	switch( aptype ) {
	    case PEXApproxImpDep:
	    case PEXApproxConstantBetweenKnots:
	    default:
		if ( apval <= 0.0 ) {
		    dt = 1.0;
		    num_subsegs = 1;

		} else {
		    dt = 1.0 / (apval + 1.0);
		    num_subsegs = apval + 1; /* assume it's been clamped */
		}

		if ( !( rknots = (ddFLOAT (*)[MAXORD])
		    xalloc( MAXORD * numKnots * sizeof(float))) )
		    goto no_mem;

		mi_nu_preprocess_knots( order, numKnots, knots, rknots );

		/* build a new list for each span */
		(*curve_list) = MI_NEXTTEMPDATALIST(pddc);
		(*curve_list)->type = control_points->type;
		(*curve_list)->numLists = 0;
		MI_ALLOCLISTHEADER((*curve_list), num_cpts);
		if (!(pddlist = (*curve_list)->ddList )) return(BadAlloc);

		for ( i = order - 1; i < num_cpts; i++ ) { 
		    if ( knots[i+1] > knots[i]
			&& knots[i] >= curve->uMin
			&& knots[i+1] <= curve->uMax )
		    {
			mi_nu_compute_nurb_basis_function( order, i,
							   knots, rknots, C );
			multiply_basis_func_control_pts( control_points->type,
							 order, i, 
							 (char *)cpts, C, A );
			compute_fwd_matrix2D( control_points->type,order,dt,A );

			MI_ALLOCLISTOFDDPOINT(pddlist, num_subsegs + 1, 
					      sizeof(ddCoord4D));
			if (!(out_pts = pddlist->pts.p4Dpt)) 
			  return(BadAlloc);

			ofd_curve( control_points->type, order, &knots[i], 
				   num_subsegs, dt, A, (char *)out_pts );

			(pddlist++)->numPoints = num_subsegs + 1;
			(*curve_list)->numLists += 1;
		    }
		}

		break;
	} /* end switch on approx type */
    }

    if (knots != curve->pKnots) xfree(knots);
    if (cpts != control_points->ddList->pts.p4Dpt) xfree(cpts);
    xfree(rknots);

    return 0;

no_mem:
    if ((knots)&&(knots != curve->pKnots)) xfree(knots);
    if ((cpts)&&(cpts != control_points->ddList->pts.p4Dpt)) xfree(cpts);
    if (rknots) xfree(rknots);

    return (BadAlloc);
}



/*++
 |
 |  Function Name:	compute_adaptive_crv_interval
 |
 |  Function Description:
 |
 |  Note(s):
 |
 --*/
static 
ddpex3rtn
compute_adaptive_crv_interval( pddc, curve, xform, apxval )
    miDDContext		*pddc;
    miNurbStruct	*curve;
    ddFLOAT		xform[4][4];
    ddFLOAT		*apxval;
{
    ddFLOAT		a_coeff, b_coeff, c_coeff, denom, z1, z2, z3;
    int			i, use_z_coord = 0;
    int			npts = curve->points.ddList->numPoints;
    ddCoord4D		*xpts, *pa, *pb, *pc;
    ddPointUnion	in_pt;
    double		w, perp_d, max_perp_d = 0.0;
    miListHeader	*tmp_list;
    int			point_size;
    ddpex3rtn		status;

    /* 
     * Compute the constant parametric between knots interval needed to
     * come close to meeting the specified approximation criteria.  The
     * method used is a gross compromise for the adaptive approximation
     * criteria, but fulfills the basic need for an adaptive approximation
     * method.
     * Note that for the NPC approximation criteria NPC isn't even the space
     * being used, it's clipping space, which is [-1,1], [-1,1], [0,1].
     * The Z component is ignored in this case though since the Z dimension
     * is perpendicular to the screen.
     */
    *apxval = 0.0;
    switch ( pddc->Static.attrs->curveApprox.approxMethod ) {
	case PEXApproxNpcChordalSize:
	case PEXCurveApproxNpcChordalDev:
	    use_z_coord = 0;
	    break;

	case PEXApproxWcsChordalSize:
	case PEXCurveApproxWcsChordalDev:
	    use_z_coord = 1;
	    break;
    }

    if ( xform ) {
      if (status = miTransform( pddc, &curve->points, &tmp_list, 
				xform, NULL4x4, DD_HOMOGENOUS_POINT))
	return (status);

      /* Normalize result by w */
      xpts = tmp_list->ddList->pts.p4Dpt;
      for ( i = 0, pa = xpts; i < npts; i++, pa++ ) {
	w = 1.0 / pa->w;
	pa->x *= w; pa->y *= w;
	if ( use_z_coord ) pa->z *= w;
      }

    } else {

      DD_VertPointSize(curve->points.type, point_size);

      if ( !( xpts = (ddCoord4D *)xalloc(npts * sizeof(ddCoord4D))) ) {
	return BadAlloc;
      }

      if ( DD_IsVert4D(curve->points.type) ) {

        /* if homogeneous values, then normalize by w */
        for ( i = 0, in_pt.p4Dpt = curve->points.ddList->pts.p4Dpt, pa = xpts;
	      i < npts; 
	      i++ ) {

	  w = 1.0 / in_pt.p4Dpt->w;
	  pa->x = in_pt.p4Dpt->x * w; 
	  pa->y = in_pt.p4Dpt->y * w;
	  if ( use_z_coord ) pa->z = in_pt.p4Dpt->z * w;

	  in_pt.ptr += point_size;
	  pa++;
        }

      } else {

	if ( DD_IsVert2D(curve->points.type) ) use_z_coord = 0;

        /* Copy 2D or 3D points into 4D array */
        for ( i = 0, in_pt.p4Dpt = curve->points.ddList->pts.p4Dpt, pa = xpts;
	      i < npts; 
	      i++ ) {

	  pa->x = in_pt.p4Dpt->x; 
	  pa->y = in_pt.p4Dpt->y;
	  if ( use_z_coord ) pa->z = in_pt.p4Dpt->z;

	  in_pt.ptr += point_size;
	  pa++;
        }

      }

    }

    /* 
     * For the above approx. types, the approx. value is the max. allowable
     * distance between the actual curve and the generated chord.
     * The distance of the ctrl point from the line joining the ctrl pts on
     * either side of it is calculated for every ctrl pt. This is calculated
     * in 2D. For approx. in WC, the 3D length is got from the 2D-length
     * and the z values of the ctrl pts. The max of all these lengths is
     * found. The final approx. value is obtd. from the ratio of the max length
     * and the required approx. value.
     */

    pa = xpts; pb = pa + 2, pc = pa + 1;
    for ( i = 1; i < npts-1; i++, pa++, pb++, pc++ ) {
	a_coeff = pb->y - pa->y;
        b_coeff = pa->x - pb->x;
        c_coeff = pb->x * pa->y - pa->x * pb->y;
        denom = ( a_coeff * a_coeff + b_coeff * b_coeff );
        perp_d = (a_coeff * pc->x + b_coeff * pc->y + c_coeff);
	if ( use_z_coord ) {
            z1 = pc->z;
            z2 = (pa->z + pb->z) /2.0;
	    z3 = z1 - z2;
            perp_d = sqrt( (perp_d * perp_d + z3 * z3 * denom) /denom );
        } else 
	    perp_d = perp_d/ (sqrt(denom));
        perp_d = fabs(perp_d);
	if ( perp_d > max_perp_d )
            max_perp_d = perp_d;
    }

    *apxval = (int)(1 + sqrt( 10*max_perp_d / 
			(pddc->Static.attrs->curveApprox.tolerance > 0.0 
			  ? pddc->Static.attrs->curveApprox.tolerance : 0.01)));

    if (xpts != tmp_list->ddList->pts.p4Dpt) xfree(xpts);

    return Success;
}

/*++
 |
 |  Function Name:	tessellate_curve
 |
 |  Function Description:
 |
 |  Note(s):
 |
 --*/ 
ddpex3rtn
tessellate_curve( pddc, curve, curve_list, xform )
    miDDContext	 	*pddc;
    miNurbStruct	*curve;
    miListHeader	**curve_list;
   ddFLOAT		xform[4][4];
{
    int			approx_type;
    ddFLOAT		approx_value;
    ddpex3rtn		status = 0;	/* !0 if successful */

    if ( curve->points.ddList->numPoints <= 0 )
	return status;

    switch ( pddc->Static.attrs->curveApprox.approxMethod ) {
	case PEXApproxImpDep:
	case PEXApproxConstantBetweenKnots:
	    approx_type = PEXApproxConstantBetweenKnots;
	    approx_value=MAX((int)pddc->Static.attrs->curveApprox.tolerance,0);
	    break;

	case PEXApproxWcsChordalSize:
	case PEXApproxNpcChordalSize:
	case PEXCurveApproxWcsChordalDev:
	case PEXCurveApproxNpcChordalDev:
	    /* The same approximation method is used for all these
	     * approximation types, and it's not exactly any of them.
	     * But the method used serves the same purpose and
	     * PHIGS PLUS allows us to approximate the defined methods.
	     */
	    approx_type = PEXApproxConstantBetweenKnots;
	    compute_adaptive_crv_interval( pddc, curve, xform, 
					   &approx_value );
	    break;

	default:
	    approx_type = PEXApproxConstantBetweenKnots;
	    approx_value = 1.0;
	    break;
    }

    return(nu_compute_nurb_curve( pddc, curve, 
				  approx_type, approx_value,
				  curve_list ));
}
