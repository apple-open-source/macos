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

#include "CFAdditions.h"
#include "KEXTPrivate.h"

#include <IOKit/IOCFURLAccess.h>

KEXTEntityType KEXTBundleGetEntityType(void)
{
    return (KEXTEntityType)CFSTR("BundleDescriptor");
}

CFStringRef KEXTBundleGetPrimaryKey(KEXTBundleRef bundle)
{
    return CFDictionaryGetValue(bundle, CFSTR("PrimaryKey"));
}

static inline CFStringRef _KEXTBundleGetName(CFDictionaryRef props, Boolean isCF)
{
    if ( isCF )
        return CFDictionaryGetValue(props, CFSTR("CFBundleIdentifier"));
    else
        return CFDictionaryGetValue(props, CFSTR("Name"));
}

static inline CFStringRef
_KEXTBundleCopyInfo(CFDictionaryRef props, Boolean isCF)
{
    CFStringRef infoStr;

    if ( isCF ) {
        infoStr = CFDictionaryGetValue(props, CFSTR("CFBundleGetInfoString"));
        if (infoStr)
            goto retainAndReturn;

        infoStr = CFDictionaryGetValue(props, CFSTR("CFBundleGetInfoHTML"));
        if (infoStr)
            goto retainAndReturn;

        infoStr = CFDictionaryGetValue(props, CFSTR("CFBundleVersion"));
        if (!infoStr)
            goto nullOut;
    }
    else {
        CFStringRef vendor = CFDictionaryGetValue(props, CFSTR(kVendorKey));
        CFStringRef version = CFDictionaryGetValue(props, CFSTR("Version"));

        if (vendor && version) {
            infoStr = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
                            CFSTR("%@ - %@"), vendor, version);
            return infoStr;	// Don't retain;
        }
        else if (vendor)
            infoStr = vendor;
        else if (version)
            infoStr = version;
        else
            goto nullOut;
    }

retainAndReturn:
    CFRetain(infoStr);
    return infoStr;

nullOut:
    return CFSTR("");
}

KEXTBundleRef KEXTBundleCreate(CFURLRef url, CFDictionaryRef properties, Boolean isCFStyle)
{
    CFStringRef path;
    CFStringRef name;
    CFStringRef info;
    CFStringRef primaryKey;
    KEXTBundleRef bundle;

    if ( !properties || !url || (CFURLGetTypeID() != CFGetTypeID(url)) ) {
        return NULL;
    }

    bundle = CFDictionaryCreateMutable(
                        kCFAllocatorDefault, 0,
                        &kCFTypeDictionaryKeyCallBacks,
                        &kCFTypeDictionaryValueCallBacks);

    if ( !bundle ) {
        // It would be pretty unusual to get this far and not be
        // able to get the date, but it could happen.
        return NULL;
    }

    name = _KEXTBundleGetName(properties, isCFStyle);
    if ( !name ) {
        CFRelease(bundle);
        return NULL;
    }

    CFDictionarySetValue(bundle, CFSTR("BundleName"), name);
    primaryKey = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("KEXTBundle?%@"), name);
    CFDictionarySetValue(bundle, CFSTR("PrimaryKey"), primaryKey);
    CFRelease(primaryKey);
    CFDictionarySetValue(bundle, CFSTR("BundleURL"), CFURLGetString(url));
    CFDictionarySetValue(bundle, CFSTR("EntityType"), CFSTR("BundleDescriptor"));

    path = CFURLGetString(url);
    CFDictionarySetValue(bundle, CFSTR("BundlePath"), path);
    
    CFDictionarySetValue(bundle, CFSTR("BundleFormat"),
        ( isCFStyle )? CFSTR("CF") : CFSTR("Legacy"));

    info = _KEXTBundleCopyInfo(properties, isCFStyle);
    if ( info )
        CFDictionarySetValue(bundle, CFSTR("BundleInfo"), info);

    return bundle;
}

KEXTBundleRef KEXTBundleRetain(KEXTBundleRef bundle)
{
    return (KEXTBundleRef)CFRetain(bundle);
}

void KEXTBundleRelease(KEXTBundleRef bundle)
{
    CFRelease(bundle);
}

KEXTValidation KEXTBundleValidate(KEXTBundleRef bundle)
{
    KEXTValidation ret;
    CFDateRef date;
    CFDateRef moddate;
    CFURLRef url;
    CFBooleanRef val;
    SInt32 err;

    if ( !bundle ) {
        printf("Not bundle???\n");
        return 1;
    }

    url = KEXTBundleCopyURL(bundle);
    if ( !url ) {
        return 0;
    }
    
    ret = kKEXTBundleValidateNoChange;
    do {
        Boolean boolval;
        val = IOURLCreatePropertyFromResource(kCFAllocatorDefault, url, kIOURLFileExists, &err);
        if ( !val ) {
            //printf("unable to get kIOURLFileExists property (%d)\n", err);
            ret = kKEXTBundleValidateRemoved;
            break;
        }

        boolval = CFBooleanGetValue(val);
        CFRelease(val);
        if ( !boolval ) {
            ret = kKEXTBundleValidateRemoved;
            break;
        }

        date = IOURLCreatePropertyFromResource(kCFAllocatorDefault, url, kIOURLFileLastModificationTime, &err);
        if ( !date ) {
            //printf("unable to get kIOURLFileLastModificationTime property (%d)\n", err);
            ret = kKEXTBundleValidateRemoved;
            break;
        }

        moddate = CFDictionaryGetValue((CFMutableDictionaryRef)bundle, CFSTR("ModificationDate"));
        boolval = CFEqual(moddate, date);
        CFRelease(date);
        if ( !boolval ) {
            ret = kKEXTBundleValidateModified;
            break;
        }
    } while ( false );

    CFRelease(url);

    return ret;
}

