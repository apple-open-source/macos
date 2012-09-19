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

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

typedef uint64_t IOPCIScalar;

enum {
    kIOPCIRangeFlagMaximizeSize  = 0x00000001,
    kIOPCIRangeFlagNoCollapse    = 0x00000002,
    kIOPCIRangeFlagSplay         = 0x00000004,
    kIOPCIRangeFlagRelocatable   = 0x00000008,
	kIOPCIRangeFlagMaximizeFlags = kIOPCIRangeFlagMaximizeSize
								  | kIOPCIRangeFlagNoCollapse
};

struct IOPCIRange
{
    IOPCIScalar         start;
    IOPCIScalar         size;
    IOPCIScalar         proposedSize;

    // end marker
    IOPCIScalar         end;
    IOPCIScalar         zero;

    IOPCIScalar         alignment;
    IOPCIScalar         minAddress;
    IOPCIScalar         maxAddress;

    uint32_t            type;
    uint32_t            flags;
    struct IOPCIRange * next;
    struct IOPCIRange * nextSubRange;
    struct IOPCIRange * allocations;
};

IOPCIScalar IOPCIScalarAlign(IOPCIScalar num, IOPCIScalar alignment);
IOPCIScalar IOPCIScalarTrunc(IOPCIScalar num, IOPCIScalar alignment);

IOPCIRange * IOPCIRangeAlloc(void);

void IOPCIRangeFree(IOPCIRange * range);

void IOPCIRangeInit(IOPCIRange * range, uint32_t type,
                  IOPCIScalar start, IOPCIScalar size, IOPCIScalar alignment = 0);

void IOPCIRangeDump(IOPCIRange * head);

bool IOPCIRangeListAddRange(IOPCIRange ** rangeList,
                          uint32_t type,
                          IOPCIScalar start,
                          IOPCIScalar size,
                          IOPCIScalar alignment = 1);

bool IOPCIRangeDeallocateSubRange(IOPCIRange * headRange,
                                IOPCIRange * oldRange);

bool IOPCIRangeListAllocateSubRange(IOPCIRange * headRange,
                                  IOPCIRange * newRange,
                                  IOPCIScalar  newStart = 0);

bool IOPCIRangeListDeallocateSubRange(IOPCIRange * headRange,
                                IOPCIRange * oldRange);

bool IOPCIRangeAppendSubRange(IOPCIRange ** headRange,
                              IOPCIRange * newRange );

IOPCIScalar IOPCIRangeListCollapse(IOPCIRange * headRange, IOPCIScalar alignment);

IOPCIScalar IOPCIRangeCollapse(IOPCIRange * headRange, IOPCIScalar alignment);

IOPCIScalar IOPCIRangeListLastFree(IOPCIRange * headRange, IOPCIScalar align);
IOPCIScalar IOPCIRangeLastFree(IOPCIRange * headRange, IOPCIScalar align);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifdef KERNEL

#include <IOKit/IOLib.h>
#include <IOKit/pci/IOPCIBridge.h>


#define kPCIBridgeIOAlignment        (4096)
#define kPCIBridgeMemoryAlignment    (1024*1024)
#define kPCIBridgeBusNumberAlignment (1)


#define FOREACH_CHILD(bridge, child) \
    for(IOPCIConfigEntry * (child) = (bridge)->child; (child); (child) = (child)->peer)

enum {
    kIOPCIConfiguratorIOLog          = 0x00000001,
    kIOPCIConfiguratorKPrintf        = 0x00000002,
    kIOPCIConfiguratorVTLog          = 0x00000004,
    
    kIOPCIConfiguratorCheckTunnel    = 0x00000008,
    kIOPCIConfiguratorNoTunnelDrv    = 0x00000010,
    kIOPCIConfiguratorNoTerminate    = 0x00000020,

    kIOPCIConfiguratorLogSaveRestore = 0x00000040,
    kIOPCIConfiguratorDeferHotPlug   = 0x00000080,
	kIOPCIConfiguratorPanicOnFault   = 0x00000100, 
    kIOPCIConfiguratorIGIsMapped     = 0x00000200,

    kIOPCIConfiguratorAllocate       = 0x00001000,
    kIOPCIConfiguratorPFM64          = 0x00002000,
    kIOPCIConfiguratorBoot	         = 0x00004000,
    kIOPCIConfiguratorReset          = 0x00010000,

    kIOPCIConfiguratorBootDefer      = kIOPCIConfiguratorDeferHotPlug | kIOPCIConfiguratorBoot,
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

    kIOPCIRangeCount,

