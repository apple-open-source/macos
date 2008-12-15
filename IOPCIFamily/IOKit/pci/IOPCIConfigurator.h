/*
 * Copyright (c) 2004-2005 Apple Computer, Inc. All rights reserved.
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

#ifndef _IOPCICONFIGURATOR_H
#define _IOPCICONFIGURATOR_H

#include <IOKit/IOLib.h>
#include <IOKit/pci/IOPCIBridge.h>


#define kPCIBridgeIOAlignment        (4096)
#define kPCIBridgeMemoryAlignment    (1024*1024)
#define kPCIBridgeBusNumberAlignment (1)

#define BRIDGE_RANGE_NUM(type)      ((type) + kIOPCIRangeBridgeMemory)

#define FOREACH_CHILD(bridge, child) \
    for(pci_dev_t (child) = (bridge)->child; (child); (child) = (child)->peer)

#define PCI_ADDRESS_TUPLE(device)   \
        device->space.s.busNum,     \
        device->space.s.deviceNum,  \
        device->space.s.functionNum

enum {
    kIOPCIConfiguratorEnable   = 0x00000001,
    kIOPCIConfiguratorAllocate = 0x00000002,
    kIOPCIConfiguratorReset    = 0x00000004,
    kIOPCIConfiguratorIOLog    = 0x00010000,
    kIOPCIConfiguratorKPrintf  = 0x00020000
};


enum {
    kIOPCIRangeFlagMaximizeSize = 0x00000001,
};
enum {
    kIOPCIRangeBAR0               = 0,
    kIOPCIRangeBAR1               = 1,
    kIOPCIRangeBAR2               = 2,
    kIOPCIRangeBAR3               = 3,
    kIOPCIRangeBAR4               = 4,
    kIOPCIRangeBAR5               = 5,
    kIOPCIRangeExpansionROM       = 6,

    // order matches kIOPCIResourceType*
    kIOPCIRangeBridgeMemory       = 7,
    kIOPCIRangeBridgePFMemory     = 8,
    kIOPCIRangeBridgeIO           = 9,
    kIOPCIRangeBridgeBusNumber    = 10,

    kIOPCIRangeCount
};

enum {
    kPCIDeviceStateBIOSConfig = 0,
    kPCIDeviceStateResourceWait,
    kPCIDeviceStateResourceAssigned,
    kPCIDeviceStateConfigurationDone
};

/*
 * PCI device struct (including bridges)
 */
struct pci_dev
{
    struct pci_dev *    child;
    struct pci_dev *    peer;
    uint32_t            classCode;
    IOPCIAddressSpace   space;
    IOPCIRange		ranges[kIOPCIRangeCount];
    uint8_t             headerType;
    uint8_t		isBridge;
    uint8_t		isHostBridge;
    uint8_t		supportsHotPlug;
    uint8_t             deviceState;
    uint8_t             secBusNum;  // bridge only
    uint8_t             subBusNum;  // bridge only
    IORegistryEntry *   dtNub;
    IORegistryEntry *   acpiDevice;
};

typedef struct pci_dev * pci_dev_t;

enum {
    kPCIHeaderType0 = 0,
    kPCIHeaderType1 = 1,
    kPCIHeaderType2 = 2
};

// value of supportsHotPlug
enum {
    kPCIStatic     = 0,
    kPCIHotPlug    = 1,
    kPCILinkChange = 2
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

#define kPCIBridgeMaxCount  256


class IOPCIConfigurator : public IOService
{
    OSDeclareDefaultStructors( IOPCIConfigurator );

    IOOptionBits	    fFlags;
    IOPCIBridge *	    fRootBridge;
    UInt8                   fCacheLineSize;
    pci_dev_t               fPCIBridgeList[kPCIBridgeMaxCount];
    int                     fPCIBridgeIndex;
    int                     fPCIBridgeTailIndex;
    int                     fBridgeConfigCount;
    int                     fDeviceConfigCount;
    int                     fYentaConfigCount;

protected:

    static void safeProbeCallback( void * refcon );
    static void configProbeCallback( void * refcon );

    static void matchDTEntry( IORegistryEntry * dtEntry, void * _context );
    static void matchACPIEntry( IORegistryEntry * dtEntry, void * _context );

    void    checkPCIConfiguration( void );
    void    pciBridgeScanBus( pci_dev_t bridge, 
			                  UInt8 busNum, UInt8 * nextBusNum, UInt8 lastBusNum );
    void    pciRangeAppendSubRange( IOPCIRange * headRange, IOPCIRange * newRange );
    void    pciBridgeCheckConfiguration( pci_dev_t bridge );
    void    pciBridgeClipRanges( IOPCIRange * rangeList, 
                                 IOPCIScalar start, IOPCIScalar size );
    void    pciBridgeAllocateResource( pci_dev_t bridge );
    void    pciBridgeDistributeResource( pci_dev_t bridge );
    void    pciBridgeDistributeResourceType( pci_dev_t bridge, UInt32 type );
    void    pciApplyConfiguration( pci_dev_t device );
    void    pciDeviceApplyConfiguration( pci_dev_t device );
    void    pciBridgeApplyConfiguration( pci_dev_t bridge );
    UInt16  pciDisableAccess( pci_dev_t device );
    void    pciRestoreAccess( pci_dev_t device, UInt16 command );
    void    pciBridgeAddChild( pci_dev_t bridge, pci_dev_t child );
    void    pciBridgeProbeChild( pci_dev_t bridge, IOPCIAddressSpace space );
    void    pciProbeBaseAddressRegister( pci_dev_t device, UInt32 lastBarNum );
    void    pciSafeProbeBaseAddressRegister( pci_dev_t device, UInt32 lastBarNum );
    void    pciDeviceProbeRanges( pci_dev_t device );
    void    pciBridgeProbeRanges( pci_dev_t bridge );
    void    pciYentaProbeRanges( pci_dev_t yenta );
    void    pciBridgeProbeBusRange( pci_dev_t bridge );
    void    pciCheckCacheLineSize( pci_dev_t device );
    void    pciWriteLatencyTimer( pci_dev_t device );
    void    pciBridgeConnectDeviceTree( pci_dev_t bridge );
    void    pciBridgeConstructDeviceTree( pci_dev_t bridge );
    OSDictionary * constructProperties( pci_dev_t device );
    void           constructAddressingProperties( pci_dev_t device, OSDictionary * propTable );

public:

    virtual bool    start( IOService * provider );
    virtual void    free( void );

    virtual bool    createRoot( IOService * provider );

    virtual UInt32  configRead32( IOPCIAddressSpace space, UInt32 offset );
    virtual UInt16  configRead16( IOPCIAddressSpace space, UInt32 offset );
    virtual UInt8   configRead8(  IOPCIAddressSpace space, UInt32 offset );
    virtual void    configWrite32( IOPCIAddressSpace space,
                           UInt32 offset, UInt32 data );
    virtual void    configWrite16( IOPCIAddressSpace space,
                           UInt32 offset, UInt16 data );
    virtual void    configWrite8(  IOPCIAddressSpace space,
                           UInt32 offset, UInt8 data );
};


#endif /* !_IOPCICONFIGURATOR_H */
