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
import IOKit_Private
import OSLog
 
internal extension Logger {
    static let filter = Logger(subsystem: "com.apple.iohid", category: "HIDSensorControlFilter")
}


extension HIDConnection {
    @usableFromInline internal var bridgedValue: IOHIDEventSystemConnection {
           unsafeBitCast(self, to: IOHIDEventSystemConnection.self)
    }
    func hasEntitlement (_ entitlement:String) -> Bool {
        return IOHIDEventSystemConnectionHasEntitlement (self.bridgedValue, entitlement as CFString)
    }
}

@objc(HIDSensorControlFilter) public class HIDSensorControlFilter: NSObject, HIDServiceFilter {
    
    private var cancelHandler: HIDBlock?
    private var eventDispatcher: HIDEventDispatcher?
    private var service: HIDEventService?
    private var queue: DispatchQueue = DispatchQueue(label: "com.apple.HIDSensorControlFilter")
    internal static var logger: Logger { Logger.filter }
    private var logger: Logger { Self.logger }
    private var serviceID: UInt64 = 0;
    private let serviceIDStr: String
    
    /// Default match score for this filter.
    /// A score of 0 indicates this filter has neutral priority and will be considered
    /// alongside other filters with the same score.
    private static let defaultMatchScore = 0
    
    /// Default sensor control options.
    /// A value of 0 indicates this filter is taking over sensor control functionality
    private static let defaultSensorControlOptions = 0
    
    typealias ControlHandler = (UInt32, UInt32, String) -> Void
    class ControlState {
        let controlKey:String
        var values: [HIDConnection:UInt32] = [:]
        var states: [HIDConnection:Bool] = [:]
        let service: HIDEventService
        let controlHandler:ControlHandler
        var controlValue:UInt32 = 0
        let serviceIDStr:String
        internal static var logger: Logger { Logger.filter }
        private var logger: Logger { Self.logger }

        init(service:HIDEventService, key:String, handler: @escaping ControlHandler) {
            self.service = service
            self.controlKey = key
            self.controlHandler = handler
            self.serviceIDStr = "0x\(String(self.service.serviceID, radix: 16))"
        }
        
        func setControl (client:HIDConnection, value:UInt32) {
            logger.log("\(self.serviceIDStr): setControl:\(self.controlKey) value:\(value) client:\(client.uuid)")
            self.values[client] = value
            updateControlValue()
        }
        
        func getControlValue () -> UInt32 {
            return self.controlValue
        }
        
        func removeClient(client: HIDConnection) {
            if values[client] != nil {
                logger.log("\(self.serviceIDStr): removeClient:\(client.uuid)")
                values[client] = nil
                states[client] = nil
                updateControlValue()
            }
        }
        
        func setClientState(client: HIDConnection, isInactive: Bool) {
            logger.log("\(self.serviceIDStr): setClientState:\(isInactive) client:\(client.uuid)")
            states[client] = isInactive
            updateControlValue()
        }
        
        /// Updates the effective control value by aggregating all active client values.
        ///
        /// **Control Value Aggregation Strategy:**
        /// - Only considers active clients (ignores inactive/unresponsive clients)
        /// - Finds the minimum non-zero value among all active clients
        /// - If no clients have non-zero values, the control value becomes 0
        ///
        /// **Rationale for Minimum Value Selection:**
        /// For sensor properties like report intervals and batch intervals, the minimum
        /// value represents the most restrictive (highest frequency) requirement among
        /// all clients. This ensures that the sensor operates at a rate that satisfies
        /// the most demanding client while being efficient for all others.
        ///
        /// **Example:**
        /// - Client A requests 100ms interval
        /// - Client B requests 50ms interval
        /// - Client C requests 200ms interval
        /// - Result: 50ms interval (satisfies all clients, most efficient)
        func updateControlValue() {
            var newControlValue:UInt32 = 0
            
            // Iterate through all client values
            for (client, value) in values {
                // Skip inactive/unresponsive clients
                if states[client] != nil && states[client] == true  {
                    continue
                }
                
                // Find minimum non-zero value (most restrictive requirement)
                if value != 0 && (newControlValue == 0 || value < newControlValue) {
                    newControlValue = value
                }
            }
            
            // Only update if the aggregated value has changed
            if newControlValue != controlValue {
                self.controlHandler(controlValue, newControlValue, controlKey)
                controlValue = newControlValue
            }
        }
        func debugState() -> NSDictionary {
            return  Dictionary (uniqueKeysWithValues:values.map { key, value in
                let state = ["value":value, "isInactive":states[key] ?? false]
                return (key.uuid as NSString, state as NSDictionary)
            }) as NSDictionary
        }
    }

    private var controls:[String:ControlState] = [:]
    
