/*
 * $Header$
 *
 * Copyright 2006 Massachusetts Institute of Technology.
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

#include <Security/Authorization.h>
#include <CoreFoundation/CoreFoundation.h>

#include <string.h>
#include <syslog.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <stdio.h>

#include <dispatch/dispatch.h>

#include "heim.h"
#include "mit-KerberosLogin.h"
#include "mit-CredentialsCache.h"

struct KLLoginOptions {
    krb5_get_init_creds_opt *opt;
    char *service;
};

krb5_context milcontext;

void
mshim_init_context(void)
{
    static dispatch_once_t once = 0;
    dispatch_once(&once, ^{
	    heim_krb5_init_context(&milcontext);
	});
}

/* 
 * Deprecated Error codes 
 */
enum {
    /* Carbon Dialog errors */
    klDialogDoesNotExistErr             = 19676,
    klDialogAlreadyExistsErr,
    klNotInForegroundErr,
    klNoAppearanceErr,
    klFatalDialogErr,
    klCarbonUnavailableErr    
};

krb5_get_init_creds_opt *__KLLoginOptionsGetKerberos5Options (KLLoginOptions ioOptions);
KLTime __KLLoginOptionsGetStartTime (KLLoginOptions ioOptions);
char *__KLLoginOptionsGetServiceName (KLLoginOptions ioOptions);

#define CHECK_VERSION(_vers) ((_vers) != kerberosVersion_V5 && (_vers) != kerberosVersion_All)


KLStatus
KLAcquireTickets (KLPrincipal   inPrincipal,
		  KLPrincipal  *outPrincipal,
		  char        **outCredCacheName)
{
    LOG_ENTRY();
    return KLAcquireInitialTickets (inPrincipal, 
				    NULL, 
				    outPrincipal, 
				    outCredCacheName);
}


KLStatus
KLAcquireNewTickets (KLPrincipal  inPrincipal,
		     KLPrincipal  *outPrincipal,
		     char        **outCredCacheName)
{
    LOG_ENTRY();
    return KLAcquireNewInitialTickets (inPrincipal, 
				       NULL, 
				       outPrincipal, 
				       outCredCacheName);
}


KLStatus
KLAcquireTicketsWithPassword (KLPrincipal      inPrincipal,
			      KLLoginOptions   inLoginOptions,
			      const char      *inPassword,
			      char           **outCredCacheName)
{
    LOG_ENTRY();
    return KLAcquireInitialTicketsWithPassword (inPrincipal, 
						inLoginOptions, 
						inPassword, 
						outCredCacheName);
}


KLStatus
KLAcquireNewTicketsWithPassword (KLPrincipal      inPrincipal,
				 KLLoginOptions   inLoginOptions,
				 const char      *inPassword,
				 char           **outCredCacheName)
{
    LOG_ENTRY();
    return KLAcquireNewInitialTicketsWithPassword (inPrincipal, 
						   inLoginOptions, 
						   inPassword, 
						   outCredCacheName);
}


KLStatus
KLSetApplicationOptions (const void *inAppOptions)
{
    return klNoErr;
}


KLStatus
KLGetApplicationOptions (void *outAppOptions)
{
    return klDialogDoesNotExistErr;
}

#define kCoreAuthPanelKerberosRight "com.apple.KerberosAgent"
#define kCoreAuthPanelKerberosPrincipal "principal"
#define kCoreAuthPanelKerberosOptions "kerberosOptions"

