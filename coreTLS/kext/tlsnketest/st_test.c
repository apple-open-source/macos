//
//  st_test.c
//  tlsnke
//
//  Created by Fabrice Gautier on 1/13/12.
//  Copyright (c) 2012 Apple, Inc. All rights reserved.
//


#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <Security/SecureTransportPriv.h> /* SSLSetOption */
#include <Security/SecRandom.h>

#include <AssertMacros.h>

#include "ssl-utils.h"

#if 0
#include <Security/SecPolicy.h>
#include <Security/SecTrust.h>
#include <Security/SecIdentity.h>
#include <Security/SecIdentityPriv.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecKeyPriv.h>
#if TARGET_OS_IPHONE
#include <Security/SecRSAKey.h>
#endif
#include <Security/SecItem.h>
#include <Security/SecRandom.h>
#endif

#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <mach/mach_time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "tlssocket.h"

/*
 SSL CipherSuite tests

 Below are all the ciphers that are individually tested.  The first element
 is the SecureTransport/RFC name; the second is what openssl calls it, which
 can be looked up in ciphers(1).

 All SSL_DH_* and TLS_DH_* are disabled because neither openssl nor
 securetranport support them:
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

 Export ciphersuites disabled on iOS 5.0:
 SSL_RSA_EXPORT_WITH_RC4_40_MD5, SSL_RSA_EXPORT_WITH_DES40_CBC_SHA,
 SSL_RSA_WITH_DES_CBC_SHA, SSL_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA,
 SSL_DHE_RSA_WITH_DES_CBC_SHA, SSL_DH_anon_EXPORT_WITH_RC4_40_MD5,
 SSL_DH_anon_EXPORT_WITH_DES40_CBC_SHA, SSL_DH_anon_WITH_DES_CBC_SHA

 */

typedef struct _CipherSuiteName {
    SSLCipherSuite cipher;
    const char *name;
    bool dh_anonymous;
} CipherSuiteName;

#define CIPHER(cipher, dh_anonymous) { cipher, #cipher, dh_anonymous },

static const CipherSuiteName ciphers[] = {
    //SSL_NULL_WITH_NULL_NULL, unsupported
    CIPHER(SSL_RSA_WITH_NULL_SHA, false)
    CIPHER(SSL_RSA_WITH_NULL_MD5, false)
    CIPHER(TLS_RSA_WITH_NULL_SHA256, false)

    //    CIPHER(SSL_RSA_WITH_RC4_128_MD5, false)
    //CIPHER(SSL_RSA_WITH_RC4_128_SHA, false)
    CIPHER(SSL_RSA_WITH_3DES_EDE_CBC_SHA, false)

    CIPHER(SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA, false)
    //CIPHER(SSL_DH_anon_WITH_RC4_128_MD5, true)
    CIPHER(SSL_DH_anon_WITH_3DES_EDE_CBC_SHA, true)
    CIPHER(TLS_DHE_RSA_WITH_AES_128_CBC_SHA, false)
    CIPHER(TLS_DH_anon_WITH_AES_128_CBC_SHA, true)
    CIPHER(TLS_DHE_RSA_WITH_AES_256_CBC_SHA, false)
    CIPHER(TLS_DH_anon_WITH_AES_256_CBC_SHA, true)

    CIPHER(TLS_RSA_WITH_AES_128_CBC_SHA, false)
    CIPHER(TLS_RSA_WITH_AES_256_CBC_SHA, false)


#if 0
    CIPHER(TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA, false)
    CIPHER(TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA, false)

    CIPHER(TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA, false)
    CIPHER(TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA, false)

    CIPHER(TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA, false)
    CIPHER(TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA, false)

    CIPHER(TLS_ECDH_anon_WITH_AES_128_CBC_SHA, true)
    CIPHER(TLS_ECDH_anon_WITH_AES_256_CBC_SHA, true)

    CIPHER(TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256, false)
    CIPHER(TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384, false)
    CIPHER(TLS_ECDH_RSA_WITH_AES_128_CBC_SHA256, false)
    CIPHER(TLS_ECDH_RSA_WITH_AES_256_CBC_SHA384, false)
#endif

#if 0
    CIPHER(TLS_RSA_WITH_AES_256_GCM_SHA384, false)
    CIPHER(TLS_RSA_WITH_AES_128_GCM_SHA256, false)
#endif

    /* Export ciphers are disabled */
#if 0
    CIPHER(SSL_RSA_EXPORT_WITH_RC4_40_MD5, false)
    CIPHER(SSL_RSA_EXPORT_WITH_DES40_CBC_SHA, false)
    CIPHER(SSL_RSA_WITH_DES_CBC_SHA,  false)
    CIPHER(SSL_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA,  false)
    CIPHER(SSL_DHE_RSA_WITH_DES_CBC_SHA, false)
    CIPHER(SSL_DH_anon_EXPORT_WITH_RC4_40_MD5, true)
    CIPHER(SSL_DH_anon_EXPORT_WITH_DES40_CBC_SHA,  true)
    CIPHER(SSL_DH_anon_WITH_DES_CBC_SHA, true)
#endif

    { -1 }
};

