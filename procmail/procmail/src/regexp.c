/************************************************************************
 *	Custom regular expression library, *fully* egrep compatible	*
 *									*
 *	Seems to be perfect.						*
 *									*
 *	Copyright (c) 1991-1999, S.R. van den Berg, The Netherlands	*
 *	#include "../README"						*
 ************************************************************************/
#ifdef RCS
static /*const*/char rcsid[]=
 "$Id: regexp.c,v 1.66 2000/10/23 09:04:25 guenther Exp $";
#endif
#include "procmail.h"
#include "sublib.h"
#include "robust.h"
#include "shell.h"
#include "misc.h"
#include "variables.h"
#include "regexp.h"

#define R_BEG_GROUP	'('
#define R_OR		'|'
#define R_END_GROUP	')'
#define R_0_OR_MORE	'*'
#define R_0_OR_1	'?'
#define R_1_OR_MORE	'+'
#define R_DOT		'.'
#define R_SOL		'^'
#define R_EOL		'$'
#define R_BEG_CLASS	'['
#define R_NOT_CLASS	'^'
#define R_RANGE		'-'
#define R_END_CLASS	']'
#define R_ESCAPE	'\\'

#define R_BEG_WORD	'<'
#define R_END_WORD	'>'
#define NO_WORD_CLASS	"[^a-zA-Z0-9_]"
#define R_SPLIT_EXPR	'/'

#define BITS_P_CHAR		8
#define OPB			(1<<BITS_P_CHAR)
#define DONE_NODE		(OPB<<1)
#define DONE_MASK		(DONE_NODE-1)
#define LOOPL_NODE		(OPB<<2)
#define LOOPR_NODE		(OPB<<3)
#define LOOP_MASK		(LOOPL_NODE-1)
#define OPC_SEMPTY		OPB		      /* stack empty special */
#define OPC_TSWITCH		(OPB+1)		      /* task switch special */
#define OPC_DOT			(OPB+2)
#define OPC_BOTEXT		(OPB+3)
#define OPC_EOTEXT		(OPB+4)
#define OPC_EPS			(OPB+5)
#define OPC_JUMP		(OPB+6)
#define OPC_CLASS		(OPB+7)
#define OPC_FIN			(OPB+8)
#define OPC_BOM			(OPB+9)
#define OPC_FILL		(OPB+10)      /* filler opcode, not executed */
		  /* Don't change any opcode above without checking skplen[] */
#define bit_type		unsigned
#define bit_bits		(sizeof(bit_type)*8)
#define bit_index(which)	((unsigned)(which)/bit_bits)
#define bit_mask(which)		((unsigned)1<<(unsigned)(which)%bit_bits)
#define bit_toggle(name,which)	(name[bit_index(which)]^=bit_mask(which))
#define bit_test(name,which)	(!!(name[bit_index(which)]&bit_mask(which)))
#define bit_set(name,which,value)	\
 (value?(name[bit_index(which)]|=bit_mask(which)):\
 (name[bit_index(which)]&=~bit_mask(which)))
#define bit_field(name,size)	bit_type name[((size)+bit_bits-1)/bit_bits]

#define SZ(x)		(sizeof(struct x))
#define Ceps		(struct eps*)
#define geno(to,add)	((char*)(to)+(add))
#define epso(to,add)	(Ceps((char*)(to)+(add)))
#define ii		(aleps.topc)
#define jj		(aleps.au.sopc)
#define spawn		sp.awn

static struct eps*r,*opcfin;
static struct{unsigned topc;union seps au;}aleps;
static uchar*p,*cachea,*cachep;
static size_t cacher;
static unsigned case_ignore,errorno;

struct jump {unsigned opcj_;union {struct eps*nextj;void*Irrelevoid;} nextj_;};
struct mchar {unsigned opcc_;struct eps*next1_;
 struct evoi {struct eps*st_;const void*wh_;} p1_,p2_;};
struct chclass {unsigned opc_;struct eps*next_;struct evoi pos1,pos2;
 bit_field(c,OPB);};
					  /* length array, used by skiplen() */
static /*const*/char skplen[]=		   /* it SHOULD have been const, but */
 {SZ(eps),SZ(jump),SZ(chclass),0,sizeof(union seps),0};
				       /* some !@#$%^&*() compilers disagree */
