/************************************************************************
 *	LMTP (Local Mail Transfer Protocol) routines			*
 *									*
 *	Copyright (c) 1997-2001, Philip Guenther, The United States	*
 *							of America	*
 *	#include "../README"						*
 ************************************************************************/
#ifdef RCS
static /*const*/char rcsid[]=
 "$Id: lmtp.c,v 1.13 2001/06/28 21:44:28 guenther Exp $"
#endif
#include "procmail.h"
#ifdef LMTP
#include "sublib.h"
#include "robust.h"
#include "misc.h"
#include "shell.h"
#include "authenticate.h"
#include "cstdio.h"
#include "mailfold.h"
#include "memblk.h"
#include "from.h"
#include "lmtp.h"

/*

lhlo		=> print stuff
mail from:	=> fork
		   Generate From_ line (just body type)
rcpt to:	=> deduce pass entry, push it somewhere
data		=> process data till end, if we run out of mem, give 552
			(or 452?) response
bdat		=> allocate requested size buffer & read it in, or 552/452
			reponse.
vrfy		=> just check the user
end of data	=> fork, perform deliveries, giving error codes as you go.
rset		=> if in child, die.


	Should this simply be a _conformant_ implementation or should it
	be _strict_?  For example, after accepting a BDAT, should a DATA
	command cause LMTP to not accept anything but a RSET?  As it stands
	we'll be lenient and simply ignore invalid commands.  It's undefined
	behavior, so we can do what we want.
*/

#define INITIAL_RCPTS	10
#define INCR_RCPTS	20

static int lreaddyn P((void));

int childserverpid;

static ctopfd;
static char*overread;
static size_t overlen;

static int nliseol;			 /* is a plain \n the EOL delimiter? */
static char*bufcur;
static const char
 lhlomsg[]=
"250-localhost\r\n\
250-SIZE\r\n\
250-8BITMIME\r\n\
250-PIPELINING\r\n\
250-CHUNKING\r\n\
250 ENHANCEDSTATUSCODES\r\n",
 nomemmsg[]="452 4.3.0 insufficient system storage\r\n",
 quitmsg[]="221 2.3.0 Goodbye!\r\n",
 baduser[]="550 5.1.1 mailbox unknown\r\n",
 protogood[]="250 2.5.0 command succeeded\r\n",
 protobad[]="503 5.5.1 command unexpected\r\n";

static void bufwrite(buffer,len,flush)
const char*buffer;int len;int flush;
{ int already;
  if((already=bufcur-buf2)+len>linebuf||flush)
   { if(already&&already!=rwrite(savstdout,bufcur=buf2,already)||
      len&&len!=rwrite(savstdout,buffer,len))
	exit(EX_OSERR);
   }
  else
     bufcur=(char*)tmemmove(bufcur,buffer,len)+len;
}
#define bufinit		bufcur=buf2
#define skiptoeol	do c=getL(); while(c!='\n');  /* skip to end-o'-line */

static int unexpect(str)const char*str;
{ char c;
  while(*str!='\0')
   { if((c=getL())-'a'<='z'-'a'&&c>='a')c-='a'-'A';
     if(c!= *str)
      { if(c!='\n')
	   skiptoeol;
	return 1;
      }
     str++;
   }
  return 0;
}
#define NOTcommand(command,offset)	unexpect((msgcmd=command)+offset)

static long slurpnumber P((void))
{ int c;long total=0;		/* strtol would require buffering the number */
  while((c=getL())-'0'>=0&&c-'0'<10)
   { total*=10;
     total+=c-'0';
   }
  if(c!=' '&&c!='\n'&&c!='\r')
   { skiptoeol;
     return -1;
   }
  if(c=='\n')
   { ungetb(c);
   }
  return total;
}

