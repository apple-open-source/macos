#include <Security/SecureObjectSync/SOSAccountPriv.h>
#include <Security/SecureObjectSync/SOSTransport.h>
#include <Security/SecureObjectSync/SOSTransportKeyParameter.h>
#include <Security/SecureObjectSync/SOSTransportKeyParameterKVS.h>
#include <Security/SecureObjectSync/SOSKVSKeys.h>
#include <SOSCloudKeychainClient.h>
#include <utilities/SecCFWrappers.h>
#include <SOSCloudCircleServer.h>

static bool SOSTransportKeyParameterKVSPublishCloudParameters(SOSTransportKeyParameterKVSRef transport, CFDataRef newParameters, CFErrorRef *error);
static bool publishCloudParameters(SOSTransportKeyParameterRef transport, CFDataRef data, CFErrorRef* error);
static bool SOSTransportKeyParameterKVSUpdateKVS(CFDictionaryRef changes, CFErrorRef *error);
static void destroy(SOSTransportKeyParameterRef transport);
static inline CFIndex getTransportType(SOSTransportKeyParameterRef transport, CFErrorRef *error);

struct __OpaqueSOSTransportKeyParameterKVS{
    struct __OpaqueSOSTransportKeyParameter     k;
};

static bool handleKeyParameterChanges(SOSTransportKeyParameterRef transport, CFDataRef data, CFErrorRef error){
    SOSAccountRef account = transport->account;
    return SOSAccountHandleParametersChange(account, data, &error);
    
}

static inline CFIndex getTransportType(SOSTransportKeyParameterRef transport, CFErrorRef *error){
    return kKVS;
}


static bool setToNewAccount(SOSTransportKeyParameterRef transport, SOSAccountRef account){
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
        tkvs->k.getTransportType = getTransportType;
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
    CloudKeychainReplyBlock log_error = ^(CFDictionaryRef returnedValues __unused, CFErrorRef block_error) {
        if (block_error) {
            secerror("Error putting: %@", block_error);
        }
    };
    
    SOSCloudKeychainPutObjectsInCloud(changes, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), log_error);
    return true;
}

static bool SOSTransportKeyParameterKVSPublishCloudParameters(SOSTransportKeyParameterKVSRef transport, CFDataRef newParameters, CFErrorRef *error)
{
    SOSAccountRef a = SOSTransportKeyParameterGetAccount((SOSTransportKeyParameterRef)transport);
    CFDictionaryRef changes = NULL;
    CFDataRef timeData = NULL;
    bool waitForeverForSynchronization = true;
    
    CFMutableStringRef timeDescription = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, CFSTR("["));
    CFAbsoluteTime currentTimeAndDate = CFAbsoluteTimeGetCurrent();

    withStringOfAbsoluteTime(currentTimeAndDate, ^(CFStringRef decription) {
        CFStringAppend(timeDescription, decription);
    });
    CFStringAppend(timeDescription, CFSTR("]"));

    timeData = CFStringCreateExternalRepresentation(NULL,timeDescription,
        kCFStringEncodingUTF8, '?');

    CFMutableDataRef timeAndKeyParametersMutable = CFDataCreateMutable(kCFAllocatorDefault, CFDataGetLength(timeData) + CFDataGetLength(newParameters)); 
    CFDataAppend(timeAndKeyParametersMutable, timeData);
    CFDataAppend(timeAndKeyParametersMutable, newParameters);
    CFDataRef timeAndKeyParameters = CFDataCreateCopy(kCFAllocatorDefault, timeAndKeyParametersMutable);

    CFStringRef ourPeerID = SOSAccountGetMyPeerID(a);

    if(ourPeerID != NULL){
        CFStringRef keyParamKey = SOSLastKeyParametersPushedKeyCreateWithPeerID(ourPeerID);
        
        changes = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                               kSOSKVSKeyParametersKey, newParameters,
                                               keyParamKey, timeAndKeyParameters,
                                               NULL);
        CFReleaseNull(keyParamKey);
    }
    else
    {
        CFStringRef keyParamKeyWithAccount = SOSLastKeyParametersPushedKeyCreateWithAccountGestalt(a);
        changes = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                               kSOSKVSKeyParametersKey, newParameters,
                                               keyParamKeyWithAccount, timeAndKeyParameters,
                                               NULL);
        CFReleaseNull(keyParamKeyWithAccount);
        
    }
    bool success = SOSTransportKeyParameterKVSUpdateKVS(changes, error);
    
    sync_the_last_data_to_kvs(a, waitForeverForSynchronization);
    
    CFReleaseNull(changes);
    CFReleaseNull(timeAndKeyParametersMutable);
    CFReleaseNull(timeAndKeyParameters);
    CFReleaseNull(timeData);
    CFReleaseNull(timeDescription);
    return success;
}
