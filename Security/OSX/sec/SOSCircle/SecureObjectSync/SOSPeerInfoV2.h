//
//  SOSPeerInfoV2.h
//  sec
//
//  Created by Richard Murphy on 1/26/15.
//
//

#ifndef _sec_SOSPeerInfoV2_
#define _sec_SOSPeerInfoV2_

#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecureObjectSync/SOSPeerInfo.h>
#include <Security/SecureObjectSync/SOSPeerInfoPriv.h>
#include <Security/SecureObjectSync/SOSViews.h>
#include <Security/SecureObjectSync/SOSPeerInfoSecurityProperties.h>

// Description Dictionary Entries Added for V2
extern CFStringRef sV2DictionaryKey;            // CFData wrapper for V2 extensions
extern CFStringRef sViewsKey;                   // Set of Views
extern CFStringRef sSerialNumberKey;            // Device Serial Number
extern CFStringRef sSecurityPropertiesKey;      // Set of Security Properties
extern CFStringRef kSOSHsaCrKeyDictionary;      // HSA Challenge-Response area
extern CFStringRef sPreferIDS;                    // Whether or not a peer requires to speak over IDS or KVS
extern CFStringRef sTransportType;              // Dictates the transport type
extern CFStringRef sDeviceID;                   // The IDS device id
extern CFStringRef sRingState;                  // Dictionary of Ring Membership States
extern CFStringRef sBackupKeyKey;
extern CFStringRef sEscrowRecord;

bool SOSPeerInfoUpdateToV2(SOSPeerInfoRef pi, CFErrorRef *error);
void SOSPeerInfoPackV2Data(SOSPeerInfoRef peer);
bool SOSPeerInfoExpandV2Data(SOSPeerInfoRef pi, CFErrorRef *error);
void SOSPeerInfoV2DictionarySetValue(SOSPeerInfoRef peer, const void *key, const void *value);
void SOSPeerInfoV2DictionaryRemoveValue(SOSPeerInfoRef peer, const void *key);

const CFMutableDataRef SOSPeerInfoV2DictionaryCopyData(SOSPeerInfoRef pi, const void *key);
const CFMutableSetRef SOSPeerInfoV2DictionaryCopySet(SOSPeerInfoRef pi, const void *key);
const CFMutableStringRef SOSPeerInfoV2DictionaryCopyString(SOSPeerInfoRef pi, const void *key);
const CFBooleanRef SOSPeerInfoV2DictionaryCopyBoolean(SOSPeerInfoRef pi, const void *key);
const CFMutableDictionaryRef SOSPeerInfoV2DictionaryCopyDictionary(SOSPeerInfoRef pi, const void *key);

bool SOSPeerInfoV2DictionaryHasSet(SOSPeerInfoRef pi, const void *key);
bool SOSPeerInfoV2DictionaryHasData(SOSPeerInfoRef pi, const void *key);
bool SOSPeerInfoV2DictionaryHasString(SOSPeerInfoRef pi, const void *key);
bool SOSPeerInfoV2DictionaryHasBoolean(SOSPeerInfoRef pi, const void *key);

bool SOSPeerInfoV2DictionaryHasSetContaining(SOSPeerInfoRef pi, const void *key, const void* value);
void SOSPeerInfoV2DictionaryForEachSetValue(SOSPeerInfoRef pi, const void *key, void (^action)(const void* value));
void SOSPeerInfoV2DictionaryWithSet(SOSPeerInfoRef pi, const void *key, void(^operation)(CFSetRef set));


bool SOSPeerInfoSerialNumberIsSet(SOSPeerInfoRef pi);
void SOSPeerInfoSetSerialNumber(SOSPeerInfoRef pi);

#endif /* defined(_sec_SOSPeerInfoV2_) */
