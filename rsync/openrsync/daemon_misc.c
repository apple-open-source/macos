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
#include <sys/stat.h>
#include <sys/wait.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "daemon.h"

_Static_assert(sizeof(id_t) <= INT_MAX, "{u,g}id_t larger than expected");

static int daemon_rangelock(struct sess *, const char *, const char *, int);

/* Compatible with smb rsync, just in case. */
#define	CONNLOCK_START(conn)	((conn) * 4)
#define	CONNLOCK_SIZE(conn)	(4)

int
daemon_apply_chmod(struct sess *sess, const char *module,
    struct opts *opts)
{
	struct daemon_role *role;
	const char *chmod, *which;
	int rc;

	role = (void *)sess->role;
	if (opts->sender)
		which = "outgoing chmod";
	else
		which = "incoming chmod";

	if (!cfg_has_param(role->dcfg, module, which))
		return 1;

	rc = cfg_param_str(role->dcfg, module, which, &chmod);
	assert(rc == 0);

	rc = chmod_parse(chmod, sess);
	if (rc != 0) {
		daemon_client_error(sess, "%s: failed to parse '%s': %s",
		    module, which, strerror(rc));
		return 0;
	}

	opts->chmod = chmod;

	return 1;
}

int
daemon_apply_chrootopts(struct sess *sess, const char *module,
    struct opts *opts, int use_chroot)
{
	struct daemon_role *role;
	int bnids;

	role = (void *)sess->role;

	/* If the client requested --numeric-ids, we'll just leave it be. */
	if (opts->numeric_ids != NIDS_OFF)
		return 1;

	/*
	 * If the parameter's not been specified, then its default depends on
	 * whether we're chrooted or not.  Note that `use_chroot` may be 0, 1,
	 * or 2, but the distinction between 1 and 2 (must chroot, try chroot)
	 * does not mattter because the caller shouldn't pass a try chroot if
	 * the chroot failed.
	 */
	if (!cfg_has_param(role->dcfg, module, "numeric ids")) {
		/*
		 * The client isn't aware that we're running with --numeric-ids,
		 * so we had to make this a tri-state to support a mode where we
		 * still send an empty list.
		 */
		if (use_chroot)
			opts->numeric_ids = NIDS_STEALTH;
		else
			opts->numeric_ids = NIDS_OFF;
		return 1;
	}

	/*
	 * Otherwise, we'll defer to a config-set value of numeric ids to
	 * determine if we're doing it or not.
	 */
	if (cfg_param_bool(role->dcfg, module, "numeric ids", &bnids) != 0) {
		ERR("%s: 'numeric ids' invalid", module);
		return 0;
	}

	if (bnids)
		opts->numeric_ids = NIDS_STEALTH;

	return 1;
}

int
daemon_apply_ignoreopts(struct sess *sess, const char *module, struct opts *opts)
{
	struct daemon_role *role;

	role = (void *)sess->role;
	if (!opts->ignore_errors &&
	    cfg_param_bool(role->dcfg, module, "ignore errors",
	    &opts->ignore_errors) != 0) {
		daemon_client_error(sess, "%s: 'ignore errors' invalid");
		return 0;
	}

	if (cfg_param_bool(role->dcfg, module, "ignore nonreadable",
	    &opts->ignore_nonreadable) != 0) {
		daemon_client_error(sess, "%s: 'ignore nonreadable' invalid");
		return 0;
	}

	return 1;
}

static int
daemon_chuser_resolve_name(const char *name, bool is_gid, id_t *oid)
{
	struct passwd *pwd;
	struct group *grp;
	char *endp;
	long long rid;

	if (is_gid) {
		grp = getgrnam(name);
		if (grp != NULL) {
			*oid = grp->gr_gid;
			return 1;
		}
	} else {
		pwd = getpwnam(name);
		if (pwd != NULL) {
			*oid = pwd->pw_uid;
			return 1;
		}
	}

	errno = 0;
	rid = strtoll(name, &endp, 10);
	if (errno != 0 || *endp != '\0')
		return 0;

	if (rid < INT_MIN || rid > INT_MAX)
		return 0;

	*oid = (id_t)rid;
	return 1;
}

int
daemon_chuser_setup(struct sess *sess, const char *module)
{
	struct daemon_role *role;
	const char *gidstr, *uidstr;
	int rc;

	role = (void *)sess->role;
	role->do_setid = geteuid() == 0;

	/* If we aren't root, nothing to do. */
	if (!role->do_setid)
		return 1;

	rc = cfg_param_str(role->dcfg, module, "uid", &uidstr);
	assert(rc == 0);
	if (!daemon_chuser_resolve_name(uidstr, false, &role->uid)) {
		daemon_client_error(sess, "%s: uid '%s' invalid",
		    module, uidstr);
		return 0;
	}

	rc = cfg_param_str(role->dcfg, module, "gid", &gidstr);
	assert(rc == 0);
	if (!daemon_chuser_resolve_name(gidstr, true, &role->gid)) {
		daemon_client_error(sess, "%s: gid '%s' invalid",
		    module, gidstr);
		return 0;
	}

	return 1;
}

int
daemon_chuser(struct sess *sess, const char *module)
{
	struct daemon_role *role;

	role = (void *)sess->role;

	if (!role->do_setid)
		return 1;
	if (role->gid != 0 && setgid((gid_t)role->gid) == -1) {
		daemon_client_error(sess, "%s: setgid to '%d' failed",
		    module, role->gid);
		return 0;
	}
	if (role->uid != 0 && setuid((uid_t)role->uid) == -1) {
		daemon_client_error(sess, "%s: setuid to '%d' failed",
		    module, role->uid);
		return 0;
	}

	return 1;
}

