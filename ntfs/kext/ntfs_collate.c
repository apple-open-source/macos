/*
 * ntfs_collate.c - NTFS kernel collation handling.
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

#include <kern/debug.h>

#include <string.h>

#include "ntfs_collate.h"
#include "ntfs_debug.h"
#include "ntfs_endian.h"
#include "ntfs_layout.h"
#include "ntfs_unistr.h"
#include "ntfs_volume.h"

/**
 * ntfs_collate_binary - byte by byte binary collation
 *
 * Used for COLLATION_BINARY and COLLATION_NTOFS_SID.
 */
static int ntfs_collate_binary(ntfs_volume *vol __attribute__((unused)),
		const void *data1, const int data1_len,
		const void *data2, const int data2_len)
{
	int len, rc;

	ntfs_debug("Entering.");
	len = data1_len;
	if (data2_len < data1_len)
		len = data2_len;
	rc = memcmp(data1, data2, len);
	if (!rc && (data1_len != data2_len)) {
		/*
		 * If @len equals @data1_len this implies that @data1_len is
		 * smaller than @data2_len.
		 */
		if (len == data1_len)
			rc = -1;
		else
			rc = 1;
	}
	ntfs_debug("Done (returning %d).", rc);
	return rc;
}

/**
 * ntfs_collate_filename - filename collation
 *
 * Used for COLLATION_FILENAME.
 *
 * Note: This only performs exact matching as it is only intended to be used
 * when looking up a particular name that is already known to exist and we just
 * want to locate the correct index entry for it so that we can modify/delete
 * it.  Alternatively, we want to add a new name and we already know that it
 * does not exist in the index so we just want to locate the correct index
 * entry in front of which we need to insert the name.
 */
static int ntfs_collate_filename(ntfs_volume *vol,
		const void *data1, const int data1_len,
		const void *data2, const int data2_len)
{
	const FILENAME_ATTR *fn1 = data1;
	const FILENAME_ATTR *fn2 = data2;
	int rc;

	ntfs_debug("Entering.");
	if (data1_len < (int)sizeof(FILENAME_ATTR))
		panic("%s(): data1_len < sizeof(FILENAME_ATTR)\n",
				__FUNCTION__);
	if (data2_len < (int)sizeof(FILENAME_ATTR))
		panic("%s(): data2_len < sizeof(FILENAME_ATTR)\n",
				__FUNCTION__);
	/*
	 * We only care about exact matches, however in order to do proper
	 * collation we need to do the case insensitive check first because the
	 * upcased characters will not necessarily collate in the same order as
	 * the non-upcased ones.
	 */
	rc = ntfs_collate_names(fn1->filename,
			fn1->filename_length, fn2->filename,
			fn2->filename_length, 1, FALSE, vol->upcase,
			vol->upcase_len);
	if (!rc)
		rc = ntfs_collate_names(fn1->filename,
				fn1->filename_length, fn2->filename,
				fn2->filename_length, 1, TRUE,
				vol->upcase, vol->upcase_len);
	ntfs_debug("Done (returning %d).", rc);
	return rc;
}

/**
 * ntfs_collate_ntofs_ulongs - le32 by le32 collation
 *
 * Used for COLLATION_NTOFS_ULONG, COLLATION_NTOFS_ULONGS, and
 * COLLATION_NTOFS_SECURITY_HASH.
 */
static int ntfs_collate_ntofs_ulongs(ntfs_volume *vol __attribute__((unused)),
		const void *data1, const int data1_len,
		const void *data2, const int data2_len)
{
	const le32 *p1 = data1;
	const le32 *p2 = data2;
	int min_len, i, rc;

	ntfs_debug("Entering.");
	if (data1_len & (sizeof(u32) - 1))
		panic("%s(): data1_len & (sizeof(u32) - 1)\n", __FUNCTION__);
	if (data2_len & (sizeof(u32) - 1))
		panic("%s(): data2_len & (sizeof(u32) - 1)\n", __FUNCTION__);
	min_len = data1_len;
	if (min_len > data2_len)
		min_len = data2_len;
	min_len >>= 2;
	for (i = 0; i < min_len; i++) {
		const u32 u1 = le32_to_cpu(p1[i]);
		const u32 u2 = le32_to_cpu(p2[i]);
		if (u1 > u2) {
			rc = 1;
			goto out;
		}
		if (u1 < u2) {
			rc = -1;
			goto out;
		}
	}
	rc = 1;
	if (data1_len < data2_len)
		rc = -1;
	else if (data1_len == data2_len)
		rc = 0;
out:
	ntfs_debug("Done (returning %d).", rc);
	return rc;
}

typedef int (*ntfs_collate_func_t)(ntfs_volume *, const void *, const int,
		const void *, const int);

static ntfs_collate_func_t ntfs_do_collate0x0[3] = {
	ntfs_collate_binary,		/* COLLATION_BINARY */
	ntfs_collate_filename,		/* COLLATION_FILENAME */
	NULL,				/* COLLATION_UNICODE_STRING */
};

static ntfs_collate_func_t ntfs_do_collate0x1[4] = {
	ntfs_collate_ntofs_ulongs,	/* COLLATION_NTOFS_ULONG */
	ntfs_collate_binary,		/* COLLATION_NTOFS_SID */
	ntfs_collate_ntofs_ulongs,	/* COLLATION_NTOFS_SECURITY_HASH */
	ntfs_collate_ntofs_ulongs,	/* COLLATION_NTOFS_ULONGS */
};

/**
 * ntfs_collate - collate two data items using a specified collation rule
 * @vol:	ntfs volume to which the data items belong
 * @cr:		collation rule to use when comparing the items
 * @data1:	first data item to collate
 * @data1_len:	length in bytes of @data1
 * @data2:	second data item to collate
 * @data2_len:	length in bytes of @data2
 *
 * Collate the two data items @data1 and @data2 using the collation rule @cr
 * and return -1, 0, ir 1 if @data1 is found, respectively, to collate before,
 * to match, or to collate after @data2.
 *
 * For speed we use the collation rule @cr as an index into two tables of
 * function pointers to call the appropriate collation function.
 */
int ntfs_collate(ntfs_volume *vol, COLLATION_RULE cr,
		const void *data1, const int data1_len,
		const void *data2, const int data2_len) {
	int i;

	i = le32_to_cpu(cr);
	ntfs_debug("Entering (collation rule 0x%x, data1_len 0x%x, data2_len "
			"0x%x).", i, data1_len, data2_len);
	/*
	 * TODO: At the moment we do not support COLLATION_UNICODE_STRING so we
	 * BUG() for it.
	 */
	if (cr == COLLATION_UNICODE_STRING)
		panic("%s(): cr == COLLATION_UNICODE_STRING\n", __FUNCTION__);
	if (i < 0)
		panic("%s(): i < 0\n", __FUNCTION__);
	if (i <= 0x02)
		return ntfs_do_collate0x0[i](vol, data1, data1_len,
				data2, data2_len);
	if (i < 0x10)
		panic("%s(): i < 0x10\n", __FUNCTION__);
	i -= 0x10;
	if (i <= 3)
		return ntfs_do_collate0x1[i](vol, data1, data1_len,
				data2, data2_len);
	panic("%s(): i > 3\n", __FUNCTION__);
	return 0;
}
