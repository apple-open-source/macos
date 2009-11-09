/*
 * socket.c -- socket library functions
 *
 * Copyright 1998 by Eric S. Raymond.
 * For license terms, see the file COPYING in this directory.
 */

#include "config.h"
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <ctype.h> /* isspace() */
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif /* HAVE_MEMORY_H */
#include <sys/types.h>
#include <sys/stat.h>
#ifndef HAVE_NET_SOCKET_H
#include <sys/socket.h>
#else
#include <net/socket.h>
#endif
#include <sys/un.h>
#include <netinet/in.h>
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#include <netdb.h>
#if defined(STDC_HEADERS)
#include <stdlib.h>
#endif
#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif
#if defined(HAVE_STDARG_H)
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include "socket.h"
#include "fetchmail.h"
#include "getaddrinfo.h"
#include "i18n.h"
#include "sdump.h"

/* Defines to allow BeOS and Cygwin to play nice... */
#ifdef __BEOS__
static char peeked;
#define fm_close(a)  closesocket(a)
#define fm_write(a,b,c)  send(a,b,c,0)
#define fm_peek(a,b,c)   recv(a,b,c,0)
#define fm_read(a,b,c)   recv(a,b,c,0)
#else
#define fm_close(a)  close(a)
#define fm_write(a,b,c)  write(a,b,c)
#define fm_peek(a,b,c)   recv(a,b,c, MSG_PEEK)
#ifdef __CYGWIN__
#define fm_read(a,b,c)   cygwin_read(a,b,c)
static ssize_t cygwin_read(int sock, void *buf, size_t count);
#else /* ! __CYGWIN__ */
#define fm_read(a,b,c)   read(a,b,c)
#endif /* __CYGWIN__ */
#endif

/* We need to define h_errno only if it is not already */
#ifndef h_errno

#ifdef HAVE_RES_SEARCH
/* some versions of FreeBSD should declare this but don't */
extern int h_errno;
#else
/* pretend we have h_errno to avoid some #ifdef's later */
static int h_errno;
#endif

#endif /* ndef h_errno */

#ifdef HAVE_SOCKETPAIR
static char *const *parse_plugin(const char *plugin, const char *host, const char *service)
{	const char **argvec;
	const char *c, *p;
	char *cp, *plugin_copy;
	unsigned int plugin_copy_len;
	unsigned int plugin_offset = 0, plugin_copy_offset = 0;
	unsigned int i, s = 2 * sizeof(char*), host_count = 0, service_count = 0;
	unsigned int plugin_len = strlen(plugin);
	unsigned int host_len = strlen(host);
	unsigned int service_len = strlen(service);

	for (c = p = plugin; *c; c++)
	{	if (isspace((unsigned char)*c) && !isspace((unsigned char)*p))
			s += sizeof(char*);
		if (*p == '%' && *c == 'h')
			host_count++;
		if (*p == '%' && *c == 'p')
			service_count++;
		p = c;
	}

	plugin_copy_len = plugin_len + host_len * host_count + service_len * service_count;
	plugin_copy = (char *)malloc(plugin_copy_len + 1);
	if (!plugin_copy)
	{
		report(stderr, GT_("fetchmail: malloc failed\n"));
		return NULL;
	}

	while (plugin_copy_offset < plugin_copy_len)
	{	if ((plugin[plugin_offset] == '%') && (plugin[plugin_offset + 1] == 'h'))
		{	strcpy(plugin_copy + plugin_copy_offset, host);
			plugin_offset += 2;
			plugin_copy_offset += host_len;
		}
		else if ((plugin[plugin_offset] == '%') && (plugin[plugin_offset + 1] == 'p'))
		{	strcpy(plugin_copy + plugin_copy_offset, service);
			plugin_offset += 2;
			plugin_copy_offset += service_len;
		}
		else
		{	plugin_copy[plugin_copy_offset] = plugin[plugin_offset];
			plugin_offset++;
			plugin_copy_offset++;
		}
	}
	plugin_copy[plugin_copy_len] = 0;

	argvec = (const char **)malloc(s);
	if (!argvec)
	{
		report(stderr, GT_("fetchmail: malloc failed\n"));
		return NULL;
	}
	memset(argvec, 0, s);
	for (c = p = plugin_copy, i = 0; *c; c++)
	{	if ((!isspace((unsigned char)*c)) && (c == p ? 1 : isspace((unsigned char)*p))) {
			argvec[i] = c;
			i++;
		}
		p = c;
	}
	for (cp = plugin_copy; *cp; cp++)
	{	if (isspace((unsigned char)*cp))
			*cp = 0;
	}
	return (char *const*)argvec;
}

