//
//  TestClient.swift
//  IOHIDFamilyUnitTests
//
//  Created by dekom on 8/24/18.
//

import Foundation
import XCTest
import HID

@objc(TestClient) class TestClient : XCTestCase {
    var userDevice : HIDUserDevice!
    var manager : HIDManager!
    var client : HIDEventSystemClient!
    var queue : DispatchQueue!
    var eventExp : XCTestExpectation!
    
    override func setUp()  {
        super.setUp()
        
        let desc = NSData(bytesNoCopy: UnsafeMutableRawPointer(mutating: HIDKeyboardDescriptor),
                          length: HIDKeyboardDescriptor.count,
                          freeWhenDone: false)
        let uuid = UUID()
        let properties : [String : Any] = [kIOHIDReportDescriptorKey: desc,
                                           kIOHIDPhysicalDeviceUniqueIDKey: uuid.uuidString,
                                           kIOHIDServiceHiddenKey as String: true]
        
        guard let userDevice = HIDUserDevice(properties: properties) else {
            XCTAssert(false, "Failed to create HIDUserDevice")
            return
        }
        
        let deviceExp = XCTestExpectation(description: "HIDUserDevice enumeration")
        
        let manager = HIDManager()
        manager.setDeviceMatching([kIOHIDPhysicalDeviceUniqueIDKey : uuid.uuidString])
        manager.setDeviceNotificationHandler { (device, added) in
            if added {
                print("Device added \(device)")
                deviceExp.fulfill()
            }
        }
        
        queue = DispatchQueue(label: "")
        manager.setDispatchQueue(queue)
        manager.activate()
        
        let result = XCTWaiter.wait(for: [deviceExp], timeout: 5)
        XCTAssert(result == XCTWaiter.Result.completed)
        
        guard let client = HIDEventSystemClient(type: HIDEventSystemClientType.monitor) else {
            XCTAssert(false, "Failed to create HIDEventSystemClient")
            return
        }
        
        client.setMatching([kIOHIDPhysicalDeviceUniqueIDKey : uuid.uuidString,
                            kIOHIDServiceHiddenKey as String: true])
        
        eventExp = XCTestExpectation(description: "HIDEvent handler")
        
        client.setEventHandler { (service, event) in
            print("Received event \(event)")
            self.eventExp.fulfill()
        }
        
        client.setDispatchQueue(queue)
        client.activate()
        
        self.userDevice = userDevice
        self.manager = manager
        self.client = client
    }
    
    override func tearDown() {
        manager.cancel()
        client.cancel()
        super.tearDown()
    }
    
    func testQueueOverflow() {
        // Verifies client is able to receive events after a queue overflow
        // See rdar://problem/41030761
        
        queue.suspend()
        
        // Dispatch 500 events. This should overflow the event queue since the
        // dispatch queue is suspended.
        for i in 0...500 {
            var report = HIDKeyboardDescriptorInputReport()
            
            if i % 2 == 0 {
                report.KB_KeyboardKeyboardLeftShift = 1
            } else {
                report.KB_KeyboardKeyboardLeftShift = 0
            }
            
            do {
                try userDevice.handleReport(Data(bytes: &report,
                                                 count: MemoryLayout.size(ofValue: report)))
            } catch let error as NSError {
                print("userDevice.handleReport failed: \(error)")
            }
        }
        
        // allow time for event dispatch
        sleep(3)
        
        // Resume the dispatch queue. We should expect to see events after this.
        queue.resume()
        
        let result = XCTWaiter.wait(for: [eventExp], timeout: 5)
        XCTAssert(result == XCTWaiter.Result.completed)
    }
    
    func testQueueOverflow2() {
        // Similar to above, but keep the queue busy rather than suspend
        
        queue.sync {
            for i in 0...500 {
                var report = HIDKeyboardDescriptorInputReport()
                
                if i % 2 == 0 {
                    report.KB_KeyboardKeyboardLeftShift = 1
                } else {
                    report.KB_KeyboardKeyboardLeftShift = 0
                }
                
                do {
                    try userDevice.handleReport(Data(bytes: &report,
                                                     count: MemoryLayout.size(ofValue: report)))
                } catch let error as NSError {
                    print("userDevice.handleReport failed: \(error)")
                }
            }
            
            // allow time for event dispatch
            sleep(3)
        }
        
        let result = XCTWaiter.wait(for: [eventExp], timeout: 5)
        XCTAssert(result == XCTWaiter.Result.completed)
    }
}
