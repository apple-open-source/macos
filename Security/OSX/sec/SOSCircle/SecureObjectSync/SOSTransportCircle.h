
#ifndef SOSTransportCircle_h
#define SOSTransportCircle_h

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>
#include <Security/SecureObjectSync/SOSAccount.h>

typedef struct __OpaqueSOSTransportCircle * SOSTransportCircleRef;

struct __OpaqueSOSTransportCircle {
    CFRuntimeBase           _base;
    SOSAccountRef           account;
    
    CFStringRef             (*copyDescription)(SOSTransportCircleRef transport);
    void                    (*destroy)(SOSTransportCircleRef transport);
    
    bool                    (*postRetirement)(SOSTransportCircleRef transport,  CFStringRef circleName, CFStringRef peer_id, CFDataRef retirement_data, CFErrorRef *error);
    bool                    (*expireRetirementRecords)(SOSTransportCircleRef transport, CFDictionaryRef retirements, CFErrorRef *error);
    bool                    (*flushChanges)(SOSTransportCircleRef transport, CFErrorRef *error);
    bool                    (*postCircle)(SOSTransportCircleRef transport,  CFStringRef circleName, CFDataRef circle_data, CFErrorRef *error);
    CFIndex                 (*getTransportType)(SOSTransportCircleRef transport, CFErrorRef *error);
    
    CFDictionaryRef         (*handleRetirementMessages)(SOSTransportCircleRef transport, CFMutableDictionaryRef circle_retirement_messages_table, CFErrorRef *error);
    CFArrayRef              (*handleCircleMessages)(SOSTransportCircleRef transport, CFMutableDictionaryRef circle_circle_messages_table, CFErrorRef *error);
    
    bool                    (*sendPeerInfo)(SOSTransportCircleRef transport, CFStringRef peerID, CFDataRef peerInfoData, CFErrorRef *error);
    bool                    (*flushRingChanges)(SOSTransportCircleRef transport, CFErrorRef* error);
    bool                    (*postRing)(SOSTransportCircleRef transport, CFStringRef ringName, CFDataRef ring, CFErrorRef *error);
    bool                    (*sendDebugInfo)(SOSTransportCircleRef transport, CFStringRef type, CFTypeRef debugInfo, CFErrorRef *error);  
    bool                    (*sendAccountChangedWithDSID)(SOSTransportCircleRef transport, CFStringRef dsid, CFErrorRef *error);
};

SOSTransportCircleRef SOSTransportCircleCreateForSubclass(size_t size, SOSAccountRef account, CFErrorRef *error);

bool SOSTransportCirclePostCircle(SOSTransportCircleRef transport,  CFStringRef circleName, CFDataRef circle_data, CFErrorRef *error);

bool SOSTransportCirclePostRetirement(SOSTransportCircleRef transport,  CFStringRef circleName, SOSPeerInfoRef peer, CFErrorRef *error);

bool SOSTransportCircleExpireRetirementRecords(SOSTransportCircleRef transport, CFDictionaryRef retirements, CFErrorRef *error);

bool SOSTransportCircleFlushChanges(SOSTransportCircleRef transport, CFErrorRef *error);

CFTypeID SOSTransportCircleGetTypeID(void);

CFIndex SOSTransportCircleGetTransportType(SOSTransportCircleRef transport, CFErrorRef *error);

SOSAccountRef SOSTransportCircleGetAccount(SOSTransportCircleRef transport);
CF_RETURNS_RETAINED CFDictionaryRef SOSTransportCircleHandleRetirementMessages(SOSTransportCircleRef transport, CFMutableDictionaryRef circle_retirement_messages_table, CFErrorRef *error);


CF_RETURNS_RETAINED CFArrayRef SOSTransportCircleHandleCircleMessages(SOSTransportCircleRef transport, CFMutableDictionaryRef circle_circle_messages_table, CFErrorRef *error);

bool SOSTransportCircleSendPeerInfo(SOSTransportCircleRef transport, CFStringRef peerID, CFDataRef peerInfoData, CFErrorRef *error);

bool SOSTransportCircleRingFlushChanges(SOSTransportCircleRef transport, CFErrorRef* error);

bool SOSTransportCircleRingPostRing(SOSTransportCircleRef transport, CFStringRef ringName, CFDataRef ring, CFErrorRef *error);

bool SOSTransportCircleSendDebugInfo(SOSTransportCircleRef transport, CFStringRef type, CFTypeRef debugInfo, CFErrorRef *error);

bool SOSTransportCircleSendAccountChangedWithDSID(SOSTransportCircleRef transport, CFStringRef dsid, CFErrorRef *error);

bool SOSTransportCircleSendOfficialDSID(SOSTransportCircleRef transport, CFStringRef dsid, CFErrorRef *error);

bool SOSTransportCircleRecordLastCirclePushedInKVS(SOSTransportCircleRef transport, CFStringRef circle_name, CFDataRef circleData);

#endif