static int handle_plugin(const char *host,
			 const char *service, const char *plugin)
/* get a socket mediated through a given external command */
{
    int fds[2];
    char *const *argvec;

    /*
     * The author of this code, Felix von Leitner <felix@convergence.de>, says:
     * he chose socketpair() instead of pipe() because socketpair creates 
     * bidirectional sockets while allegedly some pipe() implementations don't.
     */
    if (socketpair(AF_UNIX,SOCK_STREAM,0,fds))
    {
	report(stderr, GT_("fetchmail: socketpair failed\n"));
	return -1;
    }
    switch (fork()) {
	case -1:
		/* error */
		report(stderr, GT_("fetchmail: fork failed\n"));
		return -1;
	case 0:	/* child */
		/* fds[1] is the parent's end; close it for proper EOF
		** detection */
		(void) close(fds[1]);
		if ( (dup2(fds[0],0) == -1) || (dup2(fds[0],1) == -1) ) {
			report(stderr, GT_("dup2 failed\n"));
			exit(1);
		}
		/* fds[0] is now connected to 0 and 1; close it */
		(void) close(fds[0]);
		if (outlevel >= O_VERBOSE)
		    report(stderr, GT_("running %s (host %s service %s)\n"), plugin, host, service);
		argvec = parse_plugin(plugin,host,service);
		execvp(*argvec, argvec);
		report(stderr, GT_("execvp(%s) failed\n"), *argvec);
		exit(0);
		break;
	default:	/* parent */
		/* NOP */
		break;
    }
    /* fds[0] is the child's end; close it for proper EOF detection */
    (void) close(fds[0]);
    return fds[1];
}
#endif /* HAVE_SOCKETPAIR */

#ifdef __UNUSED__

int SockCheckOpen(int fd)
/* poll given socket; is it selectable? */
{
    fd_set r, w, e;
    int rt;
    struct timeval tv;
  
    for (;;) 
    {
	FD_ZERO(&r); FD_ZERO(&w); FD_ZERO(&e);
	FD_SET(fd, &e);
    
	tv.tv_sec = 0; tv.tv_usec = 0;
	rt = select(fd+1, &r, &w, &e, &tv);
	if (rt == -1 && (errno != EAGAIN && errno != EINTR))
	    return 0;
	if (rt != -1)
	    return 1;
    }
}
#endif /* __UNUSED__ */

int UnixOpen(const char *path)
{
    int sock = -1;
    struct sockaddr_un ad;
    memset(&ad, 0, sizeof(ad));
    ad.sun_family = AF_UNIX;
    strncpy(ad.sun_path, path, sizeof(ad.sun_path)-1);

    sock = socket( AF_UNIX, SOCK_STREAM, 0 );
    if (sock < 0)
    {
	h_errno = 0;
	return -1;
    }

	/* Socket opened saved. Usefull if connect timeout 
	 * because it can be closed.
	 */
	mailserver_socket_temp = sock;
    
	if (connect(sock, (struct sockaddr *) &ad, sizeof(ad)) < 0)
    {
	int olderr = errno;
	fm_close(sock);	/* don't use SockClose, no traffic yet */
	h_errno = 0;
	errno = olderr;
	sock = -1;
    }
	
	/* No connect timeout, then no need to set mailserver_socket_temp */
	mailserver_socket_temp = -1;

    return sock;
}

