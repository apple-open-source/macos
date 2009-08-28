/************************************************************************
 *	procmail - The autonomous mail processor			*
 *									*
 *	It has been designed to be able to be run suid root and (in	*
 *	case your mail spool area is *not* world writable) sgid		*
 *	mail (or daemon), without creating security holes.		*
 *									*
 *	Seems to be perfect.						*
 *									*
 *	Copyright (c) 1990-1999, S.R. van den Berg, The Netherlands	*
 *	Copyright (c) 1999-2001, Philip Guenther, The United States	*
 *							of America	*
 *	#include "../README"						*
 ************************************************************************/
#ifdef RCS
static /*const*/char rcsid[]=
 "$Id: procmail.c,v 1.183 2001/08/31 04:57:36 guenther Exp $";
#endif
#include "../patchlevel.h"
#include "procmail.h"
#include "acommon.h"
#include "sublib.h"
#include "robust.h"
#include "shell.h"
#include "misc.h"
#include "memblk.h"
#include "pipes.h"
#include "common.h"
#include "cstdio.h"
#include "exopen.h"
#include "mcommon.h"
#include "goodies.h"
#include "locking.h"
#include "mailfold.h"
#include "lastdirsep.h"
#include "authenticate.h"
#include "lmtp.h"
#include "foldinfo.h"
#include "variables.h"
#include "comsat.h"
#include "from.h"

static const char*const nullp,exflags[]=RECFLAGS,drcfile[]="Rcfile:",
 pmusage[]=PM_USAGE,*etcrc=ETCRC,misrecpt[]="Missing recipient\n",
 extrns[]="Extraneous ",ignrd[]=" ignored\n",pardir[]=chPARDIR,
 defspath[]=DEFSPATH,defmaildir[]=DEFmaildir;
char*buf,*buf2,*loclock;
const char shell[]="SHELL",lockfile[]="LOCKFILE",newline[]="\n",binsh[]=BinSh,
 unexpeof[]="Unexpected EOL\n",*const*gargv,*const*restargv= &nullp,*sgetcp,
 pmrc[]=PROCMAILRC,*rcfile,dirsep[]=DIRSEP,devnull[]=DevNull,empty[]="",
 lgname[]="LOGNAME",executing[]="Executing",oquote[]=" \"",cquote[]="\"\n",
 procmailn[]="procmail",whilstwfor[]=" whilst waiting for ",home[]="HOME",
 host[]="HOST",*defdeflock=empty,*argv0=empty,curdir[]={chCURDIR,'\0'},
 slogstr[]="%s \"%s\"",conflicting[]="Conflicting ",orgmail[]="ORGMAIL",
 insufprivs[]="Insufficient privileges\n",defpath[]=DEFPATH,
 exceededlb[]="Exceeded LINEBUF\n",errwwriting[]="Error while writing to",
 Version[]=VERSION;
int retval=EX_CANTCREAT,retvl2=EXIT_SUCCESS,sh,pwait,rc= -1,
 privileged=priv_START,lexitcode=EXIT_SUCCESS,ignwerr,crestarg,savstdout,
 berkeley,mailfilter,erestrict,Deliverymode,ifdepth;   /* depth of outermost */
struct dyna_array ifstack;
size_t linebuf=mx(DEFlinebuf,1024/*STRLEN(systm_mbox)<<1*/);
volatile int nextexit,lcking;		       /* if termination is imminent */
pid_t thepid;
long filled,lastscore;	       /* the length of the mail, and the last score */
memblk themail;							 /* the mail */
char*thebody;					  /* the body of the message */
uid_t uid;
gid_t gid,sgid;

static auth_identity*savepass(spass,uid)auth_identity*const spass;
 const uid_t uid;
{ const auth_identity*tpass;
  if(auth_filledid(spass)&&auth_whatuid(spass)==uid)
     goto ret;
  if(tpass=auth_finduid(uid,0))				  /* save by copying */
   { auth_copyid(spass,tpass);
ret: return spass;
   }
  return (auth_identity*)0;
}

#define rct_ABSOLUTE	0			    /* rctypes for tryopen() */
#define rct_CURRENT	1
#define rct_DEFAULT	2

#define rcs_DELIVERED	1		     /* rc exit codes for mainloop() */
#define rcs_EOF		2
#define rcs_HOST	3

static void
 usage P((void));
static int
 tryopen P((const int delay_setid,const int rctype,const int dowarning)),
 mainloop P((void));

