/*
 * Copyright (c) 2012,2014 Apple Inc. All Rights Reserved.
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


#include <stdio.h>
#include <stdlib.h>
#include <Security/SecureTransportPriv.h>
#include <AssertMacros.h>

#include "ssl_regressions.h"
#include "ssl-utils.h"


#include "cipherSpecs.h"

static int test_GetSupportedCiphers(SSLContextRef ssl, bool server)
{
    size_t max_ciphers = 0;
    int fail=1;
    SSLCipherSuite *ciphers = NULL;

    require_noerr(SSLGetNumberSupportedCiphers(ssl, &max_ciphers), out);

    size_t size = max_ciphers * sizeof (SSLCipherSuite);
    ciphers = (SSLCipherSuite *) malloc(size);

    require_string(ciphers, out, "out of memory");
    memset(ciphers, 0xff, size);

    size_t num_ciphers = max_ciphers;
    require_noerr(SSLGetSupportedCiphers(ssl, ciphers, &num_ciphers), out);

    for (size_t i = 0; i < num_ciphers; i++) {
        require(ciphers[i]!=(SSLCipherSuite)(-1), out);
    }

    /* Success! */
    fail=0;

out:
    if(ciphers) free(ciphers);
    return fail;
}

static
int allowed_default_ciphers(SSLCipherSuite cs, bool server, bool dhe_enabled)
{
    switch (cs) {

        /* BAD to enable by default */


        /*
         * Tags for SSL 2 cipher kinds which are not specified
         * for SSL 3.
         */
        case SSL_RSA_WITH_RC2_CBC_MD5:
        case SSL_RSA_WITH_IDEA_CBC_MD5:
        case SSL_RSA_WITH_DES_CBC_MD5:
        case SSL_RSA_WITH_3DES_EDE_CBC_MD5:

        /* Export and Simple DES ciphers */
        case SSL_RSA_EXPORT_WITH_RC4_40_MD5:
        case SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5:
        case SSL_RSA_WITH_IDEA_CBC_SHA:
        case SSL_RSA_EXPORT_WITH_DES40_CBC_SHA:
        case SSL_RSA_WITH_DES_CBC_SHA:
        case SSL_DH_DSS_EXPORT_WITH_DES40_CBC_SHA:
        case SSL_DH_DSS_WITH_DES_CBC_SHA:
        case SSL_DH_RSA_EXPORT_WITH_DES40_CBC_SHA:
        case SSL_DH_RSA_WITH_DES_CBC_SHA:
        case SSL_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA:
        case SSL_DHE_DSS_WITH_DES_CBC_SHA:
        case SSL_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA:
        case SSL_DHE_RSA_WITH_DES_CBC_SHA:
        case SSL_DH_anon_EXPORT_WITH_RC4_40_MD5:
        case SSL_DH_anon_EXPORT_WITH_DES40_CBC_SHA:
        case SSL_DH_anon_WITH_DES_CBC_SHA:
        case SSL_FORTEZZA_DMS_WITH_NULL_SHA:
        case SSL_FORTEZZA_DMS_WITH_FORTEZZA_CBC_SHA:

        case SSL_NO_SUCH_CIPHERSUITE:

        /* Null ciphers. */
        case TLS_NULL_WITH_NULL_NULL:
        case TLS_RSA_WITH_NULL_MD5:
        case TLS_RSA_WITH_NULL_SHA:
        case TLS_RSA_WITH_NULL_SHA256:
        case TLS_ECDH_ECDSA_WITH_NULL_SHA:
        case TLS_ECDHE_ECDSA_WITH_NULL_SHA:
        case TLS_ECDHE_RSA_WITH_NULL_SHA:
        case TLS_ECDH_RSA_WITH_NULL_SHA:
        case TLS_ECDH_anon_WITH_NULL_SHA:

        /* Completely anonymous Diffie-Hellman */
        case TLS_DH_anon_WITH_RC4_128_MD5:
        case TLS_DH_anon_WITH_3DES_EDE_CBC_SHA:
        case TLS_DH_anon_WITH_AES_128_CBC_SHA:
        case TLS_DH_anon_WITH_AES_256_CBC_SHA:
        case TLS_DH_anon_WITH_AES_128_CBC_SHA256:
        case TLS_DH_anon_WITH_AES_256_CBC_SHA256:
        case TLS_DH_anon_WITH_AES_128_GCM_SHA256:
        case TLS_DH_anon_WITH_AES_256_GCM_SHA384:
        case TLS_ECDH_anon_WITH_RC4_128_SHA:
        case TLS_ECDH_anon_WITH_3DES_EDE_CBC_SHA:
        case TLS_ECDH_anon_WITH_AES_128_CBC_SHA:
        case TLS_ECDH_anon_WITH_AES_256_CBC_SHA:


        /* Sstatic Diffie-Hellman and DSS */
        case TLS_DH_DSS_WITH_3DES_EDE_CBC_SHA:
        case TLS_DH_RSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA:
        case TLS_DH_DSS_WITH_AES_128_CBC_SHA:
        case TLS_DH_RSA_WITH_AES_128_CBC_SHA:
        case TLS_DHE_DSS_WITH_AES_128_CBC_SHA:
        case TLS_DH_DSS_WITH_AES_256_CBC_SHA:
        case TLS_DH_RSA_WITH_AES_256_CBC_SHA:
        case TLS_DHE_DSS_WITH_AES_256_CBC_SHA:
        case TLS_DH_DSS_WITH_AES_128_CBC_SHA256:
        case TLS_DH_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_DHE_DSS_WITH_AES_128_CBC_SHA256:
        case TLS_DH_DSS_WITH_AES_256_CBC_SHA256:
        case TLS_DH_RSA_WITH_AES_256_CBC_SHA256:
        case TLS_DHE_DSS_WITH_AES_256_CBC_SHA256:
        case TLS_DH_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_DH_RSA_WITH_AES_256_GCM_SHA384:
        case TLS_DHE_DSS_WITH_AES_128_GCM_SHA256:
        case TLS_DHE_DSS_WITH_AES_256_GCM_SHA384:
        case TLS_DH_DSS_WITH_AES_128_GCM_SHA256:
        case TLS_DH_DSS_WITH_AES_256_GCM_SHA384:

            return 0;


        /* OK to enable by default on the client only (not supported on server) */
        case TLS_ECDH_ECDSA_WITH_RC4_128_SHA:
        case TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA:
        case TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA:
        case TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA256:
        case TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA384:
        case TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256:
        case TLS_ECDH_ECDSA_WITH_AES_256_GCM_SHA384:
        case TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_ECDH_RSA_WITH_AES_256_GCM_SHA384:
        case TLS_ECDH_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_ECDH_RSA_WITH_AES_256_CBC_SHA384:
        case TLS_ECDH_RSA_WITH_RC4_128_SHA:
        case TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_ECDH_RSA_WITH_AES_128_CBC_SHA:
        case TLS_ECDH_RSA_WITH_AES_256_CBC_SHA:
            return !server;

        /* OK to enable by default for both client and server */

        case TLS_RSA_WITH_RC4_128_MD5:
        case TLS_RSA_WITH_RC4_128_SHA:
        case TLS_RSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_RSA_WITH_AES_128_CBC_SHA:
        case TLS_RSA_WITH_AES_256_CBC_SHA:
        case TLS_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_RSA_WITH_AES_256_CBC_SHA256:
        case TLS_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_RSA_WITH_AES_256_GCM_SHA384:


        case TLS_ECDHE_ECDSA_WITH_RC4_128_SHA:
        case TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA:
        case TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA:
        case TLS_ECDHE_RSA_WITH_RC4_128_SHA:
        case TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA:
        case TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA:
        case TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256:
        case TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384:
        case TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384:
        case TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256:
        case TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384:
        case TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384:
            return 1;

        case TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_DHE_RSA_WITH_AES_128_CBC_SHA:
        case TLS_DHE_RSA_WITH_AES_256_CBC_SHA:
        case TLS_DHE_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_DHE_RSA_WITH_AES_256_CBC_SHA256:
        case TLS_DHE_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_DHE_RSA_WITH_AES_256_GCM_SHA384:
            return dhe_enabled;

        /* RFC 5746 - Secure Renegotiation - not specified by the user or returned by APIs*/
        case TLS_EMPTY_RENEGOTIATION_INFO_SCSV:
            return 0;

        /* unknown cipher ? */
        default:
            return 0;
    }
}

