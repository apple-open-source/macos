/* Copyright (c) 2012-2013 Apple Inc. All Rights Reserved. */

#include "Authorization.h"
#include "authd_private.h"
#include "authutilities.h"
#include "debugging.h"

#include <Security/AuthorizationPriv.h>
#include <Security/AuthorizationDB.h>
#include <Security/AuthorizationTags.h>
#include <Security/AuthorizationTagsPriv.h>
#include <utilities/SecDispatchRelease.h>
#include <utilities/SecCFRelease.h>
#include <xpc/xpc.h>
#include <xpc/private.h>
#include <mach/mach.h>
#include <AssertMacros.h>
#include <CoreFoundation/CFXPCBridge.h>
#include <CoreGraphics/CGWindow.h>
#include <CoreFoundation/CFBundlePriv.h>
#include <dlfcn.h>
#include <os/log.h>

static os_log_t AUTH_LOG_DEFAULT(void) {
    static dispatch_once_t once;
    static os_log_t log;
    dispatch_once(&once, ^{ log = os_log_create("com.apple.Authorization", "framework"); });
    return log;
};

#define AUTH_LOG AUTH_LOG_DEFAULT()

static dispatch_queue_t
get_authorization_dispatch_queue(void)
{
    static dispatch_once_t onceToken = 0;
    static dispatch_queue_t connection_queue = NULL;
 
    dispatch_once(&onceToken, ^{
        connection_queue = dispatch_queue_create("authorization-connection-queue", DISPATCH_QUEUE_SERIAL);
    });
    
    return connection_queue;
}

static xpc_connection_t
get_authorization_connection(void)
{
    static xpc_connection_t connection = NULL;
    
    dispatch_sync(get_authorization_dispatch_queue(), ^{
        if (connection == NULL) {
            connection = xpc_connection_create(SECURITY_AUTH_NAME, NULL);

            if (!connection) {
                os_log_error(AUTH_LOG, "Failed to create xpc connection to %s", SECURITY_AUTH_NAME);
                connection = xpc_connection_create(SECURITY_AUTH_NAME, NULL);
            }
            
            if (connection == NULL) {
                os_log_error(AUTH_LOG, "Still failed to create xpc connection to %s", SECURITY_AUTH_NAME);
                return;
            }
            xpc_connection_set_event_handler(connection, ^(xpc_object_t event) {
                if (xpc_get_type(event) == XPC_TYPE_ERROR) {
                    if (event == XPC_ERROR_CONNECTION_INVALID) {
                        os_log_error(AUTH_LOG, "Server not available");
                    }
                    // XPC_ERROR_CONNECTION_INTERRUPTED
                    // XPC_ERROR_TERMINATION_IMMINENT
                } else {
                    char * desc = xpc_copy_description(event);
                    os_log_error(AUTH_LOG, "We should never get messages on this connection: %s", desc);
                    free(desc);
                }
            });
            
            xpc_connection_resume(connection);
            
            // Send
            xpc_object_t message = xpc_dictionary_create(NULL, NULL, 0);
            xpc_dictionary_set_uint64(message, AUTH_XPC_TYPE, AUTHORIZATION_SETUP);
            mach_port_t bootstrap = MACH_PORT_NULL;
            task_get_bootstrap_port(mach_task_self(), &bootstrap);
            xpc_dictionary_set_mach_send(message, AUTH_XPC_BOOTSTRAP, bootstrap);
            xpc_object_t reply = xpc_connection_send_message_with_reply_sync(connection, message);
            xpc_release_safe(message);
            xpc_release_safe(reply);
        }
    });
    
    return connection;
}

static void
setItemSet(xpc_object_t message, const char * key, const AuthorizationItemSet * itemSet)
{
    xpc_object_t serialized = SerializeItemSet(itemSet);
    if (serialized) {
        xpc_dictionary_set_value(message, key, serialized);
        xpc_release(serialized);
    }
}

