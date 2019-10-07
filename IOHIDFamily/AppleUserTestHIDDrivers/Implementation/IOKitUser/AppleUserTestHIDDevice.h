/* iig generated from AppleUserTestHIDDevice.h */

#undef	IIG_IMPLEMENTATION
#define	IIG_IMPLEMENTATION 	AppleUserTestHIDDevice.h

#include <IOKit/IOReturn.h>
#include <IOKitUser/AppleUserTestHIDDevice.h>



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
AppleUserTestHIDDevice_Start_Invocation;


typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOService_Stop_Msg * message;
        struct IOService_Stop_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
AppleUserTestHIDDevice_Stop_Invocation;

struct AppleUserTestHIDDevice_HandleReportCallback_Msg_Content
{
    IORPCMessage hdr;
    uint64_t  timestamp;
    OSObjectRef  report;
    IOHIDReportType  type;
    uint32_t  reportID;
    OSObjectRef  action;
};
#pragma pack(4)
struct AppleUserTestHIDDevice_HandleReportCallback_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t objects[3];
    AppleUserTestHIDDevice_HandleReportCallback_Msg_Content content;
};
#pragma pack()
#define AppleUserTestHIDDevice_HandleReportCallback_Msg_ObjRefs ((1ULL << (__builtin_offsetof(AppleUserTestHIDDevice_HandleReportCallback_Msg_Content, hdr.object) / sizeof(OSObjectRef))) | (1ULL << (__builtin_offsetof(AppleUserTestHIDDevice_HandleReportCallback_Msg_Content, report) / sizeof(OSObjectRef))) | (1ULL << (__builtin_offsetof(AppleUserTestHIDDevice_HandleReportCallback_Msg_Content, action) / sizeof(OSObjectRef))))

struct AppleUserTestHIDDevice_HandleReportCallback_Rpl_Content
{
    IORPCMessage hdr;
    kern_return_t _result;
};
#pragma pack(4)
struct AppleUserTestHIDDevice_HandleReportCallback_Rpl
{
    IORPCMessageMach           mach;
    AppleUserTestHIDDevice_HandleReportCallback_Rpl_Content content;
};
#pragma pack()
#define AppleUserTestHIDDevice_HandleReportCallback_Rpl_ObjRefs (0)


