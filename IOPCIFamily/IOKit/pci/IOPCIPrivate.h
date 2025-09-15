/*
 * Copyright (c) 2002-2021 Apple Computer, Inc. All rights reserved.
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

#if defined(__i386__) || defined(__x86_64__)
#define ACPI_SUPPORT            1
#else
#define ACPI_SUPPORT            0
#endif

#if !defined(__ppc__)
#define USE_IOPCICONFIGURATOR   1
#define USE_MSI                 1
#define USE_LEGACYINTS          1
#endif

class IOPCIHostBridge;

#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IORangeAllocator.h>
#include <IOKit/IOInterruptController.h>
#include <libkern/OSDebug.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/pci/IOPCIConfigurator.h>
#include <IOKit/IODeviceMemory.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOLib.h>
#include <os/log.h>
#include <IOKit/pci/IOPCITraceEventBuffer.h>

enum
{
	kIOPCIWaitForLinkUpTime	= 1000, // spec compliant link up wait time - unit in ms
	kIOPCIEnumerationWaitTime = (kIOPCIWaitForLinkUpTime - 100), // wait time during enumeration (APCIE already waits 100ms after link up)
};

enum
{
	kIOPCIClassBridge           = 0x06,
	kIOPCIClassNetwork          = 0x02,
	kIOPCIClassGraphics         = 0x03,
	kIOPCIClassMultimedia       = 0x04,

	kIOPCISubClassBridgeHost    = 0x00,
	kIOPCISubClassBridgeISA     = 0x01,
	kIOPCISubClassBridgeEISA    = 0x02,
	kIOPCISubClassBridgeMCA     = 0x03,
	kIOPCISubClassBridgePCI     = 0x04,
	kIOPCISubClassBridgePCMCIA  = 0x05,
	kIOPCISubClassBridgeNuBus   = 0x06,
	kIOPCISubClassBridgeCardBus = 0x07,
	kIOPCISubClassBridgeRaceWay = 0x08,
	kIOPCISubClassBridgeOther   = 0x80,
};

struct IOPCIDeviceExpansionData
{
    uint16_t powerCapability;
    uint8_t  pmSleepEnabled;     // T if a client has enabled PCI Power Management
    uint8_t  pmControlStatus;    // if >0 this device supports PCI Power Management
    uint16_t sleepControlBits;   // bits to set the control/status register to for sleep
    uint16_t pmLastWakeBits;     // bits read on wake

    uint16_t expressCapability;
    uint32_t expressCapabilities;
    uint32_t expressDeviceCapabilities;
    uint16_t expressASPMDefault;
    int16_t  expressMaxReadRequestSize;
	uint16_t aspmCaps;
    uint16_t l1pmCapability;
    uint32_t l1pmCaps;
    uint32_t l1pmTpoweron;
    uint32_t l1pmTcommonmode;

    uint16_t fpbCapability;

    uint16_t aerCapability;

    uint16_t ariCapability;

    bool legacyInterruptResolved;

    uint16_t            msiCapability;
    uint16_t            msiControl;
	uint16_t            msiPhysVectorCount;
	uint16_t            msiVectorCount;
    uint8_t             msiMode;
    uint8_t             msiEnable;
	uint64_t            msiTable;
	uint64_t            msiPBA;
	IOInterruptVector * msiVectors;

    uint16_t latencyToleranceCapability;
    uint16_t acsCapability;
    uint16_t acsCaps;

    uint16_t ptmCapability;

    uint8_t  headerType;
    uint8_t  rootPort;

    uint8_t  configProt;
    uint8_t  pmActive;
    uint8_t  pmeUpdate;
    uint8_t  updateWakeReason;
    uint8_t  pmWait;
    uint8_t  pmState;
	uint8_t  pciPMState;
    uint8_t  pauseFlags;
    uint8_t  needsProbe;
    uint8_t  dead;
    uint8_t  pmHibernated;

	IORecursiveLock * lock;
    struct IOPCIConfigEntry * configEntry;
    IOPCIHostBridge *hostBridge;

	IOOptionBits sessionOptions;

	IOPCIDevice * ltrDevice;
	IOByteCount   ltrOffset;
	uint32_t      ltrReg1;
	uint8_t       ltrReg2;

    uint8_t       tunnelL1Allow;
    uint8_t       offloadEngineMMIODisable;
#if ACPI_SUPPORT
	int8_t        psMethods[kIOPCIDevicePowerStateCount];
	int8_t        lastPSMethod;
#endif

	IODeviceMemory* deviceMemory[kIOPCIRangeExpansionROM + 1];
	IOMemoryMap*    deviceMemoryMap[kIOPCIRangeExpansionROM + 1];

	uint8_t       interruptVectorsResolved;

	IOPCIDeviceCrashNotification_t crashNotification;
	void *crashNotificationRef;

	bool inReset;

	uint32_t probeTimeMS;

    bool isMFD;

	bool hardwareResetNeeded;
	bool clientCrashed;

	OSSerializer *linkStatusSerializer;

	uint8_t linkDepth;
	uint64_t linkUpTimestamp;

	uint32_t domainId;

	OSDictionary *capDict;

    IOTimerEventSource *republishTimer;
	uint8_t publishRetryCount;

	IOTimerEventSource *pauseTimer;
	IONotifier *busyNotifier;
	AbsoluteTime busyTimestamp;

	IOPCIEventSource *pciEventSource;
	IOWorkLoop *pciEventSourceWorkLoop;
	IOCommandGate *pciEventSourceCmdGate;
	uint32_t iommuEventCount;
};

enum
{
    kTunnelL1Disable = false,
    kTunnelL1Enable  = true,
    kTunnelL1NotSet  = 2
};

#define expressV2(device) ((15 & device->reserved->expressCapabilities) > 1)

enum
{
    kIOPCIConfigShadowRegs        = 32,
    kIOPCIConfigEPShadowRegs      = 16,
    kIOPCIConfigBridgeShadowRegs  = 32,
    kIOPCIExpressConfigRegs       = 1024,

    kIOPCIConfigShadowSize        = kIOPCIExpressConfigRegs,

    kIOPCISaveRegsMask            = 0xFFFFFFFF
//                                  & ~(1 << (kIOPCIConfigVendorID >> 2))
};

struct IOPCIConfigSave
{
    uint32_t                 savedConfig[kIOPCIConfigShadowSize];
};

// -- Helper functions for accessing the savedConfig DW array --
static inline uint8_t savedConfigRead8( IOPCIConfigSave *save, uint16_t offset)
{
	uint8_t *savedConfig = reinterpret_cast<uint8_t*>(save->savedConfig);
	return *(uint8_t *)(&savedConfig[offset]);
}

static inline uint16_t savedConfigRead16( IOPCIConfigSave *save, uint16_t offset)
{
	uint8_t *savedConfig = reinterpret_cast<uint8_t*>(save->savedConfig);
	return *(uint16_t *)(&savedConfig[offset]);
}

static inline uint32_t savedConfigRead32( IOPCIConfigSave *save, uint16_t offset)
{
	uint8_t *savedConfig = reinterpret_cast<uint8_t*>(save->savedConfig);
	return *(uint32_t *)(&savedConfig[offset]);
}

static inline void savedConfigWrite16( IOPCIConfigSave *save, uint16_t offset, uint16_t data)
{
	uint8_t *savedConfig = reinterpret_cast<uint8_t*>(save->savedConfig);
	*(uint16_t *)(&savedConfig[offset]) = data;
}

static inline void savedConfigWrite32( IOPCIConfigSave *save, uint16_t offset, uint32_t data)
{
	uint8_t *savedConfig = reinterpret_cast<uint8_t*>(save->savedConfig);
	*(uint32_t *)(&savedConfig[offset]) = data;
}

struct IOPCIMSISave
{
	uint32_t				 address0;
	uint32_t				 address1;
	uint32_t				 data;
	uint16_t				 control;
	uint32_t				 enable;
};

struct IOPCIConfigShadow
{
    IOPCIConfigSave          configSave;
    IOPCIMSISave             msiSave;
    uint32_t                 flags;
	uint8_t                  tunnelled;
	uint8_t                  hpType;
    queue_chain_t            link;
    queue_chain_t            linkFinish;
	queue_head_t             dependents;
	IOLock      *            dependentsLock;
	IOPCIDevice *			 tunnelRoot;
	IOPCIDevice *			 sharedRoot;
    IOPCIDevice *            device;
    IOPCI2PCIBridge *        bridge;
    OSObject *               tunnelID;
    IOPCIDeviceConfigHandler handler;
    void *                   handlerRef;
    IOOptionBits             sharedRootASPMState;
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
	kIOPCIConfigShadowVolatile         = 0x00000100,
	kIOPCIConfigShadowWakeL1PMDisable  = 0x00000200,
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
#define kIOPCIHPTypeKey           "IOPCIHPType"
#define kIOPCIMSIFlagsKey         "pci-msi-flags"
#define kIOPCIMSILimitKey         "pci-msi-limit"
#define kIOPCIIgnoreLinkStatusKey "pci-ignore-linkstatus"

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

#define kIOPCIExpressMaxLatencyKey    "pci-max-latency"
#define kIOPCIExpressMaxPayloadSize   "pci-max-payload-size"
#define kIOPCIExpressEndpointMaxReadRequestSize   "pci-ep-max-read-request-size"

#define kIOPCIExpressErrorUncorrectableMaskKey	    "pci-aer-uncorrectable"
#define kIOPCIExpressErrorUncorrectableSeverityKey	"pci-aer-uncorrectable-severity"
#define kIOPCIExpressErrorCorrectableMaskKey	    "pci-aer-correctable"
#define kIOPCIExpressErrorControlKey	            "pci-aer-control"

// property to disable LTR on wake
#define kIOPMPCIWakeL1PMDisableKey      "pci-wake-l1pm-disable"

#define kIOPCIFunctionsDependentKey     "pci-functions-dependent"

#define kIOPCIBridgeSplayMask "pci-splay-mask"

#define kIOPCISlotCommandCompleted "IOPCISlotCommandCompleted"
#define kIOPCISlotPowerController  "IOPCISlotPowerController"
#define kIOPCISlotDevicePresent    "IOPCISlotDevicePresent"

#define kIOPCIPowerOnProbeKey      "IOPCIPowerOnProbe"

#define kIOPCISlotMaxLinkWidthKey            "slot-max-link-width"
#define kIOPCIExpressMaxLinkWidthOverrideKey "IOPCIExpressMaxLinkWidthOverride"

#define kIOCLxEnabledKey           "CLx Enabled"

#define kIOPCISkipRematchReset "pci-skip-rematch-reset"

#define kIOPCIGetDARTError "pci-get-dart-error"

// PCI Express Capabilities Structure Mask (sec 7.5.3)
enum
{
// PCI Express Capability List Register (Offset 00h)
	kPCIECapCapabilityID            = (0xff),
	kPCIECapNextCapabilityPointer   = (0xff << 8),

// PCI Express Capabilities Register (Offset 02h)
	kPCIECapCapabilityVersion       = 0xf,
	kPCIECapDevicePortType          = (0xf << 4),
	kPCIECapSlotConnected           = (1 << 8),
	kPCIECapInterruptMessageNumber  = (0x1f << 9)
};

// value of kPCIECapDevicePortType
enum
{
	kPCIEPortTypePCIEEndpoint               = (0 << 4),
	kPCIEPortTypeLegacyPCIEEndpoint         = (1 << 4),
	kPCIEPortTypeRCiEP                      = (9 << 4),
	kPCIEPortTypeRCEventCollector           = (0xa << 4),
	kPCIEPortTypePCIERootPort               = (4 << 4),
	kPCIEPortTypeUpstreamPCIESwitch         = (5 << 4),
	kPCIEPortTypeDownstreamPCIESwitch       = (6 << 4),
	kPCIEPortTypePCIEtoPCIBridge            = (7 << 4),
	kPCIEPortTypePCIBridgetoPCIE            = (8 << 4)
};

enum
{
	kLinkCapDataLinkLayerActiveReportingCapable = (1 << 20),
	kLinkStatusLinkTraining						= (1 << 11),
	kLinkStatusDataLinkLayerLinkActive 			= (1 << 13),
	kSlotCapHotplug					 			= (1 << 6),
	kSlotCapPowerController			 			= (1 << 1),
	kSlotCapNoCommandCompleted		 			= (1 << 18)
};

enum
{
	kIOPCIExpressASPML0s   = 0x00000001,
	kIOPCIExpressASPML1    = 0x00000002,
	kIOPCIExpressCommonClk = 0x00000040,
	kIOPCIExpressClkReq    = 0x00000100
};

enum
{
    kIOPCIMSIFlagRespect = 0x00000001,
};

enum
{
    kIOPCIExpressACSSourceValidation            = (1 << 0),
    kIOPCIExpressACSTranslationBlocking         = (1 << 1),
    kIOPCIExpressACSP2PRequestRedirect          = (1 << 2),
    kIOPCIExpressACSP2PCompletionRedirect       = (1 << 3),
    kIOPCIExpressACSP2PUpstreamForwarding       = (1 << 4),
    kIOPCIExpressACSP2PEgressControl            = (1 << 5),
    kIOPCIExpressACSDirectTranslatedP2PEnable   = (1 << 6)
};
#define kIOPCIExpressACSDefault (kIOPCIExpressACSSourceValidation | kIOPCIExpressACSTranslationBlocking)

// The L1PM control key is used to set the L1 PM Substates Control 1 and Control 2 registers.
#define kIOPCIExpressL1PMControlKey	"pci-l1pm-control"
// The L1SS enable key is used to set which L1 PM substates are enabled as well as
// the LTR L1.2 Threshold. The Tcommonmode and Tpoweron timing parameters are calculated
// by software according to the link partners' L1 PM capabilities.
#define kIOPCIExpressL1SSEnableKey	"pci-l1ss-enable"

#define kIOPCIDeviceHiddenKey       "pci-device-hidden"

#ifndef kIODebugArgumentsKey
#define kIODebugArgumentsKey	 "IODebugArguments"
#endif

#ifndef kIOMemoryDescriptorOptionsKey
#define kIOMemoryDescriptorOptionsKey	 "IOMemoryDescriptorOptions"
#endif

#define kIOPCIDeviceChangedKey			"IOPCIDeviceChanged"

#define kIOPCILinkUpTimeoutKey			"link-up-timeout"

#define kIOPCIDARTErrorDataKey			"pci-dart-error-data"

#define kIOPCIDeviceDeadOnRestoreKey	"IOPCIDeviceDeadOnRestore"

#define kIOPCIPublishRetryTimeoutKey	"IOPCIDevicePublishRetryTimeout"

// Entitlements
#define kIOPCITransportDextEntitlement                     "com.apple.developer.driverkit.transport.pci"
#define kIOPCITransportBridgeDextEntitlement               "com.apple.developer.driverkit.transport.pci.bridge"
#define kIOPCITransportDextEntitlementOffloadEngineDisable "com.apple.developer.driverkit.transport.pci.offloadEngineDisable"
extern const    IORegistryPlane * gIOPCIACPIPlane;
extern const    OSSymbol *        gIOPlatformDeviceASPMEnableKey;
extern uint32_t                   gIOPCIFlags;
extern uint32_t                   gIOPCILogFlags;
extern uint32_t                   gIOPCILogModeFlags;
extern uint32_t                   gIOPCILogDomains;
extern const OSSymbol *           gIOPlatformGetMessagedInterruptControllerKey;
extern const OSSymbol *           gIOPlatformGetMessagedInterruptAddressKey;
extern const OSSymbol *           gIOPCIThunderboltKey;
extern const OSSymbol *           gIOPCIHotplugCapableKey;
extern const OSSymbol *           gIOPCITunnelledKey;
extern const OSSymbol *           gIOPCIHPTypeKey;
extern const OSSymbol *           gIOPCIDeviceHiddenKey;

extern const OSSymbol *           gIOPolledInterfaceActiveKey;
#if ACPI_SUPPORT
extern const OSSymbol *           gIOPCIPSMethods[kIOPCIDevicePowerStateCount];
#endif
extern const OSSymbol *           gIOPCIExpressLinkStatusKey;
extern const OSSymbol *           gIOPCIDARTErrorData;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#if ACPI_SUPPORT
void InitSharedBridgeData(void);
__exported_push
extern IOReturn IOPCIPlatformInitialize(void);
extern IOReturn IOPCISetMSIInterrupt(uint32_t vector, uint32_t count, uint32_t * msiData);
extern uint64_t IOPCISetAPICInterrupt(uint64_t entry);
__exported_pop
#endif

extern IOReturn IOPCIRegisterPowerDriver(IOService * service, bool hostbridge);
extern IOService * IOPCIDeviceDMAOriginator(IOPCIDevice * device);

IOReturn parseDevASPMDefaultBootArg(uint32_t vidDid, uint16_t *expressASPMDefault);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define arrayCount(x)	(sizeof(x) / sizeof(x[0]))

enum
{
    kMSIX       = 0x01
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

//
// pci_log_mode= flags (if unspecified, defaults to os_log())
//

#define kPCI_LOG_MODE_KPRINTF	(0x00000001)
#define kPCI_LOG_MODE_IOLOG		(0x00000002)
#define kPCI_LOG_MODE_OSLOG		(0x00000004)

//
// pci_log= flags (if unspecified, defaults to ALWAYS_ON | AER)
//

/* Logs related to AER handling (e.g. reporter, type, status). */
#define kPCI_LOG_AER			(0x00000001)

