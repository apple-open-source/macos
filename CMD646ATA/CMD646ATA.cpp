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
 *	CMD646ATA.cpp
 *	
 *	Defines the concrete driver for a cmd646-ata controller.
 *	Descendent of IOPCIATA, which is derived from IOATAController.
 *
 */
 
#include "CMD646ATA.h"
#include "CMD646Root.h"
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

#include <IOKit/pci/IOPCIDevice.h>
#include <libkern/OSByteOrder.h>
#include <libkern/OSAtomic.h>


#ifdef DLOG
#undef DLOG
#endif

//#define ATA_DEBUG 1

#ifdef  ATA_DEBUG
#define DLOG(fmt, args...)  IOLog(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

// some day, we'll have an ATA recorder for IOKit

#define ATARecordEventMACRO(type,param,bus,data) 		(void) (type); (void) (param); (void) (bus); (void) (data)

#define kCompatibleString 	"cmd646-ata"
#define kModelPropertyKey 	"name"
#define kModel4String 		"ata-4"


/* Bit-significant representation of supported modes. */ 
#define kATASupportedPIOModes		 0x001F	// modes 4, 3, 2, 1, and 0
#define kATASupportedMultiDMAModes	 0x0007	// modes 2, 1, and 0
#define	kATASupportedUltraDMAModes	 0x0007	// modes  2, 1, and 0

/* Number of entries in various tables */
#define kPIOCycleEntries			12
#define kMultiDMACycleEntries 		9
#define	kUltraDMACycleEntries		3


// ------ ¥¥¥ Maximum transfer modes the CMD646 controller is capable of supporting ¥¥¥ ÐÐÐÐÐÐÐÐ
#define kATAMaxPIOMode		 	4
#define kATAMaxMultiDMAMode		2
#define	kATAMaxUltraDMAMode 	2



#pragma mark -IOService Overrides -


//---------------------------------------------------------------------------

#define super IOPCIATA
OSDefineMetaClassAndStructors ( CMD646ATA, IOPCIATA )

//---------------------------------------------------------------------------

bool 
CMD646ATA::init(OSDictionary* properties)
{
	DLOG("CMD646ATA init start\n");

	for( int i = 0; i < 3; i++)
	{

		ioBaseAddrMap[ i ] = 0;
	
	}


    // Initialize instance variables.
   
    if (super::init(properties) == false)
    {
        DLOG("CMD646ATA: super::init() failed\n");
        return false;
    }
	

	DLOG("CMD646ATA init done\n");

    return true;
}

/*---------------------------------------------------------------------------
 *
 *	Check and see if we really match this device.
 * override to accept or reject close matches based on further information
 ---------------------------------------------------------------------------*/

