/************************************************************************
 *	Routines that deal with the mailfolder(format)			*
 *									*
 *	Copyright (c) 1990-1999, S.R. van den Berg, The Netherlands	*
 *	Copyright (c) 1999-2001, Philip Guenther, The United States	*
 *							of America	*
 *	#include "../README"						*
 ************************************************************************/
#ifdef RCS
static /*const*/char rcsid[]=
 "$Id: mailfold.c,v 1.104 2001/08/04 07:16:26 guenther Exp $";
#endif
#include "procmail.h"
#include "acommon.h"
#include "sublib.h"
#include "robust.h"
#include "misc.h"
#include "memblk.h"
#include "pipes.h"
#include "common.h"
#include "exopen.h"
#include "goodies.h"
#include "variables.h"
#include "locking.h"
#include "lastdirsep.h"
#include "foldinfo.h"
#include "from.h"
#include "shell.h"
#include "mailfold.h"

int logopened,rawnonl;
off_t lasttell;
static long lastdump;
static volatile int mailread;	/* if the mail is completely read in already */
static struct dyna_array confield;		  /* escapes, concatenations */
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
   { if((endp+=STRLEN(from_expr)-1)>(ffrom=themail.p+filled))	/* add slack */
	endp=(char*)ffrom;		  /* make sure we stay within bounds */
     ffrom=fifrom(fromw,restbody,endp);			  /* scan body block */
   }
  return ffrom?(doesc=1,(ffrom-fromw)+1L):len;	 /* +1 to write out the '\n' */
}

#ifdef sMAILBOX_SEPARATOR
#define smboxseparator(fd)	(ft_delim(type)&&\
 (part=len,rwrite(fd,sMAILBOX_SEPARATOR,STRLEN(sMAILBOX_SEPARATOR))))
#define MAILBOX_SEPARATOR
#else
#define smboxseparator(fd)
#endif /* sMAILBOX_SEPARATOR */
#ifdef eMAILBOX_SEPARATOR
#define emboxseparator(fd)	\
 (ft_delim(type)&&rwrite(fd,eMAILBOX_SEPARATOR,STRLEN(eMAILBOX_SEPARATOR)))
#ifndef MAILBOX_SEPARATOR
#define MAILBOX_SEPARATOR
#endif
#else
#define emboxseparator(fd)
#endif /* eMAILBOX_SEPARATOR */

