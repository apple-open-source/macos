/* $Xorg: miNurbs.h,v 1.4 2001/02/09 02:04:08 xorgcvs Exp $ */

/*****************************************************************

Copyright 1989,1990, 1991, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

Copyright 1989,1990, 1991 by Sun Microsystems, Inc.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the names of Sun Microsystems,
and The Open Group, not be used in advertising or publicity 
pertaining to distribution of the software without specific, written 
prior permission.  

SUN MICROSYSTEMS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, 
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT 
SHALL SUN MICROSYSTEMS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL 
DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/

#ifndef MI_NURB_H
#define MI_NURB_H 1

#ifdef NDEBUG
extern unsigned		nurb_debug_flags;
#define NURB_DEBUG_FLAG(_b) (nurb_debug_flags & (_b))
#endif /* NDEBUG */

/* Nurb Debug Flags:
 *	0x01 (1)	Surface edge paths
 *	0x02 (2)	Surface iso curve paths
 *	0x04 (4)	Trimming data, tessellated and ordered
 *	0x08 (8)	Surface facet paths for untrimmed surfaces
 *	0x10 (16)	Trimmed surface polygons
 *	0x20 (32)	Surface facet paths
 *	0x40 (64)	Surface polygons before trimming
 *	0x80 (128)	Surface hollow-edge paths
 *	0x100 (256)	Surface grids
 *	0x200 (512)	Initial data
 */

#define MAXORD		10
#define MAXTCORD	6

#define	XX	0
#define	YY	1
#define	ZZ	2
#define	WW	3

#define MAX(a,b)	( ((a) > (b)) ? (a) : (b) )

extern double	mi_nu_ptofd[MAXORD][MAXORD];

#define NURB_TRIM_DATA_INIT( _t ) \
  { \
    (_t).nloops = 0; \
    (_t).loops = (Nurb_trim_loop_rep *)NULL; \
    (_t).cur_vertex = 1; \
    (_t).vertices = (Nurb_param_point *)NULL; \
    (_t).ep_index = 0; \
    (_t).ep_list_size = 0; \
    (_t).ep_list = (Nurb_edge_point *)NULL; \
  }

#define NURB_SURF_STATE_INIT( _s ) \
  { \
    (_s)->reps.facets = 0; \
    (_s)->reps.edges = 0; \
    (_s)->reps.isocrvs = 0; \
    (_s)->reps.markers = 0; \
    (_s)->reps.hollow = 0; \
    (_s)->reps.grids = 0; \
    (_s)->reps.normals = 0; \
    (_s)->reps.trim_data = 0; \
    (_s)->grids.number = 0; \
    (_s)->grids.flags.normals = 0; \
    (_s)->grids.grids = (Nurb_grid *)NULL; \
    (_s)->ruknots = (ddFLOAT *)NULL; \
    (_s)->rvknots = (ddFLOAT *)NULL; \
    (_s)->facets = (miListHeader *)NULL; \
    (_s)->sofas = (miSOFASStruct *)NULL; \
    (_s)->edges = (miListHeader *)NULL; \
    (_s)->isocrvs = (miListHeader *)NULL; \
    (_s)->markers = (miListHeader *)NULL; \
    (_s)->hollow = (miListHeader *)NULL; \
    NURB_TRIM_DATA_INIT((_s)->trim_data); \
  }

#define NURB_INIT_RANGE_LIST( _r ) \
    (_r)->size = 0; \
    (_r)->number = 0; \
    (_r)->limits = (Nurb_limit *)NULL;

#define EXTENTS_OVERLAP( _ea, _eb ) \
  (!((_eb).umin > (_ea).umax || (_eb).umax < (_ea).umin || \
   (_eb).vmin > (_ea).vmax || (_eb).vmax < (_ea).vmin))

#define GET_TRIM_CURVE_TOLERANCE( _crv, _tolerance )    	\
	switch ( (_crv)->curveApprox.approxMethod ) {   	\
	  case PEXApproxImpDep: 				\
	  case PEXApproxConstantBetweenKnots:			\
		(_tolerance) = (_crv)->curveApprox.tolerance;	\
		break;						\
	  default: 						\
		(_tolerance) = 1.0;				\
	}
 
