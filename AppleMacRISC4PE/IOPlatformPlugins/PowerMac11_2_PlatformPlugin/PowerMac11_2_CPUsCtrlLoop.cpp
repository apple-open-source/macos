/*
 * Copyright (c) 2002-2005 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2002-2005 Apple Computer, Inc.  All rights reserved.
 *
 *
 */


// IMPORTANT: All its marked as WORKAROUND needs to be removed once I get updated HW

//#define CONTROL_LOOP_DEBUG 1
//#define CONTROL_LOOP_RUNTIMELOG 1

#ifdef CONTROL_LOOP_DEBUG
#define PM11_2_DLOG(fmt, args...)  kprintf(fmt, ## args)
#else
#define PM11_2_DLOG(fmt, args...)
#endif

#if defined (CONTROL_LOOP_DEBUG) || defined (CONTROL_LOOP_RUNTIMELOG) 
#define PM11_2_RUNTIMELOG(fmt, args...)  kprintf(fmt, ## args)
#else
#define PM11_2_RUNTIMELOG(fmt, args...)
#endif

#include "IOPlatformPluginDefs.h"
#include "IOPlatformPluginSymbols.h"

#include "PowerMac11_2_CPUsCtrlLoop.h"
#include "PowerMac11_2_ThermalProfile.h"

#include "IOGroupVirtualControl.h"

/*
	Pmax = 95W
	Tmax = 85¡
	Poffset = -3.6W (note the negative sign)
	Tmax_offset = 2¡
	Gr = 0.55¡/W (note that SMU systems treat this field differently than Q77)
	Gp = 10
	Gd = 150
	Power_Integral_History = 6
	Interval should be 1s, assuming sensors can be read fast enough.
*/

// Note that the coefficients (Gp, Gr, Gd) are placeholders and are most-likely wrong for Q63.
UInt8 gCore0AlternateDataOnlyForTesting[ 24 ] = {
0xC8, 0x06, 0x02, 0x7F, 0xFF, 0x02, 0xFF, 0x06, 0xFC, 0x66, 0x00, 0x5f,
0x00, 0xA0, 0x00, 0x00, 0x00, 0x08, 0xCC, 0xCD, 0x09, 0x60, 0x00, 0x00
};

UInt8 gCore1AlternateDataOnlyForTesting[ 24 ] = {
0xC9, 0x06, 0x02, 0x7F, 0xFF, 0x02, 0xFF, 0x06, 0xFC, 0x66, 0x00, 0x5f,
0x00, 0xA0, 0x00, 0x00, 0x00, 0x08, 0xCC, 0xCD, 0x09, 0x60, 0x00, 0x00
};

// Just initialize this "global" to null:
OSDictionary *PowerMac11_2_CPUsCtrlLoop::gIOGroupVirtualControlDictionary = NULL;

#undef super
#define super IOPlatformPIDCtrlLoop
OSDefineMetaClassAndStructors( PowerMac11_2_CPUsCtrlLoop, IOPlatformPIDCtrlLoop )

SensorValue PowerMac11_2_CPUsCtrlLoop::getSensorValue( IOPlatformSensor *sensor)
{
	SensorValue currentValue;

	if ( sensor != NULL )
	{
		currentValue = sensor->forceAndFetchCurrentValue();
		//currentValue = sensor->fetchCurrentValue();
		sensor->setCurrentValue( currentValue );
		
		PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop %s %ld %d %d\n", sensor->getSensorDescKey()->getCStringNoCopy(), currentValue.sensValue, currentValue.thermValue.intValue, currentValue.thermValue.fracValue);
	}
	else
	{
		currentValue.sensValue = 0;
	}

	return currentValue;
}

bool PowerMac11_2_CPUsCtrlLoop::allControlsAndSensorsRegistredCheck( )
{
	if ( mGroupControl )
	{
		bool allSensorsRegistred = true;
		int i;
		
		for ( i = 0; ( i <  mActualNumberOfInputSets ) && allSensorsRegistred ; i ++ )
		{
			allSensorsRegistred &= (
				( mInputSetArray[i].powerSensor->isRegistered() == kOSBooleanTrue ) &&
				( mInputSetArray[i].temperatureSensor->isRegistered() == kOSBooleanTrue ) );
				
				
			if ( allSensorsRegistred )
			{
				IOService *sensorDriver =  mInputSetArray[ i ].temperatureSensor->getSensorDriver();
				
				if ( sensorDriver )
				{
					IOService *temperatureSensorDevice = sensorDriver->getProvider();
					if ( temperatureSensorDevice )
					{
						mInputSetArray[ i ].referenceSAT = temperatureSensorDevice->getProvider();
						if ( mInputSetArray[ i ].referenceSAT == NULL )
						{
							PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::AllControlsAndSensorsRegistredCheck SAT for %s:%d \t %s:%d\n",
								mInputSetArray[i].temperatureSensor->getSensorDescKey()->getCStringNoCopy(), mInputSetArray[i].temperatureSensor->isRegistered() == kOSBooleanTrue,
								mInputSetArray[i].powerSensor->getSensorDescKey()->getCStringNoCopy(), mInputSetArray[i].powerSensor->isRegistered() == kOSBooleanTrue);
						}
						else
						{
							kprintf("PowerMac11_2_CPUsCtrlLoop::allControlsAndSensorsRegistredCheck set %d -> %s\n", i, mInputSetArray[ i ].referenceSAT->getMetaClass()->getClassName());
							mInputSetArray[ i ].referenceSAT->retain();
						}
					}
				}
			}
				
			PM11_2_RUNTIMELOG("PowerMac11_2_CPUsCtrlLoop::AllControlsAndSensorsRegistredCheck %s:%d \t %s:%d\n",
				mInputSetArray[i].temperatureSensor->getSensorDescKey()->getCStringNoCopy(), mInputSetArray[i].temperatureSensor->isRegistered() == kOSBooleanTrue,
				mInputSetArray[i].powerSensor->getSensorDescKey()->getCStringNoCopy(), mInputSetArray[i].powerSensor->isRegistered() == kOSBooleanTrue);
		}

		mAllSensorsRegistred = allSensorsRegistred;
		mAllControlsRegistred = ( mGroupControl->isGroupRegistered() == kOSBooleanTrue );

		PM11_2_RUNTIMELOG("PowerMac11_2_CPUsCtrlLoop::AllControlsAndSensorsRegistredCheck for %d reads  %d, %d, %d!\n",
			( getCtrlLoopID() ? getCtrlLoopID()->unsigned16BitValue() : 0xFF ),
			mAllControlsRegistred, mAllSensorsRegistred, ctrlloopState);

		if ( ( mAllControlsRegistred ) && ( mAllSensorsRegistred ) )
		{
			PM11_2_RUNTIMELOG("PowerMac11_2_CPUsCtrlLoop::AllControlsAndSensorsRegistredCheck allRegistered!\n");

			ctrlloopState = kIOPCtrlLoopFirstAdjustment;

			// set the deadline
			deadlinePassed();
		}
		
		return ( ( mAllControlsRegistred ) && ( mAllSensorsRegistred ) );
	}
	
	return false;
}

// This is very machine-dependent, should you need to inherit this control loop
// you will likely need to complery rev this method:

