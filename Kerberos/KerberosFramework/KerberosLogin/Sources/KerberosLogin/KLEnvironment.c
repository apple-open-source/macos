/*
 * KLEnvironment.c
 *
 * $Header: /cvs/kfm/KerberosFramework/KerberosLogin/Sources/KerberosLogin/KLEnvironment.c,v 1.15 2005/03/22 18:31:48 lxs Exp $
 *
 * Copyright 2004 Massachusetts Institute of Technology.
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

#define kUDPServicePrefix "_udp"
#define kTCPServicePrefix "_tcp"
#define kKerberos5ServicePrefix "_kerberos"
#define kKerberos4ServicePrefix "_kerberos-iv"

#include "k5-int.h"
#include "os-proto.h"

static KLBoolean __KLRealmHasDNSServiceRecord (const char *inService, const char *inProtocol, const char *inRealm);
static KLBoolean __KLRealmHasKerberos4DNSServiceRecord (const char *inRealm);
static KLBoolean __KLRealmHasKerberos5DNSServiceRecord (const char *inRealm);

static KLBoolean __KLPrincipalHasKerberos5ChangePasswordServerSpecified (KLPrincipal inPrincipal);
static KLBoolean __KLPrincipalHasKerberos4ChangePasswordServerSpecified (KLPrincipal inPrincipal);

#pragma mark -

/*
 * RealmShouldHaveKerberosX return whether a realm supports Kerberos version X,
 * according to the configuration file. They do not perform realm name conversion,
 * so if a realm R1 supports v5 and its corresponding v4 realm has a different name R2,
 * only RealmShouldHaveKerberos4 is true for R2, and only RealmShouldHaveKerberos5 is true for R1.
 * 
 * RealmShouldHaveKerberosXOrEquivalent return whether a realm supports Kerberos version X,
 * either by being a Kerberos version X realm, or having a corresponding Kerberos version X
 * realm, possibly of a different name
 */

// ---------------------------------------------------------------------------

KLBoolean __KLRealmHasKerberos4Profile (const char *inRealm)
{
    KLStatus    err = klNoErr;
    KLBoolean   hasProfile = false;
    profile_t   profile = NULL;
    const char *v4RealmsList[] = { REALMS_V4_PROF_REALMS_SECTION, NULL };
    char      **v4Realms = NULL;

    if (inRealm == NULL) { err = KLError_ (klParameterErr); }

    if (err == klNoErr) {
        err = __KLRemapKerberos4Error (krb_get_profile (&profile));
    }

    if (err == klNoErr) {
        err = profile_get_subsection_names (profile, v4RealmsList, &v4Realms);
    }

    if (err == klNoErr) {
        char **realm;
        for (realm = v4Realms; *realm != NULL; realm++) {
            if (strcmp (*realm, inRealm) == 0) {
                hasProfile = true;
            }
        }
    }

    if (!hasProfile) {
        // Search for an old-style config file
        FILE *cnffile = NULL;
        char lineBuffer[BUFSIZ];
        char realmBuffer[1024];
        char scratchBuffer[1024];
        
        cnffile = krb__get_cnffile();
        if (cnffile != NULL) {
            // Skip default realm name
            if (fscanf(cnffile, "%1023s", scratchBuffer) != EOF) {
                // Walk over the realms
                while (true) {
                    if (fgets (lineBuffer, BUFSIZ, cnffile) == NULL) {
                        break;
                    }
                    if (!strchr (lineBuffer, '\n')) {
                        break;
                    }
                    
                    if (sscanf (lineBuffer, "%1023s %1023s", realmBuffer, scratchBuffer) == 2) {
                        if (strcmp (realmBuffer, inRealm) == 0) {
                            hasProfile = true;
                            err = klNoErr;
                            break;
                        }
                    }
                }
            }
            fclose(cnffile);
        }        
    }

    if (v4Realms != NULL) { profile_free_list (v4Realms); }
    if (profile  != NULL) { profile_abandon (profile); }

    return hasProfile;
    
}

// ---------------------------------------------------------------------------

