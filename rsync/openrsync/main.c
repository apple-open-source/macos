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

#include <assert.h>
#include <ctype.h>
#if HAVE_ERR
# include <err.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <locale.h>
#if HAVE_SCAN_SCALED
# include <util.h>
#endif

#include "extern.h"
#include "zlib/zlib.h"

#ifdef __APPLE__
#include <os/variant_private.h>

#define	RSYNC_SUBSYSTEM	"com.apple.rsync"
#endif

extern struct cleanup_ctx *cleanup_ctx;

int verbose;
#ifdef __APPLE__
int syslog_trace;
os_log_t syslog_trace_obj;
#endif
int poll_contimeout;
int poll_timeout;

/*
 * A remote host is has a colon before the first path separator.
 * This works for rsh remote hosts (host:/foo/bar), implicit rsync
 * remote hosts (host::/foo/bar), and explicit (rsync://host/foo).
 * Return zero if local, non-zero if remote.
 */
static int
fargs_is_remote(const char *v)
{
	size_t	 pos;

	pos = strcspn(v, ":/");
	return v[pos] == ':';
}

/*
 * Test whether a remote host is specifically an rsync daemon.
 * Return zero if not, non-zero if so.
 */
static int
fargs_is_daemon(const char *v)
{
	size_t	 pos;

	if (strncasecmp(v, "rsync://", 8) == 0)
		return 1;

	pos = strcspn(v, ":/");
	return v[pos] == ':' && v[pos + 1] == ':';
}

/*
 * Splits a string of the form host:/path/name, :/path/name, or
 * rsync://host/[module/]path forms.
 * The components will be newly allocated strings.
 * Returns 0 on error, 1 on success.
 */
static int
split_hostspec(const char *const input, char **host, char **path)
{
	char *cp;

	if (*input == ':') {
		*host = strdup("");
		if (*host == NULL) {
			ERR("malloc hostspec");
			return 0;
		}

		*path = strdup(input + 1);
		if (*path == NULL) {
			ERR("malloc path");
			return 0;
		}

		return 1;
	} else if (fargs_is_daemon(input)) {
		const char *hostpart;
		size_t hostlen;

		/*
		 * The reference implementation doesn't seem to consider the
		 * port as part of the host name, which seems like it could be
		 * an oversight but we'll aim to be compatible.
		 */
		hostpart = &input[sizeof("rsync://") - 1];
		hostlen = strcspn(hostpart, ":/");

		*host = strndup(hostpart, hostlen);
		if (*host == NULL) {
			ERR("malloc hostspec");
			return 0;
		}

		if (hostpart[hostlen] != '/')
			hostlen = strcspn(hostpart, "/");
		if (hostpart[hostlen] == '\0') {
			/* Missing path... */
			ERR("Missing path in --files-from: %s", input);
			return 0;
		}

		*path = strdup(&hostpart[hostlen + 1]);
		if (*path == NULL) {
			ERR("malloc path");
			free(*host);
			*host = NULL;
			return 0;
		}

		return 1;
	}

	/* Simple host:path */
	cp = strchr(input, ':');
	if (cp == NULL)
		return 0;

	*host = strndup(input, cp - input);
	if (*host == NULL) {
		ERR("malloc hostspec");
		return 0;
	}

	*path = strdup(cp + 1);
	if (*path == NULL) {
		ERR("malloc hostspec path");
		free(*host);
		*host = NULL;
		return 0;
	}
	return 1;
}

/*
 * Strips the hostnames from the remote host.
 *   rsync://host/module/path -> module/path
 *   host::module/path -> module/path
 *   host:path -> path
 * Also make sure that the remote hosts are the same.
 */
static void
fargs_normalize_spec(const struct fargs *f, char *spec, size_t hostlen)
{
	char *cp, *ccp;
	size_t j;

	cp = spec;
	j = strlen(cp);
	if (f->remote && strncasecmp(cp, "rsync://", 8) == 0) {
		/* rsync://[user@]host[:port]/path */
		char *host_part, *module_part;

		/* cp is the host part */
		host_part = cp + 8;
		if ((ccp = strchr(host_part, '@')) != NULL)
			host_part = ccp + 1;

		/* skip :port */
		if ((ccp = strchr(host_part, ':')) != NULL) {
			*ccp = '\0';
			ccp++;
		} else {
			ccp = host_part;
		}

		/*
		 * ccp is the part just after our hostname, which may include a
		 * port number.
		 */
		module_part = strchr(ccp + 1, '/');
		if (module_part != NULL)
			module_part++;
		else
			module_part = &ccp[strlen(ccp) - 1];

		if (strncmp(host_part, f->host, hostlen) ||
		    (host_part[hostlen] != '/' && host_part[hostlen] != '\0'))
			errx(ERR_SYNTAX, "different remote host: %s", spec);

		memmove(spec, module_part, strlen(module_part) + 1);
	} else if (f->remote && strncmp(cp, "::", 2) == 0) {
		/* ::path */
		memmove(spec, spec + 2, j - 1);
	} else if (f->remote) {
		/* host::path */
		if (strncmp(cp, f->host, hostlen) ||
		    (cp[hostlen] != ':' && cp[hostlen] != '\0'))
			errx(ERR_SYNTAX, "different remote host: %s", spec);
		memmove(spec, spec + hostlen + 2, j - hostlen - 1);
	} else if (cp[0] == ':') {
		/* :path */
		memmove(spec, spec + 1, j);
	} else {
		/* host:path */
		if (strncmp(cp, f->host, hostlen) ||
		    (cp[hostlen] != ':' && cp[hostlen] != '\0'))
			errx(ERR_SYNTAX, "different remote host: %s", spec);
		memmove(spec, spec + hostlen + 1, j - hostlen);
	}
}

/*
 * Take the command-line filenames (e.g., rsync foo/ bar/ baz/) and
 * determine our operating mode.
 * For example, if the first argument is a remote file, this means that
 * we're going to transfer from the remote to the local.
 * We also make sure that the arguments are consistent, that is, if
 * we're going to transfer from the local to the remote, that no
 * filenames for the local transfer indicate remote hosts.
 * Always returns the parsed and sanitised options.
 */
