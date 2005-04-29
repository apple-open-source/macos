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

#include <IOKit/IOLib.h>
#include <IOKit/IOLib.h>
#include <libkern/OSByteOrder.h>
#include <pexpert/i386/protos.h>
#include "AppleAC97AudioIntelICH.h"
#include "IOAC97Debug.h"

#define CLASS AppleAC97AudioIntelICH
#define super IOAC97Controller
OSDefineMetaClassAndStructors( AppleAC97AudioIntelICH, IOAC97Controller )

enum {
    kPowerStateOff = 0,
    kPowerStateDoze,
    kPowerStateOn,
    kPowerStateCount
};

static IOPMPowerState gPowerStates[ kPowerStateCount ] =
{
    { 1, 0,                 0,           0,           0, 0, 0, 0, 0, 0, 0, 0 },
    { 1, 0,                 kIOPMDoze,   kIOPMDoze,   0, 0, 0, 0, 0, 0, 0, 0 },
    { 1, kIOPMDeviceUsable, IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 }
};

/*
 * Supported DMA Engine IDs
 */
enum {
    /* ICH, ICH2 and ICH3 */
    kDMAEnginePCMIn     = 0,
    kDMAEnginePCMOut    = 1,

    /* ICH4 and ICH5 only, as well as NVIDIA */
    kDMAEngineSPDIF     = 2,
    kDMAEngineCountMax  = 3,
    kDMAEngineInvalid   = 0xFF
};

/*
 * ICH, ICH2 and ICH3
 */
#define kDMAEngineSupportMask_ICH \
        ((1 << kDMAEnginePCMIn) | \
         (1 << kDMAEnginePCMOut))

/*
 * ICH4 and ICH5
 */
#define kDMAEngineSupportMask_ICH4 \
        ((1 << kDMAEnginePCMIn)  | \
         (1 << kDMAEnginePCMOut) | \
         (1 << kDMAEngineSPDIF))

#define kDMABufferCount   32          /* max supported by hardware */
#define kDMABufferBytes   PAGE_SIZE
#define kBytesPerSample   2           /* FIXME: 20-bit audio */

#define CHECK_DMA_ENGINE_ID(e, err) \
    do { if (((e) > 31) || !((1 << (e)) & fDMASupportMask)) return err; } \
    while(0)

#ifndef RELEASE
#define RELEASE(x) do { if (x) { (x)->release(); (x) = 0; } } while (0)
#endif

//---------------------------------------------------------------------------

class AppleAC97AudioIntelHWReg
{
public:
    AppleAC97AudioIntelHWReg() {}
    virtual ~AppleAC97AudioIntelHWReg() {}

    virtual UInt32 getBaseAddress( void ) const = 0;
    virtual UInt32 read32 ( UInt32 offset ) = 0;
    virtual UInt16 read16 ( UInt32 offset ) = 0;
    virtual UInt8  read8  ( UInt32 offset ) = 0;
    virtual void   write32( UInt32 offset, UInt32 data ) = 0;
    virtual void   write16( UInt32 offset, UInt16 data ) = 0;
    virtual void   write8 ( UInt32 offset, UInt8  data ) = 0;
};

class AppleAC97AudioIntelMemoryReg : public AppleAC97AudioIntelHWReg
{
private:
    void * fMemBase;

public:
    AppleAC97AudioIntelMemoryReg( void * base ) : fMemBase(base) {}
    virtual ~AppleAC97AudioIntelMemoryReg() {}

    virtual UInt32 getBaseAddress( void ) const;
    virtual UInt32 read32 ( UInt32 offset );
    virtual UInt16 read16 ( UInt32 offset );
    virtual UInt8  read8  ( UInt32 offset );
    virtual void   write32( UInt32 offset, UInt32 data );
    virtual void   write16( UInt32 offset, UInt16 data );
    virtual void   write8 ( UInt32 offset, UInt8  data );
};

class AppleAC97AudioIntelIOReg : public AppleAC97AudioIntelHWReg
{
private:
    UInt16 fIOBase;

public:
    AppleAC97AudioIntelIOReg( UInt16 base ) : fIOBase(base) {}
    virtual ~AppleAC97AudioIntelIOReg() {}

    virtual UInt32 getBaseAddress( void ) const;
    virtual UInt32 read32 ( UInt32 offset );
    virtual UInt16 read16 ( UInt32 offset );
    virtual UInt8  read8  ( UInt32 offset );
    virtual void   write32( UInt32 offset, UInt32 data );
    virtual void   write16( UInt32 offset, UInt16 data );
    virtual void   write8 ( UInt32 offset, UInt8  data );
};

//---------------------------------------------------------------------------

