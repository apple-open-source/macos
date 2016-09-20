
#if TARGET_OS_IPHONE
// Currently only supported for iOS

#include <stdbool.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <CoreFoundation/CoreFoundation.h>

#include <AssertMacros.h>
#include <Security/SecureTransportPriv.h> /* SSLSetOption */
#include <Security/SecureTransport.h>
#include <Security/SecPolicy.h>
#include <Security/SecTrust.h>
#include <Security/SecIdentity.h>
#include <Security/SecIdentityPriv.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecItem.h>
#include <Security/SecRandom.h>

#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdlib.h>
#include <mach/mach_time.h>

#include <Security/SecRSAKey.h>

#include "testlist.h"

/*
    SSL CipherSuite tests

    Below are all the ciphers that are individually tested.  The first element
    is the SecureTransport/RFC name; the second is what openssl calls it, which
    can be looked up in ciphers(1).

    All SSL_DH_* and TLS_DH_* are disabled because neither openssl nor
    securetransport support them:
    SSL_DH_DSS_EXPORT_WITH_DES40_CBC_SHA, SSL_DH_DSS_WITH_DES_CBC_SHA,
    SSL_DH_DSS_WITH_3DES_EDE_CBC_SHA, SSL_DH_RSA_EXPORT_WITH_DES40_CBC_SHA,
    SSL_DH_RSA_WITH_DES_CBC_SHA, SSL_DH_RSA_WITH_3DES_EDE_CBC_SHA,
    TLS_DH_DSS_WITH_AES_128_CBC_SHA, TLS_DH_RSA_WITH_AES_128_CBC_SHA,
    TLS_DH_DSS_WITH_AES_256_CBC_SHA, TLS_DH_RSA_WITH_AES_256_CBC_SHA,

    DSS is unimplemented by securetransport on the phone:
    SSL_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA, SSL_DHE_DSS_WITH_DES_CBC_SHA,
    SSL_DHE_DSS_WITH_3DES_EDE_CBC_SHA, TLS_DHE_DSS_WITH_AES_128_CBC_SHA,
    TLS_DHE_DSS_WITH_AES_256_CBC_SHA,

    SSLv2 ciphersuites disabled by securetransport on phone:
    SSL_RSA_WITH_RC2_CBC_MD5, SSL_RSA_WITH_IDEA_CBC_MD5,
    SSL_RSA_WITH_DES_CBC_MD5, SSL_RSA_WITH_3DES_EDE_CBC_MD5,

    SSLv3 ciphersuites disabled by securetransport on phone:
    SSL_RSA_WITH_IDEA_CBC_SHA, SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5

*/

typedef struct _CipherSuiteName {
        SSLCipherSuite cipher;
        const char *name;
        bool dh_anonymous;
} CipherSuiteName;

#define CIPHER(cipher, dh_anonymous) { cipher, #cipher, dh_anonymous }