static struct fargs *
fargs_parse(size_t argc, char *argv[], struct opts *opts)
{
	struct fargs	*f = NULL;
	char		*cp;
	size_t		 i, j, hostlen = 0;

	/* Allocations. */

	if ((f = calloc(1, sizeof(struct fargs))) == NULL)
		err(ERR_NOMEM, NULL);

	f->sourcesz = argc - 1;
	if (f->sourcesz > 0) {
		if ((f->sources = calloc(f->sourcesz, sizeof(char *))) == NULL)
			err(ERR_NOMEM, NULL);

		for (i = 0; i < argc - 1; i++)
			if ((f->sources[i] = strdup(argv[i])) == NULL)
				err(ERR_NOMEM, NULL);
	} else if (opts->read_batch == NULL) {
		errx(ERR_SYNTAX,
		    "One argument without --read-batch not yet supported");
	}

	if ((f->sink = strdup(argv[argc - 1])) == NULL)
		err(ERR_NOMEM, NULL);

	if (opts->read_batch != NULL) {
		/* --read-batch can only take a local sink. */
		if (fargs_is_remote(f->sink))
			errx(ERR_SYNTAX,
			    "rsync --read-batch destination must be local");
		return f;
	}

	/*
	 * Test files for its locality.
	 * If the last is a remote host, then we're sending from the
	 * local to the remote host ("sender" mode).
	 * If the first, remote to local ("receiver" mode).
	 * If neither, a local transfer in sender style.
	 */

	f->mode = FARGS_SENDER;

	if (fargs_is_remote(f->sink)) {
		f->mode = FARGS_SENDER;
		if ((f->host = strdup(f->sink)) == NULL)
			err(ERR_NOMEM, NULL);
	}

	if (fargs_is_remote(f->sources[0])) {
		if (f->host != NULL)
			errx(ERR_SYNTAX, "both source and destination "
			    "cannot be remote files");
		f->mode = FARGS_RECEIVER;
		if ((f->host = strdup(f->sources[0])) == NULL)
			err(ERR_NOMEM, NULL);
	}

	if (f->host != NULL) {
		if (strncasecmp(f->host, "rsync://", 8) == 0) {
			/* rsync://[user@]host[:port]/module[/path] */
			f->remote = 1;
			hostlen = strlen(f->host) - 8;

			/* [user@]host --> extract host */
			if ((cp = strchr(f->host + 8, '@')) != NULL) {
				f->user = strndup(f->host + 8,
				    cp - (f->host + 8));
				if (f->user == NULL)
					err(ERR_NOMEM, NULL);

				cp++;
				hostlen = strlen(cp);
			} else {
				cp = f->host + 8;
			}

			memmove(f->host, cp, hostlen + 1 /* NUL */);

			if ((cp = strchr(f->host, '/')) == NULL)
				errx(ERR_SYNTAX,
				    "rsync protocol requires a module name");
			*cp++ = '\0';
			f->module = cp;
			if ((cp = strchr(f->module, '/')) != NULL)
				*cp = '\0';
			if ((cp = strchr(f->host, ':')) != NULL) {
				/* host:port --> extract port */
				*cp++ = '\0';
				opts->port = cp;
			}
		} else {
			/* host:[/path] */
			cp = strchr(f->host, ':');
			assert(cp != NULL);
			*cp++ = '\0';
			if (*cp == ':') {
				/* host::module[/path] */
				f->remote = 1;
				f->module = ++cp;
				cp = strchr(f->module, '/');
				if (cp != NULL)
					*cp = '\0';
			}
		}
		if ((hostlen = strlen(f->host)) == 0)
			errx(ERR_SYNTAX, "empty remote host");
		if (f->remote && strlen(f->module) == 0)
			errx(ERR_SYNTAX, "empty remote module");
	}

	/* Make sure we have the same "hostspec" for all files. */

	if (!f->remote) {
		if (f->mode == FARGS_SENDER)
			for (i = 0; i < f->sourcesz; i++) {
				if (!fargs_is_remote(f->sources[i]))
					continue;
				errx(ERR_SYNTAX,
				    "remote file in list of local sources: %s",
				    f->sources[i]);
			}
		if (f->mode == FARGS_RECEIVER)
			for (i = 0; i < f->sourcesz; i++) {
				if (fargs_is_remote(f->sources[i]) &&
				    !fargs_is_daemon(f->sources[i]))
					continue;
				if (fargs_is_daemon(f->sources[i]))
					errx(ERR_SYNTAX,
					    "remote daemon in list of remote "
					    "sources: %s", f->sources[i]);
				errx(ERR_SYNTAX, "local file in list of "
				    "remote sources: %s", f->sources[i]);
			}
	} else {
		if (f->mode == FARGS_SENDER)
			for (i = 0; i < f->sourcesz; i++) {
				if (!fargs_is_remote(f->sources[i]))
					continue;
				errx(ERR_SYNTAX,
				    "remote file in list of local sources: %s",
				    f->sources[i]);
			}
		if (f->mode == FARGS_RECEIVER)
			for (i = 0; i < f->sourcesz; i++) {
				if (fargs_is_daemon(f->sources[i]))
					continue;
				errx(ERR_SYNTAX, "non-remote daemon file "
					"in list of remote daemon sources: "
					"%s", f->sources[i]);
			}
	}

	/*
	 * If we're not remote and a sender, strip our hostname.
	 * Then exit if we're a sender or a local connection.
	 */

	if (!f->remote) {
		if (f->host == NULL)
			return f;
		if (f->mode == FARGS_SENDER) {
			assert(f->host != NULL);
			assert(hostlen > 0);
			j = strlen(f->sink);
			memmove(f->sink, f->sink + hostlen + 1, j - hostlen);
			return f;
		} else if (f->mode != FARGS_RECEIVER)
			return f;
	}

	assert(f->host != NULL);
	assert(hostlen > 0);

	if (f->mode == FARGS_RECEIVER) {
		for (i = 0; i < f->sourcesz; i++)
			fargs_normalize_spec(f, f->sources[i], hostlen);
	} else {
		/*
		 * ssh and local transfers bailed out earlier and stripped the
		 * host: part as needed.  If we got here, we're connecting to
		 * a daemon as a sender.
		 */
		assert(f->remote);
		fargs_normalize_spec(f, f->sink, hostlen);
	}

	return f;
}

/*
 * Like scan_scaled, but with a default for the case where no characterr
 * is given.
 * Return 0 on success, -1 and errno set on error.
 */
int
scan_scaled_def(char *maybe_scaled, long long *result, char def)
{
	int ret;
	char *s = NULL;
	size_t length;

	length = strlen(maybe_scaled);
	if (length > 0) {
		if (isascii(maybe_scaled[length - 1]) &&
			isdigit(maybe_scaled[length - 1])) {
			asprintf(&s, "%s%c", maybe_scaled, def);
			if (s == NULL) {
				err(ERR_NOMEM, NULL);
			}
		}
	}
	ret = scan_scaled(s ? s : maybe_scaled, result);
	free(s);
	return ret;
}

/*
 * This function implements the rsync chmod symbolic mode parser
 * for the grammar described below (as taken from the chmod(1)
 * man page), including the addition of the "which" rule as
 * supported by rsync.
 *
 * Note that the 'u', 'g', and 'o' terminals of the "perm" rule
 * in chmod(1) are not supported by rsync.
 *
 *   mode    ::= clause [, clause ...]
 *   clause  ::= [which] [who ...] [action ...] action
 *   action  ::= op [perm ...]
 *   which   ::= D | F
 *   who     ::= a | u | g | o
 *   op      ::= + | - | =
 *   perm    ::= r | s | t | w | x | X
 *
 * If sess is NULL then arg's syntax will be verified,
 * but no mode transforms will be computed.
 */
int
chmod_parse(const char *arg, struct sess *sess)
{
	char *strbase, *str;
	int rc = 0;

	if (arg == NULL)
		return 0;

	str = strbase = strdup(arg);
	if (str == NULL)
		return errno;

	while (str != NULL) {
		const char *tok, *op;
		mode_t xbits, bits;
		mode_t mask, who;
		int which = 0;

		/* clause */
		tok = strsep(&str, ",");
		if (tok == NULL)
			break;

		/* [which] */
		if (*tok == 'D' || *tok == 'F')
			which = *tok++;

		xbits = bits = mask = who = 0;
		op = NULL;

		/* [who ...] op */
		while (op == NULL) {
			switch (*tok) {
			case 'a':
				mask |= S_IRWXU | S_IRWXG | S_IRWXO;
				who = mask;
				break;
			case 'u':
				mask |= S_IRWXU | S_ISUID;
				who = mask;
				break;
			case 'g':
				mask |= S_IRWXG | S_ISGID;
				who = mask;
				break;
			case 'o':
				mask |= S_IRWXO;
				who = mask;
				break;
			case '+':
			case '-':
			case '=':
				if (who == 0) {
					mask = umask(0);
					umask(mask);
					mask = ~mask;
				}
				op = tok;
				break;
			default:
				rc = EINVAL;
				goto errout;
			}

			tok++;
		}

		if (*tok == '\0')
			continue;

		/* [perm ...] */
		while (*tok) {
			switch (*tok++) {
			case 'r':
				bits |= mask & (S_IRUSR | S_IRGRP | S_IROTH);
				break;
			case 's':
				bits |= (mask & (S_ISUID | S_ISGID));
				break;
			case 't':
				bits |= S_ISTXT;
				break;
			case 'w':
				bits |= mask & (S_IWUSR | S_IWGRP | S_IWOTH);
				break;
			case 'x':
				bits |= mask & (S_IXUSR | S_IXGRP | S_IXOTH);
				break;
			case 'X':
				xbits |= mask & (S_IXUSR | S_IXGRP | S_IXOTH);
				break;
			default:
				rc = EINVAL;
				goto errout;
			}
		}

		if (sess == NULL)
			continue; /* syntax check only */

		/* Apply mode transformations to the session chmod fields.
		 */
		switch (*op) {
		case '+':
			if (which == 0 || which == 'D') {
				sess->chmod_dir_AND &= ~bits;
				sess->chmod_dir_OR |= bits;
				sess->chmod_dir_X |= xbits;
			}
			if (which == 0 || which == 'F') {
				sess->chmod_file_AND &= ~bits;
				sess->chmod_file_OR |= bits;
				sess->chmod_file_X |= xbits;
			}
			break;
		case '-':
			if (which == 0 || which == 'D') {
				sess->chmod_dir_AND |= bits;
				sess->chmod_dir_OR &= ~bits;
			}
			if (which == 0 || which == 'F') {
				sess->chmod_file_AND |= bits;
				sess->chmod_file_OR &= ~bits;
			}
			break;
		case '=':
			if (which == 0 || which == 'D') {
				if (who == 0)
					sess->chmod_dir_AND = 07777;
				else
					sess->chmod_file_AND = mask & 0x777;
				sess->chmod_dir_OR = bits;
			}
			if (which == 0 || which == 'F') {
				if (who == 0)
					sess->chmod_file_AND = 07777;
				else
					sess->chmod_file_AND = mask & 0777;
				sess->chmod_file_OR = bits;
			}
			break;
		default:
			rc = EINVAL;
			goto errout;
		}
	}

  errout:
	free(strbase);
	return rc;
}

