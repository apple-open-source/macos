//
//  NSObject+IOHIDTestController.m
//  IOHIDFamily
//
//  Created by YG on 10/24/16.
//
//

#import "IOHIDUserDeviceTestController.h"
#include <IOKit/IOKitLib.h>
#include <dispatch/private.h>
#include "IOHIDUnitTestUtility.h"


static NSString * deviceDescription =
  @"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
   "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">"
   "   <plist version=\"1.0\">          "
   "   <dict>                           "
   "     <key>VendorID</key>            "
   "     <integer>555</integer>         "
   "     <key>ProductID</key>           "
   "     <integer>555</integer>         "
   "     <key>ReportInterval</key>      "
   "     <integer>10000</integer>       "
   "     <key>RequestTimeout</key>      "
   "     <integer>5000000</integer>     "
   "     <key>UnitTestService</key>     "
   "     <true/>                        "
   "   </dict>                          "
   "   </plist>                         ";

static IOReturn UserDeviceSetReportCallbackStatic(void * _Nullable refcon, IOHIDReportType type, uint32_t reportID, uint8_t * report, CFIndex reportLength);

static IOReturn UserDeviceGetReportWithReturnLengthCallbackStatic(void * _Nullable refcon, IOHIDReportType type, uint32_t reportID, uint8_t * report, CFIndex * reportLength);


@implementation IOHIDUserDeviceTestController


-(nullable instancetype) initWithDescriptor: (nonnull NSData *) descriptor DeviceUniqueID: (NSString*) uniqueID andQueue:(nullable dispatch_queue_t) queue {

    NSMutableDictionary* deviceConfig = [NSPropertyListSerialization propertyListWithData:[deviceDescription dataUsingEncoding:NSUTF8StringEncoding] options:NSPropertyListMutableContainers format:NULL error:NULL];
    deviceConfig [@kIOHIDReportDescriptorKey] = descriptor;
    deviceConfig [@kIOHIDPhysicalDeviceUniqueIDKey] = uniqueID;
    self->_userDeviceUniqueID = uniqueID;
    return [self initWithDeviceConfiguration:deviceConfig andQueue:queue];
}

-(nullable instancetype) initWithDeviceConfiguration: (nonnull NSDictionary *) deviceConfig andQueue:(nullable dispatch_queue_t) queue {

    self = [super init];
    if (!self) {
        return self;
    }

    self->_userDevice = IOHIDUserDeviceCreate(kCFAllocatorDefault, (CFDictionaryRef)deviceConfig);
    if (self.userDevice == NULL) {
        return nil;
    }

    io_service_t service = IOHIDUserDeviceCopyService(self->_userDevice);
    if (service == IO_OBJECT_NULL) {
        TestLog("initWithDeviceConfiguration: IOHIDUserDeviceCopyService == 0\n");
        return nil;
    }
    
    mach_timespec_t waitTime = {30, 0};
    IOServiceWaitQuiet(service, &waitTime);
    
    self.userDeviceSetReports = [[NSMutableArray alloc] init];
    if (!self.userDeviceSetReports) {
        return nil;
    }

    self->_userDeviceQueue = queue;
    if (queue) {
        IOHIDUserDeviceScheduleWithDispatchQueue (self.userDevice, self.userDeviceQueue);
    }
  
    IOHIDUserDeviceRegisterSetReportCallback(self.userDevice, UserDeviceSetReportCallbackStatic, (__bridge void * _Nullable)(self));
    IOHIDUserDeviceRegisterGetReportWithReturnLengthCallback (self.userDevice, UserDeviceGetReportWithReturnLengthCallbackStatic, (__bridge void * _Nullable)(self));
  return self;
}

-(IOReturn) handleReport: (nonnull NSData*) report withInterval: (NSInteger) interval {
  TestLog("handleReport: report:%@ interval:%dus\n", report, (unsigned int)interval);
  if (interval) {
      usleep ((useconds_t)interval);
  }
  return IOHIDUserDeviceHandleReport(self.userDevice, (uint8_t *)report.bytes , report.length);
}

-(IOReturn) handleReport: (nonnull uint8_t*) report Length: (NSUInteger) length  andInterval: (NSInteger) interval {
    NSData *reportData = [NSData dataWithBytes:report length:length];
    return [self handleReport:reportData withInterval:interval];
}

