/*
 * ntfs_types.h - Defines for NTFS kernel driver specific types.
 *
 * Copyright (c) 2006, 2007 Anton Altaparmakov.  All Rights Reserved.
 * Portions Copyright (c) 2006, 2007 Apple Inc.  All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution. 
 * 3. Neither the name of Apple Inc. ("Apple") nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ALTERNATIVELY, provided that this notice and licensing terms are retained in
 * full, this file may be redistributed and/or modified under the terms of the
 * GNU General Public License (GPL) Version 2, in which case the provisions of
 * that version of the GPL will apply to you instead of the license terms
 * above.  You can obtain a copy of the GPL Version 2 at
 * http://developer.apple.com/opensource/licenses/gpl-2.txt.
 */

#ifndef _OSX_NTFS_TYPES_H
#define _OSX_NTFS_TYPES_H

#include <sys/types.h>

#include <mach/boolean.h>

/* Define our fixed size types. */
typedef u_int8_t u8;
typedef u_int16_t u16;
typedef u_int32_t u32;
typedef u_int64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

/*
 * Define our fixed size, little-endian types.  Note we define the signed types
 * to be unsigned so we do not get sign extension on endianness conversions.
 * We do not bother with eight-bit, little-endian types as endianness does not
 * apply for eight-bit types.
 */
typedef u_int16_t le16;
typedef u_int32_t le32;
typedef u_int64_t le64;
typedef u_int16_t sle16;
typedef u_int32_t sle32;
typedef u_int64_t sle64;

/*
 * Define our fixed size, big-endian types.  Note we define the signed types to
 * be unsigned so we do not get sign extension on endianness conversions.  We
 * do not bother with eight-bit, big-endian types as endianness does not apply
 * for eight-bit types.
 */
typedef u_int16_t be16;
typedef u_int32_t be32;
typedef u_int64_t be64;
typedef u_int16_t sbe16;
typedef u_int32_t sbe32;
typedef u_int64_t sbe64;

/* 2-byte Unicode character type. */
typedef le16 ntfschar;
#define NTFSCHAR_SIZE_SHIFT 1

/*
 * Clusters are signed 64-bit values on NTFS volumes.  We define two types, LCN
 * and VCN, to allow for type checking and better code readability.
 */
typedef s64 VCN;
typedef sle64 leVCN;
typedef s64 LCN;
typedef sle64 leLCN;

/*
 * The NTFS journal $LogFile uses log sequence numbers which are signed 64-bit
 * values.  We define our own type LSN, to allow for type checking and better
 * code readability.
 */
typedef s64 LSN;
typedef sle64 leLSN;

/*
 * The NTFS transaction log $UsnJrnl uses usns which are signed 64-bit values.
 * We define our own type USN, to allow for type checking and better code
 * readability.
 */
typedef s64 USN;
typedef sle64 leUSN;

/* Our boolean type. */
typedef boolean_t BOOL;

typedef enum {
	CASE_SENSITIVE = 0,
	IGNORE_CASE = 1,
} IGNORE_CASE_BOOL;

#endif /* !_OSX_NTFS_TYPES_H */
