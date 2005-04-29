/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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

#ifndef __APPLEAC97AUDIOAMDCS5535_H
#define __APPLEAC97AUDIOAMDCS5535_H

#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOFilterInterruptEventSource.h>
#include "IOAC97Controller.h"
#include "IOAC97CodecDevice.h"

/*
 * PRD Table Vector
 */
struct PRDVector {
    UInt32  base;   /* physical address */
    UInt32  count;  /* byte count + flags */
};

/*
 * PRD Flags on second dword
 */
enum {
    kPRD_EOT = 0x80000000,
    kPRD_EOP = 0x40000000,
    kPRD_JMP = 0x20000000
};

/*
 * ACC I/O Registers
 */
#define ACC_CODEC_STATUS                   0x08
#define ACC_CODEC_CNTL                     0x0C
#define ACC_IRQ_STATUS                     0x12
#define ACC_ENGINE_CNTL                    0x14
#define ACC_BM0_PNTR                       0x60

/*
 * DMA Engine Register Offset
 */
#define BM_CMD                             0x00
#define BM_STATUS                          0x01
#define BM_PRD_POINTER                     0x04

#define ACC_CODEC_CMD_READ                 (1 << 31)
#define ACC_CODEC_CMD_WRITE                (0 << 31)
#define ACC_CODEC_CMD_NEW                  (1 << 16)
#define ACC_CODEC_CMD_ADDR_SHIFT           24
#define ACC_CODEC_CMD_ADDR_MASK            (0x7F << 24)
#define ACC_CODEC_CMD_DATA_MASK            0x0000FFFF

#define ACC_CODEC_STATUS_NEW               (1 << 17)
#define ACC_CODEC_STATUS_DATA_MASK         0x0000FFFF
#define ACC_CODEC_STATUS_PRI_READY         (1 << 23)
#define ACC_CODEC_STATUS_SEC_READY         (1 << 22)

#define BM_CMD_RW                          0x08
#define BM_CMD_BIG_ENDIAN                  0x04
#define BM_CMD_DISABLE                     0x00
#define BM_CMD_ENABLE                      0x01
#define BM_CMD_PAUSE                       0x03

#define BM_STATUS_ERROR                    0x02
#define BM_STATUS_EOP                      0x01


class AppleAC97AudioAMDCS5535 : public IOAC97Controller
{
    OSDeclareDefaultStructors( AppleAC97AudioAMDCS5535 )

protected:
    enum {
        kEngineIdle      = 0x00,
        kEngineActive    = 0x01,
        kEngineRunning   = 0x02,
        kEngineInterrupt = 0x80
    };

    struct DMAEngineState
    {
        IOOptionBits               flags;
        UInt16                     ioBase;
        IOBufferMemoryDescriptor * sampleMemory;
        IOPhysicalAddress          sampleMemoryPhysAddr;
        IOBufferMemoryDescriptor * prdMemory;
        PRDVector *                prdBasePtr;
        IOPhysicalAddress          prdPhysAddr;
        UInt32                     prdBufferSize;
        bool                       interruptReady;
        void *                     interruptTarget;
        IOAC97DMAEngineAction      interruptAction;
        void *                     interruptParam;
    };

    IOPCIDevice *                  fPCI;
    IOWorkLoop *                   fWorkLoop;
    IOFilterInterruptEventSource * fInterruptSource;
    bool                           fACLinkPowerDown;
    DMAEngineState *               fDMAState;
    UInt16                         fIOBase;
    thread_call_t                  fSetPowerStateThreadCall;
    IOAC97CodecDevice *            fCodecs[ kIOAC97MaxCodecCount ];
    UInt32                         fBusyOutputSlots;

    static void          interruptOccurred(
                                 OSObject * owner,
                                 IOInterruptEventSource * source,
                                 int count );

    static bool          interruptFilter(
                                 OSObject * owner,
                                 IOFilterInterruptEventSource * source );

    static void          handleSetPowerState(
                                 thread_call_param_t param0,
                                 thread_call_param_t param1 );

    virtual bool         serviceDMAEngineInterrupt(
                                 const DMAEngineState * dma );

    virtual bool         selectDMAEngineForConfiguration(
                                 IOAC97AudioConfig * config );

    virtual bool         selectSlotMapsForConfiguration(
                                 IOAC97AudioConfig * config );

    virtual bool         hwActivateConfiguration(
                                 const IOAC97AudioConfig * config );

    virtual void         hwDeactivateConfiguration(
                                 const IOAC97AudioConfig * config );

    virtual IOItemCount  attachCodecDevices( void );

    virtual void         publishCodecDevices( void );

    virtual IOAC97CodecDevice *
                         createCodecDevice( IOAC97CodecID codecID );
    
    virtual bool         waitCodecReady( IOAC97CodecID codecID );

    virtual IOReturn     waitACLinkNotBusy( void );

    enum {
        kColdReset, kWarmReset
    };

    virtual void         resetACLink( IOOptionBits type );

    virtual bool         configureProvider( IOService * provider );

public:
    virtual bool         start( IOService * provider );

    virtual void         stop( IOService * provider );

    virtual void         free( void );

    virtual IOWorkLoop * getWorkLoop( void ) const;

    virtual IOReturn     startDMAEngine(
                                 IOAC97DMAEngineID engine,
                                 IOOptionBits      options = 0 );

    virtual void         stopDMAEngine(
                                 IOAC97DMAEngineID engine );

    virtual IOByteCount  getDMAEngineHardwarePointer(
                                 IOAC97DMAEngineID engine );

    virtual IOReturn     prepareAudioConfiguration(
                                 IOAC97AudioConfig * config );

    virtual IOReturn     activateAudioConfiguration(
                                 IOAC97AudioConfig *   config,
                                 void *                target,
                                 IOAC97DMAEngineAction action,
                                 void *                param );

    virtual void         deactivateAudioConfiguration(
                                 IOAC97AudioConfig * config );

    virtual IOReturn     codecRead(
                                 IOAC97CodecID     codec,
                                 IOAC97CodecOffset offset,
                                 IOAC97CodecWord * word );

    virtual IOReturn     codecWrite(
                                 IOAC97CodecID     codec,
                                 IOAC97CodecOffset offset,
                                 IOAC97CodecWord   word );

    virtual IOReturn     setPowerState(
                                 unsigned long powerState,
                                 IOService *   policyMaker );
};

#endif /* !__APPLEAC97AUDIOAMDCS5535_H */
