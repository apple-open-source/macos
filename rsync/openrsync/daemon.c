/*
 * Copyright (c) 2024 Klara, Inc.
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

#include <sys/types.h>
#ifdef __APPLE__
#include <usbuf.h>
#else
#include <sys/sbuf.h>
#endif
#include <sys/time.h>

#include <assert.h>
#if HAVE_ERR
# include <err.h>
#endif
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <glob.h>
#include <grp.h>
#include <limits.h>
#include <paths.h>
#if HAVE_B64_NTOP
#include <netinet/in.h>
#include <resolv.h>
#endif
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if HAVE_SCAN_SCALED
# include <util.h>
#endif

#include "extern.h"
#include "daemon.h"

#ifndef _PATH_ETC
#define	_PATH_ETC	"/etc"
#endif

#define	_PATH_RSYNCD_CONF	_PATH_ETC "/rsyncd.conf"

extern struct cleanup_ctx	*cleanup_ctx;

static const char proto_prefix[] = "@RSYNCD: ";

enum {
	OP_DAEMON = CHAR_MAX + 1,
	OP_NO_DETACH,
	OP_ADDRESS,
	OP_BWLIMIT,
	OP_CONFIG,
	OP_PORT,
	OP_LOG_FILE,
	OP_LOG_FILE_FORMAT,
	OP_SOCKOPTS,
};

static const struct option	daemon_lopts[] = {
	{ "address",	required_argument,	NULL,		OP_ADDRESS },
	{ "bwlimit",	required_argument,	NULL,		OP_BWLIMIT },
	{ "config",	required_argument,	NULL,		OP_CONFIG },
	{ "daemon",	no_argument,	NULL,			OP_DAEMON },
	{ "no-detach",	no_argument,	NULL,			OP_NO_DETACH },
	{ "ipv4",	no_argument,	NULL,			'4' },
	{ "ipv6",	no_argument,	NULL,			'6' },
	{ "help",	no_argument,	NULL,			'h' },
	{ "log-file",	required_argument,	NULL,		OP_LOG_FILE },
#if 0
	{ "log-file-format",	required_argument,	NULL,	OP_LOG_FILE_FORMAT },
#endif
	{ "port",	required_argument,	NULL,		OP_PORT },
	{ "sockopts",	required_argument,	NULL,		OP_SOCKOPTS },
	{ "verbose",	no_argument,		NULL,		'v' },
	{ NULL,		0,		NULL,			0 },
};

static void
daemon_usage(int exitcode)
{
	fprintf(exitcode == 0 ? stdout : stderr, "usage: %s --daemon"
	    " [-46hv] [--address=bindaddr] [--bwlimit=limit] [--no-detach]\n"
	    "\t[--log-file=logfile] [--port=portnumber] [--sockopts=sockopts]\n",
	    getprogname());
	exit(exitcode);
}

static int
daemon_list_module(struct daemon_cfg *dcfg, const char *module, void *cookie)
{
	struct sess *sess;
	struct daemon_role *role;
	char *buf;
	const char *comment;
	int fd, list, rc;

	sess = cookie;
	role = (void *)sess->role;
	fd = role->client;

	if (cfg_param_bool(dcfg, module, "list", &list) != 0) {
		ERRX("%s: 'list' is not valid", module);
		return 0;
	}

	if (!list)
		return 1;

	if (!cfg_has_param(dcfg, module, "comment")) {
		if (!io_write_line(sess, fd, module)) {
			ERR("io_write_line");
			return 0;
		}

		return 1;
	}

	rc = cfg_param_str(dcfg, module, "comment", &comment);
	assert(rc == 0);

	if (asprintf(&buf, "%-15s%s", module, comment) == -1) {
		ERR("asprintf");
		return 0;
	}


	if (!io_write_line(sess, fd, buf)) {
		free(buf);
		ERR("io_write_line");
		return 0;
	}

	free(buf);
	return 1;
}

static int
daemon_list(struct sess *sess)
{
	struct daemon_role *role;

	role = (void *)sess->role;
	return cfg_foreach_module(role->dcfg, daemon_list_module, sess);
}

static int
daemon_read_hello(struct sess *sess, int fd, char **module)
{
	char buf[BUFSIZ];
	size_t linesz;

	/*
	 * Client is waiting for us to respond, so just grab everything we can.
	 * It should have sent us exactly two lines:
	 *   @RSYNCD: <version>\n
	 *   <module>\n
	 */
	sess->rver = -1;
	*module = NULL;

	for (size_t linecnt = 0; linecnt < 2; linecnt++) {
		linesz = sizeof(buf);
		if (!io_read_line(sess, fd, buf, &linesz) ||
		    (linesz == 0 && linecnt == 0)) {
			/*
			 * An empty line is an error, unless we already have our
			 * version information.  If we only receive version
			 * information, it's implied that the client just wants
			 * a listing.
			 */
			daemon_client_error(sess,
			    "protocol violation: expected version and module information");
			return -1;
		} else if (linesz == sizeof(buf)) {
			daemon_client_error(sess, "line buffer overrun");
			errno = EINVAL;
			return -1;
		}

		if (linecnt == 0) {
			const char *line;
			int major, minor;

			line = buf;
			if (strncmp(line, proto_prefix,
			    sizeof(proto_prefix) - 1) != 0) {
				daemon_client_error(sess,
				    "protocol violation: expected version line, got '%s'",
				    line);
				errno = EINVAL;
				return -1;
			}

			line += sizeof(proto_prefix) - 1;

			/*
			 * XXX Modern rsync sends:
			 * @RSYNCD: <version>.<subprotocol> <digest1> <digestN>
			 * @RSYNCD: 31.0 sha512 sha256 sha1 md5 md4
			 */
			if (sscanf(line, "%d.%d", &major, &minor) == 2) {
				sess->rver = major;
			} else if (sscanf(line, "%d", &major) == 1) {
				sess->rver = major;
			} else {
				daemon_client_error(sess,
				   "protocol violation: malformed version line, got '%s'",
				    line);
				errno = EINVAL;
				return -1;
			}

			/*
			 * Discard the rest of the line, respond with our
			 * protocol version.
			 */
			(void)snprintf(buf, sizeof(buf), "@RSYNCD: %d",
			    sess->lver);

			if (!io_write_line(sess, fd, buf)) {
				/* XXX OS ERR */
				return -1;
			}
		} else {
			if (linesz == 0) {
				*module = strdup("#list");
			} else {
				*module = strdup(buf);
			}

			if (*module == NULL) {
				ERR("strdup");
				return -1;
			}
		}
	}

	return 0;
}

