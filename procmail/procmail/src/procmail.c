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
 *	#include "../README"						*
 ************************************************************************/
#ifdef RCS
static /*const*/char rcsid[]=
 "$Id: procmail.c,v 1.1.1.1 1999/09/23 17:30:07 wsanchez Exp $";
#endif
#include "../patchlevel.h"
#include "procmail.h"
#include "acommon.h"
#include "sublib.h"
#include "robust.h"
#include "shell.h"
#include "misc.h"
#include "pipes.h"
#include "common.h"
#include "cstdio.h"
#include "exopen.h"
#include "regexp.h"
#include "mcommon.h"
#include "goodies.h"
#include "locking.h"
#include "mailfold.h"
#include "lastdirsep.h"
#include "authenticate.h"

static const char*const nullp,From_[]=FROM,exflags[]=RECFLAGS,
 drcfile[]="Rcfile:",pmusage[]=PM_USAGE,*etcrc=ETCRC,
 misrecpt[]="Missing recipient\n",extrns[]="Extraneous ",ignrd[]=" ignored\n",
 pardir[]=chPARDIR,curdir[]={chCURDIR,'\0'},
 insufprivs[]="Insufficient privileges\n",
 attemptst[]="Attempt to fake stamp by";
char*buf,*buf2,*loclock,*tolock;
const char shell[]="SHELL",lockfile[]="LOCKFILE",newline[]="\n",binsh[]=BinSh,
 unexpeof[]="Unexpected EOL\n",*const*gargv,*const*restargv= &nullp,*sgetcp,
 pmrc[]=PROCMAILRC,*rcfile=pmrc,dirsep[]=DIRSEP,devnull[]=DevNull,
 lgname[]="LOGNAME",executing[]="Executing",oquote[]=" \"",cquote[]="\"\n",
 procmailn[]="procmail",whilstwfor[]=" whilst waiting for ",home[]="HOME",
 host[]="HOST",*defdeflock,*argv0="",errwwriting[]="Error while writing to",
 slogstr[]="%s \"%s\"",conflicting[]="Conflicting ",orgmail[]="ORGMAIL",
 exceededlb[]="Exceeded LINEBUF\n",pathtoolong[]=" path too long";
char*Stdout;
int retval=EX_CANTCREAT,retvl2=EXIT_SUCCESS,sh,pwait,lcking,rcstate,rc= -1,
 ignwerr,lexitcode=EXIT_SUCCESS,asgnlastf,accspooldir,crestarg,skiprc,
 savstdout,berkeley,mailfilter,erestrict;
size_t linebuf=mx(DEFlinebuf+XTRAlinebuf,1024/*STRLEN(systm_mbox)<<1*/);
volatile int nextexit;			       /* if termination is imminent */
pid_t thepid;
long filled,lastscore;	       /* the length of the mail, and the last score */
char*themail,*thebody;			    /* the head and body of the mail */
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

#if 0
#define wipetcrc()	(etcrc&&(etcrc=0,closerc(),1))
#else
static int wipetcrc P((void))	  /* stupid function to avoid a compiler bug */
{ if(etcrc)
   { etcrc=0;closerc();
     return 1;
   }
  return 0;
}
#endif

