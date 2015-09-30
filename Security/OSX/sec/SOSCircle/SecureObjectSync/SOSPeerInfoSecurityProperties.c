//
//  SOSPeerInfoSecurityProperties.c
//  sec
//
//  Created by Richard Murphy on 3/14/15.
//
//


#include <AssertMacros.h>
#include <TargetConditionals.h>

#include "SOSPeerInfoSecurityProperties.h"
#include <utilities/SecCFWrappers.h>
#include <utilities/SecCFRelease.h>
#include <utilities/SecCFError.h>
#include <Security/SecureObjectSync/SOSInternal.h>

#include <Security/SecureObjectSync/SOSPeerInfo.h>
#include <Security/SecureObjectSync/SOSPeerInfoV2.h>
#include <Security/SecureObjectSync/SOSPeerInfoPriv.h>
#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include <Security/SecureObjectSync/SOSAccount.h>
#include <Security/SecureObjectSync/SOSAccountPriv.h>

CFStringRef secpropMemError                = CFSTR("Failed to get memory for SecurityProperties in PeerInfo");
CFStringRef secpropUnknownError            = CFSTR("Unknown Security Property(%@) (SOSSecurityPropertyResultCode=%d)");
CFStringRef secpropInvalidError            = CFSTR("Peer is invalid for this security property(%@) (SOSSecurityPropertyResultCode=%d)");

const CFStringRef kSOSSecPropertyHasEntropy     = CFSTR("SecPropEntropy");
const CFStringRef kSOSSecPropertyScreenLock     = CFSTR("SecPropScreenLock");
const CFStringRef kSOSSecPropertySEP            = CFSTR("SecPropSEP");
const CFStringRef kSOSSecPropertyIOS            = CFSTR("SecPropIOS");


CFSetRef SOSSecurityPropertyGetAllCurrent(void) {
    static dispatch_once_t dot;
    static CFMutableSetRef allSecurityProperties = NULL;
    dispatch_once(&dot, ^{
        allSecurityProperties = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);
        CFSetAddValue(allSecurityProperties, kSOSSecPropertyHasEntropy);
        CFSetAddValue(allSecurityProperties, kSOSSecPropertyScreenLock);
        CFSetAddValue(allSecurityProperties, kSOSSecPropertySEP);
        CFSetAddValue(allSecurityProperties, kSOSSecPropertyIOS);
    });
    return allSecurityProperties;
}

static bool SOSSecurityPropertyIsKnownProperty(CFStringRef secPropName) {
    CFSetRef allSecurityProperties = SOSSecurityPropertyGetAllCurrent();
    if(CFSetContainsValue(allSecurityProperties, secPropName)) return true;
    secnotice("SecurityProperties","Not a known Security Property");
    return false;
}


static CFMutableSetRef CFSetCreateMutableForSOSSecurityProperties(CFAllocatorRef allocator) {
    return CFSetCreateMutable(allocator, 0, &kCFTypeSetCallBacks);
}

CFMutableSetRef SOSPeerInfoCopySecurityProperty(SOSPeerInfoRef pi) {
    if (!SOSPeerInfoVersionHasV2Data(pi)) {
        return NULL;
    } else {
        CFMutableSetRef secproperty = (CFMutableSetRef)SOSPeerInfoV2DictionaryCopySet(pi, sSecurityPropertiesKey);
        if (!secproperty)
            secerror("%@ v2 peer has no security properties", SOSPeerInfoGetPeerID(pi));
        return secproperty;
    }
}

static void SOSPeerInfoSetSecurityProperty(SOSPeerInfoRef pi, CFSetRef newproperties) {
    if(!newproperties) {
        secnotice("secproperty","Asked to swap to NULL Security Properties");
        return;
    }
    SOSPeerInfoV2DictionarySetValue(pi, sSecurityPropertiesKey, newproperties);
}

static bool SOSPeerInfoSecurityPropertyIsValid(SOSPeerInfoRef pi, CFStringRef propertyname) {
    return true;
}

static bool secPropertyErrorReport(CFIndex errorCode, CFErrorRef *error, CFStringRef format, CFStringRef propertyname, int retval) {
    return SOSCreateErrorWithFormat(errorCode, NULL, error, NULL, format, propertyname, retval);
}

