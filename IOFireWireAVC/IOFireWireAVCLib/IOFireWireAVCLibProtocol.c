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

#include <stdlib.h>
#include <stdio.h>
#include <CoreFoundation/CoreFoundation.h>

#include <IOKit/avc/IOFireWireAVCLib.h>
#include "IOFireWireAVCUserClientCommon.h"
#include <IOKit/avc/IOFireWireAVCConsts.h>

#include <mach/mach_port.h>

#import <System/libkern/OSCrossEndian.h>

#include <syslog.h>	// Debug messages

__BEGIN_DECLS
#include <IOKit/iokitmig.h>
__END_DECLS

#define FWLOG printf

struct _AVCProtocol;
typedef struct _InterfaceMap 
{
    IUnknownVTbl *pseudoVTable;
    struct _AVCProtocol *obj;
} InterfaceMap;

typedef struct _AVCProtocol
{

	//////////////////////////////////////
	// cf plugin interfaces
	
	InterfaceMap 			   				fIOCFPlugInInterface;
	InterfaceMap							fIOFireWireAVCLibProtocolInterface;

	//////////////////////////////////////
	// cf plugin ref counting
	
	CFUUIDRef 	fFactoryId;	
	UInt32 		fRefCount;

	//////////////////////////////////////	
	// user client connection
	
	io_service_t 	fService;
	io_connect_t 	fConnection;

	//////////////////////////////////////	
	// async callbacks
	
    mach_port_t				fAsyncPort;
	CFRunLoopRef			fCFRunLoop;
	CFRunLoopSourceRef		fCFRunLoopSource;
    CFMachPortRef			fCFAsyncPort;
    IONotificationPortRef	fNotifyPort;
    io_object_t				fNotification;
	IOFWAVCMessageCallback	fMessageCallbackRoutine;
	void *					fMessageCallbackRefCon;

	//////////////////////////////////////	
	// Lump 'o stuff for handling AVC requests
	IOFWAVCRequestCallback	fAVCRequestCallbackRoutine;
	void *					fAVCRequestCallbackRefCon;

    UInt32					fCmdLen;
    UInt32					fCmdGeneration;
    UInt32					fCmdSource;
    UInt8					fCommand[512];
	IOFWAVCCommandHandlerCallback userCallBack;
	void 					*userRefCon;
	IOFWSpeed 				speed;
	UInt32 					handlerSearchIndex;
} AVCProtocol;

	// utility function to get "this" pointer from interface
#define getThis( self ) \
        (((InterfaceMap *) self)->obj)

static IOReturn stop( void * self );
static void removeIODispatcherFromRunLoop( void * self );
static IOReturn sendAVCResponse(void *self,
								UInt32 generation,
								UInt16 nodeID,
								const char *response,
								UInt32 responseLen);
IOReturn setSubunitPlugSignalFormat(void *self,
									UInt32 subunitTypeAndID,
									IOFWAVCPlugTypes plugType,
									UInt32 plugNum,
									UInt32 signalFormat);

//////////////////////////////////////////////////////////////////
// callback static methods
//

// messageCallback
//
//

static void messageCallback(void * refcon, io_service_t service,
                          natural_t messageType, void *messageArgument)
{
	//printf("DEBUG: AVCProtocol::messageCallback\n");

	AVCProtocol *me = (AVCProtocol *)refcon;

	if( me->fMessageCallbackRoutine != NULL )
		(me->fMessageCallbackRoutine)( me->fMessageCallbackRefCon, messageType, messageArgument );
}

static void avcCommandHandlerCallback( void *refcon, IOReturn result, io_user_reference_t *args, int numArgs)
{
    AVCProtocol *me = (AVCProtocol *)refcon;
    UInt32 pos;
    UInt32 len;
    const UInt8* src;
	UInt32 fixedArgs[kMaxAsyncArgs];
	UInt32 i;
	uint64_t inArg[4];
	uint32_t outputScalarCnt = 0;
	size_t outputStructSize = 0;
	
	//printf("DEBUG: AVCProtocol::avcCommandHandlerCallback\n");

	// First copy all the args with endian byte-swapping. Note that only
	// the args that contain command-bytes need this, but doing them all
	// here simplifies the logic below.
	IF_ROSETTA()
	{
		// Note: This code assumes ROSETTA only happens for in 32-bit mode!
		for (i=0;i<numArgs;i++)
			fixedArgs[i] = (OSSwapInt32(args[i]) & 0xFFFFFFFF); 
	}
	else
	{
		for (i=0;i<numArgs;i++)
			fixedArgs[i] = (args[i] & 0xFFFFFFFF); 
	}

    pos = args[0] & 0xFFFFFFFF;
    len = args[1] & 0xFFFFFFFF;
    src = (const UInt8*)(fixedArgs+2);
    if(pos == 0)
	{
        me->fCmdGeneration = args[2] & 0xFFFFFFFF;
        me->fCmdSource = args[3] & 0xFFFFFFFF;;
        me->fCmdLen = args[4] & 0xFFFFFFFF;
		me->userCallBack = (IOFWAVCCommandHandlerCallback) ((unsigned long)args[5]);
		me->userRefCon = (void*) ((unsigned long)args[6]);
		me->speed = (IOFWSpeed) args[7] & 0xFFFFFFFF;
		me->handlerSearchIndex = args[8] & 0xFFFFFFFF;
		src = (const UInt8*)(fixedArgs+9);
    }
	
	bcopy(src, me->fCommand+pos, len);
    if(pos+len == me->fCmdLen)
	{
        IOReturn status;
		status = me->userCallBack(me->userRefCon, me->fCmdGeneration, me->fCmdSource, me->speed, me->fCommand , me->fCmdLen);

		// See if application handled command or not
		if (status != kIOReturnSuccess)
		{
			inArg[0] = me->fCmdGeneration;
			inArg[1] = me->fCmdSource;
			inArg[2] = me->speed;
			inArg[3] = me->handlerSearchIndex;
			
			// Pass this command back to the kernel to possibly
			// find another handler, or to respond not implemented
			IOConnectCallMethod(me->fConnection,
								kIOFWAVCProtocolUserClientAVCRequestNotHandled,
								inArg,
								4,
								me->fCommand,
								me->fCmdLen,
								NULL,
								&outputScalarCnt,
								NULL,
								&outputStructSize);
		}
    }
}

