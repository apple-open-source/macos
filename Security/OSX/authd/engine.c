/* Copyright (c) 2012-2014 Apple Inc. All Rights Reserved. */

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
#include "od.h"

#include <pwd.h>
#include <Security/checkpw.h>
int checkpw_internal( const struct passwd *pw, const char* password );

#include <Security/AuthorizationTags.h>
#include <Security/AuthorizationTagsPriv.h>
#include <Security/AuthorizationPriv.h>
#include <Security/AuthorizationPlugin.h>
#include <LocalAuthentication/LAPublicDefines.h>
#include <LocalAuthentication/LAPrivateDefines.h>
#include <sandbox.h>
#include <coreauthd_spi.h>
#include <ctkloginhelper.h>
#include <EndpointSecuritySystem/ESSubmitSPI.h>
#include <mach/message.h>
#include <sys/stat.h>

AUTHD_DEFINE_LOG

static os_log_t AUTHD_ANLOG_DEFAULT(void) { \
static dispatch_once_t once; \
static os_log_t log; \
dispatch_once(&once, ^{ log = os_log_create("com.apple.Authorization", "analytics"); }); \
return log; \
};

static void _set_process_hints(auth_items_t, process_t);
static void _set_process_immutable_hints(auth_items_t, process_t);
static void _set_auth_token_hints(auth_items_t, auth_items_t, auth_token_t);
static OSStatus _evaluate_user_credential_for_rule(engine_t, credential_t, rule_t, bool, bool, enum Reason *);
static void _engine_set_credential(engine_t, credential_t, bool);
static OSStatus _evaluate_rule(engine_t, rule_t, bool *);
static bool _preevaluate_class_rule(engine_t engine, rule_t rule, const char **group, bool *sessionOwner, bool *secureToken);
static bool _preevaluate_rule(engine_t engine, rule_t rule, const char **group, bool *sessionOwner, bool *secureToken);

static uint64_t global_engine_count;

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
    
	CFTypeRef la_context;
    bool credentialsProvided;

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
    
    uint64_t engine_index;
};

