/************************************************************************
 *	Environment and variable handling routines used by procmail	*
 *									*
 *	Copyright (c) 1990-1999, S.R. van den Berg, The Netherlands	*
 *	Copyright (c) 2000-2001, Philip Guenther, The United States	*
 *							of America	*
 *	#include "../README"						*
 ************************************************************************/
#ifdef RCS
static /*const*/char rcsid[]=
 "$Id: variables.c,v 1.22 2001/08/27 08:53:15 guenther Exp $";
#endif
#include "procmail.h"
#include "acommon.h"		/* for hostname() */
#include "common.h"		/* for ultstr() */
#include "cstdio.h"
#include "robust.h"
#include "shell.h"
#include "authenticate.h"
#include "goodies.h"
#include "misc.h"
#include "locking.h"		/* for lockit() */
#include "comsat.h"
#include "sublib.h"
#include "variables.h"

struct varval strenvvar[]={{"LOCKSLEEP",DEFlocksleep},
 {"LOCKTIMEOUT",DEFlocktimeout},{"SUSPEND",DEFsuspend},
 {"NORESRETRY",DEFnoresretry},{"TIMEOUT",DEFtimeout},{"VERBOSE",DEFverbose},
 {"LOGABSTRACT",DEFlogabstract}};
struct varstr strenstr[]={{"SHELLMETAS",DEFshellmetas},{"LOCKEXT",DEFlockext},
 {"MSGPREFIX",DEFmsgprefix},{"TRAP",empty},
 {"SHELLFLAGS",DEFshellflags},{"DEFAULT",DEFdefault},{"SENDMAIL",DEFsendmail},
 {"SENDMAILFLAGS",DEFflagsendmail},{"PROCMAIL_VERSION",PM_VERSION}};

#define MAXvarvals	 maxindex(strenvvar)
#define MAXvarstrs	 maxindex(strenstr)

const char lastfolder[]="LASTFOLDER",maildir[]="MAILDIR",scomsat[]="COMSAT",
 offvalue[]="no";
int didchd;
long Stdfilled;
char*Stdout;

static void asenvtext P((const char*const chp));      /* needed by retStdout */

static const char slinebuf[]="LINEBUF",pmoverflow[]="PROCMAIL_OVERFLOW=yes",
 exitcode[]="EXITCODE";
static int setxit;

static struct dynstring*myenv;
static char**lastenv;
			      /* smart putenv, the way it was supposed to be */
const char*sputenv(a)const char*const a;
{ static int alloced;size_t eq,i;int remove;const char*split;char**preenv;
  struct dynstring*curr,**last;
  yell("Assigning",a);remove=0;
  if(!(split=strchr(a,'=')))			   /* assignment or removal? */
     remove=1,split=strchr(a,'\0');
  eq=split-a;							    /* is it */
  for(curr= *(last= &myenv);curr;curr= *(last= &curr->enext))  /* one I made */
     if(!strncmp(a,curr->ename,eq)&&((char*)curr->ename)[eq]=='=')
      { split=curr->ename;*last=curr->enext;free(curr);		 /* earlier? */
	for(preenv=environ;*preenv!=split;preenv++);
	goto wipenv;
      }
  for(preenv=environ;*preenv;preenv++)		    /* is it in the standard */
     if(!strncmp(a,*preenv,eq)&&(*preenv)[eq]=='=')	     /* environment? */
wipenv:
      { while(*preenv=preenv[1])   /* wipe this entry out of the environment */
	   preenv++;
	break;
      }
  i=(preenv-environ+2)*sizeof*environ;
  if(alloced)		   /* have we ever alloced the environ array before? */
     environ=realloc(environ,i);
  else
     alloced=1,environ=tmemmove(malloc(i),environ,i-sizeof*environ);
  if(!remove)		  /* if not remove, then add it to both environments */
   { for(preenv=environ;*preenv;preenv++);
     preenv[1]=0;*(lastenv=preenv)=(char*)(split=newdynstring(&myenv,a));
     return split+eq+1;
   }
  return empty;
}
	   /* between calling primeStdout() and retStdout() *no* environment */
void primeStdout(varname)const char*const varname;   /* changes are allowed! */
{ if(!Stdout)
     sputenv(varname);
  Stdout=(char*)myenv;
  Stdfilled=ioffsetof(struct dynstring,ename[0])+strlen(varname);
}

