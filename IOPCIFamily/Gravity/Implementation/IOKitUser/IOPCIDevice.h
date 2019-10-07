/* iig(DriverKit-) generated from IOPCIDevice.iig */

#undef	IIG_IMPLEMENTATION
#define	IIG_IMPLEMENTATION 	IOPCIDevice.iig

#include <DriverKit/IOReturn.h>
#include <IOKitUser/IOPCIDevice.h>


struct IOPCIDevice_MemoryAccess_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    uint64_t  operation;
    uint64_t  space;
    uint64_t  address;
    uint64_t  data;
};
#pragma pack(4)
struct IOPCIDevice_MemoryAccess_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    IOPCIDevice_MemoryAccess_Msg_Content content;
};
#pragma pack()
#define IOPCIDevice_MemoryAccess_Msg_ObjRefs (1)

struct IOPCIDevice_MemoryAccess_Rpl_Content
{
    IORPCMessage __hdr;
    kern_return_t __result;
    unsigned long long  returnData;
};
#pragma pack(4)
struct IOPCIDevice_MemoryAccess_Rpl
{
    IORPCMessageMach           mach;
    IOPCIDevice_MemoryAccess_Rpl_Content content;
};
#pragma pack()
#define IOPCIDevice_MemoryAccess_Rpl_ObjRefs (0)


typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOPCIDevice_MemoryAccess_Msg * message;
        struct IOPCIDevice_MemoryAccess_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOPCIDevice_MemoryAccess_Invocation;
struct IOPCIDevice_CopyMemoryDescriptor_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    uint64_t  index;
};
#pragma pack(4)
struct IOPCIDevice_CopyMemoryDescriptor_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    IOPCIDevice_CopyMemoryDescriptor_Msg_Content content;
};
#pragma pack()
#define IOPCIDevice_CopyMemoryDescriptor_Msg_ObjRefs (1)

struct IOPCIDevice_CopyMemoryDescriptor_Rpl_Content
{
    IORPCMessage __hdr;
    OSObjectRef  memory;
    kern_return_t __result;
};
#pragma pack(4)
struct IOPCIDevice_CopyMemoryDescriptor_Rpl
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t memory__descriptor;
    IOPCIDevice_CopyMemoryDescriptor_Rpl_Content content;
};
#pragma pack()
#define IOPCIDevice_CopyMemoryDescriptor_Rpl_ObjRefs (1)


typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOPCIDevice_CopyMemoryDescriptor_Msg * message;
        struct IOPCIDevice_CopyMemoryDescriptor_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOPCIDevice_CopyMemoryDescriptor_Invocation;
#if !KERNEL
extern OSMetaClass * gOSContainerMetaClass;
extern OSMetaClass * gOSDataMetaClass;
extern OSMetaClass * gOSNumberMetaClass;
extern OSMetaClass * gOSStringMetaClass;
extern OSMetaClass * gOSBooleanMetaClass;
extern OSMetaClass * gOSDictionaryMetaClass;
extern OSMetaClass * gOSArrayMetaClass;
extern OSMetaClass * gIODispatchQueueMetaClass;
extern OSMetaClass * gIOBufferMemoryDescriptorMetaClass;
extern OSMetaClass * gIOUserClientMetaClass;
extern OSMetaClass * gIOMemoryMapMetaClass;
#endif /* !KERNEL */

#if !KERNEL

#define IOPCIDevice_QueueNames  ""

#define IOPCIDevice_MethodNames  ""

#define IOPCIDeviceMetaClass_MethodNames  ""

struct OSClassDescription_IOPCIDevice_t
{
    OSClassDescription base;
    uint64_t           methodOptions[2 * 0];
    uint64_t           metaMethodOptions[2 * 0];
    char               queueNames[sizeof(IOPCIDevice_QueueNames)];
    char               methodNames[sizeof(IOPCIDevice_MethodNames)];
    char               metaMethodNames[sizeof(IOPCIDeviceMetaClass_MethodNames)];
};

