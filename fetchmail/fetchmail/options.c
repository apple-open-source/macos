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
#define LA_AUTH		20
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
#define LA_FETCHDOMAINS	34
#define LA_SMTPADDR     35
#define LA_ANTISPAM	36
#define LA_BATCHLIMIT	37
#define LA_FETCHLIMIT	38
#define LA_EXPUNGE	39
#define LA_MDA		40
#define LA_BSMTP	41
#define LA_LMTP		42
#define LA_PLUGIN	43
#define LA_PLUGOUT	44
#define LA_NETSEC	45
#define LA_INTERFACE    46
#define LA_MONITOR      47
#define LA_CONFIGDUMP	48
#define LA_YYDEBUG	49
#define LA_SMTPNAME     50
#define LA_SHOWDOTS	51
#define LA_PRINCIPAL	52
#define LA_TRACEPOLLS	53

#ifdef SSL_ENABLE
#define LA_SSL		54
#define LA_SSLKEY	55
#define LA_SSLCERT	56
#define LA_SSLPROTO 	57
#define LA_SSLCERTCK	58
#define LA_SSLCERTPATH	59
#define LA_SSLFINGERPRINT	60
#endif


/* options still left: CDgGhHjJoORwWxXYz */
static const char *shortoptions = 
	"?Vcsvd:NqL:f:i:p:UP:A:t:E:Q:u:akKFnl:r:S:Z:b:B:e:m:T:I:M:yw:D:";

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
  {"showdots",	no_argument,	   (int *) 0, LA_SHOWDOTS    },
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
  {"auth",	required_argument, (int *) 0, LA_AUTH},
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
  {"fetchdomains",	required_argument, (int *) 0, LA_FETCHDOMAINS    },
  {"smtpaddress", required_argument, (int *) 0, LA_SMTPADDR  },
  {"smtpname",  required_argument, (int *) 0, LA_SMTPNAME    },
  {"antispam",	required_argument, (int *) 0, LA_ANTISPAM    },
  
  {"batchlimit",required_argument, (int *) 0, LA_BATCHLIMIT  },
  {"fetchlimit",required_argument, (int *) 0, LA_FETCHLIMIT  },
  {"expunge",	required_argument, (int *) 0, LA_EXPUNGE     },
  {"mda",	required_argument, (int *) 0, LA_MDA         },
  {"bsmtp",	required_argument, (int *) 0, LA_BSMTP       },
  {"lmtp",	no_argument,       (int *) 0, LA_LMTP        },

#ifdef INET6_ENABLE
  {"netsec",	required_argument, (int *) 0, LA_NETSEC      },
#endif /* INET6_ENABLE */

#ifdef SSL_ENABLE
  {"ssl",       no_argument,       (int *) 0, LA_SSL        },
  {"sslkey",    required_argument, (int *) 0, LA_SSLKEY     },
  {"sslcert",   required_argument, (int *) 0, LA_SSLCERT    },
  {"sslproto",   required_argument, (int *) 0, LA_SSLPROTO    },
  {"sslcertck", no_argument,       (int *) 0, LA_SSLCERTCK  },
  {"sslcertpath",   required_argument, (int *) 0, LA_SSLCERTPATH },
  {"sslfingerprint",   required_argument, (int *) 0, LA_SSLFINGERPRINT },
#endif

  {"principal", required_argument, (int *) 0, LA_PRINCIPAL },

#if (defined(linux) && !INET6_ENABLE) || defined(__FreeBSD__)
  {"interface",	required_argument, (int *) 0, LA_INTERFACE   },
  {"monitor",	required_argument, (int *) 0, LA_MONITOR     },
#endif /* (defined(linux) && !INET6_ENABLE) || defined(__FreeBSD__) */
  {"plugin",	required_argument, (int *) 0, LA_PLUGIN      },
  {"plugout",	required_argument, (int *) 0, LA_PLUGOUT     },

  {"configdump",no_argument,	   (int *) 0, LA_CONFIGDUMP  },

  {"yydebug",	no_argument,	   (int *) 0, LA_YYDEBUG     },

  {"tracepolls",no_argument,       (int *) 0, LA_TRACEPOLLS  },

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
    	(void) fprintf(stderr, GT_("String '%s' is not a valid number string.\n"), s);
	(*errflagptr)++;
	return 0;
    }

    /* is the range valid? */
    if ( (((value == LONG_MAX) || (value == LONG_MIN)) && (errno == ERANGE)) ||
				(value > INT_MAX) || (value < INT_MIN)) {

    	(void) fprintf(stderr, GT_("Value of string '%s' is %s than %d.\n"), s,
					(value < 0) ? GT_("smaller"): GT_("larger"),
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
    	(void) fprintf(stderr, GT_("String '%s' is not a valid number string.\n"), s);
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
			    longoptions, &option_index)) != -1)
    {
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
	case LA_SHOWDOTS:
	    rctl->showdots = TRUE;
	    break;
	case 'f':
	case LA_RCFILE:
	    rcfile = (char *) xstrdup(optarg);
	    break;
	case 'i':
	case LA_IDFILE:
	    rctl->idfile = (char *) xstrdup(optarg);
	    break;
	case LA_POSTMASTER:
	    rctl->postmaster = (char *) xstrdup(optarg);
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
#if INET6_ENABLE
		ctl->server.service = KPOP_PORT;
#else /* INET6_ENABLE */
		ctl->server.port = KPOP_PORT;
#endif /* INET6_ENABLE */
#ifdef KERBEROS_V5
		ctl->server.authenticate =  A_KERBEROS_V5;
#else
		ctl->server.authenticate =  A_KERBEROS_V4;
#endif /* KERBEROS_V5 */
	    }
	    else if (strcasecmp(optarg,"imap") == 0)
		ctl->server.protocol = P_IMAP;
	    else if (strcasecmp(optarg,"etrn") == 0)
		ctl->server.protocol = P_ETRN;
	    else if (strcasecmp(optarg,"odmr") == 0)
		ctl->server.protocol = P_ODMR;
	    else {
		fprintf(stderr,GT_("Invalid protocol `%s' specified.\n"), optarg);
		errflag++;
	    }
	    break;
	case 'U':
	case LA_UIDL:
	    ctl->server.uidl = FLAG_TRUE;
	    break;
	case 'P':
	case LA_PORT:
