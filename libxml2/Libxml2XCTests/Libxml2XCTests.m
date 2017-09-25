//
//  Libxml2XCTests.m
//  Libxml2XCTests
//
//  Created by David Kilzer on 2017/01/09.
//

#import <dlfcn.h>
#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

typedef int (MainFunction)(int, char**);

@interface Libxml2XCTests : XCTestCase
@property (readonly) NSBundle *bundle;
- (int)_invokeMainForTestCommand:(NSString *)command;
- (void)_printOutputFile:(NSString *)file;
@end

@implementation Libxml2XCTests

- (NSBundle *)bundle
{
    static NSBundle *bundle = nil;
    if (!bundle) {
        bundle = [NSBundle bundleForClass:[self class]];
        XCTAssertNotNil(bundle, @"bundle should not be nil for class %@", [self class]);
    }
    return bundle;
}

- (int)_invokeMainForTestCommand:(NSString *)command
{
    // Get path to binary from NSBundle.
    NSString *resourceName = [NSString stringWithFormat:@"libxml2_%@.dylib", command];
    NSString *binaryPath = [self.bundle pathForResource:resourceName ofType:nil];
    XCTAssertNotNil(binaryPath, @"binaryPath should not be nil for %@", resourceName);

    // Use dyld to load 'main' symbol from binary.
    void* binary = dlopen([binaryPath UTF8String], RTLD_LAZY|RTLD_LOCAL);
    XCTAssertTrue(!!binary, @"binary should not be NULL loading path (%s): %s", [binaryPath UTF8String], dlerror());
    MainFunction *binaryMain = (MainFunction *)dlsym(binary, "main");
    XCTAssertTrue(!!binaryMain, @"binaryMain should not be NULL: %s", dlerror());

    // Change current directory so that the binary can find its test files.
    NSString *binaryDirectory = [binaryPath stringByDeletingLastPathComponent];
    BOOL success = [[NSFileManager defaultManager] changeCurrentDirectoryPath:binaryDirectory];
    XCTAssertTrue(success, @"Could not change directory to: %@", binaryDirectory);

    // Call 'main' to run the test.
    char* argv[] = { (char*)[command UTF8String] };
    return binaryMain(1, argv);
}

- (void)_printOutputFile:(NSString *)file
{
    NSString *path = [[self.bundle resourcePath] stringByAppendingPathComponent:file];
    fprintf(stderr, "\n------- Begin %s\n", [file UTF8String]);
    NSError *error;
    fprintf(stderr, "%s", [[NSString stringWithContentsOfFile:path encoding:NSUTF8StringEncoding error:&error] UTF8String]);
    fprintf(stderr, "------- End %s\n\n", [file UTF8String]);
    if (error)
        fprintf(stderr, "\n------- Error loading %s: %s\n\n", [path UTF8String], [[error description] UTF8String]);
}

- (void)test_runtest
{
    NSString *command = @"runtest";
    int result = [self _invokeMainForTestCommand:command];
    XCTAssertTrue(result == 0, @"%@`main() returned no errors", command);
}

- (void)test_runxmlconf
{
    NSString *command = @"runxmlconf";
    int result = [self _invokeMainForTestCommand:command];
    XCTAssertTrue(result == 0, @"%@`main() returned no errors", command);
    [self _printOutputFile:@"runxmlconf.log"];
}

- (void)test_testapi
{
    NSString *command = @"testapi";
    int result = [self _invokeMainForTestCommand:command];
    XCTAssertTrue(result == 0, @"%@`main() returned no errors", command);
    [self _printOutputFile:@"test.out"];
}

- (void)test_testchar
{
    NSString *command = @"testchar";
    int result = [self _invokeMainForTestCommand:command];
    XCTAssertTrue(result == 0, @"%@`main() returned no errors", command);
}

- (void)test_testdict
{
    NSString *command = @"testdict";
    int result = [self _invokeMainForTestCommand:command];
    XCTAssertTrue(result == 0, @"%@`main() returned no errors", command);
}

- (void)test_testrecurse
{
    NSString *command = @"testrecurse";
    int result = [self _invokeMainForTestCommand:command];
    XCTAssertTrue(result == 0, @"%@`main() returned no errors", command);
}

@end
