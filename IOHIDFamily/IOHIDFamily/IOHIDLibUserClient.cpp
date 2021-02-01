/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 1999-2020 Apple Computer, Inc.        All Rights Reserved.
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include <TargetConditionals.h>

#include <sys/systm.h>
#include <sys/proc.h>
#include <kern/task.h>
#include <mach/port.h>
#include <mach/message.h>
#include <mach/mach_port.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOService.h>
#include <IOKit/IOSyncer.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOKitKeysPrivate.h>
#include <IOKit/IOUserClient.h>
#include "IOHIDLibUserClient.h"
#include "IOHIDKeys.h"
#include "IOHIDDevice.h"
#include "IOHIDEventQueue.h"
#include "IOHIDReportElementQueue.h"
#include "IOHIDDebug.h"
#include <IOKit/hidsystem/IOHIDUsageTables.h>
#include "IOHIDPrivateKeys.h"
#include <AssertMacros.h>
#include "IOHIDDeviceElementContainer.h"
#include <IOKit/hid/IOHIDDeviceTypes.h>

__BEGIN_DECLS
#include <ipc/ipc_port.h>
__END_DECLS


#define kIOHIDManagerUserAccessKeyboardEntitlement          "com.apple.hid.manager.user-access-keyboard"
#define kIOHIDManagerUserAccessPrivilegedEntitlement        "com.apple.hid.manager.user-access-privileged"
#define kIOHIDManagerUserAccessCustomQueueSizeEntitlement   "com.apple.hid.manager.user-access-custom-queue-size"
#define kIOHIDManagerUserAccessProtectedEntitlement         "com.apple.hid.manager.user-access-protected"

#define HIDLibUserClientLogFault(fmt, ...)   HIDLogFault("%s:0x%llx " fmt "\n", getName(), getRegistryEntryID(), ##__VA_ARGS__)
#define HIDLibUserClientLogError(fmt, ...)   HIDLogError("%s:0x%llx " fmt "\n", getName(), getRegistryEntryID(), ##__VA_ARGS__)
#define HIDLibUserClientLog(fmt, ...)        HIDLog("%s:0x%llx " fmt "\n", getName(), getRegistryEntryID(), ##__VA_ARGS__)
#define HIDLibUserClientLogTest(fmt, ...)        HIDLog(" " fmt "\n",  ##__VA_ARGS__)
#define HIDLibUserClientLogInfo(fmt, ...)    HIDLogInfo("%s:0x%llx " fmt "\n", getName(), getRegistryEntryID(), ##__VA_ARGS__)
#define HIDLibUserClientLogDebug(fmt, ...)   HIDLogDebug("%s:0x%llx " fmt "\n", getName(), getRegistryEntryID(), ##__VA_ARGS__)

#define super IOUserClient

enum {
    kHIDLibUserClientNotEntitled = 0,
    kHIDLibUserClientKeyboardAccessEntitlement = 0x01,
    kHIDLibUserClientUserAccessPrivelegeEntitlement = 0x02,
    kHIDLibUserClientCustomQueueSizeEntitlement = 0x04,
    kHIDLibUserClientUserAccessProtectedEntitlement = 0x08,
    kHIDLibUserClientAdministratorPrivilege = 0x10,
};

struct AsyncParam {
    OSAsyncReference64        fAsyncRef;
    UInt32                    fMax;
    IOMemoryDescriptor        *fMem;
    IOHIDReportType            reportType;
    uint32_t                   fAsyncCount;
    IOHIDCompletion            fCompletion;
};

struct AsyncGateParam {
    OSAsyncReference        asyncRef;
    IOHIDReportType            reportType;
    UInt32                    reportID;
    void *                    reportBuffer;
    UInt32                    reportBufferSize;
    UInt32                    completionTimeOutMS;
};

OSDefineMetaClassAndStructors(IOHIDOOBReportDescriptor, IOBufferMemoryDescriptor);

IOHIDOOBReportDescriptor *IOHIDOOBReportDescriptor::inTaskWithBytes(
    task_t       inTask,
    const void * inBytes,
    vm_size_t    inLength,
    IODirection  inDirection,
    bool         inContiguous)
{
    IOHIDOOBReportDescriptor *me = new IOHIDOOBReportDescriptor;
    IOOptionBits options = 0;
    if (inTask != kernel_task) {
        options |= kIOMemoryPageable | kIOMemoryPurgeable;
    }
    if (me && !me->initWithPhysicalMask(inTask, inDirection | options
                                        | (inContiguous ? kIOMemoryPhysicallyContiguous : 0),
                                        inLength, PAGE_SIZE, 0)) {
        OSSafeReleaseNULL(me);
    }
    if (me) {
        me->setLength(inLength);
        IOMemoryMap *map = me->map();
        bcopy(inBytes, (void*)map->getAddress(), inLength);
        OSSafeReleaseNULL(map);
    }
    return me;
}

OSDefineMetaClassAndStructors(IOHIDLibUserClient, IOUserClient);


const IOExternalMethodDispatch IOHIDLibUserClient::
sMethods[kIOHIDLibUserClientNumCommands] = {
    { //    kIOHIDLibUserClientDeviceIsValid
    NULL, // Unsupported
    0, 0,
    2, 0
    },
    { //    kIOHIDLibUserClientOpen
    (IOExternalMethodAction) &IOHIDLibUserClient::_open,
    1, 0,
    0, 0
    },
    { //    kIOHIDLibUserClientClose
    (IOExternalMethodAction) &IOHIDLibUserClient::_close,
    0, 0,
    0, 0
    },
    { //    kIOHIDLibUserClientCreateQueue
    (IOExternalMethodAction) &IOHIDLibUserClient::_createQueue,
    2, 0,
    1, 0
    },
    { //    kIOHIDLibUserClientDisposeQueue
    (IOExternalMethodAction) &IOHIDLibUserClient::_disposeQueue,
    1, 0,
    0, 0
    },
    { //    kIOHIDLibUserClientAddElementToQueue
    (IOExternalMethodAction) &IOHIDLibUserClient::_addElementToQueue,
    3, 0,
    1, 0
    },
    { //    kIOHIDLibUserClientRemoveElementFromQueue
    (IOExternalMethodAction) &IOHIDLibUserClient::_removeElementFromQueue,
    2, 0,
    1, 0
    },
    { //    kIOHIDLibUserClientQueueHasElement
    (IOExternalMethodAction) &IOHIDLibUserClient::_queueHasElement,
    2, 0,
    1, 0
    },
    { //    kIOHIDLibUserClientStartQueue
    (IOExternalMethodAction) &IOHIDLibUserClient::_startQueue,
    1, 0,
    0, 0
    },
    { //    kIOHIDLibUserClientStopQueue
    (IOExternalMethodAction) &IOHIDLibUserClient::_stopQueue,
    1, 0,
    0, 0
    },
    { //    kIOHIDLibUserClientUpdateElementValues
    (IOExternalMethodAction) &IOHIDLibUserClient::_updateElementValues,
    1, kIOUCVariableStructureSize,
    0, kIOUCVariableStructureSize
    },
    { //    kIOHIDLibUserClientPostElementValues
    (IOExternalMethodAction) &IOHIDLibUserClient::_postElementValues,
    0, kIOUCVariableStructureSize,
    0, 0
    },
    { //    kIOHIDLibUserClientGetReport
    (IOExternalMethodAction) &IOHIDLibUserClient::_getReport,
    3, 0,
    0, kIOUCVariableStructureSize
    },
    { //    kIOHIDLibUserClientSetReport
    (IOExternalMethodAction) &IOHIDLibUserClient::_setReport,
    3, kIOUCVariableStructureSize,
    0, 0
    },
    { //    kIOHIDLibUserClientGetElementCount
    (IOExternalMethodAction) &IOHIDLibUserClient::_getElementCount,
    0, 0,
    2, 0
    },
    { //    kIOHIDLibUserClientGetElements
    (IOExternalMethodAction) &IOHIDLibUserClient::_getElements,
    1, 0,
    0, kIOUCVariableStructureSize
    },
    // ASYNC METHODS
    { //    kIOHIDLibUserClientSetQueueAsyncPort
    (IOExternalMethodAction) &IOHIDLibUserClient::_setQueueAsyncPort,
    1, 0,
    0, 0
    },
    { //    kIOHIDLibUserClientReleaseReport
    (IOExternalMethodAction) &IOHIDLibUserClient::_releaseReport,
    1, 0,
    0, 0
    }
};

