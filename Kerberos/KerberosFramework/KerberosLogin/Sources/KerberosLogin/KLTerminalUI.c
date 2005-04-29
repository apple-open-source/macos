/*
 * KLTerminalUI.c
 *
 * $Header: /cvs/kfm/KerberosFramework/KerberosLogin/Sources/KerberosLogin/KLTerminalUI.c,v 1.11 2004/12/17 05:03:21 lxs Exp $
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

#include <pkinit_cert_store.h>

typedef enum {
    CS_Initial,		    // first time through the loop, haven't checked yet
    CS_NoCert,		    // no cert
    CS_TryingNoPassword,    // have a cert, trying with no password
    CS_GaveUp		    // have a cert but it didn't work
} _KLCertState;

KLStatus __KLReadStringFromTerminal (char **outString, KLBoolean inHidden, const char *inFormat, ...);

// ---------------------------------------------------------------------------

KLStatus __KLAcquireNewInitialTicketsTerminal (KLPrincipal 	  inPrincipal,
                                               KLLoginOptions	  inLoginOptions,
                                               KLPrincipal 	 *outPrincipal,
                                               char 		**outCredCacheName)
{
    KLStatus     err = klNoErr;
    char        *enterPrincipalString = NULL;
    char        *enterPasswordFormat = NULL;
    char        *yesString = NULL;
    char        *noString = NULL;
    char        *passwordExpiredString = NULL;
    char        *yesOrNoAnswerOptionsString = NULL;
    char        *unknownResponseFormat = NULL;
    char        *displayPrincipalString = NULL;
    char        *principalString = NULL;
    KLPrincipal  principal = NULL;
    char        *ccacheName = NULL;
    char        *passwordString = NULL;
    _KLCertState certState = CS_Initial;
    
    // Gather localized display strings
    if (err == klNoErr) {
        err = __KLGetLocalizedString ("KLStringEnterPrincipal", &enterPrincipalString);
    }

    if (err == klNoErr) {
        err = __KLGetLocalizedString ("KLStringEnterPassword", &enterPasswordFormat);
    }

    if (err == klNoErr) {
        err = __KLGetLocalizedString ("KLStringYes", &yesString);
    }

    if (err == klNoErr) {
        err = __KLGetLocalizedString ("KLStringNo", &noString);
    }

    if (err == klNoErr) {
        err = __KLGetLocalizedString ("KLStringPasswordExpired", &passwordExpiredString);
    }

    if (err == klNoErr) {
        err = __KLGetLocalizedString ("KLStringYesOrNoAnswerOptions", &yesOrNoAnswerOptionsString);
    }

    if (err == klNoErr) {
        err = __KLGetLocalizedString ("KLStringUnknownResponse", &unknownResponseFormat);
    }

    // Get the principal string
    if (inPrincipal == NULL) {
        char *tempPrincipalString = NULL;
        KLPrincipal tempPrincipal = NULL;
        
        if (err == klNoErr) {
            err = __KLReadStringFromTerminal (&tempPrincipalString, false, enterPrincipalString);
        }
        
        if (err == klNoErr) {
            err = KLCreatePrincipalFromString (tempPrincipalString, kerberosVersion_V5, &tempPrincipal);
        }
        
        if (err == klNoErr) {
            err = KLGetDisplayStringFromPrincipal (tempPrincipal, kerberosVersion_V5, &displayPrincipalString);
        }
        
        if (err == klNoErr) {
            err = KLGetStringFromPrincipal (tempPrincipal, kerberosVersion_V5, &principalString);
        }
        
        if (tempPrincipalString != NULL) { KLDisposeString (tempPrincipalString); }
        if (tempPrincipal       != NULL) { KLDisposePrincipal (tempPrincipal); }
    } else {
        if (err == klNoErr) {
            err = KLGetDisplayStringFromPrincipal (inPrincipal, kerberosVersion_V5, &displayPrincipalString);
        }
        
        if (err == klNoErr) {
            err = KLGetStringFromPrincipal (inPrincipal, kerberosVersion_V5, &principalString);
        }
    }
    
    if (err == klNoErr) {
        err = KLCreatePrincipalFromString (principalString, kerberosVersion_V5, &principal);
    }

    if ((err == klNoErr) && (certState == CS_Initial)) {
        /*
         * See if there is a PKINIT cert for this principal; if so, skip the password for now
         */
		pkinit_signing_cert_t client_cert = NULL;
        krb5_error_code krtn = pkinit_get_client_cert(principalString, &client_cert);
        if (krtn) {
            certState = CS_NoCert;
        } else {
            certState = CS_TryingNoPassword;
            passwordString = "";
        }
		if (client_cert != NULL) { pkinit_release_cert(client_cert); }
    }
    
    // Get the password
    if ((err == klNoErr) & (certState != CS_TryingNoPassword)) {
        err = __KLReadStringFromTerminal (&passwordString, true, enterPasswordFormat, displayPrincipalString);
    }
    
    // Get tickets
    if (err == klNoErr) {
        err = KLAcquireNewInitialTicketsWithPassword (principal, inLoginOptions, passwordString, &ccacheName);
        if(certState == CS_TryingNoPassword) {
            /* avoid freeing the empty string we put here */
            passwordString = NULL;
        }         

        if (err == KRB5KDC_ERR_KEY_EXP) {
            do {  // Loop until we succeed, the user cancels the operation or the user gives up trying to change their expired password
                char *userResponseString = NULL;
                
                err = __KLReadStringFromTerminal (&userResponseString, false, "%s %s", passwordExpiredString, yesOrNoAnswerOptionsString);
                
                if (err == klNoErr) {
                    if (strcasecmp (userResponseString, yesString) == 0) {
                        char *newPasswordString = NULL;
                        
                        if (err == klNoErr) {
                            err = __KLChangePasswordTerminal (principal, &newPasswordString);
                        }
                        
                        if (err == klNoErr) {
                            err = KLAcquireNewInitialTicketsWithPassword (principal, inLoginOptions, newPasswordString, &ccacheName);
                        }
                        
                        if (newPasswordString != NULL) { KLDisposeString (newPasswordString); }
                        
                    } else if (strcasecmp (userResponseString, noString) == 0) {
                        err = KLError_ (KRB5KDC_ERR_KEY_EXP); // User doesn't want to change the password.  Restore the error.
                        
                    } else {
                        err = KLError_ (klParameterErr); // Loop and ask again
                        fprintf (stdout, unknownResponseFormat, userResponseString);
                        fprintf (stdout, "\n");
                    }
                }
                
                if (userResponseString != NULL) { KLDisposeString (userResponseString); }
                
            } while ((err != klNoErr) && (err != KRB5KDC_ERR_KEY_EXP) && (err != klUserCanceledErr));
        }
    }

    if (err == klNoErr) {
        if (outPrincipal != NULL) {
            *outPrincipal = principal;
            principal = NULL;
        }
        if (outCredCacheName != NULL) {
            *outCredCacheName = ccacheName;
            ccacheName = NULL;
        }
    } else if ((err != KRB5KDC_ERR_KEY_EXP) && (err != klUserCanceledErr)) {
               if (certState == CS_TryingNoPassword) {
                   /* PKINIT didn't work; we'll retry with a password */
                   certState = CS_GaveUp;
               } else {
                   // An error the user isn't aware of yet.  Report it.
                   err = __KLHandleErrorTerminal (err, loginLibrary_LoginDialog, true);

                   // Simulate a user cancel error.  We do this so callers don't report the error again.
                   // We can't return the error because that's not what the GUI version does.
                   if (err == klNoErr) { err = klUserCanceledErr; }       
               }
    }
    
    if (principalString            != NULL) { KLDisposeString (principalString); }
    if (displayPrincipalString     != NULL) { KLDisposeString (displayPrincipalString); }
    if (passwordString             != NULL) { KLDisposeString (passwordString); }
    if (ccacheName                 != NULL) { KLDisposeString (ccacheName); }
    if (principal                  != NULL) { KLDisposePrincipal (principal); }
    if (yesString                  != NULL) { KLDisposeString (yesString); }
    if (noString                   != NULL) { KLDisposeString (noString); }
    if (passwordExpiredString      != NULL) { KLDisposeString (passwordExpiredString); }
    if (yesOrNoAnswerOptionsString != NULL) { KLDisposeString (yesOrNoAnswerOptionsString); }
    if (unknownResponseFormat      != NULL) { KLDisposeString (unknownResponseFormat); }
    if (enterPrincipalString       != NULL) { KLDisposeString (enterPrincipalString); }
    if (enterPasswordFormat        != NULL) { KLDisposeString (enterPasswordFormat); }

    return KLError_ (err);
}


