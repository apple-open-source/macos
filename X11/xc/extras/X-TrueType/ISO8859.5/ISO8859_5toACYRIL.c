/* ===EmacsMode: -*- Mode: C; tab-width:4; c-basic-offset: 4; -*- === */
/* ===FileName: ===
   Copyright (c) 1998 Takuya SHIOZAKI, All Rights reserved.
   Copyright (c) 1998 X-TrueType Server Project, All rights reserved.

   This table from xfsft:
     Copyright (c) 1997 by Mark Leisher
     Copyright (c) 1998 by Juliusz Chroboczek

   
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

 */
/* $XFree86: xc/extras/X-TrueType/ISO8859.5/ISO8859_5toACYRIL.c,v 1.2 2003/10/22 16:25:41 tsi Exp $ */

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

static ft_char_code_t tblIso8859_5ToAppleCyrillic[] = {
/* 0x00A0 - 0x00FF */
    0x00CA, 0x00DD, 0x00AB, 0x00AE, 0x00B8, 0x00C1, 0x00A7, 0x00BA,
    0x00B7, 0x00BC, 0x00BE, 0x00CB, 0x00CD, ALTCHR, 0x00D8, 0x00DA,
    0x0080, 0x0081, 0x0082, 0x0083, 0x0084, 0x0085, 0x0086, 0x0087,
    0x0088, 0x0089, 0x008A, 0x008B, 0x008C, 0x008D, 0x008E, 0x008F,
    0x0090, 0x0091, 0x0092, 0x0093, 0x0094, 0x0095, 0x0096, 0x0097,
    0x0098, 0x0099, 0x009A, 0x009B, 0x009C, 0x009D, 0x009E, 0x009F,
    0x00E0, 0x00E1, 0x00E2, 0x00E3, 0x00E4, 0x00E5, 0x00E6, 0x00E7,
    0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x00EC, 0x00ED, 0x00EE, 0x00EF,
    0x00F0, 0x00F1, 0x00F2, 0x00F3, 0x00F4, 0x00F5, 0x00F6, 0x00F7,
    0x00F8, 0x00F9, 0x00FA, 0x00FB, 0x00FC, 0x00FD, 0x00FE, 0x00DF,
    0x00DC, 0x00DE, 0x00AC, 0x00AF, 0x00B9, 0x00CF, 0x00B4, 0x00BB,
    0x00C0, 0x00BD, 0x00BF, 0x00CC, 0x00CE, 0x00A4, 0x00D9, 0x00DB
};

CODE_CONV_ISO8859_TO_UCS2(cc_iso8859_5_to_apple_cyrillic, /* function name */
                          tblIso8859_5ToAppleCyrillic, /* table name */
                          ALTCHR /* alt char code (on ARoman) */
                          )

/* end of file */
