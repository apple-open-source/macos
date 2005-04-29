/* 
 * Network Simplex Algorithm for Ranking Nodes of a DAG
 * Version 2/4/94 for new libgraph.
 * Version 8/3/94 with new interface.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_VALUES_H
#include <values.h>
#else
#include <limits.h>
#ifndef MAXINT
#define MAXINT INT_MAX
#endif
#include <float.h>
#ifndef MAXDOUBLE
#define MAXDOUBLE DBL_MAX
#endif
#ifndef MAXFLOAT
#define MAXFLOAT FLT_MAX
#endif
#endif

static int NS_run;

#include <agraph.h>
#include <agutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nspvt.h"

#ifdef DEBUG
#include <assert.h>
#else
#define assert(x)
#endif

#ifdef DMALLOC
#include "dmalloc.h"
#endif

#define SEQ(a,b,c)		(((a) <= (b)) && ((b) <= (c)))
#define NEW(g,type)		((type*)(agalloc((g),sizeof(type))))
#define N_NEW(g,n,type)	((type*)(agalloc((g),(n)*sizeof(type))))
#define NOT(x)			(!(x))

#define	NODEDATA(v)		((nsnode_t*)(AGDATA(v)))
#define rank(v)			(NODEDATA(v)->n_rank)
#define priority(v)		(NODEDATA(v)->n_priority)
#define mark(v)			(NODEDATA(v)->n_mark)
#define onstack(v)		(NODEDATA(v)->n_onstack)
#define dmark(v)		(NODEDATA(v)->n_dmark)
#define paredge(v)		(NODEDATA(v)->n_par)
#define treelist(v)		(NODEDATA(v)->n_tree)
#define lim(v)			(NODEDATA(v)->n_lim)
#define low(v)			(NODEDATA(v)->n_low)
#define IN 0
#define OUT 1
#define treein(v)		(treelist(v)[IN])
#define treeout(v)		(treelist(v)[OUT])

#define	EDGEDATA(e)		((nsedge_t*)(AGDATA(e)))
#define cutval(e)		(EDGEDATA(e)->e_cutval)
#define weight(e)		(EDGEDATA(e)->e_weight)
#define minlen(e)		(EDGEDATA(e)->e_minlen)
#define treeflag(e)		(EDGEDATA(e)->e_treeflag)
#define prevlist(e)		(EDGEDATA(e)->prv)
#define nextlist(e)		(EDGEDATA(e)->nxt)
#define treeprevin(e)	(prevlist(e)[IN])
#define treeprevout(e)	(prevlist(e)[OUT])
#define treenextin(e)	(nextlist(e)[IN])
#define treenextout(e)	(nextlist(e)[OUT])

#define	GRAPHDATA(g)	((nsgraph_t*)(AGDATA(g)))
#define	n_tree_edges(g) (GRAPHDATA(g)->g_n_tree_edges)
#define finger(g)		(GRAPHDATA(g)->g_finger)
#define maxiter(g)		(GRAPHDATA(g)->g_maxiter)
#define n_nodes(g)		(GRAPHDATA(g)->g_n_nodes)

#define NILedge			NIL(Agedge_t*)
#define NILnode			NIL(Agnode_t*)

#define agfstn(arg)	AGFIRSTNODE(arg)
#define agnxtn(arg)	AGNEXTNODE(arg)
#define agnxte(arg) AGNXTE(arg)
#define agfout(arg)	AGFSTOUT(arg)
#define agfin(arg)	AGFSTIN(arg)

#ifdef DEBUG
int _n_tree_edges(Agraph_t *g) { return n_tree_edges(g); }

int _rank(Agnode_t *v) { return rank(v); }
Agedge_t *_paredge(Agnode_t *v) { return paredge(v); }

int _weight(Agedge_t *e) { return weight(e); }
int _minlen(Agedge_t *e) { return minlen(e); }
int _low(Agedge_t *e) { return low(e); }
int _lim(Agedge_t *e) { return lim(e); }
int _cutval(Agedge_t *e) { return cutval(e); }

int _priority(Agnode_t *n)	{ return (priority(n)); }
int _mark(Agnode_t *n)	{ return (mark(n)); }
Agedge_t* _treein(Agnode_t *n)	{ return (treelist(n)[IN]) ; }
Agedge_t* _treeout(Agnode_t *n)	{ return (treelist(n)[OUT]) ; }
Agedge_t* _treeprevin(Agedge_t *e)	{ return (prevlist(e)[IN]) ; }
Agedge_t* _treeprevout(Agedge_t *e)	{ return (prevlist(e)[OUT]) ; }
Agedge_t* _treenextin(Agedge_t *e)	{ return (nextlist(e)[IN]) ; }
Agedge_t* _treenextout(Agedge_t *e)	{ return (nextlist(e)[OUT]) ; }
#endif

/* IDs for runtime data */

