/* Copyright (c) 2012 Apple Inc. All rights reserved. */

#include "rule.h"
#include "authutilities.h"
#include "mechanism.h"
#include "crc.h"
#include "debugging.h"
#include "authitems.h"
#include "process.h"

#include <Security/AuthorizationDB.h>
#include <Security/AuthorizationTagsPriv.h>
#include "server.h"

static void _sql_get_id(rule_t,authdb_connection_t);
static RuleClass _get_cf_rule_class(CFTypeRef);
static bool _copy_cf_rule_mechanisms(rule_t,CFTypeRef,authdb_connection_t);
static bool _copy_cf_rule_delegations(rule_t, CFTypeRef,authdb_connection_t);

#define kMaximumAuthorizationTries 10000

#define RULE_ID "id"
#define RULE_NAME "name"
#define RULE_TYPE "type"
#define RULE_CLASS "class"
#define RULE_GROUP "group"
#define RULE_KOFN   "kofn"
#define RULE_TIMEOUT "timeout"
#define RULE_FLAGS "flags"
#define RULE_TRIES "tries"
#define RULE_COMMENT "comment"
#define RULE_VERSION "version"
#define RULE_CREATED "created"
#define RULE_MODIFIED "modified"
#define RULE_IDENTIFIER "identifier"
#define RULE_REQUIREMENT "requirement"
#define RULE_HASH "hash"

struct _rule_s {
    __AUTH_BASE_STRUCT_HEADER__;

    auth_items_t data;
    CFMutableArrayRef mechanisms;
    CFMutableArrayRef delegations;
    
    CFMutableDictionaryRef loc_prompts;
    CFMutableDictionaryRef loc_buttons;
    
    CFDataRef requirement_data;
    SecRequirementRef requirement;
};

static void
_rule_finalize(CFTypeRef value)
{
    rule_t rule = (rule_t)value;
    CFReleaseSafe(rule->data);
    CFReleaseSafe(rule->mechanisms);
    CFReleaseSafe(rule->delegations);
    CFReleaseSafe(rule->loc_prompts);
    CFReleaseSafe(rule->loc_buttons);
    CFReleaseSafe(rule->requirement_data);
    CFReleaseSafe(rule->requirement);
}

static Boolean
_rule_equal(CFTypeRef value1, CFTypeRef value2)
{
    rule_t rule1 = (rule_t)value1;
    rule_t rule2 = (rule_t)value2;
    
    return strcasecmp(rule_get_name(rule1), rule_get_name(rule2)) == 0;
}

static CFStringRef
_rule_copy_description(CFTypeRef value)
{
    rule_t rule = (rule_t)value;
    CFMutableStringRef str = CFStringCreateMutable(kCFAllocatorDefault, 0);
    CFStringRef tmp = CFCopyDescription(rule->data);
    CFStringAppend(str, tmp);
    CFReleaseNull(tmp);
    tmp = CFCopyDescription(rule->mechanisms);
    CFStringAppend(str, tmp);
    CFReleaseNull(tmp);
    tmp = CFCopyDescription(rule->delegations);
    CFStringAppend(str, tmp);
    CFReleaseNull(tmp);
    return str;
}

static CFHashCode
_rule_hash(CFTypeRef value)
{
    rule_t rule = (rule_t)value;
    const char * str = rule_get_name(rule);
    return crc64(str, strlen(str));
}

AUTH_TYPE_INSTANCE(rule,
                   .init = NULL,
                   .copy = NULL,
                   .finalize = _rule_finalize,
                   .equal = _rule_equal,
                   .hash = _rule_hash,
                   .copyFormattingDesc = NULL,
                   .copyDebugDesc = _rule_copy_description
                   );

static CFTypeID rule_get_type_id() {
    static CFTypeID type_id = _kCFRuntimeNotATypeID;
    static dispatch_once_t onceToken;
    
    dispatch_once(&onceToken, ^{
        type_id = _CFRuntimeRegisterClass(&_auth_type_rule);
    });
    
    return type_id;
}

static rule_t
_rule_create()
{
    rule_t rule = (rule_t)_CFRuntimeCreateInstance(kCFAllocatorDefault, rule_get_type_id(), AUTH_CLASS_SIZE(rule), NULL);
    require(rule != NULL, done);
    
    rule->data = auth_items_create();
    rule->delegations = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    rule->mechanisms = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    
done:
    return rule;
}

static rule_t
_rule_create_with_sql(auth_items_t sql)
{
    rule_t rule = NULL;
    require(sql != NULL, done);
    
    rule = _rule_create();
    require(rule != NULL, done);
    
    auth_items_copy(rule->data, sql);
    
done:
    return rule;
}

rule_t
rule_create_default()
{
    rule_t rule = _rule_create();
    require(rule != NULL, done);

    auth_items_set_int64(rule->data, RULE_TYPE, RT_RIGHT);
    auth_items_set_string(rule->data, RULE_NAME, "(default)");
    auth_items_set_int64(rule->data, RULE_CLASS, RC_USER);
    auth_items_set_string(rule->data, RULE_GROUP, "admin");
    auth_items_set_int64(rule->data, RULE_TIMEOUT, 300);
    auth_items_set_int64(rule->data, RULE_TRIES, kMaximumAuthorizationTries);
    auth_items_set_int64(rule->data, RULE_FLAGS, RuleFlagShared | RuleFlagAuthenticateUser);
    
    mechanism_t mech = mechanism_create_with_string("builtin:authenticate", NULL);
    CFArrayAppendValue(rule->mechanisms, mech);
    CFReleaseNull(mech);

    mech = mechanism_create_with_string("builtin:reset-password,privileged", NULL);
    CFArrayAppendValue(rule->mechanisms, mech);
    CFReleaseNull(mech);

    mech = mechanism_create_with_string("builtin:authenticate,privileged", NULL);
    CFArrayAppendValue(rule->mechanisms, mech);
    CFReleaseNull(mech);

    mech = mechanism_create_with_string("PKINITMechanism:auth,privileged", NULL);
    CFArrayAppendValue(rule->mechanisms, mech);
    CFReleaseNull(mech);
    
done:
    return rule;
}

