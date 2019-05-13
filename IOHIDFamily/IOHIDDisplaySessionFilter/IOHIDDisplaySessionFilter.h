//
//  IOHIDDisplaySessionFilter.h
//  IOHIDFamily
//
//  Created by AB on 1/25/19.
//

#ifndef IOHIDDisplaySessionFilter_h
#define IOHIDDisplaySessionFilter_h


#import <Foundation/Foundation.h>

@interface HIDDisplaySessionFilter : NSObject

-(BOOL) open;
-(void) close;
-(CFDictionaryRef) getProperty;

@end


#endif /* IOHIDDisplaySessionFilter_h */
