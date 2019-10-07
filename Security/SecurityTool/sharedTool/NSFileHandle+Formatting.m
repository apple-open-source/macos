//
//  NSFileHandle+Formatting.m
//  sec
//
//

#include <stdarg.h>

#import <Foundation/Foundation.h>
#import "NSFileHandle+Formatting.h"


@implementation NSFileHandle (Formatting)

- (void) writeString: (NSString*) string {
    [self writeData:[string dataUsingEncoding:NSUTF8StringEncoding]];
}

- (void) writeFormat: (NSString*) format, ... {
    va_list args;
    va_start(args, format);

    NSString* formatted = [[NSString alloc] initWithFormat:format arguments:args];

    va_end(args);

    [self writeString: formatted];
// Remove with <rdar://problem/28925164> Enable ARC wherever possible in Security.framework
#if !__has_feature(objc_arc)
    [formatted release];
#endif
}

@end
