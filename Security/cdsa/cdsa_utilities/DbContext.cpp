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


#ifdef __MWERKS__
#define _CPP_DBCONTEXT
#endif
#include <Security/DbContext.h>

#include <Security/Database.h>

#include <Security/cssmerr.h>
#include <Security/utilities.h>

DbContext::DbContext (Database &inDatabase,
                      DatabaseSession &inDatabaseSession,
                      CSSM_DB_ACCESS_TYPE inAccessRequest,
                      const CSSM_ACCESS_CREDENTIALS *inAccessCred) :
    mDatabase (inDatabase),
    mDatabaseSession (inDatabaseSession),
    mAccessRequest (inAccessRequest)
{
    // XXX Copy the ACL.
    //mAccessCred = inAccessCred;
}

DbContext::~DbContext ()
{
    //delete mAccessCred;
    // XXX How do we delete these?
}

#if 0
CSSM_HANDLE
DbContext::dataGetFirst(const DLQuery *inQuery,
                        CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR inoutAttributes,
                        CssmData *inoutData,
                        CSSM_DB_UNIQUE_RECORD_PTR &outUniqueRecord)
{
    auto_ptr<DbQuery> aQuery(mDatabase.makeQuery(const DLQuery *inQuery));
    try
    {
        mDatabase.dataGetNext(*aQuery, inoutAttributes, inoutData, outUniqueRecord);

        StLock<Mutex> _(mDbQuerySet);
        mDbQuerySet.insert(aQuery.get());
    }
    catch(...)
    {
        mDatabase.dataAbortQuery(*aQuery);
        throw;
    }

    return reinterpret_cast<CSSM_HANDLE>(aQuery.release());
}

void
DbContext::dataGetNext(CSSM_HANDLE inResultsHandle,
                       CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR inoutAttributes,
                       CssmData *inoutData,
                       CSSM_DB_UNIQUE_RECORD_PTR &outUniqueRecord)
{
    DbQuery *aQuery = reinterpret_cast<DbQuery *>(inResultsHandle);
    {
        StLock<Mutex> _(mDbQuerySet);
        DbQuerySet::iterator it = mDbQuerySet.find(aQuery);
        if (it == mDbContextMap.end())
            CssmError::throwMe(CSSMERR_DL_INVALID_RESULTS_HANDLE);
    }

    try
    {
        mDatabase.dataGetNext(*aQuery, inoutAttributes, inoutData, outUniqueRecord);
    }
    catch(...)
    {
        {
            StLock<Mutex> _(mDbQuerySet);
            mDbQuerySet.erase(aQuery);
        }
        try
        {
            mDatabase.dataAbortQuery(*aQuery);
        }
        catch(...) {}
        delete aQuery;
        throw;
    }
}

void
DbContext::dataAbortQuery(CSSM_HANDLE inResultsHandle)
{
    DbQuery *aQuery = reinterpret_cast<DbQuery *>(inResultsHandle);
    {
        StLock<Mutex> _(mDbQuerySet);
        DbQuerySet::iterator it = mDbQuerySet.find(aQuery);
        if (it == mDbContextMap.end())
            CssmError::throwMe(CSSMERR_DL_INVALID_RESULTS_HANDLE);
        mDbContextMap.erase(it);
    }

    try
    {
        mDatabase.dataAbortQuery(*aQuery);
    }
    catch(...)
    {
        delete aQuery;
        throw;
    }
    delete aQuery;
}
#endif
