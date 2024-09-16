//
//  keybag_helpers.c
//  securityd_macos
//
//  Created by Fabrice Gautier on 7/26/23.
//

#include "keybag_helpers.h"
#include "utilities/debugging.h"

#include <os/feature_private.h>
#include <AppleKeyStore/applekeystored_client.h>
#include <IOKit/IOKitLib.h>
#include <Kernel/IOKit/crypto/AppleFDEKeyStoreDefs.h>
#include <Security/Security.h>
#include <AssertMacros.h>


/*
   This shim layer redirect to either securityd_service or applekeystored, based on feature flag.

   This essentially does the same as the securityd_service_client library, but in the securityd process.
   The securityd_service_client static library can't really be modified to support this without requiring change
   on other client like loginwindow. This shim layer goes directly into securityd.

   For the stashing calls, when the applekeystored feature flag is enabled, securityd will do the part talking
   to AppleFDEKeyStore rather than have that done in applekeystored.

   Eventually this should all go away once keychain stop being on the path responsible for handling keybag.
   (except part of the stashing call that talks to AppleFDEKeyStore)
 */

#define AKSD_CTX(context) {     \
    .s_id = context->s_id,      \
    .s_uid = context->s_uid,    \
    .procToken = context->procToken, \
    .kcv = context->kcv \
}

static int
_service_return_code(int rc)
{
    if (os_feature_enabled(AppleKeyStore, applekeystored)) {
        switch(rc) {
            case kAKSReturnSuccess: return KB_Success;
            case kAKSReturnNotFound: return KB_BagNotFound;
            case kAKSReturnBadData: return KB_BagError;
            case kAKSReturnNotPrivileged: return KB_BagNotLoaded;
            case kAKSReturnBusy: return KB_BagExists;
            case kAKSReturnIPCError: return KB_InvalidSession;
            default:
                return KB_GeneralError;
        }
    } else {
        return KB_Unsupported;
    }
}

int
kb_create(service_context_t *context, const void * secret, int secret_len)
{
    if (os_feature_enabled(AppleKeyStore, applekeystored)) {
        applekeystored_context_t aksd_ctx = AKSD_CTX(context);
        return _service_return_code(applekeystored_client_kb_create(&aksd_ctx, secret, secret_len));
    }

    return KB_Unsupported;
}

int
kb_load(service_context_t *context)
{
    if (os_feature_enabled(AppleKeyStore, applekeystored)) {
        applekeystored_context_t aksd_ctx = AKSD_CTX(context);
        return _service_return_code(applekeystored_client_kb_load(&aksd_ctx));
    }

    return KB_Unsupported;
}


int
kb_save(service_context_t *context)
{
    if (os_feature_enabled(AppleKeyStore, applekeystored)) {
        applekeystored_context_t aksd_ctx = AKSD_CTX(context);
        return _service_return_code(applekeystored_client_kb_save(&aksd_ctx));
    }

    return KB_Unsupported;
}

int
kb_unlock(service_context_t *context, const void * secret, int secret_len)
{
    if (os_feature_enabled(AppleKeyStore, applekeystored)) {
        applekeystored_context_t aksd_ctx = AKSD_CTX(context);
        return _service_return_code(applekeystored_client_kb_unlock(&aksd_ctx, secret, secret_len));
    }

    return KB_Unsupported;
}

int
kb_change_secret(service_context_t *context, const void * secret, int secret_len, const void * new_secret, int new_secret_len)
{
    if (os_feature_enabled(AppleKeyStore, applekeystored)) {
        applekeystored_context_t aksd_ctx = AKSD_CTX(context);
        return _service_return_code(applekeystored_client_kb_change_secret(&aksd_ctx, secret, secret_len, new_secret, new_secret_len));
    }

    return KB_Unsupported;
}

int
kb_reset(service_context_t *context, const void * secret, int secret_len)
{
    if (os_feature_enabled(AppleKeyStore, applekeystored)) {
        applekeystored_context_t aksd_ctx = AKSD_CTX(context);
        return _service_return_code(applekeystored_client_kb_reset(&aksd_ctx, secret, secret_len));
    }

    return KB_Unsupported;
}

