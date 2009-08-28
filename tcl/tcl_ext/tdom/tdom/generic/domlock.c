/*----------------------------------------------------------------------------
|   Copyright (C) 1999  Jochen C. Loewer (loewerj@hotmail.com)
+-----------------------------------------------------------------------------
|
|   $Id: domlock.c,v 1.9 2005/06/10 00:21:57 rolf Exp $
|
|   The contents of this file are subject to the Mozilla Public License
|   Version 1.1 (the "License"); you may not use this file except in
|   compliance with the License. You may obtain a copy of the License at
|   http://www.mozilla.org/MPL/
|
|   Software distributed under the License is distributed on an "AS IS"
|   basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
|   License for the specific language governing rights and limitations
|   under the License.
|
|   The Original Code is tDOM.
|
|   The Initial Developer of the Original Code is Jochen Loewer
|   Portions created by Jochen Loewer are Copyright (C) 1998, 1999
|   Jochen Loewer. All Rights Reserved.
|
|   This implements many-readers/single-writer locking of DOM documents.
|
|   Contributor(s):
|       May02  Zoran Vasiljevic  Added this file.
|
\---------------------------------------------------------------------------*/

#ifdef TCL_THREADS

#include <dom.h>


/*----------------------------------------------------------------------------
|   Global list of document lock structures. These should be finalized
|   only on application exit.
|
\---------------------------------------------------------------------------*/
static Tcl_Mutex lockMutex = 0;
static domlock *domLocks = NULL;


/*----------------------------------------------------------------------------
|   Lock the document according to passed flag
|
\---------------------------------------------------------------------------*/
void
domLocksLock(domlock *dl, int how)
{

    Tcl_MutexLock(&dl->mutex);

    switch (how) {
    case LOCK_READ:
        while (dl->lrcnt < 0 || dl->numwr > 0) {
            dl->numrd++;
            Tcl_ConditionWait(&dl->rcond, &dl->mutex, NULL);
            dl->numrd--;
        }
        dl->lrcnt++;
        break;

    case LOCK_WRITE:
        while (dl->lrcnt != 0) {
            dl->numwr++;
            Tcl_ConditionWait(&dl->wcond, &dl->mutex, NULL);
            dl->numwr--;
        }
        dl->lrcnt = -1; /* This designates the sole writer */
        break;
    }

    Tcl_MutexUnlock(&dl->mutex);
}


/*----------------------------------------------------------------------------
|   Unlock the previously locked document.
|
\---------------------------------------------------------------------------*/
void
domLocksUnlock(domlock *dl)
{
    Tcl_MutexLock(&dl->mutex);

    if (--dl->lrcnt < 0) {
        dl->lrcnt = 0;
    }
    if (dl->numwr) {
        Tcl_ConditionNotify(&dl->wcond);
    } else if (dl->numrd) {
        Tcl_ConditionNotify(&dl->rcond);
    }

    Tcl_MutexUnlock (&dl->mutex);
}


/*----------------------------------------------------------------------------
|   Associate a lock with the document..
|
\---------------------------------------------------------------------------*/
void
domLocksAttach(domDocument *doc)
{
    domlock *dl;

    Tcl_MutexLock(&lockMutex);
    
    dl = domLocks;
    if (dl == NULL) {
        dl = (domlock*)MALLOC(sizeof(domlock));
        memset(dl, 0, sizeof(domlock));
    } else {
        domLocks = dl->next;
    }

    dl->doc = doc;
    doc->lock = dl;

    Tcl_MutexUnlock(&lockMutex);
}


/*----------------------------------------------------------------------------
|   Divorce DOM document from its lock. The lock structure is not
|   disposed and may be used for locking other documents.
|
\---------------------------------------------------------------------------*/
void
domLocksDetach(domDocument *doc)
{
    domlock *dl = doc->lock;

    Tcl_MutexLock(&lockMutex);

    if (dl->doc != doc) {
        domPanic("document lock mismatch");
    }

    dl->next = domLocks;
    domLocks = dl;

    dl->doc = NULL;
    doc->lock = NULL;

    Tcl_MutexUnlock(&lockMutex);
}


/*----------------------------------------------------------------------------
|   Reclaim storage used for lock structures. This should be done only
|   on application exit.
|
\---------------------------------------------------------------------------*/
void
domLocksFinalize(ClientData dummy)
{
    domlock *tmp, *dl;

    Tcl_MutexLock(&lockMutex);

    dl = domLocks;

    while (dl != NULL) {
        Tcl_MutexFinalize(&dl->mutex);
        Tcl_ConditionFinalize(&dl->rcond);
        Tcl_ConditionFinalize(&dl->wcond);
        tmp = dl;
        dl  = dl->next;
        FREE((char*)tmp);
    }
    domLocks = NULL;

    Tcl_MutexUnlock(&lockMutex);
}

#endif /* TCL_THREADS */
