/*
 * Copyright (c) 2001-2003 Apple Computer, Inc. All rights reserved.
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





#include "ApplePCCardATA.h"
#include <IOKit/ata/ATADeviceNub.h>

#include <IOKit/assert.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOTypes.h>
#include <IOKit/IOMessage.h>
#include <IOKit/ata/IOATATypes.h>
#include <IOKit/ata/IOATAController.h>
#include <IOKit/ata/IOATACommand.h>
#include <IOKit/ata/IOATADevice.h>
#include <IOKit/ata/IOATABusInfo.h>
#include <IOKit/ata/IOATADevConfig.h>

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

#define kCompatibleString "pccard-ata"
#define kATASupportedPIOModes		 0x0001	// mode 0 only


#pragma mark -IOService Overrides -


//---------------------------------------------------------------------------

#define super IOATAController

OSDefineMetaClassAndStructors(  ApplePCCardATA, IOATAController )

//---------------------------------------------------------------------------

bool 
ApplePCCardATA::init(OSDictionary* properties)
{
	DLOG("ApplePCCardATA init start\n");

	_devIntSrc = 0;
	isBusOnline = true;
	baseAddressMap = 0;
	baseAddressMap2 = 0;
	
    // Initialize instance variables.
   
    if (super::init(properties) == false)
    {
        DLOG("ApplePCCardATA: super::init() failed\n");
        return false;
    }
	
	DLOG("ApplePCCardATA init done\n");

    return true;
}

/*---------------------------------------------------------------------------
 *
 *	Check and see if we really match this device.
 * override to accept or reject close matches based on further information
 ---------------------------------------------------------------------------*/

