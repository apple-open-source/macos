/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 1999-2003 Apple Computer, Inc.		All Rights Reserved.
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

#include <TargetConditionals.h>

#include <sys/systm.h>
#include <sys/proc.h>
#include <kern/task.h>
#include <mach/port.h>
#include <mach/message.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOService.h>
#include <IOKit/IOSyncer.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOKitKeysPrivate.h>
#include <IOKit/IOUserClient.h>
#include "IOHIDLibUserClient.h"
#include "IOHIDDevice.h"
#include "IOHIDEventQueue.h"


#define super IOUserClient

struct AsyncParam {
	OSAsyncReference64		fAsyncRef;
	UInt32					fMax;
	IOMemoryDescriptor		*fMem;
	IOHIDReportType			reportType;
	uint32_t				fAsyncCount;
};

struct AsyncGateParam {
	OSAsyncReference		asyncRef;
	IOHIDReportType			reportType;
	UInt32					reportID;
	void *					reportBuffer;
	UInt32					reportBufferSize;
	UInt32					completionTimeOutMS;
};


OSDefineMetaClassAndStructors(IOHIDLibUserClient, IOUserClient);


const IOExternalMethodDispatch IOHIDLibUserClient::
sMethods[kIOHIDLibUserClientNumCommands] = {
	{ //	kIOHIDLibUserClientDeviceIsValid
	(IOExternalMethodAction) &IOHIDLibUserClient::_deviceIsValid,
	0, 0,
	2, 0
	},
	{ //	kIOHIDLibUserClientOpen
	(IOExternalMethodAction) &IOHIDLibUserClient::_open,
	1, 0,
	0, 0
	},
	{ //	kIOHIDLibUserClientClose
	(IOExternalMethodAction) &IOHIDLibUserClient::_close,
	0, 0,
	0, 0
	},
	{ //	kIOHIDLibUserClientCreateQueue
	(IOExternalMethodAction) &IOHIDLibUserClient::_createQueue,
	2, 0,
	1, 0
	},
	{ //	kIOHIDLibUserClientDisposeQueue
	(IOExternalMethodAction) &IOHIDLibUserClient::_disposeQueue,
	1, 0,
	0, 0
	},
	{ //	kIOHIDLibUserClientAddElementToQueue
	(IOExternalMethodAction) &IOHIDLibUserClient::_addElementToQueue,
	3, 0,
	1, 0
	},
	{ //	kIOHIDLibUserClientRemoveElementFromQueue
	(IOExternalMethodAction) &IOHIDLibUserClient::_removeElementFromQueue,
	2, 0,
	1, 0
	},
	{ //	kIOHIDLibUserClientQueueHasElement
	(IOExternalMethodAction) &IOHIDLibUserClient::_queueHasElement,
	2, 0,
	1, 0
	},
	{ //	kIOHIDLibUserClientStartQueue
	(IOExternalMethodAction) &IOHIDLibUserClient::_startQueue,
	1, 0,
	0, 0
	},
	{ //	kIOHIDLibUserClientStopQueue
	(IOExternalMethodAction) &IOHIDLibUserClient::_stopQueue,
	1, 0,
	0, 0
	},
	{ //	kIOHIDLibUserClientUpdateElementValues
	(IOExternalMethodAction) &IOHIDLibUserClient::_updateElementValues,
	0xffffffff, 0,
	0, 0
	},
	{ //	kIOHIDLibUserClientPostElementValues
	(IOExternalMethodAction) &IOHIDLibUserClient::_postElementValues,
	0xffffffff, 0,
	0, 0
	},
	{ //	kIOHIDLibUserClientGetReport
	(IOExternalMethodAction) &IOHIDLibUserClient::_getReport,
	3, 0,
	0, 0xffffffff
	},
	{ //	kIOHIDLibUserClientSetReport
	(IOExternalMethodAction) &IOHIDLibUserClient::_setReport,
	3, 0xffffffff,
	0, 0
	},
	{ //	kIOHIDLibUserClientGetElementCount
	(IOExternalMethodAction) &IOHIDLibUserClient::_getElementCount,
	0, 0,
	2, 0
	},
	{ //	kIOHIDLibUserClientGetElements
	(IOExternalMethodAction) &IOHIDLibUserClient::_getElements,
	1, 0,
	0, 0xffffffff
	},
	// ASYNC METHODS
	{ //	kIOHIDLibUserClientSetQueueAsyncPort
	(IOExternalMethodAction) &IOHIDLibUserClient::_setQueueAsyncPort,
	1, 0,
	0, 0
	}
};

static void deflate_vec(uint32_t *dp, uint32_t d, const uint64_t *sp, uint32_t s)
{
	if (d > s)
	d = s;

	for (uint32_t i = 0; i < d; i++)
	dp[i] = (uint32_t) sp[i];
}


bool IOHIDLibUserClient::
initWithTask(task_t owningTask, void * /* security_id */, UInt32 /* type */)
{
	if (!super::init())
	return false;

	fClient = owningTask;
	task_reference (fClient);
	
	proc_t p = (proc_t)get_bsdtask_info(fClient);
	fPid = proc_pid(p);
					
	fQueueMap = OSArray::withCapacity(4);
	if (!fQueueMap)
		return false;
	return true;
}

