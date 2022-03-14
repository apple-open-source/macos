/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)utils.c	8.3 (Berkeley) 4/1/94";
#endif
#endif /* not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/acl.h>
#include <sys/param.h>
#include <sys/stat.h>
#ifdef VM_AND_BUFFER_CACHE_SYNCHRONIZED
#include <sys/mman.h>
#endif

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>
#include <locale.h>

#ifdef __APPLE__
#include <sys/time.h>
#include <copyfile.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/attr.h>
#include <sys/clonefile.h>

#define	st_atim	st_atimespec
#define	st_mtim	st_mtimespec
#endif /* __APPLE__ */

#include "extern.h"
#define	cp_pct(x, y)	((y == 0) ? 0 : (int)(100.0 * (x) / (y)))

/*
 * Memory strategy threshold, in pages: if physmem is larger then this, use a 
 * large buffer.
 */
#define PHYSPAGES_THRESHOLD (32*1024)

/* Maximum buffer size in bytes - do not allow it to grow larger than this. */
#define BUFSIZE_MAX (2*1024*1024)

/*
 * Small (default) buffer size in bytes. It's inefficient for this to be
 * smaller than MAXPHYS.
 */
#define BUFSIZE_SMALL (MAXPHYS)

static ssize_t
copy_fallback(int from_fd, int to_fd, char *buf, size_t bufsize)
{
	ssize_t rcount, wresid, wcount = 0;
	char *bufp;

	rcount = read(from_fd, buf, bufsize);
	if (rcount <= 0)
		return (rcount);
	for (bufp = buf, wresid = rcount; ; bufp += wcount, wresid -= wcount) {
		wcount = write(to_fd, bufp, wresid);
		if (wcount <= 0)
			break;
		if (wcount >= (ssize_t)wresid)
			break;
	}
	return (wcount < 0 ? wcount : rcount);
}