main(argc,argv)const char*const argv[];
{ register char*chp,*chp2;register int i;int suppmunreadable;
#if 0				/* enable this if you want to trace procmail */
  kill(getpid(),SIGSTOP);/*raise(SIGSTOP);*/
#endif
  newid();
  ;{ int presenviron,Deliverymode,override;char*fromwhom=0;
     const char*idhint=0;gid_t egid=getegid();
     Deliverymode=mailfilter=override=0;
     Openlog(procmailn,LOG_PID,LOG_MAIL);		  /* for the syslogd */
     if(argc)			       /* sanity check, any argument at all? */
      { Deliverymode=strncmp(lastdirsep(argv0=argv[0]),procmailn,
	 STRLEN(procmailn));
	for(presenviron=argc=0;(chp2=(char*)argv[++argc])&&*chp2=='-';)
	   for(;;)				       /* processing options */
	    { switch(*++chp2)
	       { case VERSIONOPT:elog(procmailn);elog(VERSION);
		    elog("\nLocking strategies:\tdotlocking");
#ifndef NOfcntl_lock
		    elog(", fcntl()");		    /* a peek under the hood */
#endif
#ifdef USElockf
		    elog(", lockf()");
#endif
#ifdef USEflock
		    elog(", flock()");
#endif
		    elog("\nDefault rcfile:\t\t");elog(pmrc);
		    elog("\nYour system mailbox:\t");
		    elog(auth_mailboxname(auth_finduid(getuid(),0)));
		    elog(newline);
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
		  { static const char*argv1[]={"",0};
		    if(*++chp2)
		       goto setarg;
		    else if(chp2=(char*)argv[argc+1])
		     { argc++;
setarg:		       *argv1=chp2;restargv=argv1;crestarg=1;
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
		 case '-':
		    if(!*chp2)
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
     if(Deliverymode&&!(chp=chp2))
	nlog(misrecpt),Deliverymode=0;
last_option:
     if(Deliverymode&&presenviron)
      { presenviron=0;					   /* -d disables -p */
	goto conflopt;
      }
     if(mailfilter)
      { if(Deliverymode)				 /* -d supersedes -m */
	 { mailfilter=0;
	   goto conflopt;
	 }
	if(crestarg)				     /* -m will supersede -a */
conflopt:  nlog(conflicting),elog("options\n"),elog(pmusage);
      }
     if(!Deliverymode)
	idhint=getenv(lgname);
     if(!presenviron)				     /* drop the environment */
      { const char**emax=(const char**)environ,**ep,*const*kp;
	static const char*const keepenv[]=KEEPENV;
	for(kp=keepenv;*kp;kp++)		     /* preserve a happy few */
	   for(i=strlen(*kp),ep=emax;chp2=(char*)*ep;ep++)
	      if(!strncmp(*kp,chp2,(size_t)i)&&(chp2[i]=='='||chp2[i-1]=='_'))
	       { *ep= *emax;*emax++=chp2;			 /* swap 'em */
		 break;
	       }
	*emax=0;					    /* drop the rest */
      }
#ifdef LD_ENV_FIX
     ;{ const char**emax=(const char**)environ,**ep;
	static const char*ld_[]=
	 {"LD_","_RLD","LIBPATH=","ELF_LD_","AOUT_LD_",0};
	for(ep=emax;*emax;emax++);	  /* find the end of the environment */
	for(;*ep;ep++)
	 { const char**ldp,*p;
	   for(ldp=ld_;p= *ldp++;)
	      if(!strncmp(*ep,p,strlen(p)))	/* if it starts with LD_ (or */
	       { *ep--= *--emax;*emax=0;       /* similar) copy from the end */
		 break;
	       }
	 }
      }
#endif /* LD_ENV_FIX */
     ;{ auth_identity*pass,*passinvk;auth_identity*spassinvk;int privs;
	uid_t euid=geteuid();
	spassinvk=auth_newid();passinvk=savepass(spassinvk,uid=getuid());
	privs=1;gid=getgid();
	;{ static const char*const trusted_ids[]=TRUSTED_IDS;
	   if(Deliverymode&&*trusted_ids&&uid!=euid)
	    { struct group*grp;const char*const*kp;
	      if(passinvk)		      /* check out the invoker's uid */
		 for(chp2=(char*)auth_username(passinvk),kp=trusted_ids;*kp;)
		    if(!strcmp(chp2,*kp++)) /* is it amongst the privileged? */
		       goto privileged;
	      if(grp=getgrgid(gid))	      /* check out the invoker's gid */
		 for(chp2=grp->gr_name,kp=trusted_ids;*kp;)
		    if(!strcmp(chp2,*kp++))   /* is it among the privileged? */
		       goto privileged;
	      privs=0;
	    }
	 }
privileged:				       /* move stdout out of the way */
	endgrent();doumask(INIT_UMASK);
	while((savstdout=rdup(STDOUT))<=STDERR)
	 { rclose(savstdout);
	   if(0>(savstdout=opena(devnull)))
	      goto nodevnull;
	   syslog(LOG_EMERG,"Descriptor %d was not open\n",savstdout);
	 }
	fclose(stdout);rclose(STDOUT);			/* just to make sure */
	if(0>opena(devnull))
nodevnull:
	 { writeerr(devnull);syslog(LOG_EMERG,slogstr,errwwriting,devnull);
	   return EX_OSFILE;			     /* couldn't open stdout */
	 }
#ifdef console
	opnlog(console);
#endif
	setbuf(stdin,(char*)0);mallocbuffers(linebuf);
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
	signal(SIGPIPE,SIG_IGN);qsignal(SIGTERM,srequeue);
	qsignal(SIGINT,sbounce);qsignal(SIGHUP,sbounce);
	qsignal(SIGQUIT,slose);signal(SIGALRM,(void(*)())ftimeout);
	ultstr(0,(unsigned long)uid,buf);filled=0;
	if(!passinvk||!(chp2=(char*)auth_username(passinvk)))
	   chp2=buf;
	;{ const char*fwhom;size_t lfr,linv;int tstamp;
	   tstamp=fromwhom&&*fromwhom==REFRESH_TIME&&!fromwhom[1];fwhom=chp2;
	   if(fromwhom&&!tstamp)
	    { if(!privs&&!strcmp(fromwhom,fwhom))
		 privs=1; /* if -f user is the same as the invoker, allow it */
	      if(!privs&&fromwhom&&override)
	       { if(verbose)
		    nlog(insufprivs);		      /* ignore the bogus -f */
		 syslog(LOG_ERR,slogstr,attemptst,fwhom);fromwhom=0;
	       }
	      else
		 fwhom=fromwhom;
	    }
	   thebody=themail=
	    malloc(2*linebuf+(lfr=strlen(fwhom))+(linv=strlen(chp2)));
	   if(Deliverymode||fromwhom)  /* need to peek for a leading From_ ? */
	    { char*rstart;int r;static const char Fakefield[]=FAKE_FIELD;
	      ;{ time_t t;				 /* the current time */
		 t=time((time_t*)0);strcat(strcpy(buf2,"  "),ctime(&t));
	       }
	      lfr+=STRLEN(From_)+(r=strlen(buf2));
	      if(tstamp)
		 tstamp=r;			   /* save time stamp length */
	      if(privs)					 /* privileged user? */
		 linv=0;		 /* yes, so no need to insert >From_ */
	      else
		 linv+=STRLEN(Fakefield)+r;
	      while(1==(r=rread(STDIN,themail,1))&&*themail=='\n');
	      i=0;rstart=themail;			     /* skip garbage */
	      if(r>0&&STRLEN(From_)<=(i=rread(	      /* is it a From_ line? */
	       STDIN,rstart+1,(int)(linebuf-2-1))+1)&&eqFrom_(themail))
	       { rstart[i]='\0';
		 if(!(rstart=strchr(rstart,'\n')))
		  { do				     /* drop long From_ line */
		     { if((i=rread(STDIN,themail,(int)(linebuf-2)))<=0)
			  break;
		       themail[i]='\0';		  /* terminate it for strchr */
		     }
		    while(!(rstart=strchr(themail,'\n')));
		    i=rstart?i-(++rstart-themail):0;
		    goto no_from;
		  }
		 ;{ size_t tfrl;
		    i-=tfrl= ++rstart-themail;	     /* demarcate From_ line */
		    if(Deliverymode&&override&&!privs)
		     { if(verbose)		  /* discard the bogus From_ */
			  nlog(insufprivs);
		       syslog(LOG_ERR,slogstr,attemptst,fwhom);
		       goto no_from;
		     }
		    if(tstamp)
		       lfr=findtstamp(themail+STRLEN(From_),rstart)
			-themail+tstamp;
		    else if(!fromwhom)	       /* leave the From_ line alone */
		       if(linv)				      /* fake alert? */
			  lfr=tfrl;  /* yes, so separate From_ from the rest */
		       else
			  lfr=0,i+=tfrl;	/* no, tack it onto the rest */
		  }
	       }
	      else
no_from:       { tstamp=0;	   /* no existing From_, so nothing to stamp */
		 if(!fromwhom)					  /* no -f ? */
		    linv=0;			  /* then it can't be a fake */
	       }
	      filled=lfr+linv+i;		    /* From_ + >From_ + rest */
	      if(lfr||linv)	     /* move read text beyond our From_ line */
	       { r= *rstart;tmemmove(themail+lfr+linv,rstart,i);
		 rstart=themail+lfr;	      /* skip the From_ line, if any */
		 if(!linv)				    /* no fake alert */
		  { rstart[-tstamp]='\0';	       /* where do we append */
		    if(!tstamp)		 /* no timestamp, so generate it all */
		       strcat(strcpy(themail,From_),fwhom);	/* From user */
		  }
		 else
		  { if(lfr)			/* did we skip a From_ line? */
		       if(tstamp)	 /* copy the timestamp over the tail */
			  strcpy(rstart-tstamp,buf2);
		       else if(fromwhom)		 /* whole new From_? */
			  strcat(strcat(strcpy(themail,From_),fwhom),buf2);
		    strcat(strcpy(rstart,Fakefield),chp2);     /* fake alert */
		  }			  /* overwrite the trailing \0 again */
		 strcat(themail,buf2);themail[lfr+linv]=r;
	       }
	    }
	 }
	readmail(0,0L);			      /* read in the mail completely */
	if(Deliverymode)
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
		  }
	    }
	   while(chp=(char*)argv[argc]);
	gargv=argv+argc;suppmunreadable=verbose; /* save it for nextrcfile() */
	if(Deliverymode)	/* try recipient without changing case first */
	 { if(!(pass=auth_finduser(chp2,-1)))	    /* chp2 is the recipient */
	    { static const char unkuser[]="Unknown user";
	      nlog(unkuser);logqnl(chp2);syslog(LOG_ERR,slogstr,unkuser,chp2);
	      return EX_NOUSER;			/* we don't handle strangers */
	    }
	   if(enoughprivs(passinvk,euid,egid,auth_whatuid(pass),
	    auth_whatgid(pass)))
	      goto Setuser;
	   nlog(insufprivs);
	   syslog(LOG_CRIT,"Insufficient privileges to deliver to \"%s\"\n",
	    chp2);
	   return EX_NOPERM;	      /* need more mana, decline the request */
	 }
	else
	 { suppmunreadable=nextrcfile();
	   if(presenviron)		      /* preserving the environment? */
	      etcrc=0;				    /* don't read etcrc then */
	   if(suppmunreadable)			     /* command-line rcfile? */
	      etcrc=0,scomsat=DEFcomsat;	  /* forget etcrc and comsat */
	   if(mailfilter)
	    { if(!suppmunreadable)
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
		       suppmunreadable!=2||   /* so are variable assignments */
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
	if(idhint&&(pass=auth_finduser((char*)idhint,0))&&
	    passinvk&&auth_whatuid(passinvk)==auth_whatuid(pass)||
	   (pass=passinvk))
	  /*
	   *	set preferred uid to the intended recipient
	   */
Setuser: { gid=auth_whatgid(pass);uid=auth_whatuid(pass);
	   if(!*(chp=(char*)auth_username(pass)))
	      chp=buf;
	   setdef(lgname,chp);setdef(home,auth_homedir(pass));
	   if(euid==ROOT_uid)
	      initgroups(chp,gid);
	   endgrent();
	   if(!*(chp=(char*)auth_shell(pass)))
	      chp=(char*)binsh;
	   setdef(shell,chp);setdef(orgmail,auth_mailboxname(pass));
	 }
	else		 /* user could not be found, set reasonable defaults */
	  /*
	   *	to prevent security holes, drop any privileges now
	   */
	 { setdef(lgname,buf);setdef(home,RootDir);setdef(shell,binsh);
	   setdef(orgmail,"/tmp/dead.letter");setids();
	 }
	endpwent();auth_freeid(spassinvk);
      }
     if(!presenviron||!mailfilter)	  /* by default override environment */
      { setdef(host,hostname());sputenv(lastfolder);sputenv(exitcode);
	initdefenv();
	;{ const char*const*kp;static const char*const prestenv[]=PRESTENV;
	   for(kp=prestenv;*kp;)	/* preset some environment variables */
	    { strcpy((char*)(sgetcp=buf2),*kp++);
	      if(readparse(buf,sgetc,2))
		 setoverflow();
	      else
		 sputenv(buf);
	    }
	 }
      }		/* set fdefault and find out the name of our system lockfile */
     sgetcp=fdefault;
     if(readparse(buf,sgetc,2))		   /* uh, Houston, we have a problem */
      { nlog(orgmail);elog(pathtoolong);elog(newline);
	syslog(LOG_CRIT,"%s%s for LINEBUF for uid \"%lu\"\n",orgmail,
	 pathtoolong,(unsigned long)uid);
	fdefault="";
	goto nix_sysmbox;
      }
     fdefault=tstrdup(buf);
     strcpy(chp2=strchr(strcpy(buf,chp=(char*)getenv(orgmail)),'\0'),lockext);
     defdeflock=tstrdup(buf);sgid=egid;accspooldir=3;	/* presumed innocent */
     if(mailfilter||!screenmailbox(chp,chp2,egid,Deliverymode))
nix_sysmbox:
      { rcst_nosgid();sputenv(orgmail);	 /* nix delivering to system mailbox */
	if(!strcmp(chp,fdefault))			/* DEFAULT the same? */
	   free((char*)fdefault),fdefault="";			 /* so panic */
      }						/* bad news, be conservative */
     doumask(INIT_UMASK);
     while(chp=(char*)argv[argc])      /* interpret command line specs first */
      { argc++;
	if(!asenvcpy(chp)&&mailfilter)
	 { gargv= &nullp;			 /* stop at the first rcfile */
	   for(restargv=argv+argc;restargv[crestarg];crestarg++);
	   break;
	 }
	resettmout();
      }
   }
  ;{ int lastsucc,lastcond,prevcond;struct dyna_long ifstack;
     ifstack.filled=ifstack.tspace=0;ifstack.offs=0;
     if(etcrc)		  /* do we start with an /etc/procmailrc file first? */
      { if(0<=bopen(etcrc))
	 { yell(drcfile,etcrc);
#if !DEFverbose
	   if(rcstate!=rc_NORMAL)
	      verbose=0;		    /* no peeking in /etc/procmailrc */
#endif
	   goto startrc;
	 }
	etcrc=0;					     /* no such file */
      }
     do					     /* main rcfile interpreter loop */
      { resettmout();
	if(rc<0)					 /* open new rc file */
	 { struct stat stbuf;
	  /*
	   *	if we happen to be still running as root, and the rcfile
	   *	is mounted on a secure NFS-partition, we might not be able
	   *	to access it, so check if we can stat it or don't need any
	   *	sgid privileges, if yes, drop all privs and set uid to
	   *	the recipient beforehand
	   */
	   goto findrc;
	   do
	    { if(suppmunreadable)	  /* should we supress this message? */
fake_rc:	 readerr(buf);
	      if(!nextrcfile())		      /* not available? try the next */
	       { skiprc=0;
		 goto nomore_rc;
	       }
	      suppmunreadable=0;	      /* keep the current directory, */
findrc:	      i=0;			      /* default rcfile, or neither? */
	      if(rcfile==pmrc&&(i=2))	    /* the default .procmailrc file? */
	       { if(*strcpy((char*)(rcfile=buf2),pmrc2buf())=='\0')
pm_overflow:	  { strcpy(buf,pmrc);
		    goto fake_rc;
		  }
	       }
	      else if(strchr(dirsep,*rcfile)||		   /* absolute path? */
		 (mailfilter||*rcfile==chCURDIR&&strchr(dirsep,rcfile[1]))&&
		 (i=1))				     /* mailfilter or ./ pfx */
		 strcpy(buf,rcfile);	/* do not put anything in front then */
	      else		     /* prepend default procmailrc directory */
	       { *(chp=lastdirsep(pmrc2buf()))='\0';
		 if(buf[0]=='\0')
		    goto pm_overflow;		  /* overflow in pmrc2buf()? */
		 else
		    strcpy(chp,rcfile);			/* append the rcfile */
	       }
	      if(mailfilter!=2&&(rcstate==rc_NOSGID||	 /* nothing special? */
		  !rcstate&&!stat(buf,&stbuf)))	  /* don't need privilege or */
		 setids();		  /* it's accessible?  Transmogrify! */
	    }
	   while(0>bopen(buf));			   /* try opening the rcfile */
	   if(rcstate!=rc_NORMAL&&mailfilter!=2)      /* if it isn't special */
	    { closerc();			    /* but we are, close it, */
	      setids();			  /* transmogrify to prevent peeking */
	      if(0>bopen(buf))				    /* and try again */
		 goto fake_rc;
	    }
#ifndef NOfstat
	   if(fstat(rc,&stbuf))				    /* the right way */
#else
	   if(stat(buf,&stbuf))				  /* the best we can */
#endif
	    { static const char susprcf[]="Suspicious rcfile";
susp_rc:      closerc();nlog(susprcf);logqnl(buf);
	      syslog(LOG_ALERT,slogstr,susprcf,buf);
	      goto fake_rc;
	    }
	   if(mailfilter==2)		 /* change now if we haven't already */
	      setids();
	   erestrict=1;			      /* possibly restrict execs now */
	   if(i==1)		  /* opened rcfile in the current directory? */
	    { if(!didchd)
		 setmaildir(curdir);
	    }
	   else
	     /*
	      * OK, so now we have opened an absolute rcfile, but for security
	      * reasons we only accept it if it is owned by the recipient or
	      * root and is not world writable, and the directory it is in is
	      * not world writable or has the sticky bit set.  If this is the
	      * default rcfile then we also outlaw group writibility.
	      */
	    { register char c= *(chp=lastdirsep(buf));
	      if(((stbuf.st_uid!=uid&&stbuf.st_uid!=ROOT_uid||
		   stbuf.st_mode&S_IWOTH||
		   i&&stbuf.st_mode&S_IWGRP&&(NO_CHECK_stgid||stbuf.st_gid!=gid)
		  )&&strcmp(devnull,buf)||    /* /dev/null is a special case */
		 (*chp='\0',stat(buf,&stbuf))||
#ifdef CAN_chown
		 !(stbuf.st_mode&S_ISVTX)&&
#endif
		 ((stbuf.st_mode&(S_IWOTH|S_IXOTH))==(S_IWOTH|S_IXOTH)||
		  i&&(stbuf.st_mode&(S_IWGRP|S_IXGRP))==(S_IWGRP|S_IXGRP)
		   &&(NO_CHECK_stgid||stbuf.st_gid!=gid))))
	       { *chp=c;
		 goto susp_rc;
	       }
	      *chp=c;
	    }
	  /*
	   *	set uid back to recipient in any case, since we might just
	   *	have opened his/her .procmailrc (don't remove these, since
	   *	the rcfile might have been created after the first stat)
	   */
	   yell(drcfile,buf);
	   if(!didchd)			       /* have we done this already? */
	    { if((chp=lastdirsep(pmrc2buf()))>buf)	/* not the root dir? */
		 chp[-1]='\0';		     /* eliminate trailing separator */
	      if(buf[0]=='\0')					   /* arrrg! */
	       { nlog("procmailrc");elog(pathtoolong);elog(newline);
		 syslog(LOG_CRIT,"procmailrc%s for LINEBUF for uid \"%lu\"\n",
		  pathtoolong,(unsigned long)uid);
		 goto nomore_rc;		    /* just save it and pray */
	       }
	      if(chdir(chp=buf))      /* no, well, then try an initial chdir */
	       { chderr(buf);
		 if(chdir(chp=(char*)tgetenv(home)))
		    chderr(chp),chp=(char*)curdir;
	       }
	      setmaildir(chp);
	    }
startrc:   lastsucc=lastcond=prevcond=0;
	 }
	unlock(&loclock);			/* unlock any local lockfile */
	goto commint;
	do
	 { skipline();
commint:   do skipspace();				  /* skip whitespace */
	   while(testB('\n'));
	 }
	while(testB('#'));				   /* no comment :-) */
	if(testB(':'))				       /* check for a recipe */
	 { int locknext,succeed;char*startchar;long tobesent;
	   static char flags[maxindex(exflags)];
	   do
	    { int nrcond;
	      if(readparse(buf,getb,0))
		 goto nextrc;			      /* give up on this one */
	      ;{ char*chp3;
		 nrcond=strtol(buf,&chp3,10);chp=chp3;
	       }
	      if(chp==buf)				 /* no number parsed */
		 nrcond= -1;
	      if(tolock)	 /* clear temporary buffer for lockfile name */
		 free(tolock);
	      for(i=maxindex(flags);flags[i]=0,i--;);	  /* clear the flags */
	      for(tolock=0,locknext=0;;)
	       { chp=skpspace(chp);
		 switch(i= *chp++)
		  { default:
		       if(!(chp2=strchr(exflags,i)))	    /* a valid flag? */
			{ chp--;
			  break;
			}
		       flags[chp2-exflags]=1;		     /* set the flag */
		    case '\0':
		       if(chp!=Tmnate)		/* if not the real end, skip */
			  continue;
		       break;
		    case ':':locknext=1;    /* yep, local lockfile specified */
		       if(*chp||++chp!=Tmnate)
			  tolock=tstrdup(chp),chp=strchr(chp,'\0')+1;
		  }
		 concatenate(chp);skipped(chp);		/* display leftovers */
		 break;
	       }			      /* parse & test the conditions */
	      i=conditions(flags,prevcond,lastsucc,lastcond,nrcond);
	      if(!flags[ALSO_NEXT_RECIPE]&&!flags[ALSO_N_IF_SUCC])
		 lastcond=i==1;		   /* save the outcome for posterity */
	      if(!prevcond||!flags[ELSE_DO])
		 prevcond=i==1;	 /* same here, for `else if' like constructs */
	    }
	   while(i==2);			     /* missing in action, reiterate */
	   startchar=themail;tobesent=filled;
	   if(flags[PASS_HEAD])			    /* body, header or both? */
	    { if(!flags[PASS_BODY])
		 tobesent=thebody-themail;
	    }
	   else if(flags[PASS_BODY])
	      tobesent-=(startchar=thebody)-themail;
	   Stdout=0;succeed=sh=0;
	   pwait=flags[WAIT_EXIT]|flags[WAIT_EXIT_QUIET]<<1;
	   ignwerr=flags[IGNORE_WRITERR];skipspace();
	   if(i)
	      zombiecollect(),concon('\n');
progrm:	   if(testB('!'))				 /* forward the mail */
	    { if(!i)
		 skiprc++;
	      chp=strchr(strcpy(buf,sendmail),'\0');
	      if(*flagsendmail)
		 chp=strchr(strcpy(chp+1,flagsendmail),'\0');
	      if(readparse(chp+1,getb,0))
		 goto fail;
	      if(i)
	       { if(startchar==themail)
		  { startchar[filled]='\0';		     /* just in case */
		    if(eqFrom_(startchar))    /* leave off any leading From_ */
		       do
			  while(i= *startchar++,--tobesent&&i!='\n');
		       while(*startchar=='>');
		  }				 /* it confuses some mailers */
		 goto forward;
	       }
	      skiprc--;
	    }
	   else if(testB('|'))				    /* pipe the mail */
	    { if(getlline(buf2))		 /* get the command to start */
		 goto commint;
	      if(i)
	       { metaparse(buf2);
		 if(!sh&&buf+1==Tmnate)		      /* just a pipe symbol? */
		  { *buf='|';*(char*)(Tmnate++)='\0';		  /* fake it */
		    goto tostdout;
		  }
forward:	 if(locknext)
		  { if(!tolock)	   /* an explicit lockfile specified already */
		     { *buf2='\0';  /* find the implicit lockfile ('>>name') */
		       for(chp=buf;i= *chp++;)
			  if(i=='>'&&*chp=='>')
			   { chp=skpspace(chp+1);
			     tmemmove(buf2,chp,i=strcspn(chp,EOFName));
			     buf2[i]='\0';
			     if(sh)	 /* expand any environment variables */
			      { chp=tstrdup(buf);sgetcp=buf2;
				if(readparse(buf,sgetc,0))
				 { *buf2='\0';
				   goto nolock;
				 }
				strcpy(buf2,buf);strcpy(buf,chp);free(chp);
			      }
			     break;
			   }
		       if(!*buf2)
nolock:			{ nlog("Couldn't determine implicit lockfile from");
			  logqnl(buf);
			}
		     }
		    lcllock();
		    if(!pwait)		/* try and protect the user from his */
		       pwait=2;			   /* blissful ignorance :-) */
		  }
		 rawnonl=flags[RAW_NONL];inittmout(buf);asgnlastf=1;
		 if(flags[FILTER])
		  { if(startchar==themail&&tobesent!=filled)  /* if only 'h' */
		     { if(!pipthrough(buf,startchar,tobesent))
			  readmail(1,tobesent),succeed=!pipw;
		     }
		    else if(!pipthrough(buf,startchar,tobesent))
		       filled=startchar-themail,readmail(0,0L),succeed=!pipw;
		  }
		 else if(Stdout)		  /* capturing stdout again? */
		  { if(!pipthrough(buf,startchar,tobesent))
		       succeed=1,postStdout();	  /* only parse if no errors */
		  }
		 else if(!pipin(buf,startchar,tobesent))  /* regular program */
		    if(succeed=1,flags[CONTINUE])
		       goto logsetlsucc;
		    else
		       goto frmailed;
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
		  { getlline(buf);
		    goto fail;
		  }
		 if(skipspace())
		    chp++;		   /* keep pace with argument breaks */
		 if(testB('='))		      /* is it really an assignment? */
		  { int c;
		    *chp++='=';*chp='\0';
		    if(skipspace())
		       chp++;
		    ungetb(c=getb());
		    switch(c)
		     { case '!':case '|':		  /* ok, it's a pipe */
			  if(i)
			     primeStdout(buf);
			  goto progrm;
		     }
		  }
	       }		 /* find the end, start of a nesting recipe? */
	      else if((chp=strchr(buf,'\0'))==buf&&
		      testB('{')&&
		      (*chp++='{',*chp='\0',testB(' ')||
		       testB('\t')||
		       testB('\n')))
	       { if(locknext&&!flags[CONTINUE])
		    nlog(extrns),elog("locallockfile"),elog(ignrd);
		 app_val(&ifstack,(off_t)prevcond);	    /* push prevcond */
		 app_val(&ifstack,(off_t)lastcond);	    /* push lastcond */
		 if(!i)						/* no match? */
		    skiprc++;		      /* increase the skipping level */
		 else
		  { if(locknext)
		     { *buf2='\0';lcllock();
		       if(!pwait)	/* try and protect the user from his */
			  pwait=2;		   /* blissful ignorance :-) */
		     }
		    succeed=1;inittmout(procmailn);
		    if(flags[CONTINUE])
		     { yell("Forking",procmailn);onguard();
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
			   { int excode;  /* wait for our significant other? */
			     if(pwait&&
				(excode=waitfor(pidchild))!=EXIT_SUCCESS)
			      { if(!(pwait&2)||verbose)	 /* do we report it? */
				   progerr(procmailn,excode);
				succeed=0;
			      }
			     pidchild=0;skiprc++;    /* skip over the braces */
			   }
			}
		     }
		    goto jsetlsucc;		/* definitely no logabstract */
		  }
		 continue;
	       }
	      if(!i)						/* no match? */
		 skiprc++;		  /* temporarily disable subprograms */
	      if(readparse(chp,getb,0))
fail:	       { succeed=0;setoverflow();
		 goto setlsucc;
	       }
	      if(i)
	       { if(ofiltflag)	       /* protect who use bogus filter-flags */
		    startchar=themail,tobesent=filled;	    /* whole message */
tostdout:	 strcpy(buf2,buf);rawnonl=flags[RAW_NONL];
		 if(locknext)
		    lcllock();		     /* write to a file or directory */
		 inittmout(buf);	  /* to break messed-up kernel locks */
		 if(writefolder(buf,strchr(buf,'\0')+1,startchar,tobesent,
		     ignwerr)&&
		    (succeed=1,!flags[CONTINUE]))
frmailed:	  { if(ifstack.offs)
		       free(ifstack.offs);
		    goto mailed;
		  }
logsetlsucc:	 if(succeed&&flags[CONTINUE]&&lgabstract==2)
		    logabstract(tgetenv(lastfolder));
setlsucc:	 rawnonl=0;
jsetlsucc:	 lastsucc=succeed;lasttell= -1;		       /* for comsat */
	       }
	      else
		 skiprc--;			     /* reenable subprograms */
	    }
	 }
	else if(testB('}'))					/* end block */
	 { if(ifstack.filled)		      /* restore lastcond from stack */
	    { lastcond=ifstack.offs[--ifstack.filled];
	      prevcond=ifstack.offs[--ifstack.filled];	 /* prevcond as well */
	    }
	   else
	      nlog("Closing brace unexpected\n");	      /* stack empty */
	   if(skiprc)					    /* just skipping */
	      skiprc--;					   /* decrease level */
	 }
	else				    /* then it must be an assignment */
	 { char*end=buf+linebuf;
	   if(!(chp=gobenv(buf,end)))
	    { if(!*buf)					/* skip a word first */
		 getbl(buf,buf+linebuf);		      /* then a line */
	      skipped(buf);				/* display leftovers */
	      continue;
	    }
	   if(chp==end)				      /* overflow => give up */
nextrc:	      if(poprc()||wipetcrc())
		 continue;
	      else
		 break;
	   skipspace();
	   if(testB('='))			   /* removal or assignment? */
	    { *chp='=';
	      if(readparse(++chp,getb,1))
	       { setoverflow();
		 continue;
	       }
	    }
	   else
	      *++chp='\0';		     /* throw in a second terminator */
	   if(!skiprc)
	      chp2=(char*)sputenv(buf),chp[-1]='\0',asenv(chp2);
	 }
      }						    /* main interpreter loop */
     while(rc<0||!testB(EOF)||poprc()||wipetcrc());
nomore_rc:
     if(ifstack.offs)
	free(ifstack.offs);
   }
  ;{ int succeed;
     concon('\n');succeed=0;
     if(*(chp=(char*)fdefault))				     /* DEFAULT set? */
      { int len,tlen;
	setuid(uid);
	len=strlen(chp);		   /* make sure we have enough space */
	if(linebuf<(tlen=len+strlen(lockext)+XTRAlinebuf+UNIQnamelen))
	   mallocbuffers(linebuf=tlen);	   /* to perform the lock & delivery */
	if(strcmp(chp,devnull)&&strcmp(chp,"|"))  /* neither /dev/null nor | */
	 { cat(chp,lockext);
	   if(!globlock||strcmp(buf,globlock))		  /* already locked? */
	      lockit(tstrdup(buf),&loclock);		    /* implicit lock */
	 }
	if(writefolder(chp,(char*)0,themail,filled,0))		  /* default */
	   succeed=1;
      }						       /* if all else failed */
     if(!succeed&&*(chp2=(char*)tgetenv(orgmail))&&strcmp(chp2,chp))
      { rawnonl=0;
	if(writefolder(chp2,(char*)0,themail,filled,0))	      /* don't panic */
	   succeed=1;				      /* try the last resort */
      }
     if(succeed)				     /* should we panic now? */
mailed: rawnonl=0,retval=EXIT_SUCCESS;	  /* we're home free, mail delivered */
   }
  unlock(&loclock);Terminate();
}

int eqFrom_(a)const char*const a;
{ return !strncmp(a,From_,STRLEN(From_));
}
