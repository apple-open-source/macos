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

#ifndef _APPLEAMDCS5535ATA_H
#define _APPLEAMDCS5535ATA_H

#include <IOKit/IOFilterInterruptEventSource.h>
#include "AppleOnboardPCATA.h"
#include "AppleAMDCS5535ATATiming.h"

struct BusTimings
{
    UInt32                  pioModeNumber;
    const TimingParameter * pioTiming;
    UInt32                  dmaModeNumber;
    const TimingParameter * dmaTiming;
    UInt32                  ultraModeNumber;
    const TimingParameter * ultraTiming;
};

#define ATAC_GLD_MSR_CONFIG	  	0x51300001
#define ATAC_GLD_MSR_SMI 	  	0x51300002
#define ATAC_GLD_MSR_ERROR 	  	0x51300003
#define ATAC_GLD_MSR_PM 	  	0x51300004
#define ATAC_GLD_MSR_DIAG 	  	0x51300005

#define ATAC_IO_BAR    			0x51300008
#define ATAC_RESET    			0x51300010
#define ATAC_CH0D0_PIO    		0x51300020
#define ATAC_CH0D0_DMA    		0x51300021
#define ATAC_CH0D1_PIO    		0x51300022
#define ATAC_CH0D1_DMA    		0x51300023
#define ATAC_PCI_ABRTERR        0x51300024

#define BM_IDE_BAR_EN           0x00000001
#define BM_IDE_BAR_MASK         0x0000FFF8

class AppleAMDCS5535ATA : public AppleOnboardPCATA
{
    OSDeclareDefaultStructors( AppleAMDCS5535ATA )

protected:
    IOInterruptEventSource *   fInterruptSource;
    IOWorkLoop *               fWorkLoop;
    BusTimings                 fBusTimings[ kMaxDriveCount ];
    bool                       f80PinCable[ kMaxDriveCount ];
    UInt16                     fBMBaseAddr;

    /* Interrupt event source action */

    static void          interruptOccurred(
                               OSObject * owner,
                               IOInterruptEventSource * source,
                               int count );

    /* Interrupt event source filter */

    static bool          interruptFilter(
                               OSObject * owner,
                               IOFilterInterruptEventSource * source );

    virtual bool         getBMBaseAddress(
                               UInt32   channel,
                               UInt16 * baseAddr );

    virtual void         resetBusTimings( void );

    virtual void         selectTimingParameter(
                               IOATADevConfig * configRequest,
                               UInt32           unitNumber );

    virtual void         programTimingRegisters( void );

    virtual void         restoreHardwareState( void );

    virtual void         dumpHardwareRegisters( void );

public:
    virtual bool         start( IOService * provider );

    virtual void         stop( IOService * provider );

    virtual void         free( void );

    virtual IOWorkLoop * getWorkLoop( void ) const;

    virtual IOReturn     provideBusInfo(
                               IOATABusInfo * infoOut );

    virtual IOReturn     getConfig(
                               IOATADevConfig * configOut,
                               UInt32           unit );

    virtual IOReturn     selectConfig(
                               IOATADevConfig * config,
                               UInt32           unit );
};

#endif /* !_APPLEAMDCS5535ATA_H */
