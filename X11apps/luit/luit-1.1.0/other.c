/*
Copyright (c) 2002 by Tomohiro KUBOTA

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "other.h"

#ifndef NULL
#define NULL 0
#endif

#define EURO_10646 0x20AC

int
init_gbk(OtherStatePtr s)
{
    s->gbk.mapping =
	FontEncMapFind("gbk-0", FONT_ENCODING_UNICODE, -1, -1, NULL);
    if (!s->gbk.mapping)
	return 0;

    s->gbk.reverse = FontMapReverse(s->gbk.mapping);
    if (!s->gbk.reverse)
	return 0;

    s->gbk.buf = -1;
    return 1;
}

unsigned int
mapping_gbk(unsigned int n, OtherStatePtr s)
{
    unsigned int r;
    if (n < 128)
	return n;
    if (n == 128)
	return EURO_10646;
    r = FontEncRecode(n, s->gbk.mapping);
    return r;
}

unsigned int
reverse_gbk(unsigned int n, OtherStatePtr s)
{
    if (n < 128)
	return n;
    if (n == EURO_10646)
	return 128;
    return s->gbk.reverse->reverse(n, s->gbk.reverse->data);
}

int
stack_gbk(unsigned c, OtherStatePtr s)
{
    if (s->gbk.buf < 0) {
	if (c < 129)
	    return (int) c;
	s->gbk.buf = (int) c;
	return -1;
    } else {
	int b;
	if (c < 0x40 || c == 0x7F) {
	    s->gbk.buf = -1;
	    return (int) c;
	}
	if (s->gbk.buf < 0xFF && c < 0xFF)
	    b = (int) ((unsigned) (s->gbk.buf << 8) + c);
	else
	    b = -1;
	s->gbk.buf = -1;
	return b;
    }
}

int
init_utf8(OtherStatePtr s)
{
    s->utf8.buf_ptr = 0;
    return 1;
}

unsigned int
mapping_utf8(unsigned int n, OtherStatePtr s GCC_UNUSED)
{
    return n;
}

unsigned int
reverse_utf8(unsigned int n, OtherStatePtr s GCC_UNUSED)
{
    if (n < 0x80)
	return n;
    if (n < 0x800)
	return 0xC080 + ((n & 0x7C0) << 2) + (n & 0x3F);
    if (n < 0x10000)
	return 0xE08080 + ((n & 0xF000) << 4) + ((n & 0xFC0) << 2) + (n & 0x3F);
    return 0xF0808080 + ((n & 0x1C0000) << 6) + ((n & 0x3F000) << 4) +
	((n & 0xFC0) << 2) + (n & 0x3F);
}

int
stack_utf8(unsigned c, OtherStatePtr s)
{
    int u;

    if (c < 0x80) {
	s->utf8.buf_ptr = 0;
	return (int) c;
    }
    if (s->utf8.buf_ptr == 0) {
	if ((c & 0x40) == 0)
	    return -1;
	s->utf8.buf[s->utf8.buf_ptr++] = UChar(c);
	if ((c & 0x60) == 0x40)
	    s->utf8.len = 2;
	else if ((c & 0x70) == 0x60)
	    s->utf8.len = 3;
	else if ((c & 0x78) == 0x70)
	    s->utf8.len = 4;
	else
	    s->utf8.buf_ptr = 0;
	return -1;
    }
    if ((c & 0x40) != 0) {
	s->utf8.buf_ptr = 0;
	return -1;
    }
    s->utf8.buf[s->utf8.buf_ptr++] = UChar(c);
    if (s->utf8.buf_ptr < s->utf8.len)
	return -1;
    switch (s->utf8.len) {
    case 2:
	u = ((s->utf8.buf[0] & 0x1F) << 6) | (s->utf8.buf[1] & 0x3F);
	s->utf8.buf_ptr = 0;
	if (u < 0x80)
	    return -1;
	else
	    return u;
    case 3:
	u = ((s->utf8.buf[0] & 0x0F) << 12)
	    | ((s->utf8.buf[1] & 0x3F) << 6)
	    | (s->utf8.buf[2] & 0x3F);
	s->utf8.buf_ptr = 0;
	if (u < 0x800)
	    return -1;
	else
	    return u;
    case 4:
	u = ((s->utf8.buf[0] & 0x03) << 18)
	    | ((s->utf8.buf[1] & 0x3F) << 12)
	    | ((s->utf8.buf[2] & 0x3F) << 6)
	    | ((s->utf8.buf[3] & 0x3F));
	s->utf8.buf_ptr = 0;
	if (u < 0x10000)
	    return -1;
	else
	    return u;
    }
    s->utf8.buf_ptr = 0;
    return -1;
}

#define HALFWIDTH_10646 0xFF61
#define YEN_SJIS 0x5C
#define YEN_10646 0x00A5
#define OVERLINE_SJIS 0x7E
#define OVERLINE_10646 0x203E

int
init_sjis(OtherStatePtr s)
{
    s->sjis.x0208mapping =
	FontEncMapFind("jisx0208.1990-0", FONT_ENCODING_UNICODE, -1, -1, NULL);
    if (!s->sjis.x0208mapping)
	return 0;

    s->sjis.x0208reverse = FontMapReverse(s->sjis.x0208mapping);
    if (!s->sjis.x0208reverse)
	return 0;

    s->sjis.x0201mapping =
	FontEncMapFind("jisx0201.1976-0", FONT_ENCODING_UNICODE, -1, -1, NULL);
    if (!s->sjis.x0201mapping)
	return 0;

    s->sjis.x0201reverse = FontMapReverse(s->sjis.x0201mapping);
    if (!s->sjis.x0201reverse)
	return 0;

    s->sjis.buf = -1;
    return 1;
}

unsigned int
mapping_sjis(unsigned int n, OtherStatePtr s)
{
    unsigned int j1, j2, s1, s2;
    if (n == YEN_SJIS)
	return YEN_10646;
    if (n == OVERLINE_SJIS)
	return OVERLINE_10646;
    if (n < 0x80)
	return n;
    if (n >= 0xA0 && n <= 0xDF)
	return FontEncRecode(n, s->sjis.x0201mapping);
    s1 = ((n >> 8) & 0xFF);
    s2 = (n & 0xFF);
    j1 = (s1 << 1)
	- (unsigned) (s1 <= 0x9F ? 0xE0 : 0x160)
	- (unsigned) (s2 < 0x9F ? 1 : 0);
    j2 = s2
	- 0x1F
	- (unsigned) (s2 >= 0x7F ? 1 : 0)
	- (unsigned) (s2 >= 0x9F ? 0x5E : 0);
    return FontEncRecode((j1 << 8) + j2, s->sjis.x0208mapping);
}

unsigned int
reverse_sjis(unsigned int n, OtherStatePtr s)
{
    unsigned int j, j1, j2, s1, s2;
    if (n == YEN_10646)
	return YEN_SJIS;
    if (n == OVERLINE_10646)
	return OVERLINE_SJIS;
    if (n < 0x80)
	return n;
    if (n >= HALFWIDTH_10646)
	return s->sjis.x0201reverse->reverse(n, s->sjis.x0201reverse->data);
    j = s->sjis.x0208reverse->reverse(n, s->sjis.x0208reverse->data);
    j1 = ((j >> 8) & 0xFF);
    j2 = (j & 0xFF);
    s1 = ((j1 - 1) >> 1)
	+ (unsigned) ((j1 <= 0x5E) ? 0x71 : 0xB1);
    s2 = j2
	+ (unsigned) ((j1 & 1) ? ((j2 < 0x60) ? 0x1F : 0x20) : 0x7E);
    return (s1 << 8) + s2;
}

int
stack_sjis(unsigned c, OtherStatePtr s)
{
    if (s->sjis.buf < 0) {
	if (c < 128 || (c >= 0xA0 && c <= 0xDF))
	    return (int) c;
	s->sjis.buf = (int) c;
	return -1;
    } else {
	int b;
	if (c < 0x40 || c == 0x7F) {
	    s->sjis.buf = -1;
	    return (int) c;
	}
	if (s->sjis.buf < 0xFF && c < 0xFF)
	    b = (int) ((unsigned) (s->sjis.buf << 8) + c);
	else
	    b = -1;
	s->sjis.buf = -1;
	return b;
    }
}

int
init_hkscs(OtherStatePtr s)
{
    s->hkscs.mapping =
	FontEncMapFind("big5hkscs-0", FONT_ENCODING_UNICODE, -1, -1, NULL);
    if (!s->hkscs.mapping)
	return 0;

    s->hkscs.reverse = FontMapReverse(s->hkscs.mapping);
    if (!s->hkscs.reverse)
	return 0;

    s->hkscs.buf = -1;
    return 1;
}

unsigned int
mapping_hkscs(unsigned int n, OtherStatePtr s)
{
    unsigned int r;
    if (n < 128)
	return n;
    if (n == 128)
	return EURO_10646;
    r = FontEncRecode(n, s->hkscs.mapping);
    return r;
}

unsigned int
reverse_hkscs(unsigned int n, OtherStatePtr s)
{
    if (n < 128)
	return n;
    if (n == EURO_10646)
	return 128;
    return s->hkscs.reverse->reverse(n, s->hkscs.reverse->data);
}

int
stack_hkscs(unsigned c, OtherStatePtr s)
{
    if (s->hkscs.buf < 0) {
	if (c < 129)
	    return (int) c;
	s->hkscs.buf = (int) c;
	return -1;
    } else {
	int b;
	if (c < 0x40 || c == 0x7F) {
	    s->hkscs.buf = -1;
	    return (int) c;
	}
	if (s->hkscs.buf < 0xFF && c < 0xFF)
	    b = (int) ((unsigned) (s->hkscs.buf << 8) + c);
	else
	    b = -1;
	s->hkscs.buf = -1;
	return b;
    }
}

/*
 *  Because of the 1 ~ 4 multi-bytes nature of GB18030.
 *  CharSet encoding is split to 2 subset (besides latin)
 *  The 2Bytes MB char is defined in gb18030.2000-0
 *  The 4Bytes MB char is defined in gb18030.2000-1
 *  Please note that the mapping in 2000-1 is not a 4Bytes seq => 2Bytes value
 *  mapping.
 *  To use the 2000-1 we need to 'linear' the 4Bytes sequence and 'lookup' the 
 *  unicode value after that.
 *
 *  For more info on GB18030 standard pls check:
 *    http://oss.software.ibm.com/icu/docs/papers/gb18030.html
 *
 *  For more info on GB18030 implementation issues in XFree86 pls check:
 *    http://www.ibm.com/developerWorks/cn/linux/i18n/gb18030/xfree86/part1
 */
