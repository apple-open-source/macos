/*******************************************************************
*                                                                  *
*             This software is part of the ast package             *
*                Copyright (c) 1992-2004 AT&T Corp.                *
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
*                                                                  *
*******************************************************************/
#pragma prototyped
/*
 * Glenn Fowler
 * AT&T Research
 *
 * rm [-fir] [file ...]
 */

static const char usage[] =
"[-?\n@(#)$Id: rm (AT&T Labs Research) 2003-09-11 $\n]"
USAGE_LICENSE
"[+NAME?rm - remove files]"
"[+DESCRIPTION?\brm\b removes the named \afile\a arguments. By default it"
"	does not remove directories. If a file is unwritable, the"
"	standard input is a terminal, and the \b--force\b option is not"
"	given, \brm\b prompts the user for whether to remove the file."
"	An affirmative response (\by\b or \bY\b) removes the file, a quit"
"	response (\bq\b or \bQ\b) causes \brm\b to exit immediately, and"
"	all other responses skip the current file.]"

"[c|F:clear|clobber?Clear the contents of each file before removing by"
"	writing a 0 filled buffer the same size as the file, executing"
"	\bfsync\b(2) and closing before attempting to remove. Implemented"
"	only on systems that support \bfsync\b(2).]"
"[d:directory?\bremove\b(3) (or \bunlink\b(2)) directories rather than"
"	\brmdir\b(2), and don't require that they be empty before removal."
"	The caller requires sufficient privilege, not to mention a strong"
"	constitution, to use this option. Even though the directory must"
"	not be empty, \brm\b still attempts to empty it before removal.]"
"[f:force?Ignore nonexistent files and never prompt the user.]"
"[i:interactive|prompt?Prompt whether to remove each file."
"	An affirmative response (\by\b or \bY\b) removes the file, a quit"
"	response (\bq\b or \bQ\b) causes \brm\b to exit immediately, and"
"	all other responses skip the current file.]"
"[r|R:recursive?Remove the contents of directories recursively.]"
"[u:unconditional?If \b--recursive\b and \b--force\b are also enabled then"
"	the owner read, write and execute modes are enabled (if not already"
"	enabled) for each directory before attempting to remove directory"
"	contents.]"
"[v:verbose?Print the name of each file before removing it.]"

"\n"
"\nfile ...\n"
"\n"

"[+SEE ALSO?\bmv\b(1), \brmdir\b(2), \bunlink\b(2), \bremove\b(3)]"
;

#include <cmdlib.h>
#include <ls.h>
#include <ftwalk.h>
#include <fs3d.h>

#define RM_ENTRY	1

#define beenhere(f)	(((f)->local.number>>1)==(f)->statb.st_nlink)
#define isempty(f)	(!((f)->local.number&RM_ENTRY))
#define nonempty(f)	((f)->parent->local.number|=RM_ENTRY)
#define pathchunk(n)	roundof(n,1024)
#define retry(f)	((f)->local.number=((f)->statb.st_nlink<<1))

static struct				/* program state		*/
{
	int		clobber;	/* clear out file data first	*/
	int		directory;	/* remove(dir) not rmdir(dir)	*/
	int		force;		/* force actions		*/
	int		fs3d;		/* 3d enabled			*/
	int		interactive;	/* prompt for approval		*/
	int		interrupt;	/* interrupt -- unwind		*/
	int		recursive;	/* remove subtrees too		*/
	int		terminal;	/* attached to terminal		*/
	int		uid;		/* caller uid			*/
	int		unconditional;	/* enable dir rwx on preorder	*/
	int		verbose;	/* display each file		*/
} state;

/*
 * remove a single file
 */

