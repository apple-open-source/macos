/*
cc pcidump.c -o /tmp/pcidump -Wall -framework IOKit -framework CoreFoundation -arch i386
 */

#include <assert.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitKeys.h>


enum {
	kACPIMethodAddressSpaceRead		= 0,
	kACPIMethodAddressSpaceWrite	= 1,
	kACPIMethodDebuggerCommand		= 2,
	kACPIMethodCount
};

#pragma pack(1)

typedef UInt32 IOACPIAddressSpaceID;

enum {
    kIOACPIAddressSpaceIDSystemMemory       = 0,
    kIOACPIAddressSpaceIDSystemIO           = 1,
    kIOACPIAddressSpaceIDPCIConfiguration   = 2,
    kIOACPIAddressSpaceIDEmbeddedController = 3,
    kIOACPIAddressSpaceIDSMBus              = 4
};

/*
 * 64-bit ACPI address
 */
union IOACPIAddress {
    UInt64 addr64;
    struct {
        unsigned int offset     :16;
        unsigned int function   :3;
        unsigned int device     :5;
        unsigned int bus        :8;
        unsigned int segment    :16;
        unsigned int reserved   :16;
    } pci;
};
typedef union IOACPIAddress IOACPIAddress;

#pragma pack()

/* Definitions of PCI Config Registers */
enum {
    kIOPCIConfigVendorID                = 0x00,
    kIOPCIConfigDeviceID                = 0x02,
    kIOPCIConfigCommand                 = 0x04,
    kIOPCIConfigStatus                  = 0x06,
    kIOPCIConfigRevisionID              = 0x08,
    kIOPCIConfigClassCode               = 0x09,
    kIOPCIConfigCacheLineSize           = 0x0C,
    kIOPCIConfigLatencyTimer            = 0x0D,
    kIOPCIConfigHeaderType              = 0x0E,
    kIOPCIConfigBIST                    = 0x0F,
    kIOPCIConfigBaseAddress0            = 0x10,
    kIOPCIConfigBaseAddress1            = 0x14,
    kIOPCIConfigBaseAddress2            = 0x18,
    kIOPCIConfigBaseAddress3            = 0x1C,
    kIOPCIConfigBaseAddress4            = 0x20,
    kIOPCIConfigBaseAddress5            = 0x24,
    kIOPCIConfigCardBusCISPtr           = 0x28,
    kIOPCIConfigSubSystemVendorID       = 0x2C,
    kIOPCIConfigSubSystemID             = 0x2E,
    kIOPCIConfigExpansionROMBase        = 0x30,
    kIOPCIConfigCapabilitiesPtr         = 0x34,
    kIOPCIConfigInterruptLine           = 0x3C,
    kIOPCIConfigInterruptPin            = 0x3D,
    kIOPCIConfigMinimumGrant            = 0x3E,
    kIOPCIConfigMaximumLatency          = 0x3F
};

/* Definitions of Capabilities PCI Config Register */
enum {
    kIOPCICapabilityIDOffset            = 0x00,
    kIOPCINextCapabilityOffset          = 0x01,

    kIOPCIPowerManagementCapability     = 0x01,
    kIOPCIAGPCapability                 = 0x02,
    kIOPCIVitalProductDataCapability    = 0x03,
    kIOPCISlotIDCapability              = 0x04,
    kIOPCIMSICapability                 = 0x05,
    kIOPCICPCIHotswapCapability         = 0x06,
    kIOPCIPCIXCapability                = 0x07,
    kIOPCILDTCapability                 = 0x08,
    kIOPCIVendorSpecificCapability      = 0x09,
    kIOPCIDebugPortCapability           = 0x0a,
    kIOPCICPCIResourceControlCapability = 0x0b,
    kIOPCIHotplugCapability             = 0x0c,
    kIOPCIAGP8Capability                = 0x0e,
    kIOPCISecureCapability              = 0x0f,
    kIOPCIPCIExpressCapability          = 0x10,
    kIOPCIMSIXCapability                = 0x11,

