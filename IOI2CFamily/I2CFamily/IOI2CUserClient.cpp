/*
 * Copyright (c) 1998-2003 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2004 Apple Computer, Inc.  All rights reserved.
 *
 *	File: $Id: IOI2CUserClient.cpp,v 1.5 2005/07/01 16:09:52 bwpang Exp $
 *
 *  DRI: Joseph Lehrer
 *
 *		$Log: IOI2CUserClient.cpp,v $
 *		Revision 1.5  2005/07/01 16:09:52  bwpang
 *		[4086434] added APSL headers
 *		
 *		Revision 1.4  2004/12/15 00:53:10  jlehrer
 *		Enabled specifying options, bus, and address from the user client.
 *		
 *		Revision 1.3  2004/09/17 20:22:56  jlehrer
 *		Removed ASPL headers.
 *		
 *		Revision 1.2  2004/06/08 23:45:15  jlehrer
 *		Added ERRLOG, disabled DLOG, changed DLOGI2C to use runtime cmd.option flag.
 *		
 *		Revision 1.1  2004/06/07 21:53:41  jlehrer
 *		Initial Checkin
 *		
 *
 */

#include "IOI2CUserClient.h"
#include "IOI2CDevice.h"

#ifdef DLOG
#undef DLOG
#endif

// Uncomment to enable debug output
//#define I2CUSERCLIENT_DEBUG 1

#ifdef I2CUSERCLIENT_DEBUG
#define DLOG(fmt, args...)	kprintf(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

#define I2C_ERRLOG 1

#if (defined(I2C_ERRLOG) && I2C_ERRLOG)
#define ERRLOG(fmt, args...)  kprintf(fmt, ## args)
#else
#define ERRLOG(fmt, args...)
#endif


#define super IOUserClient
OSDefineMetaClassAndStructors(IOI2CUserClient, IOUserClient)

bool IOI2CUserClient::initWithTask(
	task_t		owningTask,
	void		*security_id,
	UInt32		type)
{
	DLOG("+IOI2CUserClient::initWithTask\n");

	// The user task must be running with EUID of 0 to access me
	if (kIOReturnSuccess != clientHasPrivilege(security_id, kIOClientPrivilegeAdministrator))
	{
		ERRLOG("-IOI2CUserClient::initWithTask client is not root!\n");
		return false;
	}

	if (!owningTask || !super::initWithTask(owningTask, security_id, type))
	{
		ERRLOG("-IOI2CUserClient::initWithTask failed to init\n");
		return false;
	}

	fOwningTask = owningTask;

	return true;
}

bool
IOI2CUserClient::start(
	IOService *provider)
{
	DLOG("+IOI2CUserClient::start provider=%08lx\n", (UInt32)provider);

    if ( ! super::start(provider) )
		return false;

	// Create some symbols for later use
	symLockI2CBus = OSSymbol::withCStringNoCopy(kLockI2Cbus);
	symUnlockI2CBus = OSSymbol::withCStringNoCopy(kUnlockI2Cbus);
	symWriteI2CBus = OSSymbol::withCStringNoCopy(kWriteI2Cbus);
	symReadI2CBus = OSSymbol::withCStringNoCopy(kReadI2Cbus);

	fProvider = provider;

	return true;
}

void IOI2CUserClient::free(void)
{
	DLOG("+IOI2CUserClient::free\n");

	if (symLockI2CBus)		{ symLockI2CBus->release();		symLockI2CBus = 0; }
	if (symUnlockI2CBus)	{ symUnlockI2CBus->release();	symUnlockI2CBus = 0; }
	if (symWriteI2CBus)		{ symWriteI2CBus->release();	symWriteI2CBus = 0; }
	if (symReadI2CBus)		{ symReadI2CBus->release();		symReadI2CBus = 0; }

	super::free();
}


