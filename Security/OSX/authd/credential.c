/* Copyright (c) 2012-2013 Apple Inc. All Rights Reserved. */

#include "credential.h"
#include "authutilities.h"
#include "authitems.h"
#include <Security/AuthorizationTagsPriv.h>
#include "debugging.h"
#include "crc.h"

#include <pwd.h>
#include <membership.h>
#include <membershipPriv.h>

AUTHD_DEFINE_LOG

struct _credential_s {
    __AUTH_BASE_STRUCT_HEADER__;

    bool right;   // is least-privileged credential
    
    uid_t uid;
    char * name;
    char * realName;
    
    CFAbsoluteTime creationTime;
    bool valid;
    bool shared;
    
    CFMutableSetRef cachedGroups;
};

static void
_credential_finalize(CFTypeRef value)
{
    credential_t cred = (credential_t)value;
    
    free_safe(cred->name);
    free_safe(cred->realName);
    CFReleaseNull(cred->cachedGroups);
}

static CFStringRef
_credential_copy_description(CFTypeRef value)
{
    credential_t cred = (credential_t)value;
    CFStringRef str = NULL;
    CFTimeZoneRef sys_tz = CFTimeZoneCopySystem();
    CFGregorianDate date = CFAbsoluteTimeGetGregorianDate(cred->creationTime, sys_tz);
    CFReleaseSafe(sys_tz);
    if (cred->right) {
        str = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("credential: right=%s, shared=%i, creation=%01i:%01i:%01i, valid=%i"), cred->name, cred->shared, date.hour,date.minute,(int32_t)date.second, cred->valid);
    } else {
        str = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("credential: uid=%i, name=%s, shared=%i, creation=%01i:%01i:%01i valid=%i"), cred->uid, cred->name, cred->shared, date.hour,date.minute,(int32_t)date.second, cred->valid);
    }
    return str;
}

static CFHashCode
_credential_hash(CFTypeRef value)
{
    credential_t cred = (credential_t)value;
    uint64_t crc = crc64_init();
    if (cred->right) {
        crc = crc64_update(crc, cred->name, strlen(cred->name));
    } else {
        crc = crc64_update(crc, &cred->uid, sizeof(cred->uid));
    }
    crc = crc64_update(crc, &cred->shared, sizeof(cred->shared));
    crc = crc64_final(crc);
    
    return (CFHashCode)crc;
}

static Boolean
_credential_equal(CFTypeRef value1, CFTypeRef value2)
{
    credential_t cred1 = (credential_t)value1;
    credential_t cred2 = (credential_t)value2;
    
    return _credential_hash(cred1) == _credential_hash(cred2);
}

AUTH_TYPE_INSTANCE(credential,
                   .init = NULL,
                   .copy = NULL,
                   .finalize = _credential_finalize,
                   .equal = _credential_equal,
                   .hash = _credential_hash,
                   .copyFormattingDesc = NULL,
                   .copyDebugDesc = _credential_copy_description
                   );

static CFTypeID credential_get_type_id(void) {
    static CFTypeID type_id = _kCFRuntimeNotATypeID;
    static dispatch_once_t onceToken;
    
    dispatch_once(&onceToken, ^{
        type_id = _CFRuntimeRegisterClass(&_auth_type_credential);
    });
    
    return type_id;
}

static credential_t
_credential_create(void)
{
    credential_t cred = NULL;

    cred = (credential_t)_CFRuntimeCreateInstance(kCFAllocatorDefault, credential_get_type_id(), AUTH_CLASS_SIZE(credential), NULL);
    require(cred != NULL, done);
    
    cred->creationTime = CFAbsoluteTimeGetCurrent();
    cred->cachedGroups = CFSetCreateMutable(kCFAllocatorDefault, 0, &kCFTypeSetCallBacks);
    
done:
    return cred;
}

