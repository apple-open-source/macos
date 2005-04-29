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
/* $XFree86: xc/extras/X-TrueType/GB18030/main.c,v 1.2 2003/10/22 16:25:29 tsi Exp $ */

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
    GB18030_0,
    GB18030_2000_0,
    GB18030_2000_1
} CharSetMagic;

static CharSetRelation const charSetRelations[] = {
    { "gb18030",  "2000", "0", GB18030_2000_0, { 0x40, 0xff, 0x81, 0xfe, 0x8140 } },
    { "gb18030",  "2000", "1", GB18030_2000_1, { 0x00, 0xff, 0x00, 0x99, 0x0000 } },
    { "gb18030",    NULL, "0", GB18030_0,      { 0x00, 0xff, 0x00, 0xff, 0x3000 } },
    { "gbk2k",      NULL, "0", GB18030_0,      { 0x00, 0xff, 0x00, 0xff, 0x3000 } },
    { NULL, NULL, NULL, 0, { 0, 0, 0, 0, 0 } }
};

CODECONV_TEMPLATE(cc_gb18030_2000_0_to_ucs2);
CODECONV_TEMPLATE(cc_gb18030_2000_1_to_ucs2);
CODECONV_TEMPLATE(cc_font_gbk2k_to_ucs2);
static MapIDRelation const mapIDRelations[] = {
    { GB18030_2000_0,  EPlfmISO,     EEncISO10646,
                                     cc_gb18030_2000_0_to_ucs2,      NULL },
    { GB18030_2000_0,  EPlfmUnicode, EEncAny,
                                     cc_gb18030_2000_0_to_ucs2,      NULL },
    { GB18030_2000_0,  EPlfmMS,      EEncMSUnicode,
                                     cc_gb18030_2000_0_to_ucs2,      NULL },
    { GB18030_2000_1,  EPlfmISO,     EEncISO10646,
                                     cc_gb18030_2000_1_to_ucs2,      NULL },
    { GB18030_2000_1,  EPlfmUnicode, EEncAny,
                                     cc_gb18030_2000_1_to_ucs2,      NULL },
    { GB18030_2000_1,  EPlfmMS,      EEncMSUnicode,
                                     cc_gb18030_2000_1_to_ucs2,      NULL },
    { GB18030_0,       EPlfmISO,     EEncISO10646,
                                     cc_font_gbk2k_to_ucs2,          NULL },
    { GB18030_0,       EPlfmUnicode, EEncAny,
                                     cc_font_gbk2k_to_ucs2,          NULL },
    { GB18030_0,       EPlfmMS,      EEncMSUnicode,
                                     cc_font_gbk2k_to_ucs2,          NULL },
    { -1, 0, 0, NULL, NULL }
};

STD_ENTRYFUNC_TEMPLATE(GB18030_entrypoint)

ft_char_code_t /* result charCodeDest */
cc_font_gbk2k_to_ucs2(ft_char_code_t codeSrc)
{
    return codeSrc;
}
/* end of file */
