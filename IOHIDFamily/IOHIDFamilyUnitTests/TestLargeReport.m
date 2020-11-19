//
//  TestLargeReport.m
//  IOHIDFamilyUnitTests
//
//  Created by Josh Kergan on 2/19/20.
//

#import <XCTest/XCTest.h>

#import <Foundation/Foundation.h>

#import "IOHIDUnitTestDescriptors.h"
#import "IOHIDUnitTestUtility.h"

#import <HID/HID.h>
#import <IOKit/hid/IOHIDPrivateKeys.h>

#define kLargeInputReportId 0x0C
#define kSmallInputReportId 0x0B
#define kLargeFeatureReportId 0x06
#define kSmallFeatureReportId 0x07
#define kVariableInputReportID 0x20

typedef struct __attribute__((packed))
{
    uint8_t reportId; // ReportID: 12 input/6 feature/32 variable input
    uint8_t payload[32768];
} HIDLargeReport;

typedef struct __attribute__((packed))
{
    uint8_t reportId; // ReportID: 11 input/7 feature
    uint8_t payload[10];
} HIDSmallReport;

static uint8_t descriptor[] = {
    0x06, 0x00, 0xFF,                 /* Usage Page (Vendor Defined 0xFF00) */
    0x09, 0x80,                       /* Usage (0x80) */
    0xA1, 0x01,                       /* Collection (Application) */
    0x06, 0x00, 0xFF,                 /*   Usage Page (Vendor Defined 0xFF00) */
    0x09, 0x80,                       /*   Usage (0x80) */
    0x85, kLargeInputReportId,        /* Report ID (12) */
    0x75, 0x08,                       /*   Report Size (8) */
    0x96, 0x00, 0x80,                 /*   Report Count (32768) */
    0x15, 0x00,                       //   Logical Minimum......... (0)
    0x26, 0xFF, 0x00,                 //   Logical Maximum......... (255)
    0x81, 0x02,                       /* Input (Data,Var,Abs) */
    0x85, kLargeFeatureReportId,      /* Report ID (6) */
    0x06, 0x00, 0xFF,                 /*   Usage Page (Vendor Defined 0xFF00) */
    0x09, 0x80,                       /*   Usage (0x80) */
    0x75, 0x08,                       /*   Report Size (8) */
    0x96, 0x00, 0x80,                 /*   Report Count (32768) */
    0x15, 0x00,                       //   Logical Minimum......... (0)
    0x26, 0xFF, 0x00,                 //   Logical Maximum......... (255)
    0xB1, 0x02,                       /* Feature (Data,Var,Abs) */
    0x85, kSmallInputReportId,        /* Report ID (11) */
    0x06, 0x00, 0xFF,                 /*   Usage Page (Vendor Defined 0xFF00) */
    0x09, 0x80,                       /*   Usage (0x80) */
    0x75, 0x08,                       /*   Report Size (8) */
    0x95, 0x0A,                       /*   Report Count (10) */
    0x15, 0x00,                       //   Logical Minimum......... (0)
    0x26, 0xFF, 0x00,                 //   Logical Maximum......... (255)
    0x81, 0x02,                       /* Input (Data,Var,Abs) */
    0x85, kSmallFeatureReportId,      /* Report ID (7) */
    0x06, 0x00, 0xFF,                 /*   Usage Page (Vendor Defined 0xFF00) */
    0x09, 0x80,                       /*   Usage (0x80) */
    0x75, 0x08,                       /*   Report Size (8) */
    0x95, 0x0A,                       /*   Report Count (10) */
    0x15, 0x00,                       //   Logical Minimum......... (0)
    0x26, 0xFF, 0x00,                 //   Logical Maximum......... (255)
    0xB1, 0x02,                       /*   Feature (Data,Var,Abs) */
    0x06, 0x00, 0xFF,                 /* Usage Page (Vendor Defined 0xFF00) */
    0x09, 0x23,                       /* Usage (0x23) (kHIDUsage_AppleVendor_Message) */
    0xA1, 0x01,                       /* Collection (Application) */
    0x06, 0x00, 0xFF,                 /*   Usage Page (Vendor Defined 0xFF00) */
    0x09, 0x80,                       /*   Usage (0x80) */
    0x75, 0x08,                       /*   Report Size (8) */
    0x96, 0x00, 0x80,                 /*   Report Count (32768) */
    0x85, kVariableInputReportID,     /*   Report ID (12) */
    0x81, 0x02,                       /*   Input (Data,Var,Abs) */
    0xC0,                             /* End Collection */
    0xC0,              /* End Collection */
};

#define LARGE_SET_REPORT_FILL 0xa
#define LARGE_GET_REPORT_FILL 0x11

