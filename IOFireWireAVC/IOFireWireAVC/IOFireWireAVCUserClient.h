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

#ifndef _IOKIT_IOFIREWIREAVCUSERCLIENT_H
#define _IOKIT_IOFIREWIREAVCUSERCLIENT_H

#include <IOKit/IOUserClient.h>

//#include <IOKit/firewire/IOFireWireController.h>

#include "IOFireWireAVCUnit.h"
#include "IOFireWireAVCUserClientCommon.h"

class IOFireWireAVCUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(IOFireWireAVCUserClient)

protected:

    static IOExternalMethod 		sMethods[kIOFWAVCUserClientNumCommands];
    //static IOExternalAsyncMethod 	sAsyncMethods[kIOFWSBP2UserClientNumAsyncCommands];

    bool					fOpened;
	bool					fStarted;
    task_t					fTask;
	
    IOFireWireAVCNub *		fUnit;
	
    virtual IOExternalMethod * getTargetAndMethodForIndex(IOService **target, UInt32 index);
    virtual IOExternalAsyncMethod * getAsyncTargetAndMethodForIndex(IOService **target, UInt32 index);

public:

    static IOFireWireAVCUserClient* withTask( task_t owningTask );

    virtual bool init( OSDictionary * dictionary = 0 );
    virtual bool start( IOService * provider );

    virtual IOReturn clientClose( void );
    virtual IOReturn clientDied( void );

    virtual IOReturn open( void *, void *, void *, void *, void *, void * );
	virtual IOReturn openWithSessionRef( IOFireWireSessionRef sessionRef, void *, void *, void *, void *, void * );
	virtual IOReturn getSessionRef( IOFireWireSessionRef * sessionRef, void *, void *, void *, void *, void * );
    virtual IOReturn close( void * = 0, void * = 0, void * = 0, void * = 0, void * = 0, void * = 0);

    virtual IOReturn AVCCommand(UInt8 * cmd, UInt8 * response, UInt32 len, UInt32 *size);
    virtual IOReturn AVCCommandInGen(UInt8 * cmd, UInt8 * response, UInt32 len, UInt32 *size);

    virtual IOReturn updateAVCCommandTimeout( void * = 0, void * = 0, void * = 0, void * = 0, void * = 0, void * = 0);
};

#endif // _IOKIT_IOFIREWIREAVCUSERCLIENT_H