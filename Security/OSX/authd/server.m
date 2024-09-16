/* Copyright (c) 2012-2013 Apple Inc. All Rights Reserved. */

#include <SecCFWrappers.h>
#include "server.h"
#include "session.h"
#include "process.h"
#include "authtoken.h"
#include "authdb.h"
#include "rule.h"
#include "authutilities.h"
#include "crc.h"
#include "mechanism.h"
#include "agent.h"
#include "authitems.h"
#include "debugging.h"
#include "engine.h"
#include "connection.h"
#include "AuthorizationTags.h"
#include "PreloginUserDb.h"
#include "Authorization.h"
#include "od.h"

#include <errno.h>
#include <libproc.h>
#include <bsm/audit_kevents.h>
#include <bsm/libbsm.h>
#include <Security/Authorization.h>
#include <Security/AuthorizationPriv.h>
#include <Security/AuthorizationTagsPriv.h>
#include <Security/AuthorizationPlugin.h>
#include <xpc/private.h>
#include <dispatch/dispatch.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFXPCBridge.h>
#include <IOKit/IOMessage.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>

#include <security_utilities/simulatecrash_assert.h>

AUTHD_DEFINE_LOG

#define MAX_PROCESS_RIGHTS   100

static CFMutableDictionaryRef gProcessMap = NULL;
static CFMutableDictionaryRef gSessionMap = NULL;
static CFMutableDictionaryRef gAuthTokenMap = NULL;
static authdb_t gDatabase = NULL;

static bool gXPCTransaction = false;

void checkForDbReset(void);

static dispatch_queue_t
get_server_dispatch_queue(void)
{
    static dispatch_once_t onceToken;
    static dispatch_queue_t server_queue = NULL;
    
    dispatch_once(&onceToken, ^{
        server_queue = dispatch_queue_create("com.apple.security.auth.server", DISPATCH_QUEUE_SERIAL);
        check(server_queue != NULL);
    });
    
    return server_queue;
}

static Boolean _processEqualCallBack(const void *value1, const void *value2)
{
    audit_info_s * info1 = (audit_info_s*)value1;
    audit_info_s * info2 = (audit_info_s*)value2;
    if (info1->pid == info2->pid) {
        if (info1->tid == info2->tid) {
            return true;
        }
    }
    return false;
}

static CFHashCode _processHashCallBack(const void *value)
{
    audit_info_s * info = (audit_info_s*)value;
    uint64_t crc = crc64_init();
    crc = crc64_update(crc, &info->pid, sizeof(info->pid));
    crc = crc64_update(crc, &info->tid, sizeof(info->tid));
    crc = crc64_final(crc);
    return (CFHashCode)crc;
}

static const CFDictionaryKeyCallBacks kProcessMapKeyCallBacks = {
    .version = 0,
    .retain = NULL,
    .release = NULL,
    .copyDescription = NULL,
    .equal = &_processEqualCallBack,
    .hash = &_processHashCallBack
};

static Boolean _sessionEqualCallBack(const void *value1, const void *value2)
{
    return (*(session_id_t*)value1) == (*(session_id_t*)value2);
}

static CFHashCode _sessionHashCallBack(const void *value)
{
    return (CFHashCode)(*(session_id_t*)(value));
}

static const CFDictionaryKeyCallBacks kSessionMapKeyCallBacks = {
    .version = 0,
    .retain = NULL,
    .release = NULL,
    .copyDescription = NULL,
    .equal = &_sessionEqualCallBack,
    .hash = &_sessionHashCallBack
};

void server_cleanup(void)
{
    CFRelease(gProcessMap);
    CFRelease(gSessionMap);
    CFRelease(gAuthTokenMap);
}

bool server_in_dark_wake(void)
{
    return IOPMIsADarkWake(IOPMConnectionGetSystemCapabilities());
}

authdb_t server_get_database(void)
{
    return gDatabase;
}

static void _setupAuditSessionMonitor(void)
{
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        au_sdev_handle_t *dev = au_sdev_open(AU_SDEVF_ALLSESSIONS);
        int event;
        auditinfo_addr_t aia;
        
        if (NULL == dev) {
            os_log_error(AUTHD_LOG, "server: could not open %{public}s %d", AUDIT_SDEV_PATH, errno);
            return;
        }
        
        for (;;) {
            if (0 != au_sdev_read_aia(dev, &event, &aia)) {
                os_log_error(AUTHD_LOG, "server: au_sdev_read_aia failed: %d", errno);
                continue;
            }
            if (event == AUE_SESSION_END) {
                dispatch_async(get_server_dispatch_queue(), ^{
                    CFDictionaryRemoveValue(gSessionMap, &aia.ai_asid);
                });
            }
        }
        
    });
}

static void _setupSignalHandlers(void)
{
    signal(SIGTERM, SIG_IGN);
    static dispatch_source_t sigtermHandler;
    sigtermHandler = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, SIGTERM, 0, get_server_dispatch_queue());
    if (sigtermHandler) {
        dispatch_source_set_event_handler(sigtermHandler, ^{

            // should we clean up any state?
            exit(EXIT_SUCCESS);
        });
        dispatch_resume(sigtermHandler);
    }
}

OSStatus server_init(void)
{
    OSStatus status = errAuthorizationSuccess;
    
    auditinfo_addr_t info;
    memset(&info, 0, sizeof(info));
    getaudit_addr(&info, sizeof(info));
    os_log_debug(AUTHD_LOG, "server: uid=%i, sid=%i", info.ai_auid, info.ai_asid);
    
    require_action(get_server_dispatch_queue() != NULL, done, status = errAuthorizationInternal);
    
    gProcessMap = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kProcessMapKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require_action(gProcessMap != NULL, done, status = errAuthorizationInternal);
    
    gSessionMap = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kSessionMapKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require_action(gSessionMap != NULL, done, status = errAuthorizationInternal);
    
    gAuthTokenMap = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kAuthTokenKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require_action(gAuthTokenMap != NULL, done, status = errAuthorizationInternal);
    
    gDatabase = authdb_create(false);
    require_action(gDatabase != NULL, done, status = errAuthorizationInternal);
    
    // check to see if we have an updates
    authdb_connection_t dbconn = authdb_connection_acquire(gDatabase);
    authdb_maintenance(dbconn);
    authdb_check_for_mandatory_rights(dbconn);
    authdb_connection_release(&dbconn);
    
    _setupAuditSessionMonitor();
    _setupSignalHandlers();
    
    if (!isInFVUnlockOrRecovery()) {
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            checkForDbReset();
        });
    }
