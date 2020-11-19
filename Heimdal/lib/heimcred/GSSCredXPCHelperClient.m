/*-
 * Copyright (c) 2013 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#import "GSSCredXPCHelperClient.h"
#import <xpc/private.h>
#import <Foundation/NSXPCConnection_Private.h>
#import <CoreFoundation/CFXPCBridge.h>
#import "gsscred.h"
#import "common.h"

@interface GSSCredXPCHelperClient ()

@end

@implementation GSSCredXPCHelperClient

+ (NSXPCConnection *)createXPCConnection:(uid_t)session
{
    NSXPCConnection *xpcConnection;
    xpcConnection = [[NSXPCConnection alloc] initWithMachServiceName:@"com.apple.GSSCred" options:NSXPCConnectionPrivileged];
    [xpcConnection setInterruptionHandler:^{
	os_log_debug(GSSOSLog(), "connection interrupted: %u", session);
    }];
    
    [xpcConnection setInvalidationHandler:^{
	os_log_debug(GSSOSLog(), "connection invalidated: %u", session);
    }];
    
    uuid_t uuid;
    uuid_parse("D58511E6-6A96-41F0-B5CB-885DF4E3A531", uuid);  //make this value static to avoid duplicate launches of GSSCred
    if (session != 0) {
	memcpy(&uuid, &session, sizeof(session));
	xpc_connection_set_oneshot_instance(xpcConnection._xpcConnection, uuid);
    }
    
    [xpcConnection resume];
    return xpcConnection;

}

+ (void)sendWakeup:(NSXPCConnection *)connection
{
    xpc_object_t request = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_string(request, "command", "wakeup");
    xpc_connection_send_message(connection._xpcConnection, request);
}

+ (krb5_error_code)acquireForCred:(HeimCredRef)cred expireTime:(time_t *)expire
{
    os_log_debug(GSSOSLog(), "gsscred_cache_acquire: %@", CFBridgingRelease(CFUUIDCreateString(NULL, cred->uuid)));
    
    NSXPCConnection *xpcConnection = [self createXPCConnection:cred->session];
    [self sendWakeup:xpcConnection];
    
    CFDictionaryRef attributes = cred->attributes;
    xpc_object_t xpcattrs = _CFXPCCreateXPCObjectFromCFObject(attributes);
    if (xpcattrs == NULL)
	return KRB5_FCC_INTERNAL;

    xpc_object_t request = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_string(request, "command", "acquire");
    xpc_dictionary_set_value(request, "attributes", xpcattrs);
     
    xpc_object_t reply = xpc_connection_send_message_with_reply_sync(xpcConnection._xpcConnection, request);
    if (reply == NULL) {
	os_log_error(GSSOSLog(), "server did not return any data");
    }

    if (xpc_get_type(reply) == XPC_TYPE_ERROR) {
	os_log_error(GSSOSLog(), "server returned an error: %@", reply);
    }

    if (xpc_get_type(reply) == XPC_TYPE_DICTIONARY) {
	NSDictionary *replyDictionary = CFBridgingRelease(_CFXPCCreateCFObjectFromXPCObject(reply));
	
	NSDictionary *resultDictionary = replyDictionary[@"result"];
	NSNumber *status = resultDictionary[@"status"];
	NSNumber *expireTime = resultDictionary[@"expire"];
	*expire = [expireTime longValue];

	return [status intValue];
	
    }

    return 1;
}

+ (krb5_error_code)refreshForCred:(HeimCredRef)cred expireTime:(time_t *)expire
{
    os_log_debug(GSSOSLog(), "gsscred_cache_refresh: %@", CFBridgingRelease(CFUUIDCreateString(NULL, cred->uuid)));
    
    NSXPCConnection *xpcConnection = [self createXPCConnection:cred->session];
    [self sendWakeup:xpcConnection];
    
    CFDictionaryRef attributes = cred->attributes;
    xpc_object_t xpcattrs = _CFXPCCreateXPCObjectFromCFObject(attributes);
    if (xpcattrs == NULL)
	return KRB5_FCC_INTERNAL;
    
    xpc_object_t request = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_string(request, "command", "refresh");
    xpc_dictionary_set_value(request, "attributes", xpcattrs);
    
    xpc_object_t reply = xpc_connection_send_message_with_reply_sync(xpcConnection._xpcConnection, request);
    if (reply == NULL) {
	os_log_error(GSSOSLog(), "server returned an error during wakeup: %@", reply);
    }

    if (xpc_get_type(reply) == XPC_TYPE_ERROR) {
	os_log_error(GSSOSLog(), "server returned an error: %@", reply);
    }

    if (xpc_get_type(reply) == XPC_TYPE_DICTIONARY) {
	NSDictionary *replyDictionary = CFBridgingRelease(_CFXPCCreateCFObjectFromXPCObject(reply));
	
	NSNumber *status = replyDictionary[@"status"];
	NSNumber *expireTime = replyDictionary[@"expire"];
	*expire = [expireTime longValue];
	
	return [status intValue];
	
    }
    
    return 1;
    
}

@end