static struct opts	 opts;

enum {
	OP_ADDRESS = CHAR_MAX + 1,
	OP_PORT,
	OP_RSYNCPATH,
	OP_TIMEOUT,
	OP_CONTIMEOUT,

	OP_DAEMON,

	OP_EXCLUDE,
	OP_NO_D,
	OP_INCLUDE,
	OP_EXCLUDE_FROM,
	OP_INCLUDE_FROM,
	OP_COMP_DEST,
	OP_COPY_DEST,
	OP_LINK_DEST,
	OP_MAX_SIZE,
	OP_MIN_SIZE,
	OP_NUMERIC_IDS,
	OP_SPARSE,

	OP_SOCKOPTS,

	OP_IGNORE_EXISTING,
	OP_IGNORE_NON_EXISTING,
	OP_DEL,
	OP_DEL_BEFORE,
	OP_DEL_DURING,
	OP_DEL_DELAY,
	OP_DEL_AFTER,
	OP_BWLIMIT,

	OP_NO_RELATIVE,

	OP_NO_DIRS,
	OP_FILESFROM,
	OP_APPEND,
	OP_PARTIAL_DIR,
	OP_CHECKSUM_SEED,
	OP_CHMOD,
	OP_BACKUP_DIR,
	OP_BACKUP_SUFFIX,
	OP_COPY_UNSAFE_LINKS,
	OP_SAFE_LINKS,
	OP_FORCE,
	OP_IGNORE_ERRORS,
	OP_PASSWORD_FILE,
	OP_PROTOCOL,
	OP_READ_BATCH,
	OP_WRITE_BATCH,
	OP_ONLY_WRITE_BATCH,
	OP_OUTFORMAT,
	OP_BIT8,
	OP_HELP,
	OP_BLOCKING_IO,
	OP_MODWIN,
	OP_MAX_DELETE,
	OP_STATS,
	OP_COMPLEVEL,
	OP_EXECUTABILITY,

#ifdef __APPLE__
	OP_TRACE,
#endif
};

