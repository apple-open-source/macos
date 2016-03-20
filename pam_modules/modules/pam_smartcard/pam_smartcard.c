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

#define PAM_SM_ACCOUNT
#define PAM_SM_AUTH
#define PAM_SM_PASSWORD
#define PAM_SM_SESSION

#define PM_DISPLAY_NAME "SmartCard"
#define MAX_PIN_RETRY (3)

/* Include PAM headers */
#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <OpenDirectory/OpenDirectory.h>
#include <Security/Security.h>
#include <sys/param.h>
#include "scmatch_evaluation.h"
#include "Common.h"

int pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
    int retval;
    const char* user = NULL;
    SecKeychainStatus keychainStatus;
    OSStatus status;
    UInt32 pathLength = MAXPATHLEN;
    char pathName[MAXPATHLEN + 1];
    ODRecordRef odRecord = NULL;
    SecKeychainRef keychain = NULL;
    SecIdentityRef identity = NULL;
    SecKeyRef pubKey = NULL;
    SecKeyRef privateKey = NULL;
    SecCertificateRef certificate = NULL;

    retval = pam_get_user(pamh, &user, "Username: ");
    if (retval != PAM_SUCCESS)
    {
        openpam_log(PAM_LOG_ERROR, "Unable to get the username: %s", pam_strerror(pamh, retval));
        return retval;
    }
    
    if (user == NULL || *user == '\0') {
        openpam_log(PAM_LOG_ERROR, "Username is invalid.");
        retval = PAM_PERM_DENIED;
        return retval;
    }
    
    /* Get user record from OD */
    retval = od_record_create_cstring(pamh, &odRecord, (const char*)user);
    if (retval != PAM_SUCCESS)
    {
        openpam_log(PAM_LOG_ERROR, "%s - Unable to get user record %d.", PM_DISPLAY_NAME, retval);
        return PAM_PERM_DENIED;
    }
    
    /* check user authentication authority */
    retval = od_record_check_authauthority(odRecord);
    if (PAM_SUCCESS != retval)
    {
        openpam_log(PAM_LOG_ERROR, "This user is disabled.");
        retval = PAM_PERM_DENIED;
        goto cleanup;
    }
    
    /* check user password policy */
    retval = od_record_check_pwpolicy(odRecord);
    if (PAM_SUCCESS != retval) {
        openpam_log(PAM_LOG_ERROR, "This user is disabled.");
        goto cleanup;
    }
    
    /* check user shell */
    if (!openpam_get_option(pamh, "no_check_shell"))
    {
        retval = od_record_check_shell(odRecord);
        if (PAM_SUCCESS != retval)
        {
            openpam_log(PAM_LOG_ERROR, "This user has no valid shell.");
            retval = PAM_PERM_DENIED;
            goto cleanup;
        }
    }
    
    keychain = copySmartCardKeychainForUser(odRecord, user, &identity);
    if (keychain == NULL)
    {
        openpam_log(PAM_LOG_DEBUG, "User has no smartcard support in OD.");
        retval = PAM_IGNORE;
        goto cleanup;
    }
    
    retval =  PAM_IGNORE;
    
    // first try to check if this keychain is already unlocked
    bool checkCard = false;
    status = SecKeychainGetStatus(keychain, &keychainStatus);
    if (status == errSecSuccess && (keychainStatus & kSecUnlockStateStatus))
    {
        checkCard = true;
    }
    else
    {
        status = SecKeychainGetPath(keychain, &pathLength, pathName);
        if (status)
        {
            openpam_log(PAM_LOG_ERROR, "Unable to get smart card name: %d", (int)status);
            goto cleanup;
        }
        
        // ask user for PIN and try to unlock keychain
        char prompt[PATH_MAX + 24];
        snprintf(prompt, PATH_MAX + 24, "Enter PIN for '%s': ", pathName);
        
        for(int try = 0; try < MAX_PIN_RETRY; ++try)
        {
            const char* pass;
            int ret = pam_get_authtok(pamh, PAM_AUTHTOK, &pass, prompt);
            if (ret != PAM_SUCCESS)
            {
                openpam_log(PAM_LOG_ERROR, "Unable to get PIN: %d", ret);
                retval = ret;
                break;
            }
            status = SecKeychainUnlock(keychain, (UInt32)strlen(pass), pass, 1);
            if (status == errSecSuccess)
            {
                checkCard = true;
                break;
            }
            else
            {
                openpam_log(PAM_LOG_ERROR, "Unlock with PIN failed: %d", (int)status);
            }
        }
    }
    if (checkCard && identity)
    {
        status = SecIdentityCopyCertificate(identity, &certificate);
        if (status != errSecSuccess)
            goto cleanup;
        
        status = SecCertificateCopyPublicKey(certificate, &pubKey);
        if (status != errSecSuccess)
            goto cleanup;
        
        status = SecIdentityCopyPrivateKey(identity, &privateKey);
        if (status != errSecSuccess)
            goto cleanup;

        status = validateCertificate(certificate, keychain);
        if (status == errSecSuccess)
        {
            openpam_log(PAM_LOG_NOTICE, "Smart card validation passed");
        }
        else
        {
            openpam_log(PAM_LOG_ERROR, "Smart card validation failed: %d", (int)status);
            goto cleanup;
        }

        status = verifySmartCardSigning(pubKey, privateKey);
        if (status == errSecSuccess)
        {
            openpam_log(PAM_LOG_NOTICE, "Smart card can be used for sign and verify");
            retval = PAM_SUCCESS;
        }
        else
        {
            openpam_log(PAM_LOG_ERROR, "Smart card cannot be used for sign and verify: %d", (int)status);
        }
    }

cleanup:
    CFReleaseSafe(pubKey);
    CFReleaseSafe(privateKey);
    CFReleaseSafe(certificate);
    CFRelease(odRecord);
    CFReleaseSafe(keychain);
    return retval;
}

int pam_sm_setcred(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
    return PAM_IGNORE;
}

int pam_sm_chauthtok(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
    return PAM_AUTHTOK_ERR;
}