void retStdout(newmyenv,fail,unset)		/* see note on primeStdout() */
 char*const newmyenv;const int fail,unset;
{ char*var,*p;
  if(fail&&unset)				     /* on second thought... */
   { myenv=((struct dynstring*)newmyenv)->enext;	 /* pull it back out */
     free(newmyenv);*lastenv=Stdout=0;
     return;
   }
  else if(!fail&&newmyenv[Stdfilled-1]=='\n')  /* strip one trailing newline */
     Stdfilled--;
  retbStdout(newmyenv);
  var=newmyenv+ioffsetof(struct dynstring,ename[0]);	    /* setup to copy */
  p=strchr(var,'=');			       /* the variable name into buf */
  tmemmove(buf,var,p-var);			     /* so that we can check */
  buf[p-var]='\0';						/* for magic */
  if(fail)
     asenvtext(p+1);	  /* we always have to update the pointers for these */
  else
     asenv(p+1);					 /* invoke any magic */
}

void retbStdout(newmyenv)char*const newmyenv;	/* see note on primeStdout() */
{ newmyenv[Stdfilled]='\0';*lastenv=(myenv=(struct dynstring*)newmyenv)->ename;
  Stdout=0;
}

		 /* Append a space and then `value' to the last variable set */
void appendlastvar(value)const char*const value;
{ size_t len;char*p;
  Stdout=(char*)value;primeStdout(empty);
  len=Stdfilled+strlen(Stdout+Stdfilled);	     /* Skip over the header */
  p=realloc(Stdout,(Stdfilled=len+1+strlen(value))+1);
  p[len]=' ';strcpy(p+len+1,buf);retbStdout(p);	  /* WARNING: no magic here! */
}

const char*eputenv(src,dst)const char*const src;char*const dst;
{ sgetcp=src;
  return readparse(dst,sgetc,2,0)?0:sputenv(buf);
}

void setdef(name,value)const char*const name,*const value;
{ char*p;
  strcpy(buf,name);				 /* insert the variable name */
  p=strchr(buf,'\0');					   /* (find the end) */
  *p++='=';						       /* then the = */
  eputenv(value,p);			/* expand the value and call sputenv */
}

const char*tgetenv(a)const char*const a;
{ const char*b;
  return (b=getenv(a))?b:empty;
}

void setoverflow P((void))
{ sputenv(pmoverflow);
}

void cleanupenv(preserve)int preserve;
{ static const char*const keepenv[]=KEEPENV,*const ld_[]=LDENV;
  const char**emax=(const char**)environ,**ep,*const*pp;
  register const char*p;
  size_t len;
  if(!preserve)					     /* drop the environment */
   { for(pp=keepenv;*pp;pp++)			     /* preserve a happy few */
      { len=strlen(*pp);
	for(ep=emax;p= *ep;ep++)		     /* scan for this keeper */
	   if(!strncmp(*pp,p,len)&&(p[len]=='='||p[len-1]=='_'))
	    { *ep= *emax;			      /* it's fine, swap 'em */
	      *emax++=p;
	      if(p[len-1]!='_')		  /* if this wasn't a wildcard match */
		break;			 /* then go on to next keepenv entry */
	    }
      }
     *emax=0;						    /* drop the rest */
   }
  else
   { while(*emax)			  /* find the end of the environment */
	emax++;
   }
  ep=(const char**)environ;
  while(*ep)					   /* check for evil entries */
   { p=strchr(*ep,'=');
     if(!p)					      /* malformed (no '=')? */
drop: { *ep= *--emax;*emax=0;				/* copy from the end */
	continue;				  /* check the swapped entry */
      }
     len=p-*ep+1;			 /* mark how long the actual name is */
     for(pp=(const char*const*)environ;pp!=ep;pp++)	 /* duplicate entry? */
	if(!strncmp(*ep,*pp,len))
	   goto drop;
     for(pp=ld_;p= *pp;pp++)	       /* does it start with LD_ or similar? */
	if(!strncmp(*ep,p,strlen(p)))
	   goto drop;
     ep++;
   }
}

void initdefenv(pass,fallback,do_presets)auth_identity*pass;
 const char*fallback;int do_presets;
{ const char*p;
  if(pass)
   { p=auth_username(pass);
     if(!p||!*p)
	p=fallback;
     setdef(lgname,p);
     p=auth_shell(pass);
     if(!p||!*p)
	p=binsh;
     setdef(shell,p);
     setdef(home,auth_homedir(pass));setdef(orgmail,auth_mailboxname(pass));
   }
  else
   { setdef(lgname,fallback);setdef(shell,binsh);
     setdef(home,ROOT_DIR);setdef(orgmail,DEAD_LETTER);
   }
  setlgcs(tgetenv(lgname));		  /* make sure sendcomsat has a copy */
  if(do_presets)
   { static const char*const prestenv[]=PRESTENV;
     const char*const*pp;
     int i=MAXvarstrs;
     do	   /* initialise all non-empty string variables into the environment */
	if(*strenstr[i].sval)
	   setdef(strenstr[i].sname,strenstr[i].sval);
     while(i--);
     setdef(host,hostname());		       /* the other standard presets */
     sputenv(lastfolder);
     sputenv(exitcode);
     eputenv(defpath,buf);
     for(pp=prestenv;*pp;pp++)			     /* non-standard presets */
	eputenv(*pp,buf);
   }
}