static int
rm(register Ftw_t* ftw)
{
	register char*	path;
	register int	n;
	int		v;
	struct stat	st;

	if (state.interrupt)
		return -1;
	if (ftw->info == FTW_NS)
	{
		if (!state.force)
			error(2, "%s: not found", ftw->path);
	}
	else if (state.fs3d && iview(&ftw->statb))
		ftw->status = FTW_SKIP;
	else switch (ftw->info)
	{
	case FTW_DNR:
	case FTW_DNX:
		if (state.unconditional)
		{
			if (!chmod(ftw->name, (ftw->statb.st_mode & S_IPERM)|S_IRWXU))
			{
				ftw->status = FTW_AGAIN;
				break;
			}
			error_info.errors++;
		}
		else if (!state.force)
			error(2, "%s: cannot %s directory", ftw->path, (ftw->info & FTW_NR) ? "read" : "search");
		else
			error_info.errors++;
		ftw->status = FTW_SKIP;
		nonempty(ftw);
		break;
	case FTW_D:
	case FTW_DC:
		path = ftw->name;
		if (path[0] == '.' && (!path[1] || path[1] == '.' && !path[2]) && (ftw->level > 0 || path[1]))
		{
			ftw->status = FTW_SKIP;
			if (!state.force)
				error(2, "%s: cannot remove", ftw->path);
			else
				error_info.errors++;
			break;
		}
		if (!state.recursive)
		{
			ftw->status = FTW_SKIP;
			error(2, "%s: directory", ftw->path);
			break;
		}
		if (!beenhere(ftw))
		{
			if (state.unconditional && (ftw->statb.st_mode ^ S_IRWXU))
				chmod(path, (ftw->statb.st_mode & S_IPERM)|S_IRWXU);
			if (ftw->level > 0)
			{
				char*	s;

				if (ftw->status == FTW_NAME || !(s = strrchr(ftw->path, '/')))
					v = !stat(".", &st);
				else
				{
					path = ftw->path;
					*s = 0;
					v = !stat(path, &st);
					*s = '/';
				}
				if (v)
					v = st.st_nlink <= 2 || st.st_ino == ftw->parent->statb.st_ino && st.st_dev == ftw->parent->statb.st_dev || strchr(astconf("PATH_ATTRIBUTES", path, NiL), 'l');
			}
			else
				v = 1;
			if (v)
			{
				if (state.interactive && astquery(2, "remove directory %s? ", ftw->path) > 0)
				{
					ftw->status = FTW_SKIP;
					nonempty(ftw);
				}
				if (ftw->info == FTW_D)
					break;
			}
			else
			{
				ftw->info = FTW_DC;
				error(1, "%s: hard link to directory", ftw->path);
			}
		}
		else if (ftw->info == FTW_D)
			break;
		/*FALLTHROUGH*/
	case FTW_DP:
		if (isempty(ftw) || state.directory)
		{
			path = ftw->name;
			if (path[0] != '.' || path[1])
			{
				if (ftw->status != FTW_NAME)
					path = ftw->path;
				if (state.verbose)
					sfputr(sfstdout, ftw->path, '\n');
				if ((ftw->info == FTW_DC || state.directory) ? remove(path) : rmdir(path)) switch (errno)
				{
				case EEXIST:
#if defined(ENOTEMPTY) && (ENOTEMPTY) != (EEXIST)
				case ENOTEMPTY:
#endif
					if (ftw->info == FTW_DP && !beenhere(ftw))
					{
						retry(ftw);
						ftw->status = FTW_AGAIN;
						break;
					}
					/*FALLTHROUGH*/
				default:
					nonempty(ftw);
					if (!state.force)
						error(ERROR_SYSTEM|2, "%s: directory not removed", ftw->path);
					else
						error_info.errors++;
					break;
				}
			}
			else if (!state.force)
				error(2, "%s: cannot remove", ftw->path);
			else
				error_info.errors++;
		}
		else
		{
			nonempty(ftw);
			if (!state.force)
				error(2, "%s: directory not removed", ftw->path);
			else
				error_info.errors++;
		}
		break;
	default:
		path = ftw->status == FTW_NAME ? ftw->name : ftw->path;
		if (state.verbose)
			sfputr(sfstdout, ftw->path, '\n');
		if (state.interactive)
		{
			if (astquery(2, "remove %s? ", ftw->path) > 0)
			{
				nonempty(ftw);
				break;
			}
		}
		else if (!state.force && state.terminal && S_ISREG(ftw->statb.st_mode))
		{
			if ((n = open(path, O_RDWR)) < 0)
			{
				if (
#ifdef ENOENT
					errno != ENOENT &&
#endif
#ifdef EROFS
					errno != EROFS &&
#endif
					astquery(2, "override protection %s for %s? ",
#ifdef ETXTBSY
					errno == ETXTBSY ? "``running program''" : 
#endif
					ftw->statb.st_uid != state.uid ? "``not owner''" :
					fmtmode(ftw->statb.st_mode & S_IPERM, 0) + 1, ftw->path) > 0)
					{
						nonempty(ftw);
						break;
					}
			}
			else
				close(n);
		}
#if _lib_fsync
		if (state.clobber && S_ISREG(ftw->statb.st_mode) && ftw->statb.st_size > 0)
		{
			if ((n = open(path, O_WRONLY)) < 0)
				error(ERROR_SYSTEM|2, "%s: cannot clear data", ftw->path);
			else
			{
				off_t		c = ftw->statb.st_size;

				static char	buf[SF_BUFSIZE];

				for (;;)
				{
					if (write(n, buf, sizeof(buf)) != sizeof(buf))
					{
						error(ERROR_SYSTEM|2, "%s: data clear error", ftw->path);
						break;
					}
					if (c <= sizeof(buf))
						break;
					c -= sizeof(buf);
				}
				fsync(n);
				close(n);
			}
		}
#endif
		if (remove(path))
		{
			nonempty(ftw);
			if (!state.force || state.interactive)
				error(ERROR_SYSTEM|2, "%s: not removed", ftw->path);
			else
				error_info.errors++;
		}
		break;
	}
	return 0;
}

