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
#include <pexpert/i386/protos.h>
#include "AppleAC97AudioVIA.h"
#include "IOAC97Debug.h"

#define CLASS AppleAC97AudioVIA
#define super IOAC97Controller
OSDefineMetaClassAndStructors( AppleAC97AudioVIA, IOAC97Controller )

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
 * Supported DMA Engines
 */
enum {
    kDMAEnginePCMOut  = 0,
    kDMAEnginePCMIn   = 1,
    kDMAEngineSPDIF   = 2,
    kDMAEngineCount   = 3,
    kDMAEngineInvalid = 0xFF
};

#define kDMAEngineSupportMask      \
        ((1 << kDMAEnginePCMOut) | \
         (1 << kDMAEnginePCMIn)  | \
         (1 << kDMAEngineSPDIF))

#define kDMABufferCount   32
#define kDMABufferBytes   PAGE_SIZE
#define kBytesPerSample   2
#define kMaxCodecNubID    0

static const UInt16 gDMAEngineIOBase[] =
{
    0x00,  /* PCM Out */
    0x60,  /* PCM In  */
    0x30   /* SPDIF   */
};

#define CHECK_DMA_ENGINE_ID(id, err) \
    do { if (((id) > 31) || !((1 << (id)) & kDMAEngineSupportMask)) \
         return err; } while(0)

#ifndef RELEASE
#define RELEASE(x) do { if (x) { (x)->release(); (x) = 0; } } while (0)
#endif

#define DMA_READ8(dma, reg)        inb(dma->ioBase + VIA_DMA_ ## reg)
#define DMA_READ32(dma, reg)       inl(dma->ioBase + VIA_DMA_ ## reg)
#define DMA_WRITE8(dma, reg, val)  outb(dma->ioBase + VIA_DMA_ ## reg, val)
#define DMA_WRITE32(dma, reg, val) outl(dma->ioBase + VIA_DMA_ ## reg, val)

#define REG_READ8(reg)             inb(fIOBase + VIA_REG_ ## reg)
#define REG_READ32(reg)            inl(fIOBase + VIA_REG_ ## reg)
#define REG_WRITE8(reg, val)       outb(fIOBase + VIA_REG_ ## reg, val)
#define REG_WRITE32(reg, val)      outl(fIOBase + VIA_REG_ ## reg, val)

//---------------------------------------------------------------------------

bool CLASS::start( IOService * provider )
{
    bool         providerOpen = false;
    IOItemCount  codecCount;

    DebugLog("%s::%s(%p)\n", getName(), __FUNCTION__, provider);

    if (provider == 0 || super::start(provider) == false)
        return false;

    // Slots 0, 1, and 2 are not available for PCM audio.

    fBusyOutputSlots = kIOAC97Slot_0 | kIOAC97Slot_1 | kIOAC97Slot_2;

    // Allocate DMA state array indexed by engine ID.

    fDMAState = IONew(DMAEngineState, kDMAEngineCount);
    if (!fDMAState) goto fail;
    memset(fDMAState, 0, sizeof(DMAEngineState) * kDMAEngineCount);

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

    if ((fInterruptSource == 0) ||
        (fWorkLoop->addEventSource(fInterruptSource) != kIOReturnSuccess))
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

    setProperty(kIOAC97HardwareNameKey, "VIA 8237");

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
        for (int i = 0; i < kDMAEngineCount; i++)
        {
            DMAEngineState * dma = &fDMAState[i];

            IOAC97Assert( dma->flags == kEngineIdle );

            if (dma->sampleMemory)
            {
                dma->sampleMemory->complete();
                dma->sampleMemory->release();
            }

            if (dma->sgdMemory)
            {
                dma->sgdMemory->complete();
                dma->sgdMemory->release();
            }
        }

        memset(fDMAState, 0, sizeof(DMAEngineState) * kDMAEngineCount);
        IODelete(fDMAState, DMAEngineState, kDMAEngineCount);
        fDMAState = 0;
    }

    for ( int id = 0; id < kIOAC97MaxCodecCount; id++ )
    {
        RELEASE( fCodecs[id] );
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
    RELEASE( fPCI );

    super::free();
}

