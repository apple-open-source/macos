/*-
 * Copyright (c) 1990, 1993, 1994
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
static char const copyright[] =
"@(#) Copyright (c) 1990, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)ps.c	8.4 (Berkeley) 4/2/94";
#endif
static const char rcsid[] =
	"$FreeBSD: ps.c,v 1.25 1998/06/30 21:34:14 phk Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/user.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <kvm.h>
#include <limits.h>
#include <nlist.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <locale.h>
#include <pwd.h>

#include "ps.h"

KINFO *kinfo;
struct varent *vhead, *vtail;

int	eval;			/* exit value */
int	cflag;			/* -c */
int	rawcpu;			/* -C */
int	sumrusage;		/* -S */
int	termwidth;		/* width of screen (0 == infinity) */
int	totwidth;		/* calculated width of requested variables */

static int needuser, needcomm, needenv;
#if defined(LAZY_PS)
static int forceuread=0;
#else
static int forceuread=1;
#endif

enum sort { DEFAULT, SORTMEM, SORTCPU } sortby = DEFAULT;

static char	*fmt __P((char **(*)(kvm_t *, const struct kinfo_proc *, int),
		    KINFO *, char *, int));
static char	*kludge_oldps_options __P((char *));
static int	 pscomp __P((const void *, const void *));
static void	 saveuser __P((KINFO *));
static void	 scanvars __P((void));
static void	 dynsizevars __P((KINFO *));
static void	 sizevars __P((void));
static void	 usage __P((void));

char dfmt[] = "pid tt state time command";
char jfmt[] = "user pid ppid pgid sess jobc state tt time command";
char lfmt[] = "uid pid ppid cpu pri nice vsz rss wchan state tt time command";
char   o1[] = "pid";
char   o2[] = "tt state time command";
char ufmt[] = "user pid %cpu %mem vsz rss tt state start time command";
char mfmt[] = "user pid tt %cpu state  pri stime utime command";
char vfmt[] = "pid state time sl re pagein vsz rss lim tsiz %cpu %mem command";

int mflg = 0; /* if -M option to display all mach threads */
int print_thread_num=0;
int print_all_thread=0;
kvm_t *kd;
int eflg=0;
int
main(argc, argv)
	int argc;
	char *argv[];
{
	struct kinfo_proc *kp, *kprocbuf;
	struct varent *vent;
	struct winsize ws;
	struct passwd *pwd;
	dev_t ttydev;
	pid_t pid;
	uid_t uid;
	int all, ch, flag, i, j, fmt, lineno, nentries, dropgid;
	int prtheader, wflag, what, xflg;
	char *nlistf, *memf, *swapf, errbuf[_POSIX2_LINE_MAX];
	int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0 };
	size_t bufSize = 0;
	size_t orig_bufSize = 0;
	int local_error=0;
	int retry_count = 0;

	(void) setlocale(LC_ALL, "");

	if ((ioctl(STDOUT_FILENO, TIOCGWINSZ, (char *)&ws) == -1 &&
	     ioctl(STDERR_FILENO, TIOCGWINSZ, (char *)&ws) == -1 &&
	     ioctl(STDIN_FILENO,  TIOCGWINSZ, (char *)&ws) == -1) ||
	     ws.ws_col == 0)
		termwidth = 79;
	else
		termwidth = ws.ws_col - 1;

	if( argc > 1 )
		argv[1] = kludge_oldps_options(argv[1]);

	all = fmt = prtheader = wflag = xflg = 0;
	pid = -1;
	uid = (uid_t) -1;
	ttydev = NODEV;
	dropgid = 0;
	memf = nlistf = swapf = _PATH_DEVNULL;
	memf = _PATH_MEM;
	while ((ch = getopt(argc, argv,
#if defined(LAZY_PS)
	    "aCcefghjLlM:mN:O:o:p:rSTt:U:uvW:wx")) != -1)
#else
	    "aCceghjLlMmO:o:p:rSTt:U:uvwx")) != -1)
