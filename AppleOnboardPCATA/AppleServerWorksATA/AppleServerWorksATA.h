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

#ifndef _APPLESERVERWORKSATA_H
#define _APPLESERVERWORKSATA_H

#include <IOKit/IOFilterInterruptEventSource.h>
#include "AppleOnboardPCATA.h"
#include "AppleServerWorksATATiming.h"

struct BusTimings {
    UInt8                   pioModeNumber;
    const TimingParameter * pioTiming;
    UInt8                   dmaModeNumber;
    const TimingParameter * dmaTiming;
    UInt8                   ultraModeNumber;
    bool                    ultraEnabled;
};

struct HardwareInfo {
    UInt32       pciDeviceID;
    UInt8        minPCIRevID;
    UInt8        maxUltraMode;
    const char * deviceName;
};

#define PCI_PIO_TIMING          0x40
#define PIO_TIMING_CH0D0_MASK   0x0000FF00
#define PIO_TIMING_CH0D1_MASK   0x000000FF
#define PIO_TIMING_CH1D0_MASK   0xFF000000
#define PIO_TIMING_CH1D1_MASK   0x00FF0000

#define PCI_DMA_TIMING          0x44
#define DMA_TIMING_CH0D0_MASK   0x0000FF00
#define DMA_TIMING_CH0D1_MASK   0x000000FF
#define DMA_TIMING_CH1D0_MASK   0xFF000000
#define DMA_TIMING_CH1D1_MASK   0x00FF0000

#define PCI_PIO_MODE            0x4A
#define PIO_MODE_CH0D0_MASK     0x000F
#define PIO_MODE_CH0D1_MASK     0x00F0
#define PIO_MODE_CH1D0_MASK     0x0F00
#define PIO_MODE_CH1D1_MASK     0xF000

#define PCI_ULTRA_ENABLE        0x54
#define ULTRA_CH0D0_ENABLE      0x01
#define ULTRA_CH0D1_ENABLE      0x02
#define ULTRA_CH1D0_ENABLE      0x04
#define ULTRA_CH1D1_ENABLE      0x08

#define PCI_ULTRA_MODE          0x56
#define ULTRA_MODE_CH0D0_MASK   0x000F
#define ULTRA_MODE_CH0D1_MASK   0x00F0
#define ULTRA_MODE_CH1D0_MASK   0x0F00
#define ULTRA_MODE_CH1D1_MASK   0xF000

#define PCI_ULTRA_CONTROL       0x5a
#define ULTRA_CTRL_DISABLE      0x40
#define ULTRA_CTRL_MODE_MASK    0x03
#define ULTRA_CTRL_MODE_4       0x02
#define ULTRA_CTRL_MODE_5       0x03

class AppleServerWorksATA : public AppleOnboardPCATA
{
    OSDeclareDefaultStructors( AppleServerWorksATA )

protected:
    IOInterruptEventSource *   fInterruptSource;
    IOWorkLoop *               fWorkLoop;
    BusTimings                 fBusTimings[ kMaxDriveCount ];
    bool                       f80PinCable[ kMaxDriveCount ];
    UInt16                     fBMBaseAddr;
    UInt32                     fUltraModeMask;
    const HardwareInfo *       fHWInfo;

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
                               UInt16 * baseAddr );

    virtual void         resetBusTimings( void );

    virtual void         selectTimingParameter(
                               IOATADevConfig * configRequest,
                               UInt32           unitNumber );

    virtual void         programTimingRegisters( void );

    virtual void         initializeHardware( void );

    virtual void         restoreHardwareState( void );

    virtual void         dumpHardwareRegisters( void );

public:
    virtual bool         start( IOService * provider );

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

#endif /* !_APPLESERVERWORKSATA_H */