#define SMALL_GET_REPORT_FILL 0xbc
#define SMALL_SET_REPORT_FILL 0xab

#define LARGE_INPUT_FILL 0xba
#define SMALL_INPUT_FILL 0xda

static const uint32_t EXPECT_INPUT_REPORT_COUNT = 10;
static const uint32_t EXPECT_SET_REPORT_COUNT = 5;
static const uint32_t EXPECT_GET_REPORT_COUNT = 5;
static const uint32_t EXPECT_VAR_SMALL_COUNT = 10;
static const uint32_t EXPECT_VAR_LARGE_COUNT = 10;


static const double REPORT_TIMEOUT = 3;

@interface TestLargeReport : XCTestCase {
    HIDUserDevice *_userDevice;
    HIDManager *_manager;
    HIDDevice *_device;
    NSString *_uuid;

    // Test Expectations
    XCTestExpectation *_devAdded;

    XCTestExpectation *_largeSetReportExp;
    XCTestExpectation *_largeInputReportExp;
    XCTestExpectation *_varInputReportExp;
}
@end

@implementation TestLargeReport

- (void)setUp {
    [super setUp];
    _uuid = [[NSUUID UUID] UUIDString];

    _largeSetReportExp = [[XCTestExpectation alloc] initWithDescription:@"Recieved Large Set Report"];
    _largeSetReportExp.expectedFulfillmentCount = EXPECT_SET_REPORT_COUNT;
    _largeInputReportExp = [[XCTestExpectation alloc] initWithDescription:@"Recieved Large Input Report"];
    _largeInputReportExp.expectedFulfillmentCount = EXPECT_INPUT_REPORT_COUNT;
    _devAdded = [[XCTestExpectation alloc] initWithDescription:@"Device matched"];

    NSData *desc = [NSData dataWithBytes:descriptor length:sizeof(descriptor)];
    dispatch_queue_t udQueue = dispatch_queue_create("com.apple.hid.largeReportDevice", DISPATCH_QUEUE_SERIAL);
    dispatch_queue_t deviceQueue = dispatch_queue_create("com.apple.hid.manager.device", DISPATCH_QUEUE_SERIAL);

    NSDictionary *matching = @{ @kIOHIDPhysicalDeviceUniqueIDKey: _uuid };
    NSDictionary *devProps = @{ @kIOHIDPhysicalDeviceUniqueIDKey: _uuid,
                                @kIOHIDReportDescriptorKey : desc };

    _userDevice = [[HIDUserDevice alloc] initWithProperties:devProps];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE,
                               _userDevice != NULL);

    TestLargeReport __weak * weakSelf = self;
    // Device setup
    [_userDevice setGetReportHandler:^IOReturn(HIDReportType __unused type, NSInteger reportID, void * _Nonnull report, NSInteger * _Nonnull reportLength) {
        NSUInteger unsignedReportLength = (NSUInteger)*reportLength;
        if (unsignedReportLength == sizeof(HIDLargeReport)) {
            HIDLargeReport *largeReport = (HIDLargeReport*)report;
            memset(largeReport->payload, LARGE_GET_REPORT_FILL, sizeof(largeReport->payload));
            largeReport->reportId = reportID;
            *reportLength = sizeof(HIDLargeReport);
            return kIOReturnSuccess;
        }

        NSLog(@"Got undersized report buffer: %ld", unsignedReportLength);
        return kIOReturnUnderrun;
    }];

    [_userDevice setSetReportHandler:^IOReturn(HIDReportType __unused type, NSInteger __unused reportID, const void * _Nonnull report, NSInteger reportLength) {
        TestLargeReport __strong * strongSelf = weakSelf;
        if (!strongSelf) {
            return kIOReturnOffline;
        }

        NSUInteger realLength = (NSUInteger)reportLength;

        if (realLength == sizeof(HIDLargeReport)) {
            HIDLargeReport *largeReport = (HIDLargeReport*)report;
            void *cpy = malloc(sizeof(largeReport->payload));
            memcpy(cpy, largeReport->payload, sizeof(largeReport->payload));
            for (uint8_t* ptr = cpy; (size_t)(ptr - (uint8_t*)cpy) < sizeof(largeReport->payload); ptr++) {
                if (*ptr != LARGE_SET_REPORT_FILL) {
                    NSLog(@"Set report contains unexpected data.");
                    return kIOReturnInternalError;
                }
            }
            [strongSelf->_largeSetReportExp fulfill];
            return kIOReturnSuccess;
        }

        NSLog(@"Got undersized report buffer: %ld", realLength);
        return kIOReturnUnderrun;
    }];

    [_userDevice setDispatchQueue:udQueue];
    [_userDevice activate];

    // Device client setup
    _manager = [HIDManager new];
    [_manager setDeviceMatching:matching];
    [_manager setDispatchQueue:deviceQueue];
    [_manager setDeviceNotificationHandler:^(HIDDevice * _Nonnull device, BOOL added) {
        TestLargeReport __strong *strongSelf = weakSelf;
        if (!strongSelf) {
            return;
        }
        if (added) {
            _device = device;
            [strongSelf->_devAdded fulfill];
        } else {
            _device = nil;
        }
    }];
    [_manager setInputReportHandler:^(HIDDevice * _Nonnull __unused sender,
                                      uint64_t __unused timestamp,
                                      HIDReportType __unused type,
                                      NSInteger __unused reportID,
                                      NSData * _Nonnull report) {
        TestLargeReport __strong *strongSelf = weakSelf;
        if (!strongSelf) {
            return;
        }
        if ([report length] == sizeof(HIDSmallReport)) {
            const HIDSmallReport* reportPtr = (const HIDSmallReport*)(report.bytes);
            assert(reportPtr->reportId == kVariableInputReportID);
            for (unsigned int i = 0; i < sizeof(reportPtr->payload); ++i) {
                if (reportPtr->payload[i] != LARGE_INPUT_FILL) {
                    NSLog(@"Unexpected value in variable report payload");
                    return;
                }
            }
            if (reportPtr->reportId == kVariableInputReportID) {
                [strongSelf->_varInputReportExp fulfill];
            } else {
                NSLog(@"Got unexpected report ID in input report. %d", reportPtr->reportId);
            }
        } else if ([report length] < sizeof(HIDLargeReport)) {
            NSLog(@"Got undersized input report buffer: %ld", [report length]);
            return;
        } else if ([report length] == sizeof(HIDLargeReport)) {
            const HIDLargeReport* reportPtr = (const HIDLargeReport*)(report.bytes);
            for (unsigned int i = 0; i < sizeof(reportPtr->payload); ++i) {
                if (reportPtr->payload[i] != LARGE_INPUT_FILL) {
                    NSLog(@"Unexpected value in large report payload");
                    return;
                }
            }
            if (reportPtr->reportId == kLargeInputReportId) {
                [strongSelf->_largeInputReportExp fulfill];
            } else if (reportPtr->reportId == kVariableInputReportID) {
                [strongSelf->_varInputReportExp fulfill];
            } else {
                NSLog(@"Got unexpected report ID in input report. %d", reportPtr->reportId);
            }
        } else {
            NSLog(@"Got unexpected report length.");
        }
    }];
    [_manager open];
    [_manager activate];
}