static void
_engine_finalizer(CFTypeRef value)
{
    engine_t engine = (engine_t)value;
    
    CFReleaseNull(engine->mechanism_agents);
    CFReleaseNull(engine->conn);
    CFReleaseNull(engine->auth);
    CFReleaseNull(engine->hints);
    CFReleaseNull(engine->context);
    CFReleaseNull(engine->immutable_hints);
    CFReleaseNull(engine->sticky_context);
    CFReleaseNull(engine->grantedRights);
    CFReleaseNull(engine->sessionCredential);
    CFReleaseNull(engine->credentials);
    CFReleaseNull(engine->effectiveCredentials);
    CFReleaseNull(engine->authenticateRule);
    CFReleaseNull(engine->la_context);
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

static CFTypeID engine_get_type_id(void) {
    static CFTypeID type_id = _kCFRuntimeNotATypeID;
    static dispatch_once_t onceToken;
    
    dispatch_once(&onceToken, ^{
        type_id = _CFRuntimeRegisterClass(&_auth_type_engine);
    });
    
    return type_id;
}

static bool _in_system_update()
{
    struct stat st;
    if (stat("/var/db/.AppleUpgrade", &st) == -1) {
        return false;
    }
    return true;
}

engine_t
engine_create(connection_t conn, auth_token_t auth)
{
    engine_t engine = NULL;
    require(conn != NULL, done);
    require(auth != NULL, done);

    engine = (engine_t)_CFRuntimeCreateInstance(kCFAllocatorDefault, engine_get_type_id(), AUTH_CLASS_SIZE(engine), NULL);
    require(engine != NULL, done);
    
    engine->engine_index = global_engine_count++;

    engine->conn = (connection_t)CFRetain(conn);
    engine->proc = connection_get_process(conn);
    engine->auth = (auth_token_t)CFRetain(auth);
    
    engine->hints = auth_items_create();
    engine->context = auth_items_create();
    engine->immutable_hints = auth_items_create();
    engine->sticky_context = auth_items_create();
    _set_process_hints(engine->hints, engine->proc);
    _set_process_immutable_hints(engine->immutable_hints, engine->proc);
	_set_auth_token_hints(engine->hints, engine->immutable_hints, auth);
    
    engine->grantedRights = auth_rights_create();
    
    engine->reason = noReason;

	engine->la_context = NULL;
    engine->credentialsProvided = false;
    
    engine->now = CFAbsoluteTimeGetCurrent();
    
    session_update(auth_token_get_session(engine->auth));
    if (isInFVUnlockOrRecovery()) {
        engine->sessionCredential = credential_create_fvunlock(NULL, true);
    } else {
        uid_t uid = session_get_uid(auth_token_get_session(engine->auth));
        if (uid != (uid_t) -1) {
            os_log_debug(AUTHD_LOG, "engine %llu: creating session credentials for %d", engine->engine_index, uid);
        } else {
            os_log_debug(AUTHD_LOG, "engine %llu: no session credentials available", engine->engine_index);
        }
        engine->sessionCredential = credential_create(uid);
    }

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
    auth_items_set_bool(immutable_hints, AGENT_HINT_CLIENT_SIGNED, process_apple_signed(proc));
	auth_items_set_bool(immutable_hints, AGENT_HINT_CLIENT_FROM_APPLE, process_firstparty_signed(proc));
}

void
_set_auth_token_hints(auth_items_t hints, auth_items_t immutable_hints, auth_token_t auth)
{
    auth_items_set_string(hints, AGENT_HINT_CLIENT_PATH, auth_token_get_code_url(auth));
    auth_items_set_int(hints, AGENT_HINT_CREATOR_PID, auth_token_get_pid(auth));
    const audit_info_s * info = auth_token_get_audit_info(auth);
    auth_items_set_data(hints, AGENT_HINT_CREATOR_AUDIT_TOKEN, &info->opaqueToken, sizeof(info->opaqueToken));

	process_t proc = process_create(info, auth_token_get_session(auth));
	if (proc) {
		auth_items_set_bool(immutable_hints, AGENT_HINT_CREATOR_SIGNED, process_apple_signed(proc));
		auth_items_set_bool(immutable_hints, AGENT_HINT_CREATOR_FROM_APPLE, process_firstparty_signed(proc));
	}
	CFReleaseSafe(proc);
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
    if (rule_get_securetokenuser(rule)) {
        auth_items_set_bool(hints, AGENT_HINT_REQUIRE_STOKEN, true);
    } else {
        auth_items_remove(hints, AGENT_HINT_REQUIRE_STOKEN);
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
    os_log_debug(AUTHD_LOG, "engine %llu: ** prepare agent hints for rule %{public}s", engine->engine_index, rule_get_name(rule));
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
                    os_log_error(AUTHD_LOG, "Rule %{public}s does NOT satisfied (rule not shared) (engine %llu)", credential_get_name(cred), engine->engine_index);
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

static bool _is_fvunlock_user_in_group(engine_t engine, rule_t rule)
{
    if (!isInFVUnlockOrRecovery()) {
        return false;
    }
    
    const char *group = rule_get_group(rule);
    if (!(group && strcmp(group, "admin") == 0)) {
        os_log_error(AUTHD_LOG, "Group %{public}s not supported in FV mode (engine %llu)", group, engine->engine_index);
        return false;
    }

    const char *data = auth_items_get_string(engine->context, kAuthorizationFVAdmin);
    if (!data) {
        os_log_error(AUTHD_LOG, "User is not member of the group %{public}s (engine %llu)", group, engine->engine_index);
        return false;
    }
    
    return (strcmp(data, "1") == 0);
}

static OSStatus
_evaluate_user_credential_for_rule(engine_t engine, credential_t cred, rule_t rule, bool ignoreShared, bool sessionOwner, enum Reason * reason)
{
    const char * cred_label = sessionOwner ? "session owner" : "credential";
    os_log(AUTHD_LOG, "Validating %{public}s%{public}s %{public}s (%i) for %{public}s (engine %llu)", credential_get_shared(cred) ? "shared " : "",
         cred_label,
         credential_get_name(cred),
         credential_get_uid(cred),
         rule_get_name(rule),
         engine->engine_index);
    
    if (rule_get_class(rule) != RC_USER) {
        os_log(AUTHD_LOG, "Invalid rule class %i (engine %llu)", rule_get_class(rule), engine->engine_index);
        return errAuthorizationDenied;
    }

    if (credential_get_valid(cred) != true && !isInFVUnlockOrRecovery()) {
        os_log(AUTHD_LOG, "%{public}s %i invalid (does NOT satisfy rule) (engine %llu)", cred_label, credential_get_uid(cred), engine->engine_index);
        if (reason) {  *reason = invalidPassphrase; }
        return errAuthorizationDenied;
    }

    if (engine->now - credential_get_creation_time(cred) > rule_get_timeout(rule)) {
        os_log(AUTHD_LOG, "%{public}s %i expired '%f > %lli' (does NOT satisfy rule) (engine %llu)", cred_label, credential_get_uid(cred),
             (engine->now - credential_get_creation_time(cred)), rule_get_timeout(rule), engine->engine_index);
        if (reason) {  *reason = unknownReason; }
        return errAuthorizationDenied;
    }
    
    if (!ignoreShared) {
        if (!rule_get_shared(rule) && credential_get_shared(cred)) {
            os_log(AUTHD_LOG, "Shared %{public}s %i (does NOT satisfy rule) (engine %llu)", cred_label, credential_get_uid(cred), engine->engine_index);
            if (reason) {  *reason = unknownReason; }
            return errAuthorizationDenied;
        }
    }
    
    if (credential_get_uid(cred) == 0) {
        os_log(AUTHD_LOG, "%{public}s %i has uid 0 (does satisfy rule) (engine %llu)", cred_label, credential_get_uid(cred), engine->engine_index);
        return errAuthorizationSuccess;
    }
    
    if (rule_get_session_owner(rule)) {
        if (credential_get_uid(cred) == session_get_uid(auth_token_get_session(engine->auth))) {
            os_log(AUTHD_LOG, "%{public}s %i is session owner (does satisfy rule) (engine %llu)", cred_label, credential_get_uid(cred), engine->engine_index);
            return errAuthorizationSuccess;
        }
    }
    
    if (!isInFVUnlockOrRecovery() && rule_get_securetokenuser(rule)) {
        if (!odUserHasSecureToken(credential_get_name(cred))) {
            os_log(AUTHD_LOG, "%{public}s %i is not a securetoken user (does NOT satisfy rule) (engine %llu)", cred_label, credential_get_uid(cred), engine->engine_index);
            if (reason) {  *reason = unacceptableUser; }
            return errAuthorizationDenied;
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
            
            if (credential_check_membership(cred, rule_get_group(rule)) || _is_fvunlock_user_in_group(engine, rule)) {
                os_log(AUTHD_LOG, "%{public}s %i is member of group %{public}s (does satisfy rule) (engine %llu)", cred_label, credential_get_uid(cred), rule_get_group(rule), engine->engine_index);
                return errAuthorizationSuccess;
            } else {
                if (reason) {  *reason = userNotInGroup; }
            }
        } while (0);
    } else if (rule_get_session_owner(rule)) { // rule asks only if user is the session owner
        if (reason) {  *reason = unacceptableUser; }
    }

	os_log(AUTHD_LOG, "%{public}s %i (does NOT satisfy rule), reason %d (engine %llu)", cred_label, credential_get_uid(cred), reason ? *reason : -1, engine->engine_index);
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

static bool
_extract_password_from_la(engine_t engine)
{
	bool retval = false;

	if (!engine->la_context) {
		return retval;
	}

	// try to retrieve secret
	CFDataRef passdata = LACopyCredential(engine->la_context, kLACredentialTypeExtractablePasscode, NULL);
	if (passdata) {
		if (CFDataGetBytePtr(passdata)) {
            os_log_debug(AUTHD_LOG, "engine %llu: LA credentials retrieved", engine->engine_index);
			auth_items_set_data(engine->context, kAuthorizationEnvironmentPassword, CFDataGetBytePtr(passdata), CFDataGetLength(passdata));
		} else {
			const char *empty_pass = "\0"; // authd code is unable to process empty strings so passing empty string as terminator only
            os_log_debug(AUTHD_LOG, "engine %llu: LA credentials empty", engine->engine_index);
			auth_items_set_data(engine->context, kAuthorizationEnvironmentPassword, empty_pass, 1);
		}
		CFRelease(passdata);
	}
	return retval;
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
    
	CFDictionaryRef laResult = NULL;
    CFStringRef unameCf = NULL;

    CFIndex count = CFArrayGetCount(mechanisms);
	bool sheet_evaluation = false;
	if (engine->la_context) {
		int tmp = kLAOptionNotInteractive;
		CFNumberRef key = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &tmp);
		tmp = 1;
		CFNumberRef value = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &tmp);
		if (key && value) {
			CFMutableDictionaryRef options = CFDictionaryCreateMutable(kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
			CFDictionarySetValue(options, key, value);
			laResult = LACopyResultOfPolicyEvaluation(engine->la_context, kLAPolicyDeviceOwnerAuthentication, options, NULL);
            os_log_debug(AUTHD_LOG, "engine %llu: Retrieve LA evaluate result: %d", engine->engine_index, laResult != NULL);
			CFReleaseSafe(options);
		}
		CFReleaseSafe(key);
		CFReleaseSafe(value);
	}
    
    Boolean apUsed = auth_items_exist(engine->context, AGENT_CONTEXT_AP_PAM_SERVICE_NAME);
    const char *uname = NULL;
    uid_t uid = (uid_t)-1;
    
    if (auth_items_exist(engine->context, "sheet-uid")) {
        uid = auth_items_get_uint(engine->context, "sheet-uid");
    } else if (uid == (uid_t)-1) {
        if (apUsed) {
            uname = auth_items_get_string(engine->context, AGENT_CONTEXT_AP_USER_NAME);
        } else {
            uname = auth_items_get_string(engine->context, AGENT_USERNAME);
        }
    }
    
    if (!uname && uid != (uid_t)-1) {
        // we did not get user from Username or AHP username field but we have UID so try it
        struct passwd *pw = getpwuid(uid);
        if (pw) {
            uname = pw->pw_name;
        }
    }
    
    if (engine->la_context) {
        if (!uname) {
            os_log_error(AUTHD_LOG, "No user specified, unable to continue");
            result = kAuthorizationResultDeny;
            goto done;
        }
        
        unameCf = CFStringCreateWithCString(kCFAllocatorDefault, uname, kCFStringEncodingUTF8);
        if(!unameCf) {
            os_log_error(AUTHD_LOG, "Unable to create a username, unable to continue");
            result = kAuthorizationResultDeny;
            goto done;
        }
        os_log_debug(AUTHD_LOG, "Sheet authorization for %{public}@", unameCf);
    }
    
	for (CFIndex i = 0; i < count; i++) {
        mechanism_t mech = (mechanism_t)CFArrayGetValueAtIndex(mechanisms, i);
        
        if (mechanism_get_type(mech)) {
            os_log_debug(AUTHD_LOG, "engine %llu: running builtin mechanism %{public}s (%li of %li)", engine->engine_index, mechanism_get_string(mech), i+1, count);
            result = _evaluate_builtin_mechanism(engine, mech);
        } else {
            bool shoud_run_agent = !(engine->credentialsProvided && strcmp(mechanism_get_string(mech), "builtin:authenticate") == 0);     // evaluate comes from sheet -> we may not want to run standard SecurityAgent or authhost
			if (engine->la_context) {
				// sheet variant in progress
				if (strcmp(mechanism_get_string(mech), "builtin:authenticate") == 0) {
					// set the UID the same way as SecurityAgent would
					if (auth_items_exist(engine->context, "sheet-uid")) {
                        uid = auth_items_get_uint(engine->context, "sheet-uid");
						os_log_debug(AUTHD_LOG, "engine %llu: setting sheet UID %d to the context", engine->engine_index, uid);
						auth_items_set_uint(engine->context, AGENT_CONTEXT_UID, auth_items_get_uint(engine->context, "sheet-uid"));
					}

					// find out if sheet just provided credentials or did real authentication
					// if password is provided or PAM service name exists, it means authd has to evaluate credentials
					// otherwise we need to check la_result
					if (apUsed || auth_items_exist(engine->context, kAuthorizationEnvironmentPassword)) {
						// do not try to get credentials as it has been already passed by sheet
						os_log_debug(AUTHD_LOG, "engine %llu: ignoring builtin sheet authenticate", engine->engine_index);
					} else {
						// sheet itself did the authenticate the user
						os_log_debug(AUTHD_LOG, "engine %llu: running builtin sheet authenticate", engine->engine_index);
						sheet_evaluation = true;
					}
					shoud_run_agent = false; // SecurityAgent should not be run for builtin:authenticate
				} else if (strcmp(mechanism_get_string(mech), "builtin:authenticate,privileged") == 0) {
					if (sheet_evaluation) {
						os_log_debug(AUTHD_LOG, "engine %llu: running builtin sheet privileged authenticate", engine->engine_index);
						shoud_run_agent = false;
						if (!laResult || TKGetSmartcardSettingForUser(kTKEnforceSmartcard, unameCf) != 0) {
							result = kAuthorizationResultDeny;
                            os_log_error(AUTHD_LOG, "engine %llu: denying sheet auth (%d)", engine->engine_index, laResult == nil);
						}
					} else {
						// should_run_agent has to be set to true because we want authorizationhost to verify the credentials
						os_log_debug(AUTHD_LOG, "engine %llu: running authorizationhost with privileged authenticate", engine->engine_index);
					}
				}
			}

			if (shoud_run_agent) {
				agent_t agent = _get_agent(engine, mech, true, i == 0);
				require_action(agent != NULL, done, result = kAuthorizationResultUndefined; os_log_error(AUTHD_LOG, "Error creating mechanism agent (engine %llu)", engine->engine_index));

				// check if any agent has been interrupted (it necessary if interrupt will come during creation)
				CFIndex j;
				agent_t agent1;
				for (j = 0; j < i; j++) {
					agent1 = _get_agent(engine, (mechanism_t)CFArrayGetValueAtIndex(mechanisms, j), false, j == 0);
					if (agent1 && agent_get_state(agent1) == interrupting) {
						break;
					}
				}
				if (j < i) {
					os_log(AUTHD_LOG, "engine %llu: mechanisms interrupted", engine->engine_index);
					char * buf = NULL;
					asprintf(&buf, "evaluation interrupted by %s; restarting evaluation there", mechanism_get_string(agent_get_mechanism(agent1)));
					ccaudit_log_mechanism(ccaudit, engine->currentRightName, mechanism_get_string(agent_get_mechanism(agent1)), kAuthorizationResultAllow, buf);
					free_safe(buf);
					ccaudit_log_mechanism(ccaudit, engine->currentRightName, mechanism_get_string(mech), kAuthorizationResultAllow, NULL);
					const char * token_name = auth_items_get_string(hints, AGENT_HINT_TOKEN_NAME);
					if (token_name && strlen(token_name) == 0) {
						auth_items_remove(hints, AGENT_HINT_TOKEN_NAME);
					}
					auth_items_copy(context, agent_get_context(agent1));
					auth_items_copy(hints, agent_get_hints(agent1));

					i = j - 1;

					continue;
				}

				os_log(AUTHD_LOG, "engine %llu: running mechanism %{public}s (%li of %li)", engine->engine_index, mechanism_get_string(agent_get_mechanism(agent)), i+1, count);

				result = agent_run(agent, hints, context, engine->immutable_hints);

                if (engine->dismissed) {
                    os_log_debug(AUTHD_LOG, "engine %llu: caller dismissed, doing cleanup", engine->engine_index);
                    break;
                }

                auth_items_copy(context, agent_get_context(agent));
                auth_items_copy(hints, agent_get_hints(agent));
                
				bool interrupted = false;
				for (CFIndex i2 = 0; i2 != i; i2++) {
					agent_t agent2 = _get_agent(engine, (mechanism_t)CFArrayGetValueAtIndex(mechanisms, i2), false, i == 0);
					if (agent2 && agent_get_state(agent2) == interrupting) {
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
				const char *token_name = auth_items_get_string(hints, AGENT_HINT_TOKEN_NAME);
				if (token_name && strlen(token_name) == 0) {
					auth_items_remove(hints, AGENT_HINT_TOKEN_NAME);
				}

				if (interrupted) {
					os_log_info(AUTHD_LOG, "Mechanisms interrupted (engine %llu)", engine->engine_index);
					enum Reason reason = worldChanged;
					auth_items_set_data(hints, AGENT_HINT_RETRY_REASON, &reason, sizeof(reason));
					result = kAuthorizationResultAllow;
                    CFRetain(engine);
                    dispatch_sync(dispatch_get_main_queue(), ^{
                        _cf_dictionary_iterate(engine->mechanism_agents, ^bool(CFTypeRef key __attribute__((__unused__)), CFTypeRef value) {
                            agent_t tempagent = (agent_t)value;
                            agent_clear_interrupt(tempagent);
                            return true;
                        });
                        CFRelease(engine);
                    });
				}
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
    auth_items_set_flags(context, AGENT_CONTEXT_AP_PAM_SERVICE_NAME, kAuthorizationContextFlagExtractable);
    if ((result == kAuthorizationResultUserCanceled) || (result == kAuthorizationResultAllow) || engine->credentialsProvided) {
        // only make non-sticky context values available externally
        auth_items_set_flags(context, kAuthorizationEnvironmentPassword, kAuthorizationContextFlagVolatile);
		// <rdar://problem/16275827> Takauthorizationenvironmentusername should always be extractable
        auth_items_set_flags(context, kAuthorizationEnvironmentUsername, kAuthorizationContextFlagExtractable);
        auth_items_copy_with_flags(engine->context, context, kAuthorizationContextFlagExtractable | kAuthorizationContextFlagVolatile);
        AuthorizationFlags flags = engine->credentialsProvided ? kAuthorizationContextFlagExtractable | kAuthorizationContextFlagVolatile | kAuthorizationContextFlagSticky : kAuthorizationContextFlagExtractable | kAuthorizationContextFlagVolatile;
        auth_items_copy_with_flags(engine->context, context, flags);
    }
    if (result == kAuthorizationResultDeny) {
        auth_items_clear(engine->sticky_context);
        // save off sticky values in context
        auth_items_copy_with_flags(engine->sticky_context, context, kAuthorizationContextFlagSticky);
    }
    
    CFReleaseSafe(ccaudit);
    CFReleaseSafe(context);
    CFReleaseSafe(hints);
	CFReleaseSafe(laResult);
    CFReleaseSafe(unameCf);

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
            os_log_error(AUTHD_LOG, "Evaluate - unexpected result %llu (engine %llu)", result, engine->engine_index);
            return errAuthorizationInternal;
        }
    }
}

static OSStatus
_evaluate_authentication(engine_t engine, rule_t rule)
{
    os_log_debug(AUTHD_LOG, "engine %llu: FV mode %d", engine->engine_index, isInFVUnlockOrRecovery());

    OSStatus status = errAuthorizationDenied;
    ccaudit_t ccaudit = ccaudit_create(engine->proc, engine->auth, AUE_ssauthint);
    os_log_debug(AUTHD_LOG, "engine %llu: evaluate authentication %{public}s", engine->engine_index, rule_get_name(rule));
    _set_rule_hints(engine->hints, rule);
    if (!isInFVUnlockOrRecovery()) {
        // we do not need to set hints in FVUnlock as we do not know which user will be authenticated
        _set_session_hints(engine, rule);
    }

    CFArrayRef mechanisms = rule_get_mechanisms(rule);
    if (!(CFArrayGetCount(mechanisms) > 0)) {
        mechanisms = rule_get_mechanisms(engine->authenticateRule);
    }
    require_action(CFArrayGetCount(mechanisms) > 0, done, os_log_debug(AUTHD_LOG, "engine %llu: error - no mechanisms found", engine->engine_index));
    
    int64_t ruleTries = rule_get_tries(rule);

	if (engine->credentialsProvided) {
		ruleTries = 1;
		os_log_debug(AUTHD_LOG, "engine %llu: credentials passed as env, one try is enough", engine->engine_index);
	}

    for (engine->tries = 0; engine->tries < ruleTries; engine->tries++) {
        
        auth_items_set_data(engine->hints, AGENT_HINT_RETRY_REASON, &engine->reason, sizeof(engine->reason));
        auth_items_set_int(engine->hints, AGENT_HINT_TRIES, engine->tries);
        status = _evaluate_mechanisms(engine, mechanisms);

        os_log_debug(AUTHD_LOG, "engine %llu: evaluate mechanisms result %d", engine->engine_index, (int)status);
        
        // successfully ran mechanisms to obtain credential
        if (status == errAuthorizationSuccess) {
            // deny is the default
            status = errAuthorizationDenied;
            
            credential_t newCred = NULL;

            if (isInFVUnlockOrRecovery() && auth_items_exist(engine->context, kAuthorizationFVAdmin)) {
                os_log_debug(AUTHD_LOG, "Credentials for FV unlock (engine %llu)", engine->engine_index);
                newCred = credential_create_fvunlock(engine->context, false);
            } else if (auth_items_exist(engine->context, AGENT_CONTEXT_UID)) {
                newCred = credential_create(auth_items_get_uint(engine->context, AGENT_CONTEXT_UID));
            } else {
                os_log_error(AUTHD_LOG, "Mechanism failed to return a valid UID (engine %llu)", engine->engine_index);
				if (engine->la_context) {
					// sheet failed so remove sheet reference and next time, standard dialog will be displayed
					CFReleaseNull(engine->la_context);
				}
            }
            
            if (newCred) {
                if (credential_get_valid(newCred) || isInFVUnlockOrRecovery()) {
                    os_log(AUTHD_LOG, "UID %u authenticated as user %{public}s (UID %i) for right '%{public}s'", auth_token_get_uid(engine->auth), credential_get_name(newCred), credential_get_uid(newCred), engine->currentRightName);
                    ccaudit_log_success(ccaudit, newCred, engine->currentRightName);
                } else {
                    os_log(AUTHD_LOG, "UID %u failed to authenticate as user '%{public}s' for right '%{public}s'", auth_token_get_uid(engine->auth), auth_items_get_string(engine->context, AGENT_USERNAME), engine->currentRightName);
                    ccaudit_log_failure(ccaudit, auth_items_get_string(engine->context, AGENT_USERNAME), engine->currentRightName);
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
                        os_log_debug(AUTHD_LOG, "engine %llu: authenticated as the session owner", engine->engine_index);
                        session_set_attributes(auth_token_get_session(engine->auth), AU_SESSION_FLAG_HAS_AUTHENTICATED);
                    }

                    break;
				} else {
					os_log_error(AUTHD_LOG, "User credential for rule failed (%d) (engine %llu)", (int)status, engine->engine_index);
				}

                CFReleaseSafe(newCred);
            }
            
        } else if (status == errAuthorizationCanceled || status == errAuthorizationInternal) {
			os_log_error(AUTHD_LOG, "Evaluate cancelled or failed %d (engine %llu)", (int)status, engine->engine_index);
			break;
        } else if (status == errAuthorizationDenied) {
			os_log_error(AUTHD_LOG, "Evaluate denied (engine %llu)", engine->engine_index);
			engine->reason = invalidPassphrase;
        }
        if (engine->dismissed) {
            // caller dismisssed, no need to run other mechanisms
            break;
        }
    }
    
    if (engine->tries == ruleTries) {
        engine->reason = tooManyTries;
        auth_items_set_data(engine->hints, AGENT_HINT_RETRY_REASON, &engine->reason, sizeof(engine->reason));
        auth_items_set_int(engine->hints, AGENT_HINT_TRIES, engine->tries);
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
                os_log_info(AUTHD_LOG, "Creator of authorization has entitlement for right %{public}s and is member of group '%{public}s' (engine %llu)", engine->currentRightName, rule_get_group(rule), engine->engine_index);
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
                os_log_info(AUTHD_LOG, "Creator of authorization has VPN entitlement and is member of group '%{public}s' (engine %llu)", rule_get_group(rule), engine->engine_index);
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
    
    if (rule_get_allow_root(rule)) {
        uid_t uid = auth_token_get_uid(engine->auth);
        if (uid == 0) {
            os_log_info(AUTHD_LOG, "Creator of authorization has uid == 0, granting right %{public}s (engine %llu)", engine->currentRightName, engine->engine_index);
            return errAuthorizationSuccess;
        } else {
            os_log_debug(AUTHD_LOG, "Creator of authorization has uid %d, NOT granting right %{public}s (engine %llu)", uid, engine->currentRightName, engine->engine_index);
        }
    }
    
    if (!rule_get_authenticate_user(rule)) {
        if (!isInFVUnlockOrRecovery()) {
            status = _evaluate_user_credential_for_rule(engine, engine->sessionCredential, rule, true, true, NULL);
            
            if (status == errAuthorizationSuccess) {
                return errAuthorizationSuccess;
            }
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
        os_log_error(AUTHD_LOG, "Fatal: authorization denied (kAuthorizationFlagExtendRights not set) (engine %llu)", engine->engine_index);
        return errAuthorizationDenied;
    }
    
    // authorization that timeout immediately cannot be preauthorized
    if (engine->flags & kAuthorizationFlagPreAuthorize && rule_get_timeout(rule) == 0) {
        return errAuthorizationSuccess;
	}

    if (!(engine->credentialsProvided)) {
        if (!(engine->flags & kAuthorizationFlagInteractionAllowed)) {
            os_log_error(AUTHD_LOG, "Fatal: interaction not allowed (kAuthorizationFlagInteractionAllowed not set) (engine %llu)", engine->engine_index);
            return errAuthorizationInteractionNotAllowed;
        }
        
        if (!(session_get_attributes(auth_token_get_session(engine->auth)) & AU_SESSION_FLAG_HAS_GRAPHIC_ACCESS)) {
            os_log_error(AUTHD_LOG, "Fatal: interaction not allowed (session has no ui access) (engine %llu)", engine->engine_index);
            return errAuthorizationInteractionNotAllowed;
        }
        
        if (server_in_dark_wake() && !(engine->flags & kAuthorizationFlagIgnoreDarkWake)) {
            os_log_error(AUTHD_LOG, "Fatal: authorization denied (DW) (engine %llu)", engine->engine_index);
            return errAuthorizationDenied;
        }
    }

	return _evaluate_authentication(engine, rule);
}

static OSStatus
_evaluate_class_rule(engine_t engine, rule_t rule, bool *save_pwd)
{
    __block OSStatus status = errAuthorizationDenied;
    int64_t kofn = rule_get_kofn(rule);

    uint32_t total = (uint32_t)rule_get_delegates_count(rule);
    __block uint32_t success_count = 0;
    __block uint32_t count = 0;
    os_log_debug(AUTHD_LOG, "engine %llu: ** rule %{public}s has %u delegates kofn = %lli", engine->engine_index, rule_get_name(rule), total, kofn);
    rule_delegates_iterator(rule, ^bool(rule_t delegate) {
        count++;
        
        if (kofn != 0 && success_count == kofn) {
            status = errAuthorizationSuccess;
            return false;
        }
        
        os_log_debug(AUTHD_LOG, "engine %llu: * evaluate rule %{public}s (%i)", engine->engine_index, rule_get_name(delegate), count);
        status = _evaluate_rule(engine, delegate, save_pwd);
        
        // if status is cancel/internal error abort
		if ((status == errAuthorizationCanceled) || (status == errAuthorizationInternal))
			return false;
        
        if (status != errAuthorizationSuccess) {
            if (kofn != 0) {
                // if remaining is less than required abort
                if ((total - count) < (kofn - success_count)) {
                    os_log_debug(AUTHD_LOG, "engine %llu: rule evaluation remaining: %i, required: %lli", engine->engine_index, (total - count), (kofn - success_count));
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

static bool
_preevaluate_class_rule(engine_t engine, rule_t rule, const char **group, bool *sessionOwner, bool *secureToken)
{
	os_log_debug(AUTHD_LOG, "engine %llu: _preevaluate_class_rule %{public}s", engine->engine_index, rule_get_name(rule));

	__block bool password_only = false;

	rule_delegates_iterator(rule, ^bool(rule_t delegate) {
		if (_preevaluate_rule(engine, delegate, group, sessionOwner, secureToken)) {
				password_only = true;
		}
		return true;
	});

	return password_only;
}

static OSStatus
_evaluate_class_mechanism(engine_t engine, rule_t rule)
{
    OSStatus status = errAuthorizationDenied;
    CFArrayRef mechanisms = NULL;
    
    require_action(rule_get_mechanisms_count(rule) > 0, done, status = errAuthorizationSuccess; os_log_error(AUTHD_LOG, "Fatal: no mechanisms specified (engine %llu)", engine->engine_index));
    
    mechanisms = rule_get_mechanisms(rule);
    
    if (server_in_dark_wake() && !(engine->flags & kAuthorizationFlagIgnoreDarkWake)) {
        CFIndex count = CFArrayGetCount(mechanisms);
        for (CFIndex i = 0; i < count; i++) {
            if (!mechanism_is_privileged((mechanism_t)CFArrayGetValueAtIndex(mechanisms, i))) {
                os_log_error(AUTHD_LOG, "Fatal: authorization denied (in DW) (engine %llu)", engine->engine_index);
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
        os_log_debug(AUTHD_LOG, "engine %llu: evaluate mechanisms result %d", engine->engine_index, (int)status);
        
		if (status == errAuthorizationSuccess) {
			credential_t newCred = NULL;
			if (auth_items_exist(engine->context, AGENT_CONTEXT_UID)) {
				newCred = credential_create(auth_items_get_uint(engine->context, AGENT_CONTEXT_UID));
			} else {
				os_log_info(AUTHD_LOG, "Mechanism did not return a uid (engine %llu)", engine->engine_index);
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

        engine->tries++;
        
    } while ( (status == errAuthorizationDenied) // only if we have an expected faulure we continue
             && ((ruleTries == 0) || ((ruleTries > 0) && engine->tries < ruleTries))); // ruleTries == 0 means we try forever
                                                                                       // ruleTires > 0 means we try upto ruleTries times
done:
    return status;
}

static OSStatus
_evaluate_rule(engine_t engine, rule_t rule, bool *save_pwd)
{
    if (rule_check_flags(rule, RuleFlagEntitled)) {
        if (auth_token_has_entitlement_for_right(engine->auth, engine->currentRightName)) {
            os_log_debug(AUTHD_LOG, "engine %llu: rule allow, creator of authorization has entitlement for right %{public}s", engine->engine_index, engine->currentRightName);
            return errAuthorizationSuccess;
        }
    }

	// check apple signature also for every sheet authorization + disable this check for debug builds
    if (engine->la_context || rule_check_flags(rule, RuleFlagRequireAppleSigned)) {
        if (!auth_token_apple_signed(engine->auth)) {
#ifdef NDEBUG
            os_log_error(AUTHD_LOG, "Rule deny, creator of authorization is not signed by Apple (engine %llu)", engine->engine_index);
            return errAuthorizationDenied;
#else
			os_log_debug(AUTHD_LOG, "engine %llu: in release mode, this rule would be denied because creator of authorization is not signed by Apple", engine->engine_index);
#endif
        }
    }
    
    *save_pwd |= rule_get_extract_password(rule);

	switch (rule_get_class(rule)) {
        case RC_ALLOW:
            os_log(AUTHD_LOG, "Rule set to allow (engine %llu)", engine->engine_index);
            return errAuthorizationSuccess;
        case RC_DENY:
            os_log(AUTHD_LOG, "Rule set to deny (engine %llu)", engine->engine_index);
            return errAuthorizationDenied;
        case RC_USER:
            return _evaluate_class_user(engine, rule);
        case RC_RULE:
            return _evaluate_class_rule(engine, rule, save_pwd);
        case RC_MECHANISM:
            return _evaluate_class_mechanism(engine, rule);
        default:
            os_log_error(AUTHD_LOG, "Invalid class for rule or rule not found: %{public}s (engine %llu)", rule_get_name(rule), engine->engine_index);
            return errAuthorizationInternal;
    }
}

// returns true if this rule or its children contain RC_USER rule with password_only==true
static bool
_preevaluate_rule(engine_t engine, rule_t rule, const char **group, bool *sessionOwner, bool *secureToken)
{
	os_log_debug(AUTHD_LOG, "engine %llu: _preevaluate_rule %{public}s", engine->engine_index, rule_get_name(rule));

	switch (rule_get_class(rule)) {
		case RC_ALLOW:
		case RC_DENY:
			return false;
		case RC_USER:
            if (group && !*group) {
                *group = rule_get_group(rule);
            }
            if (sessionOwner) {
                *sessionOwner = rule_get_session_owner(rule);
            }
            if (secureToken) {
                *secureToken = rule_get_securetokenuser(rule);
            }
			return rule_get_password_only(rule);
		case RC_RULE:
			return _preevaluate_class_rule(engine, rule, group, sessionOwner, secureToken);
		case RC_MECHANISM:
			return false;
		default:
			return false;
	}
}

static rule_t
_find_rule(engine_t engine, authdb_connection_t dbconn, const char * string, bool exact_match)
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
        if (exact_match) {
            os_log_error(AUTHD_LOG, "Rule %s not found", string);
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
    
    // set default if we didn't find a rule and we do not require exact match
    if (r == NULL && !exact_match) {
        r = rule_create_with_string("", dbconn);
        if (rule_get_id(r) == 0) {
            CFReleaseNull(r);
            os_log_error(AUTHD_LOG, "Default rule lookup error (missing), using builtin defaults (engine %llu)", engine->engine_index);
            r = rule_create_default();
        }
    }
    return r;
}

static void _parse_environment(engine_t engine, auth_items_t environment)
{
    require(environment != NULL, done);

#if DEBUG
    os_log_debug(AUTHD_LOG, "engine %llu: Dumping Environment: %@", engine->engine_index, environment);
#endif
    const char *ahp = auth_items_get_string(environment, AGENT_CONTEXT_AP_PAM_SERVICE_NAME);

    // Check if a credential was passed into the environment and we were asked to extend the rights
    if (engine->flags & kAuthorizationFlagExtendRights && !(engine->flags & kAuthorizationFlagSheet)) {
        const char * user = auth_items_get_string(environment, kAuthorizationEnvironmentUsername);
        const char * pass = auth_items_get_string(environment, kAuthorizationEnvironmentPassword);
		const bool password_was_used = (ahp == nil || strlen(ahp) == 0); // AGENT_CONTEXT_AP_PAM_SERVICE_NAME in the context means alternative PAM was used
        if (!password_was_used) {
            os_log_debug(AUTHD_LOG, "engine %llu: AHP used in environment: %{public}s", engine->engine_index, ahp);
            if (pass) {
                auth_items_set_string(engine->context, kAuthorizationEnvironmentPassword, pass);
            }
            goto done;
        }

        bool shared = auth_items_exist(environment, kAuthorizationEnvironmentShared);
        require_action(user != NULL, done, os_log_debug(AUTHD_LOG, "engine %llu: user not used password", engine->engine_index));

        struct passwd *pw = getpwnam(user);
        require_action((pw != NULL) || (engine->flags & kAuthorizationFlagSkipInternalAuth), done, os_log_error(AUTHD_LOG, "User not found %{public}s (engine %llu)", user, engine->engine_index));
        
        auth_items_set_string(engine->context, kAuthorizationEnvironmentUsername, user);
        auth_items_set_string(engine->context, kAuthorizationEnvironmentPassword, pass ? pass : "");

        if (user && pass) {
            // we will go through authrorizationhost to evaluate the credentials
            os_log_debug(AUTHD_LOG, "Credentials (%s) passed in the environment (engine %llu)", user, engine->engine_index);
            engine->credentialsProvided = true;
        }
        
        CFStringRef uname = CFStringCreateWithCString(kCFAllocatorDefault, user, kCFStringEncodingUTF8);
        require_action(uname != NULL, done, os_log_debug(AUTHD_LOG, "User name cannot be created (engine %llu)", engine->engine_index));
        int32_t enforced = TKGetSmartcardSettingForUser(kTKEnforceSmartcard, uname);
        CFReleaseSafe(uname);
        require_action(enforced == 0, done, os_log_error(AUTHD_LOG, "User needs to use SmartCard (engine %llu)", engine->engine_index));
        
        if (!(engine->flags & kAuthorizationFlagSkipInternalAuth)) {
            int checkpw_status = checkpw_internal(pw, pass ? pass : "");
            require_action(checkpw_status == CHECKPW_SUCCESS, done, os_log_error(AUTHD_LOG, "engine %llu: checkpw() returned %d; failed to authenticate user %{public}s (uid %u).", engine->engine_index, checkpw_status, pw->pw_name, pw->pw_uid));
            
            credential_t cred = credential_create(pw->pw_uid);
            if (credential_get_valid(cred)) {
                os_log_info(AUTHD_LOG, "checkpw() succeeded, creating credential for user %{public}s (engine %llu)", user, engine->engine_index);
                _engine_set_credential(engine, cred, shared);
            }
            CFReleaseSafe(cred);
        } else {
            os_log_info(AUTHD_LOG, "internal auth skipped user %{public}s (engine %llu)", user, engine->engine_index);
        }
    }
    
done:
    endpwent();
    return;
}

static bool _verify_sandbox(engine_t engine, const char * right)
{
    pid_t pid = process_get_pid(engine->proc);
    if (sandbox_check_by_audit_token(process_get_audit_info(engine->proc)->opaqueToken, "authorization-right-obtain", SANDBOX_FILTER_RIGHT_NAME, right)) {
        os_log_error(AUTHD_LOG, "Sandbox denied authorizing right '%{public}s' by client '%{public}s' [%d] (engine %llu)", right, process_get_code_url(engine->proc), pid, engine->engine_index);
        return false;
    }
    
    pid = auth_token_get_pid(engine->auth);
    if (auth_token_get_sandboxed(engine->auth) && sandbox_check_by_audit_token(auth_token_get_audit_info(engine->auth)->opaqueToken, "authorization-right-obtain", SANDBOX_FILTER_RIGHT_NAME, right)) {
        os_log_error(AUTHD_LOG, "Sandbox denied authorizing right '%{public}s' for authorization created by '%{public}s' [%d] (engine %llu)", right, auth_token_get_code_url(engine->auth), pid, engine->engine_index);
        return false;
    }
    
    return true;
}

#pragma mark -
#pragma mark engine methods

OSStatus engine_get_right_properties(engine_t engine, const char *rightName, CFDictionaryRef *output)
{
    OSStatus status = errAuthorizationInternal;
    
    CFMutableDictionaryRef properties = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require(properties != NULL, done);
    require(rightName != NULL, done);

    // first check if any of rights uses rule with password-only set to true
    // if so, set appropriate hint so SecurityAgent won't use alternate authentication methods like smartcard etc.
    authdb_connection_t dbconn = authdb_connection_acquire(server_get_database()); // get db handle
    
    os_log_debug(AUTHD_LOG, "engine %llu: get right properties %{public}s", engine->engine_index, rightName);
    rule_t rule = _find_rule(engine, dbconn, rightName, false);
    
    if (rule) {
        const char *group = NULL;
        bool sessionOwner = false;
        bool secureToken = false;
        bool passwordOnly = _preevaluate_rule(engine, rule, &group, &sessionOwner, &secureToken);
        CFDictionarySetValue(properties, CFSTR(kAuthorizationRuleParameterPasswordOnly), passwordOnly ? kCFBooleanTrue : kCFBooleanFalse);
        CFDictionarySetValue(properties, CFSTR(kAuthorizationSessionOwnerAccepted), sessionOwner ? kCFBooleanTrue : kCFBooleanFalse);
        CFDictionarySetValue(properties, CFSTR(kAuthorizationRuleParameterSecureTokenOnly), secureToken ? kCFBooleanTrue : kCFBooleanFalse);

        if (group) {
            CFStringRef groupCf = CFStringCreateWithCString(kCFAllocatorDefault, group, kCFStringEncodingUTF8);
            if (groupCf) {
                CFDictionarySetValue(properties, CFSTR(kAuthorizationRuleParameterGroup), groupCf);
                CFRelease(groupCf);
            }
        }
        CFRelease(rule);
        status = errAuthorizationSuccess;
    } else {
        os_log_debug(AUTHD_LOG, "engine %llu: cannot get right properties %{public}s", engine->engine_index, rightName);
        status = errAuthorizationInvalidRef;
    }
    
    authdb_connection_release(&dbconn); // release db handle
    os_log_debug(AUTHD_LOG, "engine %llu: right properties %@", engine->engine_index, properties);

    if (output && status == errAuthorizationSuccess) {
        *output = properties;
        CFRetain(*output);
    }
done:
    CFReleaseSafe(properties);
    return status;
}

OSStatus engine_authorize(engine_t engine, auth_rights_t rights, auth_items_t environment, AuthorizationFlags flags)
{
    __block OSStatus status = errAuthorizationSuccess;
    __block bool save_password = false;
	__block bool password_only = false;
    __block bool forbiddenElevationRightFound = false;
    CFIndex rights_count = rights ? auth_rights_get_count(rights) : 0;
    __block CFIndex evaluated_rights_count = 0;
    ccaudit_t ccaudit = NULL;
    ess_authorization_result_t ess_results[rights_count ? :1];
    __block ess_authorization_result_t *ess_current_result = &ess_results[0];
    char const *right_names[rights_count ? :1];
    __block char const **current_right = &right_names[0];
    __block audit_token_t instigator_token = INVALID_AUDIT_TOKEN_VALUE;
    __block audit_token_t petitioner_token = INVALID_AUDIT_TOKEN_VALUE;
    __block AuthorizationFlags localFlags = flags;
    __block authdb_t memory_db = NULL;
    
    require(rights != NULL, done);
    
    CFStringRef (^copyAnalyticsText)(bool, void *) = ^CFStringRef(bool extended, void *input) {
        char *(^classDescription)(ess_authorization_rule_class_t) = ^char *(ess_authorization_rule_class_t ruleClass) {
            switch (ruleClass) {
                case ESS_AUTHORIZATION_RULE_CLASS_USER:
                    return "USER";
                case ESS_AUTHORIZATION_RULE_CLASS_RULE:
                    return "RULE";
                case ESS_AUTHORIZATION_RULE_CLASS_MECHANISM:
                    return "MECHANISM";
                case ESS_AUTHORIZATION_RULE_CLASS_ALLOW:
                    return "ALLOW";
                case ESS_AUTHORIZATION_RULE_CLASS_DENY:
                    return "DENY";
                default:
                    return "UNKNOWN";
            }
        };
        
        CFStringRef insText;
        audit_token_t invalidToken = INVALID_AUDIT_TOKEN_VALUE;
        if (memcmp(&instigator_token, &invalidToken, sizeof(audit_token_t)) == 0) {
            insText = CFStringCreateCopy(kCFAllocatorDefault, CFSTR("invalid"));
        } else {
            insText = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%d (%d)"), instigator_token.val[5], instigator_token.val[6]);
        }
        CFStringRef petText;
        if (memcmp(&petitioner_token, &invalidToken, sizeof(audit_token_t)) == 0) {
            petText = CFStringCreateCopy(kCFAllocatorDefault, CFSTR("invalid"));
        } else {
            petText = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%d (%d)"), petitioner_token.val[5], petitioner_token.val[6]);
        }
        
        CFMutableStringRef rightsText = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, CFSTR("("));
        if (extended) {
            ess_authorization_result_t *current_item = input;
            for (CFIndex i = 0; i < evaluated_rights_count; ++i) {
                CFStringAppendFormat(rightsText, NULL, CFSTR("%s (class %s): %s"), current_item->right_name, classDescription(current_item->rule_class), current_item->granted ? "granted" : "not granted");
                if (rights_count - i > 1) {
                    CFStringAppend(rightsText, CFSTR(", "));
                }
                ++current_item;
            }
        } else {
            char const **current_item = input;
            for (CFIndex i = 0; i < rights_count; ++i) {
                CFStringAppendFormat(rightsText, NULL, CFSTR("%s"), *current_item);
                if (rights_count - i > 1) {
                    CFStringAppend(rightsText, CFSTR(", "));
                }
                ++current_item;
            }
        }

        CFStringAppend(rightsText, CFSTR(")"));
        CFStringRef retval;
        if (extended) {
            retval = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("Result: instigator %@, petitioner %@, status %d, %ld %s %@"), insText, petText, status, (long)evaluated_rights_count, evaluated_rights_count == 1 ? "right":"rights", rightsText);
        } else {
            retval = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("Request: instigator %@, petitioner %@, flags %08x, %ld %s %@"), insText, petText, localFlags, (long)rights_count, rights_count == 1 ? "right":"rights", rightsText);
        }
        CFReleaseSafe(insText);
        CFReleaseSafe(petText);
        CFReleaseSafe(rightsText);
        return retval;
    };
    
    ccaudit = ccaudit_create(engine->proc, engine->auth, AUE_ssauthorize);
    if (rights_count) {
        ccaudit_log(ccaudit, "begin evaluation", NULL, 0);
        __block CFMutableArrayRef rights_list = NULL;
        rights_list = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
        auth_rights_iterate(rights, ^bool(const char *key) {
            if (!key)
                return true;
            
            *current_right = key;
            current_right++;
            // AuthorizationExecuteWithPrivileges                           // SMJobBless
            if ((strcmp(key, "system.privilege.admin") == 0) || (strcmp(key, "com.apple.ServiceManagement.blesshelper") == 0)) {
                forbiddenElevationRightFound = true;
            }
            if ((strcmp(key, "system.login.screensaver") == 0) || (strcmp(key, "system.login.screensaver.unlock") == 0)) {
                localFlags |= kAuthorizationFlagSkipInternalAuth;
            }
            
            CFStringRef tmp = CFStringCreateWithCString(kCFAllocatorDefault, key, kCFStringEncodingUTF8);
            if (tmp) {
                CFArrayAppendValue(rights_list, tmp);
                CFRelease(tmp);
            } else {
                os_log_error(AUTHD_LOG, "Unable to get details for right %{public}s", key);
            }
            return true;
        });
        
        os_log_info(AUTHD_LOG, "Process %{public}s (PID %d) evaluates %ld rights with flags %08x (engine %llu, token %llu): %{public}@", process_get_code_url(engine->proc), process_get_pid(engine->proc), (long)rights_count, localFlags, engine->engine_index, auth_token_get_index(engine->auth), rights_list);
        CFReleaseNull(rights_list);
        
        if (engine->proc) {
            const audit_info_s *info = process_get_audit_info(engine->proc);
            if (info) {
                instigator_token = info->opaqueToken;
            }
        }
        
        if (engine->auth) {
            const audit_info_s *info = auth_token_get_audit_info(engine->auth);
            if (info) {
                petitioner_token = info->opaqueToken;
            }
        }
        
        __attribute__((weak_import)) typeof(ess_notify_authorization_petition) ess_notify_authorization_petition;
        if (ess_notify_authorization_petition != NULL) {
            ess_notify_authorization_petition(instigator_token, petitioner_token, localFlags, rights_count, right_names);
        }
        CFStringRef analyticsText = copyAnalyticsText(false, &right_names[0]);
        os_log_info(AUTHD_ANLOG_DEFAULT(), "%@", analyticsText);
        CFReleaseNull(analyticsText);
    }
    
	if (!auth_token_apple_signed(engine->auth) && (localFlags & kAuthorizationFlagSheet)) {
#ifdef NDEBUG
        os_log_error(AUTHD_LOG, "engine %llu: extra flags are ommited as creator is not signed by Apple", engine->engine_index);
		flags &= ~kAuthorizationFlagSheet;
#else
		os_log_debug(AUTHD_LOG, "engine %llu: in release mode, extra flags would be ommited as creator is not signed by Apple", engine->engine_index);
#endif
	}

    engine->flags = localFlags;

    // Restore all context values from the AuthorizationRef
    auth_items_t decrypted_items = auth_items_create();
    require_action(decrypted_items != NULL, done, os_log_error(AUTHD_LOG, "Unable to create items (engine %llu)", engine->engine_index));
    auth_items_content_copy(decrypted_items, auth_token_get_context(engine->auth));
    auth_items_decrypt(decrypted_items, auth_token_get_encryption_key(engine->auth));
    auth_items_copy(engine->context, decrypted_items);
    CFReleaseSafe(decrypted_items);
    
    // remove all values not relevant for authentication from the context as they are not needed anymore
    auth_items_remove(engine->context, AGENT_CONTEXT_AP_PAM_SERVICE_NAME);
    auth_items_remove(engine->context, AGENT_CONTEXT_AP_USER_NAME);
    auth_items_remove(engine->context, AGENT_CONTEXT_AP_TOKEN);
    auth_items_remove(engine->context, AGENT_CONTEXT_AP_PAM_ERROR_MESSAGE);
    auth_items_remove(engine->context, AGENT_CONTEXT_UID);
    auth_items_remove(engine->context, kAuthorizationEnvironmentUsername);
    auth_items_remove(engine->context, kAuthorizationEnvironmentPassword);
    auth_items_remove(engine->context, AGENT_CONTEXT_AUTHENTICATION_FAILURE);
    auth_items_remove(engine->context, kAuthorizationPamResult);
    
    if (environment) {
        _parse_environment(engine, environment);
        auth_items_copy(engine->hints, environment);
    }
    
    if (forbiddenElevationRightFound) {
        os_log(AUTHD_LOG, "Removing all credentials-related hints (engine %lld)", engine->engine_index);
        auth_items_set_bool(engine->immutable_hints, AGENT_HINT_TOUCHID_PROHIBITED, true);
    }
    
    // move sheet-specific items from hints to context
    const char *user = environment ? auth_items_get_string(environment, kAuthorizationEnvironmentUsername) : NULL;
    const char *service = auth_items_get_string(engine->hints, AGENT_CONTEXT_AP_PAM_SERVICE_NAME);
    if (service) {
        if (auth_items_exist(engine->hints, AGENT_CONTEXT_AP_USER_NAME)) {
            auth_items_set_string(engine->context, AGENT_CONTEXT_AP_USER_NAME, auth_items_get_string(engine->hints, AGENT_CONTEXT_AP_USER_NAME));
            auth_items_remove(engine->hints, AGENT_CONTEXT_AP_USER_NAME);
        } else if (user) {
            auth_items_set_string(engine->context, AGENT_CONTEXT_AP_USER_NAME, user);
        }

        auth_items_set_string(engine->context, AGENT_CONTEXT_AP_PAM_SERVICE_NAME, service);
        auth_items_remove(engine->hints, AGENT_CONTEXT_AP_PAM_SERVICE_NAME);
        engine->credentialsProvided = true;
        os_log_debug(AUTHD_LOG, "Credentials passed in the environment (engine %llu)", engine->engine_index);
    } else {
        auth_items_remove(engine->context, AGENT_CONTEXT_AP_PAM_SERVICE_NAME);
        auth_items_remove(engine->context, AGENT_CONTEXT_AP_USER_NAME);
    }
    
    if (engine->credentialsProvided && !auth_items_exist(engine->context, AGENT_CONTEXT_UID)) {
        struct passwd *pw = getpwnam(user);
        if (pw) {
            auth_items_set_uint(engine->context, AGENT_CONTEXT_UID, pw->pw_uid);
            os_log_debug(AUTHD_LOG, "Credentials UID %d set.", pw->pw_uid);
        } else {
            os_log_error(AUTHD_LOG, "Credentials UID not found for %s", user);
        }
    }

    if (environment && auth_items_exist(environment, AGENT_CONTEXT_AP_TOKEN)) {
        size_t datalen = 0;
        const void *data = auth_items_get_data(engine->hints, AGENT_CONTEXT_AP_TOKEN, &datalen);
        if (data) {
            auth_items_set_data(engine->context, AGENT_CONTEXT_AP_TOKEN, data, datalen);
        }
        auth_items_remove(engine->hints, AGENT_CONTEXT_AP_TOKEN);
    }
    
	if (engine->flags & kAuthorizationFlagSheet) {
        // Try to use/update fresh context values from the environment
        require_action(environment, done, os_log_error(AUTHD_LOG, "Missing environment for sheet authorization (engine %llu)", engine->engine_index); status = errAuthorizationDenied);
            
		require_action(user, done, os_log_error(AUTHD_LOG, "Missing username (engine %llu)", engine->engine_index); status = errAuthorizationDenied);

		auth_items_set_string(engine->context, kAuthorizationEnvironmentUsername, user);
		struct passwd *pwd = getpwnam(user);
		require_action(pwd, done, os_log_error(AUTHD_LOG, "Invalid username %s (engine %llu)", user, engine->engine_index); status = errAuthorizationDenied);
		auth_items_set_uint(engine->context, "sheet-uid", pwd->pw_uid);

		engine_acquire_sheet_data(engine);
		_extract_password_from_la(engine);
        engine->credentialsProvided = true;
        os_log_debug(AUTHD_LOG, "Credentials passed by LACtx (engine %llu)", engine->engine_index);
	}
    
    engine->dismissed = false;
    auth_rights_clear(engine->grantedRights);

	if (!(engine->flags & kAuthorizationFlagIgnorePasswordOnly))
	{
		// first check if any of rights uses rule with password-only set to true
		// if so, set appropriate hint so SecurityAgent won't use alternate authentication methods like smartcard etc.
		authdb_connection_t dbconn = authdb_connection_acquire(server_get_database()); // get db handle
		auth_rights_iterate(rights, ^bool(const char *key) {
			if (!key)
				return true;
			os_log_debug(AUTHD_LOG, "engine %llu: checking if rule %{public}s contains password-only item", engine->engine_index, key);

            rule_t rule = _find_rule(engine, dbconn, key, false);
			if (rule && _preevaluate_rule(engine, rule, NULL, NULL, NULL)) {
				password_only = true;
                CFReleaseSafe(rule);
				return false;
			}
            CFReleaseSafe(rule);
			return true;
		});
		authdb_connection_release(&dbconn); // release db handle
	} else {
		os_log_info(AUTHD_LOG, "Password-only flag ignored (engine %llu)", engine->engine_index);
	}

	if (password_only) {
		os_log_debug(AUTHD_LOG, "engine %llu: password-only item found, forcing SecurityAgent to use password-only UI", engine->engine_index);
		auth_items_set_bool(engine->immutable_hints, AGENT_HINT_PASSWORD_ONLY, true);
	}

    auth_rights_iterate(rights, ^bool(const char *key) {
        void (^add_analytics_result)(void) = ^void(void) {
            ess_current_result->granted = (status == errAuthorizationSuccess);
            if (engine->currentRule) {
                switch (rule_get_class(engine->currentRule)) {
                    case RC_DENY: ess_current_result->rule_class = ESS_AUTHORIZATION_RULE_CLASS_DENY;break;
                    case RC_ALLOW: ess_current_result->rule_class = ESS_AUTHORIZATION_RULE_CLASS_ALLOW;break;
                    case RC_USER: ess_current_result->rule_class = ESS_AUTHORIZATION_RULE_CLASS_USER;break;
                    case RC_MECHANISM: ess_current_result->rule_class = ESS_AUTHORIZATION_RULE_CLASS_MECHANISM;break;
                    case RC_RULE: ess_current_result->rule_class = ESS_AUTHORIZATION_RULE_CLASS_RULE;break;
                    default: ess_current_result->rule_class = ESS_AUTHORIZATION_RULE_CLASS_INVALID;break;
                }
                ess_current_result->right_name = engine->currentRightName;
            } else {
                ess_current_result->right_name = key;
                ess_current_result->rule_class = ESS_AUTHORIZATION_RULE_CLASS_UNKNOWN;  // right is not fetched yet
            }
            ++ess_current_result;
        };
        
        if (!key)
            return true;

        ++evaluated_rights_count;
        if (!_verify_sandbox(engine, key)) { // _verify_sandbox is already logging failures
            status = errAuthorizationDenied;
            add_analytics_result();
            return false;
        }
        
        authdb_connection_t dbconn = authdb_connection_acquire(server_get_database()); // get db handle
        
        os_log_debug(AUTHD_LOG, "engine %llu: evaluate right %{public}s", engine->engine_index, key);
        rule_t rule = NULL;
        if (memory_db || (_in_system_update() && strcmp(key, "system.login.console") == 0)) {
            authdb_connection_t dbconn_mem = NULL;
            if (!memory_db) {
                memory_db = authdb_create(true);
            }
            
            if (memory_db) {
                dbconn_mem = authdb_connection_acquire(memory_db);
                if (dbconn_mem) {
                    authdb_maintenance(dbconn_mem);
                    rule = _find_rule(engine, dbconn_mem, key, true);
                }
            }
            if (rule) {
                os_log(AUTHD_LOG, "Original system.login.console from the plist used");
            } else {
                os_log_error(AUTHD_LOG, "Failed to use system.login.console from the plist, fallback to the local database");
            }
            CFReleaseNull(dbconn_mem);
        }
        if (!rule) {
            rule = _find_rule(engine, dbconn, key, false);
        }
        const char * rule_name = rule_get_name(rule);
        if (rule_name && (strcasecmp(rule_name, "") == 0)) {
            rule_name = "default (not defined)";
        }
        os_log_debug(AUTHD_LOG, "engine %llu: using rule %{public}s", engine->engine_index, rule_name);

        _set_right_hints(engine->hints, key); // set authorization right name for authorizationhost as well
        auth_items_set_int64(engine->hints, kAuthorizationFlags, flags);

        // only need the hints & mechanisms if we are going to show ui
        if (engine->flags & kAuthorizationFlagInteractionAllowed || engine->credentialsProvided) {
            os_log_debug(AUTHD_LOG, "setting hints for UI authorization");
            _set_localization_hints(dbconn, engine->hints, rule);
            if (!engine->authenticateRule) {
                engine->authenticateRule = rule_create_with_string("authenticate", dbconn);
            }
        }
        
        authdb_connection_release(&dbconn); // release db handle
        
        engine->currentRightName = key;
        engine->currentRule = rule;
        
        ccaudit_log(ccaudit, key, rule_name, 0);
        
        status = _evaluate_rule(engine, engine->currentRule, &save_password);
        switch (status) {
            case errAuthorizationSuccess:
                auth_rights_add(engine->grantedRights, key);
                auth_rights_set_flags(engine->grantedRights, key, auth_rights_get_flags(rights, key));
                
                if ((engine->flags & kAuthorizationFlagPreAuthorize) &&
                    (rule_get_class(engine->currentRule) == RC_USER) &&
                    (rule_get_timeout(engine->currentRule) == 0)) {
                    // FIXME: kAuthorizationFlagPreAuthorize => kAuthorizationFlagCanNotPreAuthorize ???
                    auth_rights_set_flags(engine->grantedRights, engine->currentRightName, kAuthorizationFlagPreAuthorize);
                }
                
                os_log(AUTHD_LOG, "Succeeded authorizing right '%{public}s' by client '%{public}s' [%d] for authorization created by '%{public}s' [%d] (%X,%d) (engine %llu)",
                    key, process_get_code_url(engine->proc), process_get_pid(engine->proc),
                    auth_token_get_code_url(engine->auth), auth_token_get_pid(engine->auth), (unsigned int)engine->flags, auth_token_least_privileged(engine->auth), engine->engine_index);
                break;
            case errAuthorizationDenied:
            case errAuthorizationInteractionNotAllowed:
            case errAuthorizationCanceled:
                if (engine->flags & kAuthorizationFlagInteractionAllowed) {
                    os_log(AUTHD_LOG, "Failed to authorize right '%{public}s' by client '%{public}s' [%d] for authorization created by '%{public}s' [%d] (%X,%d) (%i) (engine %llu)",
                        key, process_get_code_url(engine->proc), process_get_pid(engine->proc),
                        auth_token_get_code_url(engine->auth), auth_token_get_pid(engine->auth), (unsigned int)engine->flags, auth_token_least_privileged(engine->auth), (int)status,
                        engine->engine_index);
                } else {
                    os_log_debug(AUTHD_LOG, "Failed to authorize right '%{public}s' by client '%{public}s' [%d] for authorization created by '%{public}s' [%d] (%X,%d) (%d) (engine %llu)",
                        key, process_get_code_url(engine->proc), process_get_pid(engine->proc),
                        auth_token_get_code_url(engine->auth), auth_token_get_pid(engine->auth), (unsigned int)engine->flags, auth_token_least_privileged(engine->auth), (int)status,
                        engine->engine_index);
                }
                break;
            default:
                os_log_error(AUTHD_LOG, "Evaluate returned %d, returning errAuthorizationInternal (engine %llu)", (int)status, engine->engine_index);
                status = errAuthorizationInternal;
                break;
        }

        add_analytics_result();
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
    
    // call endpoint security analytics
    __attribute__((weak_import)) typeof(ess_notify_authorization_judgement) ess_notify_authorization_judgement;
    if (rights_count) {
        if (ess_notify_authorization_judgement) {
            ess_notify_authorization_judgement(instigator_token, petitioner_token, status, evaluated_rights_count, ess_results);
        }
        CFStringRef analyticsText = copyAnalyticsText(true, &ess_results[0]);
        os_log_info(AUTHD_ANLOG_DEFAULT(), "%@", analyticsText);
        CFReleaseSafe(analyticsText);
    }

	if (password_only) {
		os_log_debug(AUTHD_LOG, "engine %llu: removing password-only flag", engine->engine_index);
		auth_items_remove(engine->immutable_hints, AGENT_HINT_PASSWORD_ONLY);
	}
    
    if ((engine->flags & kAuthorizationFlagPartialRights) && (auth_rights_get_count(engine->grantedRights) > 0)) {
        status = errAuthorizationSuccess;
    }
    
    if (engine->dismissed) {
		os_log_error(AUTHD_LOG, "Caller dismissed (engine %llu)", engine->engine_index);
        status = errAuthorizationDenied;
    }
    
    const char *reqScUser = auth_items_get_string(engine->context, AGENT_CONTEXT_SC_REQUIRED_USER);
    if ((status == errAuthorizationSuccess) && reqScUser) {
        os_log(AUTHD_LOG, "Password only prompt for SmartCard-enforced user %s, PIN prompt will follow", reqScUser);
        AuthorizationRef scAuthorizationRef;
        status = AuthorizationCreate(NULL, kAuthorizationEmptyEnvironment, kAuthorizationFlagDefaults, &scAuthorizationRef);
        if (status != errAuthorizationSuccess) {
            os_log_error(AUTHD_LOG, "Unable to create SC only authorization: %d", (int)status);
        } else {
            auth_items_remove(engine->context, AGENT_CONTEXT_SC_REQUIRED_USER);
            status = errSecInternalError;
            auth_token_t scAuth = NULL;
            auth_rights_t scRights = NULL;
            auth_items_t scEnvironment = NULL;
            connection_t scConn = NULL;
            
            scConn = connection_create(engine->proc);
            scAuth = auth_token_create(engine->proc, kAuthorizationFlagDefaults);
            require_action(scAuth != NULL, scDone, os_log_error(AUTHD_LOG, "Unable to create scAuth"));
            
            scRights = auth_rights_create();
            require_action(scRights != NULL, scDone, os_log_error(AUTHD_LOG, "Unable to create rights"));
            auth_rights_add(scRights, "com.apple.auth.user");
            
            scEnvironment = auth_items_create();
            require_action(scEnvironment != NULL, scDone, os_log_error(AUTHD_LOG, "Unable to create env"));
            
            auth_items_set_string(scEnvironment, AGENT_HINT_REQUIRED_USER, reqScUser);
            
            status = server_authorize(scConn, scAuth, kAuthorizationFlagInteractionAllowed | kAuthorizationFlagExtendRights, scRights, scEnvironment, NULL);
            if (status != errAuthorizationSuccess) {
                os_log_error(AUTHD_LOG, "SC authorization failed: %d", (int)status);
            }
        scDone:
            CFReleaseSafe(scAuth);
            CFReleaseSafe(scRights);
            CFReleaseSafe(scEnvironment);
            CFReleaseSafe(scConn);
        }
    }
    os_log_debug(AUTHD_LOG, "engine %llu: authorize result: %d", engine->engine_index, (int)status);

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
                os_log_debug(AUTHD_LOG, "engine %llu: adding least privileged %{public}scredential %{public}s to authorization", engine->engine_index, credential_get_shared(cred) ? "shared " : "", credential_get_name(cred));
            } else {
                os_log_debug(AUTHD_LOG, "engine %llu: adding %{public}scredential %{public}s (%i) to authorization", engine->engine_index, credential_get_shared(cred) ? "shared " : "", credential_get_name(cred), credential_get_uid(cred));
            }
            return true;
        });
    }

    if (status == errAuthorizationSuccess && save_password) {
        auth_items_set_flags(engine->context, kAuthorizationEnvironmentPassword, kAuthorizationContextFlagExtractable);
    } else if (engine->credentialsProvided) { // for prefilled credentials, errors must be exported as well
        auth_items_set_flags(engine->context, kAuthorizationEnvironmentUsername, kAuthorizationContextFlagExtractable);
        auth_items_set_flags(engine->context, kAuthorizationEnvironmentPassword, kAuthorizationContextFlagExtractable);
        auth_items_set_flags(engine->context, AGENT_CONTEXT_AP_USER_NAME, kAuthorizationContextFlagExtractable);
        auth_items_set_flags(engine->context, AGENT_CONTEXT_AP_PAM_SERVICE_NAME, kAuthorizationContextFlagExtractable);
        auth_items_set_flags(engine->context, AGENT_CONTEXT_AUTHENTICATION_FAILURE, kAuthorizationContextFlagExtractable);
        auth_items_set_flags(engine->context, AGENT_CONTEXT_AP_PAM_ERROR_MESSAGE, kAuthorizationContextFlagExtractable);
        auth_items_set_flags(engine->context, kAuthorizationPamResult, kAuthorizationContextFlagExtractable);
    }

    if ((status == errAuthorizationSuccess) || (status == errAuthorizationCanceled) || (engine->credentialsProvided)) {
		auth_items_t encrypted_items = auth_items_create();
		require_action(encrypted_items != NULL, done, os_log_error(AUTHD_LOG, "Unable to create items (engine %llu)", engine->engine_index));
		auth_items_content_copy_with_flags(encrypted_items, engine->context, kAuthorizationContextFlagExtractable);
#if DEBUG
		os_log_debug(AUTHD_LOG,"engine %llu: ********** Dumping context for encryption **********", engine->engine_index);
		os_log_debug(AUTHD_LOG, "%@", encrypted_items);
#endif
		auth_items_encrypt(encrypted_items, auth_token_get_encryption_key(engine->auth));
		auth_items_copy_with_flags(auth_token_get_context(engine->auth), encrypted_items, kAuthorizationContextFlagExtractable);
		os_log_debug(AUTHD_LOG, "engine %llu: encrypted authorization context data", engine->engine_index);
        CFReleaseSafe(encrypted_items);
    }
    
    if (auth_rights_get_count(rights) > 0) {
        ccaudit_log(ccaudit, "end evaluation", NULL, status);
    }
    
#if DEBUG
    os_log_debug(AUTHD_LOG, "engine %llu: ********** Dumping auth->credentials **********", engine->engine_index);
    auth_token_credentials_iterate(engine->auth, ^bool(credential_t cred) {
		os_log_debug(AUTHD_LOG, "%@", cred);
		return true;
    });
    os_log_debug(AUTHD_LOG, "engine %llu: ********** Dumping session->credentials **********", engine->engine_index);
    session_credentials_iterate(auth_token_get_session(engine->auth), ^bool(credential_t cred) {
		os_log_debug(AUTHD_LOG, "%@", cred);
        return true;
    });
    os_log_debug(AUTHD_LOG, "engine %llu: ********** Dumping engine->context **********", engine->engine_index);
	os_log_debug(AUTHD_LOG, "%@", engine->context);
    os_log_debug(AUTHD_LOG, "engine %llu: ********** Dumping auth->context **********", engine->engine_index);
	os_log_debug(AUTHD_LOG, "%@", engine->auth);
    os_log_debug(AUTHD_LOG, "engine %llu: ********** Dumping granted rights **********", engine->engine_index);
	os_log_debug(AUTHD_LOG, "%@", engine->grantedRights);
#endif
    
done:
    CFReleaseSafe(memory_db);
    auth_items_clear(engine->context);
    auth_items_clear(engine->sticky_context);
    CFReleaseSafe(ccaudit);
    CFRetain(engine);
    dispatch_sync(dispatch_get_main_queue(), ^{
        CFDictionaryRemoveAllValues(engine->mechanism_agents);
        CFRelease(engine);
    });
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
    
    rule = _find_rule(engine, dbconn, right, false);
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

    require_action(right[len-1] != '.', done, os_log_error(AUTHD_LOG, "Not allowed to set wild card rules (engine %llu)", engine->engine_index));

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
    status = engine_authorize(engine, checkRight, kAuthorizationEmptyEnvironment, kAuthorizationFlagDefaults | kAuthorizationFlagInteractionAllowed | kAuthorizationFlagExtendRights);

done:
    os_log_debug(AUTHD_LOG, "engine %llu: authorizing %{public}s for db modification: %d", engine->engine_index, right, (int)status);
    CFReleaseSafe(checkRight);
    return status;
}

void
_engine_set_credential(engine_t engine, credential_t cred, bool shared)
{
    os_log_debug(AUTHD_LOG, "engine %llu: adding %{public}scredential %{public}s (%i) to engine shared: %i", engine->engine_index, credential_get_shared(cred) ? "shared " : "", credential_get_name(cred), credential_get_uid(cred), shared);
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
    CFRetain(engine);
    dispatch_sync(dispatch_get_main_queue(), ^{
        _cf_dictionary_iterate(engine->mechanism_agents, ^bool(CFTypeRef key __attribute__((__unused__)), CFTypeRef value) {
            os_log_debug(AUTHD_LOG, "engine %llu: destroying %{public}s", engine->engine_index, mechanism_get_string((mechanism_t)key));
            agent_t agent = (agent_t)value;
            agent_destroy(agent);
            
            return true;
        });
        CFRelease(engine);
    });
}

void engine_interrupt_agent(engine_t engine)
{
    CFRetain(engine);
    dispatch_sync(dispatch_get_main_queue(), ^{
        _cf_dictionary_iterate(engine->mechanism_agents, ^bool(CFTypeRef key __attribute__((__unused__)), CFTypeRef value) {
            agent_t agent = (agent_t)value;
            agent_notify_interrupt(agent);
            return true;
        });
        CFRelease(engine);
    });
}

CFTypeRef engine_copy_context(engine_t engine, auth_items_t source)
{
	CFTypeRef retval = NULL;

	process_t proc = connection_get_process(engine->conn);
	if (!proc) {
		os_log_error(AUTHD_LOG, "No client process (engine %llu)", engine->engine_index);
		return retval;
	}

	uid_t client_uid = process_get_uid(proc);
	if (!client_uid) {
		os_log_error(AUTHD_LOG, "No client UID (engine %llu)", engine->engine_index);
		return retval;
	}

	size_t dataLen = 0;
	const void *data = auth_items_get_data(source, AGENT_HINT_SHEET_CONTEXT, &dataLen);
	if (data) {
		CFDataRef externalized = CFDataCreate(kCFAllocatorDefault, data, dataLen);
		if (externalized) {
			os_log_debug(AUTHD_LOG, "engine %llu: going to get LA context for UID %d", engine->engine_index, client_uid);
			retval = LACreateNewContextWithACMContextInSession(client_uid, externalized, NULL);
			CFRelease(externalized);
		}
	}

	return retval;
}

bool engine_acquire_sheet_data(engine_t engine)
{
	if (!auth_items_exist(engine->context, "sheet-uid"))
		return false;

	uid_t uid = auth_items_get_uint(engine->context, "sheet-uid");

	CFReleaseSafe(engine->la_context);
	engine->la_context = engine_copy_context(engine, engine->hints);
	if (engine->la_context) {
		os_log_debug(AUTHD_LOG, "engine %llu: Sheet UID %d", engine->engine_index, uid);
		return true;
	} else {
		// this is not real failure as no LA context in authorization context is very valid scenario
		os_log_debug(AUTHD_LOG, "engine %llu: Failed to get LA context", engine->engine_index);
	}
	return false;
}

auth_token_t engine_get_auth_token(engine_t engine)
{
    return engine->auth;
}
