#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecureObjectSync/SOSTransport.h>
#include <Security/SecureObjectSync/SOSTransportBackupPeer.h>
#include <Security/SecureObjectSync/SOSAccount.h>
#include <Security/SecureObjectSync/SOSAccountPriv.h>

#include <utilities/SecCFWrappers.h>
#include <AssertMacros.h>


CFGiblisWithHashFor(SOSTransportBackupPeer);

SOSTransportBackupPeerRef SOSTransportBackupPeerCreate(CFStringRef fileLocation, CFErrorRef *error)
{
    SOSTransportBackupPeerRef tpt = (SOSTransportBackupPeerRef)CFTypeAllocateWithSpace(SOSTransportBackupPeer, sizeof(struct __OpaqueSOSTransportBackupPeer) - sizeof(CFRuntimeBase), kCFAllocatorDefault);
    tpt->fileLocation = CFRetainSafe(fileLocation);
    return tpt;
}

static CFStringRef SOSTransportBackupPeerCopyFormatDescription(CFTypeRef aObj, CFDictionaryRef formatOptions){
    SOSTransportBackupPeerRef t = (SOSTransportBackupPeerRef) aObj;
    
    return CFStringCreateWithFormat(NULL, NULL, CFSTR("<SOSTransportBackupPeer@%p\n>"), t);
}

static void SOSTransportBackupPeerDestroy(CFTypeRef aObj){
    SOSTransportBackupPeerRef transport = (SOSTransportBackupPeerRef) aObj;
    CFReleaseNull(transport);
   }

CFIndex SOSTransportBackupPeerGetTransportType(SOSTransportBackupPeerRef transport, CFErrorRef *error){
    return kBackupPeer;
}

static CFHashCode SOSTransportBackupPeerHash(CFTypeRef obj){
    return (intptr_t) obj;
}

static Boolean SOSTransportBackupPeerCompare(CFTypeRef lhs, CFTypeRef rhs){
    return SOSTransportBackupPeerHash(lhs) == SOSTransportBackupPeerHash(rhs);
}
