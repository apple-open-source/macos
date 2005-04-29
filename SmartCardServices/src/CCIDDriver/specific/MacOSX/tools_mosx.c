/*
 *  tools_mosx.c
 *  ifd-CCID
 *
 *  Created by JL on Mon Feb 10 2003.
 *  Copyright (c) 2003 Jean-Luc Giraud. All rights reserved.
 *  See COPYING file for license.
 *
 */

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFURL.h>
#include <CoreFoundation/CFPlugIn.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/usb/IOUSBLib.h>


#include "tools.h"



const char* ParseInfoPlist(const char *bundleIdentifier, const char *keyName)
{
    CFBundleRef myBundle;
    CFStringRef propertyString;
    CFDictionaryRef bundleInfoDict;
    const char *cStringValue;
    CFStringRef stringCF;
    
    stringCF = CFStringCreateWithCString ( NULL, bundleIdentifier, kCFStringEncodingASCII);
    if (stringCF == NULL)
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, "bundleIdentiferCF error");
        return NULL;
    }
    
    
    // Look for a bundle using its identifier
    myBundle = CFBundleGetBundleWithIdentifier(stringCF);
    if ( !myBundle )
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, "Bundle Identifier not found: %s", bundleIdentifier);
        return NULL;
    }
    bundleInfoDict = CFBundleGetInfoDictionary(myBundle);
    if (bundleInfoDict == NULL)
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, "Bundle InfoDic error");
        return NULL;
    }

    stringCF = CFStringCreateWithCString ( NULL, keyName, kCFStringEncodingASCII);
    if (stringCF == NULL)
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, "keyName error");
        return NULL;
    }

    propertyString = CFDictionaryGetValue(bundleInfoDict, stringCF);
    CFRelease(stringCF);
    if (propertyString == NULL)
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, "Bundle InfoDic error: %s not found", keyName);
        return NULL;
    }

    cStringValue = CFStringGetCStringPtr(propertyString,
                                         CFStringGetSystemEncoding());
    return cStringValue;
}


DWORD CCIDToHostLong(DWORD dword)
{
    return (USBToHostLong((UInt32) dword));
}
DWORD HostToCCIDLong(DWORD dword)
{
    return (HostToUSBLong((UInt32) dword));
}
WORD CCIDToHostWord(WORD word)
{
    return (USBToHostWord((UInt16) word));
}
WORD HostToCCIDWord(WORD word)
{
    return (USBToHostWord((UInt16) word));
}
