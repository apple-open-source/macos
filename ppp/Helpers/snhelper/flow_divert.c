/*
 * Copyright (c) 2013 Apple Inc.
 * All rights reserved.
 */

#include <errno.h>

#include <xpc/xpc.h>
#include <uuid/uuid.h>

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SNHelperPrivate.h>
#include <SystemConfiguration/VPNAppLayerPrivate.h>

#include "snhelper.h"
#include "flow_divert.h"

void
handle_flow_divert_uuid_add(xpc_connection_t connection, xpc_object_t message)
{
	int64_t result = 0;
	const uint8_t *uuid = xpc_dictionary_get_uuid(message, kSNHelperMessageUUID);

	if (uuid != NULL) {
		result = VPNAppLayerUUIDPolicyAdd(uuid);
	} else {
		result = EINVAL;
	}

	send_reply(connection, message, result);
}

void
handle_flow_divert_uuid_remove(xpc_connection_t connection, xpc_object_t message)
{
	int64_t result = 0;
	const uint8_t *uuid = xpc_dictionary_get_uuid(message, kSNHelperMessageUUID);

	if (uuid != NULL) {
		result = VPNAppLayerUUIDPolicyRemove(uuid);
	} else {
		result = EINVAL;
	}

	send_reply(connection, message, result);
}

void
handle_flow_divert_uuid_clear(xpc_connection_t connection, xpc_object_t message)
{
	send_reply(connection, message, VPNAppLayerUUIDPolicyClear());
}

