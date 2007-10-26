/*
 * ntfs_unistr.c - NTFS kernel Unicode string operations.
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

#include <sys/types.h>
#include <sys/utfconv.h>

#include <libkern/OSMalloc.h>

#include "ntfs_debug.h"
#include "ntfs_endian.h"
#include "ntfs_types.h"
#include "ntfs_unistr.h"
#include "ntfs_volume.h"

/*
 * IMPORTANT
 * =========
 *
 * All these routines assume that the Unicode characters are in little endian
 * encoding inside the strings!!!
 */

/*
 * This is used by the name collation functions to quickly determine what
 * characters are (in)valid.
 */
static const u8 ntfs_legal_ansi_char_array[0x40] = {
	0x00, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
	0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,

	0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
	0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,

	0x17, 0x07, 0x18, 0x17, 0x17, 0x17, 0x17, 0x17,
	0x17, 0x17, 0x18, 0x16, 0x16, 0x17, 0x07, 0x00,

	0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17,
	0x17, 0x17, 0x04, 0x16, 0x18, 0x16, 0x18, 0x18,
};

/**
 * ntfs_are_names_equal - compare two Unicode names for equality
 * @s1:			name to compare to @s2
 * @s1_len:		length in Unicode characters of @s1
 * @s2:			name to compare to @s1
 * @s2_len:		length in Unicode characters of @s2
 * @ic:			ignore case bool
 * @upcase:		upcase table (only if @ic == IGNORE_CASE)
 * @upcase_len:		length in Unicode characters of @upcase (if present)
 *
 * Compare the names @s1 and @s2 and return TRUE (1) if the names are
 * identical, or FALSE (0) if they are not identical.  If @ic is IGNORE_CASE,
 * the @upcase table is used to performa a case insensitive comparison.
 */
BOOL ntfs_are_names_equal(const ntfschar *s1, size_t s1_len,
		const ntfschar *s2, size_t s2_len, const IGNORE_CASE_BOOL ic,
		const ntfschar *upcase, const u32 upcase_len)
{
	if (s1_len != s2_len)
		return FALSE;
	if (ic == CASE_SENSITIVE)
		return !ntfs_ucsncmp(s1, s2, s1_len);
	return !ntfs_ucsncasecmp(s1, s2, s1_len, upcase, upcase_len);
}

/**
 * ntfs_collate_names - collate two Unicode names
 * @name1:	first Unicode name to compare
 * @name2:	second Unicode name to compare
 * @err_val:	if @name1 contains an invalid character return this value
 * @ic:		either CASE_SENSITIVE or IGNORE_CASE
 * @upcase:	upcase table (ignored if @ic is CASE_SENSITIVE)
 * @upcase_len:	upcase table length (ignored if @ic is CASE_SENSITIVE)
 *
 * ntfs_collate_names collates two Unicode names and returns:
 *
 *  -1 if the first name collates before the second one,
 *   0 if the names match,
 *   1 if the second name collates before the first one, or
 * @err_val if an invalid character is found in @name1 during the comparison.
 *
 * The following characters are considered invalid: '"', '*', '<', '>' and '?'.
 */
int ntfs_collate_names(const ntfschar *name1, const u32 name1_len,
		const ntfschar *name2, const u32 name2_len, const int err_val,
		const IGNORE_CASE_BOOL ic, const ntfschar *upcase,
		const u32 upcase_len)
{
	u32 cnt, min_len;
	u16 c1, c2;

	min_len = name1_len;
	if (name1_len > name2_len)
		min_len = name2_len;
	for (cnt = 0; cnt < min_len; ++cnt) {
		c1 = le16_to_cpu(*name1++);
		c2 = le16_to_cpu(*name2++);
		if (ic) {
			if (c1 < upcase_len)
				c1 = le16_to_cpu(upcase[c1]);
			if (c2 < upcase_len)
				c2 = le16_to_cpu(upcase[c2]);
		}
		if (c1 < 64 && ntfs_legal_ansi_char_array[c1] & 8)
			return err_val;
		if (c1 < c2)
			return -1;
		if (c1 > c2)
			return 1;
	}
	if (name1_len < name2_len)
		return -1;
	if (name1_len == name2_len)
		return 0;
	/*
	 * name1_len > name2_len
	 *
	 * Sanity check the remainder of the string.
	 */
	do {
		c1 = le16_to_cpu(*name1++);
		if (c1 < 64 && ntfs_legal_ansi_char_array[c1] & 8)
			return err_val;
	} while (++cnt < name1_len);
	return 1;
}

