/************************************************************************
 *	Routines that deal with understanding the folder types		*
 *									*
 *	Copyright (c) 1990-1999, S.R. van den Berg, The Netherlands	*
 *	Copyright (c) 1999-2001, Philip Guenther, The United States	*
 *							of America	*
 *	#include "../README"						*
 ************************************************************************/
#ifdef RCS
static /*const*/char rcsid[]=
 "$Id: foldinfo.c,v 1.11 2001/08/04 07:07:42 guenther Exp $";
#endif
#include "procmail.h"
#include "misc.h"
#include "lastdirsep.h"
#include "robust.h"
#include "exopen.h"
#include "goodies.h"
#include "locking.h"
#include "foldinfo.h"

static const char
 maildirtmp[]=MAILDIRtmp,maildircur[]=MAILDIRcur;
const char
 maildirnew[]=MAILDIRnew;
int accspooldir;		     /* can we write to the spool directory? */

/* determine the requested type, chopping off the type specifier and any
   extra trailing slashes */
static int folderparse P((void))
{ char*chp;int type;
  type=ft_FILE;chp=strchr(buf,'\0');
  switch(chp-buf)
   { case 2:
	if(chp[-1]==*MCDIRSEP_)				     /* "//" or "x/" */
	   chp--,type=ft_MAILDIR;
     case 0:case 1:						/* "" or "x" */
	goto ret;
   }			    /* Okay, put chp right before the type specifier */
  if(chp[-1]==chCURDIR&&chp[-2]==*MCDIRSEP_)			    /* foo/. */
     chp-=2,type=ft_MH;
  else if(chp[-1]==*MCDIRSEP_)					     /* foo/ */
     chp--,type=ft_MAILDIR;
  else							     /* no specifier */
     goto ret;
  while(chp-1>buf&&strchr(dirsep,chp[-1]))		    /* chop extra /s */
     chp--;
ret:
  *chp='\0';
  return type;
}

int rnmbogus(name,stbuf,i,dolog)const char*const name;	      /* move a file */
 const struct stat*const stbuf;const int i,dolog;	   /* out of the way */
{ static const char renbogus[]="Renamed bogus \"%s\" into \"%s\"",
   renfbogus[]="Couldn't rename bogus \"%s\" into \"%s\"",
   bogusprefix[]=BOGUSprefix;char*p;
  p=strchr(strcpy(strcpy(buf2+i,bogusprefix)+STRLEN(bogusprefix),
   getenv(lgname)),'\0');
  *p++='.';ultoan((unsigned long)stbuf->st_ino,p);	  /* i-node numbered */
  if(dolog)
   { nlog("Renaming bogus mailbox \"");elog(name);elog("\" info");
     logqnl(buf2);
   }
  if(rename(name,buf2))			   /* try and move it out of the way */
   { syslog(LOG_ALERT,renfbogus,name,buf2);		 /* danger!  danger! */
     return 1;
   }
  syslog(LOG_CRIT,renbogus,name,buf2);
  return 0;
}

/* return the named object's mode, making it a directory if it doesn't exist
 * and renaming it out of the way if it's not _just_right_ and we're being
 * paranoid */
static mode_t trymkdir(dir,paranoid,i)const char*const dir;
 const int paranoid,i;
{ struct stat stbuf;int tries=3-1;	     /* minus one for post-decrement */
  do
   { if(!(paranoid?lstat(dir,&stbuf):stat(dir,&stbuf)))	   /* does it exist? */
      { if(!paranoid||		     /* is it okay?  If we're trusting it is */
	   (S_ISDIR(stbuf.st_mode)&&	      /* else it must be a directory */
	    (stbuf.st_uid==uid||	       /* and have the correct owner */
	     !stbuf.st_uid&&!chown(dir,uid,sgid))))  /* or be safely fixable */
	   return stbuf.st_mode;				   /* bingo! */
	else if(rnmbogus(dir,&stbuf,i,1))  /* try and move it out of the way */
	   break;						/* couldn't! */
      }
     else if(errno!=ENOENT)	    /* something more fundamental went wrong */
	break;
     else if(!mkdir(dir,NORMdirperm))	  /* it's not there, can we make it? */
      { if(!paranoid)	      /* do we need to double check the permissions? */
	   return S_IFDIR|NORMdirperm&~cumask;			     /* nope */
	tries++;		/* the mkdir succeeded, so take another shot */
      }
   }while(tries-->0);
  return 0;
}