/* Slurp "<.*?>" with rfc821 quoting rules, strip the trailing angle bracket */
/* This is stricter than it needs to be, but not as strict as the rfcs */
static char *slurpaddress P((void))
{ char*p=buf,c,*const last=buf+linebuf-1;     /* -1 to leave room for the \0 */
  do{*p=getL();}while(*p==' ');
  if(*p!='<')
   { if(*p!='\n')
	skiptoeol;
     return 0;
   }
  while(++p<last)
     switch(*p=getL())
      { case '\\':
	   if(++p==last)
	      goto syntax_error;
	   *p++=getL();
	   continue;
	case '"':
	   while(++p<last)
	      switch(*p=getL())
	       { case '\\':
		    if(++p==last)
		       goto syntax_error;
		    *p=getL();
		    continue;
		 case '"':
		    break;
		 case '\r':
			goto syntax_error;
		 case '\n':
		    return 0;
		 default:
		    continue;
	       }
	   continue;
	case '\00':case '\01':case '\02':case '\03':case '\04':
	case '\05':case '\06':case '\07':case '\10':case '\11':
		   case '\13':case '\14':case '\15':case '\16':
	case '\17':case '\20':case '\21':case '\22':case '\23':
	case '\24':case '\25':case '\26':case '\27':case '\30':
	case '\31':case '\32':case '\33':case '\34':case '\35':
	case '\36':case '\37':case '<':case '(':case ')':case ';':
syntax_error:
	   skiptoeol;
	case '\n':
	   return 0;
	case '>':
	   *p='\0';				   /* strip the trailing '>' */
	   return tstrdup(buf);
	default:
	   break;
      }
  goto syntax_error;
}

/* given a path from slurpaddress() extract the local-part and strip quoting */
static char *extractaddress(path)char *path;
{ char *p=path+1,*q=path;		       /* +1 to skip the leading '<' */
  if(*p=='@')		    /* route address.  Perhaps we should assume that */
     while(1)			/* sendmail will strip these for recipients? */
	switch(*++p)
	 { case ':':p++;       /* we can take some shortcuts because we know */
	      goto found;		/* the quoting and length to be okay */
	   case '\\':p++;
	      break;
	   case '"':
	      while(*++p!='"')
		 if(*p=='\\')
		    p++;
	      break;
	   case '>':case '\0':
	      free(path);				    /* no local part */
	      return 0;
	 }
found:
  while(1)						    /* strip quoting */
   { switch(*p)
      { case '\\':
	   p++;
	   break;
	case '"':
	   while(*++p!='"')
	      if(*p=='\\')
		 *q++= *++p;
	      else
		 *q++= *p;
	   p++;
	   continue;
	case '\0':case '>':	 /* no final host part?	 That's fine with us */
	case '@':
	   *q='\0';
	   return path;
      }
     *q++= *p++;
   }
}

/*
linebuf MUST be at least 256, and should be at least 1024 or so for buffering
*/

/* LMTP connection states */
#define S_START		0		/* lhlo => S_MAIL */
#define S_MAIL		1		/* mail => S_RCPT */
#define S_RCPT		2		/* data => S_MAIL, bdat => S_BDAT */
#define S_BDAT		3		/* bdat last => S_MAIL */

/* lmtp: run the LTMP protocol.	 It returns in children, never the
   parent.  The return value is the array of recipients, and into the
   first argument is stored a pointer to one past the end of that
   array.  the second argument should be the username of the person
   running procmail (this is used to generate the From_ line)  The
   returning child should handle the deliveries, calling lmtpresponse()
   with the exitcode of each one, then write 'overlen' bytes from
   'overread' to 'ctopfd', and exit.  If something unrecoverable goes
   wrong, and it can't do the necessary calls to lmtpresponse(), then
   it should exit with some non-zero status.  The parent will then
   syslog it, and exit with EX_SOFTWARE.  (See getL() in cstdio.c) */
struct auth_identity **lmtp(lrout,invoker)
struct auth_identity***lrout;char*invoker;
{ static const char cLHLO[]="LHLO ",cMAIL[]="MAIL FROM:",cRCPT[]="RCPT TO:",
   cDATA[]="DATA",cBDAT[]="BDAT",cRSET[]="RSET",cVRFY[]="VRFY ",cQUIT[]="QUIT",
   cNOOP[]="NOOP";
  const char*msg,*msgcmd;int flush=0,c,lmtp_state=S_START;long size=0;
  auth_identity**rcpts,**lastrcpt,**currcpt;

