/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
 *
 *	HeathrowATA.cpp
 *	
	Defines the concrete driver for a heathrow-ata controller.
	Descendent of MacIOATA, which is derived from IOATAController.

 *
 *
 */
 
#include "HeathrowATA.h"
#include <IOKit/ata/ATADeviceNub.h>

#include <IOKit/assert.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOTypes.h>
#include <IOKit/ata/IOATATypes.h>
#include <IOKit/ata/IOATAController.h>
#include <IOKit/ata/IOATACommand.h>
#include <IOKit/ata/IOATADevice.h>
#include <IOKit/ata/IOATABusInfo.h>
#include <IOKit/ata/IOATADevConfig.h>

#include <IOKit/platform/AppleMacIODevice.h>

#include <libkern/OSByteOrder.h>
#include <libkern/OSAtomic.h>


 

#ifdef DLOG
#undef DLOG
#endif

#ifdef  ATA_DEBUG
#define DLOG(fmt, args...)  IOLog(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

// some day, we'll have an ATA recorder for IOKit

#define ATARecordEventMACRO(type,param,bus,data) 		(void) (type); (void) (param); (void) (bus); (void) (data)

#define kCompatibleString "heathrow-ata"
			/*  Bit-significant representation of supported modes.*/ 
#define kATASupportedPIOModes		 0x001F	// modes 4, 3, 2, 1, and 0
#define kATASupportedMultiDMAModes	 0x0007	// modes 2, 1, and 0

/* Number of entries in various tables */
#define kHeathrowPIOCycleEntries			11
#define kHeathrowMultiDMACycleEntries 		9

// ------ ¥¥¥ Maximum transfer modes the Heathrow plugin is capable of supporting ¥¥¥ ÐÐÐÐÐÐÐÐ
#define kATAMaxPIOMode		 4
#define kATAMaxMultiDMAMode	2




#pragma mark -IOService Overrides -


//---------------------------------------------------------------------------

#define super MacIOATA

OSDefineMetaClassAndStructors(  HeathrowATA, MacIOATA )

//---------------------------------------------------------------------------

bool 
HeathrowATA::init(OSDictionary* properties)
{
	DLOG("HeathrowATA init start\n");


    // Initialize instance variables.
   
    if (super::init(properties) == false)
    {
        DLOG("HeathrowATA: super::init() failed\n");
        return false;
    }

	// initialize the timing params to PIO mode 0 at 600ns and MWDMA mode 0 at 480ns
	
	busTimings[0].cycleRegValue = busTimings[1].cycleRegValue = 0x00000526 | 0x00074000;

	busTimings[0].ataPIOSpeedMode = busTimings[1].ataPIOSpeedMode = 0x01 ;				// PIO Mode Timing class (bit-significant)
	busTimings[0].ataPIOCycleTime = busTimings[1].ataPIOCycleTime = 600 ;				// Cycle time for PIO mode
	busTimings[0].ataMultiDMASpeed = busTimings[1].ataMultiDMASpeed = 0x01;				// Multiple Word DMA Timing Class (bit-significant)
	busTimings[0].ataMultiCycleTime = busTimings[1].ataMultiCycleTime = 480;				// Cycle time for Multiword DMA mode

	DLOG("HeathrowATA init done\n");

    return true;
}

/*---------------------------------------------------------------------------
 *
 *	Check and see if we really match this device.
 * override to accept or reject close matches based on further information
 ---------------------------------------------------------------------------*/

IOService* 
HeathrowATA::probe(IOService* provider,	SInt32*	score)
{

    OSData		*compatibleEntry;
	
	DLOG("HeathrowATA starting probe\n");


	compatibleEntry  = OSDynamicCast( OSData, provider->getProperty( "compatible" ) );
	if ( compatibleEntry == 0 ) 
	{
		// error unknown controller type.

		DLOG("HeathrowATA failed getting compatible property\n");
		return 0;

	}

	
	// test the compatible property for a match to the controller type name.
	if ( compatibleEntry->isEqualTo( kCompatibleString, sizeof(kCompatibleString)-1 ) == false ) 
	{
		// not our type of controller		
		DLOG("HeathrowATA compatible property doesn't match\n");
		return 0;
		
	}

	// ok, it is the type of cell we control
	
	return this;


}