static OSStatus
acquireticket_ui(KLPrincipal inPrincipal,
		 KLLoginOptions inLoginOptions,
		 KLPrincipal *outPrincipal,
		 char **outCredCacheName)
{
    AuthorizationRef auth;
    OSStatus ret;
    char *princ = NULL;
    CFDataRef d = NULL;
    
    LOG_ENTRY();

    mshim_init_context();

    if (outPrincipal)
	*outPrincipal = NULL;
    if (outCredCacheName)
	*outCredCacheName = NULL;

    if (inPrincipal) {
	ret = heim_krb5_unparse_name(milcontext, inPrincipal, &princ);
	if (ret)
	    return ret;
    }
    

    ret = AuthorizationCreate(NULL, NULL, kAuthorizationFlagDefaults, &auth);
    if (ret) {
	free(princ);
	return ret;
    }
    
    AuthorizationItem rightItems[1] = { kCoreAuthPanelKerberosRight, 0, NULL, 0 };
    AuthorizationRights rights = { sizeof(rightItems[0])/sizeof(rightItems) , rightItems };
    AuthorizationItem envItems[3];
    AuthorizationEnvironment env = { 0 , envItems };
    AuthorizationFlags authFlags = kAuthorizationFlagInteractionAllowed | kAuthorizationFlagExtendRights;

    if (princ) {
	envItems[env.count].name = kCoreAuthPanelKerberosPrincipal;
	envItems[env.count].valueLength = strlen(princ);
	envItems[env.count].value = princ;
	envItems[env.count].flags = 0;
	env.count++;
    }

    if (inLoginOptions && inLoginOptions->opt) {
	CFMutableDictionaryRef dict;

	dict = CFDictionaryCreateMutable(NULL, 1,
					 &kCFTypeDictionaryKeyCallBacks,
					 &kCFTypeDictionaryValueCallBacks);
	if (dict == NULL)
	    goto out;
	
	if (inLoginOptions->opt->renew_life) {
	    CFStringRef t;
	    t = CFStringCreateWithFormat(NULL, 0, CFSTR("%ld"),(long)inLoginOptions->opt->renew_life);
	    CFDictionarySetValue(dict, CFSTR("renewTime"), t);
	    CFRelease(t);
	}

	d = CFPropertyListCreateData(NULL, dict, kCFPropertyListBinaryFormat_v1_0,
				     0, NULL);
	CFRelease(dict);

	envItems[env.count].name = kCoreAuthPanelKerberosOptions;
	envItems[env.count].valueLength = CFDataGetLength(d);
	envItems[env.count].value = (void *)CFDataGetBytePtr(d);
	envItems[env.count].flags = 0;
	env.count++;
    }
    
    ret = AuthorizationCopyRights(auth, &rights, &env, authFlags, NULL);

    if (ret == 0 && outPrincipal) {
	AuthorizationItemSet *info;
	UInt32 i;
	ret = AuthorizationCopyInfo(auth, NULL, &info);
	if (ret)
	    goto out;
	for(i = 0; i < info->count; i++) {
	    if (strcmp(info->items[i].name, "out-principal") == 0) {
		char *str;
		asprintf(&str, "%.*s", (int)info->items[i].valueLength, (char *)info->items[i].value);
		heim_krb5_parse_name(milcontext, str, outPrincipal);
	    } else if (strcmp(info->items[i].name, "out-cache-name") == 0) {
		asprintf(outCredCacheName, "%.*s", (int)info->items[i].valueLength, (char *)info->items[i].value);
	    }
	}
	AuthorizationFreeItemSet(info);
	if (*outPrincipal == NULL)
	    ret = EINVAL;
    }
out:
    if (d)
	CFRelease(d);
    AuthorizationFree(auth, kAuthorizationFlagDestroyRights);
    free(princ);
    return ret;
}

KLStatus
KLAcquireInitialTickets(KLPrincipal      inPrincipal,
			KLLoginOptions   inLoginOptions,
			KLPrincipal     *outPrincipal,
			char           **outCredCacheName)
{
    KLBoolean ValidTickets;
    KLStatus ret;

    LOG_ENTRY();

    ret = KLCacheHasValidTickets(inPrincipal,
				 kerberosVersion_V5,
				 &ValidTickets,
				 outPrincipal,
				 outCredCacheName);
    if (ret || !ValidTickets)
	ret = acquireticket_ui(inPrincipal, inLoginOptions, outPrincipal, outCredCacheName);

    return ret;
}


KLStatus
KLAcquireNewInitialTickets (KLPrincipal      inPrincipal,
			    KLLoginOptions   inLoginOptions,
			    KLPrincipal     *outPrincipal,
			    char           **outCredCacheName)
{
    LOG_ENTRY();

    return acquireticket_ui(inPrincipal, inLoginOptions, outPrincipal, outCredCacheName);
}


KLStatus
KLDestroyTickets(KLPrincipal inPrincipal)
{
    krb5_error_code ret;
    krb5_ccache id;

    mshim_init_context();

    ret = heim_krb5_cc_cache_match(milcontext, inPrincipal, &id);
    if (ret)
	return ret;

    return krb5_cc_destroy((mit_krb5_context)milcontext, (mit_krb5_ccache)id);
}


KLStatus KLChangePassword (KLPrincipal inPrincipal)
{
    LOG_UNIMPLEMENTED();
    return EINVAL;
}


