/* $Xorg: miNSurf.c,v 1.4 2001/02/09 02:04:10 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/level2/miNSurf.c,v 3.9 2001/12/14 19:57:29 dawes Exp $ */

#define TRIMING 1

#include "mipex.h"
#include "misc.h"
#include "miscstruct.h"
#include "PEXErr.h"
#include "miStruct.h"
#include "miRender.h"
#include "gcstruct.h"
#include "ddpex2.h"
#include "miNurbs.h"
#include "pexos.h"

#if !defined(IN_MODULE) /* hv: not a module header file */
#include <math.h>
#else
#include <xf86_ansic.h>
#endif

static ddpex3rtn       build_surf_reps();
static int             add_grid();
static int             uniform_isocurves();
static int             nonuniform_isocurves();
static void	       nurb_surf_state_free();
static ddpex3rtn       compute_adaptive_surf_interval();
static void	       determine_reps_required();
static ddpex3rtn       compute_nurb_surface();
static ddpex3rtn       build_facets();
static ddpex3rtn       build_control_polygon();
static ddpex3rtn       build_surf_markers();
static ddpex3rtn       span_grids();
static void	       compute_edge_point_normals();
static void	       build_edge_reps();
static void	       make_edge_segments();
static void	       span_evaluation_points();
#ifdef TRIMING
static ddpex3rtn       add_pgon_point();
#endif


/* 
 * This convention is established in the trimming code.
 * Note that it's clockwise from lower left.
 */
#define LL	0
#define LR	3
#define UR	2
#define UL	1

#define xin(_a,_b,_x) ((_x) >= (_a) && (_x) <= (_b))

/*++
 |
 |  Function Name:	miNurbsSurface
 |
 |  Function Description:
 |	Handles the Nurbs Surface Pex OC.
 |
 |	Rendering a surface path is a 4 step process in this
 |	implementation. 
 |
 |	The first step is to determine the proper parametric step 
 |	size according to the specified surface tolerance. Note 
 |	that tesselation is performed in Model Coordinates in this 
 |	implementation. However, surface tolerances are specified 
 |	in one of WC, NPC, or DC.  Thus the control points are tranformed 
 |	in a temporary buffer into the specified space for determination 
 |	of the maximum parametric step size. 
 |
 |	Once this step size is determined, a series of grid descriptions 
 |	are created - one grid per knot interval. In other words, 
 |	each grid describes the area specified by the four knot pairs
 |	(u,v), (u+1,v), (u,v+1), (u+1,v+1). These grids are then 
 |	subdivided according to the u,v step size computed above 
 |	and the (x,y,z,w) coordinates corresponding to each of the 
 |	(u,v) steps computed. The result of this operation is a linked
 |	list of (u,v) (x,y,z,w) pairs for each span in the surface. 
 |
 |	The third step in the process is to trim this surface according 
 |	to the supplied trim curve bounds. NEED MORE TEXT HERE. 
 |
 |	The fourth and last step in the procedure is to convert
 |	the (potentially trimmed) grid description od the surface
 |	into miListHeader point lists that can be used by the
 |	rest of the system. Note that no "direct" rendering is performed
 |	by this routine. Rather, the data that resulted from the 
 |	previous steps are converted into one of the other primitive
 |	in the system (ie SOFAS, quad mesh, polyline, etc...) and
 |	rendered by one of these routines.
 |
 |  Note(s):
 |
 --*/

ddpex3rtn
miNurbsSurface(pRend, pExecuteOC)
/* in */
    ddRendererPtr       pRend;	  /* renderer handle */
    miGenericStr       *pExecuteOC;
{
/* calls */
    ddpex3rtn		build_surf_reps();
    extern ocTableType	InitExecuteOCTable[];

/* Local variable definitions */
    miDDContext		*pddc = (miDDContext *)(pRend->pDDContext);
    miNurbSurfaceStruct	*ddSurface = (miNurbSurfaceStruct *)(pExecuteOC+1);
    miListHeader	*input_list = &ddSurface->points;
    miListHeader	*listheader;
    Nurb_surf_state	surface_state;
    miGenericStr	*pGStr;
    miMarkerStruct	*ddMarker;
    miPolylineStruct	*ddPolyline;
    miFillAreaStruct	*ddFillArea;
    Nurb_grid		*grid;
    miQuadMeshStruct	*ddQuad;
    miSOFASStruct	*ddSOFAS;
    listofddFacet	facet_list;
    ddUSHORT		save_edges;
    ddEnumTypeIndex	save_intStyle;
    int			num_points;
    int			i, j;
    ddpex3rtn		status = Success;



    switch(pddc->Static.attrs->surfApprox.approxMethod) {

	case PEXApproxImpDep:
	case PEXApproxConstantBetweenKnots:
	case PEXApproxDcChordalSize:
	case PEXSurfaceApproxDcPlanarDev:
	case PEXApproxDcRelative:
	default:

	case PEXApproxWcsChordalSize:
	case PEXSurfaceApproxWcsPlanarDev:
	case PEXApproxWcsRelative:

	   /* Transform to WC prior to tesselation */
	   /* tesselate surface into facets */
	   if (status = build_surf_reps( pddc, ddSurface, &surface_state,
					 pddc->Dynamic->mc_to_wc_xform ))
	     goto exit;

	   break;


	case PEXApproxNpcChordalSize:
	case PEXSurfaceApproxNpcPlanarDev:
	case PEXApproxNpcRelative:

	   /* tesselate surface into facets */
	   if (status = build_surf_reps( pddc, ddSurface, &surface_state,
					 pddc->Dynamic->mc_to_npc_xform ))
	     goto exit;

	   break;
    }


    /*
     * render the computed structures.
     *
     * Note that the final structure can be rendered in many ways
     * according to the surface data and rendering attributes.
     * the surface_state.reps structure contains flags to indicate
     * the data created during the tesselation process.
     */
    if ( surface_state.reps.markers ) {

	/* allocate polyline command block */
	if (!(pGStr = (miGenericStr *) (xalloc(sizeof(miGenericStr) +
                                               sizeof(miMarkerStruct))))) {
	  status = BadAlloc;
          goto exit;
	}

	pGStr->elementType = PEXOCMarker;
	/* The length data is ignored by the rendering routine and hence is */
	/* left as whatever GARBAGE that will be present at the alloc time. */

	ddMarker = (miMarkerStruct *) (pGStr + 1);
	*ddMarker = *((miMarkerStruct *)(surface_state.markers));
	status = InitExecuteOCTable[(int)(pGStr->elementType)](pRend, pGStr);

	xfree(pGStr);

    } else {

      if ( surface_state.reps.facets ) {

	/* Don't draw edges here - they are drawn seperately */
	save_edges = pddc->Static.attrs->edges;
	pddc->Static.attrs->edges = PEXOff;

	if ( ddSurface->numTrimCurveLists <= 0 ) {
	  /* 
	  * If no trimming, then each tesselated surface
	  * is described by a number of grids, each grid containing
	  * a single quad mesh. Therefore, loop through the
	  * grid list in the surface state, and call quad mesh
	  * for each grid.
	  */

	  /* allocate polyline command block */
	  if (!(pGStr = (miGenericStr *) (xalloc(sizeof(miGenericStr) +
                                               sizeof(miQuadMeshStruct))))) {
	    status = BadAlloc;
            goto exit;
	  }

	  pGStr->elementType = PEXOCQuadrilateralMesh;
	  /* The length data is ignored by the rendering routine and hence is */
	  /* left as whatever GARBAGE that will be present at the alloc time. */

	  ddQuad = (miQuadMeshStruct *) (pGStr + 1);

	  /* Initialize quad mesh structure */
	  /*** ddQuad->shape =  ***/

	  facet_list.numFacets = 0;
	  facet_list.type = DD_FACET_NONE;
	  facet_list.facets.pNoFacet = NULL;
	  facet_list.maxData = 0;
	  ddQuad->pFacets = &facet_list;
	
	  grid = surface_state.grids.grids;
	  listheader = surface_state.facets;

	  for (i = 0; i < surface_state.grids.number; i++) {

	   ddQuad->mPts = grid->nu;
	   ddQuad->nPts = (grid++)->nv;
	   ddQuad->points = *(listheader++);

	   if(status=InitExecuteOCTable[(int)(pGStr->elementType)](pRend,pGStr))
	     break;

	  }

	} else {
	  /*
	   * If trimming is enabled, then SOFAS are output from 
	   * the tesselation code.
	   * The SOFAS structure is already built, so just call the
	   * SOFAS routine.
	   */

	  /* allocate polyline command block */
	  if (!(pGStr = (miGenericStr *) (xalloc(sizeof(miGenericStr) +
                                               sizeof(miSOFASStruct))))) {
	    status = BadAlloc;
            goto exit;
	  }

	  pGStr->elementType = PEXOCSOFAS;
	  /* The length data is ignored by the rendering routine and hence is */
	  /* left as whatever GARBAGE that will be present at the alloc time. */

	  ddSOFAS = (miSOFASStruct *) (pGStr + 1);
	  *ddSOFAS = *((miSOFASStruct *)(surface_state.sofas));

	  status = InitExecuteOCTable[(int)(pGStr->elementType)](pRend, pGStr);

	}

	xfree(pGStr);
	/* restore edge flag */
	pddc->Static.attrs->edges = save_edges;

     } else if ( surface_state.reps.hollow ) {

	/* allocate fill area command block */
	if (!(pGStr = (miGenericStr *) (xalloc(sizeof(miGenericStr) +
                                               sizeof(miFillAreaStruct))))) {
	  status = BadAlloc;
          goto exit;
	}

	pGStr->elementType = PEXOCFillAreaSet;
	/* The length data is ignored by the rendering routine and hence is */
	/* left as whatever GARBAGE that will be present at the alloc time. */

	ddFillArea = (miFillAreaStruct *) (pGStr + 1);
	ddFillArea->shape = PEXUnknownShape;
	ddFillArea->ignoreEdges = PEXOn;
	ddFillArea->contourHint = PEXUnknownContour;
	facet_list.numFacets = 0;
	facet_list.type = DD_FACET_NONE;
	facet_list.facets.pNoFacet = NULL;
	facet_list.maxData = 0;
	ddFillArea->pFacets = &facet_list;
	ddFillArea->points = *(surface_state.hollow);
	status = InitExecuteOCTable[(int)(pGStr->elementType)](pRend, pGStr);

	xfree(pGStr);

     }

     if ( surface_state.reps.isocrvs ) {

	/* allocate polyline command block */
	if (!(pGStr = (miGenericStr *) (xalloc(sizeof(miGenericStr) +
                                               sizeof(miPolylineStruct))))) {
	  status = BadAlloc;
          goto exit;
	}

	pGStr->elementType = PEXOCPolylineSet;
	/* The length data is ignored by the rendering routine and hence is */
	/* left as whatever GARBAGE that will be present at the alloc time. */

	ddPolyline = (miPolylineStruct *) (pGStr + 1);
	*ddPolyline = *((miPolylineStruct *)(surface_state.isocrvs));
	status = InitExecuteOCTable[(int)(pGStr->elementType)](pRend, pGStr);

	xfree(pGStr);

     }

     if ( surface_state.reps.edges ) {

	/* set edge flag and interior style such that only edges are drawn */
	save_edges = pddc->Static.attrs->edges;
	pddc->Static.attrs->edges = PEXOn;

	save_intStyle = pddc->Static.attrs->intStyle;
	pddc->Static.attrs->intStyle = PEXInteriorStyleEmpty;

	/* allocate fill area command block */
	if (!(pGStr = (miGenericStr *) (xalloc(sizeof(miGenericStr) +
                                               sizeof(miFillAreaStruct))))) {
	  status = BadAlloc;
          goto exit;
	}

	pGStr->elementType = PEXOCFillAreaSet;
	/* The length data is ignored by the rendering routine and hence is */
	/* left as whatever GARBAGE that will be present at the alloc time. */

	ddFillArea = (miFillAreaStruct *) (pGStr + 1);
	ddFillArea->shape = PEXUnknownShape;
	ddFillArea->ignoreEdges = PEXOff;
	ddFillArea->contourHint = PEXUnknownContour;
	facet_list.numFacets = 0;
	facet_list.type = DD_FACET_NONE;
	facet_list.facets.pNoFacet = NULL;
	facet_list.maxData = 0;
	ddFillArea->pFacets = &facet_list;
	ddFillArea->points = *(surface_state.edges);
	status = InitExecuteOCTable[(int)(pGStr->elementType)](pRend, pGStr);

	xfree(pGStr);

	/* restore edge flag */
	pddc->Static.attrs->edges = save_edges;
	/* restore interior style */
	pddc->Static.attrs->intStyle = save_intStyle;

      }
    }

exit:

    /* free all temporary storage */
    nurb_surf_state_free(&surface_state);

    return (status);

}



