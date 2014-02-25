/* Copyright (c) 2012 Apple Inc. All rights reserved. */

#include "engine.h"
#include "rule.h"
#include "authitems.h"
#include "authtoken.h"
#include "agent.h"
#include "process.h"
#include "debugging.h"
#include "server.h"
#include "credential.h"
#include "session.h"
#include "mechanism.h"
#include "authutilities.h"
#include "ccaudit.h"
#include "connection.h"

#include <pwd.h>
#include <Security/checkpw.h>
int checkpw_internal( const struct passwd *pw, const char* password );

#include <Security/AuthorizationTags.h>
#include <Security/AuthorizationTagsPriv.h>
#include <Security/AuthorizationPlugin.h>
#include <sandbox.h>

static void _set_process_hints(auth_items_t, process_t);
static void _set_process_immutable_hints(auth_items_t, process_t);
static void _set_auth_token_hints(auth_items_t, auth_token_t);
static OSStatus _evaluate_user_credential_for_rule(engine_t, credential_t, rule_t, bool, bool, enum Reason *);
static void _engine_set_credential(engine_t, credential_t, bool);
static OSStatus _evaluate_rule(engine_t, rule_t);

enum {
    kEngineHintsFlagTemporary = (1 << 30)
};

#pragma mark -
#pragma mark engine creation

struct _engine_s {
    __AUTH_BASE_STRUCT_HEADER__;
    
    connection_t conn;
    process_t proc;
    auth_token_t auth;
    
    AuthorizationFlags flags;
    auth_items_t hints;
    auth_items_t context;
    auth_items_t sticky_context;
    auth_items_t immutable_hints;
    
    auth_rights_t grantedRights;
    
    enum Reason reason;
    int32_t tries;
    
    CFAbsoluteTime now;
    
    credential_t sessionCredential;
    CFMutableSetRef credentials;
    CFMutableSetRef effectiveCredentials;
    
    CFMutableDictionaryRef mechanism_agents;
    
    // set only in engine_authorize
    const char * currentRightName; // weak ref
    rule_t currentRule; // weak ref
    
    rule_t authenticateRule;
    
    bool dismissed;
};

static void
_engine_finalizer(CFTypeRef value)
{
    engine_t engine = (engine_t)value;
    
    CFReleaseSafe(engine->mechanism_agents);
    CFReleaseSafe(engine->conn);
    CFReleaseSafe(engine->auth);
    CFReleaseSafe(engine->hints);
    CFReleaseSafe(engine->context);
    CFReleaseSafe(engine->immutable_hints);
    CFReleaseSafe(engine->sticky_context);
    CFReleaseSafe(engine->grantedRights);
    CFReleaseSafe(engine->sessionCredential);
    CFReleaseSafe(engine->credentials);
    CFReleaseSafe(engine->effectiveCredentials);
    CFReleaseSafe(engine->authenticateRule);
}

AUTH_TYPE_INSTANCE(engine,
                   .init = NULL,
                   .copy = NULL,
                   .finalize = _engine_finalizer,
                   .equal = NULL,
                   .hash = NULL,
                   .copyFormattingDesc = NULL,
                   .copyDebugDesc = NULL
                   );

static CFTypeID engine_get_type_id() {
    static CFTypeID type_id = _kCFRuntimeNotATypeID;
    static dispatch_once_t onceToken;
    
    dispatch_once(&onceToken, ^{
        type_id = _CFRuntimeRegisterClass(&_auth_type_engine);
    });
    
    return type_id;
}

engine_t
engine_create(connection_t conn, auth_token_t auth)
{
    engine_t engine = NULL;
    require(conn != NULL, done);
    require(auth != NULL, done);

    engine = (engine_t)_CFRuntimeCreateInstance(kCFAllocatorDefault, engine_get_type_id(), AUTH_CLASS_SIZE(engine), NULL);
    require(engine != NULL, done);
    
    engine->conn = (connection_t)CFRetain(conn);
    engine->proc = connection_get_process(conn);
    engine->auth = (auth_token_t)CFRetain(auth);
    
    engine->hints = auth_items_create();
    engine->context = auth_items_create();
    engine->immutable_hints = auth_items_create();
    engine->sticky_context = auth_items_create();
    _set_process_hints(engine->hints, engine->proc);
    _set_process_immutable_hints(engine->immutable_hints, engine->proc);
    _set_auth_token_hints(engine->hints, auth);
    
    engine->grantedRights = auth_rights_create();
    
    engine->reason = noReason;
    
    engine->now = CFAbsoluteTimeGetCurrent();
    
    session_update(auth_token_get_session(engine->auth));
    engine->sessionCredential = credential_create(session_get_uid(auth_token_get_session(engine->auth)));
    engine->credentials = CFSetCreateMutable(kCFAllocatorDefault, 0, &kCFTypeSetCallBacks);
    engine->effectiveCredentials = CFSetCreateMutable(kCFAllocatorDefault, 0, &kCFTypeSetCallBacks);
    
    session_credentials_iterate(auth_token_get_session(engine->auth), ^bool(credential_t cred) {
        CFSetAddValue(engine->effectiveCredentials, cred);
        return true;
    });
    
    auth_token_credentials_iterate(engine->auth, ^bool(credential_t cred) {
        // we added all session credentials already now just add all previously acquired credentials
        if (!credential_get_shared(cred)) {
            CFSetAddValue(engine->credentials, cred);
        }
        return true;
    });
    
    engine->mechanism_agents = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    
done:
    return engine;
}

#pragma mark -
#pragma mark agent hints

void
_set_process_hints(auth_items_t hints, process_t proc)
{
    // process information
    RequestorType type = bundle;
    auth_items_set_data(hints, AGENT_HINT_CLIENT_TYPE, &type, sizeof(type));
    auth_items_set_int(hints, AGENT_HINT_CLIENT_PID, process_get_pid(proc));
    auth_items_set_uint(hints, AGENT_HINT_CLIENT_UID, process_get_uid(proc));
}

void
_set_process_immutable_hints(auth_items_t immutable_hints, process_t proc)
{
    // process information - immutable
    auth_items_set_bool(immutable_hints, AGENT_HINT_PROCESS_SIGNED, process_apple_signed(proc));
}

void
_set_auth_token_hints(auth_items_t hints, auth_token_t auth)
{
    auth_items_set_string(hints, AGENT_HINT_CLIENT_PATH, auth_token_get_code_url(auth));
    auth_items_set_int(hints, AGENT_HINT_CREATOR_PID, auth_token_get_pid(auth));
    const audit_info_s * info = auth_token_get_audit_info(auth);
    auth_items_set_data(hints, AGENT_HINT_CREATOR_AUDIT_TOKEN, &info->opaqueToken, sizeof(info->opaqueToken));
}

