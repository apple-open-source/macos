/*
 * init.c - main loop and initialization routines
 *
 * This file is part of zsh, the Z shell.
 *
 * Copyright (c) 1992-1996 Paul Falstad
 * All rights reserved.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and to distribute modified versions of this software for any
 * purpose, provided that the above copyright notice and the following
 * two paragraphs appear in all copies of this software.
 *
 * In no event shall Paul Falstad or the Zsh Development Group be liable
 * to any party for direct, indirect, special, incidental, or consequential
 * damages arising out of the use of this software and its documentation,
 * even if Paul Falstad and the Zsh Development Group have been advised of
 * the possibility of such damage.
 *
 * Paul Falstad and the Zsh Development Group specifically disclaim any
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose.  The software
 * provided hereunder is on an "as is" basis, and Paul Falstad and the
 * Zsh Development Group have no obligation to provide maintenance,
 * support, updates, enhancements, or modifications.
 *
 */

#define GLOBALS
#include "zsh.h"

static int noexitct = 0;

/**/
int
main(int argc, char **argv)
{
    char **t;
#ifdef USE_LOCALE
    setlocale(LC_ALL, "");
#endif

    global_permalloc();

    for (t = argv; *t; *t = metafy(*t, -1, META_ALLOC), t++);

    if (!(zsh_name = strrchr(argv[0], '/')))
	zsh_name = argv[0];
    else
	zsh_name++;
    if (*zsh_name == '-')
	zsh_name++;

    fdtable_size = OPEN_MAX;
    fdtable = zcalloc(fdtable_size);

    emulate(zsh_name, 1);   /* initialises most options */
    opts[LOGINSHELL] = (**argv == '-');
    opts[MONITOR] = 1;   /* may be unset in init_io() */
    opts[PRIVILEGED] = (getuid() != geteuid() || getgid() != getegid());
    opts[USEZLE] = 1;   /* may be unset in init_io() */
    parseargs(argv);   /* sets INTERACTIVE, SHINSTDIN and SINGLECOMMAND */

    SHTTY = -1;
    init_io();
    setupvals();
    init_signals();
    global_heapalloc();
    run_init_scripts();
    init_misc();

    for (;;) {
	do
	    loop(1,0);
	while (tok != ENDINPUT);
	if (!(isset(IGNOREEOF) && interact)) {
#if 0
	    if (interact)
		fputs(islogin ? "logout\n" : "exit\n", shout);
#endif
	    zexit(lastval, 0);
	    continue;
	}
	noexitct++;
	if (noexitct >= 10) {
	    stopmsg = 1;
	    zexit(lastval, 0);
	}
	zerrnam("zsh", (!islogin) ? "use 'exit' to exit."
		: "use 'logout' to logout.", NULL, 0);
    }
}

/* keep executing lists until EOF found */

/**/
void
loop(int toplevel, int justonce)
{
    List list;
#ifdef DEBUG
    int oasp = toplevel ? 0 : alloc_stackp;
#endif

    pushheap();
    for (;;) {
	freeheap();
	errflag = 0;
	if (isset(SHINSTDIN)) {
	    setblock_stdin();
	    if (interact)
		preprompt();
	}
	hbegin();		/* init history mech        */
	intr();			/* interrupts on            */
	lexinit();              /* initialize lexical state */
	if (!(list = parse_event())) {	/* if we couldn't parse a list */
	    hend();
	    if ((tok == ENDINPUT && !errflag) || justonce)
		break;
	    continue;
	}
	if (hend()) {
	    int toksav = tok;
	    List prelist;

	    if (toplevel && (prelist = getshfunc("preexec")) != &dummy_list) {
		Histent he = gethistent(curhist);
		LinkList args;
		PERMALLOC {
		    args = newlinklist();
		    addlinknode(args, "preexec");
		    if (he && he->text)
			addlinknode(args, he->text);
		} LASTALLOC;
		doshfunc(prelist, args, 0, 1);
		freelinklist(args, (FreeFunc) NULL);
		errflag = 0;
	    }
	    if (stopmsg)	/* unset 'you have stopped jobs' flag */
		stopmsg--;
	    execlist(list, 0, 0);
	    tok = toksav;
	    if (toplevel)
		noexitct = 0;
	}
	DPUTS(alloc_stackp != oasp, "BUG: alloc_stackp changed in loop()");
	if (ferror(stderr)) {
	    zerr("write error", NULL, 0);
	    clearerr(stderr);
	}
	if (subsh)		/* how'd we get this far in a subshell? */
	    exit(lastval);
	if (((!interact || sourcelevel) && errflag) || retflag)
	    break;
	if (trapreturn) {
	    lastval = trapreturn;
	    trapreturn = 0;
	}
	if (isset(SINGLECOMMAND) && toplevel) {
	    if (sigtrapped[SIGEXIT])
		dotrap(SIGEXIT);
	    exit(lastval);
	}
	if (justonce)
	    break;
    }
    popheap();
}

