/*
 * Copyright (c) 1998-2004 Apple Computer, Inc. All rights reserved.
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

#include <IOKit/IOMessage.h>
#include <IOKit/ata/IOATATypes.h>
#include <IOKit/ata/IOATAController.h>
#include <IOKit/ata/ATADeviceNub.h>
#include "AppleGenericPCATAChannel.h"
#include "AppleGenericPCATADriver.h"

#define super IOATAController
OSDefineMetaClassAndStructors( AppleGenericPCATADriver, IOATAController )

#ifdef  DEBUG
#define DLOG(fmt, args...)  IOLog(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

//---------------------------------------------------------------------------
//
// Start the generic single-channel ATA controller driver.
//

bool 
AppleGenericPCATADriver::start( IOService * provider )
{
    DLOG("%s::%s( %p, %p )\n", getName(), __FUNCTION__, this, provider);

    // Our provider is a 'nub' that represents a single channel
    // ATA controller.

	_provider = OSDynamicCast( AppleGenericPCATAChannel, provider );
    if ( _provider == 0 )
        goto fail;

    _provider->retain();
    
    // Open our provider.

    if ( _provider->open( this ) != true )
    {
        IOLog("%s: provider open failed\n", getName());
        goto fail;
    }

    if ( super::start(_provider) == false )
    {
        DLOG("%s: super start failed\n", getName());
        goto fail;
    }

    // This driver will handle interrupts using a work loop.
    // Create interrupt event source that will signal the
    // work loop (thread) when a device interrupt occurs.
    
    _intSrc = IOInterruptEventSource::interruptEventSource(
              (OSObject *)             this,
	          (IOInterruptEventAction) &interruptOccurred,
                                       _provider, 0 );

    if ( !_intSrc || !_workLoop ||
         (_workLoop->addEventSource(_intSrc) != kIOReturnSuccess) )
    {
        IOLog("%s: interrupt event source error\n", getName());
        goto fail;
    }
    _intSrc->enable();

    // For each device discovered on the ATA bus (by super),
    // create a nub for that device and call registerService() to
    // trigger matching against that device.

    for ( UInt32 i = 0; i < 2; i++ )
    {
        if ( _devInfo[i].type != kUnknownATADeviceType )
        {
            ATADeviceNub * nub;

			nub = ATADeviceNub::ataDeviceNub( (IOATAController*) this,
                                              (ataUnitID) i,
                                              _devInfo[i].type );

            if ( nub )
            {
                if ( nub->attach( this ) )
                {
                    _nub[i] = (IOATADevice *) nub;
                    _nub[i]->retain();
                    _nub[i]->registerService();
                }
                nub->release();
            }
		}
	}

    IOLog("%s: CMD 0x%x, CTR 0x%x, IRQ %ld\n", getName(),
          _provider->getCommandBlockAddress(),
          _provider->getControlBlockAddress(),
          _provider->getInterruptVector());

    return true;

fail:
    
    if ( _provider )
    {
        _provider->close( this );
    }
    
    return false;
}

/*---------------------------------------------------------------------------
 *
 * Release resources before this object is destroyed.
 *
 ---------------------------------------------------------------------------*/

void
AppleGenericPCATADriver::free( void )
{
#define RELEASE(x) do { if(x) { (x)->release(); (x) = 0; } } while(0)

    DLOG("%s::%s( %p )\n", getName(), __FUNCTION__, this);

    // Release resources created by start(). 

    RELEASE( _intSrc   );
    RELEASE( _nub[0]   );
    RELEASE( _nub[1]   );
    RELEASE( _provider );

    // Release registers created by configureTFPointers().

    RELEASE( _tfDataReg      );
    RELEASE( _tfFeatureReg   );
    RELEASE( _tfSCountReg    );
    RELEASE( _tfSectorNReg   );
    RELEASE( _tfCylLoReg     );
    RELEASE( _tfCylHiReg     );
    RELEASE( _tfSDHReg       );
    RELEASE( _tfStatusCmdReg );
    RELEASE( _tfAltSDevCReg  );

    // IOATAController should release this.

	if ( _doubleBuffer.logicalBuffer )
	{
		IOFree( (void *) _doubleBuffer.logicalBuffer,
                         _doubleBuffer.bufferSize );
		_doubleBuffer.bufferSize     = 0;
		_doubleBuffer.logicalBuffer  = 0;
		_doubleBuffer.physicalBuffer = 0;
	}

    super::free();
}

/*---------------------------------------------------------------------------
 *
 * Return a new work loop object, or the one (we) previously created.
 *
 ---------------------------------------------------------------------------*/

