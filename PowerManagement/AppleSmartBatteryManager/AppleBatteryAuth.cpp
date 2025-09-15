#include <TargetConditionals.h>
#if TARGET_OS_IOS || TARGET_OS_WATCH || (TARGET_OS_OSX && TARGET_CPU_ARM64)

#include "AppleBatteryAuth.h"
#include "AppleBatteryAuthKeys.h"
#include "AppleBatteryAuthKeysPrivate.h"
#include "AppleSmartBatteryManager.h"
#include <IOKit/IOWorkLoop.h>
#include <IOKit/accessory/AppleAuthCP.h>
#include <IOKit/smc/AppleSMCFamily.h>

/* This enables debug logging for this module */
#define BA_DBG_LOG_ENABLED  1

#if BA_DBG_LOG_ENABLED
#define BA_DBG(fmt, args...) IOLog("AppleBatteryAuth: DBG: " fmt, ## args)
#else
#define BA_DBG(fmt, args...)
#endif

#define BA_LOG(fmt, args...) IOLog("AppleBatteryAuth: " fmt, ## args)
#define BA_ERR(fmt, args...) IOLog("AppleBatteryAuth: " fmt, ## args)

//#define super OSObject
OSDefineMetaClassAndStructors(AppleBatteryAuth, AppleAuthCPRelayInterface)

#define VOIDPTR(arg)  ((void *)(uintptr_t)(arg))
#define SMCKEY2CHARS(key) \
    (fDisplayKeys ? (((key) >> 24) & 0xff) : ' '), (fDisplayKeys ? (((key) >> 16) & 0xff) : ' '),\
            (fDisplayKeys ? (((key) >> 8) & 0xff) : ' '), (fDisplayKeys ? ((key) & 0xff) : ' ')

#define DIV_ROUND_UP(x, y)  (((x) + ((y) - 1)) / (y))

#define MAX_TRIES               5
#define POLL_DELAY_SECS         3

#define BATT_AUTH_CERTSERIAL_LEN    32
#define BATT_AUTH_CHALLENGE_LEN     32
#define BATT_AUTH_SIGNATURE_LEN     64
#define BATT_AUTH_SMC_DATA_LEN      64
#define BATT_AUTH_STATUS_LEN        2

#define BATT_AUTH_MAX_CERT_LENGTH     609    // max length for certificate

/* Boot arg values */
#define kAppleBatteryAuthBootArgDefault   (0)   // Default
#define kAppleBatteryAuthBootArgForceAuth (1)   // Force publishing of trusted data as soon as we get nonce

#define kIOReportNumberOfReporters  1
#define kIOReportBatteryAuthGroupName "BatteryAuth"
#define kReportCategoryBatteryAuth (kIOReportCategoryPower | kIOReportCategoryField | kIOReportCategoryDebug)
#define kAuthCommandIDGetCertSNID IOREPORT_MAKEID('c', 'e', 'r', 's', 'n', 'r', 'e', 't')
#define kAuthCommandIDGetInfoID IOREPORT_MAKEID('i', 'n', 'f', 'o', ' ', 'r', 'e', 't')
#define kAuthCommandIDGetSignatureID IOREPORT_MAKEID('s', 'i', 'g', 'n', ' ', 'r', 'e', 't')
#define kAuthCommandIDGetCertID IOREPORT_MAKEID('c', 'e', 'r', 't', ' ', 'r', 'e', 't')
#define kAuthCommandIDSetTrustStatusID IOREPORT_MAKEID('t', 'r', 'u', 's', 't', 'r', 'e', 't')
#define kGGResetCountID IOREPORT_MAKEID('G', 'G', 'r', 's', 't', 'c', 'n', 't')

#define SET_INTEGER_IN_DICT(dict, key, value, width) \
do { \
    OSNumber *_num = OSNumber::withNumber((unsigned long long)(value), width); \
    if (_num) { \
        (dict)->setObject((key), _num); \
        _num->release();  \
    } \
} while (0)

static const OSSymbol *_kCommError = OSSymbol::withCString("CommunicationError");
static const OSSymbol *_kProcError = OSSymbol::withCString("CoProcError");
static const OSSymbol *_kTrustedBatteryDataNonce = OSSymbol::withCString(kBatteryAuthTrustedDataRawKey);
static const OSSymbol *_kTrustedBatteryDataEn = OSSymbol::withCString(kBatteryAuthTrustedDataEnKey);
static const OSSymbol *_kTrustedBatteryLastTS = OSSymbol::withCString(kBatteryAuthTrustedDataTSKey);
static const OSSymbol *_kTrustedBatteryAuthPass = OSSymbol::withCString(kBatteryAuthTrustedPassKey);


enum veridianAuthStatus {
    kVeridianAuthStatusReset = 0,
    kVeridianAuthStatusOk = 0xefcc,
    kVeridianAuthStatusBusy = 0xbbee,
    kVeridianAuthStatusDigestError = 0x0ebd,
    kVeridianAuthStatusStateError = 0x5a5a,
    kVeridianAuthStatusSigningError = 0xaeae,
};

struct batt_auth_cmd {
    OSData *data;
    uint32_t smcKey;
    uint8_t cmd;
};

