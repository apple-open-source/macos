//
//  PCIDriverKitPEX8733.cpp
//  PCIDriverKitPEX8733
//
//  Created by Kevin Strasberg on 10/29/19.
//
#include <stdio.h>
#include <os/log.h>
#include <DriverKit/IOLib.h>
#include <DriverKit/DriverKit.h>
#include <PCIDriverKit/PCIDriverKit.h>

#include "PCIDriverKitPEX8733.h"
#include "PEX8733Definitions.h"
#define debugLog(fmt, args...)  os_log(OS_LOG_DEFAULT, "PCIDriverKitPEX8733::%s:  " fmt,  __FUNCTION__,##args)

struct PCIDriverKitPEX8733_IVars
{
    IOPCIDevice*               pciDevice;
    IOInterruptDispatchSource* interruptSource;
    IODispatchQueue*           defaultQueue;
    uint64_t                   vendorCapabilityOffset;
    PEX8733TestMode            currentTest;
    IOBufferMemoryDescriptor*  blockTransferBuffer;
	IODMACommand*              blockTransferDMA;
    IOBufferMemoryDescriptor** descriptorTransferBuffers;
    IODMACommand**             descriptorTransferDMAs;
    IOBufferMemoryDescriptor*  ringBuffer;
	IODMACommand*              ringBufferDMA;
};

bool
PCIDriverKitPEX8733::init()
{
    if(super::init() != true)
    {
        return false;
    }

    ivars = IONewZero(PCIDriverKitPEX8733_IVars, 1);

    if(ivars == NULL)
    {
        return false;
    }

    return true;
}

void
PCIDriverKitPEX8733::free()
{
    IOSafeDeleteNULL(ivars, PCIDriverKitPEX8733_IVars, 1);
    super::free();
}

kern_return_t
IMPL(PCIDriverKitPEX8733, Start)
{
    kern_return_t result = Start(provider, SUPERDISPATCH);

    if(result != kIOReturnSuccess)
    {
        return result;
    }

    ivars->pciDevice = OSDynamicCast(IOPCIDevice, provider);
    if(ivars->pciDevice == NULL)
    {
        Stop(provider);
        return kIOReturnNoDevice;
    }
    ivars->pciDevice->retain();

    if(ivars->pciDevice->Open(this, 0) != kIOReturnSuccess)
    {
        Stop(provider);
        return kIOReturnNoDevice;
    }

    uint8_t functionNumber = 0;
    if(ivars->pciDevice->GetBusDeviceFunction(NULL, NULL, &functionNumber) != kIOReturnSuccess)
    {
        debugLog("unable to get B:D:F");
        Stop(provider);
        return false;
    }

    if(functionNumber != 1)
    {
        debugLog("skipping function %u", functionNumber);
        Stop(provider);
        return false;
    }



    debugLog("Kevin enabling bus lead and memory space");
    uint16_t command;
    ivars->pciDevice->ConfigurationRead16(kIOPCIConfigurationOffsetCommand, &command);
    ivars->pciDevice->ConfigurationWrite16(kIOPCIConfigurationOffsetCommand,
                                           command | kIOPCICommandBusLead | kIOPCICommandMemorySpace);

    ivars->pciDevice->FindPCICapability(kPEX8733DMAChannelCapabilityID, 0, &(ivars->vendorCapabilityOffset));

    // for now only allow 1 channel
    ivars->pciDevice->ConfigurationWrite32(ivars->vendorCapabilityOffset + kPEX8733DMAChannelGlobalControlOffset,
                                           kPEX8733DMAChannelGlobalControlModeOne);

    if(CopyDispatchQueue("Default", &(ivars->defaultQueue)) != kIOReturnSuccess)
    {
        debugLog("failed to copy queue");
        Stop(provider);
        return false;
    }

    if(IOInterruptDispatchSource::Create(ivars->pciDevice, 1, ivars->defaultQueue, &(ivars->interruptSource)) != kIOReturnSuccess)
    {
        debugLog("failed to create interrupt dispatch source");
        Stop(provider);
        return false;
    }
    OSAction* action;
    if(CreateActionInterruptOccurred(sizeof(void*), &action) != kIOReturnSuccess)
    {
        debugLog("failed to create interrupt action");
        Stop(provider);
        return false;
    }

    if(ivars->interruptSource->SetHandler(action) != kIOReturnSuccess)
    {
        debugLog("failed to set interrupt handler");
        Stop(provider);
        return false;
    }

    ivars->interruptSource->SetEnable(true);


    // temporary hack to ensure the bridge bus leading is enabled before issue'ing DMA's
    IOSleep(1000);
    dmaBlockTransferTest();
//    dmaDescriptorTransferTest();
    return result;
}

