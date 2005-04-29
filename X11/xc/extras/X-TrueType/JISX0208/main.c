/* ===EmacsMode: -*- Mode: C; tab-width:4; c-basic-offset: 4; -*- === */
/* ===FileName: ===
   Copyright (c) 1998 Takuya SHIOZAKI, All Rights reserved.
   Copyright (c) 1998 X-TrueType Server Project, All rights reserved. 
   
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
/* $XFree86: xc/extras/X-TrueType/JISX0208/main.c,v 1.4 2003/10/22 16:25:44 tsi Exp $ */

#include "xttversion.h"

#if 0
static char const * const releaseID =
    _XTT_RELEASE_NAME;
#endif

#include "xttcommon.h"
#include "xttcap.h"
#include "xttcconv.h"
#include "xttcconvP.h"


typedef enum
{
    JISX0208
} CharSetMagic;

static CharSetRelation const charSetRelations[] = {
    { "jisx0208", NULL, NULL, JISX0208, { 0x21, 0x7e, 0x21, 0x74, 0x2121 } },
    { "gt", NULL, NULL, JISX0208, { 0x21, 0x7e, 0x21, 0x74, 0x2121 } },
    { NULL, NULL, NULL, 0, { 0, 0, 0, 0, 0 } }
};


CODECONV_TEMPLATE(cc_jisx0208_to_win_ucs2);
CODECONV_TEMPLATE(cc_jisx0208_to_sjis);
CODECONV_TEMPLATE(cc_jisx0208_to_std_ucs2);
CODECONV_CALLBACK_TEMPLATE(cb_jisx0208_to_ucs2);
static MapIDRelation const mapIDRelations[] = {
    { JISX0208,     EPlfmMS,      EEncMSUnicode,
                                  cc_jisx0208_to_win_ucs2,
                                  cb_jisx0208_to_ucs2 },
    { JISX0208,     EPlfmISO,     EEncISO10646,
                                  cc_jisx0208_to_std_ucs2,
                                  cb_jisx0208_to_ucs2 },
    { JISX0208,     EPlfmUnicode, EEncAny,
                                  cc_jisx0208_to_std_ucs2,
                                  cb_jisx0208_to_ucs2 },
    /*
      To avoid the difficulty on the OS/2 Warp Japanese Fonts
      (OEM from Dynalab), we must place the following platform/encoding
      'Apple Japanese' before 'MS Shift JIS'.
      These fonts has both the below encodings, but MS S-JIS is incorrect.
     */
    { JISX0208,     EPlfmApple,   EEncAppleJapanese,
                                  cc_jisx0208_to_sjis,
                                  NULL },
    { JISX0208,     EPlfmMS,      EEncMSShiftJIS,
                                  cc_jisx0208_to_sjis,
                                  NULL },
    { -1, 0, 0, NULL, NULL }
};

STD_ENTRYFUNC_TEMPLATE(JISX0208_entrypoint)


/*************************************************
  callback - switch code converter by encoding options
 */

CODECONV_CALLBACK_TEMPLATE(cb_jisx0208_to_ucs2)
{
    SPropRecValContainer contRecValue;
    
    if (SPropRecValList_search_record(charSetHints->refListPropRecVal,
                                      &contRecValue,
                                      "EncodingOptions")) {
        char const *str =
            SPropContainer_value_str(contRecValue);
        if (tolower(*str) == 'm') {
            refCodeConverterInfo->ptrCodeConverter = cc_jisx0208_to_win_ucs2;
        } else if (tolower(*str) == 'i') {
            refCodeConverterInfo->ptrCodeConverter = cc_jisx0208_to_std_ucs2;
        }
    }
}


/*************************************************
  Calculatable code convert functions
 */

/* JIS -> SJIS */
ft_char_code_t /* result charCodeDest */
cc_jisx0208_to_sjis(ft_char_code_t idx)
{
    unsigned c1, c2, d1, d2;

    c1 = (idx >> 8) & 0xff;
    c2 = idx & 0xff;
    d1 = ((c1 + 1) >> 1) + (c1 < 0x5f ? 0x70 : 0xb0);
    d2 = c2 + ((c1 & 1) ? (c2 < 0x60 ? 0x1f : 0x20) : 0x7e);

    return (d1<<8) + d2;
}


/* JISX0208 -> Standard UCS2 */
ft_char_code_t /* result charCodeDest */
cc_jisx0208_to_std_ucs2(ft_char_code_t codeSrc)
{
    ucs2_t codeDst;

    switch (codeSrc) {
    case 0x2140:
        codeDst = 0x005c;
        break;
    case 0x2141:
        codeDst = 0x301c;
        break;
    case 0x2142:
        codeDst = 0x2016;
        break;
    case 0x215d:
        codeDst = 0x2212;
        break;
    case 0x2171:
        codeDst = 0x00a2;
        break;
    case 0x2172:
        codeDst = 0x00a3;
        break;
    case 0x224c:
        codeDst = 0x00ac;
        break;
    default:
        return cc_jisx0208_to_win_ucs2(codeSrc);
    }

    return codeDst;
}

/* end of file */
