%{
/*
 * rcfile_y.y -- Run control file parser for fetchmail
 *
 * For license terms, see the file COPYING in this directory.
 */

#include "config.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/file.h>
#if defined(HAVE_SYS_WAIT_H)
#include <sys/wait.h>
#endif
#include <sys/stat.h>
#include <errno.h>
#if defined(STDC_HEADERS)
#include <stdlib.h>
#endif
#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif
#include <string.h>

#if NET_SECURITY
#include <net/security.h>
#endif /* NET_SECURITY */

#include "fetchmail.h"

/* parser reads these */
char *rcfile;			/* path name of rc file */
struct query cmd_opts;		/* where to put command-line info */

/* parser sets these */
struct query *querylist;	/* head of server list (globally visible) */

int yydebug;			/* in case we didn't generate with -- debug */

static struct query current;	/* current server record */
static int prc_errflag;
static struct hostdata *leadentry;
static flag trailer;

static void record_current(void);
static void user_reset(void);
static void reset_server(const char *name, int skip);

/* using Bison, this arranges that yydebug messages will show actual tokens */
extern char * yytext;
#define YYPRINT(fp, type, val)	fprintf(fp, " = \"%s\"", yytext)
%}

%union {
  int proto;
  int number;
  char *sval;
}

%token DEFAULTS POLL SKIP VIA AKA LOCALDOMAINS PROTOCOL
%token PREAUTHENTICATE TIMEOUT KPOP SDPS KERBEROS4 KERBEROS5 KERBEROS
%token ENVELOPE QVIRTUAL USERNAME PASSWORD FOLDER SMTPHOST MDA BSMTP LMTP
%token SMTPADDRESS SPAMRESPONSE PRECONNECT POSTCONNECT LIMIT WARNINGS
%token NETSEC INTERFACE MONITOR PLUGIN PLUGOUT
%token IS HERE THERE TO MAP WILDCARD
%token BATCHLIMIT FETCHLIMIT EXPUNGE PROPERTIES
%token SET LOGFILE DAEMON SYSLOG IDFILE INVISIBLE POSTMASTER BOUNCEMAIL
%token <proto> PROTO
%token <sval>  STRING
%token <number> NUMBER
%token NO KEEP FLUSH FETCHALL REWRITE FORCECR STRIPCR PASS8BITS DROPSTATUS
%token DNS SERVICE PORT UIDL INTERVAL MIMEDECODE CHECKALIAS

%%

rcfile		: /* empty */
		| statement_list
		;

statement_list	: statement
		| statement_list statement
		;

optmap		: MAP | /* EMPTY */;

/* future global options should also have the form SET <name> optmap <value> */
statement	: SET LOGFILE optmap STRING	{run.logfile = xstrdup($4);}
		| SET IDFILE optmap STRING	{run.idfile = xstrdup($4);}
		| SET DAEMON optmap NUMBER	{run.poll_interval = $4;}
		| SET POSTMASTER optmap STRING	{run.postmaster = xstrdup($4);}
		| SET BOUNCEMAIL		{run.bouncemail = TRUE;}
		| SET NO BOUNCEMAIL		{run.bouncemail = FALSE;}
		| SET PROPERTIES optmap STRING	{run.properties =xstrdup($4);}
		| SET SYSLOG			{run.use_syslog = TRUE;}
		| SET INVISIBLE			{run.invisible = TRUE;}

/* 
 * The way the next two productions are written depends on the fact that
 * userspecs cannot be empty.  It's a kluge to deal with files that set
 * up a load of defaults and then have poll statements following with no
 * user options at all. 
 */
		| define_server serverspecs		{record_current();}
		| define_server serverspecs userspecs

/* detect and complain about the most common user error */
		| define_server serverspecs userspecs serv_option
			{yyerror("server option after user options");}
		;

define_server	: POLL STRING		{reset_server($2, FALSE);}
		| SKIP STRING		{reset_server($2, TRUE);}
		| DEFAULTS		{reset_server("defaults", FALSE);}
  		;

serverspecs	: /* EMPTY */
		| serverspecs serv_option
		;

alias_list	: STRING		{save_str(&current.server.akalist,$1,0);}
		| alias_list STRING	{save_str(&current.server.akalist,$2,0);}
		;

domain_list	: STRING		{save_str(&current.server.localdomains,$1,0);}
		| domain_list STRING	{save_str(&current.server.localdomains,$2,0);}
		;

