#ifndef EDGELIST_H
#define EDGELIST_H

#include  <render.h>

typedef struct edgelistitem {
	Dtlink_t    link;
	Agedge_t*	edge;
} edgelistitem;

typedef Dt_t edgelist;

extern edgelist* init_edgelist();
extern void add_edge(edgelist* list, Agedge_t* e);
extern void remove_edge(edgelist* list, Agedge_t* e);
extern void free_edgelist(edgelist* list);
extern int size_edgelist(edgelist* list);
#ifdef DEBUG
extern void print_edge(edgelist*);
#endif

#endif