int
copy_file(const FTSENT *entp, int dne)
{
	static char *buf = NULL;
	static size_t bufsize;
	struct stat *fs;
	ssize_t rcount;
	off_t wtotal;
	int ch, checkch, from_fd, rval, to_fd;
	char resp[] = {'\0', '\0'};
#ifdef VM_AND_BUFFER_CACHE_SYNCHRONIZED
	char *p;
#endif
#ifdef __APPLE__
	int use_copy_file_range = 0;
#else
	int use_copy_file_range = 1;
#endif
	mode_t mode = 0;
	struct stat to_stat;

	from_fd = to_fd = -1;
	if (!lflag && !sflag &&
	    (from_fd = open(entp->fts_path, O_RDONLY, 0)) == -1) {
		warn("%s", entp->fts_path);
		return (1);
	}

	fs = entp->fts_statp;

	/*
	 * If the file exists and we're interactive, verify with the user.
	 * If the file DNE, set the mode to be the from file, minus setuid
	 * bits, modified by the umask; arguably wrong, but it makes copying
	 * executables work right and it's been that way forever.  (The
	 * other choice is 666 or'ed with the execute bits on the from file
	 * modified by the umask.)
	 */
	if (!dne) {
#define YESNO "(y/n [n]) "
		if (nflag) {
			if (vflag)
				printf("%s not overwritten\n", to.p_path);
			rval = 1;
			goto done;
		} else if (iflag) {
			(void)fprintf(stderr, "overwrite %s? %s", 
			    to.p_path, YESNO);

			/* Load user specified locale */
			setlocale(LC_MESSAGES, "");

			checkch = ch = getchar();
			while (ch != '\n' && ch != EOF)
				ch = getchar();

			/* only care about the first character */
			resp[0] = checkch;

			if (rpmatch(resp) != 1) {
				(void)fprintf(stderr, "not overwritten\n");
				rval = 1;
				goto done;
			}
		}

#ifdef __APPLE__
		if (cflag) {
			(void)unlink(to.p_path);
			int error = clonefile(entp->fts_path, to.p_path, 0);
			if (error)
				warn("%s: clonefile failed", to.p_path);
			(void)close(from_fd);
			return error == 0 ? 0 : 1;
		}
#endif

		if (unix2003_compat) {
		    /* first try to overwrite existing destination file name */
		    to_fd = open(to.p_path, O_WRONLY | O_TRUNC, 0);
		    if (to_fd == -1) {
			if (fflag) {
			    /* Only if it fails remove file and create a new one */
			    (void)unlink(to.p_path);
			    to_fd = open(to.p_path, O_WRONLY | O_TRUNC | O_CREAT,
					 fs->st_mode & ~(S_ISUID | S_ISGID));
			}
		    }
		} else if (fflag) {
			/*
			 * Remove existing destination file name create a new
			 * file.
			 */
			(void)unlink(to.p_path);
			if (!lflag && !sflag) {
				to_fd = open(to.p_path,
				    O_WRONLY | O_TRUNC | O_CREAT,
				    fs->st_mode & ~(S_ISUID | S_ISGID));
			}
		} else if (!lflag && !sflag) {
			/* Overwrite existing destination file name. */
			to_fd = open(to.p_path, O_WRONLY | O_TRUNC, 0);
		}
	} else if (!lflag && !sflag) {
#ifdef __APPLE__
		if (cflag) {
			int error = clonefile(entp->fts_path, to.p_path, 0);
			if (error)
				warn("%s: clonefile failed", to.p_path);
			(void)close(from_fd);
			return error == 0 ? 0 : 1;
		}
#endif

		to_fd = open(to.p_path, O_WRONLY | O_TRUNC | O_CREAT,
		    fs->st_mode & ~(S_ISUID | S_ISGID));
	}

	if (!lflag && !sflag && to_fd == -1) {
		warn("%s", to.p_path);
		rval = 1;
		goto done;
	}

	rval = 0;

#ifdef __APPLE__
       if (S_ISREG(fs->st_mode)) {
               struct statfs sfs;

               /*
                * Pre-allocate blocks for the destination file if it
                * resides on Xsan.
                */
               if (fstatfs(to_fd, &sfs) == 0 &&
                   strcmp(sfs.f_fstypename, "acfs") == 0) {
                       fstore_t fst;

                       fst.fst_flags = 0;
                       fst.fst_posmode = F_PEOFPOSMODE;
                       fst.fst_offset = 0;
                       fst.fst_length = fs->st_size;

                       (void) fcntl(to_fd, F_PREALLOCATE, &fst);
               }
       }
#endif /* __APPLE__ */

       if (fstat(to_fd, &to_stat) != -1) {
	       mode = to_stat.st_mode;
	       if ((mode & (S_IRWXG|S_IRWXO))
		   && fchmod(to_fd, mode & ~(S_IRWXG|S_IRWXO))) {
		       if (errno != EPERM) /* we have write access but do not own the file */
			       warn("%s: fchmod failed", to.p_path);
		       mode = 0;
	       }
       } else {
	       warn("%s", to.p_path);
       }

	if (!lflag && !sflag) {
		/*
		 * Mmap and write if less than 8M (the limit is so we don't
		 * totally trash memory on big files.  This is really a minor
		 * hack, but it wins some CPU back.
		 * Some filesystems, such as smbnetfs, don't support mmap,
		 * so this is a best-effort attempt.
		 */
#ifdef VM_AND_BUFFER_CACHE_SYNCHRONIZED
		if (S_ISREG(fs->st_mode) && fs->st_size > 0 &&
		    fs->st_size <= 8 * 1024 * 1024 &&
		    (p = mmap(NULL, (size_t)fs->st_size, PROT_READ,
		    MAP_SHARED, from_fd, (off_t)0)) != MAP_FAILED) {
			wtotal = 0;
			for (bufp = p, wresid = fs->st_size; ;
			    bufp += wcount, wresid -= (size_t)wcount) {
				wcount = write(to_fd, bufp, wresid);
				if (wcount <= 0)
					break;
				wtotal += wcount;
				if (info) {
					info = 0;
					(void)fprintf(stderr,
					    "%s -> %s %3d%%\n",
					    entp->fts_path, to.p_path,
					    cp_pct(wtotal, fs->st_size));
				}
				if (wcount >= (ssize_t)wresid)
					break;
			}
			if (wcount != (ssize_t)wresid) {
				warn("%s", to.p_path);
				rval = 1;
			}
			/* Some systems don't unmap on close(2). */
			if (munmap(p, fs->st_size) < 0) {
				warn("%s", entp->fts_path);
				rval = 1;
			}
		} else
#endif
		{
			if (buf == NULL) {
				/*
				 * Note that buf and bufsize are static. If
				 * malloc() fails, it will fail at the start
				 * and not copy only some files. 
				 */ 
				if (sysconf(_SC_PHYS_PAGES) > 
				    PHYSPAGES_THRESHOLD)
					bufsize = MIN(BUFSIZE_MAX, MAXPHYS * 8);
				else
					bufsize = BUFSIZE_SMALL;
				buf = malloc(bufsize);
				if (buf == NULL)
					err(1, "Not enough memory");
			}
			wtotal = 0;
			do {
#ifndef __APPLE__
				if (use_copy_file_range) {
					rcount = copy_file_range(from_fd, NULL,
					    to_fd, NULL, SSIZE_MAX, 0);
					if (rcount < 0 && errno == EINVAL) {
						/* Prob a non-seekable FD */
						use_copy_file_range = 0;
					}
				}
#endif
				if (!use_copy_file_range) {
					rcount = copy_fallback(from_fd, to_fd,
					    buf, bufsize);
				}
				wtotal += rcount;
				if (info) {
					info = 0;
					(void)fprintf(stderr,
					    "%s -> %s %3d%%\n",
					    entp->fts_path, to.p_path,
					    cp_pct(wtotal, fs->st_size));
				}
			} while (rcount > 0);
			if (rcount < 0) {
				warn("%s", entp->fts_path);
				rval = 1;
			}
		}
	} else if (lflag) {
		if (link(entp->fts_path, to.p_path)) {
			warn("%s", to.p_path);
			rval = 1;
		}
	} else if (sflag) {
		if (symlink(entp->fts_path, to.p_path)) {
			warn("%s", to.p_path);
			rval = 1;
		}
	}

	/*
	 * Don't remove the target even after an error.  The target might
	 * not be a regular file, or its attributes might be important,
	 * or its contents might be irreplaceable.  It would only be safe
	 * to remove it if we created it and its length is 0.
	 */

	if (!lflag && !sflag) {
#ifdef __APPLE__
		if (mode != 0 && fchmod(to_fd, mode))
			warn("%s: fchmod failed", to.p_path);
		/* do these before setfile in case copyfile changes mtime */
		if (!Xflag && S_ISREG(fs->st_mode)) { /* skip devices, etc */
			if (fcopyfile(from_fd, to_fd, NULL, COPYFILE_XATTR) < 0)
				warn("%s: could not copy extended attributes to %s",
				    entp->fts_path, to.p_path);
		}
		if (pflag && setfile(fs, to_fd))
			rval = 1;
		if (pflag) {
			/* If this ACL denies writeattr then setfile will fail... */
			if (fcopyfile(from_fd, to_fd, NULL, COPYFILE_ACL) < 0)
				warn("%s: could not copy ACL to %s",
				    entp->fts_path, to.p_path);
		}
#else  /* !__APPLE__ */
		if (pflag && setfile(fs, to_fd))
			rval = 1;
		if (pflag && preserve_fd_acls(from_fd, to_fd) != 0)
			rval = 1;
#endif /* __APPLE__ */
		if (close(to_fd)) {
			warn("%s", to.p_path);
			rval = 1;
		}
	}

done:
	if (from_fd != -1)
		(void)close(from_fd);
	return (rval);
}