// This is invoked for IOServiceClose() and from clientDied()
IOReturn IOI2CUserClient::clientClose(void)
{
	DLOG("+IOI2CUserClient::clientClose\n");

	if (fClientKey)
		unlockI2CBus(fClientKey);
	fClientKey = 0;
	terminate();

	return(kIOReturnSuccess);
}

IOExternalMethod *
IOI2CUserClient::getTargetAndMethodForIndex(
	IOService	**target,
	UInt32		index)
{
	DLOG("IOI2CUserClient::getTargetAndMethodForIndex (index=%lu/%d)\n", index, kI2CUCNumMethods);

	static const IOExternalMethod sMethods[kI2CUCNumMethods] =
	{
		{	// kI2CUCLock
			NULL,	// IOService * determined at runtime below
			(IOMethod) &IOI2CUserClient::lockI2CBus,
			kIOUCScalarIScalarO,
			1,	// 1 input: bus
			1	// 1 output: &key
		},
		{	// kI2CUCUnlock
			NULL,	// IOService * determined at runtime below
			(IOMethod) &IOI2CUserClient::unlockI2CBus,
			kIOUCScalarIScalarO,
			1,	// 1 input: key
			0	// no outputs
		},
		{	// kI2CUCRead
			NULL,	// IOService * determined at runtime below
			(IOMethod) &IOI2CUserClient::readI2CBus,
			kIOUCStructIStructO,
			sizeof(I2CUserReadInput),
			sizeof(I2CUserReadOutput)
		},
		{	// kI2CUCWrite
			NULL,	// IOService * determined at runtime below
			(IOMethod) &IOI2CUserClient::writeI2CBus,
			kIOUCStructIStructO,
			sizeof(I2CUserWriteInput),
			sizeof(I2CUserWriteOutput)
		},
		{	// kI2CUCRMW
			NULL,	// IOService * determined at runtime below
			(IOMethod) &IOI2CUserClient::readModifyWriteI2CBus,
			kIOUCStructIStructO,
			sizeof(I2CRMWInput),
			0
		}
	};

	if (index < (UInt32)kI2CUCNumMethods)
	{
		*target = this;
		return ((IOExternalMethod *) &sMethods[index]);
	}

	ERRLOG("IOI2CUserClient::getTargetAndMethodForIndex (index=%lu) Not Found\n", index);
	*target = NULL;
	return NULL;
}


IOReturn
IOI2CUserClient::lockI2CBus(
	UInt32		bus,
	UInt32		*clientKeyRef)
{
	IOReturn status;
	DLOG("+IOI2CUserClient::lockI2CBus\n");

	// If the user is confused try to prevent a deadlock.
	if (fClientKey != 0)
	{
		ERRLOG("-IOI2CUserClient::lockI2CBus already locked!\n");
		*clientKeyRef = kIOI2C_CLIENT_KEY_INVALID;
		return kIOReturnExclusiveAccess;
	}

	if (kIOReturnSuccess == (status = fProvider->callPlatformFunction(symLockI2CBus, false,
						(void *)bus, (void *)&fClientKey, (void *)0, (void *)0)))
		*clientKeyRef = fClientKey;
	else
		*clientKeyRef = kIOI2C_CLIENT_KEY_INVALID;

	return status;
}

IOReturn
IOI2CUserClient::unlockI2CBus(UInt32 clientKey)
{
	DLOG("+IOI2CUserClient::unlockI2CBus\n");
	if (clientKey != fClientKey)
	{
		ERRLOG("IOI2CUserClient::unlockI2CBus with wrong key: %lx != %lx\n", clientKey, fClientKey);
	}

	fClientKey = 0;
	return fProvider->callPlatformFunction(symUnlockI2CBus, false,
						(void *)0, (void *)clientKey, (void *)0, (void *)0);
}

