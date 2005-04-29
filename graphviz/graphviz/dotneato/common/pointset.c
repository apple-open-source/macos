/*
    This software may only be used by you under license from AT&T Corp.
    ("AT&T").  A copy of AT&T's Source Code Agreement is available at
    AT&T's Internet website having the URL:
    <http://www.research.att.com/sw/tools/graphviz/license/source.html>
    If you received this software without first entering into a license
    with AT&T, you have an infringing copy of this software and cannot use
    it without violating AT&T's intellectual property rights.
*/

#include <stdio.h>
#include <pointset.h>

typedef struct {
  Dtlink_t  link;
  point     id;
} pair;

static pair*
mkPair (point p)
{
  pair* pp;

  pp = NEW(pair);
  pp->id = p;
  return pp;
}

static int
cmppair(Dt_t* d, point* key1, point* key2, Dtdisc_t* disc)
{
  if (key1->x > key2->x) return 1;
  else if (key1->x < key2->x) return -1;
  else if (key1->y > key2->y) return 1;
  else if (key1->y < key2->y) return -1;
  else return 0;
}

Dtdisc_t intPairDisc = {
    offsetof(pair,id),
    sizeof(point),
    offsetof(pair,link),
    0,
    0,
    (Dtcompar_f)cmppair,
    0,
    0,
    0
};

PointSet* 
newPS ()
{
  return (dtopen (&intPairDisc, Dtoset));
}

void      
freePS (PointSet* ps)
{
  dtclose(ps);
}

void      
insertPS (PointSet* ps, point pt)
{
  dtinsert (ps, mkPair(pt));
}

void      
addPS (PointSet* ps, int x, int y)
{
  point pt;

  pt.x = x;
  pt.y = y;
  dtinsert (ps, mkPair(pt));
}

int       
inPS (PointSet* ps, point pt)
{
  pair p;
  p.id = pt;
  return ((int)dtsearch (ps, &p));
}

int       
isInPS (PointSet* ps, int x, int y)
{
  pair p;
  p.id.x = x;
  p.id.y = y;
  return ((int)dtsearch (ps, &p));
}

int       
sizeOf (PointSet* ps)
{
  return dtsize(ps);
}

point*    
pointsOf (PointSet* ps)
{
  int    n = dtsize(ps);
  point* pts = N_NEW(n,point);
  pair*  p;
  point* pp = pts;

  for (p = (pair*)dtflatten(ps); p; p = (pair*)dtlink(ps,(Dtlink_t*)p)) {
    *pp++ = p->id;
  }

  return pts;
}