- (void)tearDown {
    [_device cancel];
    [_manager close];
    [_manager cancel];
    [super tearDown];
}

- (void)testLargeSetReports {
    HIDLargeReport *inReport = malloc(sizeof(HIDLargeReport));
    memset(inReport->payload, LARGE_SET_REPORT_FILL, sizeof(inReport->payload));

    bool res;
    XCTWaiterResult result;
    NSError *err = nil;

    result = [XCTWaiter waitForExpectations:@[_devAdded] timeout:REPORT_TIMEOUT];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL, result == XCTWaiterResultCompleted);

    for (size_t i = 0; i < EXPECT_SET_REPORT_COUNT; ++i) {
        res = [_device setReport:inReport reportLength:sizeof(HIDLargeReport) withIdentifier:kLargeFeatureReportId forType:HIDReportTypeFeature error:&err];
        HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL, res, "setReport failed ret: %d err: %@", res, err);
    }

    result = [XCTWaiter waitForExpectations:@[_largeSetReportExp] timeout:REPORT_TIMEOUT];
    HIDXCTAssertWithParameters(COLLECT_ALL, result == XCTWaiterResultCompleted);
}

- (void)testLargeGetReports {
    NSMutableData *reportData = [[NSMutableData alloc] initWithCapacity:sizeof(HIDLargeReport)];

    bool res;
    XCTWaiterResult result;

    result = [XCTWaiter waitForExpectations:@[_devAdded] timeout:REPORT_TIMEOUT];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL, result == XCTWaiterResultCompleted);

    for (size_t i = 0; i < EXPECT_GET_REPORT_COUNT; ++i) {
        NSError *err = nil;
        NSInteger reportSize = sizeof(HIDLargeReport);
        res = [_device getReport:reportData.mutableBytes reportLength:&reportSize withIdentifier:kLargeFeatureReportId forType:HIDReportTypeFeature error:&err];
        HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL, res, "getReport failed ret: %d err: %@", res, err);
        if (reportSize == sizeof(HIDLargeReport)) {
            bool valid = true;

            HIDLargeReport *report = (HIDLargeReport*)reportData.bytes;
            for (unsigned int index = 0; index < sizeof(report->payload); index++) {
                valid &= report->payload[index] == LARGE_GET_REPORT_FILL;
            }

            HIDXCTAssertWithLogs(valid, "Unexpected data in large get report");
        } else {
            XCTFail("Didn't get report buffer back with expected size:%u", (uint32_t)reportSize);
        }
    }
}

