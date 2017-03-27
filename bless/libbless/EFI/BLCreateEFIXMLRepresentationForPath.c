/*
 * Copyright (c) 2005-2007 Apple Inc. All Rights Reserved.
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
/*
 *  BLCreateEFIXMLRepresentationForPath.c
 *  bless
 *
 *  Created by Shantonu Sen on 11/9/05.
 *  Copyright 2005-2007 Apple Inc. All Rights Reserved.
 *
 */

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

#if USE_DISKARBITRATION
#include <DiskArbitration/DiskArbitration.h>
#endif

int addMatchingInfoForBSDName(BLContextPtr context,
			      mach_port_t masterPort,
			      CFMutableDictionaryRef dict,
			      const char *bsdName,
                  bool shortForm);

int BLCreateEFIXMLRepresentationForPath(BLContextPtr context,
                                        const char *path,
                                        const char *optionalData,
                                        CFStringRef *xmlString,
                                        bool shortForm)
{
    char fullpath[MAXPATHLEN];
    struct statfs sb;
    int ret;
    int i;
    size_t slen;
    mach_port_t masterPort;
    kern_return_t kret;
    
    CFDataRef xmlData;
    CFMutableDictionaryRef dict;
    CFMutableArrayRef array;

    const UInt8 *xmlBuffer;
    UInt8 *outBuffer;
    CFIndex count;
    
    CFStringRef pathString;
    
    kret = IOMasterPort(MACH_PORT_NULL, &masterPort);
    if(kret) return 1;
        
    if(NULL == realpath(path, fullpath)) {
        
        contextprintf(context, kBLLogLevelError, "Can't resolve full path for %s\n",
                   path);
        return 1;
    }
    
    ret = blsustatfs(fullpath, &sb);
    if(ret) {
        contextprintf(context, kBLLogLevelError, "Can't statfs %s\n",
                      fullpath);
        return 2;
    }
    
    if(0 != strncmp(fullpath, sb.f_mntonname, strlen(sb.f_mntonname))) {
        return 3;
    }
    
    // if fullpath was actually the path to the mountpoint,
    // don't add a path component to the XML dict
    if(0 != strcmp(fullpath, sb.f_mntonname)) {
    
        if(strlen(sb.f_mntonname) > 1) {
            memmove(fullpath, fullpath+strlen(sb.f_mntonname),
                    strlen(fullpath)-strlen(sb.f_mntonname)+1);
        }
        
        slen = strlen(fullpath);
        for(i=0; i < slen; i++) {
            if(fullpath[i] == '/')
                fullpath[i] = '\\';
        }
        
        pathString = CFStringCreateWithCString(kCFAllocatorDefault, fullpath, kCFStringEncodingUTF8);
        
        contextprintf(context, kBLLogLevelVerbose, "Relative path of %s is %s\n",
                      path, fullpath);
                
    } else {
        pathString = NULL;
        
        contextprintf(context, kBLLogLevelVerbose, "Path to mountpoint given: %s\n",
                      fullpath);        
    }
    
    array = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    
    dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                     &kCFTypeDictionaryValueCallBacks);

    ret = addMatchingInfoForBSDName(context, masterPort, dict, sb.f_mntfromname+strlen("/dev/"), shortForm);
    if(ret) {
      CFRelease(dict);
      CFRelease(array);
      return 2;
    }    
    CFArrayAppendValue(array, dict);
    CFRelease(dict);
    
    if(pathString) {
        dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                         &kCFTypeDictionaryValueCallBacks);
        CFDictionaryAddValue(dict, CFSTR("IOEFIDevicePathType"),
                             CFSTR("MediaFilePath"));
        CFDictionaryAddValue(dict, CFSTR("Path"),
                             pathString);
        CFArrayAppendValue(array, dict);
        CFRelease(dict);
        
        CFRelease(pathString);
    }
    
    if(optionalData) {
        CFStringRef optString = CFStringCreateWithCString(kCFAllocatorDefault, optionalData, kCFStringEncodingUTF8);

        dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                         &kCFTypeDictionaryValueCallBacks);
        CFDictionaryAddValue(dict, CFSTR("IOEFIBootOption"),
                             optString);
        CFArrayAppendValue(array, dict);
        CFRelease(dict);        
        
        CFRelease(optString);
    }
    
    xmlData = IOCFSerialize(array, 0);
    CFRelease(array);
    
    if(xmlData == NULL) {
        contextprintf(context, kBLLogLevelError, "Can't create XML representation\n");
        return 2;
    }
    
    count = CFDataGetLength(xmlData);
    xmlBuffer = CFDataGetBytePtr(xmlData);
    outBuffer = calloc(count+1, sizeof(char)); // terminate
    
    memcpy(outBuffer, xmlBuffer, count);
    CFRelease(xmlData);
    
    *xmlString = CFStringCreateWithCString(kCFAllocatorDefault, (const char *)outBuffer, kCFStringEncodingUTF8);

    free(outBuffer);
    
    return 0;
}