static void
_set_right_hints(auth_items_t hints, const char * right)
{
   auth_items_set_string(hints, AGENT_HINT_AUTHORIZE_RIGHT, right);
}

static void
_set_rule_hints(auth_items_t hints, rule_t rule)
{
    auth_items_set_string(hints, AGENT_HINT_AUTHORIZE_RULE, rule_get_name(rule));
    const char * group = rule_get_group(rule);
    if (rule_get_class(rule) == RC_USER && group != NULL) {
        auth_items_set_string(hints, AGENT_HINT_REQUIRE_USER_IN_GROUP, group);
    } else {
        auth_items_remove(hints, AGENT_HINT_REQUIRE_USER_IN_GROUP);
    }
}

static void
_set_localization_hints(authdb_connection_t dbconn, auth_items_t hints, rule_t rule)
{
    char * key = calloc(1u, 128);

    authdb_step(dbconn, "SELECT lang,value FROM prompts WHERE r_id = ?", ^(sqlite3_stmt *stmt) {
        sqlite3_bind_int64(stmt, 1, rule_get_id(rule));
    }, ^bool(auth_items_t data) {
        snprintf(key, 128, "%s%s", kAuthorizationRuleParameterDescription, auth_items_get_string(data, "lang"));
        auth_items_set_string(hints, key, auth_items_get_string(data, "value"));
        auth_items_set_flags(hints, key, kEngineHintsFlagTemporary);
        return true;
    });

    authdb_step(dbconn, "SELECT lang,value FROM buttons WHERE r_id = ?", ^(sqlite3_stmt *stmt) {
        sqlite3_bind_int64(stmt, 1, rule_get_id(rule));
    }, ^bool(auth_items_t data) {
        snprintf(key, 128, "%s%s", kAuthorizationRuleParameterButton, auth_items_get_string(data, "lang"));
        auth_items_set_string(hints, key, auth_items_get_string(data, "value"));
        auth_items_set_flags(hints, key, kEngineHintsFlagTemporary);
        return true;
    });

    free_safe(key);
}

static void
_set_session_hints(engine_t engine, rule_t rule)
{
    LOGV("engine[%i]: ** prepare agent hints for rule %s", connection_get_pid(engine->conn), rule_get_name(rule));
    if (_evaluate_user_credential_for_rule(engine, engine->sessionCredential, rule, true, true, NULL) == errAuthorizationSuccess) {
        const char * tmp = credential_get_name(engine->sessionCredential);
        if (tmp != NULL) {
            auth_items_set_string(engine->hints, AGENT_HINT_SUGGESTED_USER, tmp);
        }
        tmp = credential_get_realname(engine->sessionCredential);
        if (tmp != NULL) {
            auth_items_set_string(engine->hints, AGENT_HINT_SUGGESTED_USER_LONG, tmp);
        }
    } else {
        auth_items_remove(engine->hints, AGENT_HINT_SUGGESTED_USER);
        auth_items_remove(engine->hints, AGENT_HINT_SUGGESTED_USER_LONG);
    }
}

#pragma mark -
#pragma mark right processing

static OSStatus
_evaluate_credential_for_rule(engine_t engine, credential_t cred, rule_t rule, bool ignoreShared, bool sessionOwner, enum Reason * reason)
{
    if (auth_token_least_privileged(engine->auth)) {
        if (credential_is_right(cred) && credential_get_valid(cred) && _compare_string(engine->currentRightName, credential_get_name(cred))) {
            if (!ignoreShared) {
                if (!rule_get_shared(rule) && credential_get_shared(cred)) {
                    LOGV("engine[%i]: - shared right %s (does NOT satisfy rule)", connection_get_pid(engine->conn), credential_get_name(cred));
                    if (reason) {  *reason = unknownReason; }
                    return errAuthorizationDenied;
                }
            }
            
            return errAuthorizationSuccess;
        } else {
            if (reason) {  *reason = unknownReason; }
            return errAuthorizationDenied;
        }
    } else {
        return _evaluate_user_credential_for_rule(engine,cred,rule,ignoreShared,sessionOwner, reason);
    }
}

static OSStatus
_evaluate_user_credential_for_rule(engine_t engine, credential_t cred, rule_t rule, bool ignoreShared, bool sessionOwner, enum Reason * reason)
{
    const char * cred_label = sessionOwner ? "session owner" : "credential";
    LOGV("engine[%i]: - validating %s%s %s (%i) for %s", connection_get_pid(engine->conn),
         credential_get_shared(cred) ? "shared " : "",
         cred_label,
         credential_get_name(cred),
         credential_get_uid(cred),
         rule_get_name(rule));
    
    if (rule_get_class(rule) != RC_USER) {
        LOGV("engine[%i]: - invalid rule class %i (denied)", connection_get_pid(engine->conn), rule_get_class(rule));
        return errAuthorizationDenied;
    }

    if (credential_get_valid(cred) != true) {
        LOGV("engine[%i]: - %s %i invalid (does NOT satisfy rule)", connection_get_pid(engine->conn), cred_label, credential_get_uid(cred));
        if (reason) {  *reason = invalidPassphrase; }
        return errAuthorizationDenied;
    }

    if (engine->now - credential_get_creation_time(cred) > rule_get_timeout(rule)) {
        LOGV("engine[%i]: - %s %i expired '%f > %lli' (does NOT satisfy rule)", connection_get_pid(engine->conn), cred_label, credential_get_uid(cred),
             (engine->now - credential_get_creation_time(cred)), rule_get_timeout(rule));
        if (reason) {  *reason = unknownReason; }
        return errAuthorizationDenied;
    }

    
    if (!ignoreShared) {
        if (!rule_get_shared(rule) && credential_get_shared(cred)) {
            LOGV("engine[%i]: - shared %s %i (does NOT satisfy rule)", connection_get_pid(engine->conn), cred_label, credential_get_uid(cred));
            if (reason) {  *reason = unknownReason; }
            return errAuthorizationDenied;
        }
    }
    
    if (credential_get_uid(cred) == 0) {
        LOGV("engine[%i]: - %s %i has uid 0 (does satisfy rule)", connection_get_pid(engine->conn), cred_label, credential_get_uid(cred));
        return errAuthorizationSuccess;
    }
    
    if (rule_get_session_owner(rule)) {
        if (credential_get_uid(cred) == session_get_uid(auth_token_get_session(engine->auth))) {
            LOGV("engine[%i]: - %s %i is session owner (does satisfy rule)", connection_get_pid(engine->conn), cred_label, credential_get_uid(cred));
            return errAuthorizationSuccess;
        }
    }
    
    if (rule_get_group(rule) != NULL) {
        do
        {
            // This allows testing a group modifier without prompting the user
            // When (authenticate-user = false) we are just testing the creator uid.
            // If a group modifier is enabled (RuleFlagEntitledAndGroup | RuleFlagVPNEntitledAndGroup)
            // we want to skip the creator uid group check.
            // group modifiers are checked early during the evaluation in _check_entitlement_for_rule 
            if (!rule_get_authenticate_user(rule)) {
                if (rule_check_flags(rule, RuleFlagEntitledAndGroup | RuleFlagVPNEntitledAndGroup)) {
                    break;
                }
            }
            
            if (credential_check_membership(cred, rule_get_group(rule))) {
                LOGV("engine[%i]: - %s %i is member of group %s (does satisfy rule)", connection_get_pid(engine->conn), cred_label, credential_get_uid(cred), rule_get_group(rule));
                return errAuthorizationSuccess;
            } else {
                if (reason) {  *reason = userNotInGroup; }
            }
        } while (0);
    } else if (rule_get_session_owner(rule)) { // rule asks only if user is the session owner
        if (reason) {  *reason = unacceptableUser; }
    }

    LOGV("engine[%i]: - %s %i (does NOT satisfy rule)", connection_get_pid(engine->conn), cred_label, credential_get_uid(cred));
    return errAuthorizationDenied;
}

