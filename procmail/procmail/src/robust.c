/************************************************************************
 *	The fault-tolerant system-interface				*
 *									*
 *	Copyright (c) 1990-1997, S.R. van den Berg, The Netherlands	*
 *	Copyright (c) 1999-2001, Philip Guenther, The United States	*
 *						of America		*
 *	#include "../README"						*
 ************************************************************************/
#ifdef RCS
static /*const*/char rcsid[]=
 "$Id: robust.c,v 1.33 2001/06/23 08:18:50 guenther Exp $";
#endif
#include "procmail.h"
#include "robust.h"
#include "misc.h"
#include "pipes.h"
#include "common.h"
#include "mailfold.h"

mode_t cumask;

#define nomemretry	noresretry
#define noforkretry	noresretry
		       /* set nextexit to prevent elog() from using malloc() */
void nomemerr(len)const size_t len;
{ static const char outofmem[]="Out of memory";
  nextexit=2;nlog(outofmem);elog("\n");
  syslog(LOG_NOTICE,"%s as I tried to allocate %ld bytes\n",outofmem,
   (long)len);
  if(!privileged&&buf&&buf2)
   { buf[linebuf-1]=buf2[linebuf-1]='\0';elog("buffer 0:");logqnl(buf);
     elog("buffer 1:");logqnl(buf2);
   }
  if(retval!=EX_TEMPFAIL)
     retval=EX_OSERR;
  Terminate();
}

static void heapdefrag P((void))
{ register void*p;
  lcking|=lck_MEMORY;
  if(p=malloc(1))
     free(p);			   /* works on some systems with latent free */
}

void*tmalloc(len)const size_t len;    /* this malloc can survive a temporary */
{ void*p;int i;				    /* "out of swap space" condition */
  lcking|=lck_ALLOCLIB;
  if(p=malloc(len))
     goto ret;
  heapdefrag();heapdefrag();				   /* try some magic */
  for(i=nomemretry;i<0||i--;)
   { suspend();		     /* problems?  don't panic, wait a few secs till */
     if(p=malloc(len))	     /* some other process has paniced (and died 8-) */
ret:  { lcking&=~(lck_MEMORY|lck_ALLOCLIB);
	return p;
      }
   }
  nomemerr(len);
}

void*trealloc(old,len)void*const old;const size_t len;
{ void*p;int i;
  lcking|=lck_ALLOCLIB;
  if(p=realloc(old,len))
     goto ret;				    /* for comment see tmalloc above */
  heapdefrag();heapdefrag();
  for(i=nomemretry;i<0||i--;)
   { suspend();
     if(p=realloc(old,len))
ret:  { lcking&=~(lck_MEMORY|lck_ALLOCLIB);
	return p;
      }
   }
  nomemerr(len);
}

void*fmalloc(len)const size_t len;			 /* 'fragile' malloc */
{ void*p;
  lcking|=lck_ALLOCLIB;p=malloc(len);lcking&=~lck_ALLOCLIB;
  return p;
}

void*frealloc(old,len)void*const old;const size_t len;	/* 'fragile' realloc */
{ void*p;
  lcking|=lck_ALLOCLIB;p=realloc(old,len);lcking&=~lck_ALLOCLIB;
  return p;
}

void tfree(p)void*const p;
{ lcking|=lck_ALLOCLIB;free(p);lcking&=~lck_ALLOCLIB;
}

#include "shell.h"

pid_t sfork P((void))			/* this fork can survive a temporary */
{ pid_t i;int r;			   /* "process table full" condition */
  zombiecollect();elog(empty);r=noforkretry;	  /* flush log, just in case */
  while((i=fork())==-1)
   { lcking|=lck_FORK;
     if(!(r<0||r--))
	break;
     if(waitfor((pid_t)0)==NO_PROCESS)
	suspend();
   }
  lcking&=~lck_FORK;
  return i;
}

void opnlog(file)const char*file;
{ int i;
  elog(empty);						     /* flush stderr */
  if(!*file)						   /* empty LOGFILE? */
     file=devnull;				 /* substitute the bitbucket */
  if(0>(i=opena(file)))			     /* error?	keep the old LOGFILE */
     writeerr(file),syslog(LOG_NOTICE,slogstr,errwwriting,file);
  else
     rclose(STDERR),rdup(i),rclose(i),logopened=1;
}

int opena(a)const char*const a;
{ yell("Opening",a);
#ifdef O_CREAT
  return ropen(a,O_WRONLY|O_APPEND|O_CREAT,NORMperm);
#else
  ;{ int fd;
     return (fd=ropen(a,O_WRONLY,0))<0?creat(a,NORMperm):fd;
   }
#endif
}

int ropen(name,mode,mask)const char*const name;const int mode;
 const mode_t mask;
{ int i,r;					       /* a SysV secure open */
  for(r=noresretry,lcking|=lck_FILDES;0>(i=open(name,mode,mask));)
     if(errno!=EINTR&&!(errno==ENFILE&&(r<0||r--)))
	break;		 /* survives a temporary "file table full" condition */
  lcking&=~lck_FILDES;
  return i;
}

int rpipe(fd)int fd[2];
{ int i,r;					  /* catch "file table full" */
  for(r=noresretry,lcking|=lck_FILDES;0>(i=pipe(fd));)
     if(!(errno==ENFILE&&(r<0||r--)))
      { *fd=fd[1]= -1;
	break;
      }
  lcking&=~lck_FILDES;
  return i;
}

int rdup(p)const int p;
{ int i,r;					  /* catch "file table full" */
  for(r=noresretry,lcking|=lck_FILDES;0>(i=dup(p));)
     if(!(errno==ENFILE&&(r<0||r--)))
	break;
  lcking&=~lck_FILDES;
  return i;
}

int rclose(fd)const int fd;	      /* a SysV secure close (signal immune) */
{ int i;
  while((i=close(fd))&&errno==EINTR);
  return i;
}

int rread(fd,a,len)const int fd,len;void*const a;      /* a SysV secure read */
{ int i;
  while(0>(i=read(fd,a,(size_t)len))&&errno==EINTR);
  return i;
}
						      /* a SysV secure write */
int rwrite(fd,a,len)const int fd,len;const void*const a;
{ int i;
  while(0>(i=write(fd,a,(size_t)len))&&errno==EINTR);
  return i;
}

void ssleep(seconds)const unsigned seconds;
{ long t;
  sleep(seconds);
  if(alrmtime)
     if((t=alrmtime-time((time_t*)0))<=1)	  /* if less than 1s timeout */
	ftimeout();				  /* activate it by hand now */
     else		    /* set it manually again, to avoid problems with */
	alarm((unsigned)t);	/* badly implemented sleep library functions */
}

void doumask(mask)const mode_t mask;
{ umask(cumask=mask);
}
