/*
 * Copyright (c) 2013 Apple Inc.
 * All rights reserved.
 */

#import <Foundation/Foundation.h>
#if TARGET_OS_EMBEDDED
#import <MobileCoreServices/LSApplicationWorkspace.h>
#endif // TARGET_OS_EMBEDDED
#include <syslog.h>
#include <errno.h>
#include <xpc/xpc.h>
#include <uuid/uuid.h>

#include <SystemConfiguration/SNHelperPrivate.h>

#include "launch_services.h"
#include "snhelper.h"

void handle_get_uuid_for_app(xpc_connection_t connection, xpc_object_t message)
{
#if TARGET_OS_EMBEDDED
	@autoreleasepool {
		const char *name = xpc_dictionary_get_string(message, kSNHelperMessageAppID);
		if (name == NULL) {
			send_reply(connection, message, EINVAL, NULL);
			return;
		}
		
		NSString *appIdentifierString = [NSString stringWithCString:name encoding:NSASCIIStringEncoding];
		if (appIdentifierString == NULL) {
			send_reply(connection, message, EINVAL, NULL);
			return;
		}
		
		LSApplicationProxy *app = [LSApplicationProxy applicationProxyForIdentifier:appIdentifierString placeholder:NO];
		if (app == NULL) {
			send_reply(connection, message, EINVAL, NULL);
			return;
		}
		
		NSArray *uuids = [app machOUUIDs];
		if (uuids == NULL || [uuids count] == 0) {
			send_reply(connection, message, EINVAL, NULL);
			return;
		}
		
		NSUUID* uuid = [uuids objectAtIndex:0];
		if (uuid == NULL) {
			send_reply(connection, message, EINVAL, NULL);
			return;
		}
		
		uuid_t uuidBytes;
		xpc_object_t uuidObject;
		[uuid getUUIDBytes:uuidBytes];
		uuidObject = xpc_uuid_create(uuidBytes);
		send_reply(connection, message, 0, uuidObject);
		if (uuidObject != NULL) {
			xpc_release(uuidObject);
		}
	}
#else
	send_reply(connection, message, EINVAL, NULL);
#endif // TARGET_OS_EMBEDDED
}
