/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All rights reserved.
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
#define kCompatibleString3 "shasta-ata"
#define kModelPropertyKey "model"
#define kCableTypeKey "cable-type"
#define kModel6String "ata-6"
#define k80ConductorString "80-conductor"




			/*  Bit-significant representation of supported modes.*/ 
#define kATASupportedPIOModes		 0x001F	// modes 4, 3, 2, 1, and 0
#define kATASupportedMultiDMAModes	 0x0007	// modes 2, 1, and 0
#define	kATASupportedUltraDMAModes	 0x003f	// modes 5, 4, 3, 2, 1, and 0
#define	kATASupportedUltraDMA133Modes	 0x007f	// modes 6, 5, 4, 3, 2, 1, and 0

/* Number of entries in various tables */
#define kAppleKauaiPIOCycleEntries			11
#define kAppleKauaiMultiDMACycleEntries 		9
#define	kAppleKauaiUltraDMACycleEntries		6
#define	kAppleKauaiUltraDMA133CycleEntries		7

// ------ ¥¥¥ Maximum transfer modes the AppleKauai controller is capable of supporting ¥¥¥ ÐÐÐÐÐÐÐÐ
#define kATAMaxPIOMode		 4
#define kATAMaxMultiDMAMode	2
//#define	kATAMaxUltraDMAMode 5
#define	kATAMaxUltraDMA133Mode 6

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
	ultra133 = false;
	myProvider = 0;

#ifdef __KAUAI_POLLED__
	polledAdapter = 0;
	polledMode = false;
#endif	

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

			if( compatibleEntry->isEqualTo( kCompatibleString3, sizeof(kCompatibleString3)-1 ) == false) 
			{
				// not our type of controller		
				DLOG("AppleKauaiATA compatible property doesn't match\n");
				return 0;
			}
			
			
			///////////////////
			//  turn on 133 mode for Shasta ATA controller.
			//////////////////
	
			ultra133 = true;
			// turn on rxbuffer feature for this controller. 
			rxFeatureOn = true;		
			IOLog("AppleKauaiATA shasta-ata features enabled\n");
			
			
		}
		// K2-ATA controller.
		// turn on rxbuffer feature for this controller. 
		rxFeatureOn = true;		
		DLOG("AppleKauaiATA rx buffer feature enabled\n");
	}
	
#ifdef __KAUAI_POLLED__	
/// TBD  set polled mode per flag from property
 
	OSData *polledProperty =  OSDynamicCast( OSData, provider->getProperty( kPolledPropertyKey ) );
	if( polledProperty )
	{
		DLOG("AppleKauaiATA polledMode feature enabled\n");
		polledMode = true;
	} else {
	
		;
		
		DLOG("AppleKauaiATA polledMode property not found. \n");
	
	}
	
	
#endif	
	
	// do a little initialization here once the probe is succesful so we can start clean.

	OSData		*registryEntry;
	registryEntry = OSDynamicCast( OSData, provider->getProperty( kModelPropertyKey ) );
	if( registryEntry == 0)
	{
		DLOG("AppleKauaiATA unknown model property.\n");
		return 0;
	}

	// initial timing values.
	
	// initialize the timing params to PIO mode 0 at 600ns and MWDMA mode 0 at 480ns
	// ultra to mode 5 (fastest)
	busTimings[0].pioMWRegValue = busTimings[1].pioMWRegValue = 0x08000A92 | 0x00618000;
	busTimings[0].ultraRegValue = busTimings[1].ultraRegValue = 0x00002921;
	if( ultra133 )
	{	// set revised timing for ultra 133. 
		busTimings[0].pioMWRegValue = busTimings[1].pioMWRegValue = 0x0a820c97;  // pio mwdma mode 0
		busTimings[0].ultraRegValue = busTimings[1].ultraRegValue = 0x00033031;  // uata mode 5 (100) 133 is not as common as 100. 
	}
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

	myProvider = provider;

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

#ifdef __KAUAI_POLLED__
	if( polledMode )
	{
		polledAdapter = new KauaiPolledAdapter;
		
		if( 0== polledAdapter)
		{
			DLOG("  AppleKauaiATA *** failed creating polled adapter\n");
			
		} else {
		
			polledAdapter->setOwner( this );
			DLOG("  AppleKauaiATA polled adapter attached.\n");
			setProperty ( kIOPolledInterfaceSupportKey, polledAdapter );
			polledAdapter->release();
		}
	
	}
