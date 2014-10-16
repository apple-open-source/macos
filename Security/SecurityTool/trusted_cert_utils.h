/*
 * Copyright (c) 2003-2004,2006,2014 Apple Inc. All Rights Reserved.
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
 * trusted_cert_utils.h
 */
#ifndef _TRUSTED_CERT_UTILS_H_
#define _TRUSTED_CERT_UTILS_H_  1

#include <Security/SecPolicy.h>
#include <Security/SecTrustedApplication.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CFRELEASE(cf)	if(cf != NULL) { CFRelease(cf); }

extern void indentIncr(void);
extern void indentDecr(void);
extern void indent(void);
void printAscii(const char *buf, unsigned len, unsigned maxLen);
void printHex(const unsigned char *buf, unsigned len, unsigned maxLen);
void printCfStr(CFStringRef cfstr);
void printCFDate(CFDateRef dateRef);
void printCfNumber(CFNumberRef cfNum);
void printResultType(CFNumberRef cfNum);
void printKeyUsage(CFNumberRef cfNum);
void printCssmErr(CFNumberRef cfNum);

/* convert an OID to a SecPolicyRef */
extern SecPolicyRef oidToPolicy(const CSSM_OID *oid);

/* convert a policy string to a SecPolicyRef */
extern SecPolicyRef oidStringToPolicy(const char *oidStr);

/* CSSM_OID --> OID string */
extern const char *oidToOidString(const CSSM_OID *oid);

/* compare OIDs; returns 1 if identical, else returns 0 */
extern int compareOids(const CSSM_OID *oid1, const CSSM_OID *oid2);

/* app path string to SecTrustedApplicationRef */
extern SecTrustedApplicationRef appPathToAppRef(const char *appPath);

/* read a file --> SecCertificateRef */
int readCertFile(const char *fileName,	SecCertificateRef *certRef);

/* policy string --> CSSM_OID */
const CSSM_OID *policyStringToOid(const char *policy);

#ifdef __cplusplus
}
#endif

#endif /* _TRUSTED_CERT_UTILS_H_ */
