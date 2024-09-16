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

#include <sys/mman.h>
#if HAVE_SYS_QUEUE
# include <sys/queue.h>
#endif
#include <sys/stat.h>
#include <sys/time.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#include "zlib/zlib.h"
#include "md4.h"

#include "extern.h"

#define PHASE_MAX	2

struct	success_ctx {
	struct sess	*sess;
	const struct fl	*fl;
};

/*
 * A request from the receiver to download updated file data.
 */
struct	send_dl {
	int32_t			 idx; /* index in our file list */
	struct blkset		*blks; /* the sender's block information */
	size_t			 blkidx; /* last block index read */
	enum send_dl_state	 dlstate; /* current blk recv state */
	struct vstring		 linkstr; /* (itemized) link string data */
	TAILQ_ENTRY(send_dl)	 entries;
};

static enum zlib_state	 comp_state; /* compression state */
static z_stream		 cctx; /* compression context */

/*
 * The current file being "updated": sent from sender to receiver.
 * If there is no file being uploaded, "cur" is NULL.
 */
struct	send_up {
	struct send_dl	*cur; /* file being updated or NULL */
	struct blkstat	 stat; /* status of file being updated */
};

TAILQ_HEAD(send_dlq, send_dl);

/*
 * We have finished updating the receiver's file with sender data.
 * Deallocate and wipe clean all resources required for that.
 */
static void
send_up_reset(struct send_up *p)
{

	assert(p != NULL);

	/* Free the download request, if applicable. */

	if (p->cur != NULL) {
		free(p->cur->blks);
		free(p->cur);
		p->cur = NULL;
	}

	/* If we mapped a file for scanning, unmap it and close. */

	if (p->stat.map != MAP_FAILED)
		munmap(p->stat.map, p->stat.mapsz);

	p->stat.map = MAP_FAILED;
	p->stat.mapsz = 0;

	if (p->stat.fd != -1)
		close(p->stat.fd);

	p->stat.fd = -1;

	/* Now clear the in-transfer information. */

	p->stat.offs = 0;
	p->stat.hint = 0;
	p->stat.curst = BLKSTAT_NONE;
}

