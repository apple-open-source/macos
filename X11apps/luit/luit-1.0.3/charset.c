/*
Copyright (c) 2001 by Juliusz Chroboczek

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
/* $XFree86: xc/programs/luit/charset.c,v 1.8 2003/12/22 17:48:12 tsi Exp $ */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <X11/fonts/fontenc.h>
#include "other.h"
#include "charset.h"
#include "parser.h"

#ifndef NULL
#define NULL 0
#endif

static unsigned int
IdentityRecode(unsigned int n, CharsetPtr self)
{
    return n;
}

#ifdef UNUSED
static int
IdentityReverse(unsigned int n, CharsetPtr self)
{
#define IS_GL(n) ((n) >= 0x20 && (n) < 0x80)
    switch(self->type) {
    case T_94:
    case T_96:
        if (IS_GL(n)) return n; else return -1;
    case T_128:
        if (n < 0x80) return n; else return -1;
    case T_9494:
    case T_9696:
        if(IS_GL(n>>8) && IS_GL(n&0xFF))
            return n;
        else
            return -1;
    case T_94192:
        if(IS_GL(n>>8) && IS_GL(n&0x7F))
            return n;
        else
            return -1;
    default:
        abort();
    }
#undef IS_GL
}
#endif

static int
NullReverse(unsigned int n, CharsetPtr self)
{
    return -1;
}

CharsetRec Unknown94Charset = 
{ "Unknown (94)", T_94, 0, IdentityRecode, NullReverse, NULL, NULL};
CharsetRec Unknown96Charset = 
{ "Unknown (96)", T_96, 0, IdentityRecode, NullReverse, NULL, NULL};
CharsetRec Unknown9494Charset = 
{ "Unknown (94x94)", T_9494, 0, IdentityRecode, NullReverse, NULL, NULL};
CharsetRec Unknown9696Charset = 
{ "Unknown (96x96)", T_9696, 0, IdentityRecode, NullReverse, NULL, NULL};

typedef struct _FontencCharset {
    char *name;
    int type;
    unsigned char final;
    char *xlfd;
    int shift;
    FontMapPtr mapping;
    FontMapReversePtr reverse;
} FontencCharsetRec, *FontencCharsetPtr;

FontencCharsetRec fontencCharsets[] = {
    {"ISO 646 (1973)", T_94, '@', "iso646.1973-0", 0x00, NULL, NULL},
    {"ASCII", T_94, 'B', "iso8859-1", 0x00, NULL, NULL},
    {"JIS X 0201:GL", T_94, 'J', "jisx0201.1976-0", 0x00, NULL, NULL},
    {"JIS X 0201:GR", T_94, 'I', "jisx0201.1976-0", 0x80, NULL, NULL},
    {"DEC Special", T_94, '0', "dec-special", 0x00, NULL, NULL},
    {"DEC Technical", T_94, '>', "dec-dectech", 0x00, NULL, NULL},

    {"ISO 8859-1", T_96, 'A', "iso8859-1", 0x80, NULL, NULL},
    {"ISO 8859-2", T_96, 'B', "iso8859-2", 0x80, NULL, NULL},
    {"ISO 8859-3", T_96, 'C', "iso8859-3", 0x80, NULL, NULL},
    {"ISO 8859-4", T_96, 'D', "iso8859-4", 0x80, NULL, NULL},
    {"ISO 8859-5", T_96, 'L', "iso8859-5", 0x80, NULL, NULL},
    {"ISO 8859-6", T_96, 'G', "iso8859-6", 0x80, NULL, NULL},
    {"ISO 8859-7", T_96, 'F', "iso8859-7", 0x80, NULL, NULL},
    {"ISO 8859-8", T_96, 'H', "iso8859-8", 0x80, NULL, NULL},
    {"ISO 8859-9", T_96, 'M', "iso8859-9", 0x80, NULL, NULL},
    {"ISO 8859-10", T_96, 'V', "iso8859-10", 0x80, NULL, NULL},
    {"ISO 8859-11", T_96, 'T', "iso8859-11", 0x80, NULL, NULL},
    {"TIS 620", T_96, 'T', "iso8859-11", 0x80, NULL, NULL},
    {"ISO 8859-13", T_96, 'Y', "iso8859-13", 0x80, NULL, NULL},
    {"ISO 8859-14", T_96, '_', "iso8859-14", 0x80, NULL, NULL},
    {"ISO 8859-15", T_96, 'b', "iso8859-15", 0x80, NULL, NULL},
    {"ISO 8859-16", T_96, 'f', "iso8859-16", 0x80, NULL, NULL},
    {"KOI8-E", T_96, '@', "koi8-e", 0x80, NULL, NULL},
    {"TCVN", T_96, 'Z', "tcvn-0", 0x80, NULL, NULL},

    {"GB 2312", T_9494, 'A', "gb2312.1980-0", 0x0000, NULL, NULL},
    {"JIS X 0208", T_9494, 'B', "jisx0208.1990-0", 0x0000, NULL, NULL},
    {"KSC 5601", T_9494, 'C', "ksc5601.1987-0", 0x0000, NULL, NULL},
    {"JIS X 0212", T_9494, 'D', "jisx0212.1990-0", 0x0000, NULL, NULL},

    {"GB 2312", T_9696, 'A', "gb2312.1980-0", 0x0000, NULL, NULL},
    {"JIS X 0208", T_9696, 'B', "jisx0208.1990-0", 0x0000, NULL, NULL},
    {"KSC 5601", T_9696, 'C', "ksc5601.1987-0", 0x0000, NULL, NULL},
    {"JIS X 0212", T_9696, 'D', "jisx0212.1990-0", 0x0000, NULL, NULL},

    {"KOI8-R", T_128, 0, "koi8-r", 0x80, NULL, NULL},
    {"KOI8-U", T_128, 0, "koi8-u", 0x80, NULL, NULL},
    {"KOI8-RU", T_128, 0, "koi8-ru", 0x80, NULL, NULL},
    {"CP 1252", T_128, 0, "microsoft-cp1252", 0x80, NULL, NULL},
    {"CP 1251", T_128, 0, "microsoft-cp1251", 0x80, NULL, NULL},
    {"CP 1250", T_128, 0, "microsoft-cp1250", 0x80, NULL, NULL},

    {"CP 437", T_128, 0, "ibm-cp437", 0x80, NULL, NULL},
    {"CP 850", T_128, 0, "ibm-cp850", 0x80, NULL, NULL},
    {"CP 866", T_128, 0, "ibm-cp866", 0x80, NULL, NULL},

    {"Big 5", T_94192, 0, "big5.eten-0", 0x8000, NULL, NULL},
    {NULL, 0, 0, NULL, 0, NULL, NULL}
};