#endif

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

	//DLOG("AppleKauaiATA::getWorkLoop\n");

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
	if( ultra133 )
	{
		infoOut->setUltraModes( kATASupportedUltraDMA133Modes );
		
	}
	
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
	if( ultra133 )
	{
	
		ultraSupported = kATASupportedUltraDMA133Modes;
	
	}
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


UInt32 PIOCycleValue133[kAppleKauaiPIOCycleEntries] = 
					{
						0x08000FFF,					// Hardware minimum		(1260 ns = 'E' + 630 rec + 630 acc) 	63, 63
						0x0A000C97,					// Minimum PIO mode 0 	(600 ns = 'E' + 420 rec + 180 acc)	42, 18  42 * 64 + 18 = 2706
						0x07000712,					// Minimum PIO mode 1	(390 ns = 240 rec + 150 acc)	  	24, 15  24 * 64 + 15 = 1551
						0x040003CD,					// Derated PIO Mode 2/3/4	(360 ns = 180 rec + 180 acc)	18, 18  18 * 64 + 18 = 1170
						0x040003CD,					// Derated PIO Mode 2/3/4	(330 ns = 180 rec + 150 acc)    18, 15  18 * 64 + 15 = 1167
						0x040003CD,					// Derated PIO Mode 2/3/4	(300 ns = 150 rec + 150 acc)	15, 15  15 * 64 + 15 = 975
						0x040003CD,					// Derated PIO mode 2/3/4	(270 ns = 150 rec + 120 acc)	15, 12  15 * 64 + 12 = 972
						0x040003CD,					// Minimum PIO mode 2 		(240 ns = 135 rec + 105 acc)	14, 11  14 * 64 + 11 = 907
						0x040003CD,					// Derated PIO Mode 3/4 	(240 ns = 120 rec + 120 acc)	12, 12	12 * 64 + 12 = 780
						0x0400028B,					// Minimum PIO mode 3		(180 ns =  90 rec + 90 acc)	   	 9,  9   9 * 64 + 9  = 585
						0x0400010A					// Minimum PIO mode 4		(120 ns =  45 rec + 75 acc)	 	 5,  8   5 * 64 + 8  = 328 
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

static const UInt32 MultiDMACycleValue133[kAppleKauaiMultiDMACycleEntries] =
				{
					0x00FFF000, 				// Hardware maximum     ( 1260=  630rec+630acc)	63, 63 
					0x00820800, 				// Minimum Multi mode 0  (480=  240rec+240acc)	24, 24
					0x00820800, 				// Derated mode 1 or 2   (360=  180rec+180acc)	18, 18
					0x00820800, 				// Derated mode 1 or 2   (270=H+135rec+135acc)	14, 14 
					0x00820800, 				// Derated mode 1 or 2   (240=  120rec+120acc)	12, 12
					0x00820800, 				// Derated mode 1 or 2   (210=H+110rec+110acc)	11, 11
					0x00820800, 				// Derated mode 1 or 2   (180=   90rec+90acc)	 9, 9
					0x0028B000, 				// Minimum Multi mode 1  (165= H+75rec+90acc)	 8, 9
					0x001CA000	 				// Minimum Multi mode 2  (120= H+45rec+75acc) 	 5, 8
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



static const UInt32 UltraDMACycleValue100[kAppleKauaiUltraDMACycleEntries ] =
				{
					0x000070C1,					// crc 7 rdy2paus=00, wrDataSetup=12, ultra mode on     	7,0,12
					0x00005D81,					// crc 5 rdy2paus=13, wrDataSetup=8, ultra on				5,13,8 
					0x00004A61,					// crc 4 rdy2paus=10, wrDataSetup=6, ultra on				4,10,6
					0x00003A51,					// crc 3 rdy2paus=10, wrDataSetup=5, ultra on    mode 3 	3,10,5
					0x00002A31,					// crc 2 rdy2paus=10, wrDataSetup=3, ultra on    mode 4		2,10,3
					0x00002921					// crc 2 rdy2paus=9, wrDataSetup=2, ultra on    mode 5		2,9,2
				};
				
				
static const UInt32 UltraDMACycleValue133[kAppleKauaiUltraDMA133CycleEntries] =
				{
					0x00035901,					// 0	
					0x000348b1,					// 1
					0x00033881,					// 2 
					0x00033861,					// 3
					0x00033841,					// 4
					0x00033031,					// 5
					0x00033021					// 6
				};

#ifdef  ATA_DEBUG				
				
static const UInt16 UltraDMACycleTime100[kAppleKauaiUltraDMACycleEntries] =
				{
					120,						// mode 0
					90,							// mode 1
					60,							// mode 2
					45,							// mode 3
					30,							// mode 4
					20							// mode 5

				};

static const UInt16 UltraDMACycleTime133[kAppleKauaiUltraDMA133CycleEntries] =
				{
					120,						// mode 0
					90,							// mode 1
					60,							// mode 2
					45,							// mode 3
					30,							// mode 4
					20,							// mode 5
					15							// mode 6
				};

#endif				
/*-----------------------------------------*/






	UInt32 pioConfigBits = PIOCycleValue100[0];
	if( ultra133 )
	{
		pioConfigBits = PIOCycleValue133[0];
	
	}
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
			if( ultra133 )
			{
				pioConfigBits = PIOCycleValue133[i];
			
			}
			break;
		}
	}
	

	// now do the same for DMA modes.
	UInt32 mwdmaConfigBits = MultiDMACycleValue100[0];
	if( ultra133 )
	{
		mwdmaConfigBits = MultiDMACycleValue133[0];
	}
	
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
				if( ultra133 )
				{
					mwdmaConfigBits = MultiDMACycleValue133[i];
				}
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
	
		
		
		if( ultra133 )
		{
			if( ultraModeNumber > 6)
				ultraModeNumber = 6;
				
			ultraConfigBits = UltraDMACycleValue133[ ultraModeNumber ];	
		
		} else {
		
			if( ultraModeNumber > 5 )
				ultraModeNumber = 5;
				
			ultraConfigBits = UltraDMACycleValue100[ ultraModeNumber ];
		}
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
	DLOG("AppleKauaiATA Ultra mode %x at %ld ns selected for device: %x\n", (int)ultraModeNumber, UltraDMACycleTime133[ultraModeNumber], (int)unitNumber);
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
	OSWriteLittleInt32( (void*) _interruptPendingReg, 0x00, 0x80000000); // clear the pending bit on the DMA interrupt
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
	
	
// 3571515  small DMA requests should be converted to PIO on writes to disk.
/*	if(  (_currentCommand->getFlags() & (mATAFlagUseDMA))  
	 && (_currentCommand->getFlags() & (mATAFlagIOWrite) )
	 && ( _currentCommand->getByteCount() < 192 ))
	 {
		if(_currentCommand->getOpcode() == kATAPIFnExecIO)
		{
			// convert this transfer to PIO mode
			DLOG("AppleKauaiATA convert ATAPI to PIO size = %d\n", _currentCommand->getByteCount());
			// turn off DMA flag
			_currentCommand->setFlags( _currentCommand->getFlags() & (~mATAFlagUseDMA) );
			// turn off DMA bit in features register
			_currentCommand->getTaskFilePtr()->ataTFFeatures &= 0xFE;
		}
	 }
	
*/	
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
				_currentCommand->getTaskFilePtr()->ataTFFeatures &= 0xFE;
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

}

