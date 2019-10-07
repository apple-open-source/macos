//
//  HIDDisplayIOReportingInterface.h
//  IOHIDFamily
//
//  Created by AB on 4/22/19.
//

#ifndef HIDDisplayIOReportingInterface_h
#define HIDDisplayIOReportingInterface_h

#import <Foundation/Foundation.h>
#import <HIDDisplay/HIDDisplayInterface.h>
#import <HIDDisplay/HIDDisplayCAPI.h>

NS_ASSUME_NONNULL_BEGIN

@interface HIDDisplayIOReportingInterface : HIDDisplayInterface

-(nullable instancetype) init NS_UNAVAILABLE;

-(void) setInputDataHandler:(IOReportingInputDataHandler) handler;

-(void) setDispatchQueue:(dispatch_queue_t) queue;

-(bool) setOutputData:(NSData*) data error:(NSError**) err;

-(void) activate;

-(void) cancel;

-(void) setCancelHandler:(dispatch_block_t) handler;

@end

NS_ASSUME_NONNULL_END

#endif /* HIDDisplayIOReportingInterface_h */
