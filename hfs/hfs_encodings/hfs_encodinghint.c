/*
 * Copyright (c) 2001-2015 Apple Inc. All rights reserved.
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

#include <sys/param.h>
#include "../core/hfs.h"


/* CJK Mac Encoding Bits */
#define CJK_JAPAN	        0x1
#define CJK_KOREAN	        0x2
#define CJK_CHINESE_TRAD	0x4
#define CJK_CHINESE_SIMP	0x8
#define CJK_ALL	            0xF

#define CJK_CHINESE    (CJK_CHINESE_TRAD | CJK_CHINESE_SIMP)
#define CJK_KATAKANA   (CJK_JAPAN)


/* Remember the last unique CJK bit */
u_int8_t cjk_lastunique = 0;

/* Encoding bias */
u_int32_t hfs_encodingbias = 0;
int hfs_islatinbias = 0;

extern lck_mtx_t  encodinglst_mutex;


/* Map CJK bits to Mac encoding */
u_int8_t cjk_encoding[] = {
	/* 0000 */  kTextEncodingMacUnicode,
	/* 0001 */  kTextEncodingMacJapanese,
	/* 0010 */  kTextEncodingMacKorean,
	/* 0011 */  kTextEncodingMacJapanese,
	/* 0100 */  kTextEncodingMacChineseTrad,
	/* 0101 */  kTextEncodingMacJapanese,
	/* 0110 */  kTextEncodingMacKorean,
	/* 0111 */  kTextEncodingMacJapanese,
	/* 1000 */  kTextEncodingMacChineseSimp,
	/* 1001 */  kTextEncodingMacJapanese,
	/* 1010 */  kTextEncodingMacKorean,
	/* 1011 */  kTextEncodingMacJapanese,
	/* 1100 */  kTextEncodingMacChineseTrad,
	/* 1101 */  kTextEncodingMacJapanese,
	/* 1110 */  kTextEncodingMacKorean,
	/* 1111 */  kTextEncodingMacJapanese
};


u_int32_t
hfs_pickencoding(__unused const u_int16_t *src, __unused int len) {
	/* Just return kTextEncodingMacRoman if HFS standard is not supported. */
	return kTextEncodingMacRoman;
}


u_int32_t hfs_getencodingbias(void)
{
	return (hfs_encodingbias);
}


void hfs_setencodingbias(u_int32_t bias)
{
	hfs_encodingbias = bias;

	switch (bias) {
	case kTextEncodingMacRoman:
	case kTextEncodingMacCentralEurRoman:
	case kTextEncodingMacTurkish:
	case kTextEncodingMacCroatian:
	case kTextEncodingMacIcelandic:
	case kTextEncodingMacRomanian:
		hfs_islatinbias = 1;
		break;
	default:
		hfs_islatinbias = 0;
		break;					
	}
}