static OSStatus SocketWrite(SSLConnectionRef conn, const void *data, size_t *length)
{
    return errSSLWouldBlock;
}

static OSStatus SocketRead(SSLConnectionRef conn, void *data, size_t *length)
{
    return errSSLWouldBlock;
}


static int test_GetEnabledCiphers(SSLContextRef ssl, bool server, bool dhe_enabled)
{
    size_t max_ciphers = 0;
    size_t num_ciphers;
    size_t num_ciphers_2;
    size_t size;
    int fail=1;
    SSLCipherSuite *ciphers = NULL;
    SSLCipherSuite *ciphers_2 = NULL;
    OSStatus err;

    err=SSLSetIOFuncs(ssl, &SocketRead, &SocketWrite);
    err=SSLSetConnection(ssl, NULL);

    require_noerr(SSLGetNumberEnabledCiphers(ssl, &max_ciphers), out);

    err=SSLHandshake(ssl);

    require_noerr(SSLGetNumberEnabledCiphers(ssl, &max_ciphers), out);

    require(max_ciphers == (dhe_enabled?32:25), out);

    size = max_ciphers * sizeof (SSLCipherSuite);
    ciphers = (SSLCipherSuite *) malloc(size);
    require_string(ciphers, out, "out of memory");
    memset(ciphers, 0xff, size);

    num_ciphers = max_ciphers;
    require_noerr(SSLGetEnabledCiphers(ssl, ciphers, &num_ciphers), out);

    //printf("Ciphers Enabled before first handshake: %zd\n", num_ciphers);

    for (size_t i = 0; i < num_ciphers; i++) {
        char csname[256];
        snprintf(csname, 256, "(%04x) %s", ciphers[i], ciphersuite_name(ciphers[i]));
        /* Uncomment the next line if you want to list the default enabled ciphers */
        //printf("%s\n", csname);
        require_string(allowed_default_ciphers(ciphers[i], server, dhe_enabled), out, csname);
    }

    err=SSLHandshake(ssl);

    require_noerr(SSLGetNumberEnabledCiphers(ssl, &max_ciphers), out);

    size = max_ciphers * sizeof (SSLCipherSuite);
    ciphers_2 = (SSLCipherSuite *) malloc(size);
    require_string(ciphers_2, out, "out of memory");
    memset(ciphers_2, 0xff, size);

    num_ciphers_2 = max_ciphers;
    require_noerr(SSLGetEnabledCiphers(ssl, ciphers_2, &num_ciphers_2), out);

    //printf("Ciphers Enabled after first handshake: %zd\n", num_ciphers_2);

    for (size_t i = 0; i < num_ciphers_2; i++) {
        char csname[256];
        snprintf(csname, 256, "(%04x) %s", ciphers_2[i], ciphersuite_name(ciphers_2[i]));
        /* Uncomment the next line if you want to list the default enabled ciphers */
        //printf("%s\n", csname);
    }

    require(num_ciphers_2 == num_ciphers, out);
    require((memcmp(ciphers, ciphers_2, num_ciphers*sizeof(uint16_t)) == 0), out);

    /* Success! */
    fail=0;

out:
    if(ciphers) free(ciphers);
    if(ciphers_2) free(ciphers_2);
    return fail;
}

