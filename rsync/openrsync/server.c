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

#include <assert.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if HAVE_ERR
# include <err.h>
#endif

#include "extern.h"

static int
fcntl_nonblock(int fd)
{
	int	 fl;

	if ((fl = fcntl(fd, F_GETFL, 0)) == -1)
		ERR("fcntl: F_GETFL");
	else if (fcntl(fd, F_SETFL, fl|O_NONBLOCK) == -1)
		ERR("fcntl: F_SETFL");
	else
		return 1;

	return 0;
}

/*
 * The server (remote) side of the system.
 * This parses the arguments given it by the remote shell then moves
 * into receiver or sender mode depending upon those arguments.
 * Returns exit code 0 on success, 1 on failure, 2 on failure with
 * incompatible protocols.
 */
int
rsync_server(struct cleanup_ctx *cleanup_ctx, const struct opts *opts,
    size_t argc, char *argv[])
{
	struct sess	 sess;
	int		 fdin = STDIN_FILENO,
			 fdout = STDOUT_FILENO, rc = 1;

#ifndef __APPLE__
	if (pledge("stdio unix rpath wpath cpath dpath fattr chown getpw unveil",
	    NULL) == -1)
		err(ERR_IPC, "pledge");
#endif

	memset(&sess, 0, sizeof(struct sess));
	sess.opts = opts;
	sess.mode = sess.opts->sender ? FARGS_SENDER : FARGS_RECEIVER;
	sess.wbatch_fd = -1;

	cleanup_set_session(cleanup_ctx, &sess);
	cleanup_release(cleanup_ctx);

	/* Begin by making descriptors non-blocking. */

	if (!fcntl_nonblock(fdin) ||
	    !fcntl_nonblock(fdout)) {
		ERRX1("fcntl_nonblock");
		goto out;
	}

	/* Standard rsync preamble, server side. */

	sess.lver = sess.protocol = sess.opts->protocol;
	if (opts->checksum_seed == 0) {
#if HAVE_ARC4RANDOM
		sess.seed = arc4random();
#else
		sess.seed = random();
#endif
	} else {
		sess.seed = opts->checksum_seed;
	}

	if (!io_read_int(&sess, fdin, &sess.rver)) {
		ERRX1("io_read_int");
		goto out;
	} else if (!io_write_int(&sess, fdout, sess.lver)) {
		ERRX1("io_write_int");
		goto out;
	} else if (!io_write_int(&sess, fdout, sess.seed)) {
		ERRX1("io_write_int");
		goto out;
	}

	if (sess.rver < RSYNC_PROTOCOL_MIN) {
		ERRX("remote protocol %d is older than our minimum supported "
		    "%d: exiting", sess.rver, RSYNC_PROTOCOL_MIN);
		rc = 2;
		goto out;
	}

	if (sess.rver < sess.lver) {
		sess.protocol = sess.rver;
	}

	LOG2("server detected client version %d, server version %d, "
	    "negotiated protocol version %d, seed %d",
	    sess.rver, sess.lver, sess.protocol, sess.seed);

	sess.mplex_writes = 1;

	assert(sess.opts->whole_file != -1);
	LOG2("Delta transmission %s for this transfer",
	    sess.opts->whole_file ? "disabled" : "enabled");

	for (int i = 0; argv[i] != NULL; i++)
		LOG2("exec[%d] = %s", i, argv[i]);

	if (sess.opts->sender) {
		LOG2("server starting sender");

		/*
		 * At this time, I always get a period as the first
		 * argument of the command line.
		 * Let's make it a requirement until I figure out when
		 * that differs.
		 * rsync [flags] "." <source> <...>
		 */

		if (argc == 0) {
			ERRX("must have arguments");
			goto out;
		} else if (strcmp(argv[0], ".")) {
			ERRX("first argument must be a standalone period");
			goto out;
		} else if (argc == 1) {
			/*
			 * rsync 2.x can send "" as the source, in which case
			 * an implied "." is intended and must be fabricated.
			 * rsync 3.x always sends the implied "."
			 * If we only have 1 argv, reuse it to avoid making a
			 * new allocation.
			 */
		} else {
			argv++;
			argc--;
		}

		if (sess.opts->remove_source)
			sess.mplex_reads = 1;
		if (!rsync_sender(&sess, fdin, fdout, argc, argv)) {
			ERRX1("rsync_sender");
			goto out;
		}
	} else {
		LOG2("server starting receiver");

		/*
		 * I don't understand why this calling convention
		 * exists, but we must adhere to it.
		 * rsync [flags] "." <destination>
		 * destination might be "" in which case a . is implied.
		 */

		if (argc == 0) {
			ERRX("must have arguments");
			goto out;
		} else if (strcmp(argv[0], ".")) {
			ERRX("first argument must be a standalone period");
			goto out;
		} else if (argc == 1) {
			/*
			 * rsync 2.x can send "" as the dest, in which case
			 * an implied "." is intended and must be fabricated.
			 * rsync 3.x always sends the implied "."
			 * If we only have 1 argv, reuse it to avoid making a
			 * new allocation.
			 */
		} else if (argc != 2) {
			ERRX("server receiver mode requires two argument");
			goto out;
		} else {
			argv++;
			argc--;
		}

		if (!rsync_receiver(&sess, cleanup_ctx, fdin, fdout, argv[0])) {
			ERRX1("rsync_receiver");
			goto out;
		}
	}

	rc = 0;

	if (io_read_check(&sess, fdin)) {
		ERRX1("data remains in read pipe");
		rc = ERR_IPC;
	} else if (sess.err_del_limit) {
		assert(sess.total_deleted >= sess.opts->max_delete);
		rc = ERR_DEL_LIMIT;
	} else if (sess.total_errors > 0) {
		rc = ERR_PARTIAL;
	}
out:
	return rc;
}