static struct {
    uint8_t cmd;
    uint16_t len;
} battAuthCmdTbl[] = {
    { .cmd = kAuthCommandIDGetCertSN, .len = 32, },
    { .cmd = kAuthCommandIDGetInfo, .len = 8, },
    { .cmd = kAuthCommandIDGetSignature, .len = 64, },
    { .cmd = kAuthCommandIDGetCert, .len = BATT_AUTH_MAX_CERT_LENGTH, },
    { .cmd = kAuthCommandIDSetTrustStatus, .len = 1, },
};

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

static uint16_t bauthCmd2len(uint8_t cmd)
{
    for (size_t i = 0; i < ARRAY_SIZE(battAuthCmdTbl); i++) {
        if (battAuthCmdTbl[i].cmd == cmd) {
            return battAuthCmdTbl[i].len;
        }
    }

    return 0;
}

bool AppleBatteryAuth::isRoswell(void)
{
    return (getFlags() & kAuthCPChipMask) == kAuthCPChipRoswell;
}

bool AppleBatteryAuth::isVeridian(void)
{
    return (getFlags() & kAuthCPChipMask) == kAuthCPChipVeridian;
}

SMCResult AppleBatteryAuth::_smcReadKey(SMCKey key, IOByteCount length, void *data)
{
    SMCResult ret;
    size_t retry = 0;

    do {
        BA_DBG("ReadSMCKey attempt %lu/%u", retry, MAX_TRIES);
        if (retry) {
            IOSleep(700);
        }

        ret = fSMCDriver->smcReadKeyHostEndian(key, length, data);
    } while (ret != kSMCSuccess && retry++ < MAX_TRIES);

    if (ret != kSMCSuccess) {
        BA_ERR("failed to read key '%c%c%c%c' rc:%u=%s\n", SMCKEY2CHARS(key), ret, printSMCResult(ret));
    }

    return ret;
}

SMCResult AppleBatteryAuth::_smcWriteKey(SMCKey key, IOByteCount length, void *data)
{
    SMCResult ret;
    size_t retry = 0;

    do {
        BA_DBG("WriteSMCKey attempt %lu/%u", retry, MAX_TRIES);

        if (retry) {
            BA_ERR("failed to write key '%c%c%c%c', retry:%zu\n", SMCKEY2CHARS(key), retry);
            IOSleep(700);
        }

        ret = fSMCDriver->smcWriteKey(key, length, data);
    } while (ret != kSMCSuccess && retry++ < MAX_TRIES);

    if (ret != kSMCSuccess) {
        BA_ERR("failed to write key '%c%c%c%c' rc:%u=%s\n", SMCKEY2CHARS(key), ret, printSMCResult(ret));
    }

    return ret;
}

IOReturn AppleBatteryAuth::_smcAuthNotifierHandlerThread(OSObject *param)
{
    if (!fWorkLoop->inGate()) {
        return _sbCommandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this,
                                         &AppleBatteryAuth::_smcAuthNotifierHandlerThread),
                                         param);
    }

    static bool commErrorLogged = false;
    static bool procErrorLogged = false;

    OSArray *objArray = OSDynamicCast(OSArray, param);
    if (!objArray) {
        BA_ERR("invalid notification data: expected OSArray\n");
    }

    OSNumber *osNum = OSDynamicCast(OSNumber, objArray->getObject(0));
    if (!osNum) {
        BA_ERR("invalid notification data: expected OSNumber\n");
    }

    _receivedNotificationStatus.op = osNum->unsigned8BitValue();

    osNum = OSDynamicCast(OSNumber, objArray->getObject(1));
    if (!osNum) {
        BA_ERR("invalid notification data: expected OSNumber\n");
    }

    uint8_t i2c_status = osNum->unsigned8BitValue();
    _receivedNotificationStatus.i2cStatus = i2c_status;
    if (i2c_status) {
        BA_ERR("error in auth co-processor communication (%x)\n", i2c_status);
        if (!commErrorLogged) {
            setProperty(_kCommError, osNum);
            commErrorLogged = true;
        }
    }

    osNum = OSDynamicCast(OSNumber, objArray->getObject(2));
    if (!osNum) {
        BA_ERR("invalid notification data: expected OSNumber\n");
    }

    uint8_t ic_status = osNum->unsigned8BitValue();
    _receivedNotificationStatus.icStatus = ic_status;
    if (ic_status) {
        BA_ERR("error in auth co-processor (%x)\n", ic_status);
        if (!procErrorLogged) {
            setProperty(_kProcError, osNum);
            commErrorLogged = true;
        }
    }

    _sbCommandGate->commandWakeup(&_battAuthEvent);
    param->release();

    return true;
}

uint16_t AppleBatteryAuth::_readGgResetCount(void)
{
    uint16_t cnt = 0;
    uint16_t cmd = 0x0005;

    // This metric is only useful on D3x, bail if this is not D3x
    if (!isRoswell()) {
        return 0;
    }

    SMCResult ret = _smcWriteKey('GCCM', sizeof(cmd), &cmd);
    if (ret != kSMCSuccess) {
        BA_ERR("Failed to read GG reset count. rc:0x%x=%s\n", ret, printSMCResult(ret));
        goto error;
    }

    ret = _smcReadKey('GCRW', sizeof(cnt), &cnt);
    if (ret != kSMCSuccess) {
        BA_ERR("Failed to read GG reset count. rc:0x%x=%s\n", ret, printSMCResult(ret));
        goto error;
    }

error:
    return cnt;
}