rule_t
rule_create_with_string(const char * str, authdb_connection_t dbconn)
{
    rule_t rule = NULL;
    require(str != NULL, done);
    
    rule = _rule_create();
    require(rule != NULL, done);
    
    auth_items_set_string(rule->data, RULE_NAME, str);

    if (dbconn) {
        rule_sql_fetch(rule, dbconn);
    }
    
done:
    return rule;
}

static void _set_data_string(rule_t rule, const char * key, CFStringRef str)
{
    char * tmpStr = _copy_cf_string(str, NULL);
    
    if (tmpStr) {
        auth_items_set_string(rule->data, key, tmpStr);
        free_safe(tmpStr);
    }
}

rule_t
rule_create_with_plist(RuleType type, CFStringRef name, CFDictionaryRef plist, authdb_connection_t dbconn)
{
    rule_t rule = NULL;
    require(name != NULL, done);
    require(plist != NULL, done);
            
    rule = _rule_create();
    require(rule != NULL, done);
    
    _set_data_string(rule, RULE_NAME, name);
    require_action(rule_get_name(rule) != NULL, done, CFReleaseSafe(rule));
    
    _sql_get_id(rule, dbconn);

    auth_items_set_int64(rule->data, RULE_TYPE, type);
    
    auth_items_set_int64(rule->data, RULE_CLASS, _get_cf_rule_class(CFDictionaryGetValue(plist, CFSTR(kAuthorizationRuleClass))));
    _set_data_string(rule, RULE_COMMENT, CFDictionaryGetValue(plist, CFSTR(kAuthorizationComment)));

    
    CFTypeRef loc_tmp = CFDictionaryGetValue(plist, CFSTR(kAuthorizationRuleParameterDefaultPrompt));
    if (loc_tmp) {
        rule->loc_prompts = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, loc_tmp);
    }
    loc_tmp = CFDictionaryGetValue(plist, CFSTR(kAuthorizationRuleParameterDefaultButton));
    if (loc_tmp) {
        rule->loc_buttons = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, loc_tmp);
    }
    
    auth_items_set_int64(rule->data, RULE_VERSION, _get_cf_int(CFDictionaryGetValue(plist, CFSTR("version")), 0));
    
    RuleFlags flags = 0;
    
    if (_get_cf_bool(CFDictionaryGetValue(plist, CFSTR(kAuthorizationRuleParameterEntitled)), false)) {
        flags |= RuleFlagEntitled;
    }
    
    if (_get_cf_bool(CFDictionaryGetValue(plist, CFSTR(kAuthorizationRuleParameterRequireAppleSigned)), false)) {
        flags |= RuleFlagRequireAppleSigned;
    }
    
    switch (rule_get_class(rule)) {
        case RC_USER:
            _set_data_string(rule, RULE_GROUP, CFDictionaryGetValue(plist, CFSTR(kAuthorizationRuleParameterGroup)));
            auth_items_set_int64(rule->data, RULE_TIMEOUT, _get_cf_int(CFDictionaryGetValue(plist, CFSTR(kAuthorizationRuleParameterCredentialTimeout)), INT32_MAX));
            auth_items_set_int64(rule->data, RULE_TRIES, _get_cf_int(CFDictionaryGetValue(plist, CFSTR("tries")), kMaximumAuthorizationTries));

            if (_get_cf_bool(CFDictionaryGetValue(plist, CFSTR(kAuthorizationRuleParameterCredentialShared)), false)) {
                flags |= RuleFlagShared;
            }
            if (_get_cf_bool(CFDictionaryGetValue(plist, CFSTR(kAuthorizationRuleParameterAllowRoot)), false)) {
                flags |= RuleFlagAllowRoot;
            }
            if (_get_cf_bool(CFDictionaryGetValue(plist, CFSTR(kAuthorizationRuleParameterCredentialSessionOwner)), false)) {
                flags |= RuleFlagSessionOwner;
            }
            if (_get_cf_bool(CFDictionaryGetValue(plist, CFSTR(kAuthorizationRuleParameterAuthenticateUser)), true)) {
                flags |= RuleFlagAuthenticateUser;
            }
            if (_get_cf_bool(CFDictionaryGetValue(plist, CFSTR(kAuthorizationRuleParameterExtractPassword)), false)) {
                flags |= RuleFlagExtractPassword;
            }
            if (_get_cf_bool(CFDictionaryGetValue(plist, CFSTR(kAuthorizationRuleParameterEntitledAndGroup)), false)) {
                flags |= RuleFlagEntitledAndGroup;
            }
            if (_get_cf_bool(CFDictionaryGetValue(plist, CFSTR(kAuthorizationRuleParameterVPNEntitledAndGroup)), false)) {
                flags |= RuleFlagVPNEntitledAndGroup;
            }
           
            _copy_cf_rule_mechanisms(rule, CFDictionaryGetValue(plist, CFSTR(kAuthorizationRuleParameterMechanisms)), dbconn);
            
            break;
        case RC_RULE:
            auth_items_set_int64(rule->data, RULE_KOFN, _get_cf_int(CFDictionaryGetValue(plist, CFSTR(kAuthorizationRuleParameterKofN)), 0));
            
            _copy_cf_rule_delegations(rule, CFDictionaryGetValue(plist, CFSTR(kAuthorizationRightRule)), dbconn);
            break;
        case RC_MECHANISM:
            auth_items_set_int64(rule->data, RULE_TRIES, _get_cf_int(CFDictionaryGetValue(plist, CFSTR("tries")), kMaximumAuthorizationTries));
            if (_get_cf_bool(CFDictionaryGetValue(plist, CFSTR(kAuthorizationRuleParameterCredentialShared)), true)) {
                flags |= RuleFlagShared;
            }
            if (_get_cf_bool(CFDictionaryGetValue(plist, CFSTR(kAuthorizationRuleParameterExtractPassword)), false)) {
                flags |= RuleFlagExtractPassword;
            }
            
            _copy_cf_rule_mechanisms(rule, CFDictionaryGetValue(plist, CFSTR(kAuthorizationRuleParameterMechanisms)), dbconn);
            
            break;
        case RC_DENY:
            break;
        case RC_ALLOW:
            break;
        default:
            LOGD("rule: invalid rule class");
            break;
    }
    
    auth_items_set_int64(rule->data, RULE_FLAGS, flags);
    
