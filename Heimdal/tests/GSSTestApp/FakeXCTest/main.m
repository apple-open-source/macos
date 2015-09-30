//
//  FakeXCTest
//
//  Copyright (c) 2014 Apple. All rights reserved.
//

#import "FakeXCTest.h"
#import <getopt.h>

int main(int argc, char **argv)
{
    int result = 1;
    int ch;

    static struct option longopts[] = {
        { "limit",      required_argument,      NULL,           'l' },
        { NULL,         0,                      NULL,           0   }
    };

    optind = 0;
    while ((ch = getopt_long(argc, argv, "l:", longopts, NULL)) != -1) {
        switch (ch) {
            case 'l':
                [XCTest setLimit:optarg];
                break;
        }
    }


    @autoreleasepool {
        result = [XCTest runTests];
    }
    return result;
}