IOReturn AppleBatteryAuth::_smcAuthNotifierHandler(
    const OSSymbol * type,
    OSObject * val,
    uintptr_t refcon)
{
    if (type->isEqualTo(gBattAuthNotifySymbol) != true) {
        BA_LOG("invalid notification\n");
        return true;
    }

    val->retain();

    // dispatch into a different thread to avoid deadlocks when calling back
    // into AppleSMC due to ASBM creating its own workloop
    thread_call_enter1(_battAuthSMCNotifThreadCall, val);

    return true;
}

IOReturn AppleBatteryAuth::_checkOperationStatus(uint8_t op)
{
    if (_receivedNotificationStatus.op != op) {
        BA_ERR("received notification for op:%u, expected op:%u\n",
               _receivedNotificationStatus.op, op);
        return kIOReturnIOError;
    }

    if (_receivedNotificationStatus.i2cStatus) {
        BA_ERR("failure communicating with gas gauge:%u\n",
               _receivedNotificationStatus.i2cStatus);
        return kIOReturnIOError;
    }

    if (_receivedNotificationStatus.icStatus) {
        BA_ERR("auth co-processor error:%u\n",
               _receivedNotificationStatus.icStatus);
        return kIOReturnIOError;
    }

    return kIOReturnSuccess;
}

#define SMC_RESP_TIMEOUT        5
IOReturn AppleBatteryAuth::_getCertificate(struct batt_auth_cmd *cmd, OSData **retdata)
{
    SMCResult ret = kSMCSuccess;
    IOReturn ioret = kIOReturnSuccess;
    size_t retry = 0;
    uint16_t len = bauthCmd2len(cmd->cmd);
    uint8_t buf[BATT_AUTH_SMC_DATA_LEN] __attribute__((__aligned__(64)));

    if (!retdata) {
        return kIOReturnBadArgument;
    }

    bzero(buf, sizeof(buf));

    // get certificate length
    uint16_t clen;
    ioret = _getInfo(kBatteryAuthOpGetCertLength, &clen);
    if (ioret != kIOReturnSuccess) {
        return ioret;
    }

    if (clen > len) {
        ioret = kIOReturnInternalError;
        BA_ERR("Invalid certificate length '%u'\n", clen);
        return ioret;
    }

    // read GG reset count
    uint16_t ggRstCntPre = _readGgResetCount();

    uint8_t op = kBatteryAuthOpGetCert0;
    do {
        AbsoluteTime dl;
        uint16_t offset = 0;
        len = clen;
        uint8_t cert[BATT_AUTH_MAX_CERT_LENGTH] __attribute__((__aligned__(64)));

        ioret = kIOReturnSuccess;

        BA_DBG("_getCertificate attempt %lu/%u", retry, MAX_TRIES);
        if (retry) {
            _incrementChannel(op);
            IOSleep(700);
        }

        while (clen && op <= kBatteryAuthOpGetCert9) {
            ret = _smcWriteKey('BATC', sizeof(op), &op);
            if (ret) {
                ioret = kIOReturnIOError;
                BA_ERR("Failed to set op '%u', rc:0x%x=%s, retry:%zu\n", op, ret, printSMCResult(ret), retry);
                goto ErrRetryCert;
            }

            clock_interval_to_deadline(SMC_RESP_TIMEOUT, kSecondScale, &dl);
            if (_sbCommandGate->commandSleep(&_battAuthEvent, dl, THREAD_UNINT) != THREAD_AWAKENED) {
                BA_ERR("timeout: op '%u'\n", op);
                // Assume we missed the notification and move on.
                // ReadKey will fail if something is truly wrong.
            }

            ioret = _checkOperationStatus(op);
            if (ioret != kIOReturnSuccess) {
                goto ErrRetryCert;
            }

            ret = _smcReadKey('BATD', BATT_AUTH_SMC_DATA_LEN, buf);
            if (ret != kSMCSuccess) {
                ioret = kIOReturnIOError;
                BA_ERR("Failed to read auth data for op '%u', rc:0x%x=%s, retry:%zu\n", op, ret, printSMCResult(ret), retry);
                goto ErrRetryCert;
            }

            size_t sz = min(clen, BATT_AUTH_SMC_DATA_LEN);
            memcpy(&cert[offset], buf, sz);

            clen -= sz;
            offset += sz;
            op++;
        }

        *retdata = OSData::withBytes(cert, len);
        if (!*retdata) {
            return kIOReturnNoMemory;
        }

ErrRetryCert:   ;
    } while ((ret != kSMCSuccess || ioret != kIOReturnSuccess) && retry++ < MAX_TRIES);

    if (retry) {
        // read GG reset count
        uint16_t ggRstCntPost = _readGgResetCount();
        unsigned int diff = ggRstCntPost - ggRstCntPre;
        if (diff) {
            _reporter->incrementValue(kGGResetCountID, diff);
        }
    }

    return ioret;
}