done:
    return status;
}

void checkForDbReset(void)
{
    Boolean resetDb = prelogin_reset_db_wanted();
    if (resetDb) {
        os_log(AUTHD_LOG, "server: Database reset requested");
        authdb_connection_t dbconn = authdb_connection_acquire(gDatabase);
        OSStatus status = authdb_reset(dbconn);
        authdb_connection_release(&dbconn);
        if (status != noErr) {
            os_log_error(AUTHD_LOG, "server: Database reset failed: %d", (int)status);
        }
    }
}

static void _server_parse_audit_token(audit_token_t * token, audit_info_s * info)
{
    if (token && info) {
        memset(info, 0, sizeof(*info));
        au_tid_t tid;
        memset(&tid, 0, sizeof(tid));
        audit_token_to_au32(*token, &info->auid, &info->euid,
                            &info->egid, &info->ruid, &info->rgid,
                            &info->pid, &info->asid, &tid);
        info->tid = tid.port;
        info->opaqueToken = *token;
    }
}

connection_t
server_register_connection(xpc_connection_t connection)
{
    __block connection_t conn = NULL;
    __block session_t session = NULL;
    __block process_t proc = NULL;
    __block CFIndex conn_count = 0;

    require(connection != NULL, done);
    
    audit_token_t auditToken;
    audit_info_s info;
    xpc_connection_get_audit_token(connection, &auditToken);
    _server_parse_audit_token(&auditToken, &info);
    
    
    dispatch_sync(get_server_dispatch_queue(), ^{
        session = (session_t)CFDictionaryGetValue(gSessionMap, &info.asid);
        if (session) {
            CFRetain(session);
        } else {
            session = session_create(info.asid);
            CFDictionarySetValue(gSessionMap, session_get_key(session), session);
        }
        
        proc = (process_t)CFDictionaryGetValue(gProcessMap, &info);
        if (proc) {
            CFRetain(proc);
        }

        if (proc) {
            conn = connection_create(proc);
            conn_count = process_add_connection(proc, conn);
        } else {
            proc = process_create(&info, session);
            if (proc) {
                conn = connection_create(proc);
                conn_count = process_add_connection(proc, conn);
                session_add_process(session, proc);
                CFDictionarySetValue(gProcessMap, process_get_key(proc), proc);
            }
        }
        
        if (!gXPCTransaction) {
            xpc_transaction_begin();
            gXPCTransaction = true;
        }
    });
    
    os_log_debug(AUTHD_LOG, "server: registered connection (total=%li)", conn_count);

done:
    CFReleaseSafe(session);
    CFReleaseSafe(proc);
    return conn;
}

void
server_unregister_connection(connection_t conn)
{
    assert(conn); // marked non-null
    process_t proc = connection_get_process(conn);
    
    dispatch_sync(get_server_dispatch_queue(), ^{
        CFIndex connectionCount = process_get_connection_count(proc);
        os_log_debug(AUTHD_LOG, "server: unregistered connection (total=%li)", connectionCount);

        if (connectionCount == 1) {
            CFDictionaryRemoveValue(gProcessMap, process_get_key(proc));
        }
        
        if (CFDictionaryGetCount(gProcessMap) == 0) {
            xpc_transaction_end();
            gXPCTransaction = false;
        }
    });
    // move the destruction of the connection/process off the server queue
    CFRelease(conn);
}

void
server_register_auth_token(auth_token_t auth)
{
    assert(auth); // marked non-null
    dispatch_sync(get_server_dispatch_queue(), ^{
        os_log_debug(AUTHD_LOG, "server: registering authorization");
        CFDictionarySetValue(gAuthTokenMap, auth_token_get_key(auth), auth);
        auth_token_set_state(auth, auth_token_state_registered);
    });
}

void
server_unregister_auth_token(auth_token_t auth)
{
    assert(auth);
    AuthorizationBlob blob = *(AuthorizationBlob*)auth_token_get_key(auth);
    dispatch_async(get_server_dispatch_queue(), ^{
        os_log_debug(AUTHD_LOG, "server: unregistering authorization");
        CFDictionaryRemoveValue(gAuthTokenMap, &blob);
    });
}

auth_token_t
server_find_copy_auth_token(AuthorizationBlob * blob)
{
    assert(blob); // marked non-null
    __block auth_token_t auth = NULL;
    dispatch_sync(get_server_dispatch_queue(), ^{
        auth = (auth_token_t)CFDictionaryGetValue(gAuthTokenMap, blob);
        if (auth) {
            CFRetain(auth);
        }
    });
    return auth;
}

session_t
server_find_copy_session(session_id_t sid, bool create)
{
    __block session_t session = NULL;
    
    dispatch_sync(get_server_dispatch_queue(), ^{
        session = (session_t)CFDictionaryGetValue(gSessionMap, &sid);
        if (session) {
            CFRetain(session);
        } else if (create) {
            session = session_create(sid);
            if (session) {
                CFDictionarySetValue(gSessionMap, session_get_key(session), session);
            }
        }
    });
    
    return session;
}

#pragma mark -
#pragma mark API

static OSStatus
_process_find_copy_auth_token_from_xpc(process_t proc, xpc_object_t message, auth_token_t * auth_out)
{
    OSStatus status = errAuthorizationSuccess;
    require_action(auth_out != NULL, done, status = errAuthorizationInternal);
    
    size_t len;
    AuthorizationBlob * blob = (AuthorizationBlob *)xpc_dictionary_get_data(message, AUTH_XPC_BLOB, &len);
    require_action(blob != NULL, done, status = errAuthorizationInvalidRef);
    require_action(len == sizeof(AuthorizationBlob), done, status = errAuthorizationInvalidRef);
    
    auth_token_t auth = process_find_copy_auth_token(proc, blob);
    require_action(auth != NULL, done, status = errAuthorizationInvalidRef);
   
    *auth_out = auth;
    
done:
    return status;
}