/**/
void
emulate(const char *zsh_name, int fully)
{
    int optno;

    /* Work out the new emulation mode */
    if(!strcmp(zsh_name, "csh"))
	emulation = EMULATE_CSH;
    else if(!strcmp(zsh_name, "ksh"))
	emulation = EMULATE_KSH;
    else if(!strcmp(zsh_name, "sh"))
	emulation = EMULATE_SH;
    else
	emulation = EMULATE_ZSH;

    /* Set options: each non-special option is set according to the *
     * current emulation mode if either it is considered relevant   *
     * to emulation or we are doing a full emulation (as indicated  *
     * by the `fully' parameter).                                   */
    for(optno = OPT_SIZE; --optno; )
	if((fully && !(optns[optno].flags & OPT_SPECIAL)) ||
	    	(optns[optno].flags & OPT_EMULATE))
	    opts[optno] = defset(optno);
}

static char *cmd;

/**/
void
parseargs(char **argv)
{
    char **x;
    int action, optno;
    LinkList paramlist;
    int bourne = (emulation == EMULATE_KSH || emulation == EMULATE_SH);

    hackzero = argzero = *argv++;
    SHIN = 0;

    /* There's a bit of trickery with opts[INTERACTIVE] here.  It starts *
     * at a value of 2 (instead of 1) or 0.  If it is explicitly set on  *
     * the command line, it goes to 1 or 0.  If input is coming from     *
     * somewhere that normally makes the shell non-interactive, we do    *
     * "opts[INTERACTIVE] &= 1", so that only a *default* on state will  *
     * be changed.  At the end of the function, a value of 2 gets        *
     * changed to 1.                                                     */
    opts[INTERACTIVE] = isatty(0) ? 2 : 0;
    opts[SHINSTDIN] = 0;
    opts[SINGLECOMMAND] = 0;

    /* loop through command line options (begins with "-" or "+") */
    while (*argv && (**argv == '-' || **argv == '+')) {
	char *args = *argv;
	action = (**argv == '-');
	if(!argv[0][1])
	    *argv = "--";
	while (*++*argv) {
	    /* The pseudo-option `--' signifies the end of options. *
	     * `-b' does too, csh-style, unless we're emulating a   *
	     * Bourne style shell.                                  */
	    if (**argv == '-' || (!bourne && **argv == 'b')) {
		argv++;
		goto doneoptions;
	    }

	    if (**argv == 'c') {         /* -c command */
		cmd = *argv;
		opts[INTERACTIVE] &= 1;
		opts[SHINSTDIN] = 0;
	    } else if (**argv == 'o') {
		if (!*++*argv)
		    argv++;
		if (!*argv) {
		    zerr("string expected after -o", NULL, 0);
		    exit(1);
		}
		if(!(optno = optlookup(*argv)))
		    zerr("no such option: %s", *argv, 0);
		else
		    dosetopt(optno, action, 1);
              break;
	    } else if (isspace(STOUC(**argv))) {
		/* zsh's typtab not yet set, have to use ctype */
		while (*++*argv)
		    if (!isspace(STOUC(**argv))) {
			zerr("bad option string: `%s'", args, 0);
			exit(1);
		    }
		break;
	    } else {
	    	if (!(optno = optlookupc(**argv))) {
		    zerr("bad option: -%c", NULL, **argv);
		    exit(1);
		} else
		    dosetopt(optno, action, 1);
	    }
	}
	argv++;
    }
    doneoptions:
    paramlist = newlinklist();
    if (cmd) {
	if (!*argv) {
	    zerr("string expected after -%s", cmd, 0);
	    exit(1);
	}
	cmd = *argv++;
    }
    if (*argv) {
	if (unset(SHINSTDIN)) {
	    argzero = *argv;
	    if (!cmd)
		SHIN = movefd(open(unmeta(argzero), O_RDONLY));
	    if (SHIN == -1) {
		zerr("can't open input file: %s", argzero, 0);
		exit(1);
	    }
	    opts[INTERACTIVE] &= 1;
	    argv++;
	}
	while (*argv)
	    addlinknode(paramlist, ztrdup(*argv++));
    } else
	opts[SHINSTDIN] = 1;
    if(isset(SINGLECOMMAND))
	opts[INTERACTIVE] &= 1;
    opts[INTERACTIVE] = !!opts[INTERACTIVE];
    pparams = x = (char **) zcalloc((countlinknodes(paramlist) + 1) * sizeof(char *));

    while ((*x++ = (char *)getlinknode(paramlist)));
    free(paramlist);
    argzero = ztrdup(argzero);
}


