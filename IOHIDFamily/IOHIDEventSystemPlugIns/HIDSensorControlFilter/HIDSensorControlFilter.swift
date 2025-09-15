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
        
        func updateControlValue() {
            var newControlValue:UInt32 = 0
            for client in values.keys {
                if states[client] != nil && states[client] == true  {
                    continue
                }
                
                let clientValue = values[client]!
                
                if clientValue != 0 && (newControlValue == 0 || clientValue < newControlValue) {
                    newControlValue = clientValue
                }
            }
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
        score.pointee = 0
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
            return 0 as NSNumber
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
    
    public func filterEvent(_ event: HIDEvent, forClient client: HIDConnection?) -> HIDEvent? {
        guard let client = client, client.type == .rateControlled else {
            return event
        }
        
        guard let controlValue = controls[kIOHIDServiceReportIntervalKey]!.values[client], controlValue != 0 else {
            return nil
        }
        return event;
    }
    
    public func filterSetProperty(_ value: AutoreleasingUnsafeMutablePointer<AnyObject?>, forKey key: String, forClient client:HIDConnection?) {
        
        guard let client = client, client.type == .rateControlled else {
            return
        }
        
        guard var controlState = controls[key] else {
            return
        }
                
        guard let propertyValue = value.pointee as? UInt32 else {
            self.logger.error("\(self.serviceIDStr): client:\(client.uuid) property:\(key) unexpected value:\(value.pointee?.description ?? "nil")")
            return
        }
  
        controlState.setControl(client: client, value: propertyValue)
        
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

