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

/*
 * SOSViews.c -  Implementation of views
 */

#include <AssertMacros.h>
#include <TargetConditionals.h>

#include "SOSViews.h"
#include <utilities/SecCFWrappers.h>
#include <utilities/SecCFRelease.h>
#include <utilities/SecXPCError.h>

#include <utilities/SecCFError.h>
#include <utilities/der_set.h>
#include <Security/SecureObjectSync/SOSInternal.h>

#include <Security/SecureObjectSync/SOSPeerInfo.h>
#include <Security/SecureObjectSync/SOSPeerInfoV2.h>
#include <Security/SecureObjectSync/SOSPeerInfoPriv.h>
#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include <Security/SecureObjectSync/SOSAccount.h>
#include <Security/SecureObjectSync/SOSAccountPriv.h>

CFStringRef viewMemError                = CFSTR("Failed to get memory for views in PeerInfo");
CFStringRef viewUnknownError            = CFSTR("Unknown view(%@) (ViewResultCode=%d)");
CFStringRef viewInvalidError            = CFSTR("Peer is invalid for this view(%@) (ViewResultCode=%d)");

// Internal Views:
const CFStringRef kSOSViewKeychainV0_tomb           = CFSTR("KeychainV0-tomb"); // iCloud Keychain backup for v0 peers (no tombstones)
const CFStringRef kSOSViewBackupBagV0_tomb          = CFSTR("BackupBagV0-tomb");     // iCloud Keychain backup bag for v0 peers (no tombstones)
const CFStringRef kSOSViewWiFi_tomb                 = CFSTR("WiFi-tomb");
const CFStringRef kSOSViewAutofillPasswords_tomb    = CFSTR("Passwords-tomb");
const CFStringRef kSOSViewSafariCreditCards_tomb    = CFSTR("CreditCards-tomb");
const CFStringRef kSOSViewiCloudIdentity_tomb       = CFSTR("iCloudIdentity-tomb");
const CFStringRef kSOSViewOtherSyncable_tomb        = CFSTR("OtherSyncable-tomb");

// Views
const CFStringRef kSOSViewKeychainV0            = CFSTR("KeychainV0");      // iCloud Keychain syncing for v0 peers

const CFStringRef kSOSViewWiFi                  = CFSTR("WiFi");
const CFStringRef kSOSViewAutofillPasswords     = CFSTR("Passwords");
const CFStringRef kSOSViewSafariCreditCards     = CFSTR("CreditCards");
const CFStringRef kSOSViewiCloudIdentity        = CFSTR("iCloudIdentity");
const CFStringRef kSOSViewBackupBagV0           = CFSTR("BackupBagV0");     // iCloud Keychain backup bag for v0 peers (single item)
const CFStringRef kSOSViewOtherSyncable         = CFSTR("OtherSyncable");

// PCS (Protected Cloud Storage) Views
const CFStringRef kSOSViewPCSMasterKey          = CFSTR("PCS-MasterKey");
const CFStringRef kSOSViewPCSiCloudDrive        = CFSTR("PCS-iCloudDrive"); // Bladerunner
const CFStringRef kSOSViewPCSPhotos             = CFSTR("PCS-Photos");      // Hyperion
const CFStringRef kSOSViewPCSCloudKit           = CFSTR("PCS-CloudKit");    // Liverpool
const CFStringRef kSOSViewPCSEscrow             = CFSTR("PCS-Escrow");
const CFStringRef kSOSViewPCSFDE                = CFSTR("PCS-FDE");
const CFStringRef kSOSViewPCSMailDrop           = CFSTR("PCS-Maildrop");    // PianoMover
const CFStringRef kSOSViewPCSiCloudBackup       = CFSTR("PCS-Backup");
const CFStringRef kSOSViewPCSNotes              = CFSTR("PCS-Notes");
const CFStringRef kSOSViewPCSiMessage           = CFSTR("PCS-iMessage");
const CFStringRef kSOSViewPCSFeldspar           = CFSTR("PCS-Feldspar");

const CFStringRef kSOSViewAppleTV               = CFSTR("AppleTV");
const CFStringRef kSOSViewHomeKit               = CFSTR("HomeKit");