#if INET6_ENABLE
	    ctl->server.service = optarg;
#else /* INET6_ENABLE */
	    ctl->server.port = xatoi(optarg, &errflag);
#endif /* INET6_ENABLE */
	    break;
	case LA_AUTH:
	    if (strcmp(optarg, "password") == 0)
		ctl->server.authenticate = A_PASSWORD;
	    else if (strcmp(optarg, "kerberos") == 0)
#ifdef KERBEROS_V5
		ctl->server.authenticate = A_KERBEROS_V5;
#else
		ctl->server.authenticate = A_KERBEROS_V4;
#endif /* KERBEROS_V5 */
	    else if (strcmp(optarg, "kerberos_v5") == 0)
		ctl->server.authenticate = A_KERBEROS_V5;
	    else if (strcmp(optarg, "kerberos_v4") == 0)
		ctl->server.authenticate = A_KERBEROS_V4;
	    else if (strcmp(optarg, "ssh") == 0)
		ctl->server.authenticate = A_SSH;
	    else if (strcmp(optarg, "otp") == 0)
		ctl->server.authenticate = A_OTP;
	    else if (strcmp(optarg, "ntlm") == 0)
		ctl->server.authenticate = A_NTLM;
	    else if (strcmp(optarg, "cram") == 0)
		ctl->server.authenticate = A_CRAM_MD5;
	    else if (strcmp(optarg, "cram-md5") == 0)
		ctl->server.authenticate = A_CRAM_MD5;
	    else if (strcmp(optarg, "gssapi") == 0)
		ctl->server.authenticate = A_GSSAPI;
	    else if (strcmp(optarg, "any") == 0)
		ctl->server.authenticate = A_ANY;
	    else {
		fprintf(stderr,GT_("Invalid authentication `%s' specified.\n"), optarg);
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
	case LA_FETCHDOMAINS:
	    xalloca(buf, char *, strlen(optarg) + 1);
	    strcpy(buf, optarg);
	    cp = strtok(buf, ",");
	    do {
		save_str(&ctl->domainlist, cp, TRUE);
	    } while
		((cp = strtok((char *)NULL, ",")));
	    break;
	case 'D':
	case LA_SMTPADDR:
	    ctl->smtpaddress = xstrdup(optarg);
	    break;
	case LA_SMTPNAME:
	  ctl->smtpname = xstrdup(optarg);
	  break;
	case 'Z':
	case LA_ANTISPAM:
	    xalloca(buf, char *, strlen(optarg) + 1);
	    strcpy(buf, optarg);
	    cp = strtok(buf, ",");
	    do {
		struct idlist	*idp = save_str(&ctl->antispam, NULL, 0);;

		idp->val.status.num = xatoi(cp, &errflag);
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
	    fprintf(stderr, GT_("fetchmail: network security support is disabled\n"));
	    errflag++;
#endif /* NET_SECURITY */
	    break;

#if (defined(linux) && !INET6_ENABLE) || defined(__FreeBSD__)
	case 'I':
	case LA_INTERFACE:
	    interface_parse(optarg, &ctl->server);
	    break;
	case 'M':
	case LA_MONITOR:
	    ctl->server.monitor = xstrdup(optarg);
	    break;
#endif /* (defined(linux) && !INET6_ENABLE) || defined(__FreeBSD__) */
	case LA_PLUGIN:
	    ctl->server.plugin = xstrdup(optarg);
	    break;
	case LA_PLUGOUT:
	    ctl->server.plugout = xstrdup(optarg);
	    break;

#ifdef SSL_ENABLE
	case LA_SSL:
	    ctl->use_ssl = FLAG_TRUE;
	    break;

	case LA_SSLKEY:
	    ctl->sslkey = xstrdup(optarg);
	    break;

	case LA_SSLCERT:
	    ctl->sslcert = xstrdup(optarg);
	    break;

	case LA_SSLPROTO:
	    ctl->sslproto = xstrdup(optarg);
	    break;

	case LA_SSLCERTCK:
	    ctl->sslcertck = FLAG_TRUE;
	    break;

	case LA_SSLCERTPATH:
	    ctl->sslcertpath = xstrdup(optarg);
	    break;

	case LA_SSLFINGERPRINT:
	    ctl->sslfingerprint = xstrdup(optarg);
	    break;
#endif

	case LA_PRINCIPAL:
	    ctl->server.principal = xstrdup(optarg);
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

        case LA_TRACEPOLLS:
            ctl->tracepolls = FLAG_TRUE;
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
	P(GT_("usage:  fetchmail [options] [server ...]\n"));
	P(GT_("  Options are as follows:\n"));
	P(GT_("  -?, --help        display this option help\n"));
	P(GT_("  -V, --version     display version info\n"));

	P(GT_("  -c, --check       check for messages without fetching\n"));
	P(GT_("  -s, --silent      work silently\n"));
	P(GT_("  -v, --verbose     work noisily (diagnostic output)\n"));
	P(GT_("  -d, --daemon      run as a daemon once per n seconds\n"));
	P(GT_("  -N, --nodetach    don't detach daemon process\n"));
	P(GT_("  -q, --quit        kill daemon process\n"));
	P(GT_("  -L, --logfile     specify logfile name\n"));
	P(GT_("      --syslog      use syslog(3) for most messages when running as a daemon\n"));
	P(GT_("      --invisible   don't write Received & enable host spoofing\n"));
	P(GT_("  -f, --fetchmailrc specify alternate run control file\n"));
	P(GT_("  -i, --idfile      specify alternate UIDs file\n"));
	P(GT_("      --postmaster  specify recipient of last resort\n"));
	P(GT_("      --nobounce    redirect bounces from user to postmaster.\n"));
#if (defined(linux) && !INET6_ENABLE) || defined(__FreeBSD__)
	P(GT_("  -I, --interface   interface required specification\n"));
	P(GT_("  -M, --monitor     monitor interface for activity\n"));
#endif
#if defined( SSL_ENABLE )
	P(GT_("      --ssl         enable ssl encrypted session\n"));
	P(GT_("      --sslkey      ssl private key file\n"));
	P(GT_("      --sslcert     ssl client certificate\n"));
	P(GT_("      --sslproto    force ssl protocol (ssl2/ssl3/tls1)\n"));
#endif
	P(GT_("      --plugin      specify external command to open connection\n"));
	P(GT_("      --plugout     specify external command to open smtp connection\n"));

	P(GT_("  -p, --protocol    specify retrieval protocol (see man page)\n"));
	P(GT_("  -U, --uidl        force the use of UIDLs (pop3 only)\n"));
	P(GT_("  -P, --port        TCP/IP service port to connect to\n"));
	P(GT_("      --auth        authentication type (password/kerberos/ssh)\n"));
	P(GT_("  -t, --timeout     server nonresponse timeout\n"));
	P(GT_("  -E, --envelope    envelope address header\n"));
	P(GT_("  -Q, --qvirtual    prefix to remove from local user id\n"));
	P(GT_("      --principal   mail service principal\n"));
        P(GT_("      --tracepolls  add poll-tracing information to Received header\n"));

	P(GT_("  -u, --username    specify users's login on server\n"));
	P(GT_("  -a, --all         retrieve old and new messages\n"));
	P(GT_("  -K, --nokeep      delete new messages after retrieval\n"));
	P(GT_("  -k, --keep        save new messages after retrieval\n"));
	P(GT_("  -F, --flush       delete old messages from server\n"));
	P(GT_("  -n, --norewrite   don't rewrite header addresses\n"));
	P(GT_("  -l, --limit       don't fetch messages over given size\n"));
	P(GT_("  -w, --warnings    interval between warning mail notification\n"));

#if NET_SECURITY
	P(GT_("  -T, --netsec      set IP security request\n"));
#endif /* NET_SECURITY */
	P(GT_("  -S, --smtphost    set SMTP forwarding host\n"));
	P(GT_("      --fetchdomains fetch mail for specified domains\n"));
	P(GT_("  -D, --smtpaddress set SMTP delivery domain to use\n"));
	P(GT_("      --smtpname    set SMTP full name username@domain\n"));
	P(GT_("  -Z, --antispam,   set antispam response values\n"));
	P(GT_("  -b, --batchlimit  set batch limit for SMTP connections\n"));
	P(GT_("  -B, --fetchlimit  set fetch limit for server connections\n"));
	P(GT_("  -e, --expunge     set max deletions between expunges\n"));
        P(GT_("  -m, --mda         set MDA to use for forwarding\n"));
        P(GT_("      --bsmtp       set output BSMTP file\n"));
        P(GT_("      --lmtp        use LMTP (RFC2033) for delivery\n"));
	P(GT_("  -r, --folder      specify remote folder name\n"));
	P(GT_("      --showdots    show progress dots even in logfiles\n"));
#undef P

	if (helpflag)
	    exit(PS_SUCCESS);
	else
	    exit(PS_SYNTAX);
    }

    return(optind);
}

/* options.c ends here */
