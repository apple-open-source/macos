/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Portions Copyright (c) 2000 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/******************************************************************
 * The purpose of this module is to provide a local smartcard
 * authentication module for Mac OS X.
 ******************************************************************/

/* Define which PAM interfaces we provide */

#define PAM_SM_AUTH
#define PAM_SM_ACCOUNT

#define PM_DISPLAY_NAME "SmartCard"
#define MAX_PIN_RETRY (3)

/* Include PAM headers */
#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <Security/SecBase.h>
#include <OpenDirectory/OpenDirectory.h>
#include <Security/SecItem.h>
#include <Security/Security.h>
#include <Security/SecKeychainPriv.h>
#include <ctkloginhelper.h>
#include <ctkclient/ctkclient.h>
#include <pwd.h>
#include <sys/param.h>
#include "Common.h"
#include "scmatch_evaluation.h"
#include "ds_ops.h"
#include "Logging.h"

#ifdef PAM_USE_OS_LOG
PAM_DEFINE_LOG(SmartCard)
#define PAM_LOG PAM_LOG_SmartCard()
#endif

#if __has_feature(objc_arc)
#   error "this file must be compiled without ARC"
#endif

#define CFReleaseSafe(CF) { CFTypeRef _cf = (CF); if (_cf) CFRelease(_cf); }
#define PAM_OPT_PKINIT	"pkinit"

typedef NS_ENUM(NSInteger, enKeychainUnlock) {
    enNoUnlock                         = 0,
    enUnlockDirectly                   = 1,
    enUnlockUsingAhp                   = 2
};

void cleanup_func(__unused pam_handle_t *pamh, void *data, __unused int error_status)
{
	CFReleaseSafe(data);
}

CFStringRef copy_pin(pam_handle_t *pamh, const char *prompt, const char **outPin)
{
	const char *pass;
	int ret = pam_get_authtok(pamh, PAM_AUTHTOK, &pass, prompt);
	if (ret != PAM_SUCCESS) {
		_LOG_ERROR("Unable to get PIN: %d", ret);
		return NULL;
	}
	if (outPin)
		*outPin = pass;
	return CFStringCreateWithCString(kCFAllocatorDefault, pass, kCFStringEncodingUTF8);
}

Boolean copy_smartcard_unlock_context(char *buffer, size_t buffsize, CFStringRef token_id, CFStringRef pin, CFDataRef pub_key_hash, CFDataRef pub_key_hash_wrap)
{
	CFMutableDictionaryRef dict = NULL;
	CFDataRef serializedData = NULL;
	Boolean retval = FALSE;

    if (token_id == NULL || pin == NULL || pub_key_hash == NULL || pub_key_hash_wrap == NULL) {
        _LOG_ERROR("SC context - wrong params");
        return retval;
    }

	dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 4, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (dict == NULL) {
        _LOG_ERROR("SC context - invalid dict");
        return retval;
    }

	CFDictionarySetValue(dict, kSecAttrTokenID, token_id);
	CFDictionarySetValue(dict, kSecAttrService, pin);
	CFDictionarySetValue(dict, kSecAttrPublicKeyHash, pub_key_hash);
	CFDictionarySetValue(dict, kSecAttrAccount, pub_key_hash_wrap);
	serializedData = CFPropertyListCreateData(kCFAllocatorDefault, dict, kCFPropertyListBinaryFormat_v1_0, 0, NULL);
    if (serializedData == NULL) {
        _LOG_ERROR("SC context - invalid serialized data");
        goto cleanup;
    }

    @autoreleasepool {
        CFDataRef encodedData = (CFDataRef)[(NSData *)serializedData base64EncodedDataWithOptions:0];
        if (encodedData) {
            CFStringRef stringFromData = CFStringCreateWithBytes (kCFAllocatorDefault, CFDataGetBytePtr(encodedData), CFDataGetLength(encodedData), kCFStringEncodingUTF8, TRUE);
            if (stringFromData) {
                retval = CFStringGetCString(stringFromData, buffer, buffsize, kCFStringEncodingUTF8);
                _LOG_DEBUG("Keychain unlock data size %zu", buffsize);
                CFRelease(stringFromData);
            } else {
                _LOG_ERROR("SC context - not valid string");
            }
        } else {
            _LOG_ERROR("SC context - invalid encoded data");
        }
    }

cleanup:
	CFReleaseSafe(dict);
	CFReleaseSafe(serializedData);

	return retval;
}