kern_return_t
IMPL(PCIDriverKitPEX8733, Stop)
{
    debugLog("disabling bus lead and memory space");
    if(ivars->pciDevice != NULL)
    {
        uint16_t command;
        ivars->pciDevice->ConfigurationRead16(kIOPCIConfigurationOffsetCommand, &command);
        ivars->pciDevice->ConfigurationWrite16(kIOPCIConfigurationOffsetCommand,
                                               command & ~(kIOPCICommandBusLead | kIOPCICommandMemorySpace));

    }
    OSSafeReleaseNULL(ivars->pciDevice);
    return Stop(provider, SUPERDISPATCH);
}

void
IMPL(PCIDriverKitPEX8733, InterruptOccurred)
{
    uint32_t interruptControlStatus = 0;

    // Wait for transfer to complete
    ivars->pciDevice->ConfigurationRead32(ivars->vendorCapabilityOffset + kPEX8733DMAChannelInterruptControlStatusRegisterOffset,
                                          &interruptControlStatus);
    debugLog("interrupt control status 0x%x\n", interruptControlStatus);


    if((interruptControlStatus & (kPEX8733DMAChannelInterruptControlStatusDescriptorDoneInterruptStatus
                                  | kPEX8733DMAChannelInterruptControlStatusErrorInterruptStatus
                                  | kPEX8733DMAChannelInterruptControlStatusInvalidDescriptorInterruptStatus
                                  | kPEX8733DMAChannelInterruptControlStatusGracefulPauseDoneInterruptStatus
                                  | kPEX8733DMAChannelInterruptControlStatusImmediatePauseDoneInterruptStatus)) != 0)
    {
        switch(ivars->currentTest)
        {
            case kPEX8733TestModeBlock:
            {
                if (ivars->blockTransferDMA)
                {
                    ivars->blockTransferDMA->CompleteDMA(kIODMACommandCompleteDMANoOptions);
                }
                IOAddressSegment virtualAddressSegment = { 0 };
                ivars->blockTransferBuffer->GetAddressRange(&virtualAddressSegment);

                if(memcmp(reinterpret_cast<void*>(virtualAddressSegment.address),
                          reinterpret_cast<void*>(virtualAddressSegment.address + (IOVMPageSize * kPEX8733DefaultTestTransferSizeNumPages / 2)),
                          (IOVMPageSize * kPEX8733DefaultTestTransferSizeNumPages / 2)) != 0)
                {
                    debugLog("source and destination do not match\n");
                }
                else
                {
                    debugLog("dma successful\n");
                }

                ivars->pciDevice->ConfigurationWrite32(ivars->vendorCapabilityOffset + kPEX8733DMAChannelSourceAddressLowerOffset,
                                                       0);
                ivars->pciDevice->ConfigurationWrite32(ivars->vendorCapabilityOffset + kPEX8733DMAChannelSourceAddressUpperOffset,
                                                       0);

                ivars->pciDevice->ConfigurationWrite32(ivars->vendorCapabilityOffset + kPEX8733DMAChannelDestinationAddressLowerOffset,
                                                       0);
                ivars->pciDevice->ConfigurationWrite32(ivars->vendorCapabilityOffset + kPEX8733DMAChannelDestinationAddressUpperOffset,
                                                       0);
                OSSafeReleaseNULL(ivars->blockTransferDMA);
                OSSafeReleaseNULL(ivars->blockTransferBuffer);
                ivars->currentTest = kPEX8733TestModeNone;
                // start the next test
                dmaDescriptorTransferTest();
                break;
            }
            case kPEX8733TestModeDescriptor:
            {
                for(int j = 0; j < kPEX8733DefaultNumDMADescriptors; j++)
                {
                    for(unsigned int i = 0; i < kPEX8733DefaultNumDMADescriptors; i++)
                    {
                        if (ivars->descriptorTransferDMAs[i])
                        {
                            ivars->descriptorTransferDMAs[i]->CompleteDMA(kIODMACommandCompleteDMANoOptions);
                        }
                    }

                    IOAddressSegment descriptorBufferVirtualAddressSegment = { 0 };
                    ivars->descriptorTransferBuffers[j]->GetAddressRange(&descriptorBufferVirtualAddressSegment);

                    if(memcmp(reinterpret_cast<void*>(descriptorBufferVirtualAddressSegment.address),
                              reinterpret_cast<void*>(descriptorBufferVirtualAddressSegment.address + (descriptorBufferVirtualAddressSegment.length / 2)),
                              (descriptorBufferVirtualAddressSegment.length / 2)) != 0)
                    {
                        debugLog("descriptor [%u]: source and destination do not match\n", j);
                    }
                    else
                    {
                        debugLog("dma[%u] successful\n", j);
                    }
                }

                ivars->pciDevice->ConfigurationWrite32(ivars->vendorCapabilityOffset + kPEX8733DMAChannelDescriptorRingAddressLowerOffset,
                                                       0);
                ivars->pciDevice->ConfigurationWrite32(ivars->vendorCapabilityOffset + kPEX8733DMAChannelDescriptorRingAddressUpperOffset,
                                                       0);
                ivars->pciDevice->ConfigurationWrite32(ivars->vendorCapabilityOffset + kPEX8733DMAChannelDescriptorRingNextDescriptorAddressLowerOffset,
                                                       0);

                OSSafeReleaseNULL(ivars->ringBufferDMA);
                OSSafeReleaseNULL(ivars->ringBuffer);
                for(unsigned int i = 0; i < kPEX8733DefaultNumDMADescriptors; i++)
                {
                    OSSafeReleaseNULL(ivars->descriptorTransferDMAs[i]);
                    OSSafeReleaseNULL(ivars->descriptorTransferBuffers[i]);
                }
                IODelete(ivars->descriptorTransferBuffers, IOBufferMemoryDescriptor*, kPEX8733DefaultNumDMADescriptors);
                IODelete(ivars->descriptorTransferDMAs, IODMACommand*, kPEX8733DefaultNumDMADescriptors);
                ivars->currentTest = kPEX8733TestModeNone;

                debugLog("Tests complete");
                break;
            }
            default:
            {
                debugLog("unkown test");
                break;
            }
        }
    }
}

