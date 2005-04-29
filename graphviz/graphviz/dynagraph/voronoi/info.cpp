/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include "voronoi/info.h"

namespace Voronoi {


/* compare:
 * returns -1 if p < q
 *          0 if p = q
 *          1 if p > q
 * if q if NULL, returns -1
 * Ordering is by angle from -pi/2 to 3pi/4.
 * For equal angles (which should not happen in our context)
 * ordering is by closeness to origin.
 */
inline int compare (Coord* o, PtItem* p, PtItem* q) {
    double x0;
    double y0;
    double x1;
    double y1;
    double a, b;

    if (q == NULL) return -1;
    if ((p->p.x == q->p.x) && (p->p.y == q->p.y)) return 0;

    x0 = ((double)(p->p.x)) - ((double)(o->x));
    y0 = ((double)(p->p.y)) - ((double)(o->y));
    x1 = ((double)(q->p.x)) - ((double)(o->x));
    y1 = ((double)(q->p.y)) - ((double)(o->y));
    if (x0 >= 0.0) {
      if (x1 < 0.0) return -1;
      else if (x0 > 0.0) {
        if (x1 > 0.0) {
          a = y1/x1;
          b = y0/x0;
          if (b < a) return -1;
          else if (b > a) return 1;
          else if (x0 < x1) return -1;
          else return 1;
        }
        else {  /* x1 == 0.0 */
          if (y1 > 0.0) return -1;
          else return 1;
        }
      }
      else {        /* x0 == 0.0 */
        if (x1 > 0.0) {
          if (y0 <= 0.0) return -1;
          else return 1;
        }
        else {    /* x1 == 0.0 */
          if (y0 < y1) {
            if (y1 <= 0.0) return 1;
            else return -1;
          }
          else {
            if (y0 <= 0.0) return -1;
            else return 1;
          }
        }
      }
    }
    else {
      if (x1 >= 0.0) return 1;
      else {
          a = y1/x1;
          b = y0/x0;
          if (b < a) return -1;
          else if (b > a) return 1;
          else if (x0 > x1) return -1;
          else return 1;
      }
    }
}

  /*
static void
printV (PtItem *vp)
{
    if (vp == NULL) {
       fprintf (stderr, "<empty>\n");
       return;
    }

    while (vp != NULL) {
       fprintf (stderr, "(%.16f,%.16f)\n", vp->p.x, vp->p.y);
       vp = vp->next;
    }
}

static void
error (Info* ip, Site* s, double x, double y)
{
    fprintf (stderr, "Unsorted vertex list for site %d (%.16f,%.16f), pt (%f,%f)\n",
        s->sitenbr, s->coord.x, s->coord.y, x, y);
    printV (ip->verts);
}

static int
sorted (Coord* origin, PtItem* vp)
{
    PtItem*  next;

    if (vp == NULL) return 1;
    next = vp->next;

    while(next != NULL) {
      if (compare(origin,vp,next) <= 0) {
         vp = next;
         next = next->next;
      }
      else {
          fprintf (stderr, "(%.16f,%.16f) > (%.16f,%.16f)\n",
            vp->p.x, vp->p.y, next->p.x, next->p.y);
          return 0;
      }
    }

    return 1;
    
}
*/
void Infos::addVertex (Site* s, Coord c) {
    Info*   ip;
    PtItem*   p;
    PtItem*   curr;
    PtItem*   prev;
    Coord*    origin = &(s->coord);
    PtItem    tmp;
    int       cmp;

/* fprintf (stderr, "addVertex (%d, %f, %f)\n", s->sitenbr, x, y); */
    ip = &nodes[s->sitenbr];
    curr = ip->verts;

    tmp.p = c;

    cmp = compare(origin, &tmp, curr);
    if (cmp == 0) return;
    else if (cmp < 0) {
        p = fpoints.alloc();
        p->p = c;
        p->next = curr;
        ip->verts = p;
        return;
    }

    prev = curr;
    curr = curr->next;
    while ((cmp = compare(origin, &tmp, curr)) > 0) {
        prev = curr;
        curr = curr->next;
    }
    if (cmp == 0) return;
    p = fpoints.alloc();
    p->p = c;
    prev->next = p;
    p->next = curr;

    /* This test should be unnecessary */
      /* if (!sorted(origin,ip->verts))  */
        /* error (ip,s,x,y); */

}

} // namespace Voronoi