IOReturn IOHIDLibUserClient::clientClose(void)
{
	if ( !isInactive() && fGate ) {
		fGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDLibUserClient::cleanupGated));
		terminate();
	}
	
	return kIOReturnSuccess;
}

void IOHIDLibUserClient::cleanupGated(void)
{
   if (fClient) {
		task_deallocate(fClient);
		fClient = 0;
	}
   
   if (fNub) {
   
		// First clear any remaining queues
		setStateForQueues(kHIDQueueStateClear);
		
		// Have been started so we better detach
		
		// make sure device is closed (especially on crash)
		// note radar #2729708 for a more comprehensive fix
		// probably should also subclass clientDied for crash specific code
		fNub->close(this, fCachedOptionBits);
	}

	if ( fResourceNotification ) {
		fResourceNotification->remove();
		fResourceNotification = 0;
	}

	if (fResourceES) {
		if ( fWL )
			fWL->removeEventSource(fResourceES);
		fResourceES->release();
		fResourceES = 0;
	}
}

bool IOHIDLibUserClient::start(IOService *provider)
{
	if (!super::start(provider))
		return false;

	fNub = OSDynamicCast(IOHIDDevice, provider);
	if (!fNub)
		return false;

	fWL = getWorkLoop();
	if (!fWL)
		return false;
    
	fWL->retain();
    
    OSNumber *primaryUsage = OSDynamicCast(OSNumber, fNub->getProperty(kIOHIDPrimaryUsageKey));
	OSNumber *primaryUsagePage = OSDynamicCast(OSNumber, fNub->getProperty(kIOHIDPrimaryUsagePageKey));

	if ((primaryUsagePage && (primaryUsagePage->unsigned32BitValue() == kHIDPage_GenericDesktop)) &&
		(primaryUsage && ((primaryUsage->unsigned32BitValue() == kHIDUsage_GD_Keyboard) || (primaryUsage->unsigned32BitValue() == kHIDUsage_GD_Keypad))))
	{
		fNubIsKeyboard = true;
	}
			
	IOCommandGate * cmdGate = IOCommandGate::commandGate(this);
	if (!cmdGate)
		goto ABORT_START;
	
	fWL->addEventSource(cmdGate);
	
	fGate = cmdGate;

	fResourceES = IOInterruptEventSource::interruptEventSource
		(this, OSMemberFunctionCast(IOInterruptEventSource::Action, this, &IOHIDLibUserClient::resourceNotificationGated));
		
	if ( !fResourceES )
		goto ABORT_START;

	fWL->addEventSource(fResourceES);

	// Get notified everytime Root properties change
	fResourceNotification = addMatchingNotification(
		gIOPublishNotification,
		serviceMatching("IOResources"),
		OSMemberFunctionCast(IOServiceMatchingNotificationHandler, this, &IOHIDLibUserClient::resourceNotification),
		this);
					
	if ( !fResourceNotification )
		goto ABORT_START;
		
	return true;

ABORT_START:
	if (fResourceES) {
		fWL->removeEventSource(fResourceES);
		fResourceES->release();
		fResourceES = 0;
	}
	if (fGate) {
		fWL->removeEventSource(fGate);
		fGate->release();
		fGate = 0;
	}
    fWL->release();
    fWL = 0;        

	return false;
}

bool IOHIDLibUserClient::resourceNotification(void * refcon, IOService *service, IONotifier *notifier)
{
	#pragma ignore(notifier)
	
	if (!isInactive() && fResourceES)
		fResourceES->interruptOccurred(0, 0, 0);
		
	return true;
}

void IOHIDLibUserClient::resourceNotificationGated()
{
	IOReturn ret = kIOReturnSuccess;
	OSData * data;
	IOService * service = getResourceService();
	
	do {
		// We should force success on seize
		if ( kIOHIDOptionsTypeSeizeDevice & fCachedOptionBits )
			break;
		
		if ( !service ) {
			ret = kIOReturnError;
			break;
		}
			
		data = OSDynamicCast(OSData, service->getProperty(kIOConsoleUsersSeedKey));

		if ( !data || !data->getLength() || !data->getBytesNoCopy()) {
			ret = kIOReturnError;
			break;
		}
			
		UInt64 currentSeed = 0;
		
		switch ( data->getLength() ) {
			case sizeof(UInt8):
				currentSeed = *(UInt8*)(data->getBytesNoCopy());
				break;
			case sizeof(UInt16):
				currentSeed = *(UInt16*)(data->getBytesNoCopy());
				break;
			case sizeof(UInt32):
				currentSeed = *(UInt32*)(data->getBytesNoCopy());
				break;
			case sizeof(UInt64):
			default:
				currentSeed = *(UInt64*)(data->getBytesNoCopy());
				break;
		}
			
		// We should return rather than break so that previous setting is retained
		if ( currentSeed == fCachedConsoleUsersSeed )
			return;
			
		fCachedConsoleUsersSeed = currentSeed;

		ret = clientHasPrivilege(fClient, kIOClientPrivilegeAdministrator);
		if (ret == kIOReturnSuccess)
			break;

#if !TARGET_OS_EMBEDDED
		if ( fNubIsKeyboard ) {
			IOUCProcessToken token;
			token.token = fClient;
			token.pid = fPid;
			ret = clientHasPrivilege(&token, kIOClientPrivilegeSecureConsoleProcess);
		} else {
			ret = clientHasPrivilege(fClient, kIOClientPrivilegeConsoleUser);
		}
#endif
	} while (false);
	
	setValid(kIOReturnSuccess == ret);
}

