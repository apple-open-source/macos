/************************************************************************
 *	Routines that deal with the mailfolder(format)			*
 *									*
 *	Copyright (c) 1990-1999, S.R. van den Berg, The Netherlands	*
 *	#include "../README"						*
 ************************************************************************/
#ifdef RCS
static /*const*/char rcsid[]=
 "$Id: mailfold.c,v 1.1.1.1 1999/09/23 17:30:07 wsanchez Exp $";
#endif
#include "procmail.h"
#include "acommon.h"
#include "sublib.h"
#include "robust.h"
#include "shell.h"
#include "misc.h"
#include "pipes.h"
#include "common.h"
#include "exopen.h"
#include "goodies.h"
#include "locking.h"
#include "mailfold.h"

int logopened,tofile,rawnonl;
off_t lasttell;
static long lastdump;
static volatile int mailread;	/* if the mail is completely read in already */
static struct dyna_long confield;		  /* escapes, concatenations */
static const char*realstart,*restbody;
static const char from_expr[]=FROM_EXPR;

static const char*fifrom(fromw,lbound,ubound)
 const char*fromw,*const lbound;char*const ubound;
{ int i;					   /* terminate & scan block */
  i= *ubound;*ubound='\0';fromw=strstr(mx(fromw,lbound),from_expr);*ubound=i;
  return fromw;
}

static int doesc;
			       /* inserts escape characters on outgoing mail */
static long getchunk(s,fromw,len)const int s;const char*fromw;const long len;
{ static const char esc[]=ESCAP,*ffrom,*endp;
  if(doesc)		       /* still something to escape since last time? */
     doesc=0,rwrite(s,esc,STRLEN(esc)),lastdump++;		/* escape it */
  ffrom=0;					 /* start with a clean slate */
  if(fromw<thebody)			   /* are we writing the header now? */
     ffrom=fifrom(fromw,realstart,thebody);		      /* scan header */
  if(!ffrom&&(endp=fromw+len)>restbody)	       /* nothing yet? but in range? */
   { if((endp+=STRLEN(from_expr)-1)>(ffrom=themail+filled))	/* add slack */
	endp=(char*)ffrom;		  /* make sure we stay within bounds */
     ffrom=fifrom(fromw,restbody,endp);			  /* scan body block */
   }
  return ffrom?(doesc=1,(ffrom-fromw)+1L):len;	 /* +1 to write out the '\n' */
}

long dump(s,source,len)const int s;const char*source;long len;
{ int i;long part;
  lasttell=i= -1;SETerrno(EBADF);
  if(s>=0)
   { if(tofile&&(lseek(s,(off_t)0,SEEK_END),fdlock(s)))
	nlog("Kernel-lock failed\n");
     lastdump=len;doesc=0;
     part=tofile==to_FOLDER&&!rawnonl?getchunk(s,source,len):len;
     lasttell=lseek(s,(off_t)0,SEEK_END);
     if(!rawnonl)
      { smboxseparator(s);			       /* optional separator */
#ifndef NO_NFS_ATIME_HACK	       /* if it is a file, trick NFS into an */
	if(part&&tofile==to_FILE)			    /* a_time<m_time */
	 { struct stat stbuf;
	   rwrite(s,source++,1);len--;part--;		     /* set the trap */
	   if(fstat(s,&stbuf)||					  /* needed? */
	    stbuf.st_mtime==stbuf.st_atime&&stbuf.st_size!=1)
	      ssleep(1);  /* ...what a difference this (tea) second makes... */
	 }
#endif
      }
     goto jin;
     do
      { part=getchunk(s,source,len);
jin:	while(part&&(i=rwrite(s,source,BLKSIZ<part?BLKSIZ:(int)part)))
	 { if(i<0)
	      goto writefin;
	   part-=i;len-=i;source+=i;
	 }
      }
     while(len);
     if(!rawnonl)
      { if(!len&&(lastdump<2||!(source[-1]=='\n'&&source[-2]=='\n')))
	   lastdump++,rwrite(s,newline,1);     /* message always ends with a */
	emboxseparator(s);	 /* newline and an optional custom separator */
      }
writefin:
     ;{ int serrno=errno;		       /* save any error information */
	if(tofile&&fdunlock())
	   nlog("Kernel-unlock failed\n");
	SETerrno(serrno);
      }
     i=rclose(s);
   }			   /* return an error even if nothing was to be sent */
  tofile=0;
  return i&&!len?-1:len;
}