typedef struct _OtherCharset {
    char *name;
    int (*init)(OtherStatePtr);
    unsigned int (*mapping)(unsigned int, OtherStatePtr);
    unsigned int (*reverse)(unsigned int, OtherStatePtr);
    int (*stack)(unsigned char, OtherStatePtr);
} OtherCharsetRec, *OtherCharsetPtr;

OtherCharsetRec otherCharsets[] = {
    {"GBK", init_gbk, mapping_gbk, reverse_gbk, stack_gbk},
    {"UTF-8", init_utf8, mapping_utf8, reverse_utf8, stack_utf8},
    {"SJIS", init_sjis, mapping_sjis, reverse_sjis, stack_sjis},
    {"BIG5-HKSCS", init_hkscs, mapping_hkscs, reverse_hkscs, stack_hkscs},
    {"GB18030", init_gb18030, mapping_gb18030, reverse_gb18030, stack_gb18030},
    {NULL, NULL, NULL, NULL, NULL}
};

static int
compare(const char *s, const char *t)
{
    while(*s || *t) {
        if(*s && (isspace(*s) || *s == '-' || *s == '_'))
            s++;
        else if(*t && (isspace(*t) || *t == '-' || *t == '_'))
            t++;
        else if(*s && *t && tolower(*s) == tolower(*t)) {
            s++; 
            t++;
        } else
            return 1;
    }
    return 0;
}

static unsigned int
FontencCharsetRecode(unsigned int n, CharsetPtr self)
{
    FontencCharsetPtr fc = (FontencCharsetPtr)(self->data);

    return FontEncRecode(n + fc->shift, fc->mapping);
}

static int
FontencCharsetReverse(unsigned int i, CharsetPtr self)
{
    FontencCharsetPtr fc = (FontencCharsetPtr)(self->data);
    int n;

    n = fc->reverse->reverse(i, fc->reverse->data);
    if(n == 0 || n < fc->shift)
        return -1;
    else
        n -= fc->shift;

#define IS_GL(n) ((n) >= 0x20 && (n) < 0x80)
    switch(self->type) {
    case T_94: case T_96:
        if (IS_GL(n)) return n; else return -1;
        break;
    case T_128:
        if (n < 0x80) return n; else return -1;
    case T_9494: case T_9696:
        if(IS_GL(n>>8) && IS_GL(n&0xFF))
            return n;
        else
            return -1;
        break;
    case T_94192:
        if(IS_GL(n>>8) && IS_GL(n&0x7F))
            return n;
        else
            return -1;
        break;
    default:
        abort();
    }
#undef IS_GL
}


static CharsetPtr cachedCharsets = NULL;

