/*
 * Copyright (c) 1998-2003 Apple Computer, Inc. All rights reserved.
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

#ifndef _APPLEINTELPIIXPATA_H
#define _APPLEINTELPIIXPATA_H

#include <IOKit/IOFilterInterruptEventSource.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/ata/IOATATypes.h>
#include <IOKit/ata/IOPCIATA.h>
#include <IOKit/ata/IOATAController.h>
#include <IOKit/ata/ATADeviceNub.h>
#include "AppleIntelPIIXATATiming.h"
#include "AppleIntelPIIXATAHW.h"
#include "AppleIntelPIIXATAKeys.h"
#include "AppleIntelPIIXATAChannel.h"

class AppleIntelPIIXPATA : public IOPCIATA
{
    OSDeclareDefaultStructors( AppleIntelPIIXPATA )

protected:
    
    static const UInt32 kMaxDrives = 2;

    /*
     * General and PIIX specific ATA controller properties.
     */
    UInt16                        _cmdBlock;
    UInt16                        _ctrBlock;
    UInt16                        _ioBMOffset;
    UInt32                        _channel;

    /*
     * References to parent(s), and other objects.
     */
    IOPCIDevice *                 _pciDevice;
    IOInterruptEventSource *      _intSrc;
    AppleIntelPIIXATAChannel *    _provider;

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

    /* Interrupt event source filter */

    static bool interruptFilter( OSObject * owner,
                                 IOFilterInterruptEventSource * src );

    /*
     * PIIX support functions.
     */
    virtual bool configurePCIDevice( IOPCIDevice * device,
                                     UInt32        channel );

    virtual bool getBMBaseAddress( IOPCIDevice * provider,
                                   UInt32        channel,
                                   UInt16 *      addrOut );

    virtual void resetTimingsForDevice( ataUnitID unit );

    virtual void writeTimingRegisters( ataUnitID unit = kATADevice0DeviceID );

    virtual void computeUDMATimingRegisters( ataUnitID unit );

    virtual void computeTimingRegisters( ataUnitID unit );

    virtual void selectIOTiming( ataUnitID unit );

    virtual bool setDriveProperty( UInt32       driveUnit,
                                   const char * key,
                                   UInt32       value,
                                   UInt32       numberOfBits);

	virtual IOReturn synchronousIO( void );

public:
    /* IOService overrides */

    virtual bool start( IOService * provider );

    virtual void free( void );

    virtual IOWorkLoop * getWorkLoop( void ) const;

    virtual IOReturn message( UInt32      type,
                              IOService * provider,
                              void *      argument );

    /* Mandatory IOATAController overrides */

    virtual bool configureTFPointers( void );

    virtual IOReturn provideBusInfo( IOATABusInfo * infoOut );

    virtual IOReturn getConfig( IOATADevConfig * configOut,
                                UInt32           unit );

    virtual IOReturn selectConfig( IOATADevConfig * config,
                                   UInt32           unit );

    /* Optional IOATAController overrides */

    virtual UInt32 scanForDrives( void );

    virtual IOReturn handleQueueFlush( void );
};

#endif /* !_APPLEINTELPIIXPATA_H */
