/*
 * Copyright (c) 2010 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <CoreFoundation/CoreFoundation.h>

#include "UserEventAgentInterface.h"

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <asl.h>

#include "kcm.h"

typedef struct {
    UserEventAgentInterfaceStruct *agentInterface;
    CFUUIDRef factoryID;
    UInt32 refCount;

    CFRunLoopRef rl;

    CFNotificationCenterRef darwin;
    CFNotificationCenterRef distributed;

} GSSNotificationForwarder;

static void
GSSNotificationForwarderDelete(GSSNotificationForwarder *instance) {

    CFUUIDRef factoryID = instance->factoryID;

    asl_log(NULL, NULL, ASL_LEVEL_DEBUG, "UserEventAgentFactory: %s", __func__);

    /* XXX */

    if (instance->darwin)
	CFRelease(instance->darwin);
    if (instance->distributed)
	CFRelease(instance->distributed);
    if (instance->rl)
	CFRelease(instance->rl);

    if (factoryID) {
	CFPlugInRemoveInstanceForFactory(factoryID);
	CFRelease(factoryID);
    }
    free(instance);
}


static void
cc_changed(CFNotificationCenterRef center,
	   void *observer,
	   CFStringRef name,
	   const void *object,
	   CFDictionaryRef userInfo)
{
    GSSNotificationForwarder *instance = observer;

    CFNotificationCenterPostNotification(instance->distributed,
					 CFSTR(kCCAPICacheCollectionChangedNotification),
					 NULL,
					 NULL,
					 false);
}

static void
GSSNotificationForwarderInstall(void *pinstance)
{
    GSSNotificationForwarder *instance = pinstance;

    asl_log(NULL, NULL, ASL_LEVEL_DEBUG, "UserEventAgentFactory: %s", __func__);

    instance->darwin = CFNotificationCenterGetDarwinNotifyCenter();
    instance->distributed = CFNotificationCenterGetDistributedCenter();

    CFNotificationCenterAddObserver(instance->darwin,
				    instance,
				    cc_changed,
				    CFSTR(KRB5_KCM_NOTIFY_CACHE_CHANGED),
				    NULL,
				    CFNotificationSuspensionBehaviorHold);

}


static HRESULT
GSSNotificationForwarderQueryInterface(void *pinstance, REFIID iid, LPVOID *ppv)
{
    CFUUIDRef interfaceID = CFUUIDCreateFromUUIDBytes(NULL, iid);
    GSSNotificationForwarder *instance = pinstance;
        
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
GSSNotificationForwarderAddRef(void *pinstance) 
{
    GSSNotificationForwarder *instance = pinstance;
    return ++instance->refCount;
}

static ULONG
GSSNotificationForwarderRelease(void *pinstance) 
{
    GSSNotificationForwarder *instance = pinstance;
    if (instance->refCount == 1) {
	GSSNotificationForwarderDelete(instance);
	return 0;
    }
    return --instance->refCount;
}



static UserEventAgentInterfaceStruct UserEventAgentInterfaceFtbl = {
    NULL,
    GSSNotificationForwarderQueryInterface,
    GSSNotificationForwarderAddRef,
    GSSNotificationForwarderRelease,
    GSSNotificationForwarderInstall
}; 


static GSSNotificationForwarder *
GSSNotificationForwarderCreate(CFAllocatorRef allocator, CFUUIDRef factoryID)
{
    GSSNotificationForwarder *instance;

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
UserEventAgentFactory(CFAllocatorRef allocator, CFUUIDRef typeID);


void *
UserEventAgentFactory(CFAllocatorRef allocator, CFUUIDRef typeID)
{
    asl_log(NULL, NULL, ASL_LEVEL_DEBUG, "UserEventAgentFactory: %s", __func__);

    if (CFEqual(typeID, kUserEventAgentTypeID))
	return GSSNotificationForwarderCreate(allocator, kUserEventAgentFactoryID);

    return NULL;
}