#define ANY_REP_NEEDED(_st) \
    ( (_st)->reps.edges || (_st)->reps.facets || (_st)->reps.isocrvs \
	|| (_st)->reps.markers || (_st)->reps.hollow )

#define NEED_NORMALS(_st) ( (_st)->reps.facets )

/*++
 |
 |  Function Name:      build_surf_reps
 |
 |  Function Description:
 |
 |  Note(s):
 |
 --*/

static
ddpex3rtn
build_surf_reps( pddc, surface, state, trans )
    miDDContext         *pddc;
    miNurbSurfaceStruct *surface;
    Nurb_surf_state	*state;
    ddFLOAT		trans[4][4];
{
    /* uses */
    ddpex3rtn		status = Success;	/* assume success */

    if ( surface->mPts <= 0 || surface->nPts <= 0 )
	return 0;

    NURB_SURF_STATE_INIT(state);

    switch ( pddc->Static.attrs->surfApprox.approxMethod ) {
	case PEXApproxConstantBetweenKnots:
	case PEXApproxImpDep:
	default:
	    state->approx_type = PEXApproxConstantBetweenKnots;
	    state->approx_value[0] = 
			MAX((int)pddc->Static.attrs->surfApprox.uTolerance,0);
	    state->approx_value[1] = 
			MAX((int)pddc->Static.attrs->surfApprox.vTolerance,0);
	    break;

	case PEXApproxWcsChordalSize:
	case PEXApproxNpcChordalSize:
	case PEXSurfaceApproxWcsPlanarDev:
	case PEXSurfaceApproxNpcPlanarDev:
	case PEXApproxWcsRelative:
	case PEXApproxNpcRelative:
	    /* The same approximation method is used for all these
	     * approximation types, and it's not exactly any of them.
	     * But the method used serves the same purpose and
	     * PHIGS PLUS allows us to approximate the defined methods.
	     */
	    state->approx_type = PEXApproxConstantBetweenKnots;
	    compute_adaptive_surf_interval( pddc, surface, state, trans );
	    break;
    }

    determine_reps_required( pddc, surface, state );

    if ( ANY_REP_NEEDED(state) ) {
	status = compute_nurb_surface( pddc, surface, state );
    }

    return ( status );
}



/*++
 |
 |  Function Name:      compute_adaptive_surf_interval
 |
 |  Function Description:
 |
 |	This routine computes the number of steps that must
 |	be taken in the u & v directions. Note that although
 |	the actual tesselation takes place in model coordinates
 |	(to ease integration with the remainder of the rendering code),
 |	the control points must be transformed here to the
 |	space specified by the curve approximation criteria
 |	to guarantee the proper number of steps are computed.
 |
 |  Note(s):
 |
 --*/

static 
ddpex3rtn
compute_adaptive_surf_interval( pddc, surface, state, trans )
    miDDContext         *pddc;
    miNurbSurfaceStruct *surface;
    Nurb_surf_state	*state;
    ddFLOAT		trans[4][4];
{
/*  uses  */
    ddFLOAT	uval, vval, a_coeff, b_coeff, c_coeff, denom,
		z1, z2, z3;
    ddCoord4D	p;
    double	perp_d, max_u_perp_d = 0.0, max_v_perp_d = 0.0;
 
    int		i, j, use_z_coord = 0;
    int		nu = surface->mPts;
    int		nv = surface->nPts;
    ddCoord4D	*upper, *lower, *middle, *pa, *pb, *pc, *coord_buf;
    char	*ctlpts, *pin;
    double	w;
    miListHeader *input = &surface->points;
    char	rat;
    int		point_size;
    ddPointType out_type;
    ddpex3rtn	status;
 
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
    state->approx_value[0] = 0.0;
    state->approx_value[1] = 0.0;
    switch ( pddc->Static.attrs->surfApprox.approxMethod ) {
        case PEXApproxNpcChordalSize:
        case PEXSurfaceApproxNpcPlanarDev:
            use_z_coord = 0;
            break;
        case PEXApproxWcsChordalSize:
        case PEXSurfaceApproxWcsPlanarDev:
            use_z_coord = 1;
            break;
    }

    /*
     * transform comtrol points into space specified by
     * approximation tolerance
     */
    if ( trans ) {
      miListHeader	*temp;

      /* Transform to WC prior to applying lighting */
      out_type = input->type;
      if (status = miTransform( pddc, input, &temp,
			        trans, NULL4x4, DD_SetVert4D(out_type)))
          return (status);
      input = temp;
    }

    rat = DD_IsVert4D(input->type);
    ctlpts = input->ddList->pts.ptr;
    DD_VertPointSize(input->type, point_size);


    /* Allocate temporary control point store */
    if (!(coord_buf = (ddCoord4D *)xalloc(3 * nu * sizeof(ddCoord4D))))
      return(BadAlloc);

    /* 
     * For the above approx. types, the approx. value is the max.  allowable
     * distance between the actual surface and the generated segments.
     * The distance of the ctrl point from the line joining the ctrl pts on
     * either side of it is calculated for every ctrl pt. This is calculated
     * in 2D. For approx. in WC, the 3D length is got from the 2D-length
     * and the z values of the ctrl pts. The max of all these lengths is
     * found. This is repeated for all the ctrl pts in the u and v directions.
     * The final approx. value is obtd. from the ratio of the max length
     * and the required approx. value.
     */

    upper = coord_buf; middle = coord_buf + nu; lower = coord_buf + 2*nu;
    for ( j = 0; j < nv-1; j++, ctlpts += nu*point_size ) {

        /* 
	 * project the points from homogeneous space.
         */
	if ( rat ) {
          for ( i = 0, pin = ctlpts, pa = lower; 
		i < nu; 
		i++, pa++, pin += point_size ) {
	    if (((ddCoord4D *)pin)->w == 1.0) *pa = *((ddCoord4D *)pin);
	    else {
              w = 1.0 / ((ddCoord4D *)pin)->w;
              pa->x = ((ddCoord4D *)pin)->x * w; 
	      pa->y = ((ddCoord4D *)pin)->y * w;
              if ( use_z_coord ) pa->z = ((ddCoord4D *)pin)->z * w;
	    } 
          }
	} else {
          for ( i = 0, pin = ctlpts, pa = lower; 
		i < nu; 
		i++, pa++, pin += point_size ) {
	    memcpy( (char *)pa, pin, point_size);
	    pa->w = 1.0;
	  }
	}

        /* Find the required u interval between points of this row.  */
        pa = lower; pb = pa + 2, pc = pa + 1;
        for ( i = 1; i < nu-1; i++, pa++, pb++,pc++) {
            a_coeff = pb->y - pa->y;
            b_coeff = pa->x - pb->x;
            c_coeff = pb->x * pa->y - pa->x * pb->y;
            denom = ( a_coeff * a_coeff + b_coeff * b_coeff );
            perp_d = (a_coeff * pc->x + b_coeff * pc->y + c_coeff);
            if ( use_z_coord ) {
                z1 = pc->z;
                z2 = (pa->z + pb->z) /2.0;
                z3 = z1-z2;
                perp_d = sqrt( (perp_d * perp_d + z3 * z3 * denom) /denom );
            } else {
                perp_d = perp_d/ (sqrt(denom));
            }
            perp_d = fabs(perp_d);
            if ( perp_d > max_u_perp_d )
                max_u_perp_d = perp_d;
        }

        if ( j > 1 ) {
            /* Find the required v interval between these two rows.  */
            pa = upper; pb = lower, pc = middle;
            for ( i = 0; i < nu; i++, pa++, pb++, pc++ ) {
                a_coeff = pb->y - pa->y;
                b_coeff = pa->x - pb->x;
                c_coeff = pb->x * pa->y - pa->x * pb->y;
                denom = ( a_coeff * a_coeff + b_coeff * b_coeff );
                perp_d = (a_coeff * pc->x + b_coeff * pc->y + c_coeff);
                if ( use_z_coord ) {
                    z1 = pc->z;
                    z2 = (pa->z + pb->z) /2.0;
                    z3 = z1-z2;
                    perp_d = sqrt( (perp_d * perp_d + z3 * z3 * denom) /denom );
                } else {
                    perp_d = perp_d/ (sqrt(denom));
                }
                perp_d = fabs(perp_d);
                if ( perp_d > max_v_perp_d )
                    max_v_perp_d = perp_d;
            }
        }    

        /* Swap row pointers so that next row is the "lower" row. */
        pa = upper;
        upper = middle;
        middle = lower;
        lower = pa;
    }

    switch ( pddc->Static.attrs->surfApprox.approxMethod ) {
        case PEXApproxWcsChordalSize:
        case PEXApproxNpcChordalSize:
            uval = pddc->Static.attrs->surfApprox.uTolerance;
            vval = pddc->Static.attrs->surfApprox.vTolerance;
            break;
        case PEXSurfaceApproxWcsPlanarDev:
        case PEXSurfaceApproxNpcPlanarDev:
            uval = pddc->Static.attrs->surfApprox.uTolerance;
            vval = pddc->Static.attrs->surfApprox.uTolerance;
            break;
    }
    state->approx_value[0] = (int)( 1 + sqrt(10*max_u_perp_d
                / (uval > 0.0 ? uval : 0.01)));
    state->approx_value[1] = (int)( 1 + sqrt(10*max_v_perp_d
                / (vval > 0.0 ? vval : 0.01)));

    xfree(coord_buf);

    return(Success);
}



