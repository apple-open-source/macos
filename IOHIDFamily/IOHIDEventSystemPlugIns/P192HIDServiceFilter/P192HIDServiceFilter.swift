/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * This document is the property of Apple Inc.
 * It is considered confidential and proprietary.
 *
 * This document may not be reproduced or transmitted in any form,
 * in whole or in part, without the express written permission of
 * Apple Inc.
 */

import HID_Private
import HID
import IOKit_Private
import OSLog

internal extension Logger {
    static let logger = Logger(subsystem: "com.apple.iohid", category: "P192HIDServiceFilter")
}

@objc(P192HIDServiceFilter) public class P192HIDServiceFilter: NSObject, HIDServiceFilter
{
    private var service: HIDEventService?

    private var queue: DispatchQueue?
    private var cancelHandler: HIDBlock?

    internal static var logger: Logger { Logger.filter }
    private var logger: Logger { Self.logger }

    /// Flag to track whether time-sync is active and successfully converting timestamps.
    ///
    /// There are a few known issues with time-sync. To avoid log spam during this time, this flag
    /// is used to log only the first error which occurs after a previous successful timesync.
    private var timeSyncLocked: Bool

    public override var description: String
    {
        "\(Self.self)"
    }

    // P192 Input HID Event Topology
    //
    // +-o Collection Event
    //   +-o Button 1 (Tip) Event
    //   | +-o Vendor Defined Event
    //   +-o Button 4 (Barrel Secondary) Event
    //   | +-o Vendor Defined Event
    //   | +-o Proximity (Touch) Event
    //   +-o Button 3 (Barrel Primary) Event
    //   | +-o Proximity (Touch) Event
    //   +-o Button 2 (End) Event
    //   +-o Collection Event (kHIDPage_Sensor, kHIDUsage_Snsr_Motion_Accelerometer3D)
    //   | +-o Accelerometer Event
    //   | | +-o Vendor Defined Event (kHIDPage_AppleVendorSensor, kHIDUsage_AppleVendorSensor_DeviceTimestamp)
    //   | | +-o Vendor Defined Event (kHIDPage_AppleVendorSensor, kHIDUsage_AppleVendorSensor_TimeSyncTimestamp)
    //   | +-o Accelerometer Event
    //   | | +-o Vendor Defined Event (kHIDPage_AppleVendorSensor, kHIDUsage_AppleVendorSensor_DeviceTimestamp)
    //   | | +-o Vendor Defined Event (kHIDPage_AppleVendorSensor, kHIDUsage_AppleVendorSensor_TimeSyncTimestamp)
    //   | +-o Accelerometer Event
    //   |   +-o Vendor Defined Event (kHIDPage_AppleVendorSensor, kHIDUsage_AppleVendorSensor_DeviceTimestamp)
    //   |   +-o Vendor Defined Event (kHIDPage_AppleVendorSensor, kHIDUsage_AppleVendorSensor_TimeSyncTimestamp)
    //   +-o Collection Event (kHIDPage_Sensor, kHIDUsage_Snsr_Motion_Gyrometer3D)
    //     +-o Gyro Event
    //     | +-o Vendor Defined Event (kHIDPage_AppleVendorSensor, kHIDUsage_AppleVendorSensor_DeviceTimestamp)
    //     | +-o Vendor Defined Event (kHIDPage_AppleVendorSensor, kHIDUsage_AppleVendorSensor_TimeSyncTimestamp)
    //     +-o Gyro Event
    //     | +-o Vendor Defined Event (kHIDPage_AppleVendorSensor, kHIDUsage_AppleVendorSensor_DeviceTimestamp)
    //     | +-o Vendor Defined Event (kHIDPage_AppleVendorSensor, kHIDUsage_AppleVendorSensor_TimeSyncTimestamp)
    //     +-o Gyro Event
    //       +-o Vendor Defined Event (kHIDPage_AppleVendorSensor, kHIDUsage_AppleVendorSensor_DeviceTimestamp)
    //       +-o Vendor Defined Event (kHIDPage_AppleVendorSensor, kHIDUsage_AppleVendorSensor_TimeSyncTimestamp)


// MARK: HIDServiceFilter

    public static func match(
        _ service: HIDEventService,
        options: [AnyHashable : Any]? = nil,
        score: UnsafeMutablePointer<Int>
    ) -> Bool
    {
        return true;
    }

    public required init?(service: HIDEventService)
    {
        self.service = service
        self.timeSyncLocked = false;
        super.init()
    }

