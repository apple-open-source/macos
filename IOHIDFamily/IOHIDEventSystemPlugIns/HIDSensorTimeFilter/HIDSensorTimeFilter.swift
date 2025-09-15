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
    static let filter = Logger(subsystem: "com.apple.iohid", category: "HIDSensorTimeFilter")
}

fileprivate func toData<T>(value: T) -> NSData {
    return withUnsafePointer(to:value) { p in
        NSData(bytes: p, length: MemoryLayout.size(ofValue:value))
    }
}

class Stats {
    private var lastEMA: Double = 0
    private let smoothingFactor: Double
    private var vMax = UInt64.min
    private var vMin = UInt64.max

    init(smoothingFactor: Double) {
        self.smoothingFactor = smoothingFactor
    }
    func add(value: UInt64) {
        vMin = Swift.min(vMin, value)
        vMax = Swift.max(vMax, value)
        if lastEMA == 0 {
            lastEMA = Double(value) // Initialize with the first value
        } else {
            lastEMA = lastEMA * (1 - smoothingFactor) + Double(value) * smoothingFactor
        }
    }
    
    public var min: UInt64 {
        return vMin
    }

    public var max: UInt64 {
        return vMax
    }

    public var average: UInt64 {
        return UInt64(lastEMA)
    }
    
    func reset() {
        lastEMA = 0
        vMax = UInt64.min
        vMin = UInt64.max
    }
}


@objc(HIDSensorTimeFilter) public class HIDSensorTimeFilter: NSObject, HIDServiceFilter {
    
    private var cancelHandler: HIDBlock?
    private var eventDispatcher: HIDEventDispatcher?
    private var service: HIDEventService?
    private var queue: DispatchQueue = DispatchQueue(label: "com.apple.HIDSensorTimeFilter")
    internal static var logger: Logger { Logger.filter }
    private var logger: Logger { Self.logger }
    private var serviceID: UInt64 = 0
    private let serviceIDStr: String
    var timebaseInfo = mach_timebase_info_data_t()
    var timesync:HIDTimeSync?
    var timesyncPrecision:HIDTimeSyncPrecision = .unknown
    var timesyncState:HIDTimeSyncEvent = .inactive
    var reportInterval:UInt32 = 0
 
    private var logEvent:Bool = false
    var latency:Stats = Stats(smoothingFactor: 0.5)
  

    public required init?(service: HIDEventService) {
        self.service = service
        self.serviceID = service.serviceID
        self.serviceIDStr = "0x\(String(self.serviceID, radix: 16))"
        guard let vendorEventDict = service.property(forKey: "ChildVendorMessage") as? NSDictionary else {
            return nil
        }

        guard let vendorEventElements = vendorEventDict[kIOHIDElementKey] as? NSArray else {
            return nil
        }
        
        var supportTimeSync:Bool = false
        
        for element in vendorEventElements {
            guard let elementDict = element as? NSDictionary else {
                return nil
            }
            guard let page = elementDict["UsagePage"] as? Int else {
                continue
            }
            guard let usage = elementDict["Usage"] as? Int else {
                continue
            }
            if  usage == kHIDUsage_AppleVendorSensor_BTRemoteTimestamp && page == kHIDPage_AppleVendorSensor {
                supportTimeSync = true
                break
            }
        }
        if supportTimeSync == false {
            return nil
        }
        
        mach_timebase_info(&timebaseInfo)
        super.init()
    }
    
    public override var description: String {
        "\(Self.self)"
    }
    
    public func activate() {
    }
    
    public func cancel() {
        destroyTimesync()
        self.service = nil
        queue.async {
            guard let cancelHandler = self.cancelHandler else { return }
            cancelHandler()
            self.cancelHandler = nil
        }
    }
    
