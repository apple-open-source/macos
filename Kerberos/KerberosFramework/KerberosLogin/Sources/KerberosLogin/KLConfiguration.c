/*
 * KLConfiguration.c
 *
 * $Header: /cvs/kfm/KerberosFramework/KerberosLogin/Sources/KerberosLogin/KLConfiguration.c,v 1.11 2003/08/08 21:34:45 lxs Exp $
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

/* Application Configuration functions */

/* Initialize the (per-process) global options */
static KLIdleCallback gIdleCallback = NULL;
static KLRefCon       gIdleRefCon   = 0;

// ---------------------------------------------------------------------------

KLStatus KLSetIdleCallback (const KLIdleCallback inCallback, const KLRefCon inRefCon)
{
    gIdleCallback = inCallback;
    gIdleRefCon = inRefCon;
    
    return klNoErr;
}

// ---------------------------------------------------------------------------

KLStatus KLGetIdleCallback (KLIdleCallback* outCallback, KLRefCon* outRefCon)
{
    *outCallback = gIdleCallback;
    *outRefCon = gIdleRefCon;
    
    return klNoErr;
}

// ---------------------------------------------------------------------------

void __KLCallIdleCallback (void)
{
    if (gIdleCallback != NULL) {
        CallKLIdleCallback (gIdleCallback, gIdleRefCon);
    }
}

#pragma mark -

// ---------------------------------------------------------------------------

KLPrompterProcPtr gApplicationPrompter = NULL;

void __KLSetApplicationPrompter (KLPrompterProcPtr inPrompter)
{
    gApplicationPrompter = inPrompter;
}

// ---------------------------------------------------------------------------

KLBoolean __KLApplicationProvidedPrompter (void)
{
    return (gApplicationPrompter != NULL);
}

// ---------------------------------------------------------------------------

krb5_error_code __KLCallApplicationPrompter (krb5_context   context,
                                             void          *data,
                                             const char    *name,
                                             const char    *banner,
                                             int            num_prompts,
                                             krb5_prompt    prompts[])
{
    if (gApplicationPrompter == NULL) { return KLError_ (klParameterErr); }
    
    return gApplicationPrompter (context, data, name, banner, num_prompts, prompts);
}

#pragma mark -

// ---------------------------------------------------------------------------

KLStatus KLSetApplicationOptions (const KLApplicationOptions *inAppOptions)
{
    if (inAppOptions == NULL) { return KLError_ (klParameterErr); }

    return klNoErr;
}
     
// ---------------------------------------------------------------------------

KLStatus KLGetApplicationOptions (KLApplicationOptions *outAppOptions)
{
    return klNoErr;
}
     
#pragma mark -

// ---------------------------------------------------------------------------

static KLBoolean gKLAllowHomeDirectoryAccess = true;

// ---------------------------------------------------------------------------

KLStatus  __KLSetHomeDirectoryAccess (KLBoolean inAllowHomeDirectoryAccess)
{
    gKLAllowHomeDirectoryAccess = inAllowHomeDirectoryAccess;
    return KLError_ (klNoErr);
}

// ---------------------------------------------------------------------------

KLBoolean __KLAllowHomeDirectoryAccess (void)
{
    return gKLAllowHomeDirectoryAccess;
}

#pragma mark -

// ---------------------------------------------------------------------------

static KLBoolean gKLAllowAutomaticPrompting = true;
static KLBoolean gKLPluginRefCount = 0;

// ---------------------------------------------------------------------------

void __KLBeginPlugin (void)
{
    gKLPluginRefCount++;
}

// ---------------------------------------------------------------------------

void __KLEndPlugin (void)
{
    gKLPluginRefCount--;
}

// ---------------------------------------------------------------------------

KLStatus  __KLSetAutomaticPrompting (KLBoolean inAllowAutomaticPrompting)
{
    gKLAllowAutomaticPrompting = inAllowAutomaticPrompting;
    return KLError_ (klNoErr);
}

// ---------------------------------------------------------------------------

