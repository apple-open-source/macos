/*
 * For license terms, see the file COPYING in this directory.
 */

/* We need this for HAVE_STDARG_H, etc */
#include "config.h"

/* constants designating the various supported protocols */
#define		P_AUTO		1
#define		P_POP2		2
#define		P_POP3		3
#define		P_IMAP		4
#define		P_IMAP_K4	5
#define		P_IMAP_GSS	6
#define		P_APOP		7
#define		P_RPOP		8
#define		P_ETRN		9
#define		P_IMAP_CRAM_MD5	10
#define		P_IMAP_LOGIN	11

#if INET6
#define		SMTP_PORT	"smtp"
#define		KPOP_PORT	"kpop"
#else /* INET6 */
#define		SMTP_PORT	25
#define		KPOP_PORT	1109
#endif /* INET6 */

/* preauthentication types */
#define		A_PASSWORD	0	/* password or inline authentication */
#define		A_KERBEROS_V4	1	/* preauthenticate w/ Kerberos V4 */
#define		A_KERBEROS_V5	2	/* preauthenticate w/ Kerberos V5 */

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

#define		PASSWORDLEN	64	/* max password length */
#define		DIGESTLEN	33	/* length of MD5 digest */

/* exit code values */
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
/* leave space for more codes */
#define		PS_UNDEFINED	23	/* something I hadn't thought of */
#define		PS_TRANSIENT	24	/* transient failure (internal use) */
#define		PS_REFUSED	25	/* mail refused (internal use) */
#define		PS_RETAINED	26	/* message retained (internal use) */
#define		PS_TRUNCATED	27	/* headers incomplete (internal use) */

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

struct runctl
{
    char	*logfile;
    char	*idfile;
    int		poll_interval;
    char	*postmaster;
    flag	bouncemail;
    char	*properties;
    flag	use_syslog;
    flag	invisible;
};

struct idlist
{
    char *id;
    union
    {
	struct
	{
	    short	num;
	    flag	mark;		/* UID-index information */
#define UID_UNSEEN	0		/* hasn't been seen */
#define UID_SEEN	1		/* seen, but not deleted */
#define UID_DELETED	2		/* this message has been deleted */
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
#if INET6
    const char *service;
#else /* INET6 */
    int	port;			/* service port */
#endif /* INET6 */
    flag tagged;		/* if true, generate & expect command tags */
    flag delimited;		/* if true, accept "." message delimiter */
    int (*parse_response)(int, char *);
				/* response_parsing function */
    int (*password_canonify)(char *, char *);
				/* canonicalize password */
    int (*getauth)(int, struct query *, char *);
				/* authorization fetcher */
    int (*getrange)(int, struct query *, const char *, int *, int *, int *);
				/* get message range to fetch */
    int (*getsizes)(int, int, int *);
				/* get sizes of messages */
    int (*is_old)(int, struct query *, int);
				/* check for old message */
    int (*fetch_headers)(int, struct query *, int, int *);
				/* fetch FROM headera given message */
    int (*fetch_body)(int, struct query *, int, int *);
				/* fetch a given message */
    int (*trail)(int, struct query *, int);
				/* eat trailer of a message */
    int (*delete)(int, struct query *, int);
				/* delete method */
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
#if INET6
    char *service;			/* IPv6 service name */
    void *netsec;			/* IPv6 security request */
#else /* INET6 */
    int port;				/* TCP/IP service port number */
#endif /* INET6 */
    int interval;			/* # cycles to skip between polls */
    int preauthenticate;		/* preauthentication mode to try */
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
    flag checkalias;                  	/* resolve aliases by comparing IPs? */


#if defined(linux) || defined(__FreeBSD__)
    char *interface;
    char *monitor;
    int  monitor_io;
    struct interface_pair_s *interface_pair;
#endif /* linux */

    char *plugin,*plugout;