bool IOHIDLibUserClient::initWithTask(task_t owningTask, void * /* security_id */, UInt32 /* type */)
{
    OSObject *entitlement;
    char name[255];
    bzero(name, sizeof(name));
    uint32_t clientEntitlements = kHIDLibUserClientNotEntitled;
    
    if (!super::init())
        return false;

    fValid = false;
    
    fClient = owningTask;
    task_reference (fClient);

    proc_t p = (proc_t)get_bsdtask_info(fClient);
    fPid = proc_pid(p);
    
    proc_name(fPid, name, sizeof(name));
    
    entitlement = copyClientEntitlement(owningTask, kIOHIDManagerUserAccessProtectedEntitlement);
    
    if (entitlement) {
        _protectedAccessClient |= (entitlement == kOSBooleanTrue);
        entitlement->release();
        clientEntitlements |= kHIDLibUserClientUserAccessProtectedEntitlement;
    }
    
    
    entitlement = copyClientEntitlement(owningTask, kIOHIDManagerUserAccessPrivilegedEntitlement);
    
    if (entitlement) {
        _privilegedClient |= (entitlement == kOSBooleanTrue);
        entitlement->release();
        clientEntitlements |= kHIDLibUserClientUserAccessPrivelegeEntitlement;
    }
    
    entitlement = copyClientEntitlement(owningTask, kIOHIDManagerUserAccessKeyboardEntitlement);
    
    if (entitlement) {
        _privilegedClient |= (entitlement == kOSBooleanTrue);
        entitlement->release();
        clientEntitlements |= kHIDLibUserClientKeyboardAccessEntitlement;
    }
    
    IOReturn ret = IOUserClient::clientHasPrivilege(owningTask, kIOClientPrivilegeAdministrator);
    
    _privilegedClient |= (ret == kIOReturnSuccess);
    
    if (ret == kIOReturnSuccess) {
        clientEntitlements |=  kHIDLibUserClientAdministratorPrivilege;
    }
    
    if (!_privilegedClient) {
        // Preparing for extended data. Set a temporary key.
        setProperty(kIOHIDExtendedDataKey, true);
    }

    fClientOpened = false;
    
    queue_head_init(fOOBReports);

    fReportLimit = kIOHIDDefaultMaxReportSize;

    fQueueMap = OSArray::withCapacity(4);
    if (!fQueueMap) {
        return false;
    }
    
    entitlement = copyClientEntitlement(fClient, kIOHIDManagerUserAccessCustomQueueSizeEntitlement);
    
    if (entitlement) {
        _customQueueSizeEntitlement = (entitlement == kOSBooleanTrue);
        entitlement->release();
        clientEntitlements |= kHIDLibUserClientCustomQueueSizeEntitlement;
    }
    
    HIDLibUserClientLogInfo("[%s] Entitlements %x privilegedClient : %s", name, clientEntitlements , _privilegedClient ? "Yes" : "No");

    _pending = OSSet::withCapacity(4);
    if (!_pending) {
        return false;
    }
    
    return true;
}

IOReturn IOHIDLibUserClient::clientClose(void)
{
    
    terminate();
    
    return kIOReturnSuccess;
}


bool IOHIDLibUserClient::start(IOService *provider)
{
    OSDictionary    *matching = NULL;
    IOCommandGate   *cmdGate  = NULL;
    OSObject        *obj;
    OSNumber        *primaryUsage;
    OSObject        *obj2;
    OSNumber        *primaryUsagePage;
    OSSerializer    *debugStateSerializer;

    HIDLibUserClientLogInfo("start");

    if (provider->getProperty(kIOHIDProtectedAccessKey) == kOSBooleanTrue &&
        _protectedAccessClient == false) {
        HIDLibUserClientLogError("Missing entitlement to access protected service");
        return false;
    }
  
    require(super::start(provider), error);

    fNub = OSDynamicCast(IOHIDDevice, provider);
    require(fNub, error);
    
    fNub->retain();

    fWL = getWorkLoop();
    require(fWL, error);

    fWL->retain();

    obj = fNub->copyProperty(kIOHIDPrimaryUsageKey);
    primaryUsage = OSDynamicCast(OSNumber, obj);
    obj2 = fNub->copyProperty(kIOHIDPrimaryUsagePageKey);
    primaryUsagePage = OSDynamicCast(OSNumber, obj2);

    if ((primaryUsagePage && (primaryUsagePage->unsigned32BitValue() == kHIDPage_GenericDesktop)) &&
        (primaryUsage && ((primaryUsage->unsigned32BitValue() == kHIDUsage_GD_Keyboard) || (primaryUsage->unsigned32BitValue() == kHIDUsage_GD_Keypad))))
    {
        fNubIsKeyboard = true;
    }
    
    OSSafeReleaseNULL(obj);
    OSSafeReleaseNULL(obj2);
            
    cmdGate = IOCommandGate::commandGate(this);
    require(cmdGate, error);
    
    fWL->addEventSource(cmdGate);
    
    fGate = cmdGate;

    fResourceES = IOInterruptEventSource::interruptEventSource
        (this, OSMemberFunctionCast(IOInterruptEventSource::Action, this, &IOHIDLibUserClient::resourceNotificationGated));
    require(fResourceES, error);

    fWL->addEventSource(fResourceES);

    // Get notified everytime Root properties change
    matching = serviceMatching("IOResources");
    fResourceNotification = addMatchingNotification(
        gIOPublishNotification,
        matching,
        OSMemberFunctionCast(IOServiceMatchingNotificationHandler, this, &IOHIDLibUserClient::resourceNotification),
        this);
    OSSafeReleaseNULL(matching);

    require(fResourceNotification, error);
    
    debugStateSerializer = OSSerializer::forTarget(this, OSMemberFunctionCast(OSSerializerCallback, this, &IOHIDLibUserClient::serializeDebugState));
    if (debugStateSerializer) {
        setProperty("DebugState", debugStateSerializer);
        OSSafeReleaseNULL(debugStateSerializer);
    }
    
    return true;

error:
    HIDLibUserClientLogError("start failed");

    return false;
}

void IOHIDLibUserClient::stop(IOService *provider)
{
    HIDLibUserClientLogInfo("stop");
  
    if (fNub) {
        setStateForQueues(kHIDQueueStateClear);
    }
    
    if (fGate && _pending && _pending->getCount() > 0) {
    
        fGate->commandSleep(&_pending);
    
    }
    
  
    close();
    
    if ( fResourceNotification ) {
        fResourceNotification->remove();
    }
    if (fResourceES) {
        fWL->removeEventSource(fResourceES);
    }
    if (fGate) {
        fWL->removeEventSource(fGate);
    }
    
    super::stop(provider);
}

bool IOHIDLibUserClient::resourceNotification(void * refcon __unused, IOService *service __unused, IONotifier *notifier __unused)
{
    if (!isInactive() && fResourceES)
        fResourceES->interruptOccurred(0, 0, 0);
        
    return true;
}

void IOHIDLibUserClient::resourceNotificationGated()
{
    IOReturn ret = kIOReturnSuccess;
    
    do {
        if (_privilegedClient) {
            break;
        } else {
            HIDLibUserClientLogInfo("resourceNotificationGated client not privileged");
        }
        
        ret = kIOReturnError;

        OSObject * obj;
        OSData * data;
        IOService * service = getResourceService();
        if ( !service ) {
            ret = kIOReturnError;
            break;
        }
        
        obj = service->copyProperty(kIOConsoleUsersSeedKey);
        data = OSDynamicCast(OSData, obj);

        if ( !data || !data->getLength() || !data->getBytesNoCopy() ) {
            ret = kIOReturnError;
            OSSafeReleaseNULL(obj);
            break;
        }
            
        UInt64 currentSeed = 0;
        
        switch ( data->getLength() ) {
            case sizeof(UInt8):
                currentSeed = *(UInt8*)(data->getBytesNoCopy());
                break;
            case sizeof(UInt16):
                currentSeed = *(UInt16*)(data->getBytesNoCopy());
                break;
            case sizeof(UInt32):
                currentSeed = *(UInt32*)(data->getBytesNoCopy());
                break;
            case sizeof(UInt64):
            default:
                currentSeed = *(UInt64*)(data->getBytesNoCopy());
                break;
        }
        OSSafeReleaseNULL(obj);
            
        // We should return rather than break so that previous setting is retained
        if ( currentSeed == fCachedConsoleUsersSeed )
            return;
            
        fCachedConsoleUsersSeed = currentSeed;

       if ( fNubIsKeyboard ) {
          IOUCProcessToken token;
          token.token = fClient;
          token.pid = fPid;
          ret = clientHasPrivilege(&token, kIOClientPrivilegeSecureConsoleProcess);
        }
        if (ret == kIOReturnSuccess) {
            break;
        }
        if (fNubIsKeyboard) {
            if (ret != kIOReturnSuccess) {
                proc_t      process;
                process = (proc_t)get_bsdtask_info(fClient);
                char name[255];
                bzero(name, sizeof(name));
                proc_name(proc_pid(process), name, sizeof(name));
                HIDLibUserClientLogError("%s is not entitled for IOHIDLibUserClient keyboard access", name);
            }
            break;
        }

        ret = clientHasPrivilege(fClient, kIOClientPrivilegeConsoleUser);
    } while (false);
  
    setValid(kIOReturnSuccess == ret);
}

typedef struct HIDCommandGateArgs {
    uint32_t                    selector;
    IOExternalMethodArguments * arguments;
    IOExternalMethodDispatch *    dispatch;
    OSObject *                    target;
    void *                        reference;
}HIDCommandGateArgs;

