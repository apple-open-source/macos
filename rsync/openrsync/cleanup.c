/*
 * Copyright (c) 2023 Klara, Inc.
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

#include <sys/param.h>
#include <sys/wait.h>

#include <assert.h>
#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#include "extern.h"

typedef void (*rsync_cleanup_fn)(struct cleanup_ctx *);

static void cleanup_run_impl(int code, bool signal_ctx);

static void rsync_cleanup_reap_child(struct cleanup_ctx *);
static void rsync_cleanup_download(struct cleanup_ctx *);
static void rsync_cleanup_child(struct cleanup_ctx *);

/*
 * For convenience, we'll define all of our cleanup steps in order at the end of
 * this file to make it easier to follow along the steps we'll take.
 */
static rsync_cleanup_fn cleanup_process[] = {
	&rsync_cleanup_reap_child,
	&rsync_cleanup_download,
	&rsync_cleanup_child,
	NULL,
};

static struct cleanup_ctx {
	struct sess		*sess;
	struct download		*dl;
	struct fargs		*fargs;

	rsync_cleanup_fn	*cleanup_func;

	pid_t			 child_pid;
	volatile sig_atomic_t	 exitstatus;
	int			 signal;
	int			 depth;
	int			 hold;
	sigset_t		 holdmask;
} cleanup_ctx_storage = {
	.cleanup_func = &cleanup_process[0],
	.exitstatus = -1,
};

/*
 * We need to be able to access our context from a signal context and there can
 * only really reasonably be one anyways, so we allocated it globally above and
 * just use that.  Everything outside of main() and cleanup.c won't be aware of
 * this implementation detail, though.
 */
struct cleanup_ctx *cleanup_ctx = &cleanup_ctx_storage;

static void
cleanup_signaled(int signo)
{
	int code;

	switch (signo) {
	case SIGUSR1:
		code = ERR_SIGUSR1;
		break;
	case SIGUSR2:
		_exit(0);
		break;
	default:
		code = ERR_SIGNAL;
		break;
	}

	cleanup_run_impl(code, true);
}

/*
 * Hold any signals that may cause us to need a graceful cleanup until the hold
 * is released.  Used if we're freeing resources during normal execution so that
 * we don't end up in an unpredictable state when interrupted at just the right
 * time.
 *
 * We technically allow these sections to be nested, because it adds very little
 * complexity to track.
 *
 * Note that cleanup_hold/cleanup_release may be called from a signal context,
 * so we must only do async-signal-safe bits here.
 */
void
cleanup_hold(struct cleanup_ctx *ctx)
{

	/* No underflow */
	assert(ctx->hold >= 0);
	if (++ctx->hold == 1) {
		sigset_t set;

		sigemptyset(&set);
		sigaddset(&set, SIGUSR1);
		sigaddset(&set, SIGUSR2);
		sigaddset(&set, SIGHUP);
		sigaddset(&set, SIGINT);
		sigaddset(&set, SIGTERM);

		sigprocmask(SIG_BLOCK, &set, &ctx->holdmask);

		/*
		 * We don't actually know that these were unblocked when we
		 * started, so just blindly remove them from the holdmask to
		 * avoid them being blocked once we resume normal execution.
		 */
		sigdelset(&ctx->holdmask, SIGUSR1);
		sigdelset(&ctx->holdmask, SIGUSR2);
		sigdelset(&ctx->holdmask, SIGHUP);
		sigdelset(&ctx->holdmask, SIGINT);
		sigdelset(&ctx->holdmask, SIGTERM);
	}
}

/*
 * Releases the hold previously taken, unblocking any signals that we may have
 * previously blocked to avoid being preempted by user interruption.
 */
void
cleanup_release(struct cleanup_ctx *ctx)
{

	assert(ctx->hold > 0);
	if (--ctx->hold == 0)
		sigprocmask(SIG_SETMASK, &ctx->holdmask, NULL);
}

/*
 * Bare minimum needed for cleanup -- the session gives us options, if we're the
 * client we'll later pick up some fargs that we'll use for additional
 * decisions.
 *
 * cleanup_init() will block the pertinent signals until we're sure we're
 * running with enough context to make some useful decisions about what needs to
 * be done.
 */
