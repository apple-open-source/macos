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

static void avcCommandHandlerCallback( void *refcon, IOReturn result,
								void **args, int numArgs)
{
    AVCProtocol *me = (AVCProtocol *)refcon;
    UInt32 pos;
    UInt32 len;
    const UInt8* src;
	
	//printf("DEBUG: AVCProtocol::avcCommandHandlerCallback\n");

    pos = (UInt32)args[0];
    len = (UInt32)args[1];
    src = (const UInt8*)(args+2);
    if(pos == 0)
	{
        me->fCmdGeneration = (UInt32)args[2];
        me->fCmdSource = (UInt32)args[3];
        me->fCmdLen = (UInt32)args[4];
		me->userCallBack = (IOFWAVCCommandHandlerCallback) args[5];
		me->userRefCon = args[6];
		me->speed = (IOFWSpeed) args[7];
		me->handlerSearchIndex = (UInt32) args[8];
		src = (const UInt8*)(args+9);
    }
    bcopy(src, me->fCommand+pos, len);
    if(pos+len == me->fCmdLen)
	{
        IOReturn status;
		status = me->userCallBack(me->userRefCon, me->fCmdGeneration, me->fCmdSource, me->speed, me->fCommand , me->fCmdLen);

		// See if application handled command or not
		if (status != kIOReturnSuccess)
			// Pass this command back to the kernel to possibly
			// find another handler, or to respond not implemented
			IOConnectMethodScalarIStructureI(me->fConnection,
									kIOFWAVCProtocolUserClientAVCRequestNotHandled,
									4,
									me->fCmdLen,
									me->fCmdGeneration,
									me->fCmdSource,
									me->speed,
									me->handlerSearchIndex,
									me->fCommand);
    }
}

static void subunitPlugHandlerCallback( void *refcon, IOReturn result,
									   void **args, int numArgs)
{
	AVCProtocol *me = (AVCProtocol *)refcon;
	IOFWAVCSubunitPlugHandlerCallback userCallBack;
	void *userRefCon;
	IOReturn status;
	IOFWAVCSubunitPlugMessages plugMessage = (IOFWAVCSubunitPlugMessages)args[3];
	UInt32 generation = (UInt32) args[7];
	UInt32 nodeID = (UInt32) args[8];
	UInt8 response[8];
	IOFWAVCPlugTypes plugType = (IOFWAVCPlugTypes) args[1];
	UInt32 plugNum = (UInt32) args[2];
	UInt32 msgParams = (UInt32) args[4];
	UInt32 subunitTypeAndID = (UInt32) args[0];
	
	//printf("DEBUG: AVCProtocol::subunitPlugHandlerCallback\n");

	userCallBack = (IOFWAVCSubunitPlugHandlerCallback) args[5];
	userRefCon = args[6];

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
		sendAVCResponse(me,generation,(UInt16) nodeID,response,8);

		// If we accepted the control command, we need to set this
		// signal format as the current signal format
		if (status == kIOReturnSuccess)
			setSubunitPlugSignalFormat(me,subunitTypeAndID,plugType,plugNum,msgParams);
	}
	return;
}	

static void pcrWriteCallback( void *refcon, IOReturn result, 
													void **args, int numArgs)
{
    IOFWAVCPCRCallback func;

	//printf("DEBUG: AVCProtocol::pcrWriteCallback\n");

	func = (IOFWAVCPCRCallback)args[0];
    func(refcon, (UInt32)args[1], (UInt16)(UInt32)args[2], (UInt32)args[3], (UInt32)args[4], (UInt32)args[5]);
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
        me->fNotification = NULL;
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
    io_async_ref_t 			asyncRef;
    io_scalar_inband_t		params;
    mach_msg_type_number_t	size = 1;
    IOReturn status;

	//printf("DEBUG: AVCProtocol::allocateInputPlug\n");
	
    asyncRef[kIOAsyncCalloutFuncIndex] = (UInt32)(IOAsyncCallback)&pcrWriteCallback;
    asyncRef[kIOAsyncCalloutRefconIndex] = (UInt32)refcon;
    params[0] = (int)func;
    status = io_async_method_scalarI_scalarO( me->fConnection, me->fAsyncPort, 
                                                asyncRef, 3, 
                                                kIOFWAVCProtocolUserClientAllocateInputPlug,
                                                params, 1,
                                                (int *)plug, &size );
    return status;
}

