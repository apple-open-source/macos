/* $Xorg: miNSTrim.c,v 1.4 2001/02/09 02:04:10 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/level2/miNSTrim.c,v 3.7 2001/12/14 19:57:28 dawes Exp $ */

#include "mipex.h"
#include "misc.h"
#include "miscstruct.h"
#include "miRender.h"
#include "PEXErr.h"
#include "gcstruct.h"
#include "ddpex2.h"
#include "miNurbs.h"
#include "pexos.h"

#define INACTIVE	0

#define NEW_SEG( _td ) \
    ((Nurb_trim_segment *)xalloc( sizeof(Nurb_trim_segment) ))

/*++
 |
 |  Function Name:	phg_nt_free_trim_data
 |
 |  Function Description:
 |
 |	frees data allocated during the triming process.
 |
 |  Note(s):
 |
 --*/

void
phg_nt_free_trim_data( tdata )
    register Nurb_trim_data	*tdata;
{

    register int		i;
    register Nurb_trim_segment	*seg, *next_seg;

    for ( i = 0; i < tdata->nloops; i++ ) {
	for ( seg = tdata->loops[i].segs; seg; seg = next_seg ) {
	    next_seg = seg->next;
	    xfree( seg );
	}
    }
    tdata->nloops = 0;

    if ( tdata->vertices ) {
	xfree( tdata->vertices );
	tdata->vertices = (Nurb_param_point *)NULL;
    }
    if ( tdata->loops ) {
	xfree( tdata->loops );
	tdata->loops = (Nurb_trim_loop_rep *)NULL;
    }
    if ( tdata->ep_list_size > 0 ) {
	xfree( tdata->ep_list );
	tdata->ep_list_size = 0;
	tdata->ep_list = (Nurb_edge_point *)NULL;
    }
}



#define CHUNK_SIZE 5

/*++
 |
 |  Function Name:	grow_range_list
 |
 |  Function Description:
 |
 |  Note(s):
 |
 --*/

static int
grow_range_list( itr, inters )
    int		itr;
    double	**inters;
{
    if ( itr == 0 )
	*inters = (double *)xalloc( CHUNK_SIZE * sizeof(double) );
    else
	*inters = (double *)xrealloc( *inters,
	    (itr + CHUNK_SIZE) * sizeof(double) );

    return (*inters ? Success : BadAlloc);
}

/*++
 |
 |  Function Name:	compute_intersections
 |
 |  Function Description:
 |
 |  Note(s):
 |
 --*/

static int
compute_intersections( state, dir, s, inters, inter_count )
    Nurb_surf_state	*state;
    int			dir;	/* coordinate direction, 1 ==> U, 2 ==> V */
    double		s;
    double		**inters;
    int			*inter_count;
{
    register int		i, j;
    register int		itr;
    register Nurb_trim_data	*tdata;
    register Nurb_trim_segment	*cur;
    register Nurb_param_limit	*ext;
    register Nurb_param_point	*trim_pts;
    register double		pa, pb, pc, pd;
    register double		alpha;

    itr = 0;
    tdata = &state->trim_data;
    trim_pts = tdata->vertices;

    for ( i = 0; i < tdata->nloops; i++ ) {
	ext = &tdata->loops[i].extent;
	if (    (dir == 1 && s > ext->umin && s <= ext->umax )
	     || (dir == 2 && s > ext->vmin && s <= ext->vmax ) ) {
	    for ( cur = tdata->loops[i].segs; cur; cur = cur->next ) {
		ext = &cur->extent;
		if (    (dir == 1 && s > ext->umin && s <= ext->umax )
		     || (dir == 2 && s > ext->vmin && s <= ext->vmax ) ) {
		    for ( j = cur->first; j < cur->last; j++ ) {
			if ( dir == 1 ) {	/* U direction */
			    pa = trim_pts[j].u; pb = trim_pts[j+1].u;
			    pc = trim_pts[j].v; pd = trim_pts[j+1].v;
			} else {		/* V direction */
			    pa = trim_pts[j].v; pb = trim_pts[j+1].v;
			    pc = trim_pts[j].u; pd = trim_pts[j+1].u;
			}
			if ( s > pa && s <= pb || s > pb && s <= pa ) {
			    if ( itr % CHUNK_SIZE == 0 )
				if ( grow_range_list( itr, inters ) ) {
				    return (BadAlloc);
				}
			    alpha = (s - pa) / (pb - pa);
			    (*inters)[itr++] = pc + alpha * (pd - pc);
			}
		    }
		}
	    }
	}
    }

    *inter_count = itr;
    return ( Success );
}
#undef CHUNK_SIZE



/*++
 |
 |  Function Name:	phg_nt_compute_trim_range
 |
 |  Function Description:
 |
 |  Note(s):
 |
 --*/

int
phg_nt_compute_trim_range( state, dir, s, knot_min, knot_max, 
			   ranges, trim_range_count )
    Nurb_surf_state	*state;
    int			dir;	/* coordinate direction, 1 ==> U, 2 ==> V */
    double		s;
    double		knot_min;
    double		knot_max;
    Nurb_limitlst	*ranges;
    int			*trim_range_count;
{
    /* -1 ==> paint entire curve
     *  0 ==> discard the curve
     * >0 ==> paint in between trim ranges
     */

    int		inter_count;
    double	tmp;
    double	*intersections = (double *)NULL;
    int		trim_ranges = -1;
    int		status;

    register int	i, j;

    if ( state->trim_data.nloops <= 0 ) {
	*trim_range_count = -1;
	return ( Success );
    }

    if ( status = compute_intersections( state, dir, s, 
					  &intersections, &inter_count ))
      return(status);

    if ( inter_count == -1 || inter_count == 0 ) {
	/* All painted or none painted. */
	trim_ranges = inter_count;

    } else if ( inter_count == 1 && intersections[0] <= knot_min ) {
	/* All painted */
	trim_ranges = -1;

    } else {
	ranges->number = 0;
	if ( inter_count % 2 == 1 ) {
	    /* Odd number of intersections. */
	    intersections[inter_count] = 1.0E30;
	}

	if ( inter_count > 0 ) {
	    /* Get space for range values. */
	    if ( inter_count > ranges->size ) {
		if ( ranges->size <= 0 ) {
		    ranges->size = inter_count;
		    ranges->limits = (Nurb_limit *)
			xalloc( ranges->size * sizeof(Nurb_limit) );
		} else {
		    ranges->size = inter_count;
		    ranges->limits = (Nurb_limit *) xrealloc( ranges->limits,
			ranges->size * sizeof(Nurb_limit) );
		}

		if ( ! ranges->limits ) {
		    ranges->size = 0;
		    xfree( intersections );
		    return ( BadAlloc );
		}
	    }
	}

	if ( inter_count > 1 ) {
	    /* Sort intersections, lowest to highest. */
	    for ( j = 0; j < inter_count-1; j++ ) {
		for ( i = j+1; i < inter_count; i++ ) {
		    if ( intersections[j] > intersections[i] ) {
			tmp = intersections[i];
			intersections[i] = intersections[j];
			intersections[j] = tmp;
		    }
		}
	    }
	}

	/* Compute trim ranges. */
	for ( j = 0; j < inter_count; ) {
	    if ( intersections[j] >= knot_max ) {
		/* Out of range, not painted. */
		break;
	    } else {
		if ( intersections[j] <= knot_min )
		    ranges->limits[ranges->number].lmin = knot_min;
		else
		    ranges->limits[ranges->number].lmin = intersections[j];
		++j;

		if ( intersections[j] >= knot_min ) {
		    if ( intersections[j] >= knot_max)
			ranges->limits[ranges->number].lmax = knot_max;
		    else
			ranges->limits[ranges->number].lmax = intersections[j];
		    ++ranges->number;
		}
		++j;
	    }
	}
	trim_ranges = ranges->number;
    }

    if ( intersections )
	xfree( intersections );

    *trim_range_count = trim_ranges;

    return( Success );
}