int alphanum(c)const unsigned c;
{ switch(c)
   { case '0':case '1':case '2':case '3':case '4':
     case '5':case '6':case '7':case '8':case '9':
	return 2;
     case 'A':case 'B':case 'C':case 'D':case 'E':case 'F':case 'G':case 'H':
     case 'I':case 'J':case 'K':case 'L':case 'M':case 'N':case 'O':case 'P':
     case 'Q':case 'R':case 'S':case 'T':case 'U':case 'V':case 'W':case 'X':
     case 'Y':case 'Z':
     case 'a':case 'b':case 'c':case 'd':case 'e':case 'f':case 'g':case 'h':
     case 'i':case 'j':case 'k':case 'l':case 'm':case 'n':case 'o':case 'p':
     case 'q':case 'r':case 's':case 't':case 'u':case 'v':case 'w':case 'x':
     case 'y':case 'z':
     case '_':
	return 1;
     default:
	return 0;
   }
}

void setmaildir(newdir)const char*const newdir;		    /* destroys buf2 */
{ char*chp;
  didchd=1;*(chp=strcpy(buf2,maildir)+STRLEN(maildir))='=';
  strcpy(++chp,newdir);sputenv(buf2);
}

void setlastfolder(folder)const char*const folder;
{ char*chp;size_t len;
  setlfcs(folder);
  len=STRLEN(lastfolder)+2+strlen(folder);
  strcpy(chp=malloc(len),lastfolder);
  strlcat(chp,"=",len);
  strlcat(chp,folder,len);
  sputenv(chp);free(chp);
}

int setexitcode(trapisset)int trapisset;
{ char*p;int forceret;
  if(setxit&&(p=getenv(exitcode)))		 /* user specified exitcode? */
   { if((forceret=renvint(-2L,p))>=0)		     /* yes, is it positive? */
	retval=forceret;				 /* then override it */
   }
  else
   { forceret= -1;
     if(trapisset)		 /* no EXITCODE set, TRAP found, provide one */
      { p=buf2+STRLEN(exitcode);
	strcpy(buf2,exitcode);*p='=';
	ultstr(0,(unsigned long)retval,p+1);sputenv(buf2);
      }
   }
  return forceret;
}

char*gobenv(chp,end)char*chp,*end;
{ int found,i;
  found=0;end--;
  if(alphanum(i=getb())==1)
     for(found=1;*chp++=i,chp<end&&alphanum(i=getb()););
  *chp='\0';ungetb(i);
  if(chp==end)							 /* overflow */
   { nlog(exceededlb);setoverflow();
     return end+1;
   }
  switch(i)
   { case ' ':case '\t':case '\n':case '=':
	if(found)
	   return chp;
   }
  return 0;
}

int asenvcpy(src)char*src;
{ const char*chp;
  if(chp=strchr(src,'='))			     /* is it an assignment? */
    /*
     *	really change the uid now, since it would not be safe to
     *	evaluate the extra command line arguments otherwise
     */
   { size_t len=chp++-src+1;			      /* variable name + '=' */
     erestrict=1;setids();				   /* always do this */
     if(len>linebuf-XTRAlinebuf-1)			/* too long of name? */
      { setoverflow();
	nlog("Assignment to variable with excessively long name skipped\n");
      }
     else
      { memcpy(buf,src,len);
	src=buf+len;
	if(chp=eputenv(chp,src))
	 { src[-1]='\0';
	   asenv(chp);
	 }
      }
     return 1;
   }
  return 0;
}

void allocbuffers(lineb,setenv)size_t lineb;int setenv;
{ if(buf)
   { char*p=buf;
     buf=0;			    /* make sure buf is either valid or NULL */
     free(buf2);
     free(p);
   }
  buf=malloc(lineb+XTRAlinebuf);buf2=malloc(lineb+XTRAlinebuf);
  if(setenv)
   { char*chp;
     *(chp=strcpy(buf,slinebuf)+STRLEN(slinebuf))='=';
     ultstr(0,lineb,chp+1);
     sputenv(buf);
   }
}

