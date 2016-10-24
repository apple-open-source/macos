/* Copyright (c) 2012-2013 Apple Inc. All Rights Reserved. */

#include "process.h"
#include "server.h"
#include "session.h"
#include "debugging.h"
#include "authd_private.h"
#include "authtoken.h"
#include "authutilities.h"
#include "ccaudit.h"

#include <Security/SecCode.h>
#include <Security/SecRequirement.h>

struct _process_s {
    __AUTH_BASE_STRUCT_HEADER__;
    
    audit_info_s auditInfo;
    
    session_t session;
    
    CFMutableBagRef authTokens;
    dispatch_queue_t dispatch_queue;
    
    CFMutableSetRef connections;
    
    SecCodeRef codeRef;
    char code_url[PATH_MAX+1];
    char * code_identifier;
    CFDataRef code_requirement_data;
    SecRequirementRef code_requirement;
    CFDictionaryRef code_entitlements;
    
    mach_port_t bootstrap;
    
    bool appStoreSigned;
	bool firstPartySigned;
};

static void
_unregister_auth_tokens(const void *value, void *context)
{
    auth_token_t auth = (auth_token_t)value;
    process_t proc = (process_t)context;
    
    CFIndex count = auth_token_remove_process(auth, proc);
    if ((count == 0)  && auth_token_check_state(auth, auth_token_state_registered)) {
        server_unregister_auth_token(auth);
    }
}

static void
_destroy_zombie_tokens(process_t proc)
{
    LOGD("process[%i] destroy zombies, %ld auth tokens", process_get_pid(proc), CFBagGetCount(proc->authTokens));
    _cf_bag_iterate(proc->authTokens, ^bool(CFTypeRef value) {
        auth_token_t auth = (auth_token_t)value;
        LOGD("process[%i] %p, creator=%i, zombie=%i, process_cout=%ld", process_get_pid(proc), auth, auth_token_is_creator(auth, proc), auth_token_check_state(auth, auth_token_state_zombie), auth_token_get_process_count(auth));
        if (auth_token_is_creator(auth, proc) && auth_token_check_state(auth, auth_token_state_zombie) && (auth_token_get_process_count(auth) == 1)) {
            CFBagRemoveValue(proc->authTokens, auth);
        }
        return true;
    });
}

static void
_process_finalize(CFTypeRef value)
{
    process_t proc = (process_t)value;

    LOGV("process[%i]: deallocated %p", proc->auditInfo.pid, proc);
    
    dispatch_barrier_sync(proc->dispatch_queue, ^{
        CFBagApplyFunction(proc->authTokens, _unregister_auth_tokens, proc);
    });
    
    session_remove_process(proc->session, proc);
    
    dispatch_release(proc->dispatch_queue);
    CFReleaseSafe(proc->authTokens);
    CFReleaseSafe(proc->connections);
    CFReleaseSafe(proc->session);
    CFReleaseSafe(proc->codeRef);
    CFReleaseSafe(proc->code_requirement);
    CFReleaseSafe(proc->code_requirement_data);
    CFReleaseSafe(proc->code_entitlements);
    free_safe(proc->code_identifier);
    if (proc->bootstrap != MACH_PORT_NULL) {
        mach_port_deallocate(mach_task_self(), proc->bootstrap);
    }
}

AUTH_TYPE_INSTANCE(process,
                   .init = NULL,
                   .copy = NULL,
                   .finalize = _process_finalize,
                   .equal = NULL,
                   .hash = NULL,
                   .copyFormattingDesc = NULL,
                   .copyDebugDesc = NULL
                   );

static CFTypeID process_get_type_id() {
    static CFTypeID type_id = _kCFRuntimeNotATypeID;
    static dispatch_once_t onceToken;
    
    dispatch_once(&onceToken, ^{
        type_id = _CFRuntimeRegisterClass(&_auth_type_process);
    });
    
    return type_id;
}

