#include <TargetConditionals.h>

#if TARGET_OS_IOS || TARGET_OS_WATCH || (TARGET_OS_OSX && TARGET_CPU_ARM64)
#pragma once

#include <IOKit/IOService.h>
#include <IOKit/IOReturn.h>
#include <IOKit/smc/AppleSMCFamily.h>
#include <libkern/c++/OSObject.h>
#include <kern/thread_call.h>
#include <battery/gasgauge_update.h>
#include "AppleSmartBattery.h"
#include "AppleGasGaugeUpdateUserClient.h"

class IOWorkLoop;
class IOCommandGate;
class AppleSMCFamily;
class AppleGasGaugeUpdateUserClient;

struct AppleGasGaugeUpdateOpsArgs {
    enum gg_fw_update_op op_length;
    enum gg_fw_update_op op_data;
    enum gg_fw_update_op op_sign1;
    enum gg_fw_update_op op_sign2;
};

struct AppleGasGaugeUpdateNotificationStatus {
    uint8_t op;
    uint8_t i2cStatus;
    uint8_t updaterStatus;
};

class AppleGasGaugeUpdate : public IOService {
OSDeclareDefaultStructors(AppleGasGaugeUpdate)

    friend class AppleGasGaugeUpdateUserClient;

public:
    bool        start(IOService *provider) APPLE_KEXT_OVERRIDE;
    void        free(void) APPLE_KEXT_OVERRIDE;
    IOWorkLoop  *getWorkLoop() const APPLE_KEXT_OVERRIDE;
    IOReturn    newUserClient(task_t owningTask, void *securityID, UInt32 type, IOUserClient **handler) APPLE_KEXT_OVERRIDE;

    static AppleGasGaugeUpdate  *withSMCFamily(AppleSMCFamily *smc);

protected:
    IOReturn _getInfo(struct AppleGasGaugeUpdateUserClientInfo *data);
    IOReturn _startUpdate(struct AppleGasGaugeUpdateUserClientOData *data);
    IOReturn _commitImage(void);
    IOReturn _writeCertificate(const struct AppleGasGaugeUpdateUserClientIData *data);
    IOReturn _writeImg4Manifest(const struct AppleGasGaugeUpdateUserClientIData *data);
    IOReturn _writeDigestDict(const struct AppleGasGaugeUpdateUserClientIData *data);
    IOReturn _writeImage(const struct AppleGasGaugeUpdateUserClientIData *data);
    IOReturn _sendData(const struct AppleGasGaugeUpdateUserClientIData *data);

private:
    AppleSmartBattery     *_provider;
    AppleSMCFamily        *_SMCDriver;
    IOCommandGate         *_commandGate;
    IOWorkLoop            *_workLoop;
    int                   *_ggFwUpdEvent;
    thread_call_t         _ggFwUpdSMCNotifThreadCall;
    bool                  _displayKeys;
    bool                  _ggTwoStageUpdate;
    struct AppleGasGaugeUpdateNotificationStatus _receivedNotificationStatus;

    int      _smcGgFwUpdNotifierHandler(const OSSymbol *type, OSObject *val, uintptr_t refcon);
    IOReturn _smcGgFwUpdNotifierHandlerThread(void *param);
    IOReturn _smcGgFwUpdNotifierHandlerThreadGated(OSObject *param);
    IOReturn _checkOperationStatus(uint8_t op);
    SMCResult _smcReadKey(SMCKey key, IOByteCount length, void *data);
    SMCResult _smcWriteKey(SMCKey key, IOByteCount length, void *data);
    IOReturn _pollUpdaterStatus(uint16_t *out_status, unsigned int timeout_ms=2800, unsigned int retry_ms=700);
    IOReturn _getInfoGated(struct AppleGasGaugeUpdateUserClientInfo *data);
    IOReturn _startUpdateGated(struct AppleGasGaugeUpdateUserClientOData *data);
    IOReturn _commitImageGated(void);
    IOReturn _initUpdateGated(void);
    IOReturn _getBatteryIdGated(uint32_t *battId);
    IOReturn _getChallengeDataGated(uint8_t *data);
    IOReturn _startImageGated(enum AppleGasGaugeUpdateUserClientDataType type);
    IOReturn _writeDataGated(const struct AppleGasGaugeUpdateUserClientIData *data,
                             const struct AppleGasGaugeUpdateOpsArgs *ops);
    IOReturn _writeDataLengthGated(uint16_t length, enum gg_fw_update_op op,
                                   unsigned int typical_ms, unsigned int timeout_ms, unsigned int retry_ms,
                                   unsigned int status);
    IOReturn _writeDataSignatureGated(const uint8_t *signature, enum gg_fw_update_op op_sign1, enum gg_fw_update_op op_sign2);
    IOReturn _writeDataDataGated(const uint8_t *data, size_t data_length, enum gg_fw_update_op op);
    IOReturn _startCryptoGated(uint8_t images);
    void _clearErrorReports(void);
};

#endif // TARGET_OS_IOS || TARGET_OS_WATCH || (TARGET_OS_OSX && TARGET_CPU_ARM64)