/*++
 |
 |  Function Name:	evaluate_trim_curve
 |
 |  Function Description:
 |
 |  Note(s):
 |
 --*/

static void
evaluate_trim_curve( crv, span, t, p )
    ddTrimCurve	*crv;
    int		span;
    double	t;
    ddCoord3D	*p;
{
    ddCoord3D		temp_points[MAXORD] ;
    double		alpha;

    register int	i, j, k, left;
    register int	order = crv->order;
    register ddFLOAT	*knots = crv->pKnots;
    register ddCoord3D	*tmp;
    char		rat;

    /* Find the span where t belongs if not specified. */
    if ( span )
	left = span - order;
    else {
	left = crv->numKnots - 1;
	if ( knots[left] == t )
	    while ( knots[left] >= t )
		--left;  
	else
	    while ( knots[left] > t )
		--left;
	left -= order - 1;
    }
    
    /* Note that 3D implies homogeneous for trim curves */
    rat = DD_IsVert3D(crv->pttype);

    if (rat) {
      ddCoord3D	*cpts3;

      /* Copy points to temp space. */
      cpts3 = &crv->points.pts.p3Dpt[left];
      tmp = temp_points;
      memcpy( (char *)tmp, (char *)cpts3, order * sizeof(ddCoord3D) );

    } else {
      ddCoord2D	*cpts2;

      /* Copy points to temp space. */
      cpts2 = &crv->points.pts.p2Dpt[left];
      tmp = temp_points;
      for ( i = 0; i < order; i++, cpts2++, tmp++ ) {
	  tmp->x = cpts2->x;
	  tmp->y = cpts2->y;
	  tmp->z = 1.0;
      }

    }

    /* Evaluate the span. */
    tmp = temp_points;
    for ( k = 1; k < order; k++ ) {
	for ( j = order-1; j >= k; j-- ) {
	    i = left + j;
	    alpha = (t - knots[i]) / (knots[i+order-k] - knots[i]);
	    tmp[j].x = tmp[j-1].x + alpha * (tmp[j].x - tmp[j-1].x);
	    tmp[j].y = tmp[j-1].y + alpha * (tmp[j].y - tmp[j-1].y);
	    if ( rat )
		tmp[j].z = tmp[j-1].z + alpha * (tmp[j].z - tmp[j-1].z);
	}
    }

    p->x = tmp[order-1].x;
    p->y = tmp[order-1].y;
    p->z = ( ( rat ) ? tmp[order-1].z : 1.0 );
}



#define ADD_TRIM_POINT( _r, _u, _v, _w ) \
  { \
    if ( (_r) ) { \
	tdata->vertices[tdata->cur_vertex].u = (_u) / (_w); \
	tdata->vertices[tdata->cur_vertex].v = (_v) / (_w); \
    } else { \
	tdata->vertices[tdata->cur_vertex].u = (_u); \
	tdata->vertices[tdata->cur_vertex].v = (_v); \
    } \
    ++tdata->cur_vertex; \
  }

/*++
 |
 |  Function Name:	add_trim_curve
 |
 |  Function Description:
 |
 |  Note(s):
 |
 --*/

static void
add_trim_curve( state, crv, tdata, seg )
    Nurb_surf_state	*state;
    ddTrimCurve		*crv;
    Nurb_trim_data	*tdata;
    Nurb_trim_segment	*seg;
{
    double	t, dt;
    double	tmin = crv->uMin;
    double	tmax = crv->uMax;
    double	left, right;
    ddCoord3D	p;
    char	rat;

    int		i;
    ddFLOAT	*knots = crv->pKnots;
    ddFLOAT	tolerance;

    /* Initialize segment and store location of first curve vertex. */
    seg->vis = crv->visibility;
    seg->start = tdata->cur_vertex;
    seg->first = seg->start;

    /* Note that 3D implies homogeneous for trim curves */
    rat = DD_IsVert3D(crv->pttype);

    /* Tessellate the trim curve and add the points to the vertex list. */

    /* Special case second order (linear) curves. */
    if ( crv->order == 2 ) {
	double	alpha, beta;

	if (rat) {

	  ddCoord3D	*pts3 = crv->points.pts.p3Dpt;

	  /* Find first interval of interest. */
	  if (tmin < knots[1]) tmin = knots[1];
	  for ( i = 2; knots[i] <= tmin; i++ )
	    ;

	  /* First point. */
	  alpha = (tmin - knots[i-1]) / (knots[i] - knots[i-1]);
	  beta = 1.0 - alpha;
	  p.x = beta * pts3[i-2].x + alpha * pts3[i-1].x;
	  p.y = beta * pts3[i-2].y + alpha * pts3[i-1].y;
	  p.z = beta * pts3[i-2].z + alpha * pts3[i-1].z;
	  ADD_TRIM_POINT( rat, p.x, p.y, p.z );

	  /* Interior points. */
	  if (tmax > knots[crv->numKnots - 2]) tmax = knots[crv->numKnots - 2];
	  for ( ; knots[i] < tmax; i++ ) {
	    ADD_TRIM_POINT( rat, pts3[i-1].x, pts3[i-1].y, pts3[i-1].z )
	  }

	  /* Last point. */
	  alpha = (tmax - knots[i-1]) / (knots[i] - knots[i-1]);
	  beta = 1.0 - alpha;
	  p.x = beta * pts3[i-2].x + alpha * pts3[i-1].x;
	  p.y = beta * pts3[i-2].y + alpha * pts3[i-1].y;
	  p.z = beta * pts3[i-2].z + alpha * pts3[i-1].z;
	  ADD_TRIM_POINT( rat, p.x, p.y, p.z );

	} else {

	  ddCoord2D	*pts2 = crv->points.pts.p2Dpt;

	  /* Find first interval of interest. */
	  if (tmin < knots[1]) tmin = knots[1];
	  for ( i = 2; knots[i] <= tmin; i++ )
	    ;

	  /* First point. */
	  alpha = (tmin - knots[i-1]) / (knots[i] - knots[i-1]);
	  beta = 1.0 - alpha;
	  p.x = beta * pts2[i-2].x + alpha * pts2[i-1].x;
	  p.y = beta * pts2[i-2].y + alpha * pts2[i-1].y;
	  ADD_TRIM_POINT( rat, p.x, p.y, 1.0 );

	  /* Interior points. */
	  if (tmax > knots[crv->numKnots - 2]) tmax = knots[crv->numKnots - 2];
	  for ( ; knots[i] < tmax; i++ ) {
	    ADD_TRIM_POINT( rat, pts2[i-1].x, pts2[i-1].y, 1.0 )
	  }

	  /* Last point. */
	  alpha = (tmax - knots[i-1]) / (knots[i] - knots[i-1]);
	  beta = 1.0 - alpha;
	  p.x = beta * pts2[i-2].x + alpha * pts2[i-1].x;
	  p.y = beta * pts2[i-2].y + alpha * pts2[i-1].y;
	  ADD_TRIM_POINT( rat, p.x, p.y, 1.0 );
	}

    } else if ( crv->order > MAXTCORD || crv->order < 2) { /* unsupported */
	/* Use untrimmed control polygon. */
	if (rat) {
          ddCoord3D	*pts3 = crv->points.pts.p3Dpt;
	  for ( i = 0; i < crv->points.numPoints; i++ ) 
	    ADD_TRIM_POINT(rat, pts3[i].x, pts3[i].y, pts3[i].z );
	} else {
          ddCoord2D	*pts2 = crv->points.pts.p2Dpt;
	  for ( i = 0; i < crv->points.numPoints; i++ ) 
	    ADD_TRIM_POINT(rat, pts2[i].x, pts2[i].y, 1.0 );
	} 

    } else { /* supported order > 2 */
	GET_TRIM_CURVE_TOLERANCE(crv, tolerance);

#ifdef BWEE
	if ( crv->curveApprox.approxMethod == PCURV_CONSTANT_PARAMETRIC ) {
	    /* First point. */
	    evaluate_trim_curve( crv, 0, tmin, &p );
	    ADD_TRIM_POINT( rat, p.x, p.y, p.z );

	    /* Interior points. */
	    dt = (tmax - tmin) / (tolerance + 1);
	    for ( t = tmin + dt; t < tmax; t += dt ) {
		evaluate_trim_curve( crv, 0, t, &p );
		ADD_TRIM_POINT( rat, p.x, p.y, p.z );
	    }

	    /* Last point. */
	    evaluate_trim_curve( crv, 0, tmax, &p );
	    ADD_TRIM_POINT( rat, p.x, p.y, p.z );

	} else 
#endif /* BWEE */

	{

	    for ( i = crv->order - 1; i < crv->points.numPoints; i++ ) {
		if ( knots[i] != knots[i+1]
		     && knots[i] <= tmax && knots[i+1] >= tmin ) {
		    left = knots[i];
		    right = knots[i+1];
		    dt = (right - left) / (tolerance + 1);
		    t = left;

		    /* If interval contains tmin get first point(s). */
		    if ( left <= tmin && right >= tmin ) {
			while ( t < tmin )
			    t += dt;
			evaluate_trim_curve( crv, i+1, tmin, &p );
			ADD_TRIM_POINT( rat, p.x, p.y, p.z );
			if ( t > tmin ) {
			    evaluate_trim_curve( crv, i+1, t, &p );
			    ADD_TRIM_POINT( rat, p.x, p.y, p.z );
			}
			t += dt;
		    }

		    /* Interior points. */
		    while ( t < right && t < tmax ) {
			evaluate_trim_curve( crv, i+1, t, &p );
			ADD_TRIM_POINT( rat, p.x, p.y, p.z );
			t += dt;
		    }

		    /* If interval contains tmax get last point. */
		    if ( left <= tmax && right >= tmax ) {
			evaluate_trim_curve( crv, i+1, tmax, &p );
			ADD_TRIM_POINT( rat, p.x, p.y, p.z );
			break; /* all done */
		    }
		}
	    }
	}
    }

    /* Store location of last vertex */
    seg->end = tdata->cur_vertex - 1;
    seg->last = seg->end;
    seg->next = NULL;
}



