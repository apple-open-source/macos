/*
 * Copyright (c) 2003,2011,2014 Apple Inc. All Rights Reserved.
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
 * pkcs12Derive.cpp - PKCS12 PBE routine
 *
 */
 
#ifndef	_PKCS12_DERIVE_H_
#define _PKCS12_DERIVE_H_

#include <Security/cssmtype.h>
#include <security_cdsa_utilities/context.h>
#include "AppleCSPSession.h"

#ifdef __cplusplus
extern "C" {
#endif

void DeriveKey_PKCS12 (
	const Context &context,
	AppleCSPSession	&session,
	const CssmData &Param,			// other's public key
	CSSM_DATA *keyData);			// mallocd by caller
									// we fill in keyData->Length bytes

#ifdef __cplusplus
}
#endif

#endif	/* _PKCS12_DERIVE_H_ */