int main(argc,argv)int argc;const char*const argv[];
{ register char*chp,*chp2;
#if 0				/* enable this if you want to trace procmail */
  kill(getpid(),SIGSTOP);/*raise(SIGSTOP);*/
#endif
  newid();
  ;{ int presenviron,override;char*fromwhom=0;
     const char*idhint=0;gid_t egid=getegid();
     presenviron=Deliverymode=mailfilter=override=0;
     Openlog(procmailn,LOG_PID,LOG_MAIL);		  /* for the syslogd */
     if(argc)			       /* sanity check, any argument at all? */
      { Deliverymode=!!strncmp(lastdirsep(argv0=argv[0]),procmailn,
	 STRLEN(procmailn));
	for(argc=0;(chp2=(char*)argv[++argc])&&*chp2=='-';)
	   for(;;)				       /* processing options */
	    { switch(*++chp2)
	       { case VERSIONOPT:
		    usage();
		    return EXIT_SUCCESS;
		 case HELPOPT1:case HELPOPT2:elog(pmusage);elog(PM_HELP);
		    elog(PM_QREFERENCE);
		    return EX_USAGE;
		 case PRESERVOPT:presenviron=1;
		    continue;
		 case MAILFILTOPT:mailfilter=1;
		    continue;
		 case OVERRIDEOPT:override=1;
		    continue;
		 case BERKELEYOPT:case ALTBERKELEYOPT:berkeley=1;
		    continue;
		 case TEMPFAILOPT:retval=EX_TEMPFAIL;
		    continue;
		 case FROMWHOPT:case ALTFROMWHOPT:
		    if(*++chp2)
		       fromwhom=chp2;
		    else if(chp2=(char*)argv[argc+1])
		       argc++,fromwhom=chp2;
		    else
		       nlog("Missing name\n");
		    break;
		 case ARGUMENTOPT:
		  { static struct dyna_array newargv;
		    if(*++chp2)
		       goto setarg;
		    else if(chp2=(char*)argv[argc+1])
		     { argc++;
setarg:		       app_valp(newargv,(const char*)chp2);
		       restargv=&(acc_valp(newargv,0));
		       crestarg++;
		     }
		    else
		       nlog("Missing argument\n");
		    break;
		  }
		 case DELIVEROPT:
		    if(!*(chp= ++chp2)&&!(chp=(char*)argv[++argc]))
		     { nlog(misrecpt);
		       break;
		     }
		    else
		     { Deliverymode=1;
		       goto last_option;
		     }
		 case LMTPOPT:
#ifdef LMTP
		    Deliverymode=2;
		    goto last_option;
#else
		    nlog("LMTP support not enabled in this binary\n");
		    return EX_USAGE;
#endif
		 case '-':
		    if(!*++chp2)
		     { argc++;
		       goto last_option;
		     }
		 default:nlog("Unrecognised options:");logqnl(chp2);
		    elog(pmusage);elog("Processing continued\n");
		 case '\0':;
	       }
	      break;
	    }
      }
     if(Deliverymode==1&&!(chp=chp2))
	nlog(misrecpt),Deliverymode=0;
last_option:
     switch(Deliverymode)
      { case 0:
	   idhint=getenv(lgname);
	   if(mailfilter&&crestarg)
	    { crestarg=0;			     /* -m will supersede -a */
conflopt:     nlog(conflicting);elog("options\n");elog(pmusage);
	    }
	   break;
#ifdef LMTP
	case 2:
	   if(fromwhom)
	    { fromwhom=0;				  /* -z disables -f, */
	      goto confldopt;					/* -p and -m */
	    }
#endif
	case 1:
	   if(presenviron||mailfilter)
confldopt:  { presenviron=mailfilter=0;		    /* -d disables -p and -m */
	      goto conflopt;
	    }
	   break;
	default:				       /* this cannot happen */
	   abort();
      }
     cleanupenv(presenviron);
     ;{ auth_identity*pass,*passinvk;auth_identity*spassinvk;
	uid_t euid=geteuid();
	uid=getuid();gid=getgid();
	spassinvk=auth_newid();passinvk=savepass(spassinvk,uid);   /* are we */
	checkprivFrom_(euid,passinvk?auth_username(passinvk):0,override);
	doumask(INIT_UMASK);		   /* allowed to set the From_ line? */
	while((savstdout=rdup(STDOUT))<=STDERR)
	 { rclose(savstdout);		       /* move stdout out of the way */
	   if(0>(savstdout=opena(devnull)))
	      goto nodevnull;
	   syslog(LOG_ALERT,"Descriptor %d was not open\n",savstdout);
	 }
	fclose(stdout);rclose(STDOUT);			/* just to make sure */
	if(0>opena(devnull))
nodevnull:
	 { writeerr(devnull);syslog(LOG_EMERG,slogstr,errwwriting,devnull);
	   return EX_OSFILE;			  /* couldn't open /dev/null */
	 }
#ifdef console
	opnlog(console);
#endif
	setbuf(stdin,(char*)0);allocbuffers(linebuf,1);
#ifdef SIGXCPU
	signal(SIGXCPU,SIG_IGN);signal(SIGXFSZ,SIG_IGN);
#endif
#ifdef SIGLOST
	signal(SIGLOST,SIG_IGN);
#endif
#if DEFverbose
	verboff();verbon();
#else
	verbon();verboff();
#endif
#ifdef SIGCHLD
	signal(SIGCHLD,SIG_DFL);
#endif
	signal(SIGPIPE,SIG_IGN);
	setcomsat(empty);			  /* turn on biff by default */
	ultstr(0,(unsigned long)uid,buf);filled=0;
	if(!passinvk||!(chp2=(char*)auth_username(passinvk)))
	   chp2=buf;
#ifdef LMTP
	if(Deliverymode==2)
	 { auth_identity**rcpts,**lastrcpt,**currcpt;
	   currcpt=rcpts=lmtp(&lastrcpt,chp2);
	   if(currcpt+1!=lastrcpt)     /* if there's more than one recipient */
	      lockblock(&themail);     /* then no one can write to the block */
	   else				     /* otherwise the only recipient */
	      private(1);				/* gets the original */
	   while(currcpt<lastrcpt)
	    { if(!(pidchild=sfork()))
	       { setupsigs();
		 pass= *currcpt++;
		 while(currcpt<lastrcpt)
		    auth_freeid(*currcpt++);
		 freeoverread();
		 free(rcpts);newid();gargv=&nullp;
		 goto dorcpt;
	       }
	      if(forkerr(pidchild,procmailn))
		 lexitcode=EX_OSERR;
	      else
		 waitfor(pidchild);
	      lmtpresponse(lexitcode);
	      pidchild=0;
	      auth_freeid(*currcpt++);
	    }
	   free(rcpts);
	   flushoverread();		 /* pass upwards the extra LMTP data */
	   exit(EXIT_SUCCESS);
	 }
#endif
	setupsigs();
	makeFrom(fromwhom,chp2);
	readmail(0,0L);			      /* read in the mail completely */
	if(Deliverymode)
	 { if(argv[argc+1])			 /* more than one recipient? */
	    { private(0);				    /* time to share */
	      lockblock(&themail);
	    }
	   else
	      private(1);
	   do			  /* chp should point to the first recipient */
	    { chp2=chp;
	      if(argv[++argc])			  /* more than one recipient */
		 if(pidchild=sfork())
		  { if(forkerr(pidchild,procmailn)||
		       waitfor(pidchild)!=EXIT_SUCCESS)
		       retvl2=retval;
		    pidchild=0;		      /* loop for the next recipient */
		  }
		 else
		  { newid();
		    while(argv[++argc]);    /* skip till end of command line */
		    break;
		  }
	    }
	   while(chp=(char*)argv[argc]);
	 }
	gargv=argv+argc;			 /* save it for nextrcfile() */
	if(Deliverymode)	/* try recipient without changing case first */
	 { if(!(pass=auth_finduser(chp2,-1)))	    /* chp2 is the recipient */
	    { static const char unkuser[]="Unknown user";
	      nlog(unkuser);logqnl(chp2);syslog(LOG_ERR,slogstr,unkuser,chp2);
	      return EX_NOUSER;			/* we don't handle strangers */
	    }
dorcpt:	   if(enoughprivs(passinvk,euid,egid,auth_whatuid(pass),
	    auth_whatgid(pass)))
	      goto Setuser;
	   nlog(insufprivs);
	   syslog(LOG_CRIT,"Insufficient privileges to deliver to \"%s\"\n",
	    chp2);
	   return EX_NOPERM;	      /* need more mana, decline the request */
	 }
	else
	 { int commandlinerc=nextrcfile();
	   if(presenviron)		      /* preserving the environment? */
	      etcrc=0;				    /* don't read etcrc then */
	   if(commandlinerc)			     /* command-line rcfile? */
	    { etcrc=0;				 /* forget etcrc and comsat: */
	      setcomsat(DEFcomsat);			/* the internal flag */
	      if(!presenviron)			 /* and usually the variable */
		 setdef(scomsat,DEFcomsat);
	    }
	   if(mailfilter)
	    { if(!commandlinerc)
	       { nlog("Missing rcfile\n");
		 return EX_NOINPUT;
	       }
#ifdef ETCRCS
	      ;{ static const char etcrcs[]=ETCRCS;
		 if(!strncmp(etcrcs,rcfile,STRLEN(etcrcs)))
		  { struct stat stbuf; /* path starts with /etc/procmailrcs/ */
		   /*
		    *	although the filename starts with /etc/procmailrcs/
		    *	we will now check if it does not contain any backward
		    *	references which would allow it to escape the secure
		    *	tree; look for /../ sequences
		    */
		    for(chp=(char*)rcfile+STRLEN(etcrcs)-1;
			chp;		       /* any devious embedded /../? */
			chp=strpbrk(chp,dirsep))
		       if(!strncmp(pardir,++chp,STRLEN(pardir))&&
			  (chp+=STRLEN(pardir),strchr(dirsep,*chp)))
			  goto nospecial;	  /* yes, security violation */
#ifdef CAN_chown
		    *(chp=strcpy(buf2,etcrcs)+STRLEN(etcrcs))=chCURDIR;
		    *++chp='\0';
#endif
		   /*
		    *	so far, so good, now verify the credentials down to the
		    *	last bit
		    */
		    if(presenviron||			  /* -p is dangerous */
		       commandlinerc!=2||     /* so are variable assignments */
#ifdef CAN_chown		  /* anyone can chown in this filesystem so: */
		       stat(buf2,&stbuf)||	     /* the /etc/procmailrcs */
		       !S_ISDIR(stbuf.st_mode)||	/* directory must be */
		       stbuf.st_uid!=ROOT_uid&&		    /* owned by root */
			chown(buf2,ROOT_uid,stbuf.st_gid)||
		       stbuf.st_mode&(S_IXGRP|S_IXOTH)&&   /* and accessible */
			chmod(buf2,S_IRWXU)||		   /* to no one else */
#endif
		       lstat(rcfile,&stbuf)||		/* it seems to exist */
		       !enoughprivs(passinvk,euid,egid,stbuf.st_uid,
			stbuf.st_gid)||		   /* can we do this at all? */
		       S_ISDIR(stbuf.st_mode)||		  /* no directories! */
		       !savepass(spassinvk,stbuf.st_uid)     /* user exists? */
		      )
nospecial:	     { static const char densppr[]=
			"Denying special privileges for";
		       nlog(densppr);logqnl(rcfile);
		       syslog(LOG_ALERT,slogstr,densppr,rcfile);
		     }
		    else			    /* no security violation */
		       mailfilter=2,passinvk=spassinvk;
		  }				      /* accept new identity */
	       }
#endif
	    }
	 }
	pass=passinvk;
	if(passinvk&&idhint)	      /* if same uid as $LOGNAME, use latter */
	 { auth_identity*idpass=auth_finduser((char*)idhint,0);
	   if(idpass)
	    { if(auth_whatuid(passinvk)==auth_whatuid(idpass))
		 pass=idpass;
	    }
	 }
	if(pass)	      /* set preferred uid to the intended recipient */
Setuser: { gid=auth_whatgid(pass);uid=auth_whatuid(pass);
	   if(euid==ROOT_uid&&(chp=(char*)auth_username(pass))&&*chp)
	      initgroups(chp,gid);
	   endgrent();
	 }
	else					  /* user could not be found */
	   setids();   /* to prevent security holes, drop any privileges now */
	initdefenv(pass,buf,!presenviron||!mailfilter);		 /* override */
	endpwent();auth_freeid(spassinvk);	   /* environment by default */
      }
     if(buildpath(orgmail,fdefault,(char*)0))	/* setup DEFAULT and ORGMAIL */
      { fdefault=empty;			   /* uh, Houston, we have a problem */
	goto nix_sysmbox;
      }
     /*
      * Processing point of proposed /etc/procmail.conf file
      */
     fdefault=tstrdup(buf);sgid=egid;
     chp=(char*)tgetenv(orgmail);
     if(mailfilter||!screenmailbox(chp,egid,Deliverymode))
nix_sysmbox:
      { sputenv(orgmail);		 /* nix delivering to system mailbox */
	if(privileged)			       /* don't need this any longer */
	   privileged=priv_DONTNEED;
	if(!strcmp(chp,fdefault))			/* DEFAULT the same? */
	   free((char*)fdefault),fdefault=empty;		 /* so panic */
      }						/* bad news, be conservative */
   }
  doumask(INIT_UMASK);
  while(chp=(char*)argv[argc])	       /* interpret command line specs first */
   { argc++;
     if(!asenvcpy(chp)&&mailfilter)
      { gargv= &nullp;				 /* stop at the first rcfile */
	for(restargv=argv+argc;restargv[crestarg];crestarg++);
	break;
      }
   }
  if(etcrc)		  /* do we start with an /etc/procmailrc file first? */
   { if(0<=bopen(etcrc))
      { yell(drcfile,etcrc);
#if !DEFverbose
	if(privileged)
	   verbose=0;			    /* no peeking in /etc/procmailrc */
#endif
	eputenv(defspath,buf);			      /* use the secure PATH */
	if(mainloop()==rcs_DELIVERED)			   /* run the rcfile */
	   goto mailed;
	eputenv(defpath,buf);		 /* switch back to the insecure PATH */
      }
   }
  erestrict=mailfilter!=2;		      /* possibly restrict execs now */
  if(rcfile&&!mailfilter)			     /* "procmail rcfile..." */
   { int rctype,dowarning;	 /* only warn about the first missing rcfile */
     for(dowarning=1;;)
      { rctype=rct_ABSOLUTE;
	if(strchr(dirsep,*rcfile))				/* absolute? */
	   strcpy(buf,rcfile);
	else if(*rcfile==chCURDIR&&strchr(dirsep,rcfile[1]))   /* ./ prefix? */
	   strcpy(buf,rcfile),rctype=rct_CURRENT;
	else			     /* prepend default procmailrc directory */
	   if(buildpath(maildir,defmaildir,rcfile))
	      break;
	if(tryopen(0,rctype,dowarning))
	 { register int rcs=mainloop();				   /* run it */
	   if(rcs==rcs_DELIVERED)
	      goto mailed;					 /* success! */
	   if(rcs==rcs_EOF)
	      break;				     /* normal end of rcfile */
	   if(!nextrcfile())				       /* none left? */
	      goto mailed;					 /* then out */
	 }
	else				      /* not available? try the next */
	 { dowarning=0;				/* suppress further messages */
	   if(!nextrcfile())				       /* none left? */
	      break;						 /* then out */
	 }
      }
   }
  else
   { int rctype;
     if(!rcfile)			    /* no rcfile on the command line */
      { rctype=rct_DEFAULT;
	if(buildpath("default rcfile",pmrc,(char*)0))
	   goto nomore_rc;
      }
     else						  /* mailfilter mode */
      { rctype=strchr(dirsep,*rcfile)?rct_ABSOLUTE:rct_CURRENT;
	strcpy(buf,rcfile);
      }
     if(tryopen(mailfilter==2,rctype,DEFverbose||mailfilter))
	if(mainloop()!=rcs_EOF)
	   goto mailed;
   }
nomore_rc:
  if(ifstack.vals)
     free(ifstack.vals);
  ;{ int succeed;
     concon('\n');succeed=0;
     if(*(chp=(char*)fdefault))				     /* DEFAULT set? */
      { int len;
	setuid(uid);			   /* make sure we have enough space */
	if(linebuf<(len=strlen(chp)+strlen(lockext)+UNIQnamelen))
	   allocbuffers(linebuf=len,1);	   /* to perform the lock & delivery */
	if(writefolder(chp,(char*)0,themail.p,filled,0,1))	  /* default */
	   succeed=1;
      }						       /* if all else failed */
     if(!succeed&&*(chp2=(char*)tgetenv(orgmail))&&strcmp(chp2,chp))
      { rawnonl=0;
	if(writefolder(chp2,(char*)0,themail.p,filled,0,0))   /* don't panic */
	   succeed=1;				      /* try the last resort */
      }
     if(succeed)				     /* should we panic now? */
mailed: retval=EXIT_SUCCESS;		  /* we're home free, mail delivered */
   }
  unlock(&loclock);Terminate();
}

