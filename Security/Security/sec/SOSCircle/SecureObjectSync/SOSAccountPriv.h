//
//  SOSAccountPriv.h
//  sec
//

#ifndef sec_SOSAccountPriv_h
#define sec_SOSAccountPriv_h

#include "SOSAccount.h"

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecCFError.h>
#include <utilities/SecAKSWrappers.h>


#include <Security/SecKeyPriv.h>

#include <utilities/der_plist.h>
#include <utilities/der_plist_internal.h>
#include <corecrypto/ccder.h>

#include <AssertMacros.h>
#include <assert.h>

#import <notify.h>

#include <SecureObjectSync/SOSInternal.h>
#include <SecureObjectSync/SOSCircle.h>
#include <SecureObjectSync/SOSCloudCircle.h>
#include <securityd/SOSCloudCircleServer.h>
#include <SecureObjectSync/SOSEngine.h>
#include <SecureObjectSync/SOSPeer.h>
#include <SecureObjectSync/SOSFullPeerInfo.h>
#include <SecureObjectSync/SOSPeerInfo.h>
#include <SecureObjectSync/SOSPeerInfoInternal.h>
#include <SecureObjectSync/SOSUserKeygen.h>
#include <utilities/iCloudKeychainTrace.h>

#include <Security/SecItemPriv.h>

struct __OpaqueSOSAccount {
    CFRuntimeBase           _base;
    
    dispatch_queue_t        queue;
    
    CFDictionaryRef         gestalt;
    
    CFMutableDictionaryRef  circle_identities;
    CFMutableDictionaryRef  circles;
    CFMutableDictionaryRef  retired_peers;
    
    bool      user_public_trusted;
    CFDataRef user_key_parameters;
    SecKeyRef user_public;
    SecKeyRef previous_public;
    enum DepartureReason    departure_code;
    
    // Non-persistent data
    
    SOSDataSourceFactoryRef factory;
    SecKeyRef _user_private;
    dispatch_source_t user_private_timer;
    int               lock_notification_token;
    
    SOSTransportKeyParameterRef key_transport;
    CFMutableDictionaryRef circle_transports;
    CFMutableDictionaryRef message_transports;
    
    // Live Notification
    CFMutableArrayRef       change_blocks;
};

SOSAccountRef SOSAccountCreateBasic(CFAllocatorRef allocator,
                                    CFDictionaryRef gestalt,
                                    SOSDataSourceFactoryRef factory);

bool SOSAccountEnsureFactoryCircles(SOSAccountRef a);

void SOSAccountSetToNew(SOSAccountRef a);

void SOSAccountForEachKnownCircle(SOSAccountRef account,
                                  void (^handle_incompatible)(CFStringRef name),
                                  void (^handle_no_peer)(SOSCircleRef circle),
                                  void (^handle_peer)(SOSCircleRef circle, SOSFullPeerInfoRef full_peer));

bool SOSAccountIsMyPeerActiveInCircle(SOSAccountRef account, SOSCircleRef circle, CFErrorRef* error);
bool SOSAccountIsMyPeerActiveInCircleNamed(SOSAccountRef account, CFStringRef circle_name, CFErrorRef* error);

// DER Stuff


size_t der_sizeof_data_or_null(CFDataRef data, CFErrorRef* error);

uint8_t* der_encode_data_or_null(CFDataRef data, CFErrorRef* error, const uint8_t* der, uint8_t* der_end);

const uint8_t* der_decode_data_or_null(CFAllocatorRef allocator, CFDataRef* data,
                                       CFErrorRef* error,
                                       const uint8_t* der, const uint8_t* der_end);

size_t der_sizeof_public_bytes(SecKeyRef publicKey, CFErrorRef* error);

uint8_t* der_encode_public_bytes(SecKeyRef publicKey, CFErrorRef* error, const uint8_t* der, uint8_t* der_end);

const uint8_t* der_decode_public_bytes(CFAllocatorRef allocator, CFIndex algorithmID, SecKeyRef* publicKey, CFErrorRef* error, const uint8_t* der, const uint8_t* der_end);

const uint8_t* ccder_decode_bool(bool* boolean, const uint8_t* der, const uint8_t *der_end);

size_t ccder_sizeof_bool(bool value __unused, CFErrorRef *error);

uint8_t* ccder_encode_bool(bool value, const uint8_t *der, uint8_t *der_end);

// Persistence


SOSAccountRef SOSAccountCreateFromDER_V1(CFAllocatorRef allocator,
                                         SOSDataSourceFactoryRef factory,
                                         CFErrorRef* error,
                                         const uint8_t** der_p, const uint8_t *der_end);