KLStatus
KLAcquireInitialTicketsWithPassword(KLPrincipal      inPrincipal,
				    KLLoginOptions   inLoginOptions,
				    const char      *inPassword,
				    char           **outCredCacheName)
{
    KLStatus ret;
    KLBoolean ValidTickets;

    ret = KLCacheHasValidTickets(inPrincipal,
				 kerberosVersion_V5,
				 &ValidTickets,
				 NULL,
				 outCredCacheName);
    if (ret == 0) {
	if (ValidTickets)
	    return klNoErr; /* done */
	/* get credential */
	if (outCredCacheName)
	    free(*outCredCacheName);
    }
    return KLAcquireNewInitialTicketsWithPassword(inPrincipal, 
						  inLoginOptions,
						  inPassword,
						  outCredCacheName);
}

KLStatus
KLAcquireNewInitialTicketsWithPassword(KLPrincipal      inPrincipal,
				       KLLoginOptions   inLoginOptions,
				       const char      *inPassword,
				       char           **outCredCacheName)
{
    krb5_error_code ret;
    krb5_ccache cache;
    krb5_creds creds;
    char *service = NULL;
    krb5_get_init_creds_opt *opt = NULL;

    LOG_ENTRY();

    mshim_init_context();

    if (inLoginOptions) {
	service = inLoginOptions->service;
	opt = inLoginOptions->opt;
    }

    ret = heim_krb5_get_init_creds_password(milcontext, &creds,
					    inPrincipal, inPassword,
					    NULL, NULL, 0,
					    service,
					    opt);
    if (ret)
	return ret;

    ret = heim_krb5_cc_cache_match(milcontext, inPrincipal, &cache);
    if (ret)
	ret = heim_krb5_cc_new_unique(milcontext, NULL, NULL, &cache);
    if (ret)
	goto out;
	
    ret = heim_krb5_cc_initialize(milcontext, cache, creds.client);
    if(ret)
	goto out;

    ret = heim_krb5_cc_store_cred(milcontext, cache, &creds);
    if (ret)
	goto out;

    if (outCredCacheName)
	*outCredCacheName = strdup(heim_krb5_cc_get_name(milcontext, cache));

 out:
    if (cache) {
	if (ret)
	    krb5_cc_destroy((mit_krb5_context)milcontext, (mit_krb5_ccache)cache);
	else
	    heim_krb5_cc_close(milcontext, cache);
    }
    heim_krb5_free_cred_contents(milcontext, &creds);

    return ret;
}


KLStatus KLAcquireNewInitialTicketCredentialsWithPassword (KLPrincipal      inPrincipal,
                                                           KLLoginOptions   inLoginOptions,
                                                           const char      *inPassword,
                                                           void            *inV5Context,
                                                           KLBoolean       *outGotV4Credentials,
                                                           KLBoolean       *outGotV5Credentials,
                                                           void            *outV4Credentials,
                                                           void            *outV5Credentials)
{
    LOG_UNIMPLEMENTED();
    return EINVAL;
}


KLStatus KLStoreNewInitialTicketCredentials (KLPrincipal     inPrincipal,
                                             void           *inV5Context,
                                             void           *inV4Credentials,
                                             void           *inV5Credentials,
                                             char          **outCredCacheName)
{
    LOG_UNIMPLEMENTED();
    return EINVAL;
}


KLStatus KLVerifyInitialTickets (KLPrincipal   inPrincipal,
                                 KLBoolean     inFailIfNoHostKey,
                                 char        **outCredCacheName)
{
    LOG_UNIMPLEMENTED();
    return klNoErr;
}


KLStatus KLVerifyInitialTicketCredentials (void        *inV4Credentials,
                                           void        *inV5Credentials,
                                           KLBoolean    inFailIfNoHostKey)
{
    LOG_UNIMPLEMENTED();
    return klNoErr;
}


KLStatus KLAcquireNewInitialTicketsWithKeytab (KLPrincipal      inPrincipal,
                                               KLLoginOptions   inLoginOptions,
                                               const char      *inKeytabName,
                                               char           **outCredCacheName)
{
    LOG_UNIMPLEMENTED();
    return EINVAL;
}


