/*
 * Copyright (c) 2006-2007 Apple Inc. All rights reserved.
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

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFXPCBridge.h>
#include <DiskArbitration/DiskArbitration.h>

#include "NetworkAuthenticationHelper.h"

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <os/log.h>

#include <xpc/xpc.h>

static char *
cf2cstring(CFStringRef inString)
{
    char *string = NULL;
    CFIndex length;
    
    string = (char *) CFStringGetCStringPtr(inString, kCFStringEncodingUTF8);
    if (string)
        return strdup(string);
    
    length = CFStringGetMaximumSizeForEncoding(CFStringGetLength(inString),
                                               kCFStringEncodingUTF8) + 1;
    string = malloc (length);
    if (string == NULL)
        return NULL;
    if (!CFStringGetCString(inString, string, length, kCFStringEncodingUTF8)) {
        free (string);
        return NULL;
    }
    return string;
}

static void
callback(xpc_object_t disk)
{
    CFDictionaryRef dict = NULL;
    CFStringRef path, ident;
    char *str, *str2;
    CFURLRef url;
    size_t len;
    
    os_log(OS_LOG_DEFAULT, "DiskUnmountWatcher: %s", __func__);
    
    xpc_object_t desc = xpc_dictionary_get_value(disk, "Description");
    if (desc == NULL)
        goto out;
    
    dict = _CFXPCCreateCFObjectFromXPCObject(desc);
    if (dict == NULL)
        goto out;
    
    url = (CFURLRef)CFDictionaryGetValue(dict, kDADiskDescriptionVolumePathKey);
    if (url == NULL)
        goto out;
    
    path = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);
    if (path == NULL)
        goto out;
    
    str = cf2cstring(path);
    CFRelease(path);
    if (str == NULL)
        goto out;
    
    /* remove trailing / */
    len = strlen(str);
    if (len > 0 && str[len - 1] == '/')
        len--;
    
    asprintf(&str2, "fs:%.*s", (int)len, str);
    free(str);
    
    ident = CFStringCreateWithCString(NULL, str2, kCFStringEncodingUTF8);
    os_log(OS_LOG_DEFAULT, "DiskUnmountWatcher: %s find and release %s", __func__, str2);
    free(str2);
    if (ident) {
        NAHFindByLabelAndRelease(ident);
        CFRelease(ident);
    }
out:
    if (dict)
        CFRelease(dict);
}

int
main(int argc, char **argv)
{
    os_log(OS_LOG_DEFAULT, "DiskUnmountWatcher: %s", __func__);
    
    xpc_set_event_stream_handler("com.apple.diskarbitration", dispatch_get_main_queue(),
                                 ^(xpc_object_t disk) {
                                     callback(disk);
                                 });
    
    os_log(OS_LOG_DEFAULT, "DiskUnmountWatcher: %s", __func__);

    dispatch_main();
    
    return 0;
}