static OSStatus _server_get_right_properties(connection_t conn, const char *rightName, CFDictionaryRef *properties)
{
    OSStatus status = errAuthorizationDenied;
    auth_token_t auth = NULL;
    engine_t engine = NULL;

    require_action(conn, done, status = errAuthorizationInternal);
    
    auth = auth_token_create(connection_get_process(conn), false);
    require_action(auth, done, status = errAuthorizationInternal);

    engine = engine_create(conn, auth);
    require_action(engine, done, status = errAuthorizationInternal);

    status = engine_get_right_properties(engine, rightName, properties);

done:
    CFReleaseSafe(engine);
    CFReleaseSafe(auth);
    return status;
}

OSStatus server_authorize(connection_t conn, auth_token_t auth, AuthorizationFlags flags, auth_rights_t rights, auth_items_t environment, engine_t * engine_out) {
    __block OSStatus status = errAuthorizationDenied;
    engine_t engine = NULL;
    
    require_action(conn, done, status = errAuthorizationInternal);

    engine = engine_create(conn, auth);
    require_action(engine, done, status = errAuthorizationInternal);
    
    if (flags & kAuthorizationFlagInteractionAllowed) {
        dispatch_sync(connection_get_dispatch_queue(conn), ^{
            connection_set_engine(conn, engine);
            status = engine_authorize(engine, rights, environment, flags);
            connection_set_engine(conn, NULL);
        });
    } else {
        status = engine_authorize(engine, rights, environment, flags);
    }
    
done:
    if (engine) {
        if (engine_out) {
            *engine_out = engine;
        } else {
            CFRelease(engine);
        }
    }
    return status;
}

// IN:  AUTH_XPC_RIGHTS, AUTH_XPC_ENVIRONMENT, AUTH_XPC_FLAGS
// OUT: AUTH_XPC_BLOB
OSStatus
authorization_create(connection_t conn, xpc_object_t message, xpc_object_t reply)
{
    OSStatus status = errAuthorizationDenied;
    
    process_t proc = connection_get_process(conn);
    
    // Passed in args
    auth_rights_t rights = auth_rights_create_with_xpc(xpc_dictionary_get_value(message, AUTH_XPC_RIGHTS));
    auth_items_t environment = auth_items_create_with_xpc(xpc_dictionary_get_value(message, AUTH_XPC_ENVIRONMENT));
    AuthorizationFlags flags = (AuthorizationFlags)xpc_dictionary_get_uint64(message, AUTH_XPC_FLAGS);
    
    // Create Authorization Token
    auth_token_t auth = auth_token_create(proc, flags & kAuthorizationFlagLeastPrivileged);
    require_action(auth != NULL, done, status = errAuthorizationInternal);
    
    if (!(flags & kAuthorizationFlagNoData)) {
        process_add_auth_token(proc,auth);
    }
    
    status = server_authorize(conn, auth, flags, rights, environment, NULL);
    require_noerr(status, done);
    
    //reply
    xpc_dictionary_set_data(reply, AUTH_XPC_BLOB, auth_token_get_blob(auth), sizeof(AuthorizationBlob));
    
done:
    CFReleaseSafe(rights);
    CFReleaseSafe(environment);
    CFReleaseSafe(auth);
    return status;
}

// IN:  AUTH_XPC_DATA, AUTH_XPC_ENVIRONMENT, AUTH_XPC_FLAGS
// OUT: AUTH_XPC_BLOB
OSStatus authorization_create_with_audit_token(connection_t conn, xpc_object_t message, xpc_object_t reply)
{
    OSStatus status = errAuthorizationDenied;
    auth_token_t auth = NULL;
    
    process_t proc = connection_get_process(conn);
    require(process_get_uid(proc) == 0, done);  //only root can use this call
    
    // Passed in args
    size_t len = 0;
    const char * data = xpc_dictionary_get_data(message, AUTH_XPC_DATA, &len);
    require(data != NULL, done);
    require(len == sizeof(audit_token_t), done);
    
//    auth_items_t environment = auth_items_create_with_xpc(xpc_dictionary_get_value(message, AUTH_XPC_ENVIRONMENT));
    AuthorizationFlags flags = (AuthorizationFlags)xpc_dictionary_get_uint64(message, AUTH_XPC_FLAGS);
    
    // Create Authorization Token
    auth = auth_token_create(proc, flags & kAuthorizationFlagLeastPrivileged);
    require_action(auth != NULL, done, status = errAuthorizationInternal);
    
    process_add_auth_token(proc, auth);
    
    //reply
    xpc_dictionary_set_data(reply, AUTH_XPC_BLOB, auth_token_get_blob(auth), sizeof(AuthorizationBlob));
    status = errAuthorizationSuccess;
    
done:
//    CFReleaseSafe(environment);
    CFReleaseSafe(auth);
    return status;
}

// IN:  AUTH_XPC_BLOB, AUTH_XPC_FLAGS
// OUT: 
OSStatus
authorization_free(connection_t conn, xpc_object_t message, xpc_object_t reply AUTH_UNUSED)
{
    OSStatus status = errAuthorizationSuccess;
    AuthorizationFlags flags = 0;
    process_t proc = connection_get_process(conn);
    
    auth_token_t auth = NULL;
    status = _process_find_copy_auth_token_from_xpc(proc, message, &auth);
    require_noerr(status, done);
    
    flags = (AuthorizationFlags)xpc_dictionary_get_uint64(message, AUTH_XPC_FLAGS);
    
    if (flags & kAuthorizationFlagDestroyRights) {
        auth_token_credentials_iterate(auth, ^bool(credential_t cred) {
            credential_invalidate(cred);
            os_log_debug(AUTHD_LOG, "engine[%i]: invalidating %{public}scredential %{public}s (%i)", connection_get_pid(conn), credential_get_shared(cred) ? "shared " : "", credential_get_name(cred), credential_get_uid(cred));
            return true;
        });
        
        session_credentials_purge(auth_token_get_session(auth));
    }
    
    process_remove_auth_token(proc, auth, flags);
    
done:
    CFReleaseSafe(auth);
    os_log_debug(AUTHD_LOG, "server: AuthorizationFree %d (flags:%x)", (int)status, (unsigned int)flags);
    return status;
}

