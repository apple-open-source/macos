/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2006 The FreeBSD Project. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */


#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <stdlib.h>

#include <pthread.h>

#ifndef __APPLE__
#include "namespace.h"
#include "reentrant.h"
#include "un-namespace.h"
#else
#include <mach-o/dyld_priv.h>

/* from reentrant.h in FreeBSD */
#define thr_keycreate(k, d)     pthread_key_create(k, d)
#define thr_setspecific(k, p)   pthread_setspecific(k, p)
#define thr_getspecific(k)      pthread_getspecific(k)
#define thr_sigsetmask(f, n, o) pthread_sigmask(f, n, o)

#define thr_once(o, i)          pthread_once(o, i)
#define thr_self()              pthread_self()
#define thr_exit(x)             pthread_exit(x)
#define thr_main()              pthread_main_np()
#endif /* ! __APPLE__ */

#include "res_private.h"

#if 1
#undef _res

#ifdef __APPLE__
/*
 * On Apple systems, the main thread's __res_state should come from libSystem to
 * avoid breaking compatibility with older binaries.
 */
extern struct __res_state _res;

static int
use_global_state(void)
{
	static int minos_requires_global = -1;

	if (thr_main() != 0)
		return (1);

	if (minos_requires_global < 0) {
		minos_requires_global =
		    !dyld_program_minos_at_least(dyld_2024_SU_E_os_versions);
	}

	return (minos_requires_global);
}

#else
struct __res_state _res;
#endif 	/* __APPLE__ */

static pthread_key_t res_key;
static pthread_once_t res_init_once = PTHREAD_ONCE_INIT;
static int res_thr_keycreated = 0;

static void
free_res(void *ptr)
{
	res_state statp = ptr;

	if (statp->_u._ext.ext != NULL)
		res_ndestroy(statp);
	free(statp);
}

static void
res_keycreate(void)
{
	res_thr_keycreated = thr_keycreate(&res_key, free_res) == 0;
}

static res_state
res_check_reload(res_state statp)
{
	struct timespec now;
	struct stat sb;
	struct __res_state_ext *ext;

	if ((statp->options & RES_INIT) == 0) {
		return (statp);
	}

	ext = statp->_u._ext.ext;
	if (ext == NULL || ext->reload_period == 0) {
		return (statp);
	}

#ifdef __APPLE__
	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0 ||
#else
	if (clock_gettime(CLOCK_MONOTONIC_FAST, &now) != 0 ||
#endif	/* __APPLE__ */
	    (now.tv_sec - ext->conf_stat) < ext->reload_period) {
		return (statp);
	}

	ext->conf_stat = now.tv_sec;
	if (stat(_PATH_RESCONF, &sb) == 0 &&
#ifdef __APPLE__
	    (sb.st_mtimespec.tv_sec  != ext->conf_mtim.tv_sec ||
	     sb.st_mtimespec.tv_nsec != ext->conf_mtim.tv_nsec)) {
#else
	    (sb.st_mtim.tv_sec  != ext->conf_mtim.tv_sec ||
	     sb.st_mtim.tv_nsec != ext->conf_mtim.tv_nsec)) {
#endif	/* __APPLE__ */
		statp->options &= ~RES_INIT;
	}

	return (statp);
}
#endif
res_state
__res_state(void)
{
#if 1
	res_state statp;

#ifdef __APPLE__
	if (use_global_state() != 0)
#else
	if (thr_main() != 0)
#endif
		return res_check_reload(&_res);

	if (thr_once(&res_init_once, res_keycreate) != 0 ||
	    !res_thr_keycreated)
		return (&_res);

	statp = thr_getspecific(res_key);
	if (statp != NULL)
		return res_check_reload(statp);
	statp = calloc(1, sizeof(*statp));
	if (statp == NULL)
		return (&_res);
#ifdef __BIND_RES_TEXT
	statp->options = RES_TIMEOUT;			/* Motorola, et al. */
#endif
	if (thr_setspecific(res_key, statp) == 0)
		return (statp);
	free(statp);
#endif
printf("%s:%d debugging time\n", __func__, __LINE__);
	return (&_res);
}
