
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

#include <utilities/array_size.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdlib.h>
#include <mach/mach_time.h>

#include <tls_ciphersuites.h>

#if TARGET_OS_IPHONE
#include <Security/SecRSAKey.h>
#endif

#include "ssl_regressions.h"
#include "ssl-utils.h"

/*
    SSL CipherSuite tests
*/

static const SSLCipherSuite SupportedCipherSuites[] = {

    TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
    TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
    TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384,
    TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256,
    TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,
    TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
    TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA,

    TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
    TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384,
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256,
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA,
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,
    TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA,

    TLS_DHE_RSA_WITH_AES_256_GCM_SHA384,
    TLS_DHE_RSA_WITH_AES_128_GCM_SHA256,
    TLS_DHE_RSA_WITH_AES_256_CBC_SHA256,
    TLS_DHE_RSA_WITH_AES_128_CBC_SHA256,
    TLS_DHE_RSA_WITH_AES_256_CBC_SHA,
    TLS_DHE_RSA_WITH_AES_128_CBC_SHA,
    SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA,

    TLS_RSA_WITH_AES_256_GCM_SHA384,
    TLS_RSA_WITH_AES_128_GCM_SHA256,
    TLS_RSA_WITH_AES_256_CBC_SHA256,
    TLS_RSA_WITH_AES_128_CBC_SHA256,
    TLS_RSA_WITH_AES_256_CBC_SHA,
    TLS_RSA_WITH_AES_128_CBC_SHA,
    SSL_RSA_WITH_3DES_EDE_CBC_SHA,

    /* RC4 */
    TLS_ECDHE_ECDSA_WITH_RC4_128_SHA,
    TLS_ECDHE_RSA_WITH_RC4_128_SHA,
    SSL_RSA_WITH_RC4_128_SHA,
    SSL_RSA_WITH_RC4_128_MD5,

    /* Unsafe ciphersuites */

    TLS_DH_anon_WITH_AES_256_GCM_SHA384,
    TLS_DH_anon_WITH_AES_128_GCM_SHA256,
    TLS_DH_anon_WITH_AES_128_CBC_SHA256,
    TLS_DH_anon_WITH_AES_256_CBC_SHA256,
    TLS_DH_anon_WITH_AES_128_CBC_SHA,
    TLS_DH_anon_WITH_AES_256_CBC_SHA,
    SSL_DH_anon_WITH_RC4_128_MD5,
    SSL_DH_anon_WITH_3DES_EDE_CBC_SHA,

    TLS_ECDH_anon_WITH_NULL_SHA,
    TLS_ECDH_anon_WITH_RC4_128_SHA,
    TLS_ECDH_anon_WITH_3DES_EDE_CBC_SHA,
    TLS_ECDH_anon_WITH_AES_128_CBC_SHA,
    TLS_ECDH_anon_WITH_AES_256_CBC_SHA,

    TLS_ECDHE_ECDSA_WITH_NULL_SHA,
    TLS_ECDHE_RSA_WITH_NULL_SHA,

    TLS_PSK_WITH_AES_256_CBC_SHA384,
    TLS_PSK_WITH_AES_128_CBC_SHA256,
    TLS_PSK_WITH_AES_256_CBC_SHA,
    TLS_PSK_WITH_AES_128_CBC_SHA,
    TLS_PSK_WITH_RC4_128_SHA,
    TLS_PSK_WITH_3DES_EDE_CBC_SHA,
    TLS_PSK_WITH_NULL_SHA384,
    TLS_PSK_WITH_NULL_SHA256,
    TLS_PSK_WITH_NULL_SHA,

    TLS_RSA_WITH_NULL_SHA256,
    SSL_RSA_WITH_NULL_SHA,
    SSL_RSA_WITH_NULL_MD5

};

static const unsigned SupportedCipherSuitesCount = sizeof(SupportedCipherSuites)/sizeof(SupportedCipherSuites[0]);


static int protos[]={kTLSProtocol1, kTLSProtocol11, kTLSProtocol12, kDTLSProtocol1 };
static int nprotos = sizeof(protos)/sizeof(protos[0]);


