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

#include "xttversion.h"

static char const * const releaseID =
    _XTT_RELEASE_NAME;

#include "xttcommon.h"
#include "xttcap.h"
#include "xttcconv.h"
#include "xttcconvP.h"

typedef enum
{
    JISX0212
} CharSetMagic;

static CharSetRelation const charSetRelations[] = {
    { "jisx0212", NULL, NULL, JISX0212, { 0x21, 0x7e, 0x21, 0x6d, 0x2121 } },
    { NULL, NULL, NULL, 0, { 0, 0, 0, 0, 0 } }
};


CODECONV_TEMPLATE(cc_jisx0212_to_ucs2);
CODECONV_TEMPLATE(cc_jisx0212_to_win_sjis);
static MapIDRelation const mapIDRelations[] = {
    { JISX0212,     EPlfmISO,     EEncISO10646,
                                  cc_jisx0212_to_ucs2,                 NULL },
    { JISX0212,     EPlfmUnicode, EEncAny,
                                  cc_jisx0212_to_ucs2,                 NULL },
    { JISX0212,     EPlfmMS,      EEncMSUnicode,
                                  cc_jisx0212_to_ucs2,                 NULL },
    { JISX0212,     EPlfmMS,      EEncMSShiftJIS,
                                  cc_jisx0212_to_win_sjis,             NULL },
    { -1, 0, 0, NULL, NULL }
};

STD_ENTRYFUNC_TEMPLATE(JISX0212_entrypoint)


/*************************************************
  Calculatable code convert functions
 */

/* JIS X 0212 -> Windows Shift JIS Auxiliary Kanji Mapping */
ft_char_code_t /* result charCodeDest */
cc_jisx0212_to_win_sjis(ft_char_code_t idx)
{
    unsigned c1, c2, d1, d2;

    c1 = (idx >> 8) & 0xff;
    c2 = idx & 0xff;

    if (c1 >= 0x4f) c1++; /* Slide after 0x4f00 */

    /* jis to sjis */
    d1 = ((c1 + 1) >> 1) + (c1 < 0x5f ? 0x70 : 0xb0);
    d2 = c2 + ((c1 & 1) ? (c2 < 0x60 ? 0x1f : 0x20) : 0x7e);

    return (d1<<8) + d2; 
}

/* end of file */
