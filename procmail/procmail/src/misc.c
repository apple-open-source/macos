/************************************************************************
 *	Miscellaneous routines used by procmail				*
 *									*
 *	Copyright (c) 1990-1999, S.R. van den Berg, The Netherlands	*
 *	#include "../README"						*
 ************************************************************************/
#ifdef RCS
static /*const*/char rcsid[]=
 "$Id: misc.c,v 1.1.1.1 1999/09/23 17:30:07 wsanchez Exp $";
#endif
#include "procmail.h"
#include "acommon.h"
#include "sublib.h"
#include "robust.h"
#include "misc.h"
#include "pipes.h"
#include "common.h"
#include "cstdio.h"
#include "exopen.h"
#include "regexp.h"
#include "mcommon.h"
#include "goodies.h"
#include "locking.h"
#include "../patchlevel.h"
#ifndef NO_COMSAT
#include "network.h"
#endif
#include "mailfold.h"
#include "lastdirsep.h"
#include "authenticate.h"

struct varval strenvvar[]={{"LOCKSLEEP",DEFlocksleep},
 {"LOCKTIMEOUT",DEFlocktimeout},{"SUSPEND",DEFsuspend},
 {"NORESRETRY",DEFnoresretry},{"TIMEOUT",DEFtimeout},{"VERBOSE",DEFverbose},
 {"LOGABSTRACT",DEFlogabstract}};
struct varstr strenstr[]={{"SHELLMETAS",DEFshellmetas},{"LOCKEXT",DEFlockext},
 {"MSGPREFIX",DEFmsgprefix},{"COMSAT",""},{"TRAP",""},
 {"SHELLFLAGS",DEFshellflags},{"DEFAULT",DEFdefault},{"SENDMAIL",DEFsendmail},
 {"SENDMAILFLAGS",DEFflagsendmail},{"PROCMAIL_VERSION",PM_VERSION}};

#define MAXvarvals	 maxindex(strenvvar)
#define MAXvarstrs	 maxindex(strenstr)

const char lastfolder[]="LASTFOLDER",maildir[]="MAILDIR",slinebuf[]="LINEBUF";
int didchd;
char*globlock;
static time_t oldtime;
static int fakedelivery;
		       /* line buffered to keep concurrent entries untangled */
void elog(newt)const char*const newt;
{ int lnew;size_t i;static int lold;static char*old;char*p;
#ifndef O_CREAT
  lseek(STDERR,(off_t)0,SEEK_END);	  /* locking should be done actually */
#endif
  if(!(lnew=strlen(newt))||nextexit)			     /* force flush? */
     goto flush;
  i=lold+lnew;
  if(p=lold?realloc(old,i):malloc(i))			 /* unshelled malloc */
   { memmove((old=p)+lold,newt,(size_t)lnew);			   /* append */
     if(p[(lold=i)-1]=='\n')					     /* EOL? */
	rwrite(STDERR,p,(int)i),lold=0,free(p);		/* flush the line(s) */
   }
  else						   /* no memory, force flush */
flush:
   { if(lold)
      { rwrite(STDERR,old,lold);lold=0;
	if(!nextexit)
	   free(old);			/* don't use free in signal handlers */
      }
     if(lnew)
	rwrite(STDERR,newt,lnew);
   }
}

#include "shell.h"

void ignoreterm P((void))
{ signal(SIGTERM,SIG_IGN);signal(SIGHUP,SIG_IGN);signal(SIGINT,SIG_IGN);
  signal(SIGQUIT,SIG_IGN);
}

void shutdesc P((void))
{ rclose(savstdout);closelog();closerc();
}

void setids P((void))
{ if(rcstate!=rc_NORMAL)
   { if(setRgid(gid))	/* due to these !@#$%^&*() POSIX semantics, setgid() */
	setgid(gid);	   /* sets the saved gid as well; we can't use that! */
     setruid(uid);setuid(uid);setegid(gid);rcstate=rc_NORMAL;
#if !DEFverbose
     verbose=0;			/* to avoid peeking in rcfiles using SIGUSR1 */
#endif
   }
}

void writeerr(line)const char*const line;
{ nlog(errwwriting);logqnl(line);
}

int forkerr(pid,a)const pid_t pid;const char*const a;
{ if(pid==-1)
   { nlog("Failed forking");logqnl(a);
     return 1;
   }
  return 0;
}

void progerr(line,xitcode)const char*const line;int xitcode;
{ charNUM(num,thepid);
  nlog("Program failure (");ltstr(0,(long)xitcode,num);elog(num);elog(") of");
  logqnl(line);
}

void chderr(dir)const char*const dir;
{ nlog("Couldn't chdir to");logqnl(dir);
}

void readerr(file)const char*const file;
{ nlog("Couldn't read");logqnl(file);
}

void verboff P((void))
{ verbose=0;
#ifdef SIGUSR1
  qsignal(SIGUSR1,verboff);
#endif
}

