/*
 * options.c -- command-line option processing
 *
 * Copyright 1998 by Eric S. Raymond.
 * For license terms, see the file COPYING in this directory.
 */

#include "config.h"

#include <stdio.h>
#include <pwd.h>
#include <string.h>
#include <errno.h>
#if defined(STDC_HEADERS)
#include  <stdlib.h>
#include  <limits.h>
#else
#include  <ctype.h>
#endif

#include "getopt.h"
#include "fetchmail.h"
#include "i18n.h"

#define LA_HELP		1
#define LA_VERSION	2 
#define LA_CHECK	3
#define LA_SILENT	4 
#define LA_VERBOSE	5 
#define LA_DAEMON	6
#define LA_NODETACH	7
#define LA_QUIT		8
#define LA_LOGFILE	9
#define LA_INVISIBLE	10
#define LA_SYSLOG	11
#define LA_NOSYSLOG	12
#define LA_RCFILE	13
#define LA_IDFILE	14
#define LA_POSTMASTER	15
#define LA_NOBOUNCE	16
#define LA_PROTOCOL	17
#define LA_UIDL		18
#define LA_PORT		19
#define LA_PREAUTH	20
#define LA_TIMEOUT	21
#define LA_ENVELOPE	22
#define LA_QVIRTUAL     23
#define LA_USERNAME	24
#define LA_ALL          25
#define LA_NOKEEP	26
#define	LA_KEEP		27
#define LA_FLUSH        28
#define LA_NOREWRITE	29
#define LA_LIMIT	30
#define LA_WARNINGS	31
#define LA_FOLDER	32
#define LA_SMTPHOST	33
#define LA_SMTPADDR     34
#define LA_ANTISPAM	35
#define LA_BATCHLIMIT	36
#define LA_FETCHLIMIT	37
#define LA_EXPUNGE	38
#define LA_MDA		39
#define LA_BSMTP	40
#define LA_LMTP		41
#define LA_PLUGIN	42
#define LA_PLUGOUT	43
#define LA_NETSEC	44
#define LA_INTERFACE    45
#define LA_MONITOR      46
#define LA_CONFIGDUMP	47
#define LA_YYDEBUG	48

/* options still left: CDgGhHjJoORwWxXYz */
static const char *shortoptions = 
	"?Vcsvd:NqL:f:i:p:UP:A:t:E:Q:u:akKFnl:r:S:Z:b:B:e:m:T:I:M:yw:";