static const CipherSuiteName ciphers[] = {
#if 0
    /* TODO: Generate an ecdsa private key and certificate for the tests. */
    CIPHER(TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384, false),
    CIPHER(TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, false),
    CIPHER(TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384, false),
    CIPHER(TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256, false),
    CIPHER(TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA, false),
    CIPHER(TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA, false),
    CIPHER(TLS_ECDHE_ECDSA_WITH_RC4_128_SHA, false),
    CIPHER(TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA, false),
    CIPHER(TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384, false),
    CIPHER(TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256, false),
    CIPHER(TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384, false),
    CIPHER(TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256, false),
    CIPHER(TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA, false),
    CIPHER(TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA, false),
    CIPHER(TLS_ECDHE_RSA_WITH_RC4_128_SHA, false),
    CIPHER(TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA, false),
    CIPHER(TLS_ECDH_ECDSA_WITH_AES_256_GCM_SHA384, false),
    CIPHER(TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256, false),
    CIPHER(TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA384, false),
    CIPHER(TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA256, false),
    CIPHER(TLS_ECDH_RSA_WITH_AES_256_GCM_SHA384, false),
    CIPHER(TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256, false),
    CIPHER(TLS_ECDH_RSA_WITH_AES_256_CBC_SHA384, false),
    CIPHER(TLS_ECDH_RSA_WITH_AES_128_CBC_SHA256, false),
    CIPHER(TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA, false),
    CIPHER(TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA, false),
    CIPHER(TLS_ECDH_ECDSA_WITH_RC4_128_SHA, false),
    CIPHER(TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA, false),
    CIPHER(TLS_ECDH_RSA_WITH_AES_128_CBC_SHA, false),
    CIPHER(TLS_ECDH_RSA_WITH_AES_256_CBC_SHA, false),
    CIPHER(TLS_ECDH_RSA_WITH_RC4_128_SHA, false),
    CIPHER(TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA, false),
#endif
    CIPHER(TLS_RSA_WITH_AES_256_GCM_SHA384, false),
    CIPHER(TLS_RSA_WITH_AES_128_GCM_SHA256, false),
    CIPHER(TLS_RSA_WITH_AES_256_CBC_SHA256, false),
    CIPHER(TLS_RSA_WITH_AES_128_CBC_SHA256, false),
    CIPHER(TLS_RSA_WITH_AES_128_CBC_SHA, false),
    CIPHER(SSL_RSA_WITH_RC4_128_SHA, false),
    CIPHER(SSL_RSA_WITH_RC4_128_MD5, false),
    CIPHER(TLS_RSA_WITH_AES_256_CBC_SHA, false),
    CIPHER(SSL_RSA_WITH_3DES_EDE_CBC_SHA, false),
    CIPHER(SSL_RSA_WITH_3DES_EDE_CBC_MD5, false),
    CIPHER(SSL_RSA_WITH_DES_CBC_SHA, false),
    CIPHER(SSL_RSA_WITH_DES_CBC_MD5, false),
    CIPHER(SSL_RSA_EXPORT_WITH_RC4_40_MD5, false),
    CIPHER(SSL_RSA_EXPORT_WITH_DES40_CBC_SHA, false),
    CIPHER(SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5, false),
    CIPHER(SSL_RSA_WITH_RC2_CBC_MD5, false),
    CIPHER(TLS_DHE_DSS_WITH_AES_256_GCM_SHA384, false),
    CIPHER(TLS_DHE_RSA_WITH_AES_256_GCM_SHA384, false),
    CIPHER(TLS_DHE_DSS_WITH_AES_128_GCM_SHA256, false),
    CIPHER(TLS_DHE_RSA_WITH_AES_128_GCM_SHA256, false),
    CIPHER(TLS_DHE_DSS_WITH_AES_128_CBC_SHA256, false),
    CIPHER(TLS_DHE_RSA_WITH_AES_128_CBC_SHA256, false),
    CIPHER(TLS_DHE_DSS_WITH_AES_256_CBC_SHA256, false),
    CIPHER(TLS_DHE_RSA_WITH_AES_256_CBC_SHA256, false),
    CIPHER(TLS_DHE_DSS_WITH_AES_128_CBC_SHA, false),
    CIPHER(TLS_DHE_RSA_WITH_AES_128_CBC_SHA, false),
    CIPHER(TLS_DHE_DSS_WITH_AES_256_CBC_SHA, false),
    CIPHER(TLS_DHE_RSA_WITH_AES_256_CBC_SHA, false),
    CIPHER(SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA, false),
    CIPHER(SSL_DHE_RSA_WITH_DES_CBC_SHA, false),
    CIPHER(SSL_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA, false),
    CIPHER(SSL_DHE_DSS_WITH_3DES_EDE_CBC_SHA, false),
    CIPHER(SSL_DHE_DSS_WITH_DES_CBC_SHA, false),
    CIPHER(SSL_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA, false),
    CIPHER(TLS_DH_anon_WITH_AES_256_GCM_SHA384, true),
    CIPHER(TLS_DH_anon_WITH_AES_128_GCM_SHA256, true),
    CIPHER(TLS_DH_anon_WITH_AES_128_CBC_SHA256, true),
    CIPHER(TLS_DH_anon_WITH_AES_256_CBC_SHA256, true),
    CIPHER(TLS_DH_anon_WITH_AES_128_CBC_SHA, true),
    CIPHER(TLS_DH_anon_WITH_AES_256_CBC_SHA, true),
    CIPHER(SSL_DH_anon_WITH_RC4_128_MD5, true),
    CIPHER(SSL_DH_anon_WITH_3DES_EDE_CBC_SHA, true),
    CIPHER(SSL_DH_anon_WITH_DES_CBC_SHA, true),
    CIPHER(SSL_DH_anon_EXPORT_WITH_RC4_40_MD5, true),
    CIPHER(SSL_DH_anon_EXPORT_WITH_DES40_CBC_SHA, true),
#if 0
	CIPHER(TLS_ECDHE_ECDSA_WITH_NULL_SHA, false),
	CIPHER(TLS_ECDHE_RSA_WITH_NULL_SHA, false),
    CIPHER(TLS_ECDH_ECDSA_WITH_NULL_SHA, false),
	CIPHER(TLS_ECDH_RSA_WITH_NULL_SHA, false),
#endif
    CIPHER(TLS_RSA_WITH_NULL_SHA256, false),
    CIPHER(SSL_RSA_WITH_NULL_SHA, false),
    CIPHER(SSL_RSA_WITH_NULL_MD5, false),
#if 0
    /* We don't support these yet. */
    CIPHER(TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA, false),
    CIPHER(TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA, false),
    CIPHER(TLS_RSA_WITH_RC4_128_SHA, false),
    CIPHER(TLS_RSA_WITH_3DES_EDE_CBC_SHA, false),
    CIPHER(TLS_RSA_WITH_RC4_128_MD5, false),
    CIPHER(TLS_DH_DSS_WITH_AES_256_GCM_SHA384, false),
    CIPHER(TLS_DH_DSS_WITH_AES_128_GCM_SHA256, false),
    CIPHER(TLS_DH_RSA_WITH_AES_256_GCM_SHA384, false),
    CIPHER(TLS_DH_RSA_WITH_AES_128_GCM_SHA256, false),
    CIPHER(TLS_DH_DSS_WITH_AES_256_CBC_SHA256, false),
    CIPHER(TLS_DH_RSA_WITH_AES_256_CBC_SHA256, false),
    CIPHER(TLS_DH_DSS_WITH_AES_128_CBC_SHA256, false),
    CIPHER(TLS_DH_RSA_WITH_AES_128_CBC_SHA256, false),
    CIPHER(TLS_DH_DSS_WITH_AES_256_CBC_SHA, false),
    CIPHER(TLS_DH_RSA_WITH_AES_256_CBC_SHA, false),
	CIPHER(TLS_DH_DSS_WITH_AES_128_CBC_SHA, false),
    CIPHER(TLS_DH_RSA_WITH_AES_128_CBC_SHA, false),
    CIPHER(TLS_DH_DSS_WITH_3DES_EDE_CBC_SHA, false),
    CIPHER(TLS_DH_RSA_WITH_3DES_EDE_CBC_SHA, false),
    CIPHER(TLS_ECDH_anon_WITH_AES_256_CBC_SHA, false),
	CIPHER(TLS_ECDH_anon_WITH_AES_128_CBC_SHA, false),
    CIPHER(TLS_ECDH_anon_WITH_RC4_128_SHA, false),
    CIPHER(TLS_ECDH_anon_WITH_3DES_EDE_CBC_SHA, false),
    CIPHER(TLS_ECDH_anon_WITH_NULL_SHA, false),
#endif

    { -1, NULL }
};