process_t
process_create(const audit_info_s * auditInfo, session_t session)
{
    OSStatus status = errSecSuccess;
    process_t proc = NULL;
    CFDictionaryRef code_info = NULL;
    CFURLRef code_url = NULL;

    require(session != NULL, done);
    require(auditInfo != NULL, done);
    
    proc = (process_t)_CFRuntimeCreateInstance(kCFAllocatorDefault, process_get_type_id(), AUTH_CLASS_SIZE(process), NULL);
    require(proc != NULL, done);
    
    proc->auditInfo = *auditInfo;
    
    proc->session = (session_t)CFRetain(session);
    
    proc->connections = CFSetCreateMutable(kCFAllocatorDefault, 0, NULL);
    
    proc->authTokens = CFBagCreateMutable(kCFAllocatorDefault, 0, &kCFTypeBagCallBacks);
    check(proc->authTokens != NULL);
    
    proc->dispatch_queue = dispatch_queue_create(NULL, DISPATCH_QUEUE_SERIAL);
    check(proc->dispatch_queue != NULL);

    CFMutableDictionaryRef codeDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFNumberRef codePid = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &proc->auditInfo.pid);
    CFDictionarySetValue(codeDict, kSecGuestAttributePid, codePid);
    status = SecCodeCopyGuestWithAttributes(NULL, codeDict, kSecCSDefaultFlags, &proc->codeRef);
    CFReleaseSafe(codeDict);
    CFReleaseSafe(codePid);

    if (status) {
        LOGE("process[%i]: failed to create code ref %d", proc->auditInfo.pid, (int)status);
        CFReleaseNull(proc);
        goto done;
    }
    
    status = SecCodeCopySigningInformation(proc->codeRef, kSecCSRequirementInformation, &code_info);
    require_noerr_action(status, done, LOGV("process[%i]: SecCodeCopySigningInformation failed with %d", proc->auditInfo.pid, (int)status));

    CFTypeRef value = NULL;
    if (CFDictionaryGetValueIfPresent(code_info, kSecCodeInfoDesignatedRequirement, (const void**)&value)) {
        if (CFGetTypeID(value) == SecRequirementGetTypeID()) {
            SecRequirementCopyData((SecRequirementRef)value, kSecCSDefaultFlags, &proc->code_requirement_data);
            if (proc->code_requirement_data) {
                SecRequirementCreateWithData(proc->code_requirement_data, kSecCSDefaultFlags, &proc->code_requirement);
            }
        }
        value = NULL;
    }

    if (SecCodeCopyPath(proc->codeRef, kSecCSDefaultFlags, &code_url) == errSecSuccess) {
        CFURLGetFileSystemRepresentation(code_url, true, (UInt8*)proc->code_url, sizeof(proc->code_url));
    }

    if (CFDictionaryGetValueIfPresent(code_info, kSecCodeInfoIdentifier, &value)) {
        if (CFGetTypeID(value) == CFStringGetTypeID()) {
            proc->code_identifier = _copy_cf_string(value, NULL);
        }
        value = NULL;
    }
    
    if (CFDictionaryGetValueIfPresent(code_info, kSecCodeInfoEntitlementsDict, &value)) {
        if (CFGetTypeID(value) == CFDictionaryGetTypeID()) {
            proc->code_entitlements = CFDictionaryCreateCopy(kCFAllocatorDefault, value);
        }
        value = NULL;
    }

    // This is the clownfish supported way to check for a Mac App Store or B&I signed build
	// AppStore apps must have resource envelope 2. Check with spctl -a -t exec -vv <path>
    CFStringRef firstPartyRequirement = CFSTR("anchor apple");
	CFStringRef appStoreRequirement = CFSTR("anchor apple generic and certificate leaf[field.1.2.840.113635.100.6.1.9] exists");
    SecRequirementRef  secRequirementRef = NULL;
    status = SecRequirementCreateWithString(firstPartyRequirement, kSecCSDefaultFlags, &secRequirementRef);
    if (status == errSecSuccess) {
        proc->firstPartySigned = process_verify_requirement(proc, secRequirementRef);
		CFReleaseNull(secRequirementRef);
    }
	status = SecRequirementCreateWithString(appStoreRequirement, kSecCSDefaultFlags, &secRequirementRef);
	if (status == errSecSuccess) {
		proc->appStoreSigned = process_verify_requirement(proc, secRequirementRef);
		CFReleaseSafe(secRequirementRef);
	}
    LOGV("process[%i]: created (sid=%i) %s %p", proc->auditInfo.pid, proc->auditInfo.asid, proc->code_url, proc);

