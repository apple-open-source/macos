/*
 * Copyright (c) 2011-2012,2014 Apple Inc. All Rights Reserved.
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


#include <CoreFoundation/CFData.h>
#include "SecOTRPacketData.h"

#include <corecrypto/ccn.h>

#include "security_regressions.h"

static bool CFDataMatches(CFDataRef data, size_t amount, const uint8_t* bytes)
{
    if ((size_t) CFDataGetLength(data) != amount)
        return false;
    
    return 0 == memcmp(CFDataGetBytePtr(data), bytes, amount);
    
}


static void testAppendShort()
{
    CFMutableDataRef result = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    const uint8_t firstResult[] = { 1, 2 };
    AppendShort(result, 0x0102);
    ok(CFDataMatches(result, sizeof(firstResult), firstResult), "Didn't insert correctly");
    
    const uint8_t secondResult[] = { 1, 2, 3, 4};
    AppendShort(result, 0x0304);
    ok(CFDataMatches(result, sizeof(secondResult), secondResult), "Didn't append!");
    
    CFRelease(result);
    
    result = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    const uint8_t thirdResult[] = { 0, 0 };
    AppendShort(result, 0x0);
    ok(CFDataMatches(result, sizeof(thirdResult), thirdResult), "Didn't insert zero");
    
    CFRelease(result);
    
    result = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    const uint8_t fourthResult[] = { 0xFF, 0xFF};
    AppendShort(result, 0xFFFF);
    ok(CFDataMatches(result, sizeof(fourthResult), fourthResult), "Didn't insert 0xFFFFFFFF");
    
    CFRelease(result);
}

static void testAppendLong()
{
    CFMutableDataRef result = CFDataCreateMutable(kCFAllocatorDefault, 0);

    const uint8_t firstResult[] = { 1, 2, 3, 4 };
    AppendLong(result, 0x01020304);
    ok(CFDataMatches(result, sizeof(firstResult), firstResult), "Didn't insert correctly");

    const uint8_t secondResult[] = { 1, 2, 3, 4, 5, 6, 7, 8};
    AppendLong(result, 0x05060708);
    ok(CFDataMatches(result, sizeof(secondResult), secondResult), "Didn't append!");

    CFRelease(result);

    result = CFDataCreateMutable(kCFAllocatorDefault, 0);

    const uint8_t thirdResult[] = { 0, 0, 0, 0 };
    AppendLong(result, 0x0);
    ok(CFDataMatches(result, sizeof(thirdResult), thirdResult), "Didn't insert zero");

    CFRelease(result);

    result = CFDataCreateMutable(kCFAllocatorDefault, 0);

    const uint8_t fourthResult[] = { 0xFF, 0xFF, 0xFF, 0xFF };
    AppendLong(result, 0xFFFFFFFF);
    ok(CFDataMatches(result, sizeof(fourthResult), fourthResult), "Didn't insert 0xFFFFFFFF");

    CFRelease(result);
}

static void testAppendLongLong()
{
    CFMutableDataRef result = CFDataCreateMutable(kCFAllocatorDefault, 0);

    const uint8_t firstResult[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    AppendLongLong(result, 0x0102030405060708);
    ok(CFDataMatches(result, sizeof(firstResult), firstResult), "insert correctly");

    const uint8_t secondResult[] = { 1, 2, 3, 4, 5, 6, 7, 8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x10};
    AppendLongLong(result, 0x090A0B0C0D0E0F10);
    ok(CFDataMatches(result, sizeof(secondResult), secondResult), "append!");

    CFRelease(result);

    result = CFDataCreateMutable(kCFAllocatorDefault, 0);

    const uint8_t thirdResult[] = { 0, 0, 0, 0, 0, 0, 0, 0};
    AppendLongLong(result, 0x0);
    ok(CFDataMatches(result, sizeof(thirdResult), thirdResult), "insert zero");

    CFRelease(result);

    result = CFDataCreateMutable(kCFAllocatorDefault, 0);

    const uint8_t fourthResult[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    AppendLongLong(result, 0xFFFFFFFFFFFFFFFF);
    ok(CFDataMatches(result, sizeof(fourthResult), fourthResult), "insert 0xFFFFFFFFFFFFFFF");
    
    CFRelease(result);
}

static void testAppendLongLongCompact()
{
    CFMutableDataRef result = CFDataCreateMutable(kCFAllocatorDefault, 0);

    const uint64_t firstValue = 0x000001FC07F;
    const uint8_t firstResult[] = { 0xFF,0x80,0x7F };
    AppendLongLongCompact(result, firstValue);
    ok(CFDataMatches(result, sizeof(firstResult), firstResult), "insert correctly");

    uint64_t readback;
    const uint8_t *readPtr = firstResult;
    size_t size = sizeof(firstResult);
    ReadLongLongCompact(&readPtr, &size, &readback);

    is(readback, firstValue, "read back");

    const uint8_t secondResult[] = { 0xFF,0x80,0x7F, 0x82, 0x80, 0x81, 0x80, 0xff, 0x80, 0x7f };
    AppendLongLongCompact(result, 0x800101FC07F);
    ok(CFDataMatches(result, sizeof(secondResult), secondResult), "append!");

    CFRelease(result);

    result = CFDataCreateMutable(kCFAllocatorDefault, 0);

    const uint8_t thirdResult[] = { 0 };
    AppendLongLongCompact(result, 0x0);
    ok(CFDataMatches(result, sizeof(thirdResult), thirdResult), "insert zero");

    readPtr = thirdResult;
    size = sizeof(thirdResult);
    ReadLongLongCompact(&readPtr, &size, &readback);
    is(readback, 0ULL, "read back");

    CFRelease(result);

    result = CFDataCreateMutable(kCFAllocatorDefault, 0);

    const uint8_t fourthResult[] = { 0x81, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F };
    AppendLongLongCompact(result, 0xFFFFFFFFFFFFFFFF);
    ok(CFDataMatches(result, sizeof(fourthResult), fourthResult), "insert 0xFFFFFFFFFFFFFFF");

    readPtr = fourthResult;
    size = sizeof(fourthResult);
    ReadLongLongCompact(&readPtr, &size, &readback);
    is(readback, 0xFFFFFFFFFFFFFFFF, "read back");

    CFRelease(result);
}

static void testAppendData()
{
    CFMutableDataRef result = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    const uint8_t firstResult[] = { 0, 0, 0, 4, 1, 2, 3, 4 };
    AppendDATA(result, sizeof(firstResult) - 4, firstResult + 4);
    ok(CFDataMatches(result, sizeof(firstResult), firstResult), "Didn't insert correctly");
    
    const uint8_t secondResult[] = { 0, 0, 0, 4, 1, 2, 3, 4, 0, 0, 0, 1, 0 };
    AppendDATA(result, sizeof(secondResult) - 12, secondResult + 12);
    ok(CFDataMatches(result, sizeof(secondResult), secondResult), "Didn't append!");
    
    CFRelease(result);
    
    result = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    const uint8_t thirdResult[] = { 0, 0, 0, 2, 0, 0 };
    AppendDATA(result, sizeof(thirdResult) - 4, thirdResult + 4);
    ok(CFDataMatches(result, sizeof(thirdResult), thirdResult), "Didn't insert correctly");
    
    CFRelease(result);
    
    result = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    const uint8_t fourthResult[] = { 0, 0, 0, 5, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    AppendDATA(result, sizeof(fourthResult) - 4, fourthResult + 4);
    ok(CFDataMatches(result, sizeof(fourthResult), fourthResult), "Didn't insert correctly");
    
    CFRelease(result);
}


static void testAppendMPI()
{
    const size_t kUnitBufferN = 1024;
    cc_unit unitBuffer[1024];
    
    CFMutableDataRef result = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    const uint8_t firstResult[] = { 0, 0, 0, 2, 1, 2 };
    ccn_read_uint(kUnitBufferN, unitBuffer, sizeof(firstResult) - 4, firstResult + 4);
    AppendMPI(result, kUnitBufferN, unitBuffer);
    
    ok(CFDataMatches(result, sizeof(firstResult), firstResult), "Didn't insert zero");
    
    const uint8_t secondResult[] = { 0, 0, 0, 2, 1, 2, 0, 0, 0, 3, 5, 6, 7 };
    ccn_read_uint(kUnitBufferN, unitBuffer, sizeof(secondResult) - 10, secondResult + 10);
    AppendMPI(result, kUnitBufferN, unitBuffer);
    
    ok(CFDataMatches(result, sizeof(secondResult), secondResult), "Didn't append zero");
    
    CFRelease(result);
    
    result = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    const uint8_t thirdResult[] = { 0, 0, 0, 1, 1 };
    ccn_read_uint(kUnitBufferN, unitBuffer, sizeof(thirdResult) - 4, thirdResult + 4);
    AppendMPI(result, kUnitBufferN, unitBuffer);
    
    ok(CFDataMatches(result, sizeof(thirdResult), thirdResult), "Didn't insert zero");
    
    CFRelease(result);
    
    result = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    const uint8_t fourthResult[] = { 0, 0, 0, 7, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF };
    ccn_read_uint(kUnitBufferN, unitBuffer, sizeof(fourthResult) - 4, fourthResult + 4);
    AppendMPI(result, kUnitBufferN, unitBuffer);
    
    ok(CFDataMatches(result, sizeof(fourthResult), fourthResult), "Didn't insert zero");
    
    CFRelease(result);
    
    result = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    const uint8_t paddedData[] = { 0, 0xCC, 0xDD, 0xEE, 0xFF, 0xAA };
    const uint8_t shortenedResult[] = { 0, 0, 0, 5, 0xCC, 0xDD, 0xEE, 0xFF, 0xAA };
    ccn_read_uint(kUnitBufferN, unitBuffer, sizeof(paddedData), paddedData);
    AppendMPI(result, kUnitBufferN, unitBuffer);
    
    ok(CFDataMatches(result, sizeof(shortenedResult), shortenedResult), "Didn't insert zero");
    
    CFRelease(result);
    
}

int otr_packetdata(int argc, char *const * argv)
{
    plan_tests(28);

    testAppendShort();
    testAppendLong();
    testAppendLongLong();
    testAppendLongLongCompact();
    testAppendData();
    testAppendMPI();

    return 0;
}

