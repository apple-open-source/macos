/*
 *  KerberosHelper.c
 *  KerberosHelper
 */

/*
 * Copyright (c) 2006-2007 Apple Inc. All rights reserved.
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

#include "KerberosHelper.h"

#include "KerberosHelperContext.h"

#include <Kerberos/KerberosLogin.h>
#include <Kerberos/pkinit_cert_store.h>

#include "LKDCHelper.h"

#include <Carbon/Carbon.h>
#include <Security/Security.h>
#include <Security/SecCertificatePriv.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <asl.h>

#define DEBUG 0  /* Set to non-zero for more debug spew than you want. */

static krb5_error_code
_k5_check_err(krb5_error_code error, const char *function, const char *file, int line)
{
	if (error)
		asl_log(NULL, NULL, ASL_LEVEL_DEBUG, "    %s: krb5 call got %d (%s) on %s:%d", function, error, error_message(error), file, line);
    return error;
}

// static krb5_error_code _k5_check_err(krb5_error_code error, const char *function, const char *file, int line);
#define k5_ok(x) _k5_check_err (x, __func__, __FILE__, __LINE__)

#define KHLog(FMT, ...)		asl_log(NULL, NULL, ASL_LEVEL_DEBUG, FMT, __VA_ARGS__)

static const char lkdc_prefix[] = "LKDC:";

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

/* Result string in `*realm' is owned by the caller and must be free()d. */
static int
realm_for_host(krb5_context ctx, const char *hostname, const char *hintrealm,
    char **realm)
{
	char **realmlist = NULL;
	int err = 0;

	KHLog ("[[[ %s: hostname=%s hintrealm=%s", __func__, hostname, NULL == hintrealm ? "(null)" : hintrealm);

	/* If krb5_get_host_realm succeeds, use its result. */
	/* Otherwise, if the hint realm is for a Local KDC or not given,
	 * attempt LKDCDiscoverRealm.
	 */
	*realm = NULL;
	if (0 == k5_ok( krb5_get_host_realm (ctx, hostname, &realmlist))) {
		if (NULL != realmlist && NULL != realmlist[0] && '\0' != *realmlist[0] &&
		  NULL != (*realm = strdup(*realmlist))) {
			KHLog ("    %s: krb5_get_host_realm success", __func__);
			goto fin;
		} else
			KHLog ("    %s: krb5_get_host_realm returned unusable realm!", __func__);
	}
	if ((NULL == hintrealm || 0 == strncmp(hintrealm, lkdc_prefix, sizeof(lkdc_prefix)-1)) &&
	  0 == LKDCDiscoverRealm(hostname, realm)) {
		KHLog ("    %s: LKDCDiscoverRealm success", __func__);
		goto fin;
	}
	err = -1;

fin:
	if (NULL != realmlist)
		krb5_free_host_realm (ctx, realmlist);
	/* Returns 0 when successful, non-zero otherwise. */
	if (0 == err)
		KHLog("]]] %s: returning realm=%s", __func__, *realm);
	else
		KHLog("]]] %s: failed to determine realm", __func__);
	return err;
}

static int
host_matches_hint(const char *hostname, const char *realm, const char *hinthost,
    const char *hintrealm)
{
	/* Returns 1 if the hostname and realm match the hints,
	 * zero otherwise.
	 */
	if (NULL == hinthost || NULL == hintrealm)
		return 0;
	else if (0 == strncmp(hinthost, lkdc_prefix, sizeof(lkdc_prefix)-1) &&
	  0 == strcmp(realm, hinthost))
		return 1;
	else if (0 == strcasecmp(hostname, hinthost) && 0 == strcmp(realm, hintrealm))
		return 1;
	else
		return 0;
}

struct host_realm_mapping {
	char *hostname;
	char *realm;
};

static int
set_host_realm_mapping(struct host_realm_mapping *p, const char *hostname, const char *realm)
{
	struct host_realm_mapping tmp = { strdup (hostname), strdup (realm) };
	if (NULL == tmp.hostname || NULL == tmp.realm) {
		free (tmp.hostname);
		free (tmp.realm);
		return -1;
	} else {
		free (p->hostname);
		free (p->realm);
		*p = tmp;
		return 0;
	}
}

static inline void
free_host_realm_mapping(struct host_realm_mapping *p)
{
	free (p->hostname);
	p->hostname = NULL;
	free (p->realm);
	p->realm = NULL;
}

