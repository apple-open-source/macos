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
 * asn_list.c  - borrowed from Murray Goldberg
 *
 * the following routines implement the list data structure
 *
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
 * $Header: /cvs/root/Security/SecuritySNACCRuntime/c-lib/src/Attic/asn-list.c,v 1.1.1.1 2001/05/18 23:14:08 mb Exp $
 * $Log: asn-list.c,v $
 * Revision 1.1.1.1  2001/05/18 23:14:08  mb
 * Move from private repository to open source repository
 *
 * Revision 1.2  2001/05/05 00:59:25  rmurphy
 * Adding darwin license headers
 *
 * Revision 1.1.1.1  1999/03/16 18:06:30  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.2  1995/07/27 08:59:36  rj
 * merged GetAsnListElmt(), a function used only by the type table code.
 *
 * changed `_' to `-' in file names.
 *
 * Revision 1.1  1994/08/28  09:45:55  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */

#include "asn-config.h"
#include "asn-list.h"

/* remove the entire list and all its nodes (not the actual list data elmts) */
/* this is set up for the snace compiler */
void
AsnListFree PARAMS ((list),
    AsnList *list)
{
    AsnListNode *node, *next;

    node = list->first;
    while (node)
    {
	next = node->next;
	Asn1Free (node);
	node = next;
    }

    Asn1Free (list);
}  /* AsnListFree */


/*
 * this routine removes the current node from the list. After removal the
 * current pointer will point to the next node in line, or NULL if the
 * removed item was at the tail of the list.
 */
void
AsnListRemove PARAMS ((list),
    AsnList *list)
{
    AsnListNode *node;

    if (list->curr)
    {
	if (list->curr->next)
	    list->curr->next->prev = list->curr->prev;
	else
	    list->last = list->curr->prev;

	if (list->curr->prev)
	    list->curr->prev->next = list->curr->next;
	else
	    list->first = list->curr->next;

	node       = list->curr;

	list->curr = list->curr->next;
	list->count--;

	Asn1Free (node);
    }
}

/*
 * this creates a new node after the current node and returns the
 * address of the memory allocated for data. The current pointer is changed
 * to point to the newly added node in the list. If the current pointer is
 * initially off the list then this operation fails.
 */
void*
AsnListAdd PARAMS ((list),
    AsnList *list)
{
    AsnListNode *newNode;
    void      *dataAddr;

    if (list->curr)
    {
	newNode  = (AsnListNode *) Asn1Alloc (sizeof (AsnListNode) + list->dataSize);
	dataAddr = (void *) &(newNode->data);

	newNode->next = list->curr->next;
	newNode->prev = list->curr;
	if (list->curr->next)
	    list->curr->next->prev = newNode;
	else
	    list->last = newNode;
	list->curr->next = newNode;

	list->curr = newNode;
	list->count++;
    }

    else
	dataAddr = NULL;

    return dataAddr;
}

/*
 * this creates a new node before the current node and returns the
 * address of the memory allocated for data. The current pointer is changed
 * to point to the newly added node in the list. If the current pointer is
 * initially off the list then this operation fails.
 */
void*
AsnListInsert PARAMS ((list),
    AsnList *list)
{
    AsnListNode *newNode;
    void      *dataAddr;

    if (list->curr)
    {
	newNode  = (AsnListNode *) Asn1Alloc (sizeof (AsnListNode) + list->dataSize);
	dataAddr = (void *) &(newNode->data);

	newNode->next = list->curr;
	newNode->prev = list->curr->prev;
	if (list->curr->prev)
	    list->curr->prev->next = newNode;
	else
	    list->first  = newNode;
	list->curr->prev = newNode;

	list->curr = newNode;
	list->count++;
    }

    else
	dataAddr = NULL;

    return dataAddr;
}


void
AsnListInit PARAMS ((list, dataSize),
    AsnList *list _AND_
    int dataSize)
{
    list->first = list->last = list->curr = NULL;
    list->count = 0;
    list->dataSize = dataSize;

}  /* AsnListInit */


AsnList*
AsnListNew PARAMS ((dataSize),
    int dataSize)
{
    AsnList *list;

    list = (AsnList *) Asn1Alloc (sizeof (AsnList));
    list->first = list->last = list->curr = NULL;
    list->count = 0;
    list->dataSize = dataSize;

    return list;
}

/*
 * backs up the current pointer by one and returns the data address of the new
 * current node. If the current pointer is off the list, the new current node
 * will be the last node of the list (unless the list is empty).
 */
void*
AsnListPrev PARAMS ((list),
    AsnList *list)
{
    void *retVal;

    if (list->curr == NULL)
	list->curr = list->last;
    else
	list->curr = list->curr->prev;

    if (list->curr == NULL)
	retVal = NULL;
    else
	retVal = (void *) &(list->curr->data);

    return retVal;
}

/*
 * advances the current pointer by one and returns the data address of the new
 * current node. If the current pointer is off the list, the new current node
 * will be the first node of the list (unless the list is empty).
 */