/**/
void
init_io(void)
{
    long ttpgrp;
    static char outbuf[BUFSIZ], errbuf[BUFSIZ];

#ifdef RSH_BUG_WORKAROUND
    int i;
#endif

/* stdout, stderr fully buffered */
#ifdef _IOFBF
    setvbuf(stdout, outbuf, _IOFBF, BUFSIZ);
    setvbuf(stderr, errbuf, _IOFBF, BUFSIZ);
#else
    setbuffer(stdout, outbuf, BUFSIZ);
    setbuffer(stderr, errbuf, BUFSIZ);
#endif

/* This works around a bug in some versions of in.rshd. *
 * Currently this is not defined by default.            */
#ifdef RSH_BUG_WORKAROUND
    if (cmd) {
	for (i = 3; i < 10; i++)
	    close(i);
    }
#endif

    if (shout) {
	fclose(shout);
	shout = 0;
    }
    if (SHTTY != -1) {
	zclose(SHTTY);
	SHTTY = -1;
    }

    /* Send xtrace output to stderr -- see execcmd() */
    xtrerr = stderr;

    /* Make sure the tty is opened read/write. */
    if (isatty(0)) {
	zsfree(ttystrname);
	if ((ttystrname = ztrdup(ttyname(0)))) {
	    SHTTY = movefd(open(ttystrname, O_RDWR | O_NOCTTY));
#ifdef TIOCNXCL
	    /*
	     * See if the terminal claims to be busy.  If so, and fd 0
	     * is a terminal, try and set non-exclusive use for that.
	     * This is something to do with Solaris over-cleverness.
	     */
	    if (SHTTY == -1 && errno == EBUSY)
		ioctl(0, TIOCNXCL, 0);
#endif
	}
	/*
	 * xterm, rxvt and probably all terminal emulators except
	 * dtterm on Solaris 2.6 & 7 have a bug. Applications are
	 * unable to open /dev/tty or /dev/pts/<terminal number here>
	 * because something in Sun's STREAMS modules doesn't like
	 * it. The open() call fails with EBUSY which is not even
	 * listed as a possibility in the open(2) man page.  So we'll
	 * try to outsmart The Company.  -- <dave@srce.hr>
	 *
	 * Presumably there's no harm trying this on any OS, given that
	 * isatty(0) worked but opening the tty didn't.  Possibly we won't
	 * get the tty read/write, but it's the best we can do -- pws
	 *
	 * Try both stdin and stdout before trying /dev/tty. -- Bart
	 */
#if defined(HAVE_FCNTL_H) && defined(F_GETFL)
#define rdwrtty(fd)	((fcntl(fd, F_GETFL, 0) & O_RDWR) == O_RDWR)
#else
#define rdwrtty(fd)	1
#endif
	if (SHTTY == -1 && rdwrtty(0)) {
	    SHTTY = movefd(dup(0));
	}
    }
    if (SHTTY == -1 && isatty(1) && rdwrtty(1) &&
	(SHTTY = movefd(dup(1))) != -1) {
	zsfree(ttystrname);
	ttystrname = ztrdup(ttyname(1));
    }
    if (SHTTY == -1 &&
	(SHTTY = movefd(open("/dev/tty", O_RDWR | O_NOCTTY))) != -1) {
	zsfree(ttystrname);
	ttystrname = ztrdup(ttyname(SHTTY));
    }
    if (SHTTY == -1) {
	zsfree(ttystrname);
	ttystrname = ztrdup("");
    } else if (!ttystrname) {
	ttystrname = ztrdup("/dev/tty");
    }

    /* We will only use zle if shell is interactive, *
     * SHTTY != -1, and shout != 0                   */
    if (interact && SHTTY != -1) {
	init_shout();
	if(!shout)
	    opts[USEZLE] = 0;
    } else
	opts[USEZLE] = 0;

#ifdef JOB_CONTROL
    /* If interactive, make the shell the foreground process */
    if (opts[MONITOR] && interact && (SHTTY != -1)) {
	if ((mypgrp = GETPGRP()) > 0) {
	    while ((ttpgrp = gettygrp()) != -1 && ttpgrp != mypgrp) {
		sleep(1);	/* give parent time to change pgrp */
		mypgrp = GETPGRP();
		if (mypgrp == mypid)
		    attachtty(mypgrp);
		if (mypgrp == gettygrp())
		    break;
		killpg(mypgrp, SIGTTIN);
		mypgrp = GETPGRP();
	    }
	} else
	    opts[MONITOR] = 0;
    } else
	opts[MONITOR] = 0;
#else
    opts[MONITOR] = 0;
#endif
}