static CharsetPtr 
getCachedCharset(unsigned char final, int type, const char *name)
{
    CharsetPtr c;
    for(c = cachedCharsets; c; c = c->next) {
        if(((c->type == type && c->final == final) ||
            (name && !compare(c->name, name))) &&
           (c->type != T_FAILED))
            return c;
    }
    return NULL;
}

static void
cacheCharset(CharsetPtr c) {
    c->next = cachedCharsets;
    cachedCharsets = c;
}

static CharsetPtr
getFontencCharset(unsigned char final, int type, const char *name)
{
    FontencCharsetPtr fc;
    CharsetPtr c;
    FontMapPtr mapping;
    FontMapReversePtr reverse;

    fc = fontencCharsets;
    while(fc->name) {
        if(((fc->type == type && fc->final == final) ||
            (name && !compare(fc->name, name))) &&
           (fc->type != T_FAILED))
            break;
        fc++;
    }

    if(!fc->name)
        return NULL;

    c = malloc(sizeof(CharsetRec));
    if(c == NULL)
        return NULL;

    mapping = FontEncMapFind(fc->xlfd, FONT_ENCODING_UNICODE, -1, -1, NULL);
    if(!mapping) {
        fc->type = T_FAILED;
        return NULL;
    }

    reverse = FontMapReverse(mapping);
    if(!reverse) {
        fc->type = T_FAILED;
        return NULL;
    }

    fc->mapping = mapping;
    fc->reverse = reverse;

    c->name = fc->name;
    c->type = fc->type;
    c->final = fc->final;
    c->recode = FontencCharsetRecode;
    c->reverse = FontencCharsetReverse;
    c->data = fc;

    cacheCharset(c);
    return c;
}

static CharsetPtr
getOtherCharset(const char *name)
{
    OtherCharsetPtr fc;
    CharsetPtr c;
    OtherStatePtr s;

    fc = otherCharsets;
    while(fc->name) {
        if(name && !compare(fc->name, name))
            break;
        fc++;
    }

    if(!fc->name)
        return NULL;

    c = malloc(sizeof(CharsetRec));
    if(c == NULL)
        return NULL;

    s = malloc(sizeof(OtherState));
    if(s == NULL) {
        free(c);
        return NULL;
    }

    c->name = fc->name;
    c->type = T_OTHER;
    c->final = 0;
    c->data = fc;
    c->other_recode = fc->mapping;
    c->other_reverse = fc->reverse;
    c->other_stack = fc->stack;
    c->other_aux = s;

    if(!fc->init(s)) {
        c->type = T_FAILED;
        return NULL;
    }

    cacheCharset(c);
    return c;
}

CharsetPtr 
getUnknownCharset(int type)
{
    switch(type) {
    case T_94: return &Unknown94Charset;
    case T_96: return &Unknown96Charset;
    case T_9494: return &Unknown9494Charset;
    case T_9696: return &Unknown9696Charset;
    default: return &Unknown94Charset;
    }
}

CharsetPtr 
getCharset(unsigned char final, int type)
{
    CharsetPtr c;

    c = getCachedCharset(final, type, NULL);
    if(c)
        return c;

    c = getFontencCharset(final, type, NULL);
    if(c)        
        return c;

    return getUnknownCharset(type);
}

CharsetPtr 
getCharsetByName(const char *name)
{
    CharsetPtr c;

    if(name == NULL)
        return getUnknownCharset(T_94);

    c = getCachedCharset(0, 0, name);
    if(c)
        return c;

    c = getFontencCharset(0, 0, name);
    if(c)        
        return c;

    c = getOtherCharset(name);
    if(c)        
        return c;

    return getUnknownCharset(T_94);
}