/*---------------------------------------------------------------------------
 *
 *	Override IOService start.
 *
 *	Subclasses should override the start method, call the super::start
 *	first then add interrupt sources and probe their busses for devices 
 *	and create device nubs as needed.
 ---------------------------------------------------------------------------*/

bool 
HeathrowATA::start(IOService *provider)
{

    DLOG("HeathrowATA::start() begin\n");
    
    if( ! OSDynamicCast(AppleMacIODevice, provider ) )
    {
    
    	DLOG("HeathrowATA provider not AppleMacIODevice!\n");
    	return false;    
    
    }

	ATADeviceNub* newNub=0L;
 
 	// call start on the superclass
    if (!super::start( provider))
 	{
        DLOG("HeathrowATA: super::start() failed\n");
        return false;
	}

    DLOG("HeathrowATA::start() done\n");

	for( UInt32 i = 0; i < 2; i++)
	{
		if( _devInfo[i].type != kUnknownATADeviceType )
		{
		
			DLOG("HeathrowATA creating nub\n");
			newNub = ATADeviceNub::ataDeviceNub( (IOATAController*)this, (ataUnitID) i, _devInfo[i].type );
		
			if( newNub )
			{
				
				DLOG("HeathrowATA attach nub\n");
				
				newNub->attach(this);
			
				_nub[i] = (IOATADevice*) newNub;
				
				DLOG("HeathrowATA register nub\n");

				newNub->registerService();
				newNub = 0L;
					
			}
		}
	
	}

    return true;

}

/*---------------------------------------------------------------------------
 *	free() - the pseudo destructor. Let go of what we don't need anymore.
 *
 *
 ---------------------------------------------------------------------------*/
void
HeathrowATA::free()
{
	
	super::free();


}


/*---------------------------------------------------------------------------
 *	get the work loop. If not already created, make one.
 *
 ---------------------------------------------------------------------------*/
IOWorkLoop*
HeathrowATA::getWorkLoop() const
{

	DLOG("HeathrowATA::getWorkLoop\n");

	IOWorkLoop* wl = _workLoop;

	if (!wl) 
	{
        wl = IOWorkLoop::workLoop();
           
        if (!wl)
            return 0;
    }        
            
 return   wl;        

}

/*---------------------------------------------------------------------------
 *
 *	Intercept the setup for the control register pointers so we can set the 
 *	timing register in the cell to some safe value prior to scanning for devices 
 *  start.
 *
 *
 ---------------------------------------------------------------------------*/
bool 
HeathrowATA::configureTFPointers(void)
{
    DLOG("HeathrowATA config TF Pointers \n");

	// call the superclass to setup the taskfile pointers

	if( ! super::configureTFPointers() )
	{
	
		return false;
	}

	// now that the address is setup, configure the controller to some safe 
	// default value as we are called during start in order to look for drives.

    DLOG("HeathrowATA setting default timing \n");
	
	OSWriteSwapInt32(_timingConfigReg, 0, busTimings[0].cycleRegValue );

   DLOG("HeathrowATA configTFPointers done\n");
	
	return true;

}

/*---------------------------------------------------------------------------
 *	provide information on the bus capability 
 *
 *
 ---------------------------------------------------------------------------*/
IOReturn 
HeathrowATA::provideBusInfo( IOATABusInfo* infoOut)
{
	if( infoOut == 0)
	{
		DLOG("HeathrowATA nil pointer in provideBusInfo\n");
		return -1;
	
	}


	infoOut->zeroData();
	
	if( isMediaBay )
	{
		infoOut->setSocketType( kMediaBaySocket ); 

	} else {

		infoOut->setSocketType( kInternalATASocket ); // internal fixed, media-bay, PC-Card
	}
	
	
	infoOut->setPIOModes( kATASupportedPIOModes);		
	infoOut->setDMAModes( kATASupportedMultiDMAModes );	
	infoOut->setUltraModes( 0x00 );

	UInt8 units = 0;
	
	if(	_devInfo[0].type != kUnknownATADeviceType )
		units++;
		
	if(	_devInfo[1].type != kUnknownATADeviceType )
		units++;

	infoOut->setUnits( units);

	return kATANoErr;
}