CFMutableSetRef SOSSecurityPropertiesCreateDefault(SOSPeerInfoRef pi, CFErrorRef *error) {
    return CFSetCreateMutableForSOSSecurityProperties(NULL);
}

SOSSecurityPropertyResultCode SOSSecurityPropertyEnable(SOSPeerInfoRef pi, CFStringRef propertyname, CFErrorRef *error) {
    SOSSecurityPropertyResultCode retval = kSOSCCGeneralSecurityPropertyError;
    
    CFMutableSetRef newSecurityProperties = SOSPeerInfoCopySecurityProperty(pi);
    require_action_quiet(newSecurityProperties, fail,
                         SOSCreateError(kSOSErrorAllocationFailure, secpropMemError, NULL, error));
    require_action_quiet(SOSSecurityPropertyIsKnownProperty(propertyname), fail,
                         secPropertyErrorReport(kSOSErrorNameMismatch, error, secpropUnknownError, propertyname, retval = kSOSCCNoSuchSecurityProperty));
    require_action_quiet(SOSPeerInfoSecurityPropertyIsValid(pi, propertyname), fail,
                         secPropertyErrorReport(kSOSErrorNameMismatch, error, secpropInvalidError, propertyname, retval = kSOSCCSecurityPropertyNotQualified));
    CFSetAddValue(newSecurityProperties, propertyname);
    SOSPeerInfoSetSecurityProperty(pi, newSecurityProperties);
    CFReleaseSafe(newSecurityProperties);
    return kSOSCCSecurityPropertyValid;
    
fail:
    CFReleaseNull(newSecurityProperties);
    secnotice("SecurityProperties","Failed to enable Security Property(%@): %@", propertyname, *error);
    return retval;
}

SOSSecurityPropertyResultCode SOSSecurityPropertyDisable(SOSPeerInfoRef pi, CFStringRef propertyname, CFErrorRef *error) {
    SOSSecurityPropertyResultCode retval = kSOSCCGeneralSecurityPropertyError;
    CFMutableSetRef newSecurityProperties = SOSPeerInfoCopySecurityProperty(pi);
    require_action_quiet(newSecurityProperties, fail,
                         SOSCreateError(kSOSErrorAllocationFailure, secpropMemError, NULL, error));
    require_action_quiet(SOSSecurityPropertyIsKnownProperty(propertyname), fail,
                         secPropertyErrorReport(kSOSErrorNameMismatch, error, secpropUnknownError, propertyname, retval = kSOSCCNoSuchSecurityProperty));
    
    CFSetRemoveValue(newSecurityProperties, propertyname);
    SOSPeerInfoSetSecurityProperty(pi, newSecurityProperties);
    CFReleaseSafe(newSecurityProperties);
    return kSOSCCSecurityPropertyNotValid;
    
fail:
    CFReleaseNull(newSecurityProperties);
    secnotice("SecurityProperties","Failed to disable Security Property(%@): %@", propertyname, *error);
    return retval;
}

SOSSecurityPropertyResultCode SOSSecurityPropertyQuery(SOSPeerInfoRef pi, CFStringRef propertyname, CFErrorRef *error) {
    SOSSecurityPropertyResultCode retval = kSOSCCNoSuchSecurityProperty;
    secnotice("SecurityProperties", "Querying %@", propertyname);
    require_action_quiet(SOSSecurityPropertyIsKnownProperty(propertyname), fail,
                         SOSCreateError(kSOSErrorNameMismatch, secpropUnknownError, NULL, error));
    CFMutableSetRef secproperty = SOSPeerInfoCopySecurityProperty(pi);
    if(!secproperty) return kSOSCCSecurityPropertyNotValid;
    retval = (CFSetContainsValue(secproperty, propertyname)) ? kSOSCCSecurityPropertyValid: kSOSCCSecurityPropertyNotValid;
    CFReleaseNull(secproperty);

fail:
    secnotice("SecurityProperties","Failed to query Security Property(%@): %@", propertyname, *error);
    return retval;
}