const char rsync_shopts[] = "0468B:CDEFHIKLOPRSVWabcde:f:ghklnopqrtuvxyz";
const struct option	 rsync_lopts[] = {
    { "address",	required_argument, NULL,		OP_ADDRESS },
    { "append",		no_argument,	NULL,			OP_APPEND },
    { "archive",	no_argument,	NULL,			'a' },
    { "backup",		no_argument,	NULL,			'b' },
    { "backup-dir",	required_argument,	NULL,		OP_BACKUP_DIR },
    { "block-size",	required_argument, NULL,		'B' },
    { "blocking-io",	no_argument,	NULL,			OP_BLOCKING_IO },
    { "bwlimit",	required_argument, NULL,		OP_BWLIMIT },
    { "cache",		no_argument,	&opts.no_cache,		0 },
    { "no-cache",	no_argument,	&opts.no_cache,		1 },
    { "checksum",	no_argument,	NULL,			'c' },
    { "checksum-seed",	required_argument, NULL,		OP_CHECKSUM_SEED },
    { "chmod",		required_argument, NULL,		OP_CHMOD },
    { "compare-dest",	required_argument, NULL,		OP_COMP_DEST },
    { "copy-dest",	required_argument, NULL,		OP_COPY_DEST },
    { "link-dest",	required_argument, NULL,		OP_LINK_DEST },
    { "compress",	no_argument,	NULL,			'z' },
    { "compress-level",	required_argument, NULL,		OP_COMPLEVEL },
    { "contimeout",	required_argument, NULL,		OP_CONTIMEOUT },
    { "copy-dirlinks",	no_argument,	NULL,			'k' },
    { "copy-links",	no_argument,	&opts.copy_links,	'L' },
    { "copy-unsafe-links",	no_argument,	&opts.copy_unsafe_links,	OP_COPY_UNSAFE_LINKS },
    { "cvs-exclude",	no_argument,	NULL,			'C' },
    { "no-D",		no_argument,	NULL,			OP_NO_D },
    { "daemon",		no_argument,	NULL,			OP_DAEMON },
    { "del",		no_argument,	NULL,			OP_DEL },
    { "delete",		no_argument,	NULL,			OP_DEL },
    { "delete-before",	no_argument,	NULL,		OP_DEL_BEFORE },
    { "delete-during",	no_argument,	NULL,		OP_DEL_DURING },
    { "delete-delay",	no_argument,	NULL,		OP_DEL_DELAY },
    { "delete-after",	no_argument,	NULL,		OP_DEL_AFTER },
    { "delete-excluded",	no_argument,	&opts.del_excl,	1 },
    { "devices",	no_argument,	&opts.devices,		1 },
    { "no-devices",	no_argument,	&opts.devices,		0 },
    { "dry-run",	no_argument,	NULL,			'n' },
    { "exclude",	required_argument, NULL,		OP_EXCLUDE },
    { "exclude-from",	required_argument, NULL,		OP_EXCLUDE_FROM },
    { "executability",	no_argument,	NULL,			OP_EXECUTABILITY },
    { "existing",	no_argument, NULL,			OP_IGNORE_NON_EXISTING },
#ifdef __APPLE__
    { "extended-attributes",	no_argument, NULL,		'E' },
#endif
    { "filter",		required_argument, NULL,		'f' },
    { "force",		no_argument,	NULL,			OP_FORCE },
    { "fuzzy",		no_argument,	NULL,			'y' },
    { "group",		no_argument,	NULL,			'g' },
    { "no-group",	no_argument,	&opts.preserve_gids,	0 },
    { "no-g",		no_argument,	&opts.preserve_gids,	0 },
    { "hard-links",	no_argument,	&opts.hard_links,	'H' },
    { "help",		no_argument,	NULL,			OP_HELP },
    { "human-readable",	no_argument,	NULL,			'h' },
    { "ignore-errors",	no_argument,	NULL,			OP_IGNORE_ERRORS },
    { "ignore-existing", no_argument,	NULL,			OP_IGNORE_EXISTING },
    { "ignore-non-existing", no_argument, NULL,			OP_IGNORE_NON_EXISTING },
    { "ignore-times",	no_argument,	NULL,			'I' },
    { "include",	required_argument, NULL,		OP_INCLUDE },
    { "include-from",	required_argument, NULL,		OP_INCLUDE_FROM },
    { "inplace",	no_argument,	&opts.inplace,		1 },
    { "ipv4",		no_argument,	NULL,			'4' },
    { "ipv6",		no_argument,	NULL,			'6' },
    { "keep-dirlinks",	no_argument,	NULL,			'K' },
    { "links",		no_argument,	NULL,			'l' },
    { "max-delete",	required_argument, NULL,		OP_MAX_DELETE },
    { "max-size",	required_argument, NULL,		OP_MAX_SIZE },
    { "min-size",	required_argument, NULL,		OP_MIN_SIZE },
    { "motd",		no_argument,	&opts.no_motd,		0 },
    { "no-motd",	no_argument,	&opts.no_motd,		1 },
    { "no-links",	no_argument,	&opts.preserve_links,	0 },
    { "no-l",		no_argument,	&opts.preserve_links,	0 },
    { "numeric-ids",	no_argument,	NULL,			OP_NUMERIC_IDS },
    { "omit-dir-times",	no_argument,	NULL,			'O' },
    { "owner",		no_argument,	NULL,			'o' },
    { "no-owner",	no_argument,	&opts.preserve_uids,	0 },
    { "no-o",		no_argument,	&opts.preserve_uids,	0 },
    { "one-file-system",no_argument,	NULL,			'x' },
    { "password-file",	required_argument, NULL,		OP_PASSWORD_FILE },
    { "partial",	no_argument,	&opts.partial,		1 },
    { "no-partial",	no_argument,	&opts.partial,		0 },
    { "partial-dir",	required_argument,	NULL,		OP_PARTIAL_DIR },
    { "perms",		no_argument,	NULL,			'p' },
    { "no-perms",	no_argument,	&opts.preserve_perms,	0 },
    { "no-p",		no_argument,	&opts.preserve_perms,	0 },
    { "port",		required_argument, NULL,		OP_PORT },
    { "protocol",	required_argument, NULL,		OP_PROTOCOL },
    { "quiet",		no_argument,	NULL,			'q' },
    { "read-batch",	required_argument, NULL,		OP_READ_BATCH },
    { "recursive",	no_argument,	NULL,			'r' },
    { "no-recursive",	no_argument,	&opts.recursive,	0 },
    { "no-r",		no_argument,	&opts.recursive,	0 },
    { "rsh",		required_argument, NULL,		'e' },
    { "rsync-path",	required_argument, NULL,		OP_RSYNCPATH },
    { "safe-links",	no_argument,	&opts.safe_links,	1 },
    { "sender",		no_argument,	&opts.sender,		1 },
    { "server",		no_argument,	&opts.server,		1 },
    { "size-only",	no_argument,	&opts.size_only,	1 },
    { "sockopts",	required_argument,	NULL,		OP_SOCKOPTS },
    { "specials",	no_argument,	&opts.specials,		1 },
    { "no-specials",	no_argument,	&opts.specials,		0 },
    { "sparse",		no_argument,	NULL,			'S' },
    { "stats",		no_argument,	NULL,			OP_STATS },
    { "suffix",		required_argument,	NULL,		OP_BACKUP_SUFFIX },
    { "super",		no_argument,	&opts.supermode,	SMODE_ON },
    { "no-super",	no_argument,	&opts.supermode,	SMODE_OFF },
#if 0
    { "sync-file",	required_argument, NULL,		6 },
#endif
#ifdef __APPLE__
    { "syslog-trace",	no_argument,	&syslog_trace,		1 },
#endif
    { "temp-dir",	required_argument, NULL,		'T' },
    { "timeout",	required_argument, NULL,		OP_TIMEOUT },
    { "times",		no_argument,	NULL,			't' },
    { "no-times",	no_argument,	&opts.preserve_times,	0 },
    { "no-t",		no_argument,	&opts.preserve_times,	0 },
    { "update",		no_argument,	NULL,			'u' },
    { "verbose",	no_argument,	NULL,			'v' },
    { "no-verbose",	no_argument,	&verbose,		0 },
    { "no-v",		no_argument,	&verbose,		0 },
    { "whole-file",	no_argument,	NULL,			'W' },
    { "no-whole-file",	no_argument,	&opts.whole_file,	0 },
    { "no-W",		no_argument,	&opts.whole_file,	0 },
    { "only-write-batch",	required_argument, NULL,	OP_ONLY_WRITE_BATCH },
    { "write-batch",	required_argument, NULL,		OP_WRITE_BATCH },
    { "progress",	no_argument,	&opts.progress,		1 },
    { "no-progress",	no_argument,	&opts.progress,		0 },
    { "backup",		no_argument,	NULL,			'b' },
    { "relative",	no_argument,	NULL,			'R' },
    { "no-R",		no_argument,	NULL,			OP_NO_RELATIVE },
    { "no-relative",	no_argument,	NULL,			OP_NO_RELATIVE },
    { "remove-sent-files",	no_argument,	&opts.remove_source,	1 },
    { "remove-source-files",	no_argument,	&opts.remove_source,	1 },
    { "version",	no_argument,	NULL,			'V' },
    { "dirs",		no_argument,	NULL,			'd' },
    { "no-dirs",	no_argument,	NULL,			OP_NO_DIRS },
    { "files-from",	required_argument,	NULL,		OP_FILESFROM },
    { "from0",		no_argument,	NULL,			'0' },
    { "out-format",	required_argument,	NULL,		OP_OUTFORMAT },
    { "delay-updates",	no_argument,	&opts.dlupdates,	1 },
    { "modify-window",	required_argument,	NULL,		OP_MODWIN },
    { "8-bit-output",	no_argument,	NULL,			OP_BIT8 },
    { NULL,		0,		NULL,			0 }
};

static void
usage(int exitcode)
{
	fprintf(exitcode == 0 ? stdout : stderr, "usage: %s"
	    " [-0468BCDEFHIKLOPRSTWVabcdghklnopqrtuvxyz] [-e program] [-f filter]\n"
	    "\t[--8-bit-output] [--address=sourceaddr]\n"
	    "\t[--append] [--backup-dir=dir] [--bwlimit=limit] [--cache | --no-cache]\n"
	    "\t[--compare-dest=dir] [--contimeout] [--copy-dest=dir] [--copy-unsafe-links]\n"
	    "\t[--del | --delete-after | --delete-before | --delete-during]\n"
	    "\t[--delay-updates] [--dirs] [--no-dirs]\n"
	    "\t[--exclude] [--exclude-from=file]\n"
#ifdef __APPLE__
	    "\t[--extended-attributes]\n"
#endif
	    "\t[--existing] [--force] [--ignore-errors]\n"
	    "\t[--ignore-existing] [--ignore-non-existing] [--include]\n"
	    "\t[--include-from=file] [--inplace] [--keep-dirlinks] [--link-dest=dir]\n"
	    "\t[--max-delete=NUM] [--max-size=SIZE] [--min-size=SIZE]\n"
	    "\t[--modify-window=sec] [--no-motd] [--numeric-ids]\n"
	    "\t[--out-format=FMT] [--partial] [--password-file=pwfile] [--port=portnumber]\n"
	    "\t[--progress] [--protocol] [--read-batch=file]\n"
	    "\t[--remove-source-files] [--rsync-path=program] [--safe-links] [--size-only]\n"
	    "\t[--sockopts=sockopts] [--specials] [--suffix] [--super] [--timeout=seconds]\n"
	    "\t[--only-write-batch=file | --write-batch=file]\n"
	    "\tsource ... directory\n",
	    getprogname());
	exit(exitcode);
}

/*
 * Parse options out of argv; this is shared with daemon mode to parse options
 * sent over the wire from clients.  Filtering is primarily so that the daemon
 * can reject arguments based on their names.
 *
 * Returns NULL on error or the client options struct on success -- for
 * convenience we'll just use the global we've setup so that we don't need to
 * change the definition of lopts, but if we wanted to be able to take a
 * caller-allocated struct opts we'd need to return OP_* or 'f'lags from every
 * option.
 */
