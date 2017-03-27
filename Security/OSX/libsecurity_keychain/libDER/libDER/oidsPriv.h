/*
 * Copyright (c) 2005-2009,2011-2016 Apple Inc. All Rights Reserved.
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


/*
 * oids.h - declaration of OID consts
 *
 */

#ifndef	_LIB_DER_OIDSPRIV_H_
#define _LIB_DER_OIDSPRIV_H_

#include <libDER/oids.h>
#include <stdbool.h>

__BEGIN_DECLS

/* Apple Oids */
extern const DERItem
    oidAppleSecureBootCertSpec,
    oidAppleSecureBootTicketCertSpec,
    oidAppleImg4ManifestCertSpec,
    oidAppleProvisioningProfile,
    oidAppleApplicationSigning,
    oidAppleTVOSApplicationSigningProd,
    oidAppleTVOSApplicationSigningProdQA,
    oidAppleInstallerPackagingSigningExternal,
    oidAppleExtendedKeyUsageCodeSigning,
    oidAppleExtendedKeyUsageCodeSigningDev,
    oidAppleExtendedKeyUsageAppleID,
    oidAppleExtendedKeyUsagePassbook,
    oidAppleExtendedKeyUsageProfileSigning,
    oidAppleExtendedKeyUsageQAProfileSigning,
    oidAppleIntmMarkerAppleWWDR,
    oidAppleIntmMarkerAppleID,
    oidAppleIntmMarkerAppleID2,
    oidApplePushServiceClient,
    oidApplePolicyMobileStore,
    oidApplePolicyMobileStoreProdQA,
    oidApplePolicyEscrowService,
    oidAppleCertExtensionAppleIDRecordValidationSigning,
    oidAppleCertExtOSXProvisioningProfileSigning,
    oidAppleIntmMarkerAppleSystemIntg2,
    oidAppleIntmMarkerAppleSystemIntgG3,
    oidAppleCertExtAppleSMPEncryption,
    oidAppleCertExtAppleServerAuthentication,
    oidAppleCertExtAppleServerAuthenticationIDSProdQA,
    oidAppleCertExtAppleServerAuthenticationIDSProd,
    oidAppleCertExtAppleServerAuthenticationAPNProdQA,
    oidAppleCertExtAppleServerAuthenticationAPNProd,
    oidAppleCertExtAppleServerAuthenticationGS,
    oidAppleCertExtAppleServerAuthenticationPPQProdQA,
    oidAppleCertExtAppleServerAuthenticationPPQProd,
    oidAppleIntmMarkerAppleServerAuthentication,
    oidAppleCertExtApplePPQSigningProd,
    oidAppleCertExtApplePPQSigningProdQA,
    oidAppleCertExtATVAppSigningProd,
    oidAppleCertExtATVAppSigningProdQA,
    oidAppleCertExtATVVPNProfileSigning,
    oidAppleCertExtCryptoServicesExtEncryption,
    oidAppleCertExtAST2DiagnosticsServerAuthProdQA,
    oidAppleCertExtAST2DiagnosticsServerAuthProd,
    oidAppleCertExtEscrowProxyServerAuthProdQA,
    oidAppleCertExtEscrowProxyServerAuthProd,
    oidAppleCertExtFMiPServerAuthProdQA,
    oidAppleCertExtFMiPServerAuthProd,
    oidAppleCertExtHomeKitServerAuth,
    oidAppleIntmMarkerAppleHomeKitServerCA,
    oidAppleCertExtAppleServerAuthenticationMMCSProdQA,
    oidAppleCertExtAppleServerAuthenticationMMCSProd,
    oidAppleCertExtAppleServerAuthenticationiCloudSetupProdQA,
    oidAppleCertExtAppleServerAuthenticationiCloudSetupProd;

    /* Compare two decoded OIDs.  Returns true iff they are equivalent. */
    bool DEROidCompare(const DERItem *oid1, const DERItem *oid2);

__END_DECLS

#endif	/* _LIB_DER_UTILS_H_ */