  pushfd(STDIN);overread=0;overlen=0;nliseol=1;
  bufinit;ctopfd=-1;					 /* setup our output */
  currcpt=rcpts=malloc(INITIAL_RCPTS*sizeof*rcpts);
  lastrcpt=INITIAL_RCPTS+currcpt;
  bufwrite("220 ",4,0);bufwrite(procmailn,strlen(procmailn),0);
  bufwrite(Version,strchr(Version,'\n')-Version,0);
  bufwrite(" LMTP\r\n",7,1);
  while(1)
   { do{c=getL();}while(c==' ');
     switch(c)
      { case 'l': case 'L':
	   if(NOTcommand(cLHLO,1))
	      goto unknown_command;
	   ;{ int sawcrnl=0;		      /* autodetect \r\n vs plain \n */
	      while((c=getL())!='\n')
		 if(c=='\r')
		  { c=getL();			      /* they lose on \r\r\n */
		    if(c=='\n')
		     { sawcrnl=1;
		       break;
		     }
		  }
	      flush=1;
	      if(lmtp_state!=S_START)
	       { msg=protobad;
		 goto message;
	       }
	      else
	       { lmtp_state=S_MAIL;
		 msg=lhlomsg;msgcmd=0;
		 if(sawcrnl)
		    nliseol=0;
	       }
	    }
	   goto message;
	case 'm': case 'M':
	   if(NOTcommand(cMAIL,1))
	      goto unknown_command;
	   ;{ int pipefds[2];char*from;
	      if(lmtp_state!=S_MAIL)
	       { skiptoeol;msg=protobad;
		 goto message;
	       }
	      if(!(from=slurpaddress()))
	       { msg="553 5.1.7 Unable to parse MAIL address\r\n";
		 goto message;
	       }
	      size=0;
	      goto jumpin;
	      do
	       { switch(c)
		  { case 's':case 'S':
		       if(unexpect("IZE="))			  /* rfc1653 */
			  goto unknown_param;
		       size=slurpnumber();
		       if(size<0)		/* will be zerod at loop top */
			  goto unknown_param;
		       break;
		    case 'b':case 'B':
		       if(unexpect("ODY="))			  /* rfc1652 */
			  goto unknown_param;
		       while((c=getL())!='\r')		      /* just ignore */
			  switch(c)		      /* the parameter as we */
			   { case ' ':goto jumpin;	/* can't do anything */
			     case '\n':goto jumpout;	   /* useful with it */
			   }
		    case '\r':
		       if((c=getL())=='\n')
			  continue;
		    default:
		       skiptoeol;
unknown_param:	       msg="504 5.5.4 unknown MAIL parameter or bad value\r\n";
		       goto message;
		  }
jumpin:		 do c=getL();
		 while(c==' ');
		}
	      while(c!='\n');
jumpout:      rpipe(pipefds);
	      /*
	       * This is a pipe on which to write back one byte which,
	       * if non-zero, indicates something went wrong and the
	       * parent should act like the MAIL FROM: never happened.
	       * If it was zero then it should be followed by any extra
	       * LMTP commands that the child read past what it needed.
	       */
	      if(!(childserverpid=sfork()))
	       { char status=0;
		 rclose(pipefds[0]);
		 ctopfd=pipefds[1];
		 bufwrite(0,0,1);	      /* free up buf2 for makeFrom() */
		 makeFrom(from+1,invoker);
		 /* bufinit;	only needed if buf2 might be realloced */
		 free(from);
		 if(size&&!resizeblock(&themail,size+=filled+3,1))/* try for */
		  { status=1;	      /* the memory now, +3 for the "." CRLF */
		    bufwrite(nomemmsg,STRLEN(nomemmsg),1);
		  }
		 if(rwrite(pipefds[1],&status,sizeof(status))!=sizeof(status))
		    exit(EX_OSERR);
		 if(status)
		    exit(0);
		 lmtp_state=S_RCPT;
		 msg=protogood;
		 goto message;
	       }
	      rclose(pipefds[1]);
	      if(!forkerr(childserverpid,buf))
	       { char status=1;
		 rread(pipefds[0],&status,sizeof(status));
		 if(!status)
		  { pushfd(pipefds[0]);		   /* pick up what the child */
		    lmtp_state=S_MAIL;			/* left lying around */
		    bufinit;
		  }
		 continue;				     /* restart loop */
	       }
	      rclose(pipefds[0]);
	      msg="421 4.3.2 unable to fork for MAIL\r\n";
	      goto message;
	    }
	case 'r': case 'R':
	   if((c=getL())=='s'||c=='S')
	    { if(NOTcommand(cRSET,2))
		 goto unknown_command;
	      skiptoeol;
	      if(lmtp_state!=S_START)
		 lmtp_state=S_MAIL;
	      msg=protogood;
	      goto message;
	    }
	   if((c!='c'&&c!='C')||NOTcommand(cRCPT,2))
	      goto unknown_command;
	   if(lmtp_state!=S_RCPT)
	    { skiptoeol;
	      msg=protobad;
	      /* don't change lmtp_state */
	      goto message;
	    }
	   if(currcpt==lastrcpt)		    /* do I need some space? */
	    { int num=lastrcpt-rcpts;
	      rcpts=realloc(rcpts,(num+INCR_RCPTS)*sizeof*rcpts);
	      currcpt=rcpts+num;lastrcpt=currcpt+INCR_RCPTS;
	    }
	   ;{ char *path,*mailbox;auth_identity*temp;
		    /* if it errors, extractaddress() will free its argument */
	      if(!(path=slurpaddress())||!(mailbox=extractaddress(path)))
	       { msg="550 5.1.3 address syntax error\r\n";
		 goto message;
	       }
/* if we were to handle ESMTP params on the RCPT verb, we would do so here */
	      skiptoeol;
	      if(!(temp=auth_finduser(mailbox,0)))
	       { msg="550 5.1.1 mailbox unknown\r\n";
		 free(path);
		 goto message;
	       }
	      auth_copyid(*currcpt=auth_newid(),temp);
	      free(path);
	      currcpt++;
	      msg="250 2.1.5 ok\r\n";
	      goto message;
	    }
	case 'd': case 'D':
	   flush=1;
	   if(NOTcommand(cDATA,1))
	      goto unknown_command;
	   skiptoeol;
	   if(lmtp_state!=S_RCPT)
	    { msg=protobad;
	      goto message;
	    }
	   if(currcpt==rcpts)
	    { msg="554 5.5.1 to whom?\r\n";
	      goto message;
	    }
	   msg="354 Enter DATA terminated with a solo \".\"\r\n";
	   bufwrite(msg,strlen(msg),1);
	   if(!(lreaddyn()))
	    { /*
	       * At this point we either have more data to read which we
	       * can't fit, or, worse, we've lost part of the command stream.
	       * The (ugly) solution/way out is to send the 452 status code
	       * and then kill both ourselves and out parent.  That's the
	       * only solution short of teaching lreaddyn() to take a small
	       * buffer (buf2?) and repeatedly fill it looking for the end
	       * of the data stream, but that's too ugly.  If the malloc
	       * failed then the machine is probably hurting enough that
	       * our exit can only help.
	       */
	      bufwrite(nomemmsg,STRLEN(nomemmsg),1);
	      goto quit;
	    }
deliver:   readmail(2,0L);		/* fix up things */
	   lastrcpt=rcpts;
	   rcpts=realloc(rcpts,(currcpt-rcpts)*sizeof*rcpts);
	   *lrout=(currcpt-lastrcpt)+rcpts;
	   return rcpts;
	case 'b': case 'B':		/* rfc1830's BDAT */
	   if(NOTcommand(cBDAT,1))
	      goto unknown_command;
	   if((c=getL())!=' ')
	    { if(c!='\n')
		 skiptoeol;
	      msg="504 5.5.4 octets count missing\r\n";
	      goto message;
	    }
	   if(lmtp_state<S_RCPT)
	    { msg=protobad;
	      goto message;
	    }
	   if(currcpt==rcpts)
	    { msg="554 5.5.1 to whom?\r\n";
	      goto message;
	    }
	   ;{ int last=0;
	      long length=slurpnumber();
	      if(length<0)
	       { msg="555 5.5.4 octet count bad\r\n";
		 goto message;
	       }
	      do{c=getL();}while(c==' ');
	      if(c=='l'||c=='L')
	       { if(unexpect("AST"))
		  {
bad_bdat_param:	    msg="504 5.5.4 parameter unknown\r\n";
		    goto message;
		  }
		 last=1;
		 c=getL();
	       }
	      if(!nliseol&&c=='\r')
		 c=getL();
	      if(c!='\n')
	       { skiptoeol;
		 goto bad_bdat_param;
	       }
	      if(filled+length>size)
	       { if(!resizeblock(&themail,size=filled+length+BLKSIZ,1))
		  { int i;				/* eat the BDAT data */
		    while(length>linebuf)
		     { i=readLe(buf,linebuf);
		       if(i<0)
			  goto quit;
		       length-=i;
		     }
		    if(length&&0>readLe(buf,length))
		       goto quit;
		    lmtp_state=S_MAIL;
		    msg=nomemmsg;
		    flush=1;
		    goto message;
		  }
	       }
	      while(length>0)
	       { int i=readLe(themail.p+filled,length);
		 if(!i)
		    exit(EX_NOINPUT);
		 else if(i<0)
		    goto quit;
		 length-=i;
		 filled+=i;
	       }
	      if(last)
	       { if(!nliseol)				/* change CRNL to NL */
		  { char*in,*out,*q,*last;
		    last=(in=out=themail.p)+filled;
		    while(in<last)
		       if((q=memchr(in,'\r',last-in))?q>in:!!(q=last))
			{ if(in!=out)
			     memmove(out,in,q-in);
			  out+=q-in;in=q;
			}
		       else if(++in==last||*in!='\n')	     /* keep the CR? */
			  *out++='\r';
		    resizeblock(&themail,(filled-=in-out)+1,1);
		  }
		 goto deliver;
	       }
	      msg=protogood;
	      goto message;
	    }
	case 'v': case 'V':
	   if(NOTcommand(cVRFY,1))
	      goto unknown_command;
	   flush=1;
	   ;{ char *path,*mailbox;
	      auth_identity *temp;
	      if(!(path=slurpaddress())||!(mailbox=extractaddress(path)))
	       { msg="501 5.1.3 address syntax error\r\n";
		 goto message;
	       }
	      skiptoeol;
	      if(!(temp=auth_finduser(mailbox,0)))
	       { msg="550 5.1.1 user unknown\r\n";
		 free(path);
		 goto message;
	       }
	      free(path);
	      msg="252 2.5.0 successful\r\n";
	      goto message;
	    }
	case 'q': case 'Q':
	   if(NOTcommand(cQUIT,1))
	      goto unknown_command;
quit:	   if(ctopfd>=0)	   /* we're the kid: tell the parent to quit */
	    { rwrite(ctopfd,cQUIT,STRLEN(cQUIT));
	      rclose(ctopfd);
	    }
	   else
	      bufwrite(quitmsg,STRLEN(quitmsg),1);
	   exit(0);
	case 'n': case 'N':
	   if(NOTcommand(cNOOP,1))
	      goto unknown_command;
	   skiptoeol;
	   flush=1;
	   msg="200 2.0.0 ? Nope\r\n";
	   goto message;
	default:
	   skiptoeol;
unknown_command:
	case '\n':
	   msg="500 5.5.1 Unknown command given\r\n";msgcmd=0;
	   flush=1;
	   break;
      }
message:
     bufwrite(msg,10,0);msg+=10;
     if(msgcmd)					  /* insert the command name */
      { msg--;
	bufwrite(msgcmd,4,0);
	msgcmd=0;
      }
     bufwrite(msg,strlen(msg),flush||endoread());
     flush=0;
   }
}