static int mkmaildir(buffer,chp,paranoid)char*const buffer,*const chp;
 const int paranoid;
{ mode_t mode;int i;
  if(paranoid)
     memcpy(buf2,buffer,i=chp-buffer+1),buf2[i-1]= *MCDIRSEP_,buf2[i]='\0';
  return
   (strcpy(chp,maildirnew),mode=trymkdir(buffer,paranoid,i),S_ISDIR(mode))&&
   (strcpy(chp,maildircur),mode=trymkdir(buffer,paranoid,i),S_ISDIR(mode))&&
   (strcpy(chp,maildirtmp),mode=trymkdir(buffer,paranoid,i),S_ISDIR(mode));
}					      /* leave tmp in buf on success */

int foldertype(type,forcedir,modep,paranoid)int type,forcedir;
 mode_t*const modep;struct stat*const paranoid;
{ struct stat stbuf;mode_t mode;int i;char*chp;
  if(!type)
     type=folderparse();
  switch(type)
   { case ft_MAILDIR:i=MAILDIRLEN;break;
     case ft_MH:i=0;break;
     case ft_FILE:
	i=0;					    /* resolve the ambiguity */
	if(!forcedir)
	 { if(paranoid?lstat(buf,&stbuf):stat(buf,&stbuf))
	    { if(paranoid)
	       { type=ft_NOTYET;
		 goto ret;
	       }
	      goto newfile;
	    }
	   else if(mode=stbuf.st_mode,!S_ISDIR(mode))
	      goto file;
	 }
	type=ft_DIR;
	break;
     default:					     /* "this cannot happen" */
	nlog("Internal error: improper type (");
	ltstr(0,type,buf2);elog(buf2);
	elog(") passed to foldertype for folder ");logqnl(buf);
	Terminate();
   }
  chp=strchr(buf,'\0');
  if((chp-buf)+UNIQnamelen+1+i>linebuf)
   { type=ft_TOOLONG;
     goto ret;
   }
  if(type==ft_DIR&&!forcedir)		  /* we've already checked this case */
     goto done;
  if(paranoid)
     memcpy(buf2,buf,i=lastdirsep(buf)-buf),buf2[i]='\0';
  mode=trymkdir(buf,paranoid!=0,i);
  if(!S_ISDIR(mode)||(type==ft_MAILDIR&&
   (forcedir=1,!mkmaildir(buf,chp,paranoid!=0))))
   { nlog("Unable to treat as directory");logqnl(buf);	 /* we can't make it */
     if(forcedir)				     /* fallback or give up? */
      { *chp='\0';skipped(buf);type=ft_CANTCREATE;
	goto ret;
      }
     if(!mode)
newfile:mode=S_IFREG|NORMperm&~cumask;
file:type=ft_FILE;
   }
done:
  if(paranoid)
     *paranoid=stbuf;
  else
     *modep=mode;
ret:
  return type;
}

			     /* lifted out of main() to reduce main()'s size */
