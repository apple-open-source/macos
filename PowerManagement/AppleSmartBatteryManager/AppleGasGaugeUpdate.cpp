#include <TargetConditionals.h>

#if TARGET_OS_IOS || TARGET_OS_WATCH || (TARGET_OS_OSX && TARGET_CPU_ARM64)
#include "AppleGasGaugeUpdate.h"
#include "AppleGasGaugeUpdateUserClientPrivate.h"
#include "AppleSmartBatteryManager.h"
#include <IOKit/IOWorkLoop.h>

#define super IOService
OSDefineMetaClassAndStructors(AppleGasGaugeUpdate, IOService)

#define VOIDPTR(arg)  ((void *)(uintptr_t)(arg))
#define GG_UPD_LOG(fmt, args...) IOLog("AppleGasGaugeUpdate: " fmt, ## args)
#define GG_UPD_WARN(fmt, args...) IOLog("AppleGasGaugeUpdate WARN: " fmt, ## args)
#define GG_UPD_ERR(fmt, args...) IOLog("AppleGasGaugeUpdate ERROR: " fmt, ## args)
#define SMCKEY2CHARS(key) \
    (_displayKeys ? (((key) >> 24) & 0xff) : ' '), (_displayKeys ? (((key) >> 16) & 0xff) : ' '),\
            (_displayKeys ? (((key) >> 8) & 0xff) : ' '), (_displayKeys ? ((key) & 0xff) : ' ')

#define SMC_RESP_TIMEOUT      10
#define GG_SMCTRANSFER_LEN    32

static const OSSymbol *_kCommError = OSSymbol::withCString("CommunicationError");
static const OSSymbol *_kUpdaterStatus = OSSymbol::withCString("UpdaterStatus");
static const OSSymbol *_kBatteryId = OSSymbol::withCString("BatteryID");

void AppleGasGaugeUpdate::_clearErrorReports(void)
{
    removeProperty(_kCommError);
    removeProperty(_kUpdaterStatus);
}

IOReturn AppleGasGaugeUpdate::_smcGgFwUpdNotifierHandlerThreadGated(OSObject *param)
{
    OSArray *objArray = OSDynamicCast(OSArray, param);
    if (!objArray) {
        GG_UPD_ERR("invalid notification data: expected OSArray\n");
    }

    OSNumber *osNum = OSDynamicCast(OSNumber, objArray->getObject(0));
    if (!osNum) {
        GG_UPD_ERR("invalid notification data: expected OSNumber\n");
    }

    _receivedNotificationStatus.op = osNum->unsigned8BitValue();

    osNum = OSDynamicCast(OSNumber, objArray->getObject(1));
    if (!osNum) {
        GG_UPD_ERR("invalid notification data: expected OSNumber\n");
    }

    uint8_t i2c_status = osNum->unsigned8BitValue();
    _receivedNotificationStatus.i2cStatus = i2c_status;
    if (i2c_status) {
        GG_UPD_ERR("error in gas gauge communication (%x)\n", i2c_status);
        setProperty(_kCommError, osNum);
    }

    osNum = OSDynamicCast(OSNumber, objArray->getObject(2));
    if (!osNum) {
        GG_UPD_ERR("invalid notification data: expected OSNumber\n");
    }

    uint8_t ic_status = osNum->unsigned8BitValue();
    _receivedNotificationStatus.updaterStatus = ic_status;
    if (ic_status > 1 && ic_status != kRegUpdaterStatusSuccess) {
        GG_UPD_ERR("error in gas gauge update (%x)\n", ic_status);
        setProperty(_kUpdaterStatus, osNum);
    }

    _commandGate->commandWakeup(&_ggFwUpdEvent);

    param->release();

    return true;
}

IOReturn AppleGasGaugeUpdate::_smcGgFwUpdNotifierHandlerThread(void *param)
{
    return _commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this,
                                   &AppleGasGaugeUpdate::_smcGgFwUpdNotifierHandlerThreadGated),
                                   param);
}

IOReturn AppleGasGaugeUpdate::_smcGgFwUpdNotifierHandler(const OSSymbol *type,
                                                         OSObject *val,
                                                         uintptr_t refcon)
{
    if (type->isEqualTo(gGgFwUpdNotifySymbol) != true) {
        GG_UPD_LOG("invalid notification\n");
        return true;
    }

    val->retain();

    // dispatch into a different thread to avoid deadlocks when calling back
    // into AppleSMC due to ASBM creating its own workloop
    thread_call_enter1(_ggFwUpdSMCNotifThreadCall, val);

    return true;
}

#define MAX_TRIES    5
SMCResult AppleGasGaugeUpdate::_smcReadKey(SMCKey key, IOByteCount length, void *data)
{
    SMCResult ret = kSMCError;
    size_t retry = 0;

    do {
        if (retry) {
            GG_UPD_ERR("failed to read key '%c%c%c%c' retry:%zu rc:%#x=%s\n", SMCKEY2CHARS(key), retry, ret, printSMCResult(ret));
            IOSleep(700);
        }

        ret = _SMCDriver->smcReadKeyHostEndian(key, length, data);
    } while (ret != kSMCSuccess && retry++ < MAX_TRIES);

    if (ret != kSMCSuccess) {
        GG_UPD_ERR("failed to read key '%c%c%c%c' rc:%#x=%s\n", SMCKEY2CHARS(key), ret, printSMCResult(ret));
    }

    return ret;
}