IOReturn IOHIDLibUserClient::externalMethod(
                                uint32_t                    selector,
                                IOExternalMethodArguments * arguments,
                                IOExternalMethodDispatch *    dispatch,
                                OSObject *                    target,
                                void *                        reference)
{
    IOReturn status = kIOReturnOffline;
    
    if (fGate) {
        HIDCommandGateArgs args;
        
        args.selector    = selector;
        args.arguments    = arguments;
        args.dispatch    = dispatch;
        args.target        = target;
        args.reference    = reference;
        
        if (!isInactive())
            status = fGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, target, &IOHIDLibUserClient::externalMethodGated), (void *)&args);
    }

    return status;
}

IOReturn IOHIDLibUserClient::externalMethodGated(void * args)
{
    HIDCommandGateArgs *        cArgs        = (HIDCommandGateArgs *)args;
    uint32_t                    selector    = cArgs->selector;
    IOExternalMethodArguments * arguments    = cArgs->arguments;
    IOExternalMethodDispatch *    dispatch    = cArgs->dispatch;
    OSObject *                    target        = cArgs->target;
    void *                        reference    = cArgs->reference;

    if (isInactive())
        return kIOReturnOffline;
    
    if (selector < (uint32_t) kIOHIDLibUserClientNumCommands)
    {
        dispatch = (IOExternalMethodDispatch *) &sMethods[selector];
        
        if (!target)
            target = this;
    }
    
    return super::externalMethod(selector, arguments, dispatch, target, reference);
}

IOReturn IOHIDLibUserClient::_setQueueAsyncPort(IOHIDLibUserClient * target, void * reference __unused, IOExternalMethodArguments * arguments)
{
    return target->setQueueAsyncPort(target->getQueueForToken((u_int)arguments->scalarInput[0]), arguments->asyncWakePort);
}

IOReturn IOHIDLibUserClient::setQueueAsyncPort(IOHIDEventQueue * queue, mach_port_t port)
{
    if ( !queue )
        return kIOReturnBadArgument;

    queue->setNotificationPort(port);

    return kIOReturnSuccess;
}

IOReturn IOHIDLibUserClient::_open(IOHIDLibUserClient * target, void * reference __unused, IOExternalMethodArguments * arguments)
{
    return target->open((IOOptionBits)arguments->scalarInput[0]);
}

IOReturn IOHIDLibUserClient::open(IOOptionBits options)
{
    IOReturn ret = kIOReturnNotPrivileged;

    HIDLibUserClientLogInfo("open");
    
    do {
        if (_privilegedClient) {
            ret = kIOReturnSuccess;
            break;
        } else {
            HIDLibUserClientLogInfo("open client not privileged");
        }
        
        // RY: If this is a keyboard and the client is attempting to seize,
        // the client needs to be admin
        if ( !fNubIsKeyboard || ((options & kIOHIDOptionsTypeSeizeDevice) == 0) ) {
            ret = clientHasPrivilege(fClient, kIOClientPrivilegeLocalUser);
            if (ret == kIOReturnSuccess) {
              break;
            }
        }
    } while (false);

    if (ret != kIOReturnSuccess)
        return ret;

    if (!fNub)
        return kIOReturnOffline;
    if (!fNub->open(this, options))
        return kIOReturnExclusiveAccess;
    
    fClientOpened = true;
    fCachedOptionBits = options;

    fCachedConsoleUsersSeed = 0;
    resourceNotificationGated();

    return kIOReturnSuccess;
}


IOReturn IOHIDLibUserClient::_close(IOHIDLibUserClient * target, void * reference __unused, IOExternalMethodArguments * arguments __unused)
{
    return target->close();
}

IOReturn IOHIDLibUserClient::close()
{
    HIDLibUserClientLogInfo("close");

    if (!fClientOpened) {
        return kIOReturnSuccess;
    }

    if (fNub) {
        fNub->close(this, fCachedOptionBits);
    }

    setValid(false);
   
    fCachedOptionBits = 0;

    fClientOpened = false;
 
    return kIOReturnSuccess;
}

bool
IOHIDLibUserClient::didTerminate( IOService * provider, IOOptionBits options, bool * defer )
{
    close ();
    
    return super::didTerminate(provider, options, defer);
}

void IOHIDLibUserClient::free()
{
    OSSafeReleaseNULL(fQueueMap);
    OSSafeReleaseNULL(fNub);
    OSSafeReleaseNULL(fResourceES);
    OSSafeReleaseNULL(fGate);
    OSSafeReleaseNULL(fWL);
    while (!queue_empty(&fOOBReports)) {
        IOHIDOOBReportDescriptor *md = NULL;
        queue_remove_first(&fOOBReports, md, IOHIDOOBReportDescriptor *, qc);
        OSSafeReleaseNULL(md);
        HIDLibUserClientLog("Dropping report that was not released by IOKit client");
    }

    if ( fValidMessage ) {
        IOFree(fValidMessage, sizeof (struct _notifyMsg));
        fValidMessage = NULL;
    }

    if (fWakePort != MACH_PORT_NULL) {
        ipc_port_release_send(fWakePort);
        fWakePort = MACH_PORT_NULL;
    }

    if (fValidPort != MACH_PORT_NULL) {
        ipc_port_release_send(fValidPort);
        fValidPort = MACH_PORT_NULL;
    }

    if (fClient) {
        task_deallocate(fClient);
        fClient = 0;
    }
    
    if (_pending) {
        _pending->release();
    }

    super::free();
}

IOReturn IOHIDLibUserClient::message(UInt32 type, IOService * provider, void * argument )
{
    HIDLibUserClientLogDebug("message: 0x%x from: 0x%llx", (unsigned int)type, (provider ? provider->getRegistryEntryID() : 0));

    if ( !isInactive() && fGate && (type == kIOMessageServiceIsRequestingClose || type == kIOMessageServiceWasClosed)) {
        fGate->runAction(OSMemberFunctionCast(IOCommandGate::Action,
                                              this,
                                              &IOHIDLibUserClient::messageGated),
                         (void*)(intptr_t)type,
                         provider,
                         argument);
    }
    return super::message(type, provider, argument);
}

IOReturn IOHIDLibUserClient::messageGated(UInt32 type, IOService * provider __unused, void * argument )
{
    IOOptionBits options = (IOOptionBits)(uintptr_t)argument;
    switch ( type ) {
        case kIOMessageServiceIsRequestingClose:
            if ((options & kIOHIDOptionsTypeSeizeDevice) && (options != fCachedOptionBits)) {
                 HIDLibUserClientLogInfo("Message : Close Device");
                 setValid(false);
            }
            break;
            
        case kIOMessageServiceWasClosed:
            if ((options & kIOHIDOptionsTypeSeizeDevice) && (options != fCachedOptionBits)) {
                HIDLibUserClientLogInfo("Message : Seize Device");
                // instead of calling set valid, let's make sure we still have
                // permission through the resource notification
                fCachedConsoleUsersSeed = 0;
                resourceNotificationGated();
                
            }
            break;
    };
    
    return kIOReturnSuccess;
}

IOReturn IOHIDLibUserClient::setProperties(OSObject *properties)
{
    return fNub ? fNub->setProperties(properties) : kIOReturnOffline;
}

IOReturn IOHIDLibUserClient::registerNotificationPort(mach_port_t port, UInt32 type, UInt32 refCon)
{
    if (fGate) {
        return fGate->runAction(OSMemberFunctionCast(IOCommandGate::Action,
                                                     this,
                                                     &IOHIDLibUserClient::registerNotificationPortGated),
                                (void *)port,
                                (void*)(intptr_t)type,
                                (void*)(intptr_t)refCon);
    }
    else {
        return kIOReturnOffline;
    }
}

IOReturn IOHIDLibUserClient::registerNotificationPortGated(mach_port_t port, UInt32 type, UInt32 refCon __unused)
{
    IOReturn kr = kIOReturnSuccess;
    
    switch ( type ) {
        case kIOHIDLibUserClientAsyncPortType:
            if (fWakePort != MACH_PORT_NULL) {
                ipc_port_release_send(fWakePort);
                fWakePort = MACH_PORT_NULL;
            }
            fWakePort = port;
            break;
        case kIOHIDLibUserClientDeviceValidPortType:
            if (fValidPort != MACH_PORT_NULL) {
                ipc_port_release_send(fValidPort);
                fValidPort = MACH_PORT_NULL;
            }

            static struct _notifyMsg init_msg = { {
                MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0),
                sizeof (struct _notifyMsg),
                MACH_PORT_NULL,
                MACH_PORT_NULL,
                0,
                0
            } };
            
            if ( fValidMessage ) {
                IOFree(fValidMessage, sizeof (struct _notifyMsg));
                fValidMessage = NULL;
            }
                
            if ( !(fValidMessage = IOMalloc( sizeof(struct _notifyMsg))) ) {
                kr = kIOReturnNoMemory;
                break;
            }
            
            fValidPort = port;
            
            if ( !fValidPort ) {
                IOFree(fValidMessage, sizeof (struct _notifyMsg));
                fValidMessage = NULL;
                break;
            }

            // Initialize the events available message.
            *((struct _notifyMsg *)fValidMessage) = init_msg;

            ((struct _notifyMsg *)fValidMessage)->h.msgh_remote_port = fValidPort;
            
            dispatchMessage(fValidMessage);
            
            break;
        default:
            kr = kIOReturnUnsupported;
            break;
    };

    return kr;
}