    /* computed for internal use */
    const struct method *base_protocol;	/* relevant protocol method table */
    int poll_count;			/* count of polls so far */
    char *queryname;			/* name to attempt DNS lookup on */
    char *truename;			/* "true name" of server host */
    struct hostdata *lead_server;	/* ptr to lead query for this server */
    int esmtp_options;
};

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
    char *smtpaddress;		/* address to force in RCPT TO */ 
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
    flag rewrite;		/* if TRUE, canonicalize recipient addresses */
    flag stripcr;		/* if TRUE, strip CRs in text */
    flag forcecr;		/* if TRUE, force CRs before LFs in text */
    flag pass8bits;		/* if TRUE, ignore Content-Transfer-Encoding */
    flag dropstatus;		/* if TRUE, drop Status lines in mail */
    flag mimedecode;		/* if TRUE, decode MIME-armored messages */
    int	limit;			/* limit size of retrieved messages */
    int warnings;		/* size warning interval */
    int	fetchlimit;		/* max # msgs to get in single poll */
    int	batchlimit;		/* max # msgs to pass in single SMTP session */
    int	expunge;		/* max # msgs to pass between expunges */
    char *properties;		/* passthrough properties for extensions */

    /* internal use -- per-poll state */
    flag active;		/* should we actually poll this server? */
    const char *destaddr;	/* destination host for this query */
    int errcount;		/* count transient errors in last pass */
    int wedged;			/* wedged by auth failures or timeouts? */
    char *smtphost;		/* actual SMTP host we connected to */
    int smtp_socket;		/* socket descriptor for SMTP connection */
    unsigned int uid;		/* UID of user to deliver to */
    struct idlist *skipped;	/* messages skipped on the mail server */
    struct idlist *oldsaved, *newsaved;
    char *lastid;		/* last Message-ID seen on this connection */

    /* internal use -- per-message state */
    int mimemsg;		/* bitmask indicating MIME body-type */
    char digest [DIGESTLEN];	/* md5 digest buffer */

