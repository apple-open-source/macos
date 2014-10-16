/*
 * Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

//
//  SOSCloudCircle.m
//

#include <stdio.h>
#include <AssertMacros.h>
#include <SecureObjectSync/SOSCloudCircle.h>
#include <SecureObjectSync/SOSCloudCircleInternal.h>
#include <SecureObjectSync/SOSCircle.h>
#include <SecureObjectSync/SOSAccount.h>
#include <SecureObjectSync/SOSFullPeerInfo.h>
#include <SecureObjectSync/SOSPeerInfoCollections.h>
#include <SecureObjectSync/SOSInternal.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecFramework.h>
#include <CoreFoundation/CFXPCBridge.h>

#include <securityd/SecItemServer.h>

#include <utilities/SecDispatchRelease.h>
#include <utilities/SecCFRelease.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecXPCError.h>

#include <utilities/debugging.h>

#include <CoreFoundation/CoreFoundation.h>

#include <xpc/xpc.h>
#define MINIMIZE_INCLUDES MINIMIZE_INCLUDES
#include <ipc/securityd_client.h>
#include <securityd/spi.h>

#include "SOSRegressionUtilities.h"


const char * kSOSCCCircleChangedNotification = "com.apple.security.secureobjectsync.circlechanged";

#define do_if_registered(sdp, ...) if (gSecurityd && gSecurityd->sdp) { return gSecurityd->sdp(__VA_ARGS__); }

static bool xpc_dictionary_entry_is_type(xpc_object_t dictionary, const char *key, xpc_type_t type)
{
    xpc_object_t value = xpc_dictionary_get_value(dictionary, key);
    
    return value && (xpc_get_type(value) == type);
}

SOSCCStatus SOSCCThisDeviceIsInCircle(CFErrorRef *error)
{
    static int counter = 0;
    if(counter++ > 5) secerror("SOSCCThisDeviceIsInCircle!! %d\n", counter);
    sec_trace_enter_api(NULL);
    sec_trace_return_api(SOSCCStatus, ^{
        SOSCCStatus result = kSOSCCError;
        
        do_if_registered(soscc_ThisDeviceIsInCircle, error);
        
        xpc_object_t message = securityd_create_message(kSecXPCOpDeviceInCircle, error);
        if (message) {
            xpc_object_t response = securityd_message_with_reply_sync(message, error);
            
            if (response && xpc_dictionary_entry_is_type(response, kSecXPCKeyResult, XPC_TYPE_INT64)) {
                result = (SOSCCStatus) xpc_dictionary_get_int64(response, kSecXPCKeyResult);
            } else {
                result = kSOSCCError;
            }
            
            if (result < 0) {
                if (response && securityd_message_no_error(response, error))
                {
                    char *desc = xpc_copy_description(response);
                    SecCFCreateErrorWithFormat(0, sSecXPCErrorDomain, NULL, error, NULL, CFSTR("Remote error occurred/no info: %s"), desc);
                    free((void *)desc);
                }
            }
            if(response)
                xpc_release(response);
            if(message)
                xpc_release(message);
        }
        
        
        return result;
    }, CFSTR("SOSCCStatus=%d"))
}

static CFStringRef simple_cfstring_error_request(enum SecXPCOperation op, CFErrorRef* error)
{
    __block CFStringRef result = NULL;
    
    secdebug("sosops","enter - operation: %d", op);
    securityd_send_sync_and_do(op, error, NULL, ^bool(xpc_object_t response, __unused CFErrorRef *error) {
        const char *c_string = xpc_dictionary_get_string(response, kSecXPCKeyResult);

        if (c_string) {
            result = CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8*)c_string, strlen(c_string), kCFStringEncodingUTF8, false);
        }
        
        return c_string != NULL;
    });
    return result;
}

static bool simple_bool_error_request(enum SecXPCOperation op, CFErrorRef* error)
{
    __block bool result = false;
    
    secdebug("sosops","enter - operation: %d", op);
    securityd_send_sync_and_do(op, error, NULL, ^bool(xpc_object_t response, __unused CFErrorRef *error) {
        result = xpc_dictionary_get_bool(response, kSecXPCKeyResult);
        return result;
    });
    return result;
}

static CFArrayRef simple_array_error_request(enum SecXPCOperation op, CFErrorRef* error)
{
    __block CFArrayRef result = false;
    
    secdebug("sosops","enter - operation: %d", op);
    securityd_send_sync_and_do(op, error, NULL, ^bool(xpc_object_t response, CFErrorRef *error) {
        xpc_object_t temp_result = xpc_dictionary_get_value(response, kSecXPCKeyResult);
        result = _CFXPCCreateCFObjectFromXPCObject(temp_result);
        return result != NULL;
    });
    return result;
}

static int simple_int_error_request(enum SecXPCOperation op, CFErrorRef* error)
{
    __block int result = 0;
    
    secdebug("sosops","enter - operation: %d", op);
    securityd_send_sync_and_do(op, error, NULL, ^bool(xpc_object_t response, __unused CFErrorRef *error) {
        int64_t temp_result =  xpc_dictionary_get_int64(response, kSecXPCKeyResult);
        if ((temp_result >= INT32_MIN) && (temp_result <= INT32_MAX)) {
            result = (int)temp_result;
        }
        return result;
    });
    return result;
}

static CFArrayRef array_of_info_error_request(enum SecXPCOperation op, CFErrorRef* error)
{
    __block CFArrayRef result = NULL;
    
    secdebug("sosops","enter - operation: %d", op);
    securityd_send_sync_and_do(op, error, NULL, ^bool(xpc_object_t response, CFErrorRef *error) {
        xpc_object_t encoded_array = xpc_dictionary_get_value(response, kSecXPCKeyResult);
        if (response && (NULL != encoded_array)) {
            result = CreateArrayOfPeerInfoWithXPCObject(encoded_array,  error);
        }
        return result != NULL;
    });

    return result;
}

static bool info_array_to_bool_error_request(enum SecXPCOperation op, CFArrayRef peer_infos, CFErrorRef* error)
{
    __block bool result = false;
    
    secdebug("sosops", "enter - operation: %d", op);
    securityd_send_sync_and_do(op, error, ^bool(xpc_object_t message, CFErrorRef *error) {
        xpc_object_t encoded_peers = CreateXPCObjectWithArrayOfPeerInfo(peer_infos, error);
        if (encoded_peers)
            xpc_dictionary_set_value(message, kSecXPCKeyPeerInfos, encoded_peers);
        return encoded_peers != NULL;
    }, ^bool(xpc_object_t response, __unused CFErrorRef *error) {
        result = xpc_dictionary_get_bool(response, kSecXPCKeyResult);
        return result;
    });
    return result;
}

static bool uint64_t_to_bool_error_request(enum SecXPCOperation op,
                                           uint64_t number,
                                           CFErrorRef* error)
{
    __block bool result = false;
    
    securityd_send_sync_and_do(op, error, ^bool(xpc_object_t message, CFErrorRef *error) {
        xpc_dictionary_set_uint64(message, kSecXPCLimitInMinutes, number);
        return true;
    }, ^bool(xpc_object_t response, __unused CFErrorRef *error) {
        result = xpc_dictionary_get_bool(response, kSecXPCKeyResult);
        return result;
    });
    
    return result;
}

bool SOSCCRequestToJoinCircle(CFErrorRef* error)
{
    sec_trace_enter_api(NULL);
    sec_trace_return_bool_api(^{
        do_if_registered(soscc_RequestToJoinCircle, error);
        
        return simple_bool_error_request(kSecXPCOpRequestToJoin, error);
    }, NULL)
}

bool SOSCCRequestToJoinCircleAfterRestore(CFErrorRef* error)
{
    sec_trace_enter_api(NULL);
    sec_trace_return_bool_api(^{
        do_if_registered(soscc_RequestToJoinCircleAfterRestore, error);
        
        return simple_bool_error_request(kSecXPCOpRequestToJoinAfterRestore, error);
    }, NULL)
}

bool SOSCCRequestEnsureFreshParameters(CFErrorRef* error)
{
        sec_trace_enter_api(NULL);
        sec_trace_return_bool_api(^{
                do_if_registered(soscc_RequestEnsureFreshParameters, error);
        
                return simple_bool_error_request(kSecXPCOpRequestEnsureFreshParameters, error);
            }, NULL)
}

bool SOSCCResetToOffering(CFErrorRef* error)
{
    secwarning("SOSCCResetToOffering called");
    sec_trace_enter_api(NULL);
    sec_trace_return_bool_api(^{
        do_if_registered(soscc_ResetToOffering, error);
        
        return simple_bool_error_request(kSecXPCOpResetToOffering, error);
    }, NULL)
}

bool SOSCCResetToEmpty(CFErrorRef* error)
{
    secwarning("SOSCCResetToEmpty called");
    sec_trace_enter_api(NULL);
    sec_trace_return_bool_api(^{
        do_if_registered(soscc_ResetToEmpty, error);
        
        return simple_bool_error_request(kSecXPCOpResetToEmpty, error);
    }, NULL)
}

bool SOSCCRemoveThisDeviceFromCircle(CFErrorRef* error)
{
    sec_trace_enter_api(NULL);
    sec_trace_return_bool_api(^{
        do_if_registered(soscc_RemoveThisDeviceFromCircle, error);
        
        return simple_bool_error_request(kSecXPCOpRemoveThisDeviceFromCircle, error);
    }, NULL)
}

bool SOSCCBailFromCircle_BestEffort(uint64_t limit_in_seconds, CFErrorRef* error)
{
    sec_trace_enter_api(NULL);
    sec_trace_return_bool_api(^{
        do_if_registered(soscc_BailFromCircle, limit_in_seconds, error);
        
        return uint64_t_to_bool_error_request(kSecXPCOpBailFromCircle, limit_in_seconds, error);
    }, NULL)
}

bool SOSCCSignedOut(bool immediate, CFErrorRef* error)
{
    uint64_t limit = strtoul(optarg, NULL, 10);
    
    if(immediate)
        return SOSCCRemoveThisDeviceFromCircle(error);
    else
        return SOSCCBailFromCircle_BestEffort(limit, error);
    
}

CFArrayRef SOSCCCopyPeerPeerInfo(CFErrorRef* error)
{
    sec_trace_enter_api(NULL);
    sec_trace_return_api(CFArrayRef, ^{
        do_if_registered(soscc_CopyPeerInfo, error);
        
        return array_of_info_error_request(kSecXPCOpCopyPeerPeerInfo, error);
    }, CFSTR("return=%@"));
}

CFArrayRef SOSCCCopyConcurringPeerPeerInfo(CFErrorRef* error)
{
    sec_trace_enter_api(NULL);
    sec_trace_return_api(CFArrayRef, ^{
        do_if_registered(soscc_CopyConcurringPeerInfo, error);
        
        return array_of_info_error_request(kSecXPCOpCopyConcurringPeerPeerInfo, error);
    }, CFSTR("return=%@"));
}

CFArrayRef SOSCCCopyGenerationPeerInfo(CFErrorRef* error)
{
    sec_trace_enter_api(NULL);
    sec_trace_return_api(CFArrayRef, ^{
        do_if_registered(soscc_CopyGenerationPeerInfo, error);
        
        return simple_array_error_request(kSecXPCOpCopyGenerationPeerInfo, error);
    }, CFSTR("return=%@"));
}

CFArrayRef SOSCCCopyApplicantPeerInfo(CFErrorRef* error)
{
    sec_trace_enter_api(NULL);
    sec_trace_return_api(CFArrayRef, ^{
        do_if_registered(soscc_CopyApplicantPeerInfo, error);
        
        return array_of_info_error_request(kSecXPCOpCopyApplicantPeerInfo, error);
    }, CFSTR("return=%@"));
}
bool SOSCCValidateUserPublic(CFErrorRef* error){
    sec_trace_enter_api(NULL);
    sec_trace_return_api(bool, ^{
        do_if_registered(soscc_ValidateUserPublic, error);
        
        return simple_bool_error_request(kSecXPCOpValidateUserPublic, error);
    }, NULL);
}

CFArrayRef SOSCCCopyValidPeerPeerInfo(CFErrorRef* error)
{
    sec_trace_enter_api(NULL);
    sec_trace_return_api(CFArrayRef, ^{
        do_if_registered(soscc_CopyValidPeerPeerInfo, error);
        
        return array_of_info_error_request(kSecXPCOpCopyValidPeerPeerInfo, error);
    }, CFSTR("return=%@"));
}

CFArrayRef SOSCCCopyNotValidPeerPeerInfo(CFErrorRef* error)
{
    sec_trace_enter_api(NULL);
    sec_trace_return_api(CFArrayRef, ^{
        do_if_registered(soscc_CopyNotValidPeerPeerInfo, error);
        
        return array_of_info_error_request(kSecXPCOpCopyNotValidPeerPeerInfo, error);
    }, CFSTR("return=%@"));
}

CFArrayRef SOSCCCopyRetirementPeerInfo(CFErrorRef* error)
{
    sec_trace_enter_api(NULL);
    sec_trace_return_api(CFArrayRef, ^{
        do_if_registered(soscc_CopyRetirementPeerInfo, error);
        
        return array_of_info_error_request(kSecXPCOpCopyRetirementPeerInfo, error);
    }, CFSTR("return=%@"));
}

bool SOSCCAcceptApplicants(CFArrayRef applicants, CFErrorRef* error)
{
    sec_trace_enter_api(NULL);
    sec_trace_return_bool_api(^{
        do_if_registered(soscc_AcceptApplicants, applicants, error);

        return info_array_to_bool_error_request(kSecXPCOpAcceptApplicants, applicants, error);
    }, NULL)
}

bool SOSCCRejectApplicants(CFArrayRef applicants, CFErrorRef *error)
{
    sec_trace_enter_api(CFSTR("applicants=%@"), applicants);
    sec_trace_return_bool_api(^{
        do_if_registered(soscc_RejectApplicants, applicants, error);
        
        return info_array_to_bool_error_request(kSecXPCOpRejectApplicants, applicants, error);
    }, NULL)
}

static bool label_and_password_to_bool_error_request(enum SecXPCOperation op,
                                                     CFStringRef user_label, CFDataRef user_password,
                                                     CFErrorRef* error)
{
    __block bool result = false;

    securityd_send_sync_and_do(op, error, ^bool(xpc_object_t message, CFErrorRef *error) {
        CFStringPerformWithCString(user_label, ^(const char *utf8Str) {
            xpc_dictionary_set_string(message, kSecXPCKeyUserLabel, utf8Str);
        });
        xpc_dictionary_set_data(message, kSecXPCKeyUserPassword, CFDataGetBytePtr(user_password), CFDataGetLength(user_password));
        return true;
    }, ^bool(xpc_object_t response, __unused CFErrorRef *error) {
        result = xpc_dictionary_get_bool(response, kSecXPCKeyResult);
        return result;
    });

    return result;
}

static bool deviceid_to_bool_error_request(enum SecXPCOperation op,
                                                     CFStringRef IDS,
                                                     CFErrorRef* error)
{
    __block bool result = false;
    
    securityd_send_sync_and_do(op, error, ^bool(xpc_object_t message, CFErrorRef *error) {
        CFStringPerformWithCString(IDS, ^(const char *utf8Str) {
            xpc_dictionary_set_string(message, kSecXPCKeyDeviceID, utf8Str);
        });
        return true;
    }, ^bool(xpc_object_t response, __unused CFErrorRef *error) {
        result = xpc_dictionary_get_bool(response, kSecXPCKeyDeviceID);
        return result;
    });
    
    return result;
}

bool SOSCCRegisterUserCredentials(CFStringRef user_label, CFDataRef user_password, CFErrorRef* error)
{
    secerror("SOSCCRegisterUserCredentials - calling SOSCCSetUserCredentials!! %@\n", user_label);
    return SOSCCSetUserCredentials(user_label, user_password, error);
}

bool SOSCCSetUserCredentials(CFStringRef user_label, CFDataRef user_password, CFErrorRef* error)
{
    secerror("SOSCCSetUserCredentials!! %@\n", user_label);
	sec_trace_enter_api(CFSTR("user_label=%@"), user_label);
    sec_trace_return_bool_api(^{
		do_if_registered(soscc_SetUserCredentials, user_label, user_password, error);

    	return label_and_password_to_bool_error_request(kSecXPCOpSetUserCredentials, user_label, user_password, error);
    }, NULL)
}
bool SOSCCSetDeviceID(CFStringRef IDS, CFErrorRef* error)
{
    secerror("SOSCCSetDeviceID!! %@\n", IDS);
    sec_trace_enter_api(NULL);
    sec_trace_return_bool_api(^{
        do_if_registered(soscc_SetDeviceID, IDS, error);
        return deviceid_to_bool_error_request(kSecXPCOpSetDeviceID, IDS, error);
    }, NULL)
}
bool SOSCCTryUserCredentials(CFStringRef user_label, CFDataRef user_password, CFErrorRef* error)
{
	sec_trace_enter_api(CFSTR("user_label=%@"), user_label);
    sec_trace_return_bool_api(^{
	    do_if_registered(soscc_TryUserCredentials, user_label, user_password, error);

    	return label_and_password_to_bool_error_request(kSecXPCOpTryUserCredentials, user_label, user_password, error);
    }, NULL)
}


bool SOSCCCanAuthenticate(CFErrorRef* error) {
    sec_trace_enter_api(NULL);
    sec_trace_return_bool_api(^{
	    do_if_registered(soscc_CanAuthenticate, error);

	    return simple_bool_error_request(kSecXPCOpCanAuthenticate, error);
    }, NULL)
}

bool SOSCCPurgeUserCredentials(CFErrorRef* error) {
    sec_trace_enter_api(NULL);
    sec_trace_return_bool_api(^{
	    do_if_registered(soscc_PurgeUserCredentials, error);

	    return simple_bool_error_request(kSecXPCOpPurgeUserCredentials, error);
    }, NULL)
}

enum DepartureReason SOSCCGetLastDepartureReason(CFErrorRef *error) {
    sec_trace_enter_api(NULL);
    sec_trace_return_api(enum DepartureReason, ^{
	    do_if_registered(soscc_GetLastDepartureReason, error);
        
	    return (enum DepartureReason) simple_int_error_request(kSecXPCOpGetLastDepartureReason, error);
    }, NULL)
}

CFStringRef SOSCCCopyIncompatibilityInfo(CFErrorRef* error) {
    sec_trace_enter_api(NULL);
    sec_trace_return_api(CFStringRef, ^{
	    do_if_registered(soscc_CopyIncompatibilityInfo, error);
        
	    return simple_cfstring_error_request(kSecXPCOpCopyIncompatibilityInfo, error);
    }, NULL)
}

CFStringRef SOSCCRequestDeviceID(CFErrorRef* error)
{
    sec_trace_enter_api(NULL);
    sec_trace_return_api(CFStringRef, ^{
        do_if_registered(soscc_RequestDeviceID, error);
        CFStringRef deviceID = simple_cfstring_error_request(kSecXPCOpRequestDeviceID, error);
        return deviceID;
    }, NULL)
}

bool SOSCCProcessEnsurePeerRegistration(CFErrorRef* error){
    secnotice("updates", "enter SOSCCProcessEnsurePeerRegistration");
    sec_trace_enter_api(NULL);
    sec_trace_return_bool_api(^{
        do_if_registered(soscc_EnsurePeerRegistration, error);
        
        return simple_bool_error_request(soscc_EnsurePeerRegistration_id, error);
    }, NULL)
}


SyncWithAllPeersReason SOSCCProcessSyncWithAllPeers(CFErrorRef* error)
{
    sec_trace_enter_api(NULL);
    sec_trace_return_api(SyncWithAllPeersReason, ^{
	    do_if_registered(soscc_ProcessSyncWithAllPeers, error);
        
	    return (SyncWithAllPeersReason) simple_int_error_request(kSecXPCOpProcessSyncWithAllPeers, error);
    }, NULL)
}

CFStringRef SOSCCGetStatusDescription(SOSCCStatus status)
{
    switch (status) {
        case kSOSCCInCircle:
            return CFSTR("InCircle");
        case kSOSCCNotInCircle:
            return CFSTR("NotInCircle");
        case kSOSCCRequestPending:
            return CFSTR("RequestPending");
        case kSOSCCCircleAbsent:
            return CFSTR("CircleAbsent");
        case kSOSCCError:
            return CFSTR("InternalError");
        default:
            return CFSTR("Unknown Status (%d)");
    };
}
