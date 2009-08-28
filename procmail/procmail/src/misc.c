/************************************************************************
 *	Miscellaneous routines used by procmail				*
 *									*
 *	Copyright (c) 1990-1999, S.R. van den Berg, The Netherlands	*
 *	Copyright (c) 1999-2001, Philip Guenther, The United States	*
 *							of America	*
 *	#include "../README"						*
 ************************************************************************/
#ifdef RCS
static /*const*/char rcsid[]=
 "$Id: misc.c,v 1.117 2001/06/26 08:46:48 guenther Exp $";
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
#include "comsat.h"
#include "mailfold.h"
#include "lastdirsep.h"
#include "memblk.h"
#include "authenticate.h"
#include "variables.h"
#include "shell.h"

		       /* line buffered to keep concurrent entries untangled */
void elog(newt)const char*const newt;
{ int lnew;size_t i;static size_t lold,lmax;static char*old;
  if(lcking&lck_LOGGING)			 /* reentered via sighandler */
     lold=lmax=0;		       /* so give up on any buffered message */
  i=lold+(lnew=strlen(newt));	   /* calculate additional and total lengths */
  if(lnew&&				/* if this is not a forced flush and */
   (lmax>=i||		      /* either we have enough room in the buffer or */
    (MAXlogbuf>=i&&			 /* the buffer won't get too big and */
     !nextexit)))	    /* we're not in a signal handler, then it's safe */
   { if(i>lmax)				      /* to use or expand the buffer */
      { char*p;size_t newmax=lmax*2;	 /* exponential expansion by default */
	if(i>newmax)				   /* ...unless we need more */
	   newmax=i;
	if(MINlogbuf>newmax)		    /* ...or that would be too small */
	   newmax=MINlogbuf;
	lcking|=lck_LOGGING;		      /* about to change old or lmax */
	if(p=lmax?frealloc(old,newmax):fmalloc(newmax))/* fragile allocation */
	   lmax=newmax,old=p;				/* update the values */
	lcking&=~lck_LOGGING;		       /* okay, they're stable again */
	if(!p)				      /* couldn't expand the buffer? */
	   goto flush;					/* then flush it now */
      }				    /* okay, we now have enough buffer space */
     tmemmove(old+lold,newt,lnew);		      /* append the new text */
     lnew=0;lold=i;		    /* the text in newt is now in the buffer */
     if(old[i-1]!='\n')			    /* if we don't need to flush now */
	return;						  /* then we're done */
   }
flush:
#ifndef O_CREAT
  lseek(STDERR,(off_t)0,SEEK_END);	  /* locking should be done actually */
#endif
  if(lold)					       /* anything buffered? */
   { rwrite(STDERR,old,lold);
     lold=0;					 /* we never free the buffer */
   }
  if(lnew)
     rwrite(STDERR,newt,lnew);
}

void ignoreterm P((void))
{ signal(SIGTERM,SIG_IGN);signal(SIGHUP,SIG_IGN);signal(SIGINT,SIG_IGN);
  signal(SIGQUIT,SIG_IGN);
}

void shutdesc P((void))
{ rclose(savstdout);closelog();closerc();
}

/* On systems with `capabilities', setuid/setgid can fail for root! */
void checkroot(c,Xid)const int c;const unsigned long Xid;
{ uid_t eff;
  if((eff=geteuid())!=ROOT_uid&&getuid()!=ROOT_uid)
     return;
  syslog(LOG_CRIT,"set%cid(%lu) failed with ruid/euid = %lu/%lu",c,Xid,
   (unsigned long)getuid(),(unsigned long)eff);
  nlog(insufprivs);
  exit(EX_NOPERM);
}