void
cleanup_init(struct cleanup_ctx *ctx)
{

	/*
	 * We don't currently mask the other termination signals automatically,
	 * because we may choose to do something else if we get, e.g., a SIGHUP
	 * after a SIGINT.  We'll just cope with that possibility in
	 * cleanup_run and do something reasonably sane.
	 */
	signal(SIGUSR1, cleanup_signaled);
	signal(SIGUSR2, cleanup_signaled);
	signal(SIGHUP, cleanup_signaled);
	signal(SIGINT, cleanup_signaled);
	signal(SIGTERM, cleanup_signaled);

	cleanup_hold(ctx);
}

void
cleanup_run(int code)
{

	cleanup_run_impl(code, false);
}

void
cleanup_set_args(struct cleanup_ctx *ctx, struct fargs *fargs)
{

	ctx->fargs = fargs;
}

void
cleanup_set_child(struct cleanup_ctx *ctx, pid_t pid)
{

	/* No process groups here. */
	assert(pid >= 0);
	ctx->child_pid = pid;
}

void
cleanup_set_session(struct cleanup_ctx *ctx, struct sess *sess)
{

	ctx->sess = sess;
}

void
cleanup_set_download(struct cleanup_ctx *ctx, struct download *dl)
{

	ctx->dl = dl;
}

static void
cleanup_run_impl(int code, bool signal_ctx)
{
	struct cleanup_ctx *ctx = cleanup_ctx;
	rsync_cleanup_fn *func;
	int depth;

	cleanup_hold(ctx);

	/*
	 * We'll use SIGUSR1 to kill off any remaining children; ignore it now.
	 */
	signal(SIGUSR1, SIG_IGN);

	depth = ctx->depth++;

	/*
	 * We're not blocking any of our cleanup signals, so it's not
	 * unreasonable to believe that we could end up here again.  We also
	 * don't want to impose any weird restrictions on cleanup functions, so
	 * this is built for the possibility of recursion later even if we won't
	 * currently recurse into it.
	 */
	if (ctx->exitstatus == -1)
		ctx->exitstatus = code;

	/*
	 * We'll make sure we can't get interrupted again while we fetch a
	 * clean copy of the current cleanup callback and advance it by holding
	 * cleanup around the condition.  We do this to avoid running the same
	 * step twice in the face of continued signals.  In this setup, if we
	 * get signaled again then we could end up running some steps out of
	 * order as we'll generally hold off on exit() until it's all done.
	 */
	while (*ctx->cleanup_func != NULL) {
		func = ctx->cleanup_func++;
		cleanup_release(ctx);

		(*func)(ctx);

		cleanup_hold(ctx);
	}

	ctx->depth--;

	/*
	 * We'll try to give as much opportunity for the cleanup process to
	 * actually complete as we can.  At depth > 0, we've either recursed or
	 * we're running in a signal context.  In either case, we'll return here
	 * eventually and exit.
	 */
	if (depth > 0) {
		cleanup_release(ctx);
		return;
	}

	/*
	 * We have to reload ctx->exitstatus here because any of the handlers
	 * could legitimately override it.
	 */
	if (signal_ctx)
		_exit(ctx->exitstatus);
	else
		exit(ctx->exitstatus);
}

/*
 * Reaps the server if we spawned it, inheriting its exit code as needed.
 */
static void
rsync_cleanup_reap_child(struct cleanup_ctx *ctx)
{
	pid_t child, pid;
	int st;

	if ((child = ctx->child_pid) == 0)
		return;

	while ((pid = waitpid(child, &st, WNOHANG)) > 0) {
		/* We only fork once... */
		if (pid != child)
			continue;

		/*
		 * rsync will inexplicably inherit a higher-level exit status
		 * from the child; let's be compatible.
		 */
		st = WEXITSTATUS(st);
		if (st > ctx->exitstatus)
			ctx->exitstatus = st;
	}
}

/*
 * Cleans up any in-progress download, which includes --partial handling.
 */
static void
rsync_cleanup_download(struct cleanup_ctx *ctx)
{

	download_interrupted(ctx->sess, ctx->dl);
}

/*
 * Kills off any remaining children.
 */
static void
rsync_cleanup_child(struct cleanup_ctx *ctx)
{

	/*
	 * Not currently called on clean exits, but we may apply this
	 * somewhat conservatively later...
	 */
	if (ctx->exitstatus == 0)
		return;

	kill(0, SIGUSR1);
}