done:
    return rule;
}

static void
_sql_get_id(rule_t rule, authdb_connection_t dbconn)
{
    authdb_step(dbconn, "SELECT id,created,identifier,requirement FROM rules WHERE name = ? LIMIT 1",
    ^(sqlite3_stmt *stmt) {
        sqlite3_bind_text(stmt, 1, rule_get_name(rule), -1, NULL);
    }, ^bool(auth_items_t data) {
        auth_items_copy(rule->data, data);
        return true;
    });
}

static bool
_copy_cf_rule_delegations(rule_t rule, CFTypeRef value, authdb_connection_t dbconn)
{
    bool result = false;
    char * tmp_str = NULL;
    require(value != NULL, done);
    
    if (CFGetTypeID(value) == CFStringGetTypeID()) {
        tmp_str = _copy_cf_string(value, NULL);
        rule_t delegate = rule_create_with_string(tmp_str, dbconn);
        free_safe(tmp_str);
        if (delegate) {
            CFArrayAppendValue(rule->delegations, delegate);
            CFReleaseSafe(delegate);
        }
    } else { //array
        CFIndex count = CFArrayGetCount(value);
        for (CFIndex i = 0; i < count; i++) {
            tmp_str = _copy_cf_string(CFArrayGetValueAtIndex(value,i), NULL);
            rule_t delegate = rule_create_with_string(tmp_str, dbconn);
            free_safe(tmp_str);
            if (delegate) {
                CFArrayAppendValue(rule->delegations, delegate);
                CFReleaseSafe(delegate);
            }
        }
    }

    result = true;
    
done:
    return result;
}

static bool
_copy_cf_rule_mechanisms(rule_t rule, CFTypeRef array, authdb_connection_t dbconn)
{
    bool result = false;
    require(array != NULL, done);
    require(CFGetTypeID(array) == CFArrayGetTypeID(), done);
    
    CFIndex count = CFArrayGetCount(array);
    for (CFIndex i = 0; i < count; i++) {
        mechanism_t mech = NULL;
        char * string = _copy_cf_string(CFArrayGetValueAtIndex(array, i), NULL);

        if (!string)
            continue;
        
        mech = mechanism_create_with_string(string, dbconn);
        if (mech) {
            CFArrayAppendValue(rule->mechanisms, mech);
            CFReleaseSafe(mech);
        }
        free(string);
    }
    
    result = true;
    
done:
    return result;
}

static RuleClass
_get_cf_rule_class(CFTypeRef str)
{
    RuleClass rc = RC_RULE;
    require(str != NULL, done);
    require(CFGetTypeID(str) == CFStringGetTypeID(), done);

    if (CFEqual(str, CFSTR(kAuthorizationRuleClassUser)))
        return RC_USER;
    
    if (CFEqual(str, CFSTR(kAuthorizationRightRule)))
        return RC_RULE;
    
    if (CFEqual(str, CFSTR(kAuthorizationRuleClassMechanisms)))
        return RC_MECHANISM;
    
    if (CFEqual(str, CFSTR(kAuthorizationRuleClassDeny)))
        return RC_DENY;
    
    if (CFEqual(str, CFSTR(kAuthorizationRuleClassAllow)))
        return RC_ALLOW;
    
done:
    return rc;
}