struct opts *
rsync_getopt(int argc, char *argv[], rsync_option_filter *filter,
    struct sess *sess)
{
	long long	 tmplong;
	int		 tmpint;
	size_t		 basedir_cnt = 0;
	const char	*errstr;
	int		 c, cvs_excl, lidx;
	int		 implied_recursive = 0;
	int		 opts_F = 0, opts_no_relative = 0, opts_no_dirs = 0;
	int		 opts_only_batch = 0;
	int		 opts_timeout = -1;

	/*
	 * In the case of the daemon, we're re-entering and opts may be dirty
	 * in ways that matter.  Zero it out to be safe.
	 */
	memset(&opts, 0, sizeof(opts));

	cvs_excl = 0;
	lidx = -1;
	opts.max_size = opts.min_size = -1;
	opts.compression_level = Z_DEFAULT_COMPRESSION;
	opts.whole_file = -1;
	opts.outfile = stderr;
#ifdef __APPLE__
	opts.no_cache = 1;
#endif
	opts.protocol = RSYNC_PROTOCOL;

	while ((c = getopt_long(argc, argv, rsync_shopts, rsync_lopts, &lidx)) != -1) {
		/* Give the filter a shot to reject the option. */
		if (filter != NULL) {
			const struct option *lopt;
			int rc;
			char shopt;

			/*
			 * If we can tie this particular option to a long opt,
			 * even if the short option version of it was specified,
			 * then we should.  The daemon needs to be able to
			 * reject based on either name.
			 */
			if (lidx == -1) {
				shopt = c;

				for (lopt = rsync_lopts; lopt->name != NULL; lopt++) {
					if (lopt->flag == NULL &&
					    lopt->val == c)
						break;
				}

				if (lopt->name == NULL)
					lopt = NULL;
			} else {
				lopt = &rsync_lopts[lidx];
				if (isprint(lopt->val))
					shopt = lopt->val;
				else
					shopt = 0;
			}

			rc = (*filter)(sess, shopt, lopt);
			if (rc < 0)
				continue;
			else if (rc == 0)
				return NULL;
		}
		switch (c) {
		case '0':
			opts.from0 = 1;
			break;
		case '4':
			opts.ipf = 4;
			break;
		case '6':
			opts.ipf = 6;
			break;
		case '8':
			opts.bit8++;
			break;
		case 'B':
			if (scan_scaled(optarg, &tmplong) == -1)
				errx(1, "--block-size=%s: invalid numeric value", optarg);
			if (tmplong < 0)
				errx(1, "--block-size=%s: must be no less than 0", optarg);
			/* Upper bound checked only if differential transfer is required */
			opts.block_size = tmplong;
			break;
		case 'C':
			cvs_excl = 1;
			break;
		case 'D':
			opts.devices = 1;
			opts.specials = 1;
			break;
#ifdef __APPLE__
		case 'E':
			opts.extended_attributes = 1;
			break;
#endif
		case OP_EXECUTABILITY:
			opts.preserve_executability = 1;
			break;
		case 'F': {
			const char *new_rule = NULL;

			switch (++opts_F) {
			case 1:
				new_rule = ": /.rsync-filter";
				break;
			case 2:
				new_rule = "- .rsync-filter";
				break;
			default:
				/* Nop */
				break;
			}

			if (new_rule != NULL) {
				int ret;

				ret = parse_rule(new_rule, RULE_NONE,
				    opts.from0 ? 0 : '\n');
				assert(ret == 0);
			}
			break;
		}
		case 'H':
			opts.hard_links = 1;
			break;
		case 'O':
			opts.omit_dir_times = 1;
			break;
		case 'a':
			implied_recursive = 1;
			opts.recursive = 1;
			opts.preserve_links = 1;
			opts.preserve_perms = 1;
			opts.preserve_times = 1;
			opts.preserve_gids = 1;
			opts.preserve_uids = 1;
			opts.devices = 1;
			opts.specials = 1;
			break;
		case 'b':
			opts.backup++;
			break;
		case 'c':
			opts.checksum = 1;
			break;
		case 'd':
			opts.dirs = DIRMODE_REQUESTED;
			break;
		case 'e':
			opts.ssh_prog = optarg;
			break;
		case 'f':
			if (parse_rule(optarg, RULE_NONE,
			    opts.from0 ? 0 : '\n') == -1)
				errx(ERR_SYNTAX, "syntax error in filter: %s",
				    optarg);
			break;
		case 'g':
			opts.preserve_gids = 1;
			break;
		case 'k':
			opts.copy_dirlinks = 1;
			break;
		case 'K':
			opts.keep_dirlinks = 1;
			break;
		case 'l':
			opts.preserve_links = 1;
			break;
		case 'L':
			opts.copy_links = 1;
			break;
		case 'n':
			opts.dry_run = DRY_FULL;
			break;
		case 'o':
			opts.preserve_uids = 1;
			break;
		case 'P':
			opts.partial = 1;
			opts.progress++;
			break;
		case 'p':
			opts.preserve_perms = 1;
			break;
		case 'q':
			opts.quiet++;
			break;
		case 'r':
			implied_recursive = 0;
			opts.recursive = 1;
			break;
		case 't':
			opts.preserve_times = 1;
			break;
		case 'u':
			opts.update++;
			break;
		case 'v':
			verbose++;
			break;
		case 'x':
			opts.one_file_system++;
			break;
		case 'y':
			opts.fuzzy_basis = 1;
			break;
		case 'z':
			opts.compress++;
			break;
		case 'I':
			opts.ignore_times++;
			break;
		case 'S':
			opts.sparse++;
			break;
		case 'T':
			free(opts.temp_dir);
			opts.temp_dir = strdup(optarg);
			if (opts.temp_dir == NULL)
				errx(ERR_NOMEM, NULL);
			break;
		case 'W':
			opts.whole_file = 1;
			break;
		case 0:
			/* Non-NULL flag values (e.g., --sender). */
			break;
#if 0
		case 6:
			opts.syncfile = optarg;
			break;
#endif
		case OP_ADDRESS:
			opts.address = optarg;
			break;
		case OP_CONTIMEOUT:
			poll_contimeout = (int)strtonum(optarg, 0, 60*60, &errstr);
			if (errstr != NULL)
				errx(ERR_SYNTAX, "timeout is %s: %s",
				    errstr, optarg);
			break;
		case OP_PORT:
			opts.port = optarg;
			break;
		case OP_RSYNCPATH:
			opts.rsync_path = optarg;
			break;
		case OP_TIMEOUT:
			opts_timeout = (int)strtonum(optarg, 0, 60*60, &errstr);
			if (errstr != NULL)
				errx(ERR_SYNTAX, "timeout is %s: %s",
				    errstr, optarg);
			break;
		case OP_EXCLUDE:
			if (parse_rule(optarg, RULE_EXCLUDE, 0) == -1)
				errx(ERR_SYNTAX, "syntax error in exclude: %s",
				    optarg);
			break;
		case OP_INCLUDE:
			if (parse_rule(optarg, RULE_INCLUDE, 0) == -1)
				errx(ERR_SYNTAX, "syntax error in include: %s",
				    optarg);
			break;
		case OP_EXCLUDE_FROM:
			parse_file(optarg, RULE_EXCLUDE, opts.from0 ? 0 : '\n');
			break;
		case OP_INCLUDE_FROM:
			parse_file(optarg, RULE_INCLUDE, opts.from0 ? 0 : '\n');
			break;
		case OP_BLOCKING_IO:
			/* We ignore this flag */
			break;
		case OP_COMP_DEST:
			if (opts.alt_base_mode != 0 &&
			    opts.alt_base_mode != BASE_MODE_COMPARE) {
				errx(1, "option --%s conflicts with %s",
				    rsync_lopts[lidx].name,
				    alt_base_mode(opts.alt_base_mode));
			}
			opts.alt_base_mode = BASE_MODE_COMPARE;
			goto basedir;
		case OP_COPY_DEST:
			if (opts.alt_base_mode != 0 &&
			    opts.alt_base_mode != BASE_MODE_COPY) {
				errx(1, "option --%s conflicts with %s",
				    rsync_lopts[lidx].name,
				    alt_base_mode(opts.alt_base_mode));
			}
			opts.alt_base_mode = BASE_MODE_COPY;
			goto basedir;
		case OP_DAEMON:
			exit(rsync_daemon(argc, argv, &opts));
			break;
		case OP_DEL:
			/* nop if a --delete-* option has already been specified. */
			if (opts.del == DMODE_NONE)
				opts.del = DMODE_UNSPECIFIED;
			break;
		case OP_DEL_BEFORE:
			if (opts.del > DMODE_UNSPECIFIED)
				errx(1, "may only specify one --delete-* option");

			opts.del = DMODE_BEFORE;
			break;
		case OP_DEL_DURING:
			if (opts.del > DMODE_UNSPECIFIED)
				errx(1, "may only specify one --delete-* option");

			opts.del = DMODE_DURING;
			break;
		case OP_DEL_DELAY:
			if (opts.del > DMODE_UNSPECIFIED)
				errx(1, "may only specify one --delete-* option");

			opts.del = DMODE_DELAY;
			break;
		case OP_DEL_AFTER:
			if (opts.del > DMODE_UNSPECIFIED)
				errx(1, "may only specify one --delete-* option");

			opts.del = DMODE_AFTER;
			break;
		case OP_LINK_DEST:
			if (opts.alt_base_mode != 0 &&
			    opts.alt_base_mode != BASE_MODE_LINK) {
				errx(1, "option --%s conflicts with %s",
				    rsync_lopts[lidx].name,
				    alt_base_mode(opts.alt_base_mode));
			}
			opts.alt_base_mode = BASE_MODE_LINK;

basedir:
			if (basedir_cnt >= MAX_BASEDIR)
				errx(1, "too many --%s directories specified",
				    rsync_lopts[lidx].name);
			opts.basedir[basedir_cnt++] = optarg;
			break;
		case OP_READ_BATCH:
			opts.read_batch = optarg;
			break;
		case OP_ONLY_WRITE_BATCH:
			opts_only_batch = 1;
			/* FALLTHROUGH */
		case OP_WRITE_BATCH:
			opts.write_batch = optarg;
			break;
		case OP_SPARSE:
			opts.sparse++;
			break;
		case OP_MAX_SIZE:
			if (scan_scaled(optarg, &tmplong) == -1)
				err(1, "bad max-size");
			opts.max_size = tmplong;
			break;
		case OP_MIN_SIZE:
			if (scan_scaled(optarg, &tmplong) == -1)
				err(1, "bad min-size");
			opts.min_size = tmplong;
			break;
		case OP_NUMERIC_IDS:
			opts.numeric_ids = NIDS_FULL;
			break;
		case OP_NO_D:
			opts.devices = 0;
			opts.specials = 0;
			break;
		case OP_IGNORE_EXISTING:
			opts.ign_exist++;
			break;
		case OP_IGNORE_NON_EXISTING:
			opts.ign_non_exist++;
			break;
		case 'R':
			opts.relative++;
			break;
		case OP_NO_RELATIVE:
			opts.relative = 0;
			opts_no_relative++;
			break;
		case OP_NO_DIRS:
			opts.dirs = 0;
			opts_no_dirs++;
			break;
		case OP_FILESFROM:
			opts.filesfrom = optarg;
			break;
		case OP_MODWIN:
		        opts.modwin = atoi(optarg);
			break;
		case OP_OUTFORMAT:
		        opts.outformat = optarg;
			break;
		case OP_APPEND:
			opts.append++;
			break;
		case OP_BWLIMIT:
			if (scan_scaled_def(optarg, &tmplong, 'k') == -1)
				err(1, "bad bwlimit");
			opts.bwlimit = tmplong;
			break;
		case OP_COPY_UNSAFE_LINKS:
			opts.copy_unsafe_links = 1;
			break;
		case OP_CHECKSUM_SEED:
			if (*optarg != '\0') {
				char *endptr;

				errno = 0;
				tmplong = strtoll(optarg, &endptr, 0);
				if (*endptr != '\0')
					errx(1, "--checksum-seed=%s: invalid numeric value",
					     optarg);
				if (tmplong < INT_MIN)
					errx(1, "--checksum-seed=%s: must be no less than %d",
					     optarg, INT_MIN);
				if (tmplong > INT_MAX)
					errx(1, "--checksum-seed=%s: must be no greater than %d",
					     optarg, INT_MAX);
				opts.checksum_seed = (tmplong == 0) ? (int)time(NULL) : (int)tmplong;
			}
			break;
		case OP_CHMOD:
			if (chmod_parse(optarg, NULL) != 0)
				errx(ERR_SYNTAX, "--chmod=%s: invalid argument",
				     optarg);
			opts.chmod = optarg;
			break;
		case OP_BACKUP_DIR:
			free(opts.backup_dir);
			opts.backup_dir = strdup(optarg);
			if (opts.backup_dir == NULL)
				errx(ERR_NOMEM, NULL);
			break;
		case OP_BACKUP_SUFFIX:
			if (strchr(optarg, '/') != NULL) {
				errx(1, "--suffix cannot contain slashes: "
				    "%s\n", optarg);
			}
			free(opts.backup_suffix);
			opts.backup_suffix = strdup(optarg);
			if (opts.backup_suffix == NULL)
				errx(ERR_NOMEM, NULL);
			break;
		case OP_PASSWORD_FILE:
			opts.password_file = optarg;
			break;
		case OP_PARTIAL_DIR:
			opts.partial = 1;

			/*
			 * We stash off our own copy just to be logically
			 * consistent; if it's not specified here, we instead
			 * use RSYNC_PARTIAL_DIR from the environment if it's
			 * set which we'll naturally want to make a copy of.  We
			 * can thus always assume it's on the heap, rather than
			 * sometimes part of argv.
			 */
			free(opts.partial_dir);
			opts.partial_dir = strdup(optarg);
			if (opts.partial_dir == NULL)
				errx(ERR_NOMEM, NULL);
			break;
		case OP_PROTOCOL:
			if (*optarg != '\0') {
				char *endptr;

				tmpint = (int)strtoll(optarg, &endptr, 0);
				if (*endptr != '\0') {
					errx(1, "--protocol=%s: invalid value",
					    optarg);
				}
				if (tmpint < RSYNC_PROTOCOL_MIN ||
				    tmpint > RSYNC_PROTOCOL_MAX) {
					errx(1, "--protocol=%s: out of range, "
					    "min: %d, max: %d", optarg,
					    RSYNC_PROTOCOL_MIN,
					    RSYNC_PROTOCOL_MAX);
				}
				if (tmpint > RSYNC_PROTOCOL) {
					WARNX("--protocol=%s: is not supported "
					    "by this version of openrsync. "
					    "min: %d, max: %d", optarg,
					    RSYNC_PROTOCOL_MIN,
					    RSYNC_PROTOCOL_MAX);
				}
				opts.protocol = tmpint;
			}
			break;
		case OP_STATS:
			opts.stats++;
			break;
		case OP_SOCKOPTS:
			opts.sockopts = optarg;
			break;
		case OP_FORCE:
			opts.force_delete++;
			break;
		case OP_COMPLEVEL:
			if (*optarg != '\0') {
				char *endptr;

				errno = 0;
				tmpint = (int)strtoll(optarg, &endptr, 0);
				if (*endptr != '\0')
					errx(1, "--compress-level=%s: invalid numeric value",
					     optarg);
				if (tmpint < Z_DEFAULT_COMPRESSION)
					errx(1, "--compress-level=%s: must be no less than %d",
					     optarg, Z_DEFAULT_COMPRESSION);
				if (tmpint > Z_BEST_COMPRESSION)
					errx(1, "--compress-level=%s: must be no greater than %d",
					     optarg, Z_BEST_COMPRESSION);

				opts.compression_level = tmpint;
				if (opts.compression_level == Z_NO_COMPRESSION)
					opts.compress = 0;
			}
			break;
		case OP_IGNORE_ERRORS:
			opts.ignore_errors++;
			break;
		case OP_BIT8:
			opts.bit8++;
			break;
		case 'h':
			/* -h without any other parameters */
			if (argc == 2) {
				usage(0);
				break;
			}
			opts.human_readable++;
			break;
		case OP_MAX_DELETE:
			if (*optarg != '\0') {
				char *endptr;

				errno = 0;
				tmpint = (int)strtoll(optarg, &endptr, 0);
				if (*endptr != '\0')
					errx(1, "--max-delete=%s: invalid numeric value",
					     optarg);
				if (tmpint < INT_MIN)
					errx(1, "--max-delete=%s: must be no less than %d",
					     optarg, INT_MIN);
				if (tmpint > INT_MAX)
					errx(1, "--max-delete=%s: must be no greater than %d",
					     optarg, INT_MAX);

				opts.max_delete = tmpint;
			}
			break;
		case 'V':
			printf("openrsync: protocol version %u\n",
			    RSYNC_PROTOCOL);
			printf("rsync version 2.6.9 compatible\n");
			exit(0);
		case OP_HELP:
			usage(0);
			break;
		default:
			usage(ERR_SYNTAX);
		}

		lidx = -1;
	}