serv_option	: AKA alias_list
		| VIA STRING		{current.server.via = xstrdup($2);}
		| LOCALDOMAINS domain_list
		| PROTOCOL PROTO	{current.server.protocol = $2;}
		| PROTOCOL KPOP		{
					    current.server.protocol = P_POP3;

					    if (current.server.preauthenticate == A_PASSWORD)
#ifdef KERBEROS_V5
						current.server.preauthenticate = A_KERBEROS_V5;
#else
		    				current.server.preauthenticate = A_KERBEROS_V4;
#endif /* KERBEROS_V5 */
#if INET6
					    current.server.service = KPOP_PORT;
#else /* INET6 */
					    current.server.port = KPOP_PORT;
#endif /* INET6 */
					}
		| PROTOCOL SDPS		{
#ifdef SDPS_ENABLE
					    current.server.protocol = P_POP3;
					    current.server.sdps = TRUE;
#else
					    yyerror("SDPS not enabled.");
#endif /* SDPS_ENABLE */
					}
		| UIDL			{current.server.uidl = FLAG_TRUE;}
		| NO UIDL		{current.server.uidl  = FLAG_FALSE;}
		| CHECKALIAS            {current.server.checkalias = FLAG_TRUE;}
		| NO CHECKALIAS         {current.server.checkalias  = FLAG_FALSE;}
		| SERVICE STRING	{
#if INET6
					current.server.service = $2;
#endif /* INET6 */
					}
		| PORT NUMBER		{
#if !INET6
					current.server.port = $2;
#endif /* !INET6 */
					}
		| INTERVAL NUMBER		{current.server.interval = $2;}
		| PREAUTHENTICATE PASSWORD	{current.server.preauthenticate = A_PASSWORD;}
		| PREAUTHENTICATE KERBEROS4	{current.server.preauthenticate = A_KERBEROS_V4;}
                | PREAUTHENTICATE KERBEROS5 	{current.server.preauthenticate = A_KERBEROS_V5;}
                | PREAUTHENTICATE KERBEROS         {
#ifdef KERBEROS_V5
		    current.server.preauthenticate = A_KERBEROS_V5;
#else
		    current.server.preauthenticate = A_KERBEROS_V4;
#endif /* KERBEROS_V5 */
		}
		| TIMEOUT NUMBER	{current.server.timeout = $2;}

		| ENVELOPE NUMBER STRING 
					{
					    current.server.envelope = 
						xstrdup($3);
					    current.server.envskip = $2;
					}
		| ENVELOPE STRING
					{
					    current.server.envelope = 
						xstrdup($2);
					    current.server.envskip = 0;
					}

		| QVIRTUAL STRING	{current.server.qvirtual=xstrdup($2);}
		| NETSEC STRING		{
#ifdef NET_SECURITY
					    void *request;
					    int requestlen;

		    			    if (net_security_strtorequest($2, &request, &requestlen))
						yyerror("invalid security request");
					    else {
						current.server.netsec = xstrdup($2);
					        free(request);
					    }
#else
					    yyerror("network-security support disabled");
#endif /* NET_SECURITY */
					}
		| INTERFACE STRING	{
#if (defined(linux) && !defined(INET6)) || defined(__FreeBSD__)
					interface_parse($2, &current.server);
#else /* (defined(linux) && !defined(INET6)) || defined(__FreeBSD__) */
					fprintf(stderr, "fetchmail: interface option is only supported under Linux and FreeBSD\n");
#endif /* (defined(linux) && !defined(INET6)) || defined(__FreeBSD__) */
					}
		| MONITOR STRING	{
#if (defined(linux) && !defined(INET6)) || defined(__FreeBSD__)
					current.server.monitor = xstrdup($2);
#else /* (defined(linux) && !defined(INET6)) || defined(__FreeBSD__) */
					fprintf(stderr, "fetchmail: monitor option is only supported under Linux\n");
#endif /* (defined(linux) && !defined(INET6) || defined(__FreeBSD__)) */
					}
		| PLUGIN STRING		{ current.server.plugin = xstrdup($2); }
		| PLUGOUT STRING	{ current.server.plugout = xstrdup($2); }
		| DNS			{current.server.dns = FLAG_TRUE;}
		| NO DNS		{current.server.dns = FLAG_FALSE;}
		| NO ENVELOPE		{current.server.envelope = STRING_DISABLED;}
		;

userspecs	: user1opts		{record_current(); user_reset();}
		| explicits
		;

explicits	: explicitdef		{record_current(); user_reset();}
		| explicits explicitdef	{record_current(); user_reset();}
		;

explicitdef	: userdef user0opts
		;