#define	OPT_ALLOC_SLOTS	5

static bool
daemon_add_option(struct sess *sess, size_t *oargc, char ***oargv,
    size_t *oargvsz, const char *buf)
{
	char **argv, **pargv;
	size_t argc, argvsz, nextsz;

	argv = *oargv;
	argc = *oargc;
	argvsz = *oargvsz;

	if (argc == INT_MAX) {
		/* XXX Do we want to limit byte size as well? */
		daemon_client_error(sess,
		    "protection error: too many arguments sent");
		return false;
	}

	/*
	 * If argc == argvsz, we need more to be able to null terminate
	 * the array properly.
	 */
	if (argc == argvsz) {
		pargv = argv;
		nextsz = argvsz + OPT_ALLOC_SLOTS;
		argv = recallocarray(pargv, argvsz, nextsz,
		    sizeof(*argv));
		if (argv == NULL) {
			daemon_client_error(sess, "daemon out of memory");
			return false;
		}

		*oargvsz = nextsz;
		*oargv = argv;
	}

	argv[argc] = strdup(buf);
	if (argv[argc] == NULL) {
		daemon_client_error(sess, "daemon out of memory");
		return false;
	}

	(*oargc)++;
	return true;
}

static bool
daemon_add_glob(struct sess *sess, const char *module, size_t *oargc,
    char ***oargv, size_t *oargvsz, char *buf, struct sbuf *argsb)
{
	glob_t glb = { 0 };
	int error;
	bool ret;

	/* Strip any <module>/ off the beginning. */
	daemon_normalize_path(module, 0, buf);

	if ((error = glob(buf, 0, NULL, &glb)) != 0 &&
	    error != GLOB_NOMATCH) {
		fprintf(stderr, "returned %d\n", error);
		daemon_client_error(sess, "glob '%s' failed", buf);
		return false;
	}

	if (glb.gl_pathc == 0) {
		/*
		 * If we didn't match anything, then we assume it was supposed
		 * to be a literal name.
		 */
		if (argsb != NULL) {
			sbuf_cat(argsb, buf);
			sbuf_putc(argsb, '\0');
		}

		ret = daemon_add_option(sess, oargc, oargv, oargvsz, buf);
	} else {
		/*
		 * Otherwise, we expand it in-place instead.
		 */
		ret = true;
		for (size_t i = 0; i < glb.gl_pathc; i++) {
			if (argsb != NULL) {
				sbuf_cat(argsb, glb.gl_pathv[i]);
				sbuf_putc(argsb, '\0');
			}

			if (!daemon_add_option(sess, oargc, oargv, oargvsz,
			    glb.gl_pathv[i])) {
				ret = false;
				break;
			}
		}
	}

	globfree(&glb);
	return ret;
}

