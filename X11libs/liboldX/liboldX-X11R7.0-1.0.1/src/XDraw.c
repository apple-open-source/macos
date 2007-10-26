/* $Xorg: XDraw.c,v 1.4 2001/02/09 02:04:05 xorgcvs Exp $ */

/*

Copyright 1987, 1998  The Open Group

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

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

*/
/* $XFree86: xc/lib/oldX/XDraw.c,v 1.4 2001/07/25 15:04:57 dawes Exp $ */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "X11/Xlibint.h"
#include "X11/Xlib.h"
#include "X10.h"

#define OK_RETURN 1
#define ERR_RETURN 0
#include <stddef.h>

/*

Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts.

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

*/

/*
 *
 * Vertices_converter: This internal routine takes a list of Vertices and
 *                     returns another list of Vertices such that the
 *                     following is true:
 *
 *         (1) No Vertex has the VertexRelative flag on
 *         (2) No Vertex has the VertexCurved flag on
 *         (3) The first Vertex has the VertexDontDraw flag on
 *         (4) The path that the return Vertex list specifies should
 *             be drawn on the screen is very close to the path that the
 *             Vertex list passed to vertices_converter specified should be
 *             drawn.  The difference is due solely to the approximation
 *             of curved segments in the original Vertex list by many
 *             small straight line segments that approximate the curve.
 *
 *    Notes:
 *
 *         (1) The first Vertex must have the VertexRelative flag
 *             turned off.  (This is not checked, just assumed)
 *         (2) VertexDontDraw is automatically turned on for the first
 *             Vertex because drawing to the first Vertex makes no sense.
 *         (3) This routine is used only by XDraw & XDrawFilled.
 *         (4) A 0 is returned iff no error occurs.
 *         (5) The passed Vertex list not changed in any way.
 *
 *    Implementation:
 *
 *         A routine from the qvss device dependent code library from the
 *         X version 10 sources written by DEC was used, abet slightly
 *         modified as it provided 99% of the desired functionality.
 *
 */

/*****************************************************************************/
/*                                                                          */
/*                  WRITTEN BY DARYL F. HAZEL      8509.06                  */
/*                                                                          */
/*  Modification history:						    */
/*									    */
/*  Carver 8510.23  Fixed first time allocation of the coord buffer	    */
/*		    to allocate max(200, pathcount) amount insted of just   */
/*		    200.  Potential Xserver crash problem.		    */
/*									    */
/*  Carver 8510.21  Replaced old module by module worked on by Ram Rao and  */
/*		    Bob Scheifler to get better performace.		    */
/*									    */
/*  Carver 8510.03  Changed the curve line converter to return the coord    */
/*		    path if a successor point does not exist for curve gen. */
/*                                                                          */
/*  Carver 8510.03  Fixed memory loss problem.  A coord path is only 	    */
/*		    allocated if the current path will not fit in it and    */
/*		    before the new coord path is allocated the old one is   */
/*                  freed.        					    */
/*                                                                          */
/*  Carver 8509.24  Fixed pointer/integer mismatch {*ppathaddr_new = ...}   */
/*                                                                          */
/*  Corkum 8509.18  Modify code to intergrate into the draw curve command   */
/*                                                                          */
/*                  Modified by Mark Lillibridge 5/20/87 to make into       */
/*		    X11 Xlib XDraw() support.                               */
/****************************************************************************/
							 
/*
 * Definitions:
 *
 */

typedef short   WORD;
typedef	short	*WORD_POINTER;

/*
 * Lookup tables, buffers used in the conversion:
 *
 */


/*
 * modified path list storage information:
 */
static Vertex *pathaddr_new = NULL;  /*pointer to current modified path list*/
static int pathlist_bytes = 0; /* size of modified path list in bytes       */
static int pathlist_segs = 0;/* size of modified path list in segments    */
static Vertex *pathaddr_coords = NULL; /*ptr to path list with coordinates */
static int path_coord_list_bytes = 0;  /* size of path list (w/coords) buff */
static int path_coord_list_segs = 0;   /* size of path list (w/coords) buff */

