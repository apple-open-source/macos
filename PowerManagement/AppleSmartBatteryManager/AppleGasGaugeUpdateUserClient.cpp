#include <TargetConditionals.h>

#if TARGET_OS_IOS || TARGET_OS_WATCH || (TARGET_OS_OSX && TARGET_CPU_ARM64)
#include <sys/systm.h>
#include <sys/proc.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOKitKeys.h>
#include <kern/task.h>
#include "AppleGasGaugeUpdateUserClientPrivate.h"

#define super IOUserClient2022
OSDefineMetaClassAndStructors(AppleGasGaugeUpdateUserClient, IOUserClient2022)

#define GG_UPD_LOG(fmt, args...) os_log(OS_LOG_DEFAULT, "AppleGasGaugeUpdateUserClient: " fmt, ## args)
#define GG_UPD_ERR(fmt, args...) IOLog("AppleGasGaugeUpdateUserClient ERROR: " fmt, ## args)

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

IOReturn AppleGasGaugeUpdateUserClient::getInfo(struct AppleGasGaugeUpdateUserClientInfo *data)
{
    if (data == nullptr) {
        return kIOReturnBadArgument;
    }

    return _owner->_getInfo(data);
}

static IOReturn extGetInfo(OSObject *target, void *reference, IOExternalMethodArguments *arguments)
{
    AppleGasGaugeUpdateUserClient *self = OSDynamicCast(AppleGasGaugeUpdateUserClient, (OSObject *)reference);
    if (self == nullptr) {
        return kIOReturnBadArgument;
    }

    return OSMemberFunctionCast(IOReturn (*)(AppleGasGaugeUpdateUserClient *, struct AppleGasGaugeUpdateUserClientInfo *), self, &AppleGasGaugeUpdateUserClient::getInfo)(self, (struct AppleGasGaugeUpdateUserClientInfo *)arguments->structureOutput);
}

IOReturn AppleGasGaugeUpdateUserClient::startUpdate(struct AppleGasGaugeUpdateUserClientOData *data)
{
    if (data == nullptr) {
        return kIOReturnBadArgument;
    }

    return _owner->_startUpdate(data);
}

static IOReturn extStartUpdate(OSObject *target, void *reference, IOExternalMethodArguments *arguments)
{
    AppleGasGaugeUpdateUserClient *self = OSDynamicCast(AppleGasGaugeUpdateUserClient, (OSObject *)reference);
    if (self == nullptr) {
        return kIOReturnBadArgument;
    }

    return OSMemberFunctionCast(IOReturn (*)(AppleGasGaugeUpdateUserClient *, struct AppleGasGaugeUpdateUserClientOData *), self, &AppleGasGaugeUpdateUserClient::startUpdate)(self, (struct AppleGasGaugeUpdateUserClientOData *)arguments->structureOutput);
}

IOReturn AppleGasGaugeUpdateUserClient::commitImage(void)
{
    return _owner->_commitImage();
}

static IOReturn extCommitImage(OSObject *target, void *reference, IOExternalMethodArguments *arguments)
{
    AppleGasGaugeUpdateUserClient *self = OSDynamicCast(AppleGasGaugeUpdateUserClient, (OSObject *)reference);
    if (self == nullptr) {
        return kIOReturnBadArgument;
    }

    return OSMemberFunctionCast(IOReturn (*)(AppleGasGaugeUpdateUserClient *), self, &AppleGasGaugeUpdateUserClient::commitImage)(self);
}

IOReturn AppleGasGaugeUpdateUserClient::sendData(const struct AppleGasGaugeUpdateUserClientIData *data)
{
    if (data == nullptr) {
        return kIOReturnBadArgument;
    }

    return _owner->_sendData(data);
}

static bool args_valid(const struct AppleGasGaugeUpdateUserClientIData *data,
                       uint32_t length, IOMemoryDescriptor *md)
{
    if (!length && !md) {
        GG_UPD_ERR("no length or MD\n");
        return false;
    }

    if (!data && !md) {
        GG_UPD_ERR("no data or MD\n");
        return false;
    }

    if (!md && data && (length <= sizeof(*data) || !data->data_length)) {
        GG_UPD_ERR("invalid length in struct input:%u, %llu\n", length, data->data_length);
        return false;
    }

    if (!length && md && (md->getLength() <= sizeof(*data))) {
        GG_UPD_ERR("invalid length in MD input:%llu\n", md->getLength());
        return false;
    }

    return true;
}

