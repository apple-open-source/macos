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
 *	KeyLargoATA.cpp
 *	
	Defines the concrete driver for a keylargo-ata controller.
	Descendent of MacIOATA, which is derived from IOATAController.

 *
 *
 */
 
#include "KeyLargoATA.h"
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

#define kCompatibleString "keylargo-ata"
#define kModelPropertyKey "model"
#define kCableTypeKey "cable-type"
#define kModel4String "ata-4"
#define k80ConductorString "80-conductor"


			/*  Bit-significant representation of supported modes.*/ 
#define kATASupportedPIOModes		 0x001F	// modes 4, 3, 2, 1, and 0
#define kATASupportedMultiDMAModes	 0x0007	// modes 2, 1, and 0
#define	kATASupportedUltraDMAModes	 0x001f	// modes 4, 3, 2, 1, and 0

/* Number of entries in various tables */
#define kKeyLargoPIOCycleEntries			11
#define kKeyLargoMultiDMACycleEntries 		9
#define	kKeyLargoUltraDMACycleEntries		5


// ------ ¥¥¥ Maximum transfer modes the KeyLargo controller is capable of supporting ¥¥¥ ÐÐÐÐÐÐÐÐ
#define kATAMaxPIOMode		 4
#define kATAMaxMultiDMAMode	2
#define	kATAMaxUltraDMAMode 4



#pragma mark -IOService Overrides -


//---------------------------------------------------------------------------

#define super MacIOATA

OSDefineMetaClassAndStructors(  KeyLargoATA, MacIOATA )

//---------------------------------------------------------------------------

bool 
KeyLargoATA::init(OSDictionary* properties)
{
	DLOG("KeyLargoATA init start\n");


    // Initialize instance variables.
   
    if (super::init(properties) == false)
    {
        DLOG("KeyLargoATA: super::init() failed\n");
        return false;
    }
	
	isUltraCell = false;
	cableIs80Conductor = false;
	_needsResync = false;
	
	DLOG("KeyLargoATA init done\n");

    return true;
}

/*---------------------------------------------------------------------------
 *
 *	Check and see if we really match this device.
 * override to accept or reject close matches based on further information
 ---------------------------------------------------------------------------*/