SMCResult AppleGasGaugeUpdate::_smcWriteKey(SMCKey key, IOByteCount length, void *data)
{
    SMCResult ret = kSMCError;
    size_t retry = 0;

    do {
        if (retry) {
            GG_UPD_ERR("failed to write key '%c%c%c%c' retry:%zu rc:%#x=%s\n", SMCKEY2CHARS(key), retry, ret, printSMCResult(ret));
            IOSleep(700);
        }

        ret = _SMCDriver->smcWriteKey(key, length, data);
    } while (ret != kSMCSuccess && retry++ < MAX_TRIES);

    if (ret != kSMCSuccess) {
        GG_UPD_ERR("failed to write key '%c%c%c%c' rc:%#x=%s\n", SMCKEY2CHARS(key), ret, printSMCResult(ret));
    }

    return ret;
}

IOReturn AppleGasGaugeUpdate::_checkOperationStatus(uint8_t op)
{
    if (_receivedNotificationStatus.op != op) {
        GG_UPD_ERR("received notification for op:%u, expected op:%u\n",
                   _receivedNotificationStatus.op, op);
        return kIOReturnIOError;
    }

    if (_receivedNotificationStatus.i2cStatus) {
        GG_UPD_ERR("failure communicating with gas gauge:%x, op:%u\n",
                   _receivedNotificationStatus.i2cStatus, _receivedNotificationStatus.op);
        return kIOReturnIOError;
    }

    if (_receivedNotificationStatus.updaterStatus > kRegUpdaterStatusReady &&
        _receivedNotificationStatus.updaterStatus != kRegUpdaterStatusSuccess) {
        GG_UPD_ERR("updater error:%u, op:%u\n",
                   _receivedNotificationStatus.updaterStatus, _receivedNotificationStatus.op);
        return kIOReturnIOError;
    }

    return kIOReturnSuccess;
}

// Poll updater status to be non busy every @retry_ms ms for up to @timeout_ms  ms
IOReturn AppleGasGaugeUpdate::_pollUpdaterStatus(uint16_t *out_status, unsigned int timeout_ms, unsigned int retry_ms)
{
    SMCResult ret;
    AbsoluteTime dl;
    uint16_t status = kRegUpdaterStatusBusy;
    size_t retry = 0;
    size_t max_retries = timeout_ms / retry_ms + 1;

    if (!out_status) {
        return kIOReturnBadArgument;
    }

    // poll for status being not busy or until out of retries
    do {
        if (retry) {
            IOSleep(retry_ms);
        }

        uint8_t op = kGgFwUpdOpGetUpdStatus;
        ret = _smcWriteKey('BFUC', sizeof(op), &op);
        if (ret) {
            GG_UPD_ERR("Failed to set command 'read updater status'. rc:0x%x=%s\n", ret, printSMCResult(ret));
            return kIOReturnIOError;
        }

        clock_interval_to_deadline(SMC_RESP_TIMEOUT, kSecondScale, &dl);
        if (_commandGate->commandSleep(&_ggFwUpdEvent, dl, THREAD_UNINT) != THREAD_AWAKENED) {
            GG_UPD_WARN("timeout: waiting for updater status\n");
            continue;
        }

        ret = _checkOperationStatus(op);
        if (ret != kIOReturnSuccess) {
            // Veridian might be in a state that causes NACKs
            continue;
        }

        uint8_t buffer[GG_SMCTRANSFER_LEN] = {};
        ret = _smcReadKey('BFUD', sizeof(buffer), buffer);
        if (ret) {
            GG_UPD_WARN("Failed to read updater status. rc:0x%x=%s\n", ret, printSMCResult(ret));
            return kIOReturnIOError;
        }

        status = *((uint16_t *)buffer);
    } while (status == kRegUpdaterStatusBusy && retry++ < max_retries);

    *out_status = status;

    return kIOReturnSuccess;
}

IOReturn AppleGasGaugeUpdate::_writeDataLengthGated(uint16_t length, enum gg_fw_update_op op,
                                                    unsigned int typical_ms, unsigned int timeout_ms, unsigned int retry_ms,
                                                    unsigned int status)
{
    SMCResult ret;
    AbsoluteTime dl;
    uint8_t op_len = op;
    uint8_t buffer[GG_SMCTRANSFER_LEN];

    bzero(buffer, sizeof(buffer));
    *((uint16_t *)buffer) = length;
    ret = _smcWriteKey('BFUD', sizeof(buffer), buffer);
    if (ret) {
        GG_UPD_ERR("Failed to pass length for command '%u'. rc:0x%x=%s\n", op, ret, printSMCResult(ret));
        return kIOReturnIOError;
    }

    op_len = op;
    ret = _smcWriteKey('BFUC', sizeof(op_len), &op_len);
    if (ret) {
        GG_UPD_ERR("Failed to set command '%u'. rc:0x%x=%s\n", op, ret, printSMCResult(ret));
        return kIOReturnIOError;
    }

    clock_interval_to_deadline(SMC_RESP_TIMEOUT, kSecondScale, &dl);
    if (_commandGate->commandSleep(&_ggFwUpdEvent, dl, THREAD_UNINT) != THREAD_AWAKENED) {
        GG_UPD_ERR("timeout: waiting for command '%u'\n", op);
        // Assume we missed the notification and move on.
        // ReadKey will fail if something is truly wrong.
    }

    ret = _checkOperationStatus(op_len);
    if (ret != kIOReturnSuccess) {
        return ret;
    }

    IOSleep(typical_ms);

    uint16_t polled_status = kRegUpdaterStatusBusy;
    IOReturn rc = _pollUpdaterStatus(&polled_status, timeout_ms, retry_ms);
    if (rc) {
        return rc;
    }
    if (polled_status != status) {
        GG_UPD_ERR("unexpected updater status: actual:%u expected:%u op:%u\n",
                   polled_status, status, op);
        return kIOReturnTimeout;
    }

    return kIOReturnSuccess;
}

