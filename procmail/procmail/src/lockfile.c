/************************************************************************
 *	lockfile - The conditional semaphore-file creator		*
 *									*
 *	It has been designed to be able to be run sgid mail or		*
 *	any gid you see fit (in case your mail spool area is *not*	*
 *	world writable, but group writable), without creating		*
 *	security holes.							*
 *									*
 *	Seems to be relatively bug free.				*
 *									*
 *	Copyright (c) 1990-1999, S.R. van den Berg, The Netherlands	*
 *	Copyright (c) 1999-2001, Philip Guenther, The United States	*
 *							of America	*
 *	#include "../README"						*
 ************************************************************************/
#ifdef RCS
static /*const*/char rcsid[]=
 "$Id: lockfile.c,v 1.49 2001/08/04 07:12:16 guenther Exp $";
#endif
static /*const*/char rcsdate[]="$Date: 2001/08/04 07:12:16 $";
#include "includes.h"
#include "sublib.h"
#include "exopen.h"
#include "mcommon.h"
#include "authenticate.h"
#include "lastdirsep.h"
#include "../patchlevel.h"

static volatile int exitflag;
pid_t thepid;
uid_t uid;
gid_t sgid;
const char dirsep[]=DIRSEP;
static const char lockext[]=DEFlockext,
 nameprefix[]="lockfile: ",lgname[]="LOGNAME";

static void failure P((void))				      /* signal trap */
{ exitflag=2;					       /* merely sets a flag */
}
				    /* see locking.c for comment on xcreat() */
static int xcreat(name,tim)const char*const name;time_t*const tim;
{ char*p;int j= -1;size_t i;struct stat stbuf;
  i=lastdirsep(name)-name;
  if(!(p=malloc(i+UNIQnamelen)))
     return exitflag=1;
  memcpy(p,name,i);
  if(unique(p,p+i,i+UNIQnamelen,LOCKperm,0,doCHECK|doLOCK))
     stat(p,&stbuf),*tim=stbuf.st_mtime,j=myrename(p,name);
  free(p);
  return j;
}

void elog(a)const char*const a;
{ write(STDERR,a,strlen(a));
}

void nlog(a)const char*const a;
{ elog(nameprefix);elog(a);  /* decent error messages should start with this */
}

static PROGID;

