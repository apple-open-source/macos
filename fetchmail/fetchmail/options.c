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

enum {
    LA_INVISIBLE = 256,
    LA_PIDFILE,
    LA_SYSLOG,
    LA_NOSYSLOG,
    LA_POSTMASTER,
    LA_NOBOUNCE,
    LA_AUTH,
    LA_FETCHDOMAINS,
    LA_BSMTP,
    LA_LMTP,
    LA_PLUGIN,
    LA_PLUGOUT,
    LA_CONFIGDUMP,
    LA_SMTPNAME,
    LA_SHOWDOTS,
    LA_PRINCIPAL,
    LA_TRACEPOLLS,
    LA_SSL,
    LA_SSLKEY,
    LA_SSLCERT,
    LA_SSLPROTO,
    LA_SSLCERTCK,
    LA_SSLCERTPATH,
    LA_SSLCOMMONNAME,
    LA_SSLFINGERPRINT,
    LA_FETCHSIZELIMIT,
    LA_FASTUIDL,
    LA_LIMITFLUSH,
    LA_IDLE,
    LA_NOSOFTBOUNCE,
    LA_SOFTBOUNCE
};

/* options still left: CgGhHjJoORTWxXYz */
static const char *shortoptions = 
	"?Vcsvd:NqL:f:i:p:UP:A:t:E:Q:u:akKFnl:r:S:Z:b:B:e:m:I:M:yw:D:";

