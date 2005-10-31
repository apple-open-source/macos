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
 *	File: $Id: IOI2CBus.cpp,v 1.4 2005/07/01 16:09:52 bwpang Exp $
 *
 *  DRI: Joseph Lehrer
 *
 *		$Log: IOI2CBus.cpp,v $
 *		Revision 1.4  2005/07/01 16:09:52  bwpang
 *		[4086434] added APSL headers
 *		
 *		Revision 1.3  2004/09/17 20:30:33  jlehrer
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


#include <IOKit/IODeviceTreeSupport.h>
#include "IOI2CBus.h"
#include "IOI2CDefs.h"


// #define I2C_DEBUG 1

#if (defined(I2C_DEBUG) && I2C_DEBUG)
#define DLOG(fmt, args...)  kprintf(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif



#define super IOService
OSDefineMetaClassAndStructors( IOI2CBus, IOService )

bool
IOI2CBus::start(IOService *provider)
{
    OSData				*regprop;
	OSIterator			*iter;
	IORegistryEntry		*next;
	IOService			*nub;

	DLOG("+IOI2CBus::start\n");

	fProvider = provider;

	if (false == super::start(provider))
		return false;

	if (regprop = OSDynamicCast(OSData, fProvider->getProperty("reg")))
		fI2CBus = *((UInt32 *)regprop->getBytesNoCopy());
	else
		return false;

	// Create some symbols for later use
	symWriteI2CBus = OSSymbol::withCStringNoCopy(kWriteI2Cbus);
	symReadI2CBus = OSSymbol::withCStringNoCopy(kReadI2Cbus);
	symLockI2CBus = OSSymbol::withCStringNoCopy(kLockI2Cbus);
	symUnlockI2CBus = OSSymbol::withCStringNoCopy(kUnlockI2Cbus);

    // publish children...
	if (iter = fProvider->getChildIterator(gIODTPlane))
	{
		while (next = OSDynamicCast(IORegistryEntry, iter->getNextObject()))
		{
			if (nub = OSDynamicCast(IOService, OSMetaClass::allocClassWithName("IOI2CService")))
			{
				if (nub->init(next, gIODTPlane))
				{
					nub->attach(this);
					nub->registerService();
				}
				else
					nub->free();
			}
		}
		iter->release();
	}

	DLOG("-IOI2CBus@%lx::start\n",fI2CBus);

	return true;
}

void
IOI2CBus::stop(
	IOService	*provider)
{
	DLOG("IOI2CBus@%lx::stop\n",fI2CBus);
	super::stop(provider);
}

void
IOI2CBus::free(void)
{
	if (symWriteI2CBus)		{ symWriteI2CBus->release();		symWriteI2CBus = 0; }
	if (symReadI2CBus)		{ symReadI2CBus->release();			symReadI2CBus = 0; }
	if (symLockI2CBus)		{ symLockI2CBus->release();			symLockI2CBus = 0; }
	if (symUnlockI2CBus)	{ symUnlockI2CBus->release();		symUnlockI2CBus = 0; }

	super::free();
}

IOReturn
IOI2CBus::callPlatformFunction(
	const OSSymbol *functionName,
	bool waitForFunction,
	void *param1, void *param2,
	void *param3, void *param4 )
{
	if (symReadI2CBus->isEqualTo(functionName) ||
		symWriteI2CBus->isEqualTo(functionName))
	{
		((IOI2CCommand *)param1)->bus = fI2CBus;
	}
	else
	if (symLockI2CBus->isEqualTo(functionName) ||
		symUnlockI2CBus->isEqualTo(functionName))
	{
		param1 = (void *)fI2CBus;
	}

	return super::callPlatformFunction(functionName, waitForFunction, param1, param2, param3, param4);
}


// Space reserved for future expansion.
OSMetaClassDefineReservedUnused ( IOI2CBus, 0 );
OSMetaClassDefineReservedUnused ( IOI2CBus, 1 );
OSMetaClassDefineReservedUnused ( IOI2CBus, 2 );
OSMetaClassDefineReservedUnused ( IOI2CBus, 3 );
OSMetaClassDefineReservedUnused ( IOI2CBus, 4 );
OSMetaClassDefineReservedUnused ( IOI2CBus, 5 );
OSMetaClassDefineReservedUnused ( IOI2CBus, 6 );
OSMetaClassDefineReservedUnused ( IOI2CBus, 7 );
