/*++
/* NAME
/*	pfixtls
/* SUMMARY
/*	interface to openssl routines
/* SYNOPSIS
/*	#include <pfixtls.h>
/*
/*	const long scache_db_version;
/*	const long openssl_version;
/*
/*	int pfixtls_serverengine;
/*
/*	int pfixtls_clientengine;
/*
/*	int pfixtls_timed_read(fd, buf, len, timeout, unused_context)
/*	int fd;
/*	void *buf;
/*	unsigned len;
/*	int timeout;
/*	void *context;
/*
/*	int pfixtls_timed_write(fd, buf, len, timeout, unused_context);
/*	int fd;
/*	void *buf;
/*	unsigned len;
/*	int timeout;
/*	void *context;
/*
/*	int pfixtls_init_serverengine(verifydepth, askcert);
/*	int verifydepth;
/*	int askcert;
/*
/*	int pfixtls_start_servertls(stream, timeout, peername, peeraddr,
/*				    tls_info, requirecert);
/*	VSTREAM *stream;
/*	int timeout;
/*	const char *peername;
/*	const char *peeraddr;
/*	tls_info_t *tls_info;
/*	int requirecert;
/*
/*	int pfixtls_stop_servertls(stream, failure, tls_info);
/*	VSTREAM *stream;
/*	int failure;
/*	tls_info_t *tls_info;
/*	
/*	int pfixtls_init_clientengine(verifydepth);
/*	int verifydepth;
/*
/*	int pfixtls_start_clienttls(stream, timeout, peername, peeraddr,
/*				    tls_info);
/*	VSTREAM *stream;
/*	int timeout;
/*	const char *peername;
/*	const char *peeraddr;
/*	tls_info_t *tls_info;
/*
/*	int pfixtls_stop_clienttls(stream, failure, tls_info);
/*	VSTREAM *stream;
/*	int failure;
/*	tls_info_t *tls_info;
/*
/* DESCRIPTION
/*	This module is the interface between Postfix and the OpenSSL library.
/*
/*	pfixtls_timed_read() reads the requested number of bytes calling
/*	SSL_read(). pfixtls_time_read() will only be called indirect
/*	as a VSTREAM_FN function.
/*	pfixtls_timed_write() is the corresponding write function.
/*
/*	pfixtls_init_serverengine() is called once when smtpd is started
/*	in order to initialize as much of the TLS stuff as possible.
/*	The certificate handling is also decided during the setup phase,
/*	so that a peer specific handling is not possible.
/*
/*	pfixtls_init_clientengine() is the corresponding function called
/*	in smtp. Here we take the peer's (server's) certificate in any
/*	case.
/*
/*	pfixtls_start_servertls() activates the TLS feature for the VSTREAM
/*	passed as argument. We expect that all buffers are flushed and the
/*	TLS handshake can begin	immediately. Information about the peer
/*	is stored into the tls_info structure passed as argument.
/*
/*	pfixtls_stop_servertls() sends the "close notify" alert via
/*	SSL_shutdown() to the peer and resets all connection specific
/*	TLS data. As RFC2487 does not specify a seperate shutdown, it
/*	is supposed that the underlying TCP connection is shut down
/*	immediately afterwards, so we don't care about additional data
/*	coming through the channel.
/*	If the failure flag is set, the session is cleared from the cache.
/*
/*	pfixtls_start_clienttls() and pfixtls_stop_clienttls() are the
/*	corresponding functions for smtp.
/*
/*	Once the TLS connection is initiated, information about the TLS
/*	state is available via the tls_info structure:
/*	protocol holds the protocol name (SSLv2, SSLv3, TLSv1),
/*	tls_info->cipher_name the cipher name (e.g. RC4/MD5),
/*	tls_info->cipher_usebits the number of bits actually used (e.g. 40),
/*	tls_info->cipher_algbits the number of bits the algorithm is based on
/*	(e.g. 128).
/*	The last two values may be different when talking to a crippled
/*	- ahem - export controled peer (e.g. 40/128).
/*
/*	The status of the peer certificate verification is available in
/*	pfixtls_peer_verified. It is set to 1, when the certificate could
/*	be verified.
/*	If the peer offered a certifcate, part of the certificate data are
/*	available as:
/*	tls_info->peer_subject X509v3-oneline with the DN of the peer
/*	tls_info->peer_CN extracted CommonName of the peer
/*	tls_info->peer_issuer  X509v3-oneline with the DN of the issuer
/*	tls_info->peer_CN extracted CommonName of the issuer
/*	tls_info->PEER_FINGERPRINT fingerprint of the certificate
/*
/* DESCRIPTION (SESSION CACHING)
/*	In order to achieve high performance when using a lot of connections
/*	with TLS, session caching is implemented. It reduces both the CPU load
/*	(less cryptograpic operations) and the network load (the amount of
/*	certificate data exchanged is reduced).
/*	Since postfix uses a setup of independent processes for receiving
/*	and sending email, the processes must exchange the session information.
/*	Several connections at the same time between the identical peers can
/*	occur, so uniqueness and race conditions have to be taken into
/*	account.
/*	I have checked both Apache-SSL (Ben Laurie), using a seperate "gcache"
/*	process and Apache mod_ssl (Ralf S. Engelshall), using shared memory
/*	between several identical processes spawned from one parent.
/*
/*	Postfix/TLS uses a database approach based on the internal "dict"
/*	interface. Since the session cache information is approximately
/*	1300 bytes binary data, it will not fit into the dbm/ndbm model.
/*	It also needs write access to the database, ruling out most other
/*	interface, leaving Berkeley DB, which however cannot handle concurrent
/*	access by several processes. Hence a modified SDBM (public domain DBM)
/*	with enhanced buffer size is used and concurrent write capability
/*	is used. SDBM is part of Postfix/TLS.
/*
/*	Realization:
/*	Both (client and server) session cache are realized by individual
/*	cache databases. A common database would not make sense, since the
/*	key criteria are different (session ID for server, peername for
/*	client).
/*
/*	Server side:
/*	Session created by OpenSSL have a 32 byte session id, yielding a
/*	64 char file name. I consider these sessions to be unique. If they
/*	are not, the last session will win, overwriting the older one in
/*	the database. Remember: everything that is lost is a temporary
/*	information and not more than a renegotiation will happen.
/*	Originating from the same client host, several sessions can come
/*	in (e.g. from several users sending mail with Netscape at the same
/*	time), so the session id is the correct identifier; the hostname
/*	is of no importance, here.
/*
/*	Client side:
/*	We cannot recall sessions based on their session id, because we would
/*	have to check every session on disk for a matching server name, so
/*	the lookup has to be done based on the FQDN of the peer (receiving
/*	host).
/*	With regard to uniqueness, we might experience several open connections
/*	to the same server at the same time. This is even very likely to
/*	happen, since we might have several mails for the same destination
/*	in the queue, when a queue run is started. So several smtp's might
/*	negotiate sessions at the same time. We can however only save one
/*	session for one host.
/*	Like on the server side, the "last write" wins. The reason is
/*	quite simple. If we don't want to overwrite old sessions, an old
/*	session file will just stay in place until it is expired. In the
/*	meantime we would lose "fresh" session however. So we will keep the
/*	fresh one instead to avoid unnecessary renegotiations.
/*
/*	Session lifetime:
/*	RFC2246 recommends a session lifetime of less than 24 hours. The
/*	default is 300 seconds (5 minutes) for OpenSSL and is also used
/*	this way in e.g. mod_ssl. The typical usage for emails might be
/*	humans typing in emails and sending them, which might take just
/*	a while, so I think 3600 seconds (1 hour) is a good compromise.
/*	If the environment is save (the cached session contains secret
/*	key data), one might even consider using a longer timeout. Anyway,
/*	since everlasting sessions must be avoided, the session timeout
/*	is done based on the creation date of the session and so each
/*	session will timeout eventually.
/*
/*	Connection failures:
/*	RFC2246 requires us to remove sessions if something went wrong.
/*	Since the in-memory session cache of other smtp[d] processes cannot
/*	be controlled by simple means, we completely rely on the disc
/*	based session caching and remove all sessions from memory after
/*	connection closure.
/*
/*	Cache cleanup:
/*	Since old entries have to be removed from the session cache, a
/*	cleanup process is needed that runs through the collected session
/*	files on regular basis. The task is performed by tlsmgr based on
/*	the timestamp created by pfixtls and included in the saved session,
/*	so that tlsmgr has not to care about the SSL_SESSION internal data.
/*
/* BUGS
/*	The memory allocation policy of the OpenSSL library is not well
/*	documented, especially when loading sessions from disc. Hence there
/*	might be memory leaks.
/*
/* LICENSE
/* AUTHOR(S)
/*	Lutz Jaenicke
/*	BTU Cottbus
/*	Allgemeine Elektrotechnik
/*	Universitaetsplatz 3-4
/*	D-03044 Cottbus, Germany
/*--*/

/* System library. */

#include <sys_defs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>			/* gettimeofday, not in POSIX */
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

/* Utility library. */

#include <iostuff.h>
#include <mymalloc.h>
#include <vstring.h>
#include <vstream.h>
#include <dict.h>
#include <myflock.h>
#include <stringops.h>
#include <msg.h>
#include <connect.h>

/* Application-specific. */

#include "mail_params.h"
#include "pfixtls.h"

#define STR	vstring_str

const tls_info_t tls_info_zero = {
    0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0
};

#ifdef USE_SSL

/* OpenSSL library. */

#include <openssl/lhash.h>
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>

#ifdef __APPLE__
#include <Security/SecKeychain.h>

typedef struct
{
	int		len;
	char	key[ FILENAME_MAX ];
	int		reserved;
} CallbackUserData;

static CallbackUserData *sUserData = NULL;
#endif /* __APPLE__ */

/* We must keep some of the info available */
static const char hexcodes[] = "0123456789ABCDEF";

/*
 * When saving sessions, we want to make sure, that the lenght of the key
 * is somehow limited. When saving client sessions, the hostname is used
 * as key. According to HP-UX 10.20, MAXHOSTNAMELEN=64. Maybe new standards
 * will increase this value, but as this will break compatiblity with existing
 * implementations, we won't see this for long. We therefore choose a limit
 * of 64 bytes.
 * The length of the (TLS) session id can be up to 32 bytes according to
 * RFC2246, so it fits well into the 64bytes limit.
 */
#define ID_MAXLENGTH	64		/* Max ID length in bytes */

/*
 * The session_id_context is set, such that the client knows which services
 * on a host share the same session information (on the postfix host may
 * as well run a TLS-enabled webserver.
 */
static char server_session_id_context[] = "Postfix/TLS"; /* anything will do */
static int TLScontext_index = -1;
static int TLSpeername_index = -1;
static int do_dump = 0;
static DH *dh_512 = NULL, *dh_1024 = NULL;
static SSL_CTX *ctx = NULL;

static int rand_exch_fd = -1;

static DICT *scache_db = NULL;
const long scache_db_version = 0x00000003L;
const long openssl_version = OPENSSL_VERSION_NUMBER;


int     pfixtls_serverengine = 0;
static int pfixtls_serveractive = 0;	/* available or not */

int     pfixtls_clientengine = 0;
static int pfixtls_clientactive = 0;	/* available or not */

/*
 * Define a maxlength for certificate onelines. The length is checked by
 * all routines when copying.
 */
#define CCERT_BUFSIZ 256

typedef struct {
  SSL *con;
  BIO *internal_bio;			/* postfix/TLS side of pair */
  BIO *network_bio;			/* netsork side of pair */
  char peer_subject[CCERT_BUFSIZ];
  char peer_issuer[CCERT_BUFSIZ];
  char peer_CN[CCERT_BUFSIZ];
  char issuer_CN[CCERT_BUFSIZ];
  unsigned char md[EVP_MAX_MD_SIZE];
  char fingerprint[EVP_MAX_MD_SIZE * 3];
  char peername_save[129];
  int enforce_verify_errors;
  int enforce_CN;
  int hostname_matched;
} TLScontext_t;

typedef struct {
    int pid;
    struct timeval tv;
} randseed_t;

static randseed_t randseed;

/*
 * Finally some "backup" DH-Parameters to be loaded, if no parameters are
 * explicitely loaded from file.
 */
static unsigned char dh512_p[] = {
    0x88, 0x3F, 0x00, 0xAF, 0xFC, 0x0C, 0x8A, 0xB8, 0x35, 0xCD, 0xE5, 0xC2,
    0x0F, 0x55, 0xDF, 0x06, 0x3F, 0x16, 0x07, 0xBF, 0xCE, 0x13, 0x35, 0xE4,
    0x1C, 0x1E, 0x03, 0xF3, 0xAB, 0x17, 0xF6, 0x63, 0x50, 0x63, 0x67, 0x3E,
    0x10, 0xD7, 0x3E, 0xB4, 0xEB, 0x46, 0x8C, 0x40, 0x50, 0xE6, 0x91, 0xA5,
    0x6E, 0x01, 0x45, 0xDE, 0xC9, 0xB1, 0x1F, 0x64, 0x54, 0xFA, 0xD9, 0xAB,
    0x4F, 0x70, 0xBA, 0x5B,
};

static unsigned char dh512_g[] = {
    0x02,
};

