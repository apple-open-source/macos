/************************************************************************
 *	Collection of library-worthy routines				*
 *									*
 *	Copyright (c) 1990-1998, S.R. van den Berg, The Netherlands	*
 *	Copyright (c) 1998-2001, Philip Guenther, The United States	*
 *					of America			*
 *	#include "../README"						*
 ************************************************************************/
#ifdef RCS
static /*const*/char rcsid[]=
 "$Id: goodies.c,v 1.74 2001/08/04 07:17:44 guenther Exp $";
#endif
#include "procmail.h"
#include "sublib.h"
#include "robust.h"
#include "shell.h"
#include "misc.h"
#include "pipes.h"
#include "common.h"
#include "acommon.h"
#include "cstdio.h"
#include "variables.h"
#include "goodies.h"

const char test[]="test";
const char*Tmnate,*All_args;

static const char*evalenv(skipping)	/* expects the variable name in buf2 */
 int skipping;
{ int j=buf2[0]-'0';
  return skipping?(const char*)0:	      /* speed this up when skipping */
	  (unsigned)j>9?getenv(buf2):
	  !j?argv0:
	   j<=crestarg?restargv[j-1]:(const char*)0;
}

#define NOTHING_YET	(-1)	 /* readparse understands a very complete    */
#define SKIPPING_SPACE	0	 /* subset of the standard /bin/sh syntax    */
#define NORMAL_TEXT	1	 /* that includes single-, double- and back- */
#define DOUBLE_QUOTED	2	 /* quotes, backslashes and $subtitutions    */
#define SINGLE_QUOTED	3

#define fgetc() (*fpgetc)()	   /* some compilers previously choked on it */
#define CHECKINC() (fencepost<p?(skipping|=1,p=fencepost):0)

/* sarg==0 : normal parsing, split up arguments like in /bin/sh
 * sarg==1 : environment assignment parsing, parse up till first whitespace
 * sarg==2 : normal parsing, split up arguments by existing whitespace
 */
