/*
 * Copyright (c) 2001-2016 Apple Inc. All Rights Reserved.
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
 *  BLHandleAPFSBlessData.c
 *  bless
 *
 *  Created by Jon Becker on 9/21/16.
 *  Copyright (c) 2001-2016 Apple Inc. All Rights Reserved.
 *
 */
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <apfs/apfs_fsctl.h>

#include "bless.h"
#include "bless_private.h"


int BLGetAPFSBlessData(BLContextPtr context, const char *mountpoint, uint64_t *words)
{
    int ret = 0;
    
    if (fsctl(mountpoint, APFSIOC_GET_BOOTINFO, words, 0) < 0) {
        if (errno == ENOENT) {
            words[0] = words[1] = 0;
        } else {
            ret = errno;
        }
    }
    return ret;
}


int BLSetAPFSBlessData(BLContextPtr context, const char *mountpoint, uint64_t *words)
{
    return fsctl(mountpoint, APFSIOC_SET_BOOTINFO, words, 0) < 0 ? errno : 0;
}



int BLCreateAPFSVolumeInformationDictionary(BLContextPtr context, const char *mountpoint, CFDictionaryRef *outDict)
{
    uint64_t blessWords[2];
    int err;
    uint32_t i;
    uint64_t dirID;
    CFMutableDictionaryRef dict = NULL;
    CFMutableArrayRef infarray = NULL;
    
    char blesspath[MAXPATHLEN];
    
    err = BLGetAPFSBlessData(context, mountpoint, blessWords);
    if (err) {
        return 1;
    }
    
    infarray = CFArrayCreateMutable(kCFAllocatorDefault, 2, &kCFTypeArrayCallBacks);
    
    dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    
    for (i = 0; i < 2; i++) {
        CFMutableDictionaryRef word =
        CFDictionaryCreateMutable(kCFAllocatorDefault,0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFTypeRef val;
        
        dirID = blessWords[i];
        blesspath[0] = '\0';
        
        err = BLLookupFileIDOnMount64(context, mountpoint, dirID, blesspath, sizeof blesspath);
        
        val = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &dirID);
        CFDictionaryAddValue(word, CFSTR("Directory ID"), val);
        CFRelease(val); val = NULL;
        
        val = CFStringCreateWithCString(kCFAllocatorDefault, blesspath, kCFStringEncodingUTF8);
        CFDictionaryAddValue(word, CFSTR("Path"), val);
        CFRelease(val); val = NULL;
        
        if (strlen(blesspath) == 0 || 0 == strcmp(mountpoint, "/")) {
            val = CFStringCreateWithCString(kCFAllocatorDefault, blesspath, kCFStringEncodingUTF8);
        } else {
            val = CFStringCreateWithCString(kCFAllocatorDefault, blesspath+strlen(mountpoint), kCFStringEncodingUTF8);
        }
        CFDictionaryAddValue(word, CFSTR("Relative Path"), val);
        CFRelease(val); val = NULL;
        
        CFArrayAppendValue(infarray, word);
        CFRelease(word); word = NULL;
    }
    
    CFDictionaryAddValue(dict, CFSTR("Bless Info"),
                         infarray);
    
    CFRelease(infarray); infarray = NULL;
    
    *outDict = dict;
    return 0;
}
