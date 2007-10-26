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
 * dptMacTpAsn1Templates.h - ASN1 templates for .mac TP
 */
 
#ifndef	_DOT_MAC_TP_TEMPLATES_H_
#define _DOT_MAC_TP_TEMPLATES_H_

#include <security_asn1/secasn1.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * This is the ReferenceIdentitifier data returned from 
 * CSSM_TP_SubmitCredRequest() in the CSSMERR_APPLE_DOTMAC_REQ_QUEUED case.
 * It contains sufficient info to allow CSSM_TP_RetrieveCredResult() to 
 * attempt to retrieve the requested cert. 
 *
 * DotMacTpPendingRequest ::=  SEQUENCE {
 *		userName			UTF8String,
 *		certTypeTag			INTEGER }		// CSSM_DOT_MAC_TYPE_ICHAT, etc. 
 */
typedef struct {
	CSSM_DATA			userName;
	CSSM_DATA			certTypeTag;
} DotMacTpPendingRequest;

extern const SecAsn1Template DotMacTpPendingRequestTemplate[];

#ifdef __cplusplus
}
#endif

#endif	/* _DOT_MAC_TP_TEMPLATES_H_ */