/*++
 |
 |  Function Name:      determine_reps_required
 |
 |  Function Description:
 |
 |	This routine selects the format of the data
 |	created for rendering the speecified surface patch.
 |	Different data descriptions are generated according
 |	to both the surface data and the current rendering
 |	attributes. For example, quad mesh data is created
 |	to describe an un-trimmed solid surface; polyline
 |	data, however, is created if surface iso-curves
 |	are enabled.
 |
 |  Note(s):
 |
 --*/

static void
determine_reps_required( pddc, surface, state )
    miDDContext         *pddc;
    miNurbSurfaceStruct *surface;
    Nurb_surf_state	*state;
{
    if ( surface->uOrder <= 1 && surface->vOrder <= 1 ) {
	state->reps.markers = 1;

    } else if ( (surface->uOrder > MAXORD) || (surface->vOrder > MAXORD) ) {
	/* Order is not supported; just draw the control net. */
	state->reps.isocrvs = 1;
	state->isocount[0] = 1;
	state->isocount[1] = 1;

    } else {

	/*
	 * First evaluate interior style
	 */
	switch ( pddc->Static.attrs->intStyle ) {
	    case PEXInteriorStyleSolid:
		state->reps.facets = 1;
		/* Only compute normals if lighting enabled */
		if (pddc->Static.attrs->reflModel != PEXReflectionNoShading)
		  state->reps.normals = 1;
		break;
	    case PEXInteriorStyleHollow:
		state->reps.hollow = 1;
		break;
	    case PEXInteriorStylePattern:
	    case PEXInteriorStyleHatch:
		state->reps.facets = 1;
		/* Only compute normals if lighting enabled */
		if (pddc->Static.attrs->reflModel != PEXReflectionNoShading)
		  state->reps.normals = 1;
		break;
	    case PEXInteriorStyleEmpty:
		/* No addtional action required. */
		break;
	}

	/*
	 * Next, parametric surface characteristics.
	 */
	switch ( pddc->Dynamic->pPCAttr->psc.type ) {
	    case PEXPSCNone:
	    case PEXPSCImpDep:
		break;
	    case PEXPSCIsoCurves:
		state->reps.isocrvs = 1;
		/* Negative curve counts mean no curves between knots. */
		state->isocount[0] = 
		   MAX(pddc->Dynamic->pPCAttr->psc.data.isoCurves.numUcurves,0);
		state->isocount[1] = 
		   MAX(pddc->Dynamic->pPCAttr->psc.data.isoCurves.numVcurves,0);
		break;
	    case PEXPSCMcLevelCurves:
	    case PEXPSCWcLevelCurves:
		/* Note that level curves are not implemented */
		break;
	}

	if ( pddc->Static.attrs->edges == PEXOn )
	    state->reps.edges = 1;
    }
}



/*++
 |
 |  Function Name:      compute_nurb_surface
 |
 |  Function Description:
 |
 |  Note(s):
 |
 --*/

static 
ddpex3rtn
compute_nurb_surface( pddc, surface, state )
    miDDContext         *pddc;
    miNurbSurfaceStruct *surface;
    Nurb_surf_state	*state;
{

/*  calls */
#ifdef TRIMING
    ddpex3rtn phg_nt_install_trim_loops();
#endif

/*  uses  */
    ddpex3rtn		status = Success;
    int			i;

    state->range.umin = surface->pUknots[surface->uOrder - 1];
    state->range.umax = surface->pUknots[surface->numUknots - surface->uOrder];
    state->range.vmin = surface->pVknots[surface->vOrder - 1];
    state->range.vmax = surface->pVknots[surface->numVknots - surface->vOrder];

    state->param_limits.umin = state->range.umin;
    state->param_limits.umax = state->range.umax;
    state->param_limits.vmin = state->range.vmin;
    state->param_limits.vmax = state->range.vmax;

    /* Check for unsupported order. */
    if (   surface->uOrder > MAXORD || surface->uOrder > MAXORD ) {
	/* Draw *only* the control polygon. */
	return build_control_polygon( surface, state );
    }

    if ( state->reps.markers ) {
	return build_surf_markers( surface, state );
    }

    /*
     * build initial grid description 
     */
    if (status = span_grids( state, surface ))
	    goto abort;

    if ( state->reps.normals ) {
	for ( i = 0; i < state->grids.number; i++ )
	    compute_edge_point_normals( surface, &state->grids.grids[i] );
	state->grids.flags.normals = 1;
    }

    if ( surface->numTrimCurveLists > 0 ) {
#ifdef TRIMING
	if ( status = phg_nt_install_trim_loops( surface, state ) ) 
	    goto abort;
#endif /* TRIMING */
    }

    if ( state->reps.edges || state->reps.hollow ) {
	if ( surface->numTrimCurveLists <= 0 )
	    make_edge_segments( state );
	if ( state->reps.edges )
	    build_edge_reps( pddc, state, surface, &state->edges, 1 );
	if ( state->reps.hollow )
	    build_edge_reps( pddc, state, surface, &state->hollow, 0 );
    }

    if ( state->reps.facets ) {
	status = build_facets( state, surface );
    }

    if ( state->reps.isocrvs ) {
	switch( pddc->Dynamic->pPCAttr->psc.data.isoCurves.placementType ) {
	    default:
	    case 0 /* PEXPSCUniform */:
		status = uniform_isocurves( state, surface );
		break;
	    case 1 /* PEXPSCNonUniform */:
		status = nonuniform_isocurves( state, surface );
		break;
	}
    }

abort:
    return (status);

}



/*++
 |
 |  Function Name:      span_grids
 |
 |  Function Description:
 |
 |	This routine is the first step in creating a tesselated
 |	polygon description of a surface patch. The rendering
 |	
 |
 |  Note(s):
 |
 --*/

static 
ddpex3rtn
span_grids( state, surface )
    Nurb_surf_state	*state;
    miNurbSurfaceStruct *surface;
{
/*  uses */
    double		*uvals = 0, 
			*vvals = 0; /* need double precision */
    int			num_uvals, num_vvals;
    int			ucount, vcount;
    int			uspan, vspan;
    int			*uspans = 0, 
			*vspans = 0;
    int			num_uspans, num_vspans;

    int			i, j;
    ddFLOAT		*uknots = surface->pUknots;
    ddFLOAT		*vknots = surface->pVknots;
    ddpex3rtn		status = Success;

    /* Small inaccuracies sometimes cause span_evaluation_points() to
     * generate an extra point or two, so allocate the arrays two sizes
     * larger than the expected need.
     */
    ucount = state->approx_value[0] + 4;
    vcount = state->approx_value[1] + 4;

    if ( !( uvals = (double *) xalloc(ucount * sizeof(double))) ) {
	status = BadAlloc;
	goto abort;
    }

    if ( !( vvals = (double *) xalloc(vcount * sizeof(double))) ) {
	status = BadAlloc;
	goto abort;
    }

    num_uspans = 1; uspans = &uspan;
    num_vspans = 1; vspans = &vspan;
    for ( i = surface->uOrder - 1; i < surface->mPts; i++ ) {
	if ( uknots[i] != uknots[i+1] ) {
	    uspan = i + 1;
	    span_evaluation_points( uknots, i,
				    state->range.umin, state->range.umax,
				    state->approx_value[0], 
				    &num_uvals, uvals );
	    if ( num_uvals <= 0 )
		continue;

	    for ( j = surface->vOrder - 1; j < surface->nPts; j++ ) {
		if ( vknots[j] != vknots[j+1] ) {
		    vspan = j + 1;
		    span_evaluation_points( vknots, j,
					    state->range.vmin,state->range.vmax,
					    state->approx_value[1], 
					    &num_vvals, vvals );
		    if ( num_vvals <= 0 )
			continue;

		    if ( status = add_grid( state, surface, 
					    num_uvals, num_vvals, 
					    uvals, vvals, 
					    num_uspans, num_vspans,
					    uspans, vspans ) )
			goto abort;
		}
	    }
	}
    }

abort:
    if (uvals) xfree(uvals);
    if (vvals) xfree(vvals);

    return (status);
}



