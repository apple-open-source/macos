//
//  HIDDisplayPrivate.h
//  IOHIDFamily
//
//  Created by AB on 1/16/19.
//

#ifndef HIDDisplayPrivate_h
#define HIDDisplayPrivate_h

#import <os/log.h>
#import <Foundation/Foundation.h>

os_log_t HIDDisplayLog (void);
NSString* getUnicharStringFromData(NSData* data);

#endif /* HIDDisplayPrivate_h */
