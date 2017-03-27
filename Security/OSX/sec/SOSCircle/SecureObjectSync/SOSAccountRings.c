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

const CFStringRef kSOSRingKey                   = CFSTR("trusted_rings");

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


    if(CFSetContainsValue(allCurrentRings(), ringName)) {
        retval.ringType = kSOSRingBase;
        retval.dropWhenLeaving = false;
    } else {
        retval.ringType = kSOSRingBackup;
        retval.dropWhenLeaving = false;
    }
    return &retval;
}

__unused static inline void SOSAccountRingForEachRingMatching(SOSAccountRef a, void (^action)(SOSRingRef ring), bool (^condition)(SOSRingRef ring)) {
    CFSetRef allRings = allCurrentRings();
    CFSetForEach(allRings, ^(const void *value) {
        CFStringRef ringName = (CFStringRef) value;
        SOSRingRef ring = SOSAccountCopyRing(a, ringName, NULL);
        if (condition(ring)) {
            action(ring);
        }
        CFReleaseNull(ring);
    });
}

void SOSAccountAddRingDictionary(SOSAccountRef a) {
    if(a->expansion) {
        if(!CFDictionaryGetValue(a->expansion, kSOSRingKey)) {
            CFMutableDictionaryRef rings = CFDictionaryCreateMutableForCFTypes(NULL);
            CFDictionarySetValue(a->expansion, kSOSRingKey, rings);
            CFReleaseNull(rings);
        }
    }
}

static CFMutableDictionaryRef SOSAccountGetRings(SOSAccountRef a, CFErrorRef *error){
    CFMutableDictionaryRef rings = (CFMutableDictionaryRef) CFDictionaryGetValue(a->expansion, kSOSRingKey);
    if(!rings) {
        SOSAccountAddRingDictionary(a);
        rings = SOSAccountGetRings(a, error);
    }
    return rings;
}

static void SOSAccountSetRings(SOSAccountRef a, CFMutableDictionaryRef newrings){
    CFDictionarySetValue(a->expansion, newrings, kSOSRingKey);
}

bool SOSAccountForEachRing(SOSAccountRef account, SOSRingRef (^action)(CFStringRef name, SOSRingRef ring)) {
    bool retval = false;
    __block bool changed = false;
    CFMutableDictionaryRef rings = SOSAccountGetRings(account, NULL);
    CFMutableDictionaryRef ringscopy = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    require_quiet(rings, errOut);
    require_quiet(ringscopy, errOut);
    CFDictionaryForEach(rings, ^(const void *key, const void *value) {
        CFStringRef ringname = (CFStringRef) key;
        CFDataRef   ringder = CFDataCreateCopy(kCFAllocatorDefault, (CFDataRef) value);
        CFDictionaryAddValue(ringscopy, key, ringder);
        SOSRingRef  ring = SOSRingCreateFromData(NULL, ringder);
        SOSRingRef  newring = action(ringname, ring);
        if(newring) {
            CFDataRef   newringder = SOSRingCopyEncodedData(newring, NULL);
            CFDictionaryReplaceValue(ringscopy, key, newringder);
            CFReleaseNull(newringder);
            changed = true;
        }
        CFReleaseNull(ring);
        CFReleaseNull(ringder);
        CFReleaseNull(newring);
    });
    if(changed) {
        SOSAccountSetRings(account, ringscopy);
    }
    retval = true;
errOut:
    CFReleaseNull(ringscopy);
    return retval;
}

CFMutableDictionaryRef SOSAccountGetBackups(SOSAccountRef a, CFErrorRef *error){
    return a->backups;
}

SOSRingRef SOSAccountCopyRing(SOSAccountRef a, CFStringRef ringName, CFErrorRef *error) {
    CFMutableDictionaryRef rings = SOSAccountGetRings(a, error);
    require_action_quiet(rings, errOut, SOSCreateError(kSOSErrorNoRing, CFSTR("No Rings found"), NULL, error));
    CFTypeRef ringder = CFDictionaryGetValue(rings, ringName);
    require_action_quiet(ringder, errOut, SOSCreateError(kSOSErrorNoRing, CFSTR("No Ring found"), NULL, error));
    SOSRingRef ring = SOSRingCreateFromData(NULL, ringder);
    return (SOSRingRef) ring;

errOut:
    return NULL;
}

bool SOSAccountSetRing(SOSAccountRef a, SOSRingRef addRing, CFStringRef ringName, CFErrorRef *error) {
    require_quiet(addRing, errOut);
    CFMutableDictionaryRef rings = SOSAccountGetRings(a, error);
    require_action_quiet(rings, errOut, SOSCreateError(kSOSErrorNoRing, CFSTR("No Rings found"), NULL, error));
    CFDataRef ringder = SOSRingCopyEncodedData(addRing, error);
    require_quiet(ringder, errOut);
    CFDictionarySetValue(rings, ringName, ringder);
    CFReleaseNull(ringder);
    return true;
errOut:
    return false;
}

void SOSAccountRemoveRing(SOSAccountRef a, CFStringRef ringName) {
    CFMutableDictionaryRef rings = SOSAccountGetRings(a, NULL);
    require_quiet(rings, fail);
    CFDictionaryRemoveValue(rings, ringName);
fail:
    return;
}


SOSRingRef SOSAccountCopyRingNamed(SOSAccountRef a, CFStringRef ringName, CFErrorRef *error) {
    SOSRingRef found = SOSAccountCopyRing(a, ringName, error);
    if (isSOSRing(found)) return found;
    if (found) {
        secerror("Non ring in ring table: %@, purging!", found);
        SOSAccountRemoveRing(a, ringName);
    }
    found = NULL;
    return found;
}

