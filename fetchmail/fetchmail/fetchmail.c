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
#include <sys/time.h>	/* needed for Sun 4.1.2 */
#ifdef HAVE_SETRLIMIT
#include <sys/resource.h>
#endif /* HAVE_SETRLIMIT */
#include <sys/utsname.h>

#ifdef HAVE_NET_SOCKET_H
#include <net/socket.h>
#endif
#ifdef HAVE_GETHOSTBYNAME
#include <netdb.h>
#endif /* HAVE_GETHOSTBYNAME */

#ifdef HESIOD
#include <hesiod.h>
#endif

#include "getopt.h"
#include "fetchmail.h"
#include "socket.h"
#include "tunable.h"
#include "smtp.h"
#include "netrc.h"
#include "i18n.h"

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
flag check_only;	    /* if --probe was set */
flag versioninfo;	    /* emit only version info */
char *user;		    /* the name of the invoking user */
char *home;		    /* invoking user's home directory */
char *fmhome;		    /* fetchmail's home directory */
char *program_name;	    /* the name to prefix error messages with */
flag configdump;	    /* dump control blocks for configurator */
char *fetchmailhost;	    /* either `localhost' or the host's FQDN */

#if NET_SECURITY
void *request = NULL;
int requestlen = 0;
#endif /* NET_SECURITY */

static int querystatus;		/* status of query */
static int successes;		/* count number of successful polls */
static int activecount;		/* count number of active entries */
static struct runctl cmd_run;	/* global options set from command line */
static time_t parsetime;	/* time of last parse */

static void terminate_run(int);
static void terminate_poll(int);

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
#include <time.h>
#include <locale.h>
static char *timestamp (void)
{
    time_t      now;
    static char buf[60];

    time (&now);
    setlocale (LC_TIME, "");
    strftime (buf, sizeof (buf), "%c", localtime(&now));
    setlocale (LC_TIME, "C");
    return (buf);
}
#else
#define timestamp rfc822timestamp
#endif

