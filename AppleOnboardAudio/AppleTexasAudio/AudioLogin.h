//
//  AudioLogin.h
//  AudioLogin
//
//  Created by jfu on Mon Nov 12 2001.
//  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <IOKit/IOCFPlugIn.h>
#import <IOKit/IOKitLib.h>
#import <IOKit/usb/USB.h>
#import <IOKit/usb/IOUSBLib.h>

@interface AudioLogin : NSObject 
{
	mach_port_t	_masterPort;
	io_iterator_t	_iterator;
}

-(id)init;
-(void)dealloc;
-(void)willLogout;

-(BOOL)launched;
-(void)launch;
-(BOOL)shouldLaunch;

@end