#endif
		switch((char)ch) {
		case 'a':
			all = 1;
			break;
		case 'C':
			rawcpu = 1;
			break;
		case 'c':
			cflag = 1;
			break;
		case 'e':			/* XXX set ufmt */
			needenv = 1;
			eflg = 1;
			break;
		case 'g':
			break;			/* no-op */
		case 'h':
			prtheader = ws.ws_row > 5 ? ws.ws_row : 22;
			break;
		case 'j':
			parsefmt(jfmt);
			fmt = 1;
			jfmt[0] = '\0';
			break;
		case 'L':
			showkey();
			exit(0);
		case 'l':
			parsefmt(lfmt);
			fmt = 1;
			lfmt[0] = '\0';
			break;
		case 'M':
#if 0
			memf = optarg;
			dropgid = 1;
#else
			parsefmt(mfmt);
			fmt = 1;
			mfmt[0] = '\0';
			mflg  = 1;
#endif /* 0 */
			break;
		case 'm':
			sortby = SORTMEM;
			break;
		case 'N':
			nlistf = optarg;
			dropgid = 1;
			break;
		case 'O':
			parsefmt(o1);
			parsefmt(optarg);
			parsefmt(o2);
			o1[0] = o2[0] = '\0';
			fmt = 1;
			break;
		case 'o':
			parsefmt(optarg);
			fmt = 1;
			break;
#if defined(LAZY_PS)
		case 'f':
			if (getuid() == 0 || getgid() == 0)
			    forceuread = 1;
			break;
#endif
		case 'p':
		        /* Given more than one process requested with -p
			   or pid, ps will return information on only
			   the first requested process as is done by FreeBSD */
		        if (pid != -1) break;

			pid = atol(optarg);
			xflg = 1;
			break;
		case 'r':
			sortby = SORTCPU;
			break;
		case 'S':
			sumrusage = 1;
			break;
		case 'T':
			if ((optarg = ttyname(STDIN_FILENO)) == NULL)
				errx(1, "stdin: not a terminal");
			/* FALLTHROUGH */
		case 't': {
			struct stat sb;
			char *ttypath, pathbuf[MAXPATHLEN];

			if (strcmp(optarg, "co") == 0)
				ttypath = _PATH_CONSOLE;
			else if (*optarg != '/')
				(void)snprintf(ttypath = pathbuf,
				    sizeof(pathbuf), "%s%s", _PATH_TTY, optarg);
			else
				ttypath = optarg;
			if (stat(ttypath, &sb) == -1)
				err(1, "%s", ttypath);
			if (!S_ISCHR(sb.st_mode))
				errx(1, "%s: not a terminal", ttypath);
			ttydev = sb.st_rdev;
			break;
		}
		case 'U':
			pwd = getpwnam(optarg);
			if (pwd == NULL)
				errx(1, "%s: no such user", optarg);
			uid = pwd->pw_uid;
			endpwent();
			xflg++;		/* XXX: intuitive? */
			break;
		case 'u':
			parsefmt(ufmt);
			sortby = SORTCPU;
			fmt = 1;
			ufmt[0] = '\0';
			break;
		case 'v':
			parsefmt(vfmt);
			sortby = SORTMEM;
			fmt = 1;
			vfmt[0] = '\0';
			break;
		case 'W':
			swapf = optarg;
			dropgid = 1;
			break;
		case 'w':
			if (wflag)
				termwidth = UNLIMITED;
			else if (termwidth < 131)
				termwidth = 131;
			wflag++;
			break;
		case 'x':
			xflg = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

#define	BACKWARD_COMPATIBILITY
#ifdef	BACKWARD_COMPATIBILITY
	if (*argv) {
		nlistf = *argv;
		if (*++argv) {
			memf = *argv;
			if (*++argv)
				swapf = *argv;
		}
	}
#endif
	/*
	 * Discard setgid privileges if not the running kernel so that bad
	 * guys can't print interesting stuff from kernel memory.
	 */
	if (dropgid) {
		setgid(getgid());
		setuid(getuid());
	}
#if FIXME 
	kd = kvm_openfiles(nlistf, NULL, swapf, O_RDONLY, errbuf);
	if (kd == 0)
		errx(1, "%s", errbuf);
#endif /* FIXME */

	if (!fmt)
           parsefmt(dfmt);

	/* XXX - should be cleaner */
	if (!all && ttydev == NODEV && pid == -1 && uid == (uid_t)-1)
		uid = getuid();

	/*
	 * scan requested variables, noting what structures are needed,
	 * and adjusting header widths as appropiate.
	 */
	scanvars();

	/*
	 * get proc list
	 */
	if (uid != (uid_t) -1) {
		what = KERN_PROC_UID;
		flag = uid;
	} else if (ttydev != NODEV) {
		what = KERN_PROC_TTY;
		flag = ttydev;
	} else if (pid != -1) {
		what = KERN_PROC_PID;
		flag = pid;
	} else {
		what = KERN_PROC_ALL;
		flag = 0;
	}
	/*
	 * select procs
	 */
#if FIXME
	if ((kp = kvm_getprocs(kd, what, flag, &nentries)) == 0)
		errx(1, "%s", kvm_geterr(kd));
#else /* FIXME */

    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = what;
    mib[3] = flag;

    if (sysctl(mib, 4, NULL, &bufSize, NULL, 0) < 0) {
        perror("Failure calling sysctl");
        return 0;
    }

    kprocbuf= kp = (struct kinfo_proc *)malloc(bufSize);

    retry_count = 0;
    orig_bufSize = bufSize;
   for(retry_count=0; ; retry_count++) {
    /* retry for transient errors due to load in the system */
    local_error = 0;
    bufSize = orig_bufSize;
    if ((local_error = sysctl(mib, 4, kp, &bufSize, NULL, 0)) < 0) {
	if (retry_count < 1000) {
		/* 1 sec back off */
		sleep(1);
		continue;
	}
        perror("Failure calling sysctl");
        return 0;
    } else if (local_error == 0) {
	break;
    }
    /* 1 sec back off */
    sleep(1);
   }

    /* This has to be after the second sysctl since the bufSize
       may have changed.  */
    nentries = bufSize/ sizeof(struct kinfo_proc);

#endif /* FIXME */
	if ((kinfo = malloc(nentries * sizeof(KINFO))) == NULL)
		err(1, NULL);
	memset(kinfo, 0, (nentries * sizeof(*kinfo)));
	for (i = nentries; --i >= 0; ++kp) {
		kinfo[i].ki_p = kp;
#if 1
		get_task_info(&kinfo[i]);
#endif /* 1 */
		if (needuser)
			saveuser(&kinfo[i]);
		dynsizevars(&kinfo[i]);
	}
	sizevars();

	/*
	 * print header
	 */
	printheader();
	if (nentries == 0)
		exit(0);
	/*
	 * sort proc list
	 */
	qsort(kinfo, nentries, sizeof(KINFO), pscomp);
	/*
	 * for each proc, call each variable output function.
	 */
	for (i = lineno = 0; i < nentries; i++) {
		if(kinfo[i].invalid_tinfo || kinfo[i].invalid_thinfo)
			continue;
		if (KI_PROC(&kinfo[i])->p_pid == 0)
			continue;
		if (xflg == 0 && (KI_EPROC(&kinfo[i])->e_tdev == NODEV ||
		    (KI_PROC(&kinfo[i])->p_flag & P_CONTROLT ) == 0))
			continue;
		if(mflg) {
			print_all_thread = 1;
			for(j=0; j < kinfo[i].thread_count; j++) {
			print_thread_num = j;
			for (vent = vhead; vent; vent = vent->next) {
				(vent->var->oproc)(&kinfo[i], vent);
				if (vent->next != NULL)
					(void)putchar(' ');
			}

			(void)putchar('\n');
			if (prtheader && lineno++ == prtheader - 4) {
				(void)putchar('\n');
				printheader();
				lineno = 0;
			}
			}
			print_all_thread = 0;
		} else {
			for (vent = vhead; vent; vent = vent->next) {
				(vent->var->oproc)(&kinfo[i], vent);
				if (vent->next != NULL)
					(void)putchar(' ');
			}

			(void)putchar('\n');
			if (prtheader && lineno++ == prtheader - 4) {
				(void)putchar('\n');
				printheader();
				lineno = 0;
			}
		}
	}
cleanup:
	for (i = 0; i < nentries; i++) {
		if(kinfo[i].thread_count)
			free(kinfo[i].thval);	
	}
	free(kprocbuf);
	free(kinfo);
	exit(eval);
}

