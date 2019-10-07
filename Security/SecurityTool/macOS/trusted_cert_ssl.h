/*
 * Copyright (c) 2019 Apple Inc. All Rights Reserved.
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
 * trusted_cert_ssl.h
 */

#ifndef _TRUSTED_CERT_SSL_H_
#define _TRUSTED_CERT_SSL_H_  1

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecCertificate.h>
#include <Security/SecTrust.h>

#ifdef __cplusplus
extern "C" {
#endif

void printErrorDetails(SecTrustRef trust);
void printExtendedResults(SecTrustRef trust);

int evaluate_ssl(const char *urlstr, int verbose, SecTrustRef * CF_RETURNS_RETAINED trustRef);

CF_RETURNS_RETAINED CFStringRef CopyCertificateTextRepresentation(SecCertificateRef certificate);

#ifdef __cplusplus
}
#endif

#endif /* _TRUSTED_CERT_SSL_H_ */