#define CONNECT(_pa, _pb) \
  { \
    (_pb).u = (_pa).u; \
    (_pb).v = (_pa).v; \
  }

/*++
 |
 |  Function Name:	connect_trim_endpoints
 |
 |  Function Description:
 |
 |	Insures that a list of tesselated trim curve segments 
 |	forms a closed loop.
 |
 |  Note(s):
 |
 --*/

static void
connect_trim_endpoints( tdata, seglist )
    Nurb_trim_data		*tdata;
    register Nurb_trim_segment	*seglist;	/* linked list of segments */
{
    register Nurb_param_point	*trim_pts = tdata->vertices;
    register Nurb_trim_segment	*cur;

    /* Connect the tail of each curve to the head of the following curve. */
    for ( cur = seglist; cur; cur = cur->next ) {
	if ( cur->next )
	    CONNECT( trim_pts[cur->last], trim_pts[cur->next->first] )
	else /* tail of last one to head of first one */
	    CONNECT( trim_pts[seglist->first], trim_pts[cur->last] )
    }
}



/*++
 |
 |  Function Name:	make_segments_monotonic
 |
 |  Function Description:
 |
 |	Each curve segment description is traversed and, if
 |	necessary, subdivided into smaller segments until
 |	each remaining segment in the list of segments contains
 |	only monotonically increasing or decreasing (in both u & v) 
 |	line segments (in other words, there are no changes in sign
 |	in the slope in either u and v in any segment).
 |
 |  Note(s):
 |
 --*/

static int
make_segments_monotonic( tdata, tdloop )
    Nurb_trim_data	*tdata;
    Nurb_trim_loop_rep	*tdloop;
{
    unsigned	old_u, old_v, u_direc, v_direc;

    register int		top, bot, mid;
    register Nurb_param_point	*trim_pts = tdata->vertices;
    register Nurb_trim_segment	*cur, *new;

    for ( cur = tdloop->segs; cur; cur = cur->next ) {
	top = cur->first;
	mid = top;
	bot = cur->last;
	old_u = old_v = 0;
	while (1) {
	    ++mid ;
	    if ( trim_pts[mid].u > trim_pts[top].u)
		u_direc = 1;
	    else if ( trim_pts[mid].u < trim_pts[top].u )
		u_direc = 2;
	    else
		u_direc = old_u;

	    if ( trim_pts[mid].v > trim_pts[top].v )
		v_direc = 1;
	    else if ( trim_pts[mid].v < trim_pts[top].v )
		v_direc = 2;
	    else
		v_direc = old_v;

	    if ( (u_direc | old_u) == 3 || (v_direc | old_v) == 3 ) {
		cur->dir = ((old_u < 2) << 1) | (old_v < 2);
		if ( !(new = NEW_SEG( tdata )) )
		    return BadAlloc;	/* RETURN */

		new->next = cur->next;
		cur->next = new;
		cur->end = cur->last = top;
		new->start = new->first = top;
		new->end = new->last = bot;
		new->vis = cur->vis;
		cur = new;
		old_u = old_v = 0;
	    }
	    old_u = u_direc;
	    old_v = v_direc;
	    if ( mid == bot ) {	/* LOOP EXIT CONDITION */
		cur->dir = ((old_u < 2) << 1) | (old_v < 2);
		break;
	    }
	    top = mid;
	} /* while (1) */
    }

    return Success;
}



/*++
 |
 |  Function Name:	compute_trim_curve_extents
 |
 |  Function Description:
 |
 |	This function computes a trim loop extent
 |	as well as the extents of each of the segments
 |	in the loop.
 |
 |  Note(s):
 |
 --*/

static void
compute_trim_curve_extents( tdata, tdloop )
    Nurb_trim_data	*tdata;
    Nurb_trim_loop_rep	*tdloop;
{
    double	umin, vmin, umax, vmax;

    register int		top, bot;
    register Nurb_param_point	*trim_pts = tdata->vertices;
    register Nurb_trim_segment	*cur;

    umin = vmin = 1.0E30; /* large number */
    umax = vmax = -umin;
    /* Assumes each segment is monotonic in both directions. */
    for ( cur = tdloop->segs; cur; cur = cur->next ) {
	if ( cur->start == INACTIVE || cur->end == INACTIVE )
	    continue;
	top = cur->start;
	bot = cur->end;

	if ( trim_pts[top].u >= trim_pts[bot].u ) {
	    cur->extent.umin = trim_pts[bot].u;
	    cur->extent.umax = trim_pts[top].u;
	} else {
	    cur->extent.umin = trim_pts[top].u;
	    cur->extent.umax = trim_pts[bot].u;
	}

	if ( trim_pts[top].v >= trim_pts[bot].v ) {
	    cur->extent.vmin = trim_pts[bot].v;
	    cur->extent.vmax = trim_pts[top].v;
	} else {
	    cur->extent.vmin = trim_pts[top].v;
	    cur->extent.vmax = trim_pts[bot].v;
	}

	if ( cur->extent.umin < umin )
	    umin = cur->extent.umin;
	if ( cur->extent.umax > umax )
	    umax = cur->extent.umax;
	if ( cur->extent.vmin < vmin )
	    vmin = cur->extent.vmin;
	if ( cur->extent.vmax > vmax )
	    vmax = cur->extent.vmax;
    }
    tdloop->extent.umin = umin;
    tdloop->extent.umax = umax;
    tdloop->extent.vmin = vmin;
    tdloop->extent.vmax = vmax;
}



/*++
 |
 |  Function Name:	phg_nt_install_trim_loops
 |
 |  Function Description:
 |
 |	this routine creates a linked list of sorted tesselated
 |	curve segments as a prelude to trimming.
 |
 |  Note(s):
 |
 --*/