/*++
 |
 |  Function Name:      span_evaluation_points
 |
 |  Function Description:
 |
 |  Note(s):
 |
 --*/

static void
span_evaluation_points( knots, span, tmin, tmax, apxval, num_vals, vals )
    ddFLOAT	*knots;
    int		span;
    double	tmin, tmax;
    double	apxval;
    int		*num_vals;
    double	*vals;
{
    double	t, dt;
    double	left, right;

    int		count = 0, maxvals;
    double	*ep = vals;

    if ( knots[span] < tmax && knots[span+1] > tmin ) {
	/* maxvals is used to control the number of positions generated so
	 * that very small nearly zero-width intervals aren't generated
	 * due to small floating point inaccuracies.
	 */
	maxvals = apxval + 2;
	left = knots[span];
	right = knots[span+1];
	dt = (right - left) / (double)(maxvals - 1);
	t = left;

	/* If tmin is in the interval then start with it. */
	if ( tmin > left && tmin < right ) {
	    ep[count++] = tmin;
	    while ( t <= tmin ) {
		t += dt; --maxvals;
	    }
	}
	ep[count++] = t;
	t += dt; --maxvals;

	/* Interior values. */
	for ( ; maxvals > 1 && t < tmax; t += dt, --maxvals )
	    ep[count++] = t;

	/* Last value. */
	if ( right > tmax )
	    ep[count++] = tmax;
	else
	    ep[count++] = right;
    }
    *num_vals = count;
}



#define GRID_LIST_CHUNK		5

/*++
 |
 |  Function Name:      add_grid
 |
 |  Function Description:
 |
 |  Note(s):
 |
 --*/

static int
add_grid( state, surface, ucount, vcount, uvals, vvals, 
	  num_uspans, num_vspans, uspans, vspans )
    Nurb_surf_state	*state;
    miNurbSurfaceStruct	*surface;
    int			ucount, vcount;
    double		*uvals, *vvals;
    int			num_uspans, num_vspans; 
    int			*uspans, *vspans;
{
/*  calls  */
    void phg_ns_evaluate_surface_in_span();

/*  uses  */
    int			uspan, vspan;
    int			i, j;
    Nurb_edge_point	*ep;
    Nurb_grid		*grid;

    if ( ucount <= 0 || vcount <= 0 )
	return 1;

    if ( state->grids.number % GRID_LIST_CHUNK == 0 ) {
	if ( state->grids.number == 0 )
	    state->grids.grids = (Nurb_grid *)xalloc(
		GRID_LIST_CHUNK * sizeof(Nurb_grid) );
	else
	    state->grids.grids = (Nurb_grid *)xrealloc( state->grids.grids,
		(state->grids.number + GRID_LIST_CHUNK) * sizeof(Nurb_grid) );
    }
    if ( !state->grids.grids ) {
	return (BadAlloc);
    }

    ++state->grids.number;

    grid = &state->grids.grids[state->grids.number-1];
    if ( !( grid->pts = (Nurb_edge_point *)
	xalloc(ucount * vcount * sizeof(Nurb_edge_point))) ) {
	return (BadAlloc);
    }

    /* Calculate vertex coordinates. */
    ep = grid->pts;
    for ( j = 0; j < vcount; j++ ) {
	vspan = (num_vspans > 1) ? vspans[j] : vspans[0];
	for ( i = 0; i < ucount; i++, ep++ ) {
	    ep->count = 0;
	    ep->u = uvals[i]; ep->v = vvals[j];
	    uspan = (num_uspans > 1) ? uspans[i]:uspans[0];
	    phg_ns_evaluate_surface_in_span( surface, ep->u, ep->v, 
					     uspan, vspan, &ep->pt );
	}
    }
    grid->nu = ucount; grid->nv = vcount;
    grid->extent.umin = uvals[0]; grid->extent.umax = uvals[ucount-1];
    grid->extent.vmin = vvals[0]; grid->extent.vmax = vvals[vcount-1];

    return (Success);
}



/*++
 |
 |  Function Name:      phg_ns_evaluate_surface_in_span
 |
 |  Function Description:
 |
 |  Note(s):
 |
 --*/

void
phg_ns_evaluate_surface_in_span( surface, u, v, uspan, vspan, spt )
    miNurbSurfaceStruct		*surface;
    register double		u, v;
    int				uspan, vspan;
    ddCoord4D			*spt;
{
    ddCoord4D		npt[MAXORD], tmppts[MAXORD];
    int			iu, iv;
    int			i;
    double		alpha, alpha1;
    int			nu, nv, j, k;
    int			uorder = surface->uOrder;
    int			vorder = surface->vOrder;
    ddFLOAT		*uknots = surface->pUknots;
    ddFLOAT		*vknots = surface->pVknots;
    ddCoord4D		*tmp;
    char		rat;

    rat = DD_IsVert4D(surface->points.type);

    iu = uspan - uorder; iv = vspan - vorder;
    for ( nv = 0; nv < vorder; nv++ ) {

	if ( rat ) {
          for ( nu = 0; nu < uorder; nu++ )
            tmppts[nu] = surface->points.ddList->pts.p4Dpt[(iv + nv) * surface->mPts + (iu + nu)];
        } else {   
          for ( nu = 0; nu < uorder; nu++ ) {
            tmppts[nu].x = surface->points.ddList->pts.p3Dpt[(iv + nv) * surface->mPts + (iu + nu)].x;
            tmppts[nu].y = surface->points.ddList->pts.p3Dpt[(iv + nv) * surface->mPts + (iu + nu)].y;
            tmppts[nu].z = surface->points.ddList->pts.p3Dpt[(iv + nv) * surface->mPts + (iu + nu)].z;
          }
        }
 
        for ( k = 1; k < uorder; k++ ) {
            for ( j = uorder-1, tmp = &tmppts[j]; j >= k; j--, tmp--) {
                i= j + iu;
                alpha = (u - uknots[i]) / (uknots[i+uorder-k] - uknots[i]);
                alpha1 = 1.0 - alpha;
                tmp->x = alpha * tmp->x + alpha1 * (tmp-1)->x;
                tmp->y = alpha * tmp->y + alpha1 * (tmp-1)->y;
                tmp->z = alpha * tmp->z + alpha1 * (tmp-1)->z;
                if ( rat )
                    tmp->w = alpha * tmp->w + alpha1 * (tmp-1)->w;
            }  
        }    
 
        npt[nv] = tmppts[uorder - 1];
    }
 
    for ( nv = 0; nv < vorder; nv++ )  {
        tmppts[nv] = npt[nv];
    }
 
    for ( k = 1; k < vorder; k++ ) {
        for ( j = vorder - 1, tmp = &tmppts[j]; j >= k; j--, tmp--) {
            i= j + iv;
            alpha = (v - vknots[i]) / (vknots[i+vorder-k] - vknots[i]);
            alpha1 = 1.0 - alpha;
            tmp->x = alpha * tmp->x + alpha1 * (tmp-1)->x;
            tmp->y = alpha * tmp->y + alpha1 * (tmp-1)->y;
            tmp->z = alpha * tmp->z + alpha1 * (tmp-1)->z;
            if ( rat )
                tmp->w = alpha * tmp->w + alpha1 * (tmp-1)->w;
        }
    }
 
    *spt = tmppts[vorder-1];
    if ( !rat )
        spt->w = 1.0;
}



/*++
 |
 |  Function Name:      phg_ns_evaluate_surface
 |
 |  Function Description:
 |
 |  Note(s):
 |
 --*/

void
phg_ns_evaluate_surface( surface, u, v, spt )
    miNurbSurfaceStruct		*surface;
    register double		u, v;
    ddCoord4D			*spt;
{
    int			iu, iv;

    register ddFLOAT	*uknots = surface->pUknots;
    register ddFLOAT	*vknots = surface->pVknots;

    iu = surface->numUknots - 1;
    iv = surface->numVknots - 1;

    /* Ensure parameters are within range. */
    if ( u < uknots[0] )
	u = uknots[0];
    else if ( u > uknots[iu] )
	u = uknots[iu];

    if ( v < vknots[0] )
	v = vknots[0];
    else if ( v > vknots[iv] )
	v = vknots[iv];

    /* Find the span where u,v belong. */
    if ( uknots[iu] == u )
	while ( uknots[iu] >= u ) --iu;  
    else
	while ( uknots[iu] > u ) --iu;

    if ( vknots[iv] == v )
	while ( vknots[iv] >= v ) --iv;  
    else
	while ( vknots[iv] > v ) --iv;
    phg_ns_evaluate_surface_in_span( surface, u, v, ++iu, ++iv, spt );
}



/*++
 |
 |  Function Name:      avg_vertex_normal
 |
 |  Function Description:
 |
 |  Note(s):
 |
 --*/

static void
avg_vertex_normal( count, pt, ptp, ptq, nout )
    int		count;
    ddCoord3D	*pt, *ptp, *ptq;
    ddVector3D	*nout;
{
    ddVector3D	pvec, qvec;
    ddVector3D	nscratch;
    double	h;

    ddVector3D	*p = &pvec, 
		*q = &qvec;
    ddVector3D	*n;

    /* Calculate the vertex normal for the specified vertex by computing
     * the cross product of the vectors connecting "pt" to "ptp" and "ptq,"
     * N = (ptp - pt) X (ptq - pt).  If the count is > 0 average this normal
     * into the vertex normal rather than just writing it there.  This
     * facilitates computing the average normal derived from all polygons
     * adjacent to the vertex.
     */
    p->x = ptp->x - pt->x; p->y = ptp->y - pt->y; p->z = ptp->z - pt->z;
    q->x = ptq->x - pt->x; q->y = ptq->y - pt->y; q->z = ptq->z - pt->z;
    n = count > 0 ? &nscratch : nout;
    n->x = p->y * q->z - q->y * p->z;
    n->y = q->x * p->z - p->x * q->z;
    n->z = p->x * q->y - q->x * p->y;
    /* Normalize. */
    h = 1.0 / sqrt(n->x * n->x + n->y * n->y + n->z * n->z);
    n->x *= h; n->y *= h; n->z *= h;
    if ( count > 0 ) {
	/* Average in this normal. */
	h = 1.0 / (double)(count + 1);
	nout->x = h * ((double)count * nout->x + n->x);
	nout->y = h * ((double)count * nout->y + n->y);
	nout->z = h * ((double)count * nout->z + n->z);
	/* Normalize the new average. */
	n = nout;
	h = 1.0 / sqrt(n->x * n->x + n->y * n->y + n->z * n->z);
	n->x *= h; n->y *= h; n->z *= h;
    }
}



