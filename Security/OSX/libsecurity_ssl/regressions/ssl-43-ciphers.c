/*
 * Copyright (c) 2011-2014 Apple Inc. All Rights Reserved.
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


#include <stdbool.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
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

#include <utilities/array_size.h>
#include <utilities/SecIOFormat.h>

#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdlib.h>
#include <mach/mach_time.h>


#include "ssl_regressions.h"
#include "ssl-utils.h"

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


//#define OPENSSL_SERVER "ariadne.apple.com"
//#define GNUTLS_SERVER "ariadne.apple.com"
//#define OPENSSL_SERVER "kuip.apple.com"
//#define GNUTLS_SERVER "kuip.apple.com"
#define OPENSSL_SERVER "192.168.2.1"
#define GNUTLS_SERVER "192.168.2.1"

static struct {
    const char *host;
    int base_port;
    int cs_index;
    bool client_auth;
} servers[] = {
    { OPENSSL_SERVER, 4000, 0, false}, //openssl s_server w/o client side auth
    { GNUTLS_SERVER, 5000, 1, false}, // gnutls-serv w/o client side auth
//  { "www.mikestoolbox.org", 442, 2, false}, // mike's  w/o client side auth
//  { "tls.secg.org", 40022, 3, false}, // secg ecc server w/o client side auth - This server generate DH params we didnt support, but this should be fixed now
    { OPENSSL_SERVER, 4010, 0, true}, //openssl s_server w/ client side auth
    { GNUTLS_SERVER, 5010, 1, true}, // gnutls-serv w/ client side auth
//  { "www.mikestoolbox.net", 442, 2, true}, // mike's  w/ client side auth
//  { "tls.secg.org", 8442, 3}, //secg ecc server w/ client side auth
};
int nservers = sizeof(servers)/sizeof(servers[0]);

int protos[]={ kSSLProtocol3, kTLSProtocol1, kTLSProtocol11, kTLSProtocol12 };
int nprotos = sizeof(protos)/sizeof(protos[0]);

typedef struct _CipherSuiteName {
    int prot;
    SSLCipherSuite cipher;
    const char *name;
    int portoffset[4]; // 0=not supported , else = port offset for this ciphersuite
    bool dh_anonymous;
} CipherSuiteName;

/* prot: 0 = SSL3, 1=TLSv1.0, 2=TLSv1.1, 3=TLSv1.2 */
#define CIPHER(prot, cipher, offsets...) { prot, cipher, #cipher, offsets},

