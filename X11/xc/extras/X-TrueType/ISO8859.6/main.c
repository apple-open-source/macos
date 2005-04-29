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

	added support for iso-8859-6-8 and iso-8859-16 encoded fonts
        and for asmo 449+ encoded ones.

	PROBLEM: I have no idea what the X11 names are for those encodings
	I need input here. In the meanwhile I'll use *-iso8859-6_{8,16}
	and *-iso8859-6_asmo
				Pablo Saratxaga <srtxg@chanae.alphanet.ch>

	NOTE: iso-8859-6-8 X11 name is *-iso8859-6.8x

 */
/* $XFree86: xc/extras/X-TrueType/ISO8859.6/main.c,v 1.3 2003/10/22 16:25:41 tsi Exp $ */

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
    ISO8859_6,
    ISO8859_6_8,
    ISO8859_6_16,
    ASMO449_PLUS	
} CharSetMagic;

static CharSetRelation const charSetRelations[] = {
    { "iso8859",  NULL, "6",	  ISO8859_6,
		{ 0x20, 0xff,    0,     0,  0x20 } },
    { "iso8859",  NULL, "6.8x",   ISO8859_6_8,
                { 0x20, 0xff,    0,     0,  0x20 } },
    { "iso8859",  NULL, "6_8",    ISO8859_6_8,
                { 0x20, 0xff,    0,     0,  0x20 } },
    { "iso8859",  NULL, "6.16x",  ISO8859_6_16,
		{ 0x20, 0xff,    0,     0,  0x20 } },
    { "iso8859",  NULL, "6_16",   ISO8859_6_16,
		{ 0x20, 0xff,    0,     0,  0x20 } },
    { "iso8859",  NULL, "6_asmo", ASMO449_PLUS,
		{ 0x20, 0xff,    0,     0,  0x20 } },
    { "asmo",     NULL, "449",    ASMO449_PLUS,
                { 0x20, 0xff,    0,     0,  0x20 } },
    { "asmo",     NULL, "449+",   ASMO449_PLUS,
                { 0x20, 0xff,    0,     0,  0x20 } },
    { NULL, NULL, NULL, 0, { 0, 0, 0, 0, 0 } }
};


CODECONV_TEMPLATE(cc_iso8859_6_to_ucs2);
CODECONV_TEMPLATE(cc_iso8859_6_8_to_ucs2);
CODECONV_TEMPLATE(cc_iso8859_6_16_to_ucs2);
CODECONV_TEMPLATE(cc_asmo449_plus_to_ucs2);
static MapIDRelation const mapIDRelations[] = {
    { ISO8859_6,    EPlfmISO,     EEncISO10646,
                                  cc_iso8859_6_to_ucs2,                NULL },
    { ISO8859_6,    EPlfmUnicode, EEncAny,
                                  cc_iso8859_6_to_ucs2,                NULL },
    { ISO8859_6,    EPlfmMS,      EEncMSUnicode,
                                  cc_iso8859_6_to_ucs2,                NULL },
    { ISO8859_6_8,  EPlfmISO,     EEncISO10646,
                                  cc_iso8859_6_8_to_ucs2,              NULL },
    { ISO8859_6_8,  EPlfmUnicode, EEncAny,
                                  cc_iso8859_6_8_to_ucs2,              NULL },
    { ISO8859_6_8,  EPlfmMS,      EEncMSUnicode,
                                  cc_iso8859_6_8_to_ucs2,	       NULL },
    { ISO8859_6_16, EPlfmISO,     EEncISO10646,
                                  cc_iso8859_6_16_to_ucs2,             NULL },
    { ISO8859_6_16, EPlfmUnicode, EEncAny,
                                  cc_iso8859_6_16_to_ucs2,             NULL },
    { ISO8859_6_16, EPlfmMS,      EEncMSUnicode,
                                  cc_iso8859_6_16_to_ucs2,             NULL },
    { ASMO449_PLUS, EPlfmISO,     EEncISO10646,
                                  cc_asmo449_plus_to_ucs2,             NULL },
    { ASMO449_PLUS, EPlfmUnicode, EEncAny,
                                  cc_asmo449_plus_to_ucs2,             NULL },
    { ASMO449_PLUS, EPlfmMS,      EEncMSUnicode,
                                  cc_asmo449_plus_to_ucs2,             NULL },
    { -1, 0, 0, NULL, NULL }
};

STD_ENTRYFUNC_TEMPLATE(ISO8859_6_entrypoint)

/* end of file */
