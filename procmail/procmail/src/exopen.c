/************************************************************************
 *	Collection of NFS resistant exclusive creat routines		*
 *									*
 *	Copyright (c) 1990-1997, S.R. van den Berg, The Netherlands	*
 *	Copyright (c) 1999-2001, Philip Guenther, The United States	*
 *							of America	*
 *	#include "../README"						*
 ************************************************************************/
#ifdef RCS
static /*const*/char rcsid[]=
 "$Id: exopen.c,v 1.44 2001/08/26 21:10:11 guenther Exp $";
#endif
#include "procmail.h"
#include "acommon.h"
#include "robust.h"
#include "misc.h"
#include "exopen.h"
#include "lastdirsep.h"
#include "sublib.h"

static const char*safehost P((void)) /* return a hostname safe for filenames */
{ static const char*sname=0;
  if(!sname)
   { char*to;const char*from=hostname();
     int badchars=0;
     for(to=(char*)from;*to;to++)     /* check for characters that shouldn't */
	if(*to=='/'||*to==':'||*to=='\\')		  /* be in hostnames */
	   badchars++;
     if(!badchars)			      /* it's clean, pass it through */
	sname=from;
     else if(!(sname=to=malloc(3*badchars+strlen(from)+1)))
	sname="";					       /* no memory! */
     else
      { int c;
	while(badchars)
	   switch(c=(unsigned char)*from++)
	    { default:*to++=c;
		 break;
	      case '\0':from--;to--;		     /* "this cannot happen" */
		 break;
	      case '/':case ':':case '\\':	 /* we'll remap them to \ooo */
		 *to++='\\';
		 *to++='0'+(c>>6);
		 *to++='0'+((c>>3)&7);
		 *to++='0'+(c&7);
		 badchars--;
	    }
	strcpy(to,from);		    /* copy the remaining characters */
      }
   }
  return sname;
}