void verbon P((void))
{ verbose=1;
#ifdef SIGUSR2
  qsignal(SIGUSR2,verbon);
#endif
}

void yell(a,b)const char*const a,*const b;		/* log if VERBOSE=on */
{ if(verbose)
     nlog(a),logqnl(b);
}

void newid P((void))
{ thepid=getpid();oldtime=0;
}

void zombiecollect P((void))
{ while(waitpid((pid_t)-1,(int*)0,WNOHANG)>0);	      /* collect any zombies */
}

void nlog(a)const char*const a;
{ time_t newtime;
  static const char colnsp[]=": ";
  elog(procmailn);elog(colnsp);
  if(verbose&&oldtime!=(newtime=time((time_t*)0)))
   { charNUM(num,thepid);
     elog("[");oldtime=newtime;ultstr(0,(unsigned long)thepid,num);elog(num);
     elog("] ");elog(ctime(&oldtime));elog(procmailn);elog(colnsp);
   }
  elog(a);
}

void logqnl(a)const char*const a;
{ elog(oquote);elog(a);elog(cquote);
}

void skipped(x)const char*const x;
{ if(*x)
     nlog("Skipped"),logqnl(x);
}

int nextrcfile P((void))	/* next rcfile specified on the command line */
{ const char*p;int rval=2;
  while(p= *gargv)
   { gargv++;
     if(!strchr(p,'='))
      { if(strlen(p)>linebuf-1)
	 { nlog("Excessively long rcfile path skipped\n");
	   continue;
	 }
	rcfile=p;
	return rval;
      }
     rval=1;			       /* not the first argument encountered */
   }
  return 0;
}

void onguard P((void))
{ lcking|=lck_LOCKFILE;
}

void offguard P((void))
{ lcking&=~lck_LOCKFILE;
  if(nextexit==1)	  /* make sure we are not inside Terminate() already */
     elog(newline),Terminate();
}

void sterminate P((void))
{ static const char*const msg[]={"memory","fork",	  /* crosscheck with */
   "a file descriptor","a kernel-lock"};	  /* lck_ defs in procmail.h */
  ignoreterm();
  if(pidchild>0)	    /* don't kill what is not ours, we might be root */
     kill(pidchild,SIGTERM);
  if(!nextexit)
   { nextexit=1;nlog("Terminating prematurely");
     if(!(lcking&lck_LOCKFILE))
      { register unsigned i,j;
	if(i=(lcking&~(lck_ALLOCLIB|lck_LOCKFILE))>>1)
	 { elog(whilstwfor);
	   for(j=0;!((i>>=1)&1);j++);
	   elog(msg[j]);
	 }
	elog(newline);Terminate();
      }
   }
}

void Terminate P((void))
{ const char*chp;
  ignoreterm();
  if(retvl2!=EXIT_SUCCESS)
     fakedelivery=0,retval=retvl2;
  if(getpid()==thepid)
   { const char*lstfolder;
     if(retval!=EXIT_SUCCESS)
      { tofile=0;lasttell= -1;			  /* mark it for logabstract */
	lstfolder=fakedelivery?"**Lost**":
	 retval==EX_TEMPFAIL?"**Requeued**":"**Bounced**";
      }
     else
	lstfolder=tgetenv(lastfolder);
     logabstract(lstfolder);
#ifndef NO_COMSAT
     if(strlen(chp=tgetenv(lgname))+2<=linebuf)	  /* first pass length check */
      { int s;struct sockaddr_in addr;char*chad;	     /* @ seperator? */
	cat(chp,"@");			     /* start setting up the message */
	if(chad=strchr(chp=(char*)scomsat,SERV_ADDRsep))
	   *chad++='\0';	      /* split it up in service and hostname */
	else if(!renvint(-1L,chp))		/* or is it a false boolean? */
	   goto nocomsat;			       /* ok, no comsat then */
	else
	   chp="";			  /* set to yes, so take the default */
	if(!chad||!*chad)					  /* no host */
#ifndef IP_localhost
	   chad=COMSAThost;				      /* use default */
#else /* IP_localhost */
	 { static const unsigned char ip_localhost[]=IP_localhost;
	   addr.sin_family=AF_INET;
	   tmemmove(&addr.sin_addr,ip_localhost,sizeof ip_localhost);
	 }
	else
#endif /* IP_localhost */
	 { const struct hostent*host;	      /* what host?  paranoid checks */
	   if(!(host=gethostbyname(chad))||!host->h_0addr_list)
	    { endhostent();		     /* host can't be found, too bad */
	      goto nocomsat;
	    }
	   addr.sin_family=host->h_addrtype;	     /* address number found */
	   tmemmove(&addr.sin_addr,host->h_0addr_list,host->h_length);
	   endhostent();
	 }
	if(!*chp)					       /* no service */
	   chp=BIFF_serviceport;			/* new balls please! */
	s=strtol(chp,&chad,10);
	if(chp==chad)			       /* the service is not numeric */
	 { const struct servent*serv;
	   if(!(serv=getservbyname(chp,COMSATprotocol)))   /* so get its no. */
	    { endservent();
	      goto nocomsat;
	    }
	   addr.sin_port=serv->s_port;endservent();
	 }
	else
	   addr.sin_port=htons((short)s);		    /* network order */
	if(lasttell>=0)					   /* was it a file? */
	   ultstr(0,(unsigned long)lasttell,buf2),catlim(buf2);	      /* yep */
	catlim(COMSATxtrsep);				 /* custom seperator */
	if(lasttell>=0&&!strchr(dirsep,*lstfolder))    /* relative filename? */
	   catlim(tgetenv(maildir)),catlim(MCDIRSEP_);	   /* prepend curdir */
	catlim(lstfolder);s=socket(PF_INET,SOCK_DGRAM,UDP_protocolno);
	sendto(s,buf,strlen(buf),0,(const void*)&addr,sizeof(addr));rclose(s);
	yell("Notified comsat:",buf);
      }
nocomsat:
#endif /* NO_COMSAT */
     shutdesc();
     if(!(lcking&lck_ALLOCLIB))			/* don't reenter malloc/free */
	exectrap(traps);
     nextexit=2;unlock(&loclock);unlock(&globlock);fdunlock();
   }					/* flush the logfile & exit procmail */
  elog("");exit(fakedelivery==2?EXIT_SUCCESS:retval);
}

