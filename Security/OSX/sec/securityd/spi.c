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

#if TARGET_DARWINOS
#undef OCTAGON
#undef SECUREOBJECTSYNC
#undef SHAREDWEBCREDENTIALS
#endif

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfour-char-constants"

#include "securityd/spi.h"
#include "ipc/SecdWatchdog.h"
#include "ipc/server_security_helpers.h"
#include "ipc/securityd_client.h"
#include "securityd/SecItemBackupServer.h"
#include "securityd/SecItemServer.h"
#include <Security/SecureObjectSync/SOSPeerInfo.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFError.h>
#include "securityd/SOSCloudCircleServer.h"
#include "securityd/SecOTRRemote.h"
#include "securityd/SecLogSettingsServer.h"

#include <CoreFoundation/CFXPCBridge.h>
#include "utilities/iOSforOSX.h"
#include "utilities/SecFileLocations.h"
#include "OTATrustUtilities.h"
#include "../../trustd/trustd_spi.h"

#pragma clang diagnostic pop

static struct securityd securityd_spi = {
    .sec_item_add                           = _SecItemAdd,
    .sec_item_copy_matching                 = _SecItemCopyMatching,
    .sec_item_update                        = _SecItemUpdate,
    .sec_item_delete                        = _SecItemDelete,
    .sec_item_delete_all                    = _SecItemDeleteAll,
    .sec_keychain_backup                    = _SecServerKeychainCreateBackup,
    .sec_keychain_restore                   = _SecServerKeychainRestore,
    .sec_item_copy_parent_certificates      = _SecItemCopyParentCertificates,
    .sec_item_certificate_exists            = _SecItemCertificateExists,
    .sec_roll_keys                          = _SecServerRollKeysGlue,
    .sec_item_update_token_items            = _SecItemUpdateTokenItems,
    .sec_delete_items_with_access_groups    = _SecItemServerDeleteAllWithAccessGroups,
#if SHAREDWEBCREDENTIALS
    .sec_add_shared_web_credential          = _SecAddSharedWebCredential,
    .sec_copy_shared_web_credential         = _SecCopySharedWebCredential,
#endif
#if SECUREOBJECTSYNC
    .sec_keychain_backup_syncable           = _SecServerBackupSyncable,
    .sec_keychain_restore_syncable          = _SecServerRestoreSyncable,
    .sec_item_backup_copy_names             = SecServerItemBackupCopyNames,
    .sec_item_backup_handoff_fd             = SecServerItemBackupHandoffFD,
    .sec_item_backup_set_confirmed_manifest = SecServerItemBackupSetConfirmedManifest,
    .sec_item_backup_restore                = SecServerItemBackupRestore,
    .sec_otr_session_create_remote          = _SecOTRSessionCreateRemote,
    .sec_otr_session_process_packet_remote  = _SecOTRSessionProcessPacketRemote,
    .soscc_TryUserCredentials               = SOSCCTryUserCredentials_Server,
    .soscc_SetUserCredentials               = SOSCCSetUserCredentials_Server,
    .soscc_SetUserCredentialsAndDSID        = SOSCCSetUserCredentialsAndDSID_Server,
    .soscc_SetUserCredentialsAndDSIDWithAnalytics  = SOSCCSetUserCredentialsAndDSIDWithAnalytics_Server,
    .soscc_CanAuthenticate                  = SOSCCCanAuthenticate_Server,
    .soscc_PurgeUserCredentials             = SOSCCPurgeUserCredentials_Server,
    .soscc_ThisDeviceIsInCircle             = SOSCCThisDeviceIsInCircle_Server,
    .soscc_RequestToJoinCircle              = SOSCCRequestToJoinCircle_Server,
    .soscc_RequestToJoinCircleAfterRestore  = SOSCCRequestToJoinCircleAfterRestore_Server,
    .soscc_RequestToJoinCircleAfterRestoreWithAnalytics  = SOSCCRequestToJoinCircleAfterRestoreWithAnalytics_Server,
    .soscc_RequestEnsureFreshParameters     = SOSCCRequestEnsureFreshParameters_Server,
    .soscc_GetAllTheRings                   = SOSCCGetAllTheRings_Server,
    .soscc_ApplyToARing                     = SOSCCApplyToARing_Server,
    .soscc_WithdrawlFromARing               = SOSCCWithdrawlFromARing_Server,
    .soscc_EnableRing                       = SOSCCEnableRing_Server,
    .soscc_RingStatus                       = SOSCCRingStatus_Server,
    .soscc_SetToNew                         = SOSCCAccountSetToNew_Server,
    .soscc_ResetToOffering                  = SOSCCResetToOffering_Server,
    .soscc_ResetToEmpty                     = SOSCCResetToEmpty_Server,
    .soscc_ResetToEmptyWithAnalytics        = SOSCCResetToEmptyWithAnalytics_Server,
    .soscc_View                             = SOSCCView_Server,
    .soscc_ViewSet                          = SOSCCViewSet_Server,
    .soscc_ViewSetWithAnalytics             = SOSCCViewSetWithAnalytics_Server,
    .soscc_RemoveThisDeviceFromCircle       = SOSCCRemoveThisDeviceFromCircle_Server,
    .soscc_RemoveThisDeviceFromCircleWithAnalytics = SOSCCRemoveThisDeviceFromCircleWithAnalytics_Server,
    .soscc_RemovePeersFromCircle            = SOSCCRemovePeersFromCircle_Server,
    .soscc_RemovePeersFromCircleWithAnalytics = SOSCCRemovePeersFromCircleWithAnalytics_Server,
    .soscc_LoggedOutOfAccount               = SOSCCLoggedOutOfAccount_Server,
    .soscc_BailFromCircle                   = SOSCCBailFromCircle_Server,
    .soscc_AcceptApplicants                 = SOSCCAcceptApplicants_Server,
    .soscc_RejectApplicants                 = SOSCCRejectApplicants_Server,
    .soscc_CopyApplicantPeerInfo            = SOSCCCopyApplicantPeerInfo_Server,
    .soscc_CopyGenerationPeerInfo           = SOSCCCopyGenerationPeerInfo_Server,
    .soscc_CopyValidPeerPeerInfo            = SOSCCCopyValidPeerPeerInfo_Server,
    .soscc_ValidateUserPublic               = SOSCCValidateUserPublic_Server,
    .soscc_CopyNotValidPeerPeerInfo         = SOSCCCopyNotValidPeerPeerInfo_Server,
    .soscc_CopyRetirementPeerInfo           = SOSCCCopyRetirementPeerInfo_Server,
    .soscc_CopyViewUnawarePeerInfo          = SOSCCCopyViewUnawarePeerInfo_Server,
    .soscc_CopyEngineState                  = SOSCCCopyEngineState_Server,
    .soscc_CopyPeerInfo                     = SOSCCCopyPeerPeerInfo_Server,
    .soscc_CopyConcurringPeerInfo           = SOSCCCopyConcurringPeerPeerInfo_Server,
    .soscc_ProcessSyncWithPeers             = SOSCCProcessSyncWithPeers_Server,
    .soscc_ProcessSyncWithAllPeers          = SOSCCProcessSyncWithAllPeers_Server,
    .soscc_EnsurePeerRegistration           = SOSCCProcessEnsurePeerRegistration_Server,
    .sec_keychain_sync_update_message       = _SecServerKeychainSyncUpdateMessage,
    .sec_get_log_settings                   = SecCopyLogSettings_Server,
    .sec_set_xpc_log_settings               = SecSetXPCLogSettings_Server,
    .sec_set_circle_log_settings            = SecSetCircleLogSettings_Server,
    .soscc_CopyMyPeerInfo                   = SOSCCCopyMyPeerInfo_Server,
	.soscc_SetLastDepartureReason           = SOSCCSetLastDepartureReason_Server,
    .soscc_SetNewPublicBackupKey            = SOSCCSetNewPublicBackupKey_Server,
    .soscc_RegisterSingleRecoverySecret     = SOSCCRegisterSingleRecoverySecret_Server,
    .soscc_WaitForInitialSync               = SOSCCWaitForInitialSync_Server,
    .soscc_WaitForInitialSyncWithAnalytics  = SOSCCWaitForInitialSyncWithAnalytics_Server,
    .soscc_CopyYetToSyncViewsList           = SOSCCCopyYetToSyncViewsList_Server,
    .soscc_SetEscrowRecords                 = SOSCCSetEscrowRecord_Server,
    .soscc_CopyEscrowRecords                = SOSCCCopyEscrowRecord_Server,
    .sosbskb_WrapToBackupSliceKeyBagForView = SOSWrapToBackupSliceKeyBagForView_Server,
    .soscc_CopyAccountState                 = SOSCCCopyAccountState_Server,
    .soscc_DeleteAccountState               = SOSCCDeleteAccountState_Server,
    .soscc_CopyEngineData                   = SOSCCCopyEngineData_Server,
    .soscc_DeleteEngineState                = SOSCCDeleteEngineState_Server,
    .soscc_AccountHasPublicKey              = SOSCCAccountHasPublicKey_Server,
    .soscc_AccountIsNew                     = SOSCCAccountIsNew_Server,
    .soscc_IsThisDeviceLastBackup           = SOSCCkSecXPCOpIsThisDeviceLastBackup_Server,
    .soscc_SOSCCPeersHaveViewsEnabled       = SOSCCPeersHaveViewsEnabled_Server,
    .soscc_RegisterRecoveryPublicKey        = SOSCCRegisterRecoveryPublicKey_Server,
    .soscc_CopyRecoveryPublicKey            = SOSCCCopyRecoveryPublicKey_Server,
    .soscc_CopyBackupInformation            = SOSCCCopyBackupInformation_Server,
    .soscc_SOSCCMessageFromPeerIsPending    = SOSCCMessageFromPeerIsPending_Server,
    .soscc_SOSCCSendToPeerIsPending         = SOSCCSendToPeerIsPending_Server,
#endif /* SECUREOBJECTSYNC */
};

