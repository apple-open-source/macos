/* ===EmacsMode: -*- Mode: C; tab-width:4; c-basic-offset: 4; -*- === */
/* ===FileName: ===

   GEORGIAN encodings to unicode table

   Copyright (c) 1999 Takuya SHIOZAKI, All Rights reserved.
   Copyright (c) 1999 Pablo Saratxaga <srtxg@chanae.alphanet.ch>

===Notice
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
   SUCH DAMAGE.

   Major Release ID: X-TrueType Server Version 1.3 [Aoi MATSUBARA Release 3]

Notice===

    -georgian-academy is the encoding used by academic world and university
    of Georgia; also known as "de facto RS encoding", widely used on web
    servers; it follows same ordering as unicode.
    -georgian-ps is the "Parliament-Soros found" encoding.

 */

#include "xttversion.h"

static char const * const releaseID =
    _XTT_RELEASE_NAME;

#include "xttcommon.h"
#include "xttcap.h"
#include "xttcconv.h"
#include "xttcconvP.h"

#define ALTCHR 0x0020

static ucs2_t tblGeorgianRSToUcs2[] = {
/* 0x00A0 - 0x00FF */
    0x00a0, 0x00a1, 0x00a2, 0x00a3, 0x00a4, 0x00a5, 0x00a6, 0x00a7,
    0x00a8, 0x00a9, 0x00aa, 0x00ab, 0x00ac, 0x00ad, 0x00ae, 0x00af,
    0x00b0, 0x00b1, 0x00b2, 0x00b3, 0x00b4, 0x00b5, 0x00b6, 0x00b7,
    0x00b8, 0x00b9, 0x00ba, 0x00bb, 0x00bc, 0x00bd, 0x00be, 0x00bf,
    0x10d0, 0x10d1, 0x10d2, 0x10d3, 0x10d4, 0x10d5, 0x10d6, 0x10d7,
    0x10d8, 0x10d9, 0x10da, 0x10db, 0x10dc, 0x10dd, 0x10de, 0x10df,
    0x10e0, 0x10e1, 0x10e2, 0x10e3, 0x10e4, 0x10e5, 0x10e6, 0x10e7,
    0x10e8, 0x10e9, 0x10ea, 0x10eb, 0x10ec, 0x10ed, 0x10ee, 0x10ef,
    0x10f0, 0x10f1, 0x10f2, 0x10f3, 0x10f4, 0x10f5, 0x10f6, 0x00ab,
    0x00bb, 0x2018, 0x2019, 0x201e, 0x201f, ALTCHR, ALTCHR, ALTCHR,
    0xf182, 0xf183, 0xf184, 0xf185, 0xf186, 0xf187, 0xf188, 0xf189,
    0xf18a, 0xf18b, 0xf18c, 0x221e, 0x2264, 0x2265, 0xf1d0, 0xf1d1
};

static ucs2_t tblGeorgianStdToUcs2[] = {
/* 0x00A0 - 0x00FF */
    0x00a0, 0x00a1, 0x00a2, 0x00a3, 0x00a4, 0x00a5, 0x00a6, 0x00a7,
    0x00a8, 0x00a9, 0x00aa, 0x00ab, 0x00ac, 0x00ad, 0x00ae, 0x00af,
    0x00b0, 0x00b1, 0x00b2, 0x00b3, 0x00b4, 0x00b5, 0x00b6, 0x00b7,
    0x00b8, 0x00b9, 0x00ba, 0x00bb, 0x00bc, 0x00bd, 0x00be, 0x00bf,
    0x10d0, 0x10d1, 0x10d2, 0x10d3, 0x10d4, 0x10d5, 0x10d6, 0x10f1,
    0x10d7, 0x10d8, 0x10d9, 0x10da, 0x10db, 0x10dc, 0x10f2, 0x10dd,
    0x10de, 0x10df, 0x10e0, 0x10e1, 0x10e2, 0x10f3, 0x10e3, 0x10e4,
    0x10e5, 0x10e6, 0x10e7, 0x10e8, 0x10e9, 0x10ea, 0x10eb, 0x10ec,
    0x10ed, 0x10ee, 0x10f4, 0x10ef, 0x10f0, 0x10f5, 0x10f6, 0x00ab,
    0x00bb, 0x2018, 0x2019, 0x201e, 0x201f, ALTCHR, ALTCHR, ALTCHR,
    0xf182, 0xf183, 0xf184, 0xf185, 0xf186, 0xf187, 0xf188, 0xf189,
    0xf18a, 0xf18b, 0xf18c, 0x221e, 0x2264, 0x2265, 0xf1d0, 0xf1d1
};

CODE_CONV_ISO8859_TO_UCS2(cc_georgian_rs_to_ucs2, /* function name */
                          tblGeorgianRSToUcs2, /* table name */
                          ALTCHR /* alt char code (on UCS2) */
                          )

CODE_CONV_ISO8859_TO_UCS2(cc_georgian_std_to_ucs2, /* function name */
                          tblGeorgianStdToUcs2, /* table name */
                          ALTCHR /* alt char code (on UCS2) */
                          )

/* end of file */