void
daemon_client_error(struct sess *sess, const char *fmt, ...)
{
	struct daemon_role *role;
	char *msg;
	va_list ap;
	int msgsz;

	role = (void *)sess->role;

	va_start(ap, fmt);
	if ((msgsz = vasprintf(&msg, fmt, ap)) != -1) {
		if (!sess->mplex_writes) {
			if (!io_write_buf(sess, role->client, "@ERROR ",
			    sizeof("@ERROR ") - 1) ||
			    !io_write_line(sess, role->client, msg)) {
				ERR("io_write");
			}
		} else {
			if (!io_write_buf_tagged(sess, role->client, msg,
			    msgsz, IT_ERROR_XFER)) {
				ERR("io_write");
			} else if(!io_write_buf_tagged(sess, role->client, "\n",
			    1, IT_ERROR_XFER)) {
				ERR("io_write");
			}
		}
		free(msg);
	}
	va_end(ap);

	/*
	 * We may want to log this to the log file as well, but for now we'll
	 * settle on just making the client aware.
	 */
}

static void
daemon_configure_filter_type(struct daemon_role *role, const char *module,
    const char *fparam, enum rule_type type, bool is_file)
{
	const char *filter;
	int rc;

	if (!cfg_has_param(role->dcfg, module, fparam))
		return;

	rc = cfg_param_str(role->dcfg, module, fparam, &filter);
	assert(rc == 0);

	if (is_file)
		parse_file(filter, type, '\n');
	else
		parse_rule_words(filter, type, '\n');
}

int
daemon_configure_filters(struct sess *sess, const char *module)
{
	struct daemon_role *role;

	role = (void *)sess->role;

	daemon_configure_filter_type(role, module, "filter", RULE_NONE, false);
	daemon_configure_filter_type(role, module, "include from", RULE_INCLUDE,
	    true);
	daemon_configure_filter_type(role, module, "include", RULE_INCLUDE,
	    false);
	daemon_configure_filter_type(role, module, "exclude from", RULE_EXCLUDE,
	    true);
	daemon_configure_filter_type(role, module, "exclude", RULE_EXCLUDE,
	    false);

	return 1;
}

static long
parse_addr(sa_family_t family, const char *strmask, struct sockaddr *maskaddr)
{
	void *addr;

	if (family == AF_INET) {
		struct sockaddr_in *sin = (void *)maskaddr;

		addr = &sin->sin_addr;
	} else {
		struct sockaddr_in6 *sin6 = (void *)maskaddr;

		addr = &sin6->sin6_addr;
	}

	if (inet_pton(family, strmask, addr) != 1)
		return 0;

	maskaddr->sa_family = family;
	return 1;
}

static long
parse_mask(sa_family_t family, const char *strmask, struct sockaddr *maskaddr)
{
	char *endp;
	unsigned long mask;

	errno = 0;
	mask = strtoul(strmask, &endp, 10);
	if (errno == 0 && *endp == '\0') {
		uint8_t *addr;
		size_t addrsz, nbytes;
		int rem;

		/* No sanity checking at all. */
		if (family == AF_INET) {
			struct sockaddr_in *sin = (void *)maskaddr;

			addr = (uint8_t *)&sin->sin_addr;
			addrsz = sizeof(sin->sin_addr);
		} else {
			struct sockaddr_in6 *sin6 = (void *)maskaddr;

			addr = (uint8_t *)&sin6->sin6_addr;
			addrsz = sizeof(sin6->sin6_addr);
		}

		memset(addr, 0, addrsz);

		/* Invalid...  */
		if (mask > (addrsz << 3))
			return 0;

		/*
		 * We can memset up until the last byte of the mask, then
		 * set the remainder manually.
		 */
		nbytes = mask >> 3;

		/* Recalculate our last byte's worth, if any. */
		rem = mask & 0x07;	/* Truncated bits */
		mask = ~((1 << (8 - rem)) - 1);

		memset(addr, 0xff, nbytes);
		if (rem != 0)
			addr[nbytes] |= mask;

		return 1;
	}

	/* Must be an address mask... */
	if (parse_addr(family, strmask, maskaddr))
		return 1;
	return 0;
}

static int
masked_match(char *host, struct sockaddr *addr)
{
	struct sockaddr_storage hostaddr, maskaddr;
	uint8_t *laddr, *maddr, *raddr;
	char *strmask;
	size_t addrsz;
	sa_family_t family;

	family = addr->sa_family;
	strmask = strrchr(host, '/');
	assert(strmask != NULL);

	if (!parse_mask(family, strmask + 1, (struct sockaddr *)&maskaddr))
		return 0;

	*strmask = '\0';

	if (!parse_addr(family, host, (struct sockaddr *)&hostaddr))
		return 0;

	if (family == AF_INET) {
		struct sockaddr_in *left, *right, *mask;

		left = (struct sockaddr_in *)&hostaddr;
		right = (struct sockaddr_in *)addr;
		mask = (struct sockaddr_in *)&maskaddr;

		laddr = (uint8_t *)&left->sin_addr;
		raddr = (uint8_t *)&right->sin_addr;
		maddr = (uint8_t *)&mask->sin_addr;
		addrsz = sizeof(left->sin_addr);
	} else {
		struct sockaddr_in6 *left, *right, *mask;

		left = (struct sockaddr_in6 *)&hostaddr;
		right = (struct sockaddr_in6 *)addr;
		mask = (struct sockaddr_in6 *)&maskaddr;

		laddr = (uint8_t *)&left->sin6_addr;
		raddr = (uint8_t *)&right->sin6_addr;
		maddr = (uint8_t *)&mask->sin6_addr;
		addrsz = sizeof(left->sin6_addr);
	}

	/* Finally, compare the two. */
	for (size_t i = 0; i < addrsz; i++) {
		if (((laddr[i] ^ raddr[i]) & maddr[i]) != 0)
			return 0;
	}

	return 1;
}

