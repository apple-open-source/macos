
/*
 * dynarray.c
 * - simple array "object" that handles elements of the same size
 * - at init time, you give it the element size; when you add a new element,
 *   the list is allocated/grown dynamically
 */

/* 
 * Modification History
 *
 * December 6, 1999	Dieter Siegmund (dieter@apple)
 * - initial revision
 */

#import <unistd.h>
#import <stdlib.h>
#import <stdio.h>
#import <sys/types.h>
#import <strings.h>

#import "dynarray.h"

void
dynarray_init(dynarray_t * list, dynarray_free_func_t * free_func,
	      dynarray_copy_func_t * copy_func)
{
    ptrlist_init(&list->list);
    list->free_func = free_func;
    list->copy_func = copy_func;
    return;
}

void
dynarray_free(dynarray_t * list)
{
    void *	element;

    while (ptrlist_remove(&list->list, 0, &element)) {
	if (element && list->free_func) {
	    (list->free_func)(element);
	}
    }
    ptrlist_free(&list->list);
    return;
}

boolean_t
dynarray_add(dynarray_t * list, void * element)
{
    return (ptrlist_add(&list->list, element));
}

boolean_t
dynarray_remove(dynarray_t * list, int i, void * * element_p)
{
    return (ptrlist_remove(&list->list, i , element_p));
}

boolean_t
dynarray_free_element(dynarray_t * list, int i)
{
    void * p;

    if (dynarray_remove(list, i, &p)) {
	if (p && list->free_func) {
	    (*list->free_func)(p);
	}
	return (TRUE);
    }
    return (FALSE);
}

boolean_t
dynarray_dup(dynarray_t * dest, dynarray_t * source)
{
    int i;

    dest->copy_func = source->copy_func;
    dest->free_func = source->free_func;
    ptrlist_init(&dest->list);

    for (i = 0; i < ptrlist_count(&source->list); i++) {
	void * element = ptrlist_element(&source->list, i);

	if (element && dest->copy_func) {
	    element = (*dest->copy_func)(element);
	}
	ptrlist_add(&dest->list, element);
    }
    return (TRUE);
}

#if 0
/* concatenates extra onto list */
boolean_t
dynarray_concat(dynarray_t * list, dynarray_t * extra)
{
    if (extra->count == 0)
	return (TRUE);
    if (list->el_size != extra->el_size)
	return (FALSE);

    if ((extra->count + list->count) > list->size) {
	list->size = extra->count + list->count;
	if (list->array == NULL)
	    list->array = malloc(list->el_size * list->size);
	else
	    list->array = realloc(list->array, 
				  list->el_size * list->size);
    }
    if (list->array == NULL)
	return (FALSE);
    bcopy(extra->array, list->array + list->count, 
	  extra->count * list->el_size);
    list->count += extra->count;
    return (TRUE);
}
#endif 0

int
dynarray_count(dynarray_t * list)
{
    return (ptrlist_count(&list->list));
}

void *
dynarray_element(dynarray_t * list, int i)
{
    return (ptrlist_element(&list->list, i));
}

int
dynarray_index(dynarray_t * list, void * element)
{
    return (ptrlist_index(&list->list, element));
}