static int ciphers_len = array_size(ciphers);

#if 0 // currently unused
static SSLCipherSuite sslcipher_atoi(const char *name)
{
       const CipherSuiteName *a = ciphers;
       while(a->name) {
           if (0 == strcmp(a->name, name)) break;
           a++;
       }
       return a->cipher;
}

static const char * sslcipher_itoa(SSLCipherSuite num)
{
       const CipherSuiteName *a = ciphers;
       while(a->cipher >= 0) {
           if (num == a->cipher) break;
           a++;
       }
       return a->name;
}
#endif // currently unused

static unsigned char dh_param_512_bytes[] = {
  0x30, 0x46, 0x02, 0x41, 0x00, 0xdb, 0x3c, 0xfa, 0x13, 0xa6, 0xd2, 0x64,
  0xdf, 0xcc, 0x40, 0xb1, 0x21, 0xd4, 0xf2, 0xad, 0x22, 0x7f, 0xce, 0xa0,
  0xb9, 0x5b, 0x95, 0x1c, 0x2e, 0x99, 0xb0, 0x27, 0xd0, 0xed, 0xf4, 0xbd,
  0xbb, 0x36, 0x93, 0xd0, 0x9d, 0x2b, 0x32, 0xa3, 0x56, 0x53, 0xe3, 0x7b,
  0xed, 0xa1, 0x71, 0x82, 0x2e, 0x83, 0x14, 0xf9, 0xc0, 0x2f, 0x15, 0xcb,
  0xcf, 0x97, 0xab, 0x88, 0x49, 0x20, 0x28, 0x2e, 0x63, 0x02, 0x01, 0x02
};
static unsigned char *dh_param_512_der = dh_param_512_bytes;
static unsigned int dh_param_512_der_len = 72;

/* openssl req -newkey rsa:512 -sha1 -days 365  -subj "/C=US/O=Apple Inc./OU=Apple Certification Authority/CN=localhost" -x509 -nodes -outform DER -keyout privkey.der -outform der -out cert.der */
static unsigned char pkey_der[] = {
  0x30, 0x82, 0x01, 0x3b, 0x02, 0x01, 0x00, 0x02, 0x41, 0x00, 0xc0, 0x80,
  0x43, 0xf1, 0x4d, 0xdc, 0x9a, 0x24, 0xe7, 0x25, 0x7c, 0x8b, 0x8b, 0x65,
  0x87, 0x97, 0xed, 0x3f, 0xfa, 0xfe, 0xbe, 0xcb, 0x12, 0x43, 0x1f, 0x0c,
  0xb5, 0xbf, 0x6b, 0x81, 0xee, 0x1b, 0x46, 0x6a, 0x02, 0x86, 0x92, 0xec,
  0x8a, 0xb3, 0x65, 0x77, 0x15, 0xd0, 0x49, 0xb4, 0x22, 0x84, 0xf4, 0x85,
  0x56, 0x53, 0xf5, 0x5a, 0x3b, 0xad, 0x23, 0xa8, 0x0c, 0x24, 0xb7, 0xf5,
  0xf4, 0xa1, 0x02, 0x03, 0x01, 0x00, 0x01, 0x02, 0x41, 0x00, 0xb8, 0x7f,
  0xf7, 0x1e, 0xa7, 0x0e, 0xc1, 0x9a, 0x8f, 0x04, 0x49, 0xcb, 0x81, 0x4e,
  0x4d, 0x58, 0x5a, 0xe7, 0x10, 0x8c, 0xea, 0x96, 0xbd, 0xa9, 0x21, 0x70,
  0x50, 0x1d, 0xe8, 0x4f, 0x7e, 0xc2, 0x71, 0xff, 0x55, 0xc5, 0xa7, 0x28,
  0xc8, 0xf2, 0xc7, 0x19, 0xd1, 0x2c, 0x10, 0x40, 0x39, 0xa8, 0xe1, 0x5b,
  0xbd, 0x97, 0x04, 0xff, 0xd3, 0x27, 0x9b, 0xce, 0x5e, 0x8d, 0x2f, 0x0e,
  0xd9, 0xf1, 0x02, 0x21, 0x00, 0xde, 0xfc, 0x18, 0x88, 0xa4, 0xef, 0x3b,
  0x18, 0xca, 0x54, 0x3f, 0xa8, 0x14, 0x96, 0x9a, 0xd7, 0x67, 0x57, 0x55,
  0xdc, 0x6b, 0xd4, 0x8e, 0x7d, 0xb4, 0x32, 0x00, 0x63, 0x67, 0x6a, 0x57,
  0x65, 0x02, 0x21, 0x00, 0xdd, 0x00, 0xba, 0xdc, 0xa1, 0xe2, 0x5c, 0xda,
  0xfe, 0xfc, 0x50, 0x1e, 0x9b, 0x95, 0x28, 0x34, 0xf2, 0x52, 0x31, 0x7a,
  0x15, 0x00, 0x6f, 0xcc, 0x08, 0x2c, 0x6d, 0x55, 0xb0, 0x24, 0x6a, 0x8d,
  0x02, 0x20, 0x14, 0xf5, 0x7d, 0x18, 0xda, 0xe7, 0xe1, 0x96, 0x22, 0xee,
  0x68, 0x4d, 0x54, 0x22, 0x13, 0xcb, 0xcb, 0x5a, 0xda, 0x27, 0x2d, 0xbb,
  0x7c, 0xe9, 0x33, 0xd6, 0xbf, 0x52, 0x98, 0x95, 0xd6, 0x41, 0x02, 0x21,
  0x00, 0xaa, 0x58, 0x8c, 0xaf, 0xd1, 0x6b, 0xdc, 0x6c, 0xc4, 0xcc, 0x10,
  0xa9, 0x76, 0xfc, 0xc2, 0x50, 0x05, 0x53, 0xcb, 0x65, 0x31, 0x58, 0xf3,
  0xd3, 0x4d, 0x9d, 0x88, 0xec, 0xda, 0x67, 0x47, 0x65, 0x02, 0x20, 0x53,
  0xf2, 0x49, 0x77, 0x7e, 0x10, 0xc1, 0xc4, 0xed, 0xc0, 0xaf, 0x99, 0x79,
  0xab, 0x7b, 0x25, 0x0e, 0x70, 0x36, 0xd2, 0xd1, 0xa3, 0x81, 0x0d, 0x83,
  0x4f, 0x6b, 0x1b, 0x48, 0xec, 0x87, 0x90
};
static unsigned int pkey_der_len = 319;

