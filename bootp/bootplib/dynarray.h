
/*
 * dynarray.h
 * - simple array "object" that handles elements of the same size,
 *   grows dynamically as elements are added
 */

/* 
 * Modification History
 *
 * December 6, 1999	Dieter Siegmund (dieter@apple)
 * - initial revision
 */

#include <mach/boolean.h>

/* the initial number of elements in the list */
#define DYNARRAY_NUMBER		16

#import "ptrlist.h"

typedef void dynarray_free_func_t(void *);
typedef void * dynarray_copy_func_t(void * source);

typedef struct {
    ptrlist_t			list;
    dynarray_free_func_t *	free_func;
    dynarray_copy_func_t *	copy_func;
} dynarray_t;

void		dynarray_init(dynarray_t * list, 
			      dynarray_free_func_t * free_func,
			      dynarray_copy_func_t * copy_func);
boolean_t	dynarray_add(dynarray_t * list, void * element);
boolean_t	dynarray_remove(dynarray_t * list, int i, void * * element_p);
void		dynarray_free(dynarray_t * list);
boolean_t	dynarray_free_element(dynarray_t * list, int i);
boolean_t	dynarray_dup(dynarray_t * source, dynarray_t * copy);
int		dynarray_count(dynarray_t * list);
void *		dynarray_element(dynarray_t * list, int i);
int		dynarray_index(dynarray_t * list, void * element);
