/*
 * KLUserInterfaceAPI.c
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

// ---------------------------------------------------------------------------

KLStatus KLAcquireNewInitialTickets (
    KLPrincipal       inPrincipal, 
    KLLoginOptions    inLoginOptions,
    KLPrincipal      *outPrincipal, 
    char            **outCredCacheName)
{
    KLStatus  lockErr = __KLLockCCache (kWriteLock);
    KLStatus  err = lockErr;

    if (err == klNoErr) {
        switch (__KLPromptMechanism ()) {
            case klPromptMechanism_GUI:
                err = __KLAcquireNewInitialTicketsGUI (inPrincipal, inLoginOptions, outPrincipal, outCredCacheName);
                break;
                
            case klPromptMechanism_CLI:
                err = __KLAcquireNewInitialTicketsTerminal (inPrincipal, inLoginOptions, outPrincipal, outCredCacheName);
                break;
    
            default:
                err = KLError_ (klCantDisplayUIErr);
                break;
        }
    }
    
    if (lockErr == klNoErr) { __KLUnlockCCache (); }
    return KLError_ (err);
}


// ---------------------------------------------------------------------------

KLStatus KLHandleError (
    KLStatus            inError,
    KLDialogIdentifier  inDialogIdentifier,
    KLBoolean           inShowAlert)
{
    KLStatus  err = klNoErr;

    switch (__KLPromptMechanism ()) {
        case klPromptMechanism_GUI:
            err = __KLHandleErrorGUI (inError, inDialogIdentifier, inShowAlert);
            break;
            
        case klPromptMechanism_CLI:
            err = __KLHandleErrorTerminal (inError, inDialogIdentifier, inShowAlert);
            break;

        default:
            err = KLError_ (klCantDisplayUIErr);
            break;
    }

    return KLError_ (err);
}


// ---------------------------------------------------------------------------

KLStatus KLChangePassword (KLPrincipal inPrincipal)
{
    KLStatus  err = klNoErr;

    switch (__KLPromptMechanism ()) {
        case klPromptMechanism_GUI:
            err = __KLChangePasswordGUI (inPrincipal);
            break;
            
        case klPromptMechanism_CLI:
            err = __KLChangePasswordTerminal (inPrincipal, NULL);
            break;

        default:
            err = KLError_ (klCantDisplayUIErr);
            break;
    }

    return KLError_ (err);
}


// ---------------------------------------------------------------------------

KLStatus KLCancelAllDialogs(void)
{
    KLStatus  err = klNoErr;

    switch (__KLPromptMechanism ()) {
        case klPromptMechanism_GUI:
            err = __KLCancelAllDialogsGUI ();
            break;
            
        case klPromptMechanism_CLI:
            err = __KLCancelAllDialogsTerminal ();
            break;

        default:
            err = KLError_ (klCantDisplayUIErr);
            break;
    }

    return KLError_ (err);
}


// ---------------------------------------------------------------------------

krb5_error_code __KLPrompter (krb5_context   context,
                              void          *data,
                              const char    *name,
                              const char    *banner,
                              int            num_prompts,
                              krb5_prompt    prompts[])
{
    KLStatus  err = klNoErr;

    if (__KLApplicationProvidedPrompter ()) {
        err = __KLCallApplicationPrompter (context, data, name, banner, num_prompts, prompts);
    } else {
        switch (__KLPromptMechanism ()) {
            case klPromptMechanism_GUI:
                err = __KLPrompterGUI (context, data, name, banner, num_prompts, prompts);
                break;

            case klPromptMechanism_CLI:
                err = __KLPrompterTerminal (context, data, name, banner, num_prompts, prompts);
                break;

            default:
                err = KLError_ (klCantDisplayUIErr);
                break;
        }
    }
    
    // Use the krb5 prompter error for user cancelled
    err = KLError_ (err);
    return (err == klUserCanceledErr) ? KRB5_LIBOS_PWDINTR : err;
}