KLStatus
KLRenewInitialTickets(KLPrincipal      inPrincipal,
		      KLLoginOptions   inLoginOptions,
		      KLPrincipal     *outPrincipal,
		      char           **outCredCacheName)
{
    krb5_error_code ret;
    krb5_creds in, *cred = NULL;
    krb5_ccache id;
    krb5_kdc_flags flags;
    krb5_const_realm realm;

    memset(&in, 0, sizeof(in));

    LOG_ENTRY();

    mshim_init_context();

    if (outPrincipal)
	*outPrincipal = NULL;
    if (outCredCacheName)
	*outCredCacheName = NULL;

    ret = heim_krb5_cc_cache_match(milcontext, inPrincipal, &id);
    if (ret)
	return ret; /* XXX */

    in.client = inPrincipal;

    realm = heim_krb5_principal_get_realm(milcontext, in.client);

    if (inLoginOptions && inLoginOptions->service)
	ret = heim_krb5_make_principal(milcontext, &in.server, realm, inLoginOptions->service, NULL);
    else
	ret = heim_krb5_make_principal(milcontext, &in.server, realm, KRB5_TGS_NAME, realm, NULL);
    if (ret) {
	heim_krb5_cc_close(milcontext, id);
	return ret;
    }

    flags.i = 0;
    if (inLoginOptions)
	flags.i = inLoginOptions->opt->flags;

    /* Pull out renewable from previous ticket */
    ret = heim_krb5_get_credentials(milcontext, KRB5_GC_CACHED, id, &in, &cred);
    if (ret == 0 && cred) {
	flags.b.renewable = cred->flags.b.renewable;
	heim_krb5_free_creds (milcontext, cred);
	cred = NULL;
    }

    flags.b.renew = 1;

    ret = heim_krb5_get_kdc_cred(milcontext, id, flags, NULL, NULL, &in, &cred);
    heim_krb5_free_principal(milcontext, in.server);
    if (ret)
	goto out;
    ret = heim_krb5_cc_initialize(milcontext, id, in.client);
    if (ret)
	goto out;
    ret = heim_krb5_cc_store_cred(milcontext, id, cred);

 out:
    if (cred)
	heim_krb5_free_creds (milcontext, cred);
    heim_krb5_cc_close(milcontext, id);

    return ret;
}


KLStatus KLValidateInitialTickets (KLPrincipal      inPrincipal,
                                   KLLoginOptions   inLoginOptions,
                                   char           **outCredCacheName)
{
    LOG_UNIMPLEMENTED();
    return klNoErr;
}

static krb5_timestamp g_cc_change_time = 0;
static KLTime g_kl_change_time = 0;
static pthread_mutex_t g_change_time_mutex = PTHREAD_MUTEX_INITIALIZER;


KLStatus
KLLastChangedTime(KLTime *outLastChangedTime)
{
    krb5_timestamp ccChangeTime;
    KLStatus ret;
    
    LOG_ENTRY();

    mshim_init_context();

    if (outLastChangedTime == NULL)
	return klParameterErr;

    ret = heim_krb5_cccol_last_change_time(milcontext, "API", &ccChangeTime);
    if (ret)
	return ret;
	
    pthread_mutex_lock(&g_change_time_mutex);

    if (g_kl_change_time == 0)
	g_kl_change_time = ccChangeTime;
    
    if (ccChangeTime > g_cc_change_time) {

	if (ccChangeTime > g_kl_change_time)
	    g_kl_change_time = ccChangeTime;
	else
	    g_kl_change_time++;

	g_cc_change_time = ccChangeTime;
    }

    *outLastChangedTime = g_kl_change_time;

    pthread_mutex_unlock (&g_change_time_mutex);
    
    return klNoErr;
}


static krb5_error_code
fetch_creds(KLPrincipal inPrincipal, krb5_creds **ocreds,
	    char **outCredCacheName)
{	    
    krb5_principal princ = NULL;
    krb5_creds in_creds;
    krb5_const_realm realm;
    krb5_error_code ret;
    krb5_ccache id = NULL;

    LOG_ENTRY();

    mshim_init_context();

    memset(&in_creds, 0, sizeof(in_creds));

    if (inPrincipal) {
	ret = heim_krb5_cc_cache_match(milcontext, inPrincipal, &id);
    } else {
	ret = heim_krb5_cc_default(milcontext, &id);
	if (ret == 0)
	    ret = heim_krb5_cc_get_principal(milcontext, id, &princ);
	inPrincipal = princ;
    }
    if (ret)
	goto out;

    realm = heim_krb5_principal_get_realm(milcontext, inPrincipal);
    ret = heim_krb5_make_principal(milcontext, &in_creds.server, realm, KRB5_TGS_NAME, realm, NULL);
    if (ret)
	goto out;

    in_creds.client = inPrincipal;

    ret = heim_krb5_get_credentials(milcontext, KRB5_GC_CACHED, id,
				    &in_creds, ocreds);
    heim_krb5_free_principal(milcontext, in_creds.server);

    if (outCredCacheName)
	*outCredCacheName = strdup(heim_krb5_cc_get_name(milcontext, id));

 out:
    if (id)
	heim_krb5_cc_close(milcontext, id);
    if (princ)
	heim_krb5_free_principal(milcontext, princ);

    return LOG_FAILURE(ret, "fetch_creds");
}


