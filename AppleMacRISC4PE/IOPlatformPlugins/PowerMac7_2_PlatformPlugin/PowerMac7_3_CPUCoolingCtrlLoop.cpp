/*
 * Copyright (c) 2002-2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
/*
 * Copyright (c) 2002-2004 Apple Computer, Inc.  All rights reserved.
 *
 *
 */


#include "IOPlatformPluginDefs.h"
#include "IOPlatformPluginSymbols.h"
#include "PowerMac7_2_PlatformPlugin.h"
#include "IOPlatformSensor.h"
#include "PowerMac7_3_CPUCoolingCtrlLoop.h"

#define super IOPlatformPIDCtrlLoop
OSDefineMetaClassAndStructors(PowerMac7_3_CPUCoolingCtrlLoop, IOPlatformPIDCtrlLoop)

//extern const OSSymbol * gPM72EnvShroudRemoved;
//extern const OSSymbol * gPM72EnvAllowNapping;
extern PowerMac7_2_PlatformPlugin * PM72Plugin;
extern const OSSymbol * gPM72EnvSystemUncalibrated;

bool PowerMac7_3_CPUCoolingCtrlLoop::init( void )
{
	if (!super::init()) return(false);

	secOutputControl = NULL;

	tempHistory[0].sensValue = tempHistory[1].sensValue = 0;
	tempIndex = 0;

	secondsAtMaxCooling = 0;

	return(true);
}

void PowerMac7_3_CPUCoolingCtrlLoop::free( void )
{
	if (secOutputControl) { secOutputControl->release(); secOutputControl = NULL; }

	super::free();
}

IOReturn PowerMac7_3_CPUCoolingCtrlLoop::initPlatformCtrlLoop( const OSDictionary *dict)
{
	IOReturn status;
	const OSArray * array;
	const OSNumber * num;

	//CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::initPlatformCtrlLoop ENTERED\n");

	// the cpu-id property tells me which cpu I'm related to
	num = OSDynamicCast(OSNumber, dict->getObject("cpu-id"));
	if (!num)
	{
		CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::initPlatformCtrlLoop no cpu-id\n");
		return(kIOReturnError);
	}

	procID = num->unsigned32BitValue();

	// Choose a PID dataset
	if (!choosePIDDataset( dict ))
	{
		CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::initPlatformCtrlLoop failed to choose PID dataset\n");
		return(kIOReturnError);
	}
	status = super::initPlatformCtrlLoop(dict);

	// the second to sixth control are slave controls (index 1 thru 5)
	if ((array = OSDynamicCast(OSArray, dict->getObject(kIOPPluginThermalControlIDsKey))) == NULL ||
		(secOutputControl = platformPlugin->lookupControlByID( OSDynamicCast(OSNumber, array->getObject(1)) )) == NULL) //cpu1 intake
	{
		CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::initPlatformCtrlLoop no sec control ID!!\n");
		return(kIOReturnError);
	}

	secOutputControl->retain();
	addControl( secOutputControl );
        

	if ((array = OSDynamicCast(OSArray, dict->getObject(kIOPPluginThermalControlIDsKey))) == NULL ||
		(thiOutputControl = platformPlugin->lookupControlByID( OSDynamicCast(OSNumber, array->getObject(2)) )) == NULL) //cpu2 exhaust 
	{
		CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::initPlatformCtrlLoop no third control ID!!\n");
		return(kIOReturnError);
	}
 thiOutputControl->retain();
	addControl( thiOutputControl );

	if ((array = OSDynamicCast(OSArray, dict->getObject(kIOPPluginThermalControlIDsKey))) == NULL ||
		(fouOutputControl = platformPlugin->lookupControlByID( OSDynamicCast(OSNumber, array->getObject(3)) )) == NULL) //cpu2 intake
	{
		CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::initPlatformCtrlLoop no fourth control ID!!\n");
		return(kIOReturnError);
	}
 	fouOutputControl->retain();
	addControl( fouOutputControl );


	if ((array = OSDynamicCast(OSArray, dict->getObject(kIOPPluginThermalControlIDsKey))) == NULL ||
		(fifOutputControl = platformPlugin->lookupControlByID( OSDynamicCast(OSNumber, array->getObject(4)) )) == NULL) //cpu1 pump
	{
		CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::initPlatformCtrlLoop no fifth control ID!!\n");
		return(kIOReturnError);
	}
 
	fifOutputControl->retain();
	addControl( fifOutputControl );


	if ((array = OSDynamicCast(OSArray, dict->getObject(kIOPPluginThermalControlIDsKey))) == NULL ||
		(sixOutputControl = platformPlugin->lookupControlByID( OSDynamicCast(OSNumber, array->getObject(5)) )) == NULL) //cpu2 pump
	{ 
		CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::initPlatformCtrlLoop no sixth control ID!!\n");
		return(kIOReturnError);
	}
 
	sixOutputControl->retain();
	addControl( sixOutputControl );


	// the secondary output control (a.k.a. the intake fan) is programmed with a scaled version of the
	// value from the primary.  This plist variable sets the scaling factor.  It is in 16.16 fixed point
	// format.
	num = OSDynamicCast(OSNumber, dict->getObject("intake-scaling-factor"));
	if (!num)
	{
		intakeScaling = 0x00010000;   // 1.0 in 16.16 fixed point
		CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::initPlatformCtrlLoop no intake-scaling-factor, assuming 1.0\n");
	}
	else
	{
		intakeScaling = num->unsigned32BitValue();
		CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::initPlatformCtrlLoop(%d) intake-scaling-factor is 0x%08lX\n",
				procID, intakeScaling);
	}

        
        
        // voltage sensor for proc1 is at index 1 in the sensor-id array
	if ((array = OSDynamicCast(OSArray, dict->getObject(kIOPPluginThermalSensorIDsKey))) == NULL)
	{
		CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::initPlatformCtrlLoop no SensorIDArray\n");
		return(kIOReturnError);
	}

	if ((voltageSensor1 = OSDynamicCast(IOPlatformSensor,
			platformPlugin->lookupSensorByID( OSDynamicCast(OSNumber, array->getObject(1)) ))) == NULL)
	{
		CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::initPlatformCtrlLoop no voltage sensor ID for 1st processor!!\n");
		return(kIOReturnError);
	}

	voltageSensor1->retain();
	addSensor( voltageSensor1 );

	// current sensor for proc1  is at index 2 in the sensor-id array
	if ((currentSensor1 = OSDynamicCast(IOPlatformSensor,
			platformPlugin->lookupSensorByID( OSDynamicCast(OSNumber, array->getObject(2)) ))) == NULL)
	{
		CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::initPlatformCtrlLoop no current sensor ID for 1st processor!!\n");
		return(kIOReturnError);
	}

	currentSensor1->retain();
	addSensor( currentSensor1 );

	// power sensor for proc1 is at index 3 in the sensor-id array
	if ((powerSensor1 = OSDynamicCast(IOPlatformSensor,
			platformPlugin->lookupSensorByID( OSDynamicCast(OSNumber, array->getObject(3)) ))) == NULL)
	{
		CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::initPlatformCtrlLoop no power sensor ID for 1st processor!!\n");
		return(kIOReturnError);
	}

	powerSensor1->retain();
	addSensor( powerSensor1 );

	// temp sensor for proc2  is at index 4 in the sensor-id array
	if ((inputSensor2 = OSDynamicCast(IOPlatformSensor,
			platformPlugin->lookupSensorByID( OSDynamicCast(OSNumber, array->getObject(4)) ))) == NULL)
	{
		CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::initPlatformCtrlLoop no temp sensor ID for 2nd processor!!\n");
		return(kIOReturnError);
	}

	inputSensor2->retain();
	addSensor( inputSensor2 );

        // voltage sensor for proc2 is at index 5 in the sensor-id array
	if ((voltageSensor2 = OSDynamicCast(IOPlatformSensor,
			platformPlugin->lookupSensorByID( OSDynamicCast(OSNumber, array->getObject(5)) ))) == NULL)
	{
		CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::initPlatformCtrlLoop no voltage sensor ID for 2nd processor!!\n");
		return(kIOReturnError);
	}

	voltageSensor2->retain();
	addSensor( voltageSensor2 );

	// current sensor for proc2  is at index 6 in the sensor-id array
	if ((currentSensor2 = OSDynamicCast(IOPlatformSensor,
			platformPlugin->lookupSensorByID( OSDynamicCast(OSNumber, array->getObject(6)) ))) == NULL)
	{
		CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::initPlatformCtrlLoop no current sensor ID for 2nd processor!!\n");
		return(kIOReturnError);
	}

	currentSensor2->retain();
	addSensor( currentSensor2 );

	// power sensor for proc2 is at index 7 in the sensor-id array
	if ((powerSensor2 = OSDynamicCast(IOPlatformSensor,
			platformPlugin->lookupSensorByID( OSDynamicCast(OSNumber, array->getObject(7)) ))) == NULL)
	{
		CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::initPlatformCtrlLoop no power sensor ID for 2nd processor!!\n");
		return(kIOReturnError);
	}

	powerSensor2->retain();
	addSensor( powerSensor2 );        
 