static int
daemon_connection_matches_one(const struct sess *sess, char *host)
{
	const struct daemon_role *role;
	const char *addr, *masked;

	role = (void *)sess->role;
	if (role->client_host[0] != '\0' &&
	    rmatch(host, role->client_host, 0) == 0)
		return 1;

	masked = strrchr(host, '/');
	addr = &role->client_addr[0];
	if (masked == NULL)
		return strcmp(host, addr) == 0;

	return masked_match(host, role->client_sa);
}

static int
daemon_connection_matches(struct sess *sess, const char *hostlistp, bool *match,
    int *total)
{
	char *host, *hostlist;
	int cnt;
	bool matched;

	hostlist = strdup(hostlistp);
	if (hostlist == NULL) {
		daemon_client_error(sess, "out of memory");
		return -1;
	}

	cnt = 0;
	matched = false;
	while ((host = strsep(&hostlist, ", \t")) != NULL) {
		if (*host == '\0')
			continue;

		cnt++;

		/* Check host */
		if (daemon_connection_matches_one(sess, host)) {
			matched = true;
			break;
		}
	}

	*total = cnt;
	*match = matched;

	free(hostlist);
	return 0;
}

/* Check 'hosts allow' and 'hosts deny' */
int
daemon_connection_allowed(struct sess *sess, const char *module)
{
	struct daemon_role *role;
	const char *hostlist;
	int allowcnt, denycnt, rc;
	bool has_deny, matched;

	role = (void *)sess->role;

	allowcnt = denycnt = 0;
	has_deny = cfg_has_param(role->dcfg, module, "hosts deny");
	if (cfg_has_param(role->dcfg, module, "hosts allow")) {
		rc = cfg_param_str(role->dcfg, module, "hosts allow",
		    &hostlist);
		assert(rc == 0);

		/* Fail safe, don't allow if we failed to parse. */
		if (daemon_connection_matches(sess, hostlist,
		    &matched, &allowcnt) == -1) {
			daemon_client_error(sess, "failed to process allow host list");
			return 0;
		}

		if (allowcnt > 0) {
			if (matched)
				return 1;

			if (!has_deny) {
				daemon_client_error(sess,
				    "access denied by allow policy from %s [%s]",
				    role->client_host, role->client_addr);
				return 0;
			}
		}
	}

	if (has_deny) {
		rc = cfg_param_str(role->dcfg, module, "hosts deny",
		    &hostlist);
		assert(rc == 0);

		/* Fail safe, don't allow if we failed to parse. */
		if (daemon_connection_matches(sess, hostlist,
		    &matched, &denycnt) == -1) {
			daemon_client_error(sess, "failed to process deny host list");
			return 0;
		}

		if (denycnt > 0) {
			if (matched) {
				daemon_client_error(sess,
				    "access denied by deny policy from %s [%s]",
				    role->client_host, role->client_addr);
				return 0;
			}
		} else if (allowcnt > 0) {
			/*
			 * We had an allow list and we thought we had a deny
			 * list, but we parsed the list only to discover it was
			 * actually empty.  Deny the connection, since they were
			 * not allowed by the allow list.
			 */
			daemon_client_error(sess,
			    "access denied by allow policy from %s [%s]",
			    role->client_host, role->client_addr);
			return 0;
		}
	}

	/* Default policy is to accept all. */
	return 1;
}

/*
 * Return value of this one is reversed, as we're describing whether the
 * connection is being limited from happening or not.
 */
int
daemon_connection_limited(struct sess *sess, const char *module)
{
	struct daemon_role *role;
	const char *lockf;
	int max, rc;

	role = (void *)sess->role;
	if (cfg_param_int(role->dcfg, module, "max connections", &max) != 0) {
		ERRX("%s: 'max connections' invalid", module);
		return 1;
	}

	if (max < 0) {
		/* Disabled */
		daemon_client_error(sess,
		    "module '%s' is currently disabled", module);
		return 1;
	} else if (max == 0) {
		/* Unlimited allowed */
		return 0;
	}

	rc = cfg_param_str(role->dcfg, module, "lock file", &lockf);
	assert(rc == 0);

	if (*lockf == '\0') {
		ERR("%s: 'lock file' is empty with 'max connections' in place", module);
		return 1;
	}

	return !daemon_rangelock(sess, module, lockf, max);
}

/*
 * Done in the child for both pre-xfer and post-xfer commands
 */
static void
daemon_do_execcmds_common_env(struct daemon_role *role, const char *module)
{

	setenv("RSYNC_MODULE_NAME", module, 1);
	setenv("RSYNC_MODULE_PATH", role->module_path, 1);
	setenv("RSYNC_HOST_ADDR", role->client_addr, 1);
	setenv("RSYNC_HOST_NAME", role->client_host, 1);
	setenv("RSYNC_USER_NAME",
	    (role->auth_user != NULL ? role->auth_user : ""), 1);
}

