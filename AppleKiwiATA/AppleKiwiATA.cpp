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
 *	AppleKiwiATA.cpp
 *	
 *	Defines the concrete driver for a AppleKiwiATA controller.
 *	Descendent of IOPCIATA, which is derived from IOATAController.
 *
 */

 
#include "AppleKiwiATA.h"
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

#define kCompatibleString 	"kiwi-ata"
#define kModelPropertyKey 	"name"
#define kModel4String 		"ata-6"

#define kPCIInlineKey		"pci_inline"
#define kMode6Key			"mode6"

/* Bit-significant representation of supported modes. */
// modes 3 PIO only 
#define kATASupportedPIOModes		 0x0008	
// modes multi-word mode 2 only on this bus.
#define kATASupportedMultiDMAModes	 0x0004	
// modes  5,4,2 only ( 100, 66, 33) 
#define	kATASupportedUltraDMAModes	 0x0034
// used to OR mode 6 on PDC 271
#define kATAAddSupport133	 		 0x0074 

// NOTE:  The following constants are already in Little Endian format. 
// timing constant for pio-3 and mw mode 2 constant for timing register 0 in 133 pll mode. 
#define kPartATimingLE				0x270d6925
// define timing constants for all the ultra ATA modes
#define kPartBTiming33LE			0x2a07cd35
#define kPartBTiming66LE			0x1a03cd35
#define kPartBTiming100LE			0x1a02cb35
#define kPartBTiming133LE			0x1a01cb35


#pragma mark -IOService Overrides -


//---------------------------------------------------------------------------

#define super IOPCIATA
OSDefineMetaClassAndStructors ( AppleKiwiATA, IOPCIATA )

//---------------------------------------------------------------------------

bool 
AppleKiwiATA::init(OSDictionary* properties)
{
	DLOG("AppleKiwiATA init start\n");

	for( int i = 0; i < 4; i++)
	{

		ioBaseAddrMap[ i ] = 0;
	
	}
	_devIntSrc = 0;
	busChildNumber = 0;
   	isBusOnline = true;
	chipRoot = 0;
	forcePCIInline = true;
	mode6Capable = false;
	reconfigureTiming = false; // flag that a set-feature command has been executed and the 
	// pdc271 timing registers need to be reconfigured to override their hardware snooping feature. 
    if (super::init(properties) == false)
    {
        DLOG("AppleKiwiATA: super::init() failed\n");
        return false;
    }
	

	DLOG("AppleKiwiATA init done\n");

    return true;
}

/*---------------------------------------------------------------------------
 *
 *	Check and see if we really match this device.
 * override to accept or reject close matches based on further information
 ---------------------------------------------------------------------------*/

