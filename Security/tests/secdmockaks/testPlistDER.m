/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#import <XCTest/XCTest.h>
#include "utilities/der_plist.h"
#include "SecCFWrappers.h"

@interface testPlistDER : XCTestCase
@end

static CFDataRef CreateDERFromDictionary(CFDictionaryRef di, CFErrorRef *error)
{
    size_t size = der_sizeof_plist(di, error);
    if (size == 0)
        return NULL;
    uint8_t *der = malloc(size);
    if (der == NULL) {
        return NULL;
    }
    der_encode_plist(di, error, der, der+size);
    return CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, der, size, kCFAllocatorMalloc);
}

@implementation testPlistDER


- (void)testSecPListLargeData {
    NSMutableData *data = [NSMutableData dataWithLength:650000];
    memset([data mutableBytes], 'A', [data length]);

    NSDictionary *dictionary = @{
        @"BackupKey" : [NSMutableData dataWithLength:32],
        @"DeviceID" : data,
        @"EscrowRecord" : @"<null>",
        @"PreferIDFragmentation" : @(1),
        @"PreferIDS" : @(0),
        @"PreferIDSAckModel" : @(1),
        @"SecurityProperties" : @{},
        @"SerialNumber" : @"C02TD01QHXCW",
        @"TransportType" : @"KVS",
        @"Views" : @[
                @"iCloudIdentity",
                @"BackupBagV0",
                @"PCS-Maildrop",
                @"PCS-iMessage",
                @"PCS-Notes",
                @"PCS-FDE",
                @"PCS-MasterKey",
                @"NanoRegistry",
                @"PCS-Feldspar",
                @"PCS-iCloudDrive",
                @"AccessoryPairing",
                @"ContinuityUnlock",
                @"WatchMigration",
                @"PCS-Sharing",
                @"PCS-Photos",
                @"PCS-Escrow",
                @"AppleTV",
                @"HomeKit",
                @"PCS-Backup",
                @"PCS-CloudKit"
        ],
    };
    CFErrorRef error = NULL;

    size_t size = der_sizeof_plist((__bridge CFTypeRef)dictionary, &error);
    XCTAssertNotEqual(size, (size_t)0, "no data?: %@", error);
    CFReleaseNull(error);

    uint8_t *der = malloc(size);
    uint8_t *der_end = der + size;
    uint8_t *der_fin = der_encode_plist((__bridge CFTypeRef)dictionary, &error, der, der_end);

    XCTAssert(error == NULL, "error should be NULL: %@", error);
    XCTAssertEqual(der, der_fin, "under/over-flow");

    free(der);

    CFReleaseNull(error);

    NSData *outdata = (__bridge NSData *)CreateDERFromDictionary((__bridge CFTypeRef)dictionary, &error);
    XCTAssertEqual(error, NULL, "error should be NULL: %@", error);
    XCTAssertNotEqual(outdata, NULL, "should have data");

}

- (void)testSecPListLargeDataOtherThread
{
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{
        [self testSecPListLargeData];
        dispatch_semaphore_signal(sema);
    });
    dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
}


@end