static int dirfile(chp,linkonly)char*const chp;const int linkonly;
{ static const char lkingto[]="Linking to";
  if(chp)
   { long i=0;			     /* first let us try to prime i with the */
#ifndef NOopendir		     /* highest MH folder number we can find */
     long j;DIR*dirp;struct dirent*dp;char*chp2;
     if(dirp=opendir(buf))
      { while(dp=readdir(dirp))		/* there still are directory entries */
	   if((j=strtol(dp->d_name,&chp2,10))>i&&!*chp2)
	      i=j;			    /* yep, we found a higher number */
	closedir(dirp);				     /* aren't we neat today */
      }
     else
	readerr(buf);
#endif /* NOopendir */
     if(chp-buf+sizeNUM(i)-XTRAlinebuf>linebuf)
exlb: { nlog(exceededlb);setoverflow();
	goto ret;
      }
     ;{ int ok;
	do ultstr(0,++i,chp);		       /* find first empty MH folder */
	while((ok=linkonly?rlink(buf2,buf,0):hlink(buf2,buf))&&errno==EEXIST);
	if(linkonly)
	 { yell(lkingto,buf);
	   if(ok)
	      goto nolnk;
	   goto didlnk;
	 }
      }
     unlink(buf2);
     goto opn;
   }
  ;{ struct stat stbuf;
     char*p=strchr(buf,'\0');
     if(p-buf+strlen(msgprefix)+sizeNUM(stbuf.st_ino)-XTRAlinebuf>linebuf)
	goto exlb;
     stat(buf2,&stbuf);
     ultoan((unsigned long)stbuf.st_ino,      /* filename with i-node number */
      strchr(strcat(p,msgprefix),'\0'));
   }
  if(linkonly)
   { yell(lkingto,buf);
     if(rlink(buf2,buf,0)) /* hardlink the new file, it's a directory folder */
nolnk:	nlog("Couldn't make link to"),logqnl(buf);
     else
didlnk:
      { size_t len;char*p;
	Stdout=buf;primeStdout("");
	p=realloc(Stdout,(Stdfilled=(len=strlen(Stdout))+1+strlen(buf))+1);
	p[len]=' ';strcpy(p+len+1,buf);retbStdout(p);Stdout=0;
      }
     goto ret;
   }
  if(!rename(buf2,buf))		       /* rename it, we need the same i-node */
opn: return opena(buf);
ret:
  return -1;
}

static int ismhdir(chp)char*const chp;
{ if(chp-1>=buf&&chp[-1]==*MCDIRSEP_&&*chp==chCURDIR)
   { chp[-1]='\0';
     return 1;
   }
  return 0;
}
				       /* open file or new file in directory */
