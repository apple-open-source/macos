/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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


#ifndef _IOKIT_IOPCIPRIVATE_H
#define _IOKIT_IOPCIPRIVATE_H

#if defined(KERNEL)

#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IORangeAllocator.h>
#include <IOKit/IOInterruptController.h>
#include <libkern/OSDebug.h>

#if !defined(__ppc__)
#define USE_IOPCICONFIGURATOR   1
#define USE_MSI                 1
#define USE_LEGACYINTS          1
#endif

#if defined(__i386__) || defined(__x86_64__)
#define ACPI_SUPPORT            1
#endif

struct IOPCIDeviceExpansionData
{
    uint16_t powerCapability;
    uint8_t  pmSleepEnabled;     // T if a client has enabled PCI Power Management
    uint8_t  pmControlStatus;    // if >0 this device supports PCI Power Management
    uint16_t sleepControlBits;   // bits to set the control/status register to for sleep
    uint16_t pmLastWakeBits;     // bits read on wake

    uint16_t expressCapability;
    uint16_t expressCapabilities;
    uint16_t expressASPMDefault;
	uint8_t  aspmCaps;
    uint16_t l1pmCapability;
    uint32_t  l1pmCaps;

    uint16_t msiCapability;
    UInt8    msiBlockSize;
    UInt8    msiMode;
    UInt8    msiEnable;

    uint16_t latencyToleranceCapability;

    uint8_t  configProt;
    uint8_t  pmActive;
    uint8_t  pmeUpdate;
    uint8_t  pmWait;
    uint8_t  pmState;
	uint8_t  pciPMState;
    uint8_t  pauseFlags;
    uint8_t  needsProbe;
    uint8_t  dead;

	IOLock * lock;
    struct IOPCIConfigEntry * configEntry;

	IOPCIDevice * ltrDevice;
	IOByteCount   ltrOffset;
	uint32_t      ltrReg1;
	uint8_t       ltrReg2;

#if ACPI_SUPPORT
	int8_t        psMethods[kIOPCIDevicePowerStateCount];
	int8_t        lastPSMethod;
#endif
};

#define expressV2(device) ((15 & device->reserved->expressCapabilities) > 1)

enum
{
    kIOPCIConfigShadowXPressCount = 6,
    kIOPCIConfigShadowMSICount    = 6,
    kIOPCIConfigShadowL1PMCount   = 2,
    kIOPCIConfigShadowLTRCount    = 1,
    kIOPCIConfigShadowRegs        = 32,
    kIOPCIConfigEPShadowRegs      = 16,
    kIOPCIConfigBridgeShadowRegs  = 32,

    kIOPCIConfigShadowSize        = kIOPCIConfigShadowRegs 
    								+ kIOPCIConfigShadowXPressCount 
    								+ kIOPCIConfigShadowMSICount
    								+ kIOPCIConfigShadowL1PMCount
    								+ kIOPCIConfigShadowLTRCount,
    kIOPCIConfigShadowXPress      = kIOPCIConfigShadowSize   - kIOPCIConfigShadowXPressCount,
    kIOPCIConfigShadowMSI         = kIOPCIConfigShadowXPress - kIOPCIConfigShadowMSICount,
    kIOPCIConfigShadowL1PM        = kIOPCIConfigShadowMSI    - kIOPCIConfigShadowL1PMCount,
    kIOPCIConfigShadowLTR         = kIOPCIConfigShadowL1PM   - kIOPCIConfigShadowLTRCount,

#if 0
    kIOPCIVolatileRegsMask        = ((1 << kIOPCIConfigShadowRegs) - 1)
                                   & ~(1 << (kIOPCIConfigVendorID >> 2))
                                  & ~(1 << (kIOPCIConfigRevisionID >> 2))
                                  & ~(1 << (kIOPCIConfigSubSystemVendorID >> 2)),

    kIOPCISaveRegsMask          = kIOPCIVolatileRegsMask 
                                | (1 << (kIOPCIConfigVendorID >> 2))

#else
    kIOPCISaveRegsMask            = 0xFFFFFFFF
//                                  & ~(1 << (kIOPCIConfigVendorID >> 2))
#endif
};


