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

#include "libgraph.h"

#ifdef DMALLOC
#include "dmalloc.h"
#endif

typedef struct printdict_t {
	Dict_t	*nodesleft, *edgesleft, *subgleft, *e_insubg, *n_insubg;
} printdict_t;

/*
 * memgets - same api as gets
 *
 * gets one line at a time from a memory buffer and places it in a user buffer
 *    up to a maximum of n characters
 *
 * returns pointer to obtained line in user buffer, or
 * returns NULL when last line read from memory buffer
 */
static char *memgets(char *ubuf, int n, FILE *mbuf)
{
	static char *mempos;
	char	*to, *clp;  /* clp = current line pointer */
	int	i;

	if (!n) { /* a call with n==0 (from aglexinit) resets */
		mempos = (char *)mbuf;  /* cast from FILE* required by API */
	}

	clp = to = ubuf;
	for (i=0; i<n-1; i++) { /* leave room for terminator */
		if (*mempos == '\0') {
			if (i) { /* if mbuf doesn't end in \n, provide one */
				*to++ = '\n';
			}
			else {  /* all done */
				clp = NULL;
				mempos = NULL;
			}
			break; /* last line or end-of-buffer */
		}
		if (*mempos == '\n') {
			*to++ = *mempos++;
			break; /* all done with this line */
		}
		*to++ = *mempos++; /* copy character */
	}
	*to++ = '\0';  /* place terminator in ubuf */
	return clp;
}

Agraph_t *agread(FILE* fp)
{
	aglexinit(fp, (fgets));   /* use system fgets */
	agparse();
	return AG.parsed_g;
}

Agraph_t *agmemread(char* cp)
{
	/* cast into a file pointer, but flag that this is in-memory input */
	aglexinit((FILE *)cp, (memgets));  /* memgets defined above */
	agparse();
	return AG.parsed_g;
}

Agraph_t *agread_usergets(FILE* fp, gets_f usergets)
{
	aglexinit(fp, (usergets));  /* usergets provided externally */
	agparse();
	return AG.parsed_g;
}

int agerrors(void)
{
	return AG.syntax_errors;
}

/*
 * canonicalize a string for printing.
 * changes to the semantics of this function
 * also involve the string scanner in lexer.c
 * The third argument is needed because the parser stores
 * the ':' with the port name, so the HTML check needs the
 * full string, but we only want to canonicalize the arg part.
 */
char *_agstrcanon(char* arg, char* buf, char* chk)
{
	char	*s = arg;
	char	*p = buf;
	int		cnt = 0;
	int		has_special = FALSE;
	int		maybe_num;

	if (aghtmlstr(chk)) {
		*p++ = '<';
		while (*s) *p++ = *s++;
		*p++ = '>';
		*p = '\0';
		return buf;
	}
	if (ISEMPTYSTR(arg)) return "\"\"";
	*p++ = '\"';
	maybe_num = (isdigit(arg[0]) || (arg[0] == '.'));
	while (*s) {
		if (*s == '\"') { *p++ = '\\'; has_special = TRUE; }
		else {
			if (!ISALNUM(*s)) has_special = TRUE;
			else if (maybe_num && (!isdigit(*s) && (*s != '.')))
				has_special = TRUE;
		}
		*p++ = *s++;
		cnt++;
		if (cnt % SMALLBUF == 0) {*p++ = '\\'; *p++ = '\n';}
	}
	*p++ = '\"'; *p = '\0';
	if (has_special) return buf;

	/* use quotes to protect tokens (example, a node named "node") */
	if (agtoken(arg) >= 0) return buf;
	return arg;
}

char *agstrcanon(char* arg, char* buf)
{
	return _agstrcanon (arg, buf, arg);
}

static void tabover(FILE* fp, int tab)
{
	while (tab--) putc('\t',fp);
}

static char *getoutputbuffer(char *str)
{
	static char		*rv;
	static int		len;
	int				req;

	req = MAX(2 * strlen(str) + 2, BUFSIZ);
	if (req > len) {
		if (rv) rv = realloc(rv,req);
		else rv = malloc(req);
		len = req;
	}
	return rv;
}

static char *canonical(char *str)
{
	return _agstrcanon(str,getoutputbuffer(str),str);
}

static void write_dict(Agdict_t* dict, FILE* fp)
{
	int			i,cnt = 0;
	Agsym_t		*a;

	for (i = 0; i < dtsize(dict->dict); i++) {
		a = dict->list[i];
		if (ISEMPTYSTR(a->value) == FALSE) {
			if (cnt++ == 0) fprintf(fp, "\t%s [", dict->name);
			else fprintf(fp,", ");
			fprintf(fp,"%s=%s",a->name,canonical(a->value));
		}
	}
	if (cnt > 0) fprintf(fp,"];\n");
}