static void freeInputPlug( void *self, UInt32 plug)
{
    AVCProtocol *me = getThis(self);

	//printf("DEBUG: AVCProtocol::freeInputPlug\n");
	
    IOConnectMethodScalarIScalarO(me->fConnection, kIOFWAVCProtocolUserClientFreeInputPlug,
        1, 0, plug);
}

static UInt32 readInputPlug( void *self, UInt32 plug)
{
    AVCProtocol *me = getThis(self);
	UInt32 val;

	//printf("DEBUG: AVCProtocol::readInputPlug\n");

	IOConnectMethodScalarIScalarO(me->fConnection, kIOFWAVCProtocolUserClientReadInputPlug,
        1, 1, plug, &val);
    return val;
}

static IOReturn updateInputPlug( void *self, UInt32 plug, UInt32 oldVal, UInt32 newVal)
{
    AVCProtocol *me = getThis(self);

	//printf("DEBUG: AVCProtocol::updateInputPlug\n");

	return IOConnectMethodScalarIScalarO(me->fConnection, kIOFWAVCProtocolUserClientUpdateInputPlug,
        3, 0, plug, oldVal, newVal);
}

static IOReturn allocateOutputPlug( void *self, void *refcon, IOFWAVCPCRCallback func, UInt32 *plug)
{
    AVCProtocol *me = getThis(self);
    io_async_ref_t 			asyncRef;
    io_scalar_inband_t		params;
    mach_msg_type_number_t	size = 1;
    IOReturn status;

	//printf("DEBUG: AVCProtocol::allocateOutputPlug\n");

    asyncRef[kIOAsyncCalloutFuncIndex] = (UInt32)(IOAsyncCallback)&pcrWriteCallback;
    asyncRef[kIOAsyncCalloutRefconIndex] = (UInt32)refcon;
    params[0] = (int)func;
    status = io_async_method_scalarI_scalarO( me->fConnection, me->fAsyncPort, 
                                                asyncRef, 3, 
                                                kIOFWAVCProtocolUserClientAllocateOutputPlug,
                                                params, 1,
                                                (int *)plug, &size );
    return status;
}

static void freeOutputPlug( void *self, UInt32 plug)
{
    AVCProtocol *me = getThis(self);

	//printf("DEBUG: AVCProtocol::freeOutputPlug\n");

    IOConnectMethodScalarIScalarO(me->fConnection, kIOFWAVCProtocolUserClientFreeOutputPlug,
        1, 0, plug);
}

static UInt32 readOutputPlug( void *self, UInt32 plug)
{
    AVCProtocol *me = getThis(self);
    UInt32 val;

	//printf("DEBUG: AVCProtocol::readOutputPlug\n");

    IOConnectMethodScalarIScalarO(me->fConnection, kIOFWAVCProtocolUserClientReadOutputPlug,
        1, 1, plug, &val);
    return val;
}

static IOReturn updateOutputPlug( void *self, UInt32 plug, UInt32 oldVal, UInt32 newVal)
{
    AVCProtocol *me = getThis(self);

	//printf("DEBUG: AVCProtocol::updateOutputPlug\n");

	return IOConnectMethodScalarIScalarO(me->fConnection, kIOFWAVCProtocolUserClientUpdateOutputPlug,
        3, 0, plug, oldVal, newVal);
}

static UInt32 readOutputMasterPlug( void *self)
{
    AVCProtocol *me = getThis(self);
    UInt32 val;

	//printf("DEBUG: AVCProtocol::readOutputMasterPlug\n");

    IOConnectMethodScalarIScalarO(me->fConnection, kIOFWAVCProtocolUserClientReadOutputMasterPlug,
        0, 1, &val);
    return val;
}

