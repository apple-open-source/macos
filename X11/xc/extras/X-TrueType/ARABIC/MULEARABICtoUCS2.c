/* ===EmacsMode: -*- Mode: C; tab-width:4; c-basic-offset: 4; -*- === */
/* ===FileName: ===

   mulearabic to unicode table

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

    Table build by looking into the unicode chart,
    an actual X11 font using mulearabic-1 encoding.
    and help of Juliusz Chorobczek.

 */
/* $XFree86: xc/extras/X-TrueType/ARABIC/MULEARABICtoUCS2.c,v 1.2 2003/10/22 16:25:24 tsi Exp $ */

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

static ucs2_t tblMulearabic0ToUcs2[] = {
/* 0x0020 - 0x002A */
    ALTCHR, 0x0660, 0x0661, 0x0662, 0x0663, 0x0664, 0x0665, 0x0666,
    0x0667, 0x0668, 0x0669
};

static ucs2_t tblMulearabic1ToUcs2[] = {
/* 0x0020 - 0x0069 */
    ALTCHR, 0x0020, 0x0021, 0x0028, 0x0029, 0x060c, 0x06d4, 0x003a,
    0x061b, 0x061f, 0x00ab, 0x00bb, 0x000a, 0xfe80, 0xfe81, 0xfe82,
    0xfe83, 0xfe84, 0xfe85, 0xfe86, 0xfe87, 0xfe88, 0xfe8b, 0xfe8c,
    0xfe8d, 0xfe8e, 0xfe91, 0xfe92, 0xfe93, 0xfe94, 0xfe97, 0xfe98,
    0xfe9b, 0xfe9c, 0xfea9, 0xfeaa, 0xfeab, 0xfeac, 0xfead, 0xfeae,
    0xfeaf, 0xfeb0, 0xfecb, 0xfecc, 0xfecf, 0xfed0, 0xfed3, 0xfed4,
    0xfed7, 0xfed8, 0xfedf, 0xfee0, 0xfee1, 0xfee3, 0xfee4, 0xfee2,
    0xfee7, 0xfee8, 0xfee9, 0xfeeb, 0xfeec, 0xfeea, 0xfeed, 0xfeee,
    0xfef3, 0xfef4, 0xfef5, 0xfef7, 0xfef9, 0xfefb, 0xfb58, 0xfb59,
    0xfb8a, 0xfb8b
};

static ucs2_t tblMulearabic2ToUcs2[] = {
/* 0x0020 - 0x006e */
    ALTCHR, 0xfe89, 0xfe8a, 0xfe8f, 0xfe90, 0xfe95, 0xfe96, 0xfe99,
    0xfe9a, 0xfe9d, 0xfe9f, 0xfea0, 0xfe9e, 0xfea1, 0xfea3, 0xfea4,
    0xfea2, 0xfea5, 0xfea7, 0xfea8, 0xfea6, 0xfeb1, 0xfeb3, 0xfeb4,
    0xfeb2, 0xfeb5, 0xfeb7, 0xfeb8, 0xfeb6, 0xfeb9, 0xfebb, 0xfebc,
    0xfeba, 0xfebd, 0xfebf, 0xfec0, 0xfebe, 0xfec1, 0xfec3, 0xfec4,
    0xfec2, 0xfec5, 0xfec7, 0xfec8, 0xfec6, 0xfec9, 0xfeca, 0xfecd,
    0xfece, 0xfed1, 0xfed2, 0xfed5, 0xfed6, 0xfed9, 0xfedb, 0xfedc,
    0xfeda, 0xfedd, 0xfede, 0xfee5, 0xfee6, 0xfeef, 0xfef0, 0xfef1,
    0xfef2, 0xfef6, 0xfef8, 0xfefa, 0xfefc, 0xfb56, 0xfb57, 0xfb7a,
    0xfb7c, 0xfb7d, 0xfb7b, 0xfb92, 0xfb94, 0xfb95, 0xfb93
};


CODE_CONV_ONE_OCTET_TO_UCS2(cc_mulearabic0_to_ucs2, /* function name */
                            tblMulearabic0ToUcs2, /* table name */
                            0x20,0x2a, /* begin and end of table */
                            ALTCHR /* alt char code (on UCS2) */
                            )

CODE_CONV_ONE_OCTET_TO_UCS2(cc_mulearabic1_to_ucs2, /* function name */
                            tblMulearabic1ToUcs2, /* table name */
                            0x20,0x69, /* begin and end of table */
                            ALTCHR /* alt char code (on UCS2) */
                            )

CODE_CONV_ONE_OCTET_TO_UCS2(cc_mulearabic2_to_ucs2, /* function name */
                            tblMulearabic2ToUcs2, /* table name */
                            0x20,0x6e, /* begin and end of table */
                            ALTCHR /* alt char code (on UCS2) */
                            )

/* end of file */
