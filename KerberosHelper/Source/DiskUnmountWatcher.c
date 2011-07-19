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
#include <DiskArbitration/DiskArbitration.h>
#include <ApplicationServices/ApplicationServicesPriv.h>

#include "UserEventAgentInterface.h"

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <asl.h>

typedef struct {
    UserEventAgentInterfaceStruct *agentInterface;
    CFUUIDRef factoryID;
    UInt32 refCount;

    CFRunLoopRef rl;
    DASessionRef session;
} DiskUnmountWatcher;


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
callback(DADiskRef disk, void *context)
{
    CFStringRef path, ident;
    CFURLRef url;
    CFDictionaryRef dict;
    size_t len;
    char *str, *str2;

    asl_log(NULL, NULL, ASL_LEVEL_DEBUG, "UserEventAgentFactory: %s", __func__);

    dict = DADiskCopyDescription(disk);
    if (dict == NULL)
	return;

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
    asl_log(NULL, NULL, ASL_LEVEL_DEBUG, "UserEventAgentFactory: %s find and release %s", __func__, str2);
    free(str2);
    if (ident) {
	NAHFindByLabelAndRelease(ident);
	CFRelease(ident);
    }
 out:
    CFRelease(dict);
}


static void
DiskUnmountWatcherDelete(DiskUnmountWatcher *instance) {

    CFUUIDRef factoryID = instance->factoryID;

    asl_log(NULL, NULL, ASL_LEVEL_DEBUG, "UserEventAgentFactory: %s", __func__);

    DASessionUnscheduleFromRunLoop(instance->session, instance->rl, kCFRunLoopDefaultMode);

    CFRelease(instance->session);
    CFRelease(instance->rl);

    if (factoryID) {
	CFPlugInRemoveInstanceForFactory(factoryID);
	CFRelease(factoryID);
    }
    free(instance);
}


static void
DiskUnmountWatcherInstall(void *pinstance)
{
    DiskUnmountWatcher *instance = pinstance;

    asl_log(NULL, NULL, ASL_LEVEL_DEBUG, "UserEventAgentFactory: %s", __func__);

    instance->session = DASessionCreate(kCFAllocatorDefault);

    DARegisterDiskDisappearedCallback(instance->session, NULL, callback, NULL);
    DASessionScheduleWithRunLoop(instance->session, instance->rl, kCFRunLoopDefaultMode);
}


static HRESULT
DiskUnmountWatcherQueryInterface(void *pinstance, REFIID iid, LPVOID *ppv)
{
    CFUUIDRef interfaceID = CFUUIDCreateFromUUIDBytes(NULL, iid);
    DiskUnmountWatcher *instance = pinstance;
        
    asl_log(NULL, NULL, ASL_LEVEL_DEBUG, "UserEventAgentFactory: %s", __func__);

    if (CFEqual(interfaceID, kUserEventAgentInterfaceID) || CFEqual(interfaceID, IUnknownUUID)) {
	instance->agentInterface->AddRef(instance);
	*ppv = instance;
	CFRelease(interfaceID);
	return S_OK;
    }

    *ppv = NULL;
    CFRelease(interfaceID);
    return E_NOINTERFACE;
}

static ULONG
DiskUnmountWatcherAddRef(void *pinstance) 
{
    DiskUnmountWatcher *instance = pinstance;
    return ++instance->refCount;
}

static ULONG
DiskUnmountWatcherRelease(void *pinstance) 
{
    DiskUnmountWatcher *instance = pinstance;
    if (instance->refCount == 1) {
	DiskUnmountWatcherDelete(instance);
	return 0;
    }
    return --instance->refCount;
}



static UserEventAgentInterfaceStruct UserEventAgentInterfaceFtbl = {
    NULL,
    DiskUnmountWatcherQueryInterface,
    DiskUnmountWatcherAddRef,
    DiskUnmountWatcherRelease,
    DiskUnmountWatcherInstall
}; 


static DiskUnmountWatcher *
DiskUnmountWatcherCreate(CFAllocatorRef allocator, CFUUIDRef factoryID)
{
    DiskUnmountWatcher *instance;

    asl_log(NULL, NULL, ASL_LEVEL_DEBUG, "UserEventAgentFactory: %s", __func__);

    instance = calloc(1, sizeof(*instance));
    if (instance == NULL)
	return NULL;

    instance->agentInterface = &UserEventAgentInterfaceFtbl;
    if (factoryID) {
	instance->factoryID = (CFUUIDRef)CFRetain(factoryID);
	CFPlugInAddInstanceForFactory(factoryID);
    }

    instance->rl = CFRunLoopGetCurrent();
    CFRetain(instance->rl);

    instance->refCount = 1;
    return instance;
}


void *
UserEventAgentFactory(CFAllocatorRef allocator, CFUUIDRef typeID)
{
    asl_log(NULL, NULL, ASL_LEVEL_DEBUG, "UserEventAgentFactory: %s", __func__);

    if (CFEqual(typeID, kUserEventAgentTypeID))
	return DiskUnmountWatcherCreate(allocator, kUserEventAgentFactoryID);

    return NULL;
}
