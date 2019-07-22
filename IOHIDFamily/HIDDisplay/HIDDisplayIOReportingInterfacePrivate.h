//
//  HIDDisplayIOReportingInterfacePrivate.h
//  IOHIDFamily
//
//  Created by AB on 3/28/19.
//

#ifndef HIDDisplayIOReportingInterfacePrivate_h
#define HIDDisplayIOReportingInterfacePrivate_h


#import <Foundation/Foundation.h>
#import "HIDDisplayInterfacePrivate.h"

NS_ASSUME_NONNULL_BEGIN

@interface HIDDisplayIOReportingInterface (HIDDisplayIOReportingInterfacePrivate)

-(void) handleInputData:(IOHIDValueRef) value;

@end

NS_ASSUME_NONNULL_END

#endif /* HIDDisplayIOReportingInterfacePrivate_h */