int main(int argc, char **argv)
{
    int bkgd = FALSE;
    int parsestatus, implicitmode = FALSE;
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
#endif

    /*
     * Note: because we can't initialize reporting before we  know whether
     * syslog is supposed to be on, this message will go to stdout and
     * be lost when running in background.
     */
    if (outlevel >= O_VERBOSE)
    {
	int i;

	report(stdout, _("fetchmail: invoked with"));
	for (i = 0; i < argc; i++)
	    report(stdout, " %s", argv[i]);
	report(stdout, "\n");
    }

#define IDFILE_NAME	".fetchids"
    run.idfile = (char *) xmalloc(strlen(fmhome)+sizeof(IDFILE_NAME)+2);
    strcpy(run.idfile, fmhome);
    strcat(run.idfile, "/");
    strcat(run.idfile, IDFILE_NAME);
  
    outlevel = O_NORMAL;

    /*
     * We used to arrange for the lock to be removed on exit close
     * to where the lock was asserted.  Now we need to do it here, because
     * we might have re-executed in background with an existing lock
     * as the result of a changed rcfile (see the code near the execvp(3)
     * call near the beginning of the polling loop for details).  We want
     * to be sure the lock gets nuked on any error exit, basically.
     */
    lock_dispose();

    if ((parsestatus = parsecmdline(argc,argv, &cmd_run, &cmd_opts)) < 0)
	exit(PS_SYNTAX);

    if (versioninfo)
    {
	printf(_("This is fetchmail release %s"), VERSION);
#ifdef POP2_ENABLE
	printf("+POP2");
#endif /* POP2_ENABLE */
#ifndef POP3_ENABLE
	printf("-POP3");
#endif /* POP3_ENABLE */
#ifndef IMAP_ENABLE
	printf("-IMAP");
#endif /* IMAP_ENABLE */
#ifdef GSSAPI
	printf("+IMAP-GSS");
#endif /* GSSAPI */
#ifdef RPA_ENABLE
	printf("+RPA");
#endif /* RPA_ENABLE */
#ifdef NTLM_ENABLE
	printf("+NTLM");
#endif /* NTLM_ENABLE */
#ifdef SDPS_ENABLE
	printf("+SDPS");
#endif /* SDPS_ENABLE */
#ifndef ETRN_ENABLE
	printf("-ETRN");
#endif /* ETRN_ENABLE */
#ifndef ODMR_ENABLE
	printf("-ODMR");
#endif /* ODMR_ENABLE */
#ifdef SSL_ENABLE
	printf("+SSL");
#endif
#if OPIE_ENABLE
	printf("+OPIE");
#endif /* OPIE_ENABLE */
#if INET6_ENABLE
	printf("+INET6");
#endif /* INET6_ENABLE */
#if NET_SECURITY
	printf("+NETSEC");
#endif /* NET_SECURITY */
#ifdef HAVE_SOCKS
	printf("+SOCKS");
#endif /* HAVE_SOCKS */
#if ENABLE_NLS
	printf("+NLS");
#endif /* ENABLE_NLS */
	putchar('\n');
	fflush(stdout);

	/* this is an attempt to help remote debugging */
	system("uname -a");
    }

    /* avoid parsing the config file if all we're doing is killing a daemon */ 
    if (!(quitmode && argc == 2))
	implicitmode = load_params(argc, argv, optind);

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

    /* construct the lockfile */
    lock_setup();

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
    xalloca(netrc_file, char *, strlen(home) + sizeof(NETRC_FILE) + 2);
    strcpy (netrc_file, home);
    strcat (netrc_file, "/");
    strcat (netrc_file, NETRC_FILE);
    netrc_list = parse_netrc(netrc_file);
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

    /* perhaps we just want to check options? */
    if (versioninfo)
    {
	int havercfile = access(rcfile, 0);

	printf(_("Taking options from command line%s%s\n"),
				havercfile ? "" :  _(" and "),
				havercfile ? "" : rcfile);

	if (querylist == NULL)
	    fprintf(stderr,
		    _("No mailservers set up -- perhaps %s is missing?\n"),
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
    pid = lock_state();
    bkgd = (pid < 0);
    pid = bkgd ? -pid : pid;

    /* if no mail servers listed and nothing in background, we're done */
    if (!(quitmode && argc == 2) && pid == 0 && querylist == NULL) {
	(void)fputs(_("fetchmail: no mailservers have been specified.\n"),stderr);
	exit(PS_SYNTAX);
    }

    /* perhaps user asked us to kill the other fetchmail */
    if (quitmode)
    {
	if (pid == 0) 
	{
	    fprintf(stderr,_("fetchmail: no other fetchmail is running\n"));
	    if (argc == 2)
		exit(PS_EXCLUDE);
	}
	else if (kill(pid, SIGTERM) < 0)
	{
	    fprintf(stderr,_("fetchmail: error killing %s fetchmail at %d; bailing out.\n"),
		    bkgd ? _("background") : _("foreground"), pid);
	    exit(PS_EXCLUDE);
	}
	else
	{
	    fprintf(stderr,_("fetchmail: %s fetchmail at %d killed.\n"),
		    bkgd ? _("background") : _("foreground"), pid);
	    lock_release();
	    if (argc == 2)
		exit(0);
	    else
		pid = 0; 
	}
    }

    /* another fetchmail is running -- wake it up or die */
    if (pid != 0)
    {
	if (check_only)
	{
	    fprintf(stderr,
		 _("fetchmail: can't check mail while another fetchmail to same host is running.\n"));
	    return(PS_EXCLUDE);
        }
	else if (!implicitmode)
	{
	    fprintf(stderr,
		 _("fetchmail: can't poll specified hosts with another fetchmail running at %d.\n"),
		 pid);
		return(PS_EXCLUDE);
	}
	else if (!bkgd)
	{
	    fprintf(stderr,
		 _("fetchmail: another foreground fetchmail is running at %d.\n"),
		 pid);
		return(PS_EXCLUDE);
	}
	else if (argc > 1)
	{
	    /* this test enables re-execing on a changed rcfile */
	    if (getpid() == pid)
		lock_assert();
	    else
	    {
		fprintf(stderr,
			_("fetchmail: can't accept options while a background fetchmail is running.\n"));
		return(PS_EXCLUDE);
	    }
	}
	else if (kill(pid, SIGUSR1) == 0)
	{
	    fprintf(stderr,
		    _("fetchmail: background fetchmail at %d awakened.\n"),
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
		    _("fetchmail: elder sibling at %d died mysteriously.\n"),
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
			_("fetchmail: can't find a password for %s@%s.\n"),
			ctl->remotename, ctl->server.pollname);
		return(PS_AUTHFAIL);
	    }
	    else
	    {
		char* password_prompt = _("Enter password for %s@%s: ");

		xalloca(tmpbuf, char *, strlen(password_prompt) +
			strlen(ctl->remotename) +
			strlen(ctl->server.pollname) + 1);
		(void) sprintf(tmpbuf, password_prompt,
			       ctl->remotename, ctl->server.pollname);
		ctl->password = xstrdup((char *)fm_getpassword(tmpbuf));
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

    /*
     * Maybe time to go to demon mode...
     */
    if (run.poll_interval)
    {
	if (nodetach)
	    deal_with_sigchld();
	else
	    daemonize(run.logfile, terminate_run);
	report(stdout, _("starting fetchmail %s daemon \n"), VERSION);

	/*
	 * We'll set up a handler for these when we're sleeping,
	 * but ignore them otherwise so as not to interrupt a poll.
	 */
	signal(SIGUSR1, SIG_IGN);
	if (run.poll_interval && !getuid())
	    signal(SIGHUP, SIG_IGN);
    }
    else
    {
	deal_with_sigchld();  /* or else we may accumulate too many zombies */
	if (run.logfile && access(run.logfile, F_OK) == 0)
    	{
	    freopen(run.logfile, "a", stdout);
	    freopen(run.logfile, "a", stderr);
    	}
    }

#ifdef linux
    interface_init();
#endif /* linux */

    /* beyond here we don't want more than one fetchmail running per user */
    umask(0077);
    signal(SIGABRT, terminate_run);
    signal(SIGINT, terminate_run);
    signal(SIGTERM, terminate_run);
    signal(SIGALRM, terminate_run);
    signal(SIGPIPE, terminate_run);
    signal(SIGQUIT, terminate_run);

    /* here's the exclusion lock */
    lock_or_die();

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

	if (stat(rcfile, &rcstat) == -1)
	{
	    if (errno != ENOENT)
		report(stderr, 
		       _("couldn't time-check %s (error %d)\n"),
		       rcfile, errno);
	}
	else if (rcstat.st_mtime > parsetime)
	{
	    report(stdout, _("restarting fetchmail (%s changed)\n"), rcfile);
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
	    report(stderr, _("attempt to re-exec fetchmail failed\n"));
	}

#if defined(HAVE_RES_SEARCH) && defined(USE_TCPIP_FOR_DNS)
	/*
	 * This was an efficiency hack that backfired.  The theory
	 * was that using TCP/IP for DNS queries would get us better
	 * reliability and shave off some per-UDP-packet costs.
	 * Unfortunately it interacted badly with diald, which effectively 
	 * filters out DNS queries over TCP/IP for reasons having to do
	 * with some obscure Linux kernel problem involving bootstrapping of
	 * dynamically-addressed links.  I don't understand this mess
	 * and don't want to, so it's "See ya!" to this hack.
	 */
	sethostent(TRUE);	/* use TCP/IP for mailserver queries */
#endif /* HAVE_RES_SEARCH */

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
			       _("poll of %s skipped (failed authentication or too many timeouts)\n"),
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
				       _("interval not reached, not querying %s\n"),
				       ctl->server.pollname);
			    continue;
			}
		    }

#if (defined(linux) && !INET6_ENABLE) || defined(__FreeBSD__)
		    /*
		     * Don't do monitoring if we were woken by a signal.
		     * Note that interface_approve() does its own error logging.
		     */
		    if (!interface_approve(&ctl->server, !lastsig))
			continue;
#endif /* (defined(linux) && !INET6_ENABLE) || defined(__FreeBSD__) */

		    querystatus = query_host(ctl);

#ifdef POP3_ENABLE
		    /* leave the UIDL state alone if there have been any errors */
		    if (!check_only &&
				((querystatus==PS_SUCCESS) || (querystatus==PS_NOMAIL) || (querystatus==PS_MAXFETCH)))
			uid_swap_lists(ctl);
#endif  /* POP3_ENABLE */

		    if (querystatus == PS_SUCCESS)
			successes++;
		    else if (!check_only && 
			     ((querystatus!=PS_NOMAIL) || (outlevel==O_DEBUG)))
			switch(querystatus)
			{
			case PS_SUCCESS:
			    report(stdout,_("Query status=0 (SUCCESS)\n"));break;
			case PS_NOMAIL: 
			    report(stdout,_("Query status=1 (NOMAIL)\n")); break;
			case PS_SOCKET:
			    report(stdout,_("Query status=2 (SOCKET)\n")); break;
			case PS_AUTHFAIL:
			    report(stdout,_("Query status=3 (AUTHFAIL)\n"));break;
			case PS_PROTOCOL:
			    report(stdout,_("Query status=4 (PROTOCOL)\n"));break;
			case PS_SYNTAX:
			    report(stdout,_("Query status=5 (SYNTAX)\n")); break;
			case PS_IOERR:
			    report(stdout,_("Query status=6 (IOERR)\n"));  break;
			case PS_ERROR:
			    report(stdout,_("Query status=7 (ERROR)\n"));  break;
			case PS_EXCLUDE:
			    report(stdout,_("Query status=8 (EXCLUDE)\n")); break;
			case PS_LOCKBUSY:
			    report(stdout,_("Query status=9 (LOCKBUSY)\n"));break;
			case PS_SMTP:
			    report(stdout,_("Query status=10 (SMTP)\n")); break;
			case PS_DNS:
			    report(stdout,_("Query status=11 (DNS)\n")); break;
			case PS_BSMTP:
			    report(stdout,_("Query status=12 (BSMTP)\n")); break;
			case PS_MAXFETCH:
			    report(stdout,_("Query status=13 (MAXFETCH)\n"));break;
			default:
			    report(stdout,_("Query status=%d\n"),querystatus);
			    break;
			}

#if (defined(linux) && !INET6_ENABLE) || defined (__FreeBSD__)
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
#endif /* (defined(linux) && !INET6_ENABLE) || defined(__FreeBSD__) */
		}
	    }

