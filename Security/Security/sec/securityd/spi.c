/*
 * Copyright (c) 2009-2010,2012-2014 Apple Inc. All Rights Reserved.
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


#include <securityd/spi.h>
#include <ipc/securityd_client.h>
#include <securityd/SecPolicyServer.h>
#include <securityd/SecItemServer.h>
#include <securityd/SecTrustStoreServer.h>
#include <SecureObjectSync/SOSPeerInfo.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFError.h>
#include <securityd/SOSCloudCircleServer.h>
#include <securityd/SecOTRRemote.h>
#include <securityd/SecLogSettingsServer.h>

#include <CoreFoundation/CFXPCBridge.h>
#include "utilities/iOSforOSX.h"
#include "utilities/SecFileLocations.h"
#include "OTATrustUtilities.h"

static struct securityd spi = {
    .sec_item_add                           = _SecItemAdd,
    .sec_item_copy_matching                 = _SecItemCopyMatching,
    .sec_item_update                        = _SecItemUpdate,
    .sec_item_delete                        = _SecItemDelete,
    .sec_add_shared_web_credential          = _SecAddSharedWebCredential,
    .sec_copy_shared_web_credential         = _SecCopySharedWebCredential,
    .sec_trust_store_for_domain             = SecTrustStoreForDomainName,
    .sec_trust_store_contains               = SecTrustStoreContainsCertificateWithDigest,
    .sec_trust_store_set_trust_settings     = _SecTrustStoreSetTrustSettings,
    .sec_trust_store_remove_certificate     = SecTrustStoreRemoveCertificateWithDigest,
    .sec_truststore_remove_all              = _SecTrustStoreRemoveAll,
    .sec_item_delete_all                    = _SecItemDeleteAll,
    .sec_trust_evaluate                     = SecTrustServerEvaluate,
    .sec_keychain_backup                    = _SecServerKeychainBackup,
    .sec_keychain_restore                   = _SecServerKeychainRestore,
    .sec_keychain_sync_update_key_parameter = _SecServerKeychainSyncUpdateKeyParameter,
    .sec_keychain_backup_syncable           = _SecServerBackupSyncable,
    .sec_keychain_restore_syncable          = _SecServerRestoreSyncable,
    .sec_otr_session_create_remote          = _SecOTRSessionCreateRemote,
    .sec_otr_session_process_packet_remote  = _SecOTRSessionProcessPacketRemote,
    .sec_ota_pki_asset_version              = SecOTAPKIGetCurrentAssetVersion,
    .soscc_TryUserCredentials               = SOSCCTryUserCredentials_Server,
    .soscc_SetUserCredentials               = SOSCCSetUserCredentials_Server,
    .soscc_CanAuthenticate                  = SOSCCCanAuthenticate_Server,
    .soscc_PurgeUserCredentials             = SOSCCPurgeUserCredentials_Server,
    .soscc_ThisDeviceIsInCircle             = SOSCCThisDeviceIsInCircle_Server,
    .soscc_RequestToJoinCircle              = SOSCCRequestToJoinCircle_Server,
    .soscc_RequestToJoinCircleAfterRestore  = SOSCCRequestToJoinCircleAfterRestore_Server,
    .soscc_RequestEnsureFreshParameters     = SOSCCRequestEnsureFreshParameters_Server,
    .soscc_ResetToOffering                  = SOSCCResetToOffering_Server,
    .soscc_ResetToEmpty                     = SOSCCResetToEmpty_Server,
    .soscc_RemoveThisDeviceFromCircle       = SOSCCRemoveThisDeviceFromCircle_Server,
    .soscc_BailFromCircle                   = SOSCCBailFromCircle_Server,
    .soscc_AcceptApplicants                 = SOSCCAcceptApplicants_Server,
    .soscc_RejectApplicants                 = SOSCCRejectApplicants_Server,
    .soscc_CopyApplicantPeerInfo            = SOSCCCopyApplicantPeerInfo_Server,
    .soscc_CopyGenerationPeerInfo           = SOSCCCopyGenerationPeerInfo_Server,
    .soscc_CopyValidPeerPeerInfo            = SOSCCCopyValidPeerPeerInfo_Server,
    .soscc_ValidateUserPublic               = SOSCCValidateUserPublic_Server,
    .soscc_CopyNotValidPeerPeerInfo         = SOSCCCopyNotValidPeerPeerInfo_Server,
    .soscc_CopyRetirementPeerInfo           = SOSCCCopyRetirementPeerInfo_Server,
    .soscc_CopyPeerInfo                     = SOSCCCopyPeerPeerInfo_Server,
    .soscc_CopyConcurringPeerInfo           = SOSCCCopyConcurringPeerPeerInfo_Server,
    .ota_CopyEscrowCertificates             = SecOTAPKICopyCurrentEscrowCertificates,
    .sec_ota_pki_get_new_asset              = SecOTAPKISignalNewAsset,
    .soscc_ProcessSyncWithAllPeers          = SOSCCProcessSyncWithAllPeers_Server,
    .soscc_EnsurePeerRegistration           = SOSCCProcessEnsurePeerRegistration_Server,
    .sec_roll_keys                          = _SecServerRollKeys,
    .soscc_RequestDeviceID                  = SOSCCRequestDeviceID_Server,
    .soscc_SetDeviceID                      = SOSCCSetDeviceID_Server,
    .sec_keychain_sync_update_circle        = _SecServerKeychainSyncUpdateCircle,
    .sec_keychain_sync_update_message       = _SecServerKeychainSyncUpdateMessage,
    .sec_get_log_settings                   = SecCopyLogSettings_Server,
    .sec_set_xpc_log_settings               = SecSetXPCLogSettings_Server,
};

void securityd_init_server(void) {
    gSecurityd = &spi;
    SecPolicyServerInitalize();
}

void securityd_init(char* home_path) {
    if (home_path)
        SetCustomHomeURL(home_path);

    securityd_init_server();
}
