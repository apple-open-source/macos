//
//  IOHIDTestConnectionFilter.h
//  IOHIDFamily
//
//  Created by Abhishek Nayyar on 3/30/18.
//

#ifndef IOHIDTestConnectionFilter_h
#define IOHIDTestConnectionFilter_h


#include <Foundation/Foundation.h>
#include <IOKit/hid/IOHIDEvent.h>

@interface HIDTestConnectionFilter : NSObject
-(nullable instancetype) init;
-(bool) setProperty:(NSString*) key property:(id) property;
-(id) copyProperty:(NSString*) key;
-(IOHIDEventRef) filter:(IOHIDEventRef) event;
@end

#endif /* IOHIDTestConnectionFilter_h */
