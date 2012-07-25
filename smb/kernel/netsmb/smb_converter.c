/*
 * Copyright (c) 2008 - 2009 Apple Inc. All rights reserved.
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

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/smb_apple.h>

#include <netsmb/smb.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_subr.h>
#include <netsmb/smb_converter.h>
#include <sys/smb_byte_order.h>

/* UCS2 to CodePage Conversion Data */
typedef struct _UCSTo8BitCharMap {
    uint16_t _u;
    uint8_t _c;
} UCSTo8BitCharMap;

/* CodePage to UCS2 Conversion Data */
static const uint16_t cp437_to_ucs2[128] = {
	0x00c7, 0x00fc, 0x00e9, 0x00e2, 0x00e4, 0x00e0, 0x00e5, 0x00e7,
	0x00ea, 0x00eb, 0x00e8, 0x00ef, 0x00ee, 0x00ec, 0x00c4, 0x00c5,
	0x00c9, 0x00e6, 0x00c6, 0x00f4, 0x00f6, 0x00f2, 0x00fb, 0x00f9,
	0x00ff, 0x00d6, 0x00dc, 0x00a2, 0x00a3, 0x00a5, 0x20a7, 0x0192,
	0x00e1, 0x00ed, 0x00f3, 0x00fa, 0x00f1, 0x00d1, 0x00aa, 0x00ba,
	0x00bf, 0x2310, 0x00ac, 0x00bd, 0x00bc, 0x00a1, 0x00ab, 0x00bb,
	0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556,
	0x2555, 0x2563, 0x2551, 0x2557, 0x255d, 0x255c, 0x255b, 0x2510,
	0x2514, 0x2534, 0x252c, 0x251c, 0x2500, 0x253c, 0x255e, 0x255f,
	0x255a, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256c, 0x2567,
	0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256b,
	0x256a, 0x2518, 0x250c, 0x2588, 0x2584, 0x258c, 0x2590, 0x2580,
	0x03b1, 0x00df, 0x0393, 0x03c0, 0x03a3, 0x03c3, 0x00b5, 0x03c4,
	0x03a6, 0x0398, 0x03a9, 0x03b4, 0x221e, 0x03c6, 0x03b5, 0x2229,
	0x2261, 0x00b1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00f7, 0x2248,
	0x00b0, 0x2219, 0x00b7, 0x221a, 0x207f, 0x00b2, 0x25a0, 0x00a0
};

