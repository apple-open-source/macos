/*
 * KLCCacheManagement.h
 *
 * $Header: /cvs/kfm/KerberosFramework/KerberosLogin/Sources/KerberosLogin/Headers/KLCCacheManagement.h,v 1.3 2004/12/17 05:12:34 lxs Exp $
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

/* CCache information */
struct OpaqueKLCCache;
typedef struct OpaqueKLCCache * KLCCache;


KLTime __KLCheckAddresses ();
KLStatus __KLFreeAddressList ();

KLStatus __KLCreateNewCCacheWithCredentials (KLPrincipal        inPrincipal,
                                             krb5_creds        *inV5Creds, 
                                             CREDENTIALS       *inV4Creds, 
                                             KLCCache          *outCCache);
KLStatus __KLGetCCacheByName (const char *inCacheName, KLCCache *outCCache);
KLStatus __KLGetSystemDefaultCCache (KLCCache *outCCache);
KLStatus __KLGetFirstCCacheForPrincipal (KLPrincipal inPrincipal, KLCCache *outCCache);
KLStatus __KLCloseCCache (KLCCache inCCache);
KLStatus __KLDestroyCCache (KLCCache inCCache);

KLStatus __KLMoveCCache (KLCCache inSourceCCache, KLCCache inDestinationCCache);

KLStatus __KLSetDefaultCCache (KLCCache *ioCCache);

KLStatus __KLCCacheExists (KLCCache inCCache);
                           
KLBoolean __KLCCacheHasKerberos4 (KLCCache inCCache);
KLBoolean __KLCCacheHasKerberos5 (KLCCache inCCache);

KLStatus __KLGetValidV5TgtForCCache (KLCCache inCCache, krb5_creds **outV5Creds);
KLStatus __KLGetValidV4TgtForCCache (KLCCache inCCache, CREDENTIALS *outV4Creds);

KLStatus __KLCacheHasValidTickets (KLCCache inCCache, KLKerberosVersion inVersion);

KLStatus __KLGetCCacheExpirationTime (KLCCache inCCache, KLKerberosVersion inVersion, KLTime *outExpirationTime);
KLStatus __KLGetCCacheStartTime (KLCCache inCCache, KLKerberosVersion inVersion, KLTime *outStartTime);

KLStatus __KLGetKrb5CCacheAndContextForCCache (KLCCache inCCache, krb5_ccache *outCCache, krb5_context *outContext);
KLStatus __KLGetPrincipalForCCache (KLCCache inCCache, KLPrincipal *outPrincipal);
KLStatus __KLGetNameForCCacheVersion (KLCCache inCCache, KLKerberosVersion inVersion, char **outName);
KLStatus __KLGetNameForCCache (KLCCache inCCache, char **outName);
KLStatus __KLGetPrincipalAndNameForCCache (KLCCache inCCache, KLPrincipal *outPrincipal, char **outName);
