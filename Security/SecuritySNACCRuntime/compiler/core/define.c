/*
 * compiler/core/define.c - keeps a list of things that have been defined
 *            and provided means for checking if something has been
 *            defined
 *
 * MS 92
 * Copyright (C) 1991, 1992 Michael Sample
 *            and the University of British Columbia
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * $Header: /cvs/root/Security/SecuritySNACCRuntime/compiler/core/Attic/define.c,v 1.1 2001/06/20 21:27:56 dmitch Exp $
 * $Log: define.c,v $
 * Revision 1.1  2001/06/20 21:27:56  dmitch
 * Adding missing snacc compiler files.
 *
 * Revision 1.1.1.1  1999/03/16 18:06:46  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.4  1997/10/10 13:43:15  wan
 * Corrected bug in generic table decoder wrt. indefinite length elements
 * Corrected compiler access to freed memory (bug reported by Markku Savela)
 * Broke asnwish.c into two pieces so that one can build ones on wish
 * Added beredit tool (based on asnwish, allowes to edit BER messages)
 *
 * Revision 1.3  1995/07/25 19:41:21  rj
 * changed `_' to `-' in file names.
 *
 * Revision 1.2  1994/09/01  00:27:38  rj
 * snacc_config.h removed.
 *
 * Revision 1.1  1994/08/28  09:48:58  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */

#include "asn-incl.h"
#include "mem.h"
#include "define.h"

/* for CompareOids from snacc_util.c*/
int CompareOids PROTO ((OID *oid1, OID *oid2));


/* cmp routine for a null terminated string object type */
int
StrObjCmp PARAMS ((s1, s2),
    void *s1 _AND_
    void *s2)
{
    if (strcmp ((char*)s1, (char*) s2) == 0)
        return TRUE;
    else
        return FALSE;
}

/* cmp routine for a integer object type */
int
IntObjCmp PARAMS ((s1, s2),
    void *s1 _AND_
    void *s2)
{
    if (*((int*) s1) == *((int*) s2))
        return TRUE;
    else
        return FALSE;
}


/* cmp routine for a OID object type */
int
OidObjCmp PARAMS ((o1, o2),
    void *o1 _AND_
    void *o2)
{
    return CompareOids ((OID*)o1, (OID*)o2);
}

/* special cmp routine - compares the pointers themselves */
int
ObjPtrCmp PARAMS ((s1, s2),
    void *s1 _AND_
    void *s2)
{
    if (s1 == s2)
        return TRUE;
    else
        return FALSE;
}


DefinedObj*
NewObjList()
{
    return NULL;
}

/*
 * puts the given object into the give object list
 * does not check for duplicates - you should do that
 * before calling this - if you care.
 */
void
DefineObj PARAMS ((objListHndl, obj),
    DefinedObj **objListHndl _AND_
    void *obj)
{
    DefinedObj *new;

    new =  MT (DefinedObj);
    new->obj = obj;

    /* insert new one at head */
    new->next = *objListHndl;
    *objListHndl = new;

}  /* DefineObj */


/*
 * removes the first identical object from the list
 * - if you are allowing duplicates use another routine.
 *   this only removes the first for efficiency reasons - all
 *   current usage of the DefineObj stuff does not allow duplicates.
 */
void
UndefineObj PARAMS ((objListHndl, obj, cmpRoutine),
    DefinedObj **objListHndl _AND_
    void *obj _AND_
    CmpObjsRoutine cmpRoutine)
{
    DefinedObj *objListPtr;
    DefinedObj **prevHndl;

    objListPtr = *objListHndl;

    prevHndl = objListHndl;
    for ( ; objListPtr != NULL; objListPtr = *prevHndl)
    {
        if (cmpRoutine (objListPtr->obj, obj))
        {
            /* found object, now remove it */
            *prevHndl = objListPtr->next;
            Free (objListPtr);
        }
        else
            prevHndl = &objListPtr->next;
    }

}  /* UndefineObj */


/*
 * given an object list, an object and an object comparison routine,
 * ObjIsDefined returns non-zero if the given object is already in
 * the object list.  The comparison routine should take two objects and
 * return non-zero if the objects are equivalent
 */
int
ObjIsDefined PARAMS ((objListPtr, obj, cmpRoutine),
    DefinedObj *objListPtr _AND_
    void *obj _AND_
    CmpObjsRoutine cmpRoutine)
{
    for ( ; objListPtr != NULL; objListPtr = objListPtr->next)
    {
        if (cmpRoutine (objListPtr->obj, obj))
            return TRUE;
    }
    return FALSE;

}  /* ObjIsDefined */

/*
 * Frees the list holding the defined objects.
 * Does not free the objects.
 */
void
FreeDefinedObjs PARAMS ((objListHndl),
    DefinedObj **objListHndl)
{
    DefinedObj *dO;
    DefinedObj *tmpDO;

    for (dO = *objListHndl; dO != NULL; )
    {
        tmpDO = dO->next;
        Free (dO);
        dO = tmpDO;
    }
    *objListHndl = NULL;

}  /* FreeDefinedObjs */



/*
 * Frees the list holding the defined objects.
 * Does free the objects.
 */
void
FreeDefinedObjsAndContent PARAMS ((objListHndl),
    DefinedObj **objListHndl)
{
    DefinedObj *dO;
    DefinedObj *tmpDO;

    for (dO = *objListHndl; dO != NULL; )
    {
        tmpDO = dO->next;
        Free (dO->obj);
        Free (dO);
        dO = tmpDO;
    }
    *objListHndl = NULL;

}  /* FreeDefinedObjs */
