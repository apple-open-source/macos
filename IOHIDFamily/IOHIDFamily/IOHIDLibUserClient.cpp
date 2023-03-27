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
#include <libkern/c++/OSBoundedArrayRef.h>
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
#define kIOHIDManagerUserAccessInterfaceRematchEntitlement  "com.apple.hid.manager.user-access-interface-rematch"

#define HIDLibUserClientLogFault(fmt, ...)   HIDLogFault("%s:0x%llx " fmt "\n", getName(), getRegistryEntryID(), ##__VA_ARGS__)
#define HIDLibUserClientLogError(fmt, ...)   HIDLogError("%s:0x%llx " fmt "\n", getName(), getRegistryEntryID(), ##__VA_ARGS__)
#define HIDLibUserClientLog(fmt, ...)        HIDLog("%s:0x%llx " fmt "\n", getName(), getRegistryEntryID(), ##__VA_ARGS__)
#define HIDLibUserClientLogTest(fmt, ...)        HIDLog(" " fmt "\n",  ##__VA_ARGS__)
#define HIDLibUserClientLogInfo(fmt, ...)    HIDLogInfo("%s:0x%llx " fmt "\n", getName(), getRegistryEntryID(), ##__VA_ARGS__)
#define HIDLibUserClientLogDebug(fmt, ...)   HIDLogDebug("%s:0x%llx " fmt "\n", getName(), getRegistryEntryID(), ##__VA_ARGS__)

#define super IOUserClient2022
#define HID_MAX_ELEMENT_SIZE 131072 // 128KB

enum {
    kHIDLibUserClientNotEntitled                    = 0x00,
    kHIDLibUserClientKeyboardAccessEntitlement      = 0x01,
    kHIDLibUserClientUserAccessPrivelegeEntitlement = 0x02,
    kHIDLibUserClientCustomQueueSizeEntitlement     = 0x04,
    kHIDLibUserClientUserAccessProtectedEntitlement = 0x08,
    kHIDLibUserClientAdministratorPrivilege         = 0x10,
    kHIDLibUserClientInterfaceRematchEntitlement    = 0x20,
};

struct AsyncCommitParam {
    OSAsyncReference64         fAsyncRef;
    IOHIDCompletion            fCompletion;
    IOBufferMemoryDescriptor * elementData;
};

struct AsyncReportParam {
    OSAsyncReference64   fAsyncRef;
    IOMemoryDescriptor * fMem;
    IOHIDReportType      reportType;
    IOHIDCompletion      fCompletion;
};

struct AsyncGateParam {
    OSAsyncReference asyncRef;
    IOHIDReportType  reportType;
    UInt32           reportID;
    void *           reportBuffer;
    UInt32           reportBufferSize;
    UInt32           completionTimeOutMS;
};

typedef struct _IOHIDElementOOBValue
{
    IOHIDElementCookie cookie;
    uint32_t           flags:8;
    uint32_t           totalSize:24;
    uint64_t           timestamp;
    uint32_t           generation;
    mach_vm_address_t  address;
} __attribute__((packed)) IOHIDElementOOBValue;

typedef struct _IOHIDBlockedReport
{
    queue_chain_t qc;
} IOHIDBlockedReport;

OSDefineMetaClassAndStructors(IOHIDOOBReportDescriptor, IOBufferMemoryDescriptor);

IOHIDOOBReportDescriptor * IOHIDOOBReportDescriptor::inTaskWithBytes(
    task_t       inTask,
    const void * inBytes,
    vm_size_t    inLength,
    IODirection  inDirection,
    bool         inContiguous)
{
    IOHIDOOBReportDescriptor * me = new IOHIDOOBReportDescriptor;
    IOOptionBits options = inDirection | (inContiguous ? kIOMemoryPhysicallyContiguous : 0) | (inTask == kernel_task ? kIOMemoryKernelUserShared : kIOMemoryPageable | kIOMemoryPurgeable) | kIOMemoryThreadSafe;

    require_action(me && me->initWithPhysicalMask(inTask, options, inLength, PAGE_SIZE, 0), exit, OSSafeReleaseNULL(me));
    me->setLength(inLength);
    me->mapping = NULL;

    if (inBytes) {
        IOMemoryMap * map = me->map();
        bcopy(inBytes, (void *)map->getAddress(), inLength);
        OSSafeReleaseNULL(map);
    }

exit:
    return me;
}

OSDefineMetaClassAndStructors(IOHIDLibUserClient, IOUserClient2022);