typedef struct HIDCommandGateArgs {
	uint32_t					selector;
	IOExternalMethodArguments * arguments;
	IOExternalMethodDispatch *	dispatch;
	OSObject *					target;
	void *						reference;
}HIDCommandGateArgs;

IOReturn IOHIDLibUserClient::externalMethod(
								uint32_t					selector,
								IOExternalMethodArguments * arguments,
								IOExternalMethodDispatch *	dispatch,
								OSObject *					target,
								void *						reference)
{
	if (fGate) {
		HIDCommandGateArgs args;
		
		args.selector	= selector;
		args.arguments	= arguments;
		args.dispatch	= dispatch;
		args.target		= target;
		args.reference	= reference;
		
		return fGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, target, &IOHIDLibUserClient::externalMethodGated), (void *)&args);
	}
	else {
		return kIOReturnOffline;
	}
}

IOReturn IOHIDLibUserClient::externalMethodGated(void * args)
{
	HIDCommandGateArgs *		cArgs		= (HIDCommandGateArgs *)args;
	uint32_t					selector	= cArgs->selector;
	IOExternalMethodArguments * arguments	= cArgs->arguments;
	IOExternalMethodDispatch *	dispatch	= cArgs->dispatch;
	OSObject *					target		= cArgs->target;
	void *						reference	= cArgs->reference;

	if (selector < (uint32_t) kIOHIDLibUserClientNumCommands)
	{
		dispatch = (IOExternalMethodDispatch *) &sMethods[selector];
		
		if (!target)
			target = this;
	}
	
	return super::externalMethod(selector, arguments, dispatch, target, reference);
}

IOReturn IOHIDLibUserClient::_setQueueAsyncPort(IOHIDLibUserClient * target, void * reference, IOExternalMethodArguments * arguments)
{
	return target->setQueueAsyncPort(target->getQueueForToken(arguments->scalarInput[0]), arguments->asyncWakePort);
}

IOReturn IOHIDLibUserClient::setQueueAsyncPort(IOHIDEventQueue * queue, mach_port_t port)
{
	if ( !queue )
		return kIOReturnBadArgument;

	queue->setNotificationPort(port);

	return kIOReturnSuccess;
}

IOReturn IOHIDLibUserClient::_open(IOHIDLibUserClient * target, void * reference, IOExternalMethodArguments * arguments)
{
	return target->open((IOOptionBits)arguments->scalarInput[0]);
}

IOReturn IOHIDLibUserClient::open(IOOptionBits options)
{
	IOReturn ret = kIOReturnNotPrivileged;
	
	// RY: If this is a keyboard and the client is attempting to seize,
	// the client needs to be admin
	if ( !fNubIsKeyboard || ((options & kIOHIDOptionsTypeSeizeDevice) == 0) )
		ret = clientHasPrivilege(fClient, kIOClientPrivilegeLocalUser);
		
	if (ret != kIOReturnSuccess )
		ret = clientHasPrivilege(fClient, kIOClientPrivilegeAdministrator);

	if (ret != kIOReturnSuccess)
		return ret;

	if (!fNub->IOService::open(this, options))
		return kIOReturnExclusiveAccess;
		
	fCachedOptionBits = options;

	fCachedConsoleUsersSeed = 0;
	resourceNotificationGated();

	return kIOReturnSuccess;
}


IOReturn IOHIDLibUserClient::_close(IOHIDLibUserClient * target, void * reference, IOExternalMethodArguments * arguments)
{
	return target->close();
}

IOReturn IOHIDLibUserClient::close()
{
	fNub->close(this, fCachedOptionBits);

	setValid(false);
	
	fCachedOptionBits = 0;

	// @@@ gvdl: release fWakePort leak them for the time being

	return kIOReturnSuccess;
}

bool
IOHIDLibUserClient::didTerminate( IOService * provider, IOOptionBits options, bool * defer )
{
	if (fGate) {
		fGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDLibUserClient::cleanupGated));
	}
	
	return super::didTerminate(provider, options, defer);
}

void IOHIDLibUserClient::free()
{
	if (fQueueMap) {
		fQueueMap->release();
		fQueueMap = 0;
	}
		
	if (fNub) {
		fNub = 0;
	}

	if (fResourceES) {
		if (fWL)
			fWL->removeEventSource(fResourceES);
		fResourceES->release();
		fResourceES = 0;
	}

	if (fGate) {
		if (fWL)
			fWL->removeEventSource(fGate);
		
		fGate->release();
		fGate = 0;
	}
	
	if ( fWL ) {
		fWL->release();
		fWL = 0;
	}
	
	super::free();
}

IOReturn IOHIDLibUserClient::message(UInt32 type, IOService * provider, void * argument )
{
	if ( !isInactive() && fGate )
		fGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDLibUserClient::messageGated), (void *)type, provider, argument);
	return super::message(type, provider, argument);
}

IOReturn IOHIDLibUserClient::messageGated(UInt32 type, IOService * provider, void * argument )
{
	IOOptionBits options = (uintptr_t)argument;
	switch ( type ) {
		case kIOMessageServiceIsRequestingClose:
			if ((options & kIOHIDOptionsTypeSeizeDevice) && (options != fCachedOptionBits))
				setValid(false);
			break;
			
		case kIOMessageServiceWasClosed:
			if ((options & kIOHIDOptionsTypeSeizeDevice) && (options != fCachedOptionBits)) {
				// instead of calling set valid, let's make sure we still have
				// permission through the resource notification
				fCachedConsoleUsersSeed = 0;
				resourceNotificationGated();
			}
			break;
	};
	
	return kIOReturnSuccess;
}

