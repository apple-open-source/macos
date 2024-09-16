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

#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

#include "extern.h"

/*
 * Flag bitmap in the header.  Note that `protocol` may be set to 0 if it was
 * one of the initial flags preserved in the batch file.  These should be kept
 * in bit order.  Note that all of these fields should be ints in our
 * `struct opts`.
 */
#define	OPT_FIELD(n)	offsetof(struct opts, n)
static const struct batchflag {
	const char	*name;		/* Informational purposes */
	size_t		 offset;	/* Offset into struct opts */
	int		 protocol;	/* Minimum protocol */
} batchflags[] = {
	{ "recurse",	OPT_FIELD(recursive),		0 },
	{ "owner",	OPT_FIELD(preserve_uids),	0 },
	{ "group",	OPT_FIELD(preserve_gids),	0 },
	{ "links",	OPT_FIELD(preserve_links),	0 },
	{ "devices",	OPT_FIELD(devices),		0 },
	{ "hard-links",	OPT_FIELD(hard_links),		0 },
	{ "checksum",	OPT_FIELD(checksum),		0 },
	{ "dirs",	OPT_FIELD(dirs),		29 },
#ifdef NOTYET
	{ "compress",	OPT_FIELD(compress),		29 },
#endif
};

struct batchhdr {
	int		flags;
	int		rver;
	int		seed;
};

/*
 * Read the batchhdr from our file; flags is a bitmap described above, and
 * rver and seed are the initial communications we'd normally receive from a
 * remote end.
 */
static int
read_batch_header(struct sess *sess, int batch_fd, struct batchhdr *hdr)
{

	if (!io_read_int(sess, batch_fd, &hdr->flags)) {
		ERRX1("io_read_int");
		return ERR_PROTOCOL;
	}

	if (!io_read_int(sess, batch_fd, &hdr->rver)) {
		ERRX1("io_read_int");
		return ERR_PROTOCOL;
	}

	if (!io_read_int(sess, batch_fd, &hdr->seed)) {
		ERRX1("io_read_int");
		return ERR_PROTOCOL;
	}

	return 0;
}

static int
write_batch_header(struct sess *sess, int batch_fd, struct batchhdr *hdr)
{

	if (!io_write_int(sess, batch_fd, hdr->flags)) {
		ERRX1("io_write_int");
		return ERR_PROTOCOL;
	}

	if (!io_write_int(sess, batch_fd, hdr->rver)) {
		ERRX1("io_write_int");
		return ERR_PROTOCOL;
	}

	if (!io_write_int(sess, batch_fd, hdr->seed)) {
		ERRX1("io_write_int");
		return ERR_PROTOCOL;
	}

	return 0;
}

static void
batch_apply_flags(struct sess *sess, struct batchhdr *hdr, struct opts *opts)
{
	const struct batchflag	*bflag;
	int			*field, value;

	for (size_t bit = 0; bit < nitems(batchflags); bit++) {
		bflag = &batchflags[bit];
		if (bflag->protocol > 0 && bflag->protocol > sess->protocol)
			break;

		assert(bflag->offset >= 0 && bflag->offset < sizeof(*opts));
		value = !!(hdr->flags & (1 << bit));
		field = (int *)(((uintptr_t)opts) + bflag->offset);
		if (*field != value) {
			LOG1("Mismatch of %s option, changing from %d -> %d",
			    bflag->name, *field, value);
		}

		*field = value;
	}

	if (!protocol_newbatch) {
		if (opts->recursive) {
			opts->dirs = DIRMODE_IMPLIED;
		} else if (opts->dirs == DIRMODE_IMPLIED) {
			opts->dirs = DIRMODE_OFF;
		}
	}
}

static void
batch_translate_flags(struct sess *sess, struct batchhdr *hdr)
{
	const struct batchflag	*bflag;
	int			*field;

	hdr->flags = 0;
	for (size_t bit = 0; bit < nitems(batchflags); bit++) {
		bflag = &batchflags[bit];
		if (bflag->protocol > 0 && bflag->protocol > sess->protocol)
			break;

		assert(bflag->offset >= 0 &&
		    bflag->offset < sizeof(*sess->opts));
		field = (int *)(((uintptr_t)sess->opts) + bflag->offset);
		if (*field != 0)
			hdr->flags |= (1 << bit);
	}
}

