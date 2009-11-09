/** \file fetchmail.h  header file for fetchmail */
#ifndef _FETCHMAIL_H
#define _FETCHMAIL_H
/*
 * For license terms, see the file COPYING in this directory.
 */

/* We need this for HAVE_STDARG_H, etc */
#include "config.h"

struct addrinfo;

/* We need this for size_t */
#include <sys/types.h>

/* We need this for time_t */
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NET_SOCKET_H
#include <net/socket.h>
#endif
#include <netdb.h>
#include <stdio.h>

/* Import Trio if needed */
#if !defined(HAVE_SNPRINTF) || !defined(HAVE_VSNPRINTF)
#  include "trio/trio.h"
#endif

/* We need this for strstr */
#if !defined(HAVE_STRSTR) && !defined(strstr)
char *strstr(const char *, const char *);
#endif

/* constants designating the various supported protocols */
#define		P_AUTO		1
#define		P_POP2		2
#define		P_POP3		3
#define		P_APOP		4
#define		P_RPOP		5
#define		P_IMAP		6
#define		P_ETRN		7
#define		P_ODMR		8

#define		SMTP_PORT	"smtp"
#define		SMTP_PORT_NUM	25
#define		KPOP_PORT	"kpop"

#ifdef SSL_ENABLE
#define		SIMAP_PORT	993
#define		SPOP3_PORT	995
#endif

/* 
 * We need to distinguish between mailbox and mailbag protocols.
 * Under a mailbox protocol wwe're pulling mail for a speecific user.
 * Under a mailbag protocol we're fetching mail for an entire domain.
 */
#define MAILBOX_PROTOCOL(ctl)	((ctl)->server.protocol < P_ETRN)

/* authentication types */
#define		A_ANY		0	/* use the first method that works */
#define		A_PASSWORD	1	/* password authentication */
#define		A_NTLM		2	/* Microsoft NTLM protocol */
#define		A_CRAM_MD5	3	/* CRAM-MD5 shrouding (RFC2195) */
#define		A_OTP		4	/* One-time password (RFC1508) */
#define		A_KERBEROS_V4	5	/* authenticate w/ Kerberos V4 */
#define		A_KERBEROS_V5	6	/* authenticate w/ Kerberos V5 */
#define 	A_GSSAPI	7	/* authenticate with GSSAPI */
#define		A_SSH		8	/* authentication at session level */
#define		A_MSN		9	/* same as NTLM with keyword MSN */
#define		A_EXTERNAL	10	/* external authentication (client cert) */

/* some protocols or authentication types (KERBEROS, GSSAPI, SSH) don't
 * require a password */
#define NO_PASSWORD(ctl) \
    ((ctl)->server.authenticate == A_OTP \
     || (ctl)->server.authenticate == A_KERBEROS_V4 \
     || (ctl)->server.authenticate == A_KERBEROS_V5 \
     || (ctl)->server.authenticate == A_GSSAPI \
     || (ctl)->server.authenticate == A_SSH \
     || (ctl)->server.authenticate == A_EXTERNAL \
     || (ctl)->server.protocol == P_ETRN)

/*
 * Definitions for buffer sizes.  We get little help on setting maxima
 * from IMAP RFCs up to 2060, so these are mostly from POP3.
 */
#define		HOSTLEN		635	/* max hostname length (RFC1123) */
#define		POPBUFSIZE	512	/* max length of response (RFC1939) */
#define		IDLEN		128	/* max length of UID (RFC1939) */

/* per RFC1939 this should be 40, but Microsoft Exchange ignores that limit */
#define		USERNAMELEN	128	/* max POP3 arg length */

/* clear a netBSD kernel parameter out of the way */ 
#undef		MSGBUFSIZE

/*
 * The RFC822 limit on message line size is just 998.  But
 * make this *way* oversized; idiot DOS-world mailers that
 * don't line-wrap properly often ship entire paragraphs as
 * lines.
 */
#define		MSGBUFSIZE	8192

#define		NAMELEN		64	/* max username length */
#define		PASSWORDLEN	64	/* max password length */
#define		DIGESTLEN	33	/* length of MD5 digest */