/*++
 |
 |  Function Name:      build_facets
 |
 |  Function Description:
 |
 |	The purpose of this function is to take the surface internal
 |	descriptions of the grids and trim curves and convert them
 |	into descriptions of either quad meshes or SOFAS for 
 |	rendering.
 |
 |	Note that quad meshes are output if there are no trim curves,
 |	otherwise SOFAS are used.
 |
 |  Note(s):
 |
 --*/

static ddpex3rtn
build_facets( state, surface )
    Nurb_surf_state		*state;
    miNurbSurfaceStruct		*surface;
{

#ifdef TRIMING
    ddpex3rtn	phg_nt_trim_rect();
#endif /* TRIMING */

    Nurb_edge_point		*rect[4];
    int				g;
    int				i, j;
    int				ucount, vcount;
    Nurb_grid			*grid;
    miListHeader		*listheader;
    char			rat;
    ddpex3rtn			status;

    /* Set the Path_d flags. */
    if ( state->grids.number <= 0 ) return (Success);

    rat = DD_IsVert4D(surface->points.type);

    if ( surface->numTrimCurveLists > 0 ) {
#ifdef TRIMING
      /*
       * If trim curves are present, build a SOFAS description
       * of the trimmed surface.
       */

      /* Initialize output SOFAS structure */
      state->sofas = (miSOFASStruct *)xalloc(sizeof(miSOFASStruct));
      state->sofas->edgeAttribs = 0;
      state->sofas->numFAS = state->sofas->numEdges = 0;
      state->sofas->pFacets.type = DD_FACET_NONE;
      state->sofas->pFacets.numFacets = state->sofas->pFacets.maxData = 0;
      state->sofas->pFacets.facets.pNoFacet = 0;
      state->sofas->points.numLists = state->sofas->points.maxLists = 0;
      state->sofas->points.ddList = 0;
      state->sofas->connects.numListLists = state->sofas->connects.maxData = 0;
      state->sofas->connects.data = 0;

      /* Note that each rectangle of each grid is trimmed separately */
      for ( g = 0; g < state->grids.number; g++ ) {
	grid = &state->grids.grids[g];
	ucount = grid->nu; vcount = grid->nv;
	if ( surface->numTrimCurveLists > 0 ) {
	    register Nurb_edge_point	*ll, *lr, *ur, *ul;

	    /* Determine the facets and pass them to the trimming code. */
	    ll = &grid->pts[0]; lr = ll + 1;
	    ul = &grid->pts[ucount]; ur = ul + 1;
	    for ( j = 0; j < vcount-1; j++, ll++, lr++, ur++, ul++ ) {
		for ( i = 0; i < ucount-1; i++, ll++, lr++, ur++, ul++ ) {
		    rect[LL] = ll;
		    rect[LR] = lr;
		    rect[UR] = ur;
		    rect[UL] = ul;
		    if ( status = phg_nt_trim_rect( state, surface, rect,
						    add_pgon_point, 
						    state->sofas ) )
			return status;
		}
	    }
        }
      }
#endif /* TRIMING */
    } else {

      /* 
       * no triming 
       * Create a list of quad meshes from the grid data.
       */

      state->facets = 
	       (miListHeader *)xalloc(state->grids.number*sizeof(miListHeader));

      listheader = state->facets;
      grid = state->grids.grids;

      /*
       * Create a quad mesh for each grid
       */
      for ( g = 0; g < state->grids.number; g++ ) {

	int		j;
	ddPointUnion	new_point;
	int		num_pts = grid->nu * grid->nv;
	Nurb_edge_point	*ep = grid->pts;

	listheader->flags = 0;
	listheader->numLists = 0;
	listheader->maxLists = 0;
	listheader->ddList = 0;

	MI_ALLOCLISTHEADER( listheader, 1);

	/*
	 * Note that the data in the grid is already organized
	 * in quad mesh order. Therefore, all that needs to be
	 * done is to copy the data from the list of edge_points to
	 * a listofddPoints.
	 */
	if ( rat ) {

	  if (state->reps.normals) {
	    listheader->type = DD_NORM_POINT4D;
	    MI_ALLOCLISTOFDDPOINT( listheader->ddList, 
				   num_pts, sizeof(ddNormalPoint4D));
	  } else {
	    listheader->type = DD_HOMOGENOUS_POINT;
	    MI_ALLOCLISTOFDDPOINT( listheader->ddList, 
				   num_pts, sizeof(ddCoord4D));
	  }

	  new_point = listheader->ddList->pts;
	  if (!(new_point.ptr)) return(BadAlloc);

	  for ( j = 0; j < num_pts; j++, ep++ ) {
		*(new_point.p4Dpt++) = ep->pt;
		if (state->reps.normals) *(new_point.pNormal++) = ep->normal;
	  }

	  listheader->ddList->numPoints = num_pts;

	} else {

	  if (state->reps.normals) {
	    listheader->type = DD_NORM_POINT;
	    MI_ALLOCLISTOFDDPOINT( listheader->ddList, 
				   num_pts, sizeof(ddNormalPoint));
	  } else {
	    listheader->type = DD_3D_POINT;
	    MI_ALLOCLISTOFDDPOINT( listheader->ddList, 
				   num_pts, sizeof(ddCoord3D));
	  }

	  new_point = listheader->ddList->pts;
	  if (!(new_point.ptr)) return(BadAlloc);

	  for ( j = 0; j < num_pts; j++, ep++ ) {
		*(new_point.p3Dpt++) = *((ddCoord3D *)(&ep->pt));
		if (state->reps.normals) *(new_point.pNormal++) = ep->normal;
	  }

	  listheader->ddList->numPoints = num_pts;

	} 

	/* skip to next grid and point list */
	listheader++;
	grid++;

      }
    }

    return ( Success );
}



/*++
 |
 |  Function Name:      build_edge_reps
 |
 |  Function Description:
 |
 |  Note(s):
 |
 --*/

static void
build_edge_reps( pddc, state, surface, edge_list, use_edge_flags )
    miDDContext         *pddc;
    Nurb_surf_state	*state;
    miNurbSurfaceStruct *surface;
    miListHeader	**edge_list;
    int			use_edge_flags;
{
    Nurb_grid		*grid = state->grids.grids;
    miListHeader	*out_list;
    register int	i, j;

    /* Initialize output structure */
    out_list = (miListHeader *)xalloc(sizeof(miListHeader));
    *edge_list = out_list;
    out_list->type = surface->points.type;
    out_list->numLists = out_list->maxLists = 0;
    out_list->ddList = 0;

    if ( surface->numTrimCurveLists > 0 ) {
#ifdef TRIMING
	Nurb_trim_data		*tdata = &state->trim_data;
	Nurb_trim_segment	*seg;

	for ( i = 0; i < state->grids.number; i++, grid++ ) {
	    for ( j = 0; j < tdata->nloops; j++ ) {
		if ( !EXTENTS_OVERLAP( grid->extent, tdata->loops[j].extent) )
		    continue;
		if ( !(seg = tdata->loops[j].segs) )
		    continue;
		phg_nt_draw_segs( state, surface, grid, seg, tdata->vertices,
				  use_edge_flags, out_list );
	    }
	}
#endif /* TRIMING */
    } else {
	for ( i = 0; i < state->grids.number; i++, grid++ ) {
	    phg_nt_draw_segs( state, surface, grid, state->edge_segs,
			      state->corners, use_edge_flags, out_list );
	}
    }
}


/*++
 |
 |  Function Name:      isocurve
 |
 |  Function Description:
 |
 |  Note(s):
 |
 --*/

