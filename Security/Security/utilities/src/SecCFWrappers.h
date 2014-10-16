/*
 * Copyright (c) 2011-2014 Apple Inc. All Rights Reserved.
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


#ifndef _SECCFWRAPPERS_H_
#define _SECCFWRAPPERS_H_

#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CoreFoundation.h>

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

// Call this to create a function that returns a singleton instance of type stype,
// which is initialized once by calling doThisOnce, with result in its context.  Upon
// completion body should assign to *result.
#define CFGiblisGetSingleton(returnType, giblisClassName, result, doThisOnce) \
returnType giblisClassName(void); \
returnType giblisClassName(void) { \
    static dispatch_once_t s##giblisClassName##Once; \
    static returnType s##giblisClassName##Singleton; \
    returnType *result = &s##giblisClassName##Singleton; \
    dispatch_once(&s##giblisClassName##Once, doThisOnce); \
    return s##giblisClassName##Singleton; \
}

#define CFGiblisWithFunctions(gibliClassName, init_func, copy_func, finalize_func, equal_func, hash_func, copyFormattingDesc_func, copyDebugDesc_func, reclaim_func, refcount_func, run_once_block) \
CFGiblisGetSingleton(CFTypeID, gibliClassName##GetTypeID, typeID, (^{ \
    void(^_onceBlock)() = (run_once_block); \
    static const CFRuntimeClass s##gibliClassName##Class = { \
        .version = (reclaim_func == NULL ? 0 : _kCFRuntimeResourcefulObject) \
                 | (refcount_func == NULL ? 0 : _kCFRuntimeCustomRefCount), \
        .className = #gibliClassName, \
        .init = init_func, \
        .copy = copy_func, \
        .finalize = finalize_func, \
        .equal = equal_func, \
        .hash = hash_func, \
        .copyFormattingDesc = copyFormattingDesc_func, \
        .copyDebugDesc = copyDebugDesc_func, \
        .reclaim = reclaim_func, \
        .refcount = refcount_func, \
    }; \
    *typeID = _CFRuntimeRegisterClass(&s##gibliClassName##Class); \
    if (_onceBlock) \
        _onceBlock(); \
}))


#define CFGiblisWithHashFor(gibliClassName) \
    static CFStringRef  gibliClassName##CopyDescription(CFTypeRef cf); \
    static void         gibliClassName##Destroy(CFTypeRef cf); \
    static Boolean      gibliClassName##Compare(CFTypeRef lhs, CFTypeRef rhs); \
    static CFHashCode   gibliClassName##Hash(CFTypeRef cf); \
    \
    CFGiblisWithFunctions(gibliClassName, NULL, NULL, gibliClassName##Destroy, gibliClassName##Compare, gibliClassName##Hash, NULL, gibliClassName##CopyDescription, NULL, NULL, NULL)

#define CFGiblisWithCompareFor(gibliClassName) \
    static CFStringRef  gibliClassName##CopyDescription(CFTypeRef cf); \
    static void         gibliClassName##Destroy(CFTypeRef cf); \
    static Boolean      gibliClassName##Compare(CFTypeRef lhs, CFTypeRef rhs); \
    \
    CFGiblisWithFunctions(gibliClassName, NULL, NULL, gibliClassName##Destroy, gibliClassName##Compare, NULL, NULL, gibliClassName##CopyDescription, NULL, NULL, NULL)


#define CFGiblisFor(gibliClassName) \
    static CFStringRef  gibliClassName##CopyDescription(CFTypeRef cf); \
    static void         gibliClassName##Destroy(CFTypeRef cf); \
    \
    CFGiblisWithFunctions(gibliClassName, NULL, NULL, gibliClassName##Destroy, NULL, NULL, NULL, gibliClassName##CopyDescription, NULL, NULL, NULL)

#define CFTypeAllocateWithSpace(classType, space, allocator) \
    (classType##Ref) _CFRuntimeCreateInstance(allocator, classType##GetTypeID(), space, NULL)

#define CFTypeAllocate(classType, internalType, allocator) \
    CFTypeAllocateWithSpace(classType, sizeof(internalType) - sizeof(CFRuntimeBase), allocator)


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
// Printing
//

static void fprint_string(FILE *file, CFStringRef string) {
    UInt8 buf[256];
    CFRange range = { .location = 0 };
    range.length = CFStringGetLength(string);
    while (range.length > 0) {
        CFIndex bytesUsed = 0;
        CFIndex converted = CFStringGetBytes(string, range, kCFStringEncodingUTF8, 0, false, buf, sizeof(buf), &bytesUsed);
        fwrite(buf, 1, bytesUsed, file);
        range.length -= converted;
        range.location += converted;
    }
}

static inline void cffprint_v(FILE *file, CFStringRef fmt, va_list args) {
    CFStringRef line = CFStringCreateWithFormatAndArguments(NULL, NULL, fmt, args);
    fprint_string(file, line);
    CFRelease(line);
}

static inline void cffprint_c_v(FILE *file, const char *fmt, va_list args) {
    CFStringRef cffmt = CFStringCreateWithCString(kCFAllocatorDefault, fmt, kCFStringEncodingUTF8);
    cffprint_v(file, cffmt, args);
    CFRelease(cffmt);
}

static void cffprint(FILE *file, CFStringRef fmt, ...) CF_FORMAT_FUNCTION(2,0);
static inline void cffprint(FILE *file, CFStringRef fmt, ...) {
    va_list args;
    va_start(args, fmt);
    cffprint_v(file, fmt, args);
    va_end(args);
}

//
// CFError Helpers
//

/* Return false if possibleError is set.  Propagates possibleError into *error
   if *error is NULL, otherwise releases possibleError. */