int kb_is_locked(service_context_t *context, bool *locked, bool *no_pin)
{
    if (os_feature_enabled(AppleKeyStore, applekeystored)) {
        applekeystored_context_t aksd_ctx = AKSD_CTX(context);
        return _service_return_code(applekeystored_client_kb_is_locked(&aksd_ctx, locked, no_pin));
    }

    return KB_Unsupported;
}

int
kb_unwrap_key(__unused service_context_t *context, __unused const void *wrapped_key, __unused int wrapped_key_size, __unused keyclass_t wrapped_key_class, __unused void **key, __unused int *key_size)
{
    // The "wrap" and "unwrap" features were to be used to unlock the legacy keychain using a keybag key.
    // The work was not fully completed, so in effect this is currently never used, and the "wrap" side seems to be missing.
    // Furthermore, there is no reason that the wrap/unwrap needs to be done in another daemon. The only reason it was started
    // that way was for securityd_service to load the keybag in case it was not done yet. This should not be an issue in the future.
    // Bottom line is that applekeystored wont support wrap/unwrap operation.
    // Radar references:
    // rdar://29481260 (Unlock keychain using keybag when possible)
    secerror("kb_unwrap_key call not supported in applekeystored");
    return KB_Unsupported;
}

/* AppleFDEKeyStore helper functions */
static io_connect_t
open_fdekeystore(void)
{
    io_registry_entry_t service;
    io_connect_t conn;
    kern_return_t kr;

    service = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching(kAppleFDEKeyStoreServiceName));
    if (service == IO_OBJECT_NULL)
        return IO_OBJECT_NULL;

    kr = IOServiceOpen(service, mach_task_self(), 0, &conn);
    if (kr != KERN_SUCCESS)
        return IO_OBJECT_NULL;

    kr = IOConnectCallMethod(conn, kAppleFDEKeyStoreUserClientOpen, NULL, 0, NULL, 0, NULL, NULL, NULL, NULL);
    if (kr != KERN_SUCCESS) {
        IOServiceClose(conn);
        return IO_OBJECT_NULL;
    }

    return conn;
}

static void
close_fdekeystore(io_connect_t conn)
{
    kern_return_t kr;
    kr = IOConnectCallMethod(conn, kAppleFDEKeyStoreUserClientClose, NULL, 0, NULL, 0, NULL, NULL, NULL, NULL);
    if (kr != KERN_SUCCESS)
        return;
    IOServiceClose(conn);
}

// Create an keybag escrow bag with the keychain master key.
// If succesfull, stash the keychain master key in the AppleFDEKeyStore and
// flag it as the keychain master key to be added to the
// reboot NVRAM blob.
//
// This requires two calls to the AppleFDEKeystore: the first to store the
// key and get its uuid.  The second uses the uuid to flag the
// key for blob inclusion.
static OSStatus
_stash_set_key(service_context_t * context, const uint8_t *keydata, size_t keydata_len)
{
    kern_return_t kr = KERN_INVALID_ARGUMENT;
    io_connect_t conn = IO_OBJECT_NULL;
    size_t len;

    require(keydata, done);
    require(keydata_len <= MAX_KEY_SIZE, done);

    applekeystored_context_t aksd_ctx = AKSD_CTX(context);
    int aksd_rc = applekeystored_client_stash_create(&aksd_ctx, keydata, (unsigned)keydata_len);
    require_action(aksd_rc == kAKSReturnSuccess || aksd_rc == kAKSReturnUnsupported, done, secerror("stash create failed with 0x%08x", kr); kr = aksd_rc);

    conn = open_fdekeystore();
    require(conn, done);

    // Store the key in the keystore and get its uuid
    setKeyGetUUID_InStruct_t inStruct1;
    uuid_OutStruct_t outStruct1;

    _Static_assert(sizeof(inStruct1.inKey.key.key) == MAX_KEY_SIZE, "unexpected aks raw key size");
    memcpy(&inStruct1.inKey.key.key, keydata, keydata_len);
    inStruct1.inKey.key.keysize = (cryptosize_t) keydata_len;
    len = sizeof(outStruct1);
    kr = IOConnectCallMethod(conn, kAppleFDEKeyStore_setKeyGetUUID,
                             NULL, 0,
                             &inStruct1, sizeof(inStruct1),
                             NULL, NULL,
                             &outStruct1, &len);
    require_action(kr == KERN_SUCCESS, done, secerror("setKeyGetUUID failed with 0x%08x", kr));

    // Now using the uuid stash it as the master key
    setStashKey_InStruct_t inStruct2;
    _Static_assert(sizeof(outStruct1.uuid) == sizeof(inStruct2.uuid), "invalid uuid size(s)");
    memcpy(&inStruct2.uuid, &outStruct1.uuid, sizeof(outStruct1.uuid));
    inStruct2.type  = kAppleFDEKeyStoreStash_master;

    kr = IOConnectCallMethod(conn, kAppleFDEKeyStore_setStashKey,
                             NULL, 0,
                             &inStruct2, sizeof(inStruct2),
                             NULL, NULL,
                             NULL, NULL);
    require_action(kr == KERN_SUCCESS, done, secerror("setStashKey failed with 0x%08x", kr));

done:
    secinfo("keybag", "set stashkey %d", (int)kr);

    if (conn)
        close_fdekeystore(conn);

    return kr;
}


