/**
 * unistr.c - Unicode string handling.
 *
 * Copyright (c) 2000-2004 Anton Altaparmakov
 * Copyright (c) 2002-2009 Szabolcs Szakacsits
 * Copyright (c) 2008-2011 Jean-Pierre Andre
 * Copyright (c) 2008      Bernhard Kaindl
 * Copyright (c) 2008-2012 Tuxera Inc.
 *
 * See LICENSE file for licensing information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#if defined(__APPLE__) || defined(__DARWIN__)
#ifdef ENABLE_NFCONV
#include <CoreFoundation/CoreFoundation.h>
#endif /* ENABLE_NFCONV */
#endif /* defined(__APPLE__) || defined(__DARWIN__) */

#include "compat.h"
#include "attrib.h"
#include "types.h"
#include "unistr.h"
#include "debug.h"
#include "logging.h"
#include "misc.h"

#define NOREVBOM 0  /* JPA rejecting U+FFFE and U+FFFF, open to debate */

/*
 * IMPORTANT
 * =========
 *
 * All these routines assume that the Unicode characters are in little endian
 * encoding inside the strings!!!
 */

static int use_utf8 = 1; /* use UTF-8 encoding for file names */

#if defined(__APPLE__) || defined(__DARWIN__)
#ifdef ENABLE_NFCONV
/**
 * This variable controls whether or not automatic normalization form conversion
 * should be performed when translating NTFS unicode file names to UTF-8.
 * Defaults to on, but can be controlled from the outside using the function
 *   int ntfs_macosx_normalize_filenames(int normalize);
 */
static int nfconvert_utf8 = 1;
#endif /* ENABLE_NFCONV */
#endif /* defined(__APPLE__) || defined(__DARWIN__) */

/*
 * This is used by the name collation functions to quickly determine what
 * characters are (in)valid.
 */
#if 0
static const u8 legal_ansi_char_array[0x40] = {
	0x00, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
	0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,

	0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
	0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,

	0x17, 0x07, 0x18, 0x17, 0x17, 0x17, 0x17, 0x17,
	0x17, 0x17, 0x18, 0x16, 0x16, 0x17, 0x07, 0x00,

	0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17,
	0x17, 0x17, 0x04, 0x16, 0x18, 0x16, 0x18, 0x18,
};
#endif

/**
 * ntfs_names_are_equal - compare two Unicode names for equality
 * @s1:			name to compare to @s2
 * @s1_len:		length in Unicode characters of @s1
 * @s2:			name to compare to @s1
 * @s2_len:		length in Unicode characters of @s2
 * @ic:			ignore case bool
 * @upcase:		upcase table (only if @ic == IGNORE_CASE)
 * @upcase_size:	length in Unicode characters of @upcase (if present)
 *
 * Compare the names @s1 and @s2 and return TRUE (1) if the names are
 * identical, or FALSE (0) if they are not identical. If @ic is IGNORE_CASE,
 * the @upcase table is used to perform a case insensitive comparison.
 */
BOOL ntfs_names_are_equal(const ntfschar *s1, size_t s1_len,
		const ntfschar *s2, size_t s2_len,
		const IGNORE_CASE_BOOL ic,
		const ntfschar *upcase, const u32 upcase_size)
{
	if (s1_len != s2_len)
		return FALSE;
	if (!s1_len)
		return TRUE;
	if (ic == CASE_SENSITIVE)
		return ntfs_ucsncmp(s1, s2, s1_len) ? FALSE: TRUE;
	return ntfs_ucsncasecmp(s1, s2, s1_len, upcase, upcase_size) ? FALSE:
								       TRUE;
}

