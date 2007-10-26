/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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
/*
 * Copyright (c) 2002 Apple Computer, Inc.  All rights reserved.
 *
 */

#include "I2CUserClientPrivate.h"

#define super IOUserClient

OSDefineMetaClassAndStructors(I2CUserClient, IOUserClient)

IOExternalMethod *
I2CUserClient::getTargetAndMethodForIndex(IOService **target, UInt32 index)
{
	DLOG("I2CUserClient::getTargetAndMethodForIndex (index=%lu)\n", index);

	static const IOExternalMethod sMethods[kI2CUCNumMethods] =
	{
		{	// kI2CUCOpen
			NULL,	// IOService * determined at runtime below
			(IOMethod) &I2CUserClient::userClientOpen,
			kIOUCScalarIScalarO,
			0,	// no inputs
			0	// no outputs
		},
		{	// kI2CUCClose
			NULL,	// IOService * determined at runtime below
			(IOMethod) &I2CUserClient::userClientClose,
			kIOUCScalarIScalarO,
			0,	// no inputs
			0	// no outputs
		},
		{	// kI2CUCRead
			NULL,	// IOService * determined at runtime below
			(IOMethod) &I2CUserClient::read,
			kIOUCStructIStructO,
			sizeof(I2CReadInput),
			sizeof(I2CReadOutput)
		},
		{	// kI2CUCWrite
			NULL,	// IOService * determined at runtime below
			(IOMethod) &I2CUserClient::write,
			kIOUCStructIStructO,
			sizeof(I2CWriteInput),
			sizeof(I2CWriteOutput)
		},
		{	// kI2CUCRMW
			NULL,	// IOService * determined at runtime below
			(IOMethod) &I2CUserClient::rmw,
			kIOUCStructIStructO,
			sizeof(I2CRMWInput),
			0
		}
	};

	if (index < (UInt32)kI2CUCNumMethods)
	{
		*target = this;
		return((IOExternalMethod *) &sMethods[index]);
	}
	else
	{
		*target = NULL;
		return(NULL);
	}
}

bool I2CUserClient::start(IOService *provider)
{
	DLOG("+I2CUserClient::start provider=%08lx\n", (UInt32)provider);

    if (!super::start(provider)) return(false);

	fProvider = OSDynamicCast(PPCI2CInterface, provider);
	if (!fProvider)
	{
		DLOG("-I2CUserClient::start provider is not a PPCI2CInterface\n");
		return(false);
	}

	fIsOpenLock = IOLockAlloc();
	if (!fIsOpenLock)
	{
		DLOG("-I2CUserClient::start failed to allocate lock\n");
		return(false);
	}

	return(true);
}

void I2CUserClient::stop(IOService *provider)
{
	DLOG("+I2CUserClient::stop\n");

	IOLockFree(fIsOpenLock);

	super::stop(provider);
}

bool I2CUserClient::init(OSDictionary *dict)
{
	DLOG("+I2CUserClient::init\n");
	return(super::init(dict));
}

bool I2CUserClient::attach(IOService *provider)
{
	DLOG("+I2CUserClient::attach\n");
	return(super::attach(provider));
}

void I2CUserClient::detach(IOService *provider)
{
	DLOG("+I2CUserClient::detach\n");
	super::detach(provider);
}	

bool I2CUserClient::initWithTask(task_t owningTask,
		void *security_id, UInt32 type)
{
	DLOG("+I2CUserClient::initWithTask\n");

	// The user task must be running with EUID of 0 to access me
	if (clientHasPrivilege(security_id, kIOClientPrivilegeAdministrator)
			!= kIOReturnSuccess)
	{
		DLOG("-I2CUserClient::initWithTask client is not root!\n");
		return(false);
	}

	if ((!super::initWithTask(owningTask, security_id, type)) ||
	    (!owningTask))
	{
		DLOG("-I2CUserClient::initWithTask failed to init\n");
		return(false);
	}

	fOwningTask = owningTask;
	fIsOpen = false;
	fIsOpenLock = NULL;

	return(true);
}

// This is invoked for IOServiceClose() and from clientDied()
IOReturn I2CUserClient::clientClose(void)
{
	DLOG("+I2CUserClient::clientClose\n");

	terminate();

	return(kIOReturnSuccess);
}