IOWorkLoop *
AppleGenericPCATADriver::getWorkLoop( void ) const
{
    DLOG("%s::%s( %p )\n", getName(), __FUNCTION__, this);

    return ( _workLoop ) ? _workLoop :
                           IOWorkLoop::workLoop();
}

/*---------------------------------------------------------------------------
 *
 * Setup the location of the task file registers.
 *
 ---------------------------------------------------------------------------*/

bool 
AppleGenericPCATADriver::configureTFPointers( void )
{
    DLOG("%s::%s( %p )\n", getName(), __FUNCTION__, this);

    UInt16 cmdBlockAddr = _provider->getCommandBlockAddress();
    UInt16 ctrBlockAddr = _provider->getControlBlockAddress();

    _tfDataReg      = IOATAIOReg16::withAddress( cmdBlockAddr + 0 );
    _tfFeatureReg   = IOATAIOReg8::withAddress(  cmdBlockAddr + 1 );
    _tfSCountReg    = IOATAIOReg8::withAddress(  cmdBlockAddr + 2 );
    _tfSectorNReg   = IOATAIOReg8::withAddress(  cmdBlockAddr + 3 );
    _tfCylLoReg     = IOATAIOReg8::withAddress(  cmdBlockAddr + 4 );
    _tfCylHiReg     = IOATAIOReg8::withAddress(  cmdBlockAddr + 5 );
    _tfSDHReg       = IOATAIOReg8::withAddress(  cmdBlockAddr + 6 );
    _tfStatusCmdReg = IOATAIOReg8::withAddress(  cmdBlockAddr + 7 );
    _tfAltSDevCReg  = IOATAIOReg8::withAddress(  ctrBlockAddr + 2 );

    if ( !_tfDataReg || !_tfFeatureReg || !_tfSCountReg ||
         !_tfSectorNReg || !_tfCylLoReg || !_tfCylHiReg ||
         !_tfSDHReg || !_tfStatusCmdReg || !_tfAltSDevCReg )
    {
        return false;
    }

    return true;
}

/*---------------------------------------------------------------------------
 *
 * The work loop based interrupt handler called by our interrupt event
 * source.
 *
 ---------------------------------------------------------------------------*/

void
AppleGenericPCATADriver::interruptOccurred( OSObject *               owner,
                                            IOInterruptEventSource * evtSrc,
                                            int                      count )
{
	AppleGenericPCATADriver * self = (AppleGenericPCATADriver *) owner;

    // Let our superclass handle the interrupt to advance to the next state
    // in its internal state machine.

	self->handleDeviceInterrupt();
}

/*---------------------------------------------------------------------------
 *
 * Extend the implementation of scanForDrives() from IOATAController
 * to issue a soft reset before scanning for ATA/ATAPI drive signatures.
 *
 ---------------------------------------------------------------------------*/

UInt32 
AppleGenericPCATADriver::scanForDrives( void )
{
    UInt32 unitsFound;

    DLOG("%s::%s( %p )\n", getName(), __FUNCTION__, this);

    *_tfAltSDevCReg = mATADCRReset;

    IODelay( 100 );

    *_tfAltSDevCReg = 0x0;

    IOSleep( 10 );

    unitsFound = super::scanForDrives();

    *_tfSDHReg = 0x00;  // Initialize device selection to device 0.

    return unitsFound;
}

/*---------------------------------------------------------------------------
 *
 * Provide information on the bus capability.
 *
 ---------------------------------------------------------------------------*/

IOReturn AppleGenericPCATADriver::provideBusInfo( IOATABusInfo * infoOut )
{
    UInt32 pioModes;

    DLOG("%s::%s( %p, %p )\n", getName(), __FUNCTION__, this, infoOut);

	if ( infoOut == 0 )
	{
		IOLog("%s::provideBusInfo bad argument\n", getName());
		return -1;
	}

    // Convert to a bit significant mask of all modes equal to
    // or less than the mode number.
    
    pioModes = (1 << (_provider->getPIOMode() + 1)) - 1;

    infoOut->zeroData();
    infoOut->setSocketType( kInternalATASocket );

	infoOut->setPIOModes( pioModes );
	infoOut->setDMAModes( 0x00 );    // no DMA support
	infoOut->setUltraModes( 0x00 );  // no UDMA support

	UInt8 units = 0;
	if ( _devInfo[0].type != kUnknownATADeviceType ) units++;
	if ( _devInfo[1].type != kUnknownATADeviceType ) units++;

	infoOut->setUnits( units );

	return kATANoErr;
}

/*---------------------------------------------------------------------------
 *
 * Return the currently configured timing for the unit number.
 *
 ---------------------------------------------------------------------------*/