static int
parse_principal_name(__unused krb5_context ctx, const char *princname, char **namep, char **instancep, char **realmp)
{
	KLPrincipal principal = NULL;
	char *name = NULL, *instance = NULL, *realm = NULL;
	int err = 0;

	KHLog ("[[[ %s () decomposing %s", __func__, princname);
	*namep = *instancep = *realmp = NULL;
	if (klNoErr != KLCreatePrincipalFromString (princname, kerberosVersion_V5, &principal)) {
		err = -1;
		goto fin;
	}
	if (klNoErr != KLGetTripletFromPrincipal (principal, &name, &instance, &realm)) {
		err = -2;
		goto fin;
	}
	if (NULL == (*namep = strdup(name)) || NULL == (*instancep = strdup(instance)) || NULL == (*realmp = strdup(realm))) {
		err = -3;
		goto fin;
	}

fin:
	if (NULL != principal)
		KLDisposePrincipal (principal);
	if (NULL != name)
		KLDisposeString (name);
	if (NULL != instance)
		KLDisposeString (instance);
	if (NULL != realm)
		KLDisposeString (realm);
	KHLog ("]]] %s () - %d", __func__, err);
	return err;
}

OSStatus
KRBCreateSession(CFStringRef inHostName, CFStringRef inAdvertisedPrincipal,
    void **outKerberosSession)
{
	char hbuf[NI_MAXHOST];
	struct host_realm_mapping primaryMatch = { NULL, NULL }, secondaryMatch = { NULL, NULL }, *matchp = NULL;
	struct addrinfo hints, *res = NULL, *aip = NULL;
	KRBhelperContext *hCtx = NULL;
	char *tmp = NULL;
	char *hintname = NULL, *hinthost = NULL, *hintrealm = NULL;
	char *localname = NULL, *hostname = NULL, *realm = NULL;
	OSStatus err = noErr;
	int avoidDNSCanonicalizationBug = 0;

	if (NULL == outKerberosSession) {
		err = paramErr;
		goto fin;
	}
	KHLog ("[[[ %s () - required parameters okay", __func__);

	/* Create the context that we will return on success.
	 * We almost always require a Kerberos context and the default
	 * realm, so populate those up front.
	 */
	if (NULL == (hCtx = calloc(1, sizeof(*hCtx)))) {
		err = memFullErr;
		goto fin;
	}

	/* If no host name was given, then the caller is inquiring
	 * about the Local KDC realm.  Our work here is done.
	 */
	if (NULL == inHostName) {
		err = DSCopyLocalKDC (&hCtx->realm);
		KHLog ("    %s: LocalKDC realm lookup only", __func__);
		goto fin;
	}

	if (0 != k5_ok( krb5_init_context (&hCtx->krb5_ctx) )) {
		err = memFullErr;
		goto fin;
	}
	if (0 == k5_ok( krb5_get_default_realm (hCtx->krb5_ctx, &tmp) ) && NULL != tmp) {
		if (NULL == (hCtx->defaultRealm = strdup (tmp))) {
			err = memFullErr;
			goto fin;
		}
		krb5_free_default_realm (hCtx->krb5_ctx, tmp);
	}

	/* Retain any given host name in the context.  If we find a
	 * "better" host name, we will replace it in the context before
	 * returning.
	 */
	if (NULL != inHostName)
		hCtx->inHostName = CFRetain (inHostName);

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

	/* Decode the given host name with
	 * _CFNetServiceDeconstructServiceName before proceeding.  The
	 * first argument to _CFNetServiceDeconstructServiceName is an
	 * in-out parameter.  It may or may not be updated.  However, if
	 * it is updated, the previous value will not have been released.
	 */

#if DEBUG
	__KRBCreateUTF8StringFromCFString (inHostName, &tmp);
	if (NULL != tmp) {
		KHLog ("    %s: raw host name = %s", __func__, tmp);
		free (tmp);
		tmp = NULL;
	}
#endif

	if (! _CFNetServiceDeconstructServiceName (&inHostName, &hostname))
		__KRBCreateUTF8StringFromCFString (inHostName, &hostname);
	else
		avoidDNSCanonicalizationBug = 1;

	if (inHostName != hCtx->inHostName) {
		CFRelease (hCtx->inHostName);
		hCtx->inHostName = inHostName;
	}
	KHLog ("    %s: processed host name = %s", __func__, hostname);

	/* If the given name is a bare name (i.e. no dots), we may need
	 * to attempt to look it up as `<name>.local' later.
	 */
	if (NULL == strchr(hostname, '.') &&
	  (0 > asprintf(&localname, "%s.local", hostname) || NULL == localname)) {
		err = memFullErr;
		goto fin;
	}

	/* Normalize the given host name using getaddrinfo AI_CANONNAME if
	 * possible. Track the resulting host name (normalized or not) as
	 * `hostname'.
	 */
	
	/* Avoid canonicalization if possible because of <rdar://problem/5517187> getaddrinfo with hints.ai_flags = AI_CANONNAME mangles quoted DNS Names. */
	char lastChar = hostname[strlen(hostname)-1];
	KHLog ("    %s: last char of host name = 0x%02x", __func__, lastChar);
		
	if (avoidDNSCanonicalizationBug == 0 && '.' != lastChar) {
		memset (&hints, 0, sizeof(hints));
		hints.ai_flags = AI_CANONNAME;
		err = getaddrinfo (hostname, NULL, &hints, &res);
		KHLog ("    %s: getaddrinfo = %s (%d)", __func__, 0 == err ? "success" : gai_strerror (err), err);
		if (0 == err && NULL != res->ai_canonname && NULL != (tmp = strdup (res->ai_canonname))) {
			/* Use the canonical name */
			free (hostname);
			hostname = tmp;
			KHLog ("    %s: canonical host name = %s", __func__, hostname);
		}
	}

	/* Now attempt to find a host name (a) for which we can
	 * determine a realm mapping and (b) matches the advertised
	 * principal name, if any.  Track such a host name as
	 * `primaryMatch'.  If we cannot satisfy (b), then use the
	 * first name that satisfies (a).  Track such a host name as
	 * `secondaryMatch'.  If we cannot satisfy (a), then fail---
	 * Kerberos authentication cannot succeed.
	 */

	/* If `hostname' satisfies (a), it is the `secondaryMatch'. */
	if (0 == realm_for_host (hCtx->krb5_ctx, hostname, hintrealm, &realm)) {
		if (0 != set_host_realm_mapping (&secondaryMatch, hostname, realm)) {
			err = memFullErr;
			goto fin;
		}
		free (realm);
		realm = NULL;
		KHLog ("    %s: secondary match = %s", __func__, secondaryMatch.hostname);
	}

	/* If `hostname' satisfies (b) also, it is the `primaryMatch'. */
	if (NULL != secondaryMatch.hostname &&
	  host_matches_hint(secondaryMatch.hostname, secondaryMatch.realm, hinthost, hintrealm)) {
		primaryMatch = secondaryMatch;
		secondaryMatch.hostname = secondaryMatch.realm = NULL;
		KHLog ("    %s: primary match = %s", __func__, primaryMatch.hostname);
	}
	free (hostname);
	hostname = NULL;

	/* If we do not have a `primaryMatch', then go through the
	 * addresses returned earlier by getaddrinfo.  For each
	 * address, use getnameinfo to resolve into a name.  If we do
	 * not already have a `secondaryMatch' and the name satisfies
	 * (a), then it is the `secondaryMatch'.  If the name
	 * satisfies (b) also, then it is the `primaryMatch'.  If
	 * an advertised principal name was given, continue
	 * iterating through the addresses until a `primaryMatch' is
	 * found.  Otherwise, iterate only until a `secondaryMatch'
	 * is found.
	 */
	for (aip = res; NULL != aip &&
		   NULL == primaryMatch.hostname && (NULL != hinthost || NULL == secondaryMatch.hostname);
		 aip = aip->ai_next) {
		err = getnameinfo (aip->ai_addr, aip->ai_addr->sa_len, hbuf, sizeof(hbuf), NULL, 0, NI_NAMEREQD);
#if DEBUG
		KHLog ("    %s: getnameinfo result %d %s", __func__, err, 0 == err ? "success" : gai_strerror (err));
#endif
		if (0 != err)
			continue;
#if DEBUG
		char ipbuf[NI_MAXHOST];
		const void *addr;
		switch (aip->ai_family) {
		case AF_INET:
			addr = &((struct sockaddr_in *)aip->ai_addr)->sin_addr;
			break;
		case AF_INET6:
			addr = &((struct sockaddr_in6 *)aip->ai_addr)->sin6_addr;
			break;
		default:
			addr = NULL;
			break;
		}
		if (NULL == addr || NULL == inet_ntop (aip->ai_family, addr, ipbuf, sizeof(ipbuf)))
			KHLog ("    %s: inet_ntop failed %s", __func__, NULL == addr ? "unknown address family" : strerror (errno));
		else
			KHLog ("    %s: %s -> %s", __func__, ipbuf, hbuf);
#endif
		if (0 == realm_for_host (hCtx->krb5_ctx, hbuf, hintrealm, &realm)) {
			if (NULL == secondaryMatch.hostname) {
				if (0 != set_host_realm_mapping (&secondaryMatch, hbuf, realm)) {
					err = memFullErr;
					goto fin;
				}
				KHLog ("    %s: secondary match = %s", __func__, secondaryMatch.hostname);
			}
			if (NULL == primaryMatch.hostname && host_matches_hint(hbuf, realm, hinthost, hintrealm)) {
				if (0 != set_host_realm_mapping (&primaryMatch, hbuf, realm)) {
					err = memFullErr;
					goto fin;
				}
				KHLog ("    %s: primary match = %s", __func__, primaryMatch.hostname);
			}
			free (realm);
			realm = NULL;
		}
	}

	/* If we do not have a `primaryMatch' at this point, but the
	 * given host name was a bare name (i.e. no dots), make one
	 * last attempt by trying the name with `.local' appended.
	 * If we can find a realm in that fashion, then use the given
	 * name with `.local' appended as the `primaryMatch'.
	 */
	if (NULL == primaryMatch.hostname && NULL != localname &&
	  0 == realm_for_host (hCtx->krb5_ctx, localname, hintrealm, &realm)) {
		if (0 != set_host_realm_mapping (&primaryMatch, localname, realm)) {
			err = memFullErr;
			goto fin;
		}
		KHLog ("    %s: primary match = %s", __func__, primaryMatch.hostname);
		free (realm);
		realm = NULL;
	}

	/* Store the `primaryMatch' (or `secondaryMatch' if there is no
	 * `primaryMatch') as the host name in the context.  If by now we
	 * have no `primaryMatch' nor `secondaryMatch', then fail---
	 * Kerberos cannot be used.
	 */
	if (NULL != primaryMatch.hostname)
		matchp = &primaryMatch;
	else if (NULL != secondaryMatch.hostname)
		matchp = &secondaryMatch;
	else {
		KHLog ("    %s: could not find a suitable host/realm mapping", __func__);
		err = paramErr;
		goto fin;
	}
	KHLog ("    %s: Using host name = %s, realm = %s", __func__, matchp->hostname, matchp->realm);
	CFRelease (hCtx->inHostName);
	if (NULL == (hCtx->inHostName =
	  CFStringCreateWithCString (kCFAllocatorDefault, matchp->hostname, kCFStringEncodingASCII)) ||
	  NULL == (hCtx->realm = CFStringCreateWithCString (kCFAllocatorDefault, matchp->realm, kCFStringEncodingASCII))) {
		err = memFullErr;
		goto fin;
	}

fin:
	free (hintname);
	free (hinthost);
	free (hintrealm);
	free (hostname);
	free (localname);
	free (realm);
	if (NULL != res)
		freeaddrinfo (res);
	free_host_realm_mapping (&primaryMatch);
	free_host_realm_mapping (&secondaryMatch);

	/* On error, free all members of the context and the context
	 * itself.
	 */
	if (noErr != err) {
		free (hCtx->defaultRealm);
		if (NULL != hCtx->realm)
			CFRelease (hCtx->realm);
		if (NULL != hCtx->inAdvertisedPrincipal)
			CFRelease (hCtx->inAdvertisedPrincipal);
		if (NULL != hCtx->inHostName)
			CFRelease (hCtx->inHostName);
		if (NULL != hCtx->krb5_ctx)
			krb5_free_context (hCtx->krb5_ctx);
		free (hCtx);
	} else
		*outKerberosSession = hCtx;

	KHLog ("]]] %s () = %d", __func__, err);
	return err;
}