int
phg_nt_install_trim_loops( surface, state )
    miNurbSurfaceStruct		*surface;
    Nurb_surf_state		*state;
{
    int				interval_count;
    listofTrimCurve		*loop;
    Nurb_trim_segment		*seg;

    int				i, j;
    Nurb_trim_data		*tdata = &state->trim_data;
    ddTrimCurve			*crv;
    Nurb_trim_loop_rep		*tdloop;
    Nurb_trim_segment		**last_segp;
    ddFLOAT			tolerance;

    /* Determine sizes for initial memory allocations. */
    interval_count = 0;
    loop = surface->trimCurves;
    for ( i = 0; i < surface->numTrimCurveLists; i++, loop++ ) {
	crv = loop->pTC;
	for (j = 0; j < loop->count; j++, crv++) {
	  GET_TRIM_CURVE_TOLERANCE( crv, tolerance );
	    interval_count += (crv->points.numPoints * (tolerance + 4));
	}
    }

    if ( ! (tdata->vertices = (Nurb_param_point *)
	xalloc( interval_count * sizeof(tdata->vertices[0]) )) )
	goto abort;
    
    if ( ! (tdata->loops = (Nurb_trim_loop_rep *)
	xalloc( surface->numTrimCurveLists * sizeof(tdata->loops[0]) )) )
	goto abort;

    /* Initialize the loop structures. */
    for ( i = 0; i < surface->numTrimCurveLists; i++ )
	tdata->loops[i].segs = (Nurb_trim_segment *)NULL;

    /* Build the initial (non-monotonic) trim curve segments. */
    for ( i = 0, loop = surface->trimCurves; 
	  i < surface->numTrimCurveLists; i++, loop++ ) {

	tdloop = &tdata->loops[i];
	for ( j = 0, crv = loop->pTC; j < loop->count; j++, crv++ ) {
	    if ( !(seg = NEW_SEG( tdata )) )
		goto abort;
	    add_trim_curve( state, crv, tdata, seg );
	    /* Add the new segment to the end of the list. */
	    last_segp = &tdloop->segs;
	    while ( *last_segp )
		last_segp = &(*last_segp)->next;
	    *last_segp = seg;

	}

	++tdata->nloops;
	connect_trim_endpoints( tdata, tdloop->segs );
	if ( make_segments_monotonic( tdata, tdloop ) )
	    goto abort;
	compute_trim_curve_extents( tdata, tdloop );
    }

    return Success;

abort:
    phg_nt_free_trim_data( &state->trim_data );
    return BadAlloc;
}



#ifdef NDEBUG

#define PRINT_EXTENT( _e ) \
    fprintf( stderr, "extent: u = ( %f, %f), v = ( %f, %f)\n", \
    (_e).umin, (_e).umax, (_e).vmin, (_e).vmax  );

/*++
 |
 |  Function Name:	phg_nt_print_trim_rep_data
 |
 |  Function Description:
 |
 |  Note(s):
 |
 --*/

void
phg_nt_print_trim_rep_data( state )
    Nurb_surf_state	*state;
{
    Nurb_trim_data	*tdata = &state->trim_data;
    Nurb_trim_loop_rep	*loop;
    Nurb_trim_segment	*cur;

    register int	i, j;

    fprintf( stderr, "Trim data: %d loops\n", tdata->nloops );
    loop = tdata->loops;
    for ( i = 0; i < tdata->nloops; i++, loop++ ) {
	fprintf( stderr, "loop %d\n", i );
	PRINT_EXTENT( loop->extent )
	for ( cur = loop->segs; cur; cur = cur->next ) {
	    fprintf( stderr, "\n\tsegment: visibility = %s, direction = %d\n",
		cur->vis ? "PON" : "POFF", cur->dir );
	    fprintf( stderr, "\tfirst = %d, last = %d, start = %d, end = %d\n",
		cur->first, cur->last, cur->start, cur->end );
	    fprintf( stderr, "\t" );
	    PRINT_EXTENT( cur->extent )
	    for ( j = cur->first; j <= cur->last; j++ )
		fprintf( stderr, "\t\t%3d: ( %10f, %10f)\n", j,
		    tdata->vertices[j].u, tdata->vertices[j].v );
	}
    }
}
#endif /* NDEBUG */



/* Polygon Trimming Code */

#define INACTIVE	0

#define VISITED		4
#define NEXT		2
#define LAST		1

#define EP_LIST_CHUNK	10	/* must be >= 5 */

#define xin(_a,_b,_x) ((_x) >= (_a) && (_x) <= (_b))


#define ROOM_IN_EP_LIST( _td ) \
  ( (_td)->ep_index < (_td)->ep_list_size || !grow_ep_list(_td) )

/*++
 |
 |  Function Name:	grow_ep_list
 |
 |  Function Description:
 |
 |  Note(s):
 |
 --*/

static int
grow_ep_list( tdata )
    Nurb_trim_data	*tdata;
{
    /* TODO: Error reporting for failed allocations. */
    tdata->ep_list_size += EP_LIST_CHUNK;
    if ( tdata->ep_list_size == EP_LIST_CHUNK )
	tdata->ep_list = (Nurb_edge_point *) xalloc(
	    EP_LIST_CHUNK * sizeof(Nurb_edge_point) );
    else
	tdata->ep_list = (Nurb_edge_point *) xrealloc( tdata->ep_list,
	    tdata->ep_list_size * sizeof(Nurb_edge_point) );

    if ( !tdata->ep_list )
	tdata->ep_list_size = 0;

    return (tdata->ep_list ? Success : BadAlloc);
}



/*++
 |
 |  Function Name:	linear_interpolate
 |
 |  Function Description:
 |
 |  Note(s):
 |
 --*/

static void
linear_interpolate( alpha, rat, normflag, a, b, out )
    double			alpha;
    char			rat;
    unsigned			normflag;
    register Nurb_edge_point	*a, *b, *out;
{
    out->pt.x = a->pt.x + (b->pt.x - a->pt.x) * alpha;
    out->pt.y = a->pt.y + (b->pt.y - a->pt.y) * alpha;
    out->pt.z = a->pt.z + (b->pt.z - a->pt.z) * alpha;
    if ( rat )
	out->pt.w = a->pt.w + (b->pt.w - a->pt.w) * alpha;
    if ( normflag ) {
	out->normal.x = a->normal.x + (b->normal.x - a->normal.x) * alpha;  
	out->normal.y = a->normal.y + (b->normal.y - a->normal.y) * alpha;
	out->normal.z = a->normal.z + (b->normal.z - a->normal.z) * alpha;
    }
}

/*++
 |
 |  Function Name:	bilinear
 |
 |  Function Description:
 |
 |  Note(s):
 |
 --*/

static void
bilinear( alpha, beta, rat, normflag, a, b, c, d, out )
    double		alpha, beta;
    char		rat;
    unsigned		normflag;
    Nurb_edge_point	*a, *b, *c, *d;
    Nurb_edge_point	*out;
{
    Nurb_edge_point	top, bot;

    linear_interpolate( alpha, rat, normflag, a, b, &top );
    linear_interpolate( alpha, rat, normflag, d, c, &bot );
    linear_interpolate( beta, rat, normflag, &top, &bot, out );
}



/*++
 |
 |  Function Name:	append_pt
 |
 |  Function Description:
 |
 |  Note(s):
 |
 --*/

static int
append_pt( tdata, normflag, rat, u, v, epa, epb, epc, epd )
    Nurb_trim_data	*tdata;
    unsigned		normflag;
    char		rat;
    double		u, v;
    int			epa, epb, epc, epd;
{
    double	alpha, beta;
    int		status = BadAlloc;

    register Nurb_edge_point	*ep_list;
    register Nurb_edge_point	*cur_ep;

    if ( ROOM_IN_EP_LIST(tdata) ) {
	ep_list = tdata->ep_list;
	cur_ep = &ep_list[tdata->ep_index];
	cur_ep->u = u; cur_ep->v = v;
	cur_ep->flags = 0;       
	alpha = (v - ep_list[epa].v) / (ep_list[epc].v - ep_list[epa].v);
	beta =  (u - ep_list[epa].u) / (ep_list[epc].u - ep_list[epa].u);
	bilinear( alpha, beta, rat, normflag, &ep_list[epa], &ep_list[epb],
	    &ep_list[epc], &ep_list[epd], cur_ep );
	cur_ep->branch = 0;
	cur_ep->prev = tdata->ep_index - 1;
	cur_ep->next = tdata->ep_index + 1;
	++tdata->ep_index;
	status = Success;
    }

    return status;
}



