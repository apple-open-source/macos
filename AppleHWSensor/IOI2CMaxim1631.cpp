/********************************************************************
 *
 *      File: $Id: IOI2CMaxim1631.cpp,v 1.4 2005/05/18 03:42:11 galcher Exp $
 *
 * Copyright (c) 2005 Apple Computer, Inc.  All rights reserved.
 *
 *
 *******************************************************************/

#include "IOI2CMaxim1631.h"


#define super IOI2CDevice
OSDefineMetaClassAndStructors(IOI2CMaxim1631, IOI2CDevice)


/*
 * -----------------------------------------------------------------
 *  free
 * -----------------------------------------------------------------
 */

void IOI2CMaxim1631::free( void )
{
	super::free();
}


/*
 * -----------------------------------------------------------------
 *  start
 * -----------------------------------------------------------------
 */

bool IOI2CMaxim1631::start( IOService *nub )
{
IOReturn	status;
UInt8		configReg, cmdByte;

	DLOG("IOI2CMaxim1631::start - entered\n");

	if (false == super::start(nub))
	{
		DLOG( "IOI2CMaxim1631::start - super::start failed.  Exiting...\n" );
		return false;
	}

	if (!fGetSensorValueSym)
		fGetSensorValueSym = OSSymbol::withCString("getSensorValue");

	// According to the I2C gurus, accessing the config register is done by a COMBINED mode
	// (default for readI2C) I2C access with the 'kAccessConfigurationByte' command as the
	// "sub-addr" value.

	if (kIOReturnSuccess != (status = readI2C( kAccessConfigurationByte, &configReg, 1 )))
	{
		IOLog("IOI2CMaxim1631@%lx::start unable to read config reg\n", getI2CAddress());
		freeI2CResources();
		return false;
	}
	DLOG( "IOI2CMaxim1631::start - read config register- value = 0x%02X\n", configReg );
	if ( (configReg & 0x01) != 0 )	// not in continuous conversion mode (bit 0 == 1 is 1SHOT mode)
	{
		IOLog( "IOI2CMaxim1631::start - Note: not in continuous conversion mode. Setting mode.\n" );
		// should we also be setting the resolution bits here? -- bg
		configReg &= ~0x01;	// set 1SHOT bit to zero for continuous conversion mode
		// to write to the config register, you perform a Standard SubAddr I2C transaction
		// (which is the default mode for writeI2C()), specifying 'kAccessConfigurationByte'
		// command as the sub-addr.
		if ( kIOReturnSuccess != ( status = writeI2C( kAccessConfigurationByte, &configReg, 1 )) )
		{
			IOLog( "IOI2CMaxim1631::start - unable to turn off 1SHOT mode! Cannot provide temperature values.\n" );
			freeI2CResources();
			return false;
		}
	}

	// Tell the sensor to go into continuous temp. conversion mode.
	//	Talked to the I2C gurus and according to the diagram for the interface diagram
	//	for issuing the 'kStartConvertT' command, one needs to issue a Standard I2C write
	//	transaction.  The data buffer contains one (1) byte, which is the command byte,
	//	and we override the default "mode" so that IOI2CFamily issues the correct type
	//	of transaction. [rdar://problem/4118773]

	cmdByte = kStartConvertT;
	if ( kIOReturnSuccess != ( status = writeI2C( 0 /* no subaddr */, &cmdByte, 1, kIOI2C_CLIENT_KEY_DEFAULT, kI2CMode_Standard ) ) )
	{
		IOLog( "IOI2CMaxim1631::start - unable to start sensor (status = 0x%08X)\n", status );
	}

	// tell the world i'm here
	registerService();

	// Publish any child nubs under the max1631 node...
	publishChildren(nub);

	return(true);
}


/*
 * -----------------------------------------------------------------
 *  stop
 * -----------------------------------------------------------------
 */

void IOI2CMaxim1631::stop( IOService *nub )
{
	DLOG("IOI2CMaxim1631::stop - entered\n");

	// Execute any functions flagged as "on termination"
	performFunctionsWithFlags (kIOPFFlagOnTerm);

	if (fGetSensorValueSym)
	{
		fGetSensorValueSym->release();
		fGetSensorValueSym = NULL;
	}

	super::stop( nub );
}



/*
 * -----------------------------------------------------------------
 *  callPlatformFunction
 * -----------------------------------------------------------------
 */