/*
 * THE TABLE OF PSEUDO FLOATING-POINT NUMBERS WHICH MAKE UP THE
 * MULTIPLICATIVE COEFFICIENTS FOR THE CUBIC SPLINE ALGORITHM.
 */
static unsigned short bntable[] = {
			    0xfc18, 0x7f97, 0x0476, 0xffde,
			    0xf8be, 0x7e5d, 0x0972, 0xff76,
			    0xf5f4, 0x7c56, 0x0eec, 0xfecc,
			    0xf3ba, 0x7988, 0x14da, 0xfde6,
			    0xf20b, 0x75fe, 0x1b2f, 0xfcca,
			    0xf0e2, 0x71c1, 0x21dd, 0xfb81,
			    0xf038, 0x6ce1, 0x28d3, 0xfa16,
			    0xf001, 0x676d, 0x3000, 0xf894,
			    0xf034, 0x6176, 0x3750, 0xf709,
			    0xf0c3, 0x5b0f, 0x3eae, 0xf583,
			    0xf1a1, 0x544b, 0x4605, 0xf410,
			    0xf2c0, 0x4d41, 0x4d41, 0xf2c0,
			    0xf410, 0x4605, 0x544b, 0xf1a1,
			    0xf583, 0x3eae, 0x5b0f, 0xf0c3,
			    0xf709, 0x3750, 0x6176, 0xf034,
			    0xf894, 0x3000, 0x676d, 0xf001,
			    0xfa16, 0x28d3, 0x6ce1, 0xf038,
			    0xfb81, 0x21dd, 0x71c1, 0xf0e2,
			    0xfcca, 0x1b2f, 0x75fe, 0xf20b,
			    0xfde6, 0x14da, 0x7988, 0xf3ba,
			    0xfecc, 0x0eec, 0x7c56, 0xf5f4,
			    0xff76, 0x0972, 0x7e5d, 0xf8be,
			    0xffde, 0x0476, 0x7f97, 0xfc18
			    };
			    
/*
 * this is the table which determines how many straight-line segments to draw
 * for a given curved segment.
 */
static short segtable[] = {  10,  3, 56,	/* pl <= 10, 3 segments     */
			     32,  4, 40,	/* pl <= 32, 4 segments     */
			     80,  6, 24,	/* pl <= 80, 6 segments     */
			     96,  8, 16,	/* pl <= 96, 8 segments     */
			     192, 12, 8,	/* pl <= 192, 12 segments   */
			     32767, 24, 00	/* pl <= 32767, 24 segments */
			   };
			     
/*
 *
 * The Real routine:
 *
 */
							 
