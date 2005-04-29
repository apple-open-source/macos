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
/* $XFree86: xc/extras/X-TrueType/ISO8859.1/main.c,v 1.2 2003/10/22 16:25:35 tsi Exp $ */

#include "xttversion.h"

#if 0
static char const * const releaseID =
    _XTT_RELEASE_NAME;
#endif

#include "xttcommon.h"
#include "xttcap.h"
#include "xttcconv.h"
#include "xttcconvP.h"

/****************************************
  tables
 */
typedef enum
{
    ISO8859_1,
    ASCII,
    APPLE_ROMAN,
    APPLE_CENTEURO,
    APPLE_CYRILLIC,
    MS_SYMBOL,
    UNICODE
} CharSetMagic;

static CharSetRelation const charSetRelations[] = {
#ifndef I_HATE_UNICODE
    { "unicode",  NULL, NULL,    UNICODE,   { 0x00, 0xff, 0x00, 0xff, 0x20 } },
    { "iso10646", NULL, "1",     UNICODE,   { 0x00, 0xff, 0x00, 0xff, 0x20 } },
#endif
    { "iso8859",  NULL, "1",     ISO8859_1, { 0x20, 0xff, 0,    0,    0x20 } },
    { "ascii",    NULL, NULL,    ASCII,     { 0x20, 0x7e, 0,    0,    0x20 } },
    { "apple",    NULL, "roman",
                  APPLE_ROMAN,      { 0x20, 0xff, 0,    0,    0x20 } },
    { "apple",    NULL, "centeuro",
                  APPLE_CENTEURO,   { 0x20, 0xff, 0,    0,    0x20 } },
    { "microsoft",
                  NULL, "symbol",
                  MS_SYMBOL,        { 0x20, 0xff, 0,    0,    0x20 } },
    { "microsoft_symbol",
                  NULL, NULL,
                  MS_SYMBOL,        { 0x20, 0xff, 0,    0,    0x20 } },
    { "ms",       NULL, "symbol",
                  MS_SYMBOL,        { 0x20, 0xff, 0,    0,    0x20 } },
    { "ms_symbol",
                  NULL, NULL,
                  MS_SYMBOL,        { 0x20, 0xff, 0,    0,    0x20 } },
    { NULL, NULL, NULL, 0, { 0, 0, 0, 0, 0 } }
};

CODECONV_TEMPLATE(cc_iso8859_1_to_apple_roman);
CODECONV_TEMPLATE(cc_ms_symbol_to_ucs2);
static MapIDRelation const mapIDRelations[] = {
    { ISO8859_1,    EPlfmISO,     EEncISO8859_1,     NULL,             NULL },
    { ISO8859_1,    EPlfmISO,     EEncISO10646,      NULL,             NULL },
    { ISO8859_1,    EPlfmUnicode, EEncAny,           NULL,             NULL },
    { ISO8859_1,    EPlfmMS,      EEncMSUnicode,     NULL,             NULL },
    { ISO8859_1,    EPlfmApple,   EEncAppleRoman,
                                 cc_iso8859_1_to_apple_roman,         NULL },
    { ISO8859_1,    EPlfmApple,   EEncAppleJapanese, NULL,             NULL },
    { ISO8859_1,    EPlfmMS,      EEncMSShiftJIS,    NULL,             NULL },
    { ASCII,        EPlfmISO,     EEncISOASCII,      NULL,             NULL },
    { ASCII,        EPlfmISO,     EEncISO8859_1,     NULL,             NULL },
    { ASCII,        EPlfmISO,     EEncISO10646,      NULL,             NULL },
    { ASCII,        EPlfmUnicode, EEncAny,           NULL,             NULL },
    { ASCII,        EPlfmMS,      EEncMSUnicode,     NULL,             NULL },
    { ASCII,        EPlfmApple,   EEncAppleRoman,    NULL,             NULL },
    /*
      To avoid the difficulty on the OS/2 Warp Japanese Fonts
      (OEM from Dynalab), we must place the following platform/encoding
      'Apple Japanese' before 'MS Shift JIS'.
      These fonts has both the below encodings, but MS S-JIS is incorrect.
     */
    { ASCII,        EPlfmApple,   EEncAppleJapanese, NULL,             NULL },
    { ASCII,        EPlfmMS,      EEncMSShiftJIS,    NULL,             NULL },
    { APPLE_CENTEURO,
                    EPlfmApple,   EEncAppleCenteuro, NULL,             NULL },
    { APPLE_CYRILLIC,
                    EPlfmApple,   EEncAppleCyrillic, NULL,             NULL },
    { MS_SYMBOL,    EPlfmMS,      EEncMSSymbol,
                                  cc_ms_symbol_to_ucs2,                NULL },
#ifndef I_HATE_UNICODE
    { UNICODE,      EPlfmUnicode, EEncAny,           NULL,             NULL },
    { UNICODE,      EPlfmISO,     EEncISO10646,      NULL,             NULL },
    { UNICODE,      EPlfmISO,     EEncISO10646,      NULL,             NULL },
    { UNICODE,      EPlfmMS,      EEncMSUnicode,     NULL,             NULL },
    { -1, 0, 0, NULL, NULL }
#endif /* I_HATE_UNICODE */
};

STD_ENTRYFUNC_TEMPLATE(ISO8859_1_entrypoint)


/*
   MS Symbol
 */

ft_char_code_t /* result charCodeDest */
cc_ms_symbol_to_ucs2(ft_char_code_t codeSrc)
{
    ucs2_t codeDst = codeSrc | 0xf000;

    return codeDst;
}

/* end of file */
