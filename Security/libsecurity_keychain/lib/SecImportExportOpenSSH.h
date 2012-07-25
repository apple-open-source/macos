/*
 * Copyright (c) 2006 Apple Computer, Inc. All Rights Reserved.
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
 * SecImportExportOpenSSH.h - support for importing and exporting OpenSSH keys. 
 *
 * Created 8/31/2006 by dmitch.
 */

#ifndef	_SEC_IMPORT_EXPORT_OPENSSH_H_
#define _SEC_IMPORT_EXPORT_OPENSSH_H_

#include <Security/SecImportExport.h>
#include <security_cdsa_utilities/cssmdata.h>

#ifdef	__cplusplus
extern "C" {
#endif

/* 
 * Infer PrintName attribute from raw key's 'comment' field. 
 * Returned string is mallocd and must be freed by caller. 
 */
extern char *impExpOpensshInferPrintName(
	CFDataRef external, 
	SecExternalItemType externType, 
	SecExternalFormat externFormat);
	
/* 
 * Infer DescriptiveData (i.e., comment) from a SecKeyRef's PrintName
 * attribute.
 */
extern void impExpOpensshInferDescData(
	SecKeyRef keyRef,
	CssmOwnedData &descData);
	
/*
 * If cspHand is provided instead of importKeychain, the CSP 
 * handle MUST be for the CSPDL, not for the raw CSP.
 */
extern OSStatus impExpWrappedOpenSSHImport(
	CFDataRef							inData,
	SecKeychainRef						importKeychain, // optional
	CSSM_CSP_HANDLE						cspHand,		// required
	SecItemImportExportFlags			flags,
	const SecKeyImportExportParameters	*keyParams,		// optional 
	const char							*printName,
	CFMutableArrayRef					outArray);		// optional, append here 

extern OSStatus impExpWrappedOpenSSHExport(
	SecKeyRef							secKey,
	SecItemImportExportFlags			flags,		
	const SecKeyImportExportParameters	*keyParams,		// optional 
	const CssmData						&descData,
	CFMutableDataRef					outData);		// output appended here

#ifdef	__cplusplus
}
#endif

#endif	/* _SEC_IMPORT_EXPORT_OPENSSH_H_ */