OSData *PowerMac11_2_CPUsCtrlLoop::getRawDatasetFor( UInt8 cpuid, UInt8 coreid )
{
	OSData	*sdbPartitionData = NULL;
	OSData	*pidDataSet = NULL;
	OSData	*fvtDataSet = NULL;
	IOService *sat = NULL;
	IOService *ioI2CSMUSat = NULL;
	
	PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::getRawDatasetFor( %d, %d )\n", cpuid, coreid);
	
	// If there is no reference SAT look for it (again)
	// Choose the corrcet sat for the given CPU:
	switch ( cpuid )
	{
		case 0:
			sat = (IOService*) IOService::fromPath ("/smu/@0/@b/@b0", gIODTPlane);
		break;
		
		case 1:
			sat = (IOService*) IOService::fromPath ("/smu/@0/@b/@b2", gIODTPlane);
		break;
		
		default:
			kprintf("PowerMac11_2_CPUsCtrlLoop::getRawDatasetFor( %d ) cpuid can be only 0 or 1\n", cpuid);
			return NULL;
		break;
	}

	if ( sat == NULL )
	{
		kprintf("PowerMac11_2_CPUsCtrlLoop::getRawDatasetFor( %d ) sat can not be found\n", cpuid);
		return NULL;
	}

	// gets its child:
	ioI2CSMUSat = OSDynamicCast( IOService, sat->getChildEntry( gIOServicePlane ) ); 
	if ( ioI2CSMUSat == NULL )
	{
		kprintf("PowerMac11_2_CPUsCtrlLoop::getRawDatasetFor( %d ) sat does not have a matching driver\n", cpuid );
		return NULL;
	}
	else
	{
		kprintf("PowerMac11_2_CPUsCtrlLoop::getRawDatasetFor( %d ) %s\n", cpuid, ioI2CSMUSat->getMetaClass()->getClassName());
	}

	
	// and then gets the correct partition data:
	switch ( coreid )
	{
		case 0:
			PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::getRawDatasetFor sdb-partition-C8\n");
			pidDataSet = OSDynamicCast( OSData, ioI2CSMUSat->getProperty( "sdb-partition-C8" ) );
			if ( pidDataSet == NULL)
			{
				pidDataSet = OSData::withBytes( gCore0AlternateDataOnlyForTesting, sizeof( gCore0AlternateDataOnlyForTesting ) );
				kprintf("PowerMac11_2_CPUsCtrlLoop::getRawDatasetFor( %d, %d ) missing sdb-partition-C8 fall back on predefined data.\n", cpuid, coreid);
			}
		break;
		
		case 1:
			PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::getRawDatasetFor sdb-partition-C8\n");
			pidDataSet = OSDynamicCast( OSData, ioI2CSMUSat->getProperty( "sdb-partition-C9" ) );
			if ( pidDataSet == NULL)
			{
				pidDataSet = OSData::withBytes( gCore1AlternateDataOnlyForTesting, sizeof( gCore1AlternateDataOnlyForTesting ) );
				kprintf("PowerMac11_2_CPUsCtrlLoop::getRawDatasetFor( %d, %d ) missing sdb-partition-C9 fall back on predefined data.\n", cpuid, coreid);
			}
		break;
		
		default:
			PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::getRawDatasetFor( %d, %d ) core id can be only 0 or 1\n", cpuid, coreid);
			return NULL;
		break;
	}
	

	// and then gets the correct partition data:
	switch ( coreid )
	{
		case 0:
			PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::getRawDatasetFor sdb-partition-C4\n");
			fvtDataSet = OSDynamicCast( OSData, ioI2CSMUSat->getProperty( "sdb-partition-C4" ) );
			if ( fvtDataSet == NULL)
			{
				kprintf("PowerMac11_2_CPUsCtrlLoop::getRawDatasetFor( %d, %d ) missing sdb-partition-C4 fails.\n", cpuid, coreid);
				return NULL;
			}
		break;
		
		case 1:
			PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::getRawDatasetFor sdb-partition-C5\n");
			fvtDataSet = OSDynamicCast( OSData, ioI2CSMUSat->getProperty( "sdb-partition-C5" ) );
			if ( fvtDataSet == NULL)
			{
				kprintf("PowerMac11_2_CPUsCtrlLoop::getRawDatasetFor( %d, %d ) missing sdb-partition-C5 fails.\n", cpuid, coreid);
				return NULL;
			}
		break;
		
		default:
			PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::getRawDatasetFor( %d, %d ) core id can be only 0 or 1\n", cpuid, coreid);
			return NULL;
		break;
	}

	sdbPartitionData = pidDataSet;
	sdbPartitionData->appendBytes(fvtDataSet);
	
	return sdbPartitionData;
}

OSData *PowerMac11_2_CPUsCtrlLoop::getSubData( OSData * source, UInt32 offset, UInt32 length)
{
	const void* dataPointer;

	if ( ( source == NULL ) || ( source->getLength() - offset < length ) )
	{
		PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::getSubData(%p, %ld, %ld ) (%d) invalid parameters\n",
			source, offset, length, source->getLength() );

		return( NULL );
	}

	if ( ( dataPointer = source->getBytesNoCopy( offset, length ) ) == NULL )
	{
		PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::getSubData(%p, %ld, %ld ) source can not be read\n",
			source, offset, length);

		return( NULL );
	}	
	
	return OSData::withBytes( dataPointer, length );
}

OSDictionary* PowerMac11_2_CPUsCtrlLoop::getPIDDataset( const OSDictionary* dict, UInt8 cpuID, UInt8 coreID )
{
	UInt8			*buffer;
	const OSData	*tmpData;
	const OSNumber	*tmpNumber;
	OSDictionary	*dataset;
	OSData			*sdbPartitionData;
	
	PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::getPIDDataset\n");
	
	if ( ( dataset = OSDictionary::withCapacity( 14 ) ) == NULL )
	{
		PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::getPIDDataset: Cannot allocate PID dataset.\n" );

		return( NULL );
	}

	if ( ( sdbPartitionData = getRawDatasetFor( cpuID, coreID ) ) == NULL )
	{
		PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::getPIDDataset: Failed to fetch the sdbPartitionData.\n" );
		
		dataset->release();
		return( NULL );
	}

	// Proportional gain (G_p) (12.20).
	if ( ( tmpData = getSubData( sdbPartitionData, 12, 4 ) ) == NULL )
	{
		PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::getPIDDataset: Failed to fetch G_p.\n" );
		
		dataset->release();
		return( NULL );
	}
	dataset->setObject( kIOPPIDCtrlLoopProportionalGainKey, tmpData );
	tmpData->release();

	// Reset gain (G_r) (12.20) -- Actually used as power integral gain.
	if ( ( tmpData = getSubData( sdbPartitionData, 16, 4 ) ) == NULL )
	{
		PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::getPIDDataset: Failed to fetch G_r.\n" );
		
		dataset->release();
		return( NULL );
	}
	dataset->setObject( kIOPPIDCtrlLoopResetGainKey, tmpData );
	tmpData->release();

	// Derivative gain (G_d) (12.20).
	if ( ( tmpData = getSubData( sdbPartitionData, 20, 4 ) ) == NULL )
	{
		PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::getPIDDataset: Failed to fetch G_d.\n" );
		
		dataset->release();
		return( NULL );
	}
	dataset->setObject( kIOPPIDCtrlLoopDerivativeGainKey, tmpData );
	tmpData->release();

	// History length (8-bit integer).
	if ( ( buffer = (UInt8*)sdbPartitionData->getBytesNoCopy( 7, 1 ) ) == NULL )
	{
		PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::getPIDDataset: Failed to fetch history length.\n" );

		dataset->release();
		return( NULL );
	}

	tmpNumber = OSNumber::withNumber( buffer[ 0 ], 8 );
	dataset->setObject( kIOPPIDCtrlLoopHistoryLenKey, tmpNumber );
	tmpNumber->release();
	
	// Target temperature delta (16.16).  Comes from partition 0x17.
	if ( ( buffer = (UInt8*)sdbPartitionData->getBytesNoCopy( 5, 1 ) ) == NULL )
	{
		PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::getPIDDataset: Failed to fetch Tmax delta..\n" );

		dataset->release();
		return( NULL );
	}

	tmpNumber = OSNumber::withNumber( buffer[ 0 ] << 16, 32 );
	dataset->setObject( kIOPPIDCtrlLoopTargetTempDelta, tmpNumber );
	tmpNumber->release();

	// Max power (16.16).
	if ( ( buffer = (UInt8*)sdbPartitionData->getBytesNoCopy( 10, 2 ) ) == NULL )
	{
		PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::getPIDDataset: Failed to fetch max power.\n" );

		dataset->release();
		return( NULL );
	}
	tmpNumber = OSNumber::withNumber( ( (UInt32)OSReadBigInt16( buffer, 0 ) ) << 16, 32 );
	dataset->setObject( kIOPPIDCtrlLoopMaxPowerKey, tmpNumber );
	tmpNumber->release();

	// Power adjustment (16.16).
	if ( ( buffer = (UInt8*)sdbPartitionData->getBytesNoCopy( 8, 2 ) ) == NULL )
	{
		PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::getPIDDataset: Failed to fetch max power adjustment.\n" );

		dataset->release();
		return( NULL );
	}
	UInt16 eight_plus_eight_fixed = OSReadBigInt16( buffer, 0 );
	bool theNumberIsNegative = ( eight_plus_eight_fixed & 0x8000 );
	UInt32	newFixedValueForm = ( theNumberIsNegative ? ( 0xFFFF0000 | eight_plus_eight_fixed ) : eight_plus_eight_fixed ) << 8 ;	
	tmpNumber = OSNumber::withNumber( newFixedValueForm, 32 );
	dataset->setObject( kIOPPIDCtrlLoopMaxPowerAdjustmentKey, tmpNumber );
	tmpNumber->release();

	// The iteration interval is not stored in the SDB.  Set it to 1 second.
	dataset->setObject( kIOPPIDCtrlLoopIntervalKey, gIOPPluginOne );
	
	// if the data is expteded wih the FTV get the max temp out of it:
	if ( sdbPartitionData->getLength() > 24)
	{
		// Max Temperature (24+9).
		if ( ( buffer = (UInt8*)sdbPartitionData->getBytesNoCopy( 33, 1 ) ) == NULL )
		{
			PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::getPIDDataset: Failed to fetch max temp.\n" );

			dataset->release();
			return( NULL );
		}

		tmpNumber = OSNumber::withNumber( buffer[ 0 ] << 16, 32 );
		dataset->setObject( kIOPPIDCtrlLoopInputMaxKey, tmpNumber );
		tmpNumber->release();
	}

	// Extra dataset for overtemp:
	tmpNumber = OSNumber::withNumber( kPM11_2CPU_DEFAULT_slewAverageOffset << 16, 32 );
	dataset->setObject( kOverTempSlewAverageOffset, tmpNumber );
	tmpNumber->release();

	tmpNumber = OSNumber::withNumber( kPM11_2CPU_DEFAULT_slewOffset << 16, 32 );
	dataset->setObject( kOverTempSlewImmediateOffset, tmpNumber );
	tmpNumber->release();

	tmpNumber = OSNumber::withNumber( kPM11_2CPU_DEFAULT_endOfSlewOffset << 16, 32 );
	dataset->setObject( kEndOfSlewOffset, tmpNumber );
	tmpNumber->release();

	tmpNumber = OSNumber::withNumber( kPM11_2CPU_DEFAULT_slewAverageNumberOfSamples, 32 );
	dataset->setObject( kOverTempSlewAverageNumberOfSamples, tmpNumber );
	tmpNumber->release();

	// Extra dataset for sleep
	tmpNumber = OSNumber::withNumber( kPM11_2CPU_DEFAULT_sleepAverageOffset << 16, 32 );
	dataset->setObject( kOverTempSleepAverageOffset, tmpNumber );
	tmpNumber->release();

	tmpNumber = OSNumber::withNumber( kPM11_2CPU_DEFAULT_sleepOffset << 16, 32 );
	dataset->setObject( kOverTempSleepImmediateOffset, tmpNumber );
	tmpNumber->release();

	tmpNumber = OSNumber::withNumber( kPM11_2CPU_DEFAULT_sleepAverageNumberOfSamples, 32 );
	dataset->setObject( kOverTempSleepAverageNumberOfSamples, tmpNumber );
	tmpNumber->release();

	tmpNumber = OSNumber::withNumber( kPM11_2CPU_GlobalContorlDecreaseFanLimit, 32 );
	dataset->setObject( kIOGlobalContorlDecreaseFanLimit, tmpNumber );
	tmpNumber->release();

	if ( numberOfCores( ) == 4)
	{
		tmpNumber = OSNumber::withNumber( kMinReasonableOutputFor4Way, 32 );
		dataset->setObject( kIOOverrideMinValueTargetValue, tmpNumber );
		tmpNumber->release();
	}
	else
	{
		tmpNumber = OSNumber::withNumber( kMinReasonableOutputFor2Way, 32 );
		dataset->setObject( kIOOverrideMinValueTargetValue, tmpNumber );
		tmpNumber->release();
	}

	return( dataset );
}


