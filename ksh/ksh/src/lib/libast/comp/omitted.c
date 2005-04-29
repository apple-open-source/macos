#pragma prototyped noticed

/*
 * workarounds to bring the native interface close to posix and x/open
 *
 *	Glenn Fowler <gsf@research.att.com>
 *	AT&T Labs Research
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of THIS SOFTWARE FILE (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, and/or sell copies of the
 * Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following disclaimer:
 *
 * THIS SOFTWARE IS PROVIDED BY AT&T ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL AT&T BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <ast.h>
#include <error.h>
#include <tm.h>

#include "FEATURE/omitted"

#undef	OMITTED

#if _win32_botch

#define	OMITTED	1

#include <ls.h>
#include <utime.h>

#if __CYGWIN__
#include <ast_windows.h>
#endif

#ifndef MAX_PATH
#define MAX_PATH	PATH_MAX
#endif

/*
 * these workarounds assume each system call foo() has a _foo() entry
 * which is true for __CYGWIN__ and __EMX__ (both gnu based)
 *
 * the workarounds handle:
 *
 *	(1) .exe suffix inconsistencies
 *	(2) /bin/sh reference in execve() and spawnve()
 *	(3) bogus getpagesize() return values
 *	(4) a fork() bug that screws up shell fork()+script
 *
 * NOTE: Not all workarounds can be handled by unix syscall intercepts.
 *	 In particular, { ksh nmake } have workarounds for case-ignorant
 *	 filesystems and { libast } has workarounds for win32 locale info.
 */

#undef _pathconf
#undef pathconf
#undef stat

extern int		_access(const char*, int);
extern unsigned int	_alarm(unsigned int);
extern int		_chmod(const char*, mode_t);
extern int		_close(int);
extern pid_t		_execve(const char*, char* const*, char* const*);
extern int		_link(const char*, const char*);
extern int		_open(const char*, int, ...);
extern long		_pathconf(const char*, int);
extern ssize_t		_read(int, void*, size_t);
extern int		_rename(const char*, const char*);
extern pid_t		_spawnve(int, const char*, char* const*, char* const*);
extern int		_stat(const char*, struct stat*);
extern int		_unlink(const char*);
extern int		_utime(const char*, struct utimbuf*);
extern ssize_t		_write(int, const void*, size_t);

#if defined(__EXPORT__)
#define extern	__EXPORT__
#endif

static char*
suffix(register const char* path)
{
	register const char*	s = path + strlen(path);
	register int		c;

	while (s > path)
		if ((c = *--s) == '.')
			return (char*)s + 1;
		else if (c == '/' || c == '\\')
			break;
	return 0;
}

static int
execrate(const char* path, char* buf, int size, int physical)
{
	char*	s;
	int	n;
	int	oerrno;

	if (suffix(path))
		return 0;
	oerrno = errno;
	if (physical || strlen(path) >= size || !(s = pathcanon(strcpy(buf, path), PATH_PHYSICAL|PATH_DOTDOT|PATH_EXISTS)))
		snprintf(buf, size, "%s.exe", path);
	else if (!suffix(buf) && ((buf + size) - s) >= 4)
		strcpy(s, ".exe");
	errno = oerrno;
	return 1;
}

#define MAGIC_mode		0
#define MAGIC_exec		1

/*
 * return 0 if path is magic, -1 otherwise
 * op==MAGIC_exec retains errno for -1 return
 */

static int
magic(const char* path, int op)
{
	int		fd;
	int		r;
	int		oerrno;
	unsigned char	buf[2];

	oerrno = errno;
	if ((fd = _open(path, O_RDONLY, 0)) >= 0)
	{
		r = _read(fd, buf, 2) == 2 && (buf[1] == 0x5a && (buf[0] == 0x4c || buf[0] == 0x4d) || op == MAGIC_exec && buf[0] == '#' && buf[1] == '!') ? 0 : -1;
		close(fd);
		if (r && op == MAGIC_exec)
			oerrno = ENOEXEC;
	}
	else if (op != MAGIC_exec)
		r = -1;
	else if (errno == ENOENT)
	{
		oerrno = errno;
		r = -1;
	}
	else
		r = 0;
	errno = oerrno;
	return r;
}

