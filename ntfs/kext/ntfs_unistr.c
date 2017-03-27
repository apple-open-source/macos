/*
 * ntfs_unistr.c - NTFS kernel Unicode string operations.
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

#include <sys/errno.h>
#include <sys/types.h>
#include <sys/utfconv.h>

#include <string.h>

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
 * @case_sensitive:	if true compare names case sensitively
 * @upcase:		upcase table (only if @ic == IGNORE_CASE)
 * @upcase_len:		length in Unicode characters of @upcase (if present)
 *
 * Compare the names @s1 and @s2 and return true if the names are identical, or
 * false if they are not identical.  If @case_sensitive is false, the @upcase
 * table is used to perform a case insensitive comparison.
 */
BOOL ntfs_are_names_equal(const ntfschar *s1, size_t s1_len,
		const ntfschar *s2, size_t s2_len, const BOOL case_sensitive,
		const ntfschar *upcase, const u32 upcase_len)
{
	if (s1_len != s2_len)
		return FALSE;
	if (case_sensitive)
		return !ntfs_ucsncmp(s1, s2, s1_len);
	return !ntfs_ucsncasecmp(s1, s2, s1_len, upcase, upcase_len);
}

/**
 * ntfs_collate_names - collate two Unicode names
 * @name1:	first Unicode name to compare
 * @name2:	second Unicode name to compare
 * @err_val:	if @name1 contains an invalid character return this value
 * @case_sensitive:	true if to collace case sensitive and false otherwise
 * @upcase:		upcase table (ignored if @case_sensitive is false)
 * @upcase_len:		upcase table length (ignored if !@case_sensitive)
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
		const BOOL case_sensitive, const ntfschar *upcase,
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
		if (!case_sensitive) {
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
 * utf8_to_ntfs - convert UTF-8 OS X string to NTFS string
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
				"terminated, UTF-8 input string '%s' (size "
				"%lu) to NTFS string (error %d, res_size %lu, "
				"ntfs_size %lu).", ins, (unsigned long)ins_size,
				(int)err, (unsigned long)res_size,
				(unsigned long)ntfs_size);
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
 * ntfs_to_utf8 - convert NTFS string to UTF-8 OS X string
 * @vol:	ntfs volume which we are working with
 * @ins:	input NTFS string buffer
 * @ins_size:	length of input string in bytes
 * @outs:	on return contains the (allocated) output UTF-8 string buffer
 * @outs_size:	pointer to length of output string buffer in bytes
 *
 * Convert the input, little endian, 2-byte, composed Unicode string @ins, of
 * size @ins_size bytes into the decomposed, NUL terminated, UTF-8 string
 * format used in the OS X kernel.  We assume that any non-acceptable
 * characters to the Win32 API have been converted to private Unicode
 * characters when they were written and the mapping used was the same as the
 * Services For Macintosh one as described at
 * http://support.microsoft.com/kb/q117258/.
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

        /* Validate length for x64 size_t -> uint32_t conversion
           I expect that compiler would optimize it out in 32 bit builds
         */
        if (utf8_size > (size_t)UINT32_MAX) {
            ntfs_error(vol->mp, "Output string is longer than UINT32_MAX.");
            return -EINVAL;
        }

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

/**
 * ntfs_upcase_table_generate - generate the NTFS upcase table
 * @uc:		destination buffer in which to generate the upcase table
 * @uc_size:	size of the destination buffer in bytes
 *
 * Generate the full, 16-bit, little endian NTFS Unicode upcase table as used
 * by Windows Vista.
 *
 * @uc_size must be able to at least hold the full, 16-bit Unicode upcase
 * table, i.e. 2^16 * sizeof(ntfschar) = 64ki * 2 bytes = 128kiB.
 */