typedef union
{
    const IORPC rpc;
    struct
    {
        const struct AppleUserTestHIDDevice_HandleReportCallback_Msg * message;
        struct AppleUserTestHIDDevice_HandleReportCallback_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
AppleUserTestHIDDevice_HandleReportCallback_Invocation;

#if !KERNEL
extern OSMetaClass * gIODispatchQueueMetaClass;
extern OSMetaClass * gOSDataMetaClass;
extern OSMetaClass * gOSDictionaryMetaClass;
extern OSMetaClass * gIOMemoryDescriptorMetaClass;
extern OSMetaClass * gIOBufferMemoryDescriptorMetaClass;
extern OSMetaClass * gOSArrayMetaClass;
#endif /* !KERNEL */

#if !KERNEL

#define AppleUserTestHIDDevice_QueueNames  ""

#define AppleUserTestHIDDevice_MethodNames  ""

#define AppleUserTestHIDDeviceMetaClass_MethodNames  ""

struct OSClassDescription_AppleUserTestHIDDevice_t
{
    OSClassDescription base;
    uint64_t           methodOptions[2 * 0];
    uint64_t           metaMethodOptions[2 * 0];
    char               queueNames[sizeof(AppleUserTestHIDDevice_QueueNames)];
    char               methodNames[sizeof(AppleUserTestHIDDevice_MethodNames)];
    char               metaMethodNames[sizeof(AppleUserTestHIDDeviceMetaClass_MethodNames)];
};

const struct OSClassDescription_AppleUserTestHIDDevice_t
OSClassDescription_AppleUserTestHIDDevice =
{
    .base =
    {
        .descriptionSize         = sizeof(OSClassDescription_AppleUserTestHIDDevice_t),
        .name                    = "AppleUserTestHIDDevice",
        .superName               = "IOUserHIDDevice",
        .flags                   = 0*kOSClassCanRemote,
        .methodOptionsSize       = 2 * sizeof(uint64_t) * 0,
        .metaMethodOptionsSize   = 2 * sizeof(uint64_t) * 0,
        .queueNamesSize       = sizeof(AppleUserTestHIDDevice_QueueNames),
        .methodNamesSize         = sizeof(AppleUserTestHIDDevice_MethodNames),
        .metaMethodNamesSize     = sizeof(AppleUserTestHIDDeviceMetaClass_MethodNames),
        .methodOptionsOffset     = __builtin_offsetof(struct OSClassDescription_AppleUserTestHIDDevice_t, methodOptions),
        .metaMethodOptionsOffset = __builtin_offsetof(struct OSClassDescription_AppleUserTestHIDDevice_t, metaMethodOptions),
        .queueNamesOffset     = __builtin_offsetof(struct OSClassDescription_AppleUserTestHIDDevice_t, queueNames),
        .methodNamesOffset       = __builtin_offsetof(struct OSClassDescription_AppleUserTestHIDDevice_t, methodNames),
        .metaMethodNamesOffset   = __builtin_offsetof(struct OSClassDescription_AppleUserTestHIDDevice_t, metaMethodNames),
    },
    .methodOptions =
    {
    },
    .metaMethodOptions =
    {
    },
    .queueNames      = AppleUserTestHIDDevice_QueueNames,
    .methodNames     = AppleUserTestHIDDevice_MethodNames,
    .metaMethodNames = AppleUserTestHIDDeviceMetaClass_MethodNames,
};

static kern_return_t
AppleUserTestHIDDevice_New(OSMetaClass * instance);

OSClassLoadInformation
AppleUserTestHIDDevice_Class = 
{
    .description       = &OSClassDescription_AppleUserTestHIDDevice.base,
    .instanceSize      = sizeof(AppleUserTestHIDDevice),

    .New               = &AppleUserTestHIDDevice_New,
};

const void *
gAppleUserTestHIDDevice_Declaration
__attribute__((visibility("hidden"),section("__DATA,__osclassinfo,regular,no_dead_strip")))
    = &AppleUserTestHIDDevice_Class;

OSMetaClass * gAppleUserTestHIDDeviceMetaClass;

static kern_return_t
AppleUserTestHIDDevice_New(OSMetaClass * instance)
{
    if (!new(instance) AppleUserTestHIDDeviceMetaClass) return (kIOReturnNoMemory);
    return (kIOReturnSuccess);
}

kern_return_t
AppleUserTestHIDDeviceMetaClass::New(OSObject * instance)
{
    if (!new(instance) AppleUserTestHIDDevice) return (kIOReturnNoMemory);
    return (kIOReturnSuccess);
}

#endif /* !KERNEL */

kern_return_t
AppleUserTestHIDDevice::Dispatch(const IORPC rpc)
{
    return _Dispatch(this, rpc);
}

kern_return_t
AppleUserTestHIDDevice::_Dispatch(AppleUserTestHIDDevice * self, const IORPC rpc)
{
    kern_return_t ret = kIOReturnUnsupported;
    IORPCMessage * msg = IORPCMessage(rpc.message);

    switch (msg->msgid)
    {
        case IOService_Start_ID:
            ret = self->Start_Invoke(rpc, (kern_return_t (IOService::*)(IOService_Start_Args)) &AppleUserTestHIDDevice::Start_Impl);
            break;
        case IOService_Stop_ID:
            ret = self->Stop_Invoke(rpc, (kern_return_t (IOService::*)(IOService_Stop_Args)) &AppleUserTestHIDDevice::Stop_Impl);
            break;
        case AppleUserTestHIDDevice_HandleReportCallback_ID:
            ret = self->HandleReportCallback_Invoke(rpc, &AppleUserTestHIDDevice::HandleReportCallback_Impl);
            break;

        default:
            ret = IOUserHIDDevice::_Dispatch(self, rpc);
            break;
    }

    return (ret);
}

#if KERNEL
kern_return_t
AppleUserTestHIDDevice::MetaClass::Dispatch(const IORPC rpc)
{
#else /* KERNEL */
kern_return_t
AppleUserTestHIDDeviceMetaClass::Dispatch(const IORPC rpc)
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

void
AppleUserTestHIDDevice::HandleReportCallback(
        uint64_t timestamp,
        IOMemoryDescriptor * report,
        IOHIDReportType type,
        uint32_t reportID,
        OSAction * action,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        AppleUserTestHIDDevice_HandleReportCallback_Msg msg;
    } buf;
    struct AppleUserTestHIDDevice_HandleReportCallback_Msg * msg = &buf.msg;

    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.hdr.flags   = 1*kIORPCMessageOneway
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.hdr.msgid   = AppleUserTestHIDDevice_HandleReportCallback_ID;
    msg->content.hdr.object = (OSObjectRef) this;
    msg->content.hdr.objectRefs = AppleUserTestHIDDevice_HandleReportCallback_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 3;

    msg->content.timestamp = timestamp;
    msg->content.report = (OSObjectRef) report;
    msg->content.type = type;
    msg->content.reportID = reportID;
    msg->content.action = (OSObjectRef) action;
    IORPC rpc = { .message = &buf.msg.mach, .sendSize = sizeof(*msg), .reply = NULL, .replySize = 0 };
    if (supermethod) ret = supermethod((OSObject *)this, rpc);
    else             ret = ((OSObject *)this)->Invoke(rpc);

}

kern_return_t
AppleUserTestHIDDevice::HandleReportCallback_Invoke(const IORPC _rpc,
        void (AppleUserTestHIDDevice::*func)(AppleUserTestHIDDevice_HandleReportCallback_Args))
{
    AppleUserTestHIDDevice_HandleReportCallback_Invocation rpc = { _rpc };
    IOMemoryDescriptor * report;
    OSAction * action;

    if (AppleUserTestHIDDevice_HandleReportCallback_Msg_ObjRefs != rpc.message->content.hdr.objectRefs) return (kIOReturnIPCError);
    report = OSDynamicCast(IOMemoryDescriptor, (OSObject *) rpc.message->content.report);
    if (!report && rpc.message->content.report) return (kIOReturnBadArgument);
    action = OSDynamicCast(OSAction, (OSObject *) rpc.message->content.action);
    if (!action && rpc.message->content.action) return (kIOReturnBadArgument);

    (this->*func)(        
        rpc.message->content.timestamp,
        report,
        rpc.message->content.type,
        rpc.message->content.reportID,
        action);

    return (kIOReturnSuccess);
}



