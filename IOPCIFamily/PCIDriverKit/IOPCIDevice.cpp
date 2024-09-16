//
//  IOPCIDevice.cpp
//  PCIDriverKit
//
//  Created by Kevin Strasberg on 10/20/19.
//

#include <DriverKit/DriverKit.h>
#include <PCIDriverKit/IOPCIDevice.h>
#include <PCIDriverkit/IOPCIFamilyDefinitions.h>
#include <PCIDriverKit/PCIDriverKitPrivate.h>

#define debugLog(fmt, args...)  os_log(OS_LOG_DEFAULT, "IOPCIDevice::%s:  " fmt,  __FUNCTION__,##args)

struct IOPCIDevice_IVars
{
    IOMemoryMap** deviceMemoryMappings;
    IOService*    deviceClient;
    uint32_t      numDeviceMemoryMappings;
    bool          useMemoryAccess;
};


bool
IOPCIDevice::init()
{
    if(super::init() != true)
    {
        return false;
    }
    ivars = IONewZero(IOPCIDevice_IVars, 1);

    if(ivars == NULL)
    {
        return false;
    }

    return true;
}

void
IOPCIDevice::free()
{
    IOSafeDeleteNULL(ivars, IOPCIDevice_IVars, 1);
    super::free();
}

kern_return_t
IOPCIDevice::Open(IOService*   forClient,
                  IOOptionBits options)
{
    kern_return_t result;

    if((result = _ManageSession(forClient, true, options)) != kIOReturnSuccess)
    {
        return result;
    }

    ivars->deviceClient = forClient;
    ivars->deviceClient->retain();
    ivars->useMemoryAccess = false;
    OSContainer* mapMemoryContainer = NULL;

    if(forClient->SearchProperty(kIOPCIKernelMemoryAccess, "IOService", 0, &mapMemoryContainer) == kIOReturnSuccess)
    {
        OSSafeReleaseNULL(mapMemoryContainer);
        ivars->useMemoryAccess = true;
    }

    OSContainer* deviceMemoryContainer = NULL;

    if(SearchProperty(kIOPCIDeviceMemoryArrayKey, "IOService", 0, &deviceMemoryContainer) == kIOReturnSuccess)
    {
        OSArray* deviceMemoryArray = OSDynamicCast(OSArray, deviceMemoryContainer);
        if(deviceMemoryArray != NULL)
        {
            ivars->numDeviceMemoryMappings = deviceMemoryArray->getCount();
            ivars->deviceMemoryMappings    = IONewZero(IOMemoryMap *, ivars->numDeviceMemoryMappings);
            // map all the memory for the device
            for(uint32_t memoryIndex = 0; memoryIndex < ivars->numDeviceMemoryMappings; memoryIndex++)
            {
                IOMemoryDescriptor* deviceMemoryDescriptor = NULL;
                if(_CopyDeviceMemoryWithIndex(memoryIndex, &deviceMemoryDescriptor, ivars->deviceClient) == kIOReturnSuccess)
                {
                    if(deviceMemoryDescriptor != NULL)
                    {
                        deviceMemoryDescriptor->CreateMapping(0, 0, 0, 0, 0, &(ivars->deviceMemoryMappings[memoryIndex]));
                        if(ivars->deviceMemoryMappings[memoryIndex] == NULL)
                        {
                            // mapping failed
                            result = kIOReturnNoMemory;
                            break;
                        }
                    }
                }
                OSSafeReleaseNULL(deviceMemoryDescriptor);
            }
        }
        OSSafeReleaseNULL(deviceMemoryContainer);
    }
    return result;
}

void
IOPCIDevice::Close(IOService*   forClient,
                   IOOptionBits options)
{
    if(ivars->deviceClient == forClient)
    {
        _ManageSession(ivars->deviceClient, false, options);
        if(ivars->deviceMemoryMappings != NULL)
        {
            for(uint8_t i = 0; i < ivars->numDeviceMemoryMappings; i++)
            {
                OSSafeReleaseNULL(ivars->deviceMemoryMappings[i]);
            }
            IOSafeDeleteNULL(ivars->deviceMemoryMappings, IOMemoryMap *, ivars->numDeviceMemoryMappings);
        }
        OSSafeReleaseNULL(ivars->deviceClient);
    }
}

#pragma mark Memory Accessors

