//
//  TestServicePlugin.swift
//  IOHIDFamilyUnitTests
//
//  Created by dekom on 10/1/18.
//

import Foundation
import XCTest
import HID

let VendorKeyboardDescriptor : [UInt8] = [
    0x06, 0x00, 0xFF,            /* (GLOBAL) USAGE_PAGE         0xFF00 Vendor-defined  */
    0x09, 0x28,                  /* (LOCAL)  USAGE              0xFF000028   */
    0xA1, 0x01,                  /* (MAIN)   COLLECTION         0x01 Application (Usage=0x00010006: Page=Generic Desktop Page, Usage=Keyboard, Type=CA) */
    0x05, 0x07,                  /*   (GLOBAL) USAGE_PAGE         0x0007 Keyboard/Keypad Page    */
    0x19, 0xE0,                  /*   (LOCAL)  USAGE_MINIMUM      0x000700E0 Keyboard Left Control (DV=Dynamic Value)    */
    0x29, 0xE7,                  /*   (LOCAL)  USAGE_MAXIMUM      0x000700E7 Keyboard Right GUI (DV=Dynamic Value)    */
    0x15, 0x00,                  /*   (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14   */
    0x25, 0x01,                  /*   (GLOBAL) LOGICAL_MAXIMUM    0x01 (1)     */
    0x75, 0x01,                  /*   (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field     */
    0x95, 0x08,                  /*   (GLOBAL) REPORT_COUNT       0x08 (8) Number of fields     */
    0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (8 fields x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */
    0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields     */
    0x75, 0x08,                  /*   (GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field     */
    0x81, 0x01,                  /*   (MAIN)   INPUT              0x00000001 (1 field x 8 bits) 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull    */
    0x05, 0x08,                  /*   (GLOBAL) USAGE_PAGE         0x0008 LED Indicator Page    */
    0x19, 0x01,                  /*   (LOCAL)  USAGE_MINIMUM      0x00080001 Num Lock (OOC=On/Off Control)    */
    0x29, 0x05,                  /*   (LOCAL)  USAGE_MAXIMUM      0x00080005 Kana (OOC=On/Off Control)    */
    0x95, 0x05,                  /*   (GLOBAL) REPORT_COUNT       0x05 (5) Number of fields     */
    0x75, 0x01,                  /*   (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field     */
    0x91, 0x02,                  /*   (MAIN)   OUTPUT             0x00000002 (5 fields x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */
    0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields     */
    0x75, 0x03,                  /*   (GLOBAL) REPORT_SIZE        0x03 (3) Number of bits per field     */
    0x91, 0x01,                  /*   (MAIN)   OUTPUT             0x00000001 (1 field x 3 bits) 1=Constant 0=Array 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */
    0x05, 0x07,                  /*   (GLOBAL) USAGE_PAGE         0x0007 Keyboard/Keypad Page    */
    0x19, 0x00,                  /*   (LOCAL)  USAGE_MINIMUM      0x00070000 Keyboard No event indicated (Sel=Selector)    */
    0x2A, 0xFF, 0x00,            /*   (LOCAL)  USAGE_MAXIMUM      0x000700FF     */
    0x95, 0x05,                  /*   (GLOBAL) REPORT_COUNT       0x05 (5) Number of fields     */
    0x75, 0x08,                  /*   (GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field     */
    0x15, 0x00,                  /*   (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14   */
    0x26, 0xFF, 0x00,            /*   (GLOBAL) LOGICAL_MAXIMUM    0x00FF (255)     */
    0x81, 0x00,                  /*   (MAIN)   INPUT              0x00000000 (5 fields x 8 bits) 0=Data 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull    */
    0x05, 0xFF,                  /*   (GLOBAL) USAGE_PAGE         0x00FF Reserved    */
    0x09, 0x03,                  /*   (LOCAL)  USAGE              0x00FF0003     */
    0x75, 0x08,                  /*   (GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field <-- Redundant: REPORT_SIZE is already 8    */
    0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields     */
    0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (1 field x 8 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */
    0xC0,                        /* (MAIN)   END_COLLECTION     Application */
]

@objc(TestServicePlugin) class TestServicePlugin : XCTestCase {
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
        
        client.setMatching([kIOHIDVendorIDKey : 87654321,
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
        
        let desc = NSData(bytesNoCopy: UnsafeMutableRawPointer(mutating: VendorKeyboardDescriptor),
                          length: VendorKeyboardDescriptor.count,
                          freeWhenDone: false)
        
        let properties : [String : Any] = [kIOHIDReportDescriptorKey: desc,
                                           kIOHIDVendorIDKey: 87654321,
                                           kIOHIDServiceHiddenKey as String: true,
                                           kIOHIDServiceSupportKey: true]
        
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
    
    func testServicePlugin() {
        var result : Bool = false
        
        result = serviceClient.setProperty(1234, forKey: "TestHIDServicePluginSetProperty")
        XCTAssert(result, "serviceClient.setProperty failed")
        
        let prop = serviceClient.property(forKey: "TestHIDServicePluginGetProperty")
        XCTAssert(prop != nil, "servserviceClient.property failed")
        print("prop: \(String(describing: prop))")
        
        eventExp.expectedFulfillmentCount = 10;
        
        for i in 0..<10 {
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
        
        var waiter = XCTWaiter.wait(for: [eventExp], timeout: 5)
        XCTAssert(waiter == XCTWaiter.Result.completed)
        
        if let event = serviceClient.event(matching: ["EventType" : kIOHIDEventTypeKeyboard]) {
            print("event: \(event)")
            XCTAssert(event.type == kIOHIDEventTypeKeyboard)
        } else {
            XCTAssert(false, "serviceClient.event failed")
        }
        
        let debugInfo = serviceClient.property(forKey: kIOHIDServicePluginDebugKey) as? Dictionary<String,Any>
        XCTAssert(debugInfo != nil, "serviceClient.property failed for kIOHIDServicePluginDebugKey")
        
        print("debugInfo: \(String(describing: debugInfo))")
        
        XCTAssert(debugInfo?["PluginName"] as! String == "HIDServicePluginExample")
        XCTAssert(debugInfo?["cancelHandler"] as! Bool == true)
        XCTAssert(debugInfo?["dispatchQueue"] as! Bool == true)
        XCTAssert(debugInfo?["activated"] as! Bool == true)
        XCTAssert(debugInfo?["clientAdded"] as! Bool == true)
        
        let removalExp = XCTestExpectation(description: "service removed")
        serviceClient.setRemovalHandler {
            print("Service: \(self.serviceClient!) removed")
            removalExp.fulfill()
        }
        
        userDevice = nil;
        
        waiter = XCTWaiter.wait(for: [removalExp], timeout: 5)
        XCTAssert(waiter == XCTWaiter.Result.completed)
    }
}