void IOHIDLibUserClient::setValid(bool state)
{
    HIDLibUserClientLogInfo("setValid: from %s to %s", (fValid ? "true" : "false"), (state ? "true" : "false"));
    
    if (fValid == state)
        return;
    
    // set the queue states
    setStateForQueues(state ? kHIDQueueStateEnable : kHIDQueueStateDisable);
    
    // dispatch message
    dispatchMessage(fValidMessage);
    
    fValid = state;
}

IOReturn IOHIDLibUserClient::dispatchMessage(void * messageIn)
{
    IOReturn ret = kIOReturnError;
    mach_msg_header_t * msgh = (mach_msg_header_t *)messageIn;
    if( msgh) {
        ret = mach_msg_send_from_kernel( msgh, msgh->msgh_size);
        switch ( ret ) {
            case MACH_SEND_TIMED_OUT:/* Already has a message posted */
            case MACH_MSG_SUCCESS:    /* Message is posted */
                break;
        };
    }
    return ret;
}

bool IOHIDLibUserClient::serializeDebugState(void *ref __unused, OSSerialize *serializer)
{
    bool          result = false;
    OSDictionary  *debugDict = OSDictionary::withCapacity(1);
    OSNumber *num;

    require(debugDict, exit);
    
    debugDict->setObject("Privileged", _privilegedClient ? kOSBooleanTrue : kOSBooleanFalse);

    num = OSNumber::withNumber(fReportLimit, 32);
    if (num) {
        debugDict->setObject("MaxEnqueueReportSize", num);
        num->release();
    }

    if (fQueueMap) {
        debugDict->setObject("EventQueueMap", fQueueMap);
    }

    debugDict->setObject("ClientOpened", (fClientOpened ? kOSBooleanTrue : kOSBooleanFalse));

    result = debugDict->serialize(serializer);
    debugDict->release();
    
exit:
    return result;
}

void IOHIDLibUserClient::setStateForQueues(UInt32 state, IOOptionBits options __unused)
{
    HIDLibUserClientLogDebug("setStateForQueues: 0x%x", (unsigned int)state);

    for (u_int token = getNextTokenForToken(0); token != 0; token = getNextTokenForToken(token))
    {
        // this cannot return a NULL queue because of the above code
        IOHIDEventQueue *queue = getQueueForToken(token);
        if (!queue) {
            continue;
        }
        switch (state) {
            case kHIDQueueStateEnable:
                queue->enable();
                break;
            case kHIDQueueStateDisable:
                queue->disable();
                break;
            case kHIDQueueStateClear:
                fNub->stopEventDelivery(queue);
                break;
        }
    }
}

IOReturn IOHIDLibUserClient::clientMemoryForType (
                                    UInt32                    type,
                                    IOOptionBits *            options,
                                    IOMemoryDescriptor **    memory )
{
    if (fGate) {
        return fGate->runAction(OSMemberFunctionCast(IOCommandGate::Action,
                                                     this,
                                                     &IOHIDLibUserClient::clientMemoryForTypeGated),
                                (void*)(intptr_t)type,
                                (void *)options,
                                (void *)memory);
    }
    else {
        return kIOReturnOffline;
    }
}

IOReturn IOHIDLibUserClient::clientMemoryForTypeGated(UInt32 token,
                                                      IOOptionBits *options __unused,
                                                      IOMemoryDescriptor **memory)
{
    IOReturn ret = kIOReturnError;
    

    IOHIDEventQueue *queue = getQueueForToken(token);
    require_action(queue, exit, ret = kIOReturnBadArgument);

    *memory = queue->getMemoryDescriptor();
    
    if (*memory) {
        ret = kIOReturnSuccess;
    }
    
exit:
    return ret;
}


IOReturn IOHIDLibUserClient::_getElementCount(IOHIDLibUserClient * target, void * reference __unused, IOExternalMethodArguments * arguments)
{
    return target->getElementCount(&(arguments->scalarOutput[0]), &(arguments->scalarOutput[1]));
}

IOReturn IOHIDLibUserClient::getElementCount(uint64_t * pOutElementCount, uint64_t * pOutReportElementCount)
{
    uint32_t    outElementCount         = 0;
    uint32_t    outReportElementCount   = 0;
    IOReturn    ret                     = kIOReturnError;
    
    if (!pOutElementCount || !pOutReportElementCount)
        return kIOReturnBadArgument;
        
    ret = getElements(kHIDElementType, (void *)NULL, &outElementCount);
    if (ret != kIOReturnSuccess) {
        return ret;
    }
    ret = getElements(kHIDReportHandlerType, (void*)NULL, &outReportElementCount);
    if (ret != kIOReturnSuccess) {
        return ret;
    }
    
    *pOutElementCount        = outElementCount / sizeof(IOHIDElementStruct);
    *pOutReportElementCount    = outReportElementCount / sizeof(IOHIDElementStruct);

    return kIOReturnSuccess;
}

IOReturn IOHIDLibUserClient::_getElements(IOHIDLibUserClient * target, void * reference __unused, IOExternalMethodArguments * arguments)
{
    if ( arguments->structureOutputDescriptor )
        return target->getElements((uint32_t)arguments->scalarInput[0], arguments->structureOutputDescriptor, &(arguments->structureOutputDescriptorSize));
    else
        return target->getElements((uint32_t)arguments->scalarInput[0], arguments->structureOutput, &(arguments->structureOutputSize));
}

IOReturn IOHIDLibUserClient::getElements (uint32_t elementType, void *elementBuffer, uint32_t *elementBufferSize)
{
    OSArray *                array;
    uint32_t                i, bi, count;
    IOHIDElementPrivate *    element;
    IOHIDElementStruct *    elementStruct;
    uint32_t                bufferSize = 0;
    
    if (elementBuffer && elementBufferSize && !*elementBufferSize)
        return kIOReturnBadArgument;
            
    if (!fNub || isInactive())
        return kIOReturnNotAttached;

    if ( elementType == kHIDElementType )
        array = fNub->_reserved->hierarchElements;
    else
        array = fNub->_reserved->elementContainer->getInputReportElements();
    
    if ( !array )
        return kIOReturnError;

    count = array->getCount();
    
    if (elementBufferSize) {
        bufferSize = *elementBufferSize;
    }

    if ( elementBuffer && elementBufferSize)
        bzero(elementBuffer, bufferSize);
        
    bi = 0;
    
    for ( i=0; i<count; i++ )
    {
        element = OSDynamicCast(IOHIDElementPrivate, array->getObject(i));
        
        if (!element) continue;
       
        elementStruct = 0;

        do 
        {
            if ( !elementBuffer ) 
                break;

            if ( bi == 0xFFFFFFFF )
                break;

            if ( (bi + 1) > (0xFFFFFFFF / sizeof(IOHIDElementStruct)) )
                break;

            if ( (bi + 1) * sizeof(IOHIDElementStruct) <= bufferSize )
                elementStruct = &(((IOHIDElementStruct *)elementBuffer)[bi]);
        } while(0);
	
        if ( element->fillElementStruct(elementStruct) )
            bi++;
    }
    
    if (elementBufferSize)
        *elementBufferSize = bi * sizeof(IOHIDElementStruct);
        
    return kIOReturnSuccess;
}

IOReturn IOHIDLibUserClient::getElements(uint32_t elementType, IOMemoryDescriptor * mem, uint32_t *elementBufferSize)
{
    IOReturn ret = kIOReturnNoMemory;
        
    if (!fNub || isInactive())
        return kIOReturnNotAttached;

    ret = mem->prepare();
    
    if(ret == kIOReturnSuccess)
    {
        void *        elementData;
        uint32_t    elementLength;
        uint32_t    allocationSize;
        
        allocationSize = elementLength = (uint32_t)mem->getLength();
        if ( elementLength )
        {
            elementData = IOMalloc( elementLength );
            
            if ( elementData )
            {
                bzero(elementData, elementLength);

                ret = getElements(elementType, elementData, &elementLength);
                
                if ( elementBufferSize )
                    *elementBufferSize = elementLength;

                if (elementLength <= mem->getLength()) {
                    mem->writeBytes( 0, elementData, elementLength );
                } else {
                    ret = kIOReturnBadArgument;
                }

                IOFree( elementData, allocationSize );
            }
            else
                ret = kIOReturnNoMemory;
        }
        else
            ret = kIOReturnBadArgument;
            
        mem->complete();
    }

    if (ret != kIOReturnSuccess) {
        HIDLibUserClientLogError("getElements failed: 0x%x", (unsigned int)ret);
    }

    return ret;
}

IOReturn IOHIDLibUserClient::_createQueue(IOHIDLibUserClient * target, void * reference __unused, IOExternalMethodArguments * arguments)
{
    return target->createQueue((uint32_t)arguments->scalarInput[0], (uint32_t)arguments->scalarInput[1], &(arguments->scalarOutput[0]));
}