static char NS_nodedata[] = "ns_nodedata";
static char NS_edgedata[] = "ns_edgedata";
static char NS_graphdata[] = "ns_graphdata";

/* forward declarations */
void init_cutvalues(Agraph_t *g);
static void ns_check_graph(Agraph_t *g);
#if 0 /* not used */
static void ns_check_cutvalues(Agraph_t *g);
#endif
static void ns_attachattrs(Agraph_t *g);
static void ns_normalize(Agraph_t *g);
static int  ns_check_ranks(Agraph_t *g, int iter, int verbose);

/* returns TRUE if descriptor was already present (else create one) */
static boolean descriptor_to_front(void *obj, char *id, int sz)
{
	boolean		rv;
	/* Agrec_t		*rec; */

	if ((/* rec = */ aggetrec(obj, id, TRUE))) rv = TRUE;
	else {
		/* rec = */ agbindrec(obj, id, sz, TRUE);
		rv = FALSE;
	}
	return rv;
}

/* create local data */
static boolean precondition_node(Agnode_t *n)
{
	boolean	rv;

	if ((rv = descriptor_to_front(n,NS_nodedata,sizeof(nsnode_t)))) {
		rank(n) = low(n) = lim(n) = priority(n) = 0;
		mark(n) = dmark(n) = onstack(n) = 0;
		paredge(n) = treein(n) = treeout(n) = NILedge;
	}
	return rv;
}


static void scan_node(Agnode_t *n)
{
	char	*s;
	if (NOT(precondition_node(n))) {
		s = agget(n,"rank");
		rank(n) = s[0]? atoi(s) : 1;
	}
}

static boolean precondition_edge(Agedge_t *e)
{
	boolean		rv;

	if ((rv = descriptor_to_front(e,NS_edgedata,sizeof(nsedge_t)))) {
		cutval(e) = treeflag(e) = 0;
		treeprevin(e) = treeprevout(e) = treenextin(e) = treenextout(e) 
			= NILedge;
	}
	else {
		weight(e) = minlen(e) = 1;
	}
	return rv;
}

static void scan_edge(Agedge_t *e)
{
	char		*s;

	if (NOT(precondition_edge(e))) {
		s = agget(e,"weight");
		if (s[0]) weight(e) = atoi(s);
		s = agget(e,"minlen");
		if (s[0]) minlen(e) = atoi(s);
	}
}

static boolean precondition_graph(Agraph_t *g)
{
	boolean	rv;

	if ((rv = descriptor_to_front(g,NS_graphdata,sizeof(nsgraph_t)))) {
		n_tree_edges(g) = 0;
		finger(g) = NILnode;
	}
	else {
		maxiter(g) = MAXINT;
	}
	return rv;
}

static void scan_graph(Agraph_t *g)
{
	char	*s;

	if (NOT(precondition_graph(g))) {
		s = agget(g,"nslimit");
		if (s[0]) maxiter(g) = atoi(s);
	}
	n_nodes(g) = agnnodes(g);
}

/* decide when to cut off NS iteration */
int ns_step(Agraph_t *g, int iter, int verbose)
{
	int		d;
	if (verbose && (iter % 100 == 0)) {
		d = iter % 1000;
		if (d == 0) fputs("network simplex: ",stderr);
		fprintf(stderr,"%d ",iter);
		if (d == 9) fputc('\n',stderr);
	}
	return (iter >= maxiter(g));
}

/* tight tree maintenance */

