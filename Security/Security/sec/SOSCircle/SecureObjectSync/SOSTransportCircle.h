
#ifndef SOSTransportCircle_h
#define SOSTransportCircle_h

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>
#include <SecureObjectSync/SOSAccount.h>

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
    CFDictionaryRef         (*handleRetirementMessages)(SOSTransportCircleRef transport, CFMutableDictionaryRef circle_retirement_messages_table, CFErrorRef *error);
    CFArrayRef              (*handleCircleMessages)(SOSTransportCircleRef transport, CFMutableDictionaryRef circle_circle_messages_table, CFErrorRef *error);
    
};

SOSTransportCircleRef SOSTransportCircleCreateForSubclass(size_t size, SOSAccountRef account, CFErrorRef *error);

bool SOSTransportCirclePostCircle(SOSTransportCircleRef transport,  CFStringRef circleName, CFDataRef circle_data, CFErrorRef *error);

bool SOSTransportCirclePostRetirement(SOSTransportCircleRef transport,  CFStringRef circleName, CFStringRef peer_id, CFDataRef retirement_data, CFErrorRef *error);

bool SOSTransportCircleExpireRetirementRecords(SOSTransportCircleRef transport, CFDictionaryRef retirements, CFErrorRef *error);

bool SOSTransportCircleFlushChanges(SOSTransportCircleRef transport, CFErrorRef *error);


SOSAccountRef SOSTransportCircleGetAccount(SOSTransportCircleRef transport);
CF_RETURNS_RETAINED CFDictionaryRef SOSTransportCircleHandleRetirementMessages(SOSTransportCircleRef transport, CFMutableDictionaryRef circle_retirement_messages_table, CFErrorRef *error);


CF_RETURNS_RETAINED CFArrayRef SOSTransportCircleHandleCircleMessages(SOSTransportCircleRef transport, CFMutableDictionaryRef circle_circle_messages_table, CFErrorRef *error);


#endif