IOReturn IOHIDLibUserClient::registerNotificationPort(mach_port_t port, UInt32 type, UInt32 refCon)
{
	if (fGate) {
		return fGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDLibUserClient::registerNotificationPortGated), (void *)port, (void *)type, (void *)refCon);
	}
	else {
		return kIOReturnOffline;
	}
}

IOReturn IOHIDLibUserClient::registerNotificationPortGated(mach_port_t port, UInt32 type, UInt32	refCon)
{
	IOReturn kr = kIOReturnSuccess;
	
	switch ( type ) {
		case kIOHIDLibUserClientAsyncPortType:
			fWakePort = port;
			break;
		case kIOHIDLibUserClientDeviceValidPortType:
			fValidPort = port;

			static struct _notifyMsg init_msg = { {
				MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0),
				sizeof (struct _notifyMsg),
				MACH_PORT_NULL,
				MACH_PORT_NULL,
				0,
				0
			} };
			
			if ( fValidMessage ) {
				IOFree(fValidMessage, sizeof (struct _notifyMsg));
				fValidMessage = NULL;
			}

			if ( !fValidPort )
				break;
				
			if ( !(fValidMessage = IOMalloc( sizeof(struct _notifyMsg))) ) {
				kr = kIOReturnNoMemory;
				break;
			}
				
			// Initialize the events available message.
			*((struct _notifyMsg *)fValidMessage) = init_msg;

			((struct _notifyMsg *)fValidMessage)->h.msgh_remote_port = fValidPort;
			
			dispatchMessage(fValidMessage);
			
			break;
		default:
			kr = kIOReturnUnsupported;
			break;
	};

	return kr;
}

void IOHIDLibUserClient::setValid(bool state)
{
	if (fValid == state)
		return;

	if ( !state ) {
		// unmap this memory
		if (fNub && !isInactive()) {
			IOMemoryDescriptor * mem;
			IOMemoryMap * map;

			mem = fNub->getMemoryWithCurrentElementValues();
			
			if ( mem ) {
				map = removeMappingForDescriptor(mem);
				
				if ( map )
					map->release();
			}
		}
		fGeneration++;
	}
	
	// set the queue states
	setStateForQueues(state ? kHIDQueueStateEnable : kHIDQueueStateDisable);
	
	// dispatch message
	dispatchMessage(fValidMessage);
	
	fValid = state;
}

IOReturn IOHIDLibUserClient::dispatchMessage(void * messageIn)
{
	IOReturn ret = kIOReturnError;
	mach_msg_header_t * msgh = (mach_msg_header_t *)messageIn;
	if( msgh) {
		ret = mach_msg_send_from_kernel( msgh, msgh->msgh_size);
		switch ( ret ) {
			case MACH_SEND_TIMED_OUT:/* Already has a message posted */
			case MACH_MSG_SUCCESS:	/* Message is posted */
				break;
		};
	}
	return ret;
}

void IOHIDLibUserClient::setStateForQueues(UInt32 state, IOOptionBits options)
{
	for (u_int token = getNextTokenForToken(0); token != 0; token = getNextTokenForToken(token))
	{
		IOHIDEventQueue *queue = getQueueForToken(token);
		switch (state) {
			case kHIDQueueStateEnable:
				queue->enable();
				break;
			case kHIDQueueStateDisable:
				queue->disable();
				break;
			case kHIDQueueStateClear:
				fNub->stopEventDelivery(queue);
				break;
		}
	}
}

IOReturn IOHIDLibUserClient::clientMemoryForType (
									UInt32					type,
									IOOptionBits *			options,
									IOMemoryDescriptor **	memory )
{
	if (fGate) {
		return fGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDLibUserClient::clientMemoryForTypeGated), (void *)type, (void *)options, (void *)memory);
	}
	else {
		return kIOReturnOffline;
	}
}

IOReturn IOHIDLibUserClient::clientMemoryForTypeGated(
									UInt32					token,
									IOOptionBits *			options,
									IOMemoryDescriptor **	memory )
{
	IOReturn				ret				= kIOReturnNoMemory;
	IOMemoryDescriptor		*memoryToShare	= NULL;
	IOHIDEventQueue			*queue			= NULL;
	
	// if the type is element values, then get that
	if (token == kIOHIDLibUserClientElementValuesType)
	{
		// if we can get an element values ptr
		if (fValid && fNub && !isInactive())
			memoryToShare = fNub->getMemoryWithCurrentElementValues();
	}
	// otherwise, the type is token
	else if (queue = getQueueForToken(token))
	{
		memoryToShare = queue->getMemoryDescriptor();
	}
	// if we got some memory
	if (memoryToShare)
	{
		// Memory will be released by user client
		// when last map is destroyed.

		memoryToShare->retain();

		ret = kIOReturnSuccess;
	}
	
	// set the result
	*options	= 0;
	*memory		= memoryToShare;
	
	return ret;
}