#ifdef CTRLLOOP_DEBUG
	const OSNumber * slewID = OSNumber::withNumber( 0x10, 32 );
	if ((slewControl = platformPlugin->lookupControlByID( slewID )) == NULL)
	{
		CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::initPlatformCtrlLoop no slew control!!\n");
		return(kIOReturnError);
	}
	slewID->release();
#endif

	//CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::initPlatformCtrlLoop ENTERED\n");

	return(status);
}

bool PowerMac7_3_CPUCoolingCtrlLoop::choosePIDDataset( const OSDictionary * ctrlLoopDict )
{
	int i, count, cmp;
	const OSArray * tmp_array;
	OSArray * metaStateArray;
	OSDictionary * eepromDataset, * plistDataset, * chosenDataset;
	const OSData * binID, * plistBinID;

	if (!ctrlLoopDict) return(false);
// get a pointer to the meta state array
	if ((metaStateArray = OSDynamicCast(OSArray, ctrlLoopDict->getObject(gIOPPluginThermalMetaStatesKey))) == NULL)
	{
		CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::choosePIDDataset failed to find meta state array\n");
		return(false);
	}

	//CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::choosePIDDataset proc %u ctrlLoopDict %08lX metaStateArray %08lX\n", procID,
	//		(UInt32) ctrlLoopDict, (UInt32) metaStateArray);

	// extract meta state data from IIC ROM
	if ((eepromDataset = fetchPIDDatasetFromROM()) == NULL)
	{
		CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::choosePIDDataset failed to fetch EEPROM dataset\n");
		return(false);
	}
       

	if ((binID = OSDynamicCast(OSData, eepromDataset->getObject( kPM72ProcessorBinKey ))) == NULL)
	{
		CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::choosePIDDataset failed to fetch processor bin id\n");
		return(false);
	}
       
	// Search the plist for a dataset that matches this CPU's bin ID
	if ((tmp_array = OSDynamicCast(OSArray, ctrlLoopDict->getObject( kPM72CPUPIDDatasetsKey ))) == NULL)
	{
		CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::choosePIDDataset failed to fetch plist datasets\n");
		chosenDataset = eepromDataset;
		goto installChosenDataset;
	}
        
	count = tmp_array->getCount();

	//CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::choosePIDDataset proc %u count %d\n", procID, count);
	
	for (i=0; i<count; i++)
	{
		if ((plistDataset = OSDynamicCast(OSDictionary, tmp_array->getObject(i))) == NULL)
			continue;

		if ((plistBinID = OSDynamicCast(OSData, plistDataset->getObject( kPM72ProcessorBinKey ))) == NULL)
			continue;

		if (binID->isEqualTo(plistBinID))
			break;

		plistDataset = NULL;
	}

	if (plistDataset == NULL)
	{
                // since we don't have a valid parameter list in our plist, use the data fetched from the EEPROM
		CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::choosePIDDataset plist datasets don't match processor\n");
		chosenDataset = eepromDataset;
				
                goto installChosenDataset;
	}
				
	plistDataset->retain();

// Compare versions of the eeprom and plist datasets.  Choose the one that's later, or use the
	// eeprom dataset if the versions are equal.
	cmp = comparePIDDatasetVersions( OSDynamicCast(OSData, eepromDataset->getObject(kPM72PIDDatasetVersionKey)),
			OSDynamicCast(OSData, plistDataset->getObject(kPM72PIDDatasetVersionKey)) );
	if (cmp == -2)
	{
		
                CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::choosePIDDataset dataset version comparison failed\n");
		return(false);
	}
	else if (cmp == -1)
	{
		
                chosenDataset = plistDataset;
		goto installChosenDataset;
	}
	else // cmp == 0 || cmp == 1
	{

                chosenDataset = eepromDataset;
		goto installChosenDataset;
	}

installChosenDataset:
		

                CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::choosePIDDataset using %s\n",
				chosenDataset == eepromDataset ? "EEPROM" : "PLIST");
		metaStateArray->replaceObject(0, chosenDataset);
               // 
               // if (plistDataset) plistDataset->release();
		 
                eepromDataset->release();
		
                return(true);
}

