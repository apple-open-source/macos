//
//  SecCFWrappers.h
//
//  Created by Mitch Adler on 1/27/11.
//  Copyright 2011 Apple Inc. All rights reserved.
//

#ifndef _SECCFWRAPPERS_H_
#define _SECCFWRAPPERS_H_

#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFDate.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFPropertyList.h>

#include <utilities/SecCFRelease.h>
#include <utilities/debugging.h>

#include <assert.h>
#include <dispatch/dispatch.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


//
// Convenience routines.
//

//
// Macros for the pattern
//
// typedef struct _privateNewClass* NewClassRef;
//
// struct _privateNewClass {
//      CFRuntimeBase _base;
//      ... class additions
// };
//
// kClassNameRegisterClass
// kClassNameTypeID
//
// ClassNameGetTypeID()
//
// CFGiblisFor(NewClass);
//
// .. define NewClassDestroy
// .. define NewClassCopyDescription
//
// .. use CFTypeAllocate(NewClass, _privateNewClass, allocator);
//
//

#define CFGiblisWithFunctions(gibliClassName, describe_func, destroy_func, compare_func, hash_func) \
    CFTypeID gibliClassName##GetTypeID(void); \
    CFTypeID gibliClassName##GetTypeID(void) { \
    static dispatch_once_t  k##gibliClassName##RegisterClass; \
    static CFTypeID         k##gibliClassName##TypeID = _kCFRuntimeNotATypeID; \
    \
    dispatch_once(&k##gibliClassName##RegisterClass, ^{ \
        static const CFRuntimeClass k##gibliClassName##Class = { \
        .className = #gibliClassName, \
        .finalize = destroy_func, \
        .copyDebugDesc = describe_func, \
        .equal = compare_func, \
        .hash = hash_func, \
        }; \
        \
        k##gibliClassName##TypeID = _CFRuntimeRegisterClass(&k##gibliClassName##Class); \
    }); \
    return k##gibliClassName##TypeID; \
    }