	if (opts.quiet > 0)
		verbose = 0;

	/* Shouldn't be possible. */
	assert(opts.ipf == 0 || opts.ipf == 4 || opts.ipf == 6);

	if (opts.inplace) {
		if (opts.partial_dir != NULL)
			errx(ERR_SYNTAX,
			    "option --partial-dir conflicts with --inplace");
		opts.partial = 1;
	} else if (opts.partial && opts.partial_dir == NULL) {
		char *rsync_partial_dir;

		/*
		 * XXX For delayed update mode, this should use .~tmp~ instead
		 * of RSYNC_PARTIAL_DIR if --partial-dir was not supplied here.
		 */
		rsync_partial_dir = getenv("RSYNC_PARTIAL_DIR");
		if (rsync_partial_dir != NULL && rsync_partial_dir[0] != '\0') {
			opts.partial_dir = strdup(rsync_partial_dir);
			if (opts.partial_dir == NULL)
				errx(ERR_NOMEM, NULL);
		}
	}

	if (opts.partial_dir != NULL) {
		char *partial_dir;

		/* XXX Samba rsync would normalize this path a little better. */
		partial_dir = opts.partial_dir;
		if (partial_dir[0] == '\0' || strcmp(partial_dir, ".") == 0) {
			free(opts.partial_dir);
			opts.partial_dir = NULL;
		} else {
			char *endp;

			endp = &partial_dir[strlen(partial_dir) - 1];
			while (endp > partial_dir && *(endp - 1) == '/') {
				*endp-- = '\0';
			}

			if (parse_rule(partial_dir, RULE_EXCLUDE, 0) == -1) {
				errx(ERR_SYNTAX, "syntax error in exclude: %s",
				    partial_dir);
			}
		}
	}
	if (opts.append && opts.whole_file > 0) {
		errx(ERR_SYNTAX,
		    "options --append and --whole-file cannot be combined");
	}

