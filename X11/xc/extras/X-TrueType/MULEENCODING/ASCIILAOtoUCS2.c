/* ===EmacsMode: -*- Mode: C; tab-width:4; c-basic-offset: 4; -*- === */
/* ===FileName: ===

   IBM Laotian encoding to unicode table

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

	http://www.unicode.org/pending/lao/cp01133.gif

 */
/* $XFree86: xc/extras/X-TrueType/MULEENCODING/ASCIILAOtoUCS2.c,v 1.2 2003/10/22 16:25:48 tsi Exp $ */

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

static ucs2_t tblAsciilaoToUcs2[] = {
/* 0x00A0 - 0x00FF */
    ALTCHR, 0x0E81, 0x0E82, 0x0E84, 0x0E87, 0x0E88, 0x0EAA, 0x0E8A,
    0x0E8D, 0x0E94, 0x0E95, 0x0E96, 0x0E97, 0x0E99, 0x0E9A, 0x0E9B,
    0x0E9C, 0x0E9D, 0x0E9E, 0x0E9F, 0x0EA1, 0x0EA2, 0x0EA3, 0x0EA5,
    0x0EA7, 0x0EAB, 0x0EAD, 0x0EAE, ALTCHR, ALTCHR, ALTCHR, 0x0EAF,
    0x0EB0, 0x0EB2, 0x0EB3, 0x0EB4, 0x0EB5, 0x0EB6, 0x0EB7, 0x0EB8,
    0x0EB9, 0x0EBC, 0x0EB1, 0x0EBB, 0x0EBD, ALTCHR, ALTCHR, ALTCHR,
    0x0EC0, 0x0EC1, 0x0EC2, 0x0EC3, 0x0EC4, 0x0EC8, 0x0EC9, 0x0ECA,
    0x0ECB, 0x0ECC, 0x0ECD, 0x0EC6, ALTCHR, 0x0EDC, 0x0EDD, 0x20AD,
    ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR,
    ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR,
    0x0ED0, 0x0ED1, 0x0ED2, 0x0ED3, 0x0ED4, 0x0ED5, 0x0ED6, 0x0ED7,
    0x0ED8, 0x0ED9, ALTCHR, ALTCHR, 0x00A2, 0x00AC, 0x00A6, ALTCHR
};

CODE_CONV_ISO8859_TO_UCS2(cc_asciilao_to_ucs2, /* function name */
                          tblAsciilaoToUcs2, /* table name */
                          ALTCHR /* alt char code (on UCS2) */
                          )

/* end of file */