static inline
bool CFErrorPropagate(CFErrorRef possibleError CF_CONSUMED, CFErrorRef *error) {
    if (possibleError) {
        if (error && !*error) {
            *error = possibleError;
        } else {
            CFRelease(possibleError);
        }
        return false;
    }
    return true;
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

static inline CF_RETURNS_RETAINED CFStringRef CFDataCopyHexString(CFDataRef data) {
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
// MARK: CFCollectionHelpers
//

static inline
const void *SecCFRetainForCollection(CFAllocatorRef allocator, const void *value)
{
    return CFRetain(value);
}

static inline
void SecCFReleaseForCollection(CFAllocatorRef allocator, const void *value)
{
    CFRelease(value);
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

#define CFArrayForEachC(array, value) for (CFIndex _aCount = CFArrayGetCount(array), _aIX = 0;value = (__typeof__(value))(_aIX < _aCount ? CFArrayGetValueAtIndex(array, _aIX) : 0), _aIX < _aCount; ++_aIX)

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
    const void *values[entries ? entries : 1];
    for (CFIndex currentValue = 0; currentValue < entries; ++currentValue)
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

static inline CFMutableArrayRef CFArrayCreateMutableForCFTypesWithCapacity(CFAllocatorRef allocator, CFIndex capacity)
{
    return CFArrayCreateMutable(allocator, capacity, &kCFTypeArrayCallBacks);
}


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
// MARK: CFSet Helpers
//

static inline CFMutableSetRef CFSetCreateMutableForCFTypes(CFAllocatorRef allocator)
{
    return CFSetCreateMutable(kCFAllocatorDefault, 0, &kCFTypeSetCallBacks);
}

static inline void CFSetForEach(CFSetRef set, void (^operation)(const void *value)) {
    CFSetApplyFunction(set, apply_block_1, operation);
}

static inline void CFSetSetValues(CFMutableSetRef set, CFArrayRef valuesToSet) {
    CFArrayForEach(valuesToSet, ^(const void *value) {
        CFSetSetValue(set, value);
    });
}

static inline CFMutableArrayRef CFSetCopyValues(CFSetRef set) {
    CFMutableArrayRef values = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    
    CFSetForEach(set, ^(const void *value) {
        CFArrayAppendValue(values, value);
    });
    
    return values;
}

//
// MARK: CFDictionary Helpers
//

static inline void CFDictionaryForEach(CFDictionaryRef dictionary, void (^operation)(const void *key, const void *value)) {
    CFDictionaryApplyFunction(dictionary, apply_block_2, operation);
}

//
// MARK: CFCalendar helpers
//

CFCalendarRef SecCFCalendarGetZulu();

//
// MARK: CFAbsoluteTime helpers
//

static inline CFAbsoluteTime CFAbsoluteTimeForCalendarMoment(CFCalendarRef cal, int year, int month, int day, int hour, int minute, int second) {
    CFAbsoluteTime at;
    CFCalendarComposeAbsoluteTime(cal, &at, "yMdHms", year, month, day, hour, minute, second);
    return at;
}

static inline CFAbsoluteTime CFAbsoluteTimeForCalendarDay(CFCalendarRef cal, int year, int month, int day) {
    CFAbsoluteTime at;
    CFCalendarComposeAbsoluteTime(cal, &at, "yMd", year, month, day);
    return at;
}

static inline CFAbsoluteTime CFAbsoluteTimeForGregorianMoment(CFTimeZoneRef tz, int year, int month, int day, int hour, int minute, int second)
{
    CFCalendarRef cal = CFCalendarCreateWithIdentifier(NULL, kCFGregorianCalendar);
    CFCalendarSetTimeZone(cal, tz);
    CFAbsoluteTime at = CFAbsoluteTimeForCalendarMoment(cal, year, month, day, hour, minute, second);
    CFReleaseSafe(cal);
    return at;
}

static inline CFAbsoluteTime CFAbsoluteTimeForGregorianDay(CFTimeZoneRef tz, int year, int month, int day)
{
    CFCalendarRef cal = CFCalendarCreateWithIdentifier(NULL, kCFGregorianCalendar);
    CFCalendarSetTimeZone(cal, tz);
    CFAbsoluteTime at = CFAbsoluteTimeForCalendarDay(cal, year, month, day);
    CFReleaseSafe(cal);
    return at;
}

static inline CFAbsoluteTime CFAbsoluteTimeForGregorianZuluMoment(int year, int month, int day, int hour, int minute, int second)
{
    return CFAbsoluteTimeForCalendarMoment(SecCFCalendarGetZulu(), year, month, day, hour, minute, second);
}


static inline CFAbsoluteTime CFAbsoluteTimeForGregorianZuluDay(int year, int month, int day)
{
    return CFAbsoluteTimeForCalendarDay(SecCFCalendarGetZulu(), year, month, day);
}



//
// MARK: CFDate Helpers
//

static inline CFDateRef CFDateCreateForGregorianMoment(CFAllocatorRef allocator, CFTimeZoneRef tz, int year, int month, int day, int hour, int minute, int second)
{
    return CFDateCreate(allocator, CFAbsoluteTimeForGregorianMoment(tz, year, month, day, hour, minute, second));
}

static inline CFDateRef CFDateCreateForGregorianDay(CFAllocatorRef allocator, CFTimeZoneRef tz, int year, int month, int day, int hour, int minute, int second)
{
    return CFDateCreate(allocator, CFAbsoluteTimeForGregorianDay(tz, year, month, day));
}

static inline CFDateRef CFDateCreateForGregorianZuluMoment(CFAllocatorRef allocator, int year, int month, int day, int hour, int minute, int second)
{
    return CFDateCreate(allocator, CFAbsoluteTimeForGregorianZuluMoment(year, month, day, hour, minute, second));
}

static inline CFDateRef CFDateCreateForGregorianZuluDay(CFAllocatorRef allocator, int year, int month, int day)
{
    return CFDateCreate(allocator, CFAbsoluteTimeForGregorianZuluDay(year, month, day));
}


//
// MARK: Type checking
//

static inline bool isArray(CFTypeRef cfType) {
    return cfType && CFGetTypeID(cfType) == CFArrayGetTypeID();
}

static inline bool isSet(CFTypeRef cfType) {
    return cfType && CFGetTypeID(cfType) == CFSetGetTypeID();
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
        secdebug("plist", "file %@: %@", file, error);
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

#endif /* _SECCFWRAPPERS_H_ */
