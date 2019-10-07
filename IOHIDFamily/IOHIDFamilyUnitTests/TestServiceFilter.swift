//
//  TestServiceFilter.swift
//  IOHIDFamilyUnitTests
//
//  Created by dekom on 9/28/18.
//

import Foundation
import XCTest
import HID

@objc(TestServiceFilter) class TestServiceFilter : XCTestCase {
    var userDevice : HIDUserDevice!
    var client : HIDEventSystemClient!
    var serviceClient : HIDServiceClient!
    var eventExp : XCTestExpectation!
    
    override func setUp()  {
        super.setUp()
        
        guard let client = HIDEventSystemClient(type: HIDEventSystemClientType.monitor) else {
            XCTAssert(false, "Failed to create HIDEventSystemClient")
            return
        }
        
        client.setMatching([kIOHIDVendorIDKey : 12345678,
                            kIOHIDServiceHiddenKey as String: true])
        
        eventExp = XCTestExpectation(description: "HIDEvent handler")
        
        client.setEventHandler { (service, event) in
            print("Received event \(event)")
            self.eventExp.fulfill()
        }
        
        let serviceExp = XCTestExpectation(description: "service enumerated")
        
        client.setServiceNotificationHandler { (service) in
            print("Service added: \(service)")
            self.serviceClient = service;
            serviceExp.fulfill()
        }
        
        client.setDispatchQueue(DispatchQueue.main)
        client.activate()
        
        let desc = NSData(bytesNoCopy: UnsafeMutableRawPointer(mutating: HIDKeyboardDescriptor),
                          length: HIDKeyboardDescriptor.count,
                          freeWhenDone: false)
        
        let properties : [String : Any] = [kIOHIDReportDescriptorKey: desc,
                                           kIOHIDVendorIDKey: 12345678,
                                           kIOHIDServiceHiddenKey as String: true]
        
        guard let userDevice = HIDUserDevice(properties: properties) else {
            XCTAssert(false, "Failed to create HIDUserDevice")
            return
        }
        
        let result = XCTWaiter.wait(for: [serviceExp], timeout: 5)
        XCTAssert(result == XCTWaiter.Result.completed)
        
        self.userDevice = userDevice
        self.client = client
    }
    
    override func tearDown() {
        client.cancel()
        super.tearDown()
    }
    
    func testServiceFilter() {
        var result : Bool = false
        print("waiting...")
        
        result = serviceClient.setProperty(1234, forKey: "TestHIDServiceFilterSetProperty")
        XCTAssert(result, "serviceClient.setProperty failed")
        
        let prop = serviceClient.property(forKey: "TestHIDServiceFilterGetProperty")
        XCTAssert(prop != nil, "servserviceClient.property failed")
        print("prop: \(String(describing: prop))")
        
        result = serviceClient.setProperty(true, forKey: "TestEventDispatch")
        XCTAssert(result, "serviceClient.setProperty failed")
        
        let waiter = XCTWaiter.wait(for: [eventExp], timeout: 5)
        XCTAssert(waiter == XCTWaiter.Result.completed)
        
        let matching = [kIOHIDEventTypeKey  : kIOHIDEventTypeKeyboard,
                        kIOHIDUsagePageKey  : kHIDPage_KeyboardOrKeypad,
                        kIOHIDUsageKey      : kHIDUsage_KeyboardLeftShift]
        
        if let event = serviceClient.event(matching: matching) {
            print("event: \(event)")
            XCTAssert(event.type == kIOHIDEventTypeKeyboard)
            XCTAssert(event.children != nil)
        } else {
            XCTAssert(false, "serviceClient.event failed")
        }
        
        let debugInfo = serviceClient.property(forKey: kIOHIDServiceFilterDebugKey) as? Dictionary<String,Any>
        XCTAssert(debugInfo != nil, "servserviceClient.property failed for kIOHIDServiceFilterDebugKey")
        
        print("debugInfo: \(String(describing: debugInfo))")
        
        XCTAssert(debugInfo?["FilterName"] as! String == "HIDServiceFilterExample")
        XCTAssert(debugInfo?["cancelHandler"] as! Bool == true)
        XCTAssert(debugInfo?["dispatchQueue"] as! Bool == true)
        XCTAssert(debugInfo?["activated"] as! Bool == true)
        XCTAssert(debugInfo?["clientAdded"] as! Bool == true)
    }
}