	kIOPCIRangeAllMask		  	  = (1 << kIOPCIRangeCount) - 1,
	kIOPCIRangeAllBarsMask		  = (1 << (kIOPCIRangeExpansionROM + 1)) - 1,
	kIOPCIRangeAllBridgeMask	  =  (1 << kIOPCIRangeBridgeMemory)
								   | (1 << kIOPCIRangeBridgePFMemory)
								   | (1 << kIOPCIRangeBridgeIO)
								   | (1 << kIOPCIRangeBridgeBusNumber),
	

};

enum {
//    kPCIDeviceStateResourceAssigned  = 0x00000001,
    kPCIDeviceStatePropertiesDone    = 0x00000002,
    kPCIDeviceStateConfigurationDone = 0x00000008,

    kPCIDeviceStateScanned          = 0x00000010,
    kPCIDeviceStateAllocatedBus     = 0x00000020,
    kPCIDeviceStateAllocated        = 0x00000040,
    kPCIDeviceStateNoLink           = 0x00000080,

	kPCIDeviceStateConfigProtectShift = 15,
	kPCIDeviceStateConfigRProtect	= (VM_PROT_READ  << kPCIDeviceStateConfigProtectShift),
	kPCIDeviceStateConfigWProtect	= (VM_PROT_WRITE << kPCIDeviceStateConfigProtectShift),

    kPCIDeviceStateDead             = 0x80000000,
    kPCIDeviceStateEjected          = 0x40000000,
    kPCIDeviceStateToKill           = 0x20000000,
};

enum {
    kPCIHeaderType0 = 0,
    kPCIHeaderType1 = 1,
    kPCIHeaderType2 = 2
};

// value of supportsHotPlug
enum {
    kPCIStatic                  = 0,
    kPCILinkChange              = 1,
    kPCIHotPlug                 = 2,
    kPCIHotPlugRoot             = 3,
    kPCIHotPlugTunnel           = 4,
    kPCIHotPlugTunnelRoot       = 5,
    kPCIHotPlugTunnelRootParent = 6,
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

enum 
{
    kConfigOpAddHostBridge = 1,
    kConfigOpScan,
    kConfigOpGetState,
    kConfigOpNeedsScan,
    kConfigOpEject,
    kConfigOpKill,
    kConfigOpTerminated,
    kConfigOpProtect
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

struct IOPCIConfigEntry
{
    IOPCIConfigEntry *  parent;
    IOPCIConfigEntry *  child;
    IOPCIConfigEntry *  peer;
    uint32_t            classCode;
    IOPCIAddressSpace   space;
    uint32_t            vendorProduct;

	uint32_t			expressCapBlock;
	uint32_t			expressDeviceCaps1;

    IOPCIRange *        ranges[kIOPCIRangeCount];
    IOPCIRange          busResv;
    uint32_t            rangeBaseChanges;
    uint32_t            rangeSizeChanges;
    uint32_t            rangeRequestChanges;

    uint32_t            deviceState;
    uint8_t             iterator;

    uint8_t             headerType;
    uint8_t             isBridge;
    uint8_t             isHostBridge;
    uint8_t             supportsHotPlug;
    uint8_t				linkInterrupts;
    uint8_t             clean64;
    uint8_t             secBusNum;  // bridge only
    uint8_t             subBusNum;  // bridge only

	uint32_t			linkCaps;
	uint16_t			expressCaps;
    uint8_t   			expressMaxPayload;
    uint8_t   			expressPayloadSetting;