static int protos[]={kTLSProtocol1, kTLSProtocol11, kTLSProtocol12 };

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


typedef struct {
    uint32_t session_id;
    bool is_session_resume;
    SSLContextRef st;
    bool is_server;
    bool is_dtls;
    bool client_side_auth;
    bool dh_anonymous;
    int comm;
    CFArrayRef certs;
    SSLProtocol proto;
} ssl_test_handle;


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


/* 2K should be enough for everybody */
#define MTU 8000
static unsigned char readBuffer[MTU];
static unsigned int  readOff=0;
static size_t        readLeft=0;

static
OSStatus SocketRead(
                    SSLConnectionRef 	connection,
                    void 				*data,
                    size_t 				*dataLength)
{
    int fd = (int)connection;
    ssize_t len;

    if(readLeft==0)
    {
        // printf("SocketRead(%d): waiting for data %ld\n", fd, *dataLength);

        len = read(fd, readBuffer, MTU);

        if(len>0) {
            readOff=0;
            readLeft=(size_t) len;
            //printf("SocketRead(%d): %ld bytes... epoch: %02x seq=%02x%02x\n",
            //       fd, len, d[4], d[9], d[10]);

        } else {
            int theErr = errno;
            switch(theErr) {
                case EAGAIN:
                    printf("SocketRead(%d): WouldBlock\n", fd);
                    *dataLength=0;
                    /* nonblocking, no data */
                    return errSSLWouldBlock;
                default:
                    perror("SocketRead");
                    return -36;
            }
        }
    }

    if(readLeft<*dataLength) {
        *dataLength=readLeft;
    }


    memcpy(data, readBuffer+readOff, *dataLength);
    readLeft-=*dataLength;
    readOff+=*dataLength;

    // printf("%s: returning %ld bytes, left %ld\n", __FUNCTION__, *dataLength, readLeft);

    return errSecSuccess;

}