#if _win32_botch_access

extern int
access(const char* path, int op)
{
	int	r;
	int	oerrno;
	char	buf[PATH_MAX];

	oerrno = errno;
	if ((r = _access(path, op)) && errno == ENOENT && execrate(path, buf, sizeof(buf), 0))
	{
		errno = oerrno;
		r = _access(buf, op);
	}
	return r;
}

#endif

#if _win32_botch_alarm

extern unsigned int
alarm(unsigned int s)
{
	unsigned int		n;
	unsigned int		r;

	static unsigned int	a;

	n = (unsigned int)time(NiL);
	if (a <= n)
		r = 0;
	else
		r = a - n;
	a = n + s - 1;
	(void)_alarm(s);
	return r;
}

#endif

#if _win32_botch_chmod

extern int
chmod(const char* path, mode_t mode)
{
	int	r;
	int	oerrno;
	char	buf[PATH_MAX];

	if ((r = _chmod(path, mode)) && errno == ENOENT && execrate(path, buf, sizeof(buf), 0))
	{
		errno = oerrno;
		return _chmod(buf, mode);
	}
	if (!(r = _chmod(path, mode)) &&
	    (mode & (S_IXUSR|S_IXGRP|S_IXOTH)) &&
	    !suffix(path) &&
	    (strlen(path) + 4) < sizeof(buf))
	{
		oerrno = errno;
		if (!magic(path, MAGIC_mode))
		{
			snprintf(buf, sizeof(buf), "%s.exe", path);
			_rename(path, buf);
		}
		errno = oerrno;
	}
	return r;
}

#endif

#if _win32_botch_execve || _lib_spawn_mode

#if _lib_spawn_mode

/*
 * can anyone get const prototype args straight?
 */

#define execve		______execve
#define spawnve		______spawnve

#include <process.h>

#undef	execve
#undef	spawnve

#endif

#ifndef _P_OVERLAY
#define _P_OVERLAY	(-1)
#endif

#define DEBUG		1

