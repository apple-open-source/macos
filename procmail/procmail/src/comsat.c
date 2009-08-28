/************************************************************************
 *	Routines for dealing with comsat, as used by procmail		*
 *									*
 *	Copyright (c) 1990-1999, S.R. van den Berg, The Netherlands	*
 *	Copyright (c) 1999-2001, Philip Guenther, The United States	*
 *							of America	*
 *	#include "../README"						*
 ************************************************************************/

#ifdef RCS
static /*const*/char rcsid[]=
 "$Id: comsat.c,v 1.5 2001/07/03 15:05:47 guenther Exp $";
#endif

#include "procmail.h"

#ifndef NO_COMSAT

#include "sublib.h"			       /* for strtol() and strlcat() */
#include "robust.h"					     /* for rclose() */
#include "shell.h"					   /* for tmemmove() */
#include "acommon.h"					     /* for ultstr() */
#include "mailfold.h"					     /* for lasttell */
#include "misc.h"
#include "variables.h"
#include "network.h"
#include "comsat.h"

static int csvalid;		     /* is it turned on with a good address? */
static struct sockaddr_in csaddr;
static char*cslastf,*cslgname;

void setlfcs(folder)const char*folder;		/* set lastfolder for comsat */
{ char*new,*old;
  if(strchr(dirsep,*folder))			       /* absolute filename? */
     new=tstrdup(folder);
  else					/* nope; prepend the current maildir */
   { const char*md=tgetenv(maildir);
     size_t len=strlen(md)+2+strlen(folder);
     new=malloc(len);
     strcpy(new,md);
     strlcat(new,MCDIRSEP_,len);
     strlcat(new,folder,len);
   }
  onguard();			     /* protect the change over from signals */
  old=cslastf;
  cslastf=new;
  offguard();
  if(old)
     free(old);
}

void setlgcs(name)const char*name;		   /* set logname for comsat */
{ char*new,*old;
  new=tstrdup(name);
  onguard();			     /* protect the change over from signals */
  old=cslgname;
  cslgname=new;
  offguard();
  if(old)
     free(old);
}

int setcomsat(chp)const char*chp;
{ char*chad;int newvalid; struct sockaddr_in newaddr;
  chad=strchr(chp,SERV_ADDRsep);			     /* @ separator? */
  if(!chad&&!renvint(-1L,chp))
     return csvalid=0;					/* turned off comsat */
  newvalid=1;
  if(chad)
     *chad++='\0';				      /* split the specifier */
  if(!chad||!*chad)						  /* no host */
#ifndef IP_localhost			      /* Is "localhost" preresolved? */
     chad=COMSAThost;					/* nope, use default */
#else /* IP_localhost */
   { static const unsigned char ip_localhost[]=IP_localhost;
     newaddr.sin_family=AF_INET;
     tmemmove(&newaddr.sin_addr,ip_localhost,sizeof ip_localhost);
   }
  else
#endif /* IP_localhost */
   { const struct hostent*host;		      /* what host?  paranoid checks */
     if(!(host=gethostbyname(chad))||!host->h_0addr_list)
      { bbzero(&newaddr.sin_addr,sizeof newaddr.sin_addr);
	newvalid=0;			     /* host can't be found, too bad */
      }
     else
      { newaddr.sin_family=host->h_addrtype;	     /* address number found */
	tmemmove(&newaddr.sin_addr,host->h_0addr_list,host->h_length);
      }
     endhostent();
   }
  if(newvalid)						  /* so far, so good */
   { int s;
     if(!*chp)						       /* no service */
	chp=BIFF_serviceport;				/* new balls please! */
     s=strtol(chp,&chad,10);
     if(chp!=chad)			       /* the service is not numeric */
	newaddr.sin_port=htons((short)s);		    /* network order */
     else
      { const struct servent*serv;
	serv=getservbyname(chp,COMSATprotocol);		   /* so get its no. */
	if(serv)
	   newaddr.sin_port=serv->s_port;
	else
	 { newaddr.sin_port=htons((short)0);		  /* no such service */
	   newvalid=0;
	 }
	endservent();
      }
   }
  onguard();				    /* update the address atomically */
  if(csvalid=newvalid)
     tmemmove(&csaddr,&newaddr,sizeof(newaddr));
  offguard();
  return newvalid;
}

void sendcomsat(folder)const char*folder;
{ int s;const char*p;
  if(!csvalid||!buf)		  /* is comat on and set to a valid address? */
     return;
  if(!*cslgname||strlen(cslgname)+2>linebuf)	       /* is $LOGNAME bogus? */
     return;
  if(!(p=folder?folder:cslastf))		     /* do we have a folder? */
     return;
  strcpy(buf,cslgname);
  strlcat(buf,"@",linebuf);		     /* start setting up the message */
  if(lasttell>=0)					   /* was it a file? */
   { ultstr(0,(unsigned long)lasttell,buf2);	  /* yep, include the offset */
     strlcat(buf,buf2,linebuf);
   }
  strlcat(buf,COMSATxtrsep,linebuf);			 /* custom seperator */
  strlcat(buf,p,linebuf);			  /* where was it delivered? */
  if((s=socket(AF_INET,SOCK_DGRAM,UDP_protocolno))>=0)
   { sendto(s,buf,strlen(buf),0,(struct sockaddr*)&csaddr,sizeof(csaddr));
     rclose(s);
     yell("Notified comsat:",buf);
   }
}

#else
int comsat_dummy_var;		      /* to prevent insanity in some linkers */
#endif