int unique(full,p,len,mode,verbos,flags)char*const full;char*p;
 const size_t len;const mode_t mode;const int verbos,flags;
{ static const char s2c[]=".,+%";static int serial=STRLEN(s2c);
  static time_t t;char*dot,*end,*host;struct stat filebuf;
  int nicediff,i,didnice,retry=RETRYunique;
  if(flags&doCHOWN)		  /* semi-critical, try raising the priority */
   { nicediff=nice(0);SETerrno(0);nicediff-=nice(-NICE_RANGE);
     didnice=!errno;					  /* did we succeed? */
   }
  end=full+len;
  if(end-p<=UNIQnamelen-1)		      /* were we given enough space? */
     goto ret0;						    /* nope, give up */
  if(flags&doMAILDIR)				/* 'official' maildir format */
     dot=p;
  else						     /* 'traditional' format */
     *p=UNIQ_PREFIX,dot=ultoan((unsigned long)thepid,p+1);
  if(serial<STRLEN(s2c))
     goto in;
  do
   { if(serial>STRLEN(s2c)-1)			     /* roll over the count? */
      { ;{ time_t t2;
	   while(t==(t2=time((time_t*)0)))	/* make sure time has passed */
	      ssleep(1);				   /* tap tap tap... */
	   serial=0;t=t2;
	 }
in:	if(flags&doMAILDIR)
	 { dot=ultstr(0,(unsigned long)t,p);	      /* time.pid_s.hostname */
	   *dot='.';
	   dot=ultstr(0,(unsigned long)thepid,dot+1);
	   *dot++='_';
	   host=dot+2;
	 }
	else
	   host=1+ultoan((unsigned long)t,dot+1);	/* _pid%time.hostname */
	host[-1]='.';				  /* add the ".hostname" part */
	strlcpy(host,safehost(),end-host);
      }
     *dot=(flags&doMAILDIR)?'0'+serial:s2c[serial];
     serial++;
     i=lstat(full,&filebuf);
#ifdef ENAMETOOLONG
     if(i&&errno==ENAMETOOLONG)
      { char*op,*ldp;
	op=lastdirsep(full);
	ldp=op+1;		   /* keep track to avoid shortening past it */
	if(end-op>MINnamelen+1)			   /* guess at a safe length */
	   op+=MINnamelen+1;			  /* start at MINnamelen out */
	else
	   op=end-1;			    /* this shouldn't happen, but... */
	do
	   *--op='\0';					     /* try chopping */
	while((i=lstat(full,&filebuf))&&errno==ENAMETOOLONG&&op>ldp);
      }	      /* either it stopped being a problem or we ran out of filename */
#endif
   }
#ifndef O_CREAT
#define ropen(path,type,mode)	creat(path,mode)
#endif
  while((!i||errno!=ENOENT||	      /* casually check if it already exists */
	 (0>(i=ropen(full,O_WRONLY|O_CREAT|O_EXCL,mode))&&errno==EEXIST))&&
	(i= -1,retry--));
  if(flags&doCHOWN&&didnice)
     nice(nicediff);		   /* put back the priority to the old level */
  if(i<0)
   { if(verbos)			      /* this error message can be confusing */
	writeerr(full);					 /* for casual users */
     goto ret0;
   }
#ifdef NOfstat
  if(flags&doCHOWN)
   { if(
#else
  if(flags&doCHECK)
   { struct stat fdbuf;
     fstat(i,&fdbuf);			/* match between the file descriptor */
#define NEQ(what)	(fdbuf.what!=filebuf.what)	    /* and the file? */
     if(lstat(full,&filebuf)||filebuf.st_nlink!=1||filebuf.st_size||
	NEQ(st_dev)||NEQ(st_ino)||NEQ(st_uid)||NEQ(st_gid)||
	 flags&doCHOWN&&
#endif
	 chown(full,uid,sgid))
      { rclose(i);unlink(full);			 /* forget it, no permission */
ret0:	return flags&doFD?-1:0;
      }
   }
  if(flags&doLOCK)
     rwrite(i,"0",1);			   /* pid 0, `works' across networks */
  if(flags&doFD)
     return i;
  rclose(i);
  return 1;
}
				     /* rename MUST fail if already existent */
int myrename(old,newn)const char*const old,*const newn;
{ int fd,serrno;
  fd=hlink(old,newn);serrno=errno;unlink(old);
  if(fd>0)rclose(fd-1);
  SETerrno(serrno);
  return fd<0?-1:0;
}

						     /* NFS-resistant link() */
int rlink(old,newn,st)const char*const old,*const newn;struct stat*st;
{ if(link(old,newn))
   { register int serrno,ret;struct stat sto,stn;
     serrno=errno;ret= -1;
#undef NEQ			       /* compare files to see if the link() */
#define NEQ(what)	(sto.what!=stn.what)	       /* actually succeeded */
     if(lstat(old,&sto)||(ret=1,lstat(newn,&stn)||
	NEQ(st_dev)||NEQ(st_ino)||NEQ(st_uid)||NEQ(st_gid)||
	S_ISLNK(sto.st_mode)))			    /* symlinks are also bad */
      { SETerrno(serrno);
	if(st&&ret>0)
	 { *st=sto;				       /* save the stat data */
	   return ret;				    /* it was a real failure */
	 }
	return -1;
      }
     /*SETerrno(serrno);*/   /* we really succeeded, don't bother with errno */
   }
  return 0;
}
		 /* hardlink with fallback for systems that don't support it */
int hlink(old,newn)const char*const old,*const newn;
{ int ret;struct stat stbuf;
  if(0<(ret=rlink(old,newn,&stbuf)))		      /* try a real hardlink */
   { int fd;
#ifdef O_CREAT				       /* failure due to filesystem? */
     if(stbuf.st_nlink<2&&errno==EXDEV&&     /* try it by conventional means */
	0<=(fd=ropen(newn,O_WRONLY|O_CREAT|O_EXCL,stbuf.st_mode)))
	return fd+1;
#endif
     return -1;
   }
  return ret;				 /* success, or the stat failed also */
}
