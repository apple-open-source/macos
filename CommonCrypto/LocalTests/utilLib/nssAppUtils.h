/*
 * Copyright (c) 2003 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please 
 * obtain a copy of the License at http://www.apple.com/publicsource and 
 * read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER 
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, 
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. 
 * Please see the License for the specific language governing rights and 
 * limitations under the License.
 */
/*
 * nssAppUtils.h
 */
 
#ifndef	_NSS_APP_UTILS_H_
#define _NSS_APP_UTILS_H_

#include <Security/cssmtype.h>

#ifdef __cplusplus
extern "C" {
#endif

CSSM_RETURN extractDsaPartial(
	CSSM_CSP_HANDLE cspHand,
	const CSSM_KEY *pubKey, 
	CSSM_KEY_PTR pubKeyPartial);

#ifdef __cplusplus
}
#endif

#endif	/* _NSS_APP_UTILS_H_ */