IOReturn AppleGasGaugeUpdate::_writeDataSignatureGated(const uint8_t *signature,
                                                       enum gg_fw_update_op op_sign1,
                                                       enum gg_fw_update_op op_sign2)
{
    SMCResult ret;
    IOReturn ioret;
    AbsoluteTime dl;
    uint8_t op;
    uint8_t buffer[GG_SMCTRANSFER_LEN];

    op = op_sign1;
    memcpy(buffer, &signature[0], GG_SMCTRANSFER_LEN);
    ret = _smcWriteKey('BFUD', sizeof(buffer), buffer);
    if (ret) {
        GG_UPD_ERR("Failed to pass signature1 for command '%u'. rc:0x%x=%s\n", op, ret, printSMCResult(ret));
        return kIOReturnIOError;
    }

    ret = _smcWriteKey('BFUC', sizeof(op), &op);
    if (ret) {
        GG_UPD_ERR("Failed to set command '%u'. rc:0x%x=%s\n", op, ret, printSMCResult(ret));
        return kIOReturnIOError;
    }

    clock_interval_to_deadline(SMC_RESP_TIMEOUT, kSecondScale, &dl);
    if (_commandGate->commandSleep(&_ggFwUpdEvent, dl, THREAD_UNINT) != THREAD_AWAKENED) {
        GG_UPD_ERR("timeout: waiting for command '%u'\n", op);
        // Assume we missed the notification and move on.
        // ReadKey will fail if something is truly wrong.
    }

    ioret = _checkOperationStatus(op);
    if (ioret != kIOReturnSuccess) {
        return ioret;
    }

    uint16_t polled_status = kRegUpdaterStatusBusy;
    ioret = _pollUpdaterStatus(&polled_status, 700, 80);
    if (ioret) {
        return ioret;
    }
    if (polled_status != kRegUpdaterStatusReady) {
        GG_UPD_ERR("unexpected updater status: actual:%u expected:%u op:%u\n",
                   polled_status, kRegUpdaterStatusReady, op);
        return kIOReturnTimeout;
    }

    op = op_sign2;
    memcpy(buffer, &signature[32], GG_SMCTRANSFER_LEN);
    ret = _smcWriteKey('BFUD', sizeof(buffer), buffer);
    if (ret) {
        GG_UPD_ERR("Failed to pass signature2 for command '%u'. rc:0x%x=%s\n", op, ret, printSMCResult(ret));
        return kIOReturnIOError;
    }

    ret = _smcWriteKey('BFUC', sizeof(op), &op);
    if (ret) {
        GG_UPD_ERR("Failed to set command '%u'. rc:0x%x=%s\n", op, ret, printSMCResult(ret));
        return kIOReturnIOError;
    }

    clock_interval_to_deadline(SMC_RESP_TIMEOUT, kSecondScale, &dl);
    if (_commandGate->commandSleep(&_ggFwUpdEvent, dl, THREAD_UNINT) != THREAD_AWAKENED) {
        GG_UPD_ERR("timeout: waiting for command '%u'\n", op);
        // Assume we missed the notification and move on.
        // ReadKey will fail if something is truly wrong.
    }

    ioret = _checkOperationStatus(op);
    if (ioret != kIOReturnSuccess) {
        return ioret;
    }

    polled_status = kRegUpdaterStatusBusy;
    ioret = _pollUpdaterStatus(&polled_status, 700, 80);
    if (ioret) {
        return ioret;
    }
    if (polled_status != kRegUpdaterStatusReady) {
        GG_UPD_ERR("unexpected updater status: actual:%u expected:%u op:%u\n",
                   polled_status, kRegUpdaterStatusReady, op);
        return kIOReturnTimeout;
    }

    return kIOReturnSuccess;
}

IOReturn AppleGasGaugeUpdate::_startImageGated(enum AppleGasGaugeUpdateUserClientDataType type)
{
    SMCResult ret;
    AbsoluteTime dl;
    uint8_t imageType;
    uint8_t op = kGgFwUpdOpStartImage;
    uint8_t buffer[GG_SMCTRANSFER_LEN];
    unsigned int typical_ms, retry_ms, timeout_ms;

    switch (type) {
    case kGgFwUpdDataTypeFirmwareImage:
        typical_ms = 220;
        timeout_ms = 1500;
        retry_ms = 700;
        imageType = kGgFwUpdTypeFirmware;
        break;
    case kGgFwUpdDataTypeDnvdImage:
        typical_ms = 40;
        timeout_ms = 1000;
        retry_ms = 70;
        imageType = kGgFwUpdTypeDnvd;
        break;
    case kGgFwUpdDataTypeConfigImage:
        typical_ms = 40;
        timeout_ms = 1000;
        retry_ms = 70;
        imageType = kGgFwUpdTypeConfiguration;
        break;
    default:
        return kIOReturnBadArgument;
        break;
    }

    bzero(buffer, sizeof(buffer));
    buffer[0] = imageType;
    ret = _smcWriteKey('BFUD', sizeof(buffer), buffer);
    if (ret) {
        GG_UPD_ERR("Failed to pass image type '%u' for command '%u'. rc:0x%x=%s\n", imageType, op, ret, printSMCResult(ret));
        return kIOReturnIOError;
    }

    ret = _smcWriteKey('BFUC', sizeof(op), &op);
    if (ret) {
        GG_UPD_ERR("Failed to set command '%u'. rc:0x%x=%s\n", op, ret, printSMCResult(ret));
        return kIOReturnIOError;
    }

    clock_interval_to_deadline(SMC_RESP_TIMEOUT, kSecondScale, &dl);
    if (_commandGate->commandSleep(&_ggFwUpdEvent, dl, THREAD_UNINT) != THREAD_AWAKENED) {
        GG_UPD_ERR("timeout: waiting for command '%u'\n", op);
        // Assume we missed the notification and move on.
        // ReadKey will fail if something is truly wrong.
    }

    ret = _checkOperationStatus(op);
    if (ret != kIOReturnSuccess) {
        return ret;
    }

    IOSleep(typical_ms);

    uint16_t polled_status = kRegUpdaterStatusBusy;
    IOReturn rc = _pollUpdaterStatus(&polled_status, timeout_ms, retry_ms);
    if (rc) {
        return rc;
    }
    if (polled_status != kRegUpdaterStatusReady) {
        GG_UPD_ERR("unexpected updater status: actual:%u expected:%u op:%u\n",
                   polled_status, kRegUpdaterStatusReady, op);
        return kIOReturnTimeout;
    }

    return kIOReturnSuccess;
}