/* DOS Latin US (Code Page 437) */
static const UCSTo8BitCharMap cp437_from_ucs2[128] = {
	{0x00a0, 0xff}, // NO-BREAK SPACE
	{0x00a1, 0xad}, // INVERTED EXCLAMATION MARK
	{0x00a2, 0x9b}, // CENT SIGN
	{0x00a3, 0x9c}, // POUND SIGN
	{0x00a5, 0x9d}, // YEN SIGN
	{0x00aa, 0xa6}, // FEMININE ORDINAL INDICATOR
	{0x00ab, 0xae}, // LEFT-POINTING DOUBLE ANGLE QUOTATION MARK
	{0x00ac, 0xaa}, // NOT SIGN
	{0x00b0, 0xf8}, // DEGREE SIGN
	{0x00b1, 0xf1}, // PLUS-MINUS SIGN
	{0x00b2, 0xfd}, // SUPERSCRIPT TWO
	{0x00b5, 0xe6}, // MICRO SIGN
	{0x00b7, 0xfa}, // MIDDLE DOT
	{0x00ba, 0xa7}, // MASCULINE ORDINAL INDICATOR
	{0x00bb, 0xaf}, // RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK
	{0x00bc, 0xac}, // VULGAR FRACTION ONE QUARTER
	{0x00bd, 0xab}, // VULGAR FRACTION ONE HALF
	{0x00bf, 0xa8}, // INVERTED QUESTION MARK
	{0x00c4, 0x8e}, // LATIN CAPITAL LETTER A WITH DIAERESIS
	{0x00c5, 0x8f}, // LATIN CAPITAL LETTER A WITH RING ABOVE
	{0x00c6, 0x92}, // LATIN CAPITAL LIGATURE AE
	{0x00c7, 0x80}, // LATIN CAPITAL LETTER C WITH CEDILLA
	{0x00c9, 0x90}, // LATIN CAPITAL LETTER E WITH ACUTE
	{0x00d1, 0xa5}, // LATIN CAPITAL LETTER N WITH TILDE
	{0x00d6, 0x99}, // LATIN CAPITAL LETTER O WITH DIAERESIS
	{0x00dc, 0x9a}, // LATIN CAPITAL LETTER U WITH DIAERESIS
	{0x00df, 0xe1}, // LATIN SMALL LETTER SHARP S
	{0x00e0, 0x85}, // LATIN SMALL LETTER A WITH GRAVE
	{0x00e1, 0xa0}, // LATIN SMALL LETTER A WITH ACUTE
	{0x00e2, 0x83}, // LATIN SMALL LETTER A WITH CIRCUMFLEX
	{0x00e4, 0x84}, // LATIN SMALL LETTER A WITH DIAERESIS
	{0x00e5, 0x86}, // LATIN SMALL LETTER A WITH RING ABOVE
	{0x00e6, 0x91}, // LATIN SMALL LIGATURE AE
	{0x00e7, 0x87}, // LATIN SMALL LETTER C WITH CEDILLA
	{0x00e8, 0x8a}, // LATIN SMALL LETTER E WITH GRAVE
	{0x00e9, 0x82}, // LATIN SMALL LETTER E WITH ACUTE
	{0x00ea, 0x88}, // LATIN SMALL LETTER E WITH CIRCUMFLEX
	{0x00eb, 0x89}, // LATIN SMALL LETTER E WITH DIAERESIS
	{0x00ec, 0x8d}, // LATIN SMALL LETTER I WITH GRAVE
	{0x00ed, 0xa1}, // LATIN SMALL LETTER I WITH ACUTE
	{0x00ee, 0x8c}, // LATIN SMALL LETTER I WITH CIRCUMFLEX
	{0x00ef, 0x8b}, // LATIN SMALL LETTER I WITH DIAERESIS
	{0x00f1, 0xa4}, // LATIN SMALL LETTER N WITH TILDE
	{0x00f2, 0x95}, // LATIN SMALL LETTER O WITH GRAVE
	{0x00f3, 0xa2}, // LATIN SMALL LETTER O WITH ACUTE
	{0x00f4, 0x93}, // LATIN SMALL LETTER O WITH CIRCUMFLEX
	{0x00f6, 0x94}, // LATIN SMALL LETTER O WITH DIAERESIS
	{0x00f7, 0xf6}, // DIVISION SIGN
	{0x00f9, 0x97}, // LATIN SMALL LETTER U WITH GRAVE
	{0x00fa, 0xa3}, // LATIN SMALL LETTER U WITH ACUTE
	{0x00fb, 0x96}, // LATIN SMALL LETTER U WITH CIRCUMFLEX
	{0x00fc, 0x81}, // LATIN SMALL LETTER U WITH DIAERESIS
	{0x00ff, 0x98}, // LATIN SMALL LETTER Y WITH DIAERESIS
	{0x0192, 0x9f}, // LATIN SMALL LETTER F WITH HOOK
	{0x0393, 0xe2}, // GREEK CAPITAL LETTER GAMMA
	{0x0398, 0xe9}, // GREEK CAPITAL LETTER THETA
	{0x03a3, 0xe4}, // GREEK CAPITAL LETTER SIGMA
	{0x03a6, 0xe8}, // GREEK CAPITAL LETTER PHI
	{0x03a9, 0xea}, // GREEK CAPITAL LETTER OMEGA
	{0x03b1, 0xe0}, // GREEK SMALL LETTER ALPHA
	{0x03b4, 0xeb}, // GREEK SMALL LETTER DELTA
	{0x03b5, 0xee}, // GREEK SMALL LETTER EPSILON
	{0x03c0, 0xe3}, // GREEK SMALL LETTER PI
	{0x03c3, 0xe5}, // GREEK SMALL LETTER SIGMA
	{0x03c4, 0xe7}, // GREEK SMALL LETTER TAU
	{0x03c6, 0xed}, // GREEK SMALL LETTER PHI
	{0x207f, 0xfc}, // SUPERSCRIPT LATIN SMALL LETTER N
	{0x20a7, 0x9e}, // PESETA SIGN
	{0x2219, 0xf9}, // BULLET OPERATOR
	{0x221a, 0xfb}, // SQUARE ROOT
	{0x221e, 0xec}, // INFINITY
	{0x2229, 0xef}, // INTERSECTION
	{0x2248, 0xf7}, // ALMOST EQUAL TO
	{0x2261, 0xf0}, // IDENTICAL TO
	{0x2264, 0xf3}, // LESS-THAN OR EQUAL TO
	{0x2265, 0xf2}, // GREATER-THAN OR EQUAL TO
	{0x2310, 0xa9}, // REVERSED NOT SIGN
	{0x2320, 0xf4}, // TOP HALF INTEGRAL
	{0x2321, 0xf5}, // BOTTOM HALF INTEGRAL
	{0x2500, 0xc4}, // BOX DRAWINGS LIGHT HORIZONTAL
	{0x2502, 0xb3}, // BOX DRAWINGS LIGHT VERTICAL
	{0x250c, 0xda}, // BOX DRAWINGS LIGHT DOWN AND RIGHT
	{0x2510, 0xbf}, // BOX DRAWINGS LIGHT DOWN AND LEFT
	{0x2514, 0xc0}, // BOX DRAWINGS LIGHT UP AND RIGHT
	{0x2518, 0xd9}, // BOX DRAWINGS LIGHT UP AND LEFT
	{0x251c, 0xc3}, // BOX DRAWINGS LIGHT VERTICAL AND RIGHT
	{0x2524, 0xb4}, // BOX DRAWINGS LIGHT VERTICAL AND LEFT
	{0x252c, 0xc2}, // BOX DRAWINGS LIGHT DOWN AND HORIZONTAL
	{0x2534, 0xc1}, // BOX DRAWINGS LIGHT UP AND HORIZONTAL
	{0x253c, 0xc5}, // BOX DRAWINGS LIGHT VERTICAL AND HORIZONTAL
	{0x2550, 0xcd}, // BOX DRAWINGS DOUBLE HORIZONTAL
	{0x2551, 0xba}, // BOX DRAWINGS DOUBLE VERTICAL
	{0x2552, 0xd5}, // BOX DRAWINGS DOWN SINGLE AND RIGHT DOUBLE
	{0x2553, 0xd6}, // BOX DRAWINGS DOWN DOUBLE AND RIGHT SINGLE
	{0x2554, 0xc9}, // BOX DRAWINGS DOUBLE DOWN AND RIGHT
	{0x2555, 0xb8}, // BOX DRAWINGS DOWN SINGLE AND LEFT DOUBLE
	{0x2556, 0xb7}, // BOX DRAWINGS DOWN DOUBLE AND LEFT SINGLE
	{0x2557, 0xbb}, // BOX DRAWINGS DOUBLE DOWN AND LEFT
	{0x2558, 0xd4}, // BOX DRAWINGS UP SINGLE AND RIGHT DOUBLE
	{0x2559, 0xd3}, // BOX DRAWINGS UP DOUBLE AND RIGHT SINGLE
	{0x255a, 0xc8}, // BOX DRAWINGS DOUBLE UP AND RIGHT
	{0x255b, 0xbe}, // BOX DRAWINGS UP SINGLE AND LEFT DOUBLE
	{0x255c, 0xbd}, // BOX DRAWINGS UP DOUBLE AND LEFT SINGLE
	{0x255d, 0xbc}, // BOX DRAWINGS DOUBLE UP AND LEFT
	{0x255e, 0xc6}, // BOX DRAWINGS VERTICAL SINGLE AND RIGHT DOUBLE
	{0x255f, 0xc7}, // BOX DRAWINGS VERTICAL DOUBLE AND RIGHT SINGLE
	{0x2560, 0xcc}, // BOX DRAWINGS DOUBLE VERTICAL AND RIGHT
	{0x2561, 0xb5}, // BOX DRAWINGS VERTICAL SINGLE AND LEFT DOUBLE
	{0x2562, 0xb6}, // BOX DRAWINGS VERTICAL DOUBLE AND LEFT SINGLE
	{0x2563, 0xb9}, // BOX DRAWINGS DOUBLE VERTICAL AND LEFT
	{0x2564, 0xd1}, // BOX DRAWINGS DOWN SINGLE AND HORIZONTAL DOUBLE
	{0x2565, 0xd2}, // BOX DRAWINGS DOWN DOUBLE AND HORIZONTAL SINGLE
	{0x2566, 0xcb}, // BOX DRAWINGS DOUBLE DOWN AND HORIZONTAL
	{0x2567, 0xcf}, // BOX DRAWINGS UP SINGLE AND HORIZONTAL DOUBLE
	{0x2568, 0xd0}, // BOX DRAWINGS UP DOUBLE AND HORIZONTAL SINGLE
	{0x2569, 0xca}, // BOX DRAWINGS DOUBLE UP AND HORIZONTAL
	{0x256a, 0xd8}, // BOX DRAWINGS VERTICAL SINGLE AND HORIZONTAL DOUBLE
	{0x256b, 0xd7}, // BOX DRAWINGS VERTICAL DOUBLE AND HORIZONTAL SINGLE
	{0x256c, 0xce}, // BOX DRAWINGS DOUBLE VERTICAL AND HORIZONTAL
	{0x2580, 0xdf}, // UPPER HALF BLOCK
	{0x2584, 0xdc}, // LOWER HALF BLOCK
	{0x2588, 0xdb}, // FULL BLOCK
	{0x258c, 0xdd}, // LEFT HALF BLOCK
	{0x2590, 0xde}, // RIGHT HALF BLOCK
	{0x2591, 0xb0}, // LIGHT SHADE
	{0x2592, 0xb1}, // MEDIUM SHADE
	{0x2593, 0xb2}, // DARK SHADE
	{0x25a0, 0xfe}, // BLACK SQUARE
};

