/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include <IOKit/IOService.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/IONVRAM.h>
#include <libkern/c++/OSObject.h>
#include <kern/clock.h>
#include "AppleSmartBatteryManager.h"
#include "AppleSmartBattery.h"
#include "AppleSmartBatteryKeys.h"


enum {
    kSecondsUntilValidOnWake    = 30,
    kPostChargeWaitSeconds      = 120,
    kPostDischargeWaitSeconds   = 120
};

#define abs(x) (((x)<0)?(-1*(x)):(x))
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define VOIDPTR(arg)  ((void *)(uintptr_t)(arg))

#define SMCKEY2CHARS(key) \
    (fDisplayKeys ? (((key) >> 24) & 0xff) : ' '), (fDisplayKeys ? (((key) >> 16) & 0xff) : ' '),\
            (fDisplayKeys ? (((key) >> 8) & 0xff) : ' '), (fDisplayKeys ? ((key) & 0xff) : ' ')

#define ASSERT_GATED() \
do {  \
    if (fWorkLoop->inGate() != true) {   \
        panic("AppleSmartBattery: not inside workloop gate");  \
    } \
} while(false)


// Argument to transactionCompletion indicating we should start/re-start polling
#define kTransactionRestart     0x0
#define kFinishPolling          0x9999

static const uint32_t kBatteryReadAllTimeout = 10000;       // 10 seconds

#define kErrorPermanentFailure              "Permanent Battery Failure"



#define kIOReportNumberOfReporters  2
#define kReportCategoryBattery (kIOReportCategoryPower | kIOReportCategoryField | kIOReportCategoryPeripheral | kIOReportCategoryDebug)
#define kIOReportBatteryCycleCountID IOREPORT_MAKEID('c', 'y', 'c', 'l', 'e', 'c', 'n', 't')
#define kIOReportBatteryGroupName "Battery"

enum {
    INDUCTIVE_FW_CTRL_CMD_SET_CLOAK               = 0x7,
    INDUCTIVE_FW_CTRL_CMD_DISABLE_DEBUGPOWER      = 0x20,
    INDUCTIVE_FW_CTRL_CMD_DISABLE_DISP_COEX       = 0x25,
    INDUCTIVE_FW_CTRL_CMD_SET_DEMO_MODE           = 0x28,
};

// Keys we use to publish battery state in our IOPMPowerSource::properties array
// TODO: All of these would need to exist on iOS/gOS?
static const OSSymbol *_MaxErrSym               = OSSymbol::withCString(kIOPMPSMaxErrKey);
static const OSSymbol *_DeviceNameSym           = OSSymbol::withCString(kIOPMDeviceNameKey);
static const OSSymbol *_FullyChargedSym         = OSSymbol::withCString(kIOPMFullyChargedKey);
static const OSSymbol *_AvgTimeToEmptySym       = OSSymbol::withCString("AvgTimeToEmpty");
static const OSSymbol *_InstantTimeToEmptySym   = OSSymbol::withCString("InstantTimeToEmpty");
static const OSSymbol *_AmperageSym             = OSSymbol::withCStringNoCopy(kIOPMPSAmperageKey);
static const OSSymbol *_VoltageSym              = OSSymbol::withCStringNoCopy(kIOPMPSVoltageKey);
static const OSSymbol *_InstantAmperageSym      = OSSymbol::withCString("InstantAmperage");
static const OSSymbol *_AvgTimeToFullSym        = OSSymbol::withCString("AvgTimeToFull");
static const OSSymbol *_ManfDateSym             = OSSymbol::withCString(kIOPMPSManufactureDateKey);
static const OSSymbol *_DesignCapacitySym       = OSSymbol::withCString(kIOPMPSDesignCapacityKey);
static const OSSymbol *_TemperatureSym          = OSSymbol::withCString(kIOPMPSBatteryTemperatureKey);
static const OSSymbol *_CellVoltageSym          = OSSymbol::withCString("CellVoltage");
static const OSSymbol *_ManufacturerDataSym     = OSSymbol::withCString("ManufacturerData");
static const OSSymbol *_PFStatusSym             = OSSymbol::withCString("PermanentFailureStatus");
static const OSSymbol *_DesignCycleCount70Sym   = OSSymbol::withCString("DesignCycleCount70");
static const OSSymbol *_DesignCycleCount9CSym   = OSSymbol::withCString("DesignCycleCount9C");
static const OSSymbol *_PackReserveSym          = OSSymbol::withCString("PackReserve");
static const OSSymbol *_OpStatusSym             = OSSymbol::withCString("OperationStatus");
static const OSSymbol *_PermanentFailureSym     = OSSymbol::withCString(kErrorPermanentFailure);
static const OSSymbol *_SerialNumberSym         = OSSymbol::withCString("FirmwareSerialNumber");
static const OSSymbol *_HardwareSerialSym       = OSSymbol::withCString("BatterySerialNumber");
static const OSSymbol *_kChargingCurrent        = OSSymbol::withCString("ChargingCurrent");
static const OSSymbol *_kChargingVoltage        = OSSymbol::withCString("ChargingVoltage");
static const OSSymbol *_kChargerData            = OSSymbol::withCString("ChargerData");
static const OSSymbol *_kNotChargingReason      = OSSymbol::withCString("NotChargingReason");
static const OSSymbol *_kChargerId              = OSSymbol::withCString("ChargerID");
static const OSSymbol *_RawCurrentCapacity      = OSSymbol::withCString("AppleRawCurrentCapacity");
static const OSSymbol *_RawMaxCapacity          = OSSymbol::withCString("AppleRawMaxCapacity");
static const OSSymbol *_kPassedCharge           = OSSymbol::withCString("PassedCharge");
static const OSSymbol *_kBatteryFCCData         = OSSymbol::withCString("BatteryFCCData");
static const OSSymbol *_kResScale               = OSSymbol::withCString("ResScale");
static const OSSymbol *_kDOD0                   = OSSymbol::withCString("DOD0");
static const OSSymbol *_kDOD1                   = OSSymbol::withCString("DOD1");
static const OSSymbol *_kDOD2                   = OSSymbol::withCString("DOD2");
static const OSSymbol *_QmaxCell                = OSSymbol::withCString("QmaxCell0");
static const OSSymbol *_QmaxCell1               = OSSymbol::withCString("QmaxCell1");
static const OSSymbol *_QmaxCell2               = OSSymbol::withCString("QmaxCell2");
static const OSSymbol *_AdapterPower            = OSSymbol::withCString("AdapterPower");
static const OSSymbol *_SystemPower             = OSSymbol::withCString("SystemPower");
static const OSSymbol *_stateOfCharge           = OSSymbol::withCString("StateOfCharge");
static const OSSymbol *_PMUConfigured           = OSSymbol::withCString("PMUConfigured");
static const OSSymbol *_BatteryData             = OSSymbol::withCString("BatteryData");



