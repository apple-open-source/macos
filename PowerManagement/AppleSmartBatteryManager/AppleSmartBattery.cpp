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
#include <IOKit/pwr_mgt/IOPMPrivate.h>
#include <libkern/c++/OSObject.h>
#include <kern/clock.h>
#include "AppleSmartBatteryManager.h"
#include "AppleSmartBattery.h"


// Retry attempts on SMBus command failure
enum {
    kRetryAttempts = 5,
    kInitialPollCountdown = 5,
    kIncompleteReadRetryMax = 10
};

enum {
    kSecondsUntilValidOnWake    = 30,
    kPostChargeWaitSeconds      = 120,
    kPostDischargeWaitSeconds   = 120
};



// This bit lets us distinguish between reads & writes in the transactionCompletion switch statement
#define kStage2                             0x8000

// Argument to transactionCompletion indicating we should start/re-start polling
#define kTransactionRestart                 0x9999

#define kErrorRetryAttemptsExceeded         "Read Retry Attempts Exceeded"
#define kErrorOverallTimeoutExpired         "Overall Read Timeout Expired"
#define kErrorZeroCapacity                  "Capacity Read Zero"
#define kErrorPermanentFailure              "Permanent Battery Failure"
#define kErrorNonRecoverableStatus          "Non-recoverable status failure"
#define kErrorClearBattery                  "Clear Battery"

static const uint32_t kBatteryReadAllTimeout = 10000;       // 10 seconds

// Delays to use on subsequent SMBus re-read failures.
// In microseconds.
static const uint32_t microSecDelayTable[kRetryAttempts] =
    { 10, 100, 1000, 10000, 250000 };

/* The union of the errors listed in STATUS_ERROR_NEEDS_RETRY
 * and STATUS_ERROR_NON_RECOVERABLE should equal the entirety of
 * SMBus errors listed in IOSMBusController.h
 */
#define STATUS_ERROR_NEEDS_RETRY(err)                           \
     ((kIOSMBusStatusDeviceAddressNotAcknowledged == err)       \
   || (kIOSMBusStatusDeviceCommandAccessDenied == err)          \
   || (kIOSMBusStatusDeviceAccessDenied == err)                 \
   || (kIOSMBusStatusUnknownHostError == err)                   \
   || (kIOSMBusStatusUnknownFailure == err)                     \
   || (kIOSMBusStatusDeviceError == err)                        \
   || (kIOSMBusStatusTimeout == err)                            \
   || (kIOSMBusStatusBusy == err))

#define STATUS_ERROR_NON_RECOVERABLE(err)                       \
     ((kIOSMBusStatusHostUnsupportedProtocol == err)            \
   || (kIOSMBusStatusPECError == err))


// Keys we use to publish battery state in our IOPMPowerSource::properties array
static const OSSymbol *_MaxErrSym               = OSSymbol::withCString(kIOPMPSMaxErrKey);
static const OSSymbol *_DeviceNameSym           = OSSymbol::withCString(kIOPMDeviceNameKey);
static const OSSymbol *_FullyChargedSym         = OSSymbol::withCString(kIOPMFullyChargedKey);
static const OSSymbol *_AvgTimeToEmptySym       = OSSymbol::withCString("AvgTimeToEmpty");
static const OSSymbol *_InstantTimeToEmptySym   = OSSymbol::withCString("InstantTimeToEmpty");
static const OSSymbol *_InstantAmperageSym      = OSSymbol::withCString("InstantAmperage");
static const OSSymbol *_AvgTimeToFullSym        = OSSymbol::withCString("AvgTimeToFull");
static const OSSymbol *_ManfDateSym             = OSSymbol::withCString(kIOPMPSManufactureDateKey);
static const OSSymbol *_DesignCapacitySym       = OSSymbol::withCString(kIOPMPSDesignCapacityKey);
static const OSSymbol *_TemperatureSym          = OSSymbol::withCString("Temperature");
static const OSSymbol *_CellVoltageSym          = OSSymbol::withCString("CellVoltage");
static const OSSymbol *_ManufacturerDataSym     = OSSymbol::withCString("ManufacturerData");
static const OSSymbol *_PFStatusSym             = OSSymbol::withCString("PermanentFailureStatus");
static const OSSymbol *_DesignCycleCount70Sym   = OSSymbol::withCString("DesignCycleCount70");
static const OSSymbol *_DesignCycleCount9CSym   = OSSymbol::withCString("DesignCycleCount9C");
static const OSSymbol *_PackReserveSym          = OSSymbol::withCString("PackReserve");
static const OSSymbol *_OpStatusSym      = OSSymbol::withCString("OperationStatus");
static const OSSymbol *_PermanentFailureSym     = OSSymbol::withCString(kErrorPermanentFailure);
/* _SerialNumberSym represents the manufacturer's 16-bit serial number in
    numeric format.
 */
