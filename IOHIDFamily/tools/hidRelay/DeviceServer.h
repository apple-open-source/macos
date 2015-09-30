//
//  DeviceServer.h
//  HIDRelay
//
//  Created by Roberto Yepez on 8/5/14.
//  Copyright (c) 2014 Roberto Yepez. All rights reserved.
//

#ifndef HIDRelay_DeviceServer_h
#define HIDRelay_DeviceServer_h

#import <Foundation/Foundation.h>
#import "DeviceCommon.h"

@interface DeviceServer : DeviceCommon

- (id)initWithBonjour:(BOOL)bonjour;

@end


#endif