static agent_t
_get_agent(engine_t engine, mechanism_t mech, bool create, bool firstMech)
{
    agent_t agent = (agent_t)CFDictionaryGetValue(engine->mechanism_agents, mech);
    if (create && !agent) {
        agent = agent_create(engine, mech, engine->auth, engine->proc, firstMech);
        if (agent) {
            CFDictionaryAddValue(engine->mechanism_agents, mech, agent);
            CFReleaseSafe(agent);
        }
    }
    return agent;
}

static uint64_t
_evaluate_builtin_mechanism(engine_t engine, mechanism_t mech)
{
    uint64_t result = kAuthorizationResultDeny;

    switch (mechanism_get_type(mech)) {
        case kMechanismTypeEntitled:
            if (auth_token_has_entitlement_for_right(engine->auth, engine->currentRightName)) {
                result = kAuthorizationResultAllow;
            }
        break;
        default:
            break;
    }
    
    return result;
}


static OSStatus
_evaluate_mechanisms(engine_t engine, CFArrayRef mechanisms)
{
    uint64_t result = kAuthorizationResultAllow;
    ccaudit_t ccaudit = ccaudit_create(engine->proc, engine->auth, AUE_ssauthmech);
    auth_items_t context = auth_items_create();
    auth_items_t hints = auth_items_create();
    
    auth_items_copy(context, engine->context);
    auth_items_copy(hints, engine->hints);
    auth_items_copy(context, engine->sticky_context);
    
    CFIndex count = CFArrayGetCount(mechanisms);
    for (CFIndex i = 0; i < count; i++) {
        mechanism_t mech = (mechanism_t)CFArrayGetValueAtIndex(mechanisms, i);
        
        if (mechanism_get_type(mech)) {
            LOGV("engine[%i]: running builtin mechanism %s (%li of %li)", connection_get_pid(engine->conn), mechanism_get_string(mech), i+1, count);
            result = _evaluate_builtin_mechanism(engine, mech);
        } else {
            agent_t agent = _get_agent(engine, mech, true, i == 0);
            require_action(agent != NULL, done, result = kAuthorizationResultUndefined; LOGE("engine[%i]: error creating mechanism agent", connection_get_pid(engine->conn)));
            LOGV("engine[%i]: running mechanism %s (%li of %li)", connection_get_pid(engine->conn), mechanism_get_string(agent_get_mechanism(agent)), i+1, count);
            result = agent_run(agent, hints, context, engine->immutable_hints);
            
            auth_items_copy(context, agent_get_context(agent));
            auth_items_copy(hints, agent_get_hints(agent));
            
            bool interrupted = false;
            for (CFIndex i2 = 0; i2 != i; i2++) {
                agent_t agent2 = _get_agent(engine, (mechanism_t)CFArrayGetValueAtIndex(mechanisms, i2), false, i == 0);
                if (agent_get_state(agent2) == interrupting) {
                    agent_deactivate(agent);
                    interrupted = true;
                    i = i2 - 1;
                    char * buf = NULL;
                    asprintf(&buf, "evaluation interrupted by %s; restarting evaluation there", mechanism_get_string(agent_get_mechanism(agent2)));
                    ccaudit_log_mechanism(ccaudit, engine->currentRightName, mechanism_get_string(agent_get_mechanism(agent2)), kAuthorizationResultAllow, buf);
                    free_safe(buf);
                    auth_items_copy(context, agent_get_context(agent2));
                    auth_items_copy(hints, agent_get_hints(agent2));
                    break;
                }
            }

            // Empty token name means that token doesn't exist (e.g. SC was removed).
            // Remove empty token name from hints for UI drawing logic.
            const char * token_name = auth_items_get_string(hints, AGENT_HINT_TOKEN_NAME);
            if (token_name && strlen(token_name) == 0) {
                auth_items_remove(hints, AGENT_HINT_TOKEN_NAME);
            }
            
            if (interrupted) {
                LOGV("engine[%i]: mechanisms interrupted", connection_get_pid(engine->conn));
                enum Reason reason = worldChanged;
                auth_items_set_data(hints, AGENT_HINT_RETRY_REASON, &reason, sizeof(reason));
                result = kAuthorizationResultAllow;
                _cf_dictionary_iterate(engine->mechanism_agents, ^bool(CFTypeRef key __attribute__((__unused__)), CFTypeRef value) {
                    agent_t tempagent = (agent_t)value;
                    agent_clear_interrupt(tempagent);
                    return true;
                });
            }
        }
        
        if (result == kAuthorizationResultAllow) {
            ccaudit_log_mechanism(ccaudit, engine->currentRightName, mechanism_get_string(mech), kAuthorizationResultAllow, NULL);
        } else {
            ccaudit_log_mechanism(ccaudit, engine->currentRightName, mechanism_get_string(mech), (uint32_t)result, NULL);
            break;
        }
    }

done:
    if ((result == kAuthorizationResultUserCanceled) || (result == kAuthorizationResultAllow)) {
        // only make non-sticky context values available externally
        auth_items_set_flags(context, kAuthorizationEnvironmentPassword, kAuthorizationContextFlagVolatile);
        auth_items_copy_with_flags(engine->context, context, kAuthorizationContextFlagExtractable | kAuthorizationContextFlagVolatile);
    } else if (result == kAuthorizationResultDeny) {
        auth_items_clear(engine->sticky_context);
        // save off sticky values in context
        auth_items_copy_with_flags(engine->sticky_context, context, kAuthorizationContextFlagSticky);
    }
    
    CFReleaseSafe(ccaudit);
    CFReleaseSafe(context);
    CFReleaseSafe(hints);
    
    switch(result)
    {
        case kAuthorizationResultDeny:
            return errAuthorizationDenied;
        case kAuthorizationResultUserCanceled:
            return errAuthorizationCanceled;
        case kAuthorizationResultAllow:
            return errAuthorizationSuccess;
        case kAuthorizationResultUndefined:
            return errAuthorizationInternal;
        default:
        {
            LOGV("engine[%i]: unexpected error result", connection_get_pid(engine->conn));
            return errAuthorizationInternal;
        }
    }
}

