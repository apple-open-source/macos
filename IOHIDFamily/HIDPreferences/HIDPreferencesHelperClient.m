//
//  HIDPreferencesHelperClient.m
//  HIDPreferencesHelper
//
//  Created by AB on 10/15/19.
//

#import "HIDPreferencesHelperClient.h"
#import "HIDPreferencesHelperListenerPrivate.h"
#import "IOHIDDebug.h"
#import <Foundation/NSXPCConnection_Private.h>
#import "HIDPreferencesPrivate.h"
#import <AssertMacros.h>
#import <CoreFoundation/CFXPCBridge.h>

@implementation HIDPreferencesHelperClient {
    xpc_connection_t _connection;
    __weak HIDPreferencesHelperListener *_listener;
}

#pragma mark -
#pragma mark init

-(nullable instancetype) initWithConnection:(xpc_connection_t)connection listener:(nonnull HIDPreferencesHelperListener *) listener {
    self = [super init];
    if (!self) return self;
    
    _connection = connection;
    _listener = listener;
    
    if (![self setupConnection]) {
        HIDLogError("HIDPreferencesHelper failed to setup connection");
        return nil;
    }
    
    return self;
}

#pragma mark -
#pragma mark setup

-(BOOL) setupConnection {
    
    __weak HIDPreferencesHelperClient *weakSelf = self;
    
    xpc_connection_set_target_queue(_connection, dispatch_get_main_queue());
    xpc_connection_set_event_handler(_connection, ^(xpc_object_t  _Nonnull object) {
        HIDLogDebug("HIDPreferencesHelper XPC connection event");
        
        __strong HIDPreferencesHelperClient *strongSelf = weakSelf;
        if (!strongSelf) return;
        
        xpc_type_t type = xpc_get_type(object);
        if (type == XPC_TYPE_ERROR) {
            HIDLogDebug("HIDPreferencesHelper Remote Connection terminated");
            [strongSelf invalidateConnection];
            return;
        }
        
        if (type == XPC_TYPE_DICTIONARY) {
            xpc_object_t reply  = [strongSelf handleMessage:object];
            if (reply) {
                xpc_connection_send_message(strongSelf->_connection, reply);
                HIDLogDebug("HIDPreferencesHelper send event reply");
            }
        }
    });
    xpc_connection_activate(_connection);
    
    return YES;
}