void setids P((void))
{ if(privileged)
   { if(setRgid(gid)&&	/* due to these !@#$%^&*() POSIX semantics, setgid() */
      setgid(gid))	   /* sets the saved gid as well; we can't use that! */
	checkroot('g',(unsigned long)gid);	 /* did setgid fail as root? */
     setruid(uid);
     if(setuid(uid))				     /* "This cannot happen" */
	checkroot('u',(unsigned long)uid);			/* Whoops... */
     setegid(gid);
     privileged=0;
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

void progerr(line,xitcode,okay)const char*const line;int xitcode,okay;
{ charNUM(num,thepid);
  nlog(okay?"Non-zero exitcode (":"Program failure (");
  ltstr(0,(long)xitcode,num);elog(num);elog(okay?") from":") of");
  logqnl(line);
}

void chderr(dir)const char*const dir;
{ nlog("Couldn't chdir to");logqnl(dir);
}

void readerr(file)const char*const file;
{ nlog("Couldn't read");logqnl(file);
}

int buildpath(name,path,file)const char*name,*const path,*const file;
{ static const char toolong[]=" path too long",
   notabsolute[]=" is not an absolute path";
  sgetcp=path;
  if(readparse(buf,sgetc,2,0))
   { syslog(LOG_CRIT,"%s%s for LINEBUF for uid \"%lu\"\n",name,toolong,
      (unsigned long)uid);
bad: nlog(name);elog(toolong);elog(newline);
     return 1;
   }
  if(!strchr(dirsep,buf[0]))
   { nlog(name);elog(notabsolute);elog(newline);
     syslog(LOG_CRIT,"%s%s for uid \"%lu\"\n",name,notabsolute,
      (unsigned long)uid);
     return 1;
   }
  if(file)
   { char*chp=strchr(buf,'\0');
     if(chp-buf+strlen(file)+2>linebuf)			  /* +2 for / and \0 */
      { name="full rcfile";		  /* this should be passed in... XXX */
	goto bad;
      }
     *chp++= *MCDIRSEP_;
     strcpy(chp,file);				      /* append the filename */
   }
  return 0;
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

static time_t oldtime;

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
  if(verbose&&!nextexit&&oldtime!=(newtime=time((time_t*)0)))  /* don't call */
   { charNUM(num,thepid);			  /* ctime from a sighandler */
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

char*tstrdup(a)const char*const a;
{ int i;
  i=strlen(a)+1;
  return tmemmove(malloc(i),a,i);
}

char*cstr(a,b)char*const a;const char*const b;	/* dynamic buffer management */
{ if(a)
     free(a);
  return tstrdup(b);
}

void onguard P((void))
{ lcking|=lck_DELAYSIG;
}

void offguard P((void))
{ lcking&=~lck_DELAYSIG;
  if(nextexit==1)	  /* make sure we are not inside Terminate() already */
     elog(newline),Terminate();
}

static void sterminate P((void))
{ static const char*const msg[]={"memory","fork",	  /* crosscheck with */
   "a file descriptor","a kernel-lock"};	  /* lck_ defs in procmail.h */
  ignoreterm();
  if(pidchild>0)	    /* don't kill what is not ours, we might be root */
     kill(pidchild,SIGTERM);
  if(!nextexit)
   { nextexit=1;nlog("Terminating prematurely");
     if(!(lcking&lck_DELAYSIG))
      { register unsigned i,j;
	if(i=(lcking&~lck__NOMSG)>>1)
	 { elog(whilstwfor);
	   for(j=0;!((i>>=1)&1);j++);
	   elog(msg[j]);
	 }
	elog(newline);Terminate();
      }
   }
}

int fakedelivery;

void Terminate P((void))
{ ignoreterm();
  if(getpid()==thepid)
   { const char*lstfolder;
     if(retval!=EXIT_SUCCESS)
      { lasttell= -1;				   /* mark it for sendcomsat */
	lstfolder=fakedelivery?"**Lost**":
	 retval==EX_TEMPFAIL?"**Requeued**":"**Bounced**";
	sendcomsat(lstfolder);
      }
     else
      { lstfolder=tgetenv(lastfolder);
	sendcomsat(0);
      }
     logabstract(lstfolder);
     if(!nextexit)			/* these are unsafe from sighandlers */
      { shutdesc();
	exectrap(traps);
	fdunlock();
      }
     nextexit=2;unlock(&loclock);unlock(&globlock);
   }					/* flush the logfile & exit procmail */
  elog(empty);
  _exit(retvl2!=EXIT_SUCCESS?retvl2:		      /* unsuccessful child? */
   fakedelivery==2?EXIT_SUCCESS:		   /* told to throw it away? */
   retval);					/* okay, use the real status */
}

void suspend P((void))
{ ssleep((unsigned)suspendv);
}

static void srequeue P((void))
{ retval=EX_TEMPFAIL;sterminate();
}

static void slose P((void))
{ fakedelivery=2;sterminate();
}

static void sbounce P((void))
{ retval=EX_CANTCREAT;sterminate();
}

void setupsigs P((void))
{ qsignal(SIGTERM,srequeue);qsignal(SIGINT,sbounce);
  qsignal(SIGHUP,sbounce);qsignal(SIGQUIT,slose);
  signal(SIGALRM,(void(*)())ftimeout);
}

static void squeeze(target)char*target;
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

const char*newdynstring(adrp,chp)struct dynstring**const adrp;
 const char*const chp;
{ struct dynstring*curr;size_t len;
  curr=malloc(ioffsetof(struct dynstring,ename[0])+(len=strlen(chp)+1));
  tmemmove(curr->ename,chp,len);curr->enext= *adrp;*adrp=curr;
  return curr->ename;
}

void*app_val_(sp,size)struct dyna_array*const sp;int size;
{ if(sp->filled==sp->tspace)			    /* growth limit reached? */
   { size_t len=(sp->tspace+=4)*size;
     sp->vals=sp->vals?realloc(sp->vals,len):malloc(len);	   /* expand */
   }
  return &sp->vals[size*sp->filled++];			     /* append to it */
}

			     /* lifted out of main() to reduce main()'s size */
int conditions(flags,prevcond,lastsucc,lastcond,skipping,nrcond)char flags[];
 const int prevcond,lastsucc,lastcond,skipping;int nrcond;
{ char*chp,*chp2,*startchar;double score;int scored,i,skippedempty;
  long tobesent;static const char suppressed[]=" suppressed\n";
  score=scored=0;
  if(nrcond<0)		      /* assume appropriate default nr of conditions */
     nrcond=!flags[ALSO_NEXT_RECIPE]&&!flags[ALSO_N_IF_SUCC]&&!flags[ELSE_DO]&&
      !flags[ERROR_DO];
  startchar=themail.p;tobesent=thebody-themail.p;
  if(flags[BODY_GREP])			       /* what needs to be egrepped? */
     if(flags[HEAD_GREP])
	tobesent=filled;
     else
      { startchar=thebody;tobesent=filled-tobesent;
	goto noconcat;
      }
   if(!skipping)
      concon(' ');
noconcat:
   i=!skipping;						  /* init test value */
   if(flags[ERROR_DO])
    { i&=prevcond&&!lastsucc;
      if(flags[ELSE_DO])
	 nlog(conflicting),elog("else-if-flag"),elog(suppressed),
	  flags[ELSE_DO]=0;
      if(flags[ALSO_N_IF_SUCC])
	 nlog(conflicting),elog("also-if-succeeded-flag"),elog(suppressed),
	  flags[ALSO_N_IF_SUCC]=0;
    }
   if(flags[ELSE_DO])
      i&=!prevcond;
   if(flags[ALSO_N_IF_SUCC])
      i&=lastcond&&lastsucc;
   if(flags[ALSO_NEXT_RECIPE])
      i=i&&lastcond;
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
      if(getlline(buf2,buf2+linebuf))
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
		  w=strtod(buf2,&chp3);chp2=chp3;
		  if(chp2>buf2&&*(chp2=skpspace(chp2))=='^')
		   { double x;
		     x=strtod(chp2+1,&chp3);
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
		{ if(alphanum(*(chp2=chp))==1)
		   { char*chp3;
		     while(alphanum(*++chp2));
		     if(!strncmp(chp3=skpspace(chp2),"??",2))
		      { *chp2='\0';lstartchar=themail.p;
			if(!chp[1])
			 { ltobesent=thebody-themail.p;
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
		  if(readparse(buf,sgetc,2,0)&&(i=0,1))
		     break;
		  strcpy(buf2,skpspace(buf));
		  goto copydone;
	       case '!':negate^=1;chp2=skpspace(chp);
copyrest:	  strcpy(buf,chp2);
		  continue;
	       case '?':pwait=2;metaparse(chp);inittmout(buf);ignwerr=1;
		   pipin(buf,lstartchar,ltobesent,0);
		   resettmout();
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
		  if(readparse(buf,sgetc,2,0)&&(i=0,1))
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
