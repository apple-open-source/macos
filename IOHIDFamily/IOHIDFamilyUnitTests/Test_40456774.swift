//
//  Test_40456774.swift
//  IOHIDFamilyUnitTests
//
//  Created by dekom on 9/24/18.
//

import Foundation
import XCTest
import HID

@objc(Test_40456774) class Test_40456774 : XCTestCase {
    var queue : DispatchQueue!
    var addedExp : XCTestExpectation!
    var removedExp : XCTestExpectation!
    var manager : HIDManager!
    
    override func setUp() {
        queue = DispatchQueue(label: "")
        manager = HIDManager()
        manager.setDeviceMatching([kIOHIDLocationIDKey : 12345678])
        manager.setDeviceNotificationHandler { (device, added) in
            print("Device \(device)", added ? "added" : "removed")
            
            if added {
                self.addedExp.fulfill()
            } else {
                self.removedExp.fulfill()
            }
        }
        
        manager.setDispatchQueue(queue)
        manager.activate()
    }
    
    override func tearDown() {
        manager.cancel()
    }
    
    func test_40456774() {
        let desc = NSData(bytesNoCopy: UnsafeMutableRawPointer(mutating: HIDKeyboardDescriptor),
                          length: HIDKeyboardDescriptor.count,
                          freeWhenDone: false)
        let properties : [String : Any] = [kIOHIDReportDescriptorKey: desc,
                                           kIOHIDLocationIDKey: 12345678]
        
        for _ in 0..<20 {
            addedExp = XCTestExpectation(description: "HIDUserDevice added")
            removedExp = XCTestExpectation(description: "HIDUserDevice removed")
            
            var device : HIDUserDevice? = HIDUserDevice(properties: properties)
            XCTAssert(device != nil, "Failed to create HIDUserDevice")
            
            var result = XCTWaiter.wait(for: [addedExp], timeout: 5)
            XCTAssert(result == XCTWaiter.Result.completed)
            
            device = nil
            
            result = XCTWaiter.wait(for: [removedExp], timeout: 5)
            XCTAssert(result == XCTWaiter.Result.completed)
        }
    }
}