static int
isocurve( state, surface, dir, val, tmin, tmax, curve_list )
    Nurb_surf_state		*state;
    miNurbSurfaceStruct		*surface;
    int				dir; /* 1 ==> constant U, 2 ==> constant V */
    double			val;
    double			tmin, tmax;
    miListHeader		*curve_list;
{
    Nurb_trim_segment		seg;
    Nurb_param_point		spts[2];
    int				i, j;
    Nurb_grid			*grid;
    ddpex3rtn			status;

    /* Dummy up one or more trimming segments for this iso curve and pass
     * them to the trim segment drawer.
     */
    seg.first = seg.start = 0; seg.last = seg.end = 1;
    seg.next = (Nurb_trim_segment *)NULL;
    if ( dir == 2 ) {	/* constant V */
	spts[0].v = spts[1].v = val;
	seg.extent.vmin = seg.extent.vmax = val;
    } else {	/* constant U */
	spts[0].u = spts[1].u = val;
	seg.extent.umin = seg.extent.umax = val;
    }

    if ( surface->numTrimCurveLists > 0 ) {
#ifdef TRIMING
	ddpex3rtn		phg_nt_compute_trim_range();
	Nurb_limitlst		ranges;
	Nurb_limit		no_limit;
	int			tc;

	/* Get the trimming intervals. */
	NURB_INIT_RANGE_LIST(&ranges);
	if (status = phg_nt_compute_trim_range( state, dir, val, 
						tmin, tmax, &ranges, &tc ))
	  return( status );

	if ( tc == -1) {
	    tc = 1; 
	    ranges.limits = &no_limit;
	    no_limit.lmin = tmin;
	    no_limit.lmax = tmax;
	}

	for ( i = 0; i < tc; i++ ) {
	    if ( dir == 2 ) {	/* constant V */
		spts[0].u = seg.extent.umin = ranges.limits[i].lmin;
		spts[1].u = seg.extent.umax = ranges.limits[i].lmax;
	    } else {	/* constant U */
		spts[0].v = seg.extent.vmin = ranges.limits[i].lmin;
		spts[1].v = seg.extent.vmax = ranges.limits[i].lmax;
	    }
	    grid = state->grids.grids;
	    for ( j = 0; j < state->grids.number; j++, grid++ ) {
	     if (   dir == 1 && xin(grid->extent.umin,grid->extent.umax,val)
		    || dir == 2 && xin(grid->extent.vmin,grid->extent.vmax,val))
	      phg_nt_draw_segs(state, surface, grid, &seg, spts, 0, curve_list);
	    }
	}

	if ( ranges.size > 0 )
	    xfree( ranges.limits );

#endif /* TRIMING */
    } else { /* no trimming */
	if ( dir == 2 ) { /* constant V */
	    spts[0].u = seg.extent.umin = tmin;
	    spts[1].u = seg.extent.umax = tmax;
	} else { /* constant U */
	    spts[0].v = seg.extent.vmin = tmin;
	    spts[1].v = seg.extent.vmax = tmax;
	}

	grid = state->grids.grids;
	for ( i = 0; i < state->grids.number; i++, grid++ ) {
	  if (   dir == 1 && xin(grid->extent.umin,grid->extent.umax,val)
		|| dir == 2 && xin(grid->extent.vmin,grid->extent.vmax,val))
	    phg_nt_draw_segs( state, surface, grid, &seg, spts, 0, curve_list );
	}
    }

    return Success;
}



/*++
 |
 |  Function Name:      uniform_isocurves
 |
 |  Function Description:
 |
 |  Note(s):
 |
 --*/

static int
uniform_isocurves( state, surface )
    Nurb_surf_state	*state;
    miNurbSurfaceStruct	*surface;
{
    double		t, dt;
    int			j;
    Nurb_param_limit	*range = &state->param_limits;

    /* Initialize output structure */
    state->isocrvs = (miListHeader *)xalloc(sizeof(miListHeader));
    state->isocrvs->type = surface->points.type;
    state->isocrvs->numLists = state->isocrvs->maxLists = 0;
    state->isocrvs->ddList = 0;

    /* Place curves uniformly between the specified parameter limits.
     * Also place curves at the parameter limits, only if there is no
     * trimming.
     */
 
    /* First curve of constant U. */
    t = range->umin;
#ifdef CADAM
    if ( surface->numTrimCurveLists <= 0 ) {      /* no trimming */
#endif /* CADAM */
        (void)isocurve( state, surface, 1, t, range->vmin, range->vmax,
			state->isocrvs );
#ifdef CADAM
    }
#endif /* CADAM */
 
    /* Interior curves of constant U. */
    dt = (range->umax - range->umin) / (state->isocount[0] + 1);
    for ( j = 0, t += dt; j < state->isocount[0]; j++, t += dt ) {
        (void)isocurve( state, surface, 1, t, range->vmin, range->vmax,
			state->isocrvs );
    }
 
    /* Last curve of constant U. */
    t = range->umax;
#ifdef CADAM
    if ( surface->numTrimCurveLists <= 0 ) {      /* no trimming */
#endif /* CADAM */
        (void)isocurve( state, surface, 1, t, range->vmin, range->vmax,
			state->isocrvs );
#ifdef CADAM
    }
#endif /* CADAM */
 
 
    /* First curve of constant V. */
    t = range->vmin;
#ifdef CADAM
    if ( surface->numTrimCurveLists <= 0 ) {    /* no trimming */
#endif /* CADAM */
        (void)isocurve( state, surface, 2, t, range->umin, range->umax,
			state->isocrvs );
#ifdef CADAM
    }
#endif /* CADAM */

    /* Interior curves of constant V. */
    dt = (range->vmax - range->vmin) / (state->isocount[1] + 1);
    for ( j = 0, t += dt; j < state->isocount[1]; j++, t += dt ) {
        (void)isocurve( state, surface, 2, t, range->umin, range->umax,
			state->isocrvs );
    }

    /* Last curve of constant V. */
    t = range->vmax;
#ifdef CADAM
    if ( surface->numTrimCurveLists <= 0 ) {     /* no trimming */
#endif /* CADAM */
        (void)isocurve( state, surface, 2, t, range->umin, range->umax,
			state->isocrvs );
#ifdef CADAM
    }
#endif /* CADAM */

    return Success;
}


/*++
 |
 |  Function Name:      nonuniform_isocurves
 |
 |  Function Description:
 |
 |  Note(s):
 |
 --*/

static int
nonuniform_isocurves( state, surface )
    Nurb_surf_state     *state;
    miNurbSurfaceStruct	*surface;
{
    ddFLOAT		*uknots = surface->pUknots;
    ddFLOAT 		*vknots = surface->pVknots;
    double		t, dt;

    int			i, j;
    Nurb_param_limit	*range = &state->range;

    /* Initialize output structure */
    state->isocrvs = (miListHeader *)xalloc(sizeof(miListHeader));
    state->isocrvs->type = surface->points.type;
    state->isocrvs->numLists = state->isocrvs->maxLists = 0;
    state->isocrvs->ddList = 0;

    /* 
     * Place curves uniformly between each non-vacuous span. 
     * For each non-vacuous U span draw the iso U curves for all
     * surface V. 
     */
    for ( i = surface->uOrder - 1; i < surface->mPts; i++ ) {
        if ( uknots[i] != uknots[i+1] ) {
            /* First curve of span. */
            t = uknots[i];
#ifdef CADAM
            if ( surface->numTrimCurveLists <= 0 ) {    /* no trimming
*/
#endif /* CADAM */
                if ( t >= range->umin && t <= range->umax )
                    (void)isocurve( state, surface, 1, t,
				    range->vmin, range->vmax, 
				    state->isocrvs );
#ifdef CADAM
            } else {
                if ( t > range->umin && t < range->umax )
                    (void)isocurve( state, surface, 1, t,
				    range->vmin, range->vmax, 
				    state->isocrvs );
            }
#endif /* CADAM */
 
            /* Interior curves of span. */
            dt = (uknots[i+1] - uknots[i]) / (state->isocount[0] +
1);
            for ( j = 0, t += dt; j < state->isocount[0]; j++, t +=
dt ) {
                if ( t >= range->umin && t <= range->umax )
                    (void)isocurve( state, surface, 1, t,
				    range->vmin, range->vmax, 
				    state->isocrvs );
            }
 
            /* Last curve of span. */
            t = uknots[i+1];
#ifdef CADAM
            if ( surface->numTrimCurveLists <= 0 ) {    /* no trimming
*/
#endif /* CADAM */
                if ( t >= range->umin && t <= range->umax )
                    (void)isocurve( state, surface, 1, t,
				    range->vmin, range->vmax, 
				    state->isocrvs );
#ifdef CADAM
            } else {
                if ( t > range->umin && t < range->umax )
                    (void)isocurve( state, surface, 1, t,
				    range->vmin, range->vmax, 
				    state->isocrvs );
            }
#endif /* CADAM */
        }   
    }    

    /* For each non-vacuous V span draw the iso V curves for all
surface U. */
    for ( i = surface->vOrder - 1; i < surface->nPts; i++ ) {
        if ( vknots[i] != vknots[i+1] ) {
            /* First curve of span. */
            t = vknots[i];
#ifdef CADAM
            if ( surface->numTrimCurveLists <= 0 ) {    /* no trimming
*/
#endif /* CADAM */
                if ( t >= range->vmin && t <= range->vmax )
                    (void)isocurve( state, surface, 2, t,
				    range->umin, range->umax, 
				    state->isocrvs );
#ifdef CADAM
            } else {
                if ( t > range->vmin && t < range->vmax )
                    (void)isocurve( state, surface, 2, t,
				    range->umin, range->umax, 
				    state->isocrvs );
            }
#endif /* CADAM */

            /* Inteior curves of span. */
            dt = (vknots[i+1] - vknots[i]) / (state->isocount[1] +
1);
            for ( j = 0, t += dt; j < state->isocount[1]; j++, t +=
dt ) {
                if ( t >= range->vmin && t <= range->vmax )
                    (void)isocurve( state, surface, 2, t,
				    range->umin, range->umax, 
				    state->isocrvs );
            }

            /* Last curve of span. */
            t = vknots[i+1];
#ifdef CADAM
            if ( surface->numTrimCurveLists <= 0 ) {     /* no trimming
*/
#endif /* CADAM */
                if ( t >= range->vmin && t <= range->vmax )
                    (void)isocurve( state, surface, 2, t,
				    range->umin, range->umax, 
				    state->isocrvs );
#ifdef CADAM
            } else {
                if ( t > range->vmin && t < range->vmax )
                    (void)isocurve( state, surface, 2, t,
				    range->umin, range->umax, 
				    state->isocrvs );
            }
#endif /* CADAM */
        }   
    }    

    return Success;
}



/*++
 |
 |  Function Name:      compute_average_edge_point_normals
 |
 |  Function Description:
 |
 |  Note(s):
 |
 --*/

