/************************************************************************
 *	Routines to deal with the header-field objects in formail	*
 *									*
 *	Copyright (c) 1990-2000, S.R. van den Berg, The Netherlands	*
 *	#include "../README"						*
 ************************************************************************/
#ifdef RCS
static /*const*/char rcsid[]=
 "$Id: fields.c,v 1.31 2001/08/04 07:09:42 guenther Exp $";
#endif
#include "includes.h"
#include "formail.h"
#include "sublib.h"
#include "shell.h"
#include "common.h"
#include "fields.h"
#include "ecommon.h"
#include "formisc.h"
				/* find a field in the linked list of fields */
struct field*findf(p,ah)const struct field*const p;register struct field**ah;
{ size_t i;int uhead;char*chp;register struct field*h;
  uhead=ah==&uheader||ah==&Uheader;
  for(i=p->id_len,chp=(char*)p->fld_text,h= *ah;h;h= *(ah= &h->fld_next))
     if(i>=h->id_len&&!strncasecmp(chp,h->fld_text,h->id_len))
      { if(i>h->id_len&&uhead)			     /* finalise the header? */
	   *ah=0,(*(ah=addfield(ah,chp,i)))->fld_next=h,(h= *ah)->fld_ref=0;
	return h;
      }
  return (struct field*)0;
}

void cleanheader P((void))		  /* zorch whitespace before the ':' */
{ struct field**pp,*p;char*cp;
  for(pp=&rdheader;p= *pp;pp= &(*pp)->fld_next)
     if((cp=p->fld_text+p->id_len-1,*cp==HEAD_DELIMITER)&&	    /* has : */
	(*--cp==' '||*cp=='\t'))				   /* has ws */
      { char*q=cp++;int diff;
	while(*--q==' '||*q=='\t');		      /* find the field name */
	tmemmove(++q,cp,p->Tot_len-p->id_len+1);		   /* zappo! */
	p->id_len-=(diff=cp-q);
	p->Tot_len-=diff;
      }
}

void clear_uhead(hdr)register struct field*hdr;
{ for(;hdr;hdr=hdr->fld_next)
     hdr->fld_ref=0;
}

struct field**addfield(pointer,text,totlen)struct field**pointer;
 const char*const text;const size_t totlen;    /* add field to a linked list */
{ register struct field*p,**pp;int idlen;
  for(pp=pointer;*pp;pp= &(*pp)->fld_next);   /* skip to the end of the list */
  (*pp=p=malloc(FLD_HEADSIZ+totlen))->fld_next=0;idlen=breakfield(text,totlen);
  p->id_len=idlen>0?idlen:pp==&rdheader?0:-idlen;	    /* copy contents */
  tmemmove(p->fld_text,text,p->Tot_len=totlen);
  return pp;
}

struct field*delfield(pointer)struct field**pointer;
{ struct field*fldp;
  *pointer=(fldp= *pointer)->fld_next;free(fldp);
  return *pointer;
}

void concatenate(fldp)struct field*const fldp;
{ register char*p;register size_t l;	    /* concatenate a continued field */
  l=fldp->Tot_len;
  if(!eqFrom_(p=fldp->fld_text))	    /* don't concatenate From_ lines */
     while(l--)
	if(*p++=='\n'&&l)    /* by substituting all newlines except the last */
	   p[-1]=' ';
}

static void extractfield(p)register const struct field*p;
{ if(xheader||Xheader)					 /* extracting only? */
   { if(findf(p,&xheader))			   /* extract field contents */
      { char*chp,*echp;
	echp=(chp=(char*)p->fld_text+p->id_len)+(int)(p->Tot_len-p->id_len-1);
	if(zap)
	 { chp=skpspace(chp);
	   while(chp<echp)
	    { switch(*--echp)
	       { case ' ':case '\t':continue;
	       }
	      echp++;
	      break;
	    }
	 }
	putssn(chp,echp-chp);putcs('\n');
	return;
      }
     if(!findf(p,&Xheader))				   /* extract fields */
	return;
   }
  lputssn(p->fld_text,p->Tot_len);		      /* display it entirely */
}

void flushfield(pointer)register struct field**pointer;	 /* delete and print */
{ register struct field*p,*q;				   /* them as you go */
  for(p= *pointer,*pointer=0;p;p=q)
     q=p->fld_next,extractfield(p),free(p);
}

void dispfield(p)register const struct field*p;
{ for(;p;p=p->fld_next)			     /* print list non-destructively */
     if(p->id_len+1<p->Tot_len)			 /* any contents to display? */
	extractfield(p);
}
		    /* try and append one valid field to rdheader from stdin */
int readhead P((void))
{ int idlen;
  getline();
  if((idlen=breakfield(buf,buffilled))<=0) /* not the start of a valid field */
     return 0;
  if(idlen==STRLEN(FROM)&&eqFrom_(buf))			/* it's a From_ line */
   { if(rdheader)
	return 0;			       /* the From_ line was a fake! */
     for(;buflast=='>';getline());	    /* gather continued >From_ lines */
   }
  else
     for(;;getline())		      /* get the rest of the continued field */
      { switch(buflast)			     /* will this line be continued? */
	 { case ' ':case '\t':				  /* yep, it sure is */
	      continue;
	 }
	break;
      }
  addbuf();			  /* phew, got the field, add it to rdheader */
  return 1;
}

void addbuf P((void))
{ addfield(&rdheader,buf,buffilled);buffilled=0;
}
