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
/* $XFree86: xc/extras/X-TrueType/ISO8859.7/main.c,v 1.3 2003/10/22 16:25:41 tsi Exp $ */

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
    ISO8859_7,
    CP1253
} CharSetMagic;

static CharSetRelation const charSetRelations[] = {
    { "iso8859",   NULL,      "7", ISO8859_7, { 0x20, 0xff,  0,  0, 0x20 } },
    { "microsoft", NULL, "cp1253",    CP1253, { 0x20, 0xff,  0,  0, 0x20 } },
    { NULL, NULL, NULL, 0, { 0, 0, 0, 0, 0 } }
};


CODECONV_TEMPLATE(cc_iso8859_7_to_ucs2);
CODECONV_TEMPLATE(cc_cp1253_to_ucs2);
static MapIDRelation const mapIDRelations[] = {
    { ISO8859_7,    EPlfmISO,     EEncISO10646,  cc_iso8859_7_to_ucs2, NULL },
    { ISO8859_7,    EPlfmUnicode, EEncAny,       cc_iso8859_7_to_ucs2, NULL },
    { ISO8859_7,    EPlfmMS,      EEncMSUnicode, cc_iso8859_7_to_ucs2, NULL },
    { CP1253,       EPlfmISO,     EEncISO10646,  cc_cp1253_to_ucs2,    NULL },
    { CP1253,       EPlfmUnicode, EEncAny,       cc_cp1253_to_ucs2,    NULL },
    { CP1253,       EPlfmMS,      EEncMSUnicode, cc_cp1253_to_ucs2,    NULL },
    { -1, 0, 0, NULL, NULL }
};

STD_ENTRYFUNC_TEMPLATE(ISO8859_7_entrypoint)

ucs2_t
cc_cp1253_to_ucs2(ft_char_code_t codeSrc)
{
	ucs2_t codeDst;
	
	switch(codeSrc) {
		case 0x80: codeDst = 0x20ac; break;
		case 0x82: codeDst = 0x201a; break;
		case 0x83: codeDst = 0x0192; break;
		case 0x84: codeDst = 0x201e; break;
		case 0x85: codeDst = 0x2026; break;
		case 0x86: codeDst = 0x2020; break;
		case 0x87: codeDst = 0x2021; break;
		case 0x89: codeDst = 0x2030; break;
		case 0x8b: codeDst = 0x2039; break;
		case 0x91: codeDst = 0x2018; break;
		case 0x92: codeDst = 0x2019; break;
		case 0x93: codeDst = 0x201c; break;
		case 0x94: codeDst = 0x201d; break;
		case 0x95: codeDst = 0x2022; break;
		case 0x96: codeDst = 0x2013; break;
		case 0x97: codeDst = 0x2014; break;
		case 0x99: codeDst = 0x2122; break;
		case 0x9b: codeDst = 0x203a; break;
		case 0xa1: codeDst = 0x0385; break;
		case 0xa2: codeDst = 0x0386; break;
		case 0xa4: codeDst = 0x00a4; break;
		case 0xa5: codeDst = 0x00a5; break;
		case 0xae: codeDst = 0x00ae; break;
		case 0xb4: codeDst = 0x0384; break;
		case 0xb5: codeDst = 0x00b5; break;
		case 0xb6: codeDst = 0x00b6; break;
		default:   codeDst = cc_iso8859_7_to_ucs2(codeSrc);
	}
	
	return codeDst;
}
				
/* end of file */