int BLCreateEFIXMLRepresentationForPartialPath(BLContextPtr context,
                                               const char *bsdName,
                                               const char *path,
                                               const char *optionalData,
                                               CFStringRef *xmlString,
                                               bool shortForm)
{
    int ret;
    mach_port_t masterPort;
    kern_return_t kret;
    int i;
    size_t slen;
    char    newPath[MAXPATHLEN];
    
    CFDataRef xmlData;
    CFMutableDictionaryRef dict;
    CFMutableArrayRef array;
    
    const UInt8 *xmlBuffer;
    UInt8 *outBuffer;
    CFIndex count;
    
    CFStringRef pathString;
    
    kret = IOMasterPort(MACH_PORT_NULL, &masterPort);
    if (kret) return 1;
    
    strlcpy(newPath, path, sizeof newPath);
    slen = strlen(newPath);
    for (i=0; i < slen; i++) {
        if (newPath[i] == '/')
            newPath[i] = '\\';
    }
    pathString = CFStringCreateWithCString(kCFAllocatorDefault, newPath, kCFStringEncodingUTF8);

    array = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    
    dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                     &kCFTypeDictionaryValueCallBacks);
    
    ret = addMatchingInfoForBSDName(context, masterPort, dict, bsdName, shortForm);
    if (ret) {
        CFRelease(dict);
        CFRelease(array);
        return 2;
    }
    CFArrayAppendValue(array, dict);
    CFRelease(dict);
    
    if (pathString) {
        dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                         &kCFTypeDictionaryValueCallBacks);
        CFDictionaryAddValue(dict, CFSTR("IOEFIDevicePathType"),
                             CFSTR("MediaFilePath"));
        CFDictionaryAddValue(dict, CFSTR("Path"),
                             pathString);
        CFArrayAppendValue(array, dict);
        CFRelease(dict);
        
        CFRelease(pathString);
    }
    
    if (optionalData) {
        CFStringRef optString = CFStringCreateWithCString(kCFAllocatorDefault, optionalData, kCFStringEncodingUTF8);
        
        dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                         &kCFTypeDictionaryValueCallBacks);
        CFDictionaryAddValue(dict, CFSTR("IOEFIBootOption"),
                             optString);
        CFArrayAppendValue(array, dict);
        CFRelease(dict);
        
        CFRelease(optString);
    }
    
    xmlData = IOCFSerialize(array, 0);
    CFRelease(array);
    
    if (xmlData == NULL) {
        contextprintf(context, kBLLogLevelError, "Can't create XML representation\n");
        return 2;
    }
    
    count = CFDataGetLength(xmlData);
    xmlBuffer = CFDataGetBytePtr(xmlData);
    outBuffer = calloc(count+1, sizeof(char)); // terminate
    
    memcpy(outBuffer, xmlBuffer, count);
    CFRelease(xmlData);
    
    *xmlString = CFStringCreateWithCString(kCFAllocatorDefault, (const char *)outBuffer, kCFStringEncodingUTF8);
    
    free(outBuffer);
    
    return 0;
}