-(IOReturn) handleReport: (nonnull NSData*) report withTimestamp: (uint64_t) timestamp {
    TestLog("handleReport: report:%@ timestamp:%lld\n", report, timestamp);
    return IOHIDUserDeviceHandleReportWithTimeStamp(self.userDevice, timestamp, (uint8_t *)report.bytes , report.length);
}


-(IOReturn) handleReportAsync: (nonnull NSData*) report withTimestamp: (uint64_t) timestamp Callback: (IOHIDUserDeviceHandleReportAsyncCallback) callback Context: (void *) context  {
    TestLog("handleReportAsync: report:%@ timestamp:%lld\n", report, timestamp);
    return IOHIDUserDeviceHandleReportAsyncWithTimeStamp(self.userDevice, timestamp, (uint8_t *)report.bytes , report.length, callback, context);
}

-(IOReturn) handleReportAsync: (nonnull NSData*) report  Callback: (IOHIDUserDeviceHandleReportAsyncCallback) callback Context: (void *) context  {
    TestLog("handleReportAsync: report:%@\n", report);
    return IOHIDUserDeviceHandleReportAsync(self.userDevice, (uint8_t *)report.bytes , report.length, callback, context);
}

-(void)dealloc {
    
    [self invalidate];
    
    if (self.userDevice) {
        CFRelease (self.userDevice);
    }
}

-(IOReturn) handleReports: (nonnull NSArray *) reports withInterval: (NSInteger) interval {
    IOReturn result = kIOReturnBadArgument;
    for (id report in  reports) {
        usleep ((useconds_t)interval);
        result = [self handleReport:report withInterval: interval];
        if (result) {
            break;
        }
    }
  return result;
}

-(IOReturn) handleReportsWithIntervals: (nonnull NSArray *) reports {
    IOReturn result = kIOReturnBadArgument;
    for (id reportDict in  reports) {
        if (![reportDict isKindOfClass: [NSDictionary class]]) {
            return kIOReturnBadArgument;
        }
        NSData* report    = reportDict[@"report"];
        NSNumber* interval= reportDict[@"interval"];
        if (!report || [report isKindOfClass: [NSData class]] || !interval ||  [report isKindOfClass: [NSNumber class]]) {
            return kIOReturnBadArgument;
        }
        result = [self handleReport:report withInterval: interval.unsignedIntValue];
        if (result) {
            break;
        }
    }
    return result;
}


-(IOReturn) UserDeviceSetReportCallback: (IOHIDReportType) type Id: (uint32_t) reportID Bytes: (uint8_t*) report Length: (CFIndex) length {
    TestLog("UserDeviceSetReport: type:%d id:%d report:%p, lenght:%ld\n", type, reportID, report, length);
    NSData* reportData = [NSData dataWithBytes: report length:length];
    NSDictionary * reportDict = @{@"report":reportData};
    [self.userDeviceSetReports addObject: reportDict];
    return kIOReturnSuccess;
}

-(void)invalidate {
    if (self->_userDeviceQueue) {
        IOHIDUserDeviceUnscheduleFromDispatchQueue(self.userDevice,  self->_userDeviceQueue);
        self->_userDeviceQueue = nil;
    }
}

@end

IOReturn UserDeviceSetReportCallbackStatic(void * _Nullable refcon, IOHIDReportType type, uint32_t reportID, uint8_t * report, CFIndex reportLength) {
    IOHIDUserDeviceTestController *self = (__bridge IOHIDUserDeviceTestController *)refcon;
    if (self.userDeviceObserver && [self.userDeviceObserver respondsToSelector:@selector(SetReportCallback::::)]) {
        return [self.userDeviceObserver SetReportCallback: type :reportID : report : reportLength];
     }
    return kIOReturnUnsupported;
}


IOReturn UserDeviceGetReportWithReturnLengthCallbackStatic(void * _Nullable refcon, IOHIDReportType type, uint32_t reportID, uint8_t * report, CFIndex * reportLength) {
    IOHIDUserDeviceTestController *self = (__bridge IOHIDUserDeviceTestController *)refcon;
    if (self.userDeviceObserver && [self.userDeviceObserver respondsToSelector:@selector(GetReportCallback::::)]) {
        return [self.userDeviceObserver GetReportCallback: type :reportID : report : reportLength];
    }
    return kIOReturnUnsupported;
}