#define kBootPathKey             "BootPathUpdated"
#define kFullPathKey             "FullPathUpdated"
#define kUserVisPathKey          "UserVisiblePathUpdated"

#define kBatt                   kSMBusBatteryAddr
#define kMgr                    kSMBusManagerAddr



#define super IOPMPowerSource

OSDefineMetaClassAndStructors(AppleSmartBattery,IOPMPowerSource)

/******************************************************************************
 * AppleSmartBattery::smartBattery
 *
 ******************************************************************************/

AppleSmartBattery *
AppleSmartBattery::smartBattery(void)
{
    static int asbm = 0;
    AppleSmartBattery  *me;
    me = new AppleSmartBattery;

    if (asbm || (me && !me->init())) {
        me->release();
    asbm++;
        return NULL;
    }

    return me;
}

/******************************************************************************
 * AppleSmartBattery::init
 *
 ******************************************************************************/

bool AppleSmartBattery::init(void)
{
    if (!super::init()) {
        return false;
    }

    fProvider = NULL;
    fWorkLoop = NULL;

    return true;
}


/******************************************************************************
 * AppleSmartBattery::start
 *
 ******************************************************************************/
bool AppleSmartBattery::start(IOService *provider)
{
    fProvider = OSDynamicCast(AppleSmartBatteryManager, provider);

    if (!fProvider || !super::start(provider)) {
        return false;
    }

    fPollingNow             = false;
    fCancelPolling          = false;
    fPermanentFailure       = false;
    fFullyDischarged        = false;
    fFullyCharged           = false;
    fBatteryPresent         = -1;
    fACConnected            = -1;
    fInflowDisabled         = false;
    fRebootPolling          = false;
    fCellVoltages           = NULL;
    fSystemSleeping         = false;
    fPowerServiceToAck      = NULL;
    fCapacityOverride       = false;

    fIncompleteReadRetries = kIncompleteReadRetryMax;

    initializeCommands();

#if TARGET_OS_OSX
    // Make sure that we read battery state at least 5 times at 30 second intervals
    // after system boot.
    fInitialPollCountdown = kInitialPollCountdown;
    fDisplayKeys  = false;
#else
    fInitialPollCountdown = 0;
    fDisplayKeys = PE_i_can_has_debugger(NULL);
#endif

    fWorkLoop = getWorkLoop();

    fBatteryReadAllTimer = IOTimerEventSource::timerEventSource(this,
                    OSMemberFunctionCast(IOTimerEventSource::Action,
                    this, &AppleSmartBattery::incompleteReadTimeOut));

    if (!fWorkLoop
      || (kIOReturnSuccess != fWorkLoop->addEventSource(fBatteryReadAllTimer)))
    {
        BM_ERRLOG("Failed to start timer event\n");
        return false;
    }



    // Publish the intended period in seconds that our "time remaining"
    // estimate is wildly inaccurate after wake from sleep.
    setProperty(kIOPMPSInvalidWakeSecondsKey, kSecondsUntilValidOnWake, 32);

    // Publish the necessary time period (in seconds) that a battery
    // calibrating tool must wait to allow the battery to settle after
    // charge and after discharge.
    setProperty(kIOPMPSPostChargeWaitSecondsKey, kPostChargeWaitSeconds, 32);
    setProperty(kIOPMPSPostDishargeWaitSecondsKey, kPostDischargeWaitSeconds, 32);

    // zero out battery state with argument (do_update == true)
    clearBatteryState(false);

    // Kick off the 30 second timer and do an initial poll
    // No guarantee SMC is ready at this point
    pollBatteryState(kBoot);

    return true;
}



/******************************************************************************
 * AppleSmartBattery::initializeCommands
 *
 ******************************************************************************/