/*
 * ntfs_names_full_collate() fully collate two Unicode names
 *
 * @name1:	first Unicode name to compare
 * @name1_len:	length of first Unicode name to compare
 * @name2:	second Unicode name to compare
 * @name2_len:	length of second Unicode name to compare
 * @ic:		either CASE_SENSITIVE or IGNORE_CASE
 * @upcase:	upcase table (ignored if @ic is CASE_SENSITIVE)
 * @upcase_len:	upcase table size (ignored if @ic is CASE_SENSITIVE)
 *
 *  -1 if the first name collates before the second one,
 *   0 if the names match,
 *   1 if the second name collates before the first one, or
 *
 */
int ntfs_names_full_collate(const ntfschar *name1, const u32 name1_len,
		const ntfschar *name2, const u32 name2_len,
		const IGNORE_CASE_BOOL ic, const ntfschar *upcase,
		const u32 upcase_len)
{
	u32 cnt;
	u16 c1, c2;
	u16 u1, u2;

#ifdef DEBUG
	if (!name1 || !name2 || (ic && (!upcase || !upcase_len))) {
		ntfs_log_debug("ntfs_names_collate received NULL pointer!\n");
		exit(1);
	}
#endif
	cnt = min(name1_len, name2_len);
	if (cnt > 0) {
		if (ic == CASE_SENSITIVE) {
			while (--cnt && (*name1 == *name2)) {
				name1++;
				name2++;
			}
			u1 = c1 = le16_to_cpu(*name1);
			u2 = c2 = le16_to_cpu(*name2);
			if (u1 < upcase_len)
				u1 = le16_to_cpu(upcase[u1]);
			if (u2 < upcase_len)
				u2 = le16_to_cpu(upcase[u2]);
			if ((u1 == u2) && cnt)
				do {
					name1++;
					u1 = le16_to_cpu(*name1);
					name2++;
					u2 = le16_to_cpu(*name2);
					if (u1 < upcase_len)
						u1 = le16_to_cpu(upcase[u1]);
					if (u2 < upcase_len)
						u2 = le16_to_cpu(upcase[u2]);
				} while ((u1 == u2) && --cnt);
			if (u1 < u2)
				return -1;
			if (u1 > u2)
				return 1;
			if (name1_len < name2_len)
				return -1;
			if (name1_len > name2_len)
				return 1;
			if (c1 < c2)
				return -1;
			if (c1 > c2)
				return 1;
		} else {
			do {
				u1 = c1 = le16_to_cpu(*name1);
				name1++;
				u2 = c2 = le16_to_cpu(*name2);
				name2++;
				if (u1 < upcase_len)
					u1 = le16_to_cpu(upcase[u1]);
				if (u2 < upcase_len)
					u2 = le16_to_cpu(upcase[u2]);
			} while ((u1 == u2) && --cnt);
			if (u1 < u2)
				return -1;
			if (u1 > u2)
				return 1;
			if (name1_len < name2_len)
				return -1;
			if (name1_len > name2_len)
				return 1;
		}
	} else {
		if (name1_len < name2_len)
			return -1;
		if (name1_len > name2_len)
			return 1;
	}
	return 0;
}

/**
 * ntfs_ucsncmp - compare two little endian Unicode strings
 * @s1:		first string
 * @s2:		second string
 * @n:		maximum unicode characters to compare
 *
 * Compare the first @n characters of the Unicode strings @s1 and @s2,
 * The strings in little endian format and appropriate le16_to_cpu()
 * conversion is performed on non-little endian machines.
 *
 * The function returns an integer less than, equal to, or greater than zero
 * if @s1 (or the first @n Unicode characters thereof) is found, respectively,
 * to be less than, to match, or be greater than @s2.
 */
