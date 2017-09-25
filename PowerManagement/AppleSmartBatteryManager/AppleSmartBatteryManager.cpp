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

#include <IOKit/smc/AppleSMCFamily.h>
#include "AppleSmartBatteryManager.h"
#include "AppleSmartBattery.h"
#include "SmbusHandler.h"

#define kMaxRetries     5

// XXX: Temporary local definition to remove dependecy on AppleSMC changes
#ifndef kSMCNotifyAdapterDetailsChange
#define kSMCNotifyAdapterDetailsChange 0x6
#endif

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

bool AppleSmartBatteryManager::start(IOService *provider)
{

    if (!super::start(provider)) {
        return false;
    }
    
    PE_parse_boot_argn("batman", &gBMDebugFlags, sizeof(gBMDebugFlags));
    

    fProvider = OSDynamicCast(IOSMBusController, provider);
    if (!fProvider) {
        BM_ERRLOG("Provider is not SMBusController\n");
        return false;
    } else {
        BM_LOG1("Provider is IOSMBusController\n");
    }
    
    fSmbus = new SmbusHandler;
    if (!fSmbus) {
        BM_ERRLOG("Failed to instantiate SMBus Handler\n");
    }
    fSmbus->initialize(this);


    const OSSymbol *ucClassName = 
            OSSymbol::withCStringNoCopy("AppleSmartBatteryManagerUserClient");
    setProperty(gIOUserClientClassKey, (OSObject *) ucClassName);
    ucClassName->release();
    
    fWorkLoop = IOWorkLoop::workLoop();
    if (!fWorkLoop) {
        return false;
    }

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

    Action gatedHandler = (IOCommandGate::Action)OSMemberFunctionCast(IOCommandGate::Action, this, &AppleSmartBatteryManager::performExternalTransactionGated);
    
    return fManagerGate->runAction(gatedHandler, in, out, (void*)inSize, outSize);
    
}

// This function blocks the thread until command is completed
IOReturn AppleSmartBatteryManager::performExternalTransactionGated(void *in,  void *out, IOByteCount inSize, IOByteCount *outSize)
{
    while (fSmbusCommandInProgress) {
        fManagerGate->commandWakeup(&fSmbusCommandInProgress, true);
    }
    fSmbusCommandInProgress = true;
    
    return fSmbus->smbusExternalTransaction(in, out, inSize, outSize);
}


/* 
 * performTransaction
 * 
 * Called by smart battery children
 */

IOReturn AppleSmartBatteryManager::smbusCompletionHandlerGated(ASBMgrTransactionCompletion *completion, OSObject **target, void **ref)
{
    *target = fAsbmTarget;
    *ref = fAsbmReference;
    *completion = fAsbmCompletion;
    
    fSmbusCommandInProgress = false;
    fManagerGate->commandWakeup(&fSmbusCommandInProgress);
    
    return kIOReturnSuccess;
}


IOReturn AppleSmartBatteryManager::smbusCompletionHandler(void *ref, IOReturn status, size_t byteCount, uint8_t *dataBuf)
{
    ASBMgrTransactionCompletion clientHandler;
    OSObject                    *target;
    void                        *clientRef;
    uint8_t                     inData[MAX_SMBUS_DATA_SIZE];

    bzero(inData, sizeof(inData));
    if (byteCount) {
        memcpy(inData, dataBuf, (byteCount < MAX_SMBUS_DATA_SIZE) ? byteCount : MAX_SMBUS_DATA_SIZE);
    }

    Action gatedHandler = (IOCommandGate::Action)OSMemberFunctionCast(
            IOCommandGate::Action, this, &AppleSmartBatteryManager::smbusCompletionHandlerGated);
    
    fManagerGate->runAction(gatedHandler, &clientHandler, &target, &clientRef);

    clientHandler(target, clientRef, status, byteCount, inData);
    
    return kIOReturnSuccess;
}


IOReturn AppleSmartBatteryManager::performSmbusTransactionGated(
                                                                ASBMgrRequest *req,
                                                                OSObject *target, void *ref)
{
    IOReturn ret = kIOReturnSuccess;
    while (fSmbusCommandInProgress) {
        fManagerGate->commandSleep(&fSmbusCommandInProgress);
    }
    fSmbusCommandInProgress = true;
    
    // Save the incoming params
    fAsbmTarget = target;
    fAsbmReference = ref;
    fAsbmCompletion = req->completionHandler;
    
    ret = fSmbus->performTransaction(req, OSMemberFunctionCast(ASBMgrTransactionCompletion, this,
                                                         &AppleSmartBatteryManager::smbusCompletionHandler), this, NULL);
    if (ret != kIOReturnSuccess) {
        fSmbusCommandInProgress = false;
        fManagerGate->commandWakeup(&fSmbusCommandInProgress);
    }
    
    return ret;
}