static int
codepage_to_ucs2(const uint16_t *convtbl, const uint8_t *src, size_t srclen, 
				 size_t bufsize, uint16_t *unibuf, size_t *unilen)
{
	uint8_t byte;
	size_t n, r;
	
	r = n = MIN(srclen, bufsize/2);
	
	while (r--) {
		byte = *src++;
		if (byte < 0x80)
			*unibuf++ = (uint16_t)byte;
		else
			*unibuf++ = convtbl[byte - 0x80];
	}
	*unilen = n * 2;
	
	return 0;
}

/*
 * UCSTo8BitEncoding
 * Binary searches UCSTo8BitCharMap to find char mapping
 */
static int
UCSTo8BitEncoding(const UCSTo8BitCharMap *theTable, int numElem,
				  uint16_t character, uint8_t *ch)
{
    const UCSTo8BitCharMap *p, *q, *divider;
	
    if ((character < theTable[0]._u) || (character > theTable[numElem-1]._u)) {
        return 0;
    }
    p = theTable;
    q = p + (numElem-1);
    while (p <= q) {
        divider = p + ((q - p) >> 1);
        if (character < divider->_u) { q = divider - 1; }
        else if (character > divider->_u) { p = divider + 1; }
        else { *ch = divider->_c; return 1; }
    }
	return 0;
}