void add_tree_edge(Agedge_t *e)
{
	int			i;
	Agnode_t	*v[2];
	Agedge_t	*f;

	e = AGMKOUT(e);
	assert (treeprevin(e) == NILedge);
	assert (treeprevout(e) == NILedge);
	assert (treenextin(e) == NILedge);
	assert (treenextout(e) == NILedge);

	v[IN] = AGHEAD(e);
	v[OUT] = AGTAIL(e);
	for (i = 0; i < 2; i++) {
		mark(v[i]) = TRUE;
		if ((f = treelist(v[i])[i])) {
			assert(prevlist(f)[i] == NILedge);
			prevlist(f)[i] = e;
		}
		nextlist(e)[i] = f;
		treelist(v[i])[i] = e;
	}
	treeflag(e) = TRUE;
	n_tree_edges(agraphof(v[0]))++;
}

void del_tree_edge(Agedge_t *e)
{
	int			i;
	Agnode_t	*v[2];
	Agedge_t	*prev,*next;

	v[IN] = AGHEAD(e);
	v[OUT] = AGTAIL(e);

	for (i = 0; i < 2; i++) {
		prev = prevlist(e)[i];
		next = nextlist(e)[i];
		prevlist(e)[i] = nextlist(e)[i] = NILedge;
		if (prev) nextlist(prev)[i] = next;
		else treelist(v[i])[i] = next;
		if (next) prevlist(next)[i] = prev;
	}
	treeflag(e) = FALSE;
	n_tree_edges(agraphof(v[0]))--;
}

void init_rank(Agraph_t *g)
{
	int				r,ctr;
	Nqueue			*Q;
	Agnode_t		*n;
	Agedge_t		*e;

	ctr = 0;

	Q = Nqueue_new(g);
	for (n = agfstn(g); n; n = agnxtn(n))
		if (priority(n) == 0) Nqueue_insert(Q,n);

	while ((n = Nqueue_remove(Q))) {
		ctr++;
		rank(n) = 0;

		for (e = agfin(n); e; e = agnxte(e)) {
			r = rank(AGTAIL(e)) + minlen(e);
			if (rank(n) < r) rank(n) = r;
		}

		for (e = agfout(n); e; e = agnxte(e)) {
			if (priority(AGHEAD(e)) <= 0) abort();
			if (--priority(AGHEAD(e)) == 0)  Nqueue_insert(Q,AGHEAD(e));
		}
	}
	if (ctr != n_nodes(g)) {
		fprintf(stderr,"ns: init_rank failed\n");
		for (n = agfstn(g); n; n = agnxtn(n))
			if (priority(n) > 0) fprintf(stderr,"\t%s\n",agnameof(n));
	}
	Nqueue_free(g,Q);
}

Agnode_t *incident(Agedge_t *e)
{
	Agnode_t	*u,*v;

	u = AGTAIL(e);
	v = AGHEAD(e);

	if (mark(u)) { if (mark(v) == FALSE) return v; }
	else { if (mark(v)) return u; }
	return NILnode;
}

/*
 *  Returns some tree edge whose cutval is negative, or else nil.
 *  NS converges faster if edges are searched cyclically.
 *  Might be faster if it cached negative edges not chosen this time?
 */
static int		Search_size = 20;

Agedge_t *leave_edge(Agraph_t *g)
{
	Agnode_t		*n, *start;
	Agedge_t		*e, *rv = NULL;
	int				cnt, cv, pcv = 0;

	start = finger(g);
	if (start == NILnode) start = agfstn(g);
	cnt = 0;
	n = start;
	do {
		for (e = agfout(n); e;  e = agnxte(e)) {
			if ((cv = cutval(e)) < 0) {
				cnt++;
				if ((rv == NILedge) || (cv < pcv)) {rv = e; pcv = cv;}
				if (cnt > Search_size) break;
			}
		}
		n = agnxtn(n);
		if (n == NILnode) n = agfstn(g);
	} while (n != start);
	finger(g) = agnxtn(n);
	return rv;
}

/*
 * choose an edge with minimum slack to bring into the tight tree
 */
static Agedge_t	*Enter;
static int		Low,Lim,Slack,Inflag;

