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
/* $XFree86: xc/extras/X-TrueType/KSCJOHAB/main.c,v 1.3 2003/10/22 16:25:47 tsi Exp $ */

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
    KSCJOHAB
} CharSetMagic;

static CharSetRelation const charSetRelations[] = {
    { "ksc5601",   NULL,  "3", KSCJOHAB, { 0x30, 0xff, 0x84, 0xf9, 0xd931 } },
    { "ksx1001",   NULL,  "3", KSCJOHAB, { 0x30, 0xff, 0x84, 0xf9, 0xd931 } },
    { "kscjohab",  NULL, NULL, KSCJOHAB, { 0x30, 0xff, 0x84, 0xf9, 0xd931 } },
    { "ksc5601johab",  NULL, NULL, KSCJOHAB,
                                         { 0x30, 0xff, 0x84, 0xf9, 0xd931 } },
    { NULL, NULL, NULL, 0, { 0, 0, 0, 0, 0 } }
};


CODECONV_TEMPLATE(cc_ksc5601johab_to_ucs2);
static MapIDRelation const mapIDRelations[] = {
    { KSCJOHAB,     EPlfmISO,     EEncISO10646,
                                  cc_ksc5601johab_to_ucs2,             NULL },
    { KSCJOHAB,     EPlfmUnicode, EEncAny,
                                  cc_ksc5601johab_to_ucs2,             NULL },
    { KSCJOHAB,     EPlfmMS,      EEncMSUnicode,
                                  cc_ksc5601johab_to_ucs2,             NULL },
    { -1, 0, 0, NULL, NULL }
};

STD_ENTRYFUNC_TEMPLATE(KSCJOHAB_entrypoint)


/* end of file */