    IORegistryEntry *   dtNub;
#if ACPI_SUPPORT
    IORegistryEntry *   acpiDevice;
#endif
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class IOPCIConfigurator : public IOService
{
    friend class IOPCIBridge;
    OSDeclareDefaultStructors( IOPCIConfigurator );

    IOWorkLoop *            fWL;
    IOOptionBits            fFlags;
    IOPCIBridge *           fHostBridge;
    IOPCIConfigEntry *      fRoot;
	uint32_t				fRootVendorProduct;

	uint8_t					fMaxPayload;

    IOPCIRange *            fConsoleRange;
    IOPCIScalar             fPFMConsole;

    OSSet *                 fChangedServices;

    int                     fBridgeCount;
    int                     fDeviceCount;

    int                     fBridgeConfigCount;
    int                     fDeviceConfigCount;
    int                     fCardBusConfigCount;

protected:

    static void safeProbeCallback( void * refcon );
    static void configProbeCallback( void * refcon );

    static void matchDTEntry( IORegistryEntry * dtEntry, void * _context );
#if ACPI_SUPPORT
    static void matchACPIEntry( IORegistryEntry * dtEntry, void * _context );
	void        removeFixedRanges(IORegistryEntry * root);
#endif

    typedef bool (IOPCIConfigurator::*IterateProc)(void * ref, IOPCIConfigEntry * bridge);
    void    iterate(uint32_t options, 
                    IterateProc topProc, IterateProc bottomProc, 
                    void * ref = NULL);

    bool    scanProc(void * ref, IOPCIConfigEntry * bridge);
    bool    totalProc(void * ref, IOPCIConfigEntry * bridge);
    bool    allocateProc(void * ref, IOPCIConfigEntry * bridge);

    void    configure(uint32_t options);
    void    bridgeScanBus(IOPCIConfigEntry * bridge, uint8_t busNum);

    IOPCIRange * bridgeGetRange(IOPCIConfigEntry * bridge, uint32_t type);
    bool    bridgeTotalResources(IOPCIConfigEntry * bridge, uint32_t typeMask);
    bool    bridgeAllocateResources( IOPCIConfigEntry * bridge, uint32_t typeMask );

	bool    bridgeDeallocateChildRanges(IOPCIConfigEntry * bridge, IOPCIConfigEntry * dead,
								        uint32_t deallocTypes, uint32_t freeTypes);

    void    doConfigure(uint32_t options);

    void    applyConfiguration(IOPCIConfigEntry * device, uint32_t typeMask);
    void    deviceApplyConfiguration(IOPCIConfigEntry * device, uint32_t typeMask);
    void    bridgeApplyConfiguration(IOPCIConfigEntry * bridge, uint32_t typeMask);
    uint16_t disableAccess(IOPCIConfigEntry * device, bool disable);
    void    restoreAccess(IOPCIConfigEntry * device, UInt16 command);
    void    bridgeAddChild(IOPCIConfigEntry * bridge, IOPCIConfigEntry * child);
    void    bridgeRemoveChild(IOPCIConfigEntry * bridge, IOPCIConfigEntry * dead,
								uint32_t deallocTypes, uint32_t freeTypes,
								IOPCIConfigEntry ** childList);
	void    bridgeMoveChildren(IOPCIConfigEntry * to, IOPCIConfigEntry * list);
    void    bridgeDeadChild(IOPCIConfigEntry * bridge, IOPCIConfigEntry * dead);
    void    bridgeProbeChild(IOPCIConfigEntry * bridge, IOPCIAddressSpace space);
    void    probeBaseAddressRegister(IOPCIConfigEntry * device, uint32_t lastBarNum, uint8_t reset);
    void    safeProbeBaseAddressRegister(IOPCIConfigEntry * device, uint32_t lastBarNum, uint8_t reset);
    void    deviceProbeRanges(IOPCIConfigEntry * device, uint8_t reset);
    void    bridgeProbeRanges(IOPCIConfigEntry * bridge, uint8_t reset);
    void    cardbusProbeRanges(IOPCIConfigEntry * bridge, uint8_t reset);
    void    bridgeProbeBusRange(IOPCIConfigEntry * bridge, uint8_t reset);
	uint32_t findPCICapability(IOPCIConfigEntry * device,
                               uint32_t capabilityID, uint32_t * found);
    void    checkCacheLineSize(IOPCIConfigEntry * device);
    void    writeLatencyTimer(IOPCIConfigEntry * device);
	bool    bridgeFinalizeConfig(void * unused, IOPCIConfigEntry * bridge);
    void    bridgeConnectDeviceTree(IOPCIConfigEntry * bridge);
    bool    bridgeConstructDeviceTree(void * unused, IOPCIConfigEntry * bridge);
    OSDictionary * constructProperties(IOPCIConfigEntry * device);
    void           constructAddressingProperties(IOPCIConfigEntry * device, OSDictionary * propTable);

    bool     createRoot(void);
    IOReturn addHostBridge(IOPCIBridge * hostBridge);

	bool     configAccess(IOPCIConfigEntry * device, bool write);

    uint32_t configRead32( IOPCIAddressSpace space, uint32_t offset);
	void     configWrite32(IOPCIAddressSpace space, uint32_t offset, uint32_t data);
	uint32_t findPCICapability(IOPCIAddressSpace space,
                               uint32_t capabilityID, uint32_t * found);

    uint32_t configRead32( IOPCIConfigEntry * device, uint32_t offset);
    uint16_t configRead16( IOPCIConfigEntry * device, uint32_t offset);
    uint8_t  configRead8(  IOPCIConfigEntry * device, uint32_t offset);
    void     configWrite32(IOPCIConfigEntry * device, uint32_t offset, uint32_t data);
    void     configWrite16(IOPCIConfigEntry * device, uint32_t offset, uint16_t data);
    void     configWrite8( IOPCIConfigEntry * device, uint32_t offset, uint8_t  data);

public:
    bool init(IOWorkLoop * wl, uint32_t flags);
    virtual IOWorkLoop * getWorkLoop() const;
    virtual void     free(void);

    IOReturn configOp(IOService * device, uintptr_t op, void * result);
};

#endif /* KERNEL */

#endif /* !_IOPCICONFIGURATOR_H */