bool CLASS::start( IOService * provider )
{
    bool         providerOpen = false;
    IOItemCount  codecCount;
    OSNumber *   num;
    char         nameBuf[16];
    IOBufferMemoryDescriptor * buffer;

    DebugLog("%s::%s(%p)\n", getName(), __FUNCTION__, provider);

    if (provider == 0 || super::start(provider) == false)
        return false;

    // The ICH type (e.g. ICH4) comes from our matching dictionary.

    fICHxType = 2;
    num = OSDynamicCast(OSNumber, getProperty("ICH Type"));
    if (num)
    {
        fICHxType = num->unsigned32BitValue();
        DebugLog("%s: ICH%u\n", getName(), fICHxType);
    }
	if (fICHxType == 99) 
	{
		sprintf(nameBuf, "NVIDIA nForce");
	}
	else
	{
		sprintf(nameBuf, "Intel ICH%lu", fICHxType);
	}
    setProperty(kIOAC97HardwareNameKey, nameBuf);

    if (fICHxType >= 4)
        fDMASupportMask = kDMAEngineSupportMask_ICH4;
    else
        fDMASupportMask = kDMAEngineSupportMask_ICH;

    // Slots 0, 1, and 2 are not available for PCM audio.

    fBusyOutputSlots = kIOAC97Slot_0 | kIOAC97Slot_1 | kIOAC97Slot_2;

    // Allocate DMA state array indexed by engine ID.

    fDMAState = IONew(DMAEngineState, kDMAEngineCountMax);
    if (!fDMAState) goto fail;
    memset(fDMAState, 0, sizeof(DMAEngineState) * kDMAEngineCountMax);

    // Attempt to allocate a contiguous DMA buffer for PCM out engine.
    // If this fails, driver will only support 2 & 4 channel modes and
    // reject 6-channel mode due to hardware's alignment restrictions.

    buffer = IOBufferMemoryDescriptor::withOptions(
             /* options   */ kIOMemoryPhysicallyContiguous,
             /* capacity  */ kDMABufferCount * kDMABufferBytes,
             /* alignment */ PAGE_SIZE );

    if (!buffer)
    {
        DebugLog("%s: failed to allocate %u contiguous bytes\n",
                 getName(), kDMABufferCount * kDMABufferBytes);
    }
    else
    {
        if (buffer->prepare() != kIOReturnSuccess)
        {
            buffer->release();
            buffer = 0;
        }
        else
        {
            fDMAState[kDMAEnginePCMOut].sampleMemory = buffer;
            fDMAState[kDMAEnginePCMOut].sampleMemoryIsContiguous = true;
        }
    }

    // Allocate a thread callout to handle power state transitions.

    fSetPowerStateThreadCall = thread_call_allocate(
                               (thread_call_func_t)  handleSetPowerState,
                               (thread_call_param_t) this );

    if (fSetPowerStateThreadCall == 0)
    {
        goto fail;
    }

    // Allocate a work loop dedicated to the AC97 driver stack.

    fWorkLoop = IOWorkLoop::workLoop();
    if (!fWorkLoop)
    {
        goto fail;
    }

    // Open provider (exclusively) before using its services.

    if (provider->open(this) == false)
    {
        goto fail;
    }
    providerOpen = true;

    // Let the subclass process and configure the provider.

    if (configureProvider(provider) == false)
    {
        goto fail;
    }

    // Reset AC-link and all attached codecs.

    resetACLink( kColdReset );

    // Create nubs for each codec discovered.

    codecCount = attachCodecDevices();
    if (codecCount == 0)
    {
        DebugLog("%s: No audio codec detected\n", getName());
        goto fail;
    }

    // Create an interrupt event source and attach to work loop.

    fInterruptSource = 
        IOFilterInterruptEventSource::filterInterruptEventSource(
        this, interruptOccurred, interruptFilter, provider, 0 );

    if (fInterruptSource == 0)
    {
        IOLog("%s: failed to create interrupt event source\n", getName());
        goto fail;
    }
    if (fWorkLoop->addEventSource(fInterruptSource) != kIOReturnSuccess)
    {
        IOLog("%s: failed to attach interrupt event source\n", getName());
        goto fail;
    }

    // Play nice and enable interrupt sources right away just in case we
    // are using a shared interrupt vector.

    fInterruptSource->enable();

    // Close provider following codec probe. Subsequent opens will be
    // driven based on client demand.

    provider->close(this);

    // Attach to power management.

    PMinit();
    registerPowerDriver( this, gPowerStates, kPowerStateCount );
    provider->joinPMtree( this );

    // Publish codecs, and trigger codec driver matching.

    publishCodecDevices();

    return true;

fail:
    if (providerOpen)
        provider->close(this);

    super::stop( provider );

    return false;
}

//---------------------------------------------------------------------------

void CLASS::stop( IOService * provider )
{
    DebugLog("%s::%s\n", getName(), __FUNCTION__);
    PMstop();
    super::stop( provider );
}

//---------------------------------------------------------------------------

void CLASS::free( void )
{
    DebugLog("%s::%s\n", getName(), __FUNCTION__);

    if ( fDMAState )
    {
        for (int i = 0; i < kDMAEngineCountMax; i++)
        {
            DMAEngineState * dma = &fDMAState[i];

            IOAC97Assert( dma->flags == kEngineIdle );

            if (dma->sampleMemory)
            {
                dma->sampleMemory->complete();
                dma->sampleMemory->release();
            }

            if (dma->bdMemory)
            {
                dma->bdMemory->complete();
                dma->bdMemory->release();
            }

            if (dma->hwReg)
            {
                delete dma->hwReg;
                dma->hwReg = 0;
            }
        }

        memset(fDMAState, 0, sizeof(DMAEngineState) * kDMAEngineCountMax);
        IODelete(fDMAState, DMAEngineState, kDMAEngineCountMax);
        fDMAState = 0;
    }

    for ( int id = 0; id < kIOAC97MaxCodecCount; id++ )
    {
        RELEASE( fCodecs[id] );
    }

    if (fBMReg)
    {
        delete fBMReg;
        fBMReg = 0;
    }

    if (fMixerReg)
    {
        delete fMixerReg;
        fMixerReg = 0;
    }

    if ( fSetPowerStateThreadCall )
    {
        thread_call_free( fSetPowerStateThreadCall );
        fSetPowerStateThreadCall = 0;
    }

    if ( fInterruptSource && fWorkLoop )
    {
        fWorkLoop->removeEventSource(fInterruptSource);
    }

    RELEASE( fInterruptSource );
    RELEASE( fWorkLoop );
    RELEASE( fMixerMap );
    RELEASE( fBMMap );

    super::free();
}

//---------------------------------------------------------------------------

IOWorkLoop * CLASS::getWorkLoop( void ) const
{ 
    return fWorkLoop;
}

#pragma mark -
#pragma mark еее Codec Management еее
#pragma mark -

//---------------------------------------------------------------------------

IOItemCount CLASS::attachCodecDevices( void )
{
    IOItemCount count = 0;

    if (waitCodecReady() == false)
    {
        return 0;
    }

    // Probe for audio codecs.

    for (IOAC97CodecID id = 0; id <= fMaxCodecID; id++)
    {
        IOAC97CodecDevice * codec = createCodecDevice( id );
        if ( codec )
        {
            if (codec->attach(this))
            {
                fCodecs[id] = codec;
                count++;
            }
            codec->release();
        }
    }

    return count;
}

