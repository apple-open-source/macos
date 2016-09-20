//
//  KCDerTest.m
//  Security
//
//

#import <XCTest/XCTest.h>

#import <Foundation/Foundation.h>
#import <KeychainCircle/KCDer.h>

@interface KCDerTest : XCTestCase

@end

@implementation KCDerTest

- (void)setUp {
    [super setUp];
    // Put setup code here. This method is called before the invocation of each test method in the class.
}

- (void)tearDown {
    // Put teardown code here. This method is called after the invocation of each test method in the class.
    [super tearDown];
}

- (void) roundTripData: (NSData*) data {
    NSError* error = nil;
    size_t size = kcder_sizeof_data(data, &error);

    XCTAssert(size != 0, @"Bad size: %@", data);

    if (size == 0)
        return;

    uint8_t buffer[size];
    error = nil;
    uint8_t* beginning = kcder_encode_data(data, &error, buffer, buffer + sizeof(buffer));

    XCTAssert(beginning != NULL, "Error encoding: %@", error);

    if (beginning == NULL)
        return;

    XCTAssertEqual(beginning, &buffer[0], @"Size != buffer use");

    NSData* recovered = nil;

    error = nil;
    const uint8_t* end = kcder_decode_data(&recovered, &error, buffer, buffer + sizeof(buffer));

    XCTAssert(end != NULL, "Error decoding: %@", error);

    if (end == NULL)
        return;

    XCTAssertEqual(end, buffer + sizeof(buffer), @"readback didn't use all the buffer");

    XCTAssertEqualObjects(data, recovered, @"Didn't get equal object");

}

- (void)testData {
    [self roundTripData: [NSData data]];

    uint8_t bytes[] = { 1, 2, 3, 0xFF, 4, 0x0, 0xA };
    [self roundTripData: [NSData dataWithBytes:bytes length:sizeof(bytes)]];
}

- (void) roundTripString: (NSString*) string {
    NSError* error = nil;

    size_t size = kcder_sizeof_string(string, &error);

    XCTAssert(size != 0, @"Bad size: %@", string);

    if (size == 0)
        return;

    uint8_t buffer[size];
    error = nil;
    uint8_t* beginning = kcder_encode_string(string, &error, buffer, buffer + sizeof(buffer));

    XCTAssert(beginning != NULL, "Error encoding: %@", error);

    if (beginning == NULL)
        return;

    XCTAssertEqual(beginning, &buffer[0], @"Size != buffer use");

    NSString* recovered = nil;

    error = nil;
    const uint8_t* end = kcder_decode_string(&recovered, &error, buffer, buffer + sizeof(buffer));

    XCTAssert(end != NULL, "Error decoding: %@", error);

    if (end == NULL)
        return;

    XCTAssertEqual(end, buffer + sizeof(buffer), @"readback didn't use all the buffer");

    XCTAssertEqualObjects(string, recovered, @"Didn't get equal object");
    
}

- (void)testString {
    [self roundTripString: [NSString stringWithCString:"Test" encoding:NSUTF8StringEncoding]];
    [self roundTripString: [NSString stringWithCString:"ü😍🐸✝️₧➜" encoding:NSUTF8StringEncoding]];
}


@end
