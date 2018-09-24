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

#include <utilities/array_size.h>
#include <Security/SecureObjectSync/SOSAccount.h>
#include <Security/SecureObjectSync/SOSAccountPriv.h>

#define viewMemError                CFSTR("Failed to get memory for views in PeerInfo")
#define viewUnknownError            CFSTR("Unknown view(%@) (ViewResultCode=%d)")
#define viewInvalidError            CFSTR("Peer is invalid for this view(%@) (ViewResultCode=%d)")

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

#undef DOVIEWMACRO
#define DOVIEWMACRO(VIEWNAME, DEFSTRING, CMDSTRING, SYSTEM, DEFAULTSETTING, INITIALSYNCSETTING, ALWAYSONSETTING, BACKUPSETTING, V0SETTING) \
const CFStringRef k##SYSTEM##View##VIEWNAME          = CFSTR(DEFSTRING);
#include "Security/SecureObjectSync/ViewList.list"

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

CFMutableSetRef SOSViewCopyViewSet(ViewSetKind setKind) {
    CFMutableSetRef result = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);

    // Only return views in the SOS system, unless they asked for kViewSetCKKS
#undef DOVIEWMACRO
#define __TYPE_MEMBER_ false
#define __TYPE_MEMBER_D true
#define __TYPE_MEMBER_I true
#define __TYPE_MEMBER_A true
#define __TYPE_MEMBER_V true
#define __TYPE_MEMBER_B true
#define __SYSTEM_SOS true
#define __SYSTEM_CKKS false