IOReturn IOHIDLibUserClient::createQueue(uint32_t flags, uint32_t depth, uint64_t * outQueue)
{
    OSNumber    *reportBufferCount      = NULL;
    OSNumber    *reportBufferEntrySize  = NULL;
    OSNumber    *maxReportSize          = NULL;
    UInt32      bufferCount             = 0;
    UInt32      bufferEntrySize         = 0;
    UInt32      reportSize              = 0;
    UInt32      numEntries              = 0;
    UInt32      entrySize               = 0;
    IOReturn    ret                     = kIOReturnError;
    IOHIDReportElementQueue *eventQueue         = NULL;
    
    reportBufferCount = OSDynamicCast(OSNumber, fNub->copyProperty(kIOHIDMaxReportBufferCountKey));
    if (reportBufferCount) {
        bufferCount = reportBufferCount->unsigned32BitValue();
        OSSafeReleaseNULL(reportBufferCount);
    }
    
    reportBufferEntrySize = OSDynamicCast(OSNumber, fNub->copyProperty(kIOHIDReportBufferEntrySizeKey));
    if (reportBufferEntrySize) {
        bufferEntrySize = reportBufferEntrySize->unsigned32BitValue();
        OSSafeReleaseNULL(reportBufferEntrySize);
    }
    
    maxReportSize = OSDynamicCast(OSNumber, fNub->copyProperty(kIOHIDMaxInputReportSizeKey));
    if (maxReportSize) {
        reportSize = maxReportSize->unsigned32BitValue();
        OSSafeReleaseNULL(maxReportSize);
    }
    
    // Number of entries is either provided by kIOHIDMaxReportBufferCountKey or
    // the passed in depth.
    numEntries = bufferCount ? bufferCount : depth;
    
    // Entry size is either kIOHIDReportBufferEntrySizeKey or the max input
    // report size. We add overhead equal to the size of IOHIDElementValue.
    entrySize  = bufferEntrySize ? bufferEntrySize : reportSize;
    
    // A client can request a queue size up to 128K. Anything beyond that will
    // require entitlements.
    UInt32 queueSize;
    if (os_mul_overflow(numEntries, entrySize, &queueSize)) {
        HIDLibUserClientLogError("Overflow in queueSize.");
        queueSize = HID_QUEUE_CAPACITY_MAX;
    }
    if (queueSize > HID_QUEUE_CAPACITY_MAX && !_customQueueSizeEntitlement) {
        char name[255];
        bzero(name, sizeof(name));
        proc_name(fPid, name, sizeof(name));
        HIDLibUserClientLogError("%s is requesting a queue size %d (%d, %d) but is not entitled.",
                    name,
                    (int) (entrySize * numEntries),
                    (int) entrySize,
                    (int) numEntries);
        eventQueue = IOHIDReportElementQueue::withCapacity(HID_QUEUE_CAPACITY_MAX, this);
    } else {
        entrySize += HID_QUEUE_HEADER_SIZE;
        if (entrySize >= fReportLimit) {
            // Reports will be out of band, limit capacity since kernel will be allocation the buffers.
            eventQueue = IOHIDReportElementQueue::withCapacity(HID_QUEUE_CAPACITY_MIN, this);
        } else {
            eventQueue = IOHIDReportElementQueue::withCapacity(queueSize, this);
        }
    }
    
    require_action(eventQueue, exit, ret = kIOReturnNoMemory);
    
    eventQueue->setOptions(flags);
    
    if (!fValid) {
        eventQueue->disable();
    }
    
    // add the queue to the map and set out queue
    *outQueue = (uint64_t)createTokenForQueue(eventQueue);
    eventQueue->release();
    
    ret = kIOReturnSuccess;
    
exit:
    if (ret != kIOReturnSuccess) {
        HIDLibUserClientLogError("createQueue failed: 0x%x", (unsigned int)ret);
    }

    return ret;
}


IOReturn IOHIDLibUserClient::_disposeQueue(IOHIDLibUserClient * target, void * reference __unused, IOExternalMethodArguments * arguments)
{
    return target->disposeQueue(target->getQueueForToken((u_int)arguments->scalarInput[0]));
}

IOReturn IOHIDLibUserClient::disposeQueue(IOHIDEventQueue * queue)
{
    IOReturn ret = kIOReturnSuccess;

    if (!queue) {
        return kIOReturnBadArgument;
    }
    
    // remove this queue from all elements that use it
    if (fNub && !isInactive())
        ret = fNub->stopEventDelivery (queue);

    // remove the queue from the map
    removeQueueFromMap(queue);

    if (ret != kIOReturnSuccess) {
        HIDLibUserClientLogError("disposeQueue failed: 0x%x", (unsigned int)ret);
    }

    // This should really return an actual result
    return ret;
}

    // Add an element to a queue
IOReturn IOHIDLibUserClient::_addElementToQueue(IOHIDLibUserClient * target, void * reference __unused, IOExternalMethodArguments * arguments)
{
    return target->addElementToQueue(target->getQueueForToken((u_int)arguments->scalarInput[0]), (IOHIDElementCookie)arguments->scalarInput[1], (uint32_t)arguments->scalarInput[2], &(arguments->scalarOutput[0]));
}

IOReturn IOHIDLibUserClient::addElementToQueue(IOHIDEventQueue * queue, IOHIDElementCookie elementCookie, uint32_t flags __unused, uint64_t *pSizeChange __unused)
{
    IOReturn ret = kIOReturnError;
    
    require_action(fNub && !isInactive(), exit, ret = kIOReturnOffline);
    
    // add the queue to the element's queues
    ret = fNub->startEventDelivery(queue, elementCookie);
    
exit:
    if (ret != kIOReturnSuccess) {
        HIDLibUserClientLogError("addElementToQueue failed: 0x%x", (unsigned int)ret);
    }

    return ret;
}
    // remove an element from a queue
IOReturn IOHIDLibUserClient::_removeElementFromQueue (IOHIDLibUserClient * target, void * reference __unused, IOExternalMethodArguments * arguments)
{
    return target->removeElementFromQueue(target->getQueueForToken((u_int)arguments->scalarInput[0]), (IOHIDElementCookie)arguments->scalarInput[1], &(arguments->scalarOutput[0]));
}

IOReturn IOHIDLibUserClient::removeElementFromQueue (IOHIDEventQueue * queue, IOHIDElementCookie elementCookie, uint64_t *pSizeChange __unused)
{
    IOReturn ret = kIOReturnError;
    
    require_action(fNub && !isInactive(), exit, ret = kIOReturnOffline);
    
    // remove the queue from the element's queues
    ret = fNub->stopEventDelivery(queue, elementCookie);
    
exit:
    if (ret != kIOReturnSuccess) {
        HIDLibUserClientLogError("removeElementFromQueue failed: 0x%x", (unsigned int)ret);
    }

    return ret;
}
    // Check to see if a queue has an element
IOReturn IOHIDLibUserClient::_queueHasElement (IOHIDLibUserClient * target, void * reference __unused, IOExternalMethodArguments * arguments)
{
    return target->queueHasElement(target->getQueueForToken((u_int)arguments->scalarInput[0]), (IOHIDElementCookie)arguments->scalarInput[1], &(arguments->scalarOutput[0]));
}

IOReturn IOHIDLibUserClient::queueHasElement (IOHIDEventQueue * queue, IOHIDElementCookie elementCookie, uint64_t * pHasElement)
{
    IOReturn ret = kIOReturnSuccess;

    // check to see if that element is feeding that queue
    bool hasElement = false;
    
    if (fNub && !isInactive())
        ret = fNub->checkEventDelivery (queue, elementCookie, &hasElement);
    
    // set return
    *pHasElement = hasElement;
    
    return ret;
}
    // start a queue
IOReturn IOHIDLibUserClient::_startQueue (IOHIDLibUserClient * target, void * reference __unused, IOExternalMethodArguments * arguments)
{
    return target->startQueue(target->getQueueForToken((u_int)arguments->scalarInput[0]));
}

IOReturn IOHIDLibUserClient::startQueue (IOHIDEventQueue * queue)
{
    HIDLibUserClientLogDebug("startQueue");

    // start the queue
    if (queue) {
        queue->start();
        return kIOReturnSuccess;
    }

    return kIOReturnBadArgument;
}

    // stop a queue
IOReturn IOHIDLibUserClient::_stopQueue (IOHIDLibUserClient * target, void * reference __unused, IOExternalMethodArguments * arguments)
{
    return target->stopQueue(target->getQueueForToken((u_int)arguments->scalarInput[0]));
}

IOReturn IOHIDLibUserClient::stopQueue (IOHIDEventQueue * queue)
{
    HIDLibUserClientLogDebug("stopQueue");

    // stop the queue
    if (queue) {
        queue->stop();
        return kIOReturnSuccess;
    }
    return kIOReturnBadArgument;
}

    // update the feature element value