static const struct option longoptions[] = {
/* this can be const because all flag fields are 0 and will never get set */
  {"help",	no_argument,	   (int *) 0, '?' },
  {"version",	no_argument,	   (int *) 0, 'V' },
  {"check",	no_argument,	   (int *) 0, 'c' },
  {"silent",	no_argument,	   (int *) 0, 's' },
  {"verbose",	no_argument,	   (int *) 0, 'v' },
  {"daemon",	required_argument, (int *) 0, 'd' },
  {"nodetach",	no_argument,	   (int *) 0, 'N' },
  {"quit",	no_argument,	   (int *) 0, 'q' },
  {"logfile",	required_argument, (int *) 0, 'L' },
  {"invisible",	no_argument,	   (int *) 0, LA_INVISIBLE },
  {"showdots",	no_argument,	   (int *) 0, LA_SHOWDOTS },
  {"syslog",	no_argument,	   (int *) 0, LA_SYSLOG },
  {"nosyslog",	no_argument,	   (int *) 0, LA_NOSYSLOG },
  {"fetchmailrc",required_argument,(int *) 0, 'f' },
  {"idfile",	required_argument, (int *) 0, 'i' },
  {"pidfile",	required_argument, (int *) 0, LA_PIDFILE },
  {"postmaster",required_argument, (int *) 0, LA_POSTMASTER },
  {"nobounce",	no_argument,	   (int *) 0, LA_NOBOUNCE },
  {"nosoftbounce", no_argument,	   (int *) 0, LA_NOSOFTBOUNCE },
  {"softbounce", no_argument,	   (int *) 0, LA_SOFTBOUNCE },

  {"protocol",	required_argument, (int *) 0, 'p' },
  {"proto",	required_argument, (int *) 0, 'p' },
  {"uidl",	no_argument,	   (int *) 0, 'U' },
  {"idle",	no_argument,	   (int *) 0, LA_IDLE},
  {"port",	required_argument, (int *) 0, 'P' },
  {"service",	required_argument, (int *) 0, 'P' },
  {"auth",	required_argument, (int *) 0, LA_AUTH},
  {"timeout",	required_argument, (int *) 0, 't' },
  {"envelope",	required_argument, (int *) 0, 'E' },
  {"qvirtual",	required_argument, (int *) 0, 'Q' },

  {"user",	required_argument, (int *) 0, 'u' },
  {"username",	required_argument, (int *) 0, 'u' },

  {"all",	no_argument,	   (int *) 0, 'a' },
  {"fetchall",	no_argument,	   (int *) 0, 'a' },
  {"nokeep",	no_argument,	   (int *) 0, 'K' },
  {"keep",	no_argument,	   (int *) 0, 'k' },
  {"flush",	no_argument,	   (int *) 0, 'F' },
  {"limitflush",	no_argument, (int *) 0, LA_LIMITFLUSH },
  {"norewrite",	no_argument,	   (int *) 0, 'n' },
  {"limit",	required_argument, (int *) 0, 'l' },
  {"warnings",	required_argument, (int *) 0, 'w' },

  {"folder",	required_argument, (int *) 0, 'r' },
  {"smtphost",	required_argument, (int *) 0, 'S' },
  {"fetchdomains",	required_argument, (int *) 0, LA_FETCHDOMAINS },
  {"smtpaddress", required_argument, (int *) 0, 'D' },
  {"smtpname",	required_argument, (int *) 0, LA_SMTPNAME },
  {"antispam",	required_argument, (int *) 0, 'Z' },

  {"batchlimit",required_argument, (int *) 0, 'b' },
  {"fetchlimit",required_argument, (int *) 0, 'B' },
  {"fetchsizelimit",required_argument, (int *) 0, LA_FETCHSIZELIMIT },
  {"fastuidl",	required_argument, (int *) 0, LA_FASTUIDL },
  {"expunge",	required_argument, (int *) 0, 'e' },
  {"mda",	required_argument, (int *) 0, 'm' },
  {"bsmtp",	required_argument, (int *) 0, LA_BSMTP },
  {"lmtp",	no_argument,	   (int *) 0, LA_LMTP },

#ifdef SSL_ENABLE
  {"ssl",	no_argument,	   (int *) 0, LA_SSL },
  {"sslkey",	required_argument, (int *) 0, LA_SSLKEY },
  {"sslcert",	required_argument, (int *) 0, LA_SSLCERT },
  {"sslproto",	 required_argument, (int *) 0, LA_SSLPROTO },
  {"sslcertck", no_argument,	   (int *) 0, LA_SSLCERTCK },
  {"sslcertpath",   required_argument, (int *) 0, LA_SSLCERTPATH },
  {"sslcommonname",    required_argument, (int *) 0, LA_SSLCOMMONNAME },
  {"sslfingerprint",   required_argument, (int *) 0, LA_SSLFINGERPRINT },
#endif

  {"principal", required_argument, (int *) 0, LA_PRINCIPAL },

#ifdef CAN_MONITOR
  {"interface",	required_argument, (int *) 0, 'I' },
  {"monitor",	required_argument, (int *) 0, 'M' },
#endif /* CAN_MONITOR */
  {"plugin",	required_argument, (int *) 0, LA_PLUGIN },
  {"plugout",	required_argument, (int *) 0, LA_PLUGOUT },

  {"configdump",no_argument,	   (int *) 0, LA_CONFIGDUMP },

  {"yydebug",	no_argument,	   (int *) 0, 'y' },

  {"tracepolls",no_argument,	   (int *) 0, LA_TRACEPOLLS },

  {(char *) 0,	no_argument,	   (int *) 0, 0 }
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
    for (i = 0; (i < len) && isspace((unsigned char)s[i]); i++)
    	;

    dp = &s[i];

    /* check for +/- */
    if (i < len && (s[i] == '+' || s[i] == '-'))	i++;

    /* skip over digits */
    for ( /* no init */ ; (i < len) && isdigit((unsigned char)s[i]); i++)
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

/** parse and validate the command line options */
int parsecmdline (int argc /** argument count */,
		  char **argv /** argument strings */,
		  struct runctl *rctl /** global run controls to modify */,
		  struct query *ctl /** option record to initialize */)
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
	    versioninfo = TRUE;
	    break;
	case 'c':
	    check_only = TRUE;
	    break;
	case 's':
	    outlevel = O_SILENT;
	    break;
	case 'v':
	    if (outlevel >= O_VERBOSE)
		outlevel = O_DEBUG;
	    else
		outlevel = O_VERBOSE;
	    break;
	case 'd':
	    rctl->poll_interval = xatoi(optarg, &errflag);
	    break;
	case 'N':
	    nodetach = TRUE;
	    break;
	case 'q':
	    quitmode = TRUE;
	    quitind = optind;
	    break;
	case 'L':
	    rctl->logfile = prependdir (optarg, currentwd);
	    break;
	case LA_INVISIBLE:
	    rctl->invisible = TRUE;
	    break;
	case LA_SHOWDOTS:
	    rctl->showdots = FLAG_TRUE;
	    break;
	case 'f':
	    xfree(rcfile);
	    rcfile = prependdir (optarg, currentwd);
	    break;
	case 'i':
	    rctl->idfile = prependdir (optarg, currentwd);
	    break;
	case LA_PIDFILE:
	    rctl->pidfile = prependdir (optarg, currentwd);
	    break;
	case LA_POSTMASTER:
	    rctl->postmaster = (char *) xstrdup(optarg);
	    break;
	case LA_NOBOUNCE:
	    run.bouncemail = FALSE;
	    break;
	case LA_NOSOFTBOUNCE:
	    run.softbounce = FALSE;
	    break;
	case LA_SOFTBOUNCE:
	    run.softbounce = TRUE;
	    break;
	case 'p':
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
		ctl->server.service = KPOP_PORT;
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
	    ctl->server.uidl = FLAG_TRUE;
	    break;
	case LA_IDLE:
	    ctl->idle = FLAG_TRUE;
	    break;
	case 'P':
	    ctl->server.service = optarg;
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
	    else if (strcasecmp(optarg, "external") == 0)
		ctl->server.authenticate = A_EXTERNAL;
	    else if (strcmp(optarg, "otp") == 0)
		ctl->server.authenticate = A_OTP;
	    else if (strcmp(optarg, "opie") == 0)
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
	    else if (strcmp(optarg, "msn") == 0)
		ctl->server.authenticate = A_MSN;
	    else {
		fprintf(stderr,GT_("Invalid authentication `%s' specified.\n"), optarg);
		errflag++;
	    }
	    break;
	case 't':
	    ctl->server.timeout = xatoi(optarg, &errflag);
	    if (ctl->server.timeout == 0)
		ctl->server.timeout = -1;
	    break;
	case 'E':
	    ctl->server.envelope = xstrdup(optarg);
	    break;
	case 'Q':    
	    ctl->server.qvirtual = xstrdup(optarg);
	    break;

	case 'u':
	    ctl->remotename = xstrdup(optarg);
	    break;
	case 'a':
	    ctl->fetchall = FLAG_TRUE;
	    break;
	case 'K':
	    ctl->keep = FLAG_FALSE;
	    break;
	case 'k':
	    ctl->keep = FLAG_TRUE;
	    break;
	case 'F':
	    ctl->flush = FLAG_TRUE;
	    break;
	case LA_LIMITFLUSH:
	    ctl->limitflush = FLAG_TRUE;
	    break;
	case 'n':
	    ctl->rewrite = FLAG_FALSE;
	    break;
	case 'l':
	    c = xatoi(optarg, &errflag);
	    ctl->limit = NUM_VALUE_IN(c);
	    break;
	case 'r':
	    buf = xstrdup(optarg);
	    cp = strtok(buf, ",");
	    do {
		save_str(&ctl->mailboxes, cp, 0);
	    } while
		((cp = strtok((char *)NULL, ",")));
	    free(buf);
	    break;
	case 'S':
	    buf = xstrdup(optarg);
	    cp = strtok(buf, ",");
	    do {
		save_str(&ctl->smtphunt, cp, TRUE);
	    } while
		((cp = strtok((char *)NULL, ",")));
	    free(buf);
	    ocount++;
	    break;
	case LA_FETCHDOMAINS:
	    buf = xstrdup(optarg);
	    cp = strtok(buf, ",");
	    do {
		save_str(&ctl->domainlist, cp, TRUE);
	    } while
		((cp = strtok((char *)NULL, ",")));
	    free(buf);
	    break;
	case 'D':
	    ctl->smtpaddress = xstrdup(optarg);
	    break;
	case LA_SMTPNAME:
	  ctl->smtpname = xstrdup(optarg);
	  break;
	case 'Z':
	    buf = xstrdup(optarg);
	    cp = strtok(buf, ",");
	    do {
		struct idlist	*idp = save_str(&ctl->antispam, NULL, 0);;

		idp->val.status.num = xatoi(cp, &errflag);
	    } while
		((cp = strtok((char *)NULL, ",")));
	    free(buf);
	    break;
	case 'b':
	    c = xatoi(optarg, &errflag);
	    ctl->batchlimit = NUM_VALUE_IN(c);
	    break;
	case 'B':
	    c = xatoi(optarg, &errflag);
	    ctl->fetchlimit = NUM_VALUE_IN(c);
	    break;
	case LA_FETCHSIZELIMIT:
	    c = xatoi(optarg, &errflag);
	    ctl->fetchsizelimit = NUM_VALUE_IN(c);
	    break;
	case LA_FASTUIDL:
	    c = xatoi(optarg, &errflag);
	    ctl->fastuidl = NUM_VALUE_IN(c);
	    break;
	case 'e':
	    c = xatoi(optarg, &errflag);
	    ctl->expunge = NUM_VALUE_IN(c);
	    break;
	case 'm':
	    ctl->mda = xstrdup(optarg);
	    ocount++;
	    break;
	case LA_BSMTP:
	    ctl->bsmtp = prependdir (optarg, currentwd);
	    ocount++;
	    break;
	case LA_LMTP:
	    ctl->listener = LMTP_MODE;
	    break;

#ifdef CAN_MONITOR
	case 'I':
	    interface_parse(optarg, &ctl->server);
	    break;
	case 'M':
	    ctl->server.monitor = xstrdup(optarg);
	    break;
#endif /* CAN_MONITOR */
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
	    ctl->sslkey = prependdir (optarg, currentwd);
	    break;

	case LA_SSLCERT:
	    ctl->sslcert = prependdir (optarg, currentwd);
	    break;

	case LA_SSLPROTO:
	    ctl->sslproto = xstrdup(optarg);
	    break;

	case LA_SSLCERTCK:
	    ctl->sslcertck = FLAG_TRUE;
	    break;

	case LA_SSLCERTPATH:
	    ctl->sslcertpath = prependdir(optarg, currentwd);
	    break;

	case LA_SSLCOMMONNAME:
	    ctl->sslcommonname = xstrdup(optarg);
	    break;

	case LA_SSLFINGERPRINT:
	    ctl->sslfingerprint = xstrdup(optarg);
	    break;
#endif

	case LA_PRINCIPAL:
	    ctl->server.principal = xstrdup(optarg);
	    break;

	case 'y':
	    yydebug = TRUE;
	    break;

	case 'w':
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
	    ctl->server.tracepolls = FLAG_TRUE;
	    break;

	case '?':
	default:
	    helpflag++;
	}
    }

    if (errflag || ocount > 1 || helpflag) {
	/* squawk if syntax errors were detected */
#define P(s)    fputs(s, helpflag ? stdout : stderr)
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
	P(GT_("      --pidfile     specify alternate PID (lock) file\n"));
	P(GT_("      --postmaster  specify recipient of last resort\n"));
	P(GT_("      --nobounce    redirect bounces from user to postmaster.\n"));
	P(GT_("      --nosoftbounce fetchmail deletes permanently undeliverable messages.\n"));
	P(GT_("      --softbounce  keep permanently undeliverable messages on server (default).\n"));
#ifdef CAN_MONITOR
	P(GT_("  -I, --interface   interface required specification\n"));
	P(GT_("  -M, --monitor     monitor interface for activity\n"));
#endif
#if defined( SSL_ENABLE )
	P(GT_("      --ssl         enable ssl encrypted session\n"));
	P(GT_("      --sslkey      ssl private key file\n"));
	P(GT_("      --sslcert     ssl client certificate\n"));
	P(GT_("      --sslcertck   do strict server certificate check (recommended)\n"));
	P(GT_("      --sslcertpath path to ssl certificates\n"));
	P(GT_("      --sslcommonname  expect this CommonName from server (discouraged)\n"));
	P(GT_("      --sslfingerprint fingerprint that must match that of the server's cert.\n"));
	P(GT_("      --sslproto    force ssl protocol (SSL2/SSL3/TLS1)\n"));
#endif
	P(GT_("      --plugin      specify external command to open connection\n"));
	P(GT_("      --plugout     specify external command to open smtp connection\n"));

	P(GT_("  -p, --protocol    specify retrieval protocol (see man page)\n"));
	P(GT_("  -U, --uidl        force the use of UIDLs (pop3 only)\n"));
	P(GT_("      --port        TCP port to connect to (obsolete, use --service)\n"));
	P(GT_("  -P, --service     TCP service to connect to (can be numeric TCP port)\n"));
	P(GT_("      --auth        authentication type (password/kerberos/ssh/otp)\n"));
	P(GT_("  -t, --timeout     server nonresponse timeout\n"));
	P(GT_("  -E, --envelope    envelope address header\n"));
	P(GT_("  -Q, --qvirtual    prefix to remove from local user id\n"));
	P(GT_("      --principal   mail service principal\n"));
	P(GT_("      --tracepolls  add poll-tracing information to Received header\n"));

	P(GT_("  -u, --username    specify users's login on server\n"));
	P(GT_("  -a, --[fetch]all  retrieve old and new messages\n"));
	P(GT_("  -K, --nokeep      delete new messages after retrieval\n"));
	P(GT_("  -k, --keep        save new messages after retrieval\n"));
	P(GT_("  -F, --flush       delete old messages from server\n"));
	P(GT_("      --limitflush  delete oversized messages\n"));
	P(GT_("  -n, --norewrite   don't rewrite header addresses\n"));
	P(GT_("  -l, --limit       don't fetch messages over given size\n"));
	P(GT_("  -w, --warnings    interval between warning mail notification\n"));

	P(GT_("  -S, --smtphost    set SMTP forwarding host\n"));
	P(GT_("      --fetchdomains fetch mail for specified domains\n"));
	P(GT_("  -D, --smtpaddress set SMTP delivery domain to use\n"));
	P(GT_("      --smtpname    set SMTP full name username@domain\n"));
	P(GT_("  -Z, --antispam,   set antispam response values\n"));
	P(GT_("  -b, --batchlimit  set batch limit for SMTP connections\n"));
	P(GT_("  -B, --fetchlimit  set fetch limit for server connections\n"));
	P(GT_("      --fetchsizelimit set fetch message size limit\n"));
	P(GT_("      --fastuidl    do a binary search for UIDLs\n"));
	P(GT_("  -e, --expunge     set max deletions between expunges\n"));
	P(GT_("  -m, --mda         set MDA to use for forwarding\n"));
	P(GT_("      --bsmtp       set output BSMTP file\n"));
	P(GT_("      --lmtp        use LMTP (RFC2033) for delivery\n"));
	P(GT_("  -r, --folder      specify remote folder name\n"));
	P(GT_("      --showdots    show progress dots even in logfiles\n"));
#undef P
	/* undocumented:
	 * --configdump (internal use by fetchmailconf, dumps
	 *               configuration as Python source code)
	 * --yydebug    (developer use, enables parser debugging) */

	if (helpflag)
	    exit(PS_SUCCESS);
	else
	    exit(PS_SYNTAX);
    }

    return(optind);
}

/* options.c ends here */
