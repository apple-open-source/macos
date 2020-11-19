/*
* Copyright (c) 2020 Apple Inc. All rights reserved.
*
* @APPLE_LICENSE_HEADER_START@
*
* This file contains Original Code and/or Modifications of Original Code
* as defined in and that are subject to the Apple Public Source License
* Version 2.0 (the 'License'). You may not use this file except in
* compliance with the License. Please obtain a copy of the License at
* http://www.opensource.apple.com/apsl/ and read it before using this
* file.
*
* The Original Code and all software distributed under the License are
* distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
* EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
* INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
* Please see the License for the specific language governing rights and
* limitations under the License.
*
* @APPLE_LICENSE_HEADER_END@
*/

#include <TargetConditionals.h>
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SCValidation.h>
#include <Security/SecTrustPriv.h>
#include <CoreFoundation/CFPreferences.h>
#include <notify.h>
#include "EAPLog.h"
#include "symbol_scope.h"
#include "myCFUtil.h"
#include "EAPTLSUtil.h"
#include "EAPClientProperties.h"
#include "EAPSIMAKAPersistentState.h"

#define kEAPTLSTrustExceptionsID 		"com.apple.network.eapclient.tls.TrustExceptions"
#define kEAPTLSTrustExceptionsApplicationID	CFSTR(kEAPTLSTrustExceptionsID)

STATIC void
exceptions_change_check(void)
{
    STATIC int		token;
    STATIC bool		token_valid = false;
    int			check = 0;
    uint32_t		status;

    if (!token_valid) {
	status = notify_register_check(kEAPTLSTrustExceptionsID, &token);
	if (status != NOTIFY_STATUS_OK) {
	    EAPLOG_FL(LOG_NOTICE, "notify_register_check returned %d", status);
	    return;
	}
	token_valid = true;
    }
    status = notify_check(token, &check);
    if (status != NOTIFY_STATUS_OK) {
	EAPLOG_FL(LOG_NOTICE, "notify_check returned %d", status);
	return;
    }
    if (check != 0) {
	CFPreferencesSynchronize(kEAPTLSTrustExceptionsApplicationID,
				 kCFPreferencesCurrentUser,
				 kCFPreferencesAnyHost);
    }
    return;
}

STATIC void
exceptions_change_notify(void)
{
    uint32_t	status;

    status = notify_post(kEAPTLSTrustExceptionsID);
    if (status != NOTIFY_STATUS_OK) {
	EAPLOG_FL(LOG_NOTICE, "notify_post returned %d", status);
    }
    return;
}