void suspend P((void))
{ ssleep((unsigned)suspendv);
}

void app_val(sp,val)struct dyna_long*const sp;const off_t val;
{ if(sp->filled==sp->tspace)			    /* growth limit reached? */
   { if(!sp->offs)
	sp->offs=malloc(1);
     sp->offs=realloc(sp->offs,(sp->tspace+=4)*sizeof*sp->offs);   /* expand */
   }
  sp->offs[sp->filled++]=val;				     /* append to it */
}

int alphanum(c)const unsigned c;
{ return numeric(c)||c-'a'<='z'-'a'||c-'A'<='Z'-'A'||c=='_';
}

char*pmrc2buf P((void))
{ sgetcp=pmrc;
  if(readparse(buf,sgetc,2))
     buf[0]='\0';
  return buf;
}

void setmaildir(newdir)const char*const newdir;		    /* destroys buf2 */
{ char*chp;
  didchd=1;*(chp=strcpy(buf2,maildir)+STRLEN(maildir))='=';
  strcpy(++chp,newdir);sputenv(buf2);
}

void setoverflow P((void))
{ sputenv("PROCMAIL_OVERFLOW=yes");
}

void srequeue P((void))
{ retval=EX_TEMPFAIL;sterminate();
}

void slose P((void))
{ fakedelivery=2;sterminate();
}

void sbounce P((void))
{ retval=EX_CANTCREAT;sterminate();
}

void catlim(src)register const char*src;
{ register char*dest=buf;register size_t lim=linebuf;
  while(lim&&*dest)
     dest++,lim--;
  if(lim)
   { while(--lim&&(*dest++= *src++));
     *dest='\0';
   }
}

void setdef(name,contents)const char*const name,*const contents;
{ strcat(strcat(strcpy((char*)(sgetcp=buf2),name),"="),contents);
  if(!readparse(buf,sgetc,2))
     sputenv(buf);
}

void metaparse(p)const char*p;				    /* result in buf */
{ if(sh=!!strpbrk(p,shellmetas))
     strcpy(buf,p);			 /* copy literally, shell will parse */
  else
   { sgetcp=p=tstrdup(p);
     if(readparse(buf,sgetc,0)				/* parse it yourself */
#ifndef GOT_bin_test
	||!strcmp(test,buf)
#endif
	)
	strcpy(buf,p),sh=1;		   /* oops, overflow or `test' found */
     free((char*)p);
   }
}

void concatenate(p)register char*p;
{ while(p!=Tmnate)			  /* concatenate all other arguments */
   { while(*p++);
     p[-1]=' ';
   }
  *p=p[-1]='\0';
}

char*cat(a,b)const char*const a,*const b;
{ return strcat(strcpy(buf,a),b);
}

char*tstrdup(a)const char*const a;
{ int i;
  i=strlen(a)+1;
  return tmemmove(malloc(i),a,i);
}

const char*tgetenv(a)const char*const a;
{ const char*b;
  return (b=getenv(a))?b:"";
}

char*cstr(a,b)char*const a;const char*const b;	/* dynamic buffer management */
{ if(a)
     free(a);
  return tstrdup(b);
}

void setlastfolder(folder)const char*const folder;
{ if(asgnlastf)
   { char*chp;
     asgnlastf=0;
     strcpy(chp=malloc(STRLEN(lastfolder)+1+strlen(folder)+1),lastfolder);
     chp[STRLEN(lastfolder)]='=';strcpy(chp+STRLEN(lastfolder)+1,folder);
     sputenv(chp);free(chp);
   }
}