static int deliver(boxname,linkfolder)char*boxname,*linkfolder;
{ struct stat stbuf;char*chp;int mhdir;mode_t numask;
  asgnlastf=1;
  if(*boxname=='|'&&(!linkfolder||linkfolder==Tmnate))
   { setlastfolder(boxname);
     return rdup(savstdout);
   }
  numask=UPDATE_MASK&~cumask;tofile=to_FILE;
  if(boxname!=buf)
     strcpy(buf,boxname);		 /* boxname can be found back in buf */
  if(*(chp=buf))				  /* not null length target? */
     chp=strchr(buf,'\0')-1;		     /* point to just before the end */
  mhdir=ismhdir(chp);				      /* is it an MH folder? */
  if(!stat(boxname,&stbuf))					/* it exists */
   { if(numask&&!(stbuf.st_mode&UPDATE_MASK))
	chmod(boxname,stbuf.st_mode|UPDATE_MASK);
     if(!S_ISDIR(stbuf.st_mode))	 /* it exists and is not a directory */
	goto makefile;				/* no, create a regular file */
   }
  else if(!mhdir||mkdir(buf,NORMdirperm))    /* shouldn't it be a directory? */
makefile:
   { if(linkfolder)	  /* any leftovers?  Now is the time to display them */
	concatenate(linkfolder),skipped(linkfolder);
     tofile=strcmp(devnull,buf)?to_FOLDER:(rawnonl=1,0);
     return opena(boxname);
   }
  if(linkfolder)		    /* any additional directories specified? */
   { size_t blen;
     if(blen=Tmnate-linkfolder)		       /* copy the names into safety */
	Tmnate=(linkfolder=tmemmove(malloc(blen),linkfolder,blen))+blen;
     else
	linkfolder=0;
   }
  if(mhdir)				/* buf should contain directory name */
     *chp='\0',chp[-1]= *MCDIRSEP_,strcpy(buf2,buf);	   /* it ended in /. */
  else					 /* fixup directory name, append a / */
     strcat(chp,MCDIRSEP_),strcpy(buf2,buf),chp=0;
  if(strlen(buf2)+UNIQnamelen>linebuf)
   { nlog(exceededlb);setoverflow();
     return -1;
   }
  ;{ int fd= -1;		/* generate the name for the first directory */
     if(unique(buf2,strchr(buf2,'\0'),NORMperm,verbose,0)&&
      (fd=dirfile(chp,0))>=0&&linkfolder)	 /* save the file descriptor */
	for(strcpy(buf2,buf),boxname=linkfolder;boxname!=Tmnate;)
	 { strcpy(buf,boxname);		/* go through the list of other dirs */
	   if(*(chp=buf))
	      chp=strchr(buf,'\0')-1;
	   mhdir=ismhdir(chp);			      /* is it an MH folder? */
	   if(stat(boxname,&stbuf))			 /* it doesn't exist */
	      mkdir(buf,NORMdirperm);				/* create it */
	   else if(numask&&!(stbuf.st_mode&UPDATE_MASK))
	      chmod(buf,stbuf.st_mode|UPDATE_MASK);
	   if(mhdir)
	      *chp='\0',chp[-1]= *MCDIRSEP_;
	   else				 /* fixup directory name, append a / */
	      strcat(chp,MCDIRSEP_),chp=0;
	   dirfile(chp,1);		/* link it with the original in buf2 */
	   while(*boxname++);		  /* skip to the next directory name */
	 }
     if(linkfolder)					   /* free our cache */
	free(linkfolder);
     return fd;			      /* return the file descriptor we saved */
   }
}

int writefolder(boxname,linkfolder,source,len,ignwerr)char*boxname,*linkfolder;
 const char*source;const long len;const int ignwerr;
{ if(dump(deliver(boxname,linkfolder),source,len)&&!ignwerr)
   { switch(errno)
      { case ENOSPC:nlog("No space left to finish writing"),logqnl(buf);
	   break;
#ifdef EDQUOT
	case EDQUOT:nlog("Quota exceeded while writing"),logqnl(buf);
	   break;
#endif
	default:writeerr(buf);
      }
     if(lasttell>=0&&!truncate(boxname,lasttell)&&(logopened||verbose))
	nlog("Truncated file to former size\n");    /* undo appended garbage */
     return 0;
   }
  return 1;
}


void logabstract(lstfolder)const char*const lstfolder;
{ if(lgabstract>0||(logopened||verbose)&&lgabstract)  /* don't mail it back? */
   { char*chp,*chp2;int i;static const char sfolder[]=FOLDER;
     if(mailread)			  /* is the mail completely read in? */
      { i= *thebody;*thebody='\0';     /* terminate the header, just in case */
	if(eqFrom_(chp=themail))		       /* any "From " header */
	 { if(chp=strchr(themail,'\n'))
	      *chp='\0';
	   else
	      chp=thebody;			  /* preserve mailbox format */
	   elog(themail);elog(newline);*chp='\n';	     /* (any length) */
	 }
	*thebody=i;			   /* eliminate the terminator again */
	if(!(lcking&lck_ALLOCLIB)&&		/* don't reenter malloc/free */
	 (chp=egrepin(NSUBJECT,chp,(long)(thebody-chp),0)))
	 { size_t subjlen;
	   for(chp2= --chp;*--chp2!='\n';);
	   if((subjlen=chp-++chp2)>MAXSUBJECTSHOW)
	      subjlen=MAXSUBJECTSHOW;		    /* keep it within bounds */
	   ((char*)tmemmove(buf,chp2,subjlen))[subjlen]='\0';detab(buf);
	   elog(" ");elog(buf);elog(newline);
	 }
      }
     elog(sfolder);i=strlen(strncpy(buf,lstfolder,MAXfoldlen))+STRLEN(sfolder);
     buf[MAXfoldlen]='\0';detab(buf);elog(buf);i-=i%TABWIDTH;	/* last dump */
     do elog(TABCHAR);
     while((i+=TABWIDTH)<LENoffset);
     ultstr(7,lastdump,buf);elog(buf);elog(newline);
   }
}

