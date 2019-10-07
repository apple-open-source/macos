//
//  OctagonTestHarness.c
//  Security
//
//  Copyright (c) 2019 Apple Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "OctagonTestHarness.h"
#import "OctagonTestHarnessXPCServiceProtocol.h"

Protocol *FrameworkFacadeProtocol = nil;

@implementation OctagonTestHarnessLoader
+ (void)load {
    FrameworkFacadeProtocol = @protocol(OctagonTestHarnessXPCServiceProtocol);
}
@end
