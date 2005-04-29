#ifndef NODESET_H
#define NODESET_H

#include  <render.h>

typedef struct {
	Dtlink_t   link;
	Agnode_t*  np;
} nsitem_t;

typedef Dt_t nodeset_t;

extern nodeset_t* mkNodeset();
extern void freeNodeset(nodeset_t*);
extern void clearNodeset(nodeset_t*);
extern void insertNodeset(nodeset_t* ns, Agnode_t* n);
extern void removeNodeset(nodeset_t*, Agnode_t* n);
extern int sizeNodeset(nodeset_t* ns);

#ifdef DEBUG
extern void printNodeset(nodeset_t*);
#endif

#endif