/**
 * ntfs_ucsncmp - compare two little endian Unicode strings
 * @s1:		first string
 * @s2:		second string
 * @n:		maximum unicode characters to compare
 *
 * Compare the first @n characters of the Unicode strings @s1 and @s2.  The
 * strings are in little endian format and appropriate le16_to_cpu() conversion
 * is performed on non-little endian machines.
 *
 * The function returns an integer less than, equal to, or greater than zero
 * if @s1 (or the first @n Unicode characters thereof) is found, respectively,
 * to be less than, to match, or be greater than @s2.
 */
int ntfs_ucsncmp(const ntfschar *s1, const ntfschar *s2, size_t n)
{
	u16 c1, c2;
	size_t i;

	for (i = 0; i < n; ++i) {
		c1 = le16_to_cpu(s1[i]);
		c2 = le16_to_cpu(s2[i]);
		if (c1 < c2)
			return -1;
		if (c1 > c2)
			return 1;
		if (!c1)
			break;
	}
	return 0;
}

/**
 * ntfs_ucsncasecmp - compare two little endian Unicode strings, ignoring case
 * @s1:			first string
 * @s2:			second string
 * @n:			maximum unicode characters to compare
 * @upcase:		upcase table
 * @upcase_size:	upcase table size in Unicode characters
 *
 * Compare the first @n characters of the Unicode strings @s1 and @s2,
 * ignoring case.  The strings in little endian format and appropriate
 * le16_to_cpu() conversion is performed on non-little endian machines.
 *
 * Each character is uppercased using the @upcase table before the comparison.
 *
 * The function returns an integer less than, equal to, or greater than zero
 * if @s1 (or the first @n Unicode characters thereof) is found, respectively,
 * to be less than, to match, or be greater than @s2.
 */
int ntfs_ucsncasecmp(const ntfschar *s1, const ntfschar *s2, size_t n,
		const ntfschar *upcase, const u32 upcase_size)
{
	size_t i;
	u16 c1, c2;

	for (i = 0; i < n; ++i) {
		if ((c1 = le16_to_cpu(s1[i])) < upcase_size)
			c1 = le16_to_cpu(upcase[c1]);
		if ((c2 = le16_to_cpu(s2[i])) < upcase_size)
			c2 = le16_to_cpu(upcase[c2]);
		if (c1 < c2)
			return -1;
		if (c1 > c2)
			return 1;
		if (!c1)
			break;
	}
	return 0;
}

void ntfs_upcase_name(ntfschar *name, u32 name_len, const ntfschar *upcase,
		const u32 upcase_len)
{
	u32 i;
	u16 u;

	for (i = 0; i < name_len; i++)
		if ((u = le16_to_cpu(name[i])) < upcase_len)
			name[i] = upcase[u];
}