static IOReturn extSendData(OSObject *target, void *reference, IOExternalMethodArguments *arguments)
{
    AppleGasGaugeUpdate *owner = OSDynamicCast(AppleGasGaugeUpdate, target);
    if (owner == nullptr) {
        GG_UPD_ERR("no owner\n");
        return kIOReturnBadArgument;
    }

    AppleGasGaugeUpdateUserClient *self = OSDynamicCast(AppleGasGaugeUpdateUserClient, (OSObject *)reference);
    if (self == nullptr) {
        GG_UPD_ERR("no self\n");
        return kIOReturnBadArgument;
    }

    IOReturn ret;
    IOMemoryMap *map = NULL;
    const struct AppleGasGaugeUpdateUserClientIData *struct_input = (const struct AppleGasGaugeUpdateUserClientIData *)arguments->structureInput;
    uint32_t length = arguments->structureInputSize;
    IOMemoryDescriptor *md = arguments->structureInputDescriptor;
    const struct AppleGasGaugeUpdateUserClientIData *data;
    IOByteCount inputLength;

    if (!args_valid(struct_input, length, md)) {
        return kIOReturnBadArgument;
    }

    if (md) {
        map = md->map(kIOMapReadOnly);
        if (!map) {
            GG_UPD_ERR("failed to map memory\n");
            return kIOReturnNoMemory;
        }

        data = (struct AppleGasGaugeUpdateUserClientIData *)map->getVirtualAddress();
        inputLength = md->getLength();
    } else {
        data = struct_input;
        inputLength = length;
    }

    if (!data) {
        ret = kIOReturnNoMemory;
        goto err;
    }

    if (data->type >= kGgFwUpdDataTypeMax) {
        GG_UPD_ERR("invalid type:%llu\n", data->type);
        ret = kIOReturnBadArgument;
        goto err;
    }

    if (data->data_length > (inputLength - sizeof(*data))) {
        GG_UPD_ERR("invalid length:%llu>%llu\n", data->data_length, inputLength - sizeof(*data));
        ret = kIOReturnBadArgument;
        goto err;
    }

    ret = OSMemberFunctionCast(IOReturn (*)(AppleGasGaugeUpdateUserClient *, const struct AppleGasGaugeUpdateUserClientIData *), self, &AppleGasGaugeUpdateUserClient::sendData)(self, data);

err:
    if (md && map) {
        map->release();
    }

    return ret;
}

IOReturn AppleGasGaugeUpdateUserClient::externalMethod(uint32_t selector, IOExternalMethodArgumentsOpaque *arguments)
{
    static const struct IOExternalMethodDispatch2022 dispatch_arr[] = {
        // structure output: chemistry, GG FW version
        [kGgFwUpdInfo] = { extGetInfo, 0, 0, 0, sizeof(AppleGasGaugeUpdateUserClientInfo), false },
        // structure output: batteryID, Nonce
        [kGgFwUpdStartUpdate] = { extStartUpdate, 0, 0, 0, sizeof(AppleGasGaugeUpdateUserClientOData), false },
        [kGgFwUpdSendData] = { extSendData, 0, kIOUCVariableStructureSize, 0, 0, false },
        [kGgFwUpdCommitImage] = { extCommitImage, 0, 0, 0, 0, false },
    };

    return dispatchExternalMethod(selector, arguments, dispatch_arr, ARRAY_SIZE(dispatch_arr), _owner, this);
}

IOReturn AppleGasGaugeUpdateUserClient::clientClose(void)
{
    detach(_owner);

    // We only have one application client. If the app is closed,
    // we can terminate the user client.
    terminate();

    return kIOReturnSuccess;
}

bool AppleGasGaugeUpdateUserClient::start(IOService *provider)
{
    _owner = OSDynamicCast(AppleGasGaugeUpdate, provider);
    if (!_owner) {
        return false;
    }

    if (!super::start(provider)) {
        return false;
    }

    setProperty(kIOUserClientDefaultLockingKey, kOSBooleanTrue);
    setProperty(kIOUserClientDefaultLockingSetPropertiesKey, kOSBooleanTrue);
    setProperty(kIOUserClientDefaultLockingSingleThreadExternalMethodKey, kOSBooleanTrue);
    setProperty(kIOUserClientEntitlementsKey, "com.apple.private.gasgauge-update");

    return true;
}

bool AppleGasGaugeUpdateUserClient::initWithTask(task_t owningTask, void *security_id,
                                                 UInt32 type)
{
    uint32_t pid;

    if (!super::initWithTask(owningTask, security_id, type)) {
        return false;
    }

    pid = proc_selfpid();
    setProperty("pid", pid, 32);

    return true;
}

#endif // TARGET_OS_IOS || TARGET_OS_WATCH || (TARGET_OS_OSX && TARGET_CPU_ARM64)