IOReturn I2CUserClient::userClientOpen(void)
{
	IOReturn	ret = kIOReturnSuccess;

	DLOG("+I2CUserClient::userClientOpen\n");

	// We do not enforce exclusive access to our provider (the actual
	// I2C interface), but we do enforce that our user-land client app
	// use open/close semantics when communicating with this user client
	// object.

	// preliminary sanity check
	if (fIsOpen)
	{
		// already open ?!
		return(kIOReturnError);
	}

	// grab the mutex
	IOLockLock(fIsOpenLock);

	// now that we hold the mutex, check again
	if (!fIsOpen)
	{
		fIsOpen = true;
		ret = kIOReturnSuccess;
	}
	else
	{
		ret = kIOReturnError;
	}

	IOLockUnlock(fIsOpenLock);
	return(ret);
}

IOReturn I2CUserClient::userClientClose(void)
{
	IOReturn	ret = kIOReturnSuccess;

	DLOG("+I2CUserClient::userClientClose\n");

	// preliminary sanity check
	if (!fIsOpen)
	{
		// haven't been opened ?!
		return(kIOReturnError);
	}

	// grab the mutex
	IOLockLock(fIsOpenLock);

	// now that we hold the mutex, check again
	if (fIsOpen)
	{
		fIsOpen = false;
		ret = kIOReturnSuccess;
	}
	else
	{
		ret = kIOReturnError;
	}

	IOLockUnlock(fIsOpenLock);
	return(ret);
}

IOReturn I2CUserClient::read( void *inStruct, void *outStruct,
				void *inCount, void *outCount, void *p5, void *p6)
{
	IOByteCount		inputSize, *outputSizeP;
	I2CReadInput	*input;
	I2CReadOutput	*output;
	UInt16			byteCount;
	UInt8			sevenBitAddr;
	int				retries;

	DLOG("+I2CUserClient::read\n");

	// Make sure the client has followed proper open/close semantics
	if (!fIsOpen) return(kIOReturnNotReady);

	input		= (I2CReadInput *)inStruct;
	inputSize	= (IOByteCount)inCount;

	output		= (I2CReadOutput *)outStruct;
	outputSizeP	= (IOByteCount *)outCount;

	// Thoroughly check the arguments before proceeding
	if (!(fProvider && input && inputSize && output && outputSizeP) ||
		(inputSize != sizeof(I2CReadInput)) ||
		(*outputSizeP != sizeof(I2CReadOutput)) ||
		(input->count > kI2CUCBufSize))
	{
		DLOG("-I2CUserClient::read got invalid arguments\n");
		return(kIOReturnBadArgument);
	}

	// Open the bus
	if (!fProvider->openI2CBus(input->busNo))
	{
		DLOG("-I2CUserClient::read failed to open I2C bus\n");
		return(kIOReturnError);
	}

	// Set the transfer mode
	switch (input->mode)
	{
		case kI2CUCDumbMode:
			fProvider->setDumbMode();
			fProvider->setPollingMode(true);
			break;

		case kI2CUCStandardMode:
			fProvider->setStandardMode();
			fProvider->setPollingMode(true);
			break;

		case kI2CUCStandardSubMode:
			fProvider->setStandardSubMode();
			fProvider->setPollingMode(true);
			break;

		case kI2CUCCombinedMode:
			fProvider->setCombinedMode();
			fProvider->setPollingMode(true);
			break;
			
		case kI2CUCDumbIntMode:	
			fProvider->setDumbMode();
			fProvider->setPollingMode(false);  //interrupt mode used
			break;

		case kI2CUCStandardIntMode:
			fProvider->setStandardMode();
			fProvider->setPollingMode(false);  //interrupt mode used
			break;

		case kI2CUCStandardSubIntMode:
			fProvider->setStandardSubMode();
			fProvider->setPollingMode(false);  //interrupt mode used
			break;

		case kI2CUCCombinedIntMode:
			fProvider->setCombinedMode();
			fProvider->setPollingMode(false);  //interrupt mode used
			break;

		default:
			fProvider->closeI2CBus();
			DLOG("-I2CUserClient::read got invalid transfer mode\n");
			return(kIOReturnBadArgument);;;
	}

	// attempt to read into the output data buffer
	byteCount = input->count;	// convert from IOByteCount to UInt16
	sevenBitAddr = input->addr >> 1;	// shift out the R/W bit

	// attempt the read
	retries = 1;

	while (!fProvider->readI2CBus(sevenBitAddr, input->subAddr, output->buf, byteCount))
	{
		if (retries > 0)
		{
			IOLog("I2CUserClient::read read failed, retrying...\n");
			retries--;
		}
		else
		{
			fProvider->closeI2CBus();
			IOLog("I2CUserClient::read cannot read from I2C!!\n");
			output->realCount = 0;
			return(kIOReturnError);
		}
	}

	// release the i2c bus
	fProvider->closeI2CBus();
	output->realCount = input->count;
	return(kIOReturnSuccess);
}

