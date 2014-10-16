/*
 * Copyright (c) 2005-2006 Apple Computer, Inc. All Rights Reserved.
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
 * rootUtils.h - utility routines for rootStoreTool
 */
 
#ifndef	_ROOT_UTILS_H_
#define _ROOT_UTILS_H_

#include <CoreFoundation/CoreFoundation.h>
#include <security_cdsa_utils/cuOidParser.h>
#include <Security/Security.h>

#ifdef __cplusplus
extern "C" {
#endif

void indentIncr(void);
void indentDecr(void);
void indent(void);

void printAscii(
	const char *buf,
	unsigned len,
	unsigned maxLen);
void printHex(
	const unsigned char *buf,
	unsigned len,
	unsigned maxLen);
void printOid(
	const void *buf, 
	unsigned len, 
	OidParser &parser);

typedef enum {
	PD_Hex,
	PD_ASCII,
	PD_OID
} PrintDataType;

void printData(
	const char *label,
	CFDataRef data,
	PrintDataType whichType,
	OidParser &parser);

/* print the contents of a CFString */
void printCfStr(
	CFStringRef cfstr);
	
void printCFDate(
	CFDateRef dateRef);

void printCfNumber(
	CFNumberRef cfNum);

/* print a CFNumber as a resultType */
void printResult(
	CFNumberRef cfNum);

/* print a CFNumber as SecUserTrustKeyUsage */
void printKeyUsage(
	CFNumberRef cfNum);

/* print a CFNumber as CSSM_RETURN string */
void printCssmErr(
	CFNumberRef cfNum);

/* print cert's label (the one SecCertificate infers) */
OSStatus printCertLabel(
	SecCertificateRef certRef);

/* print a CFData as an X509 Name (i.e., subject or issuer) */
void printCfName(
	CFDataRef nameData,
	OidParser &parser);

#ifdef __cplusplus
}
#endif

#endif	/* _ROOT_UTILS_H_ */