int
b_rm(int argc, register char** argv, void* context)
{
	if (argc < 0)
	{
		state.interrupt = 1;
		return -1;
	}
	memset(&state, 0, sizeof(state));
	cmdinit(argv, context, ERROR_CATALOG, ERROR_NOTIFY);
	state.fs3d = fs3d(FS3D_TEST);
	state.terminal = isatty(0);
	for (;;)
	{
		switch (optget(argv, usage))
		{
		case 'd':
			state.directory = 1;
			continue;
		case 'f':
			state.force = 1;
			state.interactive = 0;
			continue;
		case 'i':
			state.interactive = 1;
			state.force = 0;
			continue;
		case 'r':
		case 'R':
			state.recursive = 1;
			if (state.fs3d)
			{
				state.fs3d = 0;
				fs3d(0);
			}
			continue;
		case 'F':
#if _lib_fsync
			state.clobber = 1;
#else
			error(1, "%s not implemented on this system", opt_info.name);
#endif
			continue;
		case 'u':
			state.unconditional = 1;
			continue;
		case 'v':
			state.verbose = 1;
			continue;
		case '?':
			error(ERROR_USAGE|4, "%s", opt_info.arg);
			continue;
		case ':':
			error(2, "%s", opt_info.arg);
			continue;
		}
		break;
	}
	argv += opt_info.index;
	if (*argv && streq(*argv, "-") && !streq(*(argv - 1), "--"))
		argv++;
	if (error_info.errors || !*argv)
		error(ERROR_USAGE|4, "%s", optusage(NiL));

	/*
	 * do it
	 */

	if (state.interactive)
		state.verbose = 0;
	state.uid = geteuid();
	state.unconditional = state.unconditional && state.recursive && state.force;
	ftwalk((char*)argv, rm, FTW_MULTIPLE|FTW_PHYSICAL|FTW_TWICE, NiL);
	return error_info.errors != 0;
}
