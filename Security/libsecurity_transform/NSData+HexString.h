//
//  NSData+HexString.h
//  libsecurity_transform
//
//  Copyright (c) 2011 Apple, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface NSData (HexString)
+(id)dataWithHexString:(NSString*)hex;
@end
