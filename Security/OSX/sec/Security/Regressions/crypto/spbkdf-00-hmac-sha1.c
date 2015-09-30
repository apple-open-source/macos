/*
 * Copyright (c) 2010,2012 Apple Inc. All Rights Reserved.
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

#if 0
static void
printComparison(const uint8_t*left, const uint8_t* right, int length)
{
    int i;
    for(i = 0; i < length; ++i)
    {
        fprintf(stderr, "#  Values :: 0x%02x :: 0x%02x\n", left[i], right[i]);
    }
}
#endif

static int kTestTestCount = 8;

static void tests(void)
{
    {
        const char *password =          "password";
        const char *salt =              "salt";
        const int iterations =          1;
        const uint8_t expected[20] =  { 0x0c, 0x60, 0xc8, 0x0f,
                                        0x96, 0x1f, 0x0e, 0x71,
                                        0xf3, 0xa9, 0xb5, 0x24,
                                        0xaf, 0x60, 0x12, 0x06,
                                        0x2f, 0xe0, 0x37, 0xa6 };

        const char resultSize = sizeof(expected);

        uint8_t actual[resultSize];

        pbkdf2_hmac_sha1((const uint8_t*) password, strlen(password), (const uint8_t*) salt, strlen(salt), iterations, actual, resultSize);

        ok(memcmp(expected, actual, resultSize) == 0, "pbkdf-sha-1: P-'password' S-'Salt' I-1");
    }

    {
        const char *password =          "password";
        const char *salt =              "salt";
        const int iterations =          2;
        const uint8_t expected[20] =  { 0xea, 0x6c, 0x01, 0x4d,
                                        0xc7, 0x2d, 0x6f, 0x8c,
                                        0xcd, 0x1e, 0xd9, 0x2a,
                                        0xce, 0x1d, 0x41, 0xf0,
                                        0xd8, 0xde, 0x89, 0x57 };

        const char resultSize = sizeof(expected);

        uint8_t actual[resultSize];

        pbkdf2_hmac_sha1((const uint8_t*) password, strlen(password), (const uint8_t*) salt, strlen(salt), iterations, actual, resultSize);

        ok(memcmp(expected, actual, resultSize) == 0, "pbkdf-sha-1: P-'password' S-'Salt' I-2");
    }

    {
        const char *password =          "password";
        const char *salt =              "salt";
        const int iterations =          4096;
        const uint8_t expected[20] =  { 0x4b, 0x00, 0x79, 0x01,
                                        0xb7, 0x65, 0x48, 0x9a,
                                        0xbe, 0xad, 0x49, 0xd9,
                                        0x26, 0xf7, 0x21, 0xd0,
                                        0x65, 0xa4, 0x29, 0xc1 };

        const char resultSize = sizeof(expected);

        uint8_t actual[resultSize];

        pbkdf2_hmac_sha1((const uint8_t*) password, strlen(password), (const uint8_t*) salt, strlen(salt), iterations, actual, resultSize);

        ok(memcmp(expected, actual, resultSize) == 0, "pbkdf-sha-1: P-'password' S-'Salt' I-4096");
    }

    SKIP: {
        skip("16777216 iterations is too slow", 1, 0);

        const char *password =          "password";
        const char *salt =              "salt";
        const int iterations =          16777216;
        const uint8_t expected[20] =  { 0xee, 0xfe, 0x3d, 0x61,
                                        0xcd, 0x4d, 0xa4, 0xe4,
                                        0xe9, 0x94, 0x5b, 0x3d,
                                        0x6b, 0xa2, 0x15, 0x8c,
                                        0x26, 0x34, 0xe9, 0x84 };

        const char resultSize = sizeof(expected);

        uint8_t actual[resultSize];

        pbkdf2_hmac_sha1((const uint8_t*) password, strlen(password), (const uint8_t*) salt, strlen(salt), iterations, actual, resultSize);

        ok(memcmp(expected, actual, resultSize) == 0, "pbkdf-sha-1: P-'password' S-'Salt' I-16777216");
    }


    {
        CFStringRef password    = CFStringCreateWithCString(NULL, "password", kCFStringEncodingUTF8);
        CFStringRef salt        = CFStringCreateWithCString(NULL, "salt", kCFStringEncodingUTF8);

        CFDataRef   passwordData    = CFStringCreateExternalRepresentation(NULL, password, kCFStringEncodingUTF8, 0);
        CFDataRef   saltData        = CFStringCreateExternalRepresentation(NULL, salt, kCFStringEncodingUTF8, 0);

        const int iterations =          1;
        const uint8_t expected[20] =  { 0x0c, 0x60, 0xc8, 0x0f,
                                        0x96, 0x1f, 0x0e, 0x71,
                                        0xf3, 0xa9, 0xb5, 0x24,
                                        0xaf, 0x60, 0x12, 0x06,
                                        0x2f, 0xe0, 0x37, 0xa6 };

        const char resultSize = sizeof(expected);

        CFMutableDataRef resultData = CFDataCreateMutable(NULL, resultSize);
        CFDataIncreaseLength(resultData, resultSize);

        SecKeyFromPassphraseDataHMACSHA1(passwordData, saltData, iterations, resultData);

        ok(memcmp(expected, CFDataGetBytePtr(resultData), resultSize) == 0, "pbkdf-sha-1: P-'password' S-'Salt' I-1");

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
        const uint8_t expected[20] =  { 0xea, 0x6c, 0x01, 0x4d,
                                        0xc7, 0x2d, 0x6f, 0x8c,
                                        0xcd, 0x1e, 0xd9, 0x2a,
                                        0xce, 0x1d, 0x41, 0xf0,
                                        0xd8, 0xde, 0x89, 0x57 };

        const char resultSize = sizeof(expected);

        CFMutableDataRef resultData = CFDataCreateMutable(NULL, resultSize);
        CFDataIncreaseLength(resultData, resultSize);

        SecKeyFromPassphraseDataHMACSHA1(passwordData, saltData, iterations, resultData);

        ok(memcmp(expected, CFDataGetBytePtr(resultData), resultSize) == 0, "pbkdf-sha-1: P-'password' S-'Salt' I-1");

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
        const uint8_t expected[20] =  { 0x4b, 0x00, 0x79, 0x01,
                                        0xb7, 0x65, 0x48, 0x9a,
                                        0xbe, 0xad, 0x49, 0xd9,
                                        0x26, 0xf7, 0x21, 0xd0,
                                        0x65, 0xa4, 0x29, 0xc1 };


        const char resultSize = sizeof(expected);

        CFMutableDataRef resultData = CFDataCreateMutable(NULL, resultSize);
        CFDataIncreaseLength(resultData, resultSize);

        SecKeyFromPassphraseDataHMACSHA1(passwordData, saltData, iterations, resultData);

        ok(memcmp(expected, CFDataGetBytePtr(resultData), resultSize) == 0, "pbkdf-sha-1: P-'password' S-'Salt' I-1");

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
        const uint8_t expected[20] =  { 0xee, 0xfe, 0x3d, 0x61,
                                        0xcd, 0x4d, 0xa4, 0xe4,
                                        0xe9, 0x94, 0x5b, 0x3d,
                                        0x6b, 0xa2, 0x15, 0x8c,
                                        0x26, 0x34, 0xe9, 0x84 };


        const char resultSize = sizeof(expected);

        CFMutableDataRef resultData = CFDataCreateMutable(NULL, resultSize);
        CFDataIncreaseLength(resultData, resultSize);

        SecKeyFromPassphraseDataHMACSHA1(passwordData, saltData, iterations, resultData);

        ok(memcmp(expected, CFDataGetBytePtr(resultData), resultSize) == 0, "pbkdf-sha-1: P-'password' S-'Salt' I-1");

        CFReleaseSafe(password);
        CFReleaseSafe(salt);
        CFReleaseSafe(passwordData);
        CFReleaseSafe(saltData);
        CFReleaseSafe(resultData);
    }

}

int spbkdf_00_hmac_sha1(int argc, char *const *argv)
{
	plan_tests(kTestTestCount);

	tests();

	return 0;
}