/* Logs related to software and local (on-die) hardware initialization */
#define kPCI_LOG_INIT			(0x00000002)

/* Logs related to software (e.g. IOPM callouts) and hardware (e.g. D-state
 * configuration) power management and PCIe reset.
 */
#define kPCI_LOG_POWER			(0x00000004)

/* Logs of every local (on-die) register access. */
#define kPCI_LOG_REG			(0x00000008)

/* Logs of every config-space access. */
#define kPCI_LOG_CONFIG			(0x00000010)

/* Logs related to driver state transitions. */
#define kPCI_LOG_STATE			(0x00000020)

/* Logs related to interrupt configuration. */
#define kPCI_LOG_INTERRUPT		(0x00000040)

/* Logs related to tunable application. */
#define kPCI_LOG_TUNABLE		(0x00000080)

/* Logs related to DART configuration. */
#define kPCI_LOG_DART			(0x00000100)

/* Logs related to PCIe enumeration (hierarchy scan, bus number and memory
 * space assignments, and device configuration) and link training.
 */
#define kPCI_LOG_ENUMERATION	(0x00000200)

/* Logs related to IOService object life cycle
 * (init()/attach()/probe()/.../stop()/detach()/free()).
 */
#define kPCI_LOG_LIFE_CYCLE  	(0x00000400)