IOReturn IOHIDLibUserClient::_getElementCount(IOHIDLibUserClient * target, void * reference, IOExternalMethodArguments * arguments)
{
	return target->getElementCount(&(arguments->scalarOutput[0]), &(arguments->scalarOutput[1]));
}

IOReturn IOHIDLibUserClient::getElementCount(uint64_t * pOutElementCount, uint64_t * pOutReportElementCount)
{
	uint32_t outElementCount, outReportElementCount;
	
	if (!pOutElementCount || !pOutReportElementCount)
		return kIOReturnBadArgument;
		
	getElements(kHIDElementType, (void *)NULL, &outElementCount);
	getElements(kHIDReportHandlerType, (void*)NULL, &outReportElementCount);
	
	*pOutElementCount		= outElementCount / sizeof(IOHIDElementStruct);
	*pOutReportElementCount	= outReportElementCount / sizeof(IOHIDElementStruct);

	return kIOReturnSuccess;
}

IOReturn IOHIDLibUserClient::_getElements(IOHIDLibUserClient * target, void * reference, IOExternalMethodArguments * arguments)
{
	if ( arguments->structureOutputDescriptor )
		return target->getElements((uint32_t)arguments->scalarInput[0], arguments->structureOutputDescriptor, &(arguments->structureOutputDescriptorSize));
	else
		return target->getElements((uint32_t)arguments->scalarInput[0], arguments->structureOutput, &(arguments->structureOutputSize));
}

IOReturn IOHIDLibUserClient::getElements (uint32_t elementType, void *elementBuffer, uint32_t *elementBufferSize)
{
	OSArray *				array;
	uint32_t				i, bi, count;
	IOHIDElementPrivate *	element;
	IOHIDElementStruct *	elementStruct;
	
	if (elementBuffer && elementBufferSize && !*elementBufferSize)
		return kIOReturnBadArgument;
			
	if (!fNub || isInactive())
		return kIOReturnNotAttached;

	if ( elementType == kHIDElementType )
		array = fNub->_reserved->hierarchElements;
	else
		array = fNub->_reserved->inputInterruptElementArray;
	
	if ( !array )
		return kIOReturnError;
		
	if ( elementBuffer )
		bzero(elementBuffer, *elementBufferSize);
		
	count = array->getCount();
	bi = 0;
	
	for ( i=0; i<count; i++ )
	{
		element = OSDynamicCast(IOHIDElementPrivate, array->getObject(i));
		
		if (!element) continue;
		
		// Passing elementBuffer=0 means we are just attempting to get the count;
		elementStruct = elementBuffer ? &(((IOHIDElementStruct *)elementBuffer)[bi]) : 0;
		
		if ( element->fillElementStruct(elementStruct) )
			bi++;
	}
	
	if (elementBufferSize)
		*elementBufferSize = bi * sizeof(IOHIDElementStruct);
		
	return kIOReturnSuccess;
}

IOReturn IOHIDLibUserClient::getElements(uint32_t elementType, IOMemoryDescriptor * mem, uint32_t *elementBufferSize)
{
	IOReturn				ret = kIOReturnNoMemory;
		
	if (!fNub || isInactive())
		return kIOReturnNotAttached;

	ret = mem->prepare();
	
	if(ret == kIOReturnSuccess)
	{
		void *		elementData;
		uint32_t	elementLength;
		
		elementLength = mem->getLength();

		if ( elementLength )
		{
			elementData = IOMalloc( elementLength );
			
			if ( elementData )
			{
				bzero(elementData, elementLength);

				ret = getElements(elementType, elementData, &elementLength);
				
				if ( elementBufferSize )
					*elementBufferSize = elementLength;

				mem->writeBytes( 0, elementData, elementLength );

				IOFree( elementData, elementLength );
			}
			else
				ret = kIOReturnNoMemory;
		}
		else
			ret = kIOReturnBadArgument;
			
		mem->complete();
	}

	return ret;
}

IOReturn IOHIDLibUserClient::_deviceIsValid(IOHIDLibUserClient * target, void * reference, IOExternalMethodArguments * arguments)
{
	IOReturn	kr;
	bool		status;
	uint64_t	generation;
	
	kr = target->deviceIsValid(&status, &generation);
	
	arguments->scalarOutput[0] = status;
	arguments->scalarOutput[1] = generation;
	
	return kr;
}

IOReturn IOHIDLibUserClient::deviceIsValid(bool *status, uint64_t *generation)
{
	if ( status )
		*status = fValid;
		
	if ( generation )
		*generation = fGeneration;
		
	return kIOReturnSuccess;
}

IOReturn IOHIDLibUserClient::_createQueue(IOHIDLibUserClient * target, void * reference, IOExternalMethodArguments * arguments)
{
	return target->createQueue((uint32_t)arguments->scalarInput[0], (uint32_t)arguments->scalarInput[1], &(arguments->scalarOutput[0]));
}

IOReturn IOHIDLibUserClient::createQueue(uint32_t flags, uint32_t depth, uint64_t * outQueue)
{
	// create the queue (fudge it a bit bigger than requested)
	IOHIDEventQueue * eventQueue = IOHIDEventQueue::withEntries (depth+1, DEFAULT_HID_ENTRY_SIZE);
	
	if ( !eventQueue )
		return kIOReturnNoMemory;
		
	eventQueue->setOptions(flags);
	
	if ( !fValid )
		eventQueue->disable();
		
	// add the queue to the map and set out queue
	*outQueue = (uint64_t)createTokenForQueue(eventQueue);

	eventQueue->release();
	
	return kIOReturnSuccess;
}


