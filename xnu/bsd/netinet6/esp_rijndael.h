/*
 * Copyright (c) 2008, 2021-2023 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

/*	$FreeBSD: src/sys/netinet6/esp_rijndael.h,v 1.1.2.1 2001/07/03 11:01:50 ume Exp $	*/
/*	$KAME: esp_rijndael.h,v 1.1 2000/09/20 18:15:22 itojun Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <sys/appleapiopts.h>

#ifdef BSD_KERNEL_PRIVATE
size_t esp_aes_schedlen(const struct esp_algorithm *);
int esp_aes_schedule(const struct esp_algorithm *, struct secasvar *);
int esp_cbc_decrypt_aes(struct mbuf *, size_t, struct secasvar *,
    const struct esp_algorithm *, int);
int
    esp_cbc_encrypt_aes(struct mbuf *, size_t, size_t, struct secasvar *,
    const struct esp_algorithm *, int);
int esp_aes_cbc_encrypt_data(struct secasvar *,
    uint8_t *__sized_by(input_data_len), size_t input_data_len,
    struct newesp *,
    uint8_t *__sized_by(out_ivlen), size_t out_ivlen,
    uint8_t *__sized_by(output_data_len), size_t output_data_len);
int esp_aes_cbc_decrypt_data(struct secasvar *,
    uint8_t *__sized_by(input_data_len), size_t input_data_len,
    struct newesp *,
    uint8_t *__sized_by(ivlen), size_t ivlen,
    uint8_t *__sized_by(output_data_len), size_t output_data_len);


size_t esp_gcm_schedlen(const struct esp_algorithm *);
int esp_gcm_schedule(const struct esp_algorithm *, struct secasvar *);
int esp_gcm_ivlen(const struct esp_algorithm *, struct secasvar *);
int esp_gcm_encrypt_aes(struct mbuf *, size_t, size_t, struct secasvar *, const struct esp_algorithm *, int);
int esp_gcm_decrypt_aes(struct mbuf *, size_t, struct secasvar *, const struct esp_algorithm *, int);
int esp_gcm_encrypt_finalize(struct secasvar *, unsigned char *, size_t);
int esp_gcm_decrypt_finalize(struct secasvar *, unsigned char *, size_t);
int esp_aes_gcm_encrypt_data(struct secasvar *,
    uint8_t *__sized_by(input_data_len), size_t input_data_len,
    struct newesp *,
    uint8_t *__sized_by(ivlen), size_t ivlen,
    uint8_t *__sized_by(output_data_len), size_t output_data_len);
int esp_aes_gcm_decrypt_data(struct secasvar *,
    uint8_t *__sized_by(input_data_len), size_t input_data_len,
    struct newesp *,
    uint8_t *__sized_by(ivlen), size_t ivlen,
    uint8_t *__sized_by(output_data_len), size_t output_data_len);
#endif /* BSD_KERNEL_PRIVATE */