// IN:  AUTH_XPC_BLOB, AUTH_XPC_DATA
// OUT:
OSStatus
authorization_copy_right_properties(connection_t conn, xpc_object_t message, xpc_object_t reply)
{
    OSStatus status = errAuthorizationDenied;
    CFDataRef serializedProperties = NULL;
    CFDictionaryRef properties = NULL;
    
    // Passed in args
    const char *right = xpc_dictionary_get_string(message, AUTH_XPC_RIGHT_NAME);
    os_log_debug(AUTHD_LOG, "server: right %s", right);

    require_action(right != NULL, done, status = errAuthorizationInvalidPointer);

    status = _server_get_right_properties(conn, right, &properties);
    require_noerr(status, done);
  
    if (properties) {
        serializedProperties = CFPropertyListCreateData(kCFAllocatorDefault, properties, kCFPropertyListBinaryFormat_v1_0, 0, NULL);
        if (serializedProperties) {
            xpc_dictionary_set_data(reply, AUTH_XPC_OUT_ITEMS, CFDataGetBytePtr(serializedProperties), CFDataGetLength(serializedProperties));
        }
    }
    
done:
    CFReleaseSafe(serializedProperties);
	CFReleaseSafe(properties);
	return status;
}

// IN:  AUTH_XPC_BLOB, AUTH_XPC_RIGHTS, AUTH_XPC_ENVIRONMENT, AUTH_XPC_FLAGS
// OUT: AUTH_XPC_OUT_ITEMS
OSStatus
authorization_copy_rights(connection_t conn, xpc_object_t message, xpc_object_t reply)
{
    OSStatus status = errAuthorizationDenied;
    engine_t engine = NULL;
    xpc_object_t outItems = NULL;
    
    process_t proc = connection_get_process(conn);
    
    // Passed in args
    auth_rights_t rights = auth_rights_create_with_xpc(xpc_dictionary_get_value(message, AUTH_XPC_RIGHTS));
    auth_items_t environment = auth_items_create_with_xpc(xpc_dictionary_get_value(message, AUTH_XPC_ENVIRONMENT));
    AuthorizationFlags flags = (AuthorizationFlags)xpc_dictionary_get_uint64(message, AUTH_XPC_FLAGS);
    
    auth_token_t auth = NULL;
    status = _process_find_copy_auth_token_from_xpc(proc, message, &auth);
    require_noerr_action_quiet(status, done, os_log_error(AUTHD_LOG, "copy_rights: no auth token"));
    
    status = server_authorize(conn, auth, flags, rights, environment, &engine);
	require_noerr_action_quiet(status, done, os_log_error(AUTHD_LOG, "copy_rights: authorization failed"));

	//reply
    outItems = auth_rights_export_xpc(engine_get_granted_rights(engine));
    xpc_dictionary_set_value(reply, AUTH_XPC_OUT_ITEMS, outItems);

done:
    CFReleaseSafe(rights);
    CFReleaseSafe(environment);
    CFReleaseSafe(auth);
    CFReleaseSafe(engine);
    
    return status;
}

// IN:  AUTH_XPC_BLOB, AUTH_XPC_TAG
// OUT: AUTH_XPC_OUT_ITEMS
OSStatus
authorization_copy_info(connection_t conn, xpc_object_t message, xpc_object_t reply)
{
    
    OSStatus status = errAuthorizationSuccess;
    auth_items_t items = NULL;
	auth_items_t local_items = NULL;
    const char * tag = NULL;
    xpc_object_t outItems = NULL;
    process_t proc = connection_get_process(conn);
    
    auth_token_t auth = NULL;
    status = _process_find_copy_auth_token_from_xpc(proc, message, &auth);
	require_noerr_action_quiet(status, done, os_log_error(AUTHD_LOG, "copy_info: no auth token"));

    items = auth_items_create();
    
    tag = xpc_dictionary_get_string(message, AUTH_XPC_TAG);
    os_log_debug(AUTHD_LOG, "server: requested tag: %{public}s", tag ? tag : "(all)");
	if (tag) {
		size_t len;
		const void * data = auth_items_get_data_with_flags(auth_token_get_context(auth), tag, &len, kAuthorizationContextFlagExtractable);
		if (data) {
			os_log_debug(AUTHD_LOG, "server: requested tag found");
			auth_items_set_data(items, tag, data, len);
		}
	} else {
		auth_items_copy_with_flags(items, auth_token_get_context(auth), kAuthorizationContextFlagExtractable);
	}

	local_items = auth_items_create();
	auth_items_content_copy(local_items, items); // we do not want decrypt content of the authorizationref memory which is where pointers point to
	auth_items_decrypt(local_items, auth_token_get_encryption_key(auth));

#if DEBUG
    os_log_debug(AUTHD_LOG, "server: Dumping requested AuthRef items: %{public}@", items);
#endif

    if (auth_items_exist(local_items, kAuthorizationEnvironmentPassword)) {
        // check if caller is entitled to get the password
        CFTypeRef extract_password_entitlement = process_copy_entitlement_value(proc, "com.apple.authorization.extract-password");
        if (extract_password_entitlement && (CFGetTypeID(extract_password_entitlement) == CFBooleanGetTypeID()) && extract_password_entitlement == kCFBooleanTrue) {
            os_log_debug(AUTHD_LOG, "server: caller allowed to extract password");
        } else {
            os_log_error(AUTHD_LOG, "server: caller NOT allowed to extract password");
            auth_items_remove(local_items, kAuthorizationEnvironmentPassword);
        }
        CFReleaseSafe(extract_password_entitlement);
    }

    //reply
    outItems = auth_items_export_xpc(local_items);
    xpc_dictionary_set_value(reply, AUTH_XPC_OUT_ITEMS, outItems);
   
done:
    CFReleaseSafe(local_items);
	CFReleaseSafe(items);
    CFReleaseSafe(auth);
    os_log_debug(AUTHD_LOG, "server: AuthorizationCopyInfo %i", (int) status);
    return status;
}