    public func setCancelHandler(_ cancelHandler: @escaping HIDBlock)
    {
        self.cancelHandler = cancelHandler
    }

    public func setDispatchQueue(_ queue: DispatchQueue)
    {
        self.queue = queue
    }

    public func activate()
    {

    }

    public func cancel()
    {
        if let handler = self.cancelHandler {
            self.queue!.async { handler() }
        }
    }

    public func property(forKey key: String, client: HIDConnection?) -> Any?
    {
        return nil;
    }

    public func setProperty(_ value: Any?, forKey key: String, client: HIDConnection?) -> Bool
    {
        return true;
    }

    public func filterEvent(_ event: HIDEvent) -> HIDEvent?
    {
        guard let children = event.children else {
            return event // nothing to do if no child events
        }

        guard var latestIMUTimestamp = getLatestIMUTimestamp(event) else {
            if timeSyncLocked {
                self.logger.error("Input event with no time-sync timestamps, check time-sync health")
                self.timeSyncLocked = false
            }
            return event // if no synced IMU timestamp is found, return the event unmodified
        }

        if !timeSyncLocked {
            self.logger.log("Time-sync locked")
            self.timeSyncLocked = true
        }

        let inputTimestampEvent = HIDEvent.vendorDefinedEvent(event.timestamp,
                                                              usagePage: UInt16(kHIDPage_AppleVendorSensor),
                                                              usage: UInt16(kHIDUsage_AppleVendorSensor_TimeSyncTimestamp),
                                                              version: 0,
                                                              data: &latestIMUTimestamp,
                                                              length: UInt32(MemoryLayout.size(ofValue:latestIMUTimestamp)),
                                                              options: 0)
        event.append(inputTimestampEvent)

        return event
    }

    public func filterEvent(matching: [AnyHashable : Any]?, event: HIDEvent, forClient client: HIDConnection?) -> HIDEvent?
    {
        return event;
    }

// MARK: Event Processing

    /// Returns the latest time-sync timestamp from all accel/gyro events in a P192 input event.
    ///
    /// - Parameters:
    ///   - event: P192 input event
    ///
    /// - Returns:
    ///   - The latest time-sync timestamp, or `nil` if there are none.
    ///
    func getLatestIMUTimestamp(_ event: HIDEvent) -> UInt64?
    {
        guard let children = event.children else { return nil }

        var latestTimestamp: UInt64? = nil

        for child in children {
            guard child.type == kIOHIDEventTypeCollection else { continue }

            let page = child.integerValue(forField: kIOHIDEventFieldCollectionUsagePage)
            guard page == kHIDPage_Sensor else { continue }

            let usage = child.integerValue(forField: kIOHIDEventFieldCollectionUsage)
            guard usage == kHIDUsage_Snsr_Motion_Accelerometer3D || usage == kHIDUsage_Snsr_Motion_Gyrometer3D else {
                continue
            }

            guard let sensorChildren = child.children else { continue }

            for sensorChild in sensorChildren {
                guard sensorChild.type == kIOHIDEventTypeAccelerometer || sensorChild.type == kIOHIDEventTypeGyro else { continue }

                if let measurementTimestamp = getMeasurementTimestampForEvent(sensorChild) {
                    if latestTimestamp == nil || measurementTimestamp > latestTimestamp! {
                        latestTimestamp = measurementTimestamp
                    }
                }
            }
        }

        return latestTimestamp
    }

    /// Returns the time-sync timestamp associated with this event, if there is one.
    ///
    /// - Parameters:
    ///   - event: The event to get the time-sync timestamp for.
    ///
    /// - Returns:
    ///     The time-sync timestamp, or `nil` if there is not one.
    ///
    func getMeasurementTimestampForEvent(_ event: HIDEvent) -> UInt64?
    {
        guard let children = event.children else { return nil }

        let expectedSize = MemoryLayout<UInt64>.size

        for child in children {
            guard child.type == kIOHIDEventTypeVendorDefined else { continue }

            let page = child.integerValue(forField: kIOHIDEventFieldVendorDefinedUsagePage)
            guard page == kHIDPage_AppleVendorSensor else { continue }

            let usage = child.integerValue(forField: kIOHIDEventFieldVendorDefinedUsage)
            guard usage == kHIDUsage_AppleVendorSensor_TimeSyncTimestamp else { continue }

            let size = child.integerValue(forField: kIOHIDEventFieldVendorDefinedDataLength)
            guard size == expectedSize else { continue }

            return child.dataValue(forField: kIOHIDEventFieldVendorDefinedData).load(as: UInt64.self)
        }

        return nil
    }
}
