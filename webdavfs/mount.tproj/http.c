/*-
 * Copyright 1997 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.	 M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.	 It is provided "as is" without express or implied
 * warranty.
 * 
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/types.h>
#include <sys/vnode.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#include <sys/param.h>		/* for MAXHOSTNAMELEN */
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#include <sys/uio.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/syslog.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include "fetch.h"
#include "http.h"
#include "pathnames.h"
#include "webdavd.h"
#include "webdav_parse.h"
#include "webdav_authcache.h"
#include "webdav_authentication.h"
#include "webdav_requestqueue.h"

/*****************************************************************************/

static void http_set_socket_sndtimeo(int fd, int32_t send_timeout);
static void http_state_free(struct http_state *https);
static int http_close(struct fetch_state *fs);
static int http_retrieve(struct fetch_state *fs,int * download_status);

extern int resolve_http_hostaddr();

/* We are only concerned with headers we might receive. */
enum http_header { 
	ht_accept_ranges, ht_age, ht_allow, ht_cache_control, ht_connection,
	ht_content_base, ht_content_encoding, ht_content_language,
	ht_content_length, ht_content_location, ht_content_md5, 
	ht_content_range, ht_content_type, ht_date, ht_dav, ht_etag, ht_expires,
	ht_last_modified, ht_location, ht_pragma, ht_proxy_authenticate,
	ht_public, ht_retry_after, ht_server, ht_transfer_encoding,
	ht_upgrade, ht_vary, ht_via, ht_www_authenticate, ht_warning,
	/* unusual cases */
	ht_syntax_error, ht_unknown, ht_end_of_header
};

static void format_http_date(time_t when, char *buf);
static enum http_header http_parse_header(char *line, char **valuep);
static int check_md5(int fd, char *base64ofmd5);  /* *** currently stubbed out *** */
static int http_first_line(char *line);
static int http_background_load(int *remote, int local, off_t total_length,
	int chunked, int * download_status);
static int parse_http_content_range(char *orig, off_t *first, off_t *total);
static void parse_ht_dav(char *field_value, int *dav_level);
static int http_parse_response_header(struct fetch_state *fs,
	size_t *total_length, int * dav_status, 
	int* chunked);

extern char *dest_server, *http_hostname, *proxy_server, *append_to_file;
extern struct sockaddr_in http_sin;
extern int proxy_ok, proxy_exception, dest_port, host_port;
extern int reconnect_min;
extern int webdav_first_read_len;
extern char *gUserAgentHeader;

time_t parse_http_date(char *datestring);

#define NIOV	24	/* max is currently 24 */

#ifdef DEBUG
	#define addstr(Iov, N, Str) \
		do \
		{ \
			Iov[N].iov_base = (void *)Str; \
			Iov[N].iov_len = strlen(Iov[n].iov_base); \
			if (N >= NIOV) \
			fprintf(stderr,"ERROR: addstr is over iov[%d]: %s\n", NIOV, Str); \
			N++; \
		} while(0)
#else
	#define addstr(Iov, N, Str) \
		do \
		{ \
			Iov[N].iov_base = (void *)Str; \
			Iov[N].iov_len = strlen(Iov[n].iov_base); \
			N++; \
		} while(0)
#endif

/*****************************************************************************/

int http_socket_reconnect(int *a_socket, int use_connect, int hangup)
{
#ifdef NOT_YET
	int ret = 0;

	/* *** since gethostbyname is not thread safe, this operation should
	   only be done at the time the volume is mounted
	   *** */
	/* if the previous connection ended badly re-resolve http_hostname */
	if (hangup)
	{
		ret = resolve_http_hostaddr();
		if (ret)
		{
			warn("hostname resolution");
			return (ret);
		}
	}
#endif

#ifdef DEBUG
	fprintf(stderr, "Attempting Reconnect: socket is %i\n", *a_socket);
#endif

	/* Must ignore errors here on fclose since it is possible that the
	 * socket will have been closed out from under the stream and fclose
	 * would erroneously return an error */

	if (*a_socket >= 0)
	{
		struct linger	lingerval;
		
		/* disgard the send and receive buffers on reconnect close */
		lingerval.l_onoff = 1;
		lingerval.l_linger = 0;
		(void)setsockopt(*a_socket, SOL_SOCKET, SO_LINGER, &lingerval, sizeof(lingerval));
		
		/* close the socket */
		(void)close(*a_socket);
	}
	*a_socket = socket(PF_INET, SOCK_STREAM, 0);

	if (*a_socket < 0)
	{
		warn("socket");
		return EIO;
	}
	
#ifdef DEBUG
	fprintf(stderr, "Recreated socket: socket is %i\n", *a_socket);
#endif

	/*
	 * Some hosts do not correctly handle data in SYN segments.
	 * If no connect(2) is done, the TCP stack will send our
	 * initial request as such a segment.  use_connect works
	 * around these broken server TCPs by avoiding this case.
	 * It is not the default because we want to exercise this
	 * code path, and in any case the majority of hosts handle
	 * our default correctly.
	 */
	if (use_connect &&
		connect(*a_socket, (struct sockaddr *) & http_sin, sizeof(struct sockaddr_in)) < 0)
	{
		warn("connect");
		return EIO;
	}
	
#ifdef DEBUG
	fprintf(stderr, "Reconnected sucessfully \n");
#endif

	return (0);
}

/*****************************************************************************/

/*
 * http_set_socket_sndtimeo sets the send timeout for the socket fd.
 * send_timeout is in seconds. 0 = use default.
 */
static void http_set_socket_sndtimeo(int fd, int32_t send_timeout)
{
	if (fd >= 0)
	{
		struct timeval timeout;
		
		timeout.tv_sec = send_timeout;
		timeout.tv_usec = 0;
		(void)setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
	}
}

/*****************************************************************************/

/*
 *	http_state_free frees the http_state and any related string buffers
 */
static void http_state_free(struct http_state *https)
{
	if (https)
	{
		if (https->http_remote_request)
		{
			free(https->http_remote_request);
		}
		if (https->http_decoded_file)
		{
			free(https->http_decoded_file);
		}
		if (https->http_host_header)
		{
			free(https->http_host_header);
		}
		free(https);
	}
}

/*****************************************************************************/

int http_parse(struct fetch_state *fs, const char *key, int use_proxy)
{
	char *file = NULL,	*slash = NULL;
	struct http_state *https;
	int rv = 0;

	/* error-check key */
	slash = strchr(key, '/');
	if (!slash)
	{
		/* the end of the host name */
		warnx("`%s': malformed `http' URL", key);
		return (EINVAL);
	}
	file = slash;

	https = malloc(sizeof * https);
	if (!https)
	{
		return (ENOMEM);
	}
	bzero(https, sizeof(*https));

	if ((!use_proxy) || (!proxy_ok) || proxy_exception)
	{
		/*
		 * NB: HTTP/1.1 servers MUST also accept a full URI.
		 * However, HTTP/1.0 servers will ONLY accept a trimmed URI.
		 */
		https->http_remote_request = malloc(strlen(file) + 1);
		if (!https->http_remote_request)
		{
			rv = ENOMEM;
			goto out;
		}
		strcpy(https->http_remote_request, file);
	}
	else
	{
		/* for proxy, put the whole uri in http_remote_request */
		https->http_remote_request = malloc(strlen(_WEBDAVPREFIX) + strlen(key) + 1);
		if (!https->http_remote_request)
		{
			rv = ENOMEM;
			goto out;
		}
		sprintf(https->http_remote_request, "%s%s", _WEBDAVPREFIX, key);
	}

	/* put the destination host name in http_host_header */
	https->http_host_header = malloc(sizeof("Host: \r\n") + strlen(dest_server) + 6);
	if (!https->http_host_header)
	{
		/* 5 for the port + 1 for the null termination */
		rv = ENOMEM;
		goto out;
	}
	sprintf(https->http_host_header, "Host: %s:%d\r\n", dest_server, dest_port);

	file++; /* after the "/" at the beginning */
	https->http_decoded_file = malloc(strlen(file) + 1);
	if (!https->http_decoded_file)
	{
		rv = ENOMEM;
		goto out;
	}
	strcpy(https->http_decoded_file, file);
	percent_decode_in_place(https->http_decoded_file);

	/* extract the basename from the decoded version */
	if (fs->fs_outputfile == 0)
	{
		slash = strrchr(https->http_decoded_file, '/');
		fs->fs_outputfile = (slash) ? slash + 1 : https->http_decoded_file;
	}

#ifdef DEBUG
	fprintf(stderr, "http_parse: fs_outputfile = %s\n", fs->fs_outputfile);
#endif

	https->http_redirected = 0;

	fs->fs_proto = https;
	fs->fs_close = http_close;
	fs->fs_retrieve = http_retrieve;
	fs->fs_fd = -1;
	
	return 0;
	
out:
	http_state_free(https);
	fs->fs_proto = NULL; /* set fs->fs_proto (https) to NULL so we'll know it's gone */
	
	return rv;
}

/*****************************************************************************/

static int http_clean_socket(int *remote, off_t total_length, int chunked)
{
	int	result;
	int	download_status;
	int last_chunk;
	
	if ( total_length != 0 )
	{
		/* set fake download_status to download rest of data */
		download_status = WEBDAV_DOWNLOAD_IN_PROGRESS;
		
		/* read rest of socket data into the bit bucket */
		if ( !chunked )
		{
			result = http_read(remote, -1, total_length, &download_status);
		}
		else
		{
			result = http_read_chunked(remote, -1, total_length, &download_status, &last_chunk);
		}
	}
	else
	{
		result = 0;
	}
	
	return ( result );
}

/*****************************************************************************/

static int http_close(struct fetch_state *fs)
{
	http_state_free(fs->fs_proto);
	fs->fs_proto = NULL; /* set fs->fs_proto (https) to NULL so we'll know it's gone */
	fs->fs_outputfile = 0;
	return 0;
}

/*****************************************************************************/

static int nullclose(struct fetch_state *fs)
{
	return 0;
}

/*****************************************************************************/

/*
 * Process a redirection.
 */
static int http_redirect(struct fetch_state *fs, char *new, int permanent, int *download_status)
{
	struct http_state	 *https = fs->fs_proto;
	int num_redirects = https->http_redirected + 1;
	int rv;

	if (num_redirects > 5)
	{

#ifdef DEBUG
		warnx("%s: HTTP redirection limit exceeded", fs->fs_outputfile);
#endif

		return (ENOENT);
	}
	
	http_state_free(fs->fs_proto);
	fs->fs_proto = NULL; /* set fs->fs_proto (https) to NULL so we'll know it's gone */
	
#ifdef DEBUG
	warnx("%s: resource has moved %s to `%s'", fs->fs_outputfile,
		permanent ? "permanently" : "temporarily", new);
#endif

	rv = http_parse(fs, new, proxy_ok);
	if (rv != 0)
	{
		fs->fs_close = nullclose;				/* XXX rethink interface? */
		return rv;
	}
	https = fs->fs_proto;
	https->http_redirected = num_redirects;

	rv = http_retrieve(fs, download_status);
	
	return rv;
}

/*****************************************************************************/

/*
 * Get a file using HTTP.  We will try to implement HTTP/1.1 eventually.
 * This subroutine makes heavy use of the 4.4-Lite standard I/O library,
 * in particular the `fgetln' which allows us to slurp an entire `line'
 * (an arbitrary string of non-NUL characters ending in a newline) directly
 * out of the stdio buffer.	 This makes interpreting the HTTP headers much
 * easier, since they are all guaranteed to end in `\r\n' and we can just
 * ignore the `\r'.
 */