void
AppleKauaiATA::handleTimeout( void )
{

#ifdef  ATA_DEBUG

	DLOG("AppleKauaiATA handleTimeout cmd flags %X\n", _currentCommand->getFlags() );
	DLOG("AppleKauaiATA handleTimeout byte count%d\n", _currentCommand->getByteCount() );
	DLOG("AppleKauaiATA handleTimeout _dmaIntExpected (true if non-zero value) = %X\n", _dmaIntExpected );
	DLOG("AppleKauaiATA handleTimeout DMA state flag %X\n", _dmaState );

	IOMemoryDescriptor* theClientBuffer = _currentCommand->getBuffer();

	DLOG("AppleKauaiATA handleTimeout buffer direction %d\n" ,theClientBuffer->getDirection());
	DLOG("AppleKauaiATA handleTimeout buffer length %d\n", theClientBuffer->getLength());
	DLOG("AppleKauaiATA handleTimeout buffer physical address %X\n",theClientBuffer->getPhysicalAddress()); 

	// dump the descriptor tables
	
	IODBDMADescriptor*	descPtr 		= _descriptors; 
	
	DLOG("AppleKauaiATA handleTimeout dumping CC chain\n");
	DLOG("ChanStat= %lx\n", IOGetDBDMAChannelStatus(_dmaControlReg));
	
	for( int index = 0; index < kATAMaxDMADesc; index++)
	{
		
		DLOG(" index=%d    ccOpr = %lx, ccAddr = %lx, ccCmd = %lx, ccResult= %lx \n",
		index,
		IOGetCCOperation( descPtr ),
		IOGetCCAddress(descPtr),
		IOGetCCCmdDep(descPtr),
		IOGetCCResult(descPtr) );
		descPtr++;
	}
#endif

	super::handleTimeout();
}

