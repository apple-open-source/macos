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
/* $XFree86: xc/extras/X-TrueType/ISO8859.15/main.c,v 1.2 2003/10/22 16:25:39 tsi Exp $ */

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
    ISO8859_15
} CharSetMagic;

static CharSetRelation const charSetRelations[] = {
    { "iso8859",  NULL, "15",  ISO8859_15, { 0x20, 0xff, 0,    0,    0x20 } },
    { NULL, NULL, NULL, 0, { 0, 0, 0, 0, 0 } }
};


CODECONV_TEMPLATE(cc_iso8859_15_to_ucs2);
static MapIDRelation const mapIDRelations[] = {
    { ISO8859_15,    EPlfmISO,     EEncISO10646,
                                  cc_iso8859_15_to_ucs2,                NULL },
    { ISO8859_15,    EPlfmUnicode, EEncAny,
                                  cc_iso8859_15_to_ucs2,                NULL },
    { ISO8859_15,    EPlfmMS,      EEncMSUnicode,
                                  cc_iso8859_15_to_ucs2,                NULL },
    { -1, 0, 0, NULL, NULL }
};

STD_ENTRYFUNC_TEMPLATE(ISO8859_15_entrypoint)


/*************************************************
  Calculatable code convert functions
 */

/*
  iso8859-15 -> UCS2

  This  from xfsft:
    Copyright (c) 1997 by Mark Leisher
    Copyright (c) 1998 by Juliusz Chroboczek

 */
ucs2_t /* result charCodeDest */
cc_iso8859_15_to_ucs2(ft_char_code_t codeSrc)
{
    ucs2_t codeDst=codeSrc;

    switch(codeSrc) {
    case 0xA4: codeDst = 0x20AC; break;
    case 0xA6: codeDst = 0x0160; break;
    case 0xA8: codeDst = 0x0161; break;
    case 0xB4: codeDst = 0x017D; break;
    case 0xB8: codeDst = 0x017E; break;
    case 0xBC: codeDst = 0x0152; break;
    case 0xBD: codeDst = 0x0153; break;
    case 0xBE: codeDst = 0x0178; break;
    }

    return codeDst;
}

/* end of file */
