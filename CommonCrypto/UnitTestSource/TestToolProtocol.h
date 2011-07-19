//
//  TestToolProtocol.h
//  CommonCrypto
//
//  Created by James Murphy on 10/28/10.
//  Copyright 2010 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>


@protocol TestToolProtocol <NSObject>

- (void)doAssertTest:(BOOL)result errorString:(NSString *)errorStr;

@end