static void subunitPlugHandlerCallback( void *refcon, IOReturn result, io_user_reference_t *args, int numArgs)
{
	AVCProtocol *me = (AVCProtocol *)refcon;
	IOFWAVCSubunitPlugHandlerCallback userCallBack;
	void *userRefCon;
	IOReturn status;
	IOFWAVCSubunitPlugMessages plugMessage = (IOFWAVCSubunitPlugMessages)args[3];
	UInt32 generation = args[7] & 0xFFFFFFFF;
	UInt32 nodeID = args[8] & 0xFFFFFFFF;
	UInt8 response[8];
	IOFWAVCPlugTypes plugType = (IOFWAVCPlugTypes) args[1] & 0xFFFFFFFF;
	UInt32 plugNum = args[2] & 0xFFFFFFFF;
	UInt32 msgParams = args[4] & 0xFFFFFFFF;
	UInt32 subunitTypeAndID = args[0] & 0xFFFFFFFF;
	
	//printf("DEBUG: AVCProtocol::subunitPlugHandlerCallback\n");

	userCallBack = (IOFWAVCSubunitPlugHandlerCallback) ((unsigned long)args[5]);
	userRefCon = (void*) ((unsigned long)args[6]);

	// Callback the user
	status = userCallBack(userRefCon,subunitTypeAndID,plugType,plugNum,plugMessage,msgParams);
	
	// For the message kIOFWAVCSubunitPlugMsgSignalFormatModified
	// send a response to the plug signal format control command
	if (plugMessage == kIOFWAVCSubunitPlugMsgSignalFormatModified)
	{
		response[kAVCCommandResponse] = (status == kIOReturnSuccess) ? kAVCAcceptedStatus : kAVCRejectedStatus;
		response[kAVCAddress] = kAVCUnitAddress;
		response[kAVCOpcode] = (plugType == IOFWAVCPlugSubunitSourceType) ? kAVCOutputPlugSignalFormatOpcode : kAVCInputPlugSignalFormatOpcode;
		response[kAVCOperand0] = plugNum;
		response[kAVCOperand1] = ((msgParams & 0xFF000000) >> 24);
		response[kAVCOperand2] = ((msgParams & 0x00FF0000) >> 16);
		response[kAVCOperand3] = ((msgParams & 0x0000FF00) >> 8);
		response[kAVCOperand4] = (msgParams & 0x000000FF);
		
		sendAVCResponse(me,generation,(UInt16) nodeID,(const char *)response,8);

		// If we accepted the control command, we need to set this
		// signal format as the current signal format
		if (status == kIOReturnSuccess)
			setSubunitPlugSignalFormat(me,subunitTypeAndID,plugType,plugNum,msgParams);
	}
	return;
}	

static void pcrWriteCallback( void *refcon, IOReturn result, io_user_reference_t *args, int numArgs)
{
    IOFWAVCPCRCallback func;

	//printf("DEBUG: AVCProtocol::pcrWriteCallback\n");

	func = (IOFWAVCPCRCallback) ((unsigned long)args[0]);
    func(refcon, (UInt32)args[1], (UInt16)args[2], (UInt32)args[3], (UInt32)args[4], (UInt32)args[5]);
}

static UInt32 addRef( void * self )
{
	//printf("DEBUG: AVCProtocol::addRef\n");

	AVCProtocol *me = getThis(self);
	me->fRefCount++;
	return me->fRefCount;
}

static UInt32 release( void * self )
{
    AVCProtocol *me = getThis(self);
	UInt32 retVal = me->fRefCount;

	//printf("DEBUG: AVCProtocol::release\n");

	if( 1 == me->fRefCount-- ) 
	{
        removeIODispatcherFromRunLoop(self);
        stop(self);
        CFPlugInRemoveInstanceForFactory( me->fFactoryId );
        CFRelease( me->fFactoryId );
		free(me);
    }
    else if( me->fRefCount < 0 )
	{
        me->fRefCount = 0;
	}
	
	return retVal;
}