    kIOPCIExpressErrorReportingCapability     = -1UL,
    kIOPCIExpressVirtualChannelCapability     = -2UL,
    kIOPCIExpressDeviceSerialNumberCapability = -3UL,
    kIOPCIExpressPowerBudgetCapability        = -4UL
};

/* Space definitions */
enum {
    kIOPCIConfigSpace           = 0,
    kIOPCIIOSpace               = 1,
    kIOPCI32BitMemorySpace      = 2,
    kIOPCI64BitMemorySpace      = 3
};

/* Command register definitions */
enum {
    kIOPCICommandIOSpace                = 0x0001,
    kIOPCICommandMemorySpace            = 0x0002,
    kIOPCICommandBusMaster              = 0x0004,
    kIOPCICommandSpecialCycles          = 0x0008,
    kIOPCICommandMemWrInvalidate        = 0x0010,
    kIOPCICommandPaletteSnoop           = 0x0020,
    kIOPCICommandParityError            = 0x0040,
    kIOPCICommandAddressStepping        = 0x0080,
    kIOPCICommandSERR                   = 0x0100,
    kIOPCICommandFastBack2Back          = 0x0200,
    kIOPCICommandInterruptDisable       = 0x0400
};

/* Status register definitions */
enum {
    kIOPCIStatusCapabilities            = 0x0010,
    kIOPCIStatusPCI66                   = 0x0020,
    kIOPCIStatusUDF                     = 0x0040,
    kIOPCIStatusFastBack2Back           = 0x0080,
    kIOPCIStatusDevSel0                 = 0x0000,
    kIOPCIStatusDevSel1                 = 0x0200,
    kIOPCIStatusDevSel2                 = 0x0400,
    kIOPCIStatusDevSel3                 = 0x0600,
    kIOPCIStatusTargetAbortCapable      = 0x0800,
    kIOPCIStatusTargetAbortActive       = 0x1000,
    kIOPCIStatusMasterAbortActive       = 0x2000,
    kIOPCIStatusSERRActive              = 0x4000,
    kIOPCIStatusParityErrActive         = 0x8000
};

// constants which are part of the PCI Bus Power Management Spec.
enum
{
    // capabilities bits in the 16 bit capabilities register
    kPCIPMCPMESupportFromD3Cold = 0x8000,
    kPCIPMCPMESupportFromD3Hot  = 0x4000,
    kPCIPMCPMESupportFromD2             = 0x2000,
    kPCIPMCPMESupportFromD1             = 0x1000,
    kPCIPMCPMESupportFromD0             = 0x0800,
    kPCIPMCD2Support                    = 0x0400,
    kPCIPMCD1Support                    = 0x0200,
 
    kPCIPMCD3Support                    = 0x0001
};

enum
{
    // bits in the power management control/status register
    kPCIPMCSPMEStatus                   = 0x8000,
    kPCIPMCSPMEEnable                   = 0x0100,
    kPCIPMCSPowerStateMask              = 0x0003,
    kPCIPMCSPowerStateD3                = 0x0003,
    kPCIPMCSPowerStateD2                = 0x0002,
    kPCIPMCSPowerStateD1                = 0x0001,
    kPCIPMCSPowerStateD0                = 0x0000,
    
    kPCIPMCSDefaultEnableBits           = (~(IOOptionBits)0)
};

union IOPCIAddressSpace {
    UInt32              bits;
    struct {
#if __BIG_ENDIAN__
        unsigned int    resv:4;
        unsigned int    registerNumExtended:4;
        unsigned int    busNum:8;
        unsigned int    deviceNum:5;
        unsigned int    functionNum:3;
        unsigned int    registerNum:8;
#elif __LITTLE_ENDIAN__
        unsigned int    registerNum:8;
        unsigned int    functionNum:3;
        unsigned int    deviceNum:5;
        unsigned int    busNum:8;
        unsigned int    registerNumExtended:4;
        unsigned int    resv:4;
#endif
    } es;
};
typedef union IOPCIAddressSpace IOPCIAddressSpace;


