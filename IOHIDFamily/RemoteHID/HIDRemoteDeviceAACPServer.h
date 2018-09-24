//
//  HIDAccesoryServer.h
//  HIDAccesoryServer
//
//  Created by yg on 12/19/17.
//  Copyright © 2017 apple. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "HIDRemoteDeviceServer.h"

@interface HIDRemoteDeviceAACPServer : HIDRemoteDeviceServer

-(nullable instancetype) initWithQueue:(dispatch_queue_t __nonnull) queue;
-(void) activate;
-(void) cancel;

@end