// View Hints
// Note that by definition, there cannot be a V0 view hint
// These will be deprecated for new constants found in SecItemPriv.h
const CFStringRef kSOSViewHintPCSMasterKey      = CFSTR("PCS-MasterKey");
const CFStringRef kSOSViewHintPCSiCloudDrive    = CFSTR("PCS-iCloudDrive");
const CFStringRef kSOSViewHintPCSPhotos         = CFSTR("PCS-Photos");
const CFStringRef kSOSViewHintPCSCloudKit       = CFSTR("PCS-CloudKit");
const CFStringRef kSOSViewHintPCSEscrow         = CFSTR("PCS-Escrow");
const CFStringRef kSOSViewHintPCSFDE            = CFSTR("PCS-FDE");
const CFStringRef kSOSViewHintPCSMailDrop       = CFSTR("PCS-Maildrop");
const CFStringRef kSOSViewHintPCSiCloudBackup   = CFSTR("PCS-Backup");
const CFStringRef kSOSViewHintPCSNotes          = CFSTR("PCS-Notes");
const CFStringRef kSOSViewHintPCSiMessage       = CFSTR("PCS-iMessage");
const CFStringRef kSOSViewHintPCSFeldspar       = CFSTR("PCS-Feldspar");

const CFStringRef kSOSViewHintAppleTV           = CFSTR("AppleTV");
const CFStringRef kSOSViewHintHomeKit           = CFSTR("HomeKit");

CFGiblisGetSingleton(CFSetRef, SOSViewsGetV0ViewSet, defaultViewSet, ^{
    // Since peer->views must never be NULL, fill in with a default
    const void *values[] = { kSOSViewKeychainV0 };
    *defaultViewSet = CFSetCreate(kCFAllocatorDefault, values, array_size(values), &kCFTypeSetCallBacks);
});

CFGiblisGetSingleton(CFSetRef, SOSViewsGetV0SubviewSet, subViewSet, (^{
    // Since peer->views must never be NULL, fill in with a default
    const void *values[] = { kSOSViewWiFi, kSOSViewAutofillPasswords, kSOSViewSafariCreditCards,
                             kSOSViewiCloudIdentity, kSOSViewBackupBagV0, kSOSViewOtherSyncable };
    *subViewSet = CFSetCreate(kCFAllocatorDefault, values, array_size(values), &kCFTypeSetCallBacks);
}));

CFGiblisGetSingleton(CFSetRef, SOSViewsGetV0BackupViewSet, defaultViewSet, ^{
    const void *values[] = { kSOSViewKeychainV0_tomb };
    *defaultViewSet = CFSetCreate(kCFAllocatorDefault, values, array_size(values), &kCFTypeSetCallBacks);
});

CFGiblisGetSingleton(CFSetRef, SOSViewsGetV0BackupBagViewSet, defaultViewSet, ^{
    const void *values[] = { kSOSViewBackupBagV0_tomb };
    *defaultViewSet = CFSetCreate(kCFAllocatorDefault, values, array_size(values), &kCFTypeSetCallBacks);
});

bool SOSViewsIsV0Subview(CFStringRef viewName) {
    return CFSetContainsValue(SOSViewsGetV0SubviewSet(), viewName);
}

CFSetRef sTestViewSet = NULL;
void SOSViewsSetTestViewsSet(CFSetRef testViewNames) {
    CFRetainAssign(sTestViewSet, testViewNames);
}