static HRESULT queryInterface( void * self, REFIID iid, void **ppv )
{
    CFUUIDRef uuid = CFUUIDCreateFromUUIDBytes(NULL, iid);
    HRESULT result = S_OK;
    AVCProtocol *me = getThis(self);

	//printf("DEBUG: AVCProtocol::queryInterface\n");

	if( CFEqual(uuid, IUnknownUUID) ||  CFEqual(uuid, kIOCFPlugInInterfaceID) ) 
	{
        *ppv = &me->fIOCFPlugInInterface;
        addRef(self);
    }
	else if( CFEqual(uuid, kIOFireWireAVCLibProtocolInterfaceID) ) 
	{
        *ppv = &me->fIOFireWireAVCLibProtocolInterface;
        addRef(self);
    }
    else
        *ppv = 0;

    if( !*ppv )
        result = E_NOINTERFACE;

    CFRelease( uuid );
	
    return result;
}

//////////////////////////////////////////////////////////////////
// IOCFPlugInInterface methods
//

// probe
//
//

static IOReturn probe( void * self, CFDictionaryRef propertyTable, 
											io_service_t service, SInt32 *order )
{

	//printf("DEBUG: AVCProtocol::probe\n");

	// only load against local FireWire node
    if( !service || !IOObjectConformsTo(service, "IOFireWireLocalNode") )
        return kIOReturnBadArgument;
		
	return kIOReturnSuccess;
}

// start
//
//

static IOReturn start( void * self, CFDictionaryRef propertyTable, 
											io_service_t service )
{
	IOReturn status = kIOReturnSuccess;
    CFNumberRef guidDesc = 0;
    io_iterator_t	enumerator = 0;
    io_object_t device = 0;
    mach_port_t		masterDevicePort;        
    AVCProtocol *me = getThis(self);
    CFMutableDictionaryRef	dict;
    CFMutableDictionaryRef	dict2;

	//printf("DEBUG: AVCProtocol::start\n");
	
	me->fService = service;
    
    // Conjure up our user client
    do {
        dict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0,
            &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);
    
        if(!dict)
            continue;
        dict2 = CFDictionaryCreateMutable( kCFAllocatorDefault, 0,
            &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);
    
        if(!dict2)
            continue;
        CFDictionarySetValue( dict2, CFSTR("IODesiredChild"), CFSTR("IOFireWireAVCProtocolUserClient") );
        CFDictionarySetValue( dict, CFSTR("SummonNub"), dict2 );
        
        status = IORegistryEntrySetCFProperties(service, dict );
        CFRelease( dict );
        CFRelease( dict2 );
    
        // Now find it - has same GUID as the IOFireWireLocalNode
        guidDesc = (CFNumberRef)IORegistryEntryCreateCFProperty(service, CFSTR("GUID"), kCFAllocatorDefault, 0);
    
        dict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0,
            &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);
    
        if(!dict)
            break;
    
        CFDictionarySetValue( dict, CFSTR(kIOProviderClassKey), CFSTR("IOFireWireAVCProtocolUserClient"));
        //CFDictionarySetValue( dict, CFSTR("GUID"), guidDesc);
        // get mach master port
        status = IOMasterPort(bootstrap_port, & masterDevicePort) ;
        if ( status != kIOReturnSuccess ) {
            break;
        }
        
        status = IOServiceGetMatchingServices(
                    masterDevicePort,
                    dict,
                    & enumerator );
    
        if( kIOReturnSuccess != status ) {
            break;
        }
        
        // Find an unused user client
        while(device = IOIteratorNext(enumerator)) {
            status = IOServiceOpen( device, mach_task_self(), 
                            kIOFireWireAVCLibConnection, &me->fConnection );
            IOObjectRelease(device);
            if(kIOReturnSuccess == status)
                break;
        }
    } while(0);
    
    if(guidDesc)
        CFRelease(guidDesc);
    if (enumerator)
        IOObjectRelease(enumerator) ;
    
	if( !me->fConnection )
		status = kIOReturnNoDevice;

	if( status == kIOReturnSuccess )
	{
	    status = IOCreateReceivePort( kOSAsyncCompleteMessageID, &me->fAsyncPort );
	}
		
	return status;
}

// stop
//
//

static IOReturn stop( void * self )
{
    AVCProtocol *me = getThis(self);

	//printf("DEBUG: AVCProtocol::stop\n");

	if( me->fConnection ) 
	{
        IOServiceClose( me->fConnection );
        me->fConnection = MACH_PORT_NULL;
    }

	if( me->fAsyncPort != MACH_PORT_NULL )
	{
        mach_port_destroy( mach_task_self(), me->fAsyncPort);
		me->fAsyncPort = MACH_PORT_NULL;
	}
	
	return kIOReturnSuccess;
}

//////////////////////////////////////////////////////////////////
// IOFireWireAVCLibProtocol methods
//

// addIODispatcherToRunLoop
//
//