void
IOPCIDevice::ConfigurationRead32(uint64_t offset, uint32_t* readData)
{
    uint64_t bounceData;
    if(_MemoryAccess(kPCIDriverKitMemoryAccessOperationConfigurationRead | kPCIDriverKitMemoryAccessOperation32Bit,
                     offset,
                     0,
                     &bounceData,
                     ivars->deviceClient,
                     0) != kIOReturnSuccess)
    {
        *readData = static_cast<uint32_t>(-1);
    }
    else
    {
        *readData = static_cast<uint32_t>(bounceData);
    }
}

void
IOPCIDevice::ConfigurationRead16(uint64_t offset, uint16_t* readData)
{
    uint64_t bounceData;
    if(_MemoryAccess(kPCIDriverKitMemoryAccessOperationConfigurationRead | kPCIDriverKitMemoryAccessOperation16Bit,
                     offset,
                     0,
                     &bounceData,
                     ivars->deviceClient,
                     0) != kIOReturnSuccess)
    {
        *readData = static_cast<uint16_t>(-1);
    }
    else
    {
        *readData = static_cast<uint16_t>(bounceData);
    }
}

void
IOPCIDevice::ConfigurationRead8(uint64_t offset, uint8_t* readData)
{
    uint64_t bounceData;
    if(_MemoryAccess(kPCIDriverKitMemoryAccessOperationConfigurationRead | kPCIDriverKitMemoryAccessOperation8Bit,
                     offset,
                     0,
                     &bounceData,
                     ivars->deviceClient,
                     0) != kIOReturnSuccess)
    {
        *readData = static_cast<uint8_t>(-1);
    }
    else
    {
        *readData = static_cast<uint8_t>(bounceData);
    }
}

void
IOPCIDevice::ConfigurationWrite32(uint64_t offset,
                                  uint32_t data)
{
    _MemoryAccess(kPCIDriverKitMemoryAccessOperationConfigurationWrite | kPCIDriverKitMemoryAccessOperation32Bit,
                  offset,
                  data,
                  NULL,
                  ivars->deviceClient,
                  0);
}

void
IOPCIDevice::ConfigurationWrite16(uint64_t offset,
                                  uint16_t data)
{
    _MemoryAccess(kPCIDriverKitMemoryAccessOperationConfigurationWrite | kPCIDriverKitMemoryAccessOperation16Bit,
                  offset,
                  data,
                  NULL,
                  ivars->deviceClient,
                  0);
}

void
IOPCIDevice::ConfigurationWrite8(uint64_t offset,
                                 uint8_t  data)
{
    _MemoryAccess(kPCIDriverKitMemoryAccessOperationConfigurationWrite | kPCIDriverKitMemoryAccessOperation8Bit,
                  offset,
                  data,
                  NULL,
                  ivars->deviceClient,
                  0);
}

void
IOPCIDevice::MemoryRead64(uint8_t   memoryIndex,
                          uint64_t  offset,
                          uint64_t* readData,
                          IOOptionBits options)
{
    if(memoryIndex >= ivars->numDeviceMemoryMappings)
    {
        *readData = static_cast<uint64_t>(-1);
        return;
    }

    IOMemoryMap* deviceMemory = ivars->deviceMemoryMappings[memoryIndex];

    if(deviceMemory && !ivars->useMemoryAccess && !(options & kIOPCIAccessLatencyTolerantHint))
    {
        *readData = *reinterpret_cast<volatile uint64_t*>(deviceMemory->GetAddress() + offset);
    }
    else if(_MemoryAccess(kPCIDriverKitMemoryAccessOperationDeviceRead | kPCIDriverKitMemoryAccessOperation64Bit | memoryIndex,
                     offset,
                     0,
                     readData,
                     ivars->deviceClient,
                     options) != kIOReturnSuccess)
    {
        *readData = static_cast<uint64_t>(-1);
    }
}

void
IOPCIDevice::MemoryRead64(uint8_t   memoryIndex,
                          uint64_t  offset,
                          uint64_t* readData)
{
	return MemoryRead64(memoryIndex, offset, readData, 0);
}

#if TARGET_CPU_X86 || TARGET_CPU_X86_64
// Assume if the memory index is in range, but doesn't have a mapping, that it's for I/O Space
#define MEMORY_READ_OPERATION kPCIDriverKitMemoryAccessOperationIORead
#else
#define MEMORY_READ_OPERATION kPCIDriverKitMemoryAccessOperationDeviceRead
#endif