char*gobenv(chp,end)char*chp,*end;
{ int found,i;
  found=0;end--;
  if(alphanum(i=getb())&&!numeric(i))
     for(found=1;*chp++=i,chp<end&&alphanum(i=getb()););
  *chp='\0';ungetb(i);
  if(chp==end)							 /* overflow */
   { nlog(exceededlb);setoverflow();
     return end+1;
   }
  switch(i)
   { case ' ':case '\t':case '\n':case '=':
	if(found)
	   return chp;
   }
  return 0;
}

int asenvcpy(src)char*src;
{ const char*chp;
  if(chp=strchr(src,'='))			     /* is it an assignment? */
    /*
     *	really change the uid now, since it would not be safe to
     *	evaluate the extra command line arguments otherwise
     */
   { erestrict=1;setids();strcpy(buf,src);src=buf+(chp-src);
     strcpy((char*)(sgetcp=buf2),++src);
     if(!readparse(src,sgetc,2))
      { chp=sputenv(buf);src[-1]='\0';
	asenv(chp);
      }
     return 1;
   }
  return 0;
}

void mallocbuffers(linebuf)size_t linebuf;
{ if(buf)
   { free(buf);
     free(buf2);
   }
  buf=malloc(linebuf);buf2=malloc(linebuf);
}

void asenv(chp)const char*const chp;
{ static const char logfile[]="LOGFILE",Log[]="LOG",sdelivered[]="DELIVERED",
   includerc[]="INCLUDERC",eumask[]="UMASK",dropprivs[]="DROPPRIVS",
   shift[]="SHIFT";
  if(!strcmp(buf,slinebuf))
   { if((linebuf=renvint(0L,chp)+XTRAlinebuf)<MINlinebuf+XTRAlinebuf)
	linebuf=MINlinebuf+XTRAlinebuf;		       /* check minimum size */
     mallocbuffers(linebuf);
   }
  else if(!strcmp(buf,maildir))
     if(chdir(chp))
	chderr(chp);
     else
	didchd=1;
  else if(!strcmp(buf,logfile))
     opnlog(chp);
  else if(!strcmp(buf,Log))
     elog(chp);
  else if(!strcmp(buf,exitcode))
     setxit=1;
  else if(!strcmp(buf,shift))
   { int i;
     if((i=renvint(0L,chp))>0)
      { if(i>crestarg)
	   i=crestarg;
	crestarg-=i;restargv+=i;		     /* shift away arguments */
      }
   }
  else if(!strcmp(buf,dropprivs))			  /* drop privileges */
   { if(renvint(0L,chp))
      { if(verbose)
	   nlog("Assuming identity of the recipient, VERBOSE=off\n");
	setids();
      }
   }
  else if(!strcmp(buf,sdelivered))			    /* fake delivery */
   { if(renvint(0L,chp))				    /* is it really? */
      { onguard();
	if((thepid=sfork())>0)			/* signals may cause trouble */
	   nextexit=2,lcking&=~lck_LOCKFILE,exit(retvl2);
	if(!forkerr(thepid,procmailn))
	   fakedelivery=1;
	newid();offguard();
      }
   }
  else if(!strcmp(buf,lockfile))
     lockit(tstrdup((char*)chp),&globlock);
  else if(!strcmp(buf,eumask))
     doumask((mode_t)strtol(chp,(char**)0,8));
  else if(!strcmp(buf,includerc))
     pushrc(chp);
  else if(!strcmp(buf,host))
   { const char*name;
     if(strcmp(chp,name=hostname()))
      { yell("HOST mismatched",name);
	if(rc<0||!nextrcfile())			  /* if no rcfile opened yet */
	   retval=EXIT_SUCCESS,Terminate();	  /* exit gracefully as well */
	closerc();
      }
   }
  else
   { int i=MAXvarvals;
     do					      /* several numeric assignments */
	if(!strcmp(buf,strenvvar[i].name))
	   strenvvar[i].val=renvint(strenvvar[i].val,chp);
     while(i--);
     i=MAXvarstrs;
     do						 /* several text assignments */
	if(!strcmp(buf,strenstr[i].sname))
	   strenstr[i].sval=chp;
     while(i--);
   }
}

long renvint(i,env)const long i;const char*const env;
{ const char*p;long t;
  t=strtol(env,(char**)&p,10);			  /* parse like a decimal nr */
  if(p==env)
   { for(;;p++)					  /* skip leading whitespace */
      { switch(*p)
	 { case '\t':case ' ':
	      continue;
	 }
	break;
      }
     t=i;
     if(!strnIcmp(p,"a",(size_t)1))
	t=2;
     else if(!strnIcmp(p,"on",(size_t)2)||!strnIcmp(p,"y",(size_t)1)||
      !strnIcmp(p,"t",(size_t)1)||!strnIcmp(p,"e",(size_t)1))
	t=1;
     else if(!strnIcmp(p,"off",(size_t)3)||!strnIcmp(p,"n",(size_t)1)||
      !strnIcmp(p,"f",(size_t)1)||!strnIcmp(p,"d",(size_t)1))
	t=0;
   }
  return t;
}