// Compares two PID dataset versions.  These are four bytes.  The format is as follows:
// 0xuuuabcld
// u = unused
// a = major
// b = minor
// c = very minor
// l = level (d, a, b, f)
// d = rev
//
// so 0x000100d9 is version 1.0.0d9.
//
// comparison procedure is to compare a, then b, then c, then l, then d
//
// this routine returns:
// -1 if v1 is less than v2
// 0 if v1 is equal to v2
// 1 if v1 is greater than v2
// -2 for error

#define MAJOR(x) (((x) & 0x000F0000) >> 16)
#define MINOR(x) (((x) & 0x0000F000) >> 12)
#define VERY_MINOR(x) (((x) & 0x00000F00) >> 8)
#define LEVEL(x) (((x) & 0x000000F0) >> 4)
#define REVISION(x) (((x) & 0x0000000F) >> 0)

int PowerMac7_3_CPUCoolingCtrlLoop::comparePIDDatasetVersions( const OSData * v1, const OSData * v2 ) const
{
	UInt32 lv1, lv2, t1, t2;

	UInt8 levelLookup[] = { /* 0 */ 0xFF,
	                        /* 1 */ 0xFF,
	                        /* 2 */ 0xFF,
	                        /* 3 */ 0xFF,
	                        /* 4 */ 0xFF,
	                        /* 5 */ 0xFF,
	                        /* 6 */ 0xFF,
	                        /* 7 */ 0xFF,
	                        /* 8 */ 0xFF,
	                        /* 9 */ 0xFF,
	                        /* A */ 1,
	                        /* B */ 2,
	                        /* C */ 0xFF,
	                        /* D */ 0,
	                        /* E */ 4,
	                        /* F */ 3 };

	if (v1 == NULL || v2 == NULL)
	{
		CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::comparePIDDatasetVersions bad args\n");
		return(-2);
	}

	lv1 = (*((UInt32 *) v1->getBytesNoCopy()) & 0x000FFFFF);
	lv2 = (*((UInt32 *) v2->getBytesNoCopy()) & 0x000FFFFF);

	//CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::comparePIDDatasetVersions %08lX %08lX\n", lv1, lv2);

	// check for equal
	if (lv1 == lv2) return(0);

	// compare major
	t1 = MAJOR(lv1);
	t2 = MAJOR(lv2);

	if (t1 < t2) return(-1);
	else if (t1 > t2) return(1);

	// compare minor
	t1 = MINOR(lv1);
	t2 = MINOR(lv2);

	if (t1 < t2) return(-1);
	else if (t1 > t2) return(1);

	// compare very minor
	t1 = VERY_MINOR(lv1);
	t2 = VERY_MINOR(lv2);

	if (t1 < t2) return(-1);
	else if (t1 > t2) return(1);

	// compare level : d=0, a=1, b=2, f=3
	t1 = (UInt32) levelLookup[LEVEL(lv1)];
	t2 = (UInt32) levelLookup[LEVEL(lv2)];

	if (t1 == 0xFF || t2 == 0xFF)
	{
		CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::comparePIDDatasetVersions invalid version\n");
		return(-2);
	}

	if (t1 < t2) return(-1);
	else if (t1 > t2) return(1);

	t1 = REVISION(lv1);
	t2 = REVISION(lv2);

	if (t1 < t2) return(-1);
	else if (t1 > t2) return(1);

	// if we're here, something is wrong.  The versions are not equal, but they're not greater
	// than or less than either?!?!
	CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::comparePIDDatasetVersions THIS SHOULDN'T HAPPEN\n");
	return(-2);
}
				