static void 
ucs2_to_codepage(const uint16_t *convtbl, const uint16_t *src, size_t srclen,
				 size_t bufsize, char *buf, size_t *buflen)
{
	uint16_t character;
	uint8_t byte;
	size_t n, r;
	
	r = n = MIN(srclen/2, bufsize);
	
	while (r--) {
		character = *src++;
		if (character < 0x80)
			*buf++ = (uint8_t)character;
		else if (UCSTo8BitEncoding((const UCSTo8BitCharMap *)convtbl, 128,
								   character, &byte))
			*buf++ = byte;
		else
			*buf++ = '_';
	}
	*buflen = n;
}

/*
 * smb_convert_to_network
 *
 * Convert a UTF8 String to a Network String either UTF16 or Code Page 437. This
 * routine should be used when dealing with file type names. We always set the 
 * precomposed flag, so this routine should not be call for non file type names.
 * The calling routine needs to set the UTF_SFM_CONVERSIONS if they want SFM 
 * style mappings for illegal NTFS characters. This routine will handle any 
 * endian issues.
 *
 * NOTE: The UTF_NO_NULL_TERM flags does not apply when to this routine. The old
 *		 code passed this flag in, but it was always ignored.
 */
int 
smb_convert_to_network(const char **inbuf, size_t *inbytesleft, char **outbuf, 
						   size_t *outbytesleft, int flags, int usingUnicode)
{
	int error;
	size_t inlen;
	size_t outlen;
	
	DBG_ASSERT(inbuf);
	DBG_ASSERT(*inbuf);
	DBG_ASSERT(outbuf);
	DBG_ASSERT(*outbuf);
	
	inlen = *inbytesleft;
	outlen = 0;
	
	flags |= UTF_PRECOMPOSED;

	if (usingUnicode) {
		/* Little endian Unicode over the wire */
		if (BYTE_ORDER != LITTLE_ENDIAN)
			flags |= UTF_REVERSE_ENDIAN;
		error = utf8_decodestr((const uint8_t*)*inbuf, inlen, (uint16_t *)*outbuf, 
							   &outlen, *outbytesleft, 0, flags);
		
	} else {
		const uint16_t *cptable = (const uint16_t *)cp437_from_ucs2;
		uint16_t buf[256];	/* When using code pages we only support 256 file names */

		error = utf8_decodestr((const uint8_t*)*inbuf, inlen, buf, &outlen, 
							   sizeof(buf), 0, flags);
		if (!error)
			ucs2_to_codepage(cptable, buf, outlen, *outbytesleft, *outbuf, &outlen);

	}
	if (error)
		return error;
		
	*inbuf += inlen;
	*outbuf += outlen;
	*inbytesleft -= inlen;
	*outbytesleft -= outlen;
	return 0;
}