KLBoolean __KLRealmHasKerberos5Profile (const char *inRealm)
{
    KLStatus     err = klNoErr;
    KLBoolean    hasProfile = false;
    krb5_context context = NULL;
    profile_t    profile = NULL;
    const char  *v5RealmsList[] = { "realms", NULL };
    char       **v5Realms = NULL;

    if (inRealm == NULL) { err = KLError_ (klParameterErr); }

    if (err == klNoErr) {
        err = krb5_init_context (&context);
    }

    if (err == klNoErr) {
        err = krb5_get_profile (context, &profile);
    }

    if (err == klNoErr) {
        err = profile_get_subsection_names (profile, v5RealmsList, &v5Realms);
    }

    if (err == klNoErr) {
        char **realm;
        for (realm = v5Realms; *realm != NULL; realm++) {
            if (strcmp (*realm, inRealm) == 0) {
                hasProfile = true;
            }
        }
    }

    if (v5Realms != NULL) { profile_free_list (v5Realms); }
    if (profile  != NULL) { profile_abandon (profile); }
    if (context  != NULL) { krb5_free_context (context); }

    return hasProfile;
}

#pragma mark -

// ---------------------------------------------------------------------------

static KLBoolean __KLRealmHasDNSServiceRecord (const char *inService, const char *inProtocol, const char *inRealm)
{
    KLStatus err = klNoErr;
    KLBoolean hasDNS = FALSE;
    krb5_context context = NULL;
    
    if (!err) {
        err = krb5_init_context (&context);
    }

    if (!err && _krb5_use_dns_kdc (context)) {
        struct srv_dns_entry *entries = NULL;
        krb5_data realm = { KV5M_DATA, strlen (inRealm), (char *)inRealm };
    
        err = krb5int_make_srv_query_realm (&realm, inService, inProtocol, &entries);
        
        if (!err) {
            // Make sure there is at least one valid entry in the list
            if ((entries != NULL) && (entries->host[0] != 0)) {
                hasDNS = TRUE;
            }
            krb5int_free_srv_dns_data (entries);
        }
    }
    
    if (context != NULL) { krb5_free_context (context); }
    
    return hasDNS;
}

#pragma mark -

// ---------------------------------------------------------------------------

static KLBoolean __KLRealmHasKerberos4DNSServiceRecord (const char *inRealm)
{
    KLStatus err = klNoErr;
    KLBoolean hasDNS = false;

    if (inRealm == NULL) { err = KLError_ (klParameterErr); }

    if (err == klNoErr) {
        hasDNS = __KLRealmHasDNSServiceRecord (kKerberos4ServicePrefix, kUDPServicePrefix, inRealm);
    }

    return hasDNS;
}

// ---------------------------------------------------------------------------

static KLBoolean __KLRealmHasKerberos5DNSServiceRecord (const char *inRealm)
{
    KLStatus err = klNoErr;
    KLBoolean hasDNS = false;

    if (inRealm == NULL) { err = KLError_ (klParameterErr); }

    if (err == klNoErr) {
        hasDNS = __KLRealmHasDNSServiceRecord (kKerberos5ServicePrefix, kUDPServicePrefix, inRealm);
        if (!hasDNS) {
            hasDNS = __KLRealmHasDNSServiceRecord (kKerberos5ServicePrefix, kTCPServicePrefix, inRealm);
        }
    }

    return hasDNS;
}

#pragma mark -

// ---------------------------------------------------------------------------

KLBoolean __KLRealmHasKerberos4 (const char *inRealm) 
{
    KLBoolean has = false;

    has = __KLRealmHasKerberos4Profile (inRealm);
    if (!has) {
        has = __KLRealmHasKerberos4DNSServiceRecord (inRealm);
    }

    return has;
}

// ---------------------------------------------------------------------------

KLBoolean __KLRealmHasKerberos5 (const char *inRealm)
{
    KLBoolean has = false;

    has = __KLRealmHasKerberos5Profile (inRealm);
    if (!has) {
        has = __KLRealmHasKerberos5DNSServiceRecord (inRealm);
    }

    return has;
}

#pragma mark -

// ---------------------------------------------------------------------------