IOReturn AppleGasGaugeUpdate::_writeDataDataGated(const uint8_t *data, size_t data_length,
                                                  enum gg_fw_update_op op)
{
    SMCResult ret;
    AbsoluteTime dl;
    uint8_t op_data;
    uint8_t buffer[GG_SMCTRANSFER_LEN];

    size_t offset = 0;
    size_t data_remaining = data_length;
    GG_UPD_LOG("writeDataData checkpoint: start transfer of %zu bytes\n", data_remaining);
    while (data_remaining) {
        size_t copy_size = min(GG_SMCTRANSFER_LEN, data_remaining);

        if (copy_size < GG_SMCTRANSFER_LEN) {
            bzero(buffer, sizeof(buffer));
        }

        memcpy(buffer, &data[offset], copy_size);
        ret = _smcWriteKey('BFUD', sizeof(buffer), buffer);
        if (ret) {
            GG_UPD_ERR("Failed to pass data for command '%u'at offset:%zu. rc:0x%x=%s\n", op, offset, ret, printSMCResult(ret));
            return kIOReturnIOError;
        }

        op_data = op;
        ret = _smcWriteKey('BFUC', sizeof(op_data), &op_data);
        if (ret) {
            GG_UPD_ERR("Failed to set command '%u' at offset:%zu. rc:0x%x=%s\n", op, offset, ret, printSMCResult(ret));
            return kIOReturnIOError;
        }

        clock_interval_to_deadline(SMC_RESP_TIMEOUT, kSecondScale, &dl);
        if (_commandGate->commandSleep(&_ggFwUpdEvent, dl, THREAD_UNINT) != THREAD_AWAKENED) {
            GG_UPD_ERR("timeout: waiting for command '%u' at offset %zu\n", op, offset);
            // Assume we missed the notification and move on.
            // ReadKey will fail if something is truly wrong.
        }

        ret = _checkOperationStatus(op_data);
        if (ret != kIOReturnSuccess) {
            GG_UPD_ERR("Operation '%u' failed at offset:%zu. rc:0x%x=%s\n", op, offset, ret, printSMCResult(ret));
            return ret;
        }

        offset += copy_size;
        data_remaining -= copy_size;

        IOSleep(15);

        uint16_t polled_status = kRegUpdaterStatusBusy;
        IOReturn rc = _pollUpdaterStatus(&polled_status, 700, 70);
        if (rc) {
            GG_UPD_ERR("Failed to poll updater status. op:%u offset:%zu rc:0x%x\n", op, offset, rc);
            return rc;
        }
        if (polled_status != kRegUpdaterStatusReady) {
            GG_UPD_ERR("unexpected updater status: actual:%u expected:%u op:%u offset:%zu\n",
                       polled_status, kRegUpdaterStatusReady, op_data, offset);
            return kIOReturnTimeout;
        }
    }

    return kIOReturnSuccess;
}

IOReturn AppleGasGaugeUpdate::_startCryptoGated(uint8_t images)
{
    SMCResult ret;
    AbsoluteTime dl;
    uint8_t types = 0;
    uint8_t buffer[GG_SMCTRANSFER_LEN];

    bzero(buffer, sizeof(buffer));

    if (images & kGgFwUpdateTypeFirmware) {
        types |= kGgFwUpdTypeFirmware;
    }
    if (images & kGgFwUpdateTypeConfiguration) {
        types |= kGgFwUpdTypeConfiguration;
    }
    if (images & kGgFwUpdateTypeDnvd) {
        types |= kGgFwUpdTypeDnvd;
    }

    if (!types) {
        return kIOReturnBadArgument;
    }

    buffer[0] = types;
    ret = _smcWriteKey('BFUD', sizeof(buffer), buffer);
    if (ret) {
        GG_UPD_ERR("Failed to pass image types '%u'. rc:0x%x=%s\n", types, ret, printSMCResult(ret));
        return kIOReturnIOError;
    }

    uint8_t op = kGgFwUpdOpStartCrypto;
    ret = _smcWriteKey('BFUC', sizeof(op), &op);
    if (ret) {
        GG_UPD_ERR("Failed to set command '%u'. rc:0x%x=%s\n", op, ret, printSMCResult(ret));
        return kIOReturnIOError;
    }

    clock_interval_to_deadline(SMC_RESP_TIMEOUT, kSecondScale, &dl);
    if (_commandGate->commandSleep(&_ggFwUpdEvent, dl, THREAD_UNINT) != THREAD_AWAKENED) {
        GG_UPD_ERR("timeout: waiting for command '%u'\n", op);
        // Assume we missed the notification and move on.
        // ReadKey will fail if something is truly wrong.
    }

    ret = _checkOperationStatus(op);
    if (ret != kIOReturnSuccess) {
        return ret;
    }

    IOSleep(12000);  // typical 12s, ca 20s when under load

    uint16_t polled_status = kRegUpdaterStatusBusy;
    IOReturn rc = _pollUpdaterStatus(&polled_status, 27000, 5000);
    if (rc) {
        return rc;
    }
    if (polled_status != kRegUpdaterStatusReady) {
        GG_UPD_ERR("unexpected updater status: actual:%u expected:%u op:%u\n",
                   polled_status, kRegUpdaterStatusReady, op);
        return kIOReturnTimeout;
    }

    return kIOReturnSuccess;
}

