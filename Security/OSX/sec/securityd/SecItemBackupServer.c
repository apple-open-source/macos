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

#include <securityd/SecItemBackupServer.h>
#include <securityd/SecItemServer.h>
#include <Security/SecureObjectSync/SOSEnginePriv.h>
#include <Security/SecureObjectSync/SOSPeer.h>
#include <Security/SecureObjectSync/SOSBackupSliceKeyBag.h>
#include <Security/SecureObjectSync/SOSViews.h>
#include <unistd.h>

#include <securityd/SecDbItem.h>
#include <utilities/der_plist.h>

static bool withDataSourceAndEngine(CFErrorRef *error, void (^action)(SOSDataSourceRef ds, SOSEngineRef engine)) {
    bool ok = false;
    SOSDataSourceFactoryRef dsf = SecItemDataSourceFactoryGetDefault();
    SOSDataSourceRef ds = SOSDataSourceFactoryCreateDataSource(dsf, kSecAttrAccessibleWhenUnlocked, error);
    if (ds) {
        SOSEngineRef engine = SOSDataSourceGetSharedEngine(ds, error);
        if (engine) {
            action(ds, engine);
            ok = true;
        }
        ok &= SOSDataSourceRelease(ds, error);
    }
    return ok;
}

int SecServerItemBackupHandoffFD(CFStringRef backupName, CFErrorRef *error) {
    __block int fd = -1;
    if (!withDataSourceAndEngine(error, ^(SOSDataSourceRef ds, SOSEngineRef engine) {
        SOSEngineForPeerID(engine, backupName, error, ^(SOSTransactionRef txn, SOSPeerRef peer) {
            fd = SOSPeerHandoffFD(peer, error);
        });
    }) && fd >= 0) {
        close(fd);
        fd = -1;
    }
    return fd;
}

bool SecServerItemBackupSetConfirmedManifest(CFStringRef backupName, CFDataRef keybagDigest, CFDataRef manifestData, CFErrorRef *error) {
    __block bool ok = true;
    ok &= withDataSourceAndEngine(error, ^(SOSDataSourceRef ds, SOSEngineRef engine) {
        ok = SOSEngineSetPeerConfirmedManifest(engine, backupName, keybagDigest, manifestData, error);
    });
    return ok;
}

CFArrayRef SecServerItemBackupCopyNames(CFErrorRef *error) {
    __block CFArrayRef names = NULL;
    if (!withDataSourceAndEngine(error, ^(SOSDataSourceRef ds, SOSEngineRef engine) {
        names = SOSEngineCopyBackupPeerNames(engine, error);
    })) {
        CFReleaseNull(names);
    }
    return names;
}

// TODO Move to datasource and remove dsRestoreObject
static bool SOSDataSourceWithBackup(SOSDataSourceRef ds, CFDataRef backup, keybag_handle_t bag_handle, CFErrorRef *error, void(^with)(SOSObjectRef item)) {
    __block bool ok = true;
    CFPropertyListRef plist = CFPropertyListCreateWithDERData(kCFAllocatorDefault, backup, kCFPropertyListImmutable, NULL, error);
    CFDictionaryRef bdict = asDictionary(plist, error);
    ok = bdict;
    if (ok) CFDictionaryForEach(bdict, ^(const void *key, const void *value) {
        CFStringRef className = asString(key, error);
        if (className) {
            const SecDbClass *cls = kc_class_with_name(className);
            if (cls) {
                CFArrayRef items = asArray(value, error);
                CFDataRef edata;
                if (items) CFArrayForEachC(items, edata) {
                    SOSObjectRef item = (SOSObjectRef)SecDbItemCreateWithEncryptedData(kCFAllocatorDefault, cls, edata, bag_handle, error);
                    if (item) {
                        with(item);
                        CFRelease(item);
                    } else {
                        ok = false;
                    }
                } else {
                    ok = false;
                }
            } else {
                ok &= SecError(errSecDecode, error, CFSTR("bad class %@ in backup"), className);
            }
        } else {
            ok = false;
        }
    });
    CFReleaseSafe(plist);
    return ok;
}

bool SecServerItemBackupRestore(CFStringRef backupName, CFStringRef peerID, CFDataRef keybag, CFDataRef secret, CFDataRef backup, CFErrorRef *error) {
    // TODO: Decrypt and merge items in backup to dataSource

    __block bool ok = false; // return false if the bag_handle code fails.
    CFDataRef aksKeybag = NULL;
    CFMutableSetRef viewSet = NULL;
    SOSBackupSliceKeyBagRef backupSliceKeyBag = NULL;
    keybag_handle_t bag_handle = bad_keybag_handle;

    require(asData(secret, error), xit);
    require(backupSliceKeyBag = SOSBackupSliceKeyBagCreateFromData(kCFAllocatorDefault, keybag, error), xit);

    if (peerID) {
        bag_handle = SOSBSKBLoadAndUnlockWithPeerIDAndSecret(backupSliceKeyBag, peerID, secret, error);
    } else {
        if (SOSBSKBIsDirect(backupSliceKeyBag)) {
            bag_handle = SOSBSKBLoadAndUnlockWithDirectSecret(backupSliceKeyBag, secret, error);
        } else {
            bag_handle = SOSBSKBLoadAndUnlockWithWrappingSecret(backupSliceKeyBag, secret, error);
        }
    }
    require(bag_handle != bad_keybag_handle, xit);

    // TODO: How do we know which views we are allow to restore
    //viewSet = SOSAccountCopyRestorableViews();

    ok = true; // Start from original code start point - otherwise begin in this nest of stuff
    ok &= withDataSourceAndEngine(error, ^(SOSDataSourceRef ds, SOSEngineRef engine) {
        ok &= SOSDataSourceWith(ds, error, ^(SOSTransactionRef txn, bool *commit) {
            ok &= SOSDataSourceWithBackup(ds, backup, bag_handle, error, ^(SOSObjectRef item) {
                //if (SOSDataSourceIsInViewSet(item, viewSet)) {
                    SOSObjectRef mergedItem = NULL;
                    if (SOSDataSourceMergeObject(ds, txn, item, &mergedItem, error)) {
                        // if mergedItem == item then it was restored otherwise it was rejected by the conflict resolver.
                        CFReleaseSafe(mergedItem);
                    }
                //}
            });
        });
    });

xit:
    if (bag_handle != bad_keybag_handle)
        ok &= ks_close_keybag(bag_handle, error);

    CFReleaseSafe(backupSliceKeyBag);
    CFReleaseSafe(aksKeybag);
    CFReleaseSafe(viewSet);

    return ok;
}




