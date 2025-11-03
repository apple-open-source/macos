/* Copyright (c) 2012-2013 Apple Inc. All Rights Reserved. */

#ifndef _SECURITY_AUTH_SERVER_H_
#define _SECURITY_AUTH_SERVER_H_

#include "authd_private.h"
#include <xpc/xpc.h>
#include "Authorization.h"

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
OSStatus authorization_copy_right_properties(connection_t, xpc_object_t, xpc_object_t);

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
OSStatus authorization_right_remove(connection_t,xpc_object_t,xpc_object_t);
    
AUTH_NONNULL_ALL
OSStatus session_set_user_preferences(connection_t,xpc_object_t,xpc_object_t);
    
AUTH_NONNULL_ALL
OSStatus authorization_copy_prelogin_userdb(connection_t,xpc_object_t,xpc_object_t);

AUTH_NONNULL_ALL
OSStatus
authorization_copy_prelogin_pref_value(connection_t conn, xpc_object_t message, xpc_object_t reply);

AUTH_NONNULL_ALL
OSStatus
authorization_prelogin_smartcardonly_override(connection_t conn, xpc_object_t message, xpc_object_t reply);

AUTH_NONNULL2
OSStatus
server_authorize(connection_t, auth_token_t, AuthorizationFlags, auth_rights_t, auth_items_t, engine_t *);

/*!
    @function authorization_stage_plugin
    @abstract Securely stages or removes authorization plugins for safe execution
    @discussion This function handles the staging of authorization plugins by copying them to a secure location
                or removing previously staged plugins. The function validates input parameters, prevents path
                traversal attacks, and manages plugin lifecycle per process.
    @param conn The XPC connection from the requesting process
    @param message XPC message containing plugin path (for staging) and operation type
    @param reply XPC reply object to send response back to client
    @result OSStatus indicating success or specific error condition
*/
AUTH_NONNULL_ALL
OSStatus
authorization_stage_plugin(connection_t conn, xpc_object_t message, xpc_object_t reply);

#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_AUTH_SERVER_H_ */