/*

	KRBCopyREALM will return the REALM for the host that was passed to KRBCreateSession
		inKerberosSession is the pointer returned by KRBCreateSession
		outREALM is the REALM of the host
*/
OSStatus KRBCopyRealm(void *inKerberosSession, CFStringRef *outRealm)
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
	KHLog ("]]] KRBCopyRealm () = %d", err);

	return err;
}

/*
	KRBCopyKeychainLookupInfo will return a dictionary containing information related to Kerberos and keychain items.
		inKerberosSession is the pointer returned by KRBCreateSession
		inUsername is an available and usable Username or NULL
		outKeychainLookupInfo is a dictionary containing keychain lookup info and if it is acceptable to store a
			password in the keychain.
			
		outKeychainLookupInfo
			kKRBUsernameKey					: CFStringRef
			kKRBKeychainAccountName			: CFStringRef
			kKRBDisableSaveToKeychainKey	: CFBooleanRef
		
*/	
OSStatus KRBCopyKeychainLookupInfo (void *inKerberosSession, CFStringRef inUsername, CFDictionaryRef *outKeychainLookupInfo)
{
	OSStatus				err = noErr;
	KRBhelperContext		*hCtx = (KRBhelperContext *)inKerberosSession;
	CFMutableDictionaryRef	outInfo = NULL;
	CFStringRef				accountName = NULL;

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
		CFDictionarySetValue (outInfo, kKRBKeychainAccountName, accountName);
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
	KHLog ("]]] KRBCopyKeychainLookupInfo () = %d", err);
	
	return err;
}

