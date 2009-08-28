/*$Id: config.h,v 1.101 2001/08/27 08:43:57 guenther Exp $*/

/*#define sMAILBOX_SEPARATOR	"\1\1\1\1\n"	/* sTART- and eNDing separ.  */
/*#define eMAILBOX_SEPARATOR	"\1\1\1\1\n"	/* uncomment (one or both)
						   if your mail system uses
	nonstandard mail separators (non sendmail or smail compatible mailers
	like MMDF), if yours is even different, uncomment and change the
	value of course */

/* KEEPENV and PRESTENV should be defined as a comma-separated null-terminated
   list of strings */

/* every environment variable appearing in KEEPENV will not be thrown away
 * upon startup of procmail, e.g. you could define KEEPENV as follows:
 * #define KEEPENV	{"TZ","LANG",0}
 * environment variables ending in an _ will designate the whole group starting
 * with this prefix (e.g. "LC_").  Note that keeping LANG and or the LC_
 * variables is not recommended for most installations due to the security
 * considerations/dependencies present in the use of locales other than
 * the "C" locale.
 */
#define KEEPENV		{"TZ",0}

/* procmail is compiled with two definitions of the PATH variable.  The first
 * definition is used while processing the /etc/procmailrc file and should
 * only contain trustable (i.e., system) directories.  Otherwise the second
 * definition is used.	Note that the /etc/procmailrc file cannot change the
 * PATH seen by user's rcfiles: the second definition will be applied upon the
 * completion of the /etc/procmailrc file (future versions of procmail are
 * expected to provide better runtime configuration control).  The autoconf
 * process attempts to determine reasonable values for these versions of PATH
 * and sets the defSPATH and defPATH variables accordingly.  If you want to
 * override those settings you should uncomment and possibly change the
 * DEFSPATH and DEFPATH defines below
 */
/*#define DEFSPATH	"PATH=/bin:/usr/bin"			/* */
/*#define DEFPATH	"PATH=$HOME/bin:/bin:/usr/bin"		/* */

/* every environment variable appearing in PRESTENV will be set or wiped
 * out of the environment (variables without an '=' sign will be thrown
 * out), e.g. you could define PRESTENV as follows:
 * #define PRESTENV	{"IFS","ENV","PWD",0}
 * any side effects (like setting the umask after an assignment to UMASK) will
 * *not* take place.  Do *not* define PATH here -- use the DEFSPATH and
 * DEFPATH defines above instead
 */
#define PRESTENV	{"IFS","ENV","PWD",0}

/*#define GROUP_PER_USER			/* uncomment this if each
						   user has his or her own
	group and procmail can therefore trust a $HOME/.procmailrc that
	is group writable or contained in a group writable home directory
	if the group involved is the user's default group. */

/*#define LMTP					/* uncomment this if you
						   want to use procmail
	as an LMTP (rfc2033) server, presumably for invocation by an MTA.
	The file examples/local_procmail_lmtp.m4 contains info on how to
	set this up with sendmail. */

/* This file previously allowed you to define SYSTEM_MBOX.  This has
   changed.  If you want mail delivery to custom mail-spool-files, edit the
   src/authenticate.c file and change the content of:  auth_mailboxname()
   (either directly, or through changing the definitions in the same file
   of MAILSPOOLDIR, MAILSPOOLSUFFIX, MAILSPOOLHASH or MAILSPOOLHOME) */

/************************************************************************
 * Only edit below this line if you have viewed/edited this file before *
 ************************************************************************/

/* every user & group appearing in TRUSTED_IDS is allowed to use the -f option
   if the list is empty (just a terminating 0), everyone can use it
   TRUSTED_IDS should be defined as a comma-separated null-terminated
   list of strings;  if unauthorised users use the -f option, an extra
   >From_ field will be added in the header */

#define TRUSTED_IDS	{"root","daemon","uucp","mail","x400","network",\
			 "list","slist","lists","news",0}

/*#define NO_fcntl_LOCK		/* uncomment any of these three if you	     */
/*#define NO_lockf_LOCK		/* definitely do not want procmail to make   */
/*#define NO_flock_LOCK		/* use of those kernel-locking methods	     */
				/* If you set LOCKINGTEST to a binary number
	than there's no need to set these.  These #defines are only useful
	if you want to disable particular locking styles but are unsure which
	of the others are safe.	 Otherwise, don't touch them.  */

/*#define RESTRICT_EXEC 100	/* uncomment to prevent users with uids equal
				   or higher than RESTRICT_EXEC from
	executing programs from within their .procmailrc files (this
	restriction does not apply to the /etc/procmailrc and
	/etc/procmailrcs files) */

