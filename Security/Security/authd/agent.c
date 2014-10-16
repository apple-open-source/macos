/* Copyright (c) 2012-2013 Apple Inc. All Rights Reserved. */

#include "agent.h"
#include "authitems.h"
#include "authd_private.h"
#include "process.h"
#include "mechanism.h"
#include "authutilities.h"
#include "authtoken.h"
#include "session.h"
#include "debugging.h"
#include "engine.h"

#import <xpc/private.h>
#include <Security/AuthorizationPlugin.h>

#include <mach/mach.h>
#include <servers/bootstrap.h>
#include <bootstrap_priv.h>

#define SECURITYAGENT_BOOTSTRAP_NAME_BASE       "com.apple.security.agentMain"
#define SECURITYAGENT_STUB_BOOTSTRAP_NAME_BASE       "com.apple.security.agentStub"
//#define SECURITYAGENT_LOGINWINDOW_BOOTSTRAP_NAME_BASE       "com.apple.security.agent.login"
#define AUTHORIZATIONHOST_BOOTSTRAP_NAME_BASE   "com.apple.security.authhost"

#define UUID_INITIALIZER_FROM_SESSIONID(sessionid) \
{ 0,0,0,0, 0,0,0,0, 0,0,0,0, (unsigned char)((0xff000000 & (sessionid))>>24), (unsigned char)((0x00ff0000 & (sessionid))>>16), (unsigned char)((0x0000ff00 & (sessionid))>>8),  (unsigned char)((0x000000ff & (sessionid))) }

struct _agent_s {
    __AUTH_BASE_STRUCT_HEADER__;
    
    auth_items_t hints;
    auth_items_t context;
    mechanism_t mech;
    engine_t engine;
    PluginState pluginState;
    uint64_t status;
    
    xpc_connection_t agentConnection;
    xpc_connection_t agentStubConnection;
    dispatch_queue_t eventQueue;
    dispatch_queue_t actionQueue;
    
    pid_t agentPid;
};

static void
_agent_finalize(CFTypeRef value)
{
    agent_t agent = (agent_t)value;
    
    // If this ever becomes a concurrent queue, then this would need to be a barrier sync
    dispatch_sync(agent->eventQueue, ^{
        // Mark the agent as dead
        agent->pluginState = dead;
    });
                  
    // We're going away, which means no outside references exist. It's safe to dispose of the XPC connection.
    if (NULL != agent->agentConnection) {
        xpc_release(agent->agentConnection);
        agent->agentConnection = NULL;
    }
    
    if (NULL != agent->agentStubConnection) {
        xpc_release(agent->agentStubConnection);
        agent->agentStubConnection = NULL;
    }
    
    // Now that we've released any XPC connection that may (or may not) be present
    // it's safe to go ahead and free our memory. This is provided that all other
    // blocks that were added to the event queue before the axe came down on the
    // xpc connection have been executed.
    
    // If this ever becomes a concurrent queue, then this would need to be a barrier sync
    dispatch_sync(agent->eventQueue, ^{
        CFReleaseSafe(agent->hints);
        CFReleaseSafe(agent->context);
        CFReleaseSafe(agent->mech);
        CFReleaseSafe(agent->engine);
        dispatch_release(agent->actionQueue);
    });
    
    dispatch_release(agent->eventQueue);

    LOGD("agent[%i]: _agent_finalize called", agent->agentPid);
}

AUTH_TYPE_INSTANCE(agent,
                   .init = NULL,
                   .copy = NULL,
                   .finalize = _agent_finalize,
                   .equal = NULL,
                   .hash = NULL,
                   .copyFormattingDesc = NULL,
                   .copyDebugDesc = NULL
                   );

static CFTypeID agent_get_type_id() {
    static CFTypeID type_id = _kCFRuntimeNotATypeID;
    static dispatch_once_t onceToken;
    
    dispatch_once(&onceToken, ^{
        type_id = _CFRuntimeRegisterClass(&_auth_type_agent);
    });
    
    return type_id;
}

