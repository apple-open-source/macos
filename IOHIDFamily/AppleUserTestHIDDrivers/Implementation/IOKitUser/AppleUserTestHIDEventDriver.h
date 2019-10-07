/* iig generated from AppleUserTestHIDEventDriver.h */

#undef	IIG_IMPLEMENTATION
#define	IIG_IMPLEMENTATION 	AppleUserTestHIDEventDriver.h

#include <IOKit/IOReturn.h>
#include <IOKitUser/AppleUserTestHIDEventDriver.h>



typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOService_Start_Msg * message;
        struct IOService_Start_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
AppleUserTestHIDEventDriver_Start_Invocation;

#if !KERNEL
extern OSMetaClass * gIODispatchQueueMetaClass;
extern OSMetaClass * gOSDictionaryMetaClass;
extern OSMetaClass * gIOBufferMemoryDescriptorMetaClass;
extern OSMetaClass * gOSArrayMetaClass;
extern OSMetaClass * gIOMemoryDescriptorMetaClass;
extern OSMetaClass * gIOHIDElementMetaClass;
extern OSMetaClass * gIOHIDEventMetaClass;
extern OSMetaClass * gIOHIDDigitizerCollectionMetaClass;
#endif /* !KERNEL */

#if !KERNEL

#define AppleUserTestHIDEventDriver_QueueNames  ""

#define AppleUserTestHIDEventDriver_MethodNames  ""

#define AppleUserTestHIDEventDriverMetaClass_MethodNames  ""

struct OSClassDescription_AppleUserTestHIDEventDriver_t
{
    OSClassDescription base;
    uint64_t           methodOptions[2 * 0];
    uint64_t           metaMethodOptions[2 * 0];
    char               queueNames[sizeof(AppleUserTestHIDEventDriver_QueueNames)];
    char               methodNames[sizeof(AppleUserTestHIDEventDriver_MethodNames)];
    char               metaMethodNames[sizeof(AppleUserTestHIDEventDriverMetaClass_MethodNames)];
};

const struct OSClassDescription_AppleUserTestHIDEventDriver_t
OSClassDescription_AppleUserTestHIDEventDriver =
{
    .base =
    {
        .descriptionSize         = sizeof(OSClassDescription_AppleUserTestHIDEventDriver_t),
        .name                    = "AppleUserTestHIDEventDriver",
        .superName               = "IOUserHIDEventDriver",
        .flags                   = 0*kOSClassCanRemote,
        .methodOptionsSize       = 2 * sizeof(uint64_t) * 0,
        .metaMethodOptionsSize   = 2 * sizeof(uint64_t) * 0,
        .queueNamesSize       = sizeof(AppleUserTestHIDEventDriver_QueueNames),
        .methodNamesSize         = sizeof(AppleUserTestHIDEventDriver_MethodNames),
        .metaMethodNamesSize     = sizeof(AppleUserTestHIDEventDriverMetaClass_MethodNames),
        .methodOptionsOffset     = __builtin_offsetof(struct OSClassDescription_AppleUserTestHIDEventDriver_t, methodOptions),
        .metaMethodOptionsOffset = __builtin_offsetof(struct OSClassDescription_AppleUserTestHIDEventDriver_t, metaMethodOptions),
        .queueNamesOffset     = __builtin_offsetof(struct OSClassDescription_AppleUserTestHIDEventDriver_t, queueNames),
        .methodNamesOffset       = __builtin_offsetof(struct OSClassDescription_AppleUserTestHIDEventDriver_t, methodNames),
        .metaMethodNamesOffset   = __builtin_offsetof(struct OSClassDescription_AppleUserTestHIDEventDriver_t, metaMethodNames),
    },
    .methodOptions =
    {
    },
    .metaMethodOptions =
    {
    },
    .queueNames      = AppleUserTestHIDEventDriver_QueueNames,
    .methodNames     = AppleUserTestHIDEventDriver_MethodNames,
    .metaMethodNames = AppleUserTestHIDEventDriverMetaClass_MethodNames,
};

static kern_return_t
AppleUserTestHIDEventDriver_New(OSMetaClass * instance);

OSClassLoadInformation
AppleUserTestHIDEventDriver_Class = 
{
    .description       = &OSClassDescription_AppleUserTestHIDEventDriver.base,
    .instanceSize      = sizeof(AppleUserTestHIDEventDriver),

    .New               = &AppleUserTestHIDEventDriver_New,
};

const void *
gAppleUserTestHIDEventDriver_Declaration
__attribute__((visibility("hidden"),section("__DATA,__osclassinfo,regular,no_dead_strip")))
    = &AppleUserTestHIDEventDriver_Class;

OSMetaClass * gAppleUserTestHIDEventDriverMetaClass;

static kern_return_t
AppleUserTestHIDEventDriver_New(OSMetaClass * instance)
{
    if (!new(instance) AppleUserTestHIDEventDriverMetaClass) return (kIOReturnNoMemory);
    return (kIOReturnSuccess);
}

kern_return_t
AppleUserTestHIDEventDriverMetaClass::New(OSObject * instance)
{
    if (!new(instance) AppleUserTestHIDEventDriver) return (kIOReturnNoMemory);
    return (kIOReturnSuccess);
}

#endif /* !KERNEL */

kern_return_t
AppleUserTestHIDEventDriver::Dispatch(const IORPC rpc)
{
    return _Dispatch(this, rpc);
}

kern_return_t
AppleUserTestHIDEventDriver::_Dispatch(AppleUserTestHIDEventDriver * self, const IORPC rpc)
{
    kern_return_t ret = kIOReturnUnsupported;
    IORPCMessage * msg = IORPCMessage(rpc.message);

    switch (msg->msgid)
    {
        case IOService_Start_ID:
            ret = self->Start_Invoke(rpc, (kern_return_t (IOService::*)(IOService_Start_Args)) &AppleUserTestHIDEventDriver::Start_Impl);
            break;

        default:
            ret = IOUserHIDEventDriver::_Dispatch(self, rpc);
            break;
    }

    return (ret);
}

#if KERNEL
kern_return_t
AppleUserTestHIDEventDriver::MetaClass::Dispatch(const IORPC rpc)
{
#else /* KERNEL */
kern_return_t
AppleUserTestHIDEventDriverMetaClass::Dispatch(const IORPC rpc)
{
#endif /* !KERNEL */

    kern_return_t ret = kIOReturnUnsupported;
    IORPCMessage * msg = IORPCMessage(rpc.message);

    switch (msg->msgid)
    {

        default:
            ret = OSMetaClassBase::Dispatch(rpc);
            break;
    }

    return (ret);
}