OSDictionary *PowerMac7_3_CPUCoolingCtrlLoop::fetchPIDDatasetFromROM( void ) const
{
	OSDictionary * dataset;
	const OSSymbol * tmp_symbol;
	const OSData * tmp_osdata;
	const OSNumber * tmp_number;
	UInt8 tmp_bytebuf[4];

	if ((dataset = OSDictionary::withCapacity(14)) == NULL)
	{
		CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::fetchPIDDatasetFromROM failed to allocate dictionary\n");
		return(NULL);
	}

	// Flag the source of this dataset
	tmp_symbol = OSSymbol::withCString( kPM72PIDDatasetSourceIICROM );
	dataset->setObject( kPM72PIDDatasetSourceKey, tmp_symbol );
	tmp_symbol->release();

	// Get processor bin identifier
	if (!PM72Plugin->readProcROM( procID, 0x08, 3, tmp_bytebuf ))
	{
		CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::fetchPIDDatasetFromROM failed to fetch processor bin ID\n");
		goto failReleaseDataset;
	}

	tmp_osdata = OSData::withBytes( tmp_bytebuf, 3 );
	dataset->setObject( kPM72ProcessorBinKey, tmp_osdata );
	tmp_osdata->release();

	// Get dataset version
	if (!PM72Plugin->readProcROM( procID, 0x04, 4, tmp_bytebuf ))
	{
		CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::fetchPIDDatasetFromROM failed to fetch processor ROM version\n");
		goto failReleaseDataset;
	}

	tmp_osdata = OSData::withBytes( tmp_bytebuf, 4 );
	dataset->setObject( kPM72PIDDatasetVersionKey, tmp_osdata );
	tmp_osdata->release();

	// Proportional gain (G_p)
	if (!PM72Plugin->readProcROM( procID, 0x2C, 4, tmp_bytebuf ))
	{
		CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::fetchPIDDatasetFromROM failed to fetch G_p\n");
		goto failReleaseDataset;
	}

	tmp_osdata = OSData::withBytes( tmp_bytebuf, 4 );
	dataset->setObject( kIOPPIDCtrlLoopProportionalGainKey, tmp_osdata );
	tmp_osdata->release();

	// Reset gain (G_r) -- this is actually used as the power integral gain
	if (!PM72Plugin->readProcROM( procID, 0x30, 4, tmp_bytebuf ))
	{
		CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::fetchPIDDatasetFromROM failed to fetch G_r\n");
		goto failReleaseDataset;
	}

	tmp_osdata = OSData::withBytes( tmp_bytebuf, 4 );
	dataset->setObject( kIOPPIDCtrlLoopResetGainKey, tmp_osdata );
	tmp_osdata->release();

	// Derivative gain (G_d)
	if (!PM72Plugin->readProcROM( procID, 0x34, 4, tmp_bytebuf ))
	{
		CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::fetchPIDDatasetFromROM failed to fetch G_d\n");
		goto failReleaseDataset;
	}

	tmp_osdata = OSData::withBytes( tmp_bytebuf, 4 );
	dataset->setObject( kIOPPIDCtrlLoopDerivativeGainKey, tmp_osdata );
	tmp_osdata->release();

	// Min output (RPM/PWM)
	if (!PM72Plugin->readProcROM( procID, 0x50, 2, tmp_bytebuf ))
	{
		CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::fetchPIDDatasetFromROM failed to fetch min output\n");
		goto failReleaseDataset;
	}

	tmp_number = OSNumber::withNumber( *((UInt16 *) tmp_bytebuf), 16 );
	dataset->setObject( kIOPPIDCtrlLoopOutputMinKey, tmp_number );
	tmp_number->release();

	// Max output (RPM/PWM)
	if (!PM72Plugin->readProcROM( procID, 0x52, 2, tmp_bytebuf ))
	{
		CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::fetchPIDDatasetFromROM failed to fetch max output\n");
		goto failReleaseDataset;
	}

	tmp_number = OSNumber::withNumber( *((UInt16 *) tmp_bytebuf), 16 );
	dataset->setObject( kIOPPIDCtrlLoopOutputMaxKey, tmp_number );
	tmp_number->release();

	// history length
	if (!PM72Plugin->readProcROM( procID, 0x2B, 1, tmp_bytebuf ))
	{
		CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::fetchPIDDatasetFromROM failed to fetch history length\n");
		goto failReleaseDataset;
	}

	tmp_number = OSNumber::withNumber( tmp_bytebuf[0], 8 );
	dataset->setObject( kIOPPIDCtrlLoopHistoryLenKey, tmp_number );
	tmp_number->release();

	// Target temperature
	if (!PM72Plugin->readProcROM( procID, 0x28, 1, tmp_bytebuf ))
	{
		CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::fetchPIDDatasetFromROM failed to fetch target temp\n");
		goto failReleaseDataset;
	}

	tmp_number = OSNumber::withNumber( ((UInt32)tmp_bytebuf[0]) << 16, 32 );
	dataset->setObject( kIOPPIDCtrlLoopInputTargetKey, tmp_number );
	tmp_number->release();

	// Max temperature
	if (!PM72Plugin->readProcROM( procID, 0x29, 1, tmp_bytebuf ))
	{
		CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::fetchPIDDatasetFromROM failed to fetch max temp\n");
		goto failReleaseDataset;
	}

	tmp_number = OSNumber::withNumber( ((UInt32)tmp_bytebuf[0]) << 16, 32 );
	dataset->setObject( kIOPPIDCtrlLoopInputMaxKey, tmp_number );
	tmp_number->release();

	// Max power
	if (!PM72Plugin->readProcROM( procID, 0x2A, 1, tmp_bytebuf ))
	{
		CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::fetchPIDDatasetFromROM failed to fetch max power\n");
		goto failReleaseDataset;
	}

	tmp_number = OSNumber::withNumber( ((UInt32)tmp_bytebuf[0]) << 16, 32 );
	dataset->setObject( kPM72MaxPowerKey, tmp_number );
	tmp_number->release();

	// Power adjustment
	if (!PM72Plugin->readProcROM( procID, 0x27, 1, tmp_bytebuf ))
	{
		CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::fetchPIDDatasetFromROM failed to fetch max power adjustment\n");
		goto failReleaseDataset;
	}

	tmp_number = OSNumber::withNumber( ((UInt32)tmp_bytebuf[0]) << 16, 32 );
	dataset->setObject( kPM72MaxPowerAdjustmentKey, tmp_number );
	tmp_number->release();

	// The iteration interval is not stored in the ROM, it is always set to 1 second
	dataset->setObject( kIOPPIDCtrlLoopIntervalKey, gIOPPluginOne );

	return(dataset);

failReleaseDataset:
	dataset->release();
	return(NULL);
}

SensorValue PowerMac7_3_CPUCoolingCtrlLoop::getAggregateSensorValue( void )
{
	SensorValue curValue, curValue1, curValue2;
	
	curValue1 = inputSensor->forceAndFetchCurrentValue();
	inputSensor->setCurrentValue( curValue1 );
	
	curValue2 = inputSensor2->forceAndFetchCurrentValue();
	inputSensor2->setCurrentValue( curValue2 );
	
	curValue = (curValue1.sensValue >= curValue2.sensValue) ? curValue1 : curValue2;
	
	return( curValue );
}

