/*
 * Copyright (c) 2012 Apple Computer, Inc. All Rights Reserved.
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
 *
 * tsaTemplates.h -  ASN1 templates Time Stamping Authority requests and responses.
 * see rfc3161.asn1 for ASN.1 and other comments
 */

#ifndef	_TSA_TEMPLATES_H_
#define _TSA_TEMPLATES_H_

#include <Security/secasn1t.h>
#include <Security/x509defs.h>      /* CSSM_X509_ALGORITHM_IDENTIFIER */
#include <Security/X509Templates.h> /* NSS_CertExtension */
#include <Security/nameTemplates.h> /* NSS_GeneralName and support */
#include "cmstpriv.h"               /* SecCmsContentInfo */

#ifdef  __cplusplus
extern "C" {
#endif

#pragma mark ----- TSA Request -----


typedef CSSM_OID TSAPolicyId;

typedef struct {
	CSSM_X509_ALGORITHM_IDENTIFIER  hashAlgorithm;
	CSSM_DATA                       hashedMessage;
} SecAsn1TSAMessageImprint;

typedef struct {
	CSSM_DATA				seconds;    // INTEGER optional
	CSSM_DATA				millis;     // INTEGER optional
	CSSM_DATA				micros;     // INTEGER optional
} SecAsn1TSAAccuracy;

typedef struct {
    CSSM_DATA                   version;            // INTEGER (1)
    SecAsn1TSAMessageImprint    messageImprint;
    TSAPolicyId                 reqPolicy;          // OPTIONAL
    CSSM_DATA                   nonce;              // INTEGER optional
    CSSM_DATA                   certReq;            // BOOL
    CSSM_X509_EXTENSIONS        **extensions;       // [0] IMPLICIT Extensions OPTIONAL
} SecAsn1TSATimeStampReq;

#pragma mark ----- TSA Response -----

typedef struct {
    CSSM_DATA                   status;
	CSSM_DATA                   statusString;      // OPTIONAL
    CSSM_DATA                   failInfo;          // OPTIONAL
} SecAsn1TSAPKIStatusInfo;

typedef SecCmsContentInfo SecTimeStampToken;

typedef struct {
    SecAsn1TSAPKIStatusInfo     status;
    SecTimeStampToken           timeStampToken;     // OPTIONAL
} SecAsn1TimeStampResp;

/*
    We use this to grab the raw DER, but not decode it for subsequent
    re-insertion into a CMS message as an unsigned attribute
*/
    
typedef struct {
    SecAsn1TSAPKIStatusInfo     status;
    CSSM_DATA                   timeStampTokenDER;     // OPTIONAL
} SecAsn1TimeStampRespDER;

typedef struct {
    CSSM_DATA                   version;            // DEFAULT 1    *****
    TSAPolicyId                 reqPolicy;          // OPTIONAL
    SecAsn1TSAMessageImprint    messageImprint;
    CSSM_DATA                   serialNumber;       // INTEGER
    CSSM_DATA                   genTime;
    SecAsn1TSAAccuracy          accuracy;           // OPTIONAL
    CSSM_DATA                   ordering;           // BOOLEAN DEFAULT FALSE
    CSSM_DATA                   nonce;              // INTEGER optional
    CSSM_DATA                   tsa;                // [0] GeneralName         OPTIONAL
    CSSM_X509_EXTENSIONS        **extensions;       // [1] IMPLICIT Extensions OPTIONAL
} SecAsn1TSATSTInfo;

typedef enum {
	PKIS_Granted = 0,
	PKIS_GrantedWithMods = 1,
	PKIS_Rejection = 2,
	PKIS_Waiting = 3,
	PKIS_RevocationWarning = 4,
	PKIS_RevocationNotification = 5
} SecAsn1TSAPKIStatus;

typedef enum {
	FI_BadAlg = 0,
	FI_BadRequest = 2,
	FI_BadDataFormat = 5,
	FI_TimeNotAvailable = 14,
	FI_UnacceptedPolicy = 15,
	FI_UnacceptedExtension = 16,
	FI_AddInfoNotAvailable = 17,
	FI_SystemFailure = 25
} SecAsn1TSAPKIFailureInfo;

    
#ifdef  __cplusplus
}
#endif

#endif	/* _TSA_TEMPLATES_H_ */