/* exit code values */
/* NOTE THAT PS_SUCCESS MUST ALWAYS BE 0 - SOME PARTS OF THE CODE
 * RELY ON THIS VALUE! */
#define		PS_SUCCESS	0	/* successful receipt of messages */
#define		PS_NOMAIL       1	/* no mail available */
#define		PS_SOCKET	2	/* socket I/O woes */
#define		PS_AUTHFAIL	3	/* user authorization failed */
#define		PS_PROTOCOL	4	/* protocol violation */
#define		PS_SYNTAX	5	/* command-line syntax error */
#define		PS_IOERR	6	/* bad permissions on rc file */
#define		PS_ERROR	7	/* protocol error */
#define		PS_EXCLUDE	8	/* client-side exclusion error */
#define		PS_LOCKBUSY	9	/* server responded lock busy */
#define		PS_SMTP         10      /* SMTP error */
#define		PS_DNS		11	/* fatal DNS error */
#define		PS_BSMTP	12	/* output batch could not be opened */
#define		PS_MAXFETCH	13	/* poll ended by fetch limit */
#define		PS_SERVBUSY	14	/* server is busy */
/* leave space for more codes */
#define		PS_UNDEFINED	23	/* something I hadn't thought of */
#define		PS_TRANSIENT	24	/* transient failure (internal use) */
#define		PS_REFUSED	25	/* mail refused (internal use) */
#define		PS_RETAINED	26	/* message retained (internal use) */
#define		PS_REPOLL	28	/* repoll immediately with changed parameters (internal use) */
#define		PS_IDLETIMEOUT	29	/* timeout on imap IDLE (internal use) */

/* output noise level */
#define         O_SILENT	0	/* mute, max squelch, etc. */
#define		O_NORMAL	1	/* user-friendly */
#define		O_VERBOSE	2	/* chatty */
#define		O_DEBUG		3	/* prolix */
#define		O_MONITOR	O_VERBOSE

#define		SIZETICKER	1024	/* print 1 dot per this many bytes */

/*
 * We #ifdef this and use flag rather than bool
 * to avoid a type clash with curses.h
 */
#ifndef TRUE
#define FALSE	0
#define TRUE	1
#endif /* TRUE */
typedef	char	flag;

/* we need to use zero as a flag-uninitialized value */
#define FLAG_TRUE	2
#define FLAG_FALSE	1

/** run control data */
struct runctl
{
    char	*logfile;	/** where to write log information */
    char	*idfile;	/** where to store UID data */
    char	*pidfile;	/** where to record the PID of daemon mode processes */
    char	*postmaster;
    char	*properties;
    int		poll_interval;	/** poll interval in seconds (daemon mode, 0 == off) */
    flag	bouncemail;
    flag	spambounce;
    flag	softbounce;
    flag	use_syslog;
    flag	invisible;
    flag	showdots;
};

struct idlist
{
    char *id;
    union
    {
	struct
	{
	    int		num;
	    flag	mark;		/* UID-index information */
#define UID_UNSEEN	0		/* hasn't been seen */
#define UID_SEEN	1		/* seen, but not deleted */
#define UID_DELETED	2		/* this message has been marked deleted */
#define UID_EXPUNGED	3		/* this message has been expunged */
        }
	status;
	char *id2;
    } val;
    struct idlist *next;
};

struct query;

struct method		/* describe methods for protocol state machine */
{
    const char *name;		/* protocol name */
    const char *service;	/* service port (unencrypted) */
    const char *sslservice;	/* service port (SSL) */
    flag tagged;		/* if true, generate & expect command tags */
    flag delimited;		/* if true, accept "." message delimiter */
    int (*parse_response)(int, char *);
				/* response_parsing function */
    int (*getauth)(int, struct query *, char *);
				/* authorization fetcher */
    int (*getrange)(int, struct query *, const char *, int *, int *, int *);
				/* get message range to fetch */
    int (*getsizes)(int, int, int *);
				/* get sizes of messages */
    int (*getpartialsizes)(int, int, int, int *);
				/* get sizes of subset of messages */
    int (*is_old)(int, struct query *, int);
				/* check for old message */
    int (*fetch_headers)(int, struct query *, int, int *);
				/* fetch header from a given message */
    int (*fetch_body)(int, struct query *, int, int *);
				/* fetch a given message */
    int (*trail)(int, struct query *, const char *);
				/* eat trailer of a message */
    int (*delete_msg)(int, struct query *, int);
				/* delete method */
    int (*mark_seen)(int, struct query *, int);
				/* mark as seen method */
    int (*end_mailbox_poll)(int, struct query *);
				/* end-of-mailbox processing */
    int (*logout_cmd)(int, struct query *);
				/* logout command */
    flag retry;			/* can getrange poll for new messages? */
};