/* static*/ IOGroupVirtualControl *PowerMac11_2_CPUsCtrlLoop::groupVirtualControlForID(OSString *groupID)
{
	IOGroupVirtualControl *groupControl = NULL;

	if ( gIOGroupVirtualControlDictionary == NULL )
	{
		gIOGroupVirtualControlDictionary = OSDictionary::withCapacity( 1 );
	}
	
	if ( gIOGroupVirtualControlDictionary )
	{
		groupControl = OSDynamicCast( IOGroupVirtualControl, gIOGroupVirtualControlDictionary->getObject( groupID ) );
		
		if ( groupControl == NULL )
		{
			groupControl = new IOGroupVirtualControl;
			
			if ( groupControl )
			{
				groupControl->init();
				
				if ( gIOGroupVirtualControlDictionary->setObject( groupID , groupControl ) )
				{
					// Now the object is in the dictionary, so I can release it, the
					// dictionary will keep the retain count:
					groupControl->release();
				}
			}
		}
	
	}
	
	return groupControl;
}

bool PowerMac11_2_CPUsCtrlLoop::init( void )
{
	bool result = super::init();
	
	PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::init %d returns %d\n", ( getCtrlLoopID() ? getCtrlLoopID()->unsigned16BitValue() : 0xFF )  ,result);

	mOverrideMinOutputValue = 0;

	return result;
}

UInt8 PowerMac11_2_CPUsCtrlLoop::numberOfCores( )
{
	OSIterator	*iterator;
	int numberOfCPUs = 0;

	// the number of cores is found counting the number of CPUS (since we have one cpu object
	// per core).
	iterator = IOService::getMatchingServices( IOService::serviceMatching( "MacRISC4CPU" ) );

	while ( ( iterator ) && ( iterator->getNextObject() ) )
	{		
		numberOfCPUs++;
	}

	if( iterator )
		iterator->release();
	
	return( numberOfCPUs );

}

void PowerMac11_2_CPUsCtrlLoop::free( void )
{
	if ( mGroupControl )
	{
		mGroupControl->release();
		mGroupControl = NULL;
	}

	if ( mActualNumberOfInputSets > 0 )
	{
		int i;
		
		for ( i = 0 ; i < mActualNumberOfInputSets ; i++ )
		{
			if ( mInputSetArray[ i ].referenceSAT )
				mInputSetArray[ i ].referenceSAT->release();
				
			if ( mInputSetArray[ i ].temperatureSensor )
				mInputSetArray[ i ].temperatureSensor->release();
				
			if ( mInputSetArray[ i ].powerSensor )
				mInputSetArray[ i ].powerSensor->release();

			mInputSetArray[ i ].temperatureSensor = NULL;
			mInputSetArray[ i ].powerSensor = NULL;
			mInputSetArray[ i ].referenceSAT = NULL;
		}
	}

	// Do not call super:free() because the superclass panics
	//super::free();
}