void flushoverread P(())		 /* pass upwards the extra LMTP data */
{ int i;
  while(overlen)
   { if(0>(i=rwrite(ctopfd,overread,overlen)))
	return;				       /* there's nothing to be done */
     overlen-=i;
     overread+=i;
   }
}

void freeoverread P(())			    /* blow away the extra LMTP data */
{ if(overread)
   { bbzero(overread,overlen);
     free(overread);
     overread=0;
   }
}

#define X(str)	{str,STRLEN(str)}
static struct{const char*mess;int len;}ret2LMTP[]=
{ X("500 5.0.0 usage error\r\n"),				    /* USAGE */
  X("501 5.6.0 data error\r\n"),				  /* DATAERR */
  X("550 5.3.0 input missing\r\n"),				  /* NOINPUT */
  X("550 5.1.1 no such user\r\n"),				   /* NOUSER */
  X("550 5.1.2 no such host\r\n"),				   /* NOHOST */
  X("554 5.0.0 something didn't work\r\n"),		      /* UNAVAILABLE */
  X("554 5.3.0 internal software error\r\n"),			 /* SOFTWARE */
  X("451 4.0.0 OS error\r\n"),					    /* OSERR */
  X("554 5.3.5 system file error\r\n"),				   /* OSFILE */
  X("550 5.0.0 output error\r\n"),				/* CANTCREAT */
  X("451 4.0.0 I/O error\r\n"),					    /* IOERR */
  X("450 4.0.0 deferred\r\n"),					 /* TEMPFAIL */
  X("554 5.5.0 protocol error\r\n"),				 /* PROTOCOL */
  X("550 5.0.0 insufficient permission\r\n"),			   /* NOPERM */
  X("554 5.3.5 configuration error\r\n"),			   /* CONFIG */
};
#undef X

