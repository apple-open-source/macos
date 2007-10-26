/*
 * Copyright (c) 2004-2005 Apple Computer, Inc.  All rights reserved.
 *
 *  File: $Id: IOI2CSMUSat.cpp,v 1.18 2005/09/08 04:44:11 galcher Exp $
 *
 *  DRI: Paul Resch
 *
 *		$Log: IOI2CSMUSat.cpp,v $
 *		Revision 1.18  2005/09/08 04:44:11  galcher
 *		[rdar://4248418] Remove Kodiak Workarounds (AppleHWSensor -
 *		IOI2CSMUSat).
 *		Removed Kodiak-related constants.  Removed code in ::start that restricted
 *		operation of the sensor to Kodiak v1.2 and beyond.
 *		
 *		Revision 1.17  2005/08/10 22:31:11  tsherman
 *		4212063 - Q63: Flashing SATs is painfully slow
 *		
 *		Revision 1.16  2005/08/10 20:43:19  larson
 *		4196134 - sat-version property should only be the version number, not also the MPUID and the CRC.
 *		
 *		Revision 1.15  2005/08/06 01:00:52  tsherman
 *		rdar:4204990 - Q63: Platform Plugin not loaded on boot.
 *		
 *		Revision 1.14  2005/07/28 01:19:45  tsherman
 *		4195836 - Q63: Disable SMU traffic in IOI2CPulsar and IOI2CSMUSat for non 1.2 Kodiak systems
 *		
 *		Revision 1.13  2005/07/26 20:19:04  jorr
 *		When the SAT base partition is erased now both 8k blocks at 0xFC000 and
 *		0xFE000 are erased. Also debug timing info added for flashing SATs.
 *		
 *		Revision 1.12  2005/06/03 00:24:17  tsherman
 *		Changed sensor cache update code to use locks instead of a IOCommandGate. This
 *		new implementation guarantees the control loop will not get blocked waiting
 *		for i2c reads (which can be up to 7/10th of second waiting on the SAT).
 *		
 *		Revision 1.11  2005/06/01 23:15:57  tsherman
 *		Marco implemented IOI2CSMUSat "lock-sensor" mechanism
 *		
 *		Revision 1.10  2005/05/24 01:01:50  mpontil
 *		Cached data was invalidated much to frequently due to a time scale error.
 *		(.001 second as opposed to 1 second.)
 *		
 *		Revision 1.9  2005/05/19 22:15:33  tsherman
 *		4095546 - SMUSAT sensors - Update driver to read all SAT sensors with one read (raddog)
 *		
 *		Revision 1.8  2005/05/02 21:45:40  raddog
 *		Do the Cam approved sign extension of 16-bit sensor data so we can handle negative sensor readings
 *		
 *		Revision 1.7  2005/04/28 20:20:41  raddog
 *		Initial SAT flash support.  Redo Josephs getProperty code and eliminate getPartition call platform function (use getProperty instead)
 *		
 *		Revision 1.6  2005/04/27 17:41:48  mpontil
 *		Checking in changes from Joseph to supprt current sensors. Also this checkin
 *		does a little of cleaning up and adds a getProperty() to read from SDB in
 *		the same way it is already done for the SMU.
 *		
 *		These changes are tagged before and after with BeforeMarco050427 and
 *		AfterMarco050427.
 *		
 *		Revision 1.5  2005/04/11 23:40:17  dirty
 *		Fix compiler warning.
 *		
 *		Revision 1.4  2005/02/01 03:25:16  larson
 *		Do not publish sensors named adc since on SAT systems they behave as indirect
 *		sensors and do not play well with the code that speaks like direct sensors.
 *		
 *		Revision 1.3  2005/01/13 01:46:48  dirty
 *		Add cvs header.  Fix build.
 *		
 *		
 */

#include "IOI2CSMUSat.h"

#define super IOI2CDevice
OSDefineMetaClassAndStructors(IOI2CSMUSat, IOI2CDevice)

