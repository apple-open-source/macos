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
#include "AppleSmartBatteryManager.h"
#include "AppleSmartBattery.h"

// Defines the order of reading properties in the power source state machine
// Bitfield
enum {
    kExistingBatteryPath    = 1,
    kNewBatteryPath         = 2
};

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


enum {
    kDefaultPollInterval = 0,
    kQuickPollInterval = 1
};

// This bit lets us distinguish between reads & writes in the transactionCompletion switch statement
#define kWriteIndicatorBit                  0x8000

// Argument to transactionCompletion indicating we should start/re-start polling
#define kTransactionRestart                 0xFFFFFFFF

#define kErrorRetryAttemptsExceeded         "Read Retry Attempts Exceeded"
#define kErrorOverallTimeoutExpired         "Overall Read Timeout Expired"
#define kErrorZeroCapacity                  "Capacity Read Zero"
#define kErrorPermanentFailure              "Permanent Battery Failure"
#define kErrorNonRecoverableStatus          "Non-recoverable status failure"
#define kErrorClearBattery                  "Clear Battery"

// Polling intervals
// The battery kext switches between polling frequencies depending on
// battery load
static uint32_t milliSecPollingTable[2] =
    {
      30000,    // 0 == Regular 30 second polling
      1000      // 1 == Quick 1 second polling
    };

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
static const OSSymbol *_MaxErrSym =
                        OSSymbol::withCString(kIOPMPSMaxErrKey);
static const OSSymbol *_DeviceNameSym =
                        OSSymbol::withCString(kIOPMDeviceNameKey);
static const OSSymbol *_FullyChargedSym =
                        OSSymbol::withCString(kIOPMFullyChargedKey);
static const OSSymbol *_AvgTimeToEmptySym =
                        OSSymbol::withCString("AvgTimeToEmpty");
static const OSSymbol *_InstantTimeToEmptySym =
                        OSSymbol::withCString("InstantTimeToEmpty");
static const OSSymbol *_InstantAmperageSym =
                        OSSymbol::withCString("InstantAmperage");
static const OSSymbol *_AvgTimeToFullSym =
                        OSSymbol::withCString("AvgTimeToFull");
static const OSSymbol *_ManfDateSym =
                        OSSymbol::withCString(kIOPMPSManufactureDateKey);
static const OSSymbol *_DesignCapacitySym =
                        OSSymbol::withCString(kIOPMPSDesignCapacityKey);
//static const OSSymbol *_TemperatureSym =
//                        OSSymbol::withCString("Temperature");
static const OSSymbol *_CellVoltageSym =
                        OSSymbol::withCString("CellVoltage");
static const OSSymbol *_ManufacturerDataSym =
                        OSSymbol::withCString("ManufacturerData");
static const OSSymbol *_PFStatusSym =
                        OSSymbol::withCString("PermanentFailureStatus");

/* _SerialNumberSym represents the manufacturer's 16-bit serial number in
    numeric format.
 */
static const OSSymbol *_SerialNumberSym =
                        OSSymbol::withCString("FirmwareSerialNumber");

/* _HardwareSerialSym == AppleSoftwareSerial
   represents the Apple-defined 12+ character string in firmware
 */
static const OSSymbol *_HardwareSerialSym =
                      OSSymbol::withCString("BatterySerialNumber");

/* _ChargeStatusSym tracks any irregular charging patterns in the battery
    that might cause it to stop charging mid-charge.
 */
// TODO: Delete these local definitions sync'd with OS (rdar://5608255)
#ifndef kIOPMPSBatteryChargeStatusKey
    #define kIOPMPSBatteryChargeStatusKey               "ChargeStatus"
    #define kIOPMBatteryChargeStatusTooHot              "HighTemperature"
    #define kIOPMBatteryChargeStatusTooCold             "LowTemperature"
    #define kIOPMBatteryChargeStatusGradient            "BatteryTemperatureGradient"
#endif

