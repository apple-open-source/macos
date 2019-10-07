//
//  main.m
//  Security
//
//  Copyright (c) 2018 Apple Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <Foundation/NSXPCConnection.h>
#import <Security/Security.h>
#import <Security/SecInternalReleasePriv.h>

#import "OctagonTestHarnessXPCServiceDelegate.h"

static OctagonTestHarnessXPCServiceDelegate *delegate = nil;
static NSXPCListener *listener =nil;

int main(int argc, const char *argv[]) {

    @autoreleasepool {
        if (!SecIsInternalRelease()) {
            NSLog(@"not internal device");
            return 1;
        }

        delegate = [[OctagonTestHarnessXPCServiceDelegate alloc] init];
        listener = [[NSXPCListener alloc] initWithMachServiceName:@"com.apple.trieste.OctagonTestHarnessXPCService"];

        listener.delegate = delegate;

        NSLog(@"Done listener initialization, resuming");

        [listener resume];
    }
    [[NSRunLoop mainRunLoop] run];

    return 0;
}