agent_t
agent_create(engine_t engine, mechanism_t mech, auth_token_t auth, process_t proc, bool firstMech)
{
    bool doSwitchAudit = false;
    bool doSwitchBootstrap = false;
    agent_t agent = NULL;
    
    agent = (agent_t)_CFRuntimeCreateInstance(kCFAllocatorDefault, agent_get_type_id(), AUTH_CLASS_SIZE(agent), NULL);
    require(agent != NULL, done);
    
    agent->mech = (mechanism_t)CFRetain(mech);
    agent->engine = (engine_t)CFRetain(engine);
    agent->hints = auth_items_create();
    agent->context = auth_items_create();
    agent->pluginState = init;
    agent->agentPid = process_get_pid(proc);
    agent->agentStubConnection = NULL;
    
    const audit_info_s *audit_info = auth_token_get_audit_info(auth);

    auditinfo_addr_t tempAddr;
    tempAddr.ai_asid = audit_info->asid;
    auditon(A_GETSINFO_ADDR, &tempAddr, sizeof(tempAddr));
    
    LOGV("agent[%i]: Stored auid %d fetched auid %d", agent->agentPid, audit_info->auid, tempAddr.ai_auid);
    uid_t auid = tempAddr.ai_auid;
    uuid_t sessionUUID = UUID_INITIALIZER_FROM_SESSIONID((uint32_t)audit_info->asid);

    agent->eventQueue = dispatch_queue_create("Agent Event Queue", 0);
    agent->actionQueue = dispatch_queue_create("Agent Action Queue", 0);
    
    mach_port_t bootstrapPort = process_get_bootstrap(proc);
    if (!mechanism_is_privileged(mech)) {
        if ((int32_t)auid != -1) {
            agent->agentStubConnection = xpc_connection_create_mach_service(SECURITYAGENT_STUB_BOOTSTRAP_NAME_BASE, NULL, 0);
            xpc_connection_set_target_uid(agent->agentStubConnection, auid);
            LOGV("agent[%i]: Creating a security agent stub", agent->agentPid);
            xpc_connection_set_event_handler(agent->agentStubConnection, ^(xpc_object_t object){}); // Yes, this is a dummy handler, we never ever care about any responses from the stub. It can die in a fire for all I care.
            xpc_connection_resume(agent->agentStubConnection);
            
            xpc_object_t wakeupMessage = xpc_dictionary_create(NULL, NULL, 0);
            xpc_dictionary_set_data(wakeupMessage, AUTH_XPC_SESSION_UUID, sessionUUID, sizeof(uuid_t));
            xpc_object_t responseMessage = xpc_connection_send_message_with_reply_sync(agent->agentStubConnection, wakeupMessage);
            if (xpc_get_type(responseMessage) == XPC_TYPE_DICTIONARY) {
                LOGV("agent[%i]: Valid response received from stub", agent->agentPid);
            } else {
                LOGV("agent[%i]: Error response received from stub", agent->agentPid);
            }
            xpc_release(wakeupMessage);
            xpc_release(responseMessage);
            
            mach_port_t newBootstrapPort = auth_token_get_creator_bootstrap(auth);
            if (newBootstrapPort != MACH_PORT_NULL) {
                bootstrapPort = newBootstrapPort;
            }
        }
        
        agent->agentConnection = xpc_connection_create_mach_service(SECURITYAGENT_BOOTSTRAP_NAME_BASE, NULL,0);
        xpc_connection_set_instance(agent->agentConnection, sessionUUID);
        LOGV("agent[%i]: Creating a security agent", agent->agentPid);
        doSwitchAudit = true;
        doSwitchBootstrap = true;
    } else {
        agent->agentConnection = xpc_connection_create_mach_service(AUTHORIZATIONHOST_BOOTSTRAP_NAME_BASE, NULL, 0);
        xpc_connection_set_instance(agent->agentConnection, sessionUUID);
        LOGV("agent[%i]: Creating a standard authhost", agent->agentPid);
        doSwitchAudit = true;
        doSwitchBootstrap = true;
    }
    
    // **************** Here's the event handler, since I can never find it
    xpc_connection_set_target_queue(agent->agentConnection, agent->eventQueue);
    xpc_connection_set_event_handler(agent->agentConnection, ^(xpc_object_t object) {
        char* objectDesc = xpc_copy_description(object);
        LOGV("agent[%i]: global xpc message received %s", agent->agentPid, objectDesc);
        free(objectDesc);
        
        if (agent->pluginState == dead) {
            // If the agent is dead for some reason, drop this message before we hurt ourselves.
            return;
        }
        
        if (xpc_get_type(object) == XPC_TYPE_DICTIONARY) {
            const char *replyType = xpc_dictionary_get_string(object, AUTH_XPC_REPLY_METHOD_KEY);
            if (0 == strcmp(replyType, AUTH_XPC_REPLY_METHOD_INTERRUPT)) {
                agent->pluginState = interrupting;
                engine_interrupt_agent(agent->engine);
            }
        }
    });
    
    xpc_connection_resume(agent->agentConnection);
    
    xpc_object_t requestObject = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_string(requestObject, AUTH_XPC_REQUEST_METHOD_KEY, AUTH_XPC_REQUEST_METHOD_CREATE);
    xpc_dictionary_set_string(requestObject, AUTH_XPC_PLUGIN_NAME, mechanism_get_plugin(mech));
    xpc_dictionary_set_string(requestObject, AUTH_XPC_MECHANISM_NAME, mechanism_get_param(mech));
	if (doSwitchAudit) {
        mach_port_name_t jobPort;
        if (audit_info != NULL) {
            if (0 == audit_session_port(audit_info->asid, &jobPort)) {
                LOGV("agent[%i]: attaching an audit session port", agent->agentPid);
                xpc_dictionary_set_mach_send(requestObject, AUTH_XPC_AUDIT_SESSION_PORT, jobPort);
                
                if (mach_port_mod_refs(mach_task_self(), jobPort, MACH_PORT_RIGHT_SEND, -1) != KERN_SUCCESS) {
                    LOGE("unable to release send right for audit session, leaking");
                }
            }
        }
    }

	if (doSwitchBootstrap) {
        LOGV("agent[%i]: attaching a bootstrap port", agent->agentPid);
        xpc_dictionary_set_mach_send(requestObject, AUTH_XPC_BOOTSTRAP_PORT, bootstrapPort);
    }
    
    // This loop will be repeated until we can get ahold of a SecurityAgent, or get a fatal error
    // This will cause us to retry any CONNECTION_INTERRUPTED status messages
    do {
        xpc_object_t object = xpc_connection_send_message_with_reply_sync(agent->agentConnection, requestObject);
        
        if (xpc_get_type(object) == XPC_TYPE_DICTIONARY) {
            const char *replyType = xpc_dictionary_get_string(object, AUTH_XPC_REPLY_METHOD_KEY);
            if (0 == strcmp(replyType, AUTH_XPC_REPLY_METHOD_CREATE)) {
                agent->status = xpc_dictionary_get_uint64(object, AUTH_XPC_REPLY_RESULT_VALUE);
                if (agent->status == kAuthorizationResultAllow) {
                    // Only go here if we have an "allow" from the SecurityAgent (the plugin create might have failed)
                    // This is backwards compatible so that it goes gently into the build, as the default is "allow".
                    agent->pluginState = created;
                } else {
                    agent->pluginState = dead;
                }
            }
        } else if (xpc_get_type(object) == XPC_TYPE_ERROR) {
            if (object == XPC_ERROR_CONNECTION_INVALID) {
                agent->pluginState = dead;
            }
        }
        xpc_release(object);
    } while ((agent->pluginState == init) && (firstMech));
    
    xpc_release(requestObject);

    if (agent->pluginState != created) {
        CFRelease(agent);
        agent = NULL;
    }
done:
    return agent;
}