/*
 * smb_convert_from_network
 *
 * Convert a Network String either UTF16 or Code Page 437 to UTF8 String. This
 * routine should be used when dealing with file type names. We always set the 
 * decomposed flag, so this routine should not be call for non file type names.
 * Currently we always set the UTF_NO_NULL_TERM, may want to change that in the
 * future. The calling routine needs to set the UTF_SFM_CONVERSIONS if they want
 * SFM style mappings for illegal NTFS characters. This routine will handle any 
 * endian issues.
 */
int 
smb_convert_from_network(const char **inbuf, size_t *inbytesleft, char **outbuf, 
							 size_t *outbytesleft, int flags, int usingUnicode)
{
	int error;
	size_t inlen;
	size_t outlen;
	
	DBG_ASSERT(inbuf);
	DBG_ASSERT(*inbuf);
	DBG_ASSERT(outbuf);
	DBG_ASSERT(*outbuf);
	
	inlen = *inbytesleft;
	outlen = 0;
	
	flags |= UTF_DECOMPOSED | UTF_NO_NULL_TERM;
	if (usingUnicode) {
		/* Little endian Unicode over the wire */
		if (BYTE_ORDER != LITTLE_ENDIAN)
			flags |= UTF_REVERSE_ENDIAN;
		error = utf8_encodestr((uint16_t *)*inbuf, inlen, (uint8_t *)*outbuf, &outlen, *outbytesleft, 0, flags);	
	} else {
		const uint16_t *cptable = (const uint16_t *)cp437_to_ucs2;
		uint16_t buf[SMB_MAXFNAMELEN*2];	/* When using code pages we only support 256 file names */

		codepage_to_ucs2(cptable, (uint8_t *)*inbuf, inlen, sizeof(buf), buf, &outlen);
		error = utf8_encodestr(buf, outlen, (uint8_t *)*outbuf, &outlen, *outbytesleft, 0, flags);
	}
	if (error)
		return error;
	
	*inbuf += inlen;
	*outbuf += outlen;
	*inbytesleft -= inlen;
	*outbytesleft -= outlen;
	return 0;
}

/*
 * smb_strtouni
 *
 * Convert a UTF8 String to a UTF16 String. This routine should be used when 
 * dealing with strings that need to go across the wire as UTF16. We never set
 * the precomposed/decomposed flag, so the calling routine should pass in
 * the correct flag.
 *
 * NOTE: The UTF_NO_NULL_TERM flags does not apply when used with this routine.
 *		  The old code passed this flag in, but it was always ignored.
 */
size_t 
smb_strtouni(uint16_t *dst, const char *src, size_t inlen, int flags)
{
	size_t outlen;
	
	if (BYTE_ORDER != LITTLE_ENDIAN)
		flags |= UTF_REVERSE_ENDIAN;
	if (utf8_decodestr((uint8_t *)src, inlen, dst, &outlen, inlen * 2, 0, flags) != 0)
		outlen = 0;
	return (outlen);
}

/*
 * smb_unitostr
 *
 * Converts the network UTF-16 string to a UTF-8 string. This routine should be 
 * used when dealing with strings that have come across the wire as UTF16. We never
 * set the precomposed/decomposed flag, so the calling routine should pass in
 * the correct flag.
 */