/**/
void
init_shout(void)
{
    static char shoutbuf[BUFSIZ];
#if defined(JOB_CONTROL) && defined(TIOCSETD) && defined(NTTYDISC)
    int ldisc = NTTYDISC;

    ioctl(SHTTY, TIOCSETD, (char *)&ldisc);
#endif

    /* Associate terminal file descriptor with a FILE pointer */
    shout = fdopen(SHTTY, "w");
#ifdef _IOFBF
    setvbuf(shout, shoutbuf, _IOFBF, BUFSIZ);
#endif
  
    gettyinfo(&shttyinfo);	/* get tty state */
#if defined(__sgi)
    if (shttyinfo.tio.c_cc[VSWTCH] <= 0)	/* hack for irises */
	shttyinfo.tio.c_cc[VSWTCH] = CSWTCH;
#endif
}

/* flag for whether terminal has automargin (wraparound) capability */
extern hasam;

/* Initialise termcap */

/**/
int
init_term(void)
{
#ifndef TGETENT_ACCEPTS_NULL
    static char termbuf[2048];	/* the termcap buffer */
#endif

    if (!*term) {
	termflags |= TERM_UNKNOWN;
	return 0;
    }

    /* unset zle if using zsh under emacs */
    if (!strcmp(term, "emacs"))
	opts[USEZLE] = 0;

#ifdef TGETENT_ACCEPTS_NULL
    /* If possible, we let tgetent allocate its own termcap buffer */
    if (tgetent(NULL, term) != 1) {
#else
    if (tgetent(termbuf, term) != 1) {
#endif

	if (isset(INTERACTIVE))
	    zerr("can't find termcap info for %s", term, 0);
	errflag = 0;
	termflags |= TERM_BAD;
	return 0;
    } else {
	char tbuf[1024], *pp;
	int t0;

	termflags &= ~TERM_BAD;
	termflags &= ~TERM_UNKNOWN;
	for (t0 = 0; t0 != TC_COUNT; t0++) {
	    pp = tbuf;
	    zsfree(tcstr[t0]);
	/* AIX tgetstr() ignores second argument */
	    if (!(pp = tgetstr(tccapnams[t0], &pp)))
		tcstr[t0] = NULL, tclen[t0] = 0;
	    else {
		tclen[t0] = strlen(pp);
		tcstr[t0] = (char *) zalloc(tclen[t0] + 1);
		memcpy(tcstr[t0], pp, tclen[t0] + 1);
	    }
	}

	/* check whether terminal has automargin (wraparound) capability */
	hasam = tgetflag("am");

	tclines = tgetnum("li");
	tccolumns = tgetnum("co");

	/* if there's no termcap entry for cursor up, use single line mode: *
	 * this is flagged by termflags which is examined in zle_refresh.c  *
	 */
	if (tccan(TCUP))
	    termflags &= ~TERM_NOUP;
	else {
	    tcstr[TCUP] = NULL;
	    termflags |= TERM_NOUP;
	}

	/* if there's no termcap entry for cursor left, use \b. */
	if (!tccan(TCLEFT)) {
	    tcstr[TCLEFT] = ztrdup("\b");
	    tclen[TCLEFT] = 1;
	}

	/* if the termcap entry for down is \n, don't use it. */
	if (tccan(TCDOWN) && tcstr[TCDOWN][0] == '\n') {
	    tclen[TCDOWN] = 0;
	    zsfree(tcstr[TCDOWN]);
	    tcstr[TCDOWN] = NULL;
	}

	/* if there's no termcap entry for clear, use ^L. */
	if (!tccan(TCCLEARSCREEN)) {
	    tcstr[TCCLEARSCREEN] = ztrdup("\14");
	    tclen[TCCLEARSCREEN] = 1;
	}
    }
    return 1;
}

/* Initialize lots of global variables and hash tables */

/**/
void
setupvals(void)
{
    struct passwd *pswd;
    struct timezone dummy_tz;
    char *ptr;
#ifdef HAVE_GETRLIMIT
    int i;
#endif

    lineno = 1;
    noeval = 0;
    curhist = 0;
    histsiz = DEFAULT_HISTSIZE;
    inithist();
    clwords = (char **) zcalloc((clwsize = 16) * sizeof(char *));

    cmdstack = (unsigned char *) zalloc(256);
    cmdsp = 0;

    bangchar = '!';
    hashchar = '#';
    hatchar = '^';
    termflags = TERM_UNKNOWN;
    curjob = prevjob = coprocin = coprocout = -1;
    gettimeofday(&shtimer, &dummy_tz);	/* init $SECONDS */
    srand((unsigned int)(shtimer.tv_sec + shtimer.tv_usec)); /* seed $RANDOM */

    hostnam     = (char *) zalloc(256);
    gethostname(hostnam, 256);

    /* Set default path */
    path    = (char **) zalloc(sizeof(*path) * 5);
    path[0] = ztrdup("/bin");
    path[1] = ztrdup("/usr/bin");
    path[2] = ztrdup("/usr/ucb");
    path[3] = ztrdup("/usr/local/bin");
    path[4] = NULL;

    cdpath   = mkarray(NULL);
    manpath  = mkarray(NULL);
    fignore  = mkarray(NULL);
    fpath    = mkarray(NULL);
    mailpath = mkarray(NULL);
    watch    = mkarray(NULL);
    psvar    = mkarray(NULL);

    /* Set default prompts */
    if (opts[INTERACTIVE]) {
	prompt  = ztrdup("%m%# ");
	prompt2 = ztrdup("%_> ");
    } else {
	prompt = ztrdup("");
	prompt2 = ztrdup("");
    }
    prompt3 = ztrdup("?# ");
    prompt4 = ztrdup("+ ");
    sprompt = ztrdup("zsh: correct '%R' to '%r' [nyae]? ");

    ifs         = ztrdup(DEFAULT_IFS);
    wordchars   = ztrdup(DEFAULT_WORDCHARS);
    postedit    = ztrdup("");
    underscore  = ztrdup("");

    zoptarg = ztrdup("");
    zoptind = 1;
    schedcmds = NULL;

    ppid  = (zlong) getppid();
    mypid = (zlong) getpid();
    term  = ztrdup("");

    /* The following variable assignments cause zsh to behave more *
     * like Bourne and Korn shells when invoked as "sh" or "ksh".  *
     * NULLCMD=":" and READNULLCMD=":"                             */

    if (emulation == EMULATE_KSH || emulation == EMULATE_SH) {
	nullcmd     = ztrdup(":");
	readnullcmd = ztrdup(":");
    } else {
	nullcmd     = ztrdup("cat");
	readnullcmd = ztrdup("more");
    }

    /* We cache the uid so we know when to *
     * recheck the info for `USERNAME'     */
    cached_uid = getuid();

    /* Get password entry and set info for `HOME' and `USERNAME' */
    if ((pswd = getpwuid(cached_uid))) {
	home = metafy(pswd->pw_dir, -1, META_DUP);
	cached_username = ztrdup(pswd->pw_name);
    } else {
	home = ztrdup("/");
	cached_username = ztrdup("");
    }

    /* Try a cheap test to see if we can *
     * initialize `PWD' from `HOME'      */
    if (ispwd(home))
	pwd = ztrdup(home);
    else if ((ptr = zgetenv("PWD")) && ispwd(ptr))
	pwd = ztrdup(ptr);
    else
	pwd = metafy(zgetcwd(), -1, META_REALLOC);

    oldpwd = ztrdup(pwd);  /* initialize `OLDPWD' = `PWD' */

    inittyptab();     /* initialize the ztypes table */
    initlextabs();    /* initialize lexing tables    */

    createreswdtable();     /* create hash table for reserved words    */
    createaliastable();     /* create hash table for aliases           */
    createcmdnamtable();    /* create hash table for external commands */
    createshfunctable();    /* create hash table for shell functions   */
    createbuiltintable();   /* create hash table for builtin commands  */
    createcompctltable();   /* create hash table for compctls          */
    createnameddirtable();  /* create hash table for named directories */
    createparamtable();     /* create paramater hash table             */

#ifdef TIOCGWINSZ
    adjustwinsize(0);
#else
    /* columns and lines are normally zero, unless something different *
     * was inhereted from the environment.  If either of them are zero *
     * the setiparam calls below set them to the defaults from termcap */
    setiparam("COLUMNS", columns);
    setiparam("LINES", lines);
#endif

    /* create hash table for multi-character emacs bindings */
    createemkeybindtable();

    /* create hash table for multi-character vi bindings */
    createvikeybindtable();

    initkeybindings();	    /* initialize key bindings */
    compctlsetup();

#ifdef HAVE_GETRLIMIT
    for (i = 0; i != RLIM_NLIMITS; i++) {
	getrlimit(i, current_limits + i);
	limits[i] = current_limits[i];
    }
#endif

    breaks = loops = 0;
    lastmailcheck = time(NULL);
    locallist = NULL;
    locallevel = sourcelevel = 0;
    trapreturn = 0;
    noerrexit = -1;
    nohistsave = 1;
    dirstack = newlinklist();
    bufstack = newlinklist();
    hsubl = hsubr = NULL;
    lastpid = 0;
    bshin = SHIN ? fdopen(SHIN, "r") : stdin;
    if (isset(SHINSTDIN) && !SHIN && unset(INTERACTIVE)) {
#ifdef _IONBF
	setvbuf(stdin, NULL, _IONBF, 0);
#else
	setlinebuf(stdin);
#endif
    }

    times(&shtms);
}

/* Initialize signal handling */

/**/
void
init_signals(void)
{
    intr();

#ifndef QDEBUG
    signal_ignore(SIGQUIT);
#endif

    install_handler(SIGHUP);
    install_handler(SIGCHLD);
#ifdef SIGWINCH
    install_handler(SIGWINCH);
#endif
    if (interact) {
	install_handler(SIGALRM);
	signal_ignore(SIGTERM);
    }
    if (jobbing) {
	long ttypgrp;

	while ((ttypgrp = gettygrp()) != -1 && ttypgrp != mypgrp)
	    kill(0, SIGTTIN);
	if (ttypgrp == -1) {
	    opts[MONITOR] = 0;
	} else {
	    signal_ignore(SIGTTOU);
	    signal_ignore(SIGTSTP);
	    signal_ignore(SIGTTIN);
	    attachtty(mypgrp);
	}
    }
    if (islogin) {
	signal_setmask(signal_mask(0));
    } else if (interact) {
	sigset_t set;

	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGQUIT);
	signal_unblock(set);
    }
}

