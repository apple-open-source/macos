//
//  HIDEventServicePrivate.h
//  HID
//
//  Created by dekom on 9/28/18.
//

#ifndef HIDEventServicePrivate_h
#define HIDEventServicePrivate_h

#import "HIDEventService.h"

@interface HIDEventService (HIDFrameworkPrivate)
- (void)dispatchEvent:(HIDEvent *)event;
@end


#endif /* HIDEventServicePrivate_h */