CFSetRef SOSViewsGetAllCurrent(void) {
    static dispatch_once_t dot;
    static CFMutableSetRef allViews = NULL;
    dispatch_once(&dot, ^{
        allViews = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
        CFSetAddValue(allViews, kSOSViewKeychainV0);
        CFSetAddValue(allViews, kSOSViewPCSMasterKey);
        CFSetAddValue(allViews, kSOSViewPCSiCloudDrive);
        CFSetAddValue(allViews, kSOSViewPCSPhotos);
        CFSetAddValue(allViews, kSOSViewPCSCloudKit);
        CFSetAddValue(allViews, kSOSViewPCSEscrow);
        CFSetAddValue(allViews, kSOSViewPCSFDE);
        CFSetAddValue(allViews, kSOSViewPCSMailDrop);
        CFSetAddValue(allViews, kSOSViewPCSiCloudBackup);
        CFSetAddValue(allViews, kSOSViewPCSNotes);
        CFSetAddValue(allViews, kSOSViewPCSiMessage);
        CFSetAddValue(allViews, kSOSViewPCSFeldspar);
        CFSetAddValue(allViews, kSOSViewAppleTV);
        CFSetAddValue(allViews, kSOSViewHomeKit);
        CFSetAddValue(allViews, kSOSViewWiFi);
        CFSetAddValue(allViews, kSOSViewAutofillPasswords);
        CFSetAddValue(allViews, kSOSViewSafariCreditCards);
        CFSetAddValue(allViews, kSOSViewiCloudIdentity);
        CFSetAddValue(allViews, kSOSViewBackupBagV0);
        CFSetAddValue(allViews, kSOSViewOtherSyncable);
    });
    return sTestViewSet ? sTestViewSet : allViews;
}

static CFMutableSetRef CFSetCreateMutableCopyForSOSViews(CFAllocatorRef allocator, CFSetRef original) {
    if(!original) return NULL;
    return CFSetCreateMutableCopy(allocator, 0, original);
}

CFMutableSetRef SOSViewsCreateDefault(bool includeLegacy, CFErrorRef *error) {
    CFMutableSetRef result = CFSetCreateMutableCopyForSOSViews(NULL, SOSViewsGetAllCurrent());

    // We don't by default particiate in V0, actually we don't want
    // to let folks enable it.
    CFSetRemoveValue(result, kSOSViewKeychainV0);

    if (!includeLegacy) {
        // We don't by default participate in fractures of iCloudKeychain
        CFSetRemoveValue(result, kSOSViewWiFi);
        CFSetRemoveValue(result, kSOSViewAutofillPasswords);
        CFSetRemoveValue(result, kSOSViewSafariCreditCards);
        CFSetRemoveValue(result, kSOSViewOtherSyncable);
    }

    return result;
}

// Eventually this will want to know the gestalt or security properties...
void SOSViewsForEachDefaultEnabledViewName(void (^operation)(CFStringRef viewName)) {
    CFMutableSetRef defaultViews = SOSViewsCreateDefault(false, NULL);
    
    CFSetForEach(defaultViews, ^(const void *value) {
        CFStringRef name = asString(value, NULL);

        if (name) {
            operation(name);
        }
    });
    
    CFReleaseNull(defaultViews);
}

static bool SOSViewsIsKnownView(CFStringRef viewname) {
    CFSetRef allViews = SOSViewsGetAllCurrent();
    if(CFSetContainsValue(allViews, viewname)) return true;
    secnotice("views","Not a known view");
    return false;
}

bool SOSPeerInfoIsEnabledView(SOSPeerInfoRef pi, CFStringRef viewName) {
    if (pi->version < kSOSPeerV2BaseVersion) {
        return CFSetContainsValue(SOSViewsGetV0ViewSet(), viewName);
    } else {
        return SOSPeerInfoV2DictionaryHasSetContaining(pi, sViewsKey, viewName);
    }
}

void SOSPeerInfoWithEnabledViewSet(SOSPeerInfoRef pi, void (^operation)(CFSetRef enabled)) {
    if (pi->version < kSOSPeerV2BaseVersion) {
        operation(SOSViewsGetV0ViewSet());
    } else {
        SOSPeerInfoV2DictionaryWithSet(pi, sViewsKey, operation);
    }
}

CFMutableSetRef SOSPeerInfoCopyEnabledViews(SOSPeerInfoRef pi) {
    if (pi->version < kSOSPeerV2BaseVersion) {
        return CFSetCreateMutableCopy(kCFAllocatorDefault, CFSetGetCount(SOSViewsGetV0ViewSet()), SOSViewsGetV0ViewSet());
    } else {
        CFMutableSetRef views = SOSPeerInfoV2DictionaryCopySet(pi, sViewsKey);
        if (!views) {
            // This is unexpected: log and return an empty set to prevent <rdar://problem/21938868>
            secerror("%@ v2 peer has no views", SOSPeerInfoGetPeerID(pi));
            views = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
        }
        return views;
    }
}