static OSStatus
_evaluate_authentication(engine_t engine, rule_t rule)
{
    OSStatus status = errAuthorizationDenied;
    ccaudit_t ccaudit = ccaudit_create(engine->proc, engine->auth, AUE_ssauthint);
    LOGV("engine[%i]: evaluate authentication", connection_get_pid(engine->conn));
    _set_rule_hints(engine->hints, rule);
    _set_session_hints(engine, rule);

    CFArrayRef mechanisms = rule_get_mechanisms(rule);
    if (!(CFArrayGetCount(mechanisms) > 0)) {
        mechanisms = rule_get_mechanisms(engine->authenticateRule);
    }
    require_action(CFArrayGetCount(mechanisms) > 0, done, LOGV("engine[%i]: error no mechanisms found", connection_get_pid(engine->conn)));
    
    int64_t ruleTries = rule_get_tries(rule);
    for (engine->tries = 0; engine->tries < ruleTries; engine->tries++) {
        
        auth_items_set_data(engine->hints, AGENT_HINT_RETRY_REASON, &engine->reason, sizeof(engine->reason));
        auth_items_set_int(engine->hints, AGENT_HINT_TRIES, engine->tries);
        
        status = _evaluate_mechanisms(engine, mechanisms);
        
        LOGV("engine[%i]: evaluate mechanisms result %i", connection_get_pid(engine->conn), status);
        
        // successfully ran mechanisms to obtain credential
        if (status == errAuthorizationSuccess) {
            // deny is the default
            status = errAuthorizationDenied;
            
            credential_t newCred = NULL;
            if (auth_items_exist(engine->context, "uid")) {
                newCred = credential_create(auth_items_get_uint(engine->context, "uid"));
            } else {
                LOGV("engine[%i]: mechanism failed to return a valid uid", connection_get_pid(engine->conn));
            }
            
            if (newCred) {
                if (credential_get_valid(newCred)) {
                    LOG("UID %u authenticated as user %s (UID %u) for right '%s'", auth_token_get_uid(engine->auth), credential_get_name(newCred), credential_get_uid(newCred), engine->currentRightName);
                    ccaudit_log_success(ccaudit, newCred, engine->currentRightName);
                } else {
                    LOG("UID %u failed to authenticate as user '%s' for right '%s'", auth_token_get_uid(engine->auth), auth_items_get_string(engine->context, "username"), engine->currentRightName);
                    ccaudit_log_failure(ccaudit, auth_items_get_string(engine->context, "username"), engine->currentRightName);
                }
                
                status = _evaluate_user_credential_for_rule(engine, newCred, rule, true, false, &engine->reason);

                if (status == errAuthorizationSuccess) {
                    _engine_set_credential(engine, newCred, rule_get_shared(rule));
                    CFReleaseSafe(newCred);
                    
                    if (auth_token_least_privileged(engine->auth)) {
                        credential_t rightCred = credential_create_with_right(engine->currentRightName);
                        _engine_set_credential(engine, rightCred, rule_get_shared(rule));
                        CFReleaseSafe(rightCred);
                    }
                    
                    session_t session = auth_token_get_session(engine->auth);
                    if (credential_get_uid(newCred) == session_get_uid(session)) {
                        LOGV("engine[%i]: authenticated as the session owner", connection_get_pid(engine->conn));
                        session_set_attributes(auth_token_get_session(engine->auth), AU_SESSION_FLAG_HAS_AUTHENTICATED);
                    }

                    break;
                }

                CFReleaseSafe(newCred);
            }
            
        } else if (status == errAuthorizationCanceled || status == errAuthorizationInternal) {
            break;
        } else if (status == errAuthorizationDenied) {
            engine->reason = invalidPassphrase;
        }
    }
    
    if (engine->tries == ruleTries) {
        engine->reason = tooManyTries;
        auth_items_set_data(engine->hints, AGENT_HINT_RETRY_REASON, &engine->reason, sizeof(engine->reason));
        auth_items_set_int(engine->hints, AGENT_HINT_TRIES, engine->tries);
        _evaluate_mechanisms(engine, mechanisms);
        ccaudit_log(ccaudit, engine->currentRightName, NULL, 1113);
    }
    
done:
    CFReleaseSafe(ccaudit);
    
    return status;
}

static bool
_check_entitlement_for_rule(engine_t engine, rule_t rule)
{
    bool entitled = false;
    CFTypeRef value = NULL;
    
    if (rule_check_flags(rule, RuleFlagEntitledAndGroup)) {
        if (auth_token_has_entitlement_for_right(engine->auth, engine->currentRightName)) {
            if (credential_check_membership(auth_token_get_credential(engine->auth), rule_get_group(rule))) {
                LOGV("engine[%i]: creator of authorization has entitlement for right %s and is member of group '%s'", connection_get_pid(engine->conn), engine->currentRightName, rule_get_group(rule));
                entitled = true;
                goto done;
            }
        }
    }
    
    if (rule_check_flags(rule, RuleFlagVPNEntitledAndGroup)) {
        // com.apple.networking.vpn.configuration is an array we only check for it's existence
        value = auth_token_copy_entitlement_value(engine->auth, "com.apple.networking.vpn.configuration");
        if (value) {
            if (credential_check_membership(auth_token_get_credential(engine->auth), rule_get_group(rule))) {
                LOGV("engine[%i]: creator of authorization has VPN entitlement and is member of group '%s'", connection_get_pid(engine->conn), rule_get_group(rule));
                entitled = true;
                goto done;
            }
        }
    }
    
done:
    CFReleaseSafe(value);
    return entitled;
}