static int http_retrieve(struct fetch_state *fs, int *download_status)
{
	struct http_state    *https = fs->fs_proto;
	int local;
	struct msghdr msg;
	struct iovec iov[NIOV];
	int n,
		status;
	int myreturn = 0;
	char *base64ofmd5 = NULL,
		*new_location = NULL,
		*line = NULL;
	ssize_t linelen;

	off_t last_byte = 0,
		restart_from = 0,
		total_length = -1;

	time_t last_modified,
		when_to_retry;
	int restarting = 0,	/* must remain FALSE until we make sure that the file
							has not been modified since it was cached */
		mirror = fs->fs_mirror,
		continue_received = 0,
		redirection = 0,
		retrying = 0,
		chunked = 0,
		reconnected = 0,
		object_unmodified = 0,
		autherror = 0,
		best_auth_level = 0,
		auth_updated = FALSE,
		no_ui_needed = FALSE,
		last_chunk;
	char *http_auth = NULL,
		*best_auth_challenge = NULL,
		*best_auth_realm = NULL;
	char rangebuf[sizeof("Range: bytes=18446744073709551616-\r\n")];
	char datebuf[30];
	int socket_ours = 1;

	/* allocate the line buffer */
	line = malloc(MAX_HTTP_LINELEN);
	if ( line == NULL )
	{
		myreturn = EIO;
		goto out;
	}
	
	fs->fs_status = "creating request message";
	msg.msg_name = (caddr_t) & http_sin;
	msg.msg_namelen = sizeof http_sin;
	msg.msg_iov = iov;
	msg.msg_control = 0;
	msg.msg_controllen = 0;
	msg.msg_flags = 0;

retry:

	myreturn = 0;

	/* the server might hang up on us up to once per transaction */
	reconnected = 0;

	n = 0;

	if (http_auth)
	{
		free(http_auth);
	}
	http_auth = NULL;
	{
		WebdavAuthcacheRetrieveRec http_auth_struct =
		{
			fs->fs_uid, https ->http_remote_request, append_to_file, "GET", NULL
		};
		if (!webdav_authcache_retrieve(&http_auth_struct))
			http_auth = http_auth_struct.authorization;
	}

	addstr(iov, n, "GET ");
	addstr(iov, n, https->http_remote_request);
	if (append_to_file)
	{
		addstr(iov, n, append_to_file);
	}
	addstr(iov, n, " HTTP/1.1\r\n");
	/*
	 * The choice of HTTP/1.1 may be a bit controversial.  The 
	 * specification says that implementations which are not at
	 * least conditionally compliant MUST NOT call themselves
	 * HTTP/1.1.  We choose not to comply with that requirement.
	 * (Eventually we will support the full HTTP/1.1, at which
	 * time this comment will not apply.  But it's amusing how
	 * specifications attempt to define behavior for implementations
	 * which aren't obeying the spec in the first place...)
	 */
	addstr(iov, n, gUserAgentHeader);
	/* do content negotiation here */
	addstr(iov, n, "Accept: */*\r\n");
	addstr(iov, n, https->http_host_header);
	if (http_auth)
	{
		addstr(iov, n, http_auth);
	}
	
	if (mirror || restarting)
	{
		format_http_date(fs->fs_st_mtime, datebuf);
	}
	
	if (mirror)
	{
		errno = 0;
		addstr(iov, n, "If-Modified-Since: ");
		addstr(iov, n, datebuf);
		addstr(iov, n, "\r\n");
	}

	if (restarting)
	{
		struct stat stab;

		errno = 0;
		if ((fstat(fs->fs_fd, &stab) == 0) && S_ISREG(stab.st_mode))
		{
			addstr(iov, n, "If-Range: ");
			addstr(iov, n, datebuf);
			addstr(iov, n, "\r\n");
			sprintf(rangebuf, "Range: bytes=%qd-\r\n", (long long)stab.st_size);
			addstr(iov, n, rangebuf);
		}
		else if (errno != 0 || !S_ISREG(stab.st_mode))
		{
			if (errno != 0)
			{
				warn("%s", fs->fs_outputfile);
			}
			else
			{
				warnx("%s: not a regular file", fs->fs_outputfile);
			}
			restarting = 0;
			warnx("cannot restart; will retrieve anew");
		}
	}

	addstr(iov, n, "\r\n");

	/* open the socket if needed */
	if (*(fs->fs_socketptr) < 0)
	{
		if (http_socket_reconnect(fs->fs_socketptr, fs->fs_use_connect, 0))
		{
			myreturn = EIO;
			goto out;
		}
	}

	/* use WEBDAV_IO_TIMEOUT */
	http_set_socket_sndtimeo(*(fs->fs_socketptr), WEBDAV_IO_TIMEOUT);

reconnect:

	msg.msg_iovlen = n;

	if (n >= NIOV)
	{
		myreturn = EIO;
		goto out;
	}
	fs->fs_status = "sending request message";

	if (logfile)
	{
		fprintf(logfile, "%s", msg.msg_iov[0].iov_base);
		fprintf(logfile, "%s On Socket %d", msg.msg_iov[1].iov_base, *(fs->fs_socketptr));
		if (mirror)
		{
			fprintf(logfile, "Only if Modified\n");
		}
		else
		{
			fprintf(logfile, "\n");
		}
	}

	if (sendmsg(*(fs->fs_socketptr), &msg, 0) < 0)
	{
		if (!reconnected)
		{
#ifdef DEBUG
			fprintf(stderr, "sendmsg errno was %d\n", errno);
#endif

			if (http_socket_reconnect(fs->fs_socketptr, fs->fs_use_connect, 1))
			{
				myreturn = EIO;
				goto out;
			}
			reconnected = 1;
			goto reconnect;
		}
		else
		{
			warnx("sendmsg (http_retrieve): error after reconnect to %s", http_hostname);
			myreturn = EIO;
			goto out;
		}
	}

got100reply:

	fs->fs_status = "reading reply status";
	linelen = socket_read_line(*(fs->fs_socketptr), line, MAX_HTTP_LINELEN);
	if (linelen == 0)
	{
		/*
		 * The server probably tried to shut down the connection so
		 * try the sendmsg again.  If we're right, it will reconnect
		 * on the sendmsg error.
		 */

		if (!reconnected)
		{
#ifdef DEBUG
			fprintf(stderr, "errno was %d\n", errno);
#endif

			if (http_socket_reconnect(fs->fs_socketptr, fs->fs_use_connect, 1))
			{
				myreturn = EIO;
				goto out;
			}
			reconnected = 1;
			goto reconnect;
		}
		else
		{
			warnx("empty reply from %s", http_hostname);
			myreturn = EIO;
			goto out;
		}
	}

	/*
	 * The other end needs to be doing HTTP 1.0 at the very least,
	 * which is checked in http_first_line().
	 */

	autherror = 0;
	status = http_first_line(line);

	if (logfile)
	{
		fprintf(logfile, "%s  On Socket %d\n", line, *(fs->fs_socketptr));
	}

	if (status == -1)
	{
		if (continue_received)
		{
			goto got100reply;
		}
		else
		{
			/* it was HTTP .9 */
			myreturn = EIO;
			goto out;
		}
	}
	continue_received = 0;

	total_length = -1;							/* -1 means ``don't know' */
	object_unmodified = 0;

	/* handle all informational 1xx status codes the same */
	if ( (status > 100) && (status <= 199) )
	{
		status = 100;
	}
	
	switch (status)
	{
		case 100:								/* Continue */
			/* read until we get a legal first line */
			continue_received = 1;
			goto got100reply;

		case 200:								/* Here come results */
		case 203:								/* Non-Authoritative Information */
			break;

		case 206:								/* Here come partial results */
			/* can only happen when restarting */
			break;

		case 301:								/* Resource has moved permanently */
		case 302:								/* Resource has moved temporarily */
			/*
			 * We formerly didn't test fs->fs_auto_retry here,
			 * so that this sort of redirection would be transparent
			 * to the user.  Unfortunately, there are a lot of idiots
			 * out there running Web sites, and some of them have
			 * decided to implement the following stupidity: rather
			 * than returning the correct `404 Not Found' error
			 * when something is not found, they instead return
			 * a 302 redirect, giving the erroneous impression that
			 * the requested resource actually exists.  This
			 * breaks any client which expects a non-existent resource
			 * to elicit a 40x response.  Grrr.
			 */
			if (!fs->fs_auto_retry)
			{
				/* -A flag */
#ifdef DEBUG
				fprintf(stderr, "Resource moved %d: %s\n", status, line);
#endif

				myreturn = ENOENT;
			}
			else
			{
				redirection = status;
			}
			break;
		case 304:								/* Object is unmodified */
			if (mirror)
			{
				/* Good news, there is nothing to get so 
				 * indicate no length and proceed to get rid
				 * of the rest of the http header
				 */

				total_length = 0;
				object_unmodified = 1;
			}
			else
			{
				myreturn = EIO;
			}
			break;

		case 400:
			myreturn = EINVAL;
			break;

		case 401:								/* Unauthorized */
		case 407:								/* Proxy Authentication Required */
			autherror = status;
			break;

		case 403:
			myreturn = EPERM;
			break;

		case 404:
			myreturn = ENOENT;
			break;

		case 501:								/* Not Implemented */
#ifdef DEBUG
			fprintf(stderr, "Not Implemented %d: %s\n", status, line);
#endif

			myreturn = ENOENT;
			break;

		case 503:								/* Service Unavailable */
#ifdef DEBUG
			fprintf(stderr, "Service Unavailable %d: %s\n", status, line);
#endif

			if (!fs->fs_auto_retry)
			{
				myreturn = ENOENT;
			}
			else
			{
				retrying = 503;
			}
			break;

		case 507:
			myreturn = ENOSPC;
			break;

		default:
#ifdef DEBUG
			fprintf(stderr, "Unknown error %d: %s\n", status, line);
#endif

			myreturn = ENOENT;
			break;
	}

	last_modified = when_to_retry = -1;
	restart_from = 0;
	chunked = 0;
	fs->fs_status = "parsing reply headers";

	while ((linelen = socket_read_line(*(fs->fs_socketptr), line, MAX_HTTP_LINELEN)) != 0)
	{
		char *value,  *ep;
		enum http_header header;
		unsigned long ul;

		header = http_parse_header(line, &value);
		if (header == ht_end_of_header)
		{
			break;
		}

		switch (header)
		{
			case ht_connection:
				/* if "connection: close", set the https->connection_close */
				if (strncasecmp(value, "close", 5) == 0)
				{
					https->connection_close = 1;
				}
				break;
				
			case ht_content_length:
				errno = 0;
				ul = strtoul(value, &ep, 10);
				if (errno != 0 || *ep)
				{
					warnx("invalid Content-Length: `%s'", value);
				}
				if (!object_unmodified)
				{
					total_length = (off_t)ul;
				}
				break;

			case ht_last_modified:
				last_modified = parse_http_date(value);
#ifdef DEBUG
				if (last_modified == -1)
				{
					warnx("invalid Last-Modified: `%s'", value);
				}
#endif

				break;

			case ht_content_md5:
				if (base64ofmd5)
				{
					free(base64ofmd5);
				}
				base64ofmd5 = malloc(strlen(value) + 1);
				if (!base64ofmd5)
				{
					if (!myreturn)
					{
						myreturn = ENOMEM;
					}
				}
				else
				{
					strcpy(base64ofmd5, value);
				}
				break;

			case ht_content_range:
				if (!restarting)				/* XXX protocol error */
				{
					break;
				}

				/* NB: we might have to restart from farther back
				  than we asked. */
				status = parse_http_content_range(value, &restart_from, &last_byte);
				/* If we couldn't understand the reply, get the whole
				  thing. */
				if (status)
				{
					restarting = 0;
					myreturn = EAGAIN;			/* goto retry later */
				}
				break;

			case ht_location:
				if (redirection)
				{
					int len;
					char *s = value + strlen(_WEBDAVPREFIX);
					/* assuming there is an "http://" in value */
					while (*s && !isspace(*s))
					{
						s++;
					}
					len = s - value - strlen(_WEBDAVPREFIX);
					if (new_location)
					{
						free(new_location);
					}
					new_location = malloc(len + 1);
					if (!new_location)
					{
						if (!myreturn)
						{
							myreturn = ENOMEM;
						}
					}
					else
					{
						strncpy(new_location, value + strlen(_WEBDAVPREFIX), len);
						new_location[len] = '\0';
					}
				}
				break;

			case ht_transfer_encoding:
				if (strncasecmp(value, "chunked", 7) == 0)
				{
					chunked = 1;
					break;
				}
				warnx("%s: %s specified Transfer-Encoding `%s'", fs->fs_outputfile,
					http_hostname, value);
				warnx("%s: output file may be uninterpretable", fs->fs_outputfile);
				break;

			case ht_retry_after:
				if (!retrying)
				{
					break;
				}

				errno = 0;
				ul = strtoul(value, &ep, 10);
				if (errno != 0 || (*ep && !isspace(*ep)))
				{
					time_t when;
					when = parse_http_date(value);
					if (when == -1)
					{
						break;
					}
					when_to_retry = when;
				}
				else
				{
					when_to_retry = time(0) + ul;
				}
				break;

			case ht_www_authenticate:
			case ht_proxy_authenticate:
				if (((header == ht_www_authenticate) && (autherror != 401)) ||
					((header == ht_proxy_authenticate) && (autherror != 407)))
				{
					break;
				}

				/* if we're still looking for the best scheme */
				if (!auth_updated)
				{
					WebdavAuthcacheEvaluateRec auth_eval =
					{
						fs->fs_uid, value, (autherror == 407), 0, FALSE, FALSE, NULL
					};
					int error = webdav_authcache_evaluate(&auth_eval);

					if (!error)
					{
						if (auth_eval.updated)
						{
							/* no authcache_insert is needed, and this  
							  is the best authentication scheme,
							  so look no further */
							auth_updated = TRUE;
						}
						else
						{
							if (auth_eval.uiNotNeeded)
							{
								no_ui_needed = TRUE;
							}
							/* Now find out what kind of authentication scheme
							  based on the level reported */
							if (auth_eval.level > best_auth_level)
							{
								best_auth_level = auth_eval.level;
								if (best_auth_challenge)
								{
									free(best_auth_challenge);
								}
								best_auth_challenge = malloc(strlen(value) + 1);
								if (!best_auth_challenge)
								{
									if (!myreturn)
									{
										myreturn = ENOMEM;
									}
								}
								else
								{
									strcpy(best_auth_challenge, value);
								}
								if (best_auth_realm)
								{
									free(best_auth_realm);
								}
								best_auth_realm = auth_eval.realmStr;
							}
							else
							{
								if (auth_eval.realmStr)
								{
									free(auth_eval.realmStr);
								}
							}
						}
					}
				}
				break;

			default:
				break;
		}
	}
	if (myreturn == EAGAIN)
	{
		goto retry;
	}

	if (myreturn)
	{
		goto out;
	}

	if (autherror)
	{
		status = 0;
		if (!auth_updated)
		{
			/* If it's not been updated, an alert/insert may be needed. */
			if (best_auth_level == 0)
			{
				status = EACCES;
			}
			else
			{
				char user[WEBDAV_MAX_USERNAME_LEN];
				char pass[WEBDAV_MAX_PASSWORD_LEN];

				bzero(user, sizeof(user));
				bzero(pass, sizeof(pass));
				if (!no_ui_needed)
				{
					char *allocatedURL = NULL;
					char *displayURL;
					
					if ( autherror == 401 )
					{
						/* 401 Unauthorized error */
						if ( reconstruct_url(http_hostname, https->http_remote_request, &allocatedURL) == 0 )
						{
							/* use allocatedURL for displayURL */
							displayURL = allocatedURL;
						}
						else
						{
							/* this shouldn't happen, but if it does, use http_remote_request */
							allocatedURL = NULL;
							displayURL = https->http_remote_request;
						}
					}
					else
					{
						/* must be 407 Proxy Authentication Required error */
						/* use proxy_server for displayURL */
						displayURL = proxy_server;
					}
					/* put up prompt for best_auth_level */
					status = webdav_get_authentication(user, sizeof(user), pass, sizeof(pass),
						(const char *)displayURL, (const char *)best_auth_realm, best_auth_level);
					/* for any error response, set status to EACCES */
					if (status)
					{
						status = EACCES;
					}
					/* free allocatedURL if allocated */
					if ( allocatedURL != NULL )
					{
						free(allocatedURL);
					}
				}
				if (!status)
				{
					/* insert authcache entry for best_auth_sofar */

					WebdavAuthcacheRemoveRec auth_rem =
					{
						fs->fs_uid, (autherror ==407), best_auth_realm
					};
					WebdavAuthcacheInsertRec auth_insert =
					{
						fs->fs_uid, best_auth_challenge, best_auth_level, (autherror == 407),
							user, pass
					};

					/* Whatever authorization we had before is no longer 
					  valid so remove it from the cache.  If geting
					  rid of it doesn't work, ignore it. */
					(void)webdav_authcache_remove(&auth_rem);
					if (fs->fs_uid != 0)
					{
						auth_rem.uid = 0;
						(void)webdav_authcache_remove(&auth_rem);
					}
					if (fs->fs_uid != 1)
					{
						auth_rem.uid = 1;
						(void)webdav_authcache_remove(&auth_rem);
					}

					status = webdav_authcache_insert(&auth_insert);
					if (!status)
					{
						/* if not "root", make an entry for root */
						if (fs->fs_uid != 0)
						{
							auth_insert.uid = 0;
							(void)webdav_authcache_insert(&auth_insert);
						}
						/* if not "daemon", make an entry for daemon */
						if (fs->fs_uid != 1)
						{
							auth_insert.uid = 1;
							(void)webdav_authcache_insert(&auth_insert);
						}
					}
				}
				bzero(user, sizeof(user));
				bzero(pass, sizeof(pass));
			}
		}
		if (!myreturn)
		{
			myreturn = (status) ? status : EAUTH;
		}
		if (myreturn == EAUTH)
		{
			/* success with authentication */
			/* now, we need to eat the entity-body (if any) and retry */
			if (chunked)
			{
				total_length = -1;
			}
			
			if (total_length != 0)
			{
				/* just eat all of the data right now */
				status = http_clean_socket(fs->fs_socketptr, total_length, chunked);
				if (status != 0)
				{
					myreturn = status;
					goto out;
				}
			}
			goto retry;
		}
		else
		{
			if (!myreturn)
			{
				myreturn = EACCES;
			}
			goto out;
		}
	}

	if (retrying)
	{
		int howlong;

		if (when_to_retry == -1)
		{
#ifdef DEBUG
			fprintf(stderr, "%s: HTTP server returned HTTP/1.1 503 Service Unavailable\n",
				fs->fs_outputfile);
#endif

			myreturn = ENOENT;
			goto out;
		}

		howlong = when_to_retry - time(0);
		if (howlong < 30)
		{
			howlong = 30;
		}
#ifdef DEBUG
		warnx("%s: service unavailable; retrying in %d seconds", http_hostname, howlong);
#endif

		fs->fs_status = "waiting to retry";
		sleep(howlong);
		goto retry;
	}

	if (myreturn)
	{
		/* *** might we be leaving something on the socket here? *** */
		goto out;
	}

	if (redirection)
	{
		if (new_location)
		{
			fs->fs_status = "processing redirection";
			status = http_redirect(fs, new_location, redirection == 301, download_status);
			myreturn = status;
		}
		else
		{
#ifdef DEBUG
			warnx("%s: redirection but no new location", fs->fs_outputfile);
#endif

			myreturn = ENOENT;
		}
		goto out;
		/* *** might we be leaving something on the socket here? *** */
	}

	fs->fs_status = "retrieving file from HTTP/1.x server";

	/*
	 * OK, if we got here, then we have finished parsing the header
	 * and have read the `\r\n' line which denotes the end of same.
	 * We may or may not have a good idea of the length of the file
	 * or its modtime.  At this point we will have to deal with
	 * any special byte-range, content-negotiation, redirection,
	 * or authentication, and probably jump back up to the top,
	 * once we implement those features.  So, all we have left to
	 * do is open up the output file and copy data from input to
	 * output until EOF. 
	 */

	/* 
	  Reset status to 0 since we will be checking it later and we 
	  don't want to confuse the status from the header for the return 
	  from some of the functions.
	*/

	status = 0;

	local = fs->fs_fd;
	if (local == -1)
	{
		myreturn = EIO;
		goto out;
	}

	if (mirror && object_unmodified)
	{
		if (fs->fs_restart)
		{
			restarting = TRUE;
			mirror = FALSE;
			goto retry;
		}
		else
		{
			goto free_local;
		}
	}

	/* It this was a get-if-modified where the file was modified, or we are
	  restarting, we will need to truncate the file to keep the kernel from 
	  getting old data.
	*/

	if (mirror || restarting)
	{
		/* Since we are truncating and essentially starting over, clear
		 * the cache file flags */

		(void)fchflags(local, 0);

		if (ftruncate(local, restart_from))
		{
			myreturn = EIO;
			goto free_local;
		}
	}
	(void)lseek(local, restart_from, SEEK_SET);

	/* According to the spec, if content is chunked, content-length must be
	  ignored. */
	if (chunked)
	{
		total_length = -1;
	}

	if (total_length == 0)
	{
		*download_status = WEBDAV_DOWNLOAD_FINISHED;
	}
	else
	{
		/* Initialize the download status. */
		*download_status = WEBDAV_DOWNLOAD_IN_PROGRESS;

		if (total_length == -1 || total_length > MAX(WEBDAV_DOWNLOAD_LIMIT, webdav_first_read_len))
		{

			/* If we're going to need to do a background download, read the
			  first webdav_first_read_len now, before the open() returns. */
			last_chunk = FALSE;
			if (chunked)
			{
				status = http_read_chunked(fs->fs_socketptr, local, webdav_first_read_len,
					download_status, &last_chunk);
			}
			else
			{
				status = http_read(fs->fs_socketptr, local, webdav_first_read_len,
					download_status);
			}
			
			if (status)
			{
				*download_status = WEBDAV_DOWNLOAD_ABORTED;
				myreturn = EIO;
				goto free_local;
			}
			
			if ( last_chunk )
			{
				/* we were reading chunked and found the last-chunk flag */
				*download_status = WEBDAV_DOWNLOAD_FINISHED;
				goto download_finished;
			}
			else
			{
				if (total_length != -1)
				{
					total_length -= webdav_first_read_len;
				}
				
				status = http_background_load(fs->fs_socketptr, local, total_length, chunked,
					download_status);
				if (status)
				{
					*download_status = WEBDAV_DOWNLOAD_ABORTED;
					myreturn = EIO;
					goto free_local;
				}
	
				/* Ok, so we have now given away our socket (remote File ptr & 
				it's underlying fd) away to be used by a download thread.  
				That means we need new ones for this thread so that the next 
				request it gets will not interfere.  Closing the socket will 
				be in the new threads hands.
				*/
	
				/* so that we won't close the socket we gave away, in 
				http_socket_reconnect() */
				socket_ours = 0;
				
				/* and so we won't attempt to use this socket on another thread */
				*(fs->fs_socketptr) = -1;
				
				goto keep_local;
			}
		}
		else
		{
			/* just read all of the data right now */
			status = http_read(fs->fs_socketptr, local, total_length, download_status);

			/* Even if there is an error, clear the download status so 
			  that close will never end up waiting for something which 
			  will never happen. 
			*/
			if (status)
			{
				*download_status = WEBDAV_DOWNLOAD_ABORTED;
				myreturn = EIO;
				goto free_local;
			}
			else
			{
				*download_status = WEBDAV_DOWNLOAD_FINISHED;
			}
		}
	}

download_finished:

	if (base64ofmd5)
	{
		/*
		 * Ack.  When restarting, the MD5 only covers the parts
		 * we are getting, not the whole thing.
		 */
		(void)lseek(local, restart_from, SEEK_SET);
		fs->fs_status = "computing MD5 message digest";
		status = check_md5(local, base64ofmd5);
	}

free_local:

	(void)close(local);

keep_local:

	if (!myreturn)
	{
		if (last_modified != -1)
		{
			fs->fs_st_mtime = last_modified;
		}
		else
		{
			if (!restarting)
			{
				fs->fs_st_mtime = 0;
				/* back in webdav_open the current time will be filled in */
			}
		}
	}

out:

	/* *** check whether the server has closed the connection *** */
	if (socket_ours)
	{
		if ((*(fs->fs_socketptr) >= 0) && ((proxy_ok && (!proxy_exception)) || https->connection_close))
		{
			(void)close(*(fs->fs_socketptr));
			*(fs->fs_socketptr) = -1;
		}
	}

	if (new_location)
	{
		free(new_location);
	}
	if (base64ofmd5)
	{
		free(base64ofmd5);
	}
	if (best_auth_challenge)
	{
		free(best_auth_challenge);
	}
	if (best_auth_realm)
	{
		free(best_auth_realm);
	}
	if (http_auth)
	{
		free(http_auth);
	}
	if (line)
	{
		free(line);
	}

	return myreturn;
}												/* http_retrieve */

