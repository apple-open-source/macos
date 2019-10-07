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

//#include <IOKit/pwr_mgt/RootDomain.h>
#include <sys/sysctl.h>

#include <IOKit/pwr_mgt/RootDomain.h>
#include "AppleSmartBatteryManager.h"
#include "AppleSmartBattery.h"
#if TARGET_OS_OSX
#include "SmbusHandler.h"
#endif

#define kMaxRetries     5

// XXX: Temporary local definition to remove dependecy on AppleSMC changes
#ifndef kSMCNotifyAdapterDetailsChange
#define kSMCNotifyAdapterDetailsChange 0x6
#endif

#define ASSERT_GATED() \
do {  \
    if (fWorkLoop->inGate() != true) {   \
        panic("AppleSmartBattery: not inside workloop gate");  \
    } \
} while(false)

uint32_t gBMDebugFlags = (BM_LOG_LEVEL0 | BM_LOG_LEVEL1);
bool gDebugAllowed = true;
/*
 * 0x00000001 : Level 1 - Log completions and failures
 * 0x00000020 : Level 2 - Transaction logging
 */
static SYSCTL_INT(_debug, OID_AUTO, batman, CTLFLAG_RW, &gBMDebugFlags, 0, "");

// Power states!
enum {
    kMyOnPowerState = 1
};

// Commands!
enum {
    kInhibitChargingCmd = 0,
    kDisableInflowCmd
};

static IOPMPowerState myTwoStates[2] = {
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, kIOPMPowerOn, kIOPMPowerOn, kIOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0}
};

#define super IOService

OSDefineMetaClassAndStructors(AppleSmartBatteryManager, IOService)


bool AppleSmartBatteryManager::init(void)
{
    if (!super::init()) {
        return false;
    }

    _started = false;

    return true;
}

bool AppleSmartBatteryManager::start(IOService *provider)
{
    if (!super::start(provider)) {
        return false;
    }

    PE_parse_boot_argn("batman", &gBMDebugFlags, sizeof(gBMDebugFlags));

#if TARGET_OS_OSX

    fProvider = OSDynamicCast(IOSMBusController, provider);
    if (!fProvider) {
        BM_ERRLOG("Provider is not SMBusController\n");
        return false;
    } else {
        BM_LOG1("Provider is IOSMBusController\n");
    }

    fWorkLoop = fProvider->getWorkLoop();
    if (!fWorkLoop) {
        return false;
    }

    fSmbus = new SmbusHandler;
    if (!fSmbus) {
        BM_ERRLOG("Failed to instantiate SMBus Handler\n");
    }
    fSmbus->initialize(this);
#else // TARGET_OS_OSX
    fWorkLoop = IOWorkLoop::workLoop();
    if (!fWorkLoop) {
        return false;
    }

    gDebugAllowed = PE_i_can_has_debugger(NULL);

    fProvider = OSDynamicCast(AppleSMCFamily, provider);
    if (!fProvider) {
        BM_ERRLOG("Provider is not AppleSMCFamily\n");
        return false;
    } else {
        BM_LOG1("Provider is AppleSMCFamily\n");
    }
#endif // TARGET_OS_OSX

    const OSSymbol *ucClassName = 
            OSSymbol::withCStringNoCopy("AppleSmartBatteryManagerUserClient");
    setProperty(gIOUserClientClassKey, (OSObject *) ucClassName);
    ucClassName->release();

    // Command gate for SmartBatteryManager
    fManagerGate = IOCommandGate::commandGate(this);
    if (!fManagerGate) {
        return false;
    }
    fWorkLoop->addEventSource(fManagerGate);

    fBattery = AppleSmartBattery::smartBattery();

    if (!fBattery) {
        BM_ERRLOG("Failed to instantiate AppleSmartBattery\n");
        return false;
    }

    fBattery->attach(this);

    fBattery->start(this);

    sysctl_register_oid(&sysctl__debug_batman);

    // Track UserClient exclusive access to smartbattery
    fExclusiveUserClient = false;

    // Join power management so that we can get a notification early during
    // wakeup to re-sample our battery data. We don't actually power manage
    // any devices.
    PMinit();
    registerPowerDriver(this, myTwoStates, 2);
    provider->joinPMtree(this);

    fBattery->registerService(0);

    this->registerService(0);


    _started = true;
    BM_ERRLOG("AppleSmartBatteryManager started\n");

    return true;
}

IOWorkLoop *AppleSmartBatteryManager::getWorkLoop() const
{
    return fWorkLoop;
}

/*
 * performExternalWordTransaction
 * 
 * Called by AppleSmartBatteryManagerUserClient
 */
IOReturn AppleSmartBatteryManager::performExternalTransaction(
                                                              void *in,
                                                              void *out,
                                                              IOByteCount inSize,
                                                              IOByteCount *outSize)
{

#if TARGET_OS_OSX
    if (!fWorkLoop->inGate()) {
        Action gatedHandler = (IOCommandGate::Action)OSMemberFunctionCast(IOCommandGate::Action, this, &AppleSmartBatteryManager::performExternalTransaction);
        return fWorkLoop->runAction( gatedHandler, this, in, out, (void*)inSize, outSize);
    }

    // This call blocks the thread until command is completed
    return fSmbus->smbusExternalTransaction(in, out, inSize, outSize);

#else
    return kIOReturnSuccess;
#endif
}