static const OSSymbol *_SerialNumberSym         = OSSymbol::withCString("FirmwareSerialNumber");
/* _HardwareSerialSym == AppleSoftwareSerial
   represents the Apple-defined 12+ character string in firmware. 
 */
static const OSSymbol *_HardwareSerialSym       = OSSymbol::withCString("BatterySerialNumber");

// CommandMachine::pathBits
enum {
    kUseLastPath    = 0,
    kBoot           = 1,
    kFull           = 2,
    kUserVis        = 4,
};
typedef int MachinePath;

#define kBootPathKey             "BootPathUpdated"
#define kFullPathKey             "FullPathUpdated"
#define kUserVisPathKey          "UserVisiblePathUpdated"

// CommandMachine::protocol
#define kWord                   kIOSMBusProtocolReadWord
#define kBlock                  kIOSMBusProtocolReadBlock
#define kBlockData              (kIOSMBusProtocolReadBlock | 0x1000)
#define kWriteWord              kIOSMBusProtocolWriteWord
#define kBatt                   kSMBusBatteryAddr
#define kMgr                    kSMBusManagerAddr

#define kFinishPolling          0xF1


#define super IOPMPowerSource

OSDefineMetaClassAndStructors(AppleSmartBattery,IOPMPowerSource)

/******************************************************************************
 * AppleSmartBattery::smartBattery
 *
 ******************************************************************************/