static void write_diffattr(FILE* fp, int indent, void* obj, void* par, Agdict_t* dict)
{
	Agsym_t*	a;
	int			i;
	char		*p,*q;
	int			cnt = 0;

	for (i = 0; i < dtsize(dict->dict); i++) {
		a = dict->list[i];
		if (a->printed == FALSE) continue;
		p = agxget(obj,a->index);
		if (par) q = agxget(par,a->index);
		else q = a->value;
		if (strcmp(p,q)) {
			if (cnt++ == 0) {
				tabover(fp,indent);
				fprintf(fp,"%s [",dict->name);
			}
			else {
				fprintf (fp,",\n");
				tabover(fp,indent+1);
			}
			fprintf(fp,"%s=",canonical(a->name));
			fprintf(fp,"%s", canonical(p));
		}
	}
	if (cnt > 0) fprintf(fp,"];\n");
}

static void writeattr(FILE	*fp, int *npp, char	*name, char *val)
{
	fprintf(fp,++(*npp) > 1?", " : " [");
	fprintf(fp,"%s=",canonical(name));
	fprintf(fp,"%s",canonical(val));
}

void agwrnode(Agraph_t* g, FILE* fp, Agnode_t* n, int full, int indent)
{
	char		*myval,*defval;
	int  		i,didwrite = FALSE;
	int			nprint = 0;
	Agdict_t	*d = n->graph->univ->nodeattr;
	Agsym_t	*a;

	if (full) {
		for (i = 0; i < dtsize(d->dict); i++) {
			a = d->list[i];
			if (a->printed == FALSE) continue;
			myval = agget(n,a->name);
			if (g == n->graph) defval = a->value;
			else defval = agget(g->proto->n,a->name);
			if (strcmp(defval,myval)) {
				if (didwrite == FALSE) {
					tabover(fp,indent);
					fprintf(fp,"%s",canonical(n->name));
					didwrite = TRUE;
				}
				writeattr(fp,&nprint,a->name,myval);
			}
		}
		if (didwrite) {
			fprintf(fp,(nprint>0? "];\n" : ";\n"));
			return;
		}
	}
	if ((agfstout(g,n) == NULL) && (agfstin(g,n) == NULL)) {
		tabover(fp,indent);
		fprintf(fp,"%s;\n",canonical(n->name));
	}
}

static void writenodeandport(FILE *fp, char *node, char *port)
{
	fprintf(fp,"%s",canonical(node));	/* slimey i know*/
	if (port && *port) 
		fprintf(fp,"%c%s",port[0], _agstrcanon(port+1,getoutputbuffer(port+1),port));
}

void agwredge(Agraph_t* g, FILE* fp, Agedge_t* e, int list_all)
{
	char		*myval,*defval,*edgeop,*tport,*hport;
	int			i,nprint = 0;
	Agdict_t	*d = e->tail->graph->univ->edgeattr;
	Agsym_t	*a;

	if (e->attr) {tport = e->attr[TAILX]; hport = e->attr[HEADX];}
	else tport = hport = "";
	if (g->kind & AGFLAG_DIRECTED) edgeop = "->"; else edgeop = "--";
	writenodeandport(fp,e->tail->name,tport);
	fprintf(fp," %s ",edgeop);
	writenodeandport(fp,e->head->name,hport);
	if (list_all) {
		for (i = 0; i < dtsize(d->dict); i++) {
			a = d->list[i];
			if ((a->printed == FALSE)||((i == KEYX) && (e->printkey != MUSTPRINT)))
				continue;
			myval = agget(e,a->name);
			if (g == g->root) defval = a->value;
			else defval = agget(g->proto->e,a->name);
			if (strcmp(defval,myval)) writeattr(fp,&nprint,a->name,myval);
		}
	}
	fprintf(fp,(nprint>0? "];\n" : ";\n"));
}

