/************************************************************************
 *	Miscellaneous routines used by formail				*
 *									*
 *	Copyright (c) 1990-1999, S.R. van den Berg, The Netherlands	*
 *	#include "../README"						*
 ************************************************************************/
#ifdef RCS
static /*const*/char rcsid[]=
 "$Id: formisc.c,v 1.41 2001/06/27 06:41:27 guenther Exp $";
#endif
#include "includes.h"
#include "formail.h"
#include "sublib.h"
#include "shell.h"
#include "common.h"
#include "acommon.h"
#include "ecommon.h"
#include "formisc.h"

static const char*skipcomment(start)const char*start;
{ for(;;)
     switch(*++start)
      { case '\0':start--;
	case ')':return start;
	case '\\':start++;
	   break;	     /* Prithee, breaking the 11th commandment here: */
	case '(':start=skipcomment(start);	 /* Thou shalt not re-curse! */
      }
}

char*skipwords(start)char*start;		 /* skips an RFC 822 address */
{ int delim,hitspc,machref,group;char*target,*oldstart;
  group=1;hitspc=machref=0;target=oldstart=start;
  if(*start=='<')
     start++,machref=1;
  for(;;)
   { switch(*start)
      { case '<':					/* machine reference */
	   if(machref)					 /* cannot be nested */
	    { target=oldstart;hitspc=0;			    /* so start over */
	      goto inc;
	    }
	   goto ret;
	case '(':start=(char*)skipcomment(start);		  /* comment */
	case ' ':case '\t':case '\n':hitspc|=1;	       /* linear white space */
inc:	   start++;
	   continue;
	case ';':
	   if(group==2)
	      start[1]='\0';			      /* terminate the group */
	case ',':
	   if(group==2)
	      goto special;				/* part of the group */
	   if(machref)		 /* allow embedded ,; in a machine reference */
	    { machref=2;
	      goto special;
	    }
	   goto retz;
	default:
	   if(!machref&&hitspc==3&&target>oldstart)
	case '\0':case '>':
	    { if(machref==2)	/* embedded ,; so you have to encapsulate it */
	       { *target++='>';tmemmove(oldstart+1,oldstart,target++-oldstart);
		 *oldstart='<';
	       }
retz:	      *target='\0';
ret:	      return start;
	    }
	   if(*start=='\\')
	      *target++='\\',start++;
	   hitspc=2;
	   goto normal;					      /* normal word */
	case ':':
	   if(group==1)
	      group=2;						/* groupies! */
	case '@':case '.':
	   if(group==1)
	      group=0;	   /* you had your chance, and you blew it, no group */
special:   hitspc=0;
normal:	   *target++= *start++;
	   continue;
	case '[':delim=']';				   /* domain-literal */
	   break;
	case '"':*target++=delim='"';start++;
      }
     ;{ int i;
	do
	   if((i= *target++= *start++)==delim)	 /* corresponding delimiter? */
	      break;
	   else if(i=='\\'&&*start)		    /* skip quoted character */
	      *target++= *start++;
	while(*start);						/* anything? */
      }
     hitspc=2;
   }
}

void loadsaved(sp)const struct saved*const sp;	     /* load some saved text */
{ switch(*sp->rexp)
   { default:loadchar(' ');	      /* make sure it has leading whitespace */
     case ' ':case '\t':;
   }
  loadbuf(sp->rexp,sp->rexl);
}
							    /* append to buf */
void loadbuf(text,len)const char*const text;const size_t len;
{ if(buffilled+len>buflen)			  /* buf can't hold the text */
     buf=realloc(buf,buflen+=Bsize);
  tmemmove(buf+buffilled,text,len);buffilled+=len;
}

void loadchar(c)const int c;		      /* append one character to buf */
{ if(buffilled==buflen)
     buf=realloc(buf,buflen+=Bsize);
  buf[buffilled++]=c;
}

int getline P((void))			   /* read a newline-terminated line */
{ if(buflast==EOF)			 /* at the end of our Latin already? */
   { loadchar('\n');					  /* fake empty line */
     return EOF;					  /* spread the word */
   }
  loadchar(buflast);			    /* load leftover into the buffer */
  if(buflast!='\n')
   { int ch;
     while((ch=getchar())!=EOF&&ch!='\n')
	rhash=rhash*67067L+(uchar)ch,loadchar(ch);  /* load rest of the line */
     loadchar('\n');		    /* make sure (!), it ends with a newline */
   }		/* (some code in formail.c depends on a terminating newline) */
  return buflast=getchar();			/* look ahead, one character */
}

void elog(a)const char*const a;				     /* error output */
{ fputs(a,stderr);
}

void tputssn(a,l)const char*a;size_t l;
{ while(l--)
     putcs(*a++);
}

void ltputssn(a,l)const char*a;size_t l;
{ if(logsummary)
     Totallen+=l;
  else
     putssn(a,l);
}

void lputcs(i)const int i;
{ if(logsummary)
     Totallen++;
  else
     putcs(i);
}

void startprog(argv)const char*Const*const argv;
{ if(nrskip)				  /* should we still skip this mail? */
   { nrskip--;opensink();return;				 /* count it */
   }
  if(nrtotal>0)
     nrtotal--;							 /* count it */
  dup(oldstdout);
  if(*argv)			    /* do we have to start a program at all? */
   { int poutfd[2];
     static int children;
     if(lenfileno>=0)
      { long val=initfileno++;char*chp;
	chp=ffileno+LEN_FILENO_VAR;
	if(val<0)
	   *chp++='-';
	ultstr(lenfileno-(val<0),val<0?-val:val,chp);
	while(*chp==' ')
	   *chp++='0';
      }
     pipe(poutfd);
     ;{ int maxchild=childlimit?childlimit:
	 (unsigned long)children*CHILD_FACTOR,excode;
	while(children&&waitpid((pid_t)-1,&excode,WNOHANG)>0)
	   if(!WIFSTOPPED(excode))		      /* collect any zombies */
	    { children--;
	      if((excode=WIFEXITED(excode)?
		  WEXITSTATUS(excode):-WTERMSIG(excode))!=EXIT_SUCCESS)
		 retval=excode;
	    }					       /* reap some children */
	while(childlimit&&children>=childlimit||(child=fork())==-1&&children)
	   for(--children;(excode=waitfor((pid_t)0))!=NO_PROCESS;)
	    { if(excode!=EXIT_SUCCESS)
		 retval=excode;
	      if(--children<=maxchild)
		 break;
	    }
      }
     if(!child)		     /* DON'T fclose(stdin), provokes a bug on HP/UX */
      { close(STDIN);close(oldstdout);close(PWRO);dup(PRDO);close(PRDO);
	shexec(argv);
      }
     close(STDOUT);close(PRDO);
     if(STDOUT!=dup(PWRO))
	nofild();
     close(PWRO);
     if(-1==child)
	nlog("Can't fork\n"),exit(EX_OSERR);
     children++;
   }
  if(!(mystdout=Fdopen(STDOUT,"a")))
     nofild();
}

void nofild P((void))
{ nlog("File table full\n");exit(EX_OSERR);
}

void nlog(a)const char*const a;
{ elog(formailn);elog(": ");elog(a);
}

void logqnl(a)const char*const a;
{ elog(" \"");elog(a);elog("\"\n");
}

void closemine P((void))
{ if(fclose(mystdout)==EOF||errout==EOF)
   { if(!quiet)
	nlog(couldntw),elog("\n");
     exit(EX_IOERR);
   }
}

void opensink P((void))
{ if(!(mystdout=fopen(DevNull,"a")))
     nofild();
}
