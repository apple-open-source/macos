/************************************************************************
 *	Some routine common to procmail, formail and lockfile		*
 *									*
 *	Copyright (c) 1993-1997, S.R. van den Berg, The Netherlands	*
 *	Copyright (c) 1999-2001, Philip Guenther, The United States	*
 *						of America		*
 *	#include "../README"						*
 ************************************************************************/
#ifdef RCS
static /*const*/char rcsid[]=
 "$Id: acommon.c,v 1.12 2001/08/04 07:14:59 guenther Exp $";
#endif
#include "includes.h"
#include "acommon.h"
#include "robust.h"
#include "shell.h"

const char*hostname P((void))
{
#ifdef	NOuname
#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN	64
#endif
  static char name[MAXHOSTNAMELEN]="";
  if(!name[0])
     gethostname(name,MAXHOSTNAMELEN),name[MAXHOSTNAMELEN-1]='\0';
#else
  static char*name=0;
  if(!name)
   { struct utsname names;
     Uname(&names);
     if(!(name=malloc(strlen(names.nodename)+1)))
	return "";	      /* can happen when called from within lockfile */
     strcpy(name,names.nodename);
   }
#endif
  return name;
}

char*ultoan(val,dest)unsigned long val;char*dest;     /* convert to a number */
{ register int i;			     /* within the set [A-Za-z0-9-_] */
  do
   { i=val&0x3f;			   /* collating sequence dependency! */
     *dest++=i+(i<26?'A':i<26+26?'a'-26:i<26+26+10?'0'-26-26:
      i==26+26+10?'-'-26-26-10:'_'-26-26-11);
   }
  while(val>>=6);
  *dest='\0';
  return dest;
}

char*ultstr(minwidth,val,dest)int minwidth;unsigned long val;char*dest;
{ int i;unsigned long j;char*ret;
  j=val;i=0;					   /* a beauty, isn't it :-) */
  do i++;					   /* determine needed width */
  while(j/=10);
  while(--minwidth>=i)				 /* fill up any excess width */
     *dest++=' ';
  *(ret=dest+=i)='\0';
  do *--dest='0'+val%10;			  /* display value backwards */
  while(val/=10);
  return ret;
}
