/*
    This software may only be used by you under license from AT&T Corp.
    ("AT&T").  A copy of AT&T's Source Code Agreement is available at
    AT&T's Internet website having the URL:
    <http://www.research.att.com/sw/tools/graphviz/license/source.html>
    If you received this software without first entering into a license
    with AT&T, you have an infringing copy of this software and cannot use
    it without violating AT&T's intellectual property rights.
*/

/*
 * Queue implementation using cdt
 *
 */

#include <queue.h>
#include <ast.h>

typedef struct {
  Dtlink_t  link;
  void*     np;
} nsitem;

static Void_t*
makef(Dt_t* d, nsitem* obj, Dtdisc_t* disc)
{
  nsitem*   p;

  p = oldof(0,nsitem,1,0);
  p->np = obj->np;
  return p;
}

static void
freef(Dt_t* d, nsitem* obj, Dtdisc_t* disc)
{
  free (obj);
}

static Dtdisc_t ndisc = {
  offsetof(nsitem,np),
  sizeof(void*),
  offsetof(nsitem,link),
  (Dtmake_f)makef,
  (Dtfree_f)freef,
  0,
  0,
  0,
  0
};

queue* 
mkQ(Dtmethod_t* meth)
{
  queue*  nq;

  nq = dtopen (&ndisc, meth);
  return nq;
}

void
push(queue* nq, void* n)
{
  nsitem   obj;

  obj.np = n;
  dtinsert (nq, &obj);
}

void* 
pop(queue* nq, int delete)
{
  nsitem*   obj;
  void* n;

  obj = dtfirst (nq);
  if (obj) {
    n = obj->np;
    if (delete) dtdelete (nq,0);    
    return n;
  }
  else return 0;
}

void 
freeQ(queue* nq)
{
  dtclose(nq);
}