#define DOVIEWMACRO_SOS(VIEWNAME, DEFSTRING, CMDSTRING, SYSTEM, DEFAULT, INITIAL, ALWAYSON, BACKUP, V0) \
    if(((setKind == kViewSetAll) || \
       ((setKind == kViewSetDefault)   && __TYPE_MEMBER_##DEFAULT)  || \
       ((setKind == kViewSetInitial)   && __TYPE_MEMBER_##INITIAL)  || \
       ((setKind == kViewSetAlwaysOn)  && __TYPE_MEMBER_##ALWAYSON) || \
       ((setKind == kViewSetRequiredForBackup)  && __TYPE_MEMBER_##BACKUP) || \
       ((setKind == kViewSetV0)  && __TYPE_MEMBER_##V0)       )) { \
        CFSetAddValue(result, k##SYSTEM##View##VIEWNAME); \
    }

#define DOVIEWMACRO_CKKS(VIEWNAME, DEFSTRING, CMDSTRING, SYSTEM, DEFAULT, INITIAL, ALWAYSON, BACKUP, V0) \
    if(setKind == kViewSetCKKS) { \
        CFSetAddValue(result, k##SYSTEM##View##VIEWNAME); \
    }

#define DOVIEWMACRO(VIEWNAME, DEFSTRING, CMDSTRING, SYSTEM, DEFAULT, INITIAL, ALWAYSON, BACKUP, V0) \
DOVIEWMACRO_##SYSTEM(VIEWNAME, DEFSTRING, CMDSTRING, SYSTEM, DEFAULT, INITIAL, ALWAYSON, BACKUP, V0)
#include "Security/SecureObjectSync/ViewList.list"

    return result;
}

bool SOSViewInSOSSystem(CFStringRef view) {

    if(CFEqualSafe(view, kSOSViewKeychainV0)) {
        return true;
    }

#undef DOVIEWMACRO
#define DOVIEWMACRO(VIEWNAME, DEFSTRING, CMDSTRING, SYSTEM, DEFAULT, INITIAL, ALWAYSON, BACKUP, V0) \
    if(CFEqualSafe(view, k##SYSTEM##View##VIEWNAME)) { \
        return __SYSTEM_##SYSTEM; \
    }
#include "Security/SecureObjectSync/ViewList.list"

    return false;
}

bool SOSViewHintInSOSSystem(CFStringRef viewHint) {
#undef DOVIEWMACRO
#define CHECK_VIEWHINT_(VIEWNAME, SYSTEM) \
  if(CFEqualSafe(viewHint, kSecAttrViewHint##VIEWNAME)) { \
    return __SYSTEM_##SYSTEM; \
  }
#define CHECK_VIEWHINT_V(VIEWNAME, SYSTEM)

#define DOVIEWMACRO(VIEWNAME, DEFSTRING, CMDSTRING, SYSTEM, DEFAULT, INITIAL, ALWAYSON, BACKUP, V0) \
CHECK_VIEWHINT_##V0(VIEWNAME, SYSTEM)
#include "Security/SecureObjectSync/ViewList.list"

    return false;
}

bool SOSViewHintInCKKSSystem(CFStringRef viewHint) {

#undef DOVIEWMACRO_SOS
#undef DOVIEWMACRO_CKKS
#undef DOVIEWMACRO

#define DOVIEWMACRO_SOS(VIEWNAME, DEFSTRING, CMDSTRING, SYSTEM, DEFAULT, INITIAL, ALWAYSON, BACKUP, V0)
#define DOVIEWMACRO_CKKS(VIEWNAME, DEFSTRING, CMDSTRING, SYSTEM, DEFAULT, INITIAL, ALWAYSON, BACKUP, V0) \
if(CFEqualSafe(viewHint, kSecAttrViewHint##VIEWNAME)) { \
    return true; \
}
#define DOVIEWMACRO(VIEWNAME, DEFSTRING, CMDSTRING, SYSTEM, DEFAULT, INITIAL, ALWAYSON, BACKUP, V0) \
DOVIEWMACRO_##SYSTEM(VIEWNAME, DEFSTRING, CMDSTRING, SYSTEM, DEFAULT, INITIAL, ALWAYSON, BACKUP, V0)

#include "Security/SecureObjectSync/ViewList.list"

    return false;
}


CFGiblisGetSingleton(CFSetRef, SOSViewsGetV0ViewSet, defaultViewSet, ^{
    // Since peer->views must never be NULL, fill in with a default
    const void *values[] = { kSOSViewKeychainV0 };
    *defaultViewSet = CFSetCreate(kCFAllocatorDefault, values, array_size(values), &kCFTypeSetCallBacks);
});

CFGiblisGetSingleton(CFSetRef, SOSViewsGetV0SubviewSet, subViewSet, (^{
    // Since peer->views must never be NULL, fill in with a default
    *subViewSet = SOSViewCopyViewSet(kViewSetV0);
}));

CFGiblisGetSingleton(CFSetRef, SOSViewsGetV0BackupViewSet, defaultViewSet, ^{
    const void *values[] = { kSOSViewKeychainV0_tomb };
    *defaultViewSet = CFSetCreate(kCFAllocatorDefault, values, array_size(values), &kCFTypeSetCallBacks);
});

CFGiblisGetSingleton(CFSetRef, SOSViewsGetV0BackupBagViewSet, defaultViewSet, ^{
    const void *values[] = { kSOSViewBackupBagV0_tomb };
    *defaultViewSet = CFSetCreate(kCFAllocatorDefault, values, array_size(values), &kCFTypeSetCallBacks);
});


CFGiblisGetSingleton(CFSetRef, SOSViewsGetInitialSyncSubviewSet, subViewSet, (^{
    *subViewSet = SOSViewCopyViewSet(kViewSetInitial);
}));


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
        allViews = SOSViewCopyViewSet(kViewSetAll);

        CFSetAddValue(allViews, kSOSViewKeychainV0);
        if(sTestViewSet) CFSetUnion(allViews, sTestViewSet);
    });
    return allViews;
}

static CFDictionaryRef SOSViewsGetBitmasks(void) {
    static dispatch_once_t once;
    static CFMutableDictionaryRef masks = NULL;

    dispatch_once(&once, ^{
        CFSetRef views = SOSViewsGetAllCurrent();
        CFMutableArrayRef viewArray = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
        CFSetForEach(views, ^(const void *value) {
            CFStringRef viewName = (CFStringRef) value;
            CFArrayAppendValue(viewArray, viewName);
        });
        CFIndex viewCount = CFArrayGetCount(viewArray);
        if(viewCount > 32) {
            secnotice("views", "Too many views defined, can't make bitmask (%d)", (int) viewCount);
        } else {
            __block uint32_t maskValue = 1;
            CFRange all = CFRangeMake(0, viewCount);
            CFArraySortValues(viewArray, all, (CFComparatorFunction)CFStringCompare, NULL);
            masks = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFCopyStringDictionaryKeyCallBacks, NULL);
            CFArrayForEach(viewArray, ^(const void *value) {
                CFDictionaryAddValue(masks, value, (const void *) (uintptr_t) maskValue);
                maskValue <<= 1;
            });
        }
        CFReleaseNull(viewArray);
    });
    return masks;
}

static uint64_t SOSViewBitmaskFromSet(CFSetRef views) {
    __block uint64_t retval = 0;
    CFDictionaryRef masks = SOSViewsGetBitmasks();
    if(masks) {
        CFSetForEach(views, ^(const void *viewName) {
            uint64_t viewMask = (uint64_t) CFDictionaryGetValue(masks, viewName);
            retval |= viewMask;
        });
    }
    return retval;
}

uint64_t SOSPeerInfoViewBitMask(SOSPeerInfoRef pi) {
    __block uint64_t retval = 0;
    CFSetRef views = SOSPeerInfoCopyEnabledViews(pi);
    if(views) {
        retval = SOSViewBitmaskFromSet(views);
        CFReleaseNull(views);
    }
    return retval;
}

CFSetRef SOSViewCreateSetFromBitmask(uint64_t bitmask) {
    CFMutableSetRef retval = NULL;
    CFDictionaryRef masks = SOSViewsGetBitmasks();
    if(masks) {
        retval = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
        CFDictionaryForEach(masks, ^(const void *key, const void *value) {
            CFStringRef viewName = (CFStringRef) key;
            uint64_t viewMask = (uint64_t) value;
            if(bitmask & viewMask) {
                CFSetAddValue(retval, viewName);
            }
        });
    }
    return retval;
}

const char *SOSViewsXlateAction(SOSViewActionCode action) {
    switch(action) {
        case kSOSCCViewEnable: return "kSOSCCViewEnable";
        case kSOSCCViewDisable: return "kSOSCCViewDisable";
        case kSOSCCViewQuery: return "kSOSCCViewQuery";
        default: return "unknownViewAction";
    }
}


// Eventually this will want to know the gestalt or security properties...
void SOSViewsForEachDefaultEnabledViewName(void (^operation)(CFStringRef viewName)) {
    CFMutableSetRef defaultViews = SOSViewCopyViewSet(kViewSetDefault);

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

static bool SOSViewsRequireIsKnownView(CFStringRef viewname, CFErrorRef* error) {
    return SOSViewsIsKnownView(viewname) || SOSCreateErrorWithFormat(kSOSErrorNameMismatch, NULL, error, NULL, viewUnknownError, viewname, kSOSCCNoSuchView);
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

SOSViewResultCode SOSViewsEnable(SOSPeerInfoRef pi, CFStringRef viewname, CFErrorRef *error) {
    SOSViewResultCode retval = kSOSCCGeneralViewError;

    CFMutableSetRef newviews = SOSPeerInfoCopyEnabledViews(pi);
    require_action_quiet(newviews, fail,
                         SOSCreateError(kSOSErrorAllocationFailure, viewMemError, NULL, error));
    require_action_quiet(SOSViewsRequireIsKnownView(viewname, error), fail,
                         retval = kSOSCCNoSuchView);
    require_action_quiet(SOSPeerInfoViewIsValid(pi, viewname), fail,
                         SOSCreateErrorWithFormat(kSOSErrorNameMismatch, NULL, error, NULL, viewInvalidError, viewname, retval = kSOSCCViewNotQualified));
    CFSetAddValue(newviews, viewname);
    SOSPeerInfoSetViews(pi, newviews);
    CFReleaseSafe(newviews);
    return kSOSCCViewMember;

fail:
    CFReleaseNull(newviews);
    secnotice("views","Failed to enable view(%@): %@", viewname, error ? *error : NULL);
    return retval;
}

bool SOSViewSetEnable(SOSPeerInfoRef pi, CFSetRef viewSet) {
    __block bool addedView = false;
    CFMutableSetRef newviews = SOSPeerInfoCopyEnabledViews(pi);
    require_action_quiet(newviews, errOut, secnotice("views", "failed to copy enabled views"));

    CFSetForEach(viewSet, ^(const void *value) {
        CFStringRef viewName = (CFStringRef) value;
        if(SOSViewsIsKnownView(viewName) && SOSPeerInfoViewIsValid(pi, viewName)) {
            if (!CFSetContainsValue(newviews, viewName)) {
                addedView = true;
                CFSetAddValue(newviews, viewName);
            }
        } else {
            secnotice("views", "couldn't add view %@", viewName);
        }
    });
    require_quiet(addedView, errOut);

    SOSPeerInfoSetViews(pi, newviews);

errOut:
    CFReleaseNull(newviews);
    return addedView;
}


SOSViewResultCode SOSViewsDisable(SOSPeerInfoRef pi, CFStringRef viewname, CFErrorRef *error) {
    SOSViewResultCode retval = kSOSCCGeneralViewError;
    CFMutableSetRef newviews = SOSPeerInfoCopyEnabledViews(pi);
    require_action_quiet(newviews, fail,
                         SOSCreateError(kSOSErrorAllocationFailure, viewMemError, NULL, error));
    require_action_quiet(SOSViewsRequireIsKnownView(viewname, error), fail, retval = kSOSCCNoSuchView);

    CFSetRemoveValue(newviews, viewname);
    SOSPeerInfoSetViews(pi, newviews);
    CFReleaseSafe(newviews);
    return kSOSCCViewNotMember;

fail:
    CFReleaseNull(newviews);
    secnotice("views","Failed to disable view(%@): %@", viewname, error ? *error : NULL);
    return retval;
}


bool SOSViewSetDisable(SOSPeerInfoRef pi, CFSetRef viewSet) {
    __block bool removed = false;
    CFMutableSetRef newviews = SOSPeerInfoCopyEnabledViews(pi);
    require_action_quiet(newviews, errOut, secnotice("views", "failed to copy enabled views"));

    CFSetForEach(viewSet, ^(const void *value) {
        CFStringRef viewName = (CFStringRef) value;
        if(SOSViewsIsKnownView(viewName) && CFSetContainsValue(newviews, viewName)) {
            removed = true;
            CFSetRemoveValue(newviews, viewName);
        } else {
            secnotice("views", "couldn't delete view %@", viewName);
        }
    });

    require_quiet(removed, errOut);

    SOSPeerInfoSetViews(pi, newviews);

errOut:
    CFReleaseNull(newviews);
    return removed;
}


SOSViewResultCode SOSViewsQuery(SOSPeerInfoRef pi, CFStringRef viewname, CFErrorRef *error) {
    SOSViewResultCode retval = kSOSCCNoSuchView;
    CFSetRef views = NULL;
    require_quiet(SOSViewsRequireIsKnownView(viewname, error), fail);

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
    secnotice("views","Failed to query view(%@): %@", viewname, error ? *error : NULL);
    CFReleaseNull(views);
    return retval;
}


/* Need XPC way to carry CFSets of views */



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