IOReturn PowerMac11_2_CPUsCtrlLoop::initPlatformCtrlLoop(const OSDictionary *dict)
{
	IOReturn result = kIOReturnSuccess;
	OSArray *array;
	OSDictionary *metaStateDict;
	OSNumber *number;
	
	PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::initPlatformCtrlLoop %d\n", ( getCtrlLoopID() ? getCtrlLoopID()->unsigned16BitValue() : 0xFF ));
	
	if ( ( number = OSDynamicCast( OSNumber, dict->getObject( kIOControlLoopCPUID ) ) ) == NULL )
	{
		PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::initPlatformCtrlLoop no %s property\n", kIOControlLoopCPUID);
		return kIOReturnError;
	}
	else
	{
		mCpuID = number->unsigned8BitValue();
	}
	
	if ( ( number = OSDynamicCast( OSNumber, dict->getObject( kIOControlLoopCoreID ) ) ) == NULL )
	{
		PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::initPlatformCtrlLoop no %s property\n", kIOControlLoopCoreID);
		return kIOReturnError;
	}
	else
	{
		mCoreID = number->unsigned8BitValue();
	}
	
	PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::initPlatformCtrlLoop %d cpu %d core %d\n",( getCtrlLoopID() ? getCtrlLoopID()->unsigned16BitValue() : 0xFF ), mCpuID, mCoreID);

	array = OSDynamicCast(OSArray, dict->getObject(gIOPPluginThermalMetaStatesKey) );
	if (array == NULL)
	{
		PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::initPlatformCtrlLoop no meta state array\n");
		return kIOReturnError;
	}

	// Create the metastate array from information in the thermal profile and the SDB.
	if ( ( metaStateDict = getPIDDataset( dict , mCpuID , mCoreID) ) == NULL )
	{
		PM11_2_DLOG( "PowerMac11_2_CPUsCtrlLoop::initPlatformCtrlLoop %d Cannot get PID dataset.\n", ( getCtrlLoopID() ? getCtrlLoopID()->unsigned16BitValue() : 0xFF ));

		return( kIOReturnError );
	}

	array->replaceObject( 0, metaStateDict );

	result = super::initPlatformCtrlLoop(dict);
	if ( result != kIOReturnSuccess )
	{
		PM11_2_DLOG( "PowerMac11_2_CPUsCtrlLoop::initPlatformCtrlLoop: super fails with error %08lx.\n", (UInt32)result);

		return result;
	}

	// Finds out which common control group this control loop refers to:
	// Common control loops group the output of multiple control loops to a single
	// control. This "virtual" control chooses which output to use and sets all its
	// belonging controls to that output:
	OSString *controlGroupName = OSDynamicCast( OSString, dict->getObject( kControlGroupingID ) );
	
	if ( controlGroupName != NULL )
	{
		mGroupControl = groupVirtualControlForID( controlGroupName );
		
		if ( mGroupControl == NULL )
		{
			PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::initPlatformCtrlLoop can not find or allcoate a mGroupControl for %s!!\n", controlGroupName->getCStringNoCopy());
			return(kIOReturnError);
		}
		else
		{
			mGroupControl->retain();
			mGroupControl->registerControlLoop( this );
		}
	}
	else
	{
		PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::initPlatformCtrlLoop can not find object for kControlGroupingID !!\n");
		return(kIOReturnError);
	}

	// Iterate trough all the controls and fill up the OutputSet. The controls should be in the following sequence:
	// Intake1 / Exaust 1 / Pump 1 / ... / Intake n / Exaust n / Pump n
	if ( ( array = OSDynamicCast( OSArray, dict->getObject(kIOPPluginThermalControlIDsKey) ) ) != NULL )
	{
		int i, tmpActualNumberOfOutputSets = array->getCount() / 3;
	
		if ( tmpActualNumberOfOutputSets == 0 )
		{
			PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::initPlatformCtrlLoop empty controls array!!\n");
			return(kIOReturnError);
		}
	
		for ( i = 0; i <  tmpActualNumberOfOutputSets; i ++ )
		{
			IOPlatformControl *intakeFan	= platformPlugin->lookupControlByID( OSDynamicCast(OSNumber, array->getObject( i * 3 ) ) );
			IOPlatformControl *exaustFan	= platformPlugin->lookupControlByID( OSDynamicCast(OSNumber, array->getObject( ( i * 3 ) + 1) ) );
			IOPlatformControl *pump		= platformPlugin->lookupControlByID( OSDynamicCast(OSNumber, array->getObject( ( i * 3 ) + 2) ) );

			if ( ( intakeFan == 0) || ( exaustFan == 0 ) )
			{
				PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::initPlatformCtrlLoop missing control or wrong id %p %p %p) !!\n", intakeFan, exaustFan, pump);
				return(kIOReturnError);
			}
			else
			{
				if ( outputControl == NULL )
					outputControl = exaustFan;
	
				mGroupControl->addExaustFan( exaustFan );
				mGroupControl->addIntakeFan( intakeFan );
				
				if ( pump != NULL )
					mGroupControl->addPump( pump );
					
				PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::initPlatformCtrlLoop adding control set %d of %d (%d) %p %p %p) !!\n", i, tmpActualNumberOfOutputSets, array->getCount() , intakeFan, exaustFan, pump);
			}
		}

	}
	else
	{
		PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::initPlatformCtrlLoop no sec control ID!!\n");
		return(kIOReturnError);
	}	

	// Iterate trough all the sensors and fill up the InputSet. The sensors should be in the following sequence:
	// Temperature 1 / Power  1 / ... / Temperature n / Power n

	if ( ( array = OSDynamicCast( OSArray, dict->getObject(kIOPPluginThermalSensorIDsKey) ) ) != NULL )
	{
		int i, maxNumberOfInputSets = numberOfCores( );
		
		mActualNumberOfInputSets = array->getCount() / 2;

		if ( mActualNumberOfInputSets > maxNumberOfInputSets )
			mActualNumberOfInputSets = maxNumberOfInputSets;

		if ( mActualNumberOfInputSets == 0 )
		{
			PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::initPlatformCtrlLoop empty sensors array!!\n");
			return(kIOReturnError);
		}				

		// sanity check: the sensors should be multiples of 2:
		assert ( ( mActualNumberOfInputSets % 2 ) == 0 );
	
		for ( i = 0; i <  mActualNumberOfInputSets; i ++ )
		{
			mInputSetArray[ i ].temperatureSensor = platformPlugin->lookupSensorByID( OSDynamicCast(OSNumber, array->getObject( ( i * 2 ) + 0) ) );
			mInputSetArray[ i ].powerSensor       = platformPlugin->lookupSensorByID( OSDynamicCast(OSNumber, array->getObject( ( i * 2 ) + 1) ) );

			if ( ( mInputSetArray[ i ].temperatureSensor == 0 ) ||( mInputSetArray[ i ].powerSensor == 0 ) )
			{
				PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::initPlatformCtrlLoop missing sensor or wrong id ( %d)  %p %p) !!\n",
					i * 2, mInputSetArray[ i ].temperatureSensor, mInputSetArray[ i ].powerSensor);
					
				return(kIOReturnError);
			}
			else
			{			
				mInputSetArray[ i ].temperatureSensor->retain();
				mInputSetArray[ i ].powerSensor->retain();

				if ( inputSensor == NULL )
					inputSensor = mInputSetArray[ i ].temperatureSensor;

				PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::initPlatformCtrlLoop  sensor set %d : %s %s) !!\n", i ,
					mInputSetArray[ i ].temperatureSensor->getSensorDescKey()->getCStringNoCopy(),
					mInputSetArray[ i ].powerSensor->getSensorDescKey()->getCStringNoCopy());
			}
		}
	}
	else
	{
		PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::initPlatformCtrlLoop no sec sensor ID!!\n");
		return(kIOReturnError);
	}	

	PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::initPlatformCtrlLoop %d returns %08lx\n", ( getCtrlLoopID() ? getCtrlLoopID()->unsigned16BitValue() : 0xFF ), (UInt32)result);
	return( result );
}

bool PowerMac11_2_CPUsCtrlLoop::updateMetaState( void )
{
	const OSArray * metaStateArray;
	const OSDictionary * metaStateDict;
	const OSNumber * newMetaState;
	
	PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::updateMetaState %d\n", ( getCtrlLoopID() ? getCtrlLoopID()->unsigned16BitValue() : 0xFF ) );

	// else if there is an overtemp condition, use meta-state 1
	// else if there is a forced meta state, use it
	// else, use meta-state 0

	if ((metaStateArray = OSDynamicCast(OSArray, infoDict->getObject(gIOPPluginThermalMetaStatesKey))) == NULL)
	{
		PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::updateMetaState no meta state array\n");
		return(false);
	}

	// Check for overtemp condition
	if (( platformPlugin->envArrayCondIsTrue(gIOPPluginEnvInternalOvertemp )) ||
	    ( platformPlugin->envArrayCondIsTrue(gIOPPluginEnvExternalOvertemp )))
	{
		//IOLog("PowerMac11_2_CPUsCtrlLoop::updateMetaState Entering Overtemp Mode (%d) ! %d !\n", platformPlugin->envArrayCondIsTrue(gIOPPluginEnvExternalOvertemp ) , platformPlugin->envArrayCondIsTrue(gIOPPluginEnvInternalOvertemp ));

		if ((metaStateDict = OSDynamicCast(OSDictionary, metaStateArray->getObject(1))) != NULL &&
		    (cacheMetaState( metaStateDict ) == true))
		{
			// successfully entered overtemp mode
			setMetaState( gIOPPluginOne );
			return(true);
		}
		else
		{
			IOLog("PowerMac11_2_CPUsCtrlLoop::updateMetaState Overtemp Mode Failed!\n");
		}
	}

	// Look for forced meta state
	if ((metaStateDict = OSDynamicCast(OSDictionary, infoDict->getObject(gIOPPluginForceCtrlLoopMetaStateKey))) != NULL)
	{
		if (cacheMetaState( metaStateDict ) == true)
		{
			PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::updateMetaState using forced meta state\n");
			newMetaState = OSNumber::withNumber( 0xFFFFFFFF, 32 );
			setMetaState( newMetaState );
			newMetaState->release();
			return(true);
		}
		else
		{
			PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::updateMetaState forced meta state is invalid, removing...\n");
			infoDict->removeObject(gIOPPluginForceCtrlLoopMetaStateKey);
		}
	}

	// Use default "Normal" meta state
	metaStateDict = OSDynamicCast(OSDictionary, metaStateArray->getObject(0));
	if ( metaStateDict == NULL )
	{
		// can't find a valid meta state, nothing we can really do except log an error
		PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::updateMetaState no valid meta states!\n");
		return(false);
	}
	
	if ( ! cacheMetaState( metaStateDict ) )
	{
		// can't find a valid meta state, nothing we can really do except log an error
		PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::updateMetaState cacheMetaState fails!\n");
		return(false);
	}

	PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::updateMetaState use meta state zero\n");
	setMetaState( gIOPPluginZero );

	// Re-check the sensors/controls state:
	if ( ( ! mAllControlsRegistred ) || (! mAllSensorsRegistred ) )
		allControlsAndSensorsRegistredCheck( );
	
	return(true);
}

