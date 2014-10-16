
#include <SecureObjectSync/SOSAccount.h>
#include <SecureObjectSync/SOSTransport.h>
#include <SecureObjectSync/SOSTransportKeyParameter.h>
#include <SecureObjectSync/SOSKVSKeys.h>

#include <utilities/SecCFWrappers.h>
#include <SecureObjectSync/SOSAccountPriv.h>
#include <SOSCloudKeychainClient.h>

CFGiblisWithCompareFor(SOSTransportKeyParameter);

SOSTransportKeyParameterRef SOSTransportKeyParameterCreateForSubclass(size_t size, SOSAccountRef account, CFErrorRef *error)
{
    SOSTransportKeyParameterRef tpt = CFTypeAllocateWithSpace(SOSTransportKeyParameter, size, kCFAllocatorDefault);
    tpt->account = CFRetainSafe(account);
    return tpt;
}

bool SOSTransportKeyParameterHandleNewAccount(SOSTransportKeyParameterRef transport){
    return transport->setToNewAccount(transport);
}

static CFStringRef SOSTransportKeyParameterCopyDescription(CFTypeRef aObj) {
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


bool SOSTrasnportKeyParameterPublishCloudParameters(SOSTransportKeyParameterRef transport, CFDataRef data, CFErrorRef* error) {
    return transport->publishCloudParameters(transport, data, error);
}

SOSAccountRef SOSTransportKeyParameterGetAccount(SOSTransportKeyParameterRef transport){
    return transport->account;
}