/*****************************************************************************/

/*
 *	Read the HTTP response in standard form into a file.
 *
 *	Input:
 *		remote			Pointer to socket to read from.
 *		local			The file to write to, or -1 if the data should be
 *						disgarded.
 *		total_length	If -1, read data from the socket until data stops
 *						coming from the server (i.e., the server closes the
 *						connection).
 *						If not -1, then this is the maximum amount of data
 *						to read from the server.
 *						This will NOT be 0.
 *		download_status	A pointer to a download_status variable. It's checked
 *						during processing of the data to determine if processing
 *						should be stopped.
 *
 *	Results:
 *		0				No errors.
 *		EIO				Error. The socket is closed on errors
 */
int http_read(int *remote, int local, off_t total_length, int *download_status)
{
	char	buffer[BUFFER_SIZE];
	ssize_t	read_result;
	ssize_t	write_result;
	off_t	remaining_bytes;
	
	/* There are two cases to handle:
	 * 1 -	We don't know how much data is coming (total_length == -1) so
	 *		we process until data stops coming from the server
	 *		(the server closes the connection).
	 * 2 -	We know how much data is coming (total_length != -1) so
	 *		we read and write total_length bytes.
	 */
	
	/* set up remaining_bytes */
	if (total_length == -1)
	{
		remaining_bytes = sizeof(buffer);
	}
	else
	{
		remaining_bytes = total_length;
	}
	
	do
	{
		/* If the download is terminated before we're done,
		 * exit with an error.
		 */
		if ( *download_status == WEBDAV_DOWNLOAD_TERMINATED )
		{
			goto error_exit;
		}
		
		read_result = socket_read_bytes(*remote, buffer, MIN(sizeof(buffer), remaining_bytes));
		if ( read_result > 0 )
		{
			/* if we know the total_length */
			if ( total_length != -1 )
			{
				/* then decrement remaining_bytes */
				remaining_bytes -= read_result;
			}
			/* else leave remaining_bytes at full buffer size */
		}
		else if ( read_result == 0 )
		{
			/* there are no more bytes to read */
			
			if ( total_length == -1 )
			{
				/* total_length was unknown and the server quit sending
				 * so we should be successful.
				 */
				break;
			}
			else
			{
				/* total_length was known, but the socket was closed on
				 * the server before we received all of the data.
				 * Exit with error.
				 */
				warnx("server closed connection before all data was received");
				goto error_exit;
			}
		}
		else
		{
			/* socket_read_bytes returned an error */
			goto error_exit;
		}
		
		/* are we writing to local file or eating the data? */
		if ( local != -1 )
		{
			/* writing the data */
			write_result = write(local, buffer, read_result);
		}
		else
		{
			/* eating the data */
			write_result = read_result;
		}

	} while ( (write_result == read_result) && (remaining_bytes > 0) );
	
	return ( 0 );
	
error_exit:
	/* close the socket */
	(void)close(*remote);
	*remote = -1;
	
	return ( EIO );
}

/*****************************************************************************/

/*
 *	Read the HTTP body in chunked form into a file.
 *
 *	Input:
 *		remote			Pointer to socket to read from.
 *		local			The file to write to, or -1 if the data should be
 *						disgarded.
 *		total_length	If -1, read data from the socket until data stops
 *						coming from the server (i.e., the server closes the
 *						connection).
 *						If not -1, then this is the amount of chunked data
 *						to read from the server before stopping (Note, more
 *						data than total_length may be processed if the last
 *						chunk processed pushes us past total_length.)
 *		download_status	A pointer to a download_status variable. It's checked
 *						during processing of the data to determine if processing
 *						should be stopped.
 *
 *	Output:
 *		last_chunk		If true, the last-chunk was found and processed.
 *
 *	Results:
 *		0				No errors.
 *		EIO				Error. The socket is closed on errors
 */
int http_read_chunked(int *remote, int local, off_t total_length,
	int *download_status, int *last_chunk)
{
	char	buffer[BUFFER_SIZE];
	ssize_t	read_result;
	ssize_t	write_result;
	ssize_t	line_length;
	size_t	bytes_to_read;
	off_t	total_read;
	u_long	chunk_size;
	char	*line = NULL;
	char	*ep;
	
	/* allocate the line buffer */
	line = malloc(MAX_HTTP_LINELEN);
	if ( line == NULL )
	{
		goto error_exit;
	}

	/* the last-chunk indicator has not been found */
	*last_chunk = FALSE;
	
	/* no bytes read yet */
	total_read = 0;
	
	/* There are two cases to handle:
	 * 1 -	We want to read all of the chunked data (total_length == -1) so
	 *		we read and write chunks until we get the last-chunk indicator.
	 * 2 -	We want to read the first part of the chucked (total_length != -1) so
	 *		we read and write chunks until we process over total_length bytes,
	 *		or until we get the last-chunk indicator.
	 * (we won't get called if total_length == 0)
	 */
	
	while ( (total_length == -1) || (total_read < total_length) )
	{
		/* read the line containing chunk-size and parse the chunk-size out of it */
		line_length = socket_read_line(*remote, line, MAX_HTTP_LINELEN);
		if (line_length == 0)
		{
			goto error_exit;
		}
		
		zero_trailing_spaces(line);
		errno = 0;
		chunk_size = strtoul(line, &ep, 16);
		if ( errno || (*line == 0) || (*ep && !isspace(*ep) && *ep != ';') )
		{
			goto error_exit;
		}
		
		/* is this the last-chunk marker? */
		if (chunk_size == 0)
		{
			/* yes, so set the last_chunk flag to TRUE and break */
			*last_chunk = TRUE;
			break;
		}
				
		/* process the chunk */
		total_read += chunk_size;
		while ( chunk_size > 0 )
		{
			/* If the download is terminated before we're done,
			 * exit with an error.
			 */
			if ( *download_status == WEBDAV_DOWNLOAD_TERMINATED )
			{
				goto error_exit;
			}
			
			/* read the chunk */
			read_result = socket_read_bytes(*remote, buffer, MIN(sizeof(buffer), chunk_size));
			if ( read_result <= 0 )
			{
				goto error_exit;
			}
			chunk_size -= read_result;
			
			/* writing to local file or eating it? */
			if ( local != -1 )
			{
				/* writing it */
				write_result = write(local, buffer, read_result);
				if (write_result != read_result)
				{
					goto error_exit;
				}
			}
		}

		/*
		 * Read the CRLF after the chunk-data. Because the CR and LF might not
		 * both be available at the same time, make sure we get them both.
		 */
		bytes_to_read = 2;	/* the CR and LF */
		do
		{
			read_result = socket_read_bytes(*remote, line, bytes_to_read);
			if ( read_result <= 0 )
			{
				goto error_exit;
			}
			bytes_to_read -= read_result;
		} while ( bytes_to_read != 0 );
		
	};
	
	if ( *last_chunk  )
	{
		/* If last-chunk was found, then ignore any trailer
		 * lines which come across.
		 */
		do
		{
			line_length = socket_read_line(*remote, line, MAX_HTTP_LINELEN);
			zero_trailing_spaces(line);
			if ( *line == '\0' )
			{
				line_length = 0;
			}
		} while ( line_length > 0 );
	}
	
	if (line)
	{
		free(line);
	}
	
	return ( 0 );
	
error_exit:

	/* close the socket */
	(void)close(*remote);
	*remote = -1;
	
	if (line)
	{
		free(line);
	}
	
	return ( EIO );
}

/*****************************************************************************/

/*
 *	Read the HTTP body in standard form.
 *
 *	Input:
 *		remote			Pointer to socket to read from.
 *
 *	Outputs:
 *		body			The buffer containing the HTTP body is returned by
 *						this parameter. The caller is responsible for freeing
 *						this buffer.
 *		total_length	The length of the data in the body buffer is returned
 *						by this parameter.
 *	Results:
 *		0				No errors.
 *		EIO				Error. The socket is closed on errors
 */
static int http_read_body(int *remote, char **body, size_t *total_length)
{
	size_t	buffer_size;
	size_t	malloc_size;
	size_t	bytes_to_read;
	ssize_t	read_result;
	size_t	total_read;
	char	*previous_buffer;
	char	*buffer;
		
	/* allocate buffer */
	buffer_size = BODY_BUFFER_SIZE;
	buffer = malloc(buffer_size);
	if ( buffer == NULL )
	{
		goto error_exit;
	}
	
	/* malloc_size is the size to grow to if the initial buffer isn't big enough */
	malloc_size = buffer_size * 2;
	
	/* no bytes in buffer yet */
	total_read = 0;
	
	while ( 1 )
	{
		/* calculate number of bytes to read this time (space in buffer) */
		bytes_to_read = buffer_size - total_read;
		
		read_result = socket_read_bytes(*remote, buffer + total_read, bytes_to_read);
		if ( read_result > 0 )
		{
			/* increment total_read */
			total_read += read_result;
			
			/* was buffer completely filled? */
			if ( read_result == bytes_to_read )
			{
				/* yes, so malloc a larger buffer for next read */
				previous_buffer = buffer;
				buffer = malloc(malloc_size);
				if ( buffer == NULL )
				{
					buffer = previous_buffer;
					goto error_exit;
				}
				
				/* copy previous_buffer to buffer */
				memcpy(buffer, previous_buffer, total_read);
				
				/* save new buffer size */
				buffer_size = malloc_size;
				
				/* double malloc_size for the next malloc (if any) */
				malloc_size *= 2;
			}
		}
		else if ( read_result == 0 )
		{
			/* there are no more bytes to read */
			break;
		}
		else
		{
			/* socket_read_bytes returned an error */
			goto error_exit;
		}
		
	}

	*body = buffer;
	*total_length = total_read;
	
	return ( 0 );
	
error_exit:
	
	/* free buffer */
	if ( buffer != NULL )
	{
		free(buffer);
	}
	
	/* close the socket */
	(void)close(*remote);
	*remote = -1;
	
	/* clear outputs */
	*body = NULL;
	*total_length = 0;
	
	return ( EIO );
}

/*****************************************************************************/

/*
 *	Read the HTTP body in chunked form.
 *
 *	Input:
 *		remote			Pointer to socket to read from.
 *
 *	Outputs:
 *		body			The buffer containing the HTTP body is returned by
 *						this parameter. The caller is responsible for freeing
 *						this buffer.
 *		total_length	The length of the data in the body buffer is returned
 *						by this parameter.
 *	Results:
 *		0				No errors.
 *		EIO				Error. The socket is closed on errors
 */