- (void)testLargeInputReports {
    HIDLargeReport *inReport = malloc(sizeof(HIDLargeReport));
    memset(inReport->payload, LARGE_INPUT_FILL, sizeof(inReport->payload));
    bool res;
    XCTWaiterResult result;
    NSError *err = nil;

    result = [XCTWaiter waitForExpectations:@[_devAdded] timeout:REPORT_TIMEOUT];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL, result == XCTWaiterResultCompleted);

    inReport->reportId = kLargeInputReportId;
    NSData* inputData = [[NSData alloc] initWithBytes:inReport length:sizeof(HIDLargeReport)];
    for (size_t i = 0; i < EXPECT_INPUT_REPORT_COUNT; ++i) {
        res = [_userDevice handleReport:inputData error:&err];
        HIDXCTAssertWithParameters(COLLECT_ALL | RETURN_FROM_TEST, res, "handleReport failed ret: %d err: %@", res, err);
    }

    result = [XCTWaiter waitForExpectations:@[_largeInputReportExp] timeout:REPORT_TIMEOUT];
    HIDXCTAssertWithParameters(COLLECT_ALL | RETURN_FROM_TEST, result == XCTWaiterResultCompleted);
}

- (void)testVariableSizeInputReports {
    HIDLargeReport *inReport = malloc(sizeof(HIDLargeReport));
    memset(inReport->payload, LARGE_INPUT_FILL, sizeof(inReport->payload));
    bool res;
    XCTWaiterResult result;
    NSError *err = nil;

    result = [XCTWaiter waitForExpectations:@[_devAdded] timeout:REPORT_TIMEOUT];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL, result == XCTWaiterResultCompleted);

    inReport->reportId = kVariableInputReportID;
    NSData* inputLargeData = [[NSData alloc] initWithBytes:inReport length:sizeof(HIDLargeReport)];
    NSData* inputSmallData = [[NSData alloc] initWithBytes:inReport length:sizeof(HIDSmallReport)];

    _varInputReportExp = [[XCTestExpectation alloc] initWithDescription:@"Variable Large Reports 1"];
    _varInputReportExp.expectedFulfillmentCount = EXPECT_VAR_LARGE_COUNT;
    for (size_t i = 0; i < EXPECT_VAR_LARGE_COUNT; ++i) {
        res = [_userDevice handleReport:inputLargeData error:&err];
        HIDXCTAssertWithParameters(COLLECT_ALL | RETURN_FROM_TEST, res, "handleReport failed ret: %d err: %@", res, err);
    }
    result = [XCTWaiter waitForExpectations:@[_varInputReportExp] timeout:REPORT_TIMEOUT];
    HIDXCTAssertWithParameters(COLLECT_ALL | RETURN_FROM_TEST, result == XCTWaiterResultCompleted);

    _varInputReportExp = [[XCTestExpectation alloc] initWithDescription:@"Variable Small Reports 1"];
    _varInputReportExp.expectedFulfillmentCount = EXPECT_VAR_SMALL_COUNT;
    for (size_t i = 0; i < EXPECT_VAR_SMALL_COUNT; ++i) {
        res = [_userDevice handleReport:inputSmallData error:&err];
        HIDXCTAssertWithParameters(COLLECT_ALL | RETURN_FROM_TEST, res, "handleReport failed ret: %d err: %@", res, err);
    }
    result = [XCTWaiter waitForExpectations:@[_varInputReportExp] timeout:REPORT_TIMEOUT];
    HIDXCTAssertWithParameters(COLLECT_ALL | RETURN_FROM_TEST, result == XCTWaiterResultCompleted);

    _varInputReportExp = [[XCTestExpectation alloc] initWithDescription:@"Variable Large Reports 2"];
    _varInputReportExp.expectedFulfillmentCount = EXPECT_VAR_LARGE_COUNT;
    for (size_t i = 0; i < EXPECT_VAR_LARGE_COUNT; ++i) {
        res = [_userDevice handleReport:inputLargeData error:&err];
        HIDXCTAssertWithParameters(COLLECT_ALL | RETURN_FROM_TEST, res, "handleReport failed ret: %d err: %@", res, err);
    }
    result = [XCTWaiter waitForExpectations:@[_varInputReportExp] timeout:REPORT_TIMEOUT];
    HIDXCTAssertWithParameters(COLLECT_ALL | RETURN_FROM_TEST, result == XCTWaiterResultCompleted);
}

@end