static int slack(Agedge_t *e)		/* e is must be the out-edge */
{
	return rank(AGHEAD(e)) - rank(AGTAIL(e)) - minlen(e);
}

static Agnode_t *agother(Agnode_t *n, Agedge_t *e)
{
	if (n != e->node) return e->node;
	return agopp(e)->node;
}

void dfs_enter_edge(Agnode_t *v)
{
	int			eslack,list;
	Agedge_t	*e;
	Agnode_t	*node;

	if (Inflag) e = agfin(v); else e = agfout(v);

	while (e) {
		node = e->node;
		if (treeflag(e) == FALSE) {
			if (!SEQ(Low,lim(node),Lim)) {
				eslack = slack(e);
				if ((eslack < Slack) || (Enter == NULL)) {
					Enter = e;
					Slack = eslack;
				}
			}
		}
		else if (lim(node) < lim(v)) dfs_enter_edge(node);
		e = agnxte(e);
	}

	if (Inflag) list = OUT; else list = IN;
	for (e = treelist(v)[list]; e && (Slack > 0); e = nextlist(e)[list]) {
		node = agother(v,e);
		if (lim(node) < lim(v)) dfs_enter_edge(node);
	} 
}

Agedge_t *enter_edge(Agedge_t *e)
{
	Agnode_t	*tail,*head,*v;

	head = AGHEAD(e); tail = AGTAIL(e);
	if (lim(tail) < lim(head)) {v = tail; Inflag = TRUE;}
	else {v = head; Inflag = FALSE;}
	Enter = NILedge;
	Slack = MAXINT;
	Low = low(v);
	Lim = lim(v);
	dfs_enter_edge(v);
	return Enter;
}

static int tsearch(Agraph_t *g, Agnode_t *n)
{
	Agedge_t	*e;

	for (e = agfstedge(n); e; e = agnxtedge(e,n)) {
		if ((slack(e) == 0) && (mark(e->node) == FALSE)) {
			add_tree_edge(e);
			if (n_tree_edges(g) == n_nodes(g)-1) return TRUE;
			if (tsearch(g,e->node)) return TRUE;
		}
	}
	return FALSE;
}

int tight_tree(Agraph_t *g)
{
	Agnode_t	*v;

	v = agfstn(g);
	if ((tsearch(g,v) == FALSE)) {
		while ((v = agnxtn(v))) {
			if (mark(v) && tsearch(g,v)) break;
		}
	}
	return n_tree_edges(g) + 1;
}

void feasible_tree(Agraph_t *g, int use_ranks)
{
	int				t,tctr,adj;
	Agnode_t		*n,**list;
	Agedge_t		*e,*f;

	if (n_nodes(g) <= 1) return;
	if (use_ranks == FALSE) init_rank(g);

	list = NIL(Agnode_t**);
	while ((t = tight_tree(g)) < n_nodes(g)) {
		if (list == NIL(Agnode_t**)) list = N_NEW(g,n_nodes(g),Agnode_t*);
		if (t == 1) mark(agfstn(g)) = TRUE;
		tctr = 0;
		e = NILedge;
		for (n = agfstn(g); n; n = agnxtn(n)) {
			if (mark(n)) {
				assert(treein(n) || treeout(n) || (t == 1));
				list[tctr++] = n;
			}
			else {
#ifdef  DEBUG
				fprintf(stderr,"%x=%s is missing\n",n,agnameof(n));
				assert(mark(n) == FALSE);
#endif
			}
			for (f = agfstedge(n); f; f = agnxtedge(f,n)) {
				if ((treeflag(f) == FALSE) && (incident(f) != NILnode)) {
					if ((e == NILedge) || (slack(f) < slack(e)))
						e = f;
				}
			}
		}
		if (e) {
#ifdef DEBUG
			fprintf(stderr,"bring in %s\n",agnameof(incident(e)));
#endif
			if ((adj = slack(e))) {
				if (incident(e) == AGTAIL(e)) adj = -adj;
				while (tctr > 0) {
					n = list[--tctr];
					rank(n) += adj;
				}
			}
			else abort();
		}
		else abort();
	}
	if (list) agfree(g,list);
	init_cutvalues(g);
}

