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
/* $XFree86: xc/extras/X-TrueType/JISX0201/main.c,v 1.3 2003/10/22 16:25:43 tsi Exp $ */

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
    JISX0201
} CharSetMagic;

static CharSetRelation const charSetRelations[] = {
    { "jisx0201",  NULL, NULL, JISX0201, { 0x20, 0xdf, 0, 0, 0x20 } },
    { NULL, NULL, NULL, 0, { 0, 0, 0, 0, 0 } }
};


CODECONV_TEMPLATE(cc_jisx0201_to_win_ucs2);
CODECONV_TEMPLATE(cc_jisx0201_to_std_ucs2);
CODECONV_CALLBACK_TEMPLATE(cb_jisx0201_to_ucs2);
static MapIDRelation const mapIDRelations[] = {
    { JISX0201,     EPlfmMS,      EEncMSUnicode,
                                  cc_jisx0201_to_win_ucs2,
                                  cb_jisx0201_to_ucs2 },
    { JISX0201,     EPlfmISO,     EEncISO10646,
                                  cc_jisx0201_to_std_ucs2, 
                                  cb_jisx0201_to_ucs2 },
    { JISX0201,     EPlfmUnicode, EEncAny,
                                  cc_jisx0201_to_std_ucs2,
                                  cb_jisx0201_to_ucs2 },
    /*
      To avoid the difficulty on the OS/2 Warp Japanese Fonts
      (OEM from Dynalab), we must place the following platform/encoding
      'Apple Japanese' before 'MS Shift JIS'.
      These fonts has both the below encodings, but MS S-JIS is incorrect.
     */
    { JISX0201,     EPlfmApple,   EEncAppleJapanese, NULL,             NULL },
    { JISX0201,     EPlfmMS,      EEncMSShiftJIS,    NULL,             NULL },
    { -1, 0, 0, NULL, NULL }
};

STD_ENTRYFUNC_TEMPLATE(JISX0201_entrypoint)
CODECONV_CALLBACK_TEMPLATE(cb_jisx0201_to_ucs2)
{
    SPropRecValContainer contRecValue;
    
    if (SPropRecValList_search_record(charSetHints->refListPropRecVal,
                                      &contRecValue,
                                      "EncodingOptions")) {
        char const *str =
            SPropContainer_value_str(contRecValue);
        if (tolower(*str) == 'm') {
            refCodeConverterInfo->ptrCodeConverter = cc_jisx0201_to_win_ucs2;
        } else if (tolower(*str) == 'i') {
            refCodeConverterInfo->ptrCodeConverter = cc_jisx0201_to_std_ucs2;
        }
    }
}

ft_char_code_t /* result charCodeDest */
cc_jisx0201_to_std_ucs2(ft_char_code_t idx)
{
    if (idx == 0x005c) {
	  return 0x00a5;
	}
	return cc_jisx0201_to_win_ucs2(idx);
}

/* end of file */