void AppleSmartBattery::initializeCommands(void)
{
    CommandStruct local_cmd[] =
    {
        // cmd,                    address, opType, smcKey, symbol
        {kTransactionRestart,       0,     kASBMInvalidOp,     0, 0, NULL,                      kUserVis},
        {kChargerDataCmd,           kBatt, kASBMSMCReadDictionary,   0, 0, NULL,                kUserVis},
        {kBatteryFCCDataCmd,        kBatt, kASBMSMCReadDictionary,   0, 0, NULL,                kUserVis},
        {kBatteryDataCmd,           kBatt, kASBMSMCReadDictionary,   0, 0, NULL,                kUserVis},
        {kBVoltageCmd,              kBatt, kASBMSMBUSReadWord, 0, 0, voltageKey,                kUserVis},
        {kBCurrentCmd,              kBatt, kASBMSMBUSReadWord, 0, 0, NULL,                      kUserVis},
        {kBAverageCurrentCmd,       kBatt, kASBMSMBUSReadWord, 0, 0, NULL,                      kUserVis},
        {kAdapterStatus,            kBatt, kASBMSMCReadDictionary,   0, 0, NULL,                kFull},
#if TARGET_OS_OSX
        {kMStateContCmd,            kMgr,  kASBMSMBUSReadWord, 0, 0, NULL,                      kUserVis},
        {kMStateCmd,                kMgr,  kASBMSMBUSReadWord, 0, 0, NULL,                      kUserVis},
        {kBBatteryStatusCmd,        kBatt, kASBMSMBUSReadWord, 0, 0, NULL,                      kUserVis},
        {kBExtendedPFStatusCmd,     kBatt, kASBMSMBUSExtendedReadWord, 0, 0, _PFStatusSym,      kFull},
        {kBExtendedOperationStatusCmd, kBatt, kASBMSMBUSExtendedReadWord, 0, 0, _OpStatusSym,   kFull},
        {kBManufactureNameCmd,      kBatt, kASBMSMBUSReadBlock, 0, 0, manufacturerKey,          kBoot},
        {kBManufactureDataCmd,      kBatt, kASBMSMBUSReadBlock, 0, 0, _ManufacturerDataSym,     kBoot},
        {kBManufacturerInfoCmd,     kBatt, kASBMSMBUSReadBlock, 0, 0, NULL,                     kBoot},
        {kBDeviceNameCmd,           kBatt, kASBMSMBUSReadBlock, 0, 0, _DeviceNameSym,           kBoot},
        {kBAppleHardwareSerialCmd,  kBatt, kASBMSMBUSReadBlock, 0, 0, _HardwareSerialSym,       kBoot},
        {kBPackReserveCmd,          kBatt, kASBMSMBUSReadWord, 0, 0, _PackReserveSym,           kBoot},
        {kBDesignCycleCount9CCmd,   kBatt, kASBMSMBUSReadWord, 0, 0, _DesignCycleCount9CSym,    kBoot},
        {kBManufactureDateCmd,      kBatt, kASBMSMBUSReadWord, 0, 0, _ManfDateSym,              kBoot},
        {kBSerialNumberCmd,         kBatt, kASBMSMBUSReadWord, 0, 0, _SerialNumberSym,          kBoot},
        {kBMaxErrorCmd,             kBatt, kASBMSMBUSReadWord, 0, 0, _MaxErrSym,                kFull},
        {kBRunTimeToEmptyCmd,       kBatt, kASBMSMBUSReadWord, 0, 0, _InstantTimeToEmptySym,    kFull},
        {kBReadCellVoltage1Cmd,     kBatt, kASBMSMBUSReadWord, 0, 0, NULL,                      kFull},
        {kBReadCellVoltage2Cmd,     kBatt, kASBMSMBUSReadWord, 0, 0, NULL,                      kFull},
        {kBReadCellVoltage3Cmd,     kBatt, kASBMSMBUSReadWord, 0, 0, NULL,                      kFull},
        {kBReadCellVoltage4Cmd,     kBatt, kASBMSMBUSReadWord, 0, 0, NULL,                      kFull},
        {kBAverageTimeToEmptyCmd,   kBatt, kASBMSMBUSReadWord, 0, 0, NULL,                      kUserVis},
        {kBTemperatureCmd,          kBatt, kASBMSMBUSReadWord, 0, 0, NULL,                      kFull},
        {kBDesignCapacityCmd,       kBatt, kASBMSMBUSReadWord, 0, 0, _DesignCapacitySym,        kBoot},
#else
        // Order of these entries is important.
        // kMStateCmd relies on data from ExternalChargeCmd and ExternalConnectedCmd
        {kAtCriticalLevelCmd,       kBatt, kASBMSMCReadBool,   0, 0, NULL,                      kUserVis},
        {kExternalChargeCapableCmd, kBatt, kASBMSMCReadBool,   0, 0, NULL,                      kUserVis},
        {kExternalConnectedCmd,     kBatt, kASBMSMCReadBool,   0, 0, NULL,                      kUserVis},
        {kRawExternalConnectedCmd,  kBatt, kASBMSMCReadBool,   0, 0, _rawExternalConnected,     kUserVis},
        {kMStateCmd,                kMgr,  kASBMSMBUSReadWord, 0, 0, NULL,                      kUserVis},
        {kBManufactureDataCmd,      kBatt, kASBMSMBUSReadBlock, 0, 0, _ManufacturerDataSym,     kFull},
        {kBManufactureDateCmd,      kBatt, kASBMSMBUSReadBlock, 0, 0, _ManfDateSym,             kFull},
        {kBSerialNumberCmd,         kBatt, kASBMSMBUSReadBlock, 0, 0, serialKey,                kFull},
        {kRawCurrentCapacityCmd,    kBatt, kASBMSMBUSReadWord, 0, 0, _RawCurrentCapacity,       kUserVis},
        {kRawMaxCapacityCmd,        kBatt, kASBMSMBUSReadWord, 0, 0, _RawMaxCapacity,           kUserVis},
        {kNominalChargeCapacityCmd, kBatt, kASBMSMBUSReadWord, 0, 0, NULL,                      kUserVis},
        {kPresentDODCmd,            kBatt, kASBMSMBUSReadWord, 0, 0, _PresentDOD,               kUserVis},
        {kAbsoluteCapacityCmd,      kBatt, kASBMSMBUSReadWord, 0, 0, _AbsoluteCapacity,         kUserVis},
        {kITMiscStatus,             kBatt, kASBMSMBUSReadWord, 0, 0, _ITMiscStatus,             kUserVis},
        {kRawBatteryVoltageCmd,     kBatt, kASBMSMBUSReadWord, 0, 0, _rawBatteryVoltage,        kUserVis},
        {kFullyChargedCmd,          kMgr,  kASBMSMCReadBool,   0, 0, _FullyChargedSym,          kUserVis},
        {kErrorConditionCmd,        kMgr,  kASBMSMBUSReadWord, 0, 0, NULL,                      kUserVis},
        {kShutdownDataCmd,          kBatt, kASBMSMCReadDictionary,   0, 0, NULL,                kBoot},
        {kChargerConfigurationCmd,  kBatt, kASBMSMBUSReadWord,   0, 0, _kChargerConfiguration,  kUserVis},
        {kGaugeFlagRawCmd,          kBatt, kASBMSMBUSReadWord,   0, 0, _kGaugeFlagRaw,          kUserVis},
        {kBootVoltageCmd,           kBatt, kASBMSMBUSReadWord,   0, 0, _kBootVoltage,           kFull},
        {kBTemperatureCmd,          kBatt, kASBMSMBUSReadWord, 0, 0, NULL,                      kUserVis},
        {kBVirtualTemperatureCmd,   kBatt, kASBMSMBUSReadWord, 0, 0, NULL,                      kUserVis},
        {kBDesignCapacityCmd,       kBatt, kASBMSMBUSReadWord, 0, 0, _DesignCapacitySym,        kBoot},
        {kBCellDisconnectCmd,       kBatt, kASBMSMBUSReadWord,   0, 0, _kBCellDisconnectCount,  kFull},
        {kCarrierModeCmd,           kBatt, kASBMSMBUSReadWord,   0, 0, NULL,                    kUserVis},
        {kKioskModeCmd,             kBatt, kASBMSMBUSReadWord,   0, 0, NULL,                    kUserVis},
#endif
        {kBCycleCountCmd,           kBatt, kASBMSMBUSReadWord, 0, 0, cycleCountKey,             kFull},
        {kBAverageTimeToFullCmd,    kBatt, kASBMSMBUSReadWord, 0, 0, NULL,                      kUserVis},
        {kBRemainingCapacityCmd,    kBatt, kASBMSMBUSReadWord, 0, 0, NULL,                      kUserVis},
        {kBFullChargeCapacityCmd,   kBatt, kASBMSMBUSReadWord, 0, 0, NULL,                      kUserVis},
        {kFinishPolling,            0,     kASBMInvalidOp,     0, 0, NULL,                      kUserVis}
    };

    cmdTable.table = NULL;
    cmdTable.count = 0;

    if ((cmdTable.table = (CommandStruct *)IOMalloc(sizeof(local_cmd)))) {
        cmdTable.count = ARRAY_SIZE(local_cmd);
        bcopy(&local_cmd, cmdTable.table, sizeof(local_cmd));
    }
}