static
OSStatus SocketWrite(
                     SSLConnectionRef   connection,
                     const void         *data,
                     size_t 			*dataLength)	/* IN/OUT */
{
    int fd = (int)connection;
    ssize_t len;
    OSStatus err = errSecSuccess;

#if 0
    const uint8_t *d=data;

    if((rand()&3)==1) {

        /* drop 1/8th packets */
        printf("SocketWrite: Drop %ld bytes... epoch: %02x seq=%02x%02x\n",
               *dataLength, d[4], d[9], d[10]);
        return errSecSuccess;

    }
#endif

    // printf("SocketWrite(%d): Sending %ld bytes... epoch: %02x seq=%02x%02x\n",
    //        fd, *dataLength, d[4], d[9], d[10]);

    len = send(fd, data, *dataLength, 0);
    if(len>0) {
        *dataLength=(size_t)len;
        return err;
    }

    int theErr = errno;
    switch(theErr) {
        case EAGAIN:
            /* nonblocking, no data */
            printf("SocketWrite(%d): WouldBlock\n", fd);
            err = errSSLWouldBlock;
            break;
        default:
            perror("SocketWrite");
            err = -36;
            break;
    }

    return err;

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
                                  bool dtls, int sock, CFArrayRef certs, SSLProtocol proto, bool kernel)
{
    SSLContextRef ctx;

    if(kernel) {
        require(dtls, out);
        ctx = SSLCreateContextWithRecordFuncs(kCFAllocatorDefault, server?kSSLServerSide:kSSLClientSide, dtls?kSSLDatagramType:kSSLStreamType, &TLSSocket_Funcs);
        require(ctx, out);
        printf("Attaching filter\n");
        require_noerr(TLSSocket_Attach(sock), out);
        require_noerr(SSLSetRecordContext(ctx, (intptr_t) sock), out);
    } else {
        ctx = SSLCreateContext(kCFAllocatorDefault, server?kSSLServerSide:kSSLClientSide, dtls?kSSLDatagramType:kSSLStreamType);
        require(ctx, out);
        require_noerr(SSLSetIOFuncs(ctx,
                                    (SSLReadFunc)SocketRead, (SSLWriteFunc)SocketWrite), out);
        require_noerr(SSLSetConnection(ctx, (intptr_t)sock), out);
    }
    require(ctx, out);

    if(dtls) {
        size_t mtu;
        require_noerr(SSLSetMaxDatagramRecordSize(ctx, 400), out);
        require_noerr(SSLGetMaxDatagramRecordSize(ctx, &mtu), out);
    } else {
        require_noerr(SSLSetProtocolVersionMax(ctx, proto), out);
        kernel = false; // not available for tls, only dtls currently.
    }

    static const char *peer_domain_name = "localhost";
    require_noerr(SSLSetPeerDomainName(ctx, peer_domain_name,
                                       strlen(peer_domain_name)), out);

    if (!dh_anonymous) {
        if (server)
            require_noerr(SSLSetCertificate(ctx, certs), out);
        if (client_side_auth && server) {
            SSLAuthenticate auth;
            require_noerr(SSLSetClientSideAuthenticate(ctx, kAlwaysAuthenticate), out);
            require_noerr(SSLGetClientSideAuthenticate(ctx, &auth), out);
            require(auth==kAlwaysAuthenticate, out);
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
        CFRelease(ctx);
    return NULL;
}

static void *securetransport_ssl_thread(void *arg)
{
    OSStatus ortn;
    ssl_test_handle * ssl = (ssl_test_handle *)arg;
    SSLContextRef ctx = ssl->st;
    SecTrustRef trust = NULL;
    bool got_server_auth = false, got_client_cert_req = false;


    if(ssl->is_server) {
        struct sockaddr_in ca; /* client address for connect */
        ssize_t l;
        int fd = ssl->comm;

        printf("Server waiting for first packet...\n");
        /* PEEK only... */
        socklen_t slen=sizeof(ca);
        char b;
        if((l=recvfrom(fd, &b, 1, MSG_PEEK, (struct sockaddr *)&ca, &slen))==-1)
        {
            perror("recvfrom");
            return NULL;
        }

        printf("Received packet from %s:%d (%ld), connecting...\n", inet_ntoa(ca.sin_addr), ca.sin_port, l);

        if(connect(fd, (struct sockaddr *)&ca, sizeof(ca))==-1)
        {
            perror("connect");
            return NULL;
        }
    }

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
                               fprintf(stderr, "Fell out of SSLHandshake with error: %ld\n", (long)ortn));

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

    if(ssl->is_dtls) {
        size_t sz;
        SSLGetDatagramWriteSize(ctx, &sz);
        // fprintf(stderr, "Max Write Size = %ld\n", sz);
    }

	Boolean	sessionWasResumed = false;
    uint8_t session_id_data[MAX_SESSION_ID_LENGTH];
    size_t session_id_length = sizeof(session_id_data);
    require_noerr_quiet(ortn = SSLGetResumableSessionInfo(ctx, &sessionWasResumed, session_id_data, &session_id_length), out);
    require_action(ssl->dh_anonymous || (ssl->is_session_resume == sessionWasResumed), out, ortn = -1);
    // if (sessionWasResumed) fprintf(stderr, "st resumed session\n");
    //hexdump(session_id_data, session_id_length);

    unsigned char ibuf[300], obuf[300];
    size_t len;
    if (ssl->is_server) {
        SecRandomCopyBytes(kSecRandomDefault, sizeof(obuf), obuf);
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
    CFRelease(ctx);
    if (trust) CFRelease(trust);
    close(ssl->comm);
    pthread_exit((void *)(intptr_t)ortn);
    return NULL;
}



static ssl_test_handle *
ssl_test_handle_create(uint32_t session_id, bool resume, bool server, bool client_side_auth, bool dh_anonymous, bool dtls,
                       int comm, CFArrayRef certs, SSLProtocol proto, bool kernel)
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
        handle->proto = proto;
        handle->st = make_ssl_ref(server, client_side_auth, dh_anonymous, dtls, comm, certs, proto, kernel);
    }
    return handle;
}