static unsigned char dh1024_p[] = {
    0xB0, 0xFE, 0xB4, 0xCF, 0xD4, 0x55, 0x07, 0xE7, 0xCC, 0x88, 0x59, 0x0D,
    0x17, 0x26, 0xC5, 0x0C, 0xA5, 0x4A, 0x92, 0x23, 0x81, 0x78, 0xDA, 0x88,
    0xAA, 0x4C, 0x13, 0x06, 0xBF, 0x5D, 0x2F, 0x9E, 0xBC, 0x96, 0xB8, 0x51,
    0x00, 0x9D, 0x0C, 0x0D, 0x75, 0xAD, 0xFD, 0x3B, 0xB1, 0x7E, 0x71, 0x4F,
    0x3F, 0x91, 0x54, 0x14, 0x44, 0xB8, 0x30, 0x25, 0x1C, 0xEB, 0xDF, 0x72,
    0x9C, 0x4C, 0xF1, 0x89, 0x0D, 0x68, 0x3F, 0x94, 0x8E, 0xA4, 0xFB, 0x76,
    0x89, 0x18, 0xB2, 0x91, 0x16, 0x90, 0x01, 0x99, 0x66, 0x8C, 0x53, 0x81,
    0x4E, 0x27, 0x3D, 0x99, 0xE7, 0x5A, 0x7A, 0xAF, 0xD5, 0xEC, 0xE2, 0x7E,
    0xFA, 0xED, 0x01, 0x18, 0xC2, 0x78, 0x25, 0x59, 0x06, 0x5C, 0x39, 0xF6,
    0xCD, 0x49, 0x54, 0xAF, 0xC1, 0xB1, 0xEA, 0x4A, 0xF9, 0x53, 0xD0, 0xDF,
    0x6D, 0xAF, 0xD4, 0x93, 0xE7, 0xBA, 0xAE, 0x9B,
};

static unsigned char dh1024_g[] = {
    0x02,
};

/*
 * DESCRIPTION: Keeping control of the network interface using BIO-pairs.
 *
 * When the TLS layer is active, all input/output must be filtered through
 * it. On the other hand to handle timeout conditions, full control over
 * the network socket must be kept. This rules out the "normal way" of
 * connecting the TLS layer directly to the socket.
 * The TLS layer is realized with a BIO-pair:
 *
 *     postfix  |   TLS-engine
 *       |      |
 *       +--------> SSL_operations()
 *              |     /\    ||
 *              |     ||    \/
 *              |   BIO-pair (internal_bio)
 *       +--------< BIO-pair (network_bio)
 *       |      |
 *     socket   |
 *
 * The normal postfix operations connect to the SSL operations to send
 * and retrieve (cleartext) data. Inside the TLS-engine the data are converted
 * to/from TLS protocol. The TLS functionality itself is only connected to
 * the internal_bio and hence only has status information about this internal
 * interface.
 * Thus, if the SSL_operations() return successfully (SSL_ERROR_NONE) or want
 * to read (SSL_ERROR_WANT_READ) there may as well be data inside the buffering
 * BIO-pair. So whenever an SSL_operation() returns without a fatal error,
 * the BIO-pair internal buffer must be flushed to the network.
 * NOTE: This is especially true in the SSL_ERROR_WANT_READ case: the TLS-layer
 * might want to read handshake data, that will never come since its own
 * written data will only reach the peer after flushing the buffer!
 *
 * The BIO-pair buffer size has been set to 8192 bytes, this is an arbitrary
 * value that can hold more data than the typical PMTU, so that it does
 * not force the generation of packets smaller than necessary.
 * It is also larger than the default VSTREAM_BUFSIZE (4096, see vstream.h),
 * so that large write operations could be handled within one call.
 * The internal buffer in the network/network_bio handling layer has been
 * set to the same value, since this seems to be reasonable. The code is
 * however able to handle arbitrary values smaller or larger than the
 * buffer size in the BIO-pair.
 */

const size_t BIO_bufsiz = 8192;

/*
 * The interface layer between network and BIO-pair. The BIO-pair buffers
 * the data to/from the TLS layer. Hence, at any time, there may be data
 * in the buffer that must be written to the network. This writing has
 * highest priority because the handshake might fail otherwise.
 * Only then a read_request can be satisfied.
 */
static int network_biopair_interop(int fd, int timeout, BIO *network_bio)
{
    int want_write;
    int num_write;
    int write_pos;
    int from_bio;
    int want_read;
    int num_read;
    int to_bio;
#define NETLAYER_BUFFERSIZE 8192
    char buffer[8192];

    while ((want_write = BIO_ctrl_pending(network_bio)) > 0) {
	if (want_write > NETLAYER_BUFFERSIZE)
	    want_write = NETLAYER_BUFFERSIZE;
	from_bio = BIO_read(network_bio, buffer, want_write);

	/*
	 * Write the complete contents of the buffer. Since TLS performs
	 * underlying handshaking, we cannot afford to leave the buffer
	 * unflushed, as we could run into a deadlock trap (the peer
	 * waiting for a final byte and we already waiting for his reply
	 * in read position).
	 */
        write_pos = 0;
	do {
	    if (timeout > 0 && write_wait(fd, timeout) < 0)
		return (-1);
	    num_write = write(fd, buffer + write_pos, from_bio - write_pos);
	    if (num_write <= 0) {
		if ((num_write < 0) && (timeout > 0) && (errno == EAGAIN)) {
		    msg_warn("write() returns EAGAIN on a writable file descriptor!");
		    msg_warn("pausing to avoid going into a tight select/write loop!");
		    sleep(1);
		} else {
		    msg_warn("Write failed in network_biopair_interop with errno=%d: num_write=%d, provided=%d", errno, num_write, from_bio - write_pos);
		    return (-1);	/* something happened to the socket */
		}
	    } else
	    	write_pos += num_write;
	} while (write_pos < from_bio);
   }

   while ((want_read = BIO_ctrl_get_read_request(network_bio)) > 0) {
	if (want_read > NETLAYER_BUFFERSIZE)
	    want_read = NETLAYER_BUFFERSIZE;
	if (timeout > 0 && read_wait(fd, timeout) < 0)
	    return (-1);
	num_read = read(fd, buffer, want_read);
	if (num_read <= 0) {
	    if ((num_write < 0) && (timeout > 0) && (errno == EAGAIN)) {
		msg_warn("read() returns EAGAIN on a readable file descriptor!");
		msg_warn("pausing to avoid going into a tight select/write loop!");
		sleep(1);
	    } else {
		msg_warn("Read failed in network_biopair_interop with errno=%d: num_read=%d, want_read=%d", errno, num_read, want_read);
		return (-1);	/* something happened to the socket */
	    }
	} else {
	    to_bio = BIO_write(network_bio, buffer, num_read);
	    if (to_bio != num_read)
		msg_fatal("to_bio != num_read");
	}
    }

    return (0);
}

static void pfixtls_print_errors(void);

 /*
  * Function to perform the handshake for SSL_accept(), SSL_connect(),
  * and SSL_shutdown() and perform the SSL_read(), SSL_write() operations.
  * Call the underlying network_biopair_interop-layer to make sure the
  * write buffer is flushed after every operation (that did not fail with
  * a fatal error).
  */
static int do_tls_operation(int fd, int timeout, TLScontext_t *TLScontext,
			int (*hsfunc)(SSL *),
			int (*rfunc)(SSL *, void *, int),
			int (*wfunc)(SSL *, const void *, int),
			char *buf, int num)
{
    int status;
    int err;
    int retval = 0;
    int biop_retval;
    int done = 0;

    while (!done) {
	if (hsfunc)
	    status = hsfunc(TLScontext->con);
	else if (rfunc)
	    status = rfunc(TLScontext->con, buf, num);
	else
	    status = wfunc(TLScontext->con, (const char *)buf, num);
	err = SSL_get_error(TLScontext->con, status);

#if (OPENSSL_VERSION_NUMBER <= 0x0090581fL)
	/*
	 * There is a bug up to and including OpenSSL-0.9.5a: if an error
	 * occurs while checking the peers certificate due to some certificate
	 * error (e.g. as happend with a RSA-padding error), the error is put
	 * onto the error stack. If verification is not enforced, this error
	 * should be ignored, but the error-queue is not cleared, so we
	 * can find this error here. The bug has been fixed on May 28, 2000.
	 *
	 * This bug so far has only manifested as
	 * 4800:error:0407006A:rsa routines:RSA_padding_check_PKCS1_type_1:block type is not 01:rsa_pk1.c:100:
	 * 4800:error:04067072:rsa routines:RSA_EAY_PUBLIC_DECRYPT:padding check failed:rsa_eay.c:396:
	 * 4800:error:0D079006:asn1 encoding routines:ASN1_verify:bad get asn1 object call:a_verify.c:109:
	 * so that we specifically test for this error. We print the errors
	 * to the logfile and automatically clear the error queue. Then we
	 * retry to get another error code. We cannot do better, since we
	 * can only retrieve the last entry of the error-queue without
	 * actually cleaning it on the way.
	 *
	 * This workaround is secure, as verify_result is set to "failed"
	 * anyway.
	 */
	if (err == SSL_ERROR_SSL) {
	    if (ERR_peek_error() == 0x0407006AL) {
		pfixtls_print_errors();	/* Keep information for the logfile */
		msg_info("OpenSSL <= 0.9.5a workaround called: certificate errors ignored");
		err = SSL_get_error(TLScontext->con, status);
	    }
	}
#endif

	switch (err) {
	case SSL_ERROR_NONE:		/* success */
	    retval = status;
	    done = 1;			/* no break, flush buffer before */
					/* leaving */
	case SSL_ERROR_WANT_WRITE:
	case SSL_ERROR_WANT_READ:
	    biop_retval = network_biopair_interop(fd, timeout,
		TLScontext->network_bio);
	    if (biop_retval < 0)
		return (-1);		/* fatal network error */
	    break;
	case SSL_ERROR_ZERO_RETURN:	/* connection was closed cleanly */
	case SSL_ERROR_SYSCALL:		
	case SSL_ERROR_SSL:
	default:
	    retval = status;
	    done = 1;
	    ;
	}
    };
    return retval;
}

int pfixtls_timed_read(int fd, void *buf, unsigned buf_len, int timeout, 
		       void *context)
{
    int     i;
    int     ret;
    char    mybuf[40];
    char   *mybuf2;
    TLScontext_t *TLScontext;

    TLScontext = (TLScontext_t *)context;
    if (!TLScontext)
      msg_fatal("Called tls_timed_read() without TLS-context");
 
    ret = do_tls_operation(fd, timeout, TLScontext, NULL, SSL_read, NULL,
			  (char *)buf, buf_len);
    if ((pfixtls_serveractive && var_smtpd_tls_loglevel >= 4) ||
        (pfixtls_clientactive && var_smtp_tls_loglevel >= 4)) {
	mybuf2 = (char *) buf;
	if (ret > 0) {
	    i = 0;
	    while ((i < 39) && (i < ret) && (mybuf2[i] != 0)) {
		mybuf[i] = mybuf2[i];
		i++;
	    }
	    mybuf[i] = '\0';
	    msg_info("Read %d chars: %s", ret, mybuf);
	}
    }
    return (ret);
}

int pfixtls_timed_write(int fd, void *buf, unsigned len, int timeout,
			void *context)
{
    int     i;
    char    mybuf[40];
    char   *mybuf2;
    TLScontext_t *TLScontext;

    TLScontext = (TLScontext_t *)context;
    if (!TLScontext)
      msg_fatal("Called tls_timed_write() without TLS-context");

    if ((pfixtls_serveractive && var_smtpd_tls_loglevel >= 4) ||
	(pfixtls_clientactive && var_smtp_tls_loglevel >= 4)) {
	mybuf2 = (char *) buf;
	if (len > 0) {
	    i = 0;
	    while ((i < 39) && (i < len) && (mybuf2[i] != 0)) {
		mybuf[i] = mybuf2[i];
		i++;
	    }
	    mybuf[i] = '\0';
	    msg_info("Write %d chars: %s", len, mybuf);
	}
    }
    return (do_tls_operation(fd, timeout, TLScontext, NULL, NULL, SSL_write,
			     buf, len));
}

/* Add some more entropy to the pool by adding the actual time */

static void pfixtls_stir_seed(void)
{
    GETTIMEOFDAY(&randseed.tv);
    RAND_seed(&randseed, sizeof(randseed_t));
}

/*
 * Skeleton taken from OpenSSL crypto/err/err_prn.c.
 * Query the error stack and print the error string into the logging facility.
 * Clear the error stack on the way.
 */

static void pfixtls_print_errors(void)
{
    unsigned long l;
    char    buf[256];
    const char   *file;
    const char   *data;
    int     line;
    int     flags;
    unsigned long es;

    es = CRYPTO_thread_id();
    while ((l = ERR_get_error_line_data(&file, &line, &data, &flags)) != 0) {
	if (flags & ERR_TXT_STRING)
	    msg_info("%lu:%s:%s:%d:%s:", es, ERR_error_string(l, buf),
		     file, line, data);
	else
	    msg_info("%lu:%s:%s:%d:", es, ERR_error_string(l, buf),
		     file, line);
    }
}


#ifdef __APPLE__

/* apple_password_callback */