bool PowerMac7_3_CPUCoolingCtrlLoop::updateMetaState( void )
{
	const OSArray * metaStateArray;
	const OSDictionary * metaStateDict;
	const OSNumber * newMetaState;

	// else if there is an overtemp condition, use meta-state 1
	// else if there is a forced meta state, use it
	// else, use meta-state 0

	if ((metaStateArray = OSDynamicCast(OSArray, infoDict->getObject(gIOPPluginThermalMetaStatesKey))) == NULL)
	{
		CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::updateMetaState no meta state array\n");
		return(false);
	}

	// Check for overtemp condition
//	if ((platformPlugin->envArrayCondIsTrue(gIOPPluginEnvInternalOvertemp)) ||
//	    (platformPlugin->envArrayCondIsTrue(gIOPPluginEnvExternalOvertemp)))
	if ((platformPlugin->envArrayCondIsTrue(gIOPPluginEnvExternalOvertemp)) ||
	    (platformPlugin->getEnv(gPM72EnvSystemUncalibrated)) != NULL)
	{
		CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::updateMetaState Entering Overtemp Mode\n");

		if ((metaStateDict = OSDynamicCast(OSDictionary, metaStateArray->getObject(1))) != NULL &&
		    (cacheMetaState( metaStateDict ) == true))
		{
			// successfully entered overtemp mode
			setMetaState( gIOPPluginOne );
			return(true);
		}
		else
		{
			CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::updateMetaState Overtemp Mode Failed!\n");
		}
	}

	// Look for forced meta state
	if ((metaStateDict = OSDynamicCast(OSDictionary, infoDict->getObject(gIOPPluginForceCtrlLoopMetaStateKey))) != NULL)
	{
		if (cacheMetaState( metaStateDict ) == true)
		{
			CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::updateMetaState using forced meta state\n");
			newMetaState = OSNumber::withNumber( 0xFFFFFFFF, 32 );
			setMetaState( newMetaState );
			newMetaState->release();
			return(true);
		}
		else
		{
			CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::updateMetaState forced meta state is invalid, removing...\n");
			infoDict->removeObject(gIOPPluginForceCtrlLoopMetaStateKey);
		}
	}

	// Use default "Normal" meta state
	if ((metaStateDict = OSDynamicCast(OSDictionary, metaStateArray->getObject(0))) != NULL &&
	    (cacheMetaState( metaStateDict ) == true))
	{
		//CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::updateMetaState use meta state zero\n");
		setMetaState( gIOPPluginZero );
		return(true);
	}
	else
	{
		// can't find a valid meta state, nothing we can really do except log an error
		CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::updateMetaState no valid meta states!\n");
		return(false);
	}
}

bool PowerMac7_3_CPUCoolingCtrlLoop::acquireSample( void )
{
	samplePoint * latest;
	SensorValue curValue, curValue1, curValue2;

	// Set up the temperature history
	if (tempIndex == 1)
		tempIndex = 0;
	else
		tempIndex = 1;

	// fetch the temperature reading
	curValue = getAggregateSensorValue();

	tempHistory[tempIndex].sensValue = curValue.sensValue;

	// move the top of the power array to the next spot -- it's circular
	if (latestSample == 0)
		latestSample = historyLen - 1;
	else
		latestSample -= 1;

	// get a pointer to the array element where we'll store this sample point
	latest = &historyArray[latestSample];

	// fetch the power reading
	// the power sensor is a "fake" logical sensor.  In order to get a good reading,
	// we have to update the current and power sensors first.

	// for processor 1
	voltageSensor1->setCurrentValue( voltageSensor1->forceAndFetchCurrentValue() );
	currentSensor1->setCurrentValue( currentSensor1->forceAndFetchCurrentValue() );

	// the cpu power sensor doesn't need a force update, its fetchCurrentValue() does all the
	// work (there's no IOHWSensor instance to send a message to since it's a fake sensor)
	curValue1 = powerSensor1->fetchCurrentValue();
	powerSensor1->setCurrentValue( curValue1 );

    // for processor 2
	voltageSensor2->setCurrentValue( voltageSensor2->forceAndFetchCurrentValue() );
	currentSensor2->setCurrentValue( currentSensor2->forceAndFetchCurrentValue() );
	
	// the cpu power sensor doesn't need a force update, its fetchCurrentValue() does all the
	// work (there's no IOHWSensor instance to send a message to since it's a fake sensor)
	curValue2 = powerSensor2->fetchCurrentValue();
	powerSensor2->setCurrentValue( curValue2 );

	curValue = ( curValue1.sensValue >= curValue2.sensValue ) ? curValue1 : curValue2;
 
	// store the sample in the history
	latest->sample.sensValue = curValue.sensValue;
	
	// calculate the error term and store it
	latest->error.sensValue = powerMaxAdj.sensValue - latest->sample.sensValue;
	
	//CTRLLOOP_DLOG("*** SAMPLE *** InT: 0x%08lX Cur: 0x%08lX Error: 0x%08lX\n",
	//		inputTarget.sensValue, latest->sample.sensValue, latest->error.sensValue);

	return(true);
}