static OSDictionary *parseAuthInfoRoswell(uint8_t *buf, IOService *provider)
{
    OSDictionary *dict = OSDictionary::withCapacity(6);
    if (!dict) {
        return NULL;
    }

    OSData *idsn = OSData::withBytes(&buf[8], 6);
    if (!idsn) {
        OSSafeReleaseNULL(dict);
        return NULL;
    }

    SET_INTEGER_IN_DICT(dict, kAppleAuthCPDeviceVersionKey, buf[0], 8);
    SET_INTEGER_IN_DICT(dict, kAppleAuthCPFirmwareVersionKey, buf[1], 8);
    SET_INTEGER_IN_DICT(dict, kAppleAuthCPAuthMajorVersionKey, buf[2], 8);
    SET_INTEGER_IN_DICT(dict, kAppleAuthCPAuthMinorVersionKey, buf[3], 8);
    SET_INTEGER_IN_DICT(dict, kAppleAuthCPDeviceIDKey, OSReadBigInt32(buf, 4), 32);
    dict->setObject(kAppleAuthCPDeviceIDSN, idsn);

    OSSafeReleaseNULL(idsn);

    return dict;
}

static OSDictionary *parseAuthInfoVeridian(uint8_t *buf)
{
    OSDictionary *dict = OSDictionary::withCapacity(1);
    if (!dict) {
        return NULL;
    }

    OSData *idsn = OSData::withBytes(buf, 4);
    if (!idsn) {
        OSSafeReleaseNULL(dict);
        return NULL;
    }

    dict->setObject(kAppleAuthCPDeviceIDSN, idsn);

    OSSafeReleaseNULL(idsn);

    return dict;
}

IOReturn AppleBatteryAuth::_veridianPollAuthStatus(unsigned int timeout_sec)
{
    OSData *status = nullptr;
    unsigned int iterations = DIV_ROUND_UP(timeout_sec, POLL_DELAY_SECS);

    for (size_t i = 0; i < iterations; i++) {
        IOSleep(POLL_DELAY_SECS * 1000);

        IOReturn ioret = _getInfo(kBatteryAuthOpGetStatus, &status);
        if (ioret != kIOReturnSuccess) {
            return ioret;
        }

        uint16_t st = *(uint16_t *)status->getBytesNoCopy();
        OSSafeReleaseNULL(status);
        if (st == kVeridianAuthStatusOk) {
            return kIOReturnSuccess;
        }
    }

    return kIOReturnTimeout;
}

IOReturn AppleBatteryAuth::_getInfo(uint8_t op, void *retdata)
{
    SMCResult ret;
    IOReturn ioret = kIOReturnSuccess;
    size_t retry = 0;
    uint8_t buf[BATT_AUTH_SMC_DATA_LEN]  __attribute__((__aligned__(64)));
    bzero(buf, sizeof(buf));

    if (!retdata) {
        return kIOReturnBadArgument;
    }

    // read GG reset count
    uint16_t ggRstCntPre = _readGgResetCount();

    do {
        AbsoluteTime dl;
        BA_DBG("_getInfo attempt %lu/%u", retry, MAX_TRIES);
        ioret = kIOReturnSuccess;

        if (retry) {
            _incrementChannel(op);
            IOSleep(700);
        }

        // write input data
        if (op == kBatteryAuthOpSetChallenge ||
            op == kBatteryAuthOpSetAuthStatus) {
            OSData *data = (OSData *)retdata;
            memcpy(buf, data->getBytesNoCopy(), min(sizeof(buf), data->getLength()));

            ret = _smcWriteKey('BATD', BATT_AUTH_SMC_DATA_LEN, buf);
            if (ret != kSMCSuccess) {
                ioret = kIOReturnIOError;
                BA_ERR("Failed to write auth data for op '%u', rc:0x%x=%s, retry:%zu\n", op, ret, printSMCResult(ret), retry);
                goto ErrRetry;
            }
        }

        ret = _smcWriteKey('BATC', sizeof(op), &op);
        if (ret) {
            ioret = kIOReturnIOError;
            BA_ERR("Failed to set op '%u', rc:0x%x=%s, retry:%zu\n", op, ret, printSMCResult(ret), retry);
            goto ErrRetry;
        }

        clock_interval_to_deadline(SMC_RESP_TIMEOUT, kSecondScale, &dl);
        if (_sbCommandGate->commandSleep(&_battAuthEvent, dl, THREAD_UNINT) != THREAD_AWAKENED) {
            BA_ERR("timeout: op '%u'\n", op);
            // Assume we missed the notification and move on.
            // ReadKey will fail if something is truly wrong.
        }

        ioret = _checkOperationStatus(op);
        if (ioret != kIOReturnSuccess) {
            goto ErrRetry;
        }

        if (op == kBatteryAuthOpSetChallenge) {
            IOSleep(550);
        }

        // get output data
        if (op != kBatteryAuthOpSetChallenge &&
            op != kBatteryAuthOpSetAuthStatus) {
            ret = _smcReadKey('BATD', BATT_AUTH_SMC_DATA_LEN, buf);
            if (ret != kSMCSuccess) {
                ioret = kIOReturnIOError;
                BA_ERR("Failed to read auth data for op '%u', rc:0x%x=%s, retry:%zu\n", op, ret, printSMCResult(ret), retry);
                goto ErrRetry;
            }
        }
ErrRetry:   ;
    } while ((ret != kSMCSuccess || ioret != kIOReturnSuccess) && retry++ < MAX_TRIES);

    if (retry) {
        // read GG reset count
        uint16_t ggRstCntPost = _readGgResetCount();
        unsigned int diff = ggRstCntPost - ggRstCntPre;
        if (diff) {
            _reporter->incrementValue(kGGResetCountID, diff);
        }
    }

    if (ret != kSMCSuccess || ioret != kIOReturnSuccess) {
        return ioret;
    }

    switch (op) {
    case kBatteryAuthOpGetInfo: {
        OSDictionary *dict = nullptr;

        if (isRoswell()) {
            dict = parseAuthInfoRoswell(buf, _provider);
        } else if (isVeridian()) {
            dict = parseAuthInfoVeridian(buf);
        }

        if (!dict) {
            return kIOReturnNoMemory;
        }

        *(OSObject **)retdata = dict;

        break;
    }

    case kBatteryAuthOpGetCertSerial: {
        OSData *data = OSData::withBytes(buf, BATT_AUTH_CERTSERIAL_LEN);
        if (!data) {
            return kIOReturnNoMemory;
        }

        *(OSObject **)retdata = data;

        break;
    }

    case kBatteryAuthOpGetCertLength:
        *(uint16_t *)retdata = *(uint16_t *)buf;
        break;

    case kBatteryAuthOpGetNonce: {
        OSData *data = OSData::withBytes(buf, BATT_AUTH_CHALLENGE_LEN);
        if (!data) {
            return kIOReturnNoMemory;
        }

        *(OSObject **)retdata = data;

        break;
    }

    case kBatteryAuthOpGetSignature: {
        OSData *data = OSData::withBytes(buf, BATT_AUTH_SIGNATURE_LEN);
        if (!data) {
            return kIOReturnNoMemory;
        }

        *(OSObject **)retdata = data;

        break;
    }

    case kBatteryAuthOpGetStatus: {
        OSData *data = OSData::withBytes(buf, BATT_AUTH_STATUS_LEN);
        if (!data) {
            return kIOReturnNoMemory;
        }

        *(OSObject **)retdata = data;

        break;
    }

    default:
        break;
    }

    return kIOReturnSuccess;
}