/*******************************************************************************
 * IOHWSensor entry point - callPlatformFunction()
 *******************************************************************************/

IOReturn IOI2CMaxim1631::callPlatformFunction(const OSSymbol *functionName,
				bool waitForFunction, void *param1, void *param2,
				void *param3, void *param4)
{
	UInt32 maximReg = (UInt32)param1;
	SInt32 *temp_buf = (SInt32 *)param2;

	DLOG("IOI2CMaxim1631::callPlatformFunction(%x) %s %s %08lx %08lx %08lx %08lx\n",
			fI2CAddress, functionName->getCStringNoCopy(), waitForFunction ? "TRUE" : "FALSE",
			(UInt32) param1, (UInt32) param2, (UInt32) param3, (UInt32) param4);

	if (functionName->isEqualTo(fGetSensorValueSym) == true)
	{
		if (isI2COffline() == true)
			return( kIOReturnOffline );

		if (temp_buf == NULL)
			return( kIOReturnBadArgument );

		return(getTemp( maximReg, temp_buf ));
	}

	return(super::callPlatformFunction(functionName, waitForFunction,
				param1, param2, param3, param4));
}



/*
 * -----------------------------------------------------------------
 *  publishChildren
 * -----------------------------------------------------------------
 */

/*******************************************************************************
 * Publish the children nubs for the Maxim 1631 nub - publishChildren()
 *******************************************************************************/

IOReturn IOI2CMaxim1631::publishChildren(IOService *nub)
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
                           			
			DLOG("IOI2CMaxim1631::publishChildren(0x%x) published child %s\n", getI2CAddress(), childEntry->getName());
		}
	
		childIterator->release();
	}

	return kIOReturnSuccess;
}

#pragma mark -
#pragma mark *** Read Temperature Channels ***
#pragma mark -

/*
 * -----------------------------------------------------------------
 *  getTemp
 * -----------------------------------------------------------------
 */

/*******************************************************************************
 * Read temperature channels from the device
 *
 *	The 1631 is a really dumb device.  Instead of sending an I2C command
 *	specifying a register to read the temperature, you actually send a
 *	Read The Current Temperature byte to the device and specify the buffer
 *	to read the data into.  The temperature is a 16-bit value and since the
 *	IOI2C readi2c() command does not allow you to specify anything other
 *	than a UInt8 as the buffer address, we -union- a UInt16 with a 2-byte
 *	UInt8 buffer and request to read the 2 bytes that way.  the data
 *	returned is two bytes, the most-significant is 1/sign, 7/ordinal temp.
 *	The least-significant byte is 4/fraction, 4/0, assuming 12-bit
 *	temperature resolution (see ::start, kAccessConfigurationByte R/W),
 *	which is the default resolution when the part starts up.
 *******************************************************************************/

IOReturn IOI2CMaxim1631::getTemp( UInt32 maximReg /* unused */, SInt32 * temp )
{
IOReturn	status;
union
{
	UInt16		aShort;
	UInt8		aByte[2];
} readBuffer;

	// get the temperature.
	//	this is done by issuing a COMBINED (default for readI2C) I2C request, with
	//	'kReadCurrentTemp' as the "sub-addr' and reading 2 bytes, since the current
	//	temp register is 16 bits.

	if (kIOReturnSuccess == (status = readI2C( kReadCurrentTemp, &readBuffer.aByte[0], 2 )))
	{
		// temperature register format:
		//
		//	MS byte:  1/sign, 7 integer
		//	LS byte:  4/fraction, 4/zero
		
		// format the 16.16 fixed point temperature and return it
		// ( the 0xFFFFF000 allows for the propagation of the sign bit,
		//   which could happen in the case of a very cold room inlet temperature )

		*temp = ( ( ( ( SInt16 ) readBuffer.aShort ) << 8 ) & 0xFFFFF000 );
	
		//DLog( "IOI2CMaxim1631::getTemp - raw data = 0x%04X, returning %08lX\n",
		//					i2cBuffer.aShort,   *temp );
	}
	else
	{
		IOLog( "IOI2CMaxim1631::getTemp - unable to read temperature (status = 0x%08X)\n", status );
		*temp = 0x7FFFFFFF;	// pass back very large positive number - will probably set off warning`
	}

	return status;
}
