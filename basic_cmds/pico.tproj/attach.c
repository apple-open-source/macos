#if	!defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: attach.c,v 1.3 2003/01/12 01:48:04 bbraun Exp $";
#endif
/*
 * Program:	Routines to support attachments in the Pine composer 
 *
 *
 * Michael Seibel
 * Networks and Distributed Computing
 * Computing and Communications
 * University of Washington
 * Administration Builiding, AG-44
 * Seattle, Washington, 98195, USA
 * Internet: mikes@cac.washington.edu
 *
 * Please address all bugs and comments to "pine-bugs@cac.washington.edu"
 *
 * Copyright 1991-1994  University of Washington
 *
 *  Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee to the University of
 * Washington is hereby granted, provided that the above copyright notice
 * appears in all copies and that both the above copyright notice and this
 * permission notice appear in supporting documentation, and that the name
 * of the University of Washington not be used in advertising or publicity
 * pertaining to distribution of the software without specific, written
 * prior permission.  This software is made available "as is", and
 * THE UNIVERSITY OF WASHINGTON DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED,
 * WITH REGARD TO THIS SOFTWARE, INCLUDING WITHOUT LIMITATION ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, AND IN
 * NO EVENT SHALL THE UNIVERSITY OF WASHINGTON BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, TORT
 * (INCLUDING NEGLIGENCE) OR STRICT LIABILITY, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Pine and Pico are trademarks of the University of Washington.
 * No commercial use of these trademarks may be made without prior
 * written permission of the University of Washington.
 *
 *
 * NOTES:
 *
 *
 */
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include "osdep.h"
#include "pico.h"
#include "estruct.h"
#include "edef.h"
#include "efunc.h"

#ifdef	ATTACHMENTS


#ifdef	ANSI
    int    ParseAttach(struct hdr_line **,int *,char *,char *,char *,int *);
    PATMT *NewAttach(char *, long, char *);
    void   ZotAttach(struct pico_atmt *);
    int    sinserts(char *, int, char *, int);
#else
    int    ParseAttach();
    PATMT *NewAttach();
    void   ZotAttach();
    int    sinserts();
#endif


/* 
 * max number of attachments
 */
#define	MAXATCH	64


/*
 * AskAttach - ask for attachment fields and build resulting structure
 *             return pointer to that struct if OK, NULL otherwise
 */
AskAttach(fn, sz, cmnt)
char *fn, *sz, *cmnt;
{
    int	    i, status;
    long    l = 0;
    char    bfn[NFILEN];

    i = 2;
    fn[0] = '\0';
    sz[0] = '\0';
    cmnt[0] = '\0';

    while(i){
	if(i == 2){
	    KEYMENU menu_attach[2];

	    menu_attach[0].name  = "^T";
	    menu_attach[0].label = "To Files";
	    menu_attach[1].name = NULL;
	    status = mlreply("File to attach: ", fn, NLINE, QNORML,
			     menu_attach);
			     
	}
	else
	  status = mlreply("Attachment comment: ", cmnt, NLINE, QNORML, NULL);

	switch(status){
	  case HELPCH:
	    if(i == 2)
	      emlwrite("No Attachment file help yet!", NULL);
	    else
	      emlwrite("No Attachment comment help yet!", NULL);

/* remove break and sleep when help text done to force redraw */     
	    sleep(3);
	    break;

	  case (CTRL|'T'):
	    if(i != 2){
		(*term.t_beep)();
		break;
	    }

	    *bfn = '\0';
	    if(*fn == '\0' || !isdir(fn, NULL))
	      strcpy(fn, gethomedir(NULL));

	    if(FileBrowse(fn, bfn, sz, FB_READ) == 1){
		strcat(fn, S_FILESEP);
		strcat(fn, bfn);
		i--;
	    }
	    else
	      *fn = '\0';

	    refresh(FALSE, 1);
	    update();
	    break;

	  case (CTRL|'L'):
	    refresh(FALSE, 1);
	    update();
	    continue;

	  case ABORT:
	    emlwrite("Attach cancelled", NULL);
	    return(0);

	  case TRUE:				/* some comment */
	  case FALSE:				/* No comment */
	    if(i-- == 2){
		fixpath(fn, NLINE);		/* names relative to ~ */
		if((gmode&MDSCUR) && homeless(fn)){
		    emlwrite("\007Restricted mode allows attachments from home directory only", NULL);
		    return(0);
		}

		if((status=fexist(fn, "r", &l)) != FIOSUC){ /* does file exist? */
		    fioperr(status, fn);
		    return(0);
		}
		strcpy(sz, prettysz(l));
	    }
	    else{
		mlerase();
		return(1);			/* mission accomplished! */
	    }

	    break;
	  default:
	    break;
	}
    }
}

