//
//  SOSPeerInfoSecurityProperties.h
//  sec
//
//  Created by Richard Murphy on 3/14/15.
//
//

#ifndef _sec_SOSPeerInfoSecurityProperties_
#define _sec_SOSPeerInfoSecurityProperties_


#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include <Security/SecureObjectSync/SOSPeerInfo.h>
#include <Security/SecureObjectSync/SOSAccount.h>

typedef struct __OpaqueSOSSecurityProperty {
    CFRuntimeBase _base;
    CFStringRef label;
} *SOSSecurityPropertyRef;

bool SOSSecurityPropertiesSetDefault(SOSPeerInfoRef pi, CFErrorRef *error);
CFMutableSetRef SOSSecurityPropertiesCreateDefault(SOSPeerInfoRef pi, CFErrorRef *error);

// Basic interfaces to change and query Security Properties
SOSSecurityPropertyResultCode SOSSecurityPropertyEnable(SOSPeerInfoRef pi, CFStringRef propertyname, CFErrorRef *error);
SOSSecurityPropertyResultCode SOSSecurityPropertyDisable(SOSPeerInfoRef pi, CFStringRef propertyname, CFErrorRef *error);
SOSSecurityPropertyResultCode SOSSecurityPropertyQuery(SOSPeerInfoRef pi, CFStringRef propertyname, CFErrorRef *error);

CFSetRef SOSSecurityPropertyGetAllCurrent(void);
CFMutableSetRef SOSPeerInfoCopySecurityProperty(SOSPeerInfoRef pi);

#endif /* defined(_sec_SOSPeerInfoSecurityProperties_) */
