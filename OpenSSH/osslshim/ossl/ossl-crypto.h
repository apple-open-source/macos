/*
 * Copyright (c) 2011-12 Apple Inc. All Rights Reserved.
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
#ifndef _OSSL_CRYPTO_H_
#define _OSSL_CRYPTO_H_

/* symbol renaming */
#define OpenSSL_add_all_algorithms		ossl_OpenSSL_add_all_algorithms
#define OpenSSL_add_all_algorithms_conf		ossl_OpenSSL_add_all_algorithms_conf
#define SSLeay					ossl_SSLeay
#define SSLeay_version				ossl_SSLeay_version


#define OPENSSL_VERSION_TEXT			"OSSLShim 0.9.8r 8 Dec 2011"
#define OPENSSL_VERSION_NUMBER			0x0090812fL
#define SSLEAY_VERSION_NUMBER			OPENSSL_VERSION_NUMBER
#define SSLEAY_VERSION				0

void OpenSSL_add_all_algorithms(void);
void OpenSSL_add_all_algorithms_conf(void);
void OpenSSL_add_all_algorithms_noconf(void);
long SSLeay(void);
const char *SSLeay_version(int t);

#endif  /* _OSSL_CRYPTO_H_ */
