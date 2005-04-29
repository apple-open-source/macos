/* ===EmacsMode: -*- Mode: C; tab-width:4; c-basic-offset: 4; -*- === */
/* ===FileName: ===

   Microsoft arabic encoding to unicode table

   Copyright (c) 1998 Takuya SHIOZAKI, All Rights reserved.
   Copyright (c) 2000 Pablo Saratxaga <pablo@mandrakesoft.com>

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

	This is the encoding used by the URDUNAQSH font

				Pablo Saratxaga <pablo@mandrakesoft.com>
 */
/* $XFree86: xc/extras/X-TrueType/ARABIC/URDUNAQSHtoUCS2.c,v 1.2 2003/10/22 16:25:24 tsi Exp $ */

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

static ucs2_t tblUrdunaqshToUcs2[] = {
    ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR,
    ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR,
    ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR,
    ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR,
    0x0020, 0xfe81, 0xfe8d, 0xfe8f, 0xfb56, 0xfe95, 0xfb66, 0xfe99,
    0xfe9d, 0xfb7a, 0xfea1, 0xfea5, 0xfea9, 0xfb88, 0xfeab, 0xfead,
    0xfb8c, 0xfeaf, 0xfb8a, 0xfeb1, 0xfeb5, 0xfeb9, 0xfebd, 0xfec1,
    0xfec5, 0xfec9, 0xfecd, 0xfed1, 0xfb6a, 0xfed5, 0xfed9, 0xfb8e,
    0xfb92, 0xfedd, 0xfefb, 0xfee1, 0xfee5, 0xfb9e, 0xfeed, 0xfee9,
    0xfe93, 0xfbaa, 0xfe80, 0xfeef, 0xfef1, 0xfbae, 0xfe91, 0xfb58,
    0xfe97, 0xfb68, 0xfe9b, 0xfe9f, 0xfb7c, 0xfea3, 0xfea7, 0xfeb3,
    0xfeb7, 0xfebb, 0xfebf, 0xfec3, 0xfec7, 0xfecb, 0xfecf, 0xfed3,
    0xfb6c, 0xfed7, 0xfedb, 0xfb94, 0xfedb, 0xfb94, 0xfedf, 0xfee3,
    0xfee7, 0xfba8, 0xfba8, 0xfeeb, 0xfe8b, 0xfef3, 0xfe92, 0xfb59,
    0xfe98, 0xfb69, 0xfe9c, 0xfea0, 0xfb7d, 0xfea4, 0xfea8, 0xfeb4,
    0xfeb8, 0xfebc, 0xfec0, 0xfec4, 0xfec8, 0xfecc, 0xfed0, ALTCHR,
    ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR,
    ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR,
    ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR,
    ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR,
    0x00A0, 0xfed4, 0xfb6d, 0xfed8, 0xfedc, 0xfb95, 0xfee0, 0xfee4,
    0xfee8, 0xfba9, 0xfeec, 0xfe8c, 0xfef4, 0xfe8e, 0xfe90, 0xfb57,
    0xfe96, 0xfb67, 0xfe9a, 0xfe9e, 0xfb7b, 0xfea2, 0xfea6, ALTCHR,
    0xfeaa, 0xfb89, 0xfeac, 0xfeae, 0xfb8d, 0xfeb0, 0xfb8b, 0xfeb2,
    0xfeb6, 0xfeba, 0xfebe, 0xfec2, 0xfec6, 0xfeca, 0xfece, 0xfed2,
    0xfb6b, 0xfed6, 0xfeda, 0xfb8f, 0xfb93, 0xfede, 0xfefc, 0xfee2,
    0xfee6, 0xfb9f, 0xfeee, 0xfba7, 0xfeea, 0xfe94, 0xfef0, 0xfef2,
    0xfbaf, 0x0661, 0x0662, 0x0663, 0x0664, 0x06f4, 0x0665, 0x06f5,
    0x0666, 0x0667, 0x06f7, 0x0668, 0x0669, 0x0660, 0x0640, 0x002e,
    0x060c, 0x061f, 0x0021, 0x003a, 0x0027, 0x0028, 0x0029, 0xF0EF,
    0xF0F0, 0xF0F1, 0xF0F2, 0x0653, 0x0670, 0x0655, 0x0654, 0x064c,
    0x064b, 0x064d, 0xF0FA, 0xF0FB, 0x0651, 0x064f, 0x064e, 0x0650 
};

CODE_CONV_ONE_OCTET_TO_UCS2_ALL(cc_urdunaqsh_to_ucs2, /* function name */
                                tblUrdunaqshToUcs2) /* table name */


/* end of file */
