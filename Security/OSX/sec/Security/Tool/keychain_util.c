/*
 * Copyright (c) 2014 Apple Inc. All Rights Reserved.
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


#include <stdio.h>
#include <AssertMacros.h>
#include <utilities/SecCFWrappers.h>
#include <ACMDefs.h>
#include <ACMAclDefs.h>
#include <libaks_acl_cf_keys.h>

#include "security.h"
#include "keychain_util.h"
#include "SecAccessControlPriv.h"

static CFStringRef createStringForValue(CFTypeRef value)
{
    CFStringRef result = NULL;
    if (CFDataGetTypeID() == CFGetTypeID(value)) {
        CFMutableStringRef stringData = CFStringCreateMutable(kCFAllocatorDefault, 0);
        CFStringAppend(stringData, CFSTR("<"));
        const UInt8 *dataPtr = CFDataGetBytePtr(value);
        for (int i = 0; i < CFDataGetLength(value); ++i) {
            CFStringAppendFormat(stringData, NULL, CFSTR("%02x"), dataPtr[i]);
        }
        CFStringAppend(stringData, CFSTR(">"));
        result = stringData;
    } else if (CFBooleanGetTypeID() == CFGetTypeID(value)) {
        if (CFEqual(value, kCFBooleanTrue))
            result = CFStringCreateWithCString(kCFAllocatorDefault, "true", kCFStringEncodingUTF8);
        else
            result = CFStringCreateWithCString(kCFAllocatorDefault, "false", kCFStringEncodingUTF8);
    } else if (CFNumberGetTypeID() == CFGetTypeID(value))
        result = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@"), value);
    else if (CFStringGetTypeID() == CFGetTypeID(value))
        result = CFStringCreateCopy(kCFAllocatorDefault, value);
    else
        result = CFStringCreateWithCString(kCFAllocatorDefault, "unrecognized value", kCFStringEncodingUTF8);

    return result;
}

static CFStringRef createStringForKofn(CFDictionaryRef kofn)
{
    CFMutableStringRef result = NULL;
    if (kofn != NULL && CFDictionaryGetTypeID() == CFGetTypeID(kofn))
    {
        result = CFStringCreateMutable(kCFAllocatorDefault, 0);
        CFDictionaryForEach(kofn, ^(const void *key, const void *value) {

            CFStringAppend(result, key);
            CFStringAppend(result, CFSTR("("));

            CFStringRef valueString = createStringForKofn(value) ?: createStringForValue(value);
            CFStringAppend(result, valueString);
            CFStringAppend(result, CFSTR(")"));
            CFReleaseSafe(valueString);
        });
    }

    return result;
}

static CFStringRef createStringForOps(CFDictionaryRef constraints)
{
    CFMutableStringRef result = NULL;
    if (constraints != NULL)
    {
        result = CFStringCreateMutable(kCFAllocatorDefault, 0);
        CFDictionaryForEach(constraints, ^(const void *key, const void *value) {
            if (CFStringGetLength(result) > 0)
                CFStringAppend(result, CFSTR(";"));
            CFStringAppend(result, key);
            CFStringAppend(result, CFSTR("("));
            CFStringRef valueString = createStringForKofn(value) ?: createStringForValue(value);
            CFStringAppend(result, valueString);
            CFStringAppend(result, CFSTR(")"));
            CFReleaseSafe(valueString);
        });
    }

    return result;
}

void
display_sac_line(SecAccessControlRef sac, CFMutableStringRef line)
{
    CFTypeRef protection = SecAccessControlGetProtection(sac);
    if (CFDictionaryGetTypeID() == CFGetTypeID(protection)) {
        CFStringRef protectionStr = createStringForOps(protection);
        CFStringAppend(line, protectionStr);
        CFRelease(protectionStr);
    } else if (CFStringGetTypeID() == CFGetTypeID(protection))
        CFStringAppend(line, protection);
    else
        CFStringAppend(line, CFSTR("??"));

    CFDictionaryRef constraints = SecAccessControlGetConstraints(sac);
    CFStringRef constraintsString = createStringForOps(constraints);
    if (constraintsString) {
        CFStringAppend(line, CFSTR(";"));
        CFStringAppend(line, constraintsString);
    }
    CFReleaseSafe(constraintsString);
}

bool
keychain_query_parse_cstring(CFMutableDictionaryRef q, const char *query) {
    CFStringRef s;
    s = CFStringCreateWithCStringNoCopy(0, query, kCFStringEncodingUTF8, kCFAllocatorNull);
    bool result = keychain_query_parse_string(q, s);
    CFRelease(s);
    return result;
}

/* Parse a string of the form attr=value,attr=value,attr=value */
bool
keychain_query_parse_string(CFMutableDictionaryRef q, CFStringRef s) {
    bool inkey = true;
    bool escaped = false;
    bool error = false;
    CFStringRef key = NULL;
    CFMutableStringRef str = CFStringCreateMutable(0, 0);
    CFRange rng = { .location = 0, .length = CFStringGetLength(s) };
    CFCharacterSetRef cs_key = CFCharacterSetCreateWithCharactersInString(0, CFSTR("=\\"));
    CFCharacterSetRef cs_value = CFCharacterSetCreateWithCharactersInString(0, CFSTR(",\\"));
    while (rng.length) {
        CFRange r;
        CFStringRef sub;
        bool complete = false;
        if (escaped) {
            r.location = rng.location;
            r.length = 1;
            sub = CFStringCreateWithSubstring(0, s, r);
            escaped = false;
        } else if (CFStringFindCharacterFromSet(s, inkey ? cs_key : cs_value, rng, 0, &r)) {
            if (CFStringGetCharacterAtIndex(s, r.location) == '\\') {
                escaped = true;
            } else {
                complete = true;
            }
            CFIndex next = r.location + 1;
            r.length = r.location - rng.location;
            r.location = rng.location;
            sub = CFStringCreateWithSubstring(0, s, r);
            rng.length -= next - rng.location;
            rng.location = next;
        } else {
            sub = CFStringCreateWithSubstring(0, s, rng);
            rng.location += rng.length;
            rng.length = 0;
            complete = true;
        }
        CFStringAppend(str, sub);
        CFRelease(sub);
        
        if (complete) {
            CFStringRef value = CFStringCreateCopy(0, str);
            CFStringReplaceAll(str, CFSTR(""));
            if (inkey) {
                key = value;
            } else {
                if(key && CFStringCompare(key, kSecAttrAccessControl, kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
                    SecAccessControlRef sac = keychain_query_parse_sac(value);
                    if(sac) {
                        CFDictionarySetValue(q, key, sac);
                    } else {
                        fprintf(stderr, "SecItemCopyMatching returned unexpected results:");
                        error = true;
                    }
                } else {
                    CFDictionarySetValue(q, key, value);
                }
                CFReleaseNull(value);
                CFReleaseNull(key);
            }
            inkey = !inkey;
        }
        if(error)
            break;
    }
    if (key) {
        /* Dangeling key value is true?. */
        CFDictionarySetValue(q, key, kCFBooleanTrue);
        CFReleaseNull(key);
    }
    CFRelease(str);
    CFRelease(cs_key);
    CFRelease(cs_value);
    return error == false;
}

static uint32_t findLeft(CFStringRef s, uint32_t off)
{
    for (int i = off; i < CFStringGetLength(s); ++i) {
        if (CFStringGetCharacterAtIndex(s, i) == '(')
            return i;
    }

    return 0;
}

static uint32_t findRight(CFStringRef s, uint32_t off)
{
    int bracersCount = 0;
    for (int i = off; i < CFStringGetLength(s); ++i) {
        if (CFStringGetCharacterAtIndex(s, i) == '(')
            ++bracersCount;

        if (CFStringGetCharacterAtIndex(s, i) == ')') {
            --bracersCount;
            if (bracersCount == 0) {
                return i;
            }
        }
    }

    return 0;
}

static bool parseKeyAndValue(CFStringRef string, CFTypeRef *key, CFTypeRef *value);

CF_RETURNS_RETAINED
static CFTypeRef parseValue(CFStringRef string)
{
    CFTypeRef result = NULL, key = NULL, value = NULL;
    CFMutableDictionaryRef resultDictionary = NULL;
    CFStringRef subString = NULL;

    uint32_t left = findLeft(string, 0);
    if (left > 0) {
        uint32_t offset = 0;
        resultDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        for (;;) {
            uint32_t right = findRight(string, left);
            if (!right)
                break;
            CFAssignRetained(subString, CFStringCreateWithSubstring(kCFAllocatorDefault, string, CFRangeMake(offset, (right + 1) - offset)));
            require_quiet(parseKeyAndValue(subString, &key, &value), out);
            CFDictionarySetValue(resultDictionary, key, value);
            offset = right + 1;
            left = findLeft(string, offset);
            if (!left)
                break;
        }
        result = CFRetain(resultDictionary);
    } else if (CFStringGetCharacterAtIndex(string, 0) == '<' && CFStringGetCharacterAtIndex(string, CFStringGetLength(string) - 1) == '>') {
        CFMutableDataRef data = CFDataCreateMutable(kCFAllocatorDefault, 0);
        if (CFStringGetLength(string) > 2) {
            CFAssignRetained(subString, CFStringCreateWithSubstring(kCFAllocatorDefault, string, CFRangeMake(1, CFStringGetLength(string) - 2)));
            const char *asciiString = CFStringGetCStringPtr(subString, kCFStringEncodingASCII);
            uint8_t byte;
            for(uint32_t i = 0; i < strlen(asciiString); i += 2) {
                sscanf(&asciiString[i], "%02hhx", &byte);
                CFDataAppendBytes(data, &byte, sizeof(byte));
            }
        }
        result = data;
    } else if (CFStringCompare(string, CFSTR("true"), 0) == kCFCompareEqualTo) {
        CFRetainAssign(result, kCFBooleanTrue);
    } else if (CFStringCompare(string, CFSTR("false"), 0) == kCFCompareEqualTo) {
        CFRetainAssign(result, kCFBooleanFalse);
    } else if (CFStringCompare(string, CFSTR(kACMPolicyDeviceOwnerAuthentication), 0) == kCFCompareEqualTo) {
        CFRetainAssign(result, CFSTR(kACMPolicyDeviceOwnerAuthentication));
    } else {
        CFLocaleRef currentLocale = CFLocaleCopyCurrent();
        CFNumberFormatterRef formaterRef = CFNumberFormatterCreate(kCFAllocatorDefault, currentLocale, kCFNumberFormatterDecimalStyle);
        result = CFNumberFormatterCreateNumberFromString(kCFAllocatorDefault, formaterRef, string, NULL, kCFNumberFormatterParseIntegersOnly);
        CFReleaseSafe(currentLocale);
        CFReleaseSafe(formaterRef);
    }

    if (!result)
        fprintf(stderr, "Failed to parse value: %s\n", CFStringGetCStringPtr(string, kCFStringEncodingUTF8));

out:
    CFReleaseSafe(key);
    CFReleaseSafe(value);
    CFReleaseSafe(subString);
    CFReleaseSafe(resultDictionary);

    return result;
}

static bool parseKeyAndValue(CFStringRef string, CFTypeRef *key, CFTypeRef *value)
{
    bool ok = false;
    CFStringRef keyString = NULL;
    CFStringRef valueString = NULL;
    CFTypeRef parsedValue = NULL;

    uint32_t left = findLeft(string, 0);
    require_action_quiet(left != 0, out, fprintf(stderr, "Failed to find '(' in: %s\n", CFStringGetCStringPtr(string, kCFStringEncodingUTF8)));
    uint32_t right = findRight(string, left);
    require_action_quiet(right != 0, out, fprintf(stderr, "Failed to find ')' in: %s\n", CFStringGetCStringPtr(string, kCFStringEncodingUTF8)));
    require_action_quiet(right == ((uint32_t)CFStringGetLength(string) - 1), out, fprintf(stderr, "Failed to find ')' in: %s\n", CFStringGetCStringPtr(string, kCFStringEncodingUTF8)));

    keyString  = CFStringCreateWithSubstring(kCFAllocatorDefault, string, CFRangeMake(0, left));
    valueString = CFStringCreateWithSubstring(kCFAllocatorDefault, string, CFRangeMake(left + 1, right - left - 1));
    require_quiet(parsedValue = parseValue(valueString), out);
    CFRetainAssign(*key, keyString);
    CFRetainAssign(*value, parsedValue);
    ok = true;

out:
    CFReleaseSafe(parsedValue);
    CFReleaseSafe(keyString);
    CFReleaseSafe(valueString);

    return ok;
}

SecAccessControlRef
keychain_query_parse_sac(CFStringRef s) {
    SecAccessControlRef sac = NULL, result = NULL;
    CFTypeRef key = NULL, value = NULL;
    CFArrayRef tokens = CFStringCreateArrayBySeparatingStrings(NULL, s, CFSTR(";"));

    // process protection part
    CFStringRef protection = CFArrayGetValueAtIndex(tokens, 0);
    
    CFErrorRef error = NULL;
    require_quiet(sac = SecAccessControlCreate(kCFAllocatorDefault, &error), out);
    require_quiet(SecAccessControlSetProtection(sac, protection, &error), out);

    CFIndex tokensCnt = CFArrayGetCount(tokens);
    for(CFIndex i = 1; i < tokensCnt; ++i) {
        require_action_quiet(parseKeyAndValue(CFArrayGetValueAtIndex(tokens, i), &key, &value), out, fprintf(stderr, "Error constructing SecAccessConstraint object\n") );

        if (CFEqual(key, CFSTR(kACMKeyAclParamRequirePasscode)))
            SecAccessControlSetRequirePassword(sac, CFEqual(value, kCFBooleanTrue)?true:false);
        else
            SecAccessControlAddConstraintForOperation(sac, key, value, NULL);
    }

    SecAccessConstraintRef constraintForDelete = SecAccessControlGetConstraint(sac, kAKSKeyOpDelete);
    if (!constraintForDelete) {
        if(!SecAccessControlAddConstraintForOperation(sac, kAKSKeyOpDelete, kCFBooleanTrue, &error)) {
            fprintf(stderr, "adding delete operation to sac object failed \n");
        }
    }

    SecAccessConstraintRef constraintForEncrypt = SecAccessControlGetConstraint(sac, kAKSKeyOpEncrypt);
    if (!constraintForEncrypt) {
        if(!SecAccessControlAddConstraintForOperation(sac, kAKSKeyOpEncrypt, kCFBooleanTrue, &error)) {
            fprintf(stderr, "adding encrypt operation to sac object failed \n");
        }
    }

    CFRetainAssign(result, sac);

out:
    CFReleaseSafe(tokens);
    CFReleaseSafe(key);
    CFReleaseSafe(value);
    CFReleaseSafe(sac);

    return result;
}
