/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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


#ifndef _IOKIT_IOFRAMEBUFFERUSERCLIENT_H
#define _IOKIT_IOFRAMEBUFFERUSERCLIENT_H

#include <IOKit/IOUserClient.h>
#include <IOKit/graphics/IOFramebuffer.h>
#include <IOKit/pci/IOAGPDevice.h>

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class IOFramebufferUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(IOFramebufferUserClient)

public:
    IOFramebuffer *	owner;
    IOFramebuffer *	other;

    // IOUserClient methods
    virtual IOReturn clientClose( void );

    virtual IOService * getService( void );

    virtual IOReturn clientMemoryForType( UInt32 type,
        IOOptionBits * options, IOMemoryDescriptor ** memory );

    virtual IOReturn externalMethod( uint32_t selector, IOExternalMethodArguments * args,
					IOExternalMethodDispatch * dispatch, OSObject * target, void * reference );

    virtual IOReturn registerNotificationPort( mach_port_t, UInt32, UInt32 );
    virtual IOReturn getNotificationSemaphore( UInt32 interruptType,
                                               semaphore_t * semaphore );

    virtual IOReturn connectClient( IOUserClient * other );

    // others

    static IOFramebufferUserClient * withTask( task_t owningTask );

    virtual bool start( IOService * provider );
    virtual IOReturn setProperties( OSObject * properties );
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class IOFramebufferSharedUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(IOFramebufferSharedUserClient)

private:

    IOFramebuffer *	 owner;

public:
    virtual void free();
    virtual void release() const;

    // IOUserClient methods
    virtual IOReturn clientClose( void );

    virtual IOService * getService( void );

    virtual IOReturn clientMemoryForType( UInt32 type,
        IOOptionBits * options, IOMemoryDescriptor ** memory );

    virtual IOReturn getNotificationSemaphore( UInt32 notification_type,
                                    semaphore_t * semaphore );

    // others
    static IOFramebufferSharedUserClient * withTask( task_t owningTask );
    virtual bool start( IOService * provider );
};


#endif /* ! _IOKIT_IOFRAMEBUFFERUSERCLIENT_H */
