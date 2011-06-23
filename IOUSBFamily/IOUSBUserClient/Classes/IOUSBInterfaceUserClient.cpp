/*
 * Copyright © 1998-2009 Apple Inc.  All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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
//================================================================================================
//
//   Headers
//
//================================================================================================
//
#include <IOKit/usb/IOUSBControllerV3.h>
#include <IOKit/IOKitKeys.h>

#include "IOUSBInterfaceUserClient.h"
#import "USBTracepoints.h"

#if DEBUG_LEVEL != 0
#include <sys/proc.h>
#endif

//================================================================================================
//
//   Local Definitions
//
//================================================================================================
//
#define super IOUserClient

// ExpansionData members
#define FOPENED_FOR_EXCLUSIVEACCESS					fIOUSBInterfaceUserClientExpansionData->fOpenedForExclusiveAccess
#define FDELAYED_WORKLOOP_FREE						fIOUSBInterfaceUserClientExpansionData->fDelayedWorkLoopFree
#define FOWNER_WAS_RELEASED							fIOUSBInterfaceUserClientExpansionData->fOwnerWasReleased

#ifndef kIOUserClientCrossEndianKey
#define kIOUserClientCrossEndianKey "IOUserClientCrossEndian"
#endif

#ifndef kIOUserClientCrossEndianCompatibleKey
#define kIOUserClientCrossEndianCompatibleKey "IOUserClientCrossEndianCompatible"
#endif

/* Convert USBLog to use kprintf debugging */
#ifndef IOUSBINTERFACEUSERCLIENT_USE_KPRINTF
#define IOUSBINTERFACEUSERCLIENT_USE_KPRINTF 0
#endif

#if IOUSBINTERFACEUSERCLIENT_USE_KPRINTF
#undef USBLog
#undef USBError
void kprintf(const char *format, ...)
__attribute__((format(printf, 1, 2)));
#define USBLog( LEVEL, FORMAT, ARGS... )  if ((LEVEL) <= IOUSBINTERFACEUSERCLIENT_USE_KPRINTF) { kprintf( FORMAT "\n", ## ARGS ) ; }
#define USBError( LEVEL, FORMAT, ARGS... )  { kprintf( FORMAT "\n", ## ARGS ) ; }
#endif
//=============================================================================================
//
//	Note on the use of IncrementOutstandingIO(), DecrementOutstandingIO() and doing extra
//	retain()/release() on async calls:
//
//	The UserClient is a complex driver.  It is attached to its provider when it is instantiated
// 	due to a device/interface being created but it is ALSO used by the user land IOUSBLib.  
//	Thus, it effectively has 2 clients, one on the kernel side and one on the user side.  When
//	the user side closes the connection to this user client, we terminate ourselves.  When the
//	device is unplugged, we get terminated by our provider.  The Increment/DecrementOutstandingIO()
//	calls are used to keep track of any IO that has not finished so that when OUR provider attempts
//	to terminate us, we won't get released (because we have our provider open) until the IO completes.
//	However, when our user land client closes us, we terminate ourselves.  In this case the 
//	OutstandingIO() calls do not help us in preventing our object from going away.  That is why we
//	also use a retain()/release() pair on async calls;  if we are closed before the async request is
//	complete, we will not go away and hence won't panic when we try to execute the completion routine.
//
//	Yes, we could have use only retain()/release(), but all our other kernel drivers use the 
//	outstandingIO metaphor so I thought it would be complete to use it here as well.
//
//	Another interesting piece is that we wait 1 ms in our clientClose method.  This will allow other 
//	pending threads to run and possible enter our driver (See bug #2862199).
//
//=============================================================================================
//

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
OSDefineMetaClassAndStructors(IOUSBInterfaceUserClientV2, super)
OSDefineMetaClassAndStructors(IOUSBLowLatencyCommand, IOCommand)