#if defined(HAVE_RES_SEARCH) && defined(USE_TCPIP_FOR_DNS)
	endhostent();		/* release TCP/IP connection to nameserver */
#endif /* HAVE_RES_SEARCH */

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
		report(stderr, _("All connections are wedged.  Exiting.\n"));
		/* FIXME: someday, send notification mail */
		exit(PS_AUTHFAIL);
	    }

	    if (outlevel >= O_VERBOSE)
		report(stdout, 
		       _("fetchmail: sleeping at %s\n"), timestamp());

	    /*
	     * OK, now pause until it's time for the next poll cycle.
	     * A nonzero return indicates we received a wakeup signal;
	     * unwedge all servers in case the problem has been
	     * manually repaired.
	     */
	    if ((lastsig = interruptible_idle(run.poll_interval)))
	    {
#ifdef SYS_SIGLIST_DECLARED
		report(stdout, 
		       _("awakened by %s\n"), sys_siglist[lastsig]);
#else
		report(stdout, 
		       _("awakened by signal %d\n"), lastsig);
#endif
		for (ctl = querylist; ctl; ctl = ctl->next)
		    ctl->wedged = FALSE;
	    }

	    if (outlevel >= O_VERBOSE)
		report(stdout, _("awakened at %s\n"), timestamp());
	}
    } while
	(run.poll_interval);

    if (outlevel >= O_VERBOSE)
	report(stdout, _("normal termination, status %d\n"),
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
#if INET6_ENABLE
    FLAG_MERGE(server.service);
    FLAG_MERGE(server.netsec);
#else /* INET6_ENABLE */
    FLAG_MERGE(server.port);
#endif /* INET6_ENABLE */
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

#if defined(linux) || defined(__FreeBSD__)
    FLAG_MERGE(server.interface);
    FLAG_MERGE(server.monitor);
    FLAG_MERGE(server.interface_pair);
#endif /* linux || defined(__FreeBSD__) */

    FLAG_MERGE(server.plugin);
    FLAG_MERGE(server.plugout);

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
    FLAG_MERGE(batchlimit);
#ifdef	SSL_ENABLE
    FLAG_MERGE(use_ssl);
    FLAG_MERGE(sslkey);
    FLAG_MERGE(sslcert);
    FLAG_MERGE(sslproto);
    FLAG_MERGE(sslcertck);
    FLAG_MERGE(sslcertpath);
    FLAG_MERGE(sslfingerprint);
#endif
    FLAG_MERGE(expunge);

    FLAG_MERGE(tracepolls);
    FLAG_MERGE(properties);
#undef FLAG_MERGE
}