credential_t
credential_create(uid_t uid)
{
    credential_t cred = NULL;
    
    cred = _credential_create();
    require(cred != NULL, done);

    struct passwd *pw = (uid != (uid_t)-1) ? getpwuid(uid) : NULL;
    if (pw != NULL) {
        // avoid hinting a locked account
        // LibInfo started to return asterisk for system accounts in <rdar://problem/31633690> J93: 17a240: Hang during boot (opendirectoryd/powerd deadlock)
        // so do not make this check for those system accounts
        if ((uid < 500) ||  (pw->pw_passwd == NULL) || strcmp(pw->pw_passwd, "*")) {
            cred->uid = pw->pw_uid;
            cred->name = _copy_string(pw->pw_name);
            cred->realName = _copy_string(pw->pw_gecos);
            cred->valid = true;
        } else {
            cred->uid = (uid_t)-2;
            cred->name = _copy_string("invalid");
            cred->valid = false;
        }
        endpwent();
    } else {
        if (uid != (uid_t)-1) {
            os_log_error(AUTHD_LOG, "credential: failed to get user uid %d", uid);
        }
        cred->uid = (uid_t)-3;
        cred->name = _copy_string("invalid");
        cred->valid = false;
    }
    
done:
    return cred;
}

credential_t
credential_create_fvunlock(auth_items_t context, bool session)
{
    credential_t cred = NULL;
    
    cred = _credential_create();
    require(cred != NULL, done);

    const char *username = session ? "system session" : auth_items_get_string(context, AGENT_USERNAME);
    cred->uid = session ? 0 : -500;
    cred->name = _copy_string(username);
    cred->realName = _copy_string(username);
    cred->valid = false;
done:
    return cred;
}

credential_t
credential_create_with_credential(credential_t srcCred, bool shared)
{
    credential_t cred = NULL;
    
    cred = _credential_create();
    require(cred != NULL, done);
    
    cred->uid = srcCred->uid;
    cred->name = _copy_string(srcCred->name);
    cred->realName = _copy_string(srcCred->realName);
    cred->valid = srcCred->valid;
    cred->right = srcCred->right;
    cred->shared = shared;
    
done:
    return cred;
}

credential_t
credential_create_with_right(const char * right)
{
    credential_t cred = NULL;
    
    cred = _credential_create();
    require(cred != NULL, done);
    
    cred->right = true;
    cred->name = _copy_string(right);
    cred->uid = (uid_t)-2;
    cred->valid = true;
    
done:
    return cred;
}

uid_t
credential_get_uid(credential_t cred)
{
    return cred->uid;
}

const char *
credential_get_name(credential_t cred)
{
    return cred->name;
}

const char *
credential_get_realname(credential_t cred)
{
    return cred->realName;
}

CFAbsoluteTime
credential_get_creation_time(credential_t cred)
{
    return cred->creationTime;
}

bool
credential_get_valid(credential_t cred)
{
    return cred->valid;
}

bool
credential_get_shared(credential_t cred)
{
    return cred->shared;
}

bool
credential_is_right(credential_t cred)
{
    return cred->right;
}

bool
credential_check_membership(credential_t cred, const char* group)
{
    bool result = false;
    
    if (isInFVUnlockOrRecovery()) {
       return false; // cannot succeed in FVUnlock as we do not have group data
    }
    
    CFStringRef cachedGroup = NULL;
    require(group != NULL, done);
    require(cred->uid != 0 || cred->uid != (uid_t)-2, done);
    require(cred->right != true, done);
    
    cachedGroup = CFStringCreateWithCString(kCFAllocatorDefault, group, kCFStringEncodingUTF8);
    require(cachedGroup != NULL, done);
    
    if (CFSetGetValue(cred->cachedGroups, cachedGroup) != NULL) {
        result = true;
        goto done;
    }
    
    int rc, ismember;
    uuid_t group_uuid, user_uuid;
    rc = mbr_group_name_to_uuid(group, group_uuid);
    require_noerr(rc, done);
    
    rc = mbr_uid_to_uuid(cred->uid, user_uuid);
    require_noerr(rc, done);
    
    rc = mbr_check_membership(user_uuid, group_uuid, &ismember);
    require_noerr(rc, done);
    
    result = ismember;
    
    if (ismember) {
        CFSetSetValue(cred->cachedGroups, cachedGroup);
    }

done:
    CFReleaseSafe(cachedGroup);
    return result;
}

void
credential_invalidate(credential_t cred)
{
    cred->valid = false;
}
