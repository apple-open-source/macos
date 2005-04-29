/* ===EmacsMode: -*- Mode: C; tab-width:4; c-basic-offset: 4; -*- === */
/* ===FileName: ===
   Copyright (c) 1998 Takuya SHIOZAKI, All Rights reserved.
   Copyright (c) 1998 X-TrueType Server Project, All rights reserved. 
   Copyright (c) 1999 Pablo Saratxaga <pablo@mandrakesoft.com>

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
/* $XFree86: xc/extras/X-TrueType/ISO8859.13/main.c,v 1.2 2003/10/22 16:25:37 tsi Exp $ */

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
    ISO8859_13,
    CP1257
} CharSetMagic;

static CharSetRelation const charSetRelations[] = {
    { "iso8859",   NULL,     "13", ISO8859_13, { 0x20, 0xff,  0,  0, 0x20 } },
    { "microsoft", NULL, "cp1257",     CP1257, { 0x20, 0xff,  0,  0, 0x20 } },
    { NULL, NULL, NULL, 0, { 0, 0, 0, 0, 0 } }
};


CODECONV_TEMPLATE(cc_iso8859_13_to_ucs2);
CODECONV_TEMPLATE(cc_cp1257_to_ucs2);
static MapIDRelation const mapIDRelations[] = {
   { ISO8859_13,   EPlfmISO,     EEncISO10646,  cc_iso8859_13_to_ucs2, NULL },
   { ISO8859_13,   EPlfmUnicode, EEncAny,       cc_iso8859_13_to_ucs2, NULL },
   { ISO8859_13,   EPlfmMS,      EEncMSUnicode, cc_iso8859_13_to_ucs2, NULL },
   { CP1257,       EPlfmISO,     EEncISO10646,  cc_cp1257_to_ucs2,     NULL },
   { CP1257,       EPlfmUnicode, EEncAny,       cc_cp1257_to_ucs2,     NULL },
   { CP1257,       EPlfmMS,      EEncMSUnicode, cc_cp1257_to_ucs2,     NULL },
   { -1, 0, 0, NULL, NULL }
};
	
STD_ENTRYFUNC_TEMPLATE(ISO8859_13_entrypoint)

ucs2_t
cc_cp1257_to_ucs2(ft_char_code_t codeSrc)
{
	ucs2_t codeDst;
	
	switch(codeSrc) {
		case 0x80: codeDst = 0x20ac; break;
		case 0x82: codeDst = 0x201a; break;
		case 0x84: codeDst = 0x201e; break;
		case 0x85: codeDst = 0x2026; break;
		case 0x86: codeDst = 0x2020; break;
		case 0x87: codeDst = 0x2021; break;
		case 0x89: codeDst = 0x2030; break;
		case 0x8b: codeDst = 0x2039; break;
		case 0x8d: codeDst = 0x00a8; break;
		case 0x8e: codeDst = 0x02c7; break;
		case 0x8f: codeDst = 0x00b8; break;
		case 0x91: codeDst = 0x2018; break;
		case 0x92: codeDst = 0x2019; break;
		case 0x93: codeDst = 0x201c; break;
		case 0x94: codeDst = 0x201d; break;
		case 0x95: codeDst = 0x2022; break;
		case 0x96: codeDst = 0x2013; break;
		case 0x97: codeDst = 0x2014; break;
		case 0x99: codeDst = 0x2122; break;
		case 0x9b: codeDst = 0x203a; break;
		case 0x9d: codeDst = 0x00af; break;
		case 0x9e: codeDst = 0x02db; break;
		default:   codeDst = cc_iso8859_13_to_ucs2(codeSrc);
	}
	
	return codeDst;
}
	
/* end of file */
