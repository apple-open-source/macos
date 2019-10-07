//
//  IOHIDTestConnectionFilter.h
//  IOHIDFamily
//
//  Created by Abhishek Nayyar on 3/30/18.
//

#include <Foundation/Foundation.h>
#include <IOKit/hid/IOHIDEvent.h>

NS_ASSUME_NONNULL_BEGIN

@interface HIDTestConnectionFilter : NSObject
-(nullable instancetype) init;
-(bool) setProperty:(NSString*) key property:(id) property;
-(id) copyProperty:(NSString*) key;
-(IOHIDEventRef) filter:(IOHIDEventRef) event;
@end

NS_ASSUME_NONNULL_END