kern_return_t
PCIDriverKitPEX8733::dmaBlockTransferTest()
{
    ivars->currentTest = kPEX8733TestModeBlock;
    debugLog("starting dmaBlockTransferTest\n");
    kern_return_t result = kIOReturnSuccess;

    uint64_t         bufferCapacity         = IOVMPageSize * kPEX8733DefaultTestTransferSizeNumPages;
    uint64_t         bufferAlignment        = IOVMPageSize;
    IOAddressSegment virtualAddressSegment  = { 0 };
    IOAddressSegment physicalAddressSegment = { 0 };
    uint64_t         dmaFlags               = 0;
    uint32_t         dmaSegmentCount        = 1;

    IOBufferMemoryDescriptor::Create(kIOMemoryDirectionOutIn,
                                     bufferCapacity,
                                     bufferAlignment,
                                     &(ivars->blockTransferBuffer));
    ivars->blockTransferBuffer->SetLength(bufferCapacity);
    ivars->blockTransferBuffer->GetAddressRange(&virtualAddressSegment);

    // Populate the first half of the buffer with data to be written to the 2nd half
    uint64_t transferSize = virtualAddressSegment.length / 2;
    for(unsigned int i = 0; i < transferSize; i++)
    {
        reinterpret_cast<uint8_t*>(virtualAddressSegment.address)[i] = i % 0xFF;
    }

	IODMACommandSpecification dmaSpecification;
	bzero(&dmaSpecification, sizeof(dmaSpecification));
	dmaSpecification.options = kIODMACommandSpecificationNoOptions;
	dmaSpecification.maxAddressBits = 64;
	IODMACommand::Create(ivars->pciDevice, kIODMACommandCreateNoOptions, &dmaSpecification, &(ivars->blockTransferDMA));

    ivars->blockTransferDMA->PrepareForDMA(kIODMACommandPrepareForDMANoOptions,
                                              ivars->blockTransferBuffer,
                                              0,
                                              0,
                                              &dmaFlags,
                                              &dmaSegmentCount,
                                              &physicalAddressSegment);

    ivars->pciDevice->ConfigurationWrite32(ivars->vendorCapabilityOffset + kPEX8733DMAChannelSourceAddressLowerOffset,
                                           static_cast<uint32_t>(physicalAddressSegment.address));
    ivars->pciDevice->ConfigurationWrite32(ivars->vendorCapabilityOffset + kPEX8733DMAChannelSourceAddressUpperOffset,
                                           static_cast<uint32_t>(physicalAddressSegment.address >> 32));

    ivars->pciDevice->ConfigurationWrite32(ivars->vendorCapabilityOffset + kPEX8733DMAChannelDestinationAddressLowerOffset,
                                           static_cast<uint32_t>(physicalAddressSegment.address + transferSize));
    ivars->pciDevice->ConfigurationWrite32(ivars->vendorCapabilityOffset + kPEX8733DMAChannelDestinationAddressUpperOffset,
                                           static_cast<uint32_t>((physicalAddressSegment.address + transferSize) >> 32));


    ivars->pciDevice->ConfigurationWrite32(ivars->vendorCapabilityOffset + kPEX8733DMAChannelTransferSizeOffset,
                                           kPEX8733DMAChannelTransferSizeRegisterDMAValid
                                           | kPEX8733DMAChannelTransferSizeRegisterDoneInterruptEnable
                                           | (transferSize & kPEX8733DMAChannelTransferSizeRegisterTransferSizeRange));

    uint32_t interruptControlStatus = 0;
    ivars->pciDevice->ConfigurationRead32(ivars->vendorCapabilityOffset + kPEX8733DMAChannelInterruptControlStatusRegisterOffset,
                                          &interruptControlStatus);

    // clear interrupts (clear on write)
    interruptControlStatus |= kPEX8733DMAChannelInterruptControlStatusInterruptStatusRange;

    // enable interrupts
    interruptControlStatus |= kPEX8733DMAChannelInterruptControlStatusErrorInterruptEnable
                              | kPEX8733DMAChannelInterruptControlStatusInvalidDescriptorInterruptEnable
                              | kPEX8733DMAChannelInterruptControlStatusGracefulPauseDoneInterruptEnable
                              | kPEX8733DMAChannelInterruptControlStatusImmediatePauseDoneInterruptEnable;

    ivars->pciDevice->ConfigurationWrite32(ivars->vendorCapabilityOffset + kPEX8733DMAChannelInterruptControlStatusRegisterOffset,
                                           interruptControlStatus);

    // Enable DMA Block Mode, disable descriptor completion status writeback, and clear any active status bits
    uint32_t controlStatus = 0;
    ivars->pciDevice->ConfigurationRead32(ivars->vendorCapabilityOffset + kPEX8733DMAChannelControlStatusRegisterOffset,
                                          &controlStatus);

    controlStatus &= (~kPEX8733DMAChannelControlStatusDescriptorModeSelectRange);
    controlStatus &= ~kPEX8733DMAChannelControlStatusCompletionStatusWriteBackEnable;
    controlStatus |= kPEX8733DMAChannelControlStatusStart | kPEX8733DMAChannelControlStatusDescriptorModeSelectBlockMode;

    // clear any active status bits
    controlStatus |= kPEX8733DMAChannelControlStatusRange | kPEX8733DMAChannelControlStatusHeaderLoggingValid;

    // start DMA transfer
    ivars->pciDevice->ConfigurationWrite32(ivars->vendorCapabilityOffset + kPEX8733DMAChannelControlStatusRegisterOffset,
                                           controlStatus);
    return result;
}