static OSStatus
_evaluate_class_user(engine_t engine, rule_t rule)
{
    __block OSStatus status = errAuthorizationDenied;
    
    if (_check_entitlement_for_rule(engine,rule)) {
        return errAuthorizationSuccess;
    }
    
    if (rule_get_allow_root(rule) && auth_token_get_uid(engine->auth) == 0) {
        LOGV("engine[%i]: creator of authorization has uid == 0 granting right %s", connection_get_pid(engine->conn), engine->currentRightName);
        return errAuthorizationSuccess;
    }
    
    if (!rule_get_authenticate_user(rule)) {
        status = _evaluate_user_credential_for_rule(engine, engine->sessionCredential, rule, true, true, NULL);
        
        if (status == errAuthorizationSuccess) {
            return errAuthorizationSuccess;
        }
        
        return errAuthorizationDenied;
    }
    
    // First -- check all the credentials we have either acquired or currently have
    _cf_set_iterate(engine->credentials, ^bool(CFTypeRef value) {
        credential_t cred = (credential_t)value;
        // Passed-in user credentials are allowed for least-privileged mode
        if (auth_token_least_privileged(engine->auth) && !credential_is_right(cred) && credential_get_valid(cred)) {
            status = _evaluate_user_credential_for_rule(engine, cred, rule, false, false, NULL);
            if (errAuthorizationSuccess == status) {
                credential_t rightCred = credential_create_with_right(engine->currentRightName);
                _engine_set_credential(engine,rightCred,rule_get_shared(rule));
                CFReleaseSafe(rightCred);
                return false; // exit loop
            }
        }
        
        status = _evaluate_credential_for_rule(engine, cred, rule, false, false, NULL);
        if (status == errAuthorizationSuccess) {
            return false; // exit loop
        }
        return true;
    });
    
    if (status == errAuthorizationSuccess) {
        return status;
    }

    // Second -- go through the credentials associated to the authorization token session/auth token
    _cf_set_iterate(engine->effectiveCredentials, ^bool(CFTypeRef value) {
        credential_t cred = (credential_t)value;
        status = _evaluate_credential_for_rule(engine, cred, rule, false, false, NULL);
        if (status == errAuthorizationSuccess) {
            // Add the credential we used to the output set.
            _engine_set_credential(engine, cred, false);
            return false; // exit loop
        }
        return true;
    });
    
    if (status == errAuthorizationSuccess) {
        return status;
    }
    
    // Finally - we didn't find a credential. Obtain a new credential if our flags let us do so.
    if (!(engine->flags & kAuthorizationFlagExtendRights)) {
        LOGV("engine[%i]: authorization denied (kAuthorizationFlagExtendRights not set)", connection_get_pid(engine->conn));
        return errAuthorizationDenied;
    }
    
    // authorization that timeout immediately cannot be preauthorized
    if (engine->flags & kAuthorizationFlagPreAuthorize && rule_get_timeout(rule) == 0) {
        return errAuthorizationSuccess;
    }
    
    if (!(engine->flags & kAuthorizationFlagInteractionAllowed)) {
		LOGV("engine[%i]: Interaction not allowed (kAuthorizationFlagInteractionAllowed not set)", connection_get_pid(engine->conn));
        return errAuthorizationInteractionNotAllowed;
    }
    
    if (!(session_get_attributes(auth_token_get_session(engine->auth)) & AU_SESSION_FLAG_HAS_GRAPHIC_ACCESS)) {
        LOGV("engine[%i]: Interaction not allowed (session has no ui access)", connection_get_pid(engine->conn));
        return errAuthorizationInteractionNotAllowed;
    }
    
    if (server_in_dark_wake()) {
        LOGV("engine[%i]: authorization denied (in DarkWake)", connection_get_pid(engine->conn));
        return errAuthorizationDenied;
    }
    
    return _evaluate_authentication(engine,rule);
}

static OSStatus
_evaluate_class_rule(engine_t engine, rule_t rule)
{
    __block OSStatus status = errAuthorizationDenied;
    int64_t kofn = rule_get_kofn(rule);

    uint32_t total = (uint32_t)rule_get_delegates_count(rule);
    __block uint32_t success_count = 0;
    __block uint32_t count = 0;
    LOGV("engine[%i]: ** rule %s has %zi delegates kofn = %lli", connection_get_pid(engine->conn), rule_get_name(rule), total, kofn);
    rule_delegates_iterator(rule, ^bool(rule_t delegate) {
        count++;
        
        if (kofn != 0 && success_count == kofn) {
            status = errAuthorizationSuccess;
            return false;
        }
        
        LOGV("engine[%i]: * evaluate rule %s (%i)", connection_get_pid(engine->conn), rule_get_name(delegate), count);
        status = _evaluate_rule(engine, delegate);
        
        // if status is cancel/internal error abort
		if ((status == errAuthorizationCanceled) || (status == errAuthorizationInternal))
			return false;
        
        if (status != errAuthorizationSuccess) {
            if (kofn != 0) {
                // if remaining is less then required abort
                if ((total - count) < (kofn - success_count)) {
                    LOGD("engine[%i]: rule evaluation remaining: %i, required: %lli", connection_get_pid(engine->conn), (total - count), (kofn - success_count));
                    return false;
                }
                return true;
            }
            return false;
        } else {
            success_count++;
            return true;
        }
    });
    
    return status;
}

static OSStatus
_evaluate_class_mechanism(engine_t engine, rule_t rule)
{
    OSStatus status = errAuthorizationDenied;
    CFArrayRef mechanisms = NULL;
    
    require_action(rule_get_mechanisms_count(rule) > 0, done, status = errAuthorizationSuccess; LOGV("engine[%i]: no mechanisms specified", connection_get_pid(engine->conn)));
    
    mechanisms = rule_get_mechanisms(rule);
    
    if (server_in_dark_wake()) {
        CFIndex count = CFArrayGetCount(mechanisms);
        for (CFIndex i = 0; i < count; i++) {
            if (!mechanism_is_privileged((mechanism_t)CFArrayGetValueAtIndex(mechanisms, i))) {
                LOGV("engine[%i]: authorization denied (in DarkWake)", connection_get_pid(engine->conn));
                goto done;
            }
        }
    }
    
    int64_t ruleTries = rule_get_tries(rule);
    engine->tries = 0;
    do {
        auth_items_set_data(engine->hints, AGENT_HINT_RETRY_REASON, &engine->reason, sizeof(engine->reason));
        auth_items_set_int(engine->hints, AGENT_HINT_TRIES, engine->tries);
        
        status = _evaluate_mechanisms(engine, mechanisms);
        LOGV("engine[%i]: evaluate mechanisms result %i", connection_get_pid(engine->conn), status);
        
        if ((status == errAuthorizationSuccess) || (status == errAuthorizationCanceled)) {
            
            if (status == errAuthorizationSuccess) {
                credential_t newCred = NULL;
                if (auth_items_exist(engine->context, "uid")) {
                    newCred = credential_create(auth_items_get_uint(engine->context, "uid"));
                } else {
                    LOGV("engine[%i]: mechanism did not return a uid", connection_get_pid(engine->conn));
                }

                if (newCred) {
                    _engine_set_credential(engine, newCred, rule_get_shared(rule));
                    
                    if (auth_token_least_privileged(engine->auth)) {
                        credential_t rightCred = credential_create_with_right(engine->currentRightName);
                        _engine_set_credential(engine, rightCred, rule_get_shared(rule));
                        CFReleaseSafe(rightCred);
                    }
                    
                    if (strcmp(engine->currentRightName, "system.login.console") == 0 && !auth_items_exist(engine->context, AGENT_CONTEXT_AUTO_LOGIN)) {
                        session_set_attributes(auth_token_get_session(engine->auth), AU_SESSION_FLAG_HAS_AUTHENTICATED);
                    }
                    
                    CFReleaseSafe(newCred);
                }
            } 
        } // mechanism errAuthorizationSuccess

        engine->tries++;
        
    } while ( (status == errAuthorizationDenied) // only if we have an expected faulure we continue
             && ((ruleTries == 0) || ((ruleTries > 0) && engine->tries < ruleTries))); // ruleTries == 0 means we try forever
                                                                                       // ruleTires > 0 means we try upto ruleTries times
done:
    return status;
}

