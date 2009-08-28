/************************************************************************
 *	Routines related to setting up pipes and filters		*
 *									*
 *	Copyright (c) 1990-1999, S.R. van den Berg, The Netherlands	*
 *	Copyright (c) 1998-2001, Philip Guenther, The United States	*
 *						of America		*
 *	#include "../README"						*
 ************************************************************************/
#ifdef RCS
static /*const*/char rcsid[]=
 "$Id: pipes.c,v 1.73 2001/08/27 08:43:59 guenther Exp $";
#endif
#include "procmail.h"
#include "robust.h"
#include "shell.h"
#include "misc.h"
#include "memblk.h"
#include "pipes.h"
#include "common.h"
#include "mcommon.h"
#include "foldinfo.h"
#include "mailfold.h"
#include "goodies.h"
#include "variables.h"

static const char comma[]=",";
pid_t pidchild;
volatile time_t alrmtime;
volatile int toutflag;
static char*lastexec,*backblock;
static long backlen;	       /* length of backblock, filter recovery block */
static pid_t pidfilt;
static int pbackfd[2];			       /* the emergency backpipe :-) */
int pipw;

#define PRDO	poutfd[0]
#define PWRO	poutfd[1]
#define PRDI	pinfd[0]
#define PWRI	pinfd[1]
#define PRDB	pbackfd[0]
#define PWRB	pbackfd[1]

void inittmout(progname)const char*const progname;
{ lastexec=cstr(lastexec,progname);toutflag=0;
  alrmtime=timeoutv?time((time_t*)0)+(unsigned)timeoutv:0;
  alarm((unsigned)timeoutv);
}

void ftimeout P((void))
{ alarm(0);alrmtime=0;toutflag=1;nlog("Timeout, ");	 /* careful, killing */
  elog(pidchild>0&&!kill(pidchild,SIGTERM)?"terminating":"was waiting for");
  logqnl(lastexec);signal(SIGALRM,(void(*)())ftimeout);
}

void resettmout P((void))
{ if(alrmtime)				       /* any need to reset timeout? */
     alarm((unsigned)(alrmtime=0));			    /* reset timeout */
}

static void stermchild P((void))
{ static const char rescdata[]="Rescue of unfiltered data ";
  if(pidfilt>0)		    /* don't kill what is not ours, we might be root */
     kill(pidfilt,SIGTERM);
  rawnonl=1;				       /* give back the raw contents */
  if(PWRB<0);						/* previously unset? */
  else if(dump(PWRB,ft_PIPE,backblock,backlen))	       /* pump data back via */
     nlog(rescdata),elog("failed\n");			     /* the backpipe */
  else if(verbose||pwait!=4)		/* are we not looking the other way? */
     nlog(rescdata),elog("succeeded\n");
  exit(lexitcode);
}

static void childsetup P((void))
{ lexitcode=EX_UNAVAILABLE;qsignal(SIGTERM,stermchild);
  qsignal(SIGINT,stermchild);qsignal(SIGHUP,stermchild);
  qsignal(SIGQUIT,stermchild);shutdesc();
}

static void getstdin(pip)const int pip;
{ rclose(STDIN);rdup(pip);rclose(pip);
}

static void callnewprog(newname)const char*const newname;
{
#ifdef RESTRICT_EXEC
  if(erestrict&&uid>=RESTRICT_EXEC)
   { syslog(LOG_ERR,slogstr,"Attempt to execute",newname);
     nlog("No permission to execute");logqnl(newname);
     return;
   }
#endif
  if(sh)					 /* should we start a shell? */
   { const char*newargv[4];
     yell(executing,newname);newargv[3]=0;newargv[2]=newname;
     newargv[1]=shellflags;*newargv=tgetenv(shell);shexec(newargv);
   }
  ;{ register const char*p;int argc;
     argc=1;p=newname;	     /* If no shell, chop up the arguments ourselves */
     if(verbose)
      { nlog(executing);elog(oquote);
	goto no_1st_comma;
      }
     do					     /* show chopped up command line */
      { if(verbose)
	 { elog(comma);
no_1st_comma:
	   elog(p);
	 }
	while(*p++);
	if(verbose&&p-1==All_args&&crestarg)		  /* any "$@" found? */
	 { const char*const*walkargs=restargv;
	   goto No_1st_comma;
	   do
	    { elog(comma);
No_1st_comma: elog(*walkargs);					/* expand it */
	    }
	   while(*++walkargs);
	 }
	if(p-1==All_args)
	   argc+=crestarg-1;			       /* and account for it */
      }
     while(argc++,p!=Tmnate);
     if(verbose)
	elog(cquote);				      /* allocate argv array */
     ;{ const char**newargv;
	newargv=malloc(argc*sizeof*newargv);p=newname;argc=0;
	do
	 { newargv[argc++]=p;
	   while(*p++);
	   if(p-1==All_args&&crestarg)
	    { const char*const*walkargs=restargv;	      /* expand "$@" */
	      argc--;
	      while(newargv[argc]= *walkargs++)
		 argc++;
	    }
	 }
	while(p!=Tmnate);
	newargv[argc]=0;shexec(newargv);
      }
   }
}