/*
 * The reference rsync seems to ignore, e.g., out of memory errors when trying
 * to construct the environment, so we'll do the same.
 */
static void
daemon_do_execcmds_num_env(const char *name, unsigned long val)
{
	char fmtbuf[32];
	int ret;

	ret = snprintf(fmtbuf, sizeof(fmtbuf), "%ld", val);
	if (ret < 0 || ret >= (int)sizeof(fmtbuf))
		return;	/* Overflow or error */

	setenv(name, fmtbuf, 1);
}

static int
daemon_await_prexfer_env_read(int infd, void *bufp, size_t bufsz)
{
	uint8_t *buf = bufp;
	size_t offset;
	ssize_t readsz;

	offset = 0;
	while (bufsz != 0) {
		readsz = read(infd, &buf[offset], bufsz);
		if (readsz <= 0) {
			if (readsz == -1 && (errno == EINTR || errno == EAGAIN))
				continue;

			return 0;
		}

		offset += readsz;
		bufsz -= readsz;
	}

	return 1;
}

static uint8_t *
daemon_await_prexfer_env(struct daemon_role *role, int infd, size_t *obufsz)
{
	uint8_t *penv;
	size_t penvsz;
	int flags;

	/* Blocking would be nice, but it's not mandatory. */
	flags = fcntl(infd, F_GETFL);
	if (flags != -1 && (flags & O_NONBLOCK) != 0) {
		flags &= ~O_NONBLOCK;
		(void)fcntl(infd, F_SETFL, flags);
	}

	if (!daemon_await_prexfer_env_read(infd, &penvsz, sizeof(penvsz)))
		_exit(1);

	penv = malloc(penvsz);
	if (penv == NULL)
		_exit(1);

	if (!daemon_await_prexfer_env_read(infd, penv, penvsz))
		_exit(1);

	close(infd);
	infd = -1;

	*obufsz = penvsz;
	return penv;
}

static int
daemon_do_execcmds_pre(struct sess *sess, const char *module, const char *cmd)
{
	struct daemon_role *role;
	uint8_t *penv;
	size_t penvsz;
	int fd[2], status;
	pid_t pid, ppid;

	role = (void *)sess->role;
	ppid = getpid();
	if (pipe(&fd[0]) == -1)
		goto stagefail;
	pid = fork();
	if (pid == -1)
		goto stagefail;

	if (pid != 0) {
		/* Parent returns, carries on with startup */
		close(fd[0]);

		role->prexfer_pipe = fd[1];
		role->prexfer_pid = pid;
		return 1;
	}

	/* Child awaits environmental information. */
	close(fd[1]);

	/*
	 * The reference rsync seems to put this in both the parent and child
	 * environments if the pre-xfer exec command is set, but it's not clear
	 * why.  We'll keep the transfer process's environment clean to match
	 * the post-xfer exec handling until we come up with a compelling reason
	 * to do otherwise (e.g., config vars that might expect RSYNC_PID to be
	 * populated -- this version of rsync deos not yet do var expansion).
	 */
	daemon_do_execcmds_num_env("RSYNC_PID", ppid);
	daemon_do_execcmds_common_env(role, module);

	penv = daemon_await_prexfer_env(role, fd[0], &penvsz);
	if (penv != NULL) {
		char envname[sizeof("RSYNC_ARG") + 6];
		const uint8_t *args, *endp, *narg;
		int ret;

		setenv("RSYNC_ARG0", "rsyncd", 1);

		narg = args = penv;

		for (size_t i = 0; penvsz != 0; i++) {
			if (i > 512)
				__builtin_trap();

			/*
			 * This should always be NUL-terminated, so we shouldn't
			 * have anything remaining if we hit endp == NULL.
			 */
			endp = memchr(narg, '\0', penvsz);
			if (endp == NULL)
				break;

			/*
			 * The first one is actually the request.  We took out
			 * RSYNC_ARG0 above with "rsyncd", but we make up for
			 * it here by stuffing it into RSYNC_REQUEST.
			 */
			if (i == 0) {
				ret = (int)strlcpy(envname, "RSYNC_REQUEST",
				    sizeof(envname));
			} else {
				/* Add it */
				ret = snprintf(envname, sizeof(envname),
				    "RSYNC_ARG%ld", i);
				if (ret == -1)
					break;
			}

			/* It should have been wide enough... */
			assert(ret < (int)sizeof(envname));
			(void)setenv(envname, (const char *)narg, 1);

			endp++;

			penvsz -= endp - narg;
			narg = endp;
		}

		free(penv);
	}

	/* Environment is set, let's exec it! */
	close(STDIN_FILENO);
	close(STDOUT_FILENO);

	status = system(cmd);
	if (WIFEXITED(status))
		status = WEXITSTATUS(status);
	else
		status = 1;

	_exit(status);
stagefail:
	daemon_client_error(sess, "%s: failed to stage pre-xfer process",
	    module);
	return 0;
}

static int
daemon_do_execcmds_post(struct sess *sess, const char *module, const char *cmd)
{
	pid_t pid, ret;
	int status;

	pid = fork();
	if (pid == -1) {
		daemon_client_error(sess, "%s: failed to fork for post-xfer");
		return 0;
	}

	/* Child returns */
	if (pid == 0)
		return 1;

	/* Parent waits */
	daemon_do_execcmds_common_env((void *)sess->role, module);
	daemon_do_execcmds_num_env("RSYNC_PID", pid);

	while ((ret = waitpid(pid, &status, 0)) == -1 &&
	    errno == EINTR) {
		continue;	/* Spin */
	}

	if (ret != pid)
		status = -1;

	daemon_do_execcmds_num_env("RSYNC_RAW_STATUS", status);

	if (WIFEXITED(status))
		status = WEXITSTATUS(status);
	else
		status = -1;

	daemon_do_execcmds_num_env("RSYNC_EXIT_STATUS", status);

	/* Execute the command, but propagate the child status either way. */
	system(cmd);
	_exit(status);
}

