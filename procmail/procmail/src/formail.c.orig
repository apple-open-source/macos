/************************************************************************
 *	formail - The mail (re)formatter				*
 *									*
 *	Seems to be relatively bug free.				*
 *									*
 *	Copyright (c) 1990-2000, S.R. van den Berg, The Netherlands	*
 *	Copyright (c) 1999-2001, Philip Guenther, The United States	*
 *							of America	*
 *	#include "../README"						*
 ************************************************************************/
#ifdef RCS
static /*const*/char rcsid[]=
 "$Id: formail.c,v 1.102 2001/08/04 07:07:43 guenther Exp $";
#endif
static /*const*/char rcsdate[]="$Date: 2001/08/04 07:07:43 $";
#include "includes.h"
#include <ctype.h>		/* iscntrl() */
#include "formail.h"
#include "acommon.h"
#include "sublib.h"
#include "shell.h"
#include "common.h"
#include "fields.h"
#include "ecommon.h"
#include "formisc.h"
#include "../patchlevel.h"

#define ssl(str)		str,STRLEN(str)
#define bsl(str)		{ssl(str)}
#define sslbar(str,bar1,bar2)	{ssl(str),STRLEN(bar1)-1,STRLEN(bar2)-1}

static const char
#define X(name,value)	name[]=value,
#include "header.h"				  /* pull in the definitions */
#undef X
 From_[]=		FROM,				/* VNIX 'From ' line */
 Article_[]=		"Article ",		   /* USENET 'Article ' line */
 x_[]=			"X-",				/* general extension */
 old_[]=		OLD_PREFIX,			     /* my extension */
 xloop[]=		"X-Loop:",				/* ditto ... */
 Resent_[]=		"Resent-",	   /* for tweaking reply preferences */
 mdaemon[]="<>",unknown[]=UNKNOWN,re[]=" Re:",fmusage[]=FM_USAGE;

static const struct {const char*hedr;int lnr;}cdigest[]=
{
#define X(name,value)	bsl(name),
#include "header.h"		     /* pull in the precalculated references */
#undef X
};

/*
 *	sender determination fields in order of importance/reliability
 *	reply-address determination fields (wrepl specifies the weight
 *	for header replies and wrrepl specifies the weight for header
 *	replies where Resent- header are used, while the position in the
 *	table index specifies the weight for envelope replies and From_
 *	line creation.
 *
 *	I bet this is the first time you've seen a bar graph in
 *	C-source-code :-)
 */
static const struct {const char*head;int len,wrepl,wrrepl;}sest[]=
{ sslbar(replyto	,"*********"	,"********"	),
  sslbar(Fromm		,"**foo***"	,"**bar**"	),
  sslbar(sender		,"*******"	,"******"	),
  sslbar(res_replyto	,"*"		,"***********"	),
  sslbar(res_from	,"*"		,"**********"	),
  sslbar(res_sender	,"*"		,"*********"	),
  sslbar(path		,"**"		,"*"		),
  sslbar(retreceiptto	,"***"		,"**"		),
  sslbar(errorsto	,"****"		,"***"		),
  sslbar(returnpath	,"******"	,"*****"	),
  sslbar(From_		,"*****"	,"****"		),
};

static struct saved rex[]=
{ bsl(subject),bsl(references),bsl(messageid),bsl(date)
};
#define subj	(rex+0)
#define refr	(rex+1)
#define msid	(rex+2)
#define hdate	(rex+3)

#ifdef sMAILBOX_SEPARATOR
#define emboxsep	smboxsep
#define MAILBOX_SEPARATOR
static const char smboxsep[]=sMAILBOX_SEPARATOR;
#endif /* sMAILBOX_SEPARATOR */
#ifdef eMAILBOX_SEPARATOR
#ifdef emboxsep
#undef emboxsep
#else
#define MAILBOX_SEPARATOR
#endif
static const char emboxsep[]=eMAILBOX_SEPARATOR;
#endif /* eMAILBOX_SEPARATOR */

const char binsh[]=BinSh,sfolder[]=FOLDER,
 couldntw[]="Couldn't write to stdout",formailn[]=FORMAILN;
int errout,oldstdout,quiet=1,zap,buflast,lenfileno;
long initfileno;
char ffileno[LEN_FILENO_VAR+8*sizeof(initfileno)*4/10+1+1]=DEFfileno;
int lexitcode;					     /* dummy, for waitfor() */
pid_t child= -1;
int childlimit;
unsigned long rhash;
FILE*mystdout;
int nrskip,nrtotal= -1,retval=EXIT_SUCCESS;
size_t buflen,buffilled;
long Totallen;
char*buf,*logsummary;
struct field*rdheader,*xheader,*Xheader,*uheader,*Uheader;
static struct field*iheader,*Iheader,*aheader,*Aheader,*Rheader,*nheader;
static int areply;

static void logfolder P((void))	 /* estimate the no. of characters needed to */
{ size_t i;charNUM(num,Totallen);		       /* represent Totallen */
  static const char tabchar[]=TABCHAR;
  if(logsummary)
   { putssn(sfolder,STRLEN(sfolder));putssn(logsummary,i=strlen(logsummary));
     i+=STRLEN(sfolder);i-=i%TABWIDTH;
     do putssn(tabchar,STRLEN(tabchar));
     while((i+=TABWIDTH)<LENoffset);
     ultstr(7,Totallen,num);putssn(num,strlen(num));putcs('\n');
   }
}

