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
/* $XFree86: xc/extras/X-TrueType/KOI8/main.c,v 1.3 2003/10/22 16:25:45 tsi Exp $ */

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
    KOI8_R,
    KOI8_U,
    KOI8_RU,
    KOI8_UNI
} CharSetMagic;

static CharSetRelation const charSetRelations[] = {
    { "koi8",   NULL, "1",   KOI8_R,   { 0x20, 0xff, 0,    0,    0x20 } },
    { "koi8",   NULL, "r",   KOI8_R,   { 0x20, 0xff, 0,    0,    0x20 } },
    { "koi8",   NULL, "u",   KOI8_U,   { 0x20, 0xff, 0,    0,    0x20 } },
    { "koi8",   NULL, "ru",  KOI8_RU,  { 0x20, 0xff, 0,    0,    0x20 } },
    { "koi8",   NULL, "uni", KOI8_UNI, { 0x20, 0xff, 0,    0,    0x20 } },
    { NULL, NULL, NULL, 0, { 0, 0, 0, 0, 0 } }
};


CODECONV_TEMPLATE(cc_koi8_r_to_ucs2);
CODECONV_TEMPLATE(cc_koi8_u_to_ucs2);
CODECONV_TEMPLATE(cc_koi8_ru_to_ucs2);
CODECONV_TEMPLATE(cc_koi8_uni_to_ucs2);
static MapIDRelation const mapIDRelations[] = {
    { KOI8_R,       EPlfmISO,     EEncISO10646,
                                  cc_koi8_r_to_ucs2,                   NULL },
    { KOI8_R,       EPlfmUnicode, EEncAny,
                                  cc_koi8_r_to_ucs2,                   NULL },
    { KOI8_R,       EPlfmMS,      EEncMSUnicode,
                                  cc_koi8_r_to_ucs2,                   NULL },
    { KOI8_U,       EPlfmISO,     EEncISO10646,
                                  cc_koi8_u_to_ucs2,                   NULL },
    { KOI8_U,       EPlfmUnicode, EEncAny,
                                  cc_koi8_u_to_ucs2,                   NULL },
    { KOI8_U,       EPlfmMS,      EEncMSUnicode,
                                  cc_koi8_u_to_ucs2,                   NULL },
    { KOI8_RU,      EPlfmISO,     EEncISO10646,
                                  cc_koi8_ru_to_ucs2,                  NULL },
    { KOI8_RU,      EPlfmUnicode, EEncAny,
                                  cc_koi8_ru_to_ucs2,                  NULL },
    { KOI8_RU,      EPlfmMS,      EEncMSUnicode,
                                  cc_koi8_ru_to_ucs2,                  NULL },
    { KOI8_UNI,     EPlfmISO,     EEncISO10646,
                                  cc_koi8_uni_to_ucs2,                 NULL },
    { KOI8_UNI,     EPlfmUnicode, EEncAny,
                                  cc_koi8_uni_to_ucs2,                 NULL },
    { KOI8_UNI,     EPlfmMS,      EEncMSUnicode,
                                  cc_koi8_uni_to_ucs2,                 NULL },
    { -1, 0, 0, NULL, NULL }
};

STD_ENTRYFUNC_TEMPLATE(KOI8_entrypoint)

/*
  koi-* -> UCS2

  This  from xfsft:
    Copyright (c) 1997 by Mark Leisher
    Copyright (c) 1998 by Juliusz Chroboczek

 */

ucs2_t
cc_koi8_u_to_ucs2(ft_char_code_t codeSrc)
{
    ucs2_t codeDst;

    switch(codeSrc) {
    case 0xA4: codeDst = 0x0454; break;
    case 0xA6: codeDst = 0x0456; break;
    case 0xA7: codeDst = 0x0457; break;
    case 0xAD: codeDst = 0x0491; break;
    case 0xB4: codeDst = 0x0404; break;
    case 0xB6: codeDst = 0x0406; break;
    case 0xB7: codeDst = 0x0407; break;
    case 0xBD: codeDst = 0x0490; break;
    default:   codeDst = cc_koi8_r_to_ucs2(codeSrc);
    }

    return codeDst;
}

ucs2_t
cc_koi8_ru_to_ucs2(ft_char_code_t codeSrc)
{
    ucs2_t codeDst;

    switch(codeSrc) {
    case 0x93: codeDst = 0x201C; break;
    case 0x96: codeDst = 0x201D; break;
    case 0x97: codeDst = 0x2014; break;
    case 0x98: codeDst = 0x2116; break;
    case 0x99: codeDst = 0x2122; break;
    case 0x9B: codeDst = 0x00BB; break;
    case 0x9C: codeDst = 0x00AE; break;
    case 0x9D: codeDst = 0x00AB; break;
    case 0x9F: codeDst = 0x00A4; break;
    case 0xA4: codeDst = 0x0454; break;
    case 0xA6: codeDst = 0x0456; break;
    case 0xA7: codeDst = 0x0457; break;
    case 0xAD: codeDst = 0x0491; break;
    case 0xAE: codeDst = 0x045E; break;
    case 0xB4: codeDst = 0x0404; break;
    case 0xB6: codeDst = 0x0406; break;
    case 0xB7: codeDst = 0x0407; break;
    case 0xBD: codeDst = 0x0490; break;
    case 0xBE: codeDst = 0x040E; break;
    default:   codeDst = cc_koi8_r_to_ucs2(codeSrc);
    }

    return codeDst;
}

/* end of file */
