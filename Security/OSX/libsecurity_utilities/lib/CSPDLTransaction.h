/*
 * Copyright (c) 2015 Apple Inc. All Rights Reserved.
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

#ifndef _H_CSPDLTRANSACTION
#define _H_CSPDLTRANSACTION

#include <security_cdsa_client/dlclient.h>

//
// This class performs a transaction on a CSPDL database.
//
// If commit() has not yet been called when the object goes out of scope, the transaction will roll back instead (exceptions will be swallowed).
//
// Nesting transactions will likely work, but isn't recommended.
//
class DLTransaction {
public:
    DLTransaction(CSSM_DL_DB_HANDLE dldbh);

    ~DLTransaction();

    // Everything has gone right; this transaction will commit.
    // If you don't call this, the transaction will roll back when the object goes out of scope.
    // Might throw on error.
    void commit();

protected:
    DLTransaction();

    // Note: disables autocommit using the dldbh
    void initialize();

    // Call rollback if necessary. Never throws.
    void finalize();

    // Rolls back database transactions. Might throw.
    void rollback();

    CSSM_DL_DB_HANDLE mDldbh;

    bool mSuccess;
    bool mFinalized;

    CSSM_BOOL mAutoCommit;
};

class CSPDLTransaction : public DLTransaction {
public:
    CSPDLTransaction(Security::CssmClient::Db& db);
    ~CSPDLTransaction();

private:
    Security::CssmClient::Db& mDb;
};

#endif // _H_CSPDLTRANSACTION