static unsigned char dh_param_1024_bytes[] = {
    0x30, 0x81, 0x87, 0x02, 0x81, 0x81, 0x00, 0xf2, 0x56, 0xb9, 0x41, 0x74,
    0x8c, 0x54, 0x22, 0xad, 0x94, 0x2b, 0xed, 0x83, 0xb9, 0xa0, 0x2f, 0x40,
    0xce, 0xf8, 0xec, 0x96, 0xed, 0xcd, 0x8e, 0xfc, 0xf8, 0xdd, 0x06, 0x15,
    0xbc, 0x68, 0x0d, 0x0e, 0x2c, 0xef, 0x00, 0x71, 0x28, 0x3d, 0x27, 0x6d,
    0x5e, 0x42, 0x8c, 0xbd, 0x0f, 0x07, 0x23, 0x9d, 0x07, 0x8e, 0x52, 0x47,
    0xa2, 0x5d, 0xf8, 0xd9, 0x9a, 0x7b, 0xb4, 0xab, 0xd2, 0xa3, 0x39, 0xe9,
    0x2c, 0x3b, 0x9b, 0xaa, 0xbe, 0x4e, 0x01, 0x36, 0x16, 0xc2, 0x9e, 0x7b,
    0x38, 0x78, 0x82, 0xd0, 0xed, 0x8e, 0x1e, 0xce, 0xa6, 0x23, 0x95, 0xae,
    0x31, 0x66, 0x58, 0x60, 0x44, 0xdf, 0x1f, 0x9c, 0x68, 0xbf, 0x8b, 0xf1,
    0xb4, 0xa8, 0xe7, 0xb2, 0x43, 0x8b, 0xa9, 0x3d, 0xa1, 0xb7, 0x1a, 0x11,
    0xcf, 0xf4, 0x5e, 0xf7, 0x08, 0xf6, 0x84, 0x1c, 0xd7, 0xfa, 0x40, 0x10,
    0xdc, 0x64, 0x83, 0x02, 0x01, 0x02
};
static unsigned char *dh_param_der = dh_param_1024_bytes;
static unsigned int dh_param_der_len = sizeof(dh_param_1024_bytes);


typedef struct {
    uint32_t session_id;
    bool is_session_resume;
    SSLContextRef st;
    bool is_server;
    bool is_dtls;
    SSLAuthenticate client_side_auth;
    bool dh_anonymous;
    int comm;
    CFArrayRef certs;
    CFArrayRef peer_certs;
    SSLProtocol proto;
    uint64_t time; // output
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
        } while ((ret < 0) && (errno == EINPROGRESS || errno == EAGAIN || errno == EINTR));
        if (ret > 0) {
            len -= ret;
            ptr += ret;
        } else {
            printf("read error(%d): ret=%zd, errno=%d\n", (int)conn, ret, errno);
            return -errno;
        }
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

static SSLContextRef make_ssl_ref(bool server, SSLAuthenticate client_side_auth, bool dh_anonymous,
    bool dtls, int sock, CFArrayRef certs, SSLProtocol proto)
{
    SSLContextRef ctx = SSLCreateContext(kCFAllocatorDefault, server?kSSLServerSide:kSSLClientSide, dtls?kSSLDatagramType:kSSLStreamType);
    require(ctx, out);

    if(dtls) {
        size_t mtu;
        require_noerr(SSLSetMaxDatagramRecordSize(ctx, 400), out);
        require_noerr(SSLGetMaxDatagramRecordSize(ctx, &mtu), out);
    }
    require_noerr(SSLSetProtocolVersionMax(ctx, proto), out);

    require_noerr(SSLSetIOFuncs(ctx,
        (SSLReadFunc)SocketRead, (SSLWriteFunc)SocketWrite), out);
    require_noerr(SSLSetConnection(ctx, (SSLConnectionRef)(intptr_t)sock), out);
    static const char *peer_domain_name = "localhost";
    require_noerr(SSLSetPeerDomainName(ctx, peer_domain_name,
        strlen(peer_domain_name)), out);

    require_noerr(SSLSetMinimumDHGroupSize(ctx, 512), out);

    if (!dh_anonymous) {
        if (server)
            require_noerr(SSLSetCertificate(ctx, certs), out);
        if ((client_side_auth != kNeverAuthenticate) && server) {
            SSLAuthenticate auth;
            require_noerr(SSLSetClientSideAuthenticate(ctx, client_side_auth), out);
            require_noerr(SSLGetClientSideAuthenticate(ctx, &auth), out);
            require(auth==client_side_auth, out);
            require_noerr(SSLAddDistinguishedName(ctx, dn, dn_len), out);
        }
#if 0 /* Setting client certificate in advance */
        if ((client_side_auth == kAlwaysAuthenticate) && !server)
            require_noerr(SSLSetCertificate(ctx, certs), out);
#endif
        if ((client_side_auth != kNeverAuthenticate) && !server) /* enable break from SSLHandshake */
            require_noerr(SSLSetSessionOption(ctx,
                kSSLSessionOptionBreakOnCertRequested, true), out);
    }

    /* Set this option, even if doing anonDH or PSK - it should NOT break out in those case */
    require_noerr(SSLSetSessionOption(ctx, kSSLSessionOptionBreakOnServerAuth, true), out);

    /* Tell SecureTransport to not check certs itself: it will break out of the
       handshake to let us take care of it instead. */
    require_noerr(SSLSetEnableCertVerify(ctx, false), out);

    if (server) {
        require_noerr(SSLSetDiffieHellmanParams(ctx,
            dh_param_der, dh_param_der_len), out);
    }
    else /* if client */ {
    }

    return ctx;
out:
    if (ctx)
        CFRelease(ctx);
    return NULL;
}

