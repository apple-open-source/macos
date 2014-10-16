
#include <CoreFoundation/CoreFoundation.h>
#include <SecureObjectSync/SOSTransportCircle.h>
#include <SecureObjectSync/SOSTransportCircleKVS.h>
#include <SecureObjectSync/SOSTransport.h>
#include <SecureObjectSync/SOSKVSKeys.h>
#include <SecureObjectSync/SOSAccount.h>
#include <SecureObjectSync/SOSAccountPriv.h>

#include <utilities/SecCFWrappers.h>
#include <SOSInternal.h>
#include <SOSCloudKeychainClient.h>
#include <AssertMacros.h>

CFGiblisWithCompareFor(SOSTransportCircle);

SOSTransportCircleRef SOSTransportCircleCreateForSubclass(size_t size, SOSAccountRef account, CFErrorRef *error)
{
    SOSTransportCircleRef tpt = CFTypeAllocateWithSpace(SOSTransportCircle, size, kCFAllocatorDefault);
    tpt->account = CFRetainSafe(account);
    return tpt;
}

static CFStringRef SOSTransportCircleCopyDescription(CFTypeRef aObj) {
    SOSTransportCircleRef t = (SOSTransportCircleRef) aObj;
    
    return CFStringCreateWithFormat(NULL, NULL, CFSTR("<SOSTransportCircle@%p\n>"), t);
}

static void SOSTransportCircleDestroy(CFTypeRef aObj) {
    SOSTransportCircleRef transport = (SOSTransportCircleRef) aObj;
    if(transport->destroy)
        transport->destroy(transport);
    CFReleaseNull(transport->account);
}

static CFHashCode SOSTransportCircleHash(CFTypeRef obj)
{
    return (intptr_t) obj;
}

static Boolean SOSTransportCircleCompare(CFTypeRef lhs, CFTypeRef rhs)
{
    return SOSTransportCircleHash(lhs) == SOSTransportCircleHash(rhs);
}

SOSAccountRef SOSTransportCircleGetAccount(SOSTransportCircleRef transport){
    return transport->account;
}

bool SOSTransportCircleFlushChanges(SOSTransportCircleRef transport, CFErrorRef *error) {
    return transport->flushChanges(transport, error);
}

bool SOSTransportCirclePostCircle(SOSTransportCircleRef transport,  CFStringRef circleName, CFDataRef circle_data, CFErrorRef *error) {
    return transport->postCircle(transport, circleName, circle_data, error);
}
CF_RETURNS_RETAINED
CFDictionaryRef SOSTransportCircleHandleRetirementMessages(SOSTransportCircleRef transport, CFMutableDictionaryRef circle_retirement_messages_table, CFErrorRef *error) {
    return transport->handleRetirementMessages(transport, circle_retirement_messages_table, error);
}

CF_RETURNS_RETAINED
CFArrayRef SOSTransportCircleHandleCircleMessages(SOSTransportCircleRef transport, CFMutableDictionaryRef circle_circle_messages_table, CFErrorRef *error) {
    return transport->handleCircleMessages(transport, circle_circle_messages_table, error);
}

bool SOSTransportCirclePostRetirement(SOSTransportCircleRef transport,  CFStringRef circleName, CFStringRef peer_id, CFDataRef retirement_data, CFErrorRef *error) {
    return transport->postRetirement(transport, circleName, peer_id, retirement_data, error);
}

bool SOSTransportCircleExpireRetirementRecords(SOSTransportCircleRef transport, CFDictionaryRef retirements, CFErrorRef *error){
    return transport->expireRetirementRecords(transport, retirements, error);
}


