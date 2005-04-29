/*
 * KLLoginLogoutNotifier.c
 *
 * $Header: /cvs/kfm/KerberosFramework/KerberosLogin/Sources/KerberosLogin/KLLoginLogoutNotifier.c,v 1.10 2004/12/10 21:22:29 lxs Exp $
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

#define kKLN_Login            CFSTR ("KerberosLoginNotification_Login")
#define kKLN_Logout           CFSTR ("KerberosLoginNotification_Logout")
#define kKLN_InitializePlugin CFSTR ("KerberosLoginNotification_InitializePlugin")
 
const KLN_APIVersion kKLN_APIVersionCurrent = 1;

typedef KLStatus (*KLN_InitializePlugin) (KLN_APIVersion inAPIVersion);

typedef KLStatus (*KLN_Login) (KLN_LoginType inLoginType, const char* inCCacheName);

typedef void (*KLN_Logout) (const char* inCCacheName);
 
static KLStatus __KLFindLoginLogoutPlugin (const char               *inPluginName,
                                           CFBundleRef              *outBundle,
                                           KLN_Login                *outLogin,
                                           KLN_Logout               *outLogout);

#pragma mark -
	
KLStatus __KLCallLoginPlugin (KLN_LoginType inLoginType, const char *inCCacheName)
{
    KLStatus     err = klNoErr;
    KLStatus     result = klNoErr;    
    krb5_context context = NULL;
    profile_t    profile = NULL;
    const char  *libdefaultList[3] = { "libdefaults", "login_logout_notification", NULL };
    char       **values = NULL;

    if (inCCacheName == NULL) { err = KLError_ (klParameterErr); }

    if (err == klNoErr) {
        err = krb5_init_context (&context);
    }
    
    if (err == klNoErr) {
        err = krb5_get_profile (context, &profile);
    }
    
    if (err == klNoErr) {
        err = profile_get_values (profile, libdefaultList, &values);
    }
        
    if (err == klNoErr) {
        char **plugin;
        
        for (plugin = values; *plugin != NULL; plugin++) {
            CFBundleRef pluginBundle = NULL;
            KLN_Login   loginCallback = NULL;
            KLN_Logout  logoutCallback = NULL;
                
            if (err == klNoErr) {
                err = __KLFindLoginLogoutPlugin (*plugin, &pluginBundle, &loginCallback, &logoutCallback);
            }
            
            if (err == klNoErr) {
                __KLBeginPlugin ();
                result = loginCallback (inLoginType, inCCacheName);
                __KLEndPlugin ();
            }
            
            if (pluginBundle != NULL) {
                CFBundleUnloadExecutable (pluginBundle);
                CFRelease (pluginBundle);
            }
            
            if (err != klNoErr) { break; }
        }
    }

    if (values  != NULL) { profile_free_list (values); }
    if (profile != NULL) { profile_abandon (profile); }
    if (context != NULL) { krb5_free_context (context); }
    
    return result;
}

void __KLCallLogoutPlugin (const char *inCCacheName)
{
    KLStatus     err = klNoErr;    
    krb5_context context = NULL;
    profile_t    profile = NULL;
    const char  *libdefaultList[3] = { "libdefaults", "login_logout_notification", NULL };
    char       **values = NULL;

    if (inCCacheName == NULL) { err = KLError_ (klParameterErr); }

    if (err == klNoErr) {
        err = krb5_init_context (&context);
    }
    
    if (err == klNoErr) {
        err = krb5_get_profile (context, &profile);
    }
    
    if (err == klNoErr) {
        err = profile_get_values (profile, libdefaultList, &values);
    }
        
    if (err == klNoErr) {
        char **plugin;
        
        for (plugin = values; *plugin != NULL; plugin++) {
            CFBundleRef pluginBundle = NULL;
            KLN_Login   loginCallback = NULL;
            KLN_Logout  logoutCallback = NULL;
                
            if (err == klNoErr) {
                err = __KLFindLoginLogoutPlugin (*plugin, &pluginBundle, &loginCallback, &logoutCallback);
            }
            
            if (err == klNoErr) {
                __KLBeginPlugin ();
                logoutCallback (inCCacheName);
                __KLEndPlugin ();
            }
            
            if (pluginBundle != NULL) {
                CFBundleUnloadExecutable (pluginBundle);
                CFRelease (pluginBundle);
            }
            
            if (err != klNoErr) { break; }
        }
    }

    if (values  != NULL) { profile_free_list (values); }
    if (profile != NULL) { profile_abandon (profile); }
    if (context != NULL) { krb5_free_context (context); }
}

#pragma mark -

static KLStatus __KLFindLoginLogoutPlugin (const char               *inPluginName,
                                           CFBundleRef              *outBundle,
                                           KLN_Login                *outLogin,
                                           KLN_Logout               *outLogout)
{
    KLStatus              err = klNoErr;
    char                  bundleName[MAXPATHLEN];
    CFStringRef           pluginString = NULL;
    CFURLRef              pluginURL = NULL;
    CFBundleRef           pluginBundle = NULL;
    KLBoolean             bundleLoaded = false;
    KLN_InitializePlugin  pluginInitFunction = NULL;

    /* Check parameters */
    if (inPluginName == NULL) { err = KLError_ (klParameterErr); }
    if (outBundle    == NULL) { err = KLError_ (klParameterErr); }
    if (outLogin     == NULL) { err = KLError_ (klParameterErr); }
    if (outLogout    == NULL) { err = KLError_ (klParameterErr); }

    if (err == klNoErr) {
        int realLength = snprintf (bundleName, MAXPATHLEN, "%s%s%s", kKLN_PluginBundleFolder, inPluginName, kKLN_PluginBundleNameSuffix);
        if (MAXPATHLEN < realLength) {
            err = KLError_ (klParameterErr);
        }
    }

    
    if (err == klNoErr) {
        pluginString = CFStringCreateWithCString (kCFAllocatorDefault, bundleName, kCFStringEncodingASCII);
        if (pluginString == NULL) { err = KLError_ (klMemFullErr); }
    }

    if (err == klNoErr) {
        pluginURL = CFURLCreateWithFileSystemPath (kCFAllocatorDefault, pluginString, kCFURLPOSIXPathStyle, true);
        if (pluginURL == NULL) { err = KLError_ (klMemFullErr); }
    }
    
    if (err == klNoErr) {
        pluginBundle = CFBundleCreate (kCFAllocatorDefault, pluginURL);
        if (pluginBundle == NULL) { err = KLError_ (klMemFullErr); }
    }

    if ((err == klNoErr) && (ddebuglevel () > 0)) {
        CFStringRef bundleIDString = CFBundleGetIdentifier (pluginBundle);
        if (bundleIDString != NULL) {
            char id[256];
            if (CFStringGetCString (bundleIDString, id, sizeof (id), kCFStringEncodingUTF8)) {
                dprintf ("KLFindLoginLogoutPlugin: bundle id for '%s' is '%s'\n", bundleName, id);
            } else {
                dprintf ("KLFindLoginLogoutPlugin: CFStringGetCString (idString) failed\n");
            }
        } else {
            dprintf ("KLFindLoginLogoutPlugin: CFBundleGetIdentifier (translatorBundle) failed\n");
        }
    }
    
    if (err == klNoErr) {
        dprintf ("KLFindTranslator: Loading '%s'...\n", bundleName);

        bundleLoaded = CFBundleIsExecutableLoaded (pluginBundle);
        if (bundleLoaded == false) {
            bundleLoaded = CFBundleLoadExecutable (pluginBundle);
            if (bundleLoaded == false)
                dprintf ("KLFindLoginLogoutPlugin: CFBundleLoadExecutable () failed, "
                        "check your bundle settings (esp. \"Executable\")\n");
        } else {
            dprintf ("KLFindLoginLogoutPlugin: Translator already loaded?!\n");
        }
        if (bundleLoaded == false) { err = KLError_ (klParameterErr); }
    }
    
    if (err == klNoErr) {
        pluginInitFunction = (KLN_InitializePlugin) CFBundleGetFunctionPointerForName ( pluginBundle, kKLN_InitializePlugin);
        if (pluginInitFunction == NULL) { err = KLError_ (klParameterErr); }
    }
    
    if (err == klNoErr) {
        err = pluginInitFunction (kKLN_APIVersion_Current);
    }
    
    if (err == klNoErr) {
        *outLogin = (KLN_Login) CFBundleGetFunctionPointerForName (pluginBundle, kKLN_Login);
        if (*outLogin == NULL) { err = KLError_ (klParameterErr); }
    }

    if (err == klNoErr) {
        *outLogout = (KLN_Logout) CFBundleGetFunctionPointerForName (pluginBundle, kKLN_Logout);
        if (*outLogout == NULL) { err = KLError_ (klParameterErr); }
    }

    if (pluginString != NULL) { CFRelease (pluginString); }
    if (pluginURL    != NULL) { CFRelease (pluginURL); }
    
    if (err == klNoErr) {
        *outBundle = pluginBundle;
    } else {
        if (bundleLoaded        ) { CFBundleUnloadExecutable (pluginBundle); }
        if (pluginBundle != NULL) { CFRelease (pluginBundle); }
    }
    
    return KLError_ (err);
}