OSStatus AuthorizationCreate(const AuthorizationRights *rights,
                    const AuthorizationEnvironment *environment,
                    AuthorizationFlags flags,
                    AuthorizationRef *authorization)
{
    OSStatus status = errAuthorizationInternal;
    xpc_object_t message = NULL;
    xpc_object_t reply = NULL;

//    require_action(!(rights == NULL && authorization == NULL), done, status = errAuthorizationInvalidSet);
    
    // Send
    message = xpc_dictionary_create(NULL, NULL, 0);
    require_action(message != NULL, done, status = errAuthorizationInternal);
    
    xpc_dictionary_set_uint64(message, AUTH_XPC_TYPE, AUTHORIZATION_CREATE);
    setItemSet(message, AUTH_XPC_RIGHTS, rights);
    setItemSet(message, AUTH_XPC_ENVIRONMENT, environment);
    xpc_dictionary_set_uint64(message, AUTH_XPC_FLAGS, flags | (authorization ? 0 : kAuthorizationFlagNoData));
    
    // Reply
    xpc_connection_t conn = get_authorization_connection();
    require_action(conn != NULL, done, status = errAuthorizationInternal);
    reply = xpc_connection_send_message_with_reply_sync(conn, message);
    require_action_quiet(reply != NULL, done, status = errAuthorizationInternal);
    require_action_quiet(xpc_get_type(reply) != XPC_TYPE_ERROR, done, status = errAuthorizationInternal;);

    // Status
    status = (OSStatus)xpc_dictionary_get_int64(reply, AUTH_XPC_STATUS);
    
    // Out
    if (authorization && status == errAuthorizationSuccess) {
        size_t len;
        const void * data = xpc_dictionary_get_data(reply, AUTH_XPC_BLOB, &len);
        require_action(data != NULL, done, status = errAuthorizationInternal);
        require_action(len == sizeof(AuthorizationBlob), done, status = errAuthorizationInternal);
        
        AuthorizationBlob * blob = (AuthorizationBlob*)calloc(1u, sizeof(AuthorizationBlob));
        require_action(blob != NULL, done, status = errAuthorizationInternal);
        *blob = *(AuthorizationBlob*)data;
        
        *authorization = (AuthorizationRef)blob;
    }

done:
    xpc_release_safe(message);
    xpc_release_safe(reply);
    return status;
}

OSStatus AuthorizationCreateWithAuditToken(audit_token_t token,
                                 const AuthorizationEnvironment *environment,
                                 AuthorizationFlags flags,
                                 AuthorizationRef *authorization)
{
    OSStatus status = errAuthorizationInternal;
    xpc_object_t message = NULL;
    xpc_object_t reply = NULL;
    
    require_action(authorization != NULL, done, status = errAuthorizationInvalidPointer);
    
    // Send
    message = xpc_dictionary_create(NULL, NULL, 0);
    require_action(message != NULL, done, status = errAuthorizationInternal);
    
    xpc_dictionary_set_uint64(message, AUTH_XPC_TYPE, AUTHORIZATION_CREATE_WITH_AUDIT_TOKEN);
    xpc_dictionary_set_data(message, AUTH_XPC_DATA, &token, sizeof(token));
    setItemSet(message, AUTH_XPC_ENVIRONMENT, environment);
    xpc_dictionary_set_uint64(message, AUTH_XPC_FLAGS, flags);
    
    // Reply
    xpc_connection_t conn = get_authorization_connection();
    require_action(conn != NULL, done, status = errAuthorizationInternal);
    reply = xpc_connection_send_message_with_reply_sync(conn, message);
    require_action(reply != NULL, done, status = errAuthorizationInternal);
    require_action(xpc_get_type(reply) != XPC_TYPE_ERROR, done, status = errAuthorizationInternal);
    
    // Status
    status = (OSStatus)xpc_dictionary_get_int64(reply, AUTH_XPC_STATUS);
    
    // Out
    if (status == errAuthorizationSuccess) {
        size_t len;
        const void * data = xpc_dictionary_get_data(reply, AUTH_XPC_BLOB, &len);
        require_action(data != NULL, done, status = errAuthorizationInternal);
        require_action(len == sizeof(AuthorizationBlob), done, status = errAuthorizationInternal);

        AuthorizationBlob * blob = (AuthorizationBlob*)calloc(1u, sizeof(AuthorizationBlob));
        require_action(blob != NULL, done, status = errAuthorizationInternal);
        *blob = *(AuthorizationBlob*)data;
        
        *authorization = (AuthorizationRef)blob;
    }
    
done:
    xpc_release_safe(message);
    xpc_release_safe(reply);
    return status;
}

OSStatus AuthorizationFree(AuthorizationRef authorization, AuthorizationFlags flags)
{
    OSStatus status = errAuthorizationInternal;
    xpc_object_t message = NULL;
    xpc_object_t reply = NULL;
    AuthorizationBlob *blob = NULL;

    require_action(authorization != NULL, done, status = errAuthorizationInvalidRef);
    blob = (AuthorizationBlob *)authorization;
    
    // Send
    message = xpc_dictionary_create(NULL, NULL, 0);
    require_action(message != NULL, done, status = errAuthorizationInternal);
    
    xpc_dictionary_set_uint64(message, AUTH_XPC_TYPE, AUTHORIZATION_FREE);
    xpc_dictionary_set_data(message, AUTH_XPC_BLOB, blob, sizeof(AuthorizationBlob));
    xpc_dictionary_set_uint64(message, AUTH_XPC_FLAGS, flags);

    // Reply
    xpc_connection_t conn = get_authorization_connection();
    require_action(conn != NULL, done, status = errAuthorizationInternal);
    reply = xpc_connection_send_message_with_reply_sync(conn, message);
    require_action(reply != NULL, done, status = errAuthorizationInternal);
    require_action(xpc_get_type(reply) != XPC_TYPE_ERROR, done, status = errAuthorizationInternal);
    
    // Status
    status = (OSStatus)xpc_dictionary_get_int64(reply, AUTH_XPC_STATUS);
    
    // Free
    free(blob);
    
done:
    xpc_release_safe(message);
    xpc_release_safe(reply);
    return status;
}