static int http_read_body_chunked(int *remote, char **body, size_t *total_length)
{
	size_t	buffer_size;
	size_t	malloc_size;
	size_t	bytes_to_read;
	ssize_t	read_result;
	size_t	total_read;
	char	*previous_buffer;
	char	*buffer;
	u_long	chunk_size;
	char	*line = NULL;
	char	*ep;
	ssize_t	line_length;
	
	buffer = NULL;
	line = NULL;
	
	/* allocate the line buffer */
	line = malloc(MAX_HTTP_LINELEN);
	if ( line == NULL )
	{
		goto error_exit;
	}

	/* allocate buffer */
	buffer_size = BODY_BUFFER_SIZE;
	buffer = malloc(buffer_size);
	if ( buffer == NULL )
	{
		goto error_exit;
	}
	
	/* no bytes in buffer yet */
	total_read = 0;
	
	do
	{
		/* read the line containing chunk-size and parse the chunk-size out of it */
		line_length = socket_read_line(*remote, line, MAX_HTTP_LINELEN);
		if (line_length == 0)
		{
			goto error_exit;
		}
		
		zero_trailing_spaces(line);
		errno = 0;
		chunk_size = strtoul(line, &ep, 16);
		if ( errno || (*line == 0) || (*ep && !isspace(*ep) && *ep != ';') )
		{
			goto error_exit;
		}
		
		/* is this the last-chunk marker? */
		if (chunk_size == 0)
		{
			/* break out of the do loop */
			break;
		}
		
		/* does the buffer need to be made larger? */
		if ( (buffer_size - total_read) < chunk_size )
		{
			size_t	amount_to_add;
			
			/* calculate malloc_size */ 
			amount_to_add = BODY_BUFFER_SIZE;
			while ( buffer_size - total_read + amount_to_add < chunk_size )
			{
				amount_to_add += BODY_BUFFER_SIZE;
			}
			malloc_size = buffer_size + amount_to_add;
			
			/* malloc a larger buffer */
			previous_buffer = buffer;
			buffer = malloc(malloc_size);
			if ( buffer == NULL )
			{
				buffer = previous_buffer;
				goto error_exit;
			}
			
			/* copy previous_buffer to buffer */
			memcpy(buffer, previous_buffer, total_read);
			
			/* save new buffer size */
			buffer_size = malloc_size;
		}
		
		/* the buffer is big enough for this chunk */
		while ( chunk_size > 0 )
		{
			/* read the chunk */
			read_result = socket_read_bytes(*remote, buffer + total_read, chunk_size);
			if ( read_result <= 0 )
			{
				goto error_exit;
			}
			total_read += read_result;
			chunk_size -= read_result;
		}

		/*
		 * Read the CRLF after the chunk-data. Because the CR and LF might not
		 * both be available at the same time, make sure we get them both.
		 */
		bytes_to_read = 2;	/* the CR and LF */
		do
		{
			read_result = socket_read_bytes(*remote, line, bytes_to_read);
			if ( read_result <= 0 )
			{
				goto error_exit;
			}
			bytes_to_read -= read_result;
		} while ( bytes_to_read != 0 );
		
	} while ( 1 );
	
	/*
	 * If we got here, then we successfully read every chunk and got
	 * the last-chunk indicator. Now we have to ignore any trailer
	 * lines which come across.
	 */
	do
	{
		line_length = socket_read_line(*remote, line, MAX_HTTP_LINELEN);
		zero_trailing_spaces(line);
		if ( *line == '\0' )
		{
			line_length = 0;
		}
	} while ( line_length > 0 );
	
	*body = buffer;
	*total_length = total_read;
	
	if ( line != NULL )
	{
		free(line);
	}
	
	return ( 0 );
	
error_exit:

	/* free buffer */
	if ( buffer != NULL )
	{
		free(buffer);
	}
	
	if ( line != NULL )
	{
		free(line);
	}
	
	/* close the socket */
	(void)close(*remote);
	*remote = -1;
	
	/* clear outputs */
	*body = NULL;
	*total_length = 0;
	
	return ( EIO );
}

/*****************************************************************************/

/* Set up a request queue entry for background downloading and set status
 * on the file so that it is clear that background downloading is
 * taking place.
 */

int http_background_load(int *remote, int local, off_t total_length, int chunked,
	int *download_status)
{
	int error = 0;

	/* As a hack, set the NODUMP bit so that the kernel
	 * knows that we are in the process of filling up the file */
	error = fchflags(local, UF_NODUMP);
	if (error)
	{
		return (error);
	}

	*download_status = WEBDAV_DOWNLOAD_IN_PROGRESS;

	error = webdav_requestqueue_enqueue_download(remote, local, total_length, chunked,
		download_status);
	return (error);
}

/*****************************************************************************/

int http_get_body(struct fetch_state *fs, struct iovec *iov, int iovlen,
	off_t *body_length, caddr_t *xml_addr)
{
	struct http_state    *https = fs->fs_proto;
	struct msghdr msg;
	int status = 0;
	int myreturn = 0;
	size_t total_length;
	ssize_t readresult, count;
	int redirection, retrying, chunked, reconnected;
	int dav_status;
#ifdef READDISPLAY
	struct timeval tv;
	struct timezone tz;
#endif

	*xml_addr = 0;
	reconnected = 0;
	redirection = 0;
	retrying = 0;

	fs->fs_status = "creating request message";
	msg.msg_name = (caddr_t) & http_sin;
	msg.msg_namelen = sizeof http_sin;
	msg.msg_iov = iov;
	msg.msg_control = 0;
	msg.msg_controllen = 0;
	msg.msg_flags = 0;
	msg.msg_iovlen = iovlen;

	/* open the socket if needed */
	if (*(fs->fs_socketptr) < 0)
	{
		if (http_socket_reconnect(fs->fs_socketptr, fs->fs_use_connect, 0))
		{
			return (EIO);
		}
	}

	/* use WEBDAV_IO_TIMEOUT */
	http_set_socket_sndtimeo(*(fs->fs_socketptr), WEBDAV_IO_TIMEOUT);

retry:
	fs->fs_status = "sending request message";

	if (logfile)
	{
		if (memcmp(msg.msg_iov[0].iov_base, "GET", 3))
		{
			fprintf(logfile, "%s", msg.msg_iov[0].iov_base);
		}
		else
		{
			fprintf(logfile, "%s", "GET-BYTES");
		}

		fprintf(logfile, "%s On Socket %d\n", msg.msg_iov[1].iov_base, *(fs->fs_socketptr));
	}

	if (sendmsg(*(fs->fs_socketptr), &msg, 0) < 0)
	{
		if (!reconnected)
		{
#ifdef DEBUG
			fprintf(stderr, "sendmsg errno was %d\n", errno);
#endif

			if (http_socket_reconnect(fs->fs_socketptr, fs->fs_use_connect, 1))
			{
				return (EIO);
			}
			reconnected = 1;
			goto retry;
		}
		else
		{
			warnx("sendmsg (http_get_body): error after reconnect to %s", http_hostname);
			return EIO;
		}

	}

	myreturn = http_parse_response_header(fs, &total_length, &dav_status, &chunked);

	switch (myreturn)
	{

		case EAGAIN:
#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
			fprintf(stderr, "http_get_body: Got error %d from parse response header\n", myreturn);
#endif
			if (!reconnected)
			{
				myreturn = http_socket_reconnect(fs->fs_socketptr, fs->fs_use_connect, 1);
				if (myreturn)
				{
					goto out;
				}
				reconnected = 1;
				goto retry;
			}
			else
			{
				warnx("empty reply from %s", http_hostname);
				myreturn = EIO;
				goto out;
			}

		case 204:
			/*
			 * 204 by definition means no body so just move
			 * along
			 */
			*body_length = 0;
			goto out;

		case EAUTH:
			/* If it's an EAUTH, the http_authentication or 
			  proxy authentication has been updated appropriately,
			  and we should retry after resetting the iov structure.
			*/
			break;

		default:
			/* Whatever myreturn is - even if it's an error, press on
			 * so that we will get the rest of the data off of the
			 * socket.	myreturn will make it back to the caller.
			 */

			break;
	}
	
	/*
	 * OK, if we got here, then we have finished parsing the header
	 * and have read the `\r\n' line which denotes the end of same.
	 * We may or may not have a good idea of the length of the file
	 * or its modtime.	At this point we will have to deal with
	 * any special byte-range, content-negotiation, redirection,
	 * or authentication, and probably jump back up to the top,
	 * once we implement those features.  So, all we have left to
	 * do is open up the output file and copy data from input to
	 * output until EOF.  We are deliberately ignoring any other
	 * error that may come back from parse_header because we still
	 * need to get the body out.
	 */

	*body_length = (off_t)total_length;			/* set in http_parse_response_header */
	if (!total_length)
	{
		goto out;
	}

	if (total_length != -1)
	{
		/* if we know the length, just suck everything down in
		  one fell swoop. Otherwise, will put it into a file
		  and then read it out */

		*xml_addr = malloc(total_length);
		if (*xml_addr == NULL)
		{
			if (!myreturn)
			{
				myreturn = ENOMEM;
			}
			goto out;
		}
#ifdef READDISPLAY
		printf("About to read from get_body with known length %ld\n", total_length);
		gettimeofday(&tv, &tz);
		printf("%s\n", ctime((long *) & tv.tv_sec));
#endif

		for (count = 0, readresult = 0; (readresult != total_length); readresult += count)
		{
			count = socket_read_bytes(*(fs->fs_socketptr),
				((char *)(*xml_addr) + readresult), (total_length - readresult));
			if (!count)
			{
				if (!myreturn)
				{
					myreturn = EIO;
				}
				goto out;
			}

#ifdef READDISPLAY
			printf("read %ld bytesof %ld\n", readresult, total_length);
			gettimeofday(&tv, &tz);
			printf("%s\n", ctime((long *) & tv.tv_sec));
#endif

		}

	}
	else
	{
		/* don't know the length */
		
		if (!fs->fs_outputfile)
		{
			myreturn = ENOENT;
			goto out;
		}

		if (chunked)
		{
			status = http_read_body_chunked(fs->fs_socketptr, xml_addr, &total_length);
		}
		else
		{
			status = http_read_body(fs->fs_socketptr, xml_addr, &total_length);
		}
		if (status)
		{
			if (!myreturn)
			{
				myreturn = status;
			}
			goto out;
		}

		*body_length = (off_t)total_length;		/* set in one of http_read_body routines */
	}

out:
	
	if (myreturn != 0)
	{
		if (*xml_addr)
		{
			/* if we have an error, get rid of any
			 * memory we may have allocated
			 */
			free(*xml_addr);
			*xml_addr = 0;
		}
		*body_length = 0;

#ifdef DEBUG
		printf("http_get_body returning %d\n", myreturn);
#endif

	}

	/* *** check whether the server has closed the connection *** */
	if ((*(fs->fs_socketptr) >= 0) && ((proxy_ok && (!proxy_exception)) || https->connection_close))
	{
		(void)close(*(fs->fs_socketptr));
		*(fs->fs_socketptr) = -1;
	}

	return (myreturn);
}

/*****************************************************************************/

/*
 * parse_ht_dav parses a DAV header's field-value.
 *	Input:
 *		field_value		pointer to field-content value to parse
 *		dav_level		pointer to current DAV level. Before calling this
 *						function the first time, dav_level must be set to 0
 *						(0 = not DAV compliant).
 *		
 *	Outputs:
 *		dav_level		if a higher DAV level is found than the input DAV level
 *						passed in, the new highest value is returned in the int
 *						pointed to by dav_level.
 * The rules for message headers are (rfc 2518, section 9.1):
 *
 *	DAV = "DAV" ":" "1" ["," "2"] ["," 1#extend]
 *	extend = Coded-URL | token
 *
 * (Note: The rules for extend are still not in the rfc - they were taken from
 * messages in the WebDAV discussion list and are needed to interoperability
 * with Apache 2.0 servers which put Coded-URLs in DAV headers.)
 */
static void parse_ht_dav(char *field_value, int *dav_level)
{
	char *token;
		
	while ( *field_value != '\0' )
	{
		/* find first non-LWS character */
		field_value = SkipLWS(field_value);
		if ( *field_value == '\0' )
		{
			/* if we're at end of string, break out of main while loop */
			break;
		}
		
		/* is value a token or a Coded-URL? */
		if ( *field_value == '<' )
		{
			/* it's a Coded-URL, so eat it */
			
			/* skip over '<' */
			++field_value;
			
			/* find '>"' marking the end of the quoted-string */
			field_value = SkipCodedURL(field_value);
			
			if ( *field_value != '\0' )
			{
				/* skip over '>' */
				++field_value;
			}
		}
		else
		{
			/* it's a token */
			
			/* mark start of the value token */
			token = field_value;
			
			/* find the end of the value token */
			field_value = SkipToken(field_value);
			
			/* could this token be '1' or '2'? */
			if ( (field_value - token) == 1 )
			{
				if ( (*token == '1') && (*dav_level < 1) )
				{
					*dav_level = 1;
				}
				else if ( *token == '2' && (*dav_level < 2) )
				{
					*dav_level = 2;
				}
			}
		}
		
		/* skip over LWS (if any) */
		field_value = SkipLWS(field_value);
		
		/* if there's any string left after the LWS... */
		if ( *field_value != '\0' )
		{
			/* we should have found a comma */
			if ( *field_value != ',' )
			{
				/* if not, break out of main while loop */
				break;
			}
			
			/* skip over one or more commas */
			while ( *field_value == ',' )
			{
				++field_value;
			}
		}
		
		/*
		 * field_value is now pointing at first character after comma
		 * delimiter, or at end of string
		 */
	}
}

/*****************************************************************************/

