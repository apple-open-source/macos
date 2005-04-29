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

#include	<assert.h>
#include	"edgelist.h"

static edgelistitem*
mkItem (Dt_t* d,edgelistitem* obj,Dtdisc_t* disc)
{
  edgelistitem*  ap = GNEW(edgelistitem);

  ap->edge = obj->edge;
  return ap;
}

static void
freeItem (Dt_t* d,edgelistitem* obj,Dtdisc_t* disc)
{
  free (obj);
}

static int
cmpItem(Dt_t* d, Agedge_t** key1, Agedge_t** key2, Dtdisc_t* disc)
{
  if (*key1 > *key2) return 1;
  else if (*key1 < *key2) return -1;
  else return 0;
}

static Dtdisc_t ELDisc = {
  offsetof(edgelistitem,edge),        /* key */
  sizeof(Agedge_t*),                  /* size */
  offsetof(edgelistitem,link),        /* link */
  (Dtmake_f)mkItem,
  (Dtfree_f)freeItem,
  (Dtcompar_f)cmpItem,
  (Dthash_f)0,
  (Dtmemory_f)0,
  (Dtevent_f)0
};

edgelist*
init_edgelist()
{
	edgelist* list = dtopen (&ELDisc, Dtoset);
	return list;
}

void
free_edgelist(edgelist* list)
{
  dtclose(list);
}

void
add_edge(edgelist* list, Agedge_t* e)
{
	edgelistitem temp;
	
	temp.edge = e;
	dtinsert (list, &temp);
}

void
remove_edge(edgelist* list, Agedge_t* e)
{
	edgelistitem temp;
	
	temp.edge = e;
	dtdelete (list,&temp);
}

#ifdef DEBUG
void
print_edge(edgelist* list)
{
	edgelistitem* temp;
	Agedge_t*     ep;

	for(temp = (edgelistitem*)dtflatten(list); temp; 
			temp = (edgelistitem*)dtlink(list,(Dtlink_t*)temp)) {
		ep = temp->edge;
		fprintf(stderr, "%s--%s \n", ep->tail->name, ep->head->name);
	}
	fputs("\n", stderr);
}
#endif

int 
size_edgelist(edgelist* list)
{
	return dtsize(list);
}