size_t 
smb_unitostr(char *dst, const uint16_t *src, size_t inlen, size_t maxlen, int flags)
{
	size_t outlen;
	
	if (BYTE_ORDER != LITTLE_ENDIAN)
		flags |= UTF_REVERSE_ENDIAN;
	
	if (utf8_encodestr(src, inlen, (uint8_t *)dst, &outlen, maxlen, 0, flags) != 0)
		outlen = 0;
	
	return (outlen);
}

/*
 * Does the same thing as strnlen, except on a utf16 string. The n_bytes is the 
 * max number of bytes in the buffer. This routine always return the size in 
 * the number of bytes.
 */
size_t 
smb_utf16_strnsize(const uint16_t *s, size_t n_bytes) 
{
	const uint16_t *es = s, *p = s;
		
	/* Make sure es is on even boundry */	
	es += (n_bytes / 2);
	while(*p && p != es)  {
		p++;
	}
	return (uint8_t *)p - (uint8_t *)s;
}

/*
 * Internal strlchr that checks for buffer overflows.
 */
static void *
smb_strlchr(const void *s, uint8_t ch, size_t max) 
{
	const uint8_t *str = s;
	const uint8_t *es = str + max;
	
	while(*str && (str != es))  { 
		if (*str == ch)
			return (void *)str;
		str++;
	}
	
	return NULL;
}

/*
 * Does the same thing as smb_strlchr, except on a utf16 string.
 */
static void *
smb_utf16_strlchr(const uint16_t *s, uint16_t ch, size_t max) 
{
	const uint16_t *es = s, *str = s;
	
	/* Make sure es is on even boundry */	
	es += (max / 2);
	while((str != es) && *str)  { 
		if (*str == ch)
			return (void *)str;
		str++;
	}
	
	return NULL;
}

static char *
set_network_delimiter(char *network, char ntwrk_delimiter, size_t delimiter_size, 
					  size_t *resid)
{	
	if (*resid < delimiter_size)
		return NULL;
	*resid -= delimiter_size;
	if (delimiter_size == 2) {
		uint16_t *utf16_ptr = (uint16_t *)network;
		
		*utf16_ptr++ = htoles((uint16_t)ntwrk_delimiter);
		return (char *)utf16_ptr;
	} else {
		*network++ = ntwrk_delimiter;
		return network;
	}
}

/* 
 * Given a UTF8 path create a network path
 *
 * path				- A UTF8 string. 
 * max_path_len		- Number of bytes in the path string
 * network			- Either  UTF16 or ASCII string
 * ntwrk_len		- On input max buffer size, on output length of network buffer
 * ntwrk_delimiter	- Delimiter to use
 * inflags			-
 *					  SMB_UTF_SFM_CONVERSIONS - Indicates that we should set the 
 *					  kernel UTF_SFM_CONVERSIONS (Use SFM mappings for illegal NTFS chars)
 *					  flag. 
 *					  SMB_FULLPATH_CONVERSIONS - Indicates they want a full path,
 *					  if the output doesn't start with a delimiter, one should be
 *					  add.
 */
int 
smb_convert_path_to_network(char *path, size_t max_path_len, char *network, 
							size_t *ntwrk_len, char ntwrk_delimiter, int inflags, 
							int usingUnicode)
{
	int error = 0;
	char * delimiter;
	size_t component_len;	/* component length */
	size_t path_resid;
	size_t resid = *ntwrk_len;	/* Room left in the the network buffer */
	size_t delimiter_size = (usingUnicode) ? 2 : 1;
	int flags = (inflags & SMB_UTF_SFM_CONVERSIONS) ? UTF_SFM_CONVERSIONS : 0;
	
	if ((inflags & SMB_FULLPATH_CONVERSIONS) && (*path != '/')) {
		network = set_network_delimiter(network, ntwrk_delimiter, delimiter_size, 
										&resid);
		if (network == NULL)
			return E2BIG;
	}
	
	while (path && resid && max_path_len) {
		DBG_ASSERT(resid > 0);	/* Should never fail */
		/* Find the next delimiter in the utf-8 string */
		delimiter = smb_strlchr(path, '/', max_path_len);
		/* Remove the delimiter so we can get the component */
		if (delimiter) {
			max_path_len -= 1; /* consume the delimiter */
			*delimiter = 0;
		}
		/* Get the size of this component */
		path_resid = component_len = strnlen(path, max_path_len);
		/* Never SFM dot or dotdot */
		if (((component_len == 1) && (*path == '.'))  || 
			((component_len == 2) && (*path == '.') && (*(path+1) == '.'))) {
			error = smb_convert_to_network((const char **)&path, &path_resid, 
										   &network, &resid, 0, usingUnicode);
		
		} else {
			error = smb_convert_to_network((const char **)&path, &path_resid, 
										   &network, &resid, flags, usingUnicode);
		}
		if (error)
			return error;
		/* Put the path delimiter back and move the pointer pass it */
		if (delimiter)
			*delimiter++ = '/';
		path = delimiter;
		/* Remove the amount that was consumed by smb_convert_to_network */
		max_path_len -= (component_len - path_resid);
		/* If we have more to process then add a network delimiter */
		if (path) {
			network = set_network_delimiter(network, ntwrk_delimiter, 
											delimiter_size, &resid);
			if (network == NULL)
				return E2BIG;
		}
	}
	*ntwrk_len -= resid;
	DBG_ASSERT((ssize_t)(*ntwrk_len) >= 0);
	return error;
}

