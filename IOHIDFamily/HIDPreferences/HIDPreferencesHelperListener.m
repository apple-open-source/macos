//
//  HIDPreferencesHelperListener.m
//  HIDPreferencesHelper
//
//  Created by AB on 10/16/19.
//

#import "HIDPreferencesHelperListener.h"
#import "HIDPreferencesHelperListenerPrivate.h"
#import "HIDPreferencesHelperClient.h"
#import "HIDPreferencesPrivate.h"
#import "IOHIDDebug.h"
#import "HIDPreferencesProtocol.h"
#import <AssertMacros.h>
#import <xpc/xpc.h>

@implementation HIDPreferencesHelperListener {
    xpc_connection_t _listener;
    NSMutableArray<HIDPreferencesHelperClient*> *_clients;
}

#pragma mark -
#pragma mark init

-(nullable instancetype) init {
    
    self = [super init];
    if (!self) return nil;
    
    _clients = [[NSMutableArray alloc] init];
    
    if (![self setupListener]) {
        HIDLogError("HIDPreferencesHelper failed to create listener");
        return nil;
    }
    
    return self;
}

#pragma mark -
#pragma mark accept connection

-(void) acceptConnection:(xpc_connection_t) connection {
    
    HIDPreferencesHelperClient *client = [[HIDPreferencesHelperClient alloc] initWithConnection:connection listener:self];
    require_action(client, exit, HIDLogError("HIDPreferencesHelper failed to accept connection"));
    
    [_clients addObject:client];
    
exit:
    return;
}

#pragma mark -
#pragma mark remove client

-(void) removeClient:(HIDPreferencesHelperClient*)client {
    
    require(_clients, exit);
    
    if ([_clients containsObject:client]) {
        [_clients removeObject:client];
    } else {
        HIDLogError("HIDPreferencesHelper invalid client removal requested");
    }
    
exit:
    return;
    
}

#pragma mark -
#pragma mark setup listener

-(BOOL) setupListener {
    
    _listener = xpc_connection_create_mach_service(kHIDPreferencesServiceName, dispatch_get_main_queue(), XPC_CONNECTION_MACH_SERVICE_LISTENER);
    if (!_listener) {
        return NO;
    }
    
    __weak HIDPreferencesHelperListener *weakSelf = self;
    
    xpc_connection_set_event_handler(_listener, ^(xpc_object_t  _Nonnull object) {
        
        xpc_type_t type = xpc_get_type(object);
         __strong HIDPreferencesHelperListener *strongSelf = weakSelf;
        if (!strongSelf) {
            return;
        }
        
        if (type == XPC_TYPE_CONNECTION) {
            HIDLogDebug("HIDPreferencesHelper new connection %@",object);
            [strongSelf acceptConnection:object];
        }
        
    });
    
    xpc_connection_activate(_listener);
    return YES;
}


@end
