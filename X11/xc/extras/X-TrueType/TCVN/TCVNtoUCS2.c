/* ===EmacsMode: -*- Mode: C; tab-width:4; c-basic-offset: 4; -*- === */
/* ===FileName: ===

   TCVN (Vietnamese) to unicode table

   Copyright (c) 1998 Takuya SHIOZAKI, All Rights reserved.
   Copyright (c) 1998 Pablo Saratxaga <srtxg@chanae.alphanet.ch>

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

    Thans to Nguyen-Dai Quy for the description of this encoding.

    Contrary to the VISCII encoding doesn't have uppercase letters for
    each lower case one; but as it is the official encoding used in VietNam
    it should be supported.
    Note that no low range positions (first char is at 0x02) are used,
    so the iso-8859-* macros can be used.

 */

#include "xttversion.h"

static char const * const releaseID =
    _XTT_RELEASE_NAME;

#include "xttcommon.h"
#include "xttcap.h"
#include "xttcconv.h"
#include "xttcconvP.h"

#define ALTCHR 0x0020

static ucs2_t tblTCVNToUcs2[] = {
/* 0x00A0 - 0x00FF */
    ALTCHR, 0x0102, 0x00c2, 0x00ca, 0x00d4, 0x01a0, 0x01af, 0x0110,
    0x0103, 0x00e2, 0x00ea, 0x00f4, 0x01a1, 0x01b0, 0x0111, ALTCHR,
    ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, 0x00e0, 0x1ea3, 0x00e3,
    0x00e1, 0x1ea1, ALTCHR, 0x1eb1, 0x1eb3, 0x1eb5, 0x1eaf, ALTCHR,
    ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, 0x1eb7, 0x1ea7,
    0x1ea9, 0x1eab, 0x1ea5, 0x1ead, 0x00e8, ALTCHR, 0x1ebb, 0x1ebd,
    0x00e9, 0x1eb9, 0x1ec1, 0x1ec3, 0x1ec5, 0x1ebf, 0x1ec7, 0x00ec,
    0x1ec9, ALTCHR, ALTCHR, ALTCHR, 0x0129, 0x00ed, 0x1ecb, 0x00f2,
    ALTCHR, 0x1ecf, 0x00f5, 0x00f3, 0x1ecd, 0x1ed3, 0x1ed5, 0x1ed7,
    0x1ed1, 0x1ed9, 0x1edd, 0x1edf, 0x1ee1, 0x1edb, 0x1ee3, 0x00f9,
    ALTCHR, 0x1ee7, 0x0169, 0x00fa, 0x1ee5, 0x1eeb, 0x1eed, 0x1eef,
    0x1ee9, 0x1ef1, 0x1ef3, 0x1ef7, 0x1ef9, 0x00fd, 0x1ef5, ALTCHR
};

CODE_CONV_ISO8859_TO_UCS2(cc_tcvn_to_ucs2, /* function name */
                          tblTCVNToUcs2, /* table name */
                          ALTCHR /* alt char code (on UCS2) */
                          )

/* end of file */
