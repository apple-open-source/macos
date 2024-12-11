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

#include "extern.h"

/*
 * In practice we're only ever mapping < 10 files at a time, so no need to go
 * overkill.
 */
static int mapped_files;
static struct sigaction sigbus_init;

struct fmap {
	void		*map;
	size_t		 mapsz;
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

struct fmap *
fmap_open(int fd, size_t sz, int prot)
{
	struct fmap *fm;

	fm = malloc(sizeof(*fm));
	if (fm == NULL)
		return NULL;

	fm->mapsz = sz;
	fm->map = mmap(NULL, sz, prot, MAP_SHARED, fd, 0);
	if (fm->map == MAP_FAILED) {
		int serrno = errno;

		free(fm);
		errno = serrno;
		return NULL;
	}

	/*
	 * We'll setup the signal handler on the first file mapped, then the
	 * caller will trap/untrap around data accesses to configure the trap a
	 * little more lightly.
	 *
	 * We'll catch SIGBUS even if it's ignored coming in so that we can do
	 * some sensible detection of file truncation, but we will never force
	 * the signal to be handled and abort if we can't handle it here.
	 */
	if (mapped_files++ == 0) {
		struct sigaction act = { .sa_flags = SA_SIGINFO };

		sigemptyset(&act.sa_mask);
		act.sa_sigaction = fmap_sigbus_handler;
		sigaction(SIGBUS, &act, &sigbus_init);
	}

	return fm;
}

void *
fmap_data(struct fmap *fm, size_t offset)
{

	if (fm == NULL)
		return (NULL);

	return &fm->map[offset];
}

size_t
fmap_size(struct fmap *fm)
{

	if (fm == NULL)
		return (0);

	return fm->mapsz;
}

void
fmap_close(struct fmap *fm)
{

	if (fm == NULL)
		return;

	/*
	 * We want all callers to be very explicit about when they trap/untrap,
	 * so consider it a leak if we're still trapped at fmap_close() time.
	 */
	assert(fmap_trapped != fm && fmap_trapped_prev != fm);

	munmap(fm->map, fm->mapsz);
	free(fm);

	if (--mapped_files == 0)
		sigaction(SIGBUS, &sigbus_init, NULL);
}