KLStatus KLCacheHasValidTickets (KLPrincipal         inPrincipal,
                                 KLKerberosVersion   inKerberosVersion,
                                 KLBoolean          *outFoundValidTickets,
                                 KLPrincipal        *outPrincipal,
                                 char              **outCredCacheName)
{
    krb5_error_code ret;
    krb5_creds *ocreds;

    LOG_ENTRY();

    mshim_init_context();

    if (CHECK_VERSION(inKerberosVersion))
	return LOG_FAILURE(klInvalidVersionErr, "wrong version");

    ret = fetch_creds(inPrincipal, &ocreds, outCredCacheName);

    if (ret == 0) {
	time_t t = time(NULL);
	/* consinder tickets that are slightly too young as valid
	 * since might just have fetched them */
	*outFoundValidTickets =
	       (ocreds->times.starttime - 10 < t)
	    && (t < ocreds->times.endtime);
	heim_krb5_free_creds(milcontext, ocreds);
    } else {
	LOG_FAILURE(ret, "fetch tickets failed");
	ret = 0;
	*outFoundValidTickets = 0;
    }

    if (outPrincipal)
	*outPrincipal = NULL;

    return LOG_FAILURE(ret, "KLCacheHasValidTickets");
}


KLStatus KLTicketStartTime (KLPrincipal        inPrincipal,
                            KLKerberosVersion  inKerberosVersion,
                            KLTime            *outStartTime)
{
    krb5_creds *creds;
    krb5_error_code ret;

    LOG_ENTRY();

    mshim_init_context();
    
    if (CHECK_VERSION(inKerberosVersion))
	return LOG_FAILURE(klInvalidVersionErr, "wrong version");

    ret = fetch_creds(inPrincipal, &creds, NULL);
    if (ret)
	return LOG_FAILURE(ret, "fetch tickets failed");

    *outStartTime = creds->times.starttime;

    heim_krb5_free_creds(milcontext, creds);
    return klNoErr;
}


KLStatus KLTicketExpirationTime (KLPrincipal        inPrincipal,
                                 KLKerberosVersion  inKerberosVersion,
                                 KLTime            *outExpirationTime)
{
    krb5_error_code ret;
    krb5_creds *creds;

    LOG_ENTRY();

    mshim_init_context();

    if (CHECK_VERSION(inKerberosVersion))
	return LOG_FAILURE(klInvalidVersionErr, "wrong version");
    
    ret = fetch_creds(inPrincipal, &creds, NULL);
    if (ret)
	return LOG_FAILURE(ret, "fetch tickets failed");

    *outExpirationTime = creds->times.endtime;

    heim_krb5_free_creds(milcontext, creds);
    return klNoErr;
}


KLStatus
KLSetSystemDefaultCache (KLPrincipal inPrincipal)
{
    krb5_error_code ret;
    krb5_ccache id;

    LOG_ENTRY();

    mshim_init_context();

    ret = heim_krb5_cc_cache_match(milcontext, inPrincipal, &id);
    if (ret)
	return LOG_FAILURE(ret, "ccache match");
    ret = heim_krb5_cc_switch(milcontext, id);
    heim_krb5_cc_close(milcontext, id);
    if (ret)
	return LOG_FAILURE(ret, "cc switch");
    return klNoErr;
}


KLStatus KLHandleError (KLStatus           inError,
                        KLDialogIdentifier inDialogIdentifier,
                        KLBoolean          inShowAlert)
{
    LOG_UNIMPLEMENTED();
    return klNoErr;
}


KLStatus KLGetErrorString (KLStatus   inError,
                           char     **outErrorString)
{
    LOG_ENTRY();

    mshim_init_context();

    *outErrorString = heim_krb5_get_error_string(milcontext);
    if (*outErrorString == NULL)
	asprintf(outErrorString, "unknown error: %d\n", (int)inError);
    if (outErrorString == NULL)
	return klMemFullErr;
    return klNoErr;
}


KLStatus KLCancelAllDialogs (void)
{
    return klNoErr;
}


/* Kerberos change password dialog low level functions */

KLStatus KLChangePasswordWithPasswords (KLPrincipal   inPrincipal,
                                        const char   *inOldPassword,
                                        const char   *inNewPassword,
                                        KLBoolean    *outRejected,
                                        char        **outRejectionError,
                                        char        **outRejectionDescription)
{
    LOG_UNIMPLEMENTED();
    return EINVAL;
}


