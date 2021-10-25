/*
 * ntfs_sfm.c - Services For Macintosh (SFM) associated code.
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

#include "ntfs_endian.h"
#include "ntfs_sfm.h"
#include "ntfs_types.h"
#include "ntfs_unistr.h"
#include "ntfs_volume.h"

/*
 * A zerofilled finder info structure for fast checking of the finder info
 * being zero.
 */
const FINDER_INFO ntfs_empty_finder_info;

/*
 * These are the names used for the various named streams used by Services For
 * Macintosh (SFM) and the OS X SMB implementation and thus also by the OS X
 * NTFS driver.
 *
 * The following names are defined:
 *
 * 	AFP_AfpInfo
 * 	AFP_DeskTop
 * 	AFP_IdIndex
 * 	AFP_Resource
 * 	Comments
 *
 * See ntfs_sfm.h for further details.
 */

ntfschar NTFS_SFM_AFPINFO_NAME[12] = { const_cpu_to_le16('A'),
		const_cpu_to_le16('F'), const_cpu_to_le16('P'),
		const_cpu_to_le16('_'), const_cpu_to_le16('A'),
		const_cpu_to_le16('f'), const_cpu_to_le16('p'),
		const_cpu_to_le16('I'), const_cpu_to_le16('n'),
		const_cpu_to_le16('f'), const_cpu_to_le16('o'), 0 };

ntfschar NTFS_SFM_DESKTOP_NAME[12] = { const_cpu_to_le16('A'),
		const_cpu_to_le16('F'), const_cpu_to_le16('P'),
		const_cpu_to_le16('_'), const_cpu_to_le16('D'),
		const_cpu_to_le16('e'), const_cpu_to_le16('s'),
		const_cpu_to_le16('k'), const_cpu_to_le16('T'),
		const_cpu_to_le16('o'), const_cpu_to_le16('p'), 0 };

ntfschar NTFS_SFM_IDINDEX_NAME[12] = { const_cpu_to_le16('A'),
		const_cpu_to_le16('F'), const_cpu_to_le16('P'),
		const_cpu_to_le16('_'), const_cpu_to_le16('I'),
		const_cpu_to_le16('d'), const_cpu_to_le16('I'),
		const_cpu_to_le16('n'), const_cpu_to_le16('d'),
		const_cpu_to_le16('e'), const_cpu_to_le16('x'), 0 };

ntfschar NTFS_SFM_RESOURCEFORK_NAME[13] = { const_cpu_to_le16('A'),
		const_cpu_to_le16('F'), const_cpu_to_le16('P'),
		const_cpu_to_le16('_'), const_cpu_to_le16('R'),
		const_cpu_to_le16('e'), const_cpu_to_le16('s'),
		const_cpu_to_le16('o'), const_cpu_to_le16('u'),
		const_cpu_to_le16('r'), const_cpu_to_le16('c'),
		const_cpu_to_le16('e'), 0 };

ntfschar NTFS_SFM_COMMENTS_NAME[9] = { const_cpu_to_le16('C'),
		const_cpu_to_le16('o'), const_cpu_to_le16('m'),
		const_cpu_to_le16('m'), const_cpu_to_le16('e'),
		const_cpu_to_le16('n'), const_cpu_to_le16('t'),
		const_cpu_to_le16('s'), 0 };

/**
 * ntfs_is_sfm_name - check if a name is a protected SFM name
 * @name:	name (in NTFS Unicode) to check
 * @len:	length of name in NTFS Unicode characters to check
 *
 * Return true if the NTFS Unicode name @name of length @len characters is a
 * Services For Macintosh (SFM) protected name and false otherwise.
 */
BOOL ntfs_is_sfm_name(ntfs_volume *vol,
		const ntfschar *name, const unsigned len)
{
	const ntfschar *upcase = vol->upcase;
	const unsigned upcase_len = vol->upcase_len;
	const BOOL case_sensitive = NVolCaseSensitive(vol);

	return (ntfs_are_names_equal(name, len, NTFS_SFM_AFPINFO_NAME, 11,
			case_sensitive, upcase, upcase_len) ||
			ntfs_are_names_equal(name, len, NTFS_SFM_DESKTOP_NAME,
			11, case_sensitive, upcase, upcase_len) ||
			ntfs_are_names_equal(name, len, NTFS_SFM_IDINDEX_NAME,
			11, case_sensitive, upcase, upcase_len) ||
			ntfs_are_names_equal(name, len,
			NTFS_SFM_RESOURCEFORK_NAME, 12, case_sensitive, upcase,
			upcase_len) ||
			ntfs_are_names_equal(name, len, NTFS_SFM_COMMENTS_NAME,
			8, case_sensitive, upcase, upcase_len));
}