#pragma mark -
#pragma mark handle message
-(xpc_object_t) handleMessage:(xpc_object_t) object {
    
    __block xpc_object_t reply = nil;
    xpc_object_t replyDict = nil;
    NSDictionary *data = nil;
    NSString *user = nil;
    NSString *domain = nil;
    NSString *host = nil;
    id key = nil;
    BOOL shouldReply = NO;
    
    HIDPreferencesRequestType requestType = kHIDPreferencesRequestTypeNone;
    
    id message = (__bridge_transfer id)_CFXPCCreateCFObjectFromXPCObject(object);
    
    require_action(message, exit, HIDLogError("HIDPreferencesHelper failed to decode message"));
    
    require_action([message isKindOfClass:[NSDictionary class]], exit, HIDLogError("HIDPreferencesHelper invalid message type"));
    
    data = (NSDictionary*)message;
    HIDLogDebug("HIDPreferencesHelper Message from remote connection %@", data);
    
    requestType = ((NSNumber*)data[@(kHIDPreferencesRequestType)]).integerValue;
    domain = (NSString*)data[@(kHIDPreferencesDomain)];
    require_action(domain, exit, HIDLogError("HIDPreferencesHelper invalid domain"));
    
    
    switch (requestType) {
        case kHIDPreferencesRequestTypeSet: {
            key = data[@(kHIDPreferencesKey)];
            require_action(key, exit, HIDLogError("HIDPreferencesHelper Set invalid key"));
            user = (NSString*)data[@(kHIDPreferencesUser)];
            require_action(user, exit, HIDLogError("HIDPreferencesHelper Set invalid user"));
            host = (NSString*)data[@(kHIDPreferencesHost)];
            require_action(host, exit, HIDLogError("HIDPreferencesHelper Set invalid host"));
            //NULL Value
            if ([key isKindOfClass:[NSString class]]) {
                [self set:(NSString*)key value:nil user:user host:host domain:domain];
            } else if ([key isKindOfClass:[NSDictionary class]]) {
                [((NSDictionary*)key) enumerateKeysAndObjectsUsingBlock:^(id  _Nonnull keyStr, id  _Nonnull obj, BOOL * _Nonnull stop __unused) {
                    [self set:keyStr value:obj user:user host:host domain:domain];
                }];
            } else {
                HIDLogError("HIDPreferencesHelper Set unknown key type %@",key);
            }
            
            break;
        }
        case kHIDPreferencesRequestTypeSetMultiple: {
            user = (NSString*)data[@(kHIDPreferencesUser)];
            require_action(user, exit, HIDLogError("HIDPreferencesHelper Set Multiple invalid user"));
            host = (NSString*)data[@(kHIDPreferencesHost)];
            require_action(host, exit, HIDLogError("HIDPreferencesHelper Set Multiple invalid host"));
            if ([data objectForKey:@(kHIDPreferencesKey)]) {
                key = data[@(kHIDPreferencesKey)];
                require([key isKindOfClass:[NSDictionary class]], exit);
                [self setMultiple:key[@(kHIDPreferencesKeysToSet)] keysToRemove:key[@(kHIDPreferencesKeysToRemove)] user:user host:host domain:domain];
            } else {
                [self setMultiple:nil keysToRemove:nil user:user host:host domain:domain];
            }
            break;
        }
        case kHIDPreferencesRequestTypeSynchronize: {
            user = (NSString*)data[@(kHIDPreferencesUser)];
            require_action(user, exit, HIDLogError("HIDPreferencesHelper Synchronize invalid user"));
            host = (NSString*)data[@(kHIDPreferencesHost)];
            require_action(host, exit, HIDLogError("HIDPreferencesHelper Synchronize invalid host"));
            [self synchronize:user host:host domain:domain];
            break;
        }
        case kHIDPreferencesRequestTypeCopy: {
            user = (NSString*)data[@(kHIDPreferencesUser)];
            require_action(user, exit, HIDLogError("HIDPreferencesHelper Copy invalid user"));
            host = (NSString*)data[@(kHIDPreferencesHost)];
            require_action(host, exit, HIDLogError("HIDPreferencesHelper Copy invalid host"));
            key = data[@(kHIDPreferencesKey)];
            require_action(key, exit, HIDLogError("HIDPreferencesHelper Copy invalid key"));
            shouldReply = YES;
            [self copy:key user:user host:host domain:domain reply:^(id  _Nullable replyData) {
                if (replyData) {
                    reply = _CFXPCCreateXPCObjectFromCFObject((__bridge CFTypeRef)replyData);
                }
            }];
            break;
        }
        case kHIDPreferencesRequestTypeCopyMultiple: {
            user = (NSString*)data[@(kHIDPreferencesUser)];
            require_action(user, exit, HIDLogError("HIDPreferencesHelper Copy Multiple invalid user"));
            host = (NSString*)data[@(kHIDPreferencesHost)];
            require_action(host, exit, HIDLogError("HIDPreferencesHelper Copy Multiple invalid host"));
            shouldReply = YES;
            if ([data objectForKey:@(kHIDPreferencesKey)]) {
                key = data[@(kHIDPreferencesKey)];
                require([key isKindOfClass:[NSDictionary class]], exit);
                require([((NSDictionary*)key) objectForKey:@(kHIDPreferencesKeysToCopy)] != nil, exit);
                [self copyMultiple:key[@(kHIDPreferencesKeysToCopy)] user:user host:host domain:domain reply:^(id  _Nullable replyData) {
                    if (replyData) {
                        reply = _CFXPCCreateXPCObjectFromCFObject((__bridge CFTypeRef)replyData);
                    }
                }];
                
            } else {
                [self copyMultiple:nil user:user host:host domain:domain reply:^(id  _Nullable replyData) {
                    if (replyData) {
                        reply = _CFXPCCreateXPCObjectFromCFObject((__bridge CFTypeRef)replyData);
                    }
                }];
            }
            break;
        }
        case kHIDPreferencesRequestTypeSetDomain: {
            key = data[@(kHIDPreferencesKey)];
            require_action(key, exit, HIDLogError("HIDPreferencesHelper Set Domain invalid key"));
            if ([key isKindOfClass:[NSString class]]) {
                [self setDomain:(NSString*)key value:nil domain:domain];
            } else if ([key isKindOfClass:[NSDictionary class]]) {
                [((NSDictionary*)key) enumerateKeysAndObjectsUsingBlock:^(id  _Nonnull keyStr, id  _Nonnull obj, BOOL * _Nonnull stop __unused) {
                    [self setDomain:keyStr value:obj domain:domain];
                }];
            } else {
                HIDLogError("HIDPreferencesHelper Set Domain unknown key type %@",key);
            }
            break;
        }
        case kHIDPreferencesRequestTypeCopyDomain: {
            key = data[@(kHIDPreferencesKey)];
            require_action(key, exit, HIDLogError("HIDPreferencesHelper Copy Domain invalid key"));
            shouldReply = YES;
            [self copyDomain:key domain:domain reply:^(id  _Nullable replyData) {
                if (replyData) {
                    reply = _CFXPCCreateXPCObjectFromCFObject((__bridge CFTypeRef)replyData);
                }
            }];
            break;
        }
        default:
            HIDLogError("HIDPreferencesHelper invalid request type %lu",(unsigned long)requestType);
            break;
    }
    
    
exit:
    if (shouldReply) {
        replyDict = xpc_dictionary_create_reply(object);
        if (replyDict) {
            //OK to set  NULL value
            xpc_dictionary_set_value(replyDict, kHIDPreferencesValue, reply);
            return replyDict;
        } else {
             HIDLogError("HIDPreferencesHelper failed to create reply object");
        }
    }
        
    return nil;
}