static bool check_peer_cert(SSLContextRef ctx, const ssl_test_handle *ssl, SecTrustRef *trust)
{
    CFMutableArrayRef peer_cert_array = NULL;
    CFMutableArrayRef orig_peer_cert_array = NULL;

    /* verify peer cert chain */
    require_noerr(SSLCopyPeerTrust(ctx, trust), out);
    SecTrustResultType trust_result = 0;
    /* this won't verify without setting up a trusted anchor */
    require_noerr(SecTrustEvaluate(*trust, &trust_result), out);

    CFIndex n_certs = SecTrustGetCertificateCount(*trust);
    /* fprintf(stderr, "%ld certs; trust_eval: %d\n", n_certs, trust_result); */

    peer_cert_array = CFArrayCreateMutable(NULL, n_certs, &kCFTypeArrayCallBacks);
    orig_peer_cert_array = CFArrayCreateMutableCopy(NULL, n_certs, ssl->peer_certs);
    while (n_certs--)
        CFArrayInsertValueAtIndex(peer_cert_array, 0,
                                  SecTrustGetCertificateAtIndex(*trust, n_certs));

    SecIdentityRef ident =
    (SecIdentityRef)CFArrayGetValueAtIndex(orig_peer_cert_array, 0);
    SecCertificateRef peer_cert = NULL;
    require_noerr(SecIdentityCopyCertificate(ident, &peer_cert), out);
    CFArraySetValueAtIndex(orig_peer_cert_array, 0, peer_cert);
    CFRelease(peer_cert);

    require(CFEqual(orig_peer_cert_array, peer_cert_array), out);
    CFReleaseNull(orig_peer_cert_array);
    CFReleaseNull(peer_cert_array);

    /*
     CFStringRef cert_name = SecCertificateCopySubjectSummary(cert);
     char cert_name_buffer[1024];
     require(CFStringGetFileSystemRepresentation(cert_name,
     cert_name_buffer, sizeof(cert_name_buffer)), out);
     fprintf(stderr, "cert name: %s\n", cert_name_buffer);
     CFRelease(trust);
     */
    return true;
out:
    CFReleaseNull(orig_peer_cert_array);
    CFReleaseNull(peer_cert_array);
    return false;
}


#include <mach/mach_time.h>

#define perf_start() uint64_t _perf_time = mach_absolute_time();
#define perf_scale_factor() ({struct mach_timebase_info info; mach_timebase_info(&info); ((double)info.numer) / (1000000.0 * info.denom);})
#define perf_time() ((mach_absolute_time() - _perf_time) * perf_scale_factor())