long dump(s,type,source,len)const int s,type;const char*source;
 long len;
{ int i;long part;
  lasttell=i= -1;SETerrno(EBADF);
  if(s>=0)
   { if(ft_lock(type)&&(lseek(s,(off_t)0,SEEK_END),fdlock(s)))
	nlog("Kernel-lock failed\n");
     lastdump=len;doesc=0;
     if(ft_delim(type)&&!rawnonl)
	part=getchunk(s,source,len);			/* must escape From_ */
     else
	part=len;
     lasttell=lseek(s,(off_t)0,SEEK_END);
     if(!rawnonl)
      { smboxseparator(s);			       /* optional separator */
#ifndef NO_NFS_ATIME_HACK	       /* if it is a file, trick NFS into an */
	if(part&&ft_atime(type))			    /* a_time<m_time */
	 { struct stat stbuf;
	   rwrite(s,source++,1);len--;part--;		     /* set the trap */
	   if(fstat(s,&stbuf)||					  /* needed? */
	    stbuf.st_mtime==stbuf.st_atime)
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
      { if(!len&&(lastdump<2||!(source[-1]=='\n'&&source[-2]=='\n'))&&
	 ft_forceblank(type))
	   lastdump++,rwrite(s,newline,1);     /* message always ends with a */
	emboxseparator(s);	 /* newline and an optional custom separator */
      }
writefin:
     i=type!=ft_PIPE&&fsync(s)&&errno!=EINVAL;	  /* EINVAL => wasn't a file */
     if(ft_lock(type))
      { int serrno=errno;		       /* save any error information */
	if(fdunlock())
	   nlog("Kernel-unlock failed\n");
	SETerrno(serrno);
      }
     i=rclose(s)||i;
   }			   /* return an error even if nothing was to be sent */
  return i&&!len?-1:len;
}

static int dirfile(chp,linkonly,type)char*const chp;const int linkonly,type;
{ static const char lkingto[]="Linking to";struct stat stbuf;
  if(type==ft_MH)
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
     if(chp-buf+sizeNUM(i)>linebuf)
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
     goto opn;
   }
  else if(type==ft_MAILDIR)
   { if(!unique(buf,chp,linebuf,NORMperm,verbose,doMAILDIR))
	goto ret;
     unlink(buf);			 /* found a name, remove file in tmp */
     memcpy(chp-MAILDIRLEN-1,maildirnew,MAILDIRLEN);	/* but link directly */
   }								 /* into new */
  else								   /* ft_DIR */
   { size_t mpl=strlen(msgprefix);
     if(chp-buf+mpl+sizeNUM(stbuf.st_ino)>linebuf)
	goto exlb;
     stat(buf2,&stbuf);			      /* filename with i-node number */
     ultoan((unsigned long)stbuf.st_ino,strcpy(chp,msgprefix)+mpl);
   }
  if(linkonly)
   { yell(lkingto,buf);
     if(rlink(buf2,buf,0)) /* hardlink the new file, it's a directory folder */
nolnk:	nlog("Couldn't make link to"),logqnl(buf);
     else
didlnk: appendlastvar(buf);		     /* lastvar is "LASTFOLDER" here */
     goto ret;
   }
  if(!rlink(buf2,buf,0))			      /* try rename-via-link */
opn: unlink(buf2);			     /* success; remove the original */
  else if(errno=EEXIST||!stat(buf,&stbuf)||errno!=ENOENT||rename(buf2,buf))
ret: return -1;	 /* rename it, but only if it won't replace an existing file */
  setlastfolder(buf);
  return opena(buf);
}

int writefolder(boxname,linkfolder,source,len,ignwerr,dolock)
 char*boxname,*linkfolder;const char*source;long len;const int ignwerr,dolock;
{ char*chp,*chp2;mode_t mode;int fd,type;
  if(*boxname=='|'&&(!linkfolder||linkfolder==Tmnate))
   { setlastfolder(boxname);
     fd=rdup(Deliverymode==2?STDOUT:savstdout);
     type=ft_PIPE;
     goto dumpc;
   }
  if(boxname!=buf)
     strcpy(buf,boxname);		 /* boxname can be found back in buf */
  if(linkfolder)		    /* any additional directories specified? */
   { size_t blen;
     if(blen=Tmnate-linkfolder)		       /* copy the names into safety */
	Tmnate=(linkfolder=tmemmove(malloc(blen),linkfolder,blen))+blen;
     else
	linkfolder=0;
   }
  type=foldertype(0,0,&mode,0);			     /* the envelope please! */
  chp=strchr(buf,'\0');
  switch(type)
   { case ft_FILE:
	if(linkfolder)	  /* any leftovers?  Now is the time to display them */
	   concatenate(linkfolder),skipped(linkfolder),free(linkfolder);
	if(!strcmp(devnull,buf))
	   type=ft_PIPE,rawnonl=1;	     /* save the effort on /dev/null */
	else if(!(UPDATE_MASK&(mode|cumask)))
	   chmod(boxname,mode|UPDATE_MASK);
	if(dolock&&type!=ft_PIPE)
	 { strcpy(chp,lockext);
	   if(!globlock||strcmp(buf,globlock))
	      lockit(tstrdup(buf),&loclock);
	   *chp='\0';
	 }
	setlastfolder(boxname);
	fd=opena(boxname);
dumpc:	if(dump(fd,type,source,len)&&!ignwerr)
dumpf:	 { switch(errno)
	    { case ENOSPC:nlog("No space left to finish writing"),logqnl(buf);
		 break;
#ifdef EDQUOT
	      case EDQUOT:nlog("Quota exceeded while writing"),logqnl(buf);
		 break;
#endif
	      default:writeerr(buf);
	    }
	   if(lasttell>=0&&!truncate(boxname,lasttell)&&(logopened||verbose))
	      nlog("Truncated file to former size\n");	    /* undo garbage */
ret0:	   return 0;
	 }
	return 1;
     case ft_TOOLONG:
exlb:	nlog(exceededlb);setoverflow();
     case ft_CANTCREATE:
retf:	if(linkfolder)
	   free(linkfolder);
	goto ret0;
     case ft_MAILDIR:
	if(source==themail.p)			      /* skip leading From_? */
	   source=skipFrom_(source,&len);
	strcpy(buf2,buf);
	chp2=buf2+(chp-buf)-MAILDIRLEN;
	*chp++= *MCDIRSEP_;
	;{ int retries=MAILDIRretries;
	   for(;;)
	    { struct stat stbuf;
	      if(0>(fd=unique(buf,chp,linebuf,NORMperm,verbose,doFD|doMAILDIR)))
		 goto nfail;
	      if(dump(fd,ft_MAILDIR,source,len)&&!ignwerr)
		 goto failed;
	      strcpy(chp2,maildirnew);
	      chp2+=MAILDIRLEN;
	      *chp2++= *MCDIRSEP_;
	      strcpy(chp2,chp);
	      if(!rlink(buf,buf2,0))
	       { unlink(buf);
		 break;
	       }
	      else if(errno!=EEXIST&&lstat(buf2,&stbuf)&&errno==ENOENT&&
	       !rename(buf,buf2))
		 break;
	      unlink(buf);
	      if(!retries--)
		 goto nfail;
	    }
	 }
	setlastfolder(buf2);
	break;
     case ft_MH:
#if 0
	if(source==themail.p)
	   source=skipFrom_(source,&len);
#endif
     default:						     /* case ft_DIR: */
	*chp++= *MCDIRSEP_;
	strcpy(buf2,buf);
	chp2=buf2+(chp-buf);
	if(!unique(buf2,chp2,linebuf,NORMperm,verbose,0)||
	 0>(fd=dirfile(chp,0,type)))
nfail:	 { nlog("Couldn't create or rename temp file");logqnl(buf);
	   goto retf;
	 }
	if(dump(fd,type,source,len)&&!ignwerr)
	 { strcpy(buf,buf2);
failed:	   unlink(buf);lasttell= -1;
	   if(linkfolder)
	      free(linkfolder);
	   goto dumpf;
	 }
	strcpy(buf2,buf);
	break;
   }
  if(!(UPDATE_MASK&(mode|cumask)))
   { chp[-1]='\0';				      /* restore folder name */
     chmod(buf,mode|UPDATE_MASK);
   }
  if(linkfolder)				 /* handle secondary folders */
   { for(boxname=linkfolder;boxname!=Tmnate;boxname=strchr(boxname,'\0')+1)
      { strcpy(buf,boxname);
	switch(type=foldertype(0,1,&mode,0))
	 { case ft_TOOLONG:goto exlb;
	   case ft_CANTCREATE:continue;			     /* just skip it */
	   case ft_DIR:case ft_MH:case ft_MAILDIR:
	      chp=strchr(buf,'\0');
	      *chp= *MCDIRSEP_;
	      if(dirfile(chp+1,1,type)) /* link it with the original in buf2 */
		 if(!(UPDATE_MASK&(mode|cumask)))
		  { *chp='\0';
		    chmod(buf,mode|UPDATE_MASK);
		  }
	      break;
	 }
      }
     free(linkfolder);
   }
  return 1;
}

void logabstract(lstfolder)const char*const lstfolder;
{ if(lgabstract>0||(logopened||verbose)&&lgabstract)  /* don't mail it back? */
   { char*chp,*chp2;int i;static const char sfolder[]=FOLDER;
     if(mailread)			  /* is the mail completely read in? */
      { i= *thebody;*thebody='\0';     /* terminate the header, just in case */
	if(eqFrom_(chp=themail.p))		       /* any "From " header */
	 { if(chp=strchr(themail.p,'\n'))
	      *chp='\0';
	   else
	      chp=thebody;			  /* preserve mailbox format */
	   elog(themail.p);elog(newline);*chp='\n';	     /* (any length) */
	 }
	*thebody=i;			   /* eliminate the terminator again */
	if(!nextexit&&				/* don't reenter malloc/free */
	 (chp=egrepin(NSUBJECT,chp,(long)(thebody-chp),0)))
	 { size_t subjlen;
	   for(chp2= --chp;*--chp2!='\n';);
	   if((subjlen=chp-++chp2)>MAXSUBJECTSHOW)
	      subjlen=MAXSUBJECTSHOW;		    /* keep it within bounds */
	   ((char*)tmemmove(buf,chp2,subjlen))[subjlen]='\0';detab(buf);
	   elog(" ");elog(buf);elog(newline);
	 }
      }
     elog(sfolder);strlcpy(buf,lstfolder,MAXfoldlen);detab(buf);elog(buf);
     i=strlen(buf)+STRLEN(sfolder);i-=i%TABWIDTH;		/* last dump */
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
	themail.p[acc_vall(confield,--i)]=ch;	       /* and flip every one */
   }
}

void readmail(rhead,tobesent)const long tobesent;
{ char*chp,*pastend;static size_t contlengthoffset;
  ;{ long dfilled;
     if(rhead==2)		  /* already read, just examine what we have */
	dfilled=mailread=0;
     else if(rhead)				/* only read in a new header */
      { memblk new;
	dfilled=mailread=0;makeblock(&new,0);readdyn(&new,&dfilled,0);
	if(tobesent>dfilled&&isprivate)		     /* put it in place here */
	 { tmemmove(themail.p+dfilled,thebody,filled-=tobesent);
	   tmemmove(themail.p,new.p,dfilled);
	   resizeblock(&themail,filled+=dfilled,1);
	   freeblock(&new);
	 }
	else			   /* too big or must share -- switch blocks */
	 { resizeblock(&new,filled-tobesent+dfilled,0);
	   tmemmove(new.p+dfilled,thebody,filled-=tobesent);
	   freeblock(&themail);
	   themail=new;private(1);
	   filled+=dfilled;
	 }
      }
     else
      { if(!mailread||!filled)
	   rhead=1;	 /* yup, we read in a new header as well as new mail */
	mailread=0;dfilled=thebody-themail.p;
	if(!isprivate)
	 { memblk new;
	   makeblock(&new,filled);
	   if(filled)
	      tmemmove(new.p,themail.p,filled);
	   freeblock(&themail);
	   themail=new;private(1);
	 }
	readdyn(&themail,&filled,filled+tobesent);
      }
     pastend=filled+(thebody=themail.p);
     while(thebody<pastend&&*thebody++=='\n');	     /* skip leading garbage */
     realstart=thebody;
     if(rhead)			      /* did we read in a new header anyway? */
      { confield.filled=0;concnd='\n';
	while(thebody=strchr(thebody,'\n'))
	   switch(*++thebody)			    /* mark continued fields */
	    { case '\t':case ' ':app_vall(confield,(long)(thebody-1-themail.p));
	      default:
		 continue;		   /* empty line marks end of header */
	      case '\n':thebody++;
		 goto eofheader;
	    }
	thebody=pastend;      /* provide a default, in case there is no body */
eofheader:
	contlengthoffset=0;		      /* traditional Berkeley format */
	if(!berkeley&&				  /* ignores Content-Length: */
	   (chp=egrepin("^Content-Length:",themail.p,
			(long)(thebody-themail.p),0)))
	   contlengthoffset=chp-themail.p;
      }
     else			       /* no new header read, keep it simple */
	thebody=themail.p+dfilled; /* that means we know where the body starts */
   }		      /* to make sure that the first From_ line is uninjured */
  if((chp=thebody)>themail.p)
     chp--;
  if(contlengthoffset)
   { unsigned places;long cntlen,actcntlen;charNUM(num,cntlen);
     chp=themail.p+contlengthoffset;cntlen=filled-(thebody-themail.p);
     if(filled>1&&themail.p[filled-2]=='\n')		 /* no phantom '\n'? */
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
	tmemmove(themail.p+contlengthoffset,num,places),actcntlen=cntlen;
     chp=thebody+actcntlen;		  /* skip the actual no we specified */
   }
  restbody=chp;mailread=1;
}