int apple_password_callback ( char *inBuf, int inSize, int in_rwflag, void *inUserData )
{
	OSStatus			status		= noErr;
	SecKeychainItemRef	keyChainRef	= NULL;
	void			   *pwdBuf		= NULL;  /* will be allocated and filled in by SecKeychainFindGenericPassword */
	UInt32				pwdLen		= 0;
	char			   *service		= "certificateManager"; /* defined by Apple */
	CallbackUserData   *cbUserData	= (CallbackUserData *)inUserData;

	if ( (cbUserData == NULL) || strlen( cbUserData->key ) == 0 ||
		 (cbUserData->len >= FILENAME_MAX) || (cbUserData->len == 0) || !inBuf )
	{
		if (var_smtpd_tls_loglevel >= 3)
			msg_info("Error: Invalid arguments in callback" );
		return( 0 );
	}

	/* Set the domain to System (daemon) */
	status = SecKeychainSetPreferenceDomain( kSecPreferencesDomainSystem );
	if ( status != noErr )
	{
		if (var_smtpd_tls_loglevel >= 3)
			msg_info("Error: SecKeychainSetPreferenceDomain returned status: %d", status );
		return( 0 );
	}

	// Passwords created by cert management have the keychain access dialog suppressed.
	status = SecKeychainFindGenericPassword( NULL, strlen( service ), service,
												   cbUserData->len, cbUserData->key,
												   &pwdLen, &pwdBuf,
												   &keyChainRef );

	if ( (status == noErr) && (keyChainRef != NULL) )
	{
		if ( pwdLen > inSize )
		{
			if (var_smtpd_tls_loglevel >= 3)
				msg_info("Error: Invalid buffer size callback (size:%d, len:%d)", inSize, pwdLen );
			SecKeychainItemFreeContent( NULL, pwdBuf );
			return( 0 );
		}

		memcpy( inBuf, (const void *)pwdBuf, pwdLen );
		if ( inSize > 0 )
		{
			inBuf[ pwdLen ] = 0;
			inBuf[ inSize - 1 ] = 0;
		}

		SecKeychainItemFreeContent( NULL, pwdBuf );

		return( strlen(inBuf ) );
	}
	else if (status == errSecNotAvailable)
	{
		if (var_smtpd_tls_loglevel >= 3)
			msg_info("Error: SecKeychainSetPreferenceDomain: No keychain is available" );
	}
	else if ( status == errSecItemNotFound )
	{
		if (var_smtpd_tls_loglevel >= 3)
			msg_info("Error: SecKeychainSetPreferenceDomain: The requested key could not be found in the system keychain");
	}
	else if (status != noErr)
	{
		if (var_smtpd_tls_loglevel >= 3)
			msg_info("Error: SecKeychainFindGenericPassword returned status %d", status);
	}

	return( 0 );
}
#endif /* __APPLE__ */


 /*
  * Set up the cert things on the server side. We do need both the
  * private key (in key_file) and the cert (in cert_file).
  * Both files may be identical.
  *
  * This function is taken from OpenSSL apps/s_cb.c
  */

static int set_cert_stuff(SSL_CTX * ctx, char *cert_file, char *key_file)
{
#ifdef __APPLE__
	if ( sUserData == NULL )
	{
		sUserData = mymalloc( sizeof(CallbackUserData) );
		if ( sUserData != NULL )
		{
			memset( sUserData, 0, sizeof(CallbackUserData) );
		}
	}
#endif /* __APPLE__ */
    if (cert_file != NULL) {
	if (SSL_CTX_use_certificate_chain_file(ctx, cert_file) <= 0) {
	    msg_info("unable to get certificate from '%s'", cert_file);
	    pfixtls_print_errors();
	    return (0);
	}
	if (key_file == NULL)
	    key_file = cert_file;
#ifdef __APPLE__
	if ( strlen( key_file ) < FILENAME_MAX )
	{
		char tmp[ FILENAME_MAX ];
		strlcpy( tmp, key_file, FILENAME_MAX );
		char *p = tmp;
		char *q = tmp;

		while ( *p != '\0' )
		{
			if ( *p == '/' )
			{
				q = p;
			}
			p++;
		}

		if ( (*q != '\0') && (*q == '/') && (*p+1 != '\0') )
		{
			p = ++q;
			int len = strlen( p );

			if ( sUserData != NULL )
			{
				if ( strncmp( p+(len-4), ".key", 4 ) == 0 )
				{
					len = len - 4;
					strncpy( sUserData->key, p, len );
					sUserData->key[ len ] = '\0';
					sUserData->len = len;
				}
				else
				{
					strcpy( sUserData->key, p );
					sUserData->len = strlen( p );
				}
				SSL_CTX_set_default_passwd_cb_userdata( ctx, (void *)sUserData );
				SSL_CTX_set_default_passwd_cb( ctx, &apple_password_callback );
			}
			else
			{
				msg_info("Could not allocate data for callback: %s", key_file);
			}
		}
		else
		{
			msg_info("Could not set custom callback: %s", key_file);
		}
	}
	else
	{
		msg_info("Key file path too big for custom callback: %s", key_file );
	}
#endif /* __APPLE__ */
	if (SSL_CTX_use_PrivateKey_file(ctx, key_file,
					SSL_FILETYPE_PEM) <= 0) {
	    msg_info("unable to get private key from '%s'", key_file);
	    pfixtls_print_errors();
	    return (0);
	}
	/* Now we know that a key and cert have been set against
         * the SSL context */
	if (!SSL_CTX_check_private_key(ctx)) {
	    msg_info("Private key does not match the certificate public key");
	    return (0);
	}
    }
    return (1);
}

/* taken from OpenSSL apps/s_cb.c */

static RSA *tmp_rsa_cb(SSL * s, int export, int keylength)
{
    static RSA *rsa_tmp = NULL;

    if (rsa_tmp == NULL) {
	rsa_tmp = RSA_generate_key(keylength, RSA_F4, NULL, NULL);
    }
    return (rsa_tmp);
}


static DH *get_dh512(void)
{
    DH *dh;

    if (dh_512 == NULL) {
	/* No parameter file loaded, use the compiled in parameters */
	if ((dh = DH_new()) == NULL) return(NULL);
	dh->p = BN_bin2bn(dh512_p, sizeof(dh512_p), NULL);
	dh->g = BN_bin2bn(dh512_g, sizeof(dh512_g), NULL);
	if ((dh->p == NULL) || (dh->g == NULL))
	    return(NULL);
	else
	    dh_512 = dh;
    }
    return (dh_512);
}

static DH *get_dh1024(void)
{
    DH *dh;

    if (dh_1024 == NULL) {
	/* No parameter file loaded, use the compiled in parameters */
	if ((dh = DH_new()) == NULL) return(NULL);
	dh->p = BN_bin2bn(dh1024_p, sizeof(dh1024_p), NULL);
	dh->g = BN_bin2bn(dh1024_g, sizeof(dh1024_g), NULL);
	if ((dh->p == NULL) || (dh->g == NULL))
	    return(NULL);
	else
	    dh_1024 = dh;
    }
    return (dh_1024);
}

/* partly inspired by mod_ssl */

static DH *tmp_dh_cb(SSL *s, int export, int keylength)
{
    DH *dh_tmp = NULL;
   
    if (export) {
	if (keylength == 512)
	    dh_tmp = get_dh512();	/* export cipher */
	else if (keylength == 1024)
	    dh_tmp = get_dh1024();	/* normal */
	else
	    dh_tmp = get_dh1024();	/* not on-the-fly (too expensive) */
					/* so use the 1024bit instead */
    }
    else {
	dh_tmp = get_dh1024();		/* sign-only certificate */
    }
    return (dh_tmp);
}


/*
 * match_hostname: match name provided in "buf" against the expected
 * hostname. Comparison is case-insensitive, wildcard certificates are
 * supported.
 * "buf" may be come from some OpenSSL data structures, so we copy before
 * modifying.
 */
static int match_hostname(const char *buf, TLScontext_t *TLScontext)
{
    char   *hostname_lowercase;
    char   *peername_left;
    int hostname_matched = 0;
    int buf_len;

    buf_len = strlen(buf);
    if (!(hostname_lowercase = (char *)mymalloc(buf_len + 1)))
	return 0;
    memcpy(hostname_lowercase, buf, buf_len + 1);

    hostname_lowercase = lowercase(hostname_lowercase);
    if (!strcmp(TLScontext->peername_save, hostname_lowercase)) {
        hostname_matched = 1;
    } else { 
        if ((buf_len > 2) &&
            (hostname_lowercase[0] == '*') && (hostname_lowercase[1] == '.')) {
            /*
             * Allow wildcard certificate matching. The proposed rules in  
             * RFCs (2818: HTTP/TLS, 2830: LDAP/TLS) are different, RFC2874
             * does not specify a rule, so here the strict rule is applied.
             * An asterisk '*' is allowed as the leftmost component and may
             * replace the left most part of the hostname. Matching is done
             * by removing '*.' from the wildcard name and the Name. from
             * the peername and compare what is left.
             */
            peername_left = strchr(TLScontext->peername_save, '.');
            if (peername_left) {
                if (!strcmp(peername_left + 1, hostname_lowercase + 2))
                    hostname_matched = 1;
            }
        }
    }
    myfree(hostname_lowercase);
    return hostname_matched;
}
                                       
/*
 * Skeleton taken from OpenSSL apps/s_cb.c
 *
 * The verify_callback is called several times (directly or indirectly) from
 * crypto/x509/x509_vfy.c. It is called as a last check for several issues,
 * so this verify_callback() has the famous "last word". If it does return "0",
 * the handshake is immediately shut down and the connection fails.
 *
 * Postfix/TLS has two modes, the "use" mode and the "enforce" mode:
 *
 * In the "use" mode we never want the connection to fail just because there is
 * something wrong with the certificate (as we would have sent happily without
 * TLS).  Therefore the return value is always "1".
 *
 * In the "enforce" mode we can shut down the connection as soon as possible.
 * In server mode TLS itself may be enforced (e.g. to protect passwords),
 * but certificates are optional. In this case the handshake must not fail
 * if we are unhappy with the certificate and return "1" in any case.
 * Only if a certificate is required the certificate must pass the verification
 * and failure to do so will result in immediate termination (return 0).
 * In the client mode the decision is made with respect to the peername
 * enforcement. If we strictly enforce the matching of the expected peername
 * the verification must fail immediatly on verification errors. We can also
 * immediatly check the expected peername, as it is the CommonName at level 0.
 * In all other cases, the problem is logged, so the SSL_get_verify_result()
 * will inform about the verification failure, but the handshake (and SMTP
 * connection will continue).
 *
 * The only error condition not handled inside the OpenSSL-Library is the
 * case of a too-long certificate chain, so we check inside verify_callback().
 * We only take care of this problem, if "ok = 1", because otherwise the
 * verification already failed because of another problem and we don't want
 * to overwrite the other error message. And if the verification failed,
 * there is no such thing as "more failed", "most failed"... :-)
 */

static int verify_callback(int ok, X509_STORE_CTX * ctx)
{
    char    buf[256];
    char   *peername_left;
    X509   *err_cert;
    int     err;
    int     depth;
    int     verify_depth;
    SSL    *con;
    TLScontext_t *TLScontext;

    err_cert = X509_STORE_CTX_get_current_cert(ctx);
    err = X509_STORE_CTX_get_error(ctx);
    depth = X509_STORE_CTX_get_error_depth(ctx);

    con = X509_STORE_CTX_get_ex_data(ctx, SSL_get_ex_data_X509_STORE_CTX_idx());
    TLScontext = SSL_get_ex_data(con, TLScontext_index);

    X509_NAME_oneline(X509_get_subject_name(err_cert), buf, 256);
    if (((pfixtls_serverengine) && (var_smtpd_tls_loglevel >= 2)) ||
	((pfixtls_clientengine) && (var_smtp_tls_loglevel >= 2)))
	msg_info("Peer cert verify depth=%d %s", depth, buf);

    verify_depth = SSL_get_verify_depth(con);
    if (ok && (verify_depth >= 0) && (depth > verify_depth)) {
	ok = 0;
	err = X509_V_ERR_CERT_CHAIN_TOO_LONG;
	X509_STORE_CTX_set_error(ctx, err);
    }
    if (!ok) {
	msg_info("verify error:num=%d:%s", err,
		 X509_verify_cert_error_string(err));
    }

    if (ok && (depth == 0) && pfixtls_clientengine) {
	int i, r;
        int hostname_matched;
	int dNSName_found;
	STACK_OF(GENERAL_NAME) *gens;

	/*
	 * Check out the name certified against the hostname expected.
	 * In case it does not match, print an information about the result.
	 * If a matching is enforced, bump out with a verification error
	 * immediately.
	 * Standards are not always clear with respect to the handling of
	 * dNSNames. RFC3207 does not specify the handling. We therefore follow
	 * the strict rules in RFC2818 (HTTP over TLS), Section 3.1:
	 * The Subject Alternative Name/dNSName has precedence over CommonName
	 * (CN). If dNSName entries are provided, CN is not checked anymore.
	 */
	hostname_matched = dNSName_found = 0;

        gens = X509_get_ext_d2i(err_cert, NID_subject_alt_name, 0, 0);
        if (gens) {
            for (i = 0, r = sk_GENERAL_NAME_num(gens); i < r; ++i) {
                const GENERAL_NAME *gn = sk_GENERAL_NAME_value(gens, i);
                if (gn->type == GEN_DNS) {
		    dNSName_found++;
                    if ((hostname_matched =
			match_hostname((char *)gn->d.ia5->data, TLScontext)))
			break;
                }
            }
	    sk_GENERAL_NAME_free(gens);
        }
	if (dNSName_found) {
	    if (!hostname_matched)
		msg_info("Peer verification: %d dNSNames in certificate found, but no one does match %s", dNSName_found, TLScontext->peername_save);
	} else {
	    buf[0] = '\0';
	    if (!X509_NAME_get_text_by_NID(X509_get_subject_name(err_cert),
                          NID_commonName, buf, 256)) {
	        msg_info("Could not parse server's subject CN");
	        pfixtls_print_errors();
	    }
	    else {
	        hostname_matched = match_hostname(buf, TLScontext);
	        if (!hostname_matched)
		    msg_info("Peer verification: CommonName in certificate does not match: %s != %s", buf, TLScontext->peername_save);
	    }
	}

	if (!hostname_matched) {
	    if (TLScontext->enforce_verify_errors && TLScontext->enforce_CN) {
		err = X509_V_ERR_CERT_REJECTED;
		X509_STORE_CTX_set_error(ctx, err);
		msg_info("Verify failure: Hostname mismatch");
		ok = 0;
	    }
	}
	else
	    TLScontext->hostname_matched = 1;
    }

    switch (ctx->error) {
    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
	X509_NAME_oneline(X509_get_issuer_name(ctx->current_cert), buf, 256);
	msg_info("issuer= %s", buf);
	break;
    case X509_V_ERR_CERT_NOT_YET_VALID:
    case X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD:
	msg_info("cert not yet valid");
	break;
    case X509_V_ERR_CERT_HAS_EXPIRED:
    case X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD:
	msg_info("cert has expired");
	break;
    }
    if (((pfixtls_serverengine) && (var_smtpd_tls_loglevel >= 2)) ||
	((pfixtls_clientengine) && (var_smtp_tls_loglevel >= 2)))
	msg_info("verify return:%d", ok);

    if (TLScontext->enforce_verify_errors)
	return (ok); 
    else
	return (1);
}