struct hostdata		/* shared among all user connections to given server */
{
    /* rc file data */
    char *pollname;			/* poll label of host */
    char *via;				/* "true" server name if non-NULL */
    struct idlist *akalist;		/* server name first, then akas */
    struct idlist *localdomains;	/* list of pass-through domains */
    int protocol;			/* protocol type */
    char *service;			/* service name */
    int interval;			/* # cycles to skip between polls */
    int authenticate;			/* authentication mode to try */
    int timeout;			/* inactivity timout in seconds */
    char *envelope;			/* envelope address list header */
    int envskip;			/* skip to numbered envelope header */
    char *qvirtual;			/* prefix removed from local user id */
    flag skip;				/* suppress poll in implicit mode? */
    flag dns;				/* do DNS lookup on multidrop? */
    flag uidl;				/* use RFC1725 UIDLs? */
#ifdef SDPS_ENABLE
    flag sdps;				/* use Demon Internet SDPS *ENV */
#endif /* SDPS_ENABLE */
    flag checkalias;			/* resolve aliases by comparing IPs? */
    flag tracepolls;			/* if TRUE, add poll trace info to Received */
    char *principal;			/* Kerberos principal for mail service */
    char *esmtp_name, *esmtp_password;	/* ESMTP AUTH information */

#if defined(linux) || defined(__FreeBSD__)
#define CAN_MONITOR
#endif

#ifdef CAN_MONITOR
    char *interface;
    char *monitor;
    int  monitor_io;
    struct interface_pair_s *interface_pair;
#endif

    char *plugin,*plugout;

    /* computed for internal use */
    const struct method *base_protocol;	/* relevant protocol method table */
    int poll_count;			/* count of polls so far */
    char *queryname;			/* name to attempt DNS lookup on */
    char *truename;			/* "true name" of server host */
    struct sockaddr *trueaddr;		/* IP address of truename */
    size_t trueaddr_len;		/* size of trueaddr data */
    struct hostdata *lead_server;	/* ptr to lead query for this server */
    int esmtp_options;
    int workarounds;			/* track which workarounds the user was warned about */
};

/*
 * bit flags to set in workarounds after the corresponding warning,
 * which we assume to be server-specific, has been printed,
 * so we don't spam our users in daemon mode.
 */
#define WKA_TOP (1L << 0)		/* Maillennium TOP -> RETR override warning */

struct query
{
    /* mailserver connection controls */
    struct hostdata server;

    /* per-user data */
    struct idlist *localnames;	/* including calling user's name */
    int wildcard;		/* should unmatched names be passed through */
    char *remotename;		/* remote login name to use */
    char *password;		/* remote password to use */
    struct idlist *mailboxes;	/* list of mailboxes to check */

    /* per-forwarding-target data */
    struct idlist *smtphunt;	/* list of SMTP hosts to try forwarding to */
    struct idlist *domainlist;	/* domainlist to fetch from */
    char *smtpaddress;		/* address to force in RCPT TO */ 
    char *smtpname;             /* full RCPT TO name, including domain */
    struct idlist *antispam;	/* list of listener's antispam response */
    char *mda;			/* local MDA to pass mail to */
    char *bsmtp;		/* BSMTP output file */
    char listener;		/* what's the listener's wire protocol? */
#define SMTP_MODE	'S'
#define LMTP_MODE	'L'
    char *preconnect;		/* pre-connection command to execute */
    char *postconnect;		/* post-connection command to execute */