extern struct headerentry *headents;

/*
 * SyncAttach - given a pointer to a linked list of attachment structures,
 *              return with that structure sync'd with what's displayed.
 *              delete any attachments in list of structs that's not on 
 *              the display, and add any that aren't in list but on display.
 */
SyncAttach()
{
    int offset = 0,				/* the offset to begin       */
        rv = 0,
        ki = 0,					/* number of known attmnts   */
        bi = 0,					/* build array index         */
        na,					/* old number of attachmnt   */
        status, i, j, n;
    char file[NLINE],				/* buffers to hold it all    */
         size[32],
         comment[1024];
    struct hdr_line *lp;			/* current line in header    */
    struct headerentry *entry;
    PATMT *tp, *knwn[MAXATCH], *bld[MAXATCH];

    for(entry = headents; entry->name != NULL; entry++) {
      if(entry->is_attach)
	break;
    }

    if(Pmaster == NULL)
      return(-1);

    for(i=0;i<MAXATCH;i++)			/* bug - ever pop this? */
	knwn[i] = bld[i] = NULL;		/* zero out table */
    
    tp = Pmaster->attachments;
    while(tp != NULL){				/* fill table of     */
	knwn[ki++] = tp;			/* known attachments */
	tp = tp->next;
    }

    n = 0;

    lp = entry->hd_text;
    while(lp != NULL){
	na = ++n;

	if(status = ParseAttach(&lp, &offset, file, size, comment, &na))
	    rv = (rv < 0) ? rv : status ;	/* remember worst case */

	if(*file == '\0'){
	    if(n != na && na > 0 && na <= ki && (knwn[na-1]->flags&A_FLIT)){
		bld[bi++] = knwn[na-1];
		knwn[na-1] = NULL;
	    }
	    continue;
	}

	if((gmode&MDSCUR) && homeless(file))
	  /* no attachments outsize ~ in secure mode! */
	  continue;

	tp = NULL;
	for(i=0;i < ki; i++){			/* already know about it? */
	    /*
	     * this is kind of gruesome. what we want to do is keep track
	     * of literal attachment entries because they may not be 
	     * actual files we can access or that the user readily 
	     * access
	     */
	    if(knwn[i] && 
	       ((!(knwn[i]->flags&A_FLIT) && !strcmp(file, knwn[i]->filename))
	       || ((knwn[i]->flags&A_FLIT) && i+1 == na))){
		tp = knwn[i];
		knwn[i] = NULL;			/* forget we know about it */

		if(status == -1)		/* ignore garbage! */
		  break;

		if((tp->flags&A_FLIT) && strcmp(file, tp->filename)){
		    rv = 1;
		    if((j=strlen(file)) > strlen(tp->filename)){
			if((tp->filename = (char *)realloc(tp->filename,
					        sizeof(char)*(j+1))) == NULL){
			    emlwrite("\007Can't realloc filename space",NULL);
			    return(-1);
			}
		    }

		    strcpy(tp->filename, file);
		}
		else if(tp->size && strcmp(tp->size, size)){
		    rv = 1;
		    if((j=strlen(size)) > strlen(tp->size)){
			if((tp->size=(char *)realloc(tp->size,
					        sizeof(char)*(j+1))) == NULL){
			    emlwrite("\007Can't realloc space for size", NULL);
			    return(-1);
			}
		    }

		    strcpy(tp->size, size);
		}

		if(strcmp(tp->description, comment)){	/* new comment */
		    rv = 1;
		    if((j=strlen(comment)) > strlen(tp->description)){
			if((tp->description=(char *)realloc(tp->description,
						sizeof(char)*(j+1))) == NULL){
			    emlwrite("\007Can't realloc description", NULL);
			    return(-1);
			}
		    }
		      
		    strcpy(tp->description, comment);
		}
		break;
	    }
	}

	if(tp){
	    bld[bi++] = tp;
	}
	else{
	    if(file[0] != '['){
		if((tp = NewAttach(file, atol(size), comment)) == NULL)
		  return(-1);
		bld[bi++] = tp;
	    }
	    else break;
	}

	if(status < 0)
	  tp->flags |= A_ERR;		/* turn ON error bit */
	else
	  tp->flags &= ~(A_ERR);	/* turn OFF error bit */
    }

    for(i=0; i < bi; i++)		/* link together newly built list */
	bld[i]->next = bld[i+1];

    Pmaster->attachments = bld[0];

    for(i=0; i < ki; i++){		/* kill old/unused references */

	if(knwn[i]){
	    ZotAttach(knwn[i]);
	    free((char *) knwn[i]);
	}
    }

    return(rv);
}