/******************************************************************************
 * AppleSmartBattery::commandForState
 *
 ******************************************************************************/
CommandStruct *AppleSmartBattery::commandForState(uint32_t state)
{
    if (cmdTable.table) {
        for (int i=0; i<cmdTable.count; i++) {
            if (state == cmdTable.table[i].cmd) {
                return &cmdTable.table[i];
            }
        }
    }
    return NULL;
}

/******************************************************************************
 * AppleSmartBattery::initiateTransaction
 *
 ******************************************************************************/
#if TARGET_OS_OSX
bool AppleSmartBattery::doInitiateTransaction(const CommandStruct *cs)
{
    ASBMgrRequest req;

    req.opType = cs->opType;
    req.address = cs->addr;
    req.command = cs->cmd;
    req.fullyDischarged = fFullyDischarged;
    req.completionHandler = OSMemberFunctionCast(ASBMgrTransactionCompletion,
            this, &AppleSmartBattery::transactionCompletion);


    return fProvider->performTransaction(&req, (OSObject *)this, (void *)cs);
}
#endif


void AppleSmartBattery::updateDictionaryInIOReg(const OSSymbol *sym, smcToRegistry *keys)
{
    OSDictionary *dict = smcKeysToDictionary(keys);
    if (!dict) {
        return;
    }

    OSDictionary *prevDict = OSDynamicCast(OSDictionary, getProperty(sym));
    if (prevDict) {
        prevDict = OSDynamicCast(OSDictionary, prevDict->copyCollection());
    }

    if (prevDict) {
        prevDict->merge(dict);
        OSSafeReleaseNULL(dict);
    } else {
        prevDict = dict;
    }

    setProperty(sym, prevDict);
    OSSafeReleaseNULL(prevDict);
}

bool AppleSmartBattery::initiateTransaction(const CommandStruct *cs)
{
    uint32_t cmd = cs->cmd;

    if (cmd == kFinishPolling) {
        this->handlePollingFinished(true);
        return true;
    }
    ret = doInitiateTransaction(cs);
    if (ret != kIOReturnSuccess) {
        BM_ERRLOG("Command 0x%x failed with error 0x%x\n", cmd, ret);
    }

    return ret;
}


/******************************************************************************
 * AppleSmartBattery::initiateNextTransaction
 *
 ******************************************************************************/
bool AppleSmartBattery::initiateNextTransaction(uint32_t state)
{
    int found_current_index = 0;
    const CommandStruct *cs = NULL;

    if (!cmdTable.table) {
        return false;
    }

    // Find index for "state" in cmd_machine
    for (found_current_index = 0; found_current_index < cmdTable.count; found_current_index++) {
        if (cmdTable.table[found_current_index].cmd == state) {
            break;
        }
    }
    // Find next state to read for fMachinePath
    if (++found_current_index < cmdTable.count) {
        for (; found_current_index<cmdTable.count; found_current_index++)
        {
            if (cmdTable.table[found_current_index].pathBits >= fMachinePath)
            {
                cs = &cmdTable.table[found_current_index];
                break;
            }
        }
    }
    
    if (cs)
        return initiateTransaction(cs);
    
    return false;
}

/******************************************************************************
 * AppleSmartBattery::handleSystemSleepWake
 *
 * Caller must hold the gate.
 ******************************************************************************/

IOReturn AppleSmartBattery::handleSystemSleepWake(
    IOService * powerService, bool isSystemSleep)
{
    IOReturn ret = kIOPMAckImplied;

    if (!fWorkLoop->inGate()) {
        return fWorkLoop->runAction(
                OSMemberFunctionCast(IOWorkLoop::Action, this,
                    &AppleSmartBattery::handleSystemSleepWake), this,
                powerService, (void *)isSystemSleep);
    }

    if (!powerService || (fSystemSleeping == isSystemSleep))
        return kIOPMAckImplied;

    if (fPowerServiceToAck)
    {
        fPowerServiceToAck->release();
        fPowerServiceToAck = 0;
    }

    fSystemSleeping = isSystemSleep;
    if (fSystemSleeping)
    {
        // Stall PM until battery poll in progress is cancelled.
        if (fPollingNow)
        {
            fPowerServiceToAck = powerService;
            fPowerServiceToAck->retain();
            ret = (kBatteryReadAllTimeout * 1000);
        }
    }
    else if (fPollRequestedInSleep) {
        pollBatteryState(kFull);
    }
    fPollRequestedInSleep = false;

    BM_LOG1("SmartBattery: handleSystemSleepWake(%d) = %u\n", isSystemSleep, (uint32_t) ret);

    return ret;
}

/******************************************************************************
 * AppleSmartBattery::acknowledgeSystemSleepWake
 *
 * Caller must hold the gate.
 ******************************************************************************/

void AppleSmartBattery::acknowledgeSystemSleepWake(void)
{
    if (fPowerServiceToAck)
    {
        fPowerServiceToAck->acknowledgeSetPowerState();
        fPowerServiceToAck->release();
        fPowerServiceToAck = 0;

        if (fBatteryReadAllTimer) {
            fBatteryReadAllTimer->cancelTimeout();
        }

        BM_LOG1("SmartBattery: final acknowledge of wake after reading all regs\n");
    }
}

/******************************************************************************
 * AppleSmartBattery::pollBatteryState
 *
 * Asynchronously kicks off the register poll.
 ******************************************************************************/

#define kMinimumPollingFrequencyMS      1000

bool AppleSmartBattery::pollBatteryState(int type)
{
    if (!fWorkLoop->inGate()) {
        return fWorkLoop->runAction(
                OSMemberFunctionCast(IOWorkLoop::Action, this,
                    &AppleSmartBattery::pollBatteryState), this, (void *)(uintptr_t)type);
    }
    /* Don't perform any SMBus activity if a AppleSmartBatteryManagerUserClient
       has grabbed exclusive access
     */
    if (fProvider->hasExclusiveClient()) {
        BM_ERRLOG("AppleSmartBattery::pollBatteryState was stalled by an exclusive user client.\n");
        return false;
    }

    if (fPollingNow && (fMachinePath <= type)) {
        /* We're already in the middle of a poll for a superset of 
         * the requested battery data.
         */
        BM_LOG1("AppleSmartBattery::pollBatteryState already polling (%d <= %d). Restarting poll\n", fMachinePath, type);
    }
    else if (type != kUseLastPath) {
        fMachinePath = type;
    }

    if (fInitialPollCountdown > 0) {
        // We're going out of our way to make sure that we get a successfull
        // initial poll at boot. Upgrade all early boot polls to kBoot.
        fMachinePath = kBoot;
    }

    if (!fPollingNow) {
        BM_LOG1("Starting poll type %d\n", fMachinePath);
        /* Start the battery polling state machine (resetting it if it's already in progress) */
        transactionCompletion((void *)kTransactionRestart, 0, 0, NULL);
        return true;
    } else {
        /* Outstanding transaction in process; flag it to restart polling from
           scratch when this flag is noticed.
         */
        fRebootPolling = true;
        return true;
    }
}



