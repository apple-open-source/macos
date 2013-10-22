/* Copyright (c) 2012 Apple Inc. All rights reserved. */

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

#include <bsm/libbsm.h>
#include <Security/Authorization.h>
#include <Security/AuthorizationPriv.h>
#include <Security/AuthorizationTagsPriv.h>
#include <xpc/private.h>
#include <dispatch/dispatch.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFXPCBridge.h>
#include <IOKit/IOMessage.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>

#define MAX_PROCESS_RIGHTS   30

static CFMutableDictionaryRef gProcessMap = NULL;
static CFMutableDictionaryRef gSessionMap = NULL;
static CFMutableDictionaryRef gAuthTokenMap = NULL;
static authdb_t gDatabase = NULL;

static dispatch_queue_t power_queue;
static bool gInDarkWake = false;
static IOPMConnection gIOPMconn = NULL;
static bool gXPCTransaction = false;

static dispatch_queue_t
get_server_dispatch_queue()
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
    return crc;
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

void server_cleanup()
{
    CFRelease(gProcessMap);
    CFRelease(gSessionMap);
    CFRelease(gAuthTokenMap);
    
    IOPMConnectionSetDispatchQueue(gIOPMconn, NULL);
    IOPMConnectionRelease(gIOPMconn);
    
    dispatch_queue_t queue = get_server_dispatch_queue();
    if (queue) {
        dispatch_release(queue);
    }
    dispatch_release(power_queue);
}

static void _IOMPCallBack(void * param AUTH_UNUSED, IOPMConnection connection, IOPMConnectionMessageToken token, IOPMSystemPowerStateCapabilities capabilities)
{
    LOGV("server: IOMP powerstates %i", capabilities);
    if (capabilities & kIOPMSystemPowerStateCapabilityDisk)
        LOGV("server: disk");
    if (capabilities & kIOPMSystemPowerStateCapabilityNetwork)
        LOGV("server: net");
    if (capabilities & kIOPMSystemPowerStateCapabilityAudio)
        LOGV("server: audio");
    if (capabilities & kIOPMSystemPowerStateCapabilityVideo)
        LOGV("server: video");
    
    /* if cpu and no display -> in DarkWake */
    LOGD("server: DarkWake check current=%i==%i", (capabilities & (kIOPMSystemPowerStateCapabilityCPU|kIOPMSystemPowerStateCapabilityVideo)), kIOPMSystemPowerStateCapabilityCPU);
    if ((capabilities & (kIOPMSystemPowerStateCapabilityCPU|kIOPMSystemPowerStateCapabilityVideo)) == kIOPMSystemPowerStateCapabilityCPU) {
        LOGV("server: enter DarkWake");
        gInDarkWake = true;
    } else if (gInDarkWake) {
        LOGV("server: exit DarkWake");
        gInDarkWake = false;
    }
    
    (void)IOPMConnectionAcknowledgeEvent(connection, token);
    
    return;
}

static void
_setupDarkWake(void *ctx)
{
    IOReturn ret;
    
    IOPMConnectionCreate(CFSTR("IOPowerWatcher"),
                         kIOPMSystemPowerStateCapabilityDisk
                         | kIOPMSystemPowerStateCapabilityNetwork
                         | kIOPMSystemPowerStateCapabilityAudio
                         | kIOPMSystemPowerStateCapabilityVideo,
                         &gIOPMconn);

    ret = IOPMConnectionSetNotification(gIOPMconn, NULL, _IOMPCallBack);
    if (ret != kIOReturnSuccess)
        return;
    
    IOPMConnectionSetDispatchQueue(gIOPMconn, power_queue);
}

bool server_in_dark_wake()
{
    return gInDarkWake;
}

authdb_t server_get_database()
{
    return gDatabase;
}