/* Source the init scripts.  If called as "ksh" or "sh"  *
 * then we source the standard sh/ksh scripts instead of *
 * the standard zsh scripts                              */

/**/
void
run_init_scripts(void)
{
    noerrexit = -1;

    if (emulation == EMULATE_KSH || emulation == EMULATE_SH) {
	if (islogin)
	    source("/etc/profile");
	if (unset(PRIVILEGED)) {
	    char *s = getsparam("ENV");
	    if (islogin)
		sourcehome(".profile");
	    noerrs = 2;
	    if (s && !parsestr(s)) {
		singsub(&s);
		noerrs = 0;
		source(s);
	    }
	    noerrs = 0;
	} else
	    source("/etc/suid_profile");
    } else {
#ifdef GLOBAL_ZSHENV
	source(GLOBAL_ZSHENV);
#endif
	if (isset(RCS)) {
	    if (unset(PRIVILEGED))
		sourcehome(".zshenv");
	    if (islogin) {
#ifdef GLOBAL_ZPROFILE
		source(GLOBAL_ZPROFILE);
#endif
		if (unset(PRIVILEGED))
		    sourcehome(".zprofile");
	    }
	    if (interact) {
#ifdef GLOBAL_ZSHRC
		source(GLOBAL_ZSHRC);
#endif
		if (unset(PRIVILEGED))
		    sourcehome(".zshrc");
	    }
	    if (islogin) {
#ifdef GLOBAL_ZLOGIN
		source(GLOBAL_ZLOGIN);
#endif
		if (unset(PRIVILEGED))
		    sourcehome(".zlogin");
	    }
	}
    }
    noerrexit = 0;
    nohistsave = 0;
}

