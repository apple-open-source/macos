/* A sed script generator (for transmogrifying the man pages automagically) */

/*$Id: manconf.c,v 1.1.1.1 1999/09/23 17:30:07 wsanchez Exp $*/

#include "../patchlevel.h"
#include "procmail.h"
#include "lastdirsep.h"

#define pn(name,val)	pnr(name,(long)(val))

static char pm_version[]=VERSION,ffileno[]=DEFfileno;
static int lines;
const char dirsep[]=DIRSEP,pmrc[]=PROCMAILRC;
char pmrc2[]=PROCMAILRC;			     /* need a writable copy */
static const char*const keepenv[]=KEEPENV,*const prestenv[]=PRESTENV,
 *const trusted_ids[]=TRUSTED_IDS,*const etcrc=ETCRC,
 *const krnllocks[]={
#ifndef NOfcntl_lock
  "\1.BR fcntl (2)",
#endif
#ifdef USElockf
  "\1.BR lockf (3)",
#endif
#ifdef USEflock
  "\1.BR flock (2)",
#endif
  0};

static char*skltmark(nl,current)char**current;
{ char*from= *current,*p;
  while(nl--)					 /* skip some newlines first */
     from=strchr(from,'\n')+1;
  while(*from=='\t')
     from++;
  *(p=strchr(from,'\n'))='\0';*current=p+1;
  return from;
}

static void putcesc(i)
{ switch(i)
   { case '\\':i='e';
	goto twoesc;
     case '\1':i='\n';lines--;		    /* \1 doubles for nroff newlines */
	goto singesc;
     case '\2':i='\\';			 /* \2 doubles for nroff backslashes */
	goto singesc;
     case '\t':i='t';
	goto fin;
     case '\n':i='n';lines--;
fin:	putchar('\\');putchar('\\');
twoesc: putchar('\\');
singesc:
     case '&':case '/':putchar('\\');
   }
  putchar(i);
}

static void putsesc(a)const char*a;
{ int c,k;
  for(c=0;;putcesc(c=k))
     switch(k= *a++)
      { case '\0':
	   return;
	case '|':case ':':
	   if(c!=' ')	 /* only insert these if there wasn't a space before */
	      printf("\\\\h'-\\\\w' 'u' ");		 /* breaking nospace */
      }
}

const char*const*gargv;

static void pname(name)const char*const name;
{ static unsigned filecount;
  static char*filebuf;
  if(!filebuf&&!(filebuf=malloc(strlen(*gargv)+1+4+1)))
     exit(EX_OSERR);
  if(lines<=0)
   { sprintf(filebuf,"%s.%04d",*gargv,filecount++);freopen(filebuf,"w",stdout);
     lines=64;				  /* POSIX says commands are limited */
   }		      /* some !@#$%^&*() implementations limit lines instead */
  putchar('s');putchar('/');putchar('@');putsesc(name);putchar('@');
  putchar('/');
}

static void pnr(name,value)const char*const name;const long value;
{ pname(name);printf("%ld/g\n",value);lines--;
}

static void putsg P((void))
{ puts("/g");lines--;
}

static void plist(name,preamble,list,postamble,ifno,andor)
 const char*const name,*const preamble,*const postamble,*const ifno,
 *const andor;const char*const*list;
{ pname(name);
  if(!*list)
     putsesc(ifno);
  else
   { putsesc(preamble);
     goto jin;
     do
      { putsesc(list[1]?", ":andor);
jin:	putsesc(*list);
      }
     while(*++list);
     putsesc(postamble);
   }
  putsg();
}

static void ps(name,value)const char*const name,*const value;
{ pname(name);putsesc(value);putsg();
}

static void pc(name,value)const char*const name;const int value;
{ pname(name);putcesc(value);putsg();
}