static int vertices_converter(pathaddr, pathcount, ppathaddr_new, newpathcnt)
  Vertex  *pathaddr;	    /* address of original path list             */
  int	  pathcount;        /* number of points in original path list    */
  Vertex  **ppathaddr_new;  /* ptr to addr of new path list      */
  int     *newpathcnt;	    /* ptr to new number of pts in path list */
{
  int p0x, p0y;   	    /* coordinates of curve's predecessor point  */
  int p1x, p1y;   	    /* coordinates of curve's starting point     */
  int p2x, p2y;   	    /* coordinates of curve's ending point       */
  int p3x, p3y;   	    /* coordinates of curve's successor point    */
  int	flag;	    	    /* flag word of current end-point in p.list  */
  int successor_x = 0;	    /* X coordinate of curve's successor point   */
  int successor_y = 0;	    /* Y coordinate of curve's successor point   */
  int little_endian;	    /* stupid bntable is little-endian */
  WORD increment;	    /* bntable-ptr-increment value       */
  WORD diffx, diffy;	    /* absolute values of x, y end-point diffs   */
  WORD lmajoraxis;	    /* "length" of seg projected onto major axis */
  union		/* accumulator variables for computing end-points... */
    {	    	/* ...using the bntable.                             */
      struct
	{
	  WORD low;
	  WORD high;
	} sword;
      int lword;
    } xxe, yye;
  register Vertex *pnewpath;/* pointer for traversing modified path list */
  int	    newpathcount;   /* number of end-points in modified path list*/
  register Vertex *pflag;   /* pointer used for looking for fig's last pt*/
  int	    i;	    	    /* general-purpose loop variable             */
  int	    ecf_flag_set;   /* flag indicating existence of set ECF flag */
  int       count;
  int	    curve_flag;	    /* 0 = no curves in path; 1 = curves in path */
  
  /*
   * The following is necessary because of the pseudo-floating point
   * calculations done down below.
   */
  i = 1;
  if (*(char *) &i)
    little_endian = 1;
  else
    little_endian = 0;

  {
    register Vertex *poldpath;/* pointer for traversing original path list */
    register Vertex *p_coord_path; /* ptr to path list with coordinates    */

    /* Init the return path count to 0 in case of error */
    *newpathcnt = 0;
    newpathcount = 0;
    
    /* Use path buffers from last time */
    pnewpath = pathaddr_new;
    p_coord_path = pathaddr_coords;
    
    /*    
     * if size of coordinate path list buffer is not at least large enough to
     * accommodate the specified path list, allocate enough memory to do so
     */
    if (path_coord_list_segs == 0) 		/* first time through*/
      {
	path_coord_list_segs = max (200, pathcount);
	path_coord_list_bytes = path_coord_list_segs * sizeof(Vertex);
	if ((pathaddr_coords = (Vertex *)Xmalloc(path_coord_list_bytes)) != 0)
	  p_coord_path = pathaddr_coords;
	else
	  return(ERR_RETURN);
      }
    else    	    	    	    	/* all but first time through       */
      {
	/* If coord buffer is large enough for this path then reuse it, other
	   wise free the old one and get a new one (Carver) 8510.03 */
	if (path_coord_list_segs < pathcount)
	  {
	    Xfree( pathaddr_coords);
	    path_coord_list_segs = pathcount;
	    path_coord_list_bytes = path_coord_list_segs * sizeof(Vertex);
	    if ((pathaddr_coords =(Vertex *)Xmalloc(path_coord_list_bytes)) !=0)
	      p_coord_path = pathaddr_coords;
	    else
	      return(ERR_RETURN);
	  };
      }

    /*
     * Copy given Vertex list (pathaddr) to coordinate buffer converting
     * relative coordinates to absolute coordinates.
     *
     * Sets curve_flag iff at least one Vertex has the VertexCurved flag set.
     * 
     */
    
    poldpath = pathaddr;
    curve_flag = 0;
    
    *p_coord_path = *poldpath++;       /* First Vertex is a special case */
    p_coord_path->flags = (p_coord_path->flags | VertexDontDraw)
                          & ~VertexRelative;
    if ((p_coord_path++)->flags & VertexCurved)
      curve_flag = 1;
    for (i = pathcount - 1; i > 0; i--)
      {
	if (poldpath->flags & VertexRelative) 
	  {                           /* compute coordinates using last pt */
	    p_coord_path->x = poldpath->x + p_coord_path[-1].x;
	    p_coord_path->y = poldpath->y + p_coord_path[-1].y;
	    p_coord_path->flags = poldpath->flags & ~VertexRelative;
	  }
    	else *p_coord_path = *poldpath;
	p_coord_path++;
    	if ((poldpath++)->flags & VertexCurved)
	  curve_flag = 1;
      }	    
  }        /* End of scope for poldaddr, p_coord_path */

  /*
   * if it has been determined that there are no curved segment end points in
   * the specified path list, return the path list containing the coordinates
   * to the calling routine.
   */
  if (!curve_flag)
    {
      *newpathcnt    = pathcount;
      *ppathaddr_new = pathaddr_coords;
      return(OK_RETURN);
    }
  
  /*
   * if size of modified path list buffer is not at least large enough to
   * accommodate the specified path list allocate enough memory to do so
   */
  pathaddr = pathaddr_coords;
  if (pathlist_segs < pathcount)
    {
      if (pathlist_segs == 0)           /* Fix storage leak -- MDL 5/20/87 */
	Xfree( pathaddr_new );
      pathlist_segs = pathcount;
      pathlist_bytes = pathcount * sizeof(Vertex);
      if ((pathaddr_new = (Vertex *)Xmalloc(pathlist_bytes)) != 0)
	pnewpath = pathaddr_new;
      else
	return(ERR_RETURN);
    }

  /*
   * initialize the beginning and ending coordinates of the first segment
   */
  p1x = 0;
  p1y = 0;
  p2x = 0;
  p2y = 0;
  
  {
    register WORD *pbntable;	/* table of multiplicative coeffics.*/
    register WORD *psegtable;	/* table used to detrmn num.sub-segs*/
    register WORD m;	    	/* num segments into which curve is divided */

    /*
     * MAIN LOOP OF THE PATH_LIST_CONVERTER ROUTINE
     */
    for ( count=pathcount ; count > 0; count--)
      {
	p0x = p1x;    /* save previous values of path-list coordinate pairs*/
	p0y = p1y;
	p1x = p2x;
	p1y = p2y;
	
	/*
    	 * read next end-point's coordinates from the path list
	 */
	p2x = pathaddr->x;
	p2y = pathaddr->y;
	flag = (pathaddr++)->flags;
	
	/*
	 * CURVED-segment considerations
	 */
	if (flag & VertexCurved)
	  {
	    /*
	     * determine which point to use as the successor point:  the next
	     * point in the list (if there is one), or a previously-saved point
	     * (when drawing closed figures)
	     */
	    if (flag & VertexEndClosed)	/* last segment of closed fig*/
	      {
		p3x = successor_x;
		p3y = successor_y;
	      }
    	    else    	   	    	/* stand-alone curved segment*/
	      {
		if (count <= 1) 	/* no points to use as successor pt */
		  {
		    
		    /* in this case draw the coord path */
		    *newpathcnt    = pathcount;
		    *ppathaddr_new = pathaddr_coords;
		    return(OK_RETURN);
		  }
		else
		  {
		    /*
    	    	     * read next end-point's coordinates from the path list...
	    	     */
	    	    p3x = pathaddr->x;
	    	    p3y = pathaddr->y;
		    
	    	    if (flag & VertexStartClosed)
		      {      /* first segment of closed figure */
			/*
			 * save P3 as successor pt for closed-fig-ending curve
			 */
			successor_x = p3x;
			successor_y = p3y;
			
			/*
			 * Traverse the original path list looking for ECF flag
    	    	    	 * After finding it, back pointer up to previous point
			 * and save the information for use as curve's pred. pt
			 */
    	    	    	ecf_flag_set = 0;
			pflag = pathaddr;
			for (i = count; i > 0; i--)
			  {
			    if (pflag->flags & VertexEndClosed)
			      {
				pflag--;
	    	    	    	p1x = pflag->x;
	    	    	    	p1y = pflag->y;
    	    	    	    	ecf_flag_set = 1;
				break;
			      }
			    pflag++;
			  }
    	    	    	if (!ecf_flag_set)
			  return(ERR_RETURN);
		      }     /* end code pertaining to starting figure*/
		  }	    /* end code pertaining to successor points   */
	      }   	    /* end code pertaining to stand-alone curved segs*/
	    
	    if ((flag & VertexDontDraw) == 0)
	      {
		/*
	    	 * determine the "length" of the segment along the major axis
	    	 */
	    	if ((diffx = p2x - p1x) < 0)
		  diffx = ~diffx + 1;
	    	if ((diffy = p2y - p1y) < 0)
		  diffy = ~diffy + 1;
	    	lmajoraxis = (diffx >= diffy) ? diffx : diffy;
	    	if (lmajoraxis == 0)	/* for vector to have length of...  */
		  lmajoraxis = 1;	/* ...at least one                  */
		
		/*
	    	 * compute M, the number of sub-segments into which a curved 
		 * segment is divided
		 */
	    	psegtable = segtable;
	    	while (lmajoraxis > *psegtable++)/* search for appropriate..*/
		  psegtable += 2;		 /* ...table entry          */
	    	m = *psegtable++;		 /* read number of segments */
	    	increment = *psegtable >> 1;
		
		/*
    	    	 * determine if there is enough room remaining in the modified
		 * path-list buffer to hold ALL of the curve's sub-segment in-
		 * formation; if not, double the buffer size (if possible).
		 */
		while ((newpathcount + m) > pathlist_segs)
		  {
		    pathlist_segs *= 2;
		    pathlist_bytes *= 2;
		    if ((pathaddr_new = (Vertex *)Xrealloc(pathaddr_new, 
							 pathlist_bytes)) != 0)
		      pnewpath = pathaddr_new + newpathcount;
	    	    else
		      return(ERR_RETURN);
		  }
		
		/*
	    	 * generate end-points of sub-segs into which curve is divided
		 */
	    	pbntable = (WORD *) bntable; /* initialize the pointer to the Bn table*/
	    	for ( ; m > 1; m--)
		  {
		    pbntable += increment;
		    xxe.lword = *pbntable * p0x;
		    yye.lword = *pbntable++ * p0y;
		    xxe.lword += *pbntable * p1x;
		    yye.lword += *pbntable++ * p1y;
		    xxe.lword += *pbntable * p2x;
		    yye.lword += *pbntable++ * p2y;
		    xxe.lword += *pbntable * p3x;
		    yye.lword += *pbntable++ * p3y;
		    xxe.lword = xxe.lword << 1;	/* double values...  */
		    yye.lword = yye.lword << 1;	/* ...bn accts for it*/
		    xxe.lword += 0x8000; /* round off the accum value */
		    yye.lword += 0x8000;
		    if (little_endian) {
			pnewpath->x = xxe.sword.high;/* the X coordinate  */
			pnewpath->y = yye.sword.high;/* the Y coordinate  */
		    } else {
			pnewpath->x = xxe.sword.low;/* the X coordinate  */
			pnewpath->y = yye.sword.low;/* the Y coordinate  */
		    }
		    (pnewpath++)->flags = 0;	/* the flag word     */
    	    	    newpathcount++;/* increment segment end-point countr*/
		  }	    	   /* end sub-segment end-point compute loop*/
	      }   	   /* end DRAW-segment considerations code      */
	  }	    	   /* end CURVED-segment considerations code        */
	
	/*
    	 * put end-point from original path list into modified list
	 */
    	pnewpath->x = p2x;
	pnewpath->y = p2y;
	(pnewpath++)->flags = flag & ~VertexCurved;
	newpathcount++;
	
	/*
    	 * determine whether or not the modified path list is full, and if so,
	 * double its size
	 */
	if (newpathcount == pathlist_segs)
	  {
	    pathlist_bytes *= 2;
	    pathlist_segs *= 2;
	    if ((pathaddr_new = (Vertex *)Xrealloc(pathaddr_new, 
						 pathlist_bytes)) != 0)
	      pnewpath = pathaddr_new + newpathcount;
    	    else
	      return(ERR_RETURN);
	  }
      }	    	    /* END of PATH_LIST_CONVERTER's main loop        */
    
    /*
     * return the address of the modified path list and the number of segment
     * end-points it contains
     */
    *newpathcnt    = newpathcount;
    *ppathaddr_new = pathaddr_new;
  }    

  return(OK_RETURN);
}