int
daemon_do_execcmds(struct sess *sess, const char *module)
{
	struct daemon_role *role;
	const char *cmd;
	int rc;

	role = (void *)sess->role;
	if (!cfg_has_param(role->dcfg, module, "pre-xfer exec") &&
	    !cfg_has_param(role->dcfg, module, "post-xfer exec"))
		return 1;

	/*
	 * We fire up the post-xfer exec command first because it will run in
	 * our current process and fork off a process to actually handle the
	 * transfer to reliably grab the final status.  Thus, to be able to
	 * provide a consistent RSYNC_PID to both commands, we need this one to
	 * fork first -- the other one will fork and handle its command in the
	 * child instead.
	 */
	if (cfg_has_param(role->dcfg, module, "post-xfer exec")) {
		rc = cfg_param_str(role->dcfg, module, "post-xfer exec", &cmd);
		assert(rc == 0);

		if (*cmd != '\0' && !daemon_do_execcmds_post(sess, module, cmd))
			return 0;
	}

	if (cfg_has_param(role->dcfg, module, "pre-xfer exec")) {
		rc = cfg_param_str(role->dcfg, module, "pre-xfer exec", &cmd);
		assert(rc == 0);

		if (*cmd != '\0' && !daemon_do_execcmds_pre(sess, module, cmd))
			return 0;
	}

	return 1;
}

static int
daemon_finish_prexfer_write(int outfd, const void *bufp, size_t bufsz)
{
	const uint8_t *buf = bufp;
	size_t offset;
	ssize_t wsz;

	offset = 0;
	while (bufsz != 0) {
		wsz = write(outfd, &buf[offset], bufsz);
		if (wsz <= 0) {
			if (wsz == -1 && errno == EINTR)
				continue;
			else if (wsz == -1)
				ERR("write");
			return 0;
		}

		offset += wsz;
		bufsz -= wsz;
	}

	return 1;
}

int
daemon_finish_prexfer(struct sess *sess, const char *req, const char *args,
    size_t argsz)
{
	struct daemon_role *role;
	size_t reqsz, totalsz;
	pid_t child, ret;
	int fd, status;
	bool error;

	role = (void *)sess->role;

	child = role->prexfer_pid;
	fd = role->prexfer_pipe;
	assert(fd != -1 && child > 0);

	if (req != NULL) {
		error = true;
		reqsz = strlen(req) + 1;

		/*
		 * We write totalsz, which includes the following:
		 *   - uint8_t[strlen(req) + 1]: request, nul terminated
		 *   - uint8_t[argsz]: nul terminated args
		 *
		 * If at any point we fail to write, we'll just close the other
		 * pipe so the other side terminates and we'll cancel the
		 * transfer based on the non-zero exit code.
		 */
		totalsz = reqsz + argsz;

		if (!daemon_finish_prexfer_write(fd, &totalsz, sizeof(totalsz)))
			goto done;
		if (!daemon_finish_prexfer_write(fd, req, reqsz))
			goto done;
		if (!daemon_finish_prexfer_write(fd, args, argsz))
			goto done;
	}

	error = false;
done:
	/*
	 * Whether we had an error or not, close out the write side of the pipe
	 * to force the other side out.
	 */
	close(fd);

	role->prexfer_pipe = -1;

	/* Finally, wait for it. */
	while ((ret = waitpid(child, &status, 0)) != child) {
		if (ret == -1) {
			if (errno == EINTR)
				continue;

			daemon_client_error(sess,
			    "error waiting for pre-exec xfer child");
			return 0;
		}
	}

	if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
		daemon_client_error(sess,
		    "pre-xfer exec command denies transfer");
		error = true;
	} else if (!WIFEXITED(status)) {
		daemon_client_error(sess, "pre-xfer exec command failed");
		error = true;
	}

	role->prexfer_pid = 0;

	return error ? 0 : 1;
}

int
daemon_fill_hostinfo(struct sess *sess, const char *module,
    const struct sockaddr *addr, socklen_t slen)
{
	struct daemon_role *role;
	int rc;

	role = (void *)sess->role;

	/*
	 * Nothing left to do here, nothing configured that needs reverse dns.
	 */
	if (!cfg_has_param(role->dcfg, module, "hosts allow") &&
	    !cfg_has_param(role->dcfg, module, "hosts deny") &&
	    !cfg_has_param(role->dcfg, module, "pre-xfer exec") &&
	    !cfg_has_param(role->dcfg, module, "post-xfer exec"))
		return 1;

	if ((rc = getnameinfo(addr, slen, &role->client_host[0],
	    sizeof(role->client_host), NULL, 0, 0)) != 0) {
		daemon_client_error(sess, "%s: reverse dns lookup failed: %s",
		    module, gai_strerror(rc));
		return 0;
	}

	return 1;
}

