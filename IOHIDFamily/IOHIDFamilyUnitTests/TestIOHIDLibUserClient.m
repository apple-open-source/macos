//
//  TestIOHIDLibUserClient.m
//  IOHIDFamilyUnitTests
//
//  Created by Josh Kergan on 3/19/20.
//

#import <XCTest/XCTest.h>

#import <IOKit/IOKitLib.h>
#import <IOHIDFamily/IOHIDLibUserClient.h>

@interface TestIOHIDLibUserClient : XCTestCase {
    io_connect_t conn;

}

@end

@implementation TestIOHIDLibUserClient

- (void)setUp {
    kern_return_t ret = KERN_SUCCESS;
    io_connect_t connection = 0;
    mach_port_t master_port = 0;
    io_iterator_t itr = 0;
    io_service_t service = 0;

    ret = host_get_io_master(mach_host_self(), &master_port);
    if (KERN_SUCCESS != ret)
    {
        NSLog(@"Failed getting master port");
        goto cleanup;
    }

    ret = IOServiceGetMatchingServices(master_port, IOServiceMatching("IOHIDDevice"), &itr);
    if (KERN_SUCCESS != ret)
    {
        NSLog(@"Failed getting matching services");
        goto cleanup;
    }

    while(IOIteratorIsValid(itr) && (service = IOIteratorNext(itr))) {
        ret = IOServiceOpen(service, mach_task_self(), kIOHIDLibUserClientConnectManager, &connection);
        if (KERN_SUCCESS != ret)
        {
            continue;
        }
    }
    if (itr) {
        IOObjectRelease(itr);
        itr = 0;
    }

cleanup:
    if (KERN_SUCCESS == ret) {
        conn = connection;
    } else {
        XCTFail("Unable to get connection to IOHIDDevice");
    }
}

- (void)tearDown {
    if (conn) {
        IOServiceClose(conn);
        conn = 0;
    }
}

// Based on PoC from <rdar://problem/58387485> IOHIDLibUserClient::registerNotificationPortGated Memory Corruption
- (void)testInvalidPort {
    kern_return_t ret = KERN_SUCCESS;
    uint64_t scalar = 10; // This value seems to affect things.

    ret = IOConnectCallMethod(conn,
                              kIOHIDLibUserClientOpen,
                              &scalar, 1,
                              NULL, 0,
                              NULL, 0,
                              NULL, 0);

    if (KERN_SUCCESS != ret)
    {
        NSLog(@"Error opening hid %d\n",ret);
    } else {
        NSLog(@"open hid ret: %d\n", ret);
    }


    mach_port_t notification_port = MACH_PORT_NULL;
    // If we allocate the port, then we don't get a crash
    //mach_port_allocate(mach_task_self(),MACH_PORT_RIGHT_RECEIVE,&notification_port);

    uint64_t ref[8] = {0};
    IOConnectSetNotificationPort(conn, kIOHIDLibUserClientDeviceValidPortType, notification_port, (uintptr_t)ref);
}

@end
