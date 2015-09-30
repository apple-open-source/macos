//
//  main.m
//  HIDRelay
//
//  Created by Roberto Yepez on 8/5/14.
//  Copyright (c) 2014 Roberto Yepez. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "DeviceServer.h"


static void printHelp()
{
    printf("\n");
    printf("hidRelayClient usage:\n\n");
    printf("\t--usage <usage>\n");
    printf("\t--usagepage <usage page>\n");
    printf("\t--transport <transport string value>\n");
    printf("\t--vid <vendor id>\n");
    printf("\t--pid <product id>\n");
    printf("\t--multicast\n");
    printf("\n");
}

int main(int argc, const char * argv[]) {
    BOOL useBonjour=YES;

    int argi;
    for (argi=1; argi<argc; argi++) {
        if ( !strcmp("--multicast", argv[argi])) {
            useBonjour = NO;
        }
        else {
            printHelp();
            return 0;
        }
    }

    @autoreleasepool {
        DeviceServer * server = [[DeviceServer alloc] initWithBonjour: useBonjour];
        
        [[NSRunLoop currentRunLoop] run];
    }
    return 0;
}