/*
	KRBCopyServicePrincipal will return the service principal for a service on the host given inInstance.
		inKerberosSession is the pointer returned by KRBCreateSession
		inServiceName is the name of the service on the host, it can be NULL if inAdvertisedPrincipal was non-NULL.  
			However it is highly recommended that this be set as it is insecure to rely on remotely provided information 
		outServicePrincipal the service principal
 */
OSStatus KRBCopyServicePrincipal (void *inKerberosSession, CFStringRef inServiceName, CFStringRef *outServicePrincipal)
{
	OSStatus			err = noErr;
	KRBhelperContext	*hCtx = (KRBhelperContext *)inKerberosSession;

	KLPrincipal		svcPrincipal = NULL;
	char			*advertisedPrincipalString = NULL, *inServiceString = NULL;
	char			*svcName = NULL, *svcInstance = NULL, *svcRealm = NULL;
	char			*useName = NULL, *useInstance = NULL, *useRealm = NULL;
	char			*inHostNameString = NULL, *realmString = NULL;
	CFStringRef		outString;
		
	if (NULL == hCtx || NULL == outServicePrincipal) { err = paramErr; goto Done; }
	*outServicePrincipal = NULL;

	KHLog ("%s", "[[[ KRBCopyServicePrincipal () - required parameters okay");

	if (NULL != inServiceName) {
		__KRBCreateUTF8StringFromCFString (inServiceName, &inServiceString);
	}
	
	if (NULL != hCtx->realm) {
		__KRBCreateUTF8StringFromCFString (hCtx->realm, &realmString);
	}
#if DEBUG
	if (NULL != inServiceString) KHLog("    KRBCopyServicePrincipal: inServiceNameString=%s", inServiceString);
	if (NULL != realmString) KHLog("    KRBCopyServicePrincipal: realmString=%s", realmString);
#endif
	if (NULL != hCtx->inAdvertisedPrincipal) {
		__KRBCreateUTF8StringFromCFString (hCtx->inAdvertisedPrincipal, &advertisedPrincipalString);
#if DEBUG
		if (NULL != advertisedPrincipalString) KHLog("    KRBCopyServicePrincipal: advertisedPrincipalString=%s", advertisedPrincipalString);
#endif
		KLCreatePrincipalFromString (advertisedPrincipalString, kerberosVersion_V5, &svcPrincipal);

		if (KLGetTripletFromPrincipal(svcPrincipal, &svcName, &svcInstance, &svcRealm) == klNoErr) {
			useName = svcName;
			if (inServiceString) { useName = inServiceString; }

			if (NULL != inServiceString && 0 != strcmp (svcName, inServiceString)) {
				/* Huh, the advertized name and the one we think it is are different! */
				/* Should probably do something about this, however the code will work "safely" as written. */
				KHLog ("    KRBCopyServicePrincipal: svcName mismatch inService = \"%s\", svcName = \"%s\"", inServiceString, svcName);
			}

			KHLog ("    KRBCopyServicePrincipal: useName = \"%s\"", useName);

			if (0 == strncmp (svcRealm, "LKDC:", 5)) {
				/* The LKDC realm should match the one we've looked up before */
				if (0 != strcmp (svcRealm, realmString)) {
					/* Huh, the advertized realm does not match what is in DNS */
					/* Should probably do something about this, however the code will work "safely" as written. */
				}
				KHLog ("    KRBCopyServicePrincipal: realm is Local KDC.", NULL);
				useRealm = useInstance = realmString;
			} else {
				/* A managed realm was advertized.  We now need to see if we
				 * also belong to this realm before choosing it.
				 * However, for now we have to always choose the managed realm
				 * because I cannot tell what our configured realms are at the
				 * moment.
				 */ 
				useRealm = svcRealm;

				if (NULL != hCtx->inHostName) {
					__KRBCreateUTF8StringFromCFString (hCtx->inHostName, &inHostNameString);
#if DEBUG
					KHLog ("    KRBCopyServicePrincipal: inHostNameString = %s", inHostNameString);
#endif
				} else {
					KHLog ("    KRBCopyServicePrincipal: Bad inHostName, using svcInstance = \"%s\"", svcInstance);
					inHostNameString = strdup (svcInstance);
				}
				useInstance = inHostNameString;
			}

			KHLog ("    KRBCopyServicePrincipal: useInstance = \"%s\"", useInstance);
		}
		
		if (svcPrincipal) { KLDisposePrincipal (svcPrincipal); }
		
	} else {
		/* We got no hints from the calling application */
		if (realmString) {
			useRealm = realmString;
			useName = inServiceString;

			if (0 == strncmp (useRealm, "LKDC:", 5)) {
				useInstance = useRealm;
			} else {
				if (NULL != hCtx->inHostName) {
					__KRBCreateUTF8StringFromCFString (hCtx->inHostName, &inHostNameString);
					useInstance = inHostNameString;
				} else {
					KHLog ("%s", "    KRBCopyServicePrincipal: Fatal - Bad inHostName & no inAdvertisedPrincipal");
					useInstance = NULL;
				}
			}
		}
	}

	if (NULL != useName && NULL != useInstance && NULL != useRealm) {
		outString = CFStringCreateWithFormat (NULL, NULL, CFSTR("%s/%s@%s"), useName, useInstance, useRealm);
		hCtx->useName		= strdup (useName);
		hCtx->useInstance	= strdup (useInstance);
		hCtx->useRealm		= strdup (useRealm);

		KHLog ("    KRBCopyServicePrincipal: principal = \"%s/%s@%s\"", useName, useInstance, useRealm);
	} else {
		err = paramErr;
	}

	*outServicePrincipal = outString; 
	
	if (svcInstance) { KLDisposeString (svcInstance); }
	if (svcRealm)	 { KLDisposeString (svcRealm); }
	if (svcName)	 { KLDisposeString (svcName); }

	if (advertisedPrincipalString) { __KRBReleaseUTF8String (advertisedPrincipalString); }
	if (inHostNameString)		   { __KRBReleaseUTF8String (inHostNameString); }	
	if (inServiceString)		   { __KRBReleaseUTF8String (inServiceString); }
	if (realmString)			   { __KRBReleaseUTF8String (realmString); }

Done:
	KHLog ("]]] KRBCopyServicePrincipal () = %d", err);

	return err;
}

