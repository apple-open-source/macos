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
#include	"stack.h"
#include	"circular.h"

nstack_t*
mkStack()
{
	nstack_t* s;

	s = NEW(nstack_t);

	s->top = NULL;
	s->sz = 0;
	return s;
}

void
freeStack(nstack_t* s)
{
	free(s);
}

void
stackPush(nstack_t* s, Agnode_t* n)
{
	SET_ONSTACK(n);
	NEXT(n) = s->top;
	s->top = n;
	s->sz += 1;
}

Agnode_t*
stackPop(nstack_t* s)
{
	Agnode_t* top = s->top;

	if(top) {
		assert (s->sz > 0);
		UNSET_ONSTACK(top);
		s->top = NEXT(top);
		s->sz -= 1;
	}
	else {
		assert(0);
	}

	return top;
}

int
stackSize(nstack_t* s)
{
	return s->sz;
}

/* stackCheck:
 * Return true if n in on the stack.
 */
int
stackCheck(nstack_t* s, Agnode_t* n)
{
	return ONSTACK(n);
#ifdef OLD
	stackitem_t* top = s->top;
	Agnode_t* node;

	while(top != NULL) {
		node = top->data;
		if(node == n)
			return 1;
		top = top->next;
	}

	return 0;
#endif
}

#ifdef DEBUG
void 
printStack (nstack_t* s)
{
	Agnode_t* n;
	for (n = s->top; n; n = NEXT(n))
		fprintf (stderr, " %s", n->name);
	fprintf (stderr, "\n");

}
#endif
