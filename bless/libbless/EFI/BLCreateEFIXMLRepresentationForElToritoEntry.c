/*
 * Copyright (c) 2013 Apple Inc. All Rights Reserved.
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

#include <TargetConditionals.h>

#import <IOKit/IOKitLib.h>
#import <IOKit/IOCFSerialize.h>
#import <IOKit/IOBSD.h>
#import <IOKit/IOKitKeys.h>
#import <IOKit/storage/IOMedia.h>

#import <CoreFoundation/CoreFoundation.h>

#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>

#include "bless.h"
#include "bless_private.h"



//
// This routine adds a matching dict to the given array. The matching dict codes for the iokit
// entry with the given BSD name and does so by using the registry entry ID.
//
static bool appendIOMedia (BLContextPtr inContext, const char* inBSDName, CFMutableArrayRef inoutArray)
{
    bool                    retSuccess = false;
    CFMutableDictionaryRef  dict;
    CFMutableDictionaryRef  match;
    io_service_t            media;
    kern_return_t           kr;
    uint64_t                entryID;
    CFStringRef             string;
    
    match = IOBSDNameMatching (kIOMasterPortDefault, 0, inBSDName);
    media = IOServiceGetMatchingService (kIOMasterPortDefault, match);
    
    // can use some other type of matching instead of registry entry ID match?
    
    kr = IORegistryEntryGetRegistryEntryID (media, &entryID);
    if (kr != KERN_SUCCESS)
    {
        contextprintf (inContext, kBLLogLevelVerbose, "IODVDMedia get registry ID failed\n");
        goto Exit;
    }
    
    match = IORegistryEntryIDMatching (entryID);
    
    dict = CFDictionaryCreateMutable (kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                      &kCFTypeDictionaryValueCallBacks);
    
    CFDictionaryAddValue (dict, CFSTR("IOMatch"), match);
    CFRelease (match);
    
    // Hint as to the last BSD name. EFI shouldn't care; it is only a hint for programmers
    string = CFStringCreateWithCString (kCFAllocatorDefault, inBSDName, kCFStringEncodingUTF8);
    CFDictionaryAddValue (dict, CFSTR("BLLastBSDName"), string);
    
    CFArrayAppendValue (inoutArray, dict);
    CFRelease (dict);
    
    retSuccess = true;
    
    Exit:;
    return retSuccess;
}



//
// This routine adds a dict with several fields to the given array.
//
static void appendMediaCDROM (BLContextPtr inContext, CFMutableArrayRef inoutArray, uint32_t inBootEntry, uint32_t inMSDOSRegionOffset, uint32_t inMSDOSRegionSize)
{
    CFMutableDictionaryRef  dict;
    CFNumberRef             number;
    uint32_t                value;
    
    dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                     &kCFTypeDictionaryValueCallBacks);
    
    CFDictionaryAddValue(dict, CFSTR("IOEFIDevicePathType"), CFSTR("MediaCDROM"));
    
    value = inBootEntry;
    number = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &value);
    CFDictionaryAddValue(dict, CFSTR("BootEntry"), number);
    CFRelease(number);
    
    value = inMSDOSRegionOffset;
    number = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &value);
    CFDictionaryAddValue(dict, CFSTR("PartitionStart"), number);
    CFRelease(number);
    
    value = inMSDOSRegionSize;
    number = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &value);
    CFDictionaryAddValue(dict, CFSTR("PartitionSize"), number);
    CFRelease(number);
    
    CFArrayAppendValue(inoutArray, dict);
    CFRelease(dict);
}



//
// This routine adds a dict with a pathname to the given array:
//
static void appendMediaFilePath (BLContextPtr inContext, CFMutableArrayRef inoutArray)
{
    CFMutableDictionaryRef  dict;
    CFStringRef             string;
    const char *            filePath = "\\EFI\\BOOT\\BOOTX64.efi";
    
    dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                     &kCFTypeDictionaryValueCallBacks);
    
    CFDictionaryAddValue(dict, CFSTR("IOEFIDevicePathType"), CFSTR("MediaFilePath"));
    
    string = CFStringCreateWithCString(kCFAllocatorDefault, filePath, kCFStringEncodingUTF8);
    CFDictionaryAddValue(dict, CFSTR("Path"), string);
    CFRelease(string);
    
    CFArrayAppendValue(inoutArray, dict);
    CFRelease(dict);
}



int BLCreateEFIXMLRepresentationForElToritoEntry(BLContextPtr   inContext,
                                                 const char *   inBSDName,
                                                 int            inBootEntry,
                                                 int            inPartitionStart,
                                                 int            inPartitionSize,
                                                 CFStringRef *  outXMLString)
{
    bool                    retErr = 0;
    
    CFMutableArrayRef       arrayOfDicts = NULL;
    
    CFDataRef               xmlData = NULL;
    CFIndex                 count;
    const UInt8 *           xmlBuffer = NULL;
    UInt8 *                 outBuffer = NULL;
    
    char                    buff [2048];
    bool                    aBool;
    
    contextprintf(inContext, kBLLogLevelVerbose, "Creating XML representation for ElTorito entry\n");
    
    // Create array. Create the top level array, empty at first:
    arrayOfDicts = CFArrayCreateMutable (kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    
    // Add to array: Add dict with IOMedia matching dict. Not sure why EFI cares:
    aBool = appendIOMedia (inContext, inBSDName, arrayOfDicts);
    if (false == aBool)
    {
        CFRelease (arrayOfDicts);
        retErr = 1;
        goto Exit;
    }
    
    // Add to array: Add dict with msdos region's offset/size to array. Where on disc the bootable OS
    // file system volume is:
    appendMediaCDROM (inContext, arrayOfDicts, inBootEntry, inPartitionStart, inPartitionSize);
    
    // Add to array: Add dict with in-msdos-path-to-boot-program-file to array. Just a path to the booter
    // that represents extra data that is used to find the booter on the file system volume:
    appendMediaFilePath (inContext, arrayOfDicts);
    
    // Verbose mode print:
    CFStringRef desc = CFCopyDescription (arrayOfDicts);
    CFStringGetCString (desc, buff, sizeof(buff)-1, kCFStringEncodingUTF8);
    contextprintf (inContext, kBLLogLevelVerbose, "array destined for XML then IORegistryEntrySetCFProperty() then NVRAM:\n\"\n%s\n\"\n", buff);
    
    // Turn the arrayOfDicts into an XML string (stored in a CFData). We no longer need the array:
    xmlData = IOCFSerialize (arrayOfDicts, 0);
    CFRelease (arrayOfDicts);
    
    if (xmlData == NULL) {
        contextprintf (inContext, kBLLogLevelVerbose, "Can't create XML representation\n");
        retErr = 2;
        goto Exit;
    }
    
    // Allocte a buffer and copy the XML string (as CFData) into a buffer and make that buffer
    // zero-terminated. We no longer need the CFData:
    count = CFDataGetLength (xmlData);
    xmlBuffer = CFDataGetBytePtr (xmlData);
    outBuffer = calloc (count+1, sizeof(char));     // allocate one more byte for termination, also zero all bytes
    memcpy (outBuffer, xmlBuffer, count);
    CFRelease (xmlData);
    contextprintf (inContext, kBLLogLevelVerbose, "array in XML form:\n\"\n%s\n\"\n", outBuffer);
    
    // Now make a CFString out of the zero-terminated string, and return it to the caller:
    *outXMLString = CFStringCreateWithCString(kCFAllocatorDefault, (const char *)outBuffer, kCFStringEncodingUTF8);
    
    // We no longer need the zero-terminated string buffer:
    free(outBuffer);
    
    Exit:;
    return retErr;
}

