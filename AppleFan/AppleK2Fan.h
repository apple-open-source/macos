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
#include <IOKit/IOService.h>

/* platform plugin message types */
enum {
	kIOPPluginMessageRegister			= 1,
	kIOPPluginMessageUnregister			= 2,
	kIOPPluginMessageLowThresholdHit	= 3,
	kIOPPluginMessageHighThresholdHit	= 4,
	kIOPPluginMessageCurrentValue		= 5,
	kIOPPluginMessageStateChanged		= 6,
	kIOPPluginMessagePowerMonitor		= 7,
	kIOPPluginMessageError				= 8,
	kIOPPluginMessageGetPlatformID		= 9
};

class AppleK2Fan : public IOService
{
    OSDeclareDefaultStructors(AppleK2Fan)

protected:
    const OSSymbol *fGetTacho;
    UInt32	fLastCount;
    UInt32	fPollingMS;		// polling period in milliseconds
    IOTimerEventSource *fTimer;
    OSDictionary *fControlDict;
    IOService *fPlatformPlugin;

    static void timerEventOccurred(OSObject *obj,
				IOTimerEventSource *sender);

public:
    virtual IOService *probe(IOService *provider, SInt32 *score);
    virtual bool start(IOService *provider);
    virtual IOReturn setPowerState(unsigned long powerStateOrdinal,
				IOService *whatDevice);
};