/* walk up from v to LCA(v,w), setting new cutvalues. */
Agnode_t *treeupdate(Agnode_t *v, Agnode_t *w, int cutvalue, int dir)
{
	Agedge_t	*e;
	int			d;

	while (!SEQ(low(v),lim(w),lim(v))) {
		e = paredge(v);
		if (v == AGTAIL(e)) d = dir; else d = NOT(dir);
		if (d) cutval(e) += cutvalue; else cutval(e) -= cutvalue;
		if (lim(AGTAIL(e)) > lim(AGHEAD(e))) v = AGTAIL(e);
		else v = AGHEAD(e);
	}
	return v;
}

void rerank(Agnode_t *v,int d)
{
	Agedge_t	*e;

	rank(v) -= d;

	for (e = treeout(v); e; e = treenextout(e))
		if (e != paredge(v)) rerank(AGHEAD(e),d);
	for (e = treein(v); e; e = treenextin(e))
		if (e != paredge(v)) rerank(AGTAIL(e),d);
}

int dfs_range(Agnode_t *v, Agedge_t *par, int low);

/* e is the tree edge that is leaving and f is the nontree edge that
 * is entering.  compute new cut values, ranks, and exchange e and f.
 */
void update(Agedge_t *e, Agedge_t *f)
{
	int			cutvalue,minlen;
	Agnode_t	*lca;

	minlen = slack(f);

	/* "for (v = in nodes in tail side of e) do rank(v) -= minlen;" */
	if (minlen > 0) {
		if (lim(AGTAIL(e)) < lim(AGHEAD(e))) rerank(AGTAIL(e),minlen);
		else rerank(AGHEAD(e),-minlen);
	}

	cutvalue = cutval(e);
	lca = treeupdate(AGTAIL(f),AGHEAD(f),cutvalue,1);
	if (treeupdate(AGHEAD(f),AGTAIL(f),cutvalue,0) != lca) abort();
	cutval(e) = 0;
	cutval(f) = -cutvalue;
	del_tree_edge(e);
	add_tree_edge(f);
	dfs_range(lca,paredge(lca),low(lca));
}

/* utility function to compute cut values */
int x_val(Agedge_t *e, Agnode_t *v, int dir)
{
	Agnode_t	*other;
	int			d,rv,f;

	other = agother(v,e);
	if (!(SEQ(low(v),lim(other),lim(v)))) {f = 1; rv = weight(e);}
	else {
		f = 0;
		if (treeflag(e)) rv = cutval(e);
		else rv = 0;
		rv -= weight(e);
	}
	if (dir > 0) {if (AGHEAD(e) == v) d = 1; else d = -1;}
	else {if (AGTAIL(e) == v) d = 1; else d = -1;}
	if (f) d = -d;
	if (d < 0) rv = -rv;
	return rv;
}

/* set cut value of f, assuming values of edges on one side were already set */
void set_cutval(Agedge_t *f)
{
	Agnode_t	*v;
	Agedge_t	*e;
	int			sum,dir;

	/* set v to the node on the side of the edge already searched */
	if (paredge(AGTAIL(f)) == f) {v = AGTAIL(f); dir = 1;}
	else { v = AGHEAD(f); dir = -1; }

	sum = 0;
	for (e = agfout(v); e; e = agnxte(e)) sum += x_val(e,v,dir);
	for (e = agfin(v); e; e = agnxte(e)) sum += x_val(e,v,dir);
	cutval(f) = sum;
}


void dfs_cutval(Agnode_t *v, Agedge_t *par)
{
	Agedge_t	*e;

	for (e = treeout(v); e; e = treenextout(e))
		if (e != par) dfs_cutval(AGHEAD(e),e);
	for (e = treein(v); e; e = treenextin(e))
		if (e != par) dfs_cutval(AGTAIL(e),e);
	if (par) set_cutval(par);
}

int dfs_range(Agnode_t *v, Agedge_t *par, int low)
{
	Agedge_t	*e;
	int			lim;

	paredge(v) = par;
	lim = low(v) = low;

	for (e = treeout(v); e; e = treenextout(e))
		if (e != par) lim = dfs_range(AGHEAD(e),e,lim);
	for (e = treein(v); e; e = treenextin(e))
		if (e != par) lim = dfs_range(AGTAIL(e),e,lim);
	lim(v) = lim;
	return lim + 1;
}

