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
/* $XFree86: xc/programs/luit/other.c,v 1.1 2002/10/17 01:06:09 dawes Exp $ */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <X11/fonts/fontenc.h>
#include "other.h"
#include "charset.h"

#ifndef NULL
#define NULL 0
#endif

#define EURO_10646 0x20AC

int
init_gbk(OtherStatePtr s)
{
    s->gbk.mapping =
        FontEncMapFind("gbk-0", FONT_ENCODING_UNICODE, -1, -1, NULL);
    if(!s->gbk.mapping) return 0;

    s->gbk.reverse = FontMapReverse(s->gbk.mapping);
    if(!s->gbk.reverse) return 0;

    s->gbk.buf = -1;
    return 1;
}

unsigned int
mapping_gbk(unsigned int n, OtherStatePtr s)
{
    unsigned int r;
    if(n < 128) return n;
    if(n == 128) return EURO_10646;
    r = FontEncRecode(n, s->gbk.mapping);
    return r;
}

unsigned int
reverse_gbk(unsigned int n, OtherStatePtr s)
{
    if(n < 128) return n;
    if(n == EURO_10646) return 128;
    return s->gbk.reverse->reverse(n, s->gbk.reverse->data);
}

int
stack_gbk(unsigned char c, OtherStatePtr s)
{
    if(s->gbk.buf < 0) {
        if(c < 129) return c;
        s->gbk.buf = c;
	return -1;
    } else {
        int b;
        if(c < 0x40 || c == 0x7F) {
            s->gbk.buf = -1;
            return c;
        }
        if(s->gbk.buf < 0xFF && c < 0xFF)
            b = (s->gbk.buf << 8) + c;
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
mapping_utf8(unsigned int n, OtherStatePtr s)
{
    return n;
}

unsigned int
reverse_utf8(unsigned int n, OtherStatePtr s)
{
    if(n < 0x80)
        return n;
    if(n < 0x800)
        return 0xC080 + ((n&0x7C0)<<2) + (n&0x3F);
    if(n < 0x10000)
        return 0xE08080 + ((n&0xF000)<<4) + ((n&0xFC0)<<2) + (n&0x3F);
    return 0xF0808080 + ((n&0x1C0000)<<6) + ((n&0x3F000)<<4) +
           ((n&0xFC0)<<2) + (n&0x3F);
}

int
stack_utf8(unsigned char c, OtherStatePtr s)
{
    int u;

    if(c < 0x80) {
        s->utf8.buf_ptr = 0;
        return c;
    }
    if(s->utf8.buf_ptr == 0) {
        if((c & 0x40) == 0) return -1;
        s->utf8.buf[s->utf8.buf_ptr++] = c;
        if((c & 0x60) == 0x40) s->utf8.len = 2;
        else if((c & 0x70) == 0x60) s->utf8.len = 3;
        else if((c & 0x78) == 0x70) s->utf8.len = 4;
        else s->utf8.buf_ptr = 0;
        return -1;
    }
    if((c & 0x40) != 0) {
        s->utf8.buf_ptr = 0;
        return -1;
    }
    s->utf8.buf[s->utf8.buf_ptr++] = c;
    if(s->utf8.buf_ptr < s->utf8.len) return -1;
    switch(s->utf8.len) {
    case 2:
        u = ((s->utf8.buf[0] & 0x1F) << 6) | (s->utf8.buf[1] & 0x3F);
        s->utf8.buf_ptr = 0;
        if(u < 0x80) return -1; else return u;
    case 3:
        u = ((s->utf8.buf[0] & 0x0F) << 12)
            | ((s->utf8.buf[1] & 0x3F) << 6)
            | (s->utf8.buf[2] & 0x3F);
        s->utf8.buf_ptr = 0;
        if(u < 0x800) return -1; else return u;
    case 4:
        u = ((s->utf8.buf[0] & 0x03) << 18)
            | ((s->utf8.buf[1] & 0x3F) << 12)
            | ((s->utf8.buf[2] & 0x3F) << 6)
            | ((s->utf8.buf[3] & 0x3F));
        s->utf8.buf_ptr = 0;
        if(u < 0x10000) return -1; else return u;
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
    if(!s->sjis.x0208mapping) return 0;

    s->sjis.x0208reverse = FontMapReverse(s->sjis.x0208mapping);
    if(!s->sjis.x0208reverse) return 0;

    s->sjis.x0201mapping =
        FontEncMapFind("jisx0201.1976-0", FONT_ENCODING_UNICODE, -1, -1, NULL);
    if(!s->sjis.x0201mapping) return 0;

    s->sjis.x0201reverse = FontMapReverse(s->sjis.x0201mapping);
    if(!s->sjis.x0201reverse) return 0;

    s->sjis.buf = -1;
    return 1;
}

unsigned int
mapping_sjis(unsigned int n, OtherStatePtr s)
{
    unsigned int j1, j2, s1, s2;
    if(n == YEN_SJIS) return YEN_10646;
    if(n == OVERLINE_SJIS) return OVERLINE_10646;
    if(n < 0x80) return n;
    if(n >= 0xA0 && n <= 0xDF) return FontEncRecode(n, s->sjis.x0201mapping);
    s1 = ((n>>8)&0xFF);
    s2 = (n&0xFF);
    j1 = (s1 << 1) - (s1 <= 0x9F ? 0xE0 : 0x160) - (s2 < 0x9F ? 1 : 0);
    j2 = s2 - 0x1F - (s2 >= 0x7F ? 1 : 0) - (s2 >= 0x9F ? 0x5E : 0);
    return FontEncRecode((j1<<8) + j2, s->sjis.x0208mapping);
}

unsigned int
reverse_sjis(unsigned int n, OtherStatePtr s)
{
    unsigned int j, j1, j2, s1, s2;
    if(n == YEN_10646) return YEN_SJIS;
    if(n == OVERLINE_10646) return OVERLINE_SJIS;
    if(n < 0x80) return n;
    if(n >= HALFWIDTH_10646)
        return s->sjis.x0201reverse->reverse(n, s->sjis.x0201reverse->data);
    j = s->sjis.x0208reverse->reverse(n, s->sjis.x0208reverse->data);
    j1 = ((j>>8)&0xFF);
    j2 = (j&0xFF);
    s1 = ((j1 - 1) >> 1) + ((j1 <= 0x5E) ? 0x71 : 0xB1);
    s2 = j2 + ((j1 & 1) ? ((j2 < 0x60) ? 0x1F : 0x20) : 0x7E);
    return (s1<<8) + s2;
}

int
stack_sjis(unsigned char c, OtherStatePtr s)
{
    if(s->sjis.buf < 0) {
        if(c < 128 || (c >= 0xA0 && c <= 0xDF)) return c;
        s->sjis.buf = c;
	return -1;
    } else {
        int b;
        if(c < 0x40 || c == 0x7F) {
            s->sjis.buf = -1;
            return c;
        }
        if(s->sjis.buf < 0xFF && c < 0xFF)
            b = (s->sjis.buf << 8) + c;
        else
            b = -1;
        s->sjis.buf = -1;
        return b;
    }
}