IOService* 
ApplePCCardATA::probe(IOService* provider,	SInt32*	score)
{

    OSData		*compatibleEntry;
	
	DLOG("ApplePCCardATA starting probe\n");


	compatibleEntry  = OSDynamicCast( OSData, provider->getProperty( "compatible" ) );
	if ( compatibleEntry == 0 ) 
	{
		// error unknown controller type.

		DLOG("ApplePCCardATA failed getting compatible property\n");
		return 0;

	}

	
	// test the compatible property for a match to the controller type name.
	if ( compatibleEntry->isEqualTo( kCompatibleString, sizeof(kCompatibleString)-1 ) == false ) 
	{
		// not our type of controller		
		DLOG("ApplePCCardATA compatible property doesn't match\n");
		return 0;
		
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
ApplePCCardATA::start(IOService *provider)
{

    DLOG("ApplePCCardATA::start() begin\n");


/*    
	IMPLEMENT:
    if( ! OSDynamicCast( xxxInsertProviderClassNameHerexxx , provider ) )
    {
    
    	DLOG("ApplePCCardATA provider not specified class!\n");
    	return false;    
    
    }
*/
	if( ! provider->open(this) )
	{
		DLOG("ApplePCCardATA provider did not open\n");
		return false;	
	}

	ATADeviceNub* newNub=0L;
 
 	// call start on the superclass
    if (!super::start( provider))
 	{
        DLOG("ApplePCCardATA: super::start() failed\n");
        provider->close(this);
        return false;
	}

	if( ! createDeviceInterrupt() )
	{
        DLOG("ApplePCCardATA:  createDeviceInterrupts failed\n");
		return false;	
	
	}
	


	// enable interrupt sources
	if( !enableInterrupt(0) )
	{
        DLOG("ApplePCCardATA: enable ints failed\n");
		return false;	
	
	}


    DLOG("ApplePCCardATA::start() done\n");

	for( UInt32 i = 0; i < 2; i++)
	{
		if( _devInfo[i].type != kUnknownATADeviceType )
		{
		
			DLOG("ApplePCCardATA creating nub\n");
			newNub = ATADeviceNub::ataDeviceNub( (IOATAController*)this, (ataUnitID) i, _devInfo[i].type );
		
			if( newNub )
			{
				
				DLOG("ApplePCCardATA attach nub\n");
				
				newNub->attach(this);
			
				_nub[i] = (IOATADevice*) newNub;
				
				DLOG("ApplePCCardATA register nub\n");

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
ApplePCCardATA::free()
{
	
	if(_devIntSrc)
	{
		_devIntSrc->release();
		_devIntSrc = 0;
	}

	if( baseAddressMap )
	{
		baseAddressMap->release();
		baseAddressMap = 0;
	}
	
	
	if( baseAddressMap2 )
	{
		baseAddressMap2->release();
		baseAddressMap2 = 0;
	}

#ifdef __i386__
#define RELEASE(x) do { if(x) { (x)->release(); (x) = 0; } } while(0)

    RELEASE( _tfDataReg      );
    RELEASE( _tfFeatureReg   );
    RELEASE( _tfSCountReg    );
    RELEASE( _tfSectorNReg   );
    RELEASE( _tfCylLoReg     );
    RELEASE( _tfCylHiReg     );
    RELEASE( _tfSDHReg       );
    RELEASE( _tfStatusCmdReg );
    RELEASE( _tfAltSDevCReg  );
#endif

	super::free();

}


/*---------------------------------------------------------------------------
 *	get the work loop. If not already created, make one.
 *
 ---------------------------------------------------------------------------*/
IOWorkLoop*
ApplePCCardATA::getWorkLoop() const
{

	DLOG("ApplePCCardATA::getWorkLoop\n");

	IOWorkLoop* wl = _workLoop;

	if (!wl) 
	{
        wl = IOWorkLoop::workLoop();
           
        if (!wl)
            return 0;
    }        
            
 return   wl;        

}



#pragma mark -Controller specific-
/*---------------------------------------------------------------------------
 *
 *	Initialize the taskfile pointers to the addresses of the ATA registers
 *	in your hardware. Subclasses must provide implementation.
 *
 ---------------------------------------------------------------------------*/
bool
ApplePCCardATA::configureTFPointers(void)
{

	// setup the taskfile pointers inherited from the superclass
	// this allows IOATAController to scan for drives during it's start()
	volatile UInt8*  baseAddress = 0;
	volatile UInt8*  alternateAddress = 0;  // some PC cards map the alt-status into a different adddress window

	baseAddressMap = _provider->mapDeviceMemoryWithIndex(0);
	
	if( !baseAddressMap )
	{
		DLOG("ApplePCCardATA no base map\n");
		return false;
	}
	
	DLOG("ApplePCCardATA basemap ptr = %lx\n", baseAddressMap );

	baseAddress = (volatile UInt8*) baseAddressMap->getVirtualAddress(); 
	if( !baseAddress )
	{
		DLOG("ApplePCCardATA no base address\n");
		return false;
	}



	// check for second memory window
	baseAddressMap2 = _provider->mapDeviceMemoryWithIndex(1);
	if( !baseAddressMap2 )
	{
	
		DLOG("ApplePCCardATA no second map\n");
		// this is OK for a single-window PC card.
	
	} else {
	
		alternateAddress = (volatile UInt8*) baseAddressMap2->getVirtualAddress(); 	
		if( !alternateAddress )
		{
			DLOG("ApplePCCardATA second window has no valid address address\n");
			return false;
		}
	}



#if defined(__ppc__)
	_tfDataReg = (volatile UInt16*) (baseAddress + 0x00);

	_tfFeatureReg = baseAddress + 0x01;
	_tfSCountReg = baseAddress + 0x2;
	_tfSectorNReg = baseAddress + 0x3;
	_tfCylLoReg = baseAddress + 0x4;
	_tfCylHiReg = baseAddress + 0x5;
	_tfSDHReg = baseAddress + 0x6;
	_tfStatusCmdReg = baseAddress + 0x7;  

	// if unified address window, use offset of 0xe, otherwise use the second address
	if( alternateAddress == 0)
	{
		_tfAltSDevCReg = baseAddress + 0xe;   
	
	} else {
	
		_tfAltSDevCReg = alternateAddress;   
	}
#elif defined(__i386__)
    UInt32 baseIOAddress = (UInt32) baseAddress;
    UInt32 alternateIOAddress = (UInt32) alternateAddress;

	_tfDataReg = IOATAIOReg16::withAddress(baseIOAddress + 0x00);
	_tfFeatureReg = IOATAIOReg8::withAddress(baseIOAddress + 0x01);
	_tfSCountReg = IOATAIOReg8::withAddress(baseIOAddress + 0x2);
	_tfSectorNReg = IOATAIOReg8::withAddress(baseIOAddress + 0x3);
	_tfCylLoReg = IOATAIOReg8::withAddress(baseIOAddress + 0x4);
	_tfCylHiReg = IOATAIOReg8::withAddress(baseIOAddress + 0x5);
	_tfSDHReg = IOATAIOReg8::withAddress(baseIOAddress + 0x6);
	_tfStatusCmdReg = IOATAIOReg8::withAddress(baseIOAddress + 0x7);  

	// if unified address window, use offset of 0xe, otherwise use the second address
	if( alternateIOAddress == 0)
	{
		_tfAltSDevCReg = IOATAIOReg8::withAddress(baseIOAddress + 0xe);   
	
	} else {
	
		_tfAltSDevCReg = IOATAIOReg8::withAddress(alternateIOAddress);   
	}
#else
#error Unknown machine architecture
#endif

	DLOG("ApplePCCardATA configTFPointers succeeded. base addr = %lx \n", baseAddress);
	DLOG("ApplePCCardATA configTFPointers succeeded. phys addr = %lx \n", (UInt8*) baseAddressMap->getPhysicalAddress() );
	return true;

}


/*---------------------------------------------------------------------------
 *
 *	All ata controller subclasses must provide an implementation.
 *
 *
  ---------------------------------------------------------------------------*/

IOReturn
ApplePCCardATA::provideBusInfo( IOATABusInfo* infoOut)
{
	if( infoOut == 0)
	{
		DLOG("ApplePCCardATA: nil pointer in provideBusInfo\n");
		return -1;
	
	}

	infoOut->zeroData();
	
	infoOut->setSocketType( kPCCardSocket );   // indicate removable PC Card
	
	infoOut->setPIOModes( 0x01); // PIO Mode 0 only for PC Cards		
	
	UInt8 units = 0;
	
	if(	_devInfo[0].type != kUnknownATADeviceType )
		units++;
		
	if(	_devInfo[1].type != kUnknownATADeviceType )
		units++;

	infoOut->setUnits( units);

	return kATANoErr;
}


/*---------------------------------------------------------------------------
 *
 *	select the bus timing configuration for a particular device
 *  should be called by device driver after doing an identify device command and working out 
 *	the desired timing configuration.
 *	should be followed by a Set Features comand to the device to set it in a 
 *	matching transfer mode.
 ---------------------------------------------------------------------------*/
IOReturn 
ApplePCCardATA::selectConfig( IOATADevConfig* configRequest, UInt32 unitNumber)
{

	// param check the pointers and the unit number.
	
	if( configRequest == 0
		|| unitNumber > 1 )
		
	{
	
		DLOG("ApplePCCardATA bad param in setConfig\n");
		return -1;	
	}

	// all config requests must include a PIO mode
	if( ( configRequest->getPIOMode() & 0x01 ) == 0x00)
	{
		DLOG("ApplePCCardATA setConfig PIO mode not supported\n");
		return kATAModeNotSupported;			
	}
	
	// check if a DMA setting was requested. No DMA engine on this bus.
	if( configRequest->getUltraMode() != 0
	|| configRequest->getDMAMode() != 0 )
	{
		DLOG("ApplePCCardATA setConfig DMA modes not supported\n");
		return kATAModeNotSupported;				
	}

	// if packet device, record the means by which packets are delivered
	_devInfo[unitNumber].packetSend = configRequest->getPacketConfig();
	DLOG("ApplePCCardATA setConfig packetConfig = %ld\n", _devInfo[unitNumber].packetSend );

	return kATANoErr;	
	

}

/*---------------------------------------------------------------------------
 *
 *	Find out what the current bus timing configuration is for a particular
 *	device. 
 *	Subclasses must implement
 *
 ---------------------------------------------------------------------------*/
IOReturn 
ApplePCCardATA::getConfig( IOATADevConfig* configOut, UInt32 unitNumber)
{

	// param check the pointers and the unit number.
	
	if( configOut == 0
		|| unitNumber > 1 )
		
	{
	
		DLOG("ApplePCCardATA: bad param in getConfig\n");
		return -1;	
	}




	// grab the info from our internal data.
	configOut->setPIOMode(0x01);  // PIO mode 0 only.
	configOut->setDMAMode(0);
	configOut->setPIOCycleTime(600);  // PIO mode 0, 600ns nominal cycle time
	configOut->setDMACycleTime(0);
	configOut->setPacketConfig( _devInfo[unitNumber].packetSend );
	configOut->setUltraMode(0);

	return kATANoErr;

} 

/*---------------------------------------------------------------------------
 *
 *  static "action" function to connect to our object
 *
 ---------------------------------------------------------------------------*/
void 
ApplePCCardATA::deviceInterruptOccurred(OSObject * owner, IOInterruptEventSource *evtSrc, int count)
{
	ApplePCCardATA* self = (ApplePCCardATA*) owner;

	self->handleDeviceInterrupt();
}


/*---------------------------------------------------------------------------
 *
 *	connect the device (drive) interrupt to our workloop
 *
 *
 ---------------------------------------------------------------------------*/
bool
ApplePCCardATA::createDeviceInterrupt(void)
{
	// create a device interrupt source and attach it to the work loop

	_devIntSrc = IOInterruptEventSource::
	interruptEventSource( (OSObject *)this,
	(IOInterruptEventAction) & ApplePCCardATA::deviceInterruptOccurred, _provider->getProvider(), 0); 


	if( !_devIntSrc || _workLoop->addEventSource(_devIntSrc) )
	{
		DLOG("ApplePCCardATA failed create dev intrpt source\n");
		return false;
	}

	_devIntSrc->enable();

	return true;

}


#pragma mark -special case overrides-

// clear spurious interrupt by reading status even if no transaction pending.
IOReturn 
ApplePCCardATA::handleDeviceInterrupt(void)
{
	if( isBusOnline == false)
	{
		return kIOReturnOffline;
	}

	if( !_currentCommand )
	{
		volatile UInt8 status = *_tfStatusCmdReg;
		OSSynchronizeIO();
		status++; // prevent compiler removal of unused variable.
		return 0;
	}

	return super::handleDeviceInterrupt();

}



// override for special case device.
// do not allow an errant drive to back-state the controller's state machine.
IOATAController::transState 
ApplePCCardATA::determineATAPIState(void)
{
	IOATAController::transState			drivePhase = super::determineATAPIState();
	if(  _currentCommand->state > drivePhase
		|| _currentCommand->state == kATAStarted )
	{
		return (IOATAController::transState) _currentCommand->state;
	}

	return drivePhase;
}


// when disabling the nIEN bit, some drives errantly assert INTRQ upon reenabling.
// disable the interrupt source, then do the sync operation, then read status twice, then reenable.
// should clear the condition even on a bad device.
IOReturn 
ApplePCCardATA::synchronousIO(void)
{
	_devIntSrc->disable();

	IOReturn result = super::synchronousIO();
	
	// read the status register in case the drive didn't clear int-pending status already.

	volatile UInt8 status = *_tfStatusCmdReg;		
	OSSynchronizeIO();	
	status++;
	status = *_tfStatusCmdReg;		
	OSSynchronizeIO();	
	status++;


	_devIntSrc->enable();
	
	return result;

}	


// Prevent glitches when switching drives.
IOReturn 
ApplePCCardATA::selectDevice( ataUnitID unit )
{

	_devIntSrc->disable();
	IOReturn result = super::selectDevice( unit);
	_devIntSrc->enable();
	
	return result;


}


#pragma mark -Removable Bus Support-
void
ApplePCCardATA::handleTimeout( void )
{
	if( isBusOnline == false)
	{
		return;
	}

	super::handleTimeout();

}

// media bay support

IOReturn 
ApplePCCardATA::message (UInt32 type, IOService* provider, void* argument)
{

	switch( type )
	{
		case kATARemovedEvent:
		DLOG( "ApplePCCardATA got remove event.\n");
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
			
			// flush any commands in the queue
			handleQueueFlush();
			
			// if there's a command active then call through the command gate
			// and clean it up from inside the workloop.
			// 

			if( _currentCommand != 0)
			{
			
				DLOG( "ApplePCCardATA Calling cleanup bus.\n");
				
					_cmdGate->runAction( (IOCommandGate::Action) 
						&ApplePCCardATA::cleanUpAction,
            			0, 			// arg 0
            			0, 		// arg 1
            			0, 0);						// arg2 arg 3

			
			
			}
			_workLoop->removeEventSource(_cmdGate);
			DLOG( "ApplePCCardATA notify the clients.\n");			
			terminate( );
			getProvider()->close(this);
		}
		break;
		
		default:		
		DLOG( "ApplePCCardATA got some other event = %d\n", (int) type);
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
ApplePCCardATA::handleQueueFlush( void )
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
ApplePCCardATA::checkTimeout( void )
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
ApplePCCardATA::executeCommand(IOATADevice* nub, IOATABusCommand* command)
{
	if( isBusOnline == false)
	{
		return kIOReturnOffline;
	}
		
	return super::executeCommand(nub, command);

}


// called through the commandGate when I get a notification that a media bay has gone away

void
ApplePCCardATA::cleanUpAction(OSObject * owner,
                                               void *     arg0,
                                               void *     arg1,
                                               void *  /* arg2 */,
                                               void *  /* arg3 */)
{


	ApplePCCardATA* self = (ApplePCCardATA*) owner;
	self->cleanUpBus();
}


void
ApplePCCardATA::cleanUpBus(void)
{
	if( _currentCommand != 0)
	{
	
		_currentCommand->setResult(kIOReturnOffline);
		_currentCommand->executeCallback();
		_currentCommand = 0;
	}

}





UInt32 
ApplePCCardATA::scanForDrives( void )
{

	DLOG("ApplePCCardATA scanForDrives forcing reset.");

	// We will set nIEN bit to 0 to force the IRQ line to be driven by the selected
	// device.  We were seeing a small number of cases where the tristated line
	// caused false interrupts to the host.	
	*_tfAltSDevCReg = mATADCRReset;
	OSSynchronizeIO();						
	// ATA standards only dictate >5us of hold time for the soft reset operation
	// 100 us should be sufficient for any device to notice the soft reset.
	IODelay( 100 );
	*_tfAltSDevCReg = 0x00;
	OSSynchronizeIO();
	DLOG("ApplePCCardATA soft reset sequenced\n");

	IODelay( 2000 );  // hold for required 2ms before accessing registers for signature values.

//// do what the super does.

	UInt32 unitsFound = 0;
	UInt8 status = 0x00;
	// count total time spent searching max time allowed = 31 secs
	// it RARELY takes this long.
	UInt32 milsSpent = 0; 
	
	// wait for a not busy bus
	// should be ready, but some devices may be slow to wake or spin up.
	for( int loopMils = 0; milsSpent < 31000; loopMils++ )
	{
		OSSynchronizeIO();
		status = *_tfStatusCmdReg;
		if( (status & mATABusy) == 0x00 )
			break;
		
		IOSleep( 1 );	
		milsSpent++;
	}

	// spun on BSY for too long, declare bus empty
	if( ! (milsSpent < 31000) )
	{
		DLOG("ApplePCCardATA scan failed, bus busy too long\n");
		goto AllDone;
	}	
	
	// select each possible device on the bus, wait for BSY- 
	// then check for protocol signatures.	
	// Master drives only on PC card ports.
	for( int unit = 0; unit < 1; unit++ )
	{

		// wait for a not busy bus
		for( int loopMils = 0; milsSpent < 31000; loopMils++ )
		{
			// write the selection bit
			OSSynchronizeIO();
			*_tfSDHReg	= ( unit << 4 );
			IODelay( 10 );
			// typically, devices respond quickly to selection
			// but we'll give it a chance in case it is slow for some reason.
			status = *_tfStatusCmdReg;
			if( (status & mATABusy) == 0x00 )
			{	
				break;	
			}
			
			IOSleep( 1 );	
			milsSpent++;
		}

		// spun on BSY too long, probably bad device
		if( ! (milsSpent < 31000) )
			goto AllDone;

		// check for ATAPI device signature first
		if ( ( *_tfCylLoReg == 0x14) && ( *_tfCylHiReg == 0xEB) )
		{	
			if(    (unit == 1 )
				&& ( _devInfo[0].type == kATAPIDeviceType )  )
			{

			// OK we've met the condition for an indeterminate bus, master is atapi and we see a slave atapi
			// signature. This is legal ATA, though we are fortunate enough that most devices don't do this.

				if( ATAPISlaveExists( ) != true )
				{
					_devInfo[unit].type = kUnknownATADeviceType;
					goto AllDone;
					
				} 

			} 

			 _devInfo[unit].type = kATAPIDeviceType;
			 _devInfo[unit].packetSend = kATAPIDRQFast;  // this is the safest default setting
			unitsFound++;

		} // check for ATA signature, including status RDY=1 and ERR=0
		else if ( (*_tfCylLoReg == 0x00) && (*_tfCylHiReg == 0x00) &&
				  (*_tfSCountReg == 0x01) && (*_tfSectorNReg == 0x01) &&
				  ( (*_tfAltSDevCReg & 0x51) == 0x50) )
		{
			// check for alternate address for feature/err reg on some older cards at offset 0xD rather than 0x1.
			if( *_tfFeatureReg != 0x01 )
			{
				DLOG("ApplePCCardATA using alternate features address\n");
				_tfFeatureReg += 0xc;
				if( *_tfFeatureReg != 0x01 )
				{
					DLOG("ApplePCCardATA alternate features address not working, unknown card type.\n");
					break;
				}
			}

			 _devInfo[unit].type = kATADeviceType;
			 _devInfo[unit].packetSend = kATAPIUnknown;  
			unitsFound++;
			
		}else{
			DLOG("ApplePCCardATA scan found unknown signature on unit = %d\n", unit);
			DLOG("ApplePCCardATA signature = %x , %x,  \n %x,  %x, \n %x \n", *_tfCylLoReg, *_tfCylHiReg, *_tfSCountReg, *_tfSectorNReg, *_tfAltSDevCReg  );

			_devInfo[unit].type = kUnknownATADeviceType;
			_devInfo[unit].packetSend = kATAPIUnknown;  
		}

	}


AllDone:

	// reselect device 0
	*_tfSDHReg	= 0x00;
	// enable device interrupts
	*_tfAltSDevCReg = 0x00;
	OSSynchronizeIO();

	// enforce ATA device selection protocol
	// before issuing the next command.
	_selectedUnit = kATAInvalidDeviceID;
	
	return unitsFound;


}