static pid_t
runve(int mode, const char* path, char* const* argv, char* const* envv)
{
	register char*	s;
	register char**	p;
	register char**	v;

	void*		m1;
	void*		m2;
	pid_t		pid;
	int		oerrno;
#if defined(_P_DETACH) && defined(_P_NOWAIT)
	int		pgrp;
#endif
	struct stat	st;
	char		buf[PATH_MAX];
	char		tmp[PATH_MAX];

#if DEBUG
	int		n;

	static int	trace;
#endif

#if defined(_P_DETACH) && defined(_P_NOWAIT)
	if (mode == _P_DETACH)
	{
		/*
		 * 2004-02-29 cygwin _P_DETACH is useless:
		 *	spawn*() returns 0 instead of the spawned pid
		 *	spawned { pgid sid } are the same as the parent
		 */

		mode = _P_NOWAIT;
		pgrp = 1;
	}
	else
		pgrp = 0;
#endif
	if (!envv)
		envv = (char* const*)environ;
	m1 = m2 = 0;
	oerrno = errno;
#if DEBUG
	if (!trace)
		trace = (s = getenv("_AST_exec_trace")) ? *s : 'n';
#endif
	if (execrate(path, buf, sizeof(buf), 0))
	{
		if (!_stat(buf, &st))
			path = (const char*)buf;
		else
			errno = oerrno;
	}
	if (path != (const char*)buf && _stat(path, &st))
		return -1;
	if (!S_ISREG(st.st_mode) || !(st.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH)))
	{
		errno = EACCES;
		return -1;
	}
	if (magic(path, MAGIC_exec))
	{
#if _CYGWIN_fork_works
		errno = ENOEXEC;
		return -1;
#else
		p = (char**)argv;
		while (*p++);
		if (!(v = (char**)malloc((p - (char**)argv + 2) * sizeof(char*))))
		{
			errno = EAGAIN;
			return -1;
		}
		m1 = v;
		p = v;
		*p++ = (char*)path;
		*p++ = (char*)path;
		path = (const char*)pathshell();
		if (*argv)
			argv++;
		while (*p++ = (char*)*argv++);
		argv = (char* const*)v;
#endif
	}

	/*
	 * the win32 dll search order is
	 *	(1) the directory of path
	 *	(2) .
	 *	(3) /c/(WINNT|WINDOWS)/system32 /c/(WINNT|WINDOWS)
	 *	(4) the directories on $PATH
	 * there are no cygwin dlls in (3), so if (1) and (2) fail
	 * to produce the required dlls its up to (4)
	 *
	 * the standard allows PATH to be anything once the path
	 * to an executable is determined; this code ensures that PATH
	 * contains /bin so that at least the cygwin dll, required
	 * by all cygwin executables, will be found
	 */

	if (p = (char**)envv)
	{
		n = 1;
		while (s = *p++)
			if (strneq(s, "PATH=", 5))
			{
				s += 5;
				do
				{
					s = pathcat(tmp, s, ':', NiL, "");
					if (streq(tmp, "/usr/bin/") || streq(tmp, "/bin/"))
					{
						n = 0;
						break;
					}
				} while (s);
				if (n)
				{
					n = 0;
					snprintf(tmp, sizeof(tmp), "%s:/bin", *(p - 1));
					*(p - 1) = tmp;
				}
				break;
			}
		if (n)
		{
			n = p - (char**)envv + 1;
			p = (char**)envv;
			if (v = (char**)malloc(n * sizeof(char*)))
			{
				m2 = v;
				envv = (char* const*)v;
				*v++ = strcpy(tmp, "PATH=/bin");
				while (*v++ = *p++);
			}
		}
	}

#if DEBUG
	if (trace == 'a' || trace == 'e')
	{
		sfprintf(sfstderr, "%s %s [", mode == _P_OVERLAY ? "_execve" : "_spawnve", path);
		for (n = 0; argv[n]; n++)
			sfprintf(sfstderr, " '%s'", argv[n]);
		if (trace == 'e')
		{
			sfprintf(sfstderr, " ] [");
			for (n = 0; envv[n]; n++)
				sfprintf(sfstderr, " '%s'", envv[n]);
		}
		sfprintf(sfstderr, " ]\n");
		sfsync(sfstderr);
	}
#endif
#if _lib_spawn_mode
	if (mode != _P_OVERLAY)
	{
		pid = _spawnve(mode, path, argv, envv);
#if defined(_P_DETACH) && defined(_P_NOWAIT)
		if (pid > 0 && pgrp)
			setpgid(pid, 0);
#endif
	}
	else
#endif
	{
#if defined(_P_DETACH) && defined(_P_NOWAIT)
		if (pgrp)
			setpgid(0, 0);
#endif
		pid = _execve(path, argv, envv);
	}
	if (m1)
		free(m1);
	if (m2)
		free(m2);
	return pid;
}

#if _win32_botch_execve

extern pid_t
execve(const char* path, char* const* argv, char* const* envv)
{
	return runve(_P_OVERLAY, path, argv, envv);
}

#endif

#if _lib_spawn_mode

extern pid_t
spawnve(int mode, const char* path, char* const* argv, char* const* envv)
{
	return runve(mode, path, argv, envv);
}

#endif

#endif

#if _win32_botch_getpagesize

extern size_t
getpagesize(void)
{
	return 64 * 1024;
}

#endif

#if _win32_botch_link