int pipthrough(line,source,len)char*line,*source;const long len;
{ int pinfd[2],poutfd[2];char*eq;
  if(Stdout)
   { *(eq=strchr(Stdout,'\0')-1)='\0';			     /* chop the '=' */
     if(!(backblock=getenv(Stdout)))			/* no current value? */
	PRDB=PWRB= -1;
     else
      { backlen=strlen(backblock);
	goto pip;
      }
   }
  else
pip: rpipe(pbackfd);
  rpipe(pinfd);						 /* main pipes setup */
  if(!(pidchild=sfork()))			/* create a sending procmail */
   { if(Stdout&&backblock)
	backlen=strlen(backblock);
     else
	backblock=source,backlen=len;
     childsetup();rclose(PRDI);rclose(PRDB);
     rpipe(poutfd);rclose(STDOUT);
     if(!(pidfilt=sfork()))				/* create the filter */
      { rclose(PWRB);rclose(PWRO);rdup(PWRI);rclose(PWRI);getstdin(PRDO);
	callnewprog(line);
      }
     rclose(PWRI);rclose(PRDO);
     if(forkerr(pidfilt,line))
	rclose(PWRO),stermchild();
     if(dump(PWRO,ft_PIPE,source,len)&&!ignwerr)	 /* send in the text */
	writeerr(line),lexitcode=EX_IOERR,stermchild();	   /* to be filtered */
     ;{ int excode;	      /* optionally check the exitcode of the filter */
	if(pwait&&(excode=waitfor(pidfilt))!=EXIT_SUCCESS)
	 { pidfilt=0;
	   if(pwait&2)				  /* do we put it on report? */
	    { pwait=4;			     /* no, we'll look the other way */
	      if(verbose)
		 goto perr;
	    }
	   else
perr:	      progerr(line,excode,pwait==4);  /* I'm going to tell my mommy! */
	   stermchild();
	 }
      }
     rclose(PWRB);exit(EXIT_SUCCESS);		  /* allow parent to proceed */
   }
  rclose(PWRB);rclose(PWRI);getstdin(PRDI);
  if(forkerr(pidchild,procmailn))
     return -1;
  if(Stdout)
   { char*name;memblk temp;		    /* ick.  This is inefficient XXX */
     *eq='=';name=Stdout;Stdout=0;primeStdout(name);free(name);
     makeblock(&temp,Stdfilled);
     tmemmove(temp.p,Stdout,Stdfilled);
     readdyn(&temp,&Stdfilled,Stdfilled+backlen+1);
     Stdout=realloc(Stdout,&Stdfilled+1);
     tmemmove(Stdout,temp.p,Stdfilled+1);
     freeblock(&temp);
     retStdout(Stdout,pwait&&pipw,!backblock);
     return pipw;
   }
  return 0;		    /* we stay behind to read back the filtered text */
}