void AppleBatteryAuth::_copyDeviceNonceTrustedData(const OSData *nonce)
{
    if (trustedDataEnabled == kOSBooleanFalse) {
        BA_LOG("Trusted data path isnt enabled, nothing to do here");
        return;
    }

    BA_LOG("Trusted data path is enabled");
    /* Copy to internal dictionary, do not publish yet */
    trustedBatteryHealthData->setObject(_kTrustedBatteryDataNonce, nonce);
}

/* Get Current time since boot in seconds */
static OSNumber* get_current_epoch_time_seconds(void)
{
    clock_sec_t secs;
    clock_usec_t microsecs;

    clock_get_calendar_microtime(&secs, &microsecs);
    
    /* We dont care about usec resolution */
    return OSNumber::withNumber(secs, 64);
}

void AppleBatteryAuth::_checkTrustValueAndPublishNonce(const OSData *authStatus)
{
    /* If this feature isnt enabled, do nothing */
    if (trustedDataEnabled == kOSBooleanFalse) {
        return;
    }

    /* Extract the trusted byte and raw nonce */
    bool authPassed = *(bool *)authStatus->getBytesNoCopy();
    OSData *nonce = OSDynamicCast(OSData, trustedBatteryHealthData->getObject(_kTrustedBatteryDataNonce));

    if (authPassed && nonce) {
        BA_LOG("Auth Passed. Publishing trusted data nonce\n");
        setProperty(_kTrustedBatteryAuthPass, kOSBooleanTrue);
        setProperty(_kTrustedBatteryDataNonce, nonce);
        setProperty(_kTrustedBatteryLastTS, get_current_epoch_time_seconds());
    } else {
        BA_ERR("Auth Failed.\n");
        setProperty(_kTrustedBatteryAuthPass, kOSBooleanFalse);
    }
    
    BA_LOG("Notify Clients about change in data\n");
    /* Notify clients that have subscribed to us telling them something has changed */
    messageClients(kIOMessageServicePropertyChange);

    /* Delete the nonce from local dictionary, so we know we are not operating on stale values */
    trustedBatteryHealthData->removeObject(_kTrustedBatteryDataNonce);
}

