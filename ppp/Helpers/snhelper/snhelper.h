/*
 * Copyright (c) 2013 Apple Inc.
 * All rights reserved.
 */

#ifndef __SNHELPER_H__
#define __SNHELPER_H__

void send_reply(xpc_connection_t connection, xpc_object_t request, int64_t result);

#endif /* __SNHELPER_H__ */
