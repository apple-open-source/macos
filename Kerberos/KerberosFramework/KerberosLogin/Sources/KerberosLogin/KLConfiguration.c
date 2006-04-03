/*
 * KLConfiguration.c
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

/* Application Configuration functions */

/* Initialize the (per-process) global options */
static pthread_mutex_t gIdleCallbackMutex = PTHREAD_MUTEX_INITIALIZER;
static KLIdleCallback  gIdleCallback = NULL;
static KLRefCon        gIdleRefCon   = 0;

// ---------------------------------------------------------------------------

KLStatus KLSetIdleCallback (const KLIdleCallback inCallback, const KLRefCon inRefCon)
{
    KLStatus lockErr = pthread_mutex_lock (&gIdleCallbackMutex);
    KLStatus err = lockErr;
    
    if (err == klNoErr) {
        gIdleCallback = inCallback;
        gIdleRefCon = inRefCon;
    }
    
    if (!lockErr) { pthread_mutex_unlock (&gIdleCallbackMutex); }
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus KLGetIdleCallback (KLIdleCallback* outCallback, KLRefCon* outRefCon)
{
    KLStatus lockErr = pthread_mutex_lock (&gIdleCallbackMutex);
    KLStatus err = lockErr;
    
    if (outCallback == NULL) { err = KLError_ (klParameterErr); }    
    if (outRefCon   == NULL) { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        *outCallback = gIdleCallback;
        *outRefCon = gIdleRefCon;
    }
    
    if (!lockErr) { pthread_mutex_unlock (&gIdleCallbackMutex); }
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

void __KLCallIdleCallback (void)
{
    KLIdleCallback callback = NULL;
    KLRefCon refCon = 0;
    KLStatus err = KLGetIdleCallback (&callback, &refCon);
    
    if ((err == klNoErr) && (callback != NULL)) {
        CallKLIdleCallback (callback, refCon);
    }
}

#pragma mark -

// ---------------------------------------------------------------------------

static pthread_mutex_t   gApplicationPrompterMutex = PTHREAD_MUTEX_INITIALIZER;
static KLPrompterProcPtr gApplicationPrompter = NULL;

KLStatus __KLSetApplicationPrompter (KLPrompterProcPtr inPrompter)
{
    KLStatus lockErr = pthread_mutex_lock (&gApplicationPrompterMutex);
    KLStatus err = lockErr;
    
    if (err == klNoErr) {
        gApplicationPrompter = inPrompter;
    }
    
    if (!lockErr) { pthread_mutex_unlock (&gApplicationPrompterMutex); }
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLGetApplicationPrompter (KLPrompterProcPtr *outPrompter)
{
    KLStatus lockErr = pthread_mutex_lock (&gApplicationPrompterMutex);
    KLStatus err = lockErr;
    
    if (outPrompter == NULL) { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        *outPrompter = gApplicationPrompter;
    }
    
    if (!lockErr) { pthread_mutex_unlock (&gApplicationPrompterMutex); }
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLBoolean __KLApplicationProvidedPrompter (void)
{
    KLPrompterProcPtr applicationPrompter = NULL;
    KLStatus err = __KLGetApplicationPrompter (&applicationPrompter);
    
    return ((err == klNoErr) && (applicationPrompter != NULL));
}

// ---------------------------------------------------------------------------

krb5_error_code __KLCallApplicationPrompter (krb5_context   context,
                                             void          *data,
                                             const char    *name,
                                             const char    *banner,
                                             int            num_prompts,
                                             krb5_prompt    prompts[])
{
    KLPrompterProcPtr applicationPrompter = NULL;
    KLStatus err = __KLGetApplicationPrompter (&applicationPrompter);
    
    if (applicationPrompter == NULL) { err = KLError_ (klParameterErr); }
    
    return applicationPrompter (context, data, name, banner, num_prompts, prompts);
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

static pthread_mutex_t gKLAllowHomeDirectoryAccessMutex = PTHREAD_MUTEX_INITIALIZER;
static KLBoolean       gKLAllowHomeDirectoryAccess = true;

// ---------------------------------------------------------------------------

KLStatus __KLSetHomeDirectoryAccess (KLBoolean inAllowHomeDirectoryAccess)
{
    KLStatus lockErr = pthread_mutex_lock (&gKLAllowHomeDirectoryAccessMutex);
    KLStatus err = lockErr;
    
    if (err == klNoErr) {
        gKLAllowHomeDirectoryAccess = inAllowHomeDirectoryAccess;
    }
    
    if (!lockErr) { pthread_mutex_unlock (&gKLAllowHomeDirectoryAccessMutex); }
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLGetHomeDirectoryAccess (KLBoolean *outAllowHomeDirectoryAccess)
{
    KLStatus lockErr = pthread_mutex_lock (&gKLAllowHomeDirectoryAccessMutex);
    KLStatus err = lockErr;
    
    if (outAllowHomeDirectoryAccess == NULL) { err = KLError_ (klParameterErr); }

    if (err == klNoErr) {
        *outAllowHomeDirectoryAccess = gKLAllowHomeDirectoryAccess;
    }
    
    if (!lockErr) { pthread_mutex_unlock (&gKLAllowHomeDirectoryAccessMutex); }
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLBoolean __KLAllowHomeDirectoryAccess (void)
{
    KLBoolean allowHomeDirectoryAccess = false;
    KLStatus err = __KLGetHomeDirectoryAccess (&allowHomeDirectoryAccess);

    return ((err == klNoErr) && allowHomeDirectoryAccess);
}

#pragma mark -

// ---------------------------------------------------------------------------

static pthread_once_t gKLPluginRefCountInitOnce = PTHREAD_ONCE_INIT;
static pthread_key_t  gKLPluginRefCountKey;
static KLStatus       gKLPluginRefCountOnceErr = 0;

// ---------------------------------------------------------------------------

static void __KLPluginRefCountInitOnceHook ()
{
    KLStatus err = klNoErr;
    
    if (err == klNoErr) {
        err = pthread_key_create (&gKLPluginRefCountKey, free);
    }
    
    if (err != klNoErr) {
        gKLPluginRefCountOnceErr = err;
    }
}

// ---------------------------------------------------------------------------

static KLStatus __KLPluginRefCountInit ()
{
    KLStatus err = klNoErr;
    
    if (err == klNoErr) {
        err = pthread_once (&gKLPluginRefCountInitOnce, __KLPluginRefCountInitOnceHook);
    }
    
    // If only pthread_once propagated an error from the initializer
    return KLError_ (err ? err : gKLPluginRefCountOnceErr);
}

// ---------------------------------------------------------------------------

static KLStatus __KLPluginSetRefCount (KLIndex *inRefCount)
{
    KLStatus err = __KLPluginRefCountInit ();
    
    if (err == klNoErr) {
        err = pthread_setspecific (gKLPluginRefCountKey, inRefCount);
    }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

static KLStatus __KLPluginGetRefCount (KLIndex **outRefCount)
{
    KLStatus err = __KLPluginRefCountInit ();
    KLIndex *refCount = NULL;
    
    if (outRefCount == NULL) { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        refCount = pthread_getspecific (gKLPluginRefCountKey);
        if (refCount == NULL) {
            refCount = (KLIndex *) calloc (1, sizeof (KLIndex));
            if (refCount == NULL) { err = KLError_ (klMemFullErr); }
        }
    }
    
    if (err == klNoErr) {
        *outRefCount = refCount;
    }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLBeginPlugin (void)
{
    KLIndex *refCount = NULL;
    KLStatus err = __KLPluginGetRefCount (&refCount);
    
    if (err == klNoErr) {
        refCount++;
        err = __KLPluginSetRefCount (refCount);
    }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLEndPlugin (void)
{
    KLIndex *refCount = NULL;
    KLStatus err = __KLPluginGetRefCount (&refCount);
    
    if (err == klNoErr) {
        refCount--;
        err = __KLPluginSetRefCount (refCount);
    }
    
    return KLError_ (err);
}

#pragma mark -

// ---------------------------------------------------------------------------

static pthread_mutex_t   gKLPromptMechanismMutex = PTHREAD_MUTEX_INITIALIZER;
static KLPromptMechanism gKLPromptMechanism = klPromptMechanism_Autodetect;

// ---------------------------------------------------------------------------

KLStatus __KLSetPromptMechanism (KLPromptMechanism inPromptMechanism)
{
    KLStatus lockErr = pthread_mutex_lock (&gKLPromptMechanismMutex);
    KLStatus err = lockErr;
    
    if (err == klNoErr) {
        gKLPromptMechanism = inPromptMechanism;
    }
    
    if (!lockErr) { pthread_mutex_unlock (&gKLPromptMechanismMutex); }
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

static KLStatus __KLGetPromptMechanism (KLPromptMechanism *outPromptMechanism)
{
    KLStatus lockErr = pthread_mutex_lock (&gKLPromptMechanismMutex);
    KLStatus err = lockErr;
    
    if (outPromptMechanism == NULL) { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        *outPromptMechanism = gKLPromptMechanism;
    }
    
    if (!lockErr) { pthread_mutex_unlock (&gKLPromptMechanismMutex); }
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLPromptMechanism __KLPromptMechanism (void)
{
    KLStatus err = klNoErr;
    LoginSessionAttributes attributes = LoginSessionGetSessionAttributes ();
    KLPromptMechanism promptMechanism = klPromptMechanism_Autodetect;
    
    if (err == klNoErr) {
        err = __KLGetPromptMechanism (&promptMechanism);
    }
    
    if (err == klNoErr) {
        if (promptMechanism == klPromptMechanism_Autodetect) {
            if (attributes & loginSessionCallerUsesGUI) {
                promptMechanism = klPromptMechanism_GUI;  // caller is a GUI app
            } else if (attributes & loginSessionHasTerminalAccess) {
                promptMechanism = klPromptMechanism_CLI;  // caller has a controlling terminal
            } else if (attributes & loginSessionHasGraphicsAccess) {
                promptMechanism = klPromptMechanism_GUI;  // we can talk to the window server
            } else {
                dprintf ("__KLPromptMechanism(): no way to talk to the user.");
                promptMechanism = klPromptMechanism_None; // no way to talk to the user
            }
        }
        
        if ((promptMechanism == klPromptMechanism_GUI) && !(attributes & loginSessionHasGraphicsAccess)) {
            dprintf ("__KLPromptMechanism(): caller asked for GUI prompt but we have no window server.");
            promptMechanism = klPromptMechanism_None;
        }
        
        if ((promptMechanism == klPromptMechanism_CLI) && !(attributes & loginSessionHasTerminalAccess)) {
            dprintf ("__KLPromptMechanism(): caller asked for CLI prompt but we have no terminal.");
            promptMechanism = klPromptMechanism_None;
        }
    }
    
    return promptMechanism;
}

#pragma mark -

// ---------------------------------------------------------------------------

static pthread_mutex_t gKLAllowAutomaticPromptingMutex = PTHREAD_MUTEX_INITIALIZER;
static KLBoolean       gKLAllowAutomaticPrompting = true;

// ---------------------------------------------------------------------------

KLStatus __KLSetAutomaticPrompting (KLBoolean inAllowAutomaticPrompting)
{
    KLStatus lockErr = pthread_mutex_lock (&gKLAllowAutomaticPromptingMutex);
    KLStatus err = lockErr;
    
    if (err == klNoErr) {
        gKLAllowAutomaticPrompting = inAllowAutomaticPrompting;
    }
    
    if (!lockErr) { pthread_mutex_unlock (&gKLAllowAutomaticPromptingMutex); }
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLGetAutomaticPrompting (KLBoolean *outAllowAutomaticPrompting)
{
    KLStatus lockErr = pthread_mutex_lock (&gKLAllowAutomaticPromptingMutex);
    KLStatus err = lockErr;
    
    if (outAllowAutomaticPrompting == NULL) { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        *outAllowAutomaticPrompting = gKLAllowAutomaticPrompting;
    }
    
    if (!lockErr) { pthread_mutex_unlock (&gKLAllowAutomaticPromptingMutex); }
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLBoolean __KLAllowAutomaticPrompting (void)
{
    KLStatus err = klNoErr;
    KLBoolean allowAutomaticPrompting = true;
    KLPromptMechanism promptMechanism = klPromptMechanism_Autodetect;
    KLIndex *refCount = NULL;
    
    if (err == klNoErr) {
        err = __KLGetPromptMechanism (&promptMechanism);
    }
    
    if (err == klNoErr) {
        err = __KLGetAutomaticPrompting (&allowAutomaticPrompting);
    }
    
    if (err == klNoErr) {
        err = __KLPluginGetRefCount (&refCount);
    }
    
    if (err == klNoErr) {
        // Don't prompt if environment variable is set.
        if (getenv ("KERBEROSLOGIN_NEVER_PROMPT") != NULL) {
            dprintf ("__KLAllowAutomaticPrompting(): KERBEROSLOGIN_NEVER_PROMPT is set.\n");
            allowAutomaticPrompting = false;
        }
    }
    
    if (err == klNoErr) {
        if (!(LoginSessionGetSessionAttributes () & loginSessionCallerUsesGUI) && (promptMechanism == klPromptMechanism_Autodetect)) {
            dprintf ("__KLAllowAutomaticPrompting(): Prompt mechanism is autodetect and caller is not using a GUI.");
            allowAutomaticPrompting = false;
        }
        
        // Don't prompt if we are in a plug-in because we will reenter KLL code
        if (*refCount > 0) {
            dprintf ("__KLAllowAutomaticPrompting(): We are in a plug-in!  Don't prompt.\n");
            allowAutomaticPrompting = false;
        }
        
    }
    
#warning "__KLAllowAutomaticPrompting () doesn't support zero-config"
    if (err == klNoErr) {
        // Make sure there is at least 1 config file
        // We don't support DNS domain-realm lookup, so if there is no
        // config, Kerberos is guaranteed not to work.
        
        KLBoolean hasKerberosConfig = false;
        char **files = NULL;
        profile_t profile = NULL;
        
        if (krb5_get_default_config_files (&files) == klNoErr) {
            if (profile_init ((const_profile_filespec_t *) files, &profile) == klNoErr) {
                hasKerberosConfig = true;
            }
        }
        
        // Krb4 traditional config files
        if (!hasKerberosConfig) {
            hasKerberosConfig = (krb__get_cnffile () != NULL);
        }
        
        if (!hasKerberosConfig) {
            dprintf ("__KLAllowAutomaticPrompting (): no valid config file.");
            allowAutomaticPrompting = false;
        }
    
        if (profile != NULL) { profile_abandon (profile); }
        if (files   != NULL) { krb5_free_config_files (files); }
    }
    
    if (err != klNoErr) {
        // Fatal error, don't prompt because we don't know what's going on!
        allowAutomaticPrompting = false;
    }
    
    dprintf ("__KLAllowAutomaticPrompting (): will %sallow prompting.", allowAutomaticPrompting ? "" : "not ");
    return allowAutomaticPrompting;
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