kern_return_t
PCIDriverKitPEX8733::dmaDescriptorTransferTest()
{
    ivars->currentTest = kPEX8733TestModeDescriptor;

    debugLog("starting dmaDescriptorTransferTest\n");

    kern_return_t    result                           = kIOReturnSuccess;
    unsigned int     numDescriptors                   = kPEX8733DefaultNumDMADescriptors;
    uint64_t         transferBufferSize               = IOVMPageSize * kPEX8733DefaultTestTransferSizeNumPages;
    uint64_t         transferBufferAlignment          = IOVMPageSize;
    IOAddressSegment ringBufferVirtualAddressSegment  = { 0 };
    IOAddressSegment ringBufferPhysicalAddressSegment = { 0 };
    uint64_t         ringDmaFlags                     = 0;
    uint32_t         ringDmaSegmentCount              = 1;

    ivars->descriptorTransferBuffers = IONew(IOBufferMemoryDescriptor*, numDescriptors);
    ivars->descriptorTransferDMAs = IONew(IODMACommand*, numDescriptors);

    IOBufferMemoryDescriptor::Create(kIOMemoryDirectionOut,
                                     numDescriptors * sizeof(DMAStandardDescriptor),
                                     transferBufferAlignment,
                                     &ivars->ringBuffer);

    ivars->ringBuffer->SetLength(numDescriptors * sizeof(DMAStandardDescriptor));
    ivars->ringBuffer->GetAddressRange(&ringBufferVirtualAddressSegment);

	IODMACommandSpecification dmaSpecification;
	bzero(&dmaSpecification, sizeof(dmaSpecification));
	dmaSpecification.options = kIODMACommandSpecificationNoOptions;
	dmaSpecification.maxAddressBits = 64;
	IODMACommand::Create(ivars->pciDevice, kIODMACommandCreateNoOptions, &dmaSpecification, &(ivars->ringBufferDMA));

    ivars->ringBufferDMA->PrepareForDMA(kIODMACommandPrepareForDMANoOptions,
                                     ivars->ringBuffer,
                                     0,
                                     0,
                                     &ringDmaFlags,
                                     &ringDmaSegmentCount,
                                     &ringBufferPhysicalAddressSegment);

    ivars->pciDevice->ConfigurationWrite32(ivars->vendorCapabilityOffset + kPEX8733DMAChannelDescriptorRingAddressLowerOffset,
                                           static_cast<uint32_t>(ringBufferPhysicalAddressSegment.address));
    ivars->pciDevice->ConfigurationWrite32(ivars->vendorCapabilityOffset + kPEX8733DMAChannelDescriptorRingAddressUpperOffset,
                                           static_cast<uint32_t>(ringBufferPhysicalAddressSegment.address >> 32));

    ivars->pciDevice->ConfigurationWrite32(ivars->vendorCapabilityOffset + kPEX8733DMAChannelDescriptorRingNextDescriptorAddressLowerOffset,
                                           static_cast<uint32_t>(ringBufferPhysicalAddressSegment.address));

    DMAStandardDescriptor* descriptorRing = reinterpret_cast<DMAStandardDescriptor*>(ringBufferVirtualAddressSegment.address);
    for(unsigned int i = 0; i < numDescriptors; i++)
    {
        IOAddressSegment descriptorBufferVirtualAddressSegment  = { 0 };
        IOAddressSegment descriptorBufferPhysicalAddressSegment = { 0 };
        uint64_t         dmaFlags                               = 0;
        uint32_t         dmaSegmentCount                        = 1;

        IOBufferMemoryDescriptor::Create(kIOMemoryDirectionOutIn,
                                         transferBufferSize,
                                         transferBufferAlignment,
                                         &ivars->descriptorTransferBuffers[i]);

        ivars->descriptorTransferBuffers[i]->SetLength(transferBufferSize);
        ivars->descriptorTransferBuffers[i]->GetAddressRange(&descriptorBufferVirtualAddressSegment);

		IODMACommandSpecification dmaSpecification;
		bzero(&dmaSpecification, sizeof(dmaSpecification));
		dmaSpecification.options = kIODMACommandSpecificationNoOptions;
		dmaSpecification.maxAddressBits = 64;
		IODMACommand::Create(ivars->pciDevice, kIODMACommandCreateNoOptions, &dmaSpecification, &(ivars->descriptorTransferDMAs[i]));

        ivars->descriptorTransferDMAs[i]->PrepareForDMA(kIODMACommandPrepareForDMANoOptions,
                                                           ivars->descriptorTransferBuffers[i],
                                                           0,
                                                           0,
                                                           &dmaFlags,
                                                           &dmaSegmentCount,
                                                           &descriptorBufferPhysicalAddressSegment);

        // Populate the first half of the buffer with data to be written to the 2nd half
        uint64_t transferSize = descriptorBufferVirtualAddressSegment.length / 2;
        for(unsigned int j = 0; j < transferSize; j++)
        {
            reinterpret_cast<uint8_t*>(descriptorBufferVirtualAddressSegment.address)[j] = j % 0xFF;
        }

        // setup the descriptor in the ring
        descriptorRing[i]                         = { 0 };
        descriptorRing[i].transferSize            = transferSize & 0x7FFFFFF;
        descriptorRing[i].descriptorValid         = 1;
        descriptorRing[i].lowerSourceAddress      = static_cast<uint32_t>(descriptorBufferPhysicalAddressSegment.address);
        descriptorRing[i].upperSourceAddress      = ((descriptorBufferPhysicalAddressSegment.address >> 32) & 0xFFFF);
        descriptorRing[i].lowerDestinationAddress = static_cast<uint32_t>(descriptorBufferPhysicalAddressSegment.address + transferSize);
        descriptorRing[i].upperDestinationAddress = (((descriptorBufferPhysicalAddressSegment.address + transferSize) >> 32) & 0xFFFF);

        if(i == (numDescriptors - 1))
        {
            // interrupt when the last descriptor is done
            descriptorRing[i].interruptWhenDoneWithDescriptor = 1;
        }
    }

    ivars->pciDevice->ConfigurationWrite32(ivars->vendorCapabilityOffset + kPEX8733DMAChannelDescriptorRingRingSizeOffset,
                                           numDescriptors);
    ivars->pciDevice->ConfigurationWrite32(ivars->vendorCapabilityOffset + kPEX8733DMAChannelMaximumPrefetchLimitOffset,
                                           kPEX8733DefaultDMADescriptorPrefetchLimit);

    uint32_t interruptControlStatus = 0;
    ivars->pciDevice->ConfigurationRead32(ivars->vendorCapabilityOffset + kPEX8733DMAChannelInterruptControlStatusRegisterOffset,
                                          &interruptControlStatus);

    // clear interrupts (clear on write)
    interruptControlStatus |= kPEX8733DMAChannelInterruptControlStatusInterruptStatusRange;

    // enable interrupts
    interruptControlStatus |= kPEX8733DMAChannelInterruptControlStatusEnableRange;

    ivars->pciDevice->ConfigurationWrite32(ivars->vendorCapabilityOffset + kPEX8733DMAChannelInterruptControlStatusRegisterOffset,
                                           interruptControlStatus);

    // Enable DMA Block Mode, disable descriptor completion status writeback, and clear any active status bits
    uint32_t controlStatus = 0;
    ivars->pciDevice->ConfigurationRead32(ivars->vendorCapabilityOffset + kPEX8733DMAChannelControlStatusRegisterOffset,
                                          &controlStatus);

    controlStatus &= (~kPEX8733DMAChannelControlStatusDescriptorModeSelectRange);
    controlStatus &= ~kPEX8733DMAChannelControlStatusCompletionStatusWriteBackEnable;
    controlStatus |= kPEX8733DMAChannelControlStatusStart | kPEX8733DMAChannelControlStatusRingStopMode | kPEX8733DMAChannelControlStatusDescriptorModeSelectOffChip;

    // clear any active status bits
    controlStatus |= kPEX8733DMAChannelControlStatusRange;

    // start DMA transfer
    ivars->pciDevice->ConfigurationWrite32(ivars->vendorCapabilityOffset + kPEX8733DMAChannelControlStatusRegisterOffset,
                                           controlStatus);
    return result;
}