userdef		: USERNAME STRING	{current.remotename = xstrdup($2);}
		| USERNAME mapping_list HERE
		| USERNAME STRING THERE	{current.remotename = xstrdup($2);}
		;

user0opts	: /* EMPTY */
		| user0opts user_option
		;

user1opts	: user_option
		| user1opts user_option
		;

localnames	: WILDCARD		{current.wildcard =  TRUE;}
		| mapping_list		{current.wildcard =  FALSE;}
		| mapping_list WILDCARD	{current.wildcard =  TRUE;}
		;

mapping_list	: mapping		
		| mapping_list mapping
		;

mapping		: STRING	
				{save_str_pair(&current.localnames, $1, NULL);}
		| STRING MAP STRING
				{save_str_pair(&current.localnames, $1, $3);}
		;

folder_list	: STRING		{save_str(&current.mailboxes,$1,0);}
		| folder_list STRING	{save_str(&current.mailboxes,$2,0);}
		;

smtp_list	: STRING		{save_str(&current.smtphunt, $1,TRUE);}
		| smtp_list STRING	{save_str(&current.smtphunt, $2,TRUE);}
		;

num_list	: NUMBER
			{
			    struct idlist *id;
			    id=save_str(&current.antispam,STRING_DUMMY,0);
			    id->val.status.num = $1;
			}
		| num_list NUMBER
			{
			    struct idlist *id;
			    id=save_str(&current.antispam,STRING_DUMMY,0);
			    id->val.status.num = $2;
			}
		;

user_option	: TO localnames HERE
		| TO localnames
		| IS localnames HERE
		| IS localnames

		| IS STRING THERE	{current.remotename  = xstrdup($2);}
		| PASSWORD STRING	{current.password    = xstrdup($2);}
		| FOLDER folder_list
		| SMTPHOST smtp_list
		| SMTPADDRESS STRING	{current.smtpaddress = xstrdup($2);}
		| SPAMRESPONSE num_list
		| MDA STRING		{current.mda         = xstrdup($2);}
		| BSMTP STRING		{current.bsmtp       = xstrdup($2);}
		| LMTP			{current.listener    = LMTP_MODE;}
		| PRECONNECT STRING	{current.preconnect  = xstrdup($2);}
		| POSTCONNECT STRING	{current.postconnect = xstrdup($2);}

		| KEEP			{current.keep        = FLAG_TRUE;}
		| FLUSH			{current.flush       = FLAG_TRUE;}
		| FETCHALL		{current.fetchall    = FLAG_TRUE;}
		| REWRITE		{current.rewrite     = FLAG_TRUE;}
		| FORCECR		{current.forcecr     = FLAG_TRUE;}
		| STRIPCR		{current.stripcr     = FLAG_TRUE;}
		| PASS8BITS		{current.pass8bits   = FLAG_TRUE;}
		| DROPSTATUS		{current.dropstatus  = FLAG_TRUE;}
		| MIMEDECODE		{current.mimedecode  = FLAG_TRUE;}

		| NO KEEP		{current.keep        = FLAG_FALSE;}
		| NO FLUSH		{current.flush       = FLAG_FALSE;}
		| NO FETCHALL		{current.fetchall    = FLAG_FALSE;}
		| NO REWRITE		{current.rewrite     = FLAG_FALSE;}
		| NO FORCECR		{current.forcecr     = FLAG_FALSE;}
		| NO STRIPCR		{current.stripcr     = FLAG_FALSE;}
		| NO PASS8BITS		{current.pass8bits   = FLAG_FALSE;}
		| NO DROPSTATUS		{current.dropstatus  = FLAG_FALSE;}
		| NO MIMEDECODE		{current.mimedecode  = FLAG_FALSE;}

		| LIMIT NUMBER		{current.limit       = NUM_VALUE_IN($2);}
		| WARNINGS NUMBER	{current.warnings    = NUM_VALUE_IN($2);}
		| FETCHLIMIT NUMBER	{current.fetchlimit  = NUM_VALUE_IN($2);}
		| BATCHLIMIT NUMBER	{current.batchlimit  = NUM_VALUE_IN($2);}
		| EXPUNGE NUMBER	{current.expunge     = NUM_VALUE_IN($2);}

		| PROPERTIES STRING	{current.properties  = xstrdup($2);}
		;
%%

/* lexer interface */
extern char *rcfile;
extern int prc_lineno;
extern char *yytext;
extern FILE *yyin;

static struct query *hosttail;	/* where to add new elements */

void yyerror (const char *s)
/* report a syntax error */
{
    report_at_line(stderr, 0, rcfile, prc_lineno, "%s at %s", s, 
		   (yytext && yytext[0]) ? yytext : "end of input");
    prc_errflag++;
}