const struct OSClassDescription_IOPCIDevice_t
OSClassDescription_IOPCIDevice =
{
    .base =
    {
        .descriptionSize         = sizeof(OSClassDescription_IOPCIDevice_t),
        .name                    = "IOPCIDevice",
        .superName               = "IOService",
        .flags                   = 1*kOSClassCanRemote,
        .methodOptionsSize       = 2 * sizeof(uint64_t) * 0,
        .metaMethodOptionsSize   = 2 * sizeof(uint64_t) * 0,
        .queueNamesSize       = sizeof(IOPCIDevice_QueueNames),
        .methodNamesSize         = sizeof(IOPCIDevice_MethodNames),
        .metaMethodNamesSize     = sizeof(IOPCIDeviceMetaClass_MethodNames),
        .methodOptionsOffset     = __builtin_offsetof(struct OSClassDescription_IOPCIDevice_t, methodOptions),
        .metaMethodOptionsOffset = __builtin_offsetof(struct OSClassDescription_IOPCIDevice_t, metaMethodOptions),
        .queueNamesOffset     = __builtin_offsetof(struct OSClassDescription_IOPCIDevice_t, queueNames),
        .methodNamesOffset       = __builtin_offsetof(struct OSClassDescription_IOPCIDevice_t, methodNames),
        .metaMethodNamesOffset   = __builtin_offsetof(struct OSClassDescription_IOPCIDevice_t, metaMethodNames),
    },
    .methodOptions =
    {
    },
    .metaMethodOptions =
    {
    },
    .queueNames      = IOPCIDevice_QueueNames,
    .methodNames     = IOPCIDevice_MethodNames,
    .metaMethodNames = IOPCIDeviceMetaClass_MethodNames,
};

OSMetaClass * gIOPCIDeviceMetaClass;

static kern_return_t
IOPCIDevice_New(OSMetaClass * instance);

const OSClassLoadInformation
IOPCIDevice_Class = 
{
    .version           = 1,
    .description       = &OSClassDescription_IOPCIDevice.base,
    .instanceSize      = sizeof(IOPCIDevice),

    .metaPointer       = &gIOPCIDeviceMetaClass,
    .New               = &IOPCIDevice_New,
};

extern const void * const
gIOPCIDevice_Declaration;
const void * const
gIOPCIDevice_Declaration
__attribute__((visibility("hidden"),section("__DATA_CONST,__osclassinfo,regular,no_dead_strip")))
    = &IOPCIDevice_Class;

static kern_return_t
IOPCIDevice_New(OSMetaClass * instance)
{
    if (!new(instance) IOPCIDeviceMetaClass) return (kIOReturnNoMemory);
    return (kIOReturnSuccess);
}

kern_return_t
IOPCIDeviceMetaClass::New(OSObject * instance)
{
    if (!new(instance) IOPCIDevice) return (kIOReturnNoMemory);
    return (kIOReturnSuccess);
}

#endif /* !KERNEL */

kern_return_t
IOPCIDevice::Dispatch(const IORPC rpc)
{
    return _Dispatch(this, rpc);
}