uint64_t
agent_run(agent_t agent, auth_items_t hints, auth_items_t context, auth_items_t immutable_hints)
{
    xpc_object_t hintsArray = auth_items_export_xpc(hints);
    xpc_object_t contextArray = auth_items_export_xpc(context);
    xpc_object_t immutableHintsArray = auth_items_export_xpc(immutable_hints);
    
    dispatch_semaphore_t replyWaiter = dispatch_semaphore_create(0);
    dispatch_sync(agent->actionQueue, ^{
        if (agent->pluginState != mechinterrupting) {
            agent->pluginState = current;
            agent->status = kAuthorizationResultUndefined; // Set this while the plugin chews on the user input.
            
            auth_items_clear(agent->hints);
            auth_items_clear(agent->context);
            
            xpc_object_t requestObject = xpc_dictionary_create(NULL, NULL, 0);
            xpc_dictionary_set_string(requestObject, AUTH_XPC_REQUEST_METHOD_KEY, AUTH_XPC_REQUEST_METHOD_INVOKE);
            xpc_dictionary_set_value(requestObject, AUTH_XPC_HINTS_NAME, hintsArray);
            xpc_dictionary_set_value(requestObject, AUTH_XPC_CONTEXT_NAME, contextArray);
            xpc_dictionary_set_value(requestObject, AUTH_XPC_IMMUTABLE_HINTS_NAME, immutableHintsArray);
            
            xpc_connection_send_message_with_reply(agent->agentConnection, requestObject, agent->actionQueue, ^(xpc_object_t object){
                if (xpc_get_type(object) == XPC_TYPE_DICTIONARY) {
                    const char *replyType = xpc_dictionary_get_string(object, AUTH_XPC_REPLY_METHOD_KEY);
                    if (0 == strcmp(replyType, AUTH_XPC_REPLY_METHOD_RESULT)) {
                        if (agent->pluginState == current) {
                            xpc_object_t xpcContext = xpc_dictionary_get_value(object, AUTH_XPC_CONTEXT_NAME);
                            xpc_object_t xpcHints = xpc_dictionary_get_value(object, AUTH_XPC_HINTS_NAME);
                            auth_items_copy_xpc(agent->context, xpcContext);
                            auth_items_copy_xpc(agent->hints, xpcHints);
                            agent->status = xpc_dictionary_get_uint64(object, AUTH_XPC_REPLY_RESULT_VALUE);
                            agent->pluginState = active;
                        }
                    }
                } else if (xpc_get_type(object) == XPC_TYPE_ERROR) {
                    if ((object == XPC_ERROR_CONNECTION_INVALID) || (object == XPC_ERROR_CONNECTION_INTERRUPTED)) {
                        agent->pluginState = dead;
                    }
                }
                dispatch_semaphore_signal(replyWaiter);
            });
            xpc_release(requestObject);
        }
    });

    if (agent->pluginState == current) {
        dispatch_semaphore_wait(replyWaiter, DISPATCH_TIME_FOREVER);
    }
    dispatch_release(replyWaiter);
    
    LOGV("agent[%i]: Finished call to SecurityAgent", agent->agentPid);
    
    xpc_release(hintsArray);
    xpc_release(contextArray);
    xpc_release(immutableHintsArray);
    
    return agent->status;
}

