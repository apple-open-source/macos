//
//  mockaksWatchDog.m
//  Security
//

#import <XCTest/XCTest.h>
#import <OCMock/OCMock.h>

#import "mockaksxcbase.h"
#import "ipc/SecdWatchdog.h"

@interface mockaksWatchDog : mockaksxcbase
@property (assign) uint64_t diskusage;
@end

@implementation mockaksWatchDog

- (bool)mockedWatchdogrusage:(rusage_info_current *)rusage
{
    memset(rusage, 0, sizeof(*rusage));
    rusage->ri_diskio_byteswritten = self.diskusage;
    rusage->ri_logical_writes = self.diskusage;
    return true;
}


- (void)testWatchDogDiskWrite {

    id mock = OCMClassMock([SecdWatchdog class]);
    OCMStub([mock watchdogrusage:[OCMArg anyPointer]]).andCall(self, @selector(mockedWatchdogrusage:));
    OCMStub([mock triggerOSFaults]).andReturn(FALSE);

    SecdWatchdog *wd = [SecdWatchdog watchdog];

    self.diskusage = 0;
    XCTAssertFalse(wd.diskUsageHigh, "diskusage high should not be true");

    self.diskusage = 2 * 1000 * 1024 * 1024; // 2GiBi
    [wd runWatchdog];

    XCTAssertTrue(wd.diskUsageHigh, "diskusage high should be true");
}

@end