static int load_params(int argc, char **argv, int optind)
{
    int	implicitmode, st;
    struct passwd *pw;
    struct query def_opts, *ctl;
    struct stat rcstat;

    run.bouncemail = TRUE;
    run.spambounce = FALSE;	/* don't bounce back to innocent bystanders */

    memset(&def_opts, '\0', sizeof(struct query));
    def_opts.smtp_socket = -1;
    def_opts.smtpaddress = (char *)0;
    def_opts.smtpname = (char *)0;
#define ANTISPAM(n)	save_str(&def_opts.antispam, STRING_DUMMY, 0)->val.status.num = (n)
    ANTISPAM(571);	/* sendmail */
    ANTISPAM(550);	/* old exim */
    ANTISPAM(501);	/* new exim */
    ANTISPAM(554);	/* Postfix */
#undef ANTISPAM

    def_opts.server.protocol = P_AUTO;
    def_opts.server.timeout = CLIENT_TIMEOUT;
    def_opts.warnings = WARNING_INTERVAL;
    def_opts.remotename = user;
    def_opts.listener = SMTP_MODE;

    /* note the parse time, so we can pick up on modifications */
    parsetime = 0;	/* foil compiler warnings */
    if (stat(rcfile, &rcstat) != -1)
	parsetime = rcstat.st_mtime;
    else if (errno != ENOENT)
	report(stderr, _("couldn't time-check the run-control file\n"));

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
		    if (predeclared && outlevel == O_VERBOSE)
			fprintf(stderr,_("Warning: multiple mentions of host %s in config file\n"),argv[optind]);
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
    for (ctl = querylist; ctl; ctl = ctl->next)
	if (ctl != querylist && strcmp(ctl->server.pollname, "defaults") == 0)
	    exit(PS_SYNTAX);

    /* use localhost if we never fetch the FQDN of this host */
    fetchmailhost = "localhost";

    /* here's where we override globals */
    if (cmd_run.logfile)
	run.logfile = cmd_run.logfile;
    if (cmd_run.idfile)
	run.idfile = cmd_run.idfile;
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
    if (strcmp(fetchmailhost, "localhost") == 0)
	for (ctl = querylist; ctl; ctl = ctl->next)
	    if (ctl->active && 
		(ctl->server.protocol==P_ETRN || ctl->server.protocol==P_ODMR
		 || ctl->server.authenticate == A_KERBEROS_V4
		 || ctl->server.authenticate == A_KERBEROS_V5))
	    {
		fetchmailhost = host_fqdn();
		break;
	    }

    /* merge in wired defaults, do sanity checks and prepare internal fields */
    for (ctl = querylist; ctl; ctl = ctl->next)
    {
	ctl->wedged = FALSE;

	if (configdump || ctl->active )
	{
	    /* merge in defaults */
	    optmerge(ctl, &def_opts, FALSE);

	    /* force command-line options */
	    optmerge(ctl, &cmd_opts, TRUE);

	    /* this code enables flags to be turned off */
#define DEFAULT(flag, dflt)	if (flag == FLAG_TRUE)\
	    				flag = TRUE;\
				else if (flag == FLAG_FALSE)\
					flag = FALSE;\
				else\
					flag = (dflt)
	    DEFAULT(ctl->keep, FALSE);
	    DEFAULT(ctl->fetchall, FALSE);
	    DEFAULT(ctl->flush, FALSE);
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
#ifdef	SSL_ENABLE
	    DEFAULT(ctl->use_ssl, FALSE);
	    DEFAULT(ctl->sslcertck, FALSE);
#endif
	    DEFAULT(ctl->server.checkalias, FALSE);
#undef DEFAULT

	    /*
	     * Make sure we have a nonempty host list to forward to.
	     */
	    if (!ctl->smtphunt)
		save_str(&ctl->smtphunt, fetchmailhost, FALSE);

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

#if !defined(HAVE_GETHOSTBYNAME) || !defined(HAVE_RES_SEARCH)
	    /* can't handle multidrop mailboxes unless we can do DNS lookups */
	    if (ctl->localnames && ctl->localnames->next && ctl->server.dns)
	    {
		ctl->server.dns = FALSE;
		report(stderr, _("fetchmail: warning: no DNS available to check multidrop fetches from %s\n"), ctl->server.pollname);
	    }
#endif /* !HAVE_GETHOSTBYNAME || !HAVE_RES_SEARCH */

	    if (ctl->server.via) 
		ctl->server.queryname = xstrdup(ctl->server.via);
	    else
		ctl->server.queryname = xstrdup(ctl->server.pollname);

#ifdef HESIOD
        /* If either the pollname or vianame are "hesiod" we want to
           lookup the user's hesiod pobox host */

        if (!strcasecmp(ctl->server.queryname, "hesiod")) {
            struct hes_postoffice *hes_p;
            hes_p = hes_getmailhost(ctl->remotename);
            if (hes_p != NULL && strcmp(hes_p->po_type, "POP") == 0) {
                 free(ctl->server.queryname);
                 ctl->server.queryname = xstrdup(hes_p->po_host);
                 if (ctl->server.via)
                     free(ctl->server.via);
                 ctl->server.via = xstrdup(hes_p->po_host);
            } else {
                 report(stderr,
			_("couldn't find HESIOD pobox for %s\n"),
			ctl->remotename);
            }
        }
#endif /* HESIOD */

	    /*
	     * We may have to canonicalize the server truename for later use.
	     * Do this just once for each lead server, if necessary, in order
	     * to minimize DNS round trips.
	     */
	    if (ctl->server.lead_server)
	    {
		char	*leadname = ctl->server.lead_server->truename;

		/* prevent core dump from ill-formed or duplicate entry */
		if (!leadname)
		{
		    report(stderr, _("Lead server has no name.\n"));
		    exit(PS_SYNTAX);
		}

		ctl->server.truename = xstrdup(leadname);
	    }
	    else if (ctl->active && ctl->server.dns && !configdump)
	    {
#ifndef HAVE_GETHOSTBYNAME
		ctl->server.truename = xstrdup(ctl->server.queryname);
		ctl->server.trueaddr = NULL;
#else
		struct hostent	*namerec;
		    
		/* 
		 * Get the host's IP, so we can report it like this:
		 *
		 * Received: from hostname [10.0.0.1]
		 *
		 * For ultra-efficiency, we should find the IP later, when
		 * we are actually resolving the hostname for a connection.
		 * Problem is this would have to be done inside SockOpen
		 * and there's no way to do that that wouldn't both (a)
		 * be horribly complicated, and (b) blow a couple of
		 * layers of modularity all to hell.
		 */
		errno = 0;
		namerec = gethostbyname(ctl->server.queryname);
		if (namerec == (struct hostent *)NULL)
		{
		    report(stderr,
			   _("couldn't find canonical DNS name of %s\n"),
			   ctl->server.pollname);
		    ctl->server.truename = xstrdup(ctl->server.queryname);
		    ctl->server.trueaddr = NULL;
		    ctl->active = FALSE;
		    /* use this initially to flag DNS errors */
		    ctl->wedged = TRUE;
		}
		else {
		    ctl->server.truename=xstrdup((char *)namerec->h_name);
		    ctl->server.trueaddr=xmalloc(namerec->h_length);
		    memcpy(ctl->server.trueaddr, 
			   namerec->h_addr_list[0],
			   namerec->h_length);
		    ctl->wedged = FALSE;
		}
#endif /* HAVE_GETHOSTBYNAME */
	    }
	    else
		/*
		 * This is a kluge.  It enables users to edit their
		 * configurations when DNS isn't available.
		 */
		ctl->server.truename = xstrdup(ctl->server.queryname);

	    /* if no folders were specified, set up the null one as default */
	    if (!ctl->mailboxes)
		save_str(&ctl->mailboxes, (char *)NULL, 0);

	    /* maybe user overrode timeout on command line? */
	    if (ctl->server.timeout == -1)	
		ctl->server.timeout = CLIENT_TIMEOUT;

#if !INET6_ENABLE
	    /* sanity checks */
	    if (ctl->server.port < 0)
	    {
		(void) fprintf(stderr,
			       _("%s configuration invalid, port number cannot be negative\n"),
			       ctl->server.pollname);
		exit(PS_SYNTAX);
	    }
	    if (ctl->server.protocol == P_RPOP && ctl->server.port >= 1024)
	    {
		(void) fprintf(stderr,
			       _("%s configuration invalid, RPOP requires a privileged port\n"),
			       ctl->server.pollname);
		exit(PS_SYNTAX);
	    }
	    if (ctl->listener == LMTP_MODE)
	    {
		struct idlist	*idp;

		for (idp = ctl->smtphunt; idp; idp = idp->next)
		{
		    char	*cp;

		    if (!(cp = strrchr(idp->id, '/')) ||
				(atoi(++cp) == SMTP_PORT))
		    {
			(void) fprintf(stderr,
				       _("%s configuration invalid, LMTP can't use default SMTP port\n"),
				       ctl->server.pollname);
			exit(PS_SYNTAX);
		    }
		}
	    }
#endif /* !INET6_ENABLE */

	    /*
	     * "I beg to you, have mercy on the week minds like myself."
	     * wrote Pehr Anderson.  Your petition is granted.
	     */
	    if (ctl->fetchall && ctl->keep && run.poll_interval && !nodetach)
	    {
		(void) fprintf(stderr,
			       _("Both fetchall and keep on in daemon mode is a mistake!\n"));
		exit(PS_SYNTAX);
	    }
	}
    }

#ifdef POP3_ENABLE
    /* initialize UID handling */
    if (!versioninfo && (st = prc_filecheck(run.idfile, !versioninfo)) != 0)
	exit(st);
    else
	initialize_saved_lists(querylist, run.idfile);
#endif /* POP3_ENABLE */

    /*
     * If the user didn't set a last-resort user to get misaddressed
     * multidrop mail, set an appropriate default here.
     */
    if (!run.postmaster)
    {
	if (getuid())				/* ordinary user */
	    run.postmaster = user;
	else					/* root */
	    run.postmaster = "postmaster";
    }

    /*
     * If all connections are wedged due to DNS errors, quit.  This is
     * important for the common case that you just have one connection.
     */
    if (querylist)
    {
	st = PS_DNS;
	for (ctl = querylist; ctl; ctl = ctl->next)
	    if (!ctl->wedged)
		st = 0;
	if (st == PS_DNS)
	{
	    (void) fprintf(stderr,
			   _("all mailserver name lookups failed, exiting\n"));
	    exit(PS_DNS);
	}
    }

    return(implicitmode);
}

