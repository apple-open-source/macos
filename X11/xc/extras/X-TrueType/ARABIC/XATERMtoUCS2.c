/* ===EmacsMode: -*- Mode: C; tab-width:4; c-basic-offset: 4; -*- === */
/* ===FileName: ===

   xaterm special encoding to unicode table

   Copyright (c) 1998 Takuya SHIOZAKI, All Rights reserved.
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

	This is the encoding used by the xaterm program (an arabic
	xterm). The font it uses is in fact a 16 bit encoded one,
	whith 0x0000 -> 0x00ff equal to iso-8859-1, 
	and 0x0100 -> 0x01ff whith the arabic glyphs.
	As the lower half is an equivalent mapping to unicode there
	is no need for a table.

				Pablo Saratxaga <srtxg@chanae.alphanet.ch>

 */
/* $XFree86: xc/extras/X-TrueType/ARABIC/XATERMtoUCS2.c,v 1.3 2003/10/22 16:25:24 tsi Exp $ */

#include "xttversion.h"

#if 0
static char const * const releaseID =
    _XTT_RELEASE_NAME;
#endif

#include "xttcommon.h"
#include "xttcap.h"
#include "xttcconv.h"
#include "xttcconvP.h"

#define ALTCHR 0x0020

static ucs2_t tblXaterm_TophalfToUcs2[] = {
    ALTCHR, 0xfefa, 0xfef8, 0xfef9, 0xfef7, 0xfe88, 0xfe84, 0xfe86,
    0x064b, 0x064c, 0x064d, 0x064e, 0x064f, 0x0650, 0x0651, 0x0652,
    0x0640, 0x0040, 0x0024, 0x2039, 0x203a, 0x061f, 0x0022, 0x003d,
    0x005f, 0x007b, 0x007d, 0x005b, 0x005d, 0x005c, 0x00a6, 0x005e,
    0x0020, 0x0021, ALTCHR, ALTCHR, 0x0023, 0x066a, 0x0026, 0x0027,
    0x0028, 0x0029, 0x002A, 0x002B, 0x060c, 0x002D, 0x06d4, 0x002F,
    0x0660, 0x0661, 0x0662, 0x0663, 0x0664, 0x0665, 0x0666, 0x0667,
    0x0668, 0x0669, 0x061b, 0x003A, 0xfe8d, 0xfe8e, 0xfeed, 0xfeee,
    0xfe91, 0xfe92, 0xfe8f, 0xfe90, 0xfe97, 0xfe98, 0xfe95, 0xfe96,
    0xfe9b, 0xfe9b, 0xfe99, 0xfe9a, 0xfe9f, 0xfea0, 0xfe9d, 0xfe9e,
    0xfea3, 0xfea4, 0xfea1, 0xfea2, 0xfea7, 0xfea8, 0xfea5, 0xfea6,
    0xfeab, 0xfeac, 0xfea9, 0xfeaa, 0xfeaf, 0xfeb0, 0xfead, 0xfeae,
    0xfeb3, 0xfeb4, 0xfeb1, 0xfeb2, 0xfeb7, 0xfeb8, 0xfeb5, 0xfeb6,
    0xfebb, 0xfebc, 0xfeb9, 0xfeba, 0xfebf, 0xfec0, 0xfebd, 0xfebe,
    0xfec3, 0xfec4, 0xfec1, 0xfec2, 0xfec7, 0xfec8, 0xfec5, 0xfec6,
    0xfecb, 0xfecc, 0xfec9, 0xfeca, 0xfecf, 0xfed0, 0xfecd, 0xfece,
    0xfed3, 0xfed4, 0xfed1, 0xfed2, 0xfed7, 0xfed8, 0xfed5, 0xfed6,
    0xfedb, 0xfedc, 0xfed9, 0xfeda, 0xfedf, 0xfec0, 0xfedd, 0xfede,
    0xfee3, 0xfee4, 0xfee1, 0xfee2, 0xfee7, 0xfee8, 0xfee5, 0xfee6,
    0xfeeb, 0xfeec, 0xfee9, 0xfeea, 0xfef3, 0xfef4, 0xfef1, 0xfef2,
    0xfe80, 0xfe81, ALTCHR, 0xfe94, 0xfe83, 0xfe85, 0xfeef, 0xfef0,
    0xfe87, 0xfe93, 0xfe89, 0xfe8a, 0xfe8b, 0xfe8c, 0xfefc, 0xfefb,
    0xfe71, 0xfe73, 0xfe75, 0xfe77, 0xfe79, 0xfe7b, 0xfe7d, 0xfe7f,
    0xfe82, 0xfef5, 0xfef6, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR,
    ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR,
    ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR,
    ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR,
    ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR,
    ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR,
    ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR,
    ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR,
    ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR
};


CODE_CONV_ONE_OCTET_TO_UCS2_ALL(cc_xaterm_tophalf_to_ucs2, /* function name */
                                tblXaterm_TophalfToUcs2) /* table name */


/* end of file */