/*
 * performTransaction
 *
 * Called by smart battery children
 */

IOReturn AppleSmartBatteryManager::smbusCompletionHandler(void *ref, IOReturn status, size_t byteCount, uint8_t *dataBuf)
{
#if TARGET_OS_OSX
    uint8_t                     inData[MAX_SMBUS_DATA_SIZE];

    ASSERT_GATED();
    bzero(inData, sizeof(inData));
    if (byteCount) {
        memcpy(inData, dataBuf, (byteCount < MAX_SMBUS_DATA_SIZE) ? byteCount : MAX_SMBUS_DATA_SIZE);
    }

    fAsbmCompletion(fAsbmTarget, fAsbmReference, status, byteCount, inData);

#endif
    return kIOReturnSuccess;
}


IOReturn AppleSmartBatteryManager::performSmbusTransactionGated(
                                                                ASBMgrRequest *req,
                                                                OSObject *target, void *ref)
{
    IOReturn ret = kIOReturnSuccess;
#if TARGET_OS_OSX
    ASSERT_GATED();

    // Save the incoming params
    fAsbmTarget = target;
    fAsbmReference = ref;
    fAsbmCompletion = req->completionHandler;

    ret = fSmbus->performTransaction(req, OSMemberFunctionCast(ASBMgrTransactionCompletion, this,
                                                         &AppleSmartBatteryManager::smbusCompletionHandler), this, NULL);
#endif
    return ret;
}

IOReturn AppleSmartBatteryManager::performTransaction(ASBMgrRequest *req, OSObject * target, void * reference)
{
#if TARGET_OS_OSX
    switch (req->opType) {
        case kASBMSMBUSReadWord:
        case kASBMSMBUSReadBlock:
        case kASBMSMBUSExtendedReadWord:
        case kASBMSMBUSWriteWord:
        {
            Action gatedHandler = (IOCommandGate::Action)OSMemberFunctionCast(
                                      IOCommandGate::Action, this, &AppleSmartBatteryManager::performSmbusTransactionGated);
            if (gatedHandler == NULL) {
                panic("gatedHandler is null\n");
            }
            return fManagerGate->runAction(gatedHandler, req, target, reference);
        }
        default:
            BM_ERRLOG("Unsupported transaction type %d\n", req->opType);
            return kIOReturnInvalid;
    }
#endif
    return kIOReturnSuccess;
}


/*
 * setPowerState
 *
 */
IOReturn AppleSmartBatteryManager::setPowerState(
    unsigned long which, 
    IOService *whom)
{
    IOReturn ret = IOPMAckImplied;

    if (_started) {
        ret = fBattery->handleSystemSleepWake(this, !which);
    }
    return ret;
}


void AppleSmartBatteryManager::messageSMC(const OSSymbol *event, OSObject *val, uintptr_t refcon)
{
}

IOReturn AppleSmartBatteryManager::message( 
    UInt32 type, 
    IOService *provider,
    void *argument )
{
#if TARGET_OS_OSX
    BM_ERRLOG("SmartBattery: notification received in message()\n");
    IOSMBusAlarmMessage     *alarm = (IOSMBusAlarmMessage *)argument;
    static uint16_t         last_data = 0;
    uint16_t                changed_bits = 0;
    uint16_t                data = 0;

    /* On SMBus alarms from the System Battery Manager, trigger a new
       poll of battery state.   */

    if(!alarm) return kIOReturnSuccess;

    if( (kIOMessageSMBusAlarm == type) 
        && (kSMBusManagerAddr == alarm->fromAddress)
        && _started)
    {
        data = (uint16_t)(alarm->data[0] | (alarm->data[1] << 8));
        changed_bits = data ^ last_data;
        last_data = data;

        if(changed_bits & kMPresentBatt_A_Bit)
        {
            if(data & kMPresentBatt_A_Bit) {
                // Battery inserted
                fManagerGate->runAction(OSMemberFunctionCast(IOCommandGate::Action,
                                                             this, &AppleSmartBatteryManager::handleBatteryInserted), this);
            } else {
                // Battery removed
                fManagerGate->runAction(OSMemberFunctionCast(IOCommandGate::Action,
                                                             this, &AppleSmartBatteryManager::handleBatteryRemoved), this);
            }
        } else {
            // Just an alarm; re-read battery state.
            fBattery->pollBatteryState(kUseLastPath);
        }
    }
#endif
    return kIOReturnSuccess;
}


void AppleSmartBatteryManager::handleBatteryInserted(void)
{
    BM_LOG2("SmartBattery: battery inserted!\n");
    fInacessible = false;
    if (_started) {
        fBattery->handleBatteryInserted();
    }
    return;
}

void AppleSmartBatteryManager::handleBatteryRemoved(void)
{
    BM_ERRLOG("Received battery removed notification\n");
    fInacessible = true;
    if (_started) {
        fBattery->handleBatteryRemoved();
    }
    return;
}