static IOReturn updateOutputMasterPlug( void *self, UInt32 oldVal, UInt32 newVal)
{
    AVCProtocol *me = getThis(self);

	//printf("DEBUG: AVCProtocol::updateOutputMasterPlug\n");

	return IOConnectMethodScalarIScalarO(me->fConnection, kIOFWAVCProtocolUserClientUpdateOutputMasterPlug,
        2, 0, oldVal, newVal);
}

static UInt32 readInputMasterPlug( void *self)
{
    AVCProtocol *me = getThis(self);
    UInt32 val;

	//printf("DEBUG: AVCProtocol::readInputMasterPlug\n");

	IOConnectMethodScalarIScalarO(me->fConnection, kIOFWAVCProtocolUserClientReadInputMasterPlug,
        0, 1, &val);
    return val;
}

static IOReturn updateInputMasterPlug( void *self, UInt32 oldVal, UInt32 newVal)
{
    AVCProtocol *me = getThis(self);

	//printf("DEBUG: AVCProtocol::updateInputMasterPlug\n");

	return IOConnectMethodScalarIScalarO(me->fConnection, kIOFWAVCProtocolUserClientUpdateInputMasterPlug,
        2, 0, oldVal, newVal);
}

static IOReturn publishAVCUnitDirectory(void *self)
{
    AVCProtocol *me = getThis(self);

	//printf("DEBUG: AVCProtocol::publishAVCUnitDirectory\n");

	IOConnectMethodScalarIScalarO(me->fConnection, kIOFWAVCProtocolUserClientPublishAVCUnitDirectory,
							   0, 0);
    return kIOReturnSuccess;
}

static IOReturn installAVCCommandHandler(void *self,
											UInt32 subUnitTypeAndID,
											UInt32 opCode,
											void *refCon,
											IOFWAVCCommandHandlerCallback callback)
{
    AVCProtocol *me = getThis(self);
    io_async_ref_t 			asyncRef;
    io_scalar_inband_t		params;
    mach_msg_type_number_t	size = 0;
    IOReturn status = kIOReturnSuccess;

	//printf("DEBUG: AVCProtocol::installAVCCommandHandler\n");

    asyncRef[0] = 0x1234;
    asyncRef[kIOAsyncCalloutFuncIndex] = (UInt32)(IOAsyncCallback)&avcCommandHandlerCallback;
    asyncRef[kIOAsyncCalloutRefconIndex] = (UInt32)me;
    asyncRef[3] = 0x3456;
    params[0]	= subUnitTypeAndID;
    params[1]	= opCode;
	params[2]	= (int) callback;
    params[3]	= (int) refCon;

    status = io_async_method_scalarI_scalarO( me->fConnection, me->fAsyncPort,
											  asyncRef, 3,
											  kIOFWAVCProtocolUserClientInstallAVCCommandHandler,
											  params, 4,
											  NULL, &size );
    return status;
}

static IOReturn sendAVCResponse(void *self,
							UInt32 generation,
							UInt16 nodeID,
							const char *response,
							UInt32 responseLen)
{
    AVCProtocol *me = getThis(self);

	return IOConnectMethodScalarIStructureI(me->fConnection, kIOFWAVCProtocolUserClientSendAVCResponse,
										   2, responseLen, generation, nodeID, response);
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
    io_async_ref_t 			asyncRef;
    io_scalar_inband_t		params;
    mach_msg_type_number_t	size = 1;
    IOReturn status = kIOReturnSuccess;

	//printf("DEBUG: AVCProtocol::addSubunit\n");

    asyncRef[0] = 0x1234;
    asyncRef[kIOAsyncCalloutFuncIndex] = (UInt32)(IOAsyncCallback)&subunitPlugHandlerCallback;
    asyncRef[kIOAsyncCalloutRefconIndex] = (UInt32)me;
    asyncRef[3] = 0x3456;
    params[0]	= subunitType;
    params[1]	= numSourcePlugs;
    params[2]	= numDestPlugs;
	params[3]	= (int) callback;
    params[4]	= (int) refCon;

    status = io_async_method_scalarI_scalarO( me->fConnection, me->fAsyncPort,
											  asyncRef, 3,
											  kIOFWAVCProtocolUserClientAddSubunit,
											  params, 5,
											  (int *)pSubunitTypeAndID, &size );
    return status;
}

