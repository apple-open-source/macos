/*
 * Copyright (c) 2019 Kristaps Dzonsons <kristaps@bsd.lv>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include "config.h"

#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <netdb.h>
#include <poll.h>
#if HAVE_READPASSPHRASE
#include <readpassphrase.h>
#endif
#include <resolv.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <limits.h>
#if HAVE_ERR
# include <err.h>
#endif

#include "extern.h"

#define	RSYNC_LISTEN_BACKLOG	5

/*
 * Defaults from the reference rsync; the max password size is specifically for
 * password files, and not otherwise strictly enforced.
 */
#define	RSYNCD_DEFAULT_USER	"nobody"
#define	RSYNCD_MAX_PASSWORDSZ	511

/*
 * Negative values don't make sense for any of the options we support, so we use
 * so_default_value < 0 with so_has_arg == true to indicate an option that
 * requires a value be specified.  so_has_arg == true with so_default_value >= 0
 * indicates an optional arg, and we'll use the so_default_value if no argument
 * was provided.  so_has_arg == false means we'll error out if a value is
 * provided, and the so_default_value will be used if the option is present.
 */
#define	SOCKOPT(name, level, has_arg, dflt)	\
	{ #name, name, level, dflt, has_arg }
static const struct sockopt {
	const char	*so_name;
	int		 so_nameval;
	int		 so_level;
	int		 so_default_value;
	bool		 so_has_arg;
} sockopts[] = {
	/*
	 * POSIX specified options first, other useful options that may be more
	 * system-dependent come after.
	 */
	SOCKOPT(SO_KEEPALIVE, SOL_SOCKET, true, 1),
	SOCKOPT(SO_REUSEADDR, SOL_SOCKET, true, 1),
	SOCKOPT(SO_SNDBUF, SOL_SOCKET, true, -1),
	SOCKOPT(SO_RCVBUF, SOL_SOCKET, true, -1),
	SOCKOPT(SO_SNDLOWAT, SOL_SOCKET, true, -1),
	SOCKOPT(SO_RCVLOWAT, SOL_SOCKET, true, -1),
	SOCKOPT(SO_SNDTIMEO, SOL_SOCKET, true, -1),
	SOCKOPT(SO_RCVTIMEO, SOL_SOCKET, true, -1),
#ifdef SO_REUSEPORT
	SOCKOPT(SO_REUSEPORT, SOL_SOCKET, true, 1),
#endif
};

/*
 * Defines a resolved IP address for the host
 * There can be many, IPV4 or IPV6.
 */
struct	source {
	int		 family; /* PF_INET or PF_INET6 */
	char		 ip[INET6_ADDRSTRLEN]; /* formatted string */
	struct sockaddr_storage sa; /* socket */
	socklen_t	 salen; /* length of socket buffer */
};

/*
 * Try to bind to a local IP address matching the address family passed.
 * Return -1 on failure to bind to any address, 0 on success.
 */
static int
inet_bind(int s, sa_family_t af, const struct source *bsrc, size_t bsrcsz)
{
	size_t i;

	if (bsrc == NULL)
		return 0;
	for (i = 0; i < bsrcsz; i++) {
		if (bsrc[i].family != af)
			continue;
		if (bind(s, (const struct sockaddr *)&bsrc[i].sa,
		    bsrc[i].salen) == -1)
			continue;
		return 0;
	}
	return -1;
}

static const struct sockopt *
inet_resolve_sockopt(const char *name)
{
	const struct sockopt *soptdef;

	for (size_t i = 0; i < nitems(sockopts); i++) {
		soptdef = &sockopts[i];
		if (strcmp(name, soptdef->so_name) == 0)
			return soptdef;
	}

	return NULL;
}

static int
inet_setsockopt(const struct sockopt *soptdef, int sock, int value)
{
	socklen_t valsz = sizeof(value);

	LOG3("sockopts: setting '%s' to '%d'", soptdef->so_name, value);
	if (setsockopt(sock, soptdef->so_level, soptdef->so_nameval,
	    &value, valsz) == -1) {
		ERR("setsockopt %s", soptdef->so_name);
		return -1;
	}

	return 0;
}

static int
inet_setsockopts(int sock, const char *options)
{
	const struct sockopt *soptdef;
	char *name, *part, *sockopts, *value;
	int error, sockval;

	if (options == NULL || options[0] == '\0')
		return 0;

	sockopts = strdup(options);
	if (sockopts == NULL) {
		ERR("strdup");
		return -1;
	}

	error = -1;
	part = sockopts;
	while ((name = strsep(&part, ",")) != NULL) {
		/* Empty fields are OK. */
		if (*name == '\0')
			continue;

		value = strchr(name, '=');
		if (value != NULL) {
			*value = '\0';
			value++;
		}

		soptdef = inet_resolve_sockopt(name);
		if (soptdef == NULL) {
			ERRX("Unresolvable socket option '%s'", name);
			goto out;
		}

		if (value != NULL) {
			const char *converr;

			if (!soptdef->so_has_arg) {
				ERRX(
				    "Socket option '%s' does not accept an argument",
				    soptdef->so_name);
				goto out;
			}

			sockval = (int)strtonum(value, 0, INT_MAX, &converr);
			if (converr != NULL) {
				ERRX("Error parsing value for socket option '%s': %s",
				    soptdef->so_name, converr);
				goto out;
			}
		} else {
			sockval = soptdef->so_default_value;
		}

		if (soptdef->so_has_arg && sockval < 0) {
			ERRX(
			    "Value required for socket option '%s'",
			    soptdef->so_name);
			goto out;
		}

		/* Process this socket option now. */
		if (inet_setsockopt(soptdef, sock, sockval) == -1)
			goto out;
	}

	error = 0;

out:
	free(sockopts);
	return error;
}

/*
 * Connect to an IP address representing a host.
 * Return <0 on failure, 0 on try another address, >0 on success.
 */
static int
inet_connect(const struct opts *opts, int *sd, const struct source *src,
    const char *host, const struct source *bsrc, size_t bsrcsz)
{
	struct pollfd	pfd;
	socklen_t	optlen;
	int		c;
	int		optval;

	if (*sd != -1)
		close(*sd);

	LOG2("trying: %s, %s", src->ip, host);

#if HAVE_SOCK_NONBLOCK
	if ((*sd = socket(src->family, SOCK_STREAM | SOCK_NONBLOCK, 0))
	    == -1) {
		ERR("socket");
		return -1;
	}
#else
	if ((*sd = socket(src->family, SOCK_STREAM, 0))
	    == -1) {
		ERR("socket");
		return -1;
	}

	if (fcntl(*sd, F_SETFL, fcntl(*sd, F_GETFL, 0) | O_NONBLOCK) == -1)
		err(ERR_IPC, "fcntl");
#endif

	/* inet_setsockopts will produce a more specific error. */
	if (inet_setsockopts(*sd, opts->sockopts) == -1)
		return -1;

	if (inet_bind(*sd, src->family, bsrc, bsrcsz) == -1) {
		ERR("bind");
		return -1;
	}

	/*
	 * Initiate blocking connection.
	 * We use non-blocking connect() so we can poll() for contimeout.
	 */

	if ((c = connect(*sd, (const struct sockaddr *)&src->sa, src->salen))
	    != 0 && errno == EINPROGRESS) {
		pfd.fd = *sd;
		pfd.events = POLLOUT;
		switch (c = poll(&pfd, 1, poll_contimeout)) {
		case 1:
			optlen = sizeof(optval);
			if ((c = getsockopt(*sd, SOL_SOCKET, SO_ERROR, &optval,
			    &optlen)) == 0) {
				errno = optval;
				if (optval != 0)
					c = -1;
			}
			break;
		case 0:
			errno = ETIMEDOUT;
			WARNX("connect timeout: %s, %s", src->ip, host);
			return 0;
		default:
			ERR("poll failed");
			return -1;
		}
	}
	if (c == -1) {
		if (errno == EADDRNOTAVAIL)
			return 0;
		if (errno == ECONNREFUSED || errno == EHOSTUNREACH) {
			WARNX("connect refused: %s, %s", src->ip, host);
			return 0;
		}
		ERR("connect");
		return -1;
	}

	return 1;
}

static int
inet_listen(const struct opts *opts, int *sock, const struct source *bsrc)
{
	int flags;

	if ((*sock = socket(bsrc->family, SOCK_STREAM, 0)) == -1) {
		ERR("socket");
		return -1;
	}

#ifdef IPV6_V6ONLY
	if (bsrc->family == PF_INET6) {
		int v6only = 1;

		/*
		 * Don't assume a given default for IPV6_V6ONLY, set it
		 * explicitly.
		 */
		if (setsockopt(*sock, IPPROTO_IPV6, IPV6_V6ONLY,
		    &v6only, sizeof(v6only)) == -1) {
			ERR("setsockopt");
			goto failed;
		}
	}
#endif

	/* inet_setsockopts will produce a more specific error. */
	if (inet_setsockopts(*sock, opts->sockopts) == -1)
		goto failed;

	if (inet_bind(*sock, bsrc->family, bsrc, 1) == -1) {
		ERR("bind");
		goto failed;
	}

	/* Set up non-blocking mode, we'll be polling anyways. */
	if ((flags = fcntl(*sock, F_GETFL, 0)) == -1) {
		ERR("fcntl");
		goto failed;
	} else if (fcntl(*sock, F_SETFL, flags | O_NONBLOCK) == -1) {
		ERR("fcntl");
		goto failed;
	}

	if (listen(*sock, RSYNC_LISTEN_BACKLOG) == -1) {
		ERR("listen");
		goto failed;
	}

	return 0;

failed:
	close(*sock);
	*sock = -1;
	return -1;
}

static int
inet_resolve_port(const char *portstr, uint16_t *oport)
{
	struct servent *serv;

	if (isdigit(portstr[0])) {
		const char *err;

		*oport = strtonum(portstr, 0, USHRT_MAX, &err);
		if (err == NULL)
			return 0;

		/*
		 * Try to resolve it with /etc/services instead, some services
		 * may begin with a digit as well.
		 */
	}

	if ((serv = getservbyname(portstr, "tcp")) == NULL) {
		ERR("getservbyname: %s", portstr);
		return -1;
	}

	/*
	 * We're going to want it in network-byte order eventually anyways, but
	 * push it to host-byte order for any logging and consistency with the
	 * above.
	 */
	*oport = ntohs(serv->s_port);
	return 0;
}

static struct source *
inet_get_any(struct sess *sess, size_t *osz)
{
	struct source *curr, *src;
	size_t sz;
	uint16_t port;
	int ipf_needed;

	if (inet_resolve_port(sess->opts->port, &port) != 0)
		return NULL;

	if ((ipf_needed = sess->opts->ipf) == 0)
		sz = 2;
	else
		sz = 1;

	src = calloc(sz, sizeof(*src));
	if (src == NULL) {
		ERR("calloc");
		return NULL;
	}

	/* XXX We don't currently populate src->ip, for better or worse. */
	curr = NULL;
#define	SRC_NEXT(src, curr)	((curr) == (NULL) ? (src) : (curr) + 1)
	if (ipf_needed == 0 || ipf_needed == 4) {
		struct sockaddr_in *sin;

		curr = SRC_NEXT(src, curr);
		curr->family = PF_INET;

		sin = (void *)&curr->sa;
		memset(sin, 0, sizeof(*sin));
		sin->sin_len = sizeof(*sin);
		sin->sin_family = AF_INET;
		sin->sin_port = htons(port);
		sin->sin_addr.s_addr = INADDR_ANY;

		curr->salen = sin->sin_len;
	}

	if (ipf_needed == 0 || ipf_needed == 6) {
		struct sockaddr_in6 *sin6;

		curr = SRC_NEXT(src, curr);
		curr->family = PF_INET6;

		sin6 = (void *)&curr->sa;
		memset(sin6, 0, sizeof(*sin6));
		sin6->sin6_len = sizeof(*sin6);
		sin6->sin6_family = AF_INET6;
		sin6->sin6_port = htons(port);
		sin6->sin6_addr = in6addr_any;

		curr->salen = sin6->sin6_len;
	}

	*osz = sz;
	return src;
}

/*
 * Resolve the socket addresses for host, both in IPV4 and IPV6.
 * Once completed, the "dns" pledge may be dropped.
 * Returns the addresses on success, NULL on failure (sz is always zero,
 * in this case).
 */
static struct source *
inet_resolve(struct sess *sess, const char *host, size_t *sz, int passive)
{
	struct addrinfo	 hints, *res0, *res;
	struct sockaddr	*sa;
	struct source	*src = NULL;
	const char	*port = sess->opts->port;
	size_t		 i, srcsz = 0;
	int		 error;

	*sz = 0;

	memset(&hints, 0, sizeof(hints));
	if (sess->opts->ipf == 4)
		hints.ai_family = AF_INET;
	else if (sess->opts->ipf == 6)
		hints.ai_family = AF_INET6;
	else
		hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if (passive) {
		hints.ai_flags = SOCK_STREAM;
		port = NULL;
	}

	error = getaddrinfo(host, port, &hints, &res0);

	LOG2("resolving: %s", host);

	if (error == EAI_AGAIN || error == EAI_NONAME) {
		ERRX("could not resolve hostname %s: %s",
		    host, gai_strerror(error));
		return NULL;
	} else if (error == EAI_SERVICE) {
		ERRX("could not resolve service rsync: %s",
		    gai_strerror(error));
		return NULL;
	} else if (error) {
		ERRX("getaddrinfo: %s: %s", host, gai_strerror(error));
		return NULL;
	}

	/* Allocate for all available addresses. */

	for (res = res0; res != NULL; res = res->ai_next)
		if (res->ai_family == AF_INET ||
		    res->ai_family == AF_INET6)
			srcsz++;

	if (srcsz == 0) {
		ERRX("no addresses resolved: %s", host);
		freeaddrinfo(res0);
		return NULL;
	}

	src = calloc(srcsz, sizeof(struct source));
	if (src == NULL) {
		ERRX("calloc");
		freeaddrinfo(res0);
		return NULL;
	}

	for (i = 0, res = res0; res != NULL; res = res->ai_next) {
		if (res->ai_family != AF_INET &&
		    res->ai_family != AF_INET6)
			continue;

		assert(i < srcsz);

		/* Copy the socket address. */

		src[i].salen = res->ai_addrlen;
		memcpy(&src[i].sa, res->ai_addr, src[i].salen);

		/* Format as a string, too. */

		sa = res->ai_addr;
		if (res->ai_family == AF_INET) {
			src[i].family = PF_INET;
			inet_ntop(AF_INET,
			    &(((struct sockaddr_in *)sa)->sin_addr),
			    src[i].ip, INET6_ADDRSTRLEN);
		} else {
			src[i].family = PF_INET6;
			inet_ntop(AF_INET6,
			    &(((struct sockaddr_in6 *)sa)->sin6_addr),
			    src[i].ip, INET6_ADDRSTRLEN);
		}

		LOG2("hostname resolved: %s: %s", host, src[i].ip);
		i++;
	}

	freeaddrinfo(res0);
	*sz = srcsz;
	return src;
}

static char *
protocol_auth_getpass(struct sess *sess, char *buf, size_t bufsz)
{
	const char *envpass;

	memset(buf, 0, bufsz);

	envpass = getenv("RSYNC_PASSWORD");
	if (sess->opts->password_file != NULL) {
		int fd;

		fd = open(sess->opts->password_file, O_RDONLY);
		if (fd == -1) {
			ERR("%s: open", sess->opts->password_file);
			return NULL;
		}

		if (check_file_mode(sess->opts->password_file, fd)) {
			ssize_t readsz;

			if (envpass != NULL)
				ERRX("RSYNC_PASSWORD environment variable set but not used in favor of --password-file");

			readsz = read(fd, buf, bufsz - 1);
			if (readsz < 0) {
				ERR("%s: read", sess->opts->password_file);
				close(fd);
				goto prompt;
			}

			close(fd);

			if (readsz == 0)
				goto prompt;

			readsz = strcspn(buf, "\r\n");
			buf[readsz] = '\0';

			return &buf[0];
		}

		close(fd);
		ERRX("Ignoring password file '%s' due to strict mode violation",
		    sess->opts->password_file);
	}

	/*
	 * Fallback to using the environment if the password file wasn't
	 * suitable at all.
	 */
	if (envpass != NULL) {
		if (strlcpy(buf, envpass, bufsz) >= bufsz) {
			ERRX("RSYNC_PASSWORD value too large (max %zu)", bufsz - 1);
			return NULL;
		}

		return &buf[0];
	}

prompt:

	return readpassphrase("Password: ", buf, bufsz, 0);
}

int
rsync_password_hash(const char *password, const char *challenge,
    char *buf, size_t bufsz)
{
	MD4_CTX ctx;
	uint8_t hash[MD4_DIGEST_LENGTH];
	int32_t seed = 0;
	size_t len;

	MD4_Init(&ctx);
	MD4_Update(&ctx, &seed, sizeof(seed));
	MD4_Update(&ctx, password, strlen(password));
	MD4_Update(&ctx, challenge, strlen(challenge));
	MD4_Final(hash, &ctx);

	if (b64_ntop(hash, sizeof(hash), buf, bufsz) < 0)
		return 0;

	/*
	 * Omitted from the reference rsync's protocol documentation is that
	 * it's base64 and, more specifically, without padding.  Chop them off.
	 */
	len = strlen(buf);
	while (len > 0 && buf[len - 1] == '=') {
		buf[--len] = '\0';
	}

	return 1;
}

/*
 * Respond to an auth request.
 * Return <0 on failure, 1 on success.
 */
static int
protocol_auth(struct sess *sess, const char *user, int sd,
    const char *challenge)
{
	char password[RSYNCD_MAX_PASSWORDSZ + 1];
	char response[RSYNCD_CHALLENGE_RESPONSESZ];
	int rc;

	while (isspace(*challenge))
		challenge++;

	if (*challenge == '\0') {
		ERRX("Malformed auth challenge");
		return -1;
	}

	if (user == NULL)
		user = getenv("USER");
	if (user == NULL)
		user = getenv("LOGNAME");
	if (user == NULL)
		user = RSYNCD_DEFAULT_USER;

	if (protocol_auth_getpass(sess, password, sizeof(password)) == NULL) {
		ERRX("Failed to obtain password");
		return -1;
	}

	memset(response, 0, sizeof(response));
	rc = rsync_password_hash(password, challenge, response,
	    sizeof(response));
	explicit_bzero(password, sizeof(password));

	if (!rc) {
		ERRX("Password hashing failed");
		return -1;
	}

	/* Respond <user> <response> */
	if (!io_write_buf(sess, sd, user, strlen(user))) {
		ERR("io_write_buf");
		return -1;
	}

	if (!io_write_byte(sess, sd, ' ')) {
		ERR("io_write_byte");
		return -1;
	}

	if (!io_write_line(sess, sd, response)) {
		ERR("io_write_line");
		return -1;
	}

	return 0;
}

/*
 * Process an rsyncd preamble line.
 * This is either free-form text or @RSYNCD commands.
 * Return <0 on failure, 0 on try more lines, >0 on finished.
 */
static int
protocol_line(struct sess *sess, __attribute__((unused)) const char *host,
    const char *user, int sd, const char *cp)
{
	int	major, minor;

	if (strncmp(cp, "@RSYNCD: ", 9)) {
		if (sess->opts->no_motd == 0)
			LOG1("%s", cp);
		return 0;
	}

	cp += 9;
	while (isspace((unsigned char)*cp))
		cp++;

	/* @RSYNCD: OK indicates that we're finished. */

	if (strcmp(cp, "OK") == 0)
		return 1;

	if (strncmp(cp, "AUTHREQD", sizeof("AUTHREQ") - 1) == 0) {
		cp += sizeof("AUTHREQD") - 1;
		return protocol_auth(sess, user, sd, cp);
	}

	/*
	 * Otherwise, all we have left is our version.
	 * There are two formats: x.y (w/submodule) and x.
	 */

	if (sscanf(cp, "%d.%d", &major, &minor) == 2) {
		sess->rver = major;
		return 0;
	} else if (sscanf(cp, "%d", &major) == 1) {
		sess->rver = major;
		return 0;
	}

	ERRX("rsyncd protocol error: unknown command");
	return -1;
}

/*
 * Connect to a remote rsync://-enabled server sender.
 * Returns exit code 0 on success, 1 on failure.
 */
int
rsync_connect(const struct opts *opts, int *sd, const struct fargs *f)
{
	struct sess	  sess;
	struct source	 *src = NULL, *bsrc = NULL;
	size_t		  i, srcsz = 0, bsrcsz = 0;
	int		  c, rc = 1;

#ifndef __APPLE__
	if (pledge("stdio unix rpath wpath cpath dpath inet fattr chown dns getpw unveil",
	    NULL) == -1)
		err(ERR_IPC, "pledge");
#endif

	memset(&sess, 0, sizeof(struct sess));
	sess.opts = opts;

	assert(f->host != NULL);

	/* Resolve all IP addresses from the host. */

	if ((src = inet_resolve(&sess, f->host, &srcsz, 0)) == NULL) {
		ERRX1("inet_resolve");
		exit(1);
	}
	if (opts->address != NULL)
		if ((bsrc = inet_resolve(&sess, opts->address, &bsrcsz, 1)) ==
		    NULL) {
			ERRX1("inet_resolve bind");
			exit(1);
		}

	/* Drop the DNS pledge. */

	if (pledge("stdio unix rpath wpath cpath dpath fattr chown getpw inet unveil",
	    NULL) == -1) {
		ERR("pledge");
		exit(1);
	}

	/*
	 * Iterate over all addresses, trying to connect.
	 * When we succeed, then continue using the connected socket.
	 */

	assert(srcsz);
	for (i = 0; i < srcsz; i++) {
		c = inet_connect(opts, sd, &src[i], f->host, bsrc, bsrcsz);
		if (c < 0) {
			ERRX1("inet_connect");
			goto out;
		} else if (c > 0)
			break;
	}

	/* Drop the inet pledge. */
	if (pledge("stdio unix rpath wpath cpath dpath fattr chown getpw unveil",
	    NULL) == -1) {
		ERR("pledge");
		goto out;
	}

	if (i == srcsz) {
		ERRX("cannot connect to host: %s", f->host);
		goto out;
	}

	LOG2("connected: %s, %s", src[i].ip, f->host);

	free(src);
	free(bsrc);
	return 0;
out:
	free(src);
	free(bsrc);
	if (*sd != -1)
		close(*sd);
	return rc;
}

static void
listener_reap(int signal __attribute__((unused)))
{
	int status;

	while (waitpid(0, &status, WNOHANG) > 0) {
		continue;
	}
}

int
rsync_listen(struct sess *sess, rsync_client_handler *handler)
{
	struct pollfd	  pfd[2];	/* IPv4 and/or IPv6 */
	struct sigaction  sigact;
	const struct opts *opts = sess->opts;
	struct source	 *bsrc = NULL;
	size_t		  bsrcsz = 0, i;
	int		  c;

	/*
	 * We'll bind up to two sockets; one ipv4, one ipv6.
	 */
	assert(opts->port != NULL);

	pfd[0].fd = pfd[1].fd = -1;
	pfd[0].events = pfd[1].events = POLLIN;

#ifndef __APPLE__
	/*
	 * rsync daemon needs to include both receiver and sender privileges,
	 * since any given module /could/ use both.  Each client accepted will
	 * promptly tighten it down.
	 */
	if (pledge("stdio unix rpath wpath cpath dpath fattr chown getpw unveil inet proc dns",
	    NULL) == -1)
		err(ERR_IPC, "pledge");
#endif

	/*
	 * We can't drop DNS until after a client is accepted, I believe, as we
	 * might need to do a reverse lookup.  Don't quote me on that.
	 */
	if (opts->address != NULL) {
		if ((bsrc = inet_resolve(sess, opts->address, &bsrcsz, 1)) ==
		    NULL) {
			ERRX1("inet_resolve bind");
			exit(1);
		}
	} else {
		if ((bsrc = inet_get_any(sess, &bsrcsz)) == NULL)
			return ERR_IPC;
	}

	for (i = 0; i < bsrcsz; i++) {
		const struct source *cursrc;
		int *cursock;

		cursrc = &bsrc[i];
		cursock = &pfd[i].fd;

		/*
		 * inet_listen() will produce a more appropriate error; just
		 * return.
		 */
		if (inet_listen(opts, cursock, cursrc) != 0) {
			for (size_t j = 0; j < i; j++)
				close(pfd[j].fd);
			return ERR_IPC;
		}
	}

	sigact.sa_handler = listener_reap;
	sigact.sa_flags = SA_NOCLDSTOP;
	sigemptyset(&sigact.sa_mask);
	if (sigaction(SIGCHLD, &sigact, NULL) != 0) {
		ERR("sigaction");
		return ERR_IPC;
	}

	for (;;) {
		if ((c = poll(pfd, (nfds_t)bsrcsz, INFTIM)) == -1) {
			if (errno == EINTR)
				continue;
			ERR("poll");
			break;
		}

		/* No timeout here. */
		assert(c > 0);

		for (i = 0; i < bsrcsz; i++) {
			struct sockaddr_storage saddr;
			socklen_t slen;
			int clsock;
			pid_t pid;

			if ((pfd[i].revents & POLLIN) == 0)
				continue;

			slen = sizeof(saddr);
			clsock = accept(pfd[i].fd, (struct sockaddr *)&saddr,
			    &slen);
			if (clsock < 0) {
				/* XXX Log it, maybe? */
				continue;
			}

			pid = fork();
			if (pid != 0) {
				/*
				 * XXX Log it for pid < 0; in both cases, we do
				 * not need the client socket in the parent.
				 */
				close(clsock);
				continue;
			}

			/* Child, none of the listeners are needed anymore. */
			for (i = 0; i < bsrcsz; i++)
				close(pfd[i].fd);

			/* Reset SIGCHLD, let the client handler manage it. */
			(void)signal(SIGCHLD, SIG_DFL);

			_exit((*handler)(sess, clsock, &saddr, slen));
		}
	}

	return 1;
}

int
rsync_setsockopts(int fd, const char *options)
{

	return inet_setsockopts(fd, options);
}

/*
 * Talk to a remote rsync://-enabled server sender.
 * Returns exit code 0 on success, 1 on failure, 2 on failure with
 * incompatible protocols.
 */
int
rsync_socket(struct cleanup_ctx *cleanup_ctx, const struct opts *opts,
    int sd, const struct fargs *f)
{
	struct sess	  sess;
	size_t		  i, skip;
	int		  c, rc = 1;
	char		**args, buf[BUFSIZ];

#ifndef __APPLE__
	if (pledge("stdio unix rpath wpath cpath dpath fattr chown getpw unveil",
	    NULL) == -1)
		err(ERR_IPC, "pledge");
#endif

	memset(&sess, 0, sizeof(struct sess));
	sess.opts = opts;
	sess.mode = f->mode;
	sess.wbatch_fd = -1;
	sess.lver = sess.protocol = sess.opts->protocol;

	cleanup_set_session(cleanup_ctx, &sess);
	cleanup_release(cleanup_ctx);

	assert(f->host != NULL);
	assert(f->module != NULL);

	args = fargs_cmdline(&sess, f, &skip);

	/* Initiate with the rsyncd version and module request. */

	(void)snprintf(buf, sizeof(buf), "@RSYNCD: %d", sess.lver);
	if (!io_write_line(&sess, sd, buf)) {
		ERRX1("io_write_line");
		goto out;
	}

	LOG2("requesting module: %s, %s", f->module, f->host);

	if (!io_write_line(&sess, sd, f->module)) {
		ERRX1("io_write_line");
		goto out;
	}

	/*
	 * Now we read the server's response, byte-by-byte, one newline
	 * terminated at a time, limited to BUFSIZ line length.
	 * For this protocol version, this consists of either @RSYNCD
	 * followed by some text (just "ok" and the remote version) or
	 * the message of the day.
	 */

	for (;;) {
		i = sizeof(buf);

		if (!io_read_line(&sess, sd, buf, &i)) {
			ERRX1("io_read_line");
			goto out;
		}

		if (i == sizeof(buf)) {
			ERRX("line buffer overrun");
			goto out;
		} else if (i == 0)
			continue;

		/*
		 * The rsyncd protocol isn't very clear as to whether we
		 * get a CRLF or not: I don't actually see this being
		 * transmitted over the wire.
		 */

		assert(i > 0);
		buf[i] = '\0';
		if (buf[i - 1] == '\r')
			buf[i - 1] = '\0';

		if ((c = protocol_line(&sess, f->host, f->user, sd, buf)) < 0) {
			ERRX1("protocol_line");
			goto out;
		} else if (c > 0)
			break;
	}

	/*
	 * Now we've exchanged all of our protocol information.
	 * We want to send our command-line arguments over the wire,
	 * each with a newline termination.
	 * Use the same arguments when invoking the server, but leave
	 * off the binary name(s).
	 * Emit a standalone newline afterward.
	 */

	for (i = skip ; args[i] != NULL; i++)
		if (!io_write_line(&sess, sd, args[i])) {
			ERRX1("io_write_line");
			goto out;
		}
	if (!io_write_byte(&sess, sd, '\n')) {
		ERRX1("io_write_line");
		goto out;
	}

	/*
	 * All data after this point is going to be multiplexed, so turn
	 * on the multiplexer for our reads and writes.
	 */

	/* Protocol exchange: get the random seed. */

	if (!io_read_int(&sess, sd, &sess.seed)) {
		ERRX1("io_read_int");
		goto out;
	}

	/* Now we've completed the handshake. */

	if (sess.rver < RSYNC_PROTOCOL_MIN) {
		ERRX("remote protocol %d is older than our minimum supported "
		    "%d: exiting", sess.rver, RSYNC_PROTOCOL_MIN);
		rc = 2;
		goto out;
	}

	if (sess.rver < sess.lver) {
		sess.protocol = sess.rver;
	}

	if (sess.opts->write_batch != NULL && (rc = batch_open(&sess)) != 0) {
		ERRX1("batch_open");
		rc = 2;
		goto out;
	}

	sess.mplex_reads = 1;
	LOG2("read multiplexing enabled");

	LOG2("socket detected client version %d, server version %d, "
	    "negotiated protocol version %d, seed %d",
	    sess.lver, sess.rver, sess.protocol, sess.seed);

	assert(sess.opts->whole_file != -1);
	LOG2("Delta transmission %s for this transfer",
	    sess.opts->whole_file ? "disabled" : "enabled");

	if (f->mode == FARGS_RECEIVER) {
		LOG2("client starting receiver: %s", f->host);
		if (!rsync_receiver(&sess, cleanup_ctx, sd, sd, f->sink)) {
			ERRX1("rsync_receiver");
			goto out;
		}
	} else {
		LOG2("client starting sender: %s", f->host);
		if (!rsync_sender(&sess, sd, sd, f->sourcesz, f->sources)) {
			ERRX1("rsync_sender");
			goto out;
		}

	}

	/*
	 * See the commentary in the client at the same point; the short version
	 * is that we don't want to miss any log messages.
	 */
	rc = 0;
	if (!io_read_close(&sess, sd)) {
		WARNX("data remains in read pipe");
		rc = ERR_IPC;
	} else if (sess.err_del_limit) {
		assert(sess.total_deleted >= sess.opts->max_delete);
		rc = ERR_DEL_LIMIT;
	} else if (sess.total_errors > 0) {
		rc = ERR_PARTIAL;
	}
out:
	batch_close(&sess, f, rc);
	free(args);
	return rc;
}

int
rsync_is_socket(int fd)
{
	struct stat sb;

	if (fstat(fd, &sb) == -1)
		return 0;
	return !!S_ISSOCK(sb.st_mode);
}