    /* internal use -- housekeeping */
    struct query *next;		/* next query control block in chain */
};

struct msgblk			/* message header parsed for open_sink() */
{
    char   		*headers;	/* raw message headers */
    struct idlist	*recipients;	/* addressees */
    char		return_path[HOSTLEN + USERNAMELEN + 4]; 
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
extern flag check_only;		/* if --check was set */
extern char *rcfile;		/* path name of rc file */
extern int linelimit;		/* limit # lines retrieved per site */
extern flag versioninfo;	/* emit only version info */
extern char *user;		/* name of invoking user */
extern char *home;		/* home directory of invoking user */
extern int pass;		/* number of re-polling pass */
extern flag configdump;		/* dump control blocks as Python dictionary */
extern const char *fetchmailhost;
				/* either "localhost" or an FQDN */
#ifdef SDPS_ENABLE
extern char *sdps_envfrom;
extern char *sdps_envto;
#endif /* SDPS_ENABLE */

/* prototypes for globally callable functions */

/* error.c: Error reporting */
#if defined(HAVE_STDARG_H)
void report_init(int foreground);
void report (FILE *fp, const char *format, ...);
void report_build (FILE *fp, const char *format, ...);
void report_complete (FILE *fp, const char *format, ...);
void report_at_line (FILE *fp, int, const char *, unsigned int, const char *, ...);
#else
void report ();
void report_build ();
void report_complete ();
void report_at_line ();
#endif

/* driver.c: transaction support */
void set_timeout(int);
#if defined(HAVE_STDARG_H)
void gen_send (int sock, const char *, ... );
int gen_recv(int sock, char *buf, int size);
int gen_transact (int sock, const char *, ... );
#else
void gen_send ();
int gen_recv();
int gen_transact ();
#endif

/* use these to track what was happening when the nonresponse timer fired */
#define GENERAL_WAIT	0	/* unknown wait type */
#define OPEN_WAIT	1	/* waiting from mailserver open */
#define SERVER_WAIT	2	/* waiting for mailserver response */
#define LISTENER_WAIT	3	/* waiting for listener initialization */
#define FORWARDING_WAIT	4	/* waiting for listener response */
extern int phase;

/* mark values for name lists */
#define XMIT_ACCEPT	1	/* accepted; matches local domain or name */
#define XMIT_REJECT	2	/* rejected; no match */
#define XMIT_RCPTBAD	3	/* SMTP listener rejected the name */ 

/* sink.c: forwarding */
int stuffline(struct query *, char *);
int open_sink(struct query*, struct msgblk *, int*, int*);
void release_sink(struct query *);
int close_sink(struct query *, struct msgblk *, flag);
int open_warning_by_mail(struct query *, struct msgblk *);
#if defined(HAVE_STDARG_H)
void stuff_warning(struct query *, const char *, ... );
#else
void stuff_warning();
#endif
void close_warning_by_mail(struct query *, struct msgblk *);

/* rfc822.c: RFC822 header parsing */
char *reply_hack(char *, const char *);
char *nxtaddr(const char *);

/* uid.c: UID support */
void initialize_saved_lists(struct query *, const char *);
struct idlist *save_str(struct idlist **, const char *, flag);
void free_str_list(struct idlist **);
void save_str_pair(struct idlist **, const char *, const char *);
void free_str_pair_list(struct idlist **);
int delete_str(struct idlist **, int);
int str_in_list(struct idlist **, const char *, const flag);
int str_nr_in_list(struct idlist **, const char *);
int str_nr_last_in_list(struct idlist **, const char *);
void str_set_mark( struct idlist **, const char *, const flag);
int count_list( struct idlist **idl );
char *str_from_nr_list( struct idlist **idl, int number );
char *str_find(struct idlist **, int);
char *idpair_find(struct idlist **, const char *);
void append_str_list(struct idlist **, struct idlist **);
void expunge_uids(struct query *);
void update_str_lists(struct query *);
void write_saved_lists(struct query *, const char *);

/* rcfile_y.y */
int prc_parse_file(const char *, const flag);
int prc_filecheck(const char *, const flag);

/* base64.c */
void to64frombits(unsigned char *, const unsigned char *, int);
int from64tobits(char *, const char *);

/* unmime.c */
/* Bit-mask returned by MimeBodyType */
#define MSG_IS_7BIT       0x01
#define MSG_IS_8BIT       0x02
#define MSG_NEEDS_DECODE  0x80
extern void UnMimeHeader(unsigned char *buf);
extern int  MimeBodyType(unsigned char *hdrs, int WantDecode);
extern int  UnMimeBodyline(unsigned char **buf, flag delimited, flag issoftline);

/* interface.c */
void interface_init(void);
void interface_parse(char *, struct hostdata *);
void interface_note_activity(struct hostdata *);
int interface_approve(struct hostdata *);

/* xmalloc.c */
#if defined(HAVE_VOIDPOINTER)
#define XMALLOCTYPE void
#else
#define XMALLOCTYPE char
#endif
XMALLOCTYPE *xmalloc(int);
XMALLOCTYPE *xrealloc(XMALLOCTYPE *, int);
char *xstrdup(const char *);
#if defined(HAVE_ALLOCA_H)
#include <alloca.h>
#else
#ifdef _AIX
 #pragma alloca
#endif
#endif
#define	xalloca(ptr, t, n)	if (!(ptr = (t) alloca(n))) {report(stderr, "alloca failed"); exit(PS_UNDEFINED);}

/* protocol driver and methods */
int do_protocol(struct query *, const struct method *);
int doPOP2 (struct query *); 
int doPOP3 (struct query *);
int doIMAP (struct query *);
int doETRN (struct query *);

/* miscellanea */
struct query *hostalloc(struct query *); 
int parsecmdline (int, char **, struct runctl *, struct query *);
char *MD5Digest (unsigned char *);
int POP3_auth_rpa(unsigned char *, unsigned char *, int socket);
int daemonize(const char *, void (*)(int));
char *getpassword(char *);
void escapes(const char *, char *);
char *visbuf(const char *);
const char *showproto(int);
void dump_config(struct runctl *runp, struct query *querylist);
int is_host_alias(const char *, struct query *);
char *host_fqdn(void);
char *rfc822timestamp(void);

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

# if HAVE_STRERROR
#  ifndef strerror		/* On some systems, strerror is a macro */
char *strerror ();
#  endif
# endif /* HAVE_STRERROR */

#define STRING_DISABLED	(char *)-1
#define STRING_DUMMY	""

#ifdef NeXT
#ifndef S_IXGRP
#define S_IXGRP 0000010
#endif
#endif

/* fetchmail.h ends here */