IOReturn AppleGasGaugeUpdate::_writeDataGated(const struct AppleGasGaugeUpdateUserClientIData *data,
                                              const struct AppleGasGaugeUpdateOpsArgs *ops)
{
    IOReturn ioret;
    uint16_t status;

    GG_UPD_LOG("writeData checkpoint: start\n");

    ioret = _pollUpdaterStatus(&status);
    if (ioret != kIOReturnSuccess) {
        return ioret;
    }

    if (data->type == kGgFwUpdDataTypeDnvdImage || data->type == kGgFwUpdDataTypeConfigImage || data->type == kGgFwUpdDataTypeFirmwareImage) {
        if (status != kRegUpdaterStatusReady && status != kRegUpdaterStatusSuccess && status != kRegUpdaterStatusReset) {
            GG_UPD_ERR("updater in invalid state '%u'\n", status);
            return kIOReturnInternalError;
        }
    } else {
        if (status != kRegUpdaterStatusReady) {
            GG_UPD_ERR("updater in invalid state '%u'\n", status);
            return kIOReturnInternalError;
        }
    }

    GG_UPD_LOG("writeData checkpoint: status OK\n");

    if (data->type == kGgFwUpdDataTypeDnvdImage ||
        data->type == kGgFwUpdDataTypeConfigImage ||
        data->type == kGgFwUpdDataTypeFirmwareImage) {
        // signal image start
        ioret = _startImageGated(data->type);
        if (ioret != kIOReturnSuccess) {
            return ioret;
        }
    } else {
        // write size
        ioret = _writeDataLengthGated(data->data_length & 0xffff, ops->op_length, 15, 700, 100, kRegUpdaterStatusReady);
        if (ioret != kIOReturnSuccess) {
            return ioret;
        }
    }

    GG_UPD_LOG("writeData checkpoint: init done\n");

    // transfer image
    ioret = _writeDataDataGated(data->data, data->data_length, ops->op_data);
    if (ioret != kIOReturnSuccess) {
        return ioret;
    }

    GG_UPD_LOG("writeData checkpoint: transfer done\n");

    if (data->type == kGgFwUpdDataTypeDnvdImage ||
        data->type == kGgFwUpdDataTypeConfigImage ||
        data->type == kGgFwUpdDataTypeFirmwareImage) {
        unsigned int typical_ms, retry_ms, timeout_ms, status;

        switch (data->type) {
        case kGgFwUpdDataTypeDnvdImage:
            typical_ms = 500;
            timeout_ms = 2000;
            retry_ms = 700;
            if (_ggTwoStageUpdate) {
                status = kRegUpdaterStatusReady;
            } else {
                status = kRegUpdaterStatusSuccess;
            }
            break;
        case kGgFwUpdDataTypeConfigImage:
            typical_ms = 1500;
            timeout_ms = 10000;
            retry_ms = 700;
            if (_ggTwoStageUpdate) {
                status = kRegUpdaterStatusReady;
            } else {
                status = kRegUpdaterStatusReset;
            }
            break;
        case kGgFwUpdDataTypeFirmwareImage:
            typical_ms = 3000;
            timeout_ms = 16000;
            retry_ms = 1000;
            if (_ggTwoStageUpdate) {
                status = kRegUpdaterStatusReady;
            } else {
                status = kRegUpdaterStatusReset;
            }
            break;
        default:
            return kIOReturnInternalError;
        }

        // write size in blocks!!
        size_t blocks = (data->data_length + 31) / 32;
        ioret = _writeDataLengthGated(blocks & 0xffff, ops->op_length, typical_ms, timeout_ms, retry_ms, status);
        if (ioret != kIOReturnSuccess) {
            return ioret;
        }
    } else if (data->type == kGgFwUpdDataTypeDigestDictionary) {
        // start crypto
        ioret = _startCryptoGated(data->updateImage);
        if (ioret != kIOReturnSuccess) {
            return ioret;
        }
    } else {
        // write signature
        ioret = _writeDataSignatureGated(data->signature, ops->op_sign1, ops->op_sign2);
        if (ioret != kIOReturnSuccess) {
            return ioret;
        }
    }

    _clearErrorReports();
    GG_UPD_LOG("writeData checkpoint: done\n");

    return kIOReturnSuccess;
}

IOReturn AppleGasGaugeUpdate::_writeImage(const struct AppleGasGaugeUpdateUserClientIData *data)
{
    const struct AppleGasGaugeUpdateOpsArgs ops = {
        .op_length = kGgFwUpdOpSetDone, .op_data = kGgFwUpdOpSetImageData,
    };

    return _commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this,
                                   &AppleGasGaugeUpdate::_writeDataGated), (void *)data,
                                   (void *)&ops);
}