IOService* 
KeyLargoATA::probe(IOService* provider,	SInt32*	score)
{

    OSData		*compatibleEntry;
	
	DLOG("KeyLargoATA starting probe\n");


	compatibleEntry  = OSDynamicCast( OSData, provider->getProperty( "compatible" ) );
	if ( compatibleEntry == 0 ) 
	{
		// error unknown controller type.

		DLOG("KeyLargoATA failed getting compatible property\n");
		return 0;

	}

	
	// test the compatible property for a match to the controller type name.
	if ( compatibleEntry->isEqualTo( kCompatibleString, sizeof(kCompatibleString)-1 ) == false ) 
	{
		// not our type of controller		
		DLOG("KeyLargoATA compatible property doesn't match\n");
		return 0;
		
	}


	// do a little initialization here once the probe is succesful so we can start clean.

	OSData		*registryEntry;
	registryEntry = OSDynamicCast( OSData, provider->getProperty( kModelPropertyKey ) );
	if( registryEntry == 0)
	{
		DLOG("KeyLargoATA unknown model property.\n");
		return 0;
	}

	isUltraCell = registryEntry->isEqualTo( kModel4String, sizeof(kModel4String)-1 ); 

	if( isUltraCell )
	{
		registryEntry = OSDynamicCast( OSData, provider->getProperty( kCableTypeKey ) );
		if( registryEntry == 0)
		{
			DLOG("KeyLargoATA unknown cable-type.\n");
			return 0;
		}

	
		cableIs80Conductor = registryEntry->isEqualTo( k80ConductorString, sizeof(k80ConductorString)-1 );
	}
	
	// initialize the timing params to PIO mode 0 at 600ns and MWDMA mode 0 at 480ns
	if( isUltraCell )
	{
		busTimings[0].cycleRegValue = busTimings[1].cycleRegValue = 0x0000038C | 0x00084000;
	
	} else {
	
		busTimings[0].cycleRegValue = busTimings[1].cycleRegValue = 0x00000526 | 0x00084000;
	
	}
	busTimings[0].ataPIOSpeedMode = busTimings[1].ataPIOSpeedMode = 0x01 ;				// PIO Mode Timing class (bit-significant)
	busTimings[0].ataPIOCycleTime = busTimings[1].ataPIOCycleTime = 600 ;				// Cycle time for PIO mode
	busTimings[0].ataMultiDMASpeed = busTimings[1].ataMultiDMASpeed = 0x01;				// Multiple Word DMA Timing Class (bit-significant)
	busTimings[0].ataMultiCycleTime = busTimings[1].ataMultiCycleTime = 480;				// Cycle time for Multiword DMA mode
	busTimings[0].ataUltraDMASpeedMode = busTimings[1].ataUltraDMASpeedMode = 0x00;				// UltraDMA Timing Class (bit-significant)





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
KeyLargoATA::start(IOService *provider)
{

    DLOG("KeyLargoATA::start() begin\n");
    
    if( ! OSDynamicCast(AppleMacIODevice, provider ) )
    {
    
    	DLOG("KeyLargoATA provider not AppleMacIODevice!\n");
    	return false;    
    
    }

	if( ! provider->open(this) )
	{
		DLOG("KeyLargo provider did not open\n");
		return false;	
	}

	ATADeviceNub* newNub=0L;
 
 	// call start on the superclass
    if (!super::start( provider))
 	{
        DLOG("KeyLargoATA: super::start() failed\n");
        provider->close(this);
        return false;
	}

    DLOG("KeyLargoATA::start() done\n");

	for( UInt32 i = 0; i < 2; i++)
	{
		if( _devInfo[i].type != kUnknownATADeviceType )
		{
		
			DLOG("KeyLargoATA creating nub\n");
			newNub = ATADeviceNub::ataDeviceNub( (IOATAController*)this, (ataUnitID) i, _devInfo[i].type );
		
			if( newNub )
			{
				
				DLOG("KeyLargoATA attach nub\n");
				
				newNub->attach(this);
			
				_nub[i] = (IOATADevice*) newNub;
				
				DLOG("KeyLargoATA register nub\n");

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
KeyLargoATA::free()
{
	
	super::free();


}


/*---------------------------------------------------------------------------
 *	get the work loop. If not already created, make one.
 *
 ---------------------------------------------------------------------------*/
IOWorkLoop*
KeyLargoATA::getWorkLoop() const
{

	DLOG("KeyLargoATA::getWorkLoop\n");

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
KeyLargoATA::configureTFPointers(void)
{
    DLOG("KeyLargoATA config TF Pointers \n");

	// call the superclass to setup the taskfile pointers

	if( ! super::configureTFPointers() )
	{
	
		return false;
	}	

	// now that the address is setup, configure the controller to some safe 
	// default value as we are called during start in order to look for drives.

    DLOG("KeyLargoATA setting default timing \n");
	
	OSWriteSwapInt32(_timingConfigReg, 0, busTimings[0].cycleRegValue );

   DLOG("KeyLargoATA configTFPointers done\n");
	
	return true;

}

/*---------------------------------------------------------------------------
 *	provide information on the bus capability 
 *
 *
 ---------------------------------------------------------------------------*/
IOReturn 
KeyLargoATA::provideBusInfo( IOATABusInfo* infoOut)
{
	if( infoOut == 0)
	{
		DLOG("KeyLargoATA nil pointer in provideBusInfo\n");
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
	if( isUltraCell )
	{
		if( cableIs80Conductor )
		{
			// this bus is equipped with cabling for speeds faster than 30ns ( > ATA/33 )
		
			infoOut->setUltraModes( kATASupportedUltraDMAModes );
		
		}else{
		
			// this bus is equipped with cabling for ATA/33
			infoOut->setUltraModes( 0x0007 );  // modes 2, 1, 0 udma only.
		}
	
	} else {
		
		// non-ultra capable cell
		infoOut->setUltraModes( 0x00 );
	
	}
	
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
KeyLargoATA::getConfig( IOATADevConfig* configRequest, UInt32 unitNumber)
{

	// param check the pointers and the unit number.
	
	if( configRequest == 0
		|| unitNumber > 1 )
		
	{
	
		DLOG("KeyLargoATA bad param in getConfig\n");
		return -1;	
	}




	// grab the info from our internal data.
	configRequest->setPIOMode( busTimings[unitNumber].ataPIOSpeedMode);
	configRequest->setDMAMode(busTimings[unitNumber].ataMultiDMASpeed);
	configRequest->setPIOCycleTime(busTimings[unitNumber].ataPIOCycleTime );
	configRequest->setDMACycleTime(busTimings[unitNumber].ataMultiCycleTime);
	configRequest->setPacketConfig( _devInfo[unitNumber].packetSend );
	configRequest->setUltraMode(busTimings[unitNumber].ataUltraDMASpeedMode);

	return kATANoErr;

}

/*---------------------------------------------------------------------------
 *	setup the timing configuration for a device 
 *
 *
 ---------------------------------------------------------------------------*/

IOReturn 
KeyLargoATA::selectConfig( IOATADevConfig* configRequest, UInt32 unitNumber)
{


	// param check the pointers and the unit number.
	
	if( configRequest == 0
		|| unitNumber > 1 )
		
	{
	
		DLOG("KeyLargoATA bad param in setConfig\n");
		return -1;	
	}



	// all config requests must include a PIO mode
	if( ( configRequest->getPIOMode() & kATASupportedPIOModes ) == 0x00 )
	{
		DLOG("KeyLargoATA setConfig PIO mode not supported\n");
		return kATAModeNotSupported;		
	
	}


	// DMA is optional - not all devices support it, we reality check for a setting
	// that is beyond our capability


	// no ultra on unless this is the ultra cell
	UInt16 ultraSupported = 0x0000;
	
	if( isUltraCell )
	{
		ultraSupported = cableIs80Conductor ? kATASupportedUltraDMAModes : 0x0007;	
	}

	// make sure a requested ultra ATA mode is within the range of this device configuration

	if( configRequest->getUltraMode() & ~ultraSupported )
	{
		DLOG("KeyLargoATA setConfig no ultra\n");
		return kATAModeNotSupported;	
		
	} 



	if( configRequest->getDMAMode() & ~kATASupportedMultiDMAModes )
	{
		DLOG("KeyLargoATA setConfig DMA mode not supported\n");
		return kATAModeNotSupported;		
	
	} 

	if( configRequest->getDMAMode() > 0x0000
		&& configRequest->getUltraMode() > 0x0000 )
	{
		DLOG("KeylargoATA err, only one DMA class allowed in config select\n");
		return kATAModeNotSupported;
	
	}

	_devInfo[unitNumber].packetSend = configRequest->getPacketConfig();
	DLOG("KeyLargoATA setConfig packetConfig = %ld\n", _devInfo[unitNumber].packetSend );

	
	return selectIOTimerValue(configRequest, unitNumber);

}


/*---------------------------------------------------------------------------
 *	calculate the timing configuration as requested.
 *
 *
 ---------------------------------------------------------------------------*/

IOReturn
KeyLargoATA::selectIOTimerValue( IOATADevConfig* configRequest, UInt32 unitNumber)
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
static const UInt32 PIOCycleValue33[kKeyLargoPIOCycleEntries] = 
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


// register definition for 66mhz cells  15ns clock
//[9:5]		pioRecovery
//[4:0]		pioAccess


UInt32 PIOCycleValue66[kKeyLargoPIOCycleEntries] = 
					{
						0x000003FF,					// Hardware minimum		(930 ns = 'E' + 465 rec + 465 acc) 	31, 31
						0x0000038C,					// Minimum PIO mode 0 	(600 ns = 'E' + 420 rec + 180 acc)	28, 12  28 * 32 + 12 = 908
						0x0000020A,					// Minimum PIO mode 1	(390 ns = 240 rec + 150 acc)	  	16, 10  16 * 32 + 10 = 522
						0x0000018C,					// Derated PIO Mode 2/3/4	(360 ns = 180 rec + 180 acc)	12, 12  12 * 32 + 12 = 396
						0x0000018A,					// Derated PIO Mode 2/3/4	(330 ns = 180 rec + 150 acc)    12, 10  12 * 32 + 10 = 394
						0x00000156,					// Derated PIO Mode 2/3/4	(300 ns = 150 rec + 150 acc)	10, 10  10 * 32 + 10 = 342
						0x00000148,					// Derated PIO mode 2/3/4	(270 ns = 150 rec + 120 acc)	10,  8  10 * 32 + 8  = 328
						0x00000127,					// Minimum PIO mode 2 		(240 ns = 135 rec + 105 acc)	 9,  7   9 * 32 + 7  = 295
						0x00000108,					// Derated PIO Mode 3/4 	(240 ns = 120 rec + 120 acc)	 8,  8	 8 * 32 + 8  = 264
						0x000000C6,					// Minimum PIO mode 3		(180 ns =  90 rec + 90 acc)	   	 6,  6   6 * 32 + 6  = 198
						0x00000065					// Minimum PIO mode 4		(120 ns =  45 rec + 75 acc)	 	 3,  5   3 * 32 + 5  = 101 
 					};



static const UInt16 PIOCycleTime[ kKeyLargoPIOCycleEntries ]=
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


// register definition for 33mhz cells  30ns clock
// [10] pio RW delay
// [15:11] dmaAccess  
// [20:16] dmaRecovery
// table of DMA cycle times and the corresponding binary value to reach that number.
static const UInt32 MultiDMACycleValue33[kKeyLargoMultiDMACycleEntries] =
				{
					0x00000000, 				// Hardware maximum     (1950=  990rec+960acc)	 
					0x00084000, 				// Minimum Multi mode 0  (480=  240rec+240acc)	8 + 8 
					0x00063000, 				// Derated mode 1 or 2   (360=  180rec+180acc)	6 + 6
					0x00052800, 				// Derated mode 1 or 2   (300= 150rec+150acc)	5 + 5 
					0x00042000, 				// Derated mode 1 or 2   (240=  120rec+120acc)	4 + 4 
					0x00041800, 				// Derated mode 1 or 2   (210= 120rec+90acc)	4 + 3
					0x00031800, 				// Derated mode 1 or 2   (180=   90rec+90acc)	3 + 3
					0x00021800, 				// Minimum Multi mode 1  (150= 60rec+90acc)	    2 + 3
					0x00011800	 				// Minimum Multi mode 2  (120= 30rec+90acc)     1 + 3
				}; 


// key largo register definition:  15 ns clock
//[19:15]		dmaRecovery      * 32768
//[14:10]		dmaAccess        * 1024
static const UInt32 MultiDMACycleValue66[kKeyLargoMultiDMACycleEntries] =
				{
					0x000FFC00, 				// Hardware maximum     ( 930=  465rec+465acc)	31, 31 
					0x00084000, 				// Minimum Multi mode 0  (480=  240rec+240acc)	16, 16
					0x00063000, 				// Derated mode 1 or 2   (360=  180rec+180acc)	12, 12
					0x0004A400, 				// Derated mode 1 or 2   (270=H+135rec+135acc)	 9, 9 
					0x00042000, 				// Derated mode 1 or 2   (240=  120rec+120acc)	 8, 8
					0x00039C00, 				// Derated mode 1 or 2   (210=H+105rec+105acc)	 7, 7
					0x00031800, 				// Derated mode 1 or 2   (180=   90rec+90acc)	 6, 6
					0x00029800, 				// Minimum Multi mode 1  (165= H+75rec+90acc)	 5, 6
					0x00019400	 				// Minimum Multi mode 2  (120= H+45rec+75acc) 	 3, 5
				}; 


static const UInt16 MultiDMACycleTime[kKeyLargoMultiDMACycleEntries] =
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


// There's no ultra DMA on the 33 mhz cells in Key largo, only on the 66 mhz cell.

				
static const UInt16 AllowUltraDMACycle[kATAMaxUltraDMAMode + 1] =
				{
					120,						// mode 0
					90,							// mode 1
					60,							// mode 2
					45,							// mode 3
					30							// mode 4
				};



// for Ultra cell with 15ns clock
//[28:25] 	ReadyToPause  * 33554432
//[24:21]	WrDataSetup   * 2097152
//[20]		UltraMode     + 1048576

static const UInt32 UltraDMACycleValue66[kKeyLargoUltraDMACycleEntries] =
				{
					0x19100000,					// rdy2paus=12, wrDataSetup=8, ultra mode on
					0x14D00000,					// rdy2paus=10, wrDataSetup=6, ultra on
					0x10900000,					// rdy2paus=8, wrDataSetup=4, ultra on
					0x0C700000,					// rdy2paus=6, wrDataSetup=3, ultra on    mode 3
					0x0C500000					// rdy2paus=6, wrDataSetup=2, ultra on    mode 4
				};
				
static const UInt16 UltraDMACycleTime[kKeyLargoUltraDMACycleEntries] =
				{
					120,						// mode 0
					90,							// mode 1
					60,							// mode 2
					45,							// mode 3
					30							// mode 4
				};
				
/*-----------------------------------------*/






	UInt32 pioConfigBits = isUltraCell? PIOCycleValue66[0] : PIOCycleValue33[0];

	// the theory is simple, just examine the requested mode and cycle time, find the 
	// entry in the table which is closest, but NOT faster than the requested cycle time.

	// convert the bit maps into numbers
	UInt32 pioModeNumber = bitSigToNumeric( configRequest->getPIOMode());

	// check for out of range values.
	if( pioModeNumber > kATAMaxPIOMode )
	{
		DLOG("KeyLargoATA pio mode out of range\n");
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
	for( int i = kKeyLargoPIOCycleEntries - 1; i >= 0; i--)
	{
	
		if( pioCycleTime <= PIOCycleTime[ i ] )
		{
			// found the fastest time which is not greater than
			// the requested time.
			pioConfigBits = isUltraCell? PIOCycleValue66[i] : PIOCycleValue33[i];
			break;
		}
	}
	

	// now do the same for DMA modes.
	UInt32 dmaConfigBits = isUltraCell? MultiDMACycleValue66[0] : MultiDMACycleValue33[0];
	UInt32 dmaModeNumber = 0;
	UInt32 dmaCycleTime = 0;
	
	if( configRequest->getDMAMode() )
	{
		dmaModeNumber = bitSigToNumeric( configRequest->getDMAMode() );

		// if the device requested no DMA mode, then just set the timer to mode 0
		if( dmaModeNumber > kATAMaxMultiDMAMode )
		{
			dmaModeNumber = 0;	
		}

		dmaCycleTime = configRequest->getDMACycleTime();
		if( dmaCycleTime < MinMultiDMACycle[ dmaModeNumber ] )
		{
			dmaCycleTime = MinMultiDMACycle[ dmaModeNumber ];
		}


		// loop until a the hardware cycle time is longer than the 
		// or equal to the requested cycle time
		for( int i = kKeyLargoMultiDMACycleEntries - 1; i >= 0; i--)
		{
		
			if( dmaCycleTime <= MultiDMACycleTime[ i ] )
			{
				// found the fastest time which is not greater than
				// the requested time.
				dmaConfigBits = isUltraCell? MultiDMACycleValue66[i] : MultiDMACycleValue33[i];
				break;
			}
		}
	}

	UInt32 ultraModeNumber = 0;
	// same deal for ultra ATA modes
	if( configRequest->getUltraMode() )
	{
		ultraModeNumber = bitSigToNumeric( configRequest->getUltraMode() );
	
		// in the event someone asks for more than we can do, go as fast as possible given our cable condition.
		if( ultraModeNumber > kATAMaxUltraDMAMode )
		{
			ultraModeNumber = cableIs80Conductor ? kATAMaxUltraDMAMode : 2;		
		}
	
		dmaConfigBits = UltraDMACycleValue66[ ultraModeNumber ];
	
	
	}


	if( isUltraCell )
	{
		// addressSetup = 3 * 15ns = 45  bits 31-29 =  011
		// RDYtoPause = 4 * 15ns = 60ns  bits 28-25 =     0100
		// DIOW setup = 2 * 15ns = 30ns  bits 24-21           0010
		// 0xA8800000
		//dmaConfigBits |= 0x68400000;
	
	}


	// now combine the bits forming the configuration and put them
	// into the timing structure along with the mode and cycle time
	
	busTimings[unitNumber].cycleRegValue = dmaConfigBits | pioConfigBits;
	busTimings[unitNumber].ataPIOSpeedMode = configRequest->getPIOMode();
	busTimings[unitNumber].ataPIOCycleTime = pioCycleTime;
	busTimings[unitNumber].ataMultiDMASpeed = configRequest->getDMAMode();
	busTimings[unitNumber].ataMultiCycleTime = dmaCycleTime;
	busTimings[unitNumber].ataUltraDMASpeedMode = configRequest->getUltraMode();

	DLOG("KeyLargoATA PIO mode %x at %ld ns selected for device: %x\n", (int)pioModeNumber, pioCycleTime, (int)unitNumber);
	DLOG("KeyLargoATA DMA mode %x at %ld ns selected for device: %x\n", (int)dmaModeNumber, dmaCycleTime, (int)unitNumber);
	DLOG("KeyLargoATA Ultra mode %x at %ld ns selected for device: %x\n", (int)ultraModeNumber, UltraDMACycleTime[ultraModeNumber], (int)unitNumber);
	DLOG("KeyLargo cycle value = %x for unit: %x\n", 	busTimings[unitNumber].cycleRegValue, unitNumber);


	// stuff the values back into the request structure and return result
	return getConfig( configRequest, unitNumber);

}



// punch the correct value into the IO timer register for the selected unit.
void 
KeyLargoATA::selectIOTiming( ataUnitID unit )
{

	//DLOG("keylargoATA setting timing config %lx on unit %d\n", busTimings[unit].cycleRegValue, unit);

	if( isUltraCell == true
		&& ( ( _currentCommand->getFlags() & (mATAFlagUseDMA | mATAFlagIORead) ) == (mATAFlagUseDMA | mATAFlagIORead))
		&& (( busTimings[unit].cycleRegValue & 0x00100000 ) == 0x00100000 ) )
	{
		OSWriteSwapInt32(_timingConfigReg, 0, busTimings[unit].cycleRegValue + 0x00800000 );

	} else {
		
		OSWriteSwapInt32(_timingConfigReg, 0, busTimings[unit].cycleRegValue );

	}


}



/*---------------------------------------------------------------------------
 *
 ---------------------------------------------------------------------------*/
IOReturn
KeyLargoATA::handleDeviceInterrupt(void)
{

	if( !_currentCommand )
	{
		volatile UInt8 status = *_tfStatusCmdReg;
		OSSynchronizeIO();
		status++; // prevent compiler removal of unused variable.
		return 0;
	}



	UInt32 intStatus = OSReadLittleInt32( (void*) _tfDataReg, 0x300);
	OSSynchronizeIO();
	
	// make sure the interrupt is still actually asserted.
	// and we aren't being called from processDMAInterrupt()
	if( !(intStatus & 0x40000000)
		&& !_needsResync )	
	{
		volatile UInt8 status = *_tfStatusCmdReg;		
		OSSynchronizeIO();	
		status++;
		return 0;
	}
	
	return super::handleDeviceInterrupt();
	
}



IOReturn
KeyLargoATA::selectDevice( ataUnitID unit )
{
	_devIntSrc->disable();
	IOReturn result = super::selectDevice( unit);
	_devIntSrc->enable();
	
	return result;


}


IOReturn
KeyLargoATA::synchronousIO(void)
{

	_devIntSrc->disable();

	IOReturn result = super::synchronousIO();
	
	// read the status register in case the drive didn't clear int-pending status already.

	UInt32 intStatus = OSReadLittleInt32( (void*) _tfDataReg, 0x300);
	OSSynchronizeIO();
	
	// loop until the drive release the interrupt line.
	while( (intStatus & 0x40000000) )	
	{
		volatile UInt8 status = *_tfStatusCmdReg;		
		OSSynchronizeIO();	
		status++;
		intStatus = OSReadLittleInt32( (void*) _tfDataReg, 0x300);
		OSSynchronizeIO();	
	}
	

	_devIntSrc->enable();
	
	return result;

}




IOReturn
KeyLargoATA::handleBusReset(void)
{

	bool		isATAPIReset = ((_currentCommand->getFlags() & mATAFlagProtocolATAPI) != 0);
	bool		doATAPI[2];
	IOReturn	err = kATANoErr;
	UInt8		index;
	UInt8 		statCheck;
	
	DLOG("IOATA bus reset start.\n");

	doATAPI[0] = doATAPI[1] = false;		

	// If this is an ATAPI reset select just the corresponding atapi device (instead of both) 
	if(isATAPIReset)
	{
		doATAPI[_currentCommand->getUnit()] = true;  // Mark only selected ATAPI as reset victim.
		
	}else if (!isUltraCell) {
	
		doATAPI[0] = doATAPI[1] = true; // In non ultra ATA case, mark both as candidates for reset commands prior to a bus reset.
		
	}											
	
	
	// Issue the needed ATAPI reset commands	
	for(index=0;index<2;index++)
	{
		if( doATAPI[index] && _devInfo[index].type == kATAPIDeviceType)
		{			
			OSSynchronizeIO();		
			*_tfSDHReg = mATASectorSize + (index << 4);

			// read the alt status and disreguard to provide 400ns delay
			OSSynchronizeIO();		
			statCheck = *_tfAltSDevCReg;  

			err = softResetBus(true);			
		}
		
	}
	
	
	// once the ATAPI device has been reset, contact the device driver
	if(isATAPIReset)
	{			
		executeEventCallouts( kATAPIResetEvent, _currentCommand->getUnit() );		
	}		
	

	// Handle the ATA reset case
	if(!isATAPIReset)
	{	
		err = softResetBus(); 	
		executeEventCallouts( kATAResetEvent, kATAInvalidDeviceID );	
	}
	
	_currentCommand->state = IOATAController::kATAComplete;

	DLOG("IOATA bus reset done.\n");


	completeIO( err );

	
	return err;

}

IOATAController::transState	
KeyLargoATA::determineATAPIState(void)
{
	IOATAController::transState			drivePhase = super::determineATAPIState();
	if(  _currentCommand->state > drivePhase
		|| _currentCommand->state == kATAStarted )
	{
		return (IOATAController::transState) _currentCommand->state;
	}

	return drivePhase;
}

void
KeyLargoATA::processDMAInterrupt(void)
{
	_needsResync = _resyncInterrupts;
	super::processDMAInterrupt();
	_needsResync = false;
}

