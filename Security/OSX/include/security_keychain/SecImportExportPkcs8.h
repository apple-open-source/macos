/*
 * Copyright (c) 2000-2004,2011,2014 Apple Inc. All Rights Reserved.
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
 * SecImportExportPkcs8.h - support for generating and parsing/decoding 
 * private keys in PKCS8 format.  
 */
 
#ifndef _SEC_IMPORT_EXPORT_PKCS8_H_
#define _SEC_IMPORT_EXPORT_PKCS8_H_

#include <Security/secasn1t.h>
#include <Security/keyTemplates.h>	

#ifdef  __cplusplus
extern "C" {
#endif

/*
 * If cspHand is provided instead of importKeychain, the CSP 
 * handle MUST be for the CSPDL, not for the raw CSP.
 */
OSStatus impExpPkcs8Import(
	CFDataRef							inData,
	SecKeychainRef						importKeychain, // optional
	CSSM_CSP_HANDLE						cspHand,		// required
	SecItemImportExportFlags			flags,
	const SecKeyImportExportParameters	*keyParams,		// optional 
	CFMutableArrayRef					outArray);		// optional, append here 

OSStatus impExpPkcs8Export(
	SecKeyRef							secKey,
	SecItemImportExportFlags			flags,		
	const SecKeyImportExportParameters	*keyParams,		// optional 
	CFMutableDataRef					outData,		// output appended here
	const char							**pemHeader);	

#ifdef  __cplusplus
}
#endif

#endif  /* _SEC_IMPORT_EXPORT_PKCS8_H_ */