IOReturn setSubunitPlugSignalFormat(void *self,
									   UInt32 subunitTypeAndID,
									   IOFWAVCPlugTypes plugType,
									   UInt32 plugNum,
									   UInt32 signalFormat)
{
    AVCProtocol *me = getThis(self);

	//printf("DEBUG: AVCProtocol::setSubunitPlugSignalFormat\n");

	return IOConnectMethodScalarIScalarO(me->fConnection, kIOFWAVCProtocolUserClientSetSubunitPlugSignalFormat,
										 4, 0, subunitTypeAndID, plugType, plugNum, signalFormat);
}


IOReturn getSubunitPlugSignalFormat(void *self,
									   UInt32 subunitTypeAndID,
									   IOFWAVCPlugTypes plugType,
									   UInt32 plugNum,
									   UInt32 *pSignalFormat)
{
    AVCProtocol *me = getThis(self);
	UInt32 sigFmt;
	IOReturn status = kIOReturnSuccess;

	//printf("DEBUG: AVCProtocol::getSubunitPlugSignalFormat\n");

	status = IOConnectMethodScalarIScalarO(me->fConnection, kIOFWAVCProtocolUserClientGetSubunitPlugSignalFormat,
									  3, 1, subunitTypeAndID, plugType, plugNum, &sigFmt);
	*pSignalFormat = sigFmt;
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
	IOByteCount outputCnt = sizeof(AVCConnectTargetPlugsOutParams);

	//printf("DEBUG: AVCProtocol::connectTargetPlugs\n");
	
	inParams.sourceSubunitTypeAndID = sourceSubunitTypeAndID;
	inParams.sourcePlugType = sourcePlugType;
	inParams.sourcePlugNum = *pSourcePlugNum;
	inParams.destSubunitTypeAndID = destSubunitTypeAndID;
	inParams.destPlugType = destPlugType;
	inParams.destPlugNum = *pDestPlugNum;
	inParams.lockConnection = lockConnection;
	inParams.permConnection = permConnection;

	status = IOConnectMethodStructureIStructureO(me->fConnection,
											  kIOFWAVCProtocolUserClientConnectTargetPlugs,
											  sizeof(AVCConnectTargetPlugsInParams),
											  &outputCnt,
											  (UInt8 *) &inParams,
											  (UInt8 *) &outParams);
											  
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

	//printf("DEBUG: AVCProtocol::disconnectTargetPlugs\n");
	return IOConnectMethodScalarIScalarO(me->fConnection, kIOFWAVCProtocolUserClientDisconnectTargetPlugs,
									  6, 0,
									  sourceSubunitTypeAndID,
									  sourcePlugType,
									  sourcePlugNum,
									  destSubunitTypeAndID,
									  destPlugType,
									  destPlugNum);
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
	IOByteCount outputCnt = sizeof(AVCGetTargetPlugConnectionInParams);

	//printf("DEBUG: AVCProtocol::getTargetPlugConnection\n");

	inParams.subunitTypeAndID = subunitTypeAndID;
	inParams.plugType = plugType;
	inParams.plugNum = plugNum;
		
	status = IOConnectMethodStructureIStructureO(me->fConnection,
											  kIOFWAVCProtocolUserClientGetTargetPlugConnection,
											  sizeof(AVCConnectTargetPlugsInParams),
											  &outputCnt,
											  (UInt8 *) &inParams,
											  (UInt8 *) &outParams);

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

