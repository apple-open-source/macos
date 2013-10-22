//
//  SOSUserKey.c
//  sec
//
//  Created by Richard Murphy on 2/13/13.
//
//

#include <stdio.h>
#include <SecureObjectSync/SOSUserKey.h>
#include <corecrypto/ccrng.h>
#include <corecrypto/ccec.h>
#include <CommonCrypto/CommonRandomSPI.h>
#include <CoreFoundation/CFString.h>

#if 0
#include <corecrypto/ccrng_pbkdf2_prng.h>

#define UK_CONST_DECL(k,v) CFTypeRef k = (CFTypeRef)(CFSTR(v));

UK_CONST_DECL (ukSalt, "salt");
UK_CONST_DECL (ukIteration, "iteration");

static const size_t saltlen = 16;
static const unsigned long iterations = 10240;

static dispatch_once_t	keyParmStoreInit;
static CFMutableDictionaryRef keyParmStorage = NULL;
static void SOSKeyParmStore(CFStringRef user_label, CFDictionaryRef parmData) {
    dispatch_once(&keyParmStoreInit, ^{
        keyParmStorage = CFDictionaryCreateMutable(kCFAllocatorDefault, 50,  &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    });
    CFDictionaryAddValue(keyParmStorage, user_label, parmData);
}

static CFDictionaryRef SOSKeyParmRetrieve(CFStringRef user_label) {
    if(keyParmStorage && CFDictionaryContainsKey(keyParmStorage, user_label)) {
        CFDictionaryRef parmData = CFDictionaryGetValue(keyParmStorage, user_label);
        CFRetain(parmData);
        return parmData;
    }
/*
    const void *keys[] = { ukSalt, ukIteration };
    const void *values[] = { cfsalt,  cfiteration };
    CFDictionaryRef query = CFDictionaryCreate(kCFAllocatorDefault, keys, values, sizeof(keys)/sizeof(*keys), NULL, NULL);
    CFDictionaryRef retval = CFDictionaryCreate(
*/
    return NULL;
}


static void
SOSUserKeyGenParmPersist(CFStringRef user_label)
{
}

static void
SOSUserKeyGenParmRetrieve(CFStringRef user_label)
{
}
#endif

bool
SOSUserKeyGenerate(int keysize, CFStringRef user_label, CFDataRef user_password, SecKeyRef *user_pubkey, SecKeyRef *user_privkey)
{
#if 0
    ccec_const_cp_t cp = ccec_get_cp(keysize);
    ccec_full_ctx_decl_cp(cp, full_key);
    struct ccrng_pbkdf2_prng_state pbkdf2_prng;
    uint8_t salt[saltlen];
    if(CCRandomCopyBytes(kCCRandomDefault, salt, sizeof(salt)) != kCCSuccess) return false;
    uint8_t password_bytes = CFDataGetBytePtr(user_password);
    size_t password_length = CFDataGetLength(user_password);
    ccrng_pbkdf2_prng_init(&pbkdf2_prng, 72, password_length, password_bytes, sizeof(salt), salt, iterations);
    struct ccrng_state *rng = (struct ccrng_state *)&pbkdf2_prng;
    ccec_generate_key(cp, rng, full_key);
#endif

    return true;
}