IOReturn 
AppleGenericPCATADriver::getConfig( IOATADevConfig * configOut,
                                    UInt32           unitNumber )
{
    const UInt16 cycleTimes[5] = { 600, 383, 240, 180, 120 };
    UInt32       pioMode;

    DLOG("%s::%s( %p, %p, %ld )\n", getName(), __FUNCTION__,
         this, configOut, unitNumber);

	if ( (configOut == 0) || (unitNumber > kATADevice1DeviceID) )
	{
		IOLog("%s::getConfig bad argument\n", getName());
		return -1;	
	}

    pioMode = _provider->getPIOMode();
 
    // The argument for the following functions must specify a single
    // mode. Some examples of valid values:
    //   None   : 00000000b
    //   Mode 0 : 00000001b
    //   Mode 1 : 00000010b

	configOut->setPIOMode( 1 << pioMode );
	configOut->setDMAMode( 0x00 );
    configOut->setUltraMode( 0x00 );

    if ( pioMode >= (sizeof(cycleTimes) / sizeof(cycleTimes[0])) )
         pioMode = sizeof(cycleTimes) / sizeof(cycleTimes[0]) - 1;

	configOut->setPIOCycleTime( cycleTimes[pioMode] );

	configOut->setPacketConfig( _devInfo[unitNumber].packetSend );

	return kATANoErr;
}

/*---------------------------------------------------------------------------
 *
 * Select the bus timing configuration for a particular device.
 * With a more capable controller, we would program our timing
 * registers for the specified device unit. However, for this
 * generic driver, we do nothing.
 *
 ---------------------------------------------------------------------------*/

IOReturn 
AppleGenericPCATADriver::selectConfig( IOATADevConfig * configRequest,
                                       UInt32           unitNumber )
{
    DLOG("%s::%s( %p, %p, %ld )\n", getName(), __FUNCTION__,
         this, configRequest, unitNumber);

	if ( (configRequest == 0) || (unitNumber > kATADevice1DeviceID) )
	{
		DLOG("%s::selectConfig bad argument\n", getName());
		return -1;
	}

	_devInfo[unitNumber].packetSend = configRequest->getPacketConfig();

	return kATANoErr;
}

/*---------------------------------------------------------------------------
 *
 * Flush the outstanding commands in the command queue.
 * Implementation borrowed from MacIOATA in IOATAFamily.
 *
 ---------------------------------------------------------------------------*/

IOReturn
AppleGenericPCATADriver::handleQueueFlush( void )
{
	UInt32 savedQstate = _queueState;

    DLOG("%s::%s( %p )\n", getName(), __FUNCTION__, this);

	_queueState = IOATAController::kQueueLocked;

	IOATABusCommand * cmdPtr = 0;

	while ( cmdPtr = dequeueFirstCommand() )
	{
		cmdPtr->setResult( kIOReturnError );
		cmdPtr->executeCallback();
	}

	_queueState = savedQstate;

	return kATANoErr;
}

/*---------------------------------------------------------------------------
 *
 * Handle termination notification from the provider.
 *
 ---------------------------------------------------------------------------*/

IOReturn
AppleGenericPCATADriver::message( UInt32      type,
                                  IOService * provider,
                                  void *      argument )
{
    if ( ( provider == _provider ) &&
         ( type == kIOMessageServiceIsTerminated ) )
    {
        _provider->close( this );
        return kIOReturnSuccess;
    }

    return super::message( type, provider, argument );
}

/*---------------------------------------------------------------------------
 *
 * Override IOATAController::synchronousIO()
 *
 ---------------------------------------------------------------------------*/

IOReturn AppleGenericPCATADriver::synchronousIO( void )
{
    IOReturn ret;
    
    // IOATAController::synchronousIO() asserts nIEN bit in order to disable
    // drive interrupts during polled mode command execution. The problem is
    // that this will float the INTRQ line and put it in high impedance state,
    // which on certain systems has the undesirable effect of latching a false
    // interrupt on the interrupt controller. Perhaps those systems lack a
    // strong pull down resistor on the INTRQ line. Experiment shows that the
    // interrupt event source is signalled, and its producerCount incremented
    // after every synchronousIO() call. This false interrupt can become
    // catastrophic after reverting to async operations since software can
    // issue a command, handle the false interrupt, and issue another command
    // to the drive before the actual completion of the first command, leading
    // to a irrecoverable bus hang. This function is called after an ATA bus
    // reset. Waking from system sleep will exercise this path.
    // The workaround is to mask the interrupt line while the INTRQ line is
    // floating (or bouncing).

    if (_intSrc) _intSrc->disable();
    ret = super::synchronousIO();
    if (_intSrc) _intSrc->enable();

    return ret;
}
