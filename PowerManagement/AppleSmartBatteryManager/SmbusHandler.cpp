//
//  smbusHandler.cpp
//  PowerManagement
//
//  Created by prasadlv on 11/8/16.

#include <IOKit/smbus/IOSMBusController.h>
#include <IOKit/acpi/IOACPIPlatformDevice.h>

#include "AppleSmartBatteryManager.h"
#include "SmbusHandler.h"

#define super OSObject
OSDefineMetaClassAndStructors(SmbusHandler, OSObject)


static int  retryDelaysTable[kRetryAttempts] =
{ 1, 10, 100, 1000, 1000 };


// Delays to use on subsequent SMBus re-read failures.
// In microseconds.
static const uint32_t microSecDelayTable[kRetryAttempts] =
{ 10, 100, 1000, 10000, 250000 };

// One "wait interval" is 100ms
#define kWaitExclusiveIntervals 30


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

#define VOIDPTR(arg)  ((void *)(uintptr_t)(arg))

IOReturn SmbusHandler::initialize(AppleSmartBatteryManager *mgr)
{
 
    fRetryAttempts = 0;
    fExternalTransactionWait = 0;
    fMgr = mgr;
    fWorkLoop = mgr->getWorkLoop();
    return kIOReturnSuccess;
}


uint32_t SmbusHandler::requiresRetryGetMicroSec(IOSMBusTransaction *transaction)
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
        fRetryAttempts = 0;
        transaction_needs_retry = false;

        goto exit;
    }

    /******************************************************************************************
     ******************************************************************************************/

    if (kIOSMBusStatusOK == transaction_status)
    {
        if (0 != fRetryAttempts) {
            BM_LOG1("SmartBattery: retry %d succeeded!\n", fRetryAttempts);

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

        BM_ERRLOG("SmartBattery: Giving up on (0x%02x, 0x%02x) after %d retries.\n",
                transaction->address, transaction->command, fRetryAttempts);

        fRetryAttempts = 0;

        transaction_needs_retry = false;

        // After too many retries, unblock PM state machine in case it is
        // waiting for the first battery poll after wake to complete,
        // avoiding a setPowerState timeout.
        //TODO:acknowledgeSystemSleepWake();

        goto exit;
    }

exit:
    if (transaction_needs_retry && fRetryAttempts < kRetryAttempts) {
        return microSecDelayTable[fRetryAttempts++];
    } else {
        return 0;
    }
}

IOReturn SmbusHandler::isTransactionAllowed()
{
    /* Stop battery work when system is going to sleep.
     */
    if (fMgr->isSystemSleeping())
    {
        BM_ERRLOG("Aborting transactions as system is sleeping\n");
        return kIOReturnOffline;
    }

    /* If a user client has exclusive access to the SMBus,
     * we'll exit immediately.
     */
    if (fMgr->exclusiveClientExists())
    {
        BM_ERRLOG("Aborting transactions due to exclusive access request\n");
        return kIOReturnExclusiveAccess;
    }

    if (fMgr->isBatteryInaccessible())
    {
        BM_ERRLOG("Aborting transactions as battery is inaccessible\n");
        return kIOReturnNoDevice;
    }

    return kIOReturnSuccess;
}