IOReturn IOHIDLibUserClient::_disposeQueue(IOHIDLibUserClient * target, void * reference, IOExternalMethodArguments * arguments)
{
	return target->disposeQueue(target->getQueueForToken(arguments->scalarInput[0]));
}

IOReturn IOHIDLibUserClient::disposeQueue(IOHIDEventQueue * queue)
{
	IOReturn ret = kIOReturnSuccess;

	// remove this queue from all elements that use it
	if (fNub && !isInactive())
		ret = fNub->stopEventDelivery (queue);
	
	// remove the queue from the map
	removeQueueFromMap(queue);

	return kIOReturnSuccess;
}

	// Add an element to a queue
IOReturn IOHIDLibUserClient::_addElementToQueue(IOHIDLibUserClient * target, void * reference, IOExternalMethodArguments * arguments)
{
	return target->addElementToQueue(target->getQueueForToken(arguments->scalarInput[0]), (IOHIDElementCookie)arguments->scalarInput[1], (uint32_t)arguments->scalarInput[2], &(arguments->scalarOutput[0]));
}

IOReturn IOHIDLibUserClient::addElementToQueue(IOHIDEventQueue * queue, IOHIDElementCookie elementCookie, uint32_t flags, uint64_t *pSizeChange)
{
	IOReturn	ret		= kIOReturnSuccess;
	UInt32		size	= 0;
	
	size = (queue) ? queue->getEntrySize() : 0;
	
	// add the queue to the element's queues
	if (fNub && !isInactive())
		ret = fNub->startEventDelivery (queue, elementCookie);
		
	*pSizeChange = (queue && (size != queue->getEntrySize()));
	
	return ret;
}
	// remove an element from a queue
IOReturn IOHIDLibUserClient::_removeElementFromQueue (IOHIDLibUserClient * target, void * reference, IOExternalMethodArguments * arguments)
{
	return target->removeElementFromQueue(target->getQueueForToken(arguments->scalarInput[0]), (IOHIDElementCookie)arguments->scalarInput[1], &(arguments->scalarOutput[0]));
}

IOReturn IOHIDLibUserClient::removeElementFromQueue (IOHIDEventQueue * queue, IOHIDElementCookie elementCookie, uint64_t *pSizeChange)
{
	IOReturn	ret		= kIOReturnSuccess;
	UInt32		size	= 0;

	size = (queue) ? queue->getEntrySize() : 0;

	// remove the queue from the element's queues
	if (fNub && !isInactive())
		ret = fNub->stopEventDelivery (queue, elementCookie);

	*pSizeChange = (queue && (size != queue->getEntrySize()));
	
	return ret;
}
	// Check to see if a queue has an element
IOReturn IOHIDLibUserClient::_queueHasElement (IOHIDLibUserClient * target, void * reference, IOExternalMethodArguments * arguments)
{
	return target->queueHasElement(target->getQueueForToken(arguments->scalarInput[0]), (IOHIDElementCookie)arguments->scalarInput[1], &(arguments->scalarOutput[0]));
}

IOReturn IOHIDLibUserClient::queueHasElement (IOHIDEventQueue * queue, IOHIDElementCookie elementCookie, uint64_t * pHasElement)
{
	IOReturn ret = kIOReturnSuccess;

	// check to see if that element is feeding that queue
	bool hasElement = false;
	
	if (fNub && !isInactive())
		ret = fNub->checkEventDelivery (queue, elementCookie, &hasElement);
	
	// set return
	*pHasElement = hasElement;
	
	return ret;
}
	// start a queue
IOReturn IOHIDLibUserClient::_startQueue (IOHIDLibUserClient * target, void * reference, IOExternalMethodArguments * arguments)
{
	return target->startQueue(target->getQueueForToken(arguments->scalarInput[0]));
}

IOReturn IOHIDLibUserClient::startQueue (IOHIDEventQueue * queue)
{
	// start the queue
	queue->start();

	return kIOReturnSuccess;
}

	// stop a queue
IOReturn IOHIDLibUserClient::_stopQueue (IOHIDLibUserClient * target, void * reference, IOExternalMethodArguments * arguments)
{
	return target->stopQueue(target->getQueueForToken(arguments->scalarInput[0]));
}

IOReturn IOHIDLibUserClient::stopQueue (IOHIDEventQueue * queue)
{
	// stop the queue
	queue->stop();

	return kIOReturnSuccess;
}

	// update the feature element value
IOReturn IOHIDLibUserClient::_updateElementValues (IOHIDLibUserClient * target, void * reference, IOExternalMethodArguments * arguments)
{
	return target->updateElementValues(arguments->scalarInput, arguments->scalarInputCount);
}

IOReturn IOHIDLibUserClient::updateElementValues (const uint64_t * lCookies, uint32_t cookieCount)
{
	IOReturn	ret = kIOReturnError;
	
	if (fNub && !isInactive()) {
		uint32_t	cookies[cookieCount];
		
		deflate_vec(cookies, cookieCount, lCookies, cookieCount);
		
		ret = fNub->updateElementValues((IOHIDElementCookie *)cookies, cookieCount);
	}
	
	return ret;
}

	// Set the element values