static void usage P((void))
{ elog(procmailn);elog(Version);
  elog("\nLocking strategies:\tdotlocking");
#ifndef NOfcntl_lock
  elog(", fcntl()");				    /* a peek under the hood */
#endif
#ifdef USElockf
  elog(", lockf()");
#endif
#ifdef USEflock
  elog(", flock()");
#endif
  elog("\nDefault rcfile:\t\t");elog(pmrc);
#ifdef GROUP_PER_USER
  elog("\n\tIt may be writable by your primary group");
#endif
  elog("\nYour system mailbox:\t");
  elog(auth_mailboxname(auth_finduid(getuid(),0)));
  elog(newline);
#ifdef USE_MMAP
  elog("\nLarge messages will be memory mapped during processing.\n");
#endif
}

/*
 *	if we happen to be still running as root, and the rcfile
 *	is mounted on a secure NFS-partition, we might not be able
 *	to access it, so check if we can stat it or don't need any
 *	sgid privileges, if yes, drop all privs and set uid to
 *	the recipient beforehand
 */
static int tryopen(delay_setid,rctype,dowarning)
 const int delay_setid,rctype,dowarning;
{ struct stat stbuf;
  if(!delay_setid&&privileged&&	  /* if we can setid now and we haven't yet, */
      (privileged==priv_DONTNEED||!stat(buf,&stbuf))) /* and we either don't */
     setids();	   /* need the privileges or it's accessible, then setid now */
  if(0>bopen(buf))				   /* try opening the rcfile */
   { if(dowarning)
rerr:	readerr(buf);
     return 0;
   }
  if(!delay_setid&&privileged)		   /* if we're not supposed to delay */
   { closerc();		       /* and we haven't changed yet, then close it, */
     setids();				 /* transmogrify to prevent peeking, */
     if(0>bopen(buf))					    /* and try again */
	goto rerr;		   /* they couldn't read it, so it was bogus */
   }
#ifndef NOfstat
  if(fstat(rc,&stbuf))					    /* the right way */
#else
  if(stat(buf,&stbuf))					  /* the best we can */
#endif
   { static const char susprcf[]="Suspicious rcfile";
suspicious_rc:
     closerc();nlog(susprcf);logqnl(buf);
     syslog(LOG_ALERT,slogstr,susprcf,buf);
     goto rerr;
   }
  if(delay_setid)			 /* change now if we haven't already */
     setids();
  if(rctype==rct_CURRENT)	  /* opened rcfile in the current directory? */
   { if(!didchd)
	setmaildir(curdir);
   }
  else
    /*
     * OK, so now we have opened an absolute rcfile, but for security
     * reasons we only accept it if it is owned by the recipient or
     * root and is not world writable, and the directory it is in is
     * not world writable or has the sticky bit set.  If this is the
     * default rcfile then we also outlaw group writability.
     */
   { register char*chp=lastdirsep(buf),c;
     c= *chp;
     if(((stbuf.st_uid!=uid&&stbuf.st_uid!=ROOT_uid||	       /* check uid, */
	  (stbuf.st_mode&S_IWOTH)||		      /* writable by others, */
	  rctype==rct_DEFAULT&&		   /* if the default then also check */
	   (stbuf.st_mode&S_IWGRP)&&		   /* for writable by group, */
	   (NO_CHECK_stgid||stbuf.st_gid!=gid)
	 )&&strcmp(devnull,buf)||	     /* /dev/null is a special case, */
	(*chp='\0',stat(buf,&stbuf))||		     /* check the directory, */
#ifndef CAN_chown				   /* sticky and can't chown */
	!(stbuf.st_mode&S_ISVTX)&&		   /* means we don't care if */
#endif					     /* it's group or world writable */
	((stbuf.st_mode&(S_IWOTH|S_IXOTH))==(S_IWOTH|S_IXOTH)||
	 rctype==rct_DEFAULT&&
	  (stbuf.st_mode&(S_IWGRP|S_IXGRP))==(S_IWGRP|S_IXGRP)&&
	  (NO_CHECK_stgid||stbuf.st_gid!=gid))))
      { *chp=c;
	goto suspicious_rc;
      }
     *chp=c;
   }
  yell(drcfile,buf);
  /*
   *	Chdir now if we haven't already
   */
  if(!didchd)				       /* have we done this already? */
   { const char*chp;
     if(buildpath(maildir,defmaildir,(char*)0))
	exit(EX_OSERR);		   /* something was wrong: give up the ghost */
     if(chdir(chp=buf))
      { chderr(buf);		      /* no, well, then try an initial chdir */
	chp=tgetenv(home);
	if(!strcmp(chp,buf)||chdir(chp))
	   chderr(chp),chp=curdir;		/* that didn't work, use "." */
      }
     setmaildir(chp);
   }
  return 1;						 /* we're good to go */
}

