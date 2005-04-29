/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 2.0 (the
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

#ifndef _APPLEONBOARDPCATA_H
#define _APPLEONBOARDPCATA_H

#include <IOKit/ata/IOATATypes.h>
#include <IOKit/ata/IOPCIATA.h>
#include <IOKit/ata/ATADeviceNub.h>
#include "AppleOnboardPCATAShared.h"
#include "AppleOnboardPCATAChannel.h"

class AppleOnboardPCATA : public IOPCIATA
{
    OSDeclareAbstractStructors( AppleOnboardPCATA )

protected:
    AppleOnboardPCATAChannel * fChannelNub;
    UInt32                     fChannelNumber;
    bool                       fHardwareLostPower;

    virtual bool     publishDriveProperty(
                               UInt32       driveUnit,
                               const char * propKey,
                               UInt32       value );

    virtual bool     openATAChannel( IOService * provider );

    virtual void     closeATAChannel( void );

    virtual void     attachATADeviceNubs( void );

    virtual IOReturn synchronousIO( void );

    virtual void     restoreHardwareState( void );

    virtual void     initForPM( IOService * provider );

    virtual void     free( void );

public:
    virtual bool     start( IOService * provider );

    virtual void     stop( IOService * provider );

    virtual IOReturn message( UInt32      type,
                              IOService * provider,
                              void *      argument );

    virtual bool     configureTFPointers( void );

    virtual UInt32   scanForDrives( void );

    virtual IOReturn handleQueueFlush( void );

    virtual IOReturn createChannelCommands( void );

    virtual bool     allocDMAChannel( void );

    virtual bool     freeDMAChannel( void );

    virtual void     initATADMAChains( PRD * descPtr );

    enum {
        kPowerStateOff = 0,
        kPowerStateDoze,
        kPowerStateOn,
        kPowerStateCount
    };

    virtual IOReturn setPowerState(
                               unsigned long stateIndex,
                               IOService *   whatDevice );
};

#endif /* !_APPLEONBOARDPCATA_H */