// IN:  AUTH_XPC_BLOB
// OUT: AUTH_XPC_EXTERNAL
OSStatus
authorization_make_external_form(connection_t conn, xpc_object_t message, xpc_object_t reply)
{
    OSStatus status = errAuthorizationSuccess;

    process_t proc = connection_get_process(conn);
    
    auth_token_t auth = NULL;
    status = _process_find_copy_auth_token_from_xpc(proc, message, &auth);
    require_noerr(status, done);
    
    AuthorizationExternalForm exForm;
    AuthorizationExternalBlob * exBlob = (AuthorizationExternalBlob *)&exForm;
    memset(&exForm, 0, sizeof(exForm));
    
    exBlob->blob = *auth_token_get_blob(auth);
    exBlob->session = process_get_session_id(proc);
    
    xpc_dictionary_set_data(reply, AUTH_XPC_EXTERNAL, &exForm, sizeof(exForm));
    server_register_auth_token(auth);
    
done:
    CFReleaseSafe(auth);
    os_log_debug(AUTHD_LOG, "server: AuthorizationMakeExternalForm %d", (int)status);
    return status;
}

// IN:  AUTH_XPC_EXTERNAL
// OUT: AUTH_XPC_BLOB
OSStatus
authorization_create_from_external_form(connection_t conn, xpc_object_t message, xpc_object_t reply)
{
    OSStatus status = errAuthorizationSuccess;
    auth_token_t auth = NULL;
    
    process_t proc = connection_get_process(conn);
    
    size_t len;
    AuthorizationExternalForm * exForm = (AuthorizationExternalForm *)xpc_dictionary_get_data(message, AUTH_XPC_EXTERNAL, &len);
    require_action(exForm != NULL, done, status = errAuthorizationInternal);
    require_action(len == sizeof(AuthorizationExternalForm), done, status = errAuthorizationInvalidRef);
    
    AuthorizationExternalBlob * exBlob = (AuthorizationExternalBlob *)exForm;
    auth = server_find_copy_auth_token(&exBlob->blob);
    require_action(auth != NULL, done, status = errAuthorizationDenied);
    
    process_add_auth_token(proc, auth);
    xpc_dictionary_set_data(reply, AUTH_XPC_BLOB, auth_token_get_blob(auth), sizeof(AuthorizationBlob));
    
done:
    CFReleaseSafe(auth);
    os_log_debug(AUTHD_LOG, "server: AuthorizationCreateFromExternalForm %d", (int)status);
    return status;
}

// IN:  AUTH_XPC_RIGHT_NAME
// OUT: AUTH_XPC_DATA
OSStatus
authorization_right_get(connection_t conn AUTH_UNUSED, xpc_object_t message, xpc_object_t reply)
{
    OSStatus status = errAuthorizationDenied;
    rule_t rule = NULL;
    CFTypeRef cfdict = NULL;
    xpc_object_t xpcdict = NULL;
    
    authdb_connection_t dbconn = authdb_connection_acquire(server_get_database());
    rule = rule_create_with_string(xpc_dictionary_get_string(message, AUTH_XPC_RIGHT_NAME), dbconn);
    require(rule != NULL, done);
    require(rule_get_id(rule) != 0, done);
    
    cfdict = rule_copy_to_cfobject(rule, dbconn);
    require(cfdict != NULL, done);
    
    xpcdict = _CFXPCCreateXPCObjectFromCFObject(cfdict);
    require(xpcdict != NULL, done);
    
    // reply
    xpc_dictionary_set_value(reply, AUTH_XPC_DATA, xpcdict);

    status = errAuthorizationSuccess;

done:
    authdb_connection_release(&dbconn);
    CFReleaseSafe(cfdict);
    CFReleaseSafe(rule);
    os_log_debug(AUTHD_LOG, "server: AuthorizationRightGet %d", (int)status);
    return status;
}

static bool _prompt_for_modifications(process_t __unused proc, rule_t __unused rule)
{
//    <rdar://problem/13853228> will put back it back at some later date
//    SecRequirementRef ruleReq = rule_get_requirement(rule);
//
//    if (ruleReq && process_verify_requirment(proc, ruleReq)) {
//        return false;
//    }
  
    // do not prompt if first party and has the entitlement
    CFTypeRef modifyEntitlement = process_copy_entitlement_value(proc, "com.apple.private.authorization.modify.all");
    if (modifyEntitlement && (CFGetTypeID(modifyEntitlement) == CFBooleanGetTypeID()) && modifyEntitlement == kCFBooleanTrue) {
        os_log_debug(AUTHD_LOG, "server: caller allowed to modify all");
        CFReleaseSafe(modifyEntitlement);
        return false;
    }
    CFReleaseSafe(modifyEntitlement);
    
    return true;
}

static int64_t _process_get_identifier_count(process_t proc, authdb_connection_t conn)
{
    __block int64_t result = 0;
    
    authdb_step(conn, "SELECT COUNT(*) AS cnt FROM rules WHERE identifier = ? ", ^(sqlite3_stmt *stmt) {
        sqlite3_bind_text(stmt, 1, process_get_identifier(proc), -1, NULL);
    }, ^bool(auth_items_t data) {
        result = auth_items_get_int64(data, "cnt");
        return true;
    });
    
    return result;
}