int prc_filecheck(const char *pathname, const flag securecheck)
/* check that a configuration file is secure */
{
#ifndef __EMX__
    struct stat statbuf;

    errno = 0;

    /* special case useful for debugging purposes */
    if (strcmp("/dev/null", pathname) == 0)
	return(PS_SUCCESS);

    /* pass through the special name for stdin */
    if (strcmp("-", pathname) == 0)
	return(PS_SUCCESS);

    /* the run control file must have the same uid as the REAL uid of this 
       process, it must have permissions no greater than 600, and it must not 
       be a symbolic link.  We check these conditions here. */

    if (lstat(pathname, &statbuf) < 0) {
	if (errno == ENOENT) 
	    return(PS_SUCCESS);
	else {
	    report(stderr, "lstat: %s: %s\n", pathname, strerror(errno));
	    return(PS_IOERR);
	}
    }

    if (!securecheck)	return 0;

    if ((statbuf.st_mode & S_IFLNK) == S_IFLNK)
    {
	fprintf(stderr, "File %s must not be a symbolic link.\n", pathname);
	return(PS_IOERR);
    }

    if (statbuf.st_mode & ~(S_IFREG | S_IREAD | S_IWRITE | S_IEXEC | S_IXGRP))
    {
	fprintf(stderr, "File %s must have no more than -rwx--x--- (0710) permissions.\n", 
		pathname);
	return(PS_IOERR);
    }

#ifdef HAVE_GETEUID
    if (statbuf.st_uid != geteuid())
#else
    if (statbuf.st_uid != getuid())
#endif /* HAVE_GETEUID */
    {
	fprintf(stderr, "File %s must be owned by you.\n", pathname);
	return(PS_IOERR);
    }
#endif
    return(PS_SUCCESS);
}

int prc_parse_file (const char *pathname, const flag securecheck)
/* digest the configuration into a linked list of host records */
{
    prc_errflag = 0;
    querylist = hosttail = (struct query *)NULL;

    errno = 0;

    /* Check that the file is secure */
    if ( (prc_errflag = prc_filecheck(pathname, securecheck)) != 0 )
	return(prc_errflag);

    if (errno == ENOENT)
	return(PS_SUCCESS);

    /* Open the configuration file and feed it to the lexer. */
    if (strcmp(pathname, "-") == 0)
	yyin = stdin;
    else if ((yyin = fopen(pathname,"r")) == (FILE *)NULL) {
	report(stderr, "open: %s: %s\n", pathname, strerror(errno));
	return(PS_IOERR);
    }

    yyparse();		/* parse entire file */

    fclose(yyin);

    if (prc_errflag) 
	return(PS_SYNTAX);
    else
	return(PS_SUCCESS);
}

static void reset_server(const char *name, int skip)
/* clear the entire global record and initialize it with a new name */
{
    trailer = FALSE;
    memset(&current,'\0',sizeof(current));
    current.smtp_socket = -1;
    current.server.pollname = xstrdup(name);
    current.server.skip = skip;
}


static void user_reset(void)
/* clear the global current record (user parameters) used by the parser */
{
    struct hostdata save;

    /*
     * Purpose of this code is to initialize the new server block, but
     * preserve whatever server name was previously set.  Also
     * preserve server options unless the command-line explicitly
     * overrides them.
     */
    save = current.server;

    memset(&current, '\0', sizeof(current));
    current.smtp_socket = -1;

    current.server = save;
}

struct query *hostalloc(init)
/* append a host record to the host list */
struct query *init;	/* pointer to block containing initial values */
{
    struct query *node;

    /* allocate new node */
    node = (struct query *) xmalloc(sizeof(struct query));

    /* initialize it */
    if (init)
	memcpy(node, init, sizeof(struct query));
    else
    {
	memset(node, '\0', sizeof(struct query));
	node->smtp_socket = -1;
    }

    /* append to end of list */
    if (hosttail != (struct query *) 0)
	hosttail->next = node;	/* list contains at least one element */
    else
	querylist = node;	/* list is empty */
    hosttail = node;

    if (trailer)
	node->server.lead_server = leadentry;
    else
    {
	node->server.lead_server = NULL;
	leadentry = &node->server;
    }

    return(node);
}

static void record_current(void)
/* register current parameters and append to the host list */
{
    (void) hostalloc(&current);
    trailer = TRUE;
}

/* easier to do this than cope with variations in where the library lives */
int yywrap(void) {return 1;}

/* rcfile_y.y ends here */


