//
//  HIDPreferences.m
//  HIDPreferences
//
//  Created by AB on 10/7/19.
//

#import "HIDPreferences.h"
#import "HIDPreferencesPrivate.h"
#import <AssertMacros.h>
#import "IOHIDDebug.h"
#import <xpc/xpc.h>
#import <xpc/private.h>
#import "HIDPreferencesProtocol.h"
#import <CoreFoundation/CFXPCBridge.h>

@implementation HIDPreferences {
    dispatch_queue_t _queue;
}

-(nullable instancetype) init {
    
    self = [super init];
    if (!self) return self;
    
    _queue = dispatch_queue_create("com.apple.hidpreferences", NULL);
    
    if (!_queue) {
        HIDLogError("HIDPreferences failed to create XPC queue");
        return nil;
    }
    
    return self;
}

#pragma mark -
#pragma mark setup connection

-(xpc_connection_t __nullable) setupConnection {
    
    
    xpc_connection_t connection = nil;
    
    connection = xpc_connection_create_mach_service(kHIDPreferencesServiceName, _queue, 0);
    require_action(connection, exit, HIDLogError("HIDPreferences failed to create XPC connection"));
    
    xpc_connection_set_event_handler(connection, ^(xpc_object_t  _Nonnull object) {
        
        xpc_type_t type = xpc_get_type(object);
        
        if (type == XPC_TYPE_ERROR) {
            HIDLogDebug("HIDPreferences Connection error %@",object);
        }
    });
    
    xpc_connection_activate(connection);
    
exit:
    return connection;
}

#pragma mark -
#pragma mark destroy connection

+(void) destroyConnection:(xpc_connection_t) connection {
    
    require(connection, exit);
    
    xpc_connection_cancel(connection);
exit:
    return;
    
}

#pragma mark -
#pragma mark set

-(void) set:(NSString *)key value:(id __nullable)value user:(NSString *)user host:(NSString *)host domain:(NSString *)domain {
    
    // Should really create connection when required
    
    xpc_connection_t connection = nil;
    NSDictionary  *data = nil;
    xpc_object_t   request = nil;
    id keyToSend = nil;
    
    connection = [self setupConnection];
    require_action(connection, exit, HIDLogError("HIDPreferences invalid connection"));
    
    // CF API can have Nullable values, don't think it's ok to have it in dictionary
    if (!value) {
        keyToSend = key;
    } else {
        keyToSend = @{key : value};
    }
    
    data = [NSDictionary dictionaryWithObjectsAndKeys:@(kHIDPreferencesRequestTypeSet),@(kHIDPreferencesRequestType),
                                                        user, @(kHIDPreferencesUser),
                                                        host, @(kHIDPreferencesHost),
                                                        domain, @(kHIDPreferencesDomain),
                                                        keyToSend, @(kHIDPreferencesKey),
                                                        nil];
    
    require_action(data, exit, HIDLogError("HIDPreferences invalid data"));
    
    request = _CFXPCCreateXPCObjectFromCFObject((__bridge CFTypeRef)data);
    require_action(request, exit, HIDLogError("HIDPreferences invalid request for data %@",data));
    
    xpc_connection_send_message(connection, request);
    
exit:
    if (connection) {
        [HIDPreferences destroyConnection:connection];
    }
    return;
    
}

#pragma mark -
#pragma mark copy

-(void) copy:(NSString *)key user:(NSString *)user host:(NSString *)host domain:(NSString *)domain reply:(HIDXPCData) reply {
    
    xpc_connection_t connection = nil;
    NSDictionary  *data = nil;
    xpc_object_t   request = nil;
    xpc_object_t replyData = nil;
    xpc_type_t replyType = nil;
    id replyValue = nil;

    connection = [self setupConnection];
    require_action(connection, exit, HIDLogError("HIDPreferences invalid connection"));
    
    data = [NSDictionary dictionaryWithObjectsAndKeys:@(kHIDPreferencesRequestTypeCopy),@(kHIDPreferencesRequestType),
                                                        user, @(kHIDPreferencesUser),
                                                        host, @(kHIDPreferencesHost),
                                                        domain, @(kHIDPreferencesDomain),
                                                        key, @(kHIDPreferencesKey),
                                                        nil];
    
    require_action(data, exit, HIDLogError("HIDPreferences invalid data"));
        
    request = _CFXPCCreateXPCObjectFromCFObject((__bridge CFTypeRef)data);
    require_action(request, exit, HIDLogError("HIDPreferences invalid request for data %@",data));
    
    
    replyData = xpc_connection_send_message_with_reply_sync(connection, request);
    require_action(replyData, exit, HIDLogError("HIDPreferences invalid reply data"));
    
    replyType = xpc_get_type(replyData);
    
    if (replyType == XPC_TYPE_DICTIONARY) {
        replyValue = [HIDPreferences getReply:replyData];
        if (reply) {
            reply(replyValue);
        }
    }
    
exit:
    if (connection) {
        [HIDPreferences destroyConnection:connection];
    }
    
    return;
}

