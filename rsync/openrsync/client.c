/*
 * Copyright (c) 2019 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2024, Klara, Inc.
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
 * Print time as hh:mm:ss
 */
static void
print_time(FILE *f, double time)
{
	int i = time;
	fprintf(f, "   %02d:%02d:%02d",
	    i / 3600, (i - i / 3600 * 3600) / 60,
	    (i - i / 60 * 60));
}

/*
 * Maybe print progress in current file.
 */
void
rsync_progress(struct sess *sess, uint64_t total_bytes, uint64_t so_far,
    bool finished, size_t idx, size_t totalidx)
{
	struct timeval tv;
	double delta, now, remaining_time, rate;

	if (!sess->opts->progress || sess->opts->server)
		return;

	gettimeofday(&tv, NULL);
	now = tv.tv_sec + (double)tv.tv_usec / 1000000.0;

	/*
	 * Print progress.
	 * This calculates from previous transfer.
	 */
	if (sess->xferstat.last_time == 0) {
		sess->xferstat.count++;
		sess->xferstat.start_time = sess->xferstat.last_time = now;
		assert(sess->xferstat.last_bytes == 0);
		return;
	}
	if ((now - sess->xferstat.last_time) < 0.5 && !finished)
		return;
	printf(" %14llu", (long long unsigned)so_far);
	printf(" %3.0f%%", (double)so_far /
	    (double)total_bytes * 100.0);

	/*
	 * Once we've finished, displaying 00:00:00 for all entries isn't really
	 * useful for anyone; switch to the total time taken for all of our
	 * stats.
	 */
	if (finished) {
		delta = (now - sess->xferstat.start_time);
		rate = (double)so_far / delta;
	} else {
		delta = (now - sess->xferstat.last_time);
		rate = (double)(so_far - sess->xferstat.last_bytes) / delta;
	}

	if (rate > 1024.0 * 1024.0 * 1024.0) {
		printf(" %7.2fGB/s", rate / 1024.0 / 1024.0 / 1024.0);
	} else if (rate > 1024.0 * 1024.0) {
		printf(" %7.2fMB/s", rate / 1024.0 / 1024.0);
	} else if (rate > 1024.0) {
		printf(" %7.2fKB/s", rate / 1024.0);
	}

	if (finished)
		remaining_time = delta;
	else
		remaining_time = (total_bytes - so_far) / rate;
	print_time(stdout, remaining_time);

	if (finished) {
		printf(" (xfer#%zu, to-check=%zu/%zu)\n",
		    sess->xferstat.count, idx, totalidx);
		sess->xferstat.start_time = sess->xferstat.last_time = 0;
		sess->xferstat.last_bytes = 0;
	} else {
		printf("\r");
		sess->xferstat.last_time = now;
		sess->xferstat.last_bytes = so_far;
	}
	fflush(stdout);
}


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

	log_format_init(&sess);

	LOG4("Printing(%d): itemize %d late %d", getpid(), sess.itemize, sess.lateprint);

	cleanup_set_session(cleanup_ctx, &sess);
	cleanup_release(cleanup_ctx);

	if (sess.opts->read_batch == NULL &&
	    !io_write_int(&sess, fd, sess.lver)) {
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

	LOG3("client detected client version %d, server version %d, "
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
	else if (sess.opts->filesfrom && sess.opts->server &&
		 strcmp(sess.opts->filesfrom, "-") == 0 &&
		 f->mode == FARGS_SENDER)
		sess.filesfrom_fd = fd;
	else
		sess.mplex_reads = 1;

	assert(sess.opts->whole_file != -1);

	if (verbose > 1 && f->mode == FARGS_RECEIVER) {
		LOG0("Delta transmission %s for this transfer",
		    sess.opts->whole_file ? "disabled" : "enabled");
	}

	/*
	 * Now we need to get our list of files.
	 * Senders (and locals) send; receivers receive.
	 */

	if (f->mode != FARGS_RECEIVER) {
		LOG3("client starting sender: %s",
		    f->host == NULL ? "(local)" : f->host);

		sess.lreceiver = (f->host == NULL);

		if (!rsync_sender(&sess, fd, fd, f->sourcesz,
		    f->sources)) {
			ERRX1("rsync_sender");
			goto out;
		}
	} else {
		LOG3("client starting receiver: %s",
		    f->host == NULL ? "(local)" : f->host);

		sess.lreceiver = true;

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
		if (sess.mplex_read_remain > 0)
			ERRX1("data remains in read pipe");
		rc = ERR_IPC;
	} else if (sess.err_del_limit) {
		assert(sess.total_deleted >= sess.opts->max_delete ||
		    sess.opts->dry_run);
		rc = ERR_DEL_LIMIT;
	} else if (sess.total_errors > 0) {
		rc = ERR_PARTIAL;
	}
out:
	batch_close(&sess, f, rc);
	sess_cleanup(&sess);
	return rc;
}
