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

#ifndef LUIT_ISO2022_H
#define LUIT_ISO2022_H 1

#include "charset.h"

#define ESC    0x1B
#define CSI    0x9B
#define CSI_7  '['
#define SS2    0x8E
#define SS2_7  0x4E
#define SS3    0x8F
#define SS3_7  0x4F
#define LS0    0x0F
#define LS1    0x0E
#define LS2_7  0x6E
#define LS3_7  0x6F
#define LS1R_7 0x7E
#define LS2R_7 0x7D
#define LS3R_7 0x7C

#define IS_FINAL_ESC(x) (((x) & 0xF0 ) != 0x20)
#define IS_FINAL_CSI(x) (((x) & 0xF0 ) != 0x20 && (((x) & 0xF0 ) != 0x30))

#define P_NORMAL    0
#define P_ESC       1
#define P_CSI       2

#define S_NORMAL    0
#define S_SS2       1
#define S_SS3       2

#define IF_SS       1
#define IF_LS       2
#define IF_EIGHTBIT 4
#define IF_SSGR     8

#define OF_SS       1
#define OF_LS       2
#define OF_SELECT   4
#define OF_PASSTHRU 8

typedef struct _Iso2022 {
    const CharsetRec **glp;
    const CharsetRec **grp;
    const CharsetRec *g[4];
    const CharsetRec *other;
    int parserState;
    int shiftState;
    int inputFlags;
    int outputFlags;
    unsigned char *buffered;
    size_t buffered_len;
    size_t buffered_count;
    int buffered_ku;
    unsigned char *outbuf;
    size_t outbuf_count;
} Iso2022Rec, *Iso2022Ptr;

#define GL(i) (*(i)->glp)
#define GR(i) (*(i)->grp)
#define G0(i) ((i)->g[0])
#define G1(i) ((i)->g[1])
#define G2(i) ((i)->g[2])
#define G3(i) ((i)->g[3])
#define OTHER(i) ((i)->other)

#define BUFFER_SIZE 512

Iso2022Ptr allocIso2022(void);
void destroyIso2022(Iso2022Ptr);
int initIso2022(const char *, const char *, Iso2022Ptr);
int mergeIso2022(Iso2022Ptr, Iso2022Ptr);
void reportIso2022(Iso2022Ptr);
void terminate(Iso2022Ptr, int);
void terminateEsc(Iso2022Ptr, int, unsigned char *, unsigned);
void copyIn(Iso2022Ptr, int, unsigned char *, int);
void copyOut(Iso2022Ptr, int, unsigned char *, unsigned);

#endif /* LUIT_ISO2022_H */