/*---------------------------------------------------------------------------
 *	return the currently configured timing for the unit number 
 *
 *
 ---------------------------------------------------------------------------*/

IOReturn 
HeathrowATA::getConfig( IOATADevConfig* configRequest, UInt32 unitNumber)
{

	// param check the pointers and the unit number.
	
	if( configRequest == 0
		|| unitNumber > 1 )
		
	{
	
		DLOG("HeathrowATA bad param in getConfig\n");
		return -1;	
	}


	// no ultra on heathrow
	configRequest->setUltraMode(0x00);

	// grab the info from our internal data.
	configRequest->setPIOMode( busTimings[unitNumber].ataPIOSpeedMode);
	configRequest->setDMAMode(busTimings[unitNumber].ataMultiDMASpeed);
	configRequest->setPIOCycleTime(busTimings[unitNumber].ataPIOCycleTime );
	configRequest->setDMACycleTime(busTimings[unitNumber].ataMultiCycleTime);
	configRequest->setPacketConfig( _devInfo[unitNumber].packetSend );

	return kATANoErr;

}

/*---------------------------------------------------------------------------
 *	setup the timing configuration for a device 
 *
 *
 ---------------------------------------------------------------------------*/

IOReturn 
HeathrowATA::selectConfig( IOATADevConfig* configRequest, UInt32 unitNumber)
{


	// param check the pointers and the unit number.
	
	if( configRequest == 0
		|| unitNumber > 1 )
		
	{
	
		DLOG("HeathrowATA bad param in setConfig\n");
		return -1;	
	}


	// no ultra on heathrow
	if( configRequest->getUltraMode() )
	{
		DLOG("HeathrowATA setConfig no ultra\n");
		return kATAModeNotSupported;	
		
	}

	// all config requests must include a PIO mode
	if( ( configRequest->getPIOMode() & kATASupportedPIOModes ) == 0x00 )
	{
		DLOG("HeathrowATA setConfig PIO mode not supported\n");
		return kATAModeNotSupported;		
	
	}


	// DMA is optional - not all devices support it, we reality check for a setting
	// that is beyond our capability
	if( configRequest->getDMAMode() & ~kATASupportedMultiDMAModes )
	{
		DLOG("HeathrowATA setConfig DMA mode not supported\n");
		return kATAModeNotSupported;		
	
	} 

	_devInfo[unitNumber].packetSend = configRequest->getPacketConfig();
	DLOG("HeathrowATA setConfig packetConfig = %ld\n", _devInfo[unitNumber].packetSend );

	
	return selectIOTimerValue(configRequest, unitNumber);

}


/*---------------------------------------------------------------------------
 *	calculate the timing configuration as requested.
 *
 *
 ---------------------------------------------------------------------------*/

