/************************************************************************
 *	Whatever is needed for (un)locking files in various ways	*
 *									*
 *	Copyright (c) 1990-1999, S.R. van den Berg, The Netherlands	*
 *	#include "../README"						*
 ************************************************************************/
#ifdef RCS
static /*const*/char rcsid[]=
 "$Id: locking.c,v 1.1.1.1 1999/09/23 17:30:07 wsanchez Exp $";
#endif
#include "procmail.h"
#include "robust.h"
#include "shell.h"
#include "misc.h"
#include "pipes.h"
#include "exopen.h"
#include "locking.h"
#include "lastdirsep.h"

void lockit(name,lockp)char*name;char**const lockp;
{ int permanent=nfsTRY,triedforce=0,locktype=doLOCK;struct stat stbuf;time_t t;
  zombiecollect();
  if(*lockp)
   { if(!strcmp(name,*lockp))	/* compare the previous lockfile to this one */
      { free(name);return;	 /* they're equal, save yourself some effort */
      }
     unlock(lockp);		       /* unlock any previous lockfile FIRST */
   }				  /* to prevent deadlocks (I hate deadlocks) */
  if(!*name)
   { free(name);return;
   }
  if(!strcmp(name,defdeflock))	       /* is it the system mailbox lockfile? */
   { locktype=doCHECK|doLOCK;
     if(sgid!=gid&&setegid(sgid))      /* try and get some extra permissions */
#ifndef fdlock
	if(!accspooldir)
	 { yell("Bypassed locking",name);
	   free(name);return;
	 }
#endif
	;
   }
  for(lcking|=lck_LOCKFILE;;)
   { yell("Locking",name);	    /* in order to cater for clock skew: get */
     if(!xcreat(name,LOCKperm,&t,locktype))    /* time t from the filesystem */
      { *lockp=name;				   /* lock acquired, hurray! */
	break;
      }
     switch(errno)
      { case EEXIST:		   /* check if it's time for a lock override */
	   if(!lstat(name,&stbuf)&&stbuf.st_size<=MAX_locksize&&locktimeout
	    &&!lstat(name,&stbuf)&&locktimeout<t-stbuf.st_mtime)
	     /*
	      * stat() till unlink() should be atomic, but can't guarantee that
	      */
	    { if(triedforce)			/* already tried, not trying */
		 goto faillock;					    /* again */
	      if(S_ISDIR(stbuf.st_mode)||unlink(name))
		 triedforce=1,nlog("Forced unlock denied on"),logqnl(name);
	      else
	       { nlog("Forcing lock on");logqnl(name);suspend();
		 goto ce;
	       }
	    }
	   else
	      triedforce=0;		 /* legitimate iteration, clear flag */
	   break;
	case ENOSPC:		   /* no space left, treat it as a transient */
#ifdef EDQUOT						      /* NFS failure */
	case EDQUOT:		      /* maybe it was a short term shortage? */
#endif
	case ENOENT:case ENOTDIR:case EIO:/*case EACCES:*/
	   if(--permanent)
	      goto ds;
	   goto faillock;
#ifdef ENAMETOOLONG
	case ENAMETOOLONG:     /* maybe filename too long, shorten and retry */
	 { int i;
	   if(0<(i=strlen(name)-1)&&!strchr(dirsep,name[i-1]))
	    { nlog("Truncating");logqnl(name);elog(" and retrying lock\n");
	      name[i]='\0';permanent=nfsTRY;
	      goto ce;
	    }
	 }
#endif
	default:
faillock:  nlog("Lock failure on");logqnl(name);
	   goto term;
      }
     permanent=nfsTRY;
ds:  ssleep((unsigned)locksleep);
ce:  if(nextexit)
term: { free(name);			     /* drop the preallocated buffer */
	break;
      }
   }
  if(rcstate==rc_NORMAL)			   /* we already set our ids */
     setegid(gid);		      /* we put back our regular permissions */
  lcking&=~lck_LOCKFILE;
  if(nextexit)
     elog(whilstwfor),elog("lockfile"),logqnl(name),Terminate();
}

void lcllock P((void))				    /* lock a local lockfile */
{ char*lckfile;			    /* locking /dev/null or | would be silly */
  if(tolock||strcmp(buf2,devnull)&&strcmp(buf2,"|"))
   { if(tolock)
	lckfile=tstrdup(tolock);
     else
      { int len=strlen(buf2);
	strcpy(strcpy(lckfile=malloc(len+strlen(lockext)+1),buf2)+len,lockext);
      }
     if(globlock&&!strcmp(lckfile,globlock))	 /* same as global lockfile? */
      { nlog("Deadlock attempted on");logqnl(lckfile);
	free(lckfile);
      }
     else
	lockit(lckfile,&loclock);
   }
}