done:
    CFReleaseSafe(code_info);
    CFReleaseSafe(code_url);
    return proc;
}

const void *
process_get_key(process_t proc)
{
    return &proc->auditInfo;
}

uid_t
process_get_uid(process_t proc)
{
    assert(proc); // marked non-null
    return proc->auditInfo.euid;
}

pid_t
process_get_pid(process_t proc)
{
    assert(proc); // marked non-null
    return proc->auditInfo.pid;
}

int32_t process_get_generation(process_t proc)
{
    assert(proc); // marked non-null
    return proc->auditInfo.tid;
}

session_id_t
process_get_session_id(process_t proc)
{
    assert(proc); // marked non-null
    return proc->auditInfo.asid;
}

session_t
process_get_session(process_t proc)
{
    assert(proc); // marked non-null
    return proc->session;
}

const audit_info_s *
process_get_audit_info(process_t proc)
{
    return &proc->auditInfo;
}

SecCodeRef
process_get_code(process_t proc)
{
    return proc->codeRef;
}

const char *
process_get_code_url(process_t proc)
{
    return proc->code_url;
}

void
process_add_auth_token(process_t proc, auth_token_t auth)
{
    dispatch_sync(proc->dispatch_queue, ^{
        CFBagAddValue(proc->authTokens, auth);
        if (CFBagGetCountOfValue(proc->authTokens, auth) == 1) {
            auth_token_add_process(auth, proc);
        }
    });
}

void
process_remove_auth_token(process_t proc, auth_token_t auth, uint32_t flags)
{
    dispatch_sync(proc->dispatch_queue, ^{
        bool destroy = false;
        bool creator = auth_token_is_creator(auth, proc);
        CFIndex count = auth_token_get_process_count(auth);

        // if we are the last ones associated with this auth token or the caller passed in the kAuthorizationFlagDestroyRights
        // then we break the link between the process and auth token.  If another process holds a reference
        // then kAuthorizationFlagDestroyRights will only break the link and not destroy the auth token
        // <rdar://problem/14553640>
        if ((count == 1) ||
            (flags & kAuthorizationFlagDestroyRights))
        {
            destroy = true;
            goto done;
        }

        // If we created this token and someone else is holding a reference to it
        // don't destroy the link until they have freed the authorization ref
        // instead set the zombie state on the auth_token
        if (creator) {
            if (CFBagGetCountOfValue(proc->authTokens, auth) == 1) {
                auth_token_set_state(auth, auth_token_state_zombie);
            } else {
                destroy = true;
            }
        } else {
            destroy = true;
        }

    done:
        if (destroy) {
            CFBagRemoveValue(proc->authTokens, auth);
            if (!CFBagContainsValue(proc->authTokens, auth)) {
                auth_token_remove_process(auth, proc);

                if ((count == 1) && auth_token_check_state(auth, auth_token_state_registered)) {
                    server_unregister_auth_token(auth);
                }
            }
        }

        // destroy all eligible zombies
        _destroy_zombie_tokens(proc);
    });
}

auth_token_t
process_find_copy_auth_token(process_t proc, const AuthorizationBlob * blob)
{
    __block CFTypeRef auth = NULL;
    dispatch_sync(proc->dispatch_queue, ^{
        _cf_bag_iterate(proc->authTokens, ^bool(CFTypeRef value) {
            auth_token_t iter = (auth_token_t)value;
            if (memcmp(blob, auth_token_get_blob(iter), sizeof(AuthorizationBlob)) == 0) {
                auth = iter;
                CFRetain(auth);
                return false;
            }
            return true;
        });
    });
    return (auth_token_t)auth;
}

CFIndex
process_get_auth_token_count(process_t proc)
{
    __block CFIndex count = 0;
    dispatch_sync(proc->dispatch_queue, ^{
        count = CFBagGetCount(proc->authTokens);
    });
    return count;
}