KLBoolean __KLPrincipalHasKerberos4Profile (KLPrincipal inPrincipal)
{
    KLStatus err = klNoErr;
    KLBoolean has = false;
    char *realm = NULL;

    if (inPrincipal == NULL) { err = KLError_ (klParameterErr); }

    if (err == klNoErr) {
        err = __KLGetRealmFromPrincipal (inPrincipal, kerberosVersion_V4, &realm);
    }

    if (err == klNoErr) {
        has = __KLRealmHasKerberos4Profile (realm);
    }

    if (realm != NULL) { KLDisposeString (realm); }

    return has;
}

// ---------------------------------------------------------------------------

KLBoolean __KLPrincipalHasKerberos5Profile (KLPrincipal inPrincipal)
{
    KLStatus err = klNoErr;
    KLBoolean has = false;
    char *realm = NULL;

    if (inPrincipal == NULL) { err = KLError_ (klParameterErr); }

    if (err == klNoErr) {
        err = __KLGetRealmFromPrincipal (inPrincipal, kerberosVersion_V5, &realm);
    }

    if (err == klNoErr) {
        has = __KLRealmHasKerberos5Profile (realm);
    }

    if (realm != NULL) { KLDisposeString (realm); }

    return has;
}

#pragma mark -

// ---------------------------------------------------------------------------

KLBoolean __KLPrincipalHasKerberos4 (KLPrincipal inPrincipal)
{
    KLStatus err = klNoErr;
    KLBoolean has = false;
    char *realm = NULL;

    if (inPrincipal == NULL) { err = KLError_ (klParameterErr); }

    if (err == klNoErr) {
        err = __KLGetRealmFromPrincipal (inPrincipal, kerberosVersion_V4, &realm);
    }

    if (err == klNoErr) {
        has = __KLRealmHasKerberos4 (realm);
    }

    if (realm != NULL) { KLDisposeString (realm); }

    return has;
}

// ---------------------------------------------------------------------------

KLBoolean __KLPrincipalHasKerberos5 (KLPrincipal inPrincipal)
{
    KLStatus err = klNoErr;
    KLBoolean has = false;
    char *realm = NULL;

    if (inPrincipal == NULL) { err = KLError_ (klParameterErr); }

    if (err == klNoErr) {
        err = __KLGetRealmFromPrincipal (inPrincipal, kerberosVersion_V5, &realm);
    }

    if (err == klNoErr) {
        has = __KLRealmHasKerberos5 (realm);
    }

    if (realm != NULL) { KLDisposeString (realm); }

    return has;
}

#pragma mark -

// ---------------------------------------------------------------------------

KLBoolean __KLPrincipalShouldUseKerberos524Protocol (KLPrincipal inPrincipal)
{
    KLStatus err = klNoErr;
    KLBoolean has = false;
    char *realm = NULL;

    if (inPrincipal == NULL) { err = KLError_ (klParameterErr); }

    if (err == klNoErr) {
        err = __KLGetRealmFromPrincipal (inPrincipal, kerberosVersion_V5, &realm);
    }

    if (err == klNoErr) {
        // This v5 realm has a v4 realm of the same name
        has = __KLRealmHasKerberos5 (realm) && __KLRealmHasKerberos4 (realm);
    }

    if (realm != NULL) { KLDisposeString (realm); }

    return has;    
}

#pragma mark -

// ---------------------------------------------------------------------------

static KLBoolean __KLPrincipalHasKerberos5ChangePasswordServerSpecified (KLPrincipal inPrincipal)
{
    KLStatus     err = klNoErr;
    KLBoolean    specified = false;
    krb5_context context = NULL;
    profile_t    profile = NULL;
    char        *realm = NULL;
    const char  *v5PasswdServersList[] = { "realms", NULL, "kpasswd_server", NULL };
    char       **v5PasswdServers = NULL;

    if (inPrincipal == NULL) { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        err = __KLGetRealmFromPrincipal (inPrincipal, kerberosVersion_V5, &realm);
        if (err == klNoErr) { v5PasswdServersList[1] = realm; }
    }
    
    if (err == klNoErr) {
        err = krb5_init_context (&context);
    }

    if (err == klNoErr) {
        err = krb5_get_profile (context, &profile);
    }
    
    if (err == klNoErr) {
        err = profile_get_values (profile, v5PasswdServersList, &v5PasswdServers);
    }
    
    // If we have a correpsonding v4 realm, that's good, if the realm is a valid v4 realm
    if (err == klNoErr) {
        if (v5PasswdServers[0] != NULL) {
            specified = true;
        }
    }

    if (realm           != NULL) { KLDisposeString (realm); }
    if (v5PasswdServers != NULL) { profile_free_list (v5PasswdServers); }
    if (profile         != NULL) { profile_abandon (profile); }
    if (context         != NULL) { krb5_free_context (context); }

    return specified;
}