static void *securetransport_ssl_thread(void *arg)
{
    OSStatus ortn;
    ssl_test_handle * ssl = (ssl_test_handle *)arg;
    SSLContextRef ctx = ssl->st;
    SecTrustRef trust = NULL;
    bool got_server_auth = false, got_client_cert_req = false;
    SSLSessionState ssl_state;

    perf_start();

    pthread_setname_np(ssl->is_server?"server thread":"client thread");

    require_noerr(ortn=SSLGetSessionState(ctx,&ssl_state), out);
    require_action(ssl_state==kSSLIdle, out, ortn = -1);

    //uint64_t start = mach_absolute_time();
    do {
        ortn = SSLHandshake(ctx);
        require_noerr(SSLGetSessionState(ctx,&ssl_state), out);

        if (ortn == errSSLPeerAuthCompleted)
        {
            require_action(ssl_state==kSSLHandshake, out, ortn = -1);
            require_string(!got_server_auth, out, "second server auth");
            require_string(!ssl->dh_anonymous, out, "server auth with anon cipher");
            // Note: Previously, the implementation always returned errSSLPeerAuthCompleted before
            // errSSLClientCertRequested. Due to OCSP stappling implementation, this is no longer guaranteed.
            // This behavior change should not be an issue, but it's possible that some applications will
            // have issue with this new behavior. If we do find out that this is causing an issue, then
            // the following require statement should be re-enabled, and the implementation changed
            // to implement the former behavior.
            //require_string(!got_client_cert_req, out, "got client cert req before server auth");
            got_server_auth = true;
            require_string(!trust, out, "Got errSSLServerAuthCompleted twice?");
            require_string(check_peer_cert(ctx, ssl, &trust), out, "Certificate check failed");
        } else if (ortn == errSSLClientCertRequested) {
            require_action(ssl_state==kSSLHandshake, out, ortn = -1);
            require_string(!got_client_cert_req, out, "second client cert req");
            // Note: see Note above.
            //require_string(got_server_auth, out, "didn't get server auth first");
            got_client_cert_req = true;

            /* set client cert */
            require_string(!ssl->is_server, out, "errSSLClientCertRequested while running server");
            require_string(!ssl->dh_anonymous, out, "errSSLClientCertRequested while running anon DH");

            CFArrayRef DNs = NULL;
            require_noerr(SSLCopyDistinguishedNames	(ctx, &DNs), out);
            require(DNs, out);
            CFRelease(DNs);

            require_string(ssl->client_side_auth != kNeverAuthenticate, out, "errSSLClientCertRequested in run not testing that");
            if(ssl->client_side_auth == kAlwaysAuthenticate) { // Only set a client cert in mode 1.
                require_noerr(SSLSetCertificate(ctx, ssl->certs), out);
            }
        } else if (ortn == errSSLWouldBlock) {
            require_action(ssl_state==kSSLHandshake, out, ortn = -1);
        }
    } while (ortn == errSSLWouldBlock
        || ortn == errSSLServerAuthCompleted
        || ortn == errSSLClientCertRequested);
    require_noerr_action_quiet(ortn, out,
        fprintf(stderr, "Fell out of SSLHandshake with error: %d (%s)\n", (int)ortn, ssl->is_server?"server":"client"));

    require_action(ssl_state==kSSLConnected, out, ortn = -1);

    if (!ssl->is_server && !ssl->dh_anonymous && !ssl->is_session_resume) {
        require_string(got_server_auth, out, "never got server auth");
        if (ssl->client_side_auth != kNeverAuthenticate)
            require_string(got_client_cert_req, out, "never got client cert req");
    }

    if (!ssl->is_server && !ssl->dh_anonymous && ssl->is_session_resume) {
        require_string(!got_server_auth, out, "got server auth during resumption??");
        require_string(check_peer_cert(ctx, ssl, &trust), out, "Certificate check failed (resumption case)");
    }
    //uint64_t elapsed = mach_absolute_time() - start;
    //fprintf(stderr, "setr elapsed: %lld\n", elapsed);

    /*
    SSLProtocol proto = kSSLProtocolUnknown;
    require_noerr_quiet(SSLGetNegotiatedProtocolVersion(ctx, &proto), out); */

    SSLCipherSuite cipherSuite;
    require_noerr_quiet(ortn = SSLGetNegotiatedCipher(ctx, &cipherSuite), out);
    //fprintf(stderr, "st negotiated %s\n", sslcipher_itoa(cipherSuite));

    if(ssl->is_dtls) {
        size_t sz;
        SSLGetDatagramWriteSize(ctx, &sz);
        //fprintf(stderr, "Max Write Size = %ld\n", sz);
    }

	Boolean	sessionWasResumed = false;
    uint8_t session_id_data[MAX_SESSION_ID_LENGTH];
    size_t session_id_length = sizeof(session_id_data);
    require_noerr_quiet(ortn = SSLGetResumableSessionInfo(ctx, &sessionWasResumed, session_id_data, &session_id_length), out);
    require_action(ssl->dh_anonymous || (ssl->is_session_resume == sessionWasResumed), out, ortn = -1);
    // if (sessionWasResumed) fprintf(stderr, "st resumed session\n");
    //hexdump(session_id_data, session_id_length);

#define BUFSIZE (8*1024)
    unsigned char ibuf[BUFSIZE], obuf[BUFSIZE];

    for(int i=0; i<10; i++) {
        size_t len;
        if (ssl->is_server) {
            memset(obuf, i, BUFSIZE);
            // SecRandomCopyBytes(kSecRandomDefault, sizeof(obuf), obuf);
            require_noerr(ortn = SSLWrite(ctx, obuf, BUFSIZE, &len), out);
            require_action(len == BUFSIZE, out, ortn = -1);

            require_noerr(ortn = SSLWrite(ctx, obuf, 0, &len), out);
            require_action(len == 0, out, ortn = -1);
        }

        len=0;
        while(len<BUFSIZE) {
            size_t l=len;
            ortn = SSLRead(ctx, ibuf+len, BUFSIZE-len, &l);
            len+=l;
            //printf("SSLRead [%p] %d, l=%zd len=%zd\n", ctx, (int)ortn, l, len);
        }

        //printf("SSLRead [%p] done\n", ctx);

        require_noerr(ortn, out);
        require_action(len == BUFSIZE, out, ortn = -1);

        if (ssl->is_server) {
            require_noerr(memcmp(ibuf, obuf, BUFSIZE), out);
        } else {
            require_noerr(ortn = SSLWrite(ctx, ibuf, BUFSIZE, &len), out);
            require_action(len == BUFSIZE, out, ortn = -1);
        }
    }

out:
    SSLClose(ctx);
    CFRelease(ctx);
    if (trust) CFRelease(trust);
    close(ssl->comm);

    ssl->time = perf_time();

    pthread_exit((void *)(intptr_t)ortn);
    return NULL;
}



