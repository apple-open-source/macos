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

#ifndef __APPLEAC97AUDIOINTELICH_H
#define __APPLEAC97AUDIOINTELICH_H

#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOFilterInterruptEventSource.h>
#include "IOAC97Controller.h"
#include "IOAC97CodecDevice.h"

class AppleAC97AudioIntelHWReg;

#define FIELD(bit, mask)  ((mask) << (bit))
#define BIT_RO(bit)       (1 << (bit))  /* read-only  */
#define BIT_RW(bit)       (1 << (bit))  /* read-write */
#define BIT_RC(bit)       (1 << (bit))  /* read-write, write 1 clear */
#define BIT_RS(bit)       (1 << (bit))  /* read-write, self clear */

/*
 * Intel AC97 controller Bus Master registers
 * in NABMBAR I/O aperture  ( 64 byte range)
 * or MBBAR memory aperture (256 byte range)
 */
enum {
    kBMBufferDescBaseAddress = 0x00,
    kBMCurrentIndex          = 0x04,
    kBMLastValidIndex        = 0x05,
    kBMStatus                = 0x06,
    kBMPositionInBuffer      = 0x08,
    kBMPrefetchedIndex       = 0x0a,
    kBMControl               = 0x0b,
    kGlobalControl           = 0x2c,
    kGlobalStatus            = 0x30,
    kCodecAccessSemaphore    = 0x34,
    kSDataInMap              = 0x80,
};

/*
 * x_SR - Status Register
 *
 * Default: 0x0001
 * Size:    16 bits (Word access only)
 */
enum {
    kFIFOError                      = BIT_RC(4),
    kBufferCompletionInterrupt      = BIT_RC(3),
    kLastValidBufferInterrupt       = BIT_RC(2),
    kCurrentEqualsLastValid         = BIT_RO(1),
    kDMAControllerHalted            = BIT_RO(0)
};

/*
 * x_CR - Control Register
 *
 * Default: 0x00
 * Size:    8 bits
 */
enum {
    kInterruptOnCompletionEnable    = BIT_RW(4),
    kFIFOErrorInterruptEnable       = BIT_RW(3),
    kLastValidBufferInterruptEnable = BIT_RW(2),
    kResetRegisters                 = BIT_RS(1),
    kRunBusMaster                   = BIT_RW(0)
};

/* GLOB_CNT - Global Control Register
 *
 * Default: 0x00000000
 * Size:    32 bits (DWord access only)
 */
enum {
    kSPDIFSlot_7_8                  = FIELD(30, 1),
    kSPDIFSlot_6_9                  = FIELD(30, 2),
    kSPDIFSlot_10_11                = FIELD(30, 3),
    kSPDIFSlotMask                  = FIELD(30, 3),
    kPCMOutMode16Bit                = FIELD(22, 0),
    kPCMOutMode20Bit                = FIELD(22, 1),
    kPCMOutModeMask                 = FIELD(22, 3),
    k2ChannelMode                   = FIELD(20, 0),
    k4ChannelMode                   = FIELD(20, 1),
    k6ChannelMode                   = FIELD(20, 2),
    kChannelModeMask                = FIELD(20, 3),
    kSecResumeInterruptEnable       = BIT_RW(5),
    kPriResumeInterruptEnable       = BIT_RW(4),
    kACLinkShutOff                  = BIT_RW(3),
    kGlobalWarmReset                = BIT_RS(2),
    kGlobalColdResetDisable         = BIT_RW(1),
    kGPIInterruptEnable             = BIT_RW(0)
};

/* GLOB_STA - Global Status Register
 *
 * Default: 0x00300000
 * Size:    32 bits (DWord access only)
 */