IOReturn AppleGasGaugeUpdate::_writeImg4Manifest(const struct AppleGasGaugeUpdateUserClientIData *data)
{
    const struct AppleGasGaugeUpdateOpsArgs ops = {
        .op_length = kGgFwUpdOpSetImg4Len, .op_data = kGgFwUpdOpSetImg4,
        .op_sign1 = kGgFwUpdOpSetImg4Sign1, .op_sign2 = kGgFwUpdOpSetImg4Sign2
    };

    IOReturn ret = _commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this,
                                           &AppleGasGaugeUpdate::_writeDataGated), (void *)data,
                                           (void *)&ops);

    return ret;
}

IOReturn AppleGasGaugeUpdate::_writeDigestDict(const struct AppleGasGaugeUpdateUserClientIData *data)
{
    const struct AppleGasGaugeUpdateOpsArgs ops = {
        .op_length = kGgFwUpdOpSetDigDictLen, .op_data = kGgFwUpdOpSetDigDict,
    };

    if (!data->updateImage) {
        return kIOReturnBadArgument;
    }

    IOReturn ret = _commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this,
                                           &AppleGasGaugeUpdate::_writeDataGated), (void *)data,
                                           (void *)&ops);

    return ret;
}

IOReturn AppleGasGaugeUpdate::_writeCertificate(const struct AppleGasGaugeUpdateUserClientIData *data)
{
    const struct AppleGasGaugeUpdateOpsArgs ops = {
        .op_length = kGgFwUpdOpSetCertLen, .op_data = kGgFwUpdOpSetCert,
        .op_sign1 = kGgFwUpdOpSetCertSign1, .op_sign2 = kGgFwUpdOpSetCertSign2
    };

    return _commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this,
                                   &AppleGasGaugeUpdate::_writeDataGated), (void *)data,
                                   (void *)&ops);
}

IOReturn AppleGasGaugeUpdate::_sendData(const struct AppleGasGaugeUpdateUserClientIData *data)
{
    GG_UPD_LOG("_sendData: type=%llu, length=%llu\n", data->type, data->data_length);

    switch (data->type) {
    case kGgFwUpdDataTypeCertificate:
        return _writeCertificate(data);
        break;

    case kGgFwUpdDataTypeImg4Manifest:
        return _writeImg4Manifest(data);
        break;

    case kGgFwUpdDataTypeDigestDictionary:
        return _writeDigestDict(data);
        break;

    case kGgFwUpdDataTypeDnvdImage:
    case kGgFwUpdDataTypeConfigImage:
    case kGgFwUpdDataTypeFirmwareImage:
        return _writeImage(data);
        break;

    default:
        return kIOReturnBadArgument;
        break;
    }
}

IOReturn AppleGasGaugeUpdate::_getBatteryIdGated(uint32_t *battId)
{
    SMCResult ret;
    AbsoluteTime dl;

    if (!battId) {
        return kIOReturnBadArgument;
    }

    uint8_t op = kGgFwUpdOpGetUid;
    ret = _smcWriteKey('BFUC', sizeof(op), &op);
    if (ret) {
        GG_UPD_ERR("Failed to set command 'read UID'. rc:0x%x\n", ret);
        return kIOReturnIOError;
    }

    clock_interval_to_deadline(SMC_RESP_TIMEOUT, kSecondScale, &dl);
    if (_commandGate->commandSleep(&_ggFwUpdEvent, dl, THREAD_UNINT) != THREAD_AWAKENED) {
        GG_UPD_ERR("timeout: waiting for UID\n");
        // Assume we missed the notification and move on.
        // ReadKey will fail if something is truly wrong.
    }

    ret = _checkOperationStatus(op);
    if (ret != kIOReturnSuccess) {
        return ret;
    }

    uint8_t buffer[GG_SMCTRANSFER_LEN] = {};
    ret = _smcReadKey('BFUD', sizeof(buffer), buffer);
    if (ret) {
        GG_UPD_ERR("Failed to read UID. rc:0x%x=%s\n", ret, printSMCResult(ret));
        return kIOReturnIOError;
    }

    *battId = *((uint32_t *)buffer);
    OSNumber *bid = OSNumber::withNumber(*battId, sizeof(*battId) * 8);
    if (bid) {
        setProperty(_kBatteryId, bid);
        OSSafeReleaseNULL(bid);
    }

    return kIOReturnSuccess;
}

IOReturn AppleGasGaugeUpdate::_getChallengeDataGated(uint8_t *data)
{
    SMCResult ret;
    AbsoluteTime dl;

    if (!data) {
        return kIOReturnBadArgument;
    }

    uint8_t op = kGgFwUpdOpGetNonce;
    ret = _smcWriteKey('BFUC', sizeof(op), &op);
    if (ret) {
        GG_UPD_ERR("Failed to set command 'read nonce'. rc:0x%x\n", ret);
        return kIOReturnIOError;
    }

    clock_interval_to_deadline(SMC_RESP_TIMEOUT, kSecondScale, &dl);
    if (_commandGate->commandSleep(&_ggFwUpdEvent, dl, THREAD_UNINT) != THREAD_AWAKENED) {
        GG_UPD_ERR("timeout: waiting for nonce\n");
        // Assume we missed the notification and move on.
        // ReadKey will fail if something is truly wrong.
    }

    ret = _checkOperationStatus(op);
    if (ret != kIOReturnSuccess) {
        return ret;
    }

    ret = _smcReadKey('BFUD', GG_SMCTRANSFER_LEN, data);
    if (ret) {
        GG_UPD_ERR("Failed to read nonce. rc:0x%x=%s\n", ret, printSMCResult(ret));
        return kIOReturnIOError;
    }

    return kIOReturnSuccess;
}