static unsigned char cert_der[] = {
  0x30, 0x82, 0x02, 0x79, 0x30, 0x82, 0x02, 0x23, 0xa0, 0x03, 0x02, 0x01,
  0x02, 0x02, 0x09, 0x00, 0xc2, 0xa8, 0x3b, 0xaa, 0x40, 0xa4, 0x29, 0x2b,
  0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01,
  0x05, 0x05, 0x00, 0x30, 0x5e, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55,
  0x04, 0x06, 0x13, 0x02, 0x55, 0x53, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03,
  0x55, 0x04, 0x0a, 0x13, 0x0a, 0x41, 0x70, 0x70, 0x6c, 0x65, 0x20, 0x49,
  0x6e, 0x63, 0x2e, 0x31, 0x26, 0x30, 0x24, 0x06, 0x03, 0x55, 0x04, 0x0b,
  0x13, 0x1d, 0x41, 0x70, 0x70, 0x6c, 0x65, 0x20, 0x43, 0x65, 0x72, 0x74,
  0x69, 0x66, 0x69, 0x63, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x20, 0x41, 0x75,
  0x74, 0x68, 0x6f, 0x72, 0x69, 0x74, 0x79, 0x31, 0x12, 0x30, 0x10, 0x06,
  0x03, 0x55, 0x04, 0x03, 0x13, 0x09, 0x6c, 0x6f, 0x63, 0x61, 0x6c, 0x68,
  0x6f, 0x73, 0x74, 0x30, 0x1e, 0x17, 0x0d, 0x30, 0x38, 0x30, 0x39, 0x31,
  0x35, 0x32, 0x31, 0x35, 0x30, 0x35, 0x36, 0x5a, 0x17, 0x0d, 0x30, 0x39,
  0x30, 0x39, 0x31, 0x35, 0x32, 0x31, 0x35, 0x30, 0x35, 0x36, 0x5a, 0x30,
  0x5e, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x02,
  0x55, 0x53, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x13,
  0x0a, 0x41, 0x70, 0x70, 0x6c, 0x65, 0x20, 0x49, 0x6e, 0x63, 0x2e, 0x31,
  0x26, 0x30, 0x24, 0x06, 0x03, 0x55, 0x04, 0x0b, 0x13, 0x1d, 0x41, 0x70,
  0x70, 0x6c, 0x65, 0x20, 0x43, 0x65, 0x72, 0x74, 0x69, 0x66, 0x69, 0x63,
  0x61, 0x74, 0x69, 0x6f, 0x6e, 0x20, 0x41, 0x75, 0x74, 0x68, 0x6f, 0x72,
  0x69, 0x74, 0x79, 0x31, 0x12, 0x30, 0x10, 0x06, 0x03, 0x55, 0x04, 0x03,
  0x13, 0x09, 0x6c, 0x6f, 0x63, 0x61, 0x6c, 0x68, 0x6f, 0x73, 0x74, 0x30,
  0x5c, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01,
  0x01, 0x01, 0x05, 0x00, 0x03, 0x4b, 0x00, 0x30, 0x48, 0x02, 0x41, 0x00,
  0xc0, 0x80, 0x43, 0xf1, 0x4d, 0xdc, 0x9a, 0x24, 0xe7, 0x25, 0x7c, 0x8b,
  0x8b, 0x65, 0x87, 0x97, 0xed, 0x3f, 0xfa, 0xfe, 0xbe, 0xcb, 0x12, 0x43,
  0x1f, 0x0c, 0xb5, 0xbf, 0x6b, 0x81, 0xee, 0x1b, 0x46, 0x6a, 0x02, 0x86,
  0x92, 0xec, 0x8a, 0xb3, 0x65, 0x77, 0x15, 0xd0, 0x49, 0xb4, 0x22, 0x84,
  0xf4, 0x85, 0x56, 0x53, 0xf5, 0x5a, 0x3b, 0xad, 0x23, 0xa8, 0x0c, 0x24,
  0xb7, 0xf5, 0xf4, 0xa1, 0x02, 0x03, 0x01, 0x00, 0x01, 0xa3, 0x81, 0xc3,
  0x30, 0x81, 0xc0, 0x30, 0x1d, 0x06, 0x03, 0x55, 0x1d, 0x0e, 0x04, 0x16,
  0x04, 0x14, 0xe3, 0x58, 0xab, 0x35, 0xc0, 0x58, 0xb8, 0x65, 0x40, 0xca,
  0x9b, 0x6c, 0xeb, 0x2f, 0xf5, 0xbf, 0xbd, 0x0b, 0xf3, 0xa6, 0x30, 0x81,
  0x90, 0x06, 0x03, 0x55, 0x1d, 0x23, 0x04, 0x81, 0x88, 0x30, 0x81, 0x85,
  0x80, 0x14, 0xe3, 0x58, 0xab, 0x35, 0xc0, 0x58, 0xb8, 0x65, 0x40, 0xca,
  0x9b, 0x6c, 0xeb, 0x2f, 0xf5, 0xbf, 0xbd, 0x0b, 0xf3, 0xa6, 0xa1, 0x62,
  0xa4, 0x60, 0x30, 0x5e, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04,
  0x06, 0x13, 0x02, 0x55, 0x53, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55,
  0x04, 0x0a, 0x13, 0x0a, 0x41, 0x70, 0x70, 0x6c, 0x65, 0x20, 0x49, 0x6e,
  0x63, 0x2e, 0x31, 0x26, 0x30, 0x24, 0x06, 0x03, 0x55, 0x04, 0x0b, 0x13,
  0x1d, 0x41, 0x70, 0x70, 0x6c, 0x65, 0x20, 0x43, 0x65, 0x72, 0x74, 0x69,
  0x66, 0x69, 0x63, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x20, 0x41, 0x75, 0x74,
  0x68, 0x6f, 0x72, 0x69, 0x74, 0x79, 0x31, 0x12, 0x30, 0x10, 0x06, 0x03,
  0x55, 0x04, 0x03, 0x13, 0x09, 0x6c, 0x6f, 0x63, 0x61, 0x6c, 0x68, 0x6f,
  0x73, 0x74, 0x82, 0x09, 0x00, 0xc2, 0xa8, 0x3b, 0xaa, 0x40, 0xa4, 0x29,
  0x2b, 0x30, 0x0c, 0x06, 0x03, 0x55, 0x1d, 0x13, 0x04, 0x05, 0x30, 0x03,
  0x01, 0x01, 0xff, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7,
  0x0d, 0x01, 0x01, 0x05, 0x05, 0x00, 0x03, 0x41, 0x00, 0x41, 0x40, 0x07,
  0xde, 0x1f, 0xd0, 0x00, 0x62, 0x75, 0x36, 0xb3, 0x94, 0xa8, 0xac, 0x3b,
  0x98, 0xbb, 0x28, 0x56, 0xf6, 0x9f, 0xe3, 0x87, 0xd4, 0xa1, 0x7a, 0x85,
  0xce, 0x40, 0x8a, 0xfd, 0x12, 0xb4, 0x99, 0x8c, 0x1d, 0x05, 0x61, 0xdb,
  0x35, 0xb8, 0x04, 0x7c, 0xfb, 0xe4, 0x97, 0x88, 0x66, 0xa0, 0x54, 0x7b,
  0x1c, 0xce, 0x99, 0xd8, 0xd3, 0x99, 0x80, 0x40, 0x9b, 0xa2, 0x73, 0x8b,
  0xfd
};
static unsigned int cert_der_len = 637;

