//
//  SOSAccountRings.c
//  sec
//

#include "SOSAccountPriv.h"
#include <Security/SecureObjectSync/SOSTransport.h>
#include <Security/SecureObjectSync/SOSRingUtils.h>

//
// MARK: Ring management
//

const CFStringRef kSOSRingCircleV2              = CFSTR("Ring-CircleV2");
const CFStringRef kSOSRingKeychainV0            = CFSTR("Ring-KeychainV0");
const CFStringRef kSOSRingPCSHyperion           = CFSTR("Ring-PCS-Photos");
const CFStringRef kSOSRingPCSBladerunner        = CFSTR("Ring-PCS-iCloudDrive");
const CFStringRef kSOSRingPCSLiverpool          = CFSTR("Ring-PCS-CloudKit");
const CFStringRef kSOSRingPCSEscrow             = CFSTR("Ring-PCS-Escrow");
const CFStringRef kSOSRingPCSPianoMover         = CFSTR("Ring-PCS-Maildrop");
const CFStringRef kSOSRingPCSNotes              = CFSTR("Ring-PCS-Notes");
const CFStringRef kSOSRingPCSFeldspar           = CFSTR("Ring-PCS-Feldspar");
const CFStringRef kSOSRingAppleTV               = CFSTR("Ring-AppleTV");
const CFStringRef kSOSRingHomeKit               = CFSTR("Ring-HomeKit");
const CFStringRef kSOSRingWifi                  = CFSTR("Ring-WiFi");
const CFStringRef kSOSRingPasswords             = CFSTR("Ring-Passwords");
const CFStringRef kSOSRingCreditCards           = CFSTR("Ring-CreditCards");
const CFStringRef kSOSRingiCloudIdentity        = CFSTR("Ring-iCloudIdentity");
const CFStringRef kSOSRingOtherSyncable         = CFSTR("Ring-OtherSyncable");


static CFSetRef allCurrentRings(void) {
    static dispatch_once_t dot;
    static CFMutableSetRef allRings = NULL;
    dispatch_once(&dot, ^{
        allRings = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);
        CFSetAddValue(allRings, kSOSRingCircleV2);
        CFSetAddValue(allRings, kSOSRingKeychainV0);
        CFSetAddValue(allRings, kSOSRingPCSHyperion);
        CFSetAddValue(allRings, kSOSRingPCSBladerunner);
        CFSetAddValue(allRings, kSOSRingPCSLiverpool);
        CFSetAddValue(allRings, kSOSRingPCSEscrow);
        CFSetAddValue(allRings, kSOSRingPCSPianoMover);
        CFSetAddValue(allRings, kSOSRingPCSNotes);
        CFSetAddValue(allRings, kSOSRingPCSFeldspar);
        CFSetAddValue(allRings, kSOSRingAppleTV);
        CFSetAddValue(allRings, kSOSRingHomeKit);
        CFSetAddValue(allRings, kSOSRingWifi);
        CFSetAddValue(allRings, kSOSRingPasswords);
        CFSetAddValue(allRings, kSOSRingCreditCards);
        CFSetAddValue(allRings, kSOSRingiCloudIdentity);
        CFSetAddValue(allRings, kSOSRingOtherSyncable);
    });
    return allRings;
}

typedef struct ringDef_t {
    CFStringRef name;
    SOSRingType ringType;
    bool dropWhenLeaving;
} ringDef, *ringDefPtr;

static ringDefPtr getRingDef(CFStringRef ringName) {
    static ringDef retval;

    // Defaults
    retval.name = ringName;
    retval.dropWhenLeaving = true;
    retval.ringType = kSOSRingEntropyKeyed;


    if(CFEqual(ringName, kSOSRingKeychainV0) == 0) {
    } else if(CFEqual(ringName, kSOSRingPCSHyperion) == 0) {
    } else if(CFEqual(ringName, kSOSRingPCSBladerunner) == 0) {
    } else if(CFEqual(ringName, kSOSRingKeychainV0) == 0) {
    } else if(CFEqual(ringName, kSOSRingKeychainV0) == 0) {
    } else if(CFEqual(ringName, kSOSRingKeychainV0) == 0) {
    } else if(CFEqual(ringName, kSOSRingKeychainV0) == 0) {
    } else if(CFEqual(ringName, kSOSRingCircleV2) == 0) {
        retval.ringType = kSOSRingBase;
        retval.dropWhenLeaving = false;
    } else return NULL;
    return &retval;
}

#if 0
static bool isRingKnown(CFStringRef ringname) {
    if(getRingDef(ringname) != NULL) return true;
    secnotice("rings","Not a known ring");
    return false;
}
#endif

static inline void SOSAccountRingForEach(void (^action)(CFStringRef ringname)) {
    CFSetRef allRings = allCurrentRings();
    CFSetForEach(allRings, ^(const void *value) {
        CFStringRef ringName = (CFStringRef) value;
            action(ringName);
    });
}


__unused static inline void SOSAccountRingForEachRingMatching(SOSAccountRef a, void (^action)(SOSRingRef ring), bool (^condition)(SOSRingRef ring)) {
    CFSetRef allRings = allCurrentRings();
    CFSetForEach(allRings, ^(const void *value) {
        CFStringRef ringName = (CFStringRef) value;
        SOSRingRef ring = SOSAccountGetRing(a, ringName, NULL);
        if (condition(ring))
            action(ring);
    });
}

CFMutableDictionaryRef SOSAccountGetRings(SOSAccountRef a, CFErrorRef *error){
    return a->trusted_rings;
}

