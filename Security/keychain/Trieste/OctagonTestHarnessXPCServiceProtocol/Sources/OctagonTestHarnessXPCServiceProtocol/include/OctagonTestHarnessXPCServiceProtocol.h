// Copyright (C) 2018 Apple Inc. All Rights Reserved.

#import <Foundation/Foundation.h>

@protocol OctagonTestHarnessXPCServiceProtocol<NSObject>

// Trieste-compliant Octagon reset
- (void)octagonReset:(NSString *)altDSID complete:(void (^)(NSNumber *reset, NSError *error))complete;

@end