static IOReturn addIODispatcherToRunLoop( void *self, CFRunLoopRef cfRunLoopRef )
{
    AVCProtocol *me = getThis(self);
	IOReturn 				status = kIOReturnSuccess;
    mach_port_t masterDevicePort;
	IONotificationPortRef notifyPort;
	CFRunLoopSourceRef cfSource;

	//printf("DEBUG: AVCProtocol::addIODispatcherToRunLoop\n");
	
	if( !me->fConnection )		    
		return kIOReturnNoDevice; 

	// get mach master port
	status = IOMasterPort(bootstrap_port, &masterDevicePort) ;

    notifyPort = IONotificationPortCreate(masterDevicePort);
    cfSource = IONotificationPortGetRunLoopSource(notifyPort);
    CFRunLoopAddSource(cfRunLoopRef, cfSource, kCFRunLoopDefaultMode);
// Get messages from device
    status = IOServiceAddInterestNotification(notifyPort, me->fService,
                                kIOGeneralInterest, messageCallback, me,
                                &me->fNotification);
	
    me->fCFRunLoop = cfRunLoopRef;
    me->fNotifyPort = notifyPort;


	if( status == kIOReturnSuccess )
	{
		CFMachPortContext context;
		Boolean	shouldFreeInfo; // zzz what's this for? I think it's set to true if the create failed.
	
		context.version = 1;
		context.info = me;
		context.retain = NULL;
		context.release = NULL;
		context.copyDescription = NULL;

		me->fCFAsyncPort = CFMachPortCreateWithPort( kCFAllocatorDefault, me->fAsyncPort, 
							(CFMachPortCallBack) IODispatchCalloutFromMessage, 
							&context, &shouldFreeInfo );
		if( !me->fCFAsyncPort )
			status = kIOReturnNoMemory;
	}
		
	if( status == kIOReturnSuccess )
	{
		me->fCFRunLoopSource = CFMachPortCreateRunLoopSource( kCFAllocatorDefault, me->fCFAsyncPort, 0 );
		if( !me->fCFRunLoopSource )
			status = kIOReturnNoMemory;
	}

	if( status == kIOReturnSuccess )
	{
		CFRunLoopAddSource(cfRunLoopRef, me->fCFRunLoopSource, kCFRunLoopDefaultMode );
	}
    
	return status;
}

// removeIODispatcherFromRunLoop
//
//

static void removeIODispatcherFromRunLoop( void * self )
{
    AVCProtocol *me = getThis(self);

	//printf("DEBUG: AVCProtocol::removeIODispatcherFromRunLoop\n");

    if( me->fNotification )
    {
        IOObjectRelease(me->fNotification);
        me->fNotification = (io_object_t)0;
    }
	if( me->fNotifyPort )
	{
		CFRunLoopRemoveSource( me->fCFRunLoop,
            IONotificationPortGetRunLoopSource(me->fNotifyPort), kCFRunLoopDefaultMode );
        IONotificationPortDestroy(me->fNotifyPort);
		me->fNotifyPort = NULL;
	}
    
    if(me->fCFRunLoopSource) {
		CFRunLoopRemoveSource( me->fCFRunLoop,
            me->fCFRunLoopSource, kCFRunLoopDefaultMode );
        CFRelease(me->fCFRunLoopSource);
        me->fCFRunLoopSource = NULL;
    }

	if( me->fCFAsyncPort != NULL ) {
        CFMachPortInvalidate(me->fCFAsyncPort);
		CFRelease( me->fCFAsyncPort );
        me->fCFAsyncPort = NULL;
    }
}

// setMessageCallback
//
//

static void setMessageCallback( void * self, void * refCon, 
													IOFWAVCMessageCallback callback )
{
    AVCProtocol *me = getThis(self);

	//printf("DEBUG: AVCProtocol::setMessageCallback\n");
	
	me->fMessageCallbackRoutine = callback;
	me->fMessageCallbackRefCon = refCon;
}

// setAVCRequestCallback
//
//

static IOReturn setAVCRequestCallback( void *self, UInt32 subUnitType, UInt32 subUnitID,
                                                void *refCon, IOFWAVCRequestCallback callback)
{
	// This function has been deprecated!
    return kIOReturnUnsupported;
}

static IOReturn allocateInputPlug( void *self, void *refcon, IOFWAVCPCRCallback func, UInt32 *plug)
{
    AVCProtocol *me = getThis(self);
    uint64_t params;
    IOReturn status;
	uint64_t refrncData[kOSAsyncRef64Count];
	uint32_t outputCnt = 1;
	uint64_t returnVal;

	//printf("DEBUG: AVCProtocol::allocateInputPlug\n");
	
    refrncData[kIOAsyncCalloutFuncIndex] = (uint64_t)&pcrWriteCallback;
    refrncData[kIOAsyncCalloutRefconIndex] = (unsigned long)refcon;
    params = (unsigned long)func;
	const uint64_t inputs[1]={(const uint64_t)params};

	status = IOConnectCallAsyncScalarMethod(me->fConnection,
											kIOFWAVCProtocolUserClientAllocateInputPlug,
											me->fAsyncPort,
											refrncData,kOSAsyncRef64Count,
											inputs, 1,
											&returnVal,&outputCnt);
	*plug = returnVal & 0xFFFFFFFF;
    
	return status;
}

static void freeInputPlug( void *self, UInt32 plug)
{
    AVCProtocol *me = getThis(self);
	uint32_t outputCnt = 0;
	const uint64_t inArg = plug;
	const uint64_t inputs[1]={(const uint64_t)inArg};


	//printf("DEBUG: AVCProtocol::freeInputPlug\n");
	
	IOConnectCallScalarMethod(me->fConnection,
							kIOFWAVCProtocolUserClientFreeInputPlug,
							inputs,
							1,
							NULL,
							&outputCnt);
}