IOReturn AppleGasGaugeUpdate::_initUpdateGated(void)
{
    SMCResult ret;
    AbsoluteTime dl;

    uint8_t op = kGgFwUpdOpStartUpdate;
    ret = _smcWriteKey('BFUC', sizeof(op), &op);
    if (ret) {
        GG_UPD_ERR("Failed to set command 'start update'. rc:0x%x=%s\n", ret, printSMCResult(ret));
        return kIOReturnIOError;
    }

    clock_interval_to_deadline(SMC_RESP_TIMEOUT, kSecondScale, &dl);
    if (_commandGate->commandSleep(&_ggFwUpdEvent, dl, THREAD_UNINT) != THREAD_AWAKENED) {
        GG_UPD_ERR("timeout: waiting for nonce\n");
        // Assume we missed the notification and move on.
        // ReadKey will fail if something is truly wrong.
    }

    ret = _checkOperationStatus(op);
    if (ret != kIOReturnSuccess) {
        return ret;
    }

    IOSleep(40);

    uint16_t polled_status = kRegUpdaterStatusBusy;
    IOReturn rc = _pollUpdaterStatus(&polled_status, 1500, 70);
    if (rc) {
        return rc;
    }
    if (polled_status != kRegUpdaterStatusReady) {
        GG_UPD_ERR("unexpected updater status: actual:%u expected:%u op:%u\n",
                   polled_status, kRegUpdaterStatusReady, op);
        return kIOReturnTimeout;
    }

    return kIOReturnSuccess;
}

IOReturn AppleGasGaugeUpdate::_startUpdateGated(struct AppleGasGaugeUpdateUserClientOData *data)
{
    IOReturn ret;

    if (!data) {
        return kIOReturnBadArgument;
    }

    _clearErrorReports();

    ret =_initUpdateGated();
    if (ret != kIOReturnSuccess) {
        return ret;
    }

    ret =_getBatteryIdGated(&data->batteryId);
    if (ret != kIOReturnSuccess) {
        return ret;
    }

    ret =_getChallengeDataGated(data->nonce);
    if (ret != kIOReturnSuccess) {
        return ret;
    }

    return kIOReturnSuccess;
}

IOReturn AppleGasGaugeUpdate::_commitImageGated(void)
{
    IOReturn ret;

    uint8_t op = kGgFwUpdOpCommitImage;
    ret = _smcWriteKey('BFUC', sizeof(op), &op);
    if (ret) {
        GG_UPD_ERR("Failed to set command 'commit image'. rc:0x%x\n", ret);
        return kIOReturnIOError;
    }

    AbsoluteTime dl;
    clock_interval_to_deadline(SMC_RESP_TIMEOUT, kSecondScale, &dl);
    if (_commandGate->commandSleep(&_ggFwUpdEvent, dl, THREAD_UNINT) != THREAD_AWAKENED) {
        GG_UPD_ERR("timeout: waiting for image commit\n");
        // Assume we missed the notification and move on.
        // ReadKey will fail if something is truly wrong.
    }

    ret = _checkOperationStatus(op);
    if (ret != kIOReturnSuccess) {
        return ret;
    }

    IOSleep(3000); // Typically 3s: Jump to bootload, erase flash area and copy data from scratchpad to flash

    uint16_t polled_status = kRegUpdaterStatusBusy;
    IOReturn rc = _pollUpdaterStatus(&polled_status, 7000, 100);
    if (rc) {
        return rc;
    }
    if (polled_status != kRegUpdaterStatusSuccess) {
        GG_UPD_ERR("unexpected updater status: actual:%u expected:%u op:%u\n",
                   polled_status, kRegUpdaterStatusSuccess, op);
        return kIOReturnTimeout;
    }

    return kIOReturnSuccess;
}

IOReturn AppleGasGaugeUpdate::_startUpdate(struct AppleGasGaugeUpdateUserClientOData *data)
{
    return _commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this,
                                   &AppleGasGaugeUpdate::_startUpdateGated), data);
}

IOReturn AppleGasGaugeUpdate::_commitImage(void)
{
    return _commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this,
                                   &AppleGasGaugeUpdate::_commitImageGated), NULL);
}

