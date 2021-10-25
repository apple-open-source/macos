/*
 * Copyright (c) 2000-2011,2012,2013-2014,2016 Apple Inc. All Rights Reserved.
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

#ifndef _SECURITY_PRIVATE_H_
#define _SECURITY_PRIVATE_H_

#include <Security/certExtensionTemplates.h>
#include <Security/CKKSControl.h>
#include <Security/CKKSControlProtocol.h>
#include <Security/CMSPrivate.h>
#include <Security/CSCommonPriv.h>
#include <Security/der_plist.h>
#include <Security/EscrowRequestXPCProtocol.h>
#include <Security/LocalKeychainAnalytics.h>
#include <Security/nameTemplates.h>
#include <Security/ocspTemplates.h>
#include <Security/OctagonSignPosts.h>
#include <Security/OTClique.h>
#include <Security/OTClique+Private.h>
#include <Security/OTConstants.h>
#include <Security/OTControl.h>
#include <Security/OTControlProtocol.h>
#include <Security/OTJoiningConfiguration.h>
#include <Security/SecAccessControlPriv.h>
#include <Security/secasn1t.h>
#include <Security/SecBase64.h>
#include <Security/SecBasePriv.h>
#include <Security/SecCertificateRequest.h>
#include <Security/SecCFAllocator.h>
#include <Security/SecCMS.h>
#include <Security/SecCmsBase.h>
#include <Security/SecCmsContentInfo.h>
#include <Security/SecCmsDecoder.h>
#include <Security/SecCmsDigestContext.h>
#include <Security/SecCmsDigestedData.h>
#include <Security/SecCmsEncoder.h>
#include <Security/SecCmsEncryptedData.h>
#include <Security/SecCmsEnvelopedData.h>
#include <Security/SecCmsMessage.h>
#include <Security/SecCmsRecipientInfo.h>
#include <Security/SecCmsSignedData.h>
#include <Security/SecCmsSignerInfo.h>
#include <Security/SecCodePriv.h>
#include <Security/SecCodeSigner.h>
#include <Security/SecCoreAnalytics.h>
#include <Security/SecDH.h>
#include <Security/SecECKey.h>
#include <Security/SecEntitlements.h>
#include <Security/SecEscrowRequest.h>
#include <Security/SecExperimentPriv.h>
#include <Security/SecFramework.h>
#include <Security/SecIdentityPriv.h>
#include <Security/SecImportExportPriv.h>
#include <Security/SecInternalReleasePriv.h>
#include <Security/SecItemBackup.h>
#include <Security/SecItemPriv.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecKeyProxy.h>
#include <Security/SecKnownFilePaths.h>
#include <Security/SecLogging.h>
#include <Security/SecOTR.h>
#include <Security/SecOTRSession.h>
#include <Security/SecPaddingConfigurationsPriv.h>
#include <Security/SecPasswordGenerate.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecProtocolConfiguration.h>
#include <Security/SecRecoveryKey.h>
#include <Security/SecServerEncryptionSupport.h>
#include <Security/SecSignpost.h>
#include <Security/SecSMIME.h>
#include <Security/SecStaticCodePriv.h>
#include <Security/SecTaskPriv.h>
#include <Security/SecTrustPriv.h>
#include <Security/SecTrustSettingsPriv.h>
#include <Security/SecTrustStore.h>
#include <Security/SecureObjectSync/SOSBackupSliceKeyBag.h>
#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include <Security/SecureObjectSync/SOSCloudCircleInternal.h>
#include <Security/SecureObjectSync/SOSPeerInfo.h>
#include <Security/SecureObjectSync/SOSTypes.h>
#include <Security/SecureObjectSync/SOSViews.h>
#include <Security/SecureTransportPriv.h>
#include <Security/SecXPCError.h>
#include <Security/SecXPCHelper.h>
#include <Security/SFAnalytics.h>
#include <Security/SFAnalyticsActivityTracker.h>
#include <Security/SFAnalyticsDefines.h>
#include <Security/SFAnalyticsMultiSampler.h>
#include <Security/SFAnalyticsSampler.h>
#include <Security/SFAnalyticsSQLiteStore.h>
#include <Security/SFSQLite.h>
#include <Security/SOSAnalytics.h>
#include <Security/SOSControlHelper.h>
#include <Security/X509Templates.h>

#if SEC_OS_OSX_INCLUDES
#include <Security/AuthorizationPriv.h>
#include <Security/AuthorizationTagsPriv.h>
#include <Security/AuthorizationTrampolinePriv.h>
#include <Security/checkpw.h>
#include <Security/csrTemplates.h>
#include <Security/cssmapplePriv.h>
#include <Security/keyTemplates.h>
#include <Security/mdspriv.h>
#include <Security/osKeyTemplates.h>
#include <Security/SecAccessPriv.h>
#include <Security/SecAssessment.h>
#include <Security/SecExternalSourceTransform.h>
#include <Security/SecFDERecoveryAsymmetricCrypto.h>
#include <Security/SecIdentitySearchPriv.h>
#include <Security/SecKeychainItemExtendedAttributes.h>
#include <Security/SecKeychainItemPriv.h>
#include <Security/SecKeychainPriv.h>
#include <Security/SecKeychainSearchPriv.h>
#include <Security/SecNullTransform.h>
#include <Security/SecPassword.h>
#include <Security/SecRandomP.h>
#include <Security/SecRecoveryPassword.h>
#include <Security/SecRequirementPriv.h>
#include <Security/SecTransformInternal.h>
#include <Security/SecTranslocate.h>
#include <Security/SecTrustedApplicationPriv.h>
#include <Security/sslTypes.h>
#include <Security/TrustSettingsSchema.h>
#include <Security/tsaSupport.h>
#elif !TARGET_OS_MACCATALYST
#include <Security/CMSDecoder.h>
#include <Security/CMSEncoder.h>
#include <Security/CodeSigning.h>
#include <Security/CSCommon.h>
#include <Security/NtlmGenerator.h>
#include <Security/oidsalg.h>
#include <Security/oidsocsp.h>
#include <Security/pbkdf2.h>
#include <Security/SecAsn1Coder.h>
#include <Security/SecAsn1Templates.h>
#include <Security/SecAsn1Types.h>
#include <Security/SecCode.h>
#include <Security/SecECKeyPriv.h>
#include <Security/SecEMCSPriv.h>
#include <Security/SecOTRDHKey.h>
#include <Security/SecOTRErrors.h>
#include <Security/SecOTRMath.h>
#include <Security/SecOTRPackets.h>
#include <Security/SecOTRSessionPriv.h>
#include <Security/SecPBKDF.h>
#include <Security/SecRequirement.h>
#include <Security/SecRSAKey.h>
#include <Security/SecSCEP.h>
#include <Security/SecStaticCode.h>
#include <Security/SecTask.h>
#include <Security/SecTrustSettings.h>
#include <Security/SecureObjectSync/SOSPeerInfoV2.h>
#include <Security/vmdh.h>
#endif

#endif // _SECURITY_PRIVATE_H_