void lmtpresponse(retcode)int retcode;
{ const char*message;int len;
  if(!retcode)
     message=protogood,len=STRLEN(protogood);
  else
   { if(retcode<0)
	retcode=EX_SOFTWARE;
     if(0>(retcode-=EX__BASE)||retcode>=(sizeof ret2LMTP/sizeof ret2LMTP[0]))
	retcode=EX_UNAVAILABLE-EX__BASE;
     message=ret2LMTP[retcode].mess;len=ret2LMTP[retcode].len;
   }
  if(len!=rwrite(savstdout,message,len))
     exit(EX_OSERR);
}

#define IS_READERR	(-1)
#define IS_NORMAL	0
#define IS_CR		1
#define IS_CRBOL	2
#define IS_CRDOT	3
#define IS_DOTCR	4
#define IS_NLBOL	5
#define IS_NLDOT	6

#define EXIT_LOOP(s)	{state=(s);goto loop_exit;}

static char*lmtp_read_crnl(char*p,long left,void*statep)
{ int got,state= *(int*)statep;
  register char*in,*q,*last;
  do
   { if(0>=(got=readL(p,left)))					/* read mail */
      { state=IS_READERR;
	return p;
      }
     last=(in=p)+got;
     /*
      * A state machine to read LMTP data.  If 'nliseol' isn't set,
      * then \r\n is the end-o'-line string, and \n is only special
      * in it.	\r's are stripped from \r\n, but are otherwise preserved.
      */
     switch(state)
      { case IS_CR:   goto is_cr;
	case IS_CRBOL:goto is_crbol;
	case IS_CRDOT:goto is_crdot;
	case IS_DOTCR:goto is_dotcr;
	case IS_NORMAL:break;
	case IS_NLBOL:case IS_NLDOT:case IS_READERR:
	   exit(EX_SOFTWARE);
      }
     while(in<last)
	if((q=memchr(in,'\r',last-in))?q>in:!!(q=last))
	 { if(in!=p)
	      memmove(p,in,q-in);
	   p+=q-in;in=q;
	 }
	else							       /* CR */
	 {
found_cr:  *p++= *in++;				   /* tenatively save the \r */
	   if(in==last)
	      EXIT_LOOP(IS_CR)
is_cr:	   if(*in!='\n')
	      continue;
	   p[-1]= *in++;				 /* overwrite the \r */
	   if(in==last)						     /* CRLF */
	      EXIT_LOOP(IS_CRBOL)
is_crbol:  if(*in=='\r')					 /* CRLF CR? */
	      goto found_cr;
	   if(*in!='.')
	    { *p++= *in++;
	      continue;
	    }
	   if(++in==last)					 /* CRLF "." */
	      EXIT_LOOP(IS_CRDOT)
is_crdot:  if((*p++= *in++)!='\r')
	      continue;
	   if(in==last)					      /* CRLF "." CR */
	      EXIT_LOOP(IS_DOTCR)
is_dotcr:  if(*in=='\n')				    /* CRLF "." CRLF */
	    { p--;				   /* remove the trailing \r */
	      if((overlen=last-++in)>0)		 /* should never be negative */
		 tmemmove(overread=malloc(overlen),in,overlen);
	      return p;
	    }
	 }
     state=IS_NORMAL;		 /* we must have fallen out because in==last */
loop_exit:
     got-=in-p;				     /* correct for what disappeared */
   }
  while(left-=got);				/* change listed buffer size */
  *(long*)statep=state;					       /* save state */
  return 0;
}