void AppleSmartBattery::handleBatteryInserted(void)
{
    if (!fWorkLoop) return;

    if (!fWorkLoop->inGate()) {
        fWorkLoop->runAction(
                OSMemberFunctionCast(IOWorkLoop::Action, this,
                    &AppleSmartBattery::handleBatteryInserted), this);
        return;
    }

    BM_LOG1("SmartBattery: battery inserted!\n");

    clearBatteryState(false);
    fRebootPolling = true;
    pollBatteryState(kBoot);
    return;
}

void AppleSmartBattery::handleBatteryRemoved(void)
{

    if (!fWorkLoop) return;

    if (!fWorkLoop->inGate()) {
        fWorkLoop->runAction(
                OSMemberFunctionCast(IOWorkLoop::Action, this,
                    &AppleSmartBattery::handleBatteryRemoved), this);
        return;
    }

    if (fPollingNow) {
        fCancelPolling = true;
        if (fBatteryReadAllTimer) {
            fBatteryReadAllTimer->cancelTimeout();
        }
    }

    clearBatteryState(true);

    BM_LOG1("SmartBattery: battery removed\n");

    acknowledgeSystemSleepWake();
    return;
}

void AppleSmartBattery::handleInflowDisabled(bool inflow_state)
{

    if (!fWorkLoop->inGate()) {
        fWorkLoop->runAction(
                OSMemberFunctionCast(IOWorkLoop::Action, this,
                    &AppleSmartBattery::handleInflowDisabled), this, VOIDPTR(inflow_state));
        return;
    }

    fInflowDisabled = inflow_state;
    // And kick off a re-poll using this new information
    pollBatteryState(kFull);

    return;
}

void AppleSmartBattery::handleChargeInhibited(bool charge_state)
{
    if (!fWorkLoop->inGate()) {
        fWorkLoop->runAction(
                OSMemberFunctionCast(IOWorkLoop::Action, this,
                    &AppleSmartBattery::handleChargeInhibited), this, VOIDPTR(charge_state));
        return;
    }

    fChargeInhibited = charge_state;
    // And kick off a re-poll using this new information
    pollBatteryState(kFull);
}

void AppleSmartBattery::handleSetOverrideCapacity(uint16_t value, bool sticky)
{
    if (!fWorkLoop->inGate()) {
        fWorkLoop->runAction(
                OSMemberFunctionCast(IOWorkLoop::Action, this,
                    &AppleSmartBattery::handleSetOverrideCapacity), this, VOIDPTR(value), VOIDPTR(sticky));
        return;
    }
    if (sticky) {
        fCapacityOverride = true;
        BM_LOG1("Capacity override is set to true\n");
    } else {
        fCapacityOverride = false;
    }

    setCurrentCapacity(value);
}

void AppleSmartBattery::handleSwitchToTrueCapacity(void)
{
    if (!fWorkLoop->inGate()) {
        fWorkLoop->runAction(
                OSMemberFunctionCast(IOWorkLoop::Action, this,
                    &AppleSmartBattery::handleSwitchToTrueCapacity), this);
        return;
    }

    fCapacityOverride = false;
    BM_LOG1("Capacity override is set to false\n");
    return;
}

/******************************************************************************
 * incompleteReadTimeOut
 *
 * The complete battery read has not completed in the allowed timeframe.
 * We assume this is for several reasons:
 *    - The EC has dropped an SMBus packet (probably recoverable)
 *    - The EC has stalled an SMBus request; IOSMBusController is hung (probably not recoverable)
 *
 * Start the battery read over from scratch.
 *****************************************************************************/

void AppleSmartBattery::incompleteReadTimeOut(void)
{
    BM_ERRLOG("Failed to complete polling of all data within %d ms\n", kBatteryReadAllTimeout);

    /* Don't launch infinite re-tries if the system isn't completing my transactions
     *  (and thus probably leaking a lot of memory every time.
     *  Quit after kIncompleteReadRetryMax
     */
    handlePollingFinished(false);
    if (!fSystemSleeping && (0 < fIncompleteReadRetries))
    {
        fIncompleteReadRetries--;
        pollBatteryState(kUseLastPath);
    }
}


void AppleSmartBattery::handlePollingFinished(bool visitedEntirePath)
{
    uint64_t now, nsec;

    if (fBatteryReadAllTimer) {
        fBatteryReadAllTimer->cancelTimeout();
    }

    if (visitedEntirePath) {
        const char *reportPathFinishedKey;
        clock_sec_t secs;
        clock_usec_t microsecs;

        if (kBoot == fMachinePath) {
            reportPathFinishedKey = kBootPathKey;
        } else if (kFull == fMachinePath) {
            reportPathFinishedKey = kFullPathKey;
        } else if (kUserVis == fMachinePath) {
            reportPathFinishedKey = kUserVisPathKey;
        } else {
            reportPathFinishedKey = NULL;
        }

        clock_get_calendar_microtime(&secs, &microsecs);
        if (reportPathFinishedKey) {
            setProperty(reportPathFinishedKey, secs, 32);
        }

        if (fInitialPollCountdown > 0) {
            fInitialPollCountdown--;
        }

        rebuildLegacyIOBatteryInfo();
        if (acAttach_ts) {
            clock_get_uptime(&now);
            SUB_ABSOLUTETIME(&now, &acAttach_ts);
            absolutetime_to_nanoseconds(now, &nsec);
            if (nsec < (60 * NSEC_PER_SEC)) {
                // In some cases, power adapter information is available thru multiple updates
                // from SMC(19657502). As power adapter info is not populated to registry, we are 
                // force setting the 'settingsChangedSinceUpdate' to make sure notifications are 
                // sent to clients.
                settingsChangedSinceUpdate = true;
            }
            else {
                // Zero this out to avoid time comparisions every time
                acAttach_ts = 0;
            }
        }
        BM_LOG1("SmartBattery: finished polling type %d\n", fMachinePath);

        updateStatus();
    } else {
        BM_ERRLOG("SmartBattery: abort polling\n");

        if (fBatteryReadAllTimer) {
            fBatteryReadAllTimer->setTimeoutMS(kBatteryReadAllTimeout);
        }
    }


    fPollingNow = false;

    acknowledgeSystemSleepWake();
}