const IOExternalMethodDispatch2022 IOHIDLibUserClient::
sMethods[kIOHIDLibUserClientNumCommands] = {
    { //    kIOHIDLibUserClientDeviceIsValid
    NULL, // Unsupported
    0, 0,
    2, 0,
    false
    },
    { //    kIOHIDLibUserClientOpen
    (IOExternalMethodAction) &IOHIDLibUserClient::_open,
    1, 0,
    0, 0,
    false
    },
    { //    kIOHIDLibUserClientClose
    (IOExternalMethodAction) &IOHIDLibUserClient::_close,
    0, 0,
    0, 0,
    false
    },
    { //    kIOHIDLibUserClientCreateQueue
    (IOExternalMethodAction) &IOHIDLibUserClient::_createQueue,
    2, 0,
    1, 0,
    false
    },
    { //    kIOHIDLibUserClientDisposeQueue
    (IOExternalMethodAction) &IOHIDLibUserClient::_disposeQueue,
    1, 0,
    0, 0,
    true
    },
    { //    kIOHIDLibUserClientAddElementToQueue
    (IOExternalMethodAction) &IOHIDLibUserClient::_addElementToQueue,
    3, 0,
    1, 0,
    true
    },
    { //    kIOHIDLibUserClientRemoveElementFromQueue
    (IOExternalMethodAction) &IOHIDLibUserClient::_removeElementFromQueue,
    2, 0,
    1, 0,
    true
    },
    { //    kIOHIDLibUserClientQueueHasElement
    (IOExternalMethodAction) &IOHIDLibUserClient::_queueHasElement,
    2, 0,
    1, 0,
    false
    },
    { //    kIOHIDLibUserClientStartQueue
    (IOExternalMethodAction) &IOHIDLibUserClient::_startQueue,
    1, 0,
    0, 0,
    true
    },
    { //    kIOHIDLibUserClientStopQueue
    (IOExternalMethodAction) &IOHIDLibUserClient::_stopQueue,
    1, 0,
    0, 0,
    true
    },
    { //    kIOHIDLibUserClientUpdateElementValues
    (IOExternalMethodAction) &IOHIDLibUserClient::_updateElementValues,
    3, kIOUCVariableStructureSize,
    0, kIOUCVariableStructureSize,
    true
    },
    { //    kIOHIDLibUserClientPostElementValues
    (IOExternalMethodAction) &IOHIDLibUserClient::_postElementValues,
    1, kIOUCVariableStructureSize,
    0, 0,
    true
    },
    { //    kIOHIDLibUserClientGetReport
    (IOExternalMethodAction) &IOHIDLibUserClient::_getReport,
    3, 0,
    0, kIOUCVariableStructureSize,
    true
    },
    { //    kIOHIDLibUserClientSetReport
    (IOExternalMethodAction) &IOHIDLibUserClient::_setReport,
    3, kIOUCVariableStructureSize,
    0, 0,
    true
    },
    { //    kIOHIDLibUserClientGetElementCount
    (IOExternalMethodAction) &IOHIDLibUserClient::_getElementCount,
    0, 0,
    2, 0,
    false
    },
    { //    kIOHIDLibUserClientGetElements
    (IOExternalMethodAction) &IOHIDLibUserClient::_getElements,
    1, 0,
    0, kIOUCVariableStructureSize,
    false
    },
    // ASYNC METHODS
    { //    kIOHIDLibUserClientSetQueueAsyncPort
    (IOExternalMethodAction) &IOHIDLibUserClient::_setQueueAsyncPort,
    1, 0,
    0, 0,
    true
    },
    { //    kIOHIDLibUserClientReleaseReport
    (IOExternalMethodAction) &IOHIDLibUserClient::_releaseReport,
    1, 0,
    0, 0,
    true
    },
    { //   kIOHIDLibUserClientResumeReports
    (IOExternalMethodAction) &IOHIDLibUserClient::_resumeReports,
    0, 0,
    0, 0,
    true
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
    fClientSeized  = false;
    
    queue_head_init(fReportList);
    queue_head_init(fBlockedReports);

    fReportLimit = kIOHIDDefaultMaxReportSize;

    fQueueMap = OSArray::withCapacity(4);
    if (!fQueueMap) {
        return false;
    }

    _queueLock = IOLockAlloc();
    if (!_queueLock) {
        return false;
    }
    
    entitlement = copyClientEntitlement(fClient, kIOHIDManagerUserAccessCustomQueueSizeEntitlement);
    
    if (entitlement) {
        _customQueueSizeEntitlement = (entitlement == kOSBooleanTrue);
        entitlement->release();
        clientEntitlements |= kHIDLibUserClientCustomQueueSizeEntitlement;
    }

    entitlement = copyClientEntitlement(fClient, kIOHIDManagerUserAccessInterfaceRematchEntitlement);

    if (entitlement) {
        _interfaceRematchEntitlement = (entitlement == kOSBooleanTrue);
        entitlement->release();
        clientEntitlements |= kHIDLibUserClientInterfaceRematchEntitlement;
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

    if (provider->getProperty(kIOHIDProtectedAccessKey) == kOSBooleanTrue && _protectedAccessClient == false) {
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

    setProperty(kIOUserClientMessageAppSuspendedKey,                      kOSBooleanTrue);
    setProperty(kIOUserClientDefaultLockingKey,                           kOSBooleanTrue);
    setProperty(kIOUserClientDefaultLockingSetPropertiesKey,              kOSBooleanTrue);
    setProperty(kIOUserClientDefaultLockingSingleThreadExternalMethodKey, kOSBooleanFalse);
    setProperty(kIOUserClientEntitlementsKey,                             kOSBooleanFalse);

    obj = fNub->copyProperty(kIOHIDPrimaryUsageKey);
    primaryUsage = OSDynamicCast(OSNumber, obj);
    obj2 = fNub->copyProperty(kIOHIDPrimaryUsagePageKey);
    primaryUsagePage = OSDynamicCast(OSNumber, obj2);

    if ((primaryUsagePage && (primaryUsagePage->unsigned32BitValue() == kHIDPage_GenericDesktop)) && (primaryUsage && ((primaryUsage->unsigned32BitValue() == kHIDUsage_GD_Keyboard) || (primaryUsage->unsigned32BitValue() == kHIDUsage_GD_Keypad)))) {
        fNubIsKeyboard = true;
    }

    OSSafeReleaseNULL(obj);
    OSSafeReleaseNULL(obj2);

    cmdGate = IOCommandGate::commandGate(this);
    require(cmdGate, error);

    fWL->addEventSource(cmdGate);

    fGate = cmdGate;

    fResourceES = IOInterruptEventSource::interruptEventSource(this, OSMemberFunctionCast(IOInterruptEventSource::Action, this, &IOHIDLibUserClient::resourceNotificationGated));
    require(fResourceES, error);

    fWL->addEventSource(fResourceES);

    // Get notified everytime Root properties change
    matching = serviceMatching("IOResources");
    fResourceNotification = addMatchingNotification(gIOPublishNotification, matching, OSMemberFunctionCast(IOServiceMatchingNotificationHandler, this, &IOHIDLibUserClient::resourceNotification), this);
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

    if (fClientOpened && !fClientSeized) {
        setValid(kIOReturnSuccess == ret);
    }
}

typedef struct HIDCommandGateArgs {
    uint32_t                          selector;
    IOExternalMethodArgumentsOpaque * arguments;
}HIDCommandGateArgs;

IOReturn IOHIDLibUserClient::externalMethod(uint32_t selector, IOExternalMethodArgumentsOpaque * arguments)
{
    IOReturn           status = kIOReturnOffline;
    HIDCommandGateArgs args;

    if (fGate) {
        args.selector  = selector;
        args.arguments = arguments;

        status = fGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDLibUserClient::externalMethodGated), (void *)&args);
    }

    return status;
}

IOReturn IOHIDLibUserClient::externalMethodGated(void * args)
{
    IOReturn                          status    = kIOReturnOffline;
    HIDCommandGateArgs              * cArgs     = (HIDCommandGateArgs *)args;
    uint32_t                          selector  = cArgs->selector;
    IOExternalMethodArgumentsOpaque * arguments = cArgs->arguments;

    if (!isInactive()) {
        status = dispatchExternalMethod(selector, arguments, sMethods, sizeof(sMethods) / sizeof(sMethods[0]), this, NULL);
    }

    return status;
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
    IOReturn ret = kIOReturnSuccess;

    HIDLibUserClientLogInfo("open");

    require_action_quiet(fNub, exit, ret = kIOReturnOffline);

    if (!_privilegedClient) {
        HIDLibUserClientLogInfo("open client not privileged");

        // If this is a keyboard and the client is attempting to seize, the client needs to be admin
        require_action(!fNubIsKeyboard || ((options & kIOHIDOptionsTypeSeizeDevice) == 0), exit, ret = kIOReturnNotPrivileged);
        require_noerr((ret = clientHasPrivilege(fClient, kIOClientPrivilegeLocalUser)), exit);
    }

    if (!fNub->open(this, options)) {
        // If open failed, but we made it into the client set, the device is seized.
        require_action(fNub->isOpen(this), exit, ret = kIOReturnError);
        ret = kIOReturnExclusiveAccess;
    }

    fClientSeized = ret == kIOReturnExclusiveAccess;
    fClientOpened = true;
    fCachedOptionBits = options;
    fCachedConsoleUsersSeed = 0;
    resourceNotificationGated();
exit:
    return ret;
}

IOReturn IOHIDLibUserClient::_close(IOHIDLibUserClient * target, void * reference __unused, IOExternalMethodArguments * arguments __unused)
{
    return target->close();
}

IOReturn IOHIDLibUserClient::close()
{
    IOHIDBlockedReport * report = NULL;

    HIDLibUserClientLogInfo("close");

    require_quiet(fNub && fClientOpened, exit);

    fNub->close(this, fCachedOptionBits);
    setValid(false);

    fCachedOptionBits = 0;
    fClientOpened     = false;
    fClientSeized     = false;

    if (!queue_empty(&fBlockedReports)) {
        IOReturn ret = THREAD_AWAKENED;
        report = (IOHIDBlockedReport*)queue_first(&fBlockedReports);
        HIDLibUserClientLog("Waking blocked reports due to close");
        fGate->commandWakeup(report);
        ret = fGate->commandSleep(&fBlockedReports);
        HIDLibUserClientLogInfo("Blocked reports sleep completed(%#x)", ret);
    }
 
exit:
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

    if (_queueLock) {
        IOLockFree(_queueLock);
        _queueLock = NULL;
    }

    while (!queue_empty(&fReportList)) {
        IOHIDOOBReportDescriptor *md = NULL;
        queue_remove_first(&fReportList, md, IOHIDOOBReportDescriptor *, qc);
        OSSafeReleaseNULL(md);
        HIDLibUserClientLog("Dropping report that was not released by IOKit client");
    }

    if ( fValidMessage ) {
        IOFreeType(fValidMessage, struct _notifyMsg);
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

    if (type == kIOMessageTaskAppSuspendedChange && (fCachedOptionBits & kIOHIDOptionsPrivateTypeNoDroppedReports)) {
        __block bool isSuspended = task_is_app_suspended (fClient);
        
        HIDLibUserClientLog("suspended:%d", isSuspended);
        
        fGate->runActionBlock(^IOReturn (){
            if (!fClientOpened) {
                return kIOReturnSuccess;
            }
            if (isSuspended) {
                fClientSuspended = true;
                // Clear any waiting reports.
                if (!queue_empty(&fBlockedReports)) {
                    IOHIDBlockedReport *report = NULL;
                    queue_remove_first(&fBlockedReports, report, IOHIDBlockedReport *, qc);
                    fGate->commandWakeup(report);
                    HIDLibUserClientLog("Woke blocked report thread due to suspended client");
                }
            } else {
                fClientSuspended = false;
            }
            return kIOReturnSuccess;
        });
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
                 fClientSeized = true;
                 setValid(false);
            }
            break;
            
        case kIOMessageServiceWasClosed:
            if ((options & kIOHIDOptionsTypeSeizeDevice) && (options != fCachedOptionBits)) {
                HIDLibUserClientLogInfo("Message : Seize Device");
                // Make sure we still have permission through the resource notification
                fClientSeized = false;
                fCachedConsoleUsersSeed = 0;
                resourceNotificationGated();
            }
            break;
    };
    
    return kIOReturnSuccess;
}

IOReturn IOHIDLibUserClient::setProperties(OSObject *properties)
{
    IOReturn ret = kIOReturnUnsupported;
    OSDictionary * props = OSDynamicCast(OSDictionary, properties);
    OSNumber     * num;

    require_action_quiet(fNub, exit, ret = kIOReturnOffline);

    if (props && props->getObject(kIOHIDDeviceGameControllerSupportKey)) {
        if (_interfaceRematchEntitlement) {
            ret = fNub->setProperty(kIOHIDDeviceGameControllerSupportKey, props->getObject(kIOHIDDeviceGameControllerSupportKey)) ? kIOReturnSuccess : kIOReturnError;
        }
        props->removeObject(kIOHIDDeviceGameControllerSupportKey);
        require_quiet(props->getCount(), exit);
    }

    if (props && props->getObject(kIOHIDDeviceForceInterfaceRematchKey)) {
        if (_interfaceRematchEntitlement) {
            ret = fNub->setProperty(kIOHIDDeviceForceInterfaceRematchKey, props->getObject(kIOHIDDeviceForceInterfaceRematchKey)) ? kIOReturnSuccess : kIOReturnError;
        }
        props->removeObject(kIOHIDDeviceForceInterfaceRematchKey);
        require_quiet(props->getCount(), exit);
    }

    ret = fNub->setProperties(properties);

    if (props && (num = OSDynamicCast(OSNumber, props->getObject(kIOHIDMaxReportBufferCountKey)))) {
        ret = setProperty(kIOHIDMaxReportBufferCountKey, num) ? kIOReturnSuccess : kIOReturnError;
    }
    if (props && (num = OSDynamicCast(OSNumber, props->getObject(kIOHIDReportBufferEntrySizeKey)))) {
        ret = setProperty(kIOHIDReportBufferEntrySizeKey, num) ? kIOReturnSuccess : kIOReturnError;
    }

exit:
    return ret;
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
                IOFreeType(fValidMessage, struct _notifyMsg);
                fValidMessage = NULL;
            }
                
            if ( !(fValidMessage = IOMallocType(struct _notifyMsg)) ) {
                kr = kIOReturnNoMemory;
                break;
            }
            
            fValidPort = port;
            
            if ( !fValidPort ) {
                IOFreeType(fValidMessage, struct _notifyMsg);
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
    if (state != fValid) {
        HIDLibUserClientLogInfo("setValid: from %s to %s", (fValid ? "true" : "false"), (state ? "true" : "false"));
    }
    
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
    
    debugDict->setObject(kIOHIDDevicePrivilegedKey, _privilegedClient ? kOSBooleanTrue : kOSBooleanFalse);

    num = OSNumber::withNumber(fReportLimit, 32);
    if (num) {
        debugDict->setObject("MaxEnqueueReportSize", num);
        num->release();
    }

    if (fQueueMap) {
        debugDict->setObject("EventQueueMap", fQueueMap);
    }

    debugDict->setObject("ClientSuspended", fClientSuspended ? kOSBooleanTrue : kOSBooleanFalse);
    debugDict->setObject("ClientOpened", (fClientOpened ? kOSBooleanTrue : kOSBooleanFalse));
    debugDict->setObject("ClientSeized", (fClientSeized ? kOSBooleanTrue : kOSBooleanFalse));

    num = OSNumber::withNumber(fCachedOptionBits, 32);
    if (num) {
        debugDict->setObject("ClientOptions", num);
        num->release();
    }

    IOLockLock(_queueLock);
    result = debugDict->serialize(serializer);
    IOLockUnlock(_queueLock);
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

OSArray * IOHIDLibUserClient::getElementsForType(uint32_t elementType)
{
    if ( elementType == kHIDElementType ) {
        return fNub->_reserved->hierarchElements;
    }
    return fNub->_reserved->elementContainer->getInputReportElements();
}


IOReturn IOHIDLibUserClient::_getElementCount(IOHIDLibUserClient * target, void * reference __unused, IOExternalMethodArguments * arguments)
{
    return target->getElementCount(&(arguments->scalarOutput[0]), &(arguments->scalarOutput[1]));
}

IOReturn IOHIDLibUserClient::getElementCount(uint64_t * pOutElementCount, uint64_t * pOutReportElementCount)
{
    IOReturn    ret                     = kIOReturnSuccess;

    if (!pOutElementCount || !pOutReportElementCount)
        return kIOReturnBadArgument;

    for (uint32_t elementType = kHIDElementType; elementType <= kHIDReportHandlerType; ++elementType) {
        uint64_t * outElementCount = NULL;
        OSArray * elements = NULL;
        uint32_t validCount = 0;

        elements = getElementsForType(elementType);
        if (elements == NULL) {
            return kIOReturnError;
        }

        outElementCount = elementType == kHIDElementType ? pOutElementCount : pOutReportElementCount;

        for (uint32_t i=0; i < elements->getCount(); ++i)
        {
            IOHIDElementPrivate * element = OSDynamicCast(IOHIDElementPrivate, elements->getObject(i));

            if (!element) {
                continue;
            }

            if (validCount == UINT32_MAX) {
                ret = kIOReturnOverrun;
                break;
            }

            if (element->isValidUserElement()) {
                validCount++;
            }
        }

        *outElementCount = validCount;
    }

    return ret;
}

IOReturn IOHIDLibUserClient::_getElements(IOHIDLibUserClient * target, void * reference __unused, IOExternalMethodArguments * arguments)
{
    if ( arguments->structureOutputDescriptor )
        return target->getElements((uint32_t)arguments->scalarInput[0], arguments->structureOutputDescriptor, &(arguments->structureOutputDescriptorSize));
    else
        return target->getElements((uint32_t)arguments->scalarInput[0], arguments->structureOutput, &(arguments->structureOutputSize));
}

IOReturn IOHIDLibUserClient::getElements(uint32_t elementType, void *elementBuffer, uint32_t *elementBufferSize)
{
    OSArray *                array;
    uint32_t                 i, bufferIndex, count;
    IOHIDElementPrivate *    element;
    IOHIDElementStruct *    elementStruct;
    uint32_t                bufferSize = 0;

    if (!elementBufferSize || !elementBuffer || !(*elementBufferSize)) {
        return kIOReturnBadArgument;
    }
    
    if (*elementBufferSize % sizeof(IOHIDElementStruct) != 0) {
        return kIOReturnBadArgument;
    }
            
    if (!fNub || isInactive()) {
        return kIOReturnNotAttached;
    }

    array = getElementsForType(elementType);
    
    if ( !array ) {
        return kIOReturnError;
    }

    count = array->getCount();
    
    bufferSize = *elementBufferSize;

    if (elementBuffer && bufferSize) {
        memset(elementBuffer, 0, bufferSize);
    }
        
    bufferIndex = 0;
    
    for ( i=0; i<count; i++ )
    {
        element = OSDynamicCast(IOHIDElementPrivate, array->getObject(i));
        
        if (!element) {
            continue;
        }
       
        elementStruct = nullptr;

        do 
        {
            if (bufferIndex == UINT32_MAX) {
                break;
            }

            if ((bufferIndex + 1) > (UINT32_MAX / sizeof(IOHIDElementStruct))) {
                break;
            }

            if ((bufferIndex + 1) * sizeof(IOHIDElementStruct) <= bufferSize) {
                elementStruct = &(((IOHIDElementStruct *)elementBuffer)[bufferIndex]);
            }
        } while(0);

        if (elementStruct && element->fillElementStruct(elementStruct)) {
            bufferIndex++;
        }
    }
    
    *elementBufferSize = bufferIndex * sizeof(IOHIDElementStruct);

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
            elementData = IOMallocZeroData( elementLength );
            
            if ( elementData )
            {
                ret = getElements(elementType, elementData, &elementLength);
                
                if ( elementBufferSize )
                    *elementBufferSize = elementLength;

                if (elementLength <= mem->getLength()) {
                    mem->writeBytes( 0, elementData, elementLength );
                } else {
                    ret = kIOReturnBadArgument;
                }

                IOFreeData( elementData, allocationSize );
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
    
    if ((reportBufferCount = OSDynamicCast(OSNumber, copyProperty(kIOHIDMaxReportBufferCountKey))) ||
        (reportBufferCount = OSDynamicCast(OSNumber, fNub->copyProperty(kIOHIDMaxReportBufferCountKey)))) {
        bufferCount = reportBufferCount->unsigned32BitValue();
        OSSafeReleaseNULL(reportBufferCount);
    }
    
    if ((reportBufferEntrySize = OSDynamicCast(OSNumber, copyProperty(kIOHIDReportBufferEntrySizeKey))) ||
        (reportBufferEntrySize = OSDynamicCast(OSNumber, fNub->copyProperty(kIOHIDReportBufferEntrySizeKey)))) {
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
        eventQueue = IOHIDReportElementQueue::withCapacity(queueSize, this);
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

IOReturn IOHIDLibUserClient::_updateElementValues(IOHIDLibUserClient * target, void * reference __unused, IOExternalMethodArguments * arguments)
{
    IOReturn          ret           = kIOReturnNoMemory;
    uint32_t          timeout       = 0;
    IOOptionBits      options       = (IOOptionBits)arguments->scalarInput[2];
    IOHIDCompletion * completion    = NULL;
    OSData          * asyncData     = NULL;
    vm_size_t         inputSize     = (vm_size_t)arguments->scalarInput[1];
    IOHIDOOBReportDescriptor * buff = NULL;
    AsyncCommitParam  pb;

    require_quiet(inputSize < HID_MAX_ELEMENT_SIZE, no_mem);

    if (arguments->asyncWakePort) {
        target->retain();

        require_action((buff = IOHIDOOBReportDescriptor::inTaskWithBytes(kernel_task, NULL, inputSize, kIODirectionOutIn)), exit, ret = kIOReturnNoMemory);
        bcopy(arguments->asyncReference, pb.fAsyncRef, sizeof(OSAsyncReference64));
        pb.fCompletion.target = target;
        pb.fCompletion.action = OSMemberFunctionCast(IOHIDCompletionAction, target, &IOHIDLibUserClient::CommitComplete);
        pb.elementData        = buff;

        require((asyncData = OSData::withBytes(&pb, sizeof(AsyncCommitParam))), exit);
        ((AsyncCommitParam *)asyncData->getBytesNoCopy())->fCompletion.parameter = asyncData;
        target->_pending->setObject(asyncData);

        timeout = (uint32_t)arguments->scalarInput[0];
        completion = &(((AsyncCommitParam *)asyncData->getBytesNoCopy())->fCompletion);
    }

    if (arguments->structureOutputDescriptor) {
        return target->updateElementValues((const IOHIDElementCookie *)arguments->structureInput, arguments->structureInputSize, arguments->structureOutputDescriptor, arguments->structureOutputDescriptorSize, options, timeout, completion, buff);
    } else {
        return target->updateElementValues((const IOHIDElementCookie *)arguments->structureInput, arguments->structureInputSize, arguments->structureOutput, arguments->structureOutputSize, options, timeout, completion, buff);
    }

exit:
    if (ret && arguments->asyncWakePort) {
        target->_pending->removeObject(asyncData);
        target->release();
        OSSafeReleaseNULL(buff);
        OSSafeReleaseNULL(asyncData);
    }

no_mem:
    return ret;
}

IOReturn IOHIDLibUserClient::updateElementValues(const IOHIDElementCookie * lCookies, uint32_t cookieSize, IOMemoryDescriptor * outputElementsDesc, uint32_t outputElementsDescSize, IOOptionBits options, uint32_t timeout, IOHIDCompletion * completion, IOBufferMemoryDescriptor * elementData)
{
    IOReturn      ret     = kIOReturnBadArgument;
    IOMemoryMap * mapping = NULL;
    uint8_t     * addr;
    uint32_t      elementLength;

    require_action(fClientOpened, exit, ret = kIOReturnNotOpen; HIDLibUserClientLogError("Client not opened"));
    require_action(!fClientSeized, exit, ret = kIOReturnExclusiveAccess; HIDLibUserClientLogError("Client is seized"));
    require_action(fValid, exit, ret = kIOReturnNotPermitted; HIDLibUserClientLogError("Client not Permitted"));
    require((elementLength = (uint32_t)outputElementsDesc->getLength()), exit);
    require_action((mapping = outputElementsDesc->map()), exit, ret = kIOReturnNoMemory);
    require_action((addr = (uint8_t *)(mapping->getVirtualAddress())), exit, ret = kIOReturnNoMemory);

    ret = updateElementValues(lCookies, cookieSize, addr, elementLength, options, timeout, completion, elementData);
    require_noerr_action(ret, exit, HIDLibUserClientLogError("updateElementValues failed: 0x%x", ret));

exit:
    OSSafeReleaseNULL(mapping);
    return ret;
}

IOReturn IOHIDLibUserClient::updateElementValues(const IOHIDElementCookie * lCookies, uint32_t cookieSize, void * outputElements, uint32_t outputElementsSize, IOOptionBits options, uint32_t timeout, IOHIDCompletion * completion, IOBufferMemoryDescriptor * elementData)
{
    IOReturn              ret         = kIOReturnSuccess;
    uint32_t              neededSize;
    uint32_t              dataOffset  = 0;
    uint32_t              cookieCount = (uint32_t)(cookieSize / sizeof(uint32_t));
    const uint32_t      * cookies     = (const uint32_t *)lCookies;
    OSArray             * elements    = fNub ? fNub->_elementArray : NULL;
    IOHIDElementPrivate * element;
    IOHIDElementValue   * elementVal;
    OSBoundedArrayRef<const uint32_t> cookiesRef;

    require_action(fClientOpened, exit, ret = kIOReturnNotOpen);
    require_action(!fClientSeized, exit, ret = kIOReturnExclusiveAccess);
    require_action(fValid, exit, ret = kIOReturnNotPermitted);
    require_action(fNub && !isInactive(), exit, ret = kIOReturnNotAttached);
    require_action(lCookies && cookieSize && (cookieSize % sizeof(uint32_t)) == 0, exit, ret = kIOReturnBadArgument);

    if (!(options & kIOHIDElementPreventPoll)) {
        ret = fNub->updateElementValues((IOHIDElementCookie *)lCookies, cookieCount, options, timeout, completion, elementData);
        require_noerr_action(ret, exit, HIDLibUserClientLogError("updateElementValues failed: 0x%x", ret));
    }

    require_quiet(!completion, exit);
    cookiesRef = OSBoundedArrayRef<const uint32_t>(&cookies[0], cookieCount);
    for (const uint32_t cookie : cookiesRef) {
        if (!(element = (IOHIDElementPrivate *)elements->getObject(cookie)) || !(elementVal = element->_elementValue)) {
            HIDLibUserClientLogError("No element found for cookie %d", cookie);
            continue;
        }

        require_action(!os_add_overflow(dataOffset, elementVal->totalSize, &neededSize), exit, ret = kIOReturnNoMemory; HIDLibUserClientLogError("Element value overflow"));
        require_action(neededSize <= outputElementsSize, exit, ret = kIOReturnBadArgument; HIDLibUserClientLogError("No space for element values. Need: %d Available: %d", neededSize, outputElementsSize));

        memcpy((uint8_t *)outputElements + dataOffset, elementVal, elementVal->totalSize);
        dataOffset += elementVal->totalSize;
    }

exit:
    return ret;
}

IOReturn IOHIDLibUserClient::_postElementValues(IOHIDLibUserClient * target, void * reference __unused, IOExternalMethodArguments * arguments)
{
    IOReturn          ret        = kIOReturnNoMemory;
    uint32_t          timeout    = 0;
    IOHIDCompletion * completion = NULL;
    OSData          * asyncData  = NULL;
    AsyncCommitParam  pb;

    if (arguments->asyncWakePort) {
        target->retain();

        bcopy(arguments->asyncReference, pb.fAsyncRef, sizeof(OSAsyncReference64));
        pb.fCompletion.target = target;
        pb.fCompletion.action = OSMemberFunctionCast(IOHIDCompletionAction, target, &IOHIDLibUserClient::CommitComplete);
        pb.elementData        = NULL;

        require((asyncData = OSData::withBytes(&pb, sizeof(AsyncCommitParam))), exit);
        ((AsyncCommitParam *)asyncData->getBytesNoCopy())->fCompletion.parameter = asyncData;
        target->_pending->setObject(asyncData);

        timeout = (uint32_t)arguments->scalarInput[0];
        completion = &(((AsyncCommitParam *)asyncData->getBytesNoCopy())->fCompletion);
    }

    if (arguments->structureInputDescriptor) {
        ret = target->postElementValues(arguments->structureInputDescriptor, timeout, completion);
    } else {
        ret = target->postElementValues((const uint8_t *)arguments->structureInput, arguments->structureInputSize, timeout, completion);
    }

exit:
    if (ret && arguments->asyncWakePort) {
        target->_pending->removeObject(asyncData);
        target->release();
        OSSafeReleaseNULL(asyncData);
    }
    return ret;
}

IOReturn IOHIDLibUserClient::postElementValues(IOMemoryDescriptor * desc, uint32_t timeout, IOHIDCompletion * completion)
{
    IOReturn      ret     = kIOReturnBadArgument;
    IOMemoryMap * mapping = NULL;
    uint8_t     * elementData;
    uint32_t      elementLength;

    require_action(fClientOpened, exit, ret = kIOReturnNotOpen; HIDLibUserClientLogError("Client not opened"));
    require_action(!fClientSeized, exit, ret = kIOReturnExclusiveAccess; HIDLibUserClientLogError("Client is seized"));
    require_action(fValid, exit, ret = kIOReturnNotPermitted; HIDLibUserClientLogError("Client not permitted"));
    require((elementLength = (uint32_t)desc->getLength()), exit);
    require_action((mapping = desc->map()), exit, ret = kIOReturnNoMemory);
    require_action(elementData = reinterpret_cast<uint8_t*>(mapping->getVirtualAddress()), exit, ret = kIOReturnNoMemory);

    ret = postElementValues(elementData, elementLength, timeout, completion);
    require_noerr_action(ret, exit, HIDLibUserClientLogError("postElementValues failed: 0x%x", ret));

exit:
    OSSafeReleaseNULL(mapping);
    return ret;
}

IOReturn IOHIDLibUserClient::postElementValues(const uint8_t * data, uint32_t dataSize, uint32_t timeout, IOHIDCompletion * completion)
{
    IOReturn ret = kIOReturnError;

    require_action(fClientOpened, exit, ret = kIOReturnNotOpen);
    require_action(!fClientSeized, exit, ret = kIOReturnExclusiveAccess);
    require_action(fValid, exit, ret = kIOReturnNotPermitted);
    require_action(fNub && !isInactive(), exit, ret = kIOReturnNotAttached);
    require_action(data && dataSize, exit, ret = kIOReturnBadArgument);

    ret = fNub->postElementTransaction(data, dataSize, timeout, completion);
    require_noerr_action(ret, exit, HIDLibUserClientLogError("postElementValues failed: 0x%x", ret));

exit:
    return ret;
}

void IOHIDLibUserClient::CommitComplete(void *param, IOReturn res, UInt32 remaining)
{
    fGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDLibUserClient::CommitCompleteGated), param, (void *)(intptr_t)res, (void *)(intptr_t)remaining);
}

IOReturn IOHIDLibUserClient::CommitCompleteGated(void * param, IOReturn res, UInt32 remaining)
{
    IOReturn                   ret       = kIOReturnError;
    io_user_reference_t        args[2]   = {0};
    OSData                   * asyncData = (OSData *)param;
    AsyncCommitParam         * pb        = NULL;
    IOHIDOOBReportDescriptor * buff;

    require(asyncData, exit);

    pb = (AsyncCommitParam *)asyncData->getBytesNoCopy();

    if ((buff = OSDynamicCast(IOHIDOOBReportDescriptor, pb->elementData))) {
        args[0] = buff->getLength();
        if ((buff->mapping = pb->elementData->createMappingInTask(fClient, 0, kIOMapAnywhere | kIOMapReadOnly, 0, args[0]))) {
            args[1] = (uint64_t)buff->mapping->getAddress();
            queue_enter(&fReportList, buff, IOHIDOOBReportDescriptor *, qc);
        } else {
            HIDLibUserClientLogError("createMappingInTask failed");
            OSSafeReleaseNULL(buff);
            args[0] = 0;
            res = kIOReturnNoMemory;
        }
    } else if (pb->elementData) {
        pb->elementData->release();
    }

    ret = sendAsyncResult64(pb->fAsyncRef, res, args, 2);
    if (ret) {
        HIDLibUserClientLogError("Async commit result dropped: 0x%x", ret);
        if (buff) {
            releaseReport(buff->mapping->getAddress());
        }
    }

    _pending->removeObject(asyncData);
    asyncData->release();

exit:
    if (_pending->getCount() == 0) {
        fGate->commandWakeup(&_pending);
    }
    // Match retain in _updateElementValues/_postElementValues
    release();
    return kIOReturnSuccess;
}

IOReturn IOHIDLibUserClient::_getReport(IOHIDLibUserClient * target, void * reference __unused, IOExternalMethodArguments * arguments)
{
    IOReturn                   ret        = kIOReturnNoMemory;
    OSData                   * asyncData  = NULL;
    uint32_t                   timeout    = 0;
    IOHIDCompletion          * completion = NULL;
    IOBufferMemoryDescriptor * mem        = NULL;
    AsyncReportParam           pb;

    if (arguments->asyncWakePort) {
        target->retain();

        bcopy(arguments->asyncReference, pb.fAsyncRef, sizeof(OSAsyncReference64));
        pb.fCompletion.target = target;
        pb.fCompletion.action = OSMemberFunctionCast(IOHIDCompletionAction, target, &IOHIDLibUserClient::ReqComplete);

        require((asyncData = OSData::withBytes(&pb, sizeof(AsyncReportParam))), exit);
        ((AsyncReportParam *)asyncData->getBytesNoCopy())->fCompletion.parameter = asyncData;
        target->_pending->setObject(asyncData);

        timeout = (uint32_t)arguments->scalarInput[2];
        completion = &(((AsyncReportParam *)asyncData->getBytesNoCopy())->fCompletion);
    }

    if (arguments->structureOutputDescriptor) {
        require((mem = IOHIDOOBReportDescriptor::inTaskWithBytes(kernel_task, NULL, arguments->structureOutputDescriptorSize, kIODirectionOutIn)), exit);

        require_noerr((ret = target->getReport(mem, &(arguments->structureOutputDescriptorSize), (IOHIDReportType)arguments->scalarInput[0], (uint32_t)arguments->scalarInput[1], timeout, completion)), exit);

        if (!completion) {
            require_noerr((ret = arguments->structureOutputDescriptor->prepare()), exit);
            arguments->structureOutputDescriptor->writeBytes(0, mem->getBytesNoCopy(), arguments->structureOutputDescriptorSize);
            arguments->structureOutputDescriptor->complete();
        }

    } else {
        ret = target->getReport(arguments->structureOutput, &(arguments->structureOutputSize), (IOHIDReportType)arguments->scalarInput[0], (uint32_t)arguments->scalarInput[1], timeout, completion);
    }

exit:
    if (ret && arguments->asyncWakePort) {
        target->_pending->removeObject(asyncData);
        target->release();
        OSSafeReleaseNULL(asyncData);
    }
    OSSafeReleaseNULL(mem);
    return ret;
}

IOReturn IOHIDLibUserClient::getReport(void * reportBuffer, uint32_t * pOutsize, IOHIDReportType reportType, uint32_t reportID, uint32_t timeout, IOHIDCompletion * completion)
{
    IOReturn             ret = kIOReturnBadArgument;
    IOMemoryDescriptor * mem = NULL;

    require_action(*pOutsize <= 0x10000, exit, HIDLibUserClientLogError("called with an irrationally large output size: %u", *pOutsize));
    require_action((mem = IOHIDOOBReportDescriptor::inTaskWithBytes(kernel_task, NULL, *pOutsize, kIODirectionOutIn)), exit, ret = kIOReturnNoMemory);

    require_noerr_action((ret = getReport(mem, pOutsize, reportType, reportID, timeout, completion)), exit, HIDLibUserClientLogError("getReport failed: 0x%x", ret));
    if (!completion) {
        mem->readBytes(0, reportBuffer, *pOutsize);
    }

exit:
    OSSafeReleaseNULL(mem);
    return ret;
}

IOReturn IOHIDLibUserClient::getReport(IOMemoryDescriptor * mem, uint32_t * pOutsize, IOHIDReportType reportType, uint32_t reportID, uint32_t timeout, IOHIDCompletion * completion)
{
    IOReturn           ret = kIOReturnBadArgument;
    AsyncReportParam * pb;

    require_action(fClientOpened, exit, ret = kIOReturnNotOpen; HIDLibUserClientLogError("Client not opened"));
    require_action(!fClientSeized, exit, ret = kIOReturnExclusiveAccess; HIDLibUserClientLogError("Client is seized"));
    require_action(fValid, exit, ret = kIOReturnNotPermitted; HIDLibUserClientLogError("Client not permitted"));
    require_action(fNub && !isInactive(), exit, ret = kIOReturnNotAttached);
    require_action(*pOutsize <= 0x10000, exit, HIDLibUserClientLogError("called with an irrationally large output size: %u", *pOutsize));
    require_noerr((ret = mem->prepare(kIODirectionInOut)), exit);

    if (completion) {
        pb              = (AsyncReportParam*)((OSData*)completion->parameter)->getBytesNoCopy();
        pb->fMem        = mem;
        pb->reportType  = reportType;
        mem->retain();

        ret = fNub->getReport(mem, reportType, reportID, timeout, completion);
        if (ret != kIOReturnSuccess) {
            mem->complete();
            mem->release();
        }

    } else {
        ret = fNub->getReport(mem, reportType, reportID);
        if (ret == kIOReturnSuccess) {
            // Update element values
            fNub->handleReport(mem, reportType, kIOHIDReportOptionNotInterrupt);
        }

        *pOutsize = (uint32_t)mem->getLength();
        mem->complete();
    }

exit:
    return ret;
}

IOReturn IOHIDLibUserClient::_setReport(IOHIDLibUserClient * target, void * reference __unused, IOExternalMethodArguments * arguments)
{
    IOReturn                   ret        = kIOReturnNoMemory;
    OSData                   * asyncData  = NULL;
    uint32_t                   timeout    = 0;
    IOHIDCompletion          * completion = NULL;
    IOBufferMemoryDescriptor * mem        = NULL;
    AsyncReportParam           pb;

    if (arguments->asyncWakePort) {
        target->retain();
        
        bcopy(arguments->asyncReference, pb.fAsyncRef, sizeof(OSAsyncReference64));
        pb.fCompletion.target = target;
        pb.fCompletion.action = OSMemberFunctionCast(IOHIDCompletionAction, target, &IOHIDLibUserClient::ReqComplete);
        
        require((asyncData = OSData::withBytes(&pb, sizeof(AsyncReportParam))), exit);
        ((AsyncReportParam *)asyncData->getBytesNoCopy())->fCompletion.parameter = asyncData;
        target->_pending->setObject(asyncData);

        timeout = (uint32_t)arguments->scalarInput[2];
        completion = &(((AsyncReportParam *)asyncData->getBytesNoCopy())->fCompletion);
    }
        
    if (arguments->structureInputDescriptor) {
        ret = target->setReport(arguments->structureInputDescriptor, (IOHIDReportType)arguments->scalarInput[0], (uint32_t)arguments->scalarInput[1], timeout, completion);
    } else {
        ret = target->setReport(arguments->structureInput, arguments->structureInputSize, (IOHIDReportType)arguments->scalarInput[0], (uint32_t)arguments->scalarInput[1], timeout, completion);
    }

exit:
    if (ret && arguments->asyncWakePort) {
        target->_pending->removeObject(asyncData);
        target->release();
        OSSafeReleaseNULL(asyncData);
    }
    OSSafeReleaseNULL(mem);
    return ret;
}

IOReturn IOHIDLibUserClient::setReport(const void * reportBuffer, uint32_t reportBufferSize, IOHIDReportType reportType, uint32_t reportID, uint32_t timeout, IOHIDCompletion * completion)
{
    IOReturn             ret = kIOReturnNoMemory;
    IOMemoryDescriptor * mem = IOBufferMemoryDescriptor::withOptions(kIODirectionOutIn | kIOMemoryKernelUserShared | kIOMemoryThreadSafe, reportBufferSize);

    require(mem, exit);
    mem->writeBytes(0, reportBuffer, reportBufferSize);
    require_noerr_action((ret = setReport(mem, reportType, reportID, timeout, completion)), exit, HIDLibUserClientLogError("setReport failed: 0x%x", (unsigned int)ret));

exit:
    OSSafeReleaseNULL(mem);
    return ret;
}

IOReturn IOHIDLibUserClient::setReport(IOMemoryDescriptor * mem, IOHIDReportType reportType, uint32_t reportID, uint32_t timeout, IOHIDCompletion * completion)
{
    IOReturn               ret;
    OSObject             * obj       = NULL;
    OSObject             * objItr;
    OSArray              * extended;
    OSCollectionIterator * itr       = NULL;
    OSNumber             * num;
    AsyncReportParam     * pb;
    uint8_t                excludedReportID;
    uint8_t                excludedReportType;

    require_action(fClientOpened, exit, ret = kIOReturnNotOpen; HIDLibUserClientLogError("Client not opened"));
    require_action(!fClientSeized, exit, ret = kIOReturnExclusiveAccess; HIDLibUserClientLogError("Client is seized"));
    require_action(fValid, exit, ret = kIOReturnNotPermitted; HIDLibUserClientLogError("Client not permitted"));
    require_action(fNub && !isInactive(), exit, ret = kIOReturnNotAttached);
    require_noerr((ret = mem->prepare(kIODirectionInOut)), exit);

    obj = copyProperty(kIOHIDExtendedDataKey);
    extended = OSDynamicCast(OSArray, obj);
    if ((itr = OSCollectionIterator::withCollection(extended))) {
        while (true) {
            itr->reset();
            while (((objItr = itr->getNextObject()))) {
                if ((num = OSDynamicCast(OSNumber, objItr))) {
                    excludedReportID   = (num->unsigned64BitValue() >> 16) & 0xff;
                    excludedReportType = (num->unsigned64BitValue() >> 24) & 0xff;
                    require_action(excludedReportID != reportID || excludedReportType != (reportType + 1), exit, ret = kIOReturnNotPrivileged; HIDLibUserClientLogError("%02x/%02x blocked due to lack of privileges", reportID, reportType));
                }
            }
            // Check if the list changed during iteration
            if (itr->isValid()) {
                break;
            }
        }
    }

    if (completion) {
        pb             = (AsyncReportParam*)((OSData*)completion->parameter)->getBytesNoCopy();
        pb->fMem       = mem;
        pb->reportType = reportType;
        mem->retain();

        ret = fNub->setReport(mem, reportType, reportID, timeout, completion);
        if (ret != kIOReturnSuccess) {
            mem->complete();
            mem->release();
        }
    }
    else {
        ret = fNub->setReport(mem, reportType, reportID);
        // Update element values
        if (ret == kIOReturnSuccess) {
            fNub->handleReport(mem, reportType, kIOHIDReportOptionNotInterrupt);
        }
        mem->complete();
    }

exit:
    OSSafeReleaseNULL(obj);
    OSSafeReleaseNULL(itr);
    return ret;
}

void IOHIDLibUserClient::ReqComplete(void *param, IOReturn res, UInt32 remaining)
{
    fGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDLibUserClient::ReqCompleteGated), param, (void *)(intptr_t)res, (void *)(intptr_t)remaining);
}

IOReturn IOHIDLibUserClient::ReqCompleteGated(void * param, IOReturn res, UInt32 remaining)
{
    IOReturn                   ret       = kIOReturnError;
    io_user_reference_t        args[2]   = {0};
    OSData                   * asyncData = (OSData *)param;
    AsyncReportParam         * pb        = NULL;
    IOHIDOOBReportDescriptor * buff;

    require(asyncData, exit);

    pb = (AsyncReportParam *)asyncData->getBytesNoCopy();

    if(res == kIOReturnSuccess) {
        // Update element values
        if (fNub && !isInactive()) {
            fNub->handleReport(pb->fMem, pb->reportType, kIOHIDReportOptionNotInterrupt);
        }
    }

    if ((buff = OSDynamicCast(IOHIDOOBReportDescriptor, pb->fMem))) {
        args[0] = buff->getLength();
        if ((buff->mapping = pb->fMem->createMappingInTask(fClient, 0, kIOMapAnywhere | kIOMapReadOnly, 0, args[0]))) {
            args[1] = (uint64_t)buff->mapping->getAddress();
            queue_enter(&fReportList, buff, IOHIDOOBReportDescriptor *, qc);
        } else {
            HIDLibUserClientLogError("createMappingInTask failed");
            OSSafeReleaseNULL(buff);
            args[0] = 0;
            res = kIOReturnNoMemory;
        }
    } else if (pb->fMem) {
        pb->fMem->complete();
        pb->fMem->release();
    }

    ret = sendAsyncResult64(pb->fAsyncRef, res, args, 2);
    if (ret) {
        HIDLibUserClientLogError("Async report result dropped: 0x%x", ret);
        if (buff) {
            releaseReport(buff->mapping->getAddress());
        }
    }

    _pending->removeObject(asyncData);
    asyncData->release();

exit:
    if (_pending->getCount() == 0) {
        fGate->commandWakeup(&_pending);
    }
    // Match retain in _getReport/_setReport
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
        IOLockLock(_queueLock);
    	fQueueMap->setObject(index, queue);
        IOLockUnlock(_queueLock);
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
            IOLockLock(_queueLock);
            fQueueMap->replaceObject(index, kOSBooleanFalse);
            IOLockUnlock(_queueLock);
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
IOHIDLibUserClient::processElement(IOHIDElementValue *element, IOHIDReportElementQueue *queue)
{
    IOReturn status = kIOReturnNoMemory;
    bool ret;
    mach_vm_address_t reportAddress;
    IOHIDElementOOBValue oobElement;
    UInt32 reportSize = ELEMENT_VALUE_REPORT_SIZE(element);

    if (reportSize < fReportLimit) {
        // Align to dword
        uint32_t enqueueSize = ALIGN_DATA_SIZE(element->totalSize);
        ret = handleEnqueue((void*)element, enqueueSize, queue);
        return ret ? kIOReturnSuccess : kIOReturnNoMemory;
    }

    IOHIDOOBReportDescriptor * md = IOHIDOOBReportDescriptor::inTaskWithBytes(fClient, element->value, reportSize, kIODirectionInOut);
    require_action(md, exit, HIDLibUserClientLogError("Unable to create large report descriptor. Dropping large report."));

    reportAddress = (mach_vm_address_t)md->getBytesNoCopy();
    require_action(reportAddress, exit, HIDLibUserClientLogError("Unable to create large report map in user task. Dropping large report."));

    queue_enter(&fReportList, md, IOHIDOOBReportDescriptor *, qc);
    md->retain();

    memcpy(&oobElement, element, ELEMENT_VALUE_HEADER_SIZE(element));
    memcpy(&oobElement.address, &reportAddress, sizeof(reportAddress));
    oobElement.flags |= kIOHIDElementValueOOBReport;
    ret = handleEnqueue((void*)&oobElement, sizeof(oobElement), queue);
    require_action(ret, exit, releaseReport(reportAddress); HIDLibUserClientLogError("Failed to enqueue oversized report."));

    status = kIOReturnSuccess;
exit:
    OSSafeReleaseNULL(md);
    return status;
}

Boolean
IOHIDLibUserClient::handleEnqueue(void *queueData, UInt32 dataSize, IOHIDReportElementQueue *queue)
{
    Boolean status = false;

    if (!queue->pendingReports()) {
        status = queue->enqueue(queueData, dataSize);
    }

    if (status == false && !canDropReport() && fClientOpened && !fClientSeized) {
        assert(fWL->inGate());
        queue->setPendingReports();
        IOHIDBlockedReport reportStruct;
        queue_enter(&fBlockedReports, &reportStruct, IOHIDBlockedReport*, qc);
        fGate->commandSleep(&reportStruct);
        while(!canDropReport() && fClientOpened && !fClientSeized) {
            status = queue->enqueue(queueData, dataSize);

            if (status == true) {
                break;
            }

            fGate->commandSleep(&reportStruct);
        }
        queue_remove(&fBlockedReports, &reportStruct, IOHIDBlockedReport *, qc);
        if (!queue_empty(&fBlockedReports)) {
            fGate->commandWakeup(queue_first(&fBlockedReports));
        } else {
            queue->clearPendingReports();
            if (!fClientOpened) {
                HIDLibUserClientLog("Waking close thread");
                fGate->commandWakeup(&fBlockedReports);
            }
        }
    }

    return status;
}

bool
IOHIDLibUserClient::canDropReport()
{
    // kIOHIDOptionsPrivateTypeNoDroppedReports must be used to prevent dropping reports
    if (!(fCachedOptionBits & kIOHIDOptionsPrivateTypeNoDroppedReports)) {
        return true;
    }

    if (fClientSuspended && !(fCachedOptionBits & kIOHIDOptionsPrivateTypeIgnoreTaskSuspend)) {
        return true;
    }

    return false;
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
    IOHIDOOBReportDescriptor * md = NULL;
    IOHIDOOBReportDescriptor * tmp = NULL;

    queue_iterate(&fReportList, tmp, IOHIDOOBReportDescriptor *, qc) {
        if ((mach_vm_address_t)tmp->getBytesNoCopy() == reportToken || (tmp->mapping && tmp->mapping->getAddress() == reportToken)) {
            md = tmp;
            break;
        }
    }

    require_action(md, exit, HIDLibUserClientLogError("Unable to find report descriptor for address %#llx", reportToken));
    queue_remove(&fReportList, md, IOHIDOOBReportDescriptor *, qc);

    OSSafeReleaseNULL(md->mapping);
    OSSafeReleaseNULL(md);
exit:
    return;
}

IOReturn
IOHIDLibUserClient::_resumeReports(IOHIDLibUserClient *target,
                                   void *reference,
                                   IOExternalMethodArguments *arguments)
{
    target->resumeReports();
    return kIOReturnSuccess;
}

void
IOHIDLibUserClient::resumeReports()
{
    if (!queue_empty(&fBlockedReports)) {
        fGate->commandWakeup(queue_first(&fBlockedReports));
    }
}