static int concnd;				 /* last concatenation value */

void concon(ch)const int ch;   /* flip between concatenated and split fields */
{ size_t i;
  if(concnd!=ch)				   /* is this run redundant? */
   { concnd=ch;			      /* no, but note this one for next time */
     for(i=confield.filled;i;)		   /* step through the saved offsets */
	themail[confield.offs[--i]]=ch;		       /* and flip every one */
   }
}

void readmail(rhead,tobesent)const long tobesent;
{ char*chp,*pastend;static size_t contlengthoffset;
  ;{ long dfilled;
     if(rhead)					/* only read in a new header */
      { dfilled=mailread=0;chp=readdyn(malloc(1),&dfilled);filled-=tobesent;
	if(tobesent<dfilled)		   /* adjust buffer size (grow only) */
	 { char*oldp=themail;
	   thebody=(themail=realloc(themail,dfilled+filled))+(thebody-oldp);
	 }
	tmemmove(themail+dfilled,thebody,filled);tmemmove(themail,chp,dfilled);
	free(chp);themail=realloc(themail,1+(filled+=dfilled));
      }
     else
      { if(!mailread||!filled)
	   rhead=1;	 /* yup, we read in a new header as well as new mail */
	mailread=0;dfilled=thebody-themail;themail=readdyn(themail,&filled);
      }
     *(pastend=filled+(thebody=themail))='\0';		   /* terminate mail */
     while(thebody<pastend&&*thebody++=='\n');	     /* skip leading garbage */
     realstart=thebody;
     if(rhead)			      /* did we read in a new header anyway? */
      { confield.filled=0;concnd='\n';
	while(thebody=strchr(thebody,'\n'))
	   switch(*++thebody)			    /* mark continued fields */
	    { case '\t':case ' ':app_val(&confield,(off_t)(thebody-1-themail));
	      default:
		 continue;		   /* empty line marks end of header */
	      case '\n':thebody++;
		 goto eofheader;
	    }
	thebody=pastend;      /* provide a default, in case there is no body */
eofheader:
	contlengthoffset=0;		      /* traditional Berkeley format */
	if(!berkeley&&				  /* ignores Content-Length: */
	   (chp=egrepin("^Content-Length:",themail,(long)(thebody-themail),0)))
	   contlengthoffset=chp-themail;
      }
     else			       /* no new header read, keep it simple */
	thebody=themail+dfilled; /* that means we know where the body starts */
   }		      /* to make sure that the first From_ line is uninjured */
  if((chp=thebody)>themail)
     chp--;
  if(contlengthoffset)
   { unsigned places;long cntlen,actcntlen;charNUM(num,cntlen);
     chp=themail+contlengthoffset;cntlen=filled-(thebody-themail);
     if(filled>1&&themail[filled-2]=='\n')		 /* no phantom '\n'? */
	cntlen--;		     /* make sure it points to the last '\n' */
     for(actcntlen=places=0;;)
      { switch(*chp)
	 { default:					/* fill'r up, please */
	      if(places<=sizeof num-2)
		 *chp++='9',places++,actcntlen=(unsigned long)actcntlen*10+9;
	      else
		 *chp++=' ';		 /* ultra long Content-Length: field */
	      continue;
	   case '\n':case '\0':;		      /* ok, end of the line */
	 }
	break;
      }
     if(cntlen<=0)			       /* any Content-Length at all? */
	cntlen=0;
     ultstr(places,cntlen,num);			       /* our preferred size */
     if(!num[places])		       /* does it fit in the existing space? */
	tmemmove(themail+contlengthoffset,num,places),actcntlen=cntlen;
     chp=thebody+actcntlen;		  /* skip the actual no we specified */
   }
  restbody=chp;mailread=1;
}
			  /* tries to locate the timestamp on the From_ line */
char*findtstamp(start,end)const char*start,*end;
{ end-=25;
  if(*start==' '&&(++start==end||*start==' '&&++start==end))
     return (char*)start-1;
  start=skpspace(start);start+=strcspn(start," \t\n");	/* jump over address */
  if(skpspace(start)>=end)			       /* enough space left? */
     return (char*)start;	 /* no, too small for a timestamp, stop here */
  while(!(end[13]==':'&&end[15]==':')&&--end>start);	  /* search for :..: */
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