/* Returns 1 on success, 0 on error */
static int
compress_reinit(struct sess *sess)
{

	if (comp_state == COMPRESS_INIT) {
		cctx.zalloc = NULL;
		cctx.zfree = NULL;
		cctx.next_in = NULL;
		cctx.avail_in = 0;
		cctx.next_out = NULL;
		cctx.avail_out = 0;
		if (deflateInit2(&cctx, sess->opts->compression_level,
		    Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
			ERRX("deflateInit2");
			return 0;
		}
		comp_state = COMPRESS_RUN;
	} else if (comp_state >= COMPRESS_DONE) {
		cctx.next_in = NULL;
		cctx.avail_in = 0;
		cctx.next_out = NULL;
		cctx.avail_out = 0;
		deflateReset(&cctx);
		comp_state = COMPRESS_RUN;
	}

	return 1;
}
/*
 * Fast forward through part of the file the other side already
 * has while keeping compression state intact.
 * Returns 1 on success, 0 on error.
 */
static int
token_ff_compressed(struct sess *sess, struct send_up *up, size_t tok)
{
	char		*buf = NULL, *cbuf = NULL;
	size_t		 sz, clen, rlen;
	off_t		 off;
	int		 res;

	if (tok >= up->cur->blks->blksz) {
		ERRX("token not in block set: %zu (have %zu blocks)",
		    tok, up->cur->blks->blksz);
		return 0;
	}
	sz = (tok == up->cur->blks->blksz - 1 && up->cur->blks->rem) ?
	    up->cur->blks->rem : up->cur->blks->len;
	assert(sz);
	assert(up->stat.map != MAP_FAILED);
	off = tok * up->cur->blks->len;
	buf = up->stat.map + off;

	cbuf = malloc(MAX_CHUNK_BUF);
	if (cbuf == NULL) {
		ERRX1("malloc");
		return 0;
	}
	if (!compress_reinit(sess)) {
		ERRX1("decompress_reinit");
		free(cbuf);
		return 0;
	}

	cctx.avail_in = 0;
	rlen = sz;
	clen = 0;
	while (rlen > 0) {
		clen = rlen;
		if (clen > MAX_CHUNK) {
			clen = MAX_CHUNK;
		}
		rlen -= clen;
		cctx.next_in = (Bytef *)buf;
		cctx.avail_in = (uInt)clen;
		cctx.next_out = (Bytef *)cbuf;
		cctx.avail_out = TOKEN_MAX_DATA;
		res = deflate(&cctx, Z_INSERT_ONLY);
		if (res != Z_OK || cctx.avail_in != 0) {
			ERRX("deflate ff res=%d", res);
			free(cbuf);
			return 0;
		}
		buf += clen;
	}
	free(cbuf);

	return 1;
}

/*
 * This is like send_up_fsm() except for sending compressed blocks
 * Returns zero on failure, non-zero on success.
 */
static int
send_up_fsm_compressed(struct sess *sess, size_t *phase,
	struct send_up *up, void **wb, size_t *wbsz, size_t *wbmax,
	struct flist *fl)
{
	size_t		 pos = 0, isz = sizeof(int32_t),
			 dsz = MD4_DIGEST_LENGTH;
	unsigned char	 fmd[MD4_DIGEST_LENGTH];
	off_t		 sz, ssz;
	char		 buf[16];
	char		*sbuf = NULL, *cbuf = NULL;
	int		 res;

	switch (up->stat.curst) {
	case BLKSTAT_DATA:
		/*
		 * A data segment to be written: buffer both the length
		 * and the data.
		 * If we've finished the transfer, move on to the token;
		 * otherwise, keep sending data.
		 */

		sz = MINIMUM(MAX_CHUNK,
			up->stat.mapsz - up->stat.curpos);
		sbuf = up->stat.map + up->stat.curpos;
		cbuf = malloc(TOKEN_MAX_BUF);
		if (cbuf == NULL) {
			ERRX1("malloc");
			return 0;
		}

		if (!compress_reinit(sess)) {
			ERRX1("decompress_reinit");
			free(cbuf);
			return 0;
		}

		assert(comp_state == COMPRESS_RUN);
		cctx.next_in = (Bytef *)sbuf;
		cctx.avail_in = (uInt)sz;
		cctx.next_out = (Bytef *)(cbuf + 2);
		cctx.avail_out = TOKEN_MAX_DATA;

		while ((res = deflate(&cctx, Z_NO_FLUSH)) == Z_OK) {
			ssz = TOKEN_MAX_DATA - cctx.avail_out;
			if (ssz == 0) {
				break;
			}
			if (!io_lowbuffer_alloc(sess, wb, wbsz, wbmax, ssz + 2)) {
				ERRX("io_lowbuffer_alloc");
				free(cbuf);
				return 0;
			}
			cbuf[0] = (TOKEN_DEFLATED + (ssz >> 8)) & 0xff;
			cbuf[1] = ssz & 0xff;
			io_lowbuffer_buf(sess, *wb, &pos, *wbsz, cbuf, ssz + 2);
			if (cctx.avail_out != 0) {
				break;
			}
			if (cctx.avail_out == 0) {
				cctx.next_out = (Bytef *)(cbuf + 2);
				/* Save room for the 4 byte trailer */
				cctx.avail_out = TOKEN_MAX_DATA;
			}
		}
		if (res != Z_OK && res != Z_BUF_ERROR) {
			ERRX("deflate res=%d", res);
			free(cbuf);
			return 0;
		}
		free(cbuf);
		up->stat.curpos += sz;
		if (up->stat.curpos == up->stat.mapsz) {
			up->stat.curst = BLKSTAT_FLUSH;
		}
		return 1;
	case BLKSTAT_TOK:
		/*
		 * The data token tells the receiver to copy a block of
		 * data from the existing file they have, instead of having
		 * us send the data.
		 * It's followed by a hash or another data segment,
		 * depending on the token.
		 */

		up->stat.curst = up->stat.curtok ?
			BLKSTAT_NEXT : BLKSTAT_FLUSH;

		if (up->stat.curtok == 0) {
			/* Empty files don't need handling */
			return 1;
		}

		if (!io_lowbuffer_alloc(sess, wb, wbsz, wbmax, 1)) {
			ERRX1("io_lowbuffer_alloc");
			return 0;
		}
		io_lowbuffer_byte(sess, *wb,
			&pos, *wbsz, TOKEN_LONG);
		if (!io_lowbuffer_alloc(sess, wb, wbsz, wbmax, isz)) {
			ERRX1("io_lowbuffer_alloc");
			return 0;
		}
		io_lowbuffer_int(sess, *wb,
			&pos, *wbsz, -(up->stat.curtok + 1));

		token_ff_compressed(sess, up, -(up->stat.curtok + 1));
		return 1;
	case BLKSTAT_HASH:
		/*
		 * The hash following transmission of all file contents.
		 * This is always followed by the state that we're
		 * finished with the file.
		 */

		hash_file(up->stat.map, up->stat.mapsz, fmd, sess);
		if (!io_lowbuffer_alloc(sess, wb, wbsz, wbmax, dsz)) {
			ERRX1("io_lowbuffer_alloc");
			return 0;
		}
		io_lowbuffer_buf(sess, *wb, &pos, *wbsz, fmd, dsz);
		up->stat.curst = BLKSTAT_DONE;
		return 1;
	case BLKSTAT_FLUSH:
		/*
		 * Flush the end of the compressed stream.
		 */

		cbuf = malloc(TOKEN_MAX_BUF);
		if (cbuf == NULL) {
			ERRX1("malloc");
			return 0;
		}
		cctx.avail_in = 0;
		cctx.next_in = NULL;
		cctx.next_out = (Bytef *)(cbuf + 2);
		cctx.avail_out = TOKEN_MAX_DATA;

		while ((res = deflate(&cctx, Z_SYNC_FLUSH)) == Z_OK) {
			ssz = TOKEN_MAX_DATA - cctx.avail_out;
			assert(ssz >= 4);
			ssz -= 4; /* Trim off the trailer bytes */
			if (ssz != 0 && res != Z_BUF_ERROR) {
				if (!io_lowbuffer_alloc(sess, wb, wbsz, wbmax, ssz + 2)) {
					ERRX("io_lowbuffer_alloc");
					free(cbuf);
					return 0;
				}
				cbuf[0] = (TOKEN_DEFLATED + (ssz >> 8)) & 0xff;
				cbuf[1] = ssz & 0xff;
				io_lowbuffer_buf(sess, *wb, &pos, *wbsz, cbuf, ssz + 2);
			}
			cctx.next_out = (Bytef *)(cbuf + 2);
			cctx.avail_out = TOKEN_MAX_DATA;
			memcpy(cctx.next_out, cbuf+TOKEN_MAX_DATA-2, 4);
			cctx.next_out += 4;
			cctx.avail_out -= 4;
		}
		if (res != Z_OK && res != Z_BUF_ERROR) {
			LOG2("final deflate() res=%d", res);
		}

		/* Send the end of token marker */
		if (!io_lowbuffer_alloc(sess, wb, wbsz, wbmax, 1)) {
			ERRX("io_lowbuffer_alloc");
			free(cbuf);
			return 0;
		}
		io_lowbuffer_byte(sess, *wb, &pos, *wbsz, 0);
		free(cbuf);
		comp_state = COMPRESS_DONE;
		up->stat.curst = BLKSTAT_HASH;
		return 1;
	case BLKSTAT_DONE:
		/*
		 * The data has been written.
		 * Clear our current send file and allow the block below
		 * to find another.
		 */

		if (!sess->opts->dry_run)
			LOG3("%s: flushed %jd KB total, %.2f%% uploaded",
			    fl[up->cur->idx].path,
			    (intmax_t)up->stat.total / 1024,
			    100.0 * up->stat.dirty / up->stat.total);

		send_up_reset(up);
		return 1;
	case BLKSTAT_PHASE:
		/*
		 * This is where we actually stop the algorithm: we're
		 * already at the second phase.
		 */

		comp_state = COMPRESS_DONE;
		send_up_reset(up);
		(*phase)++;
		sess->role->append = 0;
		return 1;
	case BLKSTAT_NEXT:
		/*
		 * Our last case: we need to find the
		 * next block (and token) to transmit to
		 * the receiver.
		 * These will drive the finite state
		 * machine in the first few conditional
		 * blocks of this set.
		 */

		assert(up->stat.fd != -1);
		blk_match(sess, up->cur->blks,
			fl[up->cur->idx].path, &up->stat);
		return 1;
	case BLKSTAT_NONE:
		break;
	}

	assert(BLKSTAT_NONE == up->stat.curst);

	/*
	 * We've either hit the phase change following the last file (or
	 * start, or prior phase change), or we need to prime the next
	 * file for transmission.
	 * We special-case dry-run mode.
	 */

	if (up->cur->idx < 0) {
		if (!io_lowbuffer_alloc(sess, wb, wbsz, wbmax, isz)) {
			ERRX1("io_lowbuffer_alloc");
			return 0;
		}
		io_lowbuffer_int(sess, *wb, &pos, *wbsz, -1);
		up->stat.curst = BLKSTAT_PHASE;
	} else if (sess->opts->dry_run) {
		if (!sess->opts->server)
			LOG1("%s", fl[up->cur->idx].wpath);

		send_iflags(sess, wb, wbsz, wbmax, &pos, fl, up->cur->idx);
		up->stat.curst = BLKSTAT_DONE;
	} else {
		assert(up->stat.fd != -1);

		/*
		 * FIXME: use the nice output of log_file() and so on in
		 * downloader.c, which means moving this into
		 * BLKSTAT_DONE instead of having it be here.
		 */

		if (!sess->opts->server)
			LOG1("%s", fl[up->cur->idx].wpath);

		send_iflags(sess, wb, wbsz, wbmax, &pos, fl, up->cur->idx);

		assert(sizeof(buf) == 16);
		if (!io_lowbuffer_alloc(sess, wb, wbsz, wbmax, sizeof(buf))) {
			ERRX1("io_lowbuffer_alloc");
			return 0;
		}
		blk_recv_ack(buf, up->cur->blks, up->cur->idx);
		io_lowbuffer_buf(sess, *wb, &pos, *wbsz, buf, sizeof(buf));

		LOG3("%s: primed for %jd B total",
		    fl[up->cur->idx].path, (intmax_t)up->cur->blks->size);
		up->stat.curst = BLKSTAT_NEXT;
	}

	return 1;
}

/*
 * This is the bulk of the sender work.
 * Here we tend to an output buffer that responds to receiver requests
 * for data.
 * This does not act upon the output descriptor itself so as to avoid
 * blocking, which otherwise would deadlock the protocol.
 * Returns zero on failure, non-zero on success.
 */
static int
send_up_fsm(struct sess *sess, size_t *phase,
	struct send_up *up, void **wb, size_t *wbsz, size_t *wbmax,
	struct flist *fl)
{
	size_t		 pos = 0, isz = sizeof(int32_t),
			 dsz = MD4_DIGEST_LENGTH;
	unsigned char	 fmd[MD4_DIGEST_LENGTH];
	off_t		 sz;
	char		 buf[16];

	switch (up->stat.curst) {
	case BLKSTAT_DATA:
		/*
		 * A data segment to be written: buffer both the length
		 * and the data.
		 * If we've finished the transfer, move on to the token;
		 * otherwise, keep sending data.
		 */

		sz = MINIMUM(MAX_CHUNK,
			up->stat.curlen - up->stat.curpos);
		if (!io_lowbuffer_alloc(sess, wb, wbsz, wbmax, isz)) {
			ERRX1("io_lowbuffer_alloc");
			return 0;
		}
		io_lowbuffer_int(sess, *wb, &pos, *wbsz, (int)sz);
		if (!io_lowbuffer_alloc(sess, wb, wbsz, wbmax, sz)) {
			ERRX1("io_lowbuffer_alloc");
			return 0;
		}
		io_lowbuffer_buf(sess, *wb, &pos, *wbsz,
			up->stat.map + up->stat.curpos, sz);

		up->stat.curpos += sz;
		if (up->stat.curpos == up->stat.curlen)
			up->stat.curst = BLKSTAT_TOK;
		return 1;
	case BLKSTAT_TOK:
		/*
		 * The data token following (maybe) a data segment.
		 * These can also come standalone if, say, the file's
		 * being fully written.
		 * It's followed by a hash or another data segment,
		 * depending on the token.
		 */

		if (!io_lowbuffer_alloc(sess, wb, wbsz, wbmax, isz)) {
			ERRX1("io_lowbuffer_alloc");
			return 0;
		}
		io_lowbuffer_int(sess, *wb,
			&pos, *wbsz, up->stat.curtok);
		up->stat.curst = up->stat.curtok ?
			BLKSTAT_NEXT : BLKSTAT_HASH;
		return 1;
	case BLKSTAT_HASH:
		/*
		 * The hash following transmission of all file contents.
		 * This is always followed by the state that we're
		 * finished with the file.
		 */

		hash_file(up->stat.map, up->stat.mapsz, fmd, sess);
		if (!io_lowbuffer_alloc(sess, wb, wbsz, wbmax, dsz)) {
			ERRX1("io_lowbuffer_alloc");
			return 0;
		}
		io_lowbuffer_buf(sess, *wb, &pos, *wbsz, fmd, dsz);
		up->stat.curst = BLKSTAT_DONE;
		return 1;
	case BLKSTAT_FLUSH:
		assert(0);
		break;
	case BLKSTAT_DONE:
		/*
		 * The data has been written.
		 * Clear our current send file and allow the block below
		 * to find another.
		 */

		if (sess->opts->dry_run != DRY_FULL)
			LOG3("%s: flushed %jd KB total, %.2f%% uploaded",
			    fl[up->cur->idx].path,
			    (intmax_t)up->stat.total / 1024,
			    100.0 * up->stat.dirty / up->stat.total);
		sess->total_files_xfer++;
		sess->total_xfer_size += fl[up->cur->idx].st.size;
		send_up_reset(up);
		return 1;
	case BLKSTAT_PHASE:
		/*
		 * This is where we actually stop the algorithm: we're
		 * already at the second phase.
		 */

		send_up_reset(up);
		(*phase)++;
		sess->role->append = 0;
		return 1;
	case BLKSTAT_NEXT:
		/*
		 * Our last case: we need to find the
		 * next block (and token) to transmit to
		 * the receiver.
		 * These will drive the finite state
		 * machine in the first few conditional
		 * blocks of this set.
		 */

		assert(up->stat.fd != -1);
		blk_match(sess, up->cur->blks,
			fl[up->cur->idx].path, &up->stat);
		return 1;
	case BLKSTAT_NONE:
		break;
	}

	assert(BLKSTAT_NONE == up->stat.curst);

	/*
	 * We've either hit the phase change following the last file (or
	 * start, or prior phase change), or we need to prime the next
	 * file for transmission.
	 * We special-case dry-run mode.
	 */

	if (up->cur->idx < 0) {
		if (!io_lowbuffer_alloc(sess, wb, wbsz, wbmax, isz)) {
			ERRX1("io_lowbuffer_alloc");
			return 0;
		}
		io_lowbuffer_int(sess, *wb, &pos, *wbsz, -1);
		up->stat.curst = BLKSTAT_PHASE;
	} else if (sess->opts->dry_run == DRY_FULL) {
		if (!sess->opts->server)
			LOG1("%s", fl[up->cur->idx].wpath);

		send_iflags(sess, wb, wbsz, wbmax, &pos, fl, up->cur->idx);
		up->stat.curst = BLKSTAT_DONE;
	} else {
		assert(up->stat.fd != -1);

		/*
		 * FIXME: use the nice output of log_file() and so on in
		 * downloader.c, which means moving this into
		 * BLKSTAT_DONE instead of having it be here.
		 */

		if (!sess->opts->server && verbose > 0 &&
		    !sess->lateprint && !sess->itemize)
			print_7_or_8_bit(sess, "%s\n", fl[up->cur->idx].wpath);

		send_iflags(sess, wb, wbsz, wbmax, &pos, fl, up->cur->idx);

		assert(sizeof(buf) == 16);
		if (!io_lowbuffer_alloc(sess, wb, wbsz, wbmax, sizeof(buf))) {
			ERRX1("io_lowbuffer_alloc");
			return 0;
		}
		blk_recv_ack(buf, up->cur->blks, up->cur->idx);
		io_lowbuffer_buf(sess, *wb, &pos, *wbsz, buf, sizeof(buf));

		LOG3("%s: primed for %jd B total",
		    fl[up->cur->idx].path, (intmax_t)up->cur->blks->size);
		up->stat.curst = BLKSTAT_NEXT;
	}

	return 1;
}

/*
 * Deal with the conditional "follows" flags for extra iflag metadata.
 *
 * Returns -1 on error, 0 for incomplete, 1 when complete.
 */
static int
sender_get_iflags(struct iobuf *buf, struct flist *fl, struct send_dl *sdl)
{

	fl = &fl[sdl->idx];
	if ((fl->iflags & IFLAG_BASIS_FOLLOWS) != 0) {
		uint8_t basis;

		if (iobuf_get_readsz(buf) < sizeof(uint8_t))
			return 0;

		iobuf_read_byte(buf, &basis);

		fl->basis = basis;

		if ((fl->iflags & IFLAG_HLINK_FOLLOWS) != 0) {
			fl->iflags |= IFLAG_HAD_BASIS;
			fl->iflags &= ~IFLAG_BASIS_FOLLOWS;
		}
	}

	if ((fl->iflags & IFLAG_HLINK_FOLLOWS) != 0) {
		int ret;

		ret = iobuf_read_vstring(buf, &sdl->linkstr);
		if (ret <= 0)
			return ret;

		fl->link = sdl->linkstr.vstring_buffer;
		if ((fl->iflags & IFLAG_HAD_BASIS) != 0) {
			fl->iflags &= ~IFLAG_HAD_BASIS;
			fl->iflags |= IFLAG_BASIS_FOLLOWS;
		}
	}

	return 1;
}

/*
 * Send the Itemization flags for an index over the wire.
 * Deal with the conditional "follows" flags for extra metadata.
 */
int
send_iflags(struct sess *sess, void **wb, size_t *wbsz, size_t *wbmax,
    size_t *pos, struct flist *fl, int32_t idx)
{
	size_t linklen;

	if (!io_lowbuffer_alloc(sess, wb, wbsz, wbmax, sizeof(int32_t))) {
		ERRX1("io_lowbuffer_alloc");
		return 0;
	}
	io_lowbuffer_int(sess, *wb, pos, *wbsz, idx);

	if (!protocol_itemize) {
                return 1;
	}

	if (!io_lowbuffer_alloc(sess, wb, wbsz, wbmax, sizeof(int16_t))) {
		ERRX1("io_lowbuffer_alloc");
		return 0;
	}
	io_lowbuffer_short(sess, *wb, pos, *wbsz, fl[idx].iflags);

	if (IFLAG_BASIS_FOLLOWS & fl[idx].iflags) {
		if (!io_lowbuffer_alloc(sess, wb, wbsz, wbmax,
		    sizeof(int8_t))) {
			ERRX1("io_lowbuffer_alloc");
			return 0;
		}
		io_lowbuffer_byte(sess, *wb, pos, *wbsz,
		    fl[idx].basis);
	}
	if (IFLAG_HLINK_FOLLOWS & fl[idx].iflags) {
		linklen = strlen(fl[idx].link);
		if (!io_lowbuffer_alloc(sess, wb, wbsz, wbmax,
		    linklen + (linklen > 0x7f ? 2 : 1))) {
			ERRX1("io_lowbuffer_alloc");
			return 0;
		}
		io_lowbuffer_vstring(sess, *wb, pos, *wbsz,
		    fl[idx].link, linklen);
	}

	return 1;
}

/*
 * Enqueue a download request, getting it off the read channel as
 * quickly a possible.
 * This frees up the read channel for further incoming requests.
 * We'll handle each element in turn, up to and including the last
 * request (phase change), which is always a -1 idx.
 * Returns zero on failure, non-zero on success.  If the received element was
 * not the phase change element, then we will return it in *mdl (NULL for phase
 * change) so that the sender may start working on the metadata.
 */
static int
send_dl_enqueue(struct sess *sess, struct send_dlq *q,
    void **wb, size_t *wbsz, size_t *wbmax,
    int32_t idx, struct flist *fl, size_t flsz, int fd, struct iobuf *buf,
    struct send_dl **mdl)
{
	struct send_dl	*s;
	uint32_t	 iflags;

	/* End-of-phase marker. */
	if (idx == -1) {
		if ((s = calloc(1, sizeof(struct send_dl))) == NULL) {
			ERR("calloc");
			return 0;
		}
		s->idx = -1;
		s->blks = NULL;
		s->dlstate = SDL_DONE;
		TAILQ_INSERT_TAIL(q, s, entries);
		*mdl = NULL;
		return 1;
	} else if (idx < 0 || (uint32_t)idx >= flsz) {
		ERRX("file index out of bounds: invalid %d out of %zu",
		    idx, flsz);
		return 0;
	}

	if (!protocol_itemize)
		fl[idx].iflags = IFLAG_TRANSFER;
	else
		iobuf_read_short(buf, &fl[idx].iflags);

	iflags = fl[idx].iflags;

	/* Validate the index. */
	if (iflags == IFLAG_NEW) {
		/* Keep alive packet, do nothing */
		return 1;
	} else if ((iflags & IFLAG_TRANSFER) == 0) {
		/* We can't return early due to the state machine */
	} else if (S_ISDIR(fl[idx].st.mode)) {
		ERRX("blocks requested for "
			"directory: %s", fl[idx].path);
		return 0;
	} else if (S_ISLNK(fl[idx].st.mode)) {
		ERRX("blocks requested for "
			"symlink: %s", fl[idx].path);
		return 0;
	} else if (!S_ISREG(fl[idx].st.mode)) {
		ERRX("blocks requested for "
			"special: %s", fl[idx].path);
		return 0;
	}

	if ((iflags & IFLAG_HLINK_FOLLOWS) != 0) {
		free(fl[idx].link);
		fl[idx].link = NULL;

		if (!iobuf_alloc(sess, buf, PATH_MAX)) {
			ERRX1("iobuf_alloc");
			return 0;
		}
	}

	if ((s = calloc(1, sizeof(struct send_dl))) == NULL) {
		ERR("callloc");
		return 0;
	}
	s->idx = idx;
	s->blks = NULL;

	/*
	 * If we're not doing a dry-run then we need to go through the blk_recv
	 * machinery, but dry-runs have nothing left to read for this file; we
	 * can just push it directly into the queue and move on.
	 */
	if (sess->opts->dry_run != DRY_FULL) {
		if (protocol_itemize)
			s->dlstate = SDL_IFLAGS;
		else
			s->dlstate = SDL_META;
		*mdl = s;
	} else {
		if (protocol_itemize)
			s->dlstate = SDL_IFLAGS;
		else
			s->dlstate = SDL_DONE;
		*mdl = s;
	}

	return 1;
}

static int
file_success(void *cookie, const void *data, size_t datasz)
{
	struct success_ctx *sctx = cookie;
	size_t pos = 0;
	int32_t idx;

	io_unbuffer_int(data, &pos, datasz, &idx);
	if (pos != datasz) {
		ERRX("bad success payload size %zu", datasz);
		return 0;
	}

	if (idx < 0 || (size_t)idx >= sctx->fl->sz) {
		ERRX("success idx %d out of range", idx);
		return 0;
	}

	if (sctx->sess->opts->remove_source) {
		struct flist *fl;
		struct stat sb;

		fl = &sctx->fl->flp[idx];
		if (lstat(fl->path, &sb) == -1) {
			/* Nothing left to do here. */
			return 1;
		}

		if (sb.st_size != fl->st.size || sb.st_mtime != fl->st.mtime) {
			ERRX("%s: not removed, size or mtime changed",
			    fl->path);
			return 1;
		}

		if (unlink(fl->path) == -1)
			ERR("%s: unlink", fl->path);
	}

	return 1;
}

static int
sender_finalize(struct sess *sess, const struct fl *fl, struct iobuf *rbuf,
     int fdin, int fdout)
{
	int32_t idx;

	idx = 0;
	if (!protocol_keepalive) {
		while (iobuf_get_readsz(rbuf) < sizeof(idx)) {
			if (!iobuf_fill(sess, rbuf, fdin)) {
				ERRX1("iobuf_fill on final goodbye");
				return ERR_PROTOCOL;
			}
		}

		iobuf_read_int(rbuf, &idx);
	} else {
		size_t needed;
		int32_t keepalive;
		enum { NEED_IDX, NEED_KEEPALIVE, DONE } state;

		needed = sizeof(int32_t);
		state = NEED_IDX;
		while (state != DONE) {
			while (iobuf_get_readsz(rbuf) < needed) {
				if (!iobuf_fill(sess, rbuf, fdin)) {
					ERRX1("iobuf_fill on final goodbye");
					break;
				}
			}

			switch (state) {
			case NEED_IDX:
				iobuf_read_int(rbuf, &idx);
				if ((uint32_t)idx != fl->sz) {
					state = DONE;
					break;
				}

				state = NEED_KEEPALIVE;
				needed = sizeof(int16_t);

				break;
			case NEED_KEEPALIVE:
				iobuf_read_short(rbuf, &keepalive);

				if (keepalive != IFLAG_NEW) {
					state = DONE;
					break;
				}

				/* Reply to the keepalive ping */
				if (!io_write_int(sess, fdout, (int)fl->sz)) {
					ERRX1("io_write_int");
					state = DONE;
					break;
				} else if (!io_write_short(sess, fdout, IFLAG_NEW)) {
					ERRX1("io_write_short");
					state = DONE;
					break;
				}

				state = NEED_IDX;
				needed = sizeof(int32_t);

				break;
			default:
				assert(state != DONE);
				break;
			}
		}
	}

	if (idx != -1) {
		ERRX("read incorrect update complete ack");
		return ERR_PROTOCOL;
	}

	return 0;
}

struct fl *flg = NULL;

/*
 * A client sender manages the read-only source files and sends data to
 * the receiver as requested.
 * First it sends its list of files, then it waits for the server to
 * request updates to individual files.
 * It queues requests for updates as soon as it receives them.
 * Returns zero on failure, non-zero on success.
 *
 * Pledges: stdio, getpw, rpath.
 */
int
rsync_sender(struct sess *sess, int fdin,
	int fdout, size_t argc, char **argv)
{
	struct iobuf	    rbuf = { 0 };
	struct role	    sender;
	struct success_ctx  sctx;
	struct fl	    fl;
	const struct flist *f;
	size_t		    flinfosz, i, phase = 0;
	int		    rc = 0, c, metadata_phase = 0, res = 0;
	int32_t		    idx;
	struct pollfd	    pfd[3];
	struct send_dlq	    sdlq;
	struct send_dl	   *dl, *mdl = NULL;
	struct send_up	    up;
	struct stat	    st;
	void		   *wbuf = NULL;
	size_t		    wbufpos = 0, wbufsz = 0, wbufmax = 0, flist_bytes = 0;
	ssize_t		    ssz;
	int		    markers = 0, shutdown = 0;
	struct timeval	    tv, fb_before, fb_after, fx_before, fx_after;
	double		    now, rate, sleeptime;
	int		    max_phase = sess->protocol >= 29 ? 2 : 1;

	/*
	 * Each time we start a new file, we want to be able to grab at least
	 * the index and its iflags.  It's safe to assume we'll get at least
	 * this much even near the end of the transfer, because the goodbye
	 * indicator is another int32_t -- we'll just accidentally see part of
	 * the goodbye indicator as available, but we won't over-read into it
	 * so everything works out.
	 */
	flinfosz = sizeof(int32_t);
	if (protocol_itemize)
		flinfosz += sizeof(int16_t);

	fl_init(&fl);
	flg = &fl;
	if (pledge("stdio getpw rpath", NULL) == -1) {
		ERR("pledge");
		return 0;
	}

	memset(&sender, 0, sizeof(sender));
	sender.append = sess->opts->append;
	sender.phase = &metadata_phase;
	sess->role = &sender;

	memset(&up, 0, sizeof(struct send_up));
	TAILQ_INIT(&sdlq);
	up.stat.fd = -1;
	up.stat.map = MAP_FAILED;
	up.stat.blktab = blkhash_alloc();

	/*
	 * Fill in from --files-from, if given.
	 */
	if (sess->opts->filesfrom != NULL) {
		/* Only one argument allowed */
		if (argc != 1) {
			ERRX("Only one src dir allowed with --files-from");
			goto out;
		}

		if (read_filesfrom(sess, argv[0]) == 0)
			goto out;
		else {
			/*
			 * The requested --files-from is expected to be relative
			 * to the specified source directory, so chdir() to it.
			 */
			if (chdir(argv[0]) == -1) {
				ERR("%s: chdir", argv[0]);
				goto out;
			}
			argc = sess->filesfrom_n;
			argv = sess->filesfrom;
		}
		if (sess->opts->filesfrom_host != NULL) {
			assert(!sess->opts->server);
			sess->mplex_reads = 1;
		}
	}

	/*
	 * Client sends zero-length exclusions if deleting, unless we're
	 * deleting excluded files, too.
	 */
	if (!sess->opts->server && sess->opts->del &&
	    (!sess->opts->del_excl || protocol_delrules))
		send_rules(sess, fdout);

	/*
	 * If we're the server, read our exclusion list.  We need to do this
	 * early as some rules may hide files from the transfer.
	 */

	if (sess->opts->server)
		recv_rules(sess, fdin);

	/*
	 * Generate the list of files we want to send from our
	 * command-line input.
	 * This will also remove all invalid files.
	 */

	gettimeofday(&fb_before, NULL);
	if (!flist_gen(sess, argc, argv, &fl)) {
		ERRX1("flist_gen");
		goto out;
	}
	gettimeofday(&fb_after, NULL);
	timersub(&fb_after, &fb_before, &tv);
	sess->flist_build = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
	sess->total_files = fl.sz;

	/*
	 * Then the file list in any mode.
	 * Finally, the IO error (always zero for us).
	 */

	gettimeofday(&fx_before, NULL);
	flist_bytes = sess->total_write;
	if (!flist_send(sess, fdin, fdout, fl.flp, fl.sz)) {
		ERRX1("flist_send");
		goto out;
	} else if (!io_write_int(sess, fdout, sess->total_errors > 0 ? 1 : 0)) {
		ERRX1("io_write_int");
		goto out;
	}

	gettimeofday(&fx_after, NULL);
	timersub(&fx_after, &fx_before, &tv);
	sess->flist_xfer = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
	sess->flist_size = sess->total_write - flist_bytes;

	/* Exit if we're the server with zero files. */

	if (fl.sz == 0 && sess->opts->server) {
		WARNX("sender has empty file list: exiting");
		rc = 1;
		goto out;
	} else if (!sess->opts->server)
		LOG1("Transfer starting: %zu files", fl.sz);

	if (sess->opts->remove_source) {
		sctx.sess = sess;
		sctx.fl = &fl;

		if (!io_register_handler(IT_SUCCESS, &file_success, &sctx)) {
			ERRX("Failed to install remove-source-files handler; exiting");
			rc = 1;
			goto out;
		}
	}

	/* Use rbuf from this point forward. */
	if (!iobuf_alloc(sess, &rbuf, sizeof(int32_t) * 5)) {
		ERRX1("iobuf_alloc");
		goto out;
	}

	/*
	 * Set up our poll events.
	 * We start by polling only in receiver requests, enabling other
	 * poll events on demand.
	 */

	pfd[0].fd = fdin; /* from receiver */
	pfd[0].events = POLLIN;
	pfd[1].fd = -1; /* to receiver */
	pfd[1].events = POLLOUT;
	pfd[2].fd = -1; /* from local file */
	pfd[2].events = POLLIN;

	for (;;) {
#define	READ_AVAIL(pfd, iobuf) \
	(((pfd)[0].revents & POLLIN) != 0 || iobuf_get_readsz((iobuf)) != 0)
		assert(pfd[0].fd != -1);

		if (iobuf_get_readsz(&rbuf) > 0) {
			/*
			 * There is pending data still, read what more is
			 * available and try to process it rather than
			 * sleeping in poll();
			 */
			c = poll(pfd, 3, 0);
			if (c == -1) {
				if (errno == EINTR)
					continue;
				ERR("poll");
				goto out;
			}
		} else if ((c = poll(pfd, 3, poll_timeout)) == -1) {
			if (errno == EINTR)
				continue;
			ERR("poll");
			goto out;
		} else if (c == 0) {
			ERRX("poll: timeout");
			goto out;
		}
		for (i = 0; i < 3; i++)
			if (pfd[i].revents & (POLLERR|POLLNVAL)) {
				ERRX("poll: bad fd");
				goto out;
			} else if (pfd[i].revents & POLLHUP) {
				ERRX("poll: hangup");
				goto out;
			}

		/*
		 * If we have a request coming down off the wire, pull
		 * it in as quickly as possible into our buffer.
		 * Start by seeing if we have a log message.
		 * If we do, pop it off, then see if we have anything
		 * left and hit it again if so (read priority).
		 */

		if (sess->mplex_reads && (pfd[0].revents & POLLIN)) {
			if (!io_read_flush(sess, fdin)) {
				ERRX1("io_read_flush");
				goto out;
			} else if (sess->mplex_read_remain == 0 && !shutdown) {
				c = io_read_check(sess, fdin);
				if (c < 0) {
					ERRX1("io_read_check");
					goto out;
				} else if (c > 0)
					continue;
				pfd[0].revents &= ~POLLIN;
			}
		}

		if ((pfd[0].revents & POLLIN) != 0 &&
		    (!sess->mplex_reads || sess->mplex_read_remain > 0)) {
			if (!iobuf_fill(sess, &rbuf, fdin)) {
				ERRX1("iobuf_fill");
				goto out;
			}

			pfd[0].revents &= ~POLLIN;
		}

		/*
		 * Now that we've handled the log messages, we're left
		 * here if we have any actual data coming down.
		 * Enqueue message requests, then loop again if we see
		 * more data (read priority).
		 */

		if (READ_AVAIL(pfd, &rbuf) && mdl == NULL && !shutdown) {
			size_t avail;

			avail = iobuf_get_readsz(&rbuf);
			if (avail == sizeof(int32_t) &&
			    iobuf_peek_int(&rbuf) == -1) {
				/*
				 * End-of-phase markers won't have any follow-up
				 * until we ack the end-of-phase, we should just
				 * pass these through.
				 */
			} else if (avail < flinfosz) {
				continue;
			}

			iobuf_read_int(&rbuf, &idx);

			/*
			 * Start to spin down; most notably, we need to avoid
			 * trying to enqueue anything else because we should
			 * only observe the end-of-transmission marker after
			 * this.  We could also see other non-data messages come
			 * in via fdin, so we should *only* stop reading indices
			 * from fdin and continue flushing out any pending
			 * messages above.
			 */
			if (idx == -1) {
				if (++markers >= max_phase + 1) {
					shutdown = 1;
				}

				/*
				 * We track the metadata phase separately
				 * because blk_find() needs to observe that the
				 * overall session is still in append mode, but
				 * send_dl_enqueue() needs to know that it
				 * should look for any incoming block
				 * information.
				 */
				metadata_phase++;
			}
			assert(mdl == NULL);
			if (!send_dl_enqueue(sess,
			    &sdlq, &wbuf, &wbufsz, &wbufmax,
			    idx, fl.flp, fl.sz, fdin, &rbuf, &mdl)) {
				ERRX1("send_dl_enqueue");
				goto out;
			}

			if (idx == -1)
				assert(mdl == NULL);

		}

		if (READ_AVAIL(pfd, &rbuf) && mdl != NULL) {
			int bret;

			switch (mdl->dlstate) {
			case SDL_IFLAGS:
				bret = sender_get_iflags(&rbuf, fl.flp, mdl);
				if (bret < 0) {
					ERRX1("sender_get_iflags");
					return 0;
				} else if (bret == 0) {
					break;
				}

				if ((fl.flp[mdl->idx].iflags & IFLAG_TRANSFER) == 0) {
					mdl->dlstate = SDL_SKIP;
					break;
				} else if (sess->opts->dry_run == DRY_FULL) {
					mdl->dlstate = SDL_DONE;
					break;
				}

				mdl->dlstate = SDL_META;
				/* FALLTHROUGH */
			case SDL_META:
			case SDL_BLOCKS:
				mdl->blks = blk_recv(sess, fdin, &rbuf,
				    fl.flp[mdl->idx].path, mdl->blks,
				    &mdl->blkidx, &mdl->dlstate);
				if (mdl->dlstate != SDL_META &&
				    mdl->blks == NULL) {
					ERRX1("blk_recv");
					return 0;
				}

				break;
			default:
				break;
			}

			if (mdl->dlstate == SDL_DONE) {
				TAILQ_INSERT_TAIL(&sdlq, mdl, entries);
				mdl = NULL;
			} else if (mdl->dlstate == SDL_SKIP) {
				free(mdl);
				mdl = NULL;
			}

			pfd[0].revents &= ~POLLIN;

			c = io_read_check(sess, fdin);
			if (c < 0) {
				ERRX1("io_read_check");
				goto out;
			} else if (c > 0)
				continue;
		}

		/*
		 * One of our local files has been opened in response
		 * to a receiver request and now we can map it.
		 * We'll respond to the event by looking at the map when
		 * the writer is available.
		 * Here we also enable the poll event for output.
		 */

		if (pfd[2].revents & POLLIN) {
			assert(up.cur != NULL);
			assert(up.stat.fd != -1);
			assert(up.stat.map == MAP_FAILED);
			assert(up.stat.mapsz == 0);
			f = &fl.flp[up.cur->idx];

			if (fstat(up.stat.fd, &st) == -1) {
				ERR("%s: fstat", f->path);
				goto out;
			}

			/*
			 * If the file is zero-length, the map will
			 * fail, but either way we want to unset that
			 * we're waiting for the file to open and set
			 * that we're ready for the output channel.
			 */

			if ((up.stat.mapsz = st.st_size) > 0) {
				up.stat.map = mmap(NULL,
					up.stat.mapsz, PROT_READ,
					MAP_SHARED, up.stat.fd, 0);
				if (up.stat.map == MAP_FAILED) {
					ERR("%s: mmap", f->path);
					goto out;
				}
			}

			pfd[2].fd = -1;
			pfd[1].fd = fdout;
		}

		/*
		 * If we have buffers waiting to write, write them out
		 * as soon as we can in a non-blocking fashion.
		 * We must not be waiting for any local files.
		 * ALL WRITES MUST HAPPEN HERE.
		 * This keeps the sender deadlock-free.
		 */

		if ((pfd[1].revents & POLLOUT) && wbufsz > 0) {
			int writefd = fdout;

			assert(pfd[2].fd == -1);
			assert(wbufsz - wbufpos);

			/*
			 * If we're writing a batch file, we just send the file
			 * data straight to the batch.  We still need to catch
			 * the end of phase marker and send that over to the
			 * other side.
			 */
			if (up.stat.curst != BLKSTAT_PHASE &&
			    sess->wbatch_fd != -1 &&
			    sess->opts->dry_run == DRY_XFER)
				writefd = sess->wbatch_fd;
			ssz = write(writefd, wbuf + wbufpos, wbufsz - wbufpos);
			if (ssz == -1) {
				ERR("write");
				goto out;
			}

			if (!io_data_written(sess, writefd,
			    wbuf + wbufpos, ssz)) {
				ERRX1("io_data_written");
				goto out;
			}

			wbufpos += ssz;
			if (wbufpos == wbufsz)
				wbufpos = wbufsz = 0;
			pfd[1].revents &= ~POLLOUT;

			if (sess->opts->bwlimit) {
				gettimeofday(&tv, NULL);
				now = tv.tv_sec + (double)tv.tv_usec / 
					1000000.0;
				if (sess->start_time == 0.0)
					sess->start_time = now;
				else {
					rate = (double)sess->total_write /
						(now - sess->start_time);
					if (rate > sess->opts->bwlimit) {
						sleeptime = 
							/* Time supposed to have expired */
							sess->total_write / 
							sess->opts->bwlimit
							/* Time actually expired */
							- (now - sess->start_time)
							;
						usleep(sleeptime * 1000 * 1000);
					}
				}
			}
		}

		/*
		 * Engage the FSM for the current transfer.
		 * If our phase changes, stop processing.
		 */

		if (pfd[1].revents & POLLOUT && up.cur != NULL) {
			assert(pfd[2].fd == -1);
			assert(wbufpos == 0 && wbufsz == 0);
			if (sess->opts->compress) {
				res = send_up_fsm_compressed(sess, &phase, &up,
				    &wbuf, &wbufsz, &wbufmax, fl.flp);
			} else {
				res = send_up_fsm(sess, &phase, &up,
				    &wbuf, &wbufsz, &wbufmax, fl.flp);
			}
			if (!res) {
				ERRX1("send_up_fsm");
				goto out;
			} else if (phase > (size_t)max_phase) {
				break;
			}
		}

		/*
		 * Incoming queue management.
		 * If we have no queue component that we're waiting on,
		 * then pull off the receiver-request queue and start
		 * processing the request.
		 */

		if (up.cur == NULL) {
			struct flist *nextfl;

			assert(pfd[2].fd == -1);
			assert(up.stat.fd == -1);
			assert(up.stat.map == MAP_FAILED);
			assert(up.stat.mapsz == 0);
			assert(wbufsz == 0 && wbufpos == 0);
			pfd[1].fd = -1;

			/*
			 * If there's nothing in the queue, then keep
			 * the output channel disabled and wait for
			 * whatever comes next from the reader.
			 */

			if ((up.cur = TAILQ_FIRST(&sdlq)) == NULL)
				continue;
			assert(up.cur->dlstate == SDL_DONE);
			TAILQ_REMOVE(&sdlq, up.cur, entries);

			/* Hash our blocks. */

			blkhash_set(up.stat.blktab, up.cur->blks);

			/*
			 * End of phase: enable channel to receiver.
			 * We'll need our output buffer enabled in order
			 * to process this event.
			 */

			if (up.cur->idx == -1) {
				pfd[1].fd = fdout;
				continue;
			}

			/*
			 * Non-blocking open of file.
			 * This will be picked up in the state machine
			 * block of not being primed.
			 *
			 * Some flist entries may be synthesized or redirected
			 * by the platform implementation, so call into the
			 * flist-specified open if provided.
			 */
			nextfl = &fl.flp[up.cur->idx];
			if (nextfl->open != NULL) {
				up.stat.fd = (*nextfl->open)(sess, nextfl,
				    O_RDONLY|O_NONBLOCK);
			} else {
				up.stat.fd = open(nextfl->path,
				    O_RDONLY|O_NONBLOCK, 0);
			}
			if (up.stat.fd == -1) {
				char buf[PATH_MAX];

				ERR("%s: open (2) in %s",
				    fl.flp[up.cur->idx].path,
				    getcwd(buf, sizeof(buf)));
				goto out;
			}
			pfd[2].fd = up.stat.fd;
		}
	}

	if (!TAILQ_EMPTY(&sdlq)) {
		ERRX("phases complete with files still queued");
		goto out;
	}

	if (!sess_stats_send(sess, fdout)) {
		ERRX1("sess_stats_end");
		goto out;
	}

	/* Final "goodbye" message. */
	if ((rc = sender_finalize(sess, &fl, &rbuf, fdin, fdout)) != 0) {
		ERRX1("sender_finalize");
		goto out;
	}

	LOG2("sender finished updating");
	rc = 1;
out:
	send_up_reset(&up);
	while ((dl = TAILQ_FIRST(&sdlq)) != NULL) {
		TAILQ_REMOVE(&sdlq, dl, entries);
		free(dl->blks);
		free(dl);
	}
	flist_free(fl.flp, fl.sz);
	free(wbuf);
	blkhash_free(up.stat.blktab);
	cleanup_filesfrom(sess);

	return rc;
}
