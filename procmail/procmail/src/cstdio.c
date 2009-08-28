/************************************************************************
 *	Custom standard-io library					*
 *									*
 *	Copyright (c) 1990-1999, S.R. van den Berg, The Netherlands	*
 *	#include "../README"						*
 ************************************************************************/
#ifdef RCS
static /*const*/char rcsid[]=
 "$Id: cstdio.c,v 1.52 2000/11/22 01:29:56 guenther Exp $";
#endif
#include "procmail.h"
#include "robust.h"
#include "misc.h"
#include "lmtp.h"
#include "variables.h"
#include "shell.h"
#include "cstdio.h"

static uchar rcbuf[STDBUF],*rcbufp,*rcbufend;	  /* buffer for custom stdio */
static off_t blasttell;
static struct dyna_array inced;				  /* includerc stack */
struct dynstring*incnamed;

static void refill(offset)const int offset;		/* refill the buffer */
{ int ret=rread(rc,rcbuf,STDBUF);
  if(ret>0)
   { rcbufend=rcbuf+ret;
     rcbufp=rcbuf+offset;				 /* restore position */
   }
  else
   { rcbufend=rcbuf;
     rcbufp=rcbuf+1;					   /* looks like EOF */
   }
}

void pushrc(name)const char*const name;		      /* open include rcfile */
{ if(*name&&strcmp(name,devnull))
   { struct stat stbuf;
     if(stat(name,&stbuf)||!S_ISREG(stbuf.st_mode))
	goto rerr;
     if(stbuf.st_size)					   /* only if size>0 */
      { app_vali(inced,rcbufp?rcbufp-rcbuf:0);			 /* save old */
	app_valo(inced,blasttell);app_vali(inced,ifdepth);/* position, brace */
	app_vali(inced,rc);				       /* depth & fd */
	ifdepth=ifstack.filled;				  /* new stack depth */
	if(bopen(name)<0)			  /* try to open the new one */
	 { poprc();			       /* we couldn't, so restore rc */
rerr:	   readerr(name);
	 }
      }
   }
}

void changerc(name)const char*const name;		    /* change rcfile */
{ if(!*name||!strcmp(name,devnull))
pr:{ ifstack.filled=ifdepth;	   /* lose all the braces to avoid a warning */
     rclose(rc);rcbufp=rcbufend+1;		    /* make it look like EOF */
     return;
   }
  if(!strcmp(name,incnamed->ename))		    /* just restart this one */
     lseek(rc,0,SEEK_SET),refill(0);
  else
   { struct stat stbuf;int orc;uchar*orbp,*orbe;struct dynstring*dp;
     if(stat(name,&stbuf)||!S_ISREG(stbuf.st_mode))
rerr: { readerr(name);				      /* skip irregularities */
	return;
      }
     if(!stbuf.st_size)			    /* avoid opening trivial rcfiles */
	goto pr;
     if(orbp=rcbufp,orbe=rcbufend,orc=rc,bopen(name)<0)
      { rcbufp=orbp;rcbufend=orbe;rc=orc;		    /* restore state */
	goto rerr;
      }
     rclose(orc);				/* success! drop the old and */
     if(dp=incnamed->enext)			      /* fixup the name list */
	incnamed->enext=dp->enext,free(dp);
   }
  ifstack.filled=ifdepth;			     /* close all the braces */
}

void duprcs P((void))		/* `duplicate' all the fds of opened rcfiles */
{ size_t i;struct dynstring*dp;
  dp=incnamed;rclose(rc);
  if(0>(rc=ropen(dp->ename,O_RDONLY,0)))     /* first reopen the current one */
     goto dupfailed;
  lseek(rc,blasttell+STDBUF,SEEK_SET);	 /* you'll never know the difference */
  for(i=inced.filled;dp=dp->enext,i;i-=3)
   { int fd;
     rclose(acc_vali(inced,--i));
     if(0>(fd=ropen(dp->ename,O_RDONLY,0)))    /* reopen all (nested) others */
dupfailed:					   /* oops, file disappeared */
	nlog("Lost"),logqnl(dp->ename),exit(EX_NOINPUT);	    /* panic */
     acc_vali(inced,i)=fd;		/* new & improved fd, decoupled from */
   }							 /* fd in the parent */
}

static void closeonerc P((void))
{ struct dynstring*last;
  if(rc>=0)
     rclose(rc),rc= -1,last=incnamed,incnamed=last->enext,free(last);
}

int poprc P((void))
{ closeonerc();					     /* close it in any case */
  if(ifstack.filled>ifdepth)		     /* force the matching of braces */
     ifstack.filled=ifdepth,nlog("Missing closing brace\n");
  if(!inced.filled)				  /* include stack is empty? */
     return 0;	      /* restore rc, seekpos, prime rcbuf and restore rcbufp */
  rc=acc_vali(inced,--inced.filled);
  ifdepth=acc_vali(inced,--inced.filled);
  blasttell=lseek(rc,acc_valo(inced,--inced.filled),SEEK_SET);
  refill(acc_vali(inced,--inced.filled));
  return 1;
}

void closerc P((void))					/* {while(poprc());} */
{ while(closeonerc(),inced.filled)
     rc=acc_vali(inced,inced.filled-1),inced.filled-=4;
  ifstack.filled=ifdepth=0;
}
							    /* destroys buf2 */