CFIndex
process_add_connection(process_t proc, connection_t conn)
{
    __block CFIndex count = 0;
    dispatch_sync(proc->dispatch_queue, ^{
        CFSetAddValue(proc->connections, conn);
        count = CFSetGetCount(proc->connections);
    });
    return count;
}

CFIndex
process_remove_connection(process_t proc, connection_t conn)
{
    __block CFIndex count = 0;
    dispatch_sync(proc->dispatch_queue, ^{
        CFSetRemoveValue(proc->connections, conn);
        count = CFSetGetCount(proc->connections);
    });
    return count;
}

CFIndex
process_get_connection_count(process_t proc)
{
    __block CFIndex count = 0;
    dispatch_sync(proc->dispatch_queue, ^{
        count = CFSetGetCount(proc->connections);
    });
    return count;
}

CFTypeRef
process_copy_entitlement_value(process_t proc, const char * entitlement)
{
    CFTypeRef value = NULL;
    require(entitlement != NULL, done);

    CFStringRef key = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, entitlement, kCFStringEncodingUTF8, kCFAllocatorNull);
    if (proc->code_entitlements && key && (CFDictionaryGetValueIfPresent(proc->code_entitlements, key, &value))) {
        CFRetainSafe(value);
    }
    CFReleaseSafe(key);
    
done:
    return value;
}

bool
process_has_entitlement(process_t proc, const char * entitlement)
{
    bool entitled = false;
    require(entitlement != NULL, done);

    CFStringRef key = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, entitlement, kCFStringEncodingUTF8, kCFAllocatorNull);
    CFTypeRef value = NULL;
    if (proc->code_entitlements && key && (CFDictionaryGetValueIfPresent(proc->code_entitlements, key, &value))) {
        if (CFGetTypeID(value) == CFBooleanGetTypeID()) {
            entitled = CFBooleanGetValue(value);
        }
    }
    CFReleaseSafe(key);
    
done:
    return entitled;
}

bool
process_has_entitlement_for_right(process_t proc, const char * right)
{
    bool entitled = false;
    require(right != NULL, done);

    CFTypeRef rights = NULL;
    if (proc->code_entitlements && CFDictionaryGetValueIfPresent(proc->code_entitlements, CFSTR("com.apple.private.AuthorizationServices"), &rights)) {
        if (CFGetTypeID(rights) == CFArrayGetTypeID()) {
            CFStringRef key = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, right, kCFStringEncodingUTF8, kCFAllocatorNull);
            require(key != NULL, done);
            
            CFIndex count = CFArrayGetCount(rights);
            for (CFIndex i = 0; i < count; i++) {
                if (CFEqual(CFArrayGetValueAtIndex(rights, i), key)) {
                    entitled = true;
                    break;
                }
            }
            CFReleaseSafe(key);
        }
    }
    
done:
    return entitled;
}

const char *
process_get_identifier(process_t proc)
{
    return proc->code_identifier;
}

CFDataRef
process_get_requirement_data(process_t proc)
{
    return proc->code_requirement_data;
}

SecRequirementRef
process_get_requirement(process_t proc)
{
    return proc->code_requirement;
}

bool process_verify_requirement(process_t proc, SecRequirementRef requirment)
{
    OSStatus status = SecCodeCheckValidity(proc->codeRef, kSecCSDefaultFlags, requirment);
    if (status != errSecSuccess) {
        LOGV("process[%i]: code requirement check failed (%d)", proc->auditInfo.pid, (int)status);
    }
    return (status == errSecSuccess);
}

// Returns true if the process was signed by B&I or the Mac App Store
bool process_apple_signed(process_t proc) {
    return (proc->firstPartySigned || proc->appStoreSigned);
}

// Returns true if the process was signed by B&I
bool process_firstparty_signed(process_t proc) {
	return proc->firstPartySigned;
}

mach_port_t process_get_bootstrap(process_t proc)
{
    return proc->bootstrap;
}

bool process_set_bootstrap(process_t proc, mach_port_t bootstrap)
{
    if (bootstrap != MACH_PORT_NULL) {
        if (proc->bootstrap != MACH_PORT_NULL) {
            mach_port_deallocate(mach_task_self(), proc->bootstrap);
        }
        proc->bootstrap = bootstrap;
        return true;
    }
    return false;
}