// This global count is used to disabled SAT sensor reads in order to speed up SDB partion reads and SAT flashing.
// This needs to be a count, over a bool, since there are multiple instances of the SAT driver.
int gSATSensorReadDisableCount = 0;

void IOI2CSMUSat::free( void )
{
	if (fGetSensorValueSym) { fGetSensorValueSym->release(); fGetSensorValueSym = NULL; }
	if (fSymDownloadCommand) { fSymDownloadCommand->release(); fSymDownloadCommand = NULL; }
	if (fSymGetDBPartition) { fSymGetDBPartition->release(); fSymGetDBPartition = NULL; }
	if (fTimerSource) { fTimerSource->cancelTimeout(); fTimerSource->release(); fTimerSource = NULL; }
	if (fCacheLock) { IOLockFree( fCacheLock ); fCacheLock = NULL; }

	super::free();
}

bool IOI2CSMUSat::start( IOService *nub )
{
	OSData			*regProp;
	
	DLOG("IOI2CSMUSat::start - entered\n");

	if (!super::start(nub)) return(false);

	if (!fGetSensorValueSym)
		fGetSensorValueSym = OSSymbol::withCString("getSensorValue");

	if (!fSymDownloadCommand)
		fSymDownloadCommand = OSSymbol::withCString("downloadCommand");
		
	if ((regProp = OSDynamicCast(OSData, nub->getProperty("reg"))) != NULL)
		setProperty ("reg", regProp);		// Copy it to our node so flasher can find us

	// tell the world i'm here
	registerService();

	char key[256];
	sprintf(key, "IOSMUSAT-%02x", getI2CAddress());
	publishResource(key, this);

	// Publish any child nubs under the smu-sat node...
	publishChildren(nub);
	
	// Allocated lock for sensor cache updates.
	fCacheLock = IOLockAlloc();
	if ( fCacheLock == NULL )
	{
        IOLog("IOI2CSMUSat::start couldn't allocate IOLock!\n");
		return false;
	}

	// Initialize newly allocated lock to kIOLockStateUnlocked state.
	IOLockInit( fCacheLock );

    // Get workloop (IOService) for use with timer later on.
    if ( !(fWorkLoop = getWorkLoop()) )
    {
        IOLog("IOI2CSMUSat::start couldn't get workLoop!\n");
		return false;
    }

	// Create a timer for periodic call backs to update sensor cache.
	fTimerSource = IOTimerEventSource::timerEventSource((OSObject *)this, timerEventOccurred);
	if ( fTimerSource == NULL ) 
	{
		IOLog("IOI2CSMUSat::start failed to create timer event source.\n");
		return false;
	}

	// Add timer to workLoop.
	if ( fWorkLoop->addEventSource(fTimerSource) != kIOReturnSuccess )
	{
		IOLog("IOI2CSMUSat::start failed to add timer event source to workloop.\n");
		fTimerSource->release();
		return false;
	}

	// Set initial timeout for timer (one-shot).
	fTimerSource->setTimeoutMS(1000);  // set timeout to be one second
	
	return(true);
}

void IOI2CSMUSat::stop( IOService *nub )
{
	DLOG("IOI2CSMUSat::stop - entered\n");

	// Execute any functions flagged as "on termination"
	performFunctionsWithFlags (kIOPFFlagOnTerm);

	super::stop( nub );
}

#pragma mark  
#pragma mark *** Call Platform Function ***
#pragma mark  

/*******************************************************************************
 * IOHWSensor entry point - callPlatformFunction()
 *******************************************************************************/

IOReturn IOI2CSMUSat::callPlatformFunction(const OSSymbol *functionName,
				bool waitForFunction, void *param1, void *param2,
				void *param3, void *param4)
{

//	DLOG("IOI2CSMUSat::callPlatformFunction %s %s %08lx %08lx %08lx %08lx\n",
//			functionName->getCStringNoCopy(), waitForFunction ? "TRUE" : "FALSE",
//			(UInt32) param1, (UInt32) param2, (UInt32) param3, (UInt32) param4);

	if (functionName->isEqualTo(fGetSensorValueSym) == true)
	{
		UInt32 satReg = (UInt32)param1;
		SInt32 *reading_buf = (SInt32 *)param2;
		
		if (isI2COffline() == true)
			return(kIOReturnOffline);

		if (reading_buf == NULL)
			return(kIOReturnBadArgument);

		return(getSensor(satReg, reading_buf));
	}

	return(super::callPlatformFunction(functionName, waitForFunction,
				param1, param2, param3, param4));
}