/* Application Configuration functions */

KLStatus KLSetIdleCallback (const KLIdleCallback inCallback,
                            const KLRefCon inRefCon)
{
    return klNoErr;
}


KLStatus KLGetIdleCallback (KLIdleCallback* inCallback,
                            KLRefCon* inRefCon)
{
    return klNoErr;
}


/* Library configuration functions */
/* Deprecated options which we now ignore */
enum {
    loginOption_ShowOptions                = 'sopt',
    loginOption_RememberShowOptions        = 'ropt',
    loginOption_LongTicketLifetimeDisplay  = 'hms ',
    loginOption_RememberPassword           = 'pass'
};



KLStatus KLGetDefaultLoginOption (const KLDefaultLoginOption  inOption,
                                  void                       *ioBuffer,
                                  KLSize                     *ioBufferSize)
{
    LOG_UNIMPLEMENTED();
    return EINVAL;
}


KLStatus KLSetDefaultLoginOption (const KLDefaultLoginOption  inOption,
                                  const void                 *inBuffer,
                                  const KLSize                inBufferSize)
{
    LOG_UNIMPLEMENTED();
    return EINVAL;
}


/* Realm configuration functions */

KLStatus KLFindKerberosRealmByName (const char *inRealmName,
                                    KLIndex    *outIndex)
{
    KLStatus ret;
    char *realm = NULL;

    ret = KLGetKerberosDefaultRealmByName (&realm);
    if (ret == klNoErr) {
        if (!strcmp (inRealmName, realm)) {
            *outIndex = 0;
        } else {
            ret = klRealmDoesNotExistErr;
        }
	free(realm);
    }

    
    return ret;
}


KLStatus KLGetKerberosRealm (KLIndex   inIndex,
                             char    **outRealmName)
{
    return KLGetKerberosDefaultRealmByName (outRealmName);
}


KLStatus KLSetKerberosRealm (KLIndex     inIndex,
                             const char *inRealmName)
{
    return klNoErr;
}


KLStatus KLRemoveKerberosRealm (KLIndex inIndex)
{
    return klNoErr;
}


KLStatus KLInsertKerberosRealm (KLIndex     inInsertBeforeIndex,
                                const char *inRealmName)
{
    return klNoErr;
}


KLStatus KLRemoveAllKerberosRealms (void)
{
    return klNoErr;
}


KLSize KLCountKerberosRealms (void)
{
    return 1;
}


KLStatus KLGetKerberosDefaultRealm(KLIndex *outIndex)
{
    if(outIndex)
	*outIndex = 0;

    return klNoErr;
}


KLStatus KLGetKerberosDefaultRealmByName (char **outRealmName)
{
    LOG_ENTRY();

    mshim_init_context();

    return krb5_get_default_realm((mit_krb5_context)milcontext, outRealmName);
}


KLStatus KLSetKerberosDefaultRealm (KLIndex inIndex)
{
    return klNoErr;
}


KLStatus KLSetKerberosDefaultRealmByName (const char *inRealm)
{
    return klNoErr;
}


/* KLPrincipal functions */

KLStatus
KLCreatePrincipalFromTriplet (const char  *inName,
			      const char  *inInstance,
			      const char  *inRealm,
			      KLPrincipal *outPrincipal)
{
    mshim_init_context();

    return heim_krb5_make_principal(milcontext, outPrincipal, inRealm, inName, inInstance, NULL);
}


KLStatus
KLCreatePrincipalFromString (const char        *inFullPrincipal,
			     KLKerberosVersion  inKerberosVersion,
			     KLPrincipal       *outPrincipal)
{
    LOG_ENTRY();

    mshim_init_context();

    if (CHECK_VERSION(inKerberosVersion))
	return LOG_FAILURE(klInvalidVersionErr, "wrong version");
    
    if (inFullPrincipal == NULL)
	return klParameterErr;
    return heim_krb5_parse_name(milcontext, inFullPrincipal, outPrincipal);
}


KLStatus KLCreatePrincipalFromKerberos5Principal (void           *inKerberos5Principal,
                                                  KLPrincipal    *outPrincipal)
{
    mshim_init_context();
    if (inKerberos5Principal == NULL)
	return klParameterErr;
    return heim_krb5_copy_principal(milcontext, inKerberos5Principal, outPrincipal);
}


KLStatus KLCreatePrincipalFromPrincipal (KLPrincipal inPrincipal,
                                         KLPrincipal *outPrincipal)
{
    mshim_init_context();
    if (inPrincipal == NULL)
	return klParameterErr;
    return heim_krb5_copy_principal(milcontext, inPrincipal, outPrincipal);
}