static void _setupAuditSessionMonitor()
{
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        au_sdev_handle_t *dev = au_sdev_open(AU_SDEVF_ALLSESSIONS);
        int event;
        auditinfo_addr_t aia;
        
        if (NULL == dev) {
            LOGE("server: could not open %s %d", AUDIT_SDEV_PATH, errno);
            return;
        }
        
        for (;;) {
            if (0 != au_sdev_read_aia(dev, &event, &aia)) {
                LOGE("server: au_sdev_read_aia failed: %d", errno);
                continue;
            }
            LOGD("server: au_sdev_handle_t event=%i, session=%i", event, aia.ai_asid);
            if (event == AUE_SESSION_CLOSE) {
                dispatch_async(get_server_dispatch_queue(), ^{
                    LOGV("server: session %i destroyed", aia.ai_asid);
                    CFDictionaryRemoveValue(gSessionMap, &aia.ai_asid);
                });
            }
        }
        
    });
}

static void _setupSignalHandlers()
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
    LOGV("server: uid=%i, sid=%i", info.ai_auid, info.ai_asid);
    
    require_action(get_server_dispatch_queue() != NULL, done, status = errAuthorizationInternal);
    
    gProcessMap = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kProcessMapKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require_action(gProcessMap != NULL, done, status = errAuthorizationInternal);
    
    gSessionMap = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kSessionMapKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require_action(gSessionMap != NULL, done, status = errAuthorizationInternal);
    
    gAuthTokenMap = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kAuthTokenKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require_action(gAuthTokenMap != NULL, done, status = errAuthorizationInternal);
    
    gDatabase = authdb_create();
    require_action(gDatabase != NULL, done, status = errAuthorizationInternal);
    
    // check to see if we have an updates
    authdb_connection_t dbconn = authdb_connection_acquire(gDatabase);
    authdb_maintenance(dbconn);
    authdb_connection_release(&dbconn);
    
    power_queue = dispatch_queue_create("com.apple.security.auth.power", DISPATCH_QUEUE_SERIAL);
    check(power_queue != NULL);
    dispatch_async_f(power_queue, NULL, _setupDarkWake);

    _setupAuditSessionMonitor();
    _setupSignalHandlers();
    
done:
    return status;
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
    
    LOGV("server[%i]: registered connection (total=%li)", info.pid, conn_count);

done:
    CFReleaseSafe(session);
    CFReleaseSafe(proc);
    return conn;
}

