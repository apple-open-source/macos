/*******************************************************************
*                                                                  *
*             This software is part of the ast package             *
*                Copyright (c) 1985-2004 AT&T Corp.                *
*        and it may only be used by you under license from         *
*                       AT&T Corp. ("AT&T")                        *
*         A copy of the Source Code Agreement is available         *
*                at the AT&T Internet web site URL                 *
*                                                                  *
*       http://www.research.att.com/sw/license/ast-open.html       *
*                                                                  *
*    If you have copied or used this software without agreeing     *
*        to the terms of the license you are infringing on         *
*           the license and copyright and are violating            *
*               AT&T's intellectual property rights.               *
*                                                                  *
*            Information and Software Systems Research             *
*                        AT&T Labs Research                        *
*                         Florham Park NJ                          *
*                                                                  *
*               Glenn Fowler <gsf@research.att.com>                *
*                David Korn <dgk@research.att.com>                 *
*                 Phong Vo <kpv@research.att.com>                  *
*                                                                  *
*******************************************************************/
#pragma prototyped

/*
 * spawnveg -- spawnve with process group or session control
 *
 *	pgid	<0	setsid()	[session group leader]
 *		 0	nothing		[retain session and process group]
 *		 1	setpgid(0,0)	[process group leader]
 *		>1	setpgid(0,pgid)	[join process group]
 */

#include <ast.h>

#if _lib_spawnveg

NoN(spawnveg)

#else

#if _lib_posix_spawn

#include <spawn.h>
#include <error.h>

pid_t
spawnveg(const char* path, char* const argv[], char* const envv[], pid_t pgid)
{
	int			err;
	pid_t			pid;
	posix_spawnattr_t	attr;

	if (err = posix_spawnattr_init(&attr))
		goto bad;
	if (pgid)
	{
		if (pgid <= 1)
			pgid = 0;
		if (err = posix_spawnattr_setpgroup(&attr, pgid))
			goto bad;
	}
	if (err = posix_spawn(&pid, path, NiL, &attr, argv, envv ? envv : environ))
		goto bad;
	posix_spawnattr_destroy(&attr);
	return pid;
 bad:
	errno = err;
	return -1;
}

#else

#if _lib_spawn_mode

#include <process.h>

#ifndef P_NOWAIT
#define P_NOWAIT	_P_NOWAIT
#endif
#ifndef P_DETACH
#define P_DETACH	_P_DETACH
#endif

pid_t
spawnveg(const char* path, char* const argv[], char* const envv[], pid_t pgid)
{
	return spawnve(pgid ? P_DETACH : P_NOWAIT, path, argv, envv ? envv : environ);
}

#else

#if _lib_spawn && _hdr_spawn && _mem_pgroup_inheritance

#include <spawn.h>

/*
 * open-edition/mvs/zos fork+exec+(setpgid)
 */

pid_t
spawnveg(const char* path, char* const argv[], char* const envv[], pid_t pgid)
{
	struct inheritance	inherit;

	inherit.flags = 0;
	if (pgid)
	{
		inherit.flags |= SPAWN_SETGROUP;
		inherit.pgroup = (pgid > 1) ? pgid : SPAWN_NEWPGROUP;
	}
	return spawn(path, 0, (int*)0, &inherit, (const char**)argv, (const char**)envv);
}

#else

#include <error.h>
#include <sig.h>
#include <ast_vfork.h>

#ifndef ENOSYS
#define ENOSYS	EINVAL
#endif

#if _lib_spawnve && _hdr_process
#include <process.h>
#if defined(P_NOWAIT) || defined(_P_NOWAIT)
#undef	_lib_spawnve
#endif
#endif

/*
 * fork+exec+(setsid|setpgid) with script check to avoid shell double fork
 */

pid_t
spawnveg(const char* path, char* const argv[], char* const envv[], pid_t pgid)
{
#if _lib_fork || _lib_vfork
	int	n;
	pid_t	pid;
#if _real_vfork
	int	exec_errno;
	int*	exec_errno_ptr;
	void*	exec_free;
	void**	exec_free_ptr;
#else
	int	err[2];
#endif
#endif
#if _AST_DEBUG_spawnveg
	int	debug;
#endif

	if (!envv)
		envv = environ;
#if _lib_spawnve
#if _lib_fork || _lib_vfork
	if (!pgid)
#endif
		return spawnve(path, argv, envv);
#endif
#if _lib_fork || _lib_vfork
	n = errno;
#if _real_vfork
	exec_errno = 0;
	exec_errno_ptr = &exec_errno;
	exec_free = 0;
	exec_free_ptr = &exec_free;
#else
	if (pipe(err) < 0)
		err[0] = -1;
	else
	{
		fcntl(err[0], F_SETFD, FD_CLOEXEC);
		fcntl(err[1], F_SETFD, FD_CLOEXEC);
	}
#endif
	sigcritical(1);
#if _lib_vfork
	pid = vfork();
#else
	pid = fork();
#endif
#if _AST_DEBUG_spawnveg
	debug = streq(path, "/bin/_ast_debug_spawnveg_");
#endif
	sigcritical(0);
	if (!pid)
	{
#if _AST_DEBUG_spawnveg
		if (debug)
			_exit(argv[1] ? (int)strtol(argv[1], NiL, 0) : 0);
#endif
		if (pgid < 0)
			setsid();
		else if (pgid > 0)
		{
			if (pgid == 1)
				pgid = 0;
			if (setpgid(0, pgid) < 0 && pgid && errno == EPERM)
				setpgid(0, 0);
		}
		execve(path, argv, envv);
#if _real_vfork
		*exec_errno_ptr = errno;
#else
		if (err[0] != -1)
		{
			n = errno;
			write(err[1], &n, sizeof(n));
		}
#endif
		_exit(errno == ENOENT ? EXIT_NOTFOUND : EXIT_NOEXEC);
	}
	else if (pid != -1)
	{
		if (pgid > 0)
		{
			/*
			 * parent and child are in a race here
			 */

			if (pgid == 1)
				pgid = pid;
			if (setpgid(pid, pgid) < 0 && pid != pgid && errno == EPERM)
				setpgid(pid, pid);
		}
#if _real_vfork
		if (exec_errno)
		{
			while (waitpid(pid, NiL, 0) == -1 && errno == EINTR);
			pid = -1;
			n = exec_errno;
		}
		if (exec_free)
			free(exec_free);
#endif
		errno = n;
#if _AST_DEBUG_spawnveg
		if (!n && debug)
			pause();
#endif
	}
#if !_real_vfork
	if (err[0] != -1)
	{
		close(err[1]);
		if (read(err[0], &n, sizeof(n)) == sizeof(n))
		{
			while (waitpid(pid, NiL, 0) == -1 && errno == EINTR);
			pid = -1;
			errno = n;
		}
		close(err[0]);
	}
#endif
	return pid;
#else
	errno = ENOSYS;
	return -1;
#endif
}

#endif

#endif

#endif

#endif