static int
daemon_read_options(struct sess *sess, const char *module, int fd,
    int *oargc, char ***oargv)
{
	char buf[BUFSIZ];
	struct daemon_role *role;
	struct sbuf *argsb, *reqsb;
	char **argv;
	size_t argc, argvsz, linesz;
	bool fargs;

	role = (void *)sess->role;
	argv = NULL;
	argc = 0;
	fargs = false;
	reqsb = NULL;

	argsb = reqsb = NULL;
	if (role->prexfer_pipe != -1) {
		argsb = sbuf_new_auto();
		if (argsb == NULL) {
			daemon_client_error(sess, "daemon out of memory");
			return -1;
		}
		reqsb = sbuf_new_auto();
		if (reqsb == NULL) {
			daemon_client_error(sess, "daemon out of memory");
			return -1;
		}
	}

	/*
	 * At a minimum we'll have these three lines:
	 *
	 * --server
	 * .
	 * <module>[/<path>]
	 *
	 * So we'll start with an array sized for 4 arguments to allow a little
	 * wiggle room, and we'll allocate in groups of 5 as we need more.
	 */
	argvsz = OPT_ALLOC_SLOTS;
	argv = recallocarray(argv, 0, OPT_ALLOC_SLOTS, sizeof(*argv));
	if (argv == NULL) {
		daemon_client_error(sess, "daemon out of memory");
		sbuf_delete(argsb);
		sbuf_delete(reqsb);
		if (argsb != NULL)
			sbuf_delete(argsb);
		if (reqsb != NULL)
			sbuf_delete(reqsb);
		return -1;
	}

	/* Fake first arg, because we can't actually reset to the 0'th argv */
	argv[argc++] = NULL;

	for (;;) {
		linesz = sizeof(buf);
		if (!io_read_line(sess, fd, buf, &linesz)) {
			daemon_client_error(sess,
			    "protocol violation: expected option line");
			errno = EINVAL;
			if (argsb != NULL)
				sbuf_delete(argsb);
			if (reqsb != NULL)
				sbuf_delete(reqsb);
			return -1;
		} else if (linesz == sizeof(buf)) {
			daemon_client_error(sess, "line buffer overrun");
			errno = EINVAL;
			if (argsb != NULL)
				sbuf_delete(argsb);
			if (reqsb != NULL)
				sbuf_delete(reqsb);
			return -1;
		} else if (linesz == 0) {
			break;
		}

		/*
		 * XXX Glob.
		 */
		if (fargs) {
			/*
			 * These get added to the request as-is, but they're
			 * added to RSYNC_ARG# with the module/ stripped in
			 * daemon_add_glob().
			 */
			if (reqsb != NULL) {
				if (sbuf_len(reqsb) != 0)
					sbuf_putc(reqsb, ' ');
				sbuf_cat(reqsb, buf);
			}

			if (!daemon_add_glob(sess, module, &argc, &argv,
			    &argvsz, buf, argsb))
				goto fail;
		} else {
			if (argsb != NULL) {
				sbuf_cat(argsb, buf);
				sbuf_putc(argsb, '\0');
			}

			if (!daemon_add_option(sess, &argc, &argv, &argvsz,
			    buf))
				goto fail;
		}

		if (strcmp(buf, ".") == 0)
			fargs = true;
	}

	if (role->prexfer_pipe != -1) {
		const char *wargs, *wreq;
		size_t wargsz;

		wargs = wreq = NULL;
		wargsz = 0;

		if (sbuf_finish(argsb) == 0) {
			wargs = sbuf_data(argsb);
			wargsz = sbuf_len(argsb);
		}

		if (sbuf_finish(reqsb) == 0)
			wreq = sbuf_data(reqsb);

		daemon_finish_prexfer(sess, wreq, wargs, wargsz);

		sbuf_delete(argsb);
		sbuf_delete(reqsb);
	}

	*oargc = (int)argc;
	*oargv = argv;
	return 0;
fail:
	if (argsb != NULL)
		sbuf_delete(argsb);
	if (reqsb != NULL)
		sbuf_delete(reqsb);
	if (role->prexfer_pipe != -1)
		daemon_finish_prexfer(sess, NULL, NULL, 0);
	for (size_t i = 0; i < argc; i++)
		free(argv[i]);
	free(argv);
	return -1;
}

static int
daemon_reject(struct sess *sess, int shopt, const struct option *lopt)
{
	struct daemon_role *role;

	role = (void *)sess->role;

	if (lopt != NULL && strcmp(lopt->name, "daemon") == 0) {
		daemon_client_error(sess,
		    "protection error: --daemon sent as client option");
		return 0;
	}

	/*
	 * Short options are quick, we just search the short opt string and
	 * refuse it if it's there.  dameon_parse_refuse() adds the short option
	 * for each long option, so we don't need to fallback to searching the
	 * long options.
	 */
	if (role->refused.refused_shopts != NULL && shopt != 0) {
		if (strchr(role->refused.refused_shopts, shopt) != NULL) {
			daemon_client_error(sess, "option refused: -%c",
			    shopt);
			return 0;
		}

		return 1;
	}

	if (role->refused.refused_lopts != NULL && lopt != NULL) {
		const struct option * const *chkopt = role->refused.refused_lopts;

		for (size_t i = 0; i < role->refused.refused_loptsz; i++) {
			if (chkopt[i] == lopt) {
				daemon_client_error(sess,
				    "option refused: --%s", lopt->name);
				return 0;
			}
		}
	}

	return 1;
}

static int
daemon_write_motd(struct sess *sess, const char *motd_file, int outfd)
{
	FILE *fp;
	char *line;
	size_t linesz;
	ssize_t linelen;
	int retval;

	/* Errors largely ignored, maybe logged. */
	if (motd_file[0] == '\0')
		return 1;

	line = NULL;
	linesz = 0;
	fp = fopen(motd_file, "r");
	if (fp == NULL) {
		/* XXX Log */
		return 1;
	}

	retval = 1;
	while ((linelen = getline(&line, &linesz, fp)) > 0) {
		if (!io_write_buf(sess, outfd, line, linelen)) {
			/* XXX Log */
			retval = 0;
			break;
		}
	}

	fclose(fp);
	free(line);
	return retval;
}