typedef struct {
    uint32_t session_id;
    bool is_session_resume;
    SSLContextRef handle;
    bool is_st;
    bool is_server;
    bool client_side_auth;
    bool dh_anonymous;
    int comm;
    CFArrayRef certs;
} ssl_test_handle;

#if 0 // currently unused
static CFArrayRef SecIdentityCopySSLClientAuthenticationChain(SecIdentityRef identity)
{
   CFMutableArrayRef chain = NULL;
   SecPolicyRef policy = NULL;
   SecTrustRef trust = NULL;
   SecTrustResultType trust_result;

   do {
       policy = SecPolicyCreateSSL(false, NULL);
       if (!policy)
           break;

       SecCertificateRef cert = NULL;
       if (SecIdentityCopyCertificate(identity, &cert))
           break;

       CFArrayRef certs = CFArrayCreate(NULL, (const void **)&cert,
                 1, &kCFTypeArrayCallBacks);
       CFRelease(cert);
       if (!certs)
           break;

       if (SecTrustCreateWithCertificates(certs, policy, &trust))
           break;
       CFRelease(certs);
       CFRelease(policy);
       if (SecTrustEvaluate(trust, &trust_result))
           break;

       int i, count = SecTrustGetCertificateCount(trust);
       chain = CFArrayCreateMutable(NULL, count, &kCFTypeArrayCallBacks);
       CFArrayAppendValue(chain, identity);
       for (i = 1; i < count; i++) {
           if ((i+1 == count) && (trust_result == kSecTrustResultUnspecified))
               continue; /* skip anchor if chain is complete */
           SecCertificateRef s = SecTrustGetCertificateAtIndex(trust, i);
           CFArrayAppendValue(chain, s);
       }
   } while (0);
   if (trust)
       CFRelease(trust);
   if (policy)
       CFRelease(policy);
   return chain;
}
#endif // currently unused

