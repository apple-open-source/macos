/************************************************************************
 *	Custom standard-io library					*
 *									*
 *	Copyright (c) 1990-1999, S.R. van den Berg, The Netherlands	*
 *	#include "../README"						*
 ************************************************************************/
#ifdef RCS
static /*const*/char rcsid[]=
 "$Id: cstdio.c,v 1.1.1.1 1999/09/23 17:30:07 wsanchez Exp $";
#endif
#include "procmail.h"
#include "robust.h"
#include "cstdio.h"
#include "misc.h"

static uchar rcbuf[STDBUF],*rcbufp,*rcbufend;	  /* buffer for custom stdio */
static off_t blasttell;
static struct dyna_long inced;				  /* includerc stack */
struct dynstring*incnamed;

void pushrc(name)const char*const name;		      /* open include rcfile */
{ struct stat stbuf;					   /* only if size>0 */
  stbuf.st_mode=0;
  if(*name&&(stat(name,&stbuf)||!S_ISREG(stbuf.st_mode)||stbuf.st_size))
   { app_val(&inced,rcbufp?(off_t)(rcbufp-rcbuf):(off_t)0);	 /* save old */
     app_val(&inced,blasttell);app_val(&inced,(off_t)rc);   /* position & fd */
     if(S_ISDIR(stbuf.st_mode)||bopen(name)<0)	  /* try to open the new one */
	readerr(name),poprc();		       /* we couldn't, so restore rc */
   }
}

void duprcs P((void))		/* `duplicate' all the fds of opened rcfiles */
{ size_t i;struct dynstring*dp;
  dp=incnamed;rclose(rc);
  if(0>(rc=ropen(dp->ename,O_RDONLY,0)))     /* first reopen the current one */
     goto dupfailed;
  lseek(rc,blasttell+STDBUF,SEEK_SET);	 /* you'll never know the difference */
  for(i=inced.filled;dp=dp->enext,i;i-=2)
   { int fd;
     rclose(inced.offs[--i]);
     if(0>(fd=ropen(dp->ename,O_RDONLY,0)))    /* reopen all (nested) others */
dupfailed:					   /* oops, file disappeared */
	nlog("Lost"),logqnl(dp->ename),exit(EX_NOINPUT);	    /* panic */
     inced.offs[i]=fd; /* new & improved fd, decoupled from fd in the parent */
   }
}

static void closeonerc P((void))
{ struct dynstring*last;
  if(rc>=0)
     rclose(rc),rc= -1,last=incnamed,incnamed=last->enext,free(last);
}

int poprc P((void))
{ closeonerc();					     /* close it in any case */
  if(skiprc)
     skiprc=0,nlog("Missing closing brace\n");
  if(!inced.filled)				  /* include stack is empty? */
     return 0;	      /* restore rc, seekpos, prime rcbuf and restore rcbufp */
  rc=inced.offs[--inced.filled];lseek(rc,inced.offs[--inced.filled],SEEK_SET);
  rcbufp=rcbufend;getb();rcbufp=rcbuf+inced.offs[--inced.filled];
  return 1;
}

void closerc P((void))					/* {while(poprc());} */
{ while(closeonerc(),inced.filled)
     rc=inced.offs[inced.filled-1],inced.filled-=3;
}
							    /* destroys buf2 */
int bopen(name)const char*const name;				 /* my fopen */
{ rcbufp=rcbufend=0;rc=ropen(name,O_RDONLY,0);
  if(rc>=0)
   { char*md;		 /* if it's a relative name and an absolute $MAILDIR */
     if(!strchr(dirsep,*name)&&
	*(md=(char*)tgetenv(maildir))&&
	strchr(dirsep,*md))
      { strcpy(buf2,md);*(md=strchr(buf2,'\0'))= *dirsep;strcpy(++md,name);
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
   { blasttell=tell(rc);rcbufend=rcbuf+rread(rc,rcbufp=rcbuf,STDBUF);
     if(rcbufp==rcbufend)
	rcbufp++;
   }
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

int getlline(target)char*target;
{ char*chp2,*end;int overflow;
  for(end=target+linebuf,overflow=0;;*target++='\n')
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