void*
AsnListNext PARAMS ((list),
    AsnList *list)
{
    void *retVal;

    if (list->curr == NULL)
	list->curr = list->first;
    else
	list->curr = list->curr->next;

    if (list->curr == NULL)
	retVal = NULL;
    else
	retVal = (void *) &(list->curr->data);

    return retVal;
}

/*
 * returns the data address of the last node (if there is one) and sets the
 * current pointer to this node.
 */
void*
AsnListLast PARAMS ((list),
    AsnList *list)
{
    void *retVal;

    list->curr = list->last;

    if (list->curr == NULL)
	retVal = NULL;
    else
	retVal = (void *) &(list->curr->data);

    return retVal;
}

/*
 * returns the data address of the first node (if there is one) and sets the
 * current pointer to this node.
 */
void*
AsnListFirst PARAMS ((list),
    AsnList *list)
{
    void *retVal;

    list->curr = list->first;

    if (list->curr == NULL)
	retVal = NULL;
    else
	retVal = (void *) &(list->curr->data);

    return retVal;
}

/*
 * this creates a new node at the beginning of the list and returns the
 * address of the memory allocated for data. The current pointer is changed
 * to point to the newly added node in the list.
 */
void*
AsnListPrepend PARAMS ((list),
    AsnList *list)
{
    AsnListNode *newNode;
    void      *dataAddr;

    newNode  = (AsnListNode *) Asn1Alloc (sizeof (AsnListNode) + list->dataSize);
    dataAddr = (void *) &(newNode->data);

    newNode->prev = NULL;

    if (list->first == NULL)
    {
	newNode->next = NULL;
	list->first   = list->last = newNode;
    }
    else
    {
	newNode->next     = list->first;
	list->first->prev = newNode;
	list->first       = newNode;
    }

    list->curr = newNode;
    list->count++;

    return dataAddr;
}

/*
 * this creates a new node at the end of the list and returns the
 * address of the memory allocated for data. The current pointer is changed
 * to point to the newly added node in the list.
 */
void*
AsnListAppend PARAMS ((list),
    AsnList *list)
{
    AsnListNode *newNode;
    void      *dataAddr;

    newNode  = (AsnListNode *) Asn1Alloc (sizeof (AsnListNode) + list->dataSize);
    dataAddr = (void *) &(newNode->data);

    newNode->next = NULL;

    if (list->last == NULL)
    {
        newNode->prev = NULL;
        list->first   = list->last = newNode;
    }
    else
    {
        newNode->prev     = list->last;
        list->last->next  = newNode;
        list->last        = newNode;
    }

    list->curr = newNode;
    list->count++;

    return dataAddr;
}

void*
AsnListCurr PARAMS ((list),
    AsnList *list)
{
    void *retVal;

    if (list->curr)
	retVal = (void *) &(list->curr->data);
    else
	retVal = NULL;

    return retVal;
}

int
AsnListCount PARAMS ((list),
    AsnList *list)
{
    return list->count;
}


AsnList*
AsnListConcat PARAMS ((l1,l2),
    AsnList *l1 _AND_
    AsnList *l2)
{
    if (l2->count == 0)
        return l1;

    if (l1->count == 0)
    {
        l1->count = l2->count;
        l1->last = l2->last;
        l1->first = l2->first;
        l1->curr = l1->first;
    }
    else
    {
        l1->count += l2->count;
        l1->last->next = l2->first;
        l2->first->prev = l1->last;
        l1->last = l2->last;
    }

    return l1;
}


/*
 * Returns the index (starting a 0 for the first elmt)
 * of the given elmt in the given list
 * returns -1 if the elmt is not in the list
 * Assumes that the list node contains a single pointer
 */
long int
GetAsnListElmtIndex PARAMS ((elmt, list),
    void *elmt _AND_
    AsnList *list)
{
    void *tmp;
    void *tmpElmt;
    long int index;

    index = 0;
    tmp = (void*) CURR_LIST_NODE (list);
    FOR_EACH_LIST_ELMT (tmpElmt, list)
    {
        if (tmpElmt == elmt)
        {
            SET_CURR_LIST_NODE (list, tmp);
            return index;
        }
        else
            index++;
    }

    SET_CURR_LIST_NODE (list, tmp);
    return -1;

}  /* GetAsnListElmtIndex */


#if TTBL
/*
 * Returns the element with the given index.
 * indexes start a 0 for the first elmt.
 * returns NULL if the index is too large.
 * Assumes that the list node contains a single pointer.
 */
void*
GetAsnListElmt PARAMS ((list, index),
    AsnList *list _AND_
    unsigned int index)
{
    void *tmp;
    void *tmpElmt;
    long int currIndex;

    if (index > LIST_COUNT (list))
        return NULL;

    currIndex = 0;
    tmp = (void*) CURR_LIST_NODE (list);
    FOR_EACH_LIST_ELMT (tmpElmt, list)
    {
        if (currIndex == index)
        {
            SET_CURR_LIST_NODE (list, tmp);
            return tmpElmt;
        }
        currIndex++;
    }
    SET_CURR_LIST_NODE (list, tmp);
    return NULL;

}  /* GetAsnListElmt */
#endif /* TTBL */
