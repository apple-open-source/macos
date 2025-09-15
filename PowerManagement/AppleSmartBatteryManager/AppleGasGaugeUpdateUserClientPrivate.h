#include <TargetConditionals.h>

#if TARGET_OS_IOS || TARGET_OS_WATCH || (TARGET_OS_OSX && TARGET_CPU_ARM64)
#pragma once

#include <IOKit/IOUserClient.h>
#include "AppleGasGaugeUpdateUserClient.h"
#include "AppleGasGaugeUpdate.h"

class AppleGasGaugeUpdateUserClient : public IOUserClient2022
{
    OSDeclareDefaultStructors(AppleGasGaugeUpdateUserClient)

private:
    AppleGasGaugeUpdate    *_owner;

public:
    bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
    bool initWithTask(task_t owningTask, void *security_id, UInt32 type) APPLE_KEXT_OVERRIDE;
    IOReturn clientClose(void) APPLE_KEXT_OVERRIDE;
    IOReturn externalMethod(uint32_t selector, IOExternalMethodArgumentsOpaque *arguments) APPLE_KEXT_OVERRIDE;
    IOReturn getInfo(struct AppleGasGaugeUpdateUserClientInfo *data);
    IOReturn startUpdate(struct AppleGasGaugeUpdateUserClientOData *data);
    IOReturn commitImage(void);
    IOReturn sendData(const struct AppleGasGaugeUpdateUserClientIData *data);
};

#endif // TARGET_OS_IOS || TARGET_OS_WATCH || (TARGET_OS_OSX && TARGET_CPU_ARM64)