static int
daemon_symlink_munge_filter(const char *link, char **outlink, enum fmode mode)
{

	if (mode == FARGS_SENDER) {
		/* Sender: de-munge */
		if (strncmp(link, RSYNCD_MUNGE_PREFIX,
		    sizeof(RSYNCD_MUNGE_PREFIX) - 1) != 0) {
			/* Nothing to de-munge, just return. */
			*outlink = NULL;
			return 0;
		}

		*outlink = strdup(link + sizeof(RSYNCD_MUNGE_PREFIX) - 1);
		if (*outlink == NULL) {
			ERR("strdup");
			return -1;
		}
	} else {
		/*
		 * Receiver: do the munge so that we can't be tricked into
		 * following it.
		 */
		if (asprintf(outlink, "%s%s", RSYNCD_MUNGE_PREFIX, link) == -1) {
			ERR("asprintf");
			return -1;
		}
	}

	return 0;
}

static int
daemon_symlink_sanitize_filter(const char *link, char **outlink,
    enum fmode mode)
{

	/*
	 * We have nothing to do if we're sending out; the damage was likely
	 * done when we received the link, and we can't exactly reverse it.
	 */
	if (mode == FARGS_SENDER) {
		*outlink = NULL;
		return (0);
	}

	/*
	 * For the receiver, we need to make the link safe by making it relative
	 * and stripping any bits that might make us jump outside of the module
	 * path.
	 */
	*outlink = make_safe_link(link);
	if (*outlink == NULL) {
		ERRX1("make_safe_link");
		return (ENOMEM);
	}

	return 0;
}

int
daemon_install_symlink_filter(struct sess *sess, const char *module,
    int chrooted)
{
	struct daemon_role *role;
	int munge;

	/* Munging default is inverse of chrooted. */
	munge = !chrooted;

	/*
	 * If we're not chrooted, we must install one of two filters: either
	 * munge symlinks is true and we need to install the munger filter, or
	 * it's false and we need to install the safety belt that strips leading
	 * '/' and sanitizes away anything that might make us escape the module
	 * path.
	 *
	 * If unspecified, "munge symlinks" defaults to false.
	 */
	role = (void *)sess->role;
	if (cfg_has_param(role->dcfg, module, "munge symlinks") &&
	    cfg_param_bool(role->dcfg, module, "munge symlinks", &munge) != 0) {
		daemon_client_error(sess, "%s: 'munge symlinks' invalid",
		    module);
		return 0;
	}

	/*
	 * If we're chrooted and not munging, we can just bail out now without
	 * installing a failure.  The user could still configure us for munging
	 * even in a chroot and we should honor it, and if we're not chrooted
	 * then we *have* to install some filter.
	 */
	if (!munge && chrooted)
		return 1;

	if (munge) {
		struct stat st;

		if (stat(RSYNCD_MUNGE_PREFIX, &st) == -1) {
			if (errno != ENOENT) {
				daemon_client_error(sess,
				    "%s: failed to stat munge dir", module);
				return 0;
			}
		} else if (S_ISDIR(st.st_mode)) {
			daemon_client_error(sess,
			    "%s: security violation: munger failure", module);
			return 0;
		}

		sess->symlink_filter = daemon_symlink_munge_filter;
	} else {
		sess->symlink_filter = daemon_symlink_sanitize_filter;
	}

	return 1;
}

int
daemon_limit_verbosity(struct sess *sess, const char *module)
{
	struct daemon_role *role;
	int max;

	role = (void *)sess->role;
	if (cfg_param_int(role->dcfg, module, "max verbosity", &max) != 0) {
		ERRX("%s: 'max verbosity' invalid", module);
		return 0;
	}

	verbose = MINIMUM(verbose, max);
	return 1;
}

void
daemon_normalize_path(const char *module, size_t modlen, char *path)
{
	size_t pathlen;

	/* module cannot be empty */
	if (modlen == 0)
		modlen = strlen(module);

	/* Search for <module>[/...] */
	if (strncmp(path, module, modlen) != 0 ||
	    (path[modlen] != '/' && path[modlen] != '\0'))
		return;

	/*
	 * If we just had <module> and not <module>/..., then we can
	 * just truncate it entirely.
	 */
	if (path[modlen] == '\0') {
		path[0] = '\0';
		return;
	}

	/*
	 * Strip the leading <module>/ prefix.  Any unprefixed paths are
	 * assumed to be relative to the module root anyways.
	 */
	pathlen = strlen(&path[modlen + 1]);
	memmove(&path[0], &path[modlen + 1],  pathlen + 1);
}

void
daemon_normalize_paths(const char *module, int argc, char *argv[])
{
	size_t modlen;

	modlen = strlen(module);
	for (int i = 0; i < argc; i++)
		daemon_normalize_path(module, modlen, argv[i]);
}

int
daemon_open_logfile(const char *logfile, bool printerr)
{

	if (logfile != NULL && *logfile == '\0')
		logfile = NULL;
	if (logfile != NULL) {
		FILE *fp;

		fp = fopen(logfile, "a");
		if (fp == NULL) {
			if (printerr)
				ERR("%s: fopen", logfile);
			return 0;
		}

		/*
		 * Logging infrastructure will take the FILE and close it if we
		 * switch away later.
		 */
		rsync_set_logfile(fp);
	} else {
		rsync_set_logfile(NULL);
	}

	return 1;
}