// ---------------------------------------------------------------------------

static KLBoolean __KLPrincipalHasKerberos4ChangePasswordServerSpecified (KLPrincipal inPrincipal)
{
    KLStatus     err = klNoErr;
    KLBoolean    specified = false;
    profile_t    profile = NULL;
    char        *realm = NULL;
    const char  *v4PasswdServersList[] = { REALMS_V4_PROF_REALMS_SECTION, NULL, REALMS_V4_PROF_KPASSWD_KDC, NULL };
    char       **v4PasswdServers = NULL;

    if (inPrincipal == NULL) { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        err = __KLGetRealmFromPrincipal (inPrincipal, kerberosVersion_V4, &realm);
        if (err == klNoErr) { v4PasswdServersList[1] = realm; }
    }
    
    if (err == klNoErr) {
        err = __KLRemapKerberos4Error (krb_get_profile (&profile));
    }
    
    if (err == klNoErr) {
        err = profile_get_values (profile, v4PasswdServersList, &v4PasswdServers);
    }
    
    // If we have a correpsonding v4 realm, that's good, if the realm is a valid v4 realm
    if (err == klNoErr) {
        if (v4PasswdServers[0] != NULL) {
            specified = true;
        }
    }
    
    if (realm           != NULL) { KLDisposeString (realm); }
    if (v4PasswdServers != NULL) { profile_free_list (v4PasswdServers); }
    if (profile         != NULL) { profile_abandon (profile); }

    return specified;
}

#pragma mark -

// ---------------------------------------------------------------------------

KLBoolean __KLPrincipalShouldUseKerberos5ChangePasswordProtocol (KLPrincipal inPrincipal)
{
    if (__KLPrincipalHasKerberos5 (inPrincipal)) {
        if (__KLPrincipalHasKerberos5ChangePasswordServerSpecified (inPrincipal)) {
            return true;	// krb5 kpasswd_server specified
        } else {
            if (__KLPrincipalHasKerberos4 (inPrincipal)) {
                if (!__KLPrincipalHasKerberos4ChangePasswordServerSpecified (inPrincipal)) {
                    return true;	// krb4 kpasswd_server not specified, use krb5
                } else {
                    return false;	// krb5 and krb4 realms exist, but default to krb5
                }
            } else {
                return true;	// krb5-only, use krb5
            }
        }
    } else {
        return false;	// No krb5 realm
    }
}

// ---------------------------------------------------------------------------

KLBoolean __KLPrincipalShouldUseKerberos4ChangePasswordProtocol (KLPrincipal inPrincipal)
{
    if (__KLPrincipalHasKerberos4 (inPrincipal)) {
        if (__KLPrincipalHasKerberos4ChangePasswordServerSpecified (inPrincipal)) {
            return true;	// krb4 kpasswd_server specified
        } else {
            if (__KLPrincipalHasKerberos5 (inPrincipal)) {
                if (!__KLPrincipalHasKerberos5ChangePasswordServerSpecified (inPrincipal)) {
                    return true;	// krb5 kpasswd_server not specified, use krb4
                } else {
                    return false;	// krb4 and krb5 realms exist, but default to krb5
                }
            } else {
                return true;	// krb4-only, use krb4
            }
        }
    } else {
        return false;	// No krb4 realm
    }
}

#pragma mark -

// ---------------------------------------------------------------------------

KLBoolean __KLIsKerberosAgent(void)
{
    CFBundleRef mainBundle = CFBundleGetMainBundle ();
    if (mainBundle != NULL) {
        CFStringRef mainBundleID = CFBundleGetIdentifier (mainBundle);
        if (mainBundleID != NULL) {
            CFComparisonResult result;
            result = CFStringCompare (mainBundleID, CFSTR("edu.mit.Kerberos.KerberosAgent"), 0);
            if (result == kCFCompareEqualTo) {
                return true;
            }
        }
    }

    return false;
}