void SmbusHandler::smbusCompletion(void *ref, IOSMBusTransaction *transaction)
{
    uint32_t delay_for;
    IOReturn ret;
    uint32_t cmdCount = (uint32_t)((uintptr_t)ref);

    if (!transaction) {
        BM_ERRLOG("smbus completion called without transaction\n");
        return;
    }

    if (!fWorkLoop->inGate()) {
        fWorkLoop->runAction(
                OSMemberFunctionCast(IOWorkLoop::Action, this,
                    &SmbusHandler::smbusCompletion), this, VOIDPTR(ref), VOIDPTR(transaction));
        return;
    }

    if (cmdCount != fCmdCount) {
        BM_ERRLOG("Unexpected smbus completion. expected cmdCnt:%d completion cmdCnt:%d\n", fCmdCount, cmdCount);
        return;
    }

    BM_LOG2("transaction cmd: 0x%x status: 0x%02x; prot: 0x%02x; word: 0x%04x\n",
            transaction->command, transaction->status, transaction->protocol,
            (transaction->receiveData[1] << 8) | transaction->receiveData[0]);

    if ((ret = isTransactionAllowed()) != kIOReturnSuccess) {
        fCompletion(fTarget, fReference, ret, 0, NULL);
        return ;
    }

    if ((delay_for = requiresRetryGetMicroSec(transaction)) != 0) {
        BM_ERRLOG("transaction cmd: 0x%02x failed with 0x%02x; retry attempt %d of %d\n",
                transaction->command, transaction->status, fRetryAttempts, kRetryAttempts);
        if (delay_for < 1000) {
            IODelay(delay_for); // microseconds
        } else {
            IOSleep(delay_for / 1000); // milliseconds
        }

        fCmdCount++;
        ret = fMgr->fProvider->performTransaction(&fTransaction,
                                OSMemberFunctionCast(IOSMBusTransactionCompletion,
                                                     this, &SmbusHandler::smbusCompletion),
                                this, VOIDPTR(fCmdCount));

        if (ret != kIOReturnSuccess) {
            BM_ERRLOG("Smbus trasaction submission failed with error 0x%x\n", ret);
            fRetryAttempts = 0;
            fCompletion(fTarget, fReference, kIOReturnIOError, 0, NULL);

        }
        return;
    }
    if (transaction->status != kIOSMBusStatusOK) {
        BM_ERRLOG("transaction cmd: 0x%x is returned due to error 0x%x after %d retries",
                  transaction->command, transaction->status, fRetryAttempts);
        fRetryAttempts = 0;
        fCompletion(fTarget, fReference, kIOReturnIOError,
                    transaction->receiveDataCount, transaction->receiveData);
        return;
    }

    switch (fOpType) {
        case kASBMSMBUSReadWord:
        case kASBMSMBUSReadBlock:
            fRetryAttempts = 0;
            fCompletion(fTarget, fReference, kIOReturnSuccess,
                        transaction->receiveDataCount, transaction->receiveData);
            break;

        case kASBMSMBUSExtendedReadWord:
            fTransaction.protocol = kIOSMBusProtocolReadWord;
            fRetryAttempts = 0;
            BM_LOG2("Issuing read for extended read. proto:%d cmd:0x%x add:0x%x",
                    fTransaction.protocol, fTransaction.command, fTransaction.address);

            fOpType = kASBMSMBUSReadWord;
            fCmdCount++;
            ret = fMgr->fProvider->performTransaction(&fTransaction,
                                                     OSMemberFunctionCast(IOSMBusTransactionCompletion,
                                                                          this, &SmbusHandler::smbusCompletion),
                                                     this, VOIDPTR(fCmdCount));
            if (ret != kIOReturnSuccess) {
                BM_ERRLOG("Smbus trasaction submission failed with error 0x%x\n", ret);
                fCompletion(fTarget, fReference, kIOReturnIOError, 0, NULL);
            }
            break;

        case kASBMSMBUSWriteWord:
            fCompletion(fTarget, fReference, kIOReturnSuccess, 0, NULL);
            break;

        default:
            panic("Completion call for unexpected SMBUS transaction protocol %d\n", fOpType);
            break;
    }

    return;
}