OSStatus AuthorizationCopyRightProperties(const char *rightName, CFDictionaryRef *output)
{
    OSStatus status = errAuthorizationInternal;
    xpc_object_t reply = NULL;
    CFDataRef data = NULL;
    xpc_object_t message = NULL;
    require_action(rightName != NULL, done, status = errAuthorizationInvalidPointer);
    
    message = xpc_dictionary_create(NULL, NULL, 0);
    require_action(message != NULL, done, status = errAuthorizationInternal);
    
    xpc_dictionary_set_uint64(message, AUTH_XPC_TYPE, AUTHORIZATION_COPY_RIGHT_PROPERTIES);
    xpc_dictionary_set_string(message, AUTH_XPC_RIGHT_NAME, rightName);
    
    xpc_connection_t conn = get_authorization_connection();
    require_action(conn != NULL, done, status = errAuthorizationInternal);
    reply = xpc_connection_send_message_with_reply_sync(conn, message);
    require_action(reply != NULL, done, status = errAuthorizationInternal);
    require_action(xpc_get_type(reply) != XPC_TYPE_ERROR, done, status = errAuthorizationInternal);
    
    // Status
    status = (OSStatus)xpc_dictionary_get_int64(reply, AUTH_XPC_STATUS);
    if (output && status == errAuthorizationSuccess) {
        size_t len;
        const void *bytes = xpc_dictionary_get_data(reply, AUTH_XPC_OUT_ITEMS, &len);
        data = CFDataCreate(kCFAllocatorDefault, bytes, len);
        require_action(data != NULL, done, status = errAuthorizationInternal);
        *output = CFPropertyListCreateWithData(kCFAllocatorDefault, data, kCFPropertyListImmutable, NULL, NULL);
    }
done:
	xpc_release_safe(message);
    xpc_release_safe(reply);
    CFReleaseSafe(data);
    
	return status;
}

static OSStatus
_AuthorizationCopyRights_send_message(xpc_object_t message, AuthorizationRights **authorizedRights)
{
    OSStatus status = errAuthorizationInternal;
    xpc_object_t reply = NULL;

    // Send
    require_action(message != NULL, done, status = errAuthorizationInternal);

    // Reply
    xpc_connection_t conn = get_authorization_connection();
    require_action(conn != NULL, done, status = errAuthorizationInternal);
    reply = xpc_connection_send_message_with_reply_sync(conn, message);
    require_action(reply != NULL, done, status = errAuthorizationInternal);
    require_action(xpc_get_type(reply) != XPC_TYPE_ERROR, done, status = errAuthorizationInternal);

    // Status
    status = (OSStatus)xpc_dictionary_get_int64(reply, AUTH_XPC_STATUS);

    // Out
    if (authorizedRights && status == errAuthorizationSuccess) {
        xpc_object_t tmpItems = xpc_dictionary_get_value(reply, AUTH_XPC_OUT_ITEMS);
        AuthorizationRights * grantedRights = DeserializeItemSet(tmpItems);
        require_action(grantedRights != NULL, done, status = errAuthorizationInternal);

        *authorizedRights = grantedRights;
    }

done:
    xpc_release_safe(reply);
    return status;
}

static OSStatus
_AuthorizationCopyRights_prepare_message(AuthorizationRef authorization, const AuthorizationRights *rights, const AuthorizationEnvironment *environment, AuthorizationFlags flags, xpc_object_t *message_out)
{
    OSStatus status = errAuthorizationInternal;
    AuthorizationBlob *blob = NULL;
    xpc_object_t message = xpc_dictionary_create(NULL, NULL, 0);
    require_action(message != NULL, done, status = errAuthorizationInternal);

    require_action(authorization != NULL, done, status = errAuthorizationInvalidRef);
    blob = (AuthorizationBlob *)authorization;

    xpc_dictionary_set_uint64(message, AUTH_XPC_TYPE, AUTHORIZATION_COPY_RIGHTS);
    xpc_dictionary_set_data(message, AUTH_XPC_BLOB, blob, sizeof(AuthorizationBlob));
    setItemSet(message, AUTH_XPC_RIGHTS, rights);
    setItemSet(message, AUTH_XPC_ENVIRONMENT, environment);
    xpc_dictionary_set_uint64(message, AUTH_XPC_FLAGS, flags);

    *message_out = message;
    message = NULL;
    status = errAuthorizationSuccess;

done:
    xpc_release_safe(message);
    return status;
}