/*
 * ParseAttach - given a header line and an offset into it, return with 
 *		 the three given fields filled in.  Assumes the size of 
 *		 the buffers passed is large enough to hold what's there.
 *		 Always updates header fields that have changed or are 
 *		 fixed.  An error advances offset to next attachment.
 *
 *		returns: 1 if a field changed
 *                       0 nothing changed
 *                      -1 on error
 */
ParseAttach(lp, off, fn, sz, cmnt, no)
struct hdr_line **lp;				/* current header line      */
int  *off;					/* offset into that line    */
char *fn, *sz, *cmnt;				/* places to return fields  */
int  *no;					/* attachment number        */
{
    int  j, status,				/* various return codes     */
         rv = 0,				/* return value             */
         lbln  = 0;				/* label'd attachment	    */
    long l;					/* attachment length        */
    char tmp[1024],
	 c,
        *p,
        *lblsz = NULL,				/* label'd attchmnt's size  */
         number[8];
    register struct hdr_line  *lprev;
    enum {					/* parse levels             */
	LWS,					/* leading white space      */
	NUMB,					/* attachment number        */
	WSN,					/* white space after number */
	TAG,					/* attachments tag (fname)  */
	WST,					/* white space after tag    */
	SIZE,					/* attachments size         */
	SWS,					/* white space after size   */
	COMMENT,				/* attachment comment       */
	TG} level;				/* trailing garbage         */

    *fn = *sz = *cmnt = '\0';			/* initialize return strings */
    p   = tmp;

    level = LWS;				/* start at beginning */
    while(*lp != NULL){

	if((c=(*lp)->text[*off]) == '\0'){	/* end of display line */
	    lprev = *lp;
	    if((*lp = (*lp)->next) != NULL)
	      c = (*lp)->text[*off = 0];	/* reset offset */
	}

	switch(level){
	  case LWS:				/* skip leading white space */
	    if(isspace(c) || c == '\0'){
		break;
	    }
	    else if(c < '0' || c > '9'){	/* add a number */
		sprintf(number, "%d. ", *no);
		*no = 0;			/* no previous number! */
		sinserts((*lp == NULL) ? &lprev->text[*off]
			               : &(*lp)->text[*off],
			     0, number, j=strlen(number));
		*off += j - 1;
		rv = 1;
		level = TAG;			/* interpret the name */
		break;
	    }
	    level = NUMB;
	  case NUMB:				/* attachment number */
	    if(c == '.'){			/* finished grabbing size */
		/*
		 * replace number if it's not right
		 */
		*p = '\0';
		sprintf(number, "%d", *no);	/* record the current...  */
		*no = atoi(tmp);		/* and the old place in list */
		if(strcmp(number, tmp)){
		    if(p-tmp > *off){		/* where to begin replacemnt */
			j = (p-tmp) - *off;
			sinserts((*lp)->text, *off, "", 0);
			sinserts(&lprev->text[strlen(lprev->text)-j], j, 
				 number, strlen(number));
			*off = 0;
		    }
		    else{
			j = (*off) - (p-tmp);
			sinserts((*lp == NULL) ? &lprev->text[j] 
				               : &(*lp)->text[j], 
				 p-tmp , number, strlen(number));
			*off += strlen(number) - (p-tmp);
		    }
		    rv = 1;
		}

		p = tmp;
		level = WSN;			/* what's next... */
	    }
	    else if(c < '0' || c > '9'){	/* Must be part of tag */
		sprintf(number, "%d. ", *no);
		sinserts((*lp == NULL) ? &lprev->text[(*off) - (p - tmp)]
			               : &(*lp)->text[(*off) - (p - tmp)],
			     0, number, j=strlen(number));
		*off += j;
		*p++ = c;
		level = TAG;			/* interpret the name */
	    }
	    else
	      *p++ = c;

	    break;

	  case WSN:				/* blast whitespace */
	    if(isspace(c) || c == '\0'){
		break;
	    }
	    else if(c == '['){			/* labeled attachment */
		lbln++;
	    }
	    else if(c == ',' || c == ' '){
		emlwrite("\007Attchmnt: '%c' not allowed in file name", 
			  (void *)(int)c);
		rv = -1;
		level = TG;			/* eat rest of garbage */
		break;
	    }
	    level = TAG;
	  case TAG:				/* get and check filename */
						/* or labeled attachment  */
						/* enclosed in []         */
	    if(c == '\0' || (!lbln && (isspace(c) || strchr(",(\"", c)))
	       || (lbln && c == ']')){
		if(p != tmp){
		    *p = '\0';			/* got something */

		    strcpy(fn, tmp);		/* store file name */
		    if(!lbln){			/* normal file attachment */
			fixpath(fn, NLINE);
			if((status=fexist(fn, "r", &l)) != FIOSUC){
			    fioperr(status, fn);
			    rv = -1;
			    level = TG;		/* munch rest of garbage */
			    break;
			}


			if((gmode&MDSCUR) && homeless(fn)){
			    emlwrite("\007Restricted mode allows attachments from home directory only", NULL);
			    rv = -1;
			    level = TG;
			    break;
			}

			if(strcmp(fn, tmp)){ 	/* fn changed: display it */
			    if(*off >=  p - tmp){	/* room for it? */
				sinserts((*lp == NULL)? 
					 &lprev->text[(*off)-(p-tmp)] :
					 &(*lp)->text[(*off)-(p-tmp)],
					 p-tmp, fn, j=strlen(fn));
				*off += j - (p - tmp);	/* advance offset */
				rv = 1;
			    }
			    else{
				emlwrite("\007Attchmnt: Problem displaying real file path", NULL);
			    }
			}
		    }
		    else{			/* labelled attachment! */
			/*
			 * should explain about labelled attachments:
			 * these are attachments that came into the composer
			 * with meaningless file names (up to caller of 
			 * composer to decide), for example, attachments
			 * being forwarded from another message.  here, we
			 * just make sure the size stays what was passed
			 * to us.  The user is SOL if they change the label
			 * since, as it is now, after changed, it will
			 * just get dropped from the list of what gets 
			 * passed back to the caller.
			 */
			PATMT *tp;

			if(c != ']'){		/* legit label? */
			    emlwrite("\007Attchmnt: Expected ']' after \"%s\"",
				     fn);
			    rv = -1;
			    level = TG;
			    break;
			}
			strcat(fn, "]");

			/*
			 * This is kind of cheating since otherwise
			 * ParseAttach doesn't know about the attachment
			 * struct.  OK if filename's not found as it will
			 * get taken care of later...
			 */
			tp = Pmaster->attachments; /* caller check Pmaster! */
			j = 0;
			while(tp != NULL){
			    if(++j == *no){
				lblsz = tp->size;
				break;
			    }
			    tp = tp->next;
			}

			if(tp == NULL){
			    emlwrite("\007Attchmnt: Unknown reference: %s",fn);
			    lblsz =  "XXX";
			}
		    }

		    p = tmp;			/* reset p in tmp */
		    level = WST;
		}

		if(!lbln && c == '(')		/* no space 'tween file, size*/
		  level = SIZE;
		else if(c == '\0' || (!lbln && (c == ',' || c == '\"'))){
		    strcpy(sz, (lblsz) ? lblsz : prettysz(l));
		    sprintf(tmp, " (%s) %s", sz, (c == '\"') ? "" : "\"\"");
		    sinserts((*lp == NULL) ? &lprev->text[*off] 
			                   : &(*lp)->text[*off],
			     0, tmp, j = strlen(tmp));
		    *off += j;
		    rv = 1;
		    level = (c == '\"') ? COMMENT : TG;/* cmnt or eat trash */
		}
	    }
	    else if(!lbln && (c == ',' || c == ' ' || c == '[' || c == ']')){
		emlwrite("\007Attchmnt: '%c' not allowed in file name",
			  (void *)(int)c);
		rv = -1;			/* bad char in file name */
		level = TG;			/* gobble garbage */
	    }
	    else
	      *p++ = c;				/* add char to name */

	    break;

	  case WST:				/* skip white space */
	    if(!isspace(c)){
		/*
		 * whole attachment, comment or done! 
		 */
		if(c == ',' || c == '\0' || c == '\"'){
		    strcpy(sz, (lblsz) ? lblsz : prettysz(l));
		    sprintf(tmp, " (%s) %s", sz, 
				           (c == '\"') ? "" : "\"\"");
		    sinserts((*lp == NULL) ? &lprev->text[*off]
				           : &(*lp)->text[*off],
			     0, tmp, j = strlen(tmp));
		    *off += j;
		    rv = 1;
		    level = (c == '\"') ? COMMENT : TG;
		    lbln = 0;			/* reset flag */
		}
		else if(c == '('){		/* get the size */
		    level = SIZE;
		}
		else{
		    emlwrite("\007Attchmnt: Expected '(' or '\"' after %s",fn);
		    rv = -1;			/* bag it all */
		    level = TG;
		}
	    }
	    break;

	  case SIZE:				/* check size */
	    if(c == ')'){			/* finished grabbing size */
		*p = '\0';
		/*
		 * replace sizes if they don't match!
		 */
		strcpy(sz, tmp);
		if(strcmp(sz, (lblsz) ? lblsz : prettysz(l))){
		    strcpy(sz, (lblsz) ? lblsz : prettysz(l));
		    if(p-tmp > *off){		/* where to begin replacemnt */
			j = (p-tmp) - *off;
			sinserts((*lp)->text, *off, "", 0);
			sinserts(&lprev->text[strlen(lprev->text)-j], j, 
				 sz, strlen(sz));
			*off = 0;
		    }
		    else{
			j = (*off) - (p-tmp);
			sinserts((*lp == NULL) ? &lprev->text[j] 
				               : &(*lp)->text[j], 
				 p-tmp , sz, strlen(sz));
			*off += strlen(sz) - (p-tmp);
		    }
		    rv = 1;
		}

		p = tmp;
		level = SWS;			/* what's next... */
	    }
	    else if(c == '\0' || c == ','){
		*p = '\0';
		emlwrite("\007Attchmnt: Size field missing ')': \"%s\"", tmp);
		rv = -1;
		level = TG;
	    }
	    else
	      *p++ = c;

	    break;

	  case SWS:				/* skip white space */
	    if(!isspace(c)){
		if(c == ','){			/* no description */
		    level = TG;			/* munch rest of garbage */
		    lbln = 0;			/* reset flag */
		}
		else if(c != '\"' && c != '\0'){
		    emlwrite("\007Attchmnt: Malformed comment, quotes required", NULL);
		    rv = -1;
		    level = TG;
		}
		else
		  level = COMMENT;
	    }
	    break;

	  case COMMENT:				/* slurp up comment */
	    if(c == '\"' || c == '\0'){		/* got comment */
		*p = '\0';			/* cap it off */
		p = tmp;			/* reset p */
		strcpy(cmnt,tmp);		/* copy the comment  */
		if(c == '\0'){
		    emlwrite("\007Attchmnt: Closing quote required at end of comment", NULL);
		    rv = -1;
		}
		level = TG;			/* prepare for next one */
		lbln = 0;			/* reset flag */
	    }
	    else
	      *p++ = c;

	    break;

	  case TG:				/* get comma or final EOL */
	    if(!isspace(c)){
		if(c != ',' && c != '\0'){
		    if(rv != -1)
		      emlwrite("\007Attchmnt: Comma must separate attachments", NULL);
		    rv = -1;
		}
	    }
	    break;

	  default:				/* something's very wrong */
	    emlwrite("\007Attchmnt: Weirdness in ParseAttach", NULL);
	    return(-1);				/* just give up */
	}

	if(c == '\0')				/* we're done */
	  break;

	(*off)++;

	/*
	 * not in comment or label name? done. 
	 */
	if(c == ',' && (level != COMMENT && !lbln))
	  break;				/* put offset past ',' */
    }

    return(rv);
}