/* Miscellaneous initializations that happen after init scripts are run */

/**/
void
init_misc(void)
{
    if (cmd) {
	if (SHIN >= 10)
	    fclose(bshin);
	SHIN = movefd(open("/dev/null", O_RDONLY));
	bshin = fdopen(SHIN, "r");
	execstring(cmd, 0, 1);
	stopmsg = 1;
	zexit(lastval, 0);
    }

    if (interact && isset(RCS))
	readhistfile(getsparam("HISTFILE"), 0);
}

/* source a file */

/**/
int
source(char *s)
{
    int tempfd, fd, cj, oldlineno;
    int oldshst, osubsh, oloops;
    FILE *obshin;
    char *old_scriptname = scriptname;

    if (!s || (tempfd = movefd(open(unmeta(s), O_RDONLY))) == -1) {
	return 1;
    }

    /* save the current shell state */
    fd        = SHIN;            /* store the shell input fd                  */
    obshin    = bshin;          /* store file handle for buffered shell input */
    osubsh    = subsh;           /* store whether we are in a subshell        */
    cj        = thisjob;         /* store our current job number              */
    oldlineno = lineno;          /* store our current lineno                  */
    oloops    = loops;           /* stored the # of nested loops we are in    */
    oldshst   = opts[SHINSTDIN]; /* store current value of this option        */

    SHIN = tempfd;
    bshin = fdopen(SHIN, "r");
    subsh  = 0;
    lineno = 1;
    loops  = 0;
    dosetopt(SHINSTDIN, 0, 1);
    scriptname = s;

    sourcelevel++;
    loop(0, 0);			/* loop through the file to be sourced        */
    sourcelevel--;
    fclose(bshin);
    fdtable[SHIN] = 0;

    /* restore the current shell state */
    SHIN = fd;                       /* the shell input fd                   */
    bshin = obshin;                  /* file handle for buffered shell input */
    subsh = osubsh;                  /* whether we are in a subshell         */
    thisjob = cj;                    /* current job number                   */
    lineno = oldlineno;              /* our current lineno                   */
    loops = oloops;                  /* the # of nested loops we are in      */
    dosetopt(SHINSTDIN, oldshst, 1); /* SHINSTDIN option                     */
    errflag = 0;
    retflag = 0;
    scriptname = old_scriptname;

    return 0;
}