CFSetRef SOSPeerInfoGetPermittedViews(SOSPeerInfoRef pi) {
    return SOSViewsGetAllCurrent();
}

static void SOSPeerInfoSetViews(SOSPeerInfoRef pi, CFSetRef newviews) {
	if(!newviews) {
		secnotice("views","Asked to swap to NULL views");
		return;
	}
    SOSPeerInfoV2DictionarySetValue(pi, sViewsKey, newviews);
}

static bool SOSPeerInfoViewIsValid(SOSPeerInfoRef pi, CFStringRef viewname) {
    return true;
}

static bool viewErrorReport(CFIndex errorCode, CFErrorRef *error, CFStringRef format, CFStringRef viewname, int retval) {
	return SOSCreateErrorWithFormat(errorCode, NULL, error, NULL, format, viewname, retval);
}

SOSViewResultCode SOSViewsEnable(SOSPeerInfoRef pi, CFStringRef viewname, CFErrorRef *error) {
	SOSViewResultCode retval = kSOSCCGeneralViewError;
    
    CFMutableSetRef newviews = SOSPeerInfoCopyEnabledViews(pi);
    require_action_quiet(newviews, fail, 
		SOSCreateError(kSOSErrorAllocationFailure, viewMemError, NULL, error));
    require_action_quiet(SOSViewsIsKnownView(viewname), fail,
		viewErrorReport(kSOSErrorNameMismatch, error, viewUnknownError, viewname, retval = kSOSCCNoSuchView));
    require_action_quiet(SOSPeerInfoViewIsValid(pi, viewname), fail,
		viewErrorReport(kSOSErrorNameMismatch, error, viewInvalidError, viewname, retval = kSOSCCViewNotQualified));
    CFSetAddValue(newviews, viewname);
	SOSPeerInfoSetViews(pi, newviews);
    CFReleaseSafe(newviews);
    return kSOSCCViewMember;

fail:
	CFReleaseNull(newviews);
	secnotice("views","Failed to enable view(%@): %@", viewname, *error);
	return retval;
}

bool SOSViewSetEnable(SOSPeerInfoRef pi, CFSetRef viewSet) {
    __block bool retval = true;
    __block bool addedView = false;
    CFMutableSetRef newviews = SOSPeerInfoCopyEnabledViews(pi);
    require_action_quiet(newviews, errOut, secnotice("views", "failed to copy enabled views"));
    
    CFSetForEach(viewSet, ^(const void *value) {
        CFStringRef viewName = (CFStringRef) value;
        if(SOSViewsIsKnownView(viewName) && SOSPeerInfoViewIsValid(pi, viewName) && !CFSetContainsValue(newviews, viewName)) {
            addedView = true;
            CFSetAddValue(newviews, viewName);
        } else {
            retval = false;
            secnotice("views", "couldn't add view %@", viewName);
        }
    });
    require_quiet(retval, errOut);

    if (addedView) {
        SOSPeerInfoSetViews(pi, newviews);
    }
    
errOut:
    CFReleaseNull(newviews);
    return retval;
}


SOSViewResultCode SOSViewsDisable(SOSPeerInfoRef pi, CFStringRef viewname, CFErrorRef *error) {
	SOSViewResultCode retval = kSOSCCGeneralViewError;
    CFMutableSetRef newviews = SOSPeerInfoCopyEnabledViews(pi);
    require_action_quiet(newviews, fail, 
		SOSCreateError(kSOSErrorAllocationFailure, viewMemError, NULL, error));
    require_action_quiet(SOSViewsIsKnownView(viewname), fail,
		viewErrorReport(kSOSErrorNameMismatch, error, viewUnknownError, viewname, retval = kSOSCCNoSuchView));

    CFSetRemoveValue(newviews, viewname);
	SOSPeerInfoSetViews(pi, newviews);
    CFReleaseSafe(newviews);
    return kSOSCCViewNotMember;

fail:
	CFReleaseNull(newviews);
	secnotice("views","Failed to disable view(%@): %@", viewname, *error);
	return retval;
}


