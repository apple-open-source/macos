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
/* This is a generated file */
#ifndef __asn1_der_private_h__
#define __asn1_der_private_h__

#include <stdarg.h>

int
_asn1_bmember_isset_bit (
	const void */*data*/,
	unsigned int /*bit*/,
	size_t /*size*/);

void
_asn1_bmember_put_bit (
	unsigned char */*p*/,
	const void */*data*/,
	unsigned int /*bit*/,
	size_t /*size*/,
	unsigned int */*bitset*/);

int
_asn1_copy (
	const struct asn1_template */*t*/,
	const void */*from*/,
	void */*to*/);

int
_asn1_copy_top (
	const struct asn1_template */*t*/,
	const void */*from*/,
	void */*to*/);

int
_asn1_decode (
	const struct asn1_template */*t*/,
	unsigned /*flags*/,
	const unsigned char */*p*/,
	size_t /*len*/,
	void */*data*/,
	size_t */*size*/);

int
_asn1_decode_top (
	const struct asn1_template */*t*/,
	unsigned /*flags*/,
	const unsigned char */*p*/,
	size_t /*len*/,
	void */*data*/,
	size_t */*size*/);

int
_asn1_encode (
	const struct asn1_template */*t*/,
	unsigned char */*p*/,
	size_t /*len*/,
	const void */*data*/,
	size_t */*size*/);

int
_asn1_encode_fuzzer (
	const struct asn1_template */*t*/,
	unsigned char */*p*/,
	size_t /*len*/,
	const void */*data*/,
	size_t */*size*/);

void
_asn1_free (
	const struct asn1_template */*t*/,
	void */*data*/);

size_t
_asn1_length (
	const struct asn1_template */*t*/,
	const void */*data*/);

size_t
_asn1_length_fuzzer (
	const struct asn1_template */*t*/,
	const void */*data*/);

size_t
_asn1_sizeofType (const struct asn1_template */*t*/);

time_t
_der_timegm (struct tm */*tm*/);

int
_heim_der_set_sort (
	const void */*a1*/,
	const void */*a2*/);

int
_heim_fix_dce (
	size_t /*reallen*/,
	size_t */*len*/);

size_t
_heim_len_int (int /*val*/);

size_t
_heim_len_unsigned (unsigned /*val*/);

int
_heim_time2generalizedtime (
	time_t /*t*/,
	heim_octet_string */*s*/,
	int /*gtimep*/);

#endif /* __asn1_der_private_h__ */