static UInt32 readInputPlug( void *self, UInt32 plug)
{
    AVCProtocol *me = getThis(self);
	UInt32 val;
	const uint64_t inArg = plug;
	uint32_t outputCnt = 1;
	uint64_t outputVal = 0;
	const uint64_t inputs[1]={(const uint64_t)inArg};

	//printf("DEBUG: AVCProtocol::readInputPlug\n");

	IOConnectCallScalarMethod(me->fConnection,
							kIOFWAVCProtocolUserClientReadInputPlug,
							inputs,
							1,
							&outputVal,
							&outputCnt);

	val = outputVal & 0xFFFFFFFF;
	
    return val;
}

static IOReturn updateInputPlug( void *self, UInt32 plug, UInt32 oldVal, UInt32 newVal)
{
    AVCProtocol *me = getThis(self);
	uint64_t inArg[3];
	uint32_t outputCnt = 0;
	
	inArg[0] = plug;
	inArg[1] = oldVal;
	inArg[2] = newVal;

	//printf("DEBUG: AVCProtocol::updateInputPlug\n");

	return IOConnectCallScalarMethod(me->fConnection,
							kIOFWAVCProtocolUserClientUpdateInputPlug,
							inArg,
							3,
							NULL,
							&outputCnt);
}

static IOReturn allocateOutputPlug( void *self, void *refcon, IOFWAVCPCRCallback func, UInt32 *plug)
{
	AVCProtocol *me = getThis(self);
    uint64_t params;
    IOReturn status;
	uint64_t refrncData[kOSAsyncRef64Count];
	uint32_t outputCnt = 1;
	uint64_t returnVal;

	//printf("DEBUG: AVCProtocol::allocateOutputPlug\n");

	refrncData[kIOAsyncCalloutFuncIndex] = (uint64_t)pcrWriteCallback;
    refrncData[kIOAsyncCalloutRefconIndex] = (unsigned long)refcon;
    params = (unsigned long)func;
	const uint64_t inputs[1]={(const uint64_t)params};

	status = IOConnectCallAsyncScalarMethod(me->fConnection,
											kIOFWAVCProtocolUserClientAllocateOutputPlug,
											me->fAsyncPort,
											refrncData,kOSAsyncRef64Count,
											inputs, 1,
											&returnVal,&outputCnt);

	*plug = returnVal & 0xFFFFFFFF;
	
	return status;
}

static void freeOutputPlug( void *self, UInt32 plug)
{
    AVCProtocol *me = getThis(self);
	uint32_t outputCnt = 0;
	const uint64_t inArg = plug;
	const uint64_t inputs[1]={(const uint64_t)inArg};

	//printf("DEBUG: AVCProtocol::freeOutputPlug\n");

	IOConnectCallScalarMethod(me->fConnection,
							kIOFWAVCProtocolUserClientFreeOutputPlug,
							inputs,
							1,
							NULL,
							&outputCnt);
}

static UInt32 readOutputPlug( void *self, UInt32 plug)
{
    AVCProtocol *me = getThis(self);
    UInt32 val;
	const uint64_t inArg = plug;
	uint32_t outputCnt = 1;
	uint64_t outputVal = 0;
	const uint64_t inputs[1]={(const uint64_t)inArg};

	//printf("DEBUG: AVCProtocol::readOutputPlug\n");

	IOConnectCallScalarMethod(me->fConnection,
							kIOFWAVCProtocolUserClientReadOutputPlug,
							inputs,
							1,
							&outputVal,
							&outputCnt);
	
	val = outputVal & 0xFFFFFFFF;
	
    return val;
}

static IOReturn updateOutputPlug( void *self, UInt32 plug, UInt32 oldVal, UInt32 newVal)
{
    AVCProtocol *me = getThis(self);
	uint64_t inArg[3];
	uint32_t outputCnt = 0;
	
	inArg[0] = plug;
	inArg[1] = oldVal;
	inArg[2] = newVal;
	
	//printf("DEBUG: AVCProtocol::updateOutputPlug\n");

	return IOConnectCallScalarMethod(me->fConnection,
								kIOFWAVCProtocolUserClientUpdateOutputPlug,
								inArg,
								3,
								NULL,
								&outputCnt);
}

static UInt32 readOutputMasterPlug( void *self)
{
    AVCProtocol *me = getThis(self);
    UInt32 val;
	uint32_t outputCnt = 1;
	uint64_t outputVal = 0;

	//printf("DEBUG: AVCProtocol::readOutputMasterPlug\n");

	IOConnectCallScalarMethod(me->fConnection,
							kIOFWAVCProtocolUserClientReadOutputMasterPlug,
							NULL,
							0,
							&outputVal,
							&outputCnt);
	
	val = outputVal & 0xFFFFFFFF;
	
    return val;
}

static IOReturn updateOutputMasterPlug( void *self, UInt32 oldVal, UInt32 newVal)
{
    AVCProtocol *me = getThis(self);
	uint64_t inArg[2];
	uint32_t outputCnt = 0;
	
	inArg[0] = oldVal;
	inArg[1] = newVal;

	//printf("DEBUG: AVCProtocol::updateOutputMasterPlug\n");

	return IOConnectCallScalarMethod(me->fConnection,
								kIOFWAVCProtocolUserClientUpdateOutputMasterPlug,
								inArg,
								2,
								NULL,
								&outputCnt);
}