AppleSmartBattery *
AppleSmartBattery::smartBattery(void)
{
    AppleSmartBattery  *me;
    me = new AppleSmartBattery;

    if (me && !me->init()) {
        me->release();
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
    IORegistryEntry *p = NULL;

    BattLog("AppleSmartBattery loading...\n");

    fProvider = OSDynamicCast(AppleSmartBatteryManager, provider);

    if (!fProvider || !super::start(provider)) {
        return false;
    }

    fPollingNow             = false;
    fCancelPolling          = false;
    fRetryAttempts          = 0;
    fPermanentFailure       = false;
    fFullyDischarged        = false;
    fFullyCharged           = false;
    fBatteryPresent         = -1;
    fACConnected            = -1;
    fAvgCurrent             = 0;
    fInflowDisabled         = false;
    fRebootPolling          = false;
    fCellVoltages           = NULL;
    fSystemSleeping         = false;
    fPowerServiceToAck      = NULL;

    fIncompleteReadRetries = kIncompleteReadRetryMax;

    initializeCommands();
    
    // Make sure that we read battery state at least 5 times at 30 second intervals
    // after system boot.
    fInitialPollCountdown = kInitialPollCountdown;

    fWorkLoop = getWorkLoop();

    fBatteryReadAllTimer = IOTimerEventSource::timerEventSource(this,
                    OSMemberFunctionCast(IOTimerEventSource::Action,
                    this, &AppleSmartBattery::incompleteReadTimeOut));

    if (!fWorkLoop
      || (kIOReturnSuccess != fWorkLoop->addEventSource(fBatteryReadAllTimer)))
    {
        return false;
    }

    // Find an object of class IOACPIPlatformDevice in my parent's
    // IORegistry service plane ancsetry.
    fACPIProvider = NULL;
    p = this;
    while (p) {
        p = p->getParentEntry(gIOServicePlane);
        if (OSDynamicCast(IOACPIPlatformDevice, p)) {
            fACPIProvider = (IOACPIPlatformDevice *)p;
            break;
        }
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

    BattLog("AppleSmartBattery::start(). Initiating a full poll.\n");

    // Kick off the 30 second timer and do an initial poll
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
        {kTransactionRestart,       0, 0, 0, NULL,                              kBoot | kFull | kUserVis},
        {kMStateContCmd,            kMgr,  kWord, 0, NULL,                      kBoot | kFull | kUserVis},
        {kMStateCmd,                kMgr,  kWord, 0, NULL,                      kBoot | kFull | kUserVis},
        {kBBatteryStatusCmd,        kBatt, kWord, 0, NULL,                      kBoot | kFull | kUserVis},
        {kBExtendedPFStatusCmd,     kBatt, kWriteWord, 0, NULL,                 kBoot | kFull},
        {kStage2 | kBExtendedPFStatusCmd, kBatt, kWord, 0, _PFStatusSym,        kBoot | kFull},
        {kBExtendedOperationStatusCmd, kBatt, kWriteWord, 0, NULL,              kBoot | kFull},
        {kStage2 | kBExtendedOperationStatusCmd, kBatt, kWord, 0, _OpStatusSym, kBoot | kFull},
        {kBManufactureNameCmd,      kBatt, kBlock, 0, manufacturerKey,          kBoot},
        {kBManufactureDataCmd,      kBatt, kBlockData, 0, _ManufacturerDataSym, kBoot},
        {kBManufacturerInfoCmd,     kBatt, kBlock, 0, NULL,                     kBoot},
        {kBDeviceNameCmd,           kBatt, kBlock, 0, _DeviceNameSym,           kBoot},
        {kBAppleHardwareSerialCmd,  kBatt, kBlock, 0, _HardwareSerialSym,       kBoot},
        {kBPackReserveCmd,          kBatt, kWord, 0, _PackReserveSym,           kBoot},
        {kBDesignCycleCount9CCmd,   kBatt, kWord, 0, _DesignCycleCount9CSym,    kBoot},
        {kBManufactureDateCmd,      kBatt, kWord, 0, _ManfDateSym,              kBoot},
        {kBSerialNumberCmd,         kBatt, kWord, 0, _SerialNumberSym,          kBoot},
        {kBDesignCapacityCmd,       kBatt, kWord, 0, _DesignCapacitySym,        kBoot},
        {kBVoltageCmd,              kBatt, kWord, 0, voltageKey,                kBoot | kFull},
        {kBMaxErrorCmd,             kBatt, kWord, 0, _MaxErrSym,                kBoot | kFull},
        {kBCycleCountCmd,           kBatt, kWord, 0, cycleCountKey,             kBoot | kFull},
        {kBRunTimeToEmptyCmd,       kBatt, kWord, 0, _InstantTimeToEmptySym,    kBoot | kFull},
        {kBTemperatureCmd,          kBatt, kWord, 0, _TemperatureSym,           kBoot | kFull},
        {kBReadCellVoltage1Cmd,     kBatt, kWord, 0, NULL,                      kBoot | kFull},
        {kBReadCellVoltage2Cmd,     kBatt, kWord, 0, NULL,                      kBoot | kFull},
        {kBReadCellVoltage3Cmd,     kBatt, kWord, 0, NULL,                      kBoot | kFull},
        {kBReadCellVoltage4Cmd,     kBatt, kWord, 0, NULL,                      kBoot | kFull},
        {kBCurrentCmd,              kBatt, kWord, 0, NULL,                      kBoot | kFull | kUserVis},
        {kBAverageCurrentCmd,       kBatt, kWord, 0, NULL,                      kBoot | kFull | kUserVis},
        {kBAverageTimeToEmptyCmd,   kBatt, kWord, 0, NULL,                      kBoot | kFull | kUserVis},
        {kBAverageTimeToFullCmd,    kBatt, kWord, 0, NULL,                      kBoot | kFull | kUserVis},
        {kBRemainingCapacityCmd,    kBatt, kWord, 0, NULL,                      kBoot | kFull | kUserVis},
        {kBFullChargeCapacityCmd,   kBatt, kWord, 0, maxCapacityKey,            kBoot | kFull | kUserVis},
        {kFinishPolling,            0, 0, 0, NULL,                              kBoot | kFull | kUserVis}
    };
    
    cmdTable.table = NULL;
    cmdTable.count = 0;
    
    if ((cmdTable.table = (CommandStruct *)IOMalloc(sizeof(local_cmd)))) {
        cmdTable.count = sizeof(local_cmd) / sizeof(CommandStruct);
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
bool AppleSmartBattery::initiateTransaction(const CommandStruct *cs, bool retry)
{
    uint32_t cmd = cs->cmd;

    if (cmd == kFinishPolling)
    {
        this->handlePollingFinished(true);
    }
    else if ((cmd == kBExtendedPFStatusCmd)
        || (cmd == kBExtendedOperationStatusCmd))
    {
        // Extended commands require a 2-stage write & read.
        writeWordAsync(cmd, cs->addr, kBManufacturerAccessCmd, cmd);
        goto command_started;
    }
    else if ((cmd == (kStage2 | kBExtendedPFStatusCmd))
        || (cmd == (kStage2 | kBExtendedOperationStatusCmd)))
    {
        readWordAsync(cmd, cs->addr, kBManufacturerAccessCmd);
        goto command_started;
    }
    else if (cs->protocol == kWord)
    {
        readWordAsync(cmd, cs->addr, cmd);
        goto command_started;
    } else if ((cs->protocol == kBlock)
            || (cs->protocol == kBlockData))
    {
        readBlockAsync(cmd, cs->addr, cmd);
        goto command_started;
    }

    return false;

command_started:
    return true;
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
            if (0 != (cmdTable.table[found_current_index].pathBits & fMachinePath))
            {
                cs = &cmdTable.table[found_current_index];
                break;
            }
        }
    }
    
    if (cs)
        return initiateTransaction(cs, false);
    
    return false;
}

/******************************************************************************
 * AppleSmartBattery::retryCurrentTransaction
 *
 ******************************************************************************/
bool AppleSmartBattery::retryCurrentTransaction(uint32_t state)
{
    int found_current_index = 0;
    const CommandStruct *cs = NULL;

    if (!cmdTable.table) {
        return false;
    }

    // Find index for "state" in cmd_machine
    for (found_current_index = 0; found_current_index < cmdTable.count; found_current_index++) {
        if (cmdTable.table[found_current_index].cmd == state) {
            cs = &cmdTable.table[found_current_index];
            break;
        }
    }
    
    if (cs)
        return initiateTransaction(cs, true);
    
    return false;
}

/******************************************************************************
 * AppleSmartBattery::logReadError
 *
 ******************************************************************************/
void AppleSmartBattery::logReadError(
    const char              *error_type,
    uint16_t                additional_error,
    IOSMBusTransaction      *t)
{

    if (!error_type) return;

    BattLog("SmartBatteryManager Error: %s (%d)\n", error_type, additional_error);
    if (t) {
        BattLog("\tCorresponding transaction addr=0x%02x cmd=0x%02x status=0x%02x\n",
                                            t->address, t->command, t->status);
    }

    return;
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
            if (fBatteryReadAllTimer) {
                fBatteryReadAllTimer->cancelTimeout();
            }
            ret = (kBatteryReadAllTimeout * 1000);
        }
    }
    else // System Wake
    {
        fPowerServiceToAck = powerService;
        fPowerServiceToAck->retain();
        pollBatteryState(kFull);

        if (fPollingNow)
        {
            // Transaction started, wait for completion.
            ret = (kBatteryReadAllTimeout * 1000);
        }
        else if (fPowerServiceToAck)
        {
            fPowerServiceToAck->release();
            fPowerServiceToAck = 0;
        }
    }

    BattLog("SmartBattery: handleSystemSleepWake(%d) = %u\n",
        isSystemSleep, (uint32_t) ret);
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
    /* Don't perform any SMBus activity if a AppleSmartBatteryManagerUserClient
       has grabbed exclusive access
     */
    if (fStalledByUserClient) {
        BattLog("AppleSmartBattery::pollBatteryState was stalled by an exclusive user client.\n");
        return false;
    }

    /*  kUseLastPath    = 0,
     *  kBoot           = 1,
     *  kFull           = 2,
     *  kUserVis        = 4
     */

    if (fPollingNow && (fMachinePath <= type)) {
        /* We're already in the middle of a poll for a superset of 
         * the requested battery data.
         */
         BattLog("AppleSmartBattery::pollBatteryState already polling (%d <= %d)\n", fMachinePath, type);
        return true;
    }

    if (type != kUseLastPath) {
        fMachinePath = type;
    }
    
    if (fInitialPollCountdown > 0) {
        // We're going out of our way to make sure that we get a successfull
        // initial poll at boot. Upgrade all early boot polls to kBoot.
        fMachinePath = kBoot;
    }
    
    if (!fPollingNow)
    {
        /* Start the battery polling state machine (resetting it if it's already in progress) */
        return transactionCompletion((void *)kTransactionRestart, NULL);
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
    clearBatteryState(false);
    pollBatteryState(kBoot);
    return;
}

