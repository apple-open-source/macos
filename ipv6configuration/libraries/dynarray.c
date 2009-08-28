/*
 * Copyright (c) 2003-2008 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */


/*
 * dynarray.c
 * - simple array "object" that handles elements of the same size
 * - at init time, you give it the element size; when you add a new element,
 *   the list is allocated/grown dynamically
 */


#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <strings.h>

#include "dynarray.h"

__private_extern__ void
dynarray_init(dynarray_t * list, dynarray_free_func_t * free_func,
	      dynarray_copy_func_t * copy_func)
{
    ptrlist_init(&list->list);
    list->free_func = free_func;
    list->copy_func = copy_func;
    return;
}

__private_extern__ void
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

__private_extern__ boolean_t
dynarray_add(dynarray_t * list, void * element)
{
    return (ptrlist_add(&list->list, element));
}

__private_extern__ boolean_t
dynarray_remove(dynarray_t * list, int i, void * * element_p)
{
    return (ptrlist_remove(&list->list, i , element_p));
}

__private_extern__ boolean_t
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

__private_extern__ boolean_t
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

__private_extern__ int
dynarray_count(dynarray_t * list)
{
    return (ptrlist_count(&list->list));
}

__private_extern__ void *
dynarray_element(dynarray_t * list, int i)
{
    return (ptrlist_element(&list->list, i));
}

__private_extern__ int
dynarray_index(dynarray_t * list, void * element)
{
    return (ptrlist_index(&list->list, element));
}

