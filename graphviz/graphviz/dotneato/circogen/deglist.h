#ifndef DEGLIST_H
#define DEGLIST_H

/* List of nodes sorted by increasing degree */

#include  <render.h>

typedef Dt_t deglist_t;

extern deglist_t* mkDeglist();
extern void freeDeglist(deglist_t* list);
extern void insertDeglist(deglist_t* list, Agnode_t* n);
extern void removeDeglist(deglist_t* list, Agnode_t* n);
extern Agnode_t* firstDeglist (deglist_t*);

#ifdef DEBUG
extern void printDeglist (deglist_t*);
#endif

#endif