static UInt32 readInputMasterPlug( void *self)
{
    AVCProtocol *me = getThis(self);
    UInt32 val;
	uint32_t outputCnt = 1;
	uint64_t outputVal = 0;

	//printf("DEBUG: AVCProtocol::readInputMasterPlug\n");

	IOConnectCallScalarMethod(me->fConnection,
							kIOFWAVCProtocolUserClientReadInputMasterPlug,
							NULL,
							0,
							&outputVal,
							&outputCnt);
	
	val = outputVal & 0xFFFFFFFF;
	
    return val;
}

static IOReturn updateInputMasterPlug( void *self, UInt32 oldVal, UInt32 newVal)
{
    AVCProtocol *me = getThis(self);
	uint64_t inArg[2];
	uint32_t outputCnt = 0;
	
	inArg[0] = oldVal;
	inArg[1] = newVal;

	//printf("DEBUG: AVCProtocol::updateInputMasterPlug\n");

	return IOConnectCallScalarMethod(me->fConnection,
									kIOFWAVCProtocolUserClientUpdateInputMasterPlug,
									inArg,
									2,
									NULL,
									&outputCnt);
}

static IOReturn publishAVCUnitDirectory(void *self)
{
    AVCProtocol *me = getThis(self);
	uint32_t outputCnt = 0;

	//printf("DEBUG: AVCProtocol::publishAVCUnitDirectory\n");

	IOConnectCallScalarMethod(me->fConnection,
							kIOFWAVCProtocolUserClientPublishAVCUnitDirectory,
							NULL,
							0,
							NULL,
							&outputCnt);
	
    return kIOReturnSuccess;
}

static IOReturn installAVCCommandHandler(void *self,
											UInt32 subUnitTypeAndID,
											UInt32 opCode,
											void *refCon,
											IOFWAVCCommandHandlerCallback callback)
{
    AVCProtocol *me = getThis(self);
    IOReturn status = kIOReturnSuccess;
	uint64_t params[4];
	uint64_t refrncData[kOSAsyncRef64Count];
	uint32_t outputCnt = 0;
	

	//printf("DEBUG: AVCProtocol::installAVCCommandHandler\n");

    refrncData[kIOAsyncCalloutFuncIndex] = (uint64_t)avcCommandHandlerCallback;
    refrncData[kIOAsyncCalloutRefconIndex] = (unsigned long)me;
    params[0]	= subUnitTypeAndID;
    params[1]	= opCode;
	params[2]	= (unsigned long)callback;
    params[3]	= (unsigned long)refCon;

	status = IOConnectCallAsyncScalarMethod(me->fConnection,
											kIOFWAVCProtocolUserClientInstallAVCCommandHandler,
											me->fAsyncPort,
											refrncData,kOSAsyncRef64Count,
											params, 4,
											NULL,&outputCnt);
	
	return status;
}

static IOReturn sendAVCResponse(void *self,
							UInt32 generation,
							UInt16 nodeID,
							const char *response,
							UInt32 responseLen)
{
    AVCProtocol *me = getThis(self);
	uint64_t inArg[2];
	uint32_t outputScalarCnt = 0;
	size_t outputStructSize = 0;

	inArg[0] = generation;
	inArg[1] = nodeID;
	
	return IOConnectCallMethod(me->fConnection,
								kIOFWAVCProtocolUserClientSendAVCResponse,
								inArg,
								2,
								response,
								responseLen,
								NULL,
								&outputScalarCnt,
								NULL,
								&outputStructSize);
	

}

IOReturn addSubunit(void *self,
					UInt32 subunitType,
					UInt32 numSourcePlugs,
					UInt32 numDestPlugs,
					void *refCon,
					IOFWAVCSubunitPlugHandlerCallback callback,
					UInt32 *pSubunitTypeAndID)
{
    AVCProtocol *me = getThis(self);
    IOReturn status = kIOReturnSuccess;
	uint64_t params[5];
	uint64_t refrncData[kOSAsyncRef64Count];
	uint32_t outputCnt = 1;
	uint64_t returnVal;

	//printf("DEBUG: AVCProtocol::addSubunit\n");

    refrncData[kIOAsyncCalloutFuncIndex] = (uint64_t)subunitPlugHandlerCallback;
    refrncData[kIOAsyncCalloutRefconIndex] = (unsigned long)me;
    params[0]	= subunitType;
    params[1]	= numSourcePlugs;
    params[2]	= numDestPlugs;
	params[3]	= (unsigned long)callback;
    params[4]	= (unsigned long)refCon;

	status = IOConnectCallAsyncScalarMethod(me->fConnection,
											kIOFWAVCProtocolUserClientAddSubunit,
											me->fAsyncPort,
											refrncData,kOSAsyncRef64Count,
											params, 5,
											&returnVal,&outputCnt);
	
	*pSubunitTypeAndID = returnVal & 0xFFFFFFFF;
	
    return status;
}