#define	AUTH_CHALLENGE_LENGTH	64	/* Arbitrary */

#if !HAVE_ARC4RANDOM
static void
daemon_auth_challenge_random(struct sess *sess, uint8_t *buf, size_t bufsz)
{
	struct timeval tv;
	struct daemon_role *role;
	uint8_t *addrbuf, *pidbuf, *timebuf;
	pid_t p;

	role = (void *)sess->role;

	p = getpid();
	gettimeofday(&tv, NULL);

	addrbuf = (uint8_t *)role->client_sa;
	pidbuf = (uint8_t *)&p;
	timebuf = (uint8_t *)&tv;

	for (size_t i = 0; i < bufsz; i++) {
		uint8_t curbyte = buf[i];

		/*
		 * A combination of: stack garbage, some more numbers, a bit of
		 * address bits mixed in as well as the pid and time.  This does
		 * not need to be perfectly random, it mostly needs to avoid
		 * replay attacks.
		 */
		curbyte ^= random();
		curbyte ^= addrbuf[i % sizeof(role->client_sa)];
		curbyte ^= pidbuf[i % sizeof(p)];
		curbyte ^= timebuf[i % sizeof(tv)];

		buf[i] = curbyte;
	}
}
#endif

static void
daemon_auth_generate_challenge(struct sess *sess, char *challenge,
    size_t challengesz)
{
	char hashed_buf[AUTH_CHALLENGE_LENGTH + 4];
	uint8_t buf[128];

#if HAVE_ARC4RANDOM
	arc4random_buf(buf, sizeof(buf));
#else
	daemon_auth_challenge_random(sess, buf, sizeof(buf));
#endif

	/*
	 * hashed_buf is sized such that we should get AUTH_CHALLENGE_LENGTH
	 * worth of base64 characters, so it can (and probably will be)
	 * truncated but we can ignore that.
	 */
	(void)b64_ntop(buf, sizeof(buf), hashed_buf, sizeof(hashed_buf));
	memcpy(challenge, hashed_buf, MIN(AUTH_CHALLENGE_LENGTH, challengesz));
}

static int
user_matches(const char *cfguser, const char *clientuser)
{

	if (cfguser[0] == '@') {
		struct group *grp;

		/* System group */
		cfguser++;

		grp = getgrnam(cfguser);
		if (grp == NULL)
			return 0;

		for (char **mem = grp->gr_mem; *mem != NULL; mem++) {
			if (strcmp(*mem, clientuser) == 0)
				return 1;
		}

		return 0;
	}

	return strcmp(cfguser, clientuser) == 0;
}

static int
secret_matches(FILE *secfp, const char *username, const char *group,
    const char *challenge, const char *response)
{
	char checkbuf[RSYNCD_CHALLENGE_RESPONSESZ];
	char *line;
	size_t linesz;
	ssize_t linelen;
	char *password;
	int matched;

	line = NULL;
	linesz = 0;
	matched = 0;
	while ((linelen = getline(&line, &linesz, secfp)) > 0) {
		while (isspace(line[linelen - 1]))
			line[--linelen] = '\0';

		if (linelen == 0 || line[0] == '#')
			continue;

		password = strchr(line, ':');
		if (password == NULL)
			continue;

		*password = '\0';
		password++;

		if (strcmp(line, username) != 0 &&
		    (group == NULL || strcmp(line, group) != 0))
			continue;

		/* Try to match it; if hashing fails, we'll reject anyways. */
		if (!rsync_password_hash(password, challenge, checkbuf,
		    sizeof(checkbuf))) {
			ERRX1("rsync_password_hash");
			continue;
		}

		if (strcmp(checkbuf, response) == 0) {
			matched = 1;
			break;
		}

	}

	if (line != NULL)
		explicit_bzero(line, linesz);
	free(line);

	return matched;
}

static int
daemon_auth_user(struct sess *sess, const char *module, const char *cfgusers,
    FILE *secfp, const char *username, const char *challenge,
    const char *response, int *read_only)
{
	const char *delims;
	char *users, *user, *setting, *strip;
	int matched;
	bool is_grp;

	if (username[0] == '#') {
		daemon_client_error(sess, "%s: bad username", module);
		return 0;
	}

	users = strdup(cfgusers);
	if (users == NULL) {
		daemon_client_error(sess, "%s: out of memory", module);
		return 0;	/* Deny */
	}

	/*
	 * A comma first character forces us to split on commas so that an entry
	 * may contain spaces.
	 */
	if (users[0] == ',')
		delims = ",";
	else
		delims = ", \t";

	matched = 0;
	setting = NULL;
	while ((user = strsep(&users, delims)) != NULL) {
		/* Trim any leading whitespace. */
		while (isspace(*user))
			user++;

		if (*user == '\0')
			continue;

		/* Trim any trailing whitespace. */
		strip = &user[strlen(user) - 1];
		while (isspace(*strip)) {
			*strip = '\0';
			strip--;
			assert(strip > user);
		}

		setting = strchr(user, ':');
		if (setting != NULL) {
			*setting = '\0';
			setting++;
		}

		if (!user_matches(user, username))
			continue;

		/*
		 * We always stop at first match, but we can match either the
		 * user or group entry secret if that's the one we hit.
		 */
		is_grp = user[0] == '@';
		if (!secret_matches(secfp, username, is_grp ? user : NULL,
		    challenge, response))
			break;

		if (setting != NULL && strcmp(setting, "deny") == 0)
			break;

		*read_only = -1;
		matched = 1;

		if (setting == NULL)
			break;
		if (strcmp(setting, "ro") == 0)
			*read_only = 1;
		else if (strcmp(setting, "rw") == 0)
			*read_only = 0;

		break;
	}

	free(users);
	return matched;
}