static void puteps(spot,to)struct eps*const spot;const struct eps*const to;
{ spot->opc=OPC_EPS;spot->next=Ceps to;spot->spawn=0;  /* epsilon transition */
}

#define Cc(p,memb)	(((struct chclass*)(p))->memb)
#define rAc		Cc(r,c)

static void bseti(i,j)unsigned i;const int j;
{ bit_set(rAc,i,j);			   /* mark 'i' as being in the class */
  if(case_ignore)				  /* mark the other case too */
   { if(i-'A'<='Z'-'A')						/* uppercase */
	i+='a'-'A';
     else if(i-'a'<='z'-'a')					/* lowercase */
	i-='a'-'A';
     else
	return;							  /* no case */
     bit_set(rAc,i,j);
   }
}
					   /* general purpose length routine */
static struct eps*skiplen(ep)const struct eps*const ep;
{ return epso(ep,(ep->opc&DONE_MASK)<OPC_EPS?
   SZ(mchar):skplen[(ep->opc&DONE_MASK)-OPC_EPS]);
}

static int por P((const struct eps*const e));

static void psimp(e)const struct eps*const e;
{ switch(*p)
   { case R_BEG_GROUP:p++;			  /* not so simple after all */
	if(por(e))
	   errorno=1;
	return;
     case R_BEG_CLASS:					   /* a simple class */
      { unsigned i,j=R_NOT_CLASS==*++p;
	if(e)
	 { r->opc=OPC_CLASS;r->next=Ceps e;Cc(r,pos1.st_)=Cc(r,pos2.st_)=0;
	   i=maxindex(rAc);
	   do rAc[i]=j?~0:0;			     /* preset the bit field */
	   while(i--);
	 }
	if(j)					  /* skip the 'not' modifier */
	 { p++;
	   if(e)
	      bit_toggle(rAc,'\n');
	 }
	if(*p==R_END_CLASS)	  /* right at the start, cannot mean the end */
	 { p++;
	   if(e)
	      i=R_END_CLASS,bit_toggle(rAc,R_END_CLASS);
	 }
	else if(*p==R_RANGE)				/* take it literally */
	 { p++;
	   if(e)
	      i=R_RANGE,bit_toggle(rAc,R_RANGE);
	 }
	for(;;p++)
	 { switch(*p)
	    { case R_END_CLASS:p++;
	      case '\0':r=epso(r,SZ(chclass));
		 return;
	      case R_RANGE:
		 switch(*++p)
		  { default:
		       if(e)
			  while(++i<*p)		    /* mark all in the range */
			     bseti(i,!j);
		       break;
		    case '\0':case R_END_CLASS:p--;		/* literally */
		  }
	    }
	   if(e)
	      bseti(i= *p,!j);		      /* a normal character, mark it */
	 }
      }
     case '\0':
	return;
     case R_DOT:			 /* matches everything but a newline */
	if(e)
	 { r->opc=OPC_DOT;
	   goto fine;
	 }
	goto fine2;
     case R_SOL:			      /* match a newline (in effect) */
	if(p[1]==R_SOL)
	 { p++;
	   if(e)
	    {  r->opc=e==opcfin?OPC_EOTEXT:OPC_BOTEXT;
	       goto fine;
	    }
	 }
	else
     case R_EOL:
	   if(e)
	    { r->opc='\n';
	      goto fine;
	    }
	goto fine2;
     case R_ESCAPE:					  /* quote something */
	switch(*++p)
	 { case R_SPLIT_EXPR:
	      if(e)
		 r->opc=OPC_BOM;
	      r=epso(r,sizeof(union seps));
	      goto fine3;
	   case R_BEG_WORD:case R_END_WORD:
	    { uchar*pold=p;
	      p=(uchar*)NO_WORD_CLASS;psimp(e);p=pold+1;
	      if(e)
		 bit_toggle(Cc(epso(r,-(int)SZ(chclass)),c),'\n');
	      return;
	    }
	   case '\0':p--;				 /* nothing to quote */
	 }
   }
  if(e)						      /* a regular character */
   { r->opc=case_ignore&&(unsigned)*p-'A'<='Z'-'A'?*p+'a'-'A':*p;
fine:
     r->next=Ceps e;Cc(r,pos1.st_)=Cc(r,pos2.st_)=0;
   }
fine2:
  r=epso(r,SZ(mchar));
fine3:
  p++;
}