IOReturn setSubunitPlugSignalFormat(void *self,
									   UInt32 subunitTypeAndID,
									   IOFWAVCPlugTypes plugType,
									   UInt32 plugNum,
									   UInt32 signalFormat)
{
    AVCProtocol *me = getThis(self);
	uint64_t inArg[4];
	uint32_t outputCnt = 0;
	
	inArg[0] = subunitTypeAndID;
	inArg[1] = plugType;
	inArg[2] = plugNum;
	inArg[3] = signalFormat;

	//printf("DEBUG: AVCProtocol::setSubunitPlugSignalFormat\n");

	return IOConnectCallScalarMethod(me->fConnection,
									kIOFWAVCProtocolUserClientSetSubunitPlugSignalFormat,
									inArg,
									4,
									NULL,
									&outputCnt);
}


IOReturn getSubunitPlugSignalFormat(void *self,
									   UInt32 subunitTypeAndID,
									   IOFWAVCPlugTypes plugType,
									   UInt32 plugNum,
									   UInt32 *pSignalFormat)
{
    AVCProtocol *me = getThis(self);
	IOReturn status = kIOReturnSuccess;
	uint32_t outputCnt = 1;
	uint64_t outputVal = 0;
	uint64_t inArg[3];
	
	inArg[0] = subunitTypeAndID;
	inArg[1] = plugType;
	inArg[2] = plugNum;

	//printf("DEBUG: AVCProtocol::getSubunitPlugSignalFormat\n");

	status =  IOConnectCallScalarMethod(me->fConnection,
									kIOFWAVCProtocolUserClientGetSubunitPlugSignalFormat,
									inArg,
									3,
									&outputVal,
									&outputCnt);
	
	*pSignalFormat = outputVal & 0xFFFFFFFF;
	return status;
}

IOReturn connectTargetPlugs(void *self,
							   UInt32 sourceSubunitTypeAndID,
							   IOFWAVCPlugTypes sourcePlugType,
							   UInt32 *pSourcePlugNum,
							   UInt32 destSubunitTypeAndID,
							   IOFWAVCPlugTypes destPlugType,
							   UInt32 *pDestPlugNum,
							   bool lockConnection,
							   bool permConnection)
{
    AVCProtocol *me = getThis(self);
	IOReturn status = kIOReturnSuccess;
	AVCConnectTargetPlugsInParams inParams;
	AVCConnectTargetPlugsOutParams outParams;
	size_t outputCnt = sizeof(AVCConnectTargetPlugsOutParams);

	//printf("DEBUG: AVCProtocol::connectTargetPlugs\n");
	
	inParams.sourceSubunitTypeAndID = sourceSubunitTypeAndID;
	inParams.sourcePlugType = sourcePlugType;
	inParams.sourcePlugNum = *pSourcePlugNum;
	inParams.destSubunitTypeAndID = destSubunitTypeAndID;
	inParams.destPlugType = destPlugType;
	inParams.destPlugNum = *pDestPlugNum;
	inParams.lockConnection = lockConnection;
	inParams.permConnection = permConnection;

	ROSETTA_ONLY(
		{
			inParams.sourceSubunitTypeAndID = OSSwapInt32(inParams.sourceSubunitTypeAndID);
			inParams.sourcePlugType = OSSwapInt32(inParams.sourcePlugType);
			inParams.sourcePlugNum = OSSwapInt32(inParams.sourcePlugNum);
			inParams.destSubunitTypeAndID = OSSwapInt32(inParams.destSubunitTypeAndID);
			inParams.destPlugType = OSSwapInt32(inParams.destPlugType);
			inParams.destPlugNum = OSSwapInt32(inParams.destPlugNum);
		}
	);

	status = IOConnectCallStructMethod(me->fConnection,
										kIOFWAVCProtocolUserClientConnectTargetPlugs,
										&inParams,
										sizeof(AVCConnectTargetPlugsInParams),
										&outParams,
										&outputCnt);
	
	ROSETTA_ONLY(
		{
			outParams.sourcePlugNum = OSSwapInt32(outParams.sourcePlugNum);
			outParams.destPlugNum = OSSwapInt32(outParams.destPlugNum);
		}
	);

	*pSourcePlugNum = outParams.sourcePlugNum;
	*pDestPlugNum = outParams.destPlugNum;
	return status;
}

IOReturn disconnectTargetPlugs(void *self,
								  UInt32 sourceSubunitTypeAndID,
								  IOFWAVCPlugTypes sourcePlugType,
								  UInt32 sourcePlugNum,
								  UInt32 destSubunitTypeAndID,
								  IOFWAVCPlugTypes destPlugType,
								  UInt32 destPlugNum)
{
    AVCProtocol *me = getThis(self);
	uint32_t outputCnt = 0;
	uint64_t inArg[6];
	
	inArg[0] = sourceSubunitTypeAndID;
	inArg[1] = sourcePlugType;
	inArg[2] = sourcePlugNum;
	inArg[3] = destSubunitTypeAndID;
	inArg[4] = destPlugType;
	inArg[5] = destPlugNum;
	
	//printf("DEBUG: AVCProtocol::disconnectTargetPlugs\n");
	
	return IOConnectCallScalarMethod(me->fConnection,
									kIOFWAVCProtocolUserClientDisconnectTargetPlugs,
									inArg,
									6,
									NULL,
									&outputCnt);
}

