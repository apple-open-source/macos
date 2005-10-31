/*
 * Copyright (c) 2004-2005 Apple Computer, Inc.  All rights reserved.
 *
 *  File: $Id: IOI2CSMUSat.cpp,v 1.18 2005/09/08 04:44:11 galcher Exp $
 *
 *  DRI: Paul Resch
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