/* taken from OpenSSL apps/s_cb.c */

static void apps_ssl_info_callback(SSL * s, int where, int ret)
{
    char   *str;
    int     w;

    w = where & ~SSL_ST_MASK;

    if (w & SSL_ST_CONNECT)
	str = "SSL_connect";
    else if (w & SSL_ST_ACCEPT)
	str = "SSL_accept";
    else
	str = "undefined";

    if (where & SSL_CB_LOOP) {
	    msg_info("%s:%s", str, SSL_state_string_long(s));
    } else if (where & SSL_CB_ALERT) {
	str = (where & SSL_CB_READ) ? "read" : "write";
	if ((ret & 0xff) != SSL3_AD_CLOSE_NOTIFY)
	msg_info("SSL3 alert %s:%s:%s", str,
		 SSL_alert_type_string_long(ret),
		 SSL_alert_desc_string_long(ret));
    } else if (where & SSL_CB_EXIT) {
	if (ret == 0)
	    msg_info("%s:failed in %s",
		     str, SSL_state_string_long(s));
	else if (ret < 0) {
	    msg_info("%s:error in %s",
		     str, SSL_state_string_long(s));
	}
    }
}

/*
 * taken from OpenSSL crypto/bio/b_dump.c, modified to save a lot of strcpy
 * and strcat by Matti Aarnio.
 */

#define TRUNCATE
#define DUMP_WIDTH	16

static int pfixtls_dump(const char *s, int len)
{
    int     ret = 0;
    char    buf[160 + 1];
    char    *ss;
    int     i;
    int     j;
    int     rows;
    int     trunc;
    unsigned char ch;

    trunc = 0;

#ifdef TRUNCATE
    for (; (len > 0) && ((s[len - 1] == ' ') || (s[len - 1] == '\0')); len--)
	trunc++;
#endif

    rows = (len / DUMP_WIDTH);
    if ((rows * DUMP_WIDTH) < len)
	rows++;

    for (i = 0; i < rows; i++) {
	buf[0] = '\0';				/* start with empty string */
	ss = buf;

	sprintf(ss, "%04x ", i * DUMP_WIDTH);
	ss += strlen(ss);
	for (j = 0; j < DUMP_WIDTH; j++) {
	    if (((i * DUMP_WIDTH) + j) >= len) {
		strcpy(ss, "   ");
	    } else {
		ch = ((unsigned char) *((char *) (s) + i * DUMP_WIDTH + j))
		    & 0xff;
		sprintf(ss, "%02x%c", ch, j == 7 ? '|' : ' ');
		ss += 3;
	    }
	}
	ss += strlen(ss);
	*ss++ = ' ';
	for (j = 0; j < DUMP_WIDTH; j++) {
	    if (((i * DUMP_WIDTH) + j) >= len)
		break;
	    ch = ((unsigned char) *((char *) (s) + i * DUMP_WIDTH + j)) & 0xff;
	    *ss++ = (((ch >= ' ') && (ch <= '~')) ? ch : '.');
	    if (j == 7) *ss++ = ' ';
	}
	*ss = 0;
	/* 
	 * if this is the last call then update the ddt_dump thing so that
         * we will move the selection point in the debug window
         */
	msg_info("%s", buf);
	ret += strlen(buf);
    }
#ifdef TRUNCATE
    if (trunc > 0) {
	sprintf(buf, "%04x - <SPACES/NULS>\n", len + trunc);
	msg_info("%s", buf);
	ret += strlen(buf);
    }
#endif
    return (ret);
}



/* taken from OpenSSL apps/s_cb.c */

static long bio_dump_cb(BIO * bio, int cmd, const char *argp, int argi,
			long argl, long ret)
{
    if (!do_dump)
	return (ret);

    if (cmd == (BIO_CB_READ | BIO_CB_RETURN)) {
	msg_info("read from %08X [%08lX] (%d bytes => %ld (0x%X))",
		 (unsigned int)bio, (unsigned long)argp, argi,
		 ret, (unsigned int)ret);
	pfixtls_dump(argp, (int) ret);
	return (ret);
    } else if (cmd == (BIO_CB_WRITE | BIO_CB_RETURN)) {
	msg_info("write to %08X [%08lX] (%d bytes => %ld (0x%X))",
		 (unsigned int)bio, (unsigned long)argp, argi,
	 	 ret, (unsigned int)ret);
	pfixtls_dump(argp, (int) ret);
    }
    return (ret);
}


 /*
  * Callback to retrieve a session from the external session cache.
  */
static SSL_SESSION *get_session_cb(SSL *ssl, unsigned char *SessionID,
				  int length, int *copy)
{
    SSL_SESSION *session;
    char idstring[2 * ID_MAXLENGTH + 1];
    int n;
    int uselength;
    int hex_length;
    const char *session_hex;
    pfixtls_scache_info_t scache_info;
    unsigned char nibble, *data, *sess_data;

    if (length > ID_MAXLENGTH)
	uselength = ID_MAXLENGTH;	/* Limit length of ID */
    else
	uselength = length;

    for(n=0 ; n < uselength ; n++)
	sprintf(idstring + 2 * n, "%02x", SessionID[n]);
    if (var_smtpd_tls_loglevel >= 3)
	msg_info("Trying to reload Session from disc: %s", idstring);

    session = NULL;

    session_hex = dict_get(scache_db, idstring);
    if (session_hex) {
	hex_length = strlen(session_hex);
	data = (unsigned char *)mymalloc(hex_length / 2);
	if (!data) {
	    msg_info("could not allocate memory for session reload");
	    return(NULL);
	}

	memset(data, 0, hex_length / 2);
	for (n = 0; n < hex_length; n++) {
	    if ((session_hex[n] >= '0') && (session_hex[n] <= '9'))
		nibble = session_hex[n] - '0';
	    else
		nibble = session_hex[n] - 'A' + 10;
	    if (n % 2)
		data[n / 2] |= nibble;
	    else
		data[n / 2] |= (nibble << 4);
	}

	/*
	 * First check the version numbers, since wrong session data might
	 * hit us hard (SEGFAULT). We also have to check for expiry.
	 */
	memcpy(&scache_info, data, sizeof(pfixtls_scache_info_t));
	if ((scache_info.scache_db_version != scache_db_version) ||
	    (scache_info.openssl_version != openssl_version) ||
	    (scache_info.timestamp + var_smtpd_tls_scache_timeout < time(NULL)))
	    dict_del(scache_db, idstring);
	else {
	    sess_data = data + sizeof(pfixtls_scache_info_t);
	    session = d2i_SSL_SESSION(NULL, &sess_data,
			      hex_length / 2 - sizeof(pfixtls_scache_info_t));
	    if (!session)
		pfixtls_print_errors();
	}
	myfree((char *)data);
    }

    if (session && (var_smtpd_tls_loglevel >= 3))
	msg_info("Successfully reloaded session from disc");

    return (session);
}


static SSL_SESSION *load_clnt_session(const char *hostname,
				      int enforce_peername)
{
    SSL_SESSION *session = NULL;
    char idstring[ID_MAXLENGTH + 1];
    int n;
    int uselength;
    int length;
    int hex_length;
    const char *session_hex;
    pfixtls_scache_info_t scache_info;
    unsigned char nibble, *data, *sess_data;

    length = strlen(hostname); 
    if (length > ID_MAXLENGTH)
	uselength = ID_MAXLENGTH;	/* Limit length of ID */
    else
	uselength = length;

    for(n=0 ; n < uselength ; n++)
	idstring[n] = tolower(hostname[n]);
    idstring[uselength] = '\0';
    if (var_smtp_tls_loglevel >= 3)
	msg_info("Trying to reload Session from disc: %s", idstring);

    session_hex = dict_get(scache_db, idstring);
    if (session_hex) {
	hex_length = strlen(session_hex);
	data = (unsigned char *)mymalloc(hex_length / 2);
	if (!data) {
	    msg_info("could not allocate memory for session reload");
	    return(NULL);
	}

	memset(data, 0, hex_length / 2);
	for (n = 0; n < hex_length; n++) {
	    if ((session_hex[n] >= '0') && (session_hex[n] <= '9'))
		nibble = session_hex[n] - '0';
	    else
		nibble = session_hex[n] - 'A' + 10;
	    if (n % 2)
		data[n / 2] |= nibble;
	    else
		data[n / 2] |= (nibble << 4);
	}

	/*
	 * First check the version numbers, since wrong session data might
	 * hit us hard (SEGFAULT). We also have to check for expiry.
	 * When we enforce_peername, we may find an old session, that was
	 * saved when enforcement was not set. In this case the session will
	 * be removed and a fresh session will be negotiated.
	 */
	memcpy(&scache_info, data, sizeof(pfixtls_scache_info_t));
	if ((scache_info.scache_db_version != scache_db_version) ||
	    (scache_info.openssl_version != openssl_version) ||
	    (scache_info.timestamp + var_smtpd_tls_scache_timeout < time(NULL)))
	    dict_del(scache_db, idstring);
	else if (enforce_peername && (!scache_info.enforce_peername))
	    dict_del(scache_db, idstring);
	else {
	    sess_data = data + sizeof(pfixtls_scache_info_t);
	    session = d2i_SSL_SESSION(NULL, &sess_data,
				      hex_length / 2 - sizeof(time_t));
	    strncpy(SSL_SESSION_get_ex_data(session, TLSpeername_index),
		    idstring, ID_MAXLENGTH + 1);
	    if (!session)
		pfixtls_print_errors();
	}
	myfree((char *)data);
    }

    if (session && (var_smtp_tls_loglevel >= 3))
        msg_info("Successfully reloaded session from disc");

    return (session);
}


static void create_client_lookup_id(char *idstring, char *hostname)
{
    int n, len, uselength;

    len = strlen(hostname);
    if (len > ID_MAXLENGTH)
	uselength = ID_MAXLENGTH;	/* Limit length of ID */
    else
	uselength = len;

    for (n = 0 ; n < uselength ; n++)
	idstring[n] = tolower(hostname[n]);
    idstring[uselength] = '\0';
}


static void create_server_lookup_id(char *idstring, SSL_SESSION *session)
{
    int n, uselength;

    if (session->session_id_length > ID_MAXLENGTH)
	uselength = ID_MAXLENGTH;	/* Limit length of ID */
    else
	uselength = session->session_id_length;

    for(n = 0; n < uselength ; n++)
	sprintf(idstring + 2 * n, "%02x", session->session_id[n]);
}


static void remove_session_cb(SSL_CTX *ctx, SSL_SESSION *session)
{
    char idstring[2 * ID_MAXLENGTH + 1];
    char *hostname;

    if (pfixtls_clientengine) {
        hostname = SSL_SESSION_get_ex_data(session, TLSpeername_index);
	create_client_lookup_id(idstring, hostname);
	if (var_smtp_tls_loglevel >= 3)
	    msg_info("Trying to remove session from disc: %s", idstring);
    }
    else {
	create_server_lookup_id(idstring, session);
	if (var_smtpd_tls_loglevel >= 3)
	    msg_info("Trying to remove session from disc: %s", idstring);
    }

    if (scache_db)
	dict_del(scache_db, idstring);
}


/*
 * We need space to save the peername into the SSL_SESSION, as we must
 * look up the external database for client sessions by peername, not
 * by session id. We therefore allocate place for the peername string,
 * when a new SSL_SESSION is generated. It is filled later.
 */
static int new_peername_func(void *parent, void *ptr, CRYPTO_EX_DATA *ad,
			     int idx, long argl, void *argp)
{
    char *peername;

    peername = (char *)mymalloc(ID_MAXLENGTH + 1);
    if (!peername)
	return 0;
    peername[0] = '\0'; 	/* initialize */
    return CRYPTO_set_ex_data(ad, idx, peername);
}

/*
 * When the SSL_SESSION is removed again, we must free the memory to avoid
 * leaks.
 */
static void free_peername_func(void *parent, void *ptr, CRYPTO_EX_DATA *ad,
			       int idx, long argl, void *argp)
{
    myfree(CRYPTO_get_ex_data(ad, idx));
}

/*
 * Duplicate application data, when a SSL_SESSION is duplicated
 */
static int dup_peername_func(CRYPTO_EX_DATA *to, CRYPTO_EX_DATA *from,
			     void *from_d, int idx, long argl, void *argp)
{
    char *peername_old, *peername_new;

    peername_old = CRYPTO_get_ex_data(from, idx);
    peername_new = CRYPTO_get_ex_data(to, idx);
    if (!peername_old || !peername_new)
	return 0;
    memcpy(peername_new, peername_old, ID_MAXLENGTH + 1);
    return 1;
}


 /*
  * Save a new session to the external cache
  */