int
daemon_operation_allowed(struct sess *sess, const struct opts *opts,
    const char *module, int user_read_only)
{
	struct daemon_role *role;
	int deny;

	role = (void *)sess->role;
	if (!opts->sender) {
		/* Client wants to send files, check read only. */
		if (user_read_only != -1) {
			deny = user_read_only;
		} else if (cfg_param_bool(role->dcfg, module, "read only",
		    &deny) != 0) {
			ERRX("%s: 'read only' invalid", module);
			return 0;
		}
	} else {
		/* Client wants to receive files, check write only. */
		if (cfg_param_bool(role->dcfg, module, "write only",
		    &deny) != 0) {
			ERRX("%s: 'write only' invalid", module);
			return 0;
		}
	}

	if (deny) {
		daemon_client_error(sess, "module '%s' is %s-protected",
		    module, opts->sender ? "read" : "write");
	}

	return !deny;
}

static int
daemon_add_short_refuse(struct sess *sess, char **shopts,
    size_t *shoptlen, size_t *shoptsz, char shopt)
{
	char *chkopts = *shopts;

	/* Allocate on demand. */
	if (chkopts == NULL) {
		*shoptsz = strlen(rsync_shopts);
		chkopts = calloc(1, *shoptsz + 1);
		if (chkopts == NULL) {
			daemon_client_error(sess, "out of memory");
			return 0;
		}

		*shopts = chkopts;
	}

	if (strchr(chkopts, shopt) != NULL)
		return 1;

	assert(*shoptlen < *shoptsz);
	chkopts[(*shoptlen)++] = shopt;

	return 1;
}

#define	REFUSE_LONG_ALLOC_BATCH		(8)

static int
daemon_add_long_refuse(struct sess *sess, const struct option ***lopts,
    size_t *optlen, size_t *optsz, const struct option *opt)
{
	const struct option **chkopts = *lopts;

	if (*optlen == *optsz) {
		*optsz += REFUSE_LONG_ALLOC_BATCH;
		chkopts = reallocarray(*lopts, *optsz, sizeof(*chkopts));
		if (chkopts == NULL) {
			daemon_client_error(sess, "out of memory");
			return 0;
		}

		*lopts = chkopts;
	}

	for (size_t i = 0; i < *optlen; i++) {
		if (chkopts[i] == opt)
			return 1;
	}

	assert(*optlen < *optsz);
	chkopts[(*optlen)++] = opt;

	return 1;
}

static int
daemon_add_long_refuse_name(struct sess *sess, const struct option ***lopts,
    size_t *optlen, size_t *optsz, const char *name)
{
	const struct option *chkopt;

	for (chkopt = &rsync_lopts[0]; chkopt->name != NULL; chkopt++) {
		if (strcmp(chkopt->name, name) != 0)
			continue;

		return (daemon_add_long_refuse(sess, lopts, optlen, optsz,
		    chkopt));
	}

	return 1;
}

static int
daemon_can_refuse_wildcard(const char *option, char shopt)
{
	static const char no_wildcard_shopts[] = { 'e', 'n', '0' };
	static const char *no_wildcard_lopts[] = {
	    "server", "sender", "rsh", "out-format", "log-format",
	    "dry-run", "from0"
	};

	assert(option != NULL || shopt != 0);
	assert((option != NULL) ^ (shopt != 0));

	if (shopt != 0) {
		for (size_t i = 0; i < nitems(no_wildcard_shopts); i++) {
			if (shopt == no_wildcard_shopts[i])
				return 0;
		}

		return 1;
	}

	for (size_t i = 0; i < nitems(no_wildcard_lopts); i++) {
		if (strcmp(option, no_wildcard_lopts[i]) == 0)
			return 0;
	}

	return 1;
}

