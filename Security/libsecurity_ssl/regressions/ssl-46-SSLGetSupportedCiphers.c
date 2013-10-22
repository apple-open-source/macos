//
//  ssl-46-SSLGetSupportedCiphers.c
//  libsecurity_ssl
//
//  Created by Fabrice Gautier on 10/15/12.
//
//

#include <stdio.h>
#include <stdlib.h>
#include <Security/SecureTransport.h>
#include <AssertMacros.h>

#include "ssl_regressions.h"
#include "ssl-utils.h"


#include "cipherSpecs.h"

static int test_GetSupportedCiphers(SSLContextRef ssl)
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
int allowed_default_ciphers(SSLCipherSuite cs)
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

            return 0;


        /* OK to enable by default */

        /* Server provided RSA certificate for key exchange. */
        case TLS_RSA_WITH_RC4_128_MD5:
        case TLS_RSA_WITH_RC4_128_SHA:
        case TLS_RSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_RSA_WITH_AES_128_CBC_SHA:
        case TLS_RSA_WITH_AES_256_CBC_SHA:
        case TLS_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_RSA_WITH_AES_256_CBC_SHA256:
            return 1;

        /* Server-authenticated (and optionally client-authenticated) Diffie-Hellman. */
        case TLS_DH_DSS_WITH_3DES_EDE_CBC_SHA:
        case TLS_DH_RSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA:
        case TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_DH_DSS_WITH_AES_128_CBC_SHA:
        case TLS_DH_RSA_WITH_AES_128_CBC_SHA:
        case TLS_DHE_DSS_WITH_AES_128_CBC_SHA:
        case TLS_DHE_RSA_WITH_AES_128_CBC_SHA:
        case TLS_DH_DSS_WITH_AES_256_CBC_SHA:
        case TLS_DH_RSA_WITH_AES_256_CBC_SHA:
        case TLS_DHE_DSS_WITH_AES_256_CBC_SHA:
        case TLS_DHE_RSA_WITH_AES_256_CBC_SHA:
        case TLS_DH_DSS_WITH_AES_128_CBC_SHA256:
        case TLS_DH_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_DHE_DSS_WITH_AES_128_CBC_SHA256:
        case TLS_DHE_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_DH_DSS_WITH_AES_256_CBC_SHA256:
        case TLS_DH_RSA_WITH_AES_256_CBC_SHA256:
        case TLS_DHE_DSS_WITH_AES_256_CBC_SHA256:
        case TLS_DHE_RSA_WITH_AES_256_CBC_SHA256:

        case TLS_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_RSA_WITH_AES_256_GCM_SHA384:
        case TLS_DHE_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_DHE_RSA_WITH_AES_256_GCM_SHA384:
        case TLS_DH_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_DH_RSA_WITH_AES_256_GCM_SHA384:
        case TLS_DHE_DSS_WITH_AES_128_GCM_SHA256:
        case TLS_DHE_DSS_WITH_AES_256_GCM_SHA384:
        case TLS_DH_DSS_WITH_AES_128_GCM_SHA256:
        case TLS_DH_DSS_WITH_AES_256_GCM_SHA384:

        case TLS_ECDH_ECDSA_WITH_RC4_128_SHA:
        case TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA:
        case TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA:
        case TLS_ECDHE_ECDSA_WITH_RC4_128_SHA:
        case TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA:
        case TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA:
        case TLS_ECDH_RSA_WITH_RC4_128_SHA:
        case TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_ECDH_RSA_WITH_AES_128_CBC_SHA:
        case TLS_ECDH_RSA_WITH_AES_256_CBC_SHA:
        case TLS_ECDHE_RSA_WITH_RC4_128_SHA:
        case TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA:
        case TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA:

        case TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256:
        case TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384:
        case TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA256:
        case TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA384:
        case TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384:
        case TLS_ECDH_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_ECDH_RSA_WITH_AES_256_CBC_SHA384:

        case TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256:
        case TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384:
        case TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256:
        case TLS_ECDH_ECDSA_WITH_AES_256_GCM_SHA384:
        case TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384:
        case TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_ECDH_RSA_WITH_AES_256_GCM_SHA384:

        /* RFC 5746 - Secure Renegotiation */
        case TLS_EMPTY_RENEGOTIATION_INFO_SCSV:
            return 1;

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


static int test_GetEnabledCiphers(SSLContextRef ssl)
{
    size_t max_ciphers = 0;
    int fail=1;
    SSLCipherSuite *ciphers = NULL;
    OSStatus err;

    err=SSLSetIOFuncs(ssl, &SocketRead, &SocketWrite);
    err=SSLSetConnection(ssl, NULL);
    err=SSLHandshake(ssl);

    require_noerr(SSLGetNumberEnabledCiphers(ssl, &max_ciphers), out);

    size_t size = max_ciphers * sizeof (SSLCipherSuite);
    ciphers = (SSLCipherSuite *) malloc(size);

    require_string(ciphers, out, "out of memory");
    memset(ciphers, 0xff, size);

    size_t num_ciphers = max_ciphers;
    require_noerr(SSLGetEnabledCiphers(ssl, ciphers, &num_ciphers), out);

    for (size_t i = 0; i < num_ciphers; i++) {
        char csname[256];
        snprintf(csname, 256, "(%04x) %s", ciphers[i], ciphersuite_name(ciphers[i]));
        /* Uncomment the next line if you want to list the default enabled ciphers */
        //printf("%s\n", csname);
        require_string(allowed_default_ciphers(ciphers[i]), out, csname);
    }

    /* Success! */
    fail=0;

out:
    if(ciphers) free(ciphers);
    return fail;
}

static int test_SetEnabledCiphers(SSLContextRef ssl)
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
test(void)
{
    SSLContextRef ssl = NULL;

    require(ssl=SSLCreateContext(kCFAllocatorDefault, kSSLClientSide, kSSLStreamType), out);
    ok(ssl, "SSLCreateContext failed");

    /* The order of this tests does matter, be careful when adding tests */
    ok(!test_GetSupportedCiphers(ssl), "GetSupportedCiphers test failed");
    ok(!test_GetEnabledCiphers(ssl), "GetEnabledCiphers test failed");

    CFRelease(ssl); ssl=NULL;

    require(ssl=SSLCreateContext(kCFAllocatorDefault, kSSLClientSide, kSSLStreamType), out);
    ok(ssl, "SSLCreateContext failed");
    
    ok(!test_SetEnabledCiphers(ssl), "SetEnabledCiphers test failed");

out:
    if(ssl) CFRelease(ssl);
}


int ssl_46_SSLGetSupportedCiphers(int argc, char *const *argv)
{
    plan_tests(5);

    test();

    return 0;
}