static char*lmtp_read_nl(char*p,long left,void*statep)
{ int got,state= *(int*)statep;
  register char*in,*q,*last;
  do
   { if(0>=(got=readL(p,left)))					/* read mail */
      { state=IS_READERR;
	return p;
      }
     last=(in=p)+got;
     /*
      * A state machine to read LMTP data.  \n is the end-o'-line
      * character and \r is not special at all.
      */
     switch(state)
      { case IS_CR:case IS_CRBOL:case IS_CRDOT:case IS_DOTCR:
	case IS_READERR:
	   exit(EX_SOFTWARE);
	case IS_NLBOL:goto is_nlbol;
	case IS_NLDOT:goto is_nldot;
	case IS_NORMAL:break;
      }
     while(in<last)
	if((q=memchr(in,'\n',last-in))?q>in:!!(q=last))
	 { if(in!=p)
	      memmove(p,in,q-in);
	   p+=q-in;in=q;
	 }
	else							       /* LF */
	 { do
	    { *p++= *in++;
is_nlbol:     ;
	    }
	   while(in<last&&*in=='\n');
	   if(in==last)
	      EXIT_LOOP(IS_NLBOL)
	   if(*in!='.')
	    { *p++= *in++;
	      continue;
	    }
	   if(++in==last)					   /* LF "." */
	      EXIT_LOOP(IS_NLDOT)
is_nldot:  if(*in=='\n')					/* LF "." LF */
	    { if((overlen=last-++in)>0)		 /* should never be negative */
		 tmemmove(overread=malloc(overlen),in,overlen);
	      return p;
	    }
	   *p++= *in++;
	 }
     state=IS_NORMAL;		 /* we must have fallen out because in==last */
loop_exit:
     got-=in-p;				     /* correct for what disappeared */
   }
  while(left-=got);				/* change listed buffer size */
  *(long*)statep=state;					       /* save state */
  return 0;
}

static int lreaddyn()
{ int state=nliseol?IS_NLBOL:IS_CRBOL;
  read2blk(&themail,&filled,nliseol?&lmtp_read_nl:&lmtp_read_crnl,
   (cleanup_func_type*)0,&state);
  return state!=IS_READERR;
}
#else
int lmtp_dummy_var;		      /* to prevent insanity in some linkers */
#endif