IOReturn
HeathrowATA::selectIOTimerValue( IOATADevConfig* configRequest, UInt32 unitNumber)
{
// table of default cycle times to use when a device requests a mode
// by number, but doesn't include a cycle time.

static const UInt16 MinPIOCycle[kATAMaxPIOMode + 1] =
				{
					600, 				// Mode 0
					383, 				// Mode 1
					240, 				// Mode 2
					180, 				// Mode 3
					120					// Mode 4
				};


static const UInt16 MinMultiDMACycle[kATAMaxMultiDMAMode + 1] =
				{
					480,				// Mode 0
					150,				// Mode 1
					120					// Mode 2
				};


// table of PIO cycle times and the corresponding binary numbers
// that the config register needs to make that timing.
static const UInt32 PIOCycleValue[kHeathrowPIOCycleEntries] = 
					{
						0x00000400,					// Hardware maximum		(2070 ns = 'E' + 1110 rec + 960 acc)
						0x00000526,					// Minimum PIO mode 0 	(600 ns = 'E' + 420 rec + 180 acc)	 
						0x00000085,					// Minimum PIO mode 1	(390 ns = 240 rec + 150 acc)	 
						0x00000046,					// Derated PIO Mode 2/3/4	(360 ns = 180 rec + 180 acc)	 
						0x00000045,					// Derated PIO Mode 2/3/4	(330 ns = 180 rec + 150 acc)	 
						0x00000025,					// Derated PIO Mode 2/3/4	(300 ns = 150 rec + 150 acc)	 
						0x00000025,					// Derated PIO mode 2/3/4	(300 ns = 150 rec + 150 acc)	 
						0x00000025,					// Minimum PIO mode 2 		(300 ns = 150 rec + 150 acc)	 
						0x00000025,					// Derated PIO Mode 3/4 	(300 ns = 150 rec + 150 acc)		 
						0x00000025,					// Minimum PIO mode 3		(300 ns = 150 rec + 150 acc)	 
						0x00000025					// Minimum PIO mode 4		(300 ns = 150 rec + 150 acc)	 
 					};


static const UInt16 PIOCycleTime[ kHeathrowPIOCycleEntries ]=
					{
						2070,						// Hardware maximum			(0)
						600,						// Minimum PIO mode 0 		(1)	 
						383,						// Minimum PIO mode 1		(2)	 
						360,						// Derated PIO Mode 2/3/4	(3) 	 
						330,						// Derated PIO Mode 2/3/4	(4) 	 
						300,						// Derated PIO Mode 2/3/4	(5) 	 
						270,						// Derated PIO mode 2/3/4	(6)	 
						240,						// Minimum PIO mode 2 		(7)	 
						239,						// Derated PIO Mode 3/4 	(8)		 
						180,						// Minimum PIO mode 3		(9)	 
						120,						// Minimum PIO mode 4		(10)	 
					};


// table of DMA cycle times and the corresponding binary value to reach that number.
static const UInt32 MultiDMACycleValue[kHeathrowMultiDMACycleEntries] =
				{
					0x00000000, 				// Hardware maximum     (1950=  990rec+960acc)	 
					0x00074000, 				// Minimum Multi mode 0  (480=  240rec+240acc)	 
					0x00053000, 				// Derated mode 1 or 2   (360=  180rec+180acc)	 
					0x00242000, 				// Derated mode 1 or 2   (270=H+135rec+135acc)	 
					0x00032000, 				// Derated mode 1 or 2   (240=  120rec+120acc)	 
					0x00231800, 				// Derated mode 1 or 2   (210=H+105rec+105acc)	 
					0x00021800, 				// Derated mode 1 or 2   (180=   90rec+90acc)	 
					0x00221000, 				// Minimum Multi mode 1  (150=H+75rec+75acc)	 
					0x00211000	 				// Minimum Multi mode 2  (120=H+45rec+75acc)(rd) (150=H+75rec+75acc)(wr)
				}; 

static const UInt16 MultiDMACycleTime[kHeathrowMultiDMACycleEntries] =
				{
					1950, 						// Hardware maximum     	(0) 
					480, 						// Minimum Multi mode 0  (1)	 
					360, 						// Derated mode 1 or 2   (2)	 
					270, 						// Derated mode 1 or 2   (3)	 
					240, 						// Derated mode 1 or 2   (4)	 
					210, 						// Derated mode 1 or 2   (5)	 
					180, 						// Derated mode 1 or 2   (6)	 
					150, 						// Minimum Multi mode 1  (7)	 
					120 						// Minimum Multi mode 2  (8)	 
				};


	UInt32 pioConfigBits = PIOCycleValue[0];

	// the theory is simple, just examine the requested mode and cycle time, find the 
	// entry in the table which is closest, but NOT faster than the requested cycle time.

	// convert the bit maps into numbers
	UInt32 pioModeNumber = bitSigToNumeric( configRequest->getPIOMode());

	// check for out of range values.
	if( pioModeNumber > kATAMaxPIOMode )
	{
		DLOG("HeathrowATA pio mode out of range\n");
		return kATAModeNotSupported;	
	}
	
	// use a default cycle time if the device didn't report a time
	// to use.
	UInt32 pioCycleTime = configRequest->getPIOCycleTime();
	if( pioCycleTime < MinPIOCycle[ pioModeNumber ] )
	{
		pioCycleTime = MinPIOCycle[ pioModeNumber ];
	}


	// loop until a the hardware cycle time is longer than the 
	// or equal to the requested cycle time
	for( int i = kHeathrowPIOCycleEntries - 1; i >= 0; i--)
	{
	
		if( pioCycleTime <= PIOCycleTime[ i ] )
		{
			// found the fastest time which is not greater than
			// the requested time.
			pioConfigBits = PIOCycleValue[i];
			break;
		}
	}
	

	// now do the same for DMA modes.
	UInt32 dmaConfigBits = MultiDMACycleValue[0];
	UInt32 dmaModeNumber = bitSigToNumeric( configRequest->getDMAMode() );

	// if the device requested no DMA mode, then just set the timer to mode 0
	if( dmaModeNumber > kATAMaxMultiDMAMode )
	{
		dmaModeNumber = 0;	
	}

	UInt32 dmaCycleTime = configRequest->getDMACycleTime();
	if( dmaCycleTime < MinMultiDMACycle[ dmaModeNumber ] )
	{
		dmaCycleTime = MinMultiDMACycle[ dmaModeNumber ];
	}


	// loop until a the hardware cycle time is longer than the 
	// or equal to the requested cycle time
	for( int i = kHeathrowMultiDMACycleEntries - 1; i >= 0; i--)
	{
	
		if( dmaCycleTime <= MultiDMACycleTime[ i ] )
		{
			// found the fastest time which is not greater than
			// the requested time.
			dmaConfigBits = MultiDMACycleValue[i];
			break;
		}
	}


	// now combine the bits forming the configuration and put them
	// into the timing structure along with the mode and cycle time
	
	busTimings[unitNumber].cycleRegValue = dmaConfigBits | pioConfigBits;
	busTimings[unitNumber].ataPIOSpeedMode = configRequest->getPIOMode();
	busTimings[unitNumber].ataPIOCycleTime = pioCycleTime;
	busTimings[unitNumber].ataMultiDMASpeed = configRequest->getDMAMode();
	busTimings[unitNumber].ataMultiCycleTime = dmaCycleTime;

	DLOG("HeathrowATA PIO mode %x at %ld ns selected for device: %x\n", (int)pioModeNumber, pioCycleTime, (int)unitNumber);
	DLOG("HeathrowATA DMA mode %x at %ld ns selected for device: %x\n", (int)dmaModeNumber, dmaCycleTime, (int)unitNumber);

	// stuff the values back into the request structure and return result
	return getConfig( configRequest, unitNumber);

}



