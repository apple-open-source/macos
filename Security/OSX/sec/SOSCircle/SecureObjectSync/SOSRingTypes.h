//
//  SOSRingTypes.h
//  sec
//
//  Created by Richard Murphy on 2/23/15.
//
//

#ifndef _sec_SOSRingTypes_
#define _sec_SOSRingTypes_


#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecureObjectSync/SOSAccount.h>
#include <Security/SecureObjectSync/SOSRingUtils.h>
#include "SOSRing.h"


typedef struct ringfuncs_t {
    char                    *typeName;
    int                     version;
    SOSRingRef              (*sosRingCreate)(CFStringRef name, CFStringRef myPeerID, CFErrorRef *error);
    bool                    (*sosRingResetToEmpty)(SOSRingRef ring, CFStringRef myPeerID, CFErrorRef *error);
    bool                    (*sosRingResetToOffering)(SOSRingRef ring, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error);
    SOSRingStatus           (*sosRingDeviceIsInRing)(SOSRingRef ring, CFStringRef peerID);
    bool                    (*sosRingApply)(SOSRingRef ring, SecKeyRef user_pubkey, SOSFullPeerInfoRef requestor, CFErrorRef *error);
    bool                    (*sosRingWithdraw)(SOSRingRef ring, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error);
    bool                    (*sosRingGenerationSign)(SOSRingRef ring, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error);
    bool                    (*sosRingConcordanceSign)(SOSRingRef ring, SOSFullPeerInfoRef requestor, CFErrorRef *error);
    SOSConcordanceStatus    (*sosRingConcordanceTrust)(SOSFullPeerInfoRef me, CFSetRef peers,
                                                       SOSRingRef knownRing, SOSRingRef proposedRing,
                                                       SecKeyRef knownPubkey, SecKeyRef userPubkey,
                                                       CFStringRef excludePeerID, CFErrorRef *error);
    bool                    (*sosRingAccept)(SOSRingRef ring, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error);
    bool                    (*sosRingReject)(SOSRingRef ring, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error);
    bool                    (*sosRingSetPayload)(SOSRingRef ring, SecKeyRef user_privkey, CFDataRef payload, SOSFullPeerInfoRef requestor, CFErrorRef *error);
    CFDataRef               (*sosRingGetPayload)(SOSRingRef ring, CFErrorRef *error);
} ringFuncStruct, *ringFuncs;

// ViewRequirements
bool SOSRingRequirementKnown(SOSAccountRef account, CFStringRef name, CFErrorRef *error);
bool SOSRingRequirementCreate(SOSAccountRef account, CFStringRef name, SOSRingType type, CFErrorRef *error);

// Admins
bool SOSRingRequirementResetToOffering(SOSAccountRef account, CFStringRef name, CFErrorRef* error);
bool SOSRingRequirementResetToEmpty(SOSAccountRef account, CFStringRef name, CFErrorRef* error);

// Clients
bool SOSRingRequirementRequestToJoin(SOSAccountRef account, CFStringRef name, CFErrorRef* error);
bool SOSRingRequirementRemoveThisDevice(SOSAccountRef account, CFStringRef name, CFErrorRef* error);

// Approvers
CFArrayRef SOSRingRequirementGetApplicants(SOSAccountRef account, CFStringRef name, CFErrorRef* error);
bool SOSRingRequirementAcceptApplicants(SOSAccountRef account, CFStringRef name, CFArrayRef applicants, CFErrorRef* error);
bool SOSRingRequirementRejectApplicants(SOSAccountRef account, CFStringRef name, CFArrayRef applicants, CFErrorRef *error);

#endif /* defined(_sec_SOSRingTypes_) */