KLStatus
KLGetTripletFromPrincipal(KLPrincipal inPrincipal,
			  char **outName,
			  char **outInstance,
			  char **outRealm)
{
    LOG_ENTRY();
    mshim_init_context();
    *outInstance = NULL;
    if (inPrincipal == NULL)
	return klParameterErr;
    switch (inPrincipal->name.name_string.len) {
    case 2:
	*outInstance = strdup(inPrincipal->name.name_string.val[1]);
    case 1:
	*outName = strdup(inPrincipal->name.name_string.val[0]);
	break;
    default:
	return klInvalidOptionErr;
    }
    *outRealm = strdup(inPrincipal->realm);
    return klNoErr;
}

KLStatus
KLGetStringFromPrincipal (KLPrincipal         inPrincipal,
			  KLKerberosVersion   inKerberosVersion,
			  char              **outFullPrincipal)
{
    LOG_ENTRY();
    mshim_init_context();
    if (inPrincipal == NULL)
	return klParameterErr;
    if (CHECK_VERSION(inKerberosVersion))
	return LOG_FAILURE(klInvalidVersionErr, "wrong version");
    return heim_krb5_unparse_name(milcontext, inPrincipal, outFullPrincipal);
}


KLStatus
KLGetDisplayStringFromPrincipal (KLPrincipal         inPrincipal,
				 KLKerberosVersion   inKerberosVersion,
				 char              **outFullPrincipal)
{
    LOG_ENTRY();
    mshim_init_context();
    if (inPrincipal == NULL)
	return klParameterErr;
    if (CHECK_VERSION(inKerberosVersion))
	return LOG_FAILURE(klInvalidVersionErr, "wrong version");
    return heim_krb5_unparse_name(milcontext, inPrincipal, outFullPrincipal);
}


KLStatus
KLComparePrincipal (KLPrincipal  inFirstPrincipal,
		    KLPrincipal  inSecondPrincipal,
		    KLBoolean   *outAreEquivalent)
{
    LOG_ENTRY();
    mshim_init_context();
    if (inFirstPrincipal == NULL || inSecondPrincipal == NULL)
	return klParameterErr;
    *outAreEquivalent = heim_krb5_principal_compare(milcontext, inFirstPrincipal, inSecondPrincipal);
    return klNoErr;
}


KLStatus
KLDisposePrincipal (KLPrincipal inPrincipal)
{
    LOG_ENTRY();
    mshim_init_context();
    if (inPrincipal == NULL)
	return klNoErr;
    heim_krb5_free_principal(milcontext, inPrincipal);
    return klNoErr;
}


/* KLLoginOptions functions */

KLStatus
KLCreateLoginOptions (KLLoginOptions *outOptions)
{
    struct KLLoginOptions *opt;
    krb5_error_code ret;

    *outOptions = NULL;

    LOG_ENTRY();

    mshim_init_context();

    opt = calloc(1, sizeof(*opt));

    ret = heim_krb5_get_init_creds_opt_alloc(milcontext, &opt->opt);
    if (ret) {
	free(opt);
	return ret; /* XXX */
    }
    *outOptions = opt;
    return klNoErr;
}


KLStatus KLLoginOptionsSetTicketLifetime (KLLoginOptions ioOptions,
                                          KLLifetime     inTicketLifetime)
{
    LOG_ENTRY();
    heim_krb5_get_init_creds_opt_set_tkt_life(ioOptions->opt, inTicketLifetime);
    return klNoErr;
}


KLStatus KLLoginOptionsSetForwardable (KLLoginOptions ioOptions,
                                       KLBoolean      inForwardable)
{
    LOG_ENTRY();
    heim_krb5_get_init_creds_opt_set_forwardable(ioOptions->opt, inForwardable);
    return klNoErr;
}


KLStatus KLLoginOptionsSetProxiable (KLLoginOptions ioOptions,
                                     KLBoolean      inProxiable)
{
    LOG_ENTRY();
    heim_krb5_get_init_creds_opt_set_proxiable(ioOptions->opt, inProxiable);
    return klNoErr;
}


KLStatus KLLoginOptionsSetRenewableLifetime (KLLoginOptions ioOptions,
                                             KLLifetime     inRenewableLifetime)
{
    LOG_ENTRY();
    heim_krb5_get_init_creds_opt_set_renew_life(ioOptions->opt, inRenewableLifetime);
    return klNoErr;
}