int screenmailbox(chp,egid,Deliverymode)
 char*chp;const gid_t egid;const int Deliverymode;
{ char ch;struct stat stbuf;int basetype,type;
  /*
   *	  do we need sgidness to access the mail-spool directory/files?
   */
  accspooldir=3;	   /* assume we can write to the spool directory and */
  sgid=gid;	      /* that we don't need to setgid() to create a lockfile */
  strcpy(buf,chp);
  basetype=folderparse();			       /* strip off the type */
  if(buf[0]=='\0')				/* don't even bother with "" */
     return 0;
  ch= *(chp=lastdirsep(buf));
  if(chp>buf)
     *chp='\0';					   /* strip off the filename */
  if(!stat(buf,&stbuf))
   { unsigned wwsdir;
     accspooldir=(wwsdir=			/* world writable spool dir? */
	    ((S_IWGRP|S_IXGRP|S_IWOTH|S_IXOTH)&stbuf.st_mode)==
	     (S_IWGRP|S_IXGRP|S_IWOTH|S_IXOTH)
	  <<1|						 /* note it in bit 1 */
	  uid==stbuf.st_uid);	   /* we own the spool dir, note it in bit 0 */
     if((CAN_toggle_sgid||accspooldir)&&privileged)
	privileged=priv_DONTNEED;	     /* we don't need root to setgid */
     if(uid!=stbuf.st_uid&&		 /* we don't own the spool directory */
	(stbuf.st_mode&S_ISGID||!wwsdir))	  /* it's not world writable */
      { if(stbuf.st_gid==egid)			 /* but we have setgid privs */
	   doumask(GROUPW_UMASK);		   /* make it group-writable */
	goto keepgid;
      }
     else if(stbuf.st_mode&S_ISGID)
keepgid:			   /* keep the gid from the parent directory */
	if((sgid=stbuf.st_gid)!=egid&&		  /* we were started nosgid, */
	 setgid(sgid))				     /* but we might need it */
	   checkroot('g',(unsigned long)sgid);
   }
  else				/* panic, mail-spool directory not available */
   { setids();mkdir(buf,NORMdirperm);	     /* try creating the last member */
   }
  *chp=ch;
 /*
  *	  check if the default-mailbox-lockfile is owned by the
  *	  recipient, if not, mark it for further investigation, it
  *	  might need to be removed
  */
  chp=strchr(buf,'\0')-1;
  for(;;)				     /* what type of folder is this? */
   { type=foldertype(basetype,0,0,&stbuf);
     if(type==ft_NOTYET)
      { if(errno!=EACCES||(setids(),lstat(buf,&stbuf)))
	   goto nobox;
      }
     else if(!ft_checkcloser(type))
      { setids();
	if(type<0)
	   goto fishy;
	goto nl;					   /* no lock needed */
      }
    /*
     *	  check if the original/default mailbox of the recipient
     *	  exists, if it does, perform some security checks on it
     *	  (check if it's a regular file, check if it's owned by
     *	  the recipient), if something is wrong try and move the
     *	  bogus mailbox out of the way, create the
     *	  original/default mailbox file, and chown it to
     *	  the recipient
     */
     ;{ int checkiter=1;
	for(;;)
	 { if(stbuf.st_uid!=uid||		      /* recipient not owner */
	      !(stbuf.st_mode&S_IWUSR)||	     /* recipient can write? */
	      S_ISLNK(stbuf.st_mode)||			/* no symbolic links */
	      (S_ISDIR(stbuf.st_mode)?	      /* directories, yes, hardlinks */
		!(stbuf.st_mode&S_IXUSR):stbuf.st_nlink!=1))	       /* no */
	     /*
	      * If another procmail is about to create the new
	      * mailbox, and has just made the link, st_nlink==2
	      */
	      if(checkiter--)		    /* maybe it was a race condition */
		 suspend();		 /* close eyes, and hope it improves */
	      else			/* can't deliver to this contraption */
	       { int i=lastdirsep(buf)-buf;
		 memcpy(buf2,buf,i);buf2[i]='\0';
		 if(rnmbogus(buf,&stbuf,i,1))
		    goto fishy;
		 goto nobox;
	       }
	   else
	      break;
	   if(lstat(buf,&stbuf))
	      goto nobox;
	 }					/* SysV type autoforwarding? */
	if(Deliverymode&&(stbuf.st_mode&S_ISUID||
	 !S_ISDIR(stbuf.st_mode)&&stbuf.st_mode&S_ISGID))
	 { nlog("Autoforwarding mailbox found\n");
	   exit(EX_NOUSER);
	 }
	else
	 { if(!(stbuf.st_mode&OVERRIDE_MASK)&&
	      stbuf.st_mode&cumask&
	       (accspooldir?~(mode_t)0:~(S_IRGRP|S_IWGRP)))	/* hold back */
	    { static const char enfperm[]=
	       "Enforcing stricter permissions on";
	      nlog(enfperm);logqnl(buf);
	      syslog(LOG_NOTICE,slogstr,enfperm,buf);setids();
	      chmod(buf,stbuf.st_mode&=~cumask);
	    }
	   break;				  /* everything is just fine */
	 }
      }
nobox:
     if(!(accspooldir&1))	     /* recipient does not own the spool dir */
      { if(!xcreat(buf,NORMperm,(time_t*)0,doCHOWN|doCHECK))	   /* create */
	   break;		   /* mailbox... yes we could, fine, proceed */
	if(!lstat(buf,&stbuf))			     /* anything in the way? */
	   continue;			       /* check if it could be valid */
      }
     setids();						   /* try some magic */
     if(!xcreat(buf,NORMperm,(time_t*)0,doCHECK))		/* try again */
	break;
     if(lstat(buf,&stbuf))			      /* nothing in the way? */
fishy:
      { nlog("Couldn't create");logqnl(buf);
	return 0;
      }
   }
  if(!S_ISDIR(stbuf.st_mode))
   { int isgrpwrite=stbuf.st_mode&S_IWGRP;
     strcpy(chp=strchr(buf,'\0'),lockext);
     defdeflock=tstrdup(buf);
     if(!isgrpwrite&&!lstat(defdeflock,&stbuf)&&stbuf.st_uid!=uid&&
      stbuf.st_uid!=ROOT_uid)
      { int i=lastdirsep(buf)-buf;
	memcpy(buf2,buf,i);buf2[i]='\0';      /* try & rename bogus lockfile */
	rnmbogus(defdeflock,&stbuf,i,0);		   /* out of the way */
      }
     *chp='\0';
   }
  else
nl:  defdeflock=empty;					   /* no lock needed */
  return 1;
}