/*#define NO_NFS_ATIME_HACK	/* uncomment if you're definitely not using
				   NFS mounted filesystems and can't afford
	procmail to sleep for 1 sec. before writing to an empty regular
	mailbox.  This lets programs correctly judge whether there is unread
	mail present.  procmail automatically suppresses this when it isn't
	needed or under heavy load. */

/*#define DEFsendmail	"/usr/sbin/sendmail"	/* uncomment and/or change if
						   the autoconfigured default
	SENDMAIL is not suitable.  This program should quack like a sendmail:
	it should accept the -oi flag (to tell it to _not_ treat a line
	containing just a period as EOF) and then a list of recipients.	 If the
	-t flag is given, it should instead extract the recipients from the
	To:, Cc:, and Bcc: header fields.  If it can't do this, many standard
	recipes will not work.	One reasonable candidate is "/etc/mta/send"
	on systems that support the MTA configuration switch. */

#define DEFmaildir	"$HOME"	     /* default value for the MAILDIR variable;
					this must be an absolute path */

#define PROCMAILRC	"$HOME/.procmailrc"	/* default rcfile for every
						   recipient;  if this file
	is not found, maildelivery will proceed as normal to the default
	system mailbox.	 This also must be an absolute path */

#define ETCRC	"/etc/procmailrc"	/* optional global procmailrc startup
					   file (will only be read if procmail
	is started with no rcfile on the command line). */

#define ETCRCS	"/etc/procmailrcs/"	/* optional trusted path prefix for
					   rcfiles which will be executed with
	the uid of the owner of the rcfile (this only happens if procmail is
	called with the -m option, without variable assignments on the command
	line). */

/*#define console	"/dev/console"	/* uncomment if you want procmail to
					   use the console (or any other
	terminal or file) to print any error messages that could not be dumped
	in the "logfile"; only recommended for debugging purposes, if you have
	trouble creating a "logfile" or suspect that the trouble starts before
	procmail can interpret any rcfile or arguments. */

/************************************************************************
 * Only edit below this line if you *think* you know what you are doing *
 ************************************************************************/

#define ROOT_uid	0
#define LDENV		{"LD_","_RLD","LIBPATH=","ELF_LD_","AOUT_LD_",0}

#define UPDATE_MASK	S_IXOTH	   /* bit set on mailboxes when mail arrived */
#define OVERRIDE_MASK	(S_IXUSR|S_ISUID|S_ISGID|S_ISVTX)    /* if found set */
		    /* the permissions on the mailbox will be left untouched */
#define INIT_UMASK	(S_IRWXG|S_IRWXO)			   /* == 077 */
#define GROUPW_UMASK	(INIT_UMASK&~S_IRWXG)			   /* == 007 */
#define NORMperm	\
 (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH|UPDATE_MASK)
	     /* == 0667, normal mode bits used to create files, before umask */
