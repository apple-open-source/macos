/* 
 * Copyright (c) 2012 Apple, Inc. All Rights Reserved.
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


#include "testmore.h"
#include "capabilities.h"
#include "testbyteBuffer.h"

#if (COMMON_CRC == 0)
entryPoint(CommonCRCTest,"CRC")
#else
#include <CommonNumerics/CommonCRC.h>

static const int kTestTestCount = 39;

static int
doCRC(CNcrc alg, char *data, uint64_t expected)
{
    uint64_t result = 0;
    CNCRC(alg, data, strlen(data), &result);
    ok(result == expected, "chksums match\n");
    // printf("Expected %16llx\n", expected);
    // printf("Got      %16llx\n", result);
    return 0;
}

int CommonCRCTest(int argc, char *const *argv)
{
	plan_tests(kTestTestCount);
    doCRC(kCN_CRC_32_Adler, "Mark Adler", 0x13070394);
    doCRC(kCN_CRC_32_Adler, "resume", 0x09150292);
    doCRC(kCN_CRC_32_Adler, "foofoofoofoo", 0x20D00511);
    doCRC(kCN_CRC_32, "123456789", 0xCBF43926); // CRC32 IEEE 802.3
    doCRC(kCN_CRC_32, "foofoofoofoo", 0xd18e130c);
    doCRC(kCN_CRC_16_CCITT_FALSE, "123456789", 0x29B1);
    doCRC(kCN_CRC_16_CCITT_FALSE, "foofoofoofoo", 0x074F);
    doCRC(kCN_CRC_16_XMODEM, "123456789", 0x0C73);
    doCRC(kCN_CRC_32_CASTAGNOLI, "123456789", 0xE3069283);
    doCRC(kCN_CRC_64_ECMA_182, "123456789", 0x62EC59E3F1A4F00AULL);
    ok(CNCRCWeakTest(kCN_CRC_8) == kCNSuccess, "Self Test");
    ok(CNCRCWeakTest(kCN_CRC_8_ICODE) == kCNSuccess, "Self Test");
    ok(CNCRCWeakTest(kCN_CRC_8_ITU) == kCNSuccess, "Self Test");
    ok(CNCRCWeakTest(kCN_CRC_8_ROHC) == kCNSuccess, "Self Test");
    ok(CNCRCWeakTest(kCN_CRC_8_WCDMA) == kCNSuccess, "Self Test");
    ok(CNCRCWeakTest(kCN_CRC_16) == kCNSuccess, "Self Test");
    ok(CNCRCWeakTest(kCN_CRC_16_CCITT_TRUE) == kCNSuccess, "Self Test");
    ok(CNCRCWeakTest(kCN_CRC_16_CCITT_FALSE) == kCNSuccess, "Self Test");
    ok(CNCRCWeakTest(kCN_CRC_16_USB) == kCNSuccess, "Self Test");
    ok(CNCRCWeakTest(kCN_CRC_16_XMODEM) == kCNSuccess, "Self Test");
    ok(CNCRCWeakTest(kCN_CRC_16_DECT_R) == kCNSuccess, "Self Test");
    ok(CNCRCWeakTest(kCN_CRC_16_DECT_X) == kCNSuccess, "Self Test");
    ok(CNCRCWeakTest(kCN_CRC_16_ICODE) == kCNSuccess, "Self Test");
    ok(CNCRCWeakTest(kCN_CRC_16_VERIFONE) == kCNSuccess, "Self Test");
    ok(CNCRCWeakTest(kCN_CRC_16_A) == kCNSuccess, "Self Test");
    ok(CNCRCWeakTest(kCN_CRC_16_B) == kCNSuccess, "Self Test");
    ok(CNCRCWeakTest(kCN_CRC_16_Fletcher) == kCNSuccess, "Self Test");
    ok(CNCRCWeakTest(kCN_CRC_32_Adler) == kCNSuccess, "Self Test");
    ok(CNCRCWeakTest(kCN_CRC_32) == kCNSuccess, "Self Test");
    ok(CNCRCWeakTest(kCN_CRC_32_CASTAGNOLI) == kCNSuccess, "Self Test");
    ok(CNCRCWeakTest(kCN_CRC_32_BZIP2) == kCNSuccess, "Self Test");
    ok(CNCRCWeakTest(kCN_CRC_32_MPEG_2) == kCNSuccess, "Self Test");
    ok(CNCRCWeakTest(kCN_CRC_32_POSIX) == kCNSuccess, "Self Test");
    ok(CNCRCWeakTest(kCN_CRC_32_XFER) == kCNSuccess, "Self Test");
    ok(CNCRCWeakTest(kCN_CRC_64_ECMA_182) == kCNSuccess, "Self Test");

    diag("Dumping 4 CRC tables - if all is well");
    ok(CNCRCDumpTable(kCN_CRC_8) == kCNSuccess, "Dump 8 bit CRC Table");
    ok(CNCRCDumpTable(kCN_CRC_16) == kCNSuccess, "Dump 16 bit CRC Table");
    ok(CNCRCDumpTable(kCN_CRC_32) == kCNSuccess, "Dump 32 bit CRC Table");
    ok(CNCRCDumpTable(kCN_CRC_64_ECMA_182) == kCNSuccess, "Dump 64 bit CRC Table");

    return 0;
}
#endif

