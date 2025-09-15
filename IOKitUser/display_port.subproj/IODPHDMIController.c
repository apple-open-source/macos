#include <strings.h>
#include <pthread.h>
#include <AssertMacros.h>
#include <CoreFoundation/CFRuntime.h>

#include "IODPLib.h"
#include "IOAVLibPrivate.h"
#include "IOAVLibUtil.h"
#include <IOKit/display_port/IODPHDMIPortUserClient.h>
#include <IOKit/display_port/IODPCommandKeys.h>
#include <IOKit/display_port/IODPPropertyKeys.h>

//############# IODPHDMIControllerPort #############

static void                 __IODPHDMIControllerPortRegister(void);
static void                 __IODPHDMIControllerPortFree( CFTypeRef object );

typedef struct __IODPHDMIControllerPort
{
    CFRuntimeBase           cfBase;   // base CFType information

    io_service_t            service;
    io_connect_t            connect;

    IODPPortUserClientType  portType;
    uint32_t                portNumber;

} __IODPHDMIControllerPort, *__IODPHDMIControllerPortRef;

static const CFRuntimeClass __IODPHDMIControllerPortClass = {
    0,                          // version
    "IODPHDMIControllerPort",   // className
    NULL,                       // init
    NULL,                       // copy
    __IODPHDMIControllerPortFree,       // finalize
    NULL,                       // equal
    NULL,                       // hash
    NULL,                       // copyFormattingDesc
    NULL,
    NULL,
    NULL
};

static pthread_once_t   __portTypeInit    = PTHREAD_ONCE_INIT;
static CFTypeID         __kIODPHDMIControllerPortTypeID = _kCFRuntimeNotATypeID;


//------------------------------------------------------------------------------
// __IODPHDMIControllerPortRegister
//------------------------------------------------------------------------------
void __IODPHDMIControllerPortRegister(void)
{
    __kIODPHDMIControllerPortTypeID = _CFRuntimeRegisterClass(&__IODPHDMIControllerPortClass);
}

//------------------------------------------------------------------------------
// __IODPHDMIControllerPortFree
//------------------------------------------------------------------------------
void __IODPHDMIControllerPortFree( CFTypeRef object )
{
    IODPHDMIControllerPortRef port = ( IODPHDMIControllerPortRef ) object;

    if ( port->connect != MACH_PORT_NULL )
        IOServiceClose(port->connect);

    if ( port->service != MACH_PORT_NULL)
        IOObjectRelease(port->service);
}

//------------------------------------------------------------------------------
// IODPHDMIControllerPortGetTypeID
//------------------------------------------------------------------------------
CFTypeID IODPHDMIControllerPortGetTypeID(void)
{
    if ( __kIODPHDMIControllerPortTypeID == _kCFRuntimeNotATypeID )
        pthread_once(&__portTypeInit, __IODPHDMIControllerPortRegister);

    return __kIODPHDMIControllerPortTypeID;
}

//------------------------------------------------------------------------------
// IODPHDMIControllerPortCreate
//------------------------------------------------------------------------------
IODPHDMIControllerPortRef IODPHDMIControllerPortCreate(CFAllocatorRef allocator, IODPPortUserClientType portType, uint32_t portNumber)
{
    return allocator == kCFAllocatorDefault ? IODPCopyFirstMatchingPort(IODPHDMIControllerPort, portType, portNumber, (uint32_t)-1) : NULL;
}