int readparse(p,fpgetc,sarg,skipping)register char*p;int(*const fpgetc)();
 const int sarg;int skipping;
{ static int i,skipbracelev,bracegot;int got,bracelev,qbracelev;
  charNUM(num,long),*startb,*const fencepost=buf+linebuf,
     *const fencepost2=buf2+linebuf;
  static char*skipback;static const char*oldstartb;
  bracelev=qbracelev=0;All_args=0;
  if(skipping)skipping=2;	  /* bottom bit is whether overflow occurred */
  for(got=NOTHING_YET;;)		    /* buf2 is used as scratch space */
loop:
   { i=fgetc();
newchar:
     fencepost[1]='\0';CHECKINC();
     switch(i)
      { case EOF:	/* check sarg too to prevent warnings in the recipe- */
	   if(sarg<2&&got>NORMAL_TEXT)		 /* condition expansion code */
early_eof:    nlog(unexpeof);
ready:	   if(got!=SKIPPING_SPACE||sarg)  /* not terminated yet or sarg==2 ? */
	      *p++='\0';
	   Tmnate=p;
	   if(skipping&1)
	    { nlog(exceededlb);setoverflow();
	    }
	   return skipping&1;
	case '\\':
	   if(got==SINGLE_QUOTED)
	      break;
	   i=fgetc();
Quoted:	   switch(i)
	    { case EOF:
		 goto early_eof;			  /* can't quote EOF */
	      case '\n':
		 continue;				/* concatenate lines */
	      case '#':
		 if(got>SKIPPING_SPACE) /* escaped comment at start of word? */
		    goto noesc;			/* apparently not, literally */
	      case ' ':case '\t':case '\'':
		 if(got==DOUBLE_QUOTED)
		    goto noesc;
	      case '"':case '\\':case '$':case '`':
		 goto nodelim;
	      case '}':
		 if(got<=NORMAL_TEXT&&bracelev||
		    got==DOUBLE_QUOTED&&bracelev>qbracelev)
		    goto nodelim;
	    }
	   if(got>NORMAL_TEXT)
noesc:	      *p++='\\';		/* nothing to escape, just echo both */
	   break;
	case '`':
	   if(got==SINGLE_QUOTED)
	      goto nodelim;
	   for(startb=p;;)			       /* mark your position */
	    { switch(i=fgetc())			 /* copy till next backquote */
	       { case '"':
		    if(got!=DOUBLE_QUOTED)     /* missing closing backquote? */
		       break;
forcebquote:	 case EOF:case '`':
		    if(skipping)
		       *(p=startb)='\0';
		    else
		     { int osh=sh;
		       *p='\0';
		       if(!(sh=!!strpbrk(startb,shellmetas)))
			{ const char*save=sgetcp,*sAll_args;
			  sgetcp=p=tstrdup(startb);sAll_args=All_args;
			  if(readparse(startb,sgetc,0,0)	/* overflow? */
#ifndef GOT_bin_test
			   ||!strcmp(test,startb)      /* oops, `test' found */
#endif
			   )strcpy(startb,p),sh=1;
			  All_args=sAll_args;
			  free(p);sgetcp=save;		       /* chopped up */
			}	    /* drop source buffer, read from program */
		       startb=fromprog(
			p=startb,startb,(size_t)(buf-startb+linebuf-3));
		       sh=osh;				       /* restore sh */
		     }
		    if(got!=DOUBLE_QUOTED)
		     { i=0;startb=p;
		       goto simplsplit;			      /* split it up */
		     }
		    if(i=='"'||got<=SKIPPING_SPACE)   /* missing closing ` ? */
		       got=NORMAL_TEXT;
		    p=startb;
		    goto loop;
		 case '\\':
		    switch(i=fgetc())
		     { case EOF:nlog(unexpeof);
			  goto forcebquote;
		       case '\n':
			  continue;
		       case '"':
			  if(got!=DOUBLE_QUOTED)
			     break;
		       case '\\':case '$':case '`':
			  goto escaped;
		     }
		    *p++='\\';
	       }
escaped:      CHECKINC();*p++=i;
	    }
	case '"':
	   switch(got)
	    { case DOUBLE_QUOTED:
		 if(qbracelev<bracelev)		   /* still inside a ${...}? */
	      case SINGLE_QUOTED:
		    goto nodelim;				 /* nonsense */
		 got=NORMAL_TEXT;
		 continue;					/* closing " */
	    }
	   qbracelev=bracelev;got=DOUBLE_QUOTED;
	   continue;						/* opening " */
	case '\'':
	   switch(got)
	    { case DOUBLE_QUOTED:
		 goto nodelim;
	      case SINGLE_QUOTED:got=NORMAL_TEXT;
		 continue;					/* closing ' */
	    }
	   got=SINGLE_QUOTED;
	   continue;						/* opening ' */
	case '}':
	   if(got<=NORMAL_TEXT&&bracelev||
	      got==DOUBLE_QUOTED&&bracelev>qbracelev)
	    { bracelev--;
	      if(skipback&&bracelev==skipbracelev)
	       { skipping-=2;p=skipback;skipback=0;startb=(char*)oldstartb;
		 got=bracegot;
		 goto closebrace;
	       }
	      continue;
	    }
	   goto nodelim;
	case '#':
	   if(got>SKIPPING_SPACE)		/* comment at start of word? */
	      break;
	   while((i=fgetc())!=EOF&&i!='\n');		    /* skip till EOL */
	   goto ready;
	case '$':
	   if(got==SINGLE_QUOTED)
	      break;
	   startb=buf2;
	   switch(i=fgetc())
	    { case EOF:*p++='$';got=NORMAL_TEXT;
		 goto ready;
	      case '@':
		 if(got!=DOUBLE_QUOTED)
		    goto normchar;
		 if(!skipping)	      /* don't do it while skipping (braces) */
		    All_args=p;
		 continue;
	      case '{':						  /* ${name} */
		 while(EOF!=(i=fgetc())&&alphanum(i))
		  { if(startb>=fencepost2)
		       startb=buf2+2,skipping|=1;
		    *startb++=i;
		  }
		 *startb='\0';
		 if(numeric(*buf2)&&buf2[1])
		    goto badsub;
		 startb=(char*)evalenv(skipping);
		 switch(i)
		  { default:
		       goto badsub;
		    case ':':
		       switch(i=fgetc())
			{ case '-':
			     if(startb&&*startb)
				goto noalt;
			     goto doalt;
			  case '+':
			     if(startb&&*startb)
				goto doalt;
			     goto noalt;
			  default:
badsub:			     nlog("Bad substitution of");logqnl(buf2);
			     continue;
			}
		    case '+':
		       if(startb)
			  goto doalt;
		       goto noalt;
		    case '-':
		       if(startb)
noalt:			  if(!skipping)
			   { skipping+=2;skipback=p;skipbracelev=bracelev;
			     oldstartb=startb;bracegot=got;
			   }
doalt:		       bracelev++;
		       continue;
#if 0
		    case '%':	  /* this is where processing of ${var%%pat} */
		    case '#':			/* and friends would/will go */
#endif
		    case '}':
closebrace:	       if(!startb)
			  startb=(char*)empty;
		       break;
		  }
		 goto ibreak;					  /* $$ =pid */
	      case '$':ultstr(0,(unsigned long)thepid,startb=num);
		 goto ieofstr;
	      case '?':ltstr(0,(long)lexitcode,startb=num);
		 goto ieofstr;
	      case '#':ultstr(0,(unsigned long)crestarg,startb=num);
		 goto ieofstr;
	      case '=':ltstr(0,lastscore,startb=num);
ieofstr:	 i='\0';
		 goto copyit;
	      case '_':startb=incnamed?incnamed->ename:(char*)empty;
		 goto ibreak;
	      case '-':startb=(char*)tgetenv(lastfolder); /* $- =$LASTFOLDER */
ibreak:		 i='\0';
		 break;
	      default:
	       { int quoted=0;
		 if(numeric(i))			   /* $n positional argument */
		  { *startb++=i;i='\0';
		    goto finsb;
		  }
		 if(i=='\\')
		    quoted=1,i=fgetc();
		 if(alphanum(i))				    /* $name */
		  { do
		     { if(startb>=fencepost2)
			  startb=buf2+2,skipping|=1;
		       *startb++=i;
		     }
		    while(EOF!=(i=fgetc())&&alphanum(i));
		    if(i==EOF)
			i='\0';
finsb:		    *startb='\0';
		    if(!(startb=(char*)evalenv(skipping)))
		       startb=(char*)empty;
		    if(quoted)
		     { *p++='(';CHECKINC();	/* protect leading character */
		       *p++=')';
		       for(;CHECKINC(),*startb;*p++= *startb++)
			  if(strchr("(|)*?+.^$[\\",*startb))	/* specials? */
			     *p++='\\';		      /* take them literally */
normchar:	       quoted=0;
		     }
		    else
		       break;
		  }
		 else				       /* not a substitution */
		    *p++='$';			 /* pretend nothing happened */
		 if(got<=SKIPPING_SPACE)
		    got=NORMAL_TEXT;
		 if(quoted)
		    goto Quoted;
		 goto eeofstr;
	       }
	    }
	   if(got!=DOUBLE_QUOTED)
simplsplit: { char*q;
	      if(sarg)
		 goto copyit;
	      if(q=simplesplit(p,startb,fencepost,&got))     /* simply split */
		 p=q;				       /* it up in arguments */
	      else
		 skipping|=1,p=fencepost;
	    }
	   else
copyit:	    { size_t len=fencepost-p+1;
	      if(strlcpy(p,startb,len)>=len)		   /* simply copy it */
		 skipping|=1;			      /* did we truncate it? */
	      if(got<=SKIPPING_SPACE)		/* can only occur if sarg!=0 */
		 got=NORMAL_TEXT;
	      p=strchr(p,'\0');
	    }
eeofstr:   if(i)			     /* already read next character? */
	      goto newchar;
	   continue;
#if 0					      /* autodetect quoted specials? */
	case '~':
	   if(got==NORMAL_TEXT&&p[-1]!='='&&p[-1]!=':')
	      break;
	case '&':case '|':case '<':case '>':case ';':
	case '?':case '*':case '[':
	   if(got<=NORMAL_TEXT)
	      sh=1;
	   break;
#endif
	case ' ':case '\t':
	   switch(got)
	    { case NORMAL_TEXT:
		 if(sarg==1)
		    goto ready;		/* already fetched a single argument */
		 got=SKIPPING_SPACE;*p++=sarg?' ':'\0';	 /* space or \0 sep. */
	      case NOTHING_YET:case SKIPPING_SPACE:
		 continue;				       /* skip space */
	    }
	case '\n':
	   if(got<=NORMAL_TEXT)
	      goto ready;			    /* EOL means we're ready */
      }
nodelim:
     *p++=i;					   /* ah, a normal character */
     if(got<=SKIPPING_SPACE)		 /* should we bother to change mode? */
	got=NORMAL_TEXT;
   }
}

