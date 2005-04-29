/*
    This software may only be used by you under license from AT&T Corp.
    ("AT&T").  A copy of AT&T's Source Code Agreement is available at
    AT&T's Internet website having the URL:
    <http://www.research.att.com/sw/tools/graphviz/license/source.html>
    If you received this software without first entering into a license
    with AT&T, you have an infringing copy of this software and cannot use
    it without violating AT&T's intellectual property rights.
*/
#pragma prototyped

#include <nodeset.h>


static nsitem_t*
mkItem (Dt_t* d,nsitem_t* obj,Dtdisc_t* disc)
{
  nsitem_t*  ap = GNEW(nsitem_t);

  ap->np = obj->np;
  return ap;
}

static void
freeItem (Dt_t* d,nsitem_t* obj,Dtdisc_t* disc)
{
  free (obj);
}

static int
cmpItem(Dt_t* d, Agnode_t** key1, Agnode_t** key2, Dtdisc_t* disc)
{
  if (*key1 > *key2) return 1;
  else if (*key1 < *key2) return -1;
  else return 0;
}

static Dtdisc_t nodeDisc = {
  offsetof(nsitem_t,np),        /* key */
  sizeof(Agnode_t*),                  /* size */
  offsetof(nsitem_t,link),        /* link */
  (Dtmake_f)mkItem,
  (Dtfree_f)freeItem,
  (Dtcompar_f)cmpItem,
  (Dthash_f)0,
  (Dtmemory_f)0,
  (Dtevent_f)0
};

/* mkNodeset:
 * Creates an empty node set.
 */
nodeset_t* 
mkNodeset()
{
	nodeset_t*  s = dtopen (&nodeDisc, Dtoset);
	return s;
}

/* freeNodeset:
 * Deletes a node set, deleting all items as well.
 * It does not delete the nodes.
 */
void 
freeNodeset(nodeset_t* s)
{
	if(s != NULL) 
		dtclose (s);
}

/* clearNodeset:
 * Remove all items from a node set.
 */
void 
clearNodeset(nodeset_t* s)
{
	dtclear (s);
}

/* insertNodeset:
 * Add a node into the nodeset.
 */
void 
insertNodeset(nodeset_t* ns, Agnode_t* n)
{
	nsitem_t key;

	key.np = n;
	dtinsert(ns, &key);
}

void 
removeNodeset(nodeset_t* ns, Agnode_t* n)
{
	nsitem_t key;

	key.np = n;
	dtdelete (ns, &key);
}

/* sizeNodeset:
 * Report on the nodeset size.
 */
int 
sizeNodeset(nodeset_t* ns)
{
	return dtsize(ns);
}

/* As the node set is a Dt_t, traversal is done using standard
 * functions from libcdt.
 */

void printNodeset(nodeset_t* ns)
{
  nsitem_t* ip;
  for (ip = (nsitem_t*)dtfirst(ns); ip; ip = (nsitem_t*)dtnext(ns,ip)) {
    fprintf (stderr, "%s", ip->np->name);
  } 
  fputs("\n", stderr);
}