int
copy_link(const FTSENT *p, int exists)
{
	ssize_t len;
	char llink[PATH_MAX];

	if (exists && nflag) {
		if (vflag)
			printf("%s not overwritten\n", to.p_path);
		return (1);
	}
	if ((len = readlink(p->fts_path, llink, sizeof(llink) - 1)) == -1) {
		warn("readlink: %s", p->fts_path);
		return (1);
	}
	llink[len] = '\0';
	if (exists && unlink(to.p_path)) {
		warn("unlink: %s", to.p_path);
		return (1);
	}
	if (symlink(llink, to.p_path)) {
		warn("symlink: %s", llink);
		return (1);
	}
#ifdef __APPLE__
	if (!Xflag)
		if (copyfile(p->fts_path, to.p_path, NULL, COPYFILE_XATTR | COPYFILE_NOFOLLOW_SRC) <0)
			warn("%s: could not copy extended attributes to %s",
			     p->fts_path, to.p_path);
#endif
	return (pflag ? setfile(p->fts_statp, -1) : 0);
}

int
copy_fifo(struct stat *from_stat, int exists)
{

	if (exists && nflag) {
		if (vflag)
			printf("%s not overwritten\n", to.p_path);
		return (1);
	}
	if (exists && unlink(to.p_path)) {
		warn("unlink: %s", to.p_path);
		return (1);
	}
	if (mkfifo(to.p_path, from_stat->st_mode)) {
		warn("mkfifo: %s", to.p_path);
		return (1);
	}
	return (pflag ? setfile(from_stat, -1) : 0);
}

int
copy_special(struct stat *from_stat, int exists)
{

	if (exists && nflag) {
		if (vflag)
			printf("%s not overwritten\n", to.p_path);
		return (1);
	}
	if (exists && unlink(to.p_path)) {
		warn("unlink: %s", to.p_path);
		return (1);
	}
	if (mknod(to.p_path, from_stat->st_mode, from_stat->st_rdev)) {
		warn("mknod: %s", to.p_path);
		return (1);
	}
	return (pflag ? setfile(from_stat, -1) : 0);
}