/* Written by Mark Lillibridge */

static XPoint *XDraw_points = NULL;    /* Buffer to hold list of points for */
static int    point_count = 0;         /* use in calling XDrawLines         */

Status XDraw (dpy, d, gc, vlist, vcount)
    register Display *dpy;
    Drawable d;
    GC gc;
    Vertex *vlist;
    int vcount;
{
  Vertex *newvlist;
  int newvcount;
  XPoint *p;
  int pcount;

  /* If less than 2 vertices, we don't have to do anything (no lines) */
  if (vcount<2)
    return(OK_RETURN);

  /* Convert curved lines to straight lines & change relative references to */
  /* absolute references. */
  if (!vertices_converter( vlist, vcount, &newvlist, &newvcount))
    return(ERR_RETURN);

  /* Insure we have enough buffer space */
  if (point_count < newvcount) {
    if (point_count != 0)
      Xfree( XDraw_points );
    if (!(XDraw_points = (XPoint *) Xmalloc( newvcount * sizeof(XPoint) )))
      return(ERR_RETURN);
    point_count = newvcount;
  }

  /*
   * Draw the lines defined by newvlist using seperate XDrawLines calls
   * to insure that all the lines that should be joined are and that closed
   * figures are joined correctly.
   */
  while (newvcount>0) {
    p = XDraw_points;               /* Put points in buffer */
    pcount = 0;
    p->x = newvlist->x;             /* Copy first point */
    (p++)->y = (newvlist++)->y;
    newvcount--; pcount++;

    /*
     * Copy more points until out of points or need to stop XDrawLine
     * because either we don't want to join to the next point
     * (VertexDontDraw) or we want to stop after the next point so that
     * a closed figure will be joined correctly.  (We have to stop before
     * a VertexStartClosed because the vertex marked VertexStartClosed
     * must be the first vertex in its XDrawLines call to get joining
     * to work correctly.
     */
    while (newvcount > 0 && !(newvlist->flags & (VertexDontDraw |
						 VertexStartClosed |
						 VertexEndClosed))) {
      p->x = newvlist->x;
      (p++)->y = (newvlist++)->y;
      newvcount--; pcount++;
    }

    /*
     * If stopped only because of need to start a new XDrawLines, copy
     * next point but don't advance pointer so two XdrawLines act like one.
     */
    if ( newvcount > 0 && !(newvlist->flags & VertexDontDraw) ) {
      p->x = newvlist->x;
      (p++)->y = newvlist->y;
      pcount++;
    }

    /* Do the XDrawLines if there are any lines to draw */
    if (pcount>1)
      XDrawLines(dpy, d, gc, XDraw_points, pcount, CoordModeOrigin);
  }

  return(OK_RETURN);
}

