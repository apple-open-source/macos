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
 *
 * SecImportExportPem.h - private PEM routines for SecImportExport
 */

#ifndef	_SECURITY_SEC_IMPORT_EXPORT_PEM_H_
#define _SECURITY_SEC_IMPORT_EXPORT_PEM_H_

#include "SecImportExport.h"
#include "SecExternalRep.h"

/* take these PEM header strings right from the authoritative source */
#include <openssl/pem.h>

/* Other PEM Header strings not defined in openssl */
#define PEM_STRING_DH_PUBLIC	"DH PUBLIC KEY"
#define PEM_STRING_DH_PRIVATE	"DH PRIVATE KEY"
#define PEM_STRING_PKCS12		"PKCS12"
#define PEM_STRING_SESSION		"SYMMETRIC KEY"
//#define PEM_STRING_ECDSA_PUBLIC	"EC PUBLIC KEY"
#define PEM_STRING_ECDSA_PRIVATE "EC PRIVATE KEY"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * PEM decode incoming data, appending SecImportRep's to specified array.
 * Returned SecImportReps may or may not have a known type and format. 
 * IF incoming data is not PEM or base64, we return errSecSuccess with *isPem false.
 */
OSStatus impExpParsePemToImportRefs(
	CFDataRef			importedData,
	CFMutableArrayRef	importReps,		// output appended here
	bool				*isPem);		// true means we think it was PEM regardless of 
										// final return code	

/*
 * PEM encode a single SecExportRep's data, appending to a CFData.
 */
OSStatus impExpPemEncodeExportRep(
	CFDataRef			derData,
	const char			*pemHeader,
	CFArrayRef			pemParamLines,  // optional 
	CFMutableDataRef	outData);
	
#ifdef	__cplusplus
}
#endif

#endif	/* _SECURITY_SEC_IMPORT_EXPORT_PEM_H_ */
