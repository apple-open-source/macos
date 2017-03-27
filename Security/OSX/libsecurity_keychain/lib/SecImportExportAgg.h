/*
 * Copyright (c) 2004,2011,2014 Apple Inc. All Rights Reserved.
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
 * SecImportExportAgg.h - private routines used by SecImportExport.h for 
 *						  aggregate (PKCS12 and PKCS7) conversion.
 */

#ifndef	_SECURITY_SEC_IMPORT_EXPORT_AGG_H_
#define _SECURITY_SEC_IMPORT_EXPORT_AGG_H_

#include <Security/SecImportExport.h>
#include "SecExternalRep.h"

#ifdef	__cplusplus
extern "C" {
#endif

OSStatus impExpPkcs12Export(
	CFArrayRef							exportReps,		// SecExportReps
	SecItemImportExportFlags			flags,		
	const SecKeyImportExportParameters	*keyParams,		// optional 
	CFMutableDataRef					outData);		// output appended here

OSStatus impExpPkcs7Export(
	CFArrayRef							exportReps,		// SecExportReps
	SecItemImportExportFlags			flags,			// kSecItemPemArmour, etc. 	
	const SecKeyImportExportParameters	*keyParams,		// optional 
	CFMutableDataRef					outData);		// output appended here

OSStatus impExpPkcs12Import(
	CFDataRef							inData,
	SecItemImportExportFlags			flags,		
	const SecKeyImportExportParameters	*keyParams,		// optional 
	ImpPrivKeyImportState				&keyImportState,// IN/OUT
	/* caller must supply one of these */
	SecKeychainRef						importKeychain, // optional
	CSSM_CSP_HANDLE						cspHand,		// optional
	CFMutableArrayRef					outArray);		// optional, append here 
	
OSStatus impExpPkcs7Import(
	CFDataRef							inData,
	SecItemImportExportFlags			flags,		
	const SecKeyImportExportParameters	*keyParams,		// optional 
	SecKeychainRef						importKeychain,	// optional
	CFMutableArrayRef					outArray);		// optional, append here 

OSStatus impExpNetscapeCertImport(
	CFDataRef							inData,
	SecItemImportExportFlags			flags,		
	const SecKeyImportExportParameters	*keyParams,		// optional 
	SecKeychainRef						importKeychain, // optional
	CFMutableArrayRef					outArray);		// optional, append here 

OSStatus impExpImportCertCommon(
	const CSSM_DATA		*cdata,
	SecKeychainRef		importKeychain, // optional
	CFMutableArrayRef	outArray);		// optional, append here 

#ifdef	__cplusplus
}
#endif

#endif  /* _SECURITY_SEC_IMPORT_EXPORT_AGG_H_ */
