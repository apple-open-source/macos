/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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

#include <IOKit/IOLib.h>
#include <IOKit/IOPlatformExpert.h>
#include "Apple8259PIC.h"
#include "PICShared.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super IOInterruptController

OSDefineMetaClassAndStructors( Apple8259PICInterruptController,
                               IOInterruptController )

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool Apple8259PIC::start( IOService * provider )
{
    IOInterruptAction handler;
    const OSSymbol *  name;

    if ( super::start(provider) == false ) return false;

    _handleSleepWakeFunction = OSSymbol::withCString(
                               kHandleSleepWakeFunction );
    if (!_handleSleepWakeFunction) return false;

    _interruptLock = IOSimpleLockAlloc();
    if (!_interruptLock) return false;

    // Allocate the memory for the vectors

    vectors = IONew( IOInterruptVector, kNumVectors );
    if ( 0 == vectors ) return false;

    bzero( vectors, sizeof(IOInterruptVector) * kNumVectors );

    // Allocate locks for the vectors

    for ( int cnt = 0; cnt < kNumVectors; cnt++ )
    {
        vectors[cnt].interruptLock = IOLockAlloc();
        if ( 0 == vectors[cnt].interruptLock )
            return false;
    }

    // Mask all interrupts except for the cascade line

    _interruptMasks = 0xffff & ~(1 << kPICSlaveID);

    // Initialize master PIC

    initializePIC( kPIC1BasePort,
      /* ICW1 */   kPIC_ICW1_IC4,
      /* ICW2 */   kBaseIOInterruptVectors,
      /* ICW3 */   (1 << kPICSlaveID),
      /* ICW4 */   kPIC_ICW4_uPM );

    // Write to OCW1, OCW3, OCW2
    // The priority order is changed to (highest to lowest)
    // 3 4 5 6 7 0 1 2
    // The default priority after initialization is (highest to lowest)
    // 0 1 2 3 4 5 6 7

    outb( kPIC_OCW1(kPIC1BasePort), _interruptMasks & 0xff );
    outb( kPIC_OCW3(kPIC1BasePort), kPIC_OCW3_MBO | kPIC_OCW3_RR );
    outb( kPIC_OCW2(kPIC1BasePort), kPIC_OCW2_R   |
                                    kPIC_OCW2_SL  |
                                    kPIC_OCW2_LEVEL(2) );
    
    // Initialize slave PIC

    initializePIC( kPIC2BasePort,
      /* ICW1 */   kPIC_ICW1_IC4,
      /* ICW2 */   kBaseIOInterruptVectors + 8,
      /* ICW3 */   kPICSlaveID,
      /* ICW4 */   kPIC_ICW4_uPM );

    // Write to OCW1, and OCW3

    outb( kPIC_OCW1(kPIC2BasePort), _interruptMasks >> 8 );
    outb( kPIC_OCW3(kPIC2BasePort), kPIC_OCW3_MBO | kPIC_OCW3_RR );

    // Save the trigger type register setup by the BIOS

    _interruptTriggerTypes = inb( kPIC1TriggerTypePort ) |
                           ( inb( kPIC2TriggerTypePort ) << 8 );

    // Platform driver may wait on the PIC

    registerService();

    // Primary interrupt controller

    getPlatform()->setCPUInterruptProperties( provider );

    // Register the interrupt handler function so it can service interrupts.

    handler = getInterruptHandlerAddress();
    if ( provider->registerInterrupt(0, this, handler, 0) != kIOReturnSuccess )
        IOPanic("8259-PIC: registerInterrupt failed");

    provider->enableInterrupt(0);

    // Register this interrupt controller so clients can find it.
    // Get the interrupt controller name from the provider's properties.

    name = OSSymbol::withString( (OSString *)
                     provider->getProperty(kInterruptControllerNameKey) );
    if ( 0 == name )
    {
        IOLog("8259-PIC: no interrupt controller name\n");
        return false;
    }

    getPlatform()->registerInterruptController( (OSSymbol *) name, this );
    name->release();

    return true;
}

