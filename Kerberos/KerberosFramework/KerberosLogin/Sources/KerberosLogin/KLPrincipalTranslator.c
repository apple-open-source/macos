/*
 * KLPrincipalTranslator.c
 *
 * $Header: /cvs/kfm/KerberosFramework/KerberosLogin/Sources/KerberosLogin/KLPrincipalTranslator.c,v 1.8 2003/08/08 21:34:51 lxs Exp $
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

#define kKLPT_TranslatePrincipal CFSTR("KerberosLoginPrincipalTranslation_TranslatePrincipal")
#define kKLPT_ReleasePrincipal   CFSTR("KerberosLoginPrincipalTranslation_ReleasePrincipal")
#define kKLPT_InitializePlugin   CFSTR("KerberosLoginPrincipalTranslation_InitializePlugin")
 
typedef KLStatus (*KLPT_InitializePlugin) (
	KLPT_APIVersion	inAPIVersion);

typedef KLStatus (*KLPT_TranslatePrincipal) (
	const char*  inName,
	const char*  inInstance,
	const char*  inRealm,
	char**       outName,
	char**       outInstance,
	char**       outRealm,
	KLBoolean*   outChanged);

typedef void (*KLPT_ReleasePrincipal) (
	const char*	inName,
	const char*	inInstance,
	const char*	inRealm);

static KLStatus __KLFindTranslator (const char               *inTranslatorName,
                                    CFBundleRef              *outBundle,
                                    KLPT_TranslatePrincipal  *outTranslate,
                                    KLPT_ReleasePrincipal    *outRelease);

static KLStatus __KLCallTranslator (KLPT_TranslatePrincipal inTranslator,
                                    KLPT_ReleasePrincipal   inRelease,
                                    const char             *inName,
                                    const char             *inInstance,
                                    const char             *inRealm,
                                    char                  **outName,
                                    char                  **outInstance,
                                    char                  **outRealm,
                                    KLBoolean              *outChanged);


#pragma mark -

KLBoolean __KLTranslatePrincipal (const char       *inName,
                                  const char       *inInstance,
                                  const char       *inRealm,
                                  char            **outName,
                                  char            **outInstance,
                                  char            **outRealm)
{
    KLStatus err = klNoErr;
    KLBoolean changed = false;
    
    char translatorName[MAXPATHLEN];
    
    krb5_context context = NULL;
    profile_t    profile = NULL;
    const char  *libdefaultRealmList[4] = { "libdefaults", inRealm, "principal_translation", NULL };
    const char  *libdefaultList[3]      = { "libdefaults", "principal_translation", NULL };
    const char  *v5RealmsList[4]        = { "realms", inRealm, "principal_translation", NULL };
    const char  *v4RealmsList[4]        = { REALMS_V4_PROF_REALMS_SECTION, inRealm, "principal_translation", NULL };
    char       **values = NULL;
    
    CFBundleRef				translatorBundle = NULL;
    KLPT_TranslatePrincipal	translatePrincipal = NULL;
    KLPT_ReleasePrincipal	releasePrincipal = NULL;

    if (err == klNoErr) {
        err = krb5_init_context (&context);
    }
    
    if (err == klNoErr) {
        err = krb5_get_profile (context, &profile);
    }
    
    if (err == klNoErr) {
        err = profile_get_values (profile, libdefaultRealmList, &values);

        if (err != klNoErr) {
            err = profile_get_values (profile, libdefaultList, &values);
        }

        if ((err != klNoErr) && __KLRealmHasKerberos5Profile (inRealm)) {
            err = profile_get_values (profile, v5RealmsList, &values);
        }
        
        if ((err != klNoErr) && __KLRealmHasKerberos4Profile (inRealm)) {
            err = profile_get_values (profile, v4RealmsList, &values);
        }
    }
        
    if (err == klNoErr) {
        if ((values[0] != NULL) && (strlen (values[0]) < (MAXPATHLEN - 1))) {
            strcpy (translatorName, values[0]);
        } else {
            err = KLError_ (klParameterErr);
        }
    }
    
    if (err == klNoErr) {
        dprintf ("TranslatePrincipal: translating realm '%s' with translator '%s'\n", inRealm, translatorName);
		err = __KLFindTranslator (translatorName, &translatorBundle, &translatePrincipal, &releasePrincipal);
	}
    
    if (err == klNoErr) {
        if ((translatePrincipal != NULL) && (releasePrincipal != NULL)) {
            err = __KLCallTranslator (translatePrincipal, releasePrincipal,
                                      inName, inInstance, inRealm,
                                      outName, outInstance, outRealm, &changed);
        }
    }

    if (translatorBundle != NULL) {
        dprintf ("TranslatePrincipal: Unloaded translator '%s'\n", translatorName);
        CFBundleUnloadExecutable (translatorBundle);
        CFRelease (translatorBundle);
    }
    
    if (values  != NULL) { profile_free_list (values); }
    if (profile != NULL) { profile_abandon (profile); }
    if (context != NULL) { krb5_free_context (context); }
    
    return changed;
}

#pragma mark -

static KLStatus __KLFindTranslator (const char               *inTranslatorName,
                                    CFBundleRef              *outBundle,
                                    KLPT_TranslatePrincipal  *outTranslate,
                                    KLPT_ReleasePrincipal    *outRelease)
{
    KLStatus              err = klNoErr;
    char                  bundleName[MAXPATHLEN];
    CFStringRef           translatorString = NULL;
    CFURLRef              translatorURL = NULL;
    CFBundleRef           translatorBundle = NULL;
    KLBoolean             bundleLoaded = false;
    KLPT_InitializePlugin pluginInitFunction = NULL;

    /* Check parameters */
    if (inTranslatorName == NULL) { err = KLError_ (klParameterErr); }
    if (outBundle        == NULL) { err = KLError_ (klParameterErr); }
    if (outTranslate     == NULL) { err = KLError_ (klParameterErr); }
    if (outRelease       == NULL) { err = KLError_ (klParameterErr); }

    if (err == klNoErr) {
        int realLength = snprintf (bundleName, MAXPATHLEN, "%s%s%s", kKLPT_PluginBundleFolder, inTranslatorName, kKLPT_PluginBundleNameSuffix);
        if (MAXPATHLEN < realLength) {
            err = KLError_ (klParameterErr);
        }
    }
    
    if (err == klNoErr) {
        translatorString = CFStringCreateWithCString (kCFAllocatorDefault, bundleName, kCFStringEncodingASCII);
        if (translatorString == NULL) { err = KLError_ (klMemFullErr); }
    }

    if (err == klNoErr) {
        translatorURL = CFURLCreateWithFileSystemPath (kCFAllocatorDefault, translatorString, kCFURLPOSIXPathStyle, true);
        if (translatorURL == NULL) { err = KLError_ (klMemFullErr); }
    }
    
    if (err == klNoErr) {
        translatorBundle = CFBundleCreate (kCFAllocatorDefault, translatorURL);
        if (translatorBundle == NULL) { err = KLError_ (klMemFullErr); }
    }

