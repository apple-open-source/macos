/************************************************************************
 *	From_ line routines used by procmail				*
 *									*
 *	Copyright (c) 1990-1999, S.R. van den Berg, The Netherlands	*
 *	Copyright (c) 2000, Philip Guenther, The United States		*
 *							of America	*
 *	#include "../README"						*
 ************************************************************************/
#ifdef RCS
static /*const*/char rcsid[]=
 "$Id: from.c,v 1.2 2000/10/27 20:03:58 guenther Exp $";
#endif
#include "procmail.h"
#include "robust.h"
#include "shell.h"
#include "memblk.h"
#include "common.h"
#include "misc.h"				/* for nlog */
#include "from.h"

static int privs;			     /* can we change the From_ line */
static const char From_[]=FROM,Fakefield[]=FAKE_FIELD,
 attemptst[]="Attempt to fake stamp by";

int eqFrom_(a)const char*const a;
{ return !strncmp(a,From_,STRLEN(From_));
}

const char*skipFrom_(startchar,tobesentp)const char*startchar;long*tobesentp;
{ if(eqFrom_(startchar))
   { long tobesent;char c;
     tobesent= *tobesentp;
     do
	while(c= *startchar++,--tobesent&&c!='\n');
     while(*startchar=='>');
     *tobesentp=tobesent;
   }
  return startchar;
}
			  /* tries to locate the timestamp on the From_ line */
static char*findtstamp(start,end)const char*start,*end;
{ end-=25;
  if(*start==' '&&(++start==end||*start==' '&&++start==end))
     return (char*)start-1;
  start=skpspace(start);start+=strcspn(start," \t\n");	/* jump over address */
  if(skpspace(start)>=end)			       /* enough space left? */
     return (char*)start;	 /* no, too small for a timestamp, stop here */
  while(!(end[13]==':'&&end[16]==':')&&--end>start);	  /* search for :..: */
  ;{ int spc=0;						 /* found it perhaps */
     while(end-->start)		      /* now skip over the space to the left */
      { switch(*end)
	 { case ' ':case '\t':spc=1;
	      continue;
	 }
	if(!spc)
	   continue;
	break;
      }
     return (char*)end+1;	   /* this should be right after the address */
   }
}

size_t ctime2buf2 P((void))
{ time_t t=time((time_t*)0);				 /* the current time */
  buf2[0]=buf2[1]=' ';strcpy(buf2+2,ctime(&t));
  return strlen(buf2);
}

