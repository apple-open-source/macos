#ifndef STACK_H
#define STACK_H

#include  <render.h>

typedef struct {
	Agnode_t*  top;
	int        sz;
} nstack_t;

extern nstack_t* mkStack();
extern void stackPush(nstack_t* s, Agnode_t* n);
extern Agnode_t* stackPop(nstack_t* s);
extern int stackSize(nstack_t* s);
extern int stackCheck(nstack_t* s, Agnode_t* n);
extern void freeStack(nstack_t* s);

#define top(sp)  ((sp)->top)

#ifdef DEBUG
extern void printStack (nstack_t*);
#endif

#endif