static void
compute_average_edge_point_normals( surface, grid )
    miNurbSurfaceStruct	*surface;
    Nurb_grid		*grid;
{
    int			i, j;
    int			ucount = grid->nu, 
			vcount = grid->nv;
    Nurb_edge_point	*ll, *lr, *ur, *ul;
    char		rat;

    rat = DD_IsVert4D(surface->points.type);

    /* Step along the surface and calculate averaged normals. */
    ll = &grid->pts[0]; lr = ll + 1;
    ul = &grid->pts[ucount]; ur = ul + 1;
    for ( j = 0; j < vcount-1; j++, ll++, lr++, ul++, ur++ ) {
	for ( i = 0; i < ucount-1; i++, ll++, lr++, ul++, ur++ ) {
	    /* Calculate and average vertex normals. */
	    if ( rat ) {
		ddCoord3D			pll, plr, pur, pul;
		register ddCoord4D	*pt;
		register double		h;

		pt = &ll->pt; h = 1.0 / pt->w;
		pll.x = h * pt->x; pll.y = h * pt->y; pll.z = h * pt->z;
		pt = &lr->pt; h = 1.0 / pt->w;
		plr.x = h * pt->x; plr.y = h * pt->y; plr.z = h * pt->z;
		pt = &ur->pt; h = 1.0 / pt->w;
		pur.x = h * pt->x; pur.y = h * pt->y; pur.z = h * pt->z;
		pt = &ul->pt; h = 1.0 / pt->w;
		pul.x = h * pt->x; pul.y = h * pt->y; pul.z = h * pt->z;
		avg_vertex_normal( ll->count, &pll, &plr, &pul, &ll->normal );
		avg_vertex_normal( lr->count, &plr, &pur, &pll, &lr->normal );
		avg_vertex_normal( ur->count, &pur, &pul, &plr, &ur->normal );
		avg_vertex_normal( ul->count, &pul, &pll, &pur, &ul->normal );
	    } else {
		avg_vertex_normal( ll->count, (ddCoord3D*)&ll->pt,
		    (ddCoord3D*)&lr->pt, (ddCoord3D*)&ul->pt, &ll->normal );
		avg_vertex_normal( lr->count, (ddCoord3D*)&lr->pt,
		    (ddCoord3D*)&ur->pt, (ddCoord3D*)&ll->pt, &lr->normal );
		avg_vertex_normal( ur->count, (ddCoord3D*)&ur->pt,
		    (ddCoord3D*)&ul->pt, (ddCoord3D*)&lr->pt, &ur->normal );
		avg_vertex_normal( ul->count, (ddCoord3D*)&ul->pt,
		    (ddCoord3D*)&ll->pt, (ddCoord3D*)&ur->pt, &ul->normal );
	    }
	    ++ll->count; ++lr->count; ++ur->count; ++ul->count;
	}
    }
}


/*++
 |
 |  Function Name:      compute_edge_point_normals
 |
 |  Function Description:
 |
 |  Note(s):
 |
 --*/

static void
compute_edge_point_normals( surface, grid )
    miNurbSurfaceStruct		*surface;
    Nurb_grid			*grid;
{
    register int		i, j;
    register int		ucount = grid->nu, vcount = grid->nv;
    register Nurb_edge_point	*ll, *lr, *ur, *ul;
    char		rat;

    rat = DD_IsVert4D(surface->points.type);

    /* Step along the surface and calculate normals. */
    ll = &grid->pts[0]; lr = ll + 1;
    ul = &grid->pts[ucount]; ur = ul + 1;
    for ( j = 0; j < vcount-1; j++, ll++, lr++, ul++, ur++ ) {
	for ( i = 0; i < ucount-1; i++, ll++, lr++, ul++, ur++ ) {
	    /* Calculate vertex normals. */
	    if ( rat ) {
		ddCoord3D			pll, plr, pur, pul;
		register ddCoord4D	*pt;
		register double		h;

		pt = &ll->pt; h = 1.0 / pt->w;
		pll.x = h * pt->x; pll.y = h * pt->y; pll.z = h * pt->z;
		pt = &lr->pt; h = 1.0 / pt->w;
		plr.x = h * pt->x; plr.y = h * pt->y; plr.z = h * pt->z;
		pt = &ul->pt; h = 1.0 / pt->w;
		pul.x = h * pt->x; pul.y = h * pt->y; pul.z = h * pt->z;
		avg_vertex_normal( 0, &pll, &plr, &pul, &ll->normal );

		/* Calculate normals at edge of grid. */
		if ( i == ucount-2 || j == vcount-2 ) {
		    pt = &ur->pt; h = 1.0 / pt->w;
		    pur.x = h * pt->x; pur.y = h * pt->y; pur.z = h * pt->z;
		}
		if ( i == ucount-2 )
		    avg_vertex_normal( 0, &plr, &pur, &pll, &lr->normal );
		if ( j == vcount-2 )
		    avg_vertex_normal( 0, &pul, &pll, &pur, &ul->normal );
		if ( i == ucount-2 && j == vcount-2 )
		    avg_vertex_normal( 0, &pur, &pul, &plr, &ur->normal );
	    } else {
		avg_vertex_normal( 0, (ddCoord3D*)&ll->pt,
		    (ddCoord3D*)&lr->pt, (ddCoord3D*)&ul->pt, &ll->normal );

		/* Calculate normals at edge of grid. */
		if ( i == ucount-2 )
		    avg_vertex_normal( 0, (ddCoord3D*)&lr->pt,
			(ddCoord3D*)&ur->pt, (ddCoord3D*)&ll->pt, &lr->normal );
		if ( j == vcount-2 )
		    avg_vertex_normal( 0, (ddCoord3D*)&ul->pt,
			(ddCoord3D*)&ll->pt, (ddCoord3D*)&ur->pt, &ul->normal );
		if ( i == ucount-2 && j == vcount-2 )
		    avg_vertex_normal( 0, (ddCoord3D*)&ur->pt,
			(ddCoord3D*)&ul->pt, (ddCoord3D*)&lr->pt, &ur->normal );
	    }
	}
    }
}



/*++
 |
 |  Function Name:      make_edge_segments
 |
 |  Function Description:
 |
 |  Note(s):
 |
 --*/

static void
make_edge_segments( state )
    Nurb_surf_state	*state;
{
    int			i;
    Nurb_param_point	*corners = state->corners;
    Nurb_trim_segment	*segs = state->edge_segs;
    Nurb_trim_segment	*seg;

    /* Generate the edges by building a fake trimming segment for each edge
     * and calling the trim edge generator.
     */
    corners[0].u = state->range.umin; corners[0].v = state->range.vmin;
    corners[1].u = state->range.umax; corners[1].v = state->range.vmin;
    corners[2].u = state->range.umax; corners[2].v = state->range.vmax;
    corners[3].u = state->range.umin; corners[3].v = state->range.vmax;
    corners[4].u = state->range.umin; corners[4].v = state->range.vmin;
    segs[0].first = segs[0].start = 0; segs[0].last = segs[0].end = 1;
    segs[1].first = segs[1].start = 1; segs[1].last = segs[1].end = 2;
    segs[2].first = segs[2].start = 2; segs[2].last = segs[2].end = 3;
    segs[3].first = segs[3].start = 3; segs[3].last = segs[3].end = 4;
    segs[0].next = &segs[1];
    segs[1].next = &segs[2];
    segs[2].next = &segs[3];
    segs[3].next = (Nurb_trim_segment *)NULL;

    for ( i = 0, seg = segs; i < 4; i++, seg++ ) {
	seg->vis = ~0;
	if ( corners[seg->first].u <= corners[seg->last].u ) {
	    seg->extent.umin = corners[seg->first].u;
	    seg->extent.umax = corners[seg->last].u;
	} else {
	    seg->extent.umin = corners[seg->last].u;
	    seg->extent.umax = corners[seg->first].u;
	}
	if ( corners[seg->first].v <= corners[seg->last].v ) {
	    seg->extent.vmin = corners[seg->first].v;
	    seg->extent.vmax = corners[seg->last].v;
	} else {
	    seg->extent.vmin = corners[seg->last].v;
	    seg->extent.vmax = corners[seg->first].v;
	}
    }
}



/*++
 |
 |  Function Name:      build_surf_markers
 |
 |  Function Description:
 |
 |  Note(s):
 |
 --*/

static ddpex3rtn 
build_surf_markers( surface, state )
    miNurbSurfaceStruct	*surface;
    Nurb_surf_state	*state;
{

    if (!state->markers) 
      state->markers = (miListHeader *)xalloc(sizeof(miListHeader));

    *state->markers = surface->points;

    return Success;
}



/*++
 |
 |  Function Name:      build_control_polygon
 |
 |  Function Description:
 |
 |  Note(s):
 |
 --*/

static ddpex3rtn 
build_control_polygon( surface, state )
    miNurbSurfaceStruct	*surface;
    Nurb_surf_state	*state;
{
    int			i, j;
    listofddPoint	*pddolist;
    ddCoord3D		*cpts3, *out_pt3;
    ddCoord4D		*cpts4, *out_pt4;
    char		rat;

    rat = DD_IsVert4D(surface->points.type);

    if (!state->isocrvs) 
      if (!(state->isocrvs = (miListHeader *)xalloc(sizeof(miListHeader))))
	return(BadAlloc);

    MI_ALLOCLISTHEADER(state->isocrvs, surface->mPts * surface->nPts);
    if (!(pddolist = state->isocrvs->ddList)) return(BadAlloc);

    state->isocrvs->type = surface->points.type;
    state->isocrvs->flags = surface->points.flags;
    state->isocrvs->numLists = surface->mPts * surface->nPts;

    /* Connect the points in the U dimension. */
    if ( rat ) {

	for ( j = 0; j < surface->mPts; j++ ) {
	  cpts4 = &surface->points.ddList->pts.p4Dpt[j * surface->mPts];
	  MI_ALLOCLISTOFDDPOINT(pddolist, surface->mPts, sizeof(ddCoord4D));
	  if (!(out_pt4 = pddolist->pts.p4Dpt)) return(BadAlloc);
	  for ( i = 1; i < surface->mPts; i++ ) {
	    *(out_pt4++) = *(cpts4++);
	  }
	  (pddolist++)->numPoints = surface->mPts;
	}

    } else {

	for ( j = 0; j < surface->mPts; j++ ) {
	  cpts3 = &surface->points.ddList->pts.p3Dpt[j * surface->mPts];
	  MI_ALLOCLISTOFDDPOINT(pddolist, surface->mPts, sizeof(ddCoord3D));
	  if (!(out_pt3 = pddolist->pts.p3Dpt)) return(BadAlloc);
	  for ( i = 1; i < surface->mPts; i++ ) {
	    *(out_pt3++) = *(cpts3++);
	  }
	  (pddolist++)->numPoints = surface->mPts;
	}

    }

    /* Connect the points in the V dimension. */
    if ( rat ) {

	for ( j = 0; j < surface->nPts; j++ ) {
	  cpts4 = &surface->points.ddList->pts.p4Dpt[j];
	  MI_ALLOCLISTOFDDPOINT(pddolist, surface->nPts, sizeof(ddCoord4D));
	  if (!(out_pt4 = pddolist->pts.p4Dpt)) return(BadAlloc);
	  for ( i = 1; i < surface->nPts; i++ ) {
	    *(out_pt4++) = *cpts4;
	    cpts4 += surface->mPts;
	  }
	  (pddolist++)->numPoints = surface->nPts;
	}

    } else {

	for ( j = 0; j < surface->nPts; j++ ) {
	  cpts3 = &surface->points.ddList->pts.p3Dpt[j];
	  MI_ALLOCLISTOFDDPOINT(pddolist, surface->nPts, sizeof(ddCoord3D));
	  if (!(out_pt3 = pddolist->pts.p3Dpt)) return(BadAlloc);
	  for ( i = 1; i < surface->nPts; i++ ) {
	    *(out_pt3++) = *cpts3;
	    cpts3 += surface->mPts;
	  }
	  (pddolist++)->numPoints = surface->nPts;
	}

    }

    return Success;
}