/* 
 * Given a network string path create a UTF8 path
 *
 * network			- Either UTF16 or ASCII string
 * max_ntwrk_len	- Number of bytes in the network string
 * path				- UTF8 string. 
 * path_len			- On input max buffer size, on output length of UTF8 string
 * ntwrk_delimiter	- Delimiter to use
 * inflags			-
 *					  SMB_UTF_SFM_CONVERSIONS - Indicates that we should set the 
 *					  kernel UTF_SFM_CONVERSIONS (Use SFM mappings for illegal NTFS chars)
 *					  flag. 
 *					  SMB_FULLPATH_CONVERSIONS - Indicates they want a full path,
 *					  if the output doesn't start with a delimiter, one should be
 *					  add. (Not currently supported, if required should be added.
 */
int 
smb_convert_network_to_path(char *network, size_t max_ntwrk_len, char *path, 
							size_t *path_len, char ntwrk_delimiter, int flags, 
							int usingUnicode)
{
	int error = 0;
	char * delimiter;
	size_t component_len;	/* component length*/
	size_t resid = *path_len;	/* Room left in the the path buffer */
	size_t ntwrk_resid;
	
	while (network && resid && max_ntwrk_len) {
		DBG_ASSERT(resid > 0);	/* Should never fail */
		/* Find the next delimiter in the network string */
		if (usingUnicode) {
			delimiter = smb_utf16_strlchr((const uint16_t *)network, 
										 htoles((uint16_t)ntwrk_delimiter), 
										 max_ntwrk_len);
			/* Remove the delimiter so we can get the component */
			if (delimiter) {
				max_ntwrk_len -= 2; /* consume the delimiter */
				*((uint16_t *)delimiter) = 0;
			}			/* Get the size of this component */
			component_len = smb_utf16_strnsize((const uint16_t *)network, max_ntwrk_len);

		} else {
			delimiter = smb_strlchr((const uint8_t *)network, ntwrk_delimiter, max_ntwrk_len);
			/* Remove the delimiter so we can get the component */
			if (delimiter) {
				max_ntwrk_len -= 1; /* consume the delimiter */
				*((uint8_t *)delimiter) = 0;
			}
			/* Get the size of this component */
			component_len = strnlen(network, max_ntwrk_len);
		}
		ntwrk_resid = component_len; /* Amount that we want them to consume */
		error = smb_convert_from_network((const char **)&network, &ntwrk_resid, 
										 &path, &resid, flags, usingUnicode);
		if (error) {
			SMBDEBUG("smb_convert_from_network = %d\n", error);
			return error;
		}
		/* Put the network delimiter back and move the pointer pass it */
		if (delimiter) {
			if (usingUnicode) {
				*((uint16_t *)delimiter) = htoles(ntwrk_delimiter);
				delimiter++;
			} else {
				*((uint8_t *)delimiter) = ntwrk_delimiter;
			}
			delimiter++;
		}
		network = delimiter;
		/* Remove the amount that was consumed by smb_convert_from_network */
		max_ntwrk_len -= (component_len - ntwrk_resid);
		/* If we have more to process then add a UNIX delimiter */
		if (network) {
			if (!resid)
				return E2BIG;
			resid -= 1;
			*path++ = '/';	
		}
	}
	*path_len -= resid;
	DBG_ASSERT((ssize_t)(*path_len) >= 0);
	return error;
}