    /* per-user control flags */
    flag keep;			/* if TRUE, leave messages undeleted */
    flag fetchall;		/* if TRUE, fetch all (not just unseen) */
    flag flush;			/* if TRUE, delete messages already seen */
    flag limitflush;		/* if TRUE, delete oversized mails */
    flag rewrite;		/* if TRUE, canonicalize recipient addresses */
    flag stripcr;		/* if TRUE, strip CRs in text */
    flag forcecr;		/* if TRUE, force CRs before LFs in text */
    flag pass8bits;		/* if TRUE, ignore Content-Transfer-Encoding */
    flag dropstatus;		/* if TRUE, drop Status lines in mail */
    flag dropdelivered;         /* if TRUE, drop Delivered-To lines in mail */
    flag mimedecode;		/* if TRUE, decode MIME-armored messages */
    flag idle;			/* if TRUE, idle after each poll */
    int	limit;			/* limit size of retrieved messages */
    int warnings;		/* size warning interval */
    int	fetchlimit;		/* max # msgs to get in single poll */
    int fetchsizelimit;		/* max # msg sizes to get in a request */
    int fastuidl;		/* do binary search for new UIDLs? */
    int fastuidlcount;		/* internal count for frequency of binary search */
    int	batchlimit;		/* max # msgs to pass in single SMTP session */
    int	expunge;		/* max # msgs to pass between expunges */
    flag use_ssl;		/* use SSL encrypted session */
    char *sslkey;		/* optional SSL private key file */
    char *sslcert;		/* optional SSL certificate file */
    char *sslproto;		/** force transport protocol (ssl2|ssl3|ssl23|tls1) - if NULL,
				  use ssl23 for SSL and opportunistic tls1 for non-SSL connections. */
    char *sslcertpath;		/* Trusted certificate directory for checking the server cert */
    flag sslcertck;		/* Strictly check the server cert. */
    char *sslcommonname;	/* CommonName to expect from server */
    char *sslfingerprint;	/* Fingerprint to check against */
    char *properties;		/* passthrough properties for extensions */

    /* internal use -- per-poll state */
    flag active;		/* should we actually poll this server? */
    char *destaddr;		/* destination host for this query */
    int errcount;		/* count transient errors in last pass */
    int authfailcount;		/* count of authorization failures */
    int wehaveauthed;		/* We've managed to logon at least once! */
    int wehavesentauthnote;	/* We've sent an authorization failure note */
    int wedged;			/* wedged by auth failures or timeouts? */
    char *smtphost;		/* actual SMTP host we connected to */
    char smtphostmode;		/* what's the actual SMTP host's wire protocol? */
    int smtp_socket;		/* socket descriptor for SMTP connection */
    unsigned int uid;		/* UID of user to deliver to */
    struct idlist *skipped;	/* messages skipped on the mail server */
    struct idlist *oldsaved, *newsaved;
    struct idlist **oldsavedend;
    char lastdigest[DIGESTLEN];	/* last MD5 hash seen on this connection */
    char *folder;		/* folder currently being polled */

    /* internal use -- per-message state */
    int mimemsg;		/* bitmask indicating MIME body-type */
    char digest[DIGESTLEN];	/* md5 digest buffer */

    /* internal use -- housekeeping */
    struct query *next;		/* next query control block in chain */
};

struct msgblk			/* message header parsed for open_sink() */
{
    char   		*headers;	/* raw message headers */
    struct idlist	*recipients;	/* addressees */
    char		return_path[HOSTLEN + USERNAMELEN + 4]; 
    int			msglen;
    int			reallen;
};


/*
 * Numeric option handling.  Numeric option value of zero actually means
 * it's unspecified.  Value less than zero is zero.  The reason for this
 * screwy encoding is so we can zero out an option block in order to set the
 * numeric flags in it to unspecified.
 */
#define NUM_VALUE_IN(n)		(((n) == 0) ? -1 : (n))
#define NUM_VALUE_OUT(n)	(((n) < 0) ? 0 : (n))
#define NUM_NONZERO(n)		((n) > 0)
#define NUM_ZERO(n)		((n) < 0)
#define NUM_SPECIFIED(n)	((n) != 0)

#define MULTIDROP(ctl)	(ctl->wildcard || \
				((ctl)->localnames && (ctl)->localnames->next))

/*
 * Note: tags are generated with an a%04d format from a 1-origin
 * integer sequence number.  Length 4 permits transaction numbers
 * up to 9999, so we force rollover with % 10000.  There's no special
 * reason for this format other than to look like the exmples in the
 * IMAP RFCs.
 */