static CFArrayRef server_chain()
{
    SecKeyRef pkey = SecKeyCreateRSAPrivateKey(kCFAllocatorDefault,
        pkey_der, pkey_der_len, kSecKeyEncodingPkcs1);
    SecCertificateRef cert = SecCertificateCreateWithBytes(kCFAllocatorDefault,
        cert_der, cert_der_len);
    SecIdentityRef ident = SecIdentityCreate(kCFAllocatorDefault, cert, pkey);
    CFRelease(pkey);
    CFRelease(cert);
    CFArrayRef items = CFArrayCreate(kCFAllocatorDefault,
        (const void **)&ident, 1, &kCFTypeArrayCallBacks);
    CFRelease(ident);
    return items;
}

// MARK: -
// MARK: SecureTransport support

#if 0
static void hexdump(const uint8_t *bytes, size_t len) {
	size_t ix;
    printf("socket write(%p, %lu)\n", bytes, len);
	for (ix = 0; ix < len; ++ix) {
        if (!(ix % 16))
            printf("\n");
		printf("%02X ", bytes[ix]);
	}
	printf("\n");
}
#else
#define hexdump(bytes, len)
#endif

static OSStatus SocketWrite(SSLConnectionRef conn, const void *data, size_t *length)
{
	size_t len = *length;
	uint8_t *ptr = (uint8_t *)data;

    do {
        ssize_t ret;
        do {
            hexdump(ptr, len);
            ret = write((int)conn, ptr, len);
            if (ret < 0)
                perror("send");
        } while ((ret < 0) && (errno == EAGAIN || errno == EINTR));
        if (ret > 0) {
            len -= ret;
            ptr += ret;
        }
        else
            return -36;
    } while (len > 0);

    *length = *length - len;
    return errSecSuccess;
}

static OSStatus SocketRead(SSLConnectionRef conn, void *data, size_t *length)
{
	size_t len = *length;
	uint8_t *ptr = (uint8_t *)data;

    do {
        ssize_t ret;
        do {
            ret = read((int)conn, ptr, len);
            if (ret < 0)
                perror("send");
        } while ((ret < 0) && (errno == EAGAIN || errno == EINTR));
        if (ret > 0) {
            len -= ret;
            ptr += ret;
        }
        else
            return -36;
    } while (len > 0);

    *length = *length - len;
    return errSecSuccess;
}

static unsigned char dn[] = {
  0x30, 0x5e, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13,
  0x02, 0x55, 0x53, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x0a,
  0x13, 0x0a, 0x41, 0x70, 0x70, 0x6c, 0x65, 0x20, 0x49, 0x6e, 0x63, 0x2e,
  0x31, 0x26, 0x30, 0x24, 0x06, 0x03, 0x55, 0x04, 0x0b, 0x13, 0x1d, 0x41,
  0x70, 0x70, 0x6c, 0x65, 0x20, 0x43, 0x65, 0x72, 0x74, 0x69, 0x66, 0x69,
  0x63, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x20, 0x41, 0x75, 0x74, 0x68, 0x6f,
  0x72, 0x69, 0x74, 0x79, 0x31, 0x12, 0x30, 0x10, 0x06, 0x03, 0x55, 0x04,
  0x03, 0x13, 0x09, 0x6c, 0x6f, 0x63, 0x61, 0x6c, 0x68, 0x6f, 0x73, 0x74
};
static unsigned int dn_len = 96;

static SSLContextRef make_ssl_ref(bool server, bool client_side_auth, bool dh_anonymous,
    bool dtls, int sock, CFArrayRef certs)
{
    SSLContextRef ctx = NULL;
    if(dtls)
        require_noerr(SSLNewDatagramContext(server, &ctx), out);
    else
        require_noerr(SSLNewContext(server, &ctx), out);
    require_noerr(SSLSetIOFuncs(ctx,
        (SSLReadFunc)SocketRead, (SSLWriteFunc)SocketWrite), out);
    require_noerr(SSLSetConnection(ctx, (SSLConnectionRef)sock), out);
    static const char *peer_domain_name = "localhost";
    require_noerr(SSLSetPeerDomainName(ctx, peer_domain_name,
        strlen(peer_domain_name)), out);

    if (!dh_anonymous) {
        if (server)
            require_noerr(SSLSetCertificate(ctx, certs), out);
        if (client_side_auth && server) {
            require_noerr(SSLSetClientSideAuthenticate(ctx, kAlwaysAuthenticate), out);
            require_noerr(SSLAddDistinguishedName(ctx, dn, dn_len), out);
        }
#if 0 /* Setting client certificate in advance */
        if (client_side_auth && !server)
            require_noerr(SSLSetCertificate(ctx, certs), out);
#endif
        if (client_side_auth && !server) /* enable break from SSLHandshake */
            require_noerr(SSLSetSessionOption(ctx,
                kSSLSessionOptionBreakOnCertRequested, true), out);
        require_noerr(SSLSetSessionOption(ctx,
            kSSLSessionOptionBreakOnServerAuth, true), out);
    }

    /* Tell SecureTransport to not check certs itself: it will break out of the
       handshake to let us take care of it instead. */
    require_noerr(SSLSetEnableCertVerify(ctx, false), out);

    if (server) {
        require_noerr(SSLSetDiffieHellmanParams(ctx,
            dh_param_512_der, dh_param_512_der_len), out);
    }
    else /* if client */ {
    }

    return ctx;
out:
    if (ctx)
        SSLDisposeContext(ctx);
    return NULL;
}