static int new_session_cb(SSL *ssl, SSL_SESSION *session)
{
    char idstring[2 * ID_MAXLENGTH + 1];
    int n;
    int dsize;
    int len;
    unsigned char *data, *sess_data;
    pfixtls_scache_info_t scache_info;
    char *hexdata, *hostname;
    TLScontext_t *TLScontext;

    if (pfixtls_clientengine) {
        TLScontext = SSL_get_ex_data(ssl, TLScontext_index);
	hostname = TLScontext->peername_save;
	create_client_lookup_id(idstring, hostname);
	strncpy(SSL_SESSION_get_ex_data(session, TLSpeername_index),
		hostname, ID_MAXLENGTH + 1);
	/*
	 * Remember, whether peername matching was enforced when the session
	 * was created. If later enforce mode is enabled, we do not want to
	 * reuse a session that was not sufficiently checked.
	 */
	scache_info.enforce_peername =
		(TLScontext->enforce_verify_errors && TLScontext->enforce_CN);

	if (var_smtp_tls_loglevel >= 3)
	    msg_info("Trying to save session for hostID to disc: %s", idstring);

#if (OPENSSL_VERSION_NUMBER < 0x00906011L) || (OPENSSL_VERSION_NUMBER == 0x00907000L)
	    /*
	     * Ugly Hack: OpenSSL before 0.9.6a does not store the verify
	     * result in sessions for the client side.
	     * We modify the session directly which is version specific,
	     * but this bug is version specific, too.
	     *
	     * READ: 0-09-06-01-1 = 0-9-6-a-beta1: all versions before
	     * beta1 have this bug, it has been fixed during development
	     * of 0.9.6a. The development version of 0.9.7 can have this
	     * bug, too. It has been fixed on 2000/11/29.
	     */
	    session->verify_result = SSL_get_verify_result(TLScontext->con);
#endif

    }
    else {
	create_server_lookup_id(idstring, session);
	if (var_smtpd_tls_loglevel >= 3)
	    msg_info("Trying to save Session to disc: %s", idstring);
    }


    /*
     * Get the session and convert it into some "database" useable form.
     * First, get the length of the session to allocate the memory.
     */
    dsize = i2d_SSL_SESSION(session, NULL);
    if (dsize < 0) {
	msg_info("Could not access session");
	return 0;
    }
    data = (unsigned char *)mymalloc(dsize + sizeof(pfixtls_scache_info_t));
    if (!data) {
	msg_info("could not allocate memory for SSL session");
	return 0;
    }

    /*
     * OpenSSL is not robust against wrong session data (might SEGFAULT),
     * so we secure it against version ids (session cache structure as well
     * as OpenSSL version).
     */
    scache_info.scache_db_version = scache_db_version;
    scache_info.openssl_version = openssl_version;

    /*
     * Put a timestamp, so that expiration can be checked without
     * analyzing the session data itself. (We would need OpenSSL funtions,
     * since the SSL_SESSION is a private structure.)
     */
    scache_info.timestamp = time(NULL);

    memcpy(data, &scache_info, sizeof(pfixtls_scache_info_t));
    sess_data = data + sizeof(pfixtls_scache_info_t);

    /*
     * Now, obtain the session. Unfortunately, it is binary and dict_update
     * cannot handle binary data (it could contain '\0' in it) directly.
     * To save memory we could use base64 encoding. To make handling easier,
     * we simply use hex format.
     */
    len = i2d_SSL_SESSION(session, &sess_data);
    len += sizeof(pfixtls_scache_info_t);

    hexdata = (char *)mymalloc(2 * len + 1);

    if (!hexdata) {
	msg_info("could not allocate memory for SSL session (HEX)");
	myfree((char *)data);
	return 0;
    }
    for (n = 0; n < len; n++) {
	hexdata[n * 2] = hexcodes[(data[n] & 0xf0) >> 4];
	hexdata[(n * 2) + 1] = hexcodes[(data[n] & 0x0f)];
    }
    hexdata[len * 2] = '\0';

    /*
     * The session id is a hex string, all uppercase. We are using SDBM as
     * compiled into Postfix with 8kB maximum entry size, so we set a limit
     * when caching. If the session is not cached, we have to renegotiate,
     * not more, not less. For a real session, this limit should never be
     * met
     */
    if (strlen(idstring) + strlen(hexdata) < 8000)
      dict_put(scache_db, idstring, hexdata);

    myfree(hexdata);
    myfree((char *)data);
    return (1);
}


 /*
  * pfixtls_exchange_seed: read bytes from the seed exchange-file (expect
  * 1024 bytes)and immediately write back random bytes. Do so with EXCLUSIVE
  * lock, so * that each process will find a completely different (and
  * reseeded) file.
  */
static void pfixtls_exchange_seed(void)
{
    unsigned char buffer[1024];

    if (rand_exch_fd == -1)
	return;

    if (myflock(rand_exch_fd, INTERNAL_LOCK, MYFLOCK_OP_EXCLUSIVE) != 0)
        msg_info("Could not lock random exchange file: %s",
                  strerror(errno));

    lseek(rand_exch_fd, 0, SEEK_SET);
    if (read(rand_exch_fd, buffer, 1024) < 0)
        msg_fatal("reading exchange file failed");
    RAND_seed(buffer, 1024);

    RAND_bytes(buffer, 1024);
    lseek(rand_exch_fd, 0, SEEK_SET);
    if (write(rand_exch_fd, buffer, 1024) != 1024)
        msg_fatal("Writing exchange file failed");

    if (myflock(rand_exch_fd, INTERNAL_LOCK, MYFLOCK_OP_NONE) != 0)
        msg_fatal("Could not unlock random exchange file: %s",
                  strerror(errno));
}

 /*
  * This is the setup routine for the SSL server. As smtpd might be called
  * more than once, we only want to do the initialization one time.
  *
  * The skeleton of this function is taken from OpenSSL apps/s_server.c.
  */

int     pfixtls_init_serverengine(int verifydepth, int askcert)
{
    int     off = 0;
    int     verify_flags = SSL_VERIFY_NONE;
    int     rand_bytes;
    int     rand_source_dev_fd;
    int     rand_source_socket_fd;
    unsigned char buffer[255];
    char   *CApath;
    char   *CAfile;
    char   *s_cert_file;
    char   *s_key_file;
    char   *s_dcert_file;
    char   *s_dkey_file;
    FILE   *paramfile;

    if (pfixtls_serverengine)
	return (0);				/* already running */

    if (var_smtpd_tls_loglevel >= 2)
	msg_info("starting TLS engine");

    /*
     * Initialize the OpenSSL library by the book!
     * To start with, we must initialize the algorithms.
     * We want cleartext error messages instead of just error codes, so we
     * load the error_strings.
     */
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();

 /*
  * Side effect, call a non-existing function to disable TLS usage with an
  * outdated OpenSSL version. There is a security reason (verify_result
  * is not stored with the session data).
  */
#if (OPENSSL_VERSION_NUMBER < 0x00905100L)
    needs_openssl_095_or_later();
#endif

    /*
     * Initialize the PRNG Pseudo Random Number Generator with some seed.
     */
    randseed.pid = getpid();
    GETTIMEOFDAY(&randseed.tv);
    RAND_seed(&randseed, sizeof(randseed_t));

    /*
     * Access the external sources for random seed. We will only query them
     * once, this should be sufficient and we will stir our entropy by using
     * the prng-exchange file anyway.
     * For reliability, we don't consider failure to access the additional
     * source fatal, as we can run happily without it (considering that we
     * still have the exchange-file). We also don't care how much entropy
     * we get back, as we must run anyway. We simply stir in the buffer
     * regardless how many bytes are actually in it.
     */
    if (*var_tls_daemon_rand_source) {
	if (!strncmp(var_tls_daemon_rand_source, "dev:", 4)) {
	    /*
	     * Source is a random device
	     */
	    rand_source_dev_fd = open(var_tls_daemon_rand_source + 4, 0, 0);
	    if (rand_source_dev_fd == -1) 
		msg_info("Could not open entropy device %s",
			  var_tls_daemon_rand_source);
	    else {
		if (var_tls_daemon_rand_bytes > 255)
		    var_tls_daemon_rand_bytes = 255;
	        read(rand_source_dev_fd, buffer, var_tls_daemon_rand_bytes);
		RAND_seed(buffer, var_tls_daemon_rand_bytes);
		close(rand_source_dev_fd);
	    }
	} else if (!strncmp(var_tls_daemon_rand_source, "egd:", 4)) {
	    /*
	     * Source is a EGD compatible socket
	     */
	    rand_source_socket_fd = unix_connect(var_tls_daemon_rand_source +4,
						 BLOCKING, 10);
	    if (rand_source_socket_fd == -1)
		msg_info("Could not connect to %s", var_tls_daemon_rand_source);
	    else {
		if (var_tls_daemon_rand_bytes > 255)
		    var_tls_daemon_rand_bytes = 255;
		buffer[0] = 1;
		buffer[1] = var_tls_daemon_rand_bytes;
		if (write(rand_source_socket_fd, buffer, 2) != 2)
		    msg_info("Could not talk to %s",
			     var_tls_daemon_rand_source);
		else if (read(rand_source_socket_fd, buffer, 1) != 1)
		    msg_info("Could not read info from %s",
			     var_tls_daemon_rand_source);
		else {
		    rand_bytes = buffer[0];
		    read(rand_source_socket_fd, buffer, rand_bytes);
		    RAND_seed(buffer, rand_bytes);
		}
		close(rand_source_socket_fd);
	    }
	} else {
	    RAND_load_file(var_tls_daemon_rand_source,
			   var_tls_daemon_rand_bytes);
	}
    }

    if (*var_tls_rand_exch_name) {
	rand_exch_fd = open(var_tls_rand_exch_name, O_RDWR | O_CREAT, 0600);
	if (rand_exch_fd != -1)
	    pfixtls_exchange_seed();
    }

    randseed.pid = getpid();
    GETTIMEOFDAY(&randseed.tv);
    RAND_seed(&randseed, sizeof(randseed_t));

    /*
     * The SSL/TLS speficications require the client to send a message in
     * the oldest specification it understands with the highest level it
     * understands in the message.
     * Netscape communicator can still communicate with SSLv2 servers, so it
     * sends out a SSLv2 client hello. To deal with it, our server must be
     * SSLv2 aware (even if we don't like SSLv2), so we need to have the
     * SSLv23 server here. If we want to limit the protocol level, we can
     * add an option to not use SSLv2/v3/TLSv1 later.
     */
    ctx = SSL_CTX_new(SSLv23_server_method());
    if (ctx == NULL) {
	pfixtls_print_errors();
	return (-1);
    };

    /*
     * Here we might set SSL_OP_NO_SSLv2, SSL_OP_NO_SSLv3, SSL_OP_NO_TLSv1.
     * Of course, the last one would not make sense, since RFC2487 is only
     * defined for TLS, but we also want to accept Netscape communicator
     * requests, and it only supports SSLv3.
     */
    off |= SSL_OP_ALL;		/* Work around all known bugs */
    SSL_CTX_set_options(ctx, off);

    /*
     * Set the info_callback, that will print out messages during
     * communication on demand.
     */
    if (var_smtpd_tls_loglevel >= 2)
	SSL_CTX_set_info_callback(ctx, apps_ssl_info_callback);

    /*
     * Set the list of ciphers, if explicitely given; otherwise the
     * (reasonable) default list is kept.
     */
    if (strlen(var_smtpd_tls_cipherlist) != 0)
	if (SSL_CTX_set_cipher_list(ctx, var_smtpd_tls_cipherlist) == 0) {
	    pfixtls_print_errors();
	    return (-1);
	}

    /*
     * Now we must add the necessary certificate stuff: A server key, a
     * server certificate, and the CA certificates for both the server
     * cert and the verification of client certificates.
     * As provided by OpenSSL we support two types of CA certificate handling:
     * One possibility is to add all CA certificates to one large CAfile,
     * the other possibility is a directory pointed to by CApath, containing
     * seperate files for each CA pointed on by softlinks named by the hash
     * values of the certificate.
     * The first alternative has the advantage, that the file is opened and
     * read at startup time, so that you don't have the hassle to maintain
     * another copy of the CApath directory for chroot-jail. On the other
     * hand, the file is not really readable.
     */
    if (strlen(var_smtpd_tls_CAfile) == 0)
	CAfile = NULL;
    else
	CAfile = var_smtpd_tls_CAfile;
    if (strlen(var_smtpd_tls_CApath) == 0)
	CApath = NULL;
    else
	CApath = var_smtpd_tls_CApath;

    if (CAfile || CApath) {
	if (!SSL_CTX_load_verify_locations(ctx, CAfile, CApath)) {
	    msg_info("TLS engine: cannot load CA data");
	    pfixtls_print_errors();
	    return (-1);
	}
	if (!SSL_CTX_set_default_verify_paths(ctx)) {
	    msg_info("TLS engine: cannot set verify paths");
	    pfixtls_print_errors();
	    return (-1);
	}
    }

    /*
     * Now we load the certificate and key from the files and check,
     * whether the cert matches the key (internally done by set_cert_stuff().
     * We cannot run without (we do not support ADH anonymous Diffie-Hellman
     * ciphers as of now).
     * We can use RSA certificates ("cert") and DSA certificates ("dcert"),
     * both can be made available at the same time. The CA certificates for
     * both are handled in the same setup already finished.
     * Which one is used depends on the cipher negotiated (that is: the first
     * cipher listed by the client which does match the server). A client with
     * RSA only (e.g. Netscape) will use the RSA certificate only.
     * A client with openssl-library will use RSA first if not especially
     * changed in the cipher setup.
     */
    if (strlen(var_smtpd_tls_cert_file) == 0)
	s_cert_file = NULL;
    else
	s_cert_file = var_smtpd_tls_cert_file;
    if (strlen(var_smtpd_tls_key_file) == 0)
	s_key_file = NULL;
    else
	s_key_file = var_smtpd_tls_key_file;

    if (strlen(var_smtpd_tls_dcert_file) == 0)
	s_dcert_file = NULL;
    else
	s_dcert_file = var_smtpd_tls_dcert_file;
    if (strlen(var_smtpd_tls_dkey_file) == 0)
	s_dkey_file = NULL;
    else
	s_dkey_file = var_smtpd_tls_dkey_file;

    if (s_cert_file) {
	if (!set_cert_stuff(ctx, s_cert_file, s_key_file)) {
	    msg_info("TLS engine: cannot load RSA cert/key data");
	    pfixtls_print_errors();
	    return (-1);
	}
    }
    if (s_dcert_file) {
	if (!set_cert_stuff(ctx, s_dcert_file, s_dkey_file)) {
	    msg_info("TLS engine: cannot load DSA cert/key data");
	    pfixtls_print_errors();
	    return (-1);
	}
    }
    if (!s_cert_file && !s_dcert_file) {
	msg_info("TLS engine: do need at least RSA _or_ DSA cert/key data");
	return (-1);
    }

    /*
     * Sometimes a temporary RSA key might be needed by the OpenSSL
     * library. The OpenSSL doc indicates, that this might happen when
     * export ciphers are in use. We have to provide one, so well, we
     * just do it.
     */
    SSL_CTX_set_tmp_rsa_callback(ctx, tmp_rsa_cb);

    /*
     * We might also need dh parameters, which can either be loaded from
     * file (preferred) or we simply take the compiled in values.
     * First, set the callback that will select the values when requested,
     * then load the (possibly) available DH parameters from files.
     * We are generous with the error handling, since we do have default
     * values compiled in, so we will not abort but just log the error message.
     */
    SSL_CTX_set_tmp_dh_callback(ctx, tmp_dh_cb);
    if (strlen(var_smtpd_tls_dh1024_param_file) != 0) {
	if ((paramfile = fopen(var_smtpd_tls_dh1024_param_file, "r")) != NULL) {
	    dh_1024 = PEM_read_DHparams(paramfile, NULL, NULL, NULL);
	    if (dh_1024 == NULL) {
		msg_info("TLS engine: cannot load 1024bit DH parameters");
		pfixtls_print_errors();
	    }
	}
	else {
	    msg_info("TLS engine: cannot load 1024bit DH parameters: %s: %s",
		     var_smtpd_tls_dh1024_param_file, strerror(errno));
	}
    }
    if (strlen(var_smtpd_tls_dh512_param_file) != 0) {
	if ((paramfile = fopen(var_smtpd_tls_dh512_param_file, "r")) != NULL) {
	    dh_512 = PEM_read_DHparams(paramfile, NULL, NULL, NULL);
	    if (dh_512 == NULL) {
		msg_info("TLS engine: cannot load 512bit DH parameters");
		pfixtls_print_errors();
	    }
	}
	else {
	    msg_info("TLS engine: cannot load 512bit DH parameters: %s: %s",
		     var_smtpd_tls_dh512_param_file, strerror(errno));
	}
    }

    /*
     * If we want to check client certificates, we have to indicate it
     * in advance. By now we only allow to decide on a global basis.
     * If we want to allow certificate based relaying, we must ask the
     * client to provide one with SSL_VERIFY_PEER. The client now can
     * decide, whether it provides one or not. We can enforce a failure
     * of the negotiation with SSL_VERIFY_FAIL_IF_NO_PEER_CERT, if we
     * do not allow a connection without one.
     * In the "server hello" following the initialization by the "client hello"
     * the server must provide a list of CAs it is willing to accept.
     * Some clever clients will then select one from the list of available
     * certificates matching these CAs. Netscape Communicator will present
     * the list of certificates for selecting the one to be sent, or it will
     * issue a warning, if there is no certificate matching the available
     * CAs.
     *
     * With regard to the purpose of the certificate for relaying, we might
     * like a later negotiation, maybe relaying would already be allowed
     * for other reasons, but this would involve severe changes in the
     * internal postfix logic, so we have to live with it the way it is.
     */
    if (askcert)
	verify_flags = SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE;
    SSL_CTX_set_verify(ctx, verify_flags, verify_callback);
    SSL_CTX_set_client_CA_list(ctx, SSL_load_client_CA_file(CAfile));

    /*
     * Initialize the session cache. We only want external caching to
     * synchronize between server sessions, so we set it to a minimum value
     * of 1. If the external cache is disabled, we won't cache at all.
     * The recall of old sessions "get" and save to disk of just created
     * sessions "new" is handled by the appropriate callback functions.
     *
     * We must not forget to set a session id context to identify to which
     * kind of server process the session was related. In our case, the
     * context is just the name of the patchkit: "Postfix/TLS".
     */
    SSL_CTX_sess_set_cache_size(ctx, 1);
    SSL_CTX_set_timeout(ctx, var_smtpd_tls_scache_timeout);
    SSL_CTX_set_session_id_context(ctx, (void*)&server_session_id_context,
                sizeof(server_session_id_context));

    /*
     * The session cache is realized by an external database file, that
     * must be opened before going to chroot jail. Since the session cache
     * data can become quite large, "[n]dbm" cannot be used as it has a
     * size limit that is by far to small.
     */
    if (*var_smtpd_tls_scache_db) {
	/*
	 * Insert a test against other dbms here, otherwise while writing
	 * a session (content to large), we will receive a fatal error!
	 */
	if (strncmp(var_smtpd_tls_scache_db, "sdbm:", 5))
	    msg_warn("Only sdbm: type allowed for %s",
		     var_smtpd_tls_scache_db);
	else
	    scache_db = dict_open(var_smtpd_tls_scache_db, O_RDWR,
	      DICT_FLAG_DUP_REPLACE | DICT_FLAG_LOCK | DICT_FLAG_SYNC_UPDATE);
	if (scache_db) {
	    SSL_CTX_set_session_cache_mode(ctx,
			SSL_SESS_CACHE_SERVER|SSL_SESS_CACHE_NO_AUTO_CLEAR);
	    SSL_CTX_sess_set_get_cb(ctx, get_session_cb);
	    SSL_CTX_sess_set_new_cb(ctx, new_session_cb);
	    SSL_CTX_sess_set_remove_cb(ctx, remove_session_cb);
	}
	else
	    msg_warn("Could not open session cache %s",
		     var_smtpd_tls_scache_db);
    }

    /*
     * Finally create the global index to access TLScontext information
     * inside verify_callback.
     */
    TLScontext_index = SSL_get_ex_new_index(0, "TLScontext ex_data index",
					    NULL, NULL, NULL);

    pfixtls_serverengine = 1;
    return (0);
}

 /*
  * This is the actual startup routine for the connection. We expect
  * that the buffers are flushed and the "220 Ready to start TLS" was
  * send to the client, so that we can immediately can start the TLS
  * handshake process.
  */