IOReturn
IOI2CUserClient::readI2CBus(
	I2CUserReadInput	*input,
	I2CUserReadOutput	*output,
	IOByteCount		inputSize,
	IOByteCount		*outputSizeP,
	void			*p5,
	void			*p6)
{
	IOReturn		status = kIOReturnSuccess;

	DLOG("+IOI2CUserClient::readI2CBus\n");

	// Thoroughly check the arguments before proceeding
	if (!(fProvider
		&& input
		&& output
		&& outputSizeP
		&& (inputSize == sizeof(I2CUserReadInput))
		&& (*outputSizeP == sizeof(I2CUserReadOutput))
		&& (input->count <= kI2CUCBufSize)) )
	{
		ERRLOG("-IOI2CUserClient::readI2CBus got invalid arguments\n");
		return kIOReturnBadArgument;
	}

	{
		IOI2CCommand cmd = {0};
		cmd.subAddress = input->subAddr;
		cmd.buffer = output->buf;
		cmd.count = input->count;
		cmd.mode = input->mode;
		cmd.bus = input->busNo;
		cmd.address = input->addr;
		cmd.options = input->options;

		DLOG("IOI2CUserClient::readI2CBus cmd key:%lx, B:%lx, A:%lx S:%lx, L:%lx, M:%lx\n",
			input->key, cmd.bus, cmd.address, cmd.subAddress, cmd.count, cmd.mode);

		status = fProvider->callPlatformFunction(symReadI2CBus, false,
						(void *)&cmd, (void *)input->key, (void *)0, (void *)0);

		if (status != kIOReturnSuccess)
			output->realCount = 0;
		else
			output->realCount = input->count;
	}

	return status;
}

IOReturn
IOI2CUserClient::writeI2CBus(
	I2CUserWriteInput	*input,
	I2CUserWriteOutput	*output,
	IOByteCount		inputSize,
	IOByteCount		*outputSizeP,
	void			*p5,
	void			*p6)
{
	IOReturn		status = kIOReturnSuccess;

	DLOG("+IOI2CUserClient::writeI2CBus\n");

	// Thoroughly check the arguments before proceeding
	if (!(fProvider
		&& input
		&& output
		&& outputSizeP
		&& (inputSize == sizeof(I2CUserWriteInput))
		&& (*outputSizeP == sizeof(I2CUserWriteOutput)) ) )
	{
		ERRLOG("-IOI2CUserClient::writeI2CBus got invalid arguments\n");
		return kIOReturnBadArgument;
	}

	{
		IOI2CCommand cmd = {0};
		cmd.subAddress = input->subAddr;
		cmd.buffer = input->buf;
		cmd.count = input->count;
		cmd.mode = input->mode;
		cmd.bus = input->busNo;
		cmd.address = input->addr;
		cmd.options = input->options;

		status = fProvider->callPlatformFunction(symWriteI2CBus, false,
						(void *)&cmd, (void *)input->key, (void *)0, (void *)0);

		if (status != kIOReturnSuccess)
			output->realCount = 0;
		else
			output->realCount = input->count;
	}

	DLOG("-IOI2CUserClient::writeI2CBus\n");
	return status;
}

IOReturn IOI2CUserClient::readModifyWriteI2CBus(
	I2CRMWInput		*input,
	IOByteCount		inputSize,
	void *p3, void *p4, void *p5, void *p6)
{
	DLOG("IOI2CUserClient::readModifyWriteI2CBus\n");
	ERRLOG("IOI2CUserClient::readModifyWriteI2CBus --- WARNING: method not supported\n");
	return kIOReturnUnsupported;
}


// Space reserved for future expansion.
OSMetaClassDefineReservedUnused ( IOI2CUserClient, 0 );
OSMetaClassDefineReservedUnused ( IOI2CUserClient, 1 );
OSMetaClassDefineReservedUnused ( IOI2CUserClient, 2 );
OSMetaClassDefineReservedUnused ( IOI2CUserClient, 3 );
OSMetaClassDefineReservedUnused ( IOI2CUserClient, 4 );
OSMetaClassDefineReservedUnused ( IOI2CUserClient, 5 );
OSMetaClassDefineReservedUnused ( IOI2CUserClient, 6 );
OSMetaClassDefineReservedUnused ( IOI2CUserClient, 7 );