static const struct option longoptions[] = {
/* this can be const because all flag fields are 0 and will never get set */
  {"help",	no_argument,	   (int *) 0, LA_HELP        },
  {"version",	no_argument,       (int *) 0, LA_VERSION     },
  {"check",	no_argument,	   (int *) 0, LA_CHECK       },
  {"silent",	no_argument,       (int *) 0, LA_SILENT      },
  {"verbose",	no_argument,       (int *) 0, LA_VERBOSE     },
  {"daemon",	required_argument, (int *) 0, LA_DAEMON      },
  {"nodetach",	no_argument,	   (int *) 0, LA_NODETACH    },
  {"quit",	no_argument,	   (int *) 0, LA_QUIT        },
  {"logfile",	required_argument, (int *) 0, LA_LOGFILE     },
  {"invisible",	no_argument,	   (int *) 0, LA_INVISIBLE   },
  {"syslog",	no_argument,	   (int *) 0, LA_SYSLOG      },
  {"nosyslog",	no_argument,	   (int *) 0, LA_NOSYSLOG    },
  {"fetchmailrc",required_argument,(int *) 0, LA_RCFILE      },
  {"idfile",	required_argument, (int *) 0, LA_IDFILE      },
  {"postmaster",required_argument, (int *) 0, LA_POSTMASTER  },
  {"nobounce",  no_argument,       (int *) 0, LA_NOBOUNCE    },

  {"protocol",	required_argument, (int *) 0, LA_PROTOCOL    },
  {"proto",	required_argument, (int *) 0, LA_PROTOCOL    },
  {"uidl",	no_argument,	   (int *) 0, LA_UIDL	     },
  {"port",	required_argument, (int *) 0, LA_PORT        },
  {"preauth",	required_argument, (int *) 0, LA_PREAUTH},
  {"timeout",	required_argument, (int *) 0, LA_TIMEOUT     },
  {"envelope",	required_argument, (int *) 0, LA_ENVELOPE    },
  {"qvirtual",	required_argument, (int *) 0, LA_QVIRTUAL    },

  {"user",	required_argument, (int *) 0, LA_USERNAME    },
  {"username",	required_argument, (int *) 0, LA_USERNAME    },

  {"all",	no_argument,       (int *) 0, LA_ALL         },
  {"nokeep",	no_argument,	   (int *) 0, LA_NOKEEP      },
  {"keep",	no_argument,       (int *) 0, LA_KEEP        },
  {"flush",	no_argument,	   (int *) 0, LA_FLUSH       },
  {"norewrite",	no_argument,	   (int *) 0, LA_NOREWRITE   },
  {"limit",	required_argument, (int *) 0, LA_LIMIT       },
  {"warnings",	required_argument, (int *) 0, LA_WARNINGS    },

  {"folder",	required_argument, (int *) 0, LA_FOLDER	     },
  {"smtphost",	required_argument, (int *) 0, LA_SMTPHOST    },
  {"smtpaddress", required_argument, (int *) 0, LA_SMTPADDR  },
  {"antispam",	required_argument, (int *) 0, LA_ANTISPAM    },
  
  {"batchlimit",required_argument, (int *) 0, LA_BATCHLIMIT  },
  {"fetchlimit",required_argument, (int *) 0, LA_FETCHLIMIT  },
  {"expunge",	required_argument, (int *) 0, LA_EXPUNGE     },
  {"mda",	required_argument, (int *) 0, LA_MDA         },
  {"bsmtp",	required_argument, (int *) 0, LA_BSMTP       },
  {"lmtp",	no_argument,       (int *) 0, LA_LMTP        },

#ifdef INET6
  {"netsec",	required_argument, (int *) 0, LA_NETSEC      },
#endif /* INET6 */

#if (defined(linux) && !INET6) || defined(__FreeBSD__)
  {"interface",	required_argument, (int *) 0, LA_INTERFACE   },
  {"monitor",	required_argument, (int *) 0, LA_MONITOR     },
#endif /* (defined(linux) && !INET6) || defined(__FreeBSD__) */
  {"plugin",	required_argument, (int *) 0, LA_PLUGIN      },
  {"plugout",	required_argument, (int *) 0, LA_PLUGOUT     },

  {"configdump",no_argument,	   (int *) 0, LA_CONFIGDUMP  },

  {"yydebug",	no_argument,	   (int *) 0, LA_YYDEBUG     },

  {(char *) 0,  no_argument,       (int *) 0, 0              }
};

static int xatoi(char *s, int *errflagptr)
/* do safe conversion from string to number */
{
#if defined (STDC_HEADERS) && defined (LONG_MAX) && defined (INT_MAX)
    /* parse and convert numbers, but also check for invalid characters in
     * numbers
     */

    char *endptr;
    long value;

    errno = 0;

    value = strtol(s, &endptr, 0);

    /* any invalid chars in string? */
    if ( (endptr == s) || (*endptr != '\0') ) {
    	(void) fprintf(stderr, _("String '%s' is not a valid number string.\n"), s);
	(*errflagptr)++;
	return 0;
    }

    /* is the range valid? */
    if ( (((value == LONG_MAX) || (value == LONG_MIN)) && (errno == ERANGE)) ||
				(value > INT_MAX) || (value < INT_MIN)) {

    	(void) fprintf(stderr, _("Value of string '%s' is %s than %d.\n"), s,
					(value < 0) ? _("smaller"): _("larger"),
					(value < 0) ? INT_MIN : INT_MAX);
	(*errflagptr)++;
	return 0;
    }

    return (int) value;  /* shut up, I know what I'm doing */
#else
    int	i;
    char *dp;
# if defined (STDC_HEADERS)
    size_t	len;
# else
    int		len;
# endif

    /* We do only base 10 conversions here (atoi)! */

    len = strlen(s);
    /* check for leading white spaces */
    for (i = 0; (i < len) && isspace(s[i]); i++)
    	;

    dp = &s[i];

    /* check for +/- */
    if (i < len && (s[i] == '+' || s[i] == '-'))	i++;

    /* skip over digits */
    for ( /* no init */ ; (i < len) && isdigit(s[i]); i++)
    	;

    /* check for trailing garbage */
    if (i != len) {
    	(void) fprintf(stderr, _("String '%s' is not a valid number string.\n"), s);
    	(*errflagptr)++;
	return 0;
    }

    /* atoi should be safe by now, except for number range over/underflow */
    return atoi(dp);
#endif
}