//---------------------------------------------------------------------------

IOWorkLoop * CLASS::getWorkLoop( void ) const
{ 
    return fWorkLoop;
}

//---------------------------------------------------------------------------

bool CLASS::configureProvider( IOService * provider )
{
    fPCI = OSDynamicCast( IOPCIDevice, provider );
    if (fPCI == 0) return false;

    fPCI->setMemoryEnable( false );
    fPCI->setIOEnable( true );
    fPCI->setBusMasterEnable( true );

    fIOBase = fPCI->configRead32( kIOPCIConfigBaseAddress0 );

    // Sanity check on I/O space indicators.

    if ((fIOBase & 0x01) == 0)
        return false;

    fIOBase &= ~0x01;  // mask out IOSE bit

    // Enable PCI power management. D3 on system sleep, D0 otherwise.

    if (fPCI->hasPCIPowerManagement( kPCIPMCPMESupportFromD3Cold ))
    {
        fPCI->enablePCIPowerManagement( kPCIPMCSPowerStateD3 );
    }

    DebugLog("%s: io = %x irq = %u\n", getName(),
             fIOBase, fPCI->configRead8(kIOPCIConfigInterruptLine));

    return true;
}

#pragma mark -
#pragma mark ••• Codec Management •••
#pragma mark -

//---------------------------------------------------------------------------