void
IOPCIDevice::MemoryRead32(uint8_t   memoryIndex,
                          uint64_t  offset,
                          uint32_t* readData,
                          IOOptionBits options)
{
    if(memoryIndex >= ivars->numDeviceMemoryMappings)
    {
        *readData = static_cast<uint64_t>(-1);
        return;
    }

    IOMemoryMap* deviceMemory = ivars->deviceMemoryMappings[memoryIndex];

    if(deviceMemory == NULL || ivars->useMemoryAccess || (options & kIOPCIAccessLatencyTolerantHint))
    {
        uint64_t bounceData;
        if(_MemoryAccess(MEMORY_READ_OPERATION | kPCIDriverKitMemoryAccessOperation32Bit | memoryIndex,
                         offset,
                         0,
                         &bounceData,
                         ivars->deviceClient,
                         options) != kIOReturnSuccess)
        {
            *readData = static_cast<uint32_t>(-1);
        }
        else
        {
            *readData = static_cast<uint32_t>(bounceData);
        }
    }
    else
    {
        *readData = *reinterpret_cast<volatile uint32_t*>(deviceMemory->GetAddress() + offset);
    }
}

void
IOPCIDevice::MemoryRead32(uint8_t   memoryIndex,
                          uint64_t  offset,
                          uint32_t* readData)
{
	return MemoryRead32(memoryIndex, offset, readData, 0);
}

void
IOPCIDevice::MemoryRead16(uint8_t   memoryIndex,
                          uint64_t  offset,
                          uint16_t* readData,
                          IOOptionBits options)
{
    if(memoryIndex >= ivars->numDeviceMemoryMappings)
    {
        *readData = static_cast<uint64_t>(-1);
        return;
    }

    IOMemoryMap* deviceMemory = ivars->deviceMemoryMappings[memoryIndex];

    if(deviceMemory == NULL || ivars->useMemoryAccess || (options & kIOPCIAccessLatencyTolerantHint))
    {
        uint64_t bounceData;
        if(_MemoryAccess(MEMORY_READ_OPERATION | kPCIDriverKitMemoryAccessOperation16Bit | memoryIndex,
                         offset,
                         0,
                         &bounceData,
                         ivars->deviceClient,
                         options) != kIOReturnSuccess)
        {
            *readData = static_cast<uint16_t>(-1);
        }
        else
        {
            *readData = static_cast<uint16_t>(bounceData);
        }
    }
    else
    {
        *readData = *reinterpret_cast<volatile uint16_t*>(deviceMemory->GetAddress() + offset);
    }
}

void
IOPCIDevice::MemoryRead16(uint8_t   memoryIndex,
                          uint64_t  offset,
                          uint16_t* readData)
{
	return MemoryRead16(memoryIndex, offset, readData, 0);
}

void
IOPCIDevice::MemoryRead8(uint8_t  memoryIndex,
                         uint64_t offset,
                         uint8_t* readData,
                         IOOptionBits options)
{
    if(memoryIndex >= ivars->numDeviceMemoryMappings)
    {
        *readData = static_cast<uint64_t>(-1);
        return;
    }

    IOMemoryMap* deviceMemory = ivars->deviceMemoryMappings[memoryIndex];

    if(deviceMemory == NULL || ivars->useMemoryAccess || (options & kIOPCIAccessLatencyTolerantHint))
    {
        uint64_t bounceData;
        if(_MemoryAccess(MEMORY_READ_OPERATION | kPCIDriverKitMemoryAccessOperation8Bit | memoryIndex,
                         offset,
                         0,
                         &bounceData,
                         ivars->deviceClient,
                         options) != kIOReturnSuccess)
        {
            *readData = static_cast<uint8_t>(-1);
        }
        else
        {
            *readData = static_cast<uint8_t>(bounceData);
        }
    }
    else
    {
        *readData = *reinterpret_cast<volatile uint8_t*>(deviceMemory->GetAddress() + offset);
    }
}

void
IOPCIDevice::MemoryRead8(uint8_t  memoryIndex,
                         uint64_t offset,
                         uint8_t* readData)
{
	return MemoryRead8(memoryIndex, offset, readData, 0);
}

void
IOPCIDevice::MemoryWrite64(uint8_t  memoryIndex,
                           uint64_t offset,
                           uint64_t data,
                           IOOptionBits options)
{
    if(memoryIndex >= ivars->numDeviceMemoryMappings)
    {
        return;
    }

    IOMemoryMap* deviceMemory = ivars->deviceMemoryMappings[memoryIndex];

    if(deviceMemory == NULL || ivars->useMemoryAccess || (options & kIOPCIAccessLatencyTolerantHint))
    {
        _MemoryAccess(kPCIDriverKitMemoryAccessOperationDeviceWrite | kPCIDriverKitMemoryAccessOperation64Bit | memoryIndex,
                      offset,
                      data,
                      NULL,
                      ivars->deviceClient,
                      options);
    }
    else
    {
        volatile uint64_t* memoryAddress = reinterpret_cast<volatile uint64_t*>(deviceMemory->GetAddress() + offset);
        *memoryAddress = data;
    }
}