IOService* 
AppleKiwiATA::probe(IOService* provider,	SInt32*	score)
{

    OSData		*compatibleEntry;
	
	DLOG("AppleKiwiATA starting probe\n");


	compatibleEntry  = OSDynamicCast( OSData, provider->getProperty( "compatible" ) );
	if ( compatibleEntry == 0 ) 
	{
		// error unknown controller type.

		DLOG("AppleKiwiATA failed getting compatible property\n");
		return 0;

	}

	
	// test the compatible property for a match to the controller type name.
	if ( compatibleEntry->isEqualTo( kCompatibleString, sizeof(kCompatibleString)-1 ) == false ) 
	{
		// not our type of controller		
		DLOG("AppleKiwiATA compatible property doesn't match\n");
		return 0;
		
	}


	// do a little initialization here once the probe is succesful so we can start clean.

	OSData		*registryEntry;
	registryEntry = OSDynamicCast( OSData, provider->getProperty( kModelPropertyKey ) );
	if( registryEntry == 0)
	{
		DLOG("AppleKiwiATA unknown model property.\n");
		return 0;
	}


	// test for pci inline.
	if ( !provider->getProperty( kPCIInlineKey ) ) 
	{
		DLOG("AppleKiwiATA pci_inline enable lock\n");
		forcePCIInline = true;
	} else { 
	
		DLOG("AppleKiwiATA pci_inline disable lock\n");
		forcePCIInline = false;
	}

	// test for mode 6.
	if ( !provider->getProperty( kMode6Key ) ) 
	{
		DLOG("AppleKiwiATA mode 5\n");
		mode6Capable = false;	
	} else { 
	
		DLOG("AppleKiwiATA mode 6\n");
			mode6Capable = true;
	}



	
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
AppleKiwiATA::start(IOService *provider)
{

    DLOG("AppleKiwiATA::start() begin\n");
	

	if( ! provider->open(this) )
	{
		DLOG("AppleKiwiATA provider did not open\n");
		getProvider()->message( 'fail', 0 );
		return false;	
	}

	chipRoot = (AppleKiwiRoot*) getProvider()->getProvider();

	ATADeviceNub* newNub=0L;

	// GET a lock before calling the start on the superclass:
	
	getLock(true);
 
 	// call start on the superclass
    if (!super::start( provider))
 	{
        DLOG("AppleKiwiATA: super::start() failed\n");
		goto failurePath;
	}
	
		// Find the interrupt source and attach it to the command gate
	
	if( ! createDeviceInterrupt() )
	{
        DLOG("AppleKiwiATA:  createDeviceInterrupts failed\n");
		goto failurePath;
	}


    DLOG("AppleKiwiATA::start() done\n");

	getLock( false );
	// the nubs will call in on handleCommand and take a lock seperately for each identifyDevice command that they issue. 

	for( UInt32 i = 0; i < 2; i++)
	{
		if( _devInfo[i].type != kUnknownATADeviceType )
		{
		
			DLOG("AppleKiwiATA creating nub\n");
			newNub = ATADeviceNub::ataDeviceNub( (IOATAController*)this, (ataUnitID) i, _devInfo[i].type );
		
			if( newNub )
			{
				
				DLOG("AppleKiwiATA attach nub\n");
				
				newNub->attach(this);
			
				_nub[i] = (IOATADevice*) newNub;
				
				DLOG("AppleKiwiATA register nub\n");

				newNub->registerService();
				newNub = 0L;
					
			}
		}
	
	}

	getProvider()->message( 'onli', 0 );
	return true;

failurePath:

	getProvider()->message( 'fail', 0 );
	getLock( false );
	return false;	


}

/*---------------------------------------------------------------------------
 *	free() - the pseudo destructor. Let go of what we don't need anymore.
 *
 *
 ---------------------------------------------------------------------------*/
void
AppleKiwiATA::free()
{
	for( int i = 0; i < 4; i++)
	{
		if( ioBaseAddrMap[i] )
		{
			ioBaseAddrMap[ i ]->release();
			ioBaseAddrMap[i] = 0;
		}
	}
	if( _devIntSrc )
	{	
		_devIntSrc->release();
		_devIntSrc = 0;
	}
	
//	if(_timer)
//	{
//		_timer->release();
//		_timer = 0;
//	}	
	super::free();


}

void 
AppleKiwiATA::getLock(bool lock)
{
	if( forcePCIInline )
	{
		chipRoot->getLock(lock);
	}
}

/*---------------------------------------------------------------------------
 *
 *	connect the device (drive) interrupt to our workloop
 *
 *
 ---------------------------------------------------------------------------*/
bool
AppleKiwiATA::createDeviceInterrupt(void)
{
	// create a device interrupt source and attach it to the work loop
	
	DLOG("AppleKiwiATA createDeviceInterrupt started\n");
	
	_devIntSrc = IOInterruptEventSource::interruptEventSource( 	
					(OSObject *)this,
					(IOInterruptEventSource::Action) &AppleKiwiATA::sDeviceInterruptOccurred, 
					getProvider(), 
					0); 
	
	DLOG("AppleKiwiATA createdDeviceInterruptsource = %x\n", _devIntSrc);
	DLOG("_workLoop = %x\n", _workLoop);
	
	if( !_devIntSrc || getWorkLoop()->addEventSource(_devIntSrc) )
	{
		DLOG("AppleKiwiATA failed create dev intrpt source\n");
		return false;
	}
		
	// enable interrupt to PCI bus
	UInt32 intMaskLE = (busChildNumber == 0)? 0x00000200 : 0x00000400;
	*globalControlReg &= ~intMaskLE; 
	OSSynchronizeIO();

	_devIntSrc->enable();
	
	DLOG("AppleKiwiATA createDeviceInterrupt done\n");
	
	return true;

}

/*---------------------------------------------------------------------------
 *
 *  static "action" function to connect to our object
 *
 ---------------------------------------------------------------------------*/
void 
AppleKiwiATA::sDeviceInterruptOccurred(OSObject * owner, IOInterruptEventSource *evtSrc, int count)
{
	AppleKiwiATA* self = (AppleKiwiATA*) owner;

	self->handleDeviceInterrupt();


}

/*---------------------------------------------------------------------------
 *
 *  static "action" function to connect to our object
 *
 ---------------------------------------------------------------------------*/
/*
bool
AppleKiwiATA::sFilterInterrupt(OSObject *owner, IOFilterInterruptEventSource *src)
{
	AppleKiwiATA* self = (AppleKiwiATA*) owner;
	
	return self->interruptIsValid( src );

}*/
/*---------------------------------------------------------------------------
 *
 *  interruptIsValid - used in a filter interrupt event source - validates 
 *	the interrupt is indeed for this device before scheduling a thread 
 *
 ---------------------------------------------------------------------------*/

bool 
AppleKiwiATA::interruptIsValid( IOFilterInterruptEventSource* )
{


	//UInt8 bmStatus = *_bmStatusReg;
//	UInt32 gcrStatus = *globalControlReg;
//	OSSynchronizeIO();
	
//	if( gcrStatus & 0x00000020 /*bmStatus & 0x04*/)
//	{
		return true;
//	}
//	return false;
}


/*---------------------------------------------------------------------------
 *
 *	handleDeviceInterrupt - overriden here so we can make sure that the DMA has
 * processed in the event first.
 *
 ---------------------------------------------------------------------------*/
IOReturn
AppleKiwiATA::handleDeviceInterrupt(void)
{	

	if( isBusOnline == false)
	{
		return kIOReturnOffline;
	}

	if( _currentCommand == 0)
	{
	
		return 0;
	}

	UInt32 intMaskLE = (busChildNumber == 0)? 0x00000200 : 0x00000400;
	
	// grab a lock
	getLock(true);
	UInt32 gcrStatus = *globalControlReg;
	UInt8 bmStatus = *_bmStatusReg;
	OSSynchronizeIO();
	
	if( ! (gcrStatus & 0x00000020 )/*bmStatus & 0x04*/ )  // check that the interrupt was asserted since we aren't filtering at interrupt level anymore. 
	{
		getLock( false );
		return 0;
	}
	
	// if this is a DMA command, make sure that the data is fully flushed into system memory
	// before proceeding. The bmStatus register will set the 0x04 flag bit indicating fifo is flushed.
	if( (_currentCommand->getFlags() & mATAFlagUseDMA ) == mATAFlagUseDMA )
	{
		while( !(bmStatus & 0x04) )
		{
			bmStatus = *_bmStatusReg;
			OSSynchronizeIO();
		}
	
	}
	
	// disable interrupt to PCI bus
	*globalControlReg |= intMaskLE; 
	OSSynchronizeIO();

	
	// clear the edge-trigger bit
	*_bmStatusReg = 0x04;
	OSSynchronizeIO();
	
	// super class clears the interrupt request by reading the status reg
	IOReturn result = super::handleDeviceInterrupt();
	

	// enable interrupt to PCI bus
	*globalControlReg &= ~intMaskLE; 
	OSSynchronizeIO();

	getLock( false );
	return result;	
	
}



/*---------------------------------------------------------------------------
 *	get the work loop. If not already created, make one.
 *
 ---------------------------------------------------------------------------*/
IOWorkLoop*
AppleKiwiATA::getWorkLoop() const
{

	DLOG("AppleKiwiATA::getWorkLoop\n");

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
AppleKiwiATA::handleTimeout( void )
{
	if( isBusOnline == false)
	{
		return;
	}

	getLock(true);
	stopDMA();

	volatile UInt8 statusByte = *_tfStatusCmdReg;
	OSSynchronizeIO();

	statusByte++;		// make sure the compiler doesn't drop this.
	
	super::handleTimeout();
	getLock( false);
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
AppleKiwiATA::configureTFPointers(void)
{
    DLOG("AppleKiwiATA config TF Pointers \n");

	OSString* locationCompare = OSString::withCString( "1" );
	if( locationCompare->isEqualTo( getProvider()->getLocation() ))
	{ 
		busChildNumber = 1;
	}
	locationCompare->release();
	locationCompare = NULL;

	DLOG("AppleKiwiATA busChildNumber = %d, string = %1s \n", busChildNumber, getProvider()->getLocation());
	


    ioBaseAddrMap[0] = getProvider()->mapDeviceMemoryWithIndex( busChildNumber == 0 ? 0 : 2 );
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

	 DLOG("AppleKiwiATA base address 0 = %lX \n", baseAddress);
	 	 

	// get the address of the alt-status register from the second base address.

    ioBaseAddrMap[1] = getProvider()->mapDeviceMemoryWithIndex( busChildNumber == 0 ? 1 : 3  );
    if ( ioBaseAddrMap[1] == NULL )
    {
    	 return false;
    }
    
    baseAddress = (volatile UInt8 *)ioBaseAddrMap[1]->getVirtualAddress();

 	_tfAltSDevCReg = baseAddress + 2;   

	 DLOG("AppleKiwiATA base address 1 = %lX altStatus at %lx \n", baseAddress, _tfAltSDevCReg);
	 


	// get the address of the BusMaster/DMA control registers from last base address.
    ioBaseAddrMap[2] = getProvider()->mapDeviceMemoryWithIndex( 4 );
    if ( ioBaseAddrMap[2] == NULL )
    {
    	 return false;
    }

    volatile UInt8* bmAddress = (volatile UInt8*)ioBaseAddrMap[2]->getVirtualAddress();
	if( busChildNumber == 1)
	{
		bmAddress += 0x08;  // secondary bus
	}

	 DLOG("AppleKiwiATA base address 2 = %lX \n", bmAddress);

	_bmCommandReg = bmAddress;
	_bmStatusReg = bmAddress + 2;
	_bmPRDAddresReg = (volatile UInt32*) (bmAddress + 4);	

	// get the address of the mmio control registers from base address 5.
    ioBaseAddrMap[3] = getProvider()->mapDeviceMemoryWithIndex( 5 );
    if ( ioBaseAddrMap[3] == NULL )
    {
    	 return false;
    }

    volatile UInt8* bar5Address = (volatile UInt8*)ioBaseAddrMap[3]->getVirtualAddress();
	DLOG("AppleKiwiATA base address 5 = %lx \n", bar5Address);
	
	if( busChildNumber == 1)
	{
		globalControlReg = (volatile UInt32*) (bar5Address + 0x1208);  // secondary bus
		timingAReg0 =(volatile UInt32*) (bar5Address + 0x120c);
		timingBReg0 =(volatile UInt32*) (bar5Address + 0x1210);
		timingAReg1 =(volatile UInt32*) (bar5Address + 0x1214);
		timingBReg1 =(volatile UInt32*) (bar5Address + 0x1218);
		
	} else {
	
		globalControlReg =(volatile UInt32*) (bar5Address + 0x1108);  // primary bus
		timingAReg0 =(volatile UInt32*) (bar5Address + 0x110c);
		timingBReg0 =(volatile UInt32*) (bar5Address + 0x1110);
		timingAReg1 =(volatile UInt32*) (bar5Address + 0x1114);
		timingBReg1 =(volatile UInt32*) (bar5Address + 0x1118);
	}

	// enable the controller pins
	*globalControlReg &= (~ 0x00000800);  // already LE
	OSSynchronizeIO();
	
	busTimings[0].ataUltraDMASpeedMode = 32;
	busTimings[1].ataUltraDMASpeedMode = 32;
	
	DLOG("AppleKiwiATA GCR = %lx \n", *globalControlReg);
	IOSleep(50);
	DLOG("AppleKiwiATA configTFPointers done\n");
	return true;

}

/*---------------------------------------------------------------------------
 *	provide information on the bus capability 
 *
 *
 ---------------------------------------------------------------------------*/
IOReturn 
AppleKiwiATA::provideBusInfo( IOATABusInfo* infoOut)
{
	if( infoOut == 0)
	{
		DLOG("AppleKiwiATA nil pointer in provideBusInfo\n");
		return -1;
	
	}


	infoOut->zeroData();
	

	
	infoOut->setSocketType( kMediaBaySocket ); // internal fixed, media-bay, PC-Card
	
	
	infoOut->setPIOModes( kATASupportedPIOModes);		
	infoOut->setDMAModes( kATASupportedMultiDMAModes );			
	if( mode6Capable )
	{
	
		infoOut->setUltraModes( kATAAddSupport133);
		DLOG( "AppleKiwiATA: indicating udma mode 6 available\n");

	} else {
	
		infoOut->setUltraModes( kATASupportedUltraDMAModes );	
	}
	
	infoOut->setExtendedLBA( true );  // indicate extended LBA is available on this bus. 
	infoOut->setMaxBlocksExtended( 0x0100 ); // allow up to 256 sectors per transfer on this controller. 
											// this kind of DMA engine is not able to resequence in middle of DMA burst. 
	
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
AppleKiwiATA::getConfig( IOATADevConfig* configRequest, UInt32 unitNumber)
{

	// param check the pointers and the unit number.
	
	if( configRequest == 0
		|| unitNumber > 1 )
		
	{
	
		DLOG("AppleKiwiATA bad param in getConfig\n");
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
AppleKiwiATA::selectConfig( IOATADevConfig* configRequest, UInt32 unitNumber)
{


	// param check the pointers and the unit number.
	
	if( configRequest == 0
		|| unitNumber > 1 )
		
	{
	
		DLOG("AppleKiwiATA bad param in setConfig\n");
		return -1;	
	}



	// all config requests must include a PIO mode
	if( ( configRequest->getPIOMode() & kATASupportedPIOModes ) == 0x00 )
	{
		DLOG("AppleKiwiATA setConfig PIO mode not supported\n");
		return kATAModeNotSupported;		
	
	}


	// DMA is optional - not all devices support it, we reality check for a setting
	// that is beyond our capability


	UInt16 ultraSupported = kATASupportedUltraDMAModes;
	if( mode6Capable )
	{	
		ultraSupported = kATAAddSupport133;	
	}
	// make sure a requested ultra ATA mode is within the range of this device configuration

	if( configRequest->getUltraMode() & ~ultraSupported )
	{
		DLOG("AppleKiwiATA setConfig no ultra\n");
		return kATAModeNotSupported;	
		
	} 



	if( configRequest->getDMAMode() & ~kATASupportedMultiDMAModes )
	{
		DLOG("AppleKiwiATA setConfig DMA mode not supported\n");
		return kATAModeNotSupported;		
	
	} 

	if( configRequest->getDMAMode() > 0x0000
		&& configRequest->getUltraMode() > 0x0000 )
	{
		DLOG("AppleKiwiATA err, only one DMA class allowed in config select\n");
		return kATAModeNotSupported;
	
	}

	_devInfo[unitNumber].packetSend = configRequest->getPacketConfig();
	DLOG("AppleKiwiATA setConfig packetConfig = %d\n", _devInfo[unitNumber].packetSend );

	
	return selectIOTimerValue(configRequest, unitNumber);;

}


/*---------------------------------------------------------------------------
 *	calculate the timing configuration as requested.
 *
 *
 ---------------------------------------------------------------------------*/

IOReturn
AppleKiwiATA::selectIOTimerValue( IOATADevConfig* configRequest, UInt32 unitNumber)
{
// This particular chip snoops the SetFeatures command, so we just snag what the 
// driver tells us as info, but don't set the chip in anyway.

	
	
	busTimings[unitNumber].ataPIOSpeedMode = configRequest->getPIOMode();
	busTimings[unitNumber].ataPIOCycleTime = configRequest->getPIOCycleTime();
	busTimings[unitNumber].ataMultiDMASpeed = configRequest->getDMAMode();
	busTimings[unitNumber].ataMultiCycleTime = configRequest->getDMACycleTime();
	busTimings[unitNumber].ataUltraDMASpeedMode = configRequest->getUltraMode();

	IOLog("AppleKiwiATA:  PIO Mode %d  UDMA mode %d \n", 
	bitSigToNumeric(busTimings[unitNumber].ataPIOSpeedMode), 
	bitSigToNumeric(busTimings[unitNumber].ataUltraDMASpeedMode) );
	
	// stuff the values back into the request structure and return result
	return getConfig( configRequest, unitNumber);

}



void 
AppleKiwiATA::selectIOTiming( ataUnitID unit )
{
	// this chip snoops the SetFeatures command and we don't need to do anything
	// unless it is running with the PLL at 133mhz in the case of the 271. In 
	// that event, we have to override the snoop mode because the chip's internals 
	// don't set the correct mode unless the pll is running at 100 mhz. 

	if(mode6Capable)
	{
		UInt32 bTiming = kPartBTiming33LE;
	
		switch(bitSigToNumeric(busTimings[unit].ataUltraDMASpeedMode) )
		{
			case 2:
				bTiming = kPartBTiming33LE;
			break;
			
			case 4:
				bTiming = kPartBTiming66LE;
			break;
			
			case 5:
				bTiming = kPartBTiming100LE;
			break;
			
			case 6:
				bTiming = kPartBTiming133LE;
			break;
			
			default:
			IOLog("AppleKiwiATA:  error setting timing registers\n");		
			break;
		}
		
		//set the registers
		if( unit == 0)
		{
			*timingAReg0 = kPartATimingLE;
			*timingBReg0  = bTiming;
		} else {
			*timingAReg1 = kPartATimingLE;
			*timingBReg1  = bTiming;		
		}
		
		OSSynchronizeIO();
	}


		return;
}


/*---------------------------------------------------------------------------
 *
 * Subclasses should take necessary action to create DMA channel programs, 
 * for the current memory descriptor in _currentCommand and activate the 
 * the DMA hardware
 ---------------------------------------------------------------------------*/
IOReturn
AppleKiwiATA::startDMA( void )
{

	IOReturn err = kATANoErr;

	// first make sure the engine is stopped.
	stopDMA();
	
	
	// reality check the memory descriptor in the current command
	
	// state flag
	_dmaState = kATADMAStarting;
	
	// create the channel commands
	err = createChannelCommands();
	
	if(	err )
	{
	
		DLOG("IOPCIATA error createChannelCmds err = %ld\n", (long int)err);
		stopDMA();
		return err;
	
	}
	
	// fire the engine
	//activateDMAEngine();
	// Promise DMA engines prefer the host activate the DMA channel *after* the command is written to the drive. This is actually
	// backwards from the ATA standard, but what is implied in the intel data from several years ago.
	return err;
	

}

IOReturn
AppleKiwiATA::issueCommand( void )
{

	IOReturn result = super::issueCommand();
	if( result == kATANoErr
		&& (_currentCommand->getFlags() & mATAFlagUseDMA ) == mATAFlagUseDMA )
	{
		activateDMAEngine();
	}
	return result;
}



// removable code
#pragma mark hot-removal
IOReturn 
AppleKiwiATA::message (UInt32 type, IOService* provider, void* argument)
{

	switch( type )
	{
		case kATARemovedEvent:
		DLOG( "AppleKiwiATA got remove event.\n");
		// mark the bus as dead.
		if(isBusOnline == true)
		{
			isBusOnline = false;
			// lock the queue, don't dispatch immediates or anything else.
			_queueState = IOATAController::kQueueLocked;
			// disable the interrupt source(s) and timers
			_devIntSrc->disable();
			stopTimer();
			
			_workLoop->removeEventSource(_devIntSrc);
			_workLoop->removeEventSource(_timer);
			
			getLock(true);
			stopDMA();
			
			// disable the controller pins
			*globalControlReg |=  0x00000800;  // LE format
			OSSynchronizeIO();
			
			// flush any commands in the queue
			handleQueueFlush();
			
			// if there's a command active then call through the command gate
			// and clean it up from inside the workloop.
			// 

			if( _currentCommand != 0)
			{
			
				DLOG( "AppleKiwiATA Calling cleanup bus.\n");
				
					_cmdGate->runAction( (IOCommandGate::Action) 
						&AppleKiwiATA::cleanUpAction,
            			0, 			// arg 0
            			0, 		// arg 1
            			0, 0);						// arg2 arg 3

			
			
			}
			_workLoop->removeEventSource(_cmdGate);
			
			getLock(false);
			
			
			DLOG( "AppleKiwiATA notify the clients.\n");			
			terminate( );
			getProvider()->message( 'ofln', 0 );
			
		}
		break;
		
		default:		
		DLOG( "AppleKiwiATA got some other event = %d\n", (int) type);
		return super::message(type, provider, argument);
		break;
	}


	return kATANoErr;

}

/*---------------------------------------------------------------------------
 *
 *
 *
 *
 ---------------------------------------------------------------------------*/
IOReturn
AppleKiwiATA::handleQueueFlush( void )
{

	UInt32 savedQstate = _queueState;

	_queueState = IOATAController::kQueueLocked;

	IOReturn errPerCommand = kIOReturnError;

	if( isBusOnline == false )
	{
	
		errPerCommand = kIOReturnOffline;
	
	}

	IOATABusCommand* cmdPtr = 0;

	while( cmdPtr = dequeueFirstCommand() )
	{
	
		cmdPtr->setResult(errPerCommand);
		cmdPtr->executeCallback();
	
	}

	_queueState = savedQstate;

	return kATANoErr;
}

/*---------------------------------------------------------------------------
 *
 *
 *
 *
 ---------------------------------------------------------------------------*/
bool
AppleKiwiATA::checkTimeout( void )
{
	if( isBusOnline == false )
	{
		// signal a timeout if we are within the workloop
		return true;
	
	} 

	return super::checkTimeout();
}

/*---------------------------------------------------------------------------
 *
 *	The main call which puts something on the work loop
 *
 ---------------------------------------------------------------------------*/

IOReturn 
AppleKiwiATA::executeCommand(IOATADevice* nub, IOATABusCommand* command)
{
	if( isBusOnline == false)
	{
		return kIOReturnOffline;
	}
		
	return super::executeCommand(nub, command);

}

// make certain that no commands slip into the work loop between offline and termination.

IOReturn 
AppleKiwiATA::handleCommand(	void*	param0,     /* the command */
				void*	param1,		/* not used = 0 */
				void*	param2,		/* not used = 0 */
				void*	param3 )	/* not used = 0 */
{
	if( isBusOnline == false)
	{
		IOATABusCommand* command = (IOATABusCommand*) param0;  		
		if( command == 0)
			return -1;
			
		command->setCommandInUse(false);
		command->state = kATADone;
		command->setResult( kIOReturnOffline );
		command->executeCallback();
		return kIOReturnOffline;
	}

	getLock(true);
	
	IOReturn result = super::handleCommand( param0, param1, param2, param3 );

	getLock( false );
	
	return result;
}

// called through the commandGate when I get a notification that a media bay has gone away

void
AppleKiwiATA::cleanUpAction(OSObject * owner,
                                               void *     arg0,
                                               void *     arg1,
                                               void *  /* arg2 */,
                                               void *  /* arg3 */)
{


	AppleKiwiATA* self = (AppleKiwiATA*) owner;
	self->cleanUpBus();
}


void
AppleKiwiATA::cleanUpBus(void)
{
	if( _currentCommand != 0)
	{
	
		_currentCommand->setResult(kIOReturnOffline);
		_currentCommand->executeCallback();
		_currentCommand = 0;
	}

}