/**
 * utf8_to_ntfs - convert UTF-8 OSX string to NTFS string
 * @vol:	ntfs volume which we are working with
 * @ins:	input UTF-8 string buffer
 * @ins_size:	length of input string in bytes
 * @outs:	on return contains the (allocated) output NTFS string buffer
 * @outs_size:	pointer to length of output string buffer in bytes
 *
 * Convert the input, decomposed, NUL terminated, UTF-8 string @ins into the
 * little endian, 2-byte, composed Unicode string format used on NTFS volumes.
 * We convert any non-acceptable characters to the Win32 API to private Unicode
 * characters on the assumption that the NTFS volume was written in this way,
 * too and that the mapping used was the same as the Services For Macintosh one
 * as described at http://support.microsoft.com/kb/q117258/.
 *
 * If *@outs is NULL, this function allocates the string and the caller is
 * responsible for calling OSFree(*@outs, *@outs_size, ntfs_malloc_tag); when
 * finished with it.
 *
 * If *@outs is not NULL, it is used as the destination buffer and the caller
 * is responsible for having allocated a big enough buffer.  The minimum size
 * is NTFS_MAX_NAME_LEN * sizeof(ntfschar) bytes.
 *
 * On success the function returns the number of Unicode characters written to
 * the output string *@outs (>= 0), not counting the terminating Unicode NUL
 * character.  If the output string buffer was allocated, *@outs is set to it
 * and *@outs_size is set to the size of the allocated buffer.
 *
 * On error, a negative number corresponding to the error code is returned.  In
 * that case the output string is not allocated.  The contents of *@outs and
 * *@outs_size are then undefined.
 *
 * FIXME: Do we want UTF_SFM_CONVERSIONS, i.e. Services For Macintosh,
 * unconditionally or flexibly by mount option?  And what about the SFU
 * (Services For Unix) conversions?  Should we use those instead or perhaps
 * allow a mount option to decide which conversions to use?
 */
signed utf8_to_ntfs(const ntfs_volume *vol, const u8 *ins,
		const size_t ins_size, ntfschar **outs, size_t *outs_size)
{
	ntfschar *ntfs;
	size_t ntfs_size, res_size;
	errno_t err;

	if (*outs) {
		ntfs = *outs;
		ntfs_size = *outs_size;
	} else {
		/* Allocate the maximum length NTFS string. */
		ntfs_size = NTFS_MAX_NAME_LEN << NTFSCHAR_SIZE_SHIFT;
		ntfs = OSMalloc(ntfs_size, ntfs_malloc_tag);
		if (!ntfs) {
			ntfs_error(vol->mp, "Failed to allocate memory for "
					"output string.");
			return -ENOMEM;
		}
	}
	ntfs_debug("Input string size in bytes %lu (UTF-8).",
			(unsigned long)ins_size);
	/*
	 * Convert the input string to NTFS formt (NUL terminated).
	 * FIXME: The "0" is ignored.  If we decide to not specify "0" when
	 * using utf8_encodelen()/utf8_encodestr() but some other value, we
	 * would also need to use that other value instead of "0" here.  This
	 * value would then be converted to the slash '/' character in the NTFS
	 * string.
	 */
	err = utf8_decodestr(ins, ins_size, ntfs, &res_size, ntfs_size, 0,
			UTF_PRECOMPOSED | UTF_LITTLE_ENDIAN |
			UTF_SFM_CONVERSIONS);
	if (err) {
		ntfs_error(vol->mp, "Failed to convert decomposed, NUL "
				"terminated, UTF-8 input string to NTFS "
				"string (error %d, res_size %lu).", (int)err,
				(unsigned long)res_size);
		goto err;
	}
	if (!*outs) {
		*outs = ntfs;
		*outs_size = ntfs_size;
	}
	res_size >>= NTFSCHAR_SIZE_SHIFT;
	ntfs_debug("Converted string size in Unicode characters %lu.",
			(unsigned long)res_size);
	return res_size;
err:
	if (!*outs)
		OSFree(ntfs, ntfs_size, ntfs_malloc_tag);
	return -err;
}