int parsecmdline (argc, argv, rctl, ctl)
/* parse and validate the command line options */
int argc;		/* argument count */
char **argv;		/* argument strings */
struct runctl *rctl;	/* global run controls to modify */
struct query *ctl;	/* option record to be initialized */
{
    /*
     * return value: if positive, argv index of last parsed option + 1
     * (presumes one or more server names follows).  if zero, the
     * command line switches are such that no server names are
     * required (e.g. --version).  if negative, the command line is
     * has one or more syntax errors.
     */

    int c;
    int ocount = 0;	/* count of destinations specified */
    int errflag = 0;	/* TRUE when a syntax error is detected */
    int helpflag = 0;	/* TRUE when option help was explicitly requested */
    int option_index;
    char *buf, *cp;

    rctl->poll_interval = -1;

    memset(ctl, '\0', sizeof(struct query));    /* start clean */
    ctl->smtp_socket = -1;

    while (!errflag && 
	   (c = getopt_long(argc,argv,shortoptions,
			    longoptions,&option_index)) != -1) {

	switch (c) {
	case 'V':
	case LA_VERSION:
	    versioninfo = TRUE;
	    break;
	case 'c':
	case LA_CHECK:
	    check_only = TRUE;
	    break;
	case 's':
	case LA_SILENT:
	    outlevel = O_SILENT;
	    break;
	case 'v':
	case LA_VERBOSE:
	    if (outlevel == O_VERBOSE)
		outlevel = O_DEBUG;
	    else
		outlevel = O_VERBOSE;
	    break;
	case 'd':
	case LA_DAEMON:
	    rctl->poll_interval = xatoi(optarg, &errflag);
	    break;
	case 'N':
	case LA_NODETACH:
	    nodetach = TRUE;
	    break;
	case 'q':
	case LA_QUIT:
	    quitmode = TRUE;
	    break;
	case 'L':
	case LA_LOGFILE:
	    rctl->logfile = optarg;
	    break;
	case LA_INVISIBLE:
	    rctl->invisible = TRUE;
	    break;
	case 'f':
	case LA_RCFILE:
	    rcfile = (char *) xmalloc(strlen(optarg)+1);
	    strcpy(rcfile,optarg);
	    break;
	case 'i':
	case LA_IDFILE:
	    rctl->idfile = (char *) xmalloc(strlen(optarg)+1);
	    strcpy(rctl->idfile,optarg);
	    break;
	case LA_POSTMASTER:
	    rctl->postmaster = (char *) xmalloc(strlen(optarg)+1);
	    strcpy(rctl->postmaster,optarg);
	    break;
	case LA_NOBOUNCE:
	    run.bouncemail = FALSE;
	    break;
	case 'p':
	case LA_PROTOCOL:
	    /* XXX -- should probably use a table lookup here */
	    if (strcasecmp(optarg,"auto") == 0)
		ctl->server.protocol = P_AUTO;
	    else if (strcasecmp(optarg,"pop2") == 0)
		ctl->server.protocol = P_POP2;
#ifdef SDPS_ENABLE
	    else if (strcasecmp(optarg,"sdps") == 0)
	    {
	        ctl->server.protocol = P_POP3; 
                ctl->server.sdps = TRUE;
	    }
#endif /* SDPS_ENABLE */
	    else if (strcasecmp(optarg,"pop3") == 0)
		ctl->server.protocol = P_POP3;
	    else if (strcasecmp(optarg,"apop") == 0)
		ctl->server.protocol = P_APOP;
	    else if (strcasecmp(optarg,"rpop") == 0)
		ctl->server.protocol = P_RPOP;
	    else if (strcasecmp(optarg,"kpop") == 0)
	    {
		ctl->server.protocol = P_POP3;
#if INET6
		ctl->server.service = KPOP_PORT;
#else /* INET6 */
		ctl->server.port = KPOP_PORT;
#endif /* INET6 */
#ifdef KERBEROS_V5
		ctl->server.preauthenticate =  A_KERBEROS_V5;
#else
		ctl->server.preauthenticate =  A_KERBEROS_V4;
#endif /* KERBEROS_V5 */
	    }
	    else if (strcasecmp(optarg,"imap") == 0)
		ctl->server.protocol = P_IMAP;
#ifdef KERBEROS_V4
	    else if (strcasecmp(optarg,"imap-k4") == 0)
		ctl->server.protocol = P_IMAP_K4;
#endif /* KERBEROS_V4 */
#ifdef GSSAPI
	    else if (strcasecmp(optarg, "imap-gss") == 0)
                ctl->server.protocol = P_IMAP_GSS;
#endif /* GSSAPI */
	    else if (strcasecmp(optarg, "imap-crammd5") == 0)
                ctl->server.protocol = P_IMAP_CRAM_MD5;
	    else if (strcasecmp(optarg, "imap-login") == 0)
                ctl->server.protocol = P_IMAP_LOGIN;
	    else if (strcasecmp(optarg,"etrn") == 0)
		ctl->server.protocol = P_ETRN;
	    else {
		fprintf(stderr,_("Invalid protocol `%s' specified.\n"), optarg);
		errflag++;
	    }
	    break;
	case 'U':
	case LA_UIDL:
	    ctl->server.uidl = FLAG_TRUE;
	    break;
	case 'P':
	case LA_PORT:
#if INET6
	    ctl->server.service = optarg;
#else /* INET6 */
	    ctl->server.port = xatoi(optarg, &errflag);
#endif /* INET6 */
	    break;
	case 'A':
	case LA_PREAUTH:
	    if (strcmp(optarg, "password") == 0)
		ctl->server.preauthenticate = A_PASSWORD;
	    else if (strcmp(optarg, "kerberos") == 0)
#ifdef KERBEROS_V5
		ctl->server.preauthenticate = A_KERBEROS_V5;
#else
		ctl->server.preauthenticate = A_KERBEROS_V4;
#endif /* KERBEROS_V5 */
	    else if (strcmp(optarg, "kerberos_v5") == 0)
		ctl->server.preauthenticate = A_KERBEROS_V5;
	    else if (strcmp(optarg, "kerberos_v4") == 0)
		ctl->server.preauthenticate = A_KERBEROS_V4;
	    else {
		fprintf(stderr,_("Invalid preauthentication `%s' specified.\n"), optarg);
		errflag++;
	    }
	    break;
	case 't':
	case LA_TIMEOUT:
	    ctl->server.timeout = xatoi(optarg, &errflag);
	    if (ctl->server.timeout == 0)
		ctl->server.timeout = -1;
	    break;
	case 'E':
	case LA_ENVELOPE:
	    ctl->server.envelope = xstrdup(optarg);
	    break;
	case 'Q':    
	case LA_QVIRTUAL:
	    ctl->server.qvirtual = xstrdup(optarg);
	    break;

	case 'u':
	case LA_USERNAME:
	    ctl->remotename = xstrdup(optarg);
	    break;
	case 'a':
	case LA_ALL:
	    ctl->fetchall = FLAG_TRUE;
	    break;
	case 'K':
	case LA_NOKEEP:
	    ctl->keep = FLAG_FALSE;
	    break;
	case 'k':
	case LA_KEEP:
	    ctl->keep = FLAG_TRUE;
	    break;
	case 'F':
	case LA_FLUSH:
	    ctl->flush = FLAG_TRUE;
	    break;
	case 'n':
	case LA_NOREWRITE:
	    ctl->rewrite = FLAG_FALSE;
	    break;
	case 'l':
	case LA_LIMIT:
	    c = xatoi(optarg, &errflag);
	    ctl->limit = NUM_VALUE_IN(c);
	    break;
	case 'r':
	case LA_FOLDER:
	    xalloca(buf, char *, strlen(optarg) + 1);
	    strcpy(buf, optarg);
	    cp = strtok(buf, ",");
	    do {
		save_str(&ctl->mailboxes, cp, 0);
	    } while
		((cp = strtok((char *)NULL, ",")));
	    break;
	case 'S':
	case LA_SMTPHOST:
	    xalloca(buf, char *, strlen(optarg) + 1);
	    strcpy(buf, optarg);
	    cp = strtok(buf, ",");
	    do {
		save_str(&ctl->smtphunt, cp, TRUE);
	    } while
		((cp = strtok((char *)NULL, ",")));
	    ocount++;
	    break;
	case 'D':
	case LA_SMTPADDR:
	    ctl->smtpaddress = xstrdup(optarg);
	    break;
	case 'Z':
	case LA_ANTISPAM:
	    xalloca(buf, char *, strlen(optarg) + 1);
	    strcpy(buf, optarg);
	    cp = strtok(buf, ",");
	    do {
		struct idlist	*idp = save_str(&ctl->antispam, NULL, 0);;

		idp->val.status.num = atoi(cp);
	    } while
		((cp = strtok((char *)NULL, ",")));
	    break;
	case 'b':
	case LA_BATCHLIMIT:
	    c = xatoi(optarg, &errflag);
	    ctl->batchlimit = NUM_VALUE_IN(c);
	    break;
	case 'B':
	case LA_FETCHLIMIT:
	    c = xatoi(optarg, &errflag);
	    ctl->fetchlimit = NUM_VALUE_IN(c);
	    break;
	case 'e':
	case LA_EXPUNGE:
	    c = xatoi(optarg, &errflag);
	    ctl->expunge = NUM_VALUE_IN(c);
	    break;
	case 'm':
	case LA_MDA:
	    ctl->mda = xstrdup(optarg);
	    ocount++;
	    break;
	case LA_BSMTP:
	    ctl->bsmtp = xstrdup(optarg);
	    ocount++;
	    break;
	case LA_LMTP:
	    ctl->listener = LMTP_MODE;
	    break;

	case 'T':
	case LA_NETSEC:
#if NET_SECURITY
	    ctl->server.netsec = (void *)optarg;
#else
	    fprintf(stderr, _("fetchmail: network security support is disabled\n"));
	    errflag++;
#endif /* NET_SECURITY */
	    break;

#if (defined(linux) && !INET6) || defined(__FreeBSD__)
	case 'I':
	case LA_INTERFACE:
	    interface_parse(optarg, &ctl->server);
	    break;
	case 'M':
	case LA_MONITOR:
	    ctl->server.monitor = xstrdup(optarg);
	    break;
#endif /* (defined(linux) && !INET6) || defined(__FreeBSD__) */
	case LA_PLUGIN:
	    ctl->server.plugin = xstrdup(optarg);
	    break;
	case LA_PLUGOUT:
	    ctl->server.plugout = xstrdup(optarg);
	    break;

	case 'y':
	case LA_YYDEBUG:
	    yydebug = TRUE;
	    break;

	case 'w':
	case LA_WARNINGS:
	    c = xatoi(optarg, &errflag);
	    ctl->warnings = NUM_VALUE_IN(c);
	    break;

	case LA_CONFIGDUMP:
	    configdump = TRUE;
	    break;

	case LA_SYSLOG:
	    rctl->use_syslog = FLAG_TRUE;
	    break;

	case LA_NOSYSLOG:
	    rctl->use_syslog = FLAG_FALSE;
	    break;

	case '?':
	case LA_HELP:
	default:
	    helpflag++;
	}
    }