struct IOPCIConfigShadow
{
    UInt32                   savedConfig[kIOPCIConfigShadowSize];
    UInt32                   flags;
    queue_chain_t            link;
	queue_head_t             dependents;
	IOPCIDevice *			 tunnelRoot;
    IOPCIDevice *            device;
    IOPCI2PCIBridge *        bridge;
    OSObject *               tunnelID;
    IOPCIDeviceConfigHandler handler;
    void *                   handlerRef;
};

#define configShadow(device)    ((IOPCIConfigShadow *) &device->savedConfig[0])


// flags in kIOPCIConfigShadowFlags
enum
{
    kIOPCIConfigShadowValid            = 0x00000001,
    kIOPCIConfigShadowBridge           = 0x00000002,
    kIOPCIConfigShadowHostBridge       = 0x00000004,
    kIOPCIConfigShadowBridgeDriver     = 0x00000008,
    kIOPCIConfigShadowBridgeInterrupts = 0x00000010,
	kIOPCIConfigShadowSleepLinkDisable = 0x00000020,
	kIOPCIConfigShadowSleepReset       = 0x00000040,
	kIOPCIConfigShadowHotplug          = 0x00000080,
	kIOPCIConfigShadowVolatile         = 0x00000100
};

// whatToDo for setDevicePowerState()
enum
{
    kSaveDeviceState    = 0,
    kRestoreDeviceState = 1,
    kSaveBridgeState    = 2,
    kRestoreBridgeState = 3
};

enum
{
	kMachineRestoreBridges      = 0x00000001,
    kMachineRestoreEarlyDevices = 0x00000002,
	kMachineRestoreDehibernate  = 0x00000004,
	kMachineRestoreTunnels      = 0x00000008,
};

enum 
{
	kIOPCIRestoreDeviceStateEarly = 0x00000001
};

#define PCI_ADDRESS_TUPLE(device)   \
        device->space.s.busNum,     \
        device->space.s.deviceNum,  \
        device->space.s.functionNum


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define kIOPCIEjectableKey        "IOPCIEjectable"
#define kIOPCIHotPlugKey          "IOPCIHotPlug"
#define kIOPCILinkChangeKey       "IOPCILinkChange"
#define kIOPCITunnelLinkChangeKey "IOPCITunnelLinkChange"
#define kIOPCITunnelBootDeferKey  "IOPCITunnelBootDefer"
#define kIOPCIResetKey            "IOPCIReset"
#define kIOPCIOnlineKey           "IOPCIOnline"
#define kIOPCIConfiguredKey       "IOPCIConfigured"
#define kIOPCIResourcedKey        "IOPCIResourced"
#define kIOPCIPMCSStateKey        "IOPCIPMCSState"

#ifndef kACPIDevicePathKey
#define kACPIDevicePathKey             "acpi-path"
#endif

#ifndef kACPIDevicePropertiesKey
#define kACPIDevicePropertiesKey       "device-properties"
#endif

#ifndef kACPIPCILinkChangeKey
#define kACPIPCILinkChangeKey       "pci-supports-link-change"
#endif

#define kIOPCIExpressASPMDefaultKey	"pci-aspm-default"

#define kIOPCIExpressMaxLatencyKey	"pci-max-latency"

enum
{
	kIOPCIExpressASPML0s = 0x00000001,
	kIOPCIExpressASPML1  = 0x00000002
};

#define kIOPCIExpressL1PMControlKey	"pci-l1pm-control"

#define kIOPCIDeviceDiagnosticsClassKey  "IOPCIDeviceDiagnosticsClass"

#ifndef kIODebugArgumentsKey
#define kIODebugArgumentsKey	 "IODebugArguments"
#endif

#ifndef kIOMemoryDescriptorOptionsKey
#define kIOMemoryDescriptorOptionsKey	 "IOMemoryDescriptorOptions"
#endif

extern const    IORegistryPlane * gIOPCIACPIPlane;
extern const    OSSymbol *        gIOPlatformDeviceASPMEnableKey;
extern uint32_t                   gIOPCIFlags;
extern const OSSymbol *           gIOPlatformGetMessagedInterruptControllerKey;
extern const OSSymbol *           gIOPlatformGetMessagedInterruptAddressKey;
extern const OSSymbol *           gIOPCIThunderboltKey;
extern const OSSymbol *           gIOPolledInterfaceActiveKey;
#if ACPI_SUPPORT
extern const OSSymbol *           gIOPCIPSMethods[kIOPCIDevicePowerStateCount];
#endif

extern IOReturn IOPCIRegisterPowerDriver(IOService * service, bool hostbridge);