kern_return_t
IOPCIDevice::_Dispatch(IOPCIDevice * self, const IORPC rpc)
{
    kern_return_t ret = kIOReturnUnsupported;
    IORPCMessage * msg = IORPCMessageFromMach(rpc.message, false);

    switch (msg->msgid)
    {
#if KERNEL
        case IOPCIDevice_MemoryAccess_ID:
        {
            union {
                kern_return_t (IOPCIDevice::*fIn)(IOPCIDevice_MemoryAccess_Args);
                IOPCIDevice::MemoryAccess_Handler handler;
            } map;
            map.fIn = &IOPCIDevice::MemoryAccess_Impl;
            ret = IOPCIDevice::MemoryAccess_Invoke(rpc, self, map.handler);
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOPCIDevice_CopyMemoryDescriptor_ID:
        {
            union {
                kern_return_t (IOPCIDevice::*fIn)(IOPCIDevice_CopyMemoryDescriptor_Args);
                IOPCIDevice::CopyMemoryDescriptor_Handler handler;
            } map;
            map.fIn = &IOPCIDevice::CopyMemoryDescriptor_Impl;
            ret = IOPCIDevice::CopyMemoryDescriptor_Invoke(rpc, self, map.handler);
            break;
        }
#endif /* !KERNEL */

        default:
            ret = IOService::_Dispatch(self, rpc);
            break;
    }

    return (ret);
}

#if KERNEL
kern_return_t
IOPCIDevice::MetaClass::Dispatch(const IORPC rpc)
{
#else /* KERNEL */
kern_return_t
IOPCIDeviceMetaClass::Dispatch(const IORPC rpc)
{
#endif /* !KERNEL */

    kern_return_t ret = kIOReturnUnsupported;
    IORPCMessage * msg = IORPCMessageFromMach(rpc.message, false);

    switch (msg->msgid)
    {

        default:
            ret = OSMetaClassBase::Dispatch(rpc);
            break;
    }

    return (ret);
}

kern_return_t
IOPCIDevice::MemoryAccess(
        uint64_t operation,
        uint64_t space,
        uint64_t address,
        uint64_t data,
        uint64_t * returnData,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOPCIDevice_MemoryAccess_Msg msg;
        struct
        {
            IOPCIDevice_MemoryAccess_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOPCIDevice_MemoryAccess_Msg * msg = &buf.msg;
    struct IOPCIDevice_MemoryAccess_Rpl * rpl = &buf.rpl.rpl;

    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOPCIDevice_MemoryAccess_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOPCIDevice_MemoryAccess_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 1;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->content.operation = operation;

    msg->content.space = space;

    msg->content.address = address;

    msg->content.data = data;

    IORPC rpc = { .message = &buf.msg.mach, .sendSize = sizeof(buf.msg), .reply = &buf.rpl.rpl.mach, .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, rpc);
    else             ret = ((OSObject *)this)->Invoke(rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOPCIDevice_MemoryAccess_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IOPCIDevice_MemoryAccess_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
            ret = rpl->content.__result;
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
        if (returnData) *returnData = rpl->content.returnData;
    }


    return (ret);
}

kern_return_t
IOPCIDevice::CopyMemoryDescriptor(
        uint64_t index,
        IOMemoryDescriptor ** memory,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOPCIDevice_CopyMemoryDescriptor_Msg msg;
        struct
        {
            IOPCIDevice_CopyMemoryDescriptor_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOPCIDevice_CopyMemoryDescriptor_Msg * msg = &buf.msg;
    struct IOPCIDevice_CopyMemoryDescriptor_Rpl * rpl = &buf.rpl.rpl;

    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 0*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOPCIDevice_CopyMemoryDescriptor_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOPCIDevice_CopyMemoryDescriptor_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 1;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->content.index = index;

    IORPC rpc = { .message = &buf.msg.mach, .sendSize = sizeof(buf.msg), .reply = &buf.rpl.rpl.mach, .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, rpc);
    else             ret = ((OSObject *)this)->Invoke(rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOPCIDevice_CopyMemoryDescriptor_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 1) { ret = kIOReturnIPCError; break; };
            if (IOPCIDevice_CopyMemoryDescriptor_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
            ret = rpl->content.__result;
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
        *memory = OSDynamicCast(IOMemoryDescriptor, (OSObject *) rpl->content.memory);
        if (rpl->content.memory && !*memory) ret = kIOReturnBadArgument;
    }


    return (ret);
}

kern_return_t
IOPCIDevice::MemoryAccess_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        MemoryAccess_Handler func)
{
    IOPCIDevice_MemoryAccess_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IOPCIDevice_MemoryAccess_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target,        
        rpc.message->content.operation,
        rpc.message->content.space,
        rpc.message->content.address,
        rpc.message->content.data,
        &rpc.reply->content.returnData);

    {
        rpc.reply->content.__hdr.msgid = IOPCIDevice_MemoryAccess_ID;
        rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
        rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
        rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
        rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
        rpc.reply->content.__result    = ret;
        if (kIOReturnSuccess == ret)
        {
            rpc.reply->content.__hdr.objectRefs = IOPCIDevice_MemoryAccess_Rpl_ObjRefs;
        }
        else
        {
            rpc.reply->content.__hdr.objectRefs = 0;
        }
    }
    return (kIOReturnSuccess);
}

kern_return_t
IOPCIDevice::CopyMemoryDescriptor_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        CopyMemoryDescriptor_Handler func)
{
    IOPCIDevice_CopyMemoryDescriptor_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IOPCIDevice_CopyMemoryDescriptor_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target,        
        rpc.message->content.index,
        (IOMemoryDescriptor **)&rpc.reply->content.memory);

    {
        rpc.reply->content.__hdr.msgid = IOPCIDevice_CopyMemoryDescriptor_ID;
        rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
        rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
        rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
        rpc.reply->mach.msgh_body.msgh_descriptor_count = 1;
        rpc.reply->content.__result    = ret;
        if (kIOReturnSuccess == ret)
        {
            rpc.reply->content.__hdr.objectRefs = IOPCIDevice_CopyMemoryDescriptor_Rpl_ObjRefs;
            rpc.reply->memory__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
        }
        else
        {
            rpc.reply->content.__hdr.objectRefs = 0;
            rpc.reply->memory__descriptor.type        = MACH_MSG_PORT_DESCRIPTOR;
            rpc.reply->memory__descriptor.disposition = MACH_MSG_TYPE_PORT_NONE;
            rpc.reply->memory__descriptor.name        = MACH_PORT_NULL;
            rpc.reply->content.memory = (OSObjectRef) 0;
        }
    }
    return (kIOReturnSuccess);
}