#define READperm	(S_IRUSR|S_IRGRP|S_IROTH)		  /* == 0444 */
#define NORMdirperm	(S_IRWXU|S_IRWXG|S_IRWXO)		  /* == 0777 */
#define LOCKperm	READperm  /* mode bits used while creating lockfiles */
#define MAX_locksize	16	  /* lockfiles are expected not to be longer */
#ifndef SMALLHEAP
#define DEFlinebuf	2048		 /* default max expanded line length */
#define BLKSIZ		16384		  /* blocksize while reading/writing */
#define STDBUF		1024		     /* blocksize for emulated stdio */
#else		   /* and some lower defaults for the unfortunate amongst us */
#define DEFlinebuf	512
#define BLKSIZ		1024
#define STDBUF		128
#undef USE_MMAP				       /* don't bother on these guys */
#endif /* SMALLHEAP */
#undef USE_MMAP					 /* UNTIL PROBLEMS ARE FIXED */
#ifdef USE_MMAP
#ifndef INEFFICIENTrealloc
#define INEFFICIENTrealloc			  /* don't pussy-foot around */
#endif
#define MAXinMEM	(1024*1024)		 /* when to switch to mmap() */
#define MMAP_DIR	"/var/spool/procmail/"		     /* where to put */
#endif								/* the files */
#define MINlogbuf	81			       /* fit an entire line */
#define MAXlogbuf	1000		       /* in case someone abuses LOG */
#define MAILERDAEMON	"MAILER-DAEMON"	      /* From_ address to replace <> */
#define FAKE_FIELD	">From "
#define RETRYunique	8	   /* # of tries at making a unique filename */
#define BOGUSprefix	"BOGUS."	     /* prepended to bogus mailboxes */
#define DEFsuspend	16		 /* multi-purpose 'idle loop' period */
#define DEFlocksleep	8
#define TO_key		"^TO_"				    /* for addresses */
#define TO_substitute	"(^((Original-)?(Resent-)?(To|Cc|Bcc)|\
(X-Envelope|Apparently(-Resent)?)-To):(.*[^-a-zA-Z0-9_.])?)"
#define TOkey		"^TO"					/* for words */
#define TOsubstitute	"(^((Original-)?(Resent-)?(To|Cc|Bcc)|\
(X-Envelope|Apparently(-Resent)?)-To):(.*[^a-zA-Z])?)"
#define FROMDkey	"^FROM_DAEMON"		     /* matches most daemons */
#define FROMDsubstitute "(^(Mailing-List:|Precedence:.*(junk|bulk|list)|\
To: Multiple recipients of |\
(((Resent-)?(From|Sender)|X-Envelope-From):|>?From )([^>]*[^(.%@a-z0-9])?(\
Post(ma?(st(e?r)?|n)|office)|(send)?Mail(er)?|daemon|m(mdf|ajordomo)|n?uucp|\
LIST(SERV|proc)|NETSERV|o(wner|ps)|r(e(quest|sponse)|oot)|b(ounce|bs\\.smtp)|\
echo|mirror|s(erv(ices?|er)|mtp(error)?|ystem)|\
A(dmin(istrator)?|MMGR|utoanswer)\
)(([^).!:a-z0-9][-_a-z0-9]*)?[%@>	 ][^<)]*(\\(.*\\).*)?)?$([^>]|$)))"
#define FROMMkey	"^FROM_MAILER"	      /* matches most mailer-daemons */
#define FROMMsubstitute "(^(((Resent-)?(From|Sender)|X-Envelope-From):|\
>?From )([^>]*[^(.%@a-z0-9])?(\
Post(ma(st(er)?|n)|office)|(send)?Mail(er)?|daemon|mmdf|n?uucp|ops|\
r(esponse|oot)|(bbs\\.)?smtp(error)?|s(erv(ices?|er)|ystem)|A(dmin(istrator)?|\
MMGR)\
)(([^).!:a-z0-9][-_a-z0-9]*)?[%@>	 ][^<)]*(\\(.*\\).*)?)?$([^>]|$))"
#define DEFshellmetas	"&|<>~;?*["		    /* never put '$' in here */
#define DEFdefault	"$ORGMAIL"
#define DEFmsgprefix	"msg."
#define DEFlockext	".lock"
#define DEFshellflags	"-c"
#define DEFlocktimeout	1024		     /* defaults to about 17 minutes */
#define DEFtimeout	(DEFlocktimeout-64)	   /* 64 seconds to clean up */
#define DEFnoresretry	4      /* default nr of retries if no resources left */
#define nfsTRY		(7+1) /* nr of times+1 to ignore spurious NFS errors */
#define MATCHVAR	"MATCH"
#define AMATCHVAR	"MATCH="
#define DEFlogabstract	-1    /* abstract by default, but don't mail it back */
#define COMSAThost	"localhost"    /* where the biff/comsat daemon lives */
#define COMSATservice	"biff"	    /* the service name of the comsat daemon */
#define COMSATprotocol	"udp" /* if you change this, comsat() needs patching */
#define COMSATxtrsep	":"		 /* mailbox-spec extension separator */
#define SERV_ADDRsep	'@'	      /* when overriding in COMSAT=serv@addr */
#define DEFcomsat	offvalue	/* when an rcfile has been specified */
				      /* set to either "offvalue" or "empty" */

#define BinSh		"/bin/sh"
#define ROOT_DIR	"/"
#define DEAD_LETTER	"/tmp/dead.letter"    /* $ORGMAIL if no passwd entry */
#define DevNull		"/dev/null"
#define NICE_RANGE	39			  /* maximal nice difference */
#define chCURDIR	'.'			    /* the current directory */
#define chPARDIR	".."			     /* the parent directory */
#define DIRSEP		"/"		 /* directory separator symbols, the */
				   /* last one should be the most common one */
#define MAILDIRtmp	"/tmp"			   /* maildir subdirectories */
#define MAILDIRcur	"/cur"
#define MAILDIRnew	"/new"
#define MAILDIRLEN	STRLEN(MAILDIRnew)
#define MAILDIRretries	5	   /* retries on obtaining a unique filename */

#define EOFName		" \t\n#`'\");"

#define HELPOPT1	'h'		 /* options to get command line help */
#define HELPOPT2	'?'