extern int
link(const char* fp, const char* tp)
{
	int	r;
	int	oerrno;
	char	fb[PATH_MAX];
	char	tb[PATH_MAX];

	oerrno = errno;
	if ((r = _link(fp, tp)) && errno == ENOENT && execrate(fp, fb, sizeof(fb), 1))
	{
		if (execrate(tp, tb, sizeof(tb), 1))
			tp = tb;
		errno = oerrno;
		r = _link(fb, tp);
	}
	return r;
}

#endif

#if _win32_botch_open || _win32_botch_copy

#if _win32_botch_copy

/*
 * this should intercept the important cases
 * dup*() and exec*() fd's will not be intercepted
 */

typedef struct Exe_test_s
{
	int		test;
	ino_t		ino;
	char		path[PATH_MAX];
} Exe_test_t;

static Exe_test_t*	exe[16];

extern int
close(int fd)
{
	int		r;
	int		oerrno;
	struct stat	st;
	char		buf[PATH_MAX];

	if (fd >= 0 && fd < elementsof(exe) && exe[fd])
	{
		r = exe[fd]->test;
		exe[fd]->test = 0;
		if (r > 0 && !fstat(fd, &st) && st.st_ino == exe[fd]->ino)
		{
			if (r = _close(fd))
				return r;
			oerrno = errno;
			if (!stat(exe[fd]->path, &st) && st.st_ino == exe[fd]->ino)
			{
				snprintf(buf, sizeof(buf), "%s.exe", exe[fd]->path);
				_rename(exe[fd]->path, buf);
			}
			errno = oerrno;
			return 0;
		}
	}
	return _close(fd);
}

extern ssize_t
write(int fd, const void* buf, size_t n)
{
	if (fd >= 0 && fd < elementsof(exe) && exe[fd] && exe[fd]->test < 0)
		exe[fd]->test = n >= 2 && ((unsigned char*)buf)[1] == 0x5a && (((unsigned char*)buf)[0] == 0x4c || ((unsigned char*)buf)[0] == 0x4d) && !lseek(fd, (off_t)0, SEEK_CUR);
	return _write(fd, buf, n);
}

#endif

extern int
open(const char* path, int flags, ...)
{
	int		fd;
	int		mode;
	int		oerrno;
	char		buf[PATH_MAX];
#if _win32_botch_copy
	struct stat	st;
#endif
	va_list		ap;

	va_start(ap, flags);
	mode = (flags & O_CREAT) ? va_arg(ap, int) : 0;
	oerrno = errno;
	fd = _open(path, flags, mode);
#if _win32_botch_open
	if (fd < 0 && errno == ENOENT && execrate(path, buf, sizeof(buf), 0))
	{
		errno = oerrno;
		fd = _open(buf, flags, mode);
	}
#endif
#if _win32_botch_copy
	if (fd >= 0 && fd < elementsof(exe) && strlen(path) < PATH_MAX &&
	    (flags & (O_CREAT|O_TRUNC)) == (O_CREAT|O_TRUNC) && (mode & 0111))
	{
		if (!suffix(path) && !fstat(fd, &st) && (exe[fd] || (exe[fd] = (Exe_test_t*)malloc(sizeof(Exe_test_t)))))
		{
			exe[fd]->test = -1;
			exe[fd]->ino = st.st_ino;
			strcpy(exe[fd]->path, path);
		}
		errno = oerrno;
	}
#endif
	va_end(ap);
	return fd;
}

#endif

#if _win32_botch_pathconf

extern long
pathconf(const char* path, int op)
{
	if (_access(path, F_OK))
		return -1;
	return _pathconf(path, op);
}

#endif

#if _win32_botch_rename

extern int
rename(const char* fp, const char* tp)
{
	int	r;
	int	oerrno;
	char	fb[PATH_MAX];
	char	tb[PATH_MAX];

	oerrno = errno;
	if ((r = _rename(fp, tp)) && errno == ENOENT && execrate(fp, fb, sizeof(fb), 1))
	{
		if (execrate(tp, tb, sizeof(tb), 1))
			tp = tb;
		errno = oerrno;
		r = _rename(fb, tp);
	}
	return r;
}