//------------------------------------------------------------------------------
// IODPHDMIControllerPortCreateWithService
//------------------------------------------------------------------------------
IODPHDMIControllerPortRef IODPHDMIControllerPortCreateWithService(CFAllocatorRef allocator, io_service_t service)
{
    IODPHDMIControllerPortRef   port  = NULL;
    CFTypeRef                   property    = NULL;
    void *                      offset      = NULL;
    uint32_t                    size;
    kern_return_t               err = 0;

    require(service != MACH_PORT_NULL, error);

    require(IOAVObjectConformsTo(service, kIODPHDMIControllerPortKey), error);

    /* allocate port object */
    size  = sizeof(__IODPHDMIControllerPort) - sizeof(CFRuntimeBase);
    port  = (IODPHDMIControllerPortRef)_CFRuntimeCreateInstance(allocator, IODPHDMIControllerPortGetTypeID(), size, NULL);

    require(port, error);

    offset = port;
    bzero(offset + sizeof(CFRuntimeBase), size);

    port->service = service;
    IOObjectRetain(port->service);

    require_noerr_action(err = IOServiceOpen(port->service, mach_task_self(), kIODPHDMIControllerPortUserClientType, &port->connect), error, printf("failed to open io_service_t err=0x%x", err));

    require_noerr(err = getPortProperty(port->service, &port->portType, &port->portNumber, NULL), error);

    return port;

error:
    if ( port ) {
        CFRelease(port);
    }
    if ( property ) {
        CFRelease(property);
    }

    return NULL;
}

//------------------------------------------------------------------------------
// IODPHDMIControllerPortGetAddress
//------------------------------------------------------------------------------
void IODPHDMIControllerPortGetAddress(IODPHDMIControllerPortRef port, IODPPortUserClientType * portType, uint32_t * portNumber)
{
    *portType = port->portType;
    *portNumber = port->portNumber;
}

//------------------------------------------------------------------------------
// IODPHDMIControllerPortEnablePCON
//------------------------------------------------------------------------------
IOReturn IODPHDMIControllerPortEnablePCON(IODPHDMIControllerPortRef port)
{
    return IOConnectCallMethod(port->connect, kIODPHDMIPortUserClientMethodEnablePCON, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
}

//------------------------------------------------------------------------------
// IODPHDMIControllerPortDisablePCON
//------------------------------------------------------------------------------
IOReturn IODPHDMIControllerPortDisablePCON(IODPHDMIControllerPortRef port)
{
    return IOConnectCallMethod(port->connect, kIODPHDMIPortUserClientMethodDisablePCON, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
}

//------------------------------------------------------------------------------
// IODPHDMIControllerGetPCONStatus
//------------------------------------------------------------------------------
IOReturn IODPHDMIControllerGetPCONStatus(IODPHDMIControllerPortRef port, bool * status)
{
    uint64_t scalarOut[1] = {(uint64_t)false};
    uint32_t scalarOutSize = 1;

    IOReturn ret = IOConnectCallMethod(port->connect, kIODPHDMIPortUserClientmethodPCONStatus, NULL, NULL, NULL, 0, scalarOut, &scalarOutSize, NULL, NULL);
    require_noerr(ret, exit);
    require(scalarOutSize == 1, exit);

    *status = scalarOut[0];
exit:
    return ret;
}

//------------------------------------------------------------------------------
// IODPHDMIControllerPortSetPortEnable
//------------------------------------------------------------------------------
IOReturn IODPHDMIControllerPortSetPortEnable(IODPHDMIControllerPortRef port, bool enable)
{
    uint64_t scalar[] = {enable};

    return IOConnectCallMethod(port->connect, kIODPHDMIPortUserClientMethodSetPortEnable, scalar, sizeof(scalar)/sizeof(uint64_t), NULL, NULL, NULL, NULL, NULL, NULL);
}


//------------------------------------------------------------------------------
// IODPHDMIControllerPortSetHDMIHPD
//------------------------------------------------------------------------------
IOReturn IODPHDMIControllerPortSetHDMIHPD(IODPHDMIControllerPortRef port, bool enable)
{
    uint64_t scalar[] = {enable};

    return IOConnectCallMethod(port->connect, kIODPHDMIPortUserClientMethodSetHDMIHPD, scalar, sizeof(scalar)/sizeof(uint64_t), NULL, NULL, NULL, NULL, NULL, NULL);
}

