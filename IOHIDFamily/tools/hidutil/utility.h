//
//  devices.h
//  IOHIDFamily
//
//  Created by YG on 4/14/16.
//
//

#ifndef devices_h
#define devices_h

#import <Foundation/Foundation.h>
#include <IOKit/hid/IOHIDEventSystemClient.h>

#define STATUS_SUCCESS 0
#define STATUS_ERROR   1

NSString* createFilterString (const char * str);
NSDictionary * createFilterDictionary (NSString *str);
id createPropertiesDicitonary (NSString *str);
NSString * createPropertiesString (const char * str);
NSDictionary * createServiceInfoDictionary (IOHIDServiceClientRef service);
NSNumber * copyServiceNumberPropertyForKey (IOHIDServiceClientRef service, NSString *key, NSNumber *def);
NSString * copyServiceStringPropertyForKey (IOHIDServiceClientRef service, NSString *key, NSString *def);

#endif /* devices_h */