char*simplesplit(to,from,fencepost,gotp)char*to;const char*from,*fencepost;
 int*gotp;
{ register int got= *gotp;
  for(;to<=fencepost;from++)
   { switch(*from)
      { case ' ':case '\t':case '\n':
	   if(got>SKIPPING_SPACE)
	      *to++='\0',got=SKIPPING_SPACE;
	   continue;
	case '\0':
	   goto ret;
      }
     *to++= *from;got=NORMAL_TEXT;
   }
  to=0;
ret:
  *gotp=got;
  return to;
}

void concatenate(p)register char*p;
{ while(p!=Tmnate)			  /* concatenate all other arguments */
   { while(*p)
	p++;
     *p++=' ';
   }
  *p=p[-1]='\0';
}

void metaparse(p)const char*p;				    /* result in buf */
{ if(sh=!!strpbrk(p,shellmetas))
     strcpy(buf,p);			 /* copy literally, shell will parse */
  else
   { sgetcp=p=tstrdup(p);
     if(readparse(buf,sgetc,0,0)			/* parse it yourself */
#ifndef GOT_bin_test
	||!strcmp(test,buf)
#endif
	)
	strcpy(buf,p),sh=1;		   /* oops, overflow or `test' found */
     free((char*)p);
   }
}

void ltstr(minwidth,val,dest)const int minwidth;const long val;char*dest;
{ if(val<0)
   { *dest=' ';ultstr(minwidth-1,-val,dest+1);
     while(*++dest==' ');		     /* look for the first non-space */
     dest[-1]='-';				  /* replace it with a minus */
   }
  else
     ultstr(minwidth,val,dest);				/* business as usual */
}

#ifdef NOstrtod
double strtod(str,ptr)const char*str;char**const ptr;
{ int sign,any;unsigned i;char*chp;double acc,fracc;
  fracc=1;acc=any=sign=0;
  switch(*(chp=skpspace(str)))					 /* the sign */
   { case '-':sign=1;
     case '+':chp++;
   }
  while((i=(unsigned)*chp++-'0')<=9)		 /* before the decimal point */
     acc=acc*10+i,any=1;
  switch(i)
   { case (unsigned)'.'-'0':case (unsigned)','-'0':
	while(fracc/=10,(i=(unsigned)*chp++-'0')<=9)  /* the fractional part */
	   acc+=fracc*i,any=1;
   }
  if(ptr)
     *ptr=any?chp-1:(char**)str;
  return sign?-acc:acc;
}
#endif
