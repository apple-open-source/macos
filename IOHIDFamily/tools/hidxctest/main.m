//
//  main.m
//  hidxctest
//
//  Created by YG on 10/21/16.
//
//

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>
#include <getopt.h>

void usage (void);
NSBundle * getBundelForURL (NSString *testBundlePath);

const char mainUsage[] =
"\nUsage:\n"
"\n"
"  hidxctest [bundlepath]\n";

void usage () {
    printf ("%s", mainUsage);
}

static const char mainOptionShort[] = "t:";
static const struct option mainOptionLong[] =
{
    { "xctest",     required_argument,  NULL,   't' },
    { NULL,         0,                  NULL,    0  }
};


NSBundle * getBundelForURL (NSString *testBundlePath) {
    NSBundle *testBundle = [NSBundle bundleWithPath:testBundlePath];
    if (testBundle ) {
      return testBundle;
    }
  
    NSString *baseBundlePath =  [NSString stringWithFormat:@"%@/%@",[[NSBundle mainBundle] bundlePath],testBundlePath];
    testBundle = [NSBundle bundleWithPath: baseBundlePath];
    return testBundle;
}

int main(int argc, const char * argv[]) {
    int status =  EXIT_FAILURE;
    NSUInteger failureCount = 0;
    @autoreleasepool {
        NSMutableArray *testCaseSelection = [[NSMutableArray alloc] init] ;
        int            arg;
//        if (argc < 2) {
//            usage();
//            return status;
//        }
        while ((arg = getopt_long(argc, (char **) argv, mainOptionShort, mainOptionLong, NULL)) != -1) {
            switch (arg) {
                case 't':
                    [testCaseSelection addObject:[NSString stringWithUTF8String:optarg]];
                    break;
                default:
                    return 1;
            }
        }

        NSString *testBundlePath = [[NSString alloc] initWithUTF8String:argv[optind]];
        
        NSBundle *testBundle = getBundelForURL (testBundlePath) ;
        if (!testBundle) {
            NSLog(@"ERROR: Creating NSBundle for %@", testBundlePath);
            return status;
        }
        if  ([testBundle load] == false) {
            NSLog(@"ERROR: Loading NSBundle (%@)", testBundlePath);
            return status;
        }
 
        if (testCaseSelection.count != 0) {
            for (NSString *testCaseString in testCaseSelection) {
                XCTestSuite *testSuite = [XCTestSuite testSuiteForTestCaseWithName:testCaseString];
                if (!testSuite) {
                    NSLog(@"ERROR: Creating XCTestSuite for test case %@", testCaseString);
                    continue;
                }
                [testSuite runTest];
                
                failureCount += [testSuite.testRun failureCount];
            }
        } else {
            
            XCTestSuite *testSuite = [XCTestSuite testSuiteForBundlePath:[testBundle bundlePath]];
            if (!testSuite) {
                NSLog(@"ERROR: Creating XCTestSuite");
                return status;
            }
            [testSuite runTest];
            
            failureCount = [testSuite.testRun failureCount];
        }
        status = failureCount ? EXIT_FAILURE : EXIT_SUCCESS;
    }
    return status;
}
