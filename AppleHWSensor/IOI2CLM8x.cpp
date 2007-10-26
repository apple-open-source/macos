/*
 * Copyright (c) 2004 Apple Computer, Inc.  All rights reserved.
 *
 * File: $Id: IOI2CLM8x.cpp,v 1.4 2005/10/15 00:31:49 tsherman Exp $
 *
 *		$Log: IOI2CLM8x.cpp,v $
 *		Revision 1.4  2005/10/15 00:31:49  tsherman
 *		AppleLM8x, IOI2CLM8x: Changed initHW() routine to force configuration register to 0x1.
 *		
 *		Revision 1.3  2005/04/11 23:39:37  dirty
 *		[4078743] Properly handle negative temperatures.
 *		
 *		Revision 1.2  2004/12/15 04:44:51  jlehrer
 *		[3867728] Support for failed hardware.
 *		
 *		Revision 1.1  2004/09/18 00:55:36  jlehrer
 *		Initial checkin.
 *		
 *
 */

#include <IOKit/IODeviceTreeSupport.h>
#include "IOI2CLM8x.h"

// Uncomment for debug info
// #define IOI2CLM8x_DEBUG 1

#ifdef IOI2CLM8x_DEBUG
#define DLOG(fmt, args...)			kprintf(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

#define ERRLOG(fmt, args...)		kprintf(fmt, ## args)


#define super IOI2CDevice
OSDefineMetaClassAndStructors(IOI2CLM8x, IOI2CDevice)

bool IOI2CLM8x::start(
	IOService			*provider)
{
	// Create symbols for later use
	sGetSensorValueSym = OSSymbol::withCString("getSensorValue");
	fSavedRegisters = (savedRegisters_t *)IOMalloc(sizeof(savedRegisters_t));

	if ((sGetSensorValueSym == NULL)	||
		(fSavedRegisters == NULL)		||
		(false == super::start(provider)))
	{
		return false;
	}

	// Configure the LM87
	if (kIOReturnSuccess != initHW(fProvider))
	{
		DLOG("-IOI2CLM8x::start(0x%lx) device not responding\n", getI2CAddress());
		freeI2CResources();
		return false;
	}

	saveRegisters();

	// Register so others can find us with a waitForService()
	registerService();

	// Parse sensor properties and create nubs
	publishChildren(provider);

	return true;
}

void IOI2CLM8x::free(void)
{
	if (sGetSensorValueSym)	{ sGetSensorValueSym->release(); sGetSensorValueSym = NULL; }
	if (fSavedRegisters)	{ IOFree(fSavedRegisters, sizeof(savedRegisters_t)); fSavedRegisters = NULL; }

	super::free();
}

#pragma mark -
#pragma mark *** Platform Functions ***
#pragma mark -

/*******************************************************************************
 * IOHWSensor entry point - callPlatformFunction()
 *******************************************************************************/

IOReturn IOI2CLM8x::callPlatformFunction(
	const OSSymbol	*functionName,
	bool			waitForFunction,
	void			*param1, 
	void			*param2,
	void			*param3, 
	void			*param4)
{
	UInt32 			reg = (UInt32)param1;
	SInt32 			*value = (SInt32 *)param2;
	IOReturn		status;
	int				i;
	UInt8			data;

	if (value == NULL)
		return kIOReturnBadArgument;

//	DLOG("IOI2CLM8x::callPlatformFunction(0x%x) %s %s %08lx %08lx %08lx %08lx\n",
//		getI2CAddress(),
//		functionName->getCStringNoCopy(), waitForFunction ? "TRUE" : "FALSE",
//		(UInt32)param1, (UInt32)param2, (UInt32)param3, (UInt32)param4);

	if (sGetSensorValueSym->isEqualTo(functionName) == TRUE)
	{
		if (isI2COffline())
		{
			ERRLOG("IOI2CLM8x::CPF offline\n");
			return kIOReturnOffline;
		}

		for (i = 0; i < LUNtableElement; i++)
		{
			if (reg == LUNtable[i].SubAddress)
			{
				status = readI2C(reg, &data, 1);
				if (status != kIOReturnSuccess)
				{
					*value = 0;
					ERRLOG("IOI2CLM8x::CPF ERROR readI2C status:0x%x\n", status);
					return status;
				}

				if (LUNtable[i].type == kTypeTemperature)
					*value = ( ( ( ( SInt8 ) data ) << 16 ) * LUNtable[i].ConversionMultiple );

				if (LUNtable[i].type == kTypeADC)
					*value = (SInt32)(((SInt32)data) * LUNtable[i].ConversionMultiple);

				if (LUNtable[i].type == kTypeVoltage)
					*value = (SInt32)(((SInt32)data) * LUNtable[i].ConversionMultiple);

				return status;
			}
		}

		ERRLOG("IOI2CLM8x::CPF ERROR could not find LUN:%d\n", reg);
		return kIOReturnNotFound;
	}

	return super::callPlatformFunction(functionName, waitForFunction, param1, param2, param3, param4);
}

