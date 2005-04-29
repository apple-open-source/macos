/* ===EmacsMode: -*- Mode: C; tab-width:4; c-basic-offset: 4; -*- === */
/* ===FileName: ===

   xaterm special encoding to unicode table

   Copyright (c) 1998 Takuya SHIOZAKI, All Rights reserved.
   Copyright (c) 2000 Pablo Saratxaga <pablo@mandrakesoft.com>

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

	This is the encoding used by the ISIRI-3342 Iranian standard

				Pablo Saratxaga <pablo@mandrakesoft.com>

 */
/* $XFree86: xc/extras/X-TrueType/ARABIC/ISIRI3342toUCS2.c,v 1.2 2003/10/22 16:25:24 tsi Exp $ */

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

static ucs2_t tblIsiri_3342ToUcs2[] = {
/* 0x00A0 - 0x00FF */
	0x0020, 0x200C, 0x200D, 0x0021, 0x00A4, 0x066A, 0x002E, 0x066C,
       	0x0029, 0x0028, 0x00D7, 0x002B, 0x060C, 0x002D, 0x066B, 0x002F,
       	0x06F0, 0x06F1, 0x06F2, 0x06F3, 0x06F4, 0x06F5, 0x06F6, 0x06F7,
       	0x06F8, 0x06F9, 0x003A, 0x061B, 0x003C, 0x003D, 0x003E, 0x061F,
       	0x0622, 0x0627, 0x0621, 0x0628, 0x067E, 0x062A, 0x062B, 0x062C,
       	0x0686, 0x062D, 0x062E, 0x062F, 0x0630, 0x0631, 0x0632, 0x0698,
       	0x0633, 0x0634, 0x0635, 0x0636, 0x0637, 0x0638, 0x0639, 0x063A,
       	0x0641, 0x0642, 0x06A9, 0x06AF, 0x0644, 0x0645, 0x0646, 0x0648,
       	0x0647, 0x06CC, 0x005D, 0x005B, 0x007D, 0x007B, 0x00AB, 0x00BB,
       	0x002A, 0x0640, 0x007C, 0x005C, ALTCHR, ALTCHR, ALTCHR, ALTCHR,
       	0x064E, 0x0650, 0x064F, 0x064B, 0x064D, 0x064C, 0x0651, 0x0652,
       	0x0623, 0x0624, 0x0625, 0x0626, 0x0629, 0x0643, 0x064A, 0x007F
};

CODE_CONV_ISO8859_TO_UCS2(cc_isiri_3342_to_ucs2, /* function name */
                          tblIsiri_3342ToUcs2, /* table name */
			  ALTCHR /* alt char code (on UCS2) */
			  )


/* end of file */