/*----------------------------------------------------------------------------------------
//	Function:		activateDMAEngine
//	Description:	Activate the DBDMA on the ATA bus associated with current device.
					engine will begin executing the command chain already programmed.
//	Input:			None
//	Output:			None
//----------------------------------------------------------------------------------------*/

void			
AppleKauaiATA::activateDMAEngine(void)
{

	if( IOGetDBDMAChannelStatus( _dmaControlReg) & kdbdmaActive )
	{
		/* For multiple DMA chain execution, don't stop the DMA or the FIFOs lose data.*/
		/* If DMA is active already (stray from an error?), shut it down cleanly. */
		shutDownATADMA();
	}
	

    IOSetDBDMACommandPtr( _dmaControlReg, (unsigned int) _descriptorsPhysical);


	// set interrupt select to s7 so we can wait for the s7 bit to go hi before the no-op generates the irq
	IOSetDBDMAWaitSelect (_dmaControlReg, kdbdmaS7 << 16);


	/* Blastoff! */
	//ATARecordEventMACRO(kAIMTapeName,' dma','true','StDM');
	_dmaIntExpected = true;
	
	// IODBDMAStart will flush the FIFO by clearing the run-bit, causing multiple chain execution 
	// to fail by losing whatever bytes may have accumulated in the ATA fifo.
	
	//IODBDMAStart(_dmaControlReg, (volatile IODBDMADescriptor *)_descriptorsPhysical);
	IOSetDBDMAChannelControl(_dmaControlReg, IOSetDBDMAChannelControlBits( kdbdmaRun | kdbdmaWake));
}

/*---------------------------------------------------------------------------
 *
 *	create the DMA channel commands.
 *
 *
 ---------------------------------------------------------------------------*/
