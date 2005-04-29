/*
 * KLPrincipal.c
 *
 * $Header: /cvs/kfm/KerberosFramework/KerberosLogin/Sources/KerberosLogin/KLPrincipal.c,v 1.15 2004/05/14 02:47:02 lxs Exp $
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

typedef struct OpaqueKLPrincipal {
    krb5_context   context;
    krb5_principal principal;
} Principal;

static KLStatus __KLTranslateKLPrincipal (KLPrincipal inPrincipal);

/* KLPrincipal functions */


// ---------------------------------------------------------------------------

KLStatus KLCreatePrincipalFromTriplet (const char  *inName,
                                       const char  *inInstance,
                                       const char  *inRealm,
                                       KLPrincipal *outPrincipal)
{
    KLKerberosVersion version = (__KLRealmHasKerberos5Profile (inRealm) ||
                                 !__KLRealmHasKerberos4Profile (inRealm)) ? kerberosVersion_V5 :
                                                                            kerberosVersion_V4;
    return __KLCreatePrincipalFromTriplet (inName, inInstance, inRealm, version, outPrincipal);
}


// ---------------------------------------------------------------------------

KLStatus __KLCreatePrincipalFromTriplet (const char  *inName,
                                         const char  *inInstance,
                                         const char  *inRealm,
                                         KLKerberosVersion  inKerberosVersion,
                                         KLPrincipal *outPrincipal)
{
    KLStatus    err = klNoErr;
    KLPrincipal principal = NULL;

    if (inName       == NULL) { err = KLError_ (klParameterErr); }
    if (inInstance   == NULL) { err = KLError_ (klParameterErr); }
    if (inRealm      == NULL) { err = KLError_ (klParameterErr); }
    if (outPrincipal == NULL) { err = KLError_ (klParameterErr); }

    // Create the principal structure
    if (err == klNoErr) {
        principal = (KLPrincipal) malloc (sizeof (Principal));
        if (principal == NULL) { err = KLError_ (klMemFullErr); }
    }

    if (err == klNoErr) {
        principal->context = NULL;
        principal->principal = NULL;
    }
    
    if (err == klNoErr) {
        err = krb5_init_context (&principal->context);
    }

    if (err == klNoErr) {
        // What type of conversion does the caller want?
        if (inKerberosVersion == kerberosVersion_V4) {
            err = krb5_425_conv_principal (principal->context, inName, inInstance, inRealm,
                                           &principal->principal);
        } else if (inKerberosVersion == kerberosVersion_V5) {
            err = krb5_build_principal_ext (principal->context, &principal->principal,
                                            strlen (inRealm), inRealm,
                                            strlen (inName), inName,
                                            strlen (inInstance), inInstance, NULL);
        } else {
            err = KLError_ (klInvalidVersionErr);
        }
    }
    
    if (err == klNoErr) {
        err = __KLTranslateKLPrincipal (principal);
    }
    
    if (err == klNoErr) {
        *outPrincipal = principal;
        principal = NULL;
    }

    if (principal != NULL) {
        if (principal->context != NULL) {
            if (principal->principal != NULL) {
                krb5_free_principal (principal->context, principal->principal);
            }
            krb5_free_context (principal->context);
        }
        free (principal);
    }
    
    return KLError_ (err);
}
     
// ---------------------------------------------------------------------------

