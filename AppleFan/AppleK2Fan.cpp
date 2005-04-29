/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2003 Apple Computer, Inc.  All rights reserved.
 *
 */
#include <IOKit/IOMessage.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOTimerEventSource.h>
#include "AppleK2Fan.h"

OSDefineMetaClassAndStructors(AppleK2Fan, IOService)

void AppleK2Fan::timerEventOccurred(OSObject *obj, IOTimerEventSource *timer)
{
    // Get tach count, fan is stopped if the count is less than 10
    UInt32 tach, count;
    AppleK2Fan *me = (AppleK2Fan *)obj;

    me->callPlatformFunction(me->fGetTacho, false, &tach, NULL, NULL, NULL);
    if(tach < me->fLastCount)
        count = 65536 + tach - me->fLastCount;
    else
        count = tach - me->fLastCount;
    me->fLastCount = tach;
    //IOLog("Tach: %ld\n", count);
    if(count == 0) {
        OSDictionary *dict;
        IOLog("Fan failure!\n");
        dict = me->dictionaryWithProperties();
        if(dict) {
            me->messageClient(kIOPPluginMessageError, me->fPlatformPlugin, dict);
            dict->release();
        }
    }
    timer->setTimeoutMS(me->fPollingMS);
}

IOService * AppleK2Fan::probe(IOService *provider, SInt32 *score)
{
    // Check for hwctrl-params-version = 1
    OSData *version;
    version = OSDynamicCast(OSData, provider->getProperty("hwctrl-params-version"));
    if(version && (*(UInt32 *)(version->getBytesNoCopy()) == 1))
        return this;
    else
        return NULL;
}
    
bool AppleK2Fan::start(IOService *provider)
{
    OSData *obj;
    IOService *ior_prt; 
    mach_timespec_t t;
    OSDictionary *dict;
    OSReturn res;
    
	// We have two power states - off and on
	static const IOPMPowerState powerStates[2] = {
        { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 1, IOPMDeviceUsable, IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 }
    };
    
    if(!IOService::start(provider))
        return false;

	// get a pointer to the Platform Plugin
    t.tv_sec = 30;		// wait for IOPlatformPlugin for up to 30 seconds 
    t.tv_nsec = 0;
    
    ior_prt = waitForService(resourceMatching("IOPlatformPlugin"), &t); 

    if(!ior_prt) {
        IOLog("AppleK2Fan::start ERROR: couldn't find IOPlatformPlugin\n");
        return false;
    }
    else {
        fPlatformPlugin = (IOService*)(ior_prt->getProperty("IOPlatformPlugin"));
    }
    
    obj = OSDynamicCast(OSData, provider->getProperty("hwctrl-id"));
    if(obj) {
        UInt32 val = *(UInt32 *)obj->getBytesNoCopy();
        setProperty("control-id", val, 32);
    }
    obj = OSDynamicCast(OSData, provider->getProperty("hwctrl-location"));
    if(obj) {
        char * val = (char *)obj->getBytesNoCopy();
        setProperty("location", val);
    }
    obj = OSDynamicCast(OSData, provider->getProperty("hwctrl-type"));
    if(obj) {
        char * val = (char *)obj->getBytesNoCopy();
        setProperty("type", val);
    }
    obj = OSDynamicCast(OSData, provider->getProperty("hwctrl-zone"));
    if(obj)
        setProperty("zone", obj);
    obj = OSDynamicCast(OSData, provider->getProperty("hwctrl-params-version"));
    if(obj) {
        UInt32 val = *(UInt32 *)obj->getBytesNoCopy();
        setProperty("version", val, 32);
    }
    setProperty("current-value", 42, 32);
    setProperty("target-value", 42, 32);
    
    dict = dictionaryWithProperties();
    if(dict) {
        res = messageClient(kIOPPluginMessageRegister, fPlatformPlugin, dict);
        dict->release();
        // If messageClient returns an error give up now.
        if(res != kIOReturnSuccess) {
            IOLog("AppleK2Fan::start, messageClient returned 0x%x\n", res);
            return false;
        }
    }
    
    char callName[128];
    OSData * data;
    UInt32 handle;
    data = OSDynamicCast(OSData, provider->getProperty( "AAPL,phandle" ));
    handle = *((UInt32 *) data->getBytesNoCopy());
    sprintf(callName,"%s-%8lx", "platform-getTACHCount", handle);
    fGetTacho = OSSymbol::withCString(callName);
    
	fPollingMS = 1000;	// Once a second
    fTimer = IOTimerEventSource::timerEventSource(this, timerEventOccurred);
    getWorkLoop()->addEventSource(fTimer);
    fTimer->enable();
    fTimer->setTimeoutMS(fPollingMS);
     
	// register interest in power state changes
	PMinit();
	provider->joinPMtree(this);
	registerPowerDriver(this, (IOPMPowerState *)powerStates, 2);
    return true;
}

IOReturn AppleK2Fan::setPowerState(unsigned long powerStateOrdinal, IOService *whatDevice)
{
	if (powerStateOrdinal == 1) {
        fTimer->setTimeoutMS(fPollingMS);
	}
	else if (powerStateOrdinal == 0) {
		fTimer->cancelTimeout();
	}

	return(IOPMAckImplied);
}

