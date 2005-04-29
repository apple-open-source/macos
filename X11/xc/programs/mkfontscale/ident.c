/*
  Copyright (c) 2003 by Juliusz Chroboczek

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
/* $XFree86: xc/programs/mkfontscale/ident.c,v 1.4 2003/10/24 20:38:13 tsi Exp $ */

/* The function identifyBitmap returns -1 if filename is definitively not
   a font file, 1 if it is a single-face bitmap font with a XLFD name,
   and 0 if it should be processed normally.  identifyBitmap is
   much faster than parsing the whole font. */

#include <stdlib.h>
#include <string.h>
#include "zlib.h"

#define PCF_VERSION (('p'<<24)|('c'<<16)|('f'<<8)|1)
#define PCF_PROPERTIES (1 << 0)

typedef struct _Prop {
    unsigned name;
    int isString;
    unsigned value;
} PropRec, *PropPtr;

static int pcfIdentify(gzFile f, char **name);
static int bdfIdentify(gzFile f, char **name);

static int
getLSB32(gzFile f)
{
    int rc;
    unsigned char c[4];

    rc = gzread(f, c, 4);
    if(rc != 4)
        return -1;
    return (c[0]) | (c[1] << 8) | (c[2] << 16) | (c[3] << 24);
}

static int
getInt8(gzFile f, int format)
{
    unsigned char c;
    int rc;

    rc = gzread(f, &c, 1);
    if(rc != 1)
        return -1;
    return c;
}

static int
getInt32(gzFile f, int format)
{
    int rc;
    unsigned char c[4];

    rc = gzread(f, c, 4);
    if(rc != 4)
        return -1;

    if(format & (1 << 2)) {
        return (c[0] << 24) | (c[1] << 16) | (c[2] << 8) | (c[3]);
    } else {
        return (c[0]) | (c[1] << 8) | (c[2] << 16) | (c[3] << 24);
    }
}

static int
pcfskip(gzFile f, int n)
{
    char buf[32];
    int i, rc;
    while(n > 0) {
        i = (n > 32 ? 32 : n);
        rc = gzread(f, buf, i);
        if(rc != i)
            return -1;
        n -= rc;
    }
    return 1;
}

int 
bitmapIdentify(char *filename, char **name)
{
    gzFile f;
    int magic;

    f = gzopen(filename, "rb");
    if(f == NULL)
        return -1;

    magic = getLSB32(f);
    if(magic == PCF_VERSION)
        return pcfIdentify(f, name);
    else if(magic == ('S' | ('T' << 8) | ('A' << 16) | ('R') << 24))
        return bdfIdentify(f, name);

    gzclose(f);
    return 0;
}

static int
pcfIdentify(gzFile f, char **name)
{
    int prop_position;
    PropPtr props = NULL;
    int format, count, nprops, i, string_size, rc;
    char *strings = NULL, *s;

    count = getLSB32(f);
    if(count <= 0)
        goto fail;

    prop_position = -1;
    for(i = 0; i < count; i++) {
        int type, offset;
        type = getLSB32(f);
        (void) getLSB32(f);
        (void) getLSB32(f);
        offset = getLSB32(f);
        if(type == PCF_PROPERTIES) {
            prop_position = offset;
            break;
        }
    }
    if(prop_position < 0)
        goto fail;

    rc = gzseek(f, prop_position, SEEK_SET);
    if(rc < 0)
        goto fail;
    
    format = getLSB32(f);
    if((format & 0xFFFFFF00) != 0)
        goto fail;
    nprops = getInt32(f, format);
    if(nprops <= 0 || nprops > 1000)
        goto fail;
    props = malloc(nprops * sizeof(PropRec));
    if(props == NULL)
        goto fail;

    for(i = 0; i < nprops; i++) {
        props[i].name = getInt32(f, format);
        props[i].isString = getInt8(f, format);
        props[i].value = getInt32(f, format);
    }
    if(nprops & 3) {
        rc = pcfskip(f, 4 - (nprops & 3));
        if(rc < 0)
            goto fail;
    }

    string_size = getInt32(f, format);
    if(string_size < 0 || string_size > 100000)
        goto fail;
    strings = malloc(string_size);
    if(!strings)
        goto fail;

    rc = gzread(f, strings, string_size);
    if(rc != string_size)
        goto fail;

    for(i = 0; i < nprops; i++) {
        if(!props[i].isString ||
           props[i].name >= string_size - 4 ||
           props[i].value >= string_size)
            continue;
        if(strcmp(strings + props[i].name, "FONT") == 0)
            break;
    }

    if(i >= nprops)
        goto fail;

    s = malloc(strlen(strings + props[i].value) + 1);
    if(s == NULL)
        goto fail;
    strcpy(s, strings + props[i].value);
    *name = s;
    free(strings);
    free(props);
    gzclose(f);
    return 1;

 fail:
    if(strings) free(strings);
    if(props) free(props);
    gzclose(f);
    return 0;
}

#define NKEY 20

static char*
getKeyword(gzFile *f, int *eol)
{
    static char keyword[NKEY + 1];
    int c, i;
    i = 0;
    while(i < NKEY) {
        c = gzgetc(f);
        if(c == ' ' || c == '\n') {
            if(i <= 0)
                return NULL;
            if(eol)
                *eol = (c == '\n');
            keyword[i] = '\0';
            return keyword;
        }
        if(c < 'A' || c > 'Z')
            return NULL;
        keyword[i++] = c;
    }
    return NULL;
}

static int
bdfskip(gzFile *f)
{
    int c;
    do {
        c = gzgetc(f);
    } while(c >= 0 && c != '\n');
    if(c < 0)
        return -1;
    return 1;
}

static char *
bdfend(gzFile *f)
{
    int c;
    char *buf = NULL;
    int bufsize = 0;
    int i = 0;

    do {
        c = gzgetc(f);
    } while (c == ' ');

    while(i < 1000) {
        if(c < 0 || (c == '\n' && i == 0)) {
            goto fail;
        }
        if(bufsize < i + 1) {
            char *newbuf;
            if(bufsize == 0) {
                bufsize = 20;
                newbuf = malloc(bufsize);
            } else {
                bufsize = 2 * bufsize;
                newbuf = realloc(buf, bufsize);
            }
            if(newbuf == NULL)
                goto fail;
            buf = newbuf;
        }
        if(c == '\n') {
            buf[i] = '\0';
            return buf;
        }
        buf[i++] = c;
        c = gzgetc(f);
    }

 fail:
    if(buf)
        free(buf);
    return NULL;
}

static int
bdfIdentify(gzFile f, char **name)
{
    char *k;
    int rc;
    int eol;
    /* bitmapIdentify already read "STAR", so we need to check for
       "TFONT" */
    k = getKeyword(f, &eol);
    if(k == NULL || eol)
        goto fail;
    if(strcmp(k, "TFONT") != 0)
        goto fail;
    while(1) {
        if(!eol) {
            rc = bdfskip(f);
            if(rc < 0) 
                goto fail;
        }
        k = getKeyword(f, &eol);
        if(k == NULL)
            goto fail;
        else if(strcmp(k, "FONT") == 0) {
            if(eol)
                goto fail;
            k = bdfend(f);
            if(k == NULL)
                goto fail;
            *name = k;
            gzclose(f);
            return 1;
        } else if(strcmp(k, "CHARS") == 0)
            goto fail;
    }
 fail:
    gzclose(f);
    return 0;
}
