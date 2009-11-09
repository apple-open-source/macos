/*
 * fetchmail.c -- main driver module for fetchmail
 *
 * For license terms, see the file COPYING in this directory.
 */
#include "config.h"

#include <stdio.h>
#if defined(STDC_HEADERS)
#include <stdlib.h>
#endif
#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#if defined(HAVE_SYSLOG)
#include <syslog.h>
#endif
#include <pwd.h>
#ifdef __FreeBSD__
#include <grp.h>
#endif
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_SETRLIMIT
#include <sys/resource.h>
#endif /* HAVE_SETRLIMIT */

#ifdef HAVE_SOCKS
#include <socks.h> /* SOCKSinit() */
#endif /* HAVE_SOCKS */

#ifdef HAVE_LANGINFO_H
#include <langinfo.h>
#endif

#include "fetchmail.h"
#include "socket.h"
#include "tunable.h"
#include "smtp.h"
#include "netrc.h"
#include "i18n.h"
#include "lock.h"

/* need these (and sys/types.h) for res_init() */
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>

#ifndef ENETUNREACH
#define ENETUNREACH   128       /* Interactive doesn't know this */
#endif /* ENETUNREACH */

/* prototypes for internal functions */
static int load_params(int, char **, int);
static void dump_params (struct runctl *runp, struct query *, flag implicit);
static int query_host(struct query *);

/* controls the detail level of status/progress messages written to stderr */
int outlevel;    	    /* see the O_.* constants above */

/* miscellaneous global controls */
struct runctl run;	    /* global controls for this run */
flag nodetach;		    /* if TRUE, don't detach daemon process */
flag quitmode;		    /* if --quit was set */
int  quitind;		    /* optind after position of last --quit option */
flag check_only;	    /* if --probe was set */
flag versioninfo;	    /* emit only version info */
char *user;		    /* the name of the invoking user */
char *home;		    /* invoking user's home directory */
char *fmhome;		    /* fetchmail's home directory */
char *program_name;	    /* the name to prefix error messages with */
flag configdump;	    /* dump control blocks for configurator */
char *fetchmailhost;	    /* either `localhost' or the host's FQDN */

static int quitonly;	    /* if we should quit after killing the running daemon */

static int querystatus;		/* status of query */
static int successes;		/* count number of successful polls */
static int activecount;		/* count number of active entries */
static struct runctl cmd_run;	/* global options set from command line */
static time_t parsetime;	/* time of last parse */

static RETSIGTYPE terminate_run(int);
static RETSIGTYPE terminate_poll(int);

#if defined(__FreeBSD__) && defined(__FreeBSD_USE_KVM)
/* drop SGID kmem privileage until we need it */
static void dropprivs(void)
{
    struct group *gr;
    gid_t        egid;
    gid_t        rgid;
    
    egid = getegid();
    rgid = getgid();
    gr = getgrgid(egid);
    
    if (gr && !strcmp(gr->gr_name, "kmem"))
    {
    	extern void interface_set_gids(gid_t egid, gid_t rgid);
    	interface_set_gids(egid, rgid);
    	setegid(rgid);
    }
}
#endif

#if defined(HAVE_SETLOCALE) && defined(ENABLE_NLS) && defined(HAVE_STRFTIME)
#include <locale.h>
/** returns timestamp in current locale,
 * and resets LC_TIME locale to POSIX. */
static char *timestamp (void)
{
    time_t      now;
    static char buf[60]; /* RATS: ignore */

    time (&now);
    setlocale (LC_TIME, "");
    strftime (buf, sizeof (buf), "%c", localtime(&now));
    setlocale (LC_TIME, "C");
    return (buf);
}
#else
#define timestamp rfc822timestamp
#endif

static RETSIGTYPE donothing(int sig) 
{
    set_signal_handler(sig, donothing);
    lastsig = sig;
}

static void printcopyright(FILE *fp) {
	fprintf(fp, GT_("Copyright (C) 2002, 2003 Eric S. Raymond\n"
		   "Copyright (C) 2004 Matthias Andree, Eric S. Raymond, Robert M. Funk, Graham Wilson\n"
		   "Copyright (C) 2005 - 2006 Sunil Shetye\n"
		   "Copyright (C) 2005 - 2009 Matthias Andree\n"
		   ));
	fprintf(fp, GT_("Fetchmail comes with ABSOLUTELY NO WARRANTY. This is free software, and you\n"
		   "are welcome to redistribute it under certain conditions. For details,\n"
		   "please see the file COPYING in the source or documentation directory.\n"));
}

const char *iana_charset;