void ntfs_upcase_table_generate(ntfschar *uc, int uc_size)
{
	int i, r;
	/*
	 * "Start" is inclusive and "End" is exclusive, every value has the
	 * value of "Add" added to it.
	 */
	static int add[][3] = { /* Start, End, Add */
	{0x0061, 0x007b,   -32}, {0x00e0, 0x00f7,  -32}, {0x00f8, 0x00ff, -32}, 
	{0x0256, 0x0258,  -205}, {0x028a, 0x028c, -217}, {0x037b, 0x037e, 130}, 
	{0x03ac, 0x03ad,   -38}, {0x03ad, 0x03b0,  -37}, {0x03b1, 0x03c2, -32},
	{0x03c2, 0x03c3,   -31}, {0x03c3, 0x03cc,  -32}, {0x03cc, 0x03cd, -64},
	{0x03cd, 0x03cf,   -63}, {0x0430, 0x0450,  -32}, {0x0450, 0x0460, -80},
	{0x0561, 0x0587,   -48}, {0x1f00, 0x1f08,    8}, {0x1f10, 0x1f16,   8},
	{0x1f20, 0x1f28,     8}, {0x1f30, 0x1f38,    8}, {0x1f40, 0x1f46,   8},
	{0x1f51, 0x1f52,     8}, {0x1f53, 0x1f54,    8}, {0x1f55, 0x1f56,   8},
	{0x1f57, 0x1f58,     8}, {0x1f60, 0x1f68,    8}, {0x1f70, 0x1f72,  74},
	{0x1f72, 0x1f76,    86}, {0x1f76, 0x1f78,  100}, {0x1f78, 0x1f7a, 128},
	{0x1f7a, 0x1f7c,   112}, {0x1f7c, 0x1f7e,  126}, {0x1f80, 0x1f88,   8},
	{0x1f90, 0x1f98,     8}, {0x1fa0, 0x1fa8,    8}, {0x1fb0, 0x1fb2,   8},
	{0x1fb3, 0x1fb4,     9}, {0x1fcc, 0x1fcd,   -9}, {0x1fd0, 0x1fd2,   8},
	{0x1fe0, 0x1fe2,     8}, {0x1fe5, 0x1fe6,    7}, {0x1ffc, 0x1ffd,  -9},
	{0x2170, 0x2180,   -16}, {0x24d0, 0x24ea,  -26}, {0x2c30, 0x2c5f, -48},
	{0x2d00, 0x2d26, -7264}, {0xff41, 0xff5b,  -32}, {0}
	};
	/*
	 * "Start" is exclusive and "End" is inclusive, every second value is
	 * decremented by one.
	 */
	static int skip_dec[][2] = { /* Start, End */
	{0x0100, 0x012f}, {0x0132, 0x0137}, {0x0139, 0x0149}, {0x014a, 0x0178},
	{0x0179, 0x017e}, {0x01a0, 0x01a6}, {0x01b3, 0x01b7}, {0x01cd, 0x01dd},
	{0x01de, 0x01ef}, {0x01f4, 0x01f5}, {0x01f8, 0x01f9}, {0x01fa, 0x0220},
	{0x0222, 0x0234}, {0x023b, 0x023c}, {0x0241, 0x0242}, {0x0246, 0x024f},
	{0x03d8, 0x03ef}, {0x03f7, 0x03f8}, {0x03fa, 0x03fb}, {0x0460, 0x0481},
	{0x048a, 0x04bf}, {0x04c1, 0x04c4}, {0x04c5, 0x04c8}, {0x04c9, 0x04ce},
	{0x04ec, 0x04ed}, {0x04d0, 0x04eb}, {0x04ee, 0x04f5}, {0x04f6, 0x0513},
	{0x1e00, 0x1e95}, {0x1ea0, 0x1ef9}, {0x2183, 0x2184}, {0x2c60, 0x2c61},
	{0x2c67, 0x2c6c}, {0x2c75, 0x2c76}, {0x2c80, 0x2ce3}, {0}
	};
	/*
	 * Set the Unicode character at offset "Offset" to "Value".  Note,
	 * "Value" is host endian.
	 */
	static int set[][2] = { /* Offset, Value */
	{0x00ff, 0x0178}, {0x0180, 0x0243}, {0x0183, 0x0182}, {0x0185, 0x0184},
	{0x0188, 0x0187}, {0x018c, 0x018b}, {0x0192, 0x0191}, {0x0195, 0x01f6},
	{0x0199, 0x0198}, {0x019a, 0x023d}, {0x019e, 0x0220}, {0x01a8, 0x01a7},
	{0x01ad, 0x01ac}, {0x01b0, 0x01af}, {0x01b9, 0x01b8}, {0x01bd, 0x01bc},
	{0x01bf, 0x01f7}, {0x01c6, 0x01c4}, {0x01c9, 0x01c7}, {0x01cc, 0x01ca},
	{0x01dd, 0x018e}, {0x01f3, 0x01f1}, {0x023a, 0x2c65}, {0x023e, 0x2c66},
	{0x0253, 0x0181}, {0x0254, 0x0186}, {0x0259, 0x018f}, {0x025b, 0x0190},
	{0x0260, 0x0193}, {0x0263, 0x0194}, {0x0268, 0x0197}, {0x0269, 0x0196},
	{0x026b, 0x2c62}, {0x026f, 0x019c}, {0x0272, 0x019d}, {0x0275, 0x019f},
	{0x027d, 0x2c64}, {0x0280, 0x01a6}, {0x0283, 0x01a9}, {0x0288, 0x01ae},
	{0x0289, 0x0244}, {0x028c, 0x0245}, {0x0292, 0x01b7}, {0x03f2, 0x03f9},
	{0x04cf, 0x04c0}, {0x1d7d, 0x2c63}, {0x214e, 0x2132}, {0}
	};

	bzero(uc, uc_size);
	uc_size /= sizeof(ntfschar);
	/* Start with a one-to-one mapping, i.e. no upcasing happens at all. */
	for (i = 0; i < uc_size; i++)
		uc[i] = cpu_to_le16(i);
	/* Adjust specified runs by the specified amount. */
	for (r = 0; add[r][0]; r++)
		for (i = add[r][0]; i < add[r][1]; i++)
			uc[i] = cpu_to_le16(le16_to_cpu(uc[i]) + add[r][2]);
	/* Decrement every second value in specified runs. */
	for (r = 0; skip_dec[r][0]; r++)
		for (i = skip_dec[r][0]; i < skip_dec[r][1];
				i += 2)
			uc[i + 1] = cpu_to_le16(le16_to_cpu(uc[i + 1]) - 1);
	/* Set specified characters to specified values. */
	for (r = 0; set[r][0]; r++)
		uc[set[r][0]] = cpu_to_le16(set[r][1]);
}
