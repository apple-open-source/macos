/*
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 * Copyright (c) 1997 Apple Computer, Inc.
 *
 *
 * HISTORY
 *
 */


#ifndef _IOKIT_IOHIDUSERCLIENT_H
#define _IOKIT_IOHIDUSERCLIENT_H

#include <libkern/c++/OSContainers.h>
#include <IOKit/IOUserClient.h>
#include "IOHIDSystem.h"
#include "IOHIDEventServiceQueue.h"

#define MAX_SCREENS 64  // same as EV_MAX_SCREENS in HIDSystem

class IOHIDUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(IOHIDUserClient)

private:

    IOHIDSystem     *owner;
    int             _screenTokens[MAX_SCREENS];

public:
    // IOUserClient methods
    virtual IOReturn clientClose( void ) APPLE_KEXT_OVERRIDE;

    virtual IOService * getService( void ) APPLE_KEXT_OVERRIDE;

    virtual IOReturn registerNotificationPort(
		mach_port_t port, UInt32 type, UInt32 refCon ) APPLE_KEXT_OVERRIDE;

    virtual IOReturn connectClient( IOUserClient * client ) APPLE_KEXT_OVERRIDE;

    virtual IOReturn clientMemoryForType( UInt32 type,
        UInt32 * flags, IOMemoryDescriptor ** memory ) APPLE_KEXT_OVERRIDE;

    virtual IOExternalMethod * getTargetAndMethodForIndex(
                        IOService ** targetP, UInt32 index ) APPLE_KEXT_OVERRIDE;

    // others
    virtual bool initWithTask(task_t owningTask, void * /* security_id */, UInt32 /* type */);
    virtual bool start( IOService * provider ) APPLE_KEXT_OVERRIDE;
    virtual void stop( IOService * provider ) APPLE_KEXT_OVERRIDE;
    virtual IOReturn close( void );
    
    virtual IOReturn setProperties( OSObject * properties ) APPLE_KEXT_OVERRIDE;
    IOReturn extGetUserHidActivityState(void*,void*,void*,void*,void*,void*);
};


class IOHIDParamUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(IOHIDParamUserClient)

private:

    IOHIDSystem     *owner;
    
public:

    // IOUserClient methods    
    virtual IOService * getService( void ) APPLE_KEXT_OVERRIDE;

    virtual IOExternalMethod * getTargetAndMethodForIndex(
                        IOService ** targetP, UInt32 index ) APPLE_KEXT_OVERRIDE;

    // others

    virtual bool start( IOService * provider ) APPLE_KEXT_OVERRIDE;
    virtual IOReturn setProperties( OSObject * properties ) APPLE_KEXT_OVERRIDE;

    IOReturn extGetUserHidActivityState(void*,void*,void*,void*,void*,void*);
private:
    virtual IOReturn clientClose(void) APPLE_KEXT_OVERRIDE;
    virtual IOReturn extPostEvent(void*,void*,void*,void*,void*,void*);
};

class IOHIDEventSystemUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(IOHIDEventSystemUserClient)

private:
    IOHIDSystem *               owner;
    IOHIDEventServiceQueue *    kernelQueue;
    IOCommandGate *             commandGate;
    mach_port_t                 _port;
    
    IOReturn registerNotificationPortGated(mach_port_t port, UInt32 type, UInt32 refCon);

public:
    virtual bool initWithTask(task_t owningTask, void * security_id, UInt32 type ) APPLE_KEXT_OVERRIDE;
    void free(void) APPLE_KEXT_OVERRIDE;

    // IOUserClient methods    
    virtual IOReturn clientClose( void ) APPLE_KEXT_OVERRIDE;

    virtual IOExternalMethod * getTargetAndMethodForIndex(IOService ** targetP, UInt32 index ) APPLE_KEXT_OVERRIDE;
    virtual IOReturn createEventQueue(void*,void*,void*,void*,void*,void*);
    virtual IOReturn createEventQueueGated(void*p1,void*p2,void*p3, void*);
    virtual IOReturn destroyEventQueue(void*,void*,void*,void*,void*,void*);
    virtual IOReturn destroyEventQueueGated(void*,void*,void*,void*);
    virtual IOReturn tickle(void*,void*,void*,void*,void*,void*);

    virtual IOReturn registerNotificationPort(mach_port_t port, UInt32 type, UInt32 refCon ) APPLE_KEXT_OVERRIDE;
    virtual IOReturn clientMemoryForType( UInt32 type, UInt32 * flags, IOMemoryDescriptor ** memory ) APPLE_KEXT_OVERRIDE;
    IOReturn clientMemoryForTypeGated( UInt32 type, UInt32 * flags, IOMemoryDescriptor ** memory );

    virtual IOService * getService( void ) APPLE_KEXT_OVERRIDE;

    virtual bool start( IOService * provider ) APPLE_KEXT_OVERRIDE;
    virtual void stop ( IOService * provider ) APPLE_KEXT_OVERRIDE;
};



#endif /* ! _IOKIT_IOHIDUSERCLIENT_H */
