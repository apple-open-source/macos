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
/* $XFree86: xc/programs/luit/other.h,v 1.1 2002/10/17 01:06:09 dawes Exp $ */

typedef struct {
    FontMapPtr mapping;
    FontMapReversePtr reverse;
    int buf;
} aux_gbk;

typedef struct {
    unsigned char buf[4];
    int buf_ptr, len;
} aux_utf8;

typedef struct {
    FontMapPtr x0208mapping;
    FontMapPtr x0201mapping;
    FontMapReversePtr x0208reverse;
    FontMapReversePtr x0201reverse;
    int buf;
} aux_sjis;

typedef union {
    aux_gbk gbk;
    aux_utf8 utf8;
    aux_sjis sjis;
} OtherState, *OtherStatePtr;

int init_gbk(OtherStatePtr);
unsigned int mapping_gbk(unsigned int, OtherStatePtr);
unsigned int reverse_gbk(unsigned int, OtherStatePtr);
int stack_gbk(unsigned char, OtherStatePtr);

int init_utf8(OtherStatePtr);
unsigned int mapping_utf8(unsigned int, OtherStatePtr);
unsigned int reverse_utf8(unsigned int, OtherStatePtr);
int stack_utf8(unsigned char, OtherStatePtr);

int init_sjis(OtherStatePtr);
unsigned int mapping_sjis(unsigned int, OtherStatePtr);
unsigned int reverse_sjis(unsigned int, OtherStatePtr);
int stack_sjis(unsigned char, OtherStatePtr);

