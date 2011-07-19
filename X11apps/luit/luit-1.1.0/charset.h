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
/* $XFree86: xc/programs/luit/charset.h,v 1.4 2002/10/17 01:06:09 dawes Exp $ */

#ifndef LUIT_CHARSET_H
#define LUIT_CHARSET_H 1

#include "other.h"

#define T_FAILED 0
#define T_94 1
#define T_96 2
#define T_128 3
#define T_9494 4
#define T_9696 5
/* Big 5 */
#define T_94192 6
#define T_OTHER 7

/* True for charsets that pass control chars unchanged, at least in
   the first byte */
#define CHARSET_REGULAR(c) ((c)->type != T_128)

typedef struct _Charset {
    const char *name;
    int type;
    unsigned char final;
    unsigned int (*recode) (unsigned int, const struct _Charset * self);
    int (*reverse) (unsigned int, const struct _Charset * self);
    const void *data;
    int (*other_stack) (unsigned c, OtherStatePtr aux);
    OtherState *other_aux;
    unsigned int (*other_recode) (unsigned int c, OtherStatePtr aux);
    unsigned int (*other_reverse) (unsigned int c, OtherStatePtr aux);
    struct _Charset *next;
} CharsetRec, *CharsetPtr;

typedef struct _LocaleCharset {
    const char *name;
    int gl;
    int gr;
    const char *g0;
    const char *g1;
    const char *g2;
    const char *g3;
    const char *other;
} LocaleCharsetRec, *LocaleCharsetPtr;

const CharsetRec *getUnknownCharset(int);
const CharsetRec *getCharset(unsigned, int);
const CharsetRec *getCharsetByName(const char *);
void reportCharsets(void);
int getLocaleState(const char *locale, const char *charset,
		   int *gl_return, int *gr_return,
		   const CharsetRec * *g0_return,
		   const CharsetRec * *g1_return,
		   const CharsetRec * *g2_return,
		   const CharsetRec * *g3_return,
		   const CharsetRec * *other_return);

#endif /* LUIT_CHARSET_H */