bool AppleSmartBattery::handleSetItAndForgetIt(int state, int val, const uint8_t *str32, IOByteCount len)
{
    CommandStruct   *this_command = NULL;
    const OSData    *publishData;
    const OSSymbol  *publishSym;

    /* Set it and forget it
     *
     * These commands specify an OSSymbol in their CommandStruct.
     * We directly publish the data into these registry keys.
     */
    if ((this_command = commandForState(state)) && this_command->setItAndForgetItSym) {
        if ((this_command->opType == kASBMSMBUSReadWord) || (this_command->opType == kASBMSMBUSExtendedReadWord)) {
            SET_INTEGER_IN_PROPERTIES(this_command->setItAndForgetItSym, val, (unsigned int)len);
            return true;
        } else if (this_command->opType == kASBMSMBUSReadBlock) {
            if (state == kBManufactureDataCmd) {
                publishData = OSData::withBytes((const void *)str32, (unsigned int)len);
                if (publishData) {
                    setPSProperty(this_command->setItAndForgetItSym, (OSObject *)publishData);
                    publishData->release();
                    return true;
                }
            }
            else {
                publishSym = OSSymbol::withCString((const char *)str32);
                if (publishSym) {
                    setPSProperty(this_command->setItAndForgetItSym, (OSObject *)publishSym);
                    publishSym->release();
                    return true;
                }
            }
        }
        else if (this_command->opType == kASBMSMCReadBool) {
            setPSProperty(this_command->setItAndForgetItSym, (*str32) ? kOSBooleanTrue : kOSBooleanFalse);
            return true;
        }
        else if (this_command->opType == kASBMSMCReadDictionary) {
            // This is data is already set to registry as required
            return true;
        }
    }

    return false;
}

/******************************************************************************
 * AppleSmartBattery::transactionCompletion
 * -> Runs in workloop context
 *
 ******************************************************************************/