static void renfield(pointer,oldl,newname,newl)struct field**const pointer;
 size_t oldl;const size_t newl;const char*const newname;    /* rename fields */
{ struct field*p;size_t i;char*chp;
  p= *pointer;(chp=p->fld_text)[p->Tot_len-1]='\0';
  if(eqFrom_(chp))				       /* continued From_ to */
     for(;chp=strstr(chp,"\n>");*++chp=' ');	  /* continued regular field */
  if(newl==STRLEN(From_)&&eqFrom_(newname))
   { for(chp=p->fld_text;chp=strchr(chp,'\n');)		/* continued regular */
	if(*++chp==' '||*chp=='\t')		 /* to continued From_ field */
	   *chp='>';
     for(chp=p->fld_text;chp=strstr(chp,"\n ");*++chp='>');
     goto replaceall;
   }
  if(newname[newl-1]==HEAD_DELIMITER)		     /* completely new field */
replaceall:
     oldl=p->id_len;			     /* replace the old one entirely */
  p->id_len+=(int)newl-(int)oldl;p->fld_text[p->Tot_len-1]='\n';
  p->Tot_len=(i=p->Tot_len-oldl)+newl;
  if(newl>oldl)
     *pointer=p=realloc(p,FLD_HEADSIZ+p->Tot_len);
  chp=p->fld_text;tmemmove(chp+newl,chp+oldl,i);tmemmove(chp,newname,newl);
}

static void procfields(sareply)const int sareply;
{ struct field*fldp,**afldp;
  fldp= *(afldp= &rdheader);
  while(fldp)
   { struct field*fp2;
     if(!sareply&&
	(fp2=findf(fldp,&iheader))&&
	!(areply&&fldp->id_len>=fp2->Tot_len-1))      /* filled replacement? */
      { renfield(afldp,(size_t)0,old_,STRLEN(old_));	/* implicitly rename */
	goto fixfldp;
      }
     if((fp2=findf(fldp,&Iheader))&&			    /* delete fields */
	!(sareply&&fldp->id_len<fp2->Tot_len-1))       /* empty replacement? */
	goto delfld;
     if(fp2=findf(fldp,&Rheader))		  /* explicitly rename field */
      { renfield(afldp,fp2->id_len,(char*)fp2->fld_text+fp2->id_len,
	 fp2->Tot_len-fp2->id_len);
fixfldp:
	fldp= *afldp;
      }
     ;{ struct field*uf;
	if((uf=findf(fldp,&uheader))&&!uf->fld_ref)
	   uf->fld_ref=afldp;			   /* first uheader, keep it */
	else if(fp2=findf(fldp,&Uheader))
	 { if(fp2->fld_ref)
	    { struct field**ch_afldp;
	      if(afldp==(ch_afldp= &(*fp2->fld_ref)->fld_next))
		 afldp=fp2->fld_ref;		   /* deleting own reference */
	      for(fldp=Uheader;fldp;fldp=fldp->fld_next)
		 if(fldp->fld_ref==ch_afldp)	  /* rearrange references to */
		    fldp->fld_ref=fp2->fld_ref;		  /* vanishing field */
	      delfield(fp2->fld_ref);		       /* delete old Uheader */
	    }
	   fp2->fld_ref=afldp;				/* keep last Uheader */
	 }
	else if(uf)			    /* delete all following uheaders */
delfld:	 { fldp=delfield(afldp);
	   continue;
	 }
      }
     fldp= *(afldp= &(*afldp)->fld_next);
   }
}
    /* checks if the last field in rdheader looks like a known digest header */
static int digheadr P((void))
{ char*chp;int i;size_t j;struct field*fp;
  for(fp=rdheader;fp->fld_next;fp=fp->fld_next);	 /* skip to the last */
  i=maxindex(cdigest);chp=fp->fld_text;j=fp->id_len;
  while(chp[j-2]==' '||chp[j-2]=='\t')	     /* whitespace before the colon? */
     j--;
  while((cdigest[i].lnr!=j||strncasecmp(cdigest[i].hedr,chp,j-1))&&i--);
  return i>=0||j>STRLEN(old_)&&!strncasecmp(old_,chp,STRLEN(old_))||
   j>STRLEN(x_)&&!strncasecmp(x_,chp,STRLEN(x_));
}

static int artheadr P((void))	     /* could it be the start of an article? */
{ if(!rdheader&&!strncmp(buf,Article_,STRLEN(Article_)))
   { addbuf();rdheader->id_len=STRLEN(Article_);
     return 1;
   }
  return 0;
}
			     /* lifted out of main() to reduce main()'s size */
