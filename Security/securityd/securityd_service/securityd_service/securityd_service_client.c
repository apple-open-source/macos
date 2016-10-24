/* Copyright (c) 2013-2014 Apple Inc. All Rights Reserved. */

#include "securityd_service.h"
#include "securityd_service_client.h"
#include <xpc/xpc.h>
#include <dispatch/dispatch.h>
#include <syslog.h>
#include <AssertMacros.h>

static xpc_connection_t
_service_get_connection()
{
    static dispatch_once_t onceToken;
    static xpc_connection_t connection = NULL;

    dispatch_once(&onceToken, ^{
        connection = xpc_connection_create_mach_service(SECURITYD_SERVICE_NAME, NULL, XPC_CONNECTION_MACH_SERVICE_PRIVILEGED);
        require(connection, done);

        xpc_connection_set_event_handler(connection, ^(xpc_object_t event) {
            if (xpc_get_type(event) == XPC_TYPE_ERROR) {
                if (event == XPC_ERROR_CONNECTION_INVALID) {
                    syslog(LOG_ERR, "securityd_service not available");
                }
                // XPC_ERROR_CONNECTION_INTERRUPTED
                // XPC_ERROR_TERMINATION_IMMINENT
            } else {
                char * desc = xpc_copy_description(event);
                syslog(LOG_ERR, "securityd_service should never get messages on this connection: %s", desc);
                free(desc);
            }
        });

        xpc_connection_resume(connection);
    done:
        return;
    });

    return connection;
}

static int
_service_send_msg(service_context_t *context, xpc_object_t message, xpc_object_t * reply_out)
{
    int rc = KB_GeneralError;
    xpc_object_t reply = NULL;
    xpc_connection_t conn = NULL;

    require(message, done);
    conn = _service_get_connection();
    require(conn, done);

    if (context) {
        xpc_dictionary_set_data(message, SERVICE_XPC_CONTEXT, context, sizeof(service_context_t));
    }
    reply = xpc_connection_send_message_with_reply_sync(conn, message);
    require(reply, done);
    require(xpc_get_type(reply) != XPC_TYPE_ERROR, done);

    rc = (int)xpc_dictionary_get_int64(reply, SERVICE_XPC_RC);

    if (reply_out) {
        *reply_out = reply;
        reply = NULL;
    }

done:
    if (reply) xpc_release(reply);
    return rc;
}

int
_service_client_send_uid(service_context_t *context, uint64_t request, uid_t uid)
{
    int rc = KB_GeneralError;
    xpc_object_t message = NULL;

    message = xpc_dictionary_create(NULL, NULL, 0);
    require_quiet(message, done);

    xpc_dictionary_set_uint64(message, SERVICE_XPC_REQUEST, request);
    xpc_dictionary_set_uint64(message, SERVICE_XPC_UID, uid);

    rc = _service_send_msg(context, message, NULL);

done:
    if (message) xpc_release(message);
    return rc;
}


int
_service_client_send_secret(service_context_t *context, uint64_t request, const void * secret, int secret_len, const void * new_secret, int new_secret_len)
{
    int rc = KB_GeneralError;
    xpc_object_t message = NULL;

    message = xpc_dictionary_create(NULL, NULL, 0);
    require_quiet(message, done);

    xpc_dictionary_set_uint64(message, SERVICE_XPC_REQUEST, request);
    if (secret) {
        xpc_dictionary_set_data(message, SERVICE_XPC_SECRET, secret, secret_len);
    }

    if (new_secret) {
        xpc_dictionary_set_data(message, SERVICE_XPC_SECRET_NEW, new_secret, new_secret_len);
    }

    rc = _service_send_msg(context, message, NULL);

done:
    if (message) xpc_release(message);
    return rc;
}

int
service_client_kb_create(service_context_t *context, const void * secret, int secret_len)
{
    return _service_client_send_secret(context, SERVICE_KB_CREATE, secret, secret_len, NULL, 0);
}

int
service_client_kb_load(service_context_t *context)
{
    return _service_client_send_secret(context, SERVICE_KB_LOAD, NULL, 0, NULL, 0);
}

int
service_client_kb_load_uid(uid_t uid)
{
    return _service_client_send_uid(NULL, SERVICE_KB_LOAD_UID, uid);
}