IOReturn IOHIDLibUserClient::_updateElementValues (IOHIDLibUserClient * target, void * reference __unused, IOExternalMethodArguments * arguments)
{
    if (arguments->structureOutputDescriptor) {
        return target->updateElementValues((const IOHIDElementCookie*)arguments->structureInput,
                                           (uint32_t)arguments->structureInputSize,
                                           arguments->structureOutputDescriptor,
                                           arguments->structureOutputDescriptorSize,
                                           (IOOptionBits)arguments->scalarInput[0]);
    } else {
        return target->updateElementValues((const IOHIDElementCookie*)arguments->structureInput,
                                           (uint32_t)arguments->structureInputSize,
                                           arguments->structureOutput,
                                           arguments->structureOutputSize,
                                           (IOOptionBits)arguments->scalarInput[0]);
    }
}

IOReturn IOHIDLibUserClient::updateElementValues (const IOHIDElementCookie * lCookies,
                                                  uint32_t cookieSize,
                                                  IOMemoryDescriptor *outputElementsDesc,
                                                  uint32_t outputElementsDescSize,
                                                  IOOptionBits options)
{
    IOReturn    ret = kIOReturnError;
    uint8_t     *elementData;
    IOMemoryMap *mapping = NULL;
    uint32_t    elementLength;

    require_action(fClientOpened, exit, ret = kIOReturnNotOpen;  HIDLibUserClientLogError("Client not opened"););
    require_action(fValid, exit, ret = kIOReturnNotPermitted; HIDLibUserClientLogError("Client not Permitted"););

    elementLength = (uint32_t)outputElementsDesc->getLength();
    if (elementLength) {
        mapping = outputElementsDesc->map();
        require_action(mapping, exit, ret=kIOReturnNoMemory);
        elementData = reinterpret_cast<uint8_t*>(mapping->getVirtualAddress());

        if (elementData) {
            ret = updateElementValues(lCookies, cookieSize, elementData, elementLength, options);
        } else {
            ret = kIOReturnNoMemory;
        }
    } else {
        ret = kIOReturnBadArgument;
    }

exit:
    OSSafeReleaseNULL(mapping);

    if (ret != kIOReturnSuccess) {
        HIDLibUserClientLogError("updateElementValues failed: 0x%x", (unsigned int)ret);
    }
    return ret;
}

IOReturn IOHIDLibUserClient::updateElementValues (const IOHIDElementCookie *lCookies,
                                                  uint32_t cookieSize,
                                                  void *outputElements,
                                                  uint32_t outputElementsSize,
                                                  IOOptionBits options)
{
    IOReturn    ret = kIOReturnError;
    uint32_t cookieCount = 0;
    
    require_action(lCookies, exit, ret =kIOReturnBadArgument);
    require_action(cookieSize, exit, ret =kIOReturnBadArgument);
    
    require_action((cookieSize % sizeof(uint32_t)) == 0, exit, ret =kIOReturnBadArgument);
    cookieCount = (uint32_t)(cookieSize / sizeof(uint32_t));
    
    require_action(fClientOpened, exit, ret = kIOReturnNotOpen);
    require_action(fValid, exit, ret = kIOReturnNotPermitted);
    
    if (fNub && !isInactive()) {
        OSArray    *elements = fNub->_elementArray;
        uint32_t   dataOffset = 0;
        const uint32_t *cookies = (const uint32_t *)lCookies;

        if (!(options & kIOHIDElementPreventPoll)) {
            ret = fNub->updateElementValues((IOHIDElementCookie *)lCookies, cookieCount);
        } else {
            ret = kIOReturnSuccess;
        }

        // Copy the values back to Userland data structs.
        for (uint32_t index = 0; index < cookieCount; ++index) {
            IOHIDElementPrivate *element = (IOHIDElementPrivate *) elements->getObject(cookies[index]);
            IOHIDElementValue *elementVal = element->_elementValue;
            // Align to dword, to make unpacking easier
            uint32_t enqueueSize = elementVal->totalSize;
            uint32_t neededSize;
            // Ensure we have space for the IOHIDElementValue.
            if (os_add_overflow(dataOffset, elementVal->totalSize, &neededSize)) {
                // space overflow
                HIDLibUserClientLogError("Overflow calculating the space needed for Element Value.");
                ret = kIOReturnNoMemory;
                break;
            }
            if (neededSize > outputElementsSize) {
                HIDLibUserClientLogError("No space for remaining Element Values. Need: %d Available: %d",
                                         neededSize,
                                         outputElementsSize);
                ret = kIOReturnBadArgument;
                break;
            }

            memcpy((uint8_t*)outputElements + dataOffset, elementVal, elementVal->totalSize);
            dataOffset += enqueueSize;
        }
    }
    
exit:
    if (ret != kIOReturnSuccess) {
        HIDLibUserClientLogError("updateElementValues failed: 0x%x", (unsigned int)ret);
    }

    return ret;
}

    // Set the element values
IOReturn IOHIDLibUserClient::_postElementValues (IOHIDLibUserClient * target, void * reference __unused, IOExternalMethodArguments * arguments)
{

    if (arguments->structureInputDescriptor) {
        return target->postElementValues(arguments->structureInputDescriptor);
    } else {
        return target->postElementValues((const uint8_t*)arguments->structureInput,
                                         arguments->structureInputSize);
    }
}

IOReturn IOHIDLibUserClient::postElementValues(IOMemoryDescriptor *desc)
{
    IOReturn    ret = kIOReturnError;
    uint8_t     *elementData;
    IOMemoryMap *mapping = NULL;
    uint32_t    elementLength;

    require_action(fClientOpened, exit, ret = kIOReturnNotOpen; HIDLibUserClientLogError("Client not opened"););
    require_action(fValid, exit, ret = kIOReturnNotPermitted; HIDLibUserClientLogError("Client not permitted"););

    elementLength = (uint32_t)desc->getLength();
    if (elementLength) {
        mapping = desc->map();
        require(mapping, exit);
        elementData = reinterpret_cast<uint8_t*>(mapping->getVirtualAddress());

        if (elementData) {
            ret = postElementValues(elementData, elementLength);
        } else {
            ret = kIOReturnNoMemory;
        }
    } else {
        ret = kIOReturnBadArgument;
    }

exit:
    OSSafeReleaseNULL(mapping);

    if (ret != kIOReturnSuccess) {
        HIDLibUserClientLogError("postElementValues failed: 0x%x", (unsigned int)ret);
    }
    return ret;
}

IOReturn IOHIDLibUserClient::postElementValues (const uint8_t * data, uint32_t dataSize)
{
    IOReturn    ret = kIOReturnError;

    require_action(data, exit, ret =kIOReturnBadArgument);
    require_action(dataSize, exit, ret =kIOReturnBadArgument);

    require_action(fClientOpened, exit, ret = kIOReturnNotOpen);
    require_action(fValid, exit, ret = kIOReturnNotPermitted);
    
    if (fNub && !isInactive()) {
        ret = fNub->postElementTransaction(data, dataSize);
    }
    
exit:
    if (ret != kIOReturnSuccess) {
        HIDLibUserClientLogError("postElementValues failed: 0x%x", (unsigned int)ret);
    }

    return ret;
}

IOReturn IOHIDLibUserClient::_getReport(IOHIDLibUserClient * target, void * reference __unused, IOExternalMethodArguments * arguments)
{
    
    OSData *asyncData = NULL;

    if ( arguments->asyncWakePort ) {
        IOReturn        ret;
        AsyncParam pb;
        
        target->retain();
        
        bcopy(arguments->asyncReference, pb.fAsyncRef, sizeof(OSAsyncReference64));
        pb.fAsyncCount = arguments->asyncReferenceCount;
        pb.fCompletion.target = target;
        pb.fCompletion.action = OSMemberFunctionCast(IOHIDCompletionAction, target, &IOHIDLibUserClient::ReqComplete);

        
        asyncData = OSData::withBytes(&pb, sizeof(AsyncParam));
        if(!asyncData) {
            target->release();
            return kIOReturnNoMemory;
        }
        
        // save reference of itself to release in report completion
        ((AsyncParam*)asyncData->getBytesNoCopy())->fCompletion.parameter = asyncData;
        
        target->_pending->setObject(asyncData);
        
        if ( arguments->structureOutputDescriptor ) {
            IOBufferMemoryDescriptor *  mem;
            
            mem = IOBufferMemoryDescriptor::withCapacity(arguments->structureOutputDescriptorSize, kIODirectionIn);
            if(mem) {
                ret = target->getReport(mem, &(arguments->structureOutputDescriptorSize), (IOHIDReportType)arguments->scalarInput[0], (uint32_t)arguments->scalarInput[1], (uint32_t)arguments->scalarInput[2], &(((AsyncParam*)asyncData->getBytesNoCopy())->fCompletion));
                if (ret == kIOReturnSuccess) {
                    ret = arguments->structureOutputDescriptor->prepare();
                    if (ret == kIOReturnSuccess) {
                        arguments->structureOutputDescriptor->writeBytes(0, mem->getBytesNoCopy(), arguments->structureOutputDescriptorSize);
                        arguments->structureOutputDescriptor->complete();
                    }
                }
                mem->release();
            }
            else {
                ret = kIOReturnNoMemory;
            }
        }
        else {
            ret = target->getReport(arguments->structureOutput, &(arguments->structureOutputSize), (IOHIDReportType)arguments->scalarInput[0], (uint32_t)arguments->scalarInput[1], (uint32_t)arguments->scalarInput[2], &(((AsyncParam*)asyncData->getBytesNoCopy())->fCompletion));
        }
            
        if ( ret ) {
            if (asyncData) {
                target->_pending->removeObject(asyncData);
                asyncData->release();
            }
            target->release();
        }
        return ret;
    }
    if ( arguments->structureOutputDescriptor ) {

        IOReturn ret;
        IOBufferMemoryDescriptor *  mem;

        mem = IOBufferMemoryDescriptor::withCapacity(arguments->structureOutputDescriptorSize, kIODirectionIn);
        if(mem) {
            ret = target->getReport(mem, &(arguments->structureOutputDescriptorSize), (IOHIDReportType)arguments->scalarInput[0], (uint32_t)arguments->scalarInput[1]);
            if (ret == kIOReturnSuccess) {
                ret = arguments->structureOutputDescriptor->prepare();
                if (ret == kIOReturnSuccess) {
                    arguments->structureOutputDescriptor->writeBytes(0, mem->getBytesNoCopy(), arguments->structureOutputDescriptorSize);
                    arguments->structureOutputDescriptor->complete();
                }
            }
            mem->release();
        }
        else {
            ret = kIOReturnNoMemory;
        }
        
        return ret;
    }
    else {
        return target->getReport(arguments->structureOutput, &(arguments->structureOutputSize), (IOHIDReportType)arguments->scalarInput[0], (uint32_t)arguments->scalarInput[1]);
    }
}