static void asenvtext(chp)const char*const chp;
{ int i=MAXvarstrs;
  do						 /* several text assignments */
     if(!strcmp(buf,strenstr[i].sname))
	strenstr[i].sval=chp;
  while(i--);
}

void asenv(chp)const char*const chp;
{ static const char logfile[]="LOGFILE",Log[]="LOG",sdelivered[]="DELIVERED",
   includerc[]="INCLUDERC",eumask[]="UMASK",dropprivs[]="DROPPRIVS",
   shift[]="SHIFT",switchrc[]="SWITCHRC";
  if(!strcmp(buf,slinebuf))
   { long lineb;			 /* signed to catch negative numbers */
     if((lineb=renvint(0L,chp))<MINlinebuf)
	lineb=MINlinebuf;			       /* check minimum size */
     allocbuffers(linebuf=lineb,0);
   }
  else if(!strcmp(buf,maildir))
   { if(chdir(chp))
      { chderr(chp);
	setmaildir(curdir);
      }
     else
	didchd=1;
   }
  else if(!strcmp(buf,logfile))
     opnlog(chp);
  else if(!strcmp(buf,Log))
     elog(chp);
  else if(!strcmp(buf,exitcode))
     setxit=1;
  else if(!strcmp(buf,lgname))
     setlgcs(chp);
  else if(!strcmp(buf,lastfolder))
     setlfcs(chp);
  else if(!strcmp(buf,scomsat))
   { if(!setcomsat(chp))
	setdef(scomsat,offvalue);		/* set it to "no" on failure */
   }
  else if(!strcmp(buf,shift))
   { int i;
     if((i=renvint(0L,chp))>0)
      { if(i>crestarg)
	   i=crestarg;
	crestarg-=i;restargv+=i;		     /* shift away arguments */
      }
   }
  else if(!strcmp(buf,dropprivs))			  /* drop privileges */
   { if(renvint(0L,chp))
      { if(verbose)
	   nlog("Assuming identity of the recipient, VERBOSE=off\n");
	setids();
      }
   }
  else if(!strcmp(buf,sdelivered))			    /* fake delivery */
   { if(renvint(0L,chp))				    /* is it really? */
      { onguard();
	if((thepid=sfork())>0)
	   _exit(retvl2);			   /* parent: do not pass go */
	if(!forkerr(thepid,procmailn))
	   fakedelivery=1;
	newid();offguard();
      }
   }
  else if(!strcmp(buf,lockfile))
   { if(!lockit(tstrdup((char*)chp),&globlock))
	sputenv(lockfile);			      /* unset it on failure */
   }
  else if(!strcmp(buf,eumask))
     doumask((mode_t)strtol(chp,(char**)0,8));
  else if(!strcmp(buf,includerc))
   { if(rc>=0)				 /* INCLUDERC and SWITCHRC only work */
	pushrc(chp);					   /* inside rcfiles */
   }					      /* and not on the command line */
  else if(!strcmp(buf,switchrc))
   { if(rc>=0)
	changerc(chp);
   }
  else if(!strcmp(buf,host))
   { const char*name;
     if(strcmp(chp,name=hostname()))
      { yell("HOST mismatched",name);
	if(rc<0)				  /* if no rcfile opened yet */
	   retval=EXIT_SUCCESS,Terminate();	  /* exit gracefully as well */
	closerc();
      }
   }
  else
   { int i=MAXvarvals;
     do					      /* several numeric assignments */
	if(!strcmp(buf,strenvvar[i].name))
	   strenvvar[i].val=renvint(strenvvar[i].val,chp);
     while(i--);
     asenvtext(chp);			    /* delegate the text assignments */
   }
}

long renvint(i,env)const long i;const char*const env;
{ const char*p;long t;
  t=strtol(env,(char**)&p,10);			  /* parse like a decimal nr */
  if(p==env)
     for(;;p++)
      { switch(*p)
	 { case ' ':case '\t':case '\n':case '\v':case '\f':case '\r':
	      continue;				  /* skip leading whitespace */
	   case 'o':case 'O':
	      if(!strncasecmp(p+1,"n",1))
	   case 'y':case 'Y':case 't':case 'T':case 'e':case 'E':
		 t=1;
	      else if(!strncasecmp(p+1,"ff",2))
	   case 'n':case 'N':case 'f':case 'F':case 'd':case 'D':
		 t=0;
	      else
	   default:
		 t=i;
	      break;
	   case 'a':case 'A':t=2;
	      break;
	 }
	break;
      }
  return t;
}