enum {
    kPCIHeaderType0 = 0,
    kPCIHeaderType1 = 1,
    kPCIHeaderType2 = 2
};

enum {
    kPCI2PCIPrimaryBus          = 0x18,
    kPCI2PCISecondaryBus        = 0x19,
    kPCI2PCISubordinateBus      = 0x1a,
    kPCI2PCISecondaryLT         = 0x1b,
    kPCI2PCIIORange             = 0x1c,
    kPCI2PCIMemoryRange         = 0x20,
    kPCI2PCIPrefetchMemoryRange = 0x24,
    kPCI2PCIPrefetchUpperBase   = 0x28,
    kPCI2PCIPrefetchUpperLimit  = 0x2c,
    kPCI2PCIUpperIORange        = 0x30,
    kPCI2PCIBridgeControl       = 0x3e
};



struct AddressSpaceParam {
	UInt64			value;
	UInt32			spaceID;
	IOACPIAddress	address;
	UInt32			bitWidth;
	UInt32			bitOffset;
	UInt32			options;
};
typedef struct AddressSpaceParam AddressSpaceParam;

static uint32_t configRead32(io_connect_t connect, uint32_t segment,
                                uint32_t bus, uint32_t device, uint32_t function,
                                uint32_t offset)
{
    AddressSpaceParam param;
    kern_return_t     status;

    param.spaceID   = kIOACPIAddressSpaceIDPCIConfiguration;
    param.bitWidth  = 32;
    param.bitOffset = 0;
    param.options   = 0;

    param.address.pci.offset   = offset;
    param.address.pci.function = function;
    param.address.pci.device   = device;
    param.address.pci.bus      = bus;
    param.address.pci.segment  = segment;
    param.address.pci.reserved = 0;
    param.value                = -1ULL;

    size_t outSize = sizeof(param);
    status = IOConnectCallStructMethod(connect, kACPIMethodAddressSpaceRead,
                                            &param, sizeof(param),
                                            &param, &outSize);
    assert(kIOReturnSuccess == status);
    return ((uint32_t) param.value);
}


static void configWrite32(io_connect_t connect, uint32_t segment,
                                uint32_t bus, uint32_t device, uint32_t function,
                                uint32_t offset,
                                uint32_t data)
{
    AddressSpaceParam param;
    kern_return_t     status;

    param.spaceID   = kIOACPIAddressSpaceIDPCIConfiguration;
    param.bitWidth  = 32;
    param.bitOffset = 0;
    param.options   = 0;

    param.address.pci.offset   = offset;
    param.address.pci.function = function;
    param.address.pci.device   = device;
    param.address.pci.bus      = bus;
    param.address.pci.segment  = segment;
    param.address.pci.reserved = 0;
    param.value                = data;

    size_t outSize = 0;
    status = IOConnectCallStructMethod(connect, kACPIMethodAddressSpaceWrite,
                                            &param, sizeof(param),
                                            NULL, &outSize);
    assert(kIOReturnSuccess == status);
}

static void physWrite32(io_connect_t connect, uint64_t offset, uint32_t data)
{
    AddressSpaceParam param;
    kern_return_t     status;

    param.spaceID   = kIOACPIAddressSpaceIDSystemMemory;
    param.bitWidth  = 32;
    param.bitOffset = 0;
    param.options   = 0;

    param.address.addr64 = offset;
    param.value          = data;

    size_t outSize = 0;
    status = IOConnectCallStructMethod(connect, kACPIMethodAddressSpaceWrite,
                                            &param, sizeof(param),
                                            NULL, &outSize);
    assert(kIOReturnSuccess == status);
}