bool PowerMac7_3_CPUCoolingCtrlLoop::cacheMetaState( const OSDictionary * metaState )
{
	const OSData * dataG_p, * dataG_d, * dataG_r;
	const OSNumber * numInterval, * numOverride, * numInputTarget, * numInputMax;
	const OSNumber * numOutputMin, * numOutputMax, * numHistLen, * numPowerMax, * numPowerAdj;
	//const OSNumber * tmpNumber;
	samplePoint * sample, * newHistoryArray;
	unsigned int i;
	UInt32 tempInterval, newHistoryLen;
        
	// cache the interval.  it is listed in seconds.
	if ((numInterval = OSDynamicCast(OSNumber, metaState->getObject("interval"))) != NULL)
	{
		tempInterval = numInterval->unsigned32BitValue();

		if ((tempInterval == 0) || (tempInterval > 300))
		{
			CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::cacheMetaState meta state interval is out of bounds\n");
			goto failNoInterval;
		}
	}
	else
	{
		CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::cacheMetaState meta state interval is absent\n");
		goto failNoInterval;
	}
        

	// if there is an output-override key, flag it.  Otherwise, look for the full
	// set of coefficients, setpoints and output bounds
	if ((numOverride = OSDynamicCast(OSNumber, metaState->getObject(kIOPPIDCtrlLoopOutputOverrideKey))) != NULL)
	{
		overrideActive = true;
		outputOverride = numOverride;
		outputOverride->retain();

		//CTRLLOOP_DLOG("*** PID CACHE *** Override: 0x%08lX\n", outputOverride->unsigned32BitValue());
	}
	else
	{
		// look for G_p, G_d, G_r, input-target, output-max, output-min
		if ((dataG_p = OSDynamicCast(OSData, metaState->getObject(kIOPPIDCtrlLoopProportionalGainKey))) == NULL)
		{
			CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::cacheMetaState meta state has no G_p\n");
			goto failFullSet;
		}

		if ((dataG_d = OSDynamicCast(OSData, metaState->getObject(kIOPPIDCtrlLoopDerivativeGainKey))) == NULL)
		{
			CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::cacheMetaState meta state has no G_d\n");
			goto failFullSet;
		}

		if ((dataG_r = OSDynamicCast(OSData, metaState->getObject(kIOPPIDCtrlLoopResetGainKey))) == NULL)
		{
			CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::cacheMetaState meta state has no G_r\n");
			goto failFullSet;
		}

		if ((numInputTarget = OSDynamicCast(OSNumber, metaState->getObject(kIOPPIDCtrlLoopInputTargetKey))) == NULL)
		{
			CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::cacheMetaState meta state has no intput-target\n");
			goto failFullSet;
		}

		if ((numInputMax = OSDynamicCast(OSNumber, metaState->getObject(kIOPPIDCtrlLoopInputMaxKey))) == NULL)
		{
			CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::cacheMetaState meta state has no input-max\n");
			goto failFullSet;
		}

		if ((numPowerMax = OSDynamicCast(OSNumber, metaState->getObject(kPM72MaxPowerKey))) == NULL)
		{
			CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::cacheMetaState meta state has no power-max\n");
			goto failFullSet;
		}

		if ((numPowerAdj = OSDynamicCast(OSNumber, metaState->getObject(kPM72MaxPowerAdjustmentKey))) == NULL)
		{
			CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::cacheMetaState meta state has no power-max-adjustment\n");
			goto failFullSet;
		}

		if ((numOutputMin = OSDynamicCast(OSNumber, metaState->getObject(kIOPPIDCtrlLoopOutputMinKey))) == NULL)
		{
			CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::cacheMetaState meta state has no output-min\n");
			goto failFullSet;
		}

		if ((numOutputMax = OSDynamicCast(OSNumber, metaState->getObject(kIOPPIDCtrlLoopOutputMaxKey))) == NULL)
		{
			CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::cacheMetaState meta state has no output-max\n");
			goto failFullSet;
		}

		if ((numHistLen = OSDynamicCast(OSNumber, metaState->getObject(kIOPPIDCtrlLoopHistoryLenKey))) == NULL)
		{
			CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::cacheMetaState meta state has no history-length\n");
			goto failFullSet;
		}
        

		overrideActive = false;
		if (outputOverride) { outputOverride->release(); outputOverride = NULL; }

		G_p = *((SInt32 *) dataG_p->getBytesNoCopy());
		G_d = *((SInt32 *) dataG_d->getBytesNoCopy());
		G_r = *((SInt32 *) dataG_r->getBytesNoCopy());

		inputTarget.sensValue = (SInt32)numInputTarget->unsigned32BitValue();
		inputMax.sensValue = (SInt32)numInputMax->unsigned32BitValue();

		powerMaxAdj.sensValue = ((SInt32) numPowerMax->unsigned32BitValue()) -
				((SInt32) numPowerAdj->unsigned32BitValue());

		CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::cacheMetaState powerMaxAdj: %ld\n",
				powerMaxAdj.sensValue >> 16);

		outputMin = numOutputMin->unsigned32BitValue();
		outputMax = numOutputMax->unsigned32BitValue();

		// resize the history array if necessary
		newHistoryLen = numHistLen->unsigned32BitValue();
		if (newHistoryLen == 0) newHistoryLen = 2;	// must be two or more in order to have a valid
													// derivative term
        

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

//		CTRLLOOP_DLOG("*** PID CACHE *** G_p: 0x%08lX G_d: 0x%08lX G_r: 0x%08lX\n"
//		              "***************** inT: 0x%08lX oMi: 0x%08lX oMa: 0x%08lX\n",
//					  G_p, G_d, G_r, inputTarget, outputMin, outputMax);
	}
        

	// set the interval
	intervalSec = tempInterval;
	clock_interval_to_absolutetime_interval(intervalSec, NSEC_PER_SEC, &interval);
//	CTRLLOOP_DLOG("***************** Interval: %u\n", intervalSec);

	return(true);

failFullSet:
failNoInterval:
	return(false);

}

ControlValue PowerMac7_3_CPUCoolingCtrlLoop::calculateNewTarget( void ) const
{
	SInt32 pRaw, dRaw, rRaw;
	SInt64 accum, dProd, rProd, pProd;
	//UInt32 result, prevResult, scratch;
	SInt32 result;
	ControlValue newTarget;
	SensorValue adjInputTarget, sVal;

	// if there is an output override, use it
	if (overrideActive)
	{
		CTRLLOOP_DLOG("*** PID *** Override Active\n");
		newTarget = outputOverride->unsigned32BitValue();
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
			// Calculate the adjusted target
			rRaw = calculateIntegralTerm().sensValue;
			// this is 12.20 * 16.16 => 28.36
			rProd = (SInt64)G_r * (SInt64)rRaw; 
			sVal.sensValue = (SInt32)((rProd >> 20) & 0xFFFFFFFF);
			sVal.sensValue = inputMax.sensValue - sVal.sensValue;

			adjInputTarget.sensValue = inputTarget.sensValue < sVal.sensValue ? inputTarget.sensValue : sVal.sensValue;

			// do the PID iteration
			result = (SInt32)outputControl->getTargetValue();

			// calculate the derivative term
			// apply the derivative gain
			// this is 12.20 * 16.16 => 28.36
			dRaw = calculateDerivativeTerm().sensValue;
			accum = dProd = (SInt64)G_d * (SInt64)dRaw;
	
			// calculate the reset term
			// apply the reset gain
			// this is 12.20 * 16.16 => 28.36
			//rRaw = calculateIntegralTerm().sensValue;
			//rProd = (SInt64)G_r * (SInt64)rRaw; 
			//accum += rProd;
	
			// calculate the proportional term
			// apply the proportional gain
			// this is 12.20 * 16.16 => 28.36
			pRaw = tempHistory[tempIndex].sensValue - adjInputTarget.sensValue;
			pProd = (SInt64)G_p * (SInt64)pRaw;
			accum += pProd;
			
			// truncate the fractional part
			accum >>= 36;
	
			//result = (UInt32)(accum < 0 ? 0 : (accum & 0xFFFFFFFF));
			result += (SInt32)accum;
		}

		newTarget = (UInt32)(result > 0) ? result : 0;

		// apply the hard limits
		if (newTarget < outputMin)
			newTarget = outputMin;
		else if (newTarget > outputMax)
			newTarget = outputMax;

/*
#ifdef CTRLLOOP_DEBUG
		if (timerCallbackActive)
		{
			const OSString * tempDesc;
			const OSNumber * val;
			val = slewControl->fetchCurrentValue();
			slewControl->setCurrentValue( val );
			val->release();
#endif
			CTRLLOOP_DLOG("%s"
						" powerSum=%08lX (%ld)"
						" G_power=%08lX"
						" powerProd=%016llX (%lld)"
						" Ttgt=%08lX (%ld)"
						" Tmax=%08lX (%ld)"
						" TtgtAdj=%08lX (%ld)"
						" T_cur=%08lX (%ld)"
						" T_err=%08lX (%ld)"
						" G_p=%08lX"
						" pProd=%016llX (%lld)"
						" dRaw=%08lX (%ld)"
						" G_d=%08lX"
						" dProd=%016llX (%lld)"
						" Res=%016llX"
						" Out=%lu"
						" Spd=%0u\n",
						(tempDesc = OSDynamicCast( OSString, infoDict->getObject(kIOPPluginThermalGenericDescKey))) != NULL ?
								tempDesc->getCStringNoCopy() : "Unknown CtrlLoop",
						rRaw, rRaw >> 16,
						G_r,
						rProd, rProd >> 36,
						inputTarget.sensValue, inputTarget.sensValue >> 16,
						inputMax.sensValue, inputMax.sensValue >> 16,
						adjInputTarget.sensValue, adjInputTarget.sensValue >> 16,
						tempHistory[tempIndex].sensValue, tempHistory[tempIndex].sensValue >> 16,
						pRaw, pRaw >> 16,
						G_p,
						pProd, pProd >> 36,
						dRaw, dRaw >> 16,
						G_d,
						dProd, dProd >> 36,
						accum,
						uResult,
						slewControl->getCurrentValue()->unsigned8BitValue() );
#ifdef CTRLLOOP_DEBUG
		}
#endif
*/

	}

	return(newTarget);
}

