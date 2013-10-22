//
//  otr-packetdata.c
//  OTR
//
//  Created by Mitch Adler on 7/22/11.
//  Copyright (c) 2011 Apple Inc. All rights reserved.
//

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
    ok(CFDataMatches(result, sizeof(thirdResult), fourthResult), "Didn't insert 0xFFFFFFFF");
    
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
    ok(CFDataMatches(result, sizeof(thirdResult), fourthResult), "Didn't insert 0xFFFFFFFF");
    
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
    plan_tests(17);

    testAppendShort();
    testAppendLong();
    testAppendData();
    testAppendMPI();
    
    return 0;
}

