//
//  TestIOHIDEventDriver_Temperature.m
//  IOHIDFamilyUnitTests
//
//  Created by Paul Doerr on 2/25/19.
//

#import <XCTest/XCTest.h>
#import "IOHIDUnitTestUtility.h"
#import "IOHIDEventDriverTestCase.h"


#define HIDEnvironmentalTemperature \
0x05, 0x20,                  /* (GLOBAL) USAGE_PAGE         0x0020 Sensor Device Page  */\
0x09, 0x33,                  /* (LOCAL)  USAGE              0x00200033 Environmental Temperature (CACP=Application or Physical Collection)  */\
0xA1, 0x01,                  /* (MAIN)   COLLECTION         0x01 Application (Usage=0x00200033: Page=Sensor Device Page, Usage=Environmental Temperature, Type=CACP) */\
0x85, 0x01,                  /*   (GLOBAL) REPORT_ID          0x01 (1)    */\
0x46, 0xFF, 0x7F,            /*   (GLOBAL) PHYSICAL_MAXIMUM   0x7FFF (32767)     */\
0x36, 0x00, 0x80,            /*   (GLOBAL) PHYSICAL_MINIMUM   0x8000 (-32768)     */\
0x26, 0xFF, 0x7F,            /*   (GLOBAL) LOGICAL_MAXIMUM    0x7FFF (32767)     */\
0x16, 0x00, 0x80,            /*   (GLOBAL) LOGICAL_MINIMUM    0x8000 (-32768)     */\
0x55, 0x0E,                  /*   (GLOBAL) UNIT_EXPONENT      0x0E (Unit Value x 10⁻²)    */\
0x05, 0x20,                  /*   (GLOBAL) USAGE_PAGE         0x0020 Sensor Device Page <-- Redundant: USAGE_PAGE is already 0x0020   */\
0x0A, 0x34, 0x04,            /*   (LOCAL)  USAGE              0x00200434 Data Field: Environmental Temperature (SV=Static Value)    */\
0x75, 0x10,                  /*   (GLOBAL) REPORT_SIZE        0x10 (16) Number of bits per field     */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields     */\
0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (1 field x 16 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0xC0,                        /* (MAIN)   END_COLLECTION     Application */\

typedef struct __attribute__((packed))
{
    uint8_t reportID;
    int16_t data;
} HIDEnvironmentalTemperatureInputReport;

static uint16_t testTemps [] = {
    2000,  // 20C
    3000,  // 30C
    4000,  // 40C
};


@interface TestIOHIDEventDriver_Temperature : IOHIDEventDriverTestCase

@property XCTestExpectation * testEventExpectation;

@end

@implementation TestIOHIDEventDriver_Temperature

- (void)setUp {

    self.testEventExpectation = [[XCTestExpectation alloc] initWithDescription:@"Expectation: event"];
    self.testEventExpectation.expectedFulfillmentCount = sizeof(testTemps)/sizeof(testTemps[0]);
    
    static uint8_t descriptor [] = {
        HIDEnvironmentalTemperature
    };
    
    self.hidDeviceDescriptor = [NSData dataWithBytes:descriptor length:sizeof(descriptor)];
    
    [super setUp];
}

- (void)tearDown {

    [super tearDown];
}


- (void)testEnvironmentalTemperature {
    IOReturn        status;
    XCTWaiterResult result;
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.eventSystem != NULL);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.userDevice != NULL, "User device description: %@", self.userDeviceDescription);
    
    result = [XCTWaiter waitForExpectations:@[self.testServiceExpectation] timeout:10];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN | COLLECT_IOREG,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@",
                                result,
                                self.testServiceExpectation);

    HIDEnvironmentalTemperatureInputReport report;

    memset(&report, 0, sizeof(report));

    report.reportID = 1;

    for (NSUInteger index = 0 ; index < sizeof(testTemps)/sizeof(testTemps[0]); ++index) {
        report.data = testTemps[index];
        
        status = IOHIDUserDeviceHandleReport(self.userDevice, (uint8_t *)&report, sizeof(report));
        HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_HIDUTIL | COLLECT_IOREG | COLLECT_LOGARCHIVE,
                                    status == kIOReturnSuccess,
                                    "IOHIDUserDeviceHandleReport:0x%x",
                                    status);
    }

    result = [XCTWaiter waitForExpectations:@[self.testEventExpectation] timeout:10];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_ALL,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@",
                                (long)result,
                                self.testEventExpectation);

    IOHIDFloat temp;

    temp = IOHIDEventGetFloatValue((IOHIDEventRef)self.events[0], kIOHIDEventFieldTemperatureLevel);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST,
                                temp == 20.0,
                                "temp:%f events:%@",
                                temp,
                                self.events);

    temp = IOHIDEventGetFloatValue((IOHIDEventRef)self.events[1], kIOHIDEventFieldTemperatureLevel);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST,
                                temp == 30.0,
                                "temp:%f events:%@",
                                temp,
                                self.events);

    temp = IOHIDEventGetFloatValue((IOHIDEventRef)self.events[2], kIOHIDEventFieldTemperatureLevel);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST,
                                temp == 40.0,
                                "temp:%f events:%@",
                                temp,
                                self.events);
}


-(void) handleEvent: (IOHIDEventRef) event fromService:(IOHIDServiceClientRef __unused) service
{
    [super handleEvent:event fromService:service];
    
    if (IOHIDEventGetType(event) == kIOHIDEventTypeTemperature) {
        [self.testEventExpectation fulfill];
    }
}




@end
