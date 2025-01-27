/*
 * Copyright (c) 2000-2001,2011,2014 Apple Inc. All Rights Reserved.
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


#ifndef _DBCONTEXT_H_
#define _DBCONTEXT_H_  1

#include <security_cdsa_plugin/Database.h>
#include <security_cdsa_utilities/handleobject.h>

#ifdef _CPP_DBCONTEXT
# pragma export on
#endif

namespace Security
{

class DatabaseSession;

class DbContext : public HandleObject
{
	NOCOPY(DbContext)
public:
    Database &mDatabase;
    DatabaseSession &mDatabaseSession;

    DbContext(Database &inDatabase,
              DatabaseSession &inDatabaseSession,
              CSSM_DB_ACCESS_TYPE inAccessRequest,
              const CSSM_ACCESS_CREDENTIALS *inAccessCred);

    virtual ~DbContext();

    CSSM_HANDLE
    dataGetFirst(const CssmQuery *inQuery,
                      CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR inoutAttributes,
                      CssmData *inoutData,
                      CSSM_DB_UNIQUE_RECORD_PTR &outUniqueRecord);

    void
    dataGetNext(CSSM_HANDLE inResultsHandle,
                CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR inoutAttributes,
                CssmData *inoutData,
                CSSM_DB_UNIQUE_RECORD_PTR &outUniqueRecord);

    void
    dataAbortQuery(CSSM_HANDLE inResultsHandle);
private:
    CSSM_DB_ACCESS_TYPE mAccessRequest;
    CSSM_ACCESS_CREDENTIALS *mAccessCred;
    //typedef set<DbQuery *> DbQuerySet;
    //DbQuerySet mDbQuerySet;
    //Mutex mDbQuerySetLock;
};

} // end namespace Security

#ifdef _CPP_DBCONTEXT
# pragma export off
#endif

#endif //_DBCONTEXT_H_
