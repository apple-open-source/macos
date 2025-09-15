#include <TargetConditionals.h>

#if TARGET_OS_IOS || TARGET_OS_WATCH || (TARGET_OS_OSX && TARGET_CPU_ARM64)
#pragma once

#include <IOKit/IOReturn.h>
#include <IOKit/IOReporter.h>
#include <IOKit/accessory/AppleAuthCPRelayInterface.h>
#include <IOKit/smc/AppleSMCFamily.h>
#include <libkern/c++/OSObject.h>
#include <kern/thread_call.h>
#include <battery/battery_authentication.h>

class IOWorkLoop;
class IOCommandGate;
class AppleSMCFamily;

struct AppleBatteryAuthNotificationStatus {
    uint8_t op;
    uint8_t i2cStatus;
    uint8_t icStatus;
};

class AppleSmartBattery;

class AppleBatteryAuth : public AppleAuthCPRelayInterface {
OSDeclareDefaultStructors(AppleBatteryAuth)

public:
    IOReturn                 authInit(OSObject *owner, AuthReceiveAction ra) APPLE_KEXT_OVERRIDE;
    IOReturn                 authSendData(uint8_t cmd, OSData *data) APPLE_KEXT_OVERRIDE;
    bool                     start(IOService *provider) APPLE_KEXT_OVERRIDE;
    void                     free(void) APPLE_KEXT_OVERRIDE;
    IOWorkLoop               *getWorkLoop() const APPLE_KEXT_OVERRIDE;

    static AppleBatteryAuth  *withSMCFamily(AppleSMCFamily *smc,
                                            uint32_t authChip,
                                            OSBoolean *trustedDataEnabled);

    bool isRoswell(void);
    bool isVeridian(void);

private:
    AppleSmartBattery     *_provider;
    AppleSMCFamily        *fSMCDriver;
    IOCommandGate         *_sbCommandGate;
    int                   *_battAuthEvent;
    IOLock                *_submitLock;
    AuthReceiveAction     _battAuthReceiveAction;
    thread_call_t         _battAuthThreadCall;
    thread_call_t         _battAuthSMCNotifThreadCall;
    OSObject*             _battAuthOwner;
    bool                  _battAuthBusy;
    OSSet                 *_reporterSet;
    IOSimpleReporter      *_reporter;
    struct AppleBatteryAuthNotificationStatus _receivedNotificationStatus;

    SMCResult _smcReadKey(SMCKey key, IOByteCount length, void *data);
    SMCResult _smcWriteKey(SMCKey key, IOByteCount length, void *data);
    int       _smcAuthNotifierHandler(const OSSymbol * type, OSObject * val, uintptr_t refcon);
    IOReturn _smcAuthNotifierHandlerThread(OSObject *param);
    void    _battAuthThread(void * param1);
    void    _battAuthThreadGated(struct batt_auth_cmd *cmd);
    bool    _interfaceTryGet(void);
    void    _interfacePut(void);
    IOReturn _createReporters(void);
    IOReturn _destroyReporters(void);
    IOReturn _checkOperationStatus(uint8_t op);

    IOReturn _getCertificate(struct batt_auth_cmd *cmd, OSData **retdata);
    IOReturn _getInfo(uint8_t op, void *retdata);
    IOReturn _veridianPollAuthStatus(unsigned int timeout_sec);
    IOReturn _updateChannelValue(IOSimpleReporter *reporter, uint64_t channel, OSObject *obj);
    IOReturn _updateChannelValue(IOSimpleReporter *reporter, uint64_t channel, int64_t value);
    IOReturn _incrementChannel(uint8_t op);
    IOReturn configureReport(IOReportChannelList *channels, IOReportConfigureAction action,
                             void *result, void *destination) APPLE_KEXT_OVERRIDE;
    IOReturn updateReport(IOReportChannelList *channels, IOReportUpdateAction action,
                          void *result, void *destination) APPLE_KEXT_OVERRIDE;

    uint16_t _readGgResetCount(void);
    void _checkTrustValueAndPublishNonce(const OSData *authStatus);
    void _copyDeviceNonceTrustedData(const OSData *nonce);
    uint32_t _getBootArg(void);

protected:
    bool                        fDisplayKeys;
    uint32_t                    bootArg;
    IOWorkLoop                  *fWorkLoop;
    OSDictionary                *trustedBatteryHealthData;
    OSBoolean                   *trustedDataEnabled;
};

#endif /* TARGET_OS_IOS || TARGET_OS_WATCH || (TARGET_OS_OSX && TARGET_CPU_ARM64)*/