int     pfixtls_start_servertls(VSTREAM *stream, int timeout,
				const char *peername, const char *peeraddr,
				tls_info_t *tls_info, int requirecert)
{
    int     sts;
    int     j;
    int verify_flags;
    unsigned int n;
    TLScontext_t *TLScontext;
    SSL_SESSION *session;
    SSL_CIPHER *cipher;
    X509   *peer;

    if (!pfixtls_serverengine) {		/* should never happen */
	msg_info("tls_engine not running");
	return (-1);
    }
    if (var_smtpd_tls_loglevel >= 1)
	msg_info("setting up TLS connection from %s[%s]", peername, peeraddr);

    /*
     * Allocate a new TLScontext for the new connection and get an SSL
     * structure. Add the location of TLScontext to the SSL to later
     * retrieve the information inside the verify_callback().
     */
    TLScontext = (TLScontext_t *)mymalloc(sizeof(TLScontext_t));
    if (!TLScontext) {
	msg_fatal("Could not allocate 'TLScontext' with mymalloc");
    }
    if ((TLScontext->con = (SSL *) SSL_new(ctx)) == NULL) {
	msg_info("Could not allocate 'TLScontext->con' with SSL_new()");
	pfixtls_print_errors();
	myfree((char *)TLScontext);
	return (-1);
    }
    if (!SSL_set_ex_data(TLScontext->con, TLScontext_index, TLScontext)) {
	msg_info("Could not set application data for 'TLScontext->con'");
	pfixtls_print_errors();
	SSL_free(TLScontext->con);
	myfree((char *)TLScontext);
	return (-1);
    }

    /*
     * Set the verification parameters to be checked in verify_callback().
     */
    if (requirecert) {
	verify_flags = SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE;
	verify_flags |= SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
	TLScontext->enforce_verify_errors = 1;
        SSL_set_verify(TLScontext->con, verify_flags, verify_callback);
    }
    else {
	TLScontext->enforce_verify_errors = 0;
    }
    TLScontext->enforce_CN = 0;

    /*
     * The TLS connection is realized by a BIO_pair, so obtain the pair.
     */
    if (!BIO_new_bio_pair(&TLScontext->internal_bio, BIO_bufsiz,
			  &TLScontext->network_bio, BIO_bufsiz)) {
	msg_info("Could not obtain BIO_pair");
	pfixtls_print_errors();
	SSL_free(TLScontext->con);
	myfree((char *)TLScontext);
	return (-1);
    }

    /*
     * Before really starting anything, try to seed the PRNG a little bit
     * more.
     */
    pfixtls_stir_seed();
    pfixtls_exchange_seed();

    /*
     * Initialize the SSL connection to accept state. This should not be
     * necessary anymore since 0.9.3, but the call is still in the library
     * and maintaining compatibility never hurts.
     */
    SSL_set_accept_state(TLScontext->con);

    /*
     * Connect the SSL-connection with the postfix side of the BIO-pair for
     * reading and writing.
     */
     SSL_set_bio(TLScontext->con, TLScontext->internal_bio,
		 TLScontext->internal_bio);

    /*
     * If the debug level selected is high enough, all of the data is
     * dumped: 3 will dump the SSL negotiation, 4 will dump everything.
     *
     * We do have an SSL_set_fd() and now suddenly a BIO_ routine is called?
     * Well there is a BIO below the SSL routines that is automatically
     * created for us, so we can use it for debugging purposes.
     */
    if (var_smtpd_tls_loglevel >= 3)
	BIO_set_callback(SSL_get_rbio(TLScontext->con), bio_dump_cb);


    /* Dump the negotiation for loglevels 3 and 4 */
    if (var_smtpd_tls_loglevel >= 3)
	do_dump = 1;

    /*
     * Now we expect the negotiation to begin. This whole process is like a
     * black box for us. We totally have to rely on the routines build into
     * the OpenSSL library. The only thing we can do we already have done
     * by choosing our own callbacks for session caching and certificate
     * verification.
     *
     * Error handling:
     * If the SSL handhake fails, we print out an error message and remove
     * everything that might be there. A session has to be removed anyway,
     * because RFC2246 requires it.
     */
    sts = do_tls_operation(vstream_fileno(stream), timeout, TLScontext,
			   SSL_accept, NULL, NULL, NULL, 0);
    if (sts <= 0) {
	msg_info("SSL_accept error from %s[%s]: %d", peername, peeraddr, sts);
	pfixtls_print_errors();
	SSL_free(TLScontext->con);
	myfree((char *)TLScontext);
	return (-1);
    }

    /* Only loglevel==4 dumps everything */
    if (var_smtpd_tls_loglevel < 4)
	do_dump = 0;

    /*
     * Lets see, whether a peer certificate is available and what is
     * the actual information. We want to save it for later use.
     */
    peer = SSL_get_peer_certificate(TLScontext->con);
    if (peer != NULL) {
	if (SSL_get_verify_result(TLScontext->con) == X509_V_OK)
	    tls_info->peer_verified = 1;

	X509_NAME_oneline(X509_get_subject_name(peer),
			  TLScontext->peer_subject, CCERT_BUFSIZ);
	if (var_smtpd_tls_loglevel >= 2)
	    msg_info("subject=%s", TLScontext->peer_subject);
	tls_info->peer_subject = TLScontext->peer_subject;
	X509_NAME_oneline(X509_get_issuer_name(peer),
			  TLScontext->peer_issuer, CCERT_BUFSIZ);
	if (var_smtpd_tls_loglevel >= 2)
	    msg_info("issuer=%s", TLScontext->peer_issuer);
	tls_info->peer_issuer = TLScontext->peer_issuer;
	if (X509_digest(peer, EVP_md5(), TLScontext->md, &n)) {
	    for (j = 0; j < (int) n; j++) {
		TLScontext->fingerprint[j * 3] =
			hexcodes[(TLScontext->md[j] & 0xf0) >> 4];
		TLScontext->fingerprint[(j * 3) + 1] =
			hexcodes[(TLScontext->md[j] & 0x0f)];
		if (j + 1 != (int) n)
		    TLScontext->fingerprint[(j * 3) + 2] = ':';
		else
		    TLScontext->fingerprint[(j * 3) + 2] = '\0';
	    }
	    if (var_smtpd_tls_loglevel >= 1)
		msg_info("fingerprint=%s", TLScontext->fingerprint);
	    tls_info->peer_fingerprint = TLScontext->fingerprint;
	}

	TLScontext->peer_CN[0] = '\0';
	if (!X509_NAME_get_text_by_NID(X509_get_subject_name(peer),
			NID_commonName, TLScontext->peer_CN, CCERT_BUFSIZ)) {
	    msg_info("Could not parse client's subject CN");
	    pfixtls_print_errors();
	}
	tls_info->peer_CN = TLScontext->peer_CN;

	TLScontext->issuer_CN[0] = '\0';
	if (!X509_NAME_get_text_by_NID(X509_get_issuer_name(peer),
			NID_commonName, TLScontext->issuer_CN, CCERT_BUFSIZ)) {
	    msg_info("Could not parse client's issuer CN");
	    pfixtls_print_errors();
	}
	if (!TLScontext->issuer_CN[0]) {
	    /* No issuer CN field, use Organization instead */
	    if (!X509_NAME_get_text_by_NID(X509_get_issuer_name(peer),
		NID_organizationName, TLScontext->issuer_CN, CCERT_BUFSIZ)) {
		msg_info("Could not parse client's issuer Organization");
		pfixtls_print_errors();
	    }
	}
	tls_info->issuer_CN = TLScontext->issuer_CN;

	if (var_smtpd_tls_loglevel >= 1) {
	    if (tls_info->peer_verified)
		msg_info("Verified: subject_CN=%s, issuer=%s",
			 TLScontext->peer_CN, TLScontext->issuer_CN);
	    else
		msg_info("Unverified: subject_CN=%s, issuer=%s",
			 TLScontext->peer_CN, TLScontext->issuer_CN);
	}

	X509_free(peer);
    }

    /*
     * At this point we should have a certificate when required.
     * We may however have a cached session, so the callback would never
     * be called. We therefore double-check to make sure and remove the
     * session, if applicable.
     */
    if (requirecert) {
	if (!tls_info->peer_verified || !tls_info->peer_CN) {
	    msg_info("Re-used session without peer certificate removed");
	    session = SSL_get_session(TLScontext->con);
	    SSL_CTX_remove_session(ctx, session);
	    return (-1);
	}
    }

    /*
     * Finally, collect information about protocol and cipher for logging
     */
    tls_info->protocol = SSL_get_version(TLScontext->con);
    cipher = SSL_get_current_cipher(TLScontext->con);
    tls_info->cipher_name = SSL_CIPHER_get_name(cipher);
    tls_info->cipher_usebits = SSL_CIPHER_get_bits(cipher,
						 &(tls_info->cipher_algbits));

    pfixtls_serveractive = 1;

    /*
     * The TLS engine is active, switch to the pfixtls_timed_read/write()
     * functions and store the context.
     */
    vstream_control(stream,
		    VSTREAM_CTL_READ_FN, pfixtls_timed_read,
		    VSTREAM_CTL_WRITE_FN, pfixtls_timed_write,
		    VSTREAM_CTL_CONTEXT, (void *)TLScontext,
		    VSTREAM_CTL_END);

    if (var_smtpd_tls_loglevel >= 1)
   	 msg_info("TLS connection established from %s[%s]: %s with cipher %s (%d/%d bits)",
		  peername, peeraddr,
		  tls_info->protocol, tls_info->cipher_name,
		  tls_info->cipher_usebits, tls_info->cipher_algbits);
    pfixtls_stir_seed();

    return (0);
}

 /*
  * Shut down the TLS connection, that does mean: remove all the information
  * and reset the flags! This is needed if the actual running smtpd is to
  * be restarted. We do not give back any value, as there is nothing to
  * be reported.
  * Since our session cache is external, we will remove the session from
  * memory in any case. The SSL_CTX_flush_sessions might be redundant here,
  * I however want to make sure nothing is left.
  * RFC2246 requires us to remove sessions if something went wrong, as
  * indicated by the "failure" value, so we remove it from the external
  * cache, too. 
  */