IOReturn IOHIDLibUserClient::_postElementValues (IOHIDLibUserClient * target, void * reference, IOExternalMethodArguments * arguments)
{
	return target->postElementValues(arguments->scalarInput, arguments->scalarInputCount);
}

IOReturn IOHIDLibUserClient::postElementValues (const uint64_t * lCookies, uint32_t cookieCount)
{
	IOReturn	ret = kIOReturnError;
	
	if (fNub && !isInactive()) {
		uint32_t	cookies[cookieCount];
		
		deflate_vec(cookies, cookieCount, lCookies, cookieCount);
		
		ret = fNub->postElementValues((IOHIDElementCookie *)cookies, cookieCount);
	}
	
	return ret;
}

IOReturn IOHIDLibUserClient::_getReport(IOHIDLibUserClient * target, void * reference, IOExternalMethodArguments * arguments)
{
	if ( arguments->asyncWakePort ) {
		IOReturn		ret;
		IOHIDCompletion tap;
		AsyncParam *	pb = (AsyncParam *)IOMalloc(sizeof(AsyncParam));
		
		if(!pb)
			return kIOReturnNoMemory;

		target->retain();
		
		bcopy(arguments->asyncReference, pb->fAsyncRef, sizeof(OSAsyncReference64));
		pb->fAsyncCount = arguments->asyncReferenceCount;
		tap.target = target;
		tap.action = OSMemberFunctionCast(IOHIDCompletionAction, target, &IOHIDLibUserClient::ReqComplete);
		tap.parameter = pb;

		if ( arguments->structureOutputDescriptor )
			ret = target->getReport(arguments->structureOutputDescriptor, &(arguments->structureOutputDescriptorSize), (IOHIDReportType)arguments->scalarInput[0], (uint32_t)arguments->scalarInput[1], (uint32_t)arguments->scalarInput[2], &tap);
		else
			ret = target->getReport(arguments->structureOutput, &(arguments->structureOutputSize), (IOHIDReportType)arguments->scalarInput[0], (uint32_t)arguments->scalarInput[1], (uint32_t)arguments->scalarInput[2], &tap);
			
		if ( ret ) {
			if ( pb )
				IOFree(pb, sizeof(*pb));
			target->release();
		}
	}
	if ( arguments->structureOutputDescriptor )
		return target->getReport(arguments->structureOutputDescriptor, &(arguments->structureOutputDescriptorSize), (IOHIDReportType)arguments->scalarInput[0], (uint32_t)arguments->scalarInput[1]);
	else
		return target->getReport(arguments->structureOutput, &(arguments->structureOutputSize), (IOHIDReportType)arguments->scalarInput[0], (uint32_t)arguments->scalarInput[1]);
}

IOReturn IOHIDLibUserClient::getReport(void *reportBuffer, uint32_t *pOutsize, IOHIDReportType reportType, uint32_t reportID, uint32_t timeout, IOHIDCompletion * completion)
{
	IOReturn				ret;
	IOMemoryDescriptor *	mem;
		
	mem = IOMemoryDescriptor::withAddress(reportBuffer, *pOutsize, kIODirectionIn);
	if(mem) {
		ret = getReport(mem, pOutsize, reportType, reportID, timeout, completion);
		mem->release();
	}
	else
		ret =  kIOReturnNoMemory;

	return ret;
}

IOReturn IOHIDLibUserClient::getReport(IOMemoryDescriptor * mem, uint32_t * pOutsize, IOHIDReportType reportType, uint32_t reportID, uint32_t timeout, IOHIDCompletion * completion)
{
	IOReturn			ret;
		
	if (fNub && !isInactive()) {
		ret = mem->prepare();
		if(ret == kIOReturnSuccess)
			if (completion) {
				AsyncParam * pb = (AsyncParam *)completion->parameter;
				pb->fMax		= *pOutsize;
				pb->fMem		= mem;
				pb->reportType	= reportType;

				mem->retain();
				
				ret = fNub->getReport(mem, reportType, reportID, timeout, completion);
			}
			else {
				ret = fNub->getReport(mem, reportType, reportID);

				// make sure the element values are updated.
				if (ret == kIOReturnSuccess)
					fNub->handleReport(mem, reportType, kIOHIDReportOptionNotInterrupt);
					
				*pOutsize = mem->getLength();
				mem->complete();
			}
	}
	else
		ret = kIOReturnNotAttached;

	return ret;

}

IOReturn IOHIDLibUserClient::_setReport(IOHIDLibUserClient * target, void * reference, IOExternalMethodArguments * arguments)
{
	IOReturn ret = kIOReturnError;

	if ( arguments->asyncWakePort ) {
		IOHIDCompletion tap;
		AsyncParam *	pb = (AsyncParam *)IOMalloc(sizeof(AsyncParam));
		
		if(!pb)
			return kIOReturnNoMemory;

		target->retain();
		
		bcopy(arguments->asyncReference, pb->fAsyncRef, sizeof(OSAsyncReference64));
		pb->fAsyncCount = arguments->asyncReferenceCount;
		tap.target = target;
		tap.action = OSMemberFunctionCast(IOHIDCompletionAction, target, &IOHIDLibUserClient::ReqComplete);
		tap.parameter = pb;

		if ( arguments->structureInputDescriptor )
			ret = target->setReport( arguments->structureInputDescriptor, (IOHIDReportType)arguments->scalarInput[0], (uint32_t)arguments->scalarInput[1],(uint32_t)arguments->scalarInput[2], &tap);
		else
			ret = target->setReport(arguments->structureInput, arguments->structureInputSize, (IOHIDReportType)arguments->scalarInput[0], (uint32_t)arguments->scalarInput[1], (uint32_t)arguments->scalarInput[2], &tap);
			
		if ( ret ) {
			if ( pb )
				IOFree(pb, sizeof(*pb));
			
			target->release();
		}
	}
	else
		if ( arguments->structureInputDescriptor )
			ret = target->setReport( arguments->structureInputDescriptor, (IOHIDReportType)arguments->scalarInput[0], (uint32_t)arguments->scalarInput[1]);
		else
			ret = target->setReport(arguments->structureInput, arguments->structureInputSize, (IOHIDReportType)arguments->scalarInput[0], (uint32_t)arguments->scalarInput[1]);
			
	return ret;
}