static int test_SetEnabledCiphers(SSLContextRef ssl, bool server)
{
    int fail=1;
    size_t num_enabled;
    
    /* This should not fail as long as we have one valid cipher in this table */
    SSLCipherSuite ciphers[] = {
        SSL_RSA_WITH_RC2_CBC_MD5, /* unsupported */
        TLS_RSA_WITH_NULL_SHA, /* supported by not enabled by default */
        TLS_RSA_WITH_AES_128_CBC_SHA, /* Supported and enabled by default */
    };

    require_noerr(SSLSetEnabledCiphers(ssl, ciphers, sizeof(ciphers)/sizeof(SSLCipherSuite)), out);
    require_noerr(SSLGetNumberEnabledCiphers(ssl, &num_enabled), out);

    require(num_enabled==2, out); /* 2 ciphers in the above table are supported */

    /* Success! */
    fail=0;

out:
    return fail;
}


static void
test(SSLProtocolSide side, bool dhe_enabled)
{
    SSLContextRef ssl = NULL;
    bool server = (side == kSSLServerSide);

    require(ssl=SSLCreateContext(kCFAllocatorDefault, side, kSSLStreamType), out);
    ok(ssl, "SSLCreateContext failed");

    ok_status(SSLSetDHEEnabled(ssl, dhe_enabled));

    /* The order of this tests does matter, be careful when adding tests */
    ok(!test_GetSupportedCiphers(ssl, server), "GetSupportedCiphers test failed");
    ok(!test_GetEnabledCiphers(ssl, server, dhe_enabled), "GetEnabledCiphers test failed");

    CFRelease(ssl); ssl=NULL;

    require(ssl=SSLCreateContext(kCFAllocatorDefault, side, kSSLStreamType), out);
    ok(ssl, "SSLCreateContext failed");
    
    ok(!test_SetEnabledCiphers(ssl, server), "SetEnabledCiphers test failed");

out:
    if(ssl) CFRelease(ssl);
}


int ssl_46_SSLGetSupportedCiphers(int argc, char *const *argv)
{
    plan_tests(24);

    test(kSSLClientSide, true);
    test(kSSLServerSide, true);
    test(kSSLClientSide, false);
    test(kSSLServerSide, false);


    return 0;
}