void AppleSmartBattery::handleBatteryRemoved(void)
{
    if (fPollingNow) {
        fCancelPolling = true;
        if (fBatteryReadAllTimer) {
            fBatteryReadAllTimer->cancelTimeout();
        }
    }

    clearBatteryState(true);
    acknowledgeSystemSleepWake();
    return;
}

void AppleSmartBattery::handleInflowDisabled(bool inflow_state)
{
    fInflowDisabled = inflow_state;
    // And kick off a re-poll using this new information
    pollBatteryState(kFull);

    return;
}

void AppleSmartBattery::handleChargeInhibited(bool charge_state)
{
    fChargeInhibited = charge_state;
    // And kick off a re-poll using this new information
    pollBatteryState(kFull);
}

// One "wait interval" is 100ms
#define kWaitExclusiveIntervals 30

void AppleSmartBattery::handleExclusiveAccess(bool exclusive)
{
    /* Write exclusive access bit to SMC via ACPI registers
     *
     * #define ACPIIO_SMB_MODE       ACPIIO_SMB_BASE + 0x29
     * Mode bit masks: 0x01 - OS requests exclusive access to battery
     *                 0x10 - SMC acknowledges OS exclusive access to battery
     *                            (resets to non-exclusive (0) on warm restart
     *                              or leaving S0 or timeout)
     */

    IOACPIAddress       ecBehaviorsAddress;
    IOReturn            ret = kIOReturnSuccess;
    UInt64              value64 = 0;
    int                 waitCount = 0;

    // Shut off SMC hardware communications with the batteries
    do {

        if (!fACPIProvider)
            break;

        /* Read BYTE */
        // Register address is 0x29 + SMB base 0x20
        ecBehaviorsAddress.addr64 = 0x20 + 0x29;
        ret = fACPIProvider->readAddressSpace(&value64,
                        kIOACPIAddressSpaceIDEmbeddedController,
                        ecBehaviorsAddress, 8, 0, 0);
        if (kIOReturnSuccess != ret) {
            break;
        }

        /* Modify - set 0x01 to indicate the SMC should not communicate with battery*/
        if (exclusive) {
            value64 |= 1;
        } else {
            // Zero'ing out bit 0x01
            value64 &= ~1;
        }

        /* Write BYTE */
        ret = fACPIProvider->writeAddressSpace(value64,
                        kIOACPIAddressSpaceIDEmbeddedController,
                        ecBehaviorsAddress, 8, 0, 0);
        if (kIOReturnSuccess != ret) {
            break;
        }

        // Wait up to 3 seconds for the SMC to set/clear bit 0x10
        // As-implemented, this waits at least 100 msec before proceeding.
        // That is OK - this is a very infrequent code path.

        waitCount = 0;
        value64 = 0;
        ret = kIOReturnSuccess;
        while ((waitCount < kWaitExclusiveIntervals)
            && (kIOReturnSuccess == ret))
        {
            waitCount++;
            IOSleep(100);

            ret = fACPIProvider->readAddressSpace(&value64,
                            kIOACPIAddressSpaceIDEmbeddedController,
                            ecBehaviorsAddress, 8, 0, 0);

            if (exclusive) {
                // wait for 0x10 bit to set
                if ((value64 & 0x10) == 0x10)
                    break;
            } else {
                // wait for 0x10 bit to clear
                if ((value64 & 0x10) == 0)
                    break;
            }
        }
    } while (0);


    if (exclusive)
    {
        // Communications with battery have been shutdown for
        // exclusive access by the user client.
        setProperty("BatteryUpdatesBlockedExclusiveAccess", true);
        fStalledByUserClient = true;

    } else {
        // Exclusive access disabled! restart polling
        removeProperty("BatteryUpdatesBlockedExclusiveAccess");
        fStalledByUserClient = false;

        // Restore battery state
        // Do a complete battery poll
        pollBatteryState(kFull);
    }
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
    logReadError(kErrorOverallTimeoutExpired, 0, NULL);

    /* Don't launch infinite re-tries if the system isn't completing my transactions
     *  (and thus probably leaking a lot of memory every time.
     *  Quit after kIncompleteReadRetryMax
     */
    if (0 < fIncompleteReadRetries)
    {
        fIncompleteReadRetries--;
        pollBatteryState(kUseLastPath);
    }
}