static int
daemon_auth(struct sess *sess, const char *module, int *read_only)
{
	char response[RSYNCD_MAXAUTHSZ];
	char challenge[AUTH_CHALLENGE_LENGTH + 1];
	struct daemon_role *role;
	const char *secretsf, *users;
	char *hash, *username;
	FILE *secfp;
	size_t linesz;
	int rc, strict;

	role = (void *)sess->role;
	if (!cfg_has_param(role->dcfg, module, "auth users"))
		return 1;

	if (!cfg_has_param(role->dcfg, module, "secrets file")) {
		daemon_client_error(sess, "%s: missing secrets file", module);
		return 0;
	}

	rc = cfg_param_str(role->dcfg, module, "secrets file", &secretsf);
	assert(rc == 0);
	if (*secretsf == '\0') {
		daemon_client_error(sess, "%s: missing secrets file", module);
		return 0;
	}

	rc = cfg_param_str(role->dcfg, module, "auth users", &users);
	assert(rc == 0);

	if (cfg_param_bool(role->dcfg, module, "strict modes", &strict) != 0) {
		daemon_client_error(sess, "%s: 'strict modes' invalid", module);
		return 0;
	}

	secfp = fopen(secretsf, "r");
	if (secfp == NULL) {
		daemon_client_error(sess, "%s: could not open secrets file",
		    module);
		return 0;
	} else if (strict && !check_file_mode(secretsf, fileno(secfp))) {
		fclose(secfp);
		daemon_client_error(sess, "%s: bad permissions on secrets file",
		    module);
		return 0;
	}

	memset(challenge, 0, sizeof(challenge));
	daemon_auth_generate_challenge(sess, challenge, sizeof(challenge));
	challenge[AUTH_CHALLENGE_LENGTH] = '\n';

	if (!io_write_buf(sess, role->client, "@RSYNCD: AUTHREQD ",
	    sizeof("@RSYNCD: AUTHREQD ") - 1)) {
		fclose(secfp);
		ERR("io_write_buf");
		return 0;
	}

	if (!io_write_buf(sess, role->client, challenge, sizeof(challenge))) {
		fclose(secfp);
		ERR("io_write_line");
		return 0;
	}

	challenge[AUTH_CHALLENGE_LENGTH] = '\0';

	linesz = sizeof(response);
	if (!io_read_line(sess, role->client, response, &linesz)) {
		fclose(secfp);
		daemon_client_error(sess, "%s: expected auth response",
		    module);
		return 0;
	} else if (linesz == sizeof(response)) {
		fclose(secfp);
		daemon_client_error(sess, "%s: line buffer overflow on auth",
		    module);
		return 0;
	}

	username = response;
	hash = strchr(response, ' ');
	if (hash == NULL) {
		fclose(secfp);
		daemon_client_error(sess, "%s: malformed auth response",
		    module);
		return 0;
	}

	*hash = '\0';
	hash++;

	rc = daemon_auth_user(sess, module, users, secfp, username, challenge,
	    hash, read_only);
	fclose(secfp);

	if (rc) {
		role->auth_user = strdup(username);
		if (role->auth_user == NULL) {
			daemon_client_error(sess, "%s: out of memory", module);
			return 0;
		}
	}

	return rc;
}

static int
daemon_extract_addr(struct sess *sess, struct sockaddr_storage *saddr,
    size_t slen)
{
	sa_family_t family = saddr->ss_family;
	struct daemon_role *role;
	void *addr;

	role = (void *)sess->role;
	assert(family == AF_INET || family == AF_INET6);
	role->client_sa = (struct sockaddr *)saddr;

	if (family == AF_INET) {
		struct sockaddr_in *sin = (void *)saddr;

		addr = &sin->sin_addr;
	} else {
		struct sockaddr_in6 *sin6 = (void *)saddr;

		addr = &sin6->sin6_addr;
	}

	if (inet_ntop(family, addr, &role->client_addr[0],
	    sizeof(role->client_addr)) == NULL) {
		ERR("inet_ntop");
		return 0;
	}

	return 1;
}