//---------------------------------------------------------------------------

void CLASS::publishCodecDevices( void )
{
    for (int id = kIOAC97MaxCodecCount - 1; id >= 0; id--)
        if (fCodecs[id]) fCodecs[id]->registerService();
}

//---------------------------------------------------------------------------

IOAC97CodecDevice * CLASS::createCodecDevice( IOAC97CodecID codecID )
{
    IOAC97CodecDevice * codec = 0;
    IOAC97CodecWord     volume;
    IOReturn            ok;

    // Look for audio codecs by reading the master volume register.

    ok = codecRead(codecID, kCodecMasterVolume, &volume);
    if ((ok == kIOReturnSuccess) && (volume != 0))
    {
        codec = IOAC97CodecDevice::codec( this, codecID );
    }

    DebugLog("%s::%s codec[%lu] = %p\n", getName(), __FUNCTION__,
             codecID, codec);

    return codec;
}

//---------------------------------------------------------------------------

bool CLASS::waitCodecReady( void )
{
    bool   isReady = false;
    UInt32 readyMask;

    // Wait for at least one codec to assert ready.

    readyMask = kPriCodecReady | kSecCodecReady;
    if (fMaxCodecID > 1)
        readyMask |= k3rdCodecReady;

    // Wait for codec ready signals.

    for (int timeout = 0; timeout < 600; timeout += 50)
    {
        if (fBMReg->read32(kGlobalStatus) & readyMask)
        {
            isReady = true;
            break;
        }
        IOSleep(50);
    }

    // Wait a bit more for any additional codec(s) to assert ready.

    if (isReady) IOSleep(250);

    DebugLog("%s::%s mask %lx, Global Status = %lx\n",
             getName(), __FUNCTION__, readyMask,
             fBMReg->read32(kGlobalStatus));

    if (false == isReady)
    {
        DebugLog("%s::%s no codec ready\n", getName(), __FUNCTION__);
    }

    return isReady;
}

//---------------------------------------------------------------------------

IOReturn CLASS::codecRead( IOAC97CodecID     codecID,
                           IOAC97CodecOffset offset,
                           IOAC97CodecWord * word )
{
    IOReturn ret;

    if ((codecID > fMaxCodecID) || (offset >= kCodecRegisterCount))
        return kIOReturnBadArgument;

    ret = acquireACLink();

    if ( ret == kIOReturnSuccess )
    {
        // clear read time-out flag.
        fBMReg->write32( kGlobalStatus, kCodecReadTimeout );

        if ( word )
            *word = mixerRead16(codecID, offset);

        releaseACLink();

        if (fBMReg->read32(kGlobalStatus) & kCodecReadTimeout)
            ret = kIOReturnTimeout;
    }

    fCodecReadCount++;

    return ret;
}

//---------------------------------------------------------------------------

IOReturn CLASS::codecWrite( IOAC97CodecID     codecID,
                            IOAC97CodecOffset offset,
                            IOAC97CodecWord   word )
{
    IOReturn ret;

    if ((codecID > fMaxCodecID) || (offset >= kCodecRegisterCount))
        return kIOReturnBadArgument;

    ret = acquireACLink();

    if ( ret == kIOReturnSuccess )
    {
        mixerWrite16(codecID, offset, word);

        // Hardware will self-clear the codec access semaphore
        // when the (posted) write is complete.
    }

    fCodecWriteCount++;

    return ret;
}

#pragma mark -
#pragma mark еее AC-Link Control еее
#pragma mark -

//---------------------------------------------------------------------------

IOReturn CLASS::acquireACLink( void )
{
    // Wait up to 500ms for the codec access semaphore, shared between
    // the driver and the hardware.

    for (int i = 0; i < 500; i++)
    {
        if ((fBMReg->read8(kCodecAccessSemaphore) & kCodecAccessInProgress) == 0)
            return kIOReturnSuccess;        
        IOSleep(1);
    }
    DebugLog("%s: Codec access semaphore timeout\n", getName());
    return kIOReturnTimeout;
}

//---------------------------------------------------------------------------

void CLASS::releaseACLink( void )
{
    fBMReg->write8( kCodecAccessSemaphore, 0 );
}

//---------------------------------------------------------------------------

void CLASS::resetACLink( IOOptionBits type )
{
    DebugLog("%s::%s gControl %lx gStatus %lx\n", getName(), __FUNCTION__,
             fBMReg->read32(kGlobalControl), fBMReg->read32(kGlobalStatus));

    switch (type)
    {
        case kColdReset:
            fBMReg->write32( kGlobalControl, 0 );
            IOSleep( 50 );

            // De-assert AC_RESET# to complete cold reset.

            fBMReg->write32( kGlobalControl,
                             kGlobalColdResetDisable | k2ChannelMode );
            IOSleep( 20 );
            break;

        case kWarmReset:

            // Warm reset the AC-link. Warm reset bit will self-clear
            // on completion.

            fBMReg->write32( kGlobalControl, kGlobalColdResetDisable |
                                             kGlobalWarmReset        |
                                             k2ChannelMode );

            // Wait for warm reset to complete.

            for ( int wait = 0;
                  fBMReg->read32( kGlobalControl ) & kGlobalWarmReset;
                  wait += 10 )
            {
                if (wait > 5000)
                {
                    DebugLog("%s::%s warm reset timed out %lx %lx\n",
                             getName(), __FUNCTION__,
                             fBMReg->read32(kGlobalControl),
                             fBMReg->read32(kGlobalStatus));
                    break;
                }
                IOSleep( 10 );
            }
            break;

        default:
            IOPanic("Invalid AC-link reset type\n");
    }
}