/*
 * NewAttach - given a filename (assumed to accessible) and comment, creat
 */
PATMT *NewAttach(f, l, c)
char *f;
long l;
char *c;
{
    PATMT  *tp;

    if((tp=(PATMT *)malloc(sizeof(PATMT))) == NULL){
	emlwrite("No memory to add attachment", NULL);
	return(NULL);
    }
    else{
	tp->filename = tp->description = NULL;
	tp->size = tp->id = NULL;
	tp->next = NULL;
    }

    /* file and size malloc */
    if((tp->filename = (char *)malloc(strlen(f)+1)) == NULL){
	emlwrite("Can't malloc name for attachment", NULL);
	free((char *) tp);
	return(NULL);
    }
    strcpy(tp->filename, f);

    if(l > -1){
	tp->size = (char *)malloc(sizeof(char)*(strlen(prettysz(l))+1));
	if(tp->size == NULL){
	    emlwrite("Can't malloc size for attachment", NULL);
	    free((char *) tp->filename);
	    free((char *) tp);
	    return(NULL);
	}
	else
	  strcpy(tp->size, prettysz(l));
    }

    /* description malloc */
    if((tp->description = (char *)malloc(strlen(c)+1)) == NULL){
	emlwrite("Can't malloc description for attachment", NULL);
	free((char *) tp->size);
	free((char *) tp->filename);
	free((char *) tp);
	return(NULL);
    }
    strcpy(tp->description, c);

    return(tp);
}