CFMutableDictionaryRef SOSAccountGetBackups(SOSAccountRef a, CFErrorRef *error){
    return a->backups;
}

SOSRingRef SOSAccountGetRing(SOSAccountRef a, CFStringRef ringName, CFErrorRef *error) {
    CFTypeRef entry = CFDictionaryGetValue(a->trusted_rings, ringName);
    require_action_quiet(entry, fail,
                         SOSCreateError(kSOSErrorNoRing, CFSTR("No Ring found"), NULL, error));
    return (SOSRingRef) entry;

fail:
    return NULL;
}

CFStringRef SOSAccountGetMyPeerID(SOSAccountRef a) {
    SOSFullPeerInfoRef fpi = SOSAccountGetMyFullPeerInfo(a);
    require_quiet(fpi, errOut);
    SOSPeerInfoRef pi = SOSFullPeerInfoGetPeerInfo(fpi);
    require_quiet(pi, errOut);
    return SOSPeerInfoGetPeerID(pi);
errOut:
    return NULL;
}

SOSRingRef SOSAccountRingCreateForName(SOSAccountRef a, CFStringRef ringName, CFErrorRef *error) {
    ringDefPtr rdef = getRingDef(ringName);
    if(!rdef) return NULL;
    SOSRingRef retval = SOSRingCreate(rdef->name, SOSAccountGetMyPeerID(a), rdef->ringType, error);
    return retval;
}

bool SOSAccountCheckForRings(SOSAccountRef a, CFErrorRef *error) {
    bool retval = isDictionary(a->trusted_rings);
    if(!retval) SOSCreateError(kSOSErrorNotReady, CFSTR("Rings not present"), NULL, error);
    return retval;
}

bool SOSAccountEnsureRings(SOSAccountRef a, CFErrorRef *error) {
    bool status = false;

    if(!a->trusted_rings) {
        a->trusted_rings = CFDictionaryCreateMutableForCFTypes(NULL);
    }

    require_quiet(SOSAccountEnsureFullPeerAvailable(a, error), errOut);

    SOSAccountRingForEach(^(CFStringRef ringname) {
        SOSRingRef ring = SOSAccountGetRing(a, ringname, NULL);
        if(!ring) {
            ring = SOSAccountRingCreateForName(a, ringname, error);
            if(ring) {
                CFDictionaryAddValue(a->trusted_rings, ringname, ring);
                SOSUpdateKeyInterest(a);
            }
            CFReleaseNull(ring);
        }
    });

    status = true;
errOut:
    return status;
}

bool SOSAccountUpdateRingFromRemote(SOSAccountRef account, SOSRingRef newRing, CFErrorRef *error) {
    return SOSAccountHandleUpdateRing(account, newRing, false, error);
}

bool SOSAccountUpdateRing(SOSAccountRef account, SOSRingRef newRing, CFErrorRef *error) {
    return SOSAccountHandleUpdateRing(account, newRing, true, error);
}

bool SOSAccountModifyRing(SOSAccountRef account, CFStringRef ringName, CFErrorRef* error, bool (^action)(SOSRingRef ring)) {
    bool success = false;

    SOSRingRef ring = SOSAccountGetRing(account, ringName, error);
    require_action_quiet(ring, fail, SOSErrorCreate(kSOSErrorNoRing, error, NULL, CFSTR("No Ring to get peer key from")));

    ring = SOSRingCopyRing(ring, error);
    require_quiet(ring, fail);

    success = true;
    require_quiet(action(ring), fail);

    success = SOSAccountUpdateRing(account, ring, error);

fail:
    CFReleaseSafe(ring);
    return success;
}

CFDataRef SOSAccountRingGetPayload(SOSAccountRef account, CFStringRef ringName, CFErrorRef *error) {
    SOSRingRef ring = SOSAccountGetRing(account, ringName, error);
    return SOSRingGetPayload(ring, error);
}

SOSRingRef SOSAccountRingCopyWithPayload(SOSAccountRef account, CFStringRef ringName, CFDataRef payload, CFErrorRef *error) {
    SOSRingRef ring = SOSAccountGetRing(account, ringName, error);
    require_quiet(ring, errOut);
    SOSRingRef new = SOSRingCopyRing(ring, error);
    require_quiet(new, errOut);
    CFDataRef oldpayload = SOSRingGetPayload(ring, error);
    require_quiet(!CFEqualSafe(oldpayload, payload), errOut);
    require_quiet(SOSRingSetPayload(new, NULL, payload, account->my_identity, error), errOut);

errOut:
    return NULL;
}

bool SOSAccountResetRing(SOSAccountRef account, CFStringRef ringName, CFErrorRef *error) {
    bool retval = false;
    SOSRingRef ring = SOSAccountGetRing(account, ringName, error);
    SOSRingRef newring = SOSRingCreate(ringName, NULL, SOSRingGetType(ring), error);
    SOSRingGenerationCreateWithBaseline(newring, ring);
    SOSBackupRingSetViews(newring, account->my_identity, SOSBackupRingGetViews(ring, NULL), error);
    require_quiet(newring, errOut);
    retval = SOSAccountUpdateRing(account, newring, error);
errOut:
    CFReleaseNull(newring);
    return retval;
}

bool SOSAccountResetAllRings(SOSAccountRef account, CFErrorRef *error) {
    __block bool retval = true;
    CFDictionaryForEach(account->trusted_rings, ^(const void *key, const void *value) {
        CFStringRef ringName = (CFStringRef) key;
        retval = retval && SOSAccountResetRing(account, ringName, error);
    });
    return retval;
}