#define TAGLEN	6		/* 'a' + 4 digits + NUL */
extern char tag[TAGLEN];
#define TAGMOD	10000

/* list of hosts assembled from run control file and command line */
extern struct query cmd_opts, *querylist;

/* what's returned by envquery */
extern void envquery(int, char **);

/* controls the detail level of status/progress messages written to stderr */
extern int outlevel;    	/* see the O_.* constants above */
extern int yydebug;		/* enable parse debugging */

/* these get computed */
extern int batchcount;		/* count of messages sent in current batch */
extern flag peek_capable;	/* can we read msgs without setting seen? */

/* miscellaneous global controls */
extern struct runctl run;	/* global controls for this run */
extern flag nodetach;		/* if TRUE, don't detach daemon process */
extern flag quitmode;		/* if --quit was set */
extern int  quitind;		/* optind after position of last --quit option */
extern flag check_only;		/* if --check was set */
extern char *rcfile;		/* path name of rc file */
extern int linelimit;		/* limit # lines retrieved per site */
extern flag versioninfo;	/* emit only version info */
extern char *user;		/* name of invoking user */
extern char *home;		/* home directory of invoking user */
extern char *fmhome;		/* fetchmail home directory */
extern int pass;		/* number of re-polling pass */
extern flag configdump;		/* dump control blocks as Python dictionary */
extern char *fetchmailhost;	/* either "localhost" or an FQDN */
extern int suppress_tags;	/* suppress tags in tagged protocols? */
extern char shroud[PASSWORDLEN*2+3];	/* string to shroud in debug output */
#ifdef SDPS_ENABLE
extern char *sdps_envfrom;
extern char *sdps_envto;
#endif /* SDPS_ENABLE */

extern const char *iana_charset;	/* IANA assigned charset name */

/* from ucs/norm_charmap.c */
const char *norm_charmap(const char *name);

/* prototypes for globally callable functions */

/* from /usr/include/sys/cdefs.h */
#if !defined __GNUC__ || __GNUC__ < 2
# define __attribute__(xyz)    /* Ignore. */
#endif

/* error.c: Error reporting */
#if defined(HAVE_STDARG_H)
void report_init(int foreground);
 /** Flush partial message, suppress program name tag for next report printout. */
void report_flush(FILE *fp);
void report (FILE *fp, const char *format, ...)
    __attribute__ ((format (printf, 2, 3)))
    ;
void report_build (FILE *fp, const char *format, ...)
    __attribute__ ((format (printf, 2, 3)))
    ;
void report_complete (FILE *fp, const char *format, ...)
    __attribute__ ((format (printf, 2, 3)))
    ;
void report_at_line (FILE *fp, int, const char *, unsigned int, const char *, ...)
    __attribute__ ((format (printf, 5, 6)))
    ;
#else
void report ();
void report_build ();
void report_complete ();
void report_at_line ();
#endif

/* driver.c -- main driver loop */
void set_timeout(int);
int is_idletimeout(void);
void resetidletimeout(void);
int do_protocol(struct query *, const struct method *);

/* transact.c: transaction support */
void init_transact(const struct method *);
int readheaders(int sock,
		       long fetchlen,
		       long reallen,
		       struct query *ctl,
		       int num,
		       flag *suppress_readbody);
int readbody(int sock, struct query *ctl, flag forward, int len);
#if defined(HAVE_STDARG_H)
void gen_send(int sock, const char *, ... )
    __attribute__ ((format (printf, 2, 3)))
    ;
int gen_recv(int sock, char *buf, int size);
int gen_transact(int sock, const char *, ... )
    __attribute__ ((format (printf, 2, 3)))
    ;
#else
void gen_send();
int gen_recv();
int gen_transact();
#endif
extern struct msgblk msgblk;

/* use these to track what was happening when the nonresponse timer fired */
#define GENERAL_WAIT	0	/* unknown wait type */
#define OPEN_WAIT	1	/* waiting from mailserver open */
#define SERVER_WAIT	2	/* waiting for mailserver response */
#define LISTENER_WAIT	3	/* waiting for listener initialization */
#define FORWARDING_WAIT	4	/* waiting for listener response */
extern int phase;

