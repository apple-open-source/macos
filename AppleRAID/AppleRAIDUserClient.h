/*
 * Copyright (c) 2001-2015 Apple Inc. All rights reserved.
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

#ifndef _APPLERAIDUSERCLIENT_H
#define _APPLERAIDUSERCLIENT_H

// User Client method names    
enum
{
    kAppleRAIDClientOpen = 0,
    kAppleRAIDClientClose,		// 1
    kAppleRAIDGetListOfSets,		// 2
    kAppleRAIDGetSetProperties,		// 3
    kAppleRAIDGetMemberProperties,	// 4
    kAppleRAIDUpdateSet,		// 5
    kAppleLVMGetVolumesForGroup,	// 6
    kAppleLVMGetVolumeProperties,	// 7
    kAppleLVMGetVolumeExtents,		// 8
    kAppleLVMUpdateLogicalVolume,	// 9
    kAppleLVMDestroyLogicalVolume,	// 10
    kAppleRAIDUserClientMethodsCount
};

#define kAppleRAIDMessageSetChanged	('raid')
#define kAppleLVMMessageVolumeChanged	('lvol')

enum {
    kAppleRAIDUUIDStringSize = 36,				// 128bit UUID (in ascii hex plus some dashes) "uuidgen | wc" == 37
    kAppleRAIDMaxUUIDStringSize = 64
};

// update set sub commands
enum {
    kAppleRAIDUpdateResetSet = 1,
    kAppleRAIDUpdateDestroySet = 2
};

#ifdef KERNEL

class AppleRAIDUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(AppleRAIDUserClient)
    
 protected:
    AppleRAID *				ucController;
    task_t				ucClient;
      
public:
       
    virtual bool initWithTask(task_t owningTask, void * security_id, UInt32 type);

    virtual bool start(IOService * provider);

    virtual void stop(IOService * provider);
    
    virtual IOReturn clientClose(void);

    virtual IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments * arguments,
				    IOExternalMethodDispatch * dispatch = 0,
				    OSObject * target = 0, void * reference = 0);

    // IOUserClient shims
    static IOReturn openShim(OSObject * target, void * reference, IOExternalMethodArguments * args)
	{ return ((AppleRAIDUserClient *)target)->openController(); };
    static IOReturn closeShim(OSObject * target, void * reference, IOExternalMethodArguments * args)
	{ return ((AppleRAIDUserClient *)target)->closeController(); };
    static IOReturn getListOfSetsShim(OSObject * target, void * reference, IOExternalMethodArguments * args)
	{ return ((AppleRAIDUserClient *)target)->getListOfSets(args); };
    static IOReturn getSetPropertiesShim(OSObject * target, void * reference, IOExternalMethodArguments * args)
	{ return ((AppleRAIDUserClient *)target)->getSetProperties(args); };
    static IOReturn getMemberPropertiesShim(OSObject * target, void * reference, IOExternalMethodArguments * args)
	{ return ((AppleRAIDUserClient *)target)->getMemberProperties(args); };
    static IOReturn updateSetShim(OSObject * target, void * reference, IOExternalMethodArguments * args)
	{ return ((AppleRAIDUserClient *)target)->updateSet(args); };
    static IOReturn getVolumesForGroupShim(OSObject * target, void * reference, IOExternalMethodArguments * args)
	{ return ((AppleRAIDUserClient *)target)->getVolumesForGroup(args); };
    static IOReturn getVolumePropertiesShim(OSObject * target, void * reference, IOExternalMethodArguments * args)
	{ return ((AppleRAIDUserClient *)target)->getVolumeProperties(args); };
    static IOReturn getVolumeExtentsShim(OSObject * target, void * reference, IOExternalMethodArguments * args)
	{ return ((AppleRAIDUserClient *)target)->getVolumeExtents(args); };
    static IOReturn updateLogicalVolumeShim(OSObject * target, void * reference, IOExternalMethodArguments * args)
	{ return ((AppleRAIDUserClient *)target)->updateLogicalVolume(args); };
    static IOReturn destroyLogicalVolumeShim(OSObject * target, void * reference, IOExternalMethodArguments * args)
	{ return ((AppleRAIDUserClient *)target)->destroyLogicalVolume(args); };

    // IOUserClient methods
    virtual IOReturn openController(void);
    virtual IOReturn closeController(void);
    virtual IOReturn getListOfSets(IOExternalMethodArguments * args);
    virtual IOReturn getSetProperties(IOExternalMethodArguments * args);
    virtual IOReturn getMemberProperties(IOExternalMethodArguments * args);
    virtual IOReturn updateSet(IOExternalMethodArguments * args);
    virtual IOReturn getVolumesForGroup(IOExternalMethodArguments * args);
    virtual IOReturn getVolumeProperties(IOExternalMethodArguments * args);
    virtual IOReturn getVolumeExtents(IOExternalMethodArguments * args);
    virtual IOReturn updateLogicalVolume(IOExternalMethodArguments * args);
    virtual IOReturn destroyLogicalVolume(IOExternalMethodArguments * args);
};

#endif /* KERNEL */

#endif /* _APPLERAIDUSERCLIENT_H */