CFURLRef KEXTBundleCopyURL(KEXTBundleRef bundle)
{
    CFStringRef string;
    CFURLRef url;
    
    if ( !bundle )
        return NULL;
    
    string = CFDictionaryGetValue(bundle, CFSTR("BundleURL"));
    if ( !string )
        return NULL;

    url = CFURLCreateWithString(kCFAllocatorDefault, string, NULL);

    return url;
}

CFTypeRef KEXTBundleGetProperty(KEXTBundleRef bundle, CFStringRef key)
{
    if ( !key || !bundle )
        return NULL;

    return CFDictionaryGetValue(bundle, key);
}

static CFURLRef _KEXTBundleCreateURLForPath(CFStringRef path)
{
    CFURLRef url;
    CFBooleanRef val;
    Boolean bval;
    SInt32 err;

    url = CFURLCreateWithString(kCFAllocatorDefault, path, NULL);
    if ( !url ) {
        return NULL;
    }

    val = IOURLCreatePropertyFromResource(kCFAllocatorDefault, url, kIOURLFileExists, &err);
    if ( !val ) {
        CFRelease(url);
        return NULL;
    }
    
    bval = CFBooleanGetValue(val);
    CFRelease(val);
    if ( !bval ) {
        CFRelease(url);
        return NULL;
    }

    return url;
}

__private_extern__ CFURLRef
_KEXTBundleCreateFileURL(KEXTBundleRef bundle, CFStringRef name, CFStringRef type)
{
    CFURLRef newUrl;
    CFStringRef urlStr;
    CFStringRef formatStr;
    CFStringRef fileStr;
    CFStringRef newPath;

    urlStr = CFDictionaryGetValue(bundle, CFSTR("BundleURL"));
    if ( !urlStr )
        return NULL;
    
    if ( type )
        fileStr = CFStringCreateWithFormat(NULL, NULL,
                                           CFSTR("%@.%@"), name, type);
    else {
        fileStr = name;
        CFRetain(fileStr);
    }

    if ( CFStringHasSuffix(urlStr, CFSTR("/")) )
        formatStr = CFSTR("%@%@%@");
    else
        formatStr = CFSTR("%@/%@%@");
    
    // XXX -- svail: Search Support Files directory first.
    newPath = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, formatStr, urlStr, CFSTR("Contents/"), fileStr);
    newUrl = _KEXTBundleCreateURLForPath(newPath);
    CFRelease(newPath);
    if ( newUrl )
        goto foundFileNowExit;

    // @@@gvdl: Is this function used for more than just the exeutable file
    // search.  If so then this is a hacking.
    // Search in the MacOS executable directory
    newPath = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, formatStr, urlStr, CFSTR("Contents/MacOS/"), fileStr);
    newUrl = _KEXTBundleCreateURLForPath(newPath);
    CFRelease(newPath);
    if ( newUrl )
        goto foundFileNowExit;

    // XXX -- svail: Search in the Resources directory.
    newPath = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, formatStr, urlStr, CFSTR("Contents/Resources/"), fileStr);
    newUrl = _KEXTBundleCreateURLForPath(newPath);
    CFRelease(newPath);
    if ( newUrl )
        goto foundFileNowExit;

    // XXX -- svail: Finally, search in the top level directory for backward compatibility.
    newPath = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, formatStr, urlStr, CFSTR(""), fileStr);
    newUrl = _KEXTBundleCreateURLForPath(newPath);
    CFRelease(newPath);

foundFileNowExit:
    CFRelease(fileStr);
    return newUrl;
}

CFURLRef KEXTBundleCreateURLForResource(KEXTBundleRef bundle, CFStringRef name, CFStringRef type, KEXTReturn * error)
{
    CFURLRef url;

    if ( !bundle || !name ) {
        *error = kKEXTReturnBadArgument;
        return NULL;
    }

    url = _KEXTBundleCreateFileURL(bundle, name, type);
    if ( !url ) {
        *error = kKEXTReturnResourceNotFound;
        return NULL;
    }

    return url;
}

Boolean KEXTBundleEqual(KEXTBundleRef bundle1, KEXTBundleRef bundle2)
{
    return CFEqual(bundle1, bundle2);
}

Boolean KEXTBundleMatchProperties(KEXTBundleRef bundle1, KEXTBundleRef bundle2, CFTypeRef match)
{
    if ( !bundle1 || !bundle2 )
        return false;
    
    return CFDictionaryMatch(bundle1, bundle2, match);
}

CFArrayRef KEXTBundleGetModuleKeys(KEXTBundleRef bundle)
{
    return CFDictionaryGetValue(bundle, CFSTR("BundleModules"));
}

CFArrayRef KEXTBundleGetPersonalityKeys(KEXTBundleRef bundle)
{
    return CFDictionaryGetValue(bundle, CFSTR("BundlePersons"));
}

void _KEXTBundleShow(KEXTBundleRef bundle)
{
    if ( bundle ) {
        CFShow(bundle);
    }
}