/* Try to source a file in the home directory */

/**/
void
sourcehome(char *s)
{
    char buf[PATH_MAX];
    char *h;

    if (emulation == EMULATE_SH || emulation == EMULATE_KSH ||
	!(h = getsparam("ZDOTDIR")))
	h = home;
    if (strlen(h) + strlen(s) + 1 >= PATH_MAX) {
	zerr("path too long: %s", s, 0);
	return;
    }
    sprintf(buf, "%s/%s", h, s);
    source(buf);
}

/**/
void
compctlsetup(void)
{
    static char
        *os[] =
    {"setopt", "unsetopt", NULL}, *vs[] =
    {"export", "typeset", "vared", "unset", NULL}, *cs[] =
    {"which", "builtin", NULL}, *bs[] =
    {"bindkey", NULL};

    compctl_process(os, CC_OPTIONS, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		    NULL, NULL, 0);
    compctl_process(vs, CC_VARS, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		    NULL, NULL, 0);
    compctl_process(bs, CC_BINDINGS, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		    NULL, NULL, 0);
    compctl_process(cs, CC_COMMPATH, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		    NULL, NULL, 0);
    cc_compos.mask = CC_COMMPATH;
    cc_default.refc = 10000;
    cc_default.mask = CC_FILES;
    cc_first.refc = 10000;
    cc_first.mask = 0;
}