int
service_client_kb_unload(service_context_t *context)
{
    return _service_client_send_secret(context, SERVICE_KB_UNLOAD, NULL, 0, NULL, 0);
}

int
service_client_kb_save(service_context_t *context)
{
    return _service_client_send_secret(context, SERVICE_KB_SAVE, NULL, 0, NULL, 0);
}

int
service_client_kb_unlock(service_context_t *context, const void * secret, int secret_len)
{
    return _service_client_send_secret(context, SERVICE_KB_UNLOCK, secret, secret_len, NULL, 0);
}

int
service_client_kb_lock(service_context_t *context)
{
    return _service_client_send_secret(context, SERVICE_KB_LOCK, NULL, 0, NULL, 0);
}

int
service_client_kb_change_secret(service_context_t *context, const void * secret, int secret_len, const void * new_secret, int new_secret_len)
{
    return _service_client_send_secret(context, SERVICE_KB_CHANGE_SECRET, secret, secret_len, new_secret, new_secret_len);
}

int
service_client_kb_reset(service_context_t *context, const void * secret, int secret_len)
{
    return _service_client_send_secret(context, SERVICE_KB_RESET, secret, secret_len, NULL, 0);
}

int service_client_kb_is_locked(service_context_t *context, bool *locked, bool *no_pin)
{
    int rc = KB_GeneralError;
    xpc_object_t message = NULL;
    xpc_object_t reply = NULL;

    if (locked) *locked = false;
    if (no_pin) *no_pin = false;

    message = xpc_dictionary_create(NULL, NULL, 0);
    require_quiet(message, done);

    xpc_dictionary_set_uint64(message, SERVICE_XPC_REQUEST, SERVICE_KB_IS_LOCKED);

    rc = _service_send_msg(context, message, &reply);

    if (rc == KB_Success) {
        if (locked) {
            *locked = xpc_dictionary_get_bool(reply, SERVICE_XPC_LOCKED);
        }
        if (no_pin) {
            *no_pin = xpc_dictionary_get_bool(reply, SERVICE_XPC_NO_PIN);
        }
    }

done:
    if (message) xpc_release(message);
    if (reply) xpc_release(reply);
    return rc;
}

int
service_client_stash_set_key(service_context_t *context, const void * key, int key_len)
{
    int rc = KB_GeneralError;
    xpc_object_t message = NULL;

    message = xpc_dictionary_create(NULL, NULL, 0);
    require_quiet(message, done);

    xpc_dictionary_set_uint64(message, SERVICE_XPC_REQUEST, SERVICE_STASH_SET_KEY);

    if (key)
        xpc_dictionary_set_data(message, SERVICE_XPC_KEY, key, key_len);

    rc = _service_send_msg(context, message, NULL);

done:
    if (message) xpc_release(message);
    return rc;
}

int
service_client_stash_load_key(service_context_t *context, const void * key, int key_len)
{
    int rc = KB_GeneralError;
    xpc_object_t message = NULL;

    message = xpc_dictionary_create(NULL, NULL, 0);
    require_quiet(message, done);

    xpc_dictionary_set_uint64(message, SERVICE_XPC_REQUEST, SERVICE_STASH_LOAD_KEY);

    if (key)
        xpc_dictionary_set_data(message, SERVICE_XPC_KEY, key, key_len);

    rc = _service_send_msg(context, message, NULL);

done:
    if (message) xpc_release(message);
    return rc;
}

int
service_client_stash_get_key(service_context_t *context, void ** key, int * key_len)
{
    int rc = KB_GeneralError;
    xpc_object_t message = NULL;
    xpc_object_t reply = NULL;

    require(key, done);
    require(key_len, done);

    message = xpc_dictionary_create(NULL, NULL, 0);
    require_quiet(message, done);

    xpc_dictionary_set_uint64(message, SERVICE_XPC_REQUEST, SERVICE_STASH_GET_KEY);

    rc = _service_send_msg(context, message, &reply);

    if (rc == KB_Success) {
        size_t data_len = 0;
        const void * data = xpc_dictionary_get_data(reply, SERVICE_XPC_KEY, &data_len);
        if (data) {
            *key = calloc(1u, data_len);
            memcpy(*key, data, data_len);
            *key_len = (int)data_len;
        }
    }

done:
    if (message) xpc_release(message);
    if (reply) xpc_release(reply);
    return rc;
}