static char*getsender(namep,fldp,headreply)char*namep;struct field*fldp;
 const int headreply;
{ char*chp;int i,nowm;size_t j;static int lastm;
  chp=fldp->fld_text;j=fldp->id_len;i=maxindex(sest);
  while((sest[i].len!=j||strncasecmp(sest[i].head,chp,j))&&i--);
  if(i>=0&&(i!=maxindex(sest)||fldp==rdheader))		  /* found anything? */
   { char*saddr;char*tmp;			     /* determine the weight */
     nowm=areply&&headreply?headreply==1?sest[i].wrepl:sest[i].wrrepl:i;chp+=j;
     tmp=malloc(j=fldp->Tot_len-j);tmemmove(tmp,chp,j);(chp=tmp)[j-1]='\0';
     if(sest[i].head==From_)
      { char*pastad;
	if(strchr(saddr=chp,'\n'))		     /* multiple From_ lines */
	   nowm-=2;				    /* aren't as trustworthy */
	if(*saddr=='\n'&&(pastad=strchr(saddr,' ')))
	   saddr=pastad+1;			/* reposition at the address */
	chp=saddr;
	while((pastad=strchr(chp,'\n'))&&(pastad=strchr(pastad,' ')))
	   chp=pastad+1;		      /* skip to the last uucp >From */
	if(pastad=strchr(chp,' '))			/* found an address? */
	 { char*savetmp;				      /* lift it out */
	   savetmp=malloc(1+(j=pastad-chp)+1+1);tmemmove(savetmp,chp,j);
	   savetmp[j]='\0';				   /* make work copy */
	   if(strchr(savetmp,'@'))			 /* domain attached? */
	      chp=savetmp,savetmp=tmp,tmp=chp;			/* ok, ready */
	   else					/* no domain, bang away! :-) */
	    { static const char remf[]=" remote from ",fwdb[]=" forwarded by ";
	      char*p1,*p2;
	      chp=tmp;
	      for(;;)
	       { int c;
		 p1=strstr(saddr,remf);
		 if(!(p2=strstr(saddr,fwdb))&&!p1)
		    break;				     /* no more info */
		 if(!p1||p2&&p2<p1)		      /* pick the first bang */
		    p1=p2+STRLEN(fwdb);
		 else
		    p1+=STRLEN(remf);
		 for(;;)				     /* copy it over */
		  { switch(c= *p1++)
		     { default:*chp++=c;
			  continue;
		       case '\0':case '\n':*chp++='!';	     /* for the buck */
		     }
		    break;
		  }
		 saddr=p1;				/* continue the hunt */
	       }
	      strcpy(chp,savetmp);chp=tmp;	     /* attach the user part */
	    }			  /* (temporary buffers might have switched) */
	   free(savetmp);savetmp=strchr(tmp,'\0');	      /* prepend '<' */
	   tmemmove(tmp+1,tmp,savetmp-tmp);*tmp='<';savetmp[1]='\0';
	 }
      }
     while(*(chp=skpspace(chp))=='\n')
	chp++;
     for(saddr=0;;chp=skipwords(chp))			/* skip RFC 822 wise */
      { switch(*chp)
	 { default:
	      if(!saddr)		   /* if we haven't got anything yet */
		 saddr=chp;			/* this might be the address */
	      continue;
	   case '<':skipwords(saddr=chp);	  /* hurray, machine useable */
	   case '\0':;
	 }
	break;
      }
     if(saddr)				    /* any useful mailaddress found? */
      { if(*saddr)				  /* did it have any length? */
	 { if(!strpbrk(saddr,"@!/"))
	      nowm-=(maxindex(sest)+2)*4;		/* depreciate "user" */
	   else if(strstr(saddr,".UUCP"))
	      nowm-=(maxindex(sest)+2)*3;	 /* depreciate .UUCP address */
	   else if(strchr(saddr,'@')&&!strchr(saddr,'.'))
	      nowm-=(maxindex(sest)+2)*2;	     /* depreciate user@host */
	   else if(strchr(saddr,'!'))
	      nowm-=(maxindex(sest)+2)*1;	     /* depreciate bangpaths */
	   if(!namep||nowm>lastm)		/* better than previous ones */
	      goto pnewname;
	 }
	else if(sest[i].head==returnpath)		/* nill Return-Path: */
	 { saddr=(char*)mdaemon;nowm=maxindex(sest)+2;		 /* override */
pnewname:  lastm=nowm;saddr=strcpy(malloc(strlen(saddr)+1),saddr);
	   if(namep)
	      free(namep);
	   namep=saddr;
	 }
      }
     free(tmp);
   }					   /* save headers for later perusal */
  return namep;
}
			     /* lifted out of main() to reduce main()'s size */
static void elimdups(namep,idcache,maxlen,split)const char*const namep;
 FILE*idcache;const long maxlen;const int split;
{ int dupid=0;char*key,*oldnewl;
  key=(char*)namep;		  /* not to worry, no change will be noticed */
  if(!areply)
   { key=0;
     if(msid->rexl)					/* any Message-ID: ? */
	*(oldnewl=(key=msid->rexp)+msid->rexl-1)='\0';
   }						/* wipe out trailing newline */
  if(key)
   { long insoffs=maxlen;
     while(*key==' ')				     /* strip leading spaces */
	key++;
     do
      { int j;char*p;		  /* start reading & comparing the next word */
	for(p=key;(j=fgetc(idcache))==*p;p++)
	   if(!j)					     /* end of word? */
	    { if(!quiet)
		 nlog("Duplicate key found:"),elog(key),elog("\n");
	      dupid=1;
	      goto dupfound;			     /* YES! duplicate found */
	    }
	if(!j)						     /* end of word? */
	 { if(p==key&&insoffs==maxlen)			 /* first character? */
	    { insoffs=ftell(idcache)-1;			     /* found end of */
	      goto skiprest;				  /* circular buffer */
	    }
	 }
	else
skiprest:  for(;;)				/* skip the rest of the word */
	    { switch(fgetc(idcache))
	       { case EOF:
		    goto noluck;
		 default:
		    continue;
		 case '\0':;
	       }
	      break;
	    }
      }
     while(ftell(idcache)<maxlen);			  /* past our quota? */
noluck:
     if(insoffs>=maxlen)				  /* past our quota? */
	insoffs=0;				     /* start up front again */
     fseek(idcache,insoffs,SEEK_SET);fwrite(key,1,strlen(key)+1,idcache);
     putc('\0',idcache);			   /* mark new end of buffer */
dupfound:
     fseek(idcache,(long)0,SEEK_SET);		 /* rewind, for any next run */
     if(!areply)
	*oldnewl='\n';				      /* restore the newline */
   }
  if(!split)				  /* not splitting?  terminate early */
     exit(dupid?EXIT_SUCCESS:1);
  if(dupid)				       /* duplicate? suppress output */
     closemine(),opensink();
}