void init_cutvalues(Agraph_t *g)
{
	dfs_range(agfstnode(g),NILedge,1);
	dfs_cutval(agfstnode(g),NILedge);
}

/* assigns initial ranks */
static int init_graph(Agraph_t *g)
{
	Agnode_t	*n;
	Agedge_t	*e;
	int			feasible;

	scan_graph(g);
	for (n = agfstn(g); n; n = agnxtn(n)) {
		scan_node(n);
		for (e = agfout(n); e; e = agnxte(e))
			scan_edge(e);
	}

	feasible = TRUE;
	for (n = agfstn(g); n; n = agnxtn(n)) {
		priority(n) = 0;
		for (e = agfin(n); e; e = agnxte(e)) {
			priority(n)++;
			cutval(e) = 0;
			if (feasible && (rank(n) - rank(e->node)) < minlen(e))
				feasible = FALSE;
		}
	}
	return feasible;
}

/* main entry point  to
 * assign ranks to nodes of a DAG 
 */
void ns_solve(Agraph_t *g, unsigned int flags)
{
	int			iter,feasible,verbose;
	Agedge_t	*e,*f;

	NS_run++;
	verbose = flags & NS_VERBOSE;
	agflatten(g,TRUE);
	feasible = init_graph(g);
	if (flags & NS_VALIDATE) ns_check_graph(g);
	feasible_tree(g,feasible);

	iter = 0;
	while ((e = leave_edge(g))) {
		f = enter_edge(e);
		update(e,f);
		if (ns_step(g,++iter, verbose)) break;
	}

	if ((verbose) || (flags & NS_DEBUG)) ns_check_ranks(g,iter,verbose);
	if (flags & NS_NORMALIZE) ns_normalize(g);
	if (flags & NS_ATTACHATTRS) ns_attachattrs(g);
	agflatten(g,FALSE);
}

void ns_setminlength(Agedge_t *e, int len)
{
	precondition_edge(e);
	minlen(e) = len;
}

void ns_setweight(Agedge_t *e, int weight)
{
	precondition_edge(e);
	weight(e) = weight;
}

int ns_getweight(Agedge_t *e)
{
	nsedge_t	*rec;
	if ((rec = (nsedge_t*)aggetrec(e,NS_edgedata,FALSE)) == NIL(void*))
		return 1;
	return rec->e_weight;
}

int ns_getminlength(Agedge_t *e)
{
	nsedge_t	*rec;
	if ((rec = (nsedge_t*)aggetrec(e,NS_edgedata,FALSE)) == NIL(void*))
		return 1;
	return rec->e_minlen;
}

int ns_getrank(Agnode_t *n)
{
	nsnode_t	*rec;
	if ((rec = (nsnode_t*)aggetrec(n,NS_nodedata,FALSE)) == NIL(void*))
		return 0;
	return rec->n_rank;
}

void ns_setrank(Agnode_t *n, int r)
{
	(void) precondition_node(n);
	rank(n) = r;
}

static int ns_check_ranks(Agraph_t *g, int iter, int verbose)
{
	int			cost,len;
	Agnode_t	*n;
	Agedge_t	*e;

	cost = 0;
	for (n = agfstn(g); n; n = agnxtn(n)) {
		for (e = agfout(n); e; e = agnxte(e)) {
			len = rank(e->node) - rank(n);
			if (len < minlen(e)) {
				fprintf(stderr,"%s %s edge constraint violated %d < %d\n",
					agnameof(agtail(e)),agnameof(aghead(e)),len,minlen(e));
				abort();
			}
			else cost += weight(e)*len;
		}
	}
	if (verbose) fprintf(stderr,"ns: %s, %d iter, %d cost\n",agnameof(g),iter,cost);
	return cost;
}