void makeFrom(from,invoker)const char*from,*const invoker;
{ static const char mdaemon[]=MAILERDAEMON;
  const char*fwhom;char*rstart;size_t lfr,linv;int tstamp,extra,r;
  if(Deliverymode!=2)
   { tstamp=from&&*from==REFRESH_TIME&&!from[1];
     fwhom=from;
   }
  else
   { tstamp=0;
     fwhom= *from?from:mdaemon;
   }
  if(from&&!tstamp)
   { if(privs!=1&&!strcmp(from,invoker))
	privs=1;	  /* if -f user is the same as the invoker, allow it */
     else if(privs==-1&&from)
      { if(verbose)
	   nlog(insufprivs);			      /* ignore the bogus -f */
	syslog(LOG_ERR,slogstr,attemptst,invoker);from=0;
	fwhom=invoker;
      }
   }
  else
     fwhom=invoker;
  makeblock(&themail,2*linebuf+(lfr=strlen(fwhom))+(linv=strlen(invoker)));
  private(1);					    /* we're not yet sharing */
  rstart=thebody=themail.p;
  if(!Deliverymode&&!from)	       /* need to peek for a leading From_ ? */
      return;							     /* nope */
  r=ctime2buf2();
  lfr+=STRLEN(From_)+r;			   /* length of requested From_ line */
  if(tstamp)
     tstamp=r;					   /* save time stamp length */
  if(privs>0)						 /* privileged user? */
     linv=0;				 /* yes, so no need to insert >From_ */
  else
     linv+=STRLEN(Fakefield)+r;			    /* length of >From_ line */
  extra=0;
  if(Deliverymode!=2)					      /* if not LMTP */
   { while(1==(r=rread(STDIN,themail.p,1)))	  /* then read in first line */
	if(themail.p[0]!='\n')			    /* skip leading newlines */
	   break;
     if(r>0&&STRLEN(From_)<=(extra=1+rread(	      /* is it a From_ line? */
      STDIN,rstart+1,(int)(linebuf-2-1)))&&eqFrom_(themail.p))
      { rstart[extra]='\0';
	if(!(rstart=strchr(rstart,'\n')))
	 { do					     /* drop long From_ line */
	    { if((extra=rread(STDIN,themail.p,(int)(linebuf-2)))<=0)
		 break;
	      themail.p[extra]='\0';		  /* terminate it for strchr */
	    }
	   while(!(rstart=strchr(themail.p,'\n')));
	   extra=rstart?extra-(++rstart-themail.p):0;
	 }
	else
	 { size_t tfrl= ++rstart-themail.p; /* length of existing From_ line */
	   extra-=tfrl;					     /* demarcate it */
	   if(Deliverymode&&privs<0)
	    { if(verbose)			  /* discard the bogus From_ */
		 nlog(insufprivs);
	      syslog(LOG_ERR,slogstr,attemptst,fwhom);
	    }
	   else
	    { if(tstamp)
		 lfr=findtstamp(themail.p+STRLEN(From_),rstart)
		  -themail.p+tstamp;
	      else if(!from)		       /* leave the From_ line alone */
		 if(linv)				      /* fake alert? */
		    lfr=tfrl;	     /* yes, so separate From_ from the rest */
		 else
		    lfr=0,extra+=tfrl;		/* no, tack it onto the rest */
	      goto got_from;
	    }
	 }
      }
   }
  tstamp=0;			   /* no existing From_, so nothing to stamp */
  if(!from)							  /* no -f ? */
     linv=0;					  /* then it can't be a fake */
got_from:
  filled=lfr+linv+extra;			    /* From_ + >From_ + rest */
  if(lfr||linv)			     /* move read text beyond our From_ line */
   { r= *rstart;tmemmove(themail.p+lfr+linv,rstart,extra);
     rstart=themail.p+lfr;		      /* skip the From_ line, if any */
     if(!linv)						    /* no fake alert */
      { rstart[-tstamp]='\0';			       /* where do we append */
	if(!tstamp)			 /* no timestamp, so generate it all */
	   strcat(strcpy(themail.p,From_),fwhom);		/* From user */
      }
     else
      { if(lfr)					/* did we skip a From_ line? */
	   if(tstamp)			 /* copy the timestamp over the tail */
	      strcpy(rstart-tstamp,buf2);
	   else if(from)				 /* whole new From_? */
	      strcat(strcat(strcpy(themail.p,From_),fwhom),buf2);
	strcat(strcpy(rstart,Fakefield),invoker);	       /* fake alert */
      }					  /* overwrite the trailing \0 again */
     strcat(themail.p,buf2);themail.p[lfr+linv]=r;
   }
}

void checkprivFrom_(euid,logname,override)uid_t euid;const char*logname;
 int override;
{ static const char*const trusted_ids[]=TRUSTED_IDS;
  privs=1;					/* assume they're privileged */
  if(Deliverymode&&*trusted_ids&&uid!=euid)
   { struct group*grp;const char*const*kp;
     if(logname)			      /* check out the invoker's uid */
	for(kp=trusted_ids;*kp;kp++)
	   if(!strcmp(logname,*kp))	    /* is it amongst the privileged? */
	      goto privileged;
     if(grp=getgrgid(gid))		      /* check out the invoker's gid */
	for(logname=grp->gr_name,kp=trusted_ids;*kp;kp++)
	   if(!strcmp(logname,*kp))	      /* is it among the privileged? */
	      goto privileged;
     privs= -override;		/* override only matters when not privileged */
   }
privileged:
  endgrent();
}
