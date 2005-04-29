#ifndef QUEUE_H
#define QUEUE_H

#include <agraph.h>

typedef Dt_t queue;

extern queue* mkQ(Dtmethod_t*);
extern void push(queue*, void*);
extern void* pop(queue*,int remove);
extern void freeQ(queue*);

/* pseudo-functions:
extern queue* mkStack();
extern queue* mkQueue();
extern void* pull(queue*);
extern void* head(queue*);
 */

#define mkStack()  mkQ(Dtstack)
#define mkQueue()  mkQ(Dtqueue)
#define pull(q)  (pop(q,1))
#define head(q)  (pop(q,0))

#endif
