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

#include "FileLockTransaction.h"
#include <Security/SecBasePriv.h>
#include <syslog.h>

FileLockTransaction::FileLockTransaction(Security::CssmClient::Db& db)
    : mDb(db), mSuccess(false), mFinalized(false), mDeleteOnFailure(false) {
    initialize();
}

void FileLockTransaction::initialize() {
    mDb->takeFileLock();
}

FileLockTransaction::~FileLockTransaction() {
    finalize();
}

void FileLockTransaction::success() {
    mSuccess = true;
}

void FileLockTransaction::setDeleteOnFailure() {
    mDeleteOnFailure = true;
}

void FileLockTransaction::finalize() {
    if(mFinalized) {
        return;
    }

    // if this transaction was a success, commit. Otherwise, roll back.
    if(mSuccess) {
        mDb->releaseFileLock(true);
    } else {
        // This is a failure.

        // Note that we're likely (but not necessarily) unwinding the stack for an exception right now.
        // (If this transaction succeeded, we wouldn't be here. So, it failed, and this code likes to fail with exceptions.)
        // If this throws an exception, we might crash the whole process.
        // Swallow exceptions whole, but log them aggressively.
        try {
            if(mDeleteOnFailure) {
                mDb->deleteFile();
            }
            mDb->releaseFileLock(false);
        } catch(CssmError cssme) {
            const char* errStr = cssmErrorString(cssme.error);
            secnotice("integrity", "caught CssmError during transaction rollback: %d %s", (int) cssme.error, errStr);
            syslog(LOG_ERR, "ERROR: failed to rollback keychain transaction: %d %s", (int) cssme.error, errStr);
        }
    }
    mFinalized = true;
}
