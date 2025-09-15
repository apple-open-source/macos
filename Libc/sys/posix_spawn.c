/*
 * Copyright (c) 2006-2012 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * [SPN] Support for _POSIX_SPAWN
 */

#include <spawn.h>
#include <spawn_private.h>
#include <sys/spawn_internal.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>	/* for OPEN_MAX, PATH_MAX */
#include <string.h>	/* for strlcpy() */
#include <paths.h>	/* for _PATH_DEFPATH */
#include <sys/stat.h>	/* for struct stat */

/*
 * posix_spawnp
 *
 * Description:	Create a new process from the process image corresponding to
 *		the supplied 'file' argument and the parent processes path
 *		environment.
 *
 * Parameters:	pid				Pointer to pid_t to receive the
 *						PID of the spawned process, if
 *						successful and 'pid' != NULL
 *		file				Name of image file to spawn
 *		file_actions			spawn file actions object which
 *						describes file actions to be
 *						performed during the spawn
 *		attrp				spawn attributes object which
 *						describes attributes to be
 *						applied during the spawn
 *		argv				argument vector array; NULL
 *						terminated
 *		envp				environment vector array; NULL
 *						terminated
 *
 * Returns:	0				Success
 *		!0				An errno value indicating the
 *						cause of the failure to spawn
 *
 * Notes:	Much of this function is derived from code from execvP() from
 *		exec.c in libc; this common code should be factored out at
 *		some point to prevent code duplication or desynchronization vs.
 *		bug fixes applied to one set of code but not the other.
 */
int
posix_spawnp(pid_t * __restrict pid, const char * __restrict file,
		const posix_spawn_file_actions_t *file_actions,
		const posix_spawnattr_t * __restrict attrp,
		char *const argv[ __restrict], char *const envp[ __restrict])
{
	const char *env_path;
	char path_buf[PATH_MAX];
	char *bp, *np, *op, *p;
	char **memp;
	size_t ln, lp;
	int cnt;
	int err = 0;
	int eacces = 0;
	struct stat sb;

	/* If it's an absolute or relative path name, it's easy. */
	if (strchr(file, '/')) {
		bp = (char *)file;
		env_path = op = NULL;
		goto retry;
	}

	if ((env_path = getenv("PATH")) == NULL)
		env_path = _PATH_DEFPATH;

	bp = path_buf;

	/* If it's an empty path name, fail in the usual POSIX way. */
	if (*file == '\0')
		return (ENOENT);

	op = env_path;
	ln = strlen(file);
	while (op != NULL) {
		np = strchrnul(op, ':');

		/*
		 * It's a SHELL path -- double, leading and trailing colons
		 * mean the current directory.
		 */
		if (np == op) {
			/* Empty component. */
			p = ".";
			lp = 1;
		} else {
			/* Non-empty component. */
			p = op;
			lp = np - op;
		}

		/* Advance to the next component or terminate after this. */
		if (*np == '\0')
			op = NULL;
		else
			op = np + 1;

		/*
		 * If the path is too long complain.  This is a possible
		 * security issue; given a way to make the path too long
		 * the user may spawn the wrong program.
		 */
		if (lp + ln + 2 > sizeof(path_buf)) {
			err = ENAMETOOLONG;
			goto done;
		}
		bcopy(p, path_buf, lp);
		path_buf[lp] = '/';
		bcopy(file, path_buf + lp + 1, ln);
		path_buf[lp + ln + 1] = '\0';

retry:		err = posix_spawn(pid, bp, file_actions, attrp, argv, envp);
		switch (err) {
		case E2BIG:
		case ENOMEM:
		case ETXTBSY:
			goto done;
		case ELOOP:
		case ENAMETOOLONG:
		case ENOENT:
		case ENOTDIR:
			break;
		case ENOEXEC:
			for (cnt = 0; argv[cnt]; ++cnt)
				;

			/*
			 * cnt may be 0 above; always allocate at least
			 * 3 entries so that we can at least fit "sh", bp, and
			 * the NULL terminator.  We can rely on cnt to take into
			 * account the NULL terminator in all other scenarios,
			 * as we drop argv[0].
			 */
			memp = alloca(MAX(3, cnt + 2) * sizeof(char *));
			if (memp == NULL) {
				/* errno = ENOMEM; XXX override ENOEXEC? */
				goto done;
			}
			if (cnt > 0) {
				memp[0] = argv[0];
				memp[1] = bp;
				bcopy(argv + 1, memp + 2, cnt * sizeof(char *));
			} else {
				memp[0] = "sh";
				memp[1] = bp;
				memp[2] = NULL;
			}
			err = posix_spawn(pid, _PATH_BSHELL, file_actions, attrp, memp, envp);
			goto done;
		default:
			/*
			 * EACCES may be for an inaccessible directory or
			 * a non-executable file.  Call stat() to decide
			 * which.  This also handles ambiguities for EFAULT
			 * and EIO, and undocumented errors like ESTALE.
			 * We hope that the race for a stat() is unimportant.
			 */
			if (stat(bp, &sb) != 0)
				break;
			if (err == EACCES) {
				eacces = 1;
				continue;
			}
			goto done;
		}
	}
	if (eacces)
		err = EACCES;
	/* Preserve errno from posix_spawn(3) if it wasn't a PATH search. */
	else if (env_path != NULL)
		err = ENOENT;
done:
	return (err);
}