int
kb_stash_set_key(service_context_t *context, const void * key, int key_len)
{
    if (os_feature_enabled(AppleKeyStore, applekeystored)) {
        return _stash_set_key(context, key, key_len);
    }

    return KB_Unsupported;
}

int
kb_stash_load_key(service_context_t *context, const void * key, int key_len)
{
    if (os_feature_enabled(AppleKeyStore, applekeystored)) {
        applekeystored_context_t aksd_ctx = AKSD_CTX(context);
        return _service_return_code(applekeystored_client_stash_load(&aksd_ctx, key, key_len, true));
    }

    return KB_Unsupported;
}

//
// Get the keychain master key from the AppleFDEKeyStore.
// Note that this is a one-time call - the master key is
// removed from the keystore after it is returned.
// Requires the entitlement: com.apple.private.securityd.keychain
//
static OSStatus
_stash_get_key(service_context_t * context, void ** key, size_t * key_len)
{
    getStashKey_InStruct_t inStruct;
    getStashKey_OutStruct_t outStruct;
    size_t outSize = sizeof(outStruct);
    kern_return_t kr = KERN_INVALID_ARGUMENT;

    io_connect_t conn = open_fdekeystore();
    require(conn, done);
    inStruct.type = kAppleFDEKeyStoreStash_master;

    kr = IOConnectCallMethod(conn, kAppleFDEKeyStore_getStashKey,
                             NULL, 0,
                             &inStruct, sizeof(inStruct),
                             NULL, NULL,
                             &outStruct, &outSize);

    if (kr == KERN_SUCCESS) {
        int aksd_rc;
        applekeystored_context_t aksd_ctx = AKSD_CTX(context);
        *key = calloc(1u, outStruct.outBuf.key.keysize);
        memcpy(*key, outStruct.outBuf.key.key, outStruct.outBuf.key.keysize);
        *key_len = outStruct.outBuf.key.keysize;
        aksd_rc = applekeystored_client_stash_load(&aksd_ctx, outStruct.outBuf.key.key, outStruct.outBuf.key.keysize, false);
        secinfo("keybag", "stash_load: 0x%08x", aksd_rc);
    } else {
        secerror("failed to get stash key from AppleFDEKeyStore: %d", (int)kr);
    }

done:
    if (conn)
        close_fdekeystore(conn);

    return kr;
}


int
kb_stash_get_key(service_context_t *context, void ** key, size_t * key_len)
{
    if (os_feature_enabled(AppleKeyStore, applekeystored)) {
        return _stash_get_key(context, key, key_len);
    }
    
    return KB_Unsupported;
}