static bool
_sql_bind(rule_t rule, sqlite3_stmt * stmt)
{
    int64_t n;
    int32_t rc = 0;
    require(stmt != NULL, err);

    int32_t column = 1;
    rc = sqlite3_bind_text(stmt, column++, rule_get_name(rule), -1, NULL);
    require_noerr(rc, err);
    rc = sqlite3_bind_int(stmt, column++, rule_get_type(rule));
    require_noerr(rc, err);
    rc = sqlite3_bind_int(stmt, column++, rule_get_class(rule));
    require_noerr(rc, err);
    
    switch (rule_get_class(rule)) {
        case RC_USER:
            rc = sqlite3_bind_text(stmt, column++, rule_get_group(rule), -1, NULL);
            require_noerr(rc, err);
            rc = sqlite3_bind_null(stmt, column++); // kofn
            require_noerr(rc, err);
            rc = sqlite3_bind_int64(stmt, column++, rule_get_timeout(rule));
            require_noerr(rc, err);
            rc = sqlite3_bind_int64(stmt, column++, auth_items_get_int64(rule->data, RULE_FLAGS));
            require_noerr(rc, err);
            rc = sqlite3_bind_int64(stmt, column++, rule_get_tries(rule));
            require_noerr(rc, err);
            break;
        case RC_RULE:
            rc = sqlite3_bind_null(stmt, column++); // group
            require_noerr(rc, err);
            n = rule_get_kofn(rule);
            if (n) {
                rc = sqlite3_bind_int64(stmt, column++, n);
            } else {
                rc = sqlite3_bind_null(stmt, column++);
            }
            require_noerr(rc, err);
            rc = sqlite3_bind_null(stmt, column++); // timeout
            require_noerr(rc, err);
            rc = sqlite3_bind_int64(stmt, column++, auth_items_get_int64(rule->data, RULE_FLAGS));
            require_noerr(rc, err);
            rc = sqlite3_bind_null(stmt, column++); // tries
            require_noerr(rc, err);
            break;
        case RC_MECHANISM:
            rc = sqlite3_bind_null(stmt, column++); // group
            require_noerr(rc, err);
            rc = sqlite3_bind_null(stmt, column++); // kofn
            require_noerr(rc, err);
            rc = sqlite3_bind_null(stmt, column++); // timeout
            require_noerr(rc, err);
            rc = sqlite3_bind_int64(stmt, column++, auth_items_get_int64(rule->data, RULE_FLAGS));
            require_noerr(rc, err);
            rc = sqlite3_bind_int64(stmt, column++, rule_get_tries(rule));
            require_noerr(rc, err);
            break;
        case RC_DENY:
        case RC_ALLOW:
            rc = sqlite3_bind_null(stmt, column++); // group
            require_noerr(rc, err);
            rc = sqlite3_bind_null(stmt, column++); // kofn
            require_noerr(rc, err);
            rc = sqlite3_bind_null(stmt, column++); // timeout
            require_noerr(rc, err);
            rc = sqlite3_bind_int64(stmt, column++, auth_items_get_int64(rule->data, RULE_FLAGS));
            require_noerr(rc, err);
            rc = sqlite3_bind_null(stmt, column++); // tries
            require_noerr(rc, err);
            break;
        default:
            LOGD("rule: sql bind, invalid rule class");
            break;
    }

    rc = sqlite3_bind_int64(stmt, column++, rule_get_version(rule)); // version
    require_noerr(rc, err);
    rc = sqlite3_bind_double(stmt, column++, rule_get_created(rule)); // created
    require_noerr(rc, err);
    rc = sqlite3_bind_double(stmt, column++, rule_get_modified(rule)); // modified
    require_noerr(rc, err);
    rc = sqlite3_bind_null(stmt, column++); // hash
    require_noerr(rc, err);
    rc = sqlite3_bind_text(stmt, column++, rule_get_identifier(rule), -1, NULL);
    require_noerr(rc, err);

    CFDataRef data = rule_get_requirment_data(rule);
    if (data) {
        rc = sqlite3_bind_blob(stmt, column++, CFDataGetBytePtr(data), (int32_t)CFDataGetLength(data), NULL);
    } else {
        rc = sqlite3_bind_null(stmt, column++);
    }
    require_noerr(rc, err);

    rc = sqlite3_bind_text(stmt, column++, rule_get_comment(rule), -1, NULL);
    require_noerr(rc, err);

    return true;
    
err:
    LOGD("rule: sql bind, error %i", rc);
    return false;
}

static void
_get_sql_mechanisms(rule_t rule, authdb_connection_t dbconn)
{
    CFArrayRemoveAllValues(rule->mechanisms);
    
    authdb_step(dbconn, "SELECT mechanisms.* " \
                "FROM mechanisms " \
                "JOIN mechanisms_map ON mechanisms.id = mechanisms_map.m_id " \
                "WHERE mechanisms_map.r_id = ? ORDER BY mechanisms_map.ord ASC",
    ^(sqlite3_stmt *stmt) {
        sqlite3_bind_int64(stmt, 1, rule_get_id(rule));
    }, ^bool(auth_items_t data) {
        mechanism_t mechanism = mechanism_create_with_sql(data);
        CFArrayAppendValue(rule->mechanisms, mechanism);
        CFReleaseSafe(mechanism);
        return true;
    });
}

static void
_get_sql_delegates(rule_t rule, authdb_connection_t dbconn)
{
    CFArrayRemoveAllValues(rule->delegations);
    
    authdb_step(dbconn, "SELECT rules.* " \
                "FROM rules " \
                "JOIN delegates_map ON rules.id = delegates_map.d_id " \
                "WHERE delegates_map.r_id = ? ORDER BY delegates_map.ord ASC",
                ^(sqlite3_stmt *stmt) {
                    sqlite3_bind_int64(stmt, 1, rule_get_id(rule));
                }, ^bool(auth_items_t data) {
                    rule_t delegate = _rule_create_with_sql(data);
                    if (delegate) {
                        _get_sql_mechanisms(delegate, dbconn);
                        
                        if (rule_get_class(rule) == RC_RULE) {
                            _get_sql_delegates(delegate, dbconn);
                        }
                        
                        CFArrayAppendValue(rule->delegations, delegate);
                        CFReleaseSafe(delegate);
                    }
                    return true;
                });
}

bool
rule_sql_fetch(rule_t rule, authdb_connection_t dbconn)
{
    __block bool result = false;
    
    authdb_step(dbconn, "SELECT * FROM rules WHERE name = ? LIMIT 1",
    ^(sqlite3_stmt *stmt) {
        sqlite3_bind_text(stmt, 1, rule_get_name(rule), -1, NULL);
    }, ^bool(auth_items_t data) {
        result = true;
        auth_items_copy(rule->data, data);
        return true;
    });

    if (rule_get_id(rule) != 0) {
        _get_sql_mechanisms(rule,dbconn);
        
        if (rule_get_class(rule) == RC_RULE) {
            _get_sql_delegates(rule, dbconn);
        }
    }

    return result;
}

static bool
_sql_update(rule_t rule, authdb_connection_t dbconn)
{
    bool result = false;
    
    result = authdb_step(dbconn, "UPDATE rules " \
                         "SET name=?,type=?,class=?,'group'=?,kofn=?,timeout=?,flags=?,tries=?,version=?,created=?,modified=?,hash=?,identifier=?,requirement=?,comment=? " \
                         "WHERE id = ?",
                         ^(sqlite3_stmt *stmt) {
                             _sql_bind(rule, stmt);
                             sqlite3_bind_int64(stmt, sqlite3_bind_parameter_count(stmt), rule_get_id(rule));
                         }, NULL);
    return result;
}