bool AppleSmartBattery::transactionCompletion_shouldAbortTransactions(IOSMBusTransaction *transaction)
{
    /* Stop battery work when system is going to sleep.
     */
    if (fSystemSleeping)
    {
        return true;
    }

    /* If a user client has exclusive access to the SMBus,
     * we'll exit immediately.
     */
    if (fStalledByUserClient)
    {
        return true;
    }

    /* Do we need to abort an ongoing polling session?
     Example: If a battery has just been removed in the midst of our polling, we
     need to abort the remainder of our scheduled SMBus reads.

     We do not abort newly started polling sessions where (NULL == transaction).
     */
    if (fCancelPolling)
    {
        fCancelPolling = false;
        if (transaction)
        {
            return true;
        }
    }
    return false;
}

uint32_t AppleSmartBattery::transactionCompletion_requiresRetryGetMicroSec(IOSMBusTransaction *transaction)
{
    IOSMBusStatus       transaction_status = kIOSMBusStatusPECError;
    bool                transaction_needs_retry = false;

    if (transaction)
        transaction_status = transaction->status;
    else
        return 0;

    /******************************************************************************************
     ******************************************************************************************/

    /* If the last transaction wasn't successful at the SMBus level, retry.
     */
    if (STATUS_ERROR_NEEDS_RETRY(transaction_status))
    {
        transaction_needs_retry = true;
    } else if (STATUS_ERROR_NON_RECOVERABLE(transaction_status))
    {
        transaction_needs_retry = false;
        logReadError(kErrorNonRecoverableStatus, transaction_status, transaction);
        goto exit;
    }

    /******************************************************************************************
     ******************************************************************************************/

    if (kIOSMBusStatusOK == transaction_status)
    {
        if (0 != fRetryAttempts) {
            BattLog("SmartBattery: retry %d succeeded!\n", fRetryAttempts);

            fRetryAttempts = 0;
            transaction_needs_retry = false;    /* potentially overridden below */
        }

        /* Check for absurd return value for RemainingCapacity or FullChargeCapacity.
         If the returned value is zero, re-read until it's non-zero (or until we
         try too many times).

         (FullChargeCapacity = 0) is NOT a valid state
         (DesignCapacity = 0) is NOT a valid state
         (RemainingCapacity = 0) is a valid state
         (RemainingCapacity = 0) && !fFullyDischarged is NOT a valid state
         */
        if (((kBFullChargeCapacityCmd == transaction->command)
             || (kBDesignCapacityCmd == transaction->command)
             || ((kBRemainingCapacityCmd == transaction->command)
                 && !fFullyDischarged))
           && ((transaction->receiveData[1] == 0)
               && (transaction->receiveData[0] == 0)))
        {
            transaction_needs_retry = true;
        }
    }

    /* Too many retries already?
     */
    if (transaction_needs_retry && (kRetryAttempts == fRetryAttempts))
    {
        // Too many consecutive failures to read this entry. Give up, and
        // go on to attempt a read on the next element in the state machine.

        BattLog("SmartBattery: Giving up on (0x%02x, 0x%02x) after %d retries.\n",
                transaction->address, transaction->command, fRetryAttempts);

        logReadError(kErrorRetryAttemptsExceeded, transaction_status, transaction);

        fRetryAttempts = 0;

        transaction_needs_retry = false;

        // After too many retries, unblock PM state machine in case it is
        // waiting for the first battery poll after wake to complete,
        // avoiding a setPowerState timeout.
        acknowledgeSystemSleepWake();

        goto exit;
    }

exit:
    if (transaction_needs_retry)
    {
        fRetryAttempts++;
        return microSecDelayTable[fRetryAttempts];
    } else {
        return 0;
    }
}