int
init_gb18030(OtherStatePtr s)
{
    s->gb18030.cs0_mapping =
	FontEncMapFind("gb18030.2000-0", FONT_ENCODING_UNICODE, -1, -1, NULL);
    if (!s->gb18030.cs0_mapping)
	return 0;

    s->gb18030.cs0_reverse = FontMapReverse(s->gb18030.cs0_mapping);
    if (!s->gb18030.cs0_reverse)
	return 0;

    s->gb18030.cs1_mapping =
	FontEncMapFind("gb18030.2000-1", FONT_ENCODING_UNICODE, -1, -1, NULL);
    if (!s->gb18030.cs1_mapping)
	return 0;

    s->gb18030.cs1_reverse = FontMapReverse(s->gb18030.cs1_mapping);
    if (!s->gb18030.cs1_reverse)
	return 0;

    s->gb18030.linear = 0;
    s->gb18030.buf_ptr = 0;
    return 1;
}

unsigned int
mapping_gb18030(unsigned int n, OtherStatePtr s)
{
    if (n <= 0x80)
	return n;		/* 0x80 is valid but unassigned codepoint */
    if (n >= 0xFFFF)
	return '?';

    return FontEncRecode(n,
			 (s->gb18030.linear) ? s->gb18030.cs1_mapping : s->gb18030.cs0_mapping);
}