IOReturn IOI2CLM8x::publishChildren(IOService *nub)
{
	OSIterator			*childIterator = NULL;
	IORegistryEntry		*childEntry = NULL;
    IOService			*childNub = NULL;
    IOReturn 			status;

	childIterator = nub->getChildIterator(gIODTPlane);
	if( childIterator != NULL )
	{
		// Iterate through children and create nubs
		while ( ( childEntry = (IORegistryEntry *)( childIterator->getNextObject() ) ) != NULL )
		{
			// Build Table
			status = buildEntryTable(childEntry);
			LUNtableElement++;
			
			// Publish child as IOService
			childNub = OSDynamicCast(IOService, OSMetaClass::allocClassWithName("IOService"));
		
			childNub->init(childEntry, gIODTPlane);
			childNub->attach(this);
			childNub->registerService();
			
//			DLOG("IOI2CLM8x::publishChildren(0x%x) published child %s\n", getI2CAddress(), childEntry->getName());
		}
	
		childIterator->release();
	}

	return kIOReturnSuccess;
}

IOReturn IOI2CLM8x::buildEntryTable(IORegistryEntry *child)
{
	OSData *data;
	
	// Build Table

	// SensorID -- no longer required - eem
/*
	data = OSDynamicCast(OSData, child->getProperty("sensor-id"));
	if(data)
		LUNtable[LUNtableElement].SensorID = *(UInt32 *)data->getBytesNoCopy();
*/
	// SubAddress
	data = OSDynamicCast(OSData, child->getProperty("reg"));
	if(data)
		LUNtable[LUNtableElement].SubAddress = *(UInt32 *)data->getBytesNoCopy();
	else
	{
		ERRLOG("IOI2CLM8x:buildEntryTable - ERROR can't find child reg property\n");
		return kIOReturnError;
	}

	// ConversionMultiple
	if (LUNtable[LUNtableElement].SubAddress == 0x20) // 2.5Vin or External Temperature 2
	{
		data = OSDynamicCast(OSData, child->getProperty("device_type"));

		if (data)
		{
			char *ptr = (char *)data->getBytesNoCopy();

			if(strcmp(ptr, "adc-sensor") == 0) // fix for broken DT
			{
				LUNtable[LUNtableElement].ConversionMultiple = k25VinMultiplier;
				LUNtable[LUNtableElement].type = kTypeADC;
			}
			else if(strcmp(ptr, "voltage-sensor") == 0)
			{
				LUNtable[LUNtableElement].ConversionMultiple = k25VinMultiplier;
				LUNtable[LUNtableElement].type = kTypeVoltage;
			}
			else
			{
				LUNtable[LUNtableElement].ConversionMultiple = 1;				
				LUNtable[LUNtableElement].type = kTypeTemperature;
			}
		}
	}
	else if (LUNtable[LUNtableElement].SubAddress == 0x21) // Vccp1
	{
		LUNtable[LUNtableElement].ConversionMultiple = Vccp1Multiplier;
		LUNtable[LUNtableElement].type = kTypeADC;
	}
	else if (LUNtable[LUNtableElement].SubAddress == 0x22) // Vcc
	{
		LUNtable[LUNtableElement].ConversionMultiple = kVccMultiplier;
		LUNtable[LUNtableElement].type = kTypeADC;
	}
	else if (LUNtable[LUNtableElement].SubAddress == 0x23) // 5V
	{
		LUNtable[LUNtableElement].ConversionMultiple = k5VinMultiplier;
		LUNtable[LUNtableElement].type = kTypeADC;
	}
	else if (LUNtable[LUNtableElement].SubAddress == 0x24) // 12V
	{
		LUNtable[LUNtableElement].ConversionMultiple = k12VinMultiplier;
		LUNtable[LUNtableElement].type = kTypeADC;
	}
	else if (LUNtable[LUNtableElement].SubAddress == 0x25) // Vccp2
	{
		LUNtable[LUNtableElement].ConversionMultiple = Vccp2Multiplier;
		LUNtable[LUNtableElement].type = kTypeADC;
	}
	else if (LUNtable[LUNtableElement].SubAddress == 0x26) // External Temperature 1
	{
		LUNtable[LUNtableElement].ConversionMultiple = 1;
		LUNtable[LUNtableElement].type = kTypeTemperature;
	}
	else if (LUNtable[LUNtableElement].SubAddress == 0x27) // Internal Temperature
	{
		LUNtable[LUNtableElement].ConversionMultiple = 1;
		LUNtable[LUNtableElement].type = kTypeTemperature;
	}
	else if (LUNtable[LUNtableElement].SubAddress == 0x28) // FAN1 or AIN1
	{
		data = OSDynamicCast(OSData, child->getProperty("device_type"));

		if (data)
		{
			char *ptr = (char *)data->getBytesNoCopy();

			if(strcmp(ptr, "adc-sensor") == 0) // fix for broken DT
			{
				LUNtable[LUNtableElement].ConversionMultiple = AIN1Multiplier;
				LUNtable[LUNtableElement].type = kTypeADC;
			}
			else if(strcmp(ptr, "voltage-sensor") == 0)
			{
				LUNtable[LUNtableElement].ConversionMultiple = AIN1Multiplier;
				LUNtable[LUNtableElement].type = kTypeVoltage;
			}
			else
			{
				LUNtable[LUNtableElement].ConversionMultiple = 0; // special case
				LUNtable[LUNtableElement].type = kTypeRPM;
			}
		}
	}
	else if (LUNtable[LUNtableElement].SubAddress == 0x29) // FAN2 or AIN2
	{
		data = OSDynamicCast(OSData, child->getProperty("device_type"));

		if (data)
		{
			char *ptr = (char *)data->getBytesNoCopy();

			if(strcmp(ptr, "adc-sensor") == 0) // fix for broken DT
			{
				LUNtable[LUNtableElement].ConversionMultiple = AIN2Multiplier;
				LUNtable[LUNtableElement].type = kTypeADC;
			}
			else if(strcmp(ptr, "voltage-sensor") == 0)
			{
				LUNtable[LUNtableElement].ConversionMultiple = AIN2Multiplier;
				LUNtable[LUNtableElement].type = kTypeVoltage;
			}
			else
			{
				LUNtable[LUNtableElement].ConversionMultiple = 0; // special case;
				LUNtable[LUNtableElement].type = kTypeRPM;
			}
		}
	}

	return kIOReturnSuccess;
}