enum {
    k3rdResumeInterrupt             = BIT_RC(29),
    k3rdCodecReady                  = BIT_RO(28),
    k6ChannelCapable                = BIT_RO(21),
    k4ChannelCapable                = BIT_RO(20),
    kModemPowerDownFlag             = BIT_RW(17),
    kAudioPowerDownFlag             = BIT_RW(16),
    kCodecReadTimeout               = BIT_RC(15),
    kSlot12Bit3                     = BIT_RO(14),
    kSlot12Bit2                     = BIT_RO(13),
    kSlot12Bit1                     = BIT_RO(12),
    kSecResumeInterrupt             = BIT_RC(11),
    kPriResumeInterrupt             = BIT_RC(10),
    kSecCodecReady                  = BIT_RO(9),
    kPriCodecReady                  = BIT_RO(8),
    kMicInInterrupt                 = BIT_RO(7),
    kPCMOutInterrupt                = BIT_RO(6),
    kPCMInInterrupt                 = BIT_RO(5),
    kModemOutInterrupt              = BIT_RO(2),
    kModemInInterrupt               = BIT_RO(1),
    kGPIInterrupt                   = BIT_RC(0)
};

/* CAS - Codec Access Semaphore Register
 *
 * Default: 0x00
 * Size:    8 bits
 */
enum {
    kCodecAccessInProgress = BIT_RS(0)
};

/* SDM - SDATA_IN Map Register (ICH4 and newer)
 *
 * Default: 0x00
 * Size:    8 bits
 */
enum {
    kSteerEnable = BIT_RW(3)
};

/*
 * DMA Engine Buffer Descriptor
 */
struct BDVector {
    IOPhysicalAddress  pointer;  /* DWORD aligned physical address */
    UInt16             samples;  /* buffer size in samples units */
    UInt16             command;  /* command flags */
};

/*
 * Buffer Descriptor Command Flags
 */
enum {
    kInterruptOnCompletion = 0x8000,
    kBufferUnderrunPolicy  = 0x4000,
};


class AppleAC97AudioIntelICH : public IOAC97Controller
{
    OSDeclareDefaultStructors( AppleAC97AudioIntelICH )

protected:
    enum {
        kEngineIdle    = 0x00,
        kEngineActive  = 0x01,
        kEngineRunning = 0x02,
        kEngineIOC     = 0x80
    };

    struct DMAEngineState
    {
        IOOptionBits               flags;
        AppleAC97AudioIntelHWReg * hwReg;
        IOBufferMemoryDescriptor * sampleMemory;
        bool                       sampleMemoryIsContiguous;
        IOBufferMemoryDescriptor * bdMemory;
        BDVector *                 bdBasePtr;
        IOPhysicalAddress          bdPhysAddr;
        UInt32                     bdBufferSize;
        bool                       interruptReady;
        void *                     interruptTarget;
        IOAC97DMAEngineAction      interruptAction;
        void *                     interruptParam;
    };

    IOWorkLoop *                   fWorkLoop;
    IOFilterInterruptEventSource * fInterruptSource;
    bool                           fACLinkPowerDown;
    DMAEngineState *               fDMAState;
    UInt32                         fMaxCodecID;
    IOMemoryMap *                  fMixerMap;   
    AppleAC97AudioIntelHWReg *     fMixerReg;
    IOMemoryMap *                  fBMMap;
    AppleAC97AudioIntelHWReg *     fBMReg;
    thread_call_t                  fSetPowerStateThreadCall;
    IOAC97CodecDevice *            fCodecs[ kIOAC97MaxCodecCount ];
    UInt32                         fICHxType;
    IOOptionBits                   fDMASupportMask;
    UInt32                         fBusyOutputSlots;
    UInt32                         fCodecReadCount;
    UInt32                         fCodecWriteCount;

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
    
    virtual bool         waitCodecReady( void );

    virtual IOReturn     acquireACLink( void );

    virtual void         releaseACLink( void );

    enum {
        kColdReset, kWarmReset
    };

    virtual void         resetACLink( IOOptionBits type );

    virtual bool         configureProvider( IOService * provider );

    virtual UInt16       mixerRead16(  IOAC97CodecID     codec,
                                       IOAC97CodecOffset offset );

    virtual void         mixerWrite16( IOAC97CodecID     codec,
                                       IOAC97CodecOffset offset,
                                       IOAC97CodecWord   word );

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

#endif /* !__APPLEAC97AUDIOINTELICH_H */