	if (opts.backup_suffix == NULL) {
		opts.backup_suffix = opts.backup_dir ? strdup("") : strdup("~");
	}
	if (opts.backup && opts.del > DMODE_UNSPECIFIED && !opts.del_excl) {
		char rbuf[PATH_MAX];

		snprintf(rbuf, sizeof(rbuf), "P *%s", opts.backup_suffix);
		if (parse_rule(rbuf, RULE_NONE, 0) == -1) {
			errx(ERR_SYNTAX, "error adding protect rule: %s",
			    rbuf);
		}
	}
	if (opts.backup && !opts.backup_dir) {
		opts.omit_dir_times = 1;
	}

	if (opts.port == NULL)
		opts.port = "rsync";

	/* by default and for --contimeout=0 disable poll_contimeout */
	if (poll_contimeout == 0)
		poll_contimeout = -1;
	else
		poll_contimeout *= 1000;

	/*
	 * Mostly for the daemon's benefit, but harmless for the other modes of
	 * operation; only allow options to increase the timeout we started out
	 * with.  For most purposes this allows the full effective range of
	 * timeout values [0, INT_MAX], but the daemon may establish a timeout
	 * floor to avoid clients requesting infinite timeout if configured to
	 * do so.
	 */
	if (opts_timeout > poll_timeout)
		poll_timeout = opts_timeout;

	/* by default and for --timeout=0 disable poll_timeout */
	if (poll_timeout == 0)
		poll_timeout = -1;
	else
		poll_timeout *= 1000;

	if (opts.filesfrom != NULL) {
		if (split_hostspec(opts.filesfrom, &opts.filesfrom_host,
				&opts.filesfrom_path)) {
			LOG2("remote file for filesfrom: '%s' '%s'\n",
				opts.filesfrom_host, opts.filesfrom_path);

		} else {
			opts.filesfrom_path = strdup(opts.filesfrom);
			if (opts.filesfrom_path == NULL) {
				ERR("strdup filesfrom no host");
				return NULL;
			}
			opts.filesfrom_host = NULL;
		}
		if (!opts_no_relative)
			opts.relative = 1;
		if (!opts_no_dirs)
			opts.dirs = DIRMODE_IMPLIED;
		if (implied_recursive)
			opts.recursive = 0;
	}

	if (opts.relative && opts_no_relative)
		ERRX1("Cannot use --relative and --no-relative at the same time");
	if (opts.dirs && opts_no_dirs)
		ERRX1("Cannot use --dirs and --no-dirs at the same time");

	/*
	 * XXX rsync started defaulting to --delete-during in later versions of
	 * the protocol (30 and up).
	 */
	if (opts.del == DMODE_UNSPECIFIED)
		opts.del = DMODE_BEFORE;

	if (opts.write_batch != NULL && opts.read_batch != NULL)
		err(ERR_SYNTAX,
		    "--write-batch and --read-batch are incompatible");

	/*
	 * --dry-run turns off write batching altogether; --only-write-batch
	 * turns on the xfer-only dry-run mode.
	 */
	if (opts.dry_run)
		opts.write_batch = NULL;
	else if (opts_only_batch)
		opts.dry_run = DRY_XFER;

	if (!opts.server) {
		if (cvs_excl) {
			int ret;

			ret = parse_rule("-C", RULE_NONE, '\n');
			assert(ret == 0);

			ret = parse_rule(":C", RULE_NONE, '\n');
			assert(ret == 0);

			/* Silence NDEBUG warnings */
			(void)ret;
		}
	}

	return &opts;
}