// ---------------------------------------------------------------------------

KLStatus __KLHandleErrorTerminal (KLStatus inError, KLDialogIdentifier inDialogIdentifier, KLBoolean inShowAlert)
{
    KLStatus err = klNoErr;
    
    if (inShowAlert) {
        char *header = NULL;
        char *details = NULL;
        
        if (err == klNoErr) {
            switch (inDialogIdentifier) {
                case loginLibrary_LoginDialog:
                    err = __KLGetLocalizedString ("KLStringLoginFailed", &header);
                    break;
                    
                case loginLibrary_ChangePasswordDialog:
                    err = __KLGetLocalizedString ("KLStringChangePasswordFailed", &header);
                    break;
                
                case loginLibrary_OptionsDialog:
                    err = __KLGetLocalizedString ("KLStringOptionsChangeFailed", &header);
                    break;
                    
                default:
                    err = __KLGetLocalizedString ("KLStringKerberosOperationFailed", &header);
                    break;
            }
        }

        if (err == klNoErr) {
            err = KLGetErrorString (inError, &details);
        }

        if (err == klNoErr) {
            fprintf (stdout, "%s %s\n", header, details);
        }
        
        if (header  != NULL) { KLDisposeString (header); }
        if (details != NULL) { KLDisposeString (details); }
    }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLChangePasswordTerminal (KLPrincipal inPrincipal, char **outNewPassword)	// Special argument used for expired passwords
{
    KLStatus  err = klNoErr;
    char     *enterOldPasswordFormat = NULL;
    char     *enterNewPasswordFormat = NULL;
    char     *enterVerifyPasswordFormat = NULL;
    char     *passwordChangedFormat = NULL;
    char     *principalDisplayString = NULL;
    
    if (inPrincipal == NULL) { err = KLError_ (klParameterErr); }

    if (err == klNoErr) {
        err = __KLGetLocalizedString ("KLStringEnterOldPassword", &enterOldPasswordFormat);
    }

    if (err == klNoErr) {
        err = __KLGetLocalizedString ("KLStringEnterNewPassword", &enterNewPasswordFormat);
    }

    if (err == klNoErr) {
        err = __KLGetLocalizedString ("KLStringEnterVerifyPassword", &enterVerifyPasswordFormat);
    }

    if (err == klNoErr) {
        err = __KLGetLocalizedString ("KLStringPasswordChanged", &passwordChangedFormat);
    }

    if (err == klNoErr) {
        err = KLGetDisplayStringFromPrincipal (inPrincipal, kerberosVersion_V5, &principalDisplayString);
    }

    do {
        char *oldPassword = NULL;
        char *newPassword = NULL;
        char *verifyPassword = NULL;
        char *rejectionError = NULL;
        char *rejectionDescription = NULL;
        KLBoolean rejected;
        
        if (err == klNoErr) {
            err = __KLReadStringFromTerminal (&oldPassword, true, enterOldPasswordFormat, principalDisplayString);
        }

        if (err == klNoErr) {
            // Make sure this is a valid password
            if (__KLPrincipalShouldUseKerberos5ChangePasswordProtocol (inPrincipal)) {
                krb5_context context = NULL;
                krb5_creds creds;

                if (err == klNoErr) {
                    err = krb5_init_context (&context);
                }

                if (err == klNoErr) {
                    err = __KLGetKerberos5ChangePasswordTicketForPrincipal (inPrincipal, oldPassword, context, &creds);
                    if (err == klNoErr) { krb5_free_cred_contents (context, &creds); }
                }
                    
                if (context != NULL) { krb5_free_context (context); }
                
            } else if (__KLPrincipalShouldUseKerberos4ChangePasswordProtocol (inPrincipal)) {
                CREDENTIALS  creds;
                char        *name = NULL;
                char        *instance = NULL;
                char        *realm = NULL;

                if (err == klNoErr) {
                    err = __KLGetTripletFromPrincipal (inPrincipal, kerberosVersion_V4,
                                                       &name, &instance, &realm);
                }

                if (err == klNoErr) {
                    err = krb_get_pw_in_tkt_creds (name, instance, realm, "changepw", "kerberos", 1, oldPassword, &creds);
                    err = __KLRemapKerberos4Error (err);
                }

                if (name     != NULL) { KLDisposeString (name); }
                if (instance != NULL) { KLDisposeString (instance); }
                if (realm    != NULL) { KLDisposeString (realm); }
            } else {
                err = KLError_ (klRealmDoesNotExistErr);
            }
        }
        
        if (err == klNoErr) {
            err = __KLReadStringFromTerminal (&newPassword, true, enterNewPasswordFormat, principalDisplayString);
        }

        if (err == klNoErr) {
            err = __KLReadStringFromTerminal (&verifyPassword, true, enterVerifyPasswordFormat, principalDisplayString);
        }

        if (err == klNoErr) {
            if (strcmp (newPassword, verifyPassword) != 0) {
                err = KLError_ (klPasswordMismatchErr);
            }
        }

        if (err == klNoErr) {
            err = KLChangePasswordWithPasswords (inPrincipal, oldPassword, newPassword,
                                                 &rejected, &rejectionError, &rejectionDescription);
        }

        if (err == klNoErr) {
            if (rejected) {
                fprintf (stdout, "%s:\n%s\n\n", rejectionError, rejectionDescription);
                err = KLError_ (klPasswordChangeFailedErr);
            } else {
                fprintf (stdout, passwordChangedFormat, principalDisplayString);
                fprintf (stdout, "\n");
                
                if (outNewPassword != NULL) {
                    *outNewPassword = newPassword;
                    newPassword = NULL;
                }
            }
        } else if (err != klUserCanceledErr) {
            // An error the user isn't aware of yet.  Report it.
            err = __KLHandleErrorTerminal (err, loginLibrary_ChangePasswordDialog, true);
        }

        if (rejectionError       != NULL) { KLDisposeString (rejectionError); }
        if (rejectionDescription != NULL) { KLDisposeString (rejectionDescription); }
        if (oldPassword          != NULL) { KLDisposeString (oldPassword); }
        if (newPassword          != NULL) { KLDisposeString (newPassword); }
        if (verifyPassword       != NULL) { KLDisposeString (verifyPassword); }
        
    } while ((err != klNoErr) && (err != klUserCanceledErr));

    if (principalDisplayString    != NULL) { KLDisposeString (principalDisplayString); }
    if (passwordChangedFormat     != NULL) { KLDisposeString (passwordChangedFormat); }
    if (enterVerifyPasswordFormat != NULL) { KLDisposeString (enterVerifyPasswordFormat); }
    if (enterNewPasswordFormat    != NULL) { KLDisposeString (enterNewPasswordFormat); }
    if (enterOldPasswordFormat    != NULL) { KLDisposeString (enterOldPasswordFormat); }

    return KLError_ (err);
}


// ---------------------------------------------------------------------------

KLStatus __KLCancelAllDialogsTerminal (void)
{
#warning KLCancelAllDialogs not implemented for Terminal
    // You can't do this from a console app right now.
    return KLError_ (klParameterErr);
}

// ---------------------------------------------------------------------------

krb5_error_code __KLPrompterTerminal (
    krb5_context    context,
    void           *data,
    const char     *name,
    const char     *banner,
    int             num_prompts,
    krb5_prompt     prompts[])
{
    // Just call the posix version.  It will do the right thing
    return krb5_prompter_posix (context, data, name, banner, num_prompts, prompts);
}

#pragma mark -

KLStatus __KLReadStringFromTerminal (char **outString, KLBoolean inHidden, const char *inFormat, ...)
{
    KLStatus     err = klNoErr;
    krb5_context context = NULL;
    krb5_prompt  prompts[1];
    char         promptString [BUFSIZ];
    krb5_data    replyData;
    char         replyString [BUFSIZ];
    
    if (inFormat  == NULL) { err = KLError_ (klParameterErr); }
    if (outString == NULL) { err = KLError_ (klParameterErr); }

    if (err == klNoErr) {
        err = krb5_init_context (&context);
    }

    if (err == klNoErr) {
        int     shouldPrint;
        va_list args;
        
        va_start (args, inFormat);
        shouldPrint = vsnprintf (promptString, sizeof (promptString), inFormat, args);
        va_end (args);
        
        if (shouldPrint > sizeof (promptString)) {
            dprintf ("__KLReadStringFromTerminal(): WARNING! Prompt should be %ld characters\n", shouldPrint);
            promptString [sizeof (promptString) - 1] = '\0';
        }
    }

    if (err == klNoErr) {
        // Build the prompt structures
        prompts[0].prompt        = promptString;
        prompts[0].hidden        = inHidden;
        prompts[0].reply         = &replyData;
        prompts[0].reply->data   = replyString;
        prompts[0].reply->length = sizeof (replyString);

        err = krb5_prompter_posix (context, NULL, NULL, NULL, 1, prompts);
        if (err == KRB5_LIBOS_PWDINTR) { err = KLError_ (klUserCanceledErr); }  // User pressed control-c
    }

    if (err == klNoErr) {
        err = __KLCreateStringFromBuffer (prompts[0].reply->data, prompts[0].reply->length, outString);
    }

    if (context != NULL) { krb5_free_context (context); }
    
    return KLError_ (err);
}