static bool
_sql_insert(rule_t rule, authdb_connection_t dbconn)
{
    bool result = false;

    result = authdb_step(dbconn, "INSERT INTO rules VALUES (NULL,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
                         ^(sqlite3_stmt *stmt) {
                             _sql_bind(rule, stmt);
                         }, NULL);
    return result;
}

static bool
_sql_commit_mechanisms_map(rule_t rule, authdb_connection_t dbconn)
{
    bool result = false;
    
    result = authdb_step(dbconn, "DELETE FROM mechanisms_map WHERE r_id = ?", ^(sqlite3_stmt *stmt) {
        sqlite3_bind_int64(stmt, 1, rule_get_id(rule));
    }, NULL);
    require(result == true, done);
    
    CFIndex count = CFArrayGetCount(rule->mechanisms);
    for(CFIndex i = 0; i < count; i++) {
        mechanism_t mech = (mechanism_t)CFArrayGetValueAtIndex(rule->mechanisms, i);
        result = authdb_step(dbconn, "INSERT INTO mechanisms_map VALUES (?,?,?)", ^(sqlite3_stmt *stmt) {
            sqlite3_bind_int64(stmt, 1, rule_get_id(rule));
            sqlite3_bind_int64(stmt, 2, mechanism_get_id(mech));
            sqlite3_bind_int64(stmt, 3, i);
        }, NULL);
        require(result == true, done);
    }
    
done:
    return result;
}

static bool
_sql_commit_delegates_map(rule_t rule, authdb_connection_t dbconn)
{
    bool result = false;
    
    result = authdb_step(dbconn, "DELETE FROM delegates_map WHERE r_id = ?", ^(sqlite3_stmt *stmt) {
        sqlite3_bind_int64(stmt, 1, rule_get_id(rule));
    }, NULL);
    require(result == true, done);
    
    CFIndex count = CFArrayGetCount(rule->delegations);
    for(CFIndex i = 0; i < count; i++) {
        rule_t delegate = (rule_t)CFArrayGetValueAtIndex(rule->delegations, i);
        result = authdb_step(dbconn, "INSERT INTO delegates_map VALUES (?,?,?)", ^(sqlite3_stmt *stmt) {
            sqlite3_bind_int64(stmt, 1, rule_get_id(rule));
            sqlite3_bind_int64(stmt, 2, rule_get_id(delegate));
            sqlite3_bind_int64(stmt, 3, i);
        }, NULL);
        require(result == true, done);
    }
    
done:
    return result;
}

static void
_sql_commit_localization(rule_t rule, authdb_connection_t dbconn)
{

    authdb_step(dbconn, "DELETE FROM prompts WHERE r_id = ?", ^(sqlite3_stmt *stmt) {
        sqlite3_bind_int64(stmt, 1, rule_get_id(rule));
    }, NULL);
    
    authdb_step(dbconn, "DELETE FROM buttons WHERE r_id = ?", ^(sqlite3_stmt *stmt) {
        sqlite3_bind_int64(stmt, 1, rule_get_id(rule));
    }, NULL);
    
    if (rule->loc_prompts) {
        _cf_dictionary_iterate(rule->loc_prompts, ^bool(CFTypeRef key, CFTypeRef value) {
            char * lang = _copy_cf_string(key, NULL);
            char * str = _copy_cf_string(value, NULL);
            
            authdb_step(dbconn, "INSERT INTO prompts VALUES (?,?,?)", ^(sqlite3_stmt *stmt) {
                sqlite3_bind_int64(stmt, 1, rule_get_id(rule));
                sqlite3_bind_text(stmt, 2, lang, -1, NULL);
                sqlite3_bind_text(stmt, 3, str, -1, NULL);
            }, NULL);
            
            free_safe(lang);
            free_safe(str);
            
            return true;
        });
    }

    if (rule->loc_buttons) {
        _cf_dictionary_iterate(rule->loc_buttons, ^bool(CFTypeRef key, CFTypeRef value) {
            char * lang = _copy_cf_string(key, NULL);
            char * str = _copy_cf_string(value, NULL);
            
            authdb_step(dbconn, "INSERT INTO buttons VALUES (?,?,?)", ^(sqlite3_stmt *stmt) {
                sqlite3_bind_int64(stmt, 1, rule_get_id(rule));
                sqlite3_bind_text(stmt, 2, lang, -1, NULL);
                sqlite3_bind_text(stmt, 3, str, -1, NULL);
            }, NULL);
            
            free_safe(lang);
            free_safe(str);
            return true;
        });
    }

}