void AppleSmartBattery::transactionCompletion(void *ref, IOReturn status, IOByteCount inCount, uint8_t *inData)
{
    bool            transaction_success = (status == kIOReturnSuccess);
    uint32_t        next_state = kTransactionRestart;
    uint32_t        val = 0;
    uint64_t        val64 = 0;
    OSNumber        *num = NULL;
    CommandStruct * cs = (CommandStruct *)ref;
    uint32_t        cmd = kTransactionRestart;
    uint32_t        smcKey = 0;
    static unsigned int txnFailures;

    if (!fWorkLoop->inGate()) {
        fWorkLoop->runAction(
                OSMemberFunctionCast(IOWorkLoop::Action, this, &AppleSmartBattery::transactionCompletion),
                this, ref, VOIDPTR(status), VOIDPTR(inCount), VOIDPTR(inData));
        return;
    }

    if (fCancelPolling) {
        goto abort;
    }

    if (fSystemSleeping) {
        BM_ERRLOG("Aborting transactions as system is sleeping\n");
        fPollRequestedInSleep = true;
        goto abort;
    }

    if (cs) {
        next_state = cmd = cs->cmd;
        smcKey = cs->smcKey;
    }

    if (cmd) {
        if (transaction_success) {
            txnFailures = 0;

            if (inData) {
                if (inCount == 1) {
                    val = inData[0];
                    BM_LOG2("smcReadKeyHostEndian for cmd 0x%x (key %c%c%c%c) %d\n", cmd, SMCKEY2CHARS(smcKey), val);
                }
                else if (inCount == 2) {
                    val = (inData[1] << 8) | inData[0];
                    BM_LOG2("smcReadKeyHostEndian for cmd 0x%x (key %c%c%c%c) %d\n", cmd, SMCKEY2CHARS(smcKey), val);
                }
                else if (inCount == 4) {
                    val = (inData[3] << 24) | (inData[2] << 16) | (inData[1] << 8) | inData[0];
                    BM_LOG2("smcReadKeyHostEndian for cmd 0x%x (key %c%c%c%c) %d\n", cmd, SMCKEY2CHARS(smcKey), val);
                }
                else if (inCount == 8) {
                    val = (inData[7] << 24) | (inData[6] << 16) | (inData[5] << 8) | inData[4];
                    val64 = (uint64_t)val << 32;
                    val = (inData[3] << 24) | (inData[2] << 16) | (inData[1] << 8) | inData[0];
                    val64 |= val;
                    BM_LOG2("smcReadKeyHostEndian for cmd 0x%x (key %c%c%c%c) 0x%llx\n", cmd, SMCKEY2CHARS(smcKey), val64);
                }
            }

            if (handleSetItAndForgetIt(cmd, val, inData, inCount)) {
                goto exit;
            }
        } else {
            txnFailures++;
            goto exit;
        }
    }

    // Restart?
    if (!cmd || fRebootPolling) {
        // NULL cmd means we should start the state machine from scratch.
        cmd = next_state = kTransactionRestart;
        fRebootPolling = false;
        BM_LOG1("Restarting poll type %d\n", fMachinePath);
    }

    switch (cmd) {
    case kTransactionRestart:
        fCancelPolling = false;
        fPollingNow = true;

        /* Initialize battery read timeout to catch any longstanding stalls. */
        if (fBatteryReadAllTimer) {
            fBatteryReadAllTimer->cancelTimeout();
            fBatteryReadAllTimer->setTimeoutMS(kBatteryReadAllTimeout);
        }
        break;

    case kMStateContCmd:
        // Determines if AC is plugged or unplugged
        // Determines if AC is "charge capable"
        if (transaction_success)
        {
            /* If fInflowDisabled is currently set, then we acknowledge
             * our lack of AC power. inflow disable means the system is not drawing power from AC.
             * (Having inflow disabled is uncommon.)
             *
             * Even with inflow disabled, the AC bit is still true if AC
             * is attached. We zero the bit instead, so that it looks
             * more accurate in BatteryMonitor.
             */
            bool new_ac_connected = (!fInflowDisabled && (val & kMACPresentBit)) ? 1:0;

            // Tell IOPMrootDomain on ac connect/disconnect
            IOPMrootDomain *rd = getPMRootDomain();
            if (new_ac_connected != fACConnected)
            {
                if (new_ac_connected) {
                    clock_get_uptime(&acAttach_ts);
                    if (rd) rd->receivePowerNotification(kIOPMSetACAdaptorConnected | kIOPMSetValue);
                } else {
                    acAttach_ts = 0;
                    if (rd) rd->receivePowerNotification(kIOPMSetACAdaptorConnected);
                }
            }

            fACConnected = new_ac_connected;
            setExternalConnectedToIOPMPowerSource(fACConnected);

            setExternalChargeCapable((val & kMPowerNotGoodBit) ? false:true);

        } else {
            fACConnected = false;
            setExternalConnectedToIOPMPowerSource(true);

            setExternalChargeCapable(false);
        }
        break;

    case kMStateCmd:
        // Determines if battery is present
        // Determines if battery is charging
        if (transaction_success)
        {
#if TARGET_OS_OSX
            fBatteryPresent = (val & kMPresentBatt_A_Bit) ? true : false;
            setBatteryInstalled(fBatteryPresent);
#else
            fBatteryPresent = ((val & (1 << 6)) >> 6) ? false : true;
#endif


            // If fChargeInhibit is currently set, then we acknowledge
            // our lack of charging and force the "isCharging" bit to false.
            //
            // charge inhibit means the battery will not charge, even if
            // AC is attached.
            // Without marking this lack of charging here, it can take
            // up to 30 seconds for the charge disable to be reflected in
            // the UI.

            if (fChargeInhibited) {
                setIsCharging(false);
             } else {
#if TARGET_OS_OSX
                setIsCharging((val & kMChargingBatt_A_Bit) ? true:false);
#else
                // 'val' represents the ChargerStatus
                // IsCharging = ExternalConnected && ChargeCapable && !ChargeInhibted
                setIsCharging((val && fACConnected && fACChargeCapable) ? true : false);
                BM_LOG2("ChargerState:%d ExtConnected:%d ChargeCapable:%d\n", val, fACConnected, fACChargeCapable);
#endif
             }

        } else {
            fBatteryPresent = false;
            setIsCharging(false);
            setBatteryInstalled(false);
        }

        break;

    case kBBatteryStatusCmd:
        if (!transaction_success) {
            fFullyCharged = false;
            fFullyDischarged = false;
        } else {

            if (val & kBFullyChargedStatusBit) {
                fFullyCharged = true;
            } else {
                fFullyCharged = false;
            }

            if (val & kBFullyDischargedStatusBit) {
                if (!fFullyDischarged) {
                    fFullyDischarged = true;

                    // Immediately cancel AC Inflow disable
                    fProvider->handleFullDischarge();
                }
            } else {
                fFullyDischarged = false;
            }

            /* Detect battery permanent failure
             * Permanent battery failure is marked by
             * (TerminateDischarge & TerminateCharge) bits being set simultaneously.
             */
            if ((val
                & (kBTerminateDischargeAlarmBit | kBTerminateChargeAlarmBit))
                == (kBTerminateDischargeAlarmBit | kBTerminateChargeAlarmBit)) {
                BM_ERRLOG("Failed with permanent failure for cmd 0x%x\n", next_state);
                setErrorCondition((OSSymbol *)_PermanentFailureSym);

                fPermanentFailure = true;

                /* We want to display the battery as present & completely discharged, not charging */
                fBatteryPresent = true;
                setBatteryInstalled(true);
                setIsCharging(false);
            } else {
                fPermanentFailure = false;
            }
        }

        setFullyCharged(fFullyCharged);

        /* If the battery is present, we continue with our state machine
           and read battery state below.
           Otherwise, if the battery is not present, we zero out all
           the settings that would have been set in a connected battery.
        */
        if (!fBatteryPresent) {
            // Clean-up battery state for absent battery; do no further
            // battery work until messaged that another battery has
            // arrived.

            // zero out battery state with argument (do_update == true)
            // clearing out the battery statud here since battery is not found!!!
            clearBatteryState(true);
            goto abort;
        }

        break;

    case kBRemainingCapacityCmd:

        fRemainingCapacity = val;

        if (!fCapacityOverride) {
            setCurrentCapacity(val);
        }
        else {
            BM_LOG1("Capacity override is true\n");
        }

#if TARGET_OS_OSX
        SET_INTEGER_IN_PROPERTIES(_RawCurrentCapacity, val, 2);
#endif

        if (!fPermanentFailure && (0 == fRemainingCapacity))
        {
            // fRemainingCapacity == 0 is an absurd value.
            // We have already retried several times, so we accept this value and move on.
            BM_ERRLOG("Battery remaining capacity is set to 0\n");
        }
        break;

    case kBFullChargeCapacityCmd:

        fFullChargeCapacity = val;

        if (!fCapacityOverride) {
            setMaxCapacity(val);
        }
        else {
            BM_LOG1("Capacity override is true\n");
        }

#if TARGET_OS_OSX
        SET_INTEGER_IN_PROPERTIES(_RawMaxCapacity, val, 2);
#endif

        break;

    /* *Instant* current */
    case kBCurrentCmd:
        SET_INTEGER_IN_PROPERTIES(_InstantAmperageSym, (int32_t)((int16_t)(val & 0xffff)), 4);
        fInstantCurrent = (int)(int16_t)val;
        break;

    /* Average current */
    case kBAverageCurrentCmd:
        setAmperage((int16_t)val);
        if (!val) {
            // Battery not present, or fully charged, or general error
            setTimeRemaining(0);
        }
        break;

    case kBAverageTimeToEmptyCmd:

        SET_INTEGER_IN_PROPERTIES(_AvgTimeToEmptySym, val, inCount);
        
        if (fInstantCurrent < 0) {
            setTimeRemaining(val);
        }
        break;
        
    case kBAverageTimeToFullCmd:

        SET_INTEGER_IN_PROPERTIES(_AvgTimeToFullSym, val, inCount);
        
        if (fInstantCurrent > 0) {
            setTimeRemaining(val);
        }
        break;
            
    case kBReadCellVoltage4Cmd:
    case kBReadCellVoltage3Cmd:
    case kBReadCellVoltage2Cmd:
    case kBReadCellVoltage1Cmd:

        if (kBReadCellVoltage1Cmd == next_state) {
            if (fCellVoltages)
            {
                fCellVoltages->release();
                fCellVoltages = NULL;
            }
            fCellVoltages = OSArray::withCapacity(4);
        }

        // Executed for all 4 CellVoltage calls through here
        if (fCellVoltages)
        {
            num = OSNumber::withNumber(val, 16);
            if (num) {
                fCellVoltages->setObject(num);
                num->release();
            }
        }

        // Executed for CellVoltage4
        if (kBReadCellVoltage4Cmd == next_state)
        {
            if (fCellVoltages)
            {
                setProperty(_CellVoltageSym, fCellVoltages);
                fCellVoltages->release();
                fCellVoltages = NULL;
            } else {
                removeProperty(_CellVoltageSym);
            }
        }
        break;

    case kBExtendedPFStatusCmd:
    case kBExtendedOperationStatusCmd:
        // 2 stage commands, first stage SMBus write completed.
        // Do nothing other than to prevent the error log in the default case.
        break;

    case kBTemperatureCmd:
        SET_INTEGER_IN_PROPERTIES(_TemperatureSym, val, 4);
        break;


    default:
        BM_ERRLOG("SmartBattery: Error state %x not expected\n", next_state);
    }

exit:
    if (txnFailures > 5) {
        BM_ERRLOG("Too many transaction errors, abort poll\n");
        goto abort;
    }

    /* Kick off the next transaction */
    if (kFinishPolling != next_state) {
        this->initiateNextTransaction(next_state);
    }
    return;

abort:
    handlePollingFinished(false);
    return;
}