#pragma mark -
#pragma mark get reply

+(id) getReply:(xpc_object_t) object {
    xpc_object_t value =  nil;
    xpc_type_t type;
    id ret = nil;
    
    HIDLogDebug("HIDPreferences XPC Reply : %@",object);
    
    value = xpc_dictionary_get_value(object, kHIDPreferencesValue);
    // NULL CFPreference is valid value
    require(value, exit);
    
    type = xpc_get_type(value);
    
    // Bridge API doesn't work here
    if (type == XPC_TYPE_UINT64) {
        ret = @(xpc_uint64_get_value(value));
    } else {
        ret = (__bridge_transfer id)_CFXPCCreateCFObjectFromXPCObject(value);
    }
exit:
    HIDLogDebug("HIDPreferences Decoded Reply : %@",ret);
    return ret;
}

#pragma mark -
#pragma mark synchronize

-(void) synchronize:(NSString *)user host:(NSString *)host domain:(NSString *)domain {
    
    xpc_connection_t connection = nil;
    NSDictionary  *data = nil;
    xpc_object_t   request = nil;
    
    connection = [self setupConnection];
    require_action(connection, exit, HIDLogError("HIDPreferences invalid connection"));
    
    data = [NSDictionary dictionaryWithObjectsAndKeys:@(kHIDPreferencesRequestTypeSynchronize),@(kHIDPreferencesRequestType),
                                                        user, @(kHIDPreferencesUser),
                                                        host, @(kHIDPreferencesHost),
                                                        domain, @(kHIDPreferencesDomain),
                                                        nil];
    require_action(data, exit, HIDLogError("HIDPreferences invalid data"));
    
    
    request = _CFXPCCreateXPCObjectFromCFObject((__bridge CFTypeRef)data);
    require_action(request, exit, HIDLogError("HIDPreferences invalid request for data %@",data));
    
    xpc_connection_send_message(connection, request);
    
exit:
    if (connection) {
        [HIDPreferences destroyConnection:connection];
    }
    return;
}
#pragma mark -
#pragma mark copy multiple

-(void) copyMultiple:(NSArray* __nullable) keys user:(NSString*) user host:(NSString*) host domain:(NSString*) domain reply:(HIDXPCData) reply {
    
    xpc_connection_t connection = nil;
    NSDictionary  *data = nil;
    xpc_object_t   request = nil;
    xpc_object_t replyData = nil;
    xpc_type_t replyType = nil;
    id replyValue = nil;
    id keysToSend = nil;
    
    connection = [self setupConnection];
    require_action(connection, exit, HIDLogError("HIDPreferences invalid connection"));
    
    if (keys) {
        keysToSend = @{@(kHIDPreferencesKeysToCopy) : keys};
        data = [NSDictionary dictionaryWithObjectsAndKeys:@(kHIDPreferencesRequestTypeCopyMultiple),@(kHIDPreferencesRequestType),
                                                    user, @(kHIDPreferencesUser),
                                                    host, @(kHIDPreferencesHost),
                                                    domain, @(kHIDPreferencesDomain),
                                                    keysToSend, @(kHIDPreferencesKey),
                                                    nil];
    } else {
        data = [NSDictionary dictionaryWithObjectsAndKeys:@(kHIDPreferencesRequestTypeCopyMultiple),@(kHIDPreferencesRequestType),
                                                user, @(kHIDPreferencesUser),
                                                host, @(kHIDPreferencesHost),
                                                domain, @(kHIDPreferencesDomain),
                                                nil];
    }
    require_action(data, exit, HIDLogError("HIDPreferences invalid data"));
        
    request = _CFXPCCreateXPCObjectFromCFObject((__bridge CFTypeRef)data);
    require_action(request, exit, HIDLogError("HIDPreferences invalid request for data %@",data));
    
    
    replyData = xpc_connection_send_message_with_reply_sync(connection, request);
    require_action(replyData, exit, HIDLogError("HIDPreferences invalid reply data"));
   
    replyType = xpc_get_type(replyData);
    
    if (replyType == XPC_TYPE_DICTIONARY) {
        replyValue = [HIDPreferences getReply:replyData];
        if (reply) {
            reply(replyValue);
        }
    }
exit:
    if (connection) {
        [HIDPreferences destroyConnection:connection];
    }
    
    return;
}

#pragma mark -
#pragma mark set multiple