void AppleSmartBattery::handlePollingFinished(bool visitedEntirePath)
{
    uint64_t now, nsec;

    if (fBatteryReadAllTimer) {
        fBatteryReadAllTimer->cancelTimeout();
    }

    const char *reportPathFinishedKey;
    if (kBoot == fMachinePath) {
        reportPathFinishedKey = kBootPathKey;
    } else if (kFull == fMachinePath) {
        reportPathFinishedKey = kFullPathKey;
    } else if (kUserVis == fMachinePath) {
        reportPathFinishedKey = kUserVisPathKey;
    } else {
        reportPathFinishedKey = NULL;
    }
    
    if (reportPathFinishedKey) {
        clock_sec_t secs;
        clock_usec_t microsecs;
        clock_get_calendar_microtime(&secs, &microsecs);
        setProperty(reportPathFinishedKey, secs, 32);
    }
    
    if (visitedEntirePath) {
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
        updateStatus();
    }

    fPollingNow = false;
    
    acknowledgeSystemSleepWake();
}


bool AppleSmartBattery::handleSetItAndForgetIt(int state, int val16, const uint8_t *str32, uint32_t len)
{
    CommandStruct   *this_command = NULL;
    const OSData    *publishData;
    const OSSymbol  *publishSym;
    OSNumber        *val16num = NULL;
    
    /* Set it and forget it
     *
     * These commands specify an OSSymbol in their CommandStruct.
     * We directly publish the data into these registry keys.
     */
    if ((this_command = commandForState(state)) && this_command->setItAndForgetItSym)
    {
        if (this_command->protocol == kWord) {
            val16num = OSNumber::withNumber(val16, 16);
            if (val16num) {
                setPSProperty(this_command->setItAndForgetItSym, val16num);
                val16num->release();
            }
            return true;
        }
        
        else if (this_command->protocol == kBlock) {
            publishSym = OSSymbol::withCString((const char *)str32);
            if (publishSym) {
                setPSProperty(this_command->setItAndForgetItSym, (OSObject *)publishSym);
                publishSym->release();
                return true;
            }
        }
        else if (this_command->protocol == kBlockData) {
            publishData = OSData::withBytes((const void *)str32, len);
            if (publishData) {
                setPSProperty(this_command->setItAndForgetItSym, (OSObject *)publishData);
                publishData->release();
                return true;
            }
        }
    }
    
    return false;
}

/******************************************************************************
 * AppleSmartBattery::transactionCompletion
 * -> Runs in workloop context
 *
 ******************************************************************************/