IOReturn I2CUserClient::write( void *inStruct, void *outStruct,
				void *inCount, void *outCount, void *p5, void *p6)
{
	IOByteCount		inputSize, *outputSizeP;
	I2CWriteInput	*input;
	I2CWriteOutput	*output;
	UInt16			byteCount;
	UInt8			sevenBitAddr;
	int				retries;

	DLOG("+I2CUserClient::write\n");

	// Make sure the client has followed proper open/close semantics
	if (!fIsOpen) return(kIOReturnNotReady);

	input		= (I2CWriteInput *)inStruct;
	inputSize	= (IOByteCount)inCount;

	output		= (I2CWriteOutput *)outStruct;
	outputSizeP	= (IOByteCount *)outCount;

	// Thoroughly check the arguments before proceeding
	if (!(fProvider && input && inputSize && output && outputSizeP) ||
		(inputSize != sizeof(I2CWriteInput)) ||
		(*outputSizeP != sizeof(I2CWriteOutput)))
	{
		DLOG("-I2CUserClient::write got invalid arguments\n");
		return(kIOReturnBadArgument);
	}

	// Open the bus
	if (!fProvider->openI2CBus(input->busNo))
	{
		DLOG("-I2CUserClient::write failed to open I2C bus\n");
		return(kIOReturnError);
	}

	// Set the transfer mode
	switch (input->mode)
	{
		case kI2CUCDumbMode:
			fProvider->setDumbMode();
			fProvider->setPollingMode(true);
			break;

		case kI2CUCStandardMode:
			fProvider->setStandardMode();
			fProvider->setPollingMode(true);
			break;

		case kI2CUCStandardSubMode:
			fProvider->setStandardSubMode();
			fProvider->setPollingMode(true);
			break;

		case kI2CUCCombinedMode:
			fProvider->setCombinedMode();
			fProvider->setPollingMode(true);
			break;
			
		case kI2CUCDumbIntMode:	
			fProvider->setDumbMode();
			fProvider->setPollingMode(false);  //interrupt mode used
			break;

		case kI2CUCStandardIntMode:
			fProvider->setStandardMode();
			fProvider->setPollingMode(false);  //interrupt mode used
			break;

		case kI2CUCStandardSubIntMode:
			fProvider->setStandardSubMode();
			fProvider->setPollingMode(false);  //interrupt mode used
			break;

		case kI2CUCCombinedIntMode:
			fProvider->setCombinedMode();
			fProvider->setPollingMode(false);  //interrupt mode used
			break;

		default:
			fProvider->closeI2CBus();
			DLOG("-I2CUserClient::write got invalid transfer mode\n");
			return(kIOReturnBadArgument);;;
	}

	// attempt to write data from the input buffer
	byteCount = input->count;	// convert from IOByteCount to UInt16
	sevenBitAddr = input->addr >> 1;	// shift out the R/W bit

	// attempt the write
	retries = 1;

	while (!fProvider->writeI2CBus(sevenBitAddr, input->subAddr, input->buf, byteCount))
	{
		if (retries > 0)
		{
			IOLog("I2CUserClient::write write failed, retrying...\n");
			retries--;
		}
		else
		{
			fProvider->closeI2CBus();
			IOLog("I2CUserClient::write cannot write to I2C!!\n");
			output->realCount = 0;
			return(kIOReturnError);
		}
	}

	fProvider->closeI2CBus();
	output->realCount = input->count;
	return(kIOReturnSuccess);
}