const CipherSuiteName ciphers[] = {
    //SSL_NULL_WITH_NULL_NULL, unsupported
#if 1
    /* RSA cipher suites */
    CIPHER(1, SSL_RSA_WITH_NULL_MD5,    {1, 1, 0, 1}, false)
    CIPHER(1, SSL_RSA_WITH_NULL_SHA,    {1, 1, 0, 1}, false)
    CIPHER(3, TLS_RSA_WITH_NULL_SHA256, {0, 1, 0, 0}, false)
#endif

#if 1
    CIPHER(1, SSL_RSA_WITH_RC4_128_MD5,         {1, 1, 1, 1}, false)
    CIPHER(1, SSL_RSA_WITH_RC4_128_SHA,         {1, 1, 1, 1}, false)
    CIPHER(1, SSL_RSA_WITH_3DES_EDE_CBC_SHA,    {1, 1, 1, 1}, false)
    CIPHER(1, TLS_RSA_WITH_AES_128_CBC_SHA,     {1, 1, 1, 1}, false)
    CIPHER(3, TLS_RSA_WITH_AES_128_CBC_SHA256,  {0, 1, 1, 0}, false)
    CIPHER(1, TLS_RSA_WITH_AES_256_CBC_SHA,     {1, 1, 1, 1}, false)
    CIPHER(3, TLS_RSA_WITH_AES_256_CBC_SHA256,  {0, 1, 1, 0}, false)
#endif

#if 1
    /* DHE_RSA ciphers suites */
    CIPHER(1, SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA,    {1, 1, 1, 1}, false)
    CIPHER(1, TLS_DHE_RSA_WITH_AES_128_CBC_SHA,     {1, 1, 1, 1}, false)
    CIPHER(3, TLS_DHE_RSA_WITH_AES_128_CBC_SHA256,  {0, 1, 1, 0}, false)
    CIPHER(1, TLS_DHE_RSA_WITH_AES_256_CBC_SHA,     {1, 1, 1, 1}, false)
    CIPHER(3, TLS_DHE_RSA_WITH_AES_256_CBC_SHA256,  {0, 1, 1, 0}, false)
#endif


#if 1
    /* DH_anon cipher suites */
    CIPHER(0, SSL_DH_anon_WITH_RC4_128_MD5,         {1, 1, 0, 1}, true)
    CIPHER(0, SSL_DH_anon_WITH_3DES_EDE_CBC_SHA,    {1, 1, 0, 1}, true)
    CIPHER(0, TLS_DH_anon_WITH_AES_128_CBC_SHA,     {1, 1, 0, 1}, true)
    CIPHER(3, TLS_DH_anon_WITH_AES_128_CBC_SHA256,  {0, 1, 0, 1}, true)
    CIPHER(0, TLS_DH_anon_WITH_AES_256_CBC_SHA,     {1, 1, 0, 1}, true)
    CIPHER(3, TLS_DH_anon_WITH_AES_256_CBC_SHA256,  {0, 1, 0, 1}, true)
#endif

#if 1
    /* ECDHE_ECDSA cipher suites */
    CIPHER(1, TLS_ECDHE_ECDSA_WITH_NULL_SHA,            {4, 4, 0, 1}, false)
    CIPHER(1, TLS_ECDHE_ECDSA_WITH_RC4_128_SHA,         {4, 0, 0, 1}, false)
    CIPHER(1, TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA,    {4, 4, 0, 1}, false)
    CIPHER(1, TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,     {4, 4, 0, 1}, false)
    CIPHER(3, TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256,  {0, 4, 0, 1}, false)
    CIPHER(1, TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,     {4, 4, 0, 1}, false)
    CIPHER(3, TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384,  {0, 4, 0, 1}, false)
#endif

#if 1
    /* ECDHE_RSA cipher suites */
    CIPHER(1, TLS_ECDHE_RSA_WITH_RC4_128_SHA,           {1, 0, 0, 1}, false)
    CIPHER(1, TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA,      {1, 1, 0, 1}, false)
    CIPHER(1, TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,       {1, 1, 0, 1}, false)
    CIPHER(3, TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256,    {0, 1, 0, 0}, false)
    CIPHER(1, TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA,       {1, 1, 0, 1}, false)
    CIPHER(3, TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384,    {0, 0, 0, 0}, false) // Not supported by either gnutls or openssl
#endif

#if 0
    CIPHER(1, TLS_PSK_WITH_RC4_128_SHA,                 {1, 1, 0, 0}, true)
    CIPHER(1, TLS_PSK_WITH_3DES_EDE_CBC_SHA,            {1, 1, 0, 0}, true)
    CIPHER(1, TLS_PSK_WITH_AES_128_CBC_SHA,             {1, 1, 0, 0}, true)
    CIPHER(1, TLS_PSK_WITH_AES_256_CBC_SHA,             {1, 1, 0, 0}, true)
    CIPHER(3, TLS_PSK_WITH_AES_128_CBC_SHA256,          {0, 1, 0, 0}, true)
    CIPHER(3, TLS_PSK_WITH_AES_256_CBC_SHA384,          {0, 0, 0, 0}, true)
    CIPHER(1, TLS_PSK_WITH_NULL_SHA,                    {0, 0, 0, 0}, true)
    CIPHER(3, TLS_PSK_WITH_NULL_SHA256,                 {0, 1, 0, 0}, true)
    CIPHER(3, TLS_PSK_WITH_NULL_SHA384,                 {0, 0, 0, 0}, true)
#endif

#if 1
    CIPHER(3, TLS_RSA_WITH_AES_128_GCM_SHA256,          {1, 1, 0, 0}, false)
    CIPHER(3, TLS_RSA_WITH_AES_256_GCM_SHA384,          {1, 0, 0, 0}, false)

    CIPHER(3, TLS_DHE_RSA_WITH_AES_128_GCM_SHA256,      {1, 1, 0, 0}, false)
    CIPHER(3, TLS_DHE_RSA_WITH_AES_256_GCM_SHA384,      {1, 0, 0, 0}, false)

    CIPHER(3, TLS_DH_anon_WITH_AES_128_GCM_SHA256,      {1, 1, 0, 0}, true)
    CIPHER(3, TLS_DH_anon_WITH_AES_256_GCM_SHA384,      {1, 0, 0, 0}, true)

    CIPHER(3, TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,    {1, 1, 0, 0}, false)
    CIPHER(3, TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,    {1, 1, 0, 0}, false)

    CIPHER(3, TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,  {4, 4, 0, 0}, false)
    CIPHER(3, TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,  {4, 0, 0, 0}, false)
#endif



#if 0
    CIPHER(SSL_RSA_EXPORT_WITH_RC4_40_MD5, true, false, true,false)
    CIPHER(SSL_RSA_EXPORT_WITH_DES40_CBC_SHA, true, false, true, false)
    CIPHER(SSL_RSA_WITH_DES_CBC_SHA, true, false, true, false)
    CIPHER(SSL_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA, true, false, true, false)
    CIPHER(SSL_DHE_RSA_WITH_DES_CBC_SHA, true, false, true, false)
    CIPHER(SSL_DH_anon_EXPORT_WITH_RC4_40_MD5, true, false, true, true)
    CIPHER(SSL_DH_anon_EXPORT_WITH_DES40_CBC_SHA, true, false, true, true)
    CIPHER(SSL_DH_anon_WITH_DES_CBC_SHA, true, false, true, true)
#endif

#if 0
    /* "Any" cipher suite - test the default configuration */
    {0, SSL_NO_SUCH_CIPHERSUITE, "Any cipher 1", {1, 1, 1, 1}, false},
    {0, SSL_NO_SUCH_CIPHERSUITE, "Any cipher 2", {2, 2, 0, 0}, false},

    // Those servers wont talk SSL3.0 because they have EC certs
    {1, SSL_NO_SUCH_CIPHERSUITE, "Any cipher 3", {3, 3, 0, 0}, false},
    {1, SSL_NO_SUCH_CIPHERSUITE, "Any cipher 4", {4, 4, 0, 0}, false},
#endif

    { -1, -1, NULL, }
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

unsigned char dh_param_512_bytes[] = {
    0x30, 0x46, 0x02, 0x41, 0x00, 0xdb, 0x3c, 0xfa, 0x13, 0xa6, 0xd2, 0x64,
    0xdf, 0xcc, 0x40, 0xb1, 0x21, 0xd4, 0xf2, 0xad, 0x22, 0x7f, 0xce, 0xa0,
    0xb9, 0x5b, 0x95, 0x1c, 0x2e, 0x99, 0xb0, 0x27, 0xd0, 0xed, 0xf4, 0xbd,
    0xbb, 0x36, 0x93, 0xd0, 0x9d, 0x2b, 0x32, 0xa3, 0x56, 0x53, 0xe3, 0x7b,
    0xed, 0xa1, 0x71, 0x82, 0x2e, 0x83, 0x14, 0xf9, 0xc0, 0x2f, 0x15, 0xcb,
    0xcf, 0x97, 0xab, 0x88, 0x49, 0x20, 0x28, 0x2e, 0x63, 0x02, 0x01, 0x02
};
unsigned char *dh_param_512_der = dh_param_512_bytes;
unsigned int dh_param_512_der_len = 72;

typedef struct {
    uint32_t session_id;
    bool is_session_resume;
    SSLContextRef st;
    bool is_server;
    bool client_side_auth;
    bool dh_anonymous;
    int comm;
    CFArrayRef certs;
} ssl_test_handle;




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

static int SocketConnect(const char *hostName, int port)
{
    struct sockaddr_in  addr;
    struct in_addr      host;
	int					sock;
    int                 err;
    struct hostent      *ent = NULL;

    if (hostName[0] >= '0' && hostName[0] <= '9')
    {
        host.s_addr = inet_addr(hostName);
    }
    else {
		unsigned dex;
#define GETHOST_RETRIES 5
		/* seeing a lot of soft failures here that I really don't want to track down */
		for(dex=0; dex<GETHOST_RETRIES; dex++) {
			if(dex != 0) {
				printf("\n...retrying gethostbyname(%s)", hostName);
			}
			ent = gethostbyname(hostName);
			if(ent != NULL) {
				break;
			}
		}
        if(ent == NULL) {
			printf("\n***gethostbyname(%s) returned: %s\n", hostName, hstrerror(h_errno));
            return -1;
        }
        memcpy(&host, ent->h_addr, sizeof(struct in_addr));
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    addr.sin_addr = host;
    addr.sin_port = htons((u_short)port);

    addr.sin_family = AF_INET;
    err = connect(sock, (struct sockaddr *) &addr, sizeof(struct sockaddr_in));

    if(err!=0)
    {
        perror("connect failed");
        return err;
    }

    return sock;
}


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

unsigned char dn[] = {
    0x30, 0x5e, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13,
    0x02, 0x55, 0x53, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x0a,
    0x13, 0x0a, 0x41, 0x70, 0x70, 0x6c, 0x65, 0x20, 0x49, 0x6e, 0x63, 0x2e,
    0x31, 0x26, 0x30, 0x24, 0x06, 0x03, 0x55, 0x04, 0x0b, 0x13, 0x1d, 0x41,
    0x70, 0x70, 0x6c, 0x65, 0x20, 0x43, 0x65, 0x72, 0x74, 0x69, 0x66, 0x69,
    0x63, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x20, 0x41, 0x75, 0x74, 0x68, 0x6f,
    0x72, 0x69, 0x74, 0x79, 0x31, 0x12, 0x30, 0x10, 0x06, 0x03, 0x55, 0x04,
    0x03, 0x13, 0x09, 0x6c, 0x6f, 0x63, 0x61, 0x6c, 0x68, 0x6f, 0x73, 0x74
};
unsigned int dn_len = 96;

static SSLContextRef make_ssl_ref(bool server, bool client_side_auth, bool dh_anonymous,
                                  bool dtls, int sock, CFArrayRef certs, SSLProtocol prot)
{
    SSLContextRef ctx = NULL;
    if(dtls)
        require_noerr(SSLNewDatagramContext(server, &ctx), out);
    else
        require_noerr(SSLNewContext(server, &ctx), out);
    require_noerr(SSLSetProtocolVersionMax(ctx, prot), out);
    require_noerr(SSLSetIOFuncs(ctx,
                                (SSLReadFunc)SocketRead, (SSLWriteFunc)SocketWrite), out);
    require_noerr(SSLSetConnection(ctx, (SSLConnectionRef)(intptr_t)sock), out);
    //static const char *peer_domain_name = "localhost";
    //require_noerr(SSLSetPeerDomainName(ctx, peer_domain_name,
    //                                  strlen(peer_domain_name)), out);


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

    require_noerr(SSLSetPSKIdentity(ctx, "Client_identity", 15), out);
    require_noerr(SSLSetPSKSharedSecret(ctx, "123456789", 9), out);


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

static OSStatus securetransport(ssl_test_handle * ssl)
{
    OSStatus ortn;
    SSLContextRef ctx = ssl->st;
    SecTrustRef trust = NULL;
    bool got_server_auth = false, got_client_cert_req = false;
    CFMutableArrayRef peer_cert_array = NULL;
    CFMutableArrayRef orig_peer_cert_array = NULL;

    //uint64_t start = mach_absolute_time();
    do {
        ortn = SSLHandshake(ctx);

        if (ortn == errSSLServerAuthCompleted)
        {
            require_string(!got_server_auth, out, "second server auth");
            got_server_auth = true;
            require_string(!trust, out, "Got errSSLServerAuthCompleted twice?");
            /* verify peer cert chain */
            require_noerr(SSLCopyPeerTrust(ctx, &trust), out);
            SecTrustResultType trust_result = 0;
            /* this won't verify without setting up a trusted anchor */
            require_noerr(SecTrustEvaluate(trust, &trust_result), out);

            CFIndex n_certs = SecTrustGetCertificateCount(trust);
            /*fprintf(stderr, "%ld certs; trust_eval: %d\n", n_certs, trust_result); */

            peer_cert_array = CFArrayCreateMutable(NULL, n_certs, &kCFTypeArrayCallBacks);
            orig_peer_cert_array = CFArrayCreateMutableCopy(NULL, n_certs, ssl->certs);
            while (n_certs--)
                CFArrayInsertValueAtIndex(peer_cert_array, 0,
                                          SecTrustGetCertificateAtIndex(trust, n_certs));

            SecIdentityRef ident = (SecIdentityRef)CFArrayGetValueAtIndex(orig_peer_cert_array, 0);
            SecCertificateRef peer_cert = NULL;
            require_noerr(SecIdentityCopyCertificate(ident, &peer_cert), out);
            CFArraySetValueAtIndex(orig_peer_cert_array, 0, peer_cert);
            CFRelease(peer_cert);

#if 0
            require(CFEqual(orig_peer_cert_array, peer_cert_array), out);
#endif

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
            got_client_cert_req = true;

            /* set client cert */
            require_string(!ssl->is_server, out, "errSSLClientCertRequested while running server");
            require_string(!ssl->dh_anonymous, out, "errSSLClientCertRequested while running anon DH");
/*
            CFArrayRef DNs = NULL;
            require_noerr(SSLCopyDistinguishedNames	(ctx, &DNs), out);
            require(DNs, out);
            CFRelease(DNs);
*/
            require_string(ssl->client_side_auth, out, "errSSLClientCertRequested in run not testing that");
            require_noerr(SSLSetCertificate(ctx, ssl->certs), out);
        }
    } while (ortn == errSSLWouldBlock
             || ortn == errSSLServerAuthCompleted
             || ortn == errSSLClientCertRequested);
    require_noerr_action_quiet(ortn, out,
                               fprintf(stderr, "Fell out of SSLHandshake with error: %d\n", (int)ortn));

    if (!ssl->is_server && !ssl->dh_anonymous && !ssl->is_session_resume) {
        require_action_string(got_server_auth, out, ortn=-1, "never got server auth.");
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

#if 1
    char *req="GET / HTTP/1.0\r\n\r\n";
    char ibuf[4096];
    size_t len;
    if (!ssl->is_server) {
        require_noerr_quiet(ortn = SSLWrite(ctx, req, strlen(req), &len), out);
        require_action_quiet(len == strlen(req), out, ortn = -1);
        require_noerr_quiet(ortn = SSLRead(ctx, ibuf, sizeof(ibuf), &len), out);
        ibuf[len]=0;
//        printf(">>>\n%s<<<\n", ibuf);
    }
#endif

out:
    CFReleaseSafe(orig_peer_cert_array);
    CFReleaseSafe(peer_cert_array);
    SSLClose(ctx);
    SSLDisposeContext(ctx);
    if (trust) CFRelease(trust);

    return ortn;
}



static ssl_test_handle *
ssl_test_handle_create(uint32_t session_id, bool resume, bool server, bool client_side_auth, bool dh_anonymous, bool dtls,
                       int comm, CFArrayRef certs, SSLProtocol prot)
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
        handle->st = make_ssl_ref(server, client_side_auth, dh_anonymous, dtls, comm, certs, prot);
    }
    return handle;
}

static void
tests(void)
{
    CFArrayRef client_certs = trusted_ec_client_chain();
    ok(client_certs, "got client certs");

    int i;
    int p, pr;

    for (p=0; p<nservers; p++) {
    for (pr=0; pr<nprotos; pr++) {
        for (i=0; ciphers[i].name != NULL; i++) {

            ssl_test_handle *client;
            SSLProtocol proto = protos[pr];
            int port;

            int s;

            SKIP: {
                skip("This ciphersuite is not supported for this protocol version", 2, ciphers[i].prot<=pr);
                skip("This server doesn't support this ciphersuite", 2, ciphers[i].portoffset[servers[p].cs_index]);

                port=servers[p].base_port + ciphers[i].portoffset[servers[p].cs_index];
                uint32_t session_id = (pr<<16) | (port<<8) | (i+1);

                s=SocketConnect(servers[p].host, port);

                ok(s,
                   "Connect failed: %40s to %s:%d proto=%d", ciphers[i].name, servers[p].host, port, pr);

                skip("Could not connect to the server", 1, s);

                //fprintf(stderr, "session_id: %d\n", session_id);
                client = ssl_test_handle_create(session_id, false, false/*client*/,
                                                servers[p].client_auth, ciphers[i].dh_anonymous, 0,
                                                s, client_certs, proto);

                /* set fixed cipher on client and server */
                if(ciphers[i].cipher != SSL_NO_SUCH_CIPHERSUITE) {
                    if(SSLSetEnabledCiphers(client->st, &ciphers[i].cipher, 1)!=0)
                        printf("Invalid cipher %04x (i=%d, p=%d, pr=%d)\n", ciphers[i].cipher, i, p, pr);
                }

                printf("Handshake : %40s to %s:%d proto=%d\n", ciphers[i].name, servers[p].host, port, pr);
                OSStatus ok = securetransport(client);
                printf("Result = %d\n", (int)ok);

                ok(!ok, "Handshake failed: %40s to %s:%d proto=%d", ciphers[i].name, servers[p].host, port, pr);

                close(s);
                free(client);

            } /* SKIP block */
        }
    } /* all ciphers */
    } /* all servers */

    CFReleaseNull(client_certs);
}

int ssl_43_ciphers(int argc, char *const *argv)
{
        plan_tests(1 + 2*nservers*nprotos*(ciphers_len-1));

        tests();

        return 0;
}

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
