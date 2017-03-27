/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecInternal.h>
#include <Security/SecItem.h>
#include <Security/SecBase.h>
#include <CommonCrypto/CommonHMAC.h>
#include <stdlib.h>
#include <unistd.h>
#include <Security/SecPBKDF.h>

#include "Security_regressions.h"

static int kTestTestCount = 8;

static void tests(void)
{
    {
        const char *password =          "password";
        const char *salt =              "salt";
        const int iterations =          1;
        const uint8_t expected[32] =  {
            0x12, 0x0f, 0xb6, 0xcf, 0xfc, 0xf8, 0xb3, 0x2c,
            0x43, 0xe7, 0x22, 0x52, 0x56, 0xc4, 0xf8, 0x37,
            0xa8, 0x65, 0x48, 0xc9, 0x2c, 0xcc, 0x35, 0x48,
            0x08, 0x05, 0x98, 0x7c, 0xb7, 0x0b, 0xe1, 0x7b
        };

        const char resultSize = sizeof(expected);

        uint8_t actual[resultSize];

        pbkdf2_hmac_sha256((const uint8_t*) password, strlen(password), (const uint8_t*) salt, strlen(salt), iterations, actual, resultSize);

        ok(memcmp(expected, actual, resultSize) == 0, "pbkdf-sha-256: P-'password' S-'salt' I-1");
    }

    {
        const char *password =          "password";
        const char *salt =              "salt";
        const int iterations =          2;
        const uint8_t expected[32] =  {
            0xae, 0x4d, 0x0c, 0x95, 0xaf, 0x6b, 0x46, 0xd3,
            0x2d, 0x0a, 0xdf, 0xf9, 0x28, 0xf0, 0x6d, 0xd0,
            0x2a, 0x30, 0x3f, 0x8e, 0xf3, 0xc2, 0x51, 0xdf,
            0xd6, 0xe2, 0xd8, 0x5a, 0x95, 0x47, 0x4c, 0x43
        };

        const char resultSize = sizeof(expected);

        uint8_t actual[resultSize];

        pbkdf2_hmac_sha256((const uint8_t*) password, strlen(password), (const uint8_t*) salt, strlen(salt), iterations, actual, resultSize);

        ok(memcmp(expected, actual, resultSize) == 0, "pbkdf-sha-256: P-'password' S-'salt' I-2");
    }

    {
        const char *password =          "password";
        const char *salt =              "salt";
        const int iterations =          4096;
        const uint8_t expected[32] =  {
            0xc5, 0xe4, 0x78, 0xd5, 0x92, 0x88, 0xc8, 0x41,
            0xaa, 0x53, 0x0d, 0xb6, 0x84, 0x5c, 0x4c, 0x8d,
            0x96, 0x28, 0x93, 0xa0, 0x01, 0xce, 0x4e, 0x11,
            0xa4, 0x96, 0x38, 0x73, 0xaa, 0x98, 0x13, 0x4a
        };

        const char resultSize = sizeof(expected);

        uint8_t actual[resultSize];

        pbkdf2_hmac_sha256((const uint8_t*) password, strlen(password), (const uint8_t*) salt, strlen(salt), iterations, actual, resultSize);

        ok(memcmp(expected, actual, resultSize) == 0, "pbkdf-sha-256: P-'password' S-'salt' I-4096");
    }

SKIP: {
    skip("16777216 iterations is too slow", 1, 0);

    const char *password =          "password";
    const char *salt =              "salt";
    const int iterations =          16777216;
    const uint8_t expected[32] =  {
        0xcf, 0x81, 0xc6, 0x6f, 0xe8, 0xcf, 0xc0, 0x4d,
        0x1f, 0x31, 0xec, 0xb6, 0x5d, 0xab, 0x40, 0x89,
        0xf7, 0xf1, 0x79, 0xe8, 0x9b, 0x3b, 0x0b, 0xcb,
        0x17, 0xad, 0x10, 0xe3, 0xac, 0x6e, 0xba, 0x46
    };

    const char resultSize = sizeof(expected);

    uint8_t actual[resultSize];

    pbkdf2_hmac_sha256((const uint8_t*) password, strlen(password), (const uint8_t*) salt, strlen(salt), iterations, actual, resultSize);

    ok(memcmp(expected, actual, resultSize) == 0, "pbkdf-sha-256: P-'password' S-'salt' I-16777216");
}


    {
        CFStringRef password    = CFStringCreateWithCString(NULL, "password", kCFStringEncodingUTF8);
        CFStringRef salt        = CFStringCreateWithCString(NULL, "salt", kCFStringEncodingUTF8);

        CFDataRef   passwordData    = CFStringCreateExternalRepresentation(NULL, password, kCFStringEncodingUTF8, 0);
        CFDataRef   saltData        = CFStringCreateExternalRepresentation(NULL, salt, kCFStringEncodingUTF8, 0);

        const int iterations =          1;
        const uint8_t expected[32] =  {
            0x12, 0x0f, 0xb6, 0xcf, 0xfc, 0xf8, 0xb3, 0x2c,
            0x43, 0xe7, 0x22, 0x52, 0x56, 0xc4, 0xf8, 0x37,
            0xa8, 0x65, 0x48, 0xc9, 0x2c, 0xcc, 0x35, 0x48,
            0x08, 0x05, 0x98, 0x7c, 0xb7, 0x0b, 0xe1, 0x7b
        };

        const char resultSize = sizeof(expected);

        CFMutableDataRef resultData = CFDataCreateMutable(NULL, resultSize);
        CFDataIncreaseLength(resultData, resultSize);

        SecKeyFromPassphraseDataHMACSHA256(passwordData, saltData, iterations, resultData);

        ok(memcmp(expected, CFDataGetBytePtr(resultData), resultSize) == 0, "pbkdf-sha-256: P-'password' S-'salt' I-1");

        CFReleaseSafe(password);
        CFReleaseSafe(salt);
        CFReleaseSafe(passwordData);
        CFReleaseSafe(saltData);
        CFReleaseSafe(resultData);
    }

    {
        CFStringRef password    = CFStringCreateWithCString(NULL, "password", kCFStringEncodingUTF8);
        CFStringRef salt        = CFStringCreateWithCString(NULL, "salt", kCFStringEncodingUTF8);

        CFDataRef   passwordData    = CFStringCreateExternalRepresentation(NULL, password, kCFStringEncodingUTF8, 0);
        CFDataRef   saltData        = CFStringCreateExternalRepresentation(NULL, salt, kCFStringEncodingUTF8, 0);

        const int iterations =          2;
        const uint8_t expected[32] =  {
            0xae, 0x4d, 0x0c, 0x95, 0xaf, 0x6b, 0x46, 0xd3,
            0x2d, 0x0a, 0xdf, 0xf9, 0x28, 0xf0, 0x6d, 0xd0,
            0x2a, 0x30, 0x3f, 0x8e, 0xf3, 0xc2, 0x51, 0xdf,
            0xd6, 0xe2, 0xd8, 0x5a, 0x95, 0x47, 0x4c, 0x43
        };

        const char resultSize = sizeof(expected);

        CFMutableDataRef resultData = CFDataCreateMutable(NULL, resultSize);
        CFDataIncreaseLength(resultData, resultSize);

        SecKeyFromPassphraseDataHMACSHA256(passwordData, saltData, iterations, resultData);

        ok(memcmp(expected, CFDataGetBytePtr(resultData), resultSize) == 0, "pbkdf-sha-256: P-'password' S-'salt' I-2");

        CFReleaseSafe(password);
        CFReleaseSafe(salt);
        CFReleaseSafe(passwordData);
        CFReleaseSafe(saltData);
        CFReleaseSafe(resultData);
    }

    {
        CFStringRef password    = CFStringCreateWithCString(NULL, "password", kCFStringEncodingUTF8);
        CFStringRef salt        = CFStringCreateWithCString(NULL, "salt", kCFStringEncodingUTF8);

        CFDataRef   passwordData    = CFStringCreateExternalRepresentation(NULL, password, kCFStringEncodingUTF8, 0);
        CFDataRef   saltData        = CFStringCreateExternalRepresentation(NULL, salt, kCFStringEncodingUTF8, 0);

        const int iterations =          4096;
        const uint8_t expected[32] =  {
            0xc5, 0xe4, 0x78, 0xd5, 0x92, 0x88, 0xc8, 0x41,
            0xaa, 0x53, 0x0d, 0xb6, 0x84, 0x5c, 0x4c, 0x8d,
            0x96, 0x28, 0x93, 0xa0, 0x01, 0xce, 0x4e, 0x11,
            0xa4, 0x96, 0x38, 0x73, 0xaa, 0x98, 0x13, 0x4a
        };

        const char resultSize = sizeof(expected);

        CFMutableDataRef resultData = CFDataCreateMutable(NULL, resultSize);
        CFDataIncreaseLength(resultData, resultSize);

        SecKeyFromPassphraseDataHMACSHA256(passwordData, saltData, iterations, resultData);

        ok(memcmp(expected, CFDataGetBytePtr(resultData), resultSize) == 0, "pbkdf-sha-256: P-'password' S-'salt' I-4096");

        CFReleaseSafe(password);
        CFReleaseSafe(salt);
        CFReleaseSafe(passwordData);
        CFReleaseSafe(saltData);
        CFReleaseSafe(resultData);
    }

SKIP: {
    skip("16777216 iterations is too slow", 1, 0);

    CFStringRef password    = CFStringCreateWithCString(NULL, "password", kCFStringEncodingUTF8);
    CFStringRef salt        = CFStringCreateWithCString(NULL, "salt", kCFStringEncodingUTF8);

    CFDataRef   passwordData    = CFStringCreateExternalRepresentation(NULL, password, kCFStringEncodingUTF8, 0);
    CFDataRef   saltData        = CFStringCreateExternalRepresentation(NULL, salt, kCFStringEncodingUTF8, 0);

    const int iterations =          16777216;
    const uint8_t expected[32] =  {
        0xcf, 0x81, 0xc6, 0x6f, 0xe8, 0xcf, 0xc0, 0x4d,
        0x1f, 0x31, 0xec, 0xb6, 0x5d, 0xab, 0x40, 0x89,
        0xf7, 0xf1, 0x79, 0xe8, 0x9b, 0x3b, 0x0b, 0xcb,
        0x17, 0xad, 0x10, 0xe3, 0xac, 0x6e, 0xba, 0x46
    };


    const char resultSize = sizeof(expected);

    CFMutableDataRef resultData = CFDataCreateMutable(NULL, resultSize);
    CFDataIncreaseLength(resultData, resultSize);

    SecKeyFromPassphraseDataHMACSHA256(passwordData, saltData, iterations, resultData);

    ok(memcmp(expected, CFDataGetBytePtr(resultData), resultSize) == 0, "pbkdf-sha-256: P-'password' S-'salt' I-16777216");

    CFReleaseSafe(password);
    CFReleaseSafe(salt);
    CFReleaseSafe(passwordData);
    CFReleaseSafe(saltData);
    CFReleaseSafe(resultData);
}

}

int spbkdf_01_hmac_sha256(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);

    tests();

    return 0;
}