static int
rsync_daemon_handler(struct sess *sess, int fd, struct sockaddr_storage *saddr,
    socklen_t slen)
{
	struct daemon_role *role;
	struct opts *client_opts;
	char **argv, *module, *motd_file;
	int argc, flags, rc, use_chroot, user_read_only;

	module = NULL;
	argc = 0;
	argv = NULL;
	user_read_only = -1;

	role = (void *)sess->role;
	role->prexfer_pid = 0;
	role->prexfer_pipe = -1;
	role->client = fd;
	assert(role->lockfd == -1);

	motd_file = role->motd_file;
	role->motd_file = NULL;

	if (role->pidfp != NULL)
		fclose(role->pidfp);
	cfg_free(role->dcfg);
	role->dcfg = NULL;

	/* XXX These should perhaps log an error, but they are not fatal. */
	(void)rsync_setsockopts(fd, "SO_KEEPALIVE");
	(void)rsync_setsockopts(fd, sess->opts->sockopts);

	if (!daemon_extract_addr(sess, saddr, slen))
		return ERR_IPC;

	sess->lver = sess->protocol = RSYNC_PROTOCOL;

	cleanup_init(cleanup_ctx);

	if ((flags = fcntl(fd, F_GETFL, 0)) == -1 ||
	    fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
		daemon_client_error(sess, "failed to set non-blocking");
		return ERR_IPC;
	}

	role->dcfg = cfg_parse(sess, role->cfg_file, 1);
	if (role->dcfg == NULL)
		return ERR_IPC;

	/* saddr == NULL only for inetd driven invocations. */
	if (daemon_read_hello(sess, fd, &module) < 0)
		goto fail;	/* Error already logged. */

	/* XXX PROTOCOL_MIN, etc. */
	if (sess->rver < RSYNC_PROTOCOL_MIN) {
		daemon_client_error(sess,
		    "could not negotiate a protocol; client requested %d (supported range: %d to %d)",
		    sess->rver, RSYNC_PROTOCOL_MIN, sess->lver);
		goto fail;
	} else if (sess->rver < sess->lver) {
		sess->protocol = sess->rver;
	}

	/* Grab the motd before we free it. */
	rc = daemon_write_motd(sess, motd_file, fd);
	free(motd_file);
	motd_file = NULL;

	/* Fatal error, already logged. */
	if (!rc)
		goto fail;

	if (module[0] == '#') {
		/*
		 * This is actually a command, but the only command we know of
		 * at the moment is #list.  If we grow more, we can perhaps make
		 * this table-based instead of inlined.
		 */
		if (strcmp(module + 1, "list") != 0) {
			daemon_client_error(sess, "%s is not a known command",
			    module);
			goto fail;
		}

		if (!daemon_list(sess))
			goto fail;

		if (!io_write_line(sess, fd, "@RSYNCD: EXIT")) {
			ERR("io_write_line");
			goto fail;
		}

		goto done;
	} else if (!cfg_is_valid_module(role->dcfg, module)) {
		daemon_client_error(sess, "%s is not a valid module", module);
		goto fail;
	}

	if (!daemon_fill_hostinfo(sess, module,
	    (const struct sockaddr *)saddr, slen))
		goto fail;

	if (!daemon_connection_allowed(sess, module))
		goto fail;

	if (daemon_connection_limited(sess, module))
		goto fail;

	if (!daemon_auth(sess, module, &user_read_only)) {
		daemon_client_error(sess, "%s: authentication failed", module);
		goto fail;
	}

	if (cfg_param_bool(role->dcfg, module, "use chroot",
	    &use_chroot) != 0) {
		/* Log it and pretend it's unset. */
		WARN("%s: 'use chroot' malformed", module);
	} else if (use_chroot && !cfg_has_param(role->dcfg, module,
	    "use chroot")) {
		/*
		 * If it's not set, note that in case it fails -- we will
		 * fallback.
		 */
		use_chroot = 2;
	}

	rc = cfg_param_str(role->dcfg, module, "path", &role->module_path);
	assert(rc == 0);

	if (!daemon_configure_filters(sess, module))
		goto fail;

	if (!daemon_setup_logfile(sess, module))
		goto fail;

	if (!daemon_do_execcmds(sess, module))
		goto fail;

	/*
	 * openrsync does not honor 'dont compress' because it may be actively
	 * harmful at the feature level we're at -- notably, per-file
	 * compression levels can't be set with earlier versions of --compress,
	 * so if the first file matches a "dont compress" pattern then we end up
	 * effectively disabling compression for the entire transfer.
	 */
	if (cfg_has_param(role->dcfg, module, "dont compress"))
		WARNX("%s: 'dont compress' is present but not honored", module);

	/*
	 * Resolve UIDs/GIDs before we enter our chroot, just in case they're
	 * not strictly numeric.
	 */
	if (!daemon_chuser_setup(sess, module))
		goto fail;

	/*
	 * We don't currently support the /./ chroot syntax of rsync 3.x.
	 */
	chdir(role->module_path);
	if (use_chroot && chroot(".") == -1) {
		if (errno != EPERM || use_chroot == 1) {
			/* XXX Fail it. */
			goto fail;
		}

		WARN("%s: attempt to chroot failed, falling back to 'no' since it is not explicitly set",
		    module);
		use_chroot = 0;
	}

	role->client_control = true;

	if (!daemon_chuser(sess, module))
		goto fail;

	if (!io_write_line(sess, fd, "@RSYNCD: OK")) {
		ERRX1("io_write_line");
		goto fail;
	}

	if (daemon_read_options(sess, module, fd, &argc, &argv) < 0)
		goto fail;	/* Error already logged. */

	/*
	 * Reset some state; our default poll_timeout is no longer valid, and
	 * we need to reset getopt_long(3).  poll_timeout after this point will
	 * not be overwritten by the client unless the client wants a little bit
	 * longer.
	 */
	poll_timeout = 0;
	if (cfg_param_int(role->dcfg, module, "timeout", &poll_timeout) != 0)
		WARN("%s: bad value for 'timeout'", module);

	if (!daemon_parse_refuse(sess, module))
		goto fail;

	optreset = 1;
	optind = 1;
	client_opts = rsync_getopt(argc, argv, &daemon_reject, sess);
	if (client_opts == NULL)
		goto fail;	/* Should have been logged. */

	argc -= optind;
	argv += optind;

	if (strcmp(argv[0], ".") != 0) {
		daemon_client_error(sess,
		    "protocol violation: expected hard stop before file list");
		goto fail;
	}

	argc--;
	argv++;

	if (!daemon_limit_verbosity(sess, module))
		goto fail;

	/* Generate a seed. */
	if (client_opts->checksum_seed == 0) {
#if HAVE_ARC4RANDOM
		sess->seed = arc4random();
#else
		sess->seed = random();
#endif
	} else {
		sess->seed = client_opts->checksum_seed;
	}

	/* Seed send-off completes the handshake. */
	if (!io_write_int(sess, fd, sess->seed)) {
		ERR("io_write_int");
		goto fail;
	}

	sess->mplex_writes = 1;
	/* XXX LOG2("write multiplexing enabled"); */

	if (!daemon_operation_allowed(sess, client_opts, module,
	    user_read_only))
		goto fail;	/* Error already logged. */

	/* Also from --files-from */
	if (client_opts->filesfrom_path != NULL)
		daemon_normalize_paths(module, 1, &client_opts->filesfrom_path);

	if (!daemon_apply_chmod(sess, module, client_opts))
		goto fail;

	if (!daemon_apply_chrootopts(sess, module, client_opts, use_chroot))
		goto fail;

	if (!daemon_apply_ignoreopts(sess, module, client_opts))
		goto fail;

	if (!daemon_install_symlink_filter(sess, module, use_chroot))
		goto fail;

	if (client_opts->whole_file < 0) {
		/* Simplify all future checking of this value */
		client_opts->whole_file = 0;
	}

	sess->opts = client_opts;

	cleanup_set_session(cleanup_ctx, sess);
	cleanup_release(cleanup_ctx);

	if (sess->opts->sender) {
		if (!rsync_sender(sess, fd, fd, argc, argv)) {
			ERR("rsync_sender");
			goto fail;
		}
	} else {
		if (!rsync_receiver(sess, cleanup_ctx, fd, fd, argv[0])) {
			ERR("rsync_receiver");
			goto fail;
		}
	}

done:
	if (role->lockfd != -1) {
		close(role->lockfd);
		role->lockfd = -1;
	}

	close(role->prexfer_pipe);
	free(role->auth_user);
	for (int i = 0; i < argc; i++)
		free(argv[i]);
	free(argv);

	return 0;

fail:
	free(module);
	cfg_free(role->dcfg);
	role->dcfg = NULL;
	if (role->lockfd != -1) {
		close(role->lockfd);
		role->lockfd = -1;
	}

	close(role->prexfer_pipe);
	free(role->auth_user);
	for (int i = 0; i < argc; i++)
		free(argv[i]);
	free(argv);

	return ERR_IPC;
}