const LocaleCharsetRec localeCharsets[] = {
    { "C", 0, 2, "ASCII", NULL, "ISO 8859-1", NULL, NULL},
    { "POSIX", 0, 2, "ASCII", NULL, "ISO 8859-1", NULL, NULL},
    { "ISO8859-1", 0, 2, "ASCII", NULL, "ISO 8859-1", NULL, NULL},
    { "ISO8859-2", 0, 2, "ASCII", NULL, "ISO 8859-2", NULL, NULL},
    { "ISO8859-3", 0, 2, "ASCII", NULL, "ISO 8859-3", NULL, NULL},
    { "ISO8859-4", 0, 2, "ASCII", NULL, "ISO 8859-4", NULL, NULL},
    { "ISO8859-5", 0, 2, "ASCII", NULL, "ISO 8859-5", NULL, NULL},
    { "ISO8859-6", 0, 2, "ASCII", NULL, "ISO 8859-6", NULL, NULL},
    { "ISO8859-7", 0, 2, "ASCII", NULL, "ISO 8859-7", NULL, NULL},
    { "ISO8859-8", 0, 2, "ASCII", NULL, "ISO 8859-8", NULL, NULL},
    { "ISO8859-9", 0, 2, "ASCII", NULL, "ISO 8859-9", NULL, NULL},
    { "ISO8859-10", 0, 2, "ASCII", NULL, "ISO 8859-10", NULL, NULL},
    { "ISO8859-11", 0, 2, "ASCII", NULL, "ISO 8859-11", NULL, NULL},
    { "TIS620", 0, 2, "ASCII", NULL, "ISO 8859-11", NULL, NULL},
    { "ISO8859-13", 0, 2, "ASCII", NULL, "ISO 8859-13", NULL, NULL},
    { "ISO8859-14", 0, 2, "ASCII", NULL, "ISO 8859-14", NULL, NULL},
    { "ISO8859-15", 0, 2, "ASCII", NULL, "ISO 8859-15", NULL, NULL},
    { "ISO8859-16", 0, 2, "ASCII", NULL, "ISO 8859-16", NULL, NULL},
    { "KOI8-R", 0, 2, "ASCII", NULL, "KOI8-R", NULL, NULL},
    { "CP1251", 0, 2, "ASCII", NULL, "CP 1251", NULL, NULL},
    { "TCVN", 0, 2, "ASCII", NULL, "TCVN", NULL, NULL},
    { "eucCN", 0, 1, "ASCII", "GB 2312", NULL, NULL, NULL},
    { "GB2312", 0, 1, "ASCII", "GB 2312", NULL, NULL, NULL},
    { "eucJP", 0, 1, "ASCII", "JIS X 0208", "JIS X 0201:GR", "JIS X 0212", NULL},
    { "eucKR", 0, 1, "ASCII", "KSC 5601", NULL, NULL, NULL},
    { "eucCN", 0, 1, "ASCII", "GB 2312", NULL, NULL, NULL},
    { "Big5", 0, 1, "ASCII", "Big 5", NULL, NULL, NULL},
    { "gbk", 0, 1, NULL, NULL, NULL, NULL, "GBK"},
    { "UTF-8", 0, 1, NULL, NULL, NULL, NULL, "UTF-8"},
    { "SJIS", 0, 1, NULL, NULL, NULL, NULL, "SJIS"},
    { "Big5-HKSCS", 0, 1, NULL, NULL, NULL, NULL, "BIG5-HKSCS"},
    { "gb18030", 0, 1, NULL, NULL, NULL, NULL, "GB18030"},
    { NULL, 0, 0, NULL, NULL, NULL, NULL, NULL}
};

void
reportCharsets(void)
{
    const LocaleCharsetRec *p;
    FontencCharsetPtr q;
    printf("Known locale encodings:\n\n");
    for(p = localeCharsets; p->name; p++) {
        if(p->other) {
            printf("  %s (non-ISO-2022 encoding)\n", p->other);
	    continue;
        }
        printf("  %s: GL -> G%d, GR -> G%d", p->name, p->gl, p->gr);
        if(p->g0) printf(", G0: %s", p->g0);
        if(p->g1) printf(", G1: %s", p->g1);
        if(p->g2) printf(", G2: %s", p->g2);
        if(p->g3) printf(", G3: %s", p->g3);
        printf("\n");
    }

    printf("\n\nKnown charsets (not all may be available):\n\n");
    for(q = fontencCharsets; q->name; q++)
        printf("  %s%s\n", 
               q->name, q->final?" (ISO 2022)":"");
}

int
getLocaleState(const char *locale, char *charset,
               int *gl_return, int *gr_return,
               CharsetPtr *g0_return, CharsetPtr *g1_return,
               CharsetPtr *g2_return, CharsetPtr *g3_return,
               CharsetPtr *other_return)
{
    char *resolved = NULL;
    const LocaleCharsetRec *p;

    if(!charset) {
        resolved = resolveLocale(locale);
        if(!resolved)
            return -1;
        charset = strrchr(resolved, '.');
        if(charset)
            charset++;
        else
            charset = resolved;
    }

    for(p = localeCharsets; p->name; p++) {
        if(compare(p->name, charset) == 0)
            break;
    }

    if(p->name == NULL) {
	if (resolved != 0)
	    free(resolved);
        return -1;
    }

    *gl_return = p->gl;
    *gr_return = p->gr;
    *g0_return = getCharsetByName(p->g0);
    *g1_return = getCharsetByName(p->g1);
    *g2_return = getCharsetByName(p->g2);
    *g3_return = getCharsetByName(p->g3);
    if(p->other)
        *other_return = getCharsetByName(p->other);
    else
        *other_return = NULL;
    return 0;
}

