//
//  HIDAnalyticsEventField.m
//  HIDAnalytics
//
//  Created by AB on 12/4/18.
//

#import "HIDAnalyticsEventField.h"

@implementation HIDAnalyticsEventField
{
    uint64_t integerValue;
}
-(nullable instancetype) initWithName:(NSString*) name
{
    self = [super init];
    if (!self) {
        return nil;
    }
    
    _fieldName = name;
    return self;
}

//returns processed value
-(id __nullable) value
{
    return @(integerValue);
}

-(void) setValue:(id __unused) value
{
    // since we support integer only for now
    // shouldn't override any value since this is one time thing
    // any call to explicity override this should be through set<ValueType>Value
    // eg setIntegerValue
    
    // for future we can support type option in normal field to know if
    // it's int or float etc
}

-(void) setIntegerValue:(uint64_t) value
{
    integerValue = value;
}
@end