/*
	KRBCopyClientPrincipalInfo will return a dictionary with the user principal and other information.
		inKerberosSession is the pointer returned by KRBCreateSession.
		inOptions a dictionary with options regarding the acquisition of the user principal.
		inIdentityRef is a reference to list of usable identities
		outClientPrincipalInfo a dictionary containing the user principal and other information necessary to get a ticket.
 
		inOptions Dictionary Keys
			kKRBAllowKerberosUIKey			: CFStringRef [See AllowKeberosUI values]
			kKRBServerDisplayNameKey		: CFStringRef
			kKRBUsernameKey					: CFStringRef
			kKRBClientPasswordKey			: CFStringRef
			kKRBCertificateKey				: SecCertificateRef
 
		outClientPrincipalInfo
			kKRBClientPrincipalKey			: CFStringRef
			kKRBUsernameKey					: CFStringRef
			and private information
 */

OSStatus KRBCopyClientPrincipalInfo (void *inKerberosSession,  CFDictionaryRef inOptions, CFDictionaryRef *outClientPrincipalInfo)
{
	OSStatus			err = noErr;
	KRBhelperContext	*hCtx = (KRBhelperContext *)inKerberosSession;

	CFIndex					newCount;
	CFMutableDictionaryRef	outInfo = NULL;
	CFStringRef				clientPrincipal = NULL;
	char					*clientPrincipalString = NULL;
	char					*useRealm = NULL;
	CFStringRef				useClientName = NULL;
	CFStringRef				inferredLabel = NULL;
	SecCertificateRef certRef = NULL;
	CFStringRef				certificateHash = NULL;
	char *cert_hash = NULL;
	int	usingCertificate = 0;
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
		CSSM_DATA certData;
		krb5_data kcert;
		CFStringRef description = NULL;
		
		/* get the cert data */
		err = SecCertificateGetData(certRef, &certData);
		if (0 != err) { goto Error; }
		
		kcert.magic = 0;
		kcert.length = certData.Length;
		kcert.data = (char *)certData.Data;

		cert_hash = krb5_pkinit_cert_hash_str(&kcert);
		if (NULL == cert_hash) { goto Error; }

		useClientName = CFStringCreateWithCString (NULL, cert_hash, kCFStringEncodingASCII);
		usingCertificate = 1;

		if (NULL != useClientName) {
			certificateHash = CFRetain (useClientName);
		}
		
		SecCertificateCopySubjectComponent (certRef, &CSSMOID_Description, &description);

		if (NULL != description && kCFCompareEqualTo == CFStringCompare(description, CFSTR (".Mac Sharing Certificate"), 0)) {
			CFStringRef	commonName = NULL, organizationalUnit = NULL;
			
			SecCertificateCopyCommonName (certRef, &commonName);
			SecCertificateCopySubjectComponent (certRef, &CSSMOID_OrganizationalUnitName, &organizationalUnit);

			if (NULL != commonName && NULL != organizationalUnit) {
				inferredLabel = CFStringCreateWithFormat (NULL, NULL, CFSTR("%@@%@"), commonName, organizationalUnit);
			}
				
			if (NULL != commonName) { CFRelease (commonName); }
			if (NULL != organizationalUnit) { CFRelease (organizationalUnit); }
		} else {
			err = SecCertificateInferLabel (certRef, &inferredLabel);
		}

		if (NULL != description) { CFRelease (description); }
			
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
		size_t	useRealmLength = strlen (hCtx->useRealm);
		size_t	defaultRealmLength = strlen (hCtx->defaultRealm);
		
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
		krb5_context kcontext;
		krb5_error_code krb_err = 0;
		cc_context_t cc_context = NULL;
		cc_ccache_iterator_t iterator = NULL;
		const char *alternateRealm = NULL;
		const char *clientRealm = principal_realm(clientPrincipalString);
		char		*alternateClientPrincipal = NULL;
		char		*bestClientPrincipal = NULL;
		int commonSubrealms = 1, tmp;

		krb_err = k5_ok (krb5_init_context(&kcontext));

		if (!krb_err) {
			krb_err = k5_ok (cc_initialize (&cc_context, ccapi_version_4, NULL, NULL));
		}
		if (!krb_err) {
			krb_err = k5_ok (cc_context_new_ccache_iterator (cc_context, &iterator));
		}

		/* We exit if we find more than one match */
		while (!krb_err) {
			cc_ccache_t   cc_ccache = NULL;
			cc_string_t   ccacheName = NULL;
			krb5_ccache ccache = NULL;
			krb5_principal ccachePrinc = NULL;
			
			krb_err = k5_ok (cc_ccache_iterator_next (iterator, &cc_ccache));

			if (!krb_err) { krb_err = k5_ok (cc_ccache_get_name (cc_ccache, &ccacheName)); }

			if (!krb_err) { krb_err = k5_ok (krb5_cc_resolve (kcontext, ccacheName->data, &ccache)); }

			if (!krb_err) { krb_err = k5_ok (krb5_cc_get_principal (kcontext, ccache, &ccachePrinc)); }

			if (!krb_err) {	krb_err = k5_ok (krb5_unparse_name (kcontext, ccachePrinc, &alternateClientPrincipal)); }

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
					break;
				} else {
					/* Inexact match (has_common_subrealm set tmp nonzero.)
					 * Keep track of the number of components matched.
					 */
					commonSubrealms = tmp;
				}
			}

			if (NULL != alternateClientPrincipal) {
				krb5_free_unparsed_name(kcontext, alternateClientPrincipal);
				alternateClientPrincipal = NULL;
			}
			if (NULL != ccache)      { krb5_cc_close (kcontext, ccache); }                
			if (NULL != ccacheName)  { cc_string_release (ccacheName); }
			if (NULL != cc_ccache)   { cc_ccache_release (cc_ccache); }
			if (NULL != ccachePrinc) { krb5_free_principal (kcontext, ccachePrinc); }
		}

		if (NULL != iterator)    { cc_ccache_iterator_release (iterator); }
		if (NULL != cc_context)  { cc_context_release (cc_context); }

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
		if (NULL != alternateClientPrincipal) { krb5_free_unparsed_name (kcontext, alternateClientPrincipal); }
		if (NULL != kcontext)                 { krb5_free_context (kcontext); }
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
	if (NULL != cert_hash)	     { free (cert_hash); }
	if (NULL != clientPrincipal) { CFRelease (clientPrincipal); }
	if (NULL != clientPrincipalString) { __KRBReleaseUTF8String (clientPrincipalString); }

	KHLog ("]]] KRBCopyClientPrincipalInfo () = %d", err);
	
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
OSStatus KRBTestForExistingTicket (void *inKerberosSession, CFDictionaryRef inClientPrincipalInfo)
{
	OSStatus			err = noErr;
	KRBhelperContext	*hCtx = (KRBhelperContext *)inKerberosSession;
	CFStringRef		clientPrincipal = NULL;
	char			*principalString = NULL;

	if (NULL == hCtx || NULL == inClientPrincipalInfo) { err = paramErr; goto Done; }

	KHLog ("%s", "[[[ KRBTestForExistingTicket () - required parameters okay");

	CFDictionaryGetValueIfPresent (inClientPrincipalInfo, kKRBClientPrincipalKey, (const void **)&clientPrincipal);

	if (NULL != clientPrincipal) {
		KLStatus	krb_err = klNoErr;
		KLLoginOptions	loginOptions = NULL;
		KLPrincipal	klPrincipal = NULL;
		KLBoolean	outFoundValidTickets = FALSE;
		KLPrincipal	outPrincipal = NULL;
		char		*ccacheName = NULL;

		__KRBCreateUTF8StringFromCFString (clientPrincipal, &principalString);
		KHLog ("    KRBTestForExistingTicket: principal = \"%s\"", principalString);

		krb_err = KLCreateLoginOptions (&loginOptions);

		KLCreatePrincipalFromString (principalString, kerberosVersion_V5, &klPrincipal);

		krb_err = KLCacheHasValidTickets (klPrincipal, kerberosVersion_V5, &outFoundValidTickets, &outPrincipal, &ccacheName);

		if (TRUE == outFoundValidTickets) {
			KHLog ("    KRBTestForExistingTicket: Valid Ticket, ccacheName = \"%s\"", ccacheName);
			err = 0;
		} else {
			err = krb_err;
		}

		if (NULL != loginOptions)  { KLDisposeLoginOptions (loginOptions); }
		if (NULL != principalString) { __KRBReleaseUTF8String (principalString); }

		if (NULL != klPrincipal)  { KLDisposePrincipal (klPrincipal); }
		if (NULL != outPrincipal) { KLDisposePrincipal (outPrincipal); }

		if (NULL != ccacheName) { free (ccacheName); }
	}
	
Done:
	KHLog ("]]] KRBTestForExistingTicket () = %d", err);

	return err;
}

