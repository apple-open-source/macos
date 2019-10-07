//
//  log_IOHIDFamily.m
//  log_IOHIDFamily
//
//  Created by AB on 11/12/18.
//

#import <Foundation/Foundation.h>
#import <os/log_private.h>
#import <IOKit/hid/IOHIDEvent.h>
#import <IOKit/hid/IOHIDUsageTables.h>
#import <HID/HID.h>
#import <HID/HIDEvent+HIDEventFields.h>

NSString *getEventDescription(HIDEvent* event);

NSString *getEventDescription(HIDEvent* event)
{
    NSString                     *eventDescription = @"Unknown";
    uint32_t                     flags = 0;
    __block NSMutableString      *details = [NSMutableString new];
    NSMutableString              *childrenDetails = [[NSMutableString alloc] init];
    NSArray                      *children = event.children;
    
    if (!event) {
        return eventDescription;
    }
    
    flags = IOHIDEventGetEventFlags((__bridge IOHIDEventRef)event);
    
    [event enumerateFieldsWithBlock:^(HIDEventFieldInfo * eventField){
        
        if (eventField->fieldType == kEventFieldDataType_Integer) {
            [details appendFormat:@"%s:%lu ", eventField->name, [event integerValueForField:eventField->field]];
        } else if (eventField->fieldType == kEventFieldDataType_Double) {
            [details appendFormat:@"%s:%f ", eventField->name, [event doubleValueForField:eventField->field]];
        }
    }];
    
    if (children) {

        for (NSUInteger i = 0; i < children.count; i++) {
            
            [childrenDetails appendString:getEventDescription([children objectAtIndex:i])];
        }
    }
    eventDescription = [NSString stringWithFormat:@"SenderID:%llx Timestamp:%llu Type:%u TypeStr:%s Flags:%u %@ Children:{ %@ }, ", event.senderID, event.timestamp, event.type, (char*)IOHIDEventGetTypeString(event.type), flags, details, childrenDetails];
    
    return eventDescription;
}

NSAttributedString *
OSLogCopyFormattedString(const char *type , id value ,
                         __unused os_log_type_info_t info) {
    
    
    NSAttributedString *decoded = nil;
    NSData *data = (NSData*)value;
    
    if (strcmp(type,"event") == 0) {
        
        HIDEvent *event = [[HIDEvent alloc] initWithData:data];
        decoded = [[NSMutableAttributedString alloc] initWithString:getEventDescription(event)];
    }
    
    return decoded;
    
}