KLBoolean __KLAllowAutomaticPrompting (void)
{
    KLBoolean hasKerberosConfig = false;

    // Don't prompt if environment variable is set.
    if (getenv ("KERBEROSLOGIN_NEVER_PROMPT") != NULL) {
        dprintf ("__KLAllowAutomaticPrompting(): KERBEROSLOGIN_NEVER_PROMPT is set.\n");
        return false;
    }

    // Don't prompt if we are in a plug-in because we will reenter KLL code
    if (gKLPluginRefCount > 0) {
        dprintf ("__KLAllowAutomaticPrompting(): We are in a plug-in!  Don't prompt.\n");
        return false;
    }

    // Currently only prompt for GUI sessions
    if (LoginSessionGetSessionUIType () != kLoginSessionWindowServer) {
        dprintf ("__KLAllowAutomaticPrompting (): session is not GUI type.\n");
        return false;
    }
    
    // Make sure there is at least 1 config file
    // We don't support DNS domain-realm lookup, so if there is no
    // config, Kerberos is guaranteed not to work.
#warning "__KLAllowAutomaticPrompting () doesn't support zero-config"

    if (!hasKerberosConfig) {
        char **files = NULL;
        profile_t profile = NULL;
        
        if (krb5_get_default_config_files (&files) == klNoErr) {
            if (profile_init ((const_profile_filespec_t *) files, &profile) == klNoErr) {
                hasKerberosConfig = true;
            }
        }

        if (profile != NULL) { profile_release (profile); }
        if (files != NULL) { krb5_free_config_files (files); }
    }
    
    // Krb4 traditional config files
    if (!hasKerberosConfig) {
        hasKerberosConfig = (krb__get_cnffile () != NULL);
    }
    
    
    if (hasKerberosConfig) {
        dprintf ("LoginSessionGetSessionUIType (): will allow prompting.\n");
        return gKLAllowAutomaticPrompting;
    } else {
        dprintf ("LoginSessionGetSessionUIType (): no valid config file.\n");
        return false;
    }
}

#pragma mark -

// ---------------------------------------------------------------------------

/* Library configuration functions */