-(void) setMultiple:(NSDictionary *)keysToSet keysToRemove:(NSArray *)keysToRemove user:(NSString *)user host:(NSString *)host domain:(NSString *)domain {
    
    xpc_connection_t connection = nil;
    NSDictionary  *data = nil;
    xpc_object_t   request = nil;
    NSMutableDictionary *key = [[NSMutableDictionary alloc] init];
    
    connection = [self setupConnection];
    require_action(connection, exit, HIDLogError("HIDPreferences invalid connection"));
    
    if (keysToSet) {
        key[@(kHIDPreferencesKeysToSet)] = keysToSet;
    }
    
    if (keysToRemove) {
        key[@(kHIDPreferencesKeysToRemove)] = keysToRemove;
    }
    
    if (key.count > 0) {
        data = [NSDictionary dictionaryWithObjectsAndKeys:@(kHIDPreferencesRequestTypeSetMultiple),@(kHIDPreferencesRequestType),
                                                user, @(kHIDPreferencesUser),
                                                host, @(kHIDPreferencesHost),
                                                domain, @(kHIDPreferencesDomain),
                                                key, @(kHIDPreferencesKey),
                                                nil];
    } else {
        data = [NSDictionary dictionaryWithObjectsAndKeys:@(kHIDPreferencesRequestTypeSetMultiple),@(kHIDPreferencesRequestType),
                                            user, @(kHIDPreferencesUser),
                                            host, @(kHIDPreferencesHost),
                                            domain, @(kHIDPreferencesDomain),
                                            nil];
    }
    
    require_action(data, exit, HIDLogError("HIDPreferences invalid data"));
    
    request = _CFXPCCreateXPCObjectFromCFObject((__bridge CFTypeRef)data);
    require_action(request, exit, HIDLogError("HIDPreferences invalid request for data %@",data));
       
    xpc_connection_send_message(connection, request);
    
exit:
    if (connection) {
        [HIDPreferences destroyConnection:connection];
    }
    return;
}
#pragma mark -
#pragma mark copy domain
-(void) copyDomain:(NSString *)key domain:(NSString *)domain reply:(HIDXPCData)reply {
    xpc_connection_t connection = nil;
    NSDictionary  *data = nil;
    xpc_object_t   request = nil;
    xpc_object_t replyData = nil;
    xpc_type_t replyType = nil;
    id replyValue = nil;
    
    connection = [self setupConnection];
    require_action(connection, exit, HIDLogError("HIDPreferences invalid connection"));
    
    data = [NSDictionary dictionaryWithObjectsAndKeys:@(kHIDPreferencesRequestTypeCopyDomain),@(kHIDPreferencesRequestType),
                                                            domain, @(kHIDPreferencesDomain),
                                                            key, @(kHIDPreferencesKey),
                                                            nil];
    require_action(data, exit, HIDLogError("HIDPreferences invalid data"));
    
    request = _CFXPCCreateXPCObjectFromCFObject((__bridge CFTypeRef)data);
    require_action(request, exit, HIDLogError("HIDPreferences invalid request for data %@",data));
    
    replyData = xpc_connection_send_message_with_reply_sync(connection, request);
    require_action(replyData, exit, HIDLogError("HIDPreferences invalid reply data"));
    
    replyType = xpc_get_type(replyData);
     
    if (replyType == XPC_TYPE_DICTIONARY) {
        replyValue = [HIDPreferences getReply:replyData];
        if (reply) {
            reply(replyValue);
        }
    }
exit:
    if (connection) {
        [HIDPreferences destroyConnection:connection];
    }
    return;
}

#pragma mark -
#pragma mark set domain
-(void) setDomain:(NSString *)key value:(id)value domain:(NSString *)domain {
    xpc_connection_t connection = nil;
    NSDictionary  *data = nil;
    xpc_object_t   request = nil;
    id keyToSend = nil;
    
    connection = [self setupConnection];
    require_action(connection, exit, HIDLogError("HIDPreferences invalid connection"));
    
    if (!value) {
        keyToSend = key;
    } else {
        keyToSend = @{key : value};
    }

    data = [NSDictionary dictionaryWithObjectsAndKeys:@(kHIDPreferencesRequestTypeSetDomain),@(kHIDPreferencesRequestType),
                                                    domain, @(kHIDPreferencesDomain),
                                                    keyToSend, @(kHIDPreferencesKey),
                                                    nil];

    require_action(data, exit, HIDLogError("HIDPreferences invalid data"));

    request = _CFXPCCreateXPCObjectFromCFObject((__bridge CFTypeRef)data);
    require_action(request, exit, HIDLogError("HIDPreferences invalid request for data %@",data));

    xpc_connection_send_message(connection, request);

exit:
    if (connection) {
        [HIDPreferences destroyConnection:connection];
    }
    return;
}

@end


