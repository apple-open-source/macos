/************************************************************************
 *	Routines related to setting up pipes and filters		*
 *									*
 *	Copyright (c) 1990-1999, S.R. van den Berg, The Netherlands	*
 *	#include "../README"						*
 ************************************************************************/
#ifdef RCS
static /*const*/char rcsid[]=
 "$Id: pipes.c,v 1.1.1.1 1999/09/23 17:30:07 wsanchez Exp $";
#endif
#include "procmail.h"
#include "robust.h"
#include "shell.h"
#include "misc.h"
#include "pipes.h"
#include "common.h"
#include "cstdio.h"
#include "exopen.h"
#include "mcommon.h"
#include "goodies.h"
#include "mailfold.h"

const char exitcode[]="EXITCODE";
static const char comma[]=",";
int setxit;
pid_t pidchild;
volatile time_t alrmtime;
volatile int toutflag;
static char*lastexec,*backblock;
static long backlen;	       /* length of backblock, filter recovery block */
static pid_t pidfilt;
static int pbackfd[2];			       /* the emergency backpipe :-) */
int pipw;

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
{ if(pidfilt>0)		    /* don't kill what is not ours, we might be root */
     kill(pidfilt,SIGTERM);
  if(!Stdout)
   { static const char rescdata[]="Rescue of unfiltered data ";
     rawnonl=1;				       /* give back the raw contents */
     if(dump(PWRB,backblock,backlen))	  /* pump data back via the backpipe */
	nlog(rescdata),elog("failed\n");
     else if(verbose||pwait!=4)		/* are we not looking the other way? */
	nlog(rescdata),elog("succeeded\n");
   }
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
  if(mailfilter!=2&&erestrict&&uid>=RESTRICT_EXEC)
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
{ int pinfd[2],poutfd[2];
  if(Stdout)
     PWRB=PRDB= -1;
  else
     rpipe(pbackfd);
  rpipe(pinfd);						 /* main pipes setup */
  if(!(pidchild=sfork()))			/* create a sending procmail */
   { backblock=source;backlen=len;childsetup();rclose(PRDI);rclose(PRDB);
     rpipe(poutfd);rclose(STDOUT);
     if(!(pidfilt=sfork()))				/* create the filter */
      { rclose(PWRB);rclose(PWRO);rdup(PWRI);rclose(PWRI);getstdin(PRDO);
	callnewprog(line);
      }
     rclose(PWRI);rclose(PRDO);
     if(forkerr(pidfilt,line))
	rclose(PWRO),stermchild();
     if(dump(PWRO,source,len)&&!ignwerr)  /* send in the text to be filtered */
	writeerr(line),lexitcode=EX_IOERR,stermchild();
     ;{ int excode;	      /* optionally check the exitcode of the filter */
	if(pwait&&(excode=waitfor(pidfilt))!=EXIT_SUCCESS)
	 { pidfilt=0;
	   if(pwait&2)				  /* do we put it on report? */
	    { pwait=4;			     /* no, we'll look the other way */
	      if(verbose)
		 goto perr;
	    }
	   else
perr:	      progerr(line,excode);	      /* I'm going to tell my mommy! */
	   stermchild();
	 }
      }
     rclose(PWRB);exit(EXIT_SUCCESS);		  /* allow parent to proceed */
   }
  rclose(PWRB);rclose(PWRI);getstdin(PRDI);
  if(forkerr(pidchild,procmailn))
     return -1;
  if(Stdout)
   { retStdout(readdyn(Stdout,&Stdfilled));
     return pipw;
   }
  return 0;		    /* we stay behind to read back the filtered text */
}

long pipin(line,source,len)char*const line;char*source;long len;
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
		       progerr(line,excode);
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
  if((len=dump(PWRO,source,len))&&(!ignwerr||(len=0)))
     writeerr(line);		       /* pipe was shut in our face, get mad */
  ;{ int excode;			    /* optionally check the exitcode */
     if(pwait&&(excode=waitfor(pidchild))!=EXIT_SUCCESS)
      { if(!(pwait&2)||verbose)			  /* do we put it on report? */
	   progerr(line,excode);
	len=1;
      }
   }
  pidchild=0;
builtin:
  if(!sh)
     concatenate(line);
  setlastfolder(line);
  return len;
}

