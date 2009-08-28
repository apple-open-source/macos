/*
 * ntfs_unistr.h - Defines for Unicode string handling in the NTFS kernel
 *		   driver.
 *
 * Copyright (c) 2006-2008 Anton Altaparmakov.  All Rights Reserved.
 * Portions Copyright (c) 2006-2008 Apple Inc.  All Rights Reserved.
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

#ifndef _OSX_NTFS_UNISTR_H
#define _OSX_NTFS_UNISTR_H

#include "ntfs_layout.h"
#include "ntfs_types.h"
#include "ntfs_volume.h"

__private_extern__ BOOL ntfs_are_names_equal(const ntfschar *s1, size_t s1_len,
		const ntfschar *s2, size_t s2_len, const BOOL case_sensitive,
		const ntfschar *upcase, const u32 upcase_len);

__private_extern__ int ntfs_collate_names(const ntfschar *name1,
		const u32 name1_len, const ntfschar *name2,
		const u32 name2_len, const int err_val,
		const BOOL case_sensitive, const ntfschar *upcase,
		const u32 upcase_len);

__private_extern__ int ntfs_ucsncmp(const ntfschar *s1, const ntfschar *s2,
		size_t n);
__private_extern__ int ntfs_ucsncasecmp(const ntfschar *s1, const ntfschar *s2,
		size_t n, const ntfschar *upcase, const u32 upcase_size);

__private_extern__ void ntfs_upcase_name(ntfschar *name, u32 name_len,
		const ntfschar *upcase, const u32 upcase_len);

static inline void ntfs_file_upcase_value(FILENAME_ATTR *filename_attr,
		const ntfschar *upcase, const u32 upcase_len)
{
	ntfs_upcase_name((ntfschar*)&filename_attr->filename,
			filename_attr->filename_length, upcase, upcase_len);
}

static inline int ntfs_file_compare_values(FILENAME_ATTR *filename_attr1,
		FILENAME_ATTR *filename_attr2, const int err_val,
		const BOOL case_sensitive, const ntfschar *upcase,
		const u32 upcase_len)
{
	return ntfs_collate_names((ntfschar*)&filename_attr1->filename,
			filename_attr1->filename_length,
			(ntfschar*)&filename_attr2->filename,
			filename_attr2->filename_length,
			err_val, case_sensitive, upcase, upcase_len);
}

__private_extern__ signed ntfs_to_utf8(const ntfs_volume *vol,
		const ntfschar *ins, const size_t ins_size,
		u8 **outs, size_t *outs_size);

__private_extern__ signed utf8_to_ntfs(const ntfs_volume *vol, const u8 *ins,
		const size_t ins_size, ntfschar **outs, size_t *outs_size);

__private_extern__ void ntfs_upcase_table_generate(ntfschar *uc, int uc_size);

#endif /* !_OSX_NTFS_UNISTR_H */