int
main(int argc, char *argv[])
{
	pid_t		 child;
	int		 fds[2], sd = -1, rc, st, i;
	struct sess	 sess;
	struct fargs	*fargs;
	char		**args;
	int              printflags;

#ifdef __APPLE__
	/*
	 * csu would use argv[0] as the default progname, but dyld has knowledge
	 * of the executable name from the kernel and will install that as the
	 * progname prior to main().  We actually do want argv[0] as the
	 * progname so that help and usage errors print the name we want to be
	 * invoked as, i.e., "rsync."
	 *
	 * Remove this if openrsync moves out of /usr/libexec.
	 */
	setprogname(argv[0]);
#endif

	/* Global pledge. */

#ifndef __APPLE__
	if (pledge("stdio unix rpath wpath cpath dpath inet fattr chown dns getpw proc exec unveil",
	    NULL) == -1)
		err(ERR_IPC, "pledge");
#endif

	rsync_set_logfile(stderr);
#ifdef __APPLE__
	if (os_variant_has_internal_content(RSYNC_SUBSYSTEM))
		syslog_trace = 1;
	if (syslog_trace)
		syslog_trace_obj = os_log_create("com.apple.rsync", "trace");
#endif

	setlocale(LC_ALL, "");
	setlocale(LC_CTYPE, "");
	setlocale(LC_NUMERIC, "");

	if (rsync_getopt(argc, argv, NULL, NULL) == NULL) {
		usage(ERR_SYNTAX);
	}

	argc -= optind;
	argv += optind;

	/*
	 * We've loosened this restriction for --read-batch, but rsync still
	 * allows just one argument to be specified.  With only one argument,
	 * it treats that argument as the source and runs in --list-only mode
	 * instead of doing any copying.
	 *
	 * in --server mode, there are only ever 2 arguments, "." and
	 * the source or destinations.  However in rsync 2.x if those
	 * are specified as "host:" then the --server gets them as ""
	 * and we have to convert it to an implied "."
	 */
	if (opts.read_batch == NULL && opts.server == 0 && argc < 2)
		usage(ERR_SYNTAX);
	else if (argc < 1)
		usage(ERR_SYNTAX);

	/*
	  * Determine whether we:
	  * - need to itemize
	  * - need to do output late
	  */
	sess.opts = &opts;
	printflags = output(&sess, NULL, 0);
	if (printflags & 1)
		sess.itemize = 1;
	if (printflags & 2)
		sess.lateprint = 1;
	LOG3("Printing(%d): itemize %d late %d", getpid(), sess.itemize, sess.lateprint);

	/*
	 * Signals blocked until we understand what session we'll be using.
	 */
	cleanup_init(cleanup_ctx);

	/*
	 * This is what happens when we're started with the "hidden"
	 * --server option, which is invoked for the rsync on the remote
	 * host by the parent.
	 */

	if (opts.server) {
		if (opts.whole_file < 0) {
			/* Simplify all future checking of this value */
			opts.whole_file = 0;
		}
		exit(rsync_server(cleanup_ctx, &opts, (size_t)argc, argv));
	}

	/*
	 * To simplify the cleanup process, we create a new process group now so
	 * that we can reliably send SIGUSR1 to any children.
	 */
	if (setpgid(0, getpid()) == -1)
		err(ERR_IPC, "setpgid");

	/*
	 * Now we know that we're the client on the local machine
	 * invoking rsync(1).
	 * At this point, we need to start the client and server
	 * initiation logic.
	 * The client is what we continue running on this host; the
	 * server is what we'll use to connect to the remote and
	 * invoke rsync with the --server option.
	 */

	fargs = fargs_parse(argc, argv, &opts);
	assert(fargs != NULL);

	cleanup_set_args(cleanup_ctx, fargs);
	if (opts.read_batch != NULL)
		exit(rsync_batch(cleanup_ctx, &opts, fargs));

	if (opts.filesfrom_host != NULL) {
		if (fargs->host == NULL) {
			ERRX("Remote --files-from with a local transfer is not valid");
			exit(2);
		}

		LOG2("--files-from host '%s'", opts.filesfrom_host);
		if (opts.filesfrom_host[0] == '\0') {
			/*
			 * The exact host doesn't matter in this case, so we'll
			 * just leave it empty -- not NULL is the pertinent
			 * detail from this point.
			 */
			LOG2("Inheriting --files-from remote side");
		} else {
			if (strcmp(opts.filesfrom_host,fargs->host)) {
				ERRX("Cannot have different hostnames "
					"for --files-from and paths.");
				exit(2);
			}
		}
	}

	/*
	 * For local transfers, enable whole_file by default
	 * if the user did not specifically ask for --no-whole-file.
	 */
	if (fargs->host == NULL && !fargs->remote && opts.whole_file < 0) {
		opts.whole_file = 1;
	} else if (opts.whole_file < 0) {
		/* Simplify all future checking of this value */
		opts.whole_file = 0;
	}

	/*
	 * If we're contacting an rsync:// daemon, then we don't need to
	 * fork, because we won't start a server ourselves.
	 * Route directly into the socket code, unless a remote shell
	 * has explicitly been specified.
	 */

	if (fargs->remote && opts.ssh_prog == NULL) {
		if ((rc = rsync_connect(&opts, &sd, fargs)) == 0) {
			rc = rsync_socket(cleanup_ctx, &opts, sd, fargs);
			close(sd);
		}
		exit(rc);
	}

	/* Drop the dns/inet possibility. */

#ifndef __APPLE__
	if (pledge("stdio unix rpath wpath cpath dpath fattr chown getpw proc exec unveil",
	    NULL) == -1)
		err(ERR_IPC, "pledge");
#endif

	/* Create a bidirectional socket and start our child. */

#if HAVE_SOCK_NONBLOCK
	if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0, fds) == -1)
		err(ERR_IPC, "socketpair");
#else
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == -1)
		err(ERR_IPC, "socketpair");
	if (fcntl(fds[0], F_SETFL, fcntl(fds[0], F_GETFL, 0) | O_NONBLOCK) == -1)
		err(ERR_IPC, "fcntl");
	if (fcntl(fds[1], F_SETFL, fcntl(fds[1], F_GETFL, 0) | O_NONBLOCK) == -1)
		err(ERR_IPC, "fcntl");
#endif

	switch ((child = fork())) {
	case -1:
		err(ERR_IPC, "fork");
	case 0:
		close(fds[0]);
#ifndef __APPLE__
		if (pledge("stdio exec", NULL) == -1)
			err(ERR_IPC, "pledge");
#endif

		memset(&sess, 0, sizeof(struct sess));
		sess.opts = &opts;
		sess.seed = opts.checksum_seed;

		/*
		 * We're about to exec(), but we need to make sure the
		 * appropriate signals are unblocked so that we can be
		 * interrupted earlier if needed.
		 */
		cleanup_set_session(cleanup_ctx, &sess);
		cleanup_release(cleanup_ctx);

		args = fargs_cmdline(&sess, fargs, NULL);

		for (i = 0; args[i] != NULL; i++)
			LOG2("exec[%d] = %s", i, args[i]);

		/* Make sure the child's stdin is from the sender. */
		if (dup2(fds[1], STDIN_FILENO) == -1)
			err(ERR_IPC, "dup2");
		if (dup2(fds[1], STDOUT_FILENO) == -1)
			err(ERR_IPC, "dup2");
		if (execvp(args[0], args) == -1)
			ERR("exec on '%s'", args[0]);
		_exit(ERR_IPC);
		/* NOTREACHED */
	default:
		cleanup_set_child(cleanup_ctx, child);

		close(fds[1]);
		if (!fargs->remote)
			rc = rsync_client(cleanup_ctx, &opts, fds[0], fargs);
		else
			rc = rsync_socket(cleanup_ctx, &opts, fds[0], fargs);
		break;
	}

	close(fds[0]);

#if 0
	/*
	 * The server goes into an infinite sleep loop once it's concluded to
	 * avoid closing the various pipes.  This gives us time to finish
	 * draining whatever's left and making our way cleanly through the state
	 * machine, after which we come here and signal the child that it's safe
	 * to shutdown.
	 */
	kill(child, SIGUSR2);
#endif

	if (waitpid(child, &st, 0) == -1)
		err(ERR_WAITPID, "waitpid");

	/*
	 * Best effort to avoid a little bit of work during cleanup, but cleanup
	 * will use WNOHANG and just move on if the child's already been reaped.
	 */
	cleanup_set_child(cleanup_ctx, 0);

	/*
	 * If we don't already have an error (rc == 0), then inherit the
	 * error code of rsync_server() if it has exited.
	 * If it hasn't exited, it overrides our return value.
	 */

	if (rc == 0) {
		if (WIFEXITED(st))
			rc = WEXITSTATUS(st);
		else if (WIFSIGNALED(st)) {
			if (WTERMSIG(st) != SIGUSR2)
				rc = ERR_TERMIMATED;
		} else
			rc = ERR_WAITPID;
	}

	free(opts.filesfrom_host);
	free(opts.filesfrom_path);

	exit(rc);
}