void squeeze(target)char*target;
{ int state;char*src;
  for(state=0,src=target;;target++,src++)
   { switch(*target= *src)
      { case '\n':
	   if(state==1)
	      target-=2;			     /* throw out \ \n pairs */
	   state=2;
	   continue;
	case '\\':state=1;
	   continue;
	case ' ':case '\t':
	   if(state==2)					     /* skip leading */
	    { target--;					       /* whitespace */
	      continue;
	    }
	default:state=0;
	   continue;
	case '\0':;
      }
     break;
   }
}

char*egrepin(expr,source,len,casesens)char*expr;const char*source;
 const long len;int casesens;
{ if(*expr)		 /* only do the search if the expression is nonempty */
   { source=(const char*)bregexec((struct eps*)(expr=(char*)
      bregcomp(expr,!casesens)),(const uchar*)source,(const uchar*)source,
      len>0?(size_t)len:(size_t)0,!casesens);
     free(expr);
   }
  return (char*)source;
}

int enoughprivs(passinvk,euid,egid,uid,gid)const auth_identity*const passinvk;
 const uid_t euid,uid;const gid_t egid,gid;
{ return euid==ROOT_uid||
   passinvk&&auth_whatuid(passinvk)==uid||
   euid==uid&&egid==gid;
}

void initdefenv P((void))
{ int i=MAXvarstrs;
  do	   /* initialise all non-empty string variables into the environment */
     if(*strenstr[i].sval)
	setdef(strenstr[i].sname,strenstr[i].sval);
  while(i--);
}

const char*newdynstring(adrp,chp)struct dynstring**const adrp;
 const char*const chp;
{ struct dynstring*curr;size_t len;
  curr=malloc(ioffsetof(struct dynstring,ename[0])+(len=strlen(chp)+1));
  tmemmove(curr->ename,chp,len);curr->enext= *adrp;*adrp=curr;
  return curr->ename;
}

void rcst_nosgid P((void))
{ if(!rcstate)
     rcstate=rc_NOSGID;
}

static void inodename(stbuf,i)const struct stat*const stbuf;const size_t i;
{ static const char bogusprefix[]=BOGUSprefix;char*p;
  p=strchr(strcpy(strcpy(buf+i,bogusprefix)+STRLEN(bogusprefix),
   getenv(lgname)),'\0');
  *p++='.';ultoan((unsigned long)stbuf->st_ino,p);	  /* i-node numbered */
}
			     /* lifted out of main() to reduce main()'s size */
