/*
 * KLCCacheManagement.h
 *
 * $Header: /cvs/kfm/KerberosFramework/KerberosLogin/Sources/KerberosLogin/Headers/KLCCacheManagement.h,v 1.1 2003/04/14 17:25:25 lxs Exp $
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

KLStatus __KLCreateNewCCacheWithCredentials (KLPrincipal        inPrincipal,
                                             krb5_context       inContext,
                                             krb5_creds        *inV5Creds,
                                             CREDENTIALS       *inV4Creds,
                                             cc_ccache_t       *outCCache);
KLStatus __KLStoreKerberos5CredentialsInCCache (krb5_context inContext, krb5_creds *inV5Creds, const cc_ccache_t inCCache);
KLStatus __KLStoreKerberos4CredentialsInCCache (CREDENTIALS *inV4Creds, const cc_ccache_t inCCache);

KLStatus __KLGetKerberos5TgtForCCache (const cc_ccache_t inCCache, krb5_context inContext, krb5_creds *outCreds);
KLStatus __KLGetKerberos4TgtForCCache (const cc_ccache_t inCCache, CREDENTIALS *outCreds);

KLStatus __KLGetSystemDefaultCCache (cc_ccache_t *outCCache);
KLStatus __KLGetFirstCCacheForPrincipal (const KLPrincipal inPrincipal, cc_ccache_t *outCCache);
KLStatus __KLGetCCacheByName (const char *inCacheName, cc_ccache_t *outCCache);

KLBoolean __KLCCacheHasKerberos4 (const cc_ccache_t inCCache);
KLBoolean __KLCCacheHasKerberos5 (const cc_ccache_t inCCache);

KLStatus __KLGetValidTgtForCCache (const cc_ccache_t inCCache, KLKerberosVersion inVersion, cc_credentials_t *outCreds);
KLStatus __KLCacheHasValidTickets (const cc_ccache_t inCCache, KLKerberosVersion inVersion);

KLStatus __KLGetPrincipalForCCache (const cc_ccache_t inCCache, KLPrincipal *outPrincipal);
KLStatus __KLGetNameForCCache (const cc_ccache_t inCCache, char **outName);
KLStatus __KLGetPrincipalAndNameForCCache (const cc_ccache_t inCCache, KLPrincipal *outPrincipal, char **outName);

KLStatus __KLGetCCacheExpirationTime (const cc_ccache_t inCCache, KLKerberosVersion inVersion, KLTime *outExpirationTime);
KLStatus __KLGetCredsExpirationTime (const cc_credentials_t inCreds, KLTime *outExpirationTime);

KLStatus __KLGetCCacheStartTime (const cc_ccache_t inCCache, KLKerberosVersion inVersion, KLTime *outStartTime);
KLStatus __KLGetCredsStartTime (const cc_credentials_t inCreds, KLTime *outStartTime);

KLStatus __KLCredsAreValid (const cc_credentials_t inCreds);

KLTime __KLCheckAddresses (void);
KLStatus __KLFreeAddressList (void);