static void terminate_poll(int sig)
/* to be executed at the end of a poll cycle */
{
    /*
     * Close all SMTP delivery sockets.  For optimum performance
     * we'd like to hold them open til end of run, but (1) this
     * loses if our poll interval is longer than the MTA's inactivity
     * timeout, and (2) some MTAs (like smail) don't deliver after
     * each message, but rather queue up mail and wait to actually
     * deliver it until the input socket is closed. 
     *
     * Sending SMTP QUIT on signal is theoretically nice, but led to a 
     * subtle bug.  If fetchmail was terminated by signal while it was 
     * shipping message text, it would hang forever waiting for a
     * command acknowledge.  In theory we could enable the QUIT
     * only outside of the message send.  In practice, we don't
     * care.  All mailservers hang up on a dropped TCP/IP connection
     * anyway.
     */

    if (sig != 0)
        report(stdout, _("terminated with signal %d\n"), sig);
    else
    {
	struct query *ctl;

	/* terminate all SMTP connections cleanly */
	for (ctl = querylist; ctl; ctl = ctl->next)
	    if (ctl->smtp_socket != -1)
	    {
		/* don't send QUIT for ODMR case because we're acting
		   as a proxy between the SMTP server and client. */
		if (ctl->server.protocol != P_ODMR)
		    SMTP_quit(ctl->smtp_socket);
		SockClose(ctl->smtp_socket);
		ctl->smtp_socket = -1;
	    }
    }

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

static void terminate_run(int sig)
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

#if !defined(HAVE_ATEXIT) && !defined(HAVE_ON_EXIT)
    lock_release();
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
    int i, st = 0;

    /*
     * If we're syslogging the progress messages are automatically timestamped.
     * Force timestamping if we're going to a logfile.
     */
    if (outlevel >= O_VERBOSE || (run.logfile && outlevel > O_SILENT))
    {
	report(stdout, _("%s querying %s (protocol %s) at %s\n"),
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
	    st = query_host(ctl);
	    if (st == PS_SUCCESS || st == PS_NOMAIL || st == PS_AUTHFAIL || st == PS_LOCKBUSY || st == PS_SMTP || st == PS_MAXFETCH)
		break;
	}
	ctl->server.protocol = P_AUTO;
	return(st);
    case P_POP2:
#ifdef POP2_ENABLE
	return(doPOP2(ctl));
#else
	report(stderr, _("POP2 support is not configured.\n"));
	return(PS_PROTOCOL);
#endif /* POP2_ENABLE */
	break;
    case P_POP3:
    case P_APOP:
    case P_RPOP:
#ifdef POP3_ENABLE
	return(doPOP3(ctl));
#else
	report(stderr, _("POP3 support is not configured.\n"));
	return(PS_PROTOCOL);
#endif /* POP3_ENABLE */
	break;
    case P_IMAP:
#ifdef IMAP_ENABLE
	return(doIMAP(ctl));
#else
	report(stderr, _("IMAP support is not configured.\n"));
	return(PS_PROTOCOL);
#endif /* IMAP_ENABLE */
    case P_ETRN:
#ifndef ETRN_ENABLE
	report(stderr, _("ETRN support is not configured.\n"));
	return(PS_PROTOCOL);
#else
#ifdef HAVE_GETHOSTBYNAME
	return(doETRN(ctl));
#else
	report(stderr, _("Cannot support ETRN without gethostbyname(2).\n"));
	return(PS_PROTOCOL);
#endif /* HAVE_GETHOSTBYNAME */
#endif /* ETRN_ENABLE */
    case P_ODMR:
#ifndef ODMR_ENABLE
	report(stderr, _("ODMR support is not configured.\n"));
	return(PS_PROTOCOL);
#else
#ifdef HAVE_GETHOSTBYNAME
	return(doODMR(ctl));
#else
	report(stderr, _("Cannot support ODMR without gethostbyname(2).\n"));
	return(PS_PROTOCOL);
#endif /* HAVE_GETHOSTBYNAME */
#endif /* ODMR_ENABLE */
    default:
	report(stderr, _("unsupported protocol selected.\n"));
	return(PS_PROTOCOL);
    }
}

