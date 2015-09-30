//
//  DeviceClient.h
//  HIDRelay
//
//  Created by Roberto Yepez on 8/5/14.
//  Copyright (c) 2014 Roberto Yepez. All rights reserved.
//

#ifndef HIDRelay_DeviceClient_h
#define HIDRelay_DeviceClient_h

#import <Foundation/Foundation.h>
#import "DeviceCommon.h"

@interface DeviceClient : DeviceCommon

- (id)initWithMatchingDictionary:(NSDictionary *)matching withBonjour:(BOOL)bonjour withInterface:(NSString *)interface;

@end


#endif
