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
/* $XFree86: xc/extras/X-TrueType/ISO8859.2/main.c,v 1.3 2003/10/22 16:25:40 tsi Exp $ */

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
    ISO8859_2,
    CP1250
} CharSetMagic;

static CharSetRelation const charSetRelations[] = {
    { "iso8859",   NULL,      "2", ISO8859_2, { 0x20, 0xff,  0,  0, 0x20 } },
    { "microsoft", NULL, "cp1250",    CP1250, { 0x20, 0xff,  0,  0, 0x20 } },
    { NULL, NULL, NULL, 0, { 0, 0, 0, 0, 0 } }
};


CODECONV_TEMPLATE(cc_iso8859_2_to_apple_centeuro);
CODECONV_TEMPLATE(cc_iso8859_2_to_ucs2);
CODECONV_TEMPLATE(cc_cp1250_to_ucs2);
static MapIDRelation const mapIDRelations[] = {
    { ISO8859_2,    EPlfmISO,     EEncISO10646,
                                  cc_iso8859_2_to_ucs2,                NULL },
    { ISO8859_2,    EPlfmUnicode, EEncAny,
                                  cc_iso8859_2_to_ucs2,                NULL },
    { ISO8859_2,    EPlfmMS,      EEncMSUnicode,
                                  cc_iso8859_2_to_ucs2,                NULL },
    { ISO8859_2,    EPlfmApple,   EEncAppleCenteuro,
                                  cc_iso8859_2_to_apple_centeuro,      NULL },
    { CP1250,       EPlfmISO,     EEncISO10646,
                                  cc_cp1250_to_ucs2,                   NULL },
    { CP1250,       EPlfmUnicode, EEncAny,
                                  cc_cp1250_to_ucs2,                   NULL },
    { CP1250,       EPlfmMS,      EEncMSUnicode,
                                  cc_cp1250_to_ucs2,                   NULL },
    { -1, 0, 0, NULL, NULL }
};

STD_ENTRYFUNC_TEMPLATE(ISO8859_2_entrypoint)

ucs2_t
cc_cp1250_to_ucs2(ft_char_code_t codeSrc)
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
		case 0x8a: codeDst = 0x0160; break;
		case 0x8b: codeDst = 0x2039; break;
		case 0x8c: codeDst = 0x015a; break;
		case 0x8d: codeDst = 0x0164; break;
		case 0x8e: codeDst = 0x017d; break;
		case 0x8f: codeDst = 0x0179; break;
		case 0x91: codeDst = 0x2018; break;
		case 0x92: codeDst = 0x2019; break;
		case 0x93: codeDst = 0x201c; break;
		case 0x94: codeDst = 0x201d; break;
		case 0x95: codeDst = 0x2022; break;
		case 0x96: codeDst = 0x2013; break;
		case 0x97: codeDst = 0x2014; break;
		case 0x99: codeDst = 0x2122; break;
		case 0x9a: codeDst = 0x0161; break;
		case 0x9b: codeDst = 0x203a; break;
		case 0x9c: codeDst = 0x015b; break;
		case 0x9d: codeDst = 0x0165; break;
		case 0x9e: codeDst = 0x017e; break;
		case 0x9f: codeDst = 0x017a; break;
		case 0xa1: codeDst = 0x02c7; break;
		case 0xa5: codeDst = 0x010a; break;
		case 0xa6: codeDst = 0x00a6; break;
		case 0xa9: codeDst = 0x00a9; break;
		case 0xab: codeDst = 0x00ab; break;
		case 0xac: codeDst = 0x00ac; break;
		case 0xae: codeDst = 0x00ae; break;
		case 0xb1: codeDst = 0x00b1; break;
		case 0xb5: codeDst = 0x00b5; break;
		case 0xb6: codeDst = 0x00b6; break;
		case 0xb7: codeDst = 0x00b7; break;
		case 0xb9: codeDst = 0x00b9; break;
		case 0xbb: codeDst = 0x00bb; break;
		case 0xbc: codeDst = 0x013d; break;
		case 0xbe: codeDst = 0x013e; break;
		default:   codeDst = cc_iso8859_2_to_ucs2(codeSrc);
	}

	return codeDst;
}

/* end of file */
