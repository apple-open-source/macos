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
 *	AppleK2SATA.cpp
 *	
 *	Defines the concrete driver for a AppleK2SATA controller.
 *	Descendent of IOPCIATA, which is derived from IOATAController.
 *
 */


// increase the PRD table size to one full page or 4096 descriptors to allow for large transfers via
// dma engine. 2048 are required for 1 megabyte of transfer assuming no fragmentation and no alignment 
// issues on the buffer. We allocate twice that since there are more issues than simple alignment for this DMA engine.

#define kATAXferDMADesc 512
#define kATAMaxDMADesc kATAXferDMADesc

// up to 2048 ATA sectors per transfer
#define kMaxATAXfer	512 * 2048

 
#include "AppleK2SATA.h"
#include <IOKit/ata/ATADeviceNub.h>

#include <IOKit/assert.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOTypes.h>
//#include <IOKit/IOTimeStamp.h>
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

#define kCompatibleString 	"k2sata-1"
#define kModelPropertyKey 	"name"
#define kModel4String 		"k2sata-1"


/* Bit-significant representation of supported modes. */ 
#define kATASupportedPIOModes		 0x001F	// modes 4, 3, 2, 1, and 0
#define kATASupportedMultiDMAModes	 0x0007	// modes 2, 1, and 0
#define	kATASupportedUltraDMAModes	 0x003F	// modes  5,4,3,2, 1, and 0

/* Number of entries in various tables */
#define kPIOCycleEntries			12
#define kMultiDMACycleEntries 		9
#define	kUltraDMACycleEntries		5


// ------ ¥¥¥ Maximum transfer modes the AppleK2SATA controller is capable of supporting ¥¥¥ ÐÐÐÐÐÐÐÐ
#define kATAMaxPIOMode		 	4
#define kATAMaxMultiDMAMode		2
#define	kATAMaxUltraDMAMode 	5



#pragma mark -IOService Overrides -


//---------------------------------------------------------------------------

#define super IOPCIATA
OSDefineMetaClassAndStructors ( AppleK2SATA, IOPCIATA )

//---------------------------------------------------------------------------

bool 
AppleK2SATA::init(OSDictionary* properties)
{
	DLOG("AppleK2SATA init start\n");

	for( int i = 0; i < 5; i++)
	{

		ioBaseAddrMap[ i ] = 0;
	
	}
	_devIntSrc = 0;
	busChildNumber = 0;
   	isBusOnline = true;
	//chipRoot = 0;
	
    if (super::init(properties) == false)
    {
        DLOG("AppleK2SATA: super::init() failed\n");
        return false;
    }
	

	DLOG("AppleK2SATA init done\n");

    return true;
}

/*---------------------------------------------------------------------------
 *
 *	Check and see if we really match this device.
 * override to accept or reject close matches based on further information
 ---------------------------------------------------------------------------*/