IOItemCount CLASS::attachCodecDevices( void )
{
    IOItemCount count = 0;

    // Probe for audio codecs.

    for (IOAC97CodecID id = 0; id <= kMaxCodecNubID; id++)
    {
        IOAC97CodecDevice * codec = createCodecDevice( id );
        if (codec)
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

    if (waitCodecReady(codecID) == false)
    {
        return 0;
    }

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

bool CLASS::waitCodecReady( IOAC97CodecID codecID )
{
    static const UInt8 readyMask[] =
    {
        VIA_ACLINK_STATUS_PRI_CODEC_READY,
        VIA_ACLINK_STATUS_SEC_CODEC_READY
    };

    bool  isReady = false;
    UInt8 status;

    if (codecID > sizeof(readyMask))
        return false;

    for ( int timeout = 0; timeout < 600; )
    {
        status = fPCI->configRead8(VIA_PCI_ACLINK_STATUS);
        if (status & readyMask[codecID])
        {
            isReady = true;
            break;
        }
        IOSleep(50);
        timeout += 50;
    }

    DebugLog("%s::%s codec[%lu] AC97 Interface Status = %02x\n",
             getName(), __FUNCTION__,
             codecID, fPCI->configRead8(VIA_PCI_ACLINK_STATUS));

    return isReady;
}

//---------------------------------------------------------------------------

IOReturn CLASS::codecRead( IOAC97CodecID     codecID,
                           IOAC97CodecOffset offset,
                           IOAC97CodecWord * word )
{
    IOReturn ret;
    UInt32   cmd;
    UInt32   validMask;

    if ((codecID > 1) || (offset >= kCodecRegisterCount))
        return kIOReturnBadArgument;

    cmd = VIA_CODEC_READ |
          ((offset << VIA_CODEC_INDEX_SHIFT) & VIA_CODEC_INDEX_MASK);

    if (codecID == 0)
    {
        validMask = VIA_CODEC_PRI_DATA_VALID;
        cmd |= (VIA_CODEC_ID_PRI | validMask);
    }
    else
    {
        validMask = VIA_CODEC_SEC_DATA_VALID;
        cmd |= (VIA_CODEC_ID_SEC | validMask);
    }

    ret = waitACLinkNotBusy();

    if (ret == kIOReturnSuccess)
    {
        REG_WRITE32(CODEC, cmd);

        ret = kIOReturnTimeout;
        for ( int timeout = 500; timeout > 0; timeout-- )
        {
            UInt32 statusReg = REG_READ32(CODEC);

            if (statusReg & validMask)
            {
                *word = statusReg & VIA_CODEC_DATA_MASK;
                ret = kIOReturnSuccess;
                break;
            }
            IOSleep(1);
        }
    }

    DebugLog("%s::%s(%x, 0x%02x, 0x%04x) status %x\n",
             getName(), __FUNCTION__,
             codecID, offset, *word, ret);

    return ret;
}

//---------------------------------------------------------------------------

IOReturn CLASS::codecWrite( IOAC97CodecID     codecID,
                            IOAC97CodecOffset offset,
                            IOAC97CodecWord   word )
{
    IOReturn ret;
    UInt32   cmd;

    if ((codecID > 1) || (offset >= kCodecRegisterCount))
        return kIOReturnBadArgument;

    cmd = ((offset << VIA_CODEC_INDEX_SHIFT) & VIA_CODEC_INDEX_MASK) |
          VIA_CODEC_WRITE | word;

    if (codecID == 0)
        cmd |= (VIA_CODEC_ID_PRI | VIA_CODEC_PRI_DATA_VALID);
    else
        cmd |= (VIA_CODEC_ID_SEC | VIA_CODEC_SEC_DATA_VALID);

    ret = waitACLinkNotBusy();

    if (ret == kIOReturnSuccess)
    {
        REG_WRITE32(CODEC, cmd);

        // FIXME: wait for write completion?
    }

    DebugLog("%s::%s(%x, 0x%02x, 0x%04x) status %x\n",
             getName(), __FUNCTION__,
             codecID, offset, word, ret);

    return ret;
}

#pragma mark -
#pragma mark ••• AC-Link Control •••
#pragma mark -

//---------------------------------------------------------------------------

IOReturn CLASS::waitACLinkNotBusy( void )
{
    for (int loops = 0; loops < 500; loops++)
    {
        if ((REG_READ32(CODEC) & VIA_CODEC_BUSY) == 0)
            return kIOReturnSuccess;
        IOSleep(1);
    }
    DebugLog("%s: Codec access timeout\n", getName());
    return kIOReturnTimeout;
}

//---------------------------------------------------------------------------

void CLASS::resetACLink( IOOptionBits type )
{
    UInt8 control = VIA_ACLINK_CONTROL_ENABLE | VIA_ACLINK_CONTROL_VRA;

    fPCI->configWrite8( VIA_PCI_ACLINK_CONTROL, control );
    IOSleep(10);  // assert RESET# for 1us min

    control |= VIA_ACLINK_CONTROL_DEASSERT_RESET;
    fPCI->configWrite8( VIA_PCI_ACLINK_CONTROL, control );
    IOSleep(10);

    DebugLog("%s: ACLINK_CONTROL = %02x\n", getName(),
             fPCI->configRead8(VIA_PCI_ACLINK_CONTROL));
}

#pragma mark -
#pragma mark ••• DMA Engine Control •••
#pragma mark -

//---------------------------------------------------------------------------

IOReturn CLASS::startDMAEngine( IOAC97DMAEngineID engine,
                                IOOptionBits      options )
{
    DMAEngineState * dma;
    UInt8            control;
    UInt32           stopIndex;

    DebugLog("%s::%s[%lx]\n", getName(), __FUNCTION__, engine);

    CHECK_DMA_ENGINE_ID(engine, kIOReturnBadArgument);

    dma = &fDMAState[engine];

    if ((dma->flags & kEngineActive) == 0)
    {
        DebugLog("%s: starting inactive DMA engine\n", getName());
        return kIOReturnNotReady;
    }

    if (dma->flags & kEngineRunning)
    {
        return kIOReturnSuccess;
    }

    control = VIA_SGD_CONTROL_START | VIA_SGD_CONTROL_AUTOSTART;

    stopIndex = VIA_SGD_STOP_INDEX_48K
              | VIA_SGD_STOP_INDEX_STEREO
              | VIA_SGD_STOP_INDEX_16BIT
              | VIA_SGD_STOP_INDEX_DISABLE;

    if (dma->flags & kEngineInterrupt)
    {
        dma->interruptReady = true;
        control |= (VIA_SGD_CONTROL_INT_EOL | VIA_SGD_CONTROL_INT_FLAG);
    }

    DMA_WRITE32( dma, SGD_TABLE_PTR,  dma->sgdPhysAddr );
    DMA_WRITE32( dma, SGD_STOP_INDEX, stopIndex);
    DMA_WRITE8(  dma, SGD_TYPE,       0x00 );
    DMA_WRITE8(  dma, SGD_CONTROL,    control );

    dma->flags |= kEngineRunning;

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

void CLASS::stopDMAEngine( IOAC97DMAEngineID engine )
{
    DMAEngineState * dma;
    int              waits;

    DebugLog("%s::%s[%lx]\n", getName(), __FUNCTION__, engine);

    CHECK_DMA_ENGINE_ID(engine, /*void*/);

    dma = &fDMAState[engine];

    if ((dma->flags & kEngineRunning) == 0)
    {
        DebugLog("%s: stop DMA redundant\n", getName());
        return;
    }

    // Stop the DMA engine, and wait for completion.

    DMA_WRITE8( dma, SGD_CONTROL, VIA_SGD_CONTROL_PAUSE     |
                                  VIA_SGD_CONTROL_TERMINATE |
                                  VIA_SGD_CONTROL_RESET );

    IOSleep(10);
    for (waits = 100; waits; waits--)
    {
        if ((DMA_READ8(dma, SGD_STATUS) & VIA_SGD_STATUS_ACTIVE) == 0)
            break;
        IOSleep(1);
    }

    // Clear control and interrupt flags following engine reset.

    DMA_WRITE8( dma, SGD_CONTROL, 0x00 );
    DMA_WRITE8( dma, SGD_STATUS, VIA_SGD_STATUS_EOL | VIA_SGD_STATUS_FLAG );

    dma->interruptReady = false;
    dma->flags &= ~kEngineRunning;
}

//---------------------------------------------------------------------------

IOByteCount CLASS::getDMAEngineHardwarePointer( IOAC97DMAEngineID engine )
{
    UInt32      countReg;
    UInt32      descIndex;
    UInt32      remainBytes;
    IOByteCount position;
    DMAEngineState * dma;

    CHECK_DMA_ENGINE_ID(engine, 0);
    dma = &fDMAState[engine];

    countReg    = DMA_READ32(dma, SGD_CURRENT_COUNT);
    descIndex   = (countReg >> 24) & 0xff;
    remainBytes = (countReg & 0xffffff);

    position = ((descIndex + 1) * dma->sgdBufferSize) - remainBytes;

    DebugLog("%s::%s[%lx] = %lx\n", getName(), __FUNCTION__,
             engine, position);

    return position;
}

#pragma mark -
#pragma mark ••• Audio Configuration •••
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
        DebugLog("%s: no matching DMA engine for type %lx dir %lx\n",
                 getName(), engineType, direction);
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

    switch (engineID)
    {
        case kDMAEnginePCMOut:
            want = 0;
            switch (channels)
            {
                case 2:
                    want = kIOAC97Slot_3_4;
                    break;
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
                map[count++] = kIOAC97Slot_3_4;
            }
            break;

        case kDMAEngineSPDIF:
            if (slotsOK & kIOAC97Slot_10_11)
            {
                map[count++] = kIOAC97Slot_10_11;
            }
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
    if (config->getDMAEngineID() == kDMAEngineSPDIF)
    {
        UInt8 control = fPCI->configRead8(VIA_PCI_SPDIF_CONTROL);
        control &= ~VIA_SPDIF_SLOT_MASK;
        control |= (VIA_SPDIF_SLOT_10_11);
        fPCI->configWrite8(VIA_PCI_SPDIF_CONTROL, control);
        DebugLog("%s: SPDIF Control = %02x\n", control);
    }

    if (config->getDMADataDirection() == kIOAC97DMADataDirectionOutput)
    {
        fBusyOutputSlots |= config->getSlotMap();
    }

    return true;
}

//---------------------------------------------------------------------------

void CLASS::hwDeactivateConfiguration( const IOAC97AudioConfig * config )
{
    if (config->getDMADataDirection() == kIOAC97DMADataDirectionOutput)
    {
        fBusyOutputSlots &= ~(config->getSlotMap());
    }
}

//---------------------------------------------------------------------------

IOReturn CLASS::activateAudioConfiguration( IOAC97AudioConfig *   config,
                                            void *                target,
                                            IOAC97DMAEngineAction action,
                                            void *                param )
{
    DMAEngineState *  dma;
    SGDVector *       vector;
    IOAC97DMAEngineID engine;
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

    // Set engine register base address.

    dma->ioBase = gDMAEngineIOBase[engine] + fIOBase;

    // Configure hardware.

    if (hwActivateConfiguration( config ) != true)
        return kIOReturnIOError;

    // Allocate memory for audio sample buffers.

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
            goto done;
        }
    }

    bzero( dma->sampleMemory->getBytesNoCopy(),
           dma->sampleMemory->getCapacity() );

    // Allocate contiguous memory for buffer descriptors.
    // Allocation size is less than a page.

    if (dma->sgdMemory == 0)
    {
        dma->sgdMemory = IOBufferMemoryDescriptor::withOptions(
                         /* options   */ kIOMemoryPhysicallyContiguous,
                         /* capacity  */ sizeof(SGDVector) * kDMABufferCount,
                         /* alignment */ sizeof(SGDVector) );

        if ( dma->sgdMemory == 0 ||
             dma->sgdMemory->prepare() != kIOReturnSuccess )
        {
            DebugLog("%s: failed to allocate %u bytes for engine %lx\n",
                     getName(), sizeof(SGDVector) * kDMABufferCount, engine);
            if (dma->sgdMemory)
            {
                dma->sgdMemory->release();
                dma->sgdMemory = 0;
            }
            goto done;
        }

        dma->sgdBasePtr = (SGDVector *) dma->sgdMemory->getBytesNoCopy();
        dma->sgdPhysAddr = dma->sgdMemory->getPhysicalSegment(0, &size);
        IOAC97Assert(size >= sizeof(SGDVector) * kDMABufferCount);
        IOAC97Assert(dma->sgdBasePtr  != 0);
        IOAC97Assert(dma->sgdPhysAddr != 0);
        DebugLog("%s: DMA BD V = %p P = %lx\n", getName(),
                 dma->sgdBasePtr, dma->sgdPhysAddr);
    }

    if (dma->sgdBufferSize != kDMABufferBytes)
    {
        // Initialize buffer descriptor list.

        vector = dma->sgdBasePtr;
        for ( UInt32 i = 0; i < kDMABufferCount; i++ )
        {
            IOPhysicalAddress phys;

            phys = dma->sampleMemory->getPhysicalSegment(
                                i * kDMABufferBytes, &size);
            IOAC97Assert(phys != 0);

            OSWriteLittleInt32( &vector->base,  0, phys );
            OSWriteLittleInt32( &vector->count, 0, kDMABufferBytes );
            vector++;
        }

        dma->sgdBufferSize = kDMABufferBytes;
        DebugLog("%s: DMA buffer size = %lu\n", getName(),
                 dma->sgdBufferSize);
    }

    // Request descriptor to trigger interrupts.

    if (target && action)
    {
        UInt32 count;

        fInterruptSource->disable();
        dma->interruptReady  = false;
        dma->interruptTarget = target;
        dma->interruptAction = action;
        dma->interruptParam  = param;
        fInterruptSource->enable();

        vector = &dma->sgdBasePtr[kDMABufferCount-1];
        count  = OSReadLittleInt32(&vector->count, 0);
        count |= kSGD_EOL;
        OSWriteLittleInt32(&vector->count, 0, count);
        dma->flags |= kEngineInterrupt;
    }
    else
    {
        UInt32 count;
        vector = &dma->sgdBasePtr[kDMABufferCount-1];
        count  = OSReadLittleInt32(&vector->count, 0);
        count &= ~kSGD_EOL;
        OSWriteLittleInt32(&vector->count, 0, count);
    }

    dma->flags |= kEngineActive;

    config->setDMABufferMemory( dma->sampleMemory );
    config->setDMABufferCount( kDMABufferCount );
    config->setDMABufferSize(  kDMABufferBytes );

    ret = super::activateAudioConfiguration(config, target, action, param);
    if (ret != kIOReturnSuccess)
    {
        deactivateAudioConfiguration(config);
    }

done:
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

    if (dma->flags & kEngineInterrupt)
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
#pragma mark ••• Interrupt Handling •••
#pragma mark -

//---------------------------------------------------------------------------

bool CLASS::interruptFilter( OSObject * owner, IOFilterInterruptEventSource * )
{
    CLASS * me = (CLASS *) owner;
    DMAEngineState * dma;

    IOAC97Assert(me != 0);

    dma = &me->fDMAState[kDMAEnginePCMOut];
    if (dma->interruptReady && me->serviceDMAEngineInterrupt(dma))
    {
        IOAC97Assert(dma->interruptAction != 0);
        IOAC97Assert(dma->interruptTarget != 0);
        dma->interruptAction(dma->interruptTarget, dma->interruptParam);
        //kprintf("[IRQ-PCM Out] ");
    }

    dma = &me->fDMAState[kDMAEngineSPDIF];
    if (dma->interruptReady && me->serviceDMAEngineInterrupt(dma))
    {
        IOAC97Assert(dma->interruptAction != 0);
        IOAC97Assert(dma->interruptTarget != 0);
        dma->interruptAction(dma->interruptTarget, dma->interruptParam);
        //kprintf("[IRQ-SPDIF Out] ");
    }

    dma = &me->fDMAState[kDMAEnginePCMIn];
    if (dma->interruptReady && me->serviceDMAEngineInterrupt(dma))
    {
        IOAC97Assert(dma->interruptAction != 0);
        IOAC97Assert(dma->interruptTarget != 0);
        dma->interruptAction(dma->interruptTarget, dma->interruptParam);
        //kprintf("[IRQ-PCM In] ");
    }

    return false;  // never signal interrupt source
}

void CLASS::interruptOccurred( OSObject *, IOInterruptEventSource *, int )
{
    return; // should not get called
}

//---------------------------------------------------------------------------

bool CLASS::serviceDMAEngineInterrupt( const DMAEngineState * dma )
{
    bool serviced = false;

    UInt16 status = DMA_READ8( dma, SGD_STATUS );

    if (status & (VIA_SGD_STATUS_EOL | VIA_SGD_STATUS_FLAG))
    {
        DMA_WRITE8( dma, SGD_STATUS,
                    VIA_SGD_STATUS_EOL | VIA_SGD_STATUS_FLAG );
        serviced = true;
    }

    return serviced;
}

#pragma mark -
#pragma mark ••• Power Management •••
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

        // Poll for codec ready signals following reset.

        for (int id = 0; id < kIOAC97MaxCodecCount; id++)
        {
            if (me->fCodecs[id])
                me->waitCodecReady(id);
        }
        me->fACLinkPowerDown = false;
    }

    me->acknowledgeSetPowerState();
    me->release();
}