/* response hooks can use this to identify the query stage */
#define STAGE_GETAUTH	0
#define STAGE_GETRANGE	1
#define STAGE_GETSIZES	2
#define STAGE_FETCH	3
#define STAGE_IDLE	4
#define STAGE_LOGOUT	5
extern int stage;

extern int mytimeout;

/* mark values for name lists */
#define XMIT_ACCEPT	1	/* accepted; matches local domain or name */
#define XMIT_REJECT	2	/* rejected; no match */
#define XMIT_RCPTBAD	3	/* SMTP listener rejected the name */ 

/* idle.c */
int interruptible_idle(int interval);
extern volatile int lastsig;

/* sink.c: forwarding */
void smtp_close(struct query *, int);
int smtp_open(struct query *);
char *rcpt_address(struct query *, const char *, int);
int stuffline(struct query *, char *);
int open_sink(struct query*, struct msgblk *, int*, int*);
void release_sink(struct query *);
int close_sink(struct query *, struct msgblk *, flag);
int open_warning_by_mail(struct query *);
#if defined(HAVE_STDARG_H)
void stuff_warning(const char *, struct query *, const char *, ... )
    __attribute__ ((format (printf, 3, 4)))
    ;
#else
void stuff_warning();
#endif
void close_warning_by_mail(struct query *, struct msgblk *);

/* rfc822.c: RFC822 header parsing */
char *reply_hack(char *, const char *, size_t *);
char *nxtaddr(const char *);

/* uid.c: UID support */
extern int dofastuidl;

void initialize_saved_lists(struct query *, const char *);
struct idlist *save_str(struct idlist **, const char *, flag);
void free_str_list(struct idlist **);
struct idlist *copy_str_list(struct idlist *idl);
void save_str_pair(struct idlist **, const char *, const char *);
void free_str_pair_list(struct idlist **);
int delete_str(struct idlist **, long);
struct idlist *str_in_list(struct idlist **, const char *, const flag);
int str_nr_in_list(struct idlist **, const char *);
int str_nr_last_in_list(struct idlist **, const char *);
void str_set_mark( struct idlist **, const char *, const flag);
int count_list( struct idlist **idl );
char *str_from_nr_list( struct idlist **idl, long number );
char *str_find(struct idlist **, long);
struct idlist *id_find(struct idlist **idl, long);
char *idpair_find(struct idlist **, const char *);
void append_str_list(struct idlist **, struct idlist **);
void expunge_uids(struct query *);
void uid_swap_lists(struct query *);
void uid_discard_new_list(struct query *ctl);
void uid_reset_num(struct query *ctl);
void write_saved_lists(struct query *, const char *);

/* rcfile_y.y */
int prc_parse_file(const char *, const flag);
int prc_filecheck(const char *, const flag);

/* base64.c */
void to64frombits(char *, const void *, int);
int from64tobits(void *, const char *, int maxlen);

/* unmime.c */
/* Bit-mask returned by MimeBodyType */
#define MSG_IS_7BIT       0x01
#define MSG_IS_8BIT       0x02
#define MSG_NEEDS_DECODE  0x80
extern void UnMimeHeader(char *buf);
extern int  MimeBodyType(char *hdrs, int WantDecode);
extern int  UnMimeBodyline(char **buf, flag delimited, flag issoftline);

/* interface.c */
void interface_init(void);
void interface_parse(char *, struct hostdata *);
void interface_note_activity(struct hostdata *);
int interface_approve(struct hostdata *, flag domonitor);

#include "xmalloc.h"

/* protocol driver and methods */
int doPOP2 (struct query *); 
int doPOP3 (struct query *);
int doIMAP (struct query *);
int doETRN (struct query *);
int doODMR (struct query *);

/* authentication functions */
int do_cram_md5(int sock, char *command, struct query *ctl, char *strip);
int do_rfc1731(int sock, char *command, char *truename);
int do_gssauth(int sock, char *command, char *service, char *hostname, char *username);
int do_otp(int sock, char *command, struct query *ctl);

/* miscellanea */

/* these should be of size PATH_MAX */
extern char currentwd[1024], rcfiledir[1024];