IOService* 
AppleK2SATA::probe(IOService* provider,	SInt32*	score)
{

    //OSData		*compatibleEntry;
	
	DLOG("AppleK2SATA starting probe\n");

/*
	compatibleEntry  = OSDynamicCast( OSData, provider->getProperty( "compatible" ) );
	if ( compatibleEntry == 0 ) 
	{
		// error unknown controller type.

		DLOG("AppleK2SATA failed getting compatible property\n");
		return 0;

	}

	
	// test the compatible property for a match to the controller type name.
	if ( compatibleEntry->isEqualTo( kCompatibleString, sizeof(kCompatibleString)-1 ) == false ) 
	{
		// not our type of controller		
		DLOG("AppleK2SATA compatible property doesn't match\n");
		return 0;
		
	}


	// do a little initialization here once the probe is succesful so we can start clean.

	OSData		*registryEntry;
	registryEntry = OSDynamicCast( OSData, provider->getProperty( kModelPropertyKey ) );
	if( registryEntry == 0)
	{
		DLOG("AppleK2SATA unknown model property.\n");
		return 0;
	}
*/
	
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
AppleK2SATA::start(IOService *provider)
{

    DLOG("AppleK2SATA::start() begin\n");
	

	if( ! provider->open(this) )
	{
		DLOG("AppleK2SATA provider did not open\n");
		return false;	
	}

	

	ATADeviceNub* newNub=0L;
 
 	// call start on the superclass
    if (!super::start( provider))
 	{
        DLOG("AppleK2SATA: super::start() failed\n");
		goto failurePath;
	}
	
		// Find the interrupt source and attach it to the command gate
	
	if( ! createDeviceInterrupt() )
	{
        DLOG("AppleK2SATA:  createDeviceInterrupts failed\n");
		goto failurePath;
	}


    DLOG("AppleK2SATA::start() done\n");
		

	// the nubs will call in on handleCommand and take a lock seperately for each identifyDevice command that they issue. 

	for( UInt32 i = 0; i < 2; i++)
	{
		if( _devInfo[i].type != kUnknownATADeviceType )
		{
		
			DLOG("AppleK2SATA creating nub\n");
			newNub = ATADeviceNub::ataDeviceNub( (IOATAController*)this, (ataUnitID) i, _devInfo[i].type );
		
			if( newNub )
			{
				
				DLOG("AppleK2SATA attach nub\n");
				
				newNub->attach(this);
			
				_nub[i] = (IOATADevice*) newNub;
				
				DLOG("AppleK2SATA register nub\n");

				newNub->registerService();
				newNub = 0L;
					
			}
		}
	
	}

	return true;

failurePath:

	return false;	


}

/*---------------------------------------------------------------------------
 *	free() - the pseudo destructor. Let go of what we don't need anymore.
 *
 *
 ---------------------------------------------------------------------------*/
void
AppleK2SATA::free()
{
	for( int i = 0; i < 5; i++)
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
AppleK2SATA::getLock(bool lock)
{



}

/*---------------------------------------------------------------------------
 *
 *	connect the device (drive) interrupt to our workloop
 *
 *
 ---------------------------------------------------------------------------*/
bool
AppleK2SATA::createDeviceInterrupt(void)
{
	// create a device interrupt source and attach it to the work loop
	
	DLOG("AppleK2SATA createDeviceInterrupt started\n");
	
	_devIntSrc = IOFilterInterruptEventSource::filterInterruptEventSource( 	
					(OSObject *)this,
					(IOInterruptEventSource::Action) &AppleK2SATA::sDeviceInterruptOccurred,
					 (IOFilterInterruptEventSource::Filter) &AppleK2SATA::sFilterInterrupt,
					getProvider(), 
					0); 
	
	DLOG("AppleK2SATA createdDeviceInterruptsource = %x\n", _devIntSrc);
	DLOG("_workLoop = %x\n", _workLoop);
	
	if( !_devIntSrc || getWorkLoop()->addEventSource(_devIntSrc) )
	{
		DLOG("AppleK2SATA failed create dev intrpt source\n");
		return false;
	}
		

	_devIntSrc->enable();
	
	DLOG("AppleK2SATA createDeviceInterrupt done\n");
	
	return true;

}

/*---------------------------------------------------------------------------
 *
 *  static "action" function to connect to our object
 *
 ---------------------------------------------------------------------------*/
void 
AppleK2SATA::sDeviceInterruptOccurred(OSObject * owner, IOInterruptEventSource *evtSrc, int count)
{
	AppleK2SATA* self = (AppleK2SATA*) owner;

	self->handleDeviceInterrupt();


}

/*---------------------------------------------------------------------------
 *
 *  static "action" function to connect to our object
 *
 ---------------------------------------------------------------------------*/

bool
AppleK2SATA::sFilterInterrupt(OSObject *owner, IOFilterInterruptEventSource *src)
{
	AppleK2SATA* self = (AppleK2SATA*) owner;
	
	return self->interruptIsValid( src );

}
/*---------------------------------------------------------------------------
 *
 *  interruptIsValid - used in a filter interrupt event source - validates 
 *	the interrupt is indeed for this device before scheduling a thread 
 *
 ---------------------------------------------------------------------------*/

bool 
AppleK2SATA::interruptIsValid( IOFilterInterruptEventSource* )
{


	UInt32 intStatus = OSReadLittleInt32( GInterruptStatus, 0);
	OSSynchronizeIO();
	
	//IOTimeStampConstant( KDBG_CODE( DBG_DRIVERS, DBG_DRVDISK, 1), this, 0x35363738, 0 ); 
	
	if( ! (intStatus & interruptBitMask ) )  // check this channel for interrupt 
	{
		return false;
	}

	return true;
}



/*---------------------------------------------------------------------------
 *
 *	handleDeviceInterrupt - overriden here so we can make sure that the DMA has
 * processed in the event first.
 *
 ---------------------------------------------------------------------------*/
IOReturn
AppleK2SATA::handleDeviceInterrupt(void)
{	

	//IOTimeStampConstant( KDBG_CODE( DBG_DRIVERS, DBG_DRVDISK, 2), this, 0x45464748, 0 ); 


	if(_currentCommand == 0 )		
	{
		return 0;
	}	

	

	// super class normally clears the interrupt request by reading the status reg
	// however, in this case, the SATA cell requires the register to be read as a 32-bit value
	// in order to clear the interrupt.	
	UInt32 clearIntStatus = *StatusWide;
	OSSynchronizeIO();
	clearIntStatus++;
	
	IOReturn result = super::handleDeviceInterrupt();

	// clear the edge-trigger bit
	*_bmStatusReg = 0x04;
	OSSynchronizeIO();

	return result;	
	
}



/*---------------------------------------------------------------------------
 *	get the work loop. If not already created, make one.
 *
 ---------------------------------------------------------------------------*/
IOWorkLoop*
AppleK2SATA::getWorkLoop() const
{

	//DLOG("AppleK2SATA::getWorkLoop\n");

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
AppleK2SATA::handleTimeout( void )
{
	if( isBusOnline == false)
	{
		return;
	}

#ifdef  ATA_DEBUG
	UInt8* myPCIHeader = ((UInt8*)GInterruptStatus) - 0x80 ;
#endif	
	DLOG("AppleK2SATA@%d, timeout occurred\n", busChildNumber);
	DLOG("AppleK2SATA@%d, PCI Status register %4X\n", busChildNumber, OSReadLittleInt16(myPCIHeader, 0x06) );
	DLOG("AppleK2SATA@%d, Bus Master Command register %2X\n", busChildNumber, *_bmCommandReg);
	DLOG("AppleK2SATA@%d, Bus Master status register %2X\n", busChildNumber, *_bmStatusReg);
	DLOG("AppleK2SATA@%d, Bus Master PRD register %4X\n", busChildNumber, OSReadLittleInt32(_bmPRDAddresReg, 0) );
	DLOG("AppleK2SATA@%d, Sata Status register %X\n", busChildNumber, *SATAStatus);
	DLOG("AppleK2SATA@%d, Sata Error register %X\n", busChildNumber, *SATAError);
	DLOG("AppleK2SATA@%d, PRD Table physical addr %X\n", busChildNumber, _prdTablePhysical);
	for (int i = 0; i < 32 ; i++)
	{
		DLOG("  PRD %2d  Target phys addr = %8X  count = %4X  flags = %4X\n", 
		i,  
		OSSwapLittleToHostInt32(_prdTable[i].bufferPtr),
		OSSwapLittleToHostInt16(_prdTable[i].byteCount),
		OSSwapLittleToHostInt16(_prdTable[i].flags) );
		
	}
	
	UInt8 cmdRegVal = *_bmCommandReg;
	OSSynchronizeIO();
	
	*_bmCommandReg = (cmdRegVal & 0xFE);  // safely hit the stop bit for this controller first.
	OSSynchronizeIO();

	stopDMA();

	volatile UInt8 statusByte = *_tfStatusCmdReg;
	OSSynchronizeIO();
	DLOG("AppleK2SATA@%d, status register %2X\n", busChildNumber, statusByte);
	
	statusByte++;		// make sure the compiler doesn't drop this.
	
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
AppleK2SATA::configureTFPointers(void)
{
    DLOG("AppleK2SATA config TF Pointers \n");
	
	char myLocString[2] = {0,0} ; 
	sprintf(myLocString, "%1s", getProvider()->getLocation());
	
	
	busChildNumber = myLocString[0] - 0x30;

	DLOG("AppleK2SATA busChildNumber = %d, string = %s \n", busChildNumber, getProvider()->getLocation());
	
	ioBaseAddrMap[0] = getProvider()->mapDeviceMemoryWithIndex( 0 ); 

    if ( ioBaseAddrMap[0] == 0) 
    { 
    	return false;
	}	

    volatile UInt8* baseAddress = (volatile UInt8*)ioBaseAddrMap[0]->getVirtualAddress();
	GInterruptStatus  = (volatile UInt32*) (baseAddress + 0x1f80);
	
	// offset the base address depending which port we are talking to
	switch( busChildNumber )
	{
	
		case 0:
			interruptBitMask = 0x00000001;
		break;
		
		case 1:
			baseAddress += 0x100;
			interruptBitMask = 0x00000002;
		break;
			
		case 2:
			baseAddress += 0x200;
			interruptBitMask = 0x00000004;
		break;
	
		case 3:
			baseAddress += 0x300;
			interruptBitMask = 0x00000008;
		break;
		
		default:
			DLOG("AppleK2SATA - bus number out of range!\n");
			return false;
		break;
	}

	_tfDataReg = (volatile UInt16*) (baseAddress + 0);
	_tfFeatureReg = baseAddress + 0x4;
	_tfSCountReg = baseAddress + 0x8;
	_tfSectorNReg = baseAddress + 0xC;
	_tfCylLoReg = baseAddress + 0x10;
	_tfCylHiReg = baseAddress + 0x14;
	_tfSDHReg = baseAddress + 0x18;
	_tfStatusCmdReg = baseAddress + 0x1C;  

	StatusWide = (volatile UInt32*) (baseAddress + 0x1c);
 	
	_tfAltSDevCReg = baseAddress + 0x20;   

	_bmCommandReg =  (baseAddress + 0x30);
	_bmStatusReg = (baseAddress + 0x31);
	_bmPRDAddresReg = (volatile UInt32*) (baseAddress + 0x34);	

	SATAStatus = (volatile UInt32*) (baseAddress + 0x40);	
	SATAError = (volatile UInt32*) (baseAddress + 0x44);	
	SATAControl = (volatile UInt32*) (baseAddress + 0x48);	
	SIMRegister = (volatile UInt32*) (baseAddress + 0x88);

	SICR1 = (volatile UInt32*) (baseAddress + 0x80);
	SICR2 = (volatile UInt32*) (baseAddress + 0x84);

	
	// clear the edge-trigger bit
	*_bmStatusReg = 0x04;
	// clear error register
	*SATAError = 0xffffffff;
	// mask various interrupts not used
	*SIMRegister = 0;
	OSSynchronizeIO();
	// disable phy slumber and partial modes
	OSWriteLittleInt32( SATAControl, 0, 0x00000300); //set bits 8 and 9
	UInt32 intCont1 = OSReadLittleInt32( SICR1, 0);
	DLOG("AppleK2SATA@%d internal control register 1 = %X\n", busChildNumber, intCont1);
	intCont1 &= (~0x00040000); // clear bit 18
	DLOG("AppleK2SATA@%d setting internal control register 1 to  %X\n", busChildNumber, intCont1);
	OSWriteLittleInt32( SICR1, 0, intCont1);
	DLOG("AppleK2SATA@%d configTFPointers done\n", busChildNumber);
	return true;

}

/*---------------------------------------------------------------------------
 *	provide information on the bus capability 
 *
 *
 ---------------------------------------------------------------------------*/
IOReturn 
AppleK2SATA::provideBusInfo( IOATABusInfo* infoOut)
{
	if( infoOut == 0)
	{
		DLOG("AppleK2SATA nil pointer in provideBusInfo\n");
		return -1;
	
	}


	infoOut->zeroData();
	

	
	infoOut->setSocketType( kInternalSATA ); // internal fixed, media-bay, PC-Card
	
	
	infoOut->setPIOModes( kATASupportedPIOModes);		
	infoOut->setDMAModes( 0 );			
	infoOut->setUltraModes( kATASupportedUltraDMAModes );

	infoOut->setExtendedLBA( true );  // indicate extended LBA is available on this bus. 
	infoOut->setMaxBlocksExtended( 0x0800 ); // allow up to 256 sectors per transfer on this controller. 
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
AppleK2SATA::getConfig( IOATADevConfig* configRequest, UInt32 unitNumber)
{

	// param check the pointers and the unit number.
	
	if( configRequest == 0
		|| unitNumber > 1 )
		
	{
	
		DLOG("AppleK2SATA bad param in getConfig\n");
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
AppleK2SATA::selectConfig( IOATADevConfig* configRequest, UInt32 unitNumber)
{


	// param check the pointers and the unit number.
	
	if( configRequest == 0
		|| unitNumber > 1 )
		
	{
	
		DLOG("AppleK2SATA bad param in setConfig\n");
		return -1;	
	}



	// all config requests must include a PIO mode
	if( ( configRequest->getPIOMode() & kATASupportedPIOModes ) == 0x00 )
	{
		DLOG("AppleK2SATA setConfig PIO mode not supported\n");
		return kATAModeNotSupported;		
	
	}


	// DMA is optional - not all devices support it, we reality check for a setting
	// that is beyond our capability


	// no ultra on unless this is the ultra cell
	UInt16 ultraSupported = kATASupportedUltraDMAModes;
	
	// make sure a requested ultra ATA mode is within the range of this device configuration

	if( configRequest->getUltraMode() & ~ultraSupported )
	{
		DLOG("AppleK2SATA setConfig no ultra\n");
		return kATAModeNotSupported;	
		
	} 



	if( configRequest->getDMAMode() & ~kATASupportedMultiDMAModes )
	{
		DLOG("AppleK2SATA setConfig DMA mode not supported\n");
		return kATAModeNotSupported;		
	
	} 

	if( configRequest->getDMAMode() > 0x0000
		&& configRequest->getUltraMode() > 0x0000 )
	{
		DLOG("AppleK2SATA err, only one DMA class allowed in config select\n");
		return kATAModeNotSupported;
	
	}

	_devInfo[unitNumber].packetSend = configRequest->getPacketConfig();
	DLOG("AppleK2SATA setConfig packetConfig = %d\n", _devInfo[unitNumber].packetSend );

	
	return kATANoErr;

}


/*---------------------------------------------------------------------------
 *	calculate the timing configuration as requested.
 *
 *
 ---------------------------------------------------------------------------*/

IOReturn
AppleK2SATA::selectIOTimerValue( IOATADevConfig* configRequest, UInt32 unitNumber)
{
// This particular chip snoops the SetFeatures command, so we just snag what the 
// driver tells us as info, but don't set the chip in anyway.


	
	
	busTimings[unitNumber].ataPIOSpeedMode = configRequest->getPIOMode();
	busTimings[unitNumber].ataPIOCycleTime = configRequest->getPIOCycleTime();
	busTimings[unitNumber].ataMultiDMASpeed = configRequest->getDMAMode();
	busTimings[unitNumber].ataMultiCycleTime = configRequest->getDMACycleTime();
	busTimings[unitNumber].ataUltraDMASpeedMode = configRequest->getUltraMode();


	// stuff the values back into the request structure and return result
	return getConfig( configRequest, unitNumber);

}



void 
AppleK2SATA::selectIOTiming( ataUnitID unit )
{
	// this chip snoops the SetFeatures command and we don't need to do anything


		return;
}


/*---------------------------------------------------------------------------
 *
 * Subclasses should take necessary action to create DMA channel programs, 
 * for the current memory descriptor in _currentCommand and activate the 
 * the DMA hardware
 ---------------------------------------------------------------------------*/
IOReturn
AppleK2SATA::startDMA( void )
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
	if(_currentCommand->getOpcode() != kATAPIFnExecIO)
		activateDMAEngine();

	return err;
	

}

IOReturn
AppleK2SATA::issueCommand( void )
{


	if( _currentCommand == 0 )
	{
		DLOG("IOATA can't issue nil command\n");
		return kATAErrUnknownType;
	}

	IOReturn result = kIOReturnSuccess;

	if( _currentCommand->getFlags() & mATAFlag48BitLBA )
	{
		IOExtendedLBA* extLBA = _currentCommand->getExtendedLBA();

		OSWriteLittleInt32(_tfSDHReg , 0, extLBA->getDevice());	
		OSWriteLittleInt32(_tfFeatureReg, 0, extLBA->getFeatures16() );
		OSWriteLittleInt32(_tfSCountReg, 0, extLBA->getSectorCount16() );
		OSWriteLittleInt32(_tfSectorNReg, 0, extLBA->getLBALow16() );
		OSWriteLittleInt32(_tfCylLoReg, 0, extLBA->getLBAMid16() );
		OSWriteLittleInt32(_tfCylHiReg, 0, extLBA->getLBAHigh16() );


		OSWriteLittleInt32(_tfStatusCmdReg, 0,   extLBA->getCommand());

	
	} else {
	
		ataTaskFile* tfRegs = _currentCommand->getTaskFilePtr();
	
	
		OSWriteLittleInt32(_tfSDHReg, 0, tfRegs->ataTFSDH);
	

		OSWriteLittleInt32(_tfFeatureReg, 0, tfRegs->ataTFFeatures);
		OSWriteLittleInt32(_tfSCountReg, 0, tfRegs->ataTFCount);
		OSWriteLittleInt32(_tfSectorNReg, 0, tfRegs->ataTFSector);
		OSWriteLittleInt32(_tfCylLoReg,	0, tfRegs->ataTFCylLo);
		OSWriteLittleInt32(_tfCylHiReg,	0, tfRegs->ataTFCylHigh);
	
	
		OSWriteLittleInt32(_tfStatusCmdReg, 0,  tfRegs->ataTFCommand);

		
	}


/*	if( result == kATANoErr
		&& (_currentCommand->getFlags() & mATAFlagUseDMA ) == mATAFlagUseDMA )
	{
		activateDMAEngine();
	}
*/
	return result;
}



// removable code
#pragma mark hot-removal
IOReturn 
AppleK2SATA::message (UInt32 type, IOService* provider, void* argument)
{

	switch( type )
	{
		case kATARemovedEvent:
		DLOG( "AppleK2SATA got remove event.\n");
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
			
			
			// flush any commands in the queue
			handleQueueFlush();
			
			// if there's a command active then call through the command gate
			// and clean it up from inside the workloop.
			// 

			if( _currentCommand != 0)
			{
			
				DLOG( "AppleK2SATA Calling cleanup bus.\n");
				
					_cmdGate->runAction( (IOCommandGate::Action) 
						&AppleK2SATA::cleanUpAction,
            			0, 			// arg 0
            			0, 		// arg 1
            			0, 0);						// arg2 arg 3

			
			
			}
			_workLoop->removeEventSource(_cmdGate);
			
			getLock(false);
			
			
			DLOG( "AppleK2SATA notify the clients.\n");			
			terminate( );
		}
		break;
		
		default:		
		DLOG( "AppleK2SATA got some other event = %d\n", (int) type);
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
AppleK2SATA::handleQueueFlush( void )
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
AppleK2SATA::checkTimeout( void )
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
AppleK2SATA::executeCommand(IOATADevice* nub, IOATABusCommand* command)
{
	if( isBusOnline == false)
	{
		return kIOReturnOffline;
	}
		
	return super::executeCommand(nub, command);

}

// make certain that no commands slip into the work loop between offline and termination.

IOReturn 
AppleK2SATA::handleCommand(	void*	param0,     /* the command */
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
AppleK2SATA::cleanUpAction(OSObject * owner,
                                               void *     arg0,
                                               void *     arg1,
                                               void *  /* arg2 */,
                                               void *  /* arg3 */)
{


	AppleK2SATA* self = (AppleK2SATA*) owner;
	self->cleanUpBus();
}


void
AppleK2SATA::cleanUpBus(void)
{
	if( _currentCommand != 0)
	{
	
		_currentCommand->setResult(kIOReturnOffline);
		_currentCommand->executeCallback();
		_currentCommand = 0;
	}

}

UInt32 
AppleK2SATA::scanForDrives( void )
{
	UInt32 sataStat = OSReadLittleInt32( SATAStatus, 0);
	UInt32 devicesFound = 0;
	
	// examine the DET bits 
	switch( (sataStat & 0x00000007) )
	{
	
		case 0:
			DLOG("AppleK2SATA@%1d no device detected\n", busChildNumber);
		break;

		case 1:
			DLOG("AppleK2SATA@%1d device detected, but phy not ready\n", busChildNumber);
		break;
		
		case 3:
			DLOG("AppleK2SATA@%1d device detected phy is ready \n", busChildNumber);
			devicesFound = 1;
		break;
		
		default:
			DLOG("AppleK2SATA@%1d Phy disabled or reserved bits set, %X\n", busChildNumber, sataStat);
		break;
	}

	if( devicesFound )
	{
		// soft reset the drive in case it hasn't been reset cleanly prior to OS startup.
		OSWriteLittleInt32(_tfAltSDevCReg, 0,  mATADCRReset);
		IODelay( 100 );
		OSWriteLittleInt32(_tfAltSDevCReg, 0, 0);
		IOSleep(100);
		
		return super::scanForDrives();
	}

	OSWriteLittleInt32( SATAControl, 0, 0x3); // disable SATA port if no devices present.

	return 0;
}

/*---------------------------------------------------------------------------
 *
 *  Selecting a device is not required on SATA, since all devices are 
 *	device 0 (master) on sata ports. We do check for pre-req conditions though
 *
 ---------------------------------------------------------------------------*/
IOReturn
AppleK2SATA::selectDevice( ataUnitID unit )
{
	UInt32 msLoops = _currentCommand->getTimeoutMS()/10;	

	// do a reality check
	if( ! (kATADevice0DeviceID == unit)  )
	{
	
		DLOG( "AppleK2SATA@%1d: invalid device ID selected\n", busChildNumber);
		return kATAInvalidDevID;
	
	}		
	
	// give a chance for software to select the correct IO Timing 
	// in case the hardware doesn't maintain seperate timing regs 
	// for each device on the bus.
	selectIOTiming( unit );
	
	UInt8 preReqMask = (mATABusy | mATADataRequest );
	UInt8 preReqCondition = 0x00;
	
	// However, we do need to test for the correct status 
	// before allowing a command to continue. So we check for BSY=0 and DRQ=0
	// before selecting a device that isn't already selected first.
	// for ATA devices, DRDY=1 is required with the exception of 
	// Init Drive Params and Execute Device diagnostics, 90h and 91h
	// as of ATA6, draft d1410r1a 
	// for Packet devices, DRDY is ignored for all commands.
	
	if( _devInfo[ unit ].type == kATADeviceType 
		&& _currentCommand->getOpcode() == kATAFnExecIO
		&& _currentCommand->getTaskFilePtr()->ataTFCommand != 0x90
		&& _currentCommand->getTaskFilePtr()->ataTFCommand != 0x91  )
	{
	
		preReqMask |= mATADriveReady;
		preReqCondition |= mATADriveReady;
	
	}
		 
	while ( !waitForU8Status( (mATABusy ), 0x00 ))
	 {
		
		OSSynchronizeIO();
		if( msLoops == 0
			|| (*_tfStatusCmdReg & (mATABusy |mATADataRequest) ) == mATADataRequest
			|| checkTimeout() )
		{
			DLOG( "AppleK2SATA@%1d: DRQ set, can't select device. %X\n", busChildNumber, OSReadLittleInt32(_tfStatusCmdReg,0) );			
			return kATAErrDevBusy;
		
		}
		msLoops--;
		IOSleep(10);  // allow other threads to run.

	} 



	// successful device selection.
	// enable device interrupt
	//*_tfAltSDevCReg = 0x00;
	//OSSynchronizeIO();

	_selectedUnit = unit;
	return kATANoErr;


}

// this controller features special access to 16 bit values, no need access through the HOB as required in parallel ata. 

IOReturn
AppleK2SATA::registerAccess(bool isWrite)
{
	UInt32	RegAccessMask = _currentCommand->getRegMask();
	IOReturn	err = kATANoErr;
	bool isExtLBA =  _currentCommand->getFlags() & mATAFlag48BitLBA;
	IOExtendedLBA* extLBA = _currentCommand->getExtendedLBA();

	
		/////////////////////////////////////////////////////////////////////////
	if (RegAccessMask & mATAErrFeaturesValid)				// error/features register
	{
		if(isWrite)
		{
			if(isExtLBA )
			{
				OSWriteLittleInt16(_tfFeatureReg, 0, extLBA->getFeatures16() );
			
			} else {

				*_tfFeatureReg	= _currentCommand->getErrorReg();
			}
		}else{
		
			if(isExtLBA )
			{
				extLBA->setFeatures16( OSReadLittleInt16(_tfFeatureReg, 0));
			
			} else {
		
				_currentCommand->setFeatures( *_tfFeatureReg) ;
			}
		}
	}

		/////////////////////////////////////////////////////////////////////////
	if (RegAccessMask & mATASectorCntValid)					// sector count register
	{
		if(isWrite)
		{
			if(isExtLBA )
			{
				OSWriteLittleInt16(_tfSCountReg, 0, extLBA->getSectorCount16() );
			
			} else {

				*_tfSCountReg = _currentCommand->getSectorCount();
			
			}
		}else{

			if(isExtLBA )
			{
				extLBA->setSectorCount16( OSReadLittleInt16(_tfSCountReg, 0 ) );
			
			} else {
				_currentCommand->setSectorCount( *_tfSCountReg );
			}
		}
	}

		/////////////////////////////////////////////////////////////////////////
	if (RegAccessMask & mATASectorNumValid)					// sector number register
	{
		if(isWrite)
		{
			if(isExtLBA )
			{
				OSWriteLittleInt16(_tfSectorNReg, 0, extLBA->getLBALow16() );
			
			} else {
				*_tfSectorNReg	= _currentCommand->getSectorNumber();
			}
			
		}else{
		
			if(isExtLBA )
			{
				extLBA->setLBALow16( OSReadLittleInt16( _tfSectorNReg, 0 ));
			
			} else {
				_currentCommand->setSectorNumber( *_tfSectorNReg );
			}
		}
	}

		/////////////////////////////////////////////////////////////////////////
	if (RegAccessMask & mATACylinderLoValid)				// cylinder low register
	{
		if(isWrite)
		{
			if(isExtLBA )
			{
				OSWriteLittleInt16(_tfCylLoReg, 0, extLBA->getLBAMid16() );
			
			} else {

				*_tfCylLoReg	= _currentCommand->getCylLo();
			}

		}else{
		
			if(isExtLBA )
			{
				extLBA->setLBAMid16( OSReadLittleInt16( _tfCylLoReg, 0) );
			
			} else {
				_currentCommand->setCylLo( *_tfCylLoReg );
			}
		}
	}

		/////////////////////////////////////////////////////////////////////////
	if (RegAccessMask & mATACylinderHiValid)				// cylinder high register
	{
		if(isWrite)
		{
			if(isExtLBA )
			{
				
				OSWriteLittleInt16(_tfCylHiReg, 0, extLBA->getLBAHigh16() );			
			
			} else {

				*_tfCylHiReg	= _currentCommand->getCylHi();
			}
		}else{
		
			if(isExtLBA )
			{
				extLBA->setLBAHigh16( OSReadLittleInt16(_tfCylHiReg, 0) );
			
			} else {
				_currentCommand->setCylHi( *_tfCylHiReg );
			}
		}
	}

		/////////////////////////////////////////////////////////////////////////
	if (RegAccessMask & mATASDHValid)						// ataTFSDH register
	{
		if(isWrite)
		{
			*_tfSDHReg	= _currentCommand->getDevice_Head();
		}else{
			_currentCommand->setDevice_Head( *_tfSDHReg );
		}
	}

	
		/////////////////////////////////////////////////////////////////////////
	if (RegAccessMask & mATAAltSDevCValid)					// alternate status/device control register
	{
		if(isWrite)
		{
			*_tfAltSDevCReg	= _currentCommand->getAltStatus();
		}else{
			_currentCommand->setControl( *_tfAltSDevCReg );
		}
	}

		/////////////////////////////////////////////////////////////////////////
	if (RegAccessMask & mATADataValid)						// data register...
	{
		if(isWrite)
		{
			*_tfDataReg	= _currentCommand->getDataReg();
		
		}else{
		
			_currentCommand->setDataReg( *_tfDataReg );
		}
	}

		/////////////////////////////////////////////////////////////////////////
	if (RegAccessMask & mATAStatusCmdValid)					// status/command register
			{
		if(isWrite)
		{
			*_tfStatusCmdReg	= _currentCommand->getStatus();
		}else{
			_currentCommand->setCommand(*_tfStatusCmdReg );
		}
	}

	return err;




}

IOReturn
AppleK2SATA::softResetBus( bool doATAPI )
{
	DLOG("AppleK2SATA@%d, reset bus\n", busChildNumber);
	IOReturn result = kATANoErr;
	
	// bit setting for do not wait for status message before sending FIS.
	// compatibility issue with Seagate drives. 
	UInt32 intCont1 = OSReadLittleInt32( SICR1, 0);
	intCont1 &= (~0x00040000); // clear bit 18
	OSWriteLittleInt32( SICR1, 0, intCont1);

	
	if (doATAPI)
	{	
	
		return super::softResetBus( doATAPI );
				
	} else {
		
		// begin the ATA soft reset sequence, which affects both devices on the 
		// bus
		
			
		// We will set nIEN bit to 0 to force the IRQ line to be driven by the selected
		// device.  We were seeing a small number of cases where the tristated line
		// caused false interrupts to the host.	

		OSWriteLittleInt32(_tfAltSDevCReg, 0,  mATADCRReset);

		// ATA standards only dictate >5us of hold time for the soft reset operation
		// 100 us should be sufficient for any device to notice the soft reset.
		
		IODelay( 100 );
		

		OSWriteLittleInt32(_tfAltSDevCReg, 0, 0);
		
		DLOG("AppleK2SATA@%d, soft reset sequenced\n", busChildNumber);
	
		// SATA only supports device 0, no need to reselect for device 1 ever.
		
		_selectedUnit = kATADevice0DeviceID;
	
	}

	// ATA-4 and ATA-5 require the host to wait for >2ms 
	// after a sRST before sampling the drive status register.
	IOSleep(50);

	// ATA and ATAPI devices indicate reset completion somewhat differently
	// for ATA, wait for BSY=0 and RDY=1. For ATAPI, wait for BSY=0 only.
	UInt8 readyMask = mATABusy;
	UInt8 readyOn	= 0x00;
	
	if( (_devInfo[0].type == kATADeviceType)
		&& (!doATAPI) )
	{
		readyMask |= mATADriveReady;  //mask-in the busy + ready bit
		readyOn	= mATADriveReady;		// look for a RDY=1 as well as BSY=0
	}

	
	bool resetFailed = true;
	
	// loop for up to 31 seconds following a reset to allow 
	// drives to come on line. Most devices take 50-100ms, a sleeping drive 
	// may need to spin up and touch media to respond. This may take several seconds.
	for( int i = 0; i < 3100; i++)
	{
		
		// read the status register - helps deal with devices which errantly 
		// set interrupt pending states during resets. Reset operations are not 
		// supposed to generate interrupts, but some devices do anyway.
		// interrupt handlers should be prepared to deal with errant interrupts on ATA busses.

		UInt32 status = OSReadLittleInt32( _tfStatusCmdReg, 0 );  	
		
		// when drive is ready, break the loop
		if( ( status & readyMask )== readyOn)
		{
			// device reset completed in time
			resetFailed = false;
			break;
		}
		
		IOSleep( 10 );  // sleep thread for another 10 ms
	
	}


	if( resetFailed )
	{
		// it is likely that this hardware is broken. 
		// There's no recovery action if the drive fails 
		// to reset.	
		DLOG("AppleK2SATA@%d, soft reset failed\n", busChildNumber);	
		result = kATATimeoutErr;
	}
	
	DLOG("AppleK2SATA@%d, soft reset completed\n", busChildNumber);

	return result;


}

/*---------------------------------------------------------------------------
 * This is a special-case state machine which synchronously polls the 
 *	status of the hardware while completing the command. This is used only in 
 *	special case of IO's which cannot be completed for some reason using the 
 *	normal interrupt system. This may involve vendor-specific commands, or 
 *	certain instances where the interrupts may not be available, such as when 
 *  handling commands issued as a result of messaging the drivers after a reset
 *	event. DMA commands are NOT accepted, only non-data and PIO commands.
 ---------------------------------------------------------------------------*/
IOReturn
AppleK2SATA::synchronousIO(void)
{
	IOReturn err = kATANoErr;

	*_tfAltSDevCReg = 0x02; // disable nIEN
	IOSleep(1);  //special for K2 
	
					
	// start by issuing the command	
	err = asyncCommand();
	DLOG("AppleK2SATA@%d, synchronous command sent: err = %ld state= %lx\n", busChildNumber, (long int) err, (int) _currentCommand->state);
	if( err )
	{
		_currentCommand->state = IOATAController::kATAComplete;
	
	} else {
	
	// spin on status until the next phase
	
		for( UInt32 i = 0; i< 3000; i++)
		{
			if( waitForU8Status( mATABusy, 0x00	) )
				break;
			IOSleep(10); //allow other threads to run.
		}
	
	}


	// if packet, send packet next
	if( _currentCommand->state == IOATAController::kATAPICmd )
	{						
		DLOG("AppleK2SATA@%d,::synchronous issue packet\n", busChildNumber);
		err = writePacket();
		
		if( err == kATANoErr )
		{
			// if there's data IO, next phase is dataTx, otherwise check status.
			if( (_currentCommand->getFlags() & (mATAFlagIORead |  mATAFlagIOWrite ) )
				&&  ((_currentCommand->getFlags() & mATAFlagUseDMA ) != mATAFlagUseDMA ) )
		
			{
				_currentCommand->state = IOATAController::kATADataTx;
		
			} else {  			
				
				// this is a non-data command, the next step is to check status.				
				_currentCommand->state = IOATAController::kATAStatus;			
			}		

		} else {
		
			// an error occured writing the packet.
			_currentCommand->state = IOATAController::kATAComplete;
		}
	}


	// PIO data transfer phase
									
	if( _currentCommand->state == IOATAController::kATADataTx ) 
	{
		while( _currentCommand->state == IOATAController::kATADataTx  )
		{
			err = asyncData();
			if( err )
			{
				_currentCommand->state = IOATAController::kATAComplete;
				break;
			}
		}		
		
		if( (_currentCommand->getFlags() & mATAFlagProtocolATAPI) == mATAFlagProtocolATAPI
			&& _currentCommand->getPacketSize() > 0)
		{			
			// atapi devices will go to status after an interrupt.
			waitForU8Status( mATABusy, 0x00	);
		}	
	
	}		

	// else fall through to status state.
	if( _currentCommand->state == IOATAController::kATAStatus ) 		
	{
		err = asyncStatus();
		_currentCommand->state = IOATAController::kATAComplete;
	}
		
		
		
	// read the status register to make sure the hardware is in a consistent state.
	UInt32 finalStatus = OSReadLittleInt32( _tfStatusCmdReg, 0 );
	
	finalStatus++;
	// call completeIO if the command is marked for completion.
	
	if( _currentCommand->state == IOATAController::kATAComplete )
	{
		completeIO(err);
	}

	// enable interrupts
	*_tfAltSDevCReg = 0x00; // enable nIEN
	IOSleep(1);  //special for K2 

	return err;


}

/*---------------------------------------------------------------------------
 *
 *	allocate memory and resources for the DMA descriptors.
 *
 *
 ---------------------------------------------------------------------------*/
bool
AppleK2SATA::allocDMAChannel(void)
{

	if(  _bmCommandReg == 0
		||	_bmStatusReg == 0
		|| _bmPRDAddresReg == 0 )
	{
	
		DLOG("AppleK2SATA@%d, bm regs not initialised.\n", busChildNumber);
		return false;	
	
	}



	_prdTable = (PRD*)IOMallocContiguous( sizeof(PRD) * kATAMaxDMADesc, 
						0x10000, 
						&_prdTablePhysical );
	
	if(	! _prdTable )
	{
		DLOG("AppleK2SATA@%d, alloc prd table failed\n",busChildNumber);
		return false;
	
	}


	_DMACursor = IONaturalMemoryCursor::withSpecification(0x10000, /*64K*/
                                       					kMaxATAXfer  /* 2048 * 512 */
                                     					/*inAlignment - Memory descriptors and cursors don't support alignment
                                     					flags yet. */);

	
	if( ! _DMACursor )
	{
		freeDMAChannel();
		DLOG("AppleK2SATA@%d, alloc DMACursor failed\n", busChildNumber);
		return false;
	}


	// fill the chain with stop commands to initialize it.	
	initATADMAChains(_prdTable);
	
	return true;
}


/*---------------------------------------------------------------------------
 *
 *	deallocate memory and resources for the DMA descriptors.
 *
 *
 ---------------------------------------------------------------------------*/
bool
AppleK2SATA::freeDMAChannel(void)
{
	
	if( _prdTable )
	{
		// make sure the engine is stopped
		stopDMA();

		// free the descriptor table.
		IOFreeContiguous( (void*) _prdTable, 
		sizeof(PRD) * kATAMaxDMADesc);
	}

	return true;
}

//----------------------------------------------------------------------------------------
//	Function:		InitATADMAChains
//	Description:	Initializes the chains with STOP commands.
//
//	Input:			Pointer to the DBDMA descriptor area: descPtr
//	
//	Output:			None
//----------------------------------------------------------------------------------------
void	
AppleK2SATA::initATADMAChains (PRD* descPtr)
{
	UInt32 i;
	
	/* Initialize the data-transfer PRD channel command descriptors. */


	for (i = 0; i < kATAMaxDMADesc; i++)
	{
		descPtr->bufferPtr = 0;
		descPtr->byteCount = 1;
		// set the stop DMA bit on the last transaction.
		descPtr->flags = OSSwapHostToLittleInt16( (UInt16) kLast_PRD);
		descPtr++;
	}
}

/*---------------------------------------------------------------------------
 *
 *	create the DMA channel commands.
 *
 *
 ---------------------------------------------------------------------------*/
IOReturn
AppleK2SATA::createChannelCommands(void)
{
	IOMemoryDescriptor* descriptor = _currentCommand->getBuffer();
	IOMemoryCursor::PhysicalSegment physSegment;
	UInt32 index = 0;
	UInt8		*xferDataPtr, *ptr2EndData, *next64KBlock, *starting64KBlock;
	UInt32		xferCount, count2Next64KBlock;
	
	if( ! descriptor )
	{
	
		DLOG("AppleK2SATA@%d, nil buffer!\n", busChildNumber);
		return -1;
	}

	// This form of DMA engine can only do 1 pass. It cannot execute multiple chains.

	// calculate remaining bytes in this transfer
	IOByteCount bytesRemaining = _currentCommand->getByteCount() ;

	// calculate position pointer
	IOByteCount xfrPosition = _currentCommand->getPosition() ;
	
	IOByteCount  transferSize = 0; 

	// There's a unique problem with pci-style controllers, in that each dma transaction is not allowed to
	// cross a 64K boundary. This leaves us with the yucky task of picking apart any descriptor segments that
	// cross such a boundary ourselves.  

		while( _DMACursor->getPhysicalSegments(
											descriptor,
					       					xfrPosition,
					       					&physSegment,
					     					1,
					     					bytesRemaining,  // limit to the requested number of bytes in the event the descriptors is larger
					       					&transferSize) )
		{
				       					
			xferDataPtr = (UInt8*) physSegment.location;
			xferCount = physSegment.length;
			
			if( ((UInt32) xferDataPtr & 0x03) || (xferCount & 0x01))
			{
				kprintf("AppleK2SATA@%d, target DMA address is at bad alignment = %X xferCount  = %X.\n", busChildNumber, xferDataPtr, xferCount);
				return kIOReturnNotAligned;			
			}
			
			// update the bytes remaining after this pass
			bytesRemaining -= xferCount;
			xfrPosition += xferCount;
			
			// now we have to examine the segment to see whether it crosses (a) 64k boundary(s)
			starting64KBlock = (UInt8*) ( (UInt32) xferDataPtr & 0xffff0000);
			ptr2EndData = xferDataPtr + xferCount;
			next64KBlock  = (starting64KBlock + 0x10000);


			// loop until this physical segment is fully accounted for.
			// it is possible to have a memory descriptor which crosses more than one 64K boundary in a 
			// single span.
			while( xferCount > 0 )
			{
				if (ptr2EndData > next64KBlock)
				{
					count2Next64KBlock = next64KBlock - xferDataPtr;
					if (index < kATAMaxDMADesc)
					{
						setPRD(xferDataPtr, (UInt16)count2Next64KBlock, &_prdTable[index], kContinue_PRD);
						xferDataPtr = next64KBlock;
						next64KBlock += 0x10000;
						xferCount -= count2Next64KBlock;
						index++;
					
					} else {
					
						DLOG("AppleK2SATA@%d, dma too big, PRD table exhausted A.\n", busChildNumber);
						_dmaState = kATADMAError;
						return -1;
					}
				
				} else {
				
					if (index < kATAMaxDMADesc)
					{
						setPRD(xferDataPtr, (UInt16) xferCount, &_prdTable[index], (bytesRemaining == 0) ? kLast_PRD : kContinue_PRD);
						xferCount = 0;
						index++;

					} else {
					
						DLOG("AppleK2SATA@%d, dma too big, PRD table exhausted B.\n", busChildNumber);
						_dmaState = kATADMAError;
						return -1;
					}
				}
			}

	} // end of segment counting loop.
	
	
		
	// transfer is satisfied and only need to check status on interrupt.
	_dmaState = kATADMAStatus;

	//DLOG("AppleK2SATA@%d, PRD chain end %ld \n", busChildNumber, index);

	
	// chain is now ready for execution.

	return kATANoErr;

}

bool 
AppleK2SATA::ATAPISlaveExists( void )
{
	return false;
	
}



IOReturn
AppleK2SATA::writePacket( void )
{

	UInt32 packetSize = _currentCommand->getPacketSize();
	UInt16* packetData = _currentCommand->getPacketData();
	
	DLOG("AppleK2SATA@%d, writePacket\n", busChildNumber);
	
	// First check if this ATAPI command requires a command packetÉ
	if ( packetSize == 0)						
	{
		DLOG("AppleK2SATA@%d, writePacket no packet data\n", busChildNumber);

		return kATANoErr;
	}

	UInt8 status = 0x00;
		
	// While the drive is busy, wait for it to set DRQ.
	// limit the amount of time we will wait for a drive to set DRQ
	// ATA specs imply that all devices should set DRQ within 3ms. 
	// we will allow up to 30ms.
	
	UInt32  breakDRQ = 3;

		
	while ( !waitForU8Status( (mATABusy | mATADataRequest), mATADataRequest)
			&& !checkTimeout()
			&& (breakDRQ != 0)  ) 
	{
		// check for a device abort - not legal under ATA standards,
		// but it could happen		
		status = *_tfAltSDevCReg;
		 //mask the BSY and ERR bits
		status &= (mATABusy | mATAError);

		// look for BSY=0 and ERR = 1
		if( mATAError == status )
		{
		DLOG("AppleK2SATA@%d, writePacket error status =%x\n", busChildNumber, status);
			return kATADeviceError;
		}
		
		breakDRQ--;
		IOSleep( 10 );  // allow other threads to run
	 }

	// let the timeout through
	if ( checkTimeout() 
			|| breakDRQ == 0)
	{
		DLOG("AppleK2SATA@%d, writePacket timed out status = %X\n", busChildNumber, status);
		return kATATimeoutErr;
	}
	// write the packet
	UInt32 packetLength = 6;
	
	if( packetSize > 12 )
	{
		packetLength = 8;
	
	}
	
	for( UInt32 i = 0; i < packetLength; i++)
	{
		OSSynchronizeIO();
		* _tfDataReg = *packetData;
		packetData++;	
	}

	DLOG("AppleK2SATA@%d, writePacket packet data complete\n", busChildNumber);

	if( _currentCommand->getFlags() & (mATAFlagUseDMA) )
	{
			DLOG("AppleK2SATA@%d, writePacket activate DMA engine\n", busChildNumber);	
			activateDMAEngine();
	}
	return  kATANoErr ;

}

