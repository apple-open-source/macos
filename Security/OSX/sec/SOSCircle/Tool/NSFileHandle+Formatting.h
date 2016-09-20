//
//  NSFileHandle+Formatting.h
//  sec
//
//

#ifndef NSFileHandle_Formatting_h
#define NSFileHandle_Formatting_h

#include <stdio.h>

#import <Foundation/Foundation.h>

@interface NSFileHandle (Formatting)

- (void) writeString: (NSString*) string;
- (void) writeFormat: (NSString*) format, ... NS_FORMAT_FUNCTION(1, 2);

@end

#endif /* NSFileHandle_Formatting_h */