#pragma mark -
#pragma mark invalidate

-(void) invalidateConnection {
    
    __strong HIDPreferencesHelperListener *listener = _listener;
    
    require(_connection, exit);
    require(listener, exit);
    
    xpc_connection_cancel(_connection);
    
    [listener removeClient:self];

exit:
    return;
}


#pragma mark -
#pragma mark Set

-(void) set:(NSString *)key value:(id __nullable)value user:(NSString *)user host:(NSString *)host domain:(NSString *)domain {
    
    HIDLogDebug("HIDPreferencesHelper Set Key:%@ Value:%@ User:%@ Host:%@ Domain:%@",key, user, value, host, domain);
    CFPreferencesSetValue((__bridge CFStringRef)key, (__bridge CFPropertyListRef)value, (__bridge CFStringRef)domain, (__bridge CFStringRef)user , (__bridge CFStringRef)host);
}

#pragma mark -
#pragma mark Copy

-(void) copy:(NSString *)key user:(NSString *)user host:(NSString *)host domain:(NSString *)domain reply:(HIDXPCData)reply {
    
    id value = nil;
    HIDLogDebug("HIDPreferencesHelper Copy Key:%@ User:%@ Host:%@ Domain:%@",key, user, host, domain);
    
    value = (__bridge_transfer id)CFPreferencesCopyValue((__bridge CFStringRef)key, (__bridge CFStringRef)domain, (__bridge CFStringRef)user, (__bridge CFStringRef)host);
    HIDLogDebug("HIDPreferencesHelper CFPreference value %@",value);
    
    require(reply, exit);
    
    reply(value);
exit:
    return;
}

#pragma mark -
#pragma mark Synchronize

-(void) synchronize:(NSString *)user host:(NSString *)host domain:(NSString *)domain {
    
    HIDLogDebug("HIDPreferencesHelper Synchronize User:%@ Host:%@ Domain:%@",user, host, domain);
    CFPreferencesSynchronize((__bridge CFStringRef)domain, (__bridge CFStringRef)user, (__bridge CFStringRef)host);
}

#pragma mark -
#pragma mark Copy Multiple

-(void) copyMultiple:(NSArray *)keys user:(NSString *)user host:(NSString *)host domain:(NSString *)domain reply:(HIDXPCData)reply {
    
    NSDictionary *value = nil;
    HIDLogDebug("HIDPreferencesHelper Copy Multiple Keys:%@ User:%@ Host:%@ Domain:%@",keys, user, host, domain);
    
    value = (__bridge_transfer NSDictionary*)CFPreferencesCopyMultiple((__bridge CFArrayRef)keys, (__bridge CFStringRef)domain, (__bridge CFStringRef)user, (__bridge CFStringRef)host);
    
    HIDLogDebug("HIDPreferencesHelper CFPreference value %@",value);
    require(reply, exit);
    
    reply(value);
exit:
    return;
}

#pragma mark -
#pragma mark Set Multiple

-(void) setMultiple:(NSDictionary *)keysToSet keysToRemove:(NSArray *)keysToRemove user:(NSString *)user host:(NSString *)host domain:(NSString *)domain {
    
    HIDLogDebug("HIDPreferencesHelper Set Multiple KeysToSet:%@ KeysToRemove:%@ User:%@ Host:%@ Domain:%@",keysToSet, keysToRemove, user, host, domain);
    
    CFPreferencesSetMultiple((__bridge CFDictionaryRef)keysToSet, (__bridge CFArrayRef)keysToRemove, (__bridge CFStringRef)domain, (__bridge CFStringRef)user, (__bridge CFStringRef)host);
}

#pragma mark -
#pragma mark copy domain
-(void) copyDomain:(NSString *)key domain:(NSString *)domain reply:(HIDXPCData)reply {
    
    id value = nil;
    HIDLogDebug("HIDPreferencesHelper Copy Domain Value Key:%@ Domain:%@",key, domain);
    value = (__bridge_transfer id)CFPreferencesCopyAppValue((__bridge CFStringRef)key, (__bridge CFStringRef)domain);
    
    HIDLogDebug("HIDPreferencesHelper CFPreference value %@",value);
    require(reply, exit);
    
    reply(value);
exit:
    return;
    
}

#pragma mark -
#pragma mark set domain
-(void) setDomain:(NSString *)key value:(id)value domain:(NSString *)domain {
    HIDLogDebug("HIDPreferencesHelper Set Domain Value Key:%@ Domain:%@ Value : %@",key, domain, value);
    CFPreferencesSetAppValue((__bridge CFStringRef)key, (__bridge CFTypeRef)value, (__bridge CFStringRef)domain);
}

@end