bool
rule_sql_commit(rule_t rule, authdb_connection_t dbconn, CFAbsoluteTime modified, process_t proc)
{
    bool result = false;
    // type and class required else rule is name only?
    RuleClass rule_class = rule_get_class(rule);
    require(rule_get_type(rule) != 0, done);
    require(rule_class != 0, done);
    
    CFIndex mechCount = 0;
    if (rule_class == RC_USER || rule_class == RC_MECHANISM) {
        // Validate mechanisms
        mechCount = CFArrayGetCount(rule->mechanisms);
        for (CFIndex i = 0; i < mechCount; i++) {
            mechanism_t mech = (mechanism_t)CFArrayGetValueAtIndex(rule->mechanisms, i);
            if (mechanism_get_id(mech) == 0) {
                if (!mechanism_sql_fetch(mech, dbconn)) {
                    mechanism_sql_commit(mech, dbconn);
                    mechanism_sql_fetch(mech, dbconn);
                }
            }
            if (!mechanism_exists(mech)) {
                LOGE("Warning mechanism not found on disk %s during import of %s", mechanism_get_string(mech), rule_get_name(rule));
            }
            require_action(mechanism_get_id(mech) != 0, done, LOGE("rule: commit, invalid mechanism %s:%s for %s", mechanism_get_plugin(mech), mechanism_get_param(mech), rule_get_name(rule)));
        }
    }
    
    CFIndex delegateCount = 0;
    if (rule_class == RC_RULE) {
        // Validate delegates
        delegateCount = CFArrayGetCount(rule->delegations);
        for (CFIndex i = 0; i < delegateCount; i++) {
            rule_t delegate = (rule_t)CFArrayGetValueAtIndex(rule->delegations, i);
            if (rule_get_id(delegate) == 0) {
                rule_sql_fetch(delegate, dbconn);
            }
            require_action(rule_get_id(delegate) != 0, done, LOGE("rule: commit, missing delegate %s for %s", rule_get_name(delegate), rule_get_name(rule)));
        }
    }
    
    auth_items_set_double(rule->data, RULE_MODIFIED, modified);
    
    result = authdb_transaction(dbconn, AuthDBTransactionNormal, ^bool{
        bool update = false;
        
        if (rule_get_id(rule)) {
            update = _sql_update(rule, dbconn);
        } else {
            if (proc) {
                const char * ident = process_get_identifier(proc);
                if (ident) {
                    auth_items_set_string(rule->data, RULE_IDENTIFIER, ident);
                }
                CFDataRef req = process_get_requirement_data(proc);
                if (req) {
                    auth_items_set_data(rule->data, RULE_REQUIREMENT, CFDataGetBytePtr(req), (size_t)CFDataGetLength(req));
                }
            }
            auth_items_set_double(rule->data, RULE_CREATED, modified);
            update = _sql_insert(rule, dbconn);
            _sql_get_id(rule, dbconn);
        }
        
        _sql_commit_localization(rule, dbconn);
        
        if (update) {
            update = _sql_commit_mechanisms_map(rule, dbconn);
        }
        
        if (update) {
            update = _sql_commit_delegates_map(rule,dbconn);
        }
        
        return update;
    });

    
done:
    if (!result) {
        LOGV("rule: commit, failed for %s (%llu)", rule_get_name(rule), rule_get_id(rule));
    }
    return result;
}

bool
rule_sql_remove(rule_t rule, authdb_connection_t dbconn)
{
    bool result = false;
    int64_t id = rule_get_id(rule);
    
    if (id == 0) {
        rule_sql_fetch(rule, dbconn);
        id = rule_get_id(rule);
        require(id != 0, done);
    }

    result = authdb_step(dbconn, "DELETE FROM rules WHERE id = ?",
                         ^(sqlite3_stmt *stmt) {
                             sqlite3_bind_int64(stmt, 1, id);
                         }, NULL);
done:
    return result;
}