static void dump_params (struct runctl *runp,
			 struct query *querylist, flag implicit)
/* display query parameters in English */
{
    struct query *ctl;

    if (runp->poll_interval)
	printf(_("Poll interval is %d seconds\n"), runp->poll_interval);
    if (runp->logfile)
	printf(_("Logfile is %s\n"), runp->logfile);
    if (strcmp(runp->idfile, IDFILE_NAME))
	printf(_("Idfile is %s\n"), runp->idfile);
#if defined(HAVE_SYSLOG)
    if (runp->use_syslog)
	printf(_("Progress messages will be logged via syslog\n"));
#endif
    if (runp->invisible)
	printf(_("Fetchmail will masquerade and will not generate Received\n"));
    if (runp->showdots)
	printf(_("Fetchmail will show progress dots even in logfiles.\n"));
    if (runp->postmaster)
	printf(_("Fetchmail will forward misaddressed multidrop messages to %s.\n"),
	       runp->postmaster);

    if (!runp->bouncemail)
	printf(_("Fetchmail will direct error mail to the postmaster.\n"));
    else if (outlevel >= O_VERBOSE)
	printf(_("Fetchmail will direct error mail to the sender.\n"));

    for (ctl = querylist; ctl; ctl = ctl->next)
    {
	if (!ctl->active || (implicit && ctl->server.skip))
	    continue;

	printf(_("Options for retrieving from %s@%s:\n"),
	       ctl->remotename, visbuf(ctl->server.pollname));

	if (ctl->server.via && MAILBOX_PROTOCOL(ctl))
	    printf(_("  Mail will be retrieved via %s\n"), ctl->server.via);

	if (ctl->server.interval)
	    printf(_("  Poll of this server will occur every %d intervals.\n"),
		   ctl->server.interval);
	if (ctl->server.truename)
	    printf(_("  True name of server is %s.\n"), ctl->server.truename);
	if (ctl->server.skip || outlevel >= O_VERBOSE)
	    printf(_("  This host %s be queried when no host is specified.\n"),
		   ctl->server.skip ? _("will not") : _("will"));
	if (!NO_PASSWORD(ctl))
	{
	    if (!ctl->password)
		printf(_("  Password will be prompted for.\n"));
	    else if (outlevel >= O_VERBOSE)
	    {
		if (ctl->server.protocol == P_APOP)
		    printf(_("  APOP secret = \"%s\".\n"),
			   visbuf(ctl->password));
		else if (ctl->server.protocol == P_RPOP)
		    printf(_("  RPOP id = \"%s\".\n"),
			   visbuf(ctl->password));
		else
		    printf(_("  Password = \"%s\".\n"),
							visbuf(ctl->password));
	    }
	}

	if (ctl->server.protocol == P_POP3 
#if INET6_ENABLE
	    && ctl->server.service && !strcmp(ctl->server.service, KPOP_PORT)
#else /* INET6_ENABLE */
	    && ctl->server.port == KPOP_PORT
#endif /* INET6_ENABLE */
	    && (ctl->server.authenticate == A_KERBEROS_V4 ||
		ctl->server.authenticate == A_KERBEROS_V5))
	    printf(_("  Protocol is KPOP with Kerberos %s authentication"),
		   ctl->server.authenticate == A_KERBEROS_V5 ? "V" : "IV");
	else
	    printf(_("  Protocol is %s"), showproto(ctl->server.protocol));
#if INET6_ENABLE
	if (ctl->server.service)
	    printf(_(" (using service %s)"), ctl->server.service);
	if (ctl->server.netsec)
	    printf(_(" (using network security options %s)"), ctl->server.netsec);
#else /* INET6_ENABLE */
	if (ctl->server.port)
	    printf(_(" (using port %d)"), ctl->server.port);
#endif /* INET6_ENABLE */
	else if (outlevel >= O_VERBOSE)
	    printf(_(" (using default port)"));
	if (ctl->server.uidl && MAILBOX_PROTOCOL(ctl))
	    printf(_(" (forcing UIDL use)"));
	putchar('.');
	putchar('\n');
	switch (ctl->server.authenticate)
	{
	case A_ANY:
	    printf(_("  All available authentication methods will be tried.\n"));
	    break;
	case A_PASSWORD:
	    printf(_("  Password authentication will be forced.\n"));
	    break;
	case A_NTLM:
	    printf(_("  NTLM authentication will be forced.\n"));
	    break;
	case A_OTP:
	    printf(_("  OTP authentication will be forced.\n"));
	    break;
	case A_CRAM_MD5:
	    printf(_("  CRAM-Md5 authentication will be forced.\n"));
	    break;
	case A_GSSAPI:
	    printf(_("  GSSAPI authentication will be forced.\n"));
	    break;
	case A_KERBEROS_V4:
	    printf(_("  Kerberos V4 authentication will be forced.\n"));
	    break;
	case A_KERBEROS_V5:
	    printf(_("  Kerberos V5 authentication will be forced.\n"));
	    break;
	case A_SSH:
	    printf(_("  End-to-end encryption assumed.\n"));
	    break;
	}
	if (ctl->server.principal != (char *) NULL)
	    printf(_("  Mail service principal is: %s\n"), ctl->server.principal);
#ifdef	SSL_ENABLE
	if (ctl->use_ssl)
	    printf(_("  SSL encrypted sessions enabled.\n"));
	if (ctl->sslcertck) {
	    printf(_("  SSL server certificate checking enabled.\n"));
	    if (ctl->sslcertpath != NULL)
		printf(_("  SSL trusted certificate directory: %s\n"), ctl->sslcertpath);
	}
	if (ctl->sslfingerprint != NULL)
		printf(_("  SSL key fingerprint (checked against the server key): %s\n"), ctl->sslfingerprint);
#endif
	if (ctl->server.timeout > 0)
	    printf(_("  Server nonresponse timeout is %d seconds"), ctl->server.timeout);
	if (ctl->server.timeout ==  CLIENT_TIMEOUT)
	    printf(_(" (default).\n"));
	else
	    printf(".\n");

	if (MAILBOX_PROTOCOL(ctl)) 
	{
	    if (!ctl->mailboxes->id)
		printf(_("  Default mailbox selected.\n"));
	    else
	    {
		struct idlist *idp;

		printf(_("  Selected mailboxes are:"));
		for (idp = ctl->mailboxes; idp; idp = idp->next)
		    printf(" %s", idp->id);
		printf("\n");
	    }
	    printf(_("  %s messages will be retrieved (--all %s).\n"),
		   ctl->fetchall ? _("All") : _("Only new"),
		   ctl->fetchall ? "on" : "off");
	    printf(_("  Fetched messages %s be kept on the server (--keep %s).\n"),
		   ctl->keep ? _("will") : _("will not"),
		   ctl->keep ? "on" : "off");
	    printf(_("  Old messages %s be flushed before message retrieval (--flush %s).\n"),
		   ctl->flush ? _("will") : _("will not"),
		   ctl->flush ? "on" : "off");
	    printf(_("  Rewrite of server-local addresses is %s (--norewrite %s).\n"),
		   ctl->rewrite ? _("enabled") : _("disabled"),
		   ctl->rewrite ? "off" : "on");
	    printf(_("  Carriage-return stripping is %s (stripcr %s).\n"),
		   ctl->stripcr ? _("enabled") : _("disabled"),
		   ctl->stripcr ? "on" : "off");
	    printf(_("  Carriage-return forcing is %s (forcecr %s).\n"),
		   ctl->forcecr ? _("enabled") : _("disabled"),
		   ctl->forcecr ? "on" : "off");
	    printf(_("  Interpretation of Content-Transfer-Encoding is %s (pass8bits %s).\n"),
		   ctl->pass8bits ? _("disabled") : _("enabled"),
		   ctl->pass8bits ? "on" : "off");
	    printf(_("  MIME decoding is %s (mimedecode %s).\n"),
		   ctl->mimedecode ? _("enabled") : _("disabled"),
		   ctl->mimedecode ? "on" : "off");
	    printf(_("  Idle after poll is %s (idle %s).\n"),
		   ctl->idle ? _("enabled") : _("disabled"),
		   ctl->idle ? "on" : "off");
	    printf(_("  Nonempty Status lines will be %s (dropstatus %s)\n"),
		   ctl->dropstatus ? _("discarded") : _("kept"),
		   ctl->dropstatus ? "on" : "off");
	    printf(_("  Delivered-To lines will be %s (dropdelivered %s)\n"),
		   ctl->dropdelivered ? _("discarded") : _("kept"),
		   ctl->dropdelivered ? "on" : "off");
	    if (NUM_NONZERO(ctl->limit))
	    {
		if (NUM_NONZERO(ctl->limit))
		    printf(_("  Message size limit is %d octets (--limit %d).\n"), 
			   ctl->limit, ctl->limit);
		else if (outlevel >= O_VERBOSE)
		    printf(_("  No message size limit (--limit 0).\n"));
		if (run.poll_interval > 0)
		    printf(_("  Message size warning interval is %d seconds (--warnings %d).\n"), 
			   ctl->warnings, ctl->warnings);
		else if (outlevel >= O_VERBOSE)
		    printf(_("  Size warnings on every poll (--warnings 0).\n"));
	    }
	    if (NUM_NONZERO(ctl->fetchlimit))
		printf(_("  Received-message limit is %d (--fetchlimit %d).\n"),
		       ctl->fetchlimit, ctl->fetchlimit);
	    else if (outlevel >= O_VERBOSE)
		printf(_("  No received-message limit (--fetchlimit 0).\n"));
	    if (NUM_NONZERO(ctl->batchlimit))
		printf(_("  SMTP message batch limit is %d.\n"), ctl->batchlimit);
	    else if (outlevel >= O_VERBOSE)
		printf(_("  No SMTP message batch limit (--batchlimit 0).\n"));
	    if (MAILBOX_PROTOCOL(ctl))
	    {
		if (NUM_NONZERO(ctl->expunge))
		    printf(_("  Deletion interval between expunges forced to %d (--expunge %d).\n"), ctl->expunge, ctl->expunge);
		else if (outlevel >= O_VERBOSE)
		    printf(_("  No forced expunges (--expunge 0).\n"));
	    }
	}
	else	/* ODMR or ETRN */
	{
	    struct idlist *idp;

	    printf(_("  Domains for which mail will be fetched are:"));
	    for (idp = ctl->domainlist; idp; idp = idp->next)
	    {
		printf(" %s", idp->id);
		if (!idp->val.status.mark)
		    printf(_(" (default)"));
	    }
	    printf("\n");
	}
	if (ctl->bsmtp)
	    printf(_("  Messages will be appended to %s as BSMTP\n"), visbuf(ctl->bsmtp));
	else if (ctl->mda && MAILBOX_PROTOCOL(ctl))
	    printf(_("  Messages will be delivered with \"%s\".\n"), visbuf(ctl->mda));
	else
	{
	    struct idlist *idp;

	    if (ctl->smtphunt)
	    {
		printf(_("  Messages will be %cMTP-forwarded to:"), 
		       ctl->listener);
		for (idp = ctl->smtphunt; idp; idp = idp->next)
		{
		    printf(" %s", idp->id);
		    if (!idp->val.status.mark)
			printf(_(" (default)"));
		}
		printf("\n");
	    }
	    if (ctl->smtpaddress)
		printf(_("  Host part of MAIL FROM line will be %s\n"),
		       ctl->smtpaddress);
	    if (ctl->smtpname)
		printf(_("  Address to be put in RCPT TO lines shipped to SMTP will be %s\n"),
		       ctl->smtpname);
	}
	if (MAILBOX_PROTOCOL(ctl))
	{
		if (ctl->antispam != (struct idlist *)NULL)
		{
		    struct idlist *idp;

		    printf(_("  Recognized listener spam block responses are:"));
		    for (idp = ctl->antispam; idp; idp = idp->next)
			printf(" %d", idp->val.status.num);
		    printf("\n");
		}
		else if (outlevel >= O_VERBOSE)
		    printf(_("  Spam-blocking disabled\n"));
	}
	if (ctl->preconnect)
	    printf(_("  Server connection will be brought up with \"%s\".\n"),
		   visbuf(ctl->preconnect));
	else if (outlevel >= O_VERBOSE)
	    printf(_("  No pre-connection command.\n"));
	if (ctl->postconnect)
	    printf(_("  Server connection will be taken down with \"%s\".\n"),
		   visbuf(ctl->postconnect));
	else if (outlevel >= O_VERBOSE)
	    printf(_("  No post-connection command.\n"));
	if (MAILBOX_PROTOCOL(ctl)) {
		if (!ctl->localnames)
		    printf(_("  No localnames declared for this host.\n"));
		else
		{
		    struct idlist *idp;
		    int count = 0;

		    for (idp = ctl->localnames; idp; idp = idp->next)
			++count;

		    if (count > 1 || ctl->wildcard)
			printf(_("  Multi-drop mode: "));
		    else
			printf(_("  Single-drop mode: "));

		    printf(_("%d local name(s) recognized.\n"), count);
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
			printf(_("  DNS lookup for multidrop addresses is %s.\n"),
			       ctl->server.dns ? _("enabled") : _("disabled"));
			if (ctl->server.dns)
			{
			    printf(_("  Server aliases will be compared with multidrop addresses by "));
	       		    if (ctl->server.checkalias)
				printf(_("IP address.\n"));
			    else
				printf(_("name.\n"));
			}
			if (ctl->server.envelope == STRING_DISABLED)
			    printf(_("  Envelope-address routing is disabled\n"));
			else
			{
			    printf(_("  Envelope header is assumed to be: %s\n"),
				   ctl->server.envelope ? ctl->server.envelope:_("Received"));
			    if (ctl->server.envskip > 1 || outlevel >= O_VERBOSE)
				printf(_("  Number of envelope header to be parsed: %d\n"),
				       ctl->server.envskip);
			    if (ctl->server.qvirtual)
				printf(_("  Prefix %s will be removed from user id\n"),
				       ctl->server.qvirtual);
			    else if (outlevel >= O_VERBOSE) 
				printf(_("  No prefix stripping\n"));
			}

			if (ctl->server.akalist)
			{
			    struct idlist *idp;

			    printf(_("  Predeclared mailserver aliases:"));
			    for (idp = ctl->server.akalist; idp; idp = idp->next)
				printf(" %s", idp->id);
			    putchar('\n');
			}
			if (ctl->server.localdomains)
			{
			    struct idlist *idp;

			    printf(_("  Local domains:"));
			    for (idp = ctl->server.localdomains; idp; idp = idp->next)
				printf(" %s", idp->id);
			    putchar('\n');
			}
		    }
		}
	}
#if defined(linux) || defined(__FreeBSD__)
	if (ctl->server.interface)
	    printf(_("  Connection must be through interface %s.\n"), ctl->server.interface);
	else if (outlevel >= O_VERBOSE)
	    printf(_("  No interface requirement specified.\n"));
	if (ctl->server.monitor)
	    printf(_("  Polling loop will monitor %s.\n"), ctl->server.monitor);
	else if (outlevel >= O_VERBOSE)
	    printf(_("  No monitor interface specified.\n"));
#endif

	if (ctl->server.plugin)
	    printf(_("  Server connections will be made via plugin %s (--plugin %s).\n"), ctl->server.plugin, ctl->server.plugin);
	else if (outlevel >= O_VERBOSE)
	    printf(_("  No plugin command specified.\n"));
	if (ctl->server.plugout)
	    printf(_("  Listener connections will be made via plugout %s (--plugout %s).\n"), ctl->server.plugout, ctl->server.plugout);
	else if (outlevel >= O_VERBOSE)
	    printf(_("  No plugout command specified.\n"));

	if (ctl->server.protocol > P_POP2 && MAILBOX_PROTOCOL(ctl))
	{
	    if (!ctl->oldsaved)
		printf(_("  No UIDs saved from this host.\n"));
	    else
	    {
		struct idlist *idp;
		int count = 0;

		for (idp = ctl->oldsaved; idp; idp = idp->next)
		    ++count;

		printf(_("  %d UIDs saved.\n"), count);
		if (outlevel >= O_VERBOSE)
		    for (idp = ctl->oldsaved; idp; idp = idp->next)
			printf("\t%s\n", idp->id);
	    }
	}

        if (ctl->tracepolls)
            printf(_("  Poll trace information will be added to the Received header.\n"));
        else if (outlevel >= O_VERBOSE)
            printf(_("  No poll trace information will be added to the Received header.\n.\n"));

	if (ctl->properties)
	    printf(_("  Pass-through properties \"%s\".\n"),
		   visbuf(ctl->properties));
    }
}

/* fetchmail.c ends here */
