/* ===EmacsMode: -*- Mode: C; tab-width:4; c-basic-offset: 4; -*- === */
/* ===FileName: ===
   Copyright (c) 1998 Takuya SHIOZAKI, All Rights reserved.
   Copyright (c) 1998 X-TrueType Server Project, All rights reserved. 
   Copyright (c) 1998,1999 Pablo Saratxaga <srtxg@chanae.alphanet.ch>
   
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

    Table build by looking into the unicode chart, and various DOS
    and Windows fonts, DOS manuals and GNU libc internationalization
    charset tables.

    support for those encodings may seem useless for X11, but doesmu users
    can appreciate it, as well as users of any kind of DOS emulator.
    Also several True Type fonts made primarly for Windows are completly
    broken, that is the unicode codes they told are wrong, they claim to
    be cp1252 while they are not. So the alias -misc-fontspecific can
    be helpfull to use TTF fonts wrongly encoded (that is the case of
    all the vietnamese fonts I've seen so far, they have to be called
    whith -misc-fontspecific instead of -viscii1.1-1 *sigh*)

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
    CP437,
    CP850,
    CP1252
} CharSetMagic;

static CharSetRelation const charSetRelations[] = {
    { "ibm",       NULL,        "cp437",  CP437,  { 0x00, 0xff, 0, 0, 0x20 } },
    { "ibm",       NULL,        "cp850",  CP850,  { 0x00, 0xff, 0, 0, 0x20 } },
    { "microsoft", NULL,       "cp1252", CP1252,  { 0x00, 0xff, 0, 0, 0x20 } },
    { "ansi",      NULL,            "0", CP1252,  { 0x00, 0xff, 0, 0, 0x20 } },
    { "microsoft", NULL,       "win3.1", CP1252,  { 0x00, 0xff, 0, 0, 0x20 } },
    { "microsoft", NULL, "fontspecific", CP1252,  { 0x00, 0xff, 0, 0, 0x20 } },
    { "misc",      NULL, "fontspecific", CP1252,  { 0x00, 0xff, 0, 0, 0x20 } },
    { NULL, NULL, NULL, 0, { 0, 0, 0, 0, 0 } }
};


CODECONV_TEMPLATE(cc_cp437_to_ucs2);
CODECONV_TEMPLATE(cc_cp850_to_ucs2);
CODECONV_TEMPLATE(cc_cp1252_to_ucs2);
static MapIDRelation const mapIDRelations[] = {
    { CP437,    EPlfmISO,     EEncISO10646,    cc_cp437_to_ucs2,    NULL },
    { CP437,    EPlfmUnicode, EEncAny,         cc_cp437_to_ucs2,    NULL },
    { CP437,    EPlfmMS,      EEncMSUnicode,   cc_cp437_to_ucs2,    NULL },
    { CP850,    EPlfmISO,     EEncISO10646,    cc_cp850_to_ucs2,    NULL },
    { CP850,    EPlfmUnicode, EEncAny,         cc_cp850_to_ucs2,    NULL },
    { CP850,    EPlfmMS,      EEncMSUnicode,   cc_cp850_to_ucs2,    NULL },
    { CP1252,   EPlfmISO,     EEncISO10646,    cc_cp1252_to_ucs2,   NULL },
    { CP1252,   EPlfmUnicode, EEncAny,         cc_cp1252_to_ucs2,   NULL },
    { CP1252,   EPlfmMS,      EEncMSUnicode,   cc_cp1252_to_ucs2,   NULL },
    { -1, 0, 0, NULL, NULL }
};

STD_ENTRYFUNC_TEMPLATE(DOSENCODING_entrypoint)

/* end of file */