int
rsync_batch(struct cleanup_ctx *cleanup_ctx, struct opts *opts,
    const struct fargs *f)
{
	struct sess	 sess;
	struct batchhdr	 hdr;
	int		 batch_fd, rc;

	memset(&sess, 0, sizeof(struct sess));
	sess.opts = opts;
	sess.mode = FARGS_RECEIVER;
	sess.lver = sess.protocol = RSYNC_PROTOCOL;
	sess.wbatch_fd = -1;

	cleanup_set_session(cleanup_ctx, &sess);
	cleanup_release(cleanup_ctx);

	batch_fd = open(sess.opts->read_batch, O_RDONLY);
	if (batch_fd == -1) {
		ERR("%s: open", sess.opts->read_batch);
		return ERR_IPC;
	}

	rc = read_batch_header(&sess, batch_fd, &hdr);
	if (rc != 0)
		goto out;

	rc = ERR_IPC;
	if (hdr.rver < RSYNC_PROTOCOL_MIN) {
		ERRX("batch protocol %d is older than our minimum supported "
		    "%d: exiting", hdr.rver, RSYNC_PROTOCOL_MIN);
		goto out;
	} else if (hdr.rver > sess.lver) {
		ERRX("batch protocol %d is newer than our maximum supported "
		    "%d: exiting", hdr.rver, sess.lver);
		goto out;
	}

	batch_apply_flags(&sess, &hdr, opts);

	sess.rver = hdr.rver;
	if (sess.rver < sess.lver) {
		sess.protocol = sess.rver;
	}

	sess.seed = hdr.seed;

	LOG2("batch detected client version %d, batch version %d, seed %d\n",
	    sess.lver, sess.rver, sess.seed);

	if (!rsync_receiver(&sess, cleanup_ctx, batch_fd, batch_fd, f->sink)) {
		ERRX1("rsync_receiver");
		goto out;
	}

	rc = 0;
out:

	close(batch_fd);
	return rc;
}

int
batch_open(struct sess *sess)
{
	struct batchhdr hdr = { 0 };
	int batch_fd, rc;

	assert(sess->opts->write_batch != NULL);
	assert(sess->wbatch_fd == -1);

	batch_fd = open(sess->opts->write_batch,
	    O_WRONLY | O_TRUNC | O_CREAT, 0600);
	if (batch_fd == -1) {
		ERR("%s: open", sess->opts->write_batch);
		return ERR_IPC;
	}

	batch_translate_flags(sess, &hdr);
	hdr.seed = sess->seed;

	/*
	 * The client records the batch file, but the batch file is written from
	 * the perspective of the sender and we need to use the appropriate
	 * version as such.
	 */
	hdr.rver = sess->protocol;

	rc = write_batch_header(sess, batch_fd, &hdr);
	if (rc != 0)
		goto out;

	rc = 0;
	sess->wbatch_fd = batch_fd;
out:
	if (rc != 0)
		close(batch_fd);
	return rc;
}

void
batch_close(struct sess *sess, const struct fargs *f, int rc)
{
	struct fargs batchf;
	char shell_path[PATH_MAX];
	FILE *fp;
	char **args;
	char **rules;

	if (sess->wbatch_fd == -1)
		return;

	close(sess->wbatch_fd);
	sess->wbatch_fd = -1;

	if (rc != 0)
		return;

	if (snprintf(shell_path, sizeof(shell_path), "%s.sh",
	    sess->opts->write_batch) >= (int)sizeof(shell_path)) {
		WARNX1("%s.sh: path too long, did not write batch shell",
		    sess->opts->write_batch);
		return;
	}

	/*
	 * We're writing out arguments that the receiver needs, so pretend that
	 * we're the sender even if we aren't.  Clear out the sources because
	 * those will not be used.
	 */
	memset(&batchf, 0, sizeof(batchf));
	batchf.mode = FARGS_SENDER;
	batchf.sink = f->sink;

	args = fargs_cmdline(sess, &batchf, NULL);

	rules = rules_export(sess);

	fp = fopen(shell_path, "w");
	if (fp == NULL) {
		ERR("%s: fopen", shell_path);
		return;
	}

	fprintf(fp, "#!/bin/sh\n\n");

	/*
	 * We want to avoid writing out the sink argument, as we want that to
	 * use the first argument with a default value of what was provided to
	 * this script.
	 */
	for (const char **arg = (const char **)args; *(arg + 1) != NULL;
	    arg++) {
		if (strcmp(*arg, ".") == 0)
			break;

		if (strcmp(*arg, "--server") == 0)
			continue;

		if (strncmp(*arg, "--only-write-batch",
		    strlen("--only-write-batch")) == 0) {
			fprintf(fp, "--read-batch=%s ", sess->opts->write_batch);
			continue;
		}

		fprintf(fp, "%s ", *arg);
	}

	free(args);

	if (*rules != NULL) {
		if (!protocol_newbatch)
			fprintf(fp, "--exclude-from=- ");
		else
			fprintf(fp, "--filter=\". -\"");
	}

	fprintf(fp, "${1-%s}", batchf.sink);

	if (*rules != NULL) {
		fprintf(fp, " <<@REOF@\n");

		for (const char **rule = (const char **)rules; *rule != NULL;
		    rule++) {
			fprintf(fp, "%s\n", *rule);
		}

		fprintf(fp, "@REOF@");
	}

	free(rules);

	fprintf(fp, "\n");
	fclose(fp);
}
