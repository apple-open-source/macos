/* $Xorg: list.h,v 1.4 2001/02/09 02:06:03 xorgcvs Exp $ */
/** ------------------------------------------------------------------------
	This file contains routines for manipulating generic lists.
	Lists are implemented with a "harness".  In other words, each
	node in the list consists of two pointers, one to the data item
	and one to the next node in the list.  The head of the list is
	the same struct as each node, but the "item" ptr is used to point
	to the current member of the list (used by the first_in_list and
	next_in_list functions).

Copyright 1994 Hewlett-Packard Co.
Copyright 1996, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

    -------------------------------------------------------------------- **/

#ifndef LIST_DEF
#define LIST_DEF

#include <X11/Xfuncproto.h>
#define LESS	-1
#define EQUAL	0
#define GREATER	1
#define DUP_WHOLE_LIST	0
#define START_AT_CURR	1

typedef struct _list_item {
    struct _list_item *next;
    union {
	void *item;		 /* in normal list node, pts to data */
	struct _list_item *curr; /* in list head, pts to curr for 1st, next */
    } ptr;
} list, list_item, *list_ptr;

typedef void (*DESTRUCT_FUNC_PTR)(
#if NeedFunctionPrototypes
void *
#endif
);

void zero_list( 
#if NeedFunctionPrototypes
          list_ptr 
#endif
    );
int add_to_list (
#if NeedFunctionPrototypes
          list_ptr , void *
#endif
    );
list_ptr new_list (
#if NeedFunctionPrototypes
          void
#endif
    );
list_ptr dup_list_head (
#if NeedFunctionPrototypes
          list_ptr , int 
#endif
    );
unsigned int list_length( 
#if NeedFunctionPrototypes
          list_ptr 
#endif
    );
void *delete_from_list (
#if NeedFunctionPrototypes
          list_ptr , void *
#endif
    );
void delete_list( 
#if NeedFunctionPrototypes
          list_ptr , int 
#endif
    );
void delete_list_destroying (
#if NeedFunctionPrototypes
          list_ptr , DESTRUCT_FUNC_PTR
#endif
    );
void *first_in_list (
#if NeedFunctionPrototypes
          list_ptr 
#endif
    );
void *next_in_list (
#if NeedFunctionPrototypes
          list_ptr 
#endif
    );
int list_is_empty (
#if NeedFunctionPrototypes
          list_ptr 
#endif
    );

#endif