#define CFGiblisWithHashFor(gibliClassName) \
    static CFStringRef  gibliClassName##CopyDescription(CFTypeRef cf); \
    static void         gibliClassName##Destroy(CFTypeRef cf); \
    static Boolean      gibliClassName##Compare(CFTypeRef lhs, CFTypeRef rhs); \
    static CFHashCode   gibliClassName##Hash(CFTypeRef cf); \
    \
    CFGiblisWithFunctions(gibliClassName, gibliClassName##CopyDescription, gibliClassName##Destroy, gibliClassName##Compare, gibliClassName##Hash)

#define CFGiblisWithCompareFor(gibliClassName) \
    static CFStringRef  gibliClassName##CopyDescription(CFTypeRef cf); \
    static void         gibliClassName##Destroy(CFTypeRef cf); \
    static Boolean      gibliClassName##Compare(CFTypeRef lhs, CFTypeRef rhs); \
    \
    CFGiblisWithFunctions(gibliClassName, gibliClassName##CopyDescription, gibliClassName##Destroy, gibliClassName##Compare, NULL)


#define CFGiblisFor(gibliClassName) \
    static CFStringRef  gibliClassName##CopyDescription(CFTypeRef cf); \
    static void         gibliClassName##Destroy(CFTypeRef cf); \
    \
    CFGiblisWithFunctions(gibliClassName, gibliClassName##CopyDescription, gibliClassName##Destroy, NULL, NULL)

#define CFTypeAllocate(classType, internalType, allocator) \
    (classType##Ref) _CFRuntimeCreateInstance(allocator, classType##GetTypeID(), \
                                              sizeof(internalType) - sizeof(CFRuntimeBase), \
                                              NULL)

__BEGIN_DECLS

//
// Call block function
//

static void apply_block_1(const void *value, void *context)
{
    return ((void (^)(const void *value))context)(value);
}

static void apply_block_2(const void *key, const void *value, void *context)
{
    return ((void (^)(const void *key, const void *value))context)(key, value);
}

//
// CFEqual Helpers
//

static inline bool CFEqualSafe(CFTypeRef left, CFTypeRef right)
{
    if (left == NULL || right == NULL)
        return left == right;
    else
        return CFEqual(left, right);
}

//
// CFNumber Helpers
//

static inline CFNumberRef CFNumberCreateWithCFIndex(CFAllocatorRef allocator, CFIndex value)
{
    return CFNumberCreate(allocator, kCFNumberCFIndexType, &value);
}

//
// CFData Helpers
//

static inline CFMutableDataRef CFDataCreateMutableWithScratch(CFAllocatorRef allocator, CFIndex size) {
    CFMutableDataRef result = CFDataCreateMutable(allocator, 0);
    CFDataSetLength(result, size);

    return result;
}

static inline void CFDataAppend(CFMutableDataRef appendTo, CFDataRef dataToAppend)
{
    CFDataAppendBytes(appendTo, CFDataGetBytePtr(dataToAppend), CFDataGetLength(dataToAppend));
}

static inline CFDataRef CFDataCreateReferenceFromRange(CFAllocatorRef allocator, CFDataRef sourceData, CFRange range)
{
    return CFDataCreateWithBytesNoCopy(allocator,
                                       CFDataGetBytePtr(sourceData) + range.location, range.length,
                                       kCFAllocatorNull);
}

static inline CFDataRef CFDataCreateCopyFromRange(CFAllocatorRef allocator, CFDataRef sourceData, CFRange range)
{
    return CFDataCreate(allocator, CFDataGetBytePtr(sourceData) + range.location, range.length);
}

static inline uint8_t* CFDataIncreaseLengthAndGetMutableBytes(CFMutableDataRef data, CFIndex extraLength)
{
    CFIndex startOffset = CFDataGetLength(data);

    CFDataIncreaseLength(data, extraLength);

    return CFDataGetMutableBytePtr(data) + startOffset;
}

static inline uint8_t* CFDataGetMutablePastEndPtr(CFMutableDataRef theData)
{
    return CFDataGetMutableBytePtr(theData) + CFDataGetLength(theData);
}

static inline const uint8_t* CFDataGetPastEndPtr(CFDataRef theData) {
    return CFDataGetBytePtr(theData) + CFDataGetLength(theData);
}

static inline CFComparisonResult CFDataCompare(CFDataRef left, CFDataRef right)
{
    const size_t left_size = CFDataGetLength(left);
    const size_t right_size = CFDataGetLength(right);
    const size_t shortest = (left_size <= right_size) ? left_size : right_size;
    
    int comparison = memcmp(CFDataGetBytePtr(left), CFDataGetBytePtr(right), shortest);

    if (comparison > 0 || (comparison == 0 && left_size > right_size))
        return kCFCompareGreaterThan;
    else if (comparison < 0 || (comparison == 0 && left_size < right_size))
        return kCFCompareLessThan;
    else
        return kCFCompareEqualTo;
}


//
// CFString Helpers
//

//
// Turn a CFString into an allocated UTF8-encoded C string.
//
static inline char *CFStringToCString(CFStringRef inStr)
{
    if (!inStr)
        return (char *)strdup("");
    CFRetain(inStr);        // compensate for release on exit

    // need to extract into buffer
    CFIndex length = CFStringGetLength(inStr);  // in 16-bit character units
    size_t len = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);
    char *buffer = (char *)malloc(len);                 // pessimistic
    if (!CFStringGetCString(inStr, buffer, len, kCFStringEncodingUTF8))
        buffer[0] = 0;

    CFRelease(inStr);
    return buffer;
}

// runs operation with inStr as a zero terminated C string
// in utf8 encoding passed to the operation block.
void CFStringPerformWithCString(CFStringRef inStr, void(^operation)(const char *utf8Str));

// runs operation with inStr as a zero terminated C string
// in utf8 passed to the operation block, the length of
// the string is also provided to the block.
void CFStringPerformWithCStringAndLength(CFStringRef inStr, void(^operation)(const char *utf8Str, size_t utf8Length));

#include <CommonNumerics/CommonCRC.h>

static inline void CFStringAppendEncryptedData(CFMutableStringRef s, CFDataRef edata)
{
    const uint8_t *bytes = CFDataGetBytePtr(edata);
    CFIndex len = CFDataGetLength(edata);
    CFStringAppendFormat(s, 0, CFSTR("%04lx:"), len);
    if(len<=8) {
        for (CFIndex ix = 0; ix < len; ++ix) {
            CFStringAppendFormat(s, 0, CFSTR("%02X"), bytes[ix]);
        }
    } else {
        uint64_t crc = 0;
        CNCRC(kCN_CRC_64_ECMA_182, bytes+8, len-8, &crc);
        for (CFIndex ix = 0; ix < 8; ++ix) {
            CFStringAppendFormat(s, 0, CFSTR("%02X"), bytes[ix]);
        }
        CFStringAppendFormat(s, 0, CFSTR("...|%08llx"), crc);
    }
}

static inline void CFStringAppendHexData(CFMutableStringRef s, CFDataRef data) {
    const uint8_t *bytes = CFDataGetBytePtr(data);
    CFIndex len = CFDataGetLength(data);
    for (CFIndex ix = 0; ix < len; ++ix) {
        CFStringAppendFormat(s, 0, CFSTR("%02X"), bytes[ix]);
    }
}

static inline CFStringRef CFDataCopyHexString(CFDataRef data) {
    CFMutableStringRef hexString = CFStringCreateMutable(kCFAllocatorDefault, 2 * CFDataGetLength(data));
    CFStringAppendHexData(hexString, data);
    return hexString;
}

static inline void CFDataPerformWithHexString(CFDataRef data, void (^operation)(CFStringRef dataString)) {
    CFStringRef hexString = CFDataCopyHexString(data);
    operation(hexString);
    CFRelease(hexString);
}

static inline void BufferPerformWithHexString(const UInt8 *bytes, CFIndex length, void (^operation)(CFStringRef dataString)) {
    CFDataRef bufferAsData = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, bytes, length, kCFAllocatorNull);
    
    CFDataPerformWithHexString(bufferAsData, operation);
    
    CFReleaseNull(bufferAsData);
}



static inline void CFStringWriteToFile(CFStringRef inStr, FILE* file)
{
    CFStringPerformWithCStringAndLength(inStr, ^(const char *utf8Str, size_t utf8Length) {
        fwrite(utf8Str, 1, utf8Length, file);
    });
}

static inline void CFStringWriteToFileWithNewline(CFStringRef inStr, FILE* file)
{
    CFStringWriteToFile(inStr, file);
    fputc('\n', file);
}

//
// MARK: CFArray Helpers
//

static inline CFIndex CFArrayRemoveAllValue(CFMutableArrayRef array, const void* value)
{
    CFIndex position = kCFNotFound;
    CFIndex numberRemoved = 0;
    
    position = CFArrayGetFirstIndexOfValue(array, CFRangeMake(0, CFArrayGetCount(array)), value);
    while (position != kCFNotFound) {
        CFArrayRemoveValueAtIndex(array, position);
        ++numberRemoved;
        position = CFArrayGetFirstIndexOfValue(array, CFRangeMake(0, CFArrayGetCount(array)), value);
    }
    
    return numberRemoved;
}

static inline void CFArrayForEach(CFArrayRef array, void (^operation)(const void *value)) {
    CFArrayApplyFunction(array, CFRangeMake(0, CFArrayGetCount(array)), apply_block_1, operation);
}

static inline void CFArrayForEachReverse(CFArrayRef array, void (^operation)(const void *value)) {
    for(CFIndex count = CFArrayGetCount(array); count > 0; --count) {
        operation(CFArrayGetValueAtIndex(array, count - 1));
    }
}

static inline const void *CFArrayGetValueMatching(CFArrayRef array, bool (^match)(const void *value)) {
    CFIndex i, n = CFArrayGetCount(array);
    for (i = 0; i < n; ++i) {
        const void *value = CFArrayGetValueAtIndex(array, i);
        if (match(value)) {
            return value;
        }
    }
    return NULL;
}

static inline bool CFArrayHasValueMatching(CFArrayRef array, bool (^match)(const void *value)) {
    return CFArrayGetValueMatching(array, match) != NULL;
}

static inline void CFMutableArrayModifyValues(CFMutableArrayRef array, const void * (^process)(const void *value)) {
    CFIndex i, n = CFArrayGetCount(array);
    for (i = 0; i < n; ++i) {
        const void *value = CFArrayGetValueAtIndex(array, i);
        CFArraySetValueAtIndex(array, i, process(value));
    }
}

//
// MARK: CFArray creatino Var args helper functions.
//
static inline CFArrayRef CFArrayCreateCountedForVC(CFAllocatorRef allocator, const CFArrayCallBacks *cbs, CFIndex entries, va_list args)
{
    const void *values[entries];
    
    for(CFIndex currentValue = 0; currentValue < entries; ++currentValue)
    {
        values[currentValue] = va_arg(args, void*);
        
        if (values[currentValue] == NULL)
            values[currentValue] = kCFNull;
    }
    
    return CFArrayCreate(allocator, values, entries, cbs);
}

static inline CFArrayRef CFArrayCreateForVC(CFAllocatorRef allocator, const CFArrayCallBacks *cbs, va_list args)
{
    va_list count;
    va_copy(count, args);

    CFIndex entries = 0;
    while (NULL != va_arg(count, void*)) {
        entries += 1;
    }

    return CFArrayCreateCountedForVC(allocator, cbs, entries, args);
    
}

//
// MARK: CFArray of CFTypes support
//

static inline CFMutableArrayRef CFArrayCreateMutableForCFTypes(CFAllocatorRef allocator)
{
    return CFArrayCreateMutable(allocator, 0, &kCFTypeArrayCallBacks);
}

static inline CFArrayRef CFArrayCreateForCFTypes(CFAllocatorRef allocator, ...)
{
    va_list args;
    va_start(args, allocator);
    
    return CFArrayCreateForVC(allocator, &kCFTypeArrayCallBacks, args);
    
}

static inline CFArrayRef CFArrayCreateCountedForCFTypes(CFAllocatorRef allocator, CFIndex entries, ...)
{
    va_list args;
    va_start(args, entries);
    
    return CFArrayCreateCountedForVC(allocator, &kCFTypeArrayCallBacks, entries, args);
}

static inline CFArrayRef CFArrayCreateCountedForCFTypesV(CFAllocatorRef allocator, CFIndex entries, va_list args)
{
    return CFArrayCreateCountedForVC(allocator, &kCFTypeArrayCallBacks, entries, args);
}

//
// MARK: CFDictionary of CFTypes helpers
//

static inline CFDictionaryRef CFDictionaryCreateCountedForCFTypesV(CFAllocatorRef allocator, CFIndex entries, va_list args)
{
    const void *keys[entries];
    const void *values[entries];
    
    for(CFIndex currentValue = 0; currentValue < entries; ++currentValue)
    {
        keys[currentValue] = va_arg(args, void*);
        values[currentValue] = va_arg(args, void*);
        
        if (values[currentValue] == NULL)
            values[currentValue] = kCFNull;
    }
    
    return CFDictionaryCreate(allocator, keys, values, entries,
                              &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
}

static inline CFDictionaryRef CFDictionaryCreateForCFTypes(CFAllocatorRef allocator, ...)
{
    va_list args;
    va_start(args, allocator);

    CFIndex entries = 0;
    while (NULL != va_arg(args, void*)) {
        entries += 2;
        (void) va_arg(args, void*);
    }

    entries /= 2;

    va_start(args, allocator);

    return CFDictionaryCreateCountedForCFTypesV(allocator, entries, args);

}

static inline CFDictionaryRef CFDictionaryCreateCountedForCFTypes(CFAllocatorRef allocator, CFIndex entries, ...)
{
    va_list args;
    va_start(args, entries);

    return CFDictionaryCreateCountedForCFTypesV(allocator, entries, args);
}

static inline CFMutableDictionaryRef CFDictionaryCreateMutableForCFTypes(CFAllocatorRef allocator)
{
    return CFDictionaryCreateMutable(allocator, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
}

static inline CFMutableDictionaryRef CFDictionaryCreateMutableForCFTypesWith(CFAllocatorRef allocator, ...)
{
    CFMutableDictionaryRef result = CFDictionaryCreateMutableForCFTypes(allocator);

    va_list args;
    va_start(args, allocator);

    void* key = va_arg(args, void*);

    while (key != NULL) {
        CFDictionarySetValue(result, key, va_arg(args, void*));
        key = va_arg(args, void*);
    };

    return result;
}

//
// MARK: CFDictionary Helpers
//

static inline void CFDictionaryForEach(CFDictionaryRef dictionary, void (^operation)(const void *key, const void *value)) {
    CFDictionaryApplyFunction(dictionary, apply_block_2, operation);
}

//
// Type checking
//

static inline bool isArray(CFTypeRef cfType) {
    return cfType && CFGetTypeID(cfType) == CFArrayGetTypeID();
}

static inline bool isData(CFTypeRef cfType) {
    return cfType && CFGetTypeID(cfType) == CFDataGetTypeID();
}

static inline bool isDate(CFTypeRef cfType) {
    return cfType && CFGetTypeID(cfType) == CFDateGetTypeID();
}

static inline bool isDictionary(CFTypeRef cfType) {
    return cfType && CFGetTypeID(cfType) == CFDictionaryGetTypeID();
}

static inline bool isNumber(CFTypeRef cfType) {
    return cfType && CFGetTypeID(cfType) == CFNumberGetTypeID();
}

static inline bool isNumberOfType(CFTypeRef cfType, CFNumberType number) {
    return isNumber(cfType) && CFNumberGetType((CFNumberRef)cfType) == number;
}

static inline bool isString(CFTypeRef cfType) {
    return cfType && CFGetTypeID(cfType) == CFStringGetTypeID();
}

static inline bool isBoolean(CFTypeRef cfType) {
    return cfType && CFGetTypeID(cfType) == CFBooleanGetTypeID();
}

static inline bool isNull(CFTypeRef cfType) {
    return cfType && CFGetTypeID(cfType) == CFNullGetTypeID();
}


//
// MARK: PropertyList Helpers
//

//
// Crazy reading and writing stuff
//

static inline void CFPropertyListWriteToFile(CFPropertyListRef plist, CFURLRef file)
{
    CFWriteStreamRef writeStream = CFWriteStreamCreateWithFile(kCFAllocatorDefault, file);
    CFErrorRef error = NULL;
    
    CFWriteStreamOpen(writeStream);
    CFPropertyListWrite(plist, writeStream, kCFPropertyListBinaryFormat_v1_0, 0, &error);
    if (error)
        secerror("Can't write plist: %@", error);
    
    CFReleaseNull(error);
    CFReleaseNull(writeStream);
}

static inline CF_RETURNS_RETAINED CFPropertyListRef CFPropertyListReadFromFile(CFURLRef file)
{
    CFPropertyListRef result = NULL;
    CFErrorRef error = NULL;
    CFBooleanRef isRegularFile;
    if (!CFURLCopyResourcePropertyForKey(file, kCFURLIsRegularFileKey, &isRegularFile, &error)) {
        secerror("file %@: %@", file, error);
    } else if (CFBooleanGetValue(isRegularFile)) {
        CFReadStreamRef readStream = CFReadStreamCreateWithFile(kCFAllocatorDefault, file);
        if (readStream) {
            if (CFReadStreamOpen(readStream)) {
                CFPropertyListFormat format;
                result = CFPropertyListCreateWithStream(kCFAllocatorDefault, readStream, 0, kCFPropertyListMutableContainers, &format, &error);
                if (!result) {
                    secerror("read plist from %@: %@", file, error);
                }
            }
            CFRelease(readStream);
        }
    }
    CFReleaseNull(error);
    
    return result;
}

__END_DECLS

#endif