bool PowerMac11_2_CPUsCtrlLoop::cacheMetaState( const OSDictionary* metaState )
{
	const OSNumber *numOverride;
	const OSNumber *numInterval;

	PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::cacheMetaState %d\n", ( getCtrlLoopID() ? getCtrlLoopID()->unsigned16BitValue() : 0xFF ) );

	// cache the interval.  it is listed in seconds.
	if ((numInterval = OSDynamicCast(OSNumber, metaState->getObject(kIOPPIDCtrlLoopIntervalKey))) != NULL)
	{
		UInt32 tempInterval = numInterval->unsigned32BitValue();

		if ((tempInterval == 0) || (tempInterval > 300))
		{
			PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::cacheMetaState meta state interval is out of bounds\n");
			return false;
		}
		
		intervalSec = tempInterval;
	}
	else
	{
		PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::cacheMetaState meta state interval is absent defaults to 1\n");
		intervalSec = 1;
	}

	// if there is an output-override key, flag it.  Otherwise, look for the full
	// set of coefficients, setpoints and output bounds
	if ((numOverride = OSDynamicCast(OSNumber, metaState->getObject(kIOPPIDCtrlLoopOutputOverrideKey))) != NULL)
	{
		overrideActive = true;
		outputOverride = numOverride;
		outputOverride->retain();

		PM11_2_DLOG("*** PID CACHE *** Override: 0x%08lX\n", (UInt32)outputOverride->unsigned32BitValue());
		//IOLog("*** PID CACHE *** Override: 0x%08lX\n", (UInt32)outputOverride->unsigned32BitValue());
	}
	else
	{
		const OSData *dataG_p, *dataG_d, *dataG_r;
		const OSNumber *numInputTarget, *numInputMax, *numOutputMin, *numOutputMax, *numHistLen, *numPowerMax, *numPowerAdj;
		
		const OSNumber *numOverTempSlewAverageOffset, *numOverTempSlewImmediateOffset, *numEndOfSlewOffset, *numOverTempSlewAverageNumberOfSamples;
		const OSNumber *numOverTempSleepAverageOffset, *numOverTempSleepImmediateOffset, *numOverTempSleepAverageNumberOfSamples;
	
		const OSNumber *decresingLimitingFactorNumber, *overrideMinValueNumber;
		
		samplePoint * sample, * newHistoryArray;
		unsigned int i;
		UInt32 newHistoryLen;
	 
		// look for G_p, G_d, G_r, input-target, output-max, output-min
		if ((dataG_p = OSDynamicCast(OSData, metaState->getObject(kIOPPIDCtrlLoopProportionalGainKey))) == NULL)
		{
			PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::cacheMetaState meta state has no G_p\n");
			return false;
		}

		if ((dataG_d = OSDynamicCast(OSData, metaState->getObject(kIOPPIDCtrlLoopDerivativeGainKey))) == NULL)
		{
			PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::cacheMetaState meta state has no G_d\n");
			return false;
		}

		if ((dataG_r = OSDynamicCast(OSData, metaState->getObject(kIOPPIDCtrlLoopResetGainKey))) == NULL)
		{
			PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::cacheMetaState meta state has no G_r\n");
			return false;
		}

		if ((numPowerMax = OSDynamicCast(OSNumber, metaState->getObject(kIOPPIDCtrlLoopMaxPowerKey))) == NULL)
		{
			PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::cacheMetaState meta state has no power-max\n");
			return false;
		}

		if ((numPowerAdj = OSDynamicCast(OSNumber, metaState->getObject(kIOPPIDCtrlLoopMaxPowerAdjustmentKey))) == NULL)
		{
			PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::cacheMetaState meta state has no power-max-adjustment\n");
			return false;
		}

		if ((numHistLen = OSDynamicCast(OSNumber, metaState->getObject(kIOPPIDCtrlLoopHistoryLenKey))) == NULL)
		{
			PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::cacheMetaState meta state has no history-length\n");
			return false;
		}
	
		if ((numInputTarget = OSDynamicCast(OSNumber, metaState->getObject(kIOPPIDCtrlLoopTargetTempDelta))) == NULL)
		{
			PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::cacheMetaState meta state has no intput-target delta\n");
			return false;
		}

		if ((numInputMax = OSDynamicCast(OSNumber, metaState->getObject(kIOPPIDCtrlLoopInputMaxKey))) == NULL)
		{
			PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::cacheMetaState meta state has no input-max\n");
			return false;
		}

		if ((numOverTempSlewAverageOffset = OSDynamicCast(OSNumber, metaState->getObject( kOverTempSlewAverageOffset))) == NULL)
		{
			PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::cacheMetaState meta state has no kOverTempSlewAverageOffset\n");
			return false;
		}

		if ((numOverTempSlewImmediateOffset = OSDynamicCast(OSNumber, metaState->getObject(kOverTempSlewImmediateOffset))) == NULL)
		{
			PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::cacheMetaState meta state has no kOverTempSlewImmediateOffset\n");
			return false;
		}

		if ((numEndOfSlewOffset = OSDynamicCast(OSNumber, metaState->getObject(kEndOfSlewOffset))) == NULL)
		{
			PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::cacheMetaState meta state has no kEndOfSlewOffset\n");
			return false;
		}

		if ((numOverTempSlewAverageNumberOfSamples = OSDynamicCast(OSNumber, metaState->getObject(kOverTempSlewAverageNumberOfSamples))) == NULL)
		{
			PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::cacheMetaState meta state has no kOverTempSlewAverageNumberOfSamples\n");
			return false;
		}

		if ((numOverTempSleepAverageOffset = OSDynamicCast(OSNumber, metaState->getObject( kOverTempSleepAverageOffset))) == NULL)
		{
			PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::cacheMetaState meta state has no kOverTempSleepAverageOffset\n");
			return false;
		}

		if ((numOverTempSleepImmediateOffset = OSDynamicCast(OSNumber, metaState->getObject(kOverTempSleepImmediateOffset))) == NULL)
		{
			PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::cacheMetaState meta state has no kOverTempSleepImmediateOffset\n");
			return false;
		}

		if ((numOverTempSleepAverageNumberOfSamples = OSDynamicCast(OSNumber, metaState->getObject(kOverTempSleepAverageNumberOfSamples))) == NULL)
		{
			PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::cacheMetaState meta state has no kOverTempSleepAverageNumberOfSamples\n");
			return false;
		}

		if ((numOutputMin = OSDynamicCast(OSNumber, metaState->getObject(kIOPPIDCtrlLoopOutputMinKey))) == NULL)
		{
			PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::cacheMetaState meta state has no output-min\n");
		}

		if ((numOutputMax = OSDynamicCast(OSNumber, metaState->getObject(kIOPPIDCtrlLoopOutputMaxKey))) == NULL)
		{
			PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::cacheMetaState meta state has no output-max\n");
		}

		if ((decresingLimitingFactorNumber = OSDynamicCast(OSNumber, metaState->getObject(kIOGlobalContorlDecreaseFanLimit))) == NULL)
		{
			PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::cacheMetaState meta state has no decrease-fan-limit\n");
		}


		if ((overrideMinValueNumber = OSDynamicCast(OSNumber, metaState->getObject(kIOOverrideMinValueTargetValue))) == NULL)
		{
			PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::cacheMetaState meta state has no kIOOverrideMinValueTargetValue\n");
		}

		overrideActive = false;
		if (outputOverride)
		{
			outputOverride->release();
			outputOverride = NULL;
		}

		G_p = *((SInt32 *) dataG_p->getBytesNoCopy());
		G_d = *((SInt32 *) dataG_d->getBytesNoCopy());
		G_r = *((SInt32 *) dataG_r->getBytesNoCopy());
	
		powerMaxAdj.sensValue = ((SInt32) numPowerMax->unsigned32BitValue()) - ((SInt32) numPowerAdj->unsigned32BitValue());

		PM11_2_RUNTIMELOG("***** PowerMac11_2_CPUsCtrlLoop::cacheMetaState powerMaxAdj: %ld == ( %ld - %ld)\n", powerMaxAdj.sensValue >> 16, ((SInt32) numPowerMax->unsigned32BitValue()) / (1 << 16), ((SInt32) numPowerAdj->unsigned32BitValue())  / (1 << 16));

		// Max Tempoerature:
		inputMax.sensValue = (SInt32)numInputMax->unsigned32BitValue();
		
		if ( numInputTarget )
		{
			inputTarget.sensValue = inputMax.sensValue - (SInt32)numInputTarget->unsigned32BitValue();
		}
		
		if ( numOutputMin )
			outputMin = numOutputMin->unsigned32BitValue();
		else
			outputMin = 1500; // Min 1500 RPM
			
		if ( numOutputMax )
			outputMax = numOutputMax->unsigned32BitValue();
		else
			outputMax = 0;
			
		if ( overrideMinValueNumber )
			mOverrideMinOutputValue = overrideMinValueNumber->unsigned32BitValue();
		else
			mOverrideMinOutputValue = 0;
	
		// resize the history array if necessary
		newHistoryLen = numHistLen->unsigned32BitValue();
		if (newHistoryLen == 0) newHistoryLen = 2;	// must be two or more in order to have a valid
													// derivative term
        
		// This limits how fast the fans can decrease their speed:
		if ( ( mGroupControl ) && ( decresingLimitingFactorNumber ) )
			mGroupControl->setDecresingLimitingFactor( decresingLimitingFactorNumber->unsigned32BitValue() );
		
		if (newHistoryLen != historyLen)
		{
			newHistoryArray = (samplePoint *) IOMalloc( sizeof(samplePoint) * newHistoryLen );
			bzero( newHistoryArray, sizeof(samplePoint) * newHistoryLen );

			// copy samples from the old array into the new
			for (i=0; i<historyLen && i<newHistoryLen; i++)
			{
				sample = sampleAtIndex(i);

				(&(newHistoryArray[i]))->sample.sensValue = sample->sample.sensValue;
				(&(newHistoryArray[i]))->error.sensValue = sample->error.sensValue;
			}

			IOFree( historyArray, sizeof(samplePoint) * historyLen );

			historyArray = newHistoryArray;
			historyLen = newHistoryLen;
			latestSample = 0;
		}

		// Overtemp Offsets:
		mOverTempSlewAverageOffset.sensValue = (SInt32)numOverTempSlewAverageOffset->unsigned32BitValue();
		mOverTempSlewImmediateOffset.sensValue = (SInt32)numOverTempSlewImmediateOffset->unsigned32BitValue();
		mEndOfSlewOffset.sensValue = (SInt32)numEndOfSlewOffset->unsigned32BitValue();
		mOverTempSlewAverageNumberOfSamples = (int)numOverTempSlewAverageNumberOfSamples->unsigned32BitValue();
	
		// Sleep Offsets:
		mOverTempSleepImmediateOffset.sensValue = (SInt32)numOverTempSleepImmediateOffset->unsigned32BitValue();
		mOverTempSleepAverageOffset.sensValue = (SInt32)numOverTempSleepAverageOffset->unsigned32BitValue();
		mOverTempSleepAverageNumberOfSamples = (int)numOverTempSleepAverageNumberOfSamples->unsigned32BitValue();

		PM11_2_RUNTIMELOG("mOverTempSlewAverageOffset %ld, mOverTempSlewImmediateOffset %ld, mEndOfSlewOffset %ld, mOverTempSlewAverageNumberOfSamples %d\n",
			mOverTempSlewAverageOffset.sensValue >> 16, mOverTempSlewImmediateOffset.sensValue >> 16, mEndOfSlewOffset.sensValue >> 16, mOverTempSlewAverageNumberOfSamples);

		PM11_2_RUNTIMELOG("mOverTempSleepImmediateOffset %ld, mOverTempSleepAverageOffset %ld, mOverTempSleepAverageNumberOfSamples %d\n",
			mOverTempSleepImmediateOffset.sensValue >> 16, mOverTempSleepAverageOffset.sensValue >> 16, mOverTempSleepAverageNumberOfSamples);
		
		int newTemperatureHistorySize = max ( mOverTempSlewAverageNumberOfSamples, mOverTempSleepAverageNumberOfSamples);
		
		if ( newTemperatureHistorySize !=  mTemperatureHistorySize )
		{
			if ( mTemperatureHistory )
			{
				IOFree ( mTemperatureHistory , sizeof(SensorValue) * mTemperatureHistorySize );
				mTemperatureHistory= NULL;
			}
		
			mTemperatureHistorySize = newTemperatureHistorySize;
			mTemperatureHistory = (SensorValue*)IOMalloc( sizeof(SensorValue) * newTemperatureHistorySize );
			bzero( mTemperatureHistory, sizeof(SensorValue) * newTemperatureHistorySize );
			mCurrentPointInTemperatureHistory = 0;
		}
		
		PM11_2_RUNTIMELOG("*** PID CACHE %d *** G_p: 0x%08lX G_d: 0x%08lX G_r: 0x%08lX\n"
		                  "******************** inT: 0x%08lX oMi: 0x%08lX oMa: 0x%08lX\n",
						  ( getCtrlLoopID() ? getCtrlLoopID()->unsigned16BitValue() : 0xFF ),
					      G_p, G_d, G_r, inputTarget.sensValue, outputMin, outputMax);
	}
        

	// set the interval
	clock_interval_to_absolutetime_interval(intervalSec, NSEC_PER_SEC, &interval);
	PM11_2_RUNTIMELOG("***************** Interval: %u\n", (unsigned int)intervalSec);

	return(true);
}

