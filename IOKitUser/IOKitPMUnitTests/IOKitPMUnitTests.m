//
//  IOKitPMUnitTests.m
//  IOKitPMUnitTests
//
//  Created by Faramola Isiaka on 7/9/21.
//

#import <XCTest/XCTest.h>
#include "IOPMLib.h"
#include "IOPMLibPrivate.h"
#include "IOKitPMStubs.h"

@interface IOKitPMUnitTests : XCTestCase

@end

@implementation IOKitPMUnitTests

- (void)setUp {
    // Put setup code here. This method is called before the invocation of each test method in the class.
}

- (void)tearDown {
    // Put teardown code here. This method is called after the invocation of each test method in the class.
    IOKitPMStubsTeardown();
}

- (CFDictionaryRef)createAssertionPropertiesDict:(CFStringRef) type value:(IOPMAssertionLevel)  value name:(CFStringRef) name details:(CFStringRef) details readableReason:(CFStringRef) readableReason bundlePath:(CFStringRef) bundlePath plugInID:(CFStringRef) plugInID frameworkID:(CFStringRef) frameworkID timeout:(CFTimeInterval) timeout timeoutAction:(CFStringRef) timeoutAction
{
    CFMutableDictionaryRef properties = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFNumberRef numRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &value);
    CFDictionarySetValue(properties, kIOPMAssertionTypeKey, type);
    CFDictionarySetValue(properties, kIOPMAssertionLevelKey, numRef);
    CFDictionarySetValue(properties, kIOPMAssertionNameKey, name);
    
    // these parameters are optional
    if(details){
        CFDictionarySetValue(properties, kIOPMAssertionDetailsKey, details);
    }
    if(readableReason){
        CFDictionarySetValue(properties, kIOPMAssertionHumanReadableReasonKey, readableReason);
    }
    if(bundlePath){
        CFDictionarySetValue(properties, kIOPMAssertionLocalizationBundlePathKey, bundlePath);
    }
    if(plugInID){
        CFDictionarySetValue(properties, kIOPMAssertionPlugInIDKey, plugInID);
    }
    if(frameworkID){
        CFDictionarySetValue(properties, kIOPMAssertionFrameworkIDKey, frameworkID);
    }
    if(timeout){
        CFNumberRef timeoutNum = CFNumberCreate(0, kCFNumberDoubleType, &timeout);
        CFDictionarySetValue(properties, kIOPMAssertionTimeoutKey, timeoutNum);
    }
    if (timeoutAction){
        CFDictionarySetValue(properties, kIOPMAssertionTimeoutActionKey, timeoutAction);
    }
    return properties;
}

- (void)testNotifyStubsExample {
    int token;
    notify_register_dispatch("string", &token,dispatch_get_main_queue(), ^(int t __unused){printf("Notify Stubs Work!\n");});
    notify_post("string");
}

- (void)testDispatchStubsExample {
    dispatch_source_t  ds = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, NULL);
    dispatch_source_set_event_handler(ds, ^{ printf("Dispatch Stubs Work!\n"); });
    dispatch_source_set_timer(ds, getCurrentTime(), 5*NSEC_PER_SEC, 0);
    advanceTime(16 * NSEC_PER_SEC);

}
- (void)testXPCStubsExample {
    xpc_connection_t fakeConn = xpc_connection_create_mach_service("fake", NULL, 0);
    xpc_object_t msg = xpc_dictionary_create(NULL, NULL, 0);
    xpc_object_t reply = xpc_dictionary_create(NULL, NULL, 0);
    addReplyForConnection("fake", reply);
    xpc_connection_set_event_handler(fakeConn, ^(xpc_object_t __unused e ) {
        printf("XPC Stubs work!\n");
    });
    xpc_connection_resume(fakeConn);
    xpc_object_t fakeReply = xpc_connection_send_message_with_reply_sync(fakeConn, msg);
    XCTAssert(fakeReply == reply, "Did not return the correct reply object!\n");
    xpc_connection_cancel(fakeConn);
}

@end
