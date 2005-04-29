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

static unsigned int HTML_BIT;
static unsigned int CNT_BITS;

#include	<assert.h>
#include	"libgraph.h"

#ifdef DMALLOC
#include "dmalloc.h"
#endif

typedef struct refstr_t {
	Dtlink_t		link;
	unsigned int	refcnt;
	char			s[1];
} refstr_t;

static Dtdisc_t Refstrdisc = {
	offsetof(refstr_t,s[0]),
	0,
	0,
	((Dtmake_f)0),
	((Dtfree_f)0),
	((Dtcompar_f)0),			/* use strcmp */
	((Dthash_f)0),
	((Dtmemory_f)0),
	((Dtevent_f)0)
};

static Dict_t*	StringDict;

#ifdef DEBUG
static int refstrprint(Dt_t* d, Void_t* obj, Void_t* env)
{
	refstr_t* r = (refstr_t*)obj;
	fprintf(stderr,"%s\n",r->s); return 0;
}

void
agrefstrdump(void)
{
	dtwalk(StringDict,refstrprint,0);
}
#endif

static void initialize_strings(void)
{
	unsigned int curr, next;

	StringDict	= dtopen(&Refstrdisc,Dttree);
	curr = 1;
	next = 2;
	while (next) {
		curr = next;
		next <<= 1;
	}
	HTML_BIT = curr;
	CNT_BITS = ~HTML_BIT;
}

char *agstrdup(char* s)
{
	refstr_t		*key,*r;

	if (StringDict == NULL) initialize_strings();
	if (s == NULL) return s;

	key = (refstr_t*)(s - offsetof(refstr_t,s[0]));
	r = (refstr_t*) dtsearch(StringDict,key);
	if (r) r->refcnt++;
	else {
		r = (refstr_t*) malloc(sizeof(refstr_t)+strlen(s));
		r->refcnt = 1;
		strcpy(r->s,s);
		dtinsert(StringDict,r);
	}
	return r->s;
}

char *agstrdup_html(char* s)
{
	refstr_t		*key,*r;

	if (StringDict == NULL) initialize_strings();
	if (s == NULL) return s;

	key = (refstr_t*)(s - offsetof(refstr_t,s[0]));
	r = (refstr_t*) dtsearch(StringDict,key);
	if (r) r->refcnt++;
	else {
		r = (refstr_t*) malloc(sizeof(refstr_t)+strlen(s));
		r->refcnt = 1 | HTML_BIT;
		strcpy(r->s,s);
		dtinsert(StringDict,r);
	}
	return r->s;
}

void agstrfree(char* s)
{
	refstr_t		*key,*r;

	if ((StringDict == NULL) || (s == NULL)) return;
	key = (refstr_t*)(s - offsetof(refstr_t,s[0]));
	r = (refstr_t*) dtsearch(StringDict,key);

	if (r) {
		r->refcnt--;
		if ((r->refcnt && CNT_BITS) == 0) {
			dtdelete(StringDict,r);
			free(r);
		}
	}
	else agerr (AGERR, "agstrfree lost %s\n",s);
}

int aghtmlstr (char* s)
{
	refstr_t		*key;

	if ((StringDict == NULL) || (s == NULL)) return 0;
	key = (refstr_t*)(s - offsetof(refstr_t,s[0]));
	return (key->refcnt & HTML_BIT);
}
