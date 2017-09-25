/*
 * Copyright (c) 2006-2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include "NetworkAuthenticationHelper.h"
#include "KerberosHelper.h"
#include "KerberosHelperContext.h"
#include "lookupDSLocalKDC.h"
#include "util.h"

#include <Heimdal/locate_plugin.h>

#include <GSS/gssapi.h>
#include <GSS/gssapi_spi.h>
#include <GSS/gssapi_spnego.h>

#include <Carbon/Carbon.h>
#include <CoreServices/CoreServices.h>
#include <CoreServices/CoreServicesPriv.h>
#include <Security/Security.h>
#include <Security/SecCertificateOIDs.h>
#include <Security/SecCertificatePriv.h>
#include <CommonCrypto/CommonDigest.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <os/log.h>

#include "DeconstructServiceName.h"
#include "spnego_asn1.h"
#include "utils.h"

int
der_print_heim_oid (
	const heim_oid */*oid*/,
	char /*delim*/,
	char **/*str*/);


#define DEBUG 0  /* Set to non-zero for more debug spew than you want. */

static krb5_error_code
_k5_check_err(krb5_error_code error, const char *function, const char *file, int line)
{
    //    if (error)
    //        os_log(OS_LOG_DEFAULT, "    %s: krb5 call got %d (%s) on %s:%d", function, error, error_message(error), file, line);
    return error;
}

// static krb5_error_code _k5_check_err(krb5_error_code error, const char *function, const char *file, int line);
#define k5_ok(x) _k5_check_err (x, __func__, __FILE__, __LINE__)

#define KHLog(FMT, ...)     os_log(OS_LOG_DEFAULT, FMT, __VA_ARGS__)

static const char lkdc_prefix[] = "LKDC:";

/*
 *
 */

static const char wellknown_lkdc[] = "WELLKNOWN:COM.APPLE.LKDC";

static int
is_lkdc_realm(const char *realm)
{
    if (realm == NULL)
	return 0;
    return strncmp(realm, lkdc_prefix, sizeof(lkdc_prefix)-1) == 0 ||
	strncmp(realm, wellknown_lkdc, sizeof(wellknown_lkdc) - 1) == 0;
}

/* If realms a and b have a common subrealm, returns the number of
 * common components.  Otherwise, returns zero.
 */
static int
has_common_subrealm(const char *a, const char *b)
{
    const char *ap = &a[strlen(a)];
    const char *bp = &b[strlen(b)];
    unsigned int n = 0;

    if (ap == a || bp == b)
        return 0;
    for (--ap, --bp; ap >= a && bp >= b && *ap == *bp; --ap, --bp)
        if ((ap == a && bp == b) ||
            (ap == a && '.' == bp[-1]) ||
            (bp == b && '.' == ap[-1]) ||
            ('.' == ap[-1] && '.' == bp[-1]))
            ++n;
    return n;
}

static const char *
principal_realm(const char *princ)
{
    const char *p = &princ[strlen(princ)];

    while (p > princ) {
        if ('@' == p[0] && '\\' != p[-1])
            break;
        --p;
    }
    if (p == princ)
        return NULL;
    else
        return &p[1];
}

static void
add_mapping(KRBHelperContextRef hCtx, const char *hostname, const char *realm, int islkdc)
{
    struct realm_mappings *p;

    p = realloc(hCtx->realms.data, sizeof(hCtx->realms.data[0]) * (hCtx->realms.len + 1));
    if (p == NULL)
	return;
    hCtx->realms.data = p;

    hCtx->realms.data[hCtx->realms.len].lkdc = islkdc;
    hCtx->realms.data[hCtx->realms.len].hostname = strdup(hostname);
    if (hCtx->realms.data[hCtx->realms.len].hostname == NULL)
	return;

    hCtx->realms.data[hCtx->realms.len].realm = strdup(realm);
    if (hCtx->realms.data[hCtx->realms.len].realm == NULL) {
	free(hCtx->realms.data[hCtx->realms.len].hostname);
	return;
    }
    hCtx->realms.len++;
}

static void
find_mapping(KRBHelperContextRef hCtx, const char *hostname)
{
    krb5_error_code ret;
    char **realmlist = NULL;
    size_t i;
	
    for (i = 0; i < hCtx->realms.len; i++)
	if (strcasecmp(hCtx->realms.data[i].hostname, hostname) == 0)
	    return;
	
    ret = krb5_get_host_realm(hCtx->krb5_ctx, hostname, &realmlist);
    if (ret == 0) {
	for (i = 0; realmlist && realmlist[i] && *(realmlist[i]); i++)
	    add_mapping(hCtx, hostname, realmlist[i], 0);
	if (i == 0)
            KHLog ("    %s: krb5_get_host_realm returned unusable realm!", __func__);
    }

    if (realmlist)
        krb5_free_host_realm (hCtx->krb5_ctx, realmlist);
}

static int
parse_principal_name(krb5_context ctx, const char *princname, char **namep, char **instancep, char **realmp)
{
    krb5_principal principal = NULL;
    int err = 0;
    int len;

    KHLog ("[[[ %s () decomposing %s", __func__, princname);
    *namep = *instancep = *realmp = NULL;

    if (krb5_parse_name(ctx, princname, &principal)) {
        err = -1;
        goto fin;
    }
    
    len = krb5_principal_get_num_comp(ctx, principal);
    if (len > 0)
	*namep = strdup(krb5_principal_get_comp_string(ctx, principal, 0));
    if (len > 1)
	*instancep = strdup(krb5_principal_get_comp_string(ctx, principal, 1));
    *realmp = strdup(krb5_principal_get_realm(ctx, principal));
    
 fin:
    if (NULL != principal)
        krb5_free_principal(ctx, principal);
    KHLog ("]]] %s () - %d", __func__, err);
    return err;
}

static int
lookup_by_kdc(KRBhelperContext *hCtx, const char *name, char **realm)
{
    krb5_error_code ret;
    krb5_cccol_cursor cursor;
    krb5_ccache id;
    krb5_creds mcred, *creds;

    memset(&mcred, 0, sizeof(mcred));
    *realm = NULL;

    ret = krb5_build_principal(hCtx->krb5_ctx, &mcred.server,
			       0, "", 
			       "host", name, NULL); /* XXX propper service name */
    if (ret)
	return ret;

    ret = krb5_cccol_cursor_new(hCtx->krb5_ctx, &cursor);
    if (ret)
	return ret;

    while ((ret = krb5_cccol_cursor_next(hCtx->krb5_ctx, cursor, &id)) == 0) {
	const char *errmsg = NULL;
	char *clientname;

	if (id == NULL) {
	    ret = memFullErr; /* XXX */
	    break;
	}

	ret = krb5_cc_get_principal(hCtx->krb5_ctx, id, &mcred.client);
	if (ret)
	    goto next;

	ret = krb5_unparse_name(hCtx->krb5_ctx, mcred.client, &clientname);
	if (ret) {
	    krb5_free_principal(hCtx->krb5_ctx, mcred.client);
	    KHLog ("Failed to unparse name %s () - %d", __func__, ret);
	    goto next;
	}

	ret = krb5_principal_set_realm(hCtx->krb5_ctx, mcred.server, mcred.client->realm);
	if (ret) {
	    free(clientname);
	    krb5_free_principal(hCtx->krb5_ctx, mcred.client);
	    goto next;
	}

	ret = krb5_get_credentials(hCtx->krb5_ctx, 0, id, &mcred, &creds);
	krb5_free_principal(hCtx->krb5_ctx, mcred.client);
	if (ret)
	    errmsg = krb5_get_error_message(hCtx->krb5_ctx, ret);
	KHLog ("krb5_get_credentials(%s): referrals %s () - %s (%d)",
	       clientname, __func__, errmsg ? errmsg : "success", ret);
	if (errmsg)
	    krb5_free_error_message(hCtx->krb5_ctx, errmsg);
	free(clientname);
	if (ret == 0) {
	    *realm = strdup(creds->server->realm);
	    krb5_free_creds(hCtx->krb5_ctx, creds);
	    krb5_cc_close(hCtx->krb5_ctx, id);
	    break;
	}
    next:
	krb5_cc_close(hCtx->krb5_ctx, id);
    }

    krb5_free_principal(hCtx->krb5_ctx, mcred.server);
    krb5_cccol_cursor_free(hCtx->krb5_ctx, &cursor);

    return ret;
}