int     pfixtls_stop_servertls(VSTREAM *stream, int timeout, int failure,
			       tls_info_t *tls_info)
{
    TLScontext_t *TLScontext;
    int retval;

    if (pfixtls_serveractive) {
	TLScontext = (TLScontext_t *)vstream_context(stream);
	/*
	 * Perform SSL_shutdown() twice, as the first attempt may return
	 * to early: it will only send out the shutdown alert but it will
	 * not wait for the peer's shutdown alert. Therefore, when we are
	 * the first party to send the alert, we must call SSL_shutdown()
	 * again.
	 * On failure we don't want to resume the session, so we will not
	 * perform SSL_shutdown() and the session will be removed as being
	 * bad.
	 */
	if (!failure) {
            retval = do_tls_operation(vstream_fileno(stream), timeout,
				TLScontext, SSL_shutdown, NULL, NULL, NULL, 0);
	    if (retval == 0)
		do_tls_operation(vstream_fileno(stream), timeout, TLScontext,
				SSL_shutdown, NULL, NULL, NULL, 0);
	}
	/*
	 * Free the SSL structure and the BIOs. Warning: the internal_bio is
	 * connected to the SSL structure and is automatically freed with
	 * it. Do not free it again (core dump)!!
	 * Only free the network_bio.
	 */
	SSL_free(TLScontext->con);
	BIO_free(TLScontext->network_bio);
	myfree((char *)TLScontext);
        vstream_control(stream,
		    VSTREAM_CTL_READ_FN, (VSTREAM_FN) NULL,
		    VSTREAM_CTL_WRITE_FN, (VSTREAM_FN) NULL,
		    VSTREAM_CTL_CONTEXT, (void *) NULL,
		    VSTREAM_CTL_END);
	SSL_CTX_flush_sessions(ctx, time(NULL));

	pfixtls_stir_seed();
	pfixtls_exchange_seed();

	*tls_info = tls_info_zero;
	pfixtls_serveractive = 0;

    }

    return (0);
}


 /*
  * This is the setup routine for the SSL client. As smtpd might be called
  * more than once, we only want to do the initialization one time.
  *
  * The skeleton of this function is taken from OpenSSL apps/s_client.c.
  */

int     pfixtls_init_clientengine(int verifydepth)
{
    int     off = 0;
    int     verify_flags = SSL_VERIFY_NONE;
    int     rand_bytes;
    int     rand_source_dev_fd;
    int     rand_source_socket_fd;
    unsigned char buffer[255];
    char   *CApath;
    char   *CAfile;
    char   *c_cert_file;
    char   *c_key_file;


    if (pfixtls_clientengine)
	return (0);				/* already running */

    if (var_smtp_tls_loglevel >= 2)
	msg_info("starting TLS engine");

    /*
     * Initialize the OpenSSL library by the book!
     * To start with, we must initialize the algorithms.
     * We want cleartext error messages instead of just error codes, so we
     * load the error_strings.
     */ 
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();

 /*
  * Side effect, call a non-existing function to disable TLS usage with an
  * outdated OpenSSL version. There is a security reason (verify_result
  * is not stored with the session data).
  */
#if (OPENSSL_VERSION_NUMBER < 0x00905100L)
    needs_openssl_095_or_later();
#endif

    /*
     * Initialize the PRNG Pseudo Random Number Generator with some seed.
     */
    randseed.pid = getpid();
    GETTIMEOFDAY(&randseed.tv);
    RAND_seed(&randseed, sizeof(randseed_t));

    /*
     * Access the external sources for random seed. We will only query them
     * once, this should be sufficient and we will stir our entropy by using
     * the prng-exchange file anyway.
     * For reliability, we don't consider failure to access the additional
     * source fatal, as we can run happily without it (considering that we
     * still have the exchange-file). We also don't care how much entropy
     * we get back, as we must run anyway. We simply stir in the buffer
     * regardless how many bytes are actually in it.
     */
    if (*var_tls_daemon_rand_source) {
	if (!strncmp(var_tls_daemon_rand_source, "dev:", 4)) {
	    /*
	     * Source is a random device
	     */
	    rand_source_dev_fd = open(var_tls_daemon_rand_source + 4, 0, 0);
	    if (rand_source_dev_fd == -1) 
		msg_info("Could not open entropy device %s",
			  var_tls_daemon_rand_source);
	    else {
		if (var_tls_daemon_rand_bytes > 255)
		    var_tls_daemon_rand_bytes = 255;
	        read(rand_source_dev_fd, buffer, var_tls_daemon_rand_bytes);
		RAND_seed(buffer, var_tls_daemon_rand_bytes);
		close(rand_source_dev_fd);
	    }
	} else if (!strncmp(var_tls_daemon_rand_source, "egd:", 4)) {
	    /*
	     * Source is a EGD compatible socket
	     */
	    rand_source_socket_fd = unix_connect(var_tls_daemon_rand_source +4,
						 BLOCKING, 10);
	    if (rand_source_socket_fd == -1)
		msg_info("Could not connect to %s", var_tls_daemon_rand_source);
	    else {
		if (var_tls_daemon_rand_bytes > 255)
		    var_tls_daemon_rand_bytes = 255;
		buffer[0] = 1;
		buffer[1] = var_tls_daemon_rand_bytes;
		if (write(rand_source_socket_fd, buffer, 2) != 2)
		    msg_info("Could not talk to %s",
			     var_tls_daemon_rand_source);
		else if (read(rand_source_socket_fd, buffer, 1) != 1)
		    msg_info("Could not read info from %s",
			     var_tls_daemon_rand_source);
		else {
		    rand_bytes = buffer[0];
		    read(rand_source_socket_fd, buffer, rand_bytes);
		    RAND_seed(buffer, rand_bytes);
		}
		close(rand_source_socket_fd);
	    }
	} else {
	    RAND_load_file(var_tls_daemon_rand_source,
			   var_tls_daemon_rand_bytes);
	}
    }

    if (*var_tls_rand_exch_name) {
	rand_exch_fd = open(var_tls_rand_exch_name, O_RDWR | O_CREAT, 0600);
	if (rand_exch_fd != -1)
	    pfixtls_exchange_seed();
    }

    randseed.pid = getpid();
    GETTIMEOFDAY(&randseed.tv);
    RAND_seed(&randseed, sizeof(randseed_t));

    /*
     * The SSL/TLS speficications require the client to send a message in
     * the oldest specification it understands with the highest level it
     * understands in the message.
     * RFC2487 is only specified for TLSv1, but we want to be as compatible
     * as possible, so we will start off with a SSLv2 greeting allowing
     * the best we can offer: TLSv1.
     * We can restrict this with the options setting later, anyhow.
     */
    ctx = SSL_CTX_new(SSLv23_client_method());
    if (ctx == NULL) {
	pfixtls_print_errors();
	return (-1);
    };

    /*
     * Here we might set SSL_OP_NO_SSLv2, SSL_OP_NO_SSLv3, SSL_OP_NO_TLSv1.
     * Of course, the last one would not make sense, since RFC2487 is only
     * defined for TLS, but we don't know what is out there. So leave things
     * completely open, as of today.
     */
    off |= SSL_OP_ALL;		/* Work around all known bugs */
    SSL_CTX_set_options(ctx, off);

    /*
     * Set the info_callback, that will print out messages during
     * communication on demand.
     */
    if (var_smtp_tls_loglevel >= 2)
	SSL_CTX_set_info_callback(ctx, apps_ssl_info_callback);

    /*
     * Set the list of ciphers, if explicitely given; otherwise the
     * (reasonable) default list is kept.
     */
    if (strlen(var_smtp_tls_cipherlist) != 0)
	if (SSL_CTX_set_cipher_list(ctx, var_smtp_tls_cipherlist) == 0) {
	    pfixtls_print_errors();
	    return (-1);
	}

    /*
     * Now we must add the necessary certificate stuff: A client key, a
     * client certificate, and the CA certificates for both the client
     * cert and the verification of server certificates.
     * In fact, we do not need a client certificate,  so the certificates
     * are only loaded (and checked), if supplied. A clever client would
     * handle multiple client certificates and decide based on the list
     * of acceptable CAs, sent by the server, which certificate to submit.
     * OpenSSL does however not do this and also has no callback hoods to
     * easily realize it.
     *
     * As provided by OpenSSL we support two types of CA certificate handling:
     * One possibility is to add all CA certificates to one large CAfile,
     * the other possibility is a directory pointed to by CApath, containing
     * seperate files for each CA pointed on by softlinks named by the hash
     * values of the certificate.
     * The first alternative has the advantage, that the file is opened and
     * read at startup time, so that you don't have the hassle to maintain
     * another copy of the CApath directory for chroot-jail. On the other
     * hand, the file is not really readable.
     */ 
    if (strlen(var_smtp_tls_CAfile) == 0)
	CAfile = NULL;
    else
	CAfile = var_smtp_tls_CAfile;
    if (strlen(var_smtp_tls_CApath) == 0)
	CApath = NULL;
    else
	CApath = var_smtp_tls_CApath;
    if (CAfile || CApath) {
	if (!SSL_CTX_load_verify_locations(ctx, CAfile, CApath)) {
	    msg_info("TLS engine: cannot load CA data");
	    pfixtls_print_errors();
	    return (-1);
	}
	if (!SSL_CTX_set_default_verify_paths(ctx)) {
	    msg_info("TLS engine: cannot set verify paths");
	    pfixtls_print_errors();
	    return (-1);
	}
    }

    if (strlen(var_smtp_tls_cert_file) == 0)
	c_cert_file = NULL;
    else
	c_cert_file = var_smtp_tls_cert_file;
    if (strlen(var_smtp_tls_key_file) == 0)
	c_key_file = NULL;
    else
	c_key_file = var_smtp_tls_key_file;
    if (c_cert_file || c_key_file)
	if (!set_cert_stuff(ctx, c_cert_file, c_key_file)) {
	    msg_info("TLS engine: cannot load cert/key data");
	    pfixtls_print_errors();
	    return (-1);
	}

    /*
     * Sometimes a temporary RSA key might be needed by the OpenSSL
     * library. The OpenSSL doc indicates, that this might happen when
     * export ciphers are in use. We have to provide one, so well, we
     * just do it.
     */
    SSL_CTX_set_tmp_rsa_callback(ctx, tmp_rsa_cb);

    /*
     * Finally, the setup for the server certificate checking, done
     * "by the book".
     */
    SSL_CTX_set_verify(ctx, verify_flags, verify_callback);

    /*
     * Initialize the session cache. We only want external caching to
     * synchronize between server sessions, so we set it to a minimum value
     * of 1. If the external cache is disabled, we won't cache at all.
     *
     * In case of the client, there is no callback used in OpenSSL, so
     * we must call the session cache functions manually during the process.
     */
    SSL_CTX_sess_set_cache_size(ctx, 1);
    SSL_CTX_set_timeout(ctx, var_smtp_tls_scache_timeout);

    /*
     * The session cache is realized by an external database file, that
     * must be opened before going to chroot jail. Since the session cache
     * data can become quite large, "[n]dbm" cannot be used as it has a
     * size limit that is by far to small.
     */
    if (*var_smtp_tls_scache_db) {
	/*
	 * Insert a test against other dbms here, otherwise while writing
	 * a session (content to large), we will receive a fatal error!
	 */
	if (strncmp(var_smtp_tls_scache_db, "sdbm:", 5))
	    msg_warn("Only sdbm: type allowed for %s",
		     var_smtp_tls_scache_db);
	else
	    scache_db = dict_open(var_smtp_tls_scache_db, O_RDWR,
	      DICT_FLAG_DUP_REPLACE | DICT_FLAG_LOCK | DICT_FLAG_SYNC_UPDATE);
	if (!scache_db)
	    msg_warn("Could not open session cache %s",
		     var_smtp_tls_scache_db);
	/*
	 * It is practical to have OpenSSL automatically save newly created
	 * sessions for us by callback. Therefore we have to enable the
	 * internal session cache for the client side. Disable automatic
	 * clearing, as smtp has limited lifetime anyway and we can call
	 * the cleanup routine at will.
	 */
	SSL_CTX_set_session_cache_mode(ctx,
			SSL_SESS_CACHE_CLIENT|SSL_SESS_CACHE_NO_AUTO_CLEAR);
	SSL_CTX_sess_set_new_cb(ctx, new_session_cb);
    }
   
    /*
     * Finally create the global index to access TLScontext information
     * inside verify_callback.
     */
    TLScontext_index = SSL_get_ex_new_index(0, "TLScontext ex_data index",
					    NULL, NULL, NULL);
    TLSpeername_index = SSL_SESSION_get_ex_new_index(0,
					    "TLSpeername ex_data index",
					    new_peername_func,
					    dup_peername_func,
					    free_peername_func);

    pfixtls_clientengine = 1;
    return (0);
}

 /*
  * This is the actual startup routine for the connection. We expect
  * that the buffers are flushed and the "220 Ready to start TLS" was
  * received by us, so that we can immediately can start the TLS
  * handshake process.
  */