IOReturn I2CUserClient::rmw( void *inStruct, void *inCount,
				void *p3, void *p4, void *p5, void *p6)
{
    IOByteCount		inputSize;
	I2CRMWInput		*input;
	UInt8			sevenBitAddr, newByte;
	int				retries;

	DLOG("+I2CUserClient::rmw\n");

	// Make sure the client has followed proper open/close semantics
	if (!fIsOpen) return(kIOReturnNotReady);

	input		= (I2CRMWInput *)inStruct;
	inputSize	= (IOByteCount)inCount;

	// Thoroughly check the arguments before proceeding
	if (!(fProvider && input && inputSize) ||
	    (inputSize != sizeof(I2CRMWInput)))
	{
		DLOG("-I2CUserClient::rmw got invalid arguments\n");
		return(kIOReturnBadArgument);
	}

	// Open the bus
	if (!fProvider->openI2CBus(input->busNo))
	{
		DLOG("-I2CUserClient::rmw failed to open I2C bus\n");
		return(kIOReturnError);
	}

	sevenBitAddr = input->addr >> 1;	// shift out the R/W bit

	// Set the read transfer mode
	switch (input->readMode)
	{
		case kI2CUCDumbMode:
			fProvider->setDumbMode();
			fProvider->setPollingMode(true);
			break;

		case kI2CUCStandardMode:
			fProvider->setStandardMode();
			fProvider->setPollingMode(true);
			break;

		case kI2CUCStandardSubMode:
			fProvider->setStandardSubMode();
			fProvider->setPollingMode(true);
			break;

		case kI2CUCCombinedMode:
			fProvider->setCombinedMode();
			fProvider->setPollingMode(true);
			break;
			
		case kI2CUCDumbIntMode:	
			fProvider->setDumbMode();
			fProvider->setPollingMode(false);  //interrupt mode used
			break;

		case kI2CUCStandardIntMode:
			fProvider->setStandardMode();
			fProvider->setPollingMode(false);  //interrupt mode used
			break;

		case kI2CUCStandardSubIntMode:
			fProvider->setStandardSubMode();
			fProvider->setPollingMode(false);  //interrupt mode used
			break;

		case kI2CUCCombinedIntMode:
			fProvider->setCombinedMode();
			fProvider->setPollingMode(false);  //interrupt mode used
			break;

		default:
			fProvider->closeI2CBus();
			DLOG("-I2CUserClient::rmw got invalid read transfer mode\n");
			return(kIOReturnBadArgument);
	}

	// attempt the read
	retries = 1;

	while (!fProvider->readI2CBus(sevenBitAddr, input->subAddr, &newByte, 1))
	{
		if (retries > 0)
		{
			IOLog("I2CUserClient::rmw read failed, retrying...\n");
			retries--;
		}
		else
		{
			fProvider->closeI2CBus();
			IOLog("I2CUserClient::rmw cannot read from I2C!!\n");
			return(kIOReturnError);
		}
	}

	// Apply the mask/value
	newByte = (newByte & ~input->mask) | (input->value & input->mask);

	// Set the write transfer mode
	switch (input->writeMode)
	{
		case kI2CUCDumbMode:
			fProvider->setDumbMode();
			fProvider->setPollingMode(true);
			break;

		case kI2CUCStandardMode:
			fProvider->setStandardMode();
			fProvider->setPollingMode(true);
			break;

		case kI2CUCStandardSubMode:
			fProvider->setStandardSubMode();
			fProvider->setPollingMode(true);
			break;

		case kI2CUCCombinedMode:
			fProvider->setCombinedMode();
			fProvider->setPollingMode(true);
			break;
			
		case kI2CUCDumbIntMode:	
			fProvider->setDumbMode();
			fProvider->setPollingMode(false);  //interrupt mode used
			break;

		case kI2CUCStandardIntMode:
			fProvider->setStandardMode();
			fProvider->setPollingMode(false);  //interrupt mode used
			break;

		case kI2CUCStandardSubIntMode:
			fProvider->setStandardSubMode();
			fProvider->setPollingMode(false);  //interrupt mode used
			break;

		case kI2CUCCombinedIntMode:
			fProvider->setCombinedMode();
			fProvider->setPollingMode(false);  //interrupt mode used
			break;

		default:
			fProvider->closeI2CBus();
			DLOG("-I2CUserClient::rmw got invalid write transfer mode\n");
			return(kIOReturnBadArgument);;;
	}

	// attempt the write
	retries = 1;

	while (!fProvider->writeI2CBus(sevenBitAddr, input->subAddr, &newByte, 1))
	{
		if (retries > 0)
		{
			IOLog("I2CUserClient::rmw write failed, retrying...\n");
			retries--;
		}
		else
		{
			fProvider->closeI2CBus();
			IOLog("I2CUserClient::rmw cannot write to I2C!!\n");
			return(kIOReturnError);
		}
	}

	// release the i2c bus
	fProvider->closeI2CBus();
	return(kIOReturnSuccess);
}
