/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 *  ccGlobals.c - CommonCrypto global DATA
 */

#include "ccGlobals.h"
#include <corecrypto/ccmd2.h>
#include <corecrypto/ccmd4.h>
#include <corecrypto/ccmd5.h>
#include <corecrypto/ccripemd.h>
#include <corecrypto/ccsha1.h>
#include <corecrypto/ccsha2.h>
#include <corecrypto/ccsha3.h>
#include "basexx.h"

#if !_LIBCOMMONCRYPTO_HAS_ALLOC_ONCE
struct cc_globals_s cc_globals_storage;

#if defined(_MSC_VER)
#include <windows.h>
dispatch_once_t cc_globals_init = INIT_ONCE_STATIC_INIT;
#else
#warning Please check init once static initializer
dispatch_once_t cc_globals_init = 0;
#endif
#endif

static void init_globals_digest(void *g){
    cc_globals_t globals = (cc_globals_t) g;

    memset(globals->digest_info, 0, sizeof (globals->digest_info));
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    globals->digest_info[kCCDigestMD2] = &ccmd2_di;
    globals->digest_info[kCCDigestMD4] = &ccmd4_di;
    globals->digest_info[kCCDigestMD5] = ccmd5_di();
    globals->digest_info[kCCDigestRMD160] = &ccrmd160_di;
    globals->digest_info[kCCDigestSHA1] = ccsha1_di();
#pragma clang diagnostic pop
    globals->digest_info[kCCDigestSHA224] = ccsha224_di();
    globals->digest_info[kCCDigestSHA256] = ccsha256_di();
    globals->digest_info[kCCDigestSHA384] = ccsha384_di();
    globals->digest_info[kCCDigestSHA512] = ccsha512_di();
    globals->digest_info[kCCDigestSHA3_224] = ccsha3_224_di();
    globals->digest_info[kCCDigestSHA3_256] = ccsha3_256_di();
    globals->digest_info[kCCDigestSHA3_384] = ccsha3_384_di();
    globals->digest_info[kCCDigestSHA3_512] = ccsha3_512_di();
}

static void init_globals_basexx(void *g){
    cc_globals_t globals = (cc_globals_t) g;
    
    globals->encoderTab[0].encoderRef = NULL;
    globals->encoderTab[kCNEncodingBase64].encoderRef = &defaultBase64;
    globals->encoderTab[kCNEncodingBase32].encoderRef = &defaultBase32;
    globals->encoderTab[kCNEncodingBase32Recovery].encoderRef = &recoveryBase32;
    globals->encoderTab[kCNEncodingBase32HEX].encoderRef = &hexBase32;
    globals->encoderTab[kCNEncodingBase16].encoderRef = &defaultBase16;
    
    for(int i=1; i<CN_STANDARD_BASE_ENCODERS; i++){//CNEncodings do not start from 0!
        setReverseMap(&globals->encoderTab[i]);
    }
}

static void init_globals_crc(void *g){
    cc_globals_t globals = (cc_globals_t) g;
    
    globals->crcSelectionTab[kCN_CRC_8].descriptor = &CC_crc8;
    globals->crcSelectionTab[kCN_CRC_8_ICODE].descriptor = &CC_crc8_icode;
    globals->crcSelectionTab[kCN_CRC_8_ITU].descriptor = &CC_crc8_itu;
    globals->crcSelectionTab[kCN_CRC_8_ROHC].descriptor = &CC_crc8_rohc;
    globals->crcSelectionTab[kCN_CRC_8_WCDMA].descriptor = &CC_crc8_wcdma;
    globals->crcSelectionTab[kCN_CRC_16].descriptor = &CC_crc16;
    globals->crcSelectionTab[kCN_CRC_16_CCITT_TRUE].descriptor = &CC_crc16_ccitt_true;
    globals->crcSelectionTab[kCN_CRC_16_CCITT_FALSE].descriptor = &CC_crc16_ccitt_false;
    globals->crcSelectionTab[kCN_CRC_16_USB].descriptor = &CC_crc16_usb;
    globals->crcSelectionTab[kCN_CRC_16_XMODEM].descriptor = &CC_crc16_xmodem;
    globals->crcSelectionTab[kCN_CRC_16_DECT_R].descriptor = &CC_crc16_dect_r;
    globals->crcSelectionTab[kCN_CRC_16_DECT_X].descriptor = &CC_crc16_dect_x;
    globals->crcSelectionTab[kCN_CRC_16_ICODE].descriptor = &CC_crc16_icode;
    globals->crcSelectionTab[kCN_CRC_16_VERIFONE].descriptor = &CC_crc16_verifone;
    globals->crcSelectionTab[kCN_CRC_16_A].descriptor = &CC_crc16_a;
    globals->crcSelectionTab[kCN_CRC_16_B].descriptor = &CC_crc16_b;
    globals->crcSelectionTab[kCN_CRC_16_Fletcher].descriptor = NULL;
    globals->crcSelectionTab[kCN_CRC_32_Adler].descriptor = &CC_adler32;
    globals->crcSelectionTab[kCN_CRC_32].descriptor = &CC_crc32;
    globals->crcSelectionTab[kCN_CRC_32_CASTAGNOLI].descriptor = &CC_crc32_castagnoli;
    globals->crcSelectionTab[kCN_CRC_32_BZIP2].descriptor = &CC_crc32_bzip2;
    globals->crcSelectionTab[kCN_CRC_32_MPEG_2].descriptor = &CC_crc32_mpeg_2;
    globals->crcSelectionTab[kCN_CRC_32_POSIX].descriptor = &CC_crc32_posix;
    globals->crcSelectionTab[kCN_CRC_32_XFER].descriptor = &CC_crc32_xfer;
    globals->crcSelectionTab[kCN_CRC_64_ECMA_182].descriptor = &CC_crc64_ecma_182;
}

void init_globals(void *g){
    init_globals_digest(g);
    init_globals_basexx(g);
    init_globals_crc(g);
}
