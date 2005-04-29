/*	File: $Id: IOI2CMaxim1989.cpp,v 1.3 2004/12/15 04:44:51 jlehrer Exp $
 *
 * Copyright (c) 2004 Apple Computer, Inc.  All rights reserved.
 *
 *		$Log: IOI2CMaxim1989.cpp,v $
 *		Revision 1.3  2004/12/15 04:44:51  jlehrer
 *		[3867728] Support for failed hardware.
 *		
 *		Revision 1.2  2004/09/18 01:12:01  jlehrer
 *		Removed APSL header.
 *		
 *
 *
 */

#include "IOI2CMaxim1989.h"

#define super IOI2CDevice
OSDefineMetaClassAndStructors(IOI2CMaxim1989, IOI2CDevice)

void IOI2CMaxim1989::free( void )
{
	super::free();
}

bool IOI2CMaxim1989::start( IOService *nub )
{
	IOReturn		status;

	DLOG("IOI2CMaxim1989::start - entered\n");

	if (false == super::start(nub))
		return false;

	if (!fGetSensorValueSym)
		fGetSensorValueSym = OSSymbol::withCString("getSensorValue");

	UInt8 deviceID;

	if (kIOReturnSuccess != (status = readI2C(kReadDeviceID, &deviceID, 1)))
	{
		IOLog("IOI2CMaxim1989@%lx::start device not responding!\n", getI2CAddress());
		freeI2CResources();
		return false;
	}

	// tell the world i'm here
	registerService();

	// Publish any child nubs under the max1989 node...
	publishChildren(nub);

	return(true);
}

void IOI2CMaxim1989::stop( IOService *nub )
{
	DLOG("IOI2CMaxim1989::stop - entered\n");

	// Execute any functions flagged as "on termination"
	performFunctionsWithFlags (kIOPFFlagOnTerm);

	if (fGetSensorValueSym) { fGetSensorValueSym->release(); fGetSensorValueSym = NULL; }

	super::stop( nub );
}

/*******************************************************************************
 * IOHWSensor entry point - callPlatformFunction()
 *******************************************************************************/

IOReturn IOI2CMaxim1989::callPlatformFunction(const OSSymbol *functionName,
				bool waitForFunction, void *param1, void *param2,
				void *param3, void *param4)
{
	UInt32 maximReg = (UInt32)param1;
	SInt32 *temp_buf = (SInt32 *)param2;

	DLOG("IOI2CMaxim1989::callPlatformFunction(%x) %s %s %08lx %08lx %08lx %08lx\n",
			fI2CAddress, functionName->getCStringNoCopy(), waitForFunction ? "TRUE" : "FALSE",
			(UInt32) param1, (UInt32) param2, (UInt32) param3, (UInt32) param4);

	if (functionName->isEqualTo(fGetSensorValueSym) == true)
	{
		if (isI2COffline() == true)
			return(kIOReturnOffline);

		if (temp_buf == NULL)
			return(kIOReturnBadArgument);

		return(getTemp(maximReg, temp_buf));
	}

	return(super::callPlatformFunction(functionName, waitForFunction,
				param1, param2, param3, param4));
}

/*******************************************************************************
 * Publish the children nubs for the Maxim 1989 nub - publishChildren()
 *******************************************************************************/

IOReturn IOI2CMaxim1989::publishChildren(IOService *nub)
{
    OSIterator			*childIterator = NULL;
    IORegistryEntry		*childEntry = NULL;
    IOService			*childNub = NULL;

	childIterator = nub->getChildIterator(gIODTPlane);
	if( childIterator != NULL )
	{
		// Iterate through children and create nubs
		while ( ( childEntry = (IORegistryEntry *)( childIterator->getNextObject() ) ) != NULL )
		{
			// Publish child as IOService
			childNub = OSDynamicCast(IOService, OSMetaClass::allocClassWithName("IOService"));
		
			childNub->init(childEntry, gIODTPlane);
			childNub->attach(this);
			childNub->registerService();
                           			
			DLOG("IOI2CMaxim1989::publishChildren(0x%x) published child %s\n", getI2CAddress(), childEntry->getName());
		}
	
		childIterator->release();
	}

	return kIOReturnSuccess;
}

#pragma mark -
#pragma mark *** Read Temperature Channels ***
#pragma mark -

/*******************************************************************************
 * Read temperature channels from the device
 *******************************************************************************/

IOReturn IOI2CMaxim1989::getTemp( UInt32 maximReg, SInt32 * temp )
{
	IOReturn			status;
	UInt8				integer;

	// get the temperature associated with maximReg register
	if (kIOReturnSuccess != (status = readI2C( maximReg, &integer, 1 )))
	{
		IOLog("IOI2CMaxim1989::getTemp read temp failed!\n");
		*temp = -1;
		return status;
	}

	// format the 16.16 fixed point temperature and return it
	*temp = (((SInt32)integer << 16) & 0x00FF0000);
	return status;
}