IOReturn
AppleKauaiATA::createChannelCommands(void)
{
	IOMemoryDescriptor* descriptor = _currentCommand->getBuffer();

	if( ! descriptor )
	{
	
		DLOG("AppleKauaiATA nil DMA buffer!\n");
		return -1;
	}

	// calculate remaining bytes in this transfer

	IOByteCount bytesRemaining = _currentCommand->getByteCount() 
								- _currentCommand->getActualTransfer();



	// calculate position pointer
	IOByteCount xfrPosition = _currentCommand->getPosition() + 
							_currentCommand->getActualTransfer();

	IOByteCount  transferSize = 0; 

	// have the DMA cursor fill out the addresses in the CC table
	// it will return the number of descriptors consumed.

	UInt32 segmentCount = _DMACursor->getPhysicalSegments(
										descriptor,
				       					xfrPosition,
				       					_descriptors,
				     					kATAXferDMADesc,
				     					bytesRemaining,  // limit to the requested number of bytes in the event the descriptors is larger
				       					&transferSize);
				       					
	if( transferSize > bytesRemaining)
	{
		DLOG("AppleKauaiATA DMA too long!!!\n");
		return -1;	
	
	}

	if( segmentCount == 0)
	{
		DLOG("AppleKauaiATA seg count 0!!!\n");
		return -1;	
	
	}


	// check if the xfer satisfies the needed size
	if( transferSize < bytesRemaining )
	{
		// indicate we need to do more DMA when the interrupt happens
	
		_dmaState = kATADMAActive;
		DLOG("AppleKauaiATA will make two passes\n");
	
	} else {
		
		// transfer is satisfied and only need to check status on interrupt.
		_dmaState = kATADMAStatus;
		DLOG("AppleKauaiATA will make one pass\n");
	
	}

	UInt32 command = kdbdmaOutputMore;

	if( _currentCommand->getFlags() & mATAFlagIORead)
	{
		command = kdbdmaInputMore;
	
	}

	DLOG("AppleKauaiATA making CC chain for %ld segs for xfersize %ld\n", segmentCount, transferSize);

	// now walk the descriptor chain and insert the commands
	for( UInt32 i = 0; i < segmentCount; i++)
	{
	
		IODBDMADescriptor* currDesc = & (_descriptors[i]);
		
		UInt32 addr = IOGetCCAddress(currDesc);
		UInt32 count =  IOGetCCOperation(currDesc) & kdbdmaReqCountMask;
		OSSynchronizeIO();

		// test if this is the last DMA command in this set and set the command to output last with different flags
		if( (i == (segmentCount -1) ) )
		{
			if( command == kdbdmaOutputMore) 
			{	
				command = kdbdmaOutputLast;
			
			} else {
				
				command = kdbdmaInputLast;
			}
			
			// make a descriptor with the xxxxLast command, wait for s7 and int always. 
			IOMakeDBDMADescriptor(currDesc,
								command,
								kdbdmaKeyStream0,
								kdbdmaIntNever,
								kdbdmaBranchNever,
								kdbdmaWaitIfTrue,
								count,
								addr);
				
						
					
		} else {


			IOMakeDBDMADescriptor(currDesc,
								command,
								kdbdmaKeyStream0,
								kdbdmaIntNever,
								kdbdmaBranchNever,
								kdbdmaWaitNever,
								count,
								addr);
		
		
		} //  comment out for running test 2
		
		DLOG("AppleKauaiATA desc# %ld at %x \n", i, currDesc );
		DLOG("addr = %lx  count = %lx  \n", addr, count );
		
		DLOG("%lx  %lx  %lx  %lx\n", currDesc->operation, currDesc->address ,currDesc->cmdDep ,currDesc->result );
		
	}

	// insert a NOP after the last data command
	IOMakeDBDMADescriptor(&(_descriptors[segmentCount]),
						kdbdmaNop,
						kdbdmaKeyStream0,
						kdbdmaIntAlways,
						kdbdmaBranchNever,
						kdbdmaWaitNever,
						0,
						0);




	// insert a stop after the NOP command
	IOMakeDBDMADescriptor(&(_descriptors[segmentCount + 1]),
						kdbdmaStop,
						kdbdmaKeyStream0,
						kdbdmaIntNever,
						kdbdmaBranchNever,
						kdbdmaWaitNever,
						0,
						0);


	// chain is now ready for execution.

	return kATANoErr;

}


#pragma mark -- Polled Interface --

 /*---------------------------------------------------------------------------*/
#ifdef __KAUAI_POLLED__
/*--------------------------------
*	Polled mode implementation
*  All stand-alone methods for polled mode are here. Note that some polled 
*  mode initialization is implemented in the probe and start methods.
*
----------------------------------*/


IOReturn 
AppleKauaiATA::startTimer( UInt32 inMS)
{

	if( polledAdapter )
	{
	
		if(polledAdapter->isPolling())
			return 0;
	
	}
	
	return super::startTimer( inMS);

}

/*---------------------------------------------------------------------------
 *
 *	Kill a running timer.
 *
 *
 ---------------------------------------------------------------------------*/
void
AppleKauaiATA::stopTimer(void)
{
	if( polledAdapter )
	{
	
		if(polledAdapter->isPolling())
			return ;
	
	}
	
	return super::stopTimer( );

}