IOService* 
CMD646ATA::probe(IOService* provider,	SInt32*	score)
{

    OSData		*compatibleEntry;
	
	DLOG("CMD646ATA starting probe\n");


	compatibleEntry  = OSDynamicCast( OSData, provider->getProperty( "compatible" ) );
	if ( compatibleEntry == 0 ) 
	{
		// error unknown controller type.

		DLOG("CMD646ATA failed getting compatible property\n");
		return 0;

	}

	
	// test the compatible property for a match to the controller type name.
	if ( compatibleEntry->isEqualTo( kCompatibleString, sizeof(kCompatibleString)-1 ) == false ) 
	{
		// not our type of controller		
		DLOG("CMD646ATA compatible property doesn't match\n");
		return 0;
		
	}


	// do a little initialization here once the probe is succesful so we can start clean.

	OSData		*registryEntry;
	registryEntry = OSDynamicCast( OSData, provider->getProperty( kModelPropertyKey ) );
	if( registryEntry == 0)
	{
		DLOG("CMD646ATA unknown model property.\n");
		return 0;
	}

	
	// initialize the timing params to PIO mode 0 at 600ns and MWDMA mode 0 at 480ns
	
	busTimings[0].ataPIOSpeedMode = busTimings[1].ataPIOSpeedMode = 0x01 ;				// PIO Mode Timing class (bit-significant)
	busTimings[0].ataPIOCycleTime = busTimings[1].ataPIOCycleTime = 600 ;				// Cycle time for PIO mode
	busTimings[0].ataMultiDMASpeed = busTimings[1].ataMultiDMASpeed = 0x01;				// Multiple Word DMA Timing Class (bit-significant)
	busTimings[0].ataMultiCycleTime = busTimings[1].ataMultiCycleTime = 480;				// Cycle time for Multiword DMA mode
	busTimings[0].ataUltraDMASpeedMode = busTimings[1].ataUltraDMASpeedMode = 0x00;				// UltraDMA Timing Class (bit-significant)

	
	busTimings[0].pioActiveRecoveryValue = busTimings[1].pioActiveRecoveryValue = 0x6d;
	busTimings[0].dmaActiveRecoveryValue = busTimings[1].dmaActiveRecoveryValue = 0x88;
	busTimings[0].ultraTimingValue = busTimings[1].ultraTimingValue = 0x00;
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
CMD646ATA::start(IOService *provider)
{

    DLOG("CMD646ATA::start() begin\n");
    
 	CMD646Device* myProvider = OSDynamicCast( CMD646Device, provider);
	
	if( myProvider == 0)
	{
	
		DLOG("CMD646 provider not CMD646 Device nub\n");
		return 0;
	}

	_cmdRoot = myProvider->getRootCMD();
	_pciNub = (IOPCIDevice*) myProvider->getPCINub();
	
	// enable access to the device.
	//_pciNub->configWrite32( 0x04, 0x05 );

	if( ! provider->open(this) )
	{
		DLOG("CMD646ATA provider did not open\n");
		return false;	
	}


	ATADeviceNub* newNub=0L;
 
 	// call start on the superclass
    if (!super::start( provider))
 	{
        DLOG("CMD646ATA: super::start() failed\n");
        provider->close(this);
        return false;
	}
	
		// Find the interrupt source and attach it to the command gate
	
	if( ! createDeviceInterrupt() )
	{
        DLOG("CMD646ATA:  createDeviceInterrupts failed\n");
		return false;	
	
	}


    DLOG("CMD646ATA::start() done\n");

	for( UInt32 i = 0; i < 2; i++)
	{
		if( _devInfo[i].type != kUnknownATADeviceType )
		{
		
			DLOG("CMD646ATA creating nub\n");
			newNub = ATADeviceNub::ataDeviceNub( (IOATAController*)this, (ataUnitID) i, _devInfo[i].type );
		
			if( newNub )
			{
				
				DLOG("CMD646ATA attach nub\n");
				
				newNub->attach(this);
			
				_nub[i] = (IOATADevice*) newNub;
				
				DLOG("CMD646ATA register nub\n");

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
CMD646ATA::free()
{
	for( int i = 0; i < 3; i++)
	{
		if( ioBaseAddrMap[i] )
			ioBaseAddrMap[ i ]->release();
	
	}
	
	super::free();


}


/*---------------------------------------------------------------------------
 *
 *	connect the device (drive) interrupt to our workloop
 *
 *
 ---------------------------------------------------------------------------*/
bool
CMD646ATA::createDeviceInterrupt(void)
{
	// create a device interrupt source and attach it to the work loop
	
	DLOG("CMD646ATA createDeviceInterrupt started\n");
	
	_devIntSrc = IOFilterInterruptEventSource::filterInterruptEventSource( 	
					(OSObject *)this,
					(IOInterruptEventSource::Action) &CMD646ATA::sDeviceInterruptOccurred, 
					(IOFilterInterruptEventSource::Filter) &CMD646ATA::sFilterInterrupt,
					_pciNub, 
					0); 
	
	DLOG("CMD646ATA createdDeviceInterruptsource = %x\n", _devIntSrc);
	DLOG("_workLoop = %x\n", _workLoop);
	
	if( !_devIntSrc || getWorkLoop()->addEventSource(_devIntSrc) )
	{
		DLOG("CMD646ATA failed create dev intrpt source\n");
		return false;
	}
		
	_devIntSrc->enable();
	
	DLOG("CMD646ATA createDeviceInterrupt done\n");
	
	return true;

}

/*---------------------------------------------------------------------------
 *
 *  static "action" function to connect to our object
 *
 ---------------------------------------------------------------------------*/
void 
CMD646ATA::sDeviceInterruptOccurred(OSObject * owner, IOInterruptEventSource *evtSrc, int count)
{
	CMD646ATA* self = (CMD646ATA*) owner;

	self->handleDeviceInterrupt();


}

/*---------------------------------------------------------------------------
 *
 *  static "action" function to connect to our object
 *
 ---------------------------------------------------------------------------*/

bool
CMD646ATA::sFilterInterrupt(OSObject *owner, IOFilterInterruptEventSource *src)
{
	CMD646ATA* self = (CMD646ATA*) owner;
	
	return self->interruptIsValid( src );

}
/*---------------------------------------------------------------------------
 *
 *  interruptIsValid - used in a filter interrupt event source - validates 
 *	the interrupt is indeed for this device before scheduling a thread 
 *
 ---------------------------------------------------------------------------*/

bool 
CMD646ATA::interruptIsValid( IOFilterInterruptEventSource* )
{

	UInt8 modeBits = *_mrdModeReg;

	
	if( ( modeBits & 0x04 ) )
	{
		*_mrdModeReg = 0x3C;	// stop interrupt propogation.	
		OSSynchronizeIO();
		return true;
	}

	return false;
}


/*---------------------------------------------------------------------------
 *
 *	handleDeviceInterrupt - overriden here so we can make sure that the DMA has
 * processed in the event first.
 *
 ---------------------------------------------------------------------------*/
IOReturn
CMD646ATA::handleDeviceInterrupt(void)
{	

	// super class clears the interrupt request by reading the status reg
	IOReturn result = super::handleDeviceInterrupt();

	*_mrdModeReg = 0x20; // re-enable interrupt propogation.
	OSSynchronizeIO();
	
	return result;	
	
}



/*---------------------------------------------------------------------------
 *	get the work loop. If not already created, make one.
 *
 ---------------------------------------------------------------------------*/
IOWorkLoop*
CMD646ATA::getWorkLoop() const
{

	DLOG("CMD646ATA::getWorkLoop\n");

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
 *	If there's a timeout, read the drive's status reg to clear any pending
 *	interrupt, then clear the interrupt bit and enable inta# propogation in the
 *	controller.
 *
 ---------------------------------------------------------------------------*/
void
CMD646ATA::handleTimeout( void )
{

	*_mrdModeReg = 0x3C;	// stop interrupt propogation.	
	OSSynchronizeIO();

	volatile UInt8 statusByte = *_tfStatusCmdReg;
	OSSynchronizeIO();

	statusByte++;		// make sure the compiler doesn't drop this.
	
	*_mrdModeReg = 0x20; // re-enable interrupt propogation.
	OSSynchronizeIO();


	super::handleTimeout();

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
CMD646ATA::configureTFPointers(void)
{
    DLOG("CMD646ATA config TF Pointers \n");


    ioBaseAddrMap[0] = _pciNub->mapDeviceMemoryWithRegister( kPrimaryCmd );
    if ( ioBaseAddrMap[0] == NULL ) 
    { 
    	return false;
	}	

    volatile UInt8* baseAddress = (volatile UInt8*)ioBaseAddrMap[0]->getVirtualAddress();
	
	_tfDataReg = (volatile UInt16*) (baseAddress + 0x00);

	_tfFeatureReg = baseAddress + 0x01;
	_tfSCountReg = baseAddress + 0x02;
	_tfSectorNReg = baseAddress + 0x03;
	_tfCylLoReg = baseAddress + 0x04;
	_tfCylHiReg = baseAddress + 0x05;
	_tfSDHReg = baseAddress + 0x06;
	_tfStatusCmdReg = baseAddress + 0x07;  

	 DLOG("CMD646ATA base address 0 = %lX \n", baseAddress);
	 	 

	// get the address of the alt-status register from the second base address.

    ioBaseAddrMap[1] = _pciNub->mapDeviceMemoryWithRegister( kPrimaryCntrl );
    if ( ioBaseAddrMap[1] == NULL )
    {
    	 return false;
    }
    
    baseAddress = (volatile UInt8 *)ioBaseAddrMap[1]->getVirtualAddress();

 	_tfAltSDevCReg = baseAddress + 2;   

	 DLOG("CMD646ATA base address 1 = %lX altStatus at %lx \n", baseAddress, _tfAltSDevCReg);
	 


	// get the address of the BusMaster/DMA control registers from last base address.
    ioBaseAddrMap[2] = _pciNub->mapDeviceMemoryWithRegister( kBusMaster );
    if ( ioBaseAddrMap[2] == NULL )
    {
    	 return false;
    }

    volatile UInt8* bmAddress = (volatile UInt8*)ioBaseAddrMap[2]->getVirtualAddress();

	 DLOG("CMD646ATA base address 2 = %lX \n", bmAddress);

	_bmCommandReg = bmAddress;
	_bmStatusReg = bmAddress + 2;
	_bmPRDAddresReg = (volatile UInt32*) (bmAddress + 4);	
	_mrdModeReg = bmAddress + 1;
	_udideTCR0 = bmAddress + 3;

	// set address setup time
	// bus 0 dev 1  (bus 1 is not used in Apple implementation)
	UInt8 scratchByte = _pciNub->configRead8( kARTTIM0);
	_pciNub->configWrite8(  kARTTIM0, scratchByte & 0x3f | 0x40);
	busTimings[0].pioAddrSetupValue = _pciNub->configRead8( 0x53);
	// setup PIO mode zero
	_pciNub->configWrite8(  kDRWTIM0, busTimings[0].pioActiveRecoveryValue );

	// bus 0 dev 1
	scratchByte = _pciNub->configRead8( kARTTIM1);
	_pciNub->configWrite8(  kARTTIM1, scratchByte & 0x3f | 0x40);
	busTimings[1].pioAddrSetupValue = _pciNub->configRead8( kARTTIM1);
	// setup PIO mode zero
	_pciNub->configWrite8(  kDRWTIM1, busTimings[1].pioActiveRecoveryValue );
	
	currentActiveRecoveryValue[0] = busTimings[0].pioActiveRecoveryValue;
	currentActiveRecoveryValue[1] = busTimings[1].pioActiveRecoveryValue;


   DLOG("CMD646ATA configTFPointers done\n");
	
	return true;

}

/*---------------------------------------------------------------------------
 *	provide information on the bus capability 
 *
 *
 ---------------------------------------------------------------------------*/
IOReturn 
CMD646ATA::provideBusInfo( IOATABusInfo* infoOut)
{
	if( infoOut == 0)
	{
		DLOG("CMD646ATA nil pointer in provideBusInfo\n");
		return -1;
	
	}


	infoOut->zeroData();
	
	// BUG - need to find out if this bus is a client of a media bay.
	
	infoOut->setSocketType( kInternalATASocket ); // internal fixed, media-bay, PC-Card
	
	
	infoOut->setPIOModes( kATASupportedPIOModes);		
	infoOut->setDMAModes( kATASupportedMultiDMAModes );			
	infoOut->setUltraModes( kATASupportedUltraDMAModes );
		
	
	UInt8 units = 0;
	
	if(	_devInfo[0].type != kUnknownATADeviceType )
		units++;
		
	if(	_devInfo[1].type != kUnknownATADeviceType )
		units++;

	infoOut->setUnits( units);

	if( units > 1
		&& _pciNub->configRead8( 0x08 ) == 0x05 )
	{
		infoOut->setUltraModes( 0x00 );
	}

	return kATANoErr;
}


/*---------------------------------------------------------------------------
 *	return the currently configured timing for the unit number 
 *
 *
 ---------------------------------------------------------------------------*/

IOReturn 
CMD646ATA::getConfig( IOATADevConfig* configRequest, UInt32 unitNumber)
{

	// param check the pointers and the unit number.
	
	if( configRequest == 0
		|| unitNumber > 1 )
		
	{
	
		DLOG("CMD646ATA bad param in getConfig\n");
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
CMD646ATA::selectConfig( IOATADevConfig* configRequest, UInt32 unitNumber)
{


	// param check the pointers and the unit number.
	
	if( configRequest == 0
		|| unitNumber > 1 )
		
	{
	
		DLOG("CMD646ATA bad param in setConfig\n");
		return -1;	
	}



	// all config requests must include a PIO mode
	if( ( configRequest->getPIOMode() & kATASupportedPIOModes ) == 0x00 )
	{
		DLOG("CMD646ATA setConfig PIO mode not supported\n");
		return kATAModeNotSupported;		
	
	}


	// DMA is optional - not all devices support it, we reality check for a setting
	// that is beyond our capability


	// no ultra on unless this is the ultra cell
	UInt16 ultraSupported = kATASupportedUltraDMAModes;
	
	// make sure a requested ultra ATA mode is within the range of this device configuration

	if( configRequest->getUltraMode() & ~ultraSupported )
	{
		DLOG("CMD646ATA setConfig no ultra\n");
		return kATAModeNotSupported;	
		
	} 



	if( configRequest->getDMAMode() & ~kATASupportedMultiDMAModes )
	{
		DLOG("CMD646ATA setConfig DMA mode not supported\n");
		return kATAModeNotSupported;		
	
	} 

	if( configRequest->getDMAMode() > 0x0000
		&& configRequest->getUltraMode() > 0x0000 )
	{
		DLOG("CMD646ATA err, only one DMA class allowed in config select\n");
		return kATAModeNotSupported;
	
	}

	_devInfo[unitNumber].packetSend = configRequest->getPacketConfig();
	DLOG("CMD646ATA setConfig packetConfig = %ld\n", _devInfo[unitNumber].packetSend );

	
	return selectIOTimerValue(configRequest, unitNumber);

}


/*---------------------------------------------------------------------------
 *	calculate the timing configuration as requested.
 *
 *
 ---------------------------------------------------------------------------*/

IOReturn
CMD646ATA::selectIOTimerValue( IOATADevConfig* configRequest, UInt32 unitNumber)
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
static const UInt8 PIOCycleValue[kPIOCycleEntries] = 
					{
						0x00,						// Hardware minimum		(960 ns = 480 rec + 480 acc)
						0x6d,						// Minimum PIO mode 0 	(600 ns = 420 rec + 180 acc)	 
						0x57,						// Minimum PIO mode 1	(390 ns = 240 rec + 150 acc)	 
						0x65,						// Derated PIO Mode 2/3/4	(360 ns = 180 rec + 180 acc)	 
						0x55,						// Derated PIO Mode 2/3/4	(330 ns = 180 rec + 150 acc)	 
						0x54,						// Derated PIO Mode 2/3/4	(300 ns = 150 rec + 150 acc)	 
						0x44,						// Derated PIO mode 2/3/4	(270 ns = 150 rec + 120 acc)	 
						0x44,						// Minimum PIO mode 2 		(240 ns = 150 rec + 120 acc)	 
						0x33,						// Derated PIO Mode 3/4 	(210 ns = 120 rec + 90 acc)		 
						0x32,						// Minimum PIO mode 3		(180 ns = 90 rec + 90 acc)
						0x31,						// Derated PIO mode 4		(150 ns = 60 rec + 90 acc)
						0x3F						// Minimum PIO mode 4		(120 ns = 30 rec + 90 acc)	 
 					};




static const UInt16 PIOCycleTime[ kPIOCycleEntries ]=
					{
						960,						// Hardware maximum			(0)
						600,						// Minimum PIO mode 0 		(1)	 
						390,						// Minimum PIO mode 1		(2)	 
						360,						// Derated PIO Mode 2/3/4	(3) 	 
						330,						// Derated PIO Mode 2/3/4	(4) 	 
						300,						// Derated PIO Mode 2/3/4	(5) 	 
						270,						// Derated PIO mode 2/3/4	(6)	 
						240,						// Minimum PIO mode 2 		(7)	 
						210,						// Derated PIO Mode 3/4 	(8)		 
						180,						// Minimum PIO mode 3		(9)
						150,						// Derated PIO mode 4		(10)
						120,						// Minimum PIO mode 4		(11)	 
					};


// table of DMA cycle times and the corresponding binary value to reach that number.
static const UInt8 MultiDMACycleValue[kMultiDMACycleEntries] =
				{
					0x00, 					// Hardware maximum     (960=  480rec+480acc)	 
					0x88, 					// Minimum Multi mode 0  (510=  270rec+240acc)	 
					0x65, 					// Derated mode 1 or 2   (360=  180rec+180acc)	 
					0x44, 					// Derated mode 1 or 2   (270=  150rec+120acc)	 
					0x43, 					// Derated mode 1 or 2   (240=  120rec+120acc)	 
					0x42, 					// Derated mode 1 or 2   (210=   90rec+120acc)	 
					0x32, 					// Derated mode 1 or 2   (180=   90rec+90acc)	 
					0x31, 					// Minimum Multi mode 1  (150=   60rec+90acc)	 
					0x3F	 				// Minimum Multi mode 2  (120=   30rec+90acc)
				}; 



static const UInt16 MultiDMACycleTime[kMultiDMACycleEntries] =
				{
					960, 						// Hardware maximum     	(0) 
					480, 						// Minimum Multi mode 0  (1)	 
					360, 						// Derated mode 1 or 2   (2)	 
					270, 						// Derated mode 1 or 2   (3)	 
					240, 						// Derated mode 1 or 2   (4)	 
					210, 						// Derated mode 1 or 2   (5)	 
					180, 						// Derated mode 1 or 2   (6)	 
					150, 						// Minimum Multi mode 1  (7)	 
					120 						// Minimum Multi mode 2  (8)	 
				};

				


static const UInt8 UltraDMADev0CycleValue[kUltraDMACycleEntries] =
				{
					0x31,						// mode 0/hardware maximum (120)
					0x21,						// mode 1 (90)
					0x11						// mode 2 (60)
				};


static const UInt8 UltraDMADev1CycleValue[kUltraDMACycleEntries] =
				{
					0xC2,						// mode 0/hardware maximum (120)
					0x82,						// mode 1 (90)
					0x42						// mode 2 (60)
				};
				
static const UInt16 UltraDMACycleTime[kUltraDMACycleEntries] =
				{
					120,						// mode 0
					90,							// mode 1
					60,							// mode 2
				};
				
/*-----------------------------------------*/






	UInt8 pioConfigBits = PIOCycleValue[0];

	// the theory is simple, just examine the requested mode and cycle time, find the 
	// entry in the table which is closest, but NOT faster than the requested cycle time.

	// convert the bit maps into numbers
	UInt32 pioModeNumber = bitSigToNumeric( configRequest->getPIOMode());

	// check for out of range values.
	if( pioModeNumber > kATAMaxPIOMode )
	{
		DLOG("CMD646ATA pio mode out of range\n");
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
	for( int i = kPIOCycleEntries - 1; i >= 0; i--)
	{
	
		if( pioCycleTime <= PIOCycleTime[ i ] )
		{
			// found the fastest time which is not greater than
			// the requested time.
			pioConfigBits = PIOCycleValue[i];;
			break;
		}
	}
	

	// now do the same for DMA modes.
	UInt32 dmaConfigBits = MultiDMACycleValue[0];
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
		for( int i = kMultiDMACycleEntries - 1; i >= 0; i--)
		{
		
			if( dmaCycleTime <= MultiDMACycleTime[ i ] )
			{
				// found the fastest time which is not greater than
				// the requested time.
				dmaConfigBits = MultiDMACycleValue[i];
				break;
			}
		}
	}

	UInt8 ultraConfigBits = 0;
	UInt32 ultraModeNumber = 0;
	// same deal for ultra ATA modes
	if( configRequest->getUltraMode() )
	{
		ultraModeNumber = bitSigToNumeric( configRequest->getUltraMode() );
	
		ultraConfigBits = unitNumber == 0 ? UltraDMADev0CycleValue[ ultraModeNumber ] : UltraDMADev1CycleValue[ ultraModeNumber ];
	
	
	}

	// now combine the bits forming the configuration and put them
	// into the timing structure along with the mode and cycle time
	
	busTimings[unitNumber].pioActiveRecoveryValue = pioConfigBits;
	busTimings[unitNumber].dmaActiveRecoveryValue = dmaConfigBits;
	busTimings[unitNumber].ultraTimingValue = ultraConfigBits;
	
	busTimings[unitNumber].ataPIOSpeedMode = configRequest->getPIOMode();
	busTimings[unitNumber].ataPIOCycleTime = pioCycleTime;
	busTimings[unitNumber].ataMultiDMASpeed = configRequest->getDMAMode();
	busTimings[unitNumber].ataMultiCycleTime = dmaCycleTime;
	busTimings[unitNumber].ataUltraDMASpeedMode = configRequest->getUltraMode();

	DLOG("CMD646ATA PIO mode %x at %ld ns hex= %x selected for device: %x\n", (int)pioModeNumber, pioCycleTime, busTimings[unitNumber].pioActiveRecoveryValue, (int)unitNumber);
	DLOG("CMD646ATA DMA mode %x at %ld ns hex= %x selected for device: %x\n", (int)dmaModeNumber, dmaCycleTime,busTimings[unitNumber].dmaActiveRecoveryValue, (int)unitNumber);
	DLOG("CMD646ATA Ultra mode %x at %ld ns selected for device: %x\n", (int)ultraModeNumber, UltraDMACycleTime[ultraModeNumber], (int)unitNumber);
	DLOG("CMD646ATA Ultra cycle value = %x for unit: %x\n", 	busTimings[unitNumber].ultraTimingValue, unitNumber);

	_devIntSrc->disable();

	// set the UDMA mode
	*_udideTCR0 = busTimings[0].ultraTimingValue | busTimings[1].ultraTimingValue;
	OSSynchronizeIO();

	_devIntSrc->enable();

	// stuff the values back into the request structure and return result
	return getConfig( configRequest, unitNumber);

}



// punch the correct value into the IO timer register for the selected unit.
void 
CMD646ATA::selectIOTiming( ataUnitID unit )
{
	_devIntSrc->disable();
	_devIntSrc->enable();

	//DLOG("CMD646ATA setting timing config %lx on unit %d\n", busTimings[unit].cycleRegValue, unit);

	if( _currentCommand->getFlags() & mATAFlagUseDMA 
		&& busTimings[unit].ataUltraDMASpeedMode != 0 )
	{
		
		// set the UDMA mode
//		*_udideTCR0 = busTimings[0].ultraTimingValue | busTimings[1].ultraTimingValue;
		OSSynchronizeIO();
		return;
	}




	UInt8 regOffset = unit == 0 ? kDRWTIM0 : kDRWTIM1;
	UInt8 value = busTimings[unit].pioActiveRecoveryValue;

	if( _currentCommand->getFlags() & mATAFlagUseDMA)
	{
		// we need to set a mwdma mode.
		value = busTimings[unit].dmaActiveRecoveryValue;	
	}

	if( value == currentActiveRecoveryValue[ unit ] )
	{
		// already setup
		return;
		
	}
	_devIntSrc->disable();

	_pciNub->configWrite8( regOffset , value );

	currentActiveRecoveryValue[ unit ] = value;
	
	_devIntSrc->enable();

}