bool SOSViewSetDisable(SOSPeerInfoRef pi, CFSetRef viewSet) {
    __block bool retval = true;
    __block bool removed = false;
    CFMutableSetRef newviews = SOSPeerInfoCopyEnabledViews(pi);
    require_action_quiet(newviews, errOut, secnotice("views", "failed to copy enabled views"));
    
    CFSetForEach(viewSet, ^(const void *value) {
        CFStringRef viewName = (CFStringRef) value;
        if(SOSViewsIsKnownView(viewName) && CFSetContainsValue(newviews, viewName)) {
            removed = true;
            CFSetRemoveValue(newviews, viewName);
        } else {
            retval = false;
            secnotice("views", "couldn't delete view %@", viewName);
        }
    });

    require_quiet(retval, errOut);

    if(removed) {
        SOSPeerInfoSetViews(pi, newviews);
    }
    
errOut:
    CFReleaseNull(newviews);
    return retval;
}


SOSViewResultCode SOSViewsQuery(SOSPeerInfoRef pi, CFStringRef viewname, CFErrorRef *error) {
	SOSViewResultCode retval = kSOSCCNoSuchView;
    CFSetRef views = NULL;
    secnotice("views", "Querying %@", viewname);
    require_action_quiet(SOSViewsIsKnownView(viewname), fail,
		SOSCreateError(kSOSErrorNameMismatch, viewUnknownError, NULL, error));
    views = SOSPeerInfoCopyEnabledViews(pi);
    if(!views){
        retval = kSOSCCViewNotMember;
        CFReleaseNull(views);
        return retval;
    }

    // kSOSViewKeychainV0 is set if there is a V0 PeerInfo in the circle.  It represents all of the subviews in
    // SOSViewsGetV0SubviewSet() so we return kSOSCCViewMember for that case.  kSOSViewKeychainV0 and the subviews
    // are mutually exclusive.
    else if(CFSetContainsValue(views, kSOSViewKeychainV0) && CFSetContainsValue(SOSViewsGetV0SubviewSet(), viewname)) {
        retval = kSOSCCViewMember;
    } else {
        retval = (CFSetContainsValue(views, viewname)) ? kSOSCCViewMember: kSOSCCViewNotMember;
    }
    
    CFReleaseNull(views);
    return retval;
    
fail:
	secnotice("views","Failed to query view(%@): %@", viewname, *error);
    CFReleaseNull(views);
    return retval;
}

static CFArrayRef SOSCreateActiveViewIntersectionArrayForPeerInfos(SOSPeerInfoRef pi1, SOSPeerInfoRef pi2) {
    CFMutableArrayRef  retval = NULL;
    CFSetRef views1 = SOSPeerInfoCopyEnabledViews(pi1);
    CFSetRef views2 = SOSPeerInfoCopyEnabledViews(pi2);
    size_t count = CFSetGetCount(views1);
    if(count == 0){
        CFReleaseNull(views1);
        CFReleaseNull(views2);
        return NULL;
    }
    CFStringRef pi1views[count];
    
    retval = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFSetGetValues(views1, (const void **) &pi1views);
    for(size_t i = 0; i < count; i++) {
        if(CFSetContainsValue(views2, pi1views[i])) {
            CFArrayAppendValue(retval, pi1views[i]);
        }
    }
    CFReleaseNull(views1);
    CFReleaseNull(views2);

    return retval;
}

CFArrayRef SOSCreateActiveViewIntersectionArrayForPeerID(SOSAccountRef account, CFStringRef peerID) {
    CFArrayRef  retval = NULL;
    SOSPeerInfoRef myPI = SOSAccountGetMyPeerInfo(account);
    SOSPeerInfoRef theirPI = SOSAccountCopyPeerWithID(account, peerID, NULL);
    require_action_quiet(myPI, errOut, retval = NULL);
    require_action_quiet(theirPI, errOut, retval = NULL);
    
    retval = SOSCreateActiveViewIntersectionArrayForPeerInfos(myPI, theirPI);
    
errOut:
    CFReleaseNull(theirPI);
    return retval;
}

