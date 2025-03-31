/*
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

#include <sys/mman.h>
#include <assert.h>
#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

#include "extern.h"

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

/*
 * In practice we're only ever mapping < 10 files at a time, so no need to go
 * overkill.
 */
static int mapped_files;
static struct sigaction sigbus_init;

struct fmap {
	void		*map;
	unsigned char	*buf;
	int		 fd;
	size_t		 mapsz;
	size_t		 bufsz;		/* Full size of the buffer. */
	size_t		 datasz;	/* Just the valid portion of the buf. */
	off_t		 dataoff;	/* Offset into the file. */
	enum fmap_type	 ftype;
};

volatile struct fmap *fmap_trapped, *fmap_trapped_prev;
sigjmp_buf fmap_signal_env;

static void
fmap_reraise(void)
{

	if (sigbus_init.sa_handler == SIG_IGN)
		return;

	/* Reset disposition, then raise(3) it. */
	sigaction(SIGBUS, &sigbus_init, NULL);
	raise(SIGBUS);
}

static void
fmap_sigbus_handler(int sig, siginfo_t *siginfo,
    void *uap __attribute__((__unused__)))
{
	struct fmap *fm;
	void *uva;

	assert(sig == SIGBUS);

	/*
	 * We'll very carefully make sure that we got hit by the bus from an
	 * access to the segment we're trapping; otherwise, we just re-raise it
	 * for the operator to deal with.
	 */
	fm = (void *)fmap_trapped;
	uva = siginfo->si_addr;
	if (fm == NULL || uva < fm->map || uva >= fm->map + fm->mapsz) {
		fmap_reraise();

		/*
		 * It might be ignored, in which case we shouldn't do anything
		 * at all.
		 */
		return;
	}

	siglongjmp(fmap_signal_env, sig);
}

static bool
fmap_mmap_allowed(void)
{
#if defined(__APPLE__) && TARGET_OS_WATCH
	static int allowed = -1;

	if (allowed == -1)
		allowed = (geteuid() == 0);
	return !!allowed;
#endif
	return true;
}

static enum fmap_type
fmap_env_type(void)
{
	const char *envp;

	/*
	 * Note that we'll still allow explicit requests for mmap even if the
	 * current platform disables it.  We'll assume the caller knows what
	 * they're doing and are intending to test something, or they've made
	 * it feasible to use mmap.
	 */
	envp = getenv("RSYNC_FMAP_TYPE");
	if (envp != NULL && *envp != '\0') {
		if (strcmp(envp, "mmap") == 0)
			return FT_MMAP;
		if (strcmp(envp, "bufio") == 0)
			return FT_BUFIO;
	}

	if (!fmap_mmap_allowed())
		return FT_BUFIO;
	return FT_MMAP;
}

static bool
fmap_open_mmap(struct fmap *fm, const char *path, int fd, size_t sz)
{
	fm->mapsz = sz;
	fm->map = mmap(NULL, sz, PROT_READ, MAP_SHARED, fd, 0);
	if (fm->map == MAP_FAILED) {
		int serrno = errno;

		/*
		 * If we got ENOMEM, we'll fallback to bufio
		 */

		if (errno == ENOMEM)
			WARNX1("%s: mmap failed, fallback to bufio", path);
		else
			ERR("%s: mmap", path);

		errno = serrno;
		return false;
	}

	/*
	 * We'll setup the signal handler on the first file mapped, then
	 * the caller will trap/untrap around data accesses to configure
	 * the trap a little more lightly.
	 *
	 * We'll catch SIGBUS even if it's ignored coming in so that we
	 * can do some sensible detection of file truncation, but we
	 * will never force the signal to be handled and abort if we
	 * can't handle it here.
	 */
	if (mapped_files++ == 0) {
		struct sigaction act = { .sa_flags = SA_SIGINFO };

		sigemptyset(&act.sa_mask);
		act.sa_sigaction = fmap_sigbus_handler;
		if (sigaction(SIGBUS, &act, &sigbus_init) != 0) {
			int serrno = errno;

			ERR("sigaction");
			errno = serrno;
			return false;
		}
	}

	return true;
}

/*
 * Open a mapping of the given fd.  Note that path is only taken for diagnostic
 * output.
 */
struct fmap *
fmap_open(const char *path, int fd, size_t sz)
{
	struct fmap *fm;

	fm = calloc(1, sizeof(*fm));
	if (fm == NULL)
		return NULL;

	fm->fd = fd;
	fm->ftype = fmap_env_type();

	assert(fm->ftype != FT_NULL);
	switch (fm->ftype) {
	case FT_MMAP:
		if (fmap_open_mmap(fm, path, fd, sz))
			break;

		/*
		 * If we got back an ENOMEM, there's a chance we can still
		 * succeed.  We'll fallback to bufio, which has much lower
		 * memory requirements by comparison.
		 */
		if (errno != ENOMEM) {
			int serrno = errno;

			free(fm);
			errno = serrno;
			return NULL;
		}

		fm->ftype = FT_BUFIO;

		/* Zapping the mmap state is not strictly required. */
		fm->mapsz = 0;
		fm->map = NULL;

		/* FALLTHROUGH */
	case FT_BUFIO:
		/* Reposition the fd to be sure we know where we're at. */
		fm->fd = fd;
		break;
	default:
		assert(0 && "Unknown type found... unreachable");
		break;
	}

	return fm;
}