IOReturn IOHIDLibUserClient::setReport(const void *reportBuffer, uint32_t reportBufferSize, IOHIDReportType reportType, uint32_t reportID, uint32_t timeout, IOHIDCompletion * completion)
{
	IOReturn				ret;
	IOMemoryDescriptor *	mem;

	mem = IOMemoryDescriptor::withAddress((void *)reportBuffer, reportBufferSize, kIODirectionOut);
	if(mem) {
		ret = setReport(mem, reportType, reportID, timeout, completion);
		mem->release();
	}
	else
		ret = kIOReturnNoMemory;

	return ret;
}

IOReturn IOHIDLibUserClient::setReport(IOMemoryDescriptor * mem, IOHIDReportType reportType, uint32_t reportID, uint32_t timeout, IOHIDCompletion * completion)
{
	IOReturn			ret;

	if (fNub && !isInactive()) {
		ret = mem->prepare();
		if(ret == kIOReturnSuccess)
			if ( completion ) {
				AsyncParam * pb = (AsyncParam *)completion->parameter;
				pb->fMax		= mem->getLength();
				pb->fMem		= mem;
				pb->reportType	= reportType;

				mem->retain();

				ret = fNub->setReport(mem, reportType, reportID, timeout, completion);
			}
			else {
				ret = fNub->setReport(mem, reportType, reportID);
					
				// make sure the element values are updated.
				if (ret == kIOReturnSuccess)
					fNub->handleReport(mem, reportType, kIOHIDReportOptionNotInterrupt);
				
				mem->complete();
			}
	}
	else
		ret = kIOReturnNotAttached;

	return ret;
}
															
void IOHIDLibUserClient::ReqComplete(void *param, IOReturn res, UInt32 remaining)
{
	fGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDLibUserClient::ReqCompleteGated), param, (void *)res, (void *)remaining);
}

IOReturn IOHIDLibUserClient::ReqCompleteGated(void *param, IOReturn res, UInt32 remaining)
{
	io_user_reference_t args[1];
	AsyncParam * pb = (AsyncParam *)param;

	if(res == kIOReturnSuccess) {
		args[0] = (io_user_reference_t)(pb->fMax - remaining);
		
		// make sure the element values are updated.
		if (fNub && !isInactive())
			fNub->handleReport(pb->fMem, pb->reportType, kIOHIDReportOptionNotInterrupt);
	} else {
		args[0] = 0;
	}
	
	if (pb->fMem) {
		pb->fMem->complete();
		pb->fMem->release();
	}

	sendAsyncResult64(pb->fAsyncRef, res, args, 1);

	IOFree(pb, sizeof(*pb));

	release();
	
	return kIOReturnSuccess;
}


// This section is to track all user queues and hand out unique tokens for 
// particular queues. vtn3
// rdar://5957582 start

enum { kIOHIDLibUserClientQueueTokenOffset = 200 };


u_int IOHIDLibUserClient::createTokenForQueue(IOHIDEventQueue *queue)
{
	u_int index = 0;

	while (OSDynamicCast(IOHIDEventQueue, fQueueMap->getObject(index)))
		index++;
	fQueueMap->setObject(index, queue);

	return (index + kIOHIDLibUserClientQueueTokenOffset);
}


void IOHIDLibUserClient::removeQueueFromMap(IOHIDEventQueue *queue)
{
	OSObject *obj = NULL;

	for (u_int index = 0; obj = fQueueMap->getObject(index); index++)
		if (obj == queue) {
			fQueueMap->replaceObject(index, kOSBooleanFalse);
		}
}


IOHIDEventQueue* IOHIDLibUserClient::getQueueForToken(u_int token)
{
	return OSDynamicCast(IOHIDEventQueue, fQueueMap->getObject(token - kIOHIDLibUserClientQueueTokenOffset));
}


u_int IOHIDLibUserClient::getNextTokenForToken(u_int token)
{
	u_int next_token = (token < kIOHIDLibUserClientQueueTokenOffset) ? 
								kIOHIDLibUserClientQueueTokenOffset : token;
	
	IOHIDEventQueue *queue = NULL;
	
	do {
		next_token++;
		queue = getQueueForToken(next_token);
	}
	while ((next_token < fQueueMap->getCount() + kIOHIDLibUserClientQueueTokenOffset) && (queue == NULL));
	
	if (next_token >= fQueueMap->getCount() + kIOHIDLibUserClientQueueTokenOffset)
		next_token = 0;
	
	return next_token;
}

// rdar://5957582 end