void
server_unregister_connection(connection_t conn)
{
    if (conn != NULL) {
        process_t proc = connection_get_process(conn);
        
        dispatch_sync(get_server_dispatch_queue(), ^{
            CFIndex connectionCount = process_get_connection_count(proc);
            LOGV("server[%i]: unregistered connection (total=%li)", process_get_pid(proc), connectionCount);

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
}

void
server_register_auth_token(auth_token_t auth)
{
    if (auth != NULL) {
        dispatch_sync(get_server_dispatch_queue(), ^{
            LOGV("server: registering auth %p", auth);
            CFDictionarySetValue(gAuthTokenMap, auth_token_get_key(auth), auth);
            auth_token_set_state(auth, auth_token_state_registered);
        });
    }
}

void
server_unregister_auth_token(auth_token_t auth)
{
    if (auth != NULL) {
        AuthorizationBlob blob = *(AuthorizationBlob*)auth_token_get_key(auth);
        dispatch_async(get_server_dispatch_queue(), ^{
            LOGV("server: unregistering auth %p", auth);
            CFDictionaryRemoveValue(gAuthTokenMap, &blob);
        });
    }
}

auth_token_t
server_find_copy_auth_token(AuthorizationBlob * blob)
{
    __block auth_token_t auth = NULL;
    if (blob != NULL) {
        dispatch_sync(get_server_dispatch_queue(), ^{
            auth = (auth_token_t)CFDictionaryGetValue(gAuthTokenMap, blob);
            if (auth) {
                CFRetain(auth);
            }
        });
    }
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

#if DEBUG
    LOGV("server[%i]: authtoken lookup %#x%x %p", process_get_pid(proc), blob->data[1],blob->data[0], auth);
#else
    LOGV("server[%i]: authtoken lookup %p", process_get_pid(proc), auth);
#endif
    
    *auth_out = auth;
    
done:
    return status;
}

static OSStatus _server_authorize(connection_t conn, auth_token_t auth, AuthorizationFlags flags, auth_rights_t rights, auth_items_t enviroment, engine_t * engine_out)
{
    __block OSStatus status = errAuthorizationDenied;
    engine_t engine = NULL;
    
    require_action(conn, done, status = errAuthorizationInternal);

    engine = engine_create(conn, auth);
    require_action(engine, done, status = errAuthorizationInternal);
    
    if (flags & kAuthorizationFlagInteractionAllowed) {
        dispatch_sync(connection_get_dispatch_queue(conn), ^{
            connection_set_engine(conn, engine);
            status = engine_authorize(engine, rights, enviroment, flags);
            connection_set_engine(conn, NULL);
        });
    } else {
        status = engine_authorize(engine, rights, enviroment, flags);
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

// IN:  AUTH_XPC_RIGHTS, AUTH_XPC_ENVIROMENT, AUTH_XPC_FLAGS
// OUT: AUTH_XPC_BLOB
OSStatus
authorization_create(connection_t conn, xpc_object_t message, xpc_object_t reply)
{
    OSStatus status = errAuthorizationDenied;
    
    process_t proc = connection_get_process(conn);
    
    // Passed in args
    auth_rights_t rights = auth_rights_create_with_xpc(xpc_dictionary_get_value(message, AUTH_XPC_RIGHTS));
    auth_items_t enviroment = auth_items_create_with_xpc(xpc_dictionary_get_value(message, AUTH_XPC_ENVIROMENT));
    AuthorizationFlags flags = (AuthorizationFlags)xpc_dictionary_get_uint64(message, AUTH_XPC_FLAGS);
    
    // Create Authorization Token
    auth_token_t auth = auth_token_create(proc, flags & kAuthorizationFlagLeastPrivileged);
    require_action(auth != NULL, done, status = errAuthorizationInternal);
    
    if (!(flags & kAuthorizationFlagNoData)) {
        process_add_auth_token(proc,auth);
    }
    
    status = _server_authorize(conn, auth, flags, rights, enviroment, NULL);
    require_noerr(status, done);
    
    //reply
    xpc_dictionary_set_data(reply, AUTH_XPC_BLOB, auth_token_get_blob(auth), sizeof(AuthorizationBlob));
    
done:
    CFReleaseSafe(rights);
    CFReleaseSafe(enviroment);
    CFReleaseSafe(auth);
    return status;
}

// IN:  AUTH_XPC_DATA, AUTH_XPC_ENVIROMENT, AUTH_XPC_FLAGS
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
    
//    auth_items_t enviroment = auth_items_create_with_xpc(xpc_dictionary_get_value(message, AUTH_XPC_ENVIROMENT));
    AuthorizationFlags flags = (AuthorizationFlags)xpc_dictionary_get_uint64(message, AUTH_XPC_FLAGS);
    
    audit_info_s auditInfo;
    _server_parse_audit_token((audit_token_t*)data, &auditInfo);
    
    // Create Authorization Token
    auth = auth_token_create(proc, flags & kAuthorizationFlagLeastPrivileged);
    require_action(auth != NULL, done, status = errAuthorizationInternal);
    
    process_add_auth_token(proc,auth);
    
    //reply
    xpc_dictionary_set_data(reply, AUTH_XPC_BLOB, auth_token_get_blob(auth), sizeof(AuthorizationBlob));

done:
//    CFReleaseSafe(enviroment);
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
            LOGV("engine[%i]: invalidating %scredential %s (%i) from authorization (%p)", connection_get_pid(conn), credential_get_shared(cred) ? "shared " : "", credential_get_name(cred), credential_get_uid(cred), auth);
            return true;
        });
        
        session_credentials_purge(auth_token_get_session(auth));
    }
    
    process_remove_auth_token(proc, auth, flags);
    
done:
    CFReleaseSafe(auth);
    LOGV("server[%i]: AuthorizationFree %i (flags:%x)", connection_get_pid(conn), status, flags);
    return status;
}

// IN:  AUTH_XPC_BLOB, AUTH_XPC_RIGHTS, AUTH_XPC_ENVIROMENT, AUTH_XPC_FLAGS
// OUT: AUTH_XPC_OUT_ITEMS
OSStatus
authorization_copy_rights(connection_t conn, xpc_object_t message, xpc_object_t reply)
{
    OSStatus status = errAuthorizationDenied;
    engine_t engine = NULL;
    
    process_t proc = connection_get_process(conn);
    
    // Passed in args
    auth_rights_t rights = auth_rights_create_with_xpc(xpc_dictionary_get_value(message, AUTH_XPC_RIGHTS));
    auth_items_t enviroment = auth_items_create_with_xpc(xpc_dictionary_get_value(message, AUTH_XPC_ENVIROMENT));
    AuthorizationFlags flags = (AuthorizationFlags)xpc_dictionary_get_uint64(message, AUTH_XPC_FLAGS);
    
    auth_token_t auth = NULL;
    status = _process_find_copy_auth_token_from_xpc(proc, message, &auth);
    require_noerr(status, done);
    
    status = _server_authorize(conn, auth, flags, rights, enviroment, &engine);
    require_noerr(status, done);
    
    //reply
    xpc_object_t outItems = auth_rights_export_xpc(engine_get_granted_rights(engine));
    xpc_dictionary_set_value(reply, AUTH_XPC_OUT_ITEMS, outItems);
    xpc_release_safe(outItems);

done:
    CFReleaseSafe(rights);
    CFReleaseSafe(enviroment);
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
    const char * tag = NULL;
    
    process_t proc = connection_get_process(conn);
    
    auth_token_t auth = NULL;
    status = _process_find_copy_auth_token_from_xpc(proc, message, &auth);
    require_noerr(status, done);
    
    items = auth_items_create();
    
    tag = xpc_dictionary_get_string(message, AUTH_XPC_TAG);
    LOGV("server[%i]: requested tag: %s", connection_get_pid(conn), tag ? tag : "(all)");
    if (tag) {
        size_t len;
        const void * data = auth_items_get_data(auth_token_get_context(auth), tag, &len);
        if (data) {
            auth_items_set_data(items, tag, data, len);
        }
    } else {
        auth_items_copy(items, auth_token_get_context(auth));
    }

#if DEBUG
    LOGV("server[%i]: Dumping requested AuthRef items", connection_get_pid(conn));
    _show_cf(items);
#endif

    //reply
    xpc_object_t outItems = auth_items_export_xpc(items);
    xpc_dictionary_set_value(reply, AUTH_XPC_OUT_ITEMS, outItems);
    xpc_release_safe(outItems);
    
done:
    CFReleaseSafe(items);
    CFReleaseSafe(auth);
    LOGV("server[%i]: AuthorizationCopyInfo %i", connection_get_pid(conn), status);
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
    LOGV("server[%i]: AuthorizationMakeExternalForm %i", connection_get_pid(conn), status);
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
    LOGV("server[%i]: AuthorizationCreateFromExternalForm %i", connection_get_pid(conn), status);
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
    xpc_release_safe(xpcdict);
    CFReleaseSafe(rule);
    LOGV("server[%i]: AuthorizationRightGet %i", connection_get_pid(conn), status);
    return status;
}

static bool _prompt_for_modifications(process_t proc, rule_t rule)
{
//    <rdar://problem/13853228> will put back it back at some later date
//    SecRequirementRef ruleReq = rule_get_requirment(rule);
//
//    if (ruleReq && process_verify_requirment(proc, ruleReq)) {
//        return false;
//    }
    
    return true;
}

static CFIndex _get_mechanism_index(CFArrayRef mechanisms, CFStringRef m_name)
{
    CFIndex index = -1;
    require(mechanisms, done);

    CFIndex c = CFArrayGetCount(mechanisms);
    CFStringRef i_name = NULL;
    for (CFIndex i = 0; i < c; ++i)
    {
        i_name = CFArrayGetValueAtIndex(mechanisms, i);
        if (i_name && (CFGetTypeID(m_name) == CFStringGetTypeID())) {
            if (CFStringCompare(i_name, m_name, kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
                index = i;
                break;
            }
        }
    }

done:
    return index;
}

static bool _update_rule_mechanism(authdb_connection_t dbconn, const char * rule_name, CFStringRef mechanism_name, CFStringRef insert_after_name, bool remove)
{
    bool updated = false;
    rule_t rule = NULL;
    rule_t update_rule = NULL;
    CFMutableDictionaryRef cfdict = NULL;
    CFStringRef update_name = NULL;

    require(mechanism_name, done);

    rule = rule_create_with_string(rule_name, dbconn);
    require(rule_get_id(rule) != 0, done); // rule doesn't exist in the database

    cfdict = rule_copy_to_cfobject(rule, dbconn);
    require(cfdict != NULL, done);

    CFMutableArrayRef mechanisms = NULL;
    bool res = CFDictionaryGetValueIfPresent(cfdict, CFSTR(kAuthorizationRuleParameterMechanisms), (void*)&mechanisms);
    require(res == true, done);

    CFIndex index = -1;

    if (remove) {
        index = _get_mechanism_index(mechanisms, mechanism_name);
    } else {
        if (insert_after_name) {
            if ((index = _get_mechanism_index(mechanisms, insert_after_name)) != -1) {
                index++;
            } else {
                index = 0; // if we couldn't find the index add it to the begining
            }
        } else {
            index = 0;
        }
    }

    if (index != -1) {
        if(remove) {
            CFArrayRemoveValueAtIndex(mechanisms, index);
        } else {
            if (index < CFArrayGetCount(mechanisms)) {
                require_action(CFStringCompare(CFArrayGetValueAtIndex(mechanisms, index), mechanism_name, kCFCompareCaseInsensitive) != kCFCompareEqualTo, done, updated = true);
            }
            CFArrayInsertValueAtIndex(mechanisms, index, mechanism_name);
        }
        
        CFDictionarySetValue(cfdict, CFSTR(kAuthorizationRuleParameterMechanisms), mechanisms);

        // and write it back
        update_name = CFStringCreateWithCString(kCFAllocatorDefault, rule_name, kCFStringEncodingUTF8);
        require(update_name, done);
        update_rule = rule_create_with_plist(rule_get_type(rule), update_name, cfdict, dbconn);
        require(update_rule, done);
        
        require(rule_sql_commit(update_rule, dbconn, CFAbsoluteTimeGetCurrent(), NULL), done);
    }

    updated = true;

done:
    CFReleaseSafe(rule);
    CFReleaseSafe(update_rule);
    CFReleaseSafe(cfdict);
    CFReleaseSafe(update_name);
    return updated;
}

/// IN:  AUTH_XPC_BLOB, AUTH_XPC_INT64
// OUT:
OSStatus
authorization_enable_smartcard(connection_t conn, xpc_object_t message, xpc_object_t reply AUTH_UNUSED)
{
    const CFStringRef SMARTCARD_LINE = CFSTR("builtin:smartcard-sniffer,privileged");
    const CFStringRef BUILTIN_LINE = CFSTR("builtin:policy-banner");
    const char* SYSTEM_LOGIN_CONSOLE = "system.login.console";
    const char* AUTHENTICATE = "authenticate";

    __block OSStatus status = errAuthorizationSuccess;
    bool enable_smartcard = false;
    authdb_connection_t dbconn = NULL;
    auth_token_t auth = NULL;
    auth_rights_t checkRight = NULL;

    process_t proc = connection_get_process(conn);

    status = _process_find_copy_auth_token_from_xpc(proc, message, &auth);
    require_noerr(status, done);

    checkRight = auth_rights_create();
    auth_rights_add(checkRight, "config.modify.smartcard");
    status = _server_authorize(conn, auth, kAuthorizationFlagDefaults | kAuthorizationFlagInteractionAllowed | kAuthorizationFlagExtendRights, checkRight, NULL, NULL);
    require_noerr(status, done);

    enable_smartcard = xpc_dictionary_get_bool(message, AUTH_XPC_DATA);

    dbconn = authdb_connection_acquire(server_get_database());

    if (!_update_rule_mechanism(dbconn, SYSTEM_LOGIN_CONSOLE, SMARTCARD_LINE, BUILTIN_LINE, enable_smartcard ? false : true)) {
        status = errAuthorizationInternal;
        LOGE("server[%i]: smartcard: enable(%i) failed to update %s", connection_get_pid(conn), enable_smartcard, SYSTEM_LOGIN_CONSOLE);
    }
    if (!_update_rule_mechanism(dbconn, AUTHENTICATE, SMARTCARD_LINE, NULL, enable_smartcard ? false : true)) {
        status = errAuthorizationInternal;
        LOGE("server[%i]: smartcard: enable(%i) failed to update %s", connection_get_pid(conn), enable_smartcard, AUTHENTICATE);
    }

    authdb_checkpoint(dbconn);

done:
    authdb_connection_release(&dbconn);
    CFReleaseSafe(checkRight);
    CFReleaseSafe(auth);
    return status;
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

static int64_t _get_max_process_rights()
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

    process_t proc = connection_get_process(conn);

    status = _process_find_copy_auth_token_from_xpc(proc, message, &auth);
    require_noerr(status, done);
    
    require_action(xpc_dictionary_get_string(message, AUTH_XPC_RIGHT_NAME) != NULL, done, status = errAuthorizationInternal);
    require_action(xpc_dictionary_get_value(message, AUTH_XPC_DATA) != NULL, done, status = errAuthorizationInternal);

    rule_name = xpc_dictionary_get_string(message, AUTH_XPC_RIGHT_NAME);
    require(rule_name != NULL, done);

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
        require_action(rule_get_extract_password(rule) == false, done, status = errAuthorizationDenied; LOGE("server[%i]: AuthorizationRightSet not allowed to set extract-password. (denied)", connection_get_pid(conn)));
    }
    
    // if rule doesn't currently exist then we have to check to see if they are over the Max.
    if (rule_get_id(rule) == 0) {
        if (process_get_identifier(proc) == NULL) {
            LOGE("server[%i]: AuthorizationRightSet required for process %s (missing code signature). To add rights to the Authorization database, your process must have a code signature.", connection_get_pid(conn), process_get_code_url(proc));
            force_modify = true;
        } else {
            int64_t process_rule_count = _process_get_identifier_count(proc, dbconn);
            if ((process_rule_count >= _get_max_process_rights())) {
                if (!connection_get_syslog_warn(conn)) {
                    LOGE("server[%i]: AuthorizationRightSet Denied API abuse process %s already contains %lli rights.", connection_get_pid(conn), process_get_code_url(proc), _get_max_process_rights());
                    connection_set_syslog_warn(conn);
                }
                status = errAuthorizationDenied;
                goto done;
            }
        }
    } else {
        if (auth_rule) {
            if (process_get_uid(proc) != 0) {
                LOGE("server[%i]: AuthorizationRightSet denied, root required to update the 'authenticate' rule", connection_get_pid(conn));
                status = errAuthorizationDenied;
                goto done;
            }
        } else {
            // verify they are updating a right and not a rule
            existingRule = rule_create_with_string(rule_get_name(rule), dbconn);
            if (rule_get_type(existingRule) == RT_RULE) {
                LOGE("server[%i]: AuthorizationRightSet Denied updating '%s' rule is prohibited", connection_get_pid(conn), rule_get_name(existingRule));
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
    
    if (rule_sql_commit(rule, dbconn, engine ? engine_get_time(engine) : CFAbsoluteTimeGetCurrent(), proc)) {
        LOGV("server[%i]: Successfully updated rule %s", connection_get_pid(conn), rule_get_name(rule));
        authdb_checkpoint(dbconn);
        status = errAuthorizationSuccess;
    } else {
        LOGE("server[%i]: Failed to update rule %s", connection_get_pid(conn), rule_get_name(rule));
        status = errAuthorizationDenied;
    }

done:
    authdb_connection_release(&dbconn);
    CFReleaseSafe(existingRule);
    CFReleaseSafe(cf_rule_name);
    CFReleaseSafe(cf_rule_dict);
    CFReleaseSafe(auth);
    CFReleaseSafe(rule);
    CFReleaseSafe(engine);
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
        rule_sql_remove(rule, dbconn);
    }
    
done:
    authdb_connection_release(&dbconn);
    CFReleaseSafe(auth);
    CFReleaseSafe(rule);
    CFReleaseSafe(engine);
    LOGV("server[%i]: AuthorizationRightRemove %i", connection_get_pid(conn), status);
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
server_dev() {
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
//    LOGV("d1=%f d2=%f", d1, d2);
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

