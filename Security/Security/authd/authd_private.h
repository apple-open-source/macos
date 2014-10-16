/* Copyright (c) 2012-2013 Apple Inc. All Rights Reserved. */

#ifndef _AUTHD_PRIVATE_H_
#define _AUTHD_PRIVATE_H_

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define AUTH_NONNULL1 __attribute__((__nonnull__(1)))
#define AUTH_NONNULL2 __attribute__((__nonnull__(2)))
#define AUTH_NONNULL3 __attribute__((__nonnull__(3)))
#define AUTH_NONNULL4 __attribute__((__nonnull__(4)))
#define AUTH_NONNULL5 __attribute__((__nonnull__(5)))
#define AUTH_NONNULL6 __attribute__((__nonnull__(6)))
#define AUTH_NONNULL7 __attribute__((__nonnull__(7)))
#define AUTH_NONNULL_ALL __attribute__((__nonnull__))
#define AUTH_MALLOC __attribute__((__malloc__))
#define AUTH_UNUSED __attribute__((unused))
#define AUTH_WARN_RESULT __attribute__((__warn_unused_result__))
#define AUTH_INLINE static __inline__ __attribute__((__always_inline__))

#if __has_feature(attribute_cf_returns_retained)
#define AUTH_RETURNS_RETAINED __attribute__((cf_returns_retained))
#else
#define AUTH_RETURNS_RETAINED
#endif

#if __has_feature(attribute_cf_returns_not_retained)
#define AUTH_RETURNS_NOT_RETAINED __attribute__((cf_returns_not_retained))
#else
#define AUTH_RETURNS_NOT_RETAINED
#endif
    
#if __has_feature(attribute_cf_consumed)
#define AUTH_CONSUMED __attribute__((cf_consumed))
#else
#define AUTH_CONSUMED
#endif
    
#define SECURITY_AUTH_NAME "com.apple.authd"
    
#define AUTH_XPC_TYPE       "_type"
#define AUTH_XPC_RIGHTS     "_rights"
#define AUTH_XPC_ENVIROMENT "_enviroment"
#define AUTH_XPC_FLAGS      "_flags"
#define AUTH_XPC_BLOB       "_blob"
#define AUTH_XPC_STATUS     "_status"
#define AUTH_XPC_RIGHT_NAME "_right_name"
#define AUTH_XPC_TAG        "_tag"
#define AUTH_XPC_OUT_ITEMS  "_out_items"
#define AUTH_XPC_EXTERNAL   "_external"
#define AUTH_XPC_DATA       "_data"
#define AUTH_XPC_BOOTSTRAP  "_bootstrap"

#define AUTH_XPC_ITEM_NAME  "_item_name"
#define AUTH_XPC_ITEM_FLAGS "_item_flags"
#define AUTH_XPC_ITEM_VALUE "_item_value"
#define AUTH_XPC_ITEM_TYPE  "_item_type"

#define AUTH_XPC_REQUEST_METHOD_KEY "_agent_request_key"
#define AUTH_XPC_REQUEST_METHOD_CREATE "_agent_request_create"
#define AUTH_XPC_REQUEST_METHOD_INVOKE "_agent_request_invoke"
#define AUTH_XPC_REQUEST_METHOD_DEACTIVATE "_agent_request_deactivate"
#define AUTH_XPC_REQUEST_METHOD_DESTROY "_agent_request_destroy"
#define AUTH_XPC_REQUEST_METHOD_INTERRUPT "_agent_request_interrupt"
#define AUTH_XPC_REQUEST_METHOD_SET_PREFS "_agent_request_setprefs"
#define AUTH_XPC_REPLY_METHOD_KEY "_agent_reply_key"
#define AUTH_XPC_REPLY_METHOD_RESULT "_agent_reply_result"
#define AUTH_XPC_REPLY_METHOD_INTERRUPT "_agent_reply_interrupt"
#define AUTH_XPC_REPLY_METHOD_CREATE "_agent_reply_create"
#define AUTH_XPC_REPLY_METHOD_DEACTIVATE "_agent_reply_deactivate"
#define AUTH_XPC_PLUGIN_NAME "_agent_plugin"
#define AUTH_XPC_MECHANISM_NAME "_agent_mechanism"
#define AUTH_XPC_HINTS_NAME "_agent_hints"
#define AUTH_XPC_CONTEXT_NAME "_agent_context"
#define AUTH_XPC_IMMUTABLE_HINTS_NAME "_agent_immutable_hints"
#define AUTH_XPC_REPLY_RESULT_VALUE "_agent_reply_result_value"
#define AUTH_XPC_AUDIT_SESSION_PORT "_agent_audit_session_port"
#define AUTH_XPC_BOOTSTRAP_PORT "_agent_bootstrap_port"
#define AUTH_XPC_SESSION_UUID "_agent_session_uuid"
#define AUTH_XPC_SESSION_PREFS "_agent_session_prefs"
#define AUTH_XPC_SESSION_INPUT_METHOD "_agent_session_inputMethod"

typedef struct AuthorizationBlob {
        uint32_t data[2];
} AuthorizationBlob;

typedef struct AuthorizationExternalBlob {
    AuthorizationBlob blob;
    int32_t session;
} AuthorizationExternalBlob;
    
enum {
    AUTHORIZATION_CREATE = 1,
    AUTHORIZATION_FREE,
    AUTHORIZATION_COPY_RIGHTS,
    AUTHORIZATION_COPY_INFO,
    AUTHORIZATION_MAKE_EXTERNAL_FORM,
    AUTHORIZATION_CREATE_FROM_EXTERNAL_FORM,
    AUTHORIZATION_RIGHT_GET,
    AUTHORIZATION_RIGHT_SET,
    AUTHORIZATION_RIGHT_REMOVE,
    SESSION_SET_USER_PREFERENCES,
    AUTHORIZATION_DEV,
    AUTHORIZATION_CREATE_WITH_AUDIT_TOKEN,
    AUTHORIZATION_DISMISS,
    AUTHORIZATION_SETUP,
    AUTHORIZATION_ENABLE_SMARTCARD
};
    
#if defined(__cplusplus)
}
#endif

#endif /* !_AUTHD_PRIVATE_H_ */
