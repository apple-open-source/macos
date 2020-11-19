//
//  SOSIntervalEvent.m
//  Security_ios
//
//  Created by murf on 9/12/19.
//

#import <Foundation/Foundation.h>
#import "SOSIntervalEvent.h"
#import "keychain/SecureObjectSync/SOSInternal.h"

/*

 interval setting examples:
 NSTimeInterval earliestGB   = 60*60*24*3;  // wait at least 3 days
 NSTimeInterval latestGB     = 60*60*24*7;  // wait at most 7 days

 pattern:

SOSIntervalEvent fooEvent = [[SOSIntervalEvent alloc] initWithDefaults:account.settings dateDescription:@"foocheck" earliest:60*60*24 latest:60*60*36];

 // should we foo?
    if([fooEvent checkDate]) {
        WeDoFooToo();
        // schedule next foo
        [fooEvent followup];
    }
    // "schedule" is only used if you think there's a date upcoming you don't want altered
    // getDate will return the next schedule event date
 */

@implementation SOSIntervalEvent

- (NSDate *) getDate {
    return [_defaults valueForKey: _dateDescription];
}

- (bool) checkDate {
    NSDate *theDate = [self getDate];
    if(theDate && ([theDate timeIntervalSinceNow] <= 0)) return true;
    return false;
}

- (void) followup {
    NSDate *theDate = SOSCreateRandomDateBetweenNowPlus(_earliestDate, _latestDate);
    [_defaults setValue:theDate forKey: _dateDescription];
}

- (void)schedule {
    NSDate *theDate = [self getDate];
    if(!theDate) {
        [self followup];
    }
}

-(id)initWithDefaults:(NSUserDefaults*) defaults dateDescription:(NSString *)dateDescription earliest:(NSTimeInterval) earliest latest: (NSTimeInterval) latest {
    if ((self = [super init])) {
        _defaults = defaults;
        if(! _defaults) {
            _defaults =  [[NSUserDefaults alloc] init];
        }
        _dateDescription = dateDescription;
        _earliestDate = earliest;
        _latestDate = latest;
        [self schedule];
    }
    return self;
}

@end