static OSStatus
_evaluate_rule(engine_t engine, rule_t rule)
{
    if (rule_check_flags(rule, RuleFlagEntitled)) {
        if (auth_token_has_entitlement_for_right(engine->auth, engine->currentRightName)) {
            LOGV("engine[%i]: rule allow, creator of authorization has entitlement for right %s", connection_get_pid(engine->conn), engine->currentRightName);
            return errAuthorizationSuccess;
        }
    }
    
    if (rule_check_flags(rule, RuleFlagRequireAppleSigned)) {
        if (!auth_token_apple_signed(engine->auth)) {
            LOGV("engine[%i]: rule deny, creator of authorization is not signed by apple", connection_get_pid(engine->conn));
            return errAuthorizationDenied;
        }
    }
    
    switch (rule_get_class(rule)) {
        case RC_ALLOW:
            LOGV("engine[%i]: rule set to allow", connection_get_pid(engine->conn));
            return errAuthorizationSuccess;
        case RC_DENY:
            LOGV("engine[%i]: rule set to deny", connection_get_pid(engine->conn));
            return errAuthorizationDenied;
        case RC_USER:
            return _evaluate_class_user(engine,rule);
        case RC_RULE:
            return _evaluate_class_rule(engine, rule);
        case RC_MECHANISM:
            return _evaluate_class_mechanism(engine, rule);
        default:
            LOGV("engine[%i]: invalid class for rule or rule not found", connection_get_pid(engine->conn));
            return errAuthorizationInternal;
    }
}

static rule_t
_find_rule(engine_t engine, authdb_connection_t dbconn, const char * string)
{
    rule_t r = NULL;
    size_t sLen = strlen(string);
    
    char * buf = calloc(1u, sLen + 1);
    strlcpy(buf, string, sLen + 1);
    char * ptr = buf + sLen;
    __block int64_t count = 0;
    
    for (;;) {
        
        // lookup rule
        authdb_step(dbconn, "SELECT COUNT(name) AS cnt FROM rules WHERE name = ? AND type = 1",
        ^(sqlite3_stmt *stmt) {
            sqlite3_bind_text(stmt, 1, buf, -1, NULL);
        }, ^bool(auth_items_t data) {
            count = auth_items_get_int64(data, "cnt");
            return false;
        });
        
        if (count > 0) {
            r = rule_create_with_string(buf, dbconn);
            goto done;
        }
        
        // if buf ends with a . and we didn't find a rule remove .
        if (*ptr == '.') {
            *ptr = '\0';
        }
        // find any remaining . and truncate the string
        ptr = strrchr(buf, '.');
        if (ptr) {
            *(ptr+1) = '\0';
        } else {
            break;
        }
    }
    
done:
    free_safe(buf);
    
    // set default if we didn't find a rule
    if (r == NULL) {
        r = rule_create_with_string("", dbconn);
        if (rule_get_id(r) == 0) {
            CFReleaseNull(r);
            LOGE("engine[%i]: default rule lookup error (missing), using builtin defaults", connection_get_pid(engine->conn));
            r = rule_create_default();
        }
    }
    return r;
}

static void _parse_enviroment(engine_t engine, auth_items_t enviroment)
{
    require(enviroment != NULL, done);

#if DEBUG
    LOGV("engine[%i]: Dumping Enviroment", connection_get_pid(engine->conn));
    _show_cf(enviroment);
#endif

    // Check if a credential was passed into the environment and we were asked to extend the rights
    if (engine->flags & kAuthorizationFlagExtendRights) {
        const char * user = auth_items_get_string(enviroment, kAuthorizationEnvironmentUsername);
        const char * pass = auth_items_get_string(enviroment, kAuthorizationEnvironmentPassword);
        bool shared = auth_items_exist(enviroment, kAuthorizationEnvironmentShared);
        require(user != NULL, done);

        struct passwd *pw = getpwnam(user);
        require_action(pw != NULL, done, LOGE("engine[%i]: user not found %s", connection_get_pid(engine->conn), user));
        
        int checkpw_status = checkpw_internal(pw, pass ? pass : "");
        require_action(checkpw_status == CHECKPW_SUCCESS, done, LOGE("engine[%i]: checkpw() returned %d; failed to authenticate user %s (uid %u).", connection_get_pid(engine->conn), checkpw_status, pw->pw_name, pw->pw_uid));
        
        credential_t cred = credential_create(pw->pw_uid);
        if (credential_get_valid(cred)) {
            LOG("engine[%i]: checkpw() succeeded, creating credential for user %s", connection_get_pid(engine->conn), user);
            _engine_set_credential(engine, cred, shared);
            
            auth_items_set_string(engine->context, kAuthorizationEnvironmentUsername, user);
            auth_items_set_string(engine->context, kAuthorizationEnvironmentPassword, pass ? pass : "");
        }
        CFReleaseSafe(cred);
    }
    
done:
    endpwent();
    return;
}

static bool _verify_sandbox(engine_t engine, const char * right)
{
    pid_t pid = process_get_pid(engine->proc);
    if (sandbox_check(pid, "authorization-right-obtain", SANDBOX_FILTER_RIGHT_NAME, right)) {
        LOGE("Sandbox denied authorizing right '%s' by client '%s' [%d]", right, process_get_code_url(engine->proc), pid);
        return false;
    }
    
    pid = auth_token_get_pid(engine->auth);
    if (auth_token_get_sandboxed(engine->auth) && sandbox_check(pid, "authorization-right-obtain", SANDBOX_FILTER_RIGHT_NAME, right)) {
        LOGE("Sandbox denied authorizing right '%s' for authorization created by '%s' [%d]", right, auth_token_get_code_url(engine->auth), pid);
        return false;
    }
    
    return true;
}