IOReturn SmbusHandler::performTransaction(ASBMgrRequest *req,
                                 ASBMgrTransactionCompletion completion,
                                 OSObject * target,
                                 void * reference)
{
    IOReturn ret;

    if (!fWorkLoop->inGate()) {
        BM_ERRLOG("Called submit smbus transaction outside the workloop\n");
    }
    BM_LOG2("opType:%d cmd:0x%x addr:0x%x\n", req->opType, req->command, req->address);

    if ((ret = isTransactionAllowed()) != kIOReturnSuccess) {
        BM_ERRLOG("Smbus transaction is not allowed\n");
        return ret;
    }

    bzero(&fTransaction, sizeof(fTransaction));
    fTransaction.address = req->address;
    fTransaction.command = req->command;
    fOpType = req->opType;

    fCompletion = completion;
    fFullyDischarged = req->fullyDischarged;
    fTarget = target;
    fReference = reference;
    fRetryAttempts = 0;

    switch (req->opType) {
        case kASBMSMBUSReadWord:
            fTransaction.protocol = kIOSMBusProtocolReadWord;
            break;

        case kASBMSMBUSExtendedReadWord:
            fTransaction.protocol = kIOSMBusProtocolWriteWord;
            fTransaction.sendData[0]   = req->command;
            fTransaction.sendData[1]   = 0;
            fTransaction.sendDataCount = 2;
            if ((req->command == kBExtendedPFStatusCmd) || (req->command == kBExtendedOperationStatusCmd)) {
                fTransaction.command = kBManufacturerAccessCmd;
            }
            else {
                BM_ERRLOG("Extended read for cmd 0x%x is not supported\n", req->command);
                return kIOReturnBadArgument;
            }

            BM_LOG2("Issuing write for extended read. proto:%d sendData:0x%x cmd:0x%x add:0x%x",
                    fTransaction.protocol, fTransaction.sendData[1] << 8 | fTransaction.sendData[0],
                    req->command, req->address);
            break;

        case kASBMSMBUSReadBlock:
            fTransaction.protocol = kIOSMBusProtocolReadBlock;
            break;

        case kASBMSMBUSWriteWord:
            fTransaction.protocol = kIOSMBusProtocolWriteWord;
            fTransaction.sendDataCount = 2;
            fTransaction.sendData[0] = req->outData[0];
            fTransaction.sendData[1] = req->outData[1];
            break;

        default:
            BM_ERRLOG("smbusRead received invalid opType: %d\n", req->opType);
            return kIOReturnInvalid;
    }

    fCmdCount++;
    ret = fMgr->fProvider->performTransaction(&fTransaction,
                                         OSMemberFunctionCast(IOSMBusTransactionCompletion,
                                                              this, &SmbusHandler::smbusCompletion),
                                         this, VOIDPTR(fCmdCount));

    if (ret != kIOReturnSuccess) {
        BM_ERRLOG("Smbus trasaction submission failed with error 0x%x\n", ret);
    }
    else {
        BM_LOG2("Command submitted  opType:%d cmd:0x%x addr:0x%x\n", req->opType, req->command, req->address);
    }
    return ret;
}

void SmbusHandler::smbusExternalTransactionCompletion(void *ref, IOSMBusTransaction *transaction)
{
    BM_LOG2("smbusExternalTransactionCompletion\n");
    fMgr->fManagerGate->commandWakeup(ref, false);
}

IOReturn SmbusHandler::getErrorCode(IOSMBusStatus status)
{
    switch (status) {
        case kIOSMBusStatusOK:
            return kIOReturnSuccess;
            break;
        case kIOSMBusStatusUnknownFailure:
        case kIOSMBusStatusDeviceAddressNotAcknowledged:
        case kIOSMBusStatusDeviceError:
        case kIOSMBusStatusDeviceCommandAccessDenied:
        case kIOSMBusStatusUnknownHostError:
            return kIOReturnNoDevice;
            break;
        case kIOSMBusStatusTimeout:
        case kIOSMBusStatusBusy:
            return kIOReturnTimeout;
            break;
        case kIOSMBusStatusHostUnsupportedProtocol:
            return kIOReturnUnsupported;
            break;
        default:
            return kIOReturnInternalError;
            break;
    }
    return kIOReturnInternalError;
}

