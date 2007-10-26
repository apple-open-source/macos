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

#ifndef _IOKIT_IOFIREWIREAVCPROTOCOLUSERCLIENT_H
#define _IOKIT_IOFIREWIREAVCPROTOCOLUSERCLIENT_H

#include <IOKit/IOUserClient.h>
#include <IOKit/firewire/IOFWAddressSpace.h>
#include <IOKit/avc/IOFireWireAVCUserClientCommon.h>
#include <IOKit/avc/IOFireWireAVCTargetSpace.h>

//#include <IOKit/firewire/IOFireWireController.h>
class IOFireWireNub;
class IOFireWirePCRSpace;
class IOFireWireAVCTargetSpace;

class IOFireWireAVCProtocolUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(IOFireWireAVCProtocolUserClient)

protected:
    task_t						fTask;
	bool						fStarted;
    IOFireWireNub *				fDevice;
    IOFireWireBus *				fBus;
    IOFireWirePCRSpace *		fPCRSpace;
    IOFireWireAVCTargetSpace *	fAVCTargetSpace;
    OSSet *						fInputPlugs;
    OSSet *						fOutputPlugs;
    
    static void forwardPlugWrite(void *refcon, UInt16 nodeID, UInt32 plug, UInt32 oldVal, UInt32 newVal);

	static void avcTargetCommandHandler(const AVCCommandHandlerInfo *pCmdInfo,
									 UInt32 generation,
									 UInt16 nodeID,
									 const void *command,
									 UInt32 cmdLen,
									 IOFWSpeed &speed,
									 UInt32 handlerSearchIndex);

	static void avcSubunitPlugHandler(const AVCSubunitInfo *pSubunitInfo,
								   IOFWAVCSubunitPlugMessages plugMessage,
								   IOFWAVCPlugTypes plugType,
								   UInt32 plugNum,
								   UInt32 messageParams,
								   UInt32 generation,
								   UInt16 nodeID);

    virtual IOReturn sendAVCResponse(UInt32 generation, UInt16 nodeID, const char *buffer, UInt32 size);
    virtual IOReturn allocateInputPlug(io_user_reference_t *asyncRef, uint64_t userRefcon, uint64_t *plug);
    virtual IOReturn freeInputPlug(UInt32 plug);
    virtual IOReturn readInputPlug(UInt32 plug, uint64_t *val);
    virtual IOReturn updateInputPlug(UInt32 plug, UInt32 oldVal, UInt32 newVal);
    virtual IOReturn allocateOutputPlug(io_user_reference_t *asyncRef, uint64_t userRefcon, uint64_t *plug);
    virtual IOReturn freeOutputPlug(UInt32 plug);
    virtual IOReturn readOutputPlug(UInt32 plug, uint64_t *val);
    virtual IOReturn updateOutputPlug(UInt32 plug, UInt32 oldVal, UInt32 newVal);
    virtual IOReturn readOutputMasterPlug(uint64_t *val);
    virtual IOReturn updateOutputMasterPlug(UInt32 oldVal, UInt32 newVal);
    virtual IOReturn readInputMasterPlug(uint64_t *val);
    virtual IOReturn updateInputMasterPlug(UInt32 oldVal, UInt32 newVal);
    virtual IOReturn publishAVCUnitDirectory(void);
	virtual IOReturn installAVCCommandHandler(io_user_reference_t *asyncRef, uint64_t subUnitTypeAndID, uint64_t opCode, uint64_t callback, uint64_t refCon);
    virtual IOReturn addSubunit(io_user_reference_t *asyncRef,
								uint64_t subunitType,
								uint64_t numSourcePlugs,
								uint64_t numDestPlugs,
								uint64_t callBack,
								uint64_t refCon,
								uint64_t *subUnitTypeAndID);
	virtual IOReturn setSubunitPlugSignalFormat(UInt32 subunitTypeAndID,
											 IOFWAVCPlugTypes plugType,
											 UInt32 plugNum,
											 UInt32 signalFormat);

	virtual IOReturn getSubunitPlugSignalFormat(UInt32 subunitTypeAndID,
											 IOFWAVCPlugTypes plugType,
											 UInt32 plugNum,
											 uint64_t *pSignalFormat);

	virtual IOReturn connectTargetPlugs(AVCConnectTargetPlugsInParams *inParams,
									 AVCConnectTargetPlugsOutParams *outParams);

	virtual IOReturn disconnectTargetPlugs(UInt32 sourceSubunitTypeAndID,
								   IOFWAVCPlugTypes sourcePlugType,
								   UInt32 sourcePlugNum,
								   UInt32 destSubunitTypeAndID,
								   IOFWAVCPlugTypes destPlugType,
								   UInt32 destPlugNum);

	virtual IOReturn getTargetPlugConnection(AVCGetTargetPlugConnectionInParams *inParams,
										  AVCGetTargetPlugConnectionOutParams *outParams);

    virtual IOReturn AVCRequestNotHandled(UInt32 generation,
										  UInt16 nodeID,
										  IOFWSpeed speed,
										  UInt32 handlerSearchIndex,
										  const char *pCmdBuf,
										  UInt32 cmdLen);

	virtual IOReturn externalMethod( uint32_t selector, 
									IOExternalMethodArguments * arguments, 
									IOExternalMethodDispatch * dispatch, 
									OSObject * target, 
									void * reference);
	
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