int http_parse_response_header(struct fetch_state *fs, size_t *total_length,
	int *dav_status, int *chunked)
{
	struct http_state    *https = fs->fs_proto;
	int remote = *(fs->fs_socketptr);
	int status = 0;
	int myreturn = 0;
	char *line = NULL;
	ssize_t linelen;
	char *new_location = NULL,
		*base64ofmd5 = NULL,
		*best_auth_challenge = NULL,
		*best_auth_realm = NULL;

	time_t last_modified, 						/* *** may not be needed *** */
	when_to_retry;
	int redirection = 0,
		continue_received = 0,
		retrying = 0,
		autherror = 0,
		best_auth_level = 0,
		auth_updated = FALSE,
		no_ui_needed = FALSE;

	*dav_status = 0;
	*chunked = 0;
	*total_length = -1;							/* -1 means don't know */

	/* allocate the line buffer */
	line = malloc(MAX_HTTP_LINELEN);
	if ( line == NULL )
	{
		myreturn = EIO;
		goto out;
	}

	fs->fs_status = "reading reply status";

got100reply:

	linelen = socket_read_line(remote, line, MAX_HTTP_LINELEN);
	if (linelen == 0)
	{
#ifdef DEBUG
		printf("parse_response_header: returning EAGAIN; errno = %d\n", errno);
#endif

		myreturn = EAGAIN;
		goto out;
	}

	/*
	 * The other end needs to be doing HTTP 1.0 at the very least,
	 * which is checked in http_first_line().
	 */

	status = http_first_line(line);

	if (logfile)
	{
		fprintf(logfile, "%s On Socket %d\n", line, remote);
	}

	if (status == -1)
	{
		if (continue_received)
		{
			goto got100reply;
		}
		else
		{
			/* it was HTTP .9 */
			myreturn = EIO;
			goto out;
		}
	}
	continue_received = 0;

	/* handle all informational 1xx status codes the same */
	if ( (status > 100) && (status <= 199) )
	{
		status = 100;
	}
	
	switch (status)
	{
		case 100:								/* Continue */
			/* read until we get a legal first line */
			continue_received = 1;
			goto got100reply;

		case 200:
		case 201:
		case 207:								/* Here come results */
		case 203:								/* Non-Authoritative Information */
		case 206:								/* Partial results (happens when we specified a
												 * range header) */
			status = 0;
			break;

		case 204:
			myreturn = status;
			break;								/* no body */

		case 301:
		case 302:								/* Resource has moved
												 * spec says don't redirect without user
												 * confirmation except on GET OR HEAD
												 * let caller decide. */
			myreturn = ENOENT;
			break;

		case 304:								/* Object is unmodified */
#ifdef DEBUG
			fprintf(stderr, "Object unmodified %d: %s\n", status, line);
#endif
			myreturn = ENOENT;
			break;

		case 400:
			myreturn = EINVAL;
			break;

		case 401:								/* Unauthorized */
		case 407:								/* Proxy Authentication Required */
			autherror = status;
			break;

		case 403:
			myreturn = EPERM;
			break;

		case 404:
			myreturn = ENOENT;
			break;

		case 423:								/* Locked, we translate this to EBUSY */
			myreturn = EBUSY;
			break;

		case 500:
			/* Internal server error.  It's been known to
			 * happen on zope when target wasn't there, so
			 * return ENOENT
			 */
			myreturn = ENOENT;
			break;

		case 501:								/* Not Implemented */
#ifdef DEBUG
			fprintf(stderr, "Not Implemented %d: %s\n", status, line);
#endif
			myreturn = EINVAL;
			break;

		case 503:								/* Service Unavailable */
#ifdef DEBUG
			fprintf(stderr, "Service Unavailable %d: %s\n", status, line);
#endif
			if (!fs->fs_auto_retry)
			{
				myreturn = ENOENT;
			}
			else
			{
				retrying = 503;
			}
			break;

		case 507:
			myreturn = ENOSPC;
			break;

		default:
#ifdef DEBUG
			fprintf(stderr, "Unknown error %d: %s\n", status, line);
#endif
			myreturn = status;
			break;
	}

	last_modified = when_to_retry = -1;
	*chunked = 0;
	fs->fs_status = "parsing reply headers";

	while ((linelen = socket_read_line(remote, line, MAX_HTTP_LINELEN)) != 0)
	{
		char *value,  *ep;
		enum http_header header;
		unsigned long ul;

		header = http_parse_header(line, &value);
		if (header == ht_end_of_header)
		{
			break;
		}

		switch (header)
		{
			case ht_connection:
				/* if "connection: close", set the https->connection_close */
				if (strncasecmp(value, "close", 5) == 0)
				{
					https->connection_close = 1;
				}
				break;
				
			case ht_content_length:
				errno = 0;
				ul = strtoul(value, &ep, 10);
				if (errno != 0 || *ep)
				{
					warnx("invalid Content-Length: `%s'", value);
				}
				*total_length = (size_t)ul;
				break;

			case ht_dav:
				parse_ht_dav(value, dav_status);
				break;

			case ht_last_modified:
				last_modified = parse_http_date(value);
#ifdef DEBUG
				if (last_modified == -1)
				{
					warnx("invalid Last-Modified: `%s'", value);
				}
#endif

				break;

			case ht_content_md5:
				if (base64ofmd5)
				{
					free(base64ofmd5);
				}
				base64ofmd5 = malloc(strlen(value) + 1);
				if (base64ofmd5 != 0)
				{
					strcpy(base64ofmd5, value);
				}
				else if (!myreturn)
				{
					myreturn = ENOMEM;
				}
				break;

			case ht_content_range:
				/* called from http_read_bytes */

				{
					off_t last_byte = 0, restart_from = 0;

					status = parse_http_content_range(value, &restart_from, &last_byte);
					if (status)
					{
						myreturn = EAGAIN;
					}
					/* *** If we don't get exactly the bytes we asked for
					  that information will not currently get back
					  to http_read_bytes()
					*** */
				}
				break;

			case ht_location:
				if (redirection)
				{
					int len;
					char *s = value;
					while (*s && !isspace(*s))
					{
						s++;
					}
					len = s - value;
					if (new_location)
					{
						free(new_location);
					}
					new_location = malloc(len + 1);
					if (!new_location)
					{
						if (!myreturn)
						{
							myreturn = ENOMEM;
						}
					}
					else
					{
						strncpy(new_location, value, len);
						new_location[len] = '\0';
					}
				}
				break;

			case ht_transfer_encoding:
				if (strncasecmp(value, "chunked", 7) == 0)
				{
					*chunked = 1;
					break;
				}
				warnx("%s: %s specified Transfer-Encoding `%s'", fs->fs_outputfile,
					http_hostname, value);
				warnx("%s: output file may be uninterpretable", fs->fs_outputfile);
				break;

			case ht_retry_after:
				if (!retrying)
				{
					break;
				}

				errno = 0;
				ul = strtoul(value, &ep, 10);
				if (errno != 0 || (*ep && !isspace(*ep)))
				{
					time_t when;
					when = parse_http_date(value);
					if (when == -1)
					{
						break;
					}
					when_to_retry = when;
				}
				else
				{
					when_to_retry = time(0) + ul;
				}
				break;

			case ht_www_authenticate:
			case ht_proxy_authenticate:
				if (((header == ht_www_authenticate) && (autherror != 401)) ||
					((header == ht_proxy_authenticate) && (autherror != 407)))
				{
					break;
				}

				/* if we're still looking for the best scheme */
				if (!auth_updated)
				{
					WebdavAuthcacheEvaluateRec auth_eval =
					{
						fs->fs_uid, value, (autherror == 407), 0, FALSE, FALSE, NULL
					};
					int error = webdav_authcache_evaluate(&auth_eval);

					if (!error)
					{
						if (auth_eval.updated)
						{
							/* no authcache_insert is needed, and this  
							  is the best authentication scheme,
							  so look no further */
							auth_updated = TRUE;
						}
						else
						{
							if (auth_eval.uiNotNeeded)
							{
								no_ui_needed = TRUE;
							}
							/* Now find out what kind of authentication scheme
							  based on the level reported */
							if (auth_eval.level > best_auth_level)
							{
								best_auth_level = auth_eval.level;
								if (best_auth_challenge)
								{
									free(best_auth_challenge);
								}
								best_auth_challenge = malloc(strlen(value) + 1);
								if (!best_auth_challenge)
								{
									error = ENOMEM;
								}
								else
								{
									strcpy(best_auth_challenge, value);
								}
								if (best_auth_realm)
								{
									free(best_auth_realm);
								}
								best_auth_realm = auth_eval.realmStr;
							}
							else
							{
								if (auth_eval.realmStr)
								{
									free(auth_eval.realmStr);
								}
							}
						}
					}
				}
				break;

			default:
				break;
		}
	}
	if (autherror)
	{
		status = 0;
		if (!auth_updated)
		{
			/* If it's not been updated, an alert/insert may be needed. */
			if (best_auth_level == 0)
			{
				status = EACCES;
			}
			else
			{
				char user[WEBDAV_MAX_USERNAME_LEN];
				char pass[WEBDAV_MAX_PASSWORD_LEN];

				bzero(user, sizeof(user));
				bzero(pass, sizeof(pass));
				if (!no_ui_needed)
				{
					char *allocatedURL = NULL;
					char *displayURL;
					
					if ( autherror == 401 )
					{
						/* 401 Unauthorized error */
						if ( reconstruct_url(http_hostname, https->http_remote_request, &allocatedURL) == 0 )
						{
							/* use allocatedURL for displayURL */
							displayURL = allocatedURL;
						}
						else
						{
							/* this shouldn't happen, but if it does, use http_remote_request */
							allocatedURL = NULL;
							displayURL = https->http_remote_request;
						}
					}
					else
					{
						/* must be 407 Proxy Authentication Required error */
						/* use proxy_server for displayURL */
						displayURL = proxy_server;
					}
					/* put up prompt for best_auth_level */
					status = webdav_get_authentication(user, sizeof(user), pass, sizeof(pass),
						(const char *)displayURL, (const char *)best_auth_realm, best_auth_level);
					/* for any error response, set status to EACCES */
					if (status)
					{
						status = EACCES;
					}
					/* free allocatedURL if allocated */
					if ( allocatedURL != NULL )
					{
						free(allocatedURL);
					}
				}
				if (!status)
				{
					/* insert authcache entry for best_auth_sofar */

					WebdavAuthcacheRemoveRec auth_rem =
					{
						fs->fs_uid, (autherror ==407), best_auth_realm
					};
					WebdavAuthcacheInsertRec auth_insert =
					{
						fs->fs_uid, best_auth_challenge, best_auth_level, (autherror == 407),
						user, pass
					};

					/* Whatever authorization we had before is no longer 
					  valid so remove it from the cache.  If geting
					  rid of it doesn't work, ignore it. */
					(void)webdav_authcache_remove(&auth_rem);
					if (fs->fs_uid != 0)
					{
						auth_rem.uid = 0;
						(void)webdav_authcache_remove(&auth_rem);
					}
					if (fs->fs_uid != 1)
					{
						auth_rem.uid = 1;
						(void)webdav_authcache_remove(&auth_rem);
					}

					status = webdav_authcache_insert(&auth_insert);
					if (!status)
					{
						/* if not "root", make an entry for root */
						if (fs->fs_uid != 0)
						{
							auth_insert.uid = 0;
							(void)webdav_authcache_insert(&auth_insert);
						}
						/* if not "daemon", make an entry for daemon */
						if (fs->fs_uid != 1)
						{
							auth_insert.uid = 1;
							(void)webdav_authcache_insert(&auth_insert);
						}
					}
				}
				bzero(user, sizeof(user));
				bzero(pass, sizeof(pass));
			}
		}
		if (!myreturn)
		{
			myreturn = (status) ? status : EAUTH;
		}
		goto out;
	}

	if (retrying)
	{
		int howlong;

		if (when_to_retry == -1)
		{
#ifdef DEBUG
			fprintf(stderr, "HTTP/1.1 503 Service Unavailable");
#endif

			myreturn = ENOENT;
			goto out;
		}
		howlong = when_to_retry - time(0);
		if (howlong < 30)
		{
			howlong = 30;
		}
#ifdef DEBUG
		warnx("%s: service unavailable; retrying in %d seconds", http_hostname, howlong);
#endif

		fs->fs_status = "waiting to retry";
		sleep(howlong);
		myreturn = EAGAIN;
	}

	fs->fs_status = "lookup: parsing xml for resource type";

out:
	if (base64ofmd5)
	{
		free(base64ofmd5);
	}
	if (new_location)
	{
		free(new_location);
	}
	if (best_auth_challenge)
	{
		free(best_auth_challenge);
	}
	if (best_auth_realm)
	{
		free(best_auth_realm);
	}
	if ( line != NULL )
	{
		free(line);
	}
	
	/* Ignore "Content-Length" header if "Transfer-Encoding: chunked"
	 * is specified (RFC 2616, section 4.4)
	 */
	if (*chunked)
	{
		*total_length = -1;
	}

	return (myreturn);
}												/* http_parse_response_header */

/*****************************************************************************/

/*
 * Get file information about a file using http/webdav
 */

int http_stat(struct fetch_state *fs, void *statstruct)
{
	struct iovec iov[NIOV];
	struct http_state *https = fs->fs_proto;
	struct vattr *statbuf = ((struct webdav_stat_struct *)statstruct)->statbuf;
	char *xml_addr;								/* memory address of xml data (mapped in) */
	off_t xml_length;							/* length of the xml string */
	off_t xml_size;
	int size_index, n;
	int error = 0;
	char lengthline[SHORT_HTTP_LINELEN];
	char *http_auth = NULL;

retry:
	n = 0;
	xml_size = 0;

	if (http_auth)
	{
		free(http_auth);
	}
	http_auth = NULL;
	{
		WebdavAuthcacheRetrieveRec http_auth_struct =
		{
			fs->fs_uid, https ->http_remote_request, append_to_file, "PROPFIND", NULL
		};
		if (!webdav_authcache_retrieve(&http_auth_struct))
		{
			http_auth = http_auth_struct.authorization;
		}
	}

	addstr(iov, n, "PROPFIND ");
	addstr(iov, n, https->http_remote_request);
	if (append_to_file)
	{
		addstr(iov, n, append_to_file);
	}
	addstr(iov, n, " HTTP/1.1\r\n");
	addstr(iov, n, gUserAgentHeader);

	/* do content negotiation here */
	addstr(iov, n, "Accept: */*\r\n");
	addstr(iov, n, https->http_host_header);
	addstr(iov, n, "Content-Type: text/xml\r\n");
	addstr(iov, n, "Depth: 0\r\n");

	size_index = n;
	++n;

	if (http_auth)
	{
		addstr(iov, n, http_auth);
	}
	addstr(iov, n, "\r\n");

	addstr(iov, n, "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n");
	xml_size += iov[n - 1].iov_len;
	addstr(iov, n, "<D:propfind xmlns:D=\"DAV:\">\n");
	xml_size += iov[n - 1].iov_len;
	addstr(iov, n, "<D:prop>\n");
	xml_size += iov[n - 1].iov_len;
	addstr(iov, n, "<D:getlastmodified/>\n");
	xml_size += iov[n - 1].iov_len;
	addstr(iov, n, "<D:getcontentlength/>\n");
	xml_size += iov[n - 1].iov_len;
	addstr(iov, n, "<D:resourcetype/>\n");
	xml_size += iov[n - 1].iov_len;
	addstr(iov, n, "</D:prop>\n");
	xml_size += iov[n - 1].iov_len;
	addstr(iov, n, "</D:propfind>\n");
	xml_size += iov[n - 1].iov_len;
	addstr(iov, n, "\r\n");
	xml_size += iov[n - 1].iov_len;
	snprintf(lengthline, SHORT_HTTP_LINELEN, "Content-Length: %qd\r\n", xml_size);
	iov[size_index].iov_base = lengthline;
	iov[size_index].iov_len = strlen(lengthline);

	if (n >= NIOV)
	{
		error = EIO;
		goto out;
	}

	error = http_get_body(fs, iov, n, &xml_length, &xml_addr);
	if (error)
	{
		if (error == EAUTH)
		{
			goto retry;
		}
		else
		{
			goto out;
		}
	}

	/* parse it to get the info */
	error = parse_stat((char *)xml_addr, (int)xml_length,
		((struct webdav_stat_struct *)statstruct)->orig_uri, statbuf);
	
	/* fall through */

out:

	if (xml_addr)
	{
		free(xml_addr);
	}

	if (http_auth)
	{
		free(http_auth);
	}

	return (error);
}

/*****************************************************************************/

/*
 * Get volume information using http/webdav
 */

int http_statfs(struct fetch_state *fs, void *arg)
{
	struct iovec iov[NIOV];
	struct http_state *https = fs->fs_proto;
	struct statfs *statfsbuf = ((struct statfs *)arg);
	char *xml_addr;								/* memory address of xml data (mapped in) */
	off_t xml_length;							/* length of the xml string */
	off_t xml_size;
	int size_index, n;
	int error = 0;
	char *http_auth = NULL;
	char lengthline[SHORT_HTTP_LINELEN];

retry:

	n = 0;
	xml_size = 0;

	if (http_auth)
		free(http_auth);
	http_auth = NULL;
	{
		WebdavAuthcacheRetrieveRec http_auth_struct =
		{
			fs->fs_uid, https ->http_remote_request, append_to_file, "PROPFIND", NULL
		};
		if (!webdav_authcache_retrieve(&http_auth_struct))
		{
			http_auth = http_auth_struct.authorization;
		}
	}

	addstr(iov, n, "PROPFIND ");
	addstr(iov, n, https->http_remote_request);
	if (append_to_file)
	{
		addstr(iov, n, append_to_file);
	}
	addstr(iov, n, " HTTP/1.1\r\n");
	addstr(iov, n, gUserAgentHeader);

	/* do content negotiation here */
	addstr(iov, n, "Accept: */*\r\n");
	addstr(iov, n, https->http_host_header);
	addstr(iov, n, "Content-Type: text/xml\r\n");
	addstr(iov, n, "Depth: 0\r\n");
	size_index = n;
	++n;

	if (http_auth)
	{
		addstr(iov, n, http_auth);
	}

	addstr(iov, n, "\r\n");
	
	addstr(iov, n, "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n");
	xml_size += iov[n - 1].iov_len;
	addstr(iov, n, "<D:propfind xmlns:D=\"DAV:\">\n");
	xml_size += iov[n - 1].iov_len;
	addstr(iov, n, "<D:prop>\n");
	xml_size += iov[n - 1].iov_len;
	addstr(iov, n, "<D:quota/>\n");
	xml_size += iov[n - 1].iov_len;
	addstr(iov, n, "<D:quotaused/>\n");
	xml_size += iov[n - 1].iov_len;
	addstr(iov, n, "</D:prop>\n");
	xml_size += iov[n - 1].iov_len;
	addstr(iov, n, "</D:propfind>\n");
	xml_size += iov[n - 1].iov_len;
	addstr(iov, n, "\r\n");
	xml_size += iov[n - 1].iov_len;
	snprintf(lengthline, SHORT_HTTP_LINELEN, "Content-Length: %qd\r\n", xml_size);
	iov[size_index].iov_base = lengthline;
	iov[size_index].iov_len = strlen(lengthline);

	if (n >= NIOV)
	{
		error = EIO;
		goto out;
	}

	error = http_get_body(fs, iov, n, &xml_length, &xml_addr);
	if (error)
	{
		if (error == EAUTH)
		{
			goto retry;
		}
		else
		{
			goto out;
		}
	}

	/* parse it to get the info */
	error = parse_statfs((char *)xml_addr, (int)xml_length, statfsbuf);
	
	/* fall through */

out:

	if (http_auth)
	{
		free(http_auth);
	}

	if (xml_addr)
	{
		free(xml_addr);
	}

	return (error);
}