static void *securetransport_ssl_thread(void *arg)
{
    OSStatus ortn;
    ssl_test_handle * ssl = (ssl_test_handle *)arg;
    SSLContextRef ctx = ssl->handle;
    SecTrustRef trust = NULL;
    bool got_server_auth = false, got_client_cert_req = false;

    //uint64_t start = mach_absolute_time();
    do {
        ortn = SSLHandshake(ctx);

        if (ortn == errSSLServerAuthCompleted)
        {
            require_string(!got_server_auth, out, "second server auth");
            require_string(!got_client_cert_req, out, "got client cert req before server auth");
            got_server_auth = true;
            require_string(!trust, out, "Got errSSLServerAuthCompleted twice?");
            /* verify peer cert chain */
            require_noerr(SSLCopyPeerTrust(ctx, &trust), out);
            SecTrustResultType trust_result = 0;
            /* this won't verify without setting up a trusted anchor */
            require_noerr(SecTrustEvaluate(trust, &trust_result), out);

            CFIndex n_certs = SecTrustGetCertificateCount(trust);
            /*fprintf(stderr, "%ld certs; trust_eval: %d\n", n_certs, trust_result); */

            CFMutableArrayRef peer_cert_array =
                CFArrayCreateMutable(NULL, n_certs, &kCFTypeArrayCallBacks);
            CFMutableArrayRef orig_peer_cert_array =
                CFArrayCreateMutableCopy(NULL, n_certs, ssl->certs);
            while (n_certs--)
                CFArrayInsertValueAtIndex(peer_cert_array, 0,
                    SecTrustGetCertificateAtIndex(trust, n_certs));

            SecIdentityRef ident =
                (SecIdentityRef)CFArrayGetValueAtIndex(orig_peer_cert_array, 0);
            SecCertificateRef peer_cert = NULL;
            require_noerr(SecIdentityCopyCertificate(ident, &peer_cert), out);
            CFArraySetValueAtIndex(orig_peer_cert_array, 0, peer_cert);
            CFRelease(peer_cert);

            require(CFEqual(orig_peer_cert_array, peer_cert_array), out);
            CFRelease(orig_peer_cert_array);
            CFRelease(peer_cert_array);

            /*
            CFStringRef cert_name = SecCertificateCopySubjectSummary(cert);
            char cert_name_buffer[1024];
            require(CFStringGetFileSystemRepresentation(cert_name,
                cert_name_buffer, sizeof(cert_name_buffer)), out);
            fprintf(stderr, "cert name: %s\n", cert_name_buffer);
            CFRelease(trust);
            */
        } else if (ortn == errSSLClientCertRequested) {
            require_string(!got_client_cert_req, out, "second client cert req");
            require_string(got_server_auth, out, "didn't get server auth first");
            got_client_cert_req = true;

            /* set client cert */
            require_string(!ssl->is_server, out, "errSSLClientCertRequested while running server");
            require_string(!ssl->dh_anonymous, out, "errSSLClientCertRequested while running anon DH");

            CFArrayRef DNs = NULL;
            require_noerr(SSLCopyDistinguishedNames	(ctx, &DNs), out);
            require(DNs, out);
            CFRelease(DNs);

            require_string(ssl->client_side_auth, out, "errSSLClientCertRequested in run not testing that");
            require_noerr(SSLSetCertificate(ctx, ssl->certs), out);
        }
    } while (ortn == errSSLWouldBlock
        || ortn == errSSLServerAuthCompleted
        || ortn == errSSLClientCertRequested);
    require_noerr_action_quiet(ortn, out,
        fprintf(stderr, "Fell out of SSLHandshake with error: %d\n", (int)ortn));

    if (!ssl->is_server && !ssl->dh_anonymous && !ssl->is_session_resume) {
        require_string(got_server_auth, out, "never got server auth");
        if (ssl->client_side_auth)
            require_string(got_client_cert_req, out, "never got client cert req");
    }
    //uint64_t elapsed = mach_absolute_time() - start;
    //fprintf(stderr, "setr elapsed: %lld\n", elapsed);

    /*
    SSLProtocol proto = kSSLProtocolUnknown;
    require_noerr_quiet(SSLGetNegotiatedProtocolVersion(ctx, &proto), out); */

    SSLCipherSuite cipherSuite;
    require_noerr_quiet(ortn = SSLGetNegotiatedCipher(ctx, &cipherSuite), out);
    //fprintf(stderr, "st negotiated %s\n", sslcipher_itoa(cipherSuite));

	Boolean	sessionWasResumed = false;
    uint8_t session_id_data[MAX_SESSION_ID_LENGTH];
    size_t session_id_length = sizeof(session_id_data);
    require_noerr_quiet(ortn = SSLGetResumableSessionInfo(ctx, &sessionWasResumed, session_id_data, &session_id_length), out);
    require_action(ssl->dh_anonymous || (ssl->is_session_resume == sessionWasResumed), out, ortn = -1);
    // if (sessionWasResumed) fprintf(stderr, "st resumed session\n");
    //hexdump(session_id_data, session_id_length);

    unsigned char ibuf[4096], obuf[4096];
    size_t len;
    if (ssl->is_server) {
        require_action(errSecSuccess==SecRandomCopyBytes(kSecRandomDefault, sizeof(obuf), obuf),out, ortn = -1);
        require_noerr_quiet(ortn = SSLWrite(ctx, obuf, sizeof(obuf), &len), out);
        require_action_quiet(len == sizeof(obuf), out, ortn = -1);
    }
    require_noerr_quiet(ortn = SSLRead(ctx, ibuf, sizeof(ibuf), &len), out);
    require_action_quiet(len == sizeof(ibuf), out, ortn = -1);

    if (ssl->is_server) {
        require_noerr(memcmp(ibuf, obuf, sizeof(ibuf)), out);
    } else {
        require_noerr_quiet(ortn = SSLWrite(ctx, ibuf, sizeof(ibuf), &len), out);
        require_action_quiet(len == sizeof(ibuf), out, ortn = -1);
    }

out:
    SSLClose(ctx);
    SSLDisposeContext(ctx);
    if (trust) CFRelease(trust);

    pthread_exit((void *)(intptr_t)ortn);
    return NULL;
}