SOSAccountRef SOSAccountCreateFromDER_V2(CFAllocatorRef allocator,
                                         SOSDataSourceFactoryRef factory,
                                         CFErrorRef* error,
                                         const uint8_t** der_p, const uint8_t *der_end);

SOSAccountRef SOSAccountCreateFromDER_V3(CFAllocatorRef allocator,
                                         SOSDataSourceFactoryRef factory,
                                         CFErrorRef* error,
                                         const uint8_t** der_p, const uint8_t *der_end);

SOSAccountRef SOSAccountCreateFromDER(CFAllocatorRef allocator,
                                      SOSDataSourceFactoryRef factory,
                                      CFErrorRef* error,
                                      const uint8_t** der_p, const uint8_t *der_end);

SOSAccountRef SOSAccountCreateFromData(CFAllocatorRef allocator, CFDataRef circleData,
                                       SOSDataSourceFactoryRef factory,
                                       CFErrorRef* error);

size_t SOSAccountGetDEREncodedSize(SOSAccountRef account, CFErrorRef *error);

uint8_t* SOSAccountEncodeToDER(SOSAccountRef account, CFErrorRef* error, const uint8_t* der, uint8_t* der_end);

size_t SOSAccountGetDEREncodedSize_V3(SOSAccountRef account, CFErrorRef *error);

uint8_t* SOSAccountEncodeToDER_V3(SOSAccountRef account, CFErrorRef* error, const uint8_t* der, uint8_t* der_end);

size_t SOSAccountGetDEREncodedSize_V2(SOSAccountRef account, CFErrorRef *error);

uint8_t* SOSAccountEncodeToDER_V2(SOSAccountRef account, CFErrorRef* error, const uint8_t* der, uint8_t* der_end);

size_t SOSAccountGetDEREncodedSize_V1(SOSAccountRef account, CFErrorRef *error);

uint8_t* SOSAccountEncodeToDER_V1(SOSAccountRef account, CFErrorRef* error, const uint8_t* der, uint8_t* der_end);

CFDataRef SOSAccountCopyEncodedData(SOSAccountRef account, CFAllocatorRef allocator, CFErrorRef *error);

// Update

bool SOSAccountHandleCircleMessage(SOSAccountRef account,
                                   CFStringRef circleName, CFDataRef encodedCircleMessage, CFErrorRef *error);

void SOSAccountRecordRetiredPeerInCircleNamed(SOSAccountRef account, CFStringRef circleName, SOSPeerInfoRef retiree);


bool SOSAccountHandleUpdateCircle(SOSAccountRef account,
                                  SOSCircleRef prospective_circle,
                                  bool writeUpdate,
                                  CFErrorRef *error);

// Circles

void SOSAccountForEachKnownCircle(SOSAccountRef account,
                                  void (^handle_incompatible)(CFStringRef name),
                                  void (^handle_no_peer)(SOSCircleRef circle),
                                  void (^handle_peer)(SOSCircleRef circle, SOSFullPeerInfoRef full_peer));

int SOSAccountCountCircles(SOSAccountRef a);

SOSFullPeerInfoRef SOSAccountMakeMyFullPeerInCircleNamed(SOSAccountRef account, CFStringRef name, CFErrorRef *error);

bool SOSAccountDestroyCirclePeerInfoNamed(SOSAccountRef account, CFStringRef name, CFErrorRef* error);

bool SOSAccountDestroyCirclePeerInfo(SOSAccountRef account, SOSCircleRef circle, CFErrorRef* error);

SOSFullPeerInfoRef SOSAccountGetMyFullPeerInCircle(SOSAccountRef account, SOSCircleRef circle, CFErrorRef* error);

SOSPeerInfoRef SOSAccountGetMyPeerInCircle(SOSAccountRef account, SOSCircleRef circle, CFErrorRef* error);

SOSPeerInfoRef SOSAccountGetMyPeerInCircleNamed(SOSAccountRef account, CFStringRef name, CFErrorRef *error);

bool SOSAccountIsActivePeerInCircleNamed(SOSAccountRef account, CFStringRef circle_name, CFStringRef peerid, CFErrorRef* error);

bool SOSAccountIsMyPeerActiveInCircle(SOSAccountRef account, SOSCircleRef circle, CFErrorRef* error);

SOSCircleRef SOSAccountFindCircle(SOSAccountRef a, CFStringRef name, CFErrorRef *error);

SOSCircleRef SOSAccountEnsureCircle(SOSAccountRef a, CFStringRef name, CFErrorRef *error);

bool SOSAccountUpdateCircleFromRemote(SOSAccountRef account, SOSCircleRef newCircle, CFErrorRef *error);

