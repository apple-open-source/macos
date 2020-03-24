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
        log = os_log_create(" com.apple.iohid ", "HIDDisplay");
    });
    return log;
}

// Extract NULL terminated unichar string
NSString* getUnicharStringFromData(NSData* data) {
    
    if (!data) return nil;
    
    uint8_t *bytes = (uint8_t*)[data bytes];
    NSUInteger noOfbytes = data.length;
    NSUInteger noOfUnichars = 0;
    NSString * res = nil;
    
    for (NSUInteger i=0; i < noOfbytes; i+=2) {
        uint16_t val = *((uint16_t*)(bytes+i));
        if (val == 0) break;
        else noOfUnichars++;
    }
    if (noOfUnichars > 0) {
        res = [[NSString alloc] initWithBytes:[data bytes] length:2*noOfUnichars encoding:NSUTF16LittleEndianStringEncoding];
    }
   
    return res;
}
