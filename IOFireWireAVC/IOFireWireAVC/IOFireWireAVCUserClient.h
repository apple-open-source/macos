/*
 * Copyright (c) 1998-2001 Apple Computer, Inc. All rights reserved.
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

#ifndef _IOKIT_IOFIREWIREAVCUSERCLIENT_H
#define _IOKIT_IOFIREWIREAVCUSERCLIENT_H

#include <IOKit/IOUserClient.h>

//#include <IOKit/firewire/IOFireWireController.h>

#include <IOKit/avc/IOFireWireAVCUnit.h>
#include <IOKit/avc/IOFireWireAVCUserClientCommon.h>

// A little class to put into the connections set
class IOFireWireAVCConnection : public OSObject
{
    OSDeclareDefaultStructors(IOFireWireAVCConnection)

public:

    UInt32 fPlugAddr;
	UInt32 fChannel;
};

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
	OSArray *				fConnections;
    
    static void remakeConnections(void *arg);
    virtual IOExternalMethod * getTargetAndMethodForIndex(IOService **target, UInt32 index);
    virtual IOExternalAsyncMethod * getAsyncTargetAndMethodForIndex(IOService **target, UInt32 index);
    virtual IOReturn IOFireWireAVCUserClient::updateP2PCount(UInt32 addr, SInt32 inc, bool failOnBusReset, UInt32 chan, IOFWSpeed speed);
    virtual IOReturn makeConnection(UInt32 addr, UInt32 chan, IOFWSpeed speed);
    virtual void breakConnection(UInt32 addr);
    
public:

    static IOFireWireAVCUserClient* withTask( task_t owningTask );

    virtual bool init( OSDictionary * dictionary = 0 );
    virtual void free();
    virtual bool start( IOService * provider );
    virtual void stop( IOService * provider );

    virtual IOReturn message(UInt32 type, IOService *provider, void *argument);
    
    virtual IOReturn clientClose( void );
    virtual IOReturn clientDied( void );

    virtual IOReturn open( void *, void *, void *, void *, void *, void * );
	virtual IOReturn openWithSessionRef( IOFireWireSessionRef sessionRef, void *, void *, void *, void *, void * );
	virtual IOReturn getSessionRef( IOFireWireSessionRef * sessionRef, void *, void *, void *, void *, void * );
    virtual IOReturn close( void * = 0, void * = 0, void * = 0, void * = 0, void * = 0, void * = 0);

    virtual IOReturn AVCCommand(UInt8 * cmd, UInt8 * response, UInt32 len, UInt32 *size);
    virtual IOReturn AVCCommandInGen(UInt8 * cmd, UInt8 * response, UInt32 len, UInt32 *size);

    virtual IOReturn updateAVCCommandTimeout( void * = 0, void * = 0, void * = 0, void * = 0, void * = 0, void * = 0);

    virtual IOReturn makeP2PInputConnection( UInt32 plugNo, UInt32 chan, void * = 0, void * = 0, void * = 0, void * = 0);
    virtual IOReturn breakP2PInputConnection( UInt32 plugNo, void * = 0, void * = 0, void * = 0, void * = 0, void * = 0);
    virtual IOReturn makeP2POutputConnection( UInt32 plugNo, UInt32 chan, IOFWSpeed speed, void * = 0, void * = 0, void * = 0);
    virtual IOReturn breakP2POutputConnection( UInt32 plugNo, void * = 0, void * = 0, void * = 0, void * = 0, void * = 0);
};

#endif // _IOKIT_IOFIREWIREAVCUSERCLIENT_H