struct query *hostalloc(struct query *); 
int parsecmdline (int, char **, struct runctl *, struct query *);
char *prependdir (const char *, const char *);
char *MD5Digest (unsigned const char *);
void hmac_md5 (char *, size_t, char *, size_t, unsigned char *, size_t);
int POP3_auth_rpa(char *, char *, int socket);
typedef RETSIGTYPE (*SIGHANDLERTYPE) (int);
void deal_with_sigchld(void);
RETSIGTYPE null_signal_handler(int sig);
SIGHANDLERTYPE set_signal_handler(int sig, SIGHANDLERTYPE handler);
int daemonize(const char *);
char *fm_getpassword(char *);
void escapes(const char *, char *);
char *visbuf(const char *);
const char *showproto(int);
void dump_config(struct runctl *runp, struct query *querylist);
int is_host_alias(const char *, struct query *, struct addrinfo **);

extern struct addrinfo *ai0, *ai1;

/** Try to obtain fully qualified hostname of current host. Exit with
 * PS_DNS if \a required is true and there is a DNS error. Exit with
 * PS_DNS if gethostname() fails, independent of the value of \a
 * required.
 * \return
 * - fully qualified hostname if \a required is non-zero.
 * - unqualified or fully qualified hostname if \a required is zero (0).
 */
char *host_fqdn(int required /** exit with PS_DNS if the name cannot be qualified */);
char *rfc822timestamp(void);
flag is_a_file(int);
char *rfc2047e(const char*, const char *);

void yyerror(const char *);
int yylex(void);

#ifdef __EMX__
void itimerthread(void*);
/* Have to include these first to avoid errors from redefining getcwd
   and chdir.  They're re-include protected in EMX, so it's okay, I
   guess.  */
#include <stdlib.h>
#include <unistd.h>
/* Redefine getcwd and chdir to get drive-letter support so we can
   find all of our lock files and stuff. */
#define getcwd _getcwd2
#define chdir _chdir2
#endif /* _EMX_ */

#ifdef HAVE_STRERROR
#  if !defined(strerror) && !defined(HAVE_DECL_STRERROR)	/* On some systems, strerror is a macro */
char *strerror (int);
#  endif
#endif /* HAVE_STRERROR */

#define STRING_DISABLED	(char *)-1
#define STRING_DUMMY	""

#ifdef NeXT
#ifndef S_IXGRP
#define S_IXGRP 0000010
#endif
#endif

#ifndef HAVE_STPCPY
char *stpcpy(char *, const char*);
#endif

#ifdef __CYGWIN__
#define ROOT_UID 18
#else /* !__CYGWIN__ */
#define ROOT_UID 0
#endif /* __CYGWIN__ */

extern int mailserver_socket_temp;
extern char *program_name;

/* POSIX space characters,
 * <tab>;<newline>;<vertical-tab>;<form-feed>;<carriage-return>;<space> */
#define POSIX_space "\t\n\v\f\r "

/* strlcpy/strlcat prototypes */
#ifndef HAVE_STRLCAT
size_t
strlcat(char *dst, const char *src, size_t siz);
#endif
#ifndef HAVE_STRLCPY
size_t
strlcpy(char *dst, const char *src, size_t siz);
#endif

/** Resolve the a TCP service name or a string containing only a decimal
 * positive integer to a port number. Returns -1 for error. */
int servport(const char *service);

#ifndef HAVE_GETNAMEINFO
# define NI_NUMERICHOST	1
# define NI_NUMERICSERV	2
# define NI_NOFQDN	4
# define NI_NAMEREQD	8
# define NI_DGRAM	16
#endif

int fm_getaddrinfo(const char *node, const char *serv, const struct addrinfo *hints, struct addrinfo **res);
void fm_freeaddrinfo(struct addrinfo *ai);

/* prototypes from tls.c */
int maybe_tls(struct query *ctl);
int must_tls(struct query *ctl);

/* prototype from rfc822valid.c */
int rfc822_valid_msgid(const unsigned char *);

/* macro to determine if we want to spam progress to stdout */
#define want_progress() \
	((outlevel >= O_VERBOSE || (outlevel > O_SILENT && run.showdots)) \
	&& !run.use_syslog \
	&& (run.showdots || !is_a_file(1)))

#endif
/* fetchmail.h ends here */