KLStatus KLGetDefaultLoginOption (KLDefaultLoginOption   inOption,
                                  void                  *ioBuffer,
                                  KLSize                *ioBufferSize)
{
    KLStatus  err = klNoErr;
    KLSize    targetSize = 0;
    KLBoolean returnSizeOnly = (ioBuffer == NULL);

    if (ioBufferSize == NULL) { return KLError_ (klParameterErr); }

    switch (inOption) {
        case loginOption_LoginName: {
            char *name = NULL;

            if (err == klNoErr) {
                err = __KLPreferencesGetKerberosLoginName (&name);
            }

            if (err == klNoErr) {
                targetSize = strlen (name);
                if (!returnSizeOnly) {
                    if (*ioBufferSize < targetSize) {
                        err = KLError_ (klBufferTooSmallErr);
                    } else {
                        memmove (ioBuffer, name, targetSize);
                    }
                }
            }
            
            if (err == klNoErr) {
                *ioBufferSize = targetSize;
            }
            
            if (name != NULL) { KLDisposeString (name); }
        } break;

        case loginOption_LoginInstance: {
            char *instance = NULL;
            
            if (err == klNoErr) {
                err = __KLPreferencesGetKerberosLoginInstance (&instance);
            }
            
            if (err == klNoErr) {
                targetSize = strlen (instance);
                if (!returnSizeOnly) {
                    if (*ioBufferSize < targetSize) {
                        err = KLError_ (klBufferTooSmallErr);
                    } else {
                        memmove (ioBuffer, instance, targetSize);
                    }
                }
            }
            
            if (err == klNoErr) {
                *ioBufferSize = targetSize;
            }
            
            if (instance != NULL) { KLDisposeString (instance); }
        } break;

        case loginOption_ShowOptions: {
            targetSize = sizeof(KLBoolean);

            if (err == klNoErr) {
                if (!returnSizeOnly) {
                    if (*ioBufferSize < targetSize) {
                        err = KLError_ (klBufferTooSmallErr);
                    } else {
                        err = __KLPreferencesGetKerberosLoginShowOptions ((KLBoolean *) ioBuffer);
                    }
                }
            }

            if (err == klNoErr) {
                *ioBufferSize = targetSize;
            }
        } break;

        case loginOption_RememberShowOptions: {
            targetSize = sizeof(KLBoolean);

            if (err == klNoErr) {
                if (!returnSizeOnly) {
                    if (*ioBufferSize < targetSize) {
                        err = KLError_ (klBufferTooSmallErr);
                    } else {
                        err = __KLPreferencesGetKerberosLoginRememberShowOptions ((KLBoolean *) ioBuffer);
                    }
                }
            }

            if (err == klNoErr) {
                *ioBufferSize = targetSize;
            }
        } break;

        case loginOption_RememberPrincipal: {
            targetSize = sizeof(KLBoolean);

            if (err == klNoErr) {
                if (!returnSizeOnly) {
                    if (*ioBufferSize < targetSize) {
                        err = KLError_ (klBufferTooSmallErr);
                    } else {
                        err = __KLPreferencesGetKerberosLoginRememberPrincipal ((KLBoolean *) ioBuffer);
                    }
                }
            }

            if (err == klNoErr) {
                *ioBufferSize = targetSize;
            }
        } break;

        case loginOption_RememberExtras: {
            targetSize = sizeof(KLBoolean);
            
            if (err == klNoErr) {
                if (!returnSizeOnly) {
                    if (*ioBufferSize < targetSize) {
                        err = KLError_ (klBufferTooSmallErr);
                    } else {
                        err = __KLPreferencesGetKerberosLoginRememberExtras ((KLBoolean *) ioBuffer);
                    }
                }                    
            }
            
            if (err == klNoErr) {
                *ioBufferSize = targetSize;
            }
        } break;
			
        case loginOption_MinimalTicketLifetime: {
            targetSize = sizeof(KLLifetime);
            
            if (err == klNoErr) {
                if (!returnSizeOnly) {
                    if (*ioBufferSize < targetSize) {
                        err = KLError_ (klBufferTooSmallErr);
                    } else {
                        err = __KLPreferencesGetKerberosLoginMinimumTicketLifetime ((KLLifetime *) ioBuffer);
                    }
                }                    
            }
            
            if (err == klNoErr) {
                *ioBufferSize = targetSize;
            }
        } break;

        case loginOption_MaximalTicketLifetime: {
            targetSize = sizeof(KLLifetime);
            
            if (err == klNoErr) {
                if (!returnSizeOnly) {
                    if (*ioBufferSize < targetSize) {
                        err = KLError_ (klBufferTooSmallErr);
                    } else {
                        err = __KLPreferencesGetKerberosLoginMaximumTicketLifetime ((KLLifetime *) ioBuffer);
                    }
                }                    
            }
            
            if (err == klNoErr) {
                *ioBufferSize = targetSize;
            }
        } break;
			
        case loginOption_DefaultTicketLifetime: {
            targetSize = sizeof(KLLifetime);
            
            if (err == klNoErr) {
                if (!returnSizeOnly) {
                    if (*ioBufferSize < targetSize) {
                        err = KLError_ (klBufferTooSmallErr);
                    } else {
                        err = __KLPreferencesGetKerberosLoginDefaultTicketLifetime ((KLLifetime *) ioBuffer);
                    }
                }                    
            }
            
            if (err == klNoErr) {
                *ioBufferSize = targetSize;
            }
        } break;

        case loginOption_LongTicketLifetimeDisplay: {
            targetSize = sizeof(KLBoolean);
            
            if (err == klNoErr) {
                if (!returnSizeOnly) {
                    if (*ioBufferSize < targetSize) {
                        err = KLError_ (klBufferTooSmallErr);
                    } else {
                        err = __KLPreferencesGetKerberosLoginLongLifetimeDisplay ((KLBoolean *) ioBuffer);
                    }
                }                    
            }
            
            if (err == klNoErr) {
                *ioBufferSize = targetSize;
            }
        } break;

        case loginOption_DefaultRenewableTicket: {
            targetSize = sizeof(KLBoolean);
            
            if (err == klNoErr) {
                if (!returnSizeOnly) {
                    if (*ioBufferSize < targetSize) {
                        err = KLError_ (klBufferTooSmallErr);
                    } else {
                        err = __KLPreferencesGetKerberosLoginDefaultRenewableTicket ((KLBoolean *) ioBuffer);
                    }
                }                    
            }
            
            if (err == klNoErr) {
                *ioBufferSize = targetSize;
            }
        } break;

        case loginOption_MinimalRenewableLifetime: {
            targetSize = sizeof(KLLifetime);
            
            if (err == klNoErr) {
                if (!returnSizeOnly) {
                    if (*ioBufferSize < targetSize) {
                        err = KLError_ (klBufferTooSmallErr);
                    } else {
                        err = __KLPreferencesGetKerberosLoginMinimumRenewableLifetime ((KLLifetime *) ioBuffer);
                    }
                }                    
            }
            
            if (err == klNoErr) {
                *ioBufferSize = targetSize;
            }
        } break;

        case loginOption_MaximalRenewableLifetime: {
            targetSize = sizeof(KLLifetime);
            
            if (err == klNoErr) {
                if (!returnSizeOnly) {
                    if (*ioBufferSize < targetSize) {
                        err = KLError_ (klBufferTooSmallErr);
                    } else {
                        err = __KLPreferencesGetKerberosLoginMaximumRenewableLifetime ((KLLifetime *) ioBuffer);
                    }
                }                    
            }
            
            if (err == klNoErr) {
                *ioBufferSize = targetSize;
            }
        } break;
			
        case loginOption_DefaultRenewableLifetime: {
            targetSize = sizeof(KLLifetime);
            
            if (err == klNoErr) {
                if (!returnSizeOnly) {
                    if (*ioBufferSize < targetSize) {
                        err = KLError_ (klBufferTooSmallErr);
                    } else {
                        err = __KLPreferencesGetKerberosLoginDefaultRenewableLifetime ((KLLifetime *) ioBuffer);
                    }
                }                    
            }
            
            if (err == klNoErr) {
                *ioBufferSize = targetSize;
            }
        } break;

        case loginOption_DefaultForwardableTicket: {
            targetSize = sizeof(KLBoolean);
            
            if (err == klNoErr) {
                if (!returnSizeOnly) {
                    if (*ioBufferSize < targetSize) {
                        err = KLError_ (klBufferTooSmallErr);
                    } else {
                        err = __KLPreferencesGetKerberosLoginDefaultForwardableTicket ((KLBoolean *) ioBuffer);
                    }
                }                    
            }
            
            if (err == klNoErr) {
                *ioBufferSize = targetSize;
            }
        } break;

        case loginOption_DefaultProxiableTicket: {
            targetSize = sizeof(KLBoolean);
            
            if (err == klNoErr) {
                if (!returnSizeOnly) {
                    if (*ioBufferSize < targetSize) {
                        err = KLError_ (klBufferTooSmallErr);
                    } else {
                        err = __KLPreferencesGetKerberosLoginDefaultProxiableTicket ((KLBoolean *) ioBuffer);
                    }
                }                    
            }
            
            if (err == klNoErr) {
                *ioBufferSize = targetSize;
            }
        } break;

        case loginOption_DefaultAddresslessTicket: {
            targetSize = sizeof(KLBoolean);
            
            if (err == klNoErr) {
                if (!returnSizeOnly) {
                    if (*ioBufferSize < targetSize) {
                        err = KLError_ (klBufferTooSmallErr);
                    } else {
                        err = __KLPreferencesGetKerberosLoginDefaultAddresslessTicket ((KLBoolean *) ioBuffer);
                    }
                }                    
            }
            
            if (err == klNoErr) {
                *ioBufferSize = targetSize;
            }
        } break;

        default:
            err = KLError_ (klInvalidOptionErr);
            break;
    }

    return KLError_ (err);
}
     
