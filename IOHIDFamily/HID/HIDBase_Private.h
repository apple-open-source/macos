//
//  HIDBase_Private.h
//  HID
//
//  Created by dekom on 9/26/18.
//

#ifndef HIDBase_Private_h
#define HIDBase_Private_h

#import <Foundation/Foundation.h>
#import <HID/HIDBase.h>

/*!
 * @protocol HIDEventDispatcher
 *
 * @abstract
 * A protocol used for objects that have the ability to dispatch HIDEvents.
 */
@protocol HIDEventDispatcher <NSObject>
- (void)dispatchEvent:(HIDEvent *)event;
@end

#endif /* HIDBase_Private_h */
