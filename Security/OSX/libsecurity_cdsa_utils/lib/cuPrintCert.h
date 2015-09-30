/*
 * Copyright (c) 2002,2011,2014 Apple Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. 
 * Please obtain a copy of the License at http://www.apple.com/publicsource
 * and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, 
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights 
 * and limitations under the License.
 */
 
/* 
 * cuPrintCert.h - text-based cert/CRL parser using CL
 */

#ifndef	_PRINT_CERT_H_
#define _PRINT_CERT_H_

#include <Security/cssmtype.h>
#include <security_cdsa_utils/cuOidParser.h>
#include <Security/x509defs.h>

#ifdef	__cplusplus
extern "C" {
#endif

/* print one field */
void printCertField(
	const CSSM_FIELD 		&field,
	OidParser 				&parser,
	CSSM_BOOL				verbose);

/* parse cert & print it */
int printCert(
	const  unsigned char 	*certData,
	unsigned				certLen,
	CSSM_BOOL				verbose);

/* print parsed CRL */
void printCrlFields(
	const CSSM_X509_SIGNED_CRL *signedCrl,
	OidParser 			&parser);

/* parse CRL & print it */
int printCrl(
	const  unsigned char 	*crlData,
	unsigned				crlLen,
	CSSM_BOOL				verbose);


void printCertShutdown();

#ifdef	__cplusplus
}
#endif

#endif	/* _PRINT_CERT_H_ */