/*++
 |
 |  Function Name:	insert_pt
 |
 |  Function Description:
 |
 |	This routine takes two indices into the
 |	Nurb_edge_point list, a point along one of the
 |	segment between the two edge points indicated
 |	by the two indices, and inserts a new edge
 |	point into the list corresponding to the
 |	supplied point.
 |
 |	Inserting a new point means that the next and
 |	previous indices of the neighboring edge points are 
 |	updated, and (x,y,z) and (if necessary) normal data 
 |	is computed from the neighboring points for the new point.
 |
 |  Note(s):
 |
 --*/

static int
insert_pt( tdata, normflag, rat, u, v, epa, epb, edge, branch )
    Nurb_trim_data	*tdata;
    unsigned		normflag;
    char		rat;
    int			epa, epb;
    double		u, v;
    int			edge, branch;
{
    double	alpha;
    int		status = BadAlloc;

    register int		eptr;
    register Nurb_edge_point	*ep_list;
    register Nurb_edge_point	*cur_ep;

    if ( ROOM_IN_EP_LIST(tdata) ) {
	ep_list = tdata->ep_list;
	cur_ep = &ep_list[tdata->ep_index];
	cur_ep->u = u; cur_ep->v = v;
	cur_ep->flags = 0;       
	switch ( edge ) {
	    case 0:
	    case 2:
		alpha = (v - ep_list[epa].v)/(ep_list[epb].v - ep_list[epa].v); 
		break;
	    case 1:
	    case 3:
		alpha = (u - ep_list[epa].u)/(ep_list[epb].u - ep_list[epa].u); 
		break;
	    default:
		break;
	}
	linear_interpolate( alpha, rat, normflag, &ep_list[epa],
	    &ep_list[epb], cur_ep );
	eptr = epa;
	while ( eptr != epb ) {
	    double	a, e, b;

	    switch ( edge ) {
	    case 0:
		a = ep_list[eptr].v;
		e = v;
		b = ep_list[ep_list[eptr].next].v;
		break;
	    case 1:
		a = ep_list[eptr].u;
		e = u;
		b = ep_list[ep_list[eptr].next].u;
		break;
	    case 2:
		a = ep_list[ep_list[eptr].next].v;
		e = v;
		b = ep_list[eptr].v;
		break;
	    case 3:
		a = ep_list[ep_list[eptr].next].u;
		e = u;
		b = ep_list[eptr].u;
		break;
	    default:
		break;
	    }
	    if (xin(a, b, e) ) {
		cur_ep->prev = eptr;
		cur_ep->next = ep_list[eptr].next;
		ep_list[ep_list[eptr].next].prev = tdata->ep_index;  
		ep_list[eptr].next = tdata->ep_index;  
		break; /* out of while */
	    }
	    eptr = ep_list[eptr].next;
	}
	cur_ep->branch = branch ? tdata->ep_index - 1 : tdata->ep_index + 1;
	++tdata->ep_index;
	status = Success;
    }
    
    return status;
}



/*++
 |
 |  Function Name:	traverse
 |
 |  Function Description:
 |
 |  Note(s):
 |
 --*/

static void
traverse( state, surface, output, ddSOFAS, el, winding )
    Nurb_surf_state		*state;
    miNurbSurfaceStruct		*surface;
    ddpex3rtn			(*output)();
    miSOFASStruct		*ddSOFAS;
    int				el;
    int				winding;
{
    int			inside, branch, forward;
    int			more = 1;
    Nurb_facet_op	start = NURB_NEW_FACET;

    register int		i, ep, epstart, ep_index;
    register Nurb_edge_point	*ep_list = state->trim_data.ep_list;

    ep = epstart = el;
    inside = winding & 1;
    /* Preprocess to mark edges. */
    do {
	if ( ep_list[ep].branch ) { 
	    ep_list[ep].flags = (inside) ? LAST : NEXT;
	    inside = !inside;
	}
	ep = ep_list[ep].next;
    } while ( ep != epstart );

    inside = winding & 1;
    branch = 0;
    forward = 1;
    /* Currently ep = epstart = el. */;
    do { /* for all loops */
	if ( start != NURB_NEW_FACET )
	    start = NURB_NEW_CONTOUR;
	do { /* for all points in loop */
	    ep_list[ep].flags |= VISITED;
	    if ( branch ) {		/* when on a branch */
		(*output)( state, surface, ddSOFAS, start, &ep_list[ep] );
		start = NURB_SAME_CONTOUR;
		if ( ep_list[ep].branch ) { /* branch hits edge */
		    branch = 0;
		    forward = (ep_list[ep].flags & NEXT);
		    ep = (forward) ? ep_list[ep].next : ep_list[ep].prev;
		} else { /* continue along branch */
		    ep = (forward) ? ep_list[ep].next : ep_list[ep].prev;
		}  
	    } else { /* when on the u,v quad boundary */
		if ( ep_list[ep].branch ) { /* edge hits branch */
		    (*output)( state, surface, ddSOFAS, start, &ep_list[ep] );
		    start = NURB_SAME_CONTOUR;
		    branch = 1;
		    if ( !inside ) {
			inside = 1;
			epstart = ep;
		    }
		    forward = (ep_list[ ep_list[ep].branch].prev == ep);
		    ep = ep_list[ep].branch;
		} else { /* continue along edge */
		    if ( inside ) {
			(*output)(state, surface, ddSOFAS, start, &ep_list[ep]);
			start = NURB_SAME_CONTOUR;
		    }
		    ep = (forward) ? ep_list[ep].next : ep_list[ep].prev;
		}
	    }
	} while ( ep != epstart );
	forward = 1;
	branch = 0;
	inside = 1;
	if ( more ) {
	    do { /* find next edge start pt */
		if ( !(ep_list[epstart].flags & VISITED) && 
		     (ep_list[epstart].flags & LAST) )
		     break;
		else
		    epstart = ep_list[epstart].next;
	    } while ( more = epstart != el );
	}
	/* Find next inside start pt. */
	if ( !more ) {
	    epstart = 0;
	    /* Note: the first ep_index starts at 5. */
	    ep_index = state->trim_data.ep_index;
	    for ( i = 5; i < ep_index; i++ ) {
		if ( !(ep_list[i].flags & VISITED) )
		    epstart = i;
	    }
	    branch = 1;
	}
    } while ( ep = epstart );
}



/* NEXT_TRIM_POINT gets next trim_pt and assign a bit pattern = <l b r t >
 *
 *  1001 | 0001 | 0011			9 | 1 | 3   
 * --------------------  	  u0  ------------
 *  1000 | 0000 | 0010    	        8 | 0 | 2
 * -------------------- 	  u1  ------------
 *  1100 | 0100 | 0110    	       12 | 4 | 6 
 *                          	          v0  v1     
 */
#define NEXT_TRIM_POINT( _idx, _ptrn, _u, _v ) \
  { \
    (_u) = trim_pts[_idx].u; (_v) = trim_pts[_idx].v; \
    if ( (_u) < u0 ) \
	(_ptrn) = (_v) < v0 ? 9 : (_v) <= v1 ? 1 : 3 ; \
    else if ( (_u) <= u1 ) \
	(_ptrn) = (_v) < v0 ? 8 : (_v) <= v1 ? 0 : 2 ; \
    else \
	(_ptrn) = (_v) < v0 ? 12 : (_v) <= v1 ? 4 : 6 ; \
  }