#pragma mark -
#pragma mark *** power management ***
#pragma mark -

void IOI2CLM8x::processPowerEvent(UInt32 eventType)
{
	switch (eventType)
	{
		case kI2CPowerEvent_OFF:
		case kI2CPowerEvent_SLEEP:
			if (kIOReturnSuccess != saveRegisters())
				ERRLOG("IOI2CLM8x::processPowerEvent(0x%lx) failed to save registers.\n", getI2CAddress());
			else
				fRegistersAreSaved = true;
			break;

		case kI2CPowerEvent_ON:
		case kI2CPowerEvent_WAKE:
			if (fRegistersAreSaved == true)
			{
				// Full Power State
				if (kIOReturnSuccess != restoreRegisters())
					ERRLOG("IOI2CLM8x::processPowerEvent(0x%lx) failed to restore registers.\n", getI2CAddress());
				fRegistersAreSaved = false;
			}
			break;
	}
}

IOReturn IOI2CLM8x::saveRegisters(void)
{
	IOReturn status;

	DLOG("IOI2CLM8x::saveRegisters(0x%x) entered.\n", getI2CAddress());

	if (kIOReturnSuccess != (status = readI2C(kChannelModeRegister, &fSavedRegisters->ChannelMode, 1)))
		ERRLOG("IOI2CLM8x::saveRegisters(0x%lx) readI2C failed ChannelModeRegister.\n", getI2CAddress());
	else
	if (kIOReturnSuccess != (status = readI2C(kConfReg1, &fSavedRegisters->Configuration1, 1)))
		ERRLOG("IOI2CLM8x::saveRegisters(0x%lx) readI2C failed ConfReg1.\n", getI2CAddress());
	else
	if (kIOReturnSuccess != (status = readI2C(kConfReg2, &fSavedRegisters->Configuration2, 1)))
		ERRLOG("IOI2CLM8x::saveRegisters(0x%lx) readI2C failed ConfReg2.\n", getI2CAddress());

	return status;
}