    public static func match(
        _ service: HIDEventService,
        options: [AnyHashable : Any]? = nil,
        score: UnsafeMutablePointer<Int>
    ) -> Bool {
        score.pointee = 0
        return true;
    }
    
    
    public func property(forKey key: String, client: HIDConnection?) -> Any? {
        if key == "ServiceFilterDebug" {
            let debugDict:NSMutableDictionary = [
                "Class" : "HIDSensorTimeFilter",
                "TimesyncState" : timesyncState.rawValue as NSNumber,
                "TimesyncPrecision" : timesyncPrecision.rawValue as NSNumber,
                "Timesync" : self.timesync != nil ? "Available" : "Not Available",
                "ReportInterval" : reportInterval as NSNumber
            ]
            return debugDict
        }
        return nil;
    }
    
    public func setProperty(_ value: Any?, forKey key: String, client: HIDConnection?) -> Bool {
        // relay on logic in HIDSensorControlFilter to only observe aggregate version of report interval (set with client = nil)
        if key == kIOHIDServiceReportIntervalKey && client == nil {
            if let reportInterval = value as? UInt32 {
                self.reportInterval = reportInterval
                //Only activate timesync if sensor streaming data
                if reportInterval > 0 {
                    logEvent = true
                    setupTimesync()
                } else {
                    logger.log("\(self.serviceIDStr): streaming session sample time latency min:\(latency.min) max:\(latency.max) average:\(latency.average)")
                    destroyTimesync()
                    latency.reset()
                }
            }
        }
        
        return true;
    }
    
    public func filterEvent(_ event: HIDEvent) -> HIDEvent? {
        
        if logEvent == true {
            self.logger.log("\(self.serviceIDStr): first event:\(event.type) timestamp:\(event.timestamp)")
            logEvent = false
        }

        guard var remoteTs = getMeasurementTimestamp (event) else {
            return event
        }
        
        let tsPrecision = getPrecision()
        
        var currentTs: UInt64 = 0
        var continuousTime: UInt64 = 0
        var currentTimeSpec = timespec(tv_sec: 0, tv_nsec: 0)

        let kr = machGetTimes(&currentTs, &continuousTime,  &currentTimeSpec)
        guard kr == KERN_SUCCESS else {
            logger.error("\(self.serviceIDStr): machGetTimes:\(kr)")
            return event
        }
        
        var ts:UInt64
        var sampleTimeType:IOHIDSensorSampleTimeType
        
        if let syncedTs = syncedTime(remoteTs) {
            if  currentTs >= syncedTs {
                sampleTimeType = .measurement
                ts = syncedTs
                
                let sampleTimestampEvent = HIDEvent.vendorDefinedEvent(event.timestamp, usagePage: UInt16(kHIDPage_AppleVendorSensor), usage: UInt16(kHIDUsage_AppleVendorSensor_TimeSyncTimestamp), version: 0, data: &ts, length: UInt32(MemoryLayout.size(ofValue:ts)), options: 0)
                
                event.append(sampleTimestampEvent)
                
                latency.add(value:machAbsoluteToNanoseconds(currentTs - syncedTs))
                
            } else {
                logger.error("\(self.serviceIDStr): timesync timstamp is in future use time of arrival current ts:\(currentTs) synce ts:\(syncedTs) remote ts:\(remoteTs)")
                
                sampleTimeType = .arrival
                ts = event.timestamp
            }
        } else {
            sampleTimeType = .arrival
            ts = event.timestamp
        }
 
        let delta = machAbsoluteToNanoseconds(currentTs - ts)
        
        let delta_sec = delta / 1000000000
        let delta_nsec = delta % UInt64(1000000000)

        guard delta_sec < currentTimeSpec.tv_sec else {
            logger.error("\(self.serviceIDStr): delta_sec:\(delta_sec) tv_sec:\(currentTimeSpec.tv_sec)")
            return event
        }
        
        var tv_nsec_sample:UInt64 = 1000000000 + UInt64(currentTimeSpec.tv_nsec) - delta_nsec
        var tv_sec_sample = UInt64 (currentTimeSpec.tv_sec) - delta_sec
        if tv_nsec_sample  >= 1000000000 {
            tv_nsec_sample = tv_nsec_sample  - 1000000000
        } else {
            tv_sec_sample = tv_sec_sample - 1
        }
   
        var sampleTime = IOHIDSensorSampleTime(type:sampleTimeType, reserved:(0,0,0),  precision_nsec:tsPrecision, tv_sec:UInt32 (tv_sec_sample), tv_nsec:UInt32(tv_nsec_sample));

        let sampleTimeEvent = HIDEvent.vendorDefinedEvent(event.timestamp, usagePage: UInt16(kHIDPage_AppleVendorSensor), usage: UInt16(kHIDUsage_AppleVendorSensor_SampleTime), version: 0, data: &sampleTime, length: UInt32(MemoryLayout.size(ofValue:sampleTime)), options: 0)

        event.append(sampleTimeEvent)

        return event
    }

