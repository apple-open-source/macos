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

#include "xttversion.h"

static char const * const releaseID =
    _XTT_RELEASE_NAME;

#include "xttcommon.h"
#include "xttcap.h"
#include "xttcconv.h"
#include "xttcconvP.h"


typedef enum
{
    KSC5601, KSC5601_EUC
} CharSetMagic;

static CharSetRelation const charSetRelations[] = {
    { "ksc5601", NULL, "0", KSC5601,     { 0x20, 0x7f, 0x21, 0x7d, 0x2121 } },
    { "ksc5601", NULL, "1", KSC5601_EUC, { 0xa0, 0xff, 0xa1, 0xfd, 0xa1a1 } },
    { NULL, NULL, NULL, 0, { 0, 0, 0, 0, 0 } }
};


CODECONV_TEMPLATE(cc_ksc5601_to_ucs2);
CODECONV_TEMPLATE(cc_ksc5601euc_to_ucs2);
CODECONV_TEMPLATE(cc_ksc5601_to_wansung);
CODECONV_TEMPLATE(cc_ksc5601euc_to_wansung);
static MapIDRelation const mapIDRelations[] = {
    { KSC5601,      EPlfmISO,     EEncISO10646,
                                  cc_ksc5601_to_ucs2,                  NULL },
    { KSC5601,      EPlfmUnicode, EEncAny,
                                  cc_ksc5601_to_ucs2,                  NULL },
    { KSC5601,      EPlfmMS,      EEncMSUnicode,
                                  cc_ksc5601_to_ucs2,                  NULL },
    { KSC5601,      EPlfmMS,      EEncMSWansung,
                                  cc_ksc5601_to_wansung,               NULL },
    { KSC5601_EUC,  EPlfmISO,     EEncISO10646,
                                  cc_ksc5601euc_to_ucs2,               NULL },
    { KSC5601_EUC,  EPlfmUnicode, EEncAny,
                                  cc_ksc5601euc_to_ucs2,               NULL },
    { KSC5601_EUC,  EPlfmMS,      EEncMSUnicode,
                                  cc_ksc5601euc_to_ucs2,               NULL },
    { KSC5601_EUC,  EPlfmMS,      EEncMSWansung,
                                  cc_ksc5601euc_to_wansung,            NULL },
    { -1, 0, 0, NULL, NULL }
};

STD_ENTRYFUNC_TEMPLATE(KSC5601_entrypoint)


/* KSC5601-1 -> KSC5601 */
ft_char_code_t /* result charCodeDest */
cc_ksc5601euc_to_ucs2(ft_char_code_t idx)
{
    return cc_ksc5601_to_ucs2(idx & 0x7f7f);
}

/* KSC5601 -> Wansung */
ft_char_code_t /* result charCodeDest */
cc_ksc5601_to_wansung(ft_char_code_t idx)
{
    /* just set MSB */
    return idx | 0x8080;
}  

/* KSC5601-1 -> Wansung */
ft_char_code_t /* result charCodeDest */
cc_ksc5601euc_to_wansung(ft_char_code_t idx)
{
    /* it is same code, in different names */
    return idx;
}  

/* end of file */