static void createsockets(int sp[2])
{


    int sock;
    struct sockaddr_in server_addr;
    struct hostent *host;
    int err;

    host = gethostbyname("localhost");

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(5000);
    server_addr.sin_addr = *((struct in_addr *)host->h_addr);
    bzero(&(server_addr.sin_zero),8);
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("server socket");
        exit(1);
    }

    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr))
        == -1) {
        perror("Unable to bind");
        exit(1);
    }

    printf("Server Waiting for client on port 5000\n");

    sp[0]=sock;

    printf("Create client socket\n");
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(sock<0) {
        perror("client socket");
        exit(1);
    }

    err = connect(sock, (struct sockaddr *)&server_addr,
                  sizeof(struct sockaddr));
    if(err)
    {
        perror("connect");
        exit(1);
    }

    sp[1]=sock;

    printf("Connected\n");


}

int st_test(void);
int st_test(void)
{
    pthread_t client_thread, server_thread;
    CFArrayRef server_certs = server_chain();
    check(server_certs);

    char msg[128];

    /* Enable this if you want to test a specific d/i/k/l/p/ combination */
#if 0
    int d=0, i=0, l=0, c=0, k=0; { {
#else
    int d,i,c,k,l,p;

        p=0;
        //for (p=0; p<nprotos; p++)
        d=1;
        //for (d=0;d<2; d++)  /* dtls or not dtls */
            //for (c=0; c<2; k++) /* csa or not */
            for (k=1; k<2; k++) /* kernel or not */
            {
                for (i=0; ciphers[i].cipher != (SSLCipherSuite)(-1); i++) {
                    l=0;
                    //for (l = 0; l<2; l++) {
#endif
                    SKIP:{
                        //skip("Session resumption tests do not work at this point", 1, l != 1);
                        int sp[2];

#if 0
                        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sp)) exit(errno);
                        fcntl(sp[0], F_SETNOSIGPIPE, 1);
                        fcntl(sp[1], F_SETNOSIGPIPE, 1);
#else
                        createsockets(sp);
#endif
                        ssl_test_handle *server, *client;

                        bool client_side_auth = (c);
                        bool kernel = (k);
                        uint32_t session_id = (c+1) << 16 | (i+1);
                        //fprintf(stderr, "session_id: %d\n", session_id);
                        server = ssl_test_handle_create(session_id, (l == 1), true /*server*/,
                                                        client_side_auth, ciphers[i].dh_anonymous, d,
                                                        sp[0], server_certs, protos[p], false);
                        client = ssl_test_handle_create(session_id, (l == 1), false/*client*/,
                                                        client_side_auth, ciphers[i].dh_anonymous, d,
                                                        sp[1], server_certs, protos[p], kernel);

                        require_noerr(SSLSetPeerID(server->st, &session_id, sizeof(session_id)), out);
                        require_noerr(SSLSetPeerID(client->st, &session_id, sizeof(session_id)), out);

                        /* set fixed cipher on client and server */
                        require_noerr(SSLSetEnabledCiphers(client->st, &ciphers[i].cipher, 1), out);
                        require_noerr(SSLSetEnabledCiphers(server->st, &ciphers[i].cipher, 1), out);


                        snprintf(msg, sizeof(msg),
                                 "%40s ADH:%d CSA:%d DTLS:%d RESUME:%d PROTO:%d KERNEL:%d",
                                 ciphers[i].name,
                                 server->dh_anonymous,
                                 server->client_side_auth,
                                 d, l, p, k);

                        printf("%s\n", msg);

                        pthread_create(&client_thread, NULL, securetransport_ssl_thread, client);
                        pthread_create(&server_thread, NULL, securetransport_ssl_thread, server);

                        int server_err, client_err;
                        pthread_join(client_thread, (void*)&client_err);
                        pthread_join(server_thread, (void*)&server_err);


                        __Check_String(!server_err && !client_err, msg);

                    out:
                        free(client);
                        free(server);

                        printf("\n\n");
                        sleep(2);
                    }
                } /* all ciphers */
            } /* all configs */

    CFRelease(server_certs);

    return 0;
}