/*****************************************************************************/

/*
 * Delete a file
 */

int http_delete(struct fetch_state *fs, void *unused_arg)
{
	struct iovec iov[NIOV];
	struct http_state *https = fs->fs_proto;
	char *http_auth = NULL;
	char *xml_addr = (char *)0;					/* memory address of xml data (mapped in) */
	off_t xml_length;							/* length of the xml string */
	int n = 0;
	int error = 0;

retry:

	n = 0;
	xml_length = 0;

	if (http_auth)
	{
		free(http_auth);
	}
	http_auth = NULL;
	{
		WebdavAuthcacheRetrieveRec http_auth_struct =
		{
			fs->fs_uid, https ->http_remote_request, append_to_file, "DELETE", NULL
		};
		if (!webdav_authcache_retrieve(&http_auth_struct))
		{
			http_auth = http_auth_struct.authorization;
		}
	}

	addstr(iov, n, "DELETE ");
	addstr(iov, n, https->http_remote_request);
	if (append_to_file)
	{
		addstr(iov, n, append_to_file);
	}
	addstr(iov, n, " HTTP/1.1\r\n");
	addstr(iov, n, gUserAgentHeader);

	/* do content negotiation here */
	addstr(iov, n, "Accept: */*\r\n");
	addstr(iov, n, https->http_host_header);

	if (http_auth)
	{
		addstr(iov, n, http_auth);
	}

	addstr(iov, n, "\r\n");

	if (n >= NIOV)
	{
		error = EIO;
		goto out;
	}

	error = http_get_body(fs, iov, n, &xml_length, &xml_addr);

	if (error == 204)
	{
		error = 0;								/* no body is a perfectly good response for delete */
	}
	else
	{
		if (error == EAUTH)
		{
			goto retry;
		}
	}

out:

	if (http_auth)
	{
		free(http_auth);
	}

	if (xml_addr)
	{
		free(xml_addr);
	}

	return (error);
}

/*****************************************************************************/

/*
 * Obtain an exclusive write lock on an object.	 
 */

int http_lock(struct fetch_state *fs, void *lockarg)
{
	struct iovec iov[NIOV];
	struct http_state *https = fs->fs_proto;
	struct webdav_lock_struct *lockdata = (struct webdav_lock_struct *)lockarg;
	char *xml_addr;								/* memory address of xml data (mapped in) */
	off_t xml_length;							/* length of the xml string */
	int size_index, n;
	off_t xml_size;
	int error = 0;
	char *http_auth = NULL;
	char lengthline[SHORT_HTTP_LINELEN], locktoken[SHORT_HTTP_LINELEN];

retry:
	
	n = 0;
	xml_size = 0;
	
	if (http_auth)
	{
		free(http_auth);
	}
	http_auth = NULL;
	{
		WebdavAuthcacheRetrieveRec http_auth_struct =
		{
			fs->fs_uid, https ->http_remote_request, append_to_file, "LOCK", NULL
		};
		if (!webdav_authcache_retrieve(&http_auth_struct))
		{
			http_auth = http_auth_struct.authorization;
		}
	}

	addstr(iov, n, "LOCK ");
	addstr(iov, n, https->http_remote_request);
	if (append_to_file)
	{
		addstr(iov, n, append_to_file);
	}
	addstr(iov, n, " HTTP/1.1\r\n");
	addstr(iov, n, gUserAgentHeader);

	/* do content negotiation here */
	addstr(iov, n, "Accept: */*\r\n");
	addstr(iov, n, https->http_host_header);
	addstr(iov, n, "Depth: 0\r\n");
	addstr(iov, n, "Timeout: Second-");
	addstr(iov, n, gtimeout_string);
	addstr(iov, n, "\r\n");
	if (!lockdata->refresh)
	{
		addstr(iov, n, "Content-Type: text/xml: charset=\"utf-8\"\r\n");
		size_index = n;
		++n;
	}
	else
	{
		size_index = 0;
	}

	if (http_auth)
	{
		addstr(iov, n, http_auth);
	}

	if (lockdata->refresh)
	{
		addstr(iov, n, "Content-Type: text/xml\r\n");
		snprintf(locktoken, SHORT_HTTP_LINELEN, "If:(<%s>)\r\n", lockdata->locktoken);
		addstr(iov, n, locktoken);
	}
	addstr(iov, n, "\r\n");

	if (!lockdata->refresh)
	{
		addstr(iov, n, "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n");
		xml_size += iov[n - 1].iov_len;
		addstr(iov, n, "<D:lockinfo xmlns:D=\"DAV:\">\n");
		xml_size += iov[n - 1].iov_len;
		addstr(iov, n, "<D:lockscope><D:exclusive/></D:lockscope>\n");
		xml_size += iov[n - 1].iov_len;
		addstr(iov, n, "<D:locktype><D:write/></D:locktype>\n");
		xml_size += iov[n - 1].iov_len;
		addstr(iov, n, "<D:owner> <D:href>default-owner</D:href> </D:owner>\n");
		xml_size += iov[n - 1].iov_len;
		addstr(iov, n, "</D:lockinfo>");
		xml_size += iov[n - 1].iov_len;
		addstr(iov, n, "\r\n");
		xml_size += iov[n - 1].iov_len;
		snprintf(lengthline, SHORT_HTTP_LINELEN, "Content-Length: %qd\r\n", xml_size);
		iov[size_index].iov_base = lengthline;
		iov[size_index].iov_len = strlen(lengthline);
	}

	if (n >= NIOV)
	{
		error = EIO;
		goto out;
	}

	error = http_get_body(fs, iov, n, &xml_length, &xml_addr);
	if (error)
	{
		if (error == 423)
		{
			error = EBUSY;
			goto out;
		}
		else
		{
			if (error == EAUTH)
			{
				goto retry;
			}
			else
			{
				goto out;
			}
		}
	}

	/* parse it to get the info */
	error = parse_lock((char *)xml_addr, (int)xml_length, &(lockdata->locktoken));
	
	/* fall through */

out:

	if (http_auth)
	{
		free(http_auth);
	}

	if (xml_addr)
	{
		free(xml_addr);
	}

	return (error);
}

/*****************************************************************************/

int http_unlock(struct fetch_state *fs, void *lockarg)
{
	struct iovec iov[NIOV];
	struct http_state *https = fs->fs_proto;
	struct webdav_lock_struct *lockdata = (struct webdav_lock_struct *)lockarg;
	char *http_auth = NULL;
	char *xml_addr;								/* memory address of xml data (mapped in) */
	off_t xml_length;							/* length of the xml string */
	int n = 0;
	int error = 0;

retry:
	
	n = 0;
	xml_length = 0;

	if (http_auth)
	{
		free(http_auth);
	}
	http_auth = NULL;
	{
		WebdavAuthcacheRetrieveRec http_auth_struct =
		{
			fs->fs_uid, https ->http_remote_request, append_to_file, "UNLOCK", NULL
		};
		if (!webdav_authcache_retrieve(&http_auth_struct))
		{
			http_auth = http_auth_struct.authorization;
		}
	}

	addstr(iov, n, "UNLOCK ");
	addstr(iov, n, https->http_remote_request);
	if (append_to_file)
	{
		addstr(iov, n, append_to_file);
	}
	addstr(iov, n, " HTTP/1.1\r\n");
	addstr(iov, n, gUserAgentHeader);

	/* do content negotiation here */
	addstr(iov, n, "Accept: */*\r\n");
	addstr(iov, n, https->http_host_header);
	addstr(iov, n, "Lock-Token:<");
	addstr(iov, n, lockdata->locktoken);
	addstr(iov, n, ">\r\n");

	if (http_auth)
	{
		addstr(iov, n, http_auth);
	}

	addstr(iov, n, "\r\n");

	if (n >= NIOV)
	{
		error = EIO;
		goto out;
	}

	error = http_get_body(fs, iov, n, &xml_length, &xml_addr);
	if (error)
	{
		if (error == 204)						/* success */
		{
			error = 0;
		}
		else if (error == EAUTH)
		{
			goto retry;
		}
	}

out:

	if (http_auth)
	{
		free(http_auth);
	}

	if (xml_addr)
	{
		free(xml_addr);
	}

	return (error);
}

/*****************************************************************************/

/* Delete a collection */

int http_delete_dir(struct fetch_state *fs, void *unused_arg)
{
	struct iovec iov[NIOV];
	struct http_state *https = fs->fs_proto;
	char *http_auth = NULL;
	char *xml_addr = (char *)0;					/* memory address of xml data (mapped in) */
	off_t xml_length;							/* length of the xml string */
	int size_index, n;
	int error = 0;
	int num_entries = 0;
	off_t xml_size;
	char lengthline[MAX_HTTP_LINELEN];

retry:
	
	n = 0;
	xml_length = 0;
	xml_size = 0;

	if (http_auth)
		free(http_auth);
	http_auth = NULL;
	{
		WebdavAuthcacheRetrieveRec http_auth_struct =
		{
			fs->fs_uid, https ->http_remote_request, append_to_file, "PROPFIND", NULL
		};
		if (!webdav_authcache_retrieve(&http_auth_struct))
		{
			http_auth = http_auth_struct.authorization;
		}
	}

	addstr(iov, n, "PROPFIND ");
	addstr(iov, n, https->http_remote_request);
	if (append_to_file)
	{
		addstr(iov, n, append_to_file);
	}
	addstr(iov, n, " HTTP/1.1\r\n");
	addstr(iov, n, gUserAgentHeader);

	/* do content negotiation here */
	addstr(iov, n, "Accept: */*\r\n");
	addstr(iov, n, https->http_host_header);
	addstr(iov, n, "Content-Type: text/xml\r\n");
	addstr(iov, n, "Depth: 1\r\n");

	size_index = n;
	++n;

	if (http_auth)
	{
		addstr(iov, n, http_auth);
	}

	addstr(iov, n, "\r\n");

	addstr(iov, n, "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n");
	xml_size += iov[n - 1].iov_len;
	addstr(iov, n, "<D:propfind xmlns:D=\"DAV:\">\n");
	xml_size += iov[n - 1].iov_len;
	addstr(iov, n, "<D:prop>\n");
	xml_size += iov[n - 1].iov_len;
	/* The prop XML element's rule is <!ELEMENT prop ANY> so
	 * we shouldn't need to ask for any properties. However,
	 * Microsoft-IIS/5.0 servers require at least one property,
	 * so asking for the resourcetype property is next best thing. */
	addstr(iov, n, "<D:resourcetype/>\n");
	xml_size += iov[n - 1].iov_len;
	addstr(iov, n, "</D:prop>\n");
	xml_size += iov[n - 1].iov_len;
	addstr(iov, n, "</D:propfind>\n");
	xml_size += iov[n - 1].iov_len;
	addstr(iov, n, "\r\n");
	xml_size += iov[n - 1].iov_len;
	snprintf(lengthline, MAX_HTTP_LINELEN, "Content-Length: %qd\r\n", xml_size);
	iov[size_index].iov_base = lengthline;
	iov[size_index].iov_len = strlen(lengthline);

	if (n >= NIOV)
	{
		error = EIO;
		goto out;
	}

	error = http_get_body(fs, iov, n, &xml_length, &xml_addr);
	if (error)
	{
		if (error == EAUTH)
		{
			goto retry;
		}
		else
		{
			goto out;
		}
	}

	error = parse_file_count(xml_addr, (int)xml_length, &num_entries);
	if (error)
	{
		goto out;
	}

	if (num_entries > 1)
	{
		/*
		 * An empty directory will have just one entry for itself as far
		 * as the server is concerned.	If there is more than that we need
		 * to barf since we don't allow deleting directories which have
		 * anything in them.
		 */
		error = ENOTEMPTY;
	}
	else
	{
		error = http_delete(fs, unused_arg);
	}

out:

	if (http_auth)
	{
		free(http_auth);
	}

	if (xml_addr)
	{
		free(xml_addr);
	}

	return (error);
}

/*****************************************************************************/

/*
 * Create a directory
 */

int http_mkcol(struct fetch_state *fs, void *unused_arg)
{
	struct iovec iov[NIOV];
	struct http_state *https = fs->fs_proto;
	char *http_auth = NULL;
	char *xml_addr = (char *)0;					/* memory address of xml data (mapped in) */
	off_t xml_length;							/* length of the xml string */
	int n = 0;
	int error = 0;

retry:

	n = 0;
	xml_length = 0;

	if (http_auth)
		free(http_auth);
	http_auth = NULL;
	{
		WebdavAuthcacheRetrieveRec http_auth_struct =
		{
			fs->fs_uid, https ->http_remote_request, append_to_file, "MKCOL", NULL
		}
		;
		if (!webdav_authcache_retrieve(&http_auth_struct))
			http_auth = http_auth_struct.authorization;
	}

	addstr(iov, n, "MKCOL ");
	addstr(iov, n, https->http_remote_request);
	if (append_to_file)
	{
		addstr(iov, n, append_to_file);
	}
	addstr(iov, n, " HTTP/1.1\r\n");
	addstr(iov, n, gUserAgentHeader);

	/* do content negotiation here */
	addstr(iov, n, "Accept: */*\r\n");
	addstr(iov, n, https->http_host_header);

	if (http_auth)
	{
		addstr(iov, n, http_auth);
	}

	addstr(iov, n, "\r\n");

	if (n >= NIOV)
	{
		error = EIO;
		goto out;
	}

	error = http_get_body(fs, iov, n, &xml_length, &xml_addr);

	if (error == 204)
	{
		error = 0;	/* no body is a perfectly good response for make collection */
	}
	else
	{
		if (error == EAUTH)
		{
			goto retry;
		}
	}

out:

	if (http_auth)
	{
		free(http_auth);
	}

	if (xml_addr)
	{
		free(xml_addr);
	}

	return (error);
}

/*****************************************************************************/

/*
 * Move an object
 */

int http_move(struct fetch_state *fs, void *dest_file)
{
	struct iovec iov[NIOV];
	struct http_state *https = fs->fs_proto;
	char *http_auth = NULL;
	char *xml_addr = (char *)0;					/* memory address of xml data (mapped in) */
	off_t xml_length;							/* length of the xml string */
	int n = 0;
	int error = 0;

retry:

	n = 0;
	xml_length = 0;

	if (http_auth)
		free(http_auth);
	http_auth = NULL;
	{
		WebdavAuthcacheRetrieveRec http_auth_struct =
		{
			fs->fs_uid, https ->http_remote_request, append_to_file, "MOVE", NULL
		};
		if (!webdav_authcache_retrieve(&http_auth_struct))
		{
			http_auth = http_auth_struct.authorization;
		}
	}

	addstr(iov, n, "MOVE ");
	addstr(iov, n, https->http_remote_request);
	if (append_to_file)
	{
		addstr(iov, n, append_to_file);
	}
	addstr(iov, n, " HTTP/1.1\r\n");
	addstr(iov, n, gUserAgentHeader);

	/* do content negotiation here */
	addstr(iov, n, "Accept: */*\r\n");
	addstr(iov, n, https->http_host_header);
	addstr(iov, n, "Destination: " _WEBDAVPREFIX);
	addstr(iov, n, (char *)dest_file);
	addstr(iov, n, "\r\n");
	addstr(iov, n, "Content-Length: 0\r\n");

	if (http_auth)
	{
		addstr(iov, n, http_auth);
	}

	addstr(iov, n, "\r\n");

	if (n >= NIOV)
	{
		error = EIO;
		goto out;
	}

	error = http_get_body(fs, iov, n, &xml_length, &xml_addr);

	if (error == 204)
	{
		error = 0;	/* no body is a perfectly good response for move */
	}
	else if (error == EAUTH)
	{
		goto retry;
	}

out:

	if (http_auth)
	{
		free(http_auth);
	}

	if (xml_addr)
	{
		free(xml_addr);
	}

	return (error);
}

