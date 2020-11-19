#import "SecDbBackupTestsBase.h"

static int checkV12DevEnabledOn(void) {
    return 1;
}

static int checkV12DevEnabledOff(void) {
	return 0;
}

NSString* _uuidstring;

@implementation SecDbBackupTestsBase {
	NSString* _testHomeDirectory;
}

+ (void)setV12Development:(BOOL)newState {
	if (newState) {
		checkV12DevEnabled = checkV12DevEnabledOn;
	} else {
		checkV12DevEnabled = checkV12DevEnabledOff;
	}
}

+ (void)setUp {
    [super setUp];
    checkV12DevEnabled = checkV12DevEnabledOn;
    SecCKKSDisable();
#if OCTAGON
    SecCKKSTestSetDisableSOS(true);
#endif
	_uuidstring = [[NSUUID UUID] UUIDString];
}

- (void)setUp {
    NSString* testName = [self.name componentsSeparatedByString:@" "][1];
    testName = [testName stringByReplacingOccurrencesOfString:@"]" withString:@""];
    secnotice("secdbbackuptest", "Beginning test %@", testName);

    // Make a new fake keychain
    NSError* error;
    _testHomeDirectory = [NSTemporaryDirectory() stringByAppendingPathComponent:[NSString stringWithFormat:@"%@/%@/", _uuidstring, testName]];

    NSLog(@"%@", _testHomeDirectory);

    [[NSFileManager defaultManager] createDirectoryAtPath:_testHomeDirectory
                              withIntermediateDirectories:YES
                                               attributes:nil
                                                    error:&error];

	XCTAssertNil(error, "Could not make directory at %@", _testHomeDirectory);

    SetCustomHomeURLString((__bridge CFStringRef)_testHomeDirectory);
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        securityd_init(NULL);
    });
    SecKeychainDbReset(NULL);

    // Actually load the database.
    kc_with_dbt(true, NULL, ^bool (SecDbConnectionRef dbt) { return false; });
}

- (void)tearDown {
    // Put teardown code here. This method is called after the invocation of each test method in the class.
}

+ (void)tearDown {
    SetCustomHomeURL(NULL);
    SecKeychainDbReset(NULL);
    resetCheckV12DevEnabled();
}

@end