OSStatus AuthorizationCopyRights(AuthorizationRef authorization,
                        const AuthorizationRights *rights,
                        const AuthorizationEnvironment *environment,
                        AuthorizationFlags flags,
                        AuthorizationRights **authorizedRights)
{
    OSStatus status = errAuthorizationInternal;
    
    if ((flags & kAuthorizationFlagSheet) && (flags & kAuthorizationFlagInteractionAllowed)) {
        if (!environment) {
            os_log_error(AUTH_LOG, "Sheet authorization requested but no environment was provided");
            goto securityAgentFallback;
        }
        // check if window ID is present in environment
        UInt64 window = 0;
        for (UInt32 i = 0; i < environment->count; ++i) {
            if (strncmp(environment->items[i].name, kAuthorizationEnvironmentWindowId, strlen(kAuthorizationEnvironmentWindowId)) == 0 && environment->items[i].value ) {
                if (environment->items[i].valueLength == sizeof(UInt64)) {
                    window = *(UInt64 *)environment->items[i].value;
                    break;
                } else if (environment->items[i].valueLength == sizeof(CGWindowID)) {
                    window = *(CGWindowID *)environment->items[i].value;
                    break;
                }
            }
        }

        if (window == 0) {
            os_log_error(AUTH_LOG, "Sheet authorization requested but no valid window handle was provided");
            goto securityAgentFallback;
        }
        os_log_debug(AUTH_LOG, "Trying to use sheet version");
        static OSStatus (*SFAuthorizationSheetWorker)(CGWindowID windowID, AuthorizationRef authorization,
                                                      const AuthorizationRights *rights,
                                                      const AuthorizationEnvironment *environment,
                                                      AuthorizationFlags flags,
                                                      AuthorizationRights **authorizedRights,
                                                      Boolean *authorizationLaunched) = NULL;
        static dispatch_once_t onceToken;
        dispatch_once(&onceToken, ^{
            void *handle = dlopen("/System/Library/Frameworks/SecurityInterface.framework/SecurityInterface", RTLD_NOW|RTLD_GLOBAL|RTLD_NODELETE);
            if (handle) {
                SFAuthorizationSheetWorker = dlsym(handle, "SFAuthorizationSheetWorker");
            }
        });
        
        if (!SFAuthorizationSheetWorker) {
            os_log_error(AUTH_LOG, "Failed to find sheet support in SecurityInterface");
            goto securityAgentFallback;
        }
        Boolean authorizationLaunched;
        status = SFAuthorizationSheetWorker((CGWindowID)window, authorization, rights, environment, flags, authorizedRights, &authorizationLaunched);
        if (authorizationLaunched == true) {
            os_log_debug(AUTH_LOG, "Returning sheet result %d", (int)status);
            return status;
        }
        
securityAgentFallback:
        // fall back to the standard (windowed) version if sheets are not available
        flags &= ~kAuthorizationFlagSheet;
        flags |= kAuthorizationFlagInteractionAllowed;
        os_log(AUTH_LOG, "Sheet authorization cannot be used this time, falling back to the SecurityAgent UI");
    }

xpc_object_t message = NULL;

require_noerr(status = _AuthorizationCopyRights_prepare_message(authorization, rights, environment, flags, &message), done);
require_noerr(status = _AuthorizationCopyRights_send_message(message, authorizedRights), done);

done:
    xpc_release_safe(message);
    return status;
}


void AuthorizationCopyRightsAsync(AuthorizationRef authorization,
                         const AuthorizationRights *rights,
                         const AuthorizationEnvironment *environment,
                         AuthorizationFlags flags,
                         AuthorizationAsyncCallback callbackBlock)
{
    OSStatus prepare_status = errAuthorizationInternal;
    __block xpc_object_t message = NULL;

    prepare_status = _AuthorizationCopyRights_prepare_message(authorization, rights, environment, flags, &message);
    if (prepare_status != errAuthorizationSuccess) {
        callbackBlock(prepare_status, NULL);
    }

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
		AuthorizationRights *blockAuthorizedRights = NULL;
        OSStatus status = _AuthorizationCopyRights_send_message(message, &blockAuthorizedRights);
        callbackBlock(status, blockAuthorizedRights);
        xpc_release_safe(message);
	});
}

OSStatus AuthorizationDismiss(void)
{
    OSStatus status = errAuthorizationInternal;
    xpc_object_t message = NULL;
    xpc_object_t reply = NULL;

    // Send
    message = xpc_dictionary_create(NULL, NULL, 0);
    require(message != NULL, done);
    
    xpc_dictionary_set_uint64(message, AUTH_XPC_TYPE, AUTHORIZATION_DISMISS);
    
    // Reply
    xpc_connection_t conn = get_authorization_connection();
    require_action(conn != NULL, done, status = errAuthorizationInternal);
    reply = xpc_connection_send_message_with_reply_sync(conn, message);
    require_action(reply != NULL, done, status = errAuthorizationInternal);
    require_action(xpc_get_type(reply) != XPC_TYPE_ERROR, done, status = errAuthorizationInternal);
    
    // Status
    status = (OSStatus)xpc_dictionary_get_int64(reply, AUTH_XPC_STATUS);
    
done:
    xpc_release_safe(message);
    xpc_release_safe(reply);
    return status;
}

