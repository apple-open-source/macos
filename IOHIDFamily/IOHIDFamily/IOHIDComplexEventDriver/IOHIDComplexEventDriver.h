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

public:

    virtual bool didTerminate(IOService * provider, IOOptionBits options, bool * defer) APPLE_KEXT_OVERRIDE;
    virtual IOReturn setProperties(OSObject * properties) APPLE_KEXT_OVERRIDE;
    virtual void free() APPLE_KEXT_OVERRIDE;

private:

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
    OSPtr<IOCommandGate> _gate; ///< gate for synchronization on provider workloop
    OSPtr<IOHIDInterface> _interface; ///< provider
    OSPtr<OSArray> _elements; ///< HID report elements

    OSPtr<IOHIDElementProcessor> _rootProcessor; ///< root collection element processor
    OSPtr<OSArray> _processors; ///< list of all element processors
};