int ntfs_ucsncmp(const ntfschar *s1, const ntfschar *s2, size_t n)
{
	ntfschar c1, c2;
	size_t i;

#ifdef DEBUG
	if (!s1 || !s2) {
		ntfs_log_debug("ntfs_wcsncmp() received NULL pointer!\n");
		exit(1);
	}
#endif
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
 * ignoring case. The strings in little endian format and appropriate
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
	u16 c1, c2;
	size_t i;

#ifdef DEBUG
	if (!s1 || !s2 || !upcase) {
		ntfs_log_debug("ntfs_wcsncasecmp() received NULL pointer!\n");
		exit(1);
	}
#endif
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

/**
 * ntfs_ucsnlen - determine the length of a little endian Unicode string
 * @s:		pointer to Unicode string
 * @maxlen:	maximum length of string @s
 *
 * Return the number of Unicode characters in the little endian Unicode
 * string @s up to a maximum of maxlen Unicode characters, not including
 * the terminating (ntfschar)'\0'. If there is no (ntfschar)'\0' between @s
 * and @s + @maxlen, @maxlen is returned.
 *
 * This function never looks beyond @s + @maxlen.
 */
u32 ntfs_ucsnlen(const ntfschar *s, u32 maxlen)
{
	u32 i;

	for (i = 0; i < maxlen; i++) {
		if (!le16_to_cpu(s[i]))
			break;
	}
	return i;
}

/**
 * ntfs_ucsndup - duplicate little endian Unicode string
 * @s:		pointer to Unicode string
 * @maxlen:	maximum length of string @s
 *
 * Return a pointer to a new little endian Unicode string which is a duplicate
 * of the string s.  Memory for the new string is obtained with ntfs_malloc(3),
 * and can be freed with free(3).
 *
 * A maximum of @maxlen Unicode characters are copied and a terminating
 * (ntfschar)'\0' little endian Unicode character is added.
 *
 * This function never looks beyond @s + @maxlen.
 *
 * Return a pointer to the new little endian Unicode string on success and NULL
 * on failure with errno set to the error code.
 */
ntfschar *ntfs_ucsndup(const ntfschar *s, u32 maxlen)
{
	ntfschar *dst;
	u32 len;

	len = ntfs_ucsnlen(s, maxlen);
	dst = ntfs_malloc((len + 1) * sizeof(ntfschar));
	if (dst) {
		memcpy(dst, s, len * sizeof(ntfschar));
		dst[len] = cpu_to_le16(L'\0');
	}
	return dst;
}

/* 
 * Return the amount of 16-bit elements in UTF-16LE needed 
 * (without the terminating null) to store given UTF-8 string.
 *
 * Return -1 with errno set if it's longer than PATH_MAX or string is invalid.
 *
 * Note: This does not check whether the input sequence is a valid utf8 string,
 *	 and should be used only in context where such check is made!
 */
static int utf8_to_utf16_size(const char *s)
{
	int ret = -1;
	unsigned int byte;
	size_t count = 0;

	while ((byte = *((const unsigned char *)s++))) {
		if (++count >= PATH_MAX) 
			goto fail;
		if (byte >= 0xc0) {
			if (byte >= 0xF5) {
				errno = EILSEQ;
				goto out;
			}
			if (!*s) 
				break;
			if (byte >= 0xC0) 
				s++;
			if (!*s) 
				break;
			if (byte >= 0xE0) 
				s++;
			if (!*s) 
				break;
			if (byte >= 0xF0) {
				s++;
				if (++count >= PATH_MAX)
					goto fail;
			}
		}
	}
	ret = count;
out:
	return ret;
fail:
	errno = ENAMETOOLONG;
	goto out;
}
/* 
 * This converts one UTF-8 sequence to cpu-endian Unicode value
 * within range U+0 .. U+10ffff and excluding U+D800 .. U+DFFF
 *
 * Return the number of used utf8 bytes or -1 with errno set 
 * if sequence is invalid.
 */
static int utf8_to_unicode(u32 *wc, const char *s)
{
    	unsigned int byte = *((const unsigned char *)s);

					/* single byte */
	if (byte == 0) {
		*wc = (u32) 0;
		return 0;
	} else if (byte < 0x80) {
		*wc = (u32) byte;
		return 1;
					/* double byte */
	} else if (byte < 0xc2) {
		goto fail;
	} else if (byte < 0xE0) {
		if ((s[1] & 0xC0) == 0x80) {
			*wc = ((u32)(byte & 0x1F) << 6)
			    | ((u32)(s[1] & 0x3F));
			return 2;
		} else
			goto fail;
					/* three-byte */
	} else if (byte < 0xF0) {
		if (((s[1] & 0xC0) == 0x80) && ((s[2] & 0xC0) == 0x80)) {
			*wc = ((u32)(byte & 0x0F) << 12)
			    | ((u32)(s[1] & 0x3F) << 6)
			    | ((u32)(s[2] & 0x3F));
			/* Check valid ranges */
#if NOREVBOM
			if (((*wc >= 0x800) && (*wc <= 0xD7FF))
			  || ((*wc >= 0xe000) && (*wc <= 0xFFFD)))
				return 3;
#else
			if (((*wc >= 0x800) && (*wc <= 0xD7FF))
			  || ((*wc >= 0xe000) && (*wc <= 0xFFFF)))
				return 3;
#endif
		}
		goto fail;
					/* four-byte */
	} else if (byte < 0xF5) {
		if (((s[1] & 0xC0) == 0x80) && ((s[2] & 0xC0) == 0x80)
		  && ((s[3] & 0xC0) == 0x80)) {
			*wc = ((u32)(byte & 0x07) << 18)
			    | ((u32)(s[1] & 0x3F) << 12)
			    | ((u32)(s[2] & 0x3F) << 6)
			    | ((u32)(s[3] & 0x3F));
		/* Check valid ranges */
		if ((*wc <= 0x10ffff) && (*wc >= 0x10000))
			return 4;
		}
		goto fail;
	}
fail:
	errno = EILSEQ;
	return -1;
}

/**
 * ntfs_utf8_to_utf16 - convert a UTF-8 string to a UTF-16LE string
 * @ins:	input multibyte string buffer
 * @outs:	on return contains the (allocated) output utf16 string
 * @outs_len:	length of output buffer in utf16 characters
 * 
 * Return -1 with errno set.
 */
static int ntfs_utf8_to_utf16(const char *ins, ntfschar **outs)
{
#if defined(__APPLE__) || defined(__DARWIN__)
#ifdef ENABLE_NFCONV
	char *new_ins = NULL;
	if(nfconvert_utf8) {
		int new_ins_len;
		new_ins_len = ntfs_macosx_normalize_utf8(ins, &new_ins, 1); // Normalize to composed form
		if(new_ins_len >= 0)
			ins = new_ins;
		else
			ntfs_log_error("Failed to normalize NTFS string to UTF-8 NFC: %s\n", ins);
	}
#endif /* ENABLE_NFCONV */
#endif /* defined(__APPLE__) || defined(__DARWIN__) */
	const char *t = ins;
	u32 wc;
	BOOL allocated;
	ntfschar *outpos;
	int shorts, ret = -1;

	shorts = utf8_to_utf16_size(ins);
	if (shorts < 0)
		goto fail;

	allocated = FALSE;
	if (!*outs) {
		*outs = ntfs_malloc((shorts + 1) * sizeof(ntfschar));
		if (!*outs)
			goto fail;
		allocated = TRUE;
	}

	outpos = *outs;

	while(1) {
		int m  = utf8_to_unicode(&wc, t);
		if (m <= 0) {
			if (m < 0) {
				/* do not leave space allocated if failed */
				if (allocated) {
					free(*outs);
					*outs = (ntfschar*)NULL;
				}
				goto fail;
			}
			*outpos++ = const_cpu_to_le16(0);
			break;
		}
		if (wc < 0x10000)
			*outpos++ = cpu_to_le16(wc);
		else {
			wc -= 0x10000;
			*outpos++ = cpu_to_le16((wc >> 10) + 0xd800);
			*outpos++ = cpu_to_le16((wc & 0x3ff) + 0xdc00);
		}
		t += m;
	}
	
	ret = --outpos - *outs;
fail:
#if defined(__APPLE__) || defined(__DARWIN__)
#ifdef ENABLE_NFCONV
	if(new_ins != NULL)
		free(new_ins);
#endif /* ENABLE_NFCONV */
#endif /* defined(__APPLE__) || defined(__DARWIN__) */
	return ret;
}

/**
 * ntfs_mbstoucs - convert a multibyte string to a little endian Unicode string
 * @ins:	input multibyte string buffer
 * @outs:	on return contains the (allocated) output Unicode string
 *
 * Convert the input multibyte string @ins, from the current locale into the
 * corresponding little endian, 2-byte Unicode string.
 *
 * The function allocates the string and the caller is responsible for calling 
 * free(*@outs); when finished with it.
 *
 * On success the function returns the number of Unicode characters written to
 * the output string *@outs (>= 0), not counting the terminating Unicode NULL
 * character.
 *
 * On error, -1 is returned, and errno is set to the error code. The following
 * error codes can be expected:
 *	EINVAL		Invalid arguments (e.g. @ins or @outs is NULL).
 *	EILSEQ		The input string cannot be represented as a Unicode
 *			string according to the current locale.
 *	ENAMETOOLONG	Destination buffer is too small for input string.
 *	ENOMEM		Not enough memory to allocate destination buffer.
 */
int ntfs_mbstoucs(const char *ins, ntfschar **outs)
{
#ifdef MB_CUR_MAX
	ntfschar *ucs;
	const char *s;
	wchar_t wc;
	int i, o, cnt, ins_len, ucs_len, ins_size;
#ifdef HAVE_MBSINIT
	mbstate_t mbstate;
#endif
#endif /* MB_CUR_MAX */

	if (!ins || !outs) {
		errno = EINVAL;
		return -1;
	}
	
	if (use_utf8)
		return ntfs_utf8_to_utf16(ins, outs);

#ifdef MB_CUR_MAX
	/* Determine the size of the multi-byte string in bytes. */
	ins_size = strlen(ins);
	/* Determine the length of the multi-byte string. */
	s = ins;
#if defined(HAVE_MBSINIT)
	memset(&mbstate, 0, sizeof(mbstate));
	ins_len = mbsrtowcs(NULL, (const char **)&s, 0, &mbstate);
#ifdef __CYGWIN32__
	if (!ins_len && *ins) {
		/* Older Cygwin had broken mbsrtowcs() implementation. */
		ins_len = strlen(ins);
	}
#endif
#elif !defined(DJGPP)
	ins_len = mbstowcs(NULL, s, 0);
#else
	/* Eeek!!! DJGPP has broken mbstowcs() implementation!!! */
	ins_len = strlen(ins);
#endif
	if (ins_len == -1)
		return ins_len;
#ifdef HAVE_MBSINIT
	if ((s != ins) || !mbsinit(&mbstate)) {
#else
	if (s != ins) {
#endif
		errno = EILSEQ;
		return -1;
	}
	/* Add the NULL terminator. */
	ins_len++;
	ucs_len = ins_len;
	ucs = ntfs_malloc(ucs_len * sizeof(ntfschar));
	if (!ucs)
		return -1;
#ifdef HAVE_MBSINIT
	memset(&mbstate, 0, sizeof(mbstate));
#else
	mbtowc(NULL, NULL, 0);
#endif
	for (i = o = cnt = 0; i < ins_size; i += cnt, o++) {
		/* Reallocate memory if necessary. */
		if (o >= ucs_len) {
			ntfschar *tc;
			ucs_len = (ucs_len * sizeof(ntfschar) + 64) & ~63;
			tc = realloc(ucs, ucs_len);
			if (!tc)
				goto err_out;
			ucs = tc;
			ucs_len /= sizeof(ntfschar);
		}
		/* Convert the multibyte character to a wide character. */
#ifdef HAVE_MBSINIT
		cnt = mbrtowc(&wc, ins + i, ins_size - i, &mbstate);
#else
		cnt = mbtowc(&wc, ins + i, ins_size - i);
#endif
		if (!cnt)
			break;
		if (cnt == -1)
			goto err_out;
		if (cnt < -1) {
			ntfs_log_trace("Eeek. cnt = %i\n", cnt);
			errno = EINVAL;
			goto err_out;
		}
		/* Make sure we are not overflowing the NTFS Unicode set. */
		if ((unsigned long)wc >= (unsigned long)(1 <<
				(8 * sizeof(ntfschar)))) {
			errno = EILSEQ;
			goto err_out;
		}
		/* Convert the CPU wide character to a LE Unicode character. */
		ucs[o] = cpu_to_le16(wc);
	}
#ifdef HAVE_MBSINIT
	/* Make sure we are back in the initial state. */
	if (!mbsinit(&mbstate)) {
		ntfs_log_trace("Eeek. mbstate not in initial state!\n");
		errno = EILSEQ;
		goto err_out;
	}
#endif
	/* Now write the NULL character. */
	ucs[o] = cpu_to_le16(L'\0');
	*outs = ucs;
	return o;
err_out:
	free(ucs);
#else /* MB_CUR_MAX */
	errno = EILSEQ;
#endif /* MB_CUR_MAX */
	return -1;
}

/**
 * ntfs_upcase_table_build - build the default upcase table for NTFS
 * @uc:		destination buffer where to store the built table
 * @uc_len:	size of destination buffer in bytes
 *
 * ntfs_upcase_table_build() builds the default upcase table for NTFS and
 * stores it in the caller supplied buffer @uc of size @uc_len.
 *
 * Note, @uc_len must be at least 128kiB in size or bad things will happen!
 */
void ntfs_upcase_table_build(ntfschar *uc, u32 uc_len)
{
#if 1 /* Vista */
	/*
	 *	This is the table as defined by Vista
	 */
	/*
	 * "Start" is inclusive and "End" is exclusive, every value has the
	 * value of "Add" added to it.
	 */
	static int uc_run_table[][3] = { /* Start, End, Add */
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
	static int uc_dup_table[][2] = { /* Start, End */
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
	static int uc_byte_table[][2] = { /* Offset, Value */
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
#else /* Vista */
	/*
	 *	This is the table as defined by Windows XP
	 */
	static int uc_run_table[][3] = { /* Start, End, Add */
	{0x0061, 0x007B,  -32}, {0x0451, 0x045D, -80}, {0x1F70, 0x1F72,  74},
	{0x00E0, 0x00F7,  -32}, {0x045E, 0x0460, -80}, {0x1F72, 0x1F76,  86},
	{0x00F8, 0x00FF,  -32}, {0x0561, 0x0587, -48}, {0x1F76, 0x1F78, 100},
	{0x0256, 0x0258, -205}, {0x1F00, 0x1F08,   8}, {0x1F78, 0x1F7A, 128},
	{0x028A, 0x028C, -217}, {0x1F10, 0x1F16,   8}, {0x1F7A, 0x1F7C, 112},
	{0x03AC, 0x03AD,  -38}, {0x1F20, 0x1F28,   8}, {0x1F7C, 0x1F7E, 126},
	{0x03AD, 0x03B0,  -37}, {0x1F30, 0x1F38,   8}, {0x1FB0, 0x1FB2,   8},
	{0x03B1, 0x03C2,  -32}, {0x1F40, 0x1F46,   8}, {0x1FD0, 0x1FD2,   8},
	{0x03C2, 0x03C3,  -31}, {0x1F51, 0x1F52,   8}, {0x1FE0, 0x1FE2,   8},
	{0x03C3, 0x03CC,  -32}, {0x1F53, 0x1F54,   8}, {0x1FE5, 0x1FE6,   7},
	{0x03CC, 0x03CD,  -64}, {0x1F55, 0x1F56,   8}, {0x2170, 0x2180, -16},
	{0x03CD, 0x03CF,  -63}, {0x1F57, 0x1F58,   8}, {0x24D0, 0x24EA, -26},
	{0x0430, 0x0450,  -32}, {0x1F60, 0x1F68,   8}, {0xFF41, 0xFF5B, -32},
	{0}
	};
	static int uc_dup_table[][2] = { /* Start, End */
	{0x0100, 0x012F}, {0x01A0, 0x01A6}, {0x03E2, 0x03EF}, {0x04CB, 0x04CC},
	{0x0132, 0x0137}, {0x01B3, 0x01B7}, {0x0460, 0x0481}, {0x04D0, 0x04EB},
	{0x0139, 0x0149}, {0x01CD, 0x01DD}, {0x0490, 0x04BF}, {0x04EE, 0x04F5},
	{0x014A, 0x0178}, {0x01DE, 0x01EF}, {0x04BF, 0x04BF}, {0x04F8, 0x04F9},
	{0x0179, 0x017E}, {0x01F4, 0x01F5}, {0x04C1, 0x04C4}, {0x1E00, 0x1E95},
	{0x018B, 0x018B}, {0x01FA, 0x0218}, {0x04C7, 0x04C8}, {0x1EA0, 0x1EF9},
	{0}
	};
	static int uc_byte_table[][2] = { /* Offset, Value */
	{0x00FF, 0x0178}, {0x01AD, 0x01AC}, {0x01F3, 0x01F1}, {0x0269, 0x0196},
	{0x0183, 0x0182}, {0x01B0, 0x01AF}, {0x0253, 0x0181}, {0x026F, 0x019C},
	{0x0185, 0x0184}, {0x01B9, 0x01B8}, {0x0254, 0x0186}, {0x0272, 0x019D},
	{0x0188, 0x0187}, {0x01BD, 0x01BC}, {0x0259, 0x018F}, {0x0275, 0x019F},
	{0x018C, 0x018B}, {0x01C6, 0x01C4}, {0x025B, 0x0190}, {0x0283, 0x01A9},
	{0x0192, 0x0191}, {0x01C9, 0x01C7}, {0x0260, 0x0193}, {0x0288, 0x01AE},
	{0x0199, 0x0198}, {0x01CC, 0x01CA}, {0x0263, 0x0194}, {0x0292, 0x01B7},
	{0x01A8, 0x01A7}, {0x01DD, 0x018E}, {0x0268, 0x0197},
	{0}
	};
#endif /* Vista */
	int i, r;
	int k, off;

	memset((char*)uc, 0, uc_len);
	uc_len >>= 1;
	if (uc_len > 65536)
		uc_len = 65536;
	for (i = 0; (u32)i < uc_len; i++)
		uc[i] = cpu_to_le16(i);
	for (r = 0; uc_run_table[r][0]; r++) {
		off = uc_run_table[r][2];
		for (i = uc_run_table[r][0]; i < uc_run_table[r][1]; i++)
			uc[i] = cpu_to_le16(i + off);
	}
	for (r = 0; uc_dup_table[r][0]; r++)
		for (i = uc_dup_table[r][0]; i < uc_dup_table[r][1]; i += 2)
			uc[i + 1] = cpu_to_le16(i);
	for (r = 0; uc_byte_table[r][0]; r++) {
		k = uc_byte_table[r][1];
		uc[uc_byte_table[r][0]] = cpu_to_le16(k);
	}
}

/**
 * ntfs_str2ucs - convert a string to a valid NTFS file name
 * @s:		input string
 * @len:	length of output buffer in Unicode characters
 *
 * Convert the input @s string into the corresponding little endian,
 * 2-byte Unicode string. The length of the converted string is less 
 * or equal to the maximum length allowed by the NTFS format (255).
 *
 * If @s is NULL then return AT_UNNAMED.
 *
 * On success the function returns the Unicode string in an allocated 
 * buffer and the caller is responsible to free it when it's not needed
 * anymore.
 *
 * On error NULL is returned and errno is set to the error code.
 */
ntfschar *ntfs_str2ucs(const char *s, int *len)
{
	ntfschar *ucs = NULL;

	if (s && ((*len = ntfs_mbstoucs(s, &ucs)) == -1)) {
		ntfs_log_perror("Couldn't convert '%s' to Unicode", s);
		return NULL;
	}
	if (*len > NTFS_MAX_NAME_LEN) {
		free(ucs);
		errno = ENAMETOOLONG;
		return NULL;
	}
	if (!ucs || !*len) {
		ucs  = AT_UNNAMED;
		*len = 0;
	}
	return ucs;
}

/**
 * ntfs_ucsfree - free memory allocated by ntfs_str2ucs()
 * @ucs		input string to be freed
 *
 * Free memory at @ucs and which was allocated by ntfs_str2ucs.
 *
 * Return value: none.
 */
void ntfs_ucsfree(ntfschar *ucs)
{
	if (ucs && (ucs != AT_UNNAMED))
		free(ucs);
}

#if defined(__APPLE__) || defined(__DARWIN__)
int ntfs_macosx_normalize_utf8(const char *utf8_string, char **target,
		int composed)
{
#ifdef ENABLE_NFCONV
	/* For this code to compile, the CoreFoundation framework must be fed to
	 * the linker. */
	CFStringRef cfSourceString;
	CFMutableStringRef cfMutableString;
	CFRange rangeToProcess;
	CFIndex requiredBufferLength;
	char *result = NULL;
	int resultLength = -1;
	
	/* Convert the UTF-8 string to a CFString. */
	cfSourceString = CFStringCreateWithCString(kCFAllocatorDefault,
		utf8_string, kCFStringEncodingUTF8);
	if (cfSourceString == NULL) {
		ntfs_log_error("CFStringCreateWithCString failed!\n");
		return -2;
	}

	/* Create a mutable string from cfSourceString that we are free to
	 * modify. */
	cfMutableString = CFStringCreateMutableCopy(kCFAllocatorDefault, 0,
		cfSourceString);
	CFRelease(cfSourceString); /* End-of-life. */
	if (cfMutableString == NULL) {
		ntfs_log_error("CFStringCreateMutableCopy failed!\n");
		return -3;
	}

	/* Normalize the mutable string to the desired normalization form. */
	CFStringNormalize(cfMutableString, (composed != 0 ?
		kCFStringNormalizationFormC : kCFStringNormalizationFormD));

	/* Store the resulting string in a '\0'-terminated UTF-8 encoded char*
	 * buffer. */
	rangeToProcess = CFRangeMake(0, CFStringGetLength(cfMutableString));
	if (CFStringGetBytes(cfMutableString, rangeToProcess,
		kCFStringEncodingUTF8, 0, false, NULL, 0,
		&requiredBufferLength) > 0)
	{
		resultLength = sizeof(char) * (requiredBufferLength + 1);
		result = ntfs_calloc(resultLength);

		if (result != NULL) {
			if (CFStringGetBytes(cfMutableString, rangeToProcess,
				kCFStringEncodingUTF8, 0, false,
				(UInt8*) result, resultLength - 1,
				&requiredBufferLength) <= 0)
			{
				ntfs_log_error("Could not perform UTF-8 "
					"conversion of normalized "
					"CFMutableString.\n");
				free(result);
				result = NULL;
			}
		}
		else {
			ntfs_log_error("Could not perform a ntfs_calloc of %d "
				"bytes for char *result.\n", resultLength);
		}
	}
	else {
		ntfs_log_error("Could not perform check for required length of "
			"UTF-8 conversion of normalized CFMutableString.\n");
	}

	CFRelease(cfMutableString);

	if (result != NULL) {
	 	*target = result;
		return resultLength - 1;
	}
	else {
		return -1;
	}
#else
	return -1;
#endif /* ENABLE_NFCONV */
}
#endif /* defined(__APPLE__) || defined(__DARWIN__) */