#define EOS(x)	(jj?Ceps e:(x))

static int endgroup(p)register const uchar*const p;
{ switch(*p)
   { case R_OR:case R_END_GROUP:case '\0':
	return 1;
   }
  return 0;
}

static void pnorm(e)const struct eps*const e;
{ void*pold;struct eps*rold;
  for(;;)
   { pold=p;rold=r;psimp(Ceps 0);ii= *p;		    /* skip it first */
     if(endgroup(p))
      { if(e)
	   p=pold,r=rold,psimp(e);
	return;
      }
     jj=endgroup(p+1);
     if(e)
	p=pold,pold=r;
     switch(ii)			   /* check for any of the postfix operators */
      { case R_0_OR_MORE:r++;
	   if(e)			  /* first an epsilon, then the rest */
	      puteps(rold,EOS(r)),r=rold+1,psimp(rold);
	   goto incagoon;
	case R_1_OR_MORE:				   /* first the rest */
	   if(e)				      /* and then an epsilon */
	    { puteps(r,rold);
	      if(jj)
		 (r+1)->opc=OPC_JUMP,(r+1)->next=Ceps e;
	      r=rold;psimp(Ceps pold);
	    }
	   r++;
	   if(endgroup(p+1))
	      r=epso(r,SZ(jump));
	   goto incagoon;
	case R_0_OR_1:r++;
	   if(e)			  /* first an epsilon, then the rest */
	      puteps(rold,r=EOS(r)),pold=r,r=rold+1,psimp(Ceps pold);
incagoon:  if(endgroup(++p))		/* at the end of this group already? */
	      return;
	   continue;				 /* regular end of the group */
      }
     if(e)			/* no fancy postfix operators, plain vanilla */
	r=rold,psimp(Ceps pold);
   }
}

static int por(e)const struct eps*const e;
{ uchar*pvold;struct eps*rvold;
  if(!e)
   { rvold=r;
     if(cachea==(pvold=p))
      { p=cachep;r=epso(rvold,cacher);
	goto ret0;
      }
   }
  for(;;)
   { uchar*pold;struct eps*rold;
     for(pold=p,rold=r;;)
      { switch(*p)
	 { default:
	      pnorm(Ceps 0);r=rold;		      /* still in this group */
	      continue;
	   case '\0':case R_END_GROUP:	       /* found the end of the group */
	      if(p==pold)				 /* empty 'or' group */
	       { if(e)
		    r->opc=OPC_JUMP,r->next=Ceps e;
		 r=epso(r,SZ(jump));
	       }
	      else
		 p=pold,pnorm(e);			/* normal last group */
	      if(!e)
	       { if(*p)
		    p++;
		 cachea=pvold;cachep=p;cacher=(char*)r-(char*)rvold;
		 goto ret0;
	       }
	      if(*p)
	       { p++;
ret0:		 return 0;
	       }
	      return 1;
	   case R_OR:r++;
	      if(p==pold)				 /* empty 'or' group */
	       { if(e)
		    puteps(rold,e);			  /* special epsilon */
	       }
	      else
	       { p=pold;pnorm(e);	      /* normal 'or' group, first an */
		 if(e)				   /* epsilon, then the rest */
		    puteps(rold,r);
	       }
	      p++;
	 }
	break;
      }
   }
}
		  /* go down recursively, mark loopbacks on the way up again */