CFMutableDictionaryRef
rule_copy_to_cfobject(rule_t rule, authdb_connection_t dbconn) {
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    
    CFTypeRef tmp = NULL;
    CFMutableArrayRef array = NULL;
    CFIndex count = 0;
    CFIndex i = 0;
    int64_t n;
    double d;
    
    const char * comment = rule_get_comment(rule);
    if (comment) {
        tmp = CFStringCreateWithCString(kCFAllocatorDefault, comment, kCFStringEncodingUTF8);
        CFDictionarySetValue(dict, CFSTR(kAuthorizationComment), tmp);
        CFReleaseSafe(tmp);
    }
    
    n = rule_get_version(rule);
    tmp = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &n);
    CFDictionarySetValue(dict, CFSTR(RULE_VERSION), tmp);
    CFReleaseSafe(tmp);
    
    d = rule_get_created(rule);
    tmp = CFNumberCreate(kCFAllocatorDefault, kCFNumberFloat64Type, &d);
    CFDictionarySetValue(dict, CFSTR(RULE_CREATED), tmp);
    CFReleaseSafe(tmp);
    
    d = rule_get_modified(rule);
    tmp = CFNumberCreate(kCFAllocatorDefault, kCFNumberFloat64Type, &d);
    CFDictionarySetValue(dict, CFSTR(RULE_MODIFIED), tmp);
    CFReleaseSafe(tmp);
    
    const char * identifier = rule_get_identifier(rule);
    if (identifier) {
        tmp = CFStringCreateWithCString(kCFAllocatorDefault, identifier, kCFStringEncodingUTF8);
        CFDictionarySetValue(dict, CFSTR(RULE_IDENTIFIER), tmp);
        CFReleaseSafe(tmp);
    }
    
    SecRequirementRef req = rule_get_requirment(rule);
    if (req) {
        CFStringRef reqStr = NULL;
        SecRequirementCopyString(req, kSecCSDefaultFlags, &reqStr);
        if (reqStr) {
            CFDictionarySetValue(dict, CFSTR(RULE_REQUIREMENT), reqStr);
            CFReleaseSafe(reqStr);
        }
    }
    
    if (rule_check_flags(rule, RuleFlagEntitled)) {
        CFDictionarySetValue(dict, CFSTR(kAuthorizationRuleParameterEntitled), kCFBooleanTrue);
    }
    
    if (rule_check_flags(rule, RuleFlagRequireAppleSigned)) {
        CFDictionarySetValue(dict, CFSTR(kAuthorizationRuleParameterRequireAppleSigned), kCFBooleanTrue);
    }
    
    if (rule_get_type(rule) == RT_RIGHT) {
        CFMutableDictionaryRef prompts = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        authdb_step(dbconn, "SELECT * FROM prompts WHERE r_id = ?", ^(sqlite3_stmt *stmt) {
            sqlite3_bind_int64(stmt, 1, rule_get_id(rule));
        }, ^bool(auth_items_t data) {
            CFStringRef key = CFStringCreateWithCString(kCFAllocatorDefault, auth_items_get_string(data, "lang"), kCFStringEncodingUTF8);
            CFStringRef value = CFStringCreateWithCString(kCFAllocatorDefault, auth_items_get_string(data, "value"), kCFStringEncodingUTF8);
            CFDictionaryAddValue(prompts, key, value);
            CFReleaseSafe(key);
            CFReleaseSafe(value);
            return true;
        });
        
        if (CFDictionaryGetCount(prompts)) {
            CFDictionaryAddValue(dict, CFSTR(kAuthorizationRuleParameterDefaultPrompt), prompts);
        }
        CFReleaseSafe(prompts);
        
        CFMutableDictionaryRef buttons = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        authdb_step(dbconn, "SELECT * FROM buttons WHERE r_id = ?", ^(sqlite3_stmt *stmt) {
            sqlite3_bind_int64(stmt, 1, rule_get_id(rule));
        }, ^bool(auth_items_t data) {
            CFStringRef key = CFStringCreateWithCString(kCFAllocatorDefault, auth_items_get_string(data, "lang"), kCFStringEncodingUTF8);
            CFStringRef value = CFStringCreateWithCString(kCFAllocatorDefault, auth_items_get_string(data, "value"), kCFStringEncodingUTF8);
            CFDictionaryAddValue(buttons, key, value);
            CFReleaseSafe(key);
            CFReleaseSafe(value);
            return true;
        });
        
        if (CFDictionaryGetCount(buttons)) {
            CFDictionaryAddValue(dict, CFSTR(kAuthorizationRuleParameterDefaultButton), buttons);
        }
        CFReleaseSafe(buttons);
    }

    switch (rule_get_class(rule)) {
        case RC_USER:
            CFDictionarySetValue(dict, CFSTR(kAuthorizationRuleClass), CFSTR(kAuthorizationRuleClassUser));
            
            const char * group = rule_get_group(rule);
            if (group) {
                tmp = CFStringCreateWithCString(kCFAllocatorDefault, group, kCFStringEncodingUTF8);
                CFDictionarySetValue(dict, CFSTR(kAuthorizationRuleParameterGroup), tmp);
                CFReleaseSafe(tmp);
            }
            
            n = rule_get_timeout(rule);
            tmp = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &n);
            CFDictionarySetValue(dict, CFSTR(kAuthorizationRuleParameterCredentialTimeout), tmp);
            CFReleaseSafe(tmp);
            
            n = rule_get_tries(rule);
            tmp = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &n);
            CFDictionarySetValue(dict, CFSTR("tries"), tmp);
            CFReleaseSafe(tmp);
            
            CFDictionarySetValue(dict, CFSTR(kAuthorizationRuleParameterCredentialShared), rule_get_shared(rule) ? kCFBooleanTrue : kCFBooleanFalse);
            CFDictionarySetValue(dict, CFSTR(kAuthorizationRuleParameterAllowRoot), rule_get_allow_root(rule) ? kCFBooleanTrue : kCFBooleanFalse);
            CFDictionarySetValue(dict, CFSTR(kAuthorizationRuleParameterCredentialSessionOwner), rule_get_session_owner(rule) ? kCFBooleanTrue : kCFBooleanFalse);
            CFDictionarySetValue(dict, CFSTR(kAuthorizationRuleParameterAuthenticateUser), rule_get_authenticate_user(rule) ? kCFBooleanTrue : kCFBooleanFalse);
            if (rule_check_flags(rule, RuleFlagEntitledAndGroup)) {
                CFDictionarySetValue(dict, CFSTR(kAuthorizationRuleParameterEntitledAndGroup), kCFBooleanTrue);
            }
            if (rule_check_flags(rule, RuleFlagVPNEntitledAndGroup)) {
                CFDictionarySetValue(dict, CFSTR(kAuthorizationRuleParameterVPNEntitledAndGroup), kCFBooleanTrue);
            }
            if (rule_get_extract_password(rule)) {
                CFDictionarySetValue(dict, CFSTR(kAuthorizationRuleParameterExtractPassword), kCFBooleanTrue);
            }
            
            count = CFArrayGetCount(rule->mechanisms);
            if (count) {
                array = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
                for (i = 0; i < count; i++) {
                    mechanism_t mech = (mechanism_t)CFArrayGetValueAtIndex(rule->mechanisms, i);
                    tmp = CFStringCreateWithCString(kCFAllocatorDefault, mechanism_get_string(mech), kCFStringEncodingUTF8);
                    CFArrayAppendValue(array, tmp);
                    CFReleaseSafe(tmp);
                }
                CFDictionaryAddValue(dict, CFSTR(kAuthorizationRuleParameterMechanisms), array);
                CFRelease(array);
            }
            break;
        case RC_RULE:
            CFDictionarySetValue(dict, CFSTR(kAuthorizationRuleClass), CFSTR(kAuthorizationRightRule));
            int64_t kofn = rule_get_kofn(rule);
            if (kofn) {
                tmp = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &kofn);
                CFDictionarySetValue(dict, CFSTR(kAuthorizationRuleParameterKofN), tmp);
                CFReleaseSafe(tmp);
            }
            
            count = CFArrayGetCount(rule->delegations);
            if (count) {
                array = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
                for (i = 0; i < count; i++) {
                    rule_t delegate = (rule_t)CFArrayGetValueAtIndex(rule->delegations, i);
                    tmp = CFStringCreateWithCString(kCFAllocatorDefault, rule_get_name(delegate), kCFStringEncodingUTF8);
                    CFArrayAppendValue(array, tmp);
                    CFReleaseSafe(tmp);
                }
                CFDictionaryAddValue(dict, CFSTR(kAuthorizationRightRule), array);
                CFRelease(array);
            }
            break;
        case RC_MECHANISM:
            CFDictionarySetValue(dict, CFSTR(kAuthorizationRuleClass), CFSTR(kAuthorizationRuleClassMechanisms));

            CFDictionarySetValue(dict, CFSTR(kAuthorizationRuleParameterCredentialShared), rule_get_shared(rule) ? kCFBooleanTrue : kCFBooleanFalse);
            if (rule_get_extract_password(rule)) {
                CFDictionarySetValue(dict, CFSTR(kAuthorizationRuleParameterExtractPassword), kCFBooleanTrue);
            }
            
            n = rule_get_tries(rule);
            tmp = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &n);
            CFDictionarySetValue(dict, CFSTR("tries"), tmp);
            CFReleaseSafe(tmp);

            count = CFArrayGetCount(rule->mechanisms);
            if (count) {
                array = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
                for (i = 0; i < count; i++) {
                    mechanism_t mech = (mechanism_t)CFArrayGetValueAtIndex(rule->mechanisms, i);
                    tmp = CFStringCreateWithCString(kCFAllocatorDefault, mechanism_get_string(mech), kCFStringEncodingUTF8);
                    CFArrayAppendValue(array, tmp);
                    CFReleaseSafe(tmp);
                }
                CFDictionaryAddValue(dict, CFSTR(kAuthorizationRuleParameterMechanisms), array);
                CFRelease(array);
            }
            break;
        case RC_DENY:
            CFDictionarySetValue(dict, CFSTR(kAuthorizationRuleClass), CFSTR(kAuthorizationRuleClassDeny));
            break;
        case RC_ALLOW:
            CFDictionarySetValue(dict, CFSTR(kAuthorizationRuleClass), CFSTR(kAuthorizationRuleClassAllow));
            break;
        default:
            break;
    }

    return dict;
}


