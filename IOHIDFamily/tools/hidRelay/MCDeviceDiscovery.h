//
//  MCDeviceDiscovery.h
//  HIDRelay
//
//  Created by Roberto Yepez on 8/5/14.
//  Copyright (c) 2014 Roberto Yepez. All rights reserved.
//
#import <Foundation/Foundation.h>

// TODO --- in case of didComplete==NO, be way more expresive as to what actually failed
typedef void(^MCDeviceHandshakeCompletion)(BOOL didComplete, NSString *localInterfaceName, NSString *remoteIP);

@interface MCDeviceDiscovery : NSObject

- (id)initWithInterface:(NSString *)interface;

// Returns the name of the local and active ethernet interface (ex. "en1")
// Returns NULL if no active ethernet interface is found
+ (NSString *)localEthernetInterfaceName;

// Returns the IPv4 address (X.X.X.X) for a given interface name.
// Returns NULL if interfaceName doesn't exists or doesn't have an IP
+ (NSString *)ipv4AddressForInterface:(NSString *)interfaceName;

// Executes the multicast handshake algorithm
- (void)performHandshakeWithCompletionBlock:(MCDeviceHandshakeCompletion)completion;

@end