Status XDrawFilled (dpy, d, gc, vlist, vcount)
    register Display *dpy;
    Drawable d;
    GC gc;
    Vertex *vlist;
    int vcount;
{
  Vertex *newvlist;
  int newvcount;
  XPoint *p;
  int pcount;

  /* If less than 2 vertices, we don't have to do anything (no lines) */
  if (vcount<2)
    return(OK_RETURN);

  /* Convert curved lines to straight lines & change relative references to */
  /* absolute references. */
  if (!vertices_converter( vlist, vcount, &newvlist, &newvcount))
    return(ERR_RETURN);

  /* Insure we have enough buffer space */
  if (point_count < newvcount) {
    if (point_count != 0)
      Xfree( XDraw_points );
    if (!(XDraw_points = (XPoint *) Xmalloc( newvcount * sizeof(XPoint) )))
      return(ERR_RETURN);
    point_count = newvcount;
  }

  /*
   * Draw the lines defined by newvlist using seperate XDrawLines calls
   * to insure that all the lines that should be joined are and that closed
   * figures are joined correctly.
   */
  while (newvcount>0) {
    p = XDraw_points;               /* Put points in buffer */
    pcount = 0;
    p->x = newvlist->x;             /* Copy first point */
    (p++)->y = (newvlist++)->y;
    newvcount--; pcount++;

    /*
     * Copy more points until out of points or need to stop XDrawLine
     * because either we don't want to join to the next point
     * (VertexDontDraw) or we want to stop after the next point so that
     * a closed figure will be joined correctly.  (We have to stop before
     * a VertexStartClosed because the vertex marked VertexStartClosed
     * must be the first vertex in its XDrawLines call to get joining
     * to work correctly.
     */
    while (newvcount > 0 && !(newvlist->flags & (VertexStartClosed |
						 VertexEndClosed))) {
      p->x = newvlist->x;
      (p++)->y = (newvlist++)->y;
      newvcount--; pcount++;
    }

    /*
     * If stopped only because of need to start a new XDrawLines, copy
     * next point but don't advance pointer so two XdrawLines act like one.
     */
    if ( newvcount > 0 ) {
      p->x = newvlist->x;
      (p++)->y = newvlist->y;
      pcount++;
    }

    /* Do the XDrawLines if there are any lines to draw */
    if (pcount>1) {
	  XFillPolygon(dpy, d, gc, XDraw_points, pcount, Complex,
		       CoordModeOrigin);
    }
  }

  return(OK_RETURN);
}