#endif

#if _win32_botch_stat

extern int
stat(const char* path, struct stat* st)
{
	int	r;
	int	oerrno;
	char	buf[PATH_MAX];

	oerrno = errno;
	if ((r = _stat(path, st)) && errno == ENOENT && execrate(path, buf, sizeof(buf), 0))
	{
		errno = oerrno;
		r = _stat(buf, st);
	}
	return r;
}

#endif

#if _win32_botch_truncate

extern int
truncate(const char* path, off_t offset)
{
	int	r;
	int	oerrno;
	char	buf[PATH_MAX];

	oerrno = errno;
	if ((r = _truncate(path, offset)) && errno == ENOENT && execrate(path, buf, sizeof(buf), 0))
	{
		errno = oerrno;
		r = _truncate(buf, offset);
	}
	return r;
}

#endif

#if _win32_botch_unlink

extern int
unlink(const char* path)
{
	int		r;
	int		drive;
	int		mask;
	int		suffix;
	int		stop;
	int		oerrno;
	unsigned long	base;
	char		buf[PATH_MAX];
	char		tmp[MAX_PATH];

#define DELETED_DIR_1	7
#define DELETED_DIR_2	16

	static char	deleted[] = "%c:\\temp\\.deleted\\%08x.%03x";

	static int	count = 0;

#if __CYGWIN__

	DWORD		fattr = FILE_ATTRIBUTE_NORMAL|FILE_FLAG_DELETE_ON_CLOSE;
	DWORD		share = FILE_SHARE_DELETE;
	HANDLE		hp;
	struct stat	st;
	char		nat[MAX_PATH];

	oerrno = errno;
	if (lstat(path, &st) || !S_ISREG(st.st_mode))
		goto try_unlink;
	cygwin_conv_to_full_win32_path(path, nat);
	if (!strncasecmp(nat + 1, ":\\temp\\", 7))
		goto try_unlink;
	drive = nat[0];
	path = (const char*)nat;
	for (;;)
	{
		hp = CreateFile(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL|FILE_FLAG_DELETE_ON_CLOSE, NULL);
		if (hp != INVALID_HANDLE_VALUE)
		{
			CloseHandle(hp);
			errno = oerrno;
			return 0;
		}
		if (GetLastError() != ERROR_FILE_NOT_FOUND)
			break;
		if (path == (const char*)buf || !execrate(path, buf, sizeof(buf), 1))
		{
			errno = ENOENT;
			return -1;
		}
		path = (const char*)buf;
	}
#else
	if (_access(path, 0))
#if _win32_botch_access
	{
		if (errno != ENOENT || !execrate(path, buf, sizeof(buf), 1) || _access(buf, 0))
			return -1;
		path = (const char*)buf;
	}
#else
		return -1;
#endif
	drive = 'C':
#endif

	/*
	 * rename to a `deleted' path just in case the file is open
	 * otherwise directory readers may choke on phantom entries
	 */

	base = ((getuid() & 0xffff) << 16) | (time(NiL) & 0xffff);
	suffix = (getpid() & 0xfff) + count++;
	snprintf(tmp, sizeof(tmp), deleted, drive, base, suffix);
	if (!_rename(path, tmp))
	{
		path = (const char*)tmp;
		goto try_delete;
	}
	if (errno != ENOTDIR && errno != ENOENT)
		goto try_unlink;
	tmp[DELETED_DIR_2] = 0;
	if (_access(tmp, 0))
	{
		mask = umask(0);
		tmp[DELETED_DIR_1] = 0;
		if (_access(tmp, 0) && _mkdir(tmp, S_IRWXU|S_IRWXG|S_IRWXO))
		{
			umask(mask);
			goto try_unlink;
		}
		tmp[DELETED_DIR_1] = '\\';
		r = _mkdir(tmp, S_IRWXU|S_IRWXG|S_IRWXO);
		umask(mask);
		if (r)
			goto try_unlink;
		errno = 0;
	}
	tmp[DELETED_DIR_2] = '\\';
	if (!errno && !_rename(path, tmp))
	{
		path = (const char*)tmp;
		goto try_delete;
	}
#if !__CYGWIN__
	if (errno == ENOENT)
	{
#if !_win32_botch_access
		if (execrate(path, buf, sizeof(buf), 1) && !_rename(buf, tmp))
			path = (const char*)tmp;
#endif
		goto try_unlink;
	}
#endif
	stop = suffix;
	do
	{
		snprintf(tmp, sizeof(tmp), deleted, drive, base, suffix);
		if (!_rename(path, tmp))
		{
			path = (const char*)tmp;
			goto try_delete;
		}
		if (++suffix > 0xfff)
			suffix = 0;
	} while (suffix != stop);
 try_delete:
#if __CYGWIN__
	hp = CreateFile(path, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL|FILE_FLAG_DELETE_ON_CLOSE, NULL);
	if (hp != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hp);
		errno = oerrno;
		return 0;
	}
