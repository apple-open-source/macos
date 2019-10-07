//
//  HIDDisplayPrivate.m
//  HIDDisplay
//
//  Created by AB on 4/22/19.
//

#import "HIDDisplayPrivate.h"

os_log_t HIDDisplayLog (void)
{
    static os_log_t log = NULL;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        log = os_log_create("com.apple.HIDDisplay", "default");
    });
    return log;
}
