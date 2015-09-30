
#include <Security/SecureObjectSync/SOSAccount.h>
#include <Security/SecureObjectSync/SOSTransport.h>
#include <Security/SecureObjectSync/SOSTransportKeyParameter.h>
#include <Security/SecureObjectSync/SOSKVSKeys.h>

#include <utilities/SecCFWrappers.h>
#include <Security/SecureObjectSync/SOSAccountPriv.h>
#include <SOSCloudKeychainClient.h>

CFGiblisWithCompareFor(SOSTransportKeyParameter);

SOSTransportKeyParameterRef SOSTransportKeyParameterCreateForSubclass(size_t size, SOSAccountRef account, CFErrorRef *error)
{
    SOSTransportKeyParameterRef tpt = CFTypeAllocateWithSpace(SOSTransportKeyParameter, size, kCFAllocatorDefault);
    tpt->account = CFRetainSafe(account);
    return tpt;
}

bool SOSTransportKeyParameterHandleNewAccount(SOSTransportKeyParameterRef transport, SOSAccountRef account){
    return transport->setToNewAccount(transport, account);
}

static CFStringRef SOSTransportKeyParameterCopyFormatDescription(CFTypeRef aObj, CFDictionaryRef formatOptions) {
    SOSTransportKeyParameterRef t = (SOSTransportKeyParameterRef) aObj;
    
    return CFStringCreateWithFormat(NULL, NULL, CFSTR("<SOSTransportKeyParameter@%p\n>"), t);
}

static void SOSTransportKeyParameterDestroy(CFTypeRef aObj) {
    SOSTransportKeyParameterRef transport = (SOSTransportKeyParameterRef) aObj;
   
    if(transport->destroy)
        transport->destroy(transport);
    
    CFReleaseNull(transport->account);
    
}

bool SOSTransportKeyParameterHandleKeyParameterChanges(SOSTransportKeyParameterRef transport, CFDataRef data, CFErrorRef error){
    return transport->handleKeyParameterChanges(transport, data, error);
}


static CFHashCode SOSTransportKeyParameterHash(CFTypeRef obj)
{
    return (intptr_t) obj;
}

static Boolean SOSTransportKeyParameterCompare(CFTypeRef lhs, CFTypeRef rhs)
{
    return SOSTransportKeyParameterHash(lhs) == SOSTransportKeyParameterHash(rhs);
}


bool SOSTransportKeyParameterPublishCloudParameters(SOSTransportKeyParameterRef transport, CFDataRef data, CFErrorRef* error) {
    return transport->publishCloudParameters(transport, data, error);
}

SOSAccountRef SOSTransportKeyParameterGetAccount(SOSTransportKeyParameterRef transport){
    return transport->account;
}


CFIndex SOSTransportKeyParameterGetTransportType(SOSTransportKeyParameterRef transport, CFErrorRef *error){
    return transport->getTransportType ? transport->getTransportType(transport, error) : kUnknown;
}