#define VERSIONOPT	'v'			/* option to display version */
#define PRESERVOPT	'p'			     /* preserve environment */
#define TEMPFAILOPT	't'		      /* return EX_TEMPFAIL on error */
#define MAILFILTOPT	'm'	     /* act as a general purpose mail filter */
#define FROMWHOPT	'f'			   /* set name on From_ line */
#define REFRESH_TIME	'-'		     /* when given as argument to -f */
#define ALTFROMWHOPT	'r'		/* alternate and obsolete form of -f */
#define OVERRIDEOPT	'o'		     /* do not generate >From_ lines */
#define BERKELEYOPT	'Y'    /* Berkeley format, disregard Content-Length: */
#define ALTBERKELEYOPT	'y'			/* same effect as -Y, kludge */
#define ARGUMENTOPT	'a'					   /* set $1 */
#define DELIVEROPT	'd'		  /* deliver mail to named recipient */
#define LMTPOPT		'z'			/* talk LTMP on stdin/stdout */
#define PM_USAGE	\
 "Usage: procmail [-vptoY] [-f fromwhom] [parameter=value | rcfile] ...\
\n   Or: procmail [-toY] [-f fromwhom] [-a argument] ... -d recipient ...\
\n\
   Or: procmail [-ptY] [-f fromwhom] -m [parameter=value] ... rcfile [arg] ...\
\n   Or: procmail [-toY] [-a argument] ... -z\
\n"
#define PM_HELP		\
 "\t-v\t\tdisplay the version number and exit\
\n\t-p\t\tpreserve (most of) the environment upon startup\
\n\t-t\t\tfail softly if mail is undeliverable\
\n\t-f fromwhom\t(re)generate the leading 'From ' line\
\n\t-o\t\toverride the leading 'From ' line if necessary\
\n\t-Y\t\tBerkeley format mailbox, disregard Content-Length:\
\n\t-a argument\twill set $1, $2, etc\
\n\t-d recipient\texplicit delivery mode\
\n\t-z\t\tact as an LMTP server\
\n\t-m\t\tact as a general purpose mail filter\n"
#define PM_QREFERENCE	\
 "Recipe flag quick reference:\
\n\tH  egrep header (default)\tB  egrep body\
\n\tD  distinguish case\
\n\tA  also execute this recipe if the common condition matched\
\n\ta  same as 'A', but only if the previous recipe was successful\
\n\tE  else execute this recipe, if the preceding condition didn't match\
\n\te  on error execute this recipe, if the previous recipe failed\
\n\th  deliver header (default)\tb  deliver body (default)\
\n\tf  filter\t\t\ti  ignore write errors\
\n\tc  carbon copy or clone message\
\n\tw  wait for a program\t\tr\
  raw mode, mail as is\
\n\tW  same as 'w', but suppress 'Program failure' messages\n"

#define MINlinebuf	128    /* minimal LINEBUF length (don't change this) */
#define FROM_EXPR	"\nFrom "
#define FROM		"From "
#define SHFROM		"From"
#define NSUBJECT	"^Subject:.*$"
#define MAXSUBJECTSHOW	78
#define FOLDER		"  Folder: "
#define LENtSTOP	9 /* tab stop at which message length will be logged */

#define TABCHAR		"\t"
#define TABWIDTH	8

#define RECFLAGS	"HBDAahbfcwWiEer"
#define HEAD_GREP	 0
#define BODY_GREP	  1
#define DISTINGUISH_CASE   2
#define ALSO_NEXT_RECIPE    3
#define ALSO_N_IF_SUCC	     4
#define PASS_HEAD	      5
#define PASS_BODY	       6
#define FILTER			7
#define CONTINUE		 8			      /* carbon copy */
#define WAIT_EXIT		  9
#define WAIT_EXIT_QUIET		   10
#define IGNORE_WRITERR		    11
#define ELSE_DO			     12
#define ERROR_DO		      13
#define RAW_NONL		       14

#define UNIQ_PREFIX	'_'	  /* prepended to temporary unique filenames */
#define ESCAP		">"

		/* some formail-specific configuration options: */

#define UNKNOWN		"foo@bar"	  /* formail default originator name */
#define OLD_PREFIX	"Old-"			 /* formail field-Old-prefix */
#define RESENT_		"Resent-"    /* -a *this* to reply to Resent headers */
#define BABYL_SEP1	'\037'		       /* BABYL format separator one */
#define BABYL_SEP2	'\f'		       /* BABYL format separator two */
#define DEFfileno	"FILENO=000"		/* split counter for formail */
#define LEN_FILENO_VAR	7			       /* =strlen("FILENO=") */
#define CHILD_FACTOR	3/4 /* do not parenthesise; average running children */