// ---------------------------------------------------------------------------

KLStatus KLSetDefaultLoginOption (const KLDefaultLoginOption   inOption,
                                  const void                  *inBuffer,
                                  const KLSize                 inBufferSize)
{
    KLStatus err = klNoErr;

    if (inBuffer == NULL) { err = KLError_ (klParameterErr); }
    if (inBufferSize < 0) { err = KLError_ (klParameterErr); }

    switch (inOption) {
        case loginOption_LoginName: {
            char *name = NULL;
            
            if (err == klNoErr) {
                err = __KLCreateStringFromBuffer (inBuffer, inBufferSize, &name);
            }
            
            if (err == klNoErr) {
                err = __KLPreferencesSetKerberosLoginName (name);
            }
            
            if (name != NULL) { KLDisposeString (name); }
        } break;

        case loginOption_LoginInstance: {
            char *instance = NULL;
            
            if (err == klNoErr) {
                err = __KLCreateStringFromBuffer (inBuffer, inBufferSize, &instance);
            }
            
            if (err == klNoErr) {
                err = __KLPreferencesSetKerberosLoginInstance (instance);
            }
            
            if (instance != NULL) { KLDisposeString (instance); }
        } break;

        case loginOption_ShowOptions: {
            if (err == klNoErr) {
                if (inBufferSize > sizeof (KLBoolean)) {
                    err = KLError_ (klBufferTooLargeErr);
                } else if (inBufferSize < sizeof (KLBoolean)) {
                    err = KLError_ (klBufferTooSmallErr);
                }
            }

            if (err == klNoErr) {
                err = __KLPreferencesSetKerberosLoginShowOptions (*(KLBoolean *)inBuffer);
            }
        } break;

        case loginOption_RememberShowOptions: {
            if (err == klNoErr) {
                if (inBufferSize > sizeof (KLBoolean)) {
                    err = KLError_ (klBufferTooLargeErr);
                } else if (inBufferSize < sizeof (KLBoolean)) {
                    err = KLError_ (klBufferTooSmallErr);
                }
            }

            if (err == klNoErr) {
                err = __KLPreferencesSetKerberosLoginRememberShowOptions (*(KLBoolean *)inBuffer);
            }
        } break;

        case loginOption_RememberPrincipal: {
            if (err == klNoErr) {
                if (inBufferSize > sizeof (KLBoolean)) {
                    err = KLError_ (klBufferTooLargeErr);
                } else if (inBufferSize < sizeof (KLBoolean)) {
                    err = KLError_ (klBufferTooSmallErr);
                }
            }

            if (err == klNoErr) {
                err = __KLPreferencesSetKerberosLoginRememberPrincipal (*(KLBoolean *)inBuffer);
            }
        } break;

        case loginOption_RememberExtras: {
            if (err == klNoErr) {
                if (inBufferSize > sizeof (KLBoolean)) {
                    err = KLError_ (klBufferTooLargeErr);
                } else if (inBufferSize < sizeof (KLBoolean)) {
                     err = KLError_ (klBufferTooSmallErr);
                }
            }
            
            if (err == klNoErr) {
                err = __KLPreferencesSetKerberosLoginRememberExtras (*(KLBoolean *)inBuffer);
            }
        } break;
			
        case loginOption_MinimalTicketLifetime: {
            if (err == klNoErr) {
                if (inBufferSize > sizeof (KLLifetime)) {
                    err = KLError_ (klBufferTooLargeErr);
                } else if (inBufferSize < sizeof (KLLifetime)) {
                     err = KLError_ (klBufferTooSmallErr);
                }
            }
            
            if (err == klNoErr) {
                err = __KLPreferencesSetKerberosLoginMinimumTicketLifetime (*(KLLifetime *)inBuffer);
            }
        } break;
			
        case loginOption_MaximalTicketLifetime: {
            if (err == klNoErr) {
                if (inBufferSize > sizeof (KLLifetime)) {
                    err = KLError_ (klBufferTooLargeErr);
                } else if (inBufferSize < sizeof (KLLifetime)) {
                     err = KLError_ (klBufferTooSmallErr);
                }
            }
            
            if (err == klNoErr) {
                err = __KLPreferencesSetKerberosLoginMaximumTicketLifetime (*(KLLifetime *)inBuffer);
            }
        } break;
			
        case loginOption_DefaultTicketLifetime: {
            if (err == klNoErr) {
                if (inBufferSize > sizeof (KLLifetime)) {
                    err = KLError_ (klBufferTooLargeErr);
                } else if (inBufferSize < sizeof (KLLifetime)) {
                     err = KLError_ (klBufferTooSmallErr);
                }
            }
            
            if (err == klNoErr) {
                err = __KLPreferencesSetKerberosLoginDefaultTicketLifetime (*(KLLifetime *)inBuffer);
            }
        } break;
			
        case loginOption_LongTicketLifetimeDisplay: {
            if (err == klNoErr) {
                if (inBufferSize > sizeof (KLBoolean)) {
                    err = KLError_ (klBufferTooLargeErr);
                } else if (inBufferSize < sizeof (KLBoolean)) {
                     err = KLError_ (klBufferTooSmallErr);
                }
            }
            
            if (err == klNoErr) {
                err = __KLPreferencesSetKerberosLoginLongLifetimeDisplay  (*(KLBoolean *)inBuffer);
            }
        } break;
		
		case loginOption_DefaultRenewableTicket: {
            if (err == klNoErr) {
                if (inBufferSize > sizeof (KLBoolean)) {
                    err = KLError_ (klBufferTooLargeErr);
                } else if (inBufferSize < sizeof (KLBoolean)) {
                     err = KLError_ (klBufferTooSmallErr);
                }
            }
            
            if (err == klNoErr) {
                err = __KLPreferencesSetKerberosLoginDefaultRenewableTicket  (*(KLBoolean *)inBuffer);
            }
        } break;

        case loginOption_MinimalRenewableLifetime: {
            if (err == klNoErr) {
                if (inBufferSize > sizeof (KLLifetime)) {
                    err = KLError_ (klBufferTooLargeErr);
                } else if (inBufferSize < sizeof (KLLifetime)) {
                     err = KLError_ (klBufferTooSmallErr);
                }
            }
            
            if (err == klNoErr) {
                err = __KLPreferencesSetKerberosLoginMinimumRenewableLifetime (*(KLLifetime *)inBuffer);
            }
        } break;
			
        case loginOption_MaximalRenewableLifetime: {
            if (err == klNoErr) {
                if (inBufferSize > sizeof (KLLifetime)) {
                    err = KLError_ (klBufferTooLargeErr);
                } else if (inBufferSize < sizeof (KLLifetime)) {
                     err = KLError_ (klBufferTooSmallErr);
                }
            }
            
            if (err == klNoErr) {
                err = __KLPreferencesSetKerberosLoginMaximumRenewableLifetime (*(KLLifetime *)inBuffer);
            }
        } break;
			
        case loginOption_DefaultRenewableLifetime: {
            if (err == klNoErr) {
                if (inBufferSize > sizeof (KLLifetime)) {
                    err = KLError_ (klBufferTooLargeErr);
                } else if (inBufferSize < sizeof (KLLifetime)) {
                     err = KLError_ (klBufferTooSmallErr);
                }
            }
            
            if (err == klNoErr) {
                err = __KLPreferencesSetKerberosLoginDefaultRenewableLifetime (*(KLLifetime *)inBuffer);
            }
        } break;

        case loginOption_DefaultForwardableTicket: {
            if (err == klNoErr) {
                if (inBufferSize > sizeof (KLBoolean)) {
                    err = KLError_ (klBufferTooLargeErr);
                } else if (inBufferSize < sizeof (KLBoolean)) {
                     err = KLError_ (klBufferTooSmallErr);
                }
            }
            
            if (err == klNoErr) {
                err = __KLPreferencesSetKerberosLoginDefaultForwardableTicket (*(KLBoolean *)inBuffer);
            }
        } break;

        case loginOption_DefaultProxiableTicket: {
            if (err == klNoErr) {
                if (inBufferSize > sizeof (KLBoolean)) {
                    err = KLError_ (klBufferTooLargeErr);
                } else if (inBufferSize < sizeof (KLBoolean)) {
                     err = KLError_ (klBufferTooSmallErr);
                }
            }
            
            if (err == klNoErr) {
                err = __KLPreferencesSetKerberosLoginDefaultProxiableTicket (*(KLBoolean *)inBuffer);
            }
        } break;

        case loginOption_DefaultAddresslessTicket: {
            if (err == klNoErr) {
                if (inBufferSize > sizeof (KLBoolean)) {
                    err = KLError_ (klBufferTooLargeErr);
                } else if (inBufferSize < sizeof (KLBoolean)) {
                     err = KLError_ (klBufferTooSmallErr);
                }
            }
            
            if (err == klNoErr) {
                err = __KLPreferencesSetKerberosLoginDefaultAddresslessTicket (*(KLBoolean *)inBuffer);
            }
        } break;

        default:
            err = KLError_ (klInvalidOptionErr);
            break;
    }

    return KLError_ (err);
}
     