    public required init?(service: HIDEventService) {
        self.service = service
        self.serviceID = service.serviceID
        self.serviceIDStr = "0x\(String(self.serviceID, radix: 16))"
        super.init()
        
        let controlHandler:ControlHandler = {prev, new, key in
            self.logger.log("\(self.serviceIDStr): set property:\(key) with new value:\(new) current value:\(prev) event stats:\(self.service!.eventStatistics() as? NSDictionary)")
            service.setProperty(new as NSNumber , forKey: key)
        }

        controls = [
            kIOHIDSensorPropertyMaxFIFOEventsKey:ControlState(service: service, key:kIOHIDSensorPropertyMaxFIFOEventsKey, handler: controlHandler),
            kIOHIDServiceReportIntervalKey:ControlState(service: service, key: kIOHIDServiceReportIntervalKey, handler: controlHandler),
            kIOHIDServiceBatchIntervalKey:ControlState(service: service, key: kIOHIDServiceBatchIntervalKey, handler: controlHandler)
        ]
    }
    
    public override var description: String {
        "\(Self.self)"
    }
    
    public func activate() {
    }
    
    public func cancel() {
        self.service = nil
        
        self.controls.removeAll()
        
        guard let cancelHandler = self.cancelHandler else { return }
        cancelHandler()
        self.cancelHandler = nil
    }
    
    public static func match(
        _ service: HIDEventService,
        options: [AnyHashable : Any]? = nil,
        score: UnsafeMutablePointer<Int>
    ) -> Bool {
        score.pointee = HIDSensorControlFilter.defaultMatchScore
        return true;
    }
    
    
    public func property(forKey key: String, client: HIDConnection?) -> Any? {
        
        if key == kIOHIDServiceFilterDebugKey {
            var controlsDebug = Dictionary (uniqueKeysWithValues:controls.map { key, value in
                return (key as NSString, value.debugState())
            }) as NSDictionary
            
            let debugDict:NSMutableDictionary = [
                "Class" : "HIDSensorControlFilter",
                "Controls" : controlsDebug
            ]
            return debugDict
        } else if key == kIOHIDEventServiceSensorControlOptionsKey {
            return HIDSensorControlFilter.defaultSensorControlOptions as NSNumber
        }
        return nil;
    }
    
    public func setProperty(_ value: Any?, forKey key: String, client: HIDConnection?) -> Bool {
        guard let client = client else {
            return false
        }
        if key != kIOHIDEventSystemClientIsUnresponsive {
            return true
        }
        guard let value = value as? Bool else {
            return false
        }
        
        for key in self.controls.keys {
            self.controls[key]?.setClientState(client:client, isInactive:value)
        }
        
        return true;
    }
    
    public func filterEvent(_ event: HIDEvent) -> HIDEvent? {
        
        return event
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
    
    /// Filters events for specific clients based on their type and control state.
    ///
    /// **Client Type Filtering:**
    /// Only processes events for rate-controlled clients. Other client types
    /// (e.g., standard clients) pass through without filtering because they
    /// don't participate in the sensor control mechanism.
    ///
    /// **Rate Control Validation:**
    /// For rate-controlled clients, ensures they have a valid report interval
    /// control value set. Events are dropped if no valid control is established.
    public func filterEvent(_ event: HIDEvent, forClient client: HIDConnection?) -> HIDEvent? {
        // Only process rate-controlled clients; others pass through unchanged
        guard let client = client, client.type == .rateControlled else {
            return event
        }
        
        // Ensure the client has established rate control
        guard let controlState = controls[kIOHIDServiceReportIntervalKey] else {
            return nil
        }
        guard let controlValue = controlState.values[client], controlValue != 0 else {
            return nil
        }
        return event;
    }
    
    /// Filters property set operations from clients, managing sensor control values.
    ///
    /// **Property Filtering Strategy:**
    /// - Only processes properties from rate-controlled clients
    /// - Validates property values are UInt32 type
    /// - Updates the control state for managed properties
    /// - Sets value to nil to prevent further propagation
    ///
    /// **Managed Properties:**
    /// - kIOHIDSensorPropertyMaxFIFOEventsKey: Maximum FIFO events
    /// - kIOHIDServiceReportIntervalKey: Report interval in microseconds
    /// - kIOHIDServiceBatchIntervalKey: Batch interval in microseconds
    public func filterSetProperty(_ value: AutoreleasingUnsafeMutablePointer<AnyObject?>, forKey key: String, forClient client:HIDConnection?) {
        
        // Only process rate-controlled clients
        guard let client = client, client.type == .rateControlled else {
            return
        }
        
        // Only handle properties we manage
        guard let controlState = controls[key] else {
            return
        }
                
        // Validate property value type
        guard let propertyValue = value.pointee as? UInt32 else {
            self.logger.error("\(self.serviceIDStr): client:\(client.uuid) property:\(key) unexpected value:\(value.pointee?.description ?? "nil")")
            return
        }
  
        // Update control state and aggregate values
        controlState.setControl(client: client, value: propertyValue)
        
        // Prevent further propagation by setting to nil
        value.pointee = nil
    }
    
    public func clientNotification(_ client: HIDConnection, added: Bool) {
        if added == true {
            return
        }
        
        for control in self.controls.values {
            control.removeClient(client: client)
        }
        
    }
}