//---------------------------------------------------------------------------
// Free the interrupt controller object. Deallocate all resources.

void Apple8259PIC::free( void )
{
    if ( vectors )
    {
        for ( int cnt = 0; cnt < kNumVectors; cnt++ )
        {
            if (vectors[cnt].interruptLock)
                IOLockFree(vectors[cnt].interruptLock);
        }
        IODelete( vectors, IOInterruptVector, kNumVectors );
        vectors = 0;
    }

    if ( _handleSleepWakeFunction )
    {
        _handleSleepWakeFunction->release();
        _handleSleepWakeFunction = 0;
    }

    if ( _interruptLock )
    {
        IOSimpleLockFree( _interruptLock );
        _interruptLock = 0;
    }

    super::free();
}

//---------------------------------------------------------------------------
// Initialize the PIC by sending the Initialization Command Words (ICW).

void Apple8259PIC::initializePIC( UInt16 port,
                                  UInt8  icw1, UInt8 icw2,
                                  UInt8  icw3, UInt8 icw4 )
{
    // Initialize 8259's. Start the initialization sequence by
    // issuing ICW1 (Initialization Command Word 1).
    // Bit 4 must be set.

    outb( kPIC_ICW1(port), kPIC_ICW1_MBO | icw1 );

    // ICW2
    // Upper 5 bits of the interrupt vector address. The lower three
    // bits are set according to the interrupt level serviced.

    outb( kPIC_ICW2(port), icw2 );

    // ICW3 (Master Device)
    // Set a 1 bit for each IR line that has a slave.

    outb( kPIC_ICW3(port), icw3 );

    // ICW4

    outb( kPIC_ICW4(port), icw4 );
}

//---------------------------------------------------------------------------
// Report whether the interrupt line is edge or level triggered.

int Apple8259PIC::getVectorType( long vectorNumber,
                                 IOInterruptVector * vector )
{
    if (_interruptTriggerTypes & (1 << vectorNumber))
        return kIOInterruptTypeLevel;
    else
        return kIOInterruptTypeEdge;
}