static uint32_t physRead32(io_connect_t connect, uint64_t offset)
{
    AddressSpaceParam param;
    kern_return_t     status;

    param.spaceID   = kIOACPIAddressSpaceIDSystemMemory;
    param.bitWidth  = 32;
    param.bitOffset = 0;
    param.options   = 0;

    param.address.addr64 = offset;
    param.value          = -1ULL;

    size_t outSize = sizeof(param);
    status = IOConnectCallStructMethod(connect, kACPIMethodAddressSpaceRead,
                                            &param, sizeof(param),
                                            &param, &outSize);
    assert(kIOReturnSuccess == status);

    return ((uint32_t) param.value);
}

static uint32_t ioRead32(io_connect_t connect, uint64_t offset)
{
    AddressSpaceParam param;
    kern_return_t     status;

    param.spaceID   = kIOACPIAddressSpaceIDSystemIO;
    param.bitWidth  = 16;
    param.bitOffset = 0;
    param.options   = 0;

    param.address.addr64 = offset;
    param.value          = -1ULL;

    size_t outSize = sizeof(param);
    status = IOConnectCallStructMethod(connect, kACPIMethodAddressSpaceRead,
                                            &param, sizeof(param),
                                            &param, &outSize);
    assert(kIOReturnSuccess == status);

    return ((uint32_t) param.value);
}

io_registry_entry_t lookService(uint32_t segment,
                                uint32_t bus, uint32_t device, uint32_t function)
{
    kern_return_t status;
    io_iterator_t iter;
    io_service_t service;

    IOPCIAddressSpace space;
    space.bits           = 0;
    space.es.busNum      = bus;
    space.es.deviceNum   = device;
    space.es.functionNum = function;
    
    status = IOServiceGetMatchingServices(kIOMasterPortDefault, 
                                            IOServiceMatching("IOPCIDevice"), &iter);
    assert(kIOReturnSuccess == status);

    while ((service = IOIteratorNext(iter)))
    {
        CFDataRef reg;
        UInt32    bits;

        reg = IORegistryEntryCreateCFProperty(service, CFSTR("reg"), 
                    kCFAllocatorDefault, kNilOptions);
        bits = 0;

        if (reg)
        {
            if (CFDataGetTypeID() == CFGetTypeID(reg))
                bits = ((UInt32 *)CFDataGetBytePtr(reg))[0];
            CFRelease(reg);
        }
        if (bits == space.bits)
        {
            IOObjectRetain(service);
            break;
        }
    }
    IOObjectRelease(iter);

    return (service);
}

static void dump( const uint8_t * bytes, size_t len )
{
    int i;

    printf("        0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F");
    for (i = 0; i < len; i++)
    {
        if( 0 == (i & 15))
            printf("\n    %02X:", i);
        printf(" %02x", bytes[i]);
    }
    printf("\n");
}

static void
dumpDevice(io_connect_t connect, uint32_t segment,
                                uint32_t bus, uint32_t device, uint32_t fn,
                                uint32_t * maxBus, uint32_t * maxFn)
{
    io_registry_entry_t service;
    kern_return_t       status;
    io_name_t           name;
    uint64_t     		entryID;
    uint32_t off;
    uint32_t vendProd;
    uint32_t vend;
    uint32_t prod;
    uint32_t headerType;
    uint32_t priBusNum;
    uint32_t secBusNum;
    uint32_t subBusNum;
    uint32_t data[256/sizeof(uint32_t)];
    uint8_t *bytes = (uint8_t *)&data[0];

    for(off = 0; off < 256; off += 4)
        data[off >> 2] = configRead32(connect, segment, bus, device, fn, off);

    vendProd = data[0];
    vend = vendProd & 0xffff;
    prod = vendProd >> 16;
    printf("[%d, %d, %d] 0x%04x, 0x%04x - ", bus, device, fn, vend, prod);

    service = lookService(segment, bus, device, fn);
    if (service)
    {
        status = IORegistryEntryGetName(service, name);
        assert(kIOReturnSuccess == status);
        status = IORegistryEntryGetRegistryEntryID(service, &entryID);
        assert(kIOReturnSuccess == status);
        printf("\"%s\", 0x%qx - ", name, entryID);
        IOObjectRelease(service);
    }

    headerType = bytes[kIOPCIConfigHeaderType];
    if (maxFn && (0x80 & headerType))
        *maxFn = 7;
    headerType &= 0x7f;
    if (!headerType)
    {
        // device dump
        printf("class: 0x%x, 0x%x, 0x%x\n", 
                bytes[kIOPCIConfigRevisionID + 3],
                bytes[kIOPCIConfigRevisionID + 2],
                bytes[kIOPCIConfigRevisionID + 1]);
    }
    else
    {
        priBusNum = bytes[kPCI2PCIPrimaryBus];
        secBusNum = bytes[kPCI2PCISecondaryBus];
        subBusNum = bytes[kPCI2PCISubordinateBus];
        printf("bridge: [%d, %d, %d]\n", priBusNum, secBusNum, subBusNum);
        if (maxBus && (subBusNum > *maxBus))
            *maxBus = subBusNum;
    }

    dump(bytes, sizeof(data));
    printf("\n");
}