// This needs to create a dictionary of sets of intersected views for an account
CFDictionaryRef SOSViewsCreateActiveViewMatrixDictionary(SOSAccountRef account, SOSCircleRef circle, CFErrorRef *error) {
    CFMutableDictionaryRef retval = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    SOSPeerInfoRef myPI = SOSAccountGetMyPeerInfo(account);
    
    // For now, all views require that a valid member peer is in the circle and active/valid
    CFMutableSetRef peers = SOSCircleCopyPeers(circle, kCFAllocatorDefault);

    require_action_quiet(retval, errOut, SOSCreateError(kSOSErrorAllocationFailure, CFSTR("Could not allocate ViewMatrix"), NULL, error));
    require_action_quiet(myPI, errOut, SOSCreateError(kSOSErrorPeerNotFound, CFSTR("Could not find our PeerInfo"), NULL, error));
    require(peers, errOut);

    CFSetRef myViews = SOSPeerInfoCopyEnabledViews(myPI);

    if (myViews)
        CFSetForEach(myViews, ^(const void *value) {
            CFStringRef viewname = (CFStringRef) value;
            CFMutableSetRef viewset = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);
            CFSetForEach(peers, ^(const void *peervalue) {
                SOSPeerInfoRef pi = (SOSPeerInfoRef) peervalue;
                CFSetRef piViews = SOSPeerInfoCopyEnabledViews(pi);

                if(piViews && CFSetContainsValue(piViews, viewname)) {
                    CFStringRef peerID = SOSPeerInfoGetPeerID(pi);
                    CFSetAddValue(viewset, peerID);
                }
                CFReleaseNull(piViews);

            });
            CFDictionaryAddValue(retval, viewname, viewset);
        });
    
    if(CFDictionaryGetCount(retval) == 0) goto errOut;  // Not really an error - just no intersection of views with anyone
    CFReleaseNull(peers);
    CFReleaseNull(myViews);
    return retval;
    
errOut:
    CFReleaseNull(retval);
    CFReleaseNull(peers);
    return NULL;
}





/* Need XPC way to carry CFSets of views */


CFSetRef CreateCFSetRefFromXPCObject(xpc_object_t xpcSetDER, CFErrorRef* error) {
    CFSetRef retval = NULL;
    require_action_quiet(xpcSetDER, errOut, SecCFCreateErrorWithFormat(kSecXPCErrorUnexpectedNull, sSecXPCErrorDomain, NULL, error, NULL, CFSTR("Unexpected Null Set to decode")));
    
    require_action_quiet(xpc_get_type(xpcSetDER) == XPC_TYPE_DATA, errOut, SecCFCreateErrorWithFormat(kSecXPCErrorUnexpectedType, sSecXPCErrorDomain, NULL, error, NULL, CFSTR("xpcSetDER not data, got %@"), xpcSetDER));
    
    const uint8_t* der = xpc_data_get_bytes_ptr(xpcSetDER);
    const uint8_t* der_end = der + xpc_data_get_length(xpcSetDER);
    der = der_decode_set(kCFAllocatorDefault, kCFPropertyListMutableContainersAndLeaves, &retval, error, der, der_end);
    if (der != der_end) {
        SecError(errSecDecode, error, CFSTR("trailing garbage at end of SecAccessControl data"));
        goto errOut;
    }
    return retval;
errOut:
    CFReleaseNull(retval);
    return NULL;
}

xpc_object_t CreateXPCObjectWithCFSetRef(CFSetRef setref, CFErrorRef *error) {
    xpc_object_t result = NULL;
    size_t data_size = 0;
    uint8_t *data = NULL;
    require_action_quiet(setref, errOut, SecCFCreateErrorWithFormat(kSecXPCErrorUnexpectedNull, sSecXPCErrorDomain, NULL, error, NULL, CFSTR("Unexpected Null Set to encode")));
    require_quiet((data_size = der_sizeof_set(setref, error)) != 0, errOut);
    require_quiet((data = (uint8_t *)malloc(data_size)) != NULL, errOut);
    
    der_encode_set(setref, error, data, data + data_size);
    result = xpc_data_create(data, data_size);
    free(data);
errOut:
    return result;
}