int bopen(name)const char*const name;				 /* my fopen */
{ rcbufp=rcbufend=0;rc=ropen(name,O_RDONLY,0);
  if(rc>=0)
   { char*md;size_t len; /* if it's a relative name and an absolute $MAILDIR */
     if(!strchr(dirsep,*name)&&
	*(md=(char*)tgetenv(maildir))&&
	strchr(dirsep,*md)&&
	(len=strlen(md))+strlen(name)+2<linebuf)
      { strcpy(buf2,md);*(md=buf2+len)= *dirsep;strcpy(++md,name);
	md=buf2;				    /* then prepend $MAILDIR */
      }
     else
	md=(char*)name;			      /* pick the original otherwise */
     newdynstring(&incnamed,md);
   }
  return rc;
}

int getbl(p,end)char*p,*end;					  /* my gets */
{ int i,overflow=0;char*q;
  for(q=p,end--;;)
   { switch(i=getb())
      { case '\n':case EOF:*q='\0';
	   return overflow?-1:p!=q;	     /* did we read anything at all? */
      }
     if(q==end)	    /* check here so that a trailing backslash won't be lost */
	q=p,overflow=1;
     *q++=i;
   }
}

int getb P((void))						 /* my fgetc */
{ if(rcbufp==rcbufend)						   /* refill */
     blasttell=tell(rc),refill(0);
  return rcbufp<rcbufend?(int)*rcbufp++:EOF;
}

void ungetb(x)const int x;	/* only for pushing back original characters */
{ if(x!=EOF)
     rcbufp--;							   /* backup */
}

int testB(x)const int x;	   /* fgetc that only succeeds if it matches */
{ int i;
  if((i=getb())==x)
     return 1;
  ungetb(i);
  return 0;
}

int sgetc P((void))				/* a fake fgetc for a string */
{ return *sgetcp?(int)*(uchar*)sgetcp++:EOF;
}

int skipspace P((void))
{ int any=0;
  while(testB(' ')||testB('\t'))
     any=1;
  return any;
}

void skipline P((void))
{ for(;;)					/* skip the rest of the line */
     switch(getb())
      { default:
	   continue;
	case '\n':case EOF:
	   return;
      }
}

int getlline(target,end)char*target,*end;
{ char*chp2;int overflow;
  for(overflow=0;;*target++='\n')
     switch(getbl(chp2=target,end))			   /* read line-wise */
      { case -1:overflow=1;
	case 1:
	   if(*(target=strchr(target,'\0')-1)=='\\')
	    { if(chp2!=target)				  /* non-empty line? */
		 target++;		      /* then preserve the backslash */
	      if(target>end-2)			  /* space enough for getbl? */
		 target=end-linebuf,overflow=1;		/* toss what we have */
	      continue;
	    }
	case 0:
	   if(overflow)
	    { nlog(exceededlb);setoverflow();
	    }
	   return overflow;
      }
}

#ifdef LMTP
static int origfd= -1;

/* flush the input buffer and switch to a new input fd */
void pushfd(fd)int fd;
{ origfd=rc;rc=fd;
  rcbufend=rcbufp;
}

/* restore the original input fd */
static int popfd P((void))
{ rclose(rc);rc=origfd;
  if(0>origfd)
     return 0;
  origfd= -1;
  return 1;
}

/*
 * Are we at the end of an input read?	If so, we'll need to flush our
 * output buffer to prevent a possible deadlock from the pipelining
 */
int endoread P((void))
{ return rcbufp>=rcbufend;
}

/*
 * refill the LMTP input buffer, switching back to the original input
 * stream if needed
 */
void refillL P((void))
{ int retcode;
  refill(0);
  if(rcbufp>=rcbufend)				     /* we must have run out */
   { if(popfd())				      /* try the original fd */
      { refill(0);					  /* fill the buffer */
	if(rcbufp<rcbufend)		   /* looks good, clean up the child */
	 { if((retcode=waitfor(childserverpid))==EXIT_SUCCESS)
	      return;	     /* successfully switched and the child was fine */
	   syslog(LOG_WARNING,"LMTP child failed: exit code %d",retcode);
	   exit(EX_SOFTWARE);	       /* give up, give up, wherever you are */
	 }
      }
     exit(EX_NOINPUT);				     /* we ran out of input! */
   }
}

/* Like getb(), except for the LMTP input stream */
int getL P((void))
{ if(rcbufp==rcbufend)
     refillL();
  return (int)*rcbufp++;
}

/* read a bunch of characters from the LMTP input stream */
int readL(p,len)char*p;const int len;
{ size_t min;
  if(rcbufp==rcbufend)
     refillL();
  min=rcbufend-rcbufp;
  if(min>len)
     min=len;
  tmemmove(p,rcbufp,min);
  rcbufp+=min;
  return min;
}

/*
 * read exactly len bytes from the LMTP input stream
 * return 1 on success, 0 on EOF, and -1 on read error
 */
int readLe(p,len)char*p;int len;
{ long got=rcbufend-rcbufp;
  if(got>0)				      /* first, copy from the buffer */
   { if(got>len)			       /* is that more than we need? */
	got=len;
     tmemmove(p,rcbufp,got);
     rcbufp+=got;
     p+=got;len-=got;
   }
  while(len)					   /* read the rest directly */
   { if(0>(got=rread(rc,p,len)))
	return -1;
     if(!got&&!popfd())
	return 0;
     p+=got;len-=got;
   }
  return 1;
}

#endif
