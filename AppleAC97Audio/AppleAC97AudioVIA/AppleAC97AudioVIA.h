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

#ifndef __APPLEAC97AUDIOVIA_H
#define __APPLEAC97AUDIOVIA_H

#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOFilterInterruptEventSource.h>
#include "IOAC97Controller.h"
#include "IOAC97CodecDevice.h"

/*
 * SGD Table Vector
 */
struct SGDVector {
    UInt32  base;   /* physical address */
    UInt32  count;  /* byte count + flags */
};

/*
 * SGD Flags
 */
enum {
    kSGD_EOL  = 0x80000000,
    kSGD_FLAG = 0x40000000,
    kSGD_STOP = 0x20000000
};

/*
 * PCI Config Space Registers
 */
#define VIA_PCI_ACLINK_STATUS              0x40
#define VIA_PCI_ACLINK_CONTROL             0x41
#define VIA_PCI_FUNCTION_ENABLE            0x42
#define VIA_PCI_PNP_CONTROL                0x43
#define VIA_PCI_MC97_CONTROL               0x44
#define VIA_PCI_FM_NMI_CONTROL             0x48
#define VIA_PCI_SPDIF_CONTROL              0x49

#define VIA_ACLINK_STATUS_SEC_CODEC_READY  0x04
#define VIA_ACLINK_STATUS_CODEC_LOW_POWER  0x02
#define VIA_ACLINK_STATUS_PRI_CODEC_READY  0x01

#define VIA_ACLINK_CONTROL_ENABLE          0x80
#define VIA_ACLINK_CONTROL_DEASSERT_RESET  0x40
#define VIA_ACLINK_CONTROL_FORCE_SYNC      0x20
#define VIA_ACLINK_CONTROL_FORCE_SDO       0x10
#define VIA_ACLINK_CONTROL_VRA             0x08
#define VIA_ACLINK_CONTROL_PCM             0x04
#define VIA_ACLINK_CONTROL_FM              0x02
#define VIA_ACLINK_CONTROL_SB              0x01

#define VIA_SPDIF_DX3                      0x08
#define VIA_SPDIF_SLOT_MASK                0x03
#define VIA_SPDIF_SLOT_6_9                 0x03
#define VIA_SPDIF_SLOT_7_8                 0x02
#define VIA_SPDIF_SLOT_3_4                 0x01
#define VIA_SPDIF_SLOT_10_11               0x00

/*
 * I/O Base 0 Registers
 */
#define VIA_REG_CODEC                      0x80

#define VIA_CODEC_ID_SHIFT                 30
#define VIA_CODEC_ID_MASK                  (3 << 30)
#define VIA_CODEC_ID_PRI                   (0 << 30)
#define VIA_CODEC_ID_SEC                   (1 << 30)
#define VIA_CODEC_SEC_DATA_VALID           (1 << 27)
#define VIA_CODEC_PRI_DATA_VALID           (1 << 25)
#define VIA_CODEC_BUSY                     (1 << 24)
#define VIA_CODEC_WRITE                    0
#define VIA_CODEC_READ                     (1 << 23)
#define VIA_CODEC_INDEX_SHIFT              16
#define VIA_CODEC_INDEX_MASK               (0x7F << 16)
#define VIA_CODEC_DATA_MASK                0xFFFF

/*
 * DMA Engine Register Offset
 */
#define VIA_DMA_SGD_STATUS                 0x00
#define VIA_DMA_SGD_CONTROL                0x01
#define VIA_DMA_SGD_TYPE                   0x02
#define VIA_DMA_SGD_TABLE_PTR              0x04
#define VIA_DMA_SGD_STOP_INDEX             0x08
#define VIA_DMA_SGD_CURRENT_COUNT          0x0C
#define VIA_DMA_SGD_CURRENT_INDEX          0x0F

#define VIA_SGD_STATUS_ACTIVE              0x80
#define VIA_SGD_STATUS_PAUSED              0x40
#define VIA_SGD_STATUS_TRIGGER_QUEUED      0x08
#define VIA_SGD_STATUS_STOPPED             0x04
#define VIA_SGD_STATUS_EOL                 0x02
#define VIA_SGD_STATUS_FLAG                0x01

#define VIA_SGD_CONTROL_START              0x80
#define VIA_SGD_CONTROL_TERMINATE          0x40
#define VIA_SGD_CONTROL_AUTOSTART          0x20
#define VIA_SGD_CONTROL_PAUSE              0x08
#define VIA_SGD_CONTROL_INT_STOP           0x04
#define VIA_SGD_CONTROL_INT_EOL            0x02
#define VIA_SGD_CONTROL_INT_FLAG           0x01
#define VIA_SGD_CONTROL_RESET              0x01

#define VIA_SGD_STOP_INDEX_16BIT           0x00200000
#define VIA_SGD_STOP_INDEX_STEREO          0x00100000
#define VIA_SGD_STOP_INDEX_48K             0x000FFFFF
#define VIA_SGD_STOP_INDEX_DISABLE         0xFF000000


class AppleAC97AudioVIA : public IOAC97Controller
{
    OSDeclareDefaultStructors( AppleAC97AudioVIA )

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
        IOBufferMemoryDescriptor * sgdMemory;
        SGDVector *                sgdBasePtr;
        IOPhysicalAddress          sgdPhysAddr;
        UInt32                     sgdBufferSize;
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
    UInt32                         fMaxCodecID;
    UInt16                         fIOBase;
    thread_call_t                  fSetPowerStateThreadCall;
    IOAC97CodecDevice *            fCodecs[ kIOAC97MaxCodecCount ];
    UInt32                         fICHxType;
    IOOptionBits                   fDMASupportMask;
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

#endif /* !__APPLEAC97AUDIOVIA_H */