static void
get_global_cfgstr(struct daemon_cfg *dcfg, const char *key, const char **out)
{
	int error;

	error = cfg_param_str(dcfg, "global", key, out);
	if (error != 0)
		*out = "";
}

static int
daemon_do_pidfile(struct sess *sess, struct daemon_cfg *dcfg)
{
	struct flock pidlock = {
		.l_start = 0,
		.l_len = 0,
		.l_type = F_WRLCK,
		.l_whence = SEEK_SET,
	};
	FILE *pidfp;
	const char *pidfile;
	struct daemon_role *role = (struct daemon_role *)sess->role;

	/* If it's empty, nothing to do here. */
	get_global_cfgstr(dcfg, "pid file", &pidfile);
	if (*pidfile == '\0')
		return 0;

	pidfp = fopen(pidfile, "w");
	if (pidfp == NULL) {
		ERR("%s: fopen", pidfile);
		return -1;
	}

	if (fcntl(fileno(pidfp), F_SETLK, &pidlock)) {
		fclose(pidfp);
		if (errno == EAGAIN)
			ERRX("%s: failed to obtain lock (is another rsyncd running?)", pidfile);
		else
			ERR("%s: acquiring lock", pidfile);
		return -1;
	}

	fprintf(pidfp, "%d\n", getpid());
	fflush(pidfp);

	role->pid_file = pidfile;
	role->pidfp = pidfp;
	return 0;
}