void unlock(lockp)char**const lockp;
{ lcking|=lck_LOCKFILE;
  if(*lockp)
   { if(!strcmp(*lockp,defdeflock))    /* is it the system mailbox lockfile? */
	setegid(sgid);		       /* try and get some extra permissions */
     yell("Unlocking",*lockp);
     if(unlink(*lockp))
	nlog("Couldn't unlock"),logqnl(*lockp);
     if(rcstate==rc_NORMAL)			   /* we already set our ids */
	setegid(gid);		      /* we put back our regular permissions */
     if(!nextexit)			   /* if not inside a signal handler */
	free(*lockp);
     *lockp=0;
   }
  offguard();
}
					/* an NFS secure exclusive file open */
int xcreat(name,mode,tim,chownit)const char*const name;const mode_t mode;
 time_t*const tim;const int chownit;
{ char*p;int j= -2;size_t i;
  i=lastdirsep(name)-name;strncpy(p=malloc(i+UNIQnamelen),name,i);
  if(unique(p,p+i,mode,verbose,chownit)) /* try & rename the unique filename */
   { if(tim)
      { struct stat stbuf;	 /* return the filesystem time to the caller */
	stat(p,&stbuf);*tim=stbuf.st_mtime;
      }
     j=myrename(p,name);
   }
  free(p);
  return j;
}
	/* if you've ever wondered what conditional compilation was good for */
#ifndef fdlock						/* watch closely :-) */
#ifdef USEflock
#ifndef SYS_FILE_H_MISSING
#include <sys/file.h>
#endif
#define REITflock	1
#else
#define REITflock	0
#endif /* USEflock */
static int oldfdlock= -1;			    /* the fd we locked last */
#ifndef NOfcntl_lock
static struct flock flck;		/* why can't it be a local variable? */
#define REITfcntl	1
#else
#define REITfcntl	0
#endif /* NOfcntl_lock */
#ifdef USElockf
static off_t oldlockoffset;
#define REITlockf	1
#else
#define REITlockf	0
#endif /* USElockf */

int fdlock(fd)
{ int ret;
  if(verbose)
     nlog("Acquiring kernel-lock\n");
#if REITfcntl+REITflock+REITlockf>1
  for(;!toutflag;verbose&&(nlog("Reiterating kernel-lock\n"),0),
   ssleep((unsigned)locksleep))
#endif
   { zombiecollect();
#ifdef USElockf
     oldlockoffset=tell(fd);
#endif
#ifndef NOfcntl_lock
     flck.l_type=F_WRLCK;flck.l_whence=SEEK_SET;flck.l_len=0;
#ifdef USElockf
     flck.l_start=oldlockoffset;
#else
     flck.l_start=tell(fd);
#endif
#endif
     lcking|=lck_KERNEL;
#ifndef NOfcntl_lock
     ret=fcntl(fd,F_SETLKW,&flck);
#ifdef USElockf
     if((ret|=lockf(fd,F_TLOCK,(off_t)0))&&(errno==EAGAIN||errno==EACCES||
      errno==EWOULDBLOCK))
ufcntl:
      { flck.l_type=F_UNLCK;fcntl(fd,F_SETLK,&flck);
	continue;
      }
#ifdef USEflock
     if((ret|=flock(fd,LOCK_EX|LOCK_NB))&&(errno==EAGAIN||errno==EACCES||
      errno==EWOULDBLOCK))
      { lockf(fd,F_ULOCK,(off_t)0);
	goto ufcntl;
      }
#endif /* USEflock */
#else /* USElockf */
#ifdef USEflock
     if((ret|=flock(fd,LOCK_EX|LOCK_NB))&&(errno==EAGAIN||errno==EACCES||
      errno==EWOULDBLOCK))
      { flck.l_type=F_UNLCK;fcntl(fd,F_SETLK,&flck);
	continue;
      }
#endif /* USEflock */
#endif /* USElockf */
#else /* NOfcntl_lock */
#ifdef USElockf
     ret=lockf(fd,F_LOCK,(off_t)0);
#ifdef USEflock
     if((ret|=flock(fd,LOCK_EX|LOCK_NB))&&(errno==EAGAIN||errno==EACCES||
      errno==EWOULDBLOCK))
      { lockf(fd,F_ULOCK,(off_t)0);
	continue;
      }
#endif /* USEflock */
#else /* USElockf */
#ifdef USEflock
     ret=flock(fd,LOCK_EX);
#endif /* USEflock */
#endif /* USElockf */
#endif /* NOfcntl_lock */
     oldfdlock=fd;lcking&=~lck_KERNEL;
     return ret;
   }
  return 1;							/* timed out */
}

int fdunlock P((void))
{ int i;
  if(oldfdlock<0)
     return -1;
  i=0;
#ifdef USEflock
  i|=flock(oldfdlock,LOCK_UN);
#endif
#ifdef USElockf
  ;{ off_t curp=tell(oldfdlock);	       /* restore the position later */
     lseek(oldfdlock,oldlockoffset,SEEK_SET);
     i|=lockf(oldfdlock,F_ULOCK,(off_t)0);lseek(oldfdlock,curp,SEEK_SET);
   }
#endif
#ifndef NOfcntl_lock
  flck.l_type=F_UNLCK;i|=fcntl(oldfdlock,F_SETLK,&flck);
#endif
  oldfdlock= -1;
  return i;
}
#endif /* fdlock */
