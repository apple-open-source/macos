/*
 * Copyright (c) 1998-2001 Apple Computer, Inc. All rights reserved.
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

#ifndef _IOKIT_IOFIREWIREIPUSERCLIENT_H
#define _IOKIT_IOFIREWIREIPUSERCLIENT_H

#include <IOKit/IOUserClient.h>
#include "IOFireWireIP.h"
#include "IOFireWireIPUserClientCommon.h"


class IOFireWireIPUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(IOFireWireIPUserClient)

protected:

    static IOExternalMethod	sMethods[kIOFWIPUserClientNumCommands];
    bool			fOpened;
    bool			fStarted;
    task_t			fTask;
    IOFireWireIP	*fFireWireIP;
	
    virtual IOExternalMethod * getTargetAndMethodForIndex(IOService **target, UInt32 index);
    virtual IOExternalAsyncMethod * getAsyncTargetAndMethodForIndex(IOService **target, UInt32 index);

public:

    static IOFireWireIPUserClient* withTask( task_t owningTask );

    virtual bool init( OSDictionary * dictionary = 0 );
    virtual bool start( IOService * provider );

    virtual IOReturn clientClose( void );
    virtual IOReturn clientDied( void );

    virtual IOReturn open( void *, void *, void *, void *, void *, void * );
    virtual IOReturn openWithSessionRef( IOFireWireSessionRef sessionRef, void *, void *, void *, void *, void * );
    virtual IOReturn getSessionRef( IOFireWireSessionRef * sessionRef, void *, void *, void *, void *, void * );
    virtual IOReturn close( void * = 0, void * = 0, void * = 0, void * = 0, void * = 0, void * = 0);

    virtual IOReturn IPCommand(UInt8 * cmd, UInt8 * response, UInt32 len, UInt32 *size);
    virtual IOReturn showLcb(UInt8 * cmd, UInt8 * response, UInt32 len, UInt32 *size);
	
};

#endif // _IOKIT_IOFIREWIREIPUSERCLIENT_H