// polled mode is called at a time when hardware interrupts are disabled.
// this is a poll-time procedure, when given a slice of time, the poll proc
// checks the state of the hardware to see if it has a pending interrupt status
// and calls the interrupt handlers in place of the interrupt event source.
void 
AppleKauaiATA::pollEntry( void )
{

	// make sure there is a current command before processing further.
	if( 0 == _currentCommand )
		return;
		
	// check the int status in hardware
	UInt32 intStatus = OSReadLittleInt32( (void*) _interruptPendingReg, 0x00);
	
	// if the command is expecting a DMA interrupt as well as device interrupt
	// wait until both interrupts are asserted before processing. 
	if( _dmaIntExpected)
	{
		if( (intStatus & 0xC0000000) == 0xC0000000 )
		{
			processDMAInterrupt();
			handleDeviceInterrupt();
		}
		return;
	}
	
		
	// if the DMA interrupt is not expected but a command is in process, 
	// then complete the pending IRQ status. 
	if( intStatus & 0x40000000 )
	{
		handleDeviceInterrupt();
		return;
	}

	return;
}

void 
AppleKauaiATA::transitionFixup( void )
{

		// ivars working up the chain of inheritance:
		_needsResync = false;
		bufferRX = false;
		clientBuffer = 0;
		
		// from MacIOATA
		_dmaIntExpected = 0;
		_dmaState = MacIOATA::kATADMAInactive;
		_resyncInterrupts = false;
		isBusOnline = true;
		
		// from IOATAController		
		_queueState = IOATAController::kQueueOpen;
		_busState = IOATAController::kBusFree;
		_currentCommand = 0L;
		_selectedUnit = kATAInvalidDeviceID;
		_queueState = IOATAController::kQueueOpen;
		_immediateGate = IOATAController::kImmediateOK;

		// make sure the hardware is running
		((IOPCIDevice *)myProvider)->restoreDeviceState();
		
}

#endif

//---------------------------------------------------------------------------
// end of AppleKauaiATA
// --------------------------------------------------------------------------


#ifdef __KAUAI_POLLED__
#undef super
//---------------------------------------------------------------------------
// begin of KauaiPolledAdapter
// --------------------------------------------------------------------------

#define super IOPolledInterface

OSDefineMetaClassAndStructors(  KauaiPolledAdapter, IOPolledInterface )



IOReturn 
KauaiPolledAdapter::probe(IOService * target)
{
	pollingActive = false;
	return kIOReturnSuccess;
}

IOReturn 
KauaiPolledAdapter::open( IOOptionBits state, IOMemoryDescriptor * buffer)
{

	switch( state )
	{
		case kIOPolledPreflightState:
			// nothing to do here for this controller
		break;
		
		case kIOPolledBeforeSleepState:
			pollingActive = true;
		break;
		
		case kIOPolledAfterSleepState:
			// ivars may be corrupt at this time. Kernel space is restored by bootx, then executed. 
			// ivars may be stale depending on the when the image snapshot took place during image write
			// call the controller to return the ivars to a queiscent state and restore the pci device state.
			owner->transitionFixup();
			pollingActive = true;

		break;	
		
		
		case kIOPolledPostflightState:
			// illegal value should not happen. 
		default:	
		break;
	}


	return kIOReturnSuccess;
}


IOReturn 
KauaiPolledAdapter::close(IOOptionBits state)
{


	switch( state )
	{
		case kIOPolledPreflightState:
		case kIOPolledBeforeSleepState:
		case kIOPolledAfterSleepState:
		case kIOPolledPostflightState:
		default:
			pollingActive = false;	
		break;
	}


	return kIOReturnSuccess;
}

IOReturn 
KauaiPolledAdapter::startIO(uint32_t 	        operation,
                             uint32_t           bufferOffset,
                             uint64_t	        deviceOffset,
                             uint64_t	        length,
                             IOPolledCompletion completion)
{




	return kIOReturnUnsupported;
}

IOReturn 
KauaiPolledAdapter::checkForWork(void)
{

	if( owner )
	{
		owner->pollEntry();
	}

	return kIOReturnSuccess;
}


bool 
KauaiPolledAdapter::isPolling( void )
{

	return pollingActive;
}

void
KauaiPolledAdapter::setOwner( AppleKauaiATA* myOwner )
{

	owner = myOwner;
	pollingActive = false;
}

#endif
