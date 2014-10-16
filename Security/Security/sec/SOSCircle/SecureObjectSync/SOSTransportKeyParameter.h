
#ifndef SOSTransportKeyParameter_h
#define SOSTransportKeyParameter_h

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>
#include <SecureObjectSync/SOSAccount.h>

typedef struct __OpaqueSOSTransportKeyParameter * SOSTransportKeyParameterRef;

struct __OpaqueSOSTransportKeyParameter {
    CFRuntimeBase   _base;
    SOSAccountRef   account;
    /* Connections from CF land to vtable land */
    CFStringRef             (*copyDescription)(SOSTransportKeyParameterRef object);
    void                    (*destroy)(SOSTransportKeyParameterRef object);
    
    
    // TODO: Make this take broader parameters and assemble the key parameters blob?
    bool                    (*publishCloudParameters)(SOSTransportKeyParameterRef transport, CFDataRef data, CFErrorRef* error);
    bool                    (*handleKeyParameterChanges)(SOSTransportKeyParameterRef transport, CFDataRef data, CFErrorRef error);
    bool                    (*setToNewAccount)(SOSTransportKeyParameterRef transport);
};

bool SOSTrasnportKeyParameterPublishCloudParameters(SOSTransportKeyParameterRef transport, CFDataRef data, CFErrorRef* error);

bool SOSTrasnportKeyParameterPublishCloudParameters(SOSTransportKeyParameterRef transport, CFDataRef data, CFErrorRef* error);
SOSTransportKeyParameterRef SOSTransportKeyParameterCreateForSubclass(size_t size, SOSAccountRef account, CFErrorRef *error);
bool SOSTransportKeyParameterHandleKeyParameterChanges(SOSTransportKeyParameterRef transport, CFDataRef data, CFErrorRef error);
bool SOSTransportKeyParameterHandleNewAccount(SOSTransportKeyParameterRef transport);

SOSAccountRef SOSTransportKeyParameterGetAccount(SOSTransportKeyParameterRef transport);

#endif