static void
scanvars() 
{
	struct varent *vent;
	VAR *v;

        struct varent *prev, *curr, *command;

        /* First we make sure that COMMAND is the last element
	   in the list */
        command = NULL;
        prev = NULL;
        curr = vhead;
        while (curr) {
            if (!strcmp(curr->var->header, "COMMAND")) {
                if (prev == NULL || (curr->next == NULL)) {
                    break;
                }
                else {
                    command = curr;
                    prev->next = curr->next;
                }
            }
            prev = curr;
            curr = curr->next;
        }

        if (command) {
            prev->next = command;
            command->next = NULL;
        }
    
        /* Now we set the spacing information */
	for (vent = vhead; vent; vent = vent->next) {
		v = vent->var;
		if (v->flag & DSIZ) {
			v->dwidth = v->width;
			v->width = 0;
		}
		if (v->flag & USER)
			needuser = 1;
		if (v->flag & COMM)
			needcomm = 1;
	}
}

static void
dynsizevars(ki)
	KINFO *ki;
{
	struct varent *vent;
	VAR *v;
	int i;

	for (vent = vhead; vent; vent = vent->next) {
		v = vent->var;
		if (!(v->flag & DSIZ))
			continue;
		i = (v->sproc)( ki);
		if (v->width < i)
			v->width = i;
		if (v->width > v->dwidth)
			v->width = v->dwidth;
	}
}

