/*
 * Copyright (c) 2000-2015 Apple Inc. All Rights Reserved.
 *
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/*
	tpPolicies.h - TP module policy implementation
*/

#ifndef	_TP_POLICIES_H_
#define _TP_POLICIES_H_

#include <Security/cssmtype.h>
#include <security_utilities/alloc.h>
#include <Security/cssmapple.h>
#include "TPCertInfo.h"

#ifdef __cplusplus
extern	"C" {
#endif /* __cplusplus */

/*
 * Enumerated certificate policies enforced by this module.
 */
typedef enum {
	kTPDefault,			/* no extension parsing, just sig and expiration */
	kTPx509Basic,		/* basic X.509/RFC3280 */
	kTPiSign,			/* (obsolete) Apple code signing */
	kTP_SSL,			/* SecureTransport/SSL */
	kCrlPolicy,			/* cert chain verification via CRL */
	kTP_SMIME,			/* S/MIME */
	kTP_EAP,
	kTP_SWUpdateSign,	/* Apple SW Update signing (was Apple Code Signing) */
	kTP_ResourceSign,	/* Apple Resource Signing */
	kTP_IPSec,			/* IPSEC */
	kTP_iChat,			/* iChat */
	kTP_PKINIT_Client,	/* PKINIT client cert */
	kTP_PKINIT_Server,	/* PKINIT server cert */
	kTP_CodeSigning,	/* new Apple Code Signing (Leopard/10.5) */
	kTP_PackageSigning,	/* Package Signing */
	kTP_MacAppStoreRec,	/* MacApp store receipt */
	kTP_AppleIDSharing,	/* AppleID Sharing */
	kTP_TimeStamping,	/* RFC3161 time stamping */
	kTP_PassbookSigning,	/* Passbook Signing */
	kTP_MobileStore,	/* Apple Mobile Store Signing */
	kTP_TestMobileStore,	/* Apple Test Mobile Store Signing */
	kTP_EscrowService,	/* Apple Escrow Service Signing */
	kTP_ProfileSigning,	/* Apple Configuration Profile Signing */
	kTP_QAProfileSigning,	/* Apple QA Configuration Profile Signing */
	kTP_PCSEscrowService,	/* Apple PCS Escrow Service Signing */
	kTP_ProvisioningProfileSigning, /* Apple OS X Provisioning Profile Signing */
} TPPolicy;

/*
 * Perform TP verification on a constructed (ordered) cert group.
 */
CSSM_RETURN tp_policyVerify(
	TPPolicy						policy,
	Allocator						&alloc,
	CSSM_CL_HANDLE					clHand,
	CSSM_CSP_HANDLE					cspHand,
	TPCertGroup 					*certGroup,
	CSSM_BOOL						verifiedToRoot,		// last cert is good root
	CSSM_BOOL						verifiedViaTrustSetting,// last cert has valid user trust
	CSSM_APPLE_TP_ACTION_FLAGS		actionFlags,
	const CSSM_DATA					*policyFieldData,	// optional
    void 							*policyControl);	// future use

/*
 * Obtain policy-specific User Trust parameters
 */
void tp_policyTrustSettingParams(
	TPPolicy				policy,
	const CSSM_DATA			*policyFieldData,		// optional
	/* returned values - not mallocd */
	const char				**policyStr,
	uint32					*policyStrLen,
	SecTrustSettingsKeyUsage	*keyUse);

#ifdef __cplusplus
}
#endif
#endif	/* _TP_POLICIES_H_ */