IOReturn AppleSmartBatteryManager::performTransaction(ASBMgrRequest *req, OSObject * target, void * reference)
{

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

    if (fBattery) {
        ret = fBattery->handleSystemSleepWake(this, !which);
    }
    return ret;
}


void AppleSmartBatteryManager::messageSMC(const OSSymbol *event, OSObject *val, uintptr_t refcon)
{
}

/* 
 * message
 *
 */
IOReturn AppleSmartBatteryManager::message( 
    UInt32 type, 
    IOService *provider,
    void *argument )
{
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
        && fBattery)
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
    return kIOReturnSuccess;
}


void AppleSmartBatteryManager::handleBatteryInserted(void)
{
    BM_LOG1("SmartBattery: battery inserted!\n");
    fInacessible = false;
    fBattery->handleBatteryInserted();
    return;
}

void AppleSmartBatteryManager::handleBatteryRemoved(void)
{
    BM_ERRLOG("Received battery removed notification\n");
    fInacessible = true;
    fBattery->handleBatteryRemoved();
    return;
}

/* 
 * inhibitCharging
 * 
 * Called by AppleSmartBatteryManagerUserClient
 */

IOReturn AppleSmartBatteryManager::inhibitChargingGated(int level)
{
    IOReturn        ret = kIOReturnSuccess;
    
    if(!fManagerGate) return kIOReturnInternalError;
    
    ret = fSmbus->inhibitCharging(level);
    if (ret == kIOReturnSuccess) {
        this->setProperty("Charging Inhibited", level ? true : false);
    }
    
    
    return ret;
    
}
IOReturn AppleSmartBatteryManager::inhibitCharging(int level)
{
    
    return fManagerGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &AppleSmartBatteryManager::inhibitChargingGated), this, (void *)(uintptr_t)level);
}

/* 
 * disableInflow
 * 
 * Called by AppleSmartBatteryManagerUserClient
 */
IOReturn AppleSmartBatteryManager::disableInflowGated(int level)
{
    IOReturn        ret = kIOReturnSuccess;
    
    if(!fManagerGate) return kIOReturnInternalError;
    
    ret = fSmbus->disableInflow(level);
    if (ret == kIOReturnSuccess) {
        this->setProperty("Inflow Disabled", level ? true : false);
    }
    
    return ret;
    
}

IOReturn AppleSmartBatteryManager::disableInflow(int level)
{

    return fManagerGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &AppleSmartBatteryManager::disableInflowGated), this, (void *)(uintptr_t)level);

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
    
#if 0
    IOPMrootDomain      *root_domain = getPMRootDomain();
    IOReturn            ret;
    void *              messageArgument = NULL;

    if( getProperty("Inflow Disabled") )
    {
        messageArgument = (void *)kInflowForciblyEnabledBit;

        /* 
         * Send disable inflow command to SB Manager
         */
        fManagerGate->runAction(OSMemberFunctionCast(IOCommandGate::Action,
                           this, &AppleSmartBatteryManager::gatedSendCommand),
                           (void *)kDisableInflowCmd, (void *)0, /* OFF */ 
                           (void *)&ret, NULL);
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
    
#endif
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
    if( request && fExclusiveUserClient) {
        /* Oops - a second client reaching for exclusive access.
         * This shouldn't happen.
         */
        return false;
    }

    fExclusiveUserClient = request;

    /* Signal our driver, and the SMC firmware to either:
        - stop communicating with the battery
        - resume communications
     */
    fManagerGate->runAction(
                    OSMemberFunctionCast( 
                        IOCommandGate::Action, this,
                        &SmbusHandler::handleExclusiveAccess),
                    (void *)request, NULL, NULL, NULL);

    if (!fExclusiveUserClient) {
        requestPoll(kFull);
    }

    return true;
}

bool AppleSmartBatteryManager::hasExclusiveClient(void) {
    return fExclusiveUserClient;    
}

bool AppleSmartBatteryManager::requestPoll(int type) {
    IOReturn ret = kIOReturnError;

    if (fBattery) {
        ret = fBattery->pollBatteryState(type);
    }

    return ret;
}



/*
 * setOverrideCapacity
 *
 */
IOReturn AppleSmartBatteryManager::setOverrideCapacity(uint16_t level)
{
    IOReturn ret = kIOReturnSuccess;
    if (fBattery) {
        fBattery->handleSetOverrideCapacity(level, true);
    }
    return ret;
}

/*
 * switchToTrueCapacity
 *
 */
IOReturn AppleSmartBatteryManager::switchToTrueCapacity(void)
{
    IOReturn ret = kIOReturnSuccess;
    
    if (fBattery) {
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