int
setfile(struct stat *fs, int fd)
{
	static struct timespec tspec[2];
	struct stat ts;
	int rval, gotstat, islink, fdval;

	rval = 0;
	fdval = fd != -1;
	islink = !fdval && S_ISLNK(fs->st_mode);
	fs->st_mode &= S_ISUID | S_ISGID | S_ISVTX |
	    S_IRWXU | S_IRWXG | S_IRWXO;

	tspec[0] = fs->st_atim;
	tspec[1] = fs->st_mtim;
	if (fdval ? futimens(fd, tspec) : utimensat(AT_FDCWD, to.p_path, tspec,
	    islink ? AT_SYMLINK_NOFOLLOW : 0)) {
		warn("utimensat: %s", to.p_path);
		rval = 1;
	}
	if (fdval ? fstat(fd, &ts) :
	    (islink ? lstat(to.p_path, &ts) : stat(to.p_path, &ts)))
		gotstat = 0;
	else {
		gotstat = 1;
		ts.st_mode &= S_ISUID | S_ISGID | S_ISVTX |
		    S_IRWXU | S_IRWXG | S_IRWXO;
	}
	/*
	 * Changing the ownership probably won't succeed, unless we're root
	 * or POSIX_CHOWN_RESTRICTED is not set.  Set uid/gid before setting
	 * the mode; current BSD behavior is to remove all setuid bits on
	 * chown.  If chown fails, lose setuid/setgid bits.
	 */
	if (!gotstat || fs->st_uid != ts.st_uid || fs->st_gid != ts.st_gid)
		if (fdval ? fchown(fd, fs->st_uid, fs->st_gid) :
		    (islink ? lchown(to.p_path, fs->st_uid, fs->st_gid) :
		    chown(to.p_path, fs->st_uid, fs->st_gid))) {
			if (errno != EPERM) {
#ifdef __APPLE__
				warn("%schown: %s", fdval ? "f" : (islink ? "l" : ""), to.p_path);
#else
				warn("chown: %s", to.p_path);
#endif
				rval = 1;
			}
			fs->st_mode &= ~(S_ISUID | S_ISGID);
		}

	if (!gotstat || fs->st_mode != ts.st_mode)
		if (fdval ? fchmod(fd, fs->st_mode) :
		    (islink ? lchmod(to.p_path, fs->st_mode) :
		    chmod(to.p_path, fs->st_mode))) {
#ifdef __APPLE__
			warn("%schmod: %s", fdval ? "f" : (islink ? "l" : ""), to.p_path);
#else
			warn("chmod: %s", to.p_path);
#endif
			rval = 1;
		}

	if (!gotstat || fs->st_flags != ts.st_flags)
		if (fdval ?
		    fchflags(fd, fs->st_flags) :
		    (islink ? lchflags(to.p_path, fs->st_flags) :
		    chflags(to.p_path, fs->st_flags))) {
#ifdef __APPLE__
			/*
			 * rdar://problem/21067328 - `cp -p` may fail due to the restrict
			 * flag.
			 */
			if (errno != EPERM) {
				warn("%schflags: %s", fdval ? "f" : (islink ? "l" : ""), to.p_path);
				rval = 1;
			}
#else
			warn("chflags: %s", to.p_path);
			rval = 1;
#endif
		}

	return (rval);
}

#ifndef __APPLE__
int
preserve_fd_acls(int source_fd, int dest_fd)
{
	acl_t acl;
	acl_type_t acl_type;
	int acl_supported = 0, ret, trivial;

	ret = fpathconf(source_fd, _PC_ACL_NFS4);
	if (ret > 0 ) {
		acl_supported = 1;
		acl_type = ACL_TYPE_NFS4;
	} else if (ret < 0 && errno != EINVAL) {
		warn("fpathconf(..., _PC_ACL_NFS4) failed for %s", to.p_path);
		return (1);
	}
	if (acl_supported == 0) {
		ret = fpathconf(source_fd, _PC_ACL_EXTENDED);
		if (ret > 0 ) {
			acl_supported = 1;
			acl_type = ACL_TYPE_ACCESS;
		} else if (ret < 0 && errno != EINVAL) {
			warn("fpathconf(..., _PC_ACL_EXTENDED) failed for %s",
			    to.p_path);
			return (1);
		}
	}
	if (acl_supported == 0)
		return (0);

	acl = acl_get_fd_np(source_fd, acl_type);
	if (acl == NULL) {
		warn("failed to get acl entries while setting %s", to.p_path);
		return (1);
	}
	if (acl_is_trivial_np(acl, &trivial)) {
		warn("acl_is_trivial() failed for %s", to.p_path);
		acl_free(acl);
		return (1);
	}
	if (trivial) {
		acl_free(acl);
		return (0);
	}
	if (acl_set_fd_np(dest_fd, acl, acl_type) < 0) {
		warn("failed to set acl entries for %s", to.p_path);
		acl_free(acl);
		return (1);
	}
	acl_free(acl);
	return (0);
}