/*****************************************************************************/

int http_read_bytes(struct fetch_state *fs, void *arg)
{
	struct iovec iov[NIOV];
	struct http_state *https = fs->fs_proto;
	struct webdav_read_byte_info *byte_info = (struct webdav_read_byte_info *)arg;
	int n = 0;
	int error = 0;
	char *http_auth = NULL;
	char byteline[SHORT_HTTP_LINELEN];

	/* do a check to make sure we actually have bytes to read 
	 * note that num_read_bytes is unsigned (off_t) */

	if (byte_info->num_bytes == 0)
	{
		byte_info->num_read_bytes = 0;
		goto done;
	}

retry:

	n = 0;

	if (http_auth)
		free(http_auth);
	http_auth = NULL;
	{
		WebdavAuthcacheRetrieveRec http_auth_struct =
		{
			fs->fs_uid, https ->http_remote_request, NULL, "GET", NULL
		};
		if (!webdav_authcache_retrieve(&http_auth_struct))
		{
			http_auth = http_auth_struct.authorization;
		}
	}

	addstr(iov, n, "GET ");
	addstr(iov, n, https->http_remote_request);
	addstr(iov, n, " HTTP/1.1\r\n");
	addstr(iov, n, gUserAgentHeader);

	/* do content negotiation here */
	addstr(iov, n, "Accept: */*\r\n");
	addstr(iov, n, https->http_host_header);

	if (http_auth)
	{
		addstr(iov, n, http_auth);
	}

	addstr(iov, n, "Range: bytes=");
	snprintf(byteline, SHORT_HTTP_LINELEN, "%qd-%qd\r\n", byte_info->byte_start,
		byte_info->byte_start + byte_info->num_bytes - 1);
	addstr(iov, n, byteline);
	addstr(iov, n, "\r\n");

	if (n >= NIOV)
	{
		error = EIO;
		goto done;
	}

	error = http_get_body(fs, iov, n, &byte_info->num_read_bytes, &byte_info->byte_addr);

	if (error == EAUTH)
	{
		goto retry;
	}

done:

	if (http_auth)
	{
		free(http_auth);
	}

	return (error);
}

/*****************************************************************************/

/* http lookup */
int http_lookup(struct fetch_state *fs, void *a_file_type)
{
	struct iovec iov[NIOV];
	struct http_state *https = fs->fs_proto;
	char *http_auth = NULL;
	char *xml_addr = 0;							/* memory address of xml data (mapped in) */
	off_t xml_length;							/* length of the xml string */
	int size_index, n;
	int error = 0;
	off_t xml_size;
	char lengthline[MAX_HTTP_LINELEN];

retry:

	n = 0;
	xml_size = 0;

	if (http_auth)
	{
		free(http_auth);
	}
	http_auth = NULL;
	{
		WebdavAuthcacheRetrieveRec http_auth_struct =
		{
			fs->fs_uid, https ->http_remote_request, append_to_file, "PROPFIND", NULL
		};
		if (!webdav_authcache_retrieve(&http_auth_struct))
		{
			http_auth = http_auth_struct.authorization;
		}
	}

	addstr(iov, n, "PROPFIND ");
	addstr(iov, n, https->http_remote_request);
	if (append_to_file)
	{
		addstr(iov, n, append_to_file);
	}
	addstr(iov, n, " HTTP/1.1\r\n");
	addstr(iov, n, gUserAgentHeader);

	/* do content negotiation here */
	addstr(iov, n, "Accept: */*\r\n");
	addstr(iov, n, https->http_host_header);
	addstr(iov, n, "Content-Type: text/xml\r\n");
	addstr(iov, n, "Depth: 0\r\n");

	size_index = n;
	++n;

	if (http_auth)
	{
		addstr(iov, n, http_auth);
	}

	addstr(iov, n, "\r\n");
	
	addstr(iov, n, "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n");
	xml_size += iov[n - 1].iov_len;
	addstr(iov, n, "<D:propfind xmlns:D=\"DAV:\">\n");
	xml_size += iov[n - 1].iov_len;
	addstr(iov, n, "<D:prop>\n");
	xml_size += iov[n - 1].iov_len;
	addstr(iov, n, "<D:resourcetype/>\n");
	xml_size += iov[n - 1].iov_len;
	addstr(iov, n, "</D:prop>\n");
	xml_size += iov[n - 1].iov_len;
	addstr(iov, n, "</D:propfind>\n");
	xml_size += iov[n - 1].iov_len;
	addstr(iov, n, "\r\n");
	xml_size += iov[n - 1].iov_len;
	snprintf(lengthline, MAX_HTTP_LINELEN, "Content-Length: %qd\r\n", xml_size);
	iov[size_index].iov_base = lengthline;
	iov[size_index].iov_len = strlen(lengthline);

	if (n >= NIOV)
	{
		error = EIO;
		goto out;
	}

	error = http_get_body(fs, iov, n, &xml_length, &xml_addr);
	if (error)
	{
		if (error == EAUTH)
		{
			goto retry;
		}
		else
		{
			goto out;
		}
	}

	/* parse it to get the filetype */
	error = parse_lookup(xml_addr, (int)xml_length, a_file_type);
	
	/* fall through */

out:

	if (http_auth)
	{
		free(http_auth);
	}

	if (xml_addr)
	{
		free(xml_addr);
	}

	return (error);
}

/*****************************************************************************/

/*
 * refresh the children of a directory (collection) on the web and cache
 * them in a local file for readdir to scan later.	This will make
 * Webdav directories essentially work like UFS directories as plain
 * text files (with a special type) that contain dir entries.
 */

int http_refreshdir(struct fetch_state *fs, void *array_elem)
{
	struct iovec iov[NIOV];
	struct http_state *https = fs->fs_proto;
	struct file_array_element *file_array_elem = (struct file_array_element *)array_elem;
	int file_desc = 0;
	char *http_auth = NULL;
	char *xml_addr;								/* memory address of xml data */
	off_t xml_length;							/* length of the xml string */
	off_t xml_size;
	int size_index, n;
	int error = 0;
	char lengthline[SHORT_HTTP_LINELEN];

retry:
	
	n = 0;
	xml_size = 0;

	if (http_auth)
	{
		free(http_auth);
	}
	http_auth = NULL;
	{
		WebdavAuthcacheRetrieveRec http_auth_struct =
		{
			fs->fs_uid, https ->http_remote_request, append_to_file, "PROPFIND", NULL
		};
		if (!webdav_authcache_retrieve(&http_auth_struct))
		{
			http_auth = http_auth_struct.authorization;
		}
	}

	addstr(iov, n, "PROPFIND ");
	addstr(iov, n, https->http_remote_request);
	if (append_to_file)
	{
		addstr(iov, n, append_to_file);
	}
	addstr(iov, n, " HTTP/1.1\r\n");
	addstr(iov, n, gUserAgentHeader);

	/* do content negotiation here */
	addstr(iov, n, "Accept: */*\r\n");
	addstr(iov, n, https->http_host_header);
	addstr(iov, n, "Content-Type: text/xml\r\n");
	addstr(iov, n, "Depth: 1\r\n");
	size_index = n;
	++n;

	if (http_auth)
	{
		addstr(iov, n, http_auth);
	}

	addstr(iov, n, "\r\n");
	
	addstr(iov, n, "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n");
	xml_size += iov[n - 1].iov_len;
	addstr(iov, n, "<D:propfind xmlns:D=\"DAV:\">\n");
	xml_size += iov[n - 1].iov_len;
	addstr(iov, n, "<D:prop>\n");
	xml_size += iov[n - 1].iov_len;
	addstr(iov, n, "<D:getlastmodified/>\n");
	xml_size += iov[n - 1].iov_len;
	addstr(iov, n, "<D:getcontentlength/>\n");
	xml_size += iov[n - 1].iov_len;
	addstr(iov, n, "<D:resourcetype/>\n");
	xml_size += iov[n - 1].iov_len;
	addstr(iov, n, "</D:prop>\n");
	xml_size += iov[n - 1].iov_len;
	addstr(iov, n, "</D:propfind>\n");
	xml_size += iov[n - 1].iov_len;
	addstr(iov, n, "\r\n");
	xml_size += iov[n - 1].iov_len;
	snprintf(lengthline, SHORT_HTTP_LINELEN, "Content-Length: %qd\r\n", xml_size);
	iov[size_index].iov_base = lengthline;
	iov[size_index].iov_len = strlen(lengthline);

	if (n >= NIOV)
	{
		error = EIO;
		goto out;
	}

	error = http_get_body(fs, iov, n, &xml_length, &xml_addr);
	if (error)
	{
		if (error == EAUTH)
		{
			goto retry;
		}
		else
		{
			goto out;
		}
	}

	file_desc = file_array_elem->fd;
	if (ftruncate(file_desc, (off_t)0))
	{
		error = EIO;
		goto out;
	}

	/* Reset the file pointer back to 0.  We can do an if 0 check
	 * to see if there is an lseek failure because we are setting
	 * explicitly to 0
	 */

	if (lseek(file_desc, (off_t)0, SEEK_SET))
	{
		error = EIO;
		goto out;
	}

	error = parse_opendir(xml_addr, (int)xml_length, file_desc,
		https->http_remote_request, http_hostname, file_array_elem->uid);
	/* fall through */

out:
	
	if (http_auth)
	{
		free(http_auth);
	}

	if (xml_addr)
	{
		free(xml_addr);
	}

	return (error);
}

/*****************************************************************************/

/*
  http_fsync: copy (presumably modified) file contents from local cache
  file up to the webdav server
*/

int http_put(struct fetch_state *fs, void *arg)
{
	struct iovec iov[NIOV];
	struct http_state *https;
	int local;									/* local file */
	int n = 0;
	struct webdav_put_struct *putinfo = (struct webdav_put_struct *)arg;
	struct msghdr msg;
	int myreturn = 0;
	size_t total_length;
	off_t local_len = 0;
	size_t readresult = 0;
	size_t writeresult = 0;
	off_t bytes_written = 0;
	int redirection, retrying, chunked, reconnected;
	int dav_status;
	char *http_auth = NULL;
	char buf[BUFFER_SIZE];
	char lengthline[SHORT_HTTP_LINELEN];

	https = fs->fs_proto;

	/* Get the local file ready */

	if (arg != (void *) - 1)
	{
		local = putinfo->fd;			/* originally opened in webdav_open() */

		/* Find out the file's length */
		local_len = lseek(local, 0, SEEK_END);
		if (local_len == -1)
		{
			warn("lseek");
			myreturn = EIO;
			goto out;
		}
		(void)lseek(local, 0, SEEK_SET);
	}
	else
	{
		local = -1;								/* indicates 0 length put */
	}

retry:
	
	n = 0;

	if (http_auth)
	{
		free(http_auth);
	}
	http_auth = NULL;
	{
		WebdavAuthcacheRetrieveRec http_auth_struct =
		{
			fs->fs_uid, https ->http_remote_request, append_to_file, "PUT", NULL
		};
		if (!webdav_authcache_retrieve(&http_auth_struct))
		{
			http_auth = http_auth_struct.authorization;
		}
	}

	addstr(iov, n, "PUT ");
	addstr(iov, n, https->http_remote_request);
	if (append_to_file)
	{
		addstr(iov, n, append_to_file);
	}
	addstr(iov, n, " HTTP/1.1\r\n");
	addstr(iov, n, gUserAgentHeader);

	/* do content negotiation here */
	addstr(iov, n, "Accept: */*\r\n");
	addstr(iov, n, https->http_host_header);
	snprintf(lengthline, SHORT_HTTP_LINELEN, "Content-Length: %qd\r\n", local_len);
	addstr(iov, n, lengthline);
	if ((arg != (void *) - 1) && (putinfo->locktoken))
	{
		addstr(iov, n, "If:(<");
		addstr(iov, n, putinfo->locktoken);
		addstr(iov, n, ">)\r\n");
	}

	if (http_auth)
	{
		addstr(iov, n, http_auth);
	}

	addstr(iov, n, "\r\n");

	if (n >= NIOV)
	{
		myreturn = EIO;
		goto out;
	}

	reconnected = 0;
	redirection = 0;
	retrying = 0;

	fs->fs_status = "creating request message";
	msg.msg_name = (caddr_t) & http_sin;
	msg.msg_namelen = sizeof http_sin;
	msg.msg_iov = iov;
	msg.msg_control = 0;
	msg.msg_controllen = 0;
	msg.msg_flags = 0;

	/* open the socket if needed */
	if (*(fs->fs_socketptr) < 0)
	{
		if (http_socket_reconnect(fs->fs_socketptr, fs->fs_use_connect, 0))
		{
			myreturn = EIO;
			goto out;
		}
	}

reconnect:

	msg.msg_iovlen = n;

	fs->fs_status = "sending request message";

	if (logfile)
	{
		fprintf(logfile, "%s", msg.msg_iov[0].iov_base);
		fprintf(logfile, "%s On Socket %d", msg.msg_iov[1].iov_base, *(fs->fs_socketptr));
		fprintf(logfile, " Length = %qd\n", local_len);
	}
	
	/* If not reconnecting, use WEBDAV_IO_TIMEOUT, else use the default timeout */
	http_set_socket_sndtimeo(*(fs->fs_socketptr), reconnected ? 0 : WEBDAV_IO_TIMEOUT);

	writeresult = sendmsg(*(fs->fs_socketptr), &msg, 0);
	if (writeresult < 0)
	{
		if (!reconnected)
		{
			if (http_socket_reconnect(fs->fs_socketptr, fs->fs_use_connect, 1))
			{
				myreturn = EIO;
				goto out;
			}
			reconnected = 1;
			goto reconnect;
		}
		else
		{
			warnx("sendmsg (http_put): error after reconnect to %s", http_hostname);
			myreturn = EIO;
			goto out;
		}
	}

	/*
	 * Note that we will stop writing if we get an error but we won't
	 * return that.	 We'll see what the server has to say, or we'll
	 * try again.
	 */

	if (local != -1)
	{
		do
		{
			readresult = read(local, buf, sizeof buf);
			bytes_written += readresult;
			if (readresult)
			{
				writeresult = send(*(fs->fs_socketptr), buf, readresult, 0);
				if (writeresult < 0)
				{
					if (!reconnected)
					{
						if (http_socket_reconnect(fs->fs_socketptr, fs->fs_use_connect, 1))
						{
							myreturn = EIO;
							goto out;
						}
						reconnected = 1;
			
						/* Ok now set up to reread the file since we will be
						* retrying */
						if (local != -1)
						{
							(void)lseek(local, 0, SEEK_SET);
						}
			
						bytes_written = 0;
						goto reconnect;
					}
				}
			}
		} while (readresult != 0 && bytes_written < local_len && writeresult != -1);
	}

	myreturn = http_parse_response_header(fs, &total_length, &dav_status, &chunked);

	if (myreturn == EAGAIN)
	{
#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
		fprintf(stderr, "http_put: Got error %d from parse response header\n", myreturn);
#endif


		if (!reconnected)
		{
			if (http_socket_reconnect(fs->fs_socketptr, fs->fs_use_connect, 1))
			{
				myreturn = EIO;
				goto out;
			}
			reconnected = 1;

			/* Ok now set up to reread the file since we will be
			 * retrying */
			if (local != -1)
			{
				(void)lseek(local, 0, SEEK_SET);
			}

			bytes_written = 0;
			goto reconnect;
		}
		else
		{
			warnx("empty reply from %s", http_hostname);
			myreturn = EIO;
			goto out;
		}
	}
	else
	{
		if (myreturn == 204)
		{
			/*
			 * 204 by definition means no body so just move
			 * along. Apache mod_dav will return this if we are
			 * putting data into an empty file even after the
			 * data goes in.
			 */
			myreturn = 0;
		}
		else
		{
			if (myreturn == EAUTH)
			{
				myreturn = 0;
				goto retry;
			}
		}
	}

	if ((total_length != -1) || chunked)
	{
		(void) http_clean_socket(fs->fs_socketptr, total_length, chunked);
	}

	/* *** check whether the server has closed the connection *** */
	if ((*(fs->fs_socketptr) >= 0) && ((proxy_ok && (!proxy_exception)) || https->connection_close))
	{
		(void)close(*(fs->fs_socketptr));
		*(fs->fs_socketptr) = -1;
	}

out:
	
	if (http_auth)
	{
		free(http_auth);
	}

	return (myreturn);
}