/* Always-on logs. */
#define kPCI_LOG_ALWAYS_ON		(0x80000000)

//
// pci_log_rc=<bitmap> (if unspecified, all RCs selected)
//
// See https://confluence.sd.apple.com/display/USBSW/PCIe+Boot-Args
// for the mapping from root complex to domain ID.
//
// E.g. enable logging on only apciec0: pci_log_rc=0x2

#define pci_log(domain, facility, fmt, args...)												\
	do {																					\
		if (   (gIOPCILogDomains & (1 << domain)) 											\
			&& (gIOPCILogFlags & (kPCI_LOG_ ## facility))) {								\
			uint64_t __now_ns, __now = mach_continuous_time();								\
			absolutetime_to_nanoseconds(__now, &__now_ns);									\
																							\
			if (gIOPCILogModeFlags & kPCI_LOG_MODE_KPRINTF)									\
			{																				\
				kprintf("[PCIe:%u %llu ns] " fmt, domain, __now_ns, ##args);	 			\
			}		 	 																	\
			if (   (gIOPCILogModeFlags & kPCI_LOG_MODE_IOLOG)								\
				&& ml_get_interrupts_enabled()) 											\
			{																				\
				IOLog("[PCIe:%u %llu ns] " fmt, domain, __now_ns, ##args);					\
			}																				\
			if (gIOPCILogModeFlags & kPCI_LOG_MODE_OSLOG) 									\
			{																				\
				os_log(OS_LOG_DEFAULT, "[PCIe:%u %llu ns] " fmt, domain, __now_ns, ##args);	\
			}																				\
		}																					\
	} while(0)																				\

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

__exported_push
class __kpi_unavailable IOPCIMessagedInterruptController : public IOInterruptController
{
    OSDeclareDefaultStructors( IOPCIMessagedInterruptController )

protected:

    // The base global system interrupt number.

    SInt32                  _vectorBase;
    UInt32                  _vectorCount;
    UInt32                  _parentOffset;

    IORangeAllocator *      _messagedInterruptsAllocator;

public:

  virtual IOReturn registerInterrupt(IOService *nub, int source,
				     void *target,
				     IOInterruptHandler handler,
				     void *refCon);
  virtual IOReturn unregisterInterrupt(IOService *nub, int source);
  
  virtual IOReturn getInterruptType(IOService *nub, int source,
				    int *interruptType);
  
  virtual IOReturn enableInterrupt(IOService *nub, int source);
  virtual IOReturn disableInterrupt(IOService *nub, int source);
  
  virtual IOReturn handleInterrupt(void *refCon, IOService *nub,
				   int source);

public:

	static IOInterruptVector * allocVectors(uint32_t count);
    static void initDevice(IOPCIDevice * device, IOPCIMSISave * save);
	static void saveDeviceState(IOPCIDevice * device, IOPCIMSISave * save);
	static void restoreDeviceState(IOPCIDevice * device, IOPCIMSISave * save);

    void enableDeviceMSI(IOPCIDevice *device);
    void disableDeviceMSI(IOPCIDevice *device);

    bool init(UInt32 numVectors, UInt32 baseVector, uint32_t domainId = 0);

    bool init(UInt32 numVectors);

	bool reserveVectors(UInt32 vector, UInt32 count);

    virtual void     initVector( IOInterruptVectorNumber vectorNumber,
                                 IOInterruptVector * vector );

    virtual int      getVectorType(IOInterruptVectorNumber vectorNumber, IOInterruptVector *vector);

    virtual bool     vectorCanBeShared( IOInterruptVectorNumber vectorNumber,
                                        IOInterruptVector * vector );

    virtual void     enableVector( IOInterruptVectorNumber vectorNumber,
                                   IOInterruptVector * vector );

    virtual void     disableVectorHard( IOInterruptVectorNumber vectorNumber,
                                        IOInterruptVector * vector );

    virtual bool     addDeviceInterruptProperties(
                                    IORegistryEntry * device,
                                    UInt32            controllerIndex,
                                    UInt32            interruptFlags,
                                    SInt32 *          deviceIndex);

    IOReturn allocateDeviceInterrupts(
				IOService * entry, uint32_t numVectors, uint32_t msiConfig,
				uint64_t * msiAddress = 0, uint32_t * msiData = 0,
				uint32_t numRequired = 0, uint32_t numRequested = 0);
    IOReturn         deallocateDeviceInterrupts(IOService * device);

    virtual void     deallocateInterrupt(UInt32 vector);

    virtual uint32_t getDeviceMSILimit(IOPCIDevice* device, uint32_t numVectorsRequested);
protected:
    virtual bool     allocateInterruptVectors( IOService *device,
                                               uint32_t numVectors,
                                               IORangeScalar *rangeStartOut);
private:
	uint32_t _domainId;
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class IOPCIDiagnosticsClient : public IOUserClient
{
    OSDeclareDefaultStructors(IOPCIDiagnosticsClient)

    friend class IOPCIBridge;

    IOPCIBridge * owner;

public:
    virtual bool initWithTask(task_t owningTask,
							  void * securityID,
							  UInt32 type,
							  OSDictionary * properties);
    virtual IOReturn    clientClose(void);
    virtual IOService * getService(void);
    virtual IOReturn    setProperties(OSObject * properties);
    virtual IOReturn    externalMethod(uint32_t selector, IOExternalMethodArguments * args,
                                       IOExternalMethodDispatch * dispatch, OSObject * target, void * reference);
};
__exported_pop

#endif /* defined(KERNEL) */

enum
{
	kIOPCISessionOptionDriverkit = 0x00010000
};

enum
{
    kIOPCIDiagnosticsClientType = 0x99000001
};

enum
{
    kIOPCIProbeOptionLinkInt      = 0x40000000,
};


enum {
	kIOPCIDiagnosticsMethodRead  = 0,
	kIOPCIDiagnosticsMethodWrite = 1,
	kIOPCIDiagnosticsMethodCount
};

struct IOPCIDiagnosticsParameters
{
	uint32_t			          options;
	uint32_t 		              spaceType;
	uint32_t			          bitWidth;
	uint32_t			          _resv;
	uint64_t			          value;
    union
    {
        uint64_t addr64;
        struct {
            unsigned int offset     :16;
            unsigned int function   :3;
            unsigned int device     :5;
            unsigned int bus        :8;
            unsigned int segment    :16;
            unsigned int reserved   :16;
        } pci;
    }                             address;
};
typedef struct IOPCIDiagnosticsParameters IOPCIDiagnosticsParameters;

enum {
    kIOPCIMapperSelectionSystem             = 0,
    kIOPCIMapperSelectionChanged            = 1 << 0,
    kIOPCIMapperSelectionDeviceBootArg      = 1 << 1,
    kIOPCIMapperSelectionBundleBootArg      = 1 << 2,
    kIOPCIMapperSelectionConfiguratorFlag   = 1 << 3,
    kIOPCIMapperSelectionBundlePlist        = 1 << 4
};

__exported_push
class IOPCIHostBridgeData : public IOService
{
    OSDeclareDefaultStructors(IOPCIHostBridgeData);
public:
    virtual bool init(void) override;
    virtual void free(void) override;

    bool initWithParameters(uint32_t domainId);

    queue_head_t        _allPCIDeviceRestoreQ;
    uint32_t            _tunnelSleep;
    uint32_t            _tunnelWait;

    IOWorkLoop *        _configWorkLoop;
    IOPCIConfigurator * _configurator;

    OSSet *             _waitingPauseSet;
    OSSet *             _pausedSet;
    OSSet *             _probeSet;
    // The set of bridges that have yet to run probeBus. probeBusGated() will
	// join type 0 children to the power plane, and will add type 1 children
    // to the publish set, inside the configurator workloop.
    OSSet *             _publishSet;

    IORecursiveLock *   _eventSourceLock;
    queue_head_t        _eventSourceQueue;

    IOSimpleLock *      _allPCI2PCIBridgesLock;
    uint32_t            _allPCI2PCIBridgeState;
    bool                _isUSBCSystem;
#if ACPI_SUPPORT
    bool                _vtdInterruptsInstalled;
#else
    IOLock *_powerChildrenLock;
    OSSet *_powerChildren;
    OSSet *_activePowerChildren;
#endif

    void lockWakeReasonLock(void);
    void unlockWakeReasonLock(void);

    void tunnelSleepIncrement(const char * deviceName, bool increment);
    void tunnelsWait(IOPCIDevice * device);

    IOReturn addPCIEPowerChild(IOService *theChild);
    IOReturn removePCIEPowerChild(IOPowerConnection *theChild);
    virtual IOReturn powerStateDidChangeTo(IOPMPowerFlags capabilities, unsigned long stateNumber, IOService *whatDevice) override;
private:
    IOLock *           _wakeReasonLock;

    IOReturn finishMachineState(IOOptionBits options);
    static IOReturn systemPowerChange(void * target, void * refCon,
                                      UInt32 messageType, IOService * service,
                                      void * messageArgument, vm_size_t argSize);
public:
    uint16_t           _aspmDefault;
    uint32_t           _l1ssOverride;
	bool               systemActive(void);
	bool               systemWaking(void);
private:
	uint32_t           _systemActive;
	uint32_t           _systemWaking;
	IOTimerEventSource*     _powerAssertionTimer;
	IOPMDriverAssertionID   _powerAssertion;

	void disablePowerAssertion(void);
	void powerAssertionTimeout(IOTimerEventSource* timer);

	IONotifier *_pmrdPowerNotifier;
public:
	void restartPowerAssertionTimer(void);
	bool _supportsOffloadEngine;
	bool _useLinkStatusSerializer;
	uint32_t _domainId;

	IOPCITraceEventBuffer 	_traceEventBuffer;
};

class IOPCIHostBridge : public IOPCIBridge
{
    OSDeclareDefaultStructors(IOPCIHostBridge);
public:
    virtual bool start(IOService *provider) override;
    virtual void free(void) override;

    virtual IOService *probe(IOService * provider, SInt32 *score) override;
    virtual bool configure(IOService * provider) override;

    // Return true if all children are in kIOPCIDeviceOnState.
    bool allChildrenPoweredOn(void);
    // Host bridge data is shared, when bridges have shared resources, i.e. PCIe config space (legacy systems).
    // They must be unique, if bridge has no resources to share (Apple Silicone).
    IOPCIHostBridgeData *bridgeData;
	uint32_t _domainId;
protected:
    virtual IOReturn setLinkSpeed(tIOPCILinkSpeed linkSpeed, bool retrain) override { return kIOReturnUnsupported; };
    virtual IOReturn getLinkSpeed(tIOPCILinkSpeed *linkSpeed) override { return kIOReturnUnsupported; };
private:
	bool childPublished(void* refcon __unused, IOService* newService, IONotifier* notifier __unused);
	IONotifier *_publishNotifier;
};
__exported_pop

#endif /* ! _IOKIT_IOPCIPRIVATE_H */