/* act as though this method was never implemented */
IOReturn IOI2CSMUSat::callPlatformFunction( const char *functionName,
                                       bool waitForFunction,
                                       void *param1, void *param2,
                                       void *param3, void *param4 )
{
	return(super::callPlatformFunction(functionName,
			waitForFunction, param1, param2, param3, param4));
}

/*******************************************************************************
 * Publish the children nubs for the SMU Satellite nub - publishChildren()
 *******************************************************************************/

IOReturn IOI2CSMUSat::publishChildren(IOService *nub)
{
    OSIterator			*childIterator = NULL;
    IORegistryEntry		*childEntry = NULL;
    IOService			*childNub = NULL;
	OSData				*data;
	OSData				*reg;
	UInt32				type;

	LUNtableElement = 0;
	
	childIterator = nub->getChildIterator(gIODTPlane);
	if( childIterator != NULL )
	{
		// Iterate through children and create nubs
		while ( ( LUNtableElement < kLUN_TABLE_COUNT ) && ( ( childEntry = (IORegistryEntry *)( childIterator->getNextObject() ) ) != NULL ) )
		{
			DLOG("IOI2CSMUSat(%x)::publishChildren unknown device_type in node '%s'\n", getI2CAddress(), childEntry->getName());
		
			// Prevent IOHWSensor drivers for adc channels. (They're for OF diags only)
			if(strcmp(childEntry->getName(), "adc") == 0)
			{
				continue;
			}

			// Publish child as IOService
			childNub = OSDynamicCast(IOService, OSMetaClass::allocClassWithName("IOService"));

			if( childNub )
			{
				childNub->init(childEntry, gIODTPlane);
				childNub->attach(this);
				reg = OSDynamicCast(OSData, childNub->getProperty("reg"));
				data = OSDynamicCast(OSData, childNub->getProperty("device_type"));
				
				DLOG("IOI2CSMUSat(%x)::publishChildren %x unknown device_type in node '%s'\n", getI2CAddress(), reg, childEntry->getName());
				
				if (data && reg)
				{
					char *ptr = (char *)data->getBytesNoCopy();
		
					if(strcmp(ptr, "adc-sensor") == 0)
						type = kTypeADC;
					else if(strcmp(ptr, "temp-sensor") == 0)
						type = kTypeTemperature;
					else if(strcmp(ptr, "voltage-sensor") == 0)
						type = kTypeVoltage;
					else if(strcmp(ptr, "current-sensor") == 0)
						type = kTypeCurrent;
					else
					{
						type = kTypeUnknown;
						DLOG("IOI2CSMUSat(%x)::publishChildren unknown device_type in node '%s'\n", getI2CAddress(), childEntry->getName());	
					}

					if ( type != kTypeUnknown )
					{
						DLOG("IOI2CSMUSat(%x)::publishChildren adding node '%s' reg:0x%lx to LUN[%d]\n", getI2CAddress(), childEntry->getName(), *(UInt32 *)reg->getBytesNoCopy(), (int)LUNtableElement);
						LUNtable[LUNtableElement].reg = *(UInt32 *)reg->getBytesNoCopy();
						LUNtable[LUNtableElement].type = type;
						LUNtableElement++;
					}
				}
				else
					DLOG("IOI2CSMUSat(%x)::publishChildren missing property in node '%s'\n", getI2CAddress(), childEntry->getName());	
				childNub->registerService();
				DLOG("IOI2CSMUSat(%x)::publishChildren published child '%s'\n", getI2CAddress(), childEntry->getName());
			}
		}
	
		childIterator->release();
	}

	return kIOReturnSuccess;
}