static void
sizevars()
{
	struct varent *vent;
	VAR *v;
	int i;

	for (vent = vhead; vent; vent = vent->next) {
		v = vent->var;
		i = strlen(v->header);
		if (v->width < i)
			v->width = i;
		totwidth += v->width + 1;	/* +1 for space */
	}
	totwidth--;
}

static char *
fmt(fn, ki, comm, maxlen)
	char **(*fn) __P((kvm_t *, const struct kinfo_proc *, int));
	KINFO *ki;
	char *comm;
	int maxlen;
{
	char *s;

	if ((s =
	    fmt_argv((*fn)(kd, ki->ki_p, termwidth), comm, maxlen)) == NULL)
		err(1, NULL);
	return (s);
}

#define UREADOK(ki)	(forceuread || (KI_PROC(ki)->p_flag & P_INMEM))

static void
saveuser(ki)
	KINFO *ki;
{
	struct pstats pstats;
	struct usave *usp;
#if FIXME
	struct user *u_addr = (struct user *)USRSTACK;
#else /* FIXME */
	struct user *u_addr = (struct user *)0;
#endif /* FIXME */

	usp = &ki->ki_u;
#if FIXME
	if (UREADOK(ki) && kvm_uread(kd, KI_PROC(ki), (unsigned long)&u_addr->u_stats,
	    (char *)&pstats, sizeof(pstats)) == sizeof(pstats)) 
	{
		/*
		 * The u-area might be swapped out, and we can't get
		 * at it because we have a crashdump and no swap.
		 * If it's here fill in these fields, otherwise, just
		 * leave them 0.
		 */
		usp->u_start = pstats.p_start;
		usp->u_ru = pstats.p_ru;
		usp->u_cru = pstats.p_cru;
		usp->u_valid = 1;
	} else
		usp->u_valid = 0;
#else /* FIXME */
		usp->u_valid = 0;
#endif /* FIXME */
	/*
	 * save arguments if needed
	 */
#if FIXME
	if (needcomm && UREADOK(ki)) {
	    ki->ki_args = fmt(kvm_getargv, ki, KI_PROC(ki)->p_comm,
		MAXCOMLEN);
	} else if (needcomm) {
	    ki->ki_args = malloc(strlen(KI_PROC(ki)->p_comm) + 3);
	    sprintf(ki->ki_args, "(%s)", KI_PROC(ki)->p_comm);
    } else {
	    ki->ki_args = NULL;
    }
#else /* FIXME */
	    ki->ki_args = malloc(strlen(KI_PROC(ki)->p_comm) + 3);
	    sprintf(ki->ki_args, "%s", KI_PROC(ki)->p_comm);
	    //ki->ki_args = malloc(10);
	    //strcpy(ki->ki_args, "()");
#endif /* FIXME */
#if FIXME
   if (needenv && UREADOK(ki)) {
	    ki->ki_env = fmt(kvm_getenvv, ki, (char *)NULL, 0);
    } else if (needenv) {
	    ki->ki_env = malloc(3);
	    strcpy(ki->ki_env, "()");
    } else {
	    ki->ki_env = NULL;
    }
#else /* FIXME */
	    ki->ki_env = malloc(10);
	    strcpy(ki->ki_env, "");
#endif /* FIXME */
}

