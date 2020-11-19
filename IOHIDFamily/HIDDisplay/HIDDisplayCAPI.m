//
//  HIDDisplayCAPI.m
//  HIDDisplay
//
//  Created by abhishek on 4/21/20.
//

#import "HIDDisplayCAPI.h"
#import "HIDDisplayInterface.h"
#import "HIDDisplayPrivate.h"


CFStringRef __nullable HIDDisplayGetContainerID(CFTypeRef hidDisplayInterface)
{
    id device = (__bridge id)hidDisplayInterface;
    
    HIDDisplayInterface *_device = nil;
    
    if (![device isKindOfClass:[HIDDisplayInterface class]]) {
        os_log_error(HIDDisplayLog(),"Invalid HIDDisplayInterfaceRef");
        return NULL;
    }
    
    _device = (HIDDisplayInterface*)device;
    
    return (__bridge CFStringRef)_device.containerID;
}
