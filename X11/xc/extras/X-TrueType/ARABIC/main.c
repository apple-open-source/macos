/* ===EmacsMode: -*- Mode: C; tab-width:4; c-basic-offset: 4; -*- === */
/* ===FileName: ===
   Copyright (c) 1998 Takuya SHIOZAKI, All Rights reserved.
   Copyright (c) 1998 X-TrueType Server Project, All rights reserved. 
   Copyright (c) 1999 Pablo Saratxaga <srtxg@chanae.alphanet.ch>
   
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
/* $XFree86: xc/extras/X-TrueType/ARABIC/main.c,v 1.3 2003/10/22 16:25:24 tsi Exp $ */

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
    MULEARABIC0,
    MULEARABIC1,
    MULEARABIC2,
    CP1256,
    XATERM, /* "extended" font used by xaterm */
    ISIRI_3342,
    IRANSYSTEM,
    URDUNAQSH
} CharSetMagic;

static CharSetRelation const charSetRelations[] = {
    { "mulearabic",  NULL,            "0",
		 MULEARABIC0, { 0x20, 0x2a,    0,    0, 0x20 } },
    { "mulearabic",  NULL,            "1",
		 MULEARABIC1, { 0x20, 0x69,    0,    0, 0x20 } },
    { "mulearabic",  NULL,            "2",
		 MULEARABIC2, { 0x20, 0x6e,    0,    0, 0x20 } },
    { "microsoft",   NULL,       "cp1256",
		      CP1256, { 0x00, 0xff,    0,    0, 0x20 } },
    { "xaterm",      NULL, "fontspecific",
		      XATERM, { 0x00, 0xff, 0x00, 0x01, 0x20 } },
    { "isiri",       NULL, "3342",
		  ISIRI_3342, { 0x00, 0xff, 0x00, 0x01, 0x20 } },
    { "iransystem",  NULL, "0",
		  IRANSYSTEM, { 0x00, 0xff, 0x00, 0x01, 0x20 } },
    { "urdunaqsh",   NULL, "0",
		   URDUNAQSH, { 0x00, 0xff, 0x00, 0x01, 0x20 } },
    { NULL, NULL, NULL, 0, { 0, 0, 0, 0, 0 } }
};


CODECONV_TEMPLATE(cc_mulearabic0_to_ucs2);
CODECONV_TEMPLATE(cc_mulearabic1_to_ucs2);
CODECONV_TEMPLATE(cc_mulearabic2_to_ucs2);
CODECONV_TEMPLATE(cc_cp1256_to_ucs2);
CODECONV_TEMPLATE(cc_xaterm_to_ucs2);
CODECONV_TEMPLATE(cc_xaterm_tophalf_to_ucs2);
CODECONV_TEMPLATE(cc_isiri_3342_to_ucs2);
CODECONV_TEMPLATE(cc_iransystem_to_ucs2);
CODECONV_TEMPLATE(cc_urdunaqsh_to_ucs2);
static MapIDRelation const mapIDRelations[] = {
    { MULEARABIC0,    EPlfmISO,     EEncISO10646,
				cc_mulearabic0_to_ucs2,		NULL },
    { MULEARABIC0,    EPlfmUnicode, EEncAny,     
				cc_mulearabic0_to_ucs2,		NULL },
    { MULEARABIC0,    EPlfmMS,      EEncMSUnicode,
				cc_mulearabic0_to_ucs2,		NULL },
    { MULEARABIC1,    EPlfmISO,     EEncISO10646,
                                cc_mulearabic1_to_ucs2,         NULL },
    { MULEARABIC1,    EPlfmUnicode, EEncAny,
                                cc_mulearabic1_to_ucs2,         NULL },
    { MULEARABIC1,    EPlfmMS,      EEncMSUnicode,
                                cc_mulearabic1_to_ucs2,         NULL },
    { MULEARABIC2,    EPlfmISO,     EEncISO10646,
                                cc_mulearabic2_to_ucs2,         NULL },
    { MULEARABIC2,    EPlfmUnicode, EEncAny,
                                cc_mulearabic2_to_ucs2,         NULL },
    { MULEARABIC2,    EPlfmMS,      EEncMSUnicode,
                                cc_mulearabic2_to_ucs2,         NULL },
    { CP1256,         EPlfmISO,     EEncISO10646,
    				cc_cp1256_to_ucs2,		NULL },
    { CP1256,         EPlfmUnicode, EEncAny,     
    				cc_cp1256_to_ucs2,		NULL },
    { CP1256,         EPlfmMS,      EEncMSUnicode,
				cc_cp1256_to_ucs2,		NULL },
    { XATERM,         EPlfmISO,     EEncISO10646,
                                cc_xaterm_to_ucs2,              NULL },
    { XATERM,         EPlfmUnicode, EEncAny,
                                cc_xaterm_to_ucs2,              NULL },
    { XATERM,         EPlfmMS,      EEncMSUnicode,
                                cc_xaterm_to_ucs2,              NULL },
    { ISIRI_3342,     EPlfmISO,     EEncISO10646,
                                cc_isiri_3342_to_ucs2,          NULL },
    { ISIRI_3342,     EPlfmUnicode, EEncAny,
                                cc_isiri_3342_to_ucs2,          NULL },
    { ISIRI_3342,     EPlfmMS,      EEncMSUnicode,
                                cc_isiri_3342_to_ucs2,          NULL },
    { IRANSYSTEM,     EPlfmISO,     EEncISO10646,
                                cc_iransystem_to_ucs2,          NULL },
    { IRANSYSTEM,     EPlfmUnicode, EEncAny,
                                cc_iransystem_to_ucs2,          NULL },
    { IRANSYSTEM,     EPlfmMS,      EEncMSUnicode,
                                cc_iransystem_to_ucs2,          NULL },
    { URDUNAQSH,      EPlfmISO,     EEncISO10646,
                                cc_urdunaqsh_to_ucs2,           NULL },
    { URDUNAQSH,      EPlfmUnicode, EEncAny,
                                cc_urdunaqsh_to_ucs2,           NULL },
    { URDUNAQSH,      EPlfmMS,      EEncMSUnicode,
                                cc_urdunaqsh_to_ucs2,           NULL },
    { -1, 0, 0, NULL, NULL }
};

STD_ENTRYFUNC_TEMPLATE(ARABIC_entrypoint)

ft_char_code_t /* result charCodeDest */
cc_xaterm_to_ucs2(ft_char_code_t codeSrc)
{
    if (codeSrc <= 0x00ff) return codeSrc;
    else return cc_xaterm_tophalf_to_ucs2( codeSrc & 0xff );
}


/* end of file */