main(argc,argv)const char*const argv[];
{ char*p;
  gargv=argv+1;
#ifdef CF_no_procmail_yet
  ps("CF_procmail","If procmail is\1\
.I not\1\
installed globally as the default mail delivery agent (ask your system \
administrator), you have to make sure it is invoked when your mail arrives.");
#else
  ps("CF_procmail","Instead of using the system provided invocation of \
procmail when mail arrives, you can control the invocation of procmail \
yourself.");
#endif
#ifndef MAILBOX_SEPARATOR
  ps("DOT_FORWARD",".forward");
#ifdef buggy_SENDMAIL
  ps("FW_content","\"|IFS=' '&&p=@BINDIR@/procmail&&test -f $p&&exec $p -Yf-||\
exit 75 \2fB#\2fP\2fIYOUR_USERNAME\2fP\"");
#else
  ps("FW_content","\"|exec @BINDIR@/procmail\"");
#endif
#else
  ps("DOT_FORWARD",".maildelivery");
  ps("FW_content","* - | ? \"IFS=' '&&p=@BINDIR@/procmail&&test -f $p&&\
exec $p||exit 75 \2fB#\2fP\2fIYOUR_USERNAME\2fP\"");
#endif
  plist("PRESTENV",
   "\1.na\1.PP\1Other cleared or preset environment variables are ",
   prestenv,".\1.ad",""," and ");
  plist("KEEPENV",", except for the value of ",keepenv,"",""," and ");
  plist("TRUSTED_IDS",
  "  If procmail is not invoked with one of the following user or group ids: ",
   trusted_ids,", but still has to generate or accept a new `@FROM@' line,\1\
it will generate an additional `@FAKE_FIELD@' line to help distinguish\1\
fake mails.",""," or ");
  plist("KERNEL_LOCKING",
   "consistently uses the following kernel locking strategies:",krnllocks,"",
   "doesn't use any additional kernel locking strategies","\1and");
#ifdef RESTRICT_EXEC
  ps("RESTRICT_EXEC","\1.PP\1Users with userids >= @RESTRICT_EXEC_ID@ are\1\
prevented from executing external programs from\1\
within their own rcfiles");
  pn("RESTRICT_EXEC_ID",RESTRICT_EXEC);
  ps("WARN_RESTRICT_EXEC","\1.TP\1No permission to execute \"x\"\1\
An attempt to execute a program from within the rcfile was blocked.");
#else
  ps("RESTRICT_EXEC","");
  ps("WARN_RESTRICT_EXEC","");
#endif
#ifdef LD_ENV_FIX
  ps("LD_ENV_FIX","\1.PP\1For security reasons, procmail will wipe out all\
 environment variables starting with LD_ upon startup.");
#else
  ps("LD_ENV_FIX","");
#endif
  ps("MAILSPOOLDIR",MAILSPOOLDIR);
  ps("ETCRC_desc",etcrc?"\1.PP\1If no rcfiles and no\1.B \2-@PRESERVOPT@\1have\
 been specified on the command line, procmail will, prior to reading\
 @PROCMAILRC@, interpret commands from\1.B @ETCRC@\1(if present).\1\
Care must be taken when creating @ETCRC@, because, if circumstances\
 permit, it will be executed with root privileges (contrary to the\
 @PROCMAILRC@ file of course).":"");
  ps("ETCRC_files",etcrc?"\1.TP\1.B @ETCRC@\1initial global rcfile":"");
  ps("DROPPRIVS",etcrc?"\1.TP\1.B DROPPRIVS\1If set to `yes' procmail\
 will drop all privileges it might have had (suid or sgid).  This is\
 only useful if you want to guarantee that the bottom half of the\
 @ETCRC@ file is executed on behalf of the recipient.":"");
  ps("ETCRC_warn",etcrc?"\1.PP\1The\1.B @ETCRC@\1file might be executed\
 with root privileges, so be very careful of what you put in it.\1\
.B SHELL\1\
will be equal to that of the current recipient, so if procmail has to invoke\
 the shell, you'd better set it to some safe value first.\1\
See also:\1.BR DROPPRIVS .":"");
  ps("ETCRC",etcrc?etcrc:"");
#ifdef ETCRCS
  ps("ETCRCS_desc","\1If the rcfile is an absolute path starting with\
\1.B @ETCRCS@\
\1without backward references (i.e. the parent directory cannot\
 be mentioned) procmail will, only if no security violations are found,\
 take on the identity of the owner of the rcfile (or symbolic link).");
  ps("ETCRCS_files","\1.TP\1.B @ETCRCS@\1special privileges path for rcfiles");
  ps("ETCRCS_warn","\1.PP\1Keep in mind that if\1.BR chown (1)\1is permitted\
 on files in\1.BR @ETCRCS@ ,\1that they can be chowned to root\
 (or anyone else) by their current owners.\1For maximum security, make\
 sure this directory is\1.I executable\1to root only.");
  ps("ETCRCS_error","\1.TP\1Denying special privileges for \"x\"\1\
Procmail will not take on the identity that comes with the rcfile because\1\
a security violation was found (e.g. \1.B \2-@PRESERVOPT@\1or variable\
 assignments on the command line) or procmail had insufficient privileges\
 to do so.");
  ps("ETCRCS",ETCRCS);
#else
  ps("ETCRCS_desc","");ps("ETCRCS_files","");ps("ETCRCS_warn","");
  ps("ETCRCS_error","");
#endif
#ifdef console
  ps("pconsole","appear on\1.BR ");
  ps("console",console);
  ps("aconsole"," .");
#else
  ps("pconsole","be mailed back to the ");
  ps("console","sender");
  ps("aconsole",".");
#endif
  pname("INIT_UMASK");printf("0%lo/g\n",(unsigned long)INIT_UMASK);lines--;
  pn("DEFlinebuf",DEFlinebuf);
  ps("BOGUSprefix",BOGUSprefix);
  ps("FAKE_FIELD",FAKE_FIELD);
  ps("PROCMAILRC",pmrc);
  pn("HOSTNAMElen",HOSTNAMElen);
  pn("DEFsuspend",DEFsuspend);
  pn("DEFlocksleep",DEFlocksleep);
  ps("TO_key",TO_key);
  ps("TO_substitute",TO_substitute);
  ps("TOkey",TOkey);
  ps("TOsubstitute",TOsubstitute);
  ps("FROMDkey",FROMDkey);
  ps("FROMDsubstitute",FROMDsubstitute);
  ps("FROMMkey",FROMMkey);
  ps("FROMMsubstitute",FROMMsubstitute);
  ps("DEFshellmetas",DEFshellmetas);
  *lastdirsep(pmrc2)='\0';
  ps("DEFmaildir",pmrc2);
  ps("DEFdefault",DEFdefault);
  ps("DEFmsgprefix",DEFmsgprefix);
  ps("DEFsendmail",DEFsendmail);
  ps("DEFflagsendmail",DEFflagsendmail);
  ps("DEFlockext",DEFlockext);
  ps("DEFshellflags",DEFshellflags);
  pn("DEFlocktimeout",DEFlocktimeout);
  pn("DEFtimeout",DEFtimeout);
  pn("DEFnoresretry",DEFnoresretry);
  ps("MATCHVAR",MATCHVAR);
  ps("COMSAThost",COMSAThost);
  ps("COMSATservice",COMSATservice);
  ps("COMSATprotocol",COMSATprotocol);
  ps("COMSATxtrsep",COMSATxtrsep);
  pc("SERV_ADDRsep",SERV_ADDRsep);
  ps("DEFcomsat",DEFcomsat);
  ps("BinSh",BinSh);
  ps("RootDir",RootDir);
  pc("MCDIRSEP",*MCDIRSEP);
  pc("chCURDIR",chCURDIR);
  pc("HELPOPT1",HELPOPT1);
  pc("HELPOPT2",HELPOPT2);
  pc("VERSIONOPT",VERSIONOPT);
  pc("PRESERVOPT",PRESERVOPT);
  pc("TEMPFAILOPT",TEMPFAILOPT);
  pc("MAILFILTOPT",MAILFILTOPT);
  pc("FROMWHOPT",FROMWHOPT);
  pc("REFRESH_TIME",REFRESH_TIME);
  pc("ALTFROMWHOPT",ALTFROMWHOPT);
  pc("OVERRIDEOPT",OVERRIDEOPT);
  pc("BERKELEYOPT",BERKELEYOPT);
  pc("ARGUMENTOPT",ARGUMENTOPT);
  pc("DELIVEROPT",DELIVEROPT);
  pn("MINlinebuf",MINlinebuf);
  ps("FROM",FROM);
  pc("HEAD_GREP",RECFLAGS[HEAD_GREP]);
  pc("BODY_GREP",RECFLAGS[BODY_GREP]);
  pc("DISTINGUISH_CASE",RECFLAGS[DISTINGUISH_CASE]);
  pc("ALSO_NEXT_RECIPE",RECFLAGS[ALSO_NEXT_RECIPE]);
  pc("ALSO_N_IF_SUCC",RECFLAGS[ALSO_N_IF_SUCC]);
  pc("ELSE_DO",RECFLAGS[ELSE_DO]);
  pc("ERROR_DO",RECFLAGS[ERROR_DO]);
  pc("PASS_HEAD",RECFLAGS[PASS_HEAD]);
  pc("PASS_BODY",RECFLAGS[PASS_BODY]);
  pc("FILTER",RECFLAGS[FILTER]);
  pc("CONTINUE",RECFLAGS[CONTINUE]);
  pc("WAIT_EXIT",RECFLAGS[WAIT_EXIT]);
  pc("WAIT_EXIT_QUIET",RECFLAGS[WAIT_EXIT_QUIET]);
  pc("IGNORE_WRITERR",RECFLAGS[IGNORE_WRITERR]);
  pc("RAW_NONL",RECFLAGS[RAW_NONL]);
  ps("FROM_EXPR",FROM_EXPR);
  pc("UNIQ_PREFIX",UNIQ_PREFIX);
  ps("ESCAP",ESCAP);
  ps("UNKNOWN",UNKNOWN);
  ps("OLD_PREFIX",OLD_PREFIX);
  ps("DEFfileno",ffileno+LEN_FILENO_VAR);
  ffileno[LEN_FILENO_VAR-1]='\0';
  ps("FILENO",ffileno);
  pc("FM_SKIP",FM_SKIP);
  pc("FM_TOTAL",FM_TOTAL);
  pc("FM_BOGUS",FM_BOGUS);
  pc("FM_BERKELEY",FM_BERKELEY);
  pc("FM_QPREFIX",FM_QPREFIX);
  pc("FM_CONCATENATE",FM_CONCATENATE);
  pc("FM_ZAPWHITE",FM_ZAPWHITE);
  pc("FM_FORCE",FM_FORCE);
  pc("FM_REPLY",FM_REPLY);
  pc("FM_KEEPB",FM_KEEPB);
  pc("FM_TRUST",FM_TRUST);
  pc("FM_SPLIT",FM_SPLIT);
  pc("FM_NOWAIT",FM_NOWAIT);
  pc("FM_EVERY",FM_EVERY);
  pc("FM_MINFIELDS",FM_MINFIELDS);
  pn("DEFminfields",DEFminfields);
  pc("FM_DIGEST",FM_DIGEST);
  pc("FM_BABYL",FM_BABYL);
  pc("FM_QUIET",FM_QUIET);
  pc("FM_DUPLICATE",FM_DUPLICATE);
  pc("FM_EXTRACT",FM_EXTRACT);
  pc("FM_EXTRC_KEEP",FM_EXTRC_KEEP);
  pc("FM_ADD_IFNOT",FM_ADD_IFNOT);
  pc("FM_ADD_ALWAYS",FM_ADD_ALWAYS);
  pc("FM_REN_INSERT",FM_REN_INSERT);
  pc("FM_DEL_INSERT",FM_DEL_INSERT);
  pc("FM_FIRST_UNIQ",FM_FIRST_UNIQ);
  pc("FM_LAST_UNIQ",FM_LAST_UNIQ);
  pc("FM_ReNAME",FM_ReNAME);
  pc("FM_VERSION",FM_VERSION);
  pn("EX_OK",EXIT_SUCCESS);
  *(p=strchr(strchr(strchr(pm_version,' ')+1,' ')+1,' '))='\0';p++;
  ps("PM_VERSION",PM_VERSION);
  ps("MY_MAIL_ADDR",skltmark(1,&p));
#if 0
  ps("MY_ALT_MAIL_ADDR",skltmark(0,&p));
#endif
  ps("PM_MAILINGLIST",skltmark(2,&p));
  ps("PM_MAILINGLISTR",skltmark(2,&p));
  ps("BINDIR",BINDIR);
#ifdef NOpow
  pc("POW",'1');
#else
  pc("POW",'x');
#endif
  ps("SETRUID",setRuid(getuid())?"":	/* is setruid() a valid system call? */
   " (or if procmail is already running with the recipient's euid and egid)");
  return EXIT_SUCCESS;
}