OSStatus AuthorizationCopyInfo(AuthorizationRef authorization,
                      AuthorizationString tag,
                      AuthorizationItemSet **info)
{
    OSStatus status = errAuthorizationInternal;
    xpc_object_t message = NULL;
    xpc_object_t reply = NULL;
    AuthorizationBlob *blob = NULL;
    
    require_action(info != NULL, done, status = errAuthorizationInvalidSet);
    require_action(authorization != NULL, done, status = errAuthorizationInvalidRef);
    blob = (AuthorizationBlob *)authorization;
    
    // Send
    message = xpc_dictionary_create(NULL, NULL, 0);
    require_action(message != NULL, done, status = errAuthorizationInternal);
    
    xpc_dictionary_set_uint64(message, AUTH_XPC_TYPE, AUTHORIZATION_COPY_INFO);
    xpc_dictionary_set_data(message, AUTH_XPC_BLOB, blob, sizeof(AuthorizationBlob));
    if (tag) {
        xpc_dictionary_set_string(message, AUTH_XPC_TAG, tag);
    }
    
    // Reply
    xpc_connection_t conn = get_authorization_connection();
    require_action(conn != NULL, done, status = errAuthorizationInternal);
    reply = xpc_connection_send_message_with_reply_sync(conn, message);
    require_action(reply != NULL, done, status = errAuthorizationInternal);
    require_action(xpc_get_type(reply) != XPC_TYPE_ERROR, done, status = errAuthorizationInternal);
    
    // Status
    status = (OSStatus)xpc_dictionary_get_int64(reply, AUTH_XPC_STATUS);

    // Out
    if (info && status == errAuthorizationSuccess) {
        xpc_object_t tmpItems = xpc_dictionary_get_value(reply, AUTH_XPC_OUT_ITEMS);
        AuthorizationRights * outInfo = DeserializeItemSet(tmpItems);
        require_action(outInfo != NULL, done, status = errAuthorizationInternal);
        
        *info = outInfo;
    }
    
done:
    xpc_release_safe(message);
    xpc_release_safe(reply);
    return status;
}

OSStatus AuthorizationMakeExternalForm(AuthorizationRef authorization,
                              AuthorizationExternalForm *extForm)
{
    OSStatus status = errAuthorizationInternal;
    xpc_object_t message = NULL;
    xpc_object_t reply = NULL;
    AuthorizationBlob *blob = NULL;
    
    require_action(extForm != NULL, done, status = errAuthorizationInvalidPointer);
    require_action(authorization != NULL, done, status = errAuthorizationInvalidRef);
    blob = (AuthorizationBlob *)authorization;
    
    // Send
    message = xpc_dictionary_create(NULL, NULL, 0);
    require_action(message != NULL, done, status = errAuthorizationInternal);
    
    xpc_dictionary_set_uint64(message, AUTH_XPC_TYPE, AUTHORIZATION_MAKE_EXTERNAL_FORM);
    xpc_dictionary_set_data(message, AUTH_XPC_BLOB, blob, sizeof(AuthorizationBlob));
    
    // Reply
    xpc_connection_t conn = get_authorization_connection();
    require_action(conn != NULL, done, status = errAuthorizationInternal);
    reply = xpc_connection_send_message_with_reply_sync(conn, message);
    require_action(reply != NULL, done, status = errAuthorizationInternal);
    require_action(xpc_get_type(reply) != XPC_TYPE_ERROR, done, status = errAuthorizationInternal);
    
    // Status
    status = (OSStatus)xpc_dictionary_get_int64(reply, AUTH_XPC_STATUS);
    
    // out
    if (status == errAuthorizationSuccess) {
        size_t len;
        const void * data = xpc_dictionary_get_data(reply, AUTH_XPC_EXTERNAL, &len);
        require_action(data != NULL, done, status = errAuthorizationInternal);
        require_action(len == sizeof(AuthorizationExternalForm), done, status = errAuthorizationInternal);

        *extForm = *(AuthorizationExternalForm*)data;
    }
    
done:
    xpc_release_safe(message);
    xpc_release_safe(reply);
    return status;
}