/**
 * ntfs_to_utf8 - convert NTFS string to UTF-8 OSX string
 * @vol:	ntfs volume which we are working with
 * @ins:	input NTFS string buffer
 * @ins_size:	length of input string in bytes
 * @outs:	on return contains the (allocated) output UTF-8 string buffer
 * @outs_size:	pointer to length of output string buffer in bytes
 *
 * Convert the input, little endian, 2-byte, composed Unicode string @ins, of
 * size @ins_size bytes into the decomposed, NUL terminated, UTF-8 string
 * format used in the OSX kernel.  We assume that any non-acceptable characters
 * to the Win32 API have been converted to private Unicode characters when they
 * were written and the mapping used was the same as the Services For Macintosh
 * one as described at http://support.microsoft.com/kb/q117258/.
 *
 * If *@outs is NULL, this function allocates the string and the caller is
 * responsible for calling OSFree(*@outs, *@outs_size, ntfs_malloc_tag); when
 * finished with it.
 *
 * On success the function returns the number of bytes written to the output
 * string *@outs (>= 0), not counting the terminating NUL byte.  If the output
 * string buffer was allocated, *@outs is set to it and *@outs_size is set to
 * the size of the allocated buffer.
 *
 * On error, a negative number corresponding to the error code is returned.  In
 * that case the output string is not allocated.  The contents of *@outs and
 * *@outs_size are then undefined.
 *
 * FIXME: Do we want UTF_SFM_CONVERSIONS, i.e. Services For Macintosh,
 * unconditionally or flexibly by mount option?  And what about the SFU
 * (Services For Unix) conversions?  Should we use those instead or perhaps
 * allow a mount option to decide which conversions to use?
 */
signed ntfs_to_utf8(const ntfs_volume *vol, const ntfschar *ins,
		const size_t ins_size, u8 **outs, size_t *outs_size)
{
	u8 *utf8;
	size_t utf8_size, res_size;
	errno_t err;

	if (*outs) {
		utf8 = *outs;
		utf8_size = *outs_size;
	} else {
		/*
		 * Calculate the length of the decomposed utf8 string.  Add one
		 * for the NUL terminator.
		 */
		utf8_size = utf8_encodelen(ins, ins_size, 0, UTF_DECOMPOSED |
				UTF_LITTLE_ENDIAN | UTF_SFM_CONVERSIONS) + 1;
		/* Allocate buffer for the converted string. */
		utf8 = OSMalloc(utf8_size, ntfs_malloc_tag);
		if (!utf8) {
			ntfs_error(vol->mp, "Failed to allocate memory for "
					"output string.");
			return -ENOMEM;
		}
	}
	ntfs_debug("Input string size in bytes %lu (NTFS), %lu (decomposed "
			"UTF-8, including NUL terminator).",
			(unsigned long)ins_size, (unsigned long)utf8_size);
	/*
	 * Convert the input string to decomposed utf-8 (NUL terminated).
	 * FIXME: The "0" causes any occurences of the slash '/' character to
	 * be converted to the underscore '_' character.  We could specify
	 * some other unused character so that that can be mapped back to '/'
	 * when converting back to NTFS string format.
	 */
	err = utf8_encodestr(ins, ins_size, utf8, &res_size, utf8_size, 0,
			UTF_DECOMPOSED | UTF_LITTLE_ENDIAN |
			UTF_SFM_CONVERSIONS);
	if (err) {
		ntfs_error(vol->mp, "Failed to convert NTFS input string to "
				"decomposed, NUL terminated, UTF-8 string "
				"(error %d).", (int)err);
		goto err;
	}
	if (!*outs) {
		if (res_size + 1 != utf8_size) {
			ntfs_error(vol->mp, "res_size (%lu) + 1 != utf8_size "
					"(%lu)", (unsigned long)res_size,
					(unsigned long)utf8_size);
			err = EILSEQ;
			goto err;
		}
		*outs = utf8;
		*outs_size = utf8_size;
	}
	ntfs_debug("Converted string size in bytes %lu (decomposed UTF-8).",
			(unsigned long)res_size);
	return res_size;
err:
	if (!*outs)
		OSFree(utf8, utf8_size, ntfs_malloc_tag);
	return -err;
}