SensorValue PowerMac11_2_CPUsCtrlLoop::temperatureAverageOnLastNSamples( int numberOfSamples )
{
	SensorValue average = { 0 };
	int lastEntry = ( ( mCurrentPointInTemperatureHistory == 0 ) ? (mTemperatureHistorySize - 1 ) : mCurrentPointInTemperatureHistory - 1);
	int numberOfEntriesToAdd = numberOfSamples;
	
	while ( numberOfEntriesToAdd )
	{
		average.sensValue += mTemperatureHistory[ lastEntry ].sensValue;
		lastEntry = ( ( lastEntry == 0 ) ? (mTemperatureHistorySize - 1 ) : lastEntry - 1);
		numberOfEntriesToAdd--;
	}
	
	average.sensValue /= numberOfSamples;
	
	PM11_2_RUNTIMELOG("%d - temperatureAverageOnLastNSamples(%d) %ld\n",
		( getCtrlLoopID() ? getCtrlLoopID()->unsigned16BitValue() : 0xFF ),
		numberOfSamples ,average.sensValue >> 16 );

	return average;
}

void PowerMac11_2_CPUsCtrlLoop::deadlinePassed( void )
{
	bool deadlineAbsolute = (ctrlloopState == kIOPCtrlLoopFirstAdjustment);
	SensorValue maxPower = { 0 }, maxTemperature = { 0 };
	int i;
	
	if ( mAllControlsRegistred && mAllSensorsRegistred )
	{
		bool oldValueForTimerCallBackActive = timerCallbackActive;
	
		// Force adjustControls to update the targets
		timerCallbackActive = true;

		// getAggreateSensorValue
		// ======================

		// acquire sample (getAggreateSensorValue does not really work since it reads only one value):
		for ( i = 0; i <  mActualNumberOfInputSets; i ++ )
		{
			if ( mInputSetArray[ i ].referenceSAT != NULL )
			{
				if ( mInputSetArray[ i ].referenceSAT->setProperty("lock-sensors", kOSBooleanTrue) == false )
				{
					kprintf("WARNING WARNING lock-sensors true fails\n");
				}
			}
			else
			{
				kprintf("deadlinePassed WARNING WARNING missing SAT\n");
			}
		
			SensorValue currentPower = getSensorValue (mInputSetArray[ i ].powerSensor );
			SensorValue currentTemp	= getSensorValue (mInputSetArray[ i ].temperatureSensor);
		
			if ( maxTemperature.sensValue < currentTemp.sensValue )
				maxTemperature.sensValue = currentTemp.sensValue;

			if ( maxPower.sensValue < currentPower.sensValue )
				maxPower.sensValue = currentPower.sensValue;

			if ( mInputSetArray[ i ].referenceSAT != NULL )
			{
				if ( mInputSetArray[ i ].referenceSAT->setProperty("lock-sensors", kOSBooleanFalse) == false )
				{
					kprintf("WARNING WARNING lock-sensors false fails\n");
				}
			}
			else
			{
				kprintf("deadlinePassed WARNING WARNING missing SAT\n");
			}
		}

		// WORKAROUND: sensors aew way off I get values that are about 1/2 of what they should be. Multiply by 2:
		//maxPower.sensValue *= 2;
		//maxTemperature.sensValue *= 2;

		PM11_2_RUNTIMELOG("**** %d *** power=%ld temp=%ld\n", ( getCtrlLoopID() ? getCtrlLoopID()->unsigned16BitValue() : 0xFF ), maxPower.sensValue >> 16, maxTemperature.sensValue >> 16);
		
		// Add the max temperature to the hisory array of temprature:
		if (tempIndex == 0)
			tempIndex = kTemperatureSensorBufferSize - 1;
		else
			tempIndex --;
			
		tempHistory[tempIndex] = maxTemperature;
				
		// Now add the power to the history array (which is the history array of power):
		// move the top of the power array to the next spot -- it's circular
		if (latestSample == 0)
			latestSample = historyLen - 1;
		else
			latestSample --;

		// store the sample in the history
		historyArray[latestSample].sample.sensValue = maxPower.sensValue;
		
		// calculate the error term and store it (the average will be calculated on this)
		historyArray[latestSample].error.sensValue = powerMaxAdj.sensValue - maxPower.sensValue;
		
		PM11_2_RUNTIMELOG("**** %d *** index %d power=%ld \n", ( getCtrlLoopID() ? getCtrlLoopID()->unsigned16BitValue() : 0xFF ), latestSample, historyArray[latestSample].error.sensValue >> 16);

		// Add the last value to the history:
		mTemperatureHistory[ mCurrentPointInTemperatureHistory ] = maxTemperature;
		mCurrentPointInTemperatureHistory = ( ( mCurrentPointInTemperatureHistory == (mTemperatureHistorySize - 1 ) ) ? 0 : mCurrentPointInTemperatureHistory + 1);

#if 0
		// Use the average calculated above to see if we are in overtemp:
		bool overtempSlewingAverage = ( temperatureAverageOnLastNSamples( mOverTempSlewAverageNumberOfSamples ).sensValue >= ( ( 35 << 16 ) + mOverTempSlewAverageOffset.sensValue ) ) ? true : false;
		bool overtempSlewingImmediate = ( maxTemperature.sensValue >= ( ( 35 << 16 ) + mOverTempSlewImmediateOffset.sensValue ) ) ? true : false;
		bool stopSlewingNow =  ( maxTemperature.sensValue < ( ( 35 << 16 ) + mEndOfSlewOffset.sensValue ) ) ? true : false;
		
		// See if the sleep constions are met
		bool overtempImmediateSleep = (maxTemperature.sensValue >= ( ( 35 << 16 ) + mOverTempSleepImmediateOffset.sensValue ) )  ? true : false;
		bool overtempAverageSleep = (temperatureAverageOnLastNSamples( mOverTempSleepAverageNumberOfSamples ).sensValue >= ( ( 35 << 16 ) + mOverTempSleepAverageOffset.sensValue ) )  ? true : false;
#else
		// Use the average calculated above to see if we are in overtemp:
		bool overtempSlewingAverage = ( temperatureAverageOnLastNSamples( mOverTempSlewAverageNumberOfSamples ).sensValue >= ( inputMax.sensValue + mOverTempSlewAverageOffset.sensValue ) ) ? true : false;
		bool overtempSlewingImmediate = ( maxTemperature.sensValue >= ( inputMax.sensValue + mOverTempSlewImmediateOffset.sensValue ) ) ? true : false;
		bool stopSlewingNow =  ( maxTemperature.sensValue < ( inputMax.sensValue + mEndOfSlewOffset.sensValue ) ) ? true : false;
		
		// See if the sleep constions are met
		bool overtempImmediateSleep = (maxTemperature.sensValue >= ( inputMax.sensValue + mOverTempSleepImmediateOffset.sensValue ) )  ? true : false;
		bool overtempAverageSleep = (temperatureAverageOnLastNSamples( mOverTempSleepAverageNumberOfSamples ).sensValue >= ( inputMax.sensValue + mOverTempSleepAverageOffset.sensValue ) )  ? true : false;
#endif

		// First handle the overtemp cases as they induce sleep:
		// =====================================================
		if ( overtempImmediateSleep )
		{
			PM11_2_RUNTIMELOG("**** %d ***: Thermal Runaway Detected: System Will Sleep\n", ( getCtrlLoopID() ? getCtrlLoopID()->unsigned16BitValue() : 0xFF ));
			PM11_2_RUNTIMELOG("**** %d *** T_cur=%ld >= (T_max:%ld + sleepOffset:%ld)\n", ( getCtrlLoopID() ? getCtrlLoopID()->unsigned16BitValue() : 0xFF ),
					maxTemperature.sensValue >> 16, inputMax.sensValue >> 16, mOverTempSleepImmediateOffset.sensValue >> 16);

			platformPlugin->coreDump();
			platformPlugin->sleepSystem();
		}
		else if ( overtempAverageSleep )
		{
			PM11_2_RUNTIMELOG("**** %d ***: Long Overtemp Detected: System Will Sleep\n", ( getCtrlLoopID() ? getCtrlLoopID()->unsigned16BitValue() : 0xFF ));
			PM11_2_RUNTIMELOG("**** %d *** T_ave=%ld >= (T_max:%ld + sleepOffset:%ld)\n", ( getCtrlLoopID() ? getCtrlLoopID()->unsigned16BitValue() : 0xFF ),
					temperatureAverageOnLastNSamples( mOverTempSleepAverageNumberOfSamples ).sensValue >> 16, inputMax.sensValue >> 16, mOverTempSleepAverageOffset.sensValue >> 16);

			// Otherwise when we wake up we sleep again.
			//bzero( mTemperatureHistory, sizeof(SensorValue) * mTemperatureHistorySize );

			platformPlugin->coreDump();
			platformPlugin->sleepSystem();
		}
		else
		{
			bool didSetEnvVar	=	false;
			bool isSlewing		=	platformPlugin->envArrayCondIsTrueForObject(this, gIOPPluginEnvInternalOvertemp);
			
			PM11_2_RUNTIMELOG("**** %d ***: isSlewing %d (A %d, I %d) %d\n",
				( getCtrlLoopID() ? getCtrlLoopID()->unsigned16BitValue() : 0xFF ),
				isSlewing, overtempSlewingAverage, overtempSlewingImmediate, stopSlewingNow);
			
			// Not Overtemp: Look for the slewing instances:
			if ( (overtempSlewingAverage || overtempSlewingImmediate) )
			{
				if ( ! isSlewing )
				{
					// Handle slewing overtemp case
					platformPlugin->setEnvArray(gIOPPluginEnvInternalOvertemp, this, true);
					didSetEnvVar = true;
				}
			}
			else if ( stopSlewingNow )
			{
				if ( isSlewing )
				{
					// Stops the Slewing
					platformPlugin->setEnvArray(gIOPPluginEnvInternalOvertemp, this, false);
					didSetEnvVar = true;
				}
			}
			
			// now we can call adjustControls:
			if ( didSetEnvVar == false )
				adjustControls();
		}
		
		// Replace the timerCallbackActive with the original value
		timerCallbackActive = oldValueForTimerCallBackActive;
		
		// set the deadline
		if (deadlineAbsolute)
		{
			// this is the first time we're setting the deadline.  In order to better stagger
			// timer callbacks, offset the deadline by 100us * ctrlloopID.
			AbsoluteTime adjustedInterval;
			const OSNumber * id = getCtrlLoopID();

			// 100 * ctrlLoopID -> absolute time format
			clock_interval_to_absolutetime_interval(100 * id->unsigned32BitValue(), NSEC_PER_USEC, &adjustedInterval);

			// Add standard interval to produce adjusted interval
			ADD_ABSOLUTETIME( &adjustedInterval, &interval );

			clock_absolutetime_interval_to_deadline(adjustedInterval, &deadline);
		}
		else
		{
			ADD_ABSOLUTETIME(&deadline, &interval);
		}
	}
}