OSStatus AuthorizationCreateFromExternalForm(const AuthorizationExternalForm *extForm,
                                    AuthorizationRef *authorization)
{
    OSStatus status = errAuthorizationInternal;
    xpc_object_t message = NULL;
    xpc_object_t reply = NULL;
    
    require_action(extForm != NULL, done, status = errAuthorizationInvalidPointer);
    require_action(authorization != NULL, done, status = errAuthorizationInvalidRef);
    
    // Send
    message = xpc_dictionary_create(NULL, NULL, 0);
    require_action(message != NULL, done, status = errAuthorizationInternal);
    
    xpc_dictionary_set_uint64(message, AUTH_XPC_TYPE, AUTHORIZATION_CREATE_FROM_EXTERNAL_FORM);
    xpc_dictionary_set_data(message, AUTH_XPC_EXTERNAL, extForm, sizeof(AuthorizationExternalForm));
    
    // Reply
    xpc_connection_t conn = get_authorization_connection();
    require_action(conn != NULL, done, status = errAuthorizationInternal);
    reply = xpc_connection_send_message_with_reply_sync(conn, message);
    require_action(reply != NULL, done, status = errAuthorizationInternal);
    require_action(xpc_get_type(reply) != XPC_TYPE_ERROR, done, status = errAuthorizationInternal);
    
    // Status
    status = (OSStatus)xpc_dictionary_get_int64(reply, AUTH_XPC_STATUS);
    
    // Out
    if (authorization && status == errAuthorizationSuccess) {
        size_t len;
        const void * data = xpc_dictionary_get_data(reply, AUTH_XPC_BLOB, &len);
        require_action(data != NULL, done, status = errAuthorizationInternal);
        require_action(len == sizeof(AuthorizationBlob), done, status = errAuthorizationInternal);

        AuthorizationBlob * blob = (AuthorizationBlob*)calloc(1u, sizeof(AuthorizationBlob));
        require_action(blob != NULL, done, status = errAuthorizationInternal);
        *blob = *(AuthorizationBlob*)data;
        
        *authorization = (AuthorizationRef)blob;
    }
    
done:
    xpc_release_safe(message);
    xpc_release_safe(reply);
    return status;
}

OSStatus AuthorizationFreeItemSet(AuthorizationItemSet *set)
{
    FreeItemSet(set);
    return errAuthorizationSuccess;
}

OSStatus AuthorizationRightGet(const char *rightName,
                      CFDictionaryRef *rightDefinition)
{
    OSStatus status = errAuthorizationInternal;
    xpc_object_t message = NULL;
    xpc_object_t reply = NULL;
    
    require_action(rightName != NULL, done, status = errAuthorizationInvalidPointer);
    
    // Send
    message = xpc_dictionary_create(NULL, NULL, 0);
    require_action(message != NULL, done, status = errAuthorizationInternal);
    
    xpc_dictionary_set_uint64(message, AUTH_XPC_TYPE, AUTHORIZATION_RIGHT_GET);
    xpc_dictionary_set_string(message, AUTH_XPC_RIGHT_NAME, rightName);
    
    // Reply
    xpc_connection_t conn = get_authorization_connection();
    require_action(conn != NULL, done, status = errAuthorizationInternal);
    reply = xpc_connection_send_message_with_reply_sync(conn, message);
    require_action(reply != NULL, done, status = errAuthorizationInternal);
    require_action(xpc_get_type(reply) != XPC_TYPE_ERROR, done, status = errAuthorizationInternal);
    
    // Status
    status = (OSStatus)xpc_dictionary_get_int64(reply, AUTH_XPC_STATUS);
    
    // Out
    if (rightDefinition && status == errAuthorizationSuccess) {
        xpc_object_t value = xpc_dictionary_get_value(reply, AUTH_XPC_DATA);
        require_action(value != NULL, done, status = errAuthorizationInternal);
        require_action(xpc_get_type(value) == XPC_TYPE_DICTIONARY, done, status = errAuthorizationInternal);
        
        CFTypeRef cfdict = _CFXPCCreateCFObjectFromXPCObject(value);
        require_action(cfdict != NULL, done, status = errAuthorizationInternal);
        
        *rightDefinition = cfdict;
    }
    
done:
    xpc_release_safe(message);
    xpc_release_safe(reply);
    return status;
}

