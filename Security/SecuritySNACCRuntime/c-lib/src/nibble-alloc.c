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
 * .../c-lib/src/nibble-alloc.c - fast mem allocation for decoded values
 *
 * MS Dec 31/91
 *
 * Copyright (C) 1992 Michael Sample and the University of British Columbia
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
 * $Header: /cvs/Darwin/Security/SecuritySNACCRuntime/c-lib/src/nibble-alloc.c,v 1.1.1.1 2001/05/18 23:14:08 mb Exp $
 * $Log: nibble-alloc.c,v $
 * Revision 1.1.1.1  2001/05/18 23:14:08  mb
 * Move from private repository to open source repository
 *
 * Revision 1.2  2001/05/05 00:59:25  rmurphy
 * Adding darwin license headers
 *
 * Revision 1.1.1.1  1999/03/16 18:06:32  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.4  1997/02/28 13:39:51  wan
 * Modifications collected for new version 1.3: Bug fixes, tk4.2.
 *
 * Revision 1.3  1995/07/27 09:06:37  rj
 * use memzero that is defined in .../snacc.h to use either memset or bzero.
 *
 * changed `_' to `-' in file names.
 *
 * Revision 1.2  1994/09/01  00:07:16  rj
 * more portable .h file inclusion.
 *
 * Revision 1.1  1994/08/28  09:46:07  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */

#include "asn-config.h"

#if STDC_HEADERS || HAVE_STRING_H
#include <string.h>
/* An ANSI string.h and pre-ANSI memory.h might conflict. */
#if !STDC_HEADERS && HAVE_MEMORY_H
#include <memory.h>
#endif /* not STDC_HEADERS and HAVE_MEMORY_H */
#endif /* not STDC_HEADERS and not HAVE_STRING_H */

#include "nibble-alloc.h"


NibbleMem *nmG = NULL;

void
InitNibbleMem PARAMS ((initialSize, incrementSize),
    unsigned long int initialSize _AND_
    unsigned long int incrementSize)
{
    NibbleMem *nm;

    nm = (NibbleMem*) malloc (sizeof (NibbleMem));
    nm->incrementSize = incrementSize;

    nm->currNibbleBuf = nm->firstNibbleBuf = (NibbleBuf*)malloc (sizeof (NibbleBuf));
    nm->firstNibbleBuf->curr = nm->firstNibbleBuf->start = (char*) malloc (initialSize);
    nm->firstNibbleBuf->end = nm->firstNibbleBuf->start + initialSize;
    nm->firstNibbleBuf->next = NULL;
    memzero (nm->currNibbleBuf->start, initialSize);

    nmG = nm;/* set global */

}  /* InitNibbleAlloc */


/*
 * alloc new nibble buf, link in, reset to curr nibble buf
 */
void
ServiceNibbleFault PARAMS ((size),
    unsigned long size)
{
    NibbleMem *nm;
    unsigned long newBufSize;

    nm = nmG;

    if (size > nm->incrementSize)
        newBufSize = size;
    else
        newBufSize = nm->incrementSize;

    nm->currNibbleBuf->next = (NibbleBuf*) malloc (sizeof (NibbleBuf));
    nm->currNibbleBuf = nm->currNibbleBuf->next;
    nm->currNibbleBuf->curr = nm->currNibbleBuf->start = (char*) malloc (newBufSize);
    nm->currNibbleBuf->end = nm->currNibbleBuf->start + newBufSize;
    nm->currNibbleBuf->next = NULL;
    memzero (nm->currNibbleBuf->start, newBufSize);
} /* serviceNibbleFault */



/*
 * returns requested space filled with zeros
 */
void*
NibbleAlloc PARAMS ((size),
    unsigned long size)
{
    NibbleMem *nm;
    char *retVal;
    unsigned long ndiff;

    nm = nmG;

    if ((nm->currNibbleBuf->end - nm->currNibbleBuf->curr) < size)
         ServiceNibbleFault (size);

    retVal = nm->currNibbleBuf->curr;

    /*
     * maintain word alignment
     */
    ndiff = size % sizeof (long);
    if (ndiff != 0)
    {
        nm->currNibbleBuf->curr += size + sizeof (long) - ndiff;

        /*
         * this is a fix from Terry Sullivan <FCLTPS@nervm.nerdc.ufl.edu>
         *
         * makes sure curr does not go past the end ptr
         */
        if (nm->currNibbleBuf->curr > nm->currNibbleBuf->end)
            nm->currNibbleBuf->curr = nm->currNibbleBuf->end;
    }
    else
        nm->currNibbleBuf->curr += size;

    return retVal;
}  /* NibbleAlloc */



/*
 * frees all nibble buffers except the first,
 * resets the first to empty and zero's it
 */
void
ResetNibbleMem()
{
    NibbleMem *nm;
    NibbleBuf *tmp;
    NibbleBuf *nextTmp;

    nm = nmG;

    /*
     * reset first nibble buf
     */
    memzero (nm->firstNibbleBuf->start, nm->firstNibbleBuf->curr - nm->firstNibbleBuf->start);

    nm->firstNibbleBuf->curr = nm->firstNibbleBuf->start;

    /*
     * free incrementally added nibble bufs
     */
    for (tmp = nm->firstNibbleBuf->next; tmp != NULL; )
    {
        free (tmp->start);
        nextTmp = tmp->next;
        free (tmp);
        tmp = nextTmp;
    }

    /* From ftp://ftp.cs.ubc.ca/pub/local/src/snacc/bugs-in-1.1 */
    nm->firstNibbleBuf->next = NULL;
    nm->currNibbleBuf = nm->firstNibbleBuf;

} /* ResetNibbleMem */


/*
 * frees all nibble buffers, closing this
 * NibbleMem completely
 */
void
ShutdownNibbleMem()
{
    NibbleMem *nm;
    NibbleBuf *tmp;
    NibbleBuf *nextTmp;

    nm = nmG;
    nmG = NULL;
    /*
     * free nibble bufs
     */
    for (tmp = nm->firstNibbleBuf; tmp != NULL; )
    {
        free (tmp->start);
        nextTmp = tmp->next;
        free (tmp);
        tmp = nextTmp;
    }

    free (nm);
} /* ShutdownNibbleMem */
