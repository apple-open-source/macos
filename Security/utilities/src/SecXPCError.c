//
//  SecCFXPCWrappers.c
//  utilities
//
//  Created by John Hurley on 5/6/13.
//  Copyright (c) 2013 Apple Inc. All rights reserved.
//

#include <stdio.h>

#include <utilities/SecXPCError.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFWrappers.h>

CFStringRef sSecXPCErrorDomain = CFSTR("com.apple.security.xpc");

static const char* kDomainKey = "domain";
static const char* kDescriptionKey = "description";
static const char* kCodeKey = "code";

CFErrorRef SecCreateCFErrorWithXPCObject(xpc_object_t xpc_error)
{
    CFErrorRef result = NULL;

    if (xpc_get_type(xpc_error) == XPC_TYPE_DICTIONARY) {
        CFStringRef domain = NULL;

        const char * domain_string = xpc_dictionary_get_string(xpc_error, kDomainKey);
        if (domain_string != NULL) {
            domain = CFStringCreateWithCString(kCFAllocatorDefault, domain_string, kCFStringEncodingUTF8);
        } else {
            domain = sSecXPCErrorDomain;
            CFRetain(domain);
        }
        CFIndex code = (CFIndex) xpc_dictionary_get_int64(xpc_error, kCodeKey);

        const char *description = xpc_dictionary_get_string(xpc_error, kDescriptionKey);

        SecCFCreateErrorWithFormat(code, domain, NULL, &result, NULL, CFSTR("Remote error : %s"), description);

        CFReleaseSafe(domain);
    } else {
        SecCFCreateErrorWithFormat(kSecXPCErrorUnexpectedType, sSecXPCErrorDomain, NULL, &result, NULL, CFSTR("Remote error not dictionary!: %@"), xpc_error);
    }
    return result;
}

static void SecXPCDictionarySetCFString(xpc_object_t dict, const char *key, CFStringRef string)
{
    CFStringPerformWithCString(string, ^(const char *utf8Str) {
        xpc_dictionary_set_string(dict, key, utf8Str);
    });
}

xpc_object_t SecCreateXPCObjectWithCFError(CFErrorRef error)
{
    xpc_object_t error_xpc = xpc_dictionary_create(NULL, NULL, 0);

    SecXPCDictionarySetCFString(error_xpc, kDomainKey, CFErrorGetDomain(error));
    xpc_dictionary_set_int64(error_xpc, kCodeKey, CFErrorGetCode(error));

    CFStringRef description = CFErrorCopyDescription(error);
    SecXPCDictionarySetCFString(error_xpc, kDescriptionKey, description);
    CFReleaseNull(description);

    return error_xpc;
}
