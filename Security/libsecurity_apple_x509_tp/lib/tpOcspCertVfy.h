/*
 * Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
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
 * tpOcspCertVfy.h - OCSP cert verification routines
 */
 
#ifndef	_TP_OCSP_CERT_VFY_H_
#define _TP_OCSP_CERT_VFY_H_

#include "TPCertInfo.h"
#include "tpCrlVerify.h"
#include <security_asn1/SecNssCoder.h>
#include <security_ocspd/ocspResponse.h>

#ifdef __cplusplus

extern "C" {
#endif

/*
 * Verify an OCSP response in the form of a pre-decoded OCSPResponse. Does 
 * signature verification as well as cert chain verification. Sometimes we can
 * verify if we don't know the issuer; sometimes we can.
 */
typedef enum {
	ORS_Unknown,			// unable to verify one way or another
	ORS_Good,				// known to be good
	ORS_Bad					// known to be bad
} OcspRespStatus;

OcspRespStatus tpVerifyOcspResp(
	TPVerifyContext		&vfyCtx,
	SecNssCoder			&coder,
	TPCertInfo			*issuer,		// issuer of the related cert, may be issuer of 
										//   reply
	OCSPResponse		&ocspResp,
	CSSM_RETURN			&cssmErr);		// possible per-cert error 

#ifdef __cplusplus
}
#endif

#endif	/* _TP_OCSP_CERT_VFY_H_ */