/*
 * AttachError - Sniff list of attachments, returning TRUE if there's
 *               any sign of trouble...
 */
int
AttachError()
{
    PATMT *ap;

    if(!Pmaster)
      return(0);

    ap = Pmaster->attachments;
    while(ap){
	if((ap->flags) & A_ERR)
	  return(1);

	ap = ap->next;
    }

    return(FALSE);
}



void ZotAttach(p)
PATMT *p;
{
    if(!p)
      return;
    if(p->description)
      free((char *)p->description);
    if(p->filename)
      free((char *)p->filename);
    if(p->size)
      free((char *)p->size);
    if(p->id)
      free((char *)p->id);
    p->next = NULL;
}
#endif	/* ATTACHMENTS */


/*
 * intag - return TRUE if i is in a column that makes up an
 *         attachment line number
 */
intag(s, i)
char *s;
int   i;
{
    char *p = s;
    int n = 0;

    while(*p != '\0' && (p-s) < 5){		/* is there a tag? it */
	if(n && *p == '.')			/* can't be more than 4 */
	  return(i <= p-s);				/* chars long! */

	if(*p < '0' || *p > '9')
	  break;
	else
	  n = (n * 10) + (*p - '0');

	p++;
    }

    return(FALSE);
}


/*
 * prettysz - return pointer to string containing nice
 */