void
IOPCIDevice::MemoryWrite64(uint8_t  memoryIndex,
                           uint64_t offset,
                           uint64_t data)
{
	MemoryWrite64(memoryIndex, offset, data, 0);
}

#if TARGET_CPU_X86 || TARGET_CPU_X86_64
// Assume if the memory index is in range, but doesn't have a mapping, that it's for I/O Space
#define MEMORY_WRITE_OPERATION kPCIDriverKitMemoryAccessOperationIOWrite
#else
#define MEMORY_WRITE_OPERATION kPCIDriverKitMemoryAccessOperationDeviceWrite
#endif

void
IOPCIDevice::MemoryWrite32(uint8_t  memoryIndex,
                           uint64_t offset,
                           uint32_t data,
                           IOOptionBits options)
{
    if(memoryIndex >= ivars->numDeviceMemoryMappings)
    {
        return;
    }

    IOMemoryMap* deviceMemory = ivars->deviceMemoryMappings[memoryIndex];

    if(deviceMemory == NULL || ivars->useMemoryAccess || (options & kIOPCIAccessLatencyTolerantHint))
    {
        _MemoryAccess(MEMORY_WRITE_OPERATION | kPCIDriverKitMemoryAccessOperation32Bit | memoryIndex,
                      offset,
                      data,
                      NULL,
                      ivars->deviceClient,
                      options);
    }
    else
    {
        volatile uint32_t* memoryAddress = reinterpret_cast<volatile uint32_t*>(deviceMemory->GetAddress() + offset);
        *memoryAddress = data;
    }
}

void
IOPCIDevice::MemoryWrite32(uint8_t  memoryIndex,
                           uint64_t offset,
                           uint32_t data)
{
	MemoryWrite32(memoryIndex, offset, data, 0);
}

void
IOPCIDevice::MemoryWrite16(uint8_t  memoryIndex,
                           uint64_t offset,
                           uint16_t data,
                           IOOptionBits options)
{
    if(memoryIndex >= ivars->numDeviceMemoryMappings)
    {
        return;
    }

    IOMemoryMap* deviceMemory = ivars->deviceMemoryMappings[memoryIndex];

    if(deviceMemory == NULL || ivars->useMemoryAccess || (options & kIOPCIAccessLatencyTolerantHint))
    {
        _MemoryAccess(MEMORY_WRITE_OPERATION | kPCIDriverKitMemoryAccessOperation16Bit | memoryIndex,
                      offset,
                      data,
                      NULL,
                      ivars->deviceClient,
                      options);
    }
    else
    {
        volatile uint16_t* memoryAddress = reinterpret_cast<volatile uint16_t*>(deviceMemory->GetAddress() + offset);
        *memoryAddress = data;
    }
}

void
IOPCIDevice::MemoryWrite16(uint8_t  memoryIndex,
                           uint64_t offset,
                           uint16_t data)
{
	MemoryWrite16(memoryIndex, offset, data, 0);
}

void
IOPCIDevice::MemoryWrite8(uint8_t  memoryIndex,
                          uint64_t offset,
                          uint8_t  data,
                          IOOptionBits options)
{
    if(memoryIndex >= ivars->numDeviceMemoryMappings)
    {
        return;
    }

    IOMemoryMap* deviceMemory = ivars->deviceMemoryMappings[memoryIndex];

    if(deviceMemory == NULL || ivars->useMemoryAccess || (options & kIOPCIAccessLatencyTolerantHint))
    {
        _MemoryAccess(MEMORY_WRITE_OPERATION | kPCIDriverKitMemoryAccessOperation8Bit | memoryIndex,
                      offset,
                      data,
                      NULL,
                      ivars->deviceClient,
                      options);
    }
    else
    {
        volatile uint8_t* memoryAddress = reinterpret_cast<volatile uint8_t*>(deviceMemory->GetAddress() + offset);
        *memoryAddress = data;
    }
}

void
IOPCIDevice::MemoryWrite8(uint8_t  memoryIndex,
                          uint64_t offset,
                          uint8_t  data)
{
	MemoryWrite8(memoryIndex, offset, data, 0);
}