#if MACDEV_DEBUG
    if (err == klNoErr) {
        CFStringRef bundleIDString = CFBundleGetIdentifier (translatorBundle);
        if (bundleIDString != NULL) {
            char id[256];
            if (CFStringGetCString (bundleIDString, id, sizeof (id), kCFStringEncodingASCII)) {
                dprintf ("KLFindTranslator: bundle id for '%s' is '%s'\n", bundleName, id);
            } else {
                dprintf ("KLFindTranslator: CFStringGetCString (idString) failed\n");
            }
            CFRelease (bundleIDString);
        } else {
            dprintf ("KLFindTranslator: CFBundleGetIdentifier (translatorBundle) failed\n");
        }
    }
#endif
    
    if (err == klNoErr) {
        dprintf ("KLFindTranslator: Loading '%s'...\n", bundleName);

        bundleLoaded = CFBundleIsExecutableLoaded (translatorBundle);
        if (bundleLoaded == false) {
            bundleLoaded = CFBundleLoadExecutable (translatorBundle);
            if (bundleLoaded == false) {
                dprintf ("KLFindTranslator: CFBundleLoadExecutable () failed, check your bundle settings (esp. \"Executable\")\n");
            }
        } else {
            dprintf ("KLFindTranslator: Translator already loaded?!\n");
        }
        if (bundleLoaded == false) { err = KLError_ (klParameterErr); }
    }
    
    if (err == klNoErr) {
        pluginInitFunction = (KLPT_InitializePlugin) CFBundleGetFunctionPointerForName (translatorBundle, kKLPT_InitializePlugin);
        if (pluginInitFunction == NULL) { err = KLError_ (klParameterErr); }
    }
    
    if (err == klNoErr) {
        __KLBeginPlugin ();
        err = pluginInitFunction (kKLPT_APIVersion_Current);
        __KLEndPlugin ();
    }
    
    if (err == klNoErr) {
        *outTranslate = (KLPT_TranslatePrincipal) CFBundleGetFunctionPointerForName (translatorBundle, kKLPT_TranslatePrincipal);
        if (*outTranslate == NULL) { err = KLError_ (klParameterErr); }
    }

    if (err == klNoErr) {
        *outRelease = (KLPT_ReleasePrincipal) CFBundleGetFunctionPointerForName (translatorBundle, kKLPT_ReleasePrincipal);
        if (*outRelease == NULL) { err = KLError_ (klParameterErr); }
    }
    
    if (translatorString != NULL)
        CFRelease (translatorString);
        
    if (translatorURL != NULL)
        CFRelease (translatorURL);
    
    if (err == klNoErr) {
        *outBundle = translatorBundle;
    } else {
        if (bundleLoaded            ) { CFBundleUnloadExecutable (translatorBundle); }
        if (translatorBundle != NULL) { CFRelease (translatorBundle); }
    }
    
    return KLError_ (err);
}