#if SECD_SERVER && SECUREOBJECTSYNC
static CFTypeRef
delayedSOSSharedObject(void)
{
    return SOSKeychainAccountGetSharedAccount();
}
#endif

void securityd_init_local_spi(void) {
    gSecurityd = &securityd_spi;
#if SECD_SERVER && SECUREOBJECTSYNC
    // We're the server: we need to handle this locally. Bring them up and set them in the global object.
    securityd_spi.soscc_status = delayedSOSSharedObject;
#endif
    // You're trying to bring up a (non-trustd) 'securityd'. Create the local handler for securityd XPCs.
    securityd_spi.secd_xpc_server = SecCreateLocalCFSecuritydXPCServer();
}

void securityd_init_server(void) {
    securityd_init_local_spi();

    // Lazy initialization is no good; bring up the keychain on start
    // If you want to not do this, you'll need to check the APSConnection Mach mailbox for messages here instead (good luck)
    CFErrorRef cferror = nil;
    bool keychainAlive = kc_with_dbt(false, &cferror, ^bool(SecDbConnectionRef dbt) {
        secnotice("keychain", "Keychain initialized!");
        return true;
    });
    if(!keychainAlive || cferror) {
        secerror("Couldn't bring up keychain: %@", cferror);
    }
    CFReleaseNull(cferror);

    SecdLoadWatchDog();
}

void securityd_init(CFURLRef home_path) {
    if (home_path) {
        SetCustomHomeURL(home_path);
    }

    securityd_init_server();
#ifdef LIBTRUSTD
    trustd_init_server();
#endif
}
