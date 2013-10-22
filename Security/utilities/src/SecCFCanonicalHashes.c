//
//  SecCFCanonicalHashes.c
//  utilities
//
//  Created by Mitch Adler on 2/8/12.
//  Copyright (c) 2012 Apple Inc. All rights reserved.
//

#include <stdio.h>

#include "utilities/SecCFCanonicalHashes.h"
#include "utilities/comparison.h"

#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFData.h>
#include <utilities/SecCFRelease.h>
#include <corecrypto/ccdigest.h>

struct AddKeyValueHashContext {
    CFMutableArrayRef array;
    const struct ccdigest_info* di;
    bool fail;
};

static void AddKeyValueHashData(const void *key, const void *value, void *context)
{
    struct AddKeyValueHashContext* akvContext = (struct AddKeyValueHashContext*) context;

    size_t key_len;
    const void* key_data;
    if (CFGetTypeID(key) == CFStringGetTypeID()) {
        key_len = CFStringGetLength((CFStringRef) key);
        key_data = CFStringGetCharactersPtr((CFStringRef) key);
    } else {
        akvContext->fail = true;
        return;
    }

    size_t value_len;
    const void* value_data;
    if (CFGetTypeID(key) == CFStringGetTypeID()) {
        value_len = CFStringGetLength((CFStringRef) value);
        value_data = CFStringGetCharactersPtr((CFStringRef) value);
    } else if (CFGetTypeID(key) == CFDataGetTypeID()) {
        value_len = CFDataGetLength((CFDataRef)value);
        value_data = CFDataGetBytePtr((CFDataRef)value);
    } else {
        akvContext->fail = true;
        return;
    }

    UInt8 hashBuffer[akvContext->di->output_size];

    ccdigest_di_decl(akvContext->di, finalContext);

    ccdigest(akvContext->di, key_len, key_data, hashBuffer);

    ccdigest_init(akvContext->di, finalContext);
    ccdigest_update(akvContext->di, finalContext, sizeof(hashBuffer), hashBuffer);

    ccdigest_update(akvContext->di, finalContext, sizeof(hashBuffer), hashBuffer);
    ccdigest(akvContext->di, value_len, value_data, hashBuffer);

    ccdigest_final(akvContext->di, finalContext, (void*)hashBuffer);

    CFDataRef hash = CFDataCreate(kCFAllocatorDefault, hashBuffer, sizeof(hashBuffer));
    CFArrayAppendValue(akvContext->array, hash);
    CFReleaseSafe(hash);
}

static CFComparisonResult CFDataCompare(CFDataRef d1, CFDataRef d2)
{
    CFIndex d1_size = CFDataGetLength(d1);
    CFIndex d2_size = CFDataGetLength(d2);

    CFIndex comparison = memcmp(CFDataGetBytePtr(d1), CFDataGetBytePtr(d2), MIN(d1_size, d2_size));

    if (comparison == 0)
        comparison = d1_size - d2_size;

    return (comparison > 0) ? kCFCompareGreaterThan : ((comparison < 0) ? kCFCompareLessThan : kCFCompareEqualTo);
}

static CFComparisonResult CFEqualComparitor(const void *val1, const void *val2, void * context __unused)
{
    return CFDataCompare((CFDataRef) val1, (CFDataRef) val2);
}

struct array_hashing_context {
    const struct ccdigest_info* di;
    struct ccdigest_ctx *       ctx;
};

static void hash_CFDatas(const void *value, void *context)
{
    struct array_hashing_context* ahc = (struct array_hashing_context*) context;

    ccdigest_update(ahc->di, ahc->ctx, CFDataGetLength((CFDataRef) value), CFDataGetBytePtr((CFDataRef) value));
}


bool SecCFAppendCFDictionaryHash(CFDictionaryRef thisDictionary, const struct ccdigest_info* di, CFMutableDataRef toThis)
{
    CFMutableArrayRef hashArray = CFArrayCreateMutable(kCFAllocatorDefault, CFDictionaryGetCount(thisDictionary), &kCFTypeArrayCallBacks);

    CFDictionaryApplyFunction(thisDictionary, &AddKeyValueHashData, hashArray);

    CFRange wholeArray = CFRangeMake(0, CFArrayGetCount(hashArray));

    CFArraySortValues(hashArray, wholeArray, &CFEqualComparitor, NULL);

    ccdigest_di_decl(di, finalContext);

    struct array_hashing_context ahc = { .di = di, .ctx = (struct ccdigest_ctx*)&finalContext};

    CFArrayApplyFunction(hashArray, wholeArray, hash_CFDatas, &ahc);

    return true;
}
