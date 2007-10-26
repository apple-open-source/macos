/*
 * ntfs_endian.h - Defines for endianness handling in NTFS kernel driver.
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

#ifndef _OSX_NTFS_ENDIAN_H
#define _OSX_NTFS_ENDIAN_H

#include <libkern/OSByteOrder.h>

#include "ntfs_types.h"

/*
 * Unigned endianness conversion functions.
 */

static inline u16 le16_to_cpu(le16 x)
{
	return (u16)(OSSwapLittleToHostInt16(x));
}

static inline u32 le32_to_cpu(le32 x)
{
	return (u32)(OSSwapLittleToHostInt32(x));
}

static inline u64 le64_to_cpu(le64 x)
{
	return (u64)(OSSwapLittleToHostInt64(x));
}

static inline u16 le16_to_cpup(le16 *x)
{
	return le16_to_cpu(*x);
}

static inline u32 le32_to_cpup(le32 *x)
{
	return le32_to_cpu(*x);
}

static inline u64 le64_to_cpup(le64 *x)
{
	return le64_to_cpu(*x);
}

static inline le16 cpu_to_le16(u16 x)
{
	return (le16)(OSSwapHostToLittleInt16(x));
}

static inline le32 cpu_to_le32(u32 x)
{
	return (le32)(OSSwapHostToLittleInt32(x));
}

static inline le64 cpu_to_le64(u64 x)
{
	return (le64)(OSSwapHostToLittleInt64(x));
}

static inline le16 cpu_to_le16p(u16 *x)
{
	return cpu_to_le16(*x);
}

static inline le32 cpu_to_le32p(u32 *x)
{
	return cpu_to_le32(*x);
}

static inline le64 cpu_to_le64p(u64 *x)
{
	return cpu_to_le64(*x);
}

/*
 * Signed endianness conversion functions.
 */

static inline s16 sle16_to_cpu(sle16 x)
{
	return (s16)le16_to_cpu((le16)x);
}

static inline s32 sle32_to_cpu(sle32 x)
{
	return (s32)le32_to_cpu((le32)x);
}

static inline s64 sle64_to_cpu(sle64 x)
{
	return (s64)le64_to_cpu((le64)x);
}

static inline s16 sle16_to_cpup(sle16 *x)
{
	return (s16)le16_to_cpu(*(le16*)x);
}

static inline s32 sle32_to_cpup(sle32 *x)
{
	return (s32)le32_to_cpu(*(le32*)x);
}

static inline s64 sle64_to_cpup(sle64 *x)
{
	return (s64)le64_to_cpu(*(le64*)x);
}

static inline sle16 cpu_to_sle16(s16 x)
{
	return (sle16)cpu_to_le16((u16)x);
}

static inline sle32 cpu_to_sle32(s32 x)
{
	return (sle32)cpu_to_le32((u32)x);
}

static inline sle64 cpu_to_sle64(s64 x)
{
	return (sle64)cpu_to_le64((u64)x);
}

static inline sle16 cpu_to_sle16p(s16 *x)
{
	return (sle16)cpu_to_le16(*(u16*)x);
}

static inline sle32 cpu_to_sle32p(s32 *x)
{
	return (sle32)cpu_to_le32(*(u32*)x);
}

static inline sle64 cpu_to_sle64p(s64 *x)
{
	return (sle64)cpu_to_le64(*(u64*)x);
}

/*
 * Constant endianness conversion defines.
 */

#define const_le16_to_cpu(x) ((u16)(OSSwapLittleToHostConstInt16(((u16)(x)))))
#define const_le32_to_cpu(x) ((u32)(OSSwapLittleToHostConstInt32(((u32)(x)))))
#define const_le64_to_cpu(x) ((u64)(OSSwapLittleToHostConstInt64(((u64)(x)))))

#define const_cpu_to_le16(x) ((le16)(OSSwapHostToLittleConstInt16(((u16)(x)))))
#define const_cpu_to_le32(x) ((le32)(OSSwapHostToLittleConstInt32(((u32)(x)))))
#define const_cpu_to_le64(x) ((le64)(OSSwapHostToLittleConstInt64(((u64)(x)))))

#endif /* !_OSX_NTFS_ENDIAN_H */
