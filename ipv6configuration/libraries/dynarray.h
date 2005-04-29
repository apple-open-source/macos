#ifndef _S_DYNARRAY_H
#define _S_DYNARRAY_H

/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * dynarray.h
 * - simple array "object" that handles elements of the same size,
 *   grows dynamically as elements are added
 */

#include "ptrlist.h"


/* the initial number of elements in the list */
#define DYNARRAY_NUMBER		16

typedef void dynarray_free_func_t(void *);
typedef void * dynarray_copy_func_t(void * source);

typedef struct dynarray_s {
    ptrlist_t				list;
    dynarray_free_func_t *	free_func;
    dynarray_copy_func_t *	copy_func;
} dynarray_t;

void		dynarray_init(dynarray_t * list, dynarray_free_func_t * free_func,
				dynarray_copy_func_t * copy_func);
boolean_t	dynarray_add(dynarray_t * list, void * element);
boolean_t	dynarray_remove(dynarray_t * list, int i, void * * element_p);
void		dynarray_free(dynarray_t * list);
boolean_t	dynarray_free_element(dynarray_t * list, int i);
boolean_t	dynarray_dup(dynarray_t * source, dynarray_t * copy);
int			dynarray_count(dynarray_t * list);
void *		dynarray_element(dynarray_t * list, int i);
int			dynarray_index(dynarray_t * list, void * element);

#endif _S_DYNARRAY_H