IOReturn IOHIDLibUserClient::getReport(void *reportBuffer, uint32_t *pOutsize, IOHIDReportType reportType, uint32_t reportID, uint32_t timeout, IOHIDCompletion * completion)
{
    IOReturn                ret = kIOReturnBadArgument;
    IOMemoryDescriptor *    mem;
    
    // VTN3: Is there a real maximum report size? It looks like the current limit is around
    // 1024 bytes, but that will (or has) changed. 65536 is above every upper limit
    // I have seen by a few factors.
    if (*pOutsize > 0x10000) {
        HIDLibUserClientLogError("called with an irrationally large output size: %lu", (long unsigned) *pOutsize);
    }
    else {
        mem = IOBufferMemoryDescriptor::withCapacity(*pOutsize, kIODirectionInOut);
        if(mem) {
            mem->prepare(kIODirectionInOut); 
            ret = getReport(mem, pOutsize, reportType, reportID, timeout, completion);
            mem->readBytes(0, reportBuffer, *pOutsize);
            mem->complete();
            mem->release();
        }
        else {
            ret =  kIOReturnNoMemory;
        }
    }

    if (ret != kIOReturnSuccess) {
        HIDLibUserClientLogError("getReport failed: 0x%x", (unsigned int)ret);
    }

    return ret;
}

IOReturn IOHIDLibUserClient::getReport(IOMemoryDescriptor * mem, uint32_t * pOutsize, IOHIDReportType reportType, uint32_t reportID, uint32_t timeout, IOHIDCompletion * completion)
{
    IOReturn ret = kIOReturnBadArgument;
    
    require_action(fClientOpened, exit, ret = kIOReturnNotOpen; HIDLibUserClientLogError("Client not opened"););
    require_action(fValid, exit, ret = kIOReturnNotPermitted; HIDLibUserClientLogError("Client not permitted"););
    
    // VTN3: Is there a real maximum report size? It looks like the current limit is around
    // 1024 bytes, but that will (or has) changed. 65536 is above every upper limit
    // I have seen by a few factors.
    if (*pOutsize > 0x10000) {
        HIDLibUserClientLogError("called with an irrationally large output size: %lu", (long unsigned) *pOutsize);
    }
    else if (fNub && !isInactive()) {
        ret = mem->prepare();
        if(ret == kIOReturnSuccess) {
            if (completion) {
                AsyncParam * pb = (AsyncParam*)((OSData*)completion->parameter)->getBytesNoCopy();
                pb->fMax        = *pOutsize;
                pb->fMem        = mem;
                pb->reportType  = reportType;

                mem->retain();
                
                ret = fNub->getReport(mem, reportType, reportID, timeout, completion);
                if (ret != kIOReturnSuccess) {
                    mem->complete();
                    mem->release();
                }
            }
            else {
                ret = fNub->getReport(mem, reportType, reportID);

                // make sure the element values are updated.
                if (ret == kIOReturnSuccess)
                    fNub->handleReport(mem, reportType, kIOHIDReportOptionNotInterrupt);
                    
                *pOutsize = (uint32_t)mem->getLength();
                mem->complete();
            }
        }
    }
    else {
        ret = kIOReturnNotAttached;
    }

exit:
    return ret;
}

IOReturn IOHIDLibUserClient::_setReport(IOHIDLibUserClient * target, void * reference __unused, IOExternalMethodArguments * arguments)
{
    IOReturn ret = kIOReturnError;
    OSData*  asyncData = NULL;

    if ( arguments->asyncWakePort ) {
        
        AsyncParam pb;
        target->retain();
        
        bcopy(arguments->asyncReference, pb.fAsyncRef, sizeof(OSAsyncReference64));
        pb.fAsyncCount = arguments->asyncReferenceCount;
        pb.fCompletion.target = target;
        pb.fCompletion.action = OSMemberFunctionCast(IOHIDCompletionAction, target, &IOHIDLibUserClient::ReqComplete);
        
        asyncData = OSData::withBytes(&pb, sizeof(AsyncParam));
        if (!asyncData) {
            target->release();
            return kIOReturnNoMemory;
        }
        
        // save reference of itself to release in report completion
        ((AsyncParam*)asyncData->getBytesNoCopy())->fCompletion.parameter = asyncData;
        target->_pending->setObject(asyncData);
        
        if ( arguments->structureInputDescriptor )
            ret = target->setReport( arguments->structureInputDescriptor, (IOHIDReportType)arguments->scalarInput[0], (uint32_t)arguments->scalarInput[1],(uint32_t)arguments->scalarInput[2], &(((AsyncParam*)asyncData->getBytesNoCopy())->fCompletion));
        else
            ret = target->setReport(arguments->structureInput, arguments->structureInputSize, (IOHIDReportType)arguments->scalarInput[0], (uint32_t)arguments->scalarInput[1], (uint32_t)arguments->scalarInput[2], &(((AsyncParam*)asyncData->getBytesNoCopy())->fCompletion));
            
        if ( ret ) {
            if (asyncData) {
                target->_pending->removeObject(asyncData);
                asyncData->release();
            }
            
            target->release();
        }
    }
    else
        if ( arguments->structureInputDescriptor )
            ret = target->setReport( arguments->structureInputDescriptor, (IOHIDReportType)arguments->scalarInput[0], (uint32_t)arguments->scalarInput[1]);
        else
            ret = target->setReport(arguments->structureInput, arguments->structureInputSize, (IOHIDReportType)arguments->scalarInput[0], (uint32_t)arguments->scalarInput[1]);
            
    return ret;
}

IOReturn IOHIDLibUserClient::setReport(const void *reportBuffer, uint32_t reportBufferSize, IOHIDReportType reportType, uint32_t reportID, uint32_t timeout, IOHIDCompletion * completion)
{
    IOReturn                ret = kIOReturnNoMemory;
    IOMemoryDescriptor *    mem;
    
    mem = IOMemoryDescriptor::withAddress((void *)reportBuffer, reportBufferSize, kIODirectionOut);
    if(mem) {
        ret = setReport(mem, reportType, reportID, timeout, completion);
        mem->release();
    }

    if (ret != kIOReturnSuccess) {
        HIDLibUserClientLogError("setReport failed: 0x%x", (unsigned int)ret);
    }

    return ret;
}

IOReturn IOHIDLibUserClient::setReport(IOMemoryDescriptor * mem, IOHIDReportType reportType, uint32_t reportID, uint32_t timeout, IOHIDCompletion * completion)
{
    IOReturn            ret;
    
    require_action(fClientOpened, exit, ret = kIOReturnNotOpen; HIDLibUserClientLogError("Client not opened"););
    require_action(fValid, exit, ret = kIOReturnNotPermitted; HIDLibUserClientLogError("Client not permitted"););

    if (fNub && !isInactive()) {
        ret = mem->prepare();
        if(ret == kIOReturnSuccess) {
            OSObject *obj = copyProperty(kIOHIDExtendedDataKey);
            OSArray *extended = OSDynamicCast(OSArray, obj);
            if (extended && extended->getCount()) {
                OSCollectionIterator *itr = OSCollectionIterator::withCollection(extended);
                if (itr) {
                    bool done = false;
                    while (!done) {
                        OSObject *objItr;
                        while (!done && (NULL != (objItr = itr->getNextObject()))) {
                            OSNumber *num = OSDynamicCast(OSNumber, objItr);
                            if (num) {
                                uint8_t excludedReportID =  (num->unsigned64BitValue() >> 16) & 0xff;
                                uint8_t excludedReportType =(num->unsigned64BitValue() >> 24) & 0xff;

                                if ((excludedReportID == reportID) && (excludedReportType == (reportType + 1))) {
                                     // Block
                                    HIDLibUserClientLogError("%02x/%02x blocked due to lack of privileges", reportID, reportType);
                                    done = true;
                                    ret = kIOReturnNotPrivileged;
                                }
                            }
                        }
                        if (itr->isValid()) {
                            // Do not block
                            done = true;
                        }
                        else {
                            // Someone changed the array. Check again.
                            itr->reset();
                        }
                    }
                    itr->release();
                }
            }
            OSSafeReleaseNULL(obj);
            if (ret == kIOReturnSuccess) {
                if ( completion ) {
                    AsyncParam * pb = (AsyncParam*)((OSData*)completion->parameter)->getBytesNoCopy();
                    pb->fMax        = (UInt32)mem->getLength();
                    pb->fMem        = mem;
                    pb->reportType    = reportType;

                    mem->retain();

                    ret = fNub->setReport(mem, reportType, reportID, timeout, completion);
                    if (ret != kIOReturnSuccess) {
                        mem->complete();
                        mem->release();
                    }
                }
                else {
                    ret = fNub->setReport(mem, reportType, reportID);

                    // make sure the element values are updated.
                    if (ret == kIOReturnSuccess)
                        fNub->handleReport(mem, reportType, kIOHIDReportOptionNotInterrupt);

                    mem->complete();
                }
            }
        }
    }
    else
        ret = kIOReturnNotAttached;

exit:
    return ret;
}