#pragma mark -
#pragma mark *** Read Sensor Channels ***
#pragma mark -

void IOI2CSMUSat::timerEventOccurred(OSObject *object, IOTimerEventSource *timer)
{
    IOI2CSMUSat *me;

	if( gSATSensorReadDisableCount == 0 )
	{
		if (me = OSDynamicCast(IOI2CSMUSat, object))
			me->updateSensorCache();
	}
	else
	{
		timer->setTimeoutMS(1000);  // set timeout to be one second
	}
}

IOReturn IOI2CSMUSat::updateSensorCache ( void )
{
	IOReturn	status;
	UInt16		tmpCachedSensorData[8];

    fTimerSource->setTimeoutMS(1000);  // set timeout to be one second

	// Store current sensor values into local buffer
	status = readI2C( 0x3F, (UInt8 *)tmpCachedSensorData, sizeof(tmpCachedSensorData) );
	if ( status == kIOReturnSuccess )
	{
		// This lock is also used in the setProperty method, please see comments there. 
		// If we take the lock, nobody currently cares if we update the cache so memcpy
		// local sensor cache into global cache.
		IOLockLock( fCacheLock );
		memcpy( fCachedSensorData, tmpCachedSensorData, sizeof(fCachedSensorData) );
		IOLockUnlock( fCacheLock );
		
#ifdef SMUSAT_DEBUG
		for (int i = 0; i < 8; i++) {
			UInt16 hi, lo, raw16;
			SInt32 raw;
			
			raw16 = fCachedSensorData[i];
			switch (i) {
				case 0:
				case 1:
					raw = ((SInt32)raw16 << 4);	// convert 4.12 to 16.16
					lo = ((raw>>13)&7)*125;
					hi = (raw >> 16) & 0xFFFF;
					kprintf ("SAT(%x:%x)Target VDD%d: %d.%d (raw16 0x%x, raw32 0x%x)\n", getI2CAddress(), i + kSATCacheRegBase, i, hi, lo, raw16, raw);
					break;
				case 2:
				case 3:
					raw = ((SInt32)raw16 << 4);		// convert 4.12 to 16.16
					lo = ((raw>>13)&7)*125;
					hi = (raw >> 16) & 0xFFFF;
					kprintf ("SAT(%x:%x)Actual VDD%d: %d.%d (raw16 0x%x, raw32 0x%x)\n", getI2CAddress(), i + kSATCacheRegBase, i - 2, hi, lo, raw16, raw);
					break;
				case 4:
				case 5:
					raw = ((SInt32)raw16 << 10);	// convert 10.6 to 16.16
					lo = ((raw>>13)&7)*125;
					hi = (raw >> 16) & 0xFFFF;
					kprintf ("SAT(%x:%x)Core%d Temp: %d.%d (raw16 0x%x, raw32 0x%x)\n", getI2CAddress(), i + kSATCacheRegBase, i - 4, hi, lo, raw16, raw);
					break;
				case 6:
				case 7:
					raw = ((SInt32)raw16 << 8);		// convert 8.8 to 16.16
					lo = ((raw>>13)&7)*125;
					hi = (raw >> 16) & 0xFFFF;
					kprintf ("SAT(%x:%x)Core%d Current: %d.%d (raw16 0x%x, raw32 0x%x)\n", getI2CAddress(), i + kSATCacheRegBase, i - 6, hi, lo, raw16, raw);
					break;
			}
		}
#endif
	} 
	else
	{
		kprintf ("cache read failed 0x%x\n",  status);
	}
	
	return status;
}

/*******************************************************************************
 * Read raw device data, using cached data if possible
 *
 *  Should only be used for registers 0x30 through 0x38
 *******************************************************************************/