CFArrayRef
rule_get_mechanisms(rule_t rule)
{
    return rule->mechanisms;
}

size_t
rule_get_mechanisms_count(rule_t rule)
{
    return (size_t)CFArrayGetCount(rule->mechanisms);
}

bool
rule_mechanisms_iterator(rule_t rule, mechanism_iterator_t iter)
{
    bool result = false;
    
    CFIndex count = CFArrayGetCount(rule->mechanisms);
    for (CFIndex i = 0; i < count; i++) {
        mechanism_t mech = (mechanism_t)CFArrayGetValueAtIndex(rule->mechanisms, i);
        result = iter(mech);
        if (!result) {
            break;
        }
    }
    
    return result;
}

size_t
rule_get_delegates_count(rule_t rule)
{
    return (size_t)CFArrayGetCount(rule->delegations);
}

bool
rule_delegates_iterator(rule_t rule, delegate_iterator_t iter)
{
    bool result = false;
    
    CFIndex count = CFArrayGetCount(rule->delegations);
    for (CFIndex i = 0; i < count; i++) {
        rule_t tmp = (rule_t)CFArrayGetValueAtIndex(rule->delegations, i);
        result = iter(tmp);
        if (!result) {
            break;
        }
    }
    
    return result;
}

int64_t
rule_get_id(rule_t rule)
{
    return auth_items_get_int64(rule->data, RULE_ID);
}

const char *
rule_get_name(rule_t rule)
{
    return auth_items_get_string(rule->data, RULE_NAME);
}

RuleType
rule_get_type(rule_t rule)
{
    return (RuleType)auth_items_get_int64(rule->data, RULE_TYPE);
}

RuleClass
rule_get_class(rule_t rule)
{
    return (RuleClass)auth_items_get_int64(rule->data, RULE_CLASS);
}

const char *
rule_get_group(rule_t rule)
{
    return auth_items_get_string(rule->data, RULE_GROUP);
}

int64_t
rule_get_kofn(rule_t rule)
{
    return auth_items_get_int64(rule->data, RULE_KOFN);
}

int64_t
rule_get_timeout(rule_t rule)
{
    return auth_items_get_int64(rule->data, RULE_TIMEOUT);
}

bool
rule_check_flags(rule_t rule, RuleFlags flags)
{
    return (auth_items_get_int64(rule->data, RULE_FLAGS) & flags) != 0;
}

bool
rule_get_shared(rule_t rule)
{
    return rule_check_flags(rule, RuleFlagShared);
}

bool
rule_get_allow_root(rule_t rule)
{
    return rule_check_flags(rule, RuleFlagAllowRoot);
}

bool
rule_get_session_owner(rule_t rule)
{
    return rule_check_flags(rule, RuleFlagSessionOwner);
}

bool
rule_get_authenticate_user(rule_t rule)
{
    return rule_check_flags(rule, RuleFlagAuthenticateUser);
}

bool
rule_get_extract_password(rule_t rule)
{
    return rule_check_flags(rule, RuleFlagExtractPassword);
}

int64_t
rule_get_tries(rule_t rule)
{
    return auth_items_get_int64(rule->data, RULE_TRIES);
}

const char *
rule_get_comment(rule_t rule)
{
    return auth_items_get_string(rule->data, RULE_COMMENT);
}

int64_t
rule_get_version(rule_t rule)
{
    return auth_items_get_int64(rule->data, RULE_VERSION);
}

double rule_get_created(rule_t rule)
{
    return auth_items_get_double(rule->data, RULE_CREATED);
}

double rule_get_modified(rule_t rule)
{
    return auth_items_get_double(rule->data, RULE_MODIFIED);
}

const char * rule_get_identifier(rule_t rule)
{
    return auth_items_get_string(rule->data, RULE_IDENTIFIER);
}

CFDataRef rule_get_requirment_data(rule_t rule)
{
    if (!rule->requirement_data && auth_items_exist(rule->data, RULE_REQUIREMENT)) {
        size_t len;
        const void * data = auth_items_get_data(rule->data, RULE_REQUIREMENT, &len);
        rule->requirement_data = CFDataCreate(kCFAllocatorDefault, data, (CFIndex)len);
    }
    
    return rule->requirement_data;
}

SecRequirementRef rule_get_requirment(rule_t rule)
{
    if (!rule->requirement) {
        CFDataRef data = rule_get_requirment_data(rule);
        if (data) {
            SecRequirementCreateWithData(data, kSecCSDefaultFlags, &rule->requirement);
        }
    }
    
    return rule->requirement;
}