IOReturn Apple8259PIC::getInterruptType( IOService * nub,
                                         int         source,
                                         int *       interruptType )
{
    IOInterruptSource * interruptSources;
    OSData            * vectorData;
  
    if (!nub || !interruptType)
    {
        return kIOReturnBadArgument;
    }

    interruptSources = nub->_interruptSources;
    vectorData = interruptSources[source].vectorData;

    if (vectorData->getLength() < sizeof(UInt64))
    {
        // On legacy platforms, report the default trigger type
        // configured by the BIOS.

        UInt32 vectorNumber = DATA_TO_VECTOR(vectorData);
        *interruptType = getVectorType(vectorNumber, 0);
    }
    else
    {
        // On ACPI platforms, report the proposed interrupt trigger
        // type configured by the platform driver, which has not yet
        // been applied to the hardware. The latter will happen when
        // the interrupt is registered and our initVector() function
        // is called. IOInterruptEventSource will grab the interrupt
        // type first and then register an interrupt handler.

        UInt32 vectorFlags = DATA_TO_FLAGS(vectorData);
        *interruptType = ((vectorFlags & kInterruptTriggerModeMask) ==
                           kInterruptTriggerModeEdge) ?
                         kIOInterruptTypeEdge : kIOInterruptTypeLevel;
    }

    APIC_LOG("PIC: %s( %s, %d ) = %s (vector %ld)\n",
             __FUNCTION__,
             nub->getName(), source,
             *interruptType == kIOInterruptTypeLevel ? "level" : "edge",
             DATA_TO_VECTOR(vectorData));

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

IOReturn Apple8259PIC::setVectorType( long vectorNumber, long vectorType )
{
    IOInterruptState state;

    if ( vectorNumber >= kNumVectors || vectorNumber == kPICSlaveID ||
         ( vectorType != kIOInterruptTypeLevel &&
           vectorType != kIOInterruptTypeEdge ) )
    {
        return kIOReturnBadArgument;
    }

    state = IOSimpleLockLockDisableInterrupt( _interruptLock );

    if ( vectorType == kIOInterruptTypeLevel )
        _interruptTriggerTypes |= ( 1 << vectorNumber );
    else
        _interruptTriggerTypes &= ~( 1 << vectorNumber );        

    outb( kPIC1TriggerTypePort, (UInt8) _interruptTriggerTypes );
    outb( kPIC2TriggerTypePort, (UInt8)(_interruptTriggerTypes >> 8));

    IOSimpleLockUnlockEnableInterrupt( _interruptLock, state );

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

IOInterruptAction Apple8259PIC::getInterruptHandlerAddress( void )
{
    return (IOInterruptAction) &Apple8259PIC::handleInterrupt;
}

//---------------------------------------------------------------------------
// Handle an interrupt by servicing the 8259, and dispatch the
// handler associated with the interrupt vector.

IOReturn Apple8259PIC::handleInterrupt( void *      savedState,
                                        IOService * nub,
                                        int         source )
{
    IOInterruptVector * vector;
    long                vectorNumber;
    long                level;
    void *              refCon;

    vectorNumber = source - kBaseIOInterruptVectors;

    if (vectorNumber < 0 || vectorNumber >= kNumVectors)
        return kIOReturnSuccess;

    level = (_interruptTriggerTypes & (1 << vectorNumber));
    if (0 == level) ackInterrupt( vectorNumber );

    vector = &vectors[ vectorNumber ];

    // AppleIntelClock needs interrupt state.
    refCon = ((vectorNumber == 0) ? savedState : vector->refCon);

    vector->interruptActive = 1;

    if ( !vector->interruptDisabledSoft && vector->interruptRegistered )
    {
        vector->handler( vector->target, refCon,
                         vector->nub,    vector->source );

        // interruptDisabledSoft flag may be set by the
        // vector handler to indicate that the interrupt
        // should now be disabled.

        if ( vector->interruptDisabledSoft )
        {
            vector->interruptDisabledHard = 1;
            disableVectorHard( vectorNumber, vector );
        }
    }
    else
    {
        vector->interruptDisabledHard = 1;
        disableVectorHard( vectorNumber, vector );
    }

    if (level) ackInterrupt( vectorNumber );

    vector->interruptActive = 0;
        
    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

bool Apple8259PIC::vectorCanBeShared( long vectorNumber,
                                      IOInterruptVector * vector )
{
    if (getVectorType(vectorNumber, vector) == kIOInterruptTypeLevel)
        return true;   // level-triggered
    else
        return false;  // edge-triggered
}

//---------------------------------------------------------------------------

void Apple8259PIC::initVector( long vectorNumber,
                               IOInterruptVector * vector )
{
    IOInterruptSource * interruptSources;
    UInt32              vectorFlags;
    OSData *            vectorData;

    // Get the vector flags assigned by the platform driver.
    // Unlike the APIC driver, the PIC driver can be loaded
    // on a non-ACPI platform driver, which does not report
    // any vector flags.

    interruptSources = vector->nub->_interruptSources;
    vectorData = interruptSources[vector->source].vectorData;
    if (vectorData->getLength() >= sizeof(UInt64))
    {
        vectorFlags = DATA_TO_FLAGS( vectorData );

        if ((vectorFlags & kInterruptTriggerModeMask) ==
             kInterruptTriggerModeEdge)
            setVectorType(vectorNumber, kIOInterruptTypeEdge);
        else
            setVectorType(vectorNumber, kIOInterruptTypeLevel);
    }
}

//---------------------------------------------------------------------------

void Apple8259PIC::disableVectorHard( long vectorNumber,
                                      IOInterruptVector * vector )
{
    IOInterruptState state;

    // cacade/slave interrupt line cannot be disable
    if ( vectorNumber == kPICSlaveID ) return;

    state = IOSimpleLockLockDisableInterrupt( _interruptLock );
    disableInterrupt( vectorNumber );
    IOSimpleLockUnlockEnableInterrupt( _interruptLock, state );
}

//---------------------------------------------------------------------------

void Apple8259PIC::enableVector( long vectorNumber,
                                 IOInterruptVector * vector )
{
    IOInterruptState state;
    state = IOSimpleLockLockDisableInterrupt( _interruptLock );
    enableInterrupt( vectorNumber );
    IOSimpleLockUnlockEnableInterrupt( _interruptLock, state );
}

//---------------------------------------------------------------------------

void Apple8259PIC::prepareForSleep( void )
{
    // Mask all interrupts. Don't assume that the IRQ lines will remain
    // masked on wake. See comment in resumeFromSleep().
    outb( kPIC_OCW1(kPIC2BasePort), 0xff );
    outb( kPIC_OCW1(kPIC1BasePort), 0xff );
}

//---------------------------------------------------------------------------

void Apple8259PIC::resumeFromSleep( void )
{
    // Masking all interrupts again on wake is not simply paranoia,
    // it is absolutely required for some machines such as Dell C610.
    // Observed that all IRQ lines came up unmasked on wake (BIOS ?).
    // IRQ 0 and IRQ 1 (Timer, PS/2 Keyboard) interrupts still comes
    // through,  but no other interrupts are dispatched for service,
    // including SCI at IRQ 9, IRQ 14/15, and PCI at IRQ 11. Masking
    // all lines again seems to reset the interrupt trigger logic.

    outb( kPIC_OCW1(kPIC2BasePort), 0xff );
    outb( kPIC_OCW1(kPIC1BasePort), 0xff );

    // Restore trigger type registers.

    outb( kPIC1TriggerTypePort, (UInt8) _interruptTriggerTypes );
    outb( kPIC2TriggerTypePort, (UInt8)(_interruptTriggerTypes >> 8));

    // Initialize master PIC.

    initializePIC( kPIC1BasePort,
      /* ICW1 */   kPIC_ICW1_IC4,
      /* ICW2 */   kBaseIOInterruptVectors,
      /* ICW3 */   (1 << kPICSlaveID),
      /* ICW4 */   kPIC_ICW4_uPM );

    // Write to OCW1, OCW3, OCW2.
    // The priority order is changed to (highest to lowest)
    // 3 4 5 6 7 0 1 2
    // The default priority after initialization is (highest to lowest)
    // 0 1 2 3 4 5 6 7

    outb( kPIC_OCW1(kPIC1BasePort), _interruptMasks & 0xff );
    outb( kPIC_OCW3(kPIC1BasePort), kPIC_OCW3_MBO | kPIC_OCW3_RR );
    outb( kPIC_OCW2(kPIC1BasePort), kPIC_OCW2_R   |
                                    kPIC_OCW2_SL  |
                                    kPIC_OCW2_LEVEL(2) );
    
    // Initialize slave PIC.

    initializePIC( kPIC2BasePort,
      /* ICW1 */   kPIC_ICW1_IC4,
      /* ICW2 */   kBaseIOInterruptVectors + 8,
      /* ICW3 */   kPICSlaveID,
      /* ICW4 */   kPIC_ICW4_uPM );

    // Write to OCW1, and OCW3.

    outb( kPIC_OCW1(kPIC2BasePort), _interruptMasks >> 8 );
    outb( kPIC_OCW3(kPIC2BasePort), kPIC_OCW3_MBO | kPIC_OCW3_RR );
}

//---------------------------------------------------------------------------

IOReturn Apple8259PIC::callPlatformFunction( const OSSymbol * function,
                                             bool   waitForFunction,
                                             void * param1, void * param2,
                                             void * param3, void * param4 )
{
    if ( function == _handleSleepWakeFunction )
    {
        if ( param1 )
            prepareForSleep();   /* prior to system sleep */
        else
            resumeFromSleep();   /* after system wake */

        return kIOReturnSuccess;
    }

    return super::callPlatformFunction( function, waitForFunction,
                                        param1, param2, param3, param4 );
}
