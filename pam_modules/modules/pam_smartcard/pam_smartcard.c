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

#define CFReleaseSafe(CF) { CFTypeRef _cf = (CF); if (_cf) CFRelease(_cf); }
#define PAM_OPT_PKINIT	"pkinit"

void cleanup_func(__unused pam_handle_t *pamh, void *data, __unused int error_status)
{
	CFReleaseSafe(data);
}

CFStringRef copy_pin(pam_handle_t *pamh, const char *prompt, const char **outPin)
{
	const char *pass;
	int ret = pam_get_authtok(pamh, PAM_AUTHTOK, &pass, prompt);
	if (ret != PAM_SUCCESS) {
		openpam_log(PAM_LOG_ERROR, "Unable to get PIN: %d", ret);
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
	SecTransformRef encoder = NULL;
	Boolean retval = FALSE;

	if (token_id == NULL || pin == NULL || pub_key_hash == NULL || pub_key_hash_wrap == NULL)
		return retval;

	dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 4, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (dict == NULL)
		return retval;

	CFDictionarySetValue(dict, kSecAttrTokenID, token_id);
	CFDictionarySetValue(dict, kSecAttrService, pin);
	CFDictionarySetValue(dict, kSecAttrPublicKeyHash, pub_key_hash);
	CFDictionarySetValue(dict, kSecAttrAccount, pub_key_hash_wrap);
	serializedData = CFPropertyListCreateData(kCFAllocatorDefault, dict, kCFPropertyListBinaryFormat_v1_0, 0, NULL);
	if (serializedData == NULL)
		goto cleanup;

	encoder = SecEncodeTransformCreate(kSecBase64Encoding, NULL);
	if (encoder == NULL)
		goto cleanup;
	if (SecTransformSetAttribute(encoder, kSecTransformInputAttributeName, serializedData, NULL) == FALSE)
		goto cleanup;

	CFDataRef encodedData = SecTransformExecute(encoder, NULL);
	if (encodedData) {
		CFStringRef stringFromData = CFStringCreateWithBytes (kCFAllocatorDefault, CFDataGetBytePtr(encodedData), CFDataGetLength(encodedData), kCFStringEncodingUTF8, TRUE);
		if (stringFromData) {
			retval = CFStringGetCString(stringFromData, buffer, buffsize, kCFStringEncodingUTF8);
			openpam_log(PAM_LOG_DEBUG, "Keychain unlock data %s", buffer);
			CFRelease(stringFromData);
		}
		CFRelease(encodedData);
	}

cleanup:
	CFReleaseSafe(dict);
	CFReleaseSafe(serializedData);
	CFReleaseSafe(encoder);

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
// legacy support block start
	SecKeychainRef keychain = NULL;
	SecIdentityRef identity = NULL;
	SecCertificateRef certificate = NULL;
	SecKeyRef pub_key = NULL;
	SecKeyRef private_key = NULL;
	Boolean keychain_unlocked = FALSE;
// legacy support block end

    if (NULL != openpam_get_option(pamh, "no_ignore")) {
        no_ignore = TRUE;
    }

	retval = pam_get_user(pamh, &user, "Username: ");
	if (retval != PAM_SUCCESS) {
		openpam_log(PAM_LOG_ERROR, "%s - Unable to get the username: %s", PM_DISPLAY_NAME, pam_strerror(pamh, retval));
		return retval;
	}

	if (user == NULL || *user == '\0') {
		openpam_log(PAM_LOG_ERROR, "%s - Username is invalid.", PM_DISPLAY_NAME);
		return retval;
	}

	struct passwd *pwd = getpwnam(user);
	if (pwd == NULL) {
		openpam_log(PAM_LOG_ERROR, "%s - Unable to get user details.", PM_DISPLAY_NAME);
		return retval;
	}

	retval = od_record_create_cstring(pamh, &od_record, (const char*)user);
	if (retval != PAM_SUCCESS) {
		openpam_log(PAM_LOG_ERROR, "%s - Unable to get user record %d.", PM_DISPLAY_NAME, retval);
		goto cleanup;
	}
	cf_user = CFStringCreateWithCString(kCFAllocatorDefault, user, kCFStringEncodingUTF8);

	if (openpam_get_option(pamh, PAM_OPT_PKINIT)) {
		pam_get_data(pamh, "kerberos", (void *)&kerberos_principal);
		if (kerberos_principal) {
			openpam_log(PAM_LOG_DEBUG, "%s - Will ask for kerberos ticket", PM_DISPLAY_NAME);
		}
	}

	uid_t *tmpUid;
	if (PAM_SUCCESS == pam_get_data(pamh, "agent_uid", (void *)&tmpUid))
		agent_uid = *tmpUid;
	else
		agent_uid = 0; // 0 means autodetect for TKFunctions

	openpam_log(PAM_LOG_DEBUG, "%s - using %d as agent id", PM_DISPLAY_NAME, agent_uid);

	// get the token context
	pam_get_data(pamh, "token_ctk", (void *)&token_context); // we do not need to check status as token_id presence reveals success
	if (token_context) {
		interactive = FALSE;
		Boolean valid_user = FALSE;
		// verify that current user is still paired
		CFDictionaryRef all_tokens_data = TKCopyAvailableTokensInfo(agent_uid, NULL);
		if (all_tokens_data) {
			CFArrayRef mappedUsers = CFDictionaryGetValue(all_tokens_data, CFSTR("0")); // 0 is APEventHintUsers from AuthenticationHintsProvider
			if (mappedUsers && CFArrayContainsValue(mappedUsers, CFRangeMake(0, CFArrayGetCount(mappedUsers)), cf_user)) {
				valid_user = TRUE;
			}
			CFReleaseSafe(all_tokens_data);
		}

		if (!valid_user) {
			openpam_log(PAM_LOG_ERROR, "%s - User is not paired with any smartcard.", PM_DISPLAY_NAME);
			retval = PAM_AUTH_ERR;
			goto cleanup;
		}
		context = CFPropertyListCreateWithData(kCFAllocatorDefault, *token_context, 0, NULL, NULL);
	} else {
		interactive = TRUE;
		CFIndex number_of_tokens;
		CFMutableDictionaryRef hints = CFDictionaryCreateMutable(kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		if (hints && cf_user) {
			CFDictionaryAddValue(hints, CFSTR(kTKXpcKeyUserName), cf_user);
			CFDictionaryRef all_tokens_data = TKCopyAvailableTokensInfo(agent_uid, hints);
			if (all_tokens_data && (number_of_tokens = CFDictionaryGetCount(all_tokens_data)) > 0) {
				// smartCardData is dictionary tokenId -> AHP data
				CFDataRef contextData = CFDictionaryGetValue(all_tokens_data, CFSTR(kTkHintContextData));
				if (contextData) {
					context = CFPropertyListCreateWithData(kCFAllocatorDefault, contextData, 0, NULL, NULL);
				}
			}
			CFReleaseSafe(all_tokens_data);
		}
		CFReleaseSafe(hints);
	}

	// do not return error for interactive session (sudo etc.) unless card login really failed
	retval = (interactive && !no_ignore) ? PAM_IGNORE : PAM_AUTH_ERR;

	if (context != NULL) {
		// using modern smartcard support
		CFDictionaryRef pub_key_hashes = CFDictionaryGetValue(context, CFSTR(kTkHintAllPubkeyHashes));
		if (pub_key_hashes) {
			CFStringRef user_string = CFStringCreateWithCString(kCFAllocatorDefault, user, kCFStringEncodingUTF8);
			if (user_string) {
				pub_key_hash = (CFDataRef)CFDictionaryGetValue(pub_key_hashes, user_string);
				if (pub_key_hash)
					CFRetain(pub_key_hash);
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
						CFRetain(token_id);
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
					openpam_log(PAM_LOG_DEBUG, "Wrap key found.");
				}
			}
		}
		CFRelease(context);
	}

	if (!token_id || !pub_key_hash ) {
// legacy support block start
		// no modern smartcard found
		// so look for the legacy smartcard
		interactive = true;
		UInt32 pathLength = PATH_MAX;
		char pathName[PATH_MAX];
		keychain = copySmartCardKeychainForUser(od_record, user, &identity);
		if (keychain == NULL)
		{
			openpam_log(PAM_LOG_DEBUG, "%s - No legacy smartcard found for this user.", PM_DISPLAY_NAME);
            retval = !no_ignore ? PAM_IGNORE : PAM_AUTH_ERR;
			goto cleanup;
		}

		// first try to check if this keychain is already unlocked
		SecKeychainStatus keychainStatus;
		status = SecKeychainGetStatus(keychain, &keychainStatus);
		if (status == errSecSuccess && (keychainStatus & kSecUnlockStateStatus)) {
			keychain_unlocked = true;
		} else {
			status = SecKeychainGetPath(keychain, &pathLength, pathName);
			if (status) {
				openpam_log(PAM_LOG_ERROR, "%s - Unable to get smart card name: %d", PM_DISPLAY_NAME, (int)status);
				goto cleanup;
			}

			snprintf(prompt, sizeof(prompt), "Enter PIN for '%s': ", pathName);
		}
	}
// legacy support block end

	retval = (interactive && !no_ignore) ? PAM_IGNORE : PAM_AUTH_ERR;
	if (keychain_unlocked == FALSE) {
		for(int i = 0; i < (interactive ? MAX_PIN_RETRY : 1); ++i) {
            CFReleaseNull(cf_pin);
			cf_pin = copy_pin(pamh, prompt, &pin); // gets pre-stored password for Authorization / asks user during interactive session
			if (cf_pin) {
				if (keychain) {
					// legacy smartcard check
					status = SecKeychainUnlock(keychain, (UInt32)strlen(pin), pin, TRUE);
				} else {
					// modern smartcard check
					status = TKPerformLogin(agent_uid, token_id, pub_key_hash, cf_pin, kerberos_principal, &error);
				}
				openpam_log(PAM_LOG_DEBUG, "%s - Smartcard verification result %d", PM_DISPLAY_NAME, (int)status);
				if (status == errSecSuccess) {
					CFReleaseSafe(error);
                    // after successfull auth, cache pubkeyhash in OD for FVUnlock mode
                    // TKBindUserAm checks if caching is necessary
                    TKBindUserAm(cf_user, pub_key_hash, NULL);
					retval = PAM_SUCCESS;
					break;
				} else if (status == kTKErrorCodeAuthenticationFailed && keychain == NULL) {
					// existing ahp_error is automatically released by pam_set_data when setting a new one
					pam_set_data(pamh, "ahp_error", (void *)error, cleanup_func); // automatically releases previous object
				} else {
					CFReleaseSafe(error);
					break; // do not retry on other errors than PIN failed
				}
			} else {
				openpam_log(PAM_LOG_ERROR, "%s - Unable to get %s PIN", PM_DISPLAY_NAME, interactive ? "interactive":"stored");
			}
		}
	} else {
		openpam_log(PAM_LOG_DEBUG, "%s - Smartcard is already unlocked", PM_DISPLAY_NAME);
		retval = PAM_SUCCESS;
	}

// legacy support block start
	if (keychain != NULL && retval == PAM_SUCCESS) {
        retval = !no_ignore ? PAM_IGNORE : PAM_AUTH_ERR;

		// legacy smartcard unlock succeeded
		status = SecIdentityCopyCertificate(identity, &certificate);
		if (status != errSecSuccess) {
			openpam_log(PAM_LOG_ERROR, "%s - failed to get certificate", PM_DISPLAY_NAME);\
			goto cleanup;
		}

		status = SecCertificateCopyPublicKey(certificate, &pub_key);
		if (status != errSecSuccess) {
			openpam_log(PAM_LOG_ERROR, "%s - failed to get public key", PM_DISPLAY_NAME);\
			goto cleanup;
		}

		status = SecIdentityCopyPrivateKey(identity, &private_key);
		if (status != errSecSuccess) {
			openpam_log(PAM_LOG_ERROR, "%s - failed to get private key", PM_DISPLAY_NAME);\
			goto cleanup;
		}

		status = validateCertificate(certificate, keychain);
		if (status == errSecSuccess) {
			openpam_log(PAM_LOG_NOTICE, "%s - Smart card validation passed", PM_DISPLAY_NAME);
		} else {
			openpam_log(PAM_LOG_ERROR, "%s - Smart card validation failed: %d", PM_DISPLAY_NAME, (int)status);
			goto cleanup;
		}

		status = verifySmartCardSigning(pub_key, private_key);
		if (status == errSecSuccess) {
			openpam_log(PAM_LOG_NOTICE, "%s - Smart card can be used for sign and verify", PM_DISPLAY_NAME);
			retval = PAM_SUCCESS;
			goto cleanup;
		} else {
			openpam_log(PAM_LOG_ERROR, "%s - Smart card cannot be used for sign and verify: %d", PM_DISPLAY_NAME, (int)status);
		}
// legacy support block end
	} else if (interactive == FALSE && retval == PAM_SUCCESS) {
		// for authorization set token ID
		CFRetain(token_id);
		pam_set_data(pamh, "token_id", (void *)token_id, cleanup_func);

		// check if we are running in the user session as this means we are called inside screensaver unlock
		if (geteuid() != 0) {
			// unlock user keychain
			char pin_context_buff[PATH_MAX];
			if (copy_smartcard_unlock_context(pin_context_buff, PATH_MAX, token_id, cf_pin, pub_key_hash, pub_key_hash_wrap)) {
				status = SecKeychainLogin((UInt32)strlen(user), user, (UInt32)strlen(pin_context_buff), pin_context_buff);
				openpam_log(PAM_LOG_DEBUG, "%s - Keychain unlock result %d", PM_DISPLAY_NAME, (int)status);
			} else {
				openpam_log(PAM_LOG_DEBUG, "%s - Unable to get data for keychain unlock", PM_DISPLAY_NAME);
			}
		}
	}

cleanup:
	CFReleaseSafe(od_record);
	CFReleaseSafe(token_id);
	CFReleaseSafe(pub_key_hash);
	CFReleaseSafe(pub_key_hash_wrap);
	CFReleaseSafe(cf_user);
    CFReleaseSafe(cf_pin);
// legacy support block start
	CFReleaseSafe(keychain);
	CFReleaseSafe(identity);
	CFReleaseSafe(certificate);
	CFReleaseSafe(pub_key);
	CFReleaseSafe(private_key);
// legacy support block end
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