int
daemon_parse_refuse(struct sess *sess, const char *module)
{
	struct daemon_role *role;
	const char *refuse_cfg;
	const struct option *chkopt, **lopts;
	char *refused, *shopts, *token;
	size_t loptlen, loptsz, shoptlen, shoptsz;
	int rc;
	bool explicit_archive = false;

	role = (void *)sess->role;
	if (!cfg_has_param(role->dcfg, module, "refuse options"))
		return 1;

	rc = cfg_param_str(role->dcfg, module, "refuse options", &refuse_cfg);
	assert(rc == 0);

	refused = strdup(refuse_cfg);
	if (refused == NULL) {
		daemon_client_error(sess, "out of memory");
		return 0;
	}

	loptlen = loptsz = 0;
	shoptlen = shoptsz = 0;
	lopts = NULL;
	shopts = NULL;
	while ((token = strsep(&refused, " ")) != NULL) {
		char shopt = 0;
		bool wildcard;

		/* Skip empty fields */
		if (token[0] == '\0')
			continue;

#define	SHORT_OPTION(token)	(isprint(token[0]) && \
    (token[1] == '\0' || (token[1] == '*' && token[2] == '\0')))

		if (SHORT_OPTION(token) && (token[1] != '*' ||
		    daemon_can_refuse_wildcard(NULL, token[0]))) {
			shopt = token[0];

			if (!daemon_add_short_refuse(sess, &shopts, &shoptlen,
			    &shoptsz, shopt)) {
				free(refused);
				free(shopts);
				free(lopts);

				return 0;
			}

			if (shopt == 'a' && token[1] == '\0')
				explicit_archive = true;

			/*
			 * We won't bother with the long option matching if it
			 * has a short option, just to save a little bit of
			 * memory and time.
			 */
			if (token[1] == '\0')
				continue;
		}

		wildcard = strchr(token, '*') != NULL;

		/* Check for explicit matches first */
		for (chkopt = &rsync_lopts[0]; chkopt->name != NULL;
		    chkopt++) {
			if (rmatch(token, chkopt->name, 0) != 0)
				continue;

			/*
			 * See if we can avoid consuming a lopt spot.
			 */
			if (chkopt->flag == NULL && isprint(chkopt->val)) {
				if (wildcard &&
				    !daemon_can_refuse_wildcard(NULL,
				    chkopt->val))
					continue;
				if (chkopt->val == 'a' &&
				    strcmp(token, "archive") == 0)
					explicit_archive = true;
				if (!daemon_add_short_refuse(sess, &shopts,
				    &shoptlen, &shoptsz, chkopt->val)) {
					free(refused);
					free(shopts);
					free(lopts);

					return 0;
				}

				continue;
			} else if (wildcard &&
			    !daemon_can_refuse_wildcard(chkopt->name, 0))
				continue;

			if (!daemon_add_long_refuse(sess, &lopts, &loptlen,
			    &loptsz, chkopt)) {
				free(refused);
				free(shopts);
				free(lopts);

				return 0;
			}
		}

		if (rmatch(token, "delete", 0) == 0) {
			if (!daemon_add_long_refuse_name(sess, &lopts, &loptlen,
			    &loptsz, "remove-sent-files") ||
			    !daemon_add_long_refuse_name(sess, &lopts, &loptlen,
			    &loptsz, "remove-source-files")) {
				free(refused);
				free(shopts);
				free(lopts);

				return 0;
			}
		}
	}

	if (shopts != NULL) {
		/*
		 * If we excluded -a explicitly rather than by wildcard, then
		 * we refuse all of the implied options as well.
		 */
		if (explicit_archive) {
			int rc;

			for (const char *shoptchk = "rlptgoD"; *shoptchk != '\0';
			    shoptchk++) {
				rc = daemon_add_short_refuse(sess, &shopts,
				    &shoptlen, &shoptsz, *shoptchk);

				assert(rc != 0);
			}
		} else if (strchr(shopts, 'a') == NULL) {
			/*
			 * If we refused any part of -a, then we need to refuse
			 * 'a' as well.
			 */
			for (const char *shoptchk = "rlptgoD"; *shoptchk != '\0';
			    shoptchk++) {
				rc = daemon_add_short_refuse(sess, &shopts,
				    &shoptlen, &shoptsz,  'a');

				assert(rc != 0);
			}
		}

		if (strchr(shopts, 'D') != NULL) {
			if (!daemon_add_long_refuse_name(sess, &lopts, &loptlen,
			    &loptsz, "devices") ||
			    !daemon_add_long_refuse_name(sess, &lopts, &loptlen,
			    &loptsz, "specials")) {
				return 0;
			}
		}

		if (strchr(shopts, 'P') != NULL) {
			if (!daemon_add_long_refuse_name(sess, &lopts, &loptlen,
			    &loptsz, "partial") ||
			    !daemon_add_long_refuse_name(sess, &lopts, &loptlen,
			    &loptsz, "progress")) {
				return 0;
			}
		}
	}

	role->refused.refused_shopts = shopts;
	role->refused.refused_lopts = lopts;
	role->refused.refused_loptsz = loptlen;
	return 1;
}

static int
daemon_rangelock(struct sess *sess, const char *module, const char *lockf,
    int max)
{
	struct flock rlock = {
		.l_start = 0,
		.l_len = 0,
		.l_type = F_WRLCK,
		.l_whence = SEEK_SET,
	};
	struct daemon_role *role;
	int fd, rc;

	role = (void *)sess->role;
	fd = open(lockf, O_WRONLY | O_CREAT, 0644);
	if (fd == -1) {
		daemon_client_error(sess, "%s: failed to open the lock file",
		    module);
		return 0;
	}

	/*
	 * We naturally can't guarantee a specific slot, so search the entire
	 * space until we find an open one.
	 */
	for (int i = 0; i < max; i++) {
		rlock.l_start = CONNLOCK_START(i);
		rlock.l_len = CONNLOCK_SIZE(i);

		rc = fcntl(fd, F_SETLK, &rlock);
		if (rc == -1 && errno != EAGAIN) {
			/*
			 * We won't alert the client on these, apparently.
			 */
			ERR("%s: lock fcntl", lockf);
			break;
		} else if (rc == -1) {
			continue;
		}

		/* Success! Stash the fd. */
		role->lockfd = fd;
		return 1;
	}

	daemon_client_error(sess, "%s: too many connections (%d max)", module,
	    max);
	close(fd);
	return 0;
}

int
daemon_setup_logfile(struct sess *sess, const char *module)
{
	struct daemon_role *role;
	const char *logfile;
	int rc;
	bool syslog = false;

	role = (void *)sess->role;
	logfile = NULL;
	if (cfg_has_param(role->dcfg, module, "log file")) {
		rc = cfg_param_str(role->dcfg, module, "log file", &logfile);
		assert(rc == 0);
	}

	if (logfile == NULL || *logfile == '\0')
		return 1;

	if (!daemon_open_logfile(logfile, false)) {
		/* Just fallback to syslog on error. */
		if (!daemon_open_logfile(NULL, false))
			return 0;

		syslog = true;
	}

	/* Setup syslog facility, if we ended up with syslog. */
	if (syslog) {
		const char *facility;

		rc = cfg_param_str(role->dcfg, module, "syslog facility",
		    &facility);
		assert(rc == 0);

		if (rsync_set_logfacility(facility) != 0) {
			ERRX1("%s: 'syslog facility' does not exist: %s",
			    module, facility);
			return 0;
		}
	}

	return 1;
}
