/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
 *
 *	AppleKauaiATA.cpp
 *	
	Defines the concrete driver for a AppleKauai-ata controller.
	Descendent of MacIOATA, which is derived from IOATAController.

 *
 *
 */
 
#include <libkern/OSByteOrder.h>
#include <libkern/OSAtomic.h>

#include <IOKit/assert.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOTypes.h>
#include <IOKit/IOKitKeys.h>

#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/ata/IOATATypes.h>
#include <IOKit/ata/IOATAController.h>
#include <IOKit/ata/IOATACommand.h>
#include <IOKit/ata/IOATADevice.h>
#include <IOKit/ata/IOATABusInfo.h>
#include <IOKit/ata/IOATADevConfig.h>

#include "AppleKauaiATA.h"
#include <IOKit/ata/ATADeviceNub.h>

 

#ifdef DLOG
#undef DLOG
#endif

//uncomment to enable IOLog tracing
//#define ATA_DEBUG 1

#ifdef  ATA_DEBUG
#define DLOG(fmt, args...)  IOLog(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

// some day, we'll have an ATA recorder for IOKit

#define ATARecordEventMACRO(type,param,bus,data) 		(void) (type); (void) (param); (void) (bus); (void) (data)

#define kCompatibleString "kauai-ata"
#define kCompatibleString2 "K2-UATA"
#define kModelPropertyKey "model"
#define kCableTypeKey "cable-type"
#define kModel6String "ata-6"
#define k80ConductorString "80-conductor"


			/*  Bit-significant representation of supported modes.*/ 
#define kATASupportedPIOModes		 0x001F	// modes 4, 3, 2, 1, and 0
#define kATASupportedMultiDMAModes	 0x0007	// modes 2, 1, and 0
#define	kATASupportedUltraDMAModes	 0x003f	// modes 5, 4, 3, 2, 1, and 0

/* Number of entries in various tables */
#define kAppleKauaiPIOCycleEntries			11
#define kAppleKauaiMultiDMACycleEntries 		9
#define	kAppleKauaiUltraDMACycleEntries		6


// ------ ¥¥¥ Maximum transfer modes the AppleKauai controller is capable of supporting ¥¥¥ ÐÐÐÐÐÐÐÐ
#define kATAMaxPIOMode		 4
#define kATAMaxMultiDMAMode	2
#define	kATAMaxUltraDMAMode 5

// 33 data descriptors + NOP + STOP
#define kATAXferDMADesc 33
#define kATAMaxDMADesc kATAXferDMADesc + 2
// up to 256 ATA sectors per transfer
#define kMaxATAXfer	512 * 256

#define kRX_bufferSize 512 * 256

#pragma mark -IOService Overrides -


//---------------------------------------------------------------------------

#define super MacIOATA

OSDefineMetaClassAndStructors(  AppleKauaiATA, MacIOATA )

//---------------------------------------------------------------------------

bool 
AppleKauaiATA::init(OSDictionary* properties)
{
	DLOG("AppleKauaiATA init start\n");


    // Initialize instance variables.
   
    if (super::init(properties) == false)
    {
        DLOG("AppleKauaiATA: super::init() failed\n");
        return false;
    }
	
	_needsResync = false;
	
	dmaBuffer = 0;
	clientBuffer = 0;
	bufferRX = false;
	rxFeatureOn = false;
	
	DLOG("AppleKauaiATA init done\n");

    return true;
}

/*---------------------------------------------------------------------------
 *
 *	Check and see if we really match this device.
 * override to accept or reject close matches based on further information
 ---------------------------------------------------------------------------*/