#pragma mark -
#pragma mark еее DMA Engine Control еее
#pragma mark -

//---------------------------------------------------------------------------

IOReturn CLASS::startDMAEngine( IOAC97DMAEngineID engine,
                                IOOptionBits      options )
{
    DMAEngineState * dma;
    UInt8            lvi;
    UInt8            control;
    AppleAC97AudioIntelHWReg * dmaReg;

    DebugLog("%s::%s[%lx]\n", getName(), __FUNCTION__, engine);

    CHECK_DMA_ENGINE_ID(engine, kIOReturnBadArgument);

    dma = &fDMAState[engine];
    dmaReg = dma->hwReg;

    if ((dma->flags & kEngineActive) == 0)
    {
        DebugLog("%s: start inactive DMA engine\n", getName());
        return kIOReturnNotReady;
    }

    // Update the last valid index to the descriptor before the current
    // descriptor. For instance, if current is 17, then write 16 to the
    // LVI register. This will keep the engine running continuously.

    lvi = dmaReg->read8( kBMCurrentIndex );
    dmaReg->write8( kBMLastValidIndex, (lvi - 1) & (kDMABufferCount - 1) );

    if (dma->flags & kEngineRunning)
    {
        return kIOReturnSuccess;
    }

    control = kRunBusMaster;

    if (dma->flags & kEngineIOC)
    {
        dma->interruptReady = true;
        control |= kInterruptOnCompletionEnable;
    }

    dmaReg->write32( kBMBufferDescBaseAddress, dma->bdPhysAddr );
    dmaReg->write8(  kBMControl, control );

    dma->flags |= kEngineRunning;

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

void CLASS::stopDMAEngine( IOAC97DMAEngineID engine )
{
    DMAEngineState * dma;
    int              tries;
    AppleAC97AudioIntelHWReg * dmaReg;

    DebugLog("%s::%s[%lx]\n", getName(), __FUNCTION__, engine);
    DebugLog("%s: codec reads = %lu, write = %lu\n", getName(),
             fCodecReadCount, fCodecWriteCount);

    CHECK_DMA_ENGINE_ID(engine, /*void*/);

    dma = &fDMAState[engine];
    dmaReg = dma->hwReg;

    if ((dma->flags & kEngineRunning) == 0)
    {
        DebugLog("%s: stop DMA redundant\n", getName());
        return;
    }

    // Stop the DMA engine, and wait for completion.

    dmaReg->write8(kBMControl, 0);
    for (tries = 1000; tries; tries--)
    {
        if (dmaReg->read16(kBMStatus) & kDMAControllerHalted)
            break;
        IOSleep(1);
    }
    if (tries == 0)
    {
        DebugLog("%s: DMA halt timeout\n", getName());
    }

    // Reset the DMA engine, then wait for bit to auto-clear.
    // According to Intel docs, setting the reset-registers
    // bit on a running engine should be avoided.

    dmaReg->write8(kBMControl, kResetRegisters);
    for (tries = 1000; tries; tries--)
    {
        if ((dmaReg->read8(kBMControl) & kResetRegisters) == 0)
            break;
        IOSleep(1);
    }
    if (tries == 0)
    {
        DebugLog("%s: DMA reset timeout\n", getName());
    }

    dma->interruptReady = false;
    dma->flags &= ~kEngineRunning;
}

//---------------------------------------------------------------------------

IOByteCount CLASS::getDMAEngineHardwarePointer( IOAC97DMAEngineID engine )
{
    UInt32      remain;    // number of samples left in current buffer
    UInt8       civ;       // current descriptor index value
    UInt8       civ_last;  // cached version of prior
    IOByteCount position;
    DMAEngineState * dma;
    AppleAC97AudioIntelHWReg * dmaReg;

    CHECK_DMA_ENGINE_ID(engine, 0);
    dma = &fDMAState[engine];
    dmaReg = dma->hwReg;

    // Determine which descriptor within the list of 32 descriptors is
    // currently being processed.

    civ = dmaReg->read8( kBMCurrentIndex );

    do {
        // Get the number of samples left to be processed in the
        // current descriptor.

        civ_last = civ;
        remain = dmaReg->read16( kBMPositionInBuffer );
    }
    while ((civ = dmaReg->read8( kBMCurrentIndex )) != civ_last);

    position = (((civ + 1) * dma->bdBufferSize) - (remain * kBytesPerSample));

    DebugLog("%s::%s[%lx] = %lx\n", getName(), __FUNCTION__,
             engine, position);

    return position;
}

#pragma mark -
#pragma mark еее Audio Configuration еее
#pragma mark -

//---------------------------------------------------------------------------

IOReturn CLASS::prepareAudioConfiguration( IOAC97AudioConfig * config )
{
    bool     ok;
    IOReturn ret;

    DebugLog("%s::%s (%p)\n", getName(), __FUNCTION__, config);

    if (!config) return kIOReturnBadArgument;

    ok = selectDMAEngineForConfiguration( config );
    if (ok) ok = selectSlotMapsForConfiguration( config );

    if (ok)
    {
        ret = super::prepareAudioConfiguration( config );
    }
    else
    {
        ret = kIOReturnUnsupported;
    }

    return ret;
}

//---------------------------------------------------------------------------

bool CLASS::selectDMAEngineForConfiguration( IOAC97AudioConfig * config )
{
    int  direction;
    int  engineType;
    int  engineID = -1;

    DebugLog("%s::%s (%p)\n", getName(), __FUNCTION__, config);

    config->setDMAEngineID( kDMAEngineInvalid );

    engineType = config->getDMAEngineType();
    direction  = config->getDMADataDirection();

    // Map the configuration to an ICH audio DMA engine.

    if (engineType == kIOAC97DMAEngineTypeAudioPCM)
    {
        if (direction == kIOAC97DMADataDirectionOutput)
            engineID = kDMAEnginePCMOut;
        else if (direction == kIOAC97DMADataDirectionInput)
            engineID = kDMAEnginePCMIn;
    }
    else if ((engineType == kIOAC97DMAEngineTypeAudioSPDIF) &&
             (direction  == kIOAC97DMADataDirectionOutput))
    {
        engineID = kDMAEngineSPDIF;
    }

    if (engineID == -1)
    {
        DebugLog("%s: no matching DMA engine for type %lu dir %lu\n",
                 getName(), engineType, direction);
        return false;
    }

    // Check if the hardware supports the selected DMA engine.

    if ((fDMASupportMask & (1 << engineID)) == 0)
    {
        DebugLog("%s: hardware does not support engine ID %lu\n",
                 getName(), engineID);
        return false;
    }

    // Update the configuration.

    config->setDMAEngineID( engineID );
    DebugLog("%s: selected engine ID %lu for config %p\n",
             getName(), engineID, config);

    return true;
}

//---------------------------------------------------------------------------

bool CLASS::selectSlotMapsForConfiguration( IOAC97AudioConfig * config )
{
    IOAC97DMAEngineID  engineID;
    IOItemCount        channels;
    IOOptionBits       want;
    IOOptionBits       map[4];
    IOOptionBits       slotsOK;
    int                count = 0;

    DebugLog("%s::%s (%p)\n", getName(), __FUNCTION__, config);

    channels = config->getAudioChannelCount();
    engineID = config->getDMAEngineID();
    slotsOK  = ~fBusyOutputSlots;

    DebugLog("%s::%s channels = %lu, engineID = %lu, slotsOK = %x\n",
             getName(), __FUNCTION__, channels, engineID, slotsOK);

    switch (engineID)
    {
        case kDMAEnginePCMOut:
            want = 0;
            switch (channels)
            {
                case 2:
                    want = kIOAC97Slot_3_4;
                    break;
                case 4:
                    want = kIOAC97Slot_3_4 | kIOAC97Slot_7_8;
                    break;
                case 6:
                    if (!fDMAState[engineID].sampleMemoryIsContiguous)
                        break;  // no 6-channel mode
                    want = kIOAC97Slot_3_4 |
                           kIOAC97Slot_7_8 |
                           kIOAC97Slot_6_9;
                    break;
                default:
                    DebugLog("%s: bad channels count = %lu\n",
                             getName(), channels);
            }

            if (!want || ((want & slotsOK) != want))
            {
                DebugLog("%s: slot conflict: want %lx, busy %lx\n",
                         getName(), want, fBusyOutputSlots);
                break;
            }

            map[0] = want;
            count = 1;
            break;

        case kDMAEnginePCMIn:
            if (channels == 2)
            {
                map[0] = kIOAC97Slot_3_4;
                count = 1;
            }
            break;

        case kDMAEngineSPDIF:
            // Assign maps in order. Best map first.
            // No simultaneous PCM output and SPDIF transmission.

            if (slotsOK & kIOAC97Slot_10_11)
                map[count++] = kIOAC97Slot_10_11;
            if (slotsOK & kIOAC97Slot_6_9)
                map[count++] = kIOAC97Slot_6_9;
            if (slotsOK & kIOAC97Slot_7_8)
                map[count++] = kIOAC97Slot_7_8;
            break;
    }

    if (count)
    {
        config->setDMAEngineSlotMaps( map, count );
        return true;
    }

    return false;
}

//---------------------------------------------------------------------------

bool CLASS::hwActivateConfiguration( const IOAC97AudioConfig * config )
{
    IOItemCount  channels = config->getAudioChannelCount();
    IOOptionBits map      = config->getSlotMap();
    UInt32       control;

    DebugLog("%s::%s (%p)\n", getName(), __FUNCTION__, config);

    switch (config->getDMAEngineID())
    {
        case kDMAEnginePCMOut:
            control = fBMReg->read32( kGlobalControl );
            control &= ~kChannelModeMask;

            switch (channels)
            {
                case 2:
                    control |= k2ChannelMode;
                    break;
                case 4:
                    control |= k4ChannelMode;
                    break;
                case 6:
                    control |= k6ChannelMode;
                    break;
                default:
                    DebugLog("%s: invalid channel count %lu\n",
                             getName(), channels);
                    return false;
            }

            fBMReg->write32( kGlobalControl, control );
            DebugLog("%s: Global Control %08lx\n",
                     getName(), fBMReg->read32(kGlobalControl));

            fBusyOutputSlots |= map;
            break;

        case kDMAEngineSPDIF:
            control = fBMReg->read32( kGlobalControl );
            control &= ~kSPDIFSlotMask;

            switch (map)
            {
                case kIOAC97Slot_7_8:
                    control |= kSPDIFSlot_7_8;
                    break;
                case kIOAC97Slot_6_9:
                    control |= kSPDIFSlot_6_9;
                    break;
                case kIOAC97Slot_10_11:
                    control |= kSPDIFSlot_10_11;
                    break;
                default:
                    DebugLog("%s: invalid SPDIF slot map %lx\n",
                             getName(), map);
                    return false;
            }

            fBMReg->write32( kGlobalControl, control );
            DebugLog("%s: Global Control %08lx\n",
                     getName(), fBMReg->read32(kGlobalControl));

            fBusyOutputSlots |= map;
            break;
    }

    return true;
}

//---------------------------------------------------------------------------

void CLASS::hwDeactivateConfiguration( const IOAC97AudioConfig * config )
{
    IOOptionBits map = config->getSlotMap();
    UInt32       control;

    DebugLog("%s::%s (%p)\n", getName(), __FUNCTION__, config);

    switch (config->getDMAEngineID())
    {
        case kDMAEnginePCMOut:
            // Switch back to default 2-channel mode.

            control = fBMReg->read32( kGlobalControl );
            control &= ~kChannelModeMask;
            control |= k2ChannelMode;
            fBMReg->write32( kGlobalControl, control );

            DebugLog("%s: Global Control %08lx\n",
                     getName(), fBMReg->read32(kGlobalControl));

            // Release the PCM Out slots.

            fBusyOutputSlots &= ~map;
            break;

        case kDMAEngineSPDIF:
            // Revert to default slot map.

            control = fBMReg->read32( kGlobalControl );
            control &= ~kSPDIFSlotMask;
            fBMReg->write32( kGlobalControl, control );

            DebugLog("%s: Global Control %08lx\n",
                     getName(), fBMReg->read32(kGlobalControl));

            // Release the SPDIF slots.

            fBusyOutputSlots &= ~map;
            break;
    }
}

//---------------------------------------------------------------------------

IOReturn CLASS::activateAudioConfiguration( IOAC97AudioConfig *   config,
                                            void *                target,
                                            IOAC97DMAEngineAction action,
                                            void *                param )
{
    DMAEngineState *  dma;
    BDVector *        vector;
    IOAC97DMAEngineID engine;
    UInt32            bufferBytes;
    UInt32            size;
    IOReturn          ret = kIOReturnNoMemory;

    DebugLog("%s::%s (%p)\n", getName(), __FUNCTION__, config);

    if (!config) return kIOReturnBadArgument;
    engine = config->getDMAEngineID();

    // Check DMA engine is supported.

    CHECK_DMA_ENGINE_ID(engine, kIOReturnBadArgument);

    dma = &fDMAState[engine];

    // DMA engine must not be busy.

    if (dma->flags != kEngineIdle)
        return kIOReturnBusy;

    // Configure hardware.

    if (hwActivateConfiguration( config ) != true)
        return kIOReturnIOError;

    // Allocate non-contiguous memory for DMA buffers.

    if (dma->sampleMemory == 0)
    {
        dma->sampleMemory = IOBufferMemoryDescriptor::withOptions(
                            /* options   */ 0,
                            /* capacity  */ kDMABufferCount * kDMABufferBytes,
                            /* alignment */ PAGE_SIZE );

        if ( dma->sampleMemory == 0 ||
             dma->sampleMemory->prepare() != kIOReturnSuccess )
        {
            DebugLog("%s: failed to allocate %u bytes for engine %lx\n",
                     getName(), kDMABufferCount * kDMABufferBytes, engine);
            if (dma->sampleMemory)
            {
                dma->sampleMemory->release();
                dma->sampleMemory = 0;
            }
            goto fail;
        }
    }

    bzero( dma->sampleMemory->getBytesNoCopy(),
           dma->sampleMemory->getCapacity() );

    // Allocate contiguous memory for buffer descriptors.
    // Allocation size is less than a page.

    if (dma->bdMemory == 0)
    {
        dma->bdMemory = IOBufferMemoryDescriptor::withOptions(
                        /* options   */ kIOMemoryPhysicallyContiguous,
                        /* capacity  */ sizeof(BDVector) * kDMABufferCount,
                        /* alignment */ sizeof(BDVector) );

        if ( dma->bdMemory == 0 ||
             dma->bdMemory->prepare() != kIOReturnSuccess )
        {
            DebugLog("%s: failed to allocate %u bytes for engine %lx\n",
                     getName(), sizeof(BDVector) * kDMABufferCount, engine);
            if (dma->bdMemory)
            {
                dma->bdMemory->release();
                dma->bdMemory = 0;
            }
            goto fail;
        }

        dma->bdBasePtr = (BDVector *) dma->bdMemory->getBytesNoCopy();
        dma->bdPhysAddr = dma->bdMemory->getPhysicalSegment(0, &size);
        IOAC97Assert(size >= sizeof(BDVector) * kDMABufferCount);
        IOAC97Assert(dma->bdBasePtr  != 0);
        IOAC97Assert(dma->bdPhysAddr != 0);
        DebugLog("%s: DMA BD V = %p P = %lx\n", getName(),
                 dma->bdBasePtr, dma->bdPhysAddr);
    }

    // Determine the required size of each data buffer in bytes.

    if (config->getDMAEngineID() == kDMAEnginePCMOut)
    {
        int granularity = config->getAudioChannelCount() * kBytesPerSample;

        bufferBytes = IOTrunc(kDMABufferBytes, granularity);
        if (!dma->sampleMemoryIsContiguous &&
            (bufferBytes & (PAGE_SIZE - 1)))
        {
            DebugLog("%s: Cannot use %u bytes buffer with %lu channels\n",
                     getName(), bufferBytes, config->getAudioChannelCount());
            goto fail;
        }
    }
    else
    {
        bufferBytes = kDMABufferBytes;
    }

    if (dma->bdBufferSize != bufferBytes)
    {
        // Initialize buffer descriptor list.

        vector = dma->bdBasePtr;
        for ( UInt32 i = 0; i < kDMABufferCount; i++ )
        {
            IOPhysicalAddress phys;

            phys = dma->sampleMemory->getPhysicalSegment(
                                i * bufferBytes, &size);
            IOAC97Assert(phys != 0);

            OSWriteLittleInt32( &vector->pointer, 0, phys );
            OSWriteLittleInt16( &vector->samples, 0,
                                bufferBytes / kBytesPerSample );
            OSWriteLittleInt16( &vector->command, 0, 0 );
            vector++;
        }

        dma->bdBufferSize = bufferBytes;
        DebugLog("%s: DMA buffer size = %lu\n", getName(),
                 dma->bdBufferSize);
    }

    // Arm IOC interrupts.

    if (target && action)
    {
        fInterruptSource->disable();
        dma->interruptReady  = false;
        dma->interruptTarget = target;
        dma->interruptAction = action;
        dma->interruptParam  = param;
        fInterruptSource->enable();

        vector = &dma->bdBasePtr[kDMABufferCount-1];
        OSWriteLittleInt16(&vector->command, 0, kInterruptOnCompletion);
        dma->flags |= kEngineIOC;
    }
    else
    {
        vector = &dma->bdBasePtr[kDMABufferCount-1];
        OSWriteLittleInt16( &vector->command, 0, 0 );
    }

    dma->flags |= kEngineActive;

    config->setDMABufferMemory( dma->sampleMemory );
    config->setDMABufferCount( kDMABufferCount );
    config->setDMABufferSize( bufferBytes );

    ret = super::activateAudioConfiguration(config, target, action, param);
    if (ret != kIOReturnSuccess)
    {
        deactivateAudioConfiguration(config);
    }

fail:
    return ret;
}

//---------------------------------------------------------------------------

void CLASS::deactivateAudioConfiguration( IOAC97AudioConfig * config )
{
    DMAEngineState *  dma;
    IOAC97DMAEngineID engine;

    DebugLog("%s::%s (%p)\n", getName(), __FUNCTION__, config);

    if (!config) return;
    engine = config->getDMAEngineID();

    CHECK_DMA_ENGINE_ID(engine, /*void*/);

    dma = &fDMAState[engine];

    if ((dma->flags & kEngineActive) != kEngineActive)
    {
        DebugLog("%s: redundant deactivation\n", getName());
        return;
    }

    if (dma->flags & kEngineRunning)
    {
        DebugLog("%s: deactivation while engine is running\n", getName());
        stopDMAEngine(engine);
    }

    if (dma->flags & kEngineIOC)
    {
        fInterruptSource->disable();
        dma->interruptReady  = false;
        dma->interruptTarget = 0;
        dma->interruptAction = 0;
        dma->interruptParam  = 0;
        fInterruptSource->enable();
    }

    hwDeactivateConfiguration( config );

    dma->flags = kEngineIdle;

    super::deactivateAudioConfiguration(config );
}

#pragma mark -
#pragma mark еее Low-Level Hardware Access еее
#pragma mark -

//---------------------------------------------------------------------------

bool CLASS::configureProvider( IOService * provider )
{
    static UInt8 DMAEngineBase[] =
    {
        0x00,  /* PCM In  */
        0x10,  /* PCM Out */
        0x60   /* SPDIF   */
    };

    void *  memBase;
    UInt16  ioBase;
	OSNumber *num;
    bool    error = false;

    IOPCIDevice * pci = OSDynamicCast( IOPCIDevice, provider );
    if (pci == 0)
        goto fail;

	// Check to see if we're an NIVIDA chip (ICH Type = 99)
	// NVIDIA has a SPDIF interrupt at 0x70, not 0x60
    num = OSDynamicCast(OSNumber, getProperty("ICH Type"));
    if (num)
    {
        UInt32 ich_type = num->unsigned32BitValue();
		if (ich_type == 99)
			DMAEngineBase[2] = 0x70;
	}
	
    // Fetch the Mixer and Bus-Master base address.
    // Newer devices can map hardware registers to either I/O or Memory
    // space, but I/O aperture is smaller and some hardware features are
    // not accessible through I/O space.

    fMixerMap = pci->mapDeviceMemoryWithRegister(kIOPCIConfigBaseAddress2);
    if (fMixerMap)
    {
        memBase = (void *) fMixerMap->getVirtualAddress();
        IOAC97Assert(memBase != NULL);
        fMaxCodecID = 2;
        fMixerReg = new AppleAC97AudioIntelMemoryReg(memBase);
    }
    else if ((ioBase = pci->configRead32(kIOPCIConfigBaseAddress0) & ~1))
    {
        fMaxCodecID = 1;
        fMixerReg = new AppleAC97AudioIntelIOReg(ioBase);
    }

    fBMMap = pci->mapDeviceMemoryWithRegister(kIOPCIConfigBaseAddress3);
    if (fBMMap)
    {
        memBase = (void *) fBMMap->getVirtualAddress();
        IOAC97Assert(memBase != NULL);
        fBMReg = new AppleAC97AudioIntelMemoryReg(memBase);
        for (int i = 0; i < kDMAEngineCountMax; i++)
        {
            void * dmaBase = (UInt8 *)memBase + DMAEngineBase[i];
            fDMAState[i].hwReg =
            new AppleAC97AudioIntelMemoryReg(dmaBase);
            if (!fDMAState[i].hwReg)
            {
                error = true;
                break;
            }
        }
    }
    else if ((ioBase = pci->configRead32(kIOPCIConfigBaseAddress1) & ~1))
    {
        fDMASupportMask &= ~(1 << kDMAEngineSPDIF);
        fBMReg = new AppleAC97AudioIntelIOReg(ioBase);
        for (int i = 0; i < kDMAEngineSPDIF; i++)
        {
            fDMAState[i].hwReg =
            new AppleAC97AudioIntelIOReg(ioBase + DMAEngineBase[i]);
            if (!fDMAState[i].hwReg)
            {
                error = true;
                break;
            }
        }
    }

    if (!fMixerReg || !fBMReg || error) goto fail;

    pci->setMemoryEnable( true );
    pci->setIOEnable( true );
    pci->setBusMasterEnable( true );

    // Enable PCI power management. D3 on system sleep, D0 otherwise.

    if (pci->hasPCIPowerManagement( kPCIPMCPMESupportFromD3Cold ))
    {
        pci->enablePCIPowerManagement( kPCIPMCSPowerStateD3 );
    }

    DebugLog("%s mixerBase = 0x%lx bmBase = 0x%lx irq = %u\n", getName(),
             fMixerReg->getBaseAddress(), fBMReg->getBaseAddress(),
             pci->configRead8( kIOPCIConfigInterruptLine ));

    return true;

fail:
    return false;
}

//---------------------------------------------------------------------------

UInt32 AppleAC97AudioIntelMemoryReg::getBaseAddress( void ) const
{
    return (UInt32) fMemBase;
}

UInt32 AppleAC97AudioIntelMemoryReg::read32( UInt32 offset )
{
    return OSReadLittleInt32(fMemBase, offset);
}

UInt16 AppleAC97AudioIntelMemoryReg::read16( UInt32 offset )
{
    return OSReadLittleInt16(fMemBase, offset);
}

UInt8 AppleAC97AudioIntelMemoryReg::read8( UInt32 offset )
{
    return ((UInt8 *)fMemBase)[offset];
}

void AppleAC97AudioIntelMemoryReg::write32( UInt32 offset, UInt32 data )
{
    OSWriteLittleInt32(fMemBase, offset, data);
}

void AppleAC97AudioIntelMemoryReg::write16( UInt32 offset, UInt16 data )
{
    OSWriteLittleInt16(fMemBase, offset, data);
}

void AppleAC97AudioIntelMemoryReg::write8( UInt32 offset, UInt8 data )
{
    ((UInt8 *)fMemBase)[offset] = data;
}

//---------------------------------------------------------------------------

UInt32 AppleAC97AudioIntelIOReg::getBaseAddress( void ) const
{
    return fIOBase;
}

UInt32 AppleAC97AudioIntelIOReg::read32( UInt32 offset )
{
    return inl(fIOBase + offset);
}

UInt16 AppleAC97AudioIntelIOReg::read16( UInt32 offset )
{
    return inw(fIOBase + offset);
}

UInt8 AppleAC97AudioIntelIOReg::read8( UInt32 offset )
{
    return inb(fIOBase + offset);
}

void AppleAC97AudioIntelIOReg::write32( UInt32 offset, UInt32 data )
{
    outl(fIOBase + offset, data);
}

void AppleAC97AudioIntelIOReg::write16( UInt32 offset, UInt16 data )
{
    outw(fIOBase + offset, data);
}

void AppleAC97AudioIntelIOReg::write8( UInt32 offset, UInt8 data )
{
    outb(fIOBase + offset, data);
}

//---------------------------------------------------------------------------

UInt16 CLASS::mixerRead16( IOAC97CodecID     codec,
                           IOAC97CodecOffset offset )
{
    return fMixerReg->read16(offset + (codec * 0x80));
}

void CLASS::mixerWrite16( IOAC97CodecID     codec,
                          IOAC97CodecOffset offset,
                          IOAC97CodecWord   word )
{
    return fMixerReg->write16(offset + (codec * 0x80), word);
}

#pragma mark -
#pragma mark еее Interrupt Handling еее
#pragma mark -

//---------------------------------------------------------------------------

bool CLASS::interruptFilter( OSObject * owner, IOFilterInterruptEventSource * )
{
    CLASS * me = (CLASS *) owner;
    DMAEngineState * dma;

    IOAC97Assert(me != 0);

    // Scan for IOC interrupts.

    dma = &me->fDMAState[kDMAEnginePCMOut];
    if (dma->interruptReady && me->serviceDMAEngineInterrupt(dma))
    {
        IOAC97Assert(dma->interruptAction != 0);
        IOAC97Assert(dma->interruptTarget != 0);
        dma->interruptAction(dma->interruptTarget, dma->interruptParam);
    }

    dma = &me->fDMAState[kDMAEngineSPDIF];
    if (dma->interruptReady && me->serviceDMAEngineInterrupt(dma))
    {
        IOAC97Assert(dma->interruptAction != 0);
        IOAC97Assert(dma->interruptTarget != 0);
        dma->interruptAction(dma->interruptTarget, dma->interruptParam);
    }

    dma = &me->fDMAState[kDMAEnginePCMIn];
    if (dma->interruptReady && me->serviceDMAEngineInterrupt(dma))
    {
        IOAC97Assert(dma->interruptAction != 0);
        IOAC97Assert(dma->interruptTarget != 0);
        dma->interruptAction(dma->interruptTarget, dma->interruptParam);
    }

    return false;  // never signal interrupt source
}

void CLASS::interruptOccurred( OSObject *, IOInterruptEventSource *, int )
{
    // Thread dispatch - this should never get called
    return;
}

//---------------------------------------------------------------------------

bool CLASS::serviceDMAEngineInterrupt( const DMAEngineState * dma )
{
    AppleAC97AudioIntelHWReg * dmaReg = dma->hwReg;

    bool serviced = false;

    UInt16 status = dmaReg->read16( kBMStatus );

    if (status & kBufferCompletionInterrupt)
    {
        serviced = true;    
        dmaReg->write16( kBMStatus, kBufferCompletionInterrupt );
    }

    return serviced;
}

#pragma mark -
#pragma mark еее Power Management еее
#pragma mark -

//---------------------------------------------------------------------------

IOReturn CLASS::setPowerState( unsigned long powerState,
                               IOService *   policyMaker)
{
    DebugLog("%s::%s (%lu, %p)\n", getName(), __FUNCTION__,
             powerState, policyMaker);

    IOAC97Assert(powerState < kPowerStateCount);
    IOAC97Assert(fSetPowerStateThreadCall != 0);

    retain();
    if ( TRUE == thread_call_enter1( fSetPowerStateThreadCall,
                                     (thread_call_param_t) powerState ) )
    {
        release();  // pending entry replaced, no net retain count change
    }

    // Claim completion under 10 seconds (in microsecond units).

    return (10 * 1000 * 1000);
}

//---------------------------------------------------------------------------

void CLASS::handleSetPowerState( thread_call_param_t param0,
                                 thread_call_param_t param1 )
{
    CLASS *       me = (CLASS *) param0;
    unsigned long powerState = (unsigned long) param1;

    DebugLog("%s::%s state = %lu\n",
             me->getName(), __FUNCTION__, powerState);

    if ((gPowerStates[powerState].capabilityFlags & kIOPMDeviceUsable) == 0)
    {
        me->fACLinkPowerDown = true;
    }
    else if (me->fACLinkPowerDown)
    {
        // Reset AC-link and restart BIT_CLK.

        me->resetACLink( kColdReset );

        // Poll for codec ready signals following AC-link reset.

        me->waitCodecReady();
        me->fACLinkPowerDown = false;
    }

    me->acknowledgeSetPowerState();
    me->release();
}