OSStatus AuthorizationRightSet(AuthorizationRef authRef,
                      const char *rightName,
                      CFTypeRef rightDefinition,
                      CFStringRef descriptionKey,
                      CFBundleRef bundle,
                      CFStringRef tableName)
{
    OSStatus status = errAuthorizationInternal;
    xpc_object_t message = NULL;
    xpc_object_t reply = NULL;
    AuthorizationBlob *blob = NULL;
    CFMutableDictionaryRef rightDict = NULL;
    CFBundleRef clientBundle = bundle;
    
    if (bundle) {
        CFRetain(bundle);
    }
    
    require_action(rightDefinition != NULL, done, status = errAuthorizationInvalidPointer);
    require_action(rightName != NULL, done, status = errAuthorizationInvalidPointer);
    require_action(authRef != NULL, done, status = errAuthorizationInvalidRef);
    blob = (AuthorizationBlob *)authRef;
    
    // Send
    message = xpc_dictionary_create(NULL, NULL, 0);
    require_action(message != NULL, done, status = errAuthorizationInternal);
    
    xpc_dictionary_set_uint64(message, AUTH_XPC_TYPE, AUTHORIZATION_RIGHT_SET);
    xpc_dictionary_set_data(message, AUTH_XPC_BLOB, blob, sizeof(AuthorizationBlob));
    xpc_dictionary_set_string(message, AUTH_XPC_RIGHT_NAME, rightName);
    
    // Create rightDict
    if (CFGetTypeID(rightDefinition) == CFStringGetTypeID()) {
        rightDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        require_action(rightDict != NULL, done, status = errAuthorizationInternal);
        
        CFDictionarySetValue(rightDict, CFSTR(kAuthorizationRightRule), rightDefinition);
    
    } else if (CFGetTypeID(rightDefinition) == CFDictionaryGetTypeID()) {
        rightDict = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, rightDefinition);
        require_action(rightDict != NULL, done, status = errAuthorizationInternal);
    
    } else {
        status = errAuthorizationInvalidPointer;
        goto done;
    }
    
    // Create locDict
    if (descriptionKey) {
        CFMutableDictionaryRef locDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        require_action(locDict != NULL, done, status = errAuthorizationInternal);
        
        if (clientBundle == NULL) {
            clientBundle = CFBundleGetMainBundle();
            CFRetain(clientBundle);
        }
        
        if (clientBundle) {
            CFArrayRef bundleLocalizations = CFBundleCopyBundleLocalizations(clientBundle);
            if (bundleLocalizations) {
                // for every CFString in localizations do
                CFIndex locIndex, allLocs = CFArrayGetCount(bundleLocalizations);
                for (locIndex = 0; locIndex < allLocs; locIndex++)
                {
                    CFStringRef oneLocalization = (CFStringRef)CFArrayGetValueAtIndex(bundleLocalizations, locIndex);
                    
                    if (!oneLocalization)
                        continue;

                    CFStringRef value = CFBundleCopyLocalizedStringForLocalization(clientBundle, descriptionKey, NULL, tableName ? tableName :  CFSTR("Localizable"), oneLocalization);

                    if (value == NULL || CFEqual(value, CFSTR(""))) {
                        CFReleaseSafe(value);
                        continue;
                    } else {
                        // oneLocalization/value into our dictionary
                        CFDictionarySetValue(locDict, oneLocalization, value);
                    }
                    CFReleaseSafe(value);
                }
                CFReleaseSafe(bundleLocalizations);
            }
        }
        
        // add the description as the default localization into the dictionary
		CFDictionarySetValue(locDict, CFSTR(""), descriptionKey);
		
		// stuff localization table into right definition
		CFDictionarySetValue(rightDict, CFSTR(kAuthorizationRuleParameterDefaultPrompt), locDict);
        CFReleaseSafe(locDict);
    }
    
    xpc_object_t value = _CFXPCCreateXPCObjectFromCFObject(rightDict);
    xpc_dictionary_set_value(message, AUTH_XPC_DATA, value);
    xpc_release_safe(value);
    
    // Reply
    xpc_connection_t conn = get_authorization_connection();
    require_action(conn != NULL, done, status = errAuthorizationInternal);
    reply = xpc_connection_send_message_with_reply_sync(conn, message);
    require_action(reply != NULL, done, status = errAuthorizationInternal);
    require_action(xpc_get_type(reply) != XPC_TYPE_ERROR, done, status = errAuthorizationInternal);
    
    // Status
    status = (OSStatus)xpc_dictionary_get_int64(reply, AUTH_XPC_STATUS);
    
done:
    CFReleaseSafe(clientBundle);
    CFReleaseSafe(rightDict);
    xpc_release_safe(message);
    xpc_release_safe(reply);
    return status;
}

OSStatus AuthorizationRightRemove(AuthorizationRef authorization,
                         const char *rightName)
{
    OSStatus status = errAuthorizationInternal;
    xpc_object_t message = NULL;
    xpc_object_t reply = NULL;
    AuthorizationBlob *blob = NULL;
    
    require_action(rightName != NULL, done, status = errAuthorizationInvalidPointer);
    require_action(authorization != NULL, done, status = errAuthorizationInvalidRef);
    blob = (AuthorizationBlob *)authorization;
    
    // Send
    message = xpc_dictionary_create(NULL, NULL, 0);
    require_action(message != NULL, done, status = errAuthorizationInternal);
    
    xpc_dictionary_set_uint64(message, AUTH_XPC_TYPE, AUTHORIZATION_RIGHT_REMOVE);
    xpc_dictionary_set_data(message, AUTH_XPC_BLOB, blob, sizeof(AuthorizationBlob));
    xpc_dictionary_set_string(message, AUTH_XPC_RIGHT_NAME, rightName);
    
    // Reply
    xpc_connection_t conn = get_authorization_connection();
    require_action(conn != NULL, done, status = errAuthorizationInternal);
    reply = xpc_connection_send_message_with_reply_sync(conn, message);
    require_action(reply != NULL, done, status = errAuthorizationInternal);
    require_action(xpc_get_type(reply) != XPC_TYPE_ERROR, done, status = errAuthorizationInternal);
    
    // Status
    status = (OSStatus)xpc_dictionary_get_int64(reply, AUTH_XPC_STATUS);
    
done:
    xpc_release_safe(message);
    xpc_release_safe(reply);
    return status;
}