#define FM_SKIP		'+'		      /* skip the first nnn messages */
#define FM_TOTAL	'-'	    /* only spit out a total of nnn messages */
#define FM_BOGUS	'b'			 /* leave bogus Froms intact */
#define FM_BERKELEY	BERKELEYOPT   /* Berkeley format, no Content-Length: */
#define FM_QPREFIX	'p'			  /* define quotation prefix */
#define FM_CONCATENATE	'c'	      /* concatenate continued header-fields */
#define FM_ZAPWHITE	'z'		 /* zap whitespace and empty headers */
#define FM_FORCE	'f'   /* force formail to accept an arbitrary format */
#define FM_REPLY	'r'		    /* generate an auto-reply header */
#define FM_KEEPB	'k'		   /* keep the header, when replying */
#define FM_TRUST	't'		       /* reply to the header sender */
#define FM_LOGSUMMARY	'l'    /* generate a procmail-compatible log summary */
#define FM_SPLIT	's'				      /* split it up */
#define FM_NOWAIT	'n'		      /* don't wait for the programs */
#define FM_EVERY	'e'	/* don't require empty lines leading headers */
#define FM_MINFIELDS	'm'    /* the number of fields that have to be found */
#define DEFminfields	2	    /* before a header is recognised as such */
#define FM_DIGEST	'd'				 /* split up digests */
#define FM_BABYL	'B'		/* split up BABYL format rmail files */
#define FM_QUIET	'q'					 /* be quiet */
#define FM_DUPLICATE	'D'		/* return success on duplicate mails */
#define FM_EXTRACT	'x'			   /* extract field contents */
#define FM_EXTRC_KEEP	'X'				    /* extract field */
#define FM_ADD_IFNOT	'a'		 /* add a field if not already there */
#define FM_ADD_ALWAYS	'A'		       /* add this field in any case */
#define FM_REN_INSERT	'i'			/* rename and insert a field */
#define FM_DEL_INSERT	'I'			/* delete and insert a field */
#define FM_FIRST_UNIQ	'u'		    /* preserve the first occurrence */
#define FM_LAST_UNIQ	'U'		     /* preserve the last occurrence */
#define FM_ReNAME	'R'				   /* rename a field */
#define FM_VERSION	VERSIONOPT		/* option to display version */
#define FM_USAGE	"\
Usage: formail [-vbczfrktqY] [-D nnn idcache] [-p prefix] [-l folder]\n\
\t[-xXaAiIuU field] [-R ofield nfield]\n\
   Or: formail [+nnn] [-nnn] [-bczfrktedqBY] [-D nnn idcache] [-p prefix]\n\
\t[-n [nnn]] [-m nnn] [-l folder] [-xXaAiIuU field] [-R ofield nfield]\n\
\t-s [prg [arg ...]]\n"	    /* split up FM_HELP, token too long for some ccs */
#define FM_HELP		\
 " -v\t\tdisplay the version number and exit\
\n -b\t\tdon't escape bogus mailbox headers\
\n -Y\t\tBerkeley format mailbox, disregard Content-Length:\
\n -c\t\tconcatenate continued header-fields\
\n -z\t\tzap whitespace and empty header-fields\
\n -f\t\tforce formail to pass along any non-mailbox format\
\n -r\t\tgenerate an auto-reply header, preserve fields with -i\
\n -k\t\ton auto-reply keep the body, prevent escaping with -b\
\n -t\t\treply to the header sender instead of the envelope sender\
\n -l folder\tgenerate a procmail-compatible log summary\
\n -D nnn idcache\tdetect duplicates with an idcache of length nnn\
\n -s prg arg\tsplit the mail, startup prg for every message\n"
#define FM_HELP2	\
 " +nnn\t\tskip the first nnn\t-nnn\toutput at most nnn messages\
\n -n [nnn]\tdon't serialise splits\t-e\tempty lines are optional\
\n -d\t\taccept digest format\t-B\texpect BABYL rmail format\
\n -q\t\tbe quiet\t\t-p prefix\tquotation prefix\
\n -m nnn \tmin fields threshold (default 2) for start of message\
\n -x field\textract contents\t-X field\textract fully\
\n -a field\tadd if not present\t-A field\tadd in any case\
\n -i field\trename and insert\t-I field\tdelete and insert\
\n -u field\tfirst unique\t\t-U field\tlast unique\
\n -R oldfield newfield\trename\n"
