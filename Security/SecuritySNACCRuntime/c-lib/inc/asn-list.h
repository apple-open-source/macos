/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/*
 * asn_list.h
 *
 *  ---------
 *  | AsnList |
 *  |  last |-------------------------------------------|
 *  |  curr |--------------------------|                |
 *  |  first|--------|                 |                |
 *  ---------        |                 |                |
 *                   V                 V                V
 *                ---------        ---------        ---------
 *                |AsnListNode       |AsnListNode       |AsnListNode
 *                | next  |---...->|  next |--...-->| next  |-----|i.
 *         .i|----| prev  |<--...--|  prev |<--...--| prev  |
 *                | data  |        |  data |        | data  |
 *                ---------        ---------        ---------
 *
 * Originally by Murray Goldberg
 * Modified for ASN.1 use.
 * MS 92
 * Copyright (C) 1992 the University of British Columbia
 *
 * This library is free software; you can redistribute it and/or
 * modify it provided that this copyright/license information is retained
 * in original form.
 *
 * If you modify this file, you must clearly indicate your changes.
 *
 * This source code is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Header: /cvs/Darwin/Security/SecuritySNACCRuntime/c-lib/inc/asn-list.h,v 1.1.1.1 2001/05/18 23:14:08 mb Exp $
 * $Log: asn-list.h,v $
 * Revision 1.1.1.1  2001/05/18 23:14:08  mb
 * Move from private repository to open source repository
 *
 * Revision 1.2  2001/05/05 00:59:22  rmurphy
 * Adding darwin license headers
 *
 * Revision 1.1.1.1  1999/03/16 18:06:20  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.3  1995/07/24 21:01:14  rj
 * changed `_' to `-' in file names.
 *
 * Revision 1.2  1994/10/08  01:40:22  rj
 * it is unwise to #define unbalanced if()s! (fixed.)
 * three declarations added.
 *
 * Revision 1.1  1994/08/28  09:21:30  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */

#ifndef _asn_list_h_
#define _asn_list_h_

typedef struct AsnListNode
{
    struct AsnListNode	*prev;
    struct AsnListNode	*next;
    void		*data;		/* this must be the last field of this structure  */
} AsnListNode;

typedef struct AsnList
{
    AsnListNode		*first;
    AsnListNode		*last;
    AsnListNode		*curr;
    int			count;		/* number of elements in list               */
    int			dataSize;	/* space required in each node for the data */
} AsnList;

#define FOR_EACH_LIST_ELMT( elmt,  al)\
  if (!(al))\
    ;\
  else\
    for ((al)->curr = (al)->first; (al)->curr && ((elmt) = (void *)(al)->curr->data); (al)->curr = (al)->curr->next)

#define FOR_EACH_LIST_ELMT_RVS( elmt,  al)\
  if (!(al))\
    ;\
  else\
    for ((al)->curr = (al)->last; (al)->curr && ((elmt) = (void *)(al)->curr->data); (al)->curr = (al)->curr->prev)


#define FOR_REST_LIST_ELMT( elmt,  al)\
  if (!(al))\
    ;\
  else\
    for (; (al)->curr && ((elmt) = (void *)(al)->curr->data); (al)->curr = (al)->curr->next)

#define FOR_REST_LIST_ELMT_RVS( elmt,  al)\
  if (!(al))\
    ;\
  else\
    for (; ((al)->curr && ((elmt) = (void *)(al)->curr->data); (al)->curr = (al)->curr->prev)

/*
 * The following macros return the pointer stored in the
 * data part of the listNode.  The do not change the current
 * list pointer.
 */
#define CURR_LIST_ELMT( al)			((al)->curr->data)
#define NEXT_LIST_ELMT( al)			((al)->curr->next->data)
#define PREV_LIST_ELMT( al)			((al)->curr->prev->data)
#define LAST_LIST_ELMT( al)			((al)->last->data)
#define FIRST_LIST_ELMT( al)			((al)->first->data)
#define LIST_EMPTY( al)				((al)->count == 0)
#define LIST_COUNT( al)				((al)->count)

/*
 * list nodes are the parts of the list that contain ptrs/data
 * to/of the list elmts.
 */
#define CURR_LIST_NODE( al)			((al)->curr)
#define FIRST_LIST_NODE( al)			((al)->first)
#define LAST_LIST_NODE( al)			((al)->last)
#define PREV_LIST_NODE( al)			((al)->curr->prev)
#define NEXT_LIST_NODE( al)			((al)->curr->next)
#define SET_CURR_LIST_NODE( al, listNode)	((al)->curr = (listNode))

void	AsnListRemove PROTO ((AsnList *));
void	*AsnListAdd PROTO ((AsnList *));
void	*AsnListInsert PROTO ((AsnList *));
void	AsnListInit PROTO ((AsnList *list, int dataSize));
AsnList	*AsnListNew PROTO ((int));
void	*AsnListPrev PROTO ((AsnList *));
void	*AsnListNext PROTO ((AsnList *));
void	*AsnListLast PROTO ((AsnList *));
void	*AsnListFirst PROTO ((AsnList *));
void	*AsnListPrepend PROTO ((AsnList *));
void	*AsnListAppend PROTO ((AsnList *));
void	*AsnListCurr PROTO ((AsnList *));
int	AsnListCount PROTO ((AsnList *));
AsnList	*AsnListConcat PROTO ((AsnList *, AsnList *));
long int GetAsnListElmtIndex PROTO ((void *elmt,AsnList *list));
void	AsnListFree PROTO (( AsnList *));
void	*GetAsnListElmt PROTO ((AsnList *list, unsigned int index));

#endif /* conditional include */