STATIC void
EAPTLSTrustExceptionsSave(CFStringRef domain, CFStringRef identifier,
			  CFStringRef hash_str, CFDataRef exceptions)
{
    CFDictionaryRef	domain_list;
    CFDictionaryRef	exceptions_list = NULL;
    bool		store_exceptions = TRUE;

    exceptions_change_check();
    domain_list = CFPreferencesCopyValue(domain,
					 kEAPTLSTrustExceptionsApplicationID,
					 kCFPreferencesCurrentUser,
					 kCFPreferencesAnyHost);
    if (domain_list != NULL && isA_CFDictionary(domain_list) == NULL) {
	CFRelease(domain_list);
	domain_list = NULL;
    }
    if (domain_list != NULL) {
	exceptions_list = CFDictionaryGetValue(domain_list, identifier);
	exceptions_list = isA_CFDictionary(exceptions_list);
	if (exceptions_list != NULL) {
	    CFDataRef	stored_exceptions;

	    stored_exceptions = CFDictionaryGetValue(exceptions_list, hash_str);
	    if (isA_CFData(stored_exceptions) != NULL
		&& CFEqual(stored_exceptions, exceptions)) {
		/* stored exceptions are correct, don't store them again */
		store_exceptions = FALSE;
	    }
	}
    }
    if (store_exceptions) {
	if (exceptions_list == NULL) {
	    /* no exceptions for this identifier yet, create one */
	    exceptions_list
		= CFDictionaryCreate(NULL,
				     (const void * * )&hash_str,
				     (const void * *)&exceptions,
				     1,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
	}
	else {
	    /* update existing exceptions with this one */
	    CFMutableDictionaryRef	new_exceptions_list;

	    new_exceptions_list
		= CFDictionaryCreateMutableCopy(NULL, 0,
						exceptions_list);
	    CFDictionarySetValue(new_exceptions_list, hash_str, exceptions);
	    /* don't CFRelease(exceptions_list), it's from domain_list */
	    exceptions_list = (CFDictionaryRef)new_exceptions_list;

	}
	if (domain_list == NULL) {
	    domain_list
		= CFDictionaryCreate(NULL,
				     (const void * *)&identifier,
				     (const void * *)&exceptions_list,
				     1,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
	}
	else {
	    CFMutableDictionaryRef	new_domain_list;

	    new_domain_list
		= CFDictionaryCreateMutableCopy(NULL, 0,
						domain_list);
	    CFDictionarySetValue(new_domain_list, identifier, exceptions_list);
	    CFRelease(domain_list);
	    domain_list = (CFDictionaryRef)new_domain_list;

	}
	CFRelease(exceptions_list);
	CFPreferencesSetValue(domain, domain_list,
			      kEAPTLSTrustExceptionsApplicationID,
			      kCFPreferencesCurrentUser,
			      kCFPreferencesAnyHost);
	CFPreferencesSynchronize(kEAPTLSTrustExceptionsApplicationID,
				 kCFPreferencesCurrentUser,
				 kCFPreferencesAnyHost);
	exceptions_change_notify();
    }
    my_CFRelease(&domain_list);
    return;
}

bool
EAPTLSSecTrustSaveExceptionsBinding(SecTrustRef trust,
				    CFStringRef domain, CFStringRef identifier,
				    CFStringRef server_hash_str)
{
    CFDataRef 	exceptions;

    exceptions = SecTrustCopyExceptions(trust);
    if (exceptions == NULL) {
	EAPLOG_FL(LOG_NOTICE, "failed to copy exceptions");
	return (FALSE);
    }
    EAPTLSTrustExceptionsSave(domain, identifier, server_hash_str,
			      exceptions);
    my_CFRelease(&exceptions);
    return (TRUE);
}

CFDataRef
EAPTLSTrustExceptionsCopy(CFStringRef domain, CFStringRef identifier,
			  CFStringRef hash_str)
{
    CFDataRef		exceptions = NULL;
    CFDictionaryRef	domain_list;

    exceptions_change_check();
    domain_list = CFPreferencesCopyValue(domain,
					 kEAPTLSTrustExceptionsApplicationID,
					 kCFPreferencesCurrentUser,
					 kCFPreferencesAnyHost);
    if (isA_CFDictionary(domain_list) != NULL) {
	CFDictionaryRef		exceptions_list;

	exceptions_list = CFDictionaryGetValue(domain_list, identifier);
	if (isA_CFDictionary(exceptions_list) != NULL) {
	    exceptions = isA_CFData(CFDictionaryGetValue(exceptions_list,
							 hash_str));
	    if (exceptions != NULL) {
		CFRetain(exceptions);
	    }
	}
    }
    my_CFRelease(&domain_list);
    return (exceptions);
}

bool
EAPTLSSecTrustApplyExceptionsBinding(SecTrustRef trust, CFStringRef domain,
				     CFStringRef identifier,
				     CFStringRef server_cert_hash)
{
    CFDataRef		exceptions;
    bool 		ret = false;

    exceptions = EAPTLSTrustExceptionsCopy(domain, identifier,
					   server_cert_hash);
    if (exceptions != NULL) {
	if (SecTrustSetExceptions(trust, exceptions) == FALSE) {
	    EAPLOG_FL(LOG_NOTICE, "SecTrustSetExceptions failed");
	} else {
	    ret = true;
	}
    }
    my_CFRelease(&exceptions);
    return ret;
}

void
EAPTLSRemoveTrustExceptionsBindings(CFStringRef domain, CFStringRef identifier)
{
    CFDictionaryRef	domain_list;
    CFDictionaryRef	exceptions_list;

#if TARGET_OS_IPHONE
    /*
     * Remove the saved EAP-SIM/EAP-AKA information as well.
     * XXX: this call should be moved into a common "EAP cleanup" function.
     */
    if (my_CFEqual(domain, kEAPTLSTrustExceptionsDomainWirelessSSID)) {
	EAPSIMAKAPersistentStateForgetSSID(identifier);
    }
#endif /* TARGET_OS_IPHONE */

    exceptions_change_check();
    domain_list = CFPreferencesCopyValue(domain,
					 kEAPTLSTrustExceptionsApplicationID,
					 kCFPreferencesCurrentUser,
					 kCFPreferencesAnyHost);
    if (domain_list != NULL && isA_CFDictionary(domain_list) == NULL) {
	CFRelease(domain_list);
	domain_list = NULL;
    }
    if (domain_list == NULL) {
	return;
    }
    exceptions_list = CFDictionaryGetValue(domain_list, identifier);
    if (exceptions_list != NULL) {
	CFMutableDictionaryRef	new_domain_list;

	new_domain_list
	    = CFDictionaryCreateMutableCopy(NULL, 0,
					    domain_list);
	CFDictionaryRemoveValue(new_domain_list, identifier);
	CFPreferencesSetValue(domain, new_domain_list,
			      kEAPTLSTrustExceptionsApplicationID,
			      kCFPreferencesCurrentUser,
			      kCFPreferencesAnyHost);
	CFPreferencesSynchronize(kEAPTLSTrustExceptionsApplicationID,
				 kCFPreferencesCurrentUser,
				 kCFPreferencesAnyHost);
	exceptions_change_notify();
	CFRelease(new_domain_list);
    }
    CFRelease(domain_list);
    return;
}

#if TARGET_OS_IPHONE
#define PREFERENCES_USERNAME CFSTR("mobile")
#endif /* TARGET_OS_IPHONE */

/* WiFiManager uses this (on iOS) to share the exception with HomePod */
CFDictionaryRef
EAPTLSCopyTrustExceptionBindings(CFStringRef domain, CFStringRef identifier)
{
    CFDictionaryRef 	domain_list = NULL;
    CFDictionaryRef 	exceptions_list = NULL;
    CFStringRef 	user_name = NULL;

#if TARGET_OS_IPHONE
    user_name = PREFERENCES_USERNAME;
#else /* TARGET_OS_IPHONE */
    user_name = kCFPreferencesCurrentUser;
#endif /* TARGET_OS_IPHONE */
    exceptions_change_check();
    domain_list = CFPreferencesCopyValue(domain,
					 kEAPTLSTrustExceptionsApplicationID,
					 user_name,
					 kCFPreferencesAnyHost);
    if (isA_CFDictionary(domain_list) != NULL) {
	exceptions_list = CFDictionaryGetValue(domain_list, identifier);
	if (isA_CFDictionary(exceptions_list) != NULL) {
	    CFRetain(exceptions_list);
	}
    }
    my_CFRelease(&domain_list);
    return (exceptions_list);
}

/* WiFiManager uses this to set it on HomePod */
void
EAPTLSSetTrustExceptionBindings(CFStringRef domain, CFStringRef identifier, CFDictionaryRef exceptionList)
{
    CFDictionaryRef 	domain_list = NULL;
    CFStringRef 	user_name = NULL;

#if TARGET_OS_IPHONE
    user_name = PREFERENCES_USERNAME;
#else /* TARGET_OS_IPHONE */
    user_name = kCFPreferencesCurrentUser;
#endif /* TARGET_OS_IPHONE */

    exceptions_change_check();
    domain_list = CFPreferencesCopyValue(domain,
					 kEAPTLSTrustExceptionsApplicationID,
					 user_name,
					 kCFPreferencesAnyHost);
    if (domain_list != NULL && isA_CFDictionary(domain_list) == NULL) {
	my_CFRelease(&domain_list);
    }
    if (domain_list == NULL) {
	domain_list = CFDictionaryCreate(NULL,
					 (const void * *)&identifier,
					 (const void * *)&exceptionList,
					 1,
					 &kCFTypeDictionaryKeyCallBacks,
					 &kCFTypeDictionaryValueCallBacks);
    }
    else {
	CFMutableDictionaryRef	new_domain_list;

	new_domain_list = CFDictionaryCreateMutableCopy(NULL, 0, domain_list);
	CFDictionarySetValue(new_domain_list, identifier, exceptionList);
	CFRelease(domain_list);
	domain_list = (CFDictionaryRef)new_domain_list;
    }
    CFPreferencesSetValue(domain, domain_list,
			  kEAPTLSTrustExceptionsApplicationID,
			  user_name,
			  kCFPreferencesAnyHost);
    CFPreferencesSynchronize(kEAPTLSTrustExceptionsApplicationID,
			     user_name,
			     kCFPreferencesAnyHost);
    exceptions_change_notify();
    my_CFRelease(&domain_list);
    return;
}
