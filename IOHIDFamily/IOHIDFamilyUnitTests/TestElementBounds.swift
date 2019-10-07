//
//  TestElementBounds.swift
//  IOHIDFamilyUnitTests
//
//  Created by dekom on 8/15/18.
//

import Foundation
import XCTest
import HID

// We need the @objc(TestElementBounds) so this class can be picked up by
// hidxctest, otherwise the call to testSuiteForTestCaseWithName will fail.
// See rdar://problem/43437350
@objc(TestElementBounds) class TestElementBounds : XCTestCase {
    var userDevice : HIDUserDevice!
    var device : HIDDevice!
    var manager : HIDManager!
    var numlockOOB : Int = 0
    var capslockOOB : Int = 1
    
    static let descriptor : [UInt8] = [
        0x05, 0x01,     // Usage Page (Generic Desktop)
        0x09, 0x06,     // Usage (Keyboard)
        0xA1, 0x01,     // Collection (Application)
        0x19, 0xE0,     //   Usage Minimum........... (224)
        0x29, 0xE7,     //   Usage Maximum........... (231)
        0x15, 0x00,     //   Logical Minimum......... (0)
        0x25, 0x01,     //   Logical Maximum......... (1)
        0x75, 0x01,     //   Report Size............. (1)
        0x95, 0x08,     //   Report Count............ (8)
        0x81, 0x02,     //   Input...................(Data, Variable, Absolute)
        0x05, 0x08,     //   Usage Page (LED)
        0x09, 0x01,     //   Usage (Num Lock)
        0x75, 0x08,     //   Report Size............. (8)
        0x95, 0x01,     //   Report Count............ (1)
        0x15, 0x00,     //   Logical Minimum......... (0)
        0x25, 0x05,     //   Logical Maximum......... (5)
        0x91, 0x02,     //   Output..................(Data, Variable, Absolute)
        0x09, 0x02,     //   Usage (Caps Lock)
        0x15, 0x05,     //   Logical Minimum......... (5)
        0x25, 0x0A,     //   Logical Maximum......... (10)
        0x91, 0x02,     //   Output..................(Data, Variable, Absolute)
        0xC0,           // End Collection
    ]
    
    override func setUp()  {
        super.setUp()
        
        let desc = NSData(bytesNoCopy: UnsafeMutableRawPointer(mutating: TestElementBounds.descriptor),
                          length: TestElementBounds.descriptor.count,
                          freeWhenDone: false)
        let uuid = UUID()
        let properties : [String : Any] = [kIOHIDReportDescriptorKey: desc,
                                           kIOHIDPhysicalDeviceUniqueIDKey: uuid.uuidString]
        
        guard let userDevice = HIDUserDevice(properties: properties) else {
            XCTAssert(false, "Failed to create HIDUserDevice")
            return
        }
        
        userDevice.setSetReportHandler { (reportType, reportID, report, reportLength) -> IOReturn in
            XCTAssert(reportType == HIDReportType.output)
            XCTAssert(reportLength == 2)
            
            // numlock out of bounds
            // logical min/max for numlock report is 0-5, so anything outside
            // of that is out of bounds
            if report[0] > 5 {
                self.numlockOOB = Int(report[0])
                print("Detected numlock OOB value \(self.numlockOOB)")
            }
            
            // capslock out of bounds
            // logical min/max for capslock report is 5-10, so anything outside
            // of that is out of bounds
            if report[1] < 5 || report[1] > 10 {
                self.capslockOOB = Int(report[1])
                print("Detected capslock OOB value \(self.capslockOOB)")
            }
            
            return kIOReturnSuccess
        }
        
        let q = DispatchQueue(label: "")
        userDevice.setDispatchQueue(q)
        userDevice.activate()
        
        let deviceExp = XCTestExpectation(description: "HIDUserDevice enuemration")
        
        let manager = HIDManager()
        manager.setDeviceMatching([kIOHIDPhysicalDeviceUniqueIDKey : uuid.uuidString])
        manager.setDeviceNotificationHandler { (device, added) in
            if added {
                print("Device added \(device)")
                self.device = device
                deviceExp.fulfill()
            }
        }
        
        manager.setDispatchQueue(q)
        manager.open()
        manager.activate()
        
        let result = XCTWaiter.wait(for: [deviceExp], timeout: 5)
        XCTAssert(result == XCTWaiter.Result.completed)
        
        self.userDevice = userDevice
        self.manager = manager
    }
    
    override func tearDown() {
        manager.close()
        manager.cancel()
        userDevice.cancel()
        super.tearDown()
    }
    
    func testOutOfBounds() {
        for element in device.elements(matching: [:]) {
            if element.usage == kHIDUsage_LED_NumLock ||
                element.usage == kHIDUsage_LED_CapsLock {
                
                element.integerValue = 5
                
                do {
                    try device.commit([element], direction: HIDDeviceCommitDirection.out)
                } catch let error as NSError {
                    print("device.setElementValue failed: \(error)")
                }
            }
        }
        
        // Since numlock logical min/max is 0-5, we would expect an out of
        // bounds value of 6 to be set
        XCTAssert(numlockOOB == 6)
        
        // Since capslock logical min/max is 5-10, we would expect an out of
        // bounds value of 0 to be set
        XCTAssert(capslockOOB == 0)
    }
}