static int
pscomp(a, b)
	const void *a, *b;
{
	int i;
#if FIXME
#define VSIZE(k) (KI_EPROC(k)->e_vm.vm_dsize + KI_EPROC(k)->e_vm.vm_ssize + \
		  KI_EPROC(k)->e_vm.vm_tsize)
#else /* FIXME */
#define VSIZE(k) ((k)->tasks_info.resident_size)

#endif /* FIXME */

	if (sortby == SORTCPU)
		return (getpcpu((KINFO *)b) - getpcpu((KINFO *)a));
	if (sortby == SORTMEM)
		return (VSIZE((KINFO *)b) - VSIZE((KINFO *)a));
	i =  KI_EPROC((KINFO *)a)->e_tdev - KI_EPROC((KINFO *)b)->e_tdev;
	if (i == 0)
		i = KI_PROC((KINFO *)a)->p_pid - KI_PROC((KINFO *)b)->p_pid;
	return (i);
}

/*
 * ICK (all for getopt), would rather hide the ugliness
 * here than taint the main code.
 *
 *  ps foo -> ps -foo
 *  ps 34 -> ps -p34
 *
 * The old convention that 't' with no trailing tty arg means the users
 * tty, is only supported if argv[1] doesn't begin with a '-'.  This same
 * feature is available with the option 'T', which takes no argument.
 */
static char *
kludge_oldps_options(s)
	char *s;
{
	size_t len;
	char *newopts, *ns, *cp;

	len = strlen(s);
	if ((newopts = ns = malloc(len + 2)) == NULL)
		err(1, NULL);
	/*
	 * options begin with '-'
	 */
	if (*s != '-')
		*ns++ = '-';	/* add option flag */
	/*
	 * gaze to end of argv[1]
	 */
	cp = s + len - 1;
	/*
	 * if last letter is a 't' flag with no argument (in the context
	 * of the oldps options -- option string NOT starting with a '-' --
	 * then convert to 'T' (meaning *this* terminal, i.e. ttyname(0)).
	 *
	 * However, if a flag accepting a string argument is found in the
	 * option string, the remainder of the string is the argument to
	 * that flag; do not modify that argument.
	 */
	if (strcspn(s, "MNOoUW") == len && *cp == 't' && *s != '-')
		*cp = 'T';
	else {
		/*
		 * otherwise check for trailing number, which *may* be a
		 * pid.
		 */
		while (cp >= s && isdigit(*cp))
			--cp;
	}
	cp++;
	memmove(ns, s, (size_t)(cp - s));	/* copy up to trailing number */
	ns += cp - s;
	/*
	 * if there's a trailing number, and not a preceding 'p' (pid) or
	 * 't' (tty) flag, then assume it's a pid and insert a 'p' flag.
	 */
	if (isdigit(*cp) &&
	    (cp == s || (cp[-1] != 't' && cp[-1] != 'p')) &&
	    (cp - 1 == s || cp[-2] != 't'))
		*ns++ = 'p';
	(void)strcpy(ns, cp);		/* and append the number */

	return (newopts);
}

static void
usage()
{

	(void)fprintf(stderr, "%s\n%s\n%s\n",
	    "usage: ps [-aChjlmMrSTuvwx] [-O|o fmt] [-p pid] [-t tty] [-U user]",
	    "          [-N system] [-W swap]",
	    "       ps [-L]");
	exit(1);
}
