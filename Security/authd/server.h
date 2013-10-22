/* Copyright (c) 2012 Apple Inc. All rights reserved. */

#ifndef _SECURITY_AUTH_SERVER_H_
#define _SECURITY_AUTH_SERVER_H_

#include "authd_private.h"
#include <xpc/xpc.h>

#if defined(__cplusplus)
extern "C" {
#endif

OSStatus server_init(void);
void server_cleanup(void);
bool server_in_dark_wake(void);
authdb_t server_get_database(void);
    
AUTH_NONNULL_ALL
connection_t server_register_connection(xpc_connection_t);

AUTH_NONNULL_ALL
void server_unregister_connection(connection_t);
    
AUTH_NONNULL_ALL
void server_register_auth_token(auth_token_t);
 
AUTH_NONNULL_ALL
void server_unregister_auth_token(auth_token_t);

AUTH_NONNULL_ALL
auth_token_t server_find_copy_auth_token(AuthorizationBlob * blob);
    
AUTH_NONNULL_ALL
session_t server_find_copy_session(session_id_t,bool create);

void server_dev(void);
    
/* API */
    
AUTH_NONNULL_ALL
OSStatus authorization_create(connection_t,xpc_object_t,xpc_object_t);

AUTH_NONNULL_ALL
OSStatus authorization_create_with_audit_token(connection_t,xpc_object_t,xpc_object_t);
    
AUTH_NONNULL_ALL
OSStatus authorization_free(connection_t,xpc_object_t,xpc_object_t);
    
AUTH_NONNULL_ALL
OSStatus authorization_copy_rights(connection_t,xpc_object_t,xpc_object_t);
    
AUTH_NONNULL_ALL
OSStatus authorization_copy_info(connection_t,xpc_object_t,xpc_object_t);
    
AUTH_NONNULL_ALL
OSStatus authorization_make_external_form(connection_t,xpc_object_t,xpc_object_t);
    
AUTH_NONNULL_ALL
OSStatus authorization_create_from_external_form(connection_t,xpc_object_t,xpc_object_t);
    
AUTH_NONNULL_ALL
OSStatus authorization_right_get(connection_t,xpc_object_t,xpc_object_t);
    
AUTH_NONNULL_ALL
OSStatus authorization_right_set(connection_t,xpc_object_t,xpc_object_t);
    
AUTH_NONNULL_ALL
OSStatus authorization_enable_smartcard(connection_t,xpc_object_t,xpc_object_t);

AUTH_NONNULL_ALL
OSStatus authorization_right_remove(connection_t,xpc_object_t,xpc_object_t);
    
AUTH_NONNULL_ALL
OSStatus session_set_user_preferences(connection_t,xpc_object_t,xpc_object_t);
    
#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_AUTH_SERVER_H_ */
