/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 *
 * This document is the property of Apple Inc.
 * It is considered confidential and proprietary.
 *
 * This document may not be reproduced or transmitted in any form,
 * in whole or in part, without the express written permission of
 * Apple Inc.
 */

#pragma once

#include <IOKit/hidevent/IOHIDEventService.h>

class IOHIDElementProcessor;
class IOHIDTimeSyncService;

/// `IOHIDComplexEventDriver` is a HID event driver which produces hierarchical events that reflect
/// the topology of the HID report descriptor. Collections in the descriptor group related items
/// that produce HID events, and nested collections produce child events.
///
class IOHIDComplexEventDriver : public IOHIDEventService
{
    OSDeclareDefaultStructors(IOHIDComplexEventDriver);
    using super = IOHIDEventService;

protected:

    virtual bool handleStart(IOService * provider) APPLE_KEXT_OVERRIDE;
    virtual OSArray * getReportElements() APPLE_KEXT_OVERRIDE;

    /// Asynchronous report handler. Dispatches HID events corresponding to the input elements in
    /// the handled report.
    ///
    /// @param  timestamp
    ///     Report timestamp.
    ///
    /// @param  report
    ///     Report payload.
    ///
    /// @param  type
    ///     Report type.
    ///
    /// @param  reportID
    ///     Report ID.
    ///
    virtual void handleInterruptReport(AbsoluteTime timestamp, IOMemoryDescriptor * report, IOHIDReportType type, UInt32 reportID);

    /// Convert a device timestamp to the corresponding mach (absolute) timestamp.
    ///
    /// @param  timestamp
    ///     Timestamp to convert, in device representation
    ///
    /// @param[out] outTime
    ///     Output storage for result.
    ///
    /// @return
    ///     - `kIOReturnSuccess` on success
    ///     - `kIOReturnUnsupported` if the device does not support time-sync
    ///     - `kIOReturnNotReady` if time-sync is not yet active
    ///     - `kIOReturnOffline` if the driver or time sync service has terminated
    ///     - Other return codes are propagated from the underlying time sync service.
    ///
    IOReturn convertDeviceToMachTimestamp(OSData * timestamp, UInt64 * outTime);

    /// Convert a mach (absolute) timestamp to the device representation.
    ///
    /// @param  timestamp
    ///     Timestamp to convert, in mach absolute time.
    ///
    /// @param  outTime
    ///     Output storage for result. Caller takes ownership and is responsible for freeing the
    ///     object on success.
    ///
    /// @return
    ///     - `kIOReturnSuccess` on success
    ///     - `kIOReturnUnsupported` if the device does not support time-sync
    ///     - `kIOReturnNotReady` if time-sync is not yet active
    ///     - `kIOReturnOffline` if the driver or time sync service has terminated
    ///     - Other return codes are propagated from the underlying time sync service.
    ///
    IOReturn convertMachToDeviceTimestamp(AbsoluteTime timestamp, OSData ** outTime);

public:

    virtual bool willTerminate(IOService * provider, IOOptionBits options) APPLE_KEXT_OVERRIDE;
    virtual IOReturn setProperties(OSObject * properties) APPLE_KEXT_OVERRIDE;

private:

    /// Perform driver cleanup.
    void cleanupHelper();

    /// Calls `parseCollection` on the root collection element during driver setup to build the tree
    /// of element processors for the HID device. Once it returns, all processors are stored in
    /// `_processors` and parent-child relationships are established.
    ///
    /// A processor is always created for the top-level application collection. If no others are
    /// created (i.e., `_processors` has length 1), no supported collections were found and the
    /// driver fails to start.
    ///
    void initProcessors();

    /// Recursively parse the collection elements in a report descriptor, creating element
    /// processors where supported.
    ///
    /// @param  collection
    ///     Collection element
    ///
    /// @param  parent
    ///     Processor which should be the parent of any processors created for this collection.
    ///
    void parseCollection(IOHIDElement * collection, IOHIDElementProcessor * parent = nullptr);

    /// Create element processors for a collection element.
    ///
    /// @param  collection
    ///     Collection element.
    ///
    /// @return
    ///     The number of processors created.
    ///
    unsigned int createProcessors(IOHIDElement * collection, IOHIDElementProcessor * parent = nullptr);
    unsigned int createRootProcessor(IOHIDElement * collection);

    // control properties
    bool isValidProcessorPropertyRequest(OSObject * object);
    void handleSetProcessorPropertyGated(OSObject * object);
    IOHIDElementProcessor * getProcessor(unsigned int cookie) const;

    IOReturn dispatchWorkloopSync(IOEventSource::ActionBlock action);

    OSPtr<IOWorkLoop> _workloop; ///< provider workloop
    OSPtr<IOHIDInterface> _interface; ///< provider
    OSPtr<OSArray> _elements; ///< HID report elements

    OSPtr<IOHIDElementProcessor> _rootProcessor; ///< root collection element processor
    OSPtr<OSArray> _processors; ///< list of all element processors


    // MARK: Time-Sync Support

    thread_call_t _serviceMatchThread; ///< thread call to handle time-sync service match notifications
    OSPtr<IONotifier> _notifier; ///< service match notifier
    OSPtr<IOHIDTimeSyncService> _timeSync; ///< time-sync service

    /// State flags used to ensure thread safety of the driver.
    ///
    enum TimeSyncState {
        kTimeSyncStateMatched = 1 << 0, ///< a matching time-sync service has been registered.
        kTimeSyncStateOpened  = 1 << 1, ///< the driver has become a client of the time-sync service.
        kTimeSyncStateActive  = 1 << 2, ///< time sync is active
    };
    os_atomic(UInt32) _timeSyncState; ///< current time-sync state

    bool _timeSyncSupported; ///< does the device support time-sync?

    /// Is the time-sync service associated with the same device as this driver?
    bool sharesHIDDeviceWith(IOHIDTimeSyncService * service) const;

    /// Setup time-sync support.
    void setupTimeSync();

    /// Handler methods for matched time-sync service.
    void timeSyncServiceMatchHandler(thread_call_param_t param);
    void timeSyncServiceMatchHandlerGated();
    void handleTimeSyncActive();
    void handleTimeSyncInactive();

    /// Do time-sync for the provided event, if supported
    void performTimeSync(IOHIDEvent * event);
    void performTimeSyncGated(IOHIDEvent * event);

    // MARK: Debug

    struct {
        struct {
            UInt64 notOpenCnt; ///< number of calls to time-sync methods before the time-sync driver is opened
            UInt64 notActiveCnt; ///< number of calls to time-sync methods before the time-sync became active
            UInt64 toLocalCnt; ///< number of successful calls to sync remote timestamp to local
            UInt64 toRemoteCnt; ///< number of successful calls to sync local timestamp to remote
        } ts;
    } _debug;

    friend class IOFastPathHIDService;
};