#pragma mark -
#pragma mark engine methods

OSStatus engine_authorize(engine_t engine, auth_rights_t rights, auth_items_t enviroment, AuthorizationFlags flags)
{
    __block OSStatus status = errAuthorizationSuccess;
    __block bool savePassword = false;
    ccaudit_t ccaudit = NULL;
    
    require(rights != NULL, done);
    
    ccaudit = ccaudit_create(engine->proc, engine->auth, AUE_ssauthorize);
    if (auth_rights_get_count(rights) > 0) {
        ccaudit_log(ccaudit, "begin evaluation", NULL, 0);
    }
    
    engine->flags = flags;
    
    if (enviroment) {
        _parse_enviroment(engine, enviroment);
        auth_items_copy(engine->hints, enviroment);
    }
    
    auth_items_copy(engine->context, auth_token_get_context(engine->auth));
    
    engine->dismissed = false;
    auth_rights_clear(engine->grantedRights);
    
    auth_rights_iterate(rights, ^bool(const char *key) {
        if (!key)
            return true;


        if (!_verify_sandbox(engine, key)) {
            status = errAuthorizationDenied;
            return false;
        }
        
        authdb_connection_t dbconn = authdb_connection_acquire(server_get_database()); // get db handle
        
        LOGV("engine[%i]: evaluate right %s", connection_get_pid(engine->conn), key);
        rule_t rule = _find_rule(engine, dbconn, key);
        const char * rule_name = rule_get_name(rule);
        if (rule_name && (strcasecmp(rule_name, "") == 0)) {
            rule_name = "default (not defined)";
        }
        LOGV("engine[%i]: using rule %s", connection_get_pid(engine->conn), rule_name);

        // only need the hints & mechanisms if we are going to show ui
        if (engine->flags & kAuthorizationFlagInteractionAllowed) {
            _set_right_hints(engine->hints, key);
            _set_localization_hints(dbconn, engine->hints, rule);
            if (!engine->authenticateRule) {
                engine->authenticateRule = rule_create_with_string("authenticate", dbconn);
            }
        }
        
        authdb_connection_release(&dbconn); // release db handle
        
        engine->currentRightName = key;
        engine->currentRule = rule;
        
        ccaudit_log(ccaudit, key, rule_name, 0);
        
        status = _evaluate_rule(engine, engine->currentRule);
        switch (status) {
            case errAuthorizationSuccess:
                auth_rights_add(engine->grantedRights, key);
                auth_rights_set_flags(engine->grantedRights, key, auth_rights_get_flags(rights,key));
                
                if ((engine->flags & kAuthorizationFlagPreAuthorize) &&
                    (rule_get_class(engine->currentRule) == RC_USER) &&
                    (rule_get_timeout(engine->currentRule) == 0)) {
                    auth_rights_set_flags(engine->grantedRights, engine->currentRightName, kAuthorizationFlagPreAuthorize);
                }
                
                LOG("Succeeded authorizing right '%s' by client '%s' [%d] for authorization created by '%s' [%d] (%X,%d)",
                    key, process_get_code_url(engine->proc), process_get_pid(engine->proc),
                    auth_token_get_code_url(engine->auth), auth_token_get_pid(engine->auth), engine->flags, auth_token_least_privileged(engine->auth));
                if (rule_get_extract_password(engine->currentRule)) {
                    savePassword = true;
                }
                break;
            case errAuthorizationDenied:
            case errAuthorizationInteractionNotAllowed:
            case errAuthorizationCanceled:
                if (engine->flags & kAuthorizationFlagInteractionAllowed) {
                    LOG("Failed to authorize right '%s' by client '%s' [%d] for authorization created by '%s' [%d] (%X,%d) (%i)",
                        key, process_get_code_url(engine->proc), process_get_pid(engine->proc),
                        auth_token_get_code_url(engine->auth), auth_token_get_pid(engine->auth), engine->flags, auth_token_least_privileged(engine->auth), status);
                } else {
                    LOGV("Failed to authorize right '%s' by client '%s' [%d] for authorization created by '%s' [%d] (%X,%d) (%i)",
                        key, process_get_code_url(engine->proc), process_get_pid(engine->proc),
                        auth_token_get_code_url(engine->auth), auth_token_get_pid(engine->auth), engine->flags, auth_token_least_privileged(engine->auth), status);
                }
                break;
            default:
                LOGE("engine[%i]: evaluate returned %i returning errAuthorizationInternal", connection_get_pid(engine->conn), status);
                status = errAuthorizationInternal;
                break;
        }

        ccaudit_log_authorization(ccaudit, engine->currentRightName, status);
        
        CFReleaseSafe(rule);
        engine->currentRightName = NULL;
        engine->currentRule = NULL;
        
        auth_items_remove_with_flags(engine->hints, kEngineHintsFlagTemporary);
        
        if (!(engine->flags & kAuthorizationFlagPartialRights) && (status != errAuthorizationSuccess)) {
            return false;
        }
        
        return true;
    });
    
    if ((engine->flags & kAuthorizationFlagPartialRights) && (auth_rights_get_count(engine->grantedRights) > 0)) {
        status = errAuthorizationSuccess;
    }
    
    if (engine->dismissed) {
        status = errAuthorizationDenied;
    }
    
    LOGV("engine[%i]: authorize result: %i", connection_get_pid(engine->conn), status);
    
    if ((engine->flags & kAuthorizationFlagExtendRights) && !(engine->flags & kAuthorizationFlagDestroyRights)) {
        _cf_set_iterate(engine->credentials, ^bool(CFTypeRef value) {
            credential_t cred = (credential_t)value;
            // skip all uid credentials when running in least privileged
            if (auth_token_least_privileged(engine->auth) && !credential_is_right(cred))
                return true; 
            
            session_t session = auth_token_get_session(engine->auth);
            auth_token_set_credential(engine->auth, cred);
            if (credential_get_shared(cred)) {
                session_set_credential(session, cred);
            }
            if (credential_is_right(cred)) {
                LOGV("engine[%i]: adding least privileged %scredential %s to authorization", connection_get_pid(engine->conn), credential_get_shared(cred) ? "shared " : "", credential_get_name(cred));
            } else {
                LOGV("engine[%i]: adding %scredential %s (%i) to authorization", connection_get_pid(engine->conn), credential_get_shared(cred) ? "shared " : "", credential_get_name(cred), credential_get_uid(cred));
            }
            return true;
        });
    }
    
    if ((status == errAuthorizationSuccess) || (status == errAuthorizationCanceled)) {
        if (savePassword && (status == errAuthorizationSuccess)) {
            auth_items_set_flags(engine->context, kAuthorizationEnvironmentPassword, kAuthorizationContextFlagExtractable);
        }
        
        auth_items_copy_with_flags(auth_token_get_context(engine->auth), engine->context, kAuthorizationContextFlagExtractable);
    }
    
    if (auth_rights_get_count(rights) > 0) {
        ccaudit_log(ccaudit, "end evaluation", NULL, status);
    }
    
#if DEBUG
    LOGV("engine[%i]: ********** Dumping auth->credentials **********", connection_get_pid(engine->conn));
    auth_token_credentials_iterate(engine->auth, ^bool(credential_t cred) {
        _show_cf(cred);
        return true;
    });
    LOGV("engine[%i]: ********** Dumping session->credentials **********", connection_get_pid(engine->conn));
    session_credentials_iterate(auth_token_get_session(engine->auth), ^bool(credential_t cred) {
        _show_cf(cred);
        return true;
    });
    LOGV("engine[%i]: ********** Dumping engine->context **********", connection_get_pid(engine->conn));
    _show_cf(engine->context);
    LOGV("engine[%i]: ********** Dumping auth->context **********", connection_get_pid(engine->conn));
    _show_cf(auth_token_get_context(engine->auth));
    LOGV("engine[%i]: ********** Dumping granted rights **********", connection_get_pid(engine->conn));
    _show_cf(engine->grantedRights);
#endif
    
done:
    auth_items_clear(engine->context);
    auth_items_clear(engine->sticky_context);
    CFReleaseSafe(ccaudit);
    CFDictionaryRemoveAllValues(engine->mechanism_agents);
    
    return status;
}