auth_items_t
agent_get_hints(agent_t agent)
{
    return agent->hints;
}

auth_items_t
agent_get_context(agent_t agent)
{
    return agent->context;
}

mechanism_t
agent_get_mechanism(agent_t agent)
{
    return agent->mech;
}

void
agent_deactivate(agent_t agent)
{
    xpc_object_t requestObject = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_string(requestObject, AUTH_XPC_REQUEST_METHOD_KEY, AUTH_XPC_REQUEST_METHOD_DEACTIVATE);

    agent->pluginState = deactivating;

    xpc_object_t object = xpc_connection_send_message_with_reply_sync(agent->agentConnection, requestObject);
    if (xpc_get_type(object) == XPC_TYPE_DICTIONARY) {
        const char *replyType = xpc_dictionary_get_string(object, AUTH_XPC_REPLY_METHOD_KEY);
        if (0 == strcmp(replyType, AUTH_XPC_REPLY_METHOD_DEACTIVATE)) {
            agent->pluginState = active; // This is strange, but true. We do actually want to set the state 'active'.
        }
    } else if (xpc_get_type(object) == XPC_TYPE_ERROR) {
        if ((object == XPC_ERROR_CONNECTION_INVALID) || (object == XPC_ERROR_CONNECTION_INTERRUPTED)) {
            agent->pluginState = dead;
        }
    }
    xpc_release(object);
    xpc_release(requestObject);
}

void agent_destroy(agent_t agent)
{
    xpc_object_t requestObject = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_string(requestObject, AUTH_XPC_REQUEST_METHOD_KEY, AUTH_XPC_REQUEST_METHOD_DESTROY);
    xpc_connection_send_message(agent->agentConnection, requestObject);
    xpc_release(requestObject);
}

PluginState
agent_get_state(agent_t agent)
{
    return agent->pluginState;
}

void
agent_notify_interrupt(agent_t agent)
{
    dispatch_sync(agent->actionQueue, ^{
        if (agent->pluginState == current) {
            xpc_object_t requestObject = xpc_dictionary_create(NULL, NULL, 0);
            xpc_dictionary_set_string(requestObject, AUTH_XPC_REQUEST_METHOD_KEY, AUTH_XPC_REQUEST_METHOD_INTERRUPT);
            xpc_connection_send_message(agent->agentConnection, requestObject);
            xpc_release(requestObject);
        } else if (agent->pluginState == active) {
            agent->pluginState = mechinterrupting;
        }
    });
}

void
agent_clear_interrupt(agent_t agent)
{
    dispatch_sync(agent->actionQueue, ^{
        if (agent->pluginState == mechinterrupting) {
            agent->pluginState = active;
        }
    });
}

void
agent_recieve(agent_t agent)
{
}
