//
//  SOSGenCount.c
//  sec
//
//  Created by Richard Murphy on 1/29/15.
//
//

#include "SOSGenCount.h"
#include <utilities/SecCFWrappers.h>
#include <CoreFoundation/CFLocale.h>

static CFAbsoluteTime SOSGenerationCountGetDateBits(int64_t genValue) {
    return (uint32_t) ((((uint64_t) genValue) >> 32) << 1);
}

void SOSGenerationCountWithDescription(SOSGenCountRef gen, void (^operation)(CFStringRef description)) {
    CFStringRef description = SOSGenerationCountCopyDescription(gen);

    operation(description);

    CFReleaseSafe(description);
}

CFStringRef SOSGenerationCountCopyDescription(SOSGenCountRef gen) {
    int64_t value = SOSGetGenerationSint(gen);

    CFMutableStringRef gcDecsription = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, CFSTR("["));

    withStringOfAbsoluteTime(SOSGenerationCountGetDateBits(value), ^(CFStringRef decription) {
        CFStringAppend(gcDecsription, decription);
    });

    CFStringAppendFormat(gcDecsription, NULL, CFSTR(" %u]"), (uint32_t)(value & 0xFFFFFFFF));

    return gcDecsription;
}

int64_t SOSGetGenerationSint(SOSGenCountRef gen) {
    int64_t value;
    if(!gen) return 0;
    CFNumberGetValue(gen, kCFNumberSInt64Type, &value);
    return value;
}

static int64_t sosGenerationSetHighBits(int64_t value, int32_t high_31)
{
    value &= 0xFFFFFFFF; // Keep the low 32 bits.
    value |= ((int64_t) high_31) << 32;

    return value;
}

static SOSGenCountRef sosGenerationCreateOrIncrement(SOSGenCountRef gen) {
    int64_t value = 0;

    if(gen) {
        value = SOSGetGenerationSint(gen);
    }

    if((value >> 32) == 0) {
        uint32_t seconds = CFAbsoluteTimeGetCurrent(); // seconds
        value = sosGenerationSetHighBits(value, (seconds >> 1));
    }

    value++;
    return CFNumberCreate(NULL, kCFNumberSInt64Type, &value);
}

SOSGenCountRef SOSGenerationCreate() {
    return sosGenerationCreateOrIncrement(NULL);
}


// We need this for a circle gencount test
SOSGenCountRef SOSGenerationCreateWithValue(int64_t value) {
    return CFNumberCreate(NULL, kCFNumberSInt64Type, &value);
}

SOSGenCountRef SOSGenerationIncrementAndCreate(SOSGenCountRef gen) {
    return sosGenerationCreateOrIncrement(gen);
}

SOSGenCountRef SOSGenerationCopy(SOSGenCountRef gen) {
    if(!gen) return NULL;
    int64_t value = SOSGetGenerationSint(gen);
    return CFNumberCreate(NULL, kCFNumberSInt64Type, &value);
}

bool SOSGenerationIsOlder(SOSGenCountRef current, SOSGenCountRef proposed) {
    return CFNumberCompare(current, proposed, NULL) == kCFCompareGreaterThan;
}

SOSGenCountRef SOSGenerationCreateWithBaseline(SOSGenCountRef reference) {
    SOSGenCountRef retval = SOSGenerationCreate();
    if(!SOSGenerationIsOlder(retval, reference)) {
        CFReleaseNull(retval);
        retval = SOSGenerationIncrementAndCreate(reference);
    }
    return retval;
}