int screenmailbox(chp,chp2,egid,Deliverymode)
 char*chp;char*const chp2;const gid_t egid;const int Deliverymode;
{ int i;struct stat stbuf;			   /* strip off the basename */
 /*
  *	  do we need sgidness to access the mail-spool directory/files?
  */
  *chp2='\0';buf[i=lastdirsep(chp)-chp]='\0';sgid=gid;
  if(!stat(buf,&stbuf))
   { unsigned wwsdir;
     if(accspooldir=(wwsdir=			/* world writable spool dir? */
	   (stbuf.st_mode&(S_IWGRP|S_IXGRP|S_IWOTH|S_IXOTH))==
	   (S_IWGRP|S_IXGRP|S_IWOTH|S_IXOTH))
	  <<1|						 /* note it in bit 1 */
	 uid==stbuf.st_uid)	   /* we own the spool dir, note it in bit 0 */
#ifdef TOGGLE_SGID_OK
	;
#endif
	rcst_nosgid();			     /* we don't *need* setgid privs */
     if(uid!=stbuf.st_uid&&		 /* we don't own the spool directory */
	(stbuf.st_mode&S_ISGID||!wwsdir))	  /* it's not world writable */
      { if(stbuf.st_gid==egid)			 /* but we have setgid privs */
	   doumask(GROUPW_UMASK);		   /* make it group-writable */
	goto keepgid;
      }
     else if(stbuf.st_mode&S_ISGID)
keepgid:			   /* keep the gid from the parent directory */
	if((sgid=stbuf.st_gid)!=egid)
	   setgid(sgid);     /* we were started nosgid, but we might need it */
   }
  else				/* panic, mail-spool directory not available */
   { int c;				     /* try creating the last member */
     setids();c=buf[i-1];buf[i-1]='\0';mkdir(buf,NORMdirperm);buf[i-1]=c;
   }
 /*
  *	  check if the default-mailbox-lockfile is owned by the
  *	  recipient, if not, mark it for further investigation, it
  *	  might need to be removed
  */
  for(;;)
   { ;{ int mboxstat;
	static const char renbogus[]="Renamed bogus \"%s\" into \"%s\"",
	 renfbogus[]="Couldn't rename bogus \"%s\" into \"%s\"";
	;{ int goodlock;
	   if(!(goodlock=lstat(defdeflock,&stbuf)||stbuf.st_uid==uid))
	      inodename(&stbuf,i);
	  /*
	   *	  check if the original/default mailbox of the recipient
	   *	  exists, if it does, perform some security checks on it
	   *	  (check if it's a regular file, check if it's owned by
	   *	  the recipient), if something is wrong try and move the
	   *	  bogus mailbox out of the way, create the
	   *	  original/default mailbox file, and chown it to
	   *	  the recipient
	   */
	   if(lstat(chp,&stbuf))			 /* stat the mailbox */
	    { mboxstat= -(errno==EACCES);
	      goto boglock;
	    }					/* lockfile unrightful owner */
	   else
	    { mboxstat=1;
	      if(!(stbuf.st_mode&S_IWGRP))
boglock:	 if(!goodlock)		      /* try & rename bogus lockfile */
		    if(rename(defdeflock,buf))		   /* out of the way */
		       syslog(LOG_EMERG,renfbogus,defdeflock,buf);
		    else
		       syslog(LOG_ALERT,renbogus,defdeflock,buf);
	    }
	 }
	if(mboxstat>0||mboxstat<0&&(setids(),!lstat(chp,&stbuf)))
	 { int checkiter=1;
	   for(;;)
	    { if(stbuf.st_uid!=uid||		      /* recipient not owner */
		 !(stbuf.st_mode&S_IWUSR)||	     /* recipient can write? */
		 S_ISLNK(stbuf.st_mode)||		/* no symbolic links */
		 (S_ISDIR(stbuf.st_mode)?     /* directories, yes, hardlinks */
		   !(stbuf.st_mode&S_IXUSR):stbuf.st_nlink!=1))	       /* no */
		/*
		 *	If another procmail is about to create the new
		 *	mailbox, and has just made the link, st_nlink==2
		 */
		 if(checkiter--)	    /* maybe it was a race condition */
		    suspend();		 /* close eyes, and hope it improves */
		 else			/* can't deliver to this contraption */
		  { inodename(&stbuf,i);nlog("Renaming bogus mailbox \"");
		    elog(chp);elog("\" into");logqnl(buf);
		    if(rename(chp,buf))	   /* try and move it out of the way */
		     { syslog(LOG_EMERG,renfbogus,chp,buf);
		       goto fishy;  /* rename failed, something's fishy here */
		     }
		    else
		       syslog(LOG_ALERT,renbogus,chp,buf);
		    goto nobox;
		  }
	      else
		 break;
	      if(lstat(chp,&stbuf))
		 goto nobox;
	    }					/* SysV type autoforwarding? */
	   if(Deliverymode&&stbuf.st_mode&(S_ISGID|S_ISUID))
	    { nlog("Autoforwarding mailbox found\n");
	      exit(EX_NOUSER);
	    }
	   else
	    { if(!(stbuf.st_mode&OVERRIDE_MASK)&&
		 stbuf.st_mode&cumask&
		  (accspooldir?~(mode_t)0:~(S_IRGRP|S_IWGRP)))	/* hold back */
	       { static const char enfperm[]=
		  "Enforcing stricter permissions on";
		 nlog(enfperm);logqnl(chp);
		 syslog(LOG_NOTICE,slogstr,enfperm,chp);setids();
		 chmod(chp,stbuf.st_mode&=~cumask);
	       }
	      break;				  /* everything is just fine */
	    }
	 }
      }
nobox:
     if(!(accspooldir&1))	     /* recipient does not own the spool dir */
      { if(!xcreat(chp,NORMperm,(time_t*)0,doCHOWN|doCHECK))	   /* create */
	   break;		   /* mailbox... yes we could, fine, proceed */
	if(!lstat(chp,&stbuf))			     /* anything in the way? */
	   continue;			       /* check if it could be valid */
      }
     setids();						   /* try some magic */
     if(!xcreat(chp,NORMperm,(time_t*)0,doCHECK))		/* try again */
	break;
     if(lstat(chp,&stbuf))			      /* nothing in the way? */
fishy:
      { nlog("Couldn't create");logqnl(chp);
	return 0;
      }
   }
  return 1;
}
			     /* lifted out of main() to reduce main()'s size */