#define ADD_POINT_TO_LIST( _l, _r, _op, _pt )				\
if ((_l)) {								\
  listofddPoint *pddlist;						\
  if ( (_op) == PT_MOVE ) {						\
    (_l)->numLists++;							\
    MI_ALLOCLISTHEADER( (_l), 						\
			MI_ROUND_LISTHEADERCOUNT( (_l)->numLists ) );	\
  }									\
  pddlist = &((_l)->ddList[(_l)->numLists-1]);				\
  pddlist->numPoints++;							\
  if ( (_r) ) {								\
    MI_ALLOCLISTOFDDPOINT( pddlist,					\
			   MI_ROUND_LISTHEADERCOUNT(pddlist->numPoints),\
			   sizeof(ddCoord4D) );				\
    pddlist->pts.p4Dpt[pddlist->numPoints - 1] = *((ddCoord4D *)(_pt));	\
  } else {								\
    MI_ALLOCLISTOFDDPOINT( pddlist,					\
			   MI_ROUND_LISTHEADERCOUNT(pddlist->numPoints),\
			   sizeof(ddCoord3D) );				\
    pddlist->pts.p3Dpt[pddlist->numPoints - 1] = *((ddCoord3D *)(_pt));	\
  }									\
}

typedef enum {
    PT_NOP,
    PT_MOVE,
    PT_LINE,
    PT_MARKER
} Nurb_path_op;

typedef enum {
    NURB_SAME_CONTOUR = 0,
    NURB_NEW_CONTOUR = 1,
    NURB_NEW_FACET = 2
} Nurb_facet_op;

typedef struct {
    unsigned facets:	1;
    unsigned isocrvs:	1;
    unsigned edges:	1;
    unsigned markers:	1;	/* first order in u and v, use markers */
    unsigned hollow:	1;
    unsigned grids:	1;
    unsigned normals:	1;
    unsigned trim_data:	1;
} Nurb_rep_flags;

typedef struct {
    double      lmin;
    double      lmax;
} Nurb_limit;

typedef struct {
    int        size;
    int        number;
    Nurb_limit  *limits;
} Nurb_limitlst;

typedef struct {
    double      u;
    double      v;
} Nurb_param_point;

typedef struct {
    double      umin, umax;
    double      vmin, vmax;
} Nurb_param_limit;

typedef struct Nurb_trim_segment {
    int				first, last;    /* indices in vertex list */
    int				start, end;     /* effective limit of seg */
    int				current;
    unsigned			dir;
    ddULONG			vis;
    Nurb_param_limit		extent;         /* of segment */
    struct Nurb_trim_segment	*next;
} Nurb_trim_segment;

typedef struct {
    Nurb_param_limit    extent; /* of loop */
    Nurb_trim_segment   *segs;  /* linked list of segments, NULL terminated */
} Nurb_trim_loop_rep;

typedef struct {
    int		flags;
    int		count;	/* of rectangles this point is part of */
    double	u, v;
    ddCoord4D	pt;
    ddVector3D	normal;
    int		next, prev, branch;
} Nurb_edge_point;

typedef struct {
    Nurb_param_point		*vertices;      /* of tessellated curves */
    int				cur_vertex;     /* index of current vertex */
    int				nloops;
    Nurb_trim_loop_rep		*loops;         /* array of representations */
    Nurb_edge_point		*ep_list;       /* polygon edge points */
    int				ep_index;       /* index of current one */
    int				ep_list_size;   /* # allocated entries */
} Nurb_trim_data;

typedef struct {
    Nurb_edge_point	*pts;
    int			nu, nv;
    Nurb_param_limit	extent;
} Nurb_grid;

typedef struct {
    int         number;
    struct {
        unsigned        normals;
    }           flags;
    Nurb_grid           *grids;
} Nurb_gridlst;

typedef struct {
    int			gitype;
    int			isocount[2];
    int			approx_type;
    ddFLOAT		approx_value[2];
    Nurb_param_limit	range;
    Nurb_param_limit	param_limits;
    Nurb_rep_flags	reps;
    ddFLOAT		*ruknots;
    ddFLOAT		*rvknots;
    Nurb_gridlst	grids;
    Nurb_trim_data	trim_data;
    Nurb_param_point	corners[5];
    Nurb_trim_segment	edge_segs[4];
    miListHeader	*facets;
    miSOFASStruct	*sofas;
    miListHeader	*edges;
    miListHeader	*isocrvs;
    miListHeader	*markers;
    miListHeader	*hollow;
} Nurb_surf_state;

#endif /* MI_NURB_H */
