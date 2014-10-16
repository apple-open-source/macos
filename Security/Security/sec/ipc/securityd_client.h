/*
 * Copyright (c) 2007-2009,2012-2014 Apple Inc. All Rights Reserved.
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
#ifndef	_SECURITYD_CLIENT_H_
#define _SECURITYD_CLIENT_H_

#include <stdint.h>

# include <Security/SecTrust.h>
#ifndef MINIMIZE_INCLUDES
# include <Security/SecTrustStore.h>
# include <Security/SecCertificatePath.h>
#else
typedef struct __SecTrustStore *SecTrustStoreRef;
# ifndef _SECURITY_SECCERTIFICATE_H_
typedef struct __SecCertificate *SecCertificateRef;
# endif // _SECURITY_SECCERTIFICATE_H_
# ifndef _SECURITY_SECCERTIFICATEPATH_H_
typedef struct SecCertificatePath *SecCertificatePathRef;
# endif // _SECURITY_SECCERTIFICATEPATH_H_
#endif // MINIMIZE_INCLUDES

#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFError.h>

#include <SecureObjectSync/SOSCloudCircle.h>

#include <xpc/xpc.h>
#include <CoreFoundation/CFXPCBridge.h>

// TODO: This should be in client of XPC code locations...
#if SECITEM_SHIM_OSX
#define kSecuritydXPCServiceName "com.apple.securityd.xpc"
#else
#define kSecuritydXPCServiceName "com.apple.securityd"
#endif // *** END SECITEM_SHIM_OSX ***

//
// MARK: XPC Information.
//

extern CFStringRef sSecXPCErrorDomain;

extern const char *kSecXPCKeyOperation;
extern const char *kSecXPCKeyResult;
extern const char *kSecXPCKeyError;
extern const char *kSecXPCKeyPeerInfos;
extern const char *kSecXPCKeyUserLabel;
extern const char *kSecXPCKeyBackup;
extern const char *kSecXPCKeyKeybag;
extern const char *kSecXPCKeyUserPassword;

//
// MARK: Dispatch macros
//

#define SECURITYD_XPC(sdp, wrapper, ...) ((gSecurityd && gSecurityd->sdp) ? gSecurityd->sdp(__VA_ARGS__) : wrapper(sdp ## _id, __VA_ARGS__))

//
// MARK: Object to XPC format conversion.
//


//
// MARK: XPC Interfaces
//

extern const char *kSecXPCKeyOperation;
extern const char *kSecXPCKeyResult;
extern const char *kSecXPCKeyError;
extern const char *kSecXPCKeyPeerInfos;
extern const char *kSecXPCKeyUserLabel;
extern const char *kSecXPCKeyUserPassword;
extern const char *kSecXPCLimitInMinutes;
extern const char *kSecXPCKeyQuery;
extern const char *kSecXPCKeyAttributesToUpdate;
extern const char *kSecXPCKeyDomain;
extern const char *kSecXPCKeyDigest;
extern const char *kSecXPCKeyCertificate;
extern const char *kSecXPCKeySettings;
extern const char *kSecXPCPublicPeerId; // Public peer id
extern const char *kSecXPCOTRSession; // OTR session bytes
extern const char *kSecXPCData; // Data to process
extern const char *kSecXPCOTRReady; // OTR ready for messages
extern const char *kSecXPCKeyDeviceID;

//
// MARK: Mach port request IDs
//
enum SecXPCOperation {
    sec_item_add_id = 0,
    sec_item_copy_matching_id = 1,
    sec_item_update_id = 2,
    sec_item_delete_id = 3,
    // trust_store_for_domain -- NOT an ipc
    sec_trust_store_contains_id = 4,
    sec_trust_store_set_trust_settings_id = 5,
    sec_trust_store_remove_certificate_id = 6,
    // remove_all -- NOT an ipc
    sec_delete_all_id = 7,
    sec_trust_evaluate_id = 8,
    // Any new items MUST be added below here
    // This allows updating roots on a device, since SecTrustEvaluate must continue to work
    sec_keychain_backup_id,
    sec_keychain_restore_id,
    sec_keychain_backup_syncable_id,
    sec_keychain_restore_syncable_id,
    sec_keychain_sync_update_key_parameter_id,
    sec_keychain_sync_update_circle_id,
    sec_keychain_sync_update_message_id,
    sec_ota_pki_asset_version_id,
    sec_otr_session_create_remote_id,
    sec_otr_session_process_packet_remote_id,
	kSecXPCOpOTAPKIGetNewAsset,
	kSecXPCOpOTAGetEscrowCertificates,
    kSecXPCOpProcessUnlockNotification,
    kSecXPCOpProcessSyncWithAllPeers,
    kSecXPCOpRollKeys,
    sec_add_shared_web_credential_id,
    sec_copy_shared_web_credential_id,
    sec_get_log_settings_id,
    sec_set_xpc_log_settings_id,
    soscc_EnsurePeerRegistration_id,
    kSecXPCOpRequestEnsureFreshParameters,
    // any process using an operation below here is required to have entitlement keychain-cloud-circle
    kSecXPCOpTryUserCredentials,
    kSecXPCOpSetUserCredentials,
    kSecXPCOpCanAuthenticate,
    kSecXPCOpPurgeUserCredentials,
    kSecXPCOpDeviceInCircle,
    kSecXPCOpRequestToJoin,
    kSecXPCOpRequestToJoinAfterRestore,
    kSecXPCOpResetToOffering,
    kSecXPCOpResetToEmpty,
    kSecXPCOpRemoveThisDeviceFromCircle,
    kSecXPCOpBailFromCircle,
    kSecXPCOpAcceptApplicants,
    kSecXPCOpRejectApplicants,
    kSecXPCOpCopyApplicantPeerInfo,
    kSecXPCOpCopyValidPeerPeerInfo,
    kSecXPCOpValidateUserPublic,
    kSecXPCOpCopyNotValidPeerPeerInfo,
    kSecXPCOpCopyPeerPeerInfo,
    kSecXPCOpCopyConcurringPeerPeerInfo,
    kSecXPCOpCopyGenerationPeerInfo,
    kSecXPCOpGetLastDepartureReason,
    kSecXPCOpCopyIncompatibilityInfo,
    kSecXPCOpCopyRetirementPeerInfo,
    kSecXPCOpRequestDeviceID,
    kSecXPCOpSetDeviceID,
};



struct securityd {
    bool (*sec_item_add)(CFDictionaryRef attributes, CFArrayRef accessGroups, CFTypeRef *result, CFErrorRef* error);
    bool (*sec_item_copy_matching)(CFDictionaryRef query, CFArrayRef accessGroups, CFTypeRef *result, CFErrorRef* error);
    bool (*sec_item_update)(CFDictionaryRef query, CFDictionaryRef attributesToUpdate, CFArrayRef accessGroups, CFErrorRef* error);
    bool (*sec_item_delete)(CFDictionaryRef query, CFArrayRef accessGroups, CFErrorRef* error);
    bool (*sec_add_shared_web_credential)(CFDictionaryRef attributes, const audit_token_t *clientAuditToken, CFStringRef appID, CFArrayRef accessGroups, CFTypeRef *result, CFErrorRef *error);
    bool (*sec_copy_shared_web_credential)(CFDictionaryRef query, const audit_token_t *clientAuditToken, CFStringRef appID, CFArrayRef accessGroups, CFTypeRef *result, CFErrorRef *error);
    SecTrustStoreRef (*sec_trust_store_for_domain)(CFStringRef domainName, CFErrorRef* error);       // TODO: remove, has no msg id
    bool (*sec_trust_store_contains)(SecTrustStoreRef ts, CFDataRef digest, bool *contains, CFErrorRef* error);
    bool (*sec_trust_store_set_trust_settings)(SecTrustStoreRef ts, SecCertificateRef certificate, CFTypeRef trustSettingsDictOrArray, CFErrorRef* error);
    bool (*sec_trust_store_remove_certificate)(SecTrustStoreRef ts, CFDataRef digest, CFErrorRef* error);
    bool (*sec_truststore_remove_all)(SecTrustStoreRef ts, CFErrorRef* error);                         // TODO: remove, has no msg id
    bool (*sec_item_delete_all)(CFErrorRef* error);
    SecTrustResultType (*sec_trust_evaluate)(CFArrayRef certificates, CFArrayRef anchors, bool anchorsOnly, CFArrayRef policies, CFAbsoluteTime verifyTime, __unused CFArrayRef accessGroups, CFArrayRef *details, CFDictionaryRef *info, SecCertificatePathRef *chain, CFErrorRef *error);
    CFDataRef (*sec_keychain_backup)(CFDataRef keybag, CFDataRef passcode, CFErrorRef* error);
    bool (*sec_keychain_restore)(CFDataRef backup, CFDataRef keybag, CFDataRef passcode, CFErrorRef* error);
    CFArrayRef (*sec_keychain_sync_update_key_parameter)(CFDictionaryRef update, CFErrorRef *error);
    CFDictionaryRef (*sec_keychain_backup_syncable)(CFDictionaryRef backup_in, CFDataRef keybag, CFDataRef passcode, CFErrorRef* error);
    bool (*sec_keychain_restore_syncable)(CFDictionaryRef backup, CFDataRef keybag, CFDataRef passcode, CFErrorRef* error);
    int (*sec_ota_pki_asset_version)(CFErrorRef* error);
    CFDataRef (*sec_otr_session_create_remote)(CFDataRef publicPeerId, CFErrorRef* error);
    bool (*sec_otr_session_process_packet_remote)(CFDataRef sessionData, CFDataRef inputPacket, CFDataRef* outputSessionData, CFDataRef* outputPacket, bool *readyForMessages, CFErrorRef* error);
    bool (*soscc_TryUserCredentials)(CFStringRef user_label, CFDataRef user_password, CFErrorRef *error);
    bool (*soscc_SetUserCredentials)(CFStringRef user_label, CFDataRef user_password, CFErrorRef *error);
    bool (*soscc_CanAuthenticate)(CFErrorRef *error);
    bool (*soscc_PurgeUserCredentials)(CFErrorRef *error);
    SOSCCStatus (*soscc_ThisDeviceIsInCircle)(CFErrorRef* error);
    bool (*soscc_RequestToJoinCircle)(CFErrorRef* error);
    bool (*soscc_RequestToJoinCircleAfterRestore)(CFErrorRef* error);
    bool (*soscc_RequestEnsureFreshParameters)(CFErrorRef* error);
    CFStringRef (*soscc_RequestDeviceID)(CFErrorRef* error);
    bool (*soscc_SetDeviceID)(CFStringRef IDS, CFErrorRef *error);
    bool (*soscc_ResetToOffering)(CFErrorRef* error);
    bool (*soscc_ResetToEmpty)(CFErrorRef* error);
    bool (*soscc_RemoveThisDeviceFromCircle)(CFErrorRef* error);
    bool (*soscc_BailFromCircle)(uint64_t limit_in_seconds, CFErrorRef* error);
    bool (*soscc_AcceptApplicants)(CFArrayRef applicants, CFErrorRef* error);
    bool (*soscc_RejectApplicants)(CFArrayRef applicants, CFErrorRef* error);
    bool (*soscc_ValidateUserPublic)(CFErrorRef* error);
    CFArrayRef (*soscc_CopyGenerationPeerInfo)(CFErrorRef* error);
    CFArrayRef (*soscc_CopyApplicantPeerInfo)(CFErrorRef* error);
    CFArrayRef (*soscc_CopyValidPeerPeerInfo)(CFErrorRef* error);
    CFArrayRef (*soscc_CopyNotValidPeerPeerInfo)(CFErrorRef* error);
    CFArrayRef (*soscc_CopyRetirementPeerInfo)(CFErrorRef* error);
    CFArrayRef (*soscc_CopyPeerInfo)(CFErrorRef* error);
    CFArrayRef (*soscc_CopyConcurringPeerInfo)(CFErrorRef* error);
    CFStringRef (*soscc_CopyIncompatibilityInfo)(CFErrorRef* error);
    enum DepartureReason (*soscc_GetLastDepartureReason)(CFErrorRef* error);
	CFArrayRef (*ota_CopyEscrowCertificates)(uint32_t escrowRootType, CFErrorRef* error);
	int (*sec_ota_pki_get_new_asset)(CFErrorRef* error);
    SyncWithAllPeersReason (*soscc_ProcessSyncWithAllPeers)(CFErrorRef* error);
    bool (*soscc_EnsurePeerRegistration)(CFErrorRef* error);
    bool (*sec_roll_keys)(bool force, CFErrorRef* error);
    CFArrayRef (*sec_keychain_sync_update_circle)(CFDictionaryRef update, CFErrorRef *error);
    CFArrayRef (*sec_keychain_sync_update_message)(CFDictionaryRef update, CFErrorRef *error);
    CFPropertyListRef   (*sec_get_log_settings)(CFErrorRef* error);
    bool   (*sec_set_xpc_log_settings)(CFTypeRef type, CFErrorRef* error);
};

extern struct securityd *gSecurityd;

CFArrayRef SecAccessGroupsGetCurrent(void);

// TODO Rename me
CFStringRef SOSCCGetOperationDescription(enum SecXPCOperation op);
xpc_object_t securityd_message_with_reply_sync(xpc_object_t message, CFErrorRef *error);
xpc_object_t securityd_create_message(enum SecXPCOperation op, CFErrorRef *error);
bool securityd_message_no_error(xpc_object_t message, CFErrorRef *error);


bool securityd_send_sync_and_do(enum SecXPCOperation op, CFErrorRef *error,
                                bool (^add_to_message)(xpc_object_t message, CFErrorRef* error),
                                bool (^handle_response)(xpc_object_t response, CFErrorRef* error));

// For testing only, never call this in a threaded program!
void SecServerSetMachServiceName(const char *name);

#endif /* _SECURITYD_CLIENT_H_ */