/*****************************************************************************/

/*
 * Establish webdav connection and ensure DAV support
 */

int http_mount(struct fetch_state *fs, void *arg)
{
	struct iovec iov[NIOV];
	struct http_state *https;
	off_t header;								/* length of the xml string */
	int n = 0;
	struct msghdr msg;
	int status;
	size_t total_length;
	int redirection, chunked;
	int dav_status;
	int reconnected = 0;
	char *http_auth = NULL;

	header = 0;
	redirection = 0;

	https = fs->fs_proto;
	*((int *)arg) = 0;

retry:
	
	n = 0;

	if (http_auth)
	{
		free(http_auth);
	}
	http_auth = NULL;
	{
		WebdavAuthcacheRetrieveRec http_auth_struct =
		{
			fs->fs_uid, https ->http_remote_request, append_to_file, "OPTIONS", NULL
		};
		if (!webdav_authcache_retrieve(&http_auth_struct))
		{
			http_auth = http_auth_struct.authorization;
		}
	}

	addstr(iov, n, "OPTIONS ");
	addstr(iov, n, https->http_remote_request);
	if (append_to_file)
	{
		addstr(iov, n, append_to_file);
	}
	addstr(iov, n, " HTTP/1.1\r\n");
	addstr(iov, n, gUserAgentHeader);

	/* do content negotiation here */
	addstr(iov, n, "Accept: */*\r\n");
	addstr(iov, n, https->http_host_header);

	if (http_auth)
	{
		addstr(iov, n, http_auth);
	}

	addstr(iov, n, "Content-Length: 0\r\n");
	addstr(iov, n, "\r\n");

	if (n >= NIOV)
	{
		status = EIO;
		goto out;
	}

	fs->fs_status = "creating request message";
	msg.msg_name = (caddr_t) & http_sin;
	msg.msg_namelen = sizeof http_sin;
	msg.msg_iov = iov;
	msg.msg_control = 0;
	msg.msg_controllen = 0;
	msg.msg_flags = 0;

	msg.msg_iovlen = n;

	fs->fs_status = "sending request message";

	/* close old socket and open a new socket */
	if (http_socket_reconnect(fs->fs_socketptr, fs->fs_use_connect, 0))
	{
		status = ENODEV;
		goto out;
	}

	/* use WEBDAV_IO_TIMEOUT */
	http_set_socket_sndtimeo(*(fs->fs_socketptr), WEBDAV_IO_TIMEOUT);

reconnect:

	if (sendmsg(*(fs->fs_socketptr), &msg, 0) < 0)
	{
		if (!reconnected)
		{
#ifdef DEBUG
			fprintf(stderr, "errno was %d\n", errno);
#endif

			if (http_socket_reconnect(fs->fs_socketptr, fs->fs_use_connect, 1))
			{
				status = ENODEV;
				goto out;
			}
			reconnected = 1;
			goto reconnect;
		}
		else
		{
			warnx("sendmsg (http_mount): error after reconnect to %s", http_hostname);
			status = ENODEV;
			goto out;
		}
	}

	status = http_parse_response_header(fs, &total_length, &dav_status, &chunked);
	if (status)
	{
		if (status == EAUTH)
		{
			goto retry;
		}
		else if (status != EACCES)
		{
			/* authentication failed */
			status = ENODEV;
		}
	}
	else
	{
		switch (dav_status)
		{
			case 1:
				*((int *)arg) |= MNT_RDONLY;
				break;
				
			case 2:
				break;
				
			default:
				status = ENODEV;
				break;
		}
	}

	if ((total_length != -1) || chunked)
	{
		(void) http_clean_socket(fs->fs_socketptr, total_length, chunked);
	}

out:

	if (http_auth)
	{
		free(http_auth);
	}

#ifdef NOT_YET
	/* *** check whether the server has closed the connection *** */
	if ((*(fs->fs_socketptr) >= 0) && ((proxy_ok && (!proxy_exception)) || https->connection_close))
	{
		(void)close(*(fs->fs_socketptr));
		*(fs->fs_socketptr) = -1;
	}
#endif

	return (status);
}

/*****************************************************************************/

/*
 * The format of the response line for an HTTP request is:
 *	HTTP/V.vv{WS}999{WS}Explanatory text for humans to read\r\n
 * Old pre-HTTP/1.0 servers can return
 *	HTTP{WS}999{WS}Explanatory text for humans to read\r\n
 * Where {WS} represents whitespace (spaces and/or tabs) and 999
 * is a machine-interprable result code.  We return the integer value
 * of that result code, or the impossible value `0' if we are unable to
 * parse the result.
 */
static int http_first_line(char *linebuf)
{
	char *ep,  *line = linebuf;
	unsigned long ul;

	zero_trailing_spaces(line);

	/* make sure that this is not HTTP .9 */
	if (strlen(line) < 5 || strncasecmp(line, "http", 4) != 0)
	{
		return -1;
	}

	/* get past the HTTP identifier, e.g. "HTTP/1.1 " */
	line += 4;
	while (*line && !isspace(*line))
	{
		/* skip non-whitespace */
		line++;
	}
	while (*line && isspace(*line))
	{
		/* skip first whitespace */
		line++;
	}

	/* now find the return code */
	errno = 0;
	ul = strtoul(line, &ep, 10);
	if (errno != 0 || ul > 999 || ul < 100 || (*ep && !isspace(*ep)))
	{
		return 0;
	}
	
	return (int)ul;
}

/*****************************************************************************/

/*
 * The format of a header line for an HTTP request is:
 *	Header-Name: header-value (with comments in parens)\r\n
 * This would be a nice application for gperf(1), except that the
 * names are case-insensitive and gperf can't handle that.
 */
static enum http_header http_parse_header(char *line, char **valuep)
{
	char *colon,  *value;

	zero_trailing_spaces(line);
	if (*line == '\0')
	{
		return ht_end_of_header;
	}

	colon = strchr(line, ':');
	if (colon == 0)
	{
		return ht_syntax_error;
	}

	/* null terminate the "name" string */
	*colon = '\0';

	/* find the start of the "value" string */
	for (value = colon + 1; *value && isspace(*value); value++)
	{
		/* do nothing */
	}
	*valuep = value;

#define cmp(name, num) do { if (!strcasecmp(line, name)) return num; } while(0)
	cmp("Accept-Ranges", ht_accept_ranges);
	cmp("Age", ht_age);
	cmp("Allow", ht_allow);
	cmp("Cache-Control", ht_cache_control);
	cmp("Connection", ht_connection);
	cmp("Content-Base", ht_content_base);
	cmp("Content-Encoding", ht_content_encoding);
	cmp("Content-Language", ht_content_language);
	cmp("Content-Length", ht_content_length);
	cmp("Content-Location", ht_content_location);
	cmp("Content-MD5", ht_content_md5);
	cmp("Content-Range", ht_content_range);
	cmp("Content-Type", ht_content_type);
	cmp("Date", ht_date);
	cmp("DAV", ht_dav);
	cmp("ETag", ht_etag);
	cmp("Expires", ht_expires);
	cmp("Last-Modified", ht_last_modified);
	cmp("Location", ht_location);
	cmp("Pragma", ht_pragma);
	cmp("Proxy-Authenticate", ht_proxy_authenticate);
	cmp("Public", ht_public);
	cmp("Retry-After", ht_retry_after);
	cmp("Server", ht_server);
	cmp("Transfer-Encoding", ht_transfer_encoding);
	cmp("Upgrade", ht_upgrade);
	cmp("Vary", ht_vary);
	cmp("Via", ht_via);
	cmp("WWW-Authenticate", ht_www_authenticate);
	cmp("Warning", ht_warning);
#undef cmp
	return ht_unknown;
}

/*****************************************************************************/

/*
 * Compute the RSA Data Security, Inc., MD5 Message Digest of the file
 * given in `fp', see if it matches the one given in base64 encoding by
 * `base64ofmd5'.  Warn and return an error if it doesn't.
 */
static int check_md5(int fd, char *base64ofmd5)
{
	return 0;
}

/*****************************************************************************/

static const char *wkdays[] = { 
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};
static const char *months[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct",
	"Nov", "Dec"
};

/*****************************************************************************/

/*
 * Interpret one of the three possible formats for an HTTP date.
 * All of them are really bogus; HTTP should use either ISO 8601
 * or NTP timestamps.  We make some attempt to accept a subset of 8601
 * format.	The three standard formats are all fixed-length subsets of their
 * respective standards (except 8601, which puts all of the stuff we
 * care about up front).
 */
time_t parse_http_date(char *string)
{
	struct tm tm;
	time_t rv;
	int i;

	/* clear tm */
	bzero(&tm, sizeof(tm));
	
	/* 8601 has the shortest minimum length */
	if (strlen(string) < 15)
	{
		return -1;
	}

	if (isdigit(*string))
	{
		/* ISO 8601: 19970127T134551stuffwedon'tcareabout */
		for (i = 0; i < 15; i++)
		{
			if (i != 8 && !isdigit(string[i]))
			{
				break;
			}
		}
		if (i < 15)
		{
			return -1;
		}
#define digit(x) (string[x] - '0')
		tm.tm_year = (digit(0) * 1000 + digit(1) * 100 + digit(2) * 10 + digit(3)) - 1900;
		tm.tm_mon = digit(4) * 10 + digit(5) - 1;
		tm.tm_mday = digit(6) * 10 + digit(7);
		if (string[8] != 'T' && string[8] != 't' && string[8] != ' ')
		{
			return -1;
		}
		tm.tm_hour = digit(9) * 10 + digit(10);
		tm.tm_min = digit(11) * 10 + digit(12);
		tm.tm_sec = digit(13) * 10 + digit(14);
		/* We don't care about the rest of the stuff after the secs. */
	}
	else if (string[3] == ',')
	{
		/* Mon, 27 Jan 1997 14:24:35 stuffwedon'tcareabout */
		if (strlen(string) < 25)
		{
			return -1;
		}
		string += 5;							/* skip over day-of-week */
		if (!(isdigit(string[0]) && isdigit(string[1])))
		{
			return -1;
		}
		tm.tm_mday = digit(0) * 10 + digit(1);
		for (i = 0; i < 12; i++)
		{
			if (strncasecmp(months[i], &string[3], 3) == 0)
			{
				break;
			}
		}
		if (i >= 12)
		{
			return -1;
		}
		tm.tm_mon = i;

		if (sscanf(&string[7], "%d %d:%d:%d", &i, &tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 4)
		{
			/* Might be Mon, 27 Jan 14:24:35 1997 stuffwedon'tcareabout */
			/* let's try that before giving up */
			if (sscanf(&string[7], "%d:%d:%d %d", &tm.tm_hour, &tm.tm_min, &tm.tm_sec, &i) != 4)
			{
				return -1;
			}
		}
		tm.tm_year = i - 1900;

	}
	else if (string[3] == ' ')
	{
		/* Mon Jan 27 14:25:20 1997 */
		if (strlen(string) < 24)
		{
			return -1;
		}
		string += 4;
		for (i = 0; i < 12; i++)
		{
			if (strncasecmp(string, months[i], 3) == 0)
			{
				break;
			}
		}
		if (i >= 12)
		{
			return -1;
		}
		tm.tm_mon = i;
		if (sscanf(&string[4], "%d %d:%d:%d %u", &tm.tm_mday, &tm.tm_hour, &tm.tm_min,
			&tm.tm_sec, &i) != 5)
		{
			return -1;
		}
		tm.tm_year = i - 1900;
	}
	else
	{
		/* Monday, 27-Jan-97 14:31:09 stuffwedon'tcareabout */
		/* Quoth RFC 2068:
		  o	 HTTP/1.1 clients and caches should assume that an RFC-850 date
		  which appears to be more than 50 years in the future is in fact
		  in the past (this helps solve the "year 2000" problem).
		*/
		time_t now;
		struct tm *tmnow;
		int this2dyear;
		char *comma = strchr(string, ',');
		char mname[4];

		if (comma == 0)
		{
			return -1;
		}
		string = comma + 1;
		if (strlen(string) < 19)
		{
			return -1;
		}
		string++;
		mname[4] = '\0';
		if (sscanf(string, "%d-%c%c%c-%d %d:%d:%d", &tm.tm_mday, mname, mname + 1,
			mname + 2, &tm.tm_year, &tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 8)
		{
			return -1;
		}
		for (i = 0; i < 12; i++)
		{
			if (strcasecmp(months[i], mname))
			{
				break;
			}
		}
		if (i >= 12)
		{
			return -1;
		}
		tm.tm_mon = i;
		/*
		 * RFC 2068 year interpretation.
		 */
		time(&now);
		tmnow = gmtime(&now);
		this2dyear = tmnow->tm_year % 100;
		tm.tm_year += tmnow->tm_year - this2dyear;
		if (tm.tm_year - tmnow->tm_year >= 50)
		{
			tm.tm_year -= 100;
		}
	}
#undef digit

	if (tm.tm_sec > 60 || tm.tm_min > 59 || tm.tm_hour > 23 || tm.tm_mday > 31 || tm.tm_mon > 11)
	{
		return -1;
	}
	if (tm.tm_sec < 0 || tm.tm_min < 0 || tm.tm_hour < 0 || tm.tm_mday < 0 ||
		tm.tm_mon < 0 || tm.tm_year < 0)
	{
		return -1;
	}

	rv = mktime(&tm);

	return rv;
}

/*****************************************************************************/

static void format_http_date(time_t when, char *buf)
{
	struct tm	*tm;
	
	tm = gmtime(&when);
	if (tm != 0)
	{
#ifndef HTTP_DATE_ISO_8601
		sprintf(buf, "%s, %02d %s %04d %02d:%02d:%02d GMT", wkdays[tm->tm_wday], tm->tm_mday,
			months[tm->tm_mon], tm->tm_year + 1900, tm->tm_hour, tm->tm_min, tm->tm_sec);
#else /* ISO 8601 */
		sprintf(buf, "%04d%02d%02dT%02d%02d%02d+0000", tm->tm_year + 1900, tm->tm_mon + 1,
			tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
#endif
	}
	else
	{
		*buf = '\0';
	}
}

/*****************************************************************************/

/*
 * Parse a Content-Range return header from the server.	 RFC 2066 defines
 * this header to have the format:
 *	Content-Range: bytes 12345-67890/123456
 */
static int parse_http_content_range(char *orig, off_t *restart_from, off_t *total_length)
{
	u_quad_t first, last, total;
	char *ep;

	if (strncasecmp(orig, "bytes", 5) != 0)
	{
		warnx("unknown Content-Range unit: `%s'", orig);
		return EIO;
	}

	orig += 5;
	while (*orig && isspace(*orig))
	{
		orig++;
	}

	errno = 0;
	first = strtouq(orig, &ep, 10);
	if (errno != 0 || *ep != '-')
	{
		warnx("invalid Content-Range: `%s'", orig);
		return EIO;
	}
	last = strtouq(ep + 1, &ep, 10);
	if (errno != 0 || *ep != '/' || last < first)
	{
		warnx("invalid Content-Range: `%s'", orig);
		return EIO;
	}
	total = strtouq(ep + 1, &ep, 10);
	if (errno != 0 || !(*ep == '\0' || isspace(*ep)))
	{
		warnx("invalid Content-Range: `%s'", orig);
		return EIO;
	}

	*restart_from = first;
	*total_length = last;
	
	return 0;
}

/*****************************************************************************/

#undef addstr

/*****************************************************************************/