KLStatus KLCreatePrincipalFromString (const char        *inFullPrincipal,
                                      KLKerberosVersion  inKerberosVersion,
                                      KLPrincipal       *outPrincipal)
{
    KLStatus    err = klNoErr;
    KLPrincipal principal = NULL;

    if (inFullPrincipal == NULL) { err = KLError_ (klParameterErr); }
    if (outPrincipal    == NULL) { err = KLError_ (klParameterErr); }

    // Create the principal structure
    if (err == klNoErr) {
        principal = (KLPrincipal) malloc (sizeof (Principal));
        if (principal == NULL) { err = KLError_ (klMemFullErr); }
    }

    if (err == klNoErr) {
        principal->context = NULL;
        principal->principal = NULL;
    }
    
    if (err == klNoErr) {
        err = krb5_init_context (&principal->context);
    }
    
    // What type of conversion does the caller want?
    if (inKerberosVersion == kerberosVersion_V4) {
        char name [ANAME_SZ];
        char instance [INST_SZ];
        char realm [REALM_SZ];

        if (err == klNoErr) {
            name[0] = '\0';
            instance[0] = '\0';
            realm[0] = '\0';    // So we can detect that kname_parse didn't change realm
            
            err = kname_parse (name, instance, realm, (char *) inFullPrincipal);
            err = __KLRemapKerberos4Error (err);
        }

        // kname_parse doesn't fill in the realm if the principal string doesn't have it
        // if it didn't get filled in, fill it in with the default realm
        if (err == klNoErr) {
            if (realm[0] == '\0') {
                err = krb_get_lrealm (realm, 1);
                err = __KLRemapKerberos4Error (err);
            }
        }
        
        if (err == klNoErr) {
            err = krb5_425_conv_principal (principal->context, name, instance, realm, &principal->principal);
        }

    } else if (inKerberosVersion == kerberosVersion_V5) {
        if (err == klNoErr) {
            err = krb5_parse_name (principal->context, inFullPrincipal, &principal->principal);
        }
        
    } else {
        err = KLError_ (klInvalidVersionErr);
    }

    if (err == klNoErr) {
        err = __KLTranslateKLPrincipal (principal);
    }

    if (err == klNoErr) {
        *outPrincipal = principal;
        principal = NULL;
    }

    if (principal != NULL) {
        if (principal->context != NULL) {
            if (principal->principal != NULL) {
                krb5_free_principal (principal->context, principal->principal);
            }
            krb5_free_context (principal->context);
        }
        free (principal);
    }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus KLCreatePrincipalFromKerberos5Principal (krb5_principal  inKerberos5Principal,
                                                  KLPrincipal    *outPrincipal)
{
    KLStatus    err = klNoErr;
    KLPrincipal principal = NULL;
    
    if (inKerberos5Principal == NULL) { err = KLError_ (klParameterErr); }
    if (outPrincipal         == NULL) { err = KLError_ (klParameterErr); }
    
    // Create the principal structure
    if (err == klNoErr) {
        principal = (KLPrincipal) malloc (sizeof (Principal));
        if (principal == NULL) { err = KLError_ (klMemFullErr); }
    }
    
    if (err == klNoErr) {
        principal->context = NULL;
        principal->principal = NULL;
    }
    
    if (err == klNoErr) {
        err = krb5_init_context (&principal->context);
    }
    
    if (err == klNoErr) {
        err = krb5_copy_principal (principal->context, 
                                   inKerberos5Principal, &principal->principal);
    }
    
    if (err == klNoErr) {
        *outPrincipal = principal;
        principal = NULL;
    }
    
    if (principal != NULL) {
        if (principal->context != NULL) {
            if (principal->principal != NULL) {
                krb5_free_principal (principal->context, principal->principal);
            }
            krb5_free_context (principal->context);
        }
        free (principal);
    }
    
    return KLError_ (err);    
}

// ---------------------------------------------------------------------------
    
KLStatus KLCreatePrincipalFromPrincipal (KLPrincipal inPrincipal,
                                         KLPrincipal *outPrincipal)
{
    KLStatus     err = klNoErr;
    
    if (inPrincipal  == NULL) { err = KLError_ (klParameterErr); }
    if (outPrincipal == NULL) { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        err = KLCreatePrincipalFromKerberos5Principal (inPrincipal->principal, outPrincipal);
    }
        
    return KLError_ (err);    
}


// ---------------------------------------------------------------------------

KLStatus KLGetTripletFromPrincipal (KLPrincipal   inPrincipal,
                                    char        **outName,
                                    char        **outInstance,
                                    char        **outRealm)
{
    return __KLGetTripletFromPrincipal (inPrincipal, kerberosVersion_Any, outName, outInstance, outRealm);
}

// ---------------------------------------------------------------------------

KLStatus __KLGetTripletFromPrincipal (KLPrincipal         inPrincipal,
                                      KLKerberosVersion   inKerberosVersion,
                                      char              **outName,
                                      char              **outInstance,
                                      char              **outRealm)
{
    KLStatus err = klNoErr;
    KLKerberosVersion version = inKerberosVersion;
    char *name = NULL;
    char *instance = NULL;
    char *realm = NULL;
    
    if (inPrincipal == NULL) { return KLError_ (klBadPrincipalErr); }
    if (outName     == NULL) { return KLError_ (klParameterErr); }
    if (outInstance == NULL) { return KLError_ (klParameterErr); }
    if (outRealm    == NULL) { return KLError_ (klParameterErr); }

    if (err == klNoErr) {
        switch (version) {
            case kerberosVersion_V4:
            case kerberosVersion_V5:
                break; // Keep passed in realm

            case kerberosVersion_All:
            case kerberosVersion_Any:
                // If there is no configuration at all, do krb5.
                if (__KLPrincipalHasKerberos5Profile (inPrincipal) ||
                    !__KLPrincipalHasKerberos4Profile (inPrincipal)) {
                    version = kerberosVersion_V5;
                } else {
                    version = kerberosVersion_V4;
                }
                break;

            default:
                err = KLError_ (klInvalidVersionErr);
                break;
        }
    }

    if (version == kerberosVersion_V5) {
        int componentCount = 0;

        if (err == klNoErr) {
            // make sure there aren't too many instances
            componentCount = krb5_princ_size (inPrincipal->context, inPrincipal->principal);
            if (componentCount > 2) { err = KLError_ (klBadPrincipalErr); }
        }

        if (err == klNoErr) {
            krb5_data *nameData = krb5_princ_name (inPrincipal->context, inPrincipal->principal);
            err = __KLCreateStringFromBuffer (nameData->data, nameData->length, &name);
        }

        if (err == klNoErr) {
            if (componentCount > 1) {
                krb5_data *instanceData = krb5_princ_component (inPrincipal->context, inPrincipal->principal, 1);
                err = __KLCreateStringFromBuffer (instanceData->data, instanceData->length, &instance);
            } else {
                err = __KLCreateString ("", &instance);
            }
        }

        if (err == klNoErr) {
            krb5_data *realmData = krb5_princ_realm (inPrincipal->context, inPrincipal->principal);
            err = __KLCreateStringFromBuffer (realmData->data, realmData->length, &realm);
        }
    } else if (version == kerberosVersion_V4) {
        char v4Name [ANAME_SZ];
        char v4Instance [INST_SZ];
        char v4Realm [REALM_SZ];

        if (err == klNoErr) {
            err = krb5_524_conv_principal (inPrincipal->context, inPrincipal->principal,
                                           v4Name, v4Instance, v4Realm);
        }

        if (err == klNoErr) {
            err = __KLCreateString (v4Name, &name);
        }
        
        if (err == klNoErr) {
            err = __KLCreateString (v4Instance, &instance);
        }
        
        if (err == klNoErr) {
            err = __KLCreateString (v4Realm, &realm);
        }
    } else {
        err = KLError_ (klInvalidVersionErr);
    }
    
    if (err == klNoErr) {
        *outName = name;
        *outInstance = instance;
        *outRealm = realm;
    } else {
        if (name     != NULL) { KLDisposeString (name); }
        if (instance != NULL) { KLDisposeString (instance); }
        if (realm    != NULL) { KLDisposeString (realm); }
    }
    
    return KLError_ (err);
}
     
// ---------------------------------------------------------------------------

KLStatus KLGetStringFromPrincipal (KLPrincipal        inPrincipal,
                                   KLKerberosVersion  inKerberosVersion,
                                   char             **outFullPrincipal)
{
    KLStatus          err = klNoErr;
    KLKerberosVersion version = inKerberosVersion;
    char             *principalString = NULL;
    krb5_context      context = NULL;
    
    if (inPrincipal      == NULL) { return KLError_ (klBadPrincipalErr); }
    if (outFullPrincipal == NULL) { return KLError_ (klParameterErr); }
    
    switch (version) {
        case kerberosVersion_V4:
        case kerberosVersion_V5:
            break; // Keep passed in realm
		
        case kerberosVersion_All:
        case kerberosVersion_Any:
            // If there is no configuration at all, do krb5.
            if (__KLPrincipalHasKerberos5Profile (inPrincipal) || !__KLPrincipalHasKerberos4Profile (inPrincipal)) {
                version = kerberosVersion_V5;
            } else {
                version = kerberosVersion_V4;
            }
            break;

        default:
            err = KLError_ (klInvalidVersionErr);
            break;
    }
    
    if (err == klNoErr) {
        err = krb5_init_context (&context);
    }
    
    if (version == kerberosVersion_V5) {
        char *tempPrincipalString = NULL;
        
        if (err == klNoErr) {
            err = krb5_unparse_name (inPrincipal->context, inPrincipal->principal, &tempPrincipalString);
        }
        
        if (err == klNoErr) {
            err = __KLCreateString (tempPrincipalString, &principalString);
        }
        
        if (tempPrincipalString != NULL) { krb5_free_unparsed_name (inPrincipal->context, tempPrincipalString); }
        
    } else if (version == kerberosVersion_V4) {
        char name [ANAME_SZ];
		char instance [INST_SZ];
		char realm [REALM_SZ];
		char tempPrincipalString [MAX_K_NAME_SZ];
    
        if (err == klNoErr) {
            err = krb5_524_conv_principal (inPrincipal->context, inPrincipal->principal, name, instance, realm);
        }

        if (err == klNoErr) {
            err = kname_unparse (tempPrincipalString, name, instance, realm);
        }

        if (err == klNoErr) {
            err = __KLCreateString (tempPrincipalString, &principalString);
        }        
    } else {
        err = KLError_ (klInvalidVersionErr);
    }
    
    if (err == klNoErr) {
        *outFullPrincipal = principalString;
    } else {
        if (principalString != NULL) { KLDisposeString (principalString); }
    }
    
    if (context != NULL) { krb5_free_context (context); }

    return KLError_ (err);
}
     
// ---------------------------------------------------------------------------

KLStatus KLGetDisplayStringFromPrincipal (KLPrincipal        inPrincipal,
                                          KLKerberosVersion  inKerberosVersion,
                                          char             **outFullPrincipal)
{
    KLStatus err = klNoErr;
    char *principalString = NULL;
    char *displayPrincipalString = NULL;
    
    if (err == klNoErr) {
        err = KLGetStringFromPrincipal (inPrincipal, inKerberosVersion, &principalString);
    }
    
    if (err == klNoErr) {
        err = __KLCreateString (principalString, &displayPrincipalString);
    }
    
    if (err == klNoErr) {
        KLIndex i = 0, j = 0;
        
        // Make sure to copy the '\0'
        for (i = 0; i <= strlen (principalString); i++) {
            if ((principalString[i] == '\\') && (principalString[i+1] != '\\')) {
                // quoted character.  Skip the quote.
                continue;
            }
            
            displayPrincipalString[j] = principalString[i]; // copy this char
            j++;                                            // advance the ptr
            
            if (principalString[i+1] == '\\') {
                i++; // a quoted backslash is a backslash.  Jump one more forward.
            }
        } 
        
        err = __KLCreateString (displayPrincipalString, outFullPrincipal);
    }
    
    if (principalString        != NULL) { KLDisposeString (principalString); }
    if (displayPrincipalString != NULL) { KLDisposeString (displayPrincipalString); }
    
    return KLError_ (err);
}
     
// ---------------------------------------------------------------------------

KLStatus KLComparePrincipal (KLPrincipal  inFirstPrincipal,
                             KLPrincipal  inSecondPrincipal,
                             KLBoolean   *outAreEquivalent)
{
    KLStatus err = klNoErr;

    if (inFirstPrincipal  == NULL) { err = KLError_ (klBadPrincipalErr); }
    if (inSecondPrincipal == NULL) { err = KLError_ (klBadPrincipalErr); }
    if (outAreEquivalent  == NULL) { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        *outAreEquivalent =  krb5_principal_compare (inFirstPrincipal->context, 
                                                     inFirstPrincipal->principal, 
                                                     inSecondPrincipal->principal);
    }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus KLDisposePrincipal (KLPrincipal inPrincipal)
{
    if (inPrincipal == NULL) { return KLError_ (klBadPrincipalErr); }

    if (inPrincipal->context != NULL) {
        if (inPrincipal->principal != NULL) {
            krb5_free_principal (inPrincipal->context, inPrincipal->principal);
        }
        krb5_free_context (inPrincipal->context);
    }
    
    free (inPrincipal);

    return klNoErr;
}

#pragma mark -

KLStatus __KLCreatePrincipalFromKerberos5Principal (krb5_principal inPrincipal,
                                                    KLPrincipal *outPrincipal)
{
    KLStatus    err = klNoErr;
    KLPrincipal principal = NULL;

    if (inPrincipal  == NULL) { err = KLError_ (klParameterErr); }
    if (outPrincipal == NULL) { err = KLError_ (klParameterErr); }

    // Create the principal structure
    if (err == klNoErr) {
        principal = (KLPrincipal) malloc (sizeof (Principal));
        if (principal == NULL) { err = KLError_ (klMemFullErr); }
    }

    if (err == klNoErr) {
        principal->context = NULL;
        principal->principal = NULL;
    }

    if (err == klNoErr) {
        err = krb5_init_context (&principal->context);
    }

    if (err == klNoErr) {
        err = krb5_copy_principal (principal->context, inPrincipal, &principal->principal);
    }

    if (err == klNoErr) {
        err = __KLTranslateKLPrincipal (principal);
    }

    if (err == klNoErr) {
        *outPrincipal = principal;
        principal = NULL;
    }
    
    if (principal != NULL) {
        if (principal->context != NULL) {
            if (principal->principal != NULL) {
                krb5_free_principal (principal->context, principal->principal);
            }
            krb5_free_context (principal->context);
        }
        free (principal);
    }

    return KLError_ (err);    
}

// ---------------------------------------------------------------------------

krb5_principal __KLGetKerberos5PrincipalFromPrincipal (KLPrincipal inPrincipal)
{
    if (inPrincipal == NULL) {
        return NULL;
    } else {
        return inPrincipal->principal;
    }
}

// ---------------------------------------------------------------------------

KLStatus __KLGetRealmFromPrincipal (KLPrincipal inPrincipal, KLKerberosVersion inKerberosVersion, char **outRealm)
{
    KLStatus err = klNoErr;
    char    *realm = NULL;
    
    if (inPrincipal == NULL) { err = KLError_ (klBadPrincipalErr); }
    if (outRealm == NULL)    { err = KLError_ (klParameterErr); }
    
    switch (inKerberosVersion) {
        case kerberosVersion_V5:
        case kerberosVersion_All:
        case kerberosVersion_Any:
            if (err == klNoErr) {
                krb5_data *realmData = krb5_princ_realm (inPrincipal->context, inPrincipal->principal);
                err = __KLCreateStringFromBuffer (realmData->data, realmData->length, &realm);
            }
            break;

        case kerberosVersion_V4:
            if (err == klNoErr) {
                char v4Name [ANAME_SZ];
                char v4Instance [INST_SZ];
                char v4Realm [REALM_SZ];
                
                if (err == klNoErr) {
                    err = krb5_524_conv_principal (inPrincipal->context, inPrincipal->principal, v4Name, v4Instance, v4Realm);
                }

                if (err == klNoErr) {
                    err = __KLCreateString (v4Realm, &realm);
                }
            }
            break;

        default:
            err = KLError_ (klInvalidVersionErr);
            break;
    }
    
    if (err == klNoErr) {
        *outRealm = realm;
    } else {
        if (realm != NULL) { KLDisposeString (realm); }
    }

    return KLError_ (err);
}

// ---------------------------------------------------------------------------

// A valid tgs principal is one of the form "krbtgt/X@Y" (Kerberos 5) or "krbtgt.X@Y" (Kerberos 4)
// X and Y are both realms, and may be the same realm if the user is not using interrealm.
// We do not check here whether X and Y are realms because it would cause us to hit DNS.
KLBoolean __KLPrincipalIsTicketGrantingService (KLPrincipal inPrincipal, KLKerberosVersion inVersion)
{
    KLStatus  err = klNoErr;
    KLBoolean isTicketGrantingService = false;
    krb5_data *name = NULL;
    KLIndex components;
    char *tgsName = (inVersion == kerberosVersion_V4) ? KRB_TICKET_GRANTING_TICKET : KRB5_TGS_NAME;
    
    if (inPrincipal == NULL) { err = KLError_ (klParameterErr); }

    if (err == klNoErr) {
        components = krb5_princ_size (inPrincipal->context, inPrincipal->principal);
        name = krb5_princ_name (inPrincipal->context, inPrincipal->principal);
    }
    
    if (err == klNoErr) {
        isTicketGrantingService = (components == 2) &&                                // one name and one instance
                                  (strlen (tgsName) == name->length) &&
                                  (strncmp (name->data, tgsName, name->length) == 0); // name is "krbtgt"
    }

    return isTicketGrantingService;
}

#pragma mark -

// ---------------------------------------------------------------------------

static KLStatus __KLTranslateKLPrincipal (KLPrincipal inPrincipal)
{
    KLStatus err = klNoErr;

    if (inPrincipal == NULL) { err = KLError_ (klParameterErr); }

    if (err == klNoErr) {
        char *name = NULL;
        char *instance = NULL;
        char *realm = NULL;

        if (__KLGetTripletFromPrincipal (inPrincipal, kerberosVersion_V5, &name, &instance, &realm) == klNoErr) {
            KLBoolean      changed = false;
            char          *realName = NULL;
            char          *realInstance = NULL;
            char          *realRealm = NULL;
            krb5_principal principal = NULL;

            // Translate the principal, if necessary
            changed = __KLTranslatePrincipal (name, instance, realm, &realName, &realInstance, &realRealm);
            if (!changed) {
                realName = name;
                realInstance = instance;
                realRealm = realm;
            }

            if (err == klNoErr) {
                err = krb5_build_principal_ext (inPrincipal->context, &principal,
                                                strlen (realRealm), realRealm,
                                                strlen (realName), realName,
                                                strlen (realInstance), realInstance, NULL);
            }

            if (err == klNoErr) {
                krb5_principal swap = inPrincipal->principal;
                inPrincipal->principal = principal;
                principal = swap;
            }

            if (changed) {
                if (realName     != NULL) { KLDisposeString (realName); }
                if (realInstance != NULL) { KLDisposeString (realInstance); }
                if (realRealm    != NULL) { KLDisposeString (realRealm); }
            }

            if (principal != NULL) { krb5_free_principal (inPrincipal->context, principal); }            
        }

        if (name     != NULL) { KLDisposeString (name); }
        if (instance != NULL) { KLDisposeString (instance); }
        if (realm    != NULL) { KLDisposeString (realm); }
    }
    
    return KLError_ (err);
}
