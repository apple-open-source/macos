/*
 * Copyright (c) 2025 Apple Inc. All Rights Reserved.
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


//
// derivedentropy - securityd derived entropy layer
//

#include "derivedentropy.h"

#include "c++utils.h"
#include "SecRandom.h"
#include "utilities/SecCFWrappers.h"

#include <CommonCrypto/CommonHMAC.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFString.h>
#include <dispatch/dispatch.h>
#include <security_utilities/debugging.h>
#include <time.h>

static CFStringRef CF_RETURNS_RETAINED AppendHexEncodedSaltToString(CFStringRef base, CFDataRef data)
{
    if (!base) {
        return NULL;
    }

    if (!data) {
        return CFStringCreateCopy(NULL, base);
    }

    CFMutableStringRef str = CFStringCreateMutableCopy(NULL, CFStringGetLength(base) + CFDataGetLength(data) * 2, base);
    CFStringAppendHexData(str, data);

    return str;
}

#define kProtectedEntropyDirectoryPath "/var/db/SystemKeys/"

static CFDataRef CopyProtectedEntropyForSalt(const CssmData& salt) {
    CFDataRef saltData = CFDataCreate(kCFAllocatorDefault, (const UInt8*)salt.data(), salt.length());
    CFTypeRefHolder __saltData(saltData);

    CFStringRef path = AppendHexEncodedSaltToString(CFSTR(kProtectedEntropyDirectoryPath), saltData);
    CFTypeRefHolder __path(path);
    secnotice("dp_login", "path: %@", path);

    CFURLRef url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, path, kCFURLPOSIXPathStyle, false);
    CFTypeRefHolder __url(url);
    secnotice("dp_login", "url: %@", url);

    CFReadStreamRef rs = CFReadStreamCreateWithFile(NULL, url);
    CFTypeRefHolder __rs(rs);

    Boolean opened = CFReadStreamOpen(rs);
    if (opened) {
        secnotice("dp_login", "found file at %@", path);
        CFErrorRef error = NULL;
        CFPropertyListRef plist = CFPropertyListCreateWithStream(NULL, rs, 0, kCFPropertyListImmutable, NULL, &error);
        CFTypeRefHolder __error(error);
        if (!plist) {
            secnotice("dp_login", "plist is null %@ %@", path, error);
            return NULL;
        }
        if (CFGetTypeID(plist) != CFDataGetTypeID()) {
            secnotice("dp_login", "type of plist is not data (%lu) %@ ", CFGetTypeID(plist), path);
            CFRelease(plist);
            return NULL;
        }
        secnotice("dp_login", "returning data from %@", path);
        return (CFDataRef)plist;
    }

    // else file didn't exist, create a new one
    secnotice("dp_login", "need to create new file");
    int ret = mkdir(kProtectedEntropyDirectoryPath, 0700);
    if (ret != 0 && errno != EEXIST) {
        secnotice("dp_login", "could not mkdir %d", errno);
        return NULL;
    }

    CFWriteStreamRef ws = CFWriteStreamCreateWithFile(NULL, url);
    CFTypeRefHolder __ws(ws);
    opened = CFWriteStreamOpen(ws);
    if (!opened) {
        secnotice("dp_login", "could not open write stream?");
        return NULL;
    }

    unsigned char rawBytes[32];
    ret = SecRandomCopyBytes(kSecRandomDefault, sizeof(rawBytes), rawBytes);
    if (ret != 0) {
        secnotice("dp_login", "could not copy random bytes?");
        return NULL;
    }
    CFDataRef protectedEntropy = CFDataCreate(NULL, rawBytes, sizeof(rawBytes));

    CFErrorRef error = NULL;
    if (CFPropertyListWrite(protectedEntropy, ws, kCFPropertyListXMLFormat_v1_0, 0, &error) == 0)
    {
        secnotice("dp_login", "could not write protected entropy %@ %@", path, error);
        CFRelease(protectedEntropy);
        CFRelease(error);
        return NULL;
    }

    return protectedEntropy;
}

// Even the first attempt might need to wait, if an attacker can get securityd to crash & restart
static uint64_t generateDerivedEntropy_wait_until = clock_gettime_nsec_np(CLOCK_UPTIME_RAW) + 500 * NSEC_PER_MSEC;

KeyHandle generateDerivedEntropy(const CssmData& salt, const CssmData& passphrase) {

    secnotice("dp_login", "generating derived entropy for %s", salt.toHex().c_str());

    uint64_t now = clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
    secnotice("dp_login", "now %llu wait %llu", now, generateDerivedEntropy_wait_until);
    if (now < generateDerivedEntropy_wait_until) {
        uint64_t wait_usec = 1+(generateDerivedEntropy_wait_until - now)/NSEC_PER_USEC;
        secnotice("dp_login", "slow down %llu", wait_usec);
        usleep((useconds_t)wait_usec);
        now = clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
    }
    generateDerivedEntropy_wait_until = now + 500 * NSEC_PER_MSEC;

    CFDataRef protectedEntropy = CopyProtectedEntropyForSalt(salt);
    CFTypeRefHolder _(protectedEntropy); // to CFRelease automatically

    // Same as HKDF-extract: salt is the HMAC key and user-specified (low-entropy) password is the data to be HMAC'd
    CCHmacContext ctxHMAC;
    CCHmacInit(&ctxHMAC, kCCHmacAlgSHA256, CFDataGetBytePtr(protectedEntropy), CFDataGetLength(protectedEntropy));
    CCHmacUpdate(&ctxHMAC, passphrase.Data, passphrase.Length);
    DataBuffer<CC_SHA256_DIGEST_LENGTH> derivedEntropy;
    CCHmacFinal(&ctxHMAC, derivedEntropy.data());

    CssmDataContainer entropyContainer;
    entropyContainer.append(derivedEntropy);
    return storeHandleData(entropyContainer);
}

// Handle storage using CFDictionary with random handles and timeout support
static CFMutableDictionaryRef handleStorage = NULL;
#define HANDLE_STORAGE_EXPIRY_TIME CFSTR("expiryTime")
#define HANDLE_STORAGE_KEY_DATA    CFSTR("keyData")

// Exipry prediate
static bool shouldRemoveEntry(CFDictionaryRef entry) {
    CFNumberRef expiryNumber = (CFNumberRef)CFDictionaryGetValue(entry, HANDLE_STORAGE_EXPIRY_TIME);
    if (!expiryNumber) {
        return true; // remove if not a valid entry
    }

    uint64_t expiryTime;
    if (!CFNumberGetValue(expiryNumber, kCFNumberLongLongType, &expiryTime)) {
        return true; // remove if not a valid entry
    }

    uint64_t now = clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
    return now > expiryTime;
}

// Initialize the handle storage dictionary
static void initializeAndCleanupHandleStorage() {
    static dispatch_once_t handleStorageOnce;
    dispatch_once(&handleStorageOnce, ^{
        handleStorage = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    });

    // Remove expired handles
    CFMutableArrayRef expiredHandles = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

    // Find expired handles
    CFDictionaryApplyFunction(handleStorage, [](const void* key, const void* value, void* context) {
        CFMutableArrayRef expiredArray = (CFMutableArrayRef)context;
        CFDictionaryRef entry = (CFDictionaryRef)value;
        if (shouldRemoveEntry(entry)) {
            CFArrayAppendValue(expiredArray, key);
        }
    }, expiredHandles);

    // Remove expired handles
    CFIndex count = CFArrayGetCount(expiredHandles);
    for (CFIndex i = 0; i < count; i++) {
        CFNumberRef handleNumber = (CFNumberRef)CFArrayGetValueAtIndex(expiredHandles, i);
        CFDictionaryRemoveValue(handleStorage, handleNumber);
    }

    if (count > 0) {
        secnotice("dp_login", "initializeHandleStorage cleaned up %ld expired handles", count);
    }

    CFRelease(expiredHandles);
}

// Generate a cryptographically secure random handle
static KeyHandle generateRandomHandle() {
    uint32_t randomValue = 0;
    while (randomValue == 0) { // Ensure we never return 0 (which might be interpreted as noKey)
        int result = SecRandomCopyBytes(kSecRandomDefault, sizeof(randomValue), (uint8_t*)&randomValue);
        if (result != 0) {
            // Fallback to arc4random if SecRandom fails
            randomValue = arc4random();
        }
    }
    return randomValue;
}

// Store key data with a handle and timeout (default 5 seconds)
KeyHandle storeHandleData(const CssmData& data, uint64_t timeoutSeconds /* = 5 */) {
    initializeAndCleanupHandleStorage();

    KeyHandle handle = noKey;
    CFNumberRef handleNumber = NULL;
    while (handle == noKey) {
        handle = generateRandomHandle();
        handleNumber = CFNumberCreate(NULL, kCFNumberSInt32Type, &handle);
        if (CFDictionaryContainsKey(handleStorage, handleNumber)) {
            handle = noKey;
            CFRelease(handleNumber);
        }
    }

    // Create the entry dictionary
    CFMutableDictionaryRef entry = CFDictionaryCreateMutable(NULL, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    // Store the key data
    CFDataRef keyData = CFDataCreate(NULL, (const UInt8*)data.data(), data.length());
    CFDictionarySetValue(entry, HANDLE_STORAGE_KEY_DATA, keyData);
    CFRelease(keyData);

    // Store the expiry time
    uint64_t expiryTime = clock_gettime_nsec_np(CLOCK_UPTIME_RAW) + timeoutSeconds * NSEC_PER_SEC;
    CFNumberRef expiryNumber = CFNumberCreate(NULL, kCFNumberLongLongType, &expiryTime);
    CFDictionarySetValue(entry, HANDLE_STORAGE_EXPIRY_TIME, expiryNumber);
    CFRelease(expiryNumber);

    // Store the entry in the main dictionary
    CFDictionarySetValue(handleStorage, handleNumber, entry);
    CFRelease(entry);
    CFRelease(handleNumber);

    secnotice("dp_login", "stored handle %u with timeout %llu seconds", handle, timeoutSeconds);
    return handle;
}

// Retrieve key data for a handle
bool retrieveHandleData(KeyHandle handle, CssmDataContainer& data) {
    initializeAndCleanupHandleStorage();

    CFNumberRef handleNumber = CFNumberCreate(NULL, kCFNumberSInt32Type, &handle);
    CFDictionaryRef entry = (CFDictionaryRef)CFDictionaryGetValue(handleStorage, handleNumber);

    if (!entry) {
        CFRelease(handleNumber);
        secnotice("dp_login", "handle %u not found", handle);
        return false;
    }

    // Retrieve the key data
    CFDataRef keyData = (CFDataRef)CFDictionaryGetValue(entry, HANDLE_STORAGE_KEY_DATA);
    if (keyData) {
        data = CssmData(keyData);
        CFRelease(handleNumber);
        secnotice("dp_login", "retrieved handle %u", handle);
        return true;
    }

    CFRelease(handleNumber);
    secnotice("dp_login", "handle %u has no data", handle);
    return false;
}

// Remove a specific handle
void removeHandle(KeyHandle handle) {
    initializeAndCleanupHandleStorage();

    CFNumberRef handleNumber = CFNumberCreate(NULL, kCFNumberSInt32Type, &handle);
    CFDictionaryRemoveValue(handleStorage, handleNumber);
    CFRelease(handleNumber);
    secnotice("dp_login", "removed handle %u", handle);
}