static bool
fmap_buf_slide_read(struct fmap *fm, off_t bufpos, off_t offset, size_t reqsz)
{
	size_t targetsz, totalsz;

	assert(bufpos + reqsz <= fm->bufsz);
	targetsz = fm->bufsz - bufpos;
	totalsz = 0;
	while (totalsz < targetsz) {
		ssize_t readsz;

		readsz = pread(fm->fd, fm->buf + bufpos, targetsz - totalsz,
		    offset);
		if (readsz == -1 && errno == EINTR)
			continue;
		if (readsz == -1) {
			int serrno = errno;

			ERR("pread");
			errno = serrno;
			return false;
		}

		/* EOF that we weren't expecting; file was truncated. */
		if (readsz == 0) {
			assert(totalsz != targetsz);

			/* Not premature: just what the caller wanted. */
			if (totalsz >= reqsz)
				break;

			fm->dataoff = 0;
			fm->datasz = 0;

			siglongjmp(fmap_signal_env, SIGBUS);

			/* UNREACHABLE */
			return false;
		}

		fm->datasz += readsz;
		totalsz += readsz;
		offset += readsz;
		bufpos += readsz;
	}

	return true;
}

static bool
fmap_buf_slide(struct fmap *fm, off_t offset, size_t datasz)
{
	off_t bufpos = 0;

	assert(fmap_trapped != NULL);
	assert(fm->ftype == FT_BUFIO);

	if (offset > fm->dataoff && offset < fm->dataoff + fm->datasz) {
		size_t clip = offset - fm->dataoff;

		/* Clip the first part we won't use, then adjust. */
		memmove(fm->buf, fm->buf + clip, fm->datasz - clip);

		fm->datasz -= clip;
		fm->dataoff += clip;
	}

	if (offset == fm->dataoff) {
		if (datasz <= fm->datasz)
			return true;

		/* We can get away with just reading the trailing portion. */
		bufpos = fm->datasz;
		datasz -= fm->datasz;
		offset += fm->datasz;
	} else {
		/* No overlap, buffer is completely invalid.  Reset it. */
		fm->dataoff = offset;
		fm->datasz = 0;
	}

	return fmap_buf_slide_read(fm, bufpos, offset, datasz);
}

static bool
fmap_buf_resize(struct fmap *fm, size_t datasz)
{
	char *lbuf;

	if (fm->bufsz >= datasz)
		return true;

	lbuf = realloc(fm->buf, datasz);
	if (lbuf == NULL) {
		int serrno = errno;

		ERR("realloc");
		errno = serrno;
		return false;
	}

	fm->buf = lbuf;
	fm->bufsz = datasz;
	return true;
}

void *
fmap_data(struct fmap *fm, off_t offset, size_t datasz)
{

	if (fm == NULL)
		return NULL;

	assert(fm->ftype != FT_NULL);
	switch (fm->ftype) {
	case FT_MMAP:
#if defined(__APPLE__) && !defined(NDEBUG)
		/* Temporary diagnostics */
		if (offset + datasz > fm->mapsz) {
			ERRX1("Invalid access; mapsz=%zu, [%llu, %llu) requested",
			    fm->mapsz, offset, offset + datasz);
		}
#endif

		assert(offset + datasz <= fm->mapsz);
		return &fm->map[offset];
	case FT_BUFIO:
		if (!fmap_buf_resize(fm, datasz))
			return NULL;

		if (!fmap_buf_slide(fm, offset, datasz))
			return NULL;

		return fm->buf;
	default:
		assert(0 && "Unknown type found... unreachable");
	}

	return NULL;
}

size_t
fmap_size(struct fmap *fm)
{

	if (fm == NULL)
		return 0;

	return fm->mapsz;
}

enum fmap_type
fmap_type(struct fmap *fm)
{

	if (fm == NULL)
		return FT_NULL;

	return fm->ftype;
}

void
fmap_close(struct fmap *fm)
{

	if (fm == NULL)
		return;

	/*
	 * We want all callers to be very explicit about when they
	 * trap/untrap, so consider it a leak if we're still trapped a
	 * fmap_close() time.
	 */
	assert(fmap_trapped != fm && fmap_trapped_prev != fm);

	assert(fm->ftype != FT_NULL);
	switch (fm->ftype) {
	case FT_MMAP:

		munmap(fm->map, fm->mapsz);
		if (--mapped_files == 0)
			(void)sigaction(SIGBUS, &sigbus_init, NULL);
		break;
	case FT_BUFIO:
		free(fm->buf);
		break;
	default:
		assert(0 && "Unknown type found... unreachable");
		break;
	}

	free(fm);
}