void AppleBatteryAuth::_battAuthThreadGated(struct batt_auth_cmd *cmd)
{
    IOReturn ioret = kIOReturnSuccess;
    OSObject *retdata = NULL;

    BA_DBG("Received Command: %d", cmd->cmd);

    switch (cmd->cmd) {
    case kAuthCommandIDGetInfo: {
        OSDictionary *dict = nullptr;
        BA_DBG("Send kBatteryAuthOpGetInfo Command to SMC");
        ioret = _getInfo(kBatteryAuthOpGetInfo, &dict);
        BA_DBG("kBatteryAuthOpGetInfo resp: %d", ioret);
        if (ioret != kIOReturnSuccess) {
            goto ErrDone;
        }

        // stuff battery serial into info dict to allow indetifying E300
        OSObject *serial = _provider->copyProperty(kIOPMPSSerialKey);
        if (serial) {
            dict->setObject(kIOPMPSSerialKey, serial);
            OSSafeReleaseNULL(serial);
        }

        retdata = dict;

        break;
    }

    case kAuthCommandIDGetCertSN: {
        OSData *data;
        BA_DBG("Send kBatteryAuthOpGetCertSerial Command to SMC");
        ioret = _getInfo(kBatteryAuthOpGetCertSerial, &data);
        BA_DBG("kBatteryAuthOpGetCertSerial resp: %d", ioret);
        if (ioret != kIOReturnSuccess) {
            goto ErrDone;
        }

        retdata = data;

        break;
    }

    case kAuthCommandIDGetCert: {
        OSData *data;
        BA_DBG("Send kAuthCommandIDGetCert Command to SMC");
        ioret = _getCertificate(cmd, &data);
        BA_DBG("kAuthCommandIDGetCert resp: %d", ioret);
        if (ioret != kIOReturnSuccess) {
            goto ErrDone;
        }

        retdata = data;

        break;
    }

    case kAuthCommandIDGetSignature: {
        BA_DBG("Send kBatteryAuthOpSetChallenge Command to SMC");
        ioret = _getInfo(kBatteryAuthOpSetChallenge, cmd->data);
        BA_DBG("kBatteryAuthOpSetChallenge resp: %d", ioret);
        OSSafeReleaseNULL(cmd->data);
        if (ioret != kIOReturnSuccess) {
            goto ErrDone;
        }

        OSData *nonce = nullptr;
        if (isVeridian()) {
            BA_DBG("PollAuthStatus started");
            ioret = _veridianPollAuthStatus(20);
            BA_DBG("PollAuthStatus resp: %d", ioret);
            if (ioret != kIOReturnSuccess) {
                goto ErrDone;
            }

            BA_DBG("Send kBatteryAuthOpGetNonce Command to SMC");
            ioret = _getInfo(kBatteryAuthOpGetNonce, &nonce);
            BA_DBG("kBatteryAuthOpGetNonce resp: %d", ioret);
            if (ioret != kIOReturnSuccess) {
                goto ErrDone;
            }
        }

        OSData *signature;
        BA_DBG("Send kBatteryAuthOpGetSignature Command to SMC");
        ioret = _getInfo(kBatteryAuthOpGetSignature, &signature);
        BA_DBG("kBatteryAuthOpGetSignature resp: %d", ioret);
        if (ioret != kIOReturnSuccess) {
            goto ErrDone;
        }

        OSDictionary *dict = OSDictionary::withCapacity(2);
        if (!dict) {
            ioret = kIOReturnNoMemory;
            goto ErrDone;
        }

        dict->setObject(kAppleAuthCPSignature, signature);
        OSSafeReleaseNULL(signature);

        if (nonce) {
            _copyDeviceNonceTrustedData(nonce);
            dict->setObject(kAppleAuthCPDeviceNonce, nonce);

            /* if boot arg said force nonce irrespective of Auth status, do it */
            if (bootArg & kAppleBatteryAuthBootArgForceAuth) {
                uint32_t forcePass = 1;
                const OSData* authFakePass = OSData::withBytes(&forcePass, sizeof(forcePass));
                if (authFakePass) {
                    BA_LOG("Force publishing of data due to boot arg\n");
                    _checkTrustValueAndPublishNonce(authFakePass);
                }
                OSSafeReleaseNULL(authFakePass);
            }

            OSSafeReleaseNULL(nonce);
        }

        retdata = dict;

        break;
    }

    case kAuthCommandIDSetTrustStatus: {
        if (!(bootArg & kAppleBatteryAuthBootArgForceAuth)) {
            /* only publish if we know that we havent already at nonce stage */
            _checkTrustValueAndPublishNonce(cmd->data);
        }

        BA_DBG("Send kBatteryAuthOpSetAuthStatus Command to SMC");
        ioret = _getInfo(kBatteryAuthOpSetAuthStatus, cmd->data);
        BA_DBG("kBatteryAuthOpSetAuthStatus resp: %d", ioret);
        OSSafeReleaseNULL(cmd->data);
        if (ioret != kIOReturnSuccess) {
            goto ErrDone;
        }
        break;
    }

    default:
        ioret = kIOReturnBadArgument;
        break;
    }

ErrDone:
    BA_DBG("Command: %d, RetCode: %d", cmd->cmd, ioret);
    // call receive callback
    _battAuthReceiveAction(_battAuthOwner, this, ioret, cmd->cmd, retdata);

    OSSafeReleaseNULL(retdata);
    IOFreeType(cmd, struct batt_auth_cmd);

    _interfacePut();
}

void AppleBatteryAuth::_battAuthThread(void * param1)
{
    struct batt_auth_cmd *cmd = static_cast<struct batt_auth_cmd *>(param1);

    _sbCommandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this,
                &AppleBatteryAuth::_battAuthThreadGated), cmd);
}

bool AppleBatteryAuth::_interfaceTryGet(void)
{
    bool ret;

    if (!IOLockTryLock(_submitLock)) {
        return false;
    }

    if (_battAuthBusy) {
        ret = false;
        goto unlock;
    }

    _battAuthBusy = true;
    ret = true;

unlock:
    IOLockUnlock(_submitLock);

    return ret;
}

void AppleBatteryAuth::_interfacePut(void)
{
    assert(_battAuthBusy);
    _battAuthBusy = false;
}

