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
#include <utilities/SecCFWrappers.h>

#include "security.h"
#include "keychain_util.h"
#include "SecAccessControlPriv.h"
#include <libaks_acl_cf_keys.h>

void
display_sac_line(SecAccessControlRef sac, CFMutableStringRef line) {
    CFTypeRef protection = SecAccessControlGetProtection(sac);
    if(CFStringGetTypeID() == CFGetTypeID(protection))
    {
        CFStringAppend(line, protection);
    }
    
    CFDictionaryRef constraints = SecAccessControlGetConstraints(sac);
    if(constraints != NULL)
    {
        CFDictionaryForEach(constraints, ^(const void *key, const void *value) {
            CFStringAppend(line, CFSTR(";"));
            CFStringAppend(line, key);
            CFDictionaryRef constraintData = (CFDictionaryRef)value;
            
            if(CFStringGetTypeID() == CFGetTypeID(key) && CFDictionaryGetTypeID() == CFGetTypeID(value)) {
                CFDictionaryForEach(constraintData, ^(const void *constraintKey, const void *constraintValue) {
                    CFStringRef constraintType = (CFStringRef)constraintKey;
                    CFStringAppend(line, CFSTR(":"));
                    CFStringAppend(line, constraintType);
                    CFStringAppend(line, CFSTR("("));
                    
                    if(CFStringCompare(constraintType, CFSTR("policy"), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
                    {
                        // for policy, argument is plain string
                        CFStringAppend(line, (CFStringRef)constraintValue);
                    }
                    else if(CFStringCompare(constraintType, CFSTR("passcode"), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
                    {
                        // for passcode, we have to decode if system-passcode is used
                        if(CFStringCompare((CFStringRef)constraintValue, CFSTR("passcode"), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
                            CFStringAppend(line, CFSTR("yes"));
                        else
                            CFStringAppend(line, CFSTR("no"));
                    }
                    else if(CFStringCompare(constraintType, CFSTR("bio"), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
                    {
                        // for bio, argument is plain string
                        CFStringAppend(line, (CFStringRef)constraintValue);
                    }
                    else
                        CFStringAppend(line, CFSTR("Not yet supported"));
                    
                    CFStringAppend(line, CFSTR(")"));
                });
            } else {
                CFStringAppend(line, CFSTR(":"));
                if (value == kCFBooleanTrue) {
                    CFStringAppend(line, CFSTR("true"));
                } else if (value == kCFBooleanFalse) {
                    CFStringAppend(line, CFSTR("false"));
                } else {
                    CFStringAppend(line, CFSTR("unrecognized value"));
                }
            }
        });
    }
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

SecAccessControlRef
keychain_query_parse_sac(CFStringRef s) {
    SecAccessControlRef sac;
    CFArrayRef tokens = CFStringCreateArrayBySeparatingStrings(NULL, s, CFSTR(";"));

    // process protection part
    CFStringRef protection = CFArrayGetValueAtIndex(tokens, 0);
    
    CFErrorRef error = NULL;
    sac = SecAccessControlCreateWithFlags(NULL, protection, 0, &error);
    if(error != NULL)
    {
        return NULL;
    }
    
    CFIndex tokensCnt = CFArrayGetCount(tokens);
    CFArrayRef params = NULL;
    CFArrayRef constraints = NULL;
    CFArrayRef pair = NULL;
    CFStringRef constraintDetails = NULL;
    CFStringRef constraintType = NULL;
    bool paramError = false;
    
    for(CFIndex i = 1; i < tokensCnt; ++i)         // process all constraints
    {
        SecAccessConstraintRef constr = NULL;

        pair = CFStringCreateArrayBySeparatingStrings(NULL, CFArrayGetValueAtIndex(tokens, i), CFSTR(":"));
        if(CFArrayGetCount(pair) != 2)
        {
            paramError = true;
            goto paramErr;
        }
        CFStringRef operationName = CFArrayGetValueAtIndex(pair, 0);
        CFStringRef strConstraint = CFArrayGetValueAtIndex(pair, 1);
        
        if (CFStringHasSuffix(strConstraint, CFSTR(")"))) {
            CFStringRef tmp;
            tmp = CFStringCreateWithSubstring(NULL, strConstraint, CFRangeMake(0, CFStringGetLength(strConstraint) - 1));
            constraints = CFStringCreateArrayBySeparatingStrings(NULL, tmp, CFSTR("("));
            CFReleaseSafe(tmp);
            
            if ( CFArrayGetCount(constraints) != 2) {
                paramError = true;
                goto paramErr;
            }
            
            constraintType = CFArrayGetValueAtIndex(constraints, 0);
            constraintDetails = CFArrayGetValueAtIndex(constraints, 1);
            
            if (CFStringCompare(constraintType, CFSTR("policy"), kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
                constr = SecAccessConstraintCreatePolicy(constraintDetails, &error);
            }
            
        
        /* NOT SUPPORTED YET
        else if(CFStringCompare(constraintType, CFSTR("passcode"), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
        {
            bool system;
            if(CFStringCompare(constraintDetails, CFSTR("yes"), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
                system = true;
            else if(CFStringCompare(constraintDetails, CFSTR("no"), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
                system = false;
            else
            {
                printf("Wrong system parameter for passcode policy: [%s]\n", CFStringGetCStringPtr(constraintDetails, kCFStringEncodingUTF8));
                paramError = true;
                goto paramErr;
            }
            constr = SecAccessConstraintCreatePasscode(system);
        }
        else if(CFStringCompare(constraintType, CFSTR("bio"), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
        {
            constr = SecAccessConstraintCreateTouchID(CFStringCreateExternalRepresentation(NULL, constraintDetails, kCFStringEncodingASCII, 32), &error);
        }
        else if(CFStringCompare(constraintType, CFSTR("kofn"), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
        {
            CFIndex kofnParamCount = 0;
            bool foundBracket = false;
            for(CFIndex j = i + 1; j < tokensCnt; ++j)
            {
                ++kofnParamCount;
                if(CFStringHasSuffix(CFArrayGetValueAtIndex(tokens, j), CFSTR(")")))
                {
                    foundBracket = true;
                    break;
                }
            }
            if(!foundBracket || kofnParamCount < 2)
            {
                printf("Invalid syntax for kofn params\n");
                paramError = true;
                goto paramErr;
            }
            // process params
            size_t kofnRequired = CFStringGetIntValue(constraintDetails);
            CFMutableArrayRef kofnParams = CFArrayCreateMutable(NULL, kofnRequired, NULL);
            CFArrayAppendArray(kofnParams, tokens, CFRangeMake(i + 1, kofnParamCount)); // first param of kofn is number of required methods
            // remove ")" for the last method
            CFStringRef tmp = CFStringCreateWithSubstring(NULL, CFArrayGetValueAtIndex(kofnParams, kofnParamCount - 1),
                                                          CFRangeMake(0, CFStringGetLength(CFArrayGetValueAtIndex(kofnParams, kofnParamCount - 1)) - 1));
            CFArraySetValueAtIndex(kofnParams, kofnParamCount -1, tmp);
            i += kofnParamCount;

            // TODO: add this as soon as kOfn starts to work
                constr = SecAccessConstraintCreateKofN(kofnRequired, kofnParams, &error);
                printf("Created KOFN constraint required %zu\n", kofnRequired);
            CFReleaseNull(kofnParams);
            CFReleaseNull(tmp);
            
        }
        NOT YET IMPLEMENTED */
        else {
            paramError = true;
            goto paramErr;
        }
        
        } else if (CFStringCompare(strConstraint, CFSTR("true"), kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
            constr = kCFBooleanTrue;
        
        } else if (CFStringCompare(strConstraint, CFSTR("false"), kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
            constr = kCFBooleanFalse;
        }
    
    
        if (error || constr == NULL) {
            paramError = true;
            goto paramErr;
        }
        
        SecAccessControlAddConstraintForOperation(sac, operationName, constr, &error);
        if (error) {
            paramError = true;
            goto paramErr;
        }
    paramErr:
        CFReleaseNull(pair);
        CFReleaseNull(params);
        CFReleaseNull(constraints);
        CFReleaseNull(constr);
    
        if (paramError) {
            break;
        }
    }
    
    CFReleaseSafe(tokens);

    SecAccessConstraintRef constraintForDelete = SecAccessControlGetConstraint(sac, kAKSKeyOpDelete);
    if (!constraintForDelete) {
        if(!SecAccessControlAddConstraintForOperation(sac, kAKSKeyOpDelete, kCFBooleanTrue, &error)) {
            fprintf(stderr, "adding delete operation to sac object failed \n");
        }
    }

    if (paramError) {
        fprintf(stderr, "Error constructing SecAccessConstraint object\n");
        CFReleaseSafe(sac);
        return NULL;
    }

    return sac;
}