IOReturn IOI2CLM8x::restoreRegisters(void)
{
	IOReturn status;

	DLOG("IOI2CLM8x::restoreRegisters(0x%x) entered.\n", getI2CAddress());

	if (kIOReturnSuccess != (status = writeI2C(kChannelModeRegister, &fSavedRegisters->ChannelMode, 1)))
		ERRLOG("IOI2CLM8x::restoreRegisters(0x%lx) writeI2C failed ChannelModeRegister.\n", getI2CAddress());
	else
	if (kIOReturnSuccess != (status = writeI2C(kConfReg2, &fSavedRegisters->Configuration2, 1)))
		ERRLOG("IOI2CLM8x::restoreRegisters(0x%lx) writeI2C failed ConfReg2.\n", getI2CAddress());
	else // restore Configuration Register 1 last, since it starts polling
	if (kIOReturnSuccess != (status = writeI2C(kConfReg1, &fSavedRegisters->Configuration1, 1)))
		ERRLOG("IOI2CLM8x::restoreRegisters(0x%lx) writeI2C failed ConfReg1.\n", getI2CAddress());

	return status;
}

IOReturn IOI2CLM8x::initHW(
	IOService	*provider)
{
	IOReturn	status;
	UInt8		cfgReg;
	int			i;

	DLOG("IOI2CLM8x::initHW(0x%x) entered.\n", getI2CAddress());

	cfgReg = 0;

	// Read Configuration Register 1
	for (i = 0; i < 10; i++)
	{
		// Check for RESET bit, this should not happen!
		if (kIOReturnSuccess == (status = readI2C(kConfReg1, &cfgReg, 1)))
		{
			if ((cfgReg & 0x10) != 0x10)
				break;
			else
			{
				status = kIOReturnBusy;
				IOSleep(5); // sleep for 5ms (max while in loop 50ms)
			}
		}
	}

	if (kIOReturnSuccess != status)
	{
		ERRLOG("IOI2CLM8x::initHW(0x%lx) readI2C encountered persistant RESET bit.\n", getI2CAddress());
	}

	// Start monitoring operations
	cfgReg = 0x1;

	// Failure of this write operation is fatal
	if (kIOReturnSuccess != (status = writeI2C(kConfReg1, &cfgReg, 1)))
	{
		ERRLOG("IOI2CLM8x::initHW(0x%lx) writeI2C failed enable monitor bit.\n", getI2CAddress());
	}

	return status;
}