SensorValue PowerMac7_3_CPUCoolingCtrlLoop::calculateDerivativeTerm( void ) const
{
	int latest, previous;
	SensorValue result;

	latest = tempIndex;
	previous = tempIndex == 0 ? 1 : 0;

	// get the change in the error term over the latest inteval
	result.sensValue = tempHistory[latest].sensValue - tempHistory[previous].sensValue;

	// divide by the elapsed time to get the slope
	result.sensValue /= (SInt32)intervalSec;

	return(result);
}

void PowerMac7_3_CPUCoolingCtrlLoop::deadlinePassed( void )
{
	bool deadlineAbsolute;
	bool didSetEnv = false;
	bool tMaxExceededNow, tMaxExceededNowCritical, tMaxExceededPreviously, maxCoolingApplied;
	SensorValue temperatureDiff;

	deadlineAbsolute = (ctrlloopState == kIOPCtrlLoopFirstAdjustment);

	timerCallbackActive = true;

	// sample the input
	if (!acquireSample())
	{
		CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::deadlinePassed FAILED TO ACQUIRE INPUT SAMPLE!!!\n");
	}

	// check to see if we've exceeded the critical temperature
	temperatureDiff.sensValue = tempHistory[tempIndex].sensValue - inputMax.sensValue;


	tMaxExceededPreviously = platformPlugin->envArrayCondIsTrueForObject(this, gIOPPluginEnvInternalOvertemp);
	tMaxExceededNow = temperatureDiff.sensValue > 0;


	// if the CPU temperature is above T_max, then we may need to force the machine to sleep
	if (tMaxExceededNow)
	{
		// if tMaxExceededPreviously is true, we know that the internal overtemp flag was set
		// causing the CPU to slew slow
		maxCoolingApplied = (tMaxExceededPreviously &&
		                     outputControl->getTargetValue() >= outputMax);

		if (maxCoolingApplied)
		{
			secondsAtMaxCooling += intervalSec;
			CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::deadlinePassed secondsAtMaxCooling=%u\n",
					secondsAtMaxCooling);
		}

		// Check for forced sleep conditions
		// If CPU temperature is above (T_max + 8)
		// -OR-
		// CPU temperature is above (T_max) and max cooling has been applied for 30 seconds or more
		tMaxExceededNowCritical = temperatureDiff.sensValue >= kPM72CPUTempCriticalOffset;

		if (tMaxExceededNowCritical ||
		    secondsAtMaxCooling >= kPM72CPUMaxCoolingLimit)
		{
			IOLog("PowerMac7,3 Thermal Manager: Thermal Runaway Detected: System Will Sleep\n");
			CTRLLOOP_DLOG("PowerMac7,3 Thermal Manager: Thermal Runaway Detected: System Will Sleep\n");
			CTRLLOOP_DLOG("PM73 T_cur=%u, T_max=%u, tMaxExceededNowCritical=%s, secondsAtMaxCooling=%u\n",
					tempHistory[tempIndex].sensValue >> 16, inputMax.sensValue >> 16,
					tMaxExceededNowCritical ? "TRUE" : "FALSE", secondsAtMaxCooling);
	
			platformPlugin->sleepSystem();
		}
	}


	// set or clear the internal overtemp flag as needed.  If set, this causes the CPU to slew slow.
	if (tMaxExceededPreviously && !tMaxExceededNow)
	{
		secondsAtMaxCooling = 0;
		platformPlugin->setEnvArray(gIOPPluginEnvInternalOvertemp, this, false);
		didSetEnv = true;
	}
	else if (tMaxExceededNow && !tMaxExceededPreviously)
	{
		platformPlugin->setEnvArray(gIOPPluginEnvInternalOvertemp, this, true);
		didSetEnv = true;
	}

	// If we changed the environment, the platform plugin will invoke updateMetaState()
	// and adjustControls().  If not, then we just need to call adjustControls()
	if (!didSetEnv)
	{
		adjustControls();
	}


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

	timerCallbackActive = false;
}

