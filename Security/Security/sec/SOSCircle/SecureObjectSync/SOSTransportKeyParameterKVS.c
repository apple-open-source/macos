#include <SecureObjectSync/SOSAccountPriv.h>
#include <SecureObjectSync/SOSTransport.h>
#include <SecureObjectSync/SOSTransportKeyParameter.h>
#include <SecureObjectSync/SOSTransportKeyParameterKVS.h>
#include <SecureObjectSync/SOSKVSKeys.h>
#include <SOSCloudKeychainClient.h>
#include <utilities/SecCFWrappers.h>

static bool SOSTransportKeyParameterKVSPublishCloudParameters(SOSTransportKeyParameterKVSRef transport, CFDataRef newParameters, CFErrorRef *error);
static bool publishCloudParameters(SOSTransportKeyParameterRef transport, CFDataRef data, CFErrorRef* error);
static bool SOSTransportKeyParameterKVSUpdateKVS(CFDictionaryRef changes, CFErrorRef *error);
static void destroy(SOSTransportKeyParameterRef transport);

struct __OpaqueSOSTransportKeyParameterKVS{
    struct __OpaqueSOSTransportKeyParameter     k;
};

static bool handleKeyParameterChanges(SOSTransportKeyParameterRef transport, CFDataRef data, CFErrorRef error){
    SOSAccountRef account = transport->account;
    return SOSAccountHandleParametersChange(account, data, &error);
    
}

static bool setToNewAccount(SOSTransportKeyParameterRef transport){
    SOSAccountRef account = transport->account;
    SOSAccountSetToNew(account);
    return true;
}

SOSTransportKeyParameterKVSRef SOSTransportKeyParameterKVSCreate(SOSAccountRef account, CFErrorRef *error) {
    SOSTransportKeyParameterKVSRef tkvs = (SOSTransportKeyParameterKVSRef) SOSTransportKeyParameterCreateForSubclass(sizeof(struct __OpaqueSOSTransportKeyParameterKVS) - sizeof(CFRuntimeBase), account, error);
    if(tkvs){
        tkvs->k.publishCloudParameters = publishCloudParameters;
        tkvs->k.handleKeyParameterChanges = handleKeyParameterChanges;
        tkvs->k.setToNewAccount = setToNewAccount;
        tkvs->k.destroy = destroy;
        SOSRegisterTransportKeyParameter((SOSTransportKeyParameterRef)tkvs);
    }
    return tkvs;
}

static void destroy(SOSTransportKeyParameterRef transport){
    SOSUnregisterTransportKeyParameter(transport);
}

bool SOSTransportKeyParameterKVSHandleCloudParameterChange(SOSTransportKeyParameterRef transport, CFDataRef data, CFErrorRef* error){
    SOSTransportKeyParameterKVSRef tkvs = (SOSTransportKeyParameterKVSRef)transport;
    SOSAccountRef account = tkvs->k.account;
    
    return SOSAccountHandleParametersChange(account, data, error);
}


bool SOSTransportKeyParameterKVSAppendKeyInterests(SOSTransportKeyParameterKVSRef transport, CFMutableArrayRef alwaysKeys, CFMutableArrayRef afterFirstUnlockKeys, CFMutableArrayRef unlockedKeys, CFErrorRef*error){
    
    CFArrayAppendValue(alwaysKeys, kSOSKVSKeyParametersKey);
    
    return true;
}

static bool publishCloudParameters(SOSTransportKeyParameterRef transport, CFDataRef data, CFErrorRef* error)
{
    return SOSTransportKeyParameterKVSPublishCloudParameters((SOSTransportKeyParameterKVSRef)transport, data, error);
}

static bool SOSTransportKeyParameterKVSUpdateKVS(CFDictionaryRef changes, CFErrorRef *error){
    CloudKeychainReplyBlock log_error = ^(CFDictionaryRef returnedValues __unused, CFErrorRef error) {
        if (error) {
            secerror("Error putting: %@", error);
            CFReleaseSafe(error);
        }
    };
    
    SOSCloudKeychainPutObjectsInCloud(changes, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), log_error);
    return true;
}

static bool SOSTransportKeyParameterKVSPublishCloudParameters(SOSTransportKeyParameterKVSRef transport, CFDataRef newParameters, CFErrorRef *error)
{
    CFDictionaryRef changes = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                           kSOSKVSKeyParametersKey, newParameters,
                                                           NULL);
    
    bool success = SOSTransportKeyParameterKVSUpdateKVS(changes, error);
    
    CFReleaseNull(changes);
    
    return success;
}