/* NOTE: These are different than the ones in the surface code. */
#define LL	1
#define LR	4
#define UR	3
#define UL	2

#define INSERT_PT( _u, _v, _ea, _eb, _ed, _br ) 		\
  ( insert_pt(  tdata, state->reps.normals, 			\
	        rat, (_u), (_v), (_ea), (_eb), (_ed), (_br) ) )

#define APPEND_PT( _u, _v, _ea, _eb, _ec, _ed ) 		\
  ( append_pt( tdata, state->reps.normals, 			\
	        rat, (_u), (_v), (_ea), (_eb), (_ec), (_ed) ) )




/*++
 |
 |  Function Name:	phg_nt_trim_rect
 |
 |  Function Description:
 |
 |	This routine takes as input a list of trim curves
 |	(each trim curve loop described as a linked list
 |	of tesselated line segments broken into montonic
 |	sections), and a rectangle in u,v space, and creates
 |	a SOFAS description of the intersection of the rectangle
 |	and the trim curve loops.
 |
 |  Note(s):
 |
 --*/

int
phg_nt_trim_rect( state, surface, rect, output, ddSOFAS )
    Nurb_surf_state		*state;
    miNurbSurfaceStruct		*surface;
    Nurb_edge_point		**rect;
    ddpex3rtn			(*output)();
    miSOFASStruct		*ddSOFAS;
{
    double		u0, u1, v0, v1;	/* rectangle in u,v space */
    double		itr;
    double		oldu, oldv, newu, newv;
    int			winding;
    int			old_out, new_out, tmp, index, first_pt;
    Nurb_param_limit	*ext;
    char		rat;

    register int		j;
    register Nurb_trim_data	*tdata = &state->trim_data;
    register Nurb_edge_point	*ep_list;
    register Nurb_trim_segment	*seg;
    register Nurb_param_point	*trim_pts = tdata->vertices;

    /* Note that 3D implies homogeneous for trim curves */
    rat = DD_IsVert3D(surface->points.type); 

    /* Make room for at least 4 points. */
    if ( tdata->ep_list_size < 5 )
	if ( grow_ep_list( tdata ) )
	    goto abort;

    /* Copy the rectangle into ep_list[1,2,3,4] */
    ep_list = tdata->ep_list;
    for ( j = 1; j < 5; j++, rect++ ) {  
	ep_list[j].u = (*rect)->u; 
	ep_list[j].v = (*rect)->v;
	ep_list[j].flags = 0; 
	ep_list[j].branch = 0;
	ep_list[j].pt.x = (*rect)->pt.x; 
	ep_list[j].pt.y = (*rect)->pt.y; 
	ep_list[j].pt.z = (*rect)->pt.z; 
	if ( rat )
	    ep_list[j].pt.w = (*rect)->pt.w; 
	if ( state->reps.normals ) {
	    ep_list[j].normal.x = (*rect)->normal.x; 
	    ep_list[j].normal.y = (*rect)->normal.y; 
	    ep_list[j].normal.z = (*rect)->normal.z; 
	}
	ep_list[j].next = j+1;     
	ep_list[j].prev = j-1;
    }
    /* Link the first and last elements */
    ep_list[1].prev = 4;
    ep_list[4].next = 1; 
    tdata->ep_index = 5;	/* elements 1 to 4 used */

    if ( tdata->nloops > 0 ) {
	winding = 0;
	u0 = ep_list[LL].u; v0 = ep_list[LL].v; 
	u1 = ep_list[UR].u; v1 = ep_list[UR].v; 
    } else
	winding = 1; 	/* if no trim curves, default winding = 1 */ 

    /* Intersect each trim loop against the rectangle bounds */
    for ( j = 0; j < tdata->nloops ; j++ ) {

	/* First, check against trim loop extent. */
	if ( (tdata->loops[j].extent.umin > u1) ||
	     (tdata->loops[j].extent.umax < u0) ||
	     (tdata->loops[j].extent.vmin > v1) ||
	     (tdata->loops[j].extent.vmax < v0) )
	  /* Cannot intersect rectangle */
	  continue;

	seg = tdata->loops[j].segs;
	if ( seg ) { /* first point of the loop */
	    NEXT_TRIM_POINT( seg->start, new_out, newu, newv );
	    first_pt = new_out ? 0 : tdata->ep_index;  
	}
	for ( ; seg; seg = seg->next ) {
	    if ( seg->start == 0 )
		continue;
	    ext = &seg->extent;
	    if ( ext->umin > u1 || ext->vmin > v1 || ext->umax < u0 )
		continue;
	    if ( ext->vmax < v0 ) {
		if ( ext->umin < u0 && ext->umax >= u0 )
		    ++ winding;
		continue;
	    }
	    index = seg->start;
	    NEXT_TRIM_POINT( index, new_out, newu, newv );
	    /* Note: first point is not inserted because trim_loop is closed */
	    while ( index != seg->end ) {
		oldu = newu;
		oldv = newv;
		old_out = new_out;
		++index;
		NEXT_TRIM_POINT( index, new_out, newu, newv );
		if ( tmp = (new_out & old_out) ) { /* in same region */
		    if ( (tmp & 010)
			&& (newu < u0 && oldu >= u0 || newu >= u0 && oldu < u0))
			++ winding;
		    continue;
		}
		if ( old_out ) {
		    /* Intersects top edge between a and b? */
		    if ( (old_out & 01) && xin( v0, v1,
			    itr = oldv+(u0-oldu)*(newv-oldv)/(newu-oldu)) ) {
			if ( INSERT_PT( u0, itr, LL, UL, 0, 0 ) )
			    goto abort;
		    } else { 
			if ( (old_out & 01) && itr < v0)
			    ++winding;
			/* Intersects right edge between b and c? */
			if ( (old_out & 02) && xin( u0, u1,
				itr = oldu+(v1-oldv)*(newu-oldu)/(newv-oldv))){
			    if ( INSERT_PT(itr, v1, UL, UR, 1, 0) )
				goto abort;
			/* Intersects bottom edge between c and d? */
			} else if ((old_out & 04) && xin(v0, v1,
				itr = oldv+(u1-oldu)*(newv-oldv)/(newu-oldu))){
			    if ( INSERT_PT( u1, itr, UR, LR, 2, 0 ) )
				goto abort;
			/* intersects left edge between d and a? */
		        } else if ((old_out & 010) && xin( u0, u1,
				itr = oldu+(v0-oldv)*(newu-oldu)/(newv-oldv))){
			    if ( INSERT_PT( itr, v0, LR, LL, 3, 0 ) )
				goto abort;
			}
		    }
		}

		if ( new_out == 0 ) {
		    if ( APPEND_PT( newu, newv, LL, UL, UR, LR ) )
			goto abort;
		} else {		
		    /* Intersect top edge? */
		    if (( new_out & 01) && xin( v0, v1,
			    itr = oldv+(u0-oldu)*(newv-oldv)/(newu-oldu))) {
			if ( INSERT_PT( u0, itr, LL, UL, 0, 1 ) )
			    goto abort;
		    } else {
			if ( (new_out & 01) && itr < v0 )
			    ++winding;
			/* Intersects right edge? */
			if ( (new_out & 02) &&	xin( u0, u1,
				itr = oldu+(v1-oldv)*(newu-oldu)/(newv-oldv))){
			    if ( INSERT_PT(itr, v1, UL, UR, 1, 1) )
				goto abort;
			/* Intersects bottom? */
		        } else if ( (new_out&04) && xin( v0, v1,
				itr = oldv+(u1-oldu)*(newv-oldv)/(newu-oldu))){
			    if ( INSERT_PT(u1, itr, UR, LR, 2, 1) )
				goto abort;
			/* Intersects left edge? */
		        } else if ( (new_out & 010) && xin( u0, u1,
				itr = oldu+(v0-oldv)*(newu-oldu)/(newv-oldv))){
			    if ( INSERT_PT(itr, v0, LR, LL, 3, 1) )
				goto abort;
			}
			/* Left region? */
		        if ( (new_out & 010) && newu >= u0
			    && trim_pts[seg->end].u < u0 ) 
			    ++winding;
		    }
		    break; /* Skip remaining points in segment. */
		}
	    } /* while ( not at end of segment ) */
	} /* for all segments */

	if ( first_pt ) {
	    index = tdata->ep_index - 1; /* back upto the last edge pt */
	    if ( tdata->ep_list[index].branch )
		tdata->ep_list[index].branch = first_pt;
	    else
		tdata->ep_list[index].next = first_pt;

	    if ( tdata->ep_list[first_pt].branch )
		tdata->ep_list[first_pt].branch = index;
	    else
		tdata->ep_list[first_pt].prev = index;
	}
    }

    /* Traverse net and generate polygons. */
    traverse( state, surface, output, ddSOFAS, LL, winding );

    return Success;

abort:
    return BadAlloc;
}



