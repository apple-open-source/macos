/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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

#ifndef _APPLEINTELPIIXATADRIVER_H
#define _APPLEINTELPIIXATADRIVER_H

#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/ata/IOPCIATA.h>
#include <IOKit/pci/IOPCIDevice.h>
#include "AppleIntelPIIXATATiming.h"

class AppleIntelPIIXATADriver : public IOPCIATA
{
    OSDeclareDefaultStructors( AppleIntelPIIXATADriver )

private:
    
    static const UInt32 kMaxDrives = 2;

    /*
     * General and PIIX specific ATA controller properties.
     */
    UInt16                        _irq;
    UInt16                        _ioPorts;
    UInt16                        _ioBMOffset;
    UInt16                        _channel;
    
    /*
     * References to parent(s), and other objects.
     */
    IOPCIDevice *                 _pciDevice;
    IOInterruptEventSource *      _intSrc;
    AppleIntelPIIXATAController * _provider;
    
    /*
     * Currently selected timings for each drive.
     */
    const PIIXTiming *            _pioTiming[  kMaxDrives ];
    const PIIXTiming *            _dmaTiming[  kMaxDrives ];
    const PIIXUDMATiming *        _udmaTiming[ kMaxDrives ];

    /*
     * The timing register values that correspond to the
     * selected timings.
     */
    UInt16                        _idetim[ kMaxDrives ];
    UInt8                         _sidetim;
    UInt8                         _udmactl;
    UInt16                        _udmatim;
    UInt16                        _ideConfig;

    /* Interrupt event source action */
    
    static void interruptOccurred( OSObject *               owner,
                                   IOInterruptEventSource * src,
                                   int                      count );

protected:
    /*
     * PIIX support functions.
     */
    virtual bool configurePCIDevice( IOPCIDevice * device,
                                     UInt16        channel );
    
    virtual bool getBMBaseAddress( IOPCIDevice * provider,
                                   UInt16        channel,
                                   UInt16 *      addrOut );

    virtual void resetTimingsForDevice( ataUnitID unit );

    virtual void writeTimingRegisters( ataUnitID unit = kATADevice0DeviceID );

    virtual void computeUDMATimingRegisters( ataUnitID unit );

	virtual void computeTimingRegisters( ataUnitID unit );

    virtual void selectIOTiming( ataUnitID unit );

    virtual bool setDriveProperty( UInt8        driveUnit,
                                   const char * key,
                                   UInt64       value,
                                   UInt         numberOfBits);

public:
    /* IOService overrides */

    virtual bool start( IOService * provider );

    virtual void free();

    virtual IOWorkLoop * getWorkLoop() const;

    virtual IOReturn message( UInt32      type,
                              IOService * provider,
                              void *      argument );

    /* Mandatory IOATAController overrides */

	virtual bool configureTFPointers();

	virtual IOReturn provideBusInfo( IOATABusInfo * infoOut );

    virtual IOReturn getConfig( IOATADevConfig * configOut,
                                UInt32           unit );

    virtual IOReturn selectConfig( IOATADevConfig * config,
                                   UInt32           unit );

    /* Optional IOATAController overrides */

    virtual UInt32 scanForDrives();

    virtual IOReturn handleQueueFlush();
};

#endif /* !_APPLEINTELPIIXATADRIVER_H */