// first look up the media object, then create a
// custom matching dictionary that should be persistent
// from boot to boot
int addMatchingInfoForBSDName(BLContextPtr context,
			      mach_port_t masterPort,
			      CFMutableDictionaryRef dict,
			      const char *bsdName,
                  bool shortForm)
{
    io_service_t                media = IO_OBJECT_NULL, checkMedia = IO_OBJECT_NULL;
    CFStringRef                 uuid = NULL;
    CFMutableDictionaryRef      propDict = NULL;
    kern_return_t               kret;
    CFStringRef			lastBSDName = NULL;

    lastBSDName = CFStringCreateWithCString(kCFAllocatorDefault,
					    bsdName,
					    kCFStringEncodingUTF8);

    propDict = IOBSDNameMatching(masterPort, 0, bsdName);
    CFDictionarySetValue(propDict, CFSTR(kIOProviderClassKey), CFSTR(kIOMediaClass));
    
    media = IOServiceGetMatchingService(masterPort,
                                        propDict);
    propDict = NULL;
    
    if(media == IO_OBJECT_NULL) {
        contextprintf(context, kBLLogLevelError, "Could not find object for %s\n", bsdName);
        CFRelease(lastBSDName);
        return 1;
    }
    
    uuid = IORegistryEntryCreateCFProperty(media, CFSTR(kIOMediaUUIDKey),
                                           kCFAllocatorDefault, 0);
    if(uuid == NULL) {
        CFUUIDRef       fsuuid = NULL;
		CFStringRef     fsuuidstr = NULL;        
		io_string_t path;
#if USE_DISKARBITRATION
        DASessionRef    session = NULL;
        DADiskRef       dadisk = NULL;
		
        contextprintf(context, kBLLogLevelVerbose, "IOMedia %s does not have a partition %s\n",
                      bsdName, kIOMediaUUIDKey);
		
        session = DASessionCreate(kCFAllocatorDefault);
        if(session) {
            dadisk = DADiskCreateFromIOMedia(kCFAllocatorDefault, session, 
                                             media);
            if(dadisk) {
                CFDictionaryRef descrip = DADiskCopyDescription(dadisk);
                if(descrip) {
                    fsuuid = CFDictionaryGetValue(descrip, kDADiskDescriptionVolumeUUIDKey);
                    
                    if(fsuuid)
                        CFRetain(fsuuid);
                    CFRelease(descrip);
                }
                
                CFRelease(dadisk);
            }
            
            CFRelease(session);
        }
#endif // USE_DISKARBITRATION
		
        if(fsuuid) {
            char        fsuuidCString[64];
			
            fsuuidstr = CFUUIDCreateString(kCFAllocatorDefault, fsuuid);
            
            CFStringGetCString(fsuuidstr,fsuuidCString,sizeof(fsuuidCString),kCFStringEncodingUTF8);
            
            contextprintf(context, kBLLogLevelVerbose, "DADiskRef %s has Volume UUID %s\n",
                          bsdName, fsuuidCString);
            
            CFRelease(fsuuid);
		} else {
            contextprintf(context, kBLLogLevelVerbose, "IOMedia %s does not have a Volume UUID\n",
                          bsdName);
		}
		
		
		// we have a volume UUID, but our primary matching mechanism will be the device path
		
		kret = IORegistryEntryGetPath(media, kIODeviceTreePlane,path);
		if(kret) {
			contextprintf(context, kBLLogLevelVerbose, "IOMedia %s does not have device tree path\n",
						  bsdName);
			
			propDict = IOServiceMatching(kIOMediaClass);
			CFDictionaryAddValue(propDict,  CFSTR(kIOBSDNameKey), lastBSDName);
			
			// add UUID as hint
			if(fsuuidstr)
				CFDictionaryAddValue(dict, CFSTR("BLVolumeUUID"), fsuuidstr);
			
		} else {
			CFStringRef blpath = CFStringCreateWithCString(kCFAllocatorDefault, path, kCFStringEncodingUTF8);
			
			contextprintf(context, kBLLogLevelVerbose, "IOMedia %s has path %s\n",
						  bsdName, path);
			
			propDict = IOServiceMatching(kIOMediaClass);
			CFDictionaryAddValue(propDict, CFSTR(kIOPathMatchKey), blpath);
			CFRelease(blpath);
			
			// add UUID as hint
			if(fsuuidstr)
				CFDictionaryAddValue(dict, CFSTR("BLVolumeUUID"), fsuuidstr);
			
			CFDictionaryAddValue(dict, CFSTR("BLLastBSDName"), lastBSDName);
		}
		
		if(fsuuidstr) {
			CFRelease(fsuuidstr);
		}
		
    } else {
      CFMutableDictionaryRef propMatch;

        contextprintf(context, kBLLogLevelVerbose, "IOMedia %s has UUID %s\n",
                      bsdName, BLGetCStringDescription(uuid));

        propMatch = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                              &kCFTypeDictionaryKeyCallBacks,
                              &kCFTypeDictionaryValueCallBacks);
        CFDictionaryAddValue(propMatch, CFSTR(kIOMediaUUIDKey), uuid);

        propDict = IOServiceMatching(kIOMediaClass);
        CFDictionaryAddValue(propDict,  CFSTR(kIOPropertyMatchKey), propMatch);
        CFRelease(propMatch);

        // add a hint to the top-level dict
        CFDictionaryAddValue(dict, CFSTR("BLLastBSDName"), lastBSDName);

        CFRelease(uuid);
    }

    // verify the dictionary matches
    CFRetain(propDict); // consumed below
    checkMedia = IOServiceGetMatchingService(masterPort,
					     propDict);
    
    if(IO_OBJECT_NULL == checkMedia
       || !IOObjectIsEqualTo(media, checkMedia)) {
      contextprintf(context, kBLLogLevelVerbose, "Inconsistent registry entries for %s\n",
		    bsdName);
      
      if(IO_OBJECT_NULL != checkMedia) IOObjectRelease(checkMedia);
      IOObjectRelease(media);
      CFRelease(lastBSDName);
      CFRelease(propDict);
      
      return 2;
    }
    
    IOObjectRelease(checkMedia);
    IOObjectRelease(media);

    CFDictionaryAddValue(dict, CFSTR("IOMatch"), propDict);        
    CFRelease(lastBSDName);
    CFRelease(propDict);

    if(shortForm) {
        CFDictionaryAddValue(dict, CFSTR("IOEFIShortForm"), kCFBooleanTrue);
    }
    
    return 0;
}
