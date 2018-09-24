//
//  IOHIDTestConnectionFilter.m
//  IOHIDTestConnectionFilter
//
//  Created by Abhishek Nayyar on 3/30/18.
//

#include "IOHIDTestConnectionFilter.h"
#include <IOKit/hid/IOHIDEventTypes.h>
#include "AppleHIDUsageTables.h"
#include <IOKit/hid/IOHIDServiceKeys.h>
#include <os/log.h>

#define kUsages            "Usages"
#define kUsagePage         "UsagePage"


@implementation HIDTestConnectionFilter {
    
    NSMutableDictionary *_filterUsages;
    //debug
    //Usage-UsagePage (Keyboard Events Only)
    // This is just example, user can add debug info accordingly
    NSMutableArray      *_droppedEvents;
}
-(nullable instancetype) init
{
    self = [super init];
    if (!self) {
        return nil;
    }
    _filterUsages = [[NSMutableDictionary alloc] init];
    _droppedEvents = [[NSMutableArray alloc] init];
    
    return self;
}
-(bool) setProperty:(NSString*)key property:(id)property
{
    
    //Property (UsagePage-Usage)
    
    if (_filterUsages && [key isEqualToString:@(kIOHIDServiceDeviceDebugUsageFilter)]) {
        
        NSArray *usages = NULL;
        NSNumber *usagePage = NULL;
        for (NSDictionary *propertyValue in property) {
            
            if ([propertyValue objectForKey:@kUsagePage]) {
                //Usage Page Filter
                // add last usage page enteries
                if (usages && usagePage) {
                    _filterUsages[usagePage] = usages;
                }
                usagePage = propertyValue[@kUsagePage];
                
            } else if ([propertyValue objectForKey:@kUsages]) {
                //Usage Filter For Usage Page
                usages = propertyValue[@kUsages];
            } else {
                usagePage = NULL;
                usages = NULL;
            }
        }
        
        //last slot
        if (usages && usagePage) {
            _filterUsages[usagePage] = usages;
        }
        
        return [_filterUsages count] > 0 ? true : false;
    }
    return false;
}
-(id) copyProperty:(NSString*)key
{
    NSArray *result = NULL;
    
    //return set of all usage key/ usagePage array for which we need to set filter
    if (_filterUsages && [key isEqualToString:@(kIOHIDServiceDeviceDebugUsageFilter)]) {
     
        NSMutableArray *ret = [[NSMutableArray alloc] init];
        
        for (NSNumber *usagePage in _filterUsages) {
            
            NSDictionary *propertyUsagePage = @{@kUsagePage : usagePage};
            [ret addObject:propertyUsagePage];
            NSDictionary *propertyUsages = @{@kUsages : _filterUsages[usagePage]};
            [ret addObject:propertyUsages];
        }
        result = ret;
    } else if ([key isEqualToString:@(kIOHIDConnectionPluginDebugKey)]) {
        result = _droppedEvents;
    }
    return result;
}
-(IOHIDEventRef) filter:(IOHIDEventRef) event
{
    IOHIDEventRef res = NULL;
    
    //Filters keyboard events
    
    if (!event || IOHIDEventGetType(event) != kIOHIDEventTypeKeyboard) {
        //If user has enabled plugin for connection, it will not recieve events other than
        // keyboard event, change as per requirement
        // return event : if user want to filter only keyboard event and return other event as such
        return res;
    }
    
    //Filter for given keyboard usage
    
    UInt32  usage;
    UInt32  usagePage;
    
    
    usage       = (UInt32)IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsage);
    usagePage   = (UInt32)IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsagePage);
    
    //We can filer events based on usage page/usage pair.
    //Verify if usage page is valid and then if it;s valid uage in given usage page
    
    os_log_debug(OS_LOG_DEFAULT,"HIDTestConnectionFilter : Usage 0x%x Usage Page 0x%x",(unsigned int)usage, (unsigned int)usagePage);
    
    if ([_filterUsages count] == 0) {
        res = event;
    } else if ([_filterUsages objectForKey:@(usagePage)]) {
        for (NSNumber *usg in _filterUsages[@(usagePage)]) {
            if (usage == [usg intValue]) {
                res = event;
                break;
            }
        }
    }
    
    if (!res) {
        //dropped event
        //tracking only keyboard events
        [_droppedEvents addObject:@{@usagePage : @usage}];
    }
    return res;
}
@end