int
rsync_daemon(int argc, char *argv[], struct opts *daemon_opts)
{
	struct sess sess;
	struct daemon_role role;
	long long tmpint;
	const char *cfg_motd, *logfile;
	int c, opt_daemon = 0, detach = 1, rc;
	bool socket_initiator;

	/* Start with a fresh session / opts */
	memset(&role, 0, sizeof(role));
	memset(daemon_opts, 0, sizeof(*daemon_opts));
	memset(&sess, 0, sizeof(sess));
	sess.opts = daemon_opts;
	sess.role = (void *)&role;

	role.cfg_file = "/etc/rsyncd.conf";
	role.client = -1;
	role.lockfd = -1;
	/* Log to syslog by default. */
	logfile = NULL;

	/*
	 * optind starting at 1, because we're parsing the original program args
	 * and should skip argv[0].
	 */
	optreset = 1;
	optind = 1;
	while ((c = getopt_long(argc, argv, "46hv", daemon_lopts,
	    NULL)) != -1) {
		switch (c) {
		case OP_ADDRESS:
			daemon_opts->address = optarg;
			break;
		case OP_BWLIMIT:
			if (scan_scaled_def(optarg, &tmpint, 'k') == -1)
				err(1, "bad bwlimit");
			daemon_opts->bwlimit = tmpint;
			break;
		case OP_CONFIG:
			role.cfg_file = optarg;
			break;
		case OP_DAEMON:
			if (++opt_daemon > 1) {
				errx(ERR_SYNTAX,
				    "--daemon may not be specified multiple times");
			}
			break;
		case OP_NO_DETACH:
			detach = 0;
			break;
		case OP_LOG_FILE:
			logfile = optarg;
			break;
		case OP_PORT:
			daemon_opts->port = optarg;
			break;
		case OP_SOCKOPTS:
			daemon_opts->sockopts = optarg;
			break;
		case '4':
			daemon_opts->ipf = 4;
			break;
		case '6':
			daemon_opts->ipf = 6;
			break;
		case 'h':
			daemon_usage(0);
			break;
		case 'v':
			verbose++;
			break;
		default:
			daemon_usage(ERR_SYNTAX);
		}
	}

	argc -= optind;
	argv += optind;

	if (!daemon_open_logfile(logfile, true))
		return ERR_IPC;

	/*
	 * The reference rsync doesn't seem to complain about extra non-option
	 * arguments, though they aren't documented to do anything.
	 */

	poll_timeout = -1;

	/*
	 * We'll act on this after we pick up the initial config.
	 */
	socket_initiator = rsync_is_socket(STDIN_FILENO);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
	if (!socket_initiator && detach && daemon(0, 0) == -1)
		err(ERR_IPC, "daemon");
#pragma clang diagnostic pop

	role.dcfg = cfg_parse(&sess, role.cfg_file, 0);
	if (role.dcfg == NULL)
		return ERR_IPC;

	if (!socket_initiator && daemon_do_pidfile(&sess, role.dcfg) != 0)
		return ERR_IPC;

	if (daemon_opts->address == NULL) {
		if (cfg_param_str(role.dcfg, "global", "address",
		    &daemon_opts->address) == -1) {
			assert(errno != ENOENT);
		} else {
			if (*daemon_opts->address == '\0')
				daemon_opts->address = NULL;
		}
	}

	/*
	 * "rsync" is set as our default value, so if it's not found
	 * we'll get that.  We'll only fetch it if it wasn't specified via
	 * arguments.
	 */
	if (daemon_opts->port == NULL)
		get_global_cfgstr(role.dcfg, "port", &daemon_opts->port);

	/* Grab the motd filename, too. */
	get_global_cfgstr(role.dcfg, "motd file", &cfg_motd);
	role.motd_file = strdup(cfg_motd);
	if (role.motd_file == NULL)
		err(ERR_IPC, "strdup");

	if (daemon_opts->sockopts == NULL)
		get_global_cfgstr(role.dcfg, "socket options",
		    &daemon_opts->sockopts);

	if (socket_initiator) {
		struct sockaddr_storage saddr;
		socklen_t slen;

		slen = sizeof(saddr);
		if (getpeername(STDIN_FILENO, (struct sockaddr *)&saddr,
		    &slen) == -1)
			err(ERR_IPC, "getpeername");

		return rsync_daemon_handler(&sess, STDIN_FILENO, &saddr, slen);
	}

	LOG0("openrsync listening on port '%s'", daemon_opts->port);

	rc = rsync_listen(&sess, &rsync_daemon_handler);
	if (role.pidfp != NULL) {
		/*
		 * We still have the lock, so we can safely unlink and close it.
		 * Failure doesn't change our exit disposition; it's not a
		 * critical issue to have an unlocked stale pidfile laying
		 * around.
		 */
		(void)unlink(role.pid_file);
		fclose(role.pidfp);
	}

	return rc;
}
