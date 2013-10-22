/*
 *  spi.c
 *  Security
 *
 *  Created by Michael Brouwer on 1/28/09.
 *  Copyright (c) 2009-2010 Apple Inc.. All Rights Reserved.
 *
 */

#include <securityd/spi.h>
#include <securityd_client.h>
#include <securityd/SecPolicyServer.h>
#include <securityd/SecItemServer.h>
#include <securityd/SecTrustStoreServer.h>
#include <SecureObjectSync/SOSPeerInfo.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFError.h>
#include <securityd/SOSCloudCircleServer.h>

#include "securityd_client.h"
#include <CoreFoundation/CFXPCBridge.h>
#include "utilities/iOSforOSX.h"
#include "utilities/SecFileLocations.h"
#include "OTATrustUtilities.h"

static struct securityd spi = {
    .sec_item_add                           = _SecItemAdd,
    .sec_item_copy_matching                 = _SecItemCopyMatching,
    .sec_item_update                        = _SecItemUpdate,
    .sec_item_delete                        = _SecItemDelete,
    .sec_trust_store_for_domain             = SecTrustStoreForDomainName,
    .sec_trust_store_contains               = SecTrustStoreContainsCertificateWithDigest,
    .sec_trust_store_set_trust_settings     = _SecTrustStoreSetTrustSettings,
    .sec_trust_store_remove_certificate     = SecTrustStoreRemoveCertificateWithDigest,
    .sec_truststore_remove_all              = _SecTrustStoreRemoveAll,
    .sec_item_delete_all                    = _SecItemDeleteAll,
    .sec_trust_evaluate                     = SecTrustServerEvaluate,
    .sec_keychain_backup                    = _SecServerKeychainBackup,
    .sec_keychain_restore                   = _SecServerKeychainRestore,
    .sec_keychain_sync_update               = _SecServerKeychainSyncUpdate,
    .sec_keychain_backup_syncable           = _SecServerBackupSyncable,
    .sec_keychain_restore_syncable          = _SecServerRestoreSyncable,
    .sec_ota_pki_asset_version              = SecOTAPKIGetCurrentAssetVersion,
    .soscc_TryUserCredentials               = SOSCCTryUserCredentials_Server,
    .soscc_SetUserCredentials               = SOSCCSetUserCredentials_Server,
    .soscc_CanAuthenticate                  = SOSCCCanAuthenticate_Server,
    .soscc_PurgeUserCredentials             = SOSCCPurgeUserCredentials_Server,
    .soscc_ThisDeviceIsInCircle             = SOSCCThisDeviceIsInCircle_Server,
    .soscc_RequestToJoinCircle              = SOSCCRequestToJoinCircle_Server,
    .soscc_RequestToJoinCircleAfterRestore  = SOSCCRequestToJoinCircleAfterRestore_Server,
    .soscc_ResetToOffering                  = SOSCCResetToOffering_Server,
    .soscc_ResetToEmpty                     = SOSCCResetToEmpty_Server,
    .soscc_RemoveThisDeviceFromCircle       = SOSCCRemoveThisDeviceFromCircle_Server,
    .soscc_BailFromCircle                   = SOSCCBailFromCircle_Server,
    .soscc_AcceptApplicants                 = SOSCCAcceptApplicants_Server,
    .soscc_RejectApplicants                 = SOSCCRejectApplicants_Server,
    .soscc_CopyApplicantPeerInfo            = SOSCCCopyApplicantPeerInfo_Server,
    .soscc_CopyPeerInfo                     = SOSCCCopyPeerPeerInfo_Server,
    .soscc_CopyConcurringPeerInfo           = SOSCCCopyConcurringPeerPeerInfo_Server,
    .ota_CopyEscrowCertificates				= SecOTAPKICopyCurrentEscrowCertificates,
	.sec_ota_pki_get_new_asset              = SecOTAPKISignalNewAsset,
    .soscc_ProcessSyncWithAllPeers          = SOSCCProcessSyncWithAllPeers_Server
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