static int
strcmp_trailer(const char *f, const char *p)
{
    size_t flen = strlen(f), plen = strlen(p);
	
    if (f[flen - 1] == '.')
	flen--;
	
    if (flen > plen)
	return flen - plen;
    return strcasecmp(&f[flen - plen], p);
}

static int
is_local_hostname(const char *host)
{
    static dispatch_once_t once;
    static char *btmmDomain;

    dispatch_once(&once, ^{
	    CFStringRef d = _CSBackToMyMacCopyDomain();
	    if (d) {
		__KRBCreateUTF8StringFromCFString(d, &btmmDomain);
		CFRelease(d);
		if (btmmDomain) {
		    size_t len = strlen(btmmDomain);
		    if (len > 0 && btmmDomain[len - 1] == '.')
			btmmDomain[len - 1] = '\0';
		}
	    }
	});


    if (strcmp_trailer(host, ".local") == 0)
	return 1;
    if (btmmDomain && strcmp_trailer(host, btmmDomain) == 0)
	return 1;
    if (strchr(host, '.'))
	return 1;
    return 0;
}

/*
 *
 */

OSStatus
KRBCreateSession(CFStringRef inHostName, CFStringRef inAdvertisedPrincipal,
                 void **outKerberosSession)
{
    KRBHelperContextRef session = NULL;
    CFMutableDictionaryRef inInfo;
    OSStatus error;

    *outKerberosSession = NULL;

    inInfo = CFDictionaryCreateMutable (kCFAllocatorDefault, 2, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (inInfo == NULL)
	return memFullErr;

    if (inHostName)
	CFDictionarySetValue (inInfo, kKRBHostnameKey, inHostName);
    if (inAdvertisedPrincipal)
	CFDictionarySetValue (inInfo, kKRBAdvertisedPrincipalKey, inAdvertisedPrincipal);

    error = KRBCreateSessionInfo(inInfo, &session);
    CFRelease(inInfo);
    if (error == noErr)
	*outKerberosSession = session;

    return error;
}


OSStatus
KRBCreateSessionInfo (CFDictionaryRef inDict, KRBHelperContextRef *outKerberosSession)
{
    CFStringRef inHostName, inAdvertisedPrincipal, noLocalKDC;
    char hbuf[NI_MAXHOST];
    struct addrinfo hints, *aip = NULL;
    KRBhelperContext *hCtx = NULL;
    char *tmp = NULL;
    char *hintname = NULL, *hinthost = NULL, *hintrealm = NULL;
    char *localname = NULL, *hostname = NULL;
    struct realm_mappings *selected_mapping = NULL;
    OSStatus err = noErr;
    int avoidDNSCanonicalizationBug = 0;
    size_t i;

    *outKerberosSession = NULL;
    
    if (NULL == outKerberosSession)
	return paramErr;

    inHostName = CFDictionaryGetValue(inDict, kKRBHostnameKey);
    inAdvertisedPrincipal = CFDictionaryGetValue(inDict, kKRBAdvertisedPrincipalKey);
    noLocalKDC = CFDictionaryGetValue(inDict, kKRBNoLKDCKey);

    KHLog ("[[[ %s () - required parameters okay: %s %s %s", __func__,
	   inHostName ? "iHN" : "-",
	   inAdvertisedPrincipal ? "iAP" : "-",
	   noLocalKDC ? "nLK" : "-");

    /*
     * Create the context that we will return on success.
     * We almost always require a Kerberos context and the default
     * realm, so populate those up front.
     */
    if (NULL == (hCtx = calloc(1, sizeof(*hCtx))))
	return memFullErr;

    if (0 != k5_ok( krb5_init_context (&hCtx->krb5_ctx) )) {
	err = memFullErr;
	goto out;
    }

    if (0 != hx509_context_init(&hCtx->hx_ctx)) {
	err = memFullErr;
	goto out;
    }

    if (0 == k5_ok( krb5_get_default_realm (hCtx->krb5_ctx, &tmp) ) && NULL != tmp) {
	if (NULL == (hCtx->defaultRealm = strdup (tmp))) {
	    err = memFullErr;
	    goto out;
	}
	krb5_xfree(tmp);
    }

    /* If no host name was given, then the caller is inquiring
     * about the Local KDC realm.  Our work here is done.
     */
    if (NULL == inHostName) {
        err = DSCopyLocalKDC (&hCtx->realm);
        KHLog ("    %s: LocalKDC realm lookup only", __func__);
        goto out;
    }

    /* Retain any given advertised principal name in the context
     * and break it down into service, instance, and realm.  These
     * will be used as hints when better information is not
     * available.
     */
    if (NULL != inAdvertisedPrincipal) {
        char *s = NULL;
        hCtx->inAdvertisedPrincipal = CFRetain (inAdvertisedPrincipal);
        if (0 != __KRBCreateUTF8StringFromCFString (inAdvertisedPrincipal, &s))
            KHLog ("    %s: __KRBCreateUTF8StringFromCFString failed", __func__);
        else {
            (void)parse_principal_name (hCtx->krb5_ctx, s, &hintname, &hinthost, &hintrealm);
            __KRBReleaseUTF8String (s);
        }
    }

    /* Decode the given host name with _CFNetServiceDeconstructServiceName before proceeding. */

    if (! _CFNetServiceDeconstructServiceName (inHostName, &hostname))
        __KRBCreateUTF8StringFromCFString (inHostName, &hostname);
    else
        avoidDNSCanonicalizationBug = 1;

    /* remove trailing dot */
    i = strlen(hostname);
    if (hostname[i - 1] == '.') {
	hostname[i - 1] = '\0';

	/* Stuff it back into context */
	hCtx->inHostName = CFStringCreateWithCString (NULL, hostname, kCFStringEncodingUTF8);
	if (hCtx->inHostName == NULL) {
	    err = memFullErr;
	    goto out;
	}
    } else {
	hCtx->inHostName = CFRetain (inHostName);
    }

    KHLog ("    %s: processed host name = %s", __func__, hostname);

    /* 
     * Try find name by asking the KDC first
     */

    if (lookup_by_kdc(hCtx, hostname, &tmp) == 0) {
	add_mapping(hCtx, hostname, tmp, 0);
	free(tmp);
	err = noErr;
	hCtx->noGuessing = 1;
	goto done;
    }


    /*
     * If the given name is a bare name (i.e. no dots), we may need
     * to attempt to look it up as `<name>.local' later.
     */
    if (NULL == strchr(hostname, '.') &&
	(0 > asprintf(&localname, "%s.local", hostname) || NULL == localname)) {
	err = memFullErr;
	goto out;
    }

    /*
     * Before we canonlize the hostname, lets find the realm.
     */

    find_mapping(hCtx, hostname);

    /* Normalize the given host name using getaddrinfo AI_CANONNAME if
     * possible. Track the resulting host name (normalized or not) as
     * `hostname'.
     *
     * Avoid canonicalization if possible because of
     * <rdar://problem/5517187> getaddrinfo with hints.ai_flags =
     * AI_CANONNAME mangles quoted DNS Names.
     */
    {
        memset (&hints, 0, sizeof(hints));
        hints.ai_flags = AI_CANONNAME;
        err = getaddrinfo (hostname, NULL, &hints, &hCtx->addr);
        KHLog ("    %s: getaddrinfo = %s (%d)", __func__, 0 == err ? "success" : gai_strerror (err), (int)err);
        if (0 == err && avoidDNSCanonicalizationBug == 0 && hCtx->addr->ai_canonname) {
	    if ((tmp = strdup(hCtx->addr->ai_canonname)) != NULL) {
		free(hostname);
		hostname = tmp;
	    }
            KHLog ("    %s: canonical host name = %s", __func__, hostname);
        }
    }

    /*
     * Try adding the mapping for the canonlical name if we got one
     */

    find_mapping(hCtx, hostname);

    /*
     * If we have a hintrealm and there our initial guessing is right,
     * lets skip the reverse lookup since that is potentially very
     * expensive.
     */

    if (hintrealm) {
	for (i = 0; i < hCtx->realms.len; i++)
	    if (strcmp(hintrealm, hCtx->realms.data[i].realm) == 0)
		goto done;
    }

    /*
     * Try to find all name for this address, and the check if we can
     * find a mapping.
     */

    for (aip = hCtx->addr; NULL != aip; aip = aip->ai_next) {
	char ipbuf[NI_MAXHOST];
		
	/* pretty print name first for logging */
	err = getnameinfo(aip->ai_addr, aip->ai_addrlen,
			  ipbuf, sizeof(ipbuf),
			  NULL, 0, NI_NUMERICHOST);
	if (err)
	    snprintf(ipbuf, sizeof(ipbuf), "getnameinfo-%d", (int)err);

        err = getnameinfo (aip->ai_addr, aip->ai_addr->sa_len, hbuf,
			   sizeof(hbuf), NULL, 0, NI_NAMEREQD);
        KHLog("    %s: getnameinfo(%s) -> %s result %d %s",
	      __func__, ipbuf, hbuf, (int)err, 0 == err ? "success" : gai_strerror (err));
        if (err) {
	    /* This is not a fatal error.  We'll keep looking for candidate host names. */
            continue;
        }
	find_mapping(hCtx, hbuf);
    }

    /* Reset err */
    err = noErr;

    /*
     * Also, add localname (bare name) that we turned into a .local
     * name if we had one.
     */

    if (localname)
	find_mapping(hCtx, localname);

 done:
    /*
     * Done fetching all data, will no try to find a mapping
     */ 

    for (i = 0; i < hCtx->realms.len; i++) {
	KHLog ("    %s: available mappings: %s -> %s (%s)", __func__,
	       hCtx->realms.data[i].hostname,
	       hCtx->realms.data[i].realm,
	       hCtx->realms.data[i].lkdc ? "LKDC" : "managed");
    }
	
    /*
     * If we have noGuessing mapping, lets pick the first guess then.
     */

    if (!selected_mapping && hCtx->noGuessing && hCtx->realms.len)
	selected_mapping = &hCtx->realms.data[0];

    /*
     * If we have "local" hostname, lets consider LKDC more aggressively.
     */

    if (noLocalKDC == 0 && is_local_hostname(hostname)) {

	if (hintrealm) {
	    /*
	     * Search for localKDC mapping when we have a hint realm.
	     */
	    for (i = 0; i < hCtx->realms.len && !selected_mapping; i++)
		if (strcasecmp(hintrealm, hCtx->realms.data[i].realm) == 0)
		    selected_mapping = &hCtx->realms.data[i];
	} else {				
	    /*
	     * If we are using have no hintrealm, just pick any LKDC realm.
	     */
	    for (i = 0; i < hCtx->realms.len && !selected_mapping; i++)
		if (hCtx->realms.data[i].lkdc)
		    selected_mapping = &hCtx->realms.data[i];
	}
    }
    /*
     * Search for managed realm, the make us prefer manged realms for
     * no local hostnames.
     */
    for (i = 0; i < hCtx->realms.len && !selected_mapping; i++) {
	if (hCtx->realms.data[i].lkdc)
	    continue;
	if (hintrealm == NULL || strcasecmp(hintrealm, hCtx->realms.data[i].realm) == 0)
	    selected_mapping = &hCtx->realms.data[i];
    }

    /*
     * Search for LKDC again if no managed realm was found 
     */
    for (i = 0; i < hCtx->realms.len && !selected_mapping; i++)
	if (hintrealm == NULL || strcasecmp(hintrealm, hCtx->realms.data[i].realm) == 0)
	    selected_mapping = &hCtx->realms.data[i];

    /*
     * If we still failed to find a mapping, just pick the first.
     */
    for (i = 0; i < hCtx->realms.len && !selected_mapping; i++)
	selected_mapping = &hCtx->realms.data[i];

    if (selected_mapping == NULL) {
	KHLog ("    %s: No mapping for host name = %s found", __func__, hostname);
	err = memFullErr;
	goto out;
    }
    KHLog ("    %s: Using host name = %s, realm = %s (%s)", __func__,
	   selected_mapping->hostname,
	   selected_mapping->realm, selected_mapping->lkdc ? "LKDC" : "managed");

    hCtx->realm = CFStringCreateWithCString (kCFAllocatorDefault, selected_mapping->realm, kCFStringEncodingASCII);
    if (hCtx->realm == NULL) {
	err = memFullErr;
	goto out;
    }

    /* If its a LKDC realm, the hostname is the LKDC realm */
    if (selected_mapping->lkdc)
	hCtx->hostname = CFRetain(hCtx->realm);
    else
	hCtx->hostname = CFStringCreateWithCString(kCFAllocatorDefault, selected_mapping->hostname, kCFStringEncodingASCII);
    if (hCtx->hostname == NULL) {
	err = memFullErr;
	goto out;
    }
	
 out:
    free (hintname);
    free (hinthost);
    free (hintrealm);
    free (hostname);
    free (localname);

    /* 
     * On error, free all members of the context and the context itself.
     */
    if (noErr != err) {
	for (i = 0; i < hCtx->realms.len; i++) {
	    free(hCtx->realms.data[i].hostname);
	    free(hCtx->realms.data[i].realm);
	}
	free(hCtx->realms.data);
	free (hCtx->defaultRealm);
	if (NULL != hCtx->realm)
	    CFRelease (hCtx->realm);
	if (NULL != hCtx->inAdvertisedPrincipal)
	    CFRelease (hCtx->inAdvertisedPrincipal);
	if (NULL != hCtx->hostname)
	    CFRelease (hCtx->hostname);
	if (NULL != hCtx->inHostName)
	    CFRelease (hCtx->inHostName);
	if (NULL != hCtx->krb5_ctx)
	    krb5_free_context (hCtx->krb5_ctx);
	if (NULL != hCtx->hx_ctx)
	    hx509_context_free(&hCtx->hx_ctx);
	if (NULL != hCtx->addr)
	    freeaddrinfo(hCtx->addr);
	
	free (hCtx);
    } else
        *outKerberosSession = hCtx;

    KHLog ("]]] %s () = %d", __func__, (int)err);
    return err;
}

/*

  KRBCopyREALM will return the REALM for the host that was passed to KRBCreateSession
  inKerberosSession is the pointer returned by KRBCreateSession
  outREALM is the REALM of the host
*/
OSStatus KRBCopyRealm(KRBHelperContextRef inKerberosSession, CFStringRef *outRealm)
{
    OSStatus err = noErr;
    
    KRBhelperContext *hCtx = (KRBhelperContext *)inKerberosSession;

    if (NULL == hCtx) {
        err = paramErr; /* Invalid session context */
        goto Done;
    }

    KHLog ("%s", "[[[ KRBCopyRealm () - required parameters okay");

    if (NULL == hCtx->realm) {
        err = paramErr;
        goto Done;
    }
    
    *outRealm = CFRetain (hCtx->realm);
 Done:
    KHLog ("]]] KRBCopyRealm () = %d", (int)err);

    return err;
}

/*
  KRBCopyKeychainLookupInfo will return a dictionary containing information related to Kerberos and keychain items.
  inKerberosSession is the pointer returned by KRBCreateSession
  inUsername is an available and usable Username or NULL
  outKeychainLookupInfo is a dictionary containing keychain lookup info and if it is acceptable to store a
  password in the keychain.
            
  outKeychainLookupInfo
  kKRBUsernameKey                   : CFStringRef
  kKRBKeychainAccountNameKey           : CFStringRef
  kKRBDisableSaveToKeychainKey  : CFBooleanRef
        
*/  
OSStatus KRBCopyKeychainLookupInfo (KRBHelperContextRef inKerberosSession, CFStringRef inUsername, CFDictionaryRef *outKeychainLookupInfo)
{
    OSStatus                err = noErr;
    KRBhelperContext        *hCtx = (KRBhelperContext *)inKerberosSession;
    CFMutableDictionaryRef  outInfo = NULL;
    CFStringRef             accountName = NULL;

    if (NULL == inKerberosSession || NULL == outKeychainLookupInfo) { err = paramErr; goto Error; }
    
    *outKeychainLookupInfo = NULL;

    outInfo = CFDictionaryCreateMutable (kCFAllocatorDefault, /* hint */ 3, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    if (NULL == outInfo) { err = memFullErr; goto Error; }

    KHLog ("%s", "[[[ KRBCopyKeychainLookupInfo () - required parameters okay");

    if (NULL != inUsername) {
        CFDictionarySetValue (outInfo, kKRBUsernameKey, inUsername);
        accountName = hCtx->realm;
    } else {
        accountName = hCtx->realm;
    }

    if (NULL != accountName) {
        CFDictionarySetValue (outInfo, kKRBKeychainAccountNameKey, accountName);
    }

    /* Can a Kerberos password be saved in the keychain? */
    CFDictionarySetValue (outInfo, kKRBDisableSaveToKeychainKey, kCFBooleanFalse);

    CFPropertyListRef savePasswordDisabled = CFPreferencesCopyAppValue(CFSTR("SavePasswordDisabled"), kKRBAgentBundleIdentifier);
    
    if (savePasswordDisabled != NULL) {
        if (CFGetTypeID(savePasswordDisabled) == CFBooleanGetTypeID() && CFBooleanGetValue(savePasswordDisabled)) {
            CFDictionarySetValue (outInfo, kKRBDisableSaveToKeychainKey, kCFBooleanTrue);
            KHLog ("%s", "    KRBCopyKeychainLookupInfo: DisableSaveToKeychainKey = TRUE");
        }
        CFRelease(savePasswordDisabled);
    } else {
        KHLog ("%s", "    KRBCopyKeychainLookupInfo: CFPreferencesCopyAppValue == NULL");
    }
    
    *outKeychainLookupInfo = outInfo;
 Error:
    KHLog ("]]] KRBCopyKeychainLookupInfo () = %d", (int)err);
    
    return err;
}

/*
  KRBCopyServicePrincipal will return the service principal for a service on the host given inInstance.
  inKerberosSession is the pointer returned by KRBCreateSession
  inServiceName is the name of the service on the host, it can be NULL if inAdvertisedPrincipal was non-NULL.
  However it is highly recommended that this be set as it is insecure to rely on remotely provided information
  outServicePrincipal the service principal
*/
OSStatus KRBCopyServicePrincipal (KRBHelperContextRef inKerberosSession, CFStringRef inServiceName, CFStringRef *outServicePrincipal)
{
    CFDictionaryRef outDict = NULL;
    CFStringRef outSPN;
    OSStatus            err;

    KHLog ("%s", "KRBCopyServicePrincipal () - enter");

    if (NULL == inKerberosSession || NULL == outServicePrincipal)
	return paramErr;

    *outServicePrincipal = NULL;

    err = KRBCopyServicePrincipalInfo(inKerberosSession,
				      inServiceName,
				      &outDict);
    if (err)
	return err;

    outSPN = CFDictionaryGetValue(outDict, kKRBServicePrincipalKey);

    *outServicePrincipal = CFRetain(outSPN);
    CFRelease(outDict);

    KHLog ("%s", "KRBCopyServicePrincipal () - return");

    return noErr;
}


OSStatus
KRBCopyServicePrincipalInfo (KRBHelperContextRef inKerberosSession, CFStringRef inServiceName, CFDictionaryRef *outServiceInfo)
{
    KRBhelperContext    *hCtx = (KRBhelperContext *)inKerberosSession;
    CFMutableDictionaryRef outInfo = NULL;
    CFStringRef outString = NULL;
    OSStatus            err = noErr;
        
    if (NULL == hCtx || NULL == outServiceInfo || NULL == inServiceName)
	return paramErr;

    *outServiceInfo = NULL;

    KHLog ("%s", "[[[ KRBCopyServicePrincipalInfo () - required parameters okay");

    outString = CFStringCreateWithFormat (NULL, NULL, CFSTR("%@/%@@%@"), inServiceName, hCtx->hostname, hCtx->realm);
    if (outString == NULL) {
	err = memFullErr;
	goto out;
    }

    outInfo = CFDictionaryCreateMutable (kCFAllocatorDefault, 2, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (outInfo == NULL) {
	err = memFullErr;
	goto out;
    }
	
    CFDictionarySetValue (outInfo, kKRBServicePrincipalKey, outString);
    if (hCtx->noGuessing)
	CFDictionarySetValue (outInfo, kKRBNoCanonKey, CFSTR("nodns"));
	

    {
	char *spn;
	__KRBCreateUTF8StringFromCFString (outString, &spn);
	KHLog ("    KRBCopyServicePrincipalInfo: principal = \"%s\"", spn);
	__KRBReleaseUTF8String (spn);
    }

    *outServiceInfo = outInfo;
    
 out:
    if (outString) CFRelease(outString);

    KHLog ("]]] KRBCopyServicePrincipalInfo () = %d", (int)err);

    return err;
}

static CFTypeRef
search_array(CFArrayRef array, CFTypeRef key)
{
    CFDictionaryRef dict;
    CFIndex n;

    for (n = 0 ; n < CFArrayGetCount(array); n++) {
	dict = CFArrayGetValueAtIndex(array, n);
	if (CFGetTypeID(dict) != CFDictionaryGetTypeID())
	    continue;
	CFTypeRef dictkey = CFDictionaryGetValue(dict, kSecPropertyKeyLabel);
	if (CFEqual(dictkey, key))
	    return CFDictionaryGetValue(dict, kSecPropertyKeyValue);
    }
    return NULL;
}

/*
  KRBCopyClientPrincipalInfo will return a dictionary with the user principal and other information.
  inKerberosSession is the pointer returned by KRBCreateSession.
  inOptions a dictionary with options regarding the acquisition of the user principal.
  inIdentityRef is a reference to list of usable identities
  outClientPrincipalInfo a dictionary containing the user principal and other information necessary to get a ticket.

  inOptions Dictionary Keys
  kKRBAllowKerberosUIKey            : CFStringRef [See AllowKeberosUI values]
  kKRBServerDisplayNameKey      : CFStringRef
  kKRBUsernameKey                   : CFStringRef
  kKRBClientPasswordKey         : CFStringRef
  kKRBCertificateKey                : SecCertificateRef

  outClientPrincipalInfo
  kKRBClientPrincipalKey            : CFStringRef
  kKRBUsernameKey                   : CFStringRef
  and private information
*/

OSStatus KRBCopyClientPrincipalInfo (KRBHelperContextRef inKerberosSession,  CFDictionaryRef inOptions, CFDictionaryRef *outClientPrincipalInfo)
{
    OSStatus            err = noErr;
    KRBhelperContext    *hCtx = (KRBhelperContext *)inKerberosSession;

    CFIndex                 newCount;
    CFMutableDictionaryRef  outInfo = NULL;
    CFStringRef             clientPrincipal = NULL;
    char                    *clientPrincipalString = NULL;
    char                    *useRealm = NULL;
    CFStringRef             useClientName = NULL;
    CFStringRef             inferredLabel = NULL;
    SecCertificateRef certRef = NULL;
    CFStringRef             certificateHash = NULL;
    char *cert_hash = NULL;
    int usingCertificate = 0;
    int clientNameProvided = 0;

    if (NULL == hCtx || NULL == outClientPrincipalInfo) { err = paramErr; goto Error; }
    *outClientPrincipalInfo = NULL;

    KHLog ("%s", "[[[ KRBCopyClientPrincipalInfo () - required parameters okay");

    if (NULL != inOptions) {
        /* Figure out the maximum expansion we'll make */
        newCount = CFDictionaryGetCount (inOptions) + 3;
        
        /* Create a mutable copy of the Dictionary. */
        outInfo = CFDictionaryCreateMutableCopy (kCFAllocatorDefault, newCount, inOptions);
        
        /* Extract the certRef if there was an input dictionary */
        CFDictionaryGetValueIfPresent (inOptions, kKRBCertificateKey, (const void **)&certRef);
        
        if (NULL != certRef) {
            KHLog ("%s", "    KRBCopyClientPrincipalInfo: Certificate information in dictionary");
        } else {
            KHLog ("%s", "    KRBCopyClientPrincipalInfo: Certificate not present in dictionary");
        }
    } else {
        outInfo = CFDictionaryCreateMutable (kCFAllocatorDefault, 3, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    }
    
    if (NULL != certRef && SecCertificateGetTypeID() == CFGetTypeID (certRef)) {
	const CFStringRef dotmac = CFSTR(".Mac Sharing Certificate");
	const CFStringRef mobileMe = CFSTR("MobileMe Sharing Certificate");
        
	useClientName = NAHCopyMMeUserNameFromCertificate(certRef);
	if (useClientName == NULL) { goto Error; }

        usingCertificate = 1;

        if (NULL != useClientName) {
            certificateHash = CFRetain (useClientName);
        }
        
	void *values[4] = { (void *)kSecOIDDescription, (void *)kSecOIDCommonName, (void *)kSecOIDOrganizationalUnitName, (void *)kSecOIDX509V1SubjectName };
	CFArrayRef attrs = CFArrayCreate(NULL, (const void **)values, sizeof(values) / sizeof(values[0]), &kCFTypeArrayCallBacks);
	if (NULL == attrs) { goto Error; }

	CFDictionaryRef certval = SecCertificateCopyValues(certRef, attrs, NULL);
	CFRelease(attrs);
	if (NULL == certval) { goto Error; }

	err = 0;

	CFDictionaryRef subject = CFDictionaryGetValue(certval, kSecOIDX509V1SubjectName);
	if (NULL != subject) {

	    CFArrayRef val = CFDictionaryGetValue(subject, kSecPropertyKeyValue);

	    if (NULL != val) {

		CFStringRef description = search_array(val, kSecOIDDescription);

		if (NULL != description &&
		    (kCFCompareEqualTo == CFStringCompare(description, dotmac, 0) || kCFCompareEqualTo == CFStringCompare(description, mobileMe, 0)))
		{
		    CFStringRef commonName = search_array(val, kSecOIDCommonName);
		    CFStringRef organizationalUnit = search_array(val, kSecOIDOrganizationalUnitName);

		    if (NULL != commonName && NULL != organizationalUnit) {
			inferredLabel = CFStringCreateWithFormat (NULL, NULL, CFSTR("%@@%@"), commonName, organizationalUnit);
		    }
		}
	    }
	}
    
	CFRelease(certval);

	if (NULL == inferredLabel)
	    err = SecCertificateInferLabel (certRef, &inferredLabel);

        if (0 != err) { goto Error; }            

    } else if (NULL != inOptions) {
        CFDictionaryGetValueIfPresent (inOptions, kKRBUsernameKey, (const void **)&useClientName);
        if (NULL != useClientName) {
            CFRetain(useClientName);
            clientNameProvided = 1;
        }
    }

    /* The ultimate fallback is to use the same username as we have locally */
    if (NULL == useClientName) {
        char *clientName = getlogin ();

        if (NULL != clientName) {
            useClientName = CFStringCreateWithCString (NULL, clientName, kCFStringEncodingUTF8);
            /* free (clientName); */
            KHLog ("    KRBCopyClientPrincipalInfo: Using login name = \"%s\"", clientName);
        }
    }

    /*
     * Check to see if the defaultRealm is heirarchically related to the realm used for
     * the service principal.
     */
    if (NULL != hCtx->useRealm && NULL != hCtx->defaultRealm) {
        size_t  useRealmLength = strlen (hCtx->useRealm);
        size_t  defaultRealmLength = strlen (hCtx->defaultRealm);
        
        if (defaultRealmLength < useRealmLength) {
            char *subRealm = hCtx->useRealm + (useRealmLength - defaultRealmLength);
            // printf ("defaultRealm = %s, subRealm = %s\n", hCtx->defaultRealm, subRealm);
            if (0 == strcmp (subRealm, hCtx->defaultRealm)) {
                useRealm = hCtx->defaultRealm;
            }
        }
    }

    if (NULL != useRealm) {
        clientPrincipal = CFStringCreateWithFormat (NULL, NULL, CFSTR("%@@%s"), useClientName, useRealm);
    } else {
        clientPrincipal = CFStringCreateWithFormat (NULL, NULL, CFSTR("%@@%@"), useClientName, hCtx->realm);
    }
    
    if (NULL != clientPrincipal) {
        __KRBCreateUTF8StringFromCFString (clientPrincipal, &clientPrincipalString);
        KHLog ("    KRBCopyClientPrincipalInfo: principal guess = \"%s\"", clientPrincipalString);
    }

    /*
     * Look in the ccache for a TGT from a REALM that matches "useRealm".  If we find one, then
     * use that principal.  Also extract the "Username" portion just in
     * case a password is needed (the ticket may have expired).
     */
    if (NULL != clientPrincipalString) {
        krb5_error_code krb_err = 0;
	krb5_cccol_cursor cursor;
        const char *alternateRealm = NULL;
        const char *clientRealm = principal_realm(clientPrincipalString);
        char        *alternateClientPrincipal = NULL;
        char        *bestClientPrincipal = NULL;
        int commonSubrealms = 1, tmp, found;

	found = 0;

	krb_err = krb5_cccol_cursor_new (hCtx->krb5_ctx, &cursor);
	if (0 != krb_err) {
	    krb_err = memFullErr;
	}

	/* We exit if we find more than one match */
        while (!krb_err && !found) {
            krb5_ccache ccache = NULL;
            krb5_principal ccachePrinc = NULL;
            
	    if (krb5_cccol_cursor_next (hCtx->krb5_ctx, cursor, &ccache) != 0 || ccache == NULL)
		krb_err = ENOENT;

            if (!krb_err) { krb_err = k5_ok (krb5_cc_get_principal (hCtx->krb5_ctx, ccache, &ccachePrinc)); }
            if (!krb_err) { krb_err = k5_ok (krb5_unparse_name (hCtx->krb5_ctx, ccachePrinc, &alternateClientPrincipal)); }

            if (!krb_err) { alternateRealm = principal_realm(alternateClientPrincipal); }

            /* If the client principal realm and the service principal
             * realm are an exact match or have a common subrealm with
             * multiple components, then in most cases it will be the
             * one to use.  If there are multiple matches, choose the
             * one with the largest number of common components.  The
             * commonSubrealms variable keeps track of the number of
             * components matched in the best match found so far.
             * Because it was initialized to 1 earlier, a realm must
             * have at least two common components to be considered.
             */
            if (!krb_err && (0 == (tmp = strcmp(clientRealm, alternateRealm)) ||
                             commonSubrealms < (tmp = has_common_subrealm(clientRealm, alternateRealm)))) {
                if (NULL != bestClientPrincipal) { free(bestClientPrincipal); }
                bestClientPrincipal = strdup(alternateClientPrincipal);
                if (0 == tmp) {
                    /* Exact match (strcmp set tmp to 0.)
                     * Exit the loop.
                     */
                    found = 1;
                } else {
                    /* Inexact match (has_common_subrealm set tmp nonzero.)
                     * Keep track of the number of components matched.
                     */
                    commonSubrealms = tmp;
                }
            }

            if (NULL != alternateClientPrincipal) {
                krb5_xfree(alternateClientPrincipal);
                alternateClientPrincipal = NULL;
            }
            if (NULL != ccache)      { krb5_cc_close (hCtx->krb5_ctx, ccache); }
            if (NULL != ccachePrinc) { krb5_free_principal (hCtx->krb5_ctx, ccachePrinc); }
        }

        if (NULL != cursor)   { krb5_cccol_cursor_free(hCtx->krb5_ctx, &cursor);
	}

        if (NULL != bestClientPrincipal) {
            KHLog ("    KRBCopyClientPrincipalInfo: ccache principal match = \"%s\"", bestClientPrincipal);
        }

        /* We only accept the principal when it doesn't match the one we computed.
         */
        if (NULL != bestClientPrincipal && !clientNameProvided &&
            0 != strcmp(clientPrincipalString, bestClientPrincipal)) {
            char *useClientNameString = NULL, *startOfRealm;
            
            KHLog ("%s", "    KRBCopyClientPrincipalInfo: found a single ticket for realm, replacing principal & username");
            usingCertificate = 0;

            if (NULL != clientPrincipal) { CFRelease (clientPrincipal); }
            if (NULL != clientPrincipalString) { __KRBReleaseUTF8String (clientPrincipalString); }

            clientPrincipal = CFStringCreateWithCString (NULL, bestClientPrincipal, kCFStringEncodingASCII);
            clientPrincipalString = strdup (bestClientPrincipal);
            
            /* Extract the "Username" from the principal */
            useClientNameString = strdup (bestClientPrincipal);
            
            /* This is an ugly loop.  It does the reverse of krb5_unparse_name () in that it searches from the
             * end of the principal looking for an unquoted "@".  This is a guaranteed way to strip the realm from
             * a principal with the smallest number of lines and the minimum amount of "special" quoting logic
             * and knowledge.  The alternative would have been to copy the first part of krb5_unparse_name () and
             * quoting all of the "special" characters and skipping the last part which appends the realm - this
             * takes 95 lines of code in the MIT Kerberos library.
             */

            do {
                /* This will be NULL, the last '@' char or the 2nd character of useClientNameString */
                startOfRealm = strrchr (&useClientNameString[1], '@');
                
                if (NULL != startOfRealm) {
                    /* Is it an escaped realm character? */
                    if ('\\' == *(startOfRealm - 1)) {
                        *(startOfRealm - 1) = '\0';
                    } else {
                        *startOfRealm = '\0';
                        break;
                    }
                }
            } while (NULL != startOfRealm);

            if (NULL != useClientName) { CFRelease (useClientName); }
            
            useClientName = CFStringCreateWithCString (NULL, useClientNameString, kCFStringEncodingUTF8);

            KHLog ("    KRBCopyClientPrincipalInfo: Setting found Username to = \"%s\"", useClientNameString);

            if (NULL != useClientNameString) { free (useClientNameString); }
        }

        if (NULL != bestClientPrincipal)      { free (bestClientPrincipal); }
        if (NULL != alternateClientPrincipal) { krb5_xfree(alternateClientPrincipal); }
    }
    
    KHLog ("    KRBCopyClientPrincipalInfo: using principal = \"%s\"", clientPrincipalString);
    
    CFDictionarySetValue (outInfo, kKRBClientPrincipalKey, clientPrincipal);
    CFDictionarySetValue (outInfo, kKRBUsernameKey, useClientName);

    KHLog ("    KRBCopyClientPrincipalInfo: usingCertificate == %d", usingCertificate);
    if (usingCertificate && NULL != certRef) {
        CFDictionarySetValue (outInfo, kKRBUsingCertificateKey, certRef);

        if (NULL != certificateHash)
            CFDictionarySetValue (outInfo, kKRBCertificateHashKey, certificateHash);

        if (NULL != inferredLabel) {
            CFDictionarySetValue (outInfo, kKRBCertificateInferredLabelKey, inferredLabel);

            char *inferredLabelString = NULL;
            __KRBCreateUTF8StringFromCFString (inferredLabel, &inferredLabelString);
            KHLog ("    KRBCopyClientPrincipalInfo: InferredLabel = \"%s\"", inferredLabelString);
            if (NULL != inferredLabelString) { __KRBReleaseUTF8String (inferredLabelString); }
        }
    }

    *outClientPrincipalInfo = outInfo;

 Error:
    if (NULL != useClientName)   { CFRelease (useClientName); }
    if (NULL != certificateHash) { CFRelease (certificateHash); }
    if (NULL != inferredLabel)   { CFRelease (inferredLabel); }
    if (NULL != cert_hash)       { free (cert_hash); }
    if (NULL != clientPrincipal) { CFRelease (clientPrincipal); }
    if (NULL != clientPrincipalString) { __KRBReleaseUTF8String (clientPrincipalString); }

    KHLog ("]]] KRBCopyClientPrincipalInfo () = %d", (int)err);
    
    return err;
}

/*
  KRBTestForExistingTicket will look for an existing ticket in the
  ccache.  This call looks for a principal that matches the principal
  stored in the outClientPrincipalInfo dictionary fom the
  KRBCopyClientPrincipalInfo call.
  This call should be performed before prompting the user to enter credential
  information.
  inKerberosSession is the pointer returned by KRBCreateSession
  inClientPrincipalInfo the dictionary containing the
  kKRBClientPrincipalKey.
*/
OSStatus KRBTestForExistingTicket (KRBHelperContextRef inKerberosSession, CFDictionaryRef inClientPrincipalInfo)
{
    OSStatus            err = noErr;
    KRBhelperContext    *hCtx = (KRBhelperContext *)inKerberosSession;
    CFStringRef     clientPrincipal = NULL;
    char            *principalString = NULL;

    if (NULL == hCtx || NULL == inClientPrincipalInfo) { err = paramErr; goto Done; }

    KHLog ("%s", "[[[ KRBTestForExistingTicket () - required parameters okay");

    CFDictionaryGetValueIfPresent (inClientPrincipalInfo, kKRBClientPrincipalKey, (const void **)&clientPrincipal);

    if (NULL != clientPrincipal) {
        krb5_error_code    krb_err = 0;
        krb5_principal principal = NULL;
	krb5_ccache ccache = NULL;
	time_t lifetime;

	err = ENOENT;

        __KRBCreateUTF8StringFromCFString (clientPrincipal, &principalString);
        KHLog ("    KRBTestForExistingTicket: principal = \"%s\"", principalString);

	krb_err = krb5_parse_name(hCtx->krb5_ctx, principalString, &principal);
	if (0 == krb_err) {
	    krb_err = krb5_cc_cache_match(hCtx->krb5_ctx, principal, &ccache);
	    if (krb_err == 0) {
		krb_err = krb5_cc_get_lifetime(hCtx->krb5_ctx, ccache, &lifetime);
		if (krb_err == 0 && lifetime > 60) {
		    KHLog ("    KRBTestForExistingTicket: Valid Ticket, ccacheName = \"%s\"", krb5_cc_get_name(hCtx->krb5_ctx, ccache));
		    err = 0;
		}
	    }
	}

	if (NULL != ccache) { krb5_cc_close(hCtx->krb5_ctx, ccache); }
	if (NULL != principalString) { __KRBReleaseUTF8String (principalString); }
        if (NULL != principal)  { krb5_free_principal(hCtx->krb5_ctx, principal); }
    }
    
 Done:
    KHLog ("]]] KRBTestForExistingTicket () = %d", (int)err);

    return err;
}

/*
  KRBAcquireTicket will acquire a ticket for the user.
  inKerberosSession is the pointer returned by KRBCreateSession.
  inClientPrincipalInfo is the outClientPrincipalInfo dictionary from KRBCopyClientPrincipalInfo.
*/
OSStatus KRBAcquireTicket(KRBHelperContextRef inKerberosSession, CFDictionaryRef inClientPrincipalInfo)
{
    OSStatus            err = noErr;
    KRBhelperContext    *hCtx = (KRBhelperContext *)inKerberosSession;
    krb5_error_code            krb_err = 0;
    krb5_principal      clientPrincipal = NULL;
    CFStringRef         principal = NULL, password = NULL;
    char                *principalString = NULL, *passwordString = NULL;
    SecIdentityRef      usingCertificate = NULL;
    CFStringRef		    inferredLabel = NULL;
    krb5_get_init_creds_opt *opt;
    krb5_ccache id = NULL;
    krb5_creds cred;
    int destroy_cache = 0;
    krb5_init_creds_context icc = NULL;
    
    memset(&cred, 0, sizeof(cred));

    if (NULL == hCtx) {
	KHLog ("%s", "[[[ KRBAcquireTicket () - no context will raise() in the future");
	return paramErr;
    }

    KHLog ("%s", "[[[ KRBAcquireTicket () - required parameters okay");

    principal = CFDictionaryGetValue (inClientPrincipalInfo, kKRBClientPrincipalKey);
    if (NULL == principal) { err = paramErr; goto Error; }
    __KRBCreateUTF8StringFromCFString (principal, &principalString);
    
    krb_err = krb5_parse_name(hCtx->krb5_ctx, principalString, &clientPrincipal);
    if (krb_err) {
	err = paramErr; goto Error;
    }
    
    CFDictionaryGetValueIfPresent (inClientPrincipalInfo, kKRBUsingCertificateKey, (const void **)&usingCertificate);
    
    krb_err = k5_ok(krb5_get_init_creds_opt_alloc (hCtx->krb5_ctx, &opt));
    if (krb_err) {
	err = paramErr; goto Error;
    }

    if (usingCertificate) {
	krb_err = k5_ok(krb5_get_init_creds_opt_set_pkinit(hCtx->krb5_ctx, opt, clientPrincipal,
							   NULL, "KEYCHAIN:", 
							   NULL, NULL, 0,
							   NULL, NULL, NULL));
	KHLog ("    KRBAcquireTicket: using a cert, GIC set pkinit returned: %d", krb_err);
	if (krb_err) {
	    err = paramErr; goto Error;
	}
    }

    krb_err = krb5_init_creds_init(hCtx->krb5_ctx, clientPrincipal, NULL, NULL,
				   0, opt, &icc);
    if (krb_err) {
	err = paramErr; goto Error;
    }

    if (is_lkdc_realm(krb5_principal_get_realm(hCtx->krb5_ctx, clientPrincipal))) {
	char *hostname;

	__KRBCreateUTF8StringFromCFString (hCtx->inHostName, &hostname);
	krb5_init_creds_set_kdc_hostname(hCtx->krb5_ctx, icc, hostname);
	free(hostname);
    }


    if (NULL != usingCertificate) {
	CFStringRef certInferredLabel;
	hx509_cert cert;

	krb_err = hx509_cert_init_SecFramework(hCtx->hx_ctx, usingCertificate, &cert);
	if (krb_err) {
	    err = paramErr; goto Error;
	}

	krb_err = krb5_init_creds_set_pkinit_client_cert(hCtx->krb5_ctx, icc, cert);
	if (krb_err) {
	    err = paramErr; goto Error;
	}
	
	passwordString = NULL;

	certInferredLabel = CFDictionaryGetValue (inClientPrincipalInfo, kKRBCertificateInferredLabelKey);
	if (certInferredLabel)
	    inferredLabel = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@ (%@)"),
						     certInferredLabel, hCtx->inHostName);
    } else {

	CFStringRef clientName;

	KHLog ("    %s: Not using certificate", __func__);

        password  = CFDictionaryGetValue (inClientPrincipalInfo, kKRBClientPasswordKey);
        if (NULL == password) {
	    KHLog ("    %s: No password, cant get tickets", __func__);
	    err = paramErr; goto Error;
	}

	clientName = CFDictionaryGetValue(inClientPrincipalInfo, kKRBUsernameKey);
	if (clientName)
	    inferredLabel = CFStringCreateWithFormat(NULL, NULL, CFSTR("LKDC %@@%@"), clientName, hCtx->inHostName);

	__KRBCreateUTF8StringFromCFString (password, &passwordString);

	krb_err = krb5_init_creds_set_password(hCtx->krb5_ctx, icc, passwordString);
	if (krb_err) {
	    err = paramErr; goto Error;
	}
    }

    krb_err = krb5_init_creds_get(hCtx->krb5_ctx, icc);
    krb5_get_init_creds_opt_free(hCtx->krb5_ctx, opt);
    KHLog ("   %s: krb5_get_init_creds_password: %d", __func__, krb_err);
    if (krb_err != 0) {
	err = paramErr; goto Error;
    }

    krb_err = krb5_init_creds_get_creds(hCtx->krb5_ctx, icc, &cred);
    if (krb_err != 0) {
	err = paramErr; goto Error;
    }

    krb_err = krb5_cc_cache_match(hCtx->krb5_ctx, clientPrincipal, &id);
    if (krb_err) {
	krb_err = krb5_cc_new_unique(hCtx->krb5_ctx, NULL, NULL, &id);
	if (krb_err) {
	    err = paramErr; goto Error;
	}
	destroy_cache = 1;
    }

    krb_err = krb5_cc_initialize(hCtx->krb5_ctx, id, clientPrincipal);
    if (krb_err) {
	err = paramErr; goto Error;
    }

    krb_err = krb5_cc_store_cred(hCtx->krb5_ctx, id, &cred);
    if (krb_err) {
	err = paramErr; goto Error;
    }
    
    krb_err = krb5_init_creds_store_config(hCtx->krb5_ctx, icc, id);
    if (krb_err) {
	err = paramErr; goto Error;
    }

    KRBCredAddReference(principal);

    if (inferredLabel) {
	char *label = NULL;

	KHLog ("%s", "    KRBAcquireTicket setting friendly name");

	if (__KRBCreateUTF8StringFromCFString (inferredLabel, &label) == noErr) {
	    krb5_data data;
	    data.data = label;
	    data.length = strlen(label) + 1;
	    
	    krb5_cc_set_config(hCtx->krb5_ctx, id, NULL, "FriendlyName", &data);
	    free(label);
	}
    }
    {
	krb5_data data;
	data.data = "1";
	data.length = 1;
	krb5_cc_set_config(hCtx->krb5_ctx, id, NULL, "nah-created", &data);
    }

    err = noErr;

 Error:
    if (icc)
	krb5_init_creds_free(hCtx->krb5_ctx, icc);
    if (id) {
	if (err != noErr && destroy_cache)
	    krb5_cc_close(hCtx->krb5_ctx, id);
	else
	    krb5_cc_close(hCtx->krb5_ctx, id);
    }
    krb5_free_cred_contents(hCtx->krb5_ctx, &cred);
    if (NULL != inferredLabel) { CFRelease(inferredLabel); }
    if (NULL != principalString) { __KRBReleaseUTF8String (principalString); }
    if (NULL != passwordString) {  __KRBReleaseUTF8String (passwordString); }
    
    if (NULL != clientPrincipal) { krb5_free_principal(hCtx->krb5_ctx, clientPrincipal); }
    
    KHLog ("]]] KRBAcquireTicket () = %d", (int)err);
    
    return err;
}


/*
  KRBCloseSession will release the kerberos session
  inKerberosSession is the pointer returned by KRBCreateSession.
*/
OSStatus KRBCloseSession(KRBHelperContextRef inKerberosSession)
{
    OSStatus            err = noErr;
    KRBhelperContext    *hCtx = (KRBhelperContext *)inKerberosSession;
    size_t i;
    
    if (NULL == hCtx) { err = paramErr; goto Error; }

    KHLog ("%s", "[[[ KRBCloseSession () - required parameters okay");

    for (i = 0; i < hCtx->realms.len; i++) {
	free(hCtx->realms.data[i].hostname);
	free(hCtx->realms.data[i].realm);
    }
    free(hCtx->realms.data);
   
    if (NULL != hCtx->inAdvertisedPrincipal) { CFRelease (hCtx->inAdvertisedPrincipal); }
    if (NULL != hCtx->hostname)              { CFRelease(hCtx->hostname); }
    if (NULL != hCtx->inHostName)            { CFRelease(hCtx->inHostName); }
    if (NULL != hCtx->realm)                 { CFRelease (hCtx->realm); }

    if (NULL != hCtx->defaultRealm) { free(hCtx->defaultRealm); }
    if (NULL != hCtx->krb5_ctx)     { krb5_free_context (hCtx->krb5_ctx); }
    if (NULL != hCtx->hx_ctx)	    { hx509_context_free(&hCtx->hx_ctx); }
    if (NULL != hCtx->addr)         { freeaddrinfo(hCtx->addr); }
    
    free(hCtx);
 Error:
    KHLog ("]]] KRBCloseSession () = %d", (int)err);

    return err;
}

static OSStatus
findCred(CFStringRef clientPrincipal, krb5_context context, krb5_ccache *id)
{
    krb5_principal client;
    krb5_error_code kret;
    char *str;

    if (__KRBCreateUTF8StringFromCFString (clientPrincipal, &str) != noErr)
	return memFullErr;

    kret = k5_ok(krb5_parse_name(context, str, &client));
    free(str);
    if (0 != kret)
	return memFullErr;

    kret = k5_ok(krb5_cc_cache_match(context, client, id));
    krb5_free_principal(context, client);
    if (0 != kret)
	return memFullErr;

    return noErr;
}

OSStatus
KRBCredChangeReferenceCount(CFStringRef clientPrincipal, int change, int excl)
{
    OSStatus ret = 0;
    krb5_context kcontext = NULL;
    krb5_ccache id = NULL;
    krb5_error_code kret;
    krb5_data data;

    KHLog ("[[[ KRBCredChangeReferenceCount: %d", change);

    kret = k5_ok(krb5_init_context(&kcontext));
    if (0 != kret) {
	ret = memFullErr;
	goto out;
    }

    ret = findCred(clientPrincipal, kcontext, &id);
    if (ret != noErr)
	goto out;

    /* Skip SSO cred-caches */
    ret = k5_ok(krb5_cc_get_config(kcontext, id, NULL, "nah-created", &data));
    if (ret) {
	ret = 0;
	goto out;
    }
    krb5_data_free(&data);

    if (change > 0) 
	ret = krb5_cc_hold(kcontext, id);
    else
	ret = krb5_cc_unhold(kcontext, id);

 out:
    if (id)
	krb5_cc_close(kcontext, id);

    KHLog ("]]] KRBCredChangeReferenceCount: %d", (int)ret);

    if (kcontext)
	krb5_free_context(kcontext);

    return ret;
}

OSStatus KRBCredAddReference(CFStringRef clientPrincipal)
{
    return KRBCredChangeReferenceCount(clientPrincipal, 1, 0);
}

OSStatus KRBCredRemoveReference(CFStringRef clientPrincipal)
{
    return KRBCredChangeReferenceCount(clientPrincipal, -1, 0);
}


OSStatus KRBCredAddReferenceAndLabel(CFStringRef clientPrincipal,
				     CFStringRef identifier)
{
    OSStatus ret = 0;
    krb5_error_code kret;
    krb5_context kcontext = NULL;
    krb5_ccache id = NULL;
    krb5_data data;
    char *label = NULL;

    label = NAHCreateRefLabelFromIdentifier(identifier);
    if (label == NULL) {
	ret = memFullErr;
	goto out;
    }

    KHLog ("%s", "[[[ KRBCredAddReferenceAndLabel");

    kret = k5_ok(krb5_init_context(&kcontext));
    if (0 != kret) {
	ret = memFullErr;
	goto out;
    }

    ret = findCred(clientPrincipal, kcontext, &id);
    if (ret != noErr)
	goto out;

    /* Skip SSO cred-caches */
    kret = k5_ok(krb5_cc_get_config(kcontext, id, NULL, "nah-created", &data));
    if (kret) {
	ret = 0;
	goto out;
    }
    krb5_data_free(&data);

    data.data = (void *)"1";
    data.length = 1;

    kret = krb5_cc_set_config(kcontext, id, NULL, label, &data);
    if (kret) {
	ret = memFullErr;
	goto out;
    }

    ret = krb5_cc_hold(kcontext, id);
    if (ret)
	goto out;

 out:
    KHLog ("]]] KRBCredAddReferenceAndLabel () = %d (label %s)", (int)ret, label);
    if (id)
	krb5_cc_close(kcontext, id);
    if (kcontext)
	krb5_free_context(kcontext);
    if (label)
	free(label);

    return ret;
}

OSStatus
KRBCredFindByLabelAndRelease(CFStringRef identifier)
{
    NAHFindByLabelAndRelease(identifier);
    return noErr;
}

/*
 * Parses the initial request from a SMB server and constructs a
 * resulting CFDictionaryRef.
 *
 *
 */

CFDictionaryRef
KRBDecodeNegTokenInit(CFAllocatorRef alloc, CFDataRef data)
{
    CFMutableDictionaryRef dict = NULL, mechs = NULL;
    union {
	NegotiationToken rfc2478;
	NegotiationTokenWin win;
    } u;
    int win = 0;
    MechTypeList *mechtypes;
    char *hintsname = NULL;
    gss_buffer_desc input_buffer, output_buffer = { 0, NULL };
    OM_uint32 junk;
    int ret;

    input_buffer.value = (void *)CFDataGetBytePtr(data);
    input_buffer.length = CFDataGetLength(data);

    junk = gss_decapsulate_token(&input_buffer, GSS_SPNEGO_MECHANISM, &output_buffer);
    if (junk)
	goto out;

    memset(&u, 0, sizeof(u));
    ret = decode_NegotiationToken(output_buffer.value, output_buffer.length, &u.rfc2478, NULL);
    if (ret == 0) {
	if (u.rfc2478.element != choice_NegotiationToken_negTokenInit)
	    goto out;

	mechtypes = &u.rfc2478.u.negTokenInit.mechTypes;
    } else {
	win = 1;

	memset(&u, 0, sizeof(u));

	ret = decode_NegotiationTokenWin(output_buffer.value, output_buffer.length, &u.win, NULL);
	if (ret)
	    goto out;

	if (u.win.element != choice_NegotiationTokenWin_negTokenInit)
	    goto out;

	mechtypes = &u.win.u.negTokenInit.mechTypes;

	if (u.win.u.negTokenInit.negHints && u.win.u.negTokenInit.negHints->hintName)
	    hintsname = *(u.win.u.negTokenInit.negHints->hintName);
    }

    dict = CFDictionaryCreateMutable(alloc, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (dict == NULL)
	goto out;

    if (mechtypes) {
	CFDataRef empty;
	unsigned n;

	mechs = CFDictionaryCreateMutable(alloc, mechtypes->len,
					  &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (mechs == NULL)
	    goto out;

	CFDictionaryAddValue(dict, kSPNEGONegTokenInitMechs, mechs);
	CFRelease(mechs); /* dict have a ref */

	empty = CFDataCreateWithBytesNoCopy(alloc, NULL, 0, kCFAllocatorNull);
	if (empty == NULL)
	    goto out;

	for (n = 0; n < mechtypes->len; n++) {
	    char *str;
	    ret = der_print_heim_oid(&mechtypes->val[n], '.', &str);
	    if (ret)
		continue;

	    CFStringRef s = CFStringCreateWithCString(alloc, str, kCFStringEncodingUTF8);
	    free(str);
	    if (s) {
		CFDictionaryAddValue(mechs, s, empty);
		CFRelease(s);
	    } else {
		CFRelease(empty);
		CFRelease(dict);
		dict = NULL;
		goto out;
	    }
	}				     
	CFRelease(empty);
    }
    if (hintsname) {
	CFStringRef s = CFStringCreateWithCString(alloc, hintsname, kCFStringEncodingUTF8);
	if (s) {
	    CFDictionaryAddValue(dict, kSPNEGONegTokenInitHintsHostname, s);
	    CFRelease(s);
	} else {
	    CFRelease(dict);
	    dict = NULL;
	    goto out;
	}
    }
    
    if (mechs) {
	if (CFDictionaryGetValue(mechs, kGSSAPIMechSupportsAppleLKDC))
	    CFDictionaryAddValue(dict, KSPNEGOSupportsLKDC, CFSTR("yes"));
    }

 out:
    gss_release_buffer(&junk, &output_buffer);
    if (win)
	free_NegotiationTokenWin(&u.win);
    else
	free_NegotiationToken(&u.rfc2478);
    
    return dict;
}

static CFDictionaryRef
CreateNegTokenLegacyMech(CFAllocatorRef alloc,
			 CFStringRef mech,
			 CFDataRef value,
			 bool support_wlkdc)
{
    CFMutableDictionaryRef dict = NULL;
    CFMutableDictionaryRef mechs;

    dict = CFDictionaryCreateMutable(alloc, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (dict == NULL)
	return NULL;

    mechs = CFDictionaryCreateMutable(alloc, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (mechs == NULL) {
	CFRelease(dict);
	return NULL;
    }

    CFDictionaryAddValue(dict, kSPNEGONegTokenInitMechs, mechs);
    CFRelease(mechs);

    CFDictionaryAddValue(mechs, mech, value);
    if (support_wlkdc)
	CFDictionaryAddValue(mechs, kGSSAPIMechSupportsAppleLKDC, CFSTR("yes"));

    return dict;
}


/*
 * Create a NegToken reference that only contains a Kerberos mech
 */

CFDictionaryRef
KRBCreateNegTokenLegacyKerberos(CFAllocatorRef alloc)
{
    CFDictionaryRef dict;
    CFDataRef empty;

    empty = CFDataCreateWithBytesNoCopy(alloc, (void *)"", 0, kCFAllocatorNull);
    if (empty == NULL)
	return NULL;

    dict = CreateNegTokenLegacyMech(alloc, kGSSAPIMechKerberosOID, empty, false);
    CFRelease(empty);
    return dict;
}

/*
 * Create a NegToken reference that only contains a NTLM mech, also
 * hint that this is a raw NTLM (w/o SPNEGO wrappings).
 */

CFDictionaryRef
KRBCreateNegTokenLegacyNTLM(CFAllocatorRef alloc)
{
    CFDictionaryRef dict;
    CFDataRef empty;

    empty = CFDataCreateWithBytesNoCopy(alloc, (void *)"raw", 3, kCFAllocatorNull);
    if (empty == NULL)
	return NULL;

    dict = CreateNegTokenLegacyMech(alloc, kGSSAPIMechNTLMOID, empty, false);
    if (empty)
	CFRelease(empty);
    return dict;
}

/*
  ;; Local Variables: **
  ;; tab-width: 8 **
  ;; c-basic-offset: 4 **
  ;; End: **
*/