// ---------------------------------------------------------------------------
		
#pragma mark -

/* Realm configuration functions */


KLStatus KLFindKerberosRealmByName (
		const	char	*inRealmName,
		KLIndex			*outIndex)
{
    return __KLPreferencesGetKerberosLoginRealmByName (inRealmName, outIndex);
}

// ---------------------------------------------------------------------------

KLStatus KLGetKerberosRealm (
		KLIndex			  inIndex,
		char			**outRealmName)
{
    return __KLPreferencesGetKerberosLoginRealm (inIndex, outRealmName);
}

// ---------------------------------------------------------------------------

KLStatus KLSetKerberosRealm (
		KLIndex			 inIndex,
		const char		*inRealmName)
{
    return __KLPreferencesSetKerberosLoginRealm (inIndex, inRealmName);
}

// ---------------------------------------------------------------------------

KLStatus KLRemoveKerberosRealm (KLIndex inIndex)
{
    return __KLPreferencesRemoveKerberosLoginRealm (inIndex);
}

// ---------------------------------------------------------------------------

KLStatus KLInsertKerberosRealm (
		KLIndex			 inInsertBeforeIndex,
		const char		*inRealmName)
{
    return __KLPreferencesInsertKerberosLoginRealm (inInsertBeforeIndex, inRealmName);
}

