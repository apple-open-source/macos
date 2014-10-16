//
//  NSDate+TimeIntervalDescription.m
//  Security
//
//  Created by J Osborne on 7/17/13.
//
//

#import "NSDate+TimeIntervalDescription.h"
#include <math.h>
#import <Security/SecFrameworkStrings.h>
#import <Foundation/NSObjCRuntime.h>

@implementation NSDate (TimeIntervalDescription)

-(NSString*)copyDescriptionOfIntervalSince:(NSDate *)originalDate
{
	// This is really expected to be "1 day", "N days" in production, but for testing we
	// use very small intervals, and we may as well have that work out.
	
	NSTimeInterval seconds = [self timeIntervalSinceDate:originalDate];
	if (seconds <= 0) {
		return (__bridge_transfer NSString*)SecCopyCKString(SEC_CK_TID_FUTURE);
	}
	if (seconds == 0) {
		return (__bridge_transfer NSString*)SecCopyCKString(SEC_CK_TID_NOW);
	}
	if (seconds < 1) {
		return (__bridge_transfer NSString*)SecCopyCKString(SEC_CK_TID_SUBSECOND);
	}
	if (seconds < 120) {
		return [NSString stringWithFormat:@"%d %@", (int)seconds, (__bridge_transfer NSString*)SecCopyCKString(SEC_CK_TID_SECONDS)];
	}
	if (seconds < 2*60*60) {
		return [NSString stringWithFormat:@"%d %@", (int)seconds/60, (__bridge_transfer NSString*)SecCopyCKString(SEC_CK_TID_MINUTES)];
	}
	if (seconds < 60*60*24) {
		return [NSString stringWithFormat:@"%d %@", (int)seconds/(60*60), (__bridge_transfer NSString*)SecCopyCKString(SEC_CK_TID_HOURS)];
	}
    
    double days = nearbyint(seconds/(60*60*24));
    // XXX "day" vs. "days"
	return [NSString stringWithFormat:@"%.0f %@", days, (days == 1) ? (__bridge_transfer NSString*)SecCopyCKString(SEC_CK_TID_DAY) : (__bridge_transfer NSString*)SecCopyCKString(SEC_CK_TID_DAYS)];
}

@end