bool AppleSmartBattery::transactionCompletion(
    void *ref,
    IOSMBusTransaction *transaction)
{
    IOSMBusStatus   transaction_status = kIOSMBusStatusPECError;
    bool            transaction_success = false;
    int             next_state = (uintptr_t)ref;
    uint16_t        val16 = 0;
    uint32_t        delay_for = 0;
    OSNumber        *num = NULL;
    

    if (transaction) {
        BattLog("transaction state = 0x%02x; status = 0x%02x; prot = 0x%02x; word = 0x%04x; %d us\n",
                next_state, transaction->status, transaction->protocol,
                (transaction->receiveData[1] << 8) | transaction->receiveData[0]);
    }

    // Abort?
    if (transactionCompletion_shouldAbortTransactions(transaction)) {
        goto abort;
    }

    // Restart?
    if (!transaction || fRebootPolling)
    {
        // NULL argument for transaction means we should start the state machine from scratch.
        transaction = NULL;
        next_state = kTransactionRestart;
        fRebootPolling = false;
    }
    
    if (transaction)
    {
        // Retry?
        delay_for = this->transactionCompletion_requiresRetryGetMicroSec(transaction);
        if (0 != delay_for)
        {
            // The transaction failed. We'll delay for a bit, then retry the transaction.
            if (delay_for < 1000) {
                IODelay(delay_for); // microseconds
            } else {
                IOSleep(delay_for / 1000); // milliseconds
            }
            
            BattLog("SmartBattery: 0x%02x failed with 0x%02x; retry attempt %d of %d\n",
                    transaction->command, transaction_status, fRetryAttempts, kRetryAttempts);
            
            // Kick off the same transaction that just failed
            retryCurrentTransaction(next_state);
            return true; // not exit/abort
        }

        transaction_success = (kIOSMBusStatusOK == transaction->status);
        if (transaction_success) {
            val16 = (transaction->receiveData[1] << 8) | transaction->receiveData[0];
        }

        // Is it a set it and forget it command?
        if (handleSetItAndForgetIt(next_state, val16, transaction->receiveData,
                                   transaction->receiveDataCount))
        {
            goto exit;
        }
    }
    
    switch(next_state)
    {
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
            bool new_ac_connected = (!fInflowDisabled && (val16 & kMACPresentBit)) ? 1:0;

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
            setExternalConnected(fACConnected);
            setExternalChargeCapable((val16 & kMPowerNotGoodBit) ? false:true);

        } else {
            fACConnected = false;
            setExternalConnected(true);
            setExternalChargeCapable(false);
        }
        break;

    case kMStateCmd:
            
        // Determines if battery is present
        // Determines if battery is charging
        if (transaction_success)
        {
            fBatteryPresent = (val16 & kMPresentBatt_A_Bit) ? true : false;

            setBatteryInstalled(fBatteryPresent);

            // If fChargeInhibit is currently set, then we acknowledge
            // our lack of charging and force the "isCharging" bit to false.
            //
            // charge inhibit means the battery will not charge, even if
            // AC is attached.
            // Without marking this lack of charging here, it can take
            // up to 30 seconds for the charge disable to be reflected in
            // the UI.

            setIsCharging((!fChargeInhibited && (val16 & kMChargingBatt_A_Bit)) ? true:false);
        } else {
            fBatteryPresent = false;
            setBatteryInstalled(false);
            setIsCharging(false);
        }

        break;

    case kBBatteryStatusCmd:
            
        if (!transaction_success)
        {
            fFullyCharged = false;
            fFullyDischarged = false;
        } else {

            if (val16 & kBFullyChargedStatusBit) {
                fFullyCharged = true;
            } else {
                fFullyCharged = false;
            }

            if (val16 & kBFullyDischargedStatusBit)
            {
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
            if ((val16
                & (kBTerminateDischargeAlarmBit | kBTerminateChargeAlarmBit))
                == (kBTerminateDischargeAlarmBit | kBTerminateChargeAlarmBit))
            {
                logReadError(kErrorPermanentFailure, 0, transaction);
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
            clearBatteryState(true);
            goto abort;
        }

        break;

    case kBRemainingCapacityCmd:

        fRemainingCapacity = val16;
        setCurrentCapacity(val16);

        if (!fPermanentFailure && (0 == fRemainingCapacity))
        {
            // fRemainingCapacity == 0 is an absurd value.
            // We have already retried several times, so we accept this value and move on.
            logReadError(kErrorZeroCapacity, kBRemainingCapacityCmd, transaction);
        }
        break;

    /* *Instant* current */
    case kBCurrentCmd:
        if ((num = OSNumber::withNumber(val16, 16))) {
            setPSProperty(_InstantAmperageSym, num);
            num->release();
        }
        fInstantCurrent = (int)(int16_t)val16;
        break;
            
    /* Average current */
    case kBAverageCurrentCmd:
        setAmperage((int16_t)val16);
        fAvgCurrent = (int16_t)val16;
        if (0 == fAvgCurrent) {
            // Battery not present, or fully charged, or general error
            setTimeRemaining(0);
        }
        break;

    case kBAverageTimeToEmptyCmd:

        setAverageTimeToEmpty(val16);
        
        if (fInstantCurrent < 0) {
            setTimeRemaining(val16);
        }
        break;
        
    case kBAverageTimeToFullCmd:

        setAverageTimeToFull(val16);
        
        if (fInstantCurrent > 0) {
            setTimeRemaining(val16);
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
            num = OSNumber::withNumber(val16, 16);
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

    default:
        BattLog("SmartBattery: Error state %x not expected\n", next_state);
    }

exit:
    /* Kick off the next transaction */
    if (kFinishPolling != next_state) {
        this->initiateNextTransaction(next_state);
    }
    return true;

abort:
    handlePollingFinished(false);
    return true;
}


void AppleSmartBattery::clearBatteryState(bool do_update)
{
    // Only clear out battery state; don't clear manager state like AC Power.
    // We just zero out the int and bool values, but remove the OSType values.

    fRetryAttempts          = 0;
    fFullyDischarged        = false;
    fFullyCharged           = false;
    fBatteryPresent         = false;
    fACConnected            = -1;
    fAvgCurrent             = 0;

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

    logReadError(kErrorClearBattery, 0, NULL);

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
}


/******************************************************************************
 *  Power Source value accessors
 *  These supplement the built-in accessors in IOPMPowerSource.h.
 ******************************************************************************/

#define CLASS   AppleSmartBattery

void AppleSmartBattery::setAverageTimeToEmpty(int seconds) {
    OSNumber *n = OSNumber::withNumber(seconds, 32);
    if (n) {
        setPSProperty(_AvgTimeToEmptySym, n);
        n->release();
    }
}
void AppleSmartBattery::setAverageTimeToFull(int seconds) {
    OSNumber *n = OSNumber::withNumber(seconds, 32);
    if (n) {
        setPSProperty(_AvgTimeToFullSym, n);
        n->release();
    }
}

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


/******************************************************************************
 ******************************************************************************
 **
 **  Async SmartBattery read convenience functions
 **
 ******************************************************************************
 ******************************************************************************/
IOReturn AppleSmartBattery::readWordAsync(
    uint32_t refnum,
    uint8_t address,
    uint8_t cmd
) {
    IOReturn                ret = kIOReturnError;
    bzero(&fTransaction, sizeof(IOSMBusTransaction));

    // All transactions are performed async
    fTransaction.protocol      = kIOSMBusProtocolReadWord;
    fTransaction.address       = address;
    fTransaction.command       = cmd;

    ret = fProvider->performTransaction(
                    &fTransaction,
                    OSMemberFunctionCast(IOSMBusTransactionCompletion,
                      this, &AppleSmartBattery::transactionCompletion),
                    (OSObject *)this,
                    (void *)(uintptr_t)refnum);

    return ret;
}

IOReturn AppleSmartBattery::writeWordAsync(
    uint32_t refnum,
    uint8_t address,
    uint8_t cmd,
    uint16_t writeWord)
{
    IOReturn                ret = kIOReturnError;
    bzero(&fTransaction, sizeof(IOSMBusTransaction));

    // All transactions are performed async
    fTransaction.protocol      = kIOSMBusProtocolWriteWord;
    fTransaction.address       = address;
    fTransaction.command       = cmd;
    fTransaction.sendData[0]   = writeWord & 0xFF;
    fTransaction.sendData[1]   = (writeWord >> 8) & 0xFF;
    fTransaction.sendDataCount = 2;

    ret = fProvider->performTransaction(
                    &fTransaction,
                    OSMemberFunctionCast(IOSMBusTransactionCompletion,
                      this, &AppleSmartBattery::transactionCompletion),
                    (OSObject *)this,
                    (void *)((uintptr_t)refnum));

    return ret;

}

IOReturn AppleSmartBattery::readBlockAsync(
    uint32_t refnum,
    uint8_t address,
    uint8_t cmd
) {
    IOReturn                ret = kIOReturnError;
    bzero(&fTransaction, sizeof(IOSMBusTransaction));

    // All transactions are performed async
    fTransaction.protocol      = kIOSMBusProtocolReadBlock;
    fTransaction.address       = address;
    fTransaction.command       = cmd;

    ret = fProvider->performTransaction(
                    &fTransaction,
                    OSMemberFunctionCast(IOSMBusTransactionCompletion,
                      this, &AppleSmartBattery::transactionCompletion),
                    (OSObject *)this,
                    (void *)(uintptr_t)refnum);

    return ret;
}