void PowerMac7_3_CPUCoolingCtrlLoop::sendNewTarget( ControlValue newTarget )
{
	// newTarget is the exhaust fan speeds
	// intakeTarget is the intake fan speeds
	// pumpTarget is the pump speeds
	ControlValue intakeTarget, pumpTarget;

	// hard-coded pump max & min RPMs
	UInt32 outputMaxForPump = 3700;
	UInt32 outputMinForPump = 1000;
        

/*
	// If the fans are at minimum speed, don't let the processors nap.  We need to
	// keep the heat sink warm or it loses efficiency.
	bool allowNapping = (newTarget->unsigned32BitValue() != outputMin);

	// Sync the internal napping state with the platform plugin's environment dict
	if (ctrlloopState == kIOPCtrlLoopFirstAdjustment ||
	    allowNapping != platformPlugin->envArrayCondIsTrueForObject(this, gPM72EnvAllowNapping))
	{
		platformPlugin->setEnvArray(gPM72EnvAllowNapping, this, allowNapping);
	}
*/
	// If the new target value is different, send it to the control
	if (ctrlloopState == kIOPCtrlLoopFirstAdjustment ||
		ctrlloopState == kIOPCtrlLoopDidWake ||
		newTarget != outputControl->getTargetValue() )
	{
		if (outputControl->sendTargetValue( newTarget )) //outputControl is still cpu1 exhaust fan
		{
			outputControl->setTargetValue(newTarget);
			ctrlloopState = kIOPCtrlLoopAllRegistered;
		}
		else
		{
			CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::sendNewTarget failed to send target value\n");
		}

		if (thiOutputControl->sendTargetValue( newTarget ))
		{
			thiOutputControl->setTargetValue( newTarget );
		}
		else
		{
			CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::sendNewTarget failed to send target value (third)\n");
		}

		// the second and fifth controls (intake fans) are programmed with a scaled version
		// of the primary control (exhaust fans) value.  Now I need to apply the 16.16
		// scaling factor to the exhaust fan's target in order to calculate the
		// intake fan's target.  This requires some shifting since the scaling
		// factor is a 16.16 fixed point value.
				
		intakeTarget = (newTarget * intakeScaling) >> 16; //cpu1 intake
				
		// The scaled value is again clipped to the min/max output rpm.
		if (intakeTarget < outputMin)
			intakeTarget = outputMin;
		else if (intakeTarget > outputMax)
			intakeTarget = outputMax;

		// program the intake fans, secOutputControl and fouOutputControl		
		if (secOutputControl->sendTargetValue( intakeTarget ))
		{
			secOutputControl->setTargetValue( intakeTarget );
		}
		else
		{
			CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::sendNewTarget failed to send target value (secondary)\n");
		}
		
		if (fouOutputControl->sendTargetValue( intakeTarget ))
		{
			fouOutputControl->setTargetValue( intakeTarget );
		}
		else
		{
			CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::sendNewTarget failed to send target value (fourth)\n");
		}

		// The fifth and sixth control are the pumps.
		// The pumps get programmed with a scaled version of the exhaust speed.  The
		// scaling factor is simply a proportion between the max fan RPM and the max
		// pump RPM, the objective being that if the fans are at max, the pumps will
		// also be at max.
		pumpTarget = newTarget * (outputMaxForPump/outputMax);

		// clip to the pump's max and min
		if (pumpTarget < outputMinForPump)
			pumpTarget = outputMinForPump;
		else if (pumpTarget > outputMaxForPump)
			pumpTarget = outputMaxForPump;
		
		if (fifOutputControl->sendTargetValue( pumpTarget ))
		{
			fifOutputControl->setTargetValue( pumpTarget );
		}
		else
		{
			CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::sendNewTarget failed to send target value (fifth)\n");
		}
		
		if (sixOutputControl->sendTargetValue( pumpTarget ))
		{
			sixOutputControl->setTargetValue( pumpTarget );
		}
		else
		{
			CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::sendNewTarget failed to send target value (sixth)\n");
		}
	}
}

void PowerMac7_3_CPUCoolingCtrlLoop::sensorRegistered( IOPlatformSensor * aSensor )
{
	//CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::sensorRegistered - entered\n");
	if (inputSensor->isRegistered() == kOSBooleanTrue &&
            inputSensor2->isRegistered() == kOSBooleanTrue &&

	    voltageSensor1->isRegistered() == kOSBooleanTrue &&
	    voltageSensor2->isRegistered() == kOSBooleanTrue &&

	    currentSensor1->isRegistered() == kOSBooleanTrue &&
	    currentSensor2->isRegistered() == kOSBooleanTrue &&

	    outputControl->isRegistered() == kOSBooleanTrue &&
	    secOutputControl->isRegistered() == kOSBooleanTrue && 
	    thiOutputControl->isRegistered() == kOSBooleanTrue &&
	    fouOutputControl->isRegistered() == kOSBooleanTrue &&
            // fifth is RPM0, but it might not exist on single pump systems
	    sixOutputControl->isRegistered() == kOSBooleanTrue) 
	{
		
                //CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::sensorRegistered allRegistered!\n");

		ctrlloopState = kIOPCtrlLoopFirstAdjustment;

		// set the deadline
		deadlinePassed();
	}
}

void PowerMac7_3_CPUCoolingCtrlLoop::controlRegistered( IOPlatformControl * aControl )
{
	
        //CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::controlRegistered - entered\n");
	
	if (inputSensor->isRegistered() == kOSBooleanTrue &&
            inputSensor2->isRegistered() == kOSBooleanTrue &&

	    voltageSensor1->isRegistered() == kOSBooleanTrue &&
	    voltageSensor2->isRegistered() == kOSBooleanTrue &&

	    currentSensor1->isRegistered() == kOSBooleanTrue &&
	    currentSensor2->isRegistered() == kOSBooleanTrue &&

	    outputControl->isRegistered() == kOSBooleanTrue &&
	    secOutputControl->isRegistered() == kOSBooleanTrue && 
	    thiOutputControl->isRegistered() == kOSBooleanTrue &&
	    fouOutputControl->isRegistered() == kOSBooleanTrue &&
            // fifth is RPM0, but it might not exist on single pump systems
	    sixOutputControl->isRegistered() == kOSBooleanTrue) 
	{
		//CTRLLOOP_DLOG("PowerMac7_3_CPUCoolingCtrlLoop::sensorRegistered allRegistered!\n");
            
		ctrlloopState = kIOPCtrlLoopFirstAdjustment;

		// set the deadline
		deadlinePassed();
	}
}