IOReturn SmbusHandler::smbusExternalTransaction(void *in, void *out, IOByteCount inSize, IOByteCount *outSize)
{

    uint16_t                i;
    uint16_t                retryAttempts = 0;
    IOReturn                transactionSuccess;

    EXSMBUSInputStruct      *inSMBus = (EXSMBUSInputStruct *)in;
    EXSMBUSOutputStruct     *outSMBus = (EXSMBUSOutputStruct *)out;

    if (!inSMBus || !outSMBus)
        return kIOReturnBadArgument;

    if (inSize < sizeof(EXSMBUSInputStruct) || *outSize < sizeof(EXSMBUSOutputStruct)) {
        return kIOReturnBadArgument;
    }

    /* Attempt up to 5 transactions if we get failures
     */
    do {

        bzero(&fTransaction, sizeof(fTransaction));

        // Input: bus address
        if (kSMBusAppleDoublerAddr == inSMBus->batterySelector
            || kSMBusBatteryAddr == inSMBus->batterySelector
            || kSMBusManagerAddr == inSMBus->batterySelector
            || kSMBusChargerAddr == inSMBus->batterySelector)
        {
            fTransaction.address = inSMBus->batterySelector;
        } else {
            if (0 == inSMBus->batterySelector)
            {
                fTransaction.address = kSMBusBatteryAddr;
            } else {
                fTransaction.address = kSMBusManagerAddr;
            }
        }

        // Input: command
        fTransaction.command = inSMBus->address;

        // Input: Read/Write Word/Block
        switch (inSMBus->type) {
            case kEXWriteWord:
                fTransaction.protocol = kIOSMBusProtocolWriteWord;
                fTransaction.sendDataCount = 2;
                break;
            case kEXReadWord:
                fTransaction.protocol = kIOSMBusProtocolReadWord;
                fTransaction.sendDataCount = 0;
                break;
            case kEXWriteBlock:
                fTransaction.protocol = kIOSMBusProtocolWriteBlock;
                // rdar://5433060 workaround for SMC SMBus blockCount bug
                // For block writes, clients always increment inByteCount +1
                // greater than the actual byte count.
                // We decrement it here for IOSMBusController.
                fTransaction.sendDataCount = inSMBus->inByteCount - 1;
                break;
            case kEXReadBlock:
                fTransaction.protocol = kIOSMBusProtocolReadBlock;
                fTransaction.sendDataCount = 0;
                break;
            case kEXWriteByte:
                fTransaction.protocol = kIOSMBusProtocolWriteByte;
                fTransaction.sendDataCount = 1;
                break;
            case kEXReadByte:
                fTransaction.protocol = kIOSMBusProtocolReadByte;
                fTransaction.sendDataCount = 0;
                break;
            case kEXSendByte:
                fTransaction.protocol = kIOSMBusProtocolSendByte;
                fTransaction.sendDataCount = 0;
                break;
            default:
                return kIOReturnBadArgument;
        }

        if (inSMBus->flags & kEXFlagUsePEC) {
            fTransaction.options = kIOSMBusTransactionUsesPEC;
        }

        // Input: copy data into transaction
        //  only need to copy data for write operations
        if ((kIOSMBusProtocolWriteWord == fTransaction.protocol)
            || (kIOSMBusProtocolWriteBlock == fTransaction.protocol))
        {
            for(i = 0; i<MAX_SMBUS_DATA_SIZE; i++) {
                fTransaction.sendData[i] = inSMBus->inBuf[i];
            }
        }

        if (inSMBus->flags & kEXFlagRetry)
        {
            if (retryAttempts >= kRetryAttempts) {
                // Don't read off the end of the table...
                retryAttempts = kRetryAttempts - 1;
            }

            // If this is a retry-on-failure, spin for a few microseconds
            IODelay( retryDelaysTable[retryAttempts] );
        }

        if (retryAttempts == 0) {
            BM_LOG2("External Smbus request with cmd:0x%x protocol:0x%x address:0x%x",
                    fTransaction.command, fTransaction.protocol, fTransaction.address);
        }
        else {
            BM_LOG2("External Smbus request retry attempt %d for cmd:0x%x protocol:0x%x address:0x%x",
                    retryAttempts, fTransaction.command, fTransaction.protocol, fTransaction.address);
        }

        transactionSuccess = fMgr->fProvider->performTransaction(&fTransaction,
                                OSMemberFunctionCast(IOSMBusTransactionCompletion, this, &SmbusHandler::smbusExternalTransactionCompletion),
                                this, &fTransaction);

        /* Output: status */
        if (kIOReturnSuccess == transactionSuccess)
        {
            // Block here until the transaction is completed
        BM_LOG2("smbusExternalTransaction blocked\n");
            fMgr->fManagerGate->commandSleep(&fTransaction, THREAD_UNINT);
            outSMBus->status = kIOReturnSuccess;
        }
        else {
            BM_ERRLOG("SMBus transaction submit for external request failed with error 0x%x\n", transactionSuccess);
            return kIOReturnError;
        }

        outSMBus->status = getErrorCode(fTransaction.status);

        if (fTransaction.status !=kIOSMBusStatusOK) {
            BM_ERRLOG("SMBus external transaction failed with error 0x%x\n", fTransaction.status);
        }
        else {
            BM_LOG2("External transaction request completed\n");
        }

        /* Retry this transaction if we received a failure
         */
    } while ((inSMBus->flags & kEXFlagRetry)
             && (outSMBus->status != kIOReturnSuccess)
             && (++retryAttempts < kRetryAttempts));

    /* Output: read word/read block results */
    if (((kIOSMBusProtocolReadWord == fTransaction.protocol)
         || (kIOSMBusProtocolReadBlock == fTransaction.protocol)
         || (kIOSMBusProtocolReadByte == fTransaction.protocol))
        && (kIOSMBusStatusOK == fTransaction.status))
    {
        if (fTransaction.receiveDataCount > sizeof(outSMBus->outBuf)) {
            outSMBus->outByteCount = sizeof(outSMBus->outBuf);
        }
        else {
            outSMBus->outByteCount = fTransaction.receiveDataCount;
        }

        memcpy(outSMBus->outBuf, fTransaction.receiveData, outSMBus->outByteCount);
    }

    return kIOReturnSuccess;

}


