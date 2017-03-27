//
//  SOSPeerInfoPriv.h
//  sec
//
//  Created by Richard Murphy on 12/4/14.
//
//

#ifndef sec_SOSPeerInfoPriv_h
#define sec_SOSPeerInfoPriv_h

#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CoreFoundation.h>
#include <utilities/SecCFWrappers.h>

struct __OpaqueSOSPeerInfo {
    CFRuntimeBase           _base;
    //
    CFMutableDictionaryRef  description;
    CFDataRef               signature;
    
    // Cached data
    CFDictionaryRef         gestalt;
    CFStringRef             id;
    CFIndex                 version;
    /* V2 and beyond are listed below */
    CFMutableDictionaryRef  v2Dictionary;
};

SOSPeerInfoRef SOSPeerInfoAllocate(CFAllocatorRef allocator);
bool SOSPeerInfoSign(SecKeyRef privKey, SOSPeerInfoRef peer, CFErrorRef *error);
bool SOSPeerInfoVerify(SOSPeerInfoRef peer, CFErrorRef *error);
void SOSPeerInfoSetVersionNumber(SOSPeerInfoRef pi, int version);

extern const CFStringRef peerIDLengthKey;

#endif