static int mainloop P((void))
{ int lastsucc,lastcond,prevcond,i,skiprc;register char*chp,*tolock;
  lastsucc=lastcond=prevcond=skiprc=0;
  tolock=0;
  do
   { unlock(&loclock);				/* unlock any local lockfile */
     goto commint;
     do
      { skipline();
commint:do skipspace();					  /* skip whitespace */
	while(testB('\n'));
      }
     while(testB('#'));					   /* no comment :-) */
     if(testB(':'))				       /* check for a recipe */
      { int locknext,succeed;char*startchar;long tobesent;
	static char flags[maxindex(exflags)];
	do
	 { int nrcond;
	   if(readparse(buf,getb,0,skiprc))
	      return rcs_EOF;			      /* give up on this one */
	   ;{ char*temp;			 /* so that chp isn't forced */
	      nrcond=strtol(buf,&temp,10);chp=temp;	      /* into memory */
	    }
	   if(chp==buf)					 /* no number parsed */
	      nrcond= -1;
	   if(tolock)		 /* clear temporary buffer for lockfile name */
	      free(tolock);
	   for(i=maxindex(flags);i;i--)			  /* clear the flags */
	      flags[i]=0;
	   for(tolock=0,locknext=0;;)
	    { chp=skpspace(chp);
	      switch(i= *chp++)
	       { default:
		    ;{ char*flg;
		       if(!(flg=strchr(exflags,i)))	    /* a valid flag? */
			{ chp--;
			  break;
			}
		       flags[flg-exflags]=1;		     /* set the flag */
		     }
		 case '\0':
		    if(chp!=Tmnate)		/* if not the real end, skip */
		       continue;
		    break;
		 case ':':locknext=1;	    /* yep, local lockfile specified */
		    if(*chp||++chp!=Tmnate)
		       tolock=tstrdup(chp),chp=strchr(chp,'\0')+1;
	       }
	      concatenate(chp);skipped(chp);		/* display leftovers */
	      break;
	    }				      /* parse & test the conditions */
	   i=conditions(flags,prevcond,lastsucc,lastcond,skiprc!=0,nrcond);
	   if(!skiprc)
	    { if(!flags[ALSO_NEXT_RECIPE]&&!flags[ALSO_N_IF_SUCC])
		 lastcond=i==1;		   /* save the outcome for posterity */
	      if(!prevcond||!flags[ELSE_DO])
		 prevcond=i==1;	      /* ditto for `else if' like constructs */
	    }
	 }
	while(i==2);			     /* missing in action, reiterate */
	startchar=themail.p;tobesent=filled;
	if(flags[PASS_HEAD])			    /* body, header or both? */
	 { if(!flags[PASS_BODY])
	      tobesent=thebody-themail.p;
	 }
	else if(flags[PASS_BODY])
	   tobesent-=(startchar=thebody)-themail.p;
	Stdout=0;succeed=sh=0;
	pwait=flags[WAIT_EXIT]|flags[WAIT_EXIT_QUIET]<<1;
	ignwerr=flags[IGNORE_WRITERR];skipspace();
	if(i)
	   zombiecollect(),concon('\n');
progrm: if(testB('!'))					 /* forward the mail */
	 { if(!i)
	      skiprc|=1;
	   if(strlcpy(buf,sendmail,linebuf)>=linebuf)
	      goto fail;
	   chp=strchr(buf,'\0');
	   if(*flagsendmail)
	    { char*q;int got=0;
	      if(!(q=simplesplit(chp+1,flagsendmail,buf+linebuf-1,&got)))
		 goto fail;
	      *(chp=q)='\0';
	    }
	   if(readparse(chp+1,getb,0,skiprc))
	      goto fail;
	   if(i)
	    { if(startchar==themail.p)
	       { startchar[filled]='\0';		     /* just in case */
		 startchar=(char*)skipFrom_(startchar,&tobesent);
	       }      /* leave off leading From_ -- it confuses some mailers */
	      goto forward;
	    }
	   skiprc&=~1;
	 }
	else if(testB('|'))				    /* pipe the mail */
	 { chp=buf2;
	   if(getlline(chp,buf2+linebuf))	 /* get the command to start */
	      goto commint;
	   if(i)
	    { metaparse(buf2);
	      if(!sh&&buf+1==Tmnate)		      /* just a pipe symbol? */
	       { *buf='|';*(char*)(Tmnate++)='\0';		  /* fake it */
		 goto tostdout;
	       }
forward:      if(locknext)
	       { if(!tolock)	   /* an explicit lockfile specified already */
		  { *buf2='\0';	    /* find the implicit lockfile ('>>name') */
		    for(chp=buf;i= *chp++;)
		       if(i=='>'&&*chp=='>')
			{ chp=skpspace(chp+1);
			  tmemmove(buf2,chp,i=strcspn(chp,EOFName));
			  buf2[i]='\0';
			  if(sh)	 /* expand any environment variables */
			   { chp=tstrdup(buf);sgetcp=buf2;
			     if(readparse(buf,sgetc,0,0))
			      { *buf2='\0';
				goto nolock;
			      }
			     strcpy(buf2,buf);strcpy(buf,chp);free(chp);
			   }
			  break;
			}
		    if(!*buf2)
nolock:		     { nlog("Couldn't determine implicit lockfile from");
		       logqnl(buf);
		     }
		  }
		 lcllock(tolock,buf2);
		 if(!pwait)		/* try and protect the user from his */
		    pwait=2;			   /* blissful ignorance :-) */
	       }
	      rawnonl=flags[RAW_NONL];
	      if(flags[CONTINUE]&&(flags[FILTER]||Stdout))
		 nlog(extrns),elog("copy-flag"),elog(ignrd);
	      inittmout(buf);
	      if(flags[FILTER])
	       { if(startchar==themail.p&&tobesent!=filled)   /* if only 'h' */
		  { if(!pipthrough(buf,startchar,tobesent))
		       readmail(1,tobesent),succeed=!pipw;
		  }
		 else if(!pipthrough(buf,startchar,tobesent))
		  { filled=startchar-themail.p;
		    readmail(0,tobesent);
		    succeed=!pipw;
		  }
	       }
	      else if(Stdout)			  /* capturing stdout again? */
		 succeed=!pipthrough(buf,startchar,tobesent);
	      else if(!pipin(buf,startchar,tobesent,1))	  /* regular program */
	       { succeed=1;
		 if(flags[CONTINUE])
		    goto logsetlsucc;
		 else
		    goto frmailed;
	       }
	      goto setlsucc;
	    }
	 }
	else if(testB(EOF))
	   nlog("Incomplete recipe\n");
	else		   /* dump the mail into a mailbox file or directory */
	 { int ofiltflag;char*end=buf+linebuf-4;	/* reserve some room */
	   if(ofiltflag=flags[FILTER])
	      flags[FILTER]=0,nlog(extrns),elog("filter-flag"),elog(ignrd);
	   if(chp=gobenv(buf,end))	   /* can it be an environment name? */
	    { if(chp==end)
	       { getlline(buf,buf+linebuf);
		 goto fail;
	       }
	      if(skipspace())
		 chp++;			   /* keep pace with argument breaks */
	      if(testB('='))		      /* is it really an assignment? */
	       { int c;
		 *chp++='=';*chp='\0';
		 if(skipspace())
		    chp++;
		 ungetb(c=getb());
		 switch(c)
		  { case '!':case '|':			  /* ok, it's a pipe */
		       if(i)
			  Stdout = tstrdup(buf);
		       goto progrm;
		  }
	       }
	    }			 /* find the end, start of a nesting recipe? */
	   else if((chp=strchr(buf,'\0'))==buf&&
		   testB('{')&&
		   (*chp++='{',*chp='\0',testB(' ')||		      /* } } */
		    testB('\t')||
		    testB('\n')))
	    { if(locknext&&!flags[CONTINUE])
		 nlog(extrns),elog("locallockfile"),elog(ignrd);
	      if(flags[PASS_BODY])
		 nlog(extrns),elog("deliver-body flag"),elog(ignrd);
	      if(flags[PASS_HEAD])
		 nlog(extrns),elog("deliver-head flag"),elog(ignrd);
	      if(flags[IGNORE_WRITERR])
		 nlog(extrns),elog("ignore-write-error flag"),elog(ignrd);
	      if(flags[RAW_NONL])
		 nlog(extrns),elog("raw-mode flag"),elog(ignrd);
	      if(!i)						/* no match? */
		 skiprc+=2;		      /* increase the skipping level */
	      else
	       { app_vali(ifstack,prevcond);		    /* push prevcond */
		 app_vali(ifstack,lastcond);		    /* push lastcond */
		 if(locknext)
		  { lcllock(tolock,"");
		    if(!pwait)		/* try and protect the user from his */
		       pwait=2;			   /* blissful ignorance :-) */
		  }
		 succeed=1;
		 if(flags[CONTINUE])
		  { yell("Forking",procmailn);
		    private(0);			      /* can't share anymore */
		    inittmout(procmailn);onguard();
		    if(!(pidchild=sfork()))		   /* clone yourself */
		     { if(loclock)	      /* lockfiles are not inherited */
			  free(loclock),loclock=0;
		       if(globlock)
			  free(globlock),globlock=0;	     /* clear up the */
		       newid();offguard();duprcs();	  /* identity crisis */
		     }
		    else
		     { offguard();
		       if(forkerr(pidchild,procmailn))
			  succeed=0;	       /* tsk, tsk, no cloning today */
		       else
			{ int excode;	  /* wait for our significant other? */
			  if(pwait&&
			     (excode=waitfor(pidchild))!=EXIT_SUCCESS)
			   { if(!(pwait&2)||verbose)	 /* do we report it? */
				progerr(procmailn,excode,pwait&2);
			     succeed=0;
			   }
			  pidchild=0;skiprc+=2;	     /* skip over the braces */
			  ifstack.filled-=2;		/* retract the stack */
			}
		     }
		  }
		 goto setlsucc;			/* definitely no logabstract */
	       }
	      continue;
	    }
	   if(!i)						/* no match? */
	      skiprc|=1;		  /* temporarily disable subprograms */
	   if(readparse(chp,getb,0,skiprc))
fail:	    { succeed=0;
	      goto setlsucc;
	    }
	   if(i)
	    { if(ofiltflag)	       /* protect who use bogus filter-flags */
		 startchar=themail.p,tobesent=filled;	    /* whole message */
tostdout:     rawnonl=flags[RAW_NONL];
	      if(locknext)		     /* write to a file or directory */
		 lcllock(tolock,buf);
	      inittmout(buf);		  /* to break messed-up kernel locks */
	      if(writefolder(buf,strchr(buf,'\0')+1,startchar,tobesent,
		  ignwerr,0)&&
		 (succeed=1,!flags[CONTINUE]))
frmailed:      { if(ifstack.vals)
		    free(ifstack.vals);
		 return rcs_DELIVERED;
	       }
logsetlsucc:  if(succeed&&flags[CONTINUE]&&lgabstract==2)
		 logabstract(tgetenv(lastfolder));
setlsucc:     rawnonl=0;lastsucc=succeed;lasttell= -1;	       /* for comsat */
	      resettmout();			  /* clear any pending timer */
	    }
	   skiprc&=~1;				     /* reenable subprograms */
	 }
      } /* { */
     else if(testB('}'))					/* end block */
      { if(skiprc>1)					    /* just skipping */
	   skiprc-=2;					   /* decrease level */
	else if(ifstack.filled>ifdepth)	      /* restore lastcond from stack */
	 { lastcond=acc_vali(ifstack,--ifstack.filled);
	   prevcond=acc_vali(ifstack,--ifstack.filled);		 /* prevcond */
	 }							  /* as well */
	else
	   nlog("Closing brace unexpected\n");		      /* stack empty */
      }
     else				    /* then it must be an assignment */
      { char*end=buf+linebuf;
	if(!(chp=gobenv(buf,end)))
	 { if(!*buf)					/* skip a word first */
	      getbl(buf,end);				      /* then a line */
	   skipped(buf);				/* display leftovers */
	   continue;
	 }
	if(chp==end)				      /* overflow => give up */
	   break;
	skipspace();
	if(testB('='))				   /* removal or assignment? */
	 { *chp='=';
	   if(readparse(++chp,getb,1,skiprc))
	      continue;
	 }
	else
	   *++chp='\0';			     /* throw in a second terminator */
	if(!skiprc)
	 { const char*p;
	   p=sputenv(buf);
	   chp[-1]='\0';
	   asenv(p);
	 }
      }
     if(rc<0)				   /* abnormal exit from the rcfile? */
	return rcs_HOST;
   }
  while(!testB(EOF)||(skiprc=0,poprc()));
  return rcs_EOF;
}