SensorValue PowerMac11_2_CPUsCtrlLoop::calculateDerivativeTerm( void ) const
{
	int	latest;
	int	previous;
	SensorValue	result;

	latest = tempIndex;
	previous = ( tempIndex == 0 ? ( kTemperatureSensorBufferSize - 1 ) : 0 );

	// Get the change in the error term over the latest interval.
	result.sensValue = tempHistory[ latest ].sensValue - tempHistory[ previous ].sensValue;

	// Divide by the elapsed time to get the slope.
	result.sensValue = result.sensValue / ( SInt32 ) intervalSec;

	return( result );
}

SensorValue PowerMac11_2_CPUsCtrlLoop::calculateIntegralTerm( void ) const
{
	SensorValue integralValue = super::calculateIntegralTerm( );

	// average on the number of samples
	integralValue.sensValue /= (SInt32)historyLen;

	return(integralValue);
}

SensorValue PowerMac11_2_CPUsCtrlLoop::calculateTargetTemp()
{
	// Calculate the adjusted target
	SInt32 rRaw = calculateIntegralTerm().sensValue;
	// this is 12.20 * 16.16 => 28.36
	SInt64 rProd = (SInt64)G_r * (SInt64)rRaw;
	// Calculates the new target temperature:
	SensorValue targetTemp;
	
	targetTemp.sensValue = inputMax.sensValue - (SInt32)((rProd >> 20) & 0xFFFFFFFF);
	
	targetTemp.sensValue = ( inputTarget.sensValue < targetTemp.sensValue ? inputTarget.sensValue : targetTemp.sensValue );
	
	mLastCalculatedDelta.sensValue = tempHistory[tempIndex].sensValue - targetTemp.sensValue ;
	
	PM11_2_RUNTIMELOG("**** %d *** min (tmax, tmax - average * G_r ): min( %ld , %ld - %ld * %ld) = %ld\n",
		( getCtrlLoopID() ? getCtrlLoopID()->unsigned16BitValue() : 0xFF ),
		inputMax.sensValue >> 16, inputMax.sensValue >> 16, rRaw >> 16, G_r >> 20,  targetTemp.sensValue >> 16);

	//IOLog("MarkC-Log1: %ld, %ld, %ld, %ld, %ld,",
	//	tempHistory[tempIndex].sensValue >> 16, historyArray[latestSample].sample.sensValue >> 16, rRaw >> 16, targetTemp.sensValue >> 16, mLastCalculatedDelta.sensValue >>16 );

	return targetTemp;
}