static bool
_wildcard_right_exists(engine_t engine, const char * right)
{
    // checks if a wild card right exists
    // ex: com.apple. system.
    bool exists = false;
    rule_t rule = NULL;
    authdb_connection_t dbconn = authdb_connection_acquire(server_get_database()); // get db handle
    require(dbconn != NULL, done);
    
    rule = _find_rule(engine, dbconn, right);
    require(rule != NULL, done);
    
    const char * ruleName = rule_get_name(rule);
    require(ruleName != NULL, done);
    size_t len = strlen(ruleName);
    require(len != 0, done);
    
    if (ruleName[len-1] == '.') {
        exists = true;
        goto done;
    }

done:
    authdb_connection_release(&dbconn);
    CFReleaseSafe(rule);

    return exists;
}

// Validate db right modification

// meta rights are constructed as follows:
// we don't allow setting of wildcard rights, so you can only be more specific
// note that you should never restrict things with a wildcard right without disallowing
// changes to the entire domain.  ie.
//		system.privilege.   		-> never
//		config.add.system.privilege.	-> never
//		config.modify.system.privilege.	-> never
//		config.delete.system.privilege.	-> never
// For now we don't allow any configuration of configuration rules
//		config.config. -> never

OSStatus engine_verify_modification(engine_t engine, rule_t rule, bool remove, bool force_modify)
{
    OSStatus status = errAuthorizationDenied;
    auth_rights_t checkRight = NULL;
    char buf[BUFSIZ];
    memset(buf, 0, sizeof(buf));
    
    const char * right = rule_get_name(rule);
    require(right != NULL, done);
    size_t len = strlen(right);
    require(len != 0, done);

    require_action(right[len-1] != '.', done, LOGE("engine[%i]: not allowed to set wild card rules", connection_get_pid(engine->conn)));

    if (strncasecmp(right, kConfigRight, strlen(kConfigRight)) == 0) {
        // special handling of meta right change:
		// config.add. config.modify. config.remove. config.{}.
		// check for config.<right> (which always starts with config.config.)
        strlcat(buf, kConfigRight, sizeof(buf));
    } else {
        bool existing = (rule_get_id(rule) != 0) ? true : _wildcard_right_exists(engine, right);
        if (!remove) {
            if (existing || force_modify) {
                strlcat(buf, kAuthorizationConfigRightModify,sizeof(buf));
            } else {
                strlcat(buf, kAuthorizationConfigRightAdd, sizeof(buf));
            }
        } else {
            if (existing) {
                strlcat(buf, kAuthorizationConfigRightRemove, sizeof(buf));
            } else {
                status = errAuthorizationSuccess;
                goto done;
            }
        }
    }
    
    strlcat(buf, right, sizeof(buf));

    checkRight = auth_rights_create();
    auth_rights_add(checkRight, buf);
    status = engine_authorize(engine, checkRight, NULL, kAuthorizationFlagDefaults | kAuthorizationFlagInteractionAllowed | kAuthorizationFlagExtendRights);

done:
    LOGV("engine[%i]: authorizing %s for db modification: %i", connection_get_pid(engine->conn), right, status);
    CFReleaseSafe(checkRight);
    return status;
}

void
_engine_set_credential(engine_t engine, credential_t cred, bool shared)
{
    LOGV("engine[%i]: adding %scredential %s (%i) to engine shared: %i", connection_get_pid(engine->conn), credential_get_shared(cred) ? "shared " : "", credential_get_name(cred), credential_get_uid(cred), shared);
    CFSetSetValue(engine->credentials, cred);
    if (shared) {
        credential_t sharedCred = credential_create_with_credential(cred, true);
        CFSetSetValue(engine->credentials, sharedCred);
        CFReleaseSafe(sharedCred);
    }
}

auth_rights_t
engine_get_granted_rights(engine_t engine)
{
    return engine->grantedRights;
}

CFAbsoluteTime engine_get_time(engine_t engine)
{
    return engine->now;
}

void engine_destroy_agents(engine_t engine)
{
    engine->dismissed = true;

    _cf_dictionary_iterate(engine->mechanism_agents, ^bool(CFTypeRef key __attribute__((__unused__)), CFTypeRef value) {
        LOGD("engine[%i]: Destroying %s", connection_get_pid(engine->conn), mechanism_get_string((mechanism_t)key));
        agent_t agent = (agent_t)value;
        agent_destroy(agent);
        
        return true;
    });
}

void engine_interrupt_agent(engine_t engine)
{
    _cf_dictionary_iterate(engine->mechanism_agents, ^bool(CFTypeRef key __attribute__((__unused__)), CFTypeRef value) {
        agent_t agent = (agent_t)value;
        agent_notify_interrupt(agent);
        return true;
    });
}