int conditions(flags,prevcond,lastsucc,lastcond,nrcond)const char flags[];
 const int prevcond,lastsucc,lastcond;int nrcond;
{ char*chp,*chp2,*startchar;double score;int scored,i,skippedempty;
  long tobesent;static const char suppressed[]=" suppressed\n";
  score=scored=0;
  if(flags[ERROR_DO]&&flags[ELSE_DO])
     nlog(conflicting),elog("else-if-flag"),elog(suppressed);
  if(flags[ERROR_DO]&&flags[ALSO_N_IF_SUCC])
     nlog(conflicting),elog("also-if-succeeded-flag"),elog(suppressed);
  if(nrcond<0)		      /* assume appropriate default nr of conditions */
     nrcond=!flags[ALSO_NEXT_RECIPE]&&!flags[ALSO_N_IF_SUCC]&&!flags[ELSE_DO]&&
      !flags[ERROR_DO];
  startchar=themail;tobesent=thebody-themail;
  if(flags[BODY_GREP])			       /* what needs to be egrepped? */
     if(flags[HEAD_GREP])
	tobesent=filled;
     else
      { startchar=thebody;tobesent=filled-tobesent;
	goto noconcat;
      }
   if(!skiprc)
      concon(' ');
noconcat:
   i=!skiprc;						  /* init test value */
   if(flags[ERROR_DO])
      i&=prevcond&&!lastsucc;
   if(flags[ELSE_DO])
      i&=!prevcond;
   if(flags[ALSO_N_IF_SUCC])
      i&=lastcond&&lastsucc;
   if(flags[ALSO_NEXT_RECIPE])
      i=i&&lastcond;
   Stdout=0;
   for(skippedempty=0;;)
    { skipspace();--nrcond;
      if(!testB('*'))			    /* marks a condition, new syntax */
	 if(nrcond<0)		/* keep counting, for backward compatibility */
	  { if(testB('#'))		      /* line starts with a comment? */
	     { skipline();				    /* skip the line */
	       continue;
	     }
	    if(testB('\n'))				 /* skip empty lines */
	     { skippedempty=1;			 /* make a note of this fact */
	       continue;
	     }
	    if(skippedempty&&testB(':'))
	     { nlog("Missing action\n");i=2;
	       goto ret;
	     }
	    break;		     /* no more conditions, time for action! */
	  }
      skipspace();
      if(getlline(buf2))
	 i=0;				       /* assume failure on overflow */
      if(i)					 /* check out all conditions */
       { int negate,scoreany;double weight,xponent,lscore;
	 char*lstartchar=startchar;long ltobesent=tobesent,sizecheck=filled;
	 for(chp=strchr(buf2,'\0');--chp>=buf2;)
	  { switch(*chp)		  /* strip off whitespace at the end */
	     { case ' ':case '\t':*chp='\0';
		  continue;
	     }
	    break;
	  }
	 negate=scoreany=0;lscore=score;
	 for(chp=buf2+1;;strcpy(buf2,buf))
copydone: { switch(*(sgetcp=buf2))
	     { case '0':case '1':case '2':case '3':case '4':case '5':case '6':
	       case '7':case '8':case '9':case '-':case '+':case '.':case ',':
		{ char*chp3;double w;
		  w=stod(buf2,(const char**)&chp3);chp2=chp3;
		  if(chp2>buf2&&*(chp2=skpspace(chp2))=='^')
		   { double x;
		     x=stod(chp2+1,(const char**)&chp3);
		     if(chp3>chp2+1)
		      { if(score>=MAX32)
			   goto skiptrue;
			xponent=x;weight=w;scored=scoreany=1;
			chp2=skpspace(chp3);
			goto copyrest;
		      }
		   }
		  chp--;
		  goto normalregexp;
		}
	       default:chp--;		     /* no special character, backup */
		{ if(alphanum(*(chp2=chp)))
		   { char*chp3;
		     while(alphanum(*++chp2));
		     if(!strncmp(chp3=skpspace(chp2),"??",2))
		      { *chp2='\0';lstartchar=themail;
			if(!chp[1])
			 { ltobesent=thebody-themail;
			   switch(*chp)
			    { case 'B':lstartchar=thebody;
				 ltobesent=filled-ltobesent;
				 goto partition;
			      case 'H':
				 goto docon;
			    }
			 }
			else if(!strcmp("HB",chp)||
			 !strcmp("BH",chp))
			 { ltobesent=filled;
docon:			   concon(' ');
			   goto partition;
			 }
			ltobesent=strlen(lstartchar=(char*)tgetenv(chp));
partition:		chp2=skpspace(chp3+2);chp++;sizecheck=ltobesent;
			goto copyrest;
		      }
		   }
		}
	       case '\\':
normalregexp:	{ int or_nocase;		/* case-distinction override */
		  static const struct {const char*regkey,*regsubst;}
		   *regsp,regs[]=
		    { {FROMDkey,FROMDsubstitute},
		      {TO_key,TO_substitute},
		      {TOkey,TOsubstitute},
		      {FROMMkey,FROMMsubstitute},
		      {0,0}
		    };
		  squeeze(chp);or_nocase=0;
		  goto jinregs;
		  do			   /* find special keyword in regexp */
		     if((chp2=strstr(chp,regsp->regkey))&&
		      (chp2==buf2||chp2[-1]!='\\'))		 /* escaped? */
		      { size_t lregs,lregk;			   /* no, so */
			lregk=strlen(regsp->regkey);		/* insert it */
			tmemmove(chp2+(lregs=strlen(regsp->regsubst)),
			 chp2+lregk,strlen(chp2)-lregk+1);
			tmemmove(chp2,regsp->regsubst,lregs);
			if(regsp==regs)			   /* daemon regexp? */
			   or_nocase=1;		     /* no case sensitivity! */
jinregs:		regsp=regs;		/* start over and look again */
		      }
		     else
			regsp++;			     /* next keyword */
		  while(regsp->regkey);
		  ;{ int igncase;
		     igncase=or_nocase||!flags[DISTINGUISH_CASE];
		     if(scoreany)
		      { struct eps*re;
			re=bregcomp(chp,igncase);chp=lstartchar;
			if(negate)
			 { if(weight&&!bregexec(re,(const uchar*)chp,
			    (const uchar*)chp,(size_t)ltobesent,igncase))
			      score+=weight;
			 }
			else
			 { double oweight=weight*weight;
			   while(weight!=0&&
				 MIN32<score&&
				 score<MAX32&&
				 ltobesent>=0&&
				 (chp2=bregexec(re,(const uchar*)lstartchar,
				  (const uchar*)chp,(size_t)ltobesent,
				  igncase)))
			    { score+=weight;weight*=xponent;
			      if(chp>=chp2)		  /* break off empty */
			       { if(0<xponent&&xponent<1)
				    score+=weight/(1-xponent);
				 else if(xponent>=1&&weight!=0)
				    score+=weight<0?MIN32:MAX32;
				 break;			    /* matches early */
			       }
			      ;{ volatile double nweight=weight*weight;
				 if(nweight<oweight&&oweight<1)
				    break;
				 oweight=nweight;
			       }
			      ltobesent-=chp2-chp;chp=chp2;
			    }
			 }
			free(re);
		      }
		     else				     /* egrep for it */
			i=!!egrepin(chp,lstartchar,ltobesent,!igncase)^negate;
		   }
		  break;
		}
	       case '$':*buf2='"';squeeze(chp);
		  if(readparse(buf,sgetc,2)&&(i=0,1))
		     break;
		  strcpy(buf2,skpspace(buf));
		  goto copydone;
	       case '!':negate^=1;chp2=skpspace(chp);
copyrest:	  strcpy(buf,chp2);
		  continue;
	       case '?':pwait=2;metaparse(chp);inittmout(buf);ignwerr=1;
		   pipin(buf,lstartchar,ltobesent);
		   if(scoreany&&lexitcode>=0)
		    { int j=lexitcode;
		      if(negate)
			 while(--j>=0&&(score+=weight)<MAX32&&score>MIN32)
			    weight*=xponent;
			 else
			    score+=j?xponent:weight;
		    }
		   else if(!!lexitcode^negate)
		      i=0;
		   strcpy(buf2,buf);
		   break;
	       case '>':case '<':
		{ long pivot;
		  if(readparse(buf,sgetc,2)&&(i=0,1))
		     break;
		  ;{ char*chp3;
		     pivot=strtol(buf+1,&chp3,10);chp=chp3;
		   }
		  skipped(skpspace(chp));strcpy(buf2,buf);
		  if(scoreany)
		   { double f;
		     if((*buf=='<')^negate)
			if(sizecheck)
			   f=(double)pivot/sizecheck;
			else if(pivot>0)
			   goto plusinfty;
			else
			   goto mininfty;
		     else if(pivot)
			f=(double)sizecheck/pivot;
		     else
			goto plusinfty;
		     score+=weight*tpow(f,xponent);
		   }
		  else if(!((*buf=='<'?sizecheck<pivot:sizecheck>pivot)^
		   negate))
		     i=0;
		}
	     }
	    break;
	  }
	 if(score>MAX32)			/* chop off at plus infinity */
plusinfty:  score=MAX32;
	 if(score<=MIN32)		       /* chop off at minus infinity */
mininfty:   score=MIN32,i=0;
	 if(verbose)
	  { if(scoreany)	     /* not entirely correct, but it will do */
	     { charNUM(num,long);
	       nlog("Score: ");ltstr(7,(long)(score-lscore),num);
	       elog(num);elog(" ");
	       ;{ long iscore=score;
		  ltstr(7,iscore,num);
		  if(!iscore&&score>0)
		     num[7-2]='+';			/* show +0 for (0,1) */
		}
	       elog(num);
	     }
	    else
	       nlog(i?"M":"No m"),elog("atch on");
	    if(negate)
	       elog(" !");
	    logqnl(buf2);
	  }
skiptrue:;
       }
    }
   if(!(lastscore=score)&&score>0)			   /* save it for $= */
      lastscore=1;					 /* round up +0 to 1 */
   if(scored&&i&&score<=0)
      i=0;					     /* it was not a success */
ret:
   return i;
}
