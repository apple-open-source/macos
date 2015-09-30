/* Copyright (c) 2012-2013 Apple Inc. All Rights Reserved. */

#include "connection.h"
#include "process.h"
#include "authdb.h"
#include "engine.h"
#include "server.h"
#include "debugging.h"

struct _connection_s {
    __AUTH_BASE_STRUCT_HEADER__;
    
    process_t proc;
    engine_t engine;
    dispatch_queue_t dispatch_queue;
    dispatch_queue_t dispatch_queue_internal;
    bool sent_syslog_warn;
};

static void
_connection_finalize(CFTypeRef value)
{
    connection_t conn = (connection_t)value;
    
    process_remove_connection(conn->proc, conn);
    
    dispatch_barrier_sync(conn->dispatch_queue, ^{});
    dispatch_barrier_sync(conn->dispatch_queue_internal, ^{});
    
    CFReleaseSafe(conn->proc);
    CFReleaseNull(conn->engine);
    dispatch_release(conn->dispatch_queue);
    dispatch_release(conn->dispatch_queue_internal);
}

AUTH_TYPE_INSTANCE(connection,
                   .init = NULL,
                   .copy = NULL,
                   .finalize = _connection_finalize,
                   .equal = NULL,
                   .hash = NULL,
                   .copyFormattingDesc = NULL,
                   .copyDebugDesc = NULL
                   );

static CFTypeID connection_get_type_id() {
    static CFTypeID type_id = _kCFRuntimeNotATypeID;
    static dispatch_once_t onceToken;
    
    dispatch_once(&onceToken, ^{
        type_id = _CFRuntimeRegisterClass(&_auth_type_connection);
    });
    
    return type_id;
}

connection_t
connection_create(process_t proc)
{
    connection_t conn = NULL;
    
    conn = (connection_t)_CFRuntimeCreateInstance(kCFAllocatorDefault, connection_get_type_id(), AUTH_CLASS_SIZE(connection), NULL);
    require(conn != NULL, done);
    
    conn->proc = (process_t)CFRetain(proc);
    conn->dispatch_queue = dispatch_queue_create(NULL, DISPATCH_QUEUE_SERIAL);
    conn->dispatch_queue_internal = dispatch_queue_create(NULL, DISPATCH_QUEUE_SERIAL);
    
done:
    return conn;
}

pid_t connection_get_pid(connection_t conn)
{
    return process_get_pid(conn->proc);
}

process_t connection_get_process(connection_t conn)
{
    return conn->proc;
}

dispatch_queue_t connection_get_dispatch_queue(connection_t conn)
{
    return conn->dispatch_queue;
}

void connection_set_engine(connection_t conn, engine_t engine)
{
    dispatch_sync(conn->dispatch_queue_internal, ^{
        if (engine) {
            CFReleaseNull(conn->engine);
            conn->engine = (engine_t)CFRetain(engine);
        } else {
            CFReleaseNull(conn->engine);
        }
    });
}

void connection_destroy_agents(connection_t conn)
{
    dispatch_sync(conn->dispatch_queue_internal, ^{
        if (conn->engine) {
            engine_destroy_agents(conn->engine);
        }
    });
}

bool connection_get_syslog_warn(connection_t conn)
{
    return conn->sent_syslog_warn;
}

void connection_set_syslog_warn(connection_t conn)
{
    conn->sent_syslog_warn = true;
}