IOReturn AppleGasGaugeUpdate::_getInfoGated(struct AppleGasGaugeUpdateUserClientInfo *data)
{
    SMCResult ret;
    AbsoluteTime dl;

    if (!data) {
        return kIOReturnBadArgument;
    }

    uint8_t op = kGgFwUpdOpGetInfo;
    ret = _smcWriteKey('BFUC', sizeof(op), &op);
    if (ret) {
        GG_UPD_ERR("Failed to set command 'read info'. rc:0x%x=%s\n", ret, printSMCResult(ret));
        return kIOReturnIOError;
    }

    clock_interval_to_deadline(SMC_RESP_TIMEOUT, kSecondScale, &dl);
    if (_commandGate->commandSleep(&_ggFwUpdEvent, dl, THREAD_UNINT) != THREAD_AWAKENED) {
        GG_UPD_ERR("timeout: waiting for op '%u'\n", op);
        // Assume we missed the notification and move on.
        // ReadKey will fail if something is truly wrong.
    }

    ret = _checkOperationStatus(op);
    if (ret != kIOReturnSuccess) {
        return ret;
    }

    uint8_t buffer[GG_SMCTRANSFER_LEN] = {};
    ret = _smcReadKey('BFUD', sizeof(buffer), buffer);
    if (ret) {
        GG_UPD_ERR("Failed to read info. rc:0x%x=%s\n", ret, printSMCResult(ret));
        return kIOReturnIOError;
    }

    struct gg_fw_update_info *info = (gg_fw_update_info *)buffer;

    data->fwVersion = info->fw_version;
    data->configVersion = info->config_version;
    data->dnvd1Version = info->dnvd1_version;
    data->dnvd2Version = info->dnvd2_version;
    data->cryptoVersion = info->crypto_version;
    data->chipId = info->chip_id;
    data->deviceType = info->device_type;
    data->chemistry = info->chemistry;

    op = kGgFwUpdOpGetInfo2;
    ret = _smcWriteKey('BFUC', sizeof(op), &op);
    if (ret) {
        GG_UPD_ERR("Failed to set command 'read info'. rc:0x%x=%s\n", ret, printSMCResult(ret));
        return kIOReturnIOError;
    }

    clock_interval_to_deadline(SMC_RESP_TIMEOUT, kSecondScale, &dl);
    if (_commandGate->commandSleep(&_ggFwUpdEvent, dl, THREAD_UNINT) != THREAD_AWAKENED) {
        GG_UPD_ERR("timeout: waiting for op '%u'\n", op);
        // Assume we missed the notification and move on.
        // ReadKey will fail if something is truly wrong.
    }

    ret = _checkOperationStatus(op);
    if (ret != kIOReturnSuccess) {
        return ret;
    }

    ret = _smcReadKey('BFUD', sizeof(buffer), buffer);
    if (ret) {
        GG_UPD_ERR("Failed to read info2. rc:0x%x=%s\n", ret, printSMCResult(ret));
        return kIOReturnIOError;
    }

    struct gg_fw_update_info2 *info2 = (gg_fw_update_info2 *)buffer;

    data->hwId = info2->hw_id;
    data->updaterStatus = info2->updater_status;
    data->flags.isTwoStageSupported = info2->flags.is_two_stage_update;
    _ggTwoStageUpdate = data->flags.isTwoStageSupported;
    memcpy(data->setIdsSupported, info2->set_ids_supported, 12);
    memcpy(data->setIds, info2->set_ids, 12);

    return kIOReturnSuccess;
}

IOReturn AppleGasGaugeUpdate::_getInfo(struct AppleGasGaugeUpdateUserClientInfo *data)
{
    return _commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this,
                                   &AppleGasGaugeUpdate::_getInfoGated), data);
}

bool AppleGasGaugeUpdate::start(IOService *provider)
{
    SMCResult rc;
    OSObject *handle;

    if (!super::start(provider)) {
        return false;
    }

    AppleSmartBattery *asbm = OSDynamicCast(AppleSmartBattery, provider);
    if (!provider) {
        GG_UPD_ERR("invalid provider\n");
    }

    _provider = asbm;

    _workLoop = IOWorkLoop::workLoop();
    if (!_workLoop) {
        return false;
    }

    _commandGate = IOCommandGate::commandGate(this);
    if (!_commandGate) {
        goto err;
    }

    _workLoop->addEventSource(_commandGate);

    _ggFwUpdSMCNotifThreadCall = thread_call_allocate_with_options(
        OSMemberFunctionCast(thread_call_func_t,
            this, &AppleGasGaugeUpdate::_smcGgFwUpdNotifierHandlerThread), this,
            THREAD_CALL_PRIORITY_USER,
            THREAD_CALL_OPTIONS_ONCE);
    if (!_ggFwUpdSMCNotifThreadCall) {
        return kIOReturnInternalError;
    }

    rc = _SMCDriver->smcRegisterNotificationController(
                /* type    */  gGgFwUpdNotifySymbol,
                /* handler */  OSMemberFunctionCast(
                    SMCNotificationCallback, this,
                    &AppleGasGaugeUpdate::_smcGgFwUpdNotifierHandler),
                /* target  */  this,
                /* refcon  */  0 ,
                /* handle */   &handle);
    if (rc != kSMCSuccess) {
        GG_UPD_ERR("failed to register notifier\n");
        goto err;
    }

    registerService();

    return true;

err:
    OSSafeReleaseNULL(_workLoop);
    OSSafeReleaseNULL(_commandGate);

    return false;
}

IOReturn AppleGasGaugeUpdate::newUserClient(task_t owningTask, void *securityID, UInt32 type, IOUserClient **handler)
{
    IOUserClient *uc = new AppleGasGaugeUpdateUserClient;
    if (!uc) {
        GG_UPD_ERR("failed to create UserClient\n");
        return kIOReturnNoMemory;
    }

    if (!uc->initWithTask(owningTask, securityID, type)) {
        uc->release();
        GG_UPD_ERR("failed to initialize UserClient\n");
        kIOReturnBadArgument;
    }

    uc->attach(this);
    uc->start(this);

    *handler = uc;

    return kIOReturnSuccess;
}

IOWorkLoop *AppleGasGaugeUpdate::getWorkLoop() const
{
    return _workLoop;
}

void AppleGasGaugeUpdate::free(void)
{
    OSSafeReleaseNULL(_workLoop);
    OSSafeReleaseNULL(_commandGate);
    super::free();
}

AppleGasGaugeUpdate *AppleGasGaugeUpdate::withSMCFamily(AppleSMCFamily *smc)
{
    if (!smc) {
        return nullptr;
    }

    AppleGasGaugeUpdate *me = new AppleGasGaugeUpdate;
    if (!me) {
        return nullptr;
    }

#if TARGET_OS_OSX
    me->_displayKeys  = false;
#else
    me->_displayKeys = PE_i_can_has_debugger(NULL);
#endif
    me->_SMCDriver = smc;
    me->_workLoop = NULL;
    me->init();

    return me;
}

#endif // TARGET_OS_IOS || TARGET_OS_WATCH || (TARGET_OS_OSX && TARGET_CPU_ARM64)
