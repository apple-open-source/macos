
#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecureObjectSync/SOSTransportCircle.h>
#include <Security/SecureObjectSync/SOSTransportCircleKVS.h>
#include <Security/SecureObjectSync/SOSTransport.h>
#include <Security/SecureObjectSync/SOSKVSKeys.h>
#include <Security/SecureObjectSync/SOSAccount.h>
#include <Security/SecureObjectSync/SOSAccountPriv.h>
#include <SOSPeerInfoDER.h>

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

static CFStringRef SOSTransportCircleCopyFormatDescription(CFTypeRef aObj, CFDictionaryRef formatOptions) {
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

bool SOSTransportCirclePostRetirement(SOSTransportCircleRef transport,  CFStringRef circleName, SOSPeerInfoRef peer, CFErrorRef *error) {
    bool success = false;
    CFDataRef retirement_data = NULL;

    retirement_data = SOSPeerInfoCopyEncodedData(peer, kCFAllocatorDefault, error);

    require_quiet(retirement_data, fail);

    success = transport->postRetirement(transport, circleName, SOSPeerInfoGetPeerID(peer), retirement_data, error);

fail:
    CFReleaseNull(retirement_data);
    return success;
}

bool SOSTransportCircleExpireRetirementRecords(SOSTransportCircleRef transport, CFDictionaryRef retirements, CFErrorRef *error){
    return transport->expireRetirementRecords(transport, retirements, error);
}

CFIndex SOSTransportCircleGetTransportType(SOSTransportCircleRef transport, CFErrorRef *error){
    return transport->getTransportType ? transport->getTransportType(transport, error) : kUnknown;
}

bool SOSTransportCircleSendPeerInfo(SOSTransportCircleRef transport, CFStringRef peerID, CFDataRef peerInfoData, CFErrorRef *error){
    return transport->sendPeerInfo(transport, peerID, peerInfoData, error);
}

bool SOSTransportCircleRingFlushChanges(SOSTransportCircleRef transport, CFErrorRef* error){
    return transport->flushRingChanges(transport, error);
}

bool SOSTransportCircleRingPostRing(SOSTransportCircleRef transport, CFStringRef ringName, CFDataRef ring, CFErrorRef *error){
    return transport->postRing(transport, ringName, ring, error);
}

bool SOSTransportCircleSendDebugInfo(SOSTransportCircleRef transport, CFStringRef type, CFTypeRef debugInfo, CFErrorRef *error){
    return transport->sendDebugInfo(transport, type, debugInfo, error);
}

bool SOSTransportCircleSendAccountChangedWithDSID(SOSTransportCircleRef transport, CFStringRef dsid, CFErrorRef *error){
    return transport->sendAccountChangedWithDSID(transport, dsid, error);
}


