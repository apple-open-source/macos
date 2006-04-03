/*
 * KLChangePassword.c
 *
 * $Header$
 *
 * Copyright 2003 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#define kKerberos5ChangePasswordServiceFormat "kadmin/changepw@%s"

KLStatus __KLChangePasswordWithPasswordsCompat (KLPrincipal inPrincipal,
                                                const char *inOldPassword,
                                                const char *inNewPassword)
{
    KLStatus 	err = klNoErr;
    KLBoolean 	rejected;

    if (err == klNoErr) {
        err = KLChangePasswordWithPasswords (inPrincipal, inOldPassword, inNewPassword, &rejected, NULL, NULL);
        if ((err == klNoErr) && rejected) { err = KLError_ (klPasswordChangeFailedErr); }
    }

    return KLError_ (err);
}

KLStatus KLChangePasswordWithPasswords (KLPrincipal   inPrincipal,
                                        const char   *inOldPassword,
                                        const char   *inNewPassword,
                                        KLBoolean    *outRejected,
                                        char        **outRejectionError,
                                        char        **outRejectionDescription)
{
    KLStatus err = klNoErr;

    if (err == klNoErr) {
        if (inPrincipal   == NULL) { err = KLError_ (klBadPrincipalErr); }
        if (inOldPassword == NULL) { err = KLError_ (klBadPasswordErr); }
        if (inNewPassword == NULL) { err = KLError_ (klBadPasswordErr); }
        if (outRejected   == NULL) { err = KLError_ (klParameterErr); }
    }

    if (err == klNoErr) {
        if ((strlen (inNewPassword) <= 0) || (strlen (inOldPassword) <= 0)) {
            err = KLError_ (klBadPasswordErr);
        }
    }

    if (__KLPrincipalShouldUseKerberos5ChangePasswordProtocol (inPrincipal)) {
        krb5_context context = NULL;
        krb5_creds   creds;
        KLBoolean    gotV5Creds = false;
        int          result_code;
        krb5_data    result_code_string, result_string;
        
        if (err == klNoErr) {
            err = krb5_init_context (&context);
        }

        if (err == klNoErr) {
            err = __KLGetKerberos5ChangePasswordTicketForPrincipal (inPrincipal, inOldPassword, context, &creds);
            if (err == klNoErr) { gotV5Creds = true; }
        }
        
        if (err == klNoErr) {
            err = krb5_change_password (context, &creds, (char *)inNewPassword, &result_code, &result_code_string, &result_string);
            dprintf ("krb5_change_password() returned %d '%s'\n", err, error_message (err));
        }

        if (err == klNoErr) {
            if (result_code) {
                char *result_code_cstring = NULL;
                char *result_cstring = NULL;

                if (err == klNoErr) {
                    if ((result_code_string.data != NULL) && (result_code_string.length > 0)) {
                        err = __KLCreateStringFromBuffer (result_code_string.data, result_code_string.length,
                                                          &result_code_cstring);
                    } else {
                        err = __KLGetLocalizedString ("KLStringChangePasswordFailed", &result_code_cstring);
                    }
                }
                
                if (err == klNoErr) {
                    if ((result_string.data != NULL) && (result_string.length > 0)) {
                        err = __KLCreateStringFromBuffer (result_string.data, result_string.length,
                                                          &result_cstring);
                    } else {
                        err = __KLGetLocalizedString ("KLStringPasswordRejected", &result_cstring);
                    }
                }

                if (err == klNoErr) {
                    char *c;
                    
                    // replace all \n and \r characters with spaces
                    for (c = result_code_cstring; *c != '\0'; c++) {
                        if ((*c == '\n') || (*c == '\r')) { *c = ' '; }
                    }
                    
                    for (c = result_cstring; *c != '\0'; c++) {
                        if ((*c == '\n') || (*c == '\r')) { *c = ' '; }
                    }
                }

                if (err == klNoErr) {
                    *outRejected = true;

                    if (outRejectionError != NULL) {
                        *outRejectionError = result_code_cstring;
                        result_code_cstring = NULL;
                    }
                        
                    if (outRejectionDescription != NULL) {
                        *outRejectionDescription = result_cstring;
                        result_cstring = NULL;
                    }
                }

                if (result_code_cstring != NULL) { KLDisposeString (result_code_cstring); }
                if (result_cstring      != NULL) { KLDisposeString (result_cstring); }

                krb5_free_data_contents (context, &result_code_string);
                krb5_free_data_contents (context, &result_string);
            } else {
                *outRejected = false;
            }
        }
        
        if (gotV5Creds     ) { krb5_free_cred_contents (context, &creds); }
        if (context != NULL) { krb5_free_context (context); }
        
    } else if (__KLPrincipalShouldUseKerberos4ChangePasswordProtocol (inPrincipal)) {
        char *name = NULL;
        char *instance = NULL;
        char *realm = NULL;
        
        if (err == klNoErr) {
            err = __KLGetTripletFromPrincipal (inPrincipal, kerberosVersion_V4, &name, &instance, &realm);
        }

        if (err == klNoErr) {
            err = krb_change_password (name, instance, realm, (char *)inOldPassword, (char *)inNewPassword);
            dprintf ("krb_change_password (%s, %s, %s) returned %d '%s'\n", name, instance, realm, err, error_message (err));
            err = __KLRemapKerberos4Error (err);
        }

        if (name     != NULL) { KLDisposeString (name); }
        if (instance != NULL) { KLDisposeString (instance); }
        if (realm    != NULL) { KLDisposeString (realm); }
        
    } else {
        err = KLError_ (klRealmDoesNotExistErr);
    }
    
    return KLError_ (err);
}

#pragma mark -

KLStatus __KLGetKerberos5ChangePasswordTicketForPrincipal (KLPrincipal   inPrincipal,
                                                           const char   *inPassword,
                                                           krb5_context  inContext,
                                                           krb5_creds   *outCreds)
{
    KLStatus err = klNoErr;
    char *realm = NULL;
    char *service = NULL;
    
    if (err == klNoErr) {
        if (inPrincipal == NULL) { err = KLError_ (klBadPrincipalErr); }
        if (inPassword  == NULL) { err = KLError_ (klBadPasswordErr); }
        if (outCreds    == NULL) { err = KLError_ (klBadPasswordErr); }
    }

    if (err == klNoErr) {
        if (strlen (inPassword) <= 0) {
            err = KLError_ (klBadPasswordErr);
        }
    }

    if (err == klNoErr) {
        if (!__KLPrincipalShouldUseKerberos5ChangePasswordProtocol (inPrincipal)) {
            err = KLError_ (klBadPrincipalErr);
        }
    }

    if (err == klNoErr) {
        err = __KLGetRealmFromPrincipal (inPrincipal, kerberosVersion_V5, &realm);
    }

    if (err == klNoErr) {
        service = (char *) calloc (strlen (kKerberos5ChangePasswordServiceFormat) + strlen (realm) + 1,
                                    sizeof (char));
        if (service == NULL) {
            err = KLError_ (klMemFullErr);
        } else {
            sprintf (service, kKerberos5ChangePasswordServiceFormat, realm);
        }
    }

    if (err == klNoErr) {
        krb5_get_init_creds_opt	opts;

        krb5_get_init_creds_opt_init (&opts);
        krb5_get_init_creds_opt_set_tkt_life (&opts, 5*60);
        krb5_get_init_creds_opt_set_renew_life (&opts, 0);
        krb5_get_init_creds_opt_set_forwardable (&opts, 0);
        krb5_get_init_creds_opt_set_proxiable (&opts, 0);

        err = krb5_get_init_creds_password (inContext, outCreds,
                                            __KLGetKerberos5PrincipalFromPrincipal (inPrincipal),
                                            (char *)inPassword, __KLPrompter, NULL, 0,
                                            service, &opts);
    }

    if (realm   != NULL) { KLDisposeString (realm); }
    if (service != NULL) { free (service); }
    
    return KLError_ (err);
}