// This is identical to adjustControls, but it is calling calculateNewTargetNonConst instead than
// calculateNewTarget () const and all this because we need a non-const implementation.

void PowerMac11_2_CPUsCtrlLoop::adjustControls( void )
{
	ControlValue newTarget;

	//CTRLLOOP_DLOG("IOPlatformPIDCtrlLoop::adjustControls - entered\n");

	if (ctrlloopState == kIOPCtrlLoopNotReady || !timerCallbackActive)
	{
		//CTRLLOOP_DLOG("IOPlatformPIDCtrlLoop::adjustControls some entities not yet registered\n");
		return;
	}

	// Apply the PID algorithm
	newTarget = calculateNewTargetNonConst();

	// set the target
	sendNewTarget( newTarget );
}

// This is copied and pasted from Q77's one. The only change is the reference to
// the outputControl, which is now the group control
ControlValue PowerMac11_2_CPUsCtrlLoop::calculateNewTargetNonConst()
{
	SInt32 dRaw;
	SInt64 accum, dProd, pProd;
	SInt32 result;
	ControlValue newTarget;
	SensorValue adjInputTarget;

	// if there is an output override, use it
	if (overrideActive)
	{
		newTarget = outputOverride->unsigned32BitValue();
		//IOLog("MarkC-Log1: *** %d *** Override Active new target %lx (T=%ld)\n", ( getCtrlLoopID() ? getCtrlLoopID()->unsigned16BitValue() : 0xFF ), newTarget, tempHistory[tempIndex].sensValue >> 16);
		PM11_2_RUNTIMELOG("*** %d *** Override Active new target %lx\n", ( getCtrlLoopID() ? getCtrlLoopID()->unsigned16BitValue() : 0xFF ), newTarget);
	}

	// apply the PID algorithm to choose a new control target value
	else
	{
		if (ctrlloopState == kIOPCtrlLoopFirstAdjustment)
		{
			result = 0;
		}
		else
		{
			adjInputTarget = calculateTargetTemp();
				
			// do the PID iteration using the result from the last set value:
			//result = mLastTargetValue;  // <-- Uncomment this to feedback the fan value that this control loop "thought" it would set
			if ( mGroupControl ) result = mGroupControl->getTargetValue(); // <-- Uncomment this to feedback the actual fan value

			// calculate the derivative term
			// apply the derivative gain
			// this is 12.20 * 16.16 => 28.36
			dRaw = calculateDerivativeTerm().sensValue;
			accum = dProd = (SInt64)G_d * (SInt64)dRaw;
								
			// calculate the proportional term
			// apply the proportional gain
			// this is 12.20 * 16.16 => 28.36
			pProd = (SInt64)G_p * (SInt64)mLastCalculatedDelta.sensValue;
			accum += pProd;
							
			// truncate the fractional part
			accum >>= 36;
	
			PM11_2_RUNTIMELOG("**** %d *** prevfan + G_d * Dt + Gp * (currentT - Target T) : %ld + %ld * %ld + %ld * (%ld - %ld) = %ld\n",
				( getCtrlLoopID() ? getCtrlLoopID()->unsigned16BitValue() : 0xFF ),
				result, G_d >> 20 , dRaw >> 16 , G_p >> 20 , tempHistory[tempIndex].sensValue >> 16, adjInputTarget.sensValue >> 16, (SInt32)(result + (SInt32)accum));
	
			//result = (UInt32)(accum < 0 ? 0 : (accum & 0xFFFFFFFF));
			result += (SInt32)accum;
		}

		newTarget = ( ControlValue )( ( result > 0 ) ? result : 0 );

		//IOLog(/* "MarkC-Log1:" */ "%ld, %ld\n",
		//	mGroupControl->getTargetValue(), newTarget);

		// apply the hard limits
		if (newTarget < outputMin)
			newTarget = outputMin;
		else if (newTarget > outputMax)
			newTarget = outputMax;
	}

	return(newTarget);
}

void PowerMac11_2_CPUsCtrlLoop::sendNewTarget( ControlValue newTarget )
{
	if ( mGroupControl )
	{
		if ( mOverrideMinOutputValue > newTarget )
			newTarget = mOverrideMinOutputValue;
	
		mLastTargetValue = newTarget;
		
		PM11_2_RUNTIMELOG("**** %d *** (%p)  New Target = %ld currentDelta = %ld\n\n",
					( getCtrlLoopID() ? getCtrlLoopID()->unsigned16BitValue() : 0xFF ), this, newTarget, mLastCalculatedDelta.sensValue >> 16);
					
		mGroupControl->setTargetValue( newTarget, this, ( mLastCalculatedDelta.sensValue * 10 ) >> 16 );  // <- uncomment this to allow a comparison based on Delta T (as Paul Thompson Wished for Q77)
		//mGroupControl->setTargetValue( newTarget, this, newTarget );  // <- uncomment this to allow a comparison based on Fan speed (as Paul Thompson asked for the first version of Q63)
		
		// ensures that from now on a new target will be calculated.
		ctrlloopState = kIOPCtrlLoopAllRegistered;
	}
}

void PowerMac11_2_CPUsCtrlLoop::sensorRegistered( IOPlatformSensor * aSensor )
{
	PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::sensorRegistered %d - entered for %s\n", ( getCtrlLoopID() ? getCtrlLoopID()->unsigned16BitValue() : 0xFF ), aSensor->getSensorDescKey()->getCStringNoCopy());
	
	allControlsAndSensorsRegistredCheck( );
}

void PowerMac11_2_CPUsCtrlLoop::controlRegistered( IOPlatformControl * aControl )
{
	PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::controlRegistered (%p %p) %d - entered for %s\n",
	outputControl, aControl, ( getCtrlLoopID() ? getCtrlLoopID()->unsigned16BitValue() : 0xFF ), aControl->getControlDescKey()->getCStringNoCopy() );

	// If it is the reference control use to add the missing info in the metastates:
	if ( outputControl == aControl )
	{
		OSNumber	*tmpNumber;

		PM11_2_DLOG("PowerMac11_2_CPUsCtrlLoop::controlRegistered UPDATE MIN/MAX Values\n");

		// Updates the metastate array with the new values:
		OSArray* metaStateArray = OSDynamicCast( OSArray, infoDict->getObject( gIOPPluginThermalMetaStatesKey ) );
		OSDictionary* metaStateDict = OSDynamicCast( OSDictionary, metaStateArray->getObject( 0 ) );

		// Min output (RPM/PWM) (16-bit integer).  Get it from outputControl.
		outputMin = outputControl->getControlMinValue();

		tmpNumber = OSNumber::withNumber( outputMin, 16 );
		metaStateDict->setObject( kIOPPIDCtrlLoopOutputMinKey, tmpNumber );
		tmpNumber->release();

		// Max output (RPM/PWM) (16-bit integer).  Get it from outputControl.
		outputMax = outputControl->getControlMaxValue();
		tmpNumber = OSNumber::withNumber( outputMax, 16 );
		metaStateDict->setObject( kIOPPIDCtrlLoopOutputMaxKey, tmpNumber );
		tmpNumber->release();
		
		// Get "Failsafe" meta state dictionary.
		metaStateDict = OSDynamicCast( OSDictionary, metaStateArray->getObject( 1 ) );

		// Set "output-override" to be "output-max".
		tmpNumber = OSNumber::withNumber( outputMax, 16 );
		metaStateDict->setObject( kIOPPIDCtrlLoopOutputOverrideKey, tmpNumber );
		tmpNumber->release();
	}

	allControlsAndSensorsRegistredCheck( );
}