int SockOpen(const char *host, const char *service,
	     const char *plugin, struct addrinfo **ai0)
{
    struct addrinfo *ai, req;
    int i, acterr = 0;

#ifdef HAVE_SOCKETPAIR
    if (plugin)
	return handle_plugin(host,service,plugin);
#endif /* HAVE_SOCKETPAIR */

    memset(&req, 0, sizeof(struct addrinfo));
    req.ai_socktype = SOCK_STREAM;
#ifdef AI_ADDRCONFIG
    req.ai_flags = AI_ADDRCONFIG;
#endif

    i = fm_getaddrinfo(host, service, &req, ai0);
    if (i) {
	report(stderr, GT_("getaddrinfo(\"%s\",\"%s\") error: %s\n"),
		host, service, gai_strerror(i));
	if (i == EAI_SERVICE)
	    report(stderr, GT_("Try adding the --service option (see also FAQ item R12).\n"));
	return -1;
    }

    i = -1;
    for (ai = *ai0; ai; ai = ai->ai_next) {
	char buf[80],pb[80];
	int gnie;

	gnie = getnameinfo(ai->ai_addr, ai->ai_addrlen, buf, sizeof(buf), NULL, 0, NI_NUMERICHOST);
	if (gnie)
	    snprintf(buf, sizeof(buf), GT_("unknown (%s)"), gai_strerror(gnie));
	gnie = getnameinfo(ai->ai_addr, ai->ai_addrlen, NULL, 0, pb, sizeof(pb), NI_NUMERICSERV);
	if (gnie)
	    snprintf(pb, sizeof(pb), GT_("unknown (%s)"), gai_strerror(gnie));

	if (outlevel >= O_VERBOSE)
	    report_build(stdout, GT_("Trying to connect to %s/%s..."), buf, pb);
	i = socket(ai->ai_family, ai->ai_socktype, 0);
	if (i < 0) {
	    /* mask EAFNOSUPPORT errors, they confuse users for
	     * multihomed hosts */
	    if (errno != EAFNOSUPPORT)
		acterr = errno;
	    if (outlevel >= O_VERBOSE)
		report_complete(stdout, GT_("cannot create socket: %s\n"), strerror(errno));
	    continue;
	}

	/* Save socket descriptor.
	 * Used to close the socket after connect timeout. */
	mailserver_socket_temp = i;

	if (connect(i, (struct sockaddr *) ai->ai_addr, ai->ai_addrlen) < 0) {
	    int e = errno;

	    /* additionally, suppress IPv4 network unreach errors */
	    if (e != EAFNOSUPPORT)
		acterr = errno;

	    if (outlevel >= O_VERBOSE)
		report_complete(stdout, GT_("connection failed.\n"));
	    if (outlevel > O_SILENT)
		report(stderr, GT_("connection to %s:%s [%s/%s] failed: %s.\n"), host, service, buf, pb, strerror(e));
	    fm_close(i);
	    i = -1;
	    continue;
	} else {
	    if (outlevel >= O_VERBOSE)
		report_complete(stdout, GT_("connected.\n"));
	}

	/* No connect timeout, then no need to set mailserver_socket_temp */
	mailserver_socket_temp = -1;

	break;
    }

    fm_freeaddrinfo(*ai0);
    *ai0 = NULL;

    if (i == -1)
	errno = acterr;

    return i;
}