// ---------------------------------------------------------------------------
		
KLStatus KLRemoveAllKerberosRealms (void)
{
    return __KLPreferencesRemoveAllKerberosLoginRealms ();
}

// ---------------------------------------------------------------------------
		
KLSize KLCountKerberosRealms (void)
{
    KLStatus err = klNoErr;
    KLIndex realmCount;
    
    err = KLError_ (__KLPreferencesCountKerberosLoginRealms (&realmCount));
	
    if (err == klNoErr) {
        return realmCount;
    } else {
        return 0;
    }
}

// ---------------------------------------------------------------------------
        
KLStatus KLGetKerberosDefaultRealm(KLIndex *outIndex)
{
    return __KLPreferencesGetKerberosLoginDefaultRealm (outIndex);
}

// ---------------------------------------------------------------------------
     
KLStatus KLGetKerberosDefaultRealmByName (char **outRealmName)
{
    return __KLPreferencesGetKerberosLoginDefaultRealmByName (outRealmName);
}

// ---------------------------------------------------------------------------
        
KLStatus KLSetKerberosDefaultRealm (KLIndex inIndex)
{
    return __KLPreferencesSetKerberosLoginDefaultRealm (inIndex);
}

// ---------------------------------------------------------------------------
        
KLStatus KLSetKerberosDefaultRealmByName (const char *inRealmName)
{
    return __KLPreferencesSetKerberosLoginDefaultRealmByName (inRealmName);
}
