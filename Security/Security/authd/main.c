/* Copyright (c) 2012-2013 Apple Inc. All Rights Reserved. */

#include "debugging.h"
#include "server.h"
#include "process.h"
#include "session.h"
#include "authtoken.h"
#include "engine.h"
#include "authd_private.h"
#include "connection.h"

#include <Security/Authorization.h>

#include <xpc/xpc.h>
#include <xpc/private.h>
#include <dispatch/dispatch.h>
#include <assert.h>
#include <sandbox.h>

#if DEBUG
#include <malloc/malloc.h>
#endif

static void
security_auth_peer_event_handler(xpc_connection_t connection, xpc_object_t event)
{
    __block OSStatus status = errAuthorizationDenied;
    
    connection_t conn = (connection_t)xpc_connection_get_context(connection);
    require_action(conn != NULL, done, LOGE("xpc[%i]: process context not found", xpc_connection_get_pid(connection)));

    CFRetainSafe(conn);

    xpc_type_t type = xpc_get_type(event);

	if (type == XPC_TYPE_ERROR) {
		if (event == XPC_ERROR_CONNECTION_INVALID) {
			// The client process on the other end of the connection has either
			// crashed or cancelled the connection. After receiving this error,
			// the connection is in an invalid state, and you do not need to
			// call xpc_connection_cancel(). Just tear down any associated state
			// here.
            LOGV("xpc[%i]: client disconnected", xpc_connection_get_pid(connection));
            connection_destory_agents(conn);
		} else if (event == XPC_ERROR_TERMINATION_IMMINENT) {
			// Handle per-connection termination cleanup.
            LOGD("xpc[%i]: per-connection termination", xpc_connection_get_pid(connection));
		}
	} else {
		assert(type == XPC_TYPE_DICTIONARY);
        
        xpc_object_t reply = xpc_dictionary_create_reply(event);
        require(reply != NULL, done);
        
        uint64_t auth_type = xpc_dictionary_get_uint64(event, AUTH_XPC_TYPE);
        LOGV("xpc[%i]: received message type=%llu", connection_get_pid(conn), auth_type);
        
        switch (auth_type) {
            case AUTHORIZATION_CREATE:
                status = authorization_create(conn,event,reply);
                break;
            case AUTHORIZATION_CREATE_WITH_AUDIT_TOKEN:
                status = authorization_create_with_audit_token(conn,event,reply);
                break;
            case AUTHORIZATION_FREE:
                status = authorization_free(conn,event,reply);
                break;
            case AUTHORIZATION_COPY_RIGHTS:
                status = authorization_copy_rights(conn,event,reply);
                break;
            case AUTHORIZATION_COPY_INFO:
                status = authorization_copy_info(conn,event,reply);
                break;
            case AUTHORIZATION_MAKE_EXTERNAL_FORM:
                status = authorization_make_external_form(conn,event,reply);
                break;
            case AUTHORIZATION_CREATE_FROM_EXTERNAL_FORM:
                status = authorization_create_from_external_form(conn,event,reply);
                break;
            case AUTHORIZATION_RIGHT_GET:
                status = authorization_right_get(conn,event,reply);
                break;
            case AUTHORIZATION_RIGHT_SET:
                status = authorization_right_set(conn,event,reply);
                break;
            case AUTHORIZATION_RIGHT_REMOVE:
                status = authorization_right_remove(conn,event,reply);
                break;
            case SESSION_SET_USER_PREFERENCES:
                status = session_set_user_preferences(conn,event,reply);
                break;
            case AUTHORIZATION_DISMISS:
                connection_destory_agents(conn);
                status = errAuthorizationSuccess;
                break;
            case AUTHORIZATION_ENABLE_SMARTCARD:
                status = authorization_enable_smartcard(conn,event,reply);
                break;
            case AUTHORIZATION_SETUP:
                {
                    mach_port_t bootstrap = xpc_dictionary_copy_mach_send(event, AUTH_XPC_BOOTSTRAP);
                    if (!process_set_bootstrap(connection_get_process(conn), bootstrap)) {
                        if (bootstrap != MACH_PORT_NULL) {
                            mach_port_deallocate(mach_task_self(), bootstrap);
                        }
                    }
                }
                status = errAuthorizationSuccess;
                break;
#if DEBUG
            case AUTHORIZATION_DEV:
                server_dev();
                break;
#endif
            default:
                break;
        }

        xpc_dictionary_set_int64(reply, AUTH_XPC_STATUS, status);
        xpc_connection_send_message(connection, reply);
        xpc_release(reply);
	}

done:
    CFReleaseSafe(conn);
}

static void
connection_finalizer(void * conn)
{
    LOGD("xpc[%i]: connection_finalizer", connection_get_pid(conn));
    server_unregister_connection(conn);

//#if DEBUG
//    malloc_printf("-=-=-=- connection_finalizer() -=-=-=-\n");
//    malloc_zone_print(malloc_default_zone(), false);
//#endif
}

static void
security_auth_event_handler(xpc_connection_t xpc_conn)
{
    connection_t conn = server_register_connection(xpc_conn);
    
    if (conn) {
        xpc_connection_set_context(xpc_conn, conn);
        xpc_connection_set_finalizer_f(xpc_conn, connection_finalizer);
        
        xpc_connection_set_event_handler(xpc_conn, ^(xpc_object_t event) {
            xpc_retain(xpc_conn);
            xpc_retain(event);
            dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
                security_auth_peer_event_handler(xpc_conn, event);
                xpc_release(event);
                xpc_release(xpc_conn);
            });
        });
        xpc_connection_resume(xpc_conn);

    } else {
        LOGE("xpc[%i]: failed to register connection", xpc_connection_get_pid(xpc_conn));
        xpc_connection_cancel(xpc_conn);
    }
}

static void sandbox()
{
    char * errorbuf;
    int32_t rc;
    
    rc = sandbox_init(SECURITY_AUTH_NAME, SANDBOX_NAMED, &errorbuf);
    
    if (rc) {
        LOGE("server: sandbox_init failed %s (%i)", errorbuf, rc);
        sandbox_free_error(errorbuf);
#ifndef DEBUG
        abort();
#endif
    }
}

int main(int argc AUTH_UNUSED, const char *argv[] AUTH_UNUSED)
{
//#if DEBUG
//    malloc_printf("-=-=-=- main() -=-=-=-\n");
//    malloc_zone_print(malloc_default_zone(), false);
//#endif

    LOGV("starting");
    
    sandbox();

    if (server_init() != errAuthorizationSuccess) {
        LOGE("auth: server_init() failed");
        return errAuthorizationInternal;
    }
        
//#if DEBUG
//    malloc_printf("-=-=-=- server_init() -=-=-=-\n");
//    malloc_zone_print(malloc_default_zone(), false);
//#endif

    xpc_main(security_auth_event_handler);
    
    server_cleanup();
    
	return 0;
}
