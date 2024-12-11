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
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if HAVE_ERR
# include <err.h>
#endif

#include "extern.h"

/*
 * The rsync client runs on the operator's local machine.
 * It can either be in sender or receiver mode.
 * In the former, it synchronises local files from a remote sink.
 * In the latter, the remote sink synchronses to the local files.
 * Returns exit code 0 on success, 1 on failure, 2 on failure with
 * incompatible protocols.
 */
int
rsync_client(struct cleanup_ctx *cleanup_ctx, const struct opts *opts,
    int fd, const struct fargs *f)
{
	struct sess	 sess;
	int		 rc = 1;

	/* Standard rsync preamble, sender side. */

#ifndef __APPLE__
	if (pledge("stdio unix rpath wpath cpath dpath fattr chown getpw unveil",
	    NULL) == -1)
		err(ERR_IPC, "pledge");
#endif

	memset(&sess, 0, sizeof(struct sess));
	sess.opts = opts;
	sess.mode = f->mode;
	sess.lver = sess.protocol = sess.opts->protocol;
	sess.wbatch_fd = -1;

	if (sess.opts->chmod != NULL)
		chmod_parse(sess.opts->chmod, &sess);

	cleanup_set_session(cleanup_ctx, &sess);
	cleanup_release(cleanup_ctx);

	if (!io_write_int(&sess, fd, sess.lver)) {
		ERRX1("io_write_int");
		goto out;
	} else if (!io_read_int(&sess, fd, &sess.rver)) {
		ERRX1("io_read_int");
		goto out;
	} else if (!io_read_int(&sess, fd, &sess.seed)) {
		ERRX1("io_read_int");
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

	LOG2("client detected client version %d, server version %d, "
	    "negotiated protocol version %d, seed %d",
	    sess.lver, sess.rver, sess.protocol, sess.seed);

	if (sess.opts->write_batch != NULL && (rc = batch_open(&sess)) != 0) {
		ERRX1("batch_open");
		rc = 2;
		goto out;
	}

	/*
	 * When --files-from is in effect, and the file is on the remote
	 * side, we need to defer multiplexing.  The receiver just dumps
	 * that file into the socket without further adherence to protocol.
	 */
	if (sess.opts->filesfrom_host && f->mode == FARGS_SENDER)
		sess.filesfrom_fd = fd;
	else
		sess.mplex_reads = 1;

	assert(sess.opts->whole_file != -1);
	LOG2("Delta transmission %s for this transfer",
	    sess.opts->whole_file ? "disabled" : "enabled");

	/*
	 * Now we need to get our list of files.
	 * Senders (and locals) send; receivers receive.
	 */

	if (f->mode != FARGS_RECEIVER) {
		LOG2("client starting sender: %s",
		    f->host == NULL ? "(local)" : f->host);
		if (!rsync_sender(&sess, fd, fd, f->sourcesz,
		    f->sources)) {
			ERRX1("rsync_sender");
			goto out;
		}
	} else {
		LOG2("client starting receiver: %s",
		    f->host == NULL ? "(local)" : f->host);

		/*
		 * The client traditionally doesn't multiplex writes, but it
		 * does need to do so after the version exchange in the case of
		 * --remove-source-files in the receiver role -- it may need to
		 * send SUCCESS messages to confirm that a transfer has
		 * completed.
		 */
		if (sess.opts->remove_source)
			sess.mplex_writes = 1;

		if (!rsync_receiver(&sess, cleanup_ctx, fd, fd, f->sink)) {
			ERRX1("rsync_receiver");
			goto out;
		}
	}

	/*
	 * Make sure we flush out any remaining log messages or whatnot before
	 * we leave.  This is especially important with higher verbosity levels
	 * as smb rsync will be a lot more chatty with non-data messages over
	 * the wire.  If there's still data-tagged messages in after a flush,
	 * then.
	 */
	rc = 0;
	if (!io_read_close(&sess, fd)) {
		ERRX1("data remains in read pipe");
		rc = ERR_IPC;
	} else if (sess.err_del_limit) {
		assert(sess.total_deleted >= sess.opts->max_delete);
		rc = ERR_DEL_LIMIT;
	} else if (sess.total_errors > 0) {
		rc = ERR_PARTIAL;
	}
out:
	batch_close(&sess, f, rc);
	return rc;
}
