
#ifndef _S_PTRLIST_H
#define _S_PTRLIST_H

#include <mach/boolean.h>

/* the initial number of elements in the list */
#define PTRLIST_NUMBER		16

typedef struct {
    void * *	array;	/* malloc'd array of pointers */
    int		size;	/* number of elements in array */
    int		count;	/* number of occupied elements */
} ptrlist_t;

void		ptrlist_init(ptrlist_t * list);
void		ptrlist_init_size(ptrlist_t * list, int size);
boolean_t	ptrlist_add(ptrlist_t * list, void * element);
boolean_t	ptrlist_insert(ptrlist_t * list, void * element, int i);
void		ptrlist_free(ptrlist_t * list);
boolean_t	ptrlist_dup(ptrlist_t * dest, ptrlist_t * source);
boolean_t	ptrlist_concat(ptrlist_t * list, ptrlist_t * extra);
int		ptrlist_count(ptrlist_t * list);
void *		ptrlist_element(ptrlist_t * list, int i);
boolean_t	ptrlist_remove(ptrlist_t * list, int i, void * * ret);
int		ptrlist_index(ptrlist_t * list, void * element);

#endif _S_PTRLIST_H
