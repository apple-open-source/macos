//
//  NSDate+TimeIntervalDescription.h
//  Security
//
//  Created by J Osborne on 7/17/13.
//
//

#import <Foundation/Foundation.h>

@interface NSDate (TimeIntervalDescription)
 -(NSString *)copyDescriptionOfIntervalSince:(NSDate*)originalDate;
@end