long pipin(line,source,len,asgnlastf)char*const line;char*source;long len;
 int asgnlastf;
{ int poutfd[2];
#if 0						     /* not supported (yet?) */
  if(!sh)					/* shortcut builtin commands */
   { const char*t1,*t2,*t3;
     static const char pbuiltin[]="Builtin";
     t1=strchr(line,'\0')+1;
     if(!strcmp(test,line))
      { if(t1!=Tmnate)
	 { t2=strchr(t1,'\0')+1;
	   if(t2!=Tmnate)
	    { t3=strchr(t2,'\0')+1;
	      if(t3!=Tmnate&&!strcmp(t2,"=")&&strchr(t3,'\0')==Tmnate-1)
	       { int excode;
		 if(verbose)
		  { nlog(pbuiltin);elog(oquote);elog(test);elog(comma),
		 if(!ignwerr)
		    writeerr(line);
		 else
		    len=0;
		 if(pwait&&(excode=strcmp(t1,t3)?1:EXIT_SUCCESS)!=EXIT_SUCCESS)
		  { if(!(pwait&2)||verbose)	  /* do we put it on report? */
		       progerr(line,excode,pwait&2);
		    len=1;
		  }
		 goto builtin;
	       }
	    }
	 }
      }
   }
#endif
  rpipe(poutfd);
  if(!(pidchild=sfork()))				    /* spawn program */
     rclose(PWRO),shutdesc(),getstdin(PRDO),callnewprog(line);
  rclose(PRDO);
  if(forkerr(pidchild,line))
   { rclose(PWRO);
     return -1;					    /* dump mail in the pipe */
   }
  if((len=dump(PWRO,ft_PIPE,source,len))&&(!ignwerr||(len=0)))
     writeerr(line);		       /* pipe was shut in our face, get mad */
  ;{ int excode;			    /* optionally check the exitcode */
     if(pwait&&(excode=waitfor(pidchild))!=EXIT_SUCCESS)
      { if(!(pwait&2)||verbose)			  /* do we put it on report? */
	   progerr(line,excode,pwait&2);
	len=1;
      }
   }
  pidchild=0;
builtin:
  if(!sh)
     concatenate(line);
  if(asgnlastf)
     setlastfolder(line);
  return len;
}

static char*read_read(p,left,data)char*p;long left;void*data;
{ long got;
  do
     if(0>=(got=rread(STDIN,p,left)))				/* read mail */
	return p;
  while(p+=got,left-=got);			/* change listed buffer size */
  return 0;
}

static int read_cleanup(mb,filledp,origfilled,data)memblk*mb;
 long*filledp,origfilled;void*data;
{ long oldfilled= *(long*)data;
  if(pidchild>0)
   { if(PRDB>=0)
      { getstdin(PRDB);			       /* filter ready, get backpipe */
	if(1==rread(STDIN,buf,1))		      /* backup pipe closed? */
	 { resizeblock(mb,oldfilled,0);
	   mb->p[origfilled]= *buf;
	   *filledp=origfilled+1;
	   PRDB= -1;pwait=2;		      /* break loop, definitely reap */
	   return 1;			       /* filter goofed, rescue data */
	 }
      }
     if(pwait)
	pipw=waitfor(pidchild);		      /* reap your child in any case */
   }
  pidchild=0;					/* child must be gone by now */
  return 0;
}

char*readdyn(mb,filled,oldfilled)memblk*const mb;long*const filled,oldfilled;
{ return read2blk(mb,filled,&read_read,&read_cleanup,&oldfilled);
}

char*fromprog(name,dest,max)char*name;char*const dest;size_t max;
{ int pinfd[2],poutfd[2];int i;char*p;
  concon('\n');rpipe(pinfd);inittmout(name);
  if(!(pidchild=sfork()))			/* create a sending procmail */
   { Stdout=name;childsetup();rclose(PRDI);rpipe(poutfd);rclose(STDOUT);
     if(!(pidfilt=sfork()))			     /* spawn program/filter */
	rclose(PWRO),rdup(PWRI),rclose(PWRI),getstdin(PRDO),callnewprog(name);
     rclose(PWRI);rclose(PRDO);
     if(forkerr(pidfilt,name))
	rclose(PWRO),stermchild();
     dump(PWRO,ft_PIPE,themail.p,filled);waitfor(pidfilt);exit(lexitcode);
   }
  rclose(PWRI);p=dest;
  if(!forkerr(pidchild,name))
   { name=tstrdup(name);
     while(0<(i=rread(PRDI,p,(int)max))&&(p+=i,max-=i));    /* read its lips */
     if(0<rread(PRDI,p,1))
      { nlog("Excessive output quenched from");logqnl(name);
	setoverflow();
      }
     else
	while(--p>=dest&&*p=='\n'); /* trailing newlines should be discarded */
     rclose(PRDI);free(name);
     p++;waitfor(pidchild);
   }
  else
     rclose(PRDI);
  resettmout();
  pidchild=0;*p='\0';
  return p;
}

void exectrap(tp)const char*const tp;
{ int forceret;
  forceret=setexitcode(*tp);		      /* whether TRAP is set matters */
  if(*tp)
   { int poutfd[2];
     rawnonl=0;					 /* force a trailing newline */
     metaparse(tp);concon('\n');			     /* expand $TRAP */
     rpipe(poutfd);inittmout(buf);
     if(!(pidchild=sfork()))	     /* connect stdout to stderr before exec */
      { rclose(PWRO);getstdin(PRDO);rclose(STDOUT);rdup(STDERR);
	callnewprog(buf);			      /* trap your heart out */
      }
     rclose(PRDO);					     /* neat & clean */
     if(!forkerr(pidchild,buf))
      { int newret;
	dump(PWRO,ft_PIPE,themail.p,filled);  /* try and shove down the mail */
	if((newret=waitfor(pidchild))!=EXIT_SUCCESS&&forceret==-2)
	   retval=newret;		       /* supersede the return value */
	pidchild=0;
      }
     else
	rclose(PWRO);
     resettmout();
   }
}