static const OSSymbol *_ChargeStatusSym =
                      OSSymbol::withCString(kIOPMPSBatteryChargeStatusKey);

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
    fPollTimer = NULL;

    return true;
}


/******************************************************************************
 * AppleSmartBattery::start
 *
 ******************************************************************************/

bool AppleSmartBattery::start(IOService *provider)
{
    IORegistryEntry *p = NULL;
    OSNumber        *debugPollingSetting;

    BattLog("AppleSmartBattery loading...\n");

    fProvider = OSDynamicCast(AppleSmartBatteryManager, provider);

    if (!fProvider || !super::start(provider)) {
        return false;
    }

    debugPollingSetting = (OSNumber *)fProvider->getProperty(kBatteryPollingDebugKey);
    if (debugPollingSetting && OSDynamicCast(OSNumber, debugPollingSetting))
    {
        /* We set our polling interval to the "BatteryPollingPeriodOverride" property's value,
            in seconds.
            Polling Period of 0 causes us to poll endlessly in a loop for testing.
         */
        fPollingInterval = debugPollingSetting->unsigned32BitValue();
        fPollingOverridden = true;
    } else {
        fPollingInterval = kDefaultPollInterval;
        fPollingOverridden = false;
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

    // Make sure that we read battery state at least 5 times at 30 second intervals
    // after system boot.
    fInitialPollCountdown = kInitialPollCountdown;

    fWorkLoop = getWorkLoop();

    fPollTimer = IOTimerEventSource::timerEventSource(this,
                    OSMemberFunctionCast(IOTimerEventSource::Action,
                    this, &AppleSmartBattery::pollingTimeOut));

    fBatteryReadAllTimer = IOTimerEventSource::timerEventSource(this,
                    OSMemberFunctionCast(IOTimerEventSource::Action,
                    this, &AppleSmartBattery::incompleteReadTimeOut));

    if (!fWorkLoop || !fPollTimer
      || (kIOReturnSuccess != fWorkLoop->addEventSource(fPollTimer))
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


    // **** Should occur on workloop
    // zero out battery state with argument (do_update == true)
    clearBatteryState(false);

    // **** Should occur on workloop
    BattLog("AppleSmartBattery polling battery data.\n");
    // Kick off the 30 second timer and do an initial poll
    pollBatteryState(kNewBatteryPath);

    return true;
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
            fPollTimer->cancelTimeout();
            fBatteryReadAllTimer->cancelTimeout();
            ret = (kBatteryReadAllTimeout * 1000);
        }
    }
    else // System Wake
    {
        fPowerServiceToAck = powerService;
        fPowerServiceToAck->retain();
        pollBatteryState(kExistingBatteryPath);

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
 * AppleSmartBattery::setPollingInterval
 *
 ******************************************************************************/
void AppleSmartBattery::setPollingInterval(
    int milliSeconds)
{
    if (!fPollingOverridden) {
        milliSecPollingTable[kDefaultPollInterval] = milliSeconds;
        fPollingInterval = kDefaultPollInterval;
    }
}

/******************************************************************************
 * AppleSmartBattery::pollBatteryState
 *
 * Asynchronously kicks off the register poll.
 ******************************************************************************/

bool AppleSmartBattery::pollBatteryState(int path)
{
    /* Don't perform any SMBus activity if a AppleSmartBatteryManagerUserClient
       has grabbed exclusive access
     */
    if (fStalledByUserClient)
    {
        return false;
    }

    // This must be called under workloop synchronization
    fMachinePath = path;

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
    // Let the loop start again
    clearBatteryState(false);

    // This must be called under workloop synchronization
    pollBatteryState(kNewBatteryPath);

    return;
}

void AppleSmartBattery::handleBatteryRemoved(void)
{
    /* Removed battery means cancel any ongoing polling session */
    if (fPollingNow) {
        fCancelPolling = true;
        fPollTimer->cancelTimeout();
        fBatteryReadAllTimer->cancelTimeout();
    }

    // This must be called under workloop synchronization
    clearBatteryState(true);
    acknowledgeSystemSleepWake();

    return;
}

void AppleSmartBattery::handleInflowDisabled(bool inflow_state)
{
    fInflowDisabled = inflow_state;
    // And kick off a re-poll using this new information
    pollBatteryState(kExistingBatteryPath);

    return;
}

void AppleSmartBattery::handleChargeInhibited(bool charge_state)
{
    fChargeInhibited = charge_state;
    // And kick off a re-poll using this new information
    pollBatteryState(kExistingBatteryPath);
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
        pollBatteryState(kNewBatteryPath);
    }
}


/******************************************************************************
 * pollingTimeOut
 *
 * Regular 30 second poll expiration handler.
 ******************************************************************************/

void AppleSmartBattery::pollingTimeOut(void)
{
    // Timer will be re-enabled from the battery polling routine.
    // Timer will not be kicked off again if battery is plugged in and
    // fully charged.
    if (fPollingNow)
        return;


    if (fInitialPollCountdown > 0)
    {
        // At boot time we make sure to re-read everything kInitialPoltoCountdown times
        pollBatteryState(kNewBatteryPath);
    } else {
        pollBatteryState(kExistingBatteryPath);
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

        // Re-start
        pollBatteryState(kNewBatteryPath);
    }
}

bool AppleSmartBattery::transactionCompletion_shouldAbortTransactions(IOSMBusTransaction *transaction)
{
    /* Stop battery work when system is going to sleep.
     */
    if (fSystemSleeping)
    {
        fPollingNow = false;
        acknowledgeSystemSleepWake();
        return true;
    }

    /* If a user client has exclusive access to the SMBus,
     * we'll exit immediately.
     */
    if (fStalledByUserClient)
    {
        fPollingNow = false;
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
            fPollingNow = false;
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

            BattLog("SmartBatteryManager: retrying command 0x%02x; retry due to absurd value _zero_\n", transaction->command);
            transaction_needs_retry = true;

            goto exit;
        }

        /* EVENTUAL SUCCESS after zero capacity retry */
        if (((kBRemainingCapacityCmd == transaction->command)
             || (kBDesignCapacityCmd == transaction->command)
             || (kBFullChargeCapacityCmd == transaction->command))
           && ((transaction->receiveData[1] != 0)
               || (transaction->receiveData[0] != 0))
           && (0 != fRetryAttempts))
        {
            BattLog("SmartBatteryManager: Successfully read %d on retry %d\n",
                    transaction->command, fRetryAttempts);
            fRetryAttempts = 0;
            transaction_needs_retry = false;

            goto exit;
        }
    }

    /*******************************************************************************************
     ******************************************************************************************/

    /* Too many retries already?
     */
    if (transaction_needs_retry
       && (kRetryAttempts == fRetryAttempts))
    {
        // Too many consecutive failures to read this entry. Give up, and
        // go on to attempt a read on the next element in the state machine.
        // ** These two setProperty lines are here purely for debugging. **
        //setProperty("LastBattReadError", transaction_status, 16);
        //setProperty("LastBattReadErrorCmd", transaction->command, 16);

        BattLog("SmartBatteryManager: Giving up on retries\n");
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

void AppleSmartBattery::transactionCompletion_handlePollingFinished(void)
{
    /* Cancel read-completion timeout; Successfully read battery state */
    fBatteryReadAllTimer->cancelTimeout();

    rebuildLegacyIOBatteryInfo();

    updateStatus();

    fPollingNow = false;
    acknowledgeSystemSleepWake();

    /* fPollingInterval == 0 --> debug mode; never cease polling.
     * Begin a new poll when the last one ended.
     * Can consume 40-60% CPU on a 2Ghz MacBook Pro when set */
    if (fPollingOverridden && (fPollingInterval==0))
    {
        /* diabolical. Never stop polling battery state. */
        pollBatteryState(kNewBatteryPath);
        return;
    }

    /* Re-arm 30 second timer only if the batteries are
     * not fully charged.
     *  - Always poll at least kInitialPollCountdown times on boot.
     *  - Always poll if fPollingOveridden
     *  Otherwise:
     *  - Poll when AC is not connected.
     *  - Do not poll when fully charged
     *  - Do not poll on battery permanent failure
     */

    if (!fPermanentFailure)
    {
        if ((fInitialPollCountdown > 0)
            || fPollingOverridden
            || !fACConnected
            || (!fFullyCharged && fBatteryPresent))
        {
            if (fInitialPollCountdown > 0) {
                fInitialPollCountdown--;
            }

            if (!fPollingOverridden)
            {
                /* Restart timer with standard polling interval */
                fPollTimer->setTimeoutMS(milliSecPollingTable[fPollingInterval]);
            } else {
                /* restart timer with debug value */
                fPollTimer->setTimeoutMS(1000 * fPollingInterval);
            }

        } else {
            // We'll let the polling timer expire.
            // Right now we're neither charging nor discharging. We'll start the timer again
            // when we get an alarm on AC plug or unplug.
            BattLog("SmartBattery: letting timeout expire.\n");
        }
    }
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
    int             next_state = (uintptr_t)ref;
    char            recv_str[kIOSMBusMaxDataCount+1];
    uint16_t        val16 = 0;
    bool            transaction_success = false;
    OSNumber        *num = NULL;

    /*
     * Abort an in-progress poll if system is on its way to sleep,
     * or we need to restart the poll, or if we've cancelled it for any reason.
     */
    if (this->transactionCompletion_shouldAbortTransactions(transaction)) {
        return true;
    }

    /*
     * Restart polling
     */
    if (!transaction || fRebootPolling)
    {
        // NULL argument for transaction means we should start
        // the state machine from scratch. Zero is the start state.
        transaction = NULL;
        next_state = kTransactionRestart;
        fRebootPolling = false;
    }

    /*
     * Retry a failed transaction
     */
    if (transaction)
    {
        uint32_t delay_for = 0;

        transaction_status = transaction->status;

        BattLog("transaction state = 0x%02x; status = 0x%02x; word = %02x.%02x\n",
                next_state, transaction->status,
                transaction->receiveData[1], transaction->receiveData[0]);

        delay_for = this->transactionCompletion_requiresRetryGetMicroSec(transaction);

        if (0 != delay_for)
        {
            // The transaction failed. We'll delay for a bit,
            // then retry the transaction.

            if (delay_for < 1000) {
                IODelay(delay_for); // microseconds
            } else {
                IOSleep(delay_for / 1000); // milliseconds
            }

            BattLog("SmartBattery: 0x%02x failed with 0x%02x; retry attempt %d of %d\n",
                    transaction->command, transaction_status, fRetryAttempts, kRetryAttempts);

            // Kick off the same transaction that just failed
            readWordAsync(transaction->address, transaction->command);

            return true;
        }
    }

    /***********************************************************************************************
     **********************************************************************************************/


    transaction_success = (kIOSMBusStatusOK == transaction_status);

    if (transaction_success) {
        val16 = (transaction->receiveData[1] << 8) | transaction->receiveData[0];
    } else {
        val16 = 0;
    }

    switch(next_state)
    {
        case kTransactionRestart:

            /* Cancel polling timer in case this round of reads was initiated
               by an alarm. We re-set the 30 second poll later. */
            fPollTimer->cancelTimeout();

            fCancelPolling = false;
            fPollingNow = true;

            /* Initialize battery read timeout to catch any longstanding stalls. */
            fBatteryReadAllTimer->cancelTimeout();
            fBatteryReadAllTimer->setTimeoutMS(kBatteryReadAllTimeout);

            readWordAsync(kSMBusManagerAddr, kMStateContCmd);

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
                if (rd && (new_ac_connected != fACConnected))
                {
                    if (new_ac_connected) {
                        rd->receivePowerNotification(kIOPMSetACAdaptorConnected | kIOPMSetValue);
                    } else {
                        rd->receivePowerNotification(kIOPMSetACAdaptorConnected);
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

            readWordAsync(kSMBusManagerAddr, kMStateCmd);

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

            /* Whether we detect a battery is present in the system or not,
               We are going to go ahead and read the battery status so we can
               detect a permanent battery failure.
             */
            readWordAsync(kSMBusBatteryAddr, kBBatteryStatusCmd);
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
                    const OSSymbol *permanentFailureSym = OSSymbol::withCString(kErrorPermanentFailure);

                    logReadError(kErrorPermanentFailure, 0, transaction);
                    setErrorCondition((OSSymbol *)permanentFailureSym);
                    permanentFailureSym->release();

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
                fPollingNow = false;
                clearBatteryState(true);
                return true;
            }

            writeWordAsync(kSMBusBatteryAddr, kBManufacturerAccessCmd, kBExtendedPFStatusCmd);

            break;

        /************ write battery permanent failure status register ****************/
        case (kWriteIndicatorBit | kBManufacturerAccessCmd):
            if (transaction_success)
            {
            // We successfully wrote 0x53 to register 0x0; meaning we can now
            // read from register 0x0 to get out the permanent failure status.
                readWordAsync(kSMBusBatteryAddr, kBManufacturerAccessCmd);
            }

            break;

        /************ Finish read battery permanent failure status register ****************/
        case kBManufacturerAccessCmd:

            if(transaction_status)
                num = OSNumber::withNumber((unsigned long long)val16, (int)32);
            else
                num = OSNumber::withNumber((unsigned long long)0, (int)32);

            if (num)
            {
                setPSProperty(_PFStatusSym, num);
                num->release();
            }

            // The battery read state machine may fork at this stage.
            if (kNewBatteryPath == fMachinePath) {
                /* Following this path reads:
                 manufacturer info; serial number; device name; design capacity; etc.
                 This path re-joins the main path at RemainingCapacity.
                 */
                readBlockAsync(kSMBusBatteryAddr, kBManufactureNameCmd);
            } else {
                /* This path continues reading the normal battery settings that change during regular use.
                 Implies (fMachinePath == kExistingBatteryPath)
                 */
                readWordAsync(kSMBusBatteryAddr, kBRemainingCapacityCmd);
            }

            break;

        /************ Only executed in ReadForNewBatteryPath ****************/
        case kBManufactureNameCmd:
            if (transaction_success)
            {
                if (0 != transaction->receiveDataCount)
                {
                    const OSSymbol *manf_sym;

                    bzero(recv_str, sizeof(recv_str));
                    bcopy(transaction->receiveData, recv_str, transaction->receiveDataCount);

                    manf_sym = OSSymbol::withCString(recv_str);
                    if (manf_sym) {
                        setManufacturer((OSSymbol *)manf_sym);
                        manf_sym->release();
                    }
                }
            } else {
                properties->removeObject(manufacturerKey);
            }
            readBlockAsync(kSMBusBatteryAddr, kBManufactureDataCmd);

        break;

        /************ Only executed in ReadForNewBatteryPath ****************/
        case kBManufactureDataCmd:
            if (transaction_success)
            {
                if (0 != transaction->receiveDataCount)
                {
                    setManufacturerData(transaction->receiveData, transaction->receiveDataCount);
                }
            } else {
                properties->removeObject(_ManufacturerDataSym);
            }

            readWordAsync(kSMBusBatteryAddr, kBManufactureDateCmd);
        break;

        /************ Only executed in ReadForNewBatteryPath ****************/
        case kBManufactureDateCmd:
            /*
             * Date is published in a bitfield per the Smart Battery Data spec rev 1.1
             * in section 5.1.26
             *   Bits 0...4 => day (value 1-31; 5 bits)
             *   Bits 5...8 => month (value 1-12; 4 bits)
             *   Bits 9...15 => years since 1980 (value 0-127; 7 bits)
             */
            setManufactureDate(val16);

            readBlockAsync(kSMBusBatteryAddr, kBDeviceNameCmd);
            break;

        /************ Only executed in ReadForNewBatteryPath ****************/
        case kBDeviceNameCmd:

            if(transaction_success)
            {
                if(0 != transaction->receiveDataCount)
                {
                    const OSSymbol *device_sym;

                    bzero(recv_str, sizeof(recv_str));
                    bcopy(transaction->receiveData, recv_str, transaction->receiveDataCount);

                    device_sym = OSSymbol::withCString(recv_str);
                    if(device_sym) {
                        setDeviceName((OSSymbol *)device_sym);
                        device_sym->release();
                    }
                }
            } else {
                properties->removeObject(_DeviceNameSym);
            }

            readWordAsync(kSMBusBatteryAddr, kBSerialNumberCmd);
            break;

        /************ Only executed in ReadForNewBatteryPath ****************/
        case kBSerialNumberCmd:
            if (transaction_success)
            {
                /* Gets the firmware's 16-bit serial number out */
                setSerialNumber(val16);
            } else {
                properties->removeObject(_SerialNumberSym);
            }

            readBlockAsync(kSMBusBatteryAddr, kBAppleHardwareSerialCmd);
            break;

        /************ Only executed in ReadForNewBatteryPath ****************/
        case kBAppleHardwareSerialCmd:

            if( kIOSMBusStatusOK == transaction_status )
            {
                // Expect transaction->receiveData to contain a NULL-terminated
                // 12+ character ASCII string.
                const OSSymbol *serialSymbol = OSSymbol::withCString(
                                            (char *)transaction->receiveData);
                if (serialSymbol) {
                    setPSProperty(_HardwareSerialSym, (OSObject *)serialSymbol);
                    serialSymbol->release();
                }
            } else {
                properties->removeObject(_HardwareSerialSym);
            }

            readWordAsync(kSMBusBatteryAddr, kBDesignCapacityCmd);
            break;


        /************ Only executed in ReadForNewBatteryPath ****************/
        case kBDesignCapacityCmd:
            num = OSNumber::withNumber(val16, 16);
            if (num) {
                properties->setObject(_DesignCapacitySym, num);
                num->release();
            }

            readWordAsync(kSMBusBatteryAddr, kBRemainingCapacityCmd);
            break;

        /* ========== Back to our regularly scheduled battery reads ==========
         * The "new battery" reads re-join all battery regular battery reads here
         */
        case kBRemainingCapacityCmd:

            fRemainingCapacity = val16;
            setCurrentCapacity(val16);

            if (!fPermanentFailure && (0 == fRemainingCapacity))
            {
                // fRemainingCapacity == 0 is an absurd value.
                // We have already retried several times, so we accept this value and move on.
                logReadError(kErrorZeroCapacity, kBRemainingCapacityCmd, transaction);
            }

            readWordAsync(kSMBusBatteryAddr, kBFullChargeCapacityCmd);
            break;

        case kBFullChargeCapacityCmd:

            fFullChargeCapacity = val16;
            setMaxCapacity(val16);
            if (!fPermanentFailure &&  (0 == fFullChargeCapacity))
            {
                // FullChargeCapacity == 0 is an absurd value.
                logReadError(kErrorZeroCapacity, kBFullChargeCapacityCmd, transaction);

                // We have already retried several times, so we accept this value and move on.
            }

            readWordAsync(kSMBusBatteryAddr, kBAverageCurrentCmd);
            break;

        /* Average current */
        case kBAverageCurrentCmd:

            setAmperage((int16_t)val16);
            fAvgCurrent = (int16_t)val16;
            if (0 == fAvgCurrent) {
                // Battery not present, or fully charged, or general error
                setTimeRemaining(0);
            }

            readWordAsync(kSMBusBatteryAddr, kBVoltageCmd);
            break;

        case kBVoltageCmd:

            setVoltage(val16);
            readWordAsync(kSMBusBatteryAddr, kBMaxErrorCmd);
            break;

        case kBMaxErrorCmd:

            setMaxErr(val16);
            readWordAsync(kSMBusBatteryAddr, kBCycleCountCmd);
           break;

        case kBCycleCountCmd:

            setCycleCount(val16);
            readWordAsync(kSMBusBatteryAddr, kBAverageTimeToEmptyCmd);
            break;

        case kBAverageTimeToEmptyCmd:

            /* Measures a 1 minute rolling average of run time to empty
               estimates. This 1 minute average can be particularly
               inaccurate during the minute following a wake from sleep. */

            if (!fPermanentFailure && transaction_success)
            {
                setAverageTimeToEmpty(val16);

                if (fAvgCurrent < 0) {
                    setTimeRemaining(val16);
                }
            } else {
                setTimeRemaining(0);
                setAverageTimeToEmpty(0);
            }

            readWordAsync(kSMBusBatteryAddr, kBRunTimeToEmptyCmd);
            break;

        case kBRunTimeToEmptyCmd:

             /* This is the instantaneous estimated runtime until the
                battery is empty, as opposed to the average run time. */
            setInstantaneousTimeToEmpty(val16);
            readWordAsync(kSMBusBatteryAddr, kBAverageTimeToFullCmd);
            break;

        case kBAverageTimeToFullCmd:

            if (!fPermanentFailure && transaction_success)
            {
                setAverageTimeToFull(val16);

                if (fAvgCurrent > 0) {
                    setTimeRemaining(val16);
                }
            } else {
                setTimeRemaining(0);
                setAverageTimeToFull(0);
            }

            readWordAsync(kSMBusBatteryAddr, kBTemperatureCmd);
            break;

        case kBTemperatureCmd:

            setProperty("Temperature",
                            (long long unsigned int)val16,
                            (unsigned int)16);

            readWordAsync(kSMBusBatteryAddr, kBReadCellVoltage1Cmd);
            break;

        case kBReadCellVoltage4Cmd:
        case kBReadCellVoltage3Cmd:
        case kBReadCellVoltage2Cmd:
        case kBReadCellVoltage1Cmd:

            if (transaction_success)
            {
                val16 = 0;
            }

            // Executed for first of 4
            if (kBReadCellVoltage1Cmd == next_state) {
                if (fCellVoltages)
                {
                    // Getting a non-NULL array here can only result
                    // from a prior batt read getting aborted sometime
                    // between reading CellVoltage1 and CellVoltage4
                    fCellVoltages->release();
                    fCellVoltages = NULL;
                }
                fCellVoltages = OSArray::withCapacity(4);
            }

            // Executed for all 4 CellVoltage calls through here
            if (fCellVoltages)
            {
                num = OSNumber::withNumber((unsigned long long)val16, 16);
                fCellVoltages->setObject(num);
                num->release();
            }

            // Executed for last of 4
            if (kBReadCellVoltage4Cmd == next_state)
            {
                // After reading cell voltage 1-4, bundle into OSArray and
                // set property in ioreg
                if (fCellVoltages)
                {
                    setProperty(_CellVoltageSym, fCellVoltages);
                    fCellVoltages->release();
                    fCellVoltages = NULL;
                } else {
                    removeProperty(_CellVoltageSym);
                }
                readWordAsync(kSMBusBatteryAddr, kBCurrentCmd);
            } else {
                // Go to the next state of the 4
                // kBReadCellVoltage2Cmd == kBReadCellVoltage1Cmd - 1
                readWordAsync(kSMBusBatteryAddr, next_state - 1);
            }
            break;


        case kBCurrentCmd:
            setInstantAmperage((int16_t)val16);
            this->transactionCompletion_handlePollingFinished();
            break;

        default:
            BattLog("SmartBattery: Error state %d not expected\n", next_state);
    }

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
    properties->removeObject(batteryInfoKey);
    removeProperty(batteryInfoKey);
    properties->removeObject(errorConditionKey);
    removeProperty(errorConditionKey);
    properties->removeObject(_ChargeStatusSym);
    removeProperty(_ChargeStatusSym);
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
 *  These supplement the built-in accessors in IOPMPowerSource.h, and should
 *  arguably be added back into the superclass IOPMPowerSource
 ******************************************************************************/

#define CLASS   AppleSmartBattery

#define IMPLEMENT_APPLESMARTBATTERY_INT_SETTER(methodName, pspropSYM, argNameX, argTypeX)        \
    void CLASS::methodName(argTypeX argNameX) {                      \
        OSNumber    *n = OSNumber::withNumber(argNameX, 32);    \
        if (n) {                                                \
            setPSProperty(pspropSYM, n);                        \
            n->release();                                       \
        }                                                       \
    }

IMPLEMENT_APPLESMARTBATTERY_INT_SETTER(setAverageTimeToEmpty, _AvgTimeToEmptySym, seconds, int);
IMPLEMENT_APPLESMARTBATTERY_INT_SETTER(setAverageTimeToFull, _AvgTimeToFullSym, seconds, int);
IMPLEMENT_APPLESMARTBATTERY_INT_SETTER(setInstantaneousTimeToEmpty, _InstantTimeToEmptySym, seconds, int);
IMPLEMENT_APPLESMARTBATTERY_INT_SETTER(setInstantAmperage, _InstantAmperageSym, mA, int);
IMPLEMENT_APPLESMARTBATTERY_INT_SETTER(setManufactureDate, _ManfDateSym, date, int);
IMPLEMENT_APPLESMARTBATTERY_INT_SETTER(setSerialNumber, _SerialNumberSym, sernum, uint16_t);
IMPLEMENT_APPLESMARTBATTERY_INT_SETTER(setMaxErr, _MaxErrSym, error, int);


#define IMPLEMENT_APPLESMARTBATTERY_INT_GETTER(methodName, pspropSYM, return_type)        \
    return_type CLASS::methodName(void) {                       \
        OSNumber *n = OSDynamicCast(OSNumber, properties->getObject(_SerialNumberSym));  \
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

void AppleSmartBattery::setDeviceName(OSSymbol *sym)
{
    if (sym)
        setPSProperty(_DeviceNameSym, (OSObject *)sym);
}

OSSymbol * AppleSmartBattery::deviceName(void)
{
    return OSDynamicCast(OSSymbol, properties->getObject(_DeviceNameSym));
}


void    AppleSmartBattery::setFullyCharged(bool charged)
{
    setPSProperty(_FullyChargedSym, (charged ? kOSBooleanTrue:kOSBooleanFalse));
}

bool    AppleSmartBattery::fullyCharged(void)
{
    return (kOSBooleanTrue == properties->getObject(_FullyChargedSym));
}

void    AppleSmartBattery::setManufacturerData(uint8_t *buffer, uint32_t bufferSize)
{
    OSData      *newData = OSData::withBytes(buffer, bufferSize);

    if (newData) {
        setPSProperty(_ManufacturerDataSym, newData);
        newData->release();
    }
}

void    AppleSmartBattery::setChargeStatus(const OSSymbol *sym)
{
    if (NULL == sym) {
        properties->removeObject(_ChargeStatusSym);
        removeProperty(_ChargeStatusSym);
    } else {
        setPSProperty(_ChargeStatusSym, (OSObject *)sym);
    }
}

const OSSymbol *AppleSmartBattery::chargeStatus(void)
{
    return (const OSSymbol *)properties->getObject(_ChargeStatusSym);
}

/******************************************************************************
 ******************************************************************************
 **
 **  Async SmartBattery read convenience functions
 **
 ******************************************************************************
 ******************************************************************************/
IOReturn AppleSmartBattery::readWordAsync(
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
                    (void *)cmd);

    return ret;
}

IOReturn AppleSmartBattery::writeWordAsync(
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
                    (void *)((uint32_t)cmd | kWriteIndicatorBit));

    return ret;

}

IOReturn AppleSmartBattery::readBlockAsync(
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
                    (void *)cmd);

    return ret;
}