static KLStatus __KLCallTranslator (KLPT_TranslatePrincipal inTranslator,
                                    KLPT_ReleasePrincipal   inRelease,
                                    const char             *inName,
                                    const char             *inInstance,
                                    const char             *inRealm,
                                    char                  **outName,
                                    char                  **outInstance,
                                    char                  **outRealm,
                                    KLBoolean              *outChanged)
{
    KLStatus err = klNoErr;		
    char *newName = NULL;
    char *newInstance = NULL;
    char *newRealm = NULL;
    KLBoolean   changed = false;

    if (inTranslator == NULL) { err = KLError_ (klParameterErr); }
    if (inRelease == NULL)    { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        err = inTranslator (inName, inInstance, inRealm, &newName, &newInstance, &newRealm, &changed);
    }

#if MACDEV_DEBUG
    if (err == klNoErr) {
        char *checkNewName = NULL;
        char *checkNewInstance = NULL;
        char *checkNewRealm = NULL;
        KLBoolean   checkChanged = false;

        __KLBeginPlugin ();
        KLStatus checkErr = inTranslator (newName, newName, newRealm, &checkNewName, &checkNewInstance, &checkNewRealm, &checkChanged);
        __KLEndPlugin ();
            
        if (checkErr == klNoErr) {
            if (checkChanged) {
                if ((strcmp (newName, checkNewName) != 0) || 
                    (strcmp (newInstance, checkNewInstance) != 0) ||
                    (strcmp (newRealm, checkNewRealm) != 0)) {
                    dprintf ("KLCallTranslator: Principal translation plugin failed idempotency test.");
                    
                    // Force translation failure if check fails
                    err = KLError_ (klBadPrincipalErr);
                    
                    *outChanged = false;
                }
            }
        }

        if ((inRelease != NULL) && (checkNewName != NULL) && (checkNewInstance != NULL) && (checkNewRealm != NULL)) {
            inRelease (checkNewName, checkNewInstance, checkNewRealm);
        }
    }
#endif // MACDEV_DEBUG
	
    // No matter what bogus thing the translator does, try to pass something reasonable back
    if (changed && (newName != NULL) && (newInstance != NULL) && (newRealm != NULL)) {
        char *name = NULL;
        char *instance = NULL;
        char *realm = NULL;
        
        if (err == klNoErr) {
            err = __KLCreateString (newName, &name);
        }
        if (err == klNoErr) {
            err = __KLCreateString (newInstance, &instance);
        }
        if (err == klNoErr) {
            err = __KLCreateString (newRealm, &realm);
        }
        
        if (err == klNoErr) {
            *outName = name;
            *outInstance = instance;
            *outRealm = realm;
            *outChanged = true;
        }
    } else {
        *outChanged = false;
    }
    
    if ((inRelease != NULL) && (newName != NULL) && (newInstance != NULL) && (newRealm != NULL)) {
        inRelease (newName, newInstance, newRealm);
    }

    return KLError_ (err);
}