IOService* 
AppleKauaiATA::probe(IOService* provider,	SInt32*	score)
{

    OSData		*compatibleEntry;
	
	DLOG("AppleKauaiATA starting probe\n");


	compatibleEntry  = OSDynamicCast( OSData, provider->getProperty( "compatible" ) );
	if ( compatibleEntry == 0 ) 
	{
		// error unknown controller type.

		DLOG("AppleKauaiATA failed getting compatible property\n");
		return 0;

	}

	// test the compatible property for a match to the controller type name.
	if ( compatibleEntry->isEqualTo( kCompatibleString, sizeof(kCompatibleString)-1 ) == false )
	{
        
		
		if( compatibleEntry->isEqualTo( kCompatibleString2, sizeof(kCompatibleString2)-1 ) == false) 
		{
		// not our type of controller		
		DLOG("AppleKauaiATA compatible property doesn't match\n");
		return 0;
		}
	
		// turn on rxbuffer feature for this controller. 
		rxFeatureOn = true;		
		DLOG("AppleKauaiATA rx buffer feature enabled\n");
	}
	// do a little initialization here once the probe is succesful so we can start clean.

	OSData		*registryEntry;
	registryEntry = OSDynamicCast( OSData, provider->getProperty( kModelPropertyKey ) );
	if( registryEntry == 0)
	{
		DLOG("AppleKauaiATA unknown model property.\n");
		return 0;
	}


	
	// initialize the timing params to PIO mode 0 at 600ns and MWDMA mode 0 at 480ns
	// ultra to mode 5 (fastest)
	busTimings[0].pioMWRegValue = busTimings[1].pioMWRegValue = 0x08000A92 | 0x00618000;
	busTimings[0].ultraRegValue = busTimings[1].ultraRegValue = 0x00002921;
	
	busTimings[0].ataPIOSpeedMode = busTimings[1].ataPIOSpeedMode = 0x01 ;				// PIO Mode Timing class (bit-significant)
	busTimings[0].ataPIOCycleTime = busTimings[1].ataPIOCycleTime = 600 ;				// Cycle time for PIO mode
	busTimings[0].ataMultiDMASpeed = busTimings[1].ataMultiDMASpeed = 0x01;				// Multiple Word DMA Timing Class (bit-significant)
	busTimings[0].ataMultiCycleTime = busTimings[1].ataMultiCycleTime = 480;				// Cycle time for Multiword DMA mode
	busTimings[0].ataUltraDMASpeedMode = busTimings[1].ataUltraDMASpeedMode = 0x20;				// UltraDMA Timing Class (bit-significant)





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
AppleKauaiATA::start(IOService *provider)
{

    DLOG("AppleKauaiATA::start() begin\n");
    
    if( ! OSDynamicCast(IOPCIDevice, provider ) )
    {
    
    	DLOG("AppleKauaiATA provider not IOPCIDevice!\n");
    	return false;    
    
    }

	if( ! provider->open(this) )
	{
		DLOG("AppleKauai provider did not open\n");
		return false;	
	}

	if( rxFeatureOn )
	{
	
		dmaBuffer =    IOBufferMemoryDescriptor::withOptions(  kIOMemoryDirectionMask,
															kRX_bufferSize,
															4096);
		if( ! dmaBuffer )
		{
		
			DLOG( "kauai ata failed to create dmaBuffer\n");
			return false;
		}
		// setup bits for K2
		((IOPCIDevice *)provider)->configWrite8( (UInt8) 0xC, (UInt8) 0x08);
		((IOPCIDevice *)provider)->configWrite8( (UInt8) 0x4, (UInt8) (((IOPCIDevice *)provider)->configRead8(0x4) | 0x10));
	
		DLOG( "kauai ata created rx buffer\n");
	
	}

	((IOPCIDevice *)provider)->setMemoryEnable(true);
	((IOPCIDevice *)provider)->setBusMasterEnable( true );

	ATADeviceNub* newNub=0L;
 
 	// call start on the superclass
    if (!super::start( provider))
 	{
        DLOG("AppleKauaiATA: super::start() failed\n");
        provider->close(this);
        return false;
	}

    DLOG("AppleKauaiATA::start() done\n");

	for( UInt32 i = 0; i < 2; i++)
	{
		if( _devInfo[i].type != kUnknownATADeviceType )
		{
		
			DLOG("AppleKauaiATA creating nub\n");
			newNub = ATADeviceNub::ataDeviceNub( (IOATAController*)this, (ataUnitID) i, _devInfo[i].type );
		
			if( newNub )
			{
				
				DLOG("AppleKauaiATA attach nub\n");
				
				newNub->attach(this);
			
				_nub[i] = (IOATADevice*) newNub;
				
				DLOG("AppleKauaiATA register nub\n");
				if( rxFeatureOn )
				{
					OSNumber* rx_alignLimit ;
					rx_alignLimit = OSNumber::withNumber( (kRX_bufferSize), 32);
					newNub->setProperty( kIOMaximumByteCountReadKey, rx_alignLimit);
					rx_alignLimit->release();
				
				}

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
AppleKauaiATA::free()
{
	
	super::free();


}


/*---------------------------------------------------------------------------
 *	get the work loop. If not already created, make one.
 *
 ---------------------------------------------------------------------------*/
IOWorkLoop*
AppleKauaiATA::getWorkLoop() const
{

	DLOG("AppleKauaiATA::getWorkLoop\n");

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
AppleKauaiATA::configureTFPointers(void)
{
    DLOG("AppleKauaiATA config TF Pointers \n");

	// call the superclass to setup the taskfile pointers

	DLOG("AppleKauaiATA configureTFPointers begin\n");


	// get the task file pointers
	_baseAddressMap = _provider->mapDeviceMemoryWithIndex(0);
	
	if( !_baseAddressMap )
	{
		DLOG("AppleKauaiATA no base map\n");
		return false;
	}

	volatile UInt8* baseAddress = (volatile UInt8*)_baseAddressMap->getVirtualAddress();

	if( !baseAddress )
	{
		DLOG("AppleKauaiATA no base address\n");
		return false;
	}

	// kauai fcr is at base.
	_kauaiATAFCR = (volatile UInt32*) baseAddress;
	DLOG("MacIOATA baseAdress = %lx\n", baseAddress);

	// the kauau DMA control is at base + 0x1000
	_dmaControlReg = (volatile IODBDMAChannelRegisters*) (baseAddress + 0x1000);
	
	// offset 0x2000 to the begining of the ATA cell registers. 
	baseAddress += 0x2000;

	_tfDataReg = (volatile UInt16*) (baseAddress + 0x00);

	_tfFeatureReg = baseAddress + 0x10;
	_tfSCountReg = baseAddress + 0x20;
	_tfSectorNReg = baseAddress + 0x30;
	_tfCylLoReg = baseAddress + 0x40;
	_tfCylHiReg = baseAddress + 0x50;
	_tfSDHReg = baseAddress + 0x60;
	_tfStatusCmdReg = baseAddress + 0x70;  
	_tfAltSDevCReg = baseAddress + 0x160;   

	_timingConfigReg = (volatile UInt32*) (baseAddress + 0x200); 
	


	_ultraTimingControl = (volatile UInt32*) (baseAddress + 0x210); 
	_autoPollingControl = (volatile UInt32*) (baseAddress + 0x220); 
	_interruptPendingReg = (volatile UInt32*) (baseAddress + 0x300); 

	DLOG("MacIOATA configureTFPointers end\n");

	// now that the address is setup, configure the controller to some safe 
	// default value as we are called during start in order to look for drives.
	DLOG("AppleKauaiATA enable FCR \n");

	OSWriteSwapInt32(_kauaiATAFCR, 0, 0x00000007 );  //set bits 0,1,2


    DLOG("AppleKauaiATA setting default timing \n");
	
	selectIOTiming( (ataUnitID) 0 );

   DLOG("AppleKauaiATA configTFPointers done\n");
	
	return true;

}

/*---------------------------------------------------------------------------
 *	provide information on the bus capability 
 *
 *
 ---------------------------------------------------------------------------*/
IOReturn 
AppleKauaiATA::provideBusInfo( IOATABusInfo* infoOut)
{
	if( infoOut == 0)
	{
		DLOG("AppleKauaiATA nil pointer in provideBusInfo\n");
		return -1;
	
	}


	infoOut->zeroData();
	

	infoOut->setSocketType( kInternalATASocket ); // internal fixed, media-bay, PC-Card
	
	infoOut->setPIOModes( kATASupportedPIOModes);		
	infoOut->setDMAModes( kATASupportedMultiDMAModes );			
	infoOut->setUltraModes( kATASupportedUltraDMAModes );
	
	infoOut->setExtendedLBA( true );  // indicate extended LBA is available on this bus. 
	infoOut->setMaxBlocksExtended( 0x0800 ); // allow up to 1 meg per transfer on this controller. 
											//Size is arbitrary up to 16 bit sector count (0x0000)
											// current technology though puts the efficiency limit around 1 meg though. 
	if( rxFeatureOn)
		infoOut->setMaxBlocksExtended( 0x0100 );



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
AppleKauaiATA::getConfig( IOATADevConfig* configRequest, UInt32 unitNumber)
{

	// param check the pointers and the unit number.
	
	if( configRequest == 0
		|| unitNumber > 1 )
		
	{
	
		DLOG("AppleKauaiATA bad param in getConfig\n");
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
AppleKauaiATA::selectConfig( IOATADevConfig* configRequest, UInt32 unitNumber)
{


	// param check the pointers and the unit number.
	
	if( configRequest == 0
		|| unitNumber > 1 )
		
	{
	
		DLOG("AppleKauaiATA bad param in setConfig\n");
		return -1;	
	}



	// all config requests must include a PIO mode
	if( ( configRequest->getPIOMode() & kATASupportedPIOModes ) == 0x00 )
	{
		DLOG("AppleKauaiATA setConfig PIO mode not supported\n");
		return kATAModeNotSupported;		
	
	}


	// DMA is optional - not all devices support it, we reality check for a setting
	// that is beyond our capability


	UInt8 ultraSupported = kATASupportedUltraDMAModes;	

	// make sure a requested ultra ATA mode is within the range of this device configuration

	if( configRequest->getUltraMode() & ~ultraSupported )
	{
		DLOG("AppleKauaiATA setConfig no ultra\n");
		return kATAModeNotSupported;	
		
	} 



	if( configRequest->getDMAMode() & ~kATASupportedMultiDMAModes )
	{
		DLOG("AppleKauaiATA setConfig DMA mode not supported\n");
		return kATAModeNotSupported;		
	
	} 

	if( configRequest->getDMAMode() > 0x0000
		&& configRequest->getUltraMode() > 0x0000 )
	{
		DLOG("AppleKauaiATA err, only one DMA class allowed in config select\n");
		return kATAModeNotSupported;
	
	}

	_devInfo[unitNumber].packetSend = configRequest->getPacketConfig();
	DLOG("AppleKauaiATA setConfig packetConfig = %ld\n", _devInfo[unitNumber].packetSend );

	
	return selectIOTimerValue(configRequest, unitNumber);

}


/*---------------------------------------------------------------------------
 *	calculate the timing configuration as requested.
 *
 *
 ---------------------------------------------------------------------------*/

IOReturn
AppleKauaiATA::selectIOTimerValue( IOATADevConfig* configRequest, UInt32 unitNumber)
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




UInt32 PIOCycleValue100[kAppleKauaiPIOCycleEntries] = 
					{
						0x08000FFF,					// Hardware minimum		(1260 ns = 'E' + 630 rec + 630 acc) 	63, 63
						0x08000A92,					// Minimum PIO mode 0 	(600 ns = 'E' + 420 rec + 180 acc)	42, 18  42 * 64 + 18 = 2706
						0x0800060F,					// Minimum PIO mode 1	(390 ns = 240 rec + 150 acc)	  	24, 15  24 * 64 + 15 = 1551
						0x08000492,					// Derated PIO Mode 2/3/4	(360 ns = 180 rec + 180 acc)	18, 18  18 * 64 + 18 = 1170
						0x0800048F,					// Derated PIO Mode 2/3/4	(330 ns = 180 rec + 150 acc)    18, 15  18 * 64 + 15 = 1167
						0x080003CF,					// Derated PIO Mode 2/3/4	(300 ns = 150 rec + 150 acc)	15, 15  15 * 64 + 15 = 975
						0x080003CC,					// Derated PIO mode 2/3/4	(270 ns = 150 rec + 120 acc)	15, 12  15 * 64 + 12 = 972
						0x0800038B,					// Minimum PIO mode 2 		(240 ns = 135 rec + 105 acc)	14, 11  14 * 64 + 11 = 907
						0x0800030C,					// Derated PIO Mode 3/4 	(240 ns = 120 rec + 120 acc)	12, 12	12 * 64 + 12 = 780
						0x05000249,					// Minimum PIO mode 3		(180 ns =  90 rec + 90 acc)	   	 9,  9   9 * 64 + 9  = 585
						0x04000148					// Minimum PIO mode 4		(120 ns =  45 rec + 75 acc)	 	 5,  8   5 * 64 + 8  = 328 
 					};



static const UInt16 PIOCycleTime[ kAppleKauaiPIOCycleEntries ]=
					{
						930,						// Hardware maximum			(0)
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


static const UInt32 MultiDMACycleValue100[kAppleKauaiMultiDMACycleEntries] =
				{
					0x00FFF000, 				// Hardware maximum     ( 1260=  630rec+630acc)	63, 63 
					0x00618000, 				// Minimum Multi mode 0  (480=  240rec+240acc)	24, 24
					0x00492000, 				// Derated mode 1 or 2   (360=  180rec+180acc)	18, 18
					0x0038E000, 				// Derated mode 1 or 2   (270=H+135rec+135acc)	14, 14 
					0x0030C000, 				// Derated mode 1 or 2   (240=  120rec+120acc)	12, 12
					0x002CB000, 				// Derated mode 1 or 2   (210=H+110rec+110acc)	11, 11
					0x00249000, 				// Derated mode 1 or 2   (180=   90rec+90acc)	 9, 9
					0x00209000, 				// Minimum Multi mode 1  (165= H+75rec+90acc)	 8, 9
					0x00148000	 				// Minimum Multi mode 2  (120= H+45rec+75acc) 	 5, 8
				}; 


static const UInt16 MultiDMACycleTime[kAppleKauaiMultiDMACycleEntries] =
				{
					1260, 						// Hardware maximum     	(0) 
					480, 						// Minimum Multi mode 0  (1)	 
					360, 						// Derated mode 1 or 2   (2)	 
					270, 						// Derated mode 1 or 2   (3)	 
					240, 						// Derated mode 1 or 2   (4)	 
					210, 						// Derated mode 1 or 2   (5)	 
					180, 						// Derated mode 1 or 2   (6)	 
					150, 						// Minimum Multi mode 1  (7)	 
					120 						// Minimum Multi mode 2  (8)	 
				};



static const UInt32 UltraDMACycleValue100[kAppleKauaiUltraDMACycleEntries] =
				{
					0x000070C1,					// crc 7 rdy2paus=00, wrDataSetup=12, ultra mode on     	7,0,12
					0x00005D81,					// crc 5 rdy2paus=13, wrDataSetup=8, ultra on				5,13,8 
					0x00004A61,					// crc 4 rdy2paus=10, wrDataSetup=6, ultra on				4,10,6
					0x00003A51,					// crc 3 rdy2paus=10, wrDataSetup=5, ultra on    mode 3 	3,10,5
					0x00002A31,					// crc 2 rdy2paus=10, wrDataSetup=3, ultra on    mode 4		2,10,3
					0x00002921					// crc 2 rdy2paus=9, wrDataSetup=2, ultra on    mode 5		2,9,2
				};
				
static const UInt16 UltraDMACycleTime[kAppleKauaiUltraDMACycleEntries] =
				{
					120,						// mode 0
					90,							// mode 1
					60,							// mode 2
					45,							// mode 3
					30,							// mode 4
					20							// mode 5
				};
				
/*-----------------------------------------*/






	UInt32 pioConfigBits = PIOCycleValue100[0];

	// the theory is simple, just examine the requested mode and cycle time, find the 
	// entry in the table which is closest, but NOT faster than the requested cycle time.

	// convert the bit maps into numbers
	UInt32 pioModeNumber = bitSigToNumeric( configRequest->getPIOMode());

	// check for out of range values.
	if( pioModeNumber > kATAMaxPIOMode )
	{
		DLOG("AppleKauaiATA pio mode out of range\n");
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
	for( int i = kAppleKauaiPIOCycleEntries - 1; i >= 0; i--)
	{
	
		if( pioCycleTime <= PIOCycleTime[ i ] )
		{
			// found the fastest time which is not greater than
			// the requested time.
			pioConfigBits = PIOCycleValue100[i];
			break;
		}
	}
	

	// now do the same for DMA modes.
	UInt32 mwdmaConfigBits = MultiDMACycleValue100[0];
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
		for( int i = kAppleKauaiMultiDMACycleEntries - 1; i >= 0; i--)
		{
		
			if( dmaCycleTime <= MultiDMACycleTime[ i ] )
			{
				// found the fastest time which is not greater than
				// the requested time.
				mwdmaConfigBits = MultiDMACycleValue100[i];
				break;
			}
		}
	}

	UInt32 ultraModeNumber = 0;
	UInt32 ultraConfigBits = 0;
	// same deal for ultra ATA modes
	if( configRequest->getUltraMode() )
	{
		ultraModeNumber = bitSigToNumeric( configRequest->getUltraMode() );
	
		// in the event someone asks for more than we can do, go as fast as possible given our cable condition.
		if( ultraModeNumber > kATAMaxUltraDMAMode )
		{
			ultraModeNumber =  kATAMaxUltraDMAMode;		
		}
	
		ultraConfigBits = UltraDMACycleValue100[ ultraModeNumber ];
	
	
	}



	// now combine the bits forming the configuration and put them
	// into the timing structure along with the mode and cycle time
	
	busTimings[unitNumber].pioMWRegValue = mwdmaConfigBits | pioConfigBits;
	busTimings[unitNumber].ultraRegValue = ultraConfigBits;
	busTimings[unitNumber].ataPIOSpeedMode = configRequest->getPIOMode();
	busTimings[unitNumber].ataPIOCycleTime = pioCycleTime;
	busTimings[unitNumber].ataMultiDMASpeed = configRequest->getDMAMode();
	busTimings[unitNumber].ataMultiCycleTime = dmaCycleTime;
	busTimings[unitNumber].ataUltraDMASpeedMode = configRequest->getUltraMode();

	DLOG("AppleKauaiATA PIO mode %x at %ld ns selected for device: %x\n", (int)pioModeNumber, pioCycleTime, (int)unitNumber);
	DLOG("AppleKauaiATA DMA mode %x at %ld ns selected for device: %x\n", (int)dmaModeNumber, dmaCycleTime, (int)unitNumber);
	DLOG("AppleKauaiATA Ultra mode %x at %ld ns selected for device: %x\n", (int)ultraModeNumber, UltraDMACycleTime[ultraModeNumber], (int)unitNumber);
	DLOG("AppleKauai pio/mw cycle value = %x for unit: %x\n", 	busTimings[unitNumber].pioMWRegValue, unitNumber);
	DLOG("AppleKauai ultra cycle value = %x for unit: %x\n", 	busTimings[unitNumber].ultraRegValue, unitNumber);


	// stuff the values back into the request structure and return result
	return getConfig( configRequest, unitNumber);

}



// punch the correct value into the IO timer register for the selected unit.
void 
AppleKauaiATA::selectIOTiming( ataUnitID unit )
{

	//DLOG("AppleKauaiATA setting timing config %lx on unit %d\n", busTimings[unit].cycleRegValue, unit);

	OSWriteSwapInt32( _kauaiATAFCR, 0, 0x7); 	
	OSWriteSwapInt32(_timingConfigReg, 0, busTimings[unit].pioMWRegValue );
	OSWriteSwapInt32( _ultraTimingControl, 0, busTimings[unit].ultraRegValue); 

}



/*---------------------------------------------------------------------------
 *
 ---------------------------------------------------------------------------*/
IOReturn
AppleKauaiATA::handleDeviceInterrupt(void)
{

	if( !_currentCommand )
	{
		volatile UInt8 status = *_tfStatusCmdReg;
		OSSynchronizeIO();
		status++; // prevent compiler removal of unused variable.
		return 0;
	}



	UInt32 intStatus = OSReadLittleInt32( (void*) _interruptPendingReg, 0x00);
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
AppleKauaiATA::selectDevice( ataUnitID unit )
{
	_devIntSrc->disable();
	IOReturn result = super::selectDevice( unit);
	_devIntSrc->enable();
	
	return result;


}


IOReturn
AppleKauaiATA::synchronousIO(void)
{

	_devIntSrc->disable();

	IOReturn result = super::synchronousIO();
	
	// read the status register in case the drive didn't clear int-pending status already.

	UInt32 intStatus = OSReadLittleInt32( (void*) _interruptPendingReg, 0x00);
	OSSynchronizeIO();
	
	// loop until the drive release the interrupt line.
	while( (intStatus & 0x40000000) )	
	{
		volatile UInt8 status = *_tfStatusCmdReg;		
		OSSynchronizeIO();	
		status++;
		intStatus = OSReadLittleInt32( (void*) _interruptPendingReg, 0x00);
		OSSynchronizeIO();	
	}
	

	_devIntSrc->enable();
	
	return result;

}




IOReturn
AppleKauaiATA::handleBusReset(void)
{

	bool		isATAPIReset = ((_currentCommand->getFlags() & mATAFlagProtocolATAPI) != 0);
	bool		doATAPI[2];
	IOReturn	err = kATANoErr;
	UInt8		index;
	UInt8 		statCheck;
	
	DLOG("AppleKauaiATA bus reset start.\n");

	doATAPI[0] = doATAPI[1] = false;		

	// If this is an ATAPI reset select just the corresponding atapi device (instead of both) 
	if(isATAPIReset)
	{
		doATAPI[_currentCommand->getUnit()] = true;  // Mark only selected ATAPI as reset victim.
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

	DLOG("AppleKauaiATA bus reset done.\n");


	completeIO( err );

	
	return err;

}

IOATAController::transState	
AppleKauaiATA::determineATAPIState(void)
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
AppleKauaiATA::processDMAInterrupt(void)
{
	_needsResync = _resyncInterrupts;
	super::processDMAInterrupt();
	_needsResync = false;
}

/*---------------------------------------------------------------------------
 *
 *	allocate memory and resources for the DMA descriptors. Override from 
 *	MacIOATA because the dma register is now in a different place from older
 *	ATA cells. 
 ---------------------------------------------------------------------------*/
bool
AppleKauaiATA::allocDMAChannel(void)
{

	if( !_dmaControlReg )
	{
		DLOG("AppleKauaiATA no DMA address\n");
		return false;
	}

	// Now allocate a playground for channel descriptors
	
	// under ATA-5 and earlier, ATA commands are allowed a maximum of 256 * 512 byte
	// sectors on a single command. This works out to 1 + 32 * 4096 byte chunks of physical
	// memory if the page size is 4K and the memory is completely fragmented, 
	// so we need a total of 33 descriptors, plus a stop and NO-OP command with 
	// to generate the interrupt. This allows for full transfers without any pauses
	// to regenerate a DMA chain.
	
	// allocate 35 descriptors  33 memory commands + 1 No Op, + 1 Stop.
	
	// IODBDMA-Start panics unless memory is aligned on 0x10
	_descriptors = (IODBDMADescriptor*)IOMallocContiguous( sizeof(IODBDMADescriptor) * kATAMaxDMADesc, 
						0x10, 
						&_descriptorsPhysical );
	
	if(	! _descriptors )
	{
		DLOG("AppleKauaiATA alloc descriptors failed\n");
		return false;
	
	}



	_DMACursor = IODBDMAMemoryCursor::withSpecification(0xFFFE, /*64K - 2*/
                                       					kMaxATAXfer  /* Maybe should be unlimited? */
                                     					/*inAlignment - byte aligned by default*/);

	
	if( ! _DMACursor )
	{
		DLOG("AppleKauaiATA alloc DMACursor failed\n");
		return false;
	}


	// fill the chain with stop commands to initialize it.	
	initATADMAChains(_descriptors);
	
	return true;
}

// preflight DMA requests for alignment parameters
IOReturn
AppleKauaiATA::handleExecIO( void )
{
	if( ! rxFeatureOn)
		goto preflightDone;
	
	DLOG("AppleKauaiATA checking flags %X\n", _currentCommand->getFlags() );
	// alignment restrictions on read, write is not affected
	if( _currentCommand->getFlags() & (mATAFlagUseDMA)  
	 && _currentCommand->getFlags() & (mATAFlagIORead) )
	{
		DLOG("AppleKauaiATA checking alignment\n");
	
		// ATAPI devices can do weird size multiples, so if it doesn't fit our restriction,
		// convert the command to a PIO command.
		if(_currentCommand->getOpcode() == kATAPIFnExecIO)
		{
			DLOG("AppleKauaiATA ATAPI length check\n");
			// check xfer length for beat parameter remainder
			UInt32 remainder = _currentCommand->getByteCount() % 16;
			if( remainder )
			{
				// convert this transfer to PIO mode
				DLOG("AppleKauaiATA convert ATAPI to PIO remainder = %d\n", remainder);
				// turn off DMA flag
				_currentCommand->setFlags( _currentCommand->getFlags() & (~mATAFlagUseDMA) );
				// turn off DMA bit in features register
				_currentCommand->getTaskFilePtr()->ataTFFeatures = 0x00;
				goto preflightDone;
			}

			if( _currentCommand->getByteCount() > kRX_bufferSize )
			{
				DLOG("AppleKauaiATA count exceeds buffer size!!! %d\n", _currentCommand->getByteCount());
				goto preflightDone;
			}

			DLOG("AppleKauaiATA double buffering dma\n");
			// ATA and ATAPI reads are going to get double buffered because we want to make sure we fit the alignment 
			// requirements
			// save the client's DMA buffer
			clientBuffer = _currentCommand->getBuffer();
			// set our flag so we know to copy back
			bufferRX = true;
			// substitute our aligned buffer in the command
			_currentCommand->setBuffer(dmaBuffer);
			dmaBuffer->prepare(kIODirectionIn);
			
			//now do DMA read as normal, but into our own buffer. At complete IO, we clean up and copy back the data.
		
		}

	}
preflightDone:	
	
	return super::handleExecIO();


}

void 
AppleKauaiATA::completeIO( IOReturn commandResult )
{
	if( bufferRX )
	{
		DLOG("AppleKauaiATA complete double buffer\n");
		bufferRX = false;
		dmaBuffer->complete(kIODirectionIn);
		clientBuffer->writeBytes(0, dmaBuffer->getBytesNoCopy(), _currentCommand->getActualTransfer() );
		_currentCommand->setBuffer( clientBuffer );
		clientBuffer = 0;
	}


	super::completeIO( commandResult );