#if defined(HAVE_STDARG_H)
int SockPrintf(int sock, const char* format, ...)
{
#else
int SockPrintf(sock,format,va_alist)
int sock;
char *format;
va_dcl {
#endif

    va_list ap;
    char buf[8192];

#if defined(HAVE_STDARG_H)
    va_start(ap, format) ;
#else
    va_start(ap);
#endif
    vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    return SockWrite(sock, buf, strlen(buf));

}

#ifdef SSL_ENABLE
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/x509v3.h>
#include <openssl/rand.h>

static	SSL_CTX *_ctx[FD_SETSIZE];
static	SSL *_ssl_context[FD_SETSIZE];

static SSL	*SSLGetContext( int );
#endif /* SSL_ENABLE */

int SockWrite(int sock, char *buf, int len)
{
    int n, wrlen = 0;
#ifdef	SSL_ENABLE
    SSL *ssl;
#endif

    while (len)
    {
#ifdef SSL_ENABLE
	if( NULL != ( ssl = SSLGetContext( sock ) ) )
		n = SSL_write(ssl, buf, len);
	else
#endif /* SSL_ENABLE */
	    n = fm_write(sock, buf, len);
        if (n <= 0)
            return -1;
        len -= n;
	wrlen += n;
	buf += n;
    }
    return wrlen;
}

int SockRead(int sock, char *buf, int len)
{
    char *newline, *bp = buf;
    int n;
#ifdef	SSL_ENABLE
    SSL *ssl;
#endif

    if (--len < 1)
	return(-1);
#ifdef __BEOS__
    if (peeked != 0){
        (*bp) = peeked;
        bp++;
        len--;
        peeked = 0;
    }
#endif        
    do {
	/* 
	 * The reason for these gymnastics is that we want two things:
	 * (1) to read \n-terminated lines,
	 * (2) to return the true length of data read, even if the
	 *     data coming in has embedded NULS.
	 */
#ifdef	SSL_ENABLE
	if( NULL != ( ssl = SSLGetContext( sock ) ) ) {
		/* Hack alert! */
		/* OK...  SSL_peek works a little different from MSG_PEEK
			Problem is that SSL_peek can return 0 if there
			is no data currently available.  If, on the other
			hand, we loose the socket, we also get a zero, but
			the SSL_read then SEGFAULTS!  To deal with this,
			we'll check the error code any time we get a return
			of zero from SSL_peek.  If we have an error, we bail.
			If we don't, we read one character in SSL_read and
			loop.  This should continue to work even if they
			later change the behavior of SSL_peek
			to "fix" this problem...  :-(	*/
		if ((n = SSL_peek(ssl, bp, len)) < 0) {
			(void)SSL_get_error(ssl, n);
			return(-1);
		}
		if( 0 == n ) {
			/* SSL_peek says no data...  Does he mean no data
			or did the connection blow up?  If we got an error
			then bail! */
			if( 0 != ( n = SSL_get_error(ssl, n) ) ) {
				return -1;
			}
			/* We didn't get an error so read at least one
				character at this point and loop */
			n = 1;
			/* Make sure newline start out NULL!
			 * We don't have a string to pass through
			 * the strchr at this point yet */
			newline = NULL;
		} else if ((newline = (char *)memchr(bp, '\n', n)) != NULL)
			n = newline - bp + 1;
		/* Matthias Andree: SSL_read can return 0, in that case
		 * we must call SSL_get_error to figure if there was
		 * an error or just a "no data" condition */
		if ((n = SSL_read(ssl, bp, n)) <= 0) {
			if ((n = SSL_get_error(ssl, n))) {
				return(-1);
			}
		}
		/* Check for case where our single character turned out to
		 * be a newline...  (It wasn't going to get caught by
		 * the strchr above if it came from the hack...  ). */
		if( NULL == newline && 1 == n && '\n' == *bp ) {
			/* Got our newline - this will break
				out of the loop now */
			newline = bp;
		}
	}
	else
#endif /* SSL_ENABLE */
	{

#ifdef __BEOS__
	    if ((n = fm_read(sock, bp, 1)) <= 0)
#else
	    if ((n = fm_peek(sock, bp, len)) <= 0)
#endif
		return (-1);
	    if ((newline = (char *)memchr(bp, '\n', n)) != NULL)
		n = newline - bp + 1;
#ifndef __BEOS__
	    if ((n = fm_read(sock, bp, n)) == -1)
		return(-1);
#endif /* __BEOS__ */
	}
	bp += n;
	len -= n;
    } while 
	    (!newline && len);
    *bp = '\0';

    return bp - buf;
}

int SockPeek(int sock)
/* peek at the next socket character without actually reading it */
{
    int n;
    char ch;
#ifdef	SSL_ENABLE
    SSL *ssl;
#endif

#ifdef	SSL_ENABLE
	if( NULL != ( ssl = SSLGetContext( sock ) ) ) {
		n = SSL_peek(ssl, &ch, 1);
		if (n < 0) {
			(void)SSL_get_error(ssl, n);
			return -1;
		}
		if( 0 == n ) {
			/* This code really needs to implement a "hold back"
			 * to simulate a functioning SSL_peek()...  sigh...
			 * Has to be coordinated with the read code above.
			 * Next on the list todo...	*/

			/* SSL_peek says 0...  Does that mean no data
			or did the connection blow up?  If we got an error
			then bail! */
			if( 0 != ( n = SSL_get_error(ssl, n) ) ) {
				return -1;
			}

			/* Haven't seen this case actually occur, but...
			   if the problem in SockRead can occur, this should
			   be possible...  Just not sure what to do here.
			   This should be a safe "punt" the "peek" but don't
			   "punt" the "session"... */

			return 0;	/* Give him a '\0' character */
		}
	}
	else
#endif /* SSL_ENABLE */
	    n = fm_peek(sock, &ch, 1);
	if (n == -1)
		return -1;

#ifdef __BEOS__
    peeked = ch;
#endif
    return(ch);
}

#ifdef SSL_ENABLE

static	char *_ssl_server_cname = NULL;
static	int _check_fp;
static	char *_check_digest;
static 	char *_server_label;
static	int _depth0ck;
static	int _prev_err;

SSL *SSLGetContext( int sock )
{
	if( sock < 0 || (unsigned)sock > FD_SETSIZE )
		return NULL;
	if( _ctx[sock] == NULL )
		return NULL;
	return _ssl_context[sock];
}


/* ok_return (preverify_ok) is 1 if this stage of certificate verification
   passed, or 0 if it failed. This callback lets us display informative
   errors, and perform additional validation (e.g. CN matches) */
static int SSL_verify_callback( int ok_return, X509_STORE_CTX *ctx, int strict )
{
	char buf[257];
	X509 *x509_cert;
	int err, depth, i;
	unsigned char digest[EVP_MAX_MD_SIZE];
	char text[EVP_MAX_MD_SIZE * 3 + 1], *tp, *te;
	const EVP_MD *digest_tp;
	unsigned int dsz, esz;
	X509_NAME *subj, *issuer;
	char *tt;

	x509_cert = X509_STORE_CTX_get_current_cert(ctx);
	err = X509_STORE_CTX_get_error(ctx);
	depth = X509_STORE_CTX_get_error_depth(ctx);

	subj = X509_get_subject_name(x509_cert);
	issuer = X509_get_issuer_name(x509_cert);

	if (depth == 0 && !_depth0ck) {
		_depth0ck = 1;

		if (outlevel >= O_VERBOSE) {
			if ((i = X509_NAME_get_text_by_NID(issuer, NID_organizationName, buf, sizeof(buf))) != -1) {
				report(stdout, GT_("Issuer Organization: %s\n"), (tt = sdump(buf, i)));
				xfree(tt);
				if ((size_t)i >= sizeof(buf) - 1)
					report(stdout, GT_("Warning: Issuer Organization Name too long (possibly truncated).\n"));
			} else
				report(stdout, GT_("Unknown Organization\n"));
			if ((i = X509_NAME_get_text_by_NID(issuer, NID_commonName, buf, sizeof(buf))) != -1) {
				report(stdout, GT_("Issuer CommonName: %s\n"), (tt = sdump(buf, i)));
				xfree(tt);
				if ((size_t)i >= sizeof(buf) - 1)
					report(stdout, GT_("Warning: Issuer CommonName too long (possibly truncated).\n"));
			} else
				report(stdout, GT_("Unknown Issuer CommonName\n"));
		}
		if ((i = X509_NAME_get_text_by_NID(subj, NID_commonName, buf, sizeof(buf))) != -1) {
			if (outlevel >= O_VERBOSE)
				report(stdout, GT_("Server CommonName: %s\n"), (tt = sdump(buf, i)));
			xfree(tt);
			if ((size_t)i >= sizeof(buf) - 1) {
				/* Possible truncation. In this case, this is a DNS name, so this
				 * is really bad. We do not tolerate this even in the non-strict case. */
				report(stderr, GT_("Bad certificate: Subject CommonName too long!\n"));
				return (0);
			}
			if ((size_t)i > strlen(buf)) {
				/* Name contains embedded NUL characters, so we complain. This is likely
				 * a certificate spoofing attack. */
				report(stderr, GT_("Bad certificate: Subject CommonName contains NUL, aborting!\n"));
				return 0;
			}
			if (_ssl_server_cname != NULL) {
				char *p1 = buf;
				char *p2 = _ssl_server_cname;
				int n;
				int matched = 0;
				STACK_OF(GENERAL_NAME) *gens;
				
				/* RFC 2595 section 2.4: find a matching name
				 * first find a match among alternative names */
				gens = (STACK_OF(GENERAL_NAME) *)X509_get_ext_d2i(x509_cert, NID_subject_alt_name, NULL, NULL);
				if (gens) {
					int j, r;
					for (j = 0, r = sk_GENERAL_NAME_num(gens); j < r; ++j) {
						const GENERAL_NAME *gn = sk_GENERAL_NAME_value(gens, j);
						if (gn->type == GEN_DNS) {
							char *p1 = (char *)gn->d.ia5->data;
							char *p2 = _ssl_server_cname;
							if (outlevel >= O_VERBOSE) {
								report(stdout, GT_("Subject Alternative Name: %s\n"), (tt = sdump(p1, (size_t)gn->d.ia5->length)));
								xfree(tt);
							}
							/* Name contains embedded NUL characters, so we complain. This
							 * is likely a certificate spoofing attack. */
							if ((size_t)gn->d.ia5->length != strlen(p1)) {
								report(stderr, GT_("Bad certificate: Subject Alternative Name contains NUL, aborting!\n"));
								sk_GENERAL_NAME_free(gens);
								return 0;
							}
							if (*p1 == '*') {
								++p1;
								n = strlen(p2) - strlen(p1);
								if (n >= 0)
									p2 += n;
							}
							if (0 == strcasecmp(p1, p2)) {
								matched = 1;
							}
						}
					}
					sk_GENERAL_NAME_free(gens);
				}
				if (*p1 == '*') {
					++p1;
					n = strlen(p2) - strlen(p1);
					if (n >= 0)
						p2 += n;
				}
				if (0 == strcasecmp(p1, p2)) {
					matched = 1;
				}
				if (!matched) {
					report(stderr,
					    GT_("Server CommonName mismatch: %s != %s\n"),
					    (tt = sdump(buf, i)), _ssl_server_cname );
					xfree(tt);
					if (ok_return && strict)
						return (0);
				}
			} else if (ok_return) {
				report(stderr, GT_("Server name not set, could not verify certificate!\n"));
				if (strict) return (0);
			}
		} else {
			if (outlevel >= O_VERBOSE)
				report(stdout, GT_("Unknown Server CommonName\n"));
			if (ok_return && strict) {
				report(stderr, GT_("Server name not specified in certificate!\n"));
				return (0);
			}
		}
		/* Print the finger print. Note that on errors, we might print it more than once
		 * normally; we kluge around that by using a global variable. */
		if (_check_fp == 1) {
			unsigned dp;

			_check_fp = -1;
			digest_tp = EVP_md5();
			if (digest_tp == NULL) {
				report(stderr, GT_("EVP_md5() failed!\n"));
				return (0);
			}
			if (!X509_digest(x509_cert, digest_tp, digest, &dsz)) {
				report(stderr, GT_("Out of memory!\n"));
				return (0);
			}
			tp = text;
			te = text + sizeof(text);
			for (dp = 0; dp < dsz; dp++) {
				esz = snprintf(tp, te - tp, dp > 0 ? ":%02X" : "%02X", digest[dp]);
				if (esz >= (size_t)(te - tp)) {
					report(stderr, GT_("Digest text buffer too small!\n"));
					return (0);
				}
				tp += esz;
			}
			if (outlevel > O_NORMAL)
			    report(stdout, GT_("%s key fingerprint: %s\n"), _server_label, text);
			if (_check_digest != NULL) {
				if (strcasecmp(text, _check_digest) == 0) {
				    if (outlevel > O_NORMAL)
					report(stdout, GT_("%s fingerprints match.\n"), _server_label);
				} else {
				    report(stderr, GT_("%s fingerprints do not match!\n"), _server_label);
				    return (0);
				}
			} /* if (_check_digest != NULL) */
		} /* if (_check_fp) */
	} /* if (depth == 0 && !_depth0ck) */

	if (err != X509_V_OK && err != _prev_err && !(_check_fp != 0 && _check_digest && !strict)) {
		_prev_err = err;
		report(stderr, GT_("Server certificate verification error: %s\n"), X509_verify_cert_error_string(err));
		/* We gave the error code, but maybe we can add some more details for debugging */
		switch (err) {
		case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
			X509_NAME_oneline(issuer, buf, sizeof(buf));
			buf[sizeof(buf) - 1] = '\0';
			report(stderr, GT_("unknown issuer (first %d characters): %s\n"), (int)(sizeof(buf)-1), buf);
			break;
		}
	}
	/*
	 * If not in strict checking mode (--sslcertck), override this
	 * and pretend that verification had succeeded.
	 */
	if (!strict)
		ok_return = 1;
	return (ok_return);
}

static int SSL_nock_verify_callback( int ok_return, X509_STORE_CTX *ctx )
{
	return SSL_verify_callback(ok_return, ctx, 0);
}

static int SSL_ck_verify_callback( int ok_return, X509_STORE_CTX *ctx )
{
	return SSL_verify_callback(ok_return, ctx, 1);
}


/* get commonName from certificate set in file.
 * commonName is stored in buffer namebuffer, limited with namebufferlen
 */
static const char *SSLCertGetCN(const char *mycert,
                                char *namebuffer, size_t namebufferlen)
{
	const char *ret       = NULL;
	BIO        *certBio   = NULL;
	X509       *x509_cert = NULL;
	X509_NAME  *certname  = NULL;

	if (namebuffer && namebufferlen > 0) {
		namebuffer[0] = 0x00;
		certBio = BIO_new_file(mycert,"r");
		if (certBio) {
			x509_cert = PEM_read_bio_X509(certBio,NULL,NULL,NULL);
			BIO_free(certBio);
		}
		if (x509_cert) {
			certname = X509_get_subject_name(x509_cert);
			if (certname &&
			    X509_NAME_get_text_by_NID(certname, NID_commonName,
						      namebuffer, namebufferlen) > 0)
				ret = namebuffer;
			X509_free(x509_cert);
		}
	}
	return ret;
}

/* performs initial SSL handshake over the connected socket
 * uses SSL *ssl global variable, which is currently defined
 * in this file
 */
int SSLOpen(int sock, char *mycert, char *mykey, char *myproto, int certck, char *certpath,
    char *fingerprint, char *servercname, char *label, char **remotename)
{
        struct stat randstat;
        int i;

	SSL_load_error_strings();
	SSLeay_add_ssl_algorithms(); /* synonym for SSL_library_init() */
	
#ifdef SSL_ENABLE
        if (stat("/dev/random", &randstat)  &&
            stat("/dev/urandom", &randstat)) {
          /* Neither /dev/random nor /dev/urandom are present, so add
             entropy to the SSL PRNG a hard way. */
          for (i = 0; i < 10000  &&  ! RAND_status (); ++i) {
            char buf[4];
            struct timeval tv;
            gettimeofday (&tv, 0);
            buf[0] = tv.tv_usec & 0xF;
            buf[2] = (tv.tv_usec & 0xF0) >> 4;
            buf[3] = (tv.tv_usec & 0xF00) >> 8;
            buf[1] = (tv.tv_usec & 0xF000) >> 12;
            RAND_add (buf, sizeof buf, 0.1);
          }
        }
#endif /* SSL_ENABLE */


	if( sock < 0 || (unsigned)sock > FD_SETSIZE ) {
		report(stderr, GT_("File descriptor out of range for SSL") );
		return( -1 );
	}

	/* Make sure a connection referring to an older context is not left */
	_ssl_context[sock] = NULL;
	if(myproto) {
		if(!strcasecmp("ssl2",myproto)) {
			_ctx[sock] = SSL_CTX_new(SSLv2_client_method());
		} else if(!strcasecmp("ssl3",myproto)) {
			_ctx[sock] = SSL_CTX_new(SSLv3_client_method());
		} else if(!strcasecmp("tls1",myproto)) {
			_ctx[sock] = SSL_CTX_new(TLSv1_client_method());
		} else if (!strcasecmp("ssl23",myproto)) {
			myproto = NULL;
		} else {
			fprintf(stderr,GT_("Invalid SSL protocol '%s' specified, using default (SSLv23).\n"), myproto);
			myproto = NULL;
		}
	}
	if(!myproto) {
		_ctx[sock] = SSL_CTX_new(SSLv23_client_method());
	}
	if(_ctx[sock] == NULL) {
		ERR_print_errors_fp(stderr);
		return(-1);
	}

	SSL_CTX_set_options(_ctx[sock], SSL_OP_ALL);

	if (certck) {
		SSL_CTX_set_verify(_ctx[sock], SSL_VERIFY_PEER, SSL_ck_verify_callback);
	} else {
		/* In this case, we do not fail if verification fails. However,
		 *  we provide the callback for output and possible fingerprint checks. */
		SSL_CTX_set_verify(_ctx[sock], SSL_VERIFY_PEER, SSL_nock_verify_callback);
	}
	if (certpath)
		SSL_CTX_load_verify_locations(_ctx[sock], NULL, certpath);
	else
		SSL_CTX_set_default_verify_paths(_ctx[sock]);
	
	_ssl_context[sock] = SSL_new(_ctx[sock]);
	
	if(_ssl_context[sock] == NULL) {
		ERR_print_errors_fp(stderr);
		SSL_CTX_free(_ctx[sock]);
		_ctx[sock] = NULL;
		return(-1);
	}
	
	/* This static is for the verify callback */
	_ssl_server_cname = servercname;
	_server_label = label;
	_check_fp = 1;
	_check_digest = fingerprint;
	_depth0ck = 0;
	_prev_err = -1;

	if( mycert || mykey ) {

	/* Ok...  He has a certificate file defined, so lets declare it.  If
	 * he does NOT have a separate certificate and private key file then
	 * assume that it's a combined key and certificate file.
	 */
		char buffer[256];
		
		if( !mykey )
			mykey = mycert;
		if( !mycert )
			mycert = mykey;

		if ((!*remotename || !**remotename) && SSLCertGetCN(mycert, buffer, sizeof(buffer))) {
			free(*remotename);
			*remotename = xstrdup(buffer);
		}
        	SSL_use_certificate_file(_ssl_context[sock], mycert, SSL_FILETYPE_PEM);
        	SSL_use_RSAPrivateKey_file(_ssl_context[sock], mykey, SSL_FILETYPE_PEM);
	}

	if (SSL_set_fd(_ssl_context[sock], sock) == 0 
	    || SSL_connect(_ssl_context[sock]) < 1) {
		ERR_print_errors_fp(stderr);
		SSL_free( _ssl_context[sock] );
		_ssl_context[sock] = NULL;
		SSL_CTX_free(_ctx[sock]);
		_ctx[sock] = NULL;
		return(-1);
	}

	/* Paranoia: was the callback not called as we expected? */
	if (!_depth0ck) {
		report(stderr, GT_("Certificate/fingerprint verification was somehow skipped!\n"));

		if (fingerprint != NULL || certck) {
			if( NULL != SSLGetContext( sock ) ) {
				/* Clean up the SSL stack */
				SSL_shutdown( _ssl_context[sock] );
				SSL_free( _ssl_context[sock] );
				_ssl_context[sock] = NULL;
				SSL_CTX_free(_ctx[sock]);
				_ctx[sock] = NULL;
			}
			return(-1);
		}
	}

	return(0);
}
#endif

int SockClose(int sock)
/* close a socket gracefully */
{
#ifdef	SSL_ENABLE
    if( NULL != SSLGetContext( sock ) ) {
        /* Clean up the SSL stack */
        SSL_shutdown( _ssl_context[sock] );
        SSL_free( _ssl_context[sock] );
        _ssl_context[sock] = NULL;
	SSL_CTX_free(_ctx[sock]);
	_ctx[sock] = NULL;
    }
#endif

#ifdef __UNUSED__
    /* 
     * This hangs in RedHat 6.2 after fetchmail runs for a while a
     * FIN_WAIT2 comes up in netstat and fetchmail never returns from
     * the recv system call. (Reported from jtnews
     * <jtnews@bellatlantic.net>, Wed, 24 May 2000 21:26:02.)
     *
     * Half-close the connection first so the other end gets notified.
     *
     * This stops sends but allows receives (effectively, it sends a
     * TCP <FIN>).  */
    if (shutdown(sock, 1) == 0) {
	char ch;
	/* If there is any data still waiting in the queue, discard it.
	 * Call recv() until either it returns 0 (meaning we received a FIN)
	 * or any error occurs.  This makes sure all data sent by the other
	 * side is acknowledged at the TCP level.
	 */
	if (fm_peek(sock, &ch, 1) > 0)
	    while (fm_read(sock, &ch, 1) > 0)
		continue;
    }
#endif /* __UNUSED__ */

    /* if there's an error closing at this point, not much we can do */
    return(fm_close(sock));	/* this is guarded */
}

#ifdef __CYGWIN__
/*
 * Workaround Microsoft Winsock recv/WSARecv(..., MSG_PEEK) bug.
 * See http://sources.redhat.com/ml/cygwin/2001-08/msg00628.html
 * for more details.
 */
static ssize_t cygwin_read(int sock, void *buf, size_t count)
{
    char *bp = buf;
    int n = 0;

    if ((n = read(sock, bp, count)) == -1)
	return(-1);

    if (n != count) {
	int n2 = 0;
	if (outlevel >= O_VERBOSE)
	    report(stdout, GT_("Cygwin socket read retry\n"));
	n2 = read(sock, bp + n, count - n);
	if (n2 == -1 || n + n2 != count) {
	    report(stderr, GT_("Cygwin socket read retry failed!\n"));
	    return(-1);
	}
    }

    return count;
}
#endif /* __CYGWIN__ */

#ifdef MAIN
/*
 * Use the chargen service to test input buffering directly.
 * You may have to uncomment the `chargen' service description in your
 * inetd.conf (and then SIGHUP inetd) for this to work.  */
main()
{
    int	 	sock = SockOpen("localhost", "chargen", NULL);
    char	buf[80];

    while (SockRead(sock, buf, sizeof(buf)-1))
	SockWrite(1, buf, strlen(buf));
    SockClose(sock);
}
#endif /* MAIN */

/* socket.c ends here */