void AppleSmartBattery::clearBatteryState(bool do_update)
{
    if (!fWorkLoop->inGate()) {
        fWorkLoop->runAction(
                OSMemberFunctionCast(IOWorkLoop::Action, this, &AppleSmartBattery::clearBatteryState),
                this, VOIDPTR(do_update));
        return;
    }

    // Only clear out battery state; don't clear manager state like AC Power.
    // We just zero out the int and bool values, but remove the OSType values.

    fFullyDischarged        = false;
    fFullyCharged           = false;
    fBatteryPresent         = false;
    fACConnected            = -1;

    setBatteryInstalled(false);
    setIsCharging(false);
    setCurrentCapacity(0);
    setMaxCapacity(0);
    setTimeRemaining(0);
    setAmperage(0);
    setVoltage(0);
    setCycleCount(0);
    setAdapterInfo(0);
    setLocation(0);

    properties->removeObject(manufacturerKey);
    removeProperty(manufacturerKey);
    properties->removeObject(serialKey);
    removeProperty(serialKey);

    // Let rebuildLegacyIOBatteryInfo() update batteryInfoKey and detect
    // if any value in the dictionary has changed. Removing batteryInfoKey
    // from properties will always dirty the battery state and will cause
    // IOPMPowerSource::updateStatus() to message clients unnecessarily
    // when battery is not present.
    // properties->removeObject(batteryInfoKey);

    removeProperty(batteryInfoKey);
    properties->removeObject(errorConditionKey);
    removeProperty(errorConditionKey);
    properties->removeObject(_PFStatusSym);
    removeProperty(_PFStatusSym);

    rebuildLegacyIOBatteryInfo();

    BM_ERRLOG("Clearing out battery data\n");

    if (do_update) {
        updateStatus();
    }
}


/******************************************************************************
 *  Package battery data in "legacy battery info" format, readable by
 *  any applications using the not-so-friendly IOPMCopyBatteryInfo()
 ******************************************************************************/

 void AppleSmartBattery::rebuildLegacyIOBatteryInfo(void)
 {
#if TARGET_OS_OSX
    if (!fWorkLoop->inGate()) {
        fWorkLoop->runAction(
                OSMemberFunctionCast(IOWorkLoop::Action, this, &AppleSmartBattery::rebuildLegacyIOBatteryInfo),
                this);
        return;
    }

    OSDictionary        *legacyDict = OSDictionary::withCapacity(5);
    uint32_t            flags = 0;
    OSNumber            *flags_num = NULL;

    if (externalConnected()) flags |= kIOPMACInstalled;
    if (batteryInstalled()) flags |= kIOPMBatteryInstalled;
    if (isCharging()) flags |= kIOPMBatteryCharging;

    flags_num = OSNumber::withNumber((unsigned long long)flags, 32);
    legacyDict->setObject(kIOBatteryFlagsKey, flags_num);
    flags_num->release();

    legacyDict->setObject(kIOBatteryCurrentChargeKey, properties->getObject(kIOPMPSCurrentCapacityKey));
    legacyDict->setObject(kIOBatteryCapacityKey, properties->getObject(kIOPMPSMaxCapacityKey));
    legacyDict->setObject(kIOBatteryVoltageKey, properties->getObject(kIOPMPSVoltageKey));
    legacyDict->setObject(kIOBatteryAmperageKey, properties->getObject(kIOPMPSAmperageKey));
    legacyDict->setObject(kIOBatteryCycleCountKey, properties->getObject(kIOPMPSCycleCountKey));

    setLegacyIOBatteryInfo(legacyDict);

    legacyDict->release();
#endif
}


/******************************************************************************
 *  Power Source value accessors
 *  These supplement the built-in accessors in IOPMPowerSource.h.
 ******************************************************************************/

#define CLASS   AppleSmartBattery

#define IMPLEMENT_APPLESMARTBATTERY_INT_GETTER(methodName, pspropSYM, return_type) \
    return_type CLASS::methodName(void) {                       \
        OSNumber *n = OSDynamicCast(OSNumber, properties->getObject(_SerialNumberSym)); \
        if (n) {                                                \
            return n->unsigned16BitValue();                     \
        } else {                                                \
            return 0;                                           \
        }                                                       \
    }
IMPLEMENT_APPLESMARTBATTERY_INT_GETTER(serialNumber, _SerialNumberSym, uint16_t);
IMPLEMENT_APPLESMARTBATTERY_INT_GETTER(maxErr, _MaxErrSym, int);
IMPLEMENT_APPLESMARTBATTERY_INT_GETTER(averageTimeToEmpty, _AvgTimeToEmptySym, int);
IMPLEMENT_APPLESMARTBATTERY_INT_GETTER(averageTimeToFull, _AvgTimeToFullSym, int);
IMPLEMENT_APPLESMARTBATTERY_INT_GETTER(manufactureDate, _ManfDateSym, int);

OSSymbol * AppleSmartBattery::deviceName(void)
{
    return OSDynamicCast(OSSymbol, properties->getObject(_DeviceNameSym));
}

void AppleSmartBattery::setFullyCharged(bool charged)
{
    setPSProperty(_FullyChargedSym, (charged ? kOSBooleanTrue:kOSBooleanFalse));
}

bool AppleSmartBattery::fullyCharged(void)
{
    return (kOSBooleanTrue == properties->getObject(_FullyChargedSym));
}

void AppleSmartBattery::setExternalConnectedToIOPMPowerSource(bool externalConnected)
{
    setExternalConnected(externalConnected);
}

