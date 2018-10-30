/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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

#import <Foundation/Foundation.h>
#import "SOSEnsureBackup.h"
#include <utilities/debugging.h>

#if OCTAGON
#import "keychain/ckks/CKKSLockStateTracker.h"
#import "keychain/ckks/NSOperationCategories.h"
#include <Security/SecureObjectSync/SOSAccount.h>

static NSOperationQueue *backupOperationQueue;
static CKKSLockStateTracker *lockStateTracker;

void SOSEnsureBackupWhileUnlocked(void) {
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        backupOperationQueue = [NSOperationQueue new];
        lockStateTracker = [CKKSLockStateTracker new];
    });

    // CKKSLockStateTracker does not use @synchronized(self). If it ever starts to this needs to be updated.
    @synchronized(lockStateTracker) {
        if ([backupOperationQueue operationCount] > 0) {
            secnotice("engine", "SOSEnsureBackup: Backup already scheduled for next unlock");
        } else {
            secnotice("engine", "SOSEnsureBackup: Scheduling a backup for next unlock");
            NSBlockOperation *backupOperation = [NSBlockOperation blockOperationWithBlock:^{
                secnotice("engine", "Performing keychain backup after unlock because backing up while locked failed");
                SOSAccount *account = (__bridge SOSAccount *)(SOSKeychainAccountGetSharedAccount());

                if(!account) {
                    secnotice("ckks", "Failed to get account object");
                    return;
                }

                [account performTransaction:^(SOSAccountTransaction *transaction) {
                    CFErrorRef error = NULL;
                    NSSet* set = CFBridgingRelease(SOSAccountCopyBackupPeersAndForceSync(transaction, &error));
                    if (set) {
                        secnotice("engine", "SOSEnsureBackup: SOS made a backup of views: %@", set);
                    } else {
                        secerror("engine: SOSEnsureBackup: encountered an error while making backup (%@)", error);
                    }

                    CFReleaseNull(error);
                }];
            }];
            [backupOperation addNullableDependency:lockStateTracker.unlockDependency];
            [backupOperationQueue addOperation:backupOperation];
        }
    }
}
#else
void SOSEnsureBackupWhileUnlocked(void) {
    secnotice("engine", "SOSEnsureBackup not available on this platform");
}
#endif