const IOExternalMethodDispatch 
IOUSBInterfaceUserClientV2::sMethods[kIOUSBLibInterfaceUserClientNumCommands] = {
    { //    kUSBInterfaceUserClientOpen
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_open,
		1, 0,
		0, 0
    },
    { //    kUSBInterfaceUserClientClose
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_close,
		0, 0,
		0, 0
    },
    { //    kUSBInterfaceUserClientGetDevice
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_GetDevice,
		0, 0,
		1, 0
    },
    { //    kUSBInterfaceUserClientSetAlternateInterface
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_SetAlternateInterface,
		1, 0,
		0, 0
    },
    { //    kUSBInterfaceUserClientGetFrameNumber
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_GetFrameNumber,
		0, 0,
		0, sizeof(IOUSBGetFrameStruct)
    },
    { //    kUSBInterfaceUserClientGetPipeProperties
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_GetPipeProperties,
		1, 0,
		5, 0
    },
    { //    kUSBInterfaceUserClientReadPipe
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_ReadPipe,
		5, 0,
		0, 0xffffffff
    },
    { //    kUSBInterfaceUserClientWritePipe
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_WritePipe,
		5, 0xffffffff,
		0, 0
    },
    { //    kUSBInterfaceUserClientGetPipeStatus
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_GetPipeStatus,
		1, 0,
		0, 0
    },
    { //    kUSBInterfaceUserClientAbortPipe
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_AbortPipe,
		1, 0,
		0, 0
    },
    { //    kUSBInterfaceUserClientResetPipe
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_ResetPipe,
		1, 0,
		0, 0
    },
    { //    kUSBInterfaceUserClientClearPipeStall
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_ClearPipeStall,
		2, 0,
		0, 0
    },
    { //    kUSBInterfaceUserClientControlRequestOut
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_ControlRequestOut,
		9, 0xffffffff,
		0, 0
    },
    { //    kUSBInterfaceUserClientControlRequestIn
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_ControlRequestIn,
		9, 0,
		1, 0xffffffff
    },
    { //    kUSBInterfaceuserClientSetPipePolicy
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_SetPipePolicy,
		3, 0,
		0, 0
    },
    { //    kUSBInterfaceuserClientGetBandwidthAvailable
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_GetBandwidthAvailable,
		0, 0,
		1, 0
    },
    { //    kUSBInterfaceuserClientGetEndpointProperties
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_GetEndpointProperties,
		3, 0,
		3, 0
    },
    { //    kUSBInterfaceUserClientLowLatencyPrepareBuffer
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_LowLatencyPrepareBuffer,
		7, 0,
		1, 0
    },
    { //    kUSBInterfaceUserClientLowLatencyReleaseBuffer
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_LowLatencyReleaseBuffer,
		7, 0,
		0, 0
    },
    { //    kUSBInterfaceUserClientGetMicroFrameNumber
        (IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_GetMicroFrameNumber,
		0, 0,
		0, sizeof(IOUSBGetFrameStruct)
    },
    { //    kUSBInterfaceUserClientGetFrameListTime
        (IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_GetFrameListTime,
		0, 0,
		1, 0
    },
	{ //    kUSBInterfaceUserClientGetFrameNumberWithTime
        (IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_GetFrameNumberWithTime,
		0, 0,
		0, sizeof(IOUSBGetFrameStruct)
    },
	{ //    kUSBInterfaceUserClientSetAsyncPort
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_SetAsyncPort,
		0, 0,
		0, 0
    },
    { //    kUSBInterfaceUserClientReadIsochPipe
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_ReadIsochPipe,
		6, 0,
		0, 0
    },
    { //    kUSBInterfaceUserClientWriteIsochPipe
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_WriteIsochPipe,
		6, 0,
		0, 0
    },
    { //    kUSBInterfaceUserClientLowLatencyReadIsochPipe
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_LowLatencyReadIsochPipe,
		9, 0,
		0, 0
    },
    { //    kUSBInterfaceUserClientLowLatencyWriteIsochPipe
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_LowLatencyWriteIsochPipe,
		9, 0,
		0, 0
    },
    {	//    kUSBInterfaceUserClientGetConfigDescriptor
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_GetConfigDescriptor,
		1, 0,
		0, 0xffffffff
    },
};



#pragma mark -

IOReturn IOUSBInterfaceUserClientV2::externalMethod( 
													uint32_t                    selector, 
													IOExternalMethodArguments * arguments,
													IOExternalMethodDispatch *  dispatch, 
													OSObject *                  target, 
													void *                      reference)
{
	
    if (selector < (uint32_t) kIOUSBLibInterfaceUserClientNumCommands)
    {
        dispatch = (IOExternalMethodDispatch *) &sMethods[selector];
        
        if (!target)
            target = this;
    }
	
	return super::externalMethod(selector, arguments, dispatch, target, reference);
}

#pragma mark Async Support

IOReturn 
IOUSBInterfaceUserClientV2::_SetAsyncPort(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::_SetAsyncPort",  target);
    return target->SetAsyncPort(arguments->asyncWakePort);
}


IOReturn IOUSBInterfaceUserClientV2::SetAsyncPort(mach_port_t port)
{
	USBLog(7,"+IOUSBInterfaceUserClientV2::SetAsyncPort");
    if (!fOwner)
        return kIOReturnNotAttached;
	
    fWakePort = port;
    return kIOReturnSuccess;
}

void
IOUSBInterfaceUserClientV2::ReqComplete(void *obj, void *param, IOReturn res, UInt32 remaining)
{
    io_user_reference_t						args[1];
    IOUSBUserClientAsyncParamBlock * pb = (IOUSBUserClientAsyncParamBlock *)param;
    IOUSBInterfaceUserClientV2 *me = OSDynamicCast(IOUSBInterfaceUserClientV2, (OSObject*)obj);
	
    if (!me)
		return;
	
    USBLog(7, "IOUSBInterfaceUserClientV2[%p]::ReqComplete, result = 0x%x (%s), req = %08x, remaining = %08x",  me, res, USBStringFromReturn(res), (int)pb->fMax, (int)remaining);
	
	USBTrace( kUSBTInterfaceUserClient,  kTPInterfaceUCReqComplete, (uintptr_t)me, res, remaining, pb->fMax );

	if ((res == kIOReturnSuccess) || (res == kIOReturnOverrun) )
    {
		// Return the len done anyway, its in the buffer
        args[0] = (io_user_reference_t)(pb->fMax - remaining);
    }
    else 
    {
        args[0] = 0;
    }
    if (pb->fMem)
    {
		pb->fMem->complete();
		pb->fMem->release();
    }
	
    if (!me->fDead)
		sendAsyncResult64(pb->fAsyncRef, res, args, 1);
	
    IOFree(pb, sizeof(*pb));
    me->DecrementOutstandingIO();
	me->release();
}


void
IOUSBInterfaceUserClientV2::IsoReqComplete(void *obj, void *param, IOReturn res, IOUSBIsocFrame *pFrames)
{
#pragma unused (pFrames)
   io_user_reference_t								args[1];
    IOUSBInterfaceUserClientISOAsyncParamBlock *	pb = (IOUSBInterfaceUserClientISOAsyncParamBlock *)param;
    IOUSBInterfaceUserClientV2 *					me = OSDynamicCast(IOUSBInterfaceUserClientV2, (OSObject*)obj);
	
    if (!me)
		return;
	
    USBLog(7, "IOUSBInterfaceUserClientV2[%p]::IsoReqComplete, result = 0x%x (%s), dataMem: %p",  me, res, USBStringFromReturn(res), pb->dataMem);
	
	USBTrace( kUSBTInterfaceUserClient,  kTPInterfaceUCIsoReqComplete, (uintptr_t)me, res, (uintptr_t) pb->frameBase, 0 );

	args[0] = (io_user_reference_t) pb->frameBase;
	if ( pb->countMem )
    	pb->countMem->writeBytes(0, pb->frames, pb->frameLen);
	
	if ( pb->dataMem )
	{
		pb->dataMem->complete();
		pb->dataMem->release();
		pb->dataMem = NULL;
    }
	
	if ( pb->countMem )
	{
		pb->countMem->complete();
		pb->countMem->release();
		pb->countMem = NULL;
    }
	
	if (!me->fDead)
		sendAsyncResult64(pb->fAsyncRef, res, args, 1);
	
    IOFree(pb, sizeof(*pb)+pb->frameLen);
    me->DecrementOutstandingIO();
	me->release();
}




void
IOUSBInterfaceUserClientV2::LowLatencyIsoReqComplete(void *obj, void *param, IOReturn res, IOUSBLowLatencyIsocFrame *pFrames)
{
#pragma unused (pFrames)
    io_user_reference_t			args[1];
    IOUSBLowLatencyCommand *	command = (IOUSBLowLatencyCommand *) param;
    IOMemoryDescriptor *		dataBufferDescriptor;
    OSAsyncReference64			asyncRef;
    
    IOUSBInterfaceUserClientV2 *	me = OSDynamicCast(IOUSBInterfaceUserClientV2, (OSObject*)obj);
	
    if (!me)
		return;
	
	USBLog(7, "%s[%p]::LowLatencyIsoReqComplete, result = 0x%x (%s)", me->getName(), me, res, USBStringFromReturn(res));
	
	USBTrace( kUSBTInterfaceUserClient,  kTPInterfaceUCLLIsoReqComplete, (uintptr_t)me, res, (uintptr_t) command->GetFrameBase(), 0 );

	args[0] = (io_user_reference_t) command->GetFrameBase(); 
    
    command->GetAsyncReference(&asyncRef);
    
    if (!me->fDead)
		sendAsyncResult64( asyncRef, res, args, 1);
	
    // Complete the memory descriptor
    //
    dataBufferDescriptor = command->GetDataBuffer();
    dataBufferDescriptor->complete();
    dataBufferDescriptor->release();
    dataBufferDescriptor = NULL;
    
	// Check to see if we need to released the LowLatency buffers
	if ( command->GetDMABufferInfo() )
	{
		USBLog(7, "%s[%p]::LowLatencyIsoReqComplete,  DMA Buffer: %p, refCount: %d, needToRelease: %d", me->getName(), me, command->GetDMABufferInfo(), (int32_t)(command->GetDMABufferInfo())->refCount, (int32_t)(command->GetDMABufferInfo())->needToRelease);

		(command->GetDMABufferInfo())->refCount--;

		if ( (command->GetDMABufferInfo())->needToRelease && ((command->GetDMABufferInfo())->refCount == 0))
		{
			USBLog(6, "%s[%p]::LowLatencyIsoReqComplete,  need to release buffer %p", me->getName(), me, command->GetDMABufferInfo());
			me->LowLatencyReleaseKernelBufferInfo(command->GetDMABufferInfo());
		}
	}
	
	if ( command->GetFrameListBufferInfo() )
	{
		USBLog(7, "%s[%p]::LowLatencyIsoReqComplete,  FrameList Buffer: %p, refCount: %d, needToRelease: %d", me->getName(), me, command->GetFrameListBufferInfo(), (int32_t)(command->GetFrameListBufferInfo())->refCount, (int32_t)(command->GetFrameListBufferInfo())->needToRelease);

		(command->GetFrameListBufferInfo())->refCount--;
		
		if ( (command->GetFrameListBufferInfo())->needToRelease && ((command->GetFrameListBufferInfo())->refCount == 0))
		{
			USBLog(6, "%s[%p]::LowLatencyIsoReqComplete,  need to release buffer %p", me->getName(), me, command->GetFrameListBufferInfo());
			me->LowLatencyReleaseKernelBufferInfo(command->GetFrameListBufferInfo());
		}
	}
	
    // Free/give back the command 
    me->fFreeUSBLowLatencyCommandPool->returnCommand(command);
	
    me->DecrementOutstandingIO();
	me->release();
}


#pragma mark User Client Start/Close

// Don't add any USBLogs to this routine.   You will panic if you use getName().
bool
IOUSBInterfaceUserClientV2::initWithTask(task_t owningTask,void *security_id , UInt32 type, OSDictionary * properties )
{
	if ( properties != NULL )
	{
		properties->setObject( kIOUserClientCrossEndianCompatibleKey, kOSBooleanTrue);
	}
	
    if (!super::initWithTask(owningTask, security_id , type, properties))
        return false;
	
    if (!owningTask)
		return false;
	
	// Allocate our expansion data
    //
    if (!fIOUSBInterfaceUserClientExpansionData)
    {
        fIOUSBInterfaceUserClientExpansionData = (IOUSBInterfaceUserClientExpansionData *)IOMalloc(sizeof(IOUSBInterfaceUserClientExpansionData));
        if (!fIOUSBInterfaceUserClientExpansionData)
            return false;
		
        bzero(fIOUSBInterfaceUserClientExpansionData, sizeof(IOUSBInterfaceUserClientExpansionData));
    }
	
	
    fTask = owningTask;
    fDead = false;
	
#if DEBUG_LEVEL != 0
	char	nbuf[256];
	proc_t	p = proc_self();
	int		owningPID = proc_pid(p);
	
	proc_name(owningPID,nbuf,256);
	
	USBLog(5,"IOUSBInterfaceUserClientV2[%p]::initWithTask  Owning PID is %d, name is %s", this, owningPID, nbuf);

	proc_rele(p);
#endif
	
	// If bit 31 of type is set, then our client is running under Rosetta
	fClientRunningUnderRosetta = ( type & 0x80000000 );
	
    return true;
}



bool 
IOUSBInterfaceUserClientV2::start( IOService * provider )
{
    IOWorkLoop	*			workLoop = NULL;
    IOCommandGate *			commandGate = NULL;
	
    USBLog(6, "+IOUSBInterfaceUserClientV2[%p]::start(%p)",  this, provider);
    
	// See comment below for when this is release()'d. 
	retain();
	
    if (!super::start(provider))
    {
        USBError(1, "IOUSBInterfaceUserClientV2[%p]::start - super::start returned false!",  this);
		release();
		return false;
    }
	
    fOwner = OSDynamicCast(IOUSBInterface, provider);
    if (!fOwner)
    {
        USBError(1, "IOUSBInterfaceUserClientV2[%p]::start - provider is NULL!",  this);
		release();
        return false;
    }
    
    // We will retain our owner/provider (above) and ourselves here.  We will release those when
    // we are done:  In clientCloseGated() when our user space client goes away (nicely or not), or in didTerminate() 
    // when our iokit device goes away.  If we have i/o pending when those 2 methods are called, we will do the release's
    // in Decrement/ChangeOutstandingIO() 

	fOwner->retain();
	FOWNER_WAS_RELEASED = false;
	
 	// Now, open() our provider, but not exclusively.  This will keep us from being release()'d until we close() it
	if ( !fOwner->open(this))
	{
		USBError(1, "IOUSBInterfaceUserClientV2[%p]::start -  could not open() our provider(%p)!",  this, fOwner);
		if ( !FOWNER_WAS_RELEASED )
		{
			USBLog(6, "IOUSBInterfaceUserClientV2[%p]::start -  releasing fOwner(%p) and UC!",  this, fOwner);
			FOWNER_WAS_RELEASED = true;
			fOwner->release();
			release();
		}
		return false;
	}
	
	
	commandGate = IOCommandGate::commandGate(this);
	
    if (!commandGate)
    {
        USBError(1, "IOUSBInterfaceUserClientV2[%p]::start - unable to create command gate",  this);
        goto ErrorExit;
    }
	
    workLoop = getWorkLoop();
    if (!workLoop)
    {
        USBError(1, "IOUSBInterfaceUserClientV2[%p]::start - unable to find my workloop",  this);
        goto ErrorExit;
    }
    workLoop->retain();
    
    if (workLoop->addEventSource(commandGate) != kIOReturnSuccess)
    {
        USBError(1, "IOUSBInterfaceUserClientV2[%p]::start - unable to add gate to work loop",  this);
        goto ErrorExit;
    }
	
    fFreeUSBLowLatencyCommandPool = IOUSBCommandPool::withWorkLoop(workLoop);
    if (!fFreeUSBLowLatencyCommandPool)
    {
        USBError(1,"IOUSBInterfaceUserClientV2[%p]::start - unable to create free command pool",  this);
        
        // Remove the event source we added above
        //
        workLoop->removeEventSource(commandGate);
        
        goto ErrorExit;
    }
	
	if ( isInactive() )
	{
		USBLog(6, "IOUSBInterfaceUserClientV2[%p]::start  we are inActive, so bailing out",  this);
        goto ErrorExit;
	}
		
    // Now that we have succesfully added our gate to the workloop, set our member variables
    //
    fGate = commandGate;
    fWorkLoop = workLoop;
	
    
    USBLog(6, "-IOUSBInterfaceUserClientV2[%p]::start",  this);
		
    return true;
    
ErrorExit:
		
	// Clean up
	if ( commandGate != NULL )
	{
		commandGate->release();
		commandGate = NULL;
	}
	
    if ( workLoop != NULL )
    {
        workLoop->release();
        workLoop = NULL;
    }
	
	if ( !FOWNER_WAS_RELEASED )
	{
		FOWNER_WAS_RELEASED = true;
		if (fOwner)
		{
			fOwner->close(this);
			fOwner->release();
		}
		
		release();
	}

    USBLog(6, "-IOUSBInterfaceUserClientV2[%p]::start returning FALSE",  this);
	
    return false;
}


IOReturn IOUSBInterfaceUserClientV2::_open(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
	USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::_open",  target);
	
	target->retain();
	IOReturn kr = target->open((bool)arguments->scalarInput[0]);
	target->release();
	
	return kr;
}

IOReturn 
IOUSBInterfaceUserClientV2::open(bool seize)
{
	IOReturn		ret = kIOReturnSuccess;
    IOOptionBits	options = seize ? (IOOptionBits)kIOServiceSeize : 0;
	
    USBLog(5, "+IOUSBInterfaceUserClientV2[%p]::open isInactive: %d",  this, isInactive());
    IncrementOutstandingIO();
	
	// Open our provider exclusively, to be closed in ::close()
    if (fOwner)
    {
		if (fOwner->open(this, options | kUSBOptionBitOpenExclusivelyMask))
		{
			FOPENED_FOR_EXCLUSIVEACCESS = true;
		}
		else
		{
			USBLog(5, "IOUSBInterfaceUserClientV2[%p]::open fOwner->open() failed.  Returning kIOReturnExclusiveAccess",  this);
			ret = kIOReturnExclusiveAccess;
		}
    }
    else
    {
        ret = kIOReturnNotAttached;
    }
    
    if ( ret == kIOReturnSuccess )
	{
		// If we are running under Rosetta, add a property to the interface nub:
		if ( fClientRunningUnderRosetta )
		{
			USBLog(5, "IOUSBInterfaceUserClientV2[%p]::open  setting kIOUserClientCrossEndianCompatibleKey TRUE on %p",  this, fOwner);
			fOwner->setProperty(kIOUserClientCrossEndianCompatibleKey, kOSBooleanTrue);
		}
		else
		{
			USBLog(5, "IOUSBInterfaceUserClientV2[%p]::open  setting kIOUserClientCrossEndianCompatibleKey FALSE on %p",  this, fOwner);
			fOwner->setProperty(kIOUserClientCrossEndianCompatibleKey, kOSBooleanFalse);
		}
		
		fNeedToClose = false;
	}
    
    USBLog(5, "-IOUSBInterfaceUserClientV2[%p]::open - returning 0x%x (%s)", this, ret, USBStringFromReturn(ret));    
    DecrementOutstandingIO();
    return ret;
}


IOReturn 
IOUSBInterfaceUserClientV2::_close(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference, arguments)
	IOReturn	ret;
	
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::_close",  target);

	target->retain();
	
	if (!target->isInactive() && target->fGate && target->fWorkLoop)
	{
		IOCommandGate *	gate = target->fGate;
		IOWorkLoop *	workLoop = target->fWorkLoop;
		
		workLoop->retain();
		gate->retain();
		
		ret = gate->runAction(closeGated);
	
		if ( ret != kIOReturnSuccess)
		{
			USBLog(3, "IOUSBInterfaceUserClientV2[%p]::_close  runAction returned 0x%x, isInactive(%s)",  target, ret, target->isInactive() ? "true" : "false");
		}
		
		gate->release();
		workLoop->release();
	}
	else
	{
		USBLog(1, "+IOUSBInterfaceUserClientV2[%p]::_close  no fGate, calling close() directly", target);
		ret = target->close();
	}
	
	target->release();
	
	return ret;
}

IOReturn  
IOUSBInterfaceUserClientV2::closeGated(OSObject *target, void *param1, void *param2, void *param3, void *param4)
{
#pragma unused (param1, param2, param3, param4)
	IOUSBInterfaceUserClientV2 *			me = OSDynamicCast(IOUSBInterfaceUserClientV2, target);
    if (!me)
    {
		USBLog(1, "IOUSBInterfaceUserClientV2::closeGated - invalid target");
		return kIOReturnBadArgument;
    }
	
    USBLog(5, "+IOUSBInterfaceUserClientV2[%p]::closeGated",  me);
	return me->close();
}


// This is NOT the normal IOService::close(IOService*) method.
// We are treating this is a proxy that we should close our parent, but
// maintain the connection with the task
IOReturn
IOUSBInterfaceUserClientV2::close()
{
    IOReturn 	ret = kIOReturnSuccess;
    
    USBLog(5, "+IOUSBInterfaceUserClientV2[%p]::close",  this);
    
    if (fOwner)
	{
		if (fOutstandingIO > 0)
		{
			int		i;
			
			// we need to abort any outstanding IO to allow us to close
			USBLog(5, "IOUSBInterfaceUserClientV2[%p]::close - outstanding IO - aborting pipes",  this);
			
			for (i = 1; i <= kUSBMaxPipes; i++)
			{
				AbortPipe(i);
			}
		}
		
		if (FOPENED_FOR_EXCLUSIVEACCESS)
		{
			IOOptionBits	options = kUSBOptionBitOpenExclusivelyMask;
			
			USBLog(6, "IOUSBInterfaceUserClientV2[%p]::close  we were open exclusively, closing exclusively",  this);
			FOPENED_FOR_EXCLUSIVEACCESS = false;
			
			fOwner->close(this, options);
		}
		else
		{
			USBLog(5, "IOUSBInterfaceUserClientV2[%p]::close - interface was not open for exclusive access",  this);
			ret = kIOReturnNotOpen;
		}
    }
    else
	{
		USBLog(5, "IOUSBInterfaceUserClientV2[%p]::close - fOwner was NULL",  this);
		ret = kIOReturnNotAttached;
	}
	
    USBLog(5, "-IOUSBInterfaceUserClientV2[%p]::close - returning 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	
    return ret;
}


#pragma mark Miscellaneous InterfaceUserClient

IOReturn IOUSBInterfaceUserClientV2::_SetAlternateInterface(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::_SetAlternateInterface",  target);
	
	target->retain();
    IOReturn kr = target->SetAlternateInterface((UInt8)arguments->scalarInput[0]);
	target->release();
	
	return kr;
}


IOReturn 
IOUSBInterfaceUserClientV2::SetAlternateInterface(UInt8 altSetting)
{
    IOReturn	ret;
    
    USBLog(6, "+IOUSBInterfaceUserClientV2[%p]::SetAlternateInterface to %d",  this, altSetting);
    IncrementOutstandingIO();
    
    if (fOwner && !isInactive())
		ret = fOwner->SetAlternateInterface(this, altSetting);
    else
		ret = kIOReturnNotAttached;
	
    if (ret)
	{
		USBLog(3, "IOUSBInterfaceUserClientV2[%p]::SetAlternateInterface - returning err 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	
    DecrementOutstandingIO();
    return ret;
}



IOReturn IOUSBInterfaceUserClientV2::_GetDevice(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::_GetDevice",  target);

	target->retain();
    IOReturn kr = target->GetDevice(&(arguments->scalarOutput[0]));
	target->release();
	
	return kr;
}

IOReturn 
IOUSBInterfaceUserClientV2::GetDevice(uint64_t *device)
{
    IOReturn		ret;
    io_object_t  	service;
	
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::GetDevice (%p, 0x%qx), isInactive: %d",  this, device, *device, isInactive());
	IncrementOutstandingIO();
	
	*device = NULL;
	
    if (fOwner && !isInactive())
    {
		// Although not documented, Simon says that exportObjectToClient consumes a reference,
		// so we have to put an extra retain on the device.  The user client side of the USB stack (USBLib)
		// always calls this routine in order to cache the USB device.  Radar #2586534
		fOwner->GetDevice()->retain();
		ret = exportObjectToClient(fTask, fOwner->GetDevice(), &service);
 		USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::GetDevice exportObjectToClient of (%p) returning err 0x%x",  this, fOwner->GetDevice(), ret);
		*device = (uint64_t) service;
   }
    else
        ret = kIOReturnNotAttached;
	
    if (ret)
	{
		USBLog(3, "IOUSBInterfaceUserClientV2[%p]::GetDevice - returning  0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	
    DecrementOutstandingIO();
    return ret;
}



IOReturn IOUSBInterfaceUserClientV2::_GetFrameNumber(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::_GetFrameNumber",  target);

	target->retain();
	IOReturn kr = target->GetFrameNumber((IOUSBGetFrameStruct *)arguments->structureOutput, (UInt32 *)&(arguments->structureOutputSize));
	target->release();
	
	return kr;
}  

IOReturn 
IOUSBInterfaceUserClientV2::GetFrameNumber(IOUSBGetFrameStruct *data, UInt32 *size)
{
    IOReturn		ret = kIOReturnSuccess;
	
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::GetFrameNumber",  this);
	
    IncrementOutstandingIO();
    if (fOwner && !isInactive())
    {
		uint64_t	currentTime = mach_absolute_time();
		data->timeStamp = * (AbsoluteTime *)&currentTime;
		data->frame = fOwner->GetDevice()->GetBus()->GetFrameNumber();
		USBLog(7,"IOUSBInterfaceUserClientV2::GetFrameNumber frame: 0x%qx, timestamp: 0x%qx", data->frame, currentTime);
		*size = sizeof(IOUSBGetFrameStruct);
    }
    else
        ret = kIOReturnNotAttached;
	
    if (ret)
	{
		USBLog(3, "IOUSBInterfaceUserClientV2[%p]::GetFrameNumber - returning err 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	
    DecrementOutstandingIO();
    USBLog(7, "-IOUSBInterfaceUserClientV2[%p]::GetFrameNumber  FrameNumber: %qd",  this,data->frame);
    return ret;
}



IOReturn IOUSBInterfaceUserClientV2::_GetMicroFrameNumber(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::_GetMicroFrameNumber",  target);
	
	target->retain();
	IOReturn kr = target->GetMicroFrameNumber((IOUSBGetFrameStruct *)arguments->structureOutput, (UInt32 *)&(arguments->structureOutputSize));
	target->release();
	
	return kr;
}  

IOReturn
IOUSBInterfaceUserClientV2::GetMicroFrameNumber(IOUSBGetFrameStruct *data, UInt32 *size)
{
    // This method only available for v2 controllers
    //
    IOUSBControllerV2	*v2 = NULL;
    IOReturn		ret = kIOReturnSuccess;
	
	if (fOwner)
		v2 = OSDynamicCast(IOUSBControllerV2, fOwner->GetDevice()->GetBus());
	
    if (!v2)
    {
        USBLog(3, "IOUSBInterfaceUserClientV2[%p]::GetMicroFrameNumber - Not a USB 2.0 controller!  Returning 0x%x",  this, kIOReturnNotAttached);
        return kIOReturnNotAttached;
    }
	
    IncrementOutstandingIO();
    if (fOwner && !isInactive())
    {
		UInt64	microFrame;
		
		uint64_t	currentTime = mach_absolute_time();
		data->timeStamp = * (AbsoluteTime *)&currentTime;
        microFrame = v2->GetMicroFrameNumber();
		if ( microFrame != 0)
		{
			data->frame = microFrame;
			USBLog(7,"IOUSBInterfaceUserClientV2::GetMicroFrameNumber frame: 0x%qx, timeStamp 0x%qx", data->frame, currentTime);
			*size = sizeof(IOUSBGetFrameStruct);
		}
		else
		{
			ret = kIOReturnUnsupported;
			*size = 0;
		}
    }
    else
    {
        USBLog(3, "IOUSBInterfaceUserClientV2[%p]::GetMicroFrameNumber - no fOwner(%p) or isInactive",  this, fOwner);
        ret = kIOReturnNotAttached;
    }
	
    if (ret)
	{
        USBLog(3, "IOUSBInterfaceUserClientV2[%p]::GetFrameNumber - returning err 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	
    DecrementOutstandingIO();
    return ret;
}


IOReturn IOUSBInterfaceUserClientV2::_GetFrameNumberWithTime(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
	USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::_GetFrameNumberWithTime",  target);
	
	target->retain();
	IOReturn kr = target->GetFrameNumberWithTime( (IOUSBGetFrameStruct*) arguments->structureOutput, (UInt32 *)&(arguments->structureOutputSize));
	target->release();
	
	return kr;
}  


IOReturn 
IOUSBInterfaceUserClientV2::GetFrameNumberWithTime(IOUSBGetFrameStruct *data, UInt32 *size)
{
    IOReturn		ret = kIOReturnSuccess;
	
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::GetFrameNumberWithTime",  this);
	
    if (*size != sizeof(IOUSBGetFrameStruct))
		return kIOReturnBadArgument;
    
    if (fOwner && !isInactive())
    {
		IOUSBControllerV2		*busV2 = OSDynamicCast(IOUSBControllerV2, fOwner->GetDevice()->GetBus());
		if (busV2)
		{
			ret = busV2->GetFrameNumberWithTime(&data->frame, &data->timeStamp);
			USBLog(7,"IOUSBInterfaceUserClientV2::GetFrameNumberWithTime frame: 0x%qx, timeStamp: 0x%qx", data->frame, AbsoluteTime_to_scalar(&data->timeStamp));
			*size = sizeof(IOUSBGetFrameStruct);
		}
		else
		{
			ret = kIOReturnUnsupported;
		}
    }
    else
        ret = kIOReturnNotAttached;
	
    if (ret)
	{
		USBLog(3, "IOUSBInterfaceUserClientV2[%p]::GetFrameNumberWithTime - returning err 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	
    return ret;
}



IOReturn IOUSBInterfaceUserClientV2::_GetBandwidthAvailable(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::_GetBandwidthAvailable",  target);
	
	target->retain();
    IOReturn kr = target->GetBandwidthAvailable(&(arguments->scalarOutput[0]));
	target->release();
	
	return kr;
}

IOReturn
IOUSBInterfaceUserClientV2::GetBandwidthAvailable(uint64_t *bandwidth)
{
    IOReturn		ret = kIOReturnSuccess;
	
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::GetBandwidthAvailable",  this);
	
    if (fOwner && !isInactive())
    {
        *bandwidth = fOwner->GetDevice()->GetBus()->GetBandwidthAvailable();
    }
    else
        ret = kIOReturnNotAttached;
	
    if (ret)
	{
        USBLog(3, "IOUSBInterfaceUserClientV2[%p]::GetBandwidthAvailable - returning err 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	
    return ret;
}


IOReturn IOUSBInterfaceUserClientV2::_GetFrameListTime(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::_GetFrameListTime",  target);

	target->retain();
	IOReturn kr = target->GetFrameListTime(&(arguments->scalarOutput[0]));
	target->release();
	
	return kr;
}

IOReturn
IOUSBInterfaceUserClientV2::GetFrameListTime(uint64_t *microsecondsInFrame)
{
    IOReturn		ret = kIOReturnSuccess;
	
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::GetFrameListTime",  this);
	
    if (fOwner && !isInactive())
    {
        UInt8	speed;
        // Find the speed of the device and return the appropriate microseconds
        // depending on the speed
        //
        speed  = fOwner->GetDevice()->GetSpeed();
		
        if ( speed == kUSBDeviceSpeedHigh )
        {
            // High Speed Device
            //
            *microsecondsInFrame = kUSBHighSpeedMicrosecondsInFrame;
        }
        else
        {
            // Low and Full Speed
            //
            *microsecondsInFrame = kUSBFullSpeedMicrosecondsInFrame;
        }
    }
    else
        ret = kIOReturnNotAttached;
	
    if (ret)
	{
        USBLog(3, "IOUSBInterfaceUserClientV2[%p]::GetFrameListTime - returning err 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	
    return ret;
}


IOReturn IOUSBInterfaceUserClientV2::_GetEndpointProperties(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::_GetEndpointProperties",  target);
	
	target->retain();
    IOReturn kr = target->GetEndpointProperties((UInt8)arguments->scalarInput[0], (UInt8)arguments->scalarInput[1], (UInt8)arguments->scalarInput[2], 
									&(arguments->scalarOutput[0]), &(arguments->scalarOutput[1]), &(arguments->scalarOutput[2]));
	target->release();
	
	return kr;
}

IOReturn 
IOUSBInterfaceUserClientV2::GetEndpointProperties(UInt8 alternateSetting, UInt8 endpointNumber, UInt8 direction, uint64_t *transferType, uint64_t *maxPacketSize, uint64_t *interval)
{
    IOReturn		ret = kIOReturnSuccess;
    UInt8			myTT;
    UInt16			myMPS;
    UInt8			myIV;
	
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::GetEndpointProperties for altSetting %d, endpoint: %d, direction %d",  this, alternateSetting, endpointNumber, direction);
	
    if (fOwner && !isInactive())
		ret = fOwner->GetEndpointProperties(alternateSetting, endpointNumber, direction, &myTT, &myMPS, &myIV);
    else
        ret = kIOReturnNotAttached;
	
    if (ret)
	{
		USBLog(3, "IOUSBInterfaceUserClientV2[%p]::GetEndpointProperties - returning err 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
    else
    {
        *transferType = myTT;
        *maxPacketSize = myMPS;
        *interval = myIV;
    }
    return ret;
}

IOReturn 
IOUSBInterfaceUserClientV2::_GetConfigDescriptor(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
	IOReturn	kr;
	
	USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::_GetConfigDescriptor",  target);
	
	target->retain();

    if ( arguments->structureOutputDescriptor ) 
        kr = target->GetConfigDescriptor((UInt8)arguments->scalarInput[0], arguments->structureOutputDescriptor, &(arguments->structureOutputDescriptorSize));
    else
        kr = target->GetConfigDescriptor((UInt8)arguments->scalarInput[0], (IOUSBConfigurationDescriptorPtr) arguments->structureOutput, (UInt32 *)&(arguments->structureOutputSize));

	target->release();
	
	return kr;
}  


IOReturn
IOUSBInterfaceUserClientV2::GetConfigDescriptor(UInt8 configIndex, IOUSBConfigurationDescriptorPtr desc, UInt32 *size)
{
    UInt16 				length;
    const IOUSBConfigurationDescriptor	*cached;
    IOReturn				ret;
    
    USBLog(7,"+IOUSBInterfaceUserClientV2[%p]::GetConfigDescriptor (Config %d), with size %d, struct: %p",  this, configIndex, (uint32_t)*size, desc);
    IncrementOutstandingIO();
    if (fOwner && !isInactive())
    {
		cached = fOwner->GetDevice()->GetFullConfigurationDescriptor(configIndex);
		if ( cached == NULL )
		{
			USBLog(5,"+IOUSBInterfaceUserClientV2[%p]::GetConfigDescriptor GetFullConfigurationDescriptor returned NULL",  this);
			desc = NULL;
			ret = kIOReturnNotFound;
		}
		else
		{
			USBLog(7,"+IOUSBInterfaceUserClientV2[%p]::GetConfigDescriptor  got descriptor %p, length: %d",  this, cached, USBToHostWord(cached->wTotalLength));
			length = USBToHostWord(cached->wTotalLength);
			if (length < *size)
			*size = length;
			bcopy(cached, desc, *size);
			ret = kIOReturnSuccess;
		}
    }
    else
        ret = kIOReturnNotAttached;
	
   	if (ret)
	{
		USBLog(5, "IOUSBInterfaceUserClientV2[%p]::GetConfigDescriptor - returning err 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	
    DecrementOutstandingIO();
	return ret;
}

IOReturn
IOUSBInterfaceUserClientV2::GetConfigDescriptor(UInt8 configIndex, IOMemoryDescriptor * mem, uint32_t *size)
{
    UInt16 				length;
    const IOUSBConfigurationDescriptor	*cached;
    IOReturn				ret;
    
    USBLog(7,"+IOUSBInterfaceUserClientV2[%p]::GetConfigDescriptor > 4K (Config %d), with size %d, mem: %p",  this, configIndex, *size, mem);
	
	if ( mem == NULL )
	{
		USBLog(3,"+IOUSBInterfaceUserClientV2[%p]::GetConfigDescriptor > 4K  mem was NULL, returning kIOReturnBadArgument",  this);
		return kIOReturnBadArgument;
	}
	
    IncrementOutstandingIO();
    if (fOwner && !isInactive())
    {
		cached = fOwner->GetDevice()->GetFullConfigurationDescriptor(configIndex);
		if ( cached == NULL )
		{
			USBLog(5,"+IOUSBInterfaceUserClientV2[%p]::GetConfigDescriptor > 4K GetFullConfigurationDescriptor returned NULL",  this);
			ret = kIOReturnNotFound;
		}
		else
		{
			USBLog(7,"+IOUSBInterfaceUserClientV2[%p]::GetConfigDescriptor > 4K  got descriptor %p, length: %d",  this, cached, USBToHostWord(cached->wTotalLength));
			length = USBToHostWord(cached->wTotalLength);
			if (length < *size)
			{
				*size = length;
			}
			
			mem->prepare();
			mem->writeBytes(0, cached, *size);
			mem->complete();
			
			ret = kIOReturnSuccess;
		}
    }
    else
        ret = kIOReturnNotAttached;
	
   	if (ret)
	{
		USBLog(5, "IOUSBInterfaceUserClientV2[%p]::GetConfigDescriptor - returning err 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	
    DecrementOutstandingIO();
	return ret;
}	



#pragma mark Pipe Methods

//================================================================================================
//
//   _ReadPipe
//
//   This method is called for both sync and async writes.  In the case of async, the parameters
//   will only be 5 scalars.  In the sync case, it will be 3 scalars and, depending on the size,
//   an inputStructure or an inputIOMD.

//================================================================================================
//
IOReturn IOUSBInterfaceUserClientV2::_ReadPipe(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
	IOReturn								ret;
	
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::_ReadPipe",  target);
	// target->PrintExternalMethodArgs(arguments, 5);
	
	if ( arguments->asyncWakePort ) 
	{
		IOUSBCompletion							tap;
		IOUSBUserClientAsyncParamBlock *		pb = (IOUSBUserClientAsyncParamBlock*) IOMalloc(sizeof(IOUSBUserClientAsyncParamBlock));
		
        if (!pb) 
            return kIOReturnNoMemory;
		
		target->retain();
        target->IncrementOutstandingIO();
        
        bcopy(arguments->asyncReference, pb->fAsyncRef, sizeof(OSAsyncReference64));
        pb->fAsyncCount = arguments->asyncReferenceCount;
        
		tap.target = target;
        tap.action = &IOUSBInterfaceUserClientV2::ReqComplete;
        tap.parameter = pb;
		
		ret = target->ReadPipe(	(UInt8) arguments->scalarInput[0],				// pipeRef
							   (UInt32) arguments->scalarInput[1],				// noDataTimeout
							   (UInt32) arguments->scalarInput[2],				// completionTimeout
							   (mach_vm_address_t) arguments->scalarInput[3],	// buffer (in user task)
							   (mach_vm_size_t) arguments->scalarInput[4],		// size of buffer
							   &tap);											// completion
		
        if ( ret ) 
		{
            if ( pb )
				IOFree(pb, sizeof(*pb));
			
			target->DecrementOutstandingIO();
			target->release();
		}
	}
	else 
	{
		target->retain();
		
		if ( arguments->structureOutputDescriptor ) 
		{
			ret = target->ReadPipe(	(UInt8)arguments->scalarInput[0], (UInt32)arguments->scalarInput[1], (UInt32)arguments->scalarInput[2],
								   arguments->structureOutputDescriptor, (IOByteCount *)&(arguments->structureOutputDescriptorSize));
		}
		else
		{
			ret = target->ReadPipe((UInt8)arguments->scalarInput[0], (UInt32)arguments->scalarInput[1], (UInt32)arguments->scalarInput[2],
								   arguments->structureOutput, (UInt32 *)&(arguments->structureOutputSize));
		}
		
		target->release();
	}
	
	return ret;
}  

IOReturn
IOUSBInterfaceUserClientV2::ReadPipe(UInt8 pipeRef, UInt32 noDataTimeout, UInt32 completionTimeout, mach_vm_address_t buffer, mach_vm_size_t size, IOUSBCompletion * completion)
{
	IOReturn				ret = kIOReturnNotAttached;
    IOMemoryDescriptor *	mem = NULL;
    IOUSBPipe *				pipeObj = NULL;
	
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::ReadPipe (Async) (pipeRef: %d, %d, %d, buffer: 0x%qx, size: %qd, completion: %p)",  this, pipeRef, (uint32_t)noDataTimeout, (uint32_t)completionTimeout, buffer, size, completion);
    
	if (fOwner && !isInactive())
    {
		if (completion == NULL)
		{
			USBLog(1,"IOUSBInterfaceUserClientV2[%p]::ReadPipe (async) bad arguments (%qd, %qx, %p)",  this, size, buffer, completion); 
			USBTrace( kUSBTInterfaceUserClient,  kTPInterfaceUCReadPipe, size, (uintptr_t)buffer, (uintptr_t)completion, kIOReturnBadArgument );
			ret = kIOReturnBadArgument;
			goto Exit;
		}
		
		pipeObj = GetPipeObj(pipeRef);
		if (pipeObj)
		{
			// This is an Async request 
			IOUSBUserClientAsyncParamBlock * pb = (IOUSBUserClientAsyncParamBlock *)completion->parameter;
			
			USBLog(7,"IOUSBInterfaceUserClientV2[%p]::ReadPipe (Async) creating IOMD:  buffer: 0x%qx, size: %qd", this, buffer, size); 
			mem = IOMemoryDescriptor::withAddressRange( buffer, size, kIODirectionIn, fTask);
			if (!mem)
			{
				USBLog(1,"IOUSBInterfaceUserClientV2[%p]::ReadPipe (async ) IOMemoryDescriptor::withAddressRange returned NULL",  this); 
				USBTrace( kUSBTInterfaceUserClient,  kTPInterfaceUCReadPipe, (uintptr_t)this, size, (uintptr_t)mem, kIOReturnNoMemory );
				ret = kIOReturnNoMemory;
				goto Exit;
			}
			
			USBLog(7,"IOUSBInterfaceUserClientV2[%p]::ReadPipe (async) created IOMD %p",  this, mem); 
			ret = mem->prepare();
			if ( ret != kIOReturnSuccess)
			{
				USBLog(3,"IOUSBInterfaceUserClientV2[%p]::ReadPipe (async) mem->prepare() returned 0x%x (%s)", this, ret, USBStringFromReturn(ret)); 
				mem->release();
				goto Exit;
			}
			
			pb->fMax = size;
			pb->fMem = mem;
			
			ret = pipeObj->Read(mem, noDataTimeout, completionTimeout, completion, NULL);
			if ( ret != kIOReturnSuccess)
			{
				USBLog(5,"IOUSBInterfaceUserClientV2[%p]::ReadPipe (async) returned 0x%x (%s)", this, ret, USBStringFromReturn(ret)); 
				if (mem != NULL)
				{
					mem->complete();
					mem->release();
				}
			}
		}
		else
		{
			USBLog(5,"IOUSBInterfaceUserClientV2[%p]::ReadPipe (async) can't find pipeRef, returning kIOUSBUnknownPipeErr",  this); 
			ret = kIOUSBUnknownPipeErr;
		}
	}
	
Exit:
	
	if (pipeObj)
		pipeObj->release();
	
    if (ret)
	{
		USBLog(3, "IOUSBInterfaceUserClientV2[%p]::ReadPipe (async - returning err 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	
	return ret;
}	


IOReturn
IOUSBInterfaceUserClientV2::ReadPipe(UInt8 pipeRef, UInt32 noDataTimeout, UInt32 completionTimeout, void *buf, UInt32 *size)
{
    IOReturn				ret = kIOReturnSuccess;
    IOMemoryDescriptor *	mem = NULL;
    IOUSBPipe *				pipeObj;
	
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::ReadPipe < 4K (pipeRef: %d, data timeout: %d, completionTimeout: %d, buf: %p, size: %d)",  this, pipeRef, (uint32_t)noDataTimeout, (uint32_t)completionTimeout, buf, (uint32_t)*size);
    
    IncrementOutstandingIO();				// do this to "hold" ourselves until we complete
    
    if (fOwner && !isInactive())
    {
		pipeObj = GetPipeObj(pipeRef);
		if (pipeObj)
		{
			IOByteCount count = *size;
			mem = IOMemoryDescriptor::withAddress( (void *)buf, *size, kIODirectionIn);
			if (mem)
			{ 
				*size = 0;
				ret = mem->prepare();
				if ( ret == kIOReturnSuccess)
				{
					ret = pipeObj->Read(mem, noDataTimeout, completionTimeout, NULL, &count);
					*size = count;
					mem->complete();
				}
				else
				{
					USBLog(3,"IOUSBInterfaceUserClientV2[%p]::ReadPipe (sync < 4K) mem->prepare() returned 0x%x (%s)", this, ret, USBStringFromReturn(ret)); 
				}
				mem->release();
				mem = NULL;
			}
			else
				ret =  kIOReturnNoMemory;
			pipeObj->release();
		}
		else
			ret = kIOUSBUnknownPipeErr;
    }
    else
        ret = kIOReturnNotAttached;
	
    if (ret)
	{
		USBLog(3, "IOUSBInterfaceUserClientV2[%p]::ReadPipe - returning err %x, size read: %d",  this, ret, (uint32_t)*size);
	}
	
    DecrementOutstandingIO();

    return ret;
}



IOReturn
IOUSBInterfaceUserClientV2::ReadPipe(UInt8 pipeRef, UInt32 noDataTimeout, UInt32 completionTimeout, IOMemoryDescriptor *mem, IOByteCount *bytesRead)
{
    IOReturn				ret = kIOReturnSuccess;
    IOUSBPipe 			*	pipeObj;
	
    USBLog(6, "+IOUSBInterfaceUserClientV2[%p]::ReadPipe > 4K (pipeRef: %d, %d, %d, IOMD: %p)",  this, pipeRef, (uint32_t)noDataTimeout, (uint32_t)completionTimeout, mem);
	
    IncrementOutstandingIO();				// do this to "hold" ourselves until we complete
    
    if (fOwner && !isInactive())
    {
		pipeObj = GetPipeObj(pipeRef);
		if (pipeObj)
		{
			if (mem)
			{
				ret = mem->prepare();
				if (ret == kIOReturnSuccess)
				{
					ret = pipeObj->Read(mem, noDataTimeout, completionTimeout, 0, bytesRead );
					mem->complete();
				}
				else
				{
					USBLog(3,"IOUSBInterfaceUserClientV2[%p]::ReadPipe (sync > 4K) mem->prepare() returned 0x%x (%s)", this, ret, USBStringFromReturn(ret));
				}
			}
			else
			{
				ret = kIOReturnNoMemory;
			}
			pipeObj->release();
		}
		else
			ret = kIOUSBUnknownPipeErr;
    }
    else
        ret = kIOReturnNotAttached;
	
    if (ret)
	{
		USBLog(3, "IOUSBInterfaceUserClientV2[%p]::ReadPipe > 4K - returning err 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	
	DecrementOutstandingIO();
	return ret;
}



//================================================================================================
//
//   _WritePipe
//
//   This method is called for both sync and async writes.  In the case of async, the parameters
//   will only be 5 scalars.  In the sync case, it will be 3 scalars and, depending on the size,
//   an inputStructure or an inputIOMD.

//================================================================================================
//
IOReturn IOUSBInterfaceUserClientV2::_WritePipe(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
	IOReturn								ret;
	
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::_WritePipe",  target);
	
	if ( arguments->asyncWakePort ) 
	{
		IOUSBCompletion							tap;
		IOUSBUserClientAsyncParamBlock *		pb = (IOUSBUserClientAsyncParamBlock*) IOMalloc(sizeof(IOUSBUserClientAsyncParamBlock));
		
        if (!pb) 
            return kIOReturnNoMemory;
		
		target->retain();
        target->IncrementOutstandingIO();
        
        bcopy(arguments->asyncReference, pb->fAsyncRef, sizeof(OSAsyncReference64));
        pb->fAsyncCount = arguments->asyncReferenceCount;
        
		tap.target = target;
        tap.action = &IOUSBInterfaceUserClientV2::ReqComplete;
        tap.parameter = pb;
		
		ret = target->WritePipe(	(UInt8) arguments->scalarInput[0],				// pipeRef
									(UInt32) arguments->scalarInput[1],				// noDataTimeout
									(UInt32) arguments->scalarInput[2],				// completionTimeout
									(mach_vm_address_t) arguments->scalarInput[3],	// buffer (in user task)
									(mach_vm_size_t) arguments->scalarInput[4],		// size of buffer
									&tap);											// completion
		
        if ( ret ) 
		{
            if ( pb )
				IOFree(pb, sizeof(*pb));
			
			target->DecrementOutstandingIO();
			target->release();
		}
	}
	else 
	{
		target->retain();
		
		if ( arguments->structureInputDescriptor ) 
		{
			ret = target->WritePipe(	(UInt8)arguments->scalarInput[0], (UInt32)arguments->scalarInput[1], (UInt32)arguments->scalarInput[2],
									arguments->structureInputDescriptor);
		}
		else
		{
			ret = target->WritePipe((UInt16)arguments->scalarInput[0], (UInt32)arguments->scalarInput[1], (UInt32)arguments->scalarInput[2],
									arguments->structureInput, arguments->structureInputSize);
		}
		
		target->release();
	}
	
	return ret;
}  


IOReturn
IOUSBInterfaceUserClientV2::WritePipe(UInt8 pipeRef, UInt32 noDataTimeout, UInt32 completionTimeout, mach_vm_address_t buffer, mach_vm_size_t size, IOUSBCompletion * completion)
{
   IOReturn					ret = kIOReturnNotAttached;
    IOMemoryDescriptor *	mem = NULL;
    IOUSBPipe *				pipeObj = NULL;
	
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::WritePipe (Async) (pipeRef: %d, %d, %d, buffer: 0x%qx, size: %qd, completion: %p)",  this, pipeRef, (uint32_t)noDataTimeout, (uint32_t)completionTimeout, buffer, size, completion);
    
	if (fOwner && !isInactive())
    {
		if (completion == NULL )
		{
			USBLog(1,"IOUSBInterfaceUserClientV2[%p]::WritePipe (async) bad arguments (%qd, %qx, %p)",  this, size, buffer, completion); 
			USBTrace( kUSBTInterfaceUserClient,  kTPInterfaceUCWritePipe, size, buffer, (uintptr_t)completion, kIOReturnBadArgument );
			ret = kIOReturnBadArgument;
			goto Exit;
		}
		
		pipeObj = GetPipeObj(pipeRef);
		if (pipeObj)
		{
			// This is an Async request 
			IOUSBUserClientAsyncParamBlock * pb = (IOUSBUserClientAsyncParamBlock *)completion->parameter;
			
			mem = IOMemoryDescriptor::withAddressRange( buffer, size, kIODirectionOut, fTask);
			if (!mem)
			{
				USBLog(1,"IOUSBInterfaceUserClientV2[%p]::WritePipe (async ) IOMemoryDescriptor::withAddressRange returned NULL",  this); 
				USBTrace( kUSBTInterfaceUserClient,  kTPInterfaceUCWritePipe, (uintptr_t)this, size, (uintptr_t)mem, kIOReturnNoMemory );
				ret = kIOReturnNoMemory;
				goto Exit;
			}

			USBLog(7,"IOUSBInterfaceUserClientV2[%p]::WritePipe (async) created IOMD %p",  this, mem); 
			ret = mem->prepare();
			if ( ret != kIOReturnSuccess)
			{
				USBLog(3,"IOUSBInterfaceUserClientV2[%p]::WritePipe (async) mem->prepare() returned 0x%x (%s)", this, ret, USBStringFromReturn(ret)); 
				mem->release();
				goto Exit;
			}

			pb->fMax = size;
			pb->fMem = mem;
			
			ret = pipeObj->Write(mem, noDataTimeout, completionTimeout, completion);
			if ( ret != kIOReturnSuccess)
			{
				USBLog(5,"IOUSBInterfaceUserClientV2[%p]::WritePipe (async) returned 0x%x (%s)", this, ret, USBStringFromReturn(ret)); 
				if (mem != NULL)
				{
					mem->complete();
					mem->release();
				}
			}
		}
		else
		{
			USBLog(5,"IOUSBInterfaceUserClientV2[%p]::WritePipe (async) can't find pipeRef, returning kIOUSBUnknownPipeErr",  this); 
			ret = kIOUSBUnknownPipeErr;
		}
	}
	
Exit:
	if (pipeObj)
		pipeObj->release();
	
    if (ret)
	{
		USBLog(3, "IOUSBInterfaceUserClientV2[%p]::WritePipe (async - returning err 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	
	return ret;
}


IOReturn
IOUSBInterfaceUserClientV2::WritePipe(UInt8 pipeRef, UInt32 noDataTimeout, UInt32 completionTimeout, const void *buf, UInt32 size)
{
    IOReturn				ret = kIOReturnSuccess;
    IOMemoryDescriptor *	mem = NULL;
    IOUSBPipe *				pipeObj;
	
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::WritePipe < 4K (pipeRef: %d, %d, %d, buf: %p, size: %d)",  this, pipeRef, (uint32_t)noDataTimeout, (uint32_t)completionTimeout, buf, (uint32_t)size);
    IncrementOutstandingIO();				// do this to "hold" ourselves until we complete
	
    if (fOwner && !isInactive())
    {
		pipeObj = GetPipeObj(pipeRef);
		if (pipeObj)
		{
			// Sync, < 4K
			mem = IOMemoryDescriptor::withAddress( (void *) buf, size, kIODirectionOut);
			if (mem) 
			{
				ret = mem->prepare();
				if ( ret == kIOReturnSuccess)
				{
					ret = pipeObj->Write(mem, noDataTimeout, completionTimeout);
					if ( ret != kIOReturnSuccess)
					{
						USBLog(5,"IOUSBInterfaceUserClientV2[%p]::WritePipe (sync < 4K) returned 0x%x (%s)", this, ret, USBStringFromReturn(ret)); 
					}
					mem->complete();
				}
				else
				{
					USBLog(3,"IOUSBInterfaceUserClientV2[%p]::WritePipe (sync < 4K) mem->prepare() returned 0x%x (%s)", this, ret, USBStringFromReturn(ret)); 
				}
				
				mem->release();
				mem = NULL;
			}
			else
				ret = kIOReturnNoMemory;
			
			pipeObj->release();
		}
		else
			ret = kIOUSBUnknownPipeErr;
    }
    else
        ret = kIOReturnNotAttached;
	
    if (ret)
	{
		USBLog(3, "IOUSBInterfaceUserClientV2[%p]::WritePipe < 4K - returning err 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}

    DecrementOutstandingIO();
    return ret;
}


IOReturn
IOUSBInterfaceUserClientV2::WritePipe(UInt8 pipeRef, UInt32 noDataTimeout, UInt32 completionTimeout, IOMemoryDescriptor *mem)
{
    IOReturn				ret;
    IOUSBPipe 			*	pipeObj;
	
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::WritePipe > 4K (pipeRef: %d, %d, %d, IOMD: %p)",  this, pipeRef, (uint32_t)noDataTimeout, (uint32_t)completionTimeout, mem);
	
	if ( mem == NULL )
	{
		USBLog(3,"+IOUSBInterfaceUserClientV2[%p]::WritePipe > 4K  mem was NULL, returning kIOReturnBadArgument",  this);
		return kIOReturnBadArgument;
	}

    IncrementOutstandingIO();				// do this to "hold" ourselves until we complete
    
    if (fOwner && !isInactive())
    {
		pipeObj = GetPipeObj(pipeRef);
		if (pipeObj)
		{
			if (mem)
			{
				ret = mem->prepare();
				if (ret == kIOReturnSuccess)
				{
					ret = pipeObj->Write(mem, noDataTimeout, completionTimeout );
					mem->complete();
				}
				else
				{
					USBLog(1,"IOUSBInterfaceUserClientV2[%p]::WritePipe > (sync > 4K) mem->prepare() returned 0x%x (%s)", this, ret, USBStringFromReturn(ret)); 
					USBTrace( kUSBTInterfaceUserClient,  kTPInterfaceUCWritePipe, (uintptr_t)this, ret, 0, 0 );
				}
			}
			else
			{
				ret = kIOReturnNoMemory;
			}
			pipeObj->release();
		}
		else
			ret = kIOUSBUnknownPipeErr;
    }
    else
        ret = kIOReturnNotAttached;
	
    if (ret)
	{
		USBLog(3, "IOUSBInterfaceUserClientV2[%p]::WritePipe > 4K - returning err 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	
    DecrementOutstandingIO();
    return ret;
}


IOReturn IOUSBInterfaceUserClientV2::_GetPipeProperties(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::_GetPipeProperties",  target);
	
	target->retain();
    IOReturn kr = target->GetPipeProperties((UInt8)arguments->scalarInput[0], &(arguments->scalarOutput[0]),&(arguments->scalarOutput[1]), 
										&(arguments->scalarOutput[2]), &(arguments->scalarOutput[3]), &(arguments->scalarOutput[4]));
	target->release();
	
	return kr;
}

IOReturn
IOUSBInterfaceUserClientV2::GetPipeProperties(UInt8 pipeRef, uint64_t *direction, uint64_t *number, uint64_t *transferType, uint64_t *maxPacketSize, uint64_t *interval)
{
    IOUSBPipe 			*pipeObj;
    IOReturn			ret = kIOReturnSuccess;
	
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::GetPipeProperties for pipe %d, isInactive: %d",  this, pipeRef, isInactive());
	
	IncrementOutstandingIO();
    if (fOwner && !isInactive())
    {
		pipeObj = GetPipeObj(pipeRef);    
		if (pipeObj)
		{
			if (direction)
				*direction = pipeObj->GetDirection();
			if (number)
				*number = pipeObj->GetEndpointNumber();
			if (transferType)
				*transferType = pipeObj->GetType();
			if (maxPacketSize)
				*maxPacketSize = pipeObj->GetMaxPacketSize();
			if (interval)
				*interval = pipeObj->GetInterval();
			pipeObj->release();
		}
		else
			ret = kIOUSBUnknownPipeErr;
    }
    else
        ret = kIOReturnNotAttached;
	
    if (ret)
	{
		USBLog(3, "IOUSBInterfaceUserClientV2[%p]::GetPipeProperties - returning err 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	
    DecrementOutstandingIO();
    return ret;
}


IOReturn IOUSBInterfaceUserClientV2::_GetPipeStatus(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::_GetPipeStatus",  target);

	target->retain();
	IOReturn kr = target->GetPipeStatus((UInt8)arguments->scalarInput[0]);
	target->release();
	
	return kr;
}


IOReturn 
IOUSBInterfaceUserClientV2::GetPipeStatus(UInt8 pipeRef)
{
    IOUSBPipe 		*pipeObj;
    IOReturn		ret;
    
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::GetPipeStatus",  this);
    
    IncrementOutstandingIO();
	
    if (fOwner && !isInactive())
    {
		pipeObj = GetPipeObj(pipeRef);
		if (pipeObj)
		{
			ret = pipeObj->GetPipeStatus();
			pipeObj->release();
		}
		else
			ret = kIOUSBUnknownPipeErr;
    }
    else
        ret = kIOReturnNotAttached;
    
    DecrementOutstandingIO();
    return ret;
}


IOReturn IOUSBInterfaceUserClientV2::_AbortPipe(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::_AbortPipe",  target);
	
	target->retain();
    IOReturn kr = target->AbortPipe((UInt8)arguments->scalarInput[0]);
	target->release();
	
	return kr;
}

IOReturn 
IOUSBInterfaceUserClientV2::AbortPipe(UInt8 pipeRef)
{
    IOUSBPipe 		*pipeObj;
    IOReturn		ret;
    
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::AbortPipe",  this);
    
    IncrementOutstandingIO();
    
    if (fOwner && !isInactive())
    {
		pipeObj = GetPipeObj(pipeRef);
		if (pipeObj)
		{
			ret =  pipeObj->Abort();
			pipeObj->release();
		}
		else
			ret = kIOUSBUnknownPipeErr;
    }
    else
        ret = kIOReturnNotAttached;
	
    if (ret)
    {
        if ( ret == kIOUSBUnknownPipeErr || ret == kIOReturnNotAttached)
		{
            USBLog(6, "IOUSBInterfaceUserClientV2[%p]::AbortPipe(%d) - returning err %x (%s)",  this, pipeRef, ret, USBStringFromReturn(ret));
		}
        else
		{
            USBLog(3, "IOUSBInterfaceUserClientV2[%p]::AbortPipe(%d) - returning err %x (%s)",  this, pipeRef, ret, USBStringFromReturn(ret));
		}
    }
    
    DecrementOutstandingIO();
    return ret;
}



IOReturn IOUSBInterfaceUserClientV2::_ResetPipe(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::_ResetPipe",  target);
	
	target->retain();
    IOReturn kr = target->ResetPipe((UInt8)arguments->scalarInput[0]);
	target->release();
	
	return kr;
}

IOReturn 
IOUSBInterfaceUserClientV2::ResetPipe(UInt8 pipeRef)
{
    IOUSBPipe 			*pipeObj;
    IOReturn			ret;
	
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::ResetPipe",  this);
	
    IncrementOutstandingIO();
	
    if (fOwner && !isInactive())
    {
		pipeObj = GetPipeObj(pipeRef);
		if (pipeObj)
		{
			ret = pipeObj->Reset();
			pipeObj->release();
		}
		else
			ret = kIOUSBUnknownPipeErr;
    }
    else
        ret = kIOReturnNotAttached;
	
    if (ret)
	{
		USBLog(3, "IOUSBInterfaceUserClientV2[%p]::ResetPipe - returning err 0x%x (%s)",  this, ret, USBStringFromReturn(ret));
    }
	
    DecrementOutstandingIO();
    return ret;
}



IOReturn IOUSBInterfaceUserClientV2::_ClearPipeStall(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::_ClearPipeStall",  target);
	
	target->retain();
    IOReturn kr = target->ClearPipeStall((UInt8)arguments->scalarInput[0], (bool)arguments->scalarInput[1]);
	target->release();
	
	return kr;
}

IOReturn 
IOUSBInterfaceUserClientV2::ClearPipeStall(UInt8 pipeRef, bool bothEnds)
{
    IOUSBPipe 		*pipeObj;
    IOReturn		ret;
	
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::ClearPipeStall (pipe: %d, bothEnds: %d)",  this, pipeRef, bothEnds);
	
    IncrementOutstandingIO();
    
    if (fOwner && !isInactive())
    {
		pipeObj = GetPipeObj(pipeRef);
		if (pipeObj)
		{
			USBLog(6, "IOUSBInterfaceUserClientV2[%p]::ClearPipeStall = bothEnds = %d",  this, bothEnds);
			ret = pipeObj->ClearPipeStall(bothEnds);
			pipeObj->release();
		}
		else
			ret = kIOUSBUnknownPipeErr;
    }
    else
        ret = kIOReturnNotAttached;
	
    if (ret)
	{
		USBLog(3, "IOUSBInterfaceUserClientV2[%p]::ClearPipeStall - returning err 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	
    DecrementOutstandingIO();
    return ret;
}



IOReturn IOUSBInterfaceUserClientV2::_SetPipePolicy(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::_SetPipePolicy",  target);
	
	target->retain();
    IOReturn kr = target->SetPipePolicy((UInt8)arguments->scalarInput[0], (UInt16)arguments->scalarInput[1], (UInt8)arguments->scalarInput[2]);
	target->release();
	
	return kr;
}

IOReturn 
IOUSBInterfaceUserClientV2::SetPipePolicy(UInt8 pipeRef, UInt16 maxPacketSize, UInt8 maxInterval)
{
    IOUSBPipe 		*pipeObj;
    IOReturn		ret;
	
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::SetPipePolicy",  this);
	
    IncrementOutstandingIO();
    
    if (fOwner && !isInactive())
    {
		pipeObj = GetPipeObj(pipeRef);
		if (pipeObj)
		{
			USBLog(7, "IOUSBInterfaceUserClientV2[%p]::SetPipePolicy(%d, %d)",  this, maxPacketSize, maxInterval);
			ret = pipeObj->SetPipePolicy(maxPacketSize, maxInterval);
			pipeObj->release();
		}
		else
			ret = kIOUSBUnknownPipeErr;
    }
    else
        ret = kIOReturnNotAttached;
	
    if (ret)
	{
		USBLog(3, "IOUSBInterfaceUserClientV2[%p]::SetPipePolicy - returning err 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	
	DecrementOutstandingIO();
	return ret;
}



#pragma mark Control Request Out

IOReturn 
IOUSBInterfaceUserClientV2::_ControlRequestOut(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
	IOReturn								ret;
	
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::_ControlRequestOut",  target);
	// target->PrintExternalMethodArgs(arguments, 5);
	
	
	if ( arguments->asyncWakePort ) 
	{
		IOUSBCompletion						tap;
		IOUSBUserClientAsyncParamBlock *	pb = (IOUSBUserClientAsyncParamBlock*) IOMalloc(sizeof(IOUSBUserClientAsyncParamBlock));
		
        if (!pb) 
            return kIOReturnNoMemory;
		
		target->retain();
        target->IncrementOutstandingIO();
        
        bcopy(arguments->asyncReference, pb->fAsyncRef, sizeof(OSAsyncReference64));
        pb->fAsyncCount = arguments->asyncReferenceCount;
        
		tap.target = target;
        tap.action = &IOUSBInterfaceUserClientV2::ReqComplete;
        tap.parameter = pb;
		
 		// ControlRequestIn, Async, buffer in  clients task
		ret = target->ControlRequestOut(	(UInt8)arguments->scalarInput[0],			// pipeRef
											(UInt8)arguments->scalarInput[1],			// bmRequestType,
											(UInt8)arguments->scalarInput[2],			// bRequest,
											(UInt16)arguments->scalarInput[3],			// wValue,
											(UInt16)arguments->scalarInput[4],			// wIndex,
											(mach_vm_size_t)arguments->scalarInput[5],		// wLength (bufferSize),
											(mach_vm_address_t)arguments->scalarInput[6],	// pData (buffer),
											(UInt32)arguments->scalarInput[7],			// noDataTimeout,
											(UInt32)arguments->scalarInput[8],			// completionTimeout,
											&tap);										// completion
		
		if ( ret ) 
		{
            if ( pb )
				IOFree(pb, sizeof(*pb));
			
			target->DecrementOutstandingIO();
			target->release();
		}
	}
	else 
	{
		target->retain();
		
		if ( arguments->structureInputDescriptor ) 
		{
			// ControlRequestOut, Sync, > 4K
			ret = target->ControlRequestOut(	(UInt8)arguments->scalarInput[0],			// pipeRef
											(UInt8)arguments->scalarInput[1],			// bmRequestType,
											(UInt8)arguments->scalarInput[2],			// bRequest,
											(UInt16)arguments->scalarInput[3],			// wValue,
											(UInt16)arguments->scalarInput[4],			// wIndex,
											(UInt32)arguments->scalarInput[7],			// noDataTimeout,
											(UInt32)arguments->scalarInput[8],			// completionTimeout,
											arguments->structureInputDescriptor);		// IOMD for request
		}
		else
		{
			// ControlRequestOut, Sync, < 4K
			ret = target->ControlRequestOut(	(UInt8)arguments->scalarInput[0],			// pipeRef
											(UInt8)arguments->scalarInput[1],			// bmRequestType,
											(UInt8)arguments->scalarInput[2],			// bRequest,
											(UInt16)arguments->scalarInput[3],			// wValue,
											(UInt16)arguments->scalarInput[4],			// wIndex,
											(UInt32)arguments->scalarInput[7],			// noDataTimeout,
											(UInt32)arguments->scalarInput[8],			// completionTimeout,
											arguments->structureInput,					// bufferPtr
											arguments->structureInputSize);				// buffer size
		}
		
		target->release();
	}
	
	return ret;
	
}  


// This is an Async
IOReturn
IOUSBInterfaceUserClientV2::ControlRequestOut(UInt8 pipeRef, UInt8 bmRequestType,  UInt8 bRequest, UInt16 wValue, UInt16 wIndex, mach_vm_size_t size, mach_vm_address_t buffer, UInt32 noDataTimeout, UInt32 completionTimeout, IOUSBCompletion * completion)
{
	IOReturn				ret = kIOReturnSuccess;
	IOMemoryDescriptor *	mem = NULL;
	IOUSBPipe *				pipeObj = NULL;;
	
	USBLog(7, "IOUSBInterfaceUserClientV2::ControlRequestOut (Async) : bmRequestType = 0x%2.2x bRequest = 0x%2.2x wValue = 0x%4.4x wIndex = 0x%4.4x, noDataTimeout = %d completionTimeout = %d",
		   bmRequestType,
		   bRequest,
		   wValue,
		   wIndex,
		   (uint32_t)noDataTimeout,
		   (uint32_t)completionTimeout);
	
	if (fOwner && !isInactive())
    {
		if (completion == NULL)
		{
			USBLog(1,"IOUSBInterfaceUserClientV2[%p]::ControlRequestOut (async) completion is NULL!",  this);
			USBTrace( kUSBTInterfaceUserClient,  kTPInterfaceUCControlRequestOut, bmRequestType, bRequest, wValue, kIOReturnBadArgument );
			ret = kIOReturnBadArgument;
			goto Exit;
		}
		
		pipeObj = GetPipeObj(pipeRef);
		if (pipeObj)
		{
			// This is an Async request 
			IOUSBUserClientAsyncParamBlock * pb = (IOUSBUserClientAsyncParamBlock *)completion->parameter;
			
			if (size != 0 )
			{
				USBLog(7,"IOUSBInterfaceUserClientV2[%p]::ControlRequestOut (async) creating IOMD",  this); 
				mem = IOMemoryDescriptor::withAddressRange( buffer, size, kIODirectionOut, fTask);
				if (!mem)
				{
					USBLog(1,"IOUSBInterfaceUserClientV2[%p]::ControlRequestOut (async ) IOMemoryDescriptor::withAddressRange returned NULL",  this); 
					USBTrace( kUSBTInterfaceUserClient,  kTPInterfaceUCControlRequestOut, (uintptr_t)this, (uintptr_t)mem, size, kIOReturnNoMemory );
					ret = kIOReturnNoMemory;
					goto Exit;
				}
				
				ret = mem->prepare();
				if ( ret != kIOReturnSuccess)
				{
					USBLog(3,"IOUSBInterfaceUserClientV2[%p]::ControlRequestOut (async) mem->prepare() returned 0x%x (%s)", this, ret, USBStringFromReturn(ret)); 
					mem->release();
					goto Exit;
				}
			}
			
			pb->req.bmRequestType = bmRequestType;
			pb->req.bRequest = bRequest;
			pb->req.wValue = wValue;
			pb->req.wIndex = wIndex;
			pb->req.wLength = size;
			pb->req.pData = mem;
			
			pb->fMax = size;
			pb->fMem = mem;
			
			ret = pipeObj->ControlRequest(&pb->req, noDataTimeout, completionTimeout, completion);
			if ( ret != kIOReturnSuccess)
			{
				USBLog(5,"IOUSBInterfaceUserClientV2[%p]::ControlRequestOut (async) returned 0x%x (%s)", this, ret, USBStringFromReturn(ret)); 
				if (mem != NULL)
				{
					mem->complete();
					mem->release();
				}
			}
		}
		else
		{
			USBLog(5,"IOUSBInterfaceUserClientV2[%p]::ControlRequestOut (async) can't find pipeRef, returning kIOUSBUnknownPipeErr",  this); 
			ret = kIOUSBUnknownPipeErr;
		}
	}
	
Exit:
	if (pipeObj)
		pipeObj->release();
	
    if (ret)
	{
		USBLog(3, "IOUSBInterfaceUserClientV2[%p]::ControlRequestOut (async) - returning err 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	
	return ret;
}


// This is an async/sync with < 4K request
IOReturn
IOUSBInterfaceUserClientV2::ControlRequestOut(UInt8 pipeRef, UInt8 bmRequestType,  UInt8 bRequest, UInt16 wValue, UInt16 wIndex, UInt32 noDataTimeout, UInt32 completionTimeout, const void *requestBuffer, uint32_t size)
{
	IOReturn				ret = kIOReturnSuccess;
	IOUSBDevRequest			req;
	IOUSBPipe		*		pipeObj = NULL;
	
	USBLog(7, "IOUSBInterfaceUserClientV2::ControlRequestOut < 4K : bmRequestType = 0x%2.2x bRequest = 0x%2.2x wValue = 0x%4.4x wIndex = 0x%4.4x wLength = 0x%4.4x noDataTimeout = %d completionTimeout = %d",
		   bmRequestType,
		   bRequest,
		   wValue,
		   wIndex,
		   size,
		   (uint32_t)noDataTimeout,
		   (uint32_t)completionTimeout);
	
	IncrementOutstandingIO();
	
	if (fOwner && !isInactive())
	{
		pipeObj = GetPipeObj(pipeRef);
		if (pipeObj)
		{
			// This is a sync request that is < 4K
			req.bmRequestType = bmRequestType;
			req.bRequest = bRequest;
			req.wValue = wValue;
			req.wIndex = wIndex;
			req.wLength = size;
			req.pData = (void *)requestBuffer;
			req.wLenDone = 0;
			
			ret = pipeObj->ControlRequest(&req, noDataTimeout, completionTimeout);
			
			pipeObj->release();
		}
		else
			ret = kIOUSBUnknownPipeErr;
	}
	else
		ret = kIOReturnNotAttached;
	
	if (ret)
	{
		USBLog(3, "IOUSBInterfaceUserClientV2[%p]::ControlRequest - returning err 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	
	DecrementOutstandingIO();
	return ret;
}

//  This is a ControlRequestOut (sync) that is OOL, so it has an IOMD
IOReturn
IOUSBInterfaceUserClientV2::ControlRequestOut(UInt8 pipeRef, UInt8 bmRequestType,  UInt8 bRequest, UInt16 wValue, UInt16 wIndex, UInt32 noDataTimeout, UInt32 completionTimeout, IOMemoryDescriptor *mem)
{
    IOUSBDevRequestDesc		req;
	IOReturn				ret;
    IOUSBPipe *				pipeObj = NULL;
	
	USBLog(7, "IOUSBInterfaceUserClientV2::ControlRequestOut > 4K: bmRequestType = 0x%2.2x bRequest = 0x%2.2x wValue = 0x%4.4x wIndex = 0x%4.4x wLength = 0x%4.4qx noDataTimeout = %d completionTimeout = %d",
		   bmRequestType,
		   bRequest,
		   wValue,
		   wIndex,
		   (uint64_t)mem->getLength(),
		   (uint32_t)noDataTimeout,
		   (uint32_t)completionTimeout);
	
	if ( mem == NULL )
	{
		USBLog(3,"+IOUSBInterfaceUserClientV2[%p]::ControlRequestOut > 4K  mem was NULL, returning kIOReturnBadArgument",  this);
		return kIOReturnBadArgument;
	}

    IncrementOutstandingIO();
    if (fOwner && !isInactive())
    {
		pipeObj = GetPipeObj(pipeRef);
		if (pipeObj)
		{
			ret = mem->prepare();
			
			if (ret == kIOReturnSuccess)
			{
				req.bmRequestType = bmRequestType;
				req.bRequest = bRequest;
				req.wValue = wValue;
				req.wIndex = wIndex;
				req.wLength = mem->getLength();
				req.pData = mem;
				req.wLenDone = 0;
				
				ret = pipeObj->ControlRequest(&req, noDataTimeout, completionTimeout);
				
				mem->complete();
			}
			else
			{
				USBLog(4,"IOUSBInterfaceUserClientV2[%p]::ControlRequestOut > 4K mem->prepare() returned 0x%x (%s)", this, ret, USBStringFromReturn(ret)); 
			}
			
			pipeObj->release();
		}
		else
			ret = kIOUSBUnknownPipeErr;
	}
    else
        ret = kIOReturnNotAttached;
	
	if (ret)
	{
		USBLog(3, "IOUSBInterfaceUserClientV2[%p]::ControlRequestOut (sync OOL) - returning err 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	
	DecrementOutstandingIO();
	return ret;
}



#pragma mark Control Request In

IOReturn IOUSBInterfaceUserClientV2::_ControlRequestIn(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
	IOReturn								ret;
	
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::_ControlRequestIn",  target);
	// target->PrintExternalMethodArgs(arguments, 5);

	if ( arguments->asyncWakePort ) 
	{
		IOUSBCompletion							tap;
		IOUSBUserClientAsyncParamBlock *	pb = (IOUSBUserClientAsyncParamBlock*) IOMalloc(sizeof(IOUSBUserClientAsyncParamBlock));
		
        if (!pb) 
            return kIOReturnNoMemory;
		
		target->retain();
        target->IncrementOutstandingIO();
        
        bcopy(arguments->asyncReference, pb->fAsyncRef, sizeof(OSAsyncReference64));
        pb->fAsyncCount = arguments->asyncReferenceCount;
        
		tap.target = target;
        tap.action = &IOUSBInterfaceUserClientV2::ReqComplete;
        tap.parameter = pb;
		
		// ControlRequestIn, Async, buffer in  clients task
		ret = target->ControlRequestIn(	(UInt8)arguments->scalarInput[0],			// pipeRef
										(UInt8)arguments->scalarInput[1],			// bmRequestType,
										(UInt8)arguments->scalarInput[2],			// bRequest,
										(UInt16)arguments->scalarInput[3],			// wValue,
										(UInt16)arguments->scalarInput[4],			// wIndex,
										(mach_vm_size_t)arguments->scalarInput[5],	// pData (buffer),
										(mach_vm_address_t)arguments->scalarInput[6],	// wLength (bufferSize),
										(UInt32)arguments->scalarInput[7],			// noDataTimeout,
										(UInt32)arguments->scalarInput[8],			// completionTimeout,
										&tap);										// completion
		
        if ( ret ) 
		{
            if ( pb )
				IOFree(pb, sizeof(*pb));
			
			target->DecrementOutstandingIO();
			target->release();
		}
	}
	else 
	{
		target->retain();
		
		if ( arguments->structureOutputDescriptor ) 
		{
			// ControlRequestIn, Sync, > 4K
			ret = target->ControlRequestIn(	(UInt8)arguments->scalarInput[0],			// pipeRef
										   (UInt8)arguments->scalarInput[1],			// bmRequestType,
										   (UInt8)arguments->scalarInput[2],			// bRequest,
										   (UInt16)arguments->scalarInput[3],			// wValue,
										   (UInt16)arguments->scalarInput[4],			// wIndex,
										   (UInt32)arguments->scalarInput[7],			// noDataTimeout,
										   (UInt32)arguments->scalarInput[8],			// completionTimeout,
										   arguments->structureOutputDescriptor,		// IOMD for request
										   &(arguments->structureOutputDescriptorSize));// IOMD size
		}
		else
		{
			// ControlRequestIn, Sync, < 4K
			ret = target->ControlRequestIn(	(UInt8)arguments->scalarInput[0],			// pipeRef
										   (UInt8)arguments->scalarInput[1],			// bmRequestType,
										   (UInt8)arguments->scalarInput[2],			// bRequest,
										   (UInt16)arguments->scalarInput[3],			// wValue,
										   (UInt16)arguments->scalarInput[4],			// wIndex,
										   (UInt32)arguments->scalarInput[7],			// noDataTimeout,
										   (UInt32)arguments->scalarInput[8],			// completionTimeout,
										   arguments->structureOutput,					// bufferPtr
										   &(arguments->structureOutputSize));			// buffer size
		}
		
		// If asked, and if we got an overrun, send a flag back and squash overrun status.
		if (arguments->scalarOutputCount > 0)
		{
			if (ret == kIOReturnOverrun)
			{
				USBLog(3, "+IOUSBInterfaceUserClientV2[%p]::_DeviceRequestIn kIOReturnOverrun",  target);
				arguments->scalarOutput[0] = 1;
				ret = kIOReturnSuccess;
			}
			else 
			{
				arguments->scalarOutput[0] = 0;
			}
		}
		
		target->release();
	}
	
	return ret;
}  



// This is an Async
IOReturn
IOUSBInterfaceUserClientV2::ControlRequestIn(UInt8 pipeRef, UInt8 bmRequestType,  UInt8 bRequest, UInt16 wValue, UInt16 wIndex, mach_vm_size_t size, mach_vm_address_t buffer, UInt32 noDataTimeout, UInt32 completionTimeout, IOUSBCompletion * completion)
{
	IOReturn				ret = kIOReturnSuccess;
	IOMemoryDescriptor *	mem = NULL;
	IOUSBPipe *				pipeObj = NULL;;

	USBLog(7, "IOUSBInterfaceUserClientV2::ControlRequestIn (Async) : bmRequestType = 0x%2.2x bRequest = 0x%2.2x wValue = 0x%4.4x wIndex = 0x%4.4x, noDataTimeout = %d completionTimeout = %d",
		   bmRequestType,
		   bRequest,
		   wValue,
		   wIndex,
		   (uint32_t)noDataTimeout,
		   (uint32_t)completionTimeout);
	
	if (fOwner && !isInactive())
    {
		if (completion == NULL)
		{
			USBLog(1,"IOUSBInterfaceUserClientV2[%p]::ControlRequestIn (async) bad arguments (%qd, %qx, %p)",  this, size, buffer, completion); 
			USBTrace( kUSBTInterfaceUserClient,  kTPInterfaceUCControlRequestIn, size, (uintptr_t)buffer, (uintptr_t)completion, kIOReturnBadArgument );
			ret = kIOReturnBadArgument;
			goto Exit;
		}
		
		pipeObj = GetPipeObj(pipeRef);
		if (pipeObj)
		{
			// This is an Async request 
			IOUSBUserClientAsyncParamBlock * pb = (IOUSBUserClientAsyncParamBlock *)completion->parameter;
			
			if ( size != 0)
			{
				USBLog(7,"IOUSBInterfaceUserClientV2[%p]::ControlRequestIn (async) creating IOMD",  this); 
				mem = IOMemoryDescriptor::withAddressRange( buffer, size, kIODirectionIn, fTask);
				if (!mem)
				{
					USBLog(1,"IOUSBInterfaceUserClientV2[%p]::ControlRequestIn (async ) IOMemoryDescriptor::withAddressRange returned NULL",  this); 
					USBTrace( kUSBTInterfaceUserClient,  kTPInterfaceUCControlRequestIn, size, (uintptr_t)buffer, (uintptr_t)mem, kIOReturnNoMemory );
					ret = kIOReturnNoMemory;
					goto Exit;
				}
				
				ret = mem->prepare();
				if ( ret != kIOReturnSuccess)
				{
					USBLog(3,"IOUSBInterfaceUserClientV2[%p]::ControlRequestIn (async) mem->prepare() returned 0x%x (%s)", this, ret, USBStringFromReturn(ret)); 
					mem->release();
					goto Exit;
				}
			}
			
			pb->req.bmRequestType = bmRequestType;
			pb->req.bRequest = bRequest;
			pb->req.wValue = wValue;
			pb->req.wIndex = wIndex;
			pb->req.wLength = size;
			pb->req.pData = mem;
			
			pb->fMax = size;
			pb->fMem = mem;
			
			ret = pipeObj->ControlRequest(&pb->req, noDataTimeout, completionTimeout, completion);
			if ( ret != kIOReturnSuccess)
			{
				USBLog(5,"IOUSBInterfaceUserClientV2[%p]::ControlRequestIn (async) returned 0x%x (%s)", this, ret, USBStringFromReturn(ret)); 
				if (mem != NULL)
				{
					mem->complete();
					mem->release();
				}
			}
		}
		else
		{
			USBLog(5,"IOUSBInterfaceUserClientV2[%p]::ControlRequestIn (async) can't find pipeRef, returning kIOUSBUnknownPipeErr",  this); 
			ret = kIOUSBUnknownPipeErr;
		}
	}
	
Exit:
	
	if (pipeObj)
		pipeObj->release();
	
    if (ret)
	{
		USBLog(3, "IOUSBInterfaceUserClientV2[%p]::ControlRequestIn (async) - returning err 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	
	return ret;
}

	// This is an sync with < 4K request
IOReturn
IOUSBInterfaceUserClientV2::ControlRequestIn(UInt8 pipeRef, UInt8 bmRequestType,  UInt8 bRequest, UInt16 wValue, UInt16 wIndex, UInt32 noDataTimeout, UInt32 completionTimeout, void *requestBuffer, uint32_t *size)
{
	IOReturn				ret = kIOReturnSuccess;
	IOUSBDevRequest			req;
	IOUSBPipe *				pipeObj = NULL;;
	
	USBLog(7, "IOUSBInterfaceUserClientV2::ControlRequestIn < 4K : bmRequestType = 0x%2.2x bRequest = 0x%2.2x wValue = 0x%4.4x wIndex = 0x%4.4x wLength = 0x%4.4x noDataTimeout = %d completionTimeout = %d",
		   bmRequestType,
		   bRequest,
		   wValue,
		   wIndex,
		   *size,
		   (uint32_t)noDataTimeout,
		   (uint32_t)completionTimeout);
	
	IncrementOutstandingIO();
	
	if (fOwner && !isInactive())
	{
		pipeObj = GetPipeObj(pipeRef);
		if (pipeObj)
		{
			// This is a sync request that is < 4K
			req.bmRequestType = bmRequestType;
			req.bRequest = bRequest;
			req.wValue = wValue;
			req.wIndex = wIndex;
			req.wLength = *size;
			req.pData = requestBuffer;
			req.wLenDone = 0;
			
			ret = pipeObj->ControlRequest(&req, noDataTimeout, completionTimeout);
			USBLog(7, "IOUSBInterfaceUserClientV2[%p]::ControlRequestIn (sync < 4k) err:0x%x, wLenDone = %d",  this, ret, (uint32_t)req.wLenDone);
		
			if ( (ret == kIOReturnSuccess) || (ret == kIOReturnOverrun) )		
			{
				*size = req.wLenDone;
			}
			else 
			{
				USBLog(3, "IOUSBInterfaceUserClientV2[%p]::ControlRequestIn (sync < 4k) err:0x%x (%s)", this, ret, USBStringFromReturn(ret));
				*size = 0;
			}
			pipeObj->release();
		}
		else
			ret = kIOUSBUnknownPipeErr;
	}
	else
		ret = kIOReturnNotAttached;
	
	if (ret)
	{
		USBLog(3, "IOUSBInterfaceUserClientV2[%p]::ControlRequestIn < 4K - returning err %x, size: %d",  this, ret, *size);
	}
	
	DecrementOutstandingIO();
	return ret;
}

//  This is a ControlRequestIn, Sync that is OOL, so it has an IOMD
IOReturn
IOUSBInterfaceUserClientV2::ControlRequestIn(UInt8 pipeRef, UInt8 bmRequestType,  UInt8 bRequest, UInt16 wValue, UInt16 wIndex, UInt32 noDataTimeout, UInt32 completionTimeout, IOMemoryDescriptor *mem, uint32_t *pOutSize)
{
	IOUSBDevRequestTO		reqIn;
    IOUSBDevRequestDesc		req;
	IOReturn				ret;
    IOUSBPipe *				pipeObj = NULL;
	
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::ControlRequestIn > 4K ",  this);

	// Copy the parameters to the reqIn
	reqIn.bmRequestType = bmRequestType;
	reqIn.bRequest = bRequest;
	reqIn.wValue = wValue;
	reqIn.wIndex = wIndex;
	reqIn.wLength = *pOutSize;
	reqIn.noDataTimeout = noDataTimeout;
	reqIn.completionTimeout = completionTimeout;
	
	USBLog(7, "IOUSBInterfaceUserClientV2::ControlRequestIn > 4K: bmRequestType = 0x%2.2x bRequest = 0x%2.2x wValue = 0x%4.4x wIndex = 0x%4.4x wLength = 0x%4.4x noDataTimeout = %d completionTimeout = %d",
				reqIn.bmRequestType,
				reqIn.bRequest,
				reqIn.wValue,
				reqIn.wIndex,
				reqIn.wLength,
				(uint32_t)reqIn.noDataTimeout,
				(uint32_t)reqIn.completionTimeout);
	
	if ( mem == NULL )
	{
		USBLog(3,"+IOUSBInterfaceUserClientV2[%p]::ControlRequestIn > 4K  mem was NULL, returning kIOReturnBadArgument",  this);
		return kIOReturnBadArgument;
	}
 
    IncrementOutstandingIO();
    if (fOwner && !isInactive())
    {
		pipeObj = GetPipeObj(pipeRef);
		if (pipeObj)
		{
			ret = mem->prepare();
			
			if (ret == kIOReturnSuccess)
			{
				req.bmRequestType = reqIn.bmRequestType;
				req.bRequest = reqIn.bRequest;
				req.wValue = reqIn.wValue;
				req.wIndex = reqIn.wIndex;
				req.wLength = reqIn.wLength;
				req.pData = mem;
				req.wLenDone = 0;
				
				ret = pipeObj->ControlRequest(&req, reqIn.noDataTimeout, reqIn.completionTimeout);
				
				if ( (ret == kIOReturnSuccess) || (ret == kIOReturnOverrun) )		
					*pOutSize = req.wLenDone;
				else 
					*pOutSize = 0;
				
				mem->complete();
			}
			else
			{
				USBLog(4,"IOUSBInterfaceUserClientV2[%p]::ControlRequestIn > 4K mem->prepare() returned 0x%x (%s)", this, ret, USBStringFromReturn(ret)); 
			}
			pipeObj->release();
		}
		else
			ret = kIOUSBUnknownPipeErr;
	}
    else
        ret = kIOReturnNotAttached;
	
	if (ret)
	{
		USBLog(3, "IOUSBInterfaceUserClientV2[%p]::ControlRequestIn > 4K - returning err %x, outSize: %d",  this, ret, *pOutSize);
	}
	
	DecrementOutstandingIO();
	return ret;
}


#pragma mark Isoch Methods


IOReturn IOUSBInterfaceUserClientV2::_ReadIsochPipe(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
	IOReturn								ret = kIOReturnUnsupported;
	
	// This call will always be Async and < 4K (the size of the IOUSBIsocStruct.  The call itself will need to map the buffers
	
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::_ReadIsochPipe",  target);
	// target->PrintExternalMethodArgs(arguments, 5);
	
	if ( arguments->asyncWakePort ) 
	{
		target->retain();
        target->IncrementOutstandingIO();
        
        if ( arguments->structureInputDescriptor ) 
		{
			USBLog(3, "+IOUSBInterfaceUserClientV2[%p]::__ReadIsochPipe  We got an unexpected structureOutputDescriptor.  Returning bad argument ",  target);
            ret = kIOReturnBadArgument;
		}
        else
		{
			IOUSBIsocStructV3		isocData;
			
			isocData.fPipe = arguments->scalarInput[0];
			isocData.fBuffer = (mach_vm_address_t)arguments->scalarInput[1];
			isocData.fBufSize = (mach_vm_size_t)arguments->scalarInput[2];
			isocData.fStartFrame = arguments->scalarInput[3];
			isocData.fNumFrames = arguments->scalarInput[4];
			isocData.fFrameListPtr = (mach_vm_address_t) arguments->scalarInput[5];
			
			ret = target->DoIsochPipeAsync(&isocData, arguments->asyncReference, arguments->asyncReferenceCount, kIODirectionIn);
		}
        if ( ret ) 
		{
			target->DecrementOutstandingIO();
			target->release();
		}
	}
	
	return ret;
}  


IOReturn IOUSBInterfaceUserClientV2::_WriteIsochPipe(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
	IOReturn								ret = kIOReturnUnsupported;
	
	// This call will always be Async and < 4K (the size of the IOUSBIsocStruct.  The call itself will need to map the buffers
	
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::_WriteIsochPipe",  target);
	// target->PrintExternalMethodArgs(arguments, 5);
	
	if ( arguments->asyncWakePort ) 
	{
 		target->retain();
		target->IncrementOutstandingIO();
        		
        if ( arguments->structureInputDescriptor ) 
		{
			USBLog(3, "+IOUSBInterfaceUserClientV2[%p]::_WriteIsochPipe  We got an unexpected structureInputDescriptor.  Returning unsupported ",  target);
            ret = kIOReturnUnsupported;
		}
        else
		{
			IOUSBIsocStructV3		isocData;
			
			isocData.fPipe = arguments->scalarInput[0];
			isocData.fBuffer = (mach_vm_address_t)arguments->scalarInput[1];
			isocData.fBufSize = (mach_vm_size_t)arguments->scalarInput[2];
			isocData.fStartFrame = arguments->scalarInput[3];
			isocData.fNumFrames = arguments->scalarInput[4];
			isocData.fFrameListPtr = (mach_vm_address_t)arguments->scalarInput[5];
			
			ret = target->DoIsochPipeAsync( &isocData,  arguments->asyncReference, arguments->asyncReferenceCount, kIODirectionOut);
		}
		
        if ( ret ) 
		{
			target->DecrementOutstandingIO();
			target->release();
		}
	}
	
	return ret;
}  


IOReturn 
IOUSBInterfaceUserClientV2::DoIsochPipeAsync(IOUSBIsocStructV3 *isocData, io_user_reference_t * asyncReference, uint32_t asyncCount, IODirection direction)
{
	IOReturn				ret;
    IOUSBPipe *				pipeObj = NULL;
    IOMemoryDescriptor *	dataMem = NULL;
    IOMemoryDescriptor *	countMem = NULL;
    uint32_t				frameLen = 0;	// In bytes
    bool					countMemPrepared = false;
    bool					dataMemPrepared = false;
	IOUSBIsocCompletion		tap;
	IOUSBInterfaceUserClientISOAsyncParamBlock *	pb = NULL;
		
	USBLog(7,"+%s[%p]::DoIsochPipeAsync  fPipe: %d, buffer: 0x%qx, fBufSize = %d, fStartFrame: %qd, fFrameListPtr: 0x%qx, direction: %d", getName(), this,
					   (uint32_t)isocData->fPipe, isocData->fBuffer, (uint32_t)isocData->fBufSize, isocData->fStartFrame, isocData->fFrameListPtr, direction);

    if (fOwner && !isInactive())
    {
		pipeObj = GetPipeObj(isocData->fPipe);
		if (pipeObj)
		{
			frameLen = isocData->fNumFrames * sizeof(IOUSBIsocFrame);
			do {
				USBLog(7,"%s[%p]::DoIsochPipeAsync creating data IOMD:  buffer: 0x%qx, size: %qd", getName(), this, isocData->fBuffer, isocData->fBufSize); 
				dataMem = IOMemoryDescriptor::withAddressRange( isocData->fBuffer, isocData->fBufSize, direction, fTask);
				if (!dataMem) 
				{
                    USBLog(1, "IOUSBInterfaceUserClientV2[%p]::DoIsochPipeAsync could not create dataMem descriptor",  this);
					USBTrace( kUSBTInterfaceUserClient,  kTPInterfaceUCDoIsochPipeAsync, (uintptr_t)this, isocData->fBufSize, kIOReturnNoMemory, 1 );
					ret = kIOReturnNoMemory;
					break;
				}
				ret = dataMem->prepare();
				if (ret != kIOReturnSuccess)
                {
                    USBLog(1, "IOUSBInterfaceUserClientV2[%p]::DoIsochPipeAsync could not prepare dataMem descriptor (0x%x)",  this, ret);
					USBTrace( kUSBTInterfaceUserClient,  kTPInterfaceUCDoIsochPipeAsync, (uintptr_t)this, isocData->fBufSize, ret, 2 );
					break;
                }
				
                dataMemPrepared = true;
                
				countMem = IOMemoryDescriptor::withAddressRange( isocData->fFrameListPtr, frameLen, kIODirectionOutIn, fTask);
				if (!countMem) 
				{
                    USBLog(1, "IOUSBInterfaceUserClientV2[%p]::DoIsochPipeAsync could not create countMem descriptor",  this);
					USBTrace( kUSBTInterfaceUserClient,  kTPInterfaceUCDoIsochPipeAsync, (uintptr_t)this, frameLen, kIOReturnNoMemory, 3 );
					ret = kIOReturnNoMemory;
					break;
				}
				
                ret = countMem->prepare();
                if (ret != kIOReturnSuccess)
                {
                    USBLog(1, "IOUSBInterfaceUserClientV2[%p]::DoIsochPipeAsync could not prepare dataMem descriptor (0x%x)",  this, ret);
					USBTrace( kUSBTInterfaceUserClient,  kTPInterfaceUCDoIsochPipeAsync, (uintptr_t)this, (uintptr_t)countMem, ret, 4 );
                    break;
                }
                countMemPrepared = true;
				
				pb = (IOUSBInterfaceUserClientISOAsyncParamBlock*) IOMalloc(sizeof(IOUSBInterfaceUserClientISOAsyncParamBlock) + frameLen);
				if (!pb) 
				{
					ret = kIOReturnNoMemory;
					break;
				}
				
                // Copy in requested transfers, we'll copy out result in completion routine
                
				bcopy(asyncReference, pb->fAsyncRef, sizeof(OSAsyncReference64));
				pb->fAsyncCount = asyncCount;
				pb->frameLen = frameLen;
				pb->frameBase = isocData->fFrameListPtr;
				pb->numFrames = isocData->fNumFrames;
				pb->dataMem = dataMem;
				pb->countMem = countMem;
				
				countMem->readBytes(0, pb->frames, frameLen);
				
				tap.target = this;
				tap.action = &IOUSBInterfaceUserClientV2::IsoReqComplete;
				tap.parameter = pb;
				
				USBLog(7,"+IOUSBInterfaceUserClientV2[%p]::DoIsochPipeAsync  fPipe: %d, dataMem: %p, countMem: %p, fBufSize = 0x%x, fStartFrame: %qd", this,
					   (uint32_t)isocData->fPipe,dataMem, countMem, (uint32_t)isocData->fBufSize, isocData->fStartFrame);

				if (direction == kIODirectionOut)
					ret = pipeObj->Write(dataMem, isocData->fStartFrame, isocData->fNumFrames, pb->frames, &tap);
				else
					ret = pipeObj->Read(dataMem, isocData->fStartFrame, isocData->fNumFrames, pb->frames, &tap);
			} while (false);
			pipeObj->release();
		}
		else
			ret = kIOUSBUnknownPipeErr;
    }
    else
        ret = kIOReturnNotAttached;
	
    if (kIOReturnSuccess != ret) 
    {
		USBLog(3, "IOUSBInterfaceUserClientV2[%p]::DoIsochPipeAsync err 0x%x (%s)", this, ret, USBStringFromReturn(ret));
		if (dataMem)
        {
            if ( dataMemPrepared )
                dataMem->complete();
			dataMem->release();
            dataMem = NULL;
        }
        
		if (countMem)
        {
            if ( countMemPrepared )
                countMem->complete();
			countMem->release();
            countMem = NULL;
        }
        
    }
	
	if ( ret != kIOReturnSuccess )
	{
		USBLog(3, "IOUSBInterfaceUserClientV2[%p]::-DoIsochPipeAsync err 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	
    return ret;
}


#pragma mark Low Latency Isoch Methods

IOReturn IOUSBInterfaceUserClientV2::_LowLatencyReadIsochPipe(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
	IOReturn								ret = kIOReturnUnsupported;
	
	// This call will always be Async and < 4K (the size of the IOUSBIsocStruct.  The call itself will need to map the buffers
	
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::_LowLatencyReadIsochPipe",  target);
	// target->PrintExternalMethodArgs(arguments, 5);
	
	if ( arguments->asyncWakePort ) 
	{
		IOUSBLowLatencyIsocCompletion	tap;
		IOUSBLowLatencyCommand *		command = NULL;
		
		// First, attempt to get a command for our transfer
		//
		command = (IOUSBLowLatencyCommand *) target->fFreeUSBLowLatencyCommandPool->getCommand(false);
		
		// If we couldn't get a command, increase the allocation and try again
		//
		if ( command == NULL )
		{
			target->IncreaseCommandPool();
			
			command = (IOUSBLowLatencyCommand *) target->fFreeUSBLowLatencyCommandPool->getCommand(false);
			if ( command == NULL )
			{
				USBLog(3,"IOUSBInterfaceUserClientV2[%p]::_LowLatencyReadIsochPipe Could not get a IOUSBLowLatencyIsocCommand", target);
				ret = kIOReturnNoResources;
				goto ErrorExit;
			}
		}
		
		target->retain();
		target->IncrementOutstandingIO();
        
		command->SetAsyncReference( arguments->asyncReference );
		command->SetAsyncCount(arguments->asyncReferenceCount);
        
		tap.target = target;
        tap.action = &IOUSBInterfaceUserClientV2::LowLatencyIsoReqComplete;
        tap.parameter = command;
		
        if ( arguments->structureInputDescriptor ) 
		{
			USBLog(3, "+IOUSBInterfaceUserClientV2[%p]::_LowLatencyReadIsochPipe  We got an unexpected structureOutputDescriptor.  Returning unsupported ",  target);
            ret = kIOReturnUnsupported;
		}
        else
		{
			IOUSBLowLatencyIsocStructV3		llIsocData;
			
			llIsocData.fPipe = arguments->scalarInput[0];
			llIsocData.fBufSize = arguments->scalarInput[1];
			llIsocData.fStartFrame = arguments->scalarInput[2];
			llIsocData.fNumFrames = arguments->scalarInput[3];
			llIsocData.fUpdateFrequency = arguments->scalarInput[4];
			llIsocData.fDataBufferCookie = arguments->scalarInput[5];
			llIsocData.fDataBufferOffset = arguments->scalarInput[6];
			llIsocData.fFrameListBufferCookie = arguments->scalarInput[7];
			llIsocData.fFrameListBufferOffset = arguments->scalarInput[8];

			if (!target->isInactive() && target->fGate && target->fWorkLoop)
			{
				IOCommandGate *	gate = target->fGate;
				IOWorkLoop *	workLoop = target->fWorkLoop;
				
				workLoop->retain();
				gate->retain();
				
				ret = gate->runAction(target->DoLowLatencyIsochPipeAsyncGated, &llIsocData,  &tap, (void* )kIODirectionIn);
				if ( ret != kIOReturnSuccess)
				{
					USBLog(3, "IOUSBInterfaceUserClientV2[%p]::_LowLatencyReadIsochPipe  runAction returned 0x%x, isInactive(%s)",  target, ret, target->isInactive() ? "true" : "false");
				}
				
				gate->release();
				workLoop->release();
			}
			else
				ret = kIOReturnNoResources;
		}
		
		if (kIOReturnSuccess != ret) 
		{
			USBLog(3, "IOUSBInterfaceUserClientV2[%p]::_LowLatencyReadIsochPipe err 0x%x",  target, ret);

			// return command
			//
			if ( command )
				target->fFreeUSBLowLatencyCommandPool->returnCommand(command);
			
			target->DecrementOutstandingIO();
			target->release();
		}
	}

ErrorExit:
	return ret;
}  


IOReturn IOUSBInterfaceUserClientV2::_LowLatencyWriteIsochPipe(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
	IOReturn								ret = kIOReturnUnsupported;
	
	// This call will always be Async and < 4K (the size of the IOUSBIsocStruct.  The call itself will need to map the buffers
	
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::_LowLatencyWriteIsochPipe",  target);
	// target->PrintExternalMethodArgs(arguments, 5);
	
	if ( arguments->asyncWakePort ) 
	{
		IOUSBLowLatencyIsocCompletion	tap;
		IOUSBLowLatencyCommand *		command = NULL;
		
		// First, attempt to get a command for our transfer
		//
		command = (IOUSBLowLatencyCommand *) target->fFreeUSBLowLatencyCommandPool->getCommand(false);
		
		// If we couldn't get a command, increase the allocation and try again
		//
		if ( command == NULL )
		{
			target->IncreaseCommandPool();
			
			command = (IOUSBLowLatencyCommand *) target->fFreeUSBLowLatencyCommandPool->getCommand(false);
			if ( command == NULL )
			{
				USBLog(3,"IOUSBInterfaceUserClientV2[%p]::_LowLatencyWriteIsochPipe Could not get a IOUSBLowLatencyIsocCommand",target);
				ret = kIOReturnNoResources;
				goto ErrorExit;
			}
		}
		
		target->retain();
		target->IncrementOutstandingIO();
        
		command->SetAsyncReference( arguments->asyncReference );
		command->SetAsyncCount(arguments->asyncReferenceCount);
        
		tap.target = target;
        tap.action = &IOUSBInterfaceUserClientV2::LowLatencyIsoReqComplete;
        tap.parameter = command;
		
        if ( arguments->structureInputDescriptor ) 
		{
			USBLog(3, "+IOUSBInterfaceUserClientV2[%p]::_LowLatencyWriteIsochPipe  We got an unexpected structureInputDescriptor.  Returning unsupported ",  target);
            ret = kIOReturnUnsupported;
		}
        else
		{
			IOUSBLowLatencyIsocStructV3		llIsocData;
			
			llIsocData.fPipe = arguments->scalarInput[0];
			llIsocData.fBufSize = arguments->scalarInput[1];
			llIsocData.fStartFrame = arguments->scalarInput[2];
			llIsocData.fNumFrames = arguments->scalarInput[3];
			llIsocData.fUpdateFrequency = arguments->scalarInput[4];
			llIsocData.fDataBufferCookie = arguments->scalarInput[5];
			llIsocData.fDataBufferOffset = arguments->scalarInput[6];
			llIsocData.fFrameListBufferCookie = arguments->scalarInput[7];
			llIsocData.fFrameListBufferOffset = arguments->scalarInput[8];
			
			if (!target->isInactive() && target->fGate && target->fWorkLoop)
			{
				IOCommandGate *	gate = target->fGate;
				IOWorkLoop *	workLoop = target->fWorkLoop;
				
				workLoop->retain();
				gate->retain();
				
				ret = gate->runAction(target->DoLowLatencyIsochPipeAsyncGated, &llIsocData,  &tap, (void* )kIODirectionOut);
				if ( ret != kIOReturnSuccess)
				{
					USBLog(3, "IOUSBInterfaceUserClientV2[%p]::_LowLatencyWriteIsochPipe  runAction returned 0x%x, isInactive(%s)",  target, ret, target->isInactive() ? "true" : "false");
				}
				
				gate->release();
				workLoop->release();
			}
			else
				ret = kIOReturnNoResources;
		}
		
		if (kIOReturnSuccess != ret) 
		{
			USBLog(3, "IOUSBInterfaceUserClientV2[%p]::_LowLatencyWriteIsochPipe err 0x%x",  target, ret);
			
			// return command
			if ( command )
				target->fFreeUSBLowLatencyCommandPool->returnCommand(command);
			
			target->DecrementOutstandingIO();
			target->release();
		}
	}

ErrorExit:
	return ret;
}  

IOReturn 
IOUSBInterfaceUserClientV2::DoLowLatencyIsochPipeAsyncGated(OSObject *target, void *param1, void *param2, void *param3, void *param4)
{
#pragma unused (param4)
	IOUSBInterfaceUserClientV2 *			me = OSDynamicCast(IOUSBInterfaceUserClientV2, target);
    if (!me)
    {
		USBLog(1, "IOUSBInterfaceUserClientV2::DoLowLatencyIsochPipeAsyncGated - invalid target");
		// USBTrace( kUSBTInterfaceUserClient,  kTPInterfaceUCChangeOutstandingIO, (uintptr_t)me, direction );
		return kIOReturnBadArgument;
    }
	
	return me->DoLowLatencyIsochPipeAsync((IOUSBLowLatencyIsocStructV3 *)param1, (IOUSBLowLatencyIsocCompletion*) param2, (uintptr_t)param3);
}

IOReturn 
IOUSBInterfaceUserClientV2::DoLowLatencyIsochPipeAsync(IOUSBLowLatencyIsocStructV3 *isocInfo, IOUSBLowLatencyIsocCompletion *completion, uintptr_t direction)
{
	IOReturn								ret;
    IOUSBPipe *								pipeObj = NULL;
    IOMemoryDescriptor *					aDescriptor = NULL;
    IOUSBLowLatencyIsocFrame *				pFrameList = NULL;
    IOUSBLowLatencyUserClientBufferInfoV4 *	dataBuffer = NULL;
    IOUSBLowLatencyUserClientBufferInfoV4 *	frameListDataBuffer	= NULL;
	
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::DoLowLatencyIsochPipeAsync",  this);

    if (fOwner && !isInactive())
    {
		pipeObj = GetPipeObj(isocInfo->fPipe);
		if (pipeObj)
		{
			do {
                USBLog(7,"IOUSBInterfaceUserClientV2[%p]::DoLowLatencyIsochPipeAsync: dataBuffer cookie: %d, offset: %d, frameList cookie: %d, offset : %d", this, (uint32_t)isocInfo->fDataBufferCookie, (uint32_t)isocInfo->fDataBufferOffset, (uint32_t)isocInfo->fFrameListBufferCookie, (uint32_t)isocInfo->fFrameListBufferOffset );
                
				IOUSBLowLatencyCommand * command = (IOUSBLowLatencyCommand *)completion->parameter;

                // Find the buffer corresponding to the data buffer cookie:
                //
                dataBuffer = FindBufferCookieInListV2(isocInfo->fDataBufferCookie);
                
                if ( dataBuffer == NULL )
				{
					USBLog(3,"IOUSBInterfaceUserClientV2[%p]::DoLowLatencyIsochPipeAsync: Could not find our buffer (cookie %d) in the list", this, (uint32_t)isocInfo->fDataBufferCookie );
					ret = kIOReturnNoMemory;
					break;
				}
                
                USBLog(7,"IOUSBInterfaceUserClientV2[%p]::DoLowLatencyIsochPipeAsync: Found data buffer for cookie: %d, descriptor: %p, dataBufferDescriptor: %p, offset : %d", this, (uint32_t)isocInfo->fDataBufferCookie, 
				dataBuffer->bufferDescriptor, dataBuffer->dataBufferIOMD,(uint32_t)isocInfo->fDataBufferOffset );
				
                // Create a new IOMD that is a subrange of our data buffer memory descriptor, and prepare it
                //
				aDescriptor = IOSubMemoryDescriptor::withSubRange( dataBuffer->bufferDescriptor == NULL ? dataBuffer->dataBufferIOMD : dataBuffer->bufferDescriptor, isocInfo->fDataBufferOffset, isocInfo->fBufSize, direction );
                if ( aDescriptor == NULL )
				{
					USBLog(3,"IOUSBInterfaceUserClientV2[%p]::DoLowLatencyIsochPipeAsync: Could not create an IOMD:withSubRange", this );
					ret = kIOReturnNoMemory;
					break;
				}
				
                // Prepare this descriptor
                //
                ret = aDescriptor->prepare();
                if (ret != kIOReturnSuccess)
                {
 					USBLog(3,"IOUSBInterfaceUserClientV2[%p]::DoLowLatencyIsochPipeAsync: Preparing the descriptor returned 0x%x", this, ret );
					break;
                }
                
                // Find the buffer corresponding to the frame list cookie:
                //
                frameListDataBuffer = FindBufferCookieInListV2(isocInfo->fFrameListBufferCookie);
                
                if ( frameListDataBuffer == NULL )
				{
					USBLog(3,"IOUSBInterfaceUserClientV2[%p]::DoLowLatencyIsochPipeAsync: Could not find our buffer (cookie %d) in the list, returning kIOReturnNoMemory", this, (uint32_t)isocInfo->fFrameListBufferCookie );
					ret = kIOReturnNoMemory;
					break;
				}
                
                USBLog(7,"IOUSBInterfaceUserClientV2[%p]::DoLowLatencyIsochPipeAsync: Found frameList buffer for cookie: %d, descriptor: %p, offset : %d", this, (uint32_t)isocInfo->fFrameListBufferCookie, 
				(void *) frameListDataBuffer->frameListKernelAddress,(uint32_t)isocInfo->fFrameListBufferOffset );
				
                // Get our virtual address by looking at the buffer data and adding in the offset that was passed in
                //
                pFrameList = (IOUSBLowLatencyIsocFrame *) ( (uint64_t) frameListDataBuffer->frameListKernelAddress + isocInfo->fFrameListBufferOffset);
                
 				// Increase the reference counts of our dma and framelist buffers
				dataBuffer->refCount++;
				frameListDataBuffer->refCount++;

				// Copy the data into our command buffer
                //
                command->SetFrameBase( (mach_vm_address_t) ((uint64_t) frameListDataBuffer->bufferAddress + isocInfo->fFrameListBufferOffset));
                command->SetDataBuffer(aDescriptor);
				command->SetDMABufferInfo(dataBuffer);
                command->SetFrameListBufferInfo(frameListDataBuffer);
                
				if ( direction == kIODirectionOut )
					ret = pipeObj->Write(aDescriptor, isocInfo->fStartFrame, isocInfo->fNumFrames, pFrameList, completion, isocInfo->fUpdateFrequency);
				else
					ret = pipeObj->Read(aDescriptor, isocInfo->fStartFrame, isocInfo->fNumFrames,pFrameList, completion, isocInfo->fUpdateFrequency);
				
            } while (false);
            
			pipeObj->release();
		}
		else
			ret = kIOUSBUnknownPipeErr;
    }
    else
        ret = kIOReturnNotAttached;
	
    if (kIOReturnSuccess != ret) 
    {
		USBLog(3, "IOUSBInterfaceUserClientV2[%p]::DoLowLatencyIsochPipeAsync err 0x%x (%s)", this, ret, USBStringFromReturn(ret));
		
        if ( aDescriptor )
        {
            aDescriptor->release();
            aDescriptor = NULL;
        }
    }
	
    USBLog(7, "-IOUSBInterfaceUserClientV2[%p]::DoLowLatencyIsochPipeAsync",  this);
    return ret;
}


IOReturn IOUSBInterfaceUserClientV2::_LowLatencyPrepareBuffer(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
	LowLatencyUserBufferInfoV3			bufferData;
	IOReturn							kr;
	
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::_LowLatencyPrepareBuffer",  target);
 	// target->PrintExternalMethodArgs(arguments, 5);

	bufferData.cookie = arguments->scalarInput[0];
	bufferData.bufferAddress = (mach_vm_address_t) arguments->scalarInput[1];
	bufferData.bufferSize = (mach_vm_size_t) arguments->scalarInput[2];
	bufferData.bufferType = arguments->scalarInput[3];
	bufferData.isPrepared = arguments->scalarInput[4];
	bufferData.nextBuffer = (LowLatencyUserBufferInfoV3 *) arguments->scalarInput[6];
	
	if (!target->isInactive() && target->fGate && target->fWorkLoop)
	{
		IOCommandGate *	gate = target->fGate;
		IOWorkLoop *	workLoop = target->fWorkLoop;
		
		workLoop->retain();
		gate->retain();
		
		kr = gate->runAction(target->LowLatencyPrepareBufferGated, &bufferData, &(arguments->scalarOutput[0]));
		if ( kr != kIOReturnSuccess)
		{
			USBLog(3, "IOUSBInterfaceUserClientV2[%p]::_LowLatencyPrepareBuffer  runAction returned 0x%x, isInactive(%s)",  target, kr, target->isInactive() ? "true" : "false");
			
		}
		
		gate->release();
		workLoop->release();
	}
	else
		kr = kIOReturnNoResources;
	
	return kr;
}


IOReturn 
IOUSBInterfaceUserClientV2::LowLatencyPrepareBufferGated(OSObject *target, void *param1, void *param2, void *param3, void *param4)
{
#pragma unused (param3, param4)
    
	IOUSBInterfaceUserClientV2 *			me = OSDynamicCast(IOUSBInterfaceUserClientV2, target);
	
    if (!me)
    {
		USBLog(1, "IOUSBInterfaceUserClientV2::LowLatencyPrepareBufferGated - invalid target");
		// USBTrace( kUSBTInterfaceUserClient,  kTPInterfaceUCChangeOutstandingIO, (uintptr_t)me, direction );
		return kIOReturnBadArgument;
    }
	
	return me->LowLatencyPrepareBuffer((LowLatencyUserBufferInfoV3 *)param1, (uint64_t *)param2);
}


IOReturn 
IOUSBInterfaceUserClientV2::LowLatencyPrepareBuffer(LowLatencyUserBufferInfoV3 *bufferData, uint64_t * addrOut)
{
	IOReturn								ret = kIOReturnSuccess;
    IOMemoryDescriptor *					aDescriptor = NULL;
    IOUSBLowLatencyUserClientBufferInfoV4 *	kernelDataBuffer = NULL;
    IOMemoryMap *							frameListMap = NULL;
    IODirection								direction;
	IOBufferMemoryDescriptor *				dataBufferDescriptor = NULL;
	IOMemoryMap *							dataBufferMap = NULL;
	uint64_t								dataBuffer = NULL;
	bool									preparedIOMD = false;
	IOUSBDevice *							device = NULL;
	IOUSBControllerV2 *						controller = NULL;
    IOOptionBits							optionBits;
	mach_vm_address_t						physicalMask;
	
    IncrementOutstandingIO();
    
    if (fOwner && !isInactive())
    {
		USBLog(7, "IOUSBInterfaceUserClientV2[%p]::LowLatencyPrepareBuffer  cookie: %d, buffer: %p, size: %d, type %d, isPrepared: %d, next: %p",  this,
			   (uint32_t)bufferData->cookie,
			   (void *)bufferData->bufferAddress,
			   (uint32_t)bufferData->bufferSize,
			   (uint32_t)bufferData->bufferType,
			   (uint32_t)bufferData->isPrepared,
			   (void *)bufferData->nextBuffer);
		
		*addrOut = 0;
		
		// If bufferSize == 0 or bufferAddress is 0, bail out
		if ( bufferData->bufferSize == 0 )
		{
            USBLog(3,"IOUSBInterfaceUserClientV2%p]::LowLatencyPrepareBuffer  Incoming buffer size = 0!", this);
			return kIOReturnBadArgument;
		}
		
		// Allocate a buffer and zero it
        //
        kernelDataBuffer = ( IOUSBLowLatencyUserClientBufferInfoV4 *) IOMalloc( sizeof(IOUSBLowLatencyUserClientBufferInfoV4) );
        if (kernelDataBuffer == NULL )
        {
            USBLog(1,"IOUSBInterfaceUserClientV2%p]::LowLatencyPrepareBuffer  Could not malloc buffer info (size = %ld)!", this, sizeof(IOUSBLowLatencyUserClientBufferInfoV4) );
			USBTrace( kUSBTInterfaceUserClient,  kTPInterfaceUCLowLatencyPrepareBuffer, (uintptr_t)this, sizeof(IOUSBLowLatencyUserClientBufferInfoV4), kIOReturnNoMemory, 1 );
            return kIOReturnNoMemory;
        }
        
        bzero(kernelDataBuffer, sizeof(IOUSBLowLatencyUserClientBufferInfoV4));
        
        // Set the known fields
        //
        kernelDataBuffer->cookie = bufferData->cookie;
        kernelDataBuffer->bufferType = bufferData->bufferType;
        
		// Get the low latency options for our IOBMD from the controller
		device = fOwner->GetDevice();
		if ( !device )
		{
			USBLog(3,"IOUSBInterfaceUserClientV2[%p]::LowLatencyPrepareBuffer  could not find our IOUSBDevice",  this );
			ret = kIOReturnNoDevice;
			goto ErrorExit;
		}
		
		// This method only available for v2 controllers
		//
		controller = OSDynamicCast(IOUSBControllerV2, device->GetBus());
		
		if ( !controller )
		{
			USBLog(3,"IOUSBInterfaceUserClientV2[%p]::LowLatencyPrepareBuffer  could not find our V2 USB controller",  this );
			ret = kIOReturnNotAttached;
			goto ErrorExit;
		}
		
        // If This is a low latency DMA buffer, we need to allocate the data in the kernel with the appropriate options depending on the machine (e.g. physically contiguous
		// or in the lower 32-bits of address space).
        //
        if ( (bufferData->bufferType == kUSBLowLatencyWriteBuffer) or (bufferData->bufferType == kUSBLowLatencyReadBuffer) )
		{
			ret = controller->GetLowLatencyOptionsAndPhysicalMask(&optionBits,&physicalMask);
			if ( kIOReturnSuccess != ret )
			{
				USBLog(3,"IOUSBInterfaceUserClientV2[%p]::LowLatencyPrepareBuffer  GetLowLatencyOptionsAndPhysicalMask returned 0x%x",  this, (uint32_t)ret );
				ret = kIOReturnBadArgument;
				goto ErrorExit;
			}
			
			USBLog(7,"IOUSBInterfaceUserClientV2[%p]::LowLatencyPrepareBuffer  GetLowLatencyOptionsAndPhysicalMask IOOptionBits: %p, physicalMask: %p",  this, (void *)optionBits, (void *)physicalMask );
			
            direction = ( bufferData->bufferType == kUSBLowLatencyWriteBuffer ? kIODirectionOut : kIODirectionIn );
			
			dataBufferDescriptor = IOBufferMemoryDescriptor::inTaskWithPhysicalMask( kernel_task, optionBits | direction | kIOMemoryKernelUserShared, bufferData->bufferSize, physicalMask);
	
			if ( dataBufferDescriptor == NULL)
			{
				USBLog(1,"IOUSBInterfaceUserClientV2[%p]::LowLatencyPrepareBuffer  Could not create a physically contiguous IOBMD (size %qd)!",  this, (uint64_t)bufferData->bufferSize );
				USBTrace( kUSBTInterfaceUserClient,  kTPInterfaceUCLowLatencyPrepareBuffer, (uintptr_t)this, bufferData->bufferSize, kIOReturnNoMemory, 2 );
				ret = kIOReturnNoMemory;
				goto ErrorExit;
			}
			
			ret = dataBufferDescriptor->prepare();
            if (ret != kIOReturnSuccess)
            {
                USBLog(1,"IOUSBInterfaceUserClientV2[%p]::LowLatencyPrepareBuffer  Could not prepare the data buffer memory descriptor 0x%x",  this, ret );
				USBTrace( kUSBTInterfaceUserClient,  kTPInterfaceUCLowLatencyPrepareBuffer, (uintptr_t)this, (uintptr_t)dataBufferDescriptor, ret, 3 );
                goto ErrorExit;
            }
			
			preparedIOMD = true;
			
			dataBufferMap = dataBufferDescriptor->createMappingInTask(fTask, NULL, kIOMapAnywhere, 0, 0	);
			if ( dataBufferMap == NULL )
			{
                USBLog(1,"IOUSBInterfaceUserClientV2[%p]::LowLatencyPrepareBuffer  Could not map the data buffer memory descriptor",  this );
				USBTrace( kUSBTInterfaceUserClient,  kTPInterfaceUCLowLatencyPrepareBuffer, (uintptr_t)this, 0, 0, 4 );
                goto ErrorExit;
			}
			
			dataBuffer = dataBufferMap->getAddress();
			if ( dataBuffer == NULL )
			{
                USBLog(1,"IOUSBInterfaceUserClientV2[%p]::LowLatencyPrepareBuffer  Could not get the virtual address of the map",  this );
				USBTrace( kUSBTInterfaceUserClient,  kTPInterfaceUCLowLatencyPrepareBuffer, (uintptr_t)this, 0, 0, 5 );
                goto ErrorExit;
			}
			
			// At this point, we have the contiguous buffer used for the UHCI Low Latency Writes, so save it in our data structure so we can clean up later on
			//
            kernelDataBuffer->bufferSize = bufferData->bufferSize;
			kernelDataBuffer->dataBufferIOMD = dataBufferDescriptor;
			kernelDataBuffer->dataBufferMap= dataBufferMap;
			
			*addrOut = dataBuffer;
 
			USBLog(7, "IOUSBInterfaceUserClientV2[%p]::LowLatencyPrepareBuffer  finished preparing data buffer: size %d, desc: %p, map %p, virtual address: 0x%qx, cookie: %d",  this,
				   (uint32_t)kernelDataBuffer->bufferSize, kernelDataBuffer->dataBufferIOMD, kernelDataBuffer->dataBufferMap,
				   dataBuffer,  (uint32_t)kernelDataBuffer->cookie);
		}
        else if ( bufferData->bufferType == kUSBLowLatencyFrameListBuffer )
        {
            // We have a frame list that we need to map to the kernel's memory space
            //
            // Create a memory descriptor for our frame list and prepare it (pages it in if necesary and prepares it). 
            //
			USBLog(7,"IOUSBInterfaceUserClientV2[%p]::LowLatencyPrepareBuffer creating IOMD:  buffer: 0x%qx, size: %qd", this, (mach_vm_address_t)bufferData->bufferAddress, (mach_vm_size_t)bufferData->bufferSize); 
			aDescriptor = IOMemoryDescriptor::withAddressRange((mach_vm_address_t)bufferData->bufferAddress, (mach_vm_size_t)bufferData->bufferSize, kIODirectionOutIn, fTask);
            if (!aDescriptor) 
            {
                USBLog(1,"IOUSBInterfaceUserClientV2[%p]::LowLatencyPrepareBuffer  Could not create a frame list memory descriptor (addr: 0x%qx, size %d)!", this, bufferData->bufferAddress, (uint32_t)bufferData->bufferSize );
				USBTrace( kUSBTInterfaceUserClient, kTPInterfaceUCLowLatencyPrepareBuffer,  bufferData->bufferAddress, (uint32_t)bufferData->bufferSize, kIOReturnNoMemory, 8 );
                ret = kIOReturnNoMemory;
                goto ErrorExit;
            }
            
            ret = aDescriptor->prepare();
            if (ret != kIOReturnSuccess)
            {
                USBLog(1,"IOUSBInterfaceUserClientV2[%p]::LowLatencyPrepareBuffer  Could not prepare the frame list memory descriptor (0x%x)",  this, ret );
				USBTrace( kUSBTInterfaceUserClient,  kTPInterfaceUCLowLatencyPrepareBuffer, kUSBLowLatencyFrameListBuffer, ret, 0, 9 );
                goto ErrorExit;
            }
            
            // Map it into the kernel
            //
            frameListMap = aDescriptor->map();
            if (!frameListMap) 
            {
                USBLog(1,"IOUSBInterfaceUserClientV2[%p]::LowLatencyPrepareBuffer  Could not map the frame list memory descriptor!",  this );
				USBTrace( kUSBTInterfaceUserClient,  kTPInterfaceUCLowLatencyPrepareBuffer, kUSBLowLatencyFrameListBuffer, kIOReturnNoMemory, 0, 10 );
                ret = kIOReturnNoMemory;
                aDescriptor->complete();
                goto ErrorExit;
            }
			
            // Get the the mapped in virtual address and save it
            //
            kernelDataBuffer->frameListKernelAddress = frameListMap->getVirtualAddress();
            
            // Save the rest of the items
            // 
            kernelDataBuffer->bufferAddress = bufferData->bufferAddress;
            kernelDataBuffer->bufferSize = bufferData->bufferSize;
            kernelDataBuffer->frameListDescriptor = aDescriptor;
            kernelDataBuffer->frameListMap = frameListMap;
			
            USBLog(7, "IOUSBInterfaceUserClientV2[%p]::LowLatencyPrepareBuffer  finished preparing frame list buffer: %p, size %d, desc: %p, map %p, kernel address: %p, cookie: %d",  this,
				   (void *)kernelDataBuffer->bufferAddress, (uint32_t)kernelDataBuffer->bufferSize, (void *)kernelDataBuffer->bufferDescriptor, (void *)kernelDataBuffer->frameListMap,
				   (void *)kernelDataBuffer->frameListKernelAddress,  (uint32_t)kernelDataBuffer->cookie);
        }
		
        // Cool, we have a good buffer, add it to our list
        //
        AddDataBufferToList( kernelDataBuffer );
        
    }
    else
        ret = kIOReturnNotAttached;
	
ErrorExit:
		
		if (ret)
		{
			USBLog(3, "IOUSBInterfaceUserClientV2[%p]::LowLatencyPrepareBuffer - returning err 0x%x (%s)", this, ret, USBStringFromReturn(ret));
			
			if ( dataBufferDescriptor )
			{
				if (preparedIOMD)
					dataBufferDescriptor->complete();
				
				dataBufferDescriptor->release();
			}
			
			if ( dataBufferMap != NULL )
				dataBufferMap->release();
			
		}
	
    DecrementOutstandingIO();    
    return ret;
}


IOReturn IOUSBInterfaceUserClientV2::_LowLatencyReleaseBuffer(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
	LowLatencyUserBufferInfoV3		dataBuffer;
	IOReturn						kr;
	
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::_LowLatencyReleaseBuffer",  target);
 	// target->PrintExternalMethodArgs(arguments, 5);
	
	dataBuffer.cookie = arguments->scalarInput[0];
	dataBuffer.bufferAddress = (mach_vm_address_t)arguments->scalarInput[1];
	dataBuffer.bufferSize = (mach_vm_size_t)arguments->scalarInput[2];
	dataBuffer.bufferType = arguments->scalarInput[3];
	dataBuffer.nextBuffer = (LowLatencyUserBufferInfoV3 *)arguments->scalarInput[6];
	
	if (!target->isInactive() && target->fGate && target->fWorkLoop)
	{
		IOCommandGate *	gate = target->fGate;
		IOWorkLoop *	workLoop = target->fWorkLoop;
		
		workLoop->retain();
		gate->retain();

		kr = gate->runAction(target->LowLatencyReleaseBufferGated, &dataBuffer);
		if ( kr != kIOReturnSuccess)
		{
			USBLog(3, "IOUSBInterfaceUserClientV2[%p]::_LowLatencyReleaseBuffer  runAction returned 0x%x, isInactive(%s)",  target, kr, target->isInactive() ? "true" : "false");
			
		}
		
		gate->release();
		workLoop->release();
	}
	else
		kr = kIOReturnNoResources;
	
	return kr;
}

IOReturn 
IOUSBInterfaceUserClientV2::LowLatencyReleaseBufferGated(OSObject *target, void *param1, void *param2, void *param3, void *param4)
{
#pragma unused (param2, param3, param4)
    
	IOUSBInterfaceUserClientV2 *			me = OSDynamicCast(IOUSBInterfaceUserClientV2, target);
	
    if (!me)
    {
		USBLog(1, "IOUSBInterfaceUserClientV2::LowLatencyReleaseBufferGated - invalid target");
		// USBTrace( kUSBTInterfaceUserClient,  kTPInterfaceUCChangeOutstandingIO, (uintptr_t)me, direction );
		return kIOReturnBadArgument;
    }

	return me->LowLatencyReleaseBuffer((LowLatencyUserBufferInfoV3 *)param1);
}

IOReturn 
IOUSBInterfaceUserClientV2::LowLatencyReleaseBuffer(LowLatencyUserBufferInfoV3 *dataBuffer)
{
	IOUSBLowLatencyUserClientBufferInfoV4 *	kernelDataBuffer	= NULL;
    IOReturn				ret 			= kIOReturnSuccess;
    bool					found 			= false;
    
    IncrementOutstandingIO();
    
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::LowLatencyReleaseBuffer for cookie: %qd, I/O = %d", this, dataBuffer->cookie, fOutstandingIO);
	
    if (fOwner && !isInactive())
    {
        // We need to find the LowLatencyUserBufferInfoV2 structure that contains
        // this buffer and then remove it from the list and free the structure
        // and the memory that was allocated for it
        //
        kernelDataBuffer = FindBufferCookieInListV2(dataBuffer->cookie);
        if ( kernelDataBuffer == NULL )
        {
            USBLog(3, "+IOUSBInterfaceUserClientV2[%p]::LowLatencyReleaseBuffer cookie: %qd, could not find buffer in list", this, dataBuffer->cookie);
            ret = kIOReturnBadArgument;
            goto ErrorExit;
        }
        
        // Now, remove this bufferData from the list
        //
        found = RemoveDataBufferFromList( kernelDataBuffer );
        if ( !found )
        {
            USBLog(3, "+IOUSBInterfaceUserClientV2[%p]::LowLatencyReleaseBuffer cookie: %qd, could not remove buffer (%p) from list", this, dataBuffer->cookie, kernelDataBuffer);
            ret = kIOReturnBadArgument;
            goto ErrorExit;
        }
		
		// If the refCount is > 0, then we need to set  up a flag and release it once the req completes.
		if ( kernelDataBuffer->refCount > 0 )
		{
            USBLog(6, "+IOUSBInterfaceUserClientV2[%p]::LowLatencyReleaseBuffer buffer (%p) in use, refCount = %d, setting needToRelease", this, kernelDataBuffer, (uint32_t)kernelDataBuffer->refCount);
			kernelDataBuffer->needToRelease = true;
		}
		else 
		{
			// Now, need to complete/release/free the objects we allocated in our prepare
			//
			LowLatencyReleaseKernelBufferInfo( kernelDataBuffer);
		}
    }
    else
        ret = kIOReturnNotAttached;
	
ErrorExit:
		
	if (ret)
	{
		USBLog(3, "IOUSBInterfaceUserClientV2[%p]::LowLatencyReleaseBuffer - returning err 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	
    DecrementOutstandingIO();
    return ret;
}

void
IOUSBInterfaceUserClientV2::LowLatencyReleaseKernelBufferInfo(IOUSBLowLatencyUserClientBufferInfoV4 *kernelDataBufferInfo)
{
	USBLog(6, "+IOUSBInterfaceUserClientV2[%p]::LowLatencyReleaseKernelBufferInfo buffer (%p)", this, kernelDataBufferInfo);

	// Now, need to complete/release/free the objects we allocated in our prepare
	//
	if ( kernelDataBufferInfo->frameListMap )
	{
		USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::LowLatencyReleaseKernelBufferInfo releasing frameListMap (%p)", this, (void *)kernelDataBufferInfo->frameListKernelAddress);
		kernelDataBufferInfo->frameListMap->release();
		kernelDataBufferInfo->frameListMap = NULL;
		kernelDataBufferInfo->frameListKernelAddress = NULL;
	}
	
	if ( kernelDataBufferInfo->frameListDescriptor )
	{
		USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::LowLatencyReleaseKernelBufferInfo releasing frameListDescriptor (%p)", this, (void *)kernelDataBufferInfo->frameListDescriptor);
		kernelDataBufferInfo->frameListDescriptor->complete();
		kernelDataBufferInfo->frameListDescriptor->release();
		kernelDataBufferInfo->frameListDescriptor = NULL;
	}
	
	if ( kernelDataBufferInfo->bufferDescriptor )
	{
		USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::LowLatencyReleaseKernelBufferInfo releasing bufferDescriptor (%p)", this, (void *)kernelDataBufferInfo->bufferDescriptor);
		kernelDataBufferInfo->bufferDescriptor->complete();
		kernelDataBufferInfo->bufferDescriptor->release();
		kernelDataBufferInfo->bufferDescriptor = NULL;
	}
	
	if ( kernelDataBufferInfo->dataBufferIOMD )
	{
		USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::LowLatencyReleaseKernelBufferInfo releasing dataBufferIOMD (%p)", this, (void *)kernelDataBufferInfo->dataBufferIOMD);
		kernelDataBufferInfo->dataBufferIOMD->complete();
		kernelDataBufferInfo->dataBufferIOMD->release();
		kernelDataBufferInfo->dataBufferIOMD = NULL;
	}
	
	if ( kernelDataBufferInfo->dataBufferMap )
	{
		USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::LowLatencyReleaseKernelBufferInfo releasing dataBufferMap (%p)", this, kernelDataBufferInfo->dataBufferMap);
		kernelDataBufferInfo->dataBufferMap->release();
		kernelDataBufferInfo->dataBufferMap = NULL;
	}
	
	// Finally, deallocate our kernelDataBuffer
	//
	IOFree(kernelDataBufferInfo, sizeof(IOUSBLowLatencyUserClientBufferInfoV4));
	
}


void
IOUSBInterfaceUserClientV2::AddDataBufferToList( IOUSBLowLatencyUserClientBufferInfoV4 * insertBuffer )
{
	IOUSBLowLatencyUserClientBufferInfoV4 *	buffer;
    
    // Traverse the list looking for last buffer and insert ours into it
    //
    if ( fUserClientBufferInfoListHead == NULL )
    {
        fUserClientBufferInfoListHead = insertBuffer;
        return;
    }
    
    buffer = fUserClientBufferInfoListHead;
    
    while ( buffer->nextBuffer != NULL )
    {
        buffer =  buffer->nextBuffer;
    }
    
    // When we get here, nextBuffer is pointing to NULL.  Our insert buffer
    // already has nextBuffer = NULL, so we just insert it
    //
    buffer->nextBuffer = insertBuffer;
}

IOUSBLowLatencyUserClientBufferInfoV4 *	
IOUSBInterfaceUserClientV2::FindBufferCookieInListV2( uint64_t cookie)
{
	IOUSBLowLatencyUserClientBufferInfoV4 *	buffer;
    bool				foundIt = true;
    
    // Traverse the list looking for this buffer
    //
    if ( fUserClientBufferInfoListHead == NULL )
    {
        USBLog(3, "IOUSBInterfaceUserClientV2[%p]::FindBufferCookieInList - fUserClientBufferInfoListHead was NULL",  this);
        return NULL;
    }
    
    buffer = fUserClientBufferInfoListHead;
    
    // Now, we need to see if our cookie is the same as one in the buffer list
    //
    while ( buffer->cookie != cookie )
    {
        buffer = buffer->nextBuffer;
        if ( buffer == NULL )
        {
            foundIt = false;
            break;
        }
    }
    
    if ( foundIt )
        return buffer;
    else
	{
        USBLog(3, "IOUSBInterfaceUserClientV2[%p]::FindBufferCookieInList - Could no find buffer for cookie (%d), returning NULL",  this, (uint32_t)cookie);
        return NULL;
	}
}

bool			
IOUSBInterfaceUserClientV2::RemoveDataBufferFromList( IOUSBLowLatencyUserClientBufferInfoV4 *removeBuffer)
{
	IOUSBLowLatencyUserClientBufferInfoV4 *	buffer;
    IOUSBLowLatencyUserClientBufferInfoV4 *	previousBuffer;
    
    // If our head is NULL, then this buffer does not exist in our list
    //
    if ( fUserClientBufferInfoListHead == NULL )
    {
        return false;
    }
    
    buffer = fUserClientBufferInfoListHead;
    
    // if our removeBuffer is the first one in the list, then just update the head and
    // exit
    //
    if ( buffer == removeBuffer )
    {
        fUserClientBufferInfoListHead = buffer->nextBuffer;
    }
    else
    {    
        // Need to start previousBuffer pointing to our initial buffer, in case we match
        // the first time
        //
        previousBuffer = buffer;
        
        while ( buffer->nextBuffer != removeBuffer )
        {
            previousBuffer = buffer;
            buffer = previousBuffer->nextBuffer;
        }
        
        // When we get here, buffer is pointing to the same buffer as removeBuffer
        // and previous buffer is pointing to the previous element in the link list,
        // so, update the link in previous to point to removeBuffer->nextBuffer;
        //
        buffer->nextBuffer = removeBuffer->nextBuffer;
    }
    
    return true;
}




#pragma mark Bookkeeping Methods

void
IOUSBInterfaceUserClientV2::DecrementOutstandingIO(void)
{
 	IOCommandGate *	gate = fGate;
	IOWorkLoop *	workLoop = fWorkLoop;

	if (!fGate)
    {
		if (!--fOutstandingIO && fNeedToClose && !FOWNER_WAS_RELEASED)
		{
			USBLog(3, "IOUSBInterfaceUserClientV2[%p]::DecrementOutstandingIO isInactive(%s), outstandingIO = %d - closing device",  this, isInactive() ? "true" : "false" , (int)fOutstandingIO);
			
			// Want to set this one if fOwner is not valid, just in case
			FOWNER_WAS_RELEASED = true;
			if (fOwner)
			{
				USBLog(6, "IOUSBInterfaceUserClientV2[%p]::DecrementOutstandingIO  closing and releasing fOwner(%p)",  this, fOwner);
				fOwner->close(this);
				fOwner->release();
				fNeedToClose = false;
			}
			release();
		}
    }
	else
	{
		//USBTrace( kUSBTOutstandingIO, kTPInterfaceUCDecrement , (uintptr_t)this, (int)fOutstandingIO );
		workLoop->retain();
		gate->retain();

		IOReturn kr = gate->runAction(ChangeOutstandingIO, (void*)-1);
		if ( kr != kIOReturnSuccess)
		{
			USBLog(3, "IOUSBInterfaceUserClientV2[%p]::DecrementOutstandingIO runAction returned 0x%x, isInactive(%s)",  this, kr, isInactive() ? "true" : "false");

		}
		
		gate->release();
		workLoop->release();
	}
	
	if (fOutstandingIO == 0 && FDELAYED_WORKLOOP_FREE)
	{
		FDELAYED_WORKLOOP_FREE = false;
		
		USBLog(6, "IOUSBInterfaceUserClientV2[%p]::DecrementOutstandingIO  calling ReleaseWorkLoopAndGate()", this);
		ReleaseWorkLoopAndGate();
	}
	
}


void
IOUSBInterfaceUserClientV2::IncrementOutstandingIO(void)
{
	IOCommandGate *	gate = fGate;
	IOWorkLoop *	workLoop = fWorkLoop;
	
	if ( isInactive() )
	{
		USBLog(5, "IOUSBInterfaceUserClientV2[%p]::IncrementOutstandingIO  but we are inActive!",  this);
	}
	
    if (!fGate)
    {
		fOutstandingIO++;
		return;
    }
	//USBTrace( kUSBTOutstandingIO, kTPInterfaceUCIncrement , (uintptr_t)this, (int)fOutstandingIO );
	workLoop->retain();
	gate->retain();
	
    IOReturn kr = gate->runAction(ChangeOutstandingIO, (void*)1);
	if ( kr != kIOReturnSuccess)
	{
		USBLog(3, "IOUSBInterfaceUserClientV2[%p]::IncrementOutstandingIO runAction returned 0x%x, isInactive(%s)",  this, kr, isInactive() ? "true" : "false");
		
	}
	
	gate->release();
	workLoop->release();
	
}


IOReturn
IOUSBInterfaceUserClientV2::ChangeOutstandingIO(OSObject *target, void *param1, void *param2, void *param3, void *param4)
{
#pragma unused (param2, param3, param4)
    IOUSBInterfaceUserClientV2 *me = OSDynamicCast(IOUSBInterfaceUserClientV2, target);
    UInt32	direction = (uintptr_t)param1;
    
    if (!me)
    {
		USBLog(1, "IOUSBInterfaceUserClientV2::ChangeOutstandingIO - invalid target");
		USBTrace( kUSBTInterfaceUserClient,  kTPInterfaceUCChangeOutstandingIO, (uintptr_t)me, direction, 0, 0 );
		return kIOReturnSuccess;
    }
	
    switch (direction)
    {
		case 1:
			me->fOutstandingIO++;
			break;
			
		case -1:
			if (!--me->fOutstandingIO && me->fNeedToClose && !me->FOWNER_WAS_RELEASED)
            {
                USBLog(6, "IOUSBInterfaceUserClientV2[%p]::ChangeOutstandingIO isInactive = %d, outstandingIO = %d - closing device",  me, me->isInactive(), me->fOutstandingIO);
				
				// Want to set this one if fOwner is not valid, just in case
				me->FOWNER_WAS_RELEASED = true;

				if (me->fOwner) 
				{
					USBLog(6, "IOUSBInterfaceUserClientV2[%p]::ChangeOutstandingIO closing fOwner(%p)",  me, me->fOwner);
					me->fOwner->close(me);
					me->fOwner->release();
					me->fNeedToClose = false;
				}
				me->release();
			}
			break;
			
		default:
			USBLog(1, "IOUSBInterfaceUserClientV2[%p]::ChangeOutstandingIO - invalid direction",  me);
			USBTrace( kUSBTInterfaceUserClient,  kTPInterfaceUCChangeOutstandingIO, (uintptr_t)me, direction, 0, 1 );

    }
    return kIOReturnSuccess;
}


UInt32
IOUSBInterfaceUserClientV2::GetOutstandingIO()
{
    UInt32			count = 0;
  	IOCommandGate *	gate = fGate;
	IOWorkLoop *	workLoop = fWorkLoop;
	
	
    if (!fGate)
    {
		return fOutstandingIO;
    }
    
	workLoop->retain();
	gate->retain();
	
	IOReturn kr = gate->runAction(GetGatedOutstandingIO, (void*)&count);
	if ( kr != kIOReturnSuccess)
	{
		USBLog(3, "IOUSBInterfaceUserClientV2[%p]::GetOutstandingIO  runAction returned 0x%x, isInactive(%s)",  this, kr, isInactive() ? "true" : "false");
		
	}
	
	gate->release();
	workLoop->release();
	
    return count;
}

IOReturn
IOUSBInterfaceUserClientV2::GetGatedOutstandingIO(OSObject *target, void *param1, void *param2, void *param3, void *param4)
{
#pragma unused (param2, param3, param4)
    IOUSBInterfaceUserClientV2 *me = OSDynamicCast(IOUSBInterfaceUserClientV2, target);
	
    if (!me)
    {
		USBLog(1, "IOUSBInterfaceUserClientV2::GetGatedOutstandingIO - invalid target");
		return kIOReturnSuccess;
    }
	
    *(UInt32 *) param1 = me->fOutstandingIO;
	
    return kIOReturnSuccess;
}

void
IOUSBInterfaceUserClientV2::IncreaseCommandPool(void)
{
    int i;
    
    USBLog(3,"IOUSBInterfaceUserClientV2[%p] Adding (%d) to Command Pool",  this, kSizeToIncrementLowLatencyCommandPool);
	
    for (i = 0; i < kSizeToIncrementLowLatencyCommandPool; i++)
    {
        IOUSBLowLatencyCommand *command = IOUSBLowLatencyCommand::NewCommand();
        if (command)
            fFreeUSBLowLatencyCommandPool->returnCommand(command);
    }
    
    fCurrentSizeOfCommandPool += kSizeToIncrementLowLatencyCommandPool;
	
}

void
IOUSBInterfaceUserClientV2::ReleasePreparedDescriptors(void)
{
    IOUSBLowLatencyUserClientBufferInfoV4 *	kernelDataBuffer;
    IOUSBLowLatencyUserClientBufferInfoV4 *	nextBuffer;
	
    if ( fOutstandingIO != 0 )
    {
        USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::ReleasePreparedDescriptors: OutstandingIO is NOT 0 (%d) ", this, fOutstandingIO);
        return;
    }
    // If we have any kernelDataBuffer pointers, then release them now
    //
    if (fUserClientBufferInfoListHead != NULL)
    {
        //USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::ReleasePreparedDescriptors: fUserClientBufferInfoListHead NOT NULL (%p) ",  this, fUserClientBufferInfoListHead);
		
        nextBuffer = fUserClientBufferInfoListHead;
        kernelDataBuffer = fUserClientBufferInfoListHead;
        
        // Traverse the list and release memory
        //
        while ( nextBuffer != NULL )
        {
            nextBuffer = kernelDataBuffer->nextBuffer;
			
            // Now, need to complete/release/free the objects we allocated in our prepare
            //
			LowLatencyReleaseKernelBufferInfo( kernelDataBuffer );
			
            kernelDataBuffer = nextBuffer;
        }
        
        fUserClientBufferInfoListHead = NULL;
    }
}

#pragma mark Helper Methods

IOUSBPipe*
IOUSBInterfaceUserClientV2::GetPipeObj(UInt8 pipeNo)
{
    IOUSBPipe *pipe = NULL;
    
    if (fOwner && !isInactive())
    {
        if (pipeNo == 0)
            pipe = fOwner->GetDevice()->GetPipeZero();
		
        if ((pipeNo > 0) && (pipeNo <= kUSBMaxPipes))
            pipe = fOwner->GetPipeObj(pipeNo-1);
    }
    // we need to retain the pipe object because it could actually get released by the device/interface
    // and we don't want it to go away. This retain should probably be done in IOUSBDevice or IOUSBInterface
    // but we would affect too many people if we did that right now.
    if (pipe)
        pipe->retain();
	
    return pipe;
}

void
IOUSBInterfaceUserClientV2::ReleaseWorkLoopAndGate()
{
    // If we have any kernelDataBuffer pointers, then release them now
    //
    if (fUserClientBufferInfoListHead != NULL)
    {
        ReleasePreparedDescriptors();
    }
	
	// IOCommandPool::free() requires the workloop, so don't call it from free().
    if ( fFreeUSBLowLatencyCommandPool )
    {
        fFreeUSBLowLatencyCommandPool->release();
        fFreeUSBLowLatencyCommandPool = NULL;
    }
	
	if (fWorkLoop && fGate)
	{
		fWorkLoop->removeEventSource(fGate);
		
		if (fGate)
		{
			fGate->release();
			fGate = NULL;
		}
		
		if (fWorkLoop)
		{
			fWorkLoop->release();
			fWorkLoop = NULL;
		}
	}
}

#pragma mark IOKit Methods

//
// clientClose - my client on the user side has released the mach port, so I will no longer
// be talking to him
//
IOReturn  
IOUSBInterfaceUserClientV2::clientClose( void )
{
	IOReturn ret = kIOReturnNoDevice;

	retain();
	
	USBLog(6, "IOUSBInterfaceUserClientV2[%p]::clientClose", this);
	
	// If we are inActive(), this means that we have been terminated.  This should not happen, as the user client connection should be severed by now
	if ( isInactive())
	{
		USBLog(1, "IOUSBInterfaceUserClientV2[%p]::clientClose  We are inactive, returning",  this);
	}
	else 
	{
		IOCommandGate *	gate = fGate;
		IOWorkLoop *	workLoop = fWorkLoop;

		if (gate && workLoop)
		{
			workLoop->retain();
			gate->retain();

			ret = gate->runAction(ClientCloseEntry);
			if ( ret != kIOReturnSuccess)
			{
				USBLog(3, "IOUSBInterfaceUserClientV2[%p]::clientClose  runAction returned 0x%x, isInactive(%s)",  this, ret, isInactive() ? "true" : "false");
			}
			
			gate->release();
			workLoop->release();
		}
		else
		{
			ret = kIOReturnNoResources;
		}
	}
	
	release();
	
	return ret;
}

IOReturn  
IOUSBInterfaceUserClientV2::ClientCloseEntry(OSObject *target, void *param1, void *param2, void *param3, void *param4)
{
#pragma unused (param1, param2, param3, param4)
	IOUSBInterfaceUserClientV2 *			me = OSDynamicCast(IOUSBInterfaceUserClientV2, target);
    if (!me)
    {
		USBLog(1, "IOUSBInterfaceUserClientV2::ClientCloseEntry - invalid target");
		return kIOReturnBadArgument;
    }
	
	USBLog(6, "IOUSBInterfaceUserClientV2[%p]::ClientCloseEntry  calling ClientCloseGated", me);
	return me->ClientCloseGated();
}

IOReturn  
IOUSBInterfaceUserClientV2::ClientCloseGated( void )
{
	USBLog(5, "+IOUSBInterfaceUserClientV2[%p]::ClientCloseGated  isInactive = %d, fOutstandingIO = %d, FOWNER_WAS_RELEASED: %d",  this, isInactive(), fOutstandingIO, FOWNER_WAS_RELEASED);
	
	// If we are inActive(), this means that we have been terminated.
	if ( isInactive())
	{
		USBLog(6, "IOUSBInterfaceUserClientV2[%p]::ClientCloseGated  We are inactive, returning kIOReturnNoDevice",  this);
		return kIOReturnNoDevice;
	}
	
	// retain()/release() ourselves because we might close our fOwner and hence could be terminated
	retain();
    
	// First, close any return any bandwith that might still be around, and close any pipes that might be open
	if ( fOwner && FOPENED_FOR_EXCLUSIVEACCESS )
	{
		// If the interface is other than 0, set it to 0 before closing the pipes
		UInt8	altSetting = fOwner->GetAlternateSetting();
		
		if ( altSetting != 0 )
		{
			IOUSBDevice *			device = fOwner->GetDevice();
			
			USBLog(6, "+IOUSBInterfaceUserClientV2[%p]::ClientCloseGated setting Alternate Interface(%d) to 0 before closing pipes",  this, altSetting);
			fOwner->SetAlternateInterface(this, 0);
			
			if ( device )
			{
				OSObject *				propertyObj = NULL;
				OSBoolean *				boolObj = NULL;
				
				// If we have the suspend property in the device, then suspend it
				propertyObj = device->copyProperty(kUSBSuspendPort);
				boolObj = OSDynamicCast( OSBoolean, propertyObj);
				if ( boolObj && boolObj->isTrue())
				{
					if ( !device->open(this) )
					{
						USBLog(6, "+IOUSBInterfaceUserClientV2[%p]::ClientCloseGated  device->open returned FALSE",  this);
					}
					else
					{
						IOReturn kr = device->SuspendDevice(true);
						if (kr != kIOReturnSuccess )
						{
							USBLog(6, "+IOUSBInterfaceUserClientV2[%p]::ClientCloseGated  device->SuspendDevice returned 0x%x",  this, kr);
						}
					}
					device->close(this);
				}
				
				if (propertyObj)
					propertyObj->release();
			}
		}
		
		// If we are inactive, that means that our provider is being terminated or has been terminated, so the pipes will be closed
		// by that termination, so we don't have to do it again.  ClosePipes() will need to grab the USB workloop to abort the endpoints.
		if ( fOwner && !isInactive())
		{
			USBLog(6, "+IOUSBInterfaceUserClientV2[%p]::ClientCloseGated  closing pipes",  this);
			fOwner->ClosePipes();
		}

		// Check to see if the client forgot to call ::close()
		if ( FOPENED_FOR_EXCLUSIVEACCESS )
		{
			IOOptionBits	options = kUSBOptionBitOpenExclusivelyMask;
			
			USBLog(6, "IOUSBInterfaceUserClientV2[%p]::ClientCloseGated  we were open exclusively, closing exclusively",  this);
			FOPENED_FOR_EXCLUSIVEACCESS = false;
			fOwner->close(this, options);
		}
	}
			
	// Note that the terminate will end up calling provider->close(), so this is where the fOwner->open() from start() is balanced.  We don't want to do this
	// if we are inActive() or we have pendingIO.
	
	if (!isInactive() && (fOutstandingIO == 0) && !FOWNER_WAS_RELEASED)
	{
		USBLog(6, "IOUSBInterfaceUserClientV2[%p]::ClientCloseGated  calling fOwner(%p)->release()",  this, fOwner);
		FOWNER_WAS_RELEASED = true;
		fOwner->release();
		release();
	}
	
	fTask = NULL;
		
	USBLog(5, "+IOUSBInterfaceUserClientV2[%p]::ClientCloseGated  calling terminate()",  this);
	
	terminate();

    USBLog(6, "-IOUSBInterfaceUserClientV2[%p]::ClientCloseGated isInactive(%d), fOutstandingIO (%d)",  this,isInactive(), fOutstandingIO);
	
	release();
	
    return kIOReturnSuccess;			// DONT call super::clientClose, which just returns notSupported
	
}


IOReturn 
IOUSBInterfaceUserClientV2::clientDied( void )
{
    IOReturn ret;
	
    USBLog(6, "+IOUSBInterfaceUserClientV2[%p]::clientDied() IO: %d",  this, fOutstandingIO);
    
    fDead = true;				// don't send any mach messages in this case
    ret = super::clientDied();
	
    USBLog(6, "-IOUSBInterfaceUserClientV2[%p]::clientDied()",  this);
	
    return ret;
}

//
// stop
// 
void 
IOUSBInterfaceUserClientV2::stop(IOService * provider)
{
    
    USBLog(6, "+IOUSBInterfaceUserClientV2[%p]::stop(%p), IsInactive: %d, outstandingIO: %d, inGate: %s",  this, provider, isInactive(), fOutstandingIO, fWorkLoop ? (fWorkLoop->inGate() ? "Yes" : "No"): "No workloop!");
	
	if (fOutstandingIO == 0)
	{
		ReleaseWorkLoopAndGate();
	}
	else 
	{
		USBLog(6, "+IOUSBInterfaceUserClientV2[%p]::stop, fOutstandingIO was not 0, so delaying free()'ing fGate and fWorkLoop",  this);
		FDELAYED_WORKLOOP_FREE = true;
	}
	
	super::stop(provider);
	
    USBLog(6, "-IOUSBInterfaceUserClientV2[%p]::stop(%p)",  this, provider);
	
}

void 
IOUSBInterfaceUserClientV2::free()
{
    USBLog(6, "+IOUSBInterfaceUserClientV2[%p]::free",  this);	
	
	//  This needs to be the LAST thing we do, as it disposes of our "fake" member
    //  variables.
    //
    if (fIOUSBInterfaceUserClientExpansionData)
    {
        IOFree(fIOUSBInterfaceUserClientExpansionData, sizeof(IOUSBInterfaceUserClientExpansionData));
        fIOUSBInterfaceUserClientExpansionData = NULL;
    }
	
    super::free();
}


bool 
IOUSBInterfaceUserClientV2::finalize( IOOptionBits options )
{
    bool ret;
	
    USBLog(6, "+IOUSBInterfaceUserClientV2[%p]::finalize(%08x), isInactive = %d, fOutstandingIO = %d, inGate: %s",  this, (int)options, isInactive(), fOutstandingIO, fWorkLoop ? (fWorkLoop->inGate() ? "Yes" : "No"): "No workloop!");
    
    ret = super::finalize(options);
    
    USBLog(7, "-IOUSBInterfaceUserClientV2[%p]::finalize(%08x) - returning %s",  this, (int)options, ret ? "true" : "false");
    return ret;
}


bool
IOUSBInterfaceUserClientV2::willTerminate( IOService * provider, IOOptionBits options )
{
    // this method is intended to be used to stop any pending I/O and to make sure that
    // we have begun getting our callbacks in order. by the time we get here, the
    // isInactive flag is set, so we really are marked as being done. we will do in here
    // what we used to do in the message method (this happens first)
	
    USBLog(5, "IOUSBInterfaceUserClientV2[%p]::willTerminate isInactive = %d, outstandingIO = %d inGate: %s",  this, isInactive(), (uint32_t)fOutstandingIO, fWorkLoop ? (fWorkLoop->inGate() ? "Yes" : "No"): "No workloop!");
	
    //  We have seen cases where our fOwner is not valid at this point.  This is strange
    //  but we'll code defensively and only execute if our provider (fOwner) is still around
    //
    if ( fWorkLoop && fOwner )
    {
        if ( fOutstandingIO > 0 )
        {
            int		i;
			
			USBLog(6, "IOUSBInterfaceUserClientV2[%p]::willTerminate - outstanding IO(%d), aborting pipes",  this, (uint32_t)fOutstandingIO);
			for (i = 1; i <= kUSBMaxPipes; i++)
			{
				AbortPipe(i);
            }
        }
    }
	
    return super::willTerminate(provider, options);
}


bool
IOUSBInterfaceUserClientV2::didTerminate( IOService * provider, IOOptionBits options, bool * defer )
{
    // this method comes at the end of the termination sequence. Hopefully, all of our outstanding IO is complete
    // in which case we can just close our provider and IOKit will take care of the rest. Otherwise, we need to 
    // hold on to the device and IOKit will terminate us when we close it later
	USBLog(5, "IOUSBInterfaceUserClientV2[%p]::didTerminate isInactive = %d, outstandingIO = %d, FOWNER_WAS_RELEASED: %d, inGate: %s",  this, isInactive(), fOutstandingIO, FOWNER_WAS_RELEASED, fWorkLoop ? (fWorkLoop->inGate() ? "Yes" : "No"): "No workloop!");
	
	if ( fWorkLoop)
	{
		// At this point, we are inactive, so if we have been opened for exclusive acces, we need to close it
		if ( FOPENED_FOR_EXCLUSIVEACCESS && fOwner)
		{
			IOOptionBits	options = kUSBOptionBitOpenExclusivelyMask;
			
			FOPENED_FOR_EXCLUSIVEACCESS = false;
			USBLog(5, "IOUSBInterfaceUserClientV2[%p]::didTerminate  FOPENED_FOR_EXCLUSIVEACCESS was true, closing fOwner(kUSBOptionBitOpenExclusivelyMask)",  this);
			fOwner->close(this, options);
		}
		
		// now, if we still have our owner open, then we need to close it
		if ( fOutstandingIO == 0)
		{
			bool	releaseIt = !FOWNER_WAS_RELEASED;
			FOWNER_WAS_RELEASED = true;
			
			USBLog(6, "+IOUSBInterfaceUserClientV2[%p]::didTerminate  fOutstandingIO is 0, releaseIt: %d",  this, releaseIt);

			if (fOwner && fOwner->isOpen(this))
			{
				
				USBLog(6, "+IOUSBInterfaceUserClientV2[%p]::didTerminate  closing fOwner",  this);
				
				fNeedToClose = false;
				fOwner->close(this);
			}
			
			if (fOwner && releaseIt)
			{
				USBLog(6, "+IOUSBInterfaceUserClientV2[%p]::didTerminate  releasing fOwner(%p) and UC",  this, fOwner);
				fOwner->release();
				release();
			}
		}
		else
		{
			USBLog(5, "+IOUSBInterfaceUserClientV2[%p]::didTerminate  will close fOwner later because IO is %d",  this, (uint32_t)fOutstandingIO);
			fNeedToClose = true;
		}
    }
	
    return super::didTerminate(provider, options, defer);
}


bool	
IOUSBInterfaceUserClientV2::terminate( IOOptionBits options )
{
	bool	retValue;
	
	USBLog(6, "%s[%p]::terminate  calling super::terminate", getName(), this);
	retValue = super::terminate(options);
	USBLog(6, "-%s[%p]::terminate", getName(), this);
	
	return retValue;
}

IOReturn 
IOUSBInterfaceUserClientV2::message( UInt32 type, IOService * provider,  void * argument )
{
#pragma unused (provider, argument)
    IOReturn	err = kIOReturnSuccess;
    
    switch ( type )
    {
	case kIOUSBMessagePortHasBeenSuspended:
		USBLog(6, "IOUSBInterfaceUserClientV2[%p]::message - received kIOUSBMessagePortHasBeenSuspended",  this);
		break;
		
	case kIOUSBMessagePortHasBeenReset:
		USBLog(6, "IOUSBInterfaceUserClientV2[%p]::message - received kIOUSBMessagePortHasBeenReset",  this);
		break;
		
	case kIOUSBMessagePortHasBeenResumed:
		USBLog(6, "IOUSBInterfaceUserClientV2[%p]::message - received kIOUSBMessagePortHasBeenResumed",  this);
		break;
		
	case kIOMessageServiceIsTerminated: 
		USBLog(6, "IOUSBInterfaceUserClientV2[%p]::message - received kIOMessageServiceIsTerminated",  this);
		break;
		
	default:
		break;
    }
    
    return err;
}


#pragma mark Debugging
void
IOUSBInterfaceUserClientV2::PrintExternalMethodArgs( IOExternalMethodArguments * arguments, UInt32 level )
{
#pragma unused (level)
	
	USBLog(level,"ExternalMethodArguments\n\tversion:\t0x%x\n\tselector:\t0x%x\n\tasyncWakePort:\t%p\n\tasyncReference:\t%p\n\tasyncReferenceCount:0x%x\n\tscalarInput:\t%p\n\tscalarInputCount:\t0x%x\n\tstructureInput:\t%p\n\tstructureInputSize:\t0x%x\n\tstructureInputMemoryDesc:\t%p\n\tscalarOutput:\t%p\n\tscalarOutputSize:\t0x%x\n\tstructureOutput:\t%p\n\tstructureOutputSize:\t0x%x\n\tstructureOutputDescriptor:\t%p\n\tstructureOutputDescriptorSize:\t0x%x\n",
	arguments->version,
	arguments->selector,
	arguments->asyncWakePort,
	arguments->asyncReference,
	arguments->asyncReferenceCount,
	arguments->scalarInput,
	arguments->scalarInputCount,
	arguments->structureInput,
	arguments->structureInputSize,
	arguments->structureInputDescriptor,
	arguments->scalarOutput,
	arguments->scalarOutputCount,
	arguments->structureOutput,
	arguments->structureOutputSize,
	arguments->structureOutputDescriptor,
	arguments->structureOutputDescriptorSize
	
	);
	
	for ( uint32_t i = 0; i < arguments->scalarInputCount; i++ )
	{
		USBLog(level,"\targuments->scalarInput[%d]: 0x%qx", i,  arguments->scalarInput[i]);
	}
}

#pragma mark Deprecated Methods
IOReturn 
IOUSBInterfaceUserClientV2::DoIsochPipeAsync(IOUSBIsocStruct *stuff, io_user_reference_t * asyncReference, uint32_t asyncCount, IODirection direction)
{
#pragma unused (stuff, asyncReference, asyncCount, direction)
    USBLog(3, "+%s[%p]::DoIsochPipeAsync  called DEPRECATED function", getName(), this);
	
	return kIOReturnUnsupported;
}

IOReturn 
IOUSBInterfaceUserClientV2::DoLowLatencyIsochPipeAsync(IOUSBLowLatencyIsocStruct *isocInfo, IOUSBLowLatencyIsocCompletion *completion, IODirection direction)
{
#pragma unused (isocInfo, completion, direction)
   USBLog(3, "+%s[%p]::DoLowLatencyIsochPipeAsync  called DEPRECATED function", getName(), this);
	
	return kIOReturnUnsupported;
}

bool			
IOUSBInterfaceUserClientV2::RemoveDataBufferFromList( IOUSBLowLatencyUserClientBufferInfo *removeBuffer)
{
#pragma unused (removeBuffer)
	USBLog(3, "%s[%p]::RemoveDataBufferFromList called DEPRECATED version of the method", getName(), this);
	return false;
}

IOUSBLowLatencyUserClientBufferInfo *	
IOUSBInterfaceUserClientV2::FindBufferCookieInList( UInt32 cookie)
{
#pragma unused (cookie)
	USBLog(3, "%s[%p]::FindBufferCookieInList called DEPRECATED version of the method", getName(), this);
	return NULL;
}

void
IOUSBInterfaceUserClientV2::AddDataBufferToList( IOUSBLowLatencyUserClientBufferInfo * insertBuffer )
{
#pragma unused (insertBuffer)
    USBLog(3, "%s[%p]::AddDataBufferToList called DEPRECATED version of the method", getName(), this);
}

IOReturn 
IOUSBInterfaceUserClientV2::LowLatencyReleaseBuffer(LowLatencyUserBufferInfoV2 *dataBuffer)
{
#pragma unused (dataBuffer)
   USBLog(3, "%s[%p]::LowLatencyReleaseBuffer called DEPRECATED version of the method", getName(), this);
	return kIOReturnUnsupported;
}

IOReturn 
IOUSBInterfaceUserClientV2::LowLatencyPrepareBuffer(LowLatencyUserBufferInfoV2 *bufferData, uint64_t * addrOut)
{
#pragma unused (bufferData, addrOut)
   USBLog(3, "%s[%p]::LowLatencyPrepareBuffer called DEPRECATED version of the method", getName(), this);
	return kIOReturnUnsupported;
}

IOReturn 
IOUSBInterfaceUserClientV2::DoLowLatencyIsochPipeAsync(IOUSBLowLatencyIsocStructV3 *isocInfo, IOUSBLowLatencyIsocCompletion *completion, IODirection direction)
{
#pragma unused (isocInfo, completion, direction)
	USBLog(3, "%s[%p]::DoLowLatencyIsochPipeAsync called DEPRECATED version of the method", getName(), this);
	return kIOReturnUnsupported;
}

void
IOUSBInterfaceUserClientV2::AddDataBufferToList( IOUSBLowLatencyUserClientBufferInfoV3 * insertBuffer )
{
#pragma unused (insertBuffer)
    USBLog(3, "%s[%p]::AddDataBufferToList called DEPRECATED version of the method", getName(), this);
}

IOUSBLowLatencyUserClientBufferInfoV3 *	
IOUSBInterfaceUserClientV2::FindBufferCookieInList( uint64_t cookie)
{
#pragma unused (cookie)
	USBLog(3, "%s[%p]::FindBufferCookieInList called DEPRECATED version of the method", getName(), this);
	return NULL;
}

bool			
IOUSBInterfaceUserClientV2::RemoveDataBufferFromList( IOUSBLowLatencyUserClientBufferInfoV3 *removeBuffer)
{
#pragma unused (removeBuffer)
	USBLog(3, "%s[%p]::RemoveDataBufferFromList called DEPRECATED version of the method", getName(), this);
	return false;
}

#pragma mark Padding Methods

OSMetaClassDefineReservedUsed(IOUSBInterfaceUserClientV2,  0);
OSMetaClassDefineReservedUsed(IOUSBInterfaceUserClientV2,  1);
OSMetaClassDefineReservedUsed(IOUSBInterfaceUserClientV2,  2);
OSMetaClassDefineReservedUsed(IOUSBInterfaceUserClientV2,  3);
OSMetaClassDefineReservedUsed(IOUSBInterfaceUserClientV2,  4);
OSMetaClassDefineReservedUsed(IOUSBInterfaceUserClientV2,  5);
OSMetaClassDefineReservedUsed(IOUSBInterfaceUserClientV2,  6);
OSMetaClassDefineReservedUsed(IOUSBInterfaceUserClientV2,  7);
OSMetaClassDefineReservedUsed(IOUSBInterfaceUserClientV2,  8);
OSMetaClassDefineReservedUsed(IOUSBInterfaceUserClientV2,  9);
OSMetaClassDefineReservedUsed(IOUSBInterfaceUserClientV2, 10);
OSMetaClassDefineReservedUsed(IOUSBInterfaceUserClientV2, 11);
OSMetaClassDefineReservedUsed(IOUSBInterfaceUserClientV2, 12);

OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV2, 13);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV2, 14);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV2, 15);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV2, 16);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV2, 17);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV2, 18);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV2, 19);


#pragma mark Low Latency Commands

IOUSBLowLatencyCommand *
IOUSBLowLatencyCommand::NewCommand()
{
    IOUSBLowLatencyCommand *me = new IOUSBLowLatencyCommand;
    
    if (me && !me->init())
    {
		me->release();
		me = NULL;
    }
    return me;
	
}

bool 
IOUSBLowLatencyCommand::init()
{
    if (!IOCommand::init())
        return false;
	
    // allocate our expansion data
    if (!fIOUSBLowLatencyExpansionData)
    {
		fIOUSBLowLatencyExpansionData = (IOUSBLowLatencyExpansionData *)IOMalloc(sizeof(IOUSBLowLatencyExpansionData));
		if (!fIOUSBLowLatencyExpansionData)
			return false;
		
		bzero(fIOUSBLowLatencyExpansionData, sizeof(IOUSBLowLatencyExpansionData));
    }
    return true;
}

void 
IOUSBLowLatencyCommand::free()
{
    //  This needs to be the LAST thing we do, as it disposes of our "fake" member
    //  variables.
    //
    if (fIOUSBLowLatencyExpansionData)
    {
		IOFree(fIOUSBLowLatencyExpansionData, sizeof(IOUSBLowLatencyExpansionData));
        fIOUSBLowLatencyExpansionData = NULL;
    }
	
    IOCommand::free();
}

void  			
IOUSBLowLatencyCommand::SetAsyncReference(OSAsyncReference64  ref)
{
    bcopy(ref, fAsyncRef, sizeof(OSAsyncReference64));
}

void  			
IOUSBLowLatencyCommand::SetAsyncCount(uint32_t  count)
{
    fAsyncReferenceCount = count;
}

// padding methods
//
OSMetaClassDefineReservedUnused(IOUSBLowLatencyCommand,  0);
OSMetaClassDefineReservedUnused(IOUSBLowLatencyCommand,  1);
OSMetaClassDefineReservedUnused(IOUSBLowLatencyCommand,  2);
OSMetaClassDefineReservedUnused(IOUSBLowLatencyCommand,  3);
OSMetaClassDefineReservedUnused(IOUSBLowLatencyCommand,  4);
OSMetaClassDefineReservedUnused(IOUSBLowLatencyCommand,  5);
OSMetaClassDefineReservedUnused(IOUSBLowLatencyCommand,  6);
OSMetaClassDefineReservedUnused(IOUSBLowLatencyCommand,  7);
OSMetaClassDefineReservedUnused(IOUSBLowLatencyCommand,  8);
OSMetaClassDefineReservedUnused(IOUSBLowLatencyCommand,  9);
OSMetaClassDefineReservedUnused(IOUSBLowLatencyCommand, 10);
OSMetaClassDefineReservedUnused(IOUSBLowLatencyCommand, 11);
OSMetaClassDefineReservedUnused(IOUSBLowLatencyCommand, 12);
OSMetaClassDefineReservedUnused(IOUSBLowLatencyCommand, 13);
OSMetaClassDefineReservedUnused(IOUSBLowLatencyCommand, 14);
OSMetaClassDefineReservedUnused(IOUSBLowLatencyCommand, 15);
OSMetaClassDefineReservedUnused(IOUSBLowLatencyCommand, 16);
OSMetaClassDefineReservedUnused(IOUSBLowLatencyCommand, 17);
OSMetaClassDefineReservedUnused(IOUSBLowLatencyCommand, 18);
OSMetaClassDefineReservedUnused(IOUSBLowLatencyCommand, 19);
