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
/* Generated from lib/asn1/asn1_err.et */
/* $Id$ */

#ifndef __asn1_err_h__
#define __asn1_err_h__

struct et_list;

void initialize_asn1_error_table_r(struct et_list **);

void initialize_asn1_error_table(void);
#define init_asn1_err_tbl initialize_asn1_error_table

typedef enum asn1_error_number{
	ASN1_BAD_TIMEFORMAT = 1859794432,
	ASN1_MISSING_FIELD = 1859794433,
	ASN1_MISPLACED_FIELD = 1859794434,
	ASN1_TYPE_MISMATCH = 1859794435,
	ASN1_OVERFLOW = 1859794436,
	ASN1_OVERRUN = 1859794437,
	ASN1_BAD_ID = 1859794438,
	ASN1_BAD_LENGTH = 1859794439,
	ASN1_BAD_FORMAT = 1859794440,
	ASN1_PARSE_ERROR = 1859794441,
	ASN1_EXTRA_DATA = 1859794442,
	ASN1_BAD_CHARACTER = 1859794443,
	ASN1_MIN_CONSTRAINT = 1859794444,
	ASN1_MAX_CONSTRAINT = 1859794445,
	ASN1_EXACT_CONSTRAINT = 1859794446,
	ASN1_INDEF_OVERRUN = 1859794447,
	ASN1_INDEF_UNDERRUN = 1859794448,
	ASN1_GOT_BER = 1859794449,
	ASN1_INDEF_EXTRA_DATA = 1859794450
} asn1_error_number;

#define ERROR_TABLE_BASE_asn1 1859794432

#define COM_ERR_BINDDOMAIN_asn1 "heim_com_err1859794432"

#endif /* __asn1_err_h__ */
