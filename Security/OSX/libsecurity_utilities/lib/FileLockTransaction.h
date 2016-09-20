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

#ifndef FileLockTransaction_h
#define FileLockTransaction_h

#include <security_cdsa_client/dlclient.h>
//
// This class performs a file lock transaction on a Cssm Db object.
//
// It will attempt to take the file lock upon creation.
//
// It will release the file lock upon destruction OR calling finalize().
//
// If you have called success(), it will tell the file lock transaction to commit
// otherwise, it will tell the file lock transaction to roll back.
//
// If you call setDeleteOnFailure(), and the transaction would normally roll
// back, this transaction will instead delete the Db's underlying file.
//
class FileLockTransaction {
public:
    FileLockTransaction(Security::CssmClient::Db& db);

    ~FileLockTransaction();

    // Everything has gone right; this transaction will commit.
    // If you don't call this, the transaction will roll back.
    void success();

    // Commit or rollback as appropriate
    void finalize();

    // After calling this method, if this class attempts to roll back the
    // transaction, it will also attempt to delete the database file.
    void setDeleteOnFailure();

protected:
    // Actually toggle autocommit using the dldbh
    void initialize();

    Security::CssmClient::Db mDb;

    bool mSuccess;
    bool mFinalized;
    bool mDeleteOnFailure;
};

#endif
