/*
 * Copyright (c) 2001 - 2007 Apple Inc. All rights reserved.
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
/*
 * Copyright (c) 2001 Apple Computer
 * All rights reserved.
 *
 */
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/smb_apple.h>
#include <sys/utfconv.h>
#include <sys/smb_iconv.h>

#include "iconv_converter_if.h"

#define HARDCODE_CP437	1


/* UCS2 to CodePage Conversion Data */
typedef struct _UCSTo8BitCharMap {
    u_int16_t _u;
    u_int8_t _c;
} UCSTo8BitCharMap;


#ifdef HARDCODE_CP437
/* CodePage to UCS2 Conversion Data */
static const u_int16_t cp437_to_ucs2[128] = {
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
#endif /* HARDCODE_CP437 */


static int
codepage_to_ucs2(const u_int16_t *convtbl, const u_int8_t *src, size_t srclen,
		size_t bufsize, u_int16_t *unibuf, size_t *unilen)
{
	u_int8_t byte;
	int n, r;

	r = n = min(srclen, bufsize/2);

	while (r--) {
		byte = *src++;
		if (byte < 0x80)
			*unibuf++ = (u_int16_t)byte;
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
		u_int16_t character, u_int8_t *ch)
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

static int
ucs2_to_codepage(const u_int16_t *convtbl, const u_int16_t *src, size_t srclen,
		size_t bufsize, char *buf, size_t *buflen)
{
	u_int16_t character;
	u_int8_t byte;
	int n, r;

	r = n = min(srclen/2, bufsize);

	while (r--) {
		character = *src++;
		if (character < 0x80)
			*buf++ = (u_int8_t)character;
		else if (UCSTo8BitEncoding((const UCSTo8BitCharMap *)convtbl, 128,
					character, &byte))
			*buf++ = byte;
		else
			*buf++ = '_';
	}
	*buflen = n;
	
	return 0;
}


/*
 * Codepage converter instance
 */
struct iconv_codepage {
	KOBJ_FIELDS;
	int d_type;
	void* d_convtbl;
	struct iconv_cspair *d_csp;
};

enum {
	ICONV_TOLOCAL = 1,
	ICONV_TOSERVER = 2
};

static int
iconv_codepage_open(struct iconv_converter_class *dcp,
	struct iconv_cspair *csp, struct iconv_cspair *cspf, void **dpp)
{
	#pragma unused(cspf)
	struct iconv_codepage *dp;

	dp = (struct iconv_codepage *)kobj_create((struct kobj_class*)dcp, M_ICONV);
	if (strncmp(csp->cp_to, "utf-8", ICONV_CSNMAXLEN) == 0) {
		dp->d_convtbl = (void *)cp437_to_ucs2;
		dp->d_type = ICONV_TOLOCAL;
	} else if (strncmp(csp->cp_from, "utf-8", ICONV_CSNMAXLEN) == 0) {
		dp->d_convtbl = (void *)cp437_from_ucs2;
		dp->d_type = ICONV_TOSERVER;
	} else {
		kobj_delete((struct kobj*)dp, M_ICONV);
		return 1;
	}
	dp->d_csp = csp;
	csp->cp_refcount++;
	*dpp = (void*)dp;
	return 0;
}

static int
iconv_codepage_close(void *data)
{
	struct iconv_codepage *dp = data;

	dp->d_csp->cp_refcount--;
	kobj_delete((struct kobj*)data, M_ICONV);
	return 0;
}

static int
iconv_codepage_conv(void *d2p, const char **inbuf, size_t *inbytesleft, char **outbuf,
		size_t *outbytesleft, int flags)
{
	struct iconv_codepage *dp = (struct iconv_codepage*)d2p;
	size_t inlen;
	size_t outlen;
	u_int16_t buf[256];
	int error;

	if (inbuf == NULL || *inbuf == NULL || outbuf == NULL || *outbuf == NULL)
		return 0;

	inlen = *inbytesleft;
	outlen = 0;
	
	if (dp->d_type == ICONV_TOLOCAL) {
		codepage_to_ucs2((const u_int16_t *)dp->d_convtbl, (u_int8_t *)*inbuf,
				inlen, sizeof(buf), buf, &outlen);
		error = utf8_encodestr(buf, outlen, (u_int8_t *)*outbuf, &outlen, *outbytesleft,
				0, UTF_DECOMPOSED | UTF_NO_NULL_TERM | flags);
	} else if (dp->d_type == ICONV_TOSERVER) {
		error = utf8_decodestr((u_int8_t *)*inbuf, inlen, buf, &outlen, sizeof(buf),
				0, UTF_PRECOMPOSED | flags);
		if (error == 0) {
			ucs2_to_codepage((const u_int16_t *)dp->d_convtbl, buf, outlen,
					*outbytesleft, *outbuf, &outlen);
		}
	} else {
		error = -1;
	}
	if (error)
		return (error);

	*inbuf += inlen;
	*outbuf += outlen;
	*inbytesleft -= inlen;
	*outbytesleft -= outlen;
	return 0;
}

static const char *
iconv_codepage_name(struct iconv_converter_class *dcp)
{
	#pragma unused(dcp)
	return "codepage";
}

static kobj_method_t iconv_codepage_methods[] = {
	KOBJMETHOD(iconv_converter_open,	iconv_codepage_open),
	KOBJMETHOD(iconv_converter_close,	iconv_codepage_close),
	KOBJMETHOD(iconv_converter_conv,	iconv_codepage_conv),
	KOBJMETHOD(iconv_converter_name,	iconv_codepage_name),
	{0, 0}
};

KICONV_CONVERTER(codepage, sizeof(struct iconv_codepage));
