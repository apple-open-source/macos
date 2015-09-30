/*
 * Copyright (c) 2002,2011,2014 Apple Inc. All Rights Reserved.
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
 * TPNetwork.h - LDAP (and eventually) other network tools 
 *
 */
 
#ifndef	_TP_NETWORK_H_
#define _TP_NETWORK_H_

#include <Security/cssmtype.h>
#include "TPCertInfo.h"
#include "TPCrlInfo.h"

extern "C" {

/*
 * Fetch CRL(s) for specified cert if the cert has a cRlDistributionPoint
 * extension. If a non-NULL CRL is returned, it has passed verification
 * with specified TPVerifyContext.
 * The common, trivial failure of "no URI in a cRlDistributionPoint 
 * extension" is indicated by CSSMERR_APPLETP_CRL_NOT_FOUND.
 */
extern CSSM_RETURN tpFetchCrlFromNet(
	TPCertInfo 			&cert,
	TPVerifyContext		&verifyContext,
	TPCrlInfo			*&crl);				// RETURNED

/*
 * Fetch issuer cert of specified cert if the cert has an issuerAltName
 * with a URI. If non-NULL cert is returned, it has passed subject/issuer
 * name comparison and signature verification with target cert.
 * The common, trivial failure of "no URI in an issuerAltName 
 * extension" is indicated by CSSMERR_TP_CERTGROUP_INCOMPLETE.
 * A CSSMERR_CSP_APPLE_PUBLIC_KEY_INCOMPLETE return indicates that
 * subsequent signature verification is needed. 
 */
extern CSSM_RETURN tpFetchIssuerFromNet(
	TPCertInfo			&subject,
	CSSM_CL_HANDLE		clHand,
	CSSM_CSP_HANDLE		cspHand,
	const char			*verifyTime,
	TPCertInfo			*&issuer);			// RETURNED
	
}

#endif	/* TP_NETWORK_H_ */