/*
 * inhibitCharging
 *
 * Called by AppleSmartBatteryManagerUserClient
 */

IOReturn AppleSmartBatteryManager::inhibitChargingGated(uint64_t level)
{
    IOReturn        ret = kIOReturnSuccess;

#if TARGET_OS_OSX
    ret = fSmbus->inhibitCharging(level ? 1 : 0);
    if (ret == kIOReturnSuccess) {
        this->setProperty("Charging Inhibited", level ? true : false);
    }
#endif

    return ret;
}
IOReturn AppleSmartBatteryManager::inhibitCharging(int level)
{
    if(!fManagerGate) return kIOReturnInternalError;
    return fManagerGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this,
                &AppleSmartBatteryManager::inhibitChargingGated), (void *)(uintptr_t)level);
}

/*
 * disableInflow
 *
 * Called by AppleSmartBatteryManagerUserClient
 */
IOReturn AppleSmartBatteryManager::disableInflowGated(uint64_t level)
{
    IOReturn        ret = kIOReturnSuccess;

#if TARGET_OS_OSX
    ret = fSmbus->disableInflow(level ? 1 : 0);
    if (ret == kIOReturnSuccess) {
        this->setProperty("Inflow Disabled", level ? true : false);
    }
#endif
    return ret;
}

IOReturn AppleSmartBatteryManager::disableInflow(int level)
{
    if(!fManagerGate) return kIOReturnInternalError;
    return fManagerGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &AppleSmartBatteryManager::disableInflowGated), (void *)(uintptr_t)level);

}

/* 
 * handleFullDischarge
 * 
 * Called by AppleSmartBattery
 * If inflow is disabled, by the user client, we re-enable it and send a message
 * via IOPMrootDomain indicating that inflow has been re-enabled.
 */
void AppleSmartBatteryManager::handleFullDischarge(void)
{
    IOPMrootDomain      *root_domain = getPMRootDomain();
    void *              messageArgument = NULL;

    if( getProperty("Inflow Disabled") )
    {
        messageArgument = (void *)kInflowForciblyEnabledBit;

        /*
         * Send disable inflow command to SB Manager
         */
        disableInflow(0);
    }

    /*
     * Message user space clients that battery is fully discharged.
     * If appropriate, set kInflowForciblyEnabledBit
     */
    if(root_domain) {
        root_domain->messageClients( 
                kIOPMMessageInternalBatteryFullyDischarged, 
                messageArgument );
    }
}


IOReturn AppleSmartBatteryManager::requestExclusiveSMBusAccessGated(bool request)
{
#if TARGET_OS_OSX
    if( request && fExclusiveUserClient) {
        /* Oops - a second client reaching for exclusive access.
         * This shouldn't happen.
         */
        return kIOReturnBusy;
    }

    fExclusiveUserClient = request;

    fSmbus->handleExclusiveAccess(request);

    if (!fExclusiveUserClient) {
        requestPoll(kFull);
    }

#endif
    return kIOReturnSuccess;
}

/*
 * requestExclusiveSMBusAcess
 *
 * Called by AppleSmartBatteryManagerUserClient
 * When exclusive SMBus access is requested, we'll hold off on any SmartBattery bus reads.
 * We do not do any periodic SMBus reads 
 */
// TODO: Should be a generic requestExclusiveAccess()
bool AppleSmartBatteryManager::requestExclusiveSMBusAccess(
    bool request)
{
#if TARGET_OS_OSX
    /* Signal our driver, and the SMC firmware to either:
        - stop communicating with the battery
        - resume communications
     */
    Action gatedHandler = (IOCommandGate::Action)OSMemberFunctionCast(
                              IOCommandGate::Action, this, &AppleSmartBatteryManager::requestExclusiveSMBusAccessGated);
    return  (fManagerGate->runAction(gatedHandler, (void *)request) == kIOReturnSuccess) ? true : false;
#endif
    return true;
}

bool AppleSmartBatteryManager::hasExclusiveClient(void) {
    return fExclusiveUserClient;
}

bool AppleSmartBatteryManager::requestPoll(int type) {
    IOReturn ret = kIOReturnError;

    if (_started) {
        ret = fBattery->pollBatteryState(type);
    }

    return ret;
}

IOReturn AppleSmartBatteryManager::setOverrideCapacity(uint16_t level)
{
    IOReturn ret = kIOReturnSuccess;
    if (_started) {
        fBattery->handleSetOverrideCapacity(level, true);
    }
    return ret;
}

IOReturn AppleSmartBatteryManager::switchToTrueCapacity(void)
{
    IOReturn ret = kIOReturnSuccess;

    if (_started) {
        fBattery->handleSwitchToTrueCapacity();
        fBattery->pollBatteryState(kUserVis);
    }
    return ret;
}

bool AppleSmartBatteryManager::isSystemSleeping()
{
    return fSystemSleeping;
}

bool AppleSmartBatteryManager::exclusiveClientExists()
{
    return fExclusiveUserClient;
}

bool AppleSmartBatteryManager::isBatteryInaccessible()
{
    return fInacessible;
}