bool SOSAccountUpdateCircle(SOSAccountRef account, SOSCircleRef newCircle, CFErrorRef *error);

bool SOSAccountModifyCircle(SOSAccountRef account,
                            CFStringRef circleName,
                            CFErrorRef* error,
                            bool (^action)(SOSCircleRef circle));

SOSFullPeerInfoRef SOSAccountGetMyFullPeerInCircleNamedIfPresent(SOSAccountRef account, CFStringRef name, CFErrorRef *error);

void AppendCircleKeyName(CFMutableArrayRef array, CFStringRef name);

CFStringRef SOSInterestListCopyDescription(CFArrayRef interests);


// Peers and PeerInfos
bool SOSAccountDestroyCirclePeerInfoNamed(SOSAccountRef account, CFStringRef name, CFErrorRef* error);

bool SOSAccountDestroyCirclePeerInfo(SOSAccountRef account, SOSCircleRef circle, CFErrorRef* error);

SOSPeerInfoRef SOSAccountGetMyPeerInCircle(SOSAccountRef account, SOSCircleRef circle, CFErrorRef* error);

SOSPeerInfoRef SOSAccountGetMyPeerInCircleNamed(SOSAccountRef account, CFStringRef name, CFErrorRef *error);

bool SOSAccountIsActivePeerInCircleNamed(SOSAccountRef account, CFStringRef circle_name, CFStringRef peerid, CFErrorRef* error);

bool SOSAccountIsMyPeerActiveInCircle(SOSAccountRef account, SOSCircleRef circle, CFErrorRef* error);

// FullPeerInfos - including Cloud Identity
SOSFullPeerInfoRef CopyCloudKeychainIdentity(SOSPeerInfoRef cloudPeer, CFErrorRef *error);

SOSFullPeerInfoRef SOSAccountGetMyFullPeerInCircleNamedIfPresent(SOSAccountRef account, CFStringRef name, CFErrorRef *error);

bool SOSAccountIsAccountIdentity(SOSAccountRef account, SOSPeerInfoRef peer_info, CFErrorRef *error);

SOSFullPeerInfoRef SOSAccountMakeMyFullPeerInCircleNamed(SOSAccountRef account, CFStringRef name, CFErrorRef *error);

SOSFullPeerInfoRef SOSAccountGetMyFullPeerInCircle(SOSAccountRef account, SOSCircleRef circle, CFErrorRef* error);

SOSPeerInfoRef GenerateNewCloudIdentityPeerInfo(CFErrorRef *error);

// Credentials
bool SOSAccountHasPublicKey(SOSAccountRef account, CFErrorRef* error);
void SOSAccountSetPreviousPublic(SOSAccountRef account);
bool SOSAccountPublishCloudParameters(SOSAccountRef account, CFErrorRef* error);
bool SOSAccountRetrieveCloudParameters(SOSAccountRef account, SecKeyRef *newKey,
                                       CFDataRef derparms,
                                       CFDataRef *newParameters, CFErrorRef* error);

//Testing
void SOSAccountSetUserPublicTrustedForTesting(SOSAccountRef account);
CFDictionaryRef SOSAccountGetMessageTransports(SOSAccountRef account);
// Utility


static inline void CFArrayAppendValueIfNot(CFMutableArrayRef array, CFTypeRef value, CFTypeRef excludedValue)
{
    if (!CFEqualSafe(value, excludedValue))
        CFArrayAppendValue(array, value);
}

static inline CFMutableDictionaryRef CFDictionaryEnsureCFDictionaryAndGetCurrentValue(CFMutableDictionaryRef dict, CFTypeRef key)
{
    CFMutableDictionaryRef result = (CFMutableDictionaryRef) CFDictionaryGetValue(dict, key);

    if (!isDictionary(result)) {
        result = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
        CFDictionarySetValue(dict, key, result);
        CFReleaseSafe(result);
    }

    return result;
}

static inline CFMutableArrayRef CFDictionaryEnsureCFArrayAndGetCurrentValue(CFMutableDictionaryRef dict, CFTypeRef key)
{
    CFMutableArrayRef result = (CFMutableArrayRef) CFDictionaryGetValue(dict, key);

    if (!isArray(result)) {
        result = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
        CFDictionarySetValue(dict, key, result);
        CFReleaseSafe(result);
    }

    return result;
}

bool sosAccountLeaveCircle(SOSAccountRef account, SOSCircleRef circle, CFErrorRef* error);

bool SOSAccountEnsurePeerRegistration(SOSAccountRef account, CFErrorRef *error);
    
#endif
