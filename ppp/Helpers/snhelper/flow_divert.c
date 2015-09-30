/*
 * Copyright (c) 2013, 2015 Apple Inc.
 * All rights reserved.
 */

#include <errno.h>
#include <xpc/xpc.h>

#include "snhelper.h"
#include "flow_divert.h"

void
handle_flow_divert_uuid_add(xpc_connection_t connection, xpc_object_t message)
{
	send_reply(connection, message, EINVAL, NULL);
}

void
handle_flow_divert_uuid_remove(xpc_connection_t connection, xpc_object_t message)
{
	send_reply(connection, message, EINVAL, NULL);
}

void
handle_flow_divert_uuid_clear(xpc_connection_t connection, xpc_object_t message)
{
	send_reply(connection, message, EINVAL, NULL);
}