void IOHIDLibUserClient::ReqComplete(void *param, IOReturn res, UInt32 remaining)
{
    fGate->runAction(OSMemberFunctionCast(IOCommandGate::Action,
                                          this,
                                          &IOHIDLibUserClient::ReqCompleteGated),
                     param,
                     (void*)(intptr_t)res,
                     (void*)(intptr_t)remaining);
}

IOReturn IOHIDLibUserClient::ReqCompleteGated(void *param, IOReturn res, UInt32 remaining)
{
    io_user_reference_t args[1];
    OSData* asyncData =  (OSData*)param;
    AsyncParam * pb = NULL;
    
    // Do we care about return type of handler
    // This is no way propogated to user
    
    require(asyncData, exit);
    
    pb = (AsyncParam*)asyncData->getBytesNoCopy();
    
    if(res == kIOReturnSuccess) {
        args[0] = (io_user_reference_t)(pb->fMax - remaining);
        
        // make sure the element values are updated.
        if (fNub && !isInactive())
            fNub->handleReport(pb->fMem, pb->reportType, kIOHIDReportOptionNotInterrupt);
    } else {
        args[0] = 0;
    }
    
    if (pb->fMem) {
        pb->fMem->complete();
        pb->fMem->release();
    }

    sendAsyncResult64(pb->fAsyncRef, res, args, 1);

    _pending->removeObject(asyncData);
    
    asyncData->release();
    
exit:
    // wake thread that might be sleeping on termination path
    // since we don't have any pending terminations we must wake
    if (_pending->getCount() == 0) {
        fGate->commandWakeup(&_pending);
    }
    
    release();
    
    return kIOReturnSuccess;
}


// This section is to track all user queues and hand out unique tokens for
// particular queues. vtn3
// rdar://5957582 start

enum { kIOHIDLibUserClientQueueTokenOffset = 200 };


u_int IOHIDLibUserClient::createTokenForQueue(IOHIDEventQueue *queue)
{
    u_int index = 0;
    u_int result = 0;

    while (OSDynamicCast(IOHIDEventQueue, fQueueMap->getObject(index)))
        index++;
    
    if (index < (UINT_MAX - kIOHIDLibUserClientQueueTokenOffset)) {
    	fQueueMap->setObject(index, queue);
    	result = index + kIOHIDLibUserClientQueueTokenOffset;
    }
    else {
		HIDLibUserClientLogError("generated out-of-range index: %d", index);
    }

    return (result);
}


void IOHIDLibUserClient::removeQueueFromMap(IOHIDEventQueue *queue)
{
    OSObject *obj = NULL;

    for (u_int index = 0; NULL != (obj = fQueueMap->getObject(index)); index++)
        if (obj == queue) {
            fQueueMap->replaceObject(index, kOSBooleanFalse);
        }
}


IOHIDEventQueue* IOHIDLibUserClient::getQueueForToken(u_int token)
{
	IOHIDEventQueue	*result = NULL;
	
	if (token >= kIOHIDLibUserClientQueueTokenOffset) {
		result = OSDynamicCast(IOHIDEventQueue, fQueueMap->getObject(token - kIOHIDLibUserClientQueueTokenOffset));
	}
	else {
		HIDLibUserClientLogError("received out-of-range token: %d", token);
	}

	return (result);
}


u_int IOHIDLibUserClient::getNextTokenForToken(u_int token)
{
    u_int next_token = (token < kIOHIDLibUserClientQueueTokenOffset) ? 
                                kIOHIDLibUserClientQueueTokenOffset - 1 : token;
    
    IOHIDEventQueue *queue = NULL;

    do {
        queue = getQueueForToken(++next_token);
    }
    while ((next_token < fQueueMap->getCount() + kIOHIDLibUserClientQueueTokenOffset) && (queue == NULL));
    
    if (next_token >= fQueueMap->getCount() + kIOHIDLibUserClientQueueTokenOffset)
        next_token = 0;
    
    return next_token;
}

// rdar://5957582 end

bool
IOHIDLibUserClient::attach(IOService * provider)
{
    if (!super::attach(provider)) {
        return false;
    }
    if (provider && getProperty(kIOHIDExtendedDataKey)) {
        // Check for extended data
        OSObject *obj = provider->copyProperty(kIOHIDExtendedDataKey, gIOServicePlane);
        OSArray *extended = OSDynamicCast(OSArray, obj);
        if (OSDynamicCast(OSArray, extended) && extended->getCount()) {
            // Extended data found. Replace the temporary key.
            setProperty(kIOHIDExtendedDataKey, extended);
        }
        else {
            // No extended data found. Remove the temporary key.
            removeProperty(kIOHIDExtendedDataKey);
        }
        OSSafeReleaseNULL(obj);
    }
    return true;
}

IOReturn
IOHIDLibUserClient::processElement(IOHIDElementValue *element, IOHIDEventQueue *queue)
{
    IOReturn status = kIOReturnNoMemory;
    bool ret;
    mach_vm_address_t reportAddress;
    UInt32 dataSize;

    UInt32 reportSize = ELEMENT_VALUE_REPORT_SIZE(element);
    if (reportSize < fReportLimit) {
        // Align to dword
        uint32_t enqueueSize = ALIGN_DATA_SIZE(element->totalSize);
        ret = queue->enqueue((void*)element, enqueueSize);
        return ret ? kIOReturnSuccess : kIOReturnNoMemory;
    }

    IOHIDOOBReportDescriptor *md = IOHIDOOBReportDescriptor::inTaskWithBytes(fClient, element->value, reportSize, kIODirectionInOut);
    require_action(md, exit, ret = kIOReturnNoMemory;
                   HIDLibUserClientLogError("Unable to create large report descriptor. Dropping large report."));

    reportAddress = (mach_vm_address_t)md->getBytesNoCopy();
    require_action(reportAddress, exit, ret = kIOReturnNoMemory;
                   HIDLibUserClientLogError("Unable to create large report map in user task. Dropping large report."));

    queue_enter(&fOOBReports, md, IOHIDOOBReportDescriptor *, qc);
    md->retain();

    bcopy(&reportAddress, &element->value[0], sizeof(reportAddress));
    element->flags = kIOHIDElementValueOOBReport;
    dataSize = ELEMENT_VALUE_HEADER_SIZE(element) + sizeof(reportAddress);
    dataSize = ALIGN_DATA_SIZE(dataSize);
    ret = queue->enqueue((void*)element, dataSize);
    if (!ret) {
        HIDLibUserClientLogError("Failed to enqueue oversized report.");
        releaseReport(reportAddress);
    } else {
        status = kIOReturnSuccess;
    }
    // Unset OOB flag, since next element may not be OOB if element is variable sized.
    element->flags &= ~kIOHIDElementValueOOBReport;

exit:
    OSSafeReleaseNULL(md);

    return status;
}

void
IOHIDLibUserClient::_releaseReport(IOHIDLibUserClient *target,
                                   void *reference,
                                   IOExternalMethodArguments *arguments)
{
    target->releaseReport((mach_vm_address_t)arguments->scalarInput[0]);
}

void
IOHIDLibUserClient::releaseReport(mach_vm_address_t reportToken)
{
    IOHIDOOBReportDescriptor* md = nullptr;
    IOHIDOOBReportDescriptor* tmp;

    queue_iterate(&fOOBReports, tmp, IOHIDOOBReportDescriptor *, qc) {
        if ((mach_vm_address_t)tmp->getBytesNoCopy() == reportToken) {
            md = tmp;
            break;
        }
    }

    if (md) {
        queue_remove(&fOOBReports, md, IOHIDOOBReportDescriptor *, qc);
        OSSafeReleaseNULL(md);
    } else {
        HIDLibUserClientLogError("Unable to find report descriptor for address %#llx", reportToken);
    }
}
