/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
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

    virtual IOExternalMethod * getTargetAndMethodForIndex(
                                        IOService ** targetP, UInt32 index );

    virtual IOReturn registerNotificationPort( mach_port_t, UInt32, UInt32 );
    virtual IOReturn getNotificationSemaphore( UInt32 interruptType,
                                               semaphore_t * semaphore );

    virtual IOReturn connectClient( IOUserClient * other );

    // others

    static IOFramebufferUserClient * withTask( task_t owningTask );

    virtual bool start( IOService * provider );
    virtual IOReturn setProperties( OSObject * properties );

    virtual IOReturn getAttribute( IOSelect attribute, UInt32 * value );
    virtual IOReturn setAttribute( IOSelect attribute, UInt32 value );
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class IOFramebufferSharedUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(IOFramebufferSharedUserClient)

private:

    IOFramebuffer *	owner;

    IOExternalMethod externals[ 0 ];

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
    
    virtual IOExternalMethod * getExternalMethodForIndex( UInt32 index );

    // others
    static IOFramebufferSharedUserClient * withTask( task_t owningTask );
    virtual bool start( IOService * provider );
};


#endif /* ! _IOKIT_IOFRAMEBUFFERUSERCLIENT_H */