#ifdef NDEBUG

/*++
 |
 |  Function Name:	print_rect
 |
 |  Function Description:
 |
 |  Note(s):
 |
 --*/

static int
print_rect( rat, rect )
    char		rat;
    Nurb_edge_point	**rect;
{
    fprintf( stderr, "\n" );
    if ( rat ) {
	fprintf( stderr, "LL: ( %f, %f) ==> ( %f, %f, %f, %f)\n",
	    rect[LL-1]->u, rect[LL-1]->v,
	    rect[LL-1]->pt.x, rect[LL-1]->pt.y, rect[LL-1]->pt.z,
	    rect[LL-1]->pt.w );
	fprintf( stderr, "LR: ( %f, %f) ==> ( %f, %f, %f, %f)\n",
	    rect[LR-1]->u, rect[LR-1]->v,
	    rect[LR-1]->pt.x, rect[LR-1]->pt.y, rect[LR-1]->pt.z,
	    rect[LR-1]->pt.w );
	fprintf( stderr, "UR: ( %f, %f) ==> ( %f, %f, %f, %f)\n",
	    rect[UR-1]->u, rect[UR-1]->v,
	    rect[UR-1]->pt.x, rect[UR-1]->pt.y, rect[UR-1]->pt.z,
	    rect[UR-1]->pt.w );
	fprintf( stderr, "UL: ( %f, %f) ==> ( %f, %f, %f, %f)\n",
	    rect[UL-1]->u, rect[UL-1]->v,
	    rect[UL-1]->pt.x, rect[UL-1]->pt.y, rect[UL-1]->pt.z,
	    rect[UL-1]->pt.w );
    } else {
	fprintf( stderr, "LL: ( %f, %f) ==> ( %f, %f, %f)\n",
	    rect[LL-1]->u, rect[LL-1]->v,
	    rect[LL-1]->pt.x, rect[LL-1]->pt.y, rect[LL-1]->pt.z );
	fprintf( stderr, "LR: ( %f, %f) ==> ( %f, %f, %f)\n",
	    rect[LR-1]->u, rect[LR-1]->v,
	    rect[LR-1]->pt.x, rect[LR-1]->pt.y, rect[LR-1]->pt.z );
	fprintf( stderr, "UR: ( %f, %f) ==> ( %f, %f, %f)\n",
	    rect[UR-1]->u, rect[UR-1]->v,
	    rect[UR-1]->pt.x, rect[UR-1]->pt.y, rect[UR-1]->pt.z );
	fprintf( stderr, "UL: ( %f, %f) ==> ( %f, %f, %f)\n",
	    rect[UL-1]->u, rect[UL-1]->v,
	    rect[UL-1]->pt.x, rect[UL-1]->pt.y, rect[UL-1]->pt.z );
    }
}
#endif



#define IN_SAME_REGION( _oca, _ocb )	((_oca) & (_ocb))

#define MORE_TRIM_POINTS( _s ) \
    ((_s) && ((_s)->current != (_s)->last || (_s)->next))

#define NEXT_POINT( _pts, _s, _p, _v ) \
  { if ((_s)->current < 0) \
	(_s)->current = (_s)->first; \
    else if ((_s)->current != (_s)->last) \
	++(_s)->current; \
    else { \
	(_s) = (_s)->next; \
	if ( (_s) ) \
	    (_s)->current = (_s)->first; \
    } \
    if ( _s ) { \
	(_p).u = (_pts)[(_s)->current].u; \
	(_p).v = (_pts)[(_s)->current].v; \
	(_v) = (_s)->vis; \
    } \
  }

/* Outcodes */
#define IN	0
#define OUT_TL	3
#define OUT_TC	2
#define OUT_TR	6
#define OUT_CL	1
#define OUT_CR	4
#define OUT_BL	9
#define OUT_BC	8
#define OUT_BR	12

#define LEFT	OUT_CL
#define RIGHT	OUT_CR
#define TOP	OUT_TC
#define BOTTOM	OUT_BC

#define OUTCODE( _e, _p, _c ) \
  { if ( (_p).u < (_e).umin ) \
      (_c) = (_p).v < (_e).vmin ? OUT_BL : \
	  (_p).v <= (_e).vmax ? OUT_CL : OUT_TL; \
    else if ( (_p).u <= (_e).umax ) \
      (_c) = (_p).v < (_e).vmin ? OUT_BC : \
	  (_p).v <= (_e).vmax ? IN     : OUT_TC; \
    else \
      (_c) = (_p).v < (_e).vmin ? OUT_BR : \
	  (_p).v <= (_e).vmax ? OUT_CR : OUT_TR; \
  }

#define ADJACENT_RECT( _g, _e, _r ) \
    switch ( _e ) { \
	case LEFT: \
	    --(_r)[LL]; --(_r)[LR]; --(_r)[UL]; --(_r)[UR]; \
	    break; \
	case RIGHT: \
	    ++(_r)[LL]; ++(_r)[LR]; ++(_r)[UL]; ++(_r)[UR]; \
	    break; \
	case TOP: \
	    (_r)[LL]+=(_g)->nu; (_r)[LR]+=(_g)->nu; \
	    (_r)[UL]+=(_g)->nu; (_r)[UR]+=(_g)->nu; \
	    break; \
	case BOTTOM: \
	    (_r)[LL]-=(_g)->nu; (_r)[LR]-=(_g)->nu; \
	    (_r)[UL]-=(_g)->nu; (_r)[UR]-=(_g)->nu; \
	    break; \
    }

#define RECT_EXTENT( _r, _e ) \
  { \
   (_e).umin = (_r)[LL]->u; (_e).umax = (_r)[UR]->u; \
   (_e).vmin = (_r)[LL]->v; (_e).vmax = (_r)[UR]->v; \
  }

#define AT_EDGE( _g, _e, _p ) \
  (   ((_e) & LEFT   && (_p).u <= (_g)->extent.umin) \
   || ((_e) & RIGHT  && (_p).u >= (_g)->extent.umax) \
   || ((_e) & BOTTOM && (_p).v <= (_g)->extent.vmin) \
   || ((_e) & TOP    && (_p).v >= (_g)->extent.vmax))


/*++
 |
 |  Function Name:	intersect
 |
 |  Function Description:
 |
 |  Note(s):
 |
 --*/