/*++
 |
 |  Function Name:      free_grids
 |
 |  Function Description:
 |
 |	free the allocated data associated with a list of grids.
 |
 |  Note(s):
 |
 --*/

static void
free_grids( grids )
    Nurb_gridlst	*grids;
{
    int			i;

    if ( grids && grids->number > 0 ) {
	for ( i = 0; i < grids->number; i++ ) {
	    if ( grids->grids[i].pts )
		xfree( grids->grids[i].pts );
	}
	xfree( grids->grids );
    }
}



/*++
 |
 |  Function Name:      nurb_surf_state_free
 |
 |  Function Description:
 |
 |	free all of the allocated data associated with a surface
 |	Nurb_surf_state data structure.
 |
 |  Note(s):
 |
 --*/

static void
nurb_surf_state_free( state )
    Nurb_surf_state	*state;
{
    int facet;

    /* Free everything but the cache data. */
    if ( state->ruknots )
	xfree( state->ruknots );
    if ( state->rvknots )
	xfree( state->rvknots );

#ifdef TRIMING
    phg_nt_free_trim_data( &state->trim_data );
#endif /* TRIMING */

    if ( state->reps.facets ) {
      if ( state->facets ) {
	MI_FREELISTHEADER(state->facets);
        for (facet = 0; facet < state->grids.number; facet++)
            MI_FREELISTHEADER(state->facets + facet);
	xfree(state->facets);
      }
      else if ( state->sofas ) {
	MI_FREELISTHEADER(&(state->sofas->points));
	xfree(state->sofas);
      }
    }
    if ( state->reps.edges && state->edges ) {
	MI_FREELISTHEADER(state->edges);
	xfree(state->edges);
    }
    if ( state->reps.isocrvs && state->isocrvs ) {
	MI_FREELISTHEADER(state->isocrvs);
	xfree(state->isocrvs);
    }
    if ( state->reps.markers && state->markers) {
	/* Note that markers are a copy of the input data - DON`T FREE IT */
	xfree(state->markers);
    }
    if ( state->reps.hollow && state->hollow ) {
	MI_FREELISTHEADER(state->hollow);
	xfree(state->hollow);
    }

    if ( state->grids.number > 0 ) {
	free_grids( &state->grids );
	state->grids.number = 0;
	state->grids.grids = (Nurb_grid *)NULL;
    }
}



#ifdef TRIMING

/*++
 |
 |  Function Name:      add_pgon_point
 |
 |  Function Description:
 |
 |	Enter the specified edge point into a miSOFASStruct
 |	according to the specified operation.
 |
 |  Note(s):
 |
 --*/

static ddpex3rtn
add_pgon_point(  state, surface, ddSOFAS, op, ep )
    Nurb_surf_state		*state;
    miNurbSurfaceStruct		*surface;
    miSOFASStruct		*ddSOFAS;
    Nurb_facet_op		op;
    Nurb_edge_point		*ep;
{
   miConnListList	*ConnListList;
   miConnList		*ConnList;
   int			num_points, data_count, i;

   /* a new point for the vertex list */
   if (ddSOFAS->points.ddList) 
     num_points = ddSOFAS->points.ddList->numPoints;
   else num_points = 0;

   switch (op) {

     /*
      * A new facet is to be started in the SOFAS output structure.
      * The first task to complete is to insure that there is sufficient
      * space for the new list od list of indices in the connection lists.
      */
     case NURB_NEW_FACET:
	ddSOFAS->connects.numListLists++;
	data_count = MI_ROUND_LISTHEADERCOUNT(ddSOFAS->connects.numListLists)
		     * sizeof(miConnListList);

	if (data_count > ddSOFAS->connects.maxData) {
	  if (ddSOFAS->connects.data) {
	    ddSOFAS->connects.data = 
		 (miConnListList *)xrealloc(ddSOFAS->connects.data, data_count);
	    ddSOFAS->connects.maxData = data_count;
	  } else {
	    ddSOFAS->connects.data = (miConnListList *)xalloc(data_count);
	    ddSOFAS->connects.maxData = data_count;
	  }

	  if (!(ddSOFAS->connects.data)) return(BadAlloc);

	  /* Initialize newly created entries */
	  ConnListList = &ddSOFAS->connects.data[ddSOFAS->numFAS];
	  for (i = MI_ROUND_LISTHEADERCOUNT(ddSOFAS->connects.numListLists) -
		   ddSOFAS->numFAS;
	       i > 0;
	       i--) {
	    ConnListList->numLists = ConnListList->maxData = 0;
	    (ConnListList++)->pConnLists = 0;
	  
	  }
	}

        ddSOFAS->numFAS++;
	  
        
     /*
      * a new contour is to be started in the SOFAS output structure.
      * The first task is to insure that there is sufficient output
      * space for the contour is the list of contour index lists
      */
     case NURB_NEW_CONTOUR:
	ConnListList = &ddSOFAS->connects.data[ddSOFAS->numFAS - 1];

	data_count = MI_ROUND_LISTHEADERCOUNT( ConnListList->numLists+1 )
		     * sizeof(miConnList);

	if (data_count > ConnListList->maxData) {

	  if (ConnListList->pConnLists) {
	    ConnListList->pConnLists = 
				(miConnList *)xrealloc(ConnListList->pConnLists,
						       data_count );
	    ConnListList->maxData = data_count;
	  } else {
	    ConnListList->pConnLists = (miConnList *)xalloc(data_count);
	    ConnListList->maxData = data_count;
	  }

	  if (!(ConnListList->pConnLists)) return (BadAlloc);
	
	  /* Initialize newly created entries */
	  ConnList = &ConnListList->pConnLists[ConnListList->numLists];
	  for (i = MI_ROUND_LISTHEADERCOUNT( ConnListList->numLists+1 ) -
		   ConnListList->numLists;
	       i > 0;
	       i--) {
	    ConnList->numLists = ConnList->maxData = 0;
	    (ConnList++)->pConnects = 0;
	  
	  }
	}

	ConnListList->numLists++;

     /*
      * Finally, insure sufficent space for the index!
      */
     case NURB_SAME_CONTOUR:
	ConnListList = &ddSOFAS->connects.data[ddSOFAS->numFAS - 1];
	ConnList = &ConnListList->pConnLists[ConnListList->numLists - 1];

	data_count = MI_ROUND_LISTHEADERCOUNT( ConnList->numLists+1 )
		     * sizeof(ddUSHORT);

	if (data_count > ConnList->maxData) {
	  if (ConnList->pConnects) {
	    ConnList->pConnects = (ddUSHORT *)xrealloc( ConnList->pConnects,
							data_count);
	    ConnList->maxData = data_count;
	  } else {
	    ConnList->pConnects = (ddUSHORT *)xalloc(data_count);
	    ConnList->maxData = data_count;
	  }

	  if (!(ConnList->pConnects)) return (BadAlloc);
	}

	/* Lastly, enter the index into the appropriate list */
	ConnList->pConnects[ConnList->numLists] = num_points;  
	ConnList->numLists++;


   }


   /* Insure there is a list for the vertex data */
   if (!(ddSOFAS->points.ddList)) {

     MI_ALLOCLISTHEADER(&ddSOFAS->points, 1)
     if (!(ddSOFAS->points.ddList)) return(BadAlloc);

     if (state->reps.normals) ddSOFAS->points.type = DD_NORM_POINT4D;
     else ddSOFAS->points.type = DD_HOMOGENOUS_POINT;

     ddSOFAS->points.flags = 0;
     ddSOFAS->points.numLists = 1;

   }

   /* Now, enter the point information into the point array */
   if (state->reps.normals) {

     MI_ALLOCLISTOFDDPOINT(ddSOFAS->points.ddList,
			   MI_ROUND_LISTHEADERCOUNT(num_points + 1),
			   sizeof(ddNormalPoint4D));
     if (!(ddSOFAS->points.ddList->pts.ptr))
       return(BadAlloc);

     ddSOFAS->points.ddList->pts.pNpt4D[num_points].pt = ep->pt;
     ddSOFAS->points.ddList->pts.pNpt4D[num_points].normal = ep->normal;

   } else {

     MI_ALLOCLISTOFDDPOINT(ddSOFAS->points.ddList,
			   MI_ROUND_LISTHEADERCOUNT(num_points + 1),
			   sizeof(ddCoord4D));
     if (!(ddSOFAS->points.ddList->pts.ptr))
       return(BadAlloc);

     ddSOFAS->points.ddList->pts.p4Dpt[num_points] = ep->pt;
   }

   ddSOFAS->points.ddList->numPoints = num_points + 1;

}

#endif /* TRIMING */

#define WS_NSRF_BOTTOM
