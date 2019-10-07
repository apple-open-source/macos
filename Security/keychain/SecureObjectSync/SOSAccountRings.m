//
//  SOSAccountRings.c
//  sec
//

#include "SOSAccountPriv.h"
#import "keychain/SecureObjectSync/SOSTransport.h"
#import "keychain/SecureObjectSync/SOSRingUtils.h"
#import "keychain/SecureObjectSync/SOSAccountTrust.h"
#import "keychain/SecureObjectSync/SOSAccountTrustClassic+Expansion.h"

#include "AssertMacros.h"

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

__unused static inline void SOSAccountRingForEachRingMatching(SOSAccount* a, void (^action)(SOSRingRef ring), bool (^condition)(SOSRingRef ring)) {
    CFSetRef allRings = allCurrentRings();
    CFSetForEach(allRings, ^(const void *value) {
        CFStringRef ringName = (CFStringRef) value;
        SOSRingRef ring = [a.trust copyRing:ringName err:NULL];
        if (condition(ring)) {
            action(ring);
        }
        CFReleaseNull(ring);
    });
}





static void SOSAccountSetRings(SOSAccount* a, CFMutableDictionaryRef newrings){
    SOSAccountTrustClassic *trust = a.trust;
    [trust.expansion setObject:(__bridge NSMutableDictionary*)newrings forKey:(__bridge NSString* _Nonnull)(kSOSRingKey)];
}

bool SOSAccountForEachRing(SOSAccount* account, SOSRingRef (^action)(CFStringRef name, SOSRingRef ring)) {
    bool retval = false;
    __block bool changed = false;
    __block CFStringRef ringname = NULL;
    __block  CFDataRef   ringder = NULL;
    __block SOSRingRef  ring = NULL;
    __block SOSRingRef  newring = NULL;
    __block CFDataRef   newringder = NULL;

    CFMutableDictionaryRef rings = [account.trust getRings:NULL];
    CFMutableDictionaryRef ringscopy = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    if(!rings){
        CFReleaseNull(ringscopy);
        return retval;
    }
    if(!ringscopy){
        CFReleaseNull(ringscopy);
        return retval;
    }
    CFDictionaryForEach(rings, ^(const void *key, const void *value) {
        ringname = (CFStringRef) key;
        ringder = CFDataCreateCopy(kCFAllocatorDefault, (CFDataRef) value);
        CFDictionaryAddValue(ringscopy, key, ringder);
        ring = SOSRingCreateFromData(NULL, ringder);
        newring = action(ringname, ring);
        if(newring) {
            newringder = SOSRingCopyEncodedData(newring, NULL);
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

    CFReleaseNull(ringscopy);
    return retval;
}

void SOSAccountRemoveRing(SOSAccount* a, CFStringRef ringName) {
    CFMutableDictionaryRef rings = [a.trust getRings:NULL];
    require_quiet(rings, fail);
    CFDictionaryRemoveValue(rings, ringName);
fail:
    return;
}


SOSRingRef SOSAccountCopyRingNamed(SOSAccount* a, CFStringRef ringName, CFErrorRef *error) {
    if(!a.trust)
    {
        return NULL;
    }
    SOSRingRef found = [a.trust copyRing:ringName err:error];

    if (isSOSRing(found)) return found;
    if (found) {
        secerror("Non ring in ring table: %@, purging!", found);
        SOSAccountRemoveRing(a, ringName);
    }
    CFReleaseNull(found); // I'm very skeptical of this function...
    found = NULL;
    return found;
}

/* Unused? */
SOSRingRef SOSAccountRingCreateForName(SOSAccount* a, CFStringRef ringName, CFErrorRef *error) {
    ringDefPtr rdef = getRingDef(ringName);
    if(!rdef) return NULL;
    SOSRingRef retval = SOSRingCreate(rdef->name, (__bridge CFStringRef) a.peerID, rdef->ringType, error);
    return retval;
}

bool SOSAccountUpdateRingFromRemote(SOSAccount* account, SOSRingRef newRing, CFErrorRef *error) {
    require_quiet(SOSAccountHasPublicKey(account, error), errOut);
  
    return [account.trust handleUpdateRing:account prospectiveRing:newRing transport:account.circle_transport userPublicKey:account.accountKey writeUpdate:false err:error];
errOut:
    return false;
}

bool SOSAccountUpdateRing(SOSAccount* account, SOSRingRef newRing, CFErrorRef *error) {
    require_quiet(SOSAccountHasPublicKey(account, error), errOut);

    return [account.trust handleUpdateRing:account prospectiveRing:newRing transport:account.circle_transport userPublicKey:account.accountKey writeUpdate:true err:error];

errOut:
    return false;
}

bool SOSAccountUpdateNamedRing(SOSAccount* account, CFStringRef ringName, CFErrorRef *error,
                                      SOSRingRef (^create)(CFStringRef ringName, CFErrorRef *error),
                                      SOSRingRef (^copyModified)(SOSRingRef existing, CFErrorRef *error)) {
    bool result = false;
    SOSRingRef found = [account.trust copyRing:ringName err:error];

    SOSRingRef newRing = NULL;
    if(!found) {
        found = create(ringName, error);
    }
    require_quiet(found, errOut);
    newRing = copyModified(found, error);
    CFReleaseNull(found);
    
    require_quiet(newRing, errOut);

    require_quiet(SOSAccountHasPublicKey(account, error), errOut);
    require_quiet(SOSAccountHasCircle(account, error), errOut);

    result = [account.trust handleUpdateRing:account prospectiveRing:newRing transport:account.circle_transport userPublicKey:account.accountKey writeUpdate:true err:error];
    
errOut:
    CFReleaseNull(found);
    CFReleaseNull(newRing);
    return result;
}