static int
intersect( ext, old_out, new_out, old, new, inter )
    Nurb_param_limit	*ext;
    unsigned		old_out, new_out;
    Nurb_param_point	*old, *new, *inter;
{
    double	t;
    int		edge;

    if ( ((old_out == IN && new_out & OUT_CL) || (old_out & OUT_CL))
	&& xin( ext->vmin, ext->vmax,
	t = old->v + (ext->umin-old->u) * (new->v-old->v)/(new->u-old->u))){
	inter->u = ext->umin;
	inter->v = t;
	edge = LEFT;
    } else if ( ((old_out == IN && new_out & OUT_TC) || (old_out & OUT_TC))
	&& xin( ext->umin, ext->umax,
	t = old->u + (ext->vmax-old->v) * (new->u-old->u)/(new->v-old->v))){
	inter->u = t;
	inter->v = ext->vmax;
	edge = TOP;
    } else if ( ((old_out == IN && new_out & OUT_CR) || (old_out & OUT_CR))
	&& xin( ext->vmin, ext->vmax,
	t = old->v + (ext->umax-old->u) * (new->v-old->v)/(new->u-old->u))){
	inter->u = ext->umax;
	inter->v = t;
	edge = RIGHT;
    } else if ( ((old_out == IN && new_out & OUT_BC) || (old_out & OUT_BC))
	&& xin( ext->umin, ext->umax,
	t = old->u + (ext->vmin-old->v) * (new->u-old->u)/(new->v-old->v))){
	inter->u = t;
	inter->v = ext->vmin;
	edge = BOTTOM;
    } else
	edge = 0;

    return edge;
}



/*++
 |
 |  Function Name:	find_containing_rect
 |
 |  Function Description:
 |
 |  Note(s):
 |
 --*/

static void
find_containing_rect( pt, grid, rect )
    register Nurb_param_point	*pt;
    register Nurb_grid		*grid;
    Nurb_edge_point		*rect[5];
{
    register Nurb_edge_point	*gpt = &grid->pts[grid->nu+1];

    for ( ; gpt->u < pt->u; gpt++ )
	;
    for ( ; gpt->v < pt->v; gpt += grid->nu )
	;
    rect[UR] = gpt;
    rect[UL] = gpt - 1;
    rect[LL] = rect[UL] - grid->nu;
    rect[LR] = rect[UR] - grid->nu;
}



/*++
 |
 |  Function Name:	add_point
 |
 |  Function Description:
 |
 |  Note(s):
 |
 --*/

static void
add_point( state, surface, vert_list, op, pt, edge, rect )
    Nurb_surf_state	*state;
    miNurbSurfaceStruct	*surface;
    miListHeader	*vert_list;
    Nurb_path_op	op;
    Nurb_param_point	*pt;
    int			edge;
    Nurb_edge_point	*rect[5];
{
    Nurb_edge_point	new, *pa, *pb;
    double		alpha, beta;
    char		rat;

    rat = DD_IsVert4D(surface->points.type);

    new.u = pt->u; new.v = pt->v;
    if ( edge ) {
	switch ( edge ) {
	    case LEFT:
		alpha = (pt->v - rect[LL]->v)/(rect[UL]->v - rect[LL]->v);
		pa = rect[LL]; pb = rect[UL];
		break;
	    case RIGHT:
		alpha = (pt->v - rect[LR]->v)/(rect[UR]->v - rect[LR]->v);
		pa = rect[LR]; pb = rect[UR];
		break;
	    case TOP:
		alpha = (pt->u - rect[UL]->u)/(rect[UR]->u - rect[UL]->u);
		pa = rect[UL]; pb = rect[UR];
		break;
	    case BOTTOM:
		alpha = (pt->u - rect[LL]->u)/(rect[LR]->u - rect[LL]->u);
		pa = rect[LL]; pb = rect[LR];
		break;
	}
	linear_interpolate( alpha, rat, 0, pa, pb, &new );
    } else {
	alpha = (pt->v - rect[LL]->v)/(rect[UR]->v - rect[LL]->v);
	beta  = (pt->u - rect[LL]->u)/(rect[UR]->u - rect[LL]->u);
	bilinear( alpha, beta, rat, 0, rect[LL], rect[UL],
	    rect[UR], rect[LR], &new );
    }

    ADD_POINT_TO_LIST( vert_list, rat, op, &new.pt );

}



#define NPATH_OP( _o, _n ) \
    ((_o) ? ((_n) ? PT_LINE : PT_NOP) : ((_n) ? PT_MOVE : PT_NOP))

/*++
 |
 |  Function Name:	follow_segs
 |
 |  Function Description:
 |
 |  Note(s):
 |
 --*/

static Nurb_trim_segment *
follow_segs(state, surface, grid, tpts, seg, vert_list, start_pt,use_edge_flags)
    Nurb_surf_state	*state;
    miNurbSurfaceStruct	*surface;
    Nurb_grid		*grid;
    Nurb_param_point	*tpts;
    Nurb_trim_segment	*seg;
    miListHeader	*vert_list;
    Nurb_param_point	*start_pt;
    int			use_edge_flags;
{
    Nurb_edge_point	*rect[5];
    Nurb_param_limit	extent;
    unsigned		old_out, new_out;
    Nurb_param_point	old, new;
    int			edge, done = 0;
    ddULONG		new_vis, old_vis;
    Nurb_path_op	op;

    find_containing_rect( start_pt, grid, rect );

    RECT_EXTENT( rect, extent )

    op = use_edge_flags ? NPATH_OP( 0, seg->vis ) : PT_MOVE;
    if ( op != PT_NOP )
	add_point( state, surface, vert_list, op, start_pt, 0, rect );

    old = *start_pt;
    old_vis = seg->vis;
    NEXT_POINT( tpts, seg, new, new_vis )
    op = use_edge_flags ? NPATH_OP( old_vis, new_vis ) : PT_LINE;
    while ( !done ) {
	OUTCODE( extent, new, new_out )
	if ( new_out == IN ) {
	    if ( op != PT_NOP )
		add_point( state, surface, vert_list, op, &new, 0, rect );
	    old = new; old_vis = new_vis;
	    if ( MORE_TRIM_POINTS(seg) ) {
		NEXT_POINT( tpts, seg, new, new_vis )
		if ( use_edge_flags )
		    op = NPATH_OP( old_vis, new_vis );
	    } else
		done = 1;
	} else {
	    OUTCODE( extent, old, old_out )
	    edge = intersect( &extent, old_out, new_out, &old, &new, &old );
	    if ( op != PT_NOP )
		add_point( state, surface, vert_list, op, &old, edge, rect );
	    if ( !AT_EDGE( grid, edge, old ) ) {
		ADJACENT_RECT( grid, edge, rect )
		RECT_EXTENT( rect, extent )
	    } else  /* hit an edge of the grid */
		done = 1;
	}
    }

    /* Segments are monotonic so we're done with this one when an
     * edge is hit.
     */
    seg = seg->next;
    if ( seg )
	seg->current = -1;
    return seg;
}



/*++
 |
 |  Function Name:	phg_nt_draw_segs
 |
 |  Function Description:
 |
 |  Note(s):
 |
 --*/

int
phg_nt_draw_segs( state, surface, grid, seg, spts, use_edge_flags, vert_list )
    Nurb_surf_state		*state;
    miNurbSurfaceStruct		*surface;
    Nurb_grid			*grid;
    register Nurb_trim_segment	*seg;
    Nurb_param_point		*spts;
    int				use_edge_flags;
    miListHeader		*vert_list;
{
    unsigned		old_out, new_out;
    Nurb_param_point	old, new;
    Nurb_param_limit	*ext;
    ddULONG		vis;

    ext = &grid->extent;
    seg->current = -1;
    old_out = IN;
    for ( ; MORE_TRIM_POINTS(seg); old = new, old_out = new_out ) {
	NEXT_POINT( spts, seg, new, vis )
	OUTCODE( *ext, new, new_out )
	if ( new_out == IN ) {
	    if ( old_out ) {
		(void)intersect( ext, old_out, new_out, &old, &new, &new );
	    }
	    --seg->current;
	    seg = follow_segs( state, surface, grid, spts, seg, vert_list, 
			       &new, use_edge_flags );
	    old_out = IN;
	    continue;
	} else if ( old_out ) {
	    if ( IN_SAME_REGION(old_out, new_out) )
		continue;
	    if ( intersect( ext, old_out, new_out, &old, &new, &new )) {
		--seg->current;
		seg = follow_segs( state, surface, grid, spts, seg, vert_list, 
				   &new, use_edge_flags );
		old_out = IN;
		continue;
	    }
	}
    }
}

#define WS_NTRM_BOTTOM