IOReturn IOI2CSMUSat::readI2CCached (UInt32 subAddress, UInt8 *data, UInt32 count)
{
	UInt16			*data16, data16cached;
	IOReturn		status = kIOReturnError;
	
	if ((count == 2) && (subAddress >= kSATCacheRegBase) && (subAddress < kSatRegCacheTop))
	{
		data16 = (UInt16 *) data;
		*data16 = fCachedSensorData[subAddress - kSATCacheRegBase];
		data16cached = *data16;
		status = kIOReturnSuccess;
	}
	
	if (status != kIOReturnSuccess)
	{
		// If we got here, either we didn't get cached data or it wasn't a valid cache read request
		// So just do a normal read
		status = readI2C( subAddress, data, count );
		data16 = (UInt16 *) data;
		kprintf ("SAT(%x:%x)direct value 0x%x, cached value 0x%x\n", getI2CAddress(), subAddress, *data16, data16cached);
	}
	
	return status;
}

/*******************************************************************************
 * Read sensor channels from the device
 *******************************************************************************/

IOReturn IOI2CSMUSat::getSensor( UInt32 satReg, SInt32 * reading )
{
	IOReturn	status;
	UInt16		int16;
	int			n;

	*reading = -1;

	// get the reading associated with satReg register
	for( n=0; n<LUNtableElement; n++ ) {
		if( LUNtable[n].reg == satReg )
			break;
	}

	// format as 16.16 fixed point and return it
	if( n < LUNtableElement )
	{
		switch (LUNtable[n].type) {
			case kTypeVoltage:
				if (kIOReturnSuccess != (status = readI2CCached( satReg, (UInt8 *)&int16, 2 )))
					DLOG("IOI2CSMUSat(%x)::getSensor(0x%x) readI2C VOLT status=0x%x\n", getI2CAddress(), (unsigned int)satReg, (int)status);
				else
				{
					*reading = ((SInt32)int16 << 4);	// convert 4.12 to 16.16
					DLOG("IOI2CSMUSat(%x)::getSensor(0x%x) raw=0x%x VOLT reading=%d.%03d\n", getI2CAddress(), (unsigned int)satReg, (int)int16, (int)(*reading) >> 16, (int)(((*reading)>>13)&7)*125 );
				}
				break;
			case kTypeTemperature:
				if (kIOReturnSuccess != (status = readI2CCached( satReg, (UInt8 *)&int16, 2 )))
					DLOG("IOI2CSMUSat(%x)::getSensor(0x%x) readI2C TEMP status=0x%x\n", getI2CAddress(), (unsigned int)satReg, (int)status);
				else
				{
					*reading = ((SInt32)int16 << 10);	// convert 10.6 to 16.16
					DLOG("IOI2CSMUSat(%x)::getSensor(0x%x) raw=0x%x TEMP reading=%d.%03d\n", getI2CAddress(), (unsigned int)satReg, (int)int16, (int)(*reading) >> 16, (int)(((*reading)>>13)&7)*125 );
				}
				break;
			case kTypeCurrent:
				if (kIOReturnSuccess != (status = readI2CCached( satReg, (UInt8 *)&int16, 2 )))
					DLOG("IOI2CSMUSat(%x)::getSensor(0x%x) readI2C CURRENT status=0x%x\n", getI2CAddress(), (unsigned int)satReg, (int)status);
				else
				{
					*reading = ((SInt32)int16 << 8);	// convert 8.8 to 16.16
					DLOG("IOI2CSMUSat(%x)::getSensor(0x%x) raw=0x%x CURRENT reading=%d.%03d\n", getI2CAddress(), (unsigned int)satReg, (int)int16, (int)(*reading) >> 16, (int)(((*reading)>>13)&7)*125 );
				}
				break;
			case kTypeADC:
			default:
				DLOG("IOI2CSMUSat(%x)::getSensor(0x%x) unsupported device!\n", getI2CAddress(), (unsigned int)satReg);
				status = kIOReturnNotFound;
				break;
		};
	}
	else
	{
		DLOG("IOI2CSMUSat(%x)::getSensor(0x%x) unknown device!\n", getI2CAddress(), (unsigned int)satReg);
		status = kIOReturnNotFound;
	}

	return status;
}

