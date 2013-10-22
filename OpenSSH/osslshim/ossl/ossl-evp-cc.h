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

/*
 * Copyright (c) 2009 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _OSSL_EVP_CC_H_
#define _OSSL_EVP_CC_H_			1

/* symbol renaming */
#define EVP_cc_md2			ossl_EVP_cc_md2
#define EVP_cc_md4			ossl_EVP_cc_md4
#define EVP_cc_md5			ossl_EVP_cc_md5
#define EVP_cc_sha1			ossl_EVP_cc_sha1
#define EVP_cc_sha224			ossl_EVP_cc_sha224
#define EVP_cc_sha256			ossl_EVP_cc_sha256
#define EVP_cc_sha384			ossl_EVP_cc_sha384
#define EVP_cc_sha512			ossl_EVP_cc_sha512
#define EVP_cc_rmd128			ossl_EVP_cc_rmd128
#define EVP_cc_rmd160			ossl_EVP_cc_rmd160
#define EVP_cc_rmd256			ossl_EVP_cc_rmd256
#define EVP_cc_rmd320			ossl_EVP_cc_rmd320
#define EVP_cc_des_cbc			ossl_EVP_cc_des_cbc
#define EVP_cc_des_ecb			ossl_EVP_cc_des_ecb
#define EVP_cc_des_ede3_cbc		ossl_EVP_cc_des_ede3_cbc
#define EVP_cc_des_ede3_ecb		ossl_EVP_cc_des_ede3_ecb
#define EVP_cc_aes_128_cbc		ossl_EVP_cc_aes_128_cbc
#define EVP_cc_aes_128_ecb		ossl_EVP_cc_aes_128_ecb
#define EVP_cc_aes_192_cbc		ossl_EVP_cc_aes_192_cbc
#define EVP_cc_aes_192_ecb		ossl_EVP_cc_aes_192_ecb
#define EVP_cc_aes_256_cbc		ossl_EVP_cc_aes_256_cbc
#define EVP_cc_aes_256_ecb		ossl_EVP_cc_aes_256_ecb
#define EVP_cc_aes_128_cfb8		ossl_EVP_cc_aes_128_cfb8
#define EVP_cc_aes_192_cfb8		ossl_EVP_cc_aes_192_cfb8
#define EVP_cc_aes_256_cfb8		ossl_EVP_cc_aes_256_cfb8
#define EVP_cc_rc4			ossl_EVP_cc_rc4
#define EVP_cc_rc4_40			ossl_EVP_cc_rc4_40
#define EVP_cc_rc2_40_cbc		ossl_EVP_cc_rc2_40_cbc
#define EVP_cc_rc2_64_cbc		ossl_EVP_cc_rc2_64_cbc
#define EVP_cc_rc2_cbc			ossl_EVP_cc_rc2_cbc
#define EVP_cc_camellia_128_cbc		ossl_EVP_cc_camellia_128_cbc
#define EVP_cc_camellia_192_cbc		ossl_EVP_cc_camellia_192_cbc
#define EVP_cc_camellia_256_cbc		ossl_EVP_cc_camellia_256_cbc
#define EVP_cc_bf_cbc			ossl_EVP_cc_bf_cbc
#define EVP_cc_bf_ecb			ossl_EVP_cc_bf_ecb
#define EVP_cc_cast5_cbc		ossl_EVP_cc_cast5_cbc
#define EVP_cc_cast5_ecb		ossl_EVP_cc_cast5_ecb

/*
 *
 */


const EVP_MD *EVP_cc_md2(void);
const EVP_MD *EVP_cc_md4(void);
const EVP_MD *EVP_cc_md5(void);
const EVP_MD *EVP_cc_sha1(void);
const EVP_MD *EVP_cc_sha224(void);
const EVP_MD *EVP_cc_sha256(void);
const EVP_MD *EVP_cc_sha384(void);
const EVP_MD *EVP_cc_sha512(void);
const EVP_MD *EVP_cc_rmd128(void);
const EVP_MD *EVP_cc_rmd160(void);
const EVP_MD *EVP_cc_rmd256(void);
const EVP_MD *EVP_cc_rmd320(void);

const EVP_CIPHER *EVP_cc_bf_cbc(void);
const EVP_CIPHER *EVP_cc_bf_ecb(void);

const EVP_CIPHER *EVP_cc_cast5_cbc(void);
const EVP_CIPHER *EVP_cc_cast5_ecb(void);

const EVP_CIPHER *EVP_cc_rc2_cbc(void);
const EVP_CIPHER *EVP_cc_rc2_40_cbc(void);
const EVP_CIPHER *EVP_cc_rc2_64_cbc(void);

const EVP_CIPHER *EVP_cc_rc4(void);
const EVP_CIPHER *EVP_cc_rc4_40(void);

const EVP_CIPHER *EVP_cc_des_cbc(void);
const EVP_CIPHER *EVP_cc_des_ecb(void);
const EVP_CIPHER *EVP_cc_des_ede3_cbc(void);
const EVP_CIPHER *EVP_cc_des_ede3_ecb(void);

const EVP_CIPHER *EVP_cc_aes_128_cbc(void);
const EVP_CIPHER *EVP_cc_aes_128_ecb(void);
const EVP_CIPHER *EVP_cc_aes_192_cbc(void);
const EVP_CIPHER *EVP_cc_aes_192_ecb(void);
const EVP_CIPHER *EVP_cc_aes_256_cbc(void);
const EVP_CIPHER *EVP_cc_aes_256_ecb(void);

const EVP_CIPHER *EVP_cc_aes_128_cfb8(void);
const EVP_CIPHER *EVP_cc_aes_192_cfb8(void);
const EVP_CIPHER *EVP_cc_aes_256_cfb8(void);

const EVP_CIPHER *EVP_cc_camellia_128_cbc(void);
const EVP_CIPHER *EVP_cc_camellia_192_cbc(void);
const EVP_CIPHER *EVP_cc_camellia_256_cbc(void);

#endif /* _OSSL_EVP_CC_H_ */
