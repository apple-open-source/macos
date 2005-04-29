#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <agraph.h>
#include <ingraphs.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
#include "compat_getopt.h"
#endif

#define BIG HUGE	/* ha */

static char *CmdName;

typedef struct nodedata_s {
		Agrec_t	hdr;
		double	dist;
		unsigned char inheap;
} nodedata_t;

typedef struct edgedata_s {
		Agrec_t	hdr;
		double	length;
		unsigned char initialized;
} edgedata_t;

static double getlength(Agedge_t *e)
{
	edgedata_t	*data;
	data = (edgedata_t*)(e->base.data);
	if (!data->initialized) {
		data->length = atof(agget(e,"len"));
		if (data->length <= 0) data->length = 1.0;
		data->initialized = 1;
	}
	return data->length;
}

static double getdist(Agnode_t *n)
{
	nodedata_t	*data;
	data = (nodedata_t*)(n->base.data);
	return data->dist;
}

static void setdist(Agnode_t *n, double dist)
{
	nodedata_t	*data;
	data = (nodedata_t*)(n->base.data);
	data->dist = dist;
}

static int cmpf(Dt_t *d, void *key1, void *key2, Dtdisc_t *disc)
{
	double	t;
	t = getdist((Agnode_t*)key1) - getdist((Agnode_t*)key2);
	if (t < 0) return -1;
	if (t > 0) return 1;
	if (key1 < key2) return -1;
	if (key1 > key2) return 1;
	return 0;
}

static Dtdisc_t MyDisc = {
	0,			/* int key */
	0,			/* int size */
	-1,			/* int link */
	0,			/* Dtmake_f makef */
	0,			/* Dtfree_f freef */
	cmpf,		/* Dtcompar_f comparf */
	0,			/* Dthash_f hashf */
	0,			/* Dtmemory_f memoryf */
	0			/* Dtevent_f eventf */
};

static void insert(Dict_t *Q, Agnode_t *n)
{
	dtinsert(Q,n);
}

static Dict_t *queue_nodes(Agraph_t *G, Agnode_t *src)
{
	Agnode_t	*n;

	Dict_t *Q = dtopen(&MyDisc,Dtoset);
	for (n = agfstnode(G); n; n = agnxtnode(n)) 
			setdist(n,BIG);
	setdist(src,0);
	for (n = agfstnode(G); n; n = agnxtnode(n))
			insert(Q,n);
	return Q;
}

static Agnode_t *extract_min(Dict_t *Q)
{
		Agnode_t	*rv;
		rv = dtfirst(Q);
		dtdelete(Q,rv);
		return rv;
}

static void update(Dict_t *Q, Agnode_t *dest, Agnode_t *src, double len)
{
		double	newlen;

		newlen = getdist(src) + len;
		if (newlen < getdist(dest)) {
			dtdelete(Q,dest);
			setdist(dest,newlen);
			dtinsert(Q,dest);
		}
}

static void pre(Agraph_t *g)
{
	aginit(g,AGNODE,"dijkstra",sizeof(nodedata_t),1);
	aginit(g,AGEDGE,"dijkstra",sizeof(edgedata_t),1);
}

static void post(Agraph_t *g)
{
	Agnode_t	*v;
	char		buf[16];
	Agsym_t		*sym;
	double		dist,maxdist = 0.0;

	sym = agattr(g,AGNODE,"dist","");
	for (v = agfstnode(g); v; v = agnxtnode(v)) {
		dist = getdist(v);
		sprintf(buf,"%.3lf",dist);
		if (maxdist < dist) maxdist = dist;
		agxset(v,sym,buf);
	}
	sprintf(buf,"%.3lf",maxdist);
	sym = agattr(g,AGRAPH,"maxdist",buf);
	agclean(g,AGNODE,"dijkstra");
	agclean(g,AGEDGE,"dijkstra");
}

void dijkstra(Agraph_t *G, Agnode_t *n)
{
	Dict_t *Q;
	Agnode_t *u;
	Agedge_t *e;

	pre(G);
	Q = queue_nodes(G,n);
	while ((u = extract_min(Q))) {
		for (e = agfstedge(u); e; e = agnxtedge(e,u)) {
			update(Q,e->node,u,getlength(e));
		}
	}
	post(G);
}

#ifdef NOTDEF
int main(int argc, char **argv)
{
	Agraph_t *g;

	while ((g = agread(stdin,0))) {
		dijkstra(g,agfstnode(g));
		agwrite(g,stdout);
	}
	return 1;
}
#endif

static char **Files;
static char **Nodes;

static char* useString =
"Usage: dijkstra [-?] <files>\n\
  -? - print usage\n\
If no files are specified, stdin is used\n";

static void
usage (int v)
{
    printf (useString);
    exit (v);
}

static void
init (int argc, char* argv[])
{
  int i,c;

  CmdName = argv[0];
  while ((c = getopt(argc, argv, ":?")) != -1) {
    switch (c) {
    case '?':
      if (optopt == '?') usage(0);
      else fprintf(stderr,"%s: option -%c unrecognized - ignored\n",CmdName, c);
      break;
    }
  }
  argv += optind;
  argc -= optind;

  Files = malloc(sizeof(char*) * argc/2 + 1);
  Nodes = malloc(sizeof(char*) * argc/2 + 1);
  for (i = 0; i + 1 < argc; i += 2) {
	  	Nodes[i/2] = argv[i];
	  	Files[i/2] = argv[i+1];
  }
  Nodes[i/2] = Files[i/2] = 0;
}

static Agraph_t*
gread (FILE* fp)
{
	  return agread(fp,(Agdisc_t*)0);
}

int main(int argc,char **argv)
{
    Agraph_t    *g;
	Agnode_t	*n;
    ingraph_state ig;
	int			i = 0;
	int			code = 0;

    init (argc, argv);
    newIngraph (&ig, Files, gread);

    while ((g = nextGraph(&ig)) != 0) {
	  if ((n = agnode(g,Nodes[i],0)))
		  dijkstra(g,n);
	  else {
		  fprintf(stderr,"%s: no such node as %s in graph %s in %s\n",CmdName,Nodes[i],agnameof(g),fileName(&ig));
		  code = 1;
	  }
      agwrite(g,stdout);
      fflush(stdout);
      agclose (g);
	  i++;
    }
    exit(code);
}
