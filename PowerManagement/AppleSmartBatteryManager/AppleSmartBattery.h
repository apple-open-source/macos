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

#ifndef __AppleSmartBattery__
#define __AppleSmartBattery__

#include <IOKit/IOService.h>
#include <IOKit/pwr_mgt/IOPMPowerSource.h>
#include <IOKit/smbus/IOSMBusController.h>
#include <IOKit/acpi/IOACPIPlatformDevice.h>

#include "AppleSmartBatteryCommands.h"
#include "AppleSmartBatteryManager.h"

#define kBatteryPollingDebugKey     "BatteryPollingPeriodOverride"

class AppleSmartBatteryManager;

class AppleSmartBattery : public IOPMPowerSource {
    OSDeclareDefaultStructors(AppleSmartBattery)

protected:
    AppleSmartBatteryManager    *fProvider;
    IOWorkLoop                  *fWorkLoop;
    IOTimerEventSource          *fPollTimer;
    IOTimerEventSource          *fBatteryReadAllTimer;
    bool                        fStalledByUserClient;
    bool                        fCancelPolling;
    bool                        fPollingNow;
    IOSMBusTransaction          fTransaction;
    uint16_t                    fMachinePath;
    uint32_t                    fPollingInterval;
    bool                        fPollingOverridden;
    bool                        fRebootPolling;
    bool                        fInflowDisabled;
    bool                        fChargeInhibited;
    uint16_t                    fRemainingCapacity;
    uint16_t                    fFullChargeCapacity;

    uint8_t                     fInitialPollCountdown;
    uint8_t                     fIncompleteReadRetries;
    int                         fRetryAttempts;

    IOService *                 fPowerServiceToAck;
    bool                        fSystemSleeping;

    bool                        fPermanentFailure;
    bool                        fFullyDischarged;
    bool                        fFullyCharged;
    int                         fBatteryPresent;
    int                         fACConnected;
    int                         fAvgCurrent;
    OSArray                     *fCellVoltages;

    IOACPIPlatformDevice        *fACPIProvider;

    // Accessor for MaxError reading
    // Percent error in MaxCapacity reading
    void    setMaxErr(int error);
    int     maxErr(void);

    // SmartBattery reports a device name
    void    setDeviceName(OSSymbol *sym);
    OSSymbol *deviceName(void);

    // Set when battery is fully charged;
    // Clear when battery starts discharging/AC is removed
    void    setFullyCharged(bool);
    bool    fullyCharged(void);

    // Time remaining estimate - as measured instantaneously
    void    setInstantaneousTimeToEmpty(int seconds);

    // Instantaneous amperage
    void    setInstantAmperage(int mA);

    // Time remaining estimate - 1 minute average
    void    setAverageTimeToEmpty(int seconds);
    int     averageTimeToEmpty(void);

    // Time remaining until full estimate - 1 minute average
    void    setAverageTimeToFull(int seconds);
    int     averageTimeToFull(void);

    void    setManufactureDate(int date);
    int     manufactureDate(void);

    void    setSerialNumber(uint16_t sernum);
    uint16_t    serialNumber(void);

    void    setChargeStatus(const OSSymbol *sym);
    const OSSymbol    *chargeStatus(void);

    // An OSData container of manufacturer specific data
    void    setManufacturerData(uint8_t *buffer, uint32_t bufferSize);

    void    oneTimeBatterySetup(void);

    void    constructAppleSerialNumber(void);

public:
    static AppleSmartBattery *smartBattery(void);

    virtual bool init(void);

    virtual bool start(IOService *provider);

    void    setPollingInterval(int milliSeconds);

    bool    pollBatteryState(int path = 0);

    void    handleBatteryInserted(void);

    void    handleBatteryRemoved(void);

    void    handleInflowDisabled(bool inflow_state);

    void    handleChargeInhibited(bool charge_state);

    void    handleExclusiveAccess(bool exclusive);

    IOReturn handleSystemSleepWake(IOService * powerService, bool isSystemSleep);

protected:
    void    logReadError( const char *error_type,
                          uint16_t additional_error,
                          IOSMBusTransaction *t);

    void    clearBatteryState(bool do_update);

    void    pollingTimeOut(void);

    void    incompleteReadTimeOut(void);

    void    rebuildLegacyIOBatteryInfo(void);

    bool    transactionCompletion(void *ref, IOSMBusTransaction *transaction);
    uint32_t    transactionCompletion_requiresRetryGetMicroSec(IOSMBusTransaction *transaction);
    bool        transactionCompletion_shouldAbortTransactions(IOSMBusTransaction *transaction);
    void        transactionCompletion_handlePollingFinished(void);

    IOReturn readWordAsync(uint8_t address, uint8_t cmd);

    IOReturn writeWordAsync(uint8_t address, uint8_t cmd, uint16_t writeWord);

    IOReturn readBlockAsync(uint8_t address, uint8_t cmd);

    void    acknowledgeSystemSleepWake( void );
};

#endif