int     pfixtls_start_clienttls(VSTREAM *stream, int timeout,
			        int enforce_peername,
				const char *peername,
				tls_info_t *tls_info)
{
    int     sts;
    SSL_SESSION *session, *old_session;
    SSL_CIPHER *cipher;
    X509   *peer;
    int     verify_flags;
    TLScontext_t *TLScontext;

    if (!pfixtls_clientengine) {		/* should never happen */
	msg_info("tls_engine not running");
	return (-1);
    }
    if (var_smtpd_tls_loglevel >= 1)
	msg_info("setting up TLS connection to %s", peername);

    /*
     * Allocate a new TLScontext for the new connection and get an SSL
     * structure. Add the location of TLScontext to the SSL to later
     * retrieve the information inside the verify_callback().
     */
    TLScontext = (TLScontext_t *)mymalloc(sizeof(TLScontext_t));
    if (!TLScontext) {
	msg_fatal("Could not allocate 'TLScontext' with mymalloc");
    }
    if ((TLScontext->con = (SSL *) SSL_new(ctx)) == NULL) {
	msg_info("Could not allocate 'TLScontext->con' with SSL_new()");
	pfixtls_print_errors();
	myfree((char *)TLScontext);
	return (-1);
    }
    if (!SSL_set_ex_data(TLScontext->con, TLScontext_index, TLScontext)) {
	msg_info("Could not set application data for 'TLScontext->con'");
	pfixtls_print_errors();
	SSL_free(TLScontext->con);
	myfree((char *)TLScontext);
	return (-1);
    }

    /*
     * Set the verification parameters to be checked in verify_callback().
     */
    if (enforce_peername) {
	verify_flags = SSL_VERIFY_PEER;
	TLScontext->enforce_verify_errors = 1;
	TLScontext->enforce_CN = 1;
        SSL_set_verify(TLScontext->con, verify_flags, verify_callback);
    }
    else {
	TLScontext->enforce_verify_errors = 0;
	TLScontext->enforce_CN = 0;
    }
    TLScontext->hostname_matched = 0;

    /*
     * The TLS connection is realized by a BIO_pair, so obtain the pair.
     */
    if (!BIO_new_bio_pair(&TLScontext->internal_bio, BIO_bufsiz,
			  &TLScontext->network_bio, BIO_bufsiz)) {
	msg_info("Could not obtain BIO_pair");
	pfixtls_print_errors();
	SSL_free(TLScontext->con);
	myfree((char *)TLScontext);
	return (-1);
    }

    old_session = NULL;

    /*
     * Find out the hashed HostID for the client cache and try to
     * load the session from the cache.
     */
    strncpy(TLScontext->peername_save, peername, ID_MAXLENGTH + 1);
    TLScontext->peername_save[ID_MAXLENGTH] = '\0';  /* just in case */
    (void)lowercase(TLScontext->peername_save);
    if (scache_db) {
	old_session = load_clnt_session(peername, enforce_peername);
	if (old_session) {
	   SSL_set_session(TLScontext->con, old_session);
#if (OPENSSL_VERSION_NUMBER < 0x00906011L) || (OPENSSL_VERSION_NUMBER == 0x00907000L)
	    /*
	     * Ugly Hack: OpenSSL before 0.9.6a does not store the verify
	     * result in sessions for the client side.
	     * We modify the session directly which is version specific,
	     * but this bug is version specific, too.
	     *
	     * READ: 0-09-06-01-1 = 0-9-6-a-beta1: all versions before
	     * beta1 have this bug, it has been fixed during development
	     * of 0.9.6a. The development version of 0.9.7 can have this
	     * bug, too. It has been fixed on 2000/11/29.
	     */
	    SSL_set_verify_result(TLScontext->con, old_session->verify_result);
#endif
	   
	}
    }

    /*
     * Before really starting anything, try to seed the PRNG a little bit
     * more.
     */
    pfixtls_stir_seed();
    pfixtls_exchange_seed();

    /*
     * Initialize the SSL connection to connect state. This should not be
     * necessary anymore since 0.9.3, but the call is still in the library
     * and maintaining compatibility never hurts.
     */
    SSL_set_connect_state(TLScontext->con);

    /*
     * Connect the SSL-connection with the postfix side of the BIO-pair for
     * reading and writing.
     */
    SSL_set_bio(TLScontext->con, TLScontext->internal_bio,
		TLScontext->internal_bio);

    /*
     * If the debug level selected is high enough, all of the data is
     * dumped: 3 will dump the SSL negotiation, 4 will dump everything.
     *
     * We do have an SSL_set_fd() and now suddenly a BIO_ routine is called?
     * Well there is a BIO below the SSL routines that is automatically
     * created for us, so we can use it for debugging purposes.
     */
    if (var_smtp_tls_loglevel >= 3)
	BIO_set_callback(SSL_get_rbio(TLScontext->con), bio_dump_cb);


    /* Dump the negotiation for loglevels 3 and 4 */
    if (var_smtp_tls_loglevel >= 3)
	do_dump = 1;

    /*
     * Now we expect the negotiation to begin. This whole process is like a
     * black box for us. We totally have to rely on the routines build into
     * the OpenSSL library. The only thing we can do we already have done
     * by choosing our own callback certificate verification.
     *
     * Error handling:
     * If the SSL handhake fails, we print out an error message and remove
     * everything that might be there. A session has to be removed anyway,
     * because RFC2246 requires it. 
     */
    sts = do_tls_operation(vstream_fileno(stream), timeout, TLScontext,
			   SSL_connect, NULL, NULL, NULL, 0);
    if (sts <= 0) {
	msg_info("SSL_connect error to %s: %d", peername, sts);
	pfixtls_print_errors();
	session = SSL_get_session(TLScontext->con);
	if (session) {
	    SSL_CTX_remove_session(ctx, session);
	    if (var_smtp_tls_loglevel >= 2)
		msg_info("SSL session removed");
	}
	if ((old_session) && (!SSL_session_reused(TLScontext->con)))
	    SSL_SESSION_free(old_session);	/* Must also be removed */
	SSL_free(TLScontext->con);
	myfree((char *)TLScontext);
	return (-1);
    }

    if (!SSL_session_reused(TLScontext->con)) {
	SSL_SESSION_free(old_session);	/* Remove unused session */
    }
    else if (var_smtp_tls_loglevel >= 3)
	msg_info("Reusing old session");

    /* Only loglevel==4 dumps everything */
    if (var_smtp_tls_loglevel < 4)
	do_dump = 0;

    /*
     * Lets see, whether a peer certificate is available and what is
     * the actual information. We want to save it for later use.
     */
    peer = SSL_get_peer_certificate(TLScontext->con);
    if (peer != NULL) {
	if (SSL_get_verify_result(TLScontext->con) == X509_V_OK)
	    tls_info->peer_verified = 1;

	tls_info->hostname_matched = TLScontext->hostname_matched;
	TLScontext->peer_CN[0] = '\0';
	if (!X509_NAME_get_text_by_NID(X509_get_subject_name(peer),
			NID_commonName, TLScontext->peer_CN, CCERT_BUFSIZ)) {
	    msg_info("Could not parse server's subject CN");
	    pfixtls_print_errors();
	}
	tls_info->peer_CN = TLScontext->peer_CN;

	TLScontext->issuer_CN[0] = '\0';
	if (!X509_NAME_get_text_by_NID(X509_get_issuer_name(peer),
			NID_commonName, TLScontext->issuer_CN, CCERT_BUFSIZ)) {
	    msg_info("Could not parse server's issuer CN");
	    pfixtls_print_errors();
	}
	if (!TLScontext->issuer_CN[0]) {
	    /* No issuer CN field, use Organization instead */
	    if (!X509_NAME_get_text_by_NID(X509_get_issuer_name(peer),
		NID_organizationName, TLScontext->issuer_CN, CCERT_BUFSIZ)) {
		msg_info("Could not parse server's issuer Organization");
		pfixtls_print_errors();
	    }
	}
	tls_info->issuer_CN = TLScontext->issuer_CN;

	if (var_smtp_tls_loglevel >= 1) {
	    if (tls_info->peer_verified)
		msg_info("Verified: subject_CN=%s, issuer=%s",
			 TLScontext->peer_CN, TLScontext->issuer_CN);
	    else
		msg_info("Unverified: subject_CN=%s, issuer=%s",
			 TLScontext->peer_CN, TLScontext->issuer_CN);
	}
	X509_free(peer);
    }

    /*
     * Finally, collect information about protocol and cipher for logging
     */ 
    tls_info->protocol = SSL_get_version(TLScontext->con);
    cipher = SSL_get_current_cipher(TLScontext->con);
    tls_info->cipher_name = SSL_CIPHER_get_name(cipher);
    tls_info->cipher_usebits = SSL_CIPHER_get_bits(cipher,
						 &(tls_info->cipher_algbits));

    pfixtls_clientactive = 1;

    /*
     * The TLS engine is active, switch to the pfixtls_timed_read/write()
     * functions.
     */
    vstream_control(stream,
		    VSTREAM_CTL_READ_FN, pfixtls_timed_read,
		    VSTREAM_CTL_WRITE_FN, pfixtls_timed_write,
		    VSTREAM_CTL_CONTEXT, (void *)TLScontext,
		    VSTREAM_CTL_END);

    if (var_smtp_tls_loglevel >= 1)
	msg_info("TLS connection established to %s: %s with cipher %s (%d/%d bits)",
		 peername, tls_info->protocol, tls_info->cipher_name,
		 tls_info->cipher_usebits, tls_info->cipher_algbits);

    pfixtls_stir_seed();

    return (0);
}

 /*
  * Shut down the TLS connection, that does mean: remove all the information
  * and reset the flags! This is needed if the actual running smtp is to
  * be restarted. We do not give back any value, as there is nothing to
  * be reported.
  * Since our session cache is external, we will remove the session from
  * memory in any case. The SSL_CTX_flush_sessions might be redundant here,
  * I however want to make sure nothing is left.
  * RFC2246 requires us to remove sessions if something went wrong, as
  * indicated by the "failure" value,so we remove it from the external
  * cache, too.
  */
int     pfixtls_stop_clienttls(VSTREAM *stream, int timeout, int failure,
			       tls_info_t *tls_info)
{
    TLScontext_t *TLScontext;
    int retval;

    if (pfixtls_clientactive) {
	TLScontext = (TLScontext_t *)vstream_context(stream);
	/*
	 * Perform SSL_shutdown() twice, as the first attempt may return
	 * to early: it will only send out the shutdown alert but it will
	 * not wait for the peer's shutdown alert. Therefore, when we are
	 * the first party to send the alert, we must call SSL_shutdown()
	 * again.
	 * On failure we don't want to resume the session, so we will not
	 * perform SSL_shutdown() and the session will be removed as being
	 * bad.
	 */
	if (!failure) {
	    retval = do_tls_operation(vstream_fileno(stream), timeout,
				TLScontext, SSL_shutdown, NULL, NULL, NULL, 0);
	    if (retval == 0)
		do_tls_operation(vstream_fileno(stream), timeout, TLScontext,
				SSL_shutdown, NULL, NULL, NULL, 0);
	}
	/*
	 * Free the SSL structure and the BIOs. Warning: the internal_bio is
	 * connected to the SSL structure and is automatically freed with
	 * it. Do not free it again (core dump)!!
	 * Only free the network_bio.
	 */
	SSL_free(TLScontext->con);
	BIO_free(TLScontext->network_bio);
	myfree((char *)TLScontext);
	vstream_control(stream,
		    VSTREAM_CTL_READ_FN, (VSTREAM_FN) NULL,
		    VSTREAM_CTL_WRITE_FN, (VSTREAM_FN) NULL,
		    VSTREAM_CTL_CONTEXT, (void *) NULL,
		    VSTREAM_CTL_END);
	SSL_CTX_flush_sessions(ctx, time(NULL));

	pfixtls_stir_seed();
	pfixtls_exchange_seed();

	*tls_info = tls_info_zero;
	pfixtls_clientactive = 0;

    }

    return (0);
}


#endif /* USE_SSL */