#if 0 /* not used */
static void ns_check_cutvalues(Agraph_t *g)
{
	Agnode_t	*v;
	Agedge_t	*e,*f,*opp_e;
	int			save;

	for (v = agfstnode(g); v; v = agnxtnode(v)) {
		for (e = treeout(v); e; e = treenextout(e)) {
			save = cutval(e);
			set_cutval(e);
			if (save != cutval(e)) abort();

			opp_e = AGOPP(e);
			for (f = treein(AGHEAD(e)); f; f = treenextin(f))
				if ((e == f)||(opp_e == f)) break;
			if (f == NILedge) abort();
		}
	}
}
#endif

static void ns_checkdfs(Agnode_t *n)
{
	Agedge_t	*e;
	Agnode_t	*w;

	if (dmark(n)) return;
	dmark(n) = TRUE;
	onstack(n) = TRUE;
	for (e = agfstout(n); e; e = agnxtout(e)) {
		w = AGHEAD(e);
		if (onstack(w)) {
			fprintf(stderr,"ns: cycle involving %s",agnameof(n));
			fprintf(stderr," -> %s\n",agnameof(w));
		}
		else {
			if (dmark(w) == FALSE) ns_checkdfs(w);
		}
	}
	onstack(n) = FALSE;
}

static void ns_checkreach(Agnode_t *n)
{
	Agedge_t	*e;

Agnode_t *bum;
	if (n) {
		dmark(n) = TRUE;
		for (e = agfstedge(n); e; e = agnxtedge(e,n)) {
			bum = e->node;
			assert(agsubnode(agraphof(bum),bum,FALSE));
			if (dmark(e->node) == FALSE) ns_checkreach(e->node);
		}
	}
}

void ns_check_con(Agraph_t *g)
{
	Agnode_t	*n;
	int			failed;

	/* check if connected */
	for (n = agfstnode(g); n; n = agnxtnode(n)) dmark(n) = FALSE;
	ns_checkreach(agfstnode(g));
	failed = 0;
	for (n = agfstnode(g); n; n = agnxtnode(n)) {
		if (dmark(n) == FALSE) {
			fprintf(stderr,"ns: %s not connected\n",agnameof(n));
			failed = 1;
		}
		dmark(n) = FALSE;
	}
	if (failed) abort();
}

static void ns_check_graph(Agraph_t *g)
{
	Agnode_t	*n;

	for (n = agfstnode(g); n; n = agnxtnode(n)) {
		if (strcmp(AGDATA(n)->name,NS_nodedata))
			fprintf(stderr,"ns: %s not preconditioned\n",agnameof(n));
		onstack(n) = FALSE;
		dmark(n) = FALSE;

	}

	ns_check_con(g);

	/* check for cycles */
	for (n = agfstnode(g); n; n = agnxtnode(n))
		ns_checkdfs(n);
}

static void set_number(void *obj, Agsym_t *a, int n)
{
	char	buf[24];

	sprintf(buf,"%d",n);
	agxset(obj,a,buf);
}

void ns_attachattrs(Agraph_t *g)
{
	Agsym_t		*a_rank,*a_weight,*a_minlen;
	Agnode_t	*n;
	Agedge_t	*e;

	a_rank = agattr(g,AGNODE,"rank","-1");
	a_weight = agattr(g,AGEDGE,"weight","1");
	a_minlen = agattr(g,AGEDGE,"minlen","1");

	for (n = agfstnode(g); n; n = agnxtnode(n)) {
		set_number(n,a_rank,ns_getrank(n));
		for (e = agfstout(n); e; e = agnxtout(e)) {
			set_number(e,a_weight,ns_getweight(e));
			set_number(e,a_minlen,ns_getminlength(e));
		}
	}
}

void ns_normalize(Agraph_t *g)
{
	Agnode_t	*n;
	int			minr;

	minr = MAXINT;
	for (n = agfstnode(g); n; n = agnxtnode(n))
		if (minr > rank(n)) minr = rank(n);

	if (minr != 0) for (n = agfstnode(g); n; n = agnxtnode(n))
		rank(n) = rank(n) - minr;
}

void ns_clean(Agraph_t *g) {
	agclean(g,AGRAPH,NS_graphdata);
	agclean(g,AGNODE,NS_nodedata);
	agclean(g,AGEDGE,NS_edgedata);
}