OSStatus AuthorizationCopyPreloginUserDatabase(const char * _Nullable const volumeUuid, const UInt32 flags, CFArrayRef _Nonnull * _Nonnull output)
{
    OSStatus status = errAuthorizationInternal;
    xpc_object_t message = NULL;
    xpc_object_t reply = NULL;

    require_action(output != NULL, done, status = errAuthorizationInvalidRef);
    
    // Send
    message = xpc_dictionary_create(NULL, NULL, 0);
    require_action(message != NULL, done, status = errAuthorizationInternal);
    xpc_dictionary_set_uint64(message, AUTH_XPC_TYPE, AUTHORIZATION_COPY_PRELOGIN_USERDB);
    if (volumeUuid) {
        xpc_dictionary_set_string(message, AUTH_XPC_TAG, volumeUuid);
    }
    xpc_dictionary_set_uint64(message, AUTH_XPC_FLAGS, flags);

    // Reply
    xpc_connection_t conn = get_authorization_connection();
    require_action(conn != NULL, done, status = errAuthorizationInternal);
    reply = xpc_connection_send_message_with_reply_sync(conn, message);
    require_action(reply != NULL, done, status = errAuthorizationInternal);
    require_action(xpc_get_type(reply) != XPC_TYPE_ERROR, done, status = errAuthorizationInternal);
    
    status = (OSStatus)xpc_dictionary_get_int64(reply, AUTH_XPC_STATUS);
    
    // fill the output
    if (status == errAuthorizationSuccess) {
        *output = _CFXPCCreateCFObjectFromXPCObject(xpc_dictionary_get_value(reply, AUTH_XPC_DATA));
    }

done:
    xpc_release_safe(message);
    xpc_release_safe(reply);
    return status;
}

OSStatus AuthorizationCopyPreloginPreferencesValue(const char * _Nonnull const volumeUuid, const char * _Nullable const username, const char * _Nonnull const domain, const char * _Nullable const item, CFTypeRef _Nonnull * _Nonnull output)
{
    OSStatus status = errAuthorizationInternal;
    xpc_object_t message = NULL;
    xpc_object_t reply = NULL;

    require_action(output != NULL, done, status = errAuthorizationInvalidRef);
    
    // Send
    message = xpc_dictionary_create(NULL, NULL, 0);
    require_action(message != NULL, done, status = errAuthorizationInternal);
    xpc_dictionary_set_uint64(message, AUTH_XPC_TYPE, AUTHORIZATION_COPY_PRELOGIN_PREFS);
    if (volumeUuid) {
        xpc_dictionary_set_string(message, AUTH_XPC_TAG, volumeUuid);
    }
    if (username) {
        xpc_dictionary_set_string(message, AUTH_XPC_RIGHT_NAME, username);
    }
    if (domain) {
        xpc_dictionary_set_string(message, AUTH_XPC_HINTS_NAME, domain);
    }
    if (item) {
        xpc_dictionary_set_string(message, AUTH_XPC_ITEM_NAME, item);
    }

    // Reply
    reply = xpc_connection_send_message_with_reply_sync(get_authorization_connection(), message);
    require_action(reply != NULL, done, status = errAuthorizationInternal);
    require_action(xpc_get_type(reply) != XPC_TYPE_ERROR, done, status = errAuthorizationInternal);
    
    status = (OSStatus)xpc_dictionary_get_int64(reply, AUTH_XPC_STATUS);
    
    // fill the output
    if (status == errAuthorizationSuccess) {
        *output = _CFXPCCreateCFObjectFromXPCObject(xpc_dictionary_get_value(reply, AUTH_XPC_DATA));
    }

done:
    xpc_release_safe(message);
    xpc_release_safe(reply);
    return status;
}

OSStatus AuthorizationHandlePreloginOverride(const char * _Nonnull const volumeUuid, const char operation, Boolean * _Nullable result)
{
    OSStatus status = errAuthorizationInternal;
    xpc_object_t message = NULL;
    xpc_object_t reply = NULL;
    
    // Send
    message = xpc_dictionary_create(NULL, NULL, 0);
    require_action(message != NULL, done, status = errAuthorizationInternal);
    xpc_dictionary_set_uint64(message, AUTH_XPC_TYPE, AUTHORIZATION_PRELOGIN_SC_OVERRIDE);
    if (volumeUuid) {
        xpc_dictionary_set_string(message, AUTH_XPC_TAG, volumeUuid);
    }
    uint64_t op = operation;
    xpc_dictionary_set_uint64(message, AUTH_XPC_ITEM_NAME, op);

    // Reply
    reply = xpc_connection_send_message_with_reply_sync(get_authorization_connection(), message);
    require_action(reply != NULL, done, status = errAuthorizationInternal);
    require_action(xpc_get_type(reply) != XPC_TYPE_ERROR, done, status = errAuthorizationInternal);
    
    status = (OSStatus)xpc_dictionary_get_int64(reply, AUTH_XPC_STATUS);
    
    // fill the output only if it is present in the dictionary and caller requested it
    if (status == errAuthorizationSuccess && result && xpc_dictionary_get_value(reply, AUTH_XPC_ITEM_VALUE)) {
        *result = xpc_dictionary_get_bool(reply, AUTH_XPC_ITEM_VALUE);
    }

done:
    xpc_release_safe(message);
    xpc_release_safe(reply);
    return status;
}
