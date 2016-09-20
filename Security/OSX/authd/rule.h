/* Copyright (c) 2012-2013 Apple Inc. All Rights Reserved. */

#ifndef _SECURITY_AUTH_RULE_H_
#define _SECURITY_AUTH_RULE_H_

#include "authdb.h"
#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecRequirement.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef bool (^mechanism_iterator_t)(mechanism_t mechanism);
typedef bool (^delegate_iterator_t)(rule_t delegate);
    
typedef enum {
    RT_RIGHT = 1,
    RT_RULE
} RuleType;

typedef enum {
    RC_USER = 1,
    RC_RULE,
    RC_MECHANISM,
    RC_ALLOW,
    RC_DENY
} RuleClass;

enum {
    RuleFlagShared              = 1 << 0,
    RuleFlagAllowRoot           = 1 << 1,
    RuleFlagSessionOwner        = 1 << 2,
    RuleFlagAuthenticateUser    = 1 << 3,
    RuleFlagExtractPassword     = 1 << 4,
    RuleFlagEntitled            = 1 << 5,
    RuleFlagEntitledAndGroup    = 1 << 6,
    RuleFlagRequireAppleSigned  = 1 << 7,
    RuleFlagVPNEntitledAndGroup = 1 << 8,
	RuleFlagPasswordOnly		= 1 << 9
};
typedef uint32_t RuleFlags;

AUTH_WARN_RESULT AUTH_MALLOC AUTH_NONNULL_ALL AUTH_RETURNS_RETAINED
rule_t rule_create_default(void);

AUTH_WARN_RESULT AUTH_MALLOC AUTH_NONNULL1 AUTH_RETURNS_RETAINED
rule_t rule_create_with_string(const char *,authdb_connection_t);
        
AUTH_WARN_RESULT AUTH_MALLOC AUTH_NONNULL_ALL AUTH_RETURNS_RETAINED
rule_t rule_create_with_plist(RuleType,CFStringRef,CFDictionaryRef,authdb_connection_t);

AUTH_NONNULL_ALL
size_t rule_get_mechanisms_count(rule_t);

AUTH_NONNULL_ALL
CFArrayRef rule_get_mechanisms(rule_t);

AUTH_NONNULL_ALL
bool rule_mechanisms_iterator(rule_t,mechanism_iterator_t iter);

AUTH_NONNULL_ALL
size_t rule_get_delegates_count(rule_t);
    
AUTH_NONNULL_ALL
bool rule_delegates_iterator(rule_t,delegate_iterator_t iter);

AUTH_NONNULL_ALL
bool rule_sql_fetch(rule_t,authdb_connection_t);
    
AUTH_NONNULL1 AUTH_NONNULL2
bool rule_sql_commit(rule_t,authdb_connection_t,CFAbsoluteTime,process_t);

AUTH_NONNULL_ALL
bool rule_sql_remove(rule_t,authdb_connection_t);

AUTH_NONNULL_ALL
CFMutableDictionaryRef rule_copy_to_cfobject(rule_t,authdb_connection_t);
    
AUTH_NONNULL_ALL
int64_t rule_get_id(rule_t);

AUTH_NONNULL_ALL
const char * rule_get_name(rule_t);
    
AUTH_NONNULL_ALL
RuleType rule_get_type(rule_t);
    
AUTH_NONNULL_ALL
RuleClass rule_get_class(rule_t);
    
AUTH_NONNULL_ALL
const char * rule_get_group(rule_t);
    
AUTH_NONNULL_ALL
int64_t rule_get_kofn(rule_t);
    
AUTH_NONNULL_ALL
int64_t rule_get_timeout(rule_t);
    
AUTH_NONNULL_ALL
bool rule_check_flags(rule_t,RuleFlags);
    
AUTH_NONNULL_ALL
bool rule_get_shared(rule_t);
    
AUTH_NONNULL_ALL
bool rule_get_allow_root(rule_t);
    
AUTH_NONNULL_ALL
bool rule_get_session_owner(rule_t);
    
AUTH_NONNULL_ALL
bool rule_get_authenticate_user(rule_t);
    
AUTH_NONNULL_ALL
bool rule_get_extract_password(rule_t);

AUTH_NONNULL_ALL
bool rule_get_password_only(rule_t);

AUTH_NONNULL_ALL
int64_t rule_get_tries(rule_t);
    
AUTH_NONNULL_ALL
const char * rule_get_comment(rule_t);

AUTH_NONNULL_ALL
int64_t rule_get_version(rule_t);

AUTH_NONNULL_ALL
double rule_get_created(rule_t);
    
AUTH_NONNULL_ALL
double rule_get_modified(rule_t);

AUTH_NONNULL_ALL
const char * rule_get_identifier(rule_t);

AUTH_NONNULL_ALL
CFDataRef rule_get_requirement_data(rule_t);

AUTH_NONNULL_ALL
SecRequirementRef rule_get_requirement(rule_t);
    
#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_AUTH_RULE_H_ */