static ssl_test_handle *
ssl_test_handle_create(uint32_t session_id, bool resume, bool server, SSLAuthenticate client_side_auth, bool dh_anonymous, bool dtls,
    int comm, CFArrayRef certs, CFArrayRef peer_certs, SSLProtocol proto)
{
    ssl_test_handle *handle = calloc(1, sizeof(ssl_test_handle));
    if (handle) {
        handle->session_id = session_id;
        handle->is_session_resume = resume;
        handle->is_server = server;
        handle->is_dtls = dtls;
        handle->client_side_auth = client_side_auth;
        handle->dh_anonymous = dh_anonymous;
        handle->comm = comm;
        handle->certs = certs;
        handle->peer_certs = peer_certs;
        handle->proto = proto;
        handle->st = make_ssl_ref(server, client_side_auth, dh_anonymous, dtls, comm, certs, proto);
    }
    return handle;
}

static void
tests(void)
{
    pthread_t client_thread, server_thread;

    CFArrayRef server_rsa_certs = server_chain();
    CFArrayRef server_ec_certs = server_ec_chain();
    CFArrayRef client_certs = trusted_client_chain();
    ok(server_rsa_certs, "got rsa server cert chain");
    ok(server_ec_certs, "got ec server cert chain");
    ok(client_certs, "got rsa client cert chain");

/* Enable this if you want to test a specific d/i/k/l/m/p combination */
#if 0
    int i=0, l=0, k=0, p=0; { {
#else
    int i,k,l, p;

    for (p=0; p<nprotos; p++)
    for (k=0; k<3; k++) /* client side auth mode:
                                0 (kSSLNeverAuthenticate): server doesn't request ,
                                1 (kSSLAlwaysAuthenticate): server request, client provide,
                                2 (kSSLTryAuthenticate): server request, client does not provide */
    {

        for (i=0; i<SupportedCipherSuitesCount; i++)
        for (l = 0; l<2; l++) { /* resumption or not */
#endif
            uint16_t cs = (uint16_t)(SupportedCipherSuites[i]);
            KeyExchangeMethod kem = sslCipherSuiteGetKeyExchangeMethod(cs);
            SSL_CipherAlgorithm cipher = sslCipherSuiteGetSymmetricCipherAlgorithm(cs);
            tls_protocol_version min_version = sslCipherSuiteGetMinSupportedTLSVersion(cs);

            CFArrayRef server_certs;

            if(kem == SSL_ECDHE_ECDSA) {
                server_certs = server_ec_certs;
            } else {
                server_certs = server_rsa_certs;
            }


            SKIP:{
                bool dtls = (protos[p] == kDTLSProtocol1);
                bool server_ok = ((kem != SSL_ECDH_ECDSA) && (kem != SSL_ECDH_RSA) && (kem != SSL_ECDH_anon));
                bool dh_anonymous = ((kem == SSL_DH_anon) || (kem == TLS_PSK));
                bool version_ok;

                switch(protos[p]) {
                    case kDTLSProtocol1:
                        version_ok = cipher != SSL_CipherAlgorithmRC4_128 && (min_version != tls_protocol_version_TLS_1_2);
                        break;
                    case kSSLProtocol3:
                        version_ok = (min_version == tls_protocol_version_SSL_3);
                        break;
                    case kTLSProtocol1:
                    case kTLSProtocol11:
                        version_ok = (min_version != tls_protocol_version_TLS_1_2);
                        break;
                    case kTLSProtocol12:
                        version_ok = true;
                        break;
                    default:
                        version_ok = false;

                }

                skip("This ciphersuite is not supported by Server", 1, server_ok);
                skip("This ciphersuite is not supported for this protocol version", 1, version_ok);

                int sp[2];
                if (socketpair(AF_UNIX, SOCK_STREAM, 0, sp)) exit(errno);
                fcntl(sp[0], F_SETNOSIGPIPE, 1);
                fcntl(sp[1], F_SETNOSIGPIPE, 1);

                ssl_test_handle *server, *client;
                size_t num_supported_ciphers = 0;
                SSLCipherSuite *supported_ciphers = NULL;

                SSLAuthenticate client_side_auth = k;

                uint32_t session_id = (p<<24) | (k<<16) | (i+1);
                //fprintf(stderr, "session_id: %d\n", session_id);
                server = ssl_test_handle_create(session_id, (l == 1), true /*server*/,
                                                client_side_auth, dh_anonymous, dtls,
                                                sp[0], server_certs, client_certs, protos[p]);
                client = ssl_test_handle_create(session_id, (l == 1), false /*client*/,
                                                client_side_auth, dh_anonymous, dtls,
                                                sp[1], client_certs, server_certs, protos[p]);

                require_noerr(SSLSetPeerID(server->st, &session_id, sizeof(session_id)), out);
                require_noerr(SSLSetPeerID(client->st, &session_id, sizeof(session_id)), out);

                /* set single cipher on client, default ciphers on server */
                num_supported_ciphers = 0;
                require_noerr(SSLSetEnabledCiphers(client->st, &(SupportedCipherSuites[i]), 1), out);
                require_noerr(SSLGetNumberSupportedCiphers(server->st, &num_supported_ciphers), out);
                require(supported_ciphers=malloc(num_supported_ciphers*sizeof(SSLCipherSuite)), out);
                require_noerr(SSLGetSupportedCiphers(server->st, supported_ciphers, &num_supported_ciphers), out);
                require_noerr(SSLSetEnabledCiphers(server->st, supported_ciphers, num_supported_ciphers), out);

                require_noerr(SSLSetPSKSharedSecret(client->st, "123456789", 9), out);
                require_noerr(SSLSetPSKSharedSecret(server->st, "123456789", 9), out);

                pthread_create(&client_thread, NULL, securetransport_ssl_thread, client);
                pthread_create(&server_thread, NULL, securetransport_ssl_thread, server);

                int server_err, client_err;
                pthread_join(client_thread, (void*)&client_err);
                pthread_join(server_thread, (void*)&server_err);

#if 0
                // If you want to print an approximate time for each handshake.
                printf("%4llu - %40s CSA:%d RESUME:%d PROTO:0x%04x\n",
                        client->time,
                        ciphersuite_name(SupportedCipherSuites[i]),
                        server->client_side_auth,
                        l, protos[p]);
#endif

                ok(!server_err && !client_err,
                   "%40s CSA:%d RESUME:%d PROTO:0x%04x",
                   ciphersuite_name(SupportedCipherSuites[i]),
                   server->client_side_auth,
                   l, protos[p]);
out:
                free(client);
                free(server);
                free(supported_ciphers);
            }
        } /* all ciphers */
    } /* all configs */


    CFReleaseSafe(server_ec_certs);
    CFReleaseSafe(server_rsa_certs);
    CFReleaseSafe(client_certs);

}

int ssl_42_ciphers(int argc, char *const *argv)
{

    plan_tests(3 * 2 * nprotos * SupportedCipherSuitesCount /* client auth 0/1/2 * #resumptions * #protos * #ciphers */
                + 3 /*cert*/);

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