    if (errflag || ocount > 1 || helpflag) {
	/* squawk if syntax errors were detected */
#define P(s)	fputs(s, helpflag ? stdout : stderr)
	P(_("usage:  fetchmail [options] [server ...]\n"));
	P(_("  Options are as follows:\n"));
	P(_("  -?, --help        display this option help\n"));
	P(_("  -V, --version     display version info\n"));

	P(_("  -c, --check       check for messages without fetching\n"));
	P(_("  -s, --silent      work silently\n"));
	P(_("  -v, --verbose     work noisily (diagnostic output)\n"));
	P(_("  -d, --daemon      run as a daemon once per n seconds\n"));
	P(_("  -N, --nodetach    don't detach daemon process\n"));
	P(_("  -q, --quit        kill daemon process\n"));
	P(_("  -L, --logfile     specify logfile name\n"));
	P(_("      --syslog      use syslog(3) for most messages when running as a daemon\n"));
	P(_("      --invisible   don't write Received & enable host spoofing\n"));
	P(_("  -f, --fetchmailrc specify alternate run control file\n"));
	P(_("  -i, --idfile      specify alternate UIDs file\n"));
	P(_("      --postmaster  specify recipient of last resort\n"));
	P(_("      --nobounce    redirect bounces from user to postmaster.\n"));
#if (defined(linux) && !INET6) || defined(__FreeBSD__)
	P(_("  -I, --interface   interface required specification\n"));
	P(_("  -M, --monitor     monitor interface for activity\n"));
#endif
	P(_("      --plugin      specify external command to open connection\n"));
	P(_("      --plugout     specify external command to open smtp connection\n"));

	P(_("  -p, --protocol    specify retrieval protocol (see man page)\n"));
	P(_("  -U, --uidl        force the use of UIDLs (pop3 only)\n"));
	P(_("  -P, --port        TCP/IP service port to connect to\n"));
	P(_("  -A, --preauth     preauthentication type (password or kerberos)\n"));
	P(_("  -t, --timeout     server nonresponse timeout\n"));
	P(_("  -E, --envelope    envelope address header\n"));
	P(_("  -Q, --qvirtual    prefix to remove from local user id\n"));

	P(_("  -u, --username    specify users's login on server\n"));
	P(_("  -a, --all         retrieve old and new messages\n"));
	P(_("  -K, --nokeep      delete new messages after retrieval\n"));
	P(_("  -k, --keep        save new messages after retrieval\n"));
	P(_("  -F, --flush       delete old messages from server\n"));
	P(_("  -n, --norewrite   don't rewrite header addresses\n"));
	P(_("  -l, --limit       don't fetch messages over given size\n"));
	P(_("  -w, --warnings    interval between warning mail notification\n"));

#if NET_SECURITY
	P(_("  -T, --netsec      set IP security request\n"));
#endif /* NET_SECURITY */
	P(_("  -S, --smtphost    set SMTP forwarding host\n"));
	P(_("  -D, --smtpaddress set SMTP delivery domain to use\n"));
	P(_("  -Z, --antispam,   set antispam response values\n"));
	P(_("  -b, --batchlimit  set batch limit for SMTP connections\n"));
	P(_("  -B, --fetchlimit  set fetch limit for server connections\n"));
	P(_("  -e, --expunge     set max deletions between expunges\n"));
        P(_("      --mda         set MDA to use for forwarding\n"));
        P(_("      --bsmtp       set output BSMTP file\n"));
        P(_("      --lmtp        use LMTP (RFC2033) for delivery\n"));
	P(_("  -r, --folder      specify remote folder name\n"));
#undef P

	if (helpflag)
	    exit(PS_SUCCESS);
	else
	    exit(PS_SYNTAX);
    }

    return(optind);
}

/* options.c ends here */