KLStatus
KLLoginOptionsSetAddressless (KLLoginOptions ioOptions,
			      KLBoolean      inAddressless)
{
    LOG_ENTRY();
    return klNoErr;
}


KLStatus
KLLoginOptionsSetTicketStartTime (KLLoginOptions ioOptions,
				  KLTime         inStartTime)
{
    LOG_ENTRY();
    return klNoErr;
}


KLStatus
KLLoginOptionsSetServiceName(KLLoginOptions  ioOptions,
			     const char     *inServiceName)
{
    LOG_ENTRY();
    if (ioOptions->service)
	free(ioOptions->service);
    ioOptions->service = strdup(inServiceName);
    return klNoErr;
}


KLStatus KLDisposeLoginOptions(KLLoginOptions ioOptions)
{
    LOG_ENTRY();
    mshim_init_context();
    heim_krb5_get_init_creds_opt_free(milcontext, ioOptions->opt);
    if (ioOptions->service)
	free(ioOptions->service);
    free(ioOptions);
    return klNoErr;
}


KLStatus KLDisposeString (char *inStringToDispose)
{
    free(inStringToDispose);
    return klNoErr;
}

KLStatus __KLSetApplicationPrompter(int inPrompter)
{
    /* Deprecated */
    return klNoErr;
}

KLStatus __KLSetHomeDirectoryAccess (KLBoolean inAllowHomeDirectoryAccess)
{
    return heim_krb5_set_home_dir_access(NULL, inAllowHomeDirectoryAccess);
}


KLBoolean __KLAllowHomeDirectoryAccess (void)
{
    return 1;
}


KLStatus __KLSetAutomaticPrompting (KLBoolean inAllowAutomaticPrompting)
{
    return klNoErr;
}


KLBoolean __KLAllowAutomaticPrompting (void)
{
    return 0;
}


KLStatus __KLSetPromptMechanism (int inPromptMechanism)
{
    return klNoErr;
}


int __KLPromptMechanism (void)
{
    return klPromptMechanism_None;
}


KLBoolean __KLAllowRememberPassword (void)
{
    return klNoErr;
}


KLStatus __KLCreatePrincipalFromTriplet (const char  *inName,
                                         const char  *inInstance,
                                         const char  *inRealm,
                                         KLKerberosVersion  inKerberosVersion,
                                         KLPrincipal *outPrincipal)
{
    LOG_UNIMPLEMENTED();
    if (CHECK_VERSION(inKerberosVersion))
	return LOG_FAILURE(klInvalidVersionErr, "wrong version");
    return EINVAL;
}


KLStatus __KLGetTripletFromPrincipal (KLPrincipal         inPrincipal,
                                      KLKerberosVersion   inKerberosVersion,
                                      char              **outName,
                                      char              **outInstance,
                                      char              **outRealm)
{
    LOG_UNIMPLEMENTED();
    if (CHECK_VERSION(inKerberosVersion))
	return LOG_FAILURE(klInvalidVersionErr, "wrong version");
    return EINVAL;
}


KLStatus __KLCreatePrincipalFromKerberos5Principal (krb5_principal inPrincipal,
                                                    KLPrincipal *outPrincipal)
{
    return KLCreatePrincipalFromKerberos5Principal (inPrincipal, outPrincipal);
}


KLStatus __KLGetKerberos5PrincipalFromPrincipal (KLPrincipal     inPrincipal, 
                                                 krb5_context    inContext, 
                                                 krb5_principal *outKrb5Principal)
{
    LOG_UNIMPLEMENTED();
    return EINVAL;
}


KLBoolean __KLPrincipalIsTicketGrantingService (KLPrincipal inPrincipal)
{
    LOG_UNIMPLEMENTED();
    return klNoErr;
}


KLStatus __KLGetKeychainPasswordForPrincipal (KLPrincipal   inPrincipal,
                                              char        **outPassword)
{
    LOG_UNIMPLEMENTED();
    return EINVAL;
}



KLStatus __KLPrincipalSetKeychainPassword (KLPrincipal  inPrincipal,
                                           const char  *inPassword)
{
    return klNoErr;
}


KLStatus __KLRemoveKeychainPasswordForPrincipal (KLPrincipal inPrincipal)
{
    return klNoErr;
}

krb5_get_init_creds_opt *
__KLLoginOptionsGetKerberos5Options (KLLoginOptions ioOptions)
{
    return NULL;
}

KLTime __KLLoginOptionsGetStartTime (KLLoginOptions ioOptions)
{
    return klNoErr;
}

char *__KLLoginOptionsGetServiceName (KLLoginOptions ioOptions)
{
    return NULL;
}