Dtdisc_t agEdgedisc = {
	offsetof(Agedge_t,id),
	sizeof(int),
	-1,
	NIL(Dtmake_f),
	NIL(Dtfree_f),
	(Dtcompar_f)agcmpid,
	NIL(Dthash_f),
	NIL(Dtmemory_f),
	NIL(Dtevent_f)
};
static void
write_subg(Agraph_t* g, FILE* fp, Agraph_t* par, int indent, printdict_t* state)
{
	Agraph_t		*subg,*meta;
	Agnode_t		*n, *pn;
	Agedge_t		*e, *pe;
	Dict_t			*save_e, *save_n;

	if (indent) {
		tabover(fp,indent++);
		if (dtsearch(state->subgleft,g->meta_node)) {
			if (strncmp(g->name,"_anonymous",10))
				fprintf(fp,"subgraph %s {\n",canonical(g->name));
			else fprintf(fp,"{\n");	/* no name printed for anonymous subg */
			write_diffattr(fp,indent,g,par,g->univ->globattr);
			/* The root node and edge environment use the dictionaries,
			 * not the proto node or edge, so the next level down must
			 * record differences with the dictionaries.
			 */
			if (par == g->root) {
				pn = NULL;
				pe = NULL;
			}
			else {
				pn = par->proto->n;
				pe = par->proto->e;
			}
			write_diffattr(fp,indent,g->proto->n,pn,g->univ->nodeattr);
			write_diffattr(fp,indent,g->proto->e,pe,g->univ->edgeattr);
			dtdelete(state->subgleft,g->meta_node);
		}
		else {
			fprintf(fp,"subgraph %s;\n",canonical(g->name));
			return;
		}
	}
	else
		write_diffattr(fp,++indent,g,NULL,g->univ->globattr);

	save_n = state->n_insubg;
	save_e = state->e_insubg;
	meta = g->meta_node->graph;
	state->n_insubg = dtopen(&agNamedisc,Dttree);
	state->e_insubg = dtopen(&agOutdisc,Dttree);
	for (e = agfstout(meta,g->meta_node); e; e = agnxtout(meta,e)) {
		subg = agusergraph(e->head);
		write_subg(subg,fp,g,indent,state);
	}
	for (n = agfstnode(g); n; n = agnxtnode(g,n)) {
		if (dtsearch(state->nodesleft,n)) {
			agwrnode(g,fp,n,TRUE,indent);
			dtdelete(state->nodesleft,n);
		}
		else {
			if (dtsearch(state->n_insubg,n) == NULL) {
				agwrnode(g,fp,n,FALSE,indent);
			}
		}
		dtinsert(save_n,n);
	}

	dtdisc(g->outedges,&agEdgedisc,0);	/* sort by id */
	for (e = (Agedge_t*)dtfirst(g->outedges); e; e = (Agedge_t*)dtnext(g->outedges,e)) {
		if (dtsearch(state->edgesleft,e)) {
			tabover(fp,indent);
			agwredge(g,fp,e,TRUE);
			dtdelete(state->edgesleft,e);
		}
		else {
			if (dtsearch(state->e_insubg,e) == NULL) {
				tabover(fp,indent);
				agwredge(g,fp,e,FALSE);
			}
		}
		dtinsert(save_e,e);
	}
	dtdisc(g->outedges,&agOutdisc,0);	/* sort by name */
	dtclose(state->n_insubg); state->n_insubg = save_n;
	dtclose(state->e_insubg); state->e_insubg = save_e;

	if (indent > 1) {
		tabover(fp,indent-1);
		fprintf(fp,"}\n");
	}
}

static Dict_t	*Copy;
static int copydictf( Dict_t	*d, void *a, void *ignored)
{
	dtinsert(Copy,(Agsym_t*)a);
	return 0;
}

static void copydict(Dict_t* from, Dict_t* to)
{
	Copy = to;
	dtwalk(from,copydictf,0);
}

static printdict_t *new_printdict_t(Agraph_t* g)
{
	printdict_t	*rv = NEW(printdict_t);
	rv->nodesleft	= dtopen(&agNodedisc,Dttree);
	copydict(g->nodes,rv->nodesleft);
	rv->edgesleft	= dtopen(&agEdgedisc,Dttree);
	copydict(g->outedges,rv->edgesleft);
	rv->n_insubg	= dtopen(&agNodedisc,Dttree);
	rv->e_insubg	= dtopen(&agOutdisc,Dttree);
	rv->subgleft	= dtopen(&agNodedisc,Dttree);
	copydict(g->meta_node->graph->nodes,rv->subgleft);
	return rv;
}

static void free_printdict_t(printdict_t* dict)
{
	dtclose(dict->nodesleft);
	dtclose(dict->n_insubg);
	dtclose(dict->edgesleft);
	dtclose(dict->e_insubg);
	dtclose(dict->subgleft);
	free(dict);
}

int agwrite(Agraph_t* g, FILE* fp)
{
	printdict_t		*p;
	char			*t0,*t1;

	/* write the graph header */
	t0 = (AG_IS_STRICT(g)) ? "strict " : "";
	t1 = (AG_IS_DIRECTED(g)) ? "digraph" : "graph";
	if (strncmp(g->name,"_anonymous",10))
		fprintf(fp, "%s%s %s {\n", t0, t1, canonical(g->name));
	else
		fprintf(fp, "%s%s {\n", t0, t1);

	/* write the top level attribute defs */
	write_dict(g->univ->globattr,fp);
	write_dict(g->univ->nodeattr,fp);
	write_dict(g->univ->edgeattr,fp);

	/* write the graph contents */
	p = new_printdict_t(g);
	write_subg(g,fp,(Agraph_t*)0,0,p);
	fprintf(fp,"}\n");
	free_printdict_t(p);
	return ferror(fp);
}