IOReturn getTargetPlugConnection(void *self,
									UInt32 subunitTypeAndID,
									IOFWAVCPlugTypes plugType,
									UInt32 plugNum,
									UInt32 *pConnectedSubunitTypeAndID,
									IOFWAVCPlugTypes *pConnectedPlugType,
									UInt32 *pConnectedPlugNum,
									bool *pLockConnection,
									bool *pPermConnection)
{
    AVCProtocol *me = getThis(self);
	IOReturn status = kIOReturnSuccess;
	AVCGetTargetPlugConnectionInParams inParams;
	AVCGetTargetPlugConnectionOutParams outParams;
	size_t outputCnt = sizeof(AVCGetTargetPlugConnectionInParams);

	//printf("DEBUG: AVCProtocol::getTargetPlugConnection\n");

	inParams.subunitTypeAndID = subunitTypeAndID;
	inParams.plugType = plugType;
	inParams.plugNum = plugNum;
		
	ROSETTA_ONLY(
		{
			inParams.subunitTypeAndID = OSSwapInt32(inParams.subunitTypeAndID);
			inParams.plugType = OSSwapInt32(inParams.plugType);
			inParams.plugNum = OSSwapInt32(inParams.plugNum);
		}
	);
		
	status = IOConnectCallStructMethod(me->fConnection,
									kIOFWAVCProtocolUserClientGetTargetPlugConnection,
									&inParams,
									sizeof(AVCConnectTargetPlugsInParams),
									&outParams,
									&outputCnt);
	
	ROSETTA_ONLY(
		{
			outParams.connectedSubunitTypeAndID = OSSwapInt32(outParams.connectedSubunitTypeAndID);
			outParams.connectedPlugType = OSSwapInt32(outParams.connectedPlugType);
			outParams.connectedPlugNum = OSSwapInt32(outParams.connectedPlugNum);
		}
	);

	*pConnectedSubunitTypeAndID = outParams.connectedSubunitTypeAndID;
	*pConnectedPlugType = outParams.connectedPlugType;
	*pConnectedPlugNum = outParams.connectedPlugNum;
	*pLockConnection = outParams.lockConnection;
	*pPermConnection = outParams.permConnection;
	
	return status;
}


// static interface table for IOCFPlugInInterface
//

static IOCFPlugInInterface sIOCFPlugInInterface = 
{
    0,
	&queryInterface,
	&addRef,
	&release,
	1, 0, // version/revision
	&probe,
	&start,
	&stop
};

//
// static interface table for IOFireWireAVCLibProtocolInterface
//

static IOFireWireAVCLibProtocolInterface sProtocolInterface =
{
    0,
	&queryInterface,
	&addRef,
	&release,
	2, 0, // version/revision
	&addIODispatcherToRunLoop,
	&removeIODispatcherFromRunLoop,
	&setMessageCallback,
    &setAVCRequestCallback,
    &allocateInputPlug,
    &freeInputPlug,
    &readInputPlug,
    &updateInputPlug,
    &allocateOutputPlug,
    &freeOutputPlug,
    &readOutputPlug,
    &updateOutputPlug,
    &readOutputMasterPlug,
    &updateOutputMasterPlug,
    &readInputMasterPlug,
    &updateInputMasterPlug,
	&publishAVCUnitDirectory,
	&installAVCCommandHandler,
	&sendAVCResponse,
	&addSubunit,
	&setSubunitPlugSignalFormat,
	&getSubunitPlugSignalFormat,
	&connectTargetPlugs,
	&disconnectTargetPlugs,
	&getTargetPlugConnection
};

// IOFireWireAVCLibProtocolFactory

// alloc
//
// static allocator, called by factory method

static IOCFPlugInInterface ** alloc()
{
	IOCFPlugInInterface ** 	interface = NULL;
    AVCProtocol *	me;

	//printf("DEBUG: AVCProtocol::alloc\n");
	
    me = (AVCProtocol *)malloc(sizeof(AVCProtocol));
    if( me )
	{
        bzero(me, sizeof(AVCProtocol));
		// we return an interface here. queryInterface will not be called. set refs to 1
        // init cf plugin ref counting
        me->fRefCount = 1;
        
        // init user client connection
        me->fConnection = MACH_PORT_NULL;
        me->fService = MACH_PORT_NULL;
        
        // create plugin interface map
        me->fIOCFPlugInInterface.pseudoVTable = (IUnknownVTbl *) &sIOCFPlugInInterface;
        me->fIOCFPlugInInterface.obj = me;
        
        // create test driver interface map
        me->fIOFireWireAVCLibProtocolInterface.pseudoVTable 
                                    = (IUnknownVTbl *) &sProtocolInterface;
        me->fIOFireWireAVCLibProtocolInterface.obj = me;
        
        me->fFactoryId = kIOFireWireAVCLibProtocolFactoryID;
        CFRetain( me->fFactoryId );
        CFPlugInAddInstanceForFactory( me->fFactoryId );
        interface = (IOCFPlugInInterface **) &me->fIOCFPlugInInterface.pseudoVTable;
    }
	return interface;
}

//
// factory method (only exported symbol)

void *IOFireWireAVCLibProtocolFactory( CFAllocatorRef allocator, CFUUIDRef typeID )
{

	//printf("DEBUG: AVCProtocol::IOFireWireAVCLibProtocolFactory\n");

	if( CFEqual(typeID, kIOFireWireAVCLibProtocolTypeID) )
        return (void *) alloc();
    else
        return NULL;
}

