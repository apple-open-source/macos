//
//  secd-02-corruption.c
//  sec
//
//  Created by Fabrice Gautier on 5/31/13.
//
//

#include "secd_regressions.h"

#include <securityd/SecDbItem.h>
#include <utilities/array_size.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecFileLocations.h>
#include <utilities/fileIo.h>

#include <securityd/SOSCloudCircleServer.h>
#include <securityd/SecItemServer.h>

#include <Security/SecBasePriv.h>

#include <TargetConditionals.h>
#include <AssertMacros.h>

#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>

#if TARGET_OS_IPHONE && USE_KEYSTORE
#include <libaks.h>

#include "SecdTestKeychainUtilities.h"

#include "brighton_keychain_2_db.h"

static OSStatus query_one(void)
{
    OSStatus ok;

    /* querying a password */
    const void *keys[] = {
        kSecClass,
        kSecAttrServer,
    };
    const void *values[] = {
        kSecClassInternetPassword,
        CFSTR("members.spamcop.net"),
    };
    CFDictionaryRef query = CFDictionaryCreate(NULL, keys, values,
                                               array_size(keys), NULL, NULL);
    CFTypeRef results = NULL;

    ok = SecItemCopyMatching(query, &results);

    CFReleaseSafe(results);
    CFReleaseSafe(query);

    return ok;
}

    

static void *do_query(void *arg)
{
    /* querying a password */
    const void *keys[] = {
        kSecClass,
        kSecAttrServer,
    };
    const void *values[] = {
        kSecClassInternetPassword,
        CFSTR("members.spamcop.net"),
    };
    CFDictionaryRef query = CFDictionaryCreate(NULL, keys, values,
                                              array_size(keys), NULL, NULL);
    CFTypeRef results = NULL;

    for(int i=0;i<20;i++)
        verify_action(SecItemCopyMatching(query, &results)==errSecUpgradePending, return (void *)-1);

    CFReleaseSafe(query);
    
    return NULL;
}

static void *do_sos(void *arg)
{
    
    for(int i=0;i<20;i++)
        verify_action(SOSCCThisDeviceIsInCircle_Server(NULL)==-1, return (void *)-1);

    return NULL;
}


#define N_THREADS 10

int secd_02_upgrade_while_locked(int argc, char *const *argv)
{
    plan_tests(11 + N_THREADS + kSecdTestSetupTestCount);

    __block keybag_handle_t keybag;
    __block keybag_state_t state;
    char *passcode="password";
    int passcode_len=(int)strlen(passcode);

    /* custom keychain dir */
    secd_test_setup_temp_keychain("secd_02_upgrade_while_locked", ^{
        CFStringRef keychain_path_cf = __SecKeychainCopyPath();
        
        CFStringPerformWithCString(keychain_path_cf, ^(const char *keychain_path) {
            writeFile(keychain_path, brighton_keychain_2_db, brighton_keychain_2_db_len);            
        
            /* custom notification */
            SecItemServerSetKeychainChangedNotification("com.apple.secdtests.keychainchanged");
            
            /* Create and lock custom keybag */            
            ok(kIOReturnSuccess==aks_create_bag(passcode, passcode_len, kAppleKeyStoreDeviceBag, &keybag), "create keybag");
            ok(kIOReturnSuccess==aks_get_lock_state(keybag, &state), "get keybag state");
            ok(!(state&keybag_state_locked), "keybag unlocked");
            SecItemServerSetKeychainKeybag(keybag);
            
            /* lock */
            ok(kIOReturnSuccess==aks_lock_bag(keybag), "lock keybag");
            ok(kIOReturnSuccess==aks_get_lock_state(keybag, &state), "get keybag state");
            ok(state&keybag_state_locked, "keybag locked");
        });
        
        CFReleaseSafe(keychain_path_cf);
    });

    pthread_t query_thread[N_THREADS];
    pthread_t sos_thread;
    void *query_err[N_THREADS] = {NULL,};
    void *sos_err = NULL;

    for(int i=0; i<N_THREADS; i++)
        pthread_create(&query_thread[i], NULL, do_query, NULL);
    pthread_create(&sos_thread, NULL, do_sos, NULL);

    for(int i=0; i<N_THREADS; i++)
        pthread_join(query_thread[i],&query_err[i]);
    pthread_join(sos_thread, &sos_err);

    for(int i=0; i<N_THREADS; i++)
        ok(query_err[i]==NULL, "query thread ok");
    ok(sos_err==NULL, "sos thread ok");

    ok(kIOReturnSuccess==aks_unlock_bag(keybag, passcode, passcode_len), "lock keybag");
    ok(kIOReturnSuccess==aks_get_lock_state(keybag, &state), "get keybag state");
    ok(!(state&keybag_state_locked), "keybag unlocked");

    is_status(query_one(), errSecItemNotFound, "Query after unlock");

    /* Reset keybag */
    SecItemServerResetKeychainKeybag();

    return 0;
}

#else

int secd_02_upgrade_while_locked(int argc, char *const *argv)
{
    plan_tests(1);

    todo("Not yet working in simulator");

TODO: {
    ok(false);
}
    /* not implemented in simulator (no keybag) */
    /* Not implemented in OSX (no upgrade scenario) */
	return 0;
}
#endif