int main(int argc, char **argv)
{
    int bkgd = FALSE;
    int implicitmode = FALSE;
    struct query *ctl;
    netrc_entry *netrc_list;
    char *netrc_file, *tmpbuf;
    pid_t pid;
    int lastsig = 0;

#if defined(__FreeBSD__) && defined(__FreeBSD_USE_KVM)
    dropprivs();
#endif

    envquery(argc, argv);
#ifdef ENABLE_NLS
    setlocale (LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
    iana_charset = norm_charmap(nl_langinfo(CODESET)); /* normalize local
							  charset to
							  IANA charset. */
#else
    iana_charset = "US-ASCII";
#endif

    if (getuid() == 0) {
	report(stderr, GT_("WARNING: Running as root is discouraged.\n"));
    }

    /*
     * Note: because we can't initialize reporting before we  know whether
     * syslog is supposed to be on, this message will go to stdout and
     * be lost when running in background.
     */
    if (outlevel >= O_VERBOSE)
    {
	int i;

	report(stdout, GT_("fetchmail: invoked with"));
	for (i = 0; i < argc; i++)
	    report(stdout, " %s", argv[i]);
	report(stdout, "\n");
    }

#define IDFILE_NAME	".fetchids"
    run.idfile = prependdir (IDFILE_NAME, fmhome);
  
    outlevel = O_NORMAL;

    /*
     * We used to arrange for the lock to be removed on exit close
     * to where the lock was asserted.  Now we need to do it here, because
     * we might have re-executed in background with an existing lock
     * as the result of a changed rcfile (see the code near the execvp(3)
     * call near the beginning of the polling loop for details).  We want
     * to be sure the lock gets nuked on any error exit, basically.
     */
    fm_lock_dispose();

#ifdef HAVE_GETCWD
    /* save the current directory */
    if (getcwd (currentwd, sizeof (currentwd)) == NULL) {
	report(stderr, GT_("could not get current working directory\n"));
	currentwd[0] = 0;
    }
#endif

    {
	int i;

	i = parsecmdline(argc, argv, &cmd_run, &cmd_opts);
	if (i < 0)
	    exit(PS_SYNTAX);

	if (quitmode && quitind == argc)
	    quitonly = 1;
    }

    if (versioninfo)
    {
	const char *features = 
#ifdef POP2_ENABLE
	"+POP2"
#endif /* POP2_ENABLE */
#ifndef POP3_ENABLE
	"-POP3"
#endif /* POP3_ENABLE */
#ifndef IMAP_ENABLE
	"-IMAP"
#endif /* IMAP_ENABLE */
#ifdef GSSAPI
	"+GSS"
#endif /* GSSAPI */
#ifdef RPA_ENABLE
	"+RPA"
#endif /* RPA_ENABLE */
#ifdef NTLM_ENABLE
	"+NTLM"
#endif /* NTLM_ENABLE */
#ifdef SDPS_ENABLE
	"+SDPS"
#endif /* SDPS_ENABLE */
#ifndef ETRN_ENABLE
	"-ETRN"
#endif /* ETRN_ENABLE */
#ifndef ODMR_ENABLE
	"-ODMR"
#endif /* ODMR_ENABLE */
#ifdef SSL_ENABLE
	"+SSL"
#endif
#ifdef OPIE_ENABLE
	"+OPIE"
#endif /* OPIE_ENABLE */
#ifdef HAVE_PKG_hesiod
	"+HESIOD"
#endif
#ifdef HAVE_SOCKS
	"+SOCKS"
#endif /* HAVE_SOCKS */
#ifdef ENABLE_NLS
	"+NLS"
#endif /* ENABLE_NLS */
#ifdef KERBEROS_V4
	"+KRB4"
#endif /* KERBEROS_V4 */
#ifdef KERBEROS_V5
	"+KRB5"
#endif /* KERBEROS_V5 */
#ifndef HAVE_RES_SEARCH
	"-DNS"
#endif
	".\n";
	printf(GT_("This is fetchmail release %s"), VERSION);
	fputs(features, stdout);
	puts("");
	printcopyright(stdout);
	puts("");
	fputs("Fallback MDA: ", stdout);
#ifdef FALLBACK_MDA
	fputs(FALLBACK_MDA, stdout);
#else
	fputs("(none)", stdout);
#endif
	putchar('\n');
	fflush(stdout);

	/* this is an attempt to help remote debugging */
	if (system("uname -a")) { /* NOOP to quench GCC complaint */ }
    }

    /* avoid parsing the config file if all we're doing is killing a daemon */
    if (!quitonly)
	implicitmode = load_params(argc, argv, optind);

    /* precedence: logfile (if effective) overrides syslog. */
    if (run.logfile && run.poll_interval && !nodetach) {
	run.use_syslog = 0;
    }

#if defined(HAVE_SYSLOG)
    /* logging should be set up early in case we were restarted from exec */
    if (run.use_syslog)
    {
#if defined(LOG_MAIL)
	openlog(program_name, LOG_PID, LOG_MAIL);
#else
	/* Assume BSD4.2 openlog with two arguments */
	openlog(program_name, LOG_PID);
#endif
	report_init(-1);
    }
    else
#endif
	report_init((run.poll_interval == 0 || nodetach) && !run.logfile);

#ifdef POP3_ENABLE
    /* initialize UID handling */
    {
	int st;

	if (!versioninfo && (st = prc_filecheck(run.idfile, !versioninfo)) != 0)
	    exit(st);
	else
	    initialize_saved_lists(querylist, run.idfile);
    }
#endif /* POP3_ENABLE */

    /* construct the lockfile */
    fm_lock_setup(&run);

#ifdef HAVE_SETRLIMIT
    /*
     * Before getting passwords, disable core dumps unless -v -d0 mode is on.
     * Core dumps could otherwise contain passwords to be scavenged by a
     * cracker.
     */
    if (outlevel < O_VERBOSE || run.poll_interval > 0)
    {
	struct rlimit corelimit;
	corelimit.rlim_cur = 0;
	corelimit.rlim_max = 0;
	setrlimit(RLIMIT_CORE, &corelimit);
    }
#endif /* HAVE_SETRLIMIT */

#define	NETRC_FILE	".netrc"
    /* parse the ~/.netrc file (if present) for future password lookups. */
    netrc_file = prependdir (NETRC_FILE, home);
    netrc_list = parse_netrc(netrc_file);
    free(netrc_file);
#undef NETRC_FILE

    /* pick up passwords where we can */ 
    for (ctl = querylist; ctl; ctl = ctl->next)
    {
	if (ctl->active && !(implicitmode && ctl->server.skip)&&!ctl->password)
	{
	    if (NO_PASSWORD(ctl))
		/* Server won't care what the password is, but there
		   must be some non-null string here.  */
		ctl->password = ctl->remotename;
	    else
	    {
		netrc_entry *p;

		/* look up the pollname and account in the .netrc file. */
		p = search_netrc(netrc_list,
				 ctl->server.pollname, ctl->remotename);
		/* if we find a matching entry with a password, use it */
		if (p && p->password)
		    ctl->password = xstrdup(p->password);

		/* otherwise try with "via" name if there is one */
		else if (ctl->server.via)
		{
		    p = search_netrc(netrc_list, 
				     ctl->server.via, ctl->remotename);
		    if (p && p->password)
		        ctl->password = xstrdup(p->password);
		}
	    }
	}
    }

    free_netrc(netrc_list);
    netrc_list = 0;

    /* perhaps we just want to check options? */
    if (versioninfo)
    {
	int havercfile = access(rcfile, 0);

	printf(GT_("Taking options from command line%s%s\n"),
				havercfile ? "" :  GT_(" and "),
				havercfile ? "" : rcfile);

	if (querylist == NULL)
	    fprintf(stderr,
		    GT_("No mailservers set up -- perhaps %s is missing?\n"),
		    rcfile);
	else
	    dump_params(&run, querylist, implicitmode);
	exit(0);
    }

    /* dump options as a Python dictionary, for configurator use */
    if (configdump)
    {
	dump_config(&run, querylist);
	exit(0);
    }

    /* check for another fetchmail running concurrently */
    pid = fm_lock_state();
    bkgd = (pid < 0);
    pid = bkgd ? -pid : pid;

    /* if no mail servers listed and nothing in background, we're done */
    if (!quitonly && pid == 0 && querylist == NULL) {
	(void)fputs(GT_("fetchmail: no mailservers have been specified.\n"),stderr);
	exit(PS_SYNTAX);
    }

    /* perhaps user asked us to kill the other fetchmail */
    if (quitmode)
    {
	if (pid == 0 || pid == getpid())
	    /* this test enables re-execing on a changed rcfile
	     * for pid == getpid() */
	{
	    if (quitonly) {
		fprintf(stderr,GT_("fetchmail: no other fetchmail is running\n"));
		exit(PS_EXCLUDE);
	    }
	}
	else if (kill(pid, SIGTERM) < 0)
	{
	    fprintf(stderr,GT_("fetchmail: error killing %s fetchmail at %d; bailing out.\n"),
		    bkgd ? GT_("background") : GT_("foreground"), pid);
	    exit(PS_EXCLUDE);
	}
	else
	{
	    int maxwait;

	    if (outlevel > O_SILENT)
		fprintf(stderr,GT_("fetchmail: %s fetchmail at %d killed.\n"),
			bkgd ? GT_("background") : GT_("foreground"), pid);
	    /* We used to nuke the other process's lock here, with
	     * fm_lock_release(), which is broken. The other process
	     * needs to clear its lock by itself. */
	    if (quitonly)
		exit(0);

	    /* wait for other process to exit */
	    maxwait = 10; /* seconds */
	    while (kill(pid, 0) == 0 && --maxwait >= 0) {
		sleep(1);
	    }
	    pid = 0;
	}
    }

    /* another fetchmail is running -- wake it up or die */
    if (pid != 0)
    {
	if (check_only)
	{
	    fprintf(stderr,
		 GT_("fetchmail: can't check mail while another fetchmail to same host is running.\n"));
	    return(PS_EXCLUDE);
        }
	else if (!implicitmode)
	{
	    fprintf(stderr,
		 GT_("fetchmail: can't poll specified hosts with another fetchmail running at %d.\n"),
		 pid);
		return(PS_EXCLUDE);
	}
	else if (!bkgd)
	{
	    fprintf(stderr,
		 GT_("fetchmail: another foreground fetchmail is running at %d.\n"),
		 pid);
		return(PS_EXCLUDE);
	}
	else if (getpid() == pid)
	    /* this test enables re-execing on a changed rcfile */
	    fm_lock_assert();
	else if (argc > 1)
	{
	    fprintf(stderr,
		    GT_("fetchmail: can't accept options while a background fetchmail is running.\n"));
	    return(PS_EXCLUDE);
	}
	else if (kill(pid, SIGUSR1) == 0)
	{
	    fprintf(stderr,
		    GT_("fetchmail: background fetchmail at %d awakened.\n"),
		    pid);
	    return(0);
	}
	else
	{
	    /*
	     * Should never happen -- possible only if a background fetchmail
	     * croaks after the first kill probe above but before the
	     * SIGUSR1/SIGHUP transmission.
	     */
	    fprintf(stderr,
		    GT_("fetchmail: elder sibling at %d died mysteriously.\n"),
		    pid);
	    return(PS_UNDEFINED);
	}
    }

    /* pick up interactively any passwords we need but don't have */ 
    for (ctl = querylist; ctl; ctl = ctl->next)
    {
	if (ctl->active && !(implicitmode && ctl->server.skip)
		&& !NO_PASSWORD(ctl) && !ctl->password)
	{
	    if (!isatty(0))
	    {
		fprintf(stderr,
			GT_("fetchmail: can't find a password for %s@%s.\n"),
			ctl->remotename, ctl->server.pollname);
		return(PS_AUTHFAIL);
	    } else {
		const char* password_prompt = GT_("Enter password for %s@%s: ");
		size_t pplen = strlen(password_prompt) + strlen(ctl->remotename) + strlen(ctl->server.pollname) + 1;

		tmpbuf = (char *)xmalloc(pplen);
		snprintf(tmpbuf, pplen, password_prompt,
			ctl->remotename, ctl->server.pollname);
		ctl->password = xstrdup((char *)fm_getpassword(tmpbuf));
		free(tmpbuf);
	    }
	}
    }

    /*
     * Time to initiate the SOCKS library (this is not mandatory: it just
     * registers the correct application name for logging purpose. If you
     * have some problem, comment out these lines).
     */
#ifdef HAVE_SOCKS
    SOCKSinit("fetchmail");
#endif /* HAVE_SOCKS */

    /* avoid zombies from plugins */
    deal_with_sigchld();

    /* Fix up log destination - if the if() is true, the precedence rule
     * above hasn't killed off the syslog option, because the logfile
     * option is ineffective (because we're not detached or not in
     * deamon mode), so kill it for the benefit of other parts of the
     * code. */
    if (run.logfile && run.use_syslog)
	run.logfile = 0;

    /*
     * Maybe time to go to demon mode...
     */
    if (run.poll_interval)
    {
	if (!nodetach) {
	    int rc;

	    rc = daemonize(run.logfile);
	    if (rc) {
		report(stderr, GT_("fetchmail: Cannot detach into background. Aborting.\n"));
		exit(rc);
	    }
	}
	report(stdout, GT_("starting fetchmail %s daemon \n"), VERSION);

	/*
	 * We'll set up a handler for these when we're sleeping,
	 * but ignore them otherwise so as not to interrupt a poll.
	 */
	set_signal_handler(SIGUSR1, SIG_IGN);
	if (run.poll_interval && getuid() == ROOT_UID)
	    set_signal_handler(SIGHUP, SIG_IGN);
    }
    else
    {
	/* not in daemon mode */
	if (run.logfile && !nodetach && access(run.logfile, F_OK) == 0)
    	{
	    if (!freopen(run.logfile, "a", stdout))
		    report(stderr, GT_("could not open %s to append logs to \n"), run.logfile);
	    if (!freopen(run.logfile, "a", stderr))
		    report(stdout, GT_("could not open %s to append logs to \n"), run.logfile);
	    if (run.use_syslog)
		report(stdout, GT_("fetchmail: Warning: syslog and logfile are set. Check both for logs!\n"));
    	}
    }

    interface_init();

    /* beyond here we don't want more than one fetchmail running per user */
    umask(0077);
    set_signal_handler(SIGABRT, terminate_run);
    set_signal_handler(SIGINT, terminate_run);
    set_signal_handler(SIGTERM, terminate_run);
    set_signal_handler(SIGALRM, terminate_run);
    set_signal_handler(SIGPIPE, SIG_IGN);
    set_signal_handler(SIGQUIT, terminate_run);

    /* here's the exclusion lock */
    fm_lock_or_die();

    if (check_only && outlevel >= O_VERBOSE) {
	report(stdout, GT_("--check mode enabled, not fetching mail\n"));
    }

    /*
     * Query all hosts. If there's only one, the error return will
     * reflect the status of that transaction.
     */
    do {
	/* 
	 * Check to see if the rcfile has been touched.  If so,
	 * re-exec so the file will be reread.  Doing it this way
	 * avoids all the complications of trying to deallocate the
	 * in-core control structures -- and the potential memory
	 * leaks...
	 */
	struct stat	rcstat;

	if (strcmp(rcfile, "-") == 0) {
	    /* do nothing */
	} else if (stat(rcfile, &rcstat) == -1) {
	    if (errno != ENOENT)
		report(stderr, 
		       GT_("couldn't time-check %s (error %d)\n"),
		       rcfile, errno);
	}
	else if (rcstat.st_mtime > parsetime)
	{
	    report(stdout, GT_("restarting fetchmail (%s changed)\n"), rcfile);

#ifdef HAVE_GETCWD
	    /* restore the startup directory */
	    if (!currentwd[0] || chdir (currentwd) == -1)
		report(stderr, GT_("attempt to re-exec may fail as directory has not been restored\n"));
#endif

	    /*
	     * Matthias Andree: Isn't this prone to introduction of
	     * "false" programs by interfering with PATH? Those
	     * path-searching execs might not be the best ideas for
	     * this reason.
	     *
	     * Rob Funk: But is there any way for someone to modify
	     * the PATH variable of a running fetchmail?  I don't know
	     * of a way.
	     *
	     * Dave's change makes fetchmail restart itself in exactly
	     * the way it was started from the shell (or shell script)
	     * in the first place.  If you're concerned about PATH
	     * contamination, call fetchmail initially with a full
	     * path, and use Dave's patch.
	     *
	     * Not using a -p variant of exec means that the restart
	     * will break if both (a) the user depended on PATH to
	     * call fetchmail in the first place, and (b) the system
	     * doesn't save the whole path in argv[0] if the whole
	     * path wasn't used in the initial call.  (If I recall
	     * correctly, Linux saves it but many other Unices don't.)
	     */
	    execvp(argv[0], argv);
	    report(stderr, GT_("attempt to re-exec fetchmail failed\n"));
	}

#ifdef HAVE_RES_SEARCH
	/* Boldly assume that we also have res_init() if we have
	 * res_search(), and call res_init() to re-read the resolv.conf
	 * file, so that we can pick up changes to that file that are
	 * written by dhpccd, dhclient, pppd, openvpn and similar. */

	/* NOTE: This assumes that /etc/resolv.conf is written
	 * atomically (i. e. a temporary file is written, flushed and
	 * then renamed into place). To fix Debian Bug#389270. */

	/* NOTE: If this leaks memory or doesn't re-read
	 * /etc/resolv.conf, we're in trouble. The res_init() interface
	 * is only lightly documented :-( */
	res_init();
#endif

	activecount = 0;
	batchcount = 0;
	for (ctl = querylist; ctl; ctl = ctl->next)
	    if (ctl->active)
	    {
		activecount++;
		if (!(implicitmode && ctl->server.skip))
		{
		    if (ctl->wedged)
		    {
			report(stderr, 
			       GT_("poll of %s skipped (failed authentication or too many timeouts)\n"),
			       ctl->server.pollname);
			continue;
		    }

		    /* check skip interval first so that it counts all polls */
		    if (run.poll_interval && ctl->server.interval) 
		    {
			if (ctl->server.poll_count++ % ctl->server.interval) 
			{
			    if (outlevel >= O_VERBOSE)
				report(stdout,
				       GT_("interval not reached, not querying %s\n"),
				       ctl->server.pollname);
			    continue;
			}
		    }

#ifdef CAN_MONITOR
		    /*
		     * Don't do monitoring if we were woken by a signal.
		     * Note that interface_approve() does its own error logging.
		     */
		    if (!interface_approve(&ctl->server, !lastsig))
			continue;
#endif /* CAN_MONITOR */

		    dofastuidl = 0; /* this is reset in the driver if required */

		    querystatus = query_host(ctl);

		    if (NUM_NONZERO(ctl->fastuidl))
			ctl->fastuidlcount = (ctl->fastuidlcount + 1) % ctl->fastuidl;
#ifdef POP3_ENABLE
		    /* leave the UIDL state alone if there have been any errors */
		    if (!check_only &&
				((querystatus==PS_SUCCESS) || (querystatus==PS_NOMAIL) || (querystatus==PS_MAXFETCH)))
			uid_swap_lists(ctl);
		    else
			uid_discard_new_list(ctl);
		    uid_reset_num(ctl);
#endif  /* POP3_ENABLE */

		    if (querystatus == PS_SUCCESS)
			successes++;
		    else if (!check_only && 
			     ((querystatus!=PS_NOMAIL) || (outlevel==O_DEBUG)))
			switch(querystatus)
			{
			case PS_SUCCESS:
			    report(stdout,GT_("Query status=0 (SUCCESS)\n"));break;
			case PS_NOMAIL: 
			    report(stdout,GT_("Query status=1 (NOMAIL)\n")); break;
			case PS_SOCKET:
			    report(stdout,GT_("Query status=2 (SOCKET)\n")); break;
			case PS_AUTHFAIL:
			    report(stdout,GT_("Query status=3 (AUTHFAIL)\n"));break;
			case PS_PROTOCOL:
			    report(stdout,GT_("Query status=4 (PROTOCOL)\n"));break;
			case PS_SYNTAX:
			    report(stdout,GT_("Query status=5 (SYNTAX)\n")); break;
			case PS_IOERR:
			    report(stdout,GT_("Query status=6 (IOERR)\n"));  break;
			case PS_ERROR:
			    report(stdout,GT_("Query status=7 (ERROR)\n"));  break;
			case PS_EXCLUDE:
			    report(stdout,GT_("Query status=8 (EXCLUDE)\n")); break;
			case PS_LOCKBUSY:
			    report(stdout,GT_("Query status=9 (LOCKBUSY)\n"));break;
			case PS_SMTP:
			    report(stdout,GT_("Query status=10 (SMTP)\n")); break;
			case PS_DNS:
			    report(stdout,GT_("Query status=11 (DNS)\n")); break;
			case PS_BSMTP:
			    report(stdout,GT_("Query status=12 (BSMTP)\n")); break;
			case PS_MAXFETCH:
			    report(stdout,GT_("Query status=13 (MAXFETCH)\n"));break;
			default:
			    report(stdout,GT_("Query status=%d\n"),querystatus);
			    break;
			}

#ifdef CAN_MONITOR
		    if (ctl->server.monitor)
		    {
			/*
			 * Allow some time for the link to quiesce.  One
			 * second is usually sufficient, three is safe.
			 * Note:  this delay is important - don't remove!
			 */
			sleep(3);
			interface_note_activity(&ctl->server);
		    }
#endif /* CAN_MONITOR */
		}
	    }

	/* close connections cleanly */
	terminate_poll(0);

	/*
	 * OK, we've polled.  Now sleep.
	 */
	if (run.poll_interval)
	{
	    /* 
	     * Because passwords can expire, it may happen that *all*
	     * hosts are now out of the loop due to authfail
	     * conditions.  If this happens daemon-mode fetchmail
	     * should softly and silently vanish away, rather than
	     * spinning uselessly.
	     */
	    int unwedged = 0;

	    for (ctl = querylist; ctl; ctl = ctl->next)
		if (ctl->active && !(implicitmode && ctl->server.skip))
		    if (!ctl->wedged)
			unwedged++;
	    if (!unwedged)
	    {
		report(stderr, GT_("All connections are wedged.  Exiting.\n"));
		/* FIXME: someday, send notification mail */
		exit(PS_AUTHFAIL);
	    }

	    if ((outlevel > O_SILENT && !run.use_syslog && isatty(1))
		    || outlevel > O_NORMAL)
		report(stdout,
		       GT_("sleeping at %s for %d seconds\n"), timestamp(), run.poll_interval);

	    /*
	     * With this simple hack, we make it possible for a foreground 
	     * fetchmail to wake up one in daemon mode.  What we want is the
	     * side effect of interrupting any sleep that may be going on,
	     * forcing fetchmail to re-poll its hosts.  The second line is
	     * for people who think all system daemons wake up on SIGHUP.
	     */
	    set_signal_handler(SIGUSR1, donothing);
	    if (getuid() == ROOT_UID)
		set_signal_handler(SIGHUP, donothing);

	    /*
	     * OK, now pause until it's time for the next poll cycle.
	     * A nonzero return indicates we received a wakeup signal;
	     * unwedge all servers in case the problem has been
	     * manually repaired.
	     */
	    if ((lastsig = interruptible_idle(run.poll_interval)))
	    {
		if (outlevel > O_SILENT)
#ifdef SYS_SIGLIST_DECLARED
		    report(stdout, 
		       GT_("awakened by %s\n"), sys_siglist[lastsig]);
#else
	    	    report(stdout, 
		       GT_("awakened by signal %d\n"), lastsig);
#endif
		for (ctl = querylist; ctl; ctl = ctl->next)
		    ctl->wedged = FALSE;
	    }

	    if ((outlevel > O_SILENT && !run.use_syslog && isatty(1))
		    || outlevel > O_NORMAL)
		report(stdout, GT_("awakened at %s\n"), timestamp());
	}
    } while (run.poll_interval);

    if (outlevel >= O_VERBOSE)
	report(stdout, GT_("normal termination, status %d\n"),
		successes ? PS_SUCCESS : querystatus);

    terminate_run(0);

    if (successes)
	exit(PS_SUCCESS);
    else if (querystatus)
	exit(querystatus);
    else
	/* in case we interrupted before a successful fetch */
	exit(PS_NOMAIL);
}

static void list_merge(struct idlist **dstl, struct idlist **srcl, int force)
{
    /*
     * If force is off, modify dstl fields only when they're empty (treat srcl
     * as defaults).  If force is on, modify each dstl field whenever scrcl
     * is nonempty (treat srcl as an override).  
     */
    if (force ? !!*srcl : !*dstl)
    {
	struct idlist *cpl = copy_str_list(*srcl);

	append_str_list(dstl, &cpl);
    }
}

static void optmerge(struct query *h2, struct query *h1, int force)
/* merge two options records */
{
    list_merge(&h2->server.localdomains, &h1->server.localdomains, force);
    list_merge(&h2->localnames, &h1->localnames, force);
    list_merge(&h2->mailboxes, &h1->mailboxes, force);
    list_merge(&h2->smtphunt, &h1->smtphunt, force);
    list_merge(&h2->domainlist, &h1->domainlist, force);
    list_merge(&h2->antispam, &h1->antispam, force);

#define FLAG_MERGE(fld) if (force ? !!h1->fld : !h2->fld) h2->fld = h1->fld
    FLAG_MERGE(server.via);
    FLAG_MERGE(server.protocol);
    FLAG_MERGE(server.service);
    FLAG_MERGE(server.interval);
    FLAG_MERGE(server.authenticate);
    FLAG_MERGE(server.timeout);
    FLAG_MERGE(server.envelope);
    FLAG_MERGE(server.envskip);
    FLAG_MERGE(server.qvirtual);
    FLAG_MERGE(server.skip);
    FLAG_MERGE(server.dns);
    FLAG_MERGE(server.checkalias);
    FLAG_MERGE(server.uidl);
    FLAG_MERGE(server.principal);

#ifdef CAN_MONITOR
    FLAG_MERGE(server.interface);
    FLAG_MERGE(server.interface_pair);
    FLAG_MERGE(server.monitor);
#endif

    FLAG_MERGE(server.plugin);
    FLAG_MERGE(server.plugout);
    FLAG_MERGE(server.tracepolls);

    FLAG_MERGE(wildcard);
    FLAG_MERGE(remotename);
    FLAG_MERGE(password);
    FLAG_MERGE(mda);
    FLAG_MERGE(bsmtp);
    FLAG_MERGE(listener);
    FLAG_MERGE(smtpaddress);
    FLAG_MERGE(smtpname);
    FLAG_MERGE(preconnect);
    FLAG_MERGE(postconnect);

    FLAG_MERGE(keep);
    FLAG_MERGE(flush);
    FLAG_MERGE(limitflush);
    FLAG_MERGE(fetchall);
    FLAG_MERGE(rewrite);
    FLAG_MERGE(forcecr);
    FLAG_MERGE(stripcr);
    FLAG_MERGE(pass8bits);
    FLAG_MERGE(dropstatus);
    FLAG_MERGE(dropdelivered);
    FLAG_MERGE(mimedecode);
    FLAG_MERGE(idle);
    FLAG_MERGE(limit);
    FLAG_MERGE(warnings);
    FLAG_MERGE(fetchlimit);
    FLAG_MERGE(fetchsizelimit);
    FLAG_MERGE(fastuidl);
    FLAG_MERGE(batchlimit);
#ifdef	SSL_ENABLE
    FLAG_MERGE(use_ssl);
    FLAG_MERGE(sslkey);
    FLAG_MERGE(sslcert);
    FLAG_MERGE(sslproto);
    FLAG_MERGE(sslcertck);
    FLAG_MERGE(sslcertpath);
    FLAG_MERGE(sslcommonname);
    FLAG_MERGE(sslfingerprint);
#endif
    FLAG_MERGE(expunge);

    FLAG_MERGE(properties);
#undef FLAG_MERGE
}

/** Load configuration files.
 * \return - true if no servers found on the command line
 *         - false if servers found on the command line */
static int load_params(int argc, char **argv, int optind)
{
    int	implicitmode, st;
    struct passwd *pw;
    struct query def_opts, *ctl;
    struct stat rcstat;
    char *p;

    run.bouncemail = TRUE;
    run.softbounce = TRUE;	/* treat permanent errors as temporary */
    run.spambounce = FALSE;	/* don't bounce back to innocent bystanders */

    memset(&def_opts, '\0', sizeof(struct query));
    def_opts.smtp_socket = -1;
    def_opts.smtpaddress = (char *)0;
    def_opts.smtpname = (char *)0;
    def_opts.server.protocol = P_AUTO;
    def_opts.server.timeout = CLIENT_TIMEOUT;
    def_opts.server.esmtp_name = user;
    def_opts.warnings = WARNING_INTERVAL;
    def_opts.remotename = user;
    def_opts.listener = SMTP_MODE;
    def_opts.fetchsizelimit = 100;
    def_opts.fastuidl = 4;

    /* get the location of rcfile */
    rcfiledir[0] = 0;
    p = strrchr (rcfile, '/');
    if (p && (size_t)(p - rcfile) < sizeof (rcfiledir)) {
	*p = 0;			/* replace '/' by '0' */
	strlcpy (rcfiledir, rcfile, sizeof(rcfiledir));
	*p = '/';		/* restore '/' */
	if (!rcfiledir[0])	/* "/.fetchmailrc" case */
	    strcpy (rcfiledir, "/");
    }

    /* note the parse time, so we can pick up on modifications */
    parsetime = 0;	/* foil compiler warnings */
    if (strcmp(rcfile, "-") == 0 || stat(rcfile, &rcstat) != -1)
	parsetime = rcstat.st_mtime;
    else if (errno != ENOENT)
	report(stderr, GT_("couldn't time-check the run-control file\n"));

    /* this builds the host list */
    if ((st = prc_parse_file(rcfile, !versioninfo)) != 0)
	/*
	 * FIXME: someday, send notification mail here if backgrounded.
	 * Right now, that can happen if the user changes the rcfile
	 * while the fetchmail is running in background.  Do similarly
	 * for the other exit() calls in this function.
	 */
	exit(st);

    if ((implicitmode = (optind >= argc)))
    {
	for (ctl = querylist; ctl; ctl = ctl->next)
	    ctl->active = !ctl->server.skip;
    }
    else
	for (; optind < argc; optind++) 
	{
	    flag	predeclared =  FALSE;

	    /*
	     * If hostname corresponds to a host known from the rc file,
	     * simply declare it active.  Otherwise synthesize a host
	     * record from command line and defaults
	     */
	    for (ctl = querylist; ctl; ctl = ctl->next)
		if (!strcmp(ctl->server.pollname, argv[optind])
			|| str_in_list(&ctl->server.akalist, argv[optind], TRUE))
		{
		    /* Is this correct? */
		    if (predeclared && outlevel >= O_VERBOSE)
			fprintf(stderr,GT_("Warning: multiple mentions of host %s in config file\n"),argv[optind]);
		    ctl->active = TRUE;
		    predeclared = TRUE;
		}

	    if (!predeclared)
	    {
		/*
		 * Allocate and link record without copying in
		 * command-line args; we'll do that with the optmerge
		 * call later on.
		 */
		ctl = hostalloc((struct query *)NULL);
		ctl->server.via =
		    ctl->server.pollname = xstrdup(argv[optind]);
		ctl->active = TRUE;
		ctl->server.lead_server = (struct hostdata *)NULL;
	    }
	}

    /*
     * If there's a defaults record, merge it and lose it.
     */ 
    if (querylist && strcmp(querylist->server.pollname, "defaults") == 0)
    {
	for (ctl = querylist->next; ctl; ctl = ctl->next)
	    optmerge(ctl, querylist, FALSE);
	querylist = querylist->next;
    }

    /* don't allow a defaults record after the first */
    for (ctl = querylist; ctl; ctl = ctl->next) {
	if (ctl != querylist && strcmp(ctl->server.pollname, "defaults") == 0) {
	    fprintf(stderr, GT_("fetchmail: Error: multiple \"defaults\" records in config file.\n"));
	    exit(PS_SYNTAX);
	}
    }

    /* use localhost if we never fetch the FQDN of this host */
    fetchmailhost = "localhost";

    /* here's where we override globals */
    if (cmd_run.logfile)
	run.logfile = cmd_run.logfile;
    if (cmd_run.idfile)
	run.idfile = cmd_run.idfile;
    if (cmd_run.pidfile)
	run.pidfile = cmd_run.pidfile;
    /* do this before the keep/fetchall test below, otherwise -d0 may fail */
    if (cmd_run.poll_interval >= 0)
	run.poll_interval = cmd_run.poll_interval;
    if (cmd_run.invisible)
	run.invisible = cmd_run.invisible;
    if (cmd_run.showdots)
	run.showdots = cmd_run.showdots;
    if (cmd_run.use_syslog)
	run.use_syslog = (cmd_run.use_syslog == FLAG_TRUE);
    if (cmd_run.postmaster)
	run.postmaster = cmd_run.postmaster;
    if (cmd_run.bouncemail)
	run.bouncemail = cmd_run.bouncemail;
    if (cmd_run.softbounce)
	run.softbounce = cmd_run.softbounce;

    /* check and daemon options are not compatible */
    if (check_only && run.poll_interval)
	run.poll_interval = 0;

    /*
     * DNS support is required for some protocols.  We used to
     * do this unconditionally, but it made fetchmail excessively
     * vulnerable to misconfigured DNS setups.
     *
     * If we're using ETRN or ODMR, the smtp hunt list is the
     * list of systems we're polling on behalf of; these have
     * to be fully-qualified domain names.  The default for
     * this list should be the FQDN of localhost.
     *
     * If we're using Kerberos for authentication, we need 
     * the FQDN in order to generate capability keys.
     */
    for (ctl = querylist; ctl; ctl = ctl->next)
	if (ctl->active && 
		(ctl->server.protocol==P_ETRN || ctl->server.protocol==P_ODMR
		 || ctl->server.authenticate == A_KERBEROS_V4
		 || ctl->server.authenticate == A_KERBEROS_V5))
	{
	    fetchmailhost = host_fqdn(1);
	    break;
	}

    if (!ctl) /* list exhausted */
	fetchmailhost = host_fqdn(0);

    /* this code enables flags to be turned off */
#define DEFAULT(flag, dflt)	if (flag == FLAG_TRUE)\
	    				flag = TRUE;\
				else if (flag == FLAG_FALSE)\
					flag = FALSE;\
				else\
					flag = (dflt)

    /* merge in wired defaults, do sanity checks and prepare internal fields */
    for (ctl = querylist; ctl; ctl = ctl->next)
    {
	ctl->wedged = FALSE;

	/* merge in defaults */
	optmerge(ctl, &def_opts, FALSE);

	/* force command-line options */
	optmerge(ctl, &cmd_opts, TRUE);

	/*
	 * queryname has to be set up for inactive servers too.  
	 * Otherwise the UIDL code core-dumps on startup.
	 */
	if (ctl->server.via) 
	    ctl->server.queryname = xstrdup(ctl->server.via);
	else
	    ctl->server.queryname = xstrdup(ctl->server.pollname);

	/*
	 * We no longer do DNS lookups at startup.
	 * This is a kluge.  It enables users to edit their
	 * configurations when DNS isn't available.
	 */
	ctl->server.truename = xstrdup(ctl->server.queryname);

	if (configdump || ctl->active )
	{
	    DEFAULT(ctl->keep, FALSE);
	    DEFAULT(ctl->fetchall, FALSE);
	    DEFAULT(ctl->flush, FALSE);
	    DEFAULT(ctl->limitflush, FALSE);
	    DEFAULT(ctl->rewrite, TRUE);
	    DEFAULT(ctl->stripcr, (ctl->mda != (char *)NULL)); 
	    DEFAULT(ctl->forcecr, FALSE);
	    DEFAULT(ctl->pass8bits, FALSE);
	    DEFAULT(ctl->dropstatus, FALSE);
	    DEFAULT(ctl->dropdelivered, FALSE);
	    DEFAULT(ctl->mimedecode, FALSE);
	    DEFAULT(ctl->idle, FALSE);
	    DEFAULT(ctl->server.dns, TRUE);
	    DEFAULT(ctl->server.uidl, FALSE);
	    DEFAULT(ctl->use_ssl, FALSE);
	    DEFAULT(ctl->sslcertck, FALSE);
	    DEFAULT(ctl->server.checkalias, FALSE);
#ifndef SSL_ENABLE
	    /*
	     * XXX FIXME: do we need this check or can we rely on the .y
	     * parser handling this?
	     */
	    if (ctl->use_ssl) 
	    {
		report(stderr, GT_("SSL support is not compiled in.\n"));
		exit(PS_SYNTAX);
	    }
#endif /* SSL_ENABLE */
#undef DEFAULT
#ifndef KERBEROS_V4
	    if (ctl->server.authenticate == A_KERBEROS_V4) {
		report(stderr, GT_("KERBEROS v4 support is configured, but not compiled in.\n"));
		exit(PS_SYNTAX);
	    }
#endif
#ifndef KERBEROS_V5
	    if (ctl->server.authenticate == A_KERBEROS_V5) {
		report(stderr, GT_("KERBEROS v5 support is configured, but not compiled in.\n"));
		exit(PS_SYNTAX);
	    }
#endif
#ifndef GSSAPI
	    if (ctl->server.authenticate == A_GSSAPI) {
		report(stderr, GT_("GSSAPI support is configured, but not compiled in.\n"));
		exit(PS_SYNTAX);
	    }
#endif

	    /*
	     * Make sure we have a nonempty host list to forward to.
	     */
	    if (!ctl->smtphunt)
		save_str(&ctl->smtphunt, "localhost", FALSE);

	    /*
	     * Make sure we have a nonempty list of domains to fetch from.
	     */
	    if ((ctl->server.protocol==P_ETRN || ctl->server.protocol==P_ODMR) && !ctl->domainlist)
		save_str(&ctl->domainlist, fetchmailhost, FALSE);

	    /* if `user' doesn't name a real local user, try to run as root */
	    if ((pw = getpwnam(user)) == (struct passwd *)NULL)
		ctl->uid = 0;
            else
		ctl->uid = pw->pw_uid;	/* for local delivery via MDA */
	    if (!ctl->localnames)	/* for local delivery via SMTP */
		save_str_pair(&ctl->localnames, user, NULL);

#ifndef HAVE_RES_SEARCH
	    /* can't handle multidrop mailboxes unless we can do DNS lookups */
	    if (MULTIDROP(ctl) && ctl->server.dns)
	    {
		ctl->server.dns = FALSE;
		report(stderr, GT_("fetchmail: warning: no DNS available to check multidrop fetches from %s\n"), ctl->server.pollname);
	    }
#endif /* !HAVE_RES_SEARCH */

	    /*
	     * can't handle multidrop mailboxes without "envelope"
	     * option, this causes truckloads full of support complaints
	     * "all mail forwarded to postmaster"
	     */
	    if (MULTIDROP(ctl) && !ctl->server.envelope)
	    {
		report(stderr, GT_("warning: multidrop for %s requires envelope option!\n"), ctl->server.pollname);
		report(stderr, GT_("warning: Do not ask for support if all mail goes to postmaster!\n"));
	    }

	    /* if no folders were specified, set up the null one as default */
	    if (!ctl->mailboxes)
		save_str(&ctl->mailboxes, (char *)NULL, 0);

	    /* maybe user overrode timeout on command line? */
	    if (ctl->server.timeout == -1)
		ctl->server.timeout = CLIENT_TIMEOUT;

	    /* sanity checks */
	    if (ctl->server.service) {
		int port = servport(ctl->server.service);
		if (port < 0)
		{
		    (void) fprintf(stderr,
				   GT_("fetchmail: %s configuration invalid, specify positive port number for service or port\n"),
				   ctl->server.pollname);
		    exit(PS_SYNTAX);
		}
		if (ctl->server.protocol == P_RPOP && port >= 1024)
		{
		    (void) fprintf(stderr,
				   GT_("fetchmail: %s configuration invalid, RPOP requires a privileged port\n"),
				   ctl->server.pollname);
		    exit(PS_SYNTAX);
		}
	    }
	    if (ctl->listener == LMTP_MODE)
	    {
		struct idlist	*idp;

		for (idp = ctl->smtphunt; idp; idp = idp->next)
		{
		    char	*cp;

		    if (!(cp = strrchr(idp->id, '/'))
			|| (0 == strcmp(cp + 1, SMTP_PORT))
			|| servport(cp + 1) == SMTP_PORT_NUM)
		    {
			(void) fprintf(stderr,
				       GT_("%s configuration invalid, LMTP can't use default SMTP port\n"),
				       ctl->server.pollname);
			exit(PS_SYNTAX);
		    }
		}
	    }

	    /*
	     * "I beg to you, have mercy on the we[a]k minds like myself."
	     * wrote Pehr Anderson.  Your petition is granted.
	     */
	    if (ctl->fetchall && ctl->keep && (run.poll_interval || ctl->idle) && !nodetach && !configdump)
	    {
		(void) fprintf(stderr,
			       GT_("Both fetchall and keep on in daemon or idle mode is a mistake!\n"));
	    }
	}
    }

    /*
     * If the user didn't set a last-resort user to get misaddressed
     * multidrop mail, set an appropriate default here.
     */
    if (!run.postmaster)
    {
	if (getuid() != ROOT_UID)		/* ordinary user */
	    run.postmaster = user;
	else					/* root */
	    run.postmaster = "postmaster";
    }

    return(implicitmode);
}

static RETSIGTYPE terminate_poll(int sig)
/* to be executed at the end of a poll cycle */
{

    if (sig != 0)
        report(stdout, GT_("terminated with signal %d\n"), sig);

#ifdef POP3_ENABLE
    /*
     * Update UID information at end of each poll, rather than at end
     * of run, because that way we don't lose all UIDL information since
     * the beginning of time if fetchmail crashes.
     */
    if (!check_only)
	write_saved_lists(querylist, run.idfile);
#endif /* POP3_ENABLE */
}

static RETSIGTYPE terminate_run(int sig)
/* to be executed on normal or signal-induced termination */
{
    struct query	*ctl;

    terminate_poll(sig);

    /* 
     * Craig Metz, the RFC1938 one-time-password guy, points out:
     * "Remember that most kernels don't zero pages before handing them to the
     * next process and many kernels share pages between user and kernel space.
     * You'd be very surprised what you can find from a short program to do a
     * malloc() and then dump the contents of the pages you got. By zeroing
     * the secrets at end of run (earlier if you can), you make sure the next
     * guy can't get the password/pass phrase."
     *
     * Right you are, Craig!
     */
    for (ctl = querylist; ctl; ctl = ctl->next)
	if (ctl->password)
	  memset(ctl->password, '\0', strlen(ctl->password));

#if !defined(HAVE_ATEXIT)
    fm_lock_release();
#endif

    if (activecount == 0)
	exit(PS_NOMAIL);
    else
	exit(successes ? PS_SUCCESS : querystatus);
}

/*
 * Sequence of protocols to try when autoprobing, most capable to least.
 */
static const int autoprobe[] = 
{
#ifdef IMAP_ENABLE
    P_IMAP,
#endif /* IMAP_ENABLE */
#ifdef POP3_ENABLE
    P_POP3,
#endif /* POP3_ENABLE */
#ifdef POP2_ENABLE
    P_POP2
#endif /* POP2_ENABLE */
};

static int query_host(struct query *ctl)
/* perform fetch transaction with single host */
{
    size_t i;
    int st = 0;

    /*
     * If we're syslogging the progress messages are automatically timestamped.
     * Force timestamping if we're going to a logfile.
     */
    if (outlevel >= O_VERBOSE)
    {
	report(stdout, GT_("%s querying %s (protocol %s) at %s: poll started\n"),
	       VERSION,
	       ctl->server.pollname,
	       showproto(ctl->server.protocol),
	       timestamp());
    }

    switch (ctl->server.protocol) {
    case P_AUTO:
	for (i = 0; i < sizeof(autoprobe)/sizeof(autoprobe[0]); i++)
	{
	    ctl->server.protocol = autoprobe[i];
	    do {
		st = query_host(ctl);
	    } while 
		(st == PS_REPOLL);
	    if (st == PS_SUCCESS || st == PS_NOMAIL || st == PS_AUTHFAIL || st == PS_LOCKBUSY || st == PS_SMTP || st == PS_MAXFETCH || st == PS_DNS)
		break;
	}
	ctl->server.protocol = P_AUTO;
	break;
    case P_POP2:
#ifdef POP2_ENABLE
	st = doPOP2(ctl);
#else
	report(stderr, GT_("POP2 support is not configured.\n"));
	st = PS_PROTOCOL;
#endif /* POP2_ENABLE */
	break;
    case P_POP3:
    case P_APOP:
    case P_RPOP:
#ifdef POP3_ENABLE
	do {
	    st = doPOP3(ctl);
	} while (st == PS_REPOLL);
#else
	report(stderr, GT_("POP3 support is not configured.\n"));
	st = PS_PROTOCOL;
#endif /* POP3_ENABLE */
	break;
    case P_IMAP:
#ifdef IMAP_ENABLE
	do {
	    st = doIMAP(ctl);
	} while (st == PS_REPOLL);
#else
	report(stderr, GT_("IMAP support is not configured.\n"));
	st = PS_PROTOCOL;
#endif /* IMAP_ENABLE */
	break;
    case P_ETRN:
#ifndef ETRN_ENABLE
	report(stderr, GT_("ETRN support is not configured.\n"));
	st = PS_PROTOCOL;
#else
	st = doETRN(ctl);
	break;
#endif /* ETRN_ENABLE */
    case P_ODMR:
#ifndef ODMR_ENABLE
	report(stderr, GT_("ODMR support is not configured.\n"));
	st = PS_PROTOCOL;
#else
	st = doODMR(ctl);
#endif /* ODMR_ENABLE */
	break;
    default:
	report(stderr, GT_("unsupported protocol selected.\n"));
	st = PS_PROTOCOL;
    }

    /*
     * If we're syslogging the progress messages are automatically timestamped.
     * Force timestamping if we're going to a logfile.
     */
    if (outlevel >= O_VERBOSE)
    {
	report(stdout, GT_("%s querying %s (protocol %s) at %s: poll completed\n"),
	       VERSION,
	       ctl->server.pollname,
	       showproto(ctl->server.protocol),
	       timestamp());
    }

    return(st);
}

static void dump_params (struct runctl *runp,
			 struct query *querylist, flag implicit)
/* display query parameters in English */
{
    struct query *ctl;

    if (runp->poll_interval)
	printf(GT_("Poll interval is %d seconds\n"), runp->poll_interval);
    if (runp->logfile)
	printf(GT_("Logfile is %s\n"), runp->logfile);
    if (strcmp(runp->idfile, IDFILE_NAME))
	printf(GT_("Idfile is %s\n"), runp->idfile);
#if defined(HAVE_SYSLOG)
    if (runp->use_syslog)
	printf(GT_("Progress messages will be logged via syslog\n"));
#endif
    if (runp->invisible)
	printf(GT_("Fetchmail will masquerade and will not generate Received\n"));
    if (runp->showdots)
	printf(GT_("Fetchmail will show progress dots even in logfiles.\n"));
    if (runp->postmaster)
	printf(GT_("Fetchmail will forward misaddressed multidrop messages to %s.\n"),
	       runp->postmaster);

    if (!runp->bouncemail)
	printf(GT_("Fetchmail will direct error mail to the postmaster.\n"));
    else if (outlevel >= O_VERBOSE)
	printf(GT_("Fetchmail will direct error mail to the sender.\n"));

    if (!runp->softbounce)
	printf(GT_("Fetchmail will treat permanent errors as permanent (drop messsages).\n"));
    else if (outlevel >= O_VERBOSE)
	printf(GT_("Fetchmail will treat permanent errors as temporary (keep messages).\n"));

    for (ctl = querylist; ctl; ctl = ctl->next)
    {
	if (!ctl->active || (implicit && ctl->server.skip))
	    continue;

	printf(GT_("Options for retrieving from %s@%s:\n"),
	       ctl->remotename, visbuf(ctl->server.pollname));

	if (ctl->server.via && MAILBOX_PROTOCOL(ctl))
	    printf(GT_("  Mail will be retrieved via %s\n"), ctl->server.via);

	if (ctl->server.interval)
	    printf(ngettext("  Poll of this server will occur every %d interval.\n",
			    "  Poll of this server will occur every %d intervals.\n",
			    ctl->server.interval), ctl->server.interval);
	if (ctl->server.truename)
	    printf(GT_("  True name of server is %s.\n"), ctl->server.truename);
	if (ctl->server.skip || outlevel >= O_VERBOSE)
	    printf(ctl->server.skip
		   ? GT_("  This host will not be queried when no host is specified.\n")
		   : GT_("  This host will be queried when no host is specified.\n"));
	if (!NO_PASSWORD(ctl))
	{
	    if (!ctl->password)
		printf(GT_("  Password will be prompted for.\n"));
	    else if (outlevel >= O_VERBOSE)
	    {
		if (ctl->server.protocol == P_APOP)
		    printf(GT_("  APOP secret = \"%s\".\n"),
			   visbuf(ctl->password));
		else if (ctl->server.protocol == P_RPOP)
		    printf(GT_("  RPOP id = \"%s\".\n"),
			   visbuf(ctl->password));
		else
		    printf(GT_("  Password = \"%s\".\n"),
							visbuf(ctl->password));
	    }
	}

	if (ctl->server.protocol == P_POP3 
	    && ctl->server.service && !strcmp(ctl->server.service, KPOP_PORT)
	    && (ctl->server.authenticate == A_KERBEROS_V4 ||
		ctl->server.authenticate == A_KERBEROS_V5))
	    printf(GT_("  Protocol is KPOP with Kerberos %s authentication"),
		   ctl->server.authenticate == A_KERBEROS_V5 ? "V" : "IV");
	else
	    printf(GT_("  Protocol is %s"), showproto(ctl->server.protocol));
	if (ctl->server.service)
	    printf(GT_(" (using service %s)"), ctl->server.service);
	else if (outlevel >= O_VERBOSE)
	    printf(GT_(" (using default port)"));
	if (ctl->server.uidl && MAILBOX_PROTOCOL(ctl))
	    printf(GT_(" (forcing UIDL use)"));
	putchar('.');
	putchar('\n');
	switch (ctl->server.authenticate)
	{
	case A_ANY:
	    printf(GT_("  All available authentication methods will be tried.\n"));
	    break;
	case A_PASSWORD:
	    printf(GT_("  Password authentication will be forced.\n"));
	    break;
	case A_MSN:
	    printf(GT_("  MSN authentication will be forced.\n"));
	    break;
	case A_NTLM:
	    printf(GT_("  NTLM authentication will be forced.\n"));
	    break;
	case A_OTP:
	    printf(GT_("  OTP authentication will be forced.\n"));
	    break;
	case A_CRAM_MD5:
	    printf(GT_("  CRAM-Md5 authentication will be forced.\n"));
	    break;
	case A_GSSAPI:
	    printf(GT_("  GSSAPI authentication will be forced.\n"));
	    break;
	case A_KERBEROS_V4:
	    printf(GT_("  Kerberos V4 authentication will be forced.\n"));
	    break;
	case A_KERBEROS_V5:
	    printf(GT_("  Kerberos V5 authentication will be forced.\n"));
	    break;
	case A_SSH:
	    printf(GT_("  End-to-end encryption assumed.\n"));
	    break;
	}
	if (ctl->server.principal != (char *) NULL)
	    printf(GT_("  Mail service principal is: %s\n"), ctl->server.principal);
#ifdef	SSL_ENABLE
	if (ctl->use_ssl)
	    printf(GT_("  SSL encrypted sessions enabled.\n"));
	if (ctl->sslproto)
	    printf(GT_("  SSL protocol: %s.\n"), ctl->sslproto);
	if (ctl->sslcertck) {
	    printf(GT_("  SSL server certificate checking enabled.\n"));
	    if (ctl->sslcertpath != NULL)
		printf(GT_("  SSL trusted certificate directory: %s\n"), ctl->sslcertpath);
	}
	if (ctl->sslcommonname != NULL)
		printf(GT_("  SSL server CommonName: %s\n"), ctl->sslcommonname);
	if (ctl->sslfingerprint != NULL)
		printf(GT_("  SSL key fingerprint (checked against the server key): %s\n"), ctl->sslfingerprint);
#endif
	if (ctl->server.timeout > 0)
	    printf(GT_("  Server nonresponse timeout is %d seconds"), ctl->server.timeout);
	if (ctl->server.timeout ==  CLIENT_TIMEOUT)
	    printf(GT_(" (default).\n"));
	else
	    printf(".\n");

	if (MAILBOX_PROTOCOL(ctl)) 
	{
	    if (!ctl->mailboxes->id)
		printf(GT_("  Default mailbox selected.\n"));
	    else
	    {
		struct idlist *idp;

		printf(GT_("  Selected mailboxes are:"));
		for (idp = ctl->mailboxes; idp; idp = idp->next)
		    printf(" %s", idp->id);
		printf("\n");
	    }
	    printf(ctl->fetchall
		   ? GT_("  All messages will be retrieved (--all on).\n")
		   : GT_("  Only new messages will be retrieved (--all off).\n"));
	    printf(ctl->keep
		   ? GT_("  Fetched messages will be kept on the server (--keep on).\n")
		   : GT_("  Fetched messages will not be kept on the server (--keep off).\n"));
	    printf(ctl->flush
		   ? GT_("  Old messages will be flushed before message retrieval (--flush on).\n")
		   : GT_("  Old messages will not be flushed before message retrieval (--flush off).\n"));
	    printf(ctl->limitflush
		   ? GT_("  Oversized messages will be flushed before message retrieval (--limitflush on).\n")
		   : GT_("  Oversized messages will not be flushed before message retrieval (--limitflush off).\n"));
	    printf(ctl->rewrite
		   ? GT_("  Rewrite of server-local addresses is enabled (--norewrite off).\n")
		   : GT_("  Rewrite of server-local addresses is disabled (--norewrite on).\n"));
	    printf(ctl->stripcr
		   ? GT_("  Carriage-return stripping is enabled (stripcr on).\n")
		   : GT_("  Carriage-return stripping is disabled (stripcr off).\n"));
	    printf(ctl->forcecr
		   ? GT_("  Carriage-return forcing is enabled (forcecr on).\n")
		   : GT_("  Carriage-return forcing is disabled (forcecr off).\n"));
	    printf(ctl->pass8bits
		   ? GT_("  Interpretation of Content-Transfer-Encoding is disabled (pass8bits on).\n")
		   : GT_("  Interpretation of Content-Transfer-Encoding is enabled (pass8bits off).\n"));
	    printf(ctl->mimedecode
		   ? GT_("  MIME decoding is enabled (mimedecode on).\n")
		   : GT_("  MIME decoding is disabled (mimedecode off).\n"));
	    printf(ctl->idle
		   ? GT_("  Idle after poll is enabled (idle on).\n")
		   : GT_("  Idle after poll is disabled (idle off).\n"));
	    printf(ctl->dropstatus
		   ? GT_("  Nonempty Status lines will be discarded (dropstatus on)\n")
		   : GT_("  Nonempty Status lines will be kept (dropstatus off)\n"));
	    printf(ctl->dropdelivered
		   ? GT_("  Delivered-To lines will be discarded (dropdelivered on)\n")
		   : GT_("  Delivered-To lines will be kept (dropdelivered off)\n"));
	    if (NUM_NONZERO(ctl->limit))
	    {
		if (NUM_NONZERO(ctl->limit))
		    printf(GT_("  Message size limit is %d octets (--limit %d).\n"), 
			   ctl->limit, ctl->limit);
		else if (outlevel >= O_VERBOSE)
		    printf(GT_("  No message size limit (--limit 0).\n"));
		if (run.poll_interval > 0)
		    printf(GT_("  Message size warning interval is %d seconds (--warnings %d).\n"), 
			   ctl->warnings, ctl->warnings);
		else if (outlevel >= O_VERBOSE)
		    printf(GT_("  Size warnings on every poll (--warnings 0).\n"));
	    }
	    if (NUM_NONZERO(ctl->fetchlimit))
		printf(GT_("  Received-message limit is %d (--fetchlimit %d).\n"),
		       ctl->fetchlimit, ctl->fetchlimit);
	    else if (outlevel >= O_VERBOSE)
		printf(GT_("  No received-message limit (--fetchlimit 0).\n"));
	    if (NUM_NONZERO(ctl->fetchsizelimit))
		printf(GT_("  Fetch message size limit is %d (--fetchsizelimit %d).\n"),
		       ctl->fetchsizelimit, ctl->fetchsizelimit);
	    else if (outlevel >= O_VERBOSE)
		printf(GT_("  No fetch message size limit (--fetchsizelimit 0).\n"));
	    if (NUM_NONZERO(ctl->fastuidl) && MAILBOX_PROTOCOL(ctl))
	    {
		if (ctl->fastuidl == 1)
		    printf(GT_("  Do binary search of UIDs during each poll (--fastuidl 1).\n"));
		else
		    printf(GT_("  Do binary search of UIDs during %d out of %d polls (--fastuidl %d).\n"), ctl->fastuidl - 1, ctl->fastuidl, ctl->fastuidl);
	    }
	    else if (outlevel >= O_VERBOSE)
		printf(GT_("   Do linear search of UIDs during each poll (--fastuidl 0).\n"));
	    if (NUM_NONZERO(ctl->batchlimit))
		printf(GT_("  SMTP message batch limit is %d.\n"), ctl->batchlimit);
	    else if (outlevel >= O_VERBOSE)
		printf(GT_("  No SMTP message batch limit (--batchlimit 0).\n"));
	    if (MAILBOX_PROTOCOL(ctl))
	    {
		if (NUM_NONZERO(ctl->expunge))
		    printf(GT_("  Deletion interval between expunges forced to %d (--expunge %d).\n"), ctl->expunge, ctl->expunge);
		else if (outlevel >= O_VERBOSE)
		    printf(GT_("  No forced expunges (--expunge 0).\n"));
	    }
	}
	else	/* ODMR or ETRN */
	{
	    struct idlist *idp;

	    printf(GT_("  Domains for which mail will be fetched are:"));
	    for (idp = ctl->domainlist; idp; idp = idp->next)
	    {
		printf(" %s", idp->id);
		if (!idp->val.status.mark)
		    printf(GT_(" (default)"));
	    }
	    printf("\n");
	}
	if (ctl->bsmtp)
	    printf(GT_("  Messages will be appended to %s as BSMTP\n"), visbuf(ctl->bsmtp));
	else if (ctl->mda && MAILBOX_PROTOCOL(ctl))
	    printf(GT_("  Messages will be delivered with \"%s\".\n"), visbuf(ctl->mda));
	else
	{
	    struct idlist *idp;

	    if (ctl->smtphunt)
	    {
		printf(GT_("  Messages will be %cMTP-forwarded to:"), 
		       ctl->listener);
		for (idp = ctl->smtphunt; idp; idp = idp->next)
		{
		    printf(" %s", idp->id);
		    if (!idp->val.status.mark)
			printf(GT_(" (default)"));
		}
		printf("\n");
	    }
	    if (ctl->smtpaddress)
		printf(GT_("  Host part of MAIL FROM line will be %s\n"),
		       ctl->smtpaddress);
	    if (ctl->smtpname)
		printf(GT_("  Address to be put in RCPT TO lines shipped to SMTP will be %s\n"),
		       ctl->smtpname);
	}
	if (MAILBOX_PROTOCOL(ctl))
	{
		if (ctl->antispam != (struct idlist *)NULL)
		{
		    struct idlist *idp;

		    printf(GT_("  Recognized listener spam block responses are:"));
		    for (idp = ctl->antispam; idp; idp = idp->next)
			printf(" %d", idp->val.status.num);
		    printf("\n");
		}
		else if (outlevel >= O_VERBOSE)
		    printf(GT_("  Spam-blocking disabled\n"));
	}
	if (ctl->preconnect)
	    printf(GT_("  Server connection will be brought up with \"%s\".\n"),
		   visbuf(ctl->preconnect));
	else if (outlevel >= O_VERBOSE)
	    printf(GT_("  No pre-connection command.\n"));
	if (ctl->postconnect)
	    printf(GT_("  Server connection will be taken down with \"%s\".\n"),
		   visbuf(ctl->postconnect));
	else if (outlevel >= O_VERBOSE)
	    printf(GT_("  No post-connection command.\n"));
	if (MAILBOX_PROTOCOL(ctl)) {
		if (!ctl->localnames)
		    printf(GT_("  No localnames declared for this host.\n"));
		else
		{
		    struct idlist *idp;
		    int count = 0;

		    for (idp = ctl->localnames; idp; idp = idp->next)
			++count;

		    if (count > 1 || ctl->wildcard)
			printf(GT_("  Multi-drop mode: "));
		    else
			printf(GT_("  Single-drop mode: "));

		    printf(ngettext("%d local name recognized.\n", "%d local names recognized.\n", count), count);
		    if (outlevel >= O_VERBOSE)
		    {
			for (idp = ctl->localnames; idp; idp = idp->next)
			    if (idp->val.id2)
				printf("\t%s -> %s\n", idp->id, idp->val.id2);
			    else
				printf("\t%s\n", idp->id);
			if (ctl->wildcard)
			    fputs("\t*\n", stdout);
		    }

		    if (count > 1 || ctl->wildcard)
		    {
			printf(ctl->server.dns
			       ? GT_("  DNS lookup for multidrop addresses is enabled.\n")
			       : GT_("  DNS lookup for multidrop addresses is disabled.\n"));
			if (ctl->server.dns)
			{
	       		    if (ctl->server.checkalias)
				printf(GT_("  Server aliases will be compared with multidrop addresses by IP address.\n"));
			    else
				printf(GT_("  Server aliases will be compared with multidrop addresses by name.\n"));
			}
			if (ctl->server.envelope == STRING_DISABLED)
			    printf(GT_("  Envelope-address routing is disabled\n"));
			else
			{
			    printf(GT_("  Envelope header is assumed to be: %s\n"),
				   ctl->server.envelope ? ctl->server.envelope : "Received");
			    if (ctl->server.envskip || outlevel >= O_VERBOSE)
				printf(GT_("  Number of envelope headers to be skipped over: %d\n"),
				       ctl->server.envskip);
			    if (ctl->server.qvirtual)
				printf(GT_("  Prefix %s will be removed from user id\n"),
				       ctl->server.qvirtual);
			    else if (outlevel >= O_VERBOSE) 
				printf(GT_("  No prefix stripping\n"));
			}

			if (ctl->server.akalist)
			{
			    struct idlist *idp;

			    printf(GT_("  Predeclared mailserver aliases:"));
			    for (idp = ctl->server.akalist; idp; idp = idp->next)
				printf(" %s", idp->id);
			    putchar('\n');
			}
			if (ctl->server.localdomains)
			{
			    struct idlist *idp;

			    printf(GT_("  Local domains:"));
			    for (idp = ctl->server.localdomains; idp; idp = idp->next)
				printf(" %s", idp->id);
			    putchar('\n');
			}
		    }
		}
	}
#ifdef CAN_MONITOR
	if (ctl->server.interface)
	    printf(GT_("  Connection must be through interface %s.\n"), ctl->server.interface);
	else if (outlevel >= O_VERBOSE)
	    printf(GT_("  No interface requirement specified.\n"));
	if (ctl->server.monitor)
	    printf(GT_("  Polling loop will monitor %s.\n"), ctl->server.monitor);
	else if (outlevel >= O_VERBOSE)
	    printf(GT_("  No monitor interface specified.\n"));
#endif

	if (ctl->server.plugin)
	    printf(GT_("  Server connections will be made via plugin %s (--plugin %s).\n"), ctl->server.plugin, ctl->server.plugin);
	else if (outlevel >= O_VERBOSE)
	    printf(GT_("  No plugin command specified.\n"));
	if (ctl->server.plugout)
	    printf(GT_("  Listener connections will be made via plugout %s (--plugout %s).\n"), ctl->server.plugout, ctl->server.plugout);
	else if (outlevel >= O_VERBOSE)
	    printf(GT_("  No plugout command specified.\n"));

	if (ctl->server.protocol > P_POP2 && MAILBOX_PROTOCOL(ctl))
	{
	    if (!ctl->oldsaved)
		printf(GT_("  No UIDs saved from this host.\n"));
	    else
	    {
		struct idlist *idp;
		int count = 0;

		for (idp = ctl->oldsaved; idp; idp = idp->next)
		    ++count;

		printf(GT_("  %d UIDs saved.\n"), count);
		if (outlevel >= O_VERBOSE)
		    for (idp = ctl->oldsaved; idp; idp = idp->next)
			printf("\t%s\n", idp->id);
	    }
	}

        if (ctl->server.tracepolls)
            printf(GT_("  Poll trace information will be added to the Received header.\n"));
        else if (outlevel >= O_VERBOSE)
            printf(GT_("  No poll trace information will be added to the Received header.\n.\n"));

	if (ctl->properties)
	    printf(GT_("  Pass-through properties \"%s\".\n"),
		   visbuf(ctl->properties));
    }
}

/* fetchmail.c ends here */