int main(int argc, char **argv)
{
    io_registry_entry_t    service;
    io_connect_t           connect;
    kern_return_t          status;

    service = IOServiceGetMatchingService(kIOMasterPortDefault, 
                                            IOServiceMatching("AppleACPIPlatformExpert"));
    assert(service);
    if (service) 
    {
        status = IOServiceOpen(service, mach_task_self(), 0, &connect);
        IOObjectRelease(service);
        assert(kIOReturnSuccess == status);
    }

    uint32_t count = 0;
    uint32_t segment = 0;
    uint32_t maxBus = 0;
    uint32_t bus, device, fn, maxFn;
    uint32_t vendProd;

    if (argc > 3)
    {
        bus    = strtoul(argv[1], NULL, 0);
        device = strtoul(argv[2], NULL, 0);
        fn     = strtoul(argv[3], NULL, 0);
		if (argc == 4)
		{
            dumpDevice(connect, segment, bus, device, fn, NULL, NULL);
    	    count++;
		}
        if (argc > 5)
        {
            uint32_t offs;
            uint32_t data;
            offs    = strtoul(argv[4], NULL, 0);
            data = strtoul(argv[5], NULL, 0);
            configWrite32(connect, segment, bus, device, fn, offs, data);
            printf("wrote 0x%08x to [%d, %d, %d]:0x%X\n", data, bus, device, fn, offs);
        }
        else if (argc > 4)
        {
            uint32_t offs;
            uint32_t data;
            offs    = strtoul(argv[4], NULL, 0);
            data = configRead32(connect, segment, bus, device, fn, offs);
            printf("read 0x%08x from [%d, %d, %d]:0x%X\n", data, bus, device, fn, offs);
        }
    }
    else if (argc > 2)
    {
        uint64_t offs;
        uint32_t data;
        offs = strtoull(argv[1], NULL, 0);
        data = strtoul(argv[2], NULL, 0);
        physWrite32(connect, offs, data);
        printf("wrote 0x%08x to 0x%llX\n", data, offs);
    }
    else if (argc > 1)
    {
        uint64_t offs;
        uint32_t data;
        offs = strtoull(argv[1], NULL, 0);
		if (true || (offs > 0x10000ULL))
		{
			data = physRead32(connect, offs);
			printf("read 0x%08x from mem 0x%llX\n", data, offs);
		}
		else
		{
			data = ioRead32(connect, offs);
			printf("read 0x%08x from i/o 0x%llX\n", data, offs);
		}
    }
    else for (bus = 0; bus <= maxBus; bus++)
    {
        for (device = 0; device < 32; device++)
        {
            maxFn = 0;
            for (fn = 0; fn <= maxFn; fn++)
            {
                vendProd = configRead32(connect, segment, bus, device, fn, kIOPCIConfigVendorID);
                if ((0xFFFFFFFF == vendProd) || !vendProd)
                    continue;
                count++;
                dumpDevice(connect, segment, bus, device, fn, &maxBus, &maxFn);
            }
        }
    }

    printf("total: %d\n", count);
    exit(0);    
}