char*readdyn(bf,filled)char*bf;long*const filled;
{ int blksiz=BLKSIZ;long oldsize= *filled;unsigned shift=EXPBLKSIZ;char*np;
  for(;;)
   {
#ifdef SMALLHEAP
     if((size_t)*filled>=(size_t)(*filled+blksiz))
	lcking|=lck_MEMORY,nomemerr();
#endif				       /* dynamically adjust the buffer size */
		       /* use the real realloc so that we can retry failures */
     while(EXPBLKSIZ&&(np=0,blksiz>BLKSIZ)&&!(np=(realloc)(bf,*filled+blksiz)))
	blksiz>>=1;				  /* try a smaller increment */
     bf=EXPBLKSIZ&&np?np:realloc(bf,*filled+blksiz);		 /* last try */
jumpback:;
     ;{ int got,left=blksiz;
	do
	   if(0>=(got=rread(STDIN,bf+*filled,left)))		/* read mail */
	      goto eoffound;
	while(*filled+=got,left-=got);		/* change listed buffer size */
      }
     if(EXPBLKSIZ&&shift)				 /* room for growth? */
      { int newbs=blksiz;newbs<<=shift--;	/* capped exponential growth */
	if(blksiz<newbs)				  /* no overflowing? */
	   blksiz=newbs;				    /* yes, take me! */
      }
   }
eoffound:
  if(pidchild>0)
   { if(!Stdout)
      { getstdin(PRDB);			       /* filter ready, get backpipe */
	if(1==rread(STDIN,buf,1))		      /* backup pipe closed? */
	 { bf=realloc(bf,(*filled=oldsize+1)+blksiz);bf[oldsize]= *buf;
	   Stdout=buf;pwait=2;		      /* break loop, definitely reap */
	   goto jumpback;		       /* filter goofed, rescue data */
	 }
      }
     if(pwait)
	pipw=waitfor(pidchild);		      /* reap your child in any case */
   }
  pidchild=0;					/* child must be gone by now */
  return (np=(realloc)(bf,*filled+1))?np:bf;  /* minimise+1 for housekeeping */
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
     dump(PWRO,themail,filled);waitfor(pidfilt);exit(lexitcode);
   }
  rclose(PWRI);p=dest;
  if(!forkerr(pidchild,name))
   { name=tstrdup(name);
     while(0<(i=rread(PRDI,p,(int)max))&&(p+=i,max-=i));    /* read its lips */
     if(0<rread(PRDI,p,1))
	nlog("Excessive output quenched from"),logqnl(name);
     rclose(PRDI);free(name);
     while(--p>=dest&&*p=='\n');    /* trailing newlines should be discarded */
     p++;waitfor(pidchild);
   }
  else
     rclose(PRDI);
  pidchild=0;*p='\0';
  return p;
}

void exectrap(tp)const char*const tp;
{ int forceret;
  ;{ char*p;
     if(setxit&&(p=getenv(exitcode)))		 /* user specified exitcode? */
      { if((forceret=renvint(-2L,p))>=0)	     /* yes, is it positive? */
	   retval=forceret;				 /* then override it */
      }
     else if(*tp)		 /* no EXITCODE set, TRAP found, provide one */
      { strcpy(p=buf2,exitcode);*(p+=STRLEN(exitcode))='=';
	ultstr(0,(unsigned long)retval,p+1);sputenv(buf2);forceret= -1;
      }
   }
  if(*tp)
   { int poutfd[2];
     metaparse(tp);concon('\n');rpipe(poutfd);inittmout(buf);
     if(!(pidchild=sfork()))	     /* connect stdout to stderr before exec */
      { rclose(PWRO);getstdin(PRDO);rclose(STDOUT);rdup(STDERR);
	callnewprog(buf);			      /* trap your heart out */
      }
     rclose(PRDO);					     /* neat & clean */
     if(!forkerr(pidchild,buf))
      { int newret;
	dump(PWRO,themail,filled);	      /* try and shove down the mail */
	if((newret=waitfor(pidchild))!=EXIT_SUCCESS&&forceret==-2)
	   retval=newret;		       /* supersede the return value */
	pidchild=0;
      }
     else
	rclose(PWRO);
   }
}