    func getMeasurementTimestamp (_ event: HIDEvent) -> UInt64? {
        guard let children = event.children  else {
            return nil
        }
        for child in children {
            if child.type == kIOHIDEventTypeVendorDefined && UInt16(child.integerValue(forField: kIOHIDEventFieldVendorDefinedUsagePage)) == UInt16(kHIDPage_AppleVendorSensor) && UInt16(child.integerValue(forField: kIOHIDEventFieldVendorDefinedUsage)) == UInt16(kHIDUsage_AppleVendorSensor_BTRemoteTimestamp) {
                
                let tsLen = child.integerValue(forField:kIOHIDEventFieldVendorDefinedDataLength)
                guard tsLen == 8  else {
                    return nil
                }
                
                let tsValue = child.dataValue(forField: kIOHIDEventFieldVendorDefinedData).load(as: UInt64.self)
                return tsValue
            }
        }
        return nil
    }
    
    public func setupTimesync() {
        
        if self.timesync != nil {
            return
        }

        self.timesync = HIDTimeSync(from: service!)
        guard let timesync = self.timesync else {
            self.logger.log ("\(self.serviceIDStr):Timesync is not available")
            return
        }
        timesync.setEventHandler { event, precision in
            self.timesyncState = event
            self.timesyncPrecision = precision
            self.logger.log ("\(self.serviceIDStr): timesync event:\(event.rawValue), precision:\(precision.rawValue) event stats:\(self.service!.eventStatistics() as? NSDictionary)")
        }
        timesync.setDispatchQueue(self.queue)
        timesync.setCancelHandler {
        }
        timesync.activate()
    }

    public func destroyTimesync() {
        self.timesyncState = .inactive
        if let timesync = self.timesync {
            timesync.cancel()
        }
        self.timesync = nil;
    }
    
    public func setCancelHandler(_ cancelHandler: @escaping HIDBlock) {
        self.cancelHandler = cancelHandler
    }
    
    public func setDispatchQueue(_ queue: DispatchQueue) {
        self.queue = queue
    }
    
    public func setEventDispatcher(_ eventDispatcher: HIDEventDispatcher) {
        self.eventDispatcher = eventDispatcher
    }
    
    public func filterEvent(matching: [AnyHashable : Any]?, event: HIDEvent, forClient client: HIDConnection?) -> HIDEvent? {
        return event;
    }
    
    func machAbsoluteToNanoseconds(_ machAbsolute: UInt64) -> UInt64 {
      let nanos = (machAbsolute * UInt64(timebaseInfo.numer)) / UInt64(timebaseInfo.denom)
      return nanos;
    }
    
    func machGetTimes(_ absolute_time: UnsafeMutablePointer<UInt64>!, _ continuous_time: UnsafeMutablePointer<UInt64>!, _ tp: UnsafeMutablePointer<timespec>!) -> kern_return_t
    {
        return mach_get_times(absolute_time, continuous_time, tp)
    }
    
    func syncedTime (_ ts:UInt64) -> UInt64? {
        if timesync != nil && timesyncState == .active {
            var error:NSError? = nil
            let syncTs = timesync!.syncedTime(from:toData(value: ts) as Data, error: &error)
            if error == nil {
                return syncTs
            }
            logger.error("\(self.serviceIDStr): syncedTime:\(error)")
        }
        return nil
    }
    
    func getPrecision () -> UInt64 {
        
        switch timesyncPrecision {
        case .unknown:
            return  0
        case .high:
            return kIOHIDBTAACPTimePrecisionHigh
        case .low:
            return kIOHIDBTAACPTimePrecisionLow
        @unknown default:
            return 0
        }
    }
}