static int64_t _get_max_process_rights(void)
{
    static dispatch_once_t onceToken;
    static int64_t max_rights = MAX_PROCESS_RIGHTS;
    
    //sudo defaults write /Library/Preferences/com.apple.authd max_process_rights -bool true
    dispatch_once(&onceToken, ^{
		CFTypeRef max = (CFNumberRef)CFPreferencesCopyValue(CFSTR("max_process_rights"), CFSTR(SECURITY_AUTH_NAME), kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
        
        if (max && CFGetTypeID(max) == CFNumberGetTypeID()) {
            CFNumberGetValue(max, kCFNumberSInt64Type, &max_rights);
        }
        CFReleaseSafe(max);
    });
    
    return max_rights;
}

// IN:  AUTH_XPC_BLOB, AUTH_XPC_RIGHT_NAME, AUTH_XPC_DATA
// OUT: 
OSStatus
authorization_right_set(connection_t conn, xpc_object_t message, xpc_object_t reply AUTH_UNUSED)
{
    __block OSStatus status = errAuthorizationDenied;
    __block engine_t engine = NULL;
    CFStringRef cf_rule_name = NULL;
    CFDictionaryRef cf_rule_dict = NULL;
    rule_t rule = NULL;
    rule_t existingRule = NULL;
    authdb_connection_t dbconn = NULL;
    auth_token_t auth = NULL;
    bool force_modify = false;
    RuleType rule_type = RT_RIGHT;
    const char * rule_name = NULL;
    bool auth_rule = false;
    CFMutableDictionaryRef payload = NULL;
    NSString *rule_name_obj;
    
    NSMutableArray *protectedRights = @[
                                 @"com.apple.trust-settings.admin",
    ].mutableCopy;
    
    // sudo defaults write /Library/Preferences/com.apple.security.authorization protectedRights com.apple.right1,com.apple.right2
    CFTypeRef prefValue = CFPreferencesCopyAppValue(CFSTR("protectedRights"), CFSTR("com.apple.security.authorization"));
    if (prefValue) {
        if (CFGetTypeID(prefValue) == CFArrayGetTypeID()) {
            [protectedRights addObjectsFromArray:(__bridge NSArray *)prefValue];
            os_log_debug(AUTHD_LOG, "Added values from an array");
        } else if (CFGetTypeID(prefValue) == CFStringGetTypeID()) {
            NSArray *parts = [(__bridge NSString *)prefValue componentsSeparatedByString:@","];
            if (parts) {
                [protectedRights addObjectsFromArray:parts];
                os_log_debug(AUTHD_LOG, "Added values from a CSV string");
            } else {
                os_log_debug(AUTHD_LOG, "Unable to use protected values: type %lu", CFGetTypeID(prefValue));
            }
        }
        CFRelease(prefValue);
    }
    
    process_t proc = connection_get_process(conn);
    
    status = _process_find_copy_auth_token_from_xpc(proc, message, &auth);
    require_noerr(status, done);
    
    require_action(xpc_dictionary_get_string(message, AUTH_XPC_RIGHT_NAME) != NULL, done, status = errAuthorizationInternal);
    require_action(xpc_dictionary_get_value(message, AUTH_XPC_DATA) != NULL, done, status = errAuthorizationInternal);
    
    rule_name = xpc_dictionary_get_string(message, AUTH_XPC_RIGHT_NAME);
    require(rule_name != NULL, done);
    
    rule_name_obj = [NSString stringWithUTF8String:rule_name];
    require_action(rule_name_obj != NULL, done, status = errAuthorizationDenied; os_log_error(AUTHD_LOG, "server: AuthorizationRightSet unable to get rule name (denied)"));

    require_action([protectedRights containsObject:rule_name_obj] == NO, done, status = errAuthorizationDenied; os_log_error(AUTHD_LOG, "server: AuthorizationRightSet not allowed to update right %{public}s. (denied)", rule_name));

    if (_compare_string(rule_name, "authenticate")) {
        rule_type = RT_RULE;
        auth_rule = true;
    }
    
    cf_rule_name = CFStringCreateWithCString(kCFAllocatorDefault, rule_name, kCFStringEncodingUTF8);
    require(cf_rule_name != NULL, done);
    
    cf_rule_dict = _CFXPCCreateCFObjectFromXPCObject(xpc_dictionary_get_value(message, AUTH_XPC_DATA));
    require(cf_rule_dict != NULL, done);
    
    dbconn = authdb_connection_acquire(server_get_database());
    
    rule = rule_create_with_plist(rule_type, cf_rule_name, cf_rule_dict, dbconn);
    if (process_get_uid(proc) != 0) {
        require_action(rule_get_extract_password(rule) == false, done, status = errAuthorizationDenied; os_log_error(AUTHD_LOG, "server: AuthorizationRightSet not allowed to set extract-password. (denied)"));
    }
    
    // if rule doesn't currently exist then we have to check to see if they are over the Max.
    if (rule_get_id(rule) == 0) {
        if (process_get_identifier(proc) == NULL) {
            os_log_error(AUTHD_LOG, "server: AuthorizationRightSet required for process %{public}s (missing code signature). To add rights to the Authorization database, your process must have a code signature.", process_get_code_url(proc));
            force_modify = true;
        } else {
            int64_t process_rule_count = _process_get_identifier_count(proc, dbconn);
            if ((process_rule_count >= _get_max_process_rights())) {
                if (!connection_get_syslog_warn(conn)) {
                    os_log_error(AUTHD_LOG, "server: AuthorizationRightSet denied API abuse process %{public}s already contains %lli rights.", process_get_code_url(proc), _get_max_process_rights());
                    connection_set_syslog_warn(conn);
                }
                status = errAuthorizationDenied;
                goto done;
            }
        }
    } else {
        if (auth_rule) {
            if (process_get_uid(proc) != 0) {
                os_log_error(AUTHD_LOG, "server: AuthorizationRightSet denied, root required to update the 'authenticate' rule");
                status = errAuthorizationDenied;
                goto done;
            }
        } else {
            // verify they are updating a right and not a rule
            existingRule = rule_create_with_string(rule_get_name(rule), dbconn);
            if (rule_get_type(existingRule) == RT_RULE) {
                os_log_error(AUTHD_LOG, "server: AuthorizationRightSet denied updating '%{public}s' rule is prohibited", rule_get_name(existingRule));
                status = errAuthorizationDenied;
                goto done;
            }
        }
    }
    
    if (_prompt_for_modifications(proc,rule)) {
        authdb_connection_release(&dbconn);

        dispatch_sync(connection_get_dispatch_queue(conn), ^{
            engine = engine_create(conn, auth);
            connection_set_engine(conn, engine);
            status = engine_verify_modification(engine, rule, false, force_modify);
            connection_set_engine(conn, NULL);
        });
        require_noerr(status, done);
        
        dbconn = authdb_connection_acquire(server_get_database());
    }
    
    {
        payload = CFDictionaryCreateMutable(kCFAllocatorDefault, 5, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFStringRef cfRightName = CFStringCreateWithCString(kCFAllocatorDefault, rule_name, kCFStringEncodingUTF8);
        if (cfRightName) {
            CFDictionarySetValue(payload, CFSTR("rightName"), cfRightName);
            CFRelease(cfRightName);
        }
        
        char pathbuf[PROC_PIDPATHINFO_MAXSIZE];
        int ret = proc_pidpath(process_get_pid(proc), pathbuf, sizeof(pathbuf));
        if (ret > 0) {
            CFStringRef cfPath = CFStringCreateWithCString(kCFAllocatorDefault, pathbuf, kCFStringEncodingUTF8);
            if (cfPath) {
                CFDictionarySetValue(payload, CFSTR("caller"), cfPath);
                CFRelease(cfPath);
            }
        }
        CFDictionarySetValue(payload, CFSTR("processSigned"), process_firstparty_signed(proc) ? kCFBooleanTrue : kCFBooleanFalse);
        CFStringRef reason = NULL;
        bool processHasUiAccess = session_get_attributes(process_get_session(proc)) & AU_SESSION_FLAG_HAS_GRAPHIC_ACCESS;
        if (processHasUiAccess) {
            reason = CFSTR("TCC");
        } else {
            uid_t uid = agent_get_active_session_uid();
            if (uid == (uid_t)-1) {
                reason = CFSTR("N/A");
            } else {
                reason = CFSTR("SA");
            }
        }
        CFDictionarySetValue(payload, CFSTR("checkType"), reason != NULL ? reason : CFSTR("NONE"));
    }
    
    if (rule_sql_commit(rule, dbconn, engine ? engine_get_time(engine) : CFAbsoluteTimeGetCurrent(), proc)) {
        os_log_debug(AUTHD_LOG, "server: Successfully updated rule %{public}s", rule_get_name(rule));
        authdb_checkpoint(dbconn);
        status = errAuthorizationSuccess;
    } else {
        os_log_error(AUTHD_LOG, "server: Failed to update rule %{public}s", rule_get_name(rule));
        status = errAuthorizationDenied;
    }
    if (payload) {
        CFNumberRef temp = CFNumberCreate(NULL, kCFNumberSInt32Type, &status);
        if (temp) {
            CFDictionarySetValue(payload, CFSTR("rightSetResult"), temp);
            CFRelease(temp);
        }
    }

    if (payload) {
        analyticsSendEventLazy(CFSTR("com.apple.authd.tcc"), payload);
    }
    
done:
    authdb_connection_release(&dbconn);
    CFReleaseSafe(existingRule);
    CFReleaseSafe(cf_rule_name);
    CFReleaseSafe(cf_rule_dict);
    CFReleaseSafe(auth);
    CFReleaseSafe(rule);
    CFReleaseSafe(engine);
    CFReleaseSafe(payload);
    return status;
}

// IN:  AUTH_XPC_BLOB, AUTH_XPC_RIGHT_NAME
// OUT:
OSStatus
authorization_right_remove(connection_t conn, xpc_object_t message, xpc_object_t reply AUTH_UNUSED)
{
    __block OSStatus status = errAuthorizationDenied;
    __block engine_t engine = NULL;
    rule_t rule = NULL;
    authdb_connection_t dbconn = NULL;
    
    process_t proc = connection_get_process(conn);

    auth_token_t auth = NULL;
    status = _process_find_copy_auth_token_from_xpc(proc, message, &auth);
    require_noerr(status, done);
    
    dbconn = authdb_connection_acquire(server_get_database());
    
    rule = rule_create_with_string(xpc_dictionary_get_string(message, AUTH_XPC_RIGHT_NAME), dbconn);
    require(rule != NULL, done);
    
    if (_prompt_for_modifications(proc,rule)) {
        authdb_connection_release(&dbconn);
        
        dispatch_sync(connection_get_dispatch_queue(conn), ^{
            engine = engine_create(conn, auth);
            connection_set_engine(conn, engine);
            status = engine_verify_modification(engine, rule, true, false);
            connection_set_engine(conn, NULL);
        });
        require_noerr(status, done);
        
        dbconn = authdb_connection_acquire(server_get_database());
    }
    
    if (rule_get_id(rule) != 0) {
        rule_sql_remove(rule, dbconn, proc);
    }
    
done:
    authdb_connection_release(&dbconn);
    CFReleaseSafe(auth);
    CFReleaseSafe(rule);
    CFReleaseSafe(engine);
    os_log_debug(AUTHD_LOG, "server: AuthorizationRightRemove (PID %d): result %d", connection_get_pid(conn), (int)status);
    return status;
}

#pragma mark -
#pragma mark test code

OSStatus
session_set_user_preferences(connection_t conn, xpc_object_t message, xpc_object_t reply)
{
    (void)conn;
    (void)message;
    (void)reply;
    return errAuthorizationSuccess;
}

void
server_dev(void) {
//    rule_t rule = rule_create_with_string("system.preferences.accounts");
//    CFDictionaryRef dict = rule_copy_to_cfobject(rule);
//    _show_cf(dict);
//    CFReleaseSafe(rule);
//    CFReleaseSafe(dict);
    
//    auth_items_t config = NULL;
//    double d2 = 0, d1 = 5;
//    authdb_get_key_value(server_get_authdb_reader(), "config", &config);
//    auth_items_set_double(config, "test", d1);
//    d2 = auth_items_get_double(config, "test");
//    os_log_debug(AUTHD_LOG, "d1=%f d2=%f", d1, d2);
//    CFReleaseSafe(config);
    
    
//    auth_items_t items = auth_items_create();
//    auth_items_set_string(items, "test", "testing 1");
//    auth_items_set_string(items, "test2", "testing 2");
//    auth_items_set_string(items, "test3", "testing 3");
//    auth_items_set_flags(items, "test3", 4);
//    auth_items_set_string(items, "apple", "apple");
//    auth_items_set_flags(items, "apple", 1);
//    auth_items_set_int(items, "int", 45);
//    auth_items_set_flags(items, "int", 2);
//    auth_items_set_bool(items, "true", true);
//    auth_items_set_bool(items, "false", false);
//    auth_items_set(items, "com.apple.");
//    auth_show(items);
//    LOGD("Yeah it works: %s", auth_items_get_string(items, "test3"));
//    LOGD("Yeah it works: %i", auth_items_get_bool(items, "true"));
//    LOGD("Yeah it works: %i", auth_items_get_bool(items, "false"));
//    LOGD("Yeah it works: %i", auth_items_get_int(items, "int"));
//    (void)auth_items_get_bool(items, "test3");
//    AuthorizationItemSet * itemSet = auth_items_get_item_set(items);
//    for (uint32_t i = 0; i < itemSet->count; i++) {
//        LOGD("item: %s", itemSet->items[i].name);
//    }
//
//    xpc_object_t xpcdata = SerializeItemSet(auth_items_get_item_set(items));
//    auth_items_t items2 = auth_items_create_with_xpc(xpcdata);
//    xpc_release(xpcdata);
//    auth_items_remove_with_flags(items2, 7);
////    auth_items_set_string(items2, "test3", "testing 3 very good");
//    auth_items_copy_with_flags(items2, items, 7);
//    LOGD("Yeah it works: %s", auth_items_get_string(items2, "test3"));
//    auth_show(items2);
//    CFReleaseSafe(items2);
//    
//    CFReleaseSafe(items);
}

// IN:  AUTH_XPC_TAG, AUTH_XPC_FLAGS
// OUT: AUTH_XPC_DATA
OSStatus
authorization_copy_prelogin_userdb(connection_t conn, xpc_object_t message, xpc_object_t reply)
{
    OSStatus status = errAuthorizationDenied;
    xpc_object_t xpcarr = NULL;
    CFArrayRef cfarray = NULL;

    const char *uuid = xpc_dictionary_get_string(message, AUTH_XPC_TAG);
    UInt32 flags = (AuthorizationFlags)xpc_dictionary_get_uint64(message, AUTH_XPC_FLAGS);

    status = preloginudb_copy_userdb(uuid, flags, &cfarray);
    xpc_dictionary_set_int64(reply, AUTH_XPC_STATUS, status);
    require_noerr_action_quiet(status, done, os_log_error(AUTHD_LOG, "authorization_copy_prelogin_userdb: database failed %d", (int)status));
  
    xpcarr = _CFXPCCreateXPCObjectFromCFObject(cfarray);
    require(xpcarr != NULL, done);
    xpc_dictionary_set_value(reply, AUTH_XPC_DATA, xpcarr);

done:
    CFReleaseSafe(cfarray);

    return status;
}

// IN:  AUTH_XPC_RIGHT_NAME, AUTH_XPC_HINTS_NAME, AUTH_XPC_FLAGS
// OUT: AUTH_XPC_DATA
OSStatus
authorization_copy_prelogin_pref_value(connection_t conn, xpc_object_t message, xpc_object_t reply)
{
    OSStatus status = errAuthorizationDenied;
    xpc_object_t xpcoutput = NULL;
    const char *uuid = xpc_dictionary_get_string(message, AUTH_XPC_TAG);
    const char *user = xpc_dictionary_get_string(message, AUTH_XPC_RIGHT_NAME);
    const char *domain = xpc_dictionary_get_string(message, AUTH_XPC_HINTS_NAME);
    const char *item = xpc_dictionary_get_string(message, AUTH_XPC_ITEM_NAME);

    CFTypeRef output = NULL;
    status = prelogin_copy_pref_value(uuid, user, domain, item, &output);
    xpc_dictionary_set_int64(reply, AUTH_XPC_STATUS, status);
  
    xpcoutput = _CFXPCCreateXPCObjectFromCFObject(output);
    require(xpcoutput != NULL, done);
    xpc_dictionary_set_value(reply, AUTH_XPC_DATA, xpcoutput);

done:
    CFReleaseSafe(output);

    return status;
}

// IN:  AUTH_XPC_RIGHT_NAME, AUTH_XPC_ITEM_NAME
// OUT: AUTH_XPC_ITEM_VALUE
OSStatus
authorization_prelogin_smartcardonly_override(connection_t conn, xpc_object_t message, xpc_object_t reply)
{
    OSStatus status = errAuthorizationDenied;

    process_t proc = connection_get_process(conn);
    
    uint64 operation = xpc_dictionary_get_uint64(message, AUTH_XPC_ITEM_NAME);
    
    if (operation != kAuthorizationOverrideOperationQuery) {
        // check if caller is entitled to handle the override
        Boolean entitlementCheckPassed = false;
        CFTypeRef overrideEntitlement = process_copy_entitlement_value(proc, "com.apple.authorization.smartcard.override");
        if (overrideEntitlement && (CFGetTypeID(overrideEntitlement) == CFBooleanGetTypeID()) && overrideEntitlement == kCFBooleanTrue) {
            os_log_debug(AUTHD_LOG, "server: caller allowed to handle override");
            entitlementCheckPassed = true;
        } else {
            os_log_error(AUTHD_LOG, "server: caller NOT allowed to handle override");
        }
        CFReleaseSafe(overrideEntitlement);
        require(entitlementCheckPassed, done);
    }

    const char *uuid = xpc_dictionary_get_string(message, AUTH_XPC_TAG);
    Boolean result = false;
    status = prelogin_smartcardonly_override(uuid, operation, &result);
    xpc_dictionary_set_int64(reply, AUTH_XPC_STATUS, status);
    if (operation == kAuthorizationOverrideOperationQuery) {
        xpc_dictionary_set_bool(reply, AUTH_XPC_ITEM_VALUE, result);
    }

done:
    return status;
}
