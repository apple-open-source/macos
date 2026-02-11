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

import Foundation
import IOKit_Private
import HID_Private
import OSLog
import CoreAnalytics

internal extension Logger {
    static let sessionFilter = Logger(subsystem: "com.apple.iohid", category: "HIDTransportAnalyticsSessionFilter")
}

extension Data {
    func hexEncodedString() -> String {
        return map { String(format: "%02hhx", $0) }.joined()
    }
}

@objc(HIDTransportAnalyticsSessionFilter) public class HIDTransportAnalyticsSessionFilter: NSObject, HIDSessionFilter {
    private var hidManager = HIDManager()
    private var queue: DispatchQueue = DispatchQueue(label: "com.apple.HIDTransportAnalyticsSessionFilter")
    
    // Using hash lookup for better performance
    private var seenEventHashes = Set<Int>()
    
    public required init?(session: HIDSession) {
        super.init()
    }
    
    public override var description: String {
        "\(Self.self)"
    }
    
    public func property(forKey key: String) -> Any? {
        return nil
    }
    
    public func setProperty(_ value: Any?, forKey key: String) -> Bool {
        return true;
    }
    
    public func filterEvent(_ event: HIDEvent, for service: HIDEventService) -> HIDEvent? {
        collectEventAnalytics(event: event, service: service)
        return event
    }
    
    private func collectEventAnalytics(event: HIDEvent, service: HIDEventService) {
        let eventType = event.type
        let transport = service.property(forKey: kIOHIDTransportKey) as? String ?? "Unknown"
        let servicePrimaryUsagePage = service.property(forKey: kIOHIDPrimaryUsagePageKey) as? UInt32
        let servicePrimaryUsage = service.property(forKey: kIOHIDPrimaryUsageKey) as? UInt32
        let builtIn = service.property(forKey: kIOHIDBuiltInKey) as? Bool ?? false
        let ioClass = service.property(forKey: kIOHIDServiceRegistryNameKey) as? String ?? "Unknown"
        let isVirtualService = service.property(forKey: kIOHIDVirtualServiceKey) as? Bool ?? false
        
        var usagePage: UInt32? = nil
        var usage: UInt32? = nil
        
        // Extract usage information from the event for specific event types we care about
        if eventType == kIOHIDEventTypeVendorDefined {
            // For vendor defined events, get the usage from the event
            usagePage = UInt32(event.integerValue(forField: kIOHIDEventFieldVendorDefinedUsagePage))
            usage = UInt32(event.integerValue(forField: kIOHIDEventFieldVendorDefinedUsage))
        }
        
        // Include all properties from the payload in the hash calculation
        let hash = fastHash(transport: transport,
                           eventType: eventType,
                           usagePage: usagePage,
                           usage: usage,
                           servicePrimaryUsagePage: servicePrimaryUsagePage,
                           servicePrimaryUsage: servicePrimaryUsage,
                           builtIn: builtIn,
                           ioClass: ioClass,
                           isVirtualService: isVirtualService)
        
        if !seenEventHashes.contains(hash) {
            seenEventHashes.insert(hash)
            
            // Send analytics immediately for previously unseen property combinations
            queue.async { [usage, usagePage, ioClass, isVirtualService] in
                AnalyticsSendEventLazy("com.apple.hid.eventtype") {
                    var payload: [String: NSObject] = [
                        "Transport": transport as NSString,
                        "EventType": NSNumber(value: eventType),
                        "BuiltIn": NSNumber(value: builtIn),
                        "IOClass": ioClass as NSString,
                        "IsVirtualService": NSNumber(value: isVirtualService)
                    ]
                    
                    if let usagePage = usagePage, let usage = usage {
                        payload["UsagePage"] = NSNumber(value: usagePage)
                        payload["Usage"] = NSNumber(value: usage)
                    }
                    
                    if let servicePrimaryUsagePage = servicePrimaryUsagePage {
                        payload["ServicePrimaryUsagePage"] = NSNumber(value: servicePrimaryUsagePage)
                    }
                    
                    if let servicePrimaryUsage = servicePrimaryUsage {
                        payload["ServicePrimaryUsage"] = NSNumber(value: servicePrimaryUsage)
                    }
                    
                    return payload
                }
            }
        }
    }
    
    private func fastHash(transport: String,
                         eventType: IOHIDEventType,
                         usagePage: UInt32?,
                         usage: UInt32?,
                         servicePrimaryUsagePage: UInt32?,
                         servicePrimaryUsage: UInt32?,
                         builtIn: Bool,
                         ioClass: String,
                         isVirtualService: Bool) -> Int {
        var hasher = Hasher()
        hasher.combine(transport)
        hasher.combine(eventType)
        hasher.combine(usagePage)
        hasher.combine(usage)
        hasher.combine(servicePrimaryUsagePage)
        hasher.combine(servicePrimaryUsage)
        hasher.combine(builtIn)
        hasher.combine(ioClass)
        hasher.combine(isVirtualService)
        return hasher.finalize()
    }

    public func activate() {
        hidManager.setDeviceMatching([])
        hidManager.setDeviceNotificationHandler { (device, added) in
            if added {
                Logger.sessionFilter.info("Found device \(device)")
                let productIDProp = device.property(forKey: kIOHIDProductIDKey)
                let vendorIDProp = device.property(forKey: kIOHIDVendorIDKey)
                let transportProp = device.property(forKey: kIOHIDTransportKey)
                let descriptorProp = device.property(forKey: kIOHIDReportDescriptorKey)
                let builtInProp = device.property(forKey: kIOHIDBuiltInKey)
                let productProp = device.property(forKey: kIOHIDProductKey)
                let manufacturerProp = device.property(forKey: kIOHIDManufacturerKey)
                let modelNumberProp = device.property(forKey: kIOHIDModelNumberKey)
                let versionNumberProp = device.property(forKey: kIOHIDVersionNumberKey)
                let ioClassProp = device.property(forKey: kIOHIDServiceRegistryNameKey)
                
                AnalyticsSendEventLazy("com.apple.hid.device.info") {
                    let productID = productIDProp as? NSNumber ?? NSNumber(value: 0)
                    let vendorID = vendorIDProp as? NSNumber ?? NSNumber(value: 0)
                    let transport = transportProp as? NSString ?? "Unknown"
                    let reportDescriptor = (descriptorProp as? Data)?.hexEncodedString() as? NSString ?? "Unknown"
                    let builtIn = builtInProp as? NSNumber ?? NSNumber(value: false)
                    let product = productProp as? NSString ?? "Unknown"
                    let manufacturer = manufacturerProp as? NSString ?? "Unknown"
                    let modelNumber = modelNumberProp as? NSString ?? "Unknown"
                    let versionNumber = versionNumberProp as? NSNumber ?? NSNumber(value: 0)
                    let ioClass = ioClassProp as? NSString ?? "Unknown"

                    return ["ProductID": productID,
                            "VendorID": vendorID,
                            "Transport": transport,
                            "ReportDescriptor": reportDescriptor,
                            "BuiltIn": builtIn,
                            "Product": product,
                            "Manufacturer": manufacturer,
                            "ModelNumber": modelNumber,
                            "VersionNumber": versionNumber,
                            "IOClass": ioClass] as [String : NSObject]
                }
            }
        }
        hidManager.setDispatchQueue(queue)
        hidManager.activate()
    }
    

}