/* Unused? */
SOSRingRef SOSAccountRingCreateForName(SOSAccountRef a, CFStringRef ringName, CFErrorRef *error) {
    ringDefPtr rdef = getRingDef(ringName);
    if(!rdef) return NULL;
    SOSRingRef retval = SOSRingCreate(rdef->name, SOSAccountGetMyPeerID(a), rdef->ringType, error);
    return retval;
}

bool SOSAccountCheckForRings(SOSAccountRef a, CFErrorRef *error) {
    __block bool retval = true;
    CFMutableDictionaryRef rings = SOSAccountGetRings(a, error);
    if(rings && isDictionary(rings)) {
        SOSAccountForEachRing(a, ^SOSRingRef(CFStringRef ringname, SOSRingRef ring) {
            if(retval == true) {
                if(!SOSRingIsStable(ring)) {
                    retval = false;
                    secnotice("ring", "Ring %@ not stable", ringname);
                }
            }
            return NULL;
        });
    } else {
        SOSCreateError(kSOSErrorNotReady, CFSTR("Rings not present"), NULL, error);
        retval = false;
    }
    return retval;
}

bool SOSAccountUpdateRingFromRemote(SOSAccountRef account, SOSRingRef newRing, CFErrorRef *error) {
    return SOSAccountHandleUpdateRing(account, newRing, false, error);
}

bool SOSAccountUpdateRing(SOSAccountRef account, SOSRingRef newRing, CFErrorRef *error) {
    return SOSAccountHandleUpdateRing(account, newRing, true, error);
}


/* Unused? */
bool SOSAccountModifyRing(SOSAccountRef account, CFStringRef ringName, CFErrorRef* error, bool (^action)(SOSRingRef ring)) {
    bool success = false;

    SOSRingRef ring = SOSAccountCopyRing(account, ringName, error);
    require_action_quiet(ring, fail, SOSErrorCreate(kSOSErrorNoRing, error, NULL, CFSTR("No Ring to get peer key from")));

    success = true;
    require_quiet(action(ring), fail);

    success = SOSAccountUpdateRing(account, ring, error);

fail:
    CFReleaseSafe(ring);
    return success;
}

/* Unused? */
CFDataRef SOSAccountRingCopyPayload(SOSAccountRef account, CFStringRef ringName, CFErrorRef *error) {
    SOSRingRef ring = SOSAccountCopyRing(account, ringName, error);
    CFDataRef payload = SOSRingGetPayload(ring, error);
    CFDataRef retval = CFDataCreateCopy(kCFAllocatorDefault, payload);
    CFReleaseNull(ring);
    return retval;
}

/* Unused? */
SOSRingRef SOSAccountRingCopyWithPayload(SOSAccountRef account, CFStringRef ringName, CFDataRef payload, CFErrorRef *error) {
    SOSRingRef ring = SOSAccountCopyRing(account, ringName, error);
    require_quiet(ring, errOut);
    CFDataRef oldpayload = SOSRingGetPayload(ring, error);
    require_quiet(!CFEqualSafe(oldpayload, payload), errOut);
    require_quiet(SOSRingSetPayload(ring, NULL, payload, account->my_identity, error), errOut);
errOut:
    return ring;
}

bool SOSAccountResetRing(SOSAccountRef account, CFStringRef ringName, CFErrorRef *error) {
    bool retval = false;
    SOSRingRef ring = SOSAccountCopyRing(account, ringName, error);
    SOSRingRef newring = SOSRingCreate(ringName, NULL, SOSRingGetType(ring), error);
    SOSRingGenerationCreateWithBaseline(newring, ring);
    SOSBackupRingSetViews(newring, account->my_identity, SOSBackupRingGetViews(ring, NULL), error);
    require_quiet(newring, errOut);
    CFReleaseNull(ring);
    retval = SOSAccountUpdateRing(account, newring, error);
errOut:
    CFReleaseNull(newring);
    return retval;
}

bool SOSAccountResetAllRings(SOSAccountRef account, CFErrorRef *error) {
    __block bool retval = true;
    CFMutableSetRef ringList = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
    require_quiet(ringList, errOut);
    
    SOSAccountForEachRing(account, ^SOSRingRef(CFStringRef name, SOSRingRef ring) {
        CFSetAddValue(ringList, name);
        return NULL; // just using this to grab names.
    });
    
    CFSetForEach(ringList, ^(const void *value) {
        CFStringRef ringName = (CFStringRef) value;
        retval = retval && SOSAccountResetRing(account, ringName, error);
    });
    
errOut:
    CFReleaseNull(ringList);
    return retval;
}


bool SOSAccountUpdateNamedRing(SOSAccountRef account, CFStringRef ringName, CFErrorRef *error,
                                      SOSRingRef (^create)(CFStringRef ringName, CFErrorRef *error),
                                      SOSRingRef (^copyModified)(SOSRingRef existing, CFErrorRef *error)) {
    bool result = false;
    SOSRingRef found = SOSAccountCopyRing(account, ringName, error);
    SOSRingRef newRing = NULL;
    if(!found) {
        found = create(ringName, error);
    }
    require_quiet(found, errOut);
    newRing = copyModified(found, error);
    CFReleaseNull(found);
    
    require_quiet(newRing, errOut);
    
    result = SOSAccountHandleUpdateRing(account, newRing, true, error);
    
errOut:
    CFReleaseNull(found);
    CFReleaseNull(newRing);
    return result;
}


