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
#include <aghdr.h>

#ifdef DMALLOC
#include "dmalloc.h"
#endif

/*
 * reference counted strings.
 */

typedef struct refstr_t {
	Dtlink_t		link;
	unsigned long	refcnt;
	char			*s;
	char			store[1];		/* this is actually a dynamic array */
} refstr_t;

static Dtdisc_t Refstrdisc = {
	offsetof(refstr_t,s),	/* key */
	-1,						/* size */
	0,						/* link offset */
	NIL(Dtmake_f),
	agdictobjfree,
	NIL(Dtcompar_f),
	NIL(Dthash_f),
	agdictobjmem,
	NIL(Dtevent_f)
};

static Dict_t *Refdict_default;

static Dict_t *refdict(Agraph_t *g)
{
	Dict_t	**dictref;

	if (g) dictref = &(g->clos->strdict);
	else dictref = &Refdict_default;
	if (*dictref == NIL(Dict_t*))
		*dictref = agdtopen(g,&Refstrdisc,Dttree);
	return *dictref;
}

void agstrclose(Agraph_t *g)
{
	agdtclose(g,refdict(g));
}

static refstr_t *refsymbind(Dict_t *strdict, char *s)
{
	refstr_t		key,*r;
	key.s = s;
	r = (refstr_t*) dtsearch(strdict,&key);
	return r;
}

static char *refstrbind(Dict_t *strdict, char *s)
{
	refstr_t	*r;
	r = refsymbind(strdict, s);
	if (r) return r->s;
	else return NIL(char*);
}

char *agstrbind(Agraph_t *g, char *s)
{
	return refstrbind(refdict(g),s);
}

char *agstrdup(Agraph_t *g, char *s)
{
	refstr_t		*r;
	Dict_t			*strdict;
	size_t			sz;

	if (s == NIL(char*)) return NIL(char*);
	strdict = refdict(g);
	r = refsymbind(strdict,s);
	if (r) r->refcnt++;
	else {
		sz = sizeof(refstr_t)+strlen(s);
		if (g) r = (refstr_t*) agalloc(g,sz);
		else r = (refstr_t*)malloc(sz);
		r->refcnt = 1;
		strcpy(r->store,s);
		r->s = r->store;
		dtinsert(strdict,r);
	}
	return r->s;
}

int agstrfree(Agraph_t *g, char *s)
{
	refstr_t		*r;
	Dict_t			*strdict;

	if (s == NIL(char*))
		return FAILURE;

	strdict = refdict(g);
	r = refsymbind(strdict,s);
	if (r && (r->s == s)) {
		r->refcnt--;
		if (r->refcnt <= 0) {
			agdtdelete(g,strdict,r);
			/*
			if (g) agfree(g,r);
			else free(r);
			*/
		}
	}
	if (r == NIL(refstr_t*)) return FAILURE;
	return SUCCESS;
}

#ifdef DEBUG
static int refstrprint(Dict_t *dict, void *ptr, void *user)
{
	refstr_t *r;

	NOTUSED(dict);
	r = ptr;
	NOTUSED(user);
	write(2,r->s,strlen(r->s));
	write(2,"\n",1);
	return 0;
}

void agrefstrdump(Agraph_t *g)
{
	dtwalk(Refdict_default,refstrprint,0);
}
#endif
