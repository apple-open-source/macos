/* ===EmacsMode: -*- Mode: C; tab-width:4; c-basic-offset: 4; -*- === */
/* ===FileName: ===

   Copyright (c) 1998 Takuya SHIOZAKI, All Rights reserved.
   Copyright (c) 1999 Pablo Saratxaga <pablo@mandrakesoft.com>
   
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

   This table data derived from Unicode, Inc.
   (ftp://ftp.unicode.org/Public/MAPPINGS/ISO8859/8859-8.TXT)

 */
/* $XFree86: xc/extras/X-TrueType/ISO8859.8/ISO8859_8toUCS2.c,v 1.3 2003/10/22 16:25:42 tsi Exp $ */

#include "xttversion.h"

#if 0
static char const * const releaseID =
    _XTT_RELEASE_NAME;
#endif

#include "xttcommon.h"
#include "xttcap.h"
#include "xttcconv.h"
#include "xttcconvP.h"

#define ALTCHR 0x0020

static ucs2_t tblIso8859_8ToUcs2[] = {
/* 0x00A0 - 0x00FF */
    0x00A0, ALTCHR, 0x00A2, 0x00A3, 0x00A4, 0x00A5, 0x00A6, 0x00A7, 
    0x00A8, 0x00A9, 0x00D7, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x203E, 
    0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x00B4, 0x00B5, 0x00B6, 0x00B7, 
    0x00B8, 0x00B9, 0x00F7, 0x00BB, 0x00BC, 0x00BD, 0x00BE, ALTCHR, 
    ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, 
    ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, 
    ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, 
    ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, 0x2017, 
    0x05D0, 0x05D1, 0x05D2, 0x05D3, 0x05D4, 0x05D5, 0x05D6, 0x05D7, 
    0x05D8, 0x05D9, 0x05DA, 0x05DB, 0x05DC, 0x05DD, 0x05DE, 0x05DF, 
    0x05E0, 0x05E1, 0x05E2, 0x05E3, 0x05E4, 0x05E5, 0x05E6, 0x05E7, 
    0x05E8, 0x05E9, 0x05EA, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, 
};

static ucs2_t tblCp1255ToUcs2[] = {
    0x20ac, ALTCHR, 0x201a, 0x0192, 0x201e, 0x2026, 0x2020, 0x2021,
    0x02c6, 0x2030, ALTCHR, 0x2039, ALTCHR, ALTCHR, ALTCHR, ALTCHR,
    ALTCHR, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014,
    0x02dc, 0x2122, ALTCHR, 0x203a, ALTCHR, ALTCHR, ALTCHR, ALTCHR,
/* 0x00A0 - 0x00FF */
    0x00A0, 0x00A1, 0x00A2, 0x00A3, 0x20AA, 0x00A5, 0x00A6, 0x00A7,
    0x00A8, 0x00A9, 0x00D7, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x00AF,
    0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x00B4, 0x00B5, 0x00B6, 0x00B7,
    0x00B8, 0x00B9, 0x00F7, 0x00BB, 0x00BC, 0x00BD, 0x00BE, 0x00BF,
    0x05b0, 0x05b1, 0x05b2, 0x05b3, 0x05b4, 0x05b5, 0x05b6, 0x05b7,
    0x05b8, 0x05b9, 0x05ba, 0x05bb, 0x05bc, 0x05bd, 0x05be, 0x05bf,
    0x05c0, 0x05c1, 0x05c2, 0x05c3, 0x05f0, 0x05f1, 0x05f2, 0x05f3,
    0x05f4, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, 0x2017,
    0x05D0, 0x05D1, 0x05D2, 0x05D3, 0x05D4, 0x05D5, 0x05D6, 0x05D7,
    0x05D8, 0x05D9, 0x05DA, 0x05DB, 0x05DC, 0x05DD, 0x05DE, 0x05DF,
    0x05E0, 0x05E1, 0x05E2, 0x05E3, 0x05E4, 0x05E5, 0x05E6, 0x05E7,
    0x05E8, 0x05E9, 0x05EA, ALTCHR, ALTCHR, 0x200e, 0x200f, ALTCHR,
};

CODE_CONV_ISO8859_TO_UCS2(cc_iso8859_8_to_ucs2, /* function name */
                          tblIso8859_8ToUcs2, /* table name */
                          ALTCHR /* alt char code (on UCS2) */
                          )

CODE_CONV_CP_TO_UCS2(cc_cp1255_to_ucs2, /* function name */
                     tblCp1255ToUcs2) /* table name */

/* end of file */
