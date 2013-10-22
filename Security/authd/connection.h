/* Copyright (c) 2012 Apple Inc. All rights reserved. */

#ifndef _SECURITY_AUTH_CONNECTION_H_
#define _SECURITY_AUTH_CONNECTION_H_

#if defined(__cplusplus)
extern "C" {
#endif

AUTH_WARN_RESULT AUTH_MALLOC AUTH_NONNULL_ALL AUTH_RETURNS_RETAINED
connection_t connection_create(process_t);

AUTH_NONNULL_ALL
pid_t connection_get_pid(connection_t);
    
AUTH_NONNULL_ALL
process_t connection_get_process(connection_t);
    
AUTH_NONNULL_ALL
dispatch_queue_t connection_get_dispatch_queue(connection_t);

AUTH_NONNULL1
void connection_set_engine(connection_t, engine_t);

AUTH_NONNULL_ALL
void connection_destory_agents(connection_t);

AUTH_NONNULL_ALL
bool connection_get_syslog_warn(connection_t);

AUTH_NONNULL_ALL
void connection_set_syslog_warn(connection_t);
    
#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_AUTH_CONNECTION_H_ */
