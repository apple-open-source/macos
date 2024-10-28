/*
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 2018 Apple, Inc.  All Rights Reserved.
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

#include "AppleUserHIDDevice.h"
#include "IOHIDResourceUserClient.h"
#include "IOHIDKeys.h"
#include "IOHIDPrivateKeys.h"
#include "IOHIDParameter.h"
#include <IOKit/IOLib.h>
#include "IOHIDInterface.h"
#include <AssertMacros.h>
#include "IOHIDDebug.h"
#include <IOKit/IOCommand.h>
#include "IOHIDFamilyPrivate.h"
#include <IOKit/IOKitKeys.h>
#include <stdatomic.h>

#define kIOHIDDeviceDextEntitlement "com.apple.developer.driverkit.family.hid.device"

#define super IOHIDDevice

OSDefineMetaClassAndStructors(AppleUserHIDDevice, IOHIDDevice)

#pragma mark -
#pragma mark Methods

#define HIDDeviceLogFault(fmt, ...)   HIDLogFault("%s:0x%llx " fmt "\n", getName(), getRegistryEntryID(), ##__VA_ARGS__)
#define HIDDeviceLogError(fmt, ...)   HIDLogError("%s:0x%llx " fmt "\n", getName(), getRegistryEntryID(), ##__VA_ARGS__)
#define HIDDeviceLog(fmt, ...)        HIDLog("%s:0x%llx " fmt "\n", getName(), getRegistryEntryID(), ##__VA_ARGS__)
#define HIDDeviceLogInfo(fmt, ...)    HIDLogInfo("%s:0x%llx " fmt "\n", getName(), getRegistryEntryID(), ##__VA_ARGS__)
#define HIDDeviceLogDebug(fmt, ...)   HIDLogDebug("%s:0x%llx " fmt "\n", getName(), getRegistryEntryID(), ##__VA_ARGS__)




#define _workLoop                       ivar->workLoop
#define _commandGate                    ivar->commandGate
#define _syncActions                    ivar->syncActions
#define _asyncActions                   ivar->asyncActions
#define _asyncActionsLock               ivar->asyncActionsLock
#define _state                          ivar->state
#define _requestTimeout                 ivar->requestTimeout
#define _setReportCount                 ivar->setReportCount
#define _setReportFailCount             ivar->setReportFailCount
#define _setReportTimeoutCount          ivar->setReportTimeoutCount
#define _setReportTime                  ivar->setReportTime
#define _getReportCount                 ivar->getReportCount
#define _getReportFailCount             ivar->getReportFailCount
#define _getReportTimeoutCount          ivar->getReportTimeoutCount
#define _getReportTime                  ivar->getReportTime
#define _inputReportCount               ivar->inputReportCount
#define _inputReportTime                ivar->inputReportTime

#define dispatch_workloop_sync_with_ret(b)                        \
        _commandGate ? _commandGate->runActionBlock(^IOReturn{    \
            if (isInactive()) {                     \
                return kIOReturnOffline;            \
            };                                      \
            b                                       \
            return kIOReturnSuccess;                \
        }) : kIOReturnOffline


enum {
    kAppleUserHIDDeviceStateStarted     = 1,
    kAppleUserHIDDeviceStateStopped     = 2,
    kAppleUserHIDDeviceStateKRStart     = 4,
    kAppleUserHIDDeviceStateDKStart     = 8,
};

struct IOHIDReportCompletion {
    IOMemoryDescriptor      * report;
    IOHIDCompletion         completion;
    IOReturn                status;
    bool                    releaseReport;
};

//----------------------------------------------------------------------------------------------------
// AppleUserHIDDevice::init
//----------------------------------------------------------------------------------------------------
bool AppleUserHIDDevice::init (OSDictionary * dict)
{
    bool ret = false;

    require (super::init(dict), exit);
    
    ivar = IOMallocType(AppleUserHIDDevice::AppleUserHIDDevice_IVars);
    require (ivar, exit);

    bzero(ivar, sizeof(AppleUserHIDDevice::AppleUserHIDDevice_IVars));
    
    _syncActions = OSSet::withCapacity(2);
    require_action (_syncActions, exit, HIDDeviceLogError("syncActions\n"));

    _asyncActions = OSSet::withCapacity(1);
    require_action (_asyncActions, exit, HIDDeviceLogError("asyncActions\n"));

    _asyncActionsLock = IOLockAlloc();
    require_action (_asyncActionsLock, exit, HIDDeviceLogError("asyncActionsLock\n"));

    super::setProperty(kIOServiceDEXTEntitlementsKey, kIOHIDDeviceDextEntitlement);
    
    super::setProperty(kIOHIDRegisterServiceKey, kOSBooleanFalse);
    
    ret = true;
    
exit:

    return ret;
}

//----------------------------------------------------------------------------------------------------
// AppleUserHIDDevice::probe
//----------------------------------------------------------------------------------------------------
IOService * AppleUserHIDDevice::probe(IOService *provider, SInt32 *score)
{
    if (isSingleUser()) {
        return NULL;
    }
    

#if TARGET_OS_VISION && RELEASE
    return NULL;
#else
    return super::probe(provider, score);
#endif
}

//----------------------------------------------------------------------------------------------------
// AppleUserHIDDevice::willTerminate
//----------------------------------------------------------------------------------------------------
bool AppleUserHIDDevice::willTerminate( IOService *provider,
                                IOOptionBits options )
{
    messageClients(kIOHIDDeviceWillTerminate);

    return super::willTerminate(provider, options);
}

//----------------------------------------------------------------------------------------------------
// AppleUserHIDDevice::start
//----------------------------------------------------------------------------------------------------
bool AppleUserHIDDevice::start(IOService * provider)
{
    IOReturn ret    = kIOReturnSuccess;
    bool     status = false;
   
    bool dkStart = getProperty(kIOHIDDKStartKey) == kOSBooleanTrue;
    
    HIDDeviceLog("start (state:0x%x)", _state);

    bool krStart = ((atomic_fetch_or((_Atomic UInt32 *)&_state, kAppleUserHIDDeviceStateKRStart) &  kAppleUserHIDDeviceStateKRStart) == 0);
   
    if (dkStart &&
        (atomic_fetch_or((_Atomic UInt32 *)&_state, kAppleUserHIDDeviceStateDKStart) &  kAppleUserHIDDeviceStateDKStart)) {
        //attemt to call kernel multiple times
        HIDDeviceLogError("Attempt to do kernel start for device multiple times");
        return kIOReturnError;
    }
   
    if (!dkStart) {
        ret = Start(provider);
        require_noerr_action(ret, exit, HIDDeviceLogError("IOHIDDevice::Start:0x%x\n", ret));
    } else if (krStart) {
        status = super::start(provider);
        require_action(status, exit, HIDDeviceLogError("super::start:0x%x\n", status));
    } else {
        return super::start(provider);
    }
    
    atomic_fetch_or((_Atomic UInt32 *)&_state, kAppleUserHIDDeviceStateStarted);

    status = true;

exit:
    
    if (_state & kAppleUserHIDDeviceStateStarted && !status) {
        stop(provider);
    }
    return status;
}


//----------------------------------------------------------------------------------------------------
// AppleUserHIDDevice::stop
//----------------------------------------------------------------------------------------------------
void AppleUserHIDDevice::stop(IOService * provider)
{
    _state |= kAppleUserHIDDeviceStateStopped;
    
    super::stop(provider);
}


//----------------------------------------------------------------------------------------------------
// AppleUserHIDDevice::free
//----------------------------------------------------------------------------------------------------
void AppleUserHIDDevice::free()
{

    if (ivar) {
        OSSafeReleaseNULL(_syncActions);
        OSSafeReleaseNULL(_asyncActions);
        OSSafeReleaseNULL(_commandGate);
        OSSafeReleaseNULL(_workLoop);
        if (_asyncActionsLock) {
            IOLockFree(_asyncActionsLock);
        }
        IOFreeType(ivar, AppleUserHIDDevice::AppleUserHIDDevice_IVars);
    }
    super::free();
}
//----------------------------------------------------------------------------------------------------
// AppleUserHIDDevice::handleStart
//----------------------------------------------------------------------------------------------------
bool AppleUserHIDDevice::handleStart( IOService * provider )
{
    bool           status           = false;
    OSSerializer * debugSerializer  = NULL;
    OSNumber     * value            = NULL;

    status = super::handleStart(provider);
    require_action(status, exit, HIDDeviceLogError("handleStart"));
    
    _workLoop = getWorkLoop();
    require_action (_workLoop, exit,  HIDDeviceLogError("workLoop\n"));
    _workLoop->retain();

    _commandGate = IOCommandGate::commandGate(this);
    require_action(_commandGate && _workLoop->addEventSource(_commandGate) == kIOReturnSuccess, exit, HIDDeviceLogError("_commandGate"));

    value = OSDynamicCast(OSNumber, getProperty(kIOHIDRequestTimeoutKey));
    _requestTimeout = value && value->unsigned32BitValue() >= kIOHIDDeviceMinAsyncRequestTimeout * 1000 && value->unsigned32BitValue() <= kIOHIDDeviceMaxAsyncRequestTimeout * 1000 ? value->unsigned32BitValue() : kIOHIDDeviceDefaultAsyncRequestTimeout * 1000;
    
    debugSerializer = OSSerializer::forTarget(this,
                                              OSMemberFunctionCast(OSSerializerCallback,
                                                                   this,
                                                                   &AppleUserHIDDevice::serializeDebugState));
    require(debugSerializer, exit);
    super::setProperty("DebugState", debugSerializer);

    status = true;

exit:
    
    OSSafeReleaseNULL(debugSerializer);

    return status;
}

//----------------------------------------------------------------------------------------------------
// AppleUserHIDDevice::handleStop
//----------------------------------------------------------------------------------------------------
void AppleUserHIDDevice::handleStop(  IOService * provider )
{
    HIDDeviceLogInfo("handleStop src: %d srfc: %d srtc: %d srt: %llu grc: %d grfc: %d grtc: %d grt: %llu irc: %d irt: %llu",
                     _setReportCount, _setReportFailCount, _setReportTimeoutCount, _setReportTime,
                     _getReportCount, _getReportFailCount, _getReportTimeoutCount, _getReportTime,
                     _inputReportCount, _inputReportTime);
    
    if (_workLoop) {
        if ( _commandGate ) {
            if (_syncActions) {
                uint32_t count = _syncActions->getCount();
                _syncActions->iterateObjects(^bool(OSObject *object) {
                    OSAction * action = OSDynamicCast(OSAction, object);
                    _commandGate->commandWakeup(action);
                    return false;
                });
                _syncActions->flushCollection();
                
                while (count) {
                    HIDDeviceLogInfo("handleStop sleeping %d", count);
                    _commandGate->commandSleep(&_syncActions);
                    count--;
                }
            }
            _workLoop->removeEventSource(_commandGate);
        }
    }
    if (_asyncActions) {
        OSSet * tmp = OSSet::withSet(_asyncActions);
        if (tmp) {
            tmp->iterateObjects(^bool(OSObject *object) {
                OSAction * action = OSDynamicCast(OSAction, object);
                completeReport(action, kIOReturnAborted, 0);
                return false;
            });
            tmp->release();
        }
    }
    super::handleStop(provider);
}

//----------------------------------------------------------------------------------------------------
// AppleUserHIDDevice::newTransportString
//----------------------------------------------------------------------------------------------------
OSString * AppleUserHIDDevice::newTransportString() const
{
    OSString * string = OSDynamicCast(OSString, getProperty (kIOHIDTransportKey));
    
    if (!string) {
        return NULL;
    }
    
    string->retain();
    
    return string;
}

//----------------------------------------------------------------------------------------------------
// AppleUserHIDDevice::newManufacturerString
//----------------------------------------------------------------------------------------------------
OSString * AppleUserHIDDevice::newManufacturerString() const
{
    OSString * string = OSDynamicCast(OSString, getProperty(kIOHIDManufacturerKey));
    
    if (!string) {
        return NULL;
    }
    
    string->retain();
    
    return string;
}

//----------------------------------------------------------------------------------------------------
// AppleUserHIDDevice::newProductString
//----------------------------------------------------------------------------------------------------
OSString * AppleUserHIDDevice::newProductString() const
{
    OSString * string = OSDynamicCast(OSString, getProperty(kIOHIDProductKey));
    
    if (!string) {
        return NULL;
    }
    
    string->retain();
    
    return string;
}

//----------------------------------------------------------------------------------------------------
// AppleUserHIDDevice::newVendorIDNumber
//----------------------------------------------------------------------------------------------------
OSNumber * AppleUserHIDDevice::newVendorIDNumber() const
{
    OSNumber * number = OSDynamicCast(OSNumber, getProperty(kIOHIDVendorIDKey));
    
    if ( !number )
        return NULL;
    
    number->retain();
    
    return number;
}

//----------------------------------------------------------------------------------------------------
// AppleUserHIDDevice::newProductIDNumber
//----------------------------------------------------------------------------------------------------
OSNumber * AppleUserHIDDevice::newProductIDNumber() const
{
    OSNumber * number = OSDynamicCast(OSNumber, getProperty(kIOHIDProductIDKey));
    
    if (!number) {
        return NULL;
    }
    
    number->retain();
    
    return number;
}

//----------------------------------------------------------------------------------------------------
// AppleUserHIDDevice::newVersionNumber
//----------------------------------------------------------------------------------------------------
OSNumber * AppleUserHIDDevice::newVersionNumber() const
{
    OSNumber * number = OSDynamicCast(OSNumber, getProperty(kIOHIDVersionNumberKey));
    
    if (!number) {
        return NULL;
    }
    
    number->retain();
    
    return number;
}

//----------------------------------------------------------------------------------------------------
// AppleUserHIDDevice::newSerialNumberString
//----------------------------------------------------------------------------------------------------
OSString * AppleUserHIDDevice::newSerialNumberString() const
{
    OSString * string = OSDynamicCast(OSString, getProperty(kIOHIDSerialNumberKey));
    
    if (!string) {
        return NULL;
    }
    
    string->retain();
    
    return string;
}

//----------------------------------------------------------------------------------------------------
// AppleUserHIDDevice::newVendorIDSourceNumber
//----------------------------------------------------------------------------------------------------
OSNumber * AppleUserHIDDevice::newVendorIDSourceNumber() const
{
    OSNumber * number = OSDynamicCast(OSNumber, getProperty(kIOHIDVendorIDSourceKey));
    
    if ( !number )
        return NULL;
    
    number->retain();
    
    return number;
}

//----------------------------------------------------------------------------------------------------
// AppleUserHIDDevice::newCountryCodeNumber
//----------------------------------------------------------------------------------------------------
OSNumber * AppleUserHIDDevice::newCountryCodeNumber() const
{
    OSNumber * number = OSDynamicCast(OSNumber, getProperty(kIOHIDCountryCodeKey));
    
    if (!number) {
        return NULL;
    }
    
    number->retain();
    
    return number;
}

//----------------------------------------------------------------------------------------------------
// AppleUserHIDDevice::newReportIntervalNumber
//----------------------------------------------------------------------------------------------------
OSNumber * AppleUserHIDDevice::newReportIntervalNumber() const
{
    OSNumber * number = OSDynamicCast(OSNumber, getProperty(kIOHIDReportIntervalKey));
    
    if ( !number ) {
        number = IOHIDDevice::newReportIntervalNumber();
    } else {
        number->retain();
    }
    
    return number;
}

//----------------------------------------------------------------------------------------------------
// AppleUserHIDDevice::newLocationIDNumber
//----------------------------------------------------------------------------------------------------
OSNumber * AppleUserHIDDevice::newLocationIDNumber() const
{
    OSNumber * number = OSDynamicCast(OSNumber, getProperty(kIOHIDLocationIDKey));
    
    if (!number) {
        return NULL;
    }
    
    number->retain();
    
    return number;
}

//----------------------------------------------------------------------------------------------------
// AppleUserHIDDevice::newReportDescriptor
//----------------------------------------------------------------------------------------------------
IOReturn AppleUserHIDDevice::newReportDescriptor(IOMemoryDescriptor ** descriptor ) const
{
    OSData *                    data;
    
    data = OSDynamicCast(OSData, getProperty(kIOHIDReportDescriptorKey));
    if (!data) {
        return kIOReturnError;
    }
    
    *descriptor = IOBufferMemoryDescriptor::withBytes(data->getBytesNoCopy(), data->getLength(), kIODirectionNone);
    
    return kIOReturnSuccess;
}

//----------------------------------------------------------------------------------------------------
// AppleUserHIDDevice::setProperty
//----------------------------------------------------------------------------------------------------
bool AppleUserHIDDevice::setProperty(const OSSymbol * aKey, OSObject * anObject)
{
    bool                        status      = false;
    OSSerialize                 * s         = NULL;
    OSDictionary                * dict      = NULL;
    IOBufferMemoryDescriptor    * md        = NULL;
    
    
    require (_state & kAppleUserHIDDeviceStateStarted, exit);
    
    //optimization, skip sending property set by registerInterestForNotifier
    require (OSDynamicCast(IOCommand, anObject) == NULL, exit);
    
    require_action (!isInactive(), exit, HIDDeviceLogError("inactive"));

    dict = OSDictionary::withObjects((const OSObject **) &anObject, &aKey, 1);
    
    s = OSSerialize::binaryWithCapacity(4096);
    require_action (s, exit, HIDDeviceLogError("OSSerialize"));
    s->setIndexed(true);
    
    require_action (dict->serialize(s), exit, HIDDeviceLogError("serialize"));
    
    require_action(s->text() && s->getLength(), exit, HIDDeviceLogError("serialize"));
    
    md = IOBufferMemoryDescriptor::withOptions(kIODirectionOutIn |
                                               kIOMemoryKernelUserShared |
                                               kIOMemoryThreadSafe,
                                               s->getLength());
    require_action (md, exit, HIDDeviceLogError("IOBufferMemoryDescriptor"));
    
    bcopy(s->text(), md->getBytesNoCopy(), s->getLength());
   
    _SetProperty(md);
    
exit:
    
    status = super::setProperty(aKey, anObject);

    OSSafeReleaseNULL(md);
    OSSafeReleaseNULL(s);
    OSSafeReleaseNULL(dict);

    return status;
}

//----------------------------------------------------------------------------------------------------
// AppleUserHIDDevice::setProperties
//----------------------------------------------------------------------------------------------------
IOReturn AppleUserHIDDevice::setProperties(OSObject * properties)
{
    IOReturn  result = kIOReturnBadArgument;
    
    OSDictionary * propertyDict = OSDynamicCast(OSDictionary, properties);
    require_action (propertyDict, exit, HIDDeviceLogError("invalid object type"));
    
    propertyDict->iterateObjects (^bool(const OSSymbol * key, OSObject * object) {
        if (IsIOHIDRestrictedIOKitProperty(key)) {
                HIDDeviceLogError("Ignore porperty for key %s", key->getCStringNoCopy());
                return false;
            }
            
        super::setProperty(key, object);
        return false;
    });
    
    
    result = kIOReturnSuccess;

exit:
    
    return result;
}

//----------------------------------------------------------------------------------------------------
// AppleUserHIDDevice::getReport
//----------------------------------------------------------------------------------------------------
IOReturn AppleUserHIDDevice::getReport(IOMemoryDescriptor   * report,
                                    IOHIDReportType         reportType,
                                    IOOptionBits            options,
                                    UInt32                  completionTimeout,
                                    IOHIDCompletion         * completion)
{
    IOReturn ret;
    
    require_action (report, exit, ret = kIOReturnBadArgument);

    require_action (isInactive() == false, exit, ret = kIOReturnOffline);

    if (completion && completion->action) {
        ret = processReport (kIOHIDReportCommandGetReport, report, reportType, options, completionTimeout, completion);
    } else {
        ret = dispatch_workloop_sync_with_ret({
                return processReport (kIOHIDReportCommandGetReport, report, reportType, options, completionTimeout, completion);
                });
    }
    
    
exit:
    return ret;
}

//----------------------------------------------------------------------------------------------------
// AppleUserHIDDevice::setReport
//----------------------------------------------------------------------------------------------------
IOReturn AppleUserHIDDevice::setReport(IOMemoryDescriptor  * report,
                                       IOHIDReportType     reportType,
                                       IOOptionBits        options,
                                       UInt32              completionTimeout,
                                       IOHIDCompletion     * completion)
{
    IOReturn ret = kIOReturnIOError;
    bool prepared = false;
    IOBufferMemoryDescriptor *memBuff = NULL;

    require_action (report, exit, ret = kIOReturnBadArgument);
    
    require_action (isInactive() == false, exit, ret = kIOReturnOffline);

    // Create IOBufferMemoryDescriptor with correct options for access in the DK HIDDevice driver.
    if (OSDynamicCast(IOBufferMemoryDescriptor, report) == NULL || ((IOBufferMemoryDescriptor*)report)->getCapacity() < PAGE_SIZE) {
        require_noerr_action(report->prepare(), exit, HIDDeviceLogError("Unable to prepare report for setReport."));
        memBuff = IOBufferMemoryDescriptor::withOptions(kIODirectionOutIn |
                                                        kIOMemoryKernelUserShared |
                                                        kIOMemoryThreadSafe,
                                                        report->getLength());

        require_action(memBuff, exit, ret = kIOReturnNoMemory);

        report->readBytes(0, memBuff->getBytesNoCopy(), report->getLength());

        report->complete();

        memBuff->setLength(report->getLength());

        require_noerr_action(memBuff->prepare(), exit, HIDDeviceLogError("Unable to prepare temp report for setReport."));
        prepared = true;
        report = memBuff;
    }

    if (completion && completion->action) {
        ret = processReport (kIOHIDReportCommandSetReport, report, reportType, options, completionTimeout, completion, memBuff != NULL);
    } else {
        ret = dispatch_workloop_sync_with_ret({
                return processReport (kIOHIDReportCommandSetReport, report, reportType, options, completionTimeout, completion);
                });
        if (memBuff) {
            memBuff->complete();
        }
        OSSafeReleaseNULL(memBuff);
    }

exit:
    if (ret != kIOReturnSuccess && memBuff) {
        if (prepared) {
            memBuff->complete();
        }
        OSSafeReleaseNULL(memBuff);
    }
    return ret;
}

//----------------------------------------------------------------------------------------------------
// AppleUserHIDDevice::processReport
//----------------------------------------------------------------------------------------------------
IOReturn AppleUserHIDDevice::processReport(HIDReportCommandType    command,
                                           IOMemoryDescriptor      * report,
                                           IOHIDReportType         reportType,
                                           IOOptionBits            options,
                                           uint32_t                completionTimeout,
                                           IOHIDCompletion         * completion,
                                           bool                    releaseReport)
{
    AbsoluteTime              ts;
    IOReturn                  ret    = kIOReturnSuccess;
    OSAction                * action = NULL;
    IOHIDReportCompletion   * reportCompletion;

    ret = CreateAction_CompleteReport(sizeof(IOHIDReportCompletion), &action);
    require_noerr_action(ret, exit, HIDDeviceLogError("CreateAction_CompleteReport:%x", ret));
    
    reportCompletion                = (IOHIDReportCompletion *) action->GetReference();
    reportCompletion->report        = report;
    reportCompletion->completion    = completion ? *completion : (IOHIDCompletion){NULL, NULL, NULL};
    reportCompletion->releaseReport = releaseReport;
    
    if (completion) {
        IOLockLock(_asyncActionsLock);
        _asyncActions->setObject(action);
        IOLockUnlock(_asyncActionsLock);
    } else {
        _syncActions->setObject(action);
    }
    
    action->SetAbortedHandler(^{
        if (completion) {
            completeReport(action, kIOReturnAborted, 0);
        } else {
            if (_commandGate) {
                _commandGate->runActionBlock(^IOReturn{
                    if (_syncActions->containsObject(action)) {
                        reportCompletion->status = kIOReturnAborted;
                        HIDDeviceLogInfo("Action aborted %d %d", command, reportType);
                        _syncActions->removeObject(action);
                        _commandGate->commandWakeup(action);
                    }
                    return kIOReturnSuccess;
                });
            }
        }
    });
    
    if (command == kIOHIDReportCommandGetReport) {
        _getReportCount++;
        _getReportTime = mach_continuous_time();
    } else {
        _setReportCount++;
        _setReportTime = mach_continuous_time();
    }
    
    _ProcessReport(command, report, reportType, options, completionTimeout, action);
    
    if (completion == NULL || completion->action == NULL) {
        completionTimeout = completionTimeout >= kIOHIDDeviceMinAsyncRequestTimeout && completionTimeout <= kIOHIDDeviceMaxAsyncRequestTimeout ? completionTimeout * 1000 : _requestTimeout;

        clock_interval_to_deadline(completionTimeout, kMicrosecondScale,  &ts);
        ret = _commandGate->commandSleep((void *)action, ts, THREAD_ABORTSAFE);
        if (ret == THREAD_AWAKENED) {
            ret = reportCompletion->status;
        } else  if (ret == THREAD_TIMED_OUT) {
            ret = kIOReturnTimeout;
        } else {
            ret = kIOReturnError;
        }
        
        if (gIOHIDFamilyDtraceDebug()) {
            IOBufferMemoryDescriptor *bmd = OSDynamicCast(IOBufferMemoryDescriptor, report);
            hid_trace(reportType == kIOHIDReportTypeInput ? kHIDTraceGetReport : kHIDTraceSetReport, (uintptr_t)getRegistryEntryID(), (uintptr_t)(options & 0xff), (uintptr_t)report->getLength(), bmd ? (uintptr_t)bmd->getBytesNoCopy() : 0, (uintptr_t)mach_absolute_time());
        }
        
        if (ret) {
            if (ret == kIOReturnTimeout) {
                if (command == kIOHIDReportCommandGetReport) {
                    _getReportTimeoutCount++;
                } else {
                    _setReportTimeoutCount++;
                }
            } else {
                if (command == kIOHIDReportCommandGetReport) {
                    _getReportFailCount++;
                } else {
                    _setReportFailCount++;
                }
            }
            
            HIDDeviceLogError("ProcessReport:0x%x %d %d", ret, reportType, command);
        }
        
        // if the call timed out we need to remove the action from the set
        if (_syncActions->containsObject(action)) {
            _syncActions->removeObject(action);
        }
        
        if (isInactive()) {
            _commandGate->commandWakeup(&_syncActions);
        }
    }
    
exit:
    
    if (action) {
        action->release();
    }
    return ret;
}

//----------------------------------------------------------------------------------------------------
// AppleUserHIDDevice::completeReport
//----------------------------------------------------------------------------------------------------
void AppleUserHIDDevice::completeReport(OSAction * action, IOReturn status, uint32_t actualByteCount)
{
    uint64_t remaining = 0;
    bool async = false;

    require_action (_state & kAppleUserHIDDeviceStateStarted, exit, HIDDeviceLogError("HID Device not ready (state:0x%x)", _state));

    IOHIDReportCompletion *reportCompletion;
    reportCompletion = (IOHIDReportCompletion *)action->GetReference();
    
    IOLockLock(_asyncActionsLock);
    async = _asyncActions->containsObject(action);
    _asyncActions->removeObject(action);
    IOLockUnlock(_asyncActionsLock);
    
    if (async) {
        if (!os_sub_overflow(reportCompletion->report->getLength(), actualByteCount, &remaining)) {
            if (reportCompletion->releaseReport) {
                reportCompletion->report->complete();
                OSSafeReleaseNULL(reportCompletion->report);
            }
            reportCompletion->completion.action(reportCompletion->completion.target,
                                                reportCompletion->completion.parameter,
                                                status,
                                                (uint32_t)remaining);
        } else {
            HIDDeviceLogError("Async report completion dropped, weird size: %u", actualByteCount);
        }

    } else {
        if (_commandGate) {
            _commandGate->runActionBlock(^IOReturn{
                if (_syncActions->containsObject(action)) {
                    reportCompletion->status = status;
                    _syncActions->removeObject(action);
                    _commandGate->commandWakeup((void *)action);
                }
                return kIOReturnSuccess;
            });
        }
    }

exit:
    return;
}

//------------------------------------------------------------------------------
// AppleUserHIDDevice::handleReport
//------------------------------------------------------------------------------
IOReturn AppleUserHIDDevice::handleReportWithTime(AbsoluteTime timeStamp,
                                                  IOMemoryDescriptor *report,
                                                  IOHIDReportType reportType,
                                                  IOOptionBits options)
{
    IOReturn ret = kIOReturnNotReady;
    
    require_action (_state & kAppleUserHIDDeviceStateStarted, exit, HIDDeviceLogError("HID Device not ready (state:0x%x)", _state));
    
    _inputReportCount++;
    _inputReportTime = mach_continuous_time();

    ret = super::handleReportWithTime(timeStamp, report, reportType, options);

exit:

    return ret;
}

#define SET_DICT_NUM(dict, key, val) do { \
    if (val) { \
        OSNumber *num = OSNumber::withNumber(val, 64); \
        if (num) { \
            dict->setObject(key, num); \
            num->release(); \
        } \
    } \
} while (0);

static inline uint64_t calculateDelta(uint64_t a, uint64_t b)
{
    uint64_t result = 0, deltaTime = 0;
    
    if (!a) {
        return 0;
    }
    
    deltaTime = AbsoluteTime_to_scalar(&b) - AbsoluteTime_to_scalar(&a);
    absolutetime_to_nanoseconds(deltaTime, &result);
    
    return result;
}

//------------------------------------------------------------------------------
// AppleUserHIDDevice::serializeDebugState
//------------------------------------------------------------------------------
bool AppleUserHIDDevice::serializeDebugState(void *ref __unused,
                                             OSSerialize *serializer)
{
    bool          result = false;
    OSDictionary  *dict = OSDictionary::withCapacity(4);
    uint64_t      now = mach_continuous_time();
    
    require(dict, exit);
    
    SET_DICT_NUM(dict, "SetReportCount", _setReportCount);
    SET_DICT_NUM(dict, "SetReportFailCount", _setReportFailCount);
    SET_DICT_NUM(dict, "SetReportTimeoutCount", _setReportTimeoutCount);
    SET_DICT_NUM(dict, "SetReportTime", calculateDelta(_setReportTime, now));
    SET_DICT_NUM(dict, "GetReportCount", _getReportCount);
    SET_DICT_NUM(dict, "GetReportFailCount", _getReportFailCount);
    SET_DICT_NUM(dict, "GetReportTimeoutCount", _getReportTimeoutCount);
    SET_DICT_NUM(dict, "GetReportTime", calculateDelta(_getReportTime, now));
    SET_DICT_NUM(dict, "InputReportCount", _inputReportCount);
    SET_DICT_NUM(dict, "InputReportTime", calculateDelta(_inputReportTime, now));
    
    result = dict->serialize(serializer);
    OSSafeReleaseNULL(dict);
    
exit:
    return result;
}