int main(argc,argv)int argc;const char*const argv[];
{ const char*const*p;char*cp;uid_t uid;
  int sleepsec,retries,invert,force,suspend,retval=EXIT_SUCCESS,virgin=1;
  static const char usage[]="Usage: lockfile -v | -nnn | -r nnn | -l nnn \
| -s nnn | -! | -ml | -mu | file ...\n";
  if(argc<=1)			       /* sanity check, any argument at all? */
     goto usg;
  sleepsec=DEFlocksleep;retries= -1;suspend=DEFsuspend;thepid=getpid();force=0;
  uid=getuid();signal(SIGPIPE,SIG_IGN);
  if(setuid(uid)||geteuid()!=uid)		  /* resist setuid operation */
sp:{ nlog("Unable to give up special permissions");
     return EX_OSERR;
   }
again:
  invert=(char*)progid-(char*)progid;qsignal(SIGHUP,failure);
  qsignal(SIGINT,failure);qsignal(SIGQUIT,failure);qsignal(SIGTERM,failure);
  for(p=argv;--argc>0;)
     if(*(cp=(char*)*++p)=='-')
	for(cp++;;)
	 { char*cp2=cp;int i;
	   switch(*cp++)
	    { case '!':invert^=1;		      /* invert the exitcode */
		 continue;
	      case 'r':case 'l':case 's':
		 if(!*cp&&(cp=(char*)*++p,!--argc)) /* concatenated/seperate */
		  { p--;
		    goto eusg;
		  }
		 i=strtol(cp,&cp,10);
		 switch(*cp2)
		  { case 'r':retries=i;
		       goto checkrdec;
		    case 'l':force=i;
		       goto checkrdec;
		    default:
		       if(i<0)			    /* suspend should be >=0 */
			  goto eusg;
		       suspend=i;
		       goto checkrdec;
		  }
	      case VERSIONOPT:elog("lockfile");elog(VERSION);
		    elog("\nYour system mailbox's lockfile:\t");
		    elog(auth_mailboxname(auth_finduid(getuid(),0)));
		    elog(lockext);elog("\n");
		  goto xusg;
	      case HELPOPT1:case HELPOPT2:elog(usage);
		 elog(
 "\t-v\tdisplay the version number and exit\
\n\t-nnn\twait nnn seconds between locking attempts\
\n\t-r nnn\tmake at most nnn retries before giving up on a lock\
\n\t-l nnn\tset locktimeout to nnn seconds\
\n\t-s nnn\tsuspend nnn seconds after a locktimeout occurred\
\n\t-!\tinvert the exitcode of lockfile\
\n\t-ml\tlock your system mail-spool file\
\n\t-mu\tunlock your system mail-spool file\n");
		 goto xusg;
	      default:
		 if(sleepsec>=0)	    /* is this still the first pass? */
		  { if((sleepsec=strtol(cp2,&cp,10))<0)
		       goto eusg;
checkrdec:	    if(cp2==cp)
eusg:		     { elog(usage);		    /* regular usage message */
xusg:		       retval=EX_USAGE;
		       goto nfailure;
		     }
		  }
		 else		      /* no second pass, so leave sleepsec<0 */
		    strtol(cp2,&cp,10);		   /* and discard the number */
		 continue;
	      case 'm':		  /* take $LOGNAME as a hint, check if valid */
	       { auth_identity*pass;static char*ma;
		 if(*cp&&cp[1]||ma&&sleepsec>=0)	     /* second pass? */
		    goto eusg;
		 if(!ma)			/* ma initialised last time? */
		  { if(!((ma=(char*)getenv(lgname))&&
		      (pass=auth_finduser(ma,0))&&
		      auth_whatuid(pass)==uid||
		     (pass=auth_finduid(uid,0))))
		     { nlog("Can't determine your mailbox, who are you?\n");
		       goto nfailure;	 /* panic, you're not in /etc/passwd */
		     }
		    ;{ const char*p;
		       if(!*(p=auth_mailboxname(pass))||
			!(ma=malloc(strlen(p)+STRLEN(lockext)+1)))
			  goto outofmem;
		       strcat(strcpy(ma,p),lockext);
		     }
		  }
		 switch(*cp)
		  { default:
		       goto eusg;		    /* somebody goofed again */
		    case 'l':				 /* lock the mailbox */
		       if(sleepsec>=0)			      /* first pass? */
			{ cp=ma;
			  goto stilv;			    /* yes, lock it! */
			}
		    case 'u':			       /* unlock the mailbox */
		       if(unlink(ma))
			{ nlog("Can't unlock \"");elog(ma);elog("\"");
			  if(*cp=='l')	 /* they messed up, give them a hint */
			     elog(" again,\n already dropped my privileges");
			  elog("\n");
			}
		       else
			  virgin=0;
		  }

	       }
	      case '\0':;
	    }
	   break;
	 }
     else if(sleepsec<0)      /* second pass, release everything we acquired */
	unlink(cp);
     else
      { time_t t;int permanent;gid_t gid=getgid();
	if(setgid(gid)||getegid()!=gid)	      /* just to be on the safe side */
	   goto sp;
stilv:	virgin=0;permanent=nfsTRY;
	while(0>xcreat(cp,&t))				     /* try and lock */
	 { struct stat stbuf;
	   if(exitflag)					    /* time to stop? */
	    { if(exitflag==1)		     /* was it failure() or malloc() */
outofmem:	 retval=EX_OSERR,nlog("Out of memory");
	      else
		 retval=EX_TEMPFAIL,nlog("Signal received");
	      goto lfailure;
	    }
	   switch(errno)		    /* why did the lock not succeed? */
	    { case EEXIST:			  /* hmmm..., by force then? */
		 if(force&&!lstat(cp,&stbuf)&&force<t-stbuf.st_mtime)
		  { nlog(unlink(cp)?"Forced unlock denied on \"":
		     "Forcing lock on \"");
		    elog(cp);elog("\"\n");sleep(suspend);	/* important */
		  }
		 else					   /* no forcing now */
		    switch(retries)    /* await your turn like everyone else */
		     { case 0:nlog("Sorry");retval=EX_CANTCREAT;
			  goto lfailure;      /* patience exhausted, give up */
		       default:retries--;		      /* count sheep */
		       case -1:sleep(sleepsec);		     /* wait and see */
		     }
		 break;
	      case ENOSPC:
#ifdef EDQUOT
	      case EDQUOT:
#endif
	      case ENOENT:case ENOTDIR:case EIO:/*case EACCES:*/
		 if(!--permanent)	 /* NFS sporadically generates these */
		  { sleep(sleepsec);	/* unwarranted, so ignore them first */
		    continue;
		  }
	      default:		     /* but, it seems to persist, so give up */
		 nlog("Try praying");retval=EX_UNAVAILABLE;
#ifdef ENAMETOOLONG
		 goto lfailure;
	      case ENAMETOOLONG:
		 if(0<(permanent=strlen(cp)-1)&&      /* can we truncate it? */
		  !strchr(dirsep,cp[permanent-1]))
		  { nlog("Truncating \"");elog(cp);	      /* then try it */
		    elog("\" and retrying lock\n");cp[permanent]='\0';
		    break;
		  }				     /* otherwise, forget it */
		 nlog("Filename too long");retval=EX_UNAVAILABLE;
#endif
lfailure:	 elog(", giving up on \"");elog(cp);elog("\"\n");
nfailure:	 sleepsec= -1;argc=p-argv;		    /* mark sleepsec */
		 goto again;
	    }  /* for second pass, and adjust argc to the no. of args parsed */
	   permanent=nfsTRY;	       /* refresh the NFS-error-ignore count */
	 }
      }
  if(retval==EXIT_SUCCESS&&virgin)	 /* any errors?	 did we do anything? */
usg:
   { elog(usage);
     return EX_USAGE;
   }
  if(invert)
     switch(retval)			 /* we only invert the regular cases */
      { case EXIT_SUCCESS:
	   return EX_CANTCREAT;
	case EX_CANTCREAT:
	   return EXIT_SUCCESS;
      }
  return retval;			       /* all other exitcodes remain */
}

void*tmalloc(len)const size_t len;				     /* stub */
{ void*p;
  if(!(p=malloc(len)))
     exitflag=1;				     /* signal out of memory */
  return p;
}

void tfree(p)void*const p;					     /* stub */
{ free(p);
}

int ropen(name,mode,mask)const char*const name;const int mode;
 const mode_t mask;
{ return open(name,mode,mask);					     /* stub */
}

int rwrite(fd,a,len)const int fd;const void*const a;const int len;   /* stub */
{ return write(fd,a,len);
}

int rclose(fd)const int fd;					     /* stub */
{ return close(fd);
}

void writeerr(a)const char*const a;				     /* stub */
{
}

char*cstr(a,b)char*const a;const char*const b;			     /* stub */
{ return 0;
}

void ssleep(seconds)const unsigned seconds;			     /* stub */
{ sleep(seconds);
}
