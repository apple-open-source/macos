/*
 * Copyright © 1998-2012 Apple Inc. All rights reserved.
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

#include <math.h>

#include "IOUSBIUnknown.h"
#include "IOUSBDeviceClass.h"
#include "IOUSBInterfaceClass.h"
#include <IOKit/usb/IOUSBLib.h>

int IOUSBIUnknown::factoryRefCount = 0;

void 
*IOUSBLibFactory(CFAllocatorRef allocator, CFUUIDRef typeID)
{
#pragma unused (allocator)
   if (CFEqual(typeID, kIOUSBDeviceUserClientTypeID))
        return (void *) IOUSBDeviceClass::alloc();
    else if (CFEqual(typeID, kIOUSBInterfaceUserClientTypeID))
        return (void *) IOUSBInterfaceClass::alloc();
    return NULL;
}



void 
IOUSBIUnknown::factoryAddRef()
{
    if (0 == factoryRefCount++) 
    {
        CFUUIDRef factoryId = kIOUSBFactoryID;

        CFRetain(factoryId);
        CFPlugInAddInstanceForFactory(factoryId);
    }
}



void 
IOUSBIUnknown::factoryRelease()
{
    if (1 == factoryRefCount--) 
	{
        CFUUIDRef factoryId = kIOUSBFactoryID;
    
        CFPlugInRemoveInstanceForFactory(factoryId);
        CFRelease(factoryId);
    }
    else if (factoryRefCount < 0)
        factoryRefCount = 0;
}



IOUSBIUnknown::IOUSBIUnknown(void *unknownVTable)
: refCount(1)
{
    iunknown.pseudoVTable = (IUnknownVTbl *) unknownVTable;
    iunknown.obj = this;

    factoryAddRef();
};



IOUSBIUnknown::~IOUSBIUnknown()
{
    factoryRelease();
}



UInt32 
IOUSBIUnknown::addRef()
{
    refCount += 1;
    return refCount;
}



UInt32 
IOUSBIUnknown::release()
{
    unsigned long retVal = refCount - 1;

    if (retVal > 0)
        refCount = retVal;
    else if (retVal == 0) {
        refCount = retVal;
        delete this;
    }
    else
        retVal = 0;

    return retVal;
}



HRESULT IOUSBIUnknown::
genericQueryInterface(void *self, REFIID iid, void **ppv)
{
    IOUSBIUnknown *me = ((InterfaceMap *) self)->obj;
    return me->queryInterface(iid, ppv);
}



UInt32 
IOUSBIUnknown::genericAddRef(void *self)
{
    IOUSBIUnknown *me = ((InterfaceMap *) self)->obj;
    return me->addRef();
}



UInt32 
IOUSBIUnknown::genericRelease(void *self)
{
    IOUSBIUnknown *me = ((InterfaceMap *) self)->obj;
    return me->release();
}

IOReturn
IOUSBIUnknown::GetIOUSBLibVersion(NumVersion *ioUSBLibVersion, NumVersion *usbFamilyVersion)
{
    CFURLRef    bundleURL;
    CFBundleRef myBundle;
    UInt32  	usbFamilyBundleVersion;
    UInt32  	usbLibBundleVersion;
    UInt32 * 	tmp;
    
    // Make a CFURLRef from the CFString representation of the
    // bundle's path. See the Core Foundation URL Services chapter
    // for details.
    bundleURL = CFURLCreateWithFileSystemPath(
                                              kCFAllocatorDefault,
                                              CFSTR("/System/Library/Extensions/IOUSBFamily.kext"),
                                              kCFURLPOSIXPathStyle,
                                              true );
    
    // Make a bundle instance using the URLRef.
    myBundle = CFBundleCreate( kCFAllocatorDefault, bundleURL );
    
    // Look for the bundle's version number.  CFBundleGetVersionNumber() does not deal with versions > 99, so if
    // we get 0, roll our own decoding
    usbFamilyBundleVersion = CFBundleGetVersionNumber( myBundle );
    if (usbFamilyBundleVersion == 0)
    {
        usbFamilyBundleVersion = _versionNumberFromString((CFStringRef)CFBundleGetValueForInfoDictionaryKey(myBundle, kCFBundleVersionKey));
    }
    
    // Any CF objects returned from functions with "create" or
    // "copy" in their names must be released by us!
    CFRelease( bundleURL );
    CFRelease( myBundle );
    
    // Make a CFURLRef from the CFString representation of the
    // bundle's path. See the Core Foundation URL Services chapter
    // for details.
    bundleURL = CFURLCreateWithFileSystemPath(
                                              kCFAllocatorDefault,
                                              CFSTR("/System/Library/Extensions/IOUSBFamily.kext/Contents/PlugIns/IOUSBLib.bundle"),
                                              kCFURLPOSIXPathStyle,
                                              true );
    
    // Make a bundle instance using the URLRef.
    myBundle = CFBundleCreate( kCFAllocatorDefault, bundleURL );
    
    // Look for the bundle's version number.  CFBundleGetVersionNumber() does not deal with versions > 99, so if
    // we get 0, roll our own decoding
    usbLibBundleVersion = CFBundleGetVersionNumber( myBundle );
    if (usbLibBundleVersion == 0)
    {
        usbLibBundleVersion = _versionNumberFromString((CFStringRef)CFBundleGetValueForInfoDictionaryKey(myBundle, kCFBundleVersionKey));
    }
    
    // Any CF objects returned from functions with "create" or
    // "copy" in their names must be released by us!
    CFRelease( bundleURL );
    CFRelease( myBundle );
    
    // Cast the NumVersion to a UInt32 so we can just copy the data directly in.
    //
    if ( ioUSBLibVersion )
    {
        tmp = (UInt32 *) ioUSBLibVersion;
        *tmp = usbLibBundleVersion;
    }
    
    if ( usbFamilyVersion )
    {
        tmp = (UInt32 *) usbFamilyVersion;
        *tmp = usbFamilyBundleVersion;
    }
    
    return kIOReturnSuccess;
    
}

#pragma mark Utilities

#define DEVELOPMENT_STAGE 0x20
#define ALPHA_STAGE 0x40
#define BETA_STAGE 0x60
#define RELEASE_STAGE 0x80
#define MAX_VERS_LEN 20

inline Boolean
IOUSBIUnknown::_isDigit(UniChar aChar) {return ((aChar >= (UniChar)'0' && aChar <= (UniChar)'9') ? true : false);}

UInt32
IOUSBIUnknown::_versionNumberFromString(CFStringRef versStr)
{
    // Parse version number from a string that has XXXX.Y.ZZZ
    // String can end at any point, but elements within the string cannot be skipped.
    
    UInt32      major1 = 0, minor1 = 0,  stage = RELEASE_STAGE, build = 0;
    UniChar     versChars[MAX_VERS_LEN];
    UniChar *   chars = NULL;
    CFIndex     len;
    UInt32      theVers;
    CFIndex     majorCount = 0;
    UInt32      firstTuple = 0;
    
    if (!versStr)
        return 0;
    
    len = CFStringGetLength(versStr);
    if (len <= 0 || len > MAX_VERS_LEN)
        return 0;
    
    CFStringGetCharacters(versStr, CFRangeMake(0, len), versChars);
    chars = versChars;
    
    // Get first tuple
    if (_isDigit(*chars))
    {
        // Count how many digits in the tuple
        while (*chars != (UniChar)'.')
        {
            majorCount++;
            chars++;
        }
        
        majorCount--;
        chars = versChars;
        while (*chars != (UniChar)'.')
        {
            major1 = *chars - (UniChar)'0';
            firstTuple += major1 * pow(10, majorCount);
            majorCount--;
            chars++;
            len--;
            if (len == 0)
                break;
        }
        
        // Now we have the first tuple, create the Major and Minor versions from it.  Minor will be the 10's + 1's.
        // Major will be the rest.
        major1 = firstTuple / 100;
        minor1 = firstTuple - (major1*100);
        chars++;
        len--;
    }
    
    // Now either len is 0 or chars points at the build stage letter.
    // Get the build stage
    if (len > 0)
    {
        if (*chars == (UniChar)'1')
        {
            stage = DEVELOPMENT_STAGE;
        }
        else if (*chars == (UniChar)'2')
        {
            stage = ALPHA_STAGE;
        }
        else if (*chars == (UniChar)'3')
        {
            stage = BETA_STAGE;
        }
        else if (*chars == (UniChar)'4')
        {
            stage = RELEASE_STAGE;
        }
        else
        {
            stage = 0;
        }
        
        // Skip over the "." and point to the release
        chars++;
        chars++;
        len--;
        len--;
    }
    
    // Now stage contains the release stage.
    // Now either len is 0 or chars points at the build number.
    // Get the first digit of the build number.
    if (len > 0)
    {
        if (_isDigit(*chars))
        {
            build = *chars - (UniChar)'0';
            chars++;
            len--;
        }
    }
    
    // Get the second digit of the build number.
    if (len > 0)
    {
        if (_isDigit(*chars))
        {
            build *= 10;
            build += *chars - (UniChar)'0';
            chars++;
            len--;
        }
    }
    
    // Get the third digit of the build number.
    if (len > 0)
    {
        if (_isDigit(*chars))
        {
            build *= 10;
            build += *chars - (UniChar)'0';
            chars++;
            len--;
        }
    }
    
    // Range check
    if (build > 0xFF)
        build = 0xFF;
    
    // Build the number
    theVers = major1 << 24;
    theVers += minor1 << 16;
    theVers += stage << 8;
    theVers += build;
    
    return theVers;
}
