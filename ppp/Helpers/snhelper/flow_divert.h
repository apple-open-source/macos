/*
 * Copyright (c) 2013 Apple Inc.
 * All rights reserved.
 */

#ifndef __SNHELPER_FLOW_DIVERT_H__
#define __SNHELPER_FLOW_DIVERT_H__

void handle_flow_divert_uuid_add(xpc_connection_t connection, xpc_object_t message);
void handle_flow_divert_uuid_remove(xpc_connection_t connection, xpc_object_t message);
void handle_flow_divert_uuid_clear(xpc_connection_t connection, xpc_object_t message);

#endif /* __SNHELPER_FLOW_DIVERT_H__ */
