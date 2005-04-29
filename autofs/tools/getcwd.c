
#include <stdio.h>
#include <unistd.h>
#include <sys/param.h>

char *mygetcwd(char *pt, size_t size);

int
main(void)
{
	char buf[MAXPATHLEN];

	if (mygetcwd(buf, MAXPATHLEN) == NULL)
		perror("getcwd");
	printf("buf: %s\n", buf);
	return (0);
}

#include <sys/param.h>
#include <sys/stat.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define	ISDOT(dp) \
	(dp->d_name[0] == '.' && (dp->d_name[1] == '\0' || \
	    (dp->d_name[1] == '.' && dp->d_name[2] == '\0')))

	extern int __getcwd(char *, size_t);

char *
mygetcwd(pt, size)
	char *pt;
	size_t size;
{
	struct dirent *dp;
	DIR *dir = NULL;
	dev_t dev;
	ino_t ino;
	int first;
	char *bpt, *bup;
	struct stat s;
	dev_t root_dev;
	ino_t root_ino;
	size_t ptsize, upsize;
	int save_errno;
	char *ept, *eup, *up, c;

	/*
	 * If no buffer specified by the user, allocate one as necessary.
	 * If a buffer is specified, the size has to be non-zero.  The path
	 * is built from the end of the buffer backwards.
	 */
	if (pt) {
		ptsize = 0;
		if (!size) {
			errno = EINVAL;
			return (NULL);
		}
		if (size == 1) {
			errno = ERANGE;
			return (NULL);
		}
		ept = pt + size;
	} else {
		if ((pt = malloc(ptsize = 1024 - 4)) == NULL)
			return (NULL);
		ept = pt + ptsize;
	}
#if 0
#if	!defined(__NETBSD_SYSCALLS)
	if (__getcwd(pt, ept - pt) == 0) {
		if (*pt != '/') {
			bpt = pt;
			ept = pt + strlen(pt) - 1;
			while (bpt < ept) {
				c = *bpt;
				*bpt++ = *ept;
				*ept-- = c;
			}
		}
		return (pt);
	}
#endif
#endif
	bpt = ept - 1;
	*bpt = '\0';

	/*
	 * Allocate bytes (1024 - malloc space) for the string of "../"'s.
	 * Should always be enough (it's 340 levels).  If it's not, allocate
	 * as necessary.  Special case the first stat, it's ".", not "..".
	 */
	if ((up = malloc(upsize = 1024 - 4)) == NULL)
		goto err;
	eup = up + MAXPATHLEN;
	bup = up;
	up[0] = '.';
	up[1] = '\0';

	/* Save root values, so know when to stop. */
	if (stat("/", &s))
		goto err;
	root_dev = s.st_dev;
	root_ino = s.st_ino;

	errno = 0;			/* XXX readdir has no error return. */

	for (first = 1;; first = 0) {
		/* Stat the current level. */
		if (lstat(up, &s))
			goto err;

		/* Save current node values. */
		ino = s.st_ino;
		dev = s.st_dev;

		/* Check for reaching root. */
		if (root_dev == dev && root_ino == ino) {
			*--bpt = '/';
			/*
			 * It's unclear that it's a requirement to copy the
			 * path to the beginning of the buffer, but it's always
			 * been that way and stuff would probably break.
			 */
			bcopy(bpt, pt, ept - bpt);
			free(up);
			fprintf(stderr, "returning: %s\n", pt);
			return (pt);
		}

		/*
		 * Build pointer to the parent directory, allocating memory
		 * as necessary.  Max length is 3 for "../", the largest
		 * possible component name, plus a trailing NUL.
		 */
		if (bup + 3  + MAXNAMLEN + 1 >= eup) {
			if ((up = reallocf(up, upsize *= 2)) == NULL)
				goto err;
			bup = up;
			eup = up + upsize;
		}
		*bup++ = '.';
		*bup++ = '.';
		*bup = '\0';

		fprintf(stderr, "up: %s\n", up);
		/* Open and stat parent directory. */
		if (!(dir = opendir(up)) || fstat(dirfd(dir), &s))
			goto err;

		/* Add trailing slash for next directory. */
		*bup++ = '/';
		*bup = '\0';

		/*
		 * If it's a mount point, have to stat each element because
		 * the inode number in the directory is for the entry in the
		 * parent directory, not the inode number of the mounted file.
		 */
		fprintf(stderr, "%d\n", __LINE__);
		save_errno = 0;
		if (s.st_dev == dev) {
			for (;;) {
				if (!(dp = readdir(dir)))
					goto notfound;
				if (dp->d_fileno == ino)
					break;
			}
		} else
			for (;;) {
				if (!(dp = readdir(dir)))
					goto notfound;
				if (ISDOT(dp))
					continue;
				bcopy(dp->d_name, bup, dp->d_namlen + 1);

				/* Save the first error for later. */
				if (lstat(up, &s)) {
					if (!save_errno)
						save_errno = errno;
					errno = 0;
					continue;
				}
				if (s.st_dev == dev && s.st_ino == ino)
					break;
			}

		/*
		 * Check for length of the current name, preceding slash,
		 * leading slash.
		 */
		fprintf(stderr, "namelen = %d, name = '%s'\n", dp->d_namlen,
		    dp->d_name);
		if (bpt - pt < dp->d_namlen + (first ? 1 : 2)) {
			size_t len, off;

			fprintf(stderr, "here...\n");
			if (!ptsize) {
				errno = ERANGE;
				goto err;
			}
			off = bpt - pt;
			len = ept - bpt;
			if ((pt = reallocf(pt, ptsize *= 2)) == NULL)
				goto err;
			bpt = pt + off;
			ept = pt + ptsize;
			bcopy(bpt, ept - len, len);
			bpt = ept - len;
		}
		if (!first)
			*--bpt = '/';
		bpt -= dp->d_namlen;
		bcopy(dp->d_name, bpt, dp->d_namlen);
		fprintf(stderr, "%d, bpt: %p '%s'\n", __LINE__, bpt, bpt);
		(void) closedir(dir);
		dir = NULL;

		/* Truncate any file name. */
		*bup = '\0';
	}

notfound:
	/*
	 * If readdir set errno, use it, not any saved error; otherwise,
	 * didn't find the current directory in its parent directory, set
	 * errno to ENOENT.
	 */
	if (!errno)
		errno = save_errno ? save_errno : ENOENT;
	/* FALLTHROUGH */
err:
	save_errno = errno;

	if (ptsize)
		free(pt);
	if (dir)
		(void) closedir(dir);
	free(up);

	errno = save_errno;
	return (NULL);
}