#endif
 try_unlink:
	errno = oerrno;
	return _unlink(path);
}

#endif

#if _win32_botch_utime

extern int
utime(const char* path, struct utimbuf* ut)
{
	int	r;
	int	oerrno;
	char	buf[PATH_MAX];

	oerrno = errno;
	if ((r = _utime(path, ut)) && errno == ENOENT && execrate(path, buf, sizeof(buf), 0))
	{
		errno = oerrno;
		r = _utime(path = buf, ut);
	}
#if __CYGWIN__

	/*
	 * cygwin refuses to set st_ctime
	 * utime() (at least) rejects that refusal
	 */

	if (!r)
	{
		HANDLE		hp;
		SYSTEMTIME	st;
		FILETIME	ct;
		WIN32_FIND_DATA	ff;
		struct stat	fs;
		char		tmp[MAX_PATH];

		if (_stat(path, &fs) || (fs.st_mode & S_IWUSR) || _chmod(path, (fs.st_mode | S_IWUSR) & S_IPERM))
			fs.st_mode = 0;
		cygwin_conv_to_win32_path(path, tmp);
		hp = CreateFile(tmp, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hp && hp != INVALID_HANDLE_VALUE)
		{
			GetSystemTime(&st);
			SystemTimeToFileTime(&st, &ct);
			SetFileTime(hp, &ct, 0, 0);
			CloseHandle(hp);
		}
		if (fs.st_mode)
			_chmod(path, fs.st_mode & S_IPERM);
		errno = oerrno;
	}
#endif
	return r;
}

#endif

#endif

/*
 * some systems (sun) miss a few functions required by their
 * own bsd-like macros
 */

#if !_lib_bzero || defined(bzero)

#undef	bzero

void
bzero(void* b, size_t n)
{
	memset(b, 0, n);
}

#endif

#if !_lib_getpagesize || defined(getpagesize)

#ifndef OMITTED
#define OMITTED	1
#endif

#undef	getpagesize

#ifdef	_SC_PAGESIZE
#undef	PAGESIZE
#define PAGESIZE	(int)sysconf(_SC_PAGESIZE)
#else
#ifndef PAGESIZE
#define PAGESIZE	4096
#endif
#endif

int
getpagesize()
{
	return PAGESIZE;
}

#endif

#if __CYGWIN__ && defined(__IMPORT__) && defined(__EXPORT__)

#ifndef OMITTED
#define OMITTED	1
#endif

/*
 * a few _imp__FUNCTION symbols are needed to avoid
 * static link multiple definitions
 */

#ifndef strtod
__EXPORT__ double (*_imp__strtod)(const char*, char**) = strtod;
#endif

#endif

#ifndef OMITTED

NoN(omitted)

#endif