static ssl_test_handle *
ssl_test_handle_create(uint32_t session_id, bool resume, bool server, bool client_side_auth, bool dh_anonymous, bool dtls,
    int comm, CFArrayRef certs)
{
    ssl_test_handle *handle = calloc(1, sizeof(ssl_test_handle));
    if (handle) {
        handle->session_id = session_id;
        handle->is_session_resume = resume;
        handle->is_server = server;
        handle->client_side_auth = client_side_auth;
        handle->dh_anonymous = dh_anonymous;
        handle->comm = comm;
        handle->certs = certs;
        handle->handle = make_ssl_ref(server, client_side_auth, dh_anonymous, dtls, comm, certs);
    }
    return handle;
}

static void
tests(void)
{
    pthread_t client_thread, server_thread;
    CFArrayRef server_certs = server_chain();
    ok(server_certs, "got server certs");

#if 0
    int i=0, j=1, k=1; {
#else
    int d,i,k,l;
    for (d=0;d<2; d++)  /* dtls or not dtls */
    for (k=0; k<2; k++)
    for (i=0; ciphers[i].cipher != (SSLCipherSuite)(-1); i++)
    for (l = 0; l<2; l++) {
#endif
        SKIP: {
            skip("ST doesn't support resumption", 1, l != 1);

            int sp[2];
            if (socketpair(AF_UNIX, SOCK_STREAM, 0, sp)) exit(errno);

            ssl_test_handle *server, *client;

            bool client_side_auth = (k);

            uint32_t session_id = (k+1) << 16 | 1 << 8 | (i+1);
            //fprintf(stderr, "session_id: %d\n", session_id);
            server = ssl_test_handle_create(session_id, (l == 1), true /*server*/,
                client_side_auth, ciphers[i].dh_anonymous, d,
                sp[0], server_certs);
            client = ssl_test_handle_create(session_id, (l == 1), false/*client*/,
                client_side_auth, ciphers[i].dh_anonymous, d,
                sp[1], server_certs);

            require_noerr(SSLSetPeerID(server->handle, &session_id, sizeof(session_id)), out);
            require_noerr(SSLSetPeerID(client->handle, &session_id, sizeof(session_id)), out);

            /* set fixed cipher on client and server */
            require_noerr(SSLSetEnabledCiphers(client->handle, &ciphers[i].cipher, 1), out);
            require_noerr(SSLSetEnabledCiphers(server->handle, &ciphers[i].cipher, 1), out);

            char test_description[1024];
            snprintf(test_description, sizeof(test_description),
                     "%40s ADH:%d CSA:%d DTLS:%d",
                     ciphers[i].name,
                     server->dh_anonymous,
                     server->client_side_auth,
                     d);

            printf("Echo test: %s\n", test_description);

            pthread_create(&client_thread, NULL, securetransport_ssl_thread, client);
            pthread_create(&server_thread, NULL, securetransport_ssl_thread, server);

            int server_err, client_err;
            pthread_join(client_thread, (void*)&client_err);
            pthread_join(server_thread, (void*)&server_err);

            ok(!server_err && !client_err, "%40s ADH:%d CSA:%d DTLS:%d",
               ciphers[i].name,
               server->dh_anonymous,
               server->client_side_auth,
               d);
        out:
            close(sp[0]);
            close(sp[1]);
        }
    } /* all configs */

    CFRelease(server_certs);

}

int ssl_39_echo(int argc, char *const *argv)
{
    plan_tests(2 * 2 * 2 * 3 * (ciphers_len-1)/* client auth on/off * #configs * #ciphers */
                + 1 /*cert*/);


    tests();

    return 0;
}

#endif

/*
TODO: count errSSLWouldBlock
TODO: skip tests that don't matter: client_auth and anonymous dh
TODO: we seem to only be negotiating tls - force a round of sslv3
TODO: allow secure transport to also defer client side auth to client
TODO: make sure anonymous dh is never selected if not expicitly enabled
TODO: make sure DHE is not available if not explicitly enabled and no parameters
      are set
TODO: resumable sessions
*/