/*
	KRBAcquireTicket will acquire a ticket for the user.
		inKerberosSession is the pointer returned by KRBCreateSession.
		inClientPrincipalInfo is the outClientPrincipalInfo dictionary from KRBCopyClientPrincipalInfo.
*/
OSStatus KRBAcquireTicket(void *inKerberosSession, CFDictionaryRef inClientPrincipalInfo)
{
	OSStatus			err = noErr;
	KRBhelperContext	*hCtx = (KRBhelperContext *)inKerberosSession;
	KLStatus			krb_err = klNoErr;
	KLLoginOptions		loginOptions = NULL;
	KLPrincipal			clientPrincipal = NULL;
	char				*ccacheName = NULL;
	CFStringRef			principal = NULL, password = NULL;
	char				*principalString = NULL, *passwordString = NULL;
	SecCertificateRef	usingCertificate = NULL;

	
	if (NULL == hCtx) { err = paramErr; goto Error; }

        KHLog ("%s", "[[[ KRBAcquireTicket () - required parameters okay");

	krb_err = KLCreateLoginOptions (&loginOptions);

	principal = CFDictionaryGetValue (inClientPrincipalInfo, kKRBClientPrincipalKey);
	if (NULL == principal) { err = paramErr; goto Error; }
	__KRBCreateUTF8StringFromCFString (principal, &principalString);
	
	KLCreatePrincipalFromString (principalString, kerberosVersion_V5, &clientPrincipal);
	/* XXX check memory allocation failure ^^^ */
	
	CFDictionaryGetValueIfPresent (inClientPrincipalInfo, kKRBUsingCertificateKey, (const void **)&usingCertificate);
	
	if (NULL != usingCertificate) {
		krb_err = k5_ok(krb5_pkinit_set_client_cert(principalString, (krb5_pkinit_cert_t)usingCertificate));
		if (0 == krb_err) {
			KHLog ("%s", "    KRBAcquireTicket: Using a certificate");
			/* KLAcquireInitialTicketsWithPassword requires *some* password.  */
			passwordString = strdup (" ");
		} else
			usingCertificate = NULL;
	}
	if (NULL == usingCertificate) {
		password  = CFDictionaryGetValue (inClientPrincipalInfo, kKRBClientPasswordKey);
		if (NULL == password) { err = paramErr; goto Error; }
	
		__KRBCreateUTF8StringFromCFString (password, &passwordString);
	}

	krb_err = KLAcquireInitialTicketsWithPassword (clientPrincipal, loginOptions, passwordString, &ccacheName);
		
	err = krb_err;

Error:
	if (NULL != loginOptions)  { KLDisposeLoginOptions (loginOptions); }
	if (NULL != principalString) { __KRBReleaseUTF8String (principalString); }
	if (NULL != passwordString) {  __KRBReleaseUTF8String (passwordString); }
	
	if (NULL != clientPrincipal) { KLDisposePrincipal (clientPrincipal); }
	
	if (NULL != ccacheName) { free (ccacheName); }

	KHLog ("]]] KRBAcquireTicket () = %d", err);
	
	return err;
}


/*
	KRBCloseSession will release the kerberos session
		inKerberosSession is the pointer returned by KRBCreateSession.
*/
OSStatus KRBCloseSession(void *inKerberosSession)
{
	OSStatus			err = noErr;
	KRBhelperContext    *hCtx = (KRBhelperContext *)inKerberosSession;
	
	if (NULL == hCtx) { err = paramErr; goto Error; }

        KHLog ("%s", "[[[ KRBCloseSession () - required parameters okay");

	if (NULL != hCtx->inAdvertisedPrincipal) { CFRelease (hCtx->inAdvertisedPrincipal); }
	if (NULL != hCtx->realm)				 { CFRelease (hCtx->realm); }

	if (NULL != hCtx->useName)		{ free (hCtx->useName); }
	if (NULL != hCtx->useInstance)	{ free (hCtx->useInstance); }
	if (NULL != hCtx->useRealm)		{ free (hCtx->useRealm); }
	
	free(hCtx);
Error:
	KHLog ("]]] KRBCloseSession () = %d", err);

	return err;
}

/*
;; Local Variables: **
;; tab-width: 4 **
;; c-basic-offset: 4 **
;; End: **
*/
