//
//  OctagonTestHarnessXPCService.h
//  Security
//
//  Copyright (c) 2018 Apple Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "OctagonTestHarnessXPCService.h"
#import "OctagonTestHarnessXPCServiceDelegate.h"
#import "OctagonTestHarnessXPCServiceProtocol.h"

@implementation OctagonTestHarnessXPCServiceDelegate

- (BOOL)listener:(__unused NSXPCListener *)listener shouldAcceptNewConnection:(NSXPCConnection *)newConnection
{
#if OCTAGON
    newConnection.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(OctagonTestHarnessXPCServiceProtocol)];
    newConnection.exportedObject = [OctagonTestHarnessXPCService new];

    [newConnection resume];

    return YES;
#else // OCTAGON
    return NO;
#endif // OCTAGON
}

@end