#define arrayCount(x)	(sizeof(x) / sizeof(x[0]))

enum
{
    kMSIX       = 0x01
};


class IOPCIMessagedInterruptController : public IOInterruptController
{
    OSDeclareDefaultStructors( IOPCIMessagedInterruptController )

protected:

    // The base global system interrupt number.

    SInt32                  _vectorBase;
    UInt32                  _vectorCount;
    UInt32                  _parentOffset;
    uint8_t               * _flags;

    IORangeAllocator *      _messagedInterruptsAllocator;

    void enableDeviceMSI(IOPCIDevice *device);
    void disableDeviceMSI(IOPCIDevice *device);

public:
    bool init(UInt32 numVectors, UInt32 baseVector);

    bool init(UInt32 numVectors);

	bool reserveVectors(UInt32 vector, UInt32 count);

    virtual IOReturn registerInterrupt( IOService *        nub,
                                        int                source,
                                        void *             target,
                                        IOInterruptHandler handler,
                                        void *             refCon );

    virtual IOReturn unregisterInterrupt( IOService *      nub,
                                        int                source);

    virtual void     initVector( IOInterruptVectorNumber vectorNumber,
                                 IOInterruptVector * vector );

    virtual int      getVectorType(IOInterruptVectorNumber vectorNumber, IOInterruptVector *vector);

    virtual bool     vectorCanBeShared( IOInterruptVectorNumber vectorNumber,
                                        IOInterruptVector * vector );

    virtual void     enableVector( IOInterruptVectorNumber vectorNumber,
                                   IOInterruptVector * vector );

    virtual void     disableVectorHard( IOInterruptVectorNumber vectorNumber,
                                        IOInterruptVector * vector );

    virtual IOReturn handleInterrupt( void * savedState,
                                      IOService * nub,
                                      int source );

//
    virtual bool     addDeviceInterruptProperties(
                                    IORegistryEntry * device,
                                    UInt32            controllerIndex,
                                    UInt32            interruptFlags,
                                    SInt32 *          deviceIndex);

    IOReturn allocateDeviceInterrupts(
				IOService * entry, uint32_t numVectors, uint32_t msiConfig,
				uint64_t * msiAddress = 0, uint32_t * msiData = 0);
    IOReturn         deallocateDeviceInterrupts(IOService * device);

    virtual void     deallocateInterrupt(UInt32 vector);

protected:
    virtual bool     allocateInterruptVectors( IOService *device,
                                               uint32_t numVectors,
                                               IORangeScalar *rangeStartOut);

};


class IOPCIProxyMessagedInterruptController : public IOPCIMessagedInterruptController
{
    OSDeclareDefaultStructors( IOPCIProxyMessagedInterruptController )

protected:
    IOInterruptController * _parentInterruptController;

public:
    bool             init( UInt32 numVectors, SInt32 parentOffset,
	                   IOInterruptController *parentController );

    virtual IOReturn registerInterrupt( IOService *        nub,
                                        int                source,
                                        void *             target,
                                        IOInterruptHandler handler,
                                        void *             refCon );

    virtual IOReturn unregisterInterrupt( IOService *      nub,
                                        int                source);

    virtual bool     addDeviceInterruptProperties(
                                    IORegistryEntry * device,
                                    UInt32            controllerIndex,
                                    UInt32            interruptFlags,
                                    SInt32 *          deviceIndex);

    virtual void     deallocateInterrupt(UInt32 vector);

    virtual void     initVector( IOInterruptVectorNumber vectorNumber,
                                 IOInterruptVector * vector );

    virtual void     enableVector( IOInterruptVectorNumber vectorNumber,
                                   IOInterruptVector * vector );

    virtual void     disableVectorHard( IOInterruptVectorNumber vectorNumber,
                                        IOInterruptVector * vector );

    virtual IOReturn handleInterrupt( void * savedState,
                                      IOService * nub,
                                      int source );

    virtual IOReturn enableInterrupt(IOService *nub, int source);

    virtual IOReturn disableInterrupt(IOService *nub, int source);
};

#endif /* defined(KERNEL) */

enum
{
    kIOPCIDeviceDiagnosticsClientType = 0x99000001
};

enum
{
    kIOPCIProbeOptionLinkInt      = 0x40000000,
};


#endif /* ! _IOKIT_IOPCIPRIVATE_H */