static struct eps*maxback(down)struct eps*down;
{ ii=0;				   /* didn't find a loop at this level (yet) */
  for(;;)
   { switch(down->opc&LOOP_MASK)			/* chase JUMP chains */
      { default:
	   goto ret0;				 /* oops, not an EPS, return */
	case OPC_JUMP:down->opc=OPC_JUMP|DONE_NODE;	/* mark them as used */
	case OPC_JUMP|DONE_NODE:down=down->next;
	   continue;
	case OPC_EPS|DONE_NODE:ii=1;   /* used EPS found, return loop number */
	   return down->spawn==Ceps&aleps?down:down->spawn;
	case OPC_EPS:;			/* unused EPS found, the work starts */
      }
     break;
   }
  if(!down->spawn)	 /* has it been visited (belongs to previous group?) */
   { struct eps*left;					/* no, so process it */
     down->opc=OPC_EPS|DONE_NODE;down->spawn=Ceps&aleps;     /* mark as used */
     left=maxback(down->next);		   /* init loop no. and recurse left */
     if(ii)				    /* loop found directly below us? */
	down->opc|=LOOPL_NODE;				 /* mark a left-loop */
     ;{ struct eps*right;		 /* recurse right, take the smallest */
	if((right=maxback(down+1))&&(char*)left>(char*)right)	 /* loop no. */
	   left=right;
      }
     if(ii)				       /* loop found directly below? */
      { down->opc|=LOOPR_NODE;				/* mark a right-loop */
	if(!(down->opc&LOOPL_NODE))    /* if we didn't also have a left-loop */
	   ii=0;		/* we tell our predecessor we are not a loop */
      }
     if(!left)					    /* found no loop at all? */
      { down->spawn=down;	     /* then give ourselves our own loop no. */
	goto ret0;
      }
     if((down->spawn=left)!=down)     /* save the loop no., check if it's us */
	return left;			       /* if not, pass the number up */
   }				     /* otherwise we are the end of the loop */
ret0:
  return 0;					       /* no loop whatsoever */
}

struct eps*bregcomp(a,ign_case)const char*const a;const unsigned ign_case;
{ struct eps*st;size_t i;
  skplen[OPC_FILL-OPC_EPS]=SZ(eps)-ioffsetof(struct eps,sp);  /* a constant! */
  errorno=0;p=(uchar*)a;case_ignore=ign_case;r=Ceps&aleps;cachea=0;
  por(Ceps 0);st=r=malloc((i=(char*)r-(char*)&aleps)+sizeof r->opc);
  p=(uchar*)a;		       /* first a trial run, determine memory needed */
  if(!por(opcfin=epso(st,i)))				   /* really compile */
     errorno=1;
  r->opc=OPC_FIN;			     /* by now r should be == opcfin */
  if(errorno)
     nlog("Invalid regexp"),logqnl(a);
  for(r=st;;st=skiplen(st))		 /* simplify the compiled code (i.e. */
     switch(st->opc)		      /* take out cyclic epsilon references) */
      { case OPC_FIN:
	   return r;						 /* finished */
	case OPC_EPS:		     /* check for any closed epsilon circles */
	   if(!st->spawn)			   /* they can't be executed */
	    { maxback(st);     /* if not visited yet, recurse and mark loops */
	      ;{ register struct eps*i;
		 for(i=r;;i=skiplen(i))		 /* search the whole program */
		  { switch(i->opc&LOOP_MASK)
		     { default:				/* renumber regulars */
			{ register struct eps*f;		/* if needed */
			  if(((f=i->next)->opc&DONE_MASK)==OPC_EPS&&f->spawn)
			   { for(;f->spawn!=f;f=f->spawn);   /* search start */
			     i->next=f;				  /* of loop */
			   }
			}	       /* spare the used nodes in this group */
		       case OPC_EPS|DONE_NODE:case OPC_JUMP|DONE_NODE:
		       case OPC_FILL:case OPC_BOM:
			  continue;
		       case OPC_FIN:;
		     }
		    break;
		  }
	       }
	      ;{ register struct eps*i;
		 for(i=r;;i=skiplen(i))		 /* search the whole program */
		  { switch(i->opc)	  /* unmark/transform the used nodes */
		     { case OPC_EPS|DONE_NODE|LOOPL_NODE:i->next=i+1;
		       case OPC_EPS|DONE_NODE|LOOPR_NODE:i->sp.sopc=OPC_FILL;
		       case OPC_JUMP|DONE_NODE:i->opc=OPC_JUMP;
			  continue;
		       case OPC_EPS|DONE_NODE|LOOPL_NODE|LOOPR_NODE:
		       case OPC_EPS|DONE_NODE:i->opc=OPC_EPS;
		       default:
			  continue;
		       case OPC_FIN:;
		     }
		    break;
		  }
	       }
	    }
      }
}

#define XOR1		\
 (ioffsetof(struct chclass,pos1)^ioffsetof(struct chclass,pos2))