IOReturn AppleBatteryAuth::authSendData(uint8_t cmdId, OSData *data)
{
    if (!_battAuthOwner) {
        return kIOReturnError;
    }

    switch (cmdId) {
    case kAuthCommandIDSetTrustStatus:
        if (!data) {
            return kIOReturnBadArgument;
        }
        if (data->getLength() != bauthCmd2len(cmdId)) {
            return kIOReturnBadArgument;
        }
        data->retain();
        break;

    case kAuthCommandIDGetSignature:
        if (!data) {
            return kIOReturnBadArgument;
        }
        if (data->getLength() != BATT_AUTH_CHALLENGE_LEN) {
            return kIOReturnBadArgument;
        }
        data->retain();
        break;

    case kAuthCommandIDGetInfo:
    case kAuthCommandIDGetCert:
    case kAuthCommandIDGetCertSN:
        break;

    default:
        return kIOReturnError;
        break;
    }

    if (!_interfaceTryGet()) {
        return kIOReturnBusy;
    }

    struct batt_auth_cmd *cmd = static_cast<struct batt_auth_cmd *>(IOMallocType(struct batt_auth_cmd));
    cmd->cmd = cmdId;
    cmd->data = data;

    thread_call_enter1(_battAuthThreadCall, cmd);

    return kIOReturnSuccess;
}

IOReturn AppleBatteryAuth::authInit(OSObject *owner, AuthReceiveAction ra)
{
    if (!owner || !ra) {
        return kIOReturnBadArgument;
    }

    _sbCommandGate = IOCommandGate::commandGate(this);
    if (!_sbCommandGate) {
        return kIOReturnError;
    }

    fWorkLoop->addEventSource(_sbCommandGate);

    _battAuthOwner = owner;
    _battAuthReceiveAction = ra;

    _battAuthThreadCall = thread_call_allocate_with_options(
        OSMemberFunctionCast(thread_call_func_t,
            this, &AppleBatteryAuth::_battAuthThread), this,
            THREAD_CALL_PRIORITY_USER,
            THREAD_CALL_OPTIONS_ONCE);
    if (!_battAuthThreadCall) {
        return kIOReturnInternalError;
    }

    _battAuthSMCNotifThreadCall = thread_call_allocate_with_options(
        OSMemberFunctionCast(thread_call_func_t,
            this, &AppleBatteryAuth::_smcAuthNotifierHandlerThread), this,
            THREAD_CALL_PRIORITY_USER,
            THREAD_CALL_OPTIONS_ONCE);
    if (!_battAuthSMCNotifThreadCall) {
        return kIOReturnInternalError;
    }

    OSObject *handle;
    SMCResult rc = fSMCDriver->smcRegisterNotificationController(
                /* type    */  gBattAuthNotifySymbol,
                /* handler */  OSMemberFunctionCast(
                    SMCNotificationCallback, this,
                    &AppleBatteryAuth::_smcAuthNotifierHandler),
                /* target  */  this,
                /* refcon  */  0 ,
                /* handle */   &handle);
    if (rc != kSMCSuccess) {
        BA_ERR("failed to register notifier %d=%s\n", rc, printSMCResult(rc));
        return kIOReturnInternalError;
    }

    trustedBatteryHealthData = OSDictionary::withCapacity(1);
    setProperty(_kTrustedBatteryDataEn, trustedDataEnabled);

    return kIOReturnSuccess;
}

IOReturn AppleBatteryAuth::_createReporters(void)
{
    _reporterSet = OSSet::withCapacity(kIOReportNumberOfReporters);
    if(!_reporterSet) {
        return kIOReturnNoResources;
    }

    _reporter = IOSimpleReporter::with(this, kReportCategoryBatteryAuth, kIOReportUnitNone);
    if (!_reporter) {
        _reporterSet->release();
        _reporterSet = NULL;
        return kIOReturnNoResources;
    }

    // add reporter channels
    _reporter->addChannel(kAuthCommandIDGetCertSNID, APPLE_BATTERYAUTH_RETRY_CERTSN_KEY);
    _reporter->addChannel(kAuthCommandIDGetInfoID, APPLE_BATTERYAUTH_RETRY_INFO_KEY);
    _reporter->addChannel(kAuthCommandIDGetSignatureID, APPLE_BATTERYAUTH_RETRY_SIGNATURE_KEY);
    _reporter->addChannel(kAuthCommandIDGetCertID, APPLE_BATTERYAUTH_RETRY_CERT_KEY);
    _reporter->addChannel(kAuthCommandIDSetTrustStatusID, APPLE_BATTERYAUTH_RETRY_TRUST_KEY);
    _reporter->addChannel(kGGResetCountID, APPLE_BATTERYAUTH_RETRY_GGRESET_KEY);

    IOReportLegend::addReporterLegend(this, _reporter, kIOReportBatteryAuthGroupName, NULL);
    _reporterSet->setObject(_reporter);

    // set initial values
    _updateChannelValue(_reporter, kAuthCommandIDGetCertSNID, (int64_t)0);
    _updateChannelValue(_reporter, kAuthCommandIDGetInfoID, (int64_t)0);
    _updateChannelValue(_reporter, kAuthCommandIDGetSignatureID, (int64_t)0);
    _updateChannelValue(_reporter, kAuthCommandIDGetCertID, (int64_t)0);
    _updateChannelValue(_reporter, kAuthCommandIDSetTrustStatusID, (int64_t)0);
    _updateChannelValue(_reporter, kGGResetCountID, (int64_t)0);

    return kIOReturnSuccess;
}