// punch the correct value into the IO timer register for the selected unit.
void 
HeathrowATA::selectIOTiming( ataUnitID unit )
{

	DLOG("heathrowATA setting timing config %lx on unit %d\n", busTimings[unit].cycleRegValue, unit);

	OSWriteSwapInt32(_timingConfigReg, 0, busTimings[unit].cycleRegValue );



}




/*-----------------------------------------------------------------------------
	Override to check for drives that haven't been reset prior to kernel 
	takeover.

-----------------------------------------------------------------------------*/
UInt32 
HeathrowATA::scanForDrives( void )
{

	IOReturn err = kATANoErr;
	
	err = softResetBus( false );

	return super::scanForDrives();

}


IOReturn
HeathrowATA::synchronousIO(void)
{

	_devIntSrc->disable();

	IOReturn result = super::synchronousIO();

	_devIntSrc->enable();
	
	return result;

}


/*---------------------------------------------------------------------------
 *
 ---------------------------------------------------------------------------*/
IOReturn
HeathrowATA::handleDeviceInterrupt(void)
{

	
	if( _dmaIntExpected != true )
	{
		volatile UInt8 status = *_tfStatusCmdReg;
		OSSynchronizeIO();
		status++; // prevent compiler removal of unused variable.
	}
	return super::handleDeviceInterrupt();
	
}



IOReturn
HeathrowATA::selectDevice( ataUnitID unit )
{
	_devIntSrc->disable();
	IOReturn result = super::selectDevice( unit);
	_devIntSrc->enable();
	
	return result;


}