CFDataRef get_key_hash_wrap(CFDictionaryRef context, CFStringRef keys, CFStringRef values, CFStringRef token_id)
{
	CFArrayRef wrap_hashes = CFDictionaryGetValue(context, keys);
	CFArrayRef token_ids = CFDictionaryGetValue(context, values);
	if (wrap_hashes && token_ids) {
		CFIndex count = CFArrayGetCount(token_ids);
		for (CFIndex i = 0; i < count; ++i) {
			CFStringRef wrap_token_id = CFArrayGetValueAtIndex(token_ids, i);
			if (CFEqual(token_id, wrap_token_id)) {
				return CFArrayGetValueAtIndex(wrap_hashes, i);
			}
		}
	}
	return NULL;
}

PAM_EXTERN int pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
    char prompt[PATH_MAX * 2];
    char certificate_name[PATH_MAX];
    int retval = PAM_AUTH_ERR;
    const char *user = NULL;
    const char *pin = NULL;
    CFStringRef cf_pin = NULL;
    OSStatus status;
    ODRecordRef od_record = NULL;
    CFDataRef *token_context = NULL;
    CFDataRef pub_key_hash = NULL;
    CFDataRef pub_key_hash_wrap = NULL;
    CFStringRef token_id = NULL;
    CFStringRef kerberos_principal = NULL;
    CFPropertyListRef context = NULL;
    CFStringRef cf_user = NULL;
    CFErrorRef error = NULL;
    Boolean interactive;
    uid_t agent_uid = 0;
    Boolean no_ignore = FALSE;
    CFDictionaryRef all_tokens_data = NULL;
    Boolean valid_user = FALSE;
    CFStringRef authorization_right = NULL;
    
    _LOG_DEBUG("pam_sm_authenticate");
    
    if (NULL != openpam_get_option(pamh, "no_ignore")) {
        no_ignore = TRUE;
    }
    
    retval = pam_get_user(pamh, &user, "Username: ");
    if (retval != PAM_SUCCESS) {
        _LOG_ERROR("%s - Unable to get the username: %s", PM_DISPLAY_NAME, pam_strerror(pamh, retval));
        return retval;
    }
    
    if (user == NULL || *user == '\0') {
        _LOG_ERROR("%s - Username is invalid.", PM_DISPLAY_NAME);
        return retval;
    }
    
    struct passwd *pwd = getpwnam(user);
    if (pwd == NULL) {
        _LOG_ERROR("%s - Unable to get user details.", PM_DISPLAY_NAME);
        return retval;
    }
    
    retval = od_record_create_cstring(pamh, &od_record, (const char*)user);
    if (retval != PAM_SUCCESS) {
        _LOG_ERROR("%s - Unable to get user record %d.", PM_DISPLAY_NAME, retval);
        goto cleanup;
    }
    cf_user = CFStringCreateWithCString(kCFAllocatorDefault, user, kCFStringEncodingUTF8);
    if (openpam_get_option(pamh, PAM_OPT_PKINIT)) {
        pam_get_data(pamh, "kerberos", (void *)&kerberos_principal);
        if (kerberos_principal) {
            CFRetain(kerberos_principal);
            _LOG_DEBUG("%s - Kerberos Principal passed in variable", PM_DISPLAY_NAME);
        }
    } else {
        _LOG_DEBUG("%s - Trying to get kerbPrincipal from OD", PM_DISPLAY_NAME);
        CFArrayRef records = find_user_record_by_attr_value(kDSNAttrRecordName, user);
        if (records) {
            for (CFIndex i = 0; i < CFArrayGetCount(records); ++i) {
                CFTypeRef userRecord = CFArrayGetValueAtIndex(records, i);
                kerberos_principal = GetPrincipalFromUser(userRecord);
                if (kerberos_principal) {
                    break;
                }
            }
        }
    }
    if (kerberos_principal) {
        _LOG_DEBUG("%s - Will ask for kerberos ticket", PM_DISPLAY_NAME);
    }

    pam_get_data(pamh, "authorization_right", (void *)&authorization_right);
    if (authorization_right) {
        CFRetain(authorization_right);
        _LOG_DEBUG("%s - evaluating authorization right %@", PM_DISPLAY_NAME, authorization_right);
    }
    
    uid_t *tmpUid;
    if (PAM_SUCCESS == pam_get_data(pamh, "agent_uid", (void *)&tmpUid)) {
        agent_uid = *tmpUid;
    } else {
        agent_uid = 0; // 0 means autodetect for TKFunctions
    }
    _LOG_DEBUG("%s - using %d as agent uid", PM_DISPLAY_NAME, agent_uid);
    
    // get the token context
    pam_get_data(pamh, "token_ctk", (void *)&token_context); // we do not need to check status as token_id presence reveals success
    interactive = token_context == NULL;

    CFMutableDictionaryRef hints = CFDictionaryCreateMutable(kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
     if (hints && cf_user) {
         CFDictionaryAddValue(hints, CFSTR(kTKXpcKeyUserName), cf_user);
         all_tokens_data = TKCopyAvailableTokensInfo(agent_uid, hints);
    }
    
    if (all_tokens_data) {
        CFArrayRef mappedUsers = CFDictionaryGetValue(all_tokens_data, CFSTR("0")); // 0 is APEventHintUsers from AuthenticationHintsProvider
        if (mappedUsers && CFArrayContainsValue(mappedUsers, CFRangeMake(0, CFArrayGetCount(mappedUsers)), cf_user)) {
            valid_user = TRUE;
        }
    }
    
    if (!valid_user) {
        _LOG_ERROR("%s - User %s is not paired with any smartcard.", PM_DISPLAY_NAME, user);
        retval = PAM_AUTH_ERR;
        goto cleanup;
    }
    
    if (interactive) {
        CFDataRef contextData = CFDictionaryGetValue(all_tokens_data, CFSTR(kTkHintContextData));
        if (contextData) {
            context = CFPropertyListCreateWithData(kCFAllocatorDefault, contextData, 0, NULL, NULL);
        }
    } else {
        context = CFPropertyListCreateWithData(kCFAllocatorDefault, *token_context, 0, NULL, NULL);
    }
    
    if (context == NULL) {
        _LOG_ERROR("%s - Cannot find context", PM_DISPLAY_NAME);
        goto cleanup;
    }
    
    CFDictionaryRef pub_key_hashes = CFDictionaryGetValue(context, CFSTR(kTkHintAllPubkeyHashes));
    if (pub_key_hashes) {
        CFStringRef user_string = CFStringCreateWithCString(kCFAllocatorDefault, user, kCFStringEncodingUTF8);
        if (user_string) {
            pub_key_hash = (CFDataRef)CFDictionaryGetValue(pub_key_hashes, user_string);
            if (pub_key_hash) {
                CFRetain(pub_key_hash);
            }
            CFRelease(user_string);
        }
    }
    
    if (pub_key_hash) {
        // these arrays are keys and values, the same length
        CFArrayRef hashes = CFDictionaryGetValue(context, CFSTR(kTkHintTokenNameHashes));
        CFArrayRef token_ids = CFDictionaryGetValue(context, CFSTR(kTkHintTokenNameIds));
        CFIndex count = CFArrayGetCount(hashes);
        for (CFIndex i = 0; i < count; ++i) {
            CFDataRef hash = CFArrayGetValueAtIndex(hashes, i);
            if (CFEqual(hash, pub_key_hash)) {
                token_id = CFArrayGetValueAtIndex(token_ids, i);
                CFRetain(token_id);
                break;
            }
        }
        if (interactive) { // prepare prompt for interactive password request
            CFArrayRef hashes = CFDictionaryGetValue(context, CFSTR(kTkHintFriendlyNameHashes));
            CFArrayRef friendlyNames = CFDictionaryGetValue(context, CFSTR(kTkHintFriendlyNames));
            CFIndex count = CFArrayGetCount(hashes);
            CFStringRef certificateName = NULL;
            for (CFIndex i = 0; i < count; ++i) {
                CFDataRef hash = CFArrayGetValueAtIndex(hashes, i);
                if (CFEqual(hash, pub_key_hash)) {
                    certificateName = CFArrayGetValueAtIndex(friendlyNames, i);
                    break;
                }
            }
            
            if (!certificateName || !CFStringGetCString(certificateName, (char *)&certificate_name, PATH_MAX - 1, kCFStringEncodingUTF8))
                strncpy(certificate_name, "Unnamed certificate", PATH_MAX - 1);
            
            snprintf(prompt, sizeof(prompt), "Enter PIN for '%s': ", certificate_name);
        } else if (token_id) { // first try RSA keys
            pub_key_hash_wrap = get_key_hash_wrap(context, CFSTR(kTkHintUnlockTokenHashes), CFSTR(kTkHintUnlockTokenIds), token_id);
            if (pub_key_hash_wrap) {
                CFRetain(pub_key_hash_wrap);
                _LOG_DEBUG("Wrap key found.");
            }
        }
    }
    CFRelease(context);

	retval = (interactive && !no_ignore) ? PAM_IGNORE : PAM_AUTH_ERR;
    
    for (int i = 0; i < (interactive ? MAX_PIN_RETRY : 1); ++i) {
        CFReleaseNull(cf_pin);
        cf_pin = copy_pin(pamh, prompt, &pin); // gets pre-stored password for Authorization / asks user during interactive session
        if (cf_pin) {
            status = TKPerformLogin(agent_uid, token_id, pub_key_hash, cf_pin, kerberos_principal, &error);
            _LOG_DEBUG("%s - Smartcard verification result %d", PM_DISPLAY_NAME, (int)status);
            if (status == errSecSuccess) {
                CFReleaseSafe(error);
                // after successfull auth, cache pubkeyhash in OD for FVUnlock mode
                // TKBindUserAm checks if caching is necessary
                TKBindUserAm(cf_user, pub_key_hash, NULL);
                retval = PAM_SUCCESS;
                break;
            } else if (status == kTKErrorCodeAuthenticationFailed) {
                // existing ahp_error is automatically released by pam_set_data when setting a new one
                pam_set_data(pamh, "ahp_error", (void *)error, cleanup_func); // automatically releases previous object
                error = nil; // already transferred to ahp_error
            } else {
                CFReleaseNull(error);
                break; // do not retry on other errors than PIN failed
            }
        } else {
            _LOG_ERROR("%s - Unable to get %s PIN", PM_DISPLAY_NAME, interactive ? "interactive":"stored");
        }
    }

	if (interactive == FALSE && retval == PAM_SUCCESS && token_id) {
		// for authorization set token ID
		CFRetain(token_id);
		pam_set_data(pamh, "token_id", (void *)token_id, cleanup_func);
        NSString *auth_right = (NSString *)authorization_right;
        NSString *exeName = NSBundle.mainBundle.executablePath;
        enKeychainUnlock keychainUnlockWay = enNoUnlock;
        if ([exeName isEqualToString:@"/System/Library/CoreServices/loginwindow.app/Contents/MacOS/loginwindow"]) {
            // loginwindow uses this module just to unlock the screen when switched to PAM mode
            keychainUnlockWay = enUnlockDirectly;
        } else if ([auth_right isEqualToString:@"system.login.screensaver"]) {
            // auth_right is set only by authorizationhost so its implied that we are invoked from authorizationhost
            keychainUnlockWay = enUnlockUsingAhp;
        }
        if (keychainUnlockWay != enNoUnlock) {
            char pin_context_buff[PATH_MAX];
            if (copy_smartcard_unlock_context(pin_context_buff, PATH_MAX, token_id, cf_pin, pub_key_hash, pub_key_hash_wrap)) {
                // check if we are running in the user session as this means we are called inside screensaver unlock
                _LOG_DEBUG("%s - Going to unlock keychain", PM_DISPLAY_NAME);
                if (keychainUnlockWay == enUnlockDirectly) {
                    // unlock user keychain
                    status = SecKeychainLogin((UInt32)strlen(user), user, (UInt32)strlen(pin_context_buff), pin_context_buff);
                    _LOG_DEBUG("%s - Keychain unlock result %d", PM_DISPLAY_NAME, (int)status);
                } else if (keychainUnlockWay == enUnlockUsingAhp){
                    NSString *pinData = [NSString stringWithUTF8String:pin_context_buff];
                    if (pinData) {
                        status = TKUnlockKeybag(pwd->pw_uid, (__bridge CFStringRef)pinData);
                        _LOG_DEBUG("%s - TKUnlockKeybag unlock result %d", PM_DISPLAY_NAME, (int)status);
                    } else {
                        _LOG_ERROR("%s - Unable to construct pindata for keychain unlock", PM_DISPLAY_NAME);
                    }
                }
            } else {
                _LOG_ERROR("%s - Unable to get data for keychain unlock", PM_DISPLAY_NAME);
            }
        } else {
            _LOG_DEBUG("%s - keychain unlock not needed", PM_DISPLAY_NAME);
        }
	}

cleanup:
	CFReleaseSafe(od_record);
	CFReleaseSafe(token_id);
	CFReleaseSafe(pub_key_hash);
	CFReleaseSafe(pub_key_hash_wrap);
	CFReleaseSafe(cf_user);
    CFReleaseSafe(cf_pin);
    CFReleaseSafe(kerberos_principal);
    CFReleaseSafe(all_tokens_data);
    CFReleaseSafe(authorization_right);
	return retval;
}

PAM_EXTERN int pam_sm_setcred(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_chauthtok(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_acct_mgmt(pam_handle_t * pamh, int flags, int argc, const char **argv)
{
	return PAM_SUCCESS;
}
