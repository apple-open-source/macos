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

#ifndef _IOKIT_IOFIREWIREAVCPROTOCOLUSERCLIENT_H
#define _IOKIT_IOFIREWIREAVCPROTOCOLUSERCLIENT_H

#include <IOKit/IOUserClient.h>
#include <IOKit/firewire/IOFWAddressSpace.h>
#include "IOFireWireAVCUserClientCommon.h"

//#include <IOKit/firewire/IOFireWireController.h>
class IOFireWireAVCRequestSpace;
class IOFireWireNub;
class IOFireWirePCRSpace;

class IOFireWireAVCProtocolUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(IOFireWireAVCProtocolUserClient)

protected:
    static IOExternalAsyncMethod 	sAsyncMethods[kIOFWAVCProtocolUserClientNumAsyncCommands];
    static IOExternalMethod 		sMethods[kIOFWAVCProtocolUserClientNumCommands];
    
    task_t						fTask;
	bool						fStarted;
    IOFireWireNub *				fDevice;
    IOFireWireBus *				fBus;
    IOFireWireAVCRequestSpace *	fAVCSpace;
    OSAsyncReference			fAVCRequestCallbackAsyncRef;
    IOFireWirePCRSpace *		fPCRSpace;
    OSSet *						fInputPlugs;
    OSSet *						fOutputPlugs;
    
    static UInt32 avcRequestHandler(void *refcon, UInt16 nodeID, IOFWSpeed &speed,
                                  FWAddress addr, UInt32 len, const void *buf, IOFWRequestRefCon requestRefcon);
    static void forwardPlugWrite(void *refcon, UInt16 nodeID, UInt32 plug, UInt32 oldVal, UInt32 newVal);

    virtual IOReturn setAVCRequestCallback(OSAsyncReference asyncRef, UInt32 subUnitType, UInt32 subUnitID);
    virtual IOReturn sendAVCResponse(UInt32 generation, UInt16 nodeID, const char *buffer, UInt32 size);
    virtual IOReturn allocateInputPlug(OSAsyncReference asyncRef, void *userRefcon, UInt32 *plug);
    virtual IOReturn freeInputPlug(UInt32 plug);
    virtual IOReturn readInputPlug(UInt32 plug, UInt32 *val);
    virtual IOReturn updateInputPlug(UInt32 plug, UInt32 oldVal, UInt32 newVal);
    virtual IOReturn allocateOutputPlug(OSAsyncReference asyncRef, void *userRefcon, UInt32 *plug);
    virtual IOReturn freeOutputPlug(UInt32 plug);
    virtual IOReturn readOutputPlug(UInt32 plug, UInt32 *val);
    virtual IOReturn updateOutputPlug(UInt32 plug, UInt32 oldVal, UInt32 newVal);
    virtual IOReturn readOutputMasterPlug(UInt32 *val);
    virtual IOReturn updateOutputMasterPlug(UInt32 oldVal, UInt32 newVal);
    virtual IOReturn readInputMasterPlug(UInt32 *val);
    virtual IOReturn updateInputMasterPlug(UInt32 oldVal, UInt32 newVal);
    
    virtual IOExternalMethod * getTargetAndMethodForIndex(IOService **target, UInt32 index);
    virtual IOExternalAsyncMethod * getAsyncTargetAndMethodForIndex(IOService **target, UInt32 index);

    virtual void free();
    
public:
    virtual bool start( IOService * provider );
    virtual IOReturn newUserClient( task_t owningTask, void * securityID,
                                    UInt32 type, OSDictionary * properties,
                                    IOUserClient ** handler );
    virtual IOReturn clientClose( void );
    virtual IOReturn clientDied( void );

    // Make it easy to find
    virtual bool matchPropertyTable(OSDictionary * table);

};

#endif // _IOKIT_IOFIREWIREAVCPROTOCOLUSERCLIENT_H