void SmbusHandler::handleExclusiveAccess(bool exclusive)
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
    IORegistryEntry     *p = NULL;

    p = fMgr;
#if TARGET_OS_OSX
    while (p) {
        p = p->getParentEntry(gIOServicePlane);
        if (OSDynamicCast(IOACPIPlatformDevice, p)) {
            fACPIProvider = (IOACPIPlatformDevice *)p;
            break;
        }
    }
#endif

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
        fMgr->setProperty("BatteryUpdatesBlockedExclusiveAccess", true);
    } else {
        // Exclusive access disabled! restart polling
        fMgr->removeProperty("BatteryUpdatesBlockedExclusiveAccess");
    }
}

IOReturn SmbusHandler::disableInflow(int level)
{
    IOReturn  transactionSuccess;

    IOSMBusTransaction  transaction;

    transaction.address = kSMBusManagerAddr;
    transaction.command = kMStateContCmd;
    transaction.protocol = kIOSMBusProtocolWriteWord;
    transaction.sendDataCount = 2;

    if (level != 0) {
        transaction.sendData[0] = kMCalibrateBit;
    }
    else {
        transaction.sendData[0] = 0x0;
    }
    transactionSuccess = fMgr->fProvider->performTransaction(&transaction,
                                 OSMemberFunctionCast(IOSMBusTransactionCompletion, this,
                                     &SmbusHandler::smbusExternalTransactionCompletion), this, &transaction);

    /* Output: status */
    if (kIOReturnSuccess == transactionSuccess)
    {
        // Block here until the transaction is completed
        fMgr->fManagerGate->commandSleep(&transaction, THREAD_UNINT);
        BM_LOG1("Disable inflow(%d) status:%d\n", level, transaction.status);
        return getErrorCode(transaction.status);
    }
    else {
        BM_ERRLOG("SMBus transaction submit for external request failed with error 0x%x\n", transactionSuccess);
        return kIOReturnError;
    }

}

IOReturn SmbusHandler::inhibitCharging(int level)
{
    IOReturn  transactionSuccess;
    IOSMBusTransaction  transaction;

    transaction.address = kSMBusChargerAddr;
    transaction.command = kBChargingCurrentCmd;
    transaction.protocol = kIOSMBusProtocolWriteWord;
    transaction.sendDataCount = 2;

    if (level != 0) {
        transaction.sendData[0] = 0x0;
    }
    else {
        // We re-enable charging by setting it to 6000, a signifcantly
        // large number to ensure charging will resume.
        transaction.sendData[0] = 0x70;
        transaction.sendData[1] = 0x17;
    }
    transactionSuccess = fMgr->fProvider->performTransaction(&transaction,
                                 OSMemberFunctionCast(IOSMBusTransactionCompletion, this,
                                     &SmbusHandler::smbusExternalTransactionCompletion), this, &transaction);

    /* Output: status */
    if (kIOReturnSuccess == transactionSuccess)
    {
        // Block here until the transaction is completed
        fMgr->fManagerGate->commandSleep(&transaction, THREAD_UNINT);
        BM_LOG1("Inhibit charging(%d) status:%d\n", level, transaction.status);
        return getErrorCode(transaction.status);
    }
    else {
        BM_ERRLOG("SMBus transaction submit for external request failed with error 0x%x\n", transactionSuccess);
        return kIOReturnError;
    }
}