unsigned int
reverse_gb18030(unsigned int n, OtherStatePtr s)
{
    /* when lookup in 2000-0 failed. */
    /* lookup in 2000-1 and then try to unlinear'd */
    unsigned int r;
    if (n <= 0x80)
	return n;

    r = s->gb18030.cs0_reverse->reverse(n, s->gb18030.cs0_reverse->data);
    if (r != 0)
	return r;

    r = s->gb18030.cs1_reverse->reverse(n, s->gb18030.cs1_reverse->data);
    if (r != 0) {
	unsigned char bytes[4];

	bytes[3] = UChar(0x30 + r % 10);
	r /= 10;
	bytes[2] = UChar(0x81 + r % 126);
	r /= 126;
	bytes[1] = UChar(0x30 + r % 10);
	r /= 10;
	bytes[0] = UChar(0x81 + r);

	r = (unsigned int) bytes[0] << 24;
	r |= (unsigned int) bytes[1] << 16;
	r |= (unsigned int) bytes[2] << 8;
	r |= (unsigned int) bytes[3];
    }
    return r;
}

int
stack_gb18030(unsigned c, OtherStatePtr s)
{
    /* if set gb18030.linear => True. the return value is "linear'd" */
    if (s->gb18030.buf_ptr == 0) {
	if (c <= 0x80)
	    return (int) c;
	if (c == 0xFF)
	    return -1;
	s->gb18030.linear = 0;
	s->gb18030.buf[s->gb18030.buf_ptr++] = (int) c;
	return -1;
    } else if (s->gb18030.buf_ptr == 1) {
	if (c >= 0x40) {
	    s->gb18030.buf_ptr = 0;
	    if ((c == 0x80) || (c == 0xFF))
		return -1;
	    else
		return (int) ((unsigned) (s->gb18030.buf[0] << 8) + c);
	} else if (c >= 30) {	/* 2Byte is (0x30 -> 0x39) */
	    s->gb18030.buf[s->gb18030.buf_ptr++] = (int) c;
	    return -1;
	} else {
	    s->gb18030.buf_ptr = 0;
	    return (int) c;
	}
    } else if (s->gb18030.buf_ptr == 2) {
	if ((c >= 0x81) && (c <= 0xFE)) {
	    s->gb18030.buf[s->gb18030.buf_ptr++] = (int) c;
	    return -1;
	} else {
	    s->gb18030.buf_ptr = 0;
	    return (int) c;
	}
    } else {
	int r = 0;
	s->gb18030.buf_ptr = 0;
	if ((c >= 0x30) && (c <= 0x39)) {
	    s->gb18030.linear = 1;
	    r = (((s->gb18030.buf[0] - 0x81) * 10
		  + (s->gb18030.buf[1] - 0x30)) * 126
		 + (s->gb18030.buf[2] - 0x81)) * 10
		+ ((int) c - 0x30);
	    return r;
	}
	return -1;
    }
}