IOReturn AppleBatteryAuth::_destroyReporters(void)
{
    OSSafeReleaseNULL(_reporter);
    OSSafeReleaseNULL(_reporterSet);

    _reporter = NULL;
    _reporterSet = NULL;

    return kIOReturnSuccess;
}

IOReturn AppleBatteryAuth::_updateChannelValue(IOSimpleReporter *reporter, uint64_t channel, OSObject *obj)
{
    OSNumber *num = OSDynamicCast(OSNumber, obj);

    if (!num) {
        return kIOReturnBadArgument;
    }

    return _updateChannelValue(reporter, channel, (int)num->unsigned64BitValue());
}

IOReturn AppleBatteryAuth::_updateChannelValue(IOSimpleReporter *reporter, uint64_t channel, int64_t value)
{
    if (!reporter) {
        return kIOReturnBadArgument;
    }

    return reporter->setValue(channel, value);
}

static uint64_t bauthOp2channelId(uint8_t op)
{
    switch (op) {
    case kBatteryAuthOpGetCertSerial:
        return kAuthCommandIDGetCertSNID;
    case kBatteryAuthOpGetInfo:
        return kAuthCommandIDGetInfoID;
    case kBatteryAuthOpSetChallenge:
    case kBatteryAuthOpGetNonce:
    case kBatteryAuthOpGetSignature:
        return kAuthCommandIDGetSignatureID;
    case kBatteryAuthOpGetCert0 ... kBatteryAuthOpGetCert9:
        return kAuthCommandIDGetCertID;
    case kBatteryAuthOpSetAuthStatus:
        return kAuthCommandIDSetTrustStatusID;
    default:
        return 0;
    }
}

IOReturn AppleBatteryAuth::_incrementChannel(uint8_t op)
{
    uint64_t channelId = bauthOp2channelId(op);
    if (!channelId) {
        return kIOReturnBadArgument;
    }

    if (!_reporter) {
        return kIOReturnInternalError;
    }

    return _reporter->incrementValue(channelId, 1);
}

IOReturn AppleBatteryAuth::configureReport(IOReportChannelList *channels, IOReportConfigureAction action,
                                           void *result, void *destination)
{
    if (_reporterSet) {
        return IOReporter::configureAllReports(_reporterSet, channels, action, result, destination);
    }

    return super::configureReport(channels, action, result, destination);
}

IOReturn AppleBatteryAuth::updateReport(IOReportChannelList *channels, IOReportUpdateAction action,
                                void *result, void *destination)
{
    if (_reporterSet) {
        return IOReporter::updateAllReports(_reporterSet, channels, action, result, destination);
    }

    return super::updateReport(channels, action, result, destination);;
}

IOWorkLoop *AppleBatteryAuth::getWorkLoop() const
{
    return fWorkLoop;
}

uint32_t AppleBatteryAuth::_getBootArg(void)
{
    uint32_t barg = kAppleBatteryAuthBootArgDefault;
#if ASBM_DEVELOPMENT
    PE_parse_boot_argn("hagrid", &barg, sizeof(barg));
#endif // ASBM_DEVELOPMENT

    return barg;
}

#define kGGFWID_HERCULES        0x610
#define kGGFWID_HERCULES_2      0x611
bool AppleBatteryAuth::start(IOService *provider)
{
    IOReturn ret;

    _provider = OSDynamicCast(AppleSmartBattery, provider);
    if (!_provider) {
        return false;
    }

    ret = _createReporters();
    if (ret) {
        return ret;
    }

    fWorkLoop = IOWorkLoop::workLoop();
    if (!fWorkLoop) {
        goto err;
    }

    if (!super::start(_provider)) {
        goto err;
    }

    _flags |= kAuthCPFlagAccessoryTypeInternalBattery;
    bootArg = _getBootArg();

    registerService();

    return true;

err:
    _destroyReporters();
    OSSafeReleaseNULL(fWorkLoop);

    return false;
}

void AppleBatteryAuth::free(void)
{
    OSSafeReleaseNULL(fWorkLoop);
    _destroyReporters();
    IOLockFree(_submitLock);
    super::free();
}

AppleBatteryAuth *AppleBatteryAuth::withSMCFamily(AppleSMCFamily *smc,
                                                  uint32_t authChip,
                                                  OSBoolean *trustedDataEnabled)
{
    authChip &= kAuthCPChipMask;

    if (!smc || !authChip) {
        return nullptr;
    }

    AppleBatteryAuth *me = new AppleBatteryAuth;
    if (!me) {
        return nullptr;
    }

    me->_submitLock = IOLockAlloc();
    if (!me->_submitLock) {
        delete me;
        return nullptr;
    }

#if TARGET_OS_OSX
    me->fDisplayKeys  = false;
#else
    me->fDisplayKeys = PE_i_can_has_debugger(NULL);
#endif

    me->fSMCDriver = smc;
    me->fWorkLoop = NULL;
    me->_battAuthBusy = false;
    me->_flags = authChip;
    me->trustedDataEnabled = trustedDataEnabled;

    me->init();

    return me;
}

#endif // TARGET_OS_IOS || TARGET_OS_WATCH || (TARGET_OS_OSX && TARGET_CPU_ARM64)
