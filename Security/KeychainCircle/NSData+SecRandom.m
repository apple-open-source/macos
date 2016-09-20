//
//  NSData+SecRandom.m
//  Security
//
//  Created by Mitch Adler on 4/15/16.
//
//

#import <Foundation/Foundation.h>
#import <NSData+SecRandom.h>

#include <Security/SecRandom.h>

@implementation NSMutableData (SecRandom)

+ (instancetype) dataWithRandomBytes: (int) length {

    NSMutableData* result = [NSMutableData dataWithLength: length];

    if (0 != SecRandomCopyBytes(kSecRandomDefault, result.length, result.mutableBytes))
        return nil;

    return result;
}

@end