char *prettysz(l)
long l;
{
    static char b[32];

    if(l < 1000)
      sprintf(b, "%d  B", l);			/* xxx B */
    else if(l < 10000)
      sprintf(b, "%1.1f KB", (float)l/1000);    /* x.x KB */
    else if(l < 1000000)
      sprintf(b, "%d KB", l/1000);		/* xxx KB */
    else if(l < 10000000)
      sprintf(b, "%1.1f MB", (float)l/1000000); /* x.x MB */
    else
      sprintf(b, "%d MB", l/1000000);		/* xxx MB */
    return(b);
}


/*
 * sinserts - s insert into another string
 */
sinserts(ds, dl, ss, sl)
char *ds, *ss;					/* dest. and source strings */
int  dl, sl;					/* their lengths */
{
    char *dp, *edp;				/* pointers into dest. */
    int  j;					/* jump difference */

    if(sl >= dl){				/* source bigger than dest. */
	dp = ds + dl;				/* shift dest. to make room */
	if((edp = strchr(dp, '\0')) != NULL){
	    j = sl - dl;

	    for( ;edp >= dp; edp--)
	      edp[j] = *edp;

	    while(sl--)
	       *ds++ = *ss++;
	}
	else
	  emlwrite("\007No end of line???", NULL);	/* can this happen? */
    }
    else{					/* dest is longer, shrink it */
	j = dl - sl;				/* difference in lengths */

	while(sl--)				/* copy ss onto ds */
	  *ds++ = *ss++;

	if(strlen(ds) > j){			/* shuffle the rest left */
	    do
	      *ds = ds[j];
	    while(*ds++ != '\0');
	}
	else
	  *ds = '\0';
    }
}