static PROGID;

int main(lastm,argv)int lastm;const char*const argv[];
{ int i,split=0,force=0,bogus=1,every=0,headreply=0,digest=0,nowait=0,keepb=0,
   minfields=(char*)progid-(char*)progid,conctenate=0,babyl=0,babylstart,
   berkeley=0,forgetclen;
  long maxlen,ctlength;FILE*idcache=0;pid_t thepid;
  size_t j,lnl,escaplen;char*chp,*namep,*escap=ESCAP;
  struct field*fldp,*fp2,**afldp,*fdate,*fcntlength,*fsubject,*fFrom_;
  if(lastm)			       /* sanity check, any argument at all? */
#define Qnext_arg()	if(!*chp&&!(chp=(char*)*++argv))goto usg
     while(chp=(char*)*++argv)
      { if((lastm= *chp++)==FM_SKIP)
	   goto number;
	else if(lastm!=FM_TOTAL)
	   goto usg;
	for(;;)
	 { switch(lastm= *chp++)
	    { case FM_TRUST:headreply|=1;
		 continue;
	      case FM_REPLY:areply=1;
		 continue;
	      case FM_FORCE:force=1;
		 continue;
	      case FM_EVERY:every=1;
		 continue;
	      case FM_BABYL:babyl=every=1;
	      case FM_DIGEST:digest=1;
		 continue;
	      case FM_NOWAIT:nowait=1;Qnext_arg();
		 childlimit=strtol(chp,&chp,10);
		 continue;
	      case FM_KEEPB:keepb=1;
		 continue;
	      case FM_CONCATENATE:conctenate=1;
		 continue;
	      case FM_ZAPWHITE:zap=1;
		 continue;
	      case FM_QUIET:quiet=1;
		 if(*chp=='-')
		    chp++,quiet=0;
		 continue;
	      case FM_LOGSUMMARY:Qnext_arg();
		 if(strlen(logsummary=chp)>MAXfoldlen)
		    chp[MAXfoldlen]='\0';
		 detab(chp);
		 break;
	      case FM_SPLIT:split=1;
		 if(!*chp)
		  { ++argv;
		    goto parsedoptions;
		  }
		 goto usg;
	      case HELPOPT1:case HELPOPT2:elog(fmusage);elog(FM_HELP);
		 elog(FM_HELP2); /* had to split up FM_HELP, compiler limits */
		 goto xusg;
	      case FM_DUPLICATE:case FM_MINFIELDS:Qnext_arg();chp++;
	      default:chp--;
number:		 if(*chp-'0'>(unsigned)9)	    /* the number is not >=0 */
		    goto usg;
		 i=strtol(chp,&chp,10);
		 switch(lastm)			/* where does the number go? */
		  { case FM_SKIP:nrskip=i;
		       break;
		    case FM_DUPLICATE:maxlen=i;Qnext_arg();
		       if(!(idcache=fopen(chp,"r+b"))&&	  /* existing cache? */
			  !(idcache=fopen(chp,"w+b")))	    /* create cache? */
			{ nlog("Couldn't open");logqnl(chp);
			  return EX_CANTCREAT;
			}
		       goto nextarg;
		    case FM_MINFIELDS:minfields=i;
		       break;
		    default:nrtotal=i;
		  }
		 continue;
	      case FM_BOGUS:bogus=0;
		 continue;
	      case FM_BERKELEY:berkeley=1;
		 continue;
	      case FM_QPREFIX:Qnext_arg();escap=chp;
		 break;
	      case FM_VERSION:elog(formailn);elog(VERSION);
		 goto xusg;
	      case FM_ADD_IFNOT:case FM_ADD_ALWAYS:case FM_REN_INSERT:
	      case FM_DEL_INSERT:case FM_EXTRACT:case FM_EXTRC_KEEP:
	      case FM_FIRST_UNIQ:case FM_LAST_UNIQ:case FM_ReNAME:Qnext_arg();
		 i=breakfield(chp,lnl=strlen(chp));
		 switch(lastm)
		  { case FM_ADD_IFNOT:
		       if(i>0)
			  break;
		       if(i!=-STRLEN(Resent_)||-i!=lnl|| /* the only partial */
			strncasecmp(chp,Resent_,STRLEN(Resent_)+1)) /* field */
			  goto invfield;       /* allowed with -a is Resent- */
		       headreply|=2;
		       goto nextarg;		    /* don't add to the list */
		    default:
		       if(-i!=lnl)	  /* it is not an early ending field */
		    case FM_ADD_ALWAYS:
			  if(i<=0)	      /* and it is not a valid field */
			     goto invfield;			 /* complain */
		    case FM_ReNAME:;		       /* everything allowed */
		  }
		 chp[lnl]='\n';			       /* terminate the line */
		 afldp=addfield(lastm==FM_REN_INSERT?&iheader:
		  lastm==FM_DEL_INSERT?&Iheader:lastm==FM_ADD_IFNOT?&aheader:
		  lastm==FM_ADD_ALWAYS?&Aheader:lastm==FM_EXTRACT?&xheader:
		  lastm==FM_FIRST_UNIQ?&uheader:lastm==FM_LAST_UNIQ?&Uheader:
		  lastm==FM_EXTRC_KEEP?&Xheader:&Rheader,chp,++lnl);
		 if(lastm==FM_ReNAME)	      /* then we need a second field */
		  { int copied=0;
		    for(namep=(chp=(fldp= *afldp)->fld_text)+lnl,
		     chp+=lnl=fldp->id_len;chp<namep;++chp)
		     { switch(*chp)			  /* skip whitespace */
			{ case ' ':case '\t':case '\n':
			     continue;
			}
		       break;
		     }				   /* second field attached? */
		    lastm=i;
		    if((i=breakfield(chp,(size_t)(namep-chp)))<0) /* partial */
		       if(lastm>0)		     /* complete first field */
			  goto invfield;	   /* impossible combination */
		       else
			  i= -i;
		    if(i)
		       tmemmove((char*)fldp->fld_text+lnl,chp,i),copied=1;
		    else if(namep>chp||				 /* garbage? */
			    !(chp=(char*)*++argv)||	 /* look at next arg */
			    (!(i=breakfield(chp,strlen(chp)))&& /* fieldish? */
			     *chp)||			   /* but "" is fine */
			    i<=0&&(i= -i,lastm>0)) /* impossible combination */
invfield:	     { nlog("Invalid field-name:");logqnl(chp?chp:"");
		       goto usg;
		     }
		    *afldp=fldp=
		     realloc(fldp,FLD_HEADSIZ+(fldp->Tot_len=lnl+i));
		    if(!copied)			   /* if not squeezed on yet */
		       tmemmove((char*)fldp->fld_text+lnl,chp,i);  /* do now */
		  }
	      case '\0':;
	    }
	   break;
	 }
nextarg:;
      }
parsedoptions:
  escaplen=strlen(escap);mystdout=stdout;signal(SIGPIPE,SIG_IGN);
#ifdef SIGCHLD
  signal(SIGCHLD,SIG_DFL);
#endif
  thepid=getpid();
  if(babyl)						/* skip BABYL leader */
   { while(getchar()!=BABYL_SEP1||getchar()!=BABYL_SEP2||getchar()!='\n')
	while(getchar()!='\n');
     while(getchar()!='\n');
   }
  while((buflast=getchar())=='\n');		     /* skip leading garbage */
  if(split)
   { char**ep;char**vfileno=0;
     if(buflast==EOF)			   /* avoid splitting empty messages */
	return EXIT_SUCCESS;
     for(ep=environ;*ep;ep++)		   /* gobble through the environment */
	if(!strncmp(*ep,ffileno,LEN_FILENO_VAR))	 /* look for FILENO= */
	   vfileno=ep;					    /* yes, found it */
     if(!vfileno)			/* FILENO= found in the environment? */
      { size_t envlen;						 /* no, pity */
	envlen=(ep-environ+1)*sizeof*environ;		   /* current length */
	tmemmove(ep=malloc(envlen+sizeof*environ),environ,envlen);
	*(vfileno=(char**)((char*)(environ=ep)+envlen))=0;*--vfileno=ffileno;
      }						      /* copy over the array */
     if((lenfileno=strlen(chp= *vfileno+LEN_FILENO_VAR))>
	STRLEN(ffileno)-LEN_FILENO_VAR-1)	  /* check the desired width */
	lenfileno=STRLEN(ffileno)-LEN_FILENO_VAR-1;	/* too big, truncate */
     if((initfileno=strtol(chp,&chp,10))<0)	  /* fetch the initial value */
	lenfileno--;				 /* correct it for negatives */
     if(*chp)						 /* no valid number? */
	lenfileno= -1;			    /* disable the FILENO generation */
     else
	*vfileno=ffileno;	    /* stuff our template in the environment */
     oldstdout=dup(STDOUT);fclose(stdout);
     if(!nrtotal)
	goto onlyhead;
     startprog((const char*Const*)argv);
     if(!minfields)			       /* no user specified minimum? */
	minfields=DEFminfields;				 /* take our default */
   }
  else if(nrskip>0||nrtotal>=0||every||digest||minfields||nowait)
     goto usg;			     /* only valid in combination with split */
  if((xheader||Xheader)&&logsummary||keepb&&!(areply||xheader||Xheader))
usg:						     /* options sanity check */
   { elog(fmusage);					   /* impossible mix */
xusg:
     return EX_USAGE;
   }
  if(headreply==2)				/* -aResent- is only allowed */
   { chp=(char*)Resent_;		  /* as a modifier to header replies */
     goto invfield;
   }
  buf=malloc(buflen=Bsize);Totallen=0;i=maxindex(rex); /* prime some buffers */
  do rex[i].rexp=malloc(1);
  while(i--);
  fdate=0;addfield(&fdate,date,STRLEN(date)); /* fdate is only for searching */
  fcntlength=0;addfield(&fcntlength,cntlength,STRLEN(cntlength));   /* ditto */
  fFrom_=0;addfield(&fFrom_,From_,STRLEN(From_));
  fsubject=0;addfield(&fsubject,subject,STRLEN(subject));	 /* likewise */
  forgetclen=digest||		      /* forget Content-Length: for a digest */
	     berkeley||				      /* for Berkeley format */
	     keepb&&			    /* if we're keeping the body and */
	      (areply||					     /* autoreplying */
	       Xheader&&			    /* or eXtracting without */
	       !findf(fcntlength,&Xheader));	  /* getting Content-Length: */
  if(areply)					       /* when auto-replying */
     addfield(&iheader,xloop,STRLEN(xloop));	  /* preserve X-Loop: fields */
  if(!readhead())					    /* start looking */
   {
#ifdef sMAILBOX_SEPARATOR			      /* check for a leading */
     if(!strncmp(smboxsep,buf,STRLEN(smboxsep)))	/* mailbox separator */
      { buffilled=0;						  /* skip it */
	goto startover;
      }
#endif
     if(digest&&artheadr())
	goto startover;
   }
  else
startover:
     while(readhead());				 /* read in the whole header */
  cleanheader();
  ;{ size_t lenparkedbuf;void*parkedbuf;int wasafrom_;
     if(rdheader)
      { char*tmp,*tmp2;
	if(!strncmp(tmp=(char*)rdheader->fld_text,Article_,STRLEN(Article_)))
	   tmp[STRLEN(Article_)-1]=HEAD_DELIMITER;
	else if(babyl&&
		!force&&
		!strncmp(tmp,mailfrom,STRLEN(mailfrom))&&
		eqFrom_(tmp2=skpspace(tmp+STRLEN(mailfrom))))
	 { rdheader->id_len=STRLEN(From_);
	   tmemmove(tmp,tmp2,rdheader->Tot_len-=tmp2-tmp);
	 }
      }
     namep=0;Totallen=0;i=maxindex(rex);
     do rex[i].rexl=0;
     while(i--);			      /* reset all state information */
     clear_uhead(uheader);clear_uhead(Uheader);
     wasafrom_=!force&&rdheader&&eqFrom_(rdheader->fld_text);
     procfields(areply);
     for(fldp= *(afldp= &rdheader);fldp;)
      { if(zap)		      /* go through the linked list of header-fields */
	 { chp=fldp->fld_text+(j=fldp->id_len);
	   if(chp[-1]==HEAD_DELIMITER)
	      if((*chp!=' '&&*chp!='\t')&&fldp->Tot_len>j+1)
	       { chp=j+(*afldp=fldp=
		  realloc(fldp,FLD_HEADSIZ+(i=fldp->Tot_len++)+1))->fld_text;
		 tmemmove(chp+1,chp,i-j);*chp=' ';
	       }
	      else if(fldp->Tot_len<=j+2)
	       { *afldp=fldp->fld_next;free(fldp);fldp= *afldp;
		 continue;
	       }
	 }
	if(conctenate)
	   concatenate(fldp);		    /* save fields for later perusal */
	namep=getsender(namep,fldp,headreply);
	i=maxindex(rex);chp=fldp->fld_text;j=fldp->id_len;
	while((rex[i].lenr!=j||strncasecmp(rex[i].headr,chp,j))&&i--);
	chp+=j;
	if(i>=0&&(j=fldp->Tot_len-j)>1)			  /* found anything? */
	 { tmemmove(rex[i].rexp=realloc(rex[i].rexp,(rex[i].rexl=j)+1),chp,j);
	   rex[i].rexp[j]='\0';			     /* add a terminating \0 */
	 }
	fldp= *(afldp= &fldp->fld_next);
      }
     if(idcache)
	elimdups(namep,idcache,maxlen,split);
     ctlength=0;
     if(!forgetclen&&(fldp=findf(fcntlength,&rdheader)))
      { *(chp=(char*)fldp->fld_text+fldp->Tot_len-1)='\0';   /* terminate it */
	ctlength=strtol((char*)fldp->fld_text+STRLEN(cntlength),(char**)0,10);
	*chp='\n';			     /* restore the trailing newline */
      }
     tmemmove(parkedbuf=malloc(buffilled),buf,lenparkedbuf=buffilled);
     buffilled=0;    /* moved the contents of buf out of the way temporarily */
     if(areply)		      /* autoreply requested, we clean up the header */
      { for(fldp= *(afldp= &rdheader);fldp;)
	   if(!(fp2=findf(fldp,&iheader))||fp2->id_len<fp2->Tot_len-1)
	      *afldp=fldp->fld_next,free(fldp),fldp= *afldp;   /* remove all */
	   else					/* except the ones mentioned */
	      fldp= *(afldp= &fldp->fld_next);		       /* as -i ...: */
	loadbuf(To,STRLEN(To));loadchar(' ');	   /* generate the To: field */
	if(namep)	       /* did we find a valid return address at all? */
	   loadbuf(namep,strlen(namep));	      /* then insert it here */
	else					    /* or insert our default */
	   retval=EX_NOUSER,loadbuf(unknown,STRLEN(unknown));
	loadchar('\n');addbuf();		       /* add it to rdheader */
	if(subj->rexl)				      /* any Subject: found? */
	 { loadbuf(subject,STRLEN(subject));	  /* sure, check for leading */
	   if(strncasecmp(skpspace(chp=subj->rexp),Re,STRLEN(Re)))    /* Re: */
	      loadbuf(re,STRLEN(re));	       /* no Re: , add one ourselves */
	   loadsaved(subj);addbuf();
	 }
	if(refr->rexl||msid->rexl)	   /* any References: or Message-ID: */
	 { loadbuf(references,STRLEN(references)); /* yes insert References: */
	   if(refr->rexl)
	    { if(msid->rexl)	    /* if we're going to append a Message-ID */
		 --refr->rexl;		    /* suppress the trailing newline */
	      loadsaved(refr);
	    }
	   if(msid->rexl)
	      loadsaved(msid);		       /* here's our missing newline */
	   addbuf();
	 }
	if(msid->rexl)			 /* do we add an In-Reply-To: field? */
	   loadbuf(inreplyto,STRLEN(inreplyto)),loadsaved(msid),addbuf();
	procfields(0);
      }
     else if(!force&&		       /* are we allowed to add From_ lines? */
	     (!rdheader||!eqFrom_(rdheader->fld_text))&&   /* is it missing? */
	     ((fldp=findf(fFrom_,&aheader))&&STRLEN(From_)+1>=fldp->Tot_len||
	      !wasafrom_&&			    /* if there was no From_ */
	      !findf(fFrom_,&iheader)&&		   /* and From_ is not being */
	      !findf(fFrom_,&Iheader)&&				/* supressed */
	      !findf(fFrom_,&Rheader)))
      { struct field*old;time_t t;	     /* insert a From_ line up front */
	t=time((time_t*)0);old=rdheader;rdheader=0;
	loadbuf(From_,STRLEN(From_));
	if(namep)			  /* we found a valid return address */
	   loadbuf(namep,strlen(namep));
	else
	   loadbuf(unknown,STRLEN(unknown));
	loadchar(' ');				   /* insert one extra blank */
	if(!hdate->rexl||!findf(fdate,&aheader))		    /* Date: */
	   loadchar(' '),chp=ctime(&t),loadbuf(chp,strlen(chp)); /* no Date: */
	else					 /* we generate it ourselves */
	   loadsaved(hdate);	      /* yes, found Date:, then copy from it */
	addbuf();rdheader->fld_next=old;
      }
     for(fldp=aheader;fldp;fldp=fldp->fld_next)
	if(!findf(fldp,&rdheader))	       /* only add what didn't exist */
	   if(fldp->id_len+1>=fldp->Tot_len&&		  /* field name only */
	      (fldp->id_len==STRLEN(messageid)&&
	       !strncasecmp(fldp->fld_text,messageid,STRLEN(messageid))||
	       fldp->id_len==STRLEN(res_messageid)&&
	       !strncasecmp(fldp->fld_text,res_messageid,STRLEN(res_messageid))
	      ))
	    { char*p;const char*name;unsigned long h1,h2,h3;
	      static unsigned long h4; /* conjure up a `unique' msg-id field */
	      h1=time((time_t*)0);h2=thepid;h3=rhash;
	      p=chp=malloc(fldp->id_len+2+((sizeof h1*8+5)/6+1)*4+
	       strlen(name=hostname())+2);     /* allocate worst case length */
	      memcpy(p,fldp->fld_text,fldp->id_len);*(p+=fldp->id_len)=' ';
	      *++p='<';*(p=ultoan(h3,p+1))='.';*(p=ultoan(h4,p+1))='.';
	      *(p=ultoan(h2,p+1))='.';*(p=ultoan(h1,p+1))='@';strcpy(p+1,name);
	      *(p=strchr(p,'\0'))='>';*++p='\n';addfield(&nheader,chp,p-chp+1);
	      free(chp);h4++;					/* put it in */
	    }
	   else
	      addfield(&nheader,fldp->fld_text,fldp->Tot_len);
     if(logsummary)
      { if(eqFrom_(rdheader->fld_text))
	   putssn(rdheader->fld_text,rdheader->Tot_len);
	if(fldp=findf(fsubject,&rdheader))
	 { concatenate(fldp);(chp=fldp->fld_text)[i=fldp->Tot_len-1]='\0';
	   detab(chp);putcs(' ');
	   putssn(chp,i>=MAXSUBJECTSHOW?MAXSUBJECTSHOW:i);putcs('\n');
	 }
      }					/* restore the saved contents of buf */
     tmemmove(buf,parkedbuf,buffilled=lenparkedbuf);free(parkedbuf);
   }
  flushfield(&rdheader);flushfield(&nheader);dispfield(Aheader);
  dispfield(iheader);dispfield(Iheader);
  if(namep)
     free(namep);
  if(keepb||!(xheader||Xheader))	 /* we're not just extracting fields */
     lputcs('\n');		/* make sure it is followed by an empty line */
  if(!keepb&&(areply||xheader||Xheader))		    /* decision time */
   { logfolder();				   /* we throw away the rest */
     if(split)
	closemine();
     else		      /* terminate early, only the header was needed */
	goto onlyhead;
     opensink();					 /* discard the body */
   }
  lnl=1;					  /* last line was a newline */
  if(buffilled==1)		   /* the header really ended with a newline */
     buffilled=0;	      /* throw it away, since we already inserted it */
  if(babyl)
   { int c,lc;					/* ditch pseudo BABYL header */
     for(lc=0;c=getchar(),c!=EOF&&(c!='\n'||lc!='\n');lc=c);
     buflast=c;babylstart=0;
   }
  if(ctlength>0)
   { if(buffilled)
	lputssn(buf,buffilled),ctlength-=buffilled,buffilled=lnl=0;
     ;{ int tbl=buflast,lwr='\n';
	while(--ctlength>=0&&tbl!=EOF)	       /* skip Content-Length: bytes */
	   lnl=lwr==tbl&&lwr=='\n',putcs(lwr=tbl),tbl=getchar();
	if((buflast=tbl)=='\n'&&lwr!=tbl)	/* just before a line break? */
	   putcs('\n'),buflast=getchar();		/* wrap up loose end */
      }
     if(!quiet&&ctlength>0)
      { charNUM(num,ctlength);
	nlog(cntlength);elog(" field exceeds actual length by ");
	ultstr(0,(unsigned long)ctlength,num);elog(num);elog(" bytes\n");
      }
   }
  while(buffilled||!lnl||buflast!=EOF)	 /* continue the quest, line by line */
   { if(!buffilled)				      /* is it really empty? */
	readhead();				      /* read the next field */
     if(!babyl||babylstart)	       /* don't split BABYL files everywhere */
      { if(rdheader)		    /* anything looking like a header found? */
	 { if(eqFrom_(chp=rdheader->fld_text))	      /* check if it's From_ */
fromanyway: { register size_t k;
	      if(split&&
		 (lnl||every)&&	       /* more thorough check for a postmark */
		 (k=strcspn(chp=skpspace(chp+STRLEN(From_))," \t\n"))&&
		 *skpspace(chp+k)!='\n')
		 goto accuhdr;		     /* ok, postmark found, split it */
	      if(bogus)						   /* disarm */
		 lputssn(escap,escaplen);
	    }
	   else if(split&&digest&&(lnl||every)&&digheadr())	  /* digest? */
accuhdr:    { for(i=minfields;--i&&readhead()&&digheadr();); /* found enough */
	      if(!i)					   /* then split it! */
splitit:       { if(!lnl)   /* did the previous mail end with an empty line? */
		    lputcs('\n');		      /* but now it does :-) */
		 logfolder();
		 if(fclose(mystdout)==EOF||errout==EOF)
		  { split= -1;
		    if(!quiet)
		       nlog(couldntw),elog(", continuing...\n");
		  }
		 if(!nowait&&*argv)	 /* wait till the child has finished */
		  { int excode;
		    if((excode=waitfor(child))!=EXIT_SUCCESS&&
		       retval==EXIT_SUCCESS)
		       retval=excode;
		  }
		 if(!nrtotal)
		    goto nconlyhead;
		 startprog((const char*Const*)argv);
		 goto startover;
	       }				    /* and there we go again */
	    }
	 }
	else if(eqFrom_(buf))			 /* special case, From_ line */
	 { addbuf();		       /* add it manually, readhead() didn't */
	   goto fromanyway;
	 }
	else if(split&&digest&&(lnl||every)&&artheadr())
	   goto accuhdr;
      }
#ifdef MAILBOX_SEPARATOR
     if(!strncmp(emboxsep,buf,STRLEN(emboxsep)))	     /* end of mail? */
      { if(split)		       /* gobble up the next start separator */
	 { buffilled=0;
#ifdef sMAILBOX_SEPARATOR
	   getline();buffilled=0;		 /* but only if it's defined */
#endif
	   if(buflast!=EOF)					   /* if any */
	      goto splitit;
	   break;
	 }
#ifdef eMAILBOX_SEPARATOR
	if(buflast==EOF)
	   break;
#endif
	if(bogus)
	   goto putsp;				   /* escape it with a space */
      }
     else if(!strncmp(smboxsep,buf,STRLEN(smboxsep))&&bogus)
putsp:	lputcs(' ');
#endif /* MAILBOX_SEPARATOR */
     lnl=buffilled==1;		      /* check if we just read an empty line */
     if(babyl&&*buf==BABYL_SEP1)
	babylstart=1,closemine(),opensink();		 /* discard the rest */
     if(areply&&bogus)					  /* escape the body */
	if(fldp=rdheader)	      /* we already read some "valid" fields */
	 { register char*p;
	   rdheader=0;
	   do			       /* careful, they can contain newlines */
	    { fp2=fldp->fld_next;chp=fldp->fld_text;
	      do
	       { lputssn(escap,escaplen);
		 if(p=memchr(chp,'\n',fldp->Tot_len))
		    p++;
		 else
		    p=(char*)fldp->fld_text+fldp->Tot_len;
		 lputssn(chp,p-chp);
	       }
	      while((chp=p)<(char*)fldp->fld_text+fldp->Tot_len);
	      free(fldp);					/* delete it */
	    }
	   while(fldp=fp2);		       /* escape all fields we found */
	 }
	else
	 { if(buffilled>1)	  /* we don't escape empty lines, looks neat */
	      lputssn(escap,escaplen);
	   goto flbuf;
	 }
     else if(rdheader)
      { struct field*ox,*oX;
	ox=xheader;oX=Xheader;xheader=Xheader=0;flushfield(&rdheader);
	xheader=ox;Xheader=oX; /* beware, after this buf can still be filled */
      }
     else
flbuf:	lputssn(buf,buffilled),buffilled=0;
   }			       /* make sure the mail ends with an empty line */
  logfolder();
onlyhead:
  closemine();
nconlyhead:
  if(split)						/* wait for everyone */
   { int excode;
     close(STDIN);	       /* close stdin now, we're not reading anymore */
     while((excode=waitfor((pid_t)0))!=NO_PROCESS)
	if(retval==EXIT_SUCCESS&&excode!=EXIT_SUCCESS)
	   retval=excode;
   }
  if(retval<0)
     retval=EX_UNAVAILABLE;
  return retval!=EXIT_SUCCESS?retval:split<0?EX_IOERR:EXIT_SUCCESS;
}

int eqFrom_(a)const char*const a;
{ return !strncmp(a,From_,STRLEN(From_));
}

int breakfield(line,len)const char*const line;size_t len;  /* look where the */
{ const char*p=line;			   /* fieldname ends (RFC 822 specs) */
  while(len)
   { switch(*p)
      { default:len--;
	   if(iscntrl(*p))		    /* no control characters allowed */
	      break;
	   p++;
	   continue;
	case HEAD_DELIMITER:
	   len=p-line;
	   return len?len+1:0;					  /* eureka! */
	case ' ':case '\t':	/* whitespace is okay right before the colon */
	   if(p>line)	    /* but only if we've seen something else already */
	    { const char*q=++p;
	      while(--len&&(*q==' '||*q=='\t'))		     /* skip forward */
		 q++;
	      if(len&&*q==HEAD_DELIMITER)			/* it's okay */
		 return q-line+1;
	      if(eqFrom_(line))			      /* special case, From_ */
		 return STRLEN(From_);
	    }					   /* it was bogus after all */
      }
     break;
   }
  return -(int)(p-line);    /* sorry, does not seem to be a legitimate field */
}
