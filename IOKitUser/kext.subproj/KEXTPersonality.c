/*
 * Copyright (c) 1999-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * HISTORY
 *
 */

#include "KEXTPrivate.h"
#include <sys/time.h>

KEXTEntityType KEXTPersonalityGetEntityType(void)
{
    return (KEXTEntityType)CFSTR("PersonDescriptor");
}

static CFStringRef
_KEXTPersonalityCreateGUID(CFStringRef parentKey, CFStringRef name)
{
    CFStringRef guid;

    guid = CFStringCreateWithFormat(
                            kCFAllocatorDefault, NULL,
                            CFSTR("%@?KEXTPersonality?%@"),
                            parentKey,
                            name);

    return guid;
}

KEXTPersonalityRef KEXTPersonalityCreate(CFStringRef parentKey, CFDictionaryRef properties)
{
    KEXTPersonalityRef person;
    CFStringRef guid;
    CFStringRef name;
    CFMutableDictionaryRef dict;
    CFTypeRef props;


    if ( !parentKey || !properties ) {
        return NULL;
    }

    person = CFDictionaryCreateMutable(
                        kCFAllocatorDefault, 0,
                        &kCFTypeDictionaryKeyCallBacks,
                        &kCFTypeDictionaryValueCallBacks);
    if ( !person ) {
        return NULL;
    }
 
    dict = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0,  properties);
    if ( !dict ) {
        CFRelease(person);
        return NULL;
    }
 
    CFDictionarySetValue(person, CFSTR("EntityType"), KEXTPersonalityGetEntityType());

    // Move properties we don't want to go to the kernel to the top level.
    CFDictionarySetValue(person, CFSTR("PersonProperties"), dict);
    CFRelease(dict);
    props = CFDictionaryGetValue(dict, CFSTR("EditableProperties"));
    if ( props ) {
        CFDictionarySetValue(person, CFSTR("EditableProperties"), props);
        CFDictionaryRemoveValue(dict, CFSTR("EditableProperties"));
    }
    props = CFDictionaryGetValue(dict, CFSTR("EditLayout"));
    if ( props ) {
        CFDictionarySetValue(person, CFSTR("EditLayout"), props);
        CFDictionaryRemoveValue(dict, CFSTR("EditLayout"));
    }
    
    CFDictionarySetValue(person, CFSTR("ParentKey"), parentKey);
    CFDictionarySetValue(dict, CFSTR("ParentKey"), parentKey);

    name = CFDictionaryGetValue(properties, CFSTR("IOPersonalityName"));
    guid = _KEXTPersonalityCreateGUID(parentKey, name);
    CFDictionarySetValue(person, CFSTR("PrimaryKey"), guid);
    CFRelease(guid);

    return person;
}

KEXTPersonalityRef KEXTPersonalityRetain(KEXTPersonalityRef personality)
{
    return (KEXTPersonalityRef)CFRetain(personality);
}

void KEXTPersonalityRelease(KEXTPersonalityRef personality)
{
    CFRelease(personality);
}

CFURLRef KEXTPersonalityCopyURL(KEXTPersonalityRef personality)
{
    CFStringRef string;
    CFURLRef url;

    if ( !personality ) {
        return NULL;
    }

    string = CFDictionaryGetValue(personality, CFSTR("ParentKey"));
    if ( !string ) {
        return NULL;
    }

    url = CFURLCreateWithString(kCFAllocatorDefault, string, NULL);

    return url;
}

CFTypeRef KEXTPersonalityGetProperty(KEXTPersonalityRef personality, CFStringRef key)
{
    CFDictionaryRef props;
    if ( !personality || !key) {
        return NULL;
    }

    props = CFDictionaryGetValue(personality, CFSTR("PersonProperties"));
    if ( !props ) {
        return NULL;
    }
    
    return CFDictionaryGetValue(props, key);
}

CFStringRef KEXTPersonalityGetPrimaryKey(KEXTPersonalityRef personality)
{
    return CFDictionaryGetValue(personality, CFSTR("PrimaryKey"));
}

CFDictionaryRef _KEXTPersonalityGetProperties(KEXTPersonalityRef personality)
{
    return CFDictionaryGetValue(personality, CFSTR("PersonProperties"));
}

Boolean KEXTPersonalityEqual(KEXTPersonalityRef personality1, KEXTPersonalityRef personality2)
{
    CFDictionaryRef prop1;
    CFDictionaryRef prop2;

    prop1 = CFDictionaryGetValue(personality1, CFSTR("PersonProperties"));
    prop2 = CFDictionaryGetValue(personality2, CFSTR("PersonProperties"));

    if ( !prop1 || !prop2 ) {
        return NULL;
    }

    return CFEqual(prop1, prop2);
}


