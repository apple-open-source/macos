/*	$NetBSD: t_getcwd.c,v 1.3 2011/07/27 05:04:11 jruoho Exp $ */

/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jukka Ruohonen.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__RCSID("$NetBSD: t_getcwd.c,v 1.3 2011/07/27 05:04:11 jruoho Exp $");

#include <sys/param.h>
#include <sys/stat.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <darwintest.h>
#include <darwintest_utils.h>

T_DECL(getcwd_err, "Test error conditions in getcwd(3)")
{
	char buf[MAXPATHLEN];

	errno = 0;

	T_ASSERT_NULL(getcwd(buf, 0), NULL);
	T_ASSERT_EQ(errno, EINVAL, NULL);
}

T_DECL(getcwd_fts, "A basic test of getcwd(3)")
{
	char buf[MAXPATHLEN];
	char *args[2] = {"/System", NULL};
	FTSENT *ftse;
	FTS *fts;
	int ops;
	short depth;

	/*
	 * Do not traverse too deep
	 */
	depth = 2;

	/*
	 * Test that getcwd(3) works with basic
	 * system directories. Note that having
	 * no FTS_NOCHDIR specified should ensure
	 * that the current directory is visited.
	 */
	ops = FTS_PHYSICAL | FTS_NOSTAT;
	fts = fts_open(args, ops, NULL);

	T_ASSERT_NOTNULL(fts, NULL);

	while ((ftse = fts_read(fts)) != NULL) {

		if (ftse->fts_level < 1)
			continue;

		if (ftse->fts_level > depth) {
			(void)fts_set(fts, ftse, FTS_SKIP);
			continue;
		}

		switch(ftse->fts_info) {

		case FTS_DP:
			(void)memset(buf, 0, sizeof(buf));
			T_WITH_ERRNO;
			T_ASSERT_NOTNULL(getcwd(buf, sizeof(buf)), NULL);
			T_LOG("ftse->fts_path: %s", ftse->fts_path);
			T_ASSERT_NOTNULL(strstr(ftse->fts_path, buf), NULL);
			break;

		default:
			break;
		}
	}

	(void)fts_close(fts);
}

#ifdef __APPLE__
/* Not a converted NetBSD test. */
T_DECL(getcwd_nested, "Test getcwd(3) with large directory nestings")
{
	/*
	 * 768 is more than sufficient to guarantee we would have ended up
	 * breaking getcwd(3).  We generally just needed to trigger the
	 * reallocation ("bup + 3  + MAXNAMLEN + 1 >= eup"), and we generate
	 * three bytes ("../") per nested level.  768 deep from where we're
	 * executed seems like a reasonable expectation; previous failures were
	 * observed at 506 levels deep from the root of the filesystem.
	 */
	int nesting = 768;
	int dirfd;
	int ret;
	char *tmp_path;

	T_ASSERT_POSIX_SUCCESS(asprintf(&tmp_path, "%s/%s-XXXXXX", dt_tmpdir(),
	    T_NAME), NULL);
	T_ASSERT_NOTNULL(mktemp(tmp_path), NULL);
	T_ASSERT_POSIX_SUCCESS(mkdir(tmp_path, 0777), NULL);
	dirfd = open(tmp_path, O_RDONLY | O_DIRECTORY);
	free(tmp_path);

	for (int i = 0; i < nesting; ++i) {
		char buf[MAXNAMLEN];
		int fd;

		snprintf(buf, sizeof(buf), "lvl_%d", i);
		ret = mkdirat(dirfd, buf, S_IRWXU);
		if (ret != 0) {
			T_FAIL("mkdirat(%s) failed, errno %d", buf, errno);
			return;
		}

		fd = openat(dirfd, buf, O_RDONLY | O_DIRECTORY);
		if (fd < 0) {
			T_FAIL("openat(%s) failed, errno %d", buf, errno);
			return;
		}

		if (dirfd >= 0)
			close(dirfd);

		dirfd = fd;
		ret = fchdir(dirfd);
		if (ret != 0) {
			T_FAIL("fchdir() failed, errno %d", errno);
			return;
		}

		char *cwd = getcwd(NULL, 0);
		T_ASSERT_NOTNULL(cwd, NULL);
		free(cwd);
	}
}
#endif