#define PC(thiss,t)	(((struct evoi*)geno(thiss,t))->st_)
#define PCp(thiss,t)	(((struct evoi*)geno(thiss,t))->wh_)
#define PcP(reg)	(*(const void**)\
 geno(reg,ioffsetof(struct evoi,wh_)-ioffsetof(struct evoi,st_)))

static struct mchar tswitch={OPC_TSWITCH,Ceps&tswitch};

static struct eps*cleantail(start,thiss,th1)const char*const start;
 register struct eps*thiss;const unsigned th1;
{ register struct eps**reg,*save=Ceps&tswitch,*oldthis;
  while(thiss= *(reg= &PC(oldthis=thiss,th1)))	   /* wipe out list till you */
     if(start<(char*)PcP(reg))
	*reg=0;						    /* reach tswitch */
     else
	*reg=save,save=oldthis;
  return save;
}

char*bregexec(code,text,str,len,ign_case)struct eps*code;
 const uchar*const text;const uchar*str;size_t len;unsigned ign_case;
{ register struct eps*reg,*stack,*other,*thiss;unsigned i,th1,ot1;
  struct eps*initcode;const char*eom,*pend;
  static struct eps sempty={OPC_SEMPTY,&sempty};
  static const struct jump nop={OPC_FILL};
  sempty.spawn= &sempty;			      /* static initialisers */
  ign_case=ign_case?~(unsigned)0:0;eom=0;stack= &sempty;initcode=code;
  th1=ioffsetof(struct chclass,pos1);ot1=ioffsetof(struct chclass,pos2);
  other=Ceps&tswitch;pend=(const char*)str+len+1;	     /* two past end */
  if(str--==text||*str=='\n')
     goto begofline;	      /* make sure any beginning-of-line-hooks catch */
  if(!len)
   { str++;
begofline:
     i='\n';len++;
     if(initcode->opc!=OPC_BOTEXT)
	goto setups;
     reg=initcode;initcode=Ceps&nop;thiss=Ceps&tswitch;
     goto dobotext;
   }
  do
   { i= *++str;				 /* get the next real-text character */
     if(i-'A'<='Z'-'A')
	i+=ign_case&'a'-'A';		     /* transmogrify it to lowercase */
setups:					     /* switch this & other pc-stack */
     th1^=XOR1;ot1^=XOR1;thiss=other;other=Ceps&tswitch;reg=initcode; /* pop */
     for(;;thiss=PC(reg=thiss,th1),PC(reg,th1)=0,reg=reg->next)	 /* pc-stack */
      { for(;;reg=stack->next,stack=stack->spawn)     /* pop from work-stack */
	   for(;;)
	    { switch(reg->opc-OPB)
	       { default:
		    if(i==reg->opc)		  /* regular character match */
		       goto yep;
		    break;	    /* push spawned branch on the work-stack */
		 case OPC_EPS-OPB:reg->spawn=stack;reg=(stack=reg)+1;
		    continue;
		 case OPC_JUMP-OPB:reg=reg->next;
		    continue;
		 case OPC_BOM-OPB:
		    goto foundbom;
		 case OPC_FILL-OPB:		/* nop, nothing points at it */
		    if(thiss==Ceps&tswitch)
		       goto nomatch;	     /* so the stack is always empty */
		 case OPC_SEMPTY-OPB:
		    goto empty_stack;
		 case OPC_TSWITCH-OPB:
		    goto pcstack_switch;
		 case OPC_EOTEXT-OPB:
		    if(ign_case==2)		     /* only at the very end */
		 case OPC_FIN-OPB:
		       goto nobom;
		 case OPC_BOTEXT-OPB:
dobotext:	    if(str<text)	       /* only at the very beginning */
		       goto yep;
		    break;
		 case OPC_CLASS-OPB:
		    if(bit_test(((struct chclass*)reg)->c,i))
		       goto yep;		       /* character in class */
		    break;
		 case OPC_DOT-OPB:			     /* dot-wildcard */
		    if(i!='\n')
yep:		       if(!PC(reg,ot1))		     /* state not yet pushed */
			  PC(reg,ot1)=other,PCp(other=reg,ot1)=pend;
	       }
	      break;
	    }
empty_stack:;					  /* the work-stack is empty */
      }
pcstack_switch:;				   /* this pc-stack is empty */
   }
  while(--len);					     /* still text to search */
  goto wrapup;
  ;{ const char*start,*bom;
     do
      { i= *++str;			 /* get the next real-text character */
	if(i-'A'<='Z'-'A')
	   i+=ign_case&'a'-'A';		     /* transmogrify it to lowercase */
	th1^=XOR1;ot1^=XOR1;start=pend;thiss=other;other=Ceps&tswitch;
	reg=initcode;
	for(;;							 /* pc-stack */
	 thiss=PC(reg=thiss,th1),PC(reg,th1)=0,start=PCp(reg,th1),
	 reg=reg->next)
	 { for(;;reg=stack->next,stack=stack->spawn)  /* pop from work-stack */
	      for(;;)
	       { switch(reg->opc-OPB)
		  { default:
		       if(i==reg->opc)		  /* regular character match */
			  goto Yep;
		       break;	    /* push spawned branch on the work-stack */
		    case OPC_EPS-OPB:reg->spawn=stack;reg=(stack=reg)+1;
		       continue;
		    case OPC_JUMP-OPB:reg=reg->next;
		       continue;
		    case OPC_BOM-OPB:
		       if(!eom)
foundbom:		  start=(const char*)str;
		       reg=epso(reg,sizeof(union seps));
		       continue;
		    case OPC_FILL-OPB:		/* nop, nothing points at it */
		       if(thiss==Ceps&tswitch)
			  goto checkmatch;   /* so the stack is always empty */
		    case OPC_SEMPTY-OPB:
		       goto Empty_stack;
		    case OPC_TSWITCH-OPB:
		       goto Pcstack_switch;
		    case OPC_EOTEXT-OPB:
		       if(ign_case==2)		     /* only at the very end */
		    case OPC_FIN-OPB:
			{ if(start<pend)		       /* any match? */
			   { thiss=cleantail(bom=start,thiss,th1);
			     other=cleantail(start,other,ot1);
			     eom=(const char*)str;initcode=Ceps&nop;
			     break;
			   }			      /* reset the automaton */
nobom:			  cleantail(--pend,thiss,th1);
			  cleantail(pend,other,ot1);
			  return (char*)str;	       /* one past the match */
			}
		    case OPC_BOTEXT-OPB:
		       if(str<text)	       /* only at the very beginning */
			  goto Yep;
		       break;
		    case OPC_CLASS-OPB:
		       if(bit_test(((struct chclass*)reg)->c,i))
			  goto Yep;		       /* character in class */
		       break;
		    case OPC_DOT-OPB:			     /* dot-wildcard */
		       if(i!='\n')
Yep:			  if(!PC(reg,ot1))	     /* state not yet pushed */
			   { PC(reg,ot1)=other;other=reg;   /* push location */
earlier:		     PCp(reg,ot1)=start;      /* onto other pc-stack */
			   }
			  else if(start<(char*)PCp(reg,ot1))
			     goto earlier;
		  }
		 break;
	       }
Empty_stack:;					  /* the work-stack is empty */
	 }
Pcstack_switch:;				   /* this pc-stack is empty */
      }
     while(--len);				     /* still text to search */
wrapup:
     switch(ign_case)
      { case 0:case ~(unsigned)0:ign_case=1;i='\n';	   /* just finished? */
	case 2:ign_case++;str++;len=1;th1^=XOR1;ot1^=XOR1;start=pend;
	   thiss=other;other=Ceps&tswitch;
	   goto Empty_stack;			 /* check if we just matched */
      }
checkmatch:
     if(eom)
      { static const char match[]=MATCHVAR,amatch[]=AMATCHVAR;char*q;
	if(bom<(char*)text)
	   bom=(const char*)text;
	if(eom>--pend)
	   eom=pend;
	len=eom>bom?eom-bom:0;
	if(getenv(match)==(const char*)text)	     /* anal retentive match */
	   tmemmove(q=(char*)text,bom,len),q[len]='\0',bom=q;
	else
	 { char*p;
	   primeStdout(amatch);p=realloc(Stdout,(Stdfilled+=len)+1);
	   tmemmove(q=p+Stdfilled-(int)len,bom,len);retbStdout(p);
	 }
	yell("Matched",q);
      }
   }
nomatch:
  return (char*)eom;						   /* match? */
}