int
preserve_dir_acls(struct stat *fs, char *source_dir, char *dest_dir)
{
	acl_t (*aclgetf)(const char *, acl_type_t);
	int (*aclsetf)(const char *, acl_type_t, acl_t);
	struct acl *aclp;
	acl_t acl;
	acl_type_t acl_type;
	int acl_supported = 0, ret, trivial;

	ret = pathconf(source_dir, _PC_ACL_NFS4);
	if (ret > 0) {
		acl_supported = 1;
		acl_type = ACL_TYPE_NFS4;
	} else if (ret < 0 && errno != EINVAL) {
		warn("fpathconf(..., _PC_ACL_NFS4) failed for %s", source_dir);
		return (1);
	}
	if (acl_supported == 0) {
		ret = pathconf(source_dir, _PC_ACL_EXTENDED);
		if (ret > 0) {
			acl_supported = 1;
			acl_type = ACL_TYPE_ACCESS;
		} else if (ret < 0 && errno != EINVAL) {
			warn("fpathconf(..., _PC_ACL_EXTENDED) failed for %s",
			    source_dir);
			return (1);
		}
	}
	if (acl_supported == 0)
		return (0);

	/*
	 * If the file is a link we will not follow it.
	 */
	if (S_ISLNK(fs->st_mode)) {
		aclgetf = acl_get_link_np;
		aclsetf = acl_set_link_np;
	} else {
		aclgetf = acl_get_file;
		aclsetf = acl_set_file;
	}
	if (acl_type == ACL_TYPE_ACCESS) {
		/*
		 * Even if there is no ACL_TYPE_DEFAULT entry here, a zero
		 * size ACL will be returned. So it is not safe to simply
		 * check the pointer to see if the default ACL is present.
		 */
		acl = aclgetf(source_dir, ACL_TYPE_DEFAULT);
		if (acl == NULL) {
			warn("failed to get default acl entries on %s",
			    source_dir);
			return (1);
		}
		aclp = &acl->ats_acl;
		if (aclp->acl_cnt != 0 && aclsetf(dest_dir,
		    ACL_TYPE_DEFAULT, acl) < 0) {
			warn("failed to set default acl entries on %s",
			    dest_dir);
			acl_free(acl);
			return (1);
		}
		acl_free(acl);
	}
	acl = aclgetf(source_dir, acl_type);
	if (acl == NULL) {
		warn("failed to get acl entries on %s", source_dir);
		return (1);
	}
	if (acl_is_trivial_np(acl, &trivial)) {
		warn("acl_is_trivial() failed on %s", source_dir);
		acl_free(acl);
		return (1);
	}
	if (trivial) {
		acl_free(acl);
		return (0);
	}
	if (aclsetf(dest_dir, acl_type, acl) < 0) {
		warn("failed to set acl entries on %s", dest_dir);
		acl_free(acl);
		return (1);
	}
	acl_free(acl);
	return (0);
}
#endif /* !__APPLE__ */

void
usage(void)
{

	if (unix2003_compat) {
	(void)fprintf(stderr, "%s\n%s\n",
	    "usage: cp [-R [-H | -L | -P]] [-fi | -n] [-aclpsvXx] "
	    "source_file target_file",
	    "       cp [-R [-H | -L | -P]] [-fi | -n] [-aclpsvXx] "
	    "source_file ... "
	    "target_directory");
	} else {
	(void)fprintf(stderr, "%s\n%s\n",
#ifdef __APPLE__
	    "usage: cp [-R [-H | -L | -P]] [-f | -i | -n] [-aclpsvXx] "
	    "source_file target_file",
	    "       cp [-R [-H | -L | -P]] [-f | -i | -n] [-aclpsvXx] "
#else
	    "usage: cp [-R [-H | -L | -P]] [-f | -i | -n] [-alpsvx] "
	    "source_file target_file",
	    "       cp [-R [-H | -L | -P]] [-f | -i | -n] [-alpsvx] "
#endif
	    "source_file ... "
	    "target_directory");
	}
	exit(EX_USAGE